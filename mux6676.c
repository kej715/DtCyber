/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**
**  Name: mux6676.c
**
**  Description:
**      Perform emulation of CDC 6671 and 6676 data set controllers.
**      The 6671 was used primarily as a mux for synchronous MODE4
**      terminals, and the 6676 was used primarily for asynchronous
**      TELEX/IAF terminals.
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
#define DEBUG_6671            0
#define DEBUG_6676            0
#define DEBUG_NETIO           0
#define DEBUG_PPIO            0
#define DEBUG_PPIO_VERBOSE    0

/*
**  -------------
**  Include Files
**  -------------
*/
#include <stdio.h>
#include <stdlib.h>
#include "const.h"
#include <fcntl.h>
#include "types.h"
#include "proto.h"
#include <sys/types.h>
#include <memory.h>
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
**  Function codes.
*/
#define Fc667xOutput              00001
#define Fc667xStatus              00002
#define Fc667xInput               00003

#define Fc667xEqMask              07000
#define Fc667xEqShift             9

#define St667xServiceFailure      00001
#define St667xInputRequired       00002
#define St667xChannelAReserved    00004

#define IoTurnsPerPoll            4
#define InBufSize                 256
#define OutBufSize                16

/*
**  -----------------------
**  Private Macro Functions
**  -----------------------
*/
#if DEBUG_6671 || DEBUG_6676
#define HexColumn(x)      (3 * (x) + 4)
#define AsciiColumn(x)    (HexColumn(16) + 2 + (x))
#define LogLineLength    (AsciiColumn(16))
#endif

/*
**  -----------------------------------------
**  Private Typedef and Structure Definitions
**  -----------------------------------------
*/
#define MaxPortGroups 16

typedef struct portGroup
    {
    int       listenFd;
    int       listenPort;
    int       portIndex;
    int       portCount;
    } PortGroup;

typedef struct portParam
    {
    struct muxParam *mux;
    PortGroup       *group;
    u8              id;
    bool            active;
    bool            enabled;
    bool            carrierOn;
    int             connFd;
    int             inInIdx;
    int             inOutIdx;
    u8              inBuffer[InBufSize];
    int             outInIdx;
    int             outOutIdx;
    u8              outBuffer[OutBufSize];
    } PortParam;

typedef struct muxParam
    {
    struct muxParam *next;
    char            name[20];
    int             type;
    u8              channelNo;
    u8              eqNo;
    int             portCount;
    int             ioTurns;
    PortGroup       portGroups[MaxPortGroups];
    PortParam       *ports;
    } MuxParam;

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static FcStatus mux667xFunc(PpWord funcCode);
static void mux667xActivate(void);
static void mux667xCheckIo(MuxParam *mp);
static void mux667xCreateThread(DevSlot *dp);
static void mux667xClose(PortParam *pp);
static void mux667xDisconnect(void);
static void mux667xInit(u8 eqNo, u8 channelNo, int muxType, char *params);
static void mux667xIo(void);
static bool mux667xInputRequired(MuxParam *mp);

#if DEBUG_6671 || DEBUG_6676
static char *mux667xFunc2String(PpWord funcCode);
static void mux667xLogBytes(FILE *log, MuxParam *mp, u8 *bytes, int len);
static void mux667xLogFlush(FILE *log, MuxParam *mp);

#endif

/*
**  ----------------
**  Public Variables
**  ----------------
*/
u16 mux6676TelnetPort;
u16 mux6676TelnetConns;

/*
**  -----------------
**  Private Variables
**  -----------------
*/
MuxParam *firstMux = NULL;
MuxParam *lastMux  = NULL;

static char connectingMsg[] = "\r\nConnecting to host - please wait ...";
static char noPortsMsg[]    = "\r\nNo free ports available - please try again later.\r\n";

#if DEBUG_6671
static FILE *mux6671Log = NULL;
#endif
#if DEBUG_6676
static FILE *mux6676Log = NULL;
#endif
#if DEBUG_6671 || DEBUG_6676
static char mux667xLogBuf[LogLineLength + 1];
static int  mux667xLogBytesCol = 0;
#endif

/*
 **--------------------------------------------------------------------------
 **
 **  Public Functions
 **
 **--------------------------------------------------------------------------
 */
/*--------------------------------------------------------------------------
**  Purpose:        Initialise 6671 terminal multiplexer.
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
void mux6671Init(u8 eqNo, u8 unitNo, u8 channelNo, char *params)
    {
    mux667xInit(eqNo, channelNo, DtMux6671, params);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Initialise 6676 terminal multiplexer.
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
void mux6676Init(u8 eqNo, u8 unitNo, u8 channelNo, char *params)
    {
    mux667xInit(eqNo, channelNo, DtMux6676, params);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Show mux status (operator interface).
**
**  Parameters:     Name        Description.
**
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void mux6676ShowStatus()
    {
    char      *cts;
    int       g;
    int       i;
    PortGroup *gp;
    MuxParam  *mp;
    char      *mts;
    char      outBuf[200];
    PortParam *pp;

    for (mp = firstMux; mp != NULL; mp = mp->next)
        {
        if (mp->type == DtMux6676)
            {
            mts = "6676";
            cts = "async";
            }
        else
            {
            mts = "6671";
            cts = "mode4";
            }
        for (g = 0, gp = &mp->portGroups[0]; g < MaxPortGroups && gp->portCount > 0; g++, gp++)
            {
            if (gp->listenFd > 0)
                {
                sprintf(outBuf, "    >   %-8s C%02o E%02o     ", mts, mp->channelNo, mp->eqNo);
                opDisplay(outBuf);
                sprintf(outBuf, FMTNETSTATUS "\n", netGetLocalTcpAddress(gp->listenFd), "", cts, "listening");
                opDisplay(outBuf);
                for (i = 0, pp = mp->ports + gp->portIndex; i < gp->portCount; i++, pp++)
                    {
                    if (pp->active && (pp->connFd > 0))
                        {
                        sprintf(outBuf, "    >   %-8s         P%02o ", mts, pp->id);
                        opDisplay(outBuf);
                        sprintf(outBuf, FMTNETSTATUS "\n", netGetLocalTcpAddress(pp->connFd), netGetPeerTcpAddress(pp->connFd), cts, "connected");
                        opDisplay(outBuf);
                        }
                    }
                }
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
**  Purpose:        Initialise 667x terminal multiplexer.
**
**  Parameters:     Name        Description.
**                  eqNo        equipment number
**                  channelNo   channel number the device is attached to
**                  muxType     mux device type
**                  params      optional device parameters
**
**  Returns:        Nothing.
**
**  The device parameters are a comma-separated list of pairs of TCP port
**  numbers and multiplexor port counts. Each pair specifies a TCP port on
**  which to listen for connections and a count representing the number of
**  consecutive multiplexor ports associated with the TCP port.
**
**------------------------------------------------------------------------*/
static void mux667xInit(u8 eqNo, u8 channelNo, int muxType, char *params)
    {
    char               *cp;
    DevSlot            *dp;
    u8                 g;
    PortGroup          *gp;
    u8                 i;
    int                listenPort;
    int                maxPorts;
    MuxParam           *mp;
    char               *mts;
    int                n;
    int                numParam;
    int                portCount;
    PortParam          *pp;

    dp = channelAttach(channelNo, eqNo, muxType);

    dp->activate   = mux667xActivate;
    dp->disconnect = mux667xDisconnect;
    dp->func       = mux667xFunc;
    dp->io         = mux667xIo;

    if (muxType == DtMux6676)
        {
        mts      = "MUX6676";
        maxPorts = 64;
        }
    else
        {
        mts      = "MUX6671";
        maxPorts = 16;
        }

    /*
    **  Only one MUX667x unit is possible per equipment.
    */
    if (dp->context[0] != NULL)
        {
        logDtError(LogErrorLocation, "Only one %s unit is possible per equipment\n", mts);
        exit(1);
        }

    /*
    **  Allocate and initialise mux control block.
    */
    mp = calloc(1, sizeof(MuxParam));
    if (mp == NULL)
        {
        logDtError(LogErrorLocation, "Failed to allocate %s context block\n", mts);
        exit(1);
        }
    if (firstMux == NULL)
        {
        firstMux = mp;
        }
    else
        {
        lastMux->next = mp;
        }
    lastMux = mp;

    sprintf(mp->name, "%s_CH%02o_EQ%02o", mts, channelNo, eqNo);
    mp->type      = muxType;
    mp->channelNo = channelNo;
    mp->eqNo      = eqNo;
    mp->ioTurns   = IoTurnsPerPoll - 1;
    if (params == NULL)
        {
        params = "";
        }
    cp = params;
    g  = 0;

    while (TRUE)
        {
        numParam = sscanf(cp, "%d,%d", &listenPort, &portCount);
        if (numParam < 1)
            {
            if (g > 0)
                {
                break;
                }
            else if (muxType == DtMux6676)
                {
                listenPort = mux6676TelnetPort;
                portCount  = maxPorts;
                }
            else
                {
                logDtError(LogErrorLocation, "TCP port missing from %s definition\n", mts);
                exit(1);
                }
            }
        else if (numParam < 2)
            {
            portCount = maxPorts - mp->portCount;
            }
        if ((listenPort < 0) || (listenPort > 65535))
            {
            logDtError(LogErrorLocation, "Invalid TCP port number in %s definition: %d\n", mts, listenPort);
            exit(1);
            }
        if (portCount < 1)
            {
            logDtError(LogErrorLocation, "Invalid port count %d in %s definition, valid range is 0 < count <= %d\n", portCount, mts, maxPorts);
            exit(1);
            }
        gp             = &mp->portGroups[g++];
        gp->portIndex  = mp->portCount;
        gp->portCount  = portCount;
        gp->listenPort = listenPort;
        mp->portCount += portCount;
        if (mp->portCount > maxPorts)
            {
            logDtError(LogErrorLocation, "Invalid total port count %d in %s definition, valid range is 0 < count <= %d\n", mp->portCount, mts, maxPorts);
            exit(1);
            }
        if (listenPort > 0)
            {
            /*
            **  Create socket, bind to specified port, and begin listening for connections
            */
            gp->listenFd = (int)netCreateListener(listenPort);
#if defined(_WIN32)
            if (gp->listenFd == INVALID_SOCKET)
#else
            if (gp->listenFd == -1)
#endif
                {
                logDtError(LogErrorLocation, "Can't listen for %s on port %d\n", mts, listenPort);
                exit(1);
                }
            }

        /*
        **  Advance to next port/count pair
        */
        while (*cp != '\0' && *cp != ',')
            {
            cp += 1;
            }
        if (*cp == ',')
            {
            cp += 1;
            }
        while (*cp != '\0' && *cp != ',')
            {
            cp += 1;
            }
        if (*cp == ',')
            {
            cp += 1;
            }
        }
    dp->context[0] = mp;

    /*
    **  Initialise port control blocks.
    */
    mp->ports = (PortParam *)calloc(mp->portCount, sizeof(PortParam));
    if (mp->ports == NULL)
        {
        logDtError(LogErrorLocation, "Failed to allocate %s context block\n", mts);
        exit(1);
        }
    gp = &mp->portGroups[0];
    for (i = 0, pp = mp->ports; i < mp->portCount; i++, pp++)
        {
        if (i >= gp->portIndex + gp->portCount)
            {
            gp += 1;
            }
        pp->mux       = mp;
        pp->group     = gp;
        pp->enabled   = mp->type == DtMux6676; // enabled by default for 6676
        pp->carrierOn = mp->type == DtMux6676; //      on by default for 6676
        pp->active    = FALSE;
        pp->connFd    = 0;
        pp->id        = i;
        }

    /*
    **  Print a friendly message.
    */
    printf("(mux6676) %s initialised on channel %o equipment %o, TCP port/mux ports: ",
           mts, channelNo, eqNo);
    n = 0;
    for (g = 0, gp = &mp->portGroups[0]; g < MaxPortGroups && gp->portCount > 0; g++, gp++)
        {
        if (gp->listenPort != 0)
            {
            if (n++ > 0)
                {
                fputs(", ", stdout);
                }
            printf("%d/%d", gp->listenPort, gp->portIndex);
            if (gp->portCount > 1)
                {
                printf("-%d", gp->portIndex + gp->portCount - 1);
                }
            }
        }
    fputc('\n', stdout);

#if DEBUG_6671
    if (mux6671Log == NULL)
        {
        mux6671Log = fopen("mux6671log.txt", "wt");
        }
#endif
#if DEBUG_6676
    if (mux6676Log == NULL)
        {
        mux6676Log = fopen("mux6676log.txt", "wt");
        }
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Execute function code on 667x mux.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        FcStatus
**
**------------------------------------------------------------------------*/
static FcStatus mux667xFunc(PpWord funcCode)
    {
    u8       eqNo;
    MuxParam *mp = (MuxParam *)activeDevice->context[0];

#if DEBUG_PPIO
#if DEBUG_6671
    if ((mp->type == DtMux6671) && (DEBUG_PPIO_VERBOSE != 0))
        {
        fprintf(mux6671Log, "\n%010u %s PP:%02o CH:%02o P:%04o f:%04o T:%s",
                traceSequenceNo, mp->name, activePpu->id, activeDevice->channel->id,
                activePpu->regP, funcCode, mux667xFunc2String(funcCode));
        }
#endif
#if DEBUG_6676
    if ((mp->type == DtMux6676) && (DEBUG_PPIO_VERBOSE != 0))
        {
        fprintf(mux6676Log, "\n%010u %s PP:%02o CH:%02o P:%04o f:%04o T:%s",
                traceSequenceNo, mp->name, activePpu->id, activeDevice->channel->id,
                activePpu->regP, funcCode, mux667xFunc2String(funcCode));
        }
#endif
#endif

    eqNo = (funcCode & Fc667xEqMask) >> Fc667xEqShift;
    if (eqNo != activeDevice->eqNo)
        {
        /*
        **  Equipment not configured.
        */
        return (FcDeclined);
        }

    funcCode &= ~Fc667xEqMask;

    switch (funcCode)
        {
    default:
#if DEBUG_PPIO
#if DEBUG_6671
        if ((mp->type == DtMux6671) && (DEBUG_PPIO_VERBOSE != 0))
            {
            fputs("\n  FUNC not implemented & declined!", mux6671Log);
            }
#endif
#if DEBUG_6676
        if ((mp->type == DtMux6676) && (DEBUG_PPIO_VERBOSE != 0))
            {
            fputs("\n  FUNC not implemented & declined!", mux6676Log);
            }
#endif
#endif

        return (FcDeclined);

    case Fc667xOutput:
    case Fc667xStatus:
    case Fc667xInput:
        activeDevice->recordLength = 0;
        break;
        }

    activeDevice->fcode = funcCode;

    return (FcAccepted);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Perform I/O on 667x mux.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mux667xIo(void)
    {
    PpWord    function;
    u8        in;
    MuxParam  *mp = (MuxParam *)activeDevice->context[0];
    PortParam *pp;
    u8        portNumber;
    PpWord    word;
    u8        x;

    mux667xCheckIo(mp);

    switch (activeDevice->fcode)
        {
    default:
        break;

    case Fc667xOutput:
        if (activeChannel->full)
            {
            /*
            **  Output data.
            */
            activeChannel->full = FALSE;
            portNumber          = (u8)activeDevice->recordLength++;
            if (portNumber < mp->portCount)
                {
                word     = activeChannel->data;
                function = word >> 9;
#if DEBUG_PPIO
#if DEBUG_6671
                if (mp->type == DtMux6671)
                    {
                    fprintf(mux6671Log, "\n%010u %s PP:%02o CH:%02o P:%04o f:%04o T:%s Port:%02o Data:%04o",
                            traceSequenceNo, mp->name, activePpu->id, activeDevice->channel->id,
                            activePpu->regP, activeDevice->fcode, mux667xFunc2String(activeDevice->fcode), portNumber, word);
                    }
#endif
#if DEBUG_6676
                if (mp->type == DtMux6676)
                    {
                    fprintf(mux6676Log, "\n%010u %s PP:%02o CH:%02o P:%04o f:%04o T:%s Port:%02o Data:%04o",
                            traceSequenceNo, mp->name, activePpu->id, activeDevice->channel->id,
                            activePpu->regP, activeDevice->fcode, mux667xFunc2String(activeDevice->fcode), portNumber, word);
                    }
#endif
#endif
                pp = mp->ports + portNumber;
                if (pp->active)
                    {
                    /*
                    **  Port with active TCP connection.
                    */
                    switch (function)
                    {
                    case 2:
                    case 3:
                        if (pp->mux->type == DtMux6671)
                            {
                            pp->carrierOn = FALSE;
                            }
                        break;

                    case 5:
                        if (pp->mux->type != DtMux6671)
                            {
                            break;
                            }

                    // fall through if 6671
                    case 4:
                        if (mp->type == DtMux6676)
                            {
                            x = (word >> 1) & 0x7f; // send data with parity stripped off
                            }
                        else
                            {
                            pp->carrierOn = TRUE;
                            x             = word & 0xff;
                            }
                        if (pp->outInIdx < OutBufSize)
                            {
                            pp->outBuffer[pp->outInIdx++] = x;
                            }
#if DEBUG_NETIO
#if DEBUG_6671
                        else if (pp->mux->type == DtMux6671)
                            {
                            fprintf(mux6671Log, "\n%010u %s output buffer overflow on port %02o",
                                    traceSequenceNo, pp->mux->name, pp->id);
                            }
#endif
#if DEBUG_6676
                        else if (pp->mux->type == DtMux6676)
                            {
                            fprintf(mux6676Log, "\n%010u %s output buffer overflow on port %02o",
                                    traceSequenceNo, pp->mux->name, pp->id);
                            }
#endif
#endif
                        break;

                    case 6:
                        /*
                        **  Disconnect.
                        */
                        mux667xClose(pp);
                        break;

                    case 7:
                        /*
                        **  Enable.
                        */
                        pp->enabled = TRUE;
                        break;

                    default:
                        break;
                    }
                    }
                else if ((pp->mux->type == DtMux6671) && (function == 7))
                    {
                    pp->enabled = TRUE;
                    }
                }
            }
        break;

    case Fc667xInput:
        if (!activeChannel->full)
            {
            activeChannel->data = 0;
            activeChannel->full = TRUE;
            portNumber          = (u8)activeDevice->recordLength++;
            if (portNumber < mp->portCount)
                {
                pp = mp->ports + portNumber;
                if (pp->active)
                    {
                    /*
                    **  Port with active TCP connection.
                    */
                    activeChannel->data |= 01000;
                    if (pp->inOutIdx < pp->inInIdx)
                        {
                        in = pp->inBuffer[pp->inOutIdx++];
                        if (pp->inOutIdx >= pp->inInIdx)
                            {
                            pp->inInIdx  = 0;
                            pp->inOutIdx = 0;
                            }
                        if (mp->type == DtMux6676)
                            {
                            activeChannel->data |= ((in & 0x7F) << 1) | 04000;
                            }
                        else
                            {
                            activeChannel->data |= in | 04000;
                            }
                        }
                    }
#if DEBUG_PPIO
                if ((DEBUG_PPIO_VERBOSE != 0) || ((activeChannel->data & 04000) != 0))
                    {
#if DEBUG_6671
                    if (mp->type == DtMux6671)
                        {
                        fprintf(mux6671Log, "\n%010u %s PP:%02o CH:%02o P:%04o f:%04o T:%s Port:%02o Data:%04o",
                                traceSequenceNo, mp->name, activePpu->id, activeDevice->channel->id,
                                activePpu->regP, activeDevice->fcode, mux667xFunc2String(activeDevice->fcode), portNumber, activeChannel->data);
                        }
#endif
#if DEBUG_6676
                    if (mp->type == DtMux6676)
                        {
                        fprintf(mux6676Log, "\n%010u %s PP:%02o CH:%02o P:%04o f:%04o T:%s Port:%02o Data:%04o",
                                traceSequenceNo, mp->name, activePpu->id, activeDevice->channel->id,
                                activePpu->regP, activeDevice->fcode, mux667xFunc2String(activeDevice->fcode), portNumber, activeChannel->data);
                        }
#endif
                    }
#endif
                }
            }
        break;

    case Fc667xStatus:
        activeChannel->data = St667xChannelAReserved;
        if (mux667xInputRequired(mp))
            {
            activeChannel->data |= St667xInputRequired;
            }
        activeChannel->full = TRUE;

#if DEBUG_PPIO
#if DEBUG_6671
        if (mp->type == DtMux6671)
            {
            fprintf(mux6671Log, "\n%010u %s PP:%02o CH:%02o P:%04o f:%04o T:%s Data:%04o",
                    traceSequenceNo, mp->name, activePpu->id, activeDevice->channel->id,
                    activePpu->regP, activeDevice->fcode, mux667xFunc2String(activeDevice->fcode), activeChannel->data);
            }
#endif
#if DEBUG_6676
        if (mp->type == DtMux6676)
            {
            fprintf(mux6676Log, "\n%010u %s PP:%02o CH:%02o P:%04o f:%04o T:%s Data:%04o",
                    traceSequenceNo, mp->name, activePpu->id, activeDevice->channel->id,
                    activePpu->regP, activeDevice->fcode, mux667xFunc2String(activeDevice->fcode), activeChannel->data);
            }
#endif
#endif
        break;
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
static void mux667xActivate(void)
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
static void mux667xDisconnect(void)
    {
    }

/*--------------------------------------------------------------------------
**  Purpose:        Check for I/O availability.
**
**  Parameters:     Name        Description.
**                  mp          pointer to mux parameters.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mux667xCheckIo(MuxParam *mp)
    {
    PortParam *availablePort;

#if defined(_WIN32)
    u_long blockEnable = 1;
#endif
    struct sockaddr_in from;
#if defined(_WIN32)
    int fromLen;
#else
    socklen_t fromLen;
#endif
    int            fd;
    int            g;
    PortGroup      *gp;
    int            i;
    int            maxFd;
    int            n;
    int            optEnable = 1;
    PortParam      *pp;
    fd_set         readFds;
    struct timeval timeout;
    fd_set         writeFds;

    mp->ioTurns = (mp->ioTurns + 1) % IoTurnsPerPoll;
    if (mp->ioTurns != 0)
        {
        return;
        }

    FD_ZERO(&readFds);
    FD_ZERO(&writeFds);
    maxFd = 0;

    for (i = 0, pp = mp->ports; i < mp->portCount; i++, pp++)
        {
        if (pp->active)
            {
            if (pp->inInIdx < InBufSize)
                {
                FD_SET(pp->connFd, &readFds);
                if (pp->connFd > maxFd)
                    {
                    maxFd = pp->connFd;
                    }
                }
            if (pp->carrierOn)
                {
                if (pp->outInIdx > pp->outOutIdx)
                    {
                    FD_SET(pp->connFd, &writeFds);
                    if (pp->connFd > maxFd)
                        {
                        maxFd = pp->connFd;
                        }
                    }
                }
            }
        else if (pp->enabled && (pp->group->listenFd != 0) && !FD_ISSET(pp->group->listenFd, &readFds))
            {
            FD_SET(pp->group->listenFd, &readFds);
            if (pp->group->listenFd > maxFd)
                {
                maxFd = pp->group->listenFd;
                }
            }
        }

    if (maxFd < 1)
        {
        return;
        }

    timeout.tv_sec  = 0;
    timeout.tv_usec = 0;
    n = select(maxFd + 1, &readFds, &writeFds, NULL, &timeout);
    if (n < 1)
        {
        return;
        }

    for (i = 0, pp = mp->ports; i < mp->portCount; i++, pp++)
        {
        if (pp->active)
            {
            if (FD_ISSET(pp->connFd, &readFds))
                {
                n = recv(pp->connFd, &pp->inBuffer[pp->inInIdx], InBufSize - pp->inInIdx, 0);
                if (n > 0)
                    {
#if DEBUG_NETIO
#if DEBUG_6671
                    if (pp->mux->type == DtMux6671)
                        {
                        fprintf(mux6671Log, "\n%010u %s received %d bytes on port %02o",
                                traceSequenceNo, mp->name, n, pp->id);
                        mux667xLogBytes(mux6671Log, mp, &pp->inBuffer[pp->inInIdx], n);
                        }
#endif
#if DEBUG_6676
                    if (pp->mux->type == DtMux6676)
                        {
                        fprintf(mux6676Log, "\n%010u %s received %d bytes on port %02o",
                                traceSequenceNo, mp->name, n, pp->id);
                        mux667xLogBytes(mux6676Log, mp, &pp->inBuffer[pp->inInIdx], n);
                        }
#endif
#endif
                    pp->inInIdx += n;
                    }
                else
                    {
                    mux667xClose(pp);
                    }
                }
            if (FD_ISSET(pp->connFd, &writeFds) && (pp->outOutIdx < pp->outInIdx))
                {
                n = send(pp->connFd, &pp->outBuffer[pp->outOutIdx], pp->outInIdx - pp->outOutIdx, 0);
                if (n >= 0)
                    {
#if DEBUG_NETIO
#if DEBUG_6671
                    if (pp->mux->type == DtMux6671)
                        {
                        fprintf(mux6671Log, "\n%010u %s sent %d bytes to port %02o",
                                traceSequenceNo, mp->name, n, pp->id);
                        mux667xLogBytes(mux6671Log, mp, &pp->outBuffer[pp->outOutIdx], n);
                        }
#endif
#if DEBUG_6676
                    if (pp->mux->type == DtMux6676)
                        {
                        fprintf(mux6676Log, "\n%010u %s sent %d bytes to port %02o",
                                traceSequenceNo, mp->name, n, pp->id);
                        mux667xLogBytes(mux6676Log, mp, &pp->outBuffer[pp->outOutIdx], n);
                        }
#endif
#endif
                    pp->outOutIdx += n;
                    if (pp->outOutIdx >= pp->outInIdx)
                        {
                        pp->outInIdx  = 0;
                        pp->outOutIdx = 0;
                        }
                    }
                }
            }
        }
    for (g = 0, gp = &mp->portGroups[0]; g < MaxPortGroups && gp->portCount > 0; g++, gp++)
        {
        if ((gp->listenFd != 0) && FD_ISSET(gp->listenFd, &readFds))
            {
            fromLen = sizeof(from);
            fd      = (int)accept(gp->listenFd, (struct sockaddr *)&from, &fromLen);
            if (fd < 0)
                {
                break;
                }
            availablePort = NULL;
            for (i = gp->portIndex; i < gp->portIndex + gp->portCount; i++)
                {
                pp = mp->ports + i;
                if (pp->active == FALSE)
                    {
                    availablePort = pp;
                    break;
                    }
                }
            if (availablePort != NULL)
                {
                availablePort->active    = TRUE;
                availablePort->connFd    = fd;
                availablePort->inInIdx   = 0;
                availablePort->inOutIdx  = 0;
                availablePort->outInIdx  = 0;
                availablePort->outOutIdx = 0;

                /*
                **  Set Keepalive option so that we can eventually discover if
                **  a client has been rebooted.
                */
                setsockopt(availablePort->connFd, SOL_SOCKET, SO_KEEPALIVE, (void *)&optEnable, sizeof(optEnable));

                /*
                **  Make socket non-blocking.
                */
#if defined(_WIN32)
                ioctlsocket(availablePort->connFd, FIONBIO, &blockEnable);
#else
                fcntl(availablePort->connFd, F_SETFL, O_NONBLOCK);
#endif
                if (availablePort->mux->type == DtMux6676)
                    {
                    send(fd, connectingMsg, (int)strlen(connectingMsg), 0);
                    }
#if DEBUG_NETIO
#if DEBUG_6671
                if (availablePort->mux->type == DtMux6671)
                    {
                    fprintf(mux6671Log, "\n%010u %s accepted connection on port %02o",
                            traceSequenceNo, availablePort->mux->name, availablePort->id);
                    }
#endif
#if DEBUG_6676
                if (availablePort->mux->type == DtMux6676)
                    {
                    fprintf(mux6676Log, "\n%010u %s accepted connection on port %02o",
                            traceSequenceNo, availablePort->mux->name, availablePort->id);
                    }
#endif
#endif
                }
            else
                {
                if (mp->type == DtMux6676)
                    {
                    send(fd, noPortsMsg, (int)strlen(noPortsMsg), 0);
                    }
                netCloseConnection(fd);
                }
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Determine if input is required.
**
**  Parameters:     Name        Description.
**                  mp          pointer to mux parameters
**
**  Returns:        TRUE if input is required, FALSE otherwise.
**
**------------------------------------------------------------------------*/
static bool mux667xInputRequired(MuxParam *mp)
    {
    int       i;
    PortParam *pp;

    for (i = 0, pp = mp->ports; i < mp->portCount; i++, pp++)
        {
        if (pp->active && (pp->inOutIdx < pp->inInIdx))
            {
            return TRUE;
            }
        }

    return FALSE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Close a mux port and mark it inactive.
**
**  Parameters:     Name        Description.
**                  pp          pointer to mux port parameters.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mux667xClose(PortParam *pp)
    {
    netCloseConnection(pp->connFd);
    pp->connFd    = 0;
    pp->active    = FALSE;
    pp->inInIdx   = 0;
    pp->inOutIdx  = 0;
    pp->outInIdx  = 0;
    pp->outOutIdx = 0;
    if (pp->mux->type == DtMux6671)
        {
        pp->enabled   = FALSE;
        pp->carrierOn = FALSE;
        }
#if DEBUG_NETIO
#if DEBUG_6671
    if (pp->mux->type == DtMux6671)
        {
        fprintf(mux6671Log, "\n%010u %s connection closed on port %02o",
                traceSequenceNo, pp->mux->name, pp->id);
        }
#endif
#if DEBUG_6676
    if (pp->mux->type == DtMux6676)
        {
        fprintf(mux6676Log, "\n%010u %s connection closed on port %02o",
                traceSequenceNo, pp->mux->name, pp->id);
        }
#endif
#endif
    }

#if DEBUG_6671 || DEBUG_6676

/*--------------------------------------------------------------------------
**  Purpose:        Convert function code to string.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        String equivalent of function code.
**
**------------------------------------------------------------------------*/
static char *mux667xFunc2String(PpWord funcCode)
    {
    static char buf[30];

    switch (funcCode)
        {
    case Fc667xOutput:
        return "Output";

    case Fc667xInput:
        return "Input ";

    case Fc667xStatus:
        return "Status";
        }
    sprintf(buf, "UNKNOWN: %04o", funcCode);

    return (buf);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Flush incomplete data line
**
**  Parameters:     Name        Description.
**                  log         log file pointer
**                  mp          pointer to mux parameters
**
**  Returns:        nothing
**
**------------------------------------------------------------------------*/
static void mux667xLogFlush(FILE *log, MuxParam *mp)
    {
    if (mux667xLogBytesCol > 0)
        {
        fprintf(log, "\n%010u %s ", traceSequenceNo, mp->name);
        fputs(mux667xLogBuf, log);
        fflush(log);
        }
    mux667xLogBytesCol = 0;
    memset(mux667xLogBuf, ' ', LogLineLength);
    mux667xLogBuf[LogLineLength] = '\0';
    }

/*--------------------------------------------------------------------------
**  Purpose:        Log a sequence of ASCII bytes
**
**  Parameters:     Name        Description.
**                  log         log file pointer
**                  mp          pointer to mux parameters
**                  bytes       pointer to sequence of bytes
**                  len         length of the sequence
**
**  Returns:        nothing
**
**------------------------------------------------------------------------*/
static void mux667xLogBytes(FILE *log, MuxParam *mp, u8 *bytes, int len)
    {
    u8   ac;
    int  ascCol;
    u8   b;
    char hex[3];
    int  hexCol;
    int  i;

    mux667xLogBytesCol = 0;
    mux667xLogFlush(log, mp); // initialize the log buffer
    ascCol = AsciiColumn(mux667xLogBytesCol);
    hexCol = HexColumn(mux667xLogBytesCol);

    for (i = 0; i < len; i++)
        {
        b  = bytes[i];
        ac = b;
        if ((ac < 0x20) || (ac >= 0x7f))
            {
            ac = '.';
            }
        sprintf(hex, "%02x", b);
        memcpy(mux667xLogBuf + hexCol, hex, 2);
        hexCol += 3;
        mux667xLogBuf[ascCol++] = ac;
        if (++mux667xLogBytesCol >= 16)
            {
            mux667xLogFlush(log, mp);
            ascCol = AsciiColumn(mux667xLogBytesCol);
            hexCol = HexColumn(mux667xLogBytesCol);
            }
        }
    mux667xLogFlush(log, mp);
    }

#endif

/*---------------------------  End Of File  ------------------------------*/
