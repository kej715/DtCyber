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
**  About remote console support:
**    Remote consoles are supported using a derivative of a strategy devised
**    by Paul Koning for detecting refresh cycles. Paul observed that most
**    PP programs interacting with the console refresh the two screens,
**    then they poll the keyboard for input, then they repeat. Consequently,
**    the strategy employed here is to compute an inexpensive checksum
**    against X/Y coordinate positions starting from a keyboard input check.
**    If a match is found for a previously computed checksum, a refresh cycle
**    is detected, and console output data accumulated between the matching
**    checksums is eligible to be sent to the remote console. The frequency
**    at which eligible data is actually sent depends upon the setting of
**    a minimum refresh interval which is specified by the remote console
**    client.
**
**  Remote console streams format:
**    The data stream transmitted by DtCyber to a remote console consists
**    of intermixed control information and characters to be displayed. The
**    data stream received by DtCyber from a remote console may contain
**    intermixed control information and characters representing console
**    keystrokes. In both cases, control information is distinguished from
**    displayable characters or keystrokes by examining the sign bit of
**    each byte. When the sign bit is set, the byte represents control
**    information.
**
**    The control codes transmitted by DtCyber in the remote console output
**    stream are defined as follows:
**
**      0x80 : Set small X coordinate. One parameter byte follows. The new
**             X coordinate is exactly the value in the parameter byte.
**      0x81 : Set Y small coordinate. One parameter byte follows. The new
**             Y coordinate is exactly the value in the parameter byte.
**      0x82 : Set large X coordinate. One parameter byte follows. The new
**             X coordinate is 256 + the value in the parameter byte.
**      0x83 : Set large Y coordinate. One parameter byte follows. The new
**             Y coordinate is 256 + the value in the parameter byte.
**      0x84 : Set screen. One parameter byte follows:
**             0 = left screen, 1 = right screen
**      0x85 : Set font type. One parameter byte follows:
**             0 = dot mode, 1 = small font, 2 = medium font, 3 = large font
**      0xFF : End of frame. Data between occurrences of this control code
**             represent one console display refresh cycle.
**
**    The control codes recognized by DtCyber in the remote console input
**    stream are defined as follows:
**
**      0x80 : Set refresh interval. One parameter byte follows. The parameter byte
**             defines the refresh interval in units of 0.01 seconds. However, if the
**             value is 0, the automatic transmission of refresh cycles is disabled.
**      0x81 : Send frame immediately. Causes the next frame to be sent immediately.
**             Thereafter, frames are sent according to the current refresh interval.
**             This control code can be used to poll for frames (e.g., when refresh
**             interval set to 0) or to force frames to be sent at any time.
**
**    Note that when a remote console connection is first established, the default
**    refresh interval is 0. Consequently, no frames will be sent until a non-0
**    refresh interval is set by sending the 0x80 control code, or the 0x81 control
**    code is sent to poll for a frame explicitly.
**
**--------------------------------------------------------------------------
*/

#define DEBUG 0

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

#define CycleDataBufSize         16384
#define CycleDataLimit           (CycleDataBufSize-1)
#define InBufSize                 1024
#define OutBufSize               16384

#define CmdSetXLow                0x80
#define CmdSetYLow                0x81
#define CmdSetXHigh               0x82
#define CmdSetYHigh               0x83
#define CmdSetScreen              0x84
#define CmdSetFontType            0x85
#define CmdEndFrame               0xFF

#define FontTypeDot                  0
#define FontTypeSmall                1
#define FontTypeMedium               2
#define FontTypeLarge                3

#define InfiniteRefreshInterval      ((u64)1000*60*60*24*365)
#define MaxCycleDataEntries          5

/*
**  -----------------------
**  Private Macro Functions
**  -----------------------
*/
#if !defined(_WIN32)
#define INVALID_SOCKET -1
#define SOCKET int
#endif

#if DEBUG
#define HexColumn(x)      (3 * (x) + 4)
#define AsciiColumn(x)    (HexColumn(16) + 2 + (x))
#define LogLineLength     (AsciiColumn(16))
#endif

/*
**  -----------------------------------------
**  Private Typedef and Structure Definitions
**  -----------------------------------------
*/
typedef struct cycleData
    {
    int         sum1;           /* Fletcher checksum accumulators */
    int         sum2;
    int         first;          /* Index of first accumulated byte in display sequence */
    int         limit;          /* Index of last + 1 accumulated byte in sequence */
    } CycleData;

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static void     consoleAcceptConnection(void);
static void     consoleActivate(void);
static void     consoleCheckDisplayCycle(void);
static FcStatus consoleFunc(PpWord funcCode);
static void     consoleDisconnect(void);
static void     consoleFlushCycleData(int first, int limit);
static void     consoleInitCycleData(void);
static void     consoleIo(void);
static void     consoleNetIo(void);
static void     consoleSetFontType(u8 fontType);
static void     consoleSetScreen(u8 screen);
static void     consoleSetX(u16 x);
static void     consoleSetY(u16 y);
static void     consoleQueueChar(u8 ch);
static void     consoleQueueCmd(u8 cmd, u8 parm);
static void     consoleQueueCurState(void);
static void     consoleUpdateChecksum(u16 datum);
#if DEBUG
static char *consoleCmdToString(u8 cmd);
static void consoleLogBytes(u8 *bytes, int len);
static void consoleLogFlush(void);
#endif

/*
**  ----------------
**  Public Variables
**  ----------------
*/


long colorBG;                         // Console
long colorFG;                         // Console
long fontHeightLarge;                 // Console
long fontHeightMedium;                // Console
long fontHeightSmall;                 // Console
long fontLarge;                       // Console
long fontMedium;                      // Console
long fontSmall;                       // Console
CHAR fontName[LF_FACESIZE];           // Console
long heightPX;                        // Console
long scaleX;                          // Console
long scaleY;                          // Console
long timerRate;                       // Console
long widthPX;                         // Console

/*
**  -----------------
**  Private Variables
**  -----------------
*/
static bool      doOpenConsoleWindow   = TRUE;
static bool      isConsoleWindowOpen   = FALSE;

static u8        consoleChannelNo;
static u8        consoleEqNo;

static CycleData *currentCycleData;
static int       currentCycleDataIndex = 0;
static CycleData cycleDataSequences[MaxCycleDataEntries];
static u64       minRefreshInterval;
static u64       earliestCycleFlush;

static u8        currentFontType       = FontTypeSmall;
static u16       currentIncrement      = 8;
static u8        currentScreen         = 0xff;
static u16       currentX              = 0;
static u16       currentY              = 0;
static u8        fontSizes[4]          = { FontDot, FontSmall, FontMedium, FontLarge };
static u16       xOffsets[2]           = { OffLeftScreen, OffRightScreen };

static SOCKET    connFd                = INVALID_SOCKET;
static SOCKET    listenFd              = INVALID_SOCKET;

static u8        cycleDataBuf[CycleDataBufSize];
static int       cycleDataIn           = 0;
static int       cycleDataOut          = 0;

static u8        inBuf[InBufSize];
static int       inBufIn               = 0;
static int       inBufOut              = 0;

static u8        outBuf[OutBufSize];
static int       outBufIn              = 0;

#if DEBUG
static FILE *consoleLog   = NULL;
static char consoleLogBuf[LogLineLength + 1];
static int  consoleLogBytesCol = 0;
static bool queueCharLast = FALSE;
#endif

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
    char    str[40];

#if DEBUG
    if (consoleLog == NULL)
        {
        consoleLog = fopen("consolelog.txt", "wt");
        }
#endif

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
        n = sscanf(params, "%d,%s", &consolePort, str);
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
        if (n > 1)
            {
            if (strcasecmp(str, "nowin") == 0)
                {
                doOpenConsoleWindow = FALSE;
                }
            else if (strcasecmp(str, "win") != 0)
                {
                fprintf(stderr, "(console) Unrecognized parameter in CO6612 definition: %s\n", str);
                exit(1);
                }
            }
        listenFd = netCreateListener(consolePort);
        if (listenFd == INVALID_SOCKET)
            {
            fprintf(stderr, "(console) Failed to listen for TCP connections on port %d\n", consolePort);
            exit(1);
            }
        fprintf(stdout, "(console) Listening for connections on port %d\n", consolePort);
        }

    consoleInitCycleData();

    /*
    **  Open console window, if enabled.
    */
    if (doOpenConsoleWindow)
        {
        windowInit();
        isConsoleWindowOpen = TRUE;
        }

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
        connFd      = INVALID_SOCKET;
        cycleDataIn = cycleDataOut = 0;
        inBufIn     = inBufOut     = 0;
        outBufIn    = 0;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Close the local console window.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void consoleCloseWindow(void)
    {
    if (isConsoleWindowOpen)
        {
        windowTerminate();
        isConsoleWindowOpen = FALSE;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Open the local console window.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void consoleOpenWindow(void)
    {
    if (isConsoleWindowOpen == FALSE)
        {
        windowInit();
        isConsoleWindowOpen = TRUE;
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
                consoleQueueChar(consoleToAscii[(activeChannel->data >> 6) & Mask6]);
                consoleQueueChar(consoleToAscii[(activeChannel->data >> 0) & Mask6]);
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
                    consoleQueueChar('.');
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
        consoleCheckDisplayCycle();
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
    n = select((int)(listenFd + 1), &readFds, NULL, NULL, &timeout);
    if (n > 0)
        {
        connFd = netAcceptConnection(listenFd);
        consoleInitCycleData();
        consoleQueueCurState();
        minRefreshInterval = InfiniteRefreshInterval;
        earliestCycleFlush = getMilliseconds() + minRefreshInterval;
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
**  Purpose:        Send output to remote console if display cycle detected
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
static void consoleCheckDisplayCycle(void)
    {
    CycleData *cdp;
    int i;

    if (connFd == INVALID_SOCKET) return;

    //
    // Search backward for a matching checksum. A cycle is detected if a
    // match is found.
    //
    for (i = currentCycleDataIndex - 1; i >= 0; i--)
        {
        cdp = &cycleDataSequences[i];
        if (cdp->sum1 == currentCycleData->sum1 && cdp->sum2 == currentCycleData->sum2)
            {
#if DEBUG
            if (queueCharLast) fputs("\n", consoleLog);
            fputs("cycle detected\n", consoleLog);
            queueCharLast = FALSE;
#endif
            cycleDataBuf[cycleDataIn++] = CmdEndFrame;
            currentCycleData->limit = cycleDataIn;
            consoleFlushCycleData(cdp->limit, currentCycleData->limit);
            currentCycleDataIndex = -1;
            break;
            }
        }
    if (currentCycleDataIndex + 1 < MaxCycleDataEntries)
       {
       currentCycleDataIndex += 1;
       currentCycleData = &cycleDataSequences[currentCycleDataIndex];
       memset(currentCycleData, 0, sizeof(CycleData));
       currentCycleData->first = currentCycleData->limit = cycleDataIn;
       consoleQueueCurState();
       }
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
**                  first       index of first character to flush from
**                              output buffer
**                  limit       index+1 of last character to flush from
**                              output buffer
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void consoleFlushCycleData(int first, int limit)
    {
    u64 currentTime;
    int len;
    int n;

    if (connFd != INVALID_SOCKET)
        {
        currentTime = getMilliseconds();;
#if DEBUG
        if (queueCharLast) fputs("\n", consoleLog);
        fprintf(consoleLog, "flush: first %d, limit %d, outBufIn %d, currentTime %lu, earliestCycleFlush %lu\n",
            first, limit, outBufIn, currentTime, earliestCycleFlush);
        queueCharLast = FALSE;
#endif
        if (outBufIn > 0)
            {
            if (limit > first && currentTime >= earliestCycleFlush)
                {
                n = limit - first;
                if (outBufIn + n <= OutBufSize)
                    {
                    memcpy(&outBuf[outBufIn], &cycleDataBuf[first], n);
                    outBufIn += n;
                    }
                else // output buffer overflow -- replace contents with latest cycle data
                    {
                    memcpy(outBuf, &cycleDataBuf[first], n);
                    outBufIn = n;
                    }
                earliestCycleFlush = currentTime + minRefreshInterval;
                consoleInitCycleData();
                consoleQueueCurState();
                }
            n = send(connFd, outBuf, outBufIn, 0);
            if (n > 0)
                {
#if DEBUG
                consoleLogBytes(outBuf, n);
#endif
                if (n < outBufIn)
                    {
                    memcpy(outBuf, &outBuf[n], outBufIn - n);
                    }
                outBufIn -= n;
                }
            }
        else if (limit > first)
            {
            if (currentTime >= earliestCycleFlush)
                {
                len = limit - first;
                n = send(connFd, &cycleDataBuf[first], len, 0);
#if DEBUG
                if (n > 0) consoleLogBytes(&cycleDataBuf[first], n);
#endif
                if (n < len)
                    {
                    if (n > 0)
                        {
                        len -= n;
                        memcpy(outBuf, &cycleDataBuf[first + n], len);
                        outBufIn = len;
                        }
                    }
                earliestCycleFlush = currentTime + minRefreshInterval;
                }
            consoleInitCycleData();
            consoleQueueCurState();
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Initialize cycle data collection.
**
**  Parameters:     Name        Description.
**
**  Returns:        nothing
**
**------------------------------------------------------------------------*/
static void consoleInitCycleData(void)
    {
    memset(cycleDataSequences, 0, sizeof(cycleDataSequences));
    currentCycleDataIndex = 0;
    currentCycleData      = &cycleDataSequences[0];
    cycleDataIn           = cycleDataOut = 0;
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
    u8             ch;
    int            n;
    fd_set         readFds;
    struct timeval timeout;

    FD_ZERO(&readFds);
    FD_SET(connFd, &readFds);
    timeout.tv_sec  = 0;
    timeout.tv_usec = 0;
    n = select((int)(connFd + 1), &readFds, NULL, NULL, &timeout);

    if (ppKeyIn == 0 && inBufOut < inBufIn)
        {
        ch = inBuf[inBufOut++];
        if (ch == 0x80) // set minimum refresh interval
            {
            if (inBufOut < inBufIn)
                {
                minRefreshInterval = inBuf[inBufOut++] * 10;
                if (minRefreshInterval == 0) minRefreshInterval = InfiniteRefreshInterval;
                earliestCycleFlush = getMilliseconds() + minRefreshInterval;
                }
            else
                {
                inBufOut -= 1;
                }
            return;
            }
        else if (ch == 0x81)
            {
            earliestCycleFlush = 0;
            return;
            }
        ppKeyIn = ch;
        if (inBufOut >= inBufIn) inBufIn = inBufOut = 0;
        }
    if (n > 0 && inBufIn < InBufSize && FD_ISSET(connFd, &readFds))
        {
        n = recv(connFd, &inBuf[inBufIn], InBufSize - inBufIn, 0);
        if (n <= 0)
            {
            consoleCloseRemote();
            }
        else
            {
            inBufIn += n;
            }
        }
    if (outBufIn > 0)
        {
        consoleFlushCycleData(0, 0);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Queue a character.
**
**  Parameters:     Name        Description.
**                  ch          character to be queued.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void consoleQueueChar(u8 ch)
    {
    if (connFd == INVALID_SOCKET)
        {
        if (isConsoleWindowOpen) windowQueue(ch);
        }
    else
        {
        if (cycleDataIn >= CycleDataLimit)
            {
            cycleDataBuf[cycleDataIn++] = CmdEndFrame;
            consoleFlushCycleData(0, cycleDataIn);
            }
        if (currentCycleData->limit > 0)
            {
            cycleDataBuf[cycleDataIn++] = ch;
            currentCycleData->limit = cycleDataIn;
            }
#if DEBUG
        fprintf(consoleLog, "%02x ", ch);
        queueCharLast = TRUE;
#endif
        }
    currentX += currentIncrement;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Queue a console command and parameter.
**
**  Parameters:     Name        Description.
**                  cmd         command to be queued.
**                  parm        parameter of the command
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void consoleQueueCmd(u8 cmd, u8 parm)
    {
    if (connFd != INVALID_SOCKET)
        {
        if (cycleDataIn + 1 >= CycleDataLimit)
            {
            cycleDataBuf[cycleDataIn++] = CmdEndFrame;
            consoleFlushCycleData(0, cycleDataIn);
            }
        cycleDataBuf[cycleDataIn++] = cmd;
        cycleDataBuf[cycleDataIn++] = parm;
        currentCycleData->limit = cycleDataIn;
#if DEBUG
        if (queueCharLast) fputs("\n", consoleLog);
        fprintf(consoleLog, "queueCmd: %s %02x\n", consoleCmdToString(cmd), parm);
        queueCharLast = FALSE;
#endif
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Queue current display state.
**
**  Parameters:     Name        Description.
**                  ch          character to be queued.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void consoleQueueCurState(void)
    {
    consoleQueueCmd(CmdSetScreen, currentScreen);
    consoleQueueCmd(CmdSetFontType, currentFontType);
    if (currentX > 0xff)
        {
        consoleQueueCmd(CmdSetXHigh, (u8)(currentX & 0xff));
        }
    else
        {
        consoleQueueCmd(CmdSetXLow, (u8)currentX);
        }
    if (currentY > 0xff)
        {
        consoleQueueCmd(CmdSetYHigh, (u8)(currentY & 0xff));
        }
    else
        {
        consoleQueueCmd(CmdSetYLow, (u8)currentY);
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
        if (isConsoleWindowOpen) windowSetFont(fontSizes[fontType]);
        }
    else if (currentFontType != fontType)
        {
        consoleQueueCmd(CmdSetFontType, fontType);
        }
    switch (fontType)
        {
        case FontTypeDot:
            currentIncrement = 1;
            break;
        case FontTypeSmall:
            currentIncrement = 8;
            break;
        case FontTypeMedium:
            currentIncrement = 16;
            break;
        case FontTypeLarge:
            currentIncrement = 32;
            break;
        default:
            break;
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
    if (currentScreen != screen)
        {
        consoleUpdateChecksum(screen);
        consoleQueueCmd(CmdSetScreen, screen);
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
    consoleUpdateChecksum(x);
    if (connFd == INVALID_SOCKET)
        {
        if (isConsoleWindowOpen) windowSetX(x + xOffsets[currentScreen]);
        }
    else if (x > 0xff)
        {
        consoleQueueCmd(CmdSetXHigh, (u8)(x & 0xff));
        }
    else
        {
        consoleQueueCmd(CmdSetXLow, (u8)x);
        }
    currentX = x;
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
    consoleUpdateChecksum(y);
    if (connFd == INVALID_SOCKET)
        {
        if (isConsoleWindowOpen) windowSetY(y);
        }
    else if (y > 0xff)
        {
        consoleQueueCmd(CmdSetYHigh, (u8)(y & 0xff));
        }
    else
        {
        consoleQueueCmd(CmdSetYLow, (u8)y);
        }
    currentY = y;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Update Fletcher checksum.
**
**  Parameters:     Name        Description.
**                  datum       datum with which to update checksum
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void consoleUpdateChecksum(u16 datum)
    {
    currentCycleData->sum1 += datum;
    currentCycleData->sum2 += currentCycleData->sum1;
    }

#if DEBUG
static char *consoleCmdToString(u8 cmd)
    {
    static char buf[8];

    switch (cmd)
        {
    case CmdSetXLow:     return "setXLow";
    case CmdSetYLow:     return "setYLow";
    case CmdSetXHigh:    return "setXHigh";
    case CmdSetYHigh:    return "setYHigh";
    case CmdSetScreen:   return "setScreen";
    case CmdSetFontType: return "setFontType";
    case CmdEndFrame:    return "endFrame";
    default:
        sprintf(buf, "%02x", cmd);
        return buf;
        }
    }

static void consoleLogFlush(void)
    {
    if (consoleLogBytesCol > 0)
        {
        fprintf(consoleLog, "%s\n", consoleLogBuf);
        fflush(consoleLog);
        }
    consoleLogBytesCol = 0;
    memset(consoleLogBuf, ' ', LogLineLength);
    consoleLogBuf[LogLineLength] = '\0';
    }

static void consoleLogBytes(u8 *bytes, int len)
    {
    int  ascCol;
    u8   b;
    u8   c;
    char hex[3];
    int  hexCol;
    int  i;

    consoleLogBytesCol = 0;
    consoleLogFlush(); // initialize the log buffer
    ascCol = AsciiColumn(consoleLogBytesCol);
    hexCol = HexColumn(consoleLogBytesCol);

    for (i = 0; i < len; i++)
        {
        b = c = bytes[i];
        if ((b < 0x20) || (b >= 0x7f))
            {
            c = '.';
            }
        sprintf(hex, "%02x", b);
        memcpy(consoleLogBuf + hexCol, hex, 2);
        hexCol += 3;
        consoleLogBuf[ascCol++] = c;
        if (++consoleLogBytesCol >= 16)
            {
            consoleLogFlush();
            ascCol = AsciiColumn(consoleLogBytesCol);
            hexCol = HexColumn(consoleLogBytesCol);
            }
        }
    consoleLogFlush();
    }

#endif

/*---------------------------  End Of File  ------------------------------*/
