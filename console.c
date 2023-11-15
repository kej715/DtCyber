/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**
**  Name: console.c
**
**  Description:
**      Perform emulation of CDC 6612 or CC545 console.
**
**  This program is free software: you can redistribute it and/or modify
**  it under the terms of the GNU General Public License version 3 as
**  published by the Free Software Foundation.
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License version 3 for more details.
**
**  You should have received a copy of the GNU General Public License
**  version 3 along with this program in file "license-gpl-3.0.txt".
**  If not, see <http://www.gnu.org/licenses/gpl-3.0.txt>.
**
**--------------------------------------------------------------------------
*/

/*
**  -------------
**  Include Files
**  -------------
*/
#include <stdio.h>
#include <stdlib.h>
#include "const.h"
#include "types.h"
#include "proto.h"
#if defined(_WIN32)
#include <winsock.h>
#else
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

/*
**  -----------------
**  Private Constants
**  -----------------
*/

/*
**  CDC 6612 console functions and status codes.
*/
#define Fc6612Sel64CharLeft      07000
#define Fc6612Sel32CharLeft      07001
#define Fc6612Sel16CharLeft      07002

#define Fc6612Sel512DotsLeft     07010
#define Fc6612Sel512DotsRight    07110
#define Fc6612SelKeyIn           07020

#define Fc6612Sel64CharRight     07100
#define Fc6612Sel32CharRight     07101
#define Fc6612Sel16CharRight     07102

#define InBufSize                 1024
#define OutBufSize                8192

#define CmdSetXLow                0x80
#define CmdSetYLow                0x81
#define CmdSetXHigh               0x82
#define CmdSetYHigh               0x83
#define CmdSetScreen              0x84
#define CmdSetFontType            0x85

#define FontTypeDot                  0
#define FontTypeSmall                1
#define FontTypeMedium               2
#define FontTypeLarge                3

/*
**  -----------------------
**  Private Macro Functions
**  -----------------------
*/
#if !defined(_WIN32)
#define INVALID_SOCKET -1
#define SOCKET int
#endif

/*
**  -----------------------------------------
**  Private Typedef and Structure Definitions
**  -----------------------------------------
*/

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static void     consoleAcceptConnection(void);
static void     consoleActivate(void);
static FcStatus consoleFunc(PpWord funcCode);
static void     consoleDisconnect(void);
static void     consoleIo(void);
static void     consoleNetIo(void);
static void     consoleSetFontType(u8 fontType);
static void     consoleSetScreen(u8 screen);
static void     consoleSetX(u16 x);
static void     consoleSetY(u16 y);
static void     consoleQueue(u8 ch);

/*
**  ----------------
**  Public Variables
**  ----------------
*/

/*
**  -----------------
**  Private Variables
**  -----------------
*/
static u8     consoleChannelNo;
static u8     consoleEqNo;

static u8     currentFontType  = FontTypeSmall;
static u8     currentScreen    = 0xff;
static u8     fontSizes[4]     = { FontDot, FontSmall, FontMedium, FontLarge };
static u16    xOffsets[2]      = { OffLeftScreen, OffRightScreen };

static SOCKET connFd           = INVALID_SOCKET;
static SOCKET listenFd         = INVALID_SOCKET;

static u8     consoleInBuf[InBufSize];
static int    consoleInBufIn   = 0;
static int    consoleInBufOut  = 0;

static u8     consoleOutBuf[OutBufSize];
static int    consoleOutBufIn  = 0;

/*
 **--------------------------------------------------------------------------
 **
 **  Public Functions
 **
 **--------------------------------------------------------------------------
 */

/*--------------------------------------------------------------------------
**  Purpose:        Initialise 6612 console.
**
**  Parameters:     Name        Description.
**                  eqNo        equipment number
**                  unitNo      unit number
**                  channelNo   channel number the device is attached to
**                  params      optional device parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void consoleInit(u8 eqNo, u8 unitNo, u8 channelNo, char *params)
    {
    int     consolePort;
    DevSlot *dp;
    int     n;

    consoleChannelNo = channelNo;
    consoleEqNo      = eqNo;

    dp = channelAttach(channelNo, eqNo, DtConsole);

    dp->activate     = consoleActivate;
    dp->disconnect   = consoleDisconnect;
    dp->selectedUnit = 0;
    dp->func         = consoleFunc;
    dp->io           = consoleIo;

    if (params != NULL)
        {
        n = sscanf(params, "%d", &consolePort);
        if (n < 1)
            {
            fputs("(console) TCP port missing from CO6612 definition\n", stderr);
            exit(1);
            }
        if (consolePort < 1 || consolePort > 65535)
            {
            fprintf(stderr, "(console) Invalid TCP port number in CO6612 definition: %d\n", consolePort);
            exit(1);
            }
        listenFd = netCreateListener(consolePort);
        if (listenFd == INVALID_SOCKET)
            {
            fprintf(stderr, "(console) Failed to listen for TCP connections on port %d\n", consolePort);
            exit(1);
            }
        fprintf(stdout, "(console) Listening for connections on port %d\n", consolePort);
        }

    /*
    **  Initialise (X)Windows environment.
    */
    windowInit();

    /*
    **  Print a friendly message.
    */
    printf("(console) Initialised on channel %o\n", channelNo);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Unconditionally close a remote console connection.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void consoleCloseRemote(void)
    {
    if (connFd != INVALID_SOCKET)
        {
        netCloseConnection(connFd);
        connFd = INVALID_SOCKET;
        consoleInBufIn  = consoleInBufOut  = 0;
        consoleOutBufIn = 0;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Answer whether a remote console is active.
**
**  Parameters:     Name        Description.
**
**  Returns:        TRUE if remote console active.
**
**------------------------------------------------------------------------*/
bool consoleIsRemoteActive(void)
    {
    return connFd != INVALID_SOCKET;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Show remote console status (operator interface).
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void consoleShowStatus(void)
    {
    char      outBuf[200];

    if (listenFd != INVALID_SOCKET)
        {
        sprintf(outBuf, "    >   %-8s C%02o E%02o     ",  "6612", consoleChannelNo, consoleEqNo);
        opDisplay(outBuf);
        sprintf(outBuf, FMTNETSTATUS"\n", netGetLocalTcpAddress(listenFd), "", "console", "listening");
        opDisplay(outBuf);
        if (connFd != INVALID_SOCKET)
            {
            sprintf(outBuf, "    >   %-8s             ",  "6612");
            opDisplay(outBuf);
            sprintf(outBuf, FMTNETSTATUS"\n", netGetLocalTcpAddress(connFd), netGetPeerTcpAddress(connFd), "console", "connected");
            opDisplay(outBuf);
            }
        }
    }

/*
 **--------------------------------------------------------------------------
 **
 **  Private Functions
 **
 **--------------------------------------------------------------------------
 */

/*--------------------------------------------------------------------------
**  Purpose:        Execute function code on 6612 console.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        FcStatus
**
**------------------------------------------------------------------------*/
static FcStatus consoleFunc(PpWord funcCode)
    {
    if (listenFd != INVALID_SOCKET && connFd == INVALID_SOCKET)
        {
        consoleAcceptConnection();
        }

    activeChannel->full = FALSE;

    switch (funcCode)
        {
    default:
        return (FcDeclined);

    case Fc6612Sel512DotsLeft:
        consoleSetScreen(LeftScreen);
        consoleSetFontType(FontTypeDot);
        break;

    case Fc6612Sel512DotsRight:
        consoleSetScreen(RightScreen);
        consoleSetFontType(FontTypeDot);
        break;

    case Fc6612Sel64CharLeft:
        consoleSetScreen(LeftScreen);
        consoleSetFontType(FontTypeSmall);
        break;

    case Fc6612Sel32CharLeft:
        consoleSetScreen(LeftScreen);
        consoleSetFontType(FontTypeMedium);
        break;

    case Fc6612Sel16CharLeft:
        consoleSetScreen(LeftScreen);
        consoleSetFontType(FontTypeLarge);
        break;

    case Fc6612Sel64CharRight:
        consoleSetScreen(RightScreen);
        consoleSetFontType(FontTypeSmall);
        break;

    case Fc6612Sel32CharRight:
        consoleSetScreen(RightScreen);
        consoleSetFontType(FontTypeMedium);
        break;

    case Fc6612Sel16CharRight:
        consoleSetScreen(RightScreen);
        consoleSetFontType(FontTypeLarge);
        break;

    case Fc6612SelKeyIn:
        break;
        }

    activeDevice->fcode = funcCode;

    return (FcAccepted);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Perform I/O on 6612 console.
**
**  Parameters:     Name        Description.
**                  device      Device control block
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void consoleIo(void)
    {
    u8 ch;

    switch (activeDevice->fcode)
        {
    default:
        break;

    case Fc6612Sel64CharLeft:
    case Fc6612Sel32CharLeft:
    case Fc6612Sel16CharLeft:
    case Fc6612Sel64CharRight:
    case Fc6612Sel32CharRight:
    case Fc6612Sel16CharRight:
        if (activeChannel->full)
            {
            ch = (u8)((activeChannel->data >> 6) & Mask6);

            if (ch >= 060)
                {
                if (ch >= 070)
                    {
                    /*
                    **  Vertical coordinate.
                    */
                    consoleSetY((u16)(activeChannel->data & Mask9));
                    }
                else
                    {
                    /*
                    **  Horizontal coordinate.
                    */
                    consoleSetX((u16)(activeChannel->data & Mask9));
                    }
                }
            else
                {
                consoleQueue(consoleToAscii[(activeChannel->data >> 6) & Mask6]);
                consoleQueue(consoleToAscii[(activeChannel->data >> 0) & Mask6]);
                }

            activeChannel->full = FALSE;
            }
        break;

    case Fc6612Sel512DotsLeft:
    case Fc6612Sel512DotsRight:
        if (activeChannel->full)
            {
            ch = (u8)((activeChannel->data >> 6) & Mask6);

            if (ch >= 060)
                {
                if (ch >= 070)
                    {
                    /*
                    **  Vertical coordinate.
                    */
                    consoleSetY((u16)(activeChannel->data & Mask9));
                    consoleQueue('.');
                    }
                else
                    {
                    /*
                    **  Horizontal coordinate.
                    */
                    consoleSetX((u16)(activeChannel->data & Mask9));
                    }
                }

            activeChannel->full = FALSE;
            }
        break;

    case Fc6612SelKeyIn:
        activeChannel->data   = 0;
        activeChannel->full   = TRUE;
        activeChannel->status = 0;
        activeDevice->fcode   = 0;
        if (ppKeyIn != 0)
            {
            activeChannel->data = asciiToConsole[ppKeyIn];
            ppKeyIn             = 0;
            }
        else if (opKeyIn != 0)
            {
            activeChannel->data = asciiToConsole[opKeyIn];
            opKeyIn             = 0;
            }
        break;
        }

    if (connFd != INVALID_SOCKET) consoleNetIo();
    }

/*--------------------------------------------------------------------------
**  Purpose:        Accept rmeote console connection.
**
**  Parameters:     Name        Description.
**
**  Returns:        nothing
**
**------------------------------------------------------------------------*/
static void consoleAcceptConnection(void)
    {
    int            n;
    fd_set         readFds;
    struct timeval timeout;

    FD_ZERO(&readFds);
    FD_SET(listenFd, &readFds);
    timeout.tv_sec  = 0;
    timeout.tv_usec = 0;
    n = select(listenFd + 1, &readFds, NULL, NULL, &timeout);
    if (n > 0)
        {
        connFd = netAcceptConnection(listenFd);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Handle channel activation.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void consoleActivate(void)
    {
    }

/*--------------------------------------------------------------------------
**  Purpose:        Handle disconnecting of channel.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void consoleDisconnect(void)
    {
    }

/*--------------------------------------------------------------------------
**  Purpose:        Flush remote console output buffer.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void consoleNetFlush(void)
    {
    int n;

    if (consoleOutBufIn > 0)
        {
        n = send(connFd, consoleOutBuf, consoleOutBufIn, 0);
        if (n > 0)
            {
            if (n < consoleOutBufIn)
                {
                memcpy(consoleOutBuf, &consoleOutBuf[n], consoleOutBufIn - n);
                }
            consoleOutBufIn -= n;
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Manage remote console I/O.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void consoleNetIo(void)
    {
    int            n;
    fd_set         readFds;
    struct timeval timeout;

    FD_ZERO(&readFds);
    FD_SET(connFd, &readFds);
    timeout.tv_sec  = 0;
    timeout.tv_usec = 0;
    n = select(connFd + 1, &readFds, NULL, NULL, &timeout);

    if (ppKeyIn == 0 && consoleInBufOut < consoleInBufIn)
        {
        ppKeyIn = consoleInBuf[consoleInBufOut++];
        if (consoleInBufOut >= consoleInBufIn) consoleInBufIn = consoleInBufOut = 0;
        }
    if (n > 0 && consoleInBufIn < InBufSize && FD_ISSET(connFd, &readFds))
        {
        n = recv(connFd, &consoleInBuf[consoleInBufIn], InBufSize - consoleInBufIn, 0);
        if (n <= 0)
            {
            consoleCloseRemote();
            }
        else
            {
            consoleInBufIn += n;
            }
        }
    if (connFd != INVALID_SOCKET && consoleOutBufIn >= (OutBufSize / 2))
        {
        consoleNetFlush();
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Queue characters.
**
**  Parameters:     Name        Description.
**                  ch          character to be queued.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void consoleQueue(u8 ch)
    {
    if (connFd == INVALID_SOCKET)
        {
        windowQueue(ch);
        }
    else if (consoleOutBufIn < OutBufSize)
        {
        consoleOutBuf[consoleOutBufIn++] = ch & 0x7f;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Set font type.
**
**  Parameters:     Name        Description.
**                  fontType    one of FontTypeDot, FontTypeSmall,
**                              FontTypeMedium, or FontTypeLarge
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void consoleSetFontType(u8 fontType)
    {
    if (connFd == INVALID_SOCKET)
        {
        windowSetFont(fontSizes[fontType]);
        }
    else if (currentFontType != fontType && consoleOutBufIn < OutBufSize - 1)
        {
        consoleOutBuf[consoleOutBufIn++] = CmdSetFontType;
        consoleOutBuf[consoleOutBufIn++] = fontType;
        }
    currentFontType = fontType;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Set screen
**
**  Parameters:     Name        Description.
**                  screen      0 = left screen, 1 = right screen
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void consoleSetScreen(u8 screen)
    {
    if (currentScreen != screen && consoleOutBufIn < OutBufSize - 1)
        {
        consoleOutBuf[consoleOutBufIn++] = CmdSetScreen;
        consoleOutBuf[consoleOutBufIn++] = screen;
        }
    currentScreen = screen;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Set X coordinate.
**
**  Parameters:     Name        Description.
**                  x           horinzontal coordinate (0 - 0777)
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void consoleSetX(u16 x)
    {
    if (connFd == INVALID_SOCKET)
        {
        windowSetX(x + xOffsets[currentScreen]);
        }
    else if (consoleOutBufIn < OutBufSize - 1)
        {
        if (x > 0xff)
            {
            consoleOutBuf[consoleOutBufIn++] = CmdSetXHigh;
            consoleOutBuf[consoleOutBufIn++] = x & 0xff;
            }
        else
            {
            consoleOutBuf[consoleOutBufIn++] = CmdSetXLow;
            consoleOutBuf[consoleOutBufIn++] = x;
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Set Y coordinate.
**
**  Parameters:     Name        Description.
**                  y           horizontal coordinate (0 - 0777)
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void consoleSetY(u16 y)
    {
    if (connFd == INVALID_SOCKET)
        {
        windowSetY(y);
        }
    else if (consoleOutBufIn < OutBufSize - 1)
        {
        if (y > 0xff)
            {
            consoleOutBuf[consoleOutBufIn++] = CmdSetYHigh;
            consoleOutBuf[consoleOutBufIn++] = y & 0xff;
            }
        else
            {
            consoleOutBuf[consoleOutBufIn++] = CmdSetYLow;
            consoleOutBuf[consoleOutBufIn++] = y;
            }
        }
    }

/*---------------------------  End Of File  ------------------------------*/
