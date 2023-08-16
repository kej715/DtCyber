/*--------------------------------------------------------------------------
**
**  Copyright (c) 2019, Kevin Jordan, Tom Hunter
**
**  Name: npu_nje.c
**
**  Description:
**      Perform emulation of the NJE TIP in an NPU consisting of a
**      CDC 2550 HCP running CCP.  The NJE TIP is used by the NJF
**      application.  NJF enables a NOS system to exchange batch jobs
**      and output files with other computer systems implementing
**      the IBM NJE protocol, typically IBM mainframes running
**      MVS with JES2/JES3 or CMS with RSCS.
**
**      On the network-facing side, this TIP implements the NJE/TCP
**      protocol, as specified in IBM'S specification, "Network Job
**      Entry (NJE) Formats and Protocols", available here:
**
**        https://www-01.ibm.com/servers/resourcelink/svc00100.nsf/pages/zosv2r3sa320988/$file/hasa600_v2r3.pdf
**
**      and the original VMNET protocol definition, available here:
**
**        http://www.nic.funet.fi/pub/netinfo/CREN/brfc0002.text
**
**      This NJE/TCP implementation is fully compatible and interoperable
**      with implementations in the Hercules IBM mainframe emulator
**      as well as Funetnje (an NJE/TCP implementation for Unix/Linux)
**      and JANET (an NJE/TCP implementation for VAX/VMS).
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

#define DEBUG    1

/*
**  -------------
**  Include Files
**  -------------
*/
#if defined(_WIN32)
#include <Windows.h>
#else
#include <sys/time.h>
#endif

#if defined(__FreeBSD__)
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif 

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include "const.h"
#include "types.h"
#include "proto.h"
#include "npu.h"

#if defined(__APPLE__)
#include <execinfo.h>
#endif


/*
**  -----------------
**  Private Constants
**  -----------------
*/

#define MaxRetries                   8
#define MaxUplineBlockSize           640
#define MaxWaitTime                  15

/*
**  Special ASCII characters used by NAM protocol
*/
#define US                           0x1f

/*
**  Special EBCDIC characters used by NJE protocol
*/
#define SOH                          0x01
#define STX                          0x02
#define DLE                          0x10
#define ENQ                          0x2d
#define SYN                          0x32
#define NAK                          0x3d
#define ACK0                         0x70

#define EbcdicBlank                  0x40

/*
**  NJE Record Control Block (RCB) codes
*/
#define RCB_RTI                      0x90
#define RCB_PTI                      0xa0
#define RCB_Deny                     0xb0
#define RCB_TransComplete            0xc0
#define RCB_RTR                      0xd0
#define RCB_SeqErr                   0xe0
#define RCB_GCR                      0xf0
#define RCB_NJFTIPCommand            0xff

/*
**  RCB offsets (from IBM "Network Job Entry (NJE) Formats and Protocols")
*/
#define NCCRCB                       0x00
#define NCCSRCB                      0x01
#define NCCIDL                       0x02
#define NCCINODE                     0x03
#define NCCIQUAL                     0x0b
#define NCCIEVNT                     0x0c
#define NCCIREST                     0x10
#define NCCIBFSZ                     0x12
#define NCCILPAS                     0x14
#define NCCIMPAS                     0x1c
#define NCCIFLG                      0x24

/*
**  NJE Secondary Record Control Block (SRCB) codes
*/
#define SRCB_Signoff                 0xc2
#define SRCB_InitialSignon           0xc9
#define SRCB_RespSignon              0xd1
#define SRCB_ResetSignon             0xd2
#define SRCB_AcceptSignon            0xd3
#define SRCB_AddConnection           0xd4
#define SRCB_DeleteConnection        0xd5

#define SRCB_CMDXBZ                  0x00 // NJF TIP command: Set Transmission Block Size
#define SRCB_CMDABT                  0x01 // NJF TIP command: Abort Transmitter

/*
**  Internal NJE status and error codes
*/
#define NjeStatusSYN_NAK             4
#define NjeStatusDLE_ACK0            3
#define NjeStatusSOH_ENQ             2
#define NjeStatusNothingUploaded     1
#define NjeStatusOk                  0
#define NjeErrBlockTooShort          -1
#define NjeErrBlockTooLong           -2
#define NjeErrBadLeader              -3
#define NjeErrBadBCB                 -4
#define NjeErrBadBSN                 -5
#define NjeErrBadFCS                 -6
#define NjeErrBadRCB                 -7
#define NjeErrBadSRCB                -8
#define NjeErrBadSCB                 -9
#define NjeErrRecordTooLong          -10
#define NjeErrTooManyRetries         -11
#define NjeErrProtocolError          -12

/*
**  NJE/TCP Data Block Header length and offsets
*/
#define TtbLength                    8
#define TtbOffFlags                  0
#define TtbOffLength                 2

/*
**  NJE/TCP Data Block Record Header length and offsets
*/
#define TtrLength                    4
#define TtrOffFlags                  0
#define TtrOffLength                 2

/*
**  NJE/TCP Control Record length and offsets
*/
#define CrLength                     33
#define CrOffType                    0
#define CrOffRHost                   8
#define CrOffRIP                     16
#define CrOffOHost                   20
#define CrOffOIP                     28
#define CrOffR                       32

/*
**  NJE/TCP Control Record NAK reason codes
*/
#define CrNakNoSuchLink              0x01
#define CrNakLinkActive              0x02
#define CrNakAttemptingActiveOpen    0x03
#define CrNakTemporaryFailure        0x04

#if DEBUG
static char *NjeConnStates[] = {
    "StNjeDisconnected",
    "StNjeRcvOpen",
    "StNjeRcvSOH_ENQ",
    "StNjeSndOpen",
    "StNjeRcvAck",
    "StNjeRcvSignon",
    "StNjeRcvResponseSignon",
    "StNjeExchangeData"
    };
static char *CrNakReasons[] =
    {
    "",
    "No such link",
    "Link active",
    "Link attempting active open",
    "Temporary failure"
    };
#endif

/*
**  -----------------------
**  Private Macro Functions
**  -----------------------
*/
#if DEBUG
#define HexColumn(x)      (3 * (x) + 4)
#define AsciiColumn(x)    (HexColumn(16) + 2 + (x))
#define LogLineLength    (AsciiColumn(16))
#endif

/*
**  -----------------------------------------
**  Private Typedef and Structure Definitions
**  -----------------------------------------
*/

#if DEBUG
typedef enum
    {
    ASCII = 0,
    EBCDIC,
    DisplayCode
    } CharEncoding;
#endif

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static u8 *npuNjeAppendLeader(Pcb *pcbp, u8 *bp);
static int npuNjeAppendRecords(Pcb *pcbp, u8 *bp, int len, u8 blockType);
static void npuNjeAsciiToEbcdic(u8 *ascii, u8 *ebcdic, int len);
static void npuNjeCloseConnection(Pcb *pcbp);
static u8 *npuNjeCloseDownlineBlock(Pcb *pcbp, u8 *dp);
static u8 *npuNjeCollectBlock(Pcb *pcbp, u8 *start, u8 *limit, bool *isComplete, int *size, int *status);
static bool npuNjeConnectTerminal(Pcb *pcbp);
static void npuNjeEbcdicToAscii(u8 *ebcdic, u8 *ascii, int len);
static Pcb *npuNjeFindPcbForCr(char *rhost, u32 rip, char *ohost, u32 oip);
static Tcb *npuNjeFindTcb(Pcb *pcbp);
static void npuNjeFlushOutput(Pcb *pcbp);
static void npuNjeParseControlRecord(u8 *crp, char *rhost, u32 *rip, char *ohost, u32 *oip, u8 *r);
static void npuNjePrepareOutput(Pcb *pcbp);
static int npuNjeSend(Pcb *pcbp, u8 *dp, int len);
static bool npuNjeSendControlRecord(Pcb *pcbp, u8 *crp, char *localName, u32 localIP, char *peerName, u32 peerIP, u8 r);
static void npuNjeSendUplineBlock(Pcb *pcbp, NpuBuffer *bp, u8 *dp, u8 blockType);
static void npuNjeSetTtrLength(Pcb *pcbp);
static void npuNjeTransmitQueuedBlocks(Pcb *pcbp);
static void npuNjeTrim(char *str);
static int npuNjeUploadBlock(Pcb *pcbp, u8 *blkp, int size, u8 *rcb, u8 *srcb);

#if DEBUG
static void npuNjeLogBytes(u8 *bytes, int len, CharEncoding encoding);
static void npuNjeLogFlush(void);
static void npuNjePrintStackTrace(FILE *fp);

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
static FILE *npuNjeLog = NULL;
static char npuNjeLogBuf[LogLineLength + 1];
static int  npuNjeLogBytesCol = 0;
#endif

/*
**  NJE/TCP Control Record Type strings (EBCDIC)
*/
static u8 CrTypeAck[8]  = { 0xc1, 0xc3, 0xd2, 0x40, 0x40, 0x40, 0x40, 0x40 }; // 'ACK     '
static u8 CrTypeOpen[8] = { 0xd6, 0xd7, 0xc5, 0xd5, 0x40, 0x40, 0x40, 0x40 }; // 'OPEN    '
static u8 CrTypeNak[8]  = { 0xd5, 0xc1, 0xd2, 0x40, 0x40, 0x40, 0x40, 0x40 }; // 'NAK     '

/*
**  Special NJE/TCP blocks
*/
static u8 DLE_ACK0[] =
    {
    0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00, 0x00, // TTB
    0x00, 0x00, 0x00, 0x02,                         // TTR
    DLE,  ACK0,                                     // data
    0x00, 0x00, 0x00, 0x00                          // TTREOB
    };
static u8 EmptyBlock[] =
    {
    0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00, // TTB
    0x00, 0x00, 0x00, 0x00                          // TTR
    };
static u8 SOH_ENQ[] =
    {
    0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00, 0x00, // TTB
    0x00, 0x00, 0x00, 0x02,                         // TTR
    SOH,  ENQ,                                      // data
    0x00, 0x00, 0x00, 0x00                          // TTREOB
    };
static u8 SYN_NAK[] =
    {
    0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00, 0x00, // TTB
    0x00, 0x00, 0x00, 0x02,                         // TTR
    SYN,  NAK,                                      // data
    0x00, 0x00, 0x00, 0x00                          // TTREOB
    };

/*
**  Special non-transparent NAM messages
*/
static char *ApplicationFailed       = "APPLICATION FAILED.";
#define     ApplicationFailedLen       19
static char *ApplicationNotPresent   = "APPLICATION NOT PRESENT.";
#define     ApplicationNotPresentLen   24
static char *ApplicationBusy         = "APPLICATION BUSY";
#define     ApplicationBusyLen         16
static char *LoggedOut               = "LOGGED OUT.";
#define     LoggedOutLen               11

/*
 **--------------------------------------------------------------------------
 **
 **  Public Functions
 **
 **--------------------------------------------------------------------------
 */

/*--------------------------------------------------------------------------
**  Purpose:        Try to send any queued data.
**
**  Parameters:     Name        Description.
**                  pcbp        PCB pointer
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuNjeTryOutput(Pcb *pcbp)
    {
    NpuBuffer *bp;
    time_t    currentTime;
    int       n;
    Tcb       *tcbp;

    currentTime = getSeconds();
    tcbp        = npuNjeFindTcb(pcbp);

    switch (pcbp->controls.nje.state)
        {
    case StNjeRcvOpen:
    case StNjeRcvSOH_ENQ:
    case StNjeRcvAck:
    case StNjeRcvSignon:
    case StNjeRcvResponseSignon:
        if (currentTime - pcbp->controls.nje.lastXmit > MaxWaitTime)
            {
#if DEBUG
            fprintf(npuNjeLog, "Port %02x: timeout in state %d\n", pcbp->claPort, pcbp->controls.nje.state);
#endif
            npuNjeCloseConnection(pcbp);

            return;
            }
        break;

    case StNjeExchangeData:
        if ((currentTime - pcbp->controls.nje.lastXmit > pcbp->controls.nje.pingInterval)
            && (pcbp->controls.nje.pingInterval > 0))
            {
            if ((tcbp != NULL) && (npuBipQueueNotEmpty(&tcbp->outputQ) == FALSE))
                {
                npuNetSend(tcbp, EmptyBlock, sizeof(EmptyBlock));
                }
            }
        break;

    case StNjeSndOpen:
        if (npuNjeSendControlRecord(pcbp, CrTypeOpen, npuNetHostID, pcbp->controls.nje.localIP,
                                    pcbp->ncbp->hostName, pcbp->controls.nje.remoteIP, 0))
            {
            pcbp->controls.nje.state = StNjeRcvAck;
            }
        break;
        }

    if (tcbp != NULL)
        {
        while ((bp = npuBipQueueExtract(&tcbp->outputQ)) != NULL)
            {
            n = npuNjeSend(pcbp, bp->data + bp->offset, bp->numBytes - bp->offset);
            if (n > 0)
                {
                bp->offset += n;
                }
            if (bp->offset >= bp->numBytes)
                {
                npuBipBufRelease(bp);
                }
            else
                {
                npuBipQueuePrepend(bp, &tcbp->outputQ);
                break;
                }
            }
        if (tcbp->state == StTermConnected)
            {
            npuNjeTransmitQueuedBlocks(pcbp);
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Process downline data from host.
**
**  Parameters:     Name        Description.
**                  tcbp        pointer to TCB
**                  bp          buffer with downline data message.
**                  last        last buffer.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuNjeProcessDownlineData(Tcb *tcbp, NpuBuffer *bp, bool last)
    {
    u8  blockType;
    u8  dbc;
    u8  *dp;
    int len;
    u8  *limit;
    Pcb *pcbp;
    int status;
    u8  *start;

    pcbp      = tcbp->pcbp;
    blockType = bp->data[BlkOffBTBSN] & BlkMaskBT;
    dp        = bp->data + BlkOffDbc;
    len       = bp->numBytes - BlkOffDbc;
    dbc       = *dp++; // extract data block clarifier
    len      -= 1;

#if DEBUG
    fprintf(npuNjeLog, "Port %02x: downline data received for %.7s, size %d, block type %u, dbc %02x\n",
            pcbp->claPort, tcbp->termName, len, blockType, dbc);
    npuNjeLogBytes(bp->data, bp->numBytes, (dbc & DbcTransparent) != 0 ? EBCDIC : ASCII);
    npuNjeLogFlush();
#endif

    if ((dbc & DbcTransparent) != 0)
        {
        status = npuNjeAppendRecords(pcbp, dp, len, blockType);
        npuTipNotifySent(tcbp, bp->data[BlkOffBTBSN]);
        if (status != NjeStatusOk)
            {
            npuNjeCloseConnection(pcbp);
            }
        }
    else
        {
        /*
        **  Scan the non-transparent block to look for messages indicating that NJF has failed or
        **  is not running. If any such messages are found, close the connection.
        **
        **  Each record in a non-transparent block begins with a format effector and ends with <US>
        */
        limit = dp + len;
        while (dp < limit)
            {
            start = dp++; // start points to format effector
            while (dp < limit && *dp != US)
                {
                dp += 1;
                }
            len = dp - start;
            if (((len > ApplicationFailedLen) && (memcmp(ApplicationFailed, start + 1, ApplicationFailedLen) == 0))
                || ((len > ApplicationNotPresentLen) && (memcmp(ApplicationNotPresent, start + 1, ApplicationNotPresentLen) == 0))
                || ((len > ApplicationBusyLen) && (memcmp(ApplicationBusy, start + 1, ApplicationBusyLen) == 0))
                || ((len > LoggedOutLen) && (memcmp(LoggedOut, start + 1, LoggedOutLen) == 0)))
                {
#if DEBUG
                fprintf(npuNjeLog, "Port %02x: %.7s disconnected from NJF\n", pcbp->claPort, tcbp->termName);
#endif
                npuTipNotifySent(tcbp, bp->data[BlkOffBTBSN]);
                npuNjeCloseConnection(pcbp);

                return;
                }
            dp += 1;
            }
        npuTipNotifySent(tcbp, bp->data[BlkOffBTBSN]);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Process upline data from terminal.
**
**  Parameters:     Name        Description.
**                  pcbp        PCB pointer
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuNjeProcessUplineData(Pcb *pcbp)
    {
    time_t delay;
    bool   done;
    u8     *dp;
    bool   isComplete;
    u8     *limit;
    char   ohost[9];
    u32    oip;
    Pcb    *pcbp2;
    u8     r;
    u8     rcb;
    char   rhost[9];
    u32    rip;
    int    size;
    u8     *sp;
    u8     srcb;
    int    status;
    Tcb    *tcbp;

#if DEBUG
    if (pcbp->controls.nje.state > StNjeRcvOpen)
        {
        fprintf(npuNjeLog, "Port %02x: TCP data received from %s, state %s\n",
                pcbp->claPort, pcbp->ncbp->hostName, NjeConnStates[pcbp->controls.nje.state]);
        }
    else
        {
        fprintf(npuNjeLog, "Port %02x: TCP data received, state %s\n",
                pcbp->claPort, NjeConnStates[pcbp->controls.nje.state]);
        }
    npuNjeLogBytes(pcbp->inputData, pcbp->inputCount, EBCDIC);
    npuNjeLogFlush();
#endif

    sp = pcbp->inputData;
    dp = pcbp->controls.nje.inputBufPtr;

    if (dp + pcbp->inputCount > pcbp->controls.nje.inputBuf + pcbp->controls.nje.inputBufSize)
        {
        fprintf(stderr, "Port %02x: NJE input buffer overflow, data discarded\n", pcbp->claPort);
        return;
        }
    while (pcbp->inputCount-- > 0)
        {
        *dp++ = *sp++;
        }
    pcbp->controls.nje.inputBufPtr = dp;
    dp = pcbp->controls.nje.inputBuf;
    done = FALSE;

    while (dp < pcbp->controls.nje.inputBufPtr && done == FALSE)
        {
        switch (pcbp->controls.nje.state)
            {
        case StNjeDisconnected:
            // discard any data received while disconnected
#if DEBUG
            fprintf(npuNjeLog, "Port %02x: disconnected, data discarded\n", pcbp->claPort);
#endif
            dp = pcbp->controls.nje.inputBufPtr;
            break;

        case StNjeRcvOpen:
            /*
            **  Expect and process an OPEN control record
            */
            if ((pcbp->controls.nje.inputBufPtr - dp) < CrLength)
                {
                done = TRUE;
                }
            else if (memcmp(dp, CrTypeOpen, 8) == 0)
                {
                npuNjeParseControlRecord(dp + 8, rhost, &rip, ohost, &oip, &r);
                dp += CrLength;
                pcbp2 = npuNjeFindPcbForCr(rhost, rip, ohost, oip);
                r     = 0;
                if (pcbp2 == NULL)
                    {
                    r = CrNakNoSuchLink;
                    }
                else if (pcbp2->claPort == pcbp->claPort)
                    {
                    tcbp = npuNjeFindTcb(pcbp);
                    if (tcbp != NULL)
                        {
                        r = CrNakAttemptingActiveOpen;
                        }
                    }
                else if (pcbp2->connFd > 0)
                    {
#if DEBUG
                    fprintf(npuNjeLog, "Port %02x: close connection due to active link conflict\n", pcbp2->claPort);
#endif
                    r = CrNakLinkActive;
                    npuNetCloseConnection(pcbp2);
                    }
                else
                    {
#if DEBUG
                    fprintf(npuNjeLog, "Port %02x: connection reassigned to port %02x\n", pcbp->claPort, pcbp2->claPort);
#endif
                    npuNjeResetPcb(pcbp2);
                    pcbp2->connFd                 = pcbp->connFd;
                    pcbp2->controls.nje.state     = pcbp->controls.nje.state;
                    pcbp2->controls.nje.isPassive = pcbp->controls.nje.isPassive;
                    pcbp2->controls.nje.lastXmit  = pcbp->controls.nje.lastXmit;
                    pcbp->connFd                  = 0;
                    pcbp->controls.nje.state      = StNjeDisconnected;
                    pcbp                          = pcbp2;
                    dp                            = pcbp->controls.nje.inputBufPtr;
                    }
                if (r == 0)
                    {
#if DEBUG
                    fprintf(npuNjeLog, "Port %02x: send upline request to connect terminal\n", pcbp->claPort);
#endif
                    if (npuNjeConnectTerminal(pcbp))
                        {
                        pcbp->controls.nje.state = StNjeRcvSOH_ENQ;
                        }
                    else
                        {
#if DEBUG
                        fprintf(npuNjeLog, "Port %02x: failed to issue terminal connection request\n",
                                pcbp->claPort);
#endif
                        r = CrNakTemporaryFailure;
                        }
                    }
                if ((npuNjeSendControlRecord(pcbp, (r == 0) ? CrTypeAck : CrTypeNak, ohost, oip, rhost, rip, r) == FALSE)
                    || (r != 0))
                    {
                    npuNjeCloseConnection(pcbp);
                    dp = pcbp->controls.nje.inputBufPtr;
                    }
                }
            else
                {
#if DEBUG
                fprintf(npuNjeLog, "Port %02x: expecting OPEN\n", pcbp->claPort);
#endif
                npuNjeCloseConnection(pcbp);
                dp = pcbp->controls.nje.inputBufPtr;
                }
            break;

        case StNjeRcvSOH_ENQ:
            /*
            **  Expect <SOH><ENQ> or <SYN><NAK> after sending ACK control record
            */
            dp = npuNjeCollectBlock(pcbp, dp, pcbp->controls.nje.inputBufPtr, &isComplete, &size, &status);
            if (isComplete)
                {
                if (status == 0)
                    {
                    status = npuNjeUploadBlock(pcbp, pcbp->controls.nje.inputBuf, size, &rcb, &srcb);
                    switch (status)
                        {
                    case NjeStatusSOH_ENQ:
                        if (npuNjeSend(pcbp, DLE_ACK0, sizeof(DLE_ACK0)) == sizeof(DLE_ACK0))
                            {
                            pcbp->controls.nje.state = StNjeRcvSignon;
                            }
                        else
                            {
                            npuNjeCloseConnection(pcbp);
                            dp = pcbp->controls.nje.inputBufPtr;
                            }
                        break;

                    case NjeStatusSYN_NAK:
                        if (npuNjeSend(pcbp, SOH_ENQ, sizeof(SOH_ENQ)) == sizeof(SOH_ENQ))
                            {
#if DEBUG
                            fprintf(npuNjeLog, "Port %02x: send upline request to connect terminal\n",
                                    pcbp->claPort);
#endif
                            if (npuNjeConnectTerminal(pcbp))
                                {
                                /*
                                **  The terminal connection request should cause the terminal to be
                                **  connected to NJF, and NJF should respond by sending an initial
                                **  signon request, so the next NJE protocol element received from
                                **  the peer should be a response signon.
                                */
                                pcbp->controls.nje.state = StNjeRcvResponseSignon;
                                }
                            else
                                {
#if DEBUG
                                fprintf(npuNjeLog, "Port %02x: failed to issue terminal connection request\n",
                                        pcbp->claPort);
#endif
                                npuNjeCloseConnection(pcbp);
                                dp = pcbp->controls.nje.inputBufPtr;
                                }
                            }
                        else
                            {
                            npuNjeCloseConnection(pcbp);
                            dp = pcbp->controls.nje.inputBufPtr;
                            }
                        break;

                    case NjeStatusDLE_ACK0:
                        // Ignore idle block
                        break;

                    case NjeStatusNothingUploaded:
                        // Retransmitted blocks were probably detected, so remain in this state
                        break;

                    default:
#if DEBUG
                        fprintf(npuNjeLog, "Port %02x: expecting <SOH><ENQ> or <SYN><NAK>, received status %d\n",
                                pcbp->claPort, status);
#endif
                        npuNjeCloseConnection(pcbp);
                        dp = pcbp->controls.nje.inputBufPtr;
                        break;
                        }
                    }
                else
                    {
#if DEBUG
                    fprintf(npuNjeLog, "Port %02x: block collection error %d\n", pcbp->claPort, status);
#endif
                    npuNjeCloseConnection(pcbp);
                    dp = pcbp->controls.nje.inputBufPtr;
                    }
                }
            else // isComplete == FALSE
                {
                done = TRUE;
                }
            break;

        case StNjeRcvAck:
            /*
            **  Expect and process an ACK or NAK control record
            */
            if ((pcbp->controls.nje.inputBufPtr - dp) < CrLength)
                {
                done = TRUE;
                }
            else if (memcmp(dp, CrTypeAck, 8) == 0)
                {
                dp += CrLength;
                if (npuNjeSend(pcbp, SOH_ENQ, sizeof(SOH_ENQ)) == sizeof(SOH_ENQ))
                    {
#if DEBUG
                    fprintf(npuNjeLog, "Port %02x: send upline request to connect terminal\n", pcbp->claPort);
#endif
                    if (npuNjeConnectTerminal(pcbp))
                        {
                        /*
                        **  The terminal connection request should cause the terminal to be
                        **  connected to NJF, and NJF should respond by sending an initial
                        **  signon request, so the next NJE protocol element received from
                        **  the peer should be a response signon.
                        */
                        pcbp->controls.nje.state = StNjeRcvResponseSignon;
                        }
                    else
                        {
#if DEBUG
                        fprintf(npuNjeLog, "Port %02x: failed to issue terminal connection request\n",
                                pcbp->claPort);
#endif
                        npuNjeCloseConnection(pcbp);
                        dp = pcbp->controls.nje.inputBufPtr;
                        }
                    }
                else
                    {
                    npuNjeCloseConnection(pcbp);
                    dp = pcbp->controls.nje.inputBufPtr;
                    }
                }
            else if (memcmp(dp, CrTypeNak, 8) == 0)
                {
                npuNjeParseControlRecord(dp + 8, rhost, &rip, ohost, &oip, &r);
                dp += CrLength;
#if DEBUG
                fprintf(npuNjeLog, "Port %02x: OPEN request denied: %s\n", pcbp->claPort, CrNakReasons[r]);
#endif
                npuNjeCloseConnection(pcbp);
                dp = pcbp->controls.nje.inputBufPtr;
                delay = (time_t)((getMilliseconds() % 5) + 3);
                // Arrange to attempt reconnection after a relatively short and random-ish interval
#if DEBUG
                fprintf(npuNjeLog, "Port %02x: delay %ld secs before attempting reconnection\n", pcbp->claPort, delay);
#endif
                pcbp->ncbp->nextConnectionAttempt = getSeconds() + delay;
                }
            else
                {
#if DEBUG
                fprintf(npuNjeLog, "Port %02x: expecting ACK or NAK\n", pcbp->claPort);
#endif
                npuNjeCloseConnection(pcbp);
                dp = pcbp->controls.nje.inputBufPtr;
                }
            break;

        case StNjeRcvSignon:
            /*
            **  Expect and process an initial signon record
            */
            dp = npuNjeCollectBlock(pcbp, dp, limit, &isComplete, &size, &status);
            if (isComplete)
                {
                if (status == 0)
                    {
                    status = npuNjeUploadBlock(pcbp, pcbp->controls.nje.inputBuf, size, &rcb, &srcb);
                    if ((status == NjeStatusOk) && (rcb == RCB_GCR) && (srcb == SRCB_InitialSignon))
                        {
                        /*
                        **  An initial signon was received and sent upline, so enter normal
                        **  data exchange mode. The next downline block received from NJF
                        **  should be the response signon, and then subsequent blocks should
                        **  stream-related blocks.
                        */
                        pcbp->controls.nje.state = StNjeExchangeData;
#if DEBUG
                        fprintf(npuNjeLog, "Port %02x: enter data exchange state with ping interval %d secs\n",
                                pcbp->claPort, pcbp->controls.nje.pingInterval);
#endif
                        }
                    else if ((status != NjeStatusNothingUploaded) && (status != NjeStatusDLE_ACK0))
                        {
#if DEBUG
                        fprintf(npuNjeLog, "Port %02x: expecting initial signon\n", pcbp->claPort);
#endif
                        npuNjeCloseConnection(pcbp);
                        dp = pcbp->controls.nje.inputBufPtr;
                        }
                    }
                else
                    {
#if DEBUG
                    fprintf(npuNjeLog, "Port %02x: expecting initial signon\n", pcbp->claPort);
#endif
                    npuNjeCloseConnection(pcbp);
                    dp = pcbp->controls.nje.inputBufPtr;
                    }
                }
            else // isComplete == FALSE
                {
                done = TRUE;
                }
            break;

        case StNjeRcvResponseSignon:
            /*
            **  Expect and process a signon response record
            */
            dp = npuNjeCollectBlock(pcbp, dp, limit, &isComplete, &size, &status);
            if (isComplete)
                {
                if (status == 0)
                    {
                    status = npuNjeUploadBlock(pcbp, pcbp->controls.nje.inputBuf, size, &rcb, &srcb);
                    if ((status == NjeStatusOk) && (rcb == RCB_GCR) && (srcb == SRCB_RespSignon))
                        {
                        npuNetSend(npuNjeFindTcb(pcbp), DLE_ACK0, sizeof(DLE_ACK0));
                        pcbp->controls.nje.state = StNjeExchangeData;
#if DEBUG
                        fprintf(npuNjeLog, "Port %02x: enter data exchange state with ping interval %d secs\n",
                                pcbp->claPort, pcbp->controls.nje.pingInterval);
#endif
                        }
                    else if ((status != NjeStatusNothingUploaded) && (status != NjeStatusDLE_ACK0))
                        {
#if DEBUG
                        fprintf(npuNjeLog, "Port %02x: expecting response signon\n", pcbp->claPort);
#endif
                        npuNjeCloseConnection(pcbp);
                        dp = pcbp->controls.nje.inputBufPtr;
                        }
                    }
                else
                    {
#if DEBUG
                    fprintf(npuNjeLog, "Port %02x: expecting response signon\n", pcbp->claPort);
#endif
                    npuNjeCloseConnection(pcbp);
                    dp = pcbp->controls.nje.inputBufPtr;
                    }
                }
            else // isComplete == FALSE
                {
                done = TRUE;
                }
            break;

        case StNjeExchangeData:
            /*
            **  Process ordinary data exchanges
            */
            dp = npuNjeCollectBlock(pcbp, dp, limit, &isComplete, &size, &status);
            if (isComplete)
                {
                if (status == 0)
                    {
                    status = npuNjeUploadBlock(pcbp, pcbp->controls.nje.inputBuf, size, &rcb, &srcb);
                    if ((status != NjeStatusOk) && (status != NjeStatusNothingUploaded) && (status != NjeStatusDLE_ACK0))
                        {
#if DEBUG
                        fprintf(npuNjeLog, "Port %02x: expecting normal data exchange, detected status %d\n",
                                pcbp->claPort, status);
#endif
                        npuNjeCloseConnection(pcbp);
                        dp = pcbp->controls.nje.inputBufPtr;
                        }
                    }
                else
                    {
#if DEBUG
                    fprintf(npuNjeLog, "Port %02x: expecting normal data exchange, detected status %d\n",
                            pcbp->claPort, status);
#endif
                    npuNjeCloseConnection(pcbp);
                    dp = pcbp->controls.nje.inputBufPtr;
                    }
                }
            else // isComplete == FALSE
                {
                done = TRUE;
                }
            break;

        default:
#if DEBUG
            fprintf(npuNjeLog, "Invalid NJE state: %d\n", pcbp->controls.nje.state);
#endif
            dp = pcbp->controls.nje.inputBufPtr;
            break;
            }
        }

    //
    // Move residual data, if any, to the beginning of the input buffer.
    //
    if (dp < pcbp->controls.nje.inputBufPtr)
        {
        if (dp > pcbp->controls.nje.inputBuf)
            {
            size = pcbp->controls.nje.inputBufPtr - dp;
            sp = dp;
            dp = pcbp->controls.nje.inputBuf;
            while (size-- > 0)
                {
                *dp++ = *sp++;
                }
            pcbp->controls.nje.inputBufPtr = dp;
            }
        }
    else
        {
        pcbp->controls.nje.inputBufPtr = pcbp->controls.nje.inputBuf;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Handle upline block acknowledgement
**
**  Parameters:     Name        Description.
**                  tcbp        Pointer to TCB
**                  bsn         Block sequence number of acknowledged block
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuNjeNotifyAck(Tcb *tcbp, u8 bsn)
    {
    tcbp->uplineBlockLimit += 1;
#if DEBUG
    fprintf(npuNjeLog, "Port %02x: ack for upline block from %.7s, ubl %d\n",
            tcbp->pcbp->claPort, tcbp->termName, tcbp->uplineBlockLimit);
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Handles a network connect notification from NET.
**
**  Parameters:     Name        Description.
**                  pcbp        pointer to PCB of connected port
**                  isPassive   TRUE if passive connection
**
**  Returns:        TRUE if notification processed successfully.
**
**------------------------------------------------------------------------*/
bool npuNjeNotifyNetConnect(Pcb *pcbp, bool isPassive)
    {
#if DEBUG
    fprintf(npuNjeLog, "Port %02x: %s network connection indication\n", pcbp->claPort,
            isPassive ? "passive" : "active");
#endif
    npuNjeResetPcb(pcbp);
    if (isPassive)
        {
        pcbp->controls.nje.isPassive = TRUE;
        pcbp->controls.nje.state     = StNjeRcvOpen;
        }
    else
        {
        /*
        **  This node initiated the connection. First, check the state of the
        **  port. If the port is not already connected, arrange to send an OPEN
        **  control record. Otherwise, set a long delay on next connection attempt
        **  and return FALSE so that the net module will close the socket.
        */
        if (pcbp->controls.nje.state != StNjeDisconnected)
            {
#if DEBUG
            fprintf(npuNjeLog, "Port %02x: port is already connected in state %s\n", pcbp->claPort,
                    NjeConnStates[pcbp->controls.nje.state]);
#endif
            pcbp->ncbp->nextConnectionAttempt = getSeconds() + (time_t)(24 * 60 * 60);

            return FALSE;
            }
        pcbp->controls.nje.state = StNjeSndOpen;
        }
    pcbp->controls.nje.lastXmit = getSeconds();

    return TRUE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Handles a network disconnect notification from NET.
**
**  Parameters:     Name        Description.
**                  pcbp        pointer to PCB of disconnected port
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuNjeNotifyNetDisconnect(Pcb *pcbp)
    {
#if DEBUG
    fprintf(npuNjeLog, "Port %02x: network disconnection indication\n", pcbp->claPort);
#endif
    npuNjeCloseConnection(pcbp);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Handles a terminal connect notification from SVM.
**
**  Parameters:     Name        Description.
**                  tcbp        pointer to TCB of connected terminal
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuNjeNotifyTermConnect(Tcb *tcbp)
    {
#if DEBUG
    fprintf(npuNjeLog, "Port %02x: connect terminal %.7s\n", tcbp->pcbp->claPort, tcbp->termName);
#endif

    if (tcbp->pcbp->connFd > 0)
        {
        tcbp->uplineBlockLimit = tcbp->params.fvUBL;
#if DEBUG
        fprintf(npuNjeLog, "Port %02x: upline block limit %d\n", tcbp->pcbp->claPort,
                tcbp->uplineBlockLimit);
#endif
        }
    else
        {
#if DEBUG
        fprintf(npuNjeLog, "Port %02x: no network connection, disconnect terminal %.7s\n",
                tcbp->pcbp->claPort, tcbp->termName);
#endif
        npuSvmSendDiscRequest(tcbp);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Handles a terminal disconnect event from SVM.
**
**  Parameters:     Name        Description.
**                  tcbp        pointer to TCB of disconnected terminal
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuNjeNotifyTermDisconnect(Tcb *tcbp)
    {
    // nothing to be done
    }

/*--------------------------------------------------------------------------
**  Purpose:        Presets NJE controls in a freshly allocated PCB.
**
**  Parameters:     Name        Description.
**                  pcbp        pointer to PCB
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuNjePresetPcb(Pcb *pcbp)
    {
#if DEBUG
    if (npuNjeLog == NULL)
        {
        npuNjeLog = fopen("njelog.txt", "wt");
        if (npuNjeLog == NULL)
            {
            fprintf(stderr, "njelog.txt - aborting\n");
            exit(1);
            }
        npuNjeLogFlush();    // initialize log buffer
        }
#endif
    pcbp->controls.nje.maxRecordSize = 1024; // renegotiated during signon
    pcbp->controls.nje.uplineQ.first = NULL;
    pcbp->controls.nje.uplineQ.last  = NULL;

    npuNjeResetPcb(pcbp);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Resets NJE controls in a PCB.
**
**  Parameters:     Name        Description.
**                  pcbp        pointer to PCB
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuNjeResetPcb(Pcb *pcbp)
    {
    NpuBuffer *bp;
    Tcb       *tcbp;

    pcbp->controls.nje.state            = StNjeDisconnected;
    pcbp->controls.nje.tp               = (Tcb *)NULL;
    pcbp->controls.nje.isPassive        = FALSE;
    pcbp->controls.nje.downlineBSN      = 0xff;
    pcbp->controls.nje.uplineBSN        = 0x0f;
    pcbp->controls.nje.lastDownlineRCB  = 0;
    pcbp->controls.nje.lastDownlineSRCB = 0;
    pcbp->controls.nje.retries          = 0;
    pcbp->controls.nje.lastXmit         = (time_t)0;
    pcbp->controls.nje.inputBufPtr      = pcbp->controls.nje.inputBuf;
    pcbp->controls.nje.outputBufPtr     = pcbp->controls.nje.outputBuf;
    pcbp->controls.nje.ttrp             = NULL;

    while ((bp = npuBipQueueExtract(&pcbp->controls.nje.uplineQ)) != NULL)
        {
        npuBipBufRelease(bp);
        }
    tcbp = npuNjeFindTcb(pcbp);
    if (tcbp != NULL)
        {
        while ((bp = npuBipQueueExtract(&tcbp->outputQ)) != NULL)
            {
            npuBipBufRelease(bp);
            }
        }
#if DEBUG
    fprintf(npuNjeLog, "Port %02x: reset PCB\n", pcbp->claPort);
#endif
    }

/*
 **--------------------------------------------------------------------------
 **
 **  Private Functions
 **
 **--------------------------------------------------------------------------
 */

/*--------------------------------------------------------------------------
**  Purpose:        Appends an NJE block leader to a buffer.
**
**  Parameters:     Name        Description.
**                  pcbp        Pointer to PCB
**                  bp          Pointer to buffer
**
**  Returns:        Pointer to next byte in buffer.
**
**------------------------------------------------------------------------*/
static u8 *npuNjeAppendLeader(Pcb *pcbp, u8 *bp)
    {
    *bp++ = DLE;
    *bp++ = STX;
    if (pcbp->controls.nje.downlineBSN == 0xff)
        {
        *bp++ = 0xa0; // Reset block sequence number
        pcbp->controls.nje.downlineBSN = 0;
        }
    else
        {
        *bp++ = 0x80 | pcbp->controls.nje.downlineBSN; // BCB
        pcbp->controls.nje.downlineBSN = (pcbp->controls.nje.downlineBSN + 1) & 0x0f;
        }
    *bp++ = 0x8f; // FCS
    *bp++ = 0xcf; // FCS

    return bp;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Appends downline records from NJF to the TCP output buffer.
**
**  Parameters:     Name        Description.
**                  pcbp        Pointer to PCB
**                  bp          Pointer to block
**                  len         Length of block
**                  blockType   BtHTMSG or BtHTBLK
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static int npuNjeAppendRecords(Pcb *pcbp, u8 *bp, int len, u8 blockType)
    {
    int blockLen;
    int bufLen;
    u8  *dp;
    u8  *limit;
    int maxBytesNeeded;
    int nBytes;
    u8  ncc;
    u8  rcb;
    u8  *recLimit;
    u8  srcb;

    dp    = pcbp->controls.nje.outputBufPtr;
    limit = bp + len;
    while (bp < limit)
        {
        ncc  = *bp++;
        rcb  = *bp++;
        srcb = *bp++;
        if (rcb == RCB_NJFTIPCommand)
            {
            switch (srcb)
                {
            case SRCB_CMDXBZ:
                pcbp->controls.nje.maxRecordSize = (*bp << 8) | *(bp + 1);
#if DEBUG
                fprintf(npuNjeLog, "Port %02x: TIP command, set transmission block size to %d\n",
                        pcbp->claPort, pcbp->controls.nje.maxRecordSize);
#endif
                bp += 2;
                break;

            case SRCB_CMDABT:
#if DEBUG
                fprintf(npuNjeLog, "Port %02x: TIP command, abort transmission, stream %d, sub-record control byte %02x\n",
                        pcbp->claPort, *bp, *(bp + 1));
#endif
                bp += 2;
                break;

            default:
#if DEBUG
                fprintf(npuNjeLog, "Port %02x: unrecognized TIP command %02x\n", pcbp->claPort, srcb);
#endif
                bp = limit;
                break;
                }
            continue;
            }

        /*
        **  If the block is an initial signon and the connection is
        **  passive, signal a protocol error as NJF is out of sync
        **  with the state of the connection.
        */
        if ((rcb == RCB_GCR) && (srcb == SRCB_InitialSignon) && pcbp->controls.nje.isPassive)
            {
#if DEBUG
            fprintf(npuNjeLog, "Port %02x: downline initial signon detected and discarded while connection is in passive state\n",
                    pcbp->claPort);
#endif

            return NjeStatusOk;
            }

        /*
        **  Calculate maximum number of bytes needed to encode record
        */
        maxBytesNeeded = ncc                      // number of data bytes in record
                         + 2                      // RCB and SRCB
                         + ((ncc + 62) / 63) + 1; // number of SCB bytes needed;

        /*
        **  If insufficient space remains in the output buffer to append the
        **  record, flush the buffer downline, and start a new one.
        */
        bufLen = dp - pcbp->controls.nje.outputBuf;
        if (bufLen + maxBytesNeeded + TtrLength + 32 > pcbp->controls.nje.blockSize)
            {
            *dp++ = 0x00; // end of block RCB
            pcbp->controls.nje.outputBufPtr = dp;
            npuNjeFlushOutput(pcbp);
            bufLen = 0;
            }

        /*
        **  If the output buffer is empty, prepare it for a new downline block
        */
        if (bufLen < TtbLength)
            {
            npuNjePrepareOutput(pcbp);
            dp     = pcbp->controls.nje.outputBufPtr;
            bufLen = dp - pcbp->controls.nje.outputBuf;
            }

        /*
        **  Else if the last RCB or SRCB differs from the RCB or SRCB of this
        **  record, close the current NJE block, and start a new one.
        */
        else if (((pcbp->controls.nje.lastDownlineRCB != rcb) && (pcbp->controls.nje.lastDownlineRCB != 0))
                 || ((pcbp->controls.nje.lastDownlineSRCB != srcb) && (pcbp->controls.nje.lastDownlineSRCB != 0)))
            {
            dp = npuNjeCloseDownlineBlock(pcbp, dp);
            }

        /*
        **  If appending the record to the current NJE block would exceed the
        **  maximum negotiated block size, close the current block and start a
        **  new one.
        **/
        blockLen = dp - (pcbp->controls.nje.ttrp + TtrLength);
        if (blockLen + maxBytesNeeded + 8 > pcbp->controls.nje.maxRecordSize)
            {
            dp = npuNjeCloseDownlineBlock(pcbp, dp);
            }

        *dp++    = rcb;  // RCB
        *dp++    = srcb; // SRCB
        recLimit = bp + ncc;
        pcbp->controls.nje.lastDownlineRCB  = rcb;
        pcbp->controls.nje.lastDownlineSRCB = srcb;

        if (rcb == RCB_GCR)
            {
            if ((srcb == SRCB_InitialSignon) || (srcb == SRCB_RespSignon))
                {
                while (bp < recLimit)
                    {
                    *dp++ = *bp++;
                    }
                }
            else
                {
                bp = recLimit;
                }
            }
        else
            {
            while (bp < recLimit)
                {
                nBytes = recLimit - bp;
                if (nBytes > 63)
                    {
                    nBytes = 63;
                    }
                *dp++ = 0xc0 + nBytes; // SCB
                while (nBytes-- > 0)
                    {
                    *dp++ = *bp++;
                    }
                }
            *dp++ = 0x00; // end of record SCB
            }
        }
    if (dp > pcbp->controls.nje.outputBufPtr)
        {
        if (blockType == BtHTMSG)
            {
            if (rcb != RCB_GCR)
                {
                *dp++ = 0x00; // End of data RCB
                }
            pcbp->controls.nje.outputBufPtr = dp;
            npuNjeFlushOutput(pcbp);
            }
        else
            {
            pcbp->controls.nje.outputBufPtr = dp;
            }
        }

    return NjeStatusOk;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Translates an array of ASCII characters to EBCDIC.
**
**  Parameters:     Name        Description.
**                  ascii       Pointer to source array of ASCII characters
**                  ebcdic      Pointer to destination array of EBCDIC characters
**                  len         Number of characters to translate
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuNjeAsciiToEbcdic(u8 *ascii, u8 *ebcdic, int len)
    {
    while (len-- > 0)
        {
        *ebcdic++ = asciiToEbcdic[*ascii++];
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Closes an NJE connection.
**
**  Parameters:     Name        Description.
**                  pcbp        pointer to PCB
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuNjeCloseConnection(Pcb *pcbp)
    {
    Tcb *tcbp;

#if DEBUG
    fprintf(npuNjeLog, "Port %02x: close connection\n", pcbp->claPort);
#endif
    tcbp = npuNjeFindTcb(pcbp);
    if (tcbp != NULL && tcbp->state != StTermIdle)
        {
        npuSvmSendDiscRequest(tcbp);
        }
    else
        {
        npuNetCloseConnection(pcbp);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Closes the current downline block and starts a new one.
**
**  Parameters:     Name        Description.
**                  pcbp        Pointer to PCB
**                  dp          Pointer to next byte of current block
**
**  Returns:        Pointer to next byte of new block.
**
**------------------------------------------------------------------------*/
static u8 *npuNjeCloseDownlineBlock(Pcb *pcbp, u8 *dp)
    {
    *dp++ = 0x00; // end of block RCB
    pcbp->controls.nje.outputBufPtr = dp;
    npuNjeSetTtrLength(pcbp);
    memset(dp, 0, TtrLength); // initialize new TTR
    pcbp->controls.nje.ttrp = dp;
    dp += TtrLength;
    dp  = npuNjeAppendLeader(pcbp, dp);
    pcbp->controls.nje.outputBufPtr = dp;

    return dp;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Collects a complete NJE/TCP block from network input.
**                  The block is collected in the PCB input buffer.
**
**  Parameters:     Name        Description.
**                  pcbp        Pointer to PCB
**                  start       Pointer to start of input data
**                  limit       Pointer to one beyond end of input data
**                  isComplete  TRUE if complete NJE/TCP block collected
**                  size        Returned size of collected block
**                  status      Returned status indication
**
**  Returns:        Pointer to next byte of uncollected input data
**                    (e.g., beginning of next NJE/TCP block).
**
**------------------------------------------------------------------------*/
static u8 *npuNjeCollectBlock(Pcb *pcbp, u8 *start, u8 *limit, bool *isComplete, int *size, int *status)
    {
    int currentBlockSize;
    u8  *dp;
    u8  *ibLimit;
    u8  *ibp;
    int njeBlockSize;
    int recLen;
    u8  *sp;

    *isComplete      = FALSE;
    *size            = 0;
    *status          = 0;
    currentBlockSize = 0;

    /*
    **  First, ensure that the TTB has been collected because it contains
    **  the length of the block.
    */
    sp = start;
    dp = pcbp->controls.nje.inputBuf;
    while (sp < limit && currentBlockSize < TtbLength)
        {
        *dp++ = *sp++;
        currentBlockSize += 1;
        }
    if (currentBlockSize < TtbLength)
        {
        return start;
        }

    /*
    **  Validate that the provided block length does not exceed the maximum block
    **  size configured for the peer.
    */
    njeBlockSize = (*(pcbp->controls.nje.inputBuf + TtbOffLength) << 8)
                   | *(pcbp->controls.nje.inputBuf + TtbOffLength + 1);
    if (njeBlockSize > pcbp->controls.nje.blockSize)
        {
#if DEBUG
        fprintf(npuNjeLog, "Port %02x: block size received in TTB (%d) exceeds configured max block size (%d)\n",
                pcbp->claPort, njeBlockSize, pcbp->controls.nje.blockSize);
#endif
        *status     = NjeErrBlockTooLong;
        *isComplete = TRUE;
        return start;
        }

    /*
    **  Collect all of the bytes due
    */
    while (sp < limit && currentBlockSize < njeBlockSize)
        {
        *dp++ = *sp++;
        currentBlockSize += 1;
        }

    /*
    **  When all of the bytes have been collected, compress out the TTB
    **  and TTR's before making the result available to the caller.
    */
    if (currentBlockSize >= njeBlockSize)
        {
        ibp     = pcbp->controls.nje.inputBuf;
        ibLimit = ibp + njeBlockSize;
        dp      = ibp + TtbLength;
        while (dp + TtrLength < ibLimit)
            {
            recLen = (*(dp + TtrOffLength) << 8) | *(dp + TtrOffLength + 1);
            dp    += TtrLength;
            while (dp < ibLimit && recLen-- > 0)
                {
                *ibp++ = *dp++;
                }
            }
        *size = ibp - pcbp->controls.nje.inputBuf;
        *isComplete = TRUE;
        return sp;
        }

    return start;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Start host connection sequence.
**
**  Parameters:     Name        Description.
**                  pcbp        PCB pointer
**
**  Returns:        TRUE if sequence started, FALSE otherwise.
**
**------------------------------------------------------------------------*/
static bool npuNjeConnectTerminal(Pcb *pcbp)
    {
    Tcb *tcbp;

    tcbp = npuNjeFindTcb(pcbp);
    if (tcbp == NULL)
        {
        return npuSvmConnectTerminal(pcbp);
        }
    else
        {
#if DEBUG
        fprintf(npuNjeLog, "Port %02x: already associated with a TCB\n", pcbp->claPort);
#endif

        return FALSE;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Translates an array of EBCDIC characters to ASCII.
**
**  Parameters:     Name        Description.
**                  ebcdic      Pointer to destination array of EBCDIC characters
**                  ascii       Pointer to source array of ASCII characters
**                  len         Number of characters to translate
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuNjeEbcdicToAscii(u8 *ebcdic, u8 *ascii, int len)
    {
    while (len-- > 0)
        {
        *ascii++ = ebcdicToAscii[*ebcdic++];
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Find the PCB assoicated with a given NJE control record
**
**  Parameters:     Name        Description.
**                  rhost       Pointer to RHOST element of control record
**                  rip         RIP element of control record
**                  ohost       Pointer to OHOST element of control record
**                  oip         OIP element of control record
**
**  Returns:        pointer to PCB, or NULL if PCB not found.
**
**------------------------------------------------------------------------*/
static Pcb *npuNjeFindPcbForCr(char *rhost, u32 rip, char *ohost, u32 oip)
    {
    int claPort;
    Pcb *pcbp;

    /*
    **  Find matching NJE definition in Pcb table
    */
    for (claPort = 0; claPort <= npuNetMaxClaPort; claPort++)
        {
        pcbp = npuNetFindPcb(claPort);
        if ((pcbp->ncbp != NULL)
            && (pcbp->ncbp->connType == ConnTypeNje)
            && (strcasecmp(pcbp->ncbp->hostName, rhost) == 0)
            && (strcasecmp(npuNetHostID, ohost) == 0)
            && (pcbp->controls.nje.remoteIP == rip
                || pcbp->controls.nje.remoteIP == 0)
/*          && (pcbp->controls.nje.localIP == oip) */ )
            {
            return pcbp;
            }
        }

    return NULL;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Find the TCB assoicated with a given PCB
**
**  Parameters:     Name        Description.
**                  pcbp        pointer to PCB
**
**  Returns:        pointer to TCB, or NULL if TCB not found.
**
**------------------------------------------------------------------------*/
static Tcb *npuNjeFindTcb(Pcb *pcbp)
    {
    int i;
    Tcb *tcbp;

    tcbp = pcbp->controls.nje.tp;
    if (tcbp != NULL && tcbp->state != StTermIdle && tcbp->pcbp == pcbp)
        {
        return tcbp;
        }

    for (i = 1; i < MaxTcbs; i++)
        {
        tcbp = &npuTcbs[i];
        if (tcbp->state != StTermIdle && tcbp->pcbp == pcbp)
            {
            pcbp->controls.nje.tp = tcbp;

            return tcbp;
            }
        }

    return NULL;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Flush the downline output buffer downstream.
**
**  Parameters:     Name        Description.
**                  pcbp        Pointer to PCB
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuNjeFlushOutput(Pcb *pcbp)
    {
    int bufLen;

    npuNjeSetTtrLength(pcbp);
    memset(pcbp->controls.nje.outputBufPtr, 0, TtrLength); // Append end of buffer TTR
    pcbp->controls.nje.outputBufPtr += TtrLength;
    bufLen = pcbp->controls.nje.outputBufPtr - pcbp->controls.nje.outputBuf;
    pcbp->controls.nje.outputBuf[TtbOffLength]     = bufLen >> 8;
    pcbp->controls.nje.outputBuf[TtbOffLength + 1] = bufLen & 0xff;
    npuNetSend(npuNjeFindTcb(pcbp), pcbp->controls.nje.outputBuf,
               pcbp->controls.nje.outputBufPtr - pcbp->controls.nje.outputBuf);
    pcbp->controls.nje.outputBufPtr     = pcbp->controls.nje.outputBuf;
    pcbp->controls.nje.ttrp             = NULL;
    pcbp->controls.nje.lastDownlineRCB  = 0;
    pcbp->controls.nje.lastDownlineSRCB = 0;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Parse and translate parameters of an NJE/TCP control record.
**
**  Parameters:     Name        Description.
**                  crp         Pointer to RHOST parameter of control record
**                  rhost       Pointer to buffer for translated RHOST
**                  rip         Pointer to RIP value
**                  ohost       Pointer to buffer for translated OHOST
**                  oip         Pointer to OIP value
**                  r           Pointer to R field value
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuNjeParseControlRecord(u8 *crp, char *rhost, u32 *rip, char *ohost, u32 *oip, u8 *r)
    {
    u32 val;

    npuNjeEbcdicToAscii(crp, (u8 *)rhost, 8);
    rhost[8] = '\0';
    npuNjeTrim(rhost);
    crp += 8;
    val  = *crp++ << 24;
    val |= *crp++ << 16;
    val |= *crp++ << 8;
    val |= *crp++;
    *rip = val;
    npuNjeEbcdicToAscii(crp, (u8 *)ohost, 8);
    ohost[8] = '\0';
    npuNjeTrim(ohost);
    crp += 8;
    val  = *crp++ << 24;
    val |= *crp++ << 16;
    val |= *crp++ << 8;
    val |= *crp++;
    *oip = val;
    *r   = *crp;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Prepares for sending a new NJE/TCP block downline.
**
**  Parameters:     Name        Description.
**                  pcbp        Pointer to PCB
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuNjePrepareOutput(Pcb *pcbp)
    {
    memset(pcbp->controls.nje.outputBuf, 0, TtbLength + TtrLength); // preset TTB and TTR
    pcbp->controls.nje.ttrp             = pcbp->controls.nje.outputBuf + TtbLength;
    pcbp->controls.nje.outputBufPtr     = npuNjeAppendLeader(pcbp, pcbp->controls.nje.ttrp + TtrLength);
    pcbp->controls.nje.lastDownlineRCB  = 0;
    pcbp->controls.nje.lastDownlineSRCB = 0;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Sends a packet of bytes to a peer.
**
**  Parameters:     Name        Description.
**                  pcbp        Pointer to PCB
**                  dp          Pointer to packet of bytes
**                  len         Number of bytes in packet
**
**  Returns:        TRUE if packet sent successfully.
**
**------------------------------------------------------------------------*/
static int npuNjeSend(Pcb *pcbp, u8 *dp, int len)
    {
    int n;

    n = send(pcbp->connFd, dp, len, 0);
    pcbp->controls.nje.lastXmit = getSeconds();
#if DEBUG
    if (n > 0)
        {
        fprintf(npuNjeLog, "Port %02x: TCP data sent to %s (%d/%d bytes)\n", pcbp->claPort, pcbp->ncbp->hostName, n, len);
        npuNjeLogBytes(dp, n, EBCDIC);
        npuNjeLogFlush();
        }
    else if (n == 0)
        {
        fprintf(npuNjeLog, "Port %02x: TCP congested, no data sent to %s\n", pcbp->claPort, pcbp->ncbp->hostName);
        }
    else
        {
        fprintf(npuNjeLog, "Port %02x: failed to send TCP data to %s (errno=%d)\n", pcbp->claPort, pcbp->ncbp->hostName, errno);
        }
#endif

    return n;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Sends an NJE/TCP control record to a peer.
**
**  Parameters:     Name        Description.
**                  pcbp        Pointer to PCB
**                  crp         Pointer to control record type
**                  localName   Local host's node name
**                  localIP     Local host's IPv4 address
**                  peerName    Peer's node name
**                  peerIP      Peer's IPv4 address
**                  r           The 'R' field value for the control record
**
**  Returns:        TRUE if control record sent successfully.
**
**------------------------------------------------------------------------*/
static bool npuNjeSendControlRecord(Pcb *pcbp, u8 *crp, char *localName, u32 localIP, char *peerName, u32 peerIP, u8 r)
    {
    u8  *bp;
    u8  buffer[33];
    int len;

    bp = buffer;
    memcpy(bp, crp, 8);
    bp += 8;
    len = strlen(localName);
    npuNjeAsciiToEbcdic((u8 *)localName, bp, (len < 9) ? len : 8);
    bp += len;
    while (len++ < 8)
        {
        *bp++ = EbcdicBlank;
        }
    *bp++ = localIP >> 24;
    *bp++ = (localIP >> 16) & 0xff;
    *bp++ = (localIP >> 8) & 0xff;
    *bp++ = localIP & 0xff;
    len   = strlen(peerName);
    npuNjeAsciiToEbcdic((u8 *)peerName, bp, (len < 9) ? len : 8);
    bp += len;
    while (len++ < 8)
        {
        *bp++ = EbcdicBlank;
        }
    *bp++ = peerIP >> 24;
    *bp++ = (peerIP >> 16) & 0xff;
    *bp++ = (peerIP >> 8) & 0xff;
    *bp++ = peerIP & 0xff;
    *bp   = r;

    return npuNjeSend(pcbp, buffer, sizeof(buffer)) == sizeof(buffer);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Sends a block of NJE data upline.
**
**  Parameters:     Name        Description.
**                  pcbp        Pointer to PCB
**                  bp          Pointer to NpuBuffer
**                  dp          Pointer to next byte in data buffer
**                  blockType   BtHTBLK or BtHTMSG
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuNjeSendUplineBlock(Pcb *pcbp, NpuBuffer *bp, u8 *dp, u8 blockType)
    {
    Tcb *tcbp;

    bp->data[BlkOffBTBSN] = blockType;
    bp->data[BlkOffDbc]   = DbcTransparent;
    bp->numBytes          = dp - bp->data;
    npuBipQueueAppend(bp, &pcbp->controls.nje.uplineQ);

    tcbp = npuNjeFindTcb(pcbp);
    if ((tcbp != NULL) && (tcbp->state == StTermConnected))
        {
        npuNjeTransmitQueuedBlocks(pcbp);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Sets the length field in the last TTR of the downline
**                  output buffer.
**
**  Parameters:     Name        Description.
**                  pcbp        Pointer to PCB
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuNjeSetTtrLength(Pcb *pcbp)
    {
    int recLen;
    u8  *ttrp;

    ttrp   = pcbp->controls.nje.ttrp;
    recLen = pcbp->controls.nje.outputBufPtr - (ttrp + TtrLength);
    *(ttrp + TtrOffLength)     = recLen >> 8;
    *(ttrp + TtrOffLength + 1) = recLen & 0xff;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Transmits queued blocks upline to NAM.
**
**  Parameters:     Name        Description.
**                  pcbp        Pointer to PCB
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuNjeTransmitQueuedBlocks(Pcb *pcbp)
    {
    NpuBuffer *bp;
    Tcb       *tcbp;

    tcbp = npuNjeFindTcb(pcbp);
    if (tcbp != NULL)
        {
        while (tcbp->uplineBlockLimit > 0
               && (bp = npuBipQueueExtract(&pcbp->controls.nje.uplineQ)) != NULL)
            {
            bp->data[BlkOffDN]     = npuSvmCouplerNode;
            bp->data[BlkOffSN]     = npuSvmNpuNode;
            bp->data[BlkOffCN]     = tcbp->cn;
            bp->data[BlkOffBTBSN] |= tcbp->uplineBsn++ << BlkShiftBSN;
            if (tcbp->uplineBsn >= 8)
                {
                tcbp->uplineBsn = 1;
                }
            tcbp->uplineBlockLimit -= 1;
            npuBipRequestUplineTransfer(bp);
#if DEBUG
            fprintf(npuNjeLog, "Port %02x: upline data sent from %.7s, size %d, block type %u, ubl %d, dbc %02x\n",
                    pcbp->claPort, tcbp->termName, bp->numBytes - (BlkOffDbc + 1), bp->data[BlkOffBTBSN] & BlkMaskBT,
                    tcbp->uplineBlockLimit, bp->data[BlkOffDbc]);
            npuNjeLogBytes(bp->data, bp->numBytes, EBCDIC);
            npuNjeLogFlush();
#endif
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Trim trailing blanks from a string.
**
**  Parameters:     Name        Description.
**                  str         Pointer to string
**
**  Returns:        nothing
**
**------------------------------------------------------------------------*/
static void npuNjeTrim(char *str)
    {
    char *cp;
    char *lastNB;

    lastNB = str;
    cp     = str;
    while (*cp)
        {
        if (*cp != ' ')
            {
            lastNB = cp;
            }
        cp += 1;
        }
    *(lastNB + 1) = '\0';
    }

/*--------------------------------------------------------------------------
**  Purpose:        Parses a block received from a peer and queues its
**                  records for uploading to NJF.
**
**  Parameters:     Name        Description.
**                  pcbp        Pointer to PCB
**                  blkp        Pointer to block
**                  size        Size of block
**                  rcb         Pointer to RCB value to be returned
**                  srcb        Pointer to SRCB value to be returned
**
**  Returns:        Status code.
**
**------------------------------------------------------------------------*/
static int npuNjeUploadBlock(Pcb *pcbp, u8 *blkp, int size, u8 *rcb, u8 *srcb)
    {
    u8        blockType;
    int       blocksUploaded;
    NpuBuffer *bp;
    u8        bsn;
    u8        *ibLimit;
    u8        *ibp;
    bool      isRetransmission;
    int       len;
    u8        *obLimit;
    u8        *obp;
    int       recLen;
    u8        scb;
    u8        *start;

    *rcb             = *srcb = 0;
    ibp              = blkp;
    ibLimit          = blkp + size;
    bp               = npuBipBufGet();
    obp              = bp->data + BlkOffDbc + 1;
    obLimit          = bp->data + MaxUplineBlockSize;
    blocksUploaded   = 0;
    isRetransmission = FALSE;

    while (ibp < ibLimit)
        {
        len = ibLimit - ibp;
        if (len < 2)
            {
#if DEBUG
            fprintf(npuNjeLog, "Port %02x: block too short\n", pcbp->claPort);
#endif
            npuBipBufRelease(bp);

            return NjeErrBlockTooShort;
            }
        if ((*ibp == SOH) && (*(ibp + 1) == ENQ))
            {
            npuBipBufRelease(bp);

            return NjeStatusSOH_ENQ;
            }
        if ((*ibp == SYN) && (*(ibp + 1) == NAK))
            {
            npuBipBufRelease(bp);

            return NjeStatusSYN_NAK;
            }
        if (*ibp == DLE)
            {
            if (*(ibp + 1) == ACK0)
                {
                npuBipBufRelease(bp);

                return NjeStatusDLE_ACK0;
                }
            if (*(ibp + 1) != STX)
                {
#if DEBUG
                fprintf(npuNjeLog, "Port %02x: bad block leader\n", pcbp->claPort);
#endif
                npuBipBufRelease(bp);

                return NjeErrBadLeader;
                }
            }

        /*
        **  Block is a protocol block starting with <DLE><STX>. First, examine and validate
        **  the BCB byte.
        */
        if (len < 7)
            {
#if DEBUG
            fprintf(npuNjeLog, "Port %02x: block too short, %d < min 7\n", pcbp->claPort, len);
#endif
            npuBipBufRelease(bp);

            return NjeErrBlockTooShort;
            }
        isRetransmission = FALSE;
        ibp += 2;
        bsn  = *ibp & 0x0f;
        switch (*ibp & 0xf0)
            {
        /*
        **  BCB: Normal block
        */
        case 0x80:
            if (((pcbp->controls.nje.uplineBSN + 1) & 0x0f) == bsn)
                {
                pcbp->controls.nje.uplineBSN = bsn;
                pcbp->controls.nje.retries   = 0;
                }
            else if (pcbp->controls.nje.uplineBSN == bsn)
                {
                // Continue to validate the block, and then discard it
                pcbp->controls.nje.retries += 1;
#if DEBUG
                fprintf(npuNjeLog, "Port %02x: retransmission (%d) detected (bsn %02x), block will be discarded\n",
                        pcbp->claPort, pcbp->controls.nje.retries, bsn);
#endif
                if (pcbp->controls.nje.retries > MaxRetries)
                    {
#if DEBUG
                    fprintf(npuNjeLog, "Port %02x: retransmission limit (%d) exceeded\n", pcbp->claPort, MaxRetries);
#endif

                    return NjeErrTooManyRetries;
                    }
                isRetransmission = TRUE;
                }
            else
                {
#if DEBUG
                fprintf(npuNjeLog, "Port %02x: invalid sequence number in BCB (%02x) of data block (expected bsn %02x)\n",
                        pcbp->claPort, *ibp, (pcbp->controls.nje.uplineBSN + 1) & 0x0f);
#endif
                npuBipBufRelease(bp);

                return NjeErrBadBSN;
                }
            break;

        /*
        **  BCB: Bypass sequence count validation
        */
        case 0x90:
            pcbp->controls.nje.retries = 0;
            break;

        /*
        **  BCB: Reset sequence count
        */
        case 0xa0:
            pcbp->controls.nje.uplineBSN = (bsn - 1) & 0x0f;
            pcbp->controls.nje.retries   = 0;
            break;

        /*
        **  BCB: invalid
        */
        default:
#if DEBUG
            fprintf(npuNjeLog, "Port %02x: invalid BCB (%02x)\n", pcbp->claPort, *ibp);
#endif
            npuBipBufRelease(bp);

            return NjeErrBadBCB;
            }

        /*
        **  Next, examine and validate the FCS bytes
        */
        ibp += 1;
        if (((*ibp & 0x80) == 0x00) || ((*(ibp + 1) & 0x80) == 0x00))
            {
#if DEBUG
            fprintf(npuNjeLog, "Port %02x: invalid FCS (%02x%02x) in data block\n", pcbp->claPort, *ibp, *(ibp + 1));
#endif
            npuBipBufRelease(bp);

            return NjeErrBadFCS;
            }

        /*
        **  Next, examine, validate, and process the RCB and SRCB bytes
        */
        ibp += 2;
        while (ibp < ibLimit)
            {
            *rcb      = *ibp++;
            *srcb     = *ibp++;
            blockType = BtHTMSG; // preset block type
            switch (*rcb)
                {
            /*
            **  General Control Record (e.g., signon, response signon, signoff, etc.)
            **
            **  This record type should be the first and only record in a block.
            */
            case 0xf0: // general control record
                switch (*srcb)
                {
                case SRCB_Signoff:
                    recLen = 0;
                    break;

                case SRCB_InitialSignon:
                case SRCB_RespSignon:
                    if (obp != bp->data + BlkOffDbc + 1)
                        {
#if DEBUG
                        fprintf(npuNjeLog, "Port %02x: GCR is not first record in block\n", pcbp->claPort);
#endif
                        npuBipBufRelease(bp);

                        return NjeErrProtocolError;
                        }
                    recLen = *ibp - 2;
                    break;

                case SRCB_ResetSignon:
                case SRCB_AcceptSignon:
                case SRCB_AddConnection:
                case SRCB_DeleteConnection:
                default:
#if DEBUG
                    fprintf(npuNjeLog, "Port %02x: unsupported GCR type %02x\n", pcbp->claPort, *srcb);
#endif
                    npuBipBufRelease(bp);

                    return NjeErrProtocolError;

                    break;
                }
                *obp++ = recLen;
                *obp++ = *rcb;
                *obp++ = *srcb;
                while (recLen-- > 0)
                    {
                    *obp++ = *ibp++;
                    }
                if (isRetransmission)
                    {
                    npuBipBufRelease(bp);
                    }
                else
                    {
                    npuNjeSendUplineBlock(pcbp, bp, obp, BtHTMSG);
                    blocksUploaded += 1;
                    }

                return (blocksUploaded > 0) ? NjeStatusOk : NjeStatusNothingUploaded;

            /*
            **  These record types are variable length and contain SCB's (String Control Bytes)
            **  describing the substrings comprising of each record.
            **
            **  Depending upon the SRCB type of a SYSIN/SYSOUT record, the upline block type
            **  might be BtHTBLK instead of BtHTMSG.
            */
            case 0x98: // SYSIN record
            case 0xa8: //     "
            case 0xb8: //     "
            case 0xc8: //     "
            case 0xd8: //     "
            case 0xe8: //     "
            case 0xf8: //     "
            case 0x99: // SYSOUT record
            case 0xa9: //     "
            case 0xb9: //     "
            case 0xc9: //     "
            case 0xd9: //     "
            case 0xe9: //     "
            case 0xf9: //     "
                /*
                **  The two most significant bits of the SRCB indicate whether
                **  the record is a data record or a control record.
                */
                if ((*srcb & 0xc0) == 0x80)
                    {
                    blockType = BtHTBLK;
                    }

            // fall through
            case 0x90:                  // request to initiate stream
            case 0xa0:                  // permission to initiate stream
            case 0xc0:                  // acknowledge transmission complete
            case 0xd0:                  // ready to receive stream
            case 0xe0:                  // BCB sequence error
            case 0x9a:                  // command or message record
                if (ibLimit - ibp <= 0) // must be at least long enough to have an SCB
                    {
#if DEBUG
                    fprintf(npuNjeLog, "Port %02x: block too short for RCB (%02x)\n", pcbp->claPort, *rcb);
#endif
                    npuBipBufRelease(bp);

                    return NjeErrBlockTooShort;
                    }

                /*
                **  Flush the output buffer if this is not a data record and the buffer
                **  is not empty.
                */
                if ((blockType == BtHTMSG) && (obp > bp->data + BlkOffDbc + 1))
                    {
                    if (isRetransmission == FALSE)
                        {
                        npuNjeSendUplineBlock(pcbp, bp, obp, BtHTBLK);
                        blocksUploaded += 1;
                        bp              = npuBipBufGet();
                        }
                    obp     = bp->data + BlkOffDbc + 1;
                    obLimit = bp->data + MaxUplineBlockSize;
                    }

                /*
                **  Make two passes across the record. Calculate the total uncompressed
                **  length of the record in the first pass, and copy the uncompressed
                **  data to the output buffer in the second pass.  If the record won't
                **  fit into the current upline buffer, queue the buffer and start a new
                **  one.
                **
                **  Substrings are traversed until an SCB with value 0x00 is encountered.
                **  A 0x00 SCB indicates end of record.
                */

                /*  Pass 1: calculate total uncompressed record length */
                start  = ibp;
                recLen = 0;
                while (ibp < ibLimit && *ibp != 0x00)
                    {
                    scb = *ibp++;
                    switch (scb & 0xc0)
                    {
                    case 0x40: // terminate stream transmission
                        break;

                    case 0x80: // compressed string
                        recLen += scb & 0x1f;
                        if ((scb & 0x20) == 0x20)
                            {
                            ibp += 1;
                            }
                        break;

                    case 0xc0: // non-compressed string
                        recLen += scb & 0x3f;
                        ibp    += scb & 0x3f;
                        break;

                    default:
#if DEBUG
                        fprintf(npuNjeLog, "Port %02x: bad SCB (%02x) for RCB (%02x)\n", pcbp->claPort, scb, *rcb);
#endif
                        npuBipBufRelease(bp);

                        return NjeErrBadSCB;
                    }
                    }
                if (ibp >= ibLimit)
                    {
#if DEBUG
                    fprintf(npuNjeLog, "Port %02x: end of record SCB (00) missing for RCB (%02x)\n", pcbp->claPort, *rcb);
#endif
                    npuBipBufRelease(bp);

                    return NjeErrProtocolError;
                    }
                if (obp + recLen + 3 > obLimit)
                    {
                    if (isRetransmission == FALSE)
                        {
                        npuNjeSendUplineBlock(pcbp, bp, obp, BtHTBLK);
                        blocksUploaded += 1;
                        bp              = npuBipBufGet();
                        }
                    obp     = bp->data + BlkOffDbc + 1;
                    obLimit = bp->data + MaxUplineBlockSize;
                    }
                if (recLen == 0) // end of stream
                    {
                    blockType = BtHTMSG;
                    }
#if DEBUG
                if (recLen > 255)
                    {
                    fprintf(npuNjeLog, "Port %02x: upline record too long (%d > 255), will be truncated\n", pcbp->claPort, recLen);
                    }
#endif
                /*  Pass 2: copy substrings to output buffer */
                ibp    = start;
                *obp++ = (recLen < 256) ? (u8)recLen : 255;
                *obp++ = *rcb;
                *obp++ = *srcb;
                recLen = 0;
                while (ibp < ibLimit && *ibp != 0x00)
                    {
                    scb = *ibp++;
                    switch (scb & 0xc0)
                    {
                    case 0x80: // compressed string
                        len = scb & 0x1f;
                        if ((scb & 0x20) == 0x20)
                            {
                            while (len-- > 0 && recLen++ < 255)
                                {
                                *obp++ = *ibp;
                                }
                            ibp += 1;
                            }
                        else
                            {
                            while (len-- > 0 && recLen++ < 255)
                                {
                                *obp++ = EbcdicBlank;
                                }
                            }
                        break;

                    case 0xc0: // non-compressed string
                        len = scb & 0x3f;
                        while (len > 0 && recLen < 255)
                            {
                            *obp++  = *ibp++;
                            len    -= 1;
                            recLen += 1;
                            }
                        ibp += len;
                        break;

                    default:
                        break;
                    }
                    }
                ibp += 1; // advance past end of record SCB (0x00)

                /*
                **  If the block is type BtHTMSG, flush it upline.
                */
                if (blockType == BtHTMSG)
                    {
                    if (isRetransmission == FALSE)
                        {
                        npuNjeSendUplineBlock(pcbp, bp, obp, BtHTMSG);
                        blocksUploaded += 1;
                        bp              = npuBipBufGet();
                        }
                    obp     = bp->data + BlkOffDbc + 1;
                    obLimit = bp->data + MaxUplineBlockSize;
                    }
                break;

            default:
#if DEBUG
                fprintf(npuNjeLog, "Port %02x: invalid RCB (%02x)\n", pcbp->claPort, *ibp);
#endif
                npuBipBufRelease(bp);

                return NjeErrBadRCB;
                }
            if ((ibp < ibLimit) && (*ibp == 0x00)) // end of block
                {
                ibp += 1;
                if (obp > bp->data + BlkOffDbc + 1)
                    {
                    if (isRetransmission == FALSE)
                        {
                        npuNjeSendUplineBlock(pcbp, bp, obp, BtHTBLK);
                        blocksUploaded += 1;
                        bp              = npuBipBufGet();
                        }
                    obp     = bp->data + BlkOffDbc + 1;
                    obLimit = bp->data + MaxUplineBlockSize;
                    }
                break;
                }
            }
        }
    if ((obp > bp->data + BlkOffDbc + 1) && (isRetransmission == FALSE))
        {
        npuNjeSendUplineBlock(pcbp, bp, obp, BtHTBLK);
        blocksUploaded += 1;
        }
    else
        {
        npuBipBufRelease(bp);
        }

    return (blocksUploaded > 0) ? NjeStatusOk : NjeStatusNothingUploaded;
    }

#if DEBUG

/*--------------------------------------------------------------------------
**  Purpose:        Flush incomplete data line
**
**  Parameters:     Name        Description.
**
**  Returns:        nothing
**
**------------------------------------------------------------------------*/
static void npuNjeLogFlush(void)
    {
    if (npuNjeLogBytesCol > 0)
        {
        fputs(npuNjeLogBuf, npuNjeLog);
        fputc('\n', npuNjeLog);
        fflush(npuNjeLog);
        }
    npuNjeLogBytesCol = 0;
    memset(npuNjeLogBuf, ' ', LogLineLength);
    npuNjeLogBuf[LogLineLength] = '\0';
    }

/*--------------------------------------------------------------------------
**  Purpose:        Log a sequence of ASCII or EBCDIC bytes
**
**  Parameters:     Name        Description.
**                  bytes       pointer to sequence of bytes
**                  len         length of the sequence
**                  encoding    ASCII, EBCDIC, or DisplayCode
**
**  Returns:        nothing
**
**------------------------------------------------------------------------*/
static void npuNjeLogBytes(u8 *bytes, int len, CharEncoding encoding)
    {
    u8   ac;
    int  ascCol;
    u8   b;
    char hex[3];
    int  hexCol;
    int  i;

    ascCol = AsciiColumn(npuNjeLogBytesCol);
    hexCol = HexColumn(npuNjeLogBytesCol);

    for (i = 0; i < len; i++)
        {
        b = bytes[i];
        switch (encoding)
            {
        default:
        case ASCII:
            ac = b;
            break;

        case EBCDIC:
            ac = ebcdicToAscii[b];
            break;

        case DisplayCode:
            ac = (b < 0x40) ? cdcToAscii[b] : '.';
            break;
            }
        if ((ac < 0x20) || (ac >= 0x7f))
            {
            ac = '.';
            }
        sprintf(hex, "%02x", b);
        memcpy(npuNjeLogBuf + hexCol, hex, 2);
        hexCol += 3;
        npuNjeLogBuf[ascCol++] = ac;
        if (++npuNjeLogBytesCol >= 16)
            {
            npuNjeLogFlush();
            ascCol = AsciiColumn(npuNjeLogBytesCol);
            hexCol = HexColumn(npuNjeLogBytesCol);
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Log a stack trace
**
**  Parameters:     none
**
**  Returns:        nothing
**
**------------------------------------------------------------------------*/
static void npuNjePrintStackTrace(FILE *fp)
    {
#if defined(__APPLE__)
    void *callstack[128];
    int  i;
    int  frames;
    char **strs;

    frames = backtrace(callstack, 128);
    strs   = backtrace_symbols(callstack, frames);
    for (i = 1; i < frames; ++i)
        {
        fprintf(fp, "%s\n", strs[i]);
        }
    free(strs);
#endif
    }

#endif // DEBUG

/*---------------------------  End Of File  ------------------------------*/
