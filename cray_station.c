/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2021, Kevin Jordan, Tom Hunter
**
**  Name: cray_station.c
**
**  Description:
**      Perform eumlation of Cray Station hardware interface. Cray supported
**      two types of station interfaces to its supercomputers:
**
**        1) Front end interface (FEI). This was a direct channel interface
**           between a station (e.g., Cyber mainframe) and a Cray front end.
**        2) NSC Hyperchannel interface. This was an interface from a
**           station to an NSC Hyperchannel networking device which, in
**           turn, provided connectivity to one or more Cray supercomputers.
**
**      This module implements the FEI interface.
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

#define DEBUG 0

/*
**  -------------
**  Include Files
**  -------------
*/
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#if defined(_WIN32)
#include <winsock.h>
#else
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif
#include <string.h>
#include "const.h"
#include "types.h"
#include "proto.h"

/*
**  -----------------
**  Private Constants
**  -----------------
*/

/*
**  Station function codes as defined in the Cray Station Driver PP (CSD).
**
**    PPOT     EQU    0000        FUNCTION FOR OUTPUT
**    PPIN     EQU    0100        FUNCTION FOR INPUT
**    PPST     EQU    0200        STATUS REQUEST
**    PPBF     EQU    0400        BAD FUNCTION TO SELECT AFTER PPMC
**    PPMC     EQU    0700        MASTER CLEAR INTERFACE
**
**  Station reply codes as defined in the Cray Station Driver PP (CSD).
**
**    SROC     EQU    0001        INTERFACE READY FOR OUTPUT
**    SRIC     EQU    0002        INTERFACE READY FOR INPUT
**    SRIB     EQU    0004        INTERFACE BUSY
**    SRPE     EQU    0010        TRANSMISSION PARITY ERROR
*/

#define FcCsFeiOutput           00000 /* not used by CSD */
#define FcCsFeiInput            00100 /* not used by CSD */
#define FcCsFeiStatus           00200
#define FcCsFeiBad              00400
#define FcCsFeiMasterClear      00700

#define RcCsFeiReadyForOutput   00001 /* Cray is sending data */
#define RcCsFeiReadyForInput    00002 /* Cray is receiving data - not used by CSD */
#define RcCsFeiBusy             00004 /* Interface is busy */
#define RcCsFeiParityError      00010

#define McLogon                 001     /* Login message code   */
#define McStart                 004     /* Start message code   */
#define McControl               011     /* Control message code */

/*
**  Misc constants.
*/
#define BytesPerLCP             48
#define ConnectionRetryInterval 60
#define PpWordsPerLCP           32

/*
**  Buffer sizes.
**
**  The Cray Station software specifies the maximum subsegment size to
**  be 3840 60-bit words, so set the maximum PP buffer size to 5x this
**  value plus some extra, and set the maximum byte buffer size to
**  coincide.
*/
#define MaxPpBuf                20000
#define MaxByteBuf              30000

/*
**  -----------------------
**  Private Macro Functions
**  -----------------------
*/
#if DEBUG
#define HexColumn(x) (3 * (x) + 4)
#define AsciiColumn(x) (HexColumn(16) + 2 + (x))
#define LogLineLength (AsciiColumn(16))
#endif

/*
**  -----------------------------------------
**  Private Typedef and Structure Definitions
**  -----------------------------------------
*/

/*
**  FEI data communication state.
*/
typedef enum {
    StCsFeiDisconnected = 0,
    StCsFeiConnecting,
    StCsFeiSendLCP,
    StCsFeiSendSubsegment,
    StCsFeiRecvLCPLen,
    StCsFeiRecvLCP1,
    StCsFeiRecvLCP2,
    StCsFeiRecvSubsegmentLen,
    StCsFeiRecvSubsegment1,
    StCsFeiRecvSubsegment2
    } FeiState;

/*
**  FEI byte-oriented I/O buffer
*/
typedef struct feiBuffer
    {
    u32 in;
    u32 out;
    u8  data[MaxByteBuf];
    } FeiBuffer;

/*
**  PP word-oriented I/O buffer
*/
typedef struct ppBuffer
    {
    u32    in;
    u32    out;
    PpWord data[MaxPpBuf];
    } PpBuffer;

/*
**  Key LCP (Link Control Package) parameters
*/
typedef struct feiLcpParams
    {
    char did[3];  // destination ID
    char sid[3];  // source ID
    u8  nssg;     // number of subsegments in segment
    u8  mn;       // message number
    u8  mc;       // message code
    u8  msc;      // message subcode
    u8  stn;      // stream number
    u32 sgn;      // segment number
    u32 sgbc;     // number of data bits in segment
    } FeiLcpParams;

/*
**  FEI parameter block.
*/
typedef struct feiParam
    {
    FeiState     state;
    time_t       nextConnectionAttempt;
    char        *serverName;
    struct sockaddr_in serverAddr;
#if defined(_WIN32)
    SOCKET       fd;
#else
    int          fd;
#endif
    FeiLcpParams lcpParams;
    int          subsegSize;
    FeiBuffer    inputBuffer;
    FeiBuffer    outputBuffer;
    PpBuffer     ppIoBuffer;
    } FeiParam;

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static void     csFeiActivate(void);
static void     csFeiCheckStatus(FeiParam *feip);
static void     csFeiCloseConnection(FeiParam *feip);
static void     csFeiDisconnect(void);
static FcStatus csFeiFunc(PpWord funcCode);
static void     csFeiIo(void);
static void     csFeiInitiateConnection(FeiParam *feip);
static void     csFeiPackPpBuffer(FeiParam *feip, int maxWords);
static void     csFeiReceiveData(FeiParam *feip);
static void     csFeiReset(FeiParam *feip);
static void     csFeiSendData(FeiParam *feip);
static bool     csFeiSetupConnection(FeiParam *feip);
static void     csFeiUnpackPpBuffer(FeiParam *feip);

#if DEBUG
static char     *csFeiFunc2String(PpWord funcCode);
static void     csFeiLogBytes(u8 *bytes, int len);
static void     csFeiLogFlush(void);
static void     csFeiLogLcpParams(FeiParam *feip);
static void     csFeiLogPpBuffer(FeiParam *feip);
#endif

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

#if DEBUG
static FILE *csFeiLog = NULL;
static char csFeiLogBuf[LogLineLength + 1];
static int  csFeiLogBytesCol = 0;
#endif

/*
**--------------------------------------------------------------------------
**
**  Public Functions
**
**--------------------------------------------------------------------------
*/
/*--------------------------------------------------------------------------
**  Purpose:        Initialise FEI interface.
**
**  Parameters:     Name        Description.
**                  eqNo        equipment number
**                  unitNo      number of the unit to initialise
**                  channelNo   channel number the device is attached to
**                  deviceName  optional TCP address of Cray emulator
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void csFeiInit(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName)
    {
    DevSlot *dp;
    struct hostent *hp;
    u16 serverPort;
    char *sp;
    FeiParam *feip;
    long value;

    if (deviceName == NULL)
        {
        fprintf(stderr, "Cray computer simulator connection information required for FEI on channel %o equipment %o unit %o\n",
          channelNo, eqNo, unitNo);
        exit(1);
        }

#if DEBUG
    if (csFeiLog == NULL)
        {
        csFeiLog = fopen("csfeilog.txt", "wt");
        csFeiLogFlush();
        }
#endif

    /*
    **  Attach device to channel.
    */
    dp = channelAttach(channelNo, eqNo, DtCsFei);

    /*
    **  Setup channel functions.
    */
    dp->activate = csFeiActivate;
    dp->disconnect = csFeiDisconnect;
    dp->func = csFeiFunc;
    dp->io = csFeiIo;
    dp->selectedUnit = unitNo;

    /*
    **  Allocate and initialize FEI context.
    */
    feip = calloc(1, sizeof(FeiParam));
    if (feip == NULL)
        {
        fprintf(stderr, "Failed to allocate Cray Station FEI context block\n");
        exit(1);
        }
    dp->controllerContext = feip;
    feip->state = StCsFeiDisconnected;
    feip->nextConnectionAttempt = 0;
    feip->fd = 0;
    csFeiReset(feip);

    /*
    **  Set up server connection
    */
    feip->serverName = NULL;
    serverPort = 0;
    sp = strchr(deviceName, ':');
    if (sp != NULL)
        {
        *sp++ = '\0';
        feip->serverName = (char *)malloc(strlen(deviceName) + 1);
        strcpy(feip->serverName, deviceName);
        value = strtol(sp, NULL, 10);
        if (value > 0 && value < 0x10000) serverPort = (u16)value;
        }
    if (feip->serverName == NULL || serverPort == 0)
        {
        fprintf(stderr,
            "Invalid Cray computer simulator connection specification for Cray Station FEI on channel %o equipment %o unit %o\n",
            channelNo, eqNo, unitNo);
        exit(1);
        }
    hp = gethostbyname(feip->serverName);
    if (hp == NULL)
        {
        fprintf(stderr, "Failed to lookup address of Cray computer simulator host %s\n", feip->serverName);
        exit(1);
        }
    feip->serverAddr.sin_family = AF_INET;
    memcpy(&feip->serverAddr.sin_addr.s_addr, (struct in_addr *)hp->h_addr_list[0], sizeof(struct in_addr));
    feip->serverAddr.sin_port = htons(serverPort);

    /*
    **  Print a friendly message.
    */
    printf("Cray Station FEI initialised on channel %o equipment %o unit %o for Cray computer simulator at %s:%d\n",
        channelNo, eqNo, unitNo, feip->serverName, serverPort);
    }

/*
**--------------------------------------------------------------------------
**
**  Private Functions
**
**--------------------------------------------------------------------------
*/

/*--------------------------------------------------------------------------
**  Purpose:        Handle channel activation.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void csFeiActivate(void)
    {
#if DEBUG
    fprintf(csFeiLog, "\n%010u PP:%02o CH:%02o P:%04o Activate",
        traceSequenceNo,
        activePpu->id,
        activeDevice->channel->id,
        activePpu->regP);
#endif
    //activeChannel->delayStatus = 5;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Process Cray Station FEI I/O and state transitions.
**
**  Parameters:     Name        Description.
**                  feip        pointer to FEI parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void csFeiCheckStatus(FeiParam *feip)
    {
    fd_set readFds;
    int readySockets;
    struct timeval timeout;
    fd_set writeFds;

    /*
    **  First, process possible connection in progress
    */
    if (feip->state == StCsFeiDisconnected)
        {
        csFeiInitiateConnection(feip);
        }
    else if (feip->fd > 0 && feip->state == StCsFeiConnecting)
        {
        FD_ZERO(&writeFds);
        FD_SET(feip->fd, &writeFds);
        timeout.tv_sec = 0;
        timeout.tv_usec = 0;
        readySockets = select(feip->fd + 1, NULL, &writeFds, NULL, &timeout);
        if (readySockets > 0 && FD_ISSET(feip->fd, &writeFds))
            {
            csFeiReset(feip);
            if (csFeiSetupConnection(feip)) feip->state = StCsFeiSendLCP;
            }
        }

    /*
    **  Second, process normal I/O for connected FEI
    */
    if (feip->fd > 0 && feip->state > StCsFeiConnecting)
        {
        FD_ZERO(&readFds);
        FD_ZERO(&writeFds);
        FD_SET(feip->fd, &readFds);
        if (feip->outputBuffer.out < feip->outputBuffer.in)
            {
            FD_SET(feip->fd, &writeFds);
            }
        timeout.tv_sec = 0;
        timeout.tv_usec = 0;
        readySockets = select(feip->fd + 1, &readFds, &writeFds, NULL, &timeout);
        if (readySockets > 0)
            {
            if (FD_ISSET(feip->fd, &readFds))
                {
                csFeiReceiveData(feip);
                }
            if (feip->fd > 0 && FD_ISSET(feip->fd, &writeFds))
                {
                csFeiSendData(feip);
                }
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Close FEI connection
**
**  Parameters:     Name        Description.
**                  feip        pointer to FEI parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void csFeiCloseConnection(FeiParam *feip)
    {
#if DEBUG
    fprintf(csFeiLog, "\n%010u Close connection on socket %d to %s:%u", traceSequenceNo,
        feip->fd, feip->serverName, ntohs(feip->serverAddr.sin_port));
#endif
#if defined(_WIN32)
    closesocket(feip->fd);
#else
    close(feip->fd);
#endif
    feip->fd = 0;
    feip->state = StCsFeiDisconnected;
    feip->nextConnectionAttempt = getSeconds() + (time_t)ConnectionRetryInterval;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Handle disconnecting of channel.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void csFeiDisconnect(void)
    {
#if DEBUG
    fprintf(csFeiLog, "\n%010u PP:%02o CH:%02o P:%04o Disconnect",
        traceSequenceNo,
        activePpu->id,
        activeDevice->channel->id,
        activePpu->regP);
#endif
    /*
    **  Abort pending device disconnects - the PP is doing the disconnect.
    */
    activeChannel->delayDisconnect = 0;
    activeChannel->discAfterInput = FALSE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Execute function code on FEI link device.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        FcStatus
**
**------------------------------------------------------------------------*/
static FcStatus csFeiFunc(PpWord funcCode)
    {
    FeiParam *feip;

#if DEBUG
    fprintf(csFeiLog, "\n%010u PP:%02o CH:%02o P:%04o f:%04o T:%-25s",
        traceSequenceNo,
        activePpu->id,
        activeDevice->channel->id,
        activePpu->regP,
        funcCode,
        csFeiFunc2String(funcCode));
#endif

    /*
    **  Reset function code.
    */
    activeDevice->fcode = 0;
    activeChannel->full = FALSE;

    /*
    **  Process FEI function.
    */
    feip = (FeiParam *)activeDevice->controllerContext;
    if (feip == NULL) return(FcDeclined);
    switch (funcCode)
        {
    case FcCsFeiStatus:
        activeDevice->fcode = funcCode;
        return(FcAccepted);

    case FcCsFeiBad:
        return(FcProcessed);

    case FcCsFeiMasterClear:
        if (feip->state > StCsFeiConnecting)
            {
            csFeiReset(feip);
            feip->state = StCsFeiSendLCP;
            }
        return(FcProcessed);

    default:
#if DEBUG
        fprintf(csFeiLog, " FUNC not implemented & declined!");
#endif
        return(FcDeclined);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Initiate a TCP connection to a Cray computer simulator
**
**  Parameters:     Name        Description.
**                  feip        pointer to FEI parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void csFeiInitiateConnection(FeiParam *feip)
    {
    time_t currentTime;
#if defined(_WIN32)
    SOCKET fd;
    u_long blockEnable = 1;
#else
    int fd;
    int optVal;
#endif
    int optEnable = 1;
    int rc;

    currentTime = getSeconds();
    if (feip->nextConnectionAttempt > currentTime) return;
    feip->nextConnectionAttempt = currentTime + ConnectionRetryInterval;

    fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#if defined(_WIN32)
    //  -------------------------------
    //       Windows socket setup
    //  -------------------------------
    if (fd == INVALID_SOCKET)
        {
        fprintf(stderr, "FEI: Failed to create socket for host: %s\n", feip->serverName);
        return;
        }
    if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (void *)&optEnable, sizeof(optEnable)) < 0)
        {
#if DEBUG
        fprintf(csFeiLog, "\n%010u Failed to set KEEPALIVE option on socket %d to %s:%u, %s", traceSequenceNo,
            fd, feip->serverName, ntohs(feip->serverAddr.sin_port), strerror(errno));
#endif
        closesocket(fd);
        return;
        }
    if (ioctlsocket(fd, FIONBIO, &blockEnable) < 0)
        {
#if DEBUG
        fprintf(csFeiLog, "\n%010u Failed to set non-blocking I/O on socket %d to %s:%u, %s", traceSequenceNo,
            fd, feip->serverName, ntohs(feip->serverAddr.sin_port), strerror(errno));
#endif
        closesocket(fd);
        return;
        }
    rc = connect(fd, (struct sockaddr *)&feip->serverAddr, sizeof(feip->serverAddr));
    if (rc == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)
        {
#if DEBUG
        fprintf(csFeiLog, "\n%010u Failed to connect to %s:%u, %s", traceSequenceNo,
            feip->serverName, ntohs(feip->serverAddr.sin_port), strerror(errno));
#endif
        closesocket(fd);
        }
    else // connection in progress
        {
        feip->fd = fd;
        feip->state = StCsFeiConnecting;
#if DEBUG
        fprintf(csFeiLog, "\n%010u Initiated connection on socket %d to %s:%u", traceSequenceNo,
            feip->fd, feip->serverName, ntohs(feip->serverAddr.sin_port));
#endif
        }
#else
    //  -------------------------------
    //     non-Windows socket setup
    //  -------------------------------
    if (fd < 0)
        {
        fprintf(stderr, "FEI: Failed to create socket for host: %s\n", feip->serverName);
        return;
        }
    if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (void *)&optEnable, sizeof(optEnable)) < 0)
        {
#if DEBUG
        fprintf(csFeiLog, "\n%010u Failed to set KEEPALIVE option on socket %d to %s:%u, %s", traceSequenceNo,
            fd, feip->serverName, ntohs(feip->serverAddr.sin_port), strerror(errno));
#endif
        close(fd);
        return;
        }
    if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0)
        {
#if DEBUG
        fprintf(csFeiLog, "\n%010u Failed to set non-blocking I/O on socket %d to %s:%u, %s", traceSequenceNo,
            fd, feip->serverName, ntohs(feip->serverAddr.sin_port), strerror(errno));
#endif
        close(fd);
        return;
        }
    rc = connect(fd, (struct sockaddr *)&feip->serverAddr, sizeof(feip->serverAddr));
    if (rc < 0 && errno != EINPROGRESS)
        {
#if DEBUG
        fprintf(csFeiLog, "\n%010u Failed to connect to %s:%u, %s", traceSequenceNo,
            feip->serverName, ntohs(feip->serverAddr.sin_port), strerror(errno));
#endif
        close(fd);
        }
    else // connection in progress
        {
        feip->fd = fd;
        feip->state = StCsFeiConnecting;
#if DEBUG
        fprintf(csFeiLog, "\n%010u Initiated connection on socket %d to %s:%u", traceSequenceNo,
            feip->fd, feip->serverName, ntohs(feip->serverAddr.sin_port));
#endif
        }
#endif // not _WIN32
    }

/*--------------------------------------------------------------------------
**  Purpose:        Perform I/O on FEI.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void csFeiIo(void)
    {
    int di;
    FeiParam *feip;
    int i;
    int recordLength;

    /*
    **  Perform actual I/O according to function issued.
    */
    feip = (FeiParam *)activeDevice->controllerContext;
    if (feip == NULL) return;

    csFeiCheckStatus(feip);

    switch (activeDevice->fcode)
        {
    case FcCsFeiStatus:
        if (activeChannel->full == FALSE)
            {
            activeChannel->data = 0;
            switch (feip->state)
                {
            case StCsFeiSendLCP:
            case StCsFeiSendSubsegment:
                if (feip->outputBuffer.out < feip->outputBuffer.in)
                    {
                    activeChannel->data = RcCsFeiReadyForInput;
                    }
                break;
            case StCsFeiRecvLCPLen:
            case StCsFeiRecvLCP1:
            case StCsFeiRecvLCP2:
            case StCsFeiRecvSubsegmentLen:
            case StCsFeiRecvSubsegment1:
            case StCsFeiRecvSubsegment2:
                if (feip->inputBuffer.out < feip->inputBuffer.in)
                    {
                    activeChannel->data = RcCsFeiReadyForOutput;
                    }
                break;
            default:
                break;
                }
            activeDevice->fcode = 0;
            activeChannel->discAfterInput = FALSE;
            activeChannel->full = TRUE;
#if DEBUG
            fprintf(csFeiLog, " %04o", activeChannel->data);
#endif
            }
        break;

    case 0: /* I/O typically after FcCsFeiStatus function processed */
        switch (feip->state)
            {
        case StCsFeiSendLCP:
            if (activeChannel->full == TRUE)
                {
                feip->ppIoBuffer.data[feip->ppIoBuffer.in++] = activeChannel->data;
                activeChannel->full = FALSE;
                if (feip->ppIoBuffer.in - feip->ppIoBuffer.out >= PpWordsPerLCP) // full LCP received from Cyber
                    {
#if DEBUG
                    csFeiLogPpBuffer(feip);
#endif
                    csFeiUnpackPpBuffer(feip);
                    /*
                    **  Deserialize key fields of LCP
                    */
                    di = 4;
                    memcpy(feip->lcpParams.did, &feip->outputBuffer.data[di], 2);
                    feip->lcpParams.did[2] = '\0';
                    di += 2;
                    memcpy(feip->lcpParams.sid, &feip->outputBuffer.data[di], 2);
                    feip->lcpParams.sid[2] = '\0';
                    di += 2;
                    feip->lcpParams.nssg = feip->outputBuffer.data[di++];
                    feip->lcpParams.mn   = feip->outputBuffer.data[di++];
                    feip->lcpParams.mc   = feip->outputBuffer.data[di++];
                    feip->lcpParams.msc  = feip->outputBuffer.data[di++];
                    feip->lcpParams.stn  = feip->outputBuffer.data[di++] & 0x0f;
                    feip->lcpParams.sgn  = (feip->outputBuffer.data[di  ] << 16)
                                         | (feip->outputBuffer.data[di+1] <<  8)
                                         |  feip->outputBuffer.data[di+2];
                    di += 3;
                    feip->lcpParams.sgbc = (feip->outputBuffer.data[di  ] << 24)
                                         | (feip->outputBuffer.data[di+1] << 16)
                                         | (feip->outputBuffer.data[di+2] <<  8)
                                         |  feip->outputBuffer.data[di+3];
#if DEBUG
                    csFeiLogLcpParams(feip);
#endif
                    feip->ppIoBuffer.out = feip->ppIoBuffer.in = 0;
                    csFeiSendData(feip);
                    if (feip->lcpParams.mc == McLogon)
                        {
                        feip->subsegSize = 32; // # of PP words in logon message
                        }
                    feip->state = (feip->lcpParams.nssg > 0) ? StCsFeiSendSubsegment : StCsFeiRecvLCPLen;
                    }
                }
            break;
        case StCsFeiSendSubsegment:
            if (activeChannel->full == TRUE)
                {
                feip->ppIoBuffer.data[feip->ppIoBuffer.in++] = activeChannel->data;
                activeChannel->full = FALSE;
                if (feip->ppIoBuffer.in - feip->ppIoBuffer.out >= feip->subsegSize) // full subsegment received from Cyber
                    {
#if DEBUG
                    csFeiLogPpBuffer(feip);
#endif
                    csFeiUnpackPpBuffer(feip);
                    feip->ppIoBuffer.out = feip->ppIoBuffer.in = 0;
                    if (feip->lcpParams.mc == McLogon) // logon message
                        {
                        // Extract subsegment size from logon message
                        feip->subsegSize = (((feip->outputBuffer.data[10] << 8) | feip->outputBuffer.data[11]) * 64) / 12;
#if DEBUG
                        fprintf(csFeiLog, "\n%010u SSGZ: %d PP words (%d 64-bit words), VARS: %s", traceSequenceNo, feip->subsegSize,
                            (feip->outputBuffer.data[10] << 8) | feip->outputBuffer.data[11],
                            ((feip->outputBuffer.data[13] & 0x20) == 0) ? "no" : "yes");
#endif
                        }
                    csFeiSendData(feip);
                    feip->lcpParams.nssg -= 1;
                    if (feip->lcpParams.nssg <= 0) feip->state = StCsFeiRecvLCPLen;
                    }
                }
            break;
        case StCsFeiRecvLCPLen:
            if (feip->inputBuffer.in - feip->inputBuffer.out < 4)
                {
                break; // continue waiting for receipt of full PDU length value
                }
            di = feip->inputBuffer.out;
            activeDevice->recordLength = (feip->inputBuffer.data[di  ] << 24)
                                       | (feip->inputBuffer.data[di+1] << 16)
                                       | (feip->inputBuffer.data[di+2] <<  8)
                                       |  feip->inputBuffer.data[di+3];
            feip->state = StCsFeiRecvLCP1;
            feip->inputBuffer.out += 4;
            // fall through
        case StCsFeiRecvLCP1:
            if (feip->inputBuffer.in - feip->inputBuffer.out < activeDevice->recordLength)
                {
                break; // continue waiting for receipt of full LCP
                }
            /*
            **  Full LCP received. Deserialize the key fields.
            */
            di = feip->inputBuffer.out;
            memcpy(feip->lcpParams.did, &feip->inputBuffer.data[di], 2);
            feip->lcpParams.did[2] = '\0';
            di += 2;
            memcpy(feip->lcpParams.sid, &feip->inputBuffer.data[di], 2);
            feip->lcpParams.sid[2] = '\0';
            di += 2;
            feip->lcpParams.nssg = feip->inputBuffer.data[di++];
            feip->lcpParams.mn   = feip->inputBuffer.data[di++];
            feip->lcpParams.mc   = feip->inputBuffer.data[di++];
            feip->lcpParams.msc  = feip->inputBuffer.data[di++];
            feip->lcpParams.stn  = feip->inputBuffer.data[di++] & 0x0f;
            feip->lcpParams.sgn  = (feip->inputBuffer.data[di  ] << 16)
                                 | (feip->inputBuffer.data[di+1] <<  8)
                                 |  feip->inputBuffer.data[di+2];
            di += 3;
            feip->lcpParams.sgbc = (feip->inputBuffer.data[di  ] << 24)
                                 | (feip->inputBuffer.data[di+1] << 16)
                                 | (feip->inputBuffer.data[di+2] <<  8)
                                 |  feip->inputBuffer.data[di+3];
#if DEBUG
            csFeiLogLcpParams(feip);
#endif
            /*
            **  Pack the LCP into the PP I/O buffer, then move any
            **  bytes remaining in the input buffer to the beginning
            **  of the buffer.
            */
            csFeiPackPpBuffer(feip, PpWordsPerLCP);
#if DEBUG
            csFeiLogPpBuffer(feip);
#endif
            i = 0;
            while (feip->inputBuffer.out < feip->inputBuffer.in)
               {
               feip->inputBuffer.data[i++] = feip->inputBuffer.data[feip->inputBuffer.out++];
               }
            feip->inputBuffer.out = 0;
            feip->inputBuffer.in = i;
            feip->state = StCsFeiRecvLCP2;
            activeDevice->recordLength = PpWordsPerLCP;
            // fall through
        case StCsFeiRecvLCP2:
            if (activeChannel->full == FALSE)
                {
                activeChannel->data = feip->ppIoBuffer.data[feip->ppIoBuffer.out++];
                activeChannel->full = TRUE;
                activeDevice->recordLength -= 1;
                if (feip->ppIoBuffer.out >= feip->ppIoBuffer.in)
                    {
                    feip->ppIoBuffer.out = feip->ppIoBuffer.in = 0;
                    }
                if (activeDevice->recordLength <= 0)
                    {
                    activeChannel->discAfterInput = TRUE;
                    if (feip->lcpParams.nssg > 0)
                        {
                        feip->state = StCsFeiRecvSubsegmentLen;
                        }
                    else
                        {
                        feip->state = StCsFeiSendLCP;
                        }
                    }
                }
            break;
        case StCsFeiRecvSubsegmentLen:
            if (feip->inputBuffer.in - feip->inputBuffer.out < 4)
                {
                break; // continue waiting for receipt of full PDU length value
                }
            di = feip->inputBuffer.out;
            activeDevice->recordLength = (feip->inputBuffer.data[di  ] << 24)
                                       | (feip->inputBuffer.data[di+1] << 16)
                                       | (feip->inputBuffer.data[di+2] <<  8)
                                       |  feip->inputBuffer.data[di+3];
            feip->state = StCsFeiRecvSubsegment1;
            feip->inputBuffer.out += 4;
            // fall through
        case StCsFeiRecvSubsegment1:
            if (feip->inputBuffer.in - feip->inputBuffer.out < activeDevice->recordLength)
                {
                break; // continue waiting for receipt of full subsegment
                }
            /*
            **  Pack the subsegment into the PP I/O buffer, then move any
            **  bytes remaining in the input buffer to the beginning of
            **  the buffer.
            */
            recordLength = (activeDevice->recordLength * 8) / 12;
            csFeiPackPpBuffer(feip, recordLength);
#if DEBUG
            fprintf(csFeiLog, "\n%010u Received subsegment (%d bytes, %d PP words) from %s:%u", traceSequenceNo,
                activeDevice->recordLength, recordLength, feip->serverName, ntohs(feip->serverAddr.sin_port));
            csFeiLogPpBuffer(feip);
#endif
            i = 0;
            while (feip->inputBuffer.out < feip->inputBuffer.in)
               {
               feip->inputBuffer.data[i++] = feip->inputBuffer.data[feip->inputBuffer.out++];
               }
            feip->inputBuffer.out = 0;
            feip->inputBuffer.in = i;
            feip->state = StCsFeiRecvSubsegment2;
            activeDevice->recordLength = recordLength;
            // fall through
        case StCsFeiRecvSubsegment2:
            if (activeChannel->full == FALSE)
                {
                activeChannel->data = feip->ppIoBuffer.data[feip->ppIoBuffer.out++];
                activeChannel->full = TRUE;
                activeDevice->recordLength -= 1;
                if (feip->ppIoBuffer.out >= feip->ppIoBuffer.in)
                    {
                    feip->ppIoBuffer.out = feip->ppIoBuffer.in = 0;
                    }
                if (activeDevice->recordLength <= 0)
                    {
                    activeChannel->discAfterInput = TRUE;
                    feip->lcpParams.nssg -= 1;
                    if (feip->lcpParams.nssg > 0)
                        {
                        feip->state = StCsFeiRecvSubsegmentLen;
                        }
                    else
                        {
                        feip->state = StCsFeiSendLCP;
                        }
                    }
                }
            break;
        case StCsFeiDisconnected:
        case StCsFeiConnecting:
            if (activeChannel->full == TRUE)
                {
                activeChannel->full = FALSE;
                }
            else
                {
                activeChannel->data = 0;
                activeChannel->full = TRUE;
                activeChannel->discAfterInput = TRUE;
                }
            break;
        default:
            logError(LogErrorLocation, "channel %02o - invalid state: %d", activeChannel->id, feip->state);
            break;
            }
        break;

    default:
        logError(LogErrorLocation, "channel %02o - unsupported function code: %04o",
            activeChannel->id, activeDevice->fcode);
        break;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Pack the byte-oriented input buffer into the word-oriented
**                  PP buffer.
**
**  Parameters:     Name        Description.
**                  feip        pointer to FEI parameters
**                  maxWords    maximum number of words to pack
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void csFeiPackPpBuffer(FeiParam *feip, int maxWords)
    {
    u8 b1, b2, b3;

    while (feip->inputBuffer.out + 2 < feip->inputBuffer.in
           && feip->ppIoBuffer.in - feip->ppIoBuffer.out + 2 <= maxWords)
        {
        b1 = feip->inputBuffer.data[feip->inputBuffer.out++];
        b2 = feip->inputBuffer.data[feip->inputBuffer.out++];
        b3 = feip->inputBuffer.data[feip->inputBuffer.out++];
        feip->ppIoBuffer.data[feip->ppIoBuffer.in++] = (b1 << 4) | (b2 >> 4);
        feip->ppIoBuffer.data[feip->ppIoBuffer.in++] = ((b2 & 0x0f) << 8) | b3;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Receive data from a Cray computer simulator
**
**  Parameters:     Name        Description.
**                  feip        pointer to FEI parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void csFeiReceiveData(FeiParam *feip)
    {
	int n;

    n = recv(feip->fd, &feip->inputBuffer.data[feip->inputBuffer.in], sizeof(feip->inputBuffer.data) - feip->inputBuffer.in, 0);
    if (n <= 0)
        {
#if DEBUG
        fprintf(csFeiLog, "\n%010u Disconnected on socket %d from %s:%u, %s", traceSequenceNo,
            feip->fd, feip->serverName, ntohs(feip->serverAddr.sin_port), (n == 0) ? "end of stream" : strerror(errno));
#endif
        csFeiCloseConnection(feip);
        }
    else
        {
#if DEBUG
        fprintf(csFeiLog, "\n%010u Received %d bytes on socket %d from %s:%u\n", traceSequenceNo, n,
            feip->fd, feip->serverName, ntohs(feip->serverAddr.sin_port));
        csFeiLogBytes(&feip->inputBuffer.data[feip->inputBuffer.in], n);
        csFeiLogFlush();
#endif
        if (feip->state >= StCsFeiRecvLCPLen)
            {
            feip->inputBuffer.in += n;
            }
#if DEBUG
        else
            {
            fprintf(csFeiLog, "\n%010u Unexpected data discarded in state %d", traceSequenceNo, feip->state);
            }
#endif
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Reset FEI I/O buffer pointers and indices.
**
**  Parameters:     Name        Description.
**                  feip        pointer to FEI parameters
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
static void csFeiReset(FeiParam *feip)
    {
    feip->inputBuffer.out  = feip->inputBuffer.in = 0;
    feip->outputBuffer.out = feip->outputBuffer.in = 0;
    feip->ppIoBuffer.out   = feip->ppIoBuffer.in = 0;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Send a message to a Cray computer simulator
**
**  Parameters:     Name        Description.
**                  feip        pointer to FEI parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void csFeiSendData(FeiParam *feip)
    {
    int len;
    int n;

    len = feip->outputBuffer.in - feip->outputBuffer.out;
    n = send(feip->fd, &feip->outputBuffer.data[feip->outputBuffer.out], len, 0);
    if (n > 0)
        {
#if DEBUG
        fprintf(csFeiLog, "\n%010u Sent %d bytes on socket %d to %s:%u\n", traceSequenceNo, n,
            feip->fd, feip->serverName, ntohs(feip->serverAddr.sin_port));
        csFeiLogBytes(&feip->outputBuffer.data[feip->outputBuffer.out], n);
        csFeiLogFlush();
#endif
        feip->outputBuffer.out += n;
        if (feip->outputBuffer.out >= feip->outputBuffer.in)
            {
            feip->outputBuffer.out = feip->outputBuffer.in = 0;
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Set up a completed TCP connection
**
**  Parameters:     Name        Description.
**                  feip        pointer to FEI parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/

static bool csFeiSetupConnection(FeiParam *feip)
    {
#if defined(_WIN32)
    int optLen;
    int optVal;
#else
    socklen_t optLen;
    int optVal;
#endif
    int rc;

#if defined(_WIN32)
    optLen = sizeof(optVal);
    rc = getsockopt(feip->fd, SOL_SOCKET, SO_ERROR, (char *)&optVal, &optLen);
#else
    optLen = (socklen_t)sizeof(optVal);
    rc = getsockopt(feip->fd, SOL_SOCKET, SO_ERROR, &optVal, &optLen);
#endif
    if (rc < 0)
        {
#if DEBUG
        fprintf(csFeiLog, "\n%010u Failed to query socket options on socket %d for %s:%u, %s", traceSequenceNo,
            feip->fd, feip->serverName, ntohs(feip->serverAddr.sin_port), strerror(errno));
#endif
        csFeiCloseConnection(feip);
        return FALSE;
        }
    else if (optVal != 0) // connection failed
        {
#if DEBUG
        fprintf(csFeiLog, "\n%010u Failed to connect on socket %d to %s:%u, %s", traceSequenceNo,
            feip->fd, feip->serverName, ntohs(feip->serverAddr.sin_port), strerror(optVal));
#endif
        csFeiCloseConnection(feip);
        return FALSE;
        }
    else
        {
#if DEBUG
        fprintf(csFeiLog, "\n%010u Connected on socket %d to %s:%u", traceSequenceNo,
            feip->fd, feip->serverName, ntohs(feip->serverAddr.sin_port));
#endif
        return TRUE;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Unpack the packed PP word buffer to the byte-oriented
**                  output buffer.
**
**  Parameters:     Name        Description.
**                  feip        pointer to FEI parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/

static void csFeiUnpackPpBuffer(FeiParam *feip)
    {
    int in;
    int pduLen;
    PpWord word1;
    PpWord word2;

    in = feip->outputBuffer.in; // save position at which to insert PDU length
    feip->outputBuffer.in += 4;

    while (feip->ppIoBuffer.out < feip->ppIoBuffer.in)
        {
        word1 = feip->ppIoBuffer.data[feip->ppIoBuffer.out++];
        feip->outputBuffer.data[feip->outputBuffer.in++] = word1 >> 4;
        if (feip->ppIoBuffer.out >= feip->ppIoBuffer.in) break;
        word2 = feip->ppIoBuffer.data[feip->ppIoBuffer.out++];
        feip->outputBuffer.data[feip->outputBuffer.in++] = ((word1 & 0x0f) << 4) | (word2 >> 8);
        feip->outputBuffer.data[feip->outputBuffer.in++] = word2 & 0xff;
        }

    pduLen = feip->outputBuffer.in - (in + 4);

    feip->outputBuffer.data[in++] = (pduLen >> 24) & 0xff;
    feip->outputBuffer.data[in++] = (pduLen >> 16) & 0xff;
    feip->outputBuffer.data[in++] = (pduLen >>  8) & 0xff;
    feip->outputBuffer.data[in  ] =  pduLen & 0xff;
    }

#if DEBUG
/*--------------------------------------------------------------------------
**  Purpose:        Convert function code to string.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        String equivalent of function code.
**
**------------------------------------------------------------------------*/
static char *csFeiFunc2String(PpWord funcCode)
    {
    static char buf[30];
    switch(funcCode)
        {
    case FcCsFeiOutput      : return "Output";
    case FcCsFeiInput       : return "Input";
    case FcCsFeiStatus      : return "Status";
    case FcCsFeiBad         : return "BadRequest";
    case FcCsFeiMasterClear : return "MasterClear";
        }
    sprintf(buf, "UNKNOWN: %04o", funcCode);
    return(buf);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Flush incomplete data line
**
**  Parameters:     Name        Description.
**
**  Returns:        nothing
**
**------------------------------------------------------------------------*/
static void csFeiLogFlush(void)
    {
    if (csFeiLogBytesCol > 0)
        {
        fputs(csFeiLogBuf, csFeiLog);
        fputc('\n', csFeiLog);
        fflush(csFeiLog);
        }
    csFeiLogBytesCol = 0;
    memset(csFeiLogBuf, ' ', LogLineLength);
    csFeiLogBuf[LogLineLength] = '\0';
    }

/*--------------------------------------------------------------------------
**  Purpose:        Log a sequence of ASCII bytes
**
**  Parameters:     Name        Description.
**                  bytes       pointer to sequence of bytes
**                  len         length of the sequence
**
**  Returns:        nothing
**
**------------------------------------------------------------------------*/
static void csFeiLogBytes(u8 *bytes, int len)
    {
    u8 ac;
    int ascCol;
    u8 b;
    char hex[3];
    int hexCol;
    int i;

    ascCol = AsciiColumn(csFeiLogBytesCol);
    hexCol = HexColumn(csFeiLogBytesCol);

    for (i = 0; i < len; i++)
        {
        b = bytes[i];
        ac = b;
        if (ac < 0x20 || ac >= 0x7f)
            {
            ac = '.';
            }
        sprintf(hex, "%02x", b);
        memcpy(csFeiLogBuf + hexCol, hex, 2);
        hexCol += 3;
        csFeiLogBuf[ascCol++] = ac;
        if (++csFeiLogBytesCol >= 16)
            {
            csFeiLogFlush();
            ascCol = AsciiColumn(csFeiLogBytesCol);
            hexCol = HexColumn(csFeiLogBytesCol);
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Log deserialized LCP parameters
**
**  Parameters:     Name        Description.
**                  feip        pointer to FEI parameters
**
**  Returns:        nothing
**
**------------------------------------------------------------------------*/
static void csFeiLogLcpParams(FeiParam *feip)
    {
    fprintf(csFeiLog, "\n%010u %s %s:%u", traceSequenceNo,
        (feip->state >= StCsFeiRecvLCPLen) ? "Received LCP from" : "Sent LCP to",
        feip->serverName, ntohs(feip->serverAddr.sin_port));
    fprintf(csFeiLog, "\n                DID: %s   SID: %s  NSSG: %02x    MN: %02x    MC: %02x   MSC: %02x",
        feip->lcpParams.did, feip->lcpParams.sid, feip->lcpParams.nssg, feip->lcpParams.mn, feip->lcpParams.mc, feip->lcpParams.msc);
    fprintf(csFeiLog, "\n                STN: %02x   SGN: %06x        SGBC: %08x",
        feip->lcpParams.stn, feip->lcpParams.sgn, feip->lcpParams.sgbc);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Log current contents of PP buffer
**
**  Parameters:     Name        Description.
**                  feip        pointer to FEI parameters
**
**  Returns:        nothing
**
**------------------------------------------------------------------------*/
static void csFeiLogPpBuffer(FeiParam *feip)
    {
    char octal[5];
    PpWord word;
    int i;
    int n;

    for (i = feip->ppIoBuffer.out, n = 0; i < feip->ppIoBuffer.in; i++, n++)
        {
        if (n % 5 == 0) fputs("\n    ", csFeiLog);
        fprintf(csFeiLog, " %04o", feip->ppIoBuffer.data[i]);
        }
    }

#endif // DEBUG

/*---------------------------  End Of File  ------------------------------*/

