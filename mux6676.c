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
#define DEBUG_6671         0
#define DEBUG_6676         0
#define DEBUG_NETIO        0
#define DEBUG_PPIO         0
#define DEBUG_PPIO_VERBOSE 0

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
#define Fc667xOutput            00001
#define Fc667xStatus            00002
#define Fc667xInput             00003

#define Fc667xEqMask            07000
#define Fc667xEqShift           9

#define St667xServiceFailure    00001
#define St667xInputRequired     00002
#define St667xChannelAReserved  00004

#define IoTurnsPerPoll          4
#define InBufSize               256
#define OutBufSize              16

/*
**  -----------------------
**  Private Macro Functions
**  -----------------------
*/
#if DEBUG_6671||DEBUG_6676
#define HexColumn(x) (3 * (x) + 4)
#define AsciiColumn(x) (HexColumn(16) + 2 + (x))
#define LogLineLength (AsciiColumn(16))
#endif

/*
**  -----------------------------------------
**  Private Typedef and Structure Definitions
**  -----------------------------------------
*/
typedef struct portParam
    {
    struct muxParam *mux;
    u8   id;
    bool active;
    bool enabled;
    bool carrierOn;
    int  connFd;
    int  inInIdx;
    int  inOutIdx;
    u8   inBuffer[InBufSize];
    int  outInIdx;
    int  outOutIdx;
    u8   outBuffer[OutBufSize];
    } PortParam;

typedef struct muxParam
    {
    char name[20];
    int type;
    int listenPort;
    int listenFd;
    int portCount;
    int ioTurns;
    PortParam *ports;
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

#if DEBUG_6671||DEBUG_6676
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
#if DEBUG_6671
static FILE *mux6671Log = NULL;
#endif
#if DEBUG_6676
static FILE *mux6676Log = NULL;
#endif
#if DEBUG_6671||DEBUG_6676
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
**                  deviceName  optional device file name
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void mux6671Init(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName)
    {
    mux667xInit(eqNo, channelNo, DtMux6671, deviceName);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Initialise 6676 terminal multiplexer.
**
**  Parameters:     Name        Description.
**                  eqNo        equipment number
**                  unitNo      unit number
**                  channelNo   channel number the device is attached to
**                  deviceName  optional device file name
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void mux6676Init(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName)
    {
    mux667xInit(eqNo, channelNo, DtMux6676, deviceName);
    }

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
**------------------------------------------------------------------------*/
static void mux667xInit(u8 eqNo, u8 channelNo, int muxType, char *params)
    {
#if defined(_WIN32)
    u_long blockEnable = 1;
#endif
    DevSlot *dp;
    u8 i;
    int listenPort;
    int maxPorts;
    MuxParam *mp;
    char *mts;
    int numParam;
    int optEnable = 1;
    int portCount;
    PortParam *pp;
    struct sockaddr_in server;

    dp = channelAttach(channelNo, eqNo, muxType);

    dp->activate = mux667xActivate;
    dp->disconnect = mux667xDisconnect;
    dp->func = mux667xFunc;
    dp->io = mux667xIo;

    mts = (muxType == DtMux6676) ? "MUX6676" : "MUX6671";

    /*
    **  Only one MUX667x unit is possible per equipment.
    */
    if (dp->context[0] != NULL)
        {
        fprintf(stderr, "Only one %s unit is possible per equipment\n", mts);
        exit (1);
        }
    if (params == NULL) params = "";
    numParam = sscanf(params, "%d,%d", &listenPort, &portCount);
    if (numParam < 1)
        {
        if (muxType == DtMux6676)
            {
            listenPort = mux6676TelnetPort;
            portCount = mux6676TelnetConns;
            }
        else
            {
            fprintf(stderr, "TCP port missing from %s definition\n", mts);
            exit(1);
            }
        }
    else if (numParam < 2)
        {
        portCount = (muxType == DtMux6676) ? mux6676TelnetConns : 16;
        }
    if (listenPort < 1 || listenPort > 65535)
        {
        fprintf(stderr, "Invalid TCP port number in %s definition: %d\n", mts, listenPort);
        exit(1);
        }
    maxPorts = (muxType == DtMux6676) ? 64 : 16;
    if (portCount < 1 || portCount > maxPorts)
        {
        fprintf(stderr, "Invalid mux port count in %s definition: %d\n", mts, portCount);
        exit(1);
        }

    mp = calloc(1, sizeof(MuxParam));
    if (mp == NULL)
        {
        fprintf(stderr, "Failed to allocate %s context block\n", mts);
        exit(1);
        }
    dp->context[0] = mp;

    /*
    **  Initialise mux control block.
    */
    sprintf(mp->name, "%s_CH%02o_EQ%02o", mts, channelNo, eqNo);
    mp->type = muxType;
    mp->listenPort = listenPort;
    mp->portCount = portCount;
    mp->ports = (PortParam *)calloc(portCount, sizeof(PortParam));
    mp->ioTurns = IoTurnsPerPoll - 1;
    if (mp->ports == NULL)
        {
        fprintf(stderr, "Failed to allocate %s context block\n", mts);
        exit(1);
        }
    /*
    **  Initialise port control blocks.
    */
    for (i = 0, pp = mp->ports; i < portCount; i++, pp++)
        {
        pp->mux = mp;
        pp->enabled   = mp->type == DtMux6676; // enabled by default for 6676
        pp->carrierOn = mp->type == DtMux6676; //      on by default for 6676
        pp->active = FALSE;
        pp->connFd = 0;
        pp->id = i;
        }

    /*
    **  Create socket, bind to specified port, and begin listening for connections
    */
    mp->listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (mp->listenFd < 0)
        {
        fprintf(stderr, "Can't create socket for %s on port %d\n", mts, mp->listenPort);
        exit(1);
        }
    /*
    **  Accept will block if client drops connection attempt between select and accept.
    **  We can't block so make listening socket non-blocking to avoid this condition.
    */
#if defined(_WIN32)
    ioctlsocket(mp->listenFd, FIONBIO, &blockEnable);
#else
    fcntl(mp->listenFd, F_SETFL, O_NONBLOCK);
#endif
    /*
    **  Bind to configured TCP port number
    */
    setsockopt(mp->listenFd, SOL_SOCKET, SO_REUSEADDR, (void *)&optEnable, sizeof(optEnable));
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr("0.0.0.0");
    server.sin_port = htons(mp->listenPort);
    if (bind(mp->listenFd, (struct sockaddr *)&server, sizeof(server)) < 0)
        {
        fprintf(stderr, "Can't bind to listen socket for %s on port %d\n", mts, mp->listenPort);
        exit(1);
        }
    /*
    **  Start listening for new connections on this TCP port number
    */
    if (listen(mp->listenFd, 5) < 0)
        {
        fprintf(stderr, "Can't listen for %s on port %d\n", mts, mp->listenPort);
        exit(1);
        }

    /*
    **  Print a friendly message.
    */
    printf("%s initialised on channel %o equipment %o, mux ports %d, TCP port %d\n",
        mts, channelNo, eqNo, mp->portCount, mp->listenPort);

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
    u8 eqNo;
    MuxParam *mp = (MuxParam *)activeDevice->context[0];

#if DEBUG_PPIO
#if DEBUG_6671
    if (mp->type == DtMux6671 && DEBUG_PPIO_VERBOSE != 0)
        {
        fprintf(mux6671Log, "\n%010u %s PP:%02o CH:%02o P:%04o f:%04o T:%s",
            traceSequenceNo, mp->name, activePpu->id, activeDevice->channel->id,
            activePpu->regP, funcCode, mux667xFunc2String(funcCode));
        }
#endif
#if DEBUG_6676
    if (mp->type == DtMux6676 && DEBUG_PPIO_VERBOSE != 0)
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
        return(FcDeclined);
        }

    funcCode &= ~Fc667xEqMask;

    switch (funcCode)
        {
    default:
#if DEBUG_PPIO
#if DEBUG_6671
        if (mp->type == DtMux6671 && DEBUG_PPIO_VERBOSE != 0)
            fputs("\n  FUNC not implemented & declined!", mux6671Log);
#endif
#if DEBUG_6676
        if (mp->type == DtMux6676 && DEBUG_PPIO_VERBOSE != 0)
            fputs("\n  FUNC not implemented & declined!", mux6676Log);
#endif
#endif
        return(FcDeclined);

    case Fc667xOutput:
    case Fc667xStatus:
    case Fc667xInput:
        activeDevice->recordLength = 0;
        break;
        }

    activeDevice->fcode = funcCode;
    return(FcAccepted);
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
    PpWord function;
    u8 in;
    MuxParam *mp = (MuxParam *)activeDevice->context[0];
    PortParam *pp;
    u8 portNumber;
    PpWord word;
    u8 x;

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
            portNumber = (u8)activeDevice->recordLength++;
            if (portNumber < mp->portCount)
                {
                word = activeChannel->data;
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
                        if (pp->mux->type != DtMux6671) break;
                        // fall through if 6671
                    case 4:
                        if (mp->type == DtMux6676)
                            {
                            x = (word >> 1) & 0x7f; // send data with parity stripped off
                            }
                        else
                            {
                            pp->carrierOn = TRUE;
                            x = word & 0xff;
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
                else if (pp->mux->type == DtMux6671 && function == 7)
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
            portNumber = (u8)activeDevice->recordLength++;
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
                            pp->inInIdx = 0;
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
                if (DEBUG_PPIO_VERBOSE != 0 || (activeChannel->data & 04000) != 0)
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
    int activeCount;
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
    int i;
    int maxFd;
    int n;
    int optEnable = 1;
    PortParam *pp;
    fd_set readFds;
    struct timeval timeout;
    fd_set writeFds;

    mp->ioTurns = (mp->ioTurns + 1) % IoTurnsPerPoll;
    if (mp->ioTurns != 0) return;

    FD_ZERO(&readFds);
    FD_ZERO(&writeFds);
    maxFd = 0;
    availablePort = NULL;

    for (i = 0, pp = mp->ports; i < mp->portCount; i++, pp++)
        {
        if (pp->active)
            {
            if (pp->inInIdx < InBufSize)
                {
                FD_SET(pp->connFd, &readFds);
                if (pp->connFd > maxFd) maxFd = pp->connFd;
                }
            if (pp->carrierOn)
                {
                if (pp->outInIdx > pp->outOutIdx)
                    {
                    FD_SET(pp->connFd, &writeFds);
                    if (pp->connFd > maxFd) maxFd = pp->connFd;
                    }
                }
            }
        else if (availablePort == NULL && pp->enabled)
            {
            availablePort = pp;
            FD_SET(mp->listenFd, &readFds);
            if (mp->listenFd > maxFd) maxFd = mp->listenFd;
            }
        }

    if (maxFd < 1) return;

    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    n = select(maxFd + 1, &readFds, &writeFds, NULL, &timeout);
    if (n < 1) return;

    for (i = 0, pp = mp->ports; i < mp->portCount; i++, pp++)
        {
        if (pp->active)
            {
            if (FD_ISSET(pp->connFd, &readFds)) {
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
            if (FD_ISSET(pp->connFd, &writeFds) && pp->outOutIdx < pp->outInIdx)
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
    if (availablePort != NULL && FD_ISSET(mp->listenFd, &readFds))
        {
        fromLen = sizeof(from);
        availablePort->connFd = accept(mp->listenFd, (struct sockaddr *)&from, &fromLen);
        if (availablePort->connFd > 0)
            {
            availablePort->active = TRUE;
            availablePort->inInIdx = 0;
            availablePort->inOutIdx = 0;
            availablePort->outInIdx = 0;
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
#if DEBUG_NETIO
#if DEBUG_6671
            if (availablePort->mux->type == DtMux6671)
                fprintf(mux6671Log, "\n%010u %s accepted connection on port %02o",
                    traceSequenceNo, availablePort->mux->name, availablePort->id);
#endif
#if DEBUG_6676
            if (availablePort->mux->type == DtMux6676)
                fprintf(mux6676Log, "\n%010u %s accepted connection on port %02o",
                    traceSequenceNo, availablePort->mux->name, availablePort->id);
#endif
#endif
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
    int i;
    PortParam *pp;

    for (i = 0, pp = mp->ports; i < mp->portCount; i++, pp++)
         {
         if (pp->active && pp->inOutIdx < pp->inInIdx) return TRUE;
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
#if defined(_WIN32)
    closesocket(pp->connFd);
#else
    close(pp->connFd);
#endif
    pp->connFd    = 0;
    pp->active    = FALSE;
    pp->inInIdx   = 0;
    pp->inOutIdx  = 0;
    pp->outInIdx  = 0;
    pp->outOutIdx = 0;
    if (pp->mux->type == DtMux6671)
        {
        pp->enabled = FALSE;
        pp->carrierOn = FALSE;
        }
#if DEBUG_NETIO
#if DEBUG_6671
    if (pp->mux->type == DtMux6671)
        fprintf(mux6671Log, "\n%010u %s connection closed on port %02o",
            traceSequenceNo, pp->mux->name, pp->id);
#endif
#if DEBUG_6676
    if (pp->mux->type == DtMux6676)
        fprintf(mux6676Log, "\n%010u %s connection closed on port %02o",
            traceSequenceNo, pp->mux->name, pp->id);
#endif
#endif
    }

#if DEBUG_6671||DEBUG_6676
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
    switch(funcCode)
        {
    case Fc667xOutput : return "Output";
    case Fc667xInput  : return "Input ";
    case Fc667xStatus : return "Status";
        }
    sprintf(buf, "UNKNOWN: %04o", funcCode);
    return(buf);
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
    u8 ac;
    int ascCol;
    u8 b;
    char hex[3];
    int hexCol;
    int i;

    mux667xLogBytesCol = 0;
    mux667xLogFlush(log, mp); // initialize the log buffer
    ascCol = AsciiColumn(mux667xLogBytesCol);
    hexCol = HexColumn(mux667xLogBytesCol);

    for (i = 0; i < len; i++)
        {
        b = bytes[i];
        ac = b;
        if (ac < 0x20 || ac >= 0x7f)
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
