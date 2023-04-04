/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Gerard J van der Grinten, Tom Hunter
**
**  Name: tpmux.c
**
**  Description:
**      Perform emulation of the two port multiplexer.
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

#define DEBUG    0

/*
**  -------------
**  Include Files
**  -------------
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "const.h"
#include "types.h"
#include "proto.h"
#if defined(_WIN32)
#include <winsock.h>
#else
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
**
**         THE FOLLOWING IS A DESCIPTION OF TWO-PORT MULTIPLEXER
**         STATUS AND DATA FORMATS.
**
**
**         TWO-PORT MULTIPLEXER STATUS CODES-
**
**                BIT         DESCRIPTION
**                ---         -----------
**
**               11-5         NOT USED.
**                  4         OUTPUT BUFFER READY (NOT FULL).
**                  3         INPUT READY.
**                  2         CARRIER ON.
**                  1         DATA SET READY.
**                  0         RING INDICATOR.
**
**
**         TWO-PORT MULTIPLEXER MODE SELECTION FUNCTION CODES-
**
**                BIT         DESCRIPTION
**                ---         -----------
**
**               11-6         NOT USED.
**                  5         ENABLE LOOP-BACK.
**                  4         DISABLE PARITY.
**                  3         NUMBER OF STOP BITS-
**                                0 = 1 STOP BIT.
**                                1 = 2 STOP BITS.
**                2-1         DATA BITS PER CHARACTER-
**                               00 = 5 BITS.
**                               01 = 6 BITS.
**                               10 = 7 BITS.
**                               11 = 8 BITS.
**                  0         PARITY-
**                                0 = ODD PARITY.
**                                1 = EVEN PARITY.
**
**
**         INPUT DATA BYTE FORMAT-
**
**                BIT         DESCRIPTION
**                ---         -----------
**
**                 11         DATA SET READY.
**                 10         DATA SET READY .AND. CARRIER ON.
**                  9         LOST DATA.
**                  8         FRAMING ERROR OR PARITY ERROR.
**                7-0         DATA BITS.
**
**
**         OUTPUT DATA BYTE FORMAT-
**
**                BIT         DESCRIPTION
**                ---         -----------
**
**               11-8         NOT USED.
**                  7         DATA PARITY.
**                6-0         DATA BITS (LEAST SIGNIFICANT DATA BIT
**                            IN BIT POSITION 0).
**
**
**         TWO-PORT MULTIPLEXER EST ENTRY.
**
**         THE FORMAT OF THE TWO-PORT MULTIPLEXER EQUIPMENT STATUS
**         TABLE ENTRY IS AS SHOWN BELOW.
**
**         EST    12/0, 12/CH, 12/0, 12/RM, 11/0, 1/N
**
**                CH = CHANNEL NUMBER.
**
**                RM = MNEMONIC *RM*.
**
**                N  = PORT NUMBER, 0 OR 1.
**
**
**         THE FORMAT OF THE *EQPDECK* ENTRY FOR THE TWO-PORT
**         MULTIPLEXER IS AS SHOWN BELOW.
**
**         EQXXX=RM,ST=ON/OFF,CH=NN,PT=N.
**
**                *EQ* = MNEMONIC *EQ*.
**
**                *RM* = MNEMONIC *RM*.
**
**                ST   = STATUS, *ON* OR *OFF*.
**
**                PN   = PORT NUMBER, *0* OR *1*.
**
**                CH   = CHANNEL NUMBER.
**
**         EXAMPLE -
**
**         EQ765=RM,ST=ON,CH=15,PT=0.
**
**                THE ABOVE ENTRY DEFINES EST ORDINAL 765 AS PORT 0 OF
**                THE TWO-PORT MULTIPLEXER, LOGICALLY *ON* IN THE EST.
**
**
**
**           TWO PORT MULTIPLEXOR EQUIVALENCES.
**
**           FUNCTIONS.
**
**  MXSS     EQU    0000        STATUS SUMMARY
**  MXRD     EQU    0100        READ CHARACTER
**  MXWT     EQU    0200        WRITE CHARACTERS
**  MXSM     EQU    0300        SET TERMINAL OPERATION MODE
**  MXDR     EQU    0400        SET/CLEAR DATA TERMINAL READY SIGNAL
**  MXRTS    EQU    0500        SET/CLEAR REQUEST TO SEND SIGNAL
**           EQU    0600        (NOT USED)
**  MXMC     EQU    0700        MASTER CLEAR
**  MXDM     EQU    6000        DESELECT TERMINAL
**  MXPT     EQU    7000        CONNECT TO PORT
**
**           STATUS SUMMARY BIT DEFINITIONS.
**
**  OBRB     EQU    4           OUTPUT BUFFER NOT FULL
**  INRB     EQU    3           INPUT READY
**  DCDB     EQU    2           DATA CARRIER DETECT
**  DSRB     EQU    1           DATA SET READY
**  RNGB     EQU    0           RING INDICATOR
**
**           INPUT CHARACTER BIT DEFINITIONS.
**
**  RDSR     EQU    13          DATA SET READY
**  RDSC     EQU    12          DATA SET READY AND DATA CHARACTER DETECT
**  ROVR     EQU    11          OVER RUN
**  RFPE     EQU    10          FRAMING PARITY ERROR
**  CHAR     EQU    0-7         DATA CHARACTER
**
**           OPERATION MODE SELECTIONS.
**
**  SPTY     EQU    0020        NO PARITY
**  SSTP     EQU    0010        SELECT ADDITIONAL STOP BIT
**  S8BC     EQU    0006        SELECT 8 DATA BITS PER CHARACTER
**  S7BC     EQU    0004        SELECT 7 DATA BITS PER CHARACTER
**  S6BC     EQU    0002        SELECT 6 DATA BITS PER CHARACTER
**  S5BC     EQU    0000        SELECT 5 DATA BITS PER CHARACTER
**  SODD     EQU    0001        SELECT ODD PARITY
**
*/

/*
**  Function codes.
*/
#define FcTpmStatusSumary     00000
#define FcTpmReadChar         00100
#define FcTpmWriteChar        00200
#define FcTpmSetTerminal      00300
#define FcTpmFlipDTR          00400
#define FcTpmFlipRTS          00500
#define FcTpmNotUsed          00600
#define FcTpmMasterClear      00700
#define FcTpmDeSelect         06000
#define FcTpmConPort          07000

#define StTpmOutputNotFull    00020
#define StTpmInputReady       00010
#define StTpmDataCharDet      00004
#define StTpmDataSetReady     00002
#define StTpmRingIndicator    00001

#define IcTpmDataSetReady     04000
#define IcTpmDsrAndDcd        02000
#define IcTpmDataOverrun      01000
#define IcTpmFramingError     00400

#define TpmSystemConsole      00000
#define TpmMaintConsole       00001

#define IoTurnsPerPoll        4
#define InBufSize             256
#define OutBufSize            32
#define MaxPorts              2

/*
**  -----------------------
**  Private Macro Functions
**  -----------------------
*/

/*
**  -----------------------------------------
**  Private Typedef and Structure Definitions
**  -----------------------------------------
*/
typedef struct portParam
    {
    u8     id;
    bool   active;
    int    connFd;
    PpWord status;
    int    inInIdx;
    int    inOutIdx;
    u8     inBuffer[InBufSize];
    int    outInIdx;
    int    outOutIdx;
    u8     outBuffer[OutBufSize];
    } PortParam;

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static void     tpMuxCheckIo(void);
static FcStatus tpMuxFunc(PpWord funcCode);
static void     tpMuxIo(void);
static void     tpMuxActivate(void);
static void     tpMuxDisconnect(void);

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
static int        ioTurns     = IoTurnsPerPoll - 1;
static int        listenFd    = 0;
static DevSlot    *mux        = NULL;
static PortParam  *portVector = NULL;
static u16        telnetPort  = 6602;

static char connectingMsg[] = "\r\nConnecting to host - please wait ...";
static char noPortsMsg[]    = "\r\nNo free ports available - please try again later.\r\n";

/*
 **--------------------------------------------------------------------------
 **
 **  Public Functions
 **
 **--------------------------------------------------------------------------
 */

/*--------------------------------------------------------------------------
**  Purpose:        Initialise terminal multiplexer.
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
void tpMuxInit(u8 eqNo, u8 unitNo, u8 channelNo, char *params)
    {
    DevSlot   *dp;
    u8        i;
    long      port;
    PortParam *pp;

    if (mux != NULL)
        {
        fputs("(tpmux  ) Only one TPM allowed\n", stderr);
        exit(1);
        }

    dp = mux         = channelAttach(channelNo, eqNo, DtTpm);
    dp->activate     = tpMuxActivate;
    dp->disconnect   = tpMuxDisconnect;
    dp->func         = tpMuxFunc;
    dp->io           = tpMuxIo;
    dp->selectedUnit = -1;

    if (params != NULL)
        {
        //  Parse the TCP Port Number
        port = strtol(params, NULL, 10);
        if (port < 1 || port > 65535)
            {
            fprintf(stderr, "(tpmux  ) Invalid TCP port number in TPM definition: %ld\n", port);
            exit(1);
            }
        telnetPort = (u16)port;
        }

    portVector = calloc(MaxPorts, sizeof(PortParam));
    if (portVector == NULL)
        {
        fprintf(stderr, "(tpmux  ) Failed to allocate two port mux context block\n");
        exit(1);
        }

    dp->context[0] = portVector;

    /*
    **  Initialise port control blocks.
    */
    for (i = 0, pp = portVector; i < MaxPorts; i++, pp++)
        {
        pp->status = 00026;
        pp->active = FALSE;
        pp->connFd = 0;
        pp->id     = i;
        }

    /*
    **  Create the listening socket
    */
    listenFd = netCreateListener(telnetPort);
#if defined(_WIN32)
    if (listenFd == INVALID_SOCKET)
#else
    if (listenFd == -1)
#endif
        {
        fprintf(stderr, "(tpmux  ) Can't listen on port %d\n", telnetPort);
        exit(1);
        }

    /*
    **  Print a friendly message.
    */
    printf("(tpmux  ) Two port MUX initialised on channel %o, telnet port %d.\n", channelNo, telnetPort);
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
void tpMuxShowStatus()
    {
    int       i;
    char      outBuf[200];
    PortParam *pp;

    if (listenFd <= 0) return;

    sprintf(outBuf, "    >   %-8s C%02o E%02o     ", "2pMux", mux->channel->id, mux->eqNo);
    opDisplay(outBuf);
    sprintf(outBuf, FMTNETSTATUS"\n", netGetLocalTcpAddress(listenFd), "", "async", "listening");
    opDisplay(outBuf);

    for (i = 0, pp = portVector; i < MaxPorts; i++, pp++)
        {
        if (pp->active && pp->connFd > 0)
            {
            sprintf(outBuf, "    >   %-8s         P%02o ", "2pMux", pp->id);
            opDisplay(outBuf);
            sprintf(outBuf, FMTNETSTATUS"\n", netGetLocalTcpAddress(pp->connFd), netGetPeerTcpAddress(pp->connFd), "async", "connected");
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
**  Purpose:        Check for I/O availability.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void tpMuxCheckIo(void)
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
    int            i;
    int            maxFd;
    int            n;
    int            optEnable = 1;
    PortParam      *pp;
    fd_set         readFds;
    struct timeval timeout;
    fd_set         writeFds;

    ioTurns = (ioTurns + 1) % IoTurnsPerPoll;
    if (ioTurns != 0)
        {
        return;
        }

    FD_ZERO(&readFds);
    FD_ZERO(&writeFds);
    maxFd = 0;

    for (i = 0, pp = portVector; i < MaxPorts; i++, pp++)
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
    if (listenFd >= 0)
        {
        FD_SET(listenFd, &readFds);
        if (listenFd > maxFd)
            {
            maxFd = listenFd;
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

    for (i = 0, pp = portVector; i < MaxPorts; i++, pp++)
        {
        if (pp->active)
            {
            if (FD_ISSET(pp->connFd, &readFds))
                {
                n = recv(pp->connFd, &pp->inBuffer[pp->inInIdx], InBufSize - pp->inInIdx, 0);
                if (n > 0)
                    {
                    pp->inInIdx += n;
                    }
                else
                    {
                    netCloseConnection(pp->connFd);
                    pp->active = FALSE;
                    }
                }
            if (FD_ISSET(pp->connFd, &writeFds) && (pp->outOutIdx < pp->outInIdx))
                {
                n = send(pp->connFd, &pp->outBuffer[pp->outOutIdx], pp->outInIdx - pp->outOutIdx, 0);
                if (n >= 0)
                    {
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
    if (listenFd >= 0 && FD_ISSET(listenFd, &readFds))
        {
        fromLen = sizeof(from);
        fd = accept(listenFd, (struct sockaddr *)&from, &fromLen);
        if (fd < 0) return;
        availablePort = NULL;
        for (i = 0, pp = portVector; i < MaxPorts; i++, pp++)
            {
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
#if DEBUG
            printf("(tpmux  ) Connection accepted on port %d\n", availablePort->id);
#endif
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
            send(fd, connectingMsg, strlen(connectingMsg), 0);
            }
        else
            {
            send(fd, noPortsMsg, strlen(noPortsMsg), 0);
            netCloseConnection(fd);
#if DEBUG
            puts("(tpmux  ) No free ports available");
#endif
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Execute function code on two-port mux.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        FcStatus
**
**------------------------------------------------------------------------*/
static FcStatus tpMuxFunc(PpWord funcCode)
    {
    int funcParam;

    funcParam = funcCode & 077;
    switch (funcCode & 07700)
        {
    default:
#if DEBUG
        printf("(tpmux  ) Function on tpm %04o declined\n", funcCode);
#endif

        return (FcDeclined);

    case FcTpmStatusSumary:
        break;

    case FcTpmReadChar:
        break;

    case FcTpmWriteChar:
        break;

    case FcTpmSetTerminal:
#if DEBUG
        printf("(tpmux  ) Set Terminal mode %03o (unit %d)\n", funcParam, activeDevice->selectedUnit);
#endif
        break;

    case FcTpmFlipDTR:
#if DEBUG
        printf("(tpmux  ) %s DTR (unit %d)\n", funcParam == 0 ? "Clear" : "Set", activeDevice->selectedUnit);
#endif
        break;

    case FcTpmFlipRTS:
#if DEBUG
        printf("(tpmux  ) %s RTS (unit %d)\n", funcParam == 0 ? "Clear" : "Set", activeDevice->selectedUnit);
#endif
        break;

    case FcTpmMasterClear:
        return (FcProcessed);

    case FcTpmDeSelect:
        activeDevice->selectedUnit = -1;

        return (FcProcessed);

    case FcTpmConPort:
        activeDevice->selectedUnit = 1 - (funcParam & 1);

        return (FcProcessed);
        }

    activeDevice->fcode = funcCode;

    return (FcAccepted);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Perform I/O on two-port mux.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void tpMuxIo(void)
    {
    PortParam *pp;

    tpMuxCheckIo();

    if (activeDevice->selectedUnit < 0)
        {
        return;
        }

    pp = (PortParam *)activeDevice->context[0] + activeDevice->selectedUnit;

    switch (activeDevice->fcode & 00700)
        {
    case FcTpmStatusSumary:
        if (!activeChannel->full)
            {
            if (pp->active && pp->inOutIdx < pp->inInIdx)
                {
                pp->status |= 00010;
                }
            else
                {
                pp->status &= ~00010;
                }

            activeChannel->data = pp->status;
            activeChannel->full = TRUE;
            }
        break;

    case FcTpmReadChar:
        if (!activeChannel->full && ((pp->status & 00010) != 0))
            {
            activeChannel->data = (PpWord)pp->inBuffer[pp->inOutIdx++] | 06000;
            activeChannel->full = TRUE;
            pp->status         &= ~00010;
            if (pp->inOutIdx >= pp->inInIdx)
                {
                pp->inOutIdx = 0;
                pp->inInIdx  = 0;
                }
#if DEBUG
            printf("(tpmux  ) read port %d -  %04o\n", pp->id, activeChannel->data);
#endif
            }
        break;

    case FcTpmWriteChar:
        if (activeChannel->full)
            {
            /*
            **  Output data.
            */
            activeChannel->full = FALSE;

            if (pp->active && pp->outInIdx < OutBufSize)
                {
                pp->outBuffer[pp->outInIdx++] = (u8)activeChannel->data & 0177;
#if DEBUG
                printf("(tpmux  ) write port %d - %04o\n", pp->id, activeChannel->data);
#endif
                }
            }
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
static void tpMuxActivate(void)
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
static void tpMuxDisconnect(void)
    {
    }

/*---------------------------  End Of File  ------------------------------*/
