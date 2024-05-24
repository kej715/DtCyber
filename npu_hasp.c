/*--------------------------------------------------------------------------
**
**  Copyright (c) 2019, Kevin Jordan, Tom Hunter
**
**  Name: npu_hasp.c
**
**  Description:
**      Perform emulation of the HASP and Reverse HASP TIPs in an NPU
**      consisting of a CDC 2550 HCP running CCP.  The HASP TIP is used
**      by the RBF application.  RBF enables a NOS system to serve as
**      a host for HASP remote batch stations. The Reverse HASP TIP is
**      used by the TLF application.  TLF enables a NOS system to behave
**      as a HASP remote batch station.
**
**      This TIP implements HASP over TCP and is fully compatible
**      and interoperable with HASP over TCP implementations provided
**      by the Hercules IBM maintframe emulator and Prime 50 series
**      emulator.
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
#if defined(_WIN32)
#include <Windows.h>
#else
#include <sys/time.h>
#endif
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(__FreeBSD__)
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif 

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

#define BlockCushion        140
#define DeadPeerTimeout     15000
#define HaspPduHdrLen       5
#define HaspMaxPruDataSize  1280
#define HaspStartTimeout    (5 * 60 * 1000)
#define InBufThreshold      (HaspMaxPruDataSize + HaspPduHdrLen)
#define MaxRetries          5
#define PauseTimeout        100
#define RecvTimeout         5000
#define SendTimeout         100

#define SOH                 0x01
#define STX                 0x02
#define ETX                 0x03
#define DLE                 0x10
#define ITB                 0x1f
#define ETB                 0x26
#define ENQ                 0x2d
#define SYN                 0x32
#define EOT                 0x37
#define NAK                 0x3d
#define ACK0                0x70

#define DcBlank             055
#define EbcdicBlank         0x40

#define SRCB_GCR            0
#define SRCB_RTI            1
#define SRCB_PTI            2
#define SRCB_CI             3
#define SRCB_CO             4
#define SRCB_LP             5
#define SRCB_CP             6
#define SRCB_CR             7
#define SRCB_BAD_BCB        8

/*
**  Field name codes used in setting batch device parameters.
*/
#define FnDevTbsUpper       30 // TBS upper 3 bits
#define FnDevTbsLower       31 // TBS lower 8 bits
#define FnDevPW             35 // Page width
#define FnDevPL             36 // Page length
#define FnDevPrintTrain     76 // Print train type

/*
**  Field name codes used in setting batch file parameters.
*/
#define FnFileType          81 // File type
#define FnFileCC            82 // Carriage control
#define FnFileLace          83 // Lace card punching
#define FnFileLimUpper      84 // File limit upper byte
#define FnFileLimLower      85 // File limit lower byte
#define FnFilePunchLimit    86 // Punch limit

/*
**  -----------------------
**  Private Macro Functions
**  -----------------------
*/
#define isPostPrint(tp) ((tp)->params.fvTC == TcHASP)
#if DEBUG
#define HexColumn(x)    (3 * (x) + 4)
#define AsciiColumn(x)  (HexColumn(16) + 2 + (x))
#define LogLineLength   (AsciiColumn(16))
#endif

/*
**  -----------------------------------------
**  Private Typedef and Structure Definitions
**  -----------------------------------------
*/
typedef enum
    {
    DO26 = 0,
    DO29,
    ASC,
    TRANS6,
    TRANS8
    } FileType;

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
static int  npuHaspAppendOutput(Pcb *pcbp, u8 *data, int len);
static int  npuHaspAppendRecord(Pcb *pcbp, u8 *data, int len);
static void npuHaspCloseConnection(Pcb *pcbp);
static Scb *npuHaspFindStream(Pcb *pcbp, u8 streamId, u8 deviceType);
static Scb *npuHaspFindStreamWithOutput(Pcb *pcbp);
static Scb *npuHaspFindStreamWithPendingRTI(Pcb *pcbp);
static bool npuHaspFlushBuffer(Pcb *pcbp);
static int  npuHaspFlushPruFragment(Tcb *tp);
static int  npuHaspFlushPruPostPrintFragment(Tcb *tp);
static int  npuHaspFlushPruPrePrintFragment(Tcb *tp);
static void npuHaspFlushUplineData(Scb *scbp, bool isEof);
static void npuHaspProcessFormatControl(Pcb *pcbp);
static void npuHaspReleaseLastBlockSent(Pcb *pcbp);
static void npuHaspResetScb(Scb *scbp);
static void npuHaspResetSendDeadline(Tcb *tp);
static int  npuHaspSend(Pcb *pcbp, u8 *data, int len);
static int  npuHaspSendBlockHeader(Tcb *tp);
static int  npuHaspSendBlockTrailer(Tcb *tp);
static bool npuHaspSendDownlineData(Tcb *tp);
static int  npuHaspSendEofRecord(Tcb *tp);
static int  npuHaspSendRecordHeader(Tcb *tp, u8 srcb);
static int  npuHaspSendRecordStrings(Tcb *tp, u8 *data, int len);
static void npuHaspSendSignonRecord(Tcb *tp, u8 *data, int len);
static void npuHaspSendUplineData(Tcb *tp, u8 *data, int len);
static void npuHaspSendUplineEoiAcctg(Tcb *tp, u8 sfc);
static void npuHaspSendUplineEOS(Tcb *tp);
static void npuHaspStageUplineData(Scb *scbp, u8 *data, int len);
static void npuHaspTransmitQueuedBlocks(Tcb *tp);
static u8   npuHaspTranslateSrcbToFe(u8 cc);

#if DEBUG
static void npuHaspLogBytes(u8 *bytes, int len, CharEncoding encoding);
static void npuHaspLogDevParam(Tcb *tp, u8 fn, u8 fv);
static void npuHaspLogFileParam(Tcb *tp, u8 fn, u8 fv);
static void npuHaspLogFlush(void);
static void npuHaspPrintStackTrace(FILE *fp);

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
static u8 blank        [] = { EbcdicBlank };
static u8 blockHeader  [] = { SYN, SYN, SYN, SYN, DLE, STX };
static u8 blockTrailer [] = { 0x00, DLE, ETB };
static u8 enqIndication[] = { SOH, ENQ };
static u8 ackIndication[] = { SYN, SYN, SYN, SYN, DLE, ACK0 };
static u8 nakIndication[] = { SYN, SYN, SYN, SYN, NAK };
static u8 ptiRecord    [] = { 0xa0, 0x80, 0x00 };
static u8 rtiRecord    [] = { 0x90, 0x80, 0x00 };

static u8 dcEoi        [] = { 050, 047, 005, 017, 011 }; //  /*EOI
static u8 dcEor        [] = { 050, 047, 005, 017, 022 }; //  /*EOR

#if DEBUG
static FILE *npuHaspLog = NULL;
static char npuHaspLogBuf[LogLineLength + 1];
static int  npuHaspLogBytesCol = 0;
#endif

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
void npuHaspTryOutput(Pcb *pcbp)
    {
    u64 currentTime;
    int i;
    Scb *scbp;

    /*
     *  Send queued blocks upline
     */
    npuHaspTransmitQueuedBlocks(pcbp->controls.hasp.consoleStream.tp);
    for (i = 0; i < MaxHaspStreams; ++i)
        {
        npuHaspTransmitQueuedBlocks(pcbp->controls.hasp.readerStreams[i].tp);
        npuHaspTransmitQueuedBlocks(pcbp->controls.hasp.printStreams [i].tp);
        npuHaspTransmitQueuedBlocks(pcbp->controls.hasp.punchStreams [i].tp);
        }

    /*
     *  Process HASP protocol output
     */ 
    currentTime = getMilliseconds();

    switch (pcbp->controls.hasp.majorState)
        {
    case StHaspMajorInit:
        if (pcbp->ncbp->connType == ConnTypeHasp)
            {
            pcbp->controls.hasp.minorState   = StHaspMinorRecvSOH;
            pcbp->controls.hasp.majorState   = StHaspMajorWaitENQ;
            pcbp->controls.hasp.recvDeadline = currentTime + HaspStartTimeout;
            }
        break;

    case StHaspMajorRecvData:
        if ((pcbp->controls.hasp.recvDeadline < currentTime)
            && (pcbp->controls.hasp.recvDeadline > 0))
            {
            /*
            **  Too much time has elapsed without receiving anything
            **  from the peer, so take an appropriate action depending
            **  upon the current minor state.
            */
#if DEBUG
            fprintf(npuHaspLog, "Port %02x: receive timeout\n", pcbp->claPort);
#endif
            if (currentTime - pcbp->controls.hasp.lastRecvTime > DeadPeerTimeout)
                {
#if DEBUG
                fprintf(npuHaspLog, "Port %02x: peer not responding, closing connection\n", pcbp->claPort);
#endif
                npuHaspCloseConnection(pcbp);
                pcbp->controls.hasp.majorState = StHaspMajorInit;

                return;
                }
            if ((pcbp->controls.hasp.minorState >= StHaspMinorRecvENQ_Resp)
                && (pcbp->controls.hasp.minorState <= StHaspMinorRecvACK0))
                {
                /*
                **  Timeout awaiting response to ENQ, so return to SendENQ
                **  state to re-send ENQ.
                */
                pcbp->controls.hasp.majorState = StHaspMajorSendENQ;
                }
            else
                {
                /*
                **  Timeout after initial connection has been established, so
                **  send NAK and wait for a normal frame.
                */
                npuHaspAppendOutput(pcbp, nakIndication, sizeof(nakIndication));
                if (npuHaspFlushBuffer(pcbp) == FALSE)
                    {
                    pcbp->controls.hasp.majorState = StHaspMajorSendData;
                    pcbp->controls.hasp.minorState = StHaspMinorRecvBOF;
                    }
                }
            }
        break;

    case StHaspMajorSendData:
        /*
        **  If the last exchange between peers involved idle frames,
        **  then wait until the idle timeout expires before sending
        **  a new frame.
        */
        if (pcbp->controls.hasp.sendDeadline > currentTime)
            {
            return;
            }

        /*
        **  If the port's output buffer is allocated, send the data
        **  contained in it.
        */
        if (pcbp->controls.hasp.outBuf != NULL)
            {
            if (npuHaspFlushBuffer(pcbp))
                {
                pcbp->controls.hasp.majorState = StHaspMajorRecvData;
                }

            return;
            }

        /*
        **  Check for streams with pending requests to initiate transmission.
        */
        scbp = npuHaspFindStreamWithPendingRTI(pcbp);
        if (scbp != NULL)
            {
            scbp->isWaitingPTI = FALSE;
            scbp->recordCount  = 0;
            scbp->lastSRCB     = 0;
            switch (scbp->tp->deviceType)
                {
            case DtCR:
                ptiRecord[1] = 0x80 | (scbp->tp->streamId << 4) | 3;
                break;

            case DtLP:
                ptiRecord[1] = 0x80 | (scbp->tp->streamId << 4) | 4;
                break;

            case DtCP:
                ptiRecord[1] = 0x80 | (scbp->tp->streamId << 4) | 5;
                break;
            }
            npuHaspAppendRecord(pcbp, ptiRecord, sizeof(ptiRecord));
            npuHaspAppendOutput(pcbp, blockTrailer, sizeof(blockTrailer));
            if (npuHaspFlushBuffer(pcbp))
                {
                pcbp->controls.hasp.majorState = StHaspMajorRecvData;
                }

            return;
            }

        /*
        **  Attempt to find a stream having data to transmit.
        */
        scbp = pcbp->controls.hasp.currentOutputStream;
        if (scbp == NULL)
            {
            scbp = npuHaspFindStreamWithOutput(pcbp);
            if (scbp == NULL || scbp->isTerminateRequested)
                {
                /*
                **  No streams have output to send, so send ACK0 frame.  However, if the
                **  last frame received from the peer was also an ACK0 frame, this indicates
                **  that both sides are idle, so set a delay that will prevent ACK0 frames
                **  from being exchanged too furiously.
                */
                npuHaspAppendOutput(pcbp, ackIndication, sizeof(ackIndication));
                if (npuHaspFlushBuffer(pcbp))
                    {
                    pcbp->controls.hasp.majorState = StHaspMajorRecvData;
                    if (pcbp->controls.hasp.lastRecvFrameType == ACK0)
                        {
                        pcbp->controls.hasp.sendDeadline = currentTime + SendTimeout;
                        }
                    if (scbp != NULL && scbp->isTerminateRequested)
                        {
                        npuHaspSendUplineEoiAcctg(scbp->tp, SfcIOT);
                        scbp->isTerminateRequested = FALSE;
                        }
                    }

                return;
                }
            else
                {
                pcbp->controls.hasp.currentOutputStream = scbp;
                }
            }

        /*
        **  A stream with data to send has been identified.  If the stream has been granted
        **  permission to initiate transmission already, send the data.  Otherwise, send a
        **  request to initiate transmission.
        */
        switch (scbp->state)
            {
        case StHaspStreamReady:
        case StHaspStreamSendRTI:
        case StHaspStreamWaitAcctng:
            if (npuHaspSendDownlineData(scbp->tp))
                {
                pcbp->controls.hasp.majorState          = StHaspMajorRecvData;
                pcbp->controls.hasp.currentOutputStream = NULL;
                }
            break;

        default:
#if DEBUG
            fprintf(npuHaspLog, "Port %02x: invalid stream state %d on stream %u (%.7s)\n",
                    pcbp->claPort, scbp->state, scbp->tp->streamId, scbp->tp->termName);
#endif
            break;
        }
        break;

    case StHaspMajorSendENQ:
        if (npuHaspSend(pcbp, enqIndication, sizeof(enqIndication)) >= sizeof(enqIndication))
            {
            pcbp->controls.hasp.majorState = StHaspMajorRecvData;
            pcbp->controls.hasp.minorState = StHaspMinorRecvENQ_Resp;
            }
        break;

    case StHaspMajorWaitENQ:
        if (pcbp->controls.hasp.recvDeadline < currentTime)
            {
            /*
            **  Too much time has elapsed without receiving the initial
            **  SOH ENQ sequence from the peer, so close the connection.
            */
#if DEBUG
            fprintf(npuHaspLog, "Port %02x: HASP startup timeout\n", pcbp->claPort);
#endif
            npuHaspCloseConnection(pcbp);
            pcbp->controls.hasp.majorState = StHaspMajorInit;

            return;
            }
        break;

    case StHaspMajorWaitSignon:
        if (pcbp->controls.hasp.outBuf != NULL)
            {
            if (npuHaspFlushBuffer(pcbp))
                {
                pcbp->controls.hasp.majorState = StHaspMajorRecvData;
                pcbp->controls.hasp.minorState = StHaspMinorRecvBOF;
                }
            else
                {
                pcbp->controls.hasp.majorState = StHaspMajorSendSignon;
                }
            }
        break;

    case StHaspMajorSendSignon:
        if (npuHaspFlushBuffer(pcbp))
            {
            pcbp->controls.hasp.majorState = StHaspMajorRecvData;
            pcbp->controls.hasp.minorState = StHaspMinorRecvBOF;
            }
        break;

    default:
        fprintf(stderr, "(npu_hasp) Port %02x: invalid  major state: %02x\n",
                pcbp->claPort, pcbp->controls.hasp.majorState);
        pcbp->controls.hasp.majorState = StHaspMajorInit;
        break;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Process downline data from host.
**
**  Parameters:     Name        Description.
**                  tp          pointer to TCB
**                  bp          buffer with downline data message.
**                  last        last buffer.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuHaspProcessDownlineData(Tcb *tp, NpuBuffer *bp, bool last)
    {
    u8  *blk;
    int blockLen;
    int blocksQueued;
    u8  blockType;
    u8  c;
    u8  dbc;
    u8  *fp;
    int len;
    Pcb *pcbp;
    u8  recordLen;
    u8  *recordStart;
    Scb *scbp;
    u8  srcb;

    pcbp = tp->pcbp;
    blk  = bp->data + BlkOffData;
    len  = bp->numBytes - BlkOffData;
    dbc  = *blk++; // extract data block clarifier
    len -= 1;

#if DEBUG
    blockType = bp->data[BlkOffBTBSN] & BlkMaskBT;
    fprintf(npuHaspLog, "Port %02x: downline data received from host for stream %u (%.7s), block type %u, block len %d, dbc %02x\n",
            pcbp->claPort, tp->streamId, tp->termName, blockType, bp->numBytes, dbc);
    if ((dbc & DbcPRU) != 0)
        {
        npuHaspLogBytes(bp->data, bp->numBytes,
            (tp->scbp != NULL && tp->scbp->params.fvFileType == ASC) ? ASCII : DisplayCode);
        }
    else if ((dbc & DbcTransparent) == 0)
        {
        npuHaspLogBytes(bp->data, bp->numBytes, ASCII);
        }
    else
        {
        npuHaspLogBytes(bp->data, bp->numBytes, DisplayCode);
        }
    npuHaspLogFlush();
#endif

    scbp = tp->scbp;

    if (scbp == NULL)
        {
        return;
        }

    if (scbp->state == StHaspStreamInit)
        {
        scbp->recordCount = 0;
        scbp->lastSRCB    = 0;
        switch (tp->deviceType)
            {
        case DtCR:
            rtiRecord[1] = 0x80 | (tp->streamId << 4) | 3;
            break;

        case DtLP:
            rtiRecord[1]                = 0x80 | (tp->streamId << 4) | 4;
            scbp->pruFragmentSize       = 0;
            scbp->isPruFragmentComplete = TRUE;
            scbp->pruFragment2          = NULL;
            break;

        case DtCP:
        case DtPLOTTER:
            rtiRecord[1]                = 0x80 | (tp->streamId << 4) | 5;
            scbp->pruFragmentSize       = 0;
            scbp->isPruFragmentComplete = FALSE;
            break;

        default:
#if DEBUG
            fprintf(npuHaspLog, "Port %02x: invalid device type %u on RTI\n",
                    pcbp->claPort, tp->deviceType);
#endif
            break;
            }
        blockLen = npuHaspSendBlockHeader(tp);
        npuNetSend(tp, rtiRecord, sizeof(rtiRecord));
        blockLen += sizeof(rtiRecord);
        blockLen += npuHaspSendBlockTrailer(tp);
        scbp->state = StHaspStreamSendRTI;
        npuBipQueueAppend(npuBipBufGet(), &tp->outputQ);
        npuHaspResetSendDeadline(tp);
        }

    if ((dbc & DbcPRU) != 0)
        {
        /*
        **  Process PRU data.
        **
        **  RBF sends PRU data on print and punch streams. PRU data can be encoded
        **  in ASCII or Display Code. Display Code characters are right justified
        **  in 8-bit bytes. Records are terminated by 0xff bytes in both cases.
        **
        **  Additionally, the Data Block Clarifier byte at the beginning of every
        **  downline block indicates whether the block represents EOR, EOI, or
        **  an accounting record (both EOR and EOI bits set).
        **
        */
        if ((len < 1) && ((dbc & DbcAcctg) != 0) && ((dbc & DbcAcctg) != DbcAcctg))
            {
            /*
            **  EOR or EOI with no data, and not accounting record, so
            **  flush PRU fragment data, if any, or queue an empty block
            **  that triggers acknowledgement, if necessary.
            */
            if (scbp->pruFragmentSize > 0)
                {
                npuHaspSendBlockHeader(tp);
                npuHaspFlushPruFragment(tp);
                npuHaspSendBlockTrailer(tp);
                }
            else
                {
                npuBipQueueAppend(npuBipBufGet(), &tp->outputQ);
                }
            blockType = (dbc & DbcEOI) != 0 ? BtHTMSG : BtHTBLK;
            npuNetQueueAck(tp, (bp->data[BlkOffBTBSN] & (BlkMaskBSN << BlkShiftBSN)) | blockType);
            npuHaspResetSendDeadline(tp);

            return;
            }
        blockLen = 0;
        blocksQueued = 0;
        while (len > 0)
            {
            if (scbp->isPruFragmentComplete)
                {
                /*
                **  A complete record has been collected, so flush the record
                **  to the output stream.
                */
                if (blockLen == 0) blockLen = npuHaspSendBlockHeader(tp);
                blockLen += npuHaspFlushPruFragment(tp);
                if (blockLen > pcbp->controls.hasp.blockSize - BlockCushion)
                    {
                    /*
                    **  Sufficient data has been staged to fill a block,
                    **  so complete the block, queue it for transmission,
                    **  and start a new block.
                    */
                    blockLen += npuHaspSendBlockTrailer(tp);
                    npuBipQueueAppend(npuBipBufGet(), &tp->outputQ);
                    blocksQueued += 1;
                    blockLen = 0;
                    }
                }
            if (scbp->params.fvFileType == ASC)
                {
                fp = scbp->pruFragment + scbp->pruFragmentSize;
                while (len-- > 0)
                    {
                    c = *blk++;
                    if (c == 0xff)
                        {
                        scbp->isPruFragmentComplete = TRUE;
                        break;
                        }
                    else if (scbp->pruFragmentSize < MaxBuffer)
                        {
                        *fp++ = asciiToEbcdic[c];
                        scbp->pruFragmentSize += 1;
                        }
                    }
                }
            else // Display Code
                {
                fp = scbp->pruFragment + scbp->pruFragmentSize;
                while (len-- > 0)
                    {
                    c = *blk++;
                    if (c == 0xff)
                        {
                        scbp->isPruFragmentComplete = TRUE;
                        break;
                        }
                    else if (scbp->pruFragmentSize < MaxBuffer)
                        {
                        *fp++ = asciiToEbcdic[cdcToAscii[c]];
                        scbp->pruFragmentSize += 1;
                        }
                    }
                }
            }
        if (scbp->isPruFragmentComplete)
            {
            if (blockLen == 0) blockLen = npuHaspSendBlockHeader(tp);
            blockLen += npuHaspFlushPruFragment(tp);
            if (blockLen > pcbp->controls.hasp.blockSize - BlockCushion)
                {
                blockLen += npuHaspSendBlockTrailer(tp);
                npuBipQueueAppend(npuBipBufGet(), &tp->outputQ);
                blocksQueued += 1;
                blockLen = 0;
                }
            }
        if ((dbc & DbcAcctg) == DbcAcctg) // EOI accounting record
            {
            /*
            **  If the PRU contains an accounting record, then it is the final
            **  PRU of a file, so flush any data that might have been collected,
            **  and send a HASP end-of-file indication.
            */
            if (scbp->pruFragmentSize > 0)
                {
                if (blockLen == 0) blockLen = npuHaspSendBlockHeader(tp);
                blockLen += npuHaspFlushPruFragment(tp);
                }
            if (blockLen == 0) blockLen = npuHaspSendBlockHeader(tp);
            blockLen += npuHaspSendEofRecord(tp);
            }
        if (blockLen > 0)
            {
            blockLen += npuHaspSendBlockTrailer(tp);
            blocksQueued += 1;
            }
        if (blocksQueued > 0)
            {
            blockType = (dbc & DbcEOI) != 0 ? BtHTMSG : BtHTBLK;
            npuNetQueueAck(tp, (bp->data[BlkOffBTBSN] & (BlkMaskBSN << BlkShiftBSN)) | blockType);
            npuHaspResetSendDeadline(tp);
            }
        }
    else if ((dbc & DbcTransparent) == 0)
        {
        /*
        **  Process normal (non-transparent) data.
        **
        **  NVF and RBF send normal data on console streams.  Records of normal
        **  streams are delimited by ASCII <US> characters.
        */
        if ((pcbp->controls.hasp.isSignedOn == FALSE) && (pcbp->ncbp->connType == ConnTypeRevHasp))
            {
#if DEBUG
            fprintf(npuHaspLog, "Port %02x: not signed on yet, data discarded from stream %u (%.7s)\n",
                    pcbp->claPort, tp->streamId, tp->termName);
#endif
            npuTipNotifySent(tp, bp->data[BlkOffBTBSN]);

            return;
            }
        blockType = bp->data[BlkOffBTBSN] & BlkMaskBT;
        blockLen  = npuHaspSendBlockHeader(tp);
        while (len > 0)
            {
            recordStart = blk;
            while (len > 0)
                {
                c = *blk;
                if (c == ChrUS)
                    {
                    break;
                    }
                else
                    {
                    *blk++ = asciiToEbcdic[c];
                    len   -= 1;
                    }
                }
            srcb = 0;
            if (blk > recordStart)
                {
                if (tp->deviceType == DtCONSOLE)
                    {
                    recordStart += 1;  // discard format effector
                    }
                else if (tp->deviceType == DtLP)
                    {
                    srcb = 0x01;
                    }
                }
            blockLen += npuHaspSendRecordHeader(tp, srcb);
            recordLen = blk - recordStart;
            if (recordLen > 0)
                {
                blockLen += npuHaspSendRecordStrings(tp, recordStart, recordLen);
                }
            else
                {
                blockLen += npuHaspSendRecordStrings(tp, blank, sizeof(blank));
                }
            if (len > 0)
                {
                blk += 1;
                len -= 1;
                }
            if ((blockLen > pcbp->controls.hasp.blockSize - BlockCushion) && (len > 0))
                {
                blockLen += npuHaspSendBlockTrailer(tp);
                npuBipQueueAppend(npuBipBufGet(), &tp->outputQ);
                blockLen = npuHaspSendBlockHeader(tp);
                }
            }
        if ((blockType == BtHTMSG) && (tp->deviceType != DtCONSOLE))
            {
            blockLen += npuHaspSendEofRecord(tp);
            }
        npuHaspSendBlockTrailer(tp);
        npuNetQueueAck(tp, bp->data[BlkOffBTBSN]);
        npuHaspResetSendDeadline(tp);
        }
    else if (pcbp->controls.hasp.majorState == StHaspMajorWaitSignon)
        {
        /*
        **  The first record sent on a Reverse HASP console connection should be a signon record
        **  from TLF. A signon record from TLF begins with <01> (transparency control character?).
        */
        recordLen = (*blk++) - 1;
        len      -= 1;
        if (len > 0)
            {
            blk       += 1;
            recordLen -= 1;
            pcbp->controls.hasp.isSignedOn  = TRUE;
            pcbp->controls.hasp.downlineBSN = 0;
            npuHaspSendSignonRecord(tp, blk, recordLen);
            }
        npuTipNotifySent(tp, bp->data[BlkOffBTBSN]);
        }
    else
        {
        /*
        **  Process transparent data.
        **
        **  TLF sends transparent data on reader and console streams. Each record is prefaced
        **  by a byte specifying the record length, and the record length includes the length
        **  byte itself.
        */
        blockType = bp->data[BlkOffBTBSN] & BlkMaskBT;
        blockLen  = npuHaspSendBlockHeader(tp);
        while (len > 0)
            {
            recordLen = (*blk++) - 1;
            len      -= 1;
            blockLen += npuHaspSendRecordHeader(tp, 0);
            if (recordLen > 0)
                {
                blockLen += npuHaspSendRecordStrings(tp, blk, recordLen);
                }
            else
                {
                blockLen += npuHaspSendRecordStrings(tp, blank, sizeof(blank));
                }
            blk      += recordLen;
            len      -= recordLen;
            if ((blockLen > pcbp->controls.hasp.blockSize - BlockCushion) && (len > 0))
                {
                blockLen += npuHaspSendBlockTrailer(tp);
                npuBipQueueAppend(npuBipBufGet(), &tp->outputQ);
                blockLen = npuHaspSendBlockHeader(tp);
                }
            }
        if ((blockType == BtHTMSG) && (tp->deviceType != DtCONSOLE))
            {
            blockLen += npuHaspSendEofRecord(tp);
            }
        blockLen += npuHaspSendBlockTrailer(tp);
        npuNetQueueAck(tp, bp->data[BlkOffBTBSN]);
        npuHaspResetSendDeadline(tp);
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
void npuHaspProcessUplineData(Pcb *pcbp)
    {
    int blockLen;
    u8  blockSeqNum;
    u8  buf[128];
    u8  ch;
    u8  deviceType;
    u8  *dp;
    int i;
    int len;
    u8  mask;
    int numBytes;
    u8  recordType;
    u8  retries;
    Scb *scbp;
    u8  streamId;

    dp  = pcbp->inputData;
    len = pcbp->inputCount;

#if DEBUG
    fprintf(npuHaspLog, "Port %02x: received from terminal\n", pcbp->claPort);
    npuHaspLogBytes(dp, len, EBCDIC);
    npuHaspLogFlush();
#endif

    if (len > 0)
        {
        pcbp->controls.hasp.lastRecvTime = getMilliseconds();
        }

    /*
    **  Process HASP protocol.
    */
    while (len--)
        {
        ch = *dp++;
        switch (pcbp->controls.hasp.minorState)
            {
        /*
        **  Search for the beginning of a data frame. Read and discard leading SYN bytes. DLE begins
        **  a data frame, SOH re/initiates a connection from a station, and NAK indicates that the
        **  previously sent frame has been rejected.
        */
        case StHaspMinorRecvBOF:
            switch (ch)
            {
            case SYN:
                // Do nothing, continue searching for beginning of frame
                break;

            case DLE:
                npuHaspReleaseLastBlockSent(pcbp);
                pcbp->controls.hasp.minorState = StHaspMinorRecvSTX;
                break;

            case NAK:
                pcbp->controls.hasp.lastRecvFrameType = ch;
                if (pcbp->controls.hasp.lastBlockSent != NULL)
                    {
                    if (pcbp->controls.hasp.outBuf != NULL)
                        {
                        npuBipBufRelease(pcbp->controls.hasp.outBuf);
                        }
                    retries = pcbp->controls.hasp.retries + 1;
                    if (retries < MaxRetries)
                        {
                        pcbp->controls.hasp.outBuf        = pcbp->controls.hasp.lastBlockSent;
                        pcbp->controls.hasp.lastBlockSent = NULL;
                        if (npuHaspFlushBuffer(pcbp))
                            {
                            pcbp->controls.hasp.majorState = StHaspMajorRecvData;
                            }
                        else
                            {
                            pcbp->controls.hasp.majorState = StHaspMajorSendData;
                            }
                        }
                    else
                        {
#if DEBUG
                        fprintf(npuHaspLog, "Port %02x: maximum retry attempts exceeded\n", pcbp->claPort);
#endif
                        npuHaspReleaseLastBlockSent(pcbp);
                        npuHaspCloseConnection(pcbp);
                        pcbp->controls.hasp.majorState = StHaspMajorInit;

                        return;
                        }
                    }
                break;

            case SOH:
                npuHaspReleaseLastBlockSent(pcbp);
                pcbp->controls.hasp.minorState = StHaspMinorRecvENQ;
                break;

            default:
#if DEBUG
                fprintf(npuHaspLog, "Port %02x: expected SYN, DLE, or SOH, received <%02x>\n",
                        pcbp->claPort, ch);
#endif
                break;
            }
            break;

        /*
        **  Read and process the byte following a DLE byte. It should be either
        **  STX indicating start of block or ACK0 indicating idle frame.
        */
        case StHaspMinorRecvSTX:
            pcbp->controls.hasp.lastRecvFrameType = ch;
            switch (ch)
            {
            case STX:
                pcbp->controls.hasp.minorState = StHaspMinorRecvBCB;
                break;

            case ACK0:
                pcbp->controls.hasp.majorState = StHaspMajorSendData;
                pcbp->controls.hasp.minorState = StHaspMinorRecvBOF;
                break;

            default:
#if DEBUG
                fprintf(npuHaspLog, "Port %02x: expected ACK0 or STX, received <%02x>\n",
                        pcbp->claPort, ch);
#endif
                pcbp->controls.hasp.minorState = StHaspMinorRecvBOF;
                break;
            }
            break;

        /*
        **  Wait for response to SOH-ENQ. Read and discard leading SYN bytes.
        **  The next non-SYN should be a DLE followed by ACK0.
        */
        case StHaspMinorRecvENQ_Resp:
            switch (ch)
            {
            case SYN:
                // Do nothing, continue waiting for response
                break;

            case DLE:
                npuHaspReleaseLastBlockSent(pcbp);
                pcbp->controls.hasp.minorState = StHaspMinorRecvACK0;
                break;

            default:
#if DEBUG
                fprintf(npuHaspLog, "Port %02x: expected DLE, received <%02x>\n",
                        pcbp->claPort, ch);
#endif
                break;
            }
            break;

        /*
        **  Read and process the byte following a DLE byte during initial
        **  connection creation.  It should be ACK0 acknowledging ENQ.
        */
        case StHaspMinorRecvACK0:
            pcbp->controls.hasp.lastRecvFrameType = ch;
            switch (ch)
            {
            case ACK0:
                pcbp->controls.hasp.majorState = StHaspMajorWaitSignon;
                break;

            default:
#if DEBUG
                fprintf(npuHaspLog, "Port %02x: expected ACK0, received <%02x>\n",
                        pcbp->claPort, ch);
#endif
                break;
            }
            break;

        /*
        **  Read bytes until SOH detected.  This is the initial state of a
        **  HASP connection. The HASP host should send SOH ENQ to initiate
        **  the connection.
        */
        case StHaspMinorRecvSOH:
            if (ch == SOH)
                {
                pcbp->controls.hasp.minorState = StHaspMinorRecvENQ;
                }
            break;

        /*
        **  Read and process the byte following an SOH byte. It should be ENQ.
        **  If not, return to SOH state.
        */
        case StHaspMinorRecvENQ:
            switch (ch)
            {
            case ENQ:
                if (pcbp->controls.hasp.consoleStream.tp != NULL)
                    {
                    pcbp->controls.hasp.minorState = StHaspMinorRecvBOF;
                    npuHaspAppendOutput(pcbp, ackIndication, sizeof(ackIndication));
                    if (npuHaspFlushBuffer(pcbp))
                        {
                        pcbp->controls.hasp.majorState = StHaspMajorRecvData;
                        }
                    else
                        {
                        pcbp->controls.hasp.majorState = StHaspMajorSendData;
                        }
                    }
                else
                    {
                    pcbp->controls.hasp.minorState = StHaspMinorRecvSOH;
#if DEBUG
                    fprintf(npuHaspLog, "Port %02x: port not configured yet, ENQ ignored\n",
                            pcbp->claPort);
#endif
                    }
                break;

            case SOH:
                // remain in this state
                break;

            default:
                pcbp->controls.hasp.minorState = StHaspMinorRecvSOH;
                break;
            }
            break;

        /*
        **  Read and process Block Control Byte.
        */
        case StHaspMinorRecvBCB:
            if ((ch & 0x80) != 0)
                {
                blockSeqNum = ch & 0x0f;
                switch ((ch >> 4) & 0x07)
                {
                case 0x00: // Normal Block
                    if (blockSeqNum == ((pcbp->controls.hasp.uplineBSN + 1) & 0x0f))
                        {
                        pcbp->controls.hasp.uplineBSN  = blockSeqNum;
                        pcbp->controls.hasp.minorState = StHaspMinorRecvFCS1;
                        }
                    else
                        {
#if DEBUG
                        fprintf(npuHaspLog, "Port %02x: expected BSN %u, received %u\n",
                                pcbp->claPort, (pcbp->controls.hasp.uplineBSN + 1) & 0x0f, blockSeqNum);
#endif
                        // TODO: send BAD BCB indication
                        pcbp->controls.hasp.minorState = StHaspMinorRecvDLE2;
                        }
                    break;

                case 0x01: // Bypass sequence count validation
                    pcbp->controls.hasp.minorState = StHaspMinorRecvFCS1;
                    break;

                case 0x02: // Reset expected block sequence count to this one
                    pcbp->controls.hasp.uplineBSN  = (blockSeqNum - 1) & 0x0f;
                    pcbp->controls.hasp.minorState = StHaspMinorRecvFCS1;
                    break;

                default:
#if DEBUG
                    fprintf(npuHaspLog, "Port %02x: unrecognized BCB type: <%02x>\n",
                            pcbp->claPort, ch);
#endif
                    pcbp->controls.hasp.minorState = StHaspMinorRecvDLE2;
                    break;
                }
                }
            else
                {
#if DEBUG
                fprintf(npuHaspLog, "Port %02x: invalid BCB byte: <%02x>\n",
                        pcbp->claPort, ch);
#endif
                pcbp->controls.hasp.minorState = StHaspMinorRecvDLE2;
                }
            break;

        /*
        **  Read the first byte of Function Control Sequence.
        */
        case StHaspMinorRecvFCS1:
            if ((ch & 0x80) != 0)
                {
                pcbp->controls.hasp.pauseAllOutput = (ch & 0x40) != 0;
                pcbp->controls.hasp.fcsMask        = (ch & 0x0f) << 4;
                pcbp->controls.hasp.minorState     = StHaspMinorRecvFCS2;
                if (pcbp->controls.hasp.pauseAllOutput)
                    {
                    pcbp->controls.hasp.pauseDeadline = getMilliseconds() + PauseTimeout;
                    }
                }
            else
                {
#if DEBUG
                fprintf(npuHaspLog, "Port %02x: invalid FCS1 byte: <%02x>\n",
                        pcbp->claPort, ch);
#endif
                pcbp->controls.hasp.minorState = StHaspMinorRecvDLE2;
                }
            break;

        /*
        **  Read the second byte of Function Control Sequence.
        */
        case StHaspMinorRecvFCS2:
            if ((ch & 0x80) != 0)
                {
                scbp           = npuHaspFindStream(pcbp, 0, DtCONSOLE);
                scbp->tp->xoff = (ch & 0x40) == 0;
                pcbp->controls.hasp.fcsMask |= ch & 0x0f;
                mask = 0x80;
                if (pcbp->ncbp->connType == ConnTypeHasp)
                    {
                    for (streamId = 1; streamId <= 8; streamId++)
                        {
                        scbp = npuHaspFindStream(pcbp, streamId, DtLP);
                        if (scbp != NULL)
                            {
                            scbp->tp->xoff = (pcbp->controls.hasp.fcsMask & mask) == 0;
                            }
                        scbp = npuHaspFindStream(pcbp, 9 - streamId, DtCP);
                        if (scbp != NULL)
                            {
                            scbp->tp->xoff = (pcbp->controls.hasp.fcsMask & mask) == 0;
                            }
                        mask >>= 1;
                        }
                    }
                else // pcbp->ncbp->connType == ConnTypeRevHasp
                    {
                    for (streamId = 1; streamId <= 8; streamId++)
                        {
                        scbp = npuHaspFindStream(pcbp, streamId, DtCR);
                        if (scbp != NULL)
                            {
                            scbp->tp->xoff = (pcbp->controls.hasp.fcsMask & mask) == 0;
                            }
                        mask >>= 1;
                        }
                    }
                pcbp->controls.hasp.minorState = StHaspMinorRecvRCB;
                }
            else
                {
#if DEBUG
                fprintf(npuHaspLog, "Port %02x: invalid FCS2 byte: <%02x>\n",
                        pcbp->claPort, ch);
#endif
                pcbp->controls.hasp.minorState = StHaspMinorRecvDLE2;
                }
            break;

        /*
        **  Read and process Record Control Byte.
        */
        case StHaspMinorRecvRCB:
            pcbp->controls.hasp.designatedStream = NULL;
            pcbp->controls.hasp.minorState       = StHaspMinorRecvSRCB;
            if ((ch & 0x80) == 0) // end of transmission block
                {
                pcbp->controls.hasp.minorState = StHaspMinorRecvDLE1;
                }
            else
                {
                recordType = ch & 0x0f;
                streamId   = (ch >> 4) & 0x07;
                switch (recordType)
                {
                case 0x00:            // Control record
                    switch (streamId) // stream id is control info for control record
                    {
                    case 1:           // Request to initiate a function transmission
                        pcbp->controls.hasp.sRCBType = SRCB_RTI;
                        break;

                    case 2: // Permission to initiate a function transmission
                        pcbp->controls.hasp.sRCBType = SRCB_PTI;
                        break;

                    case 6: // Bad BCB on last block
                        pcbp->controls.hasp.sRCBType = SRCB_BAD_BCB;
                        break;

                    case 7: // General Control Record (type indicated in SRCB)
                        pcbp->controls.hasp.sRCBType = SRCB_GCR;
                        break;

                    default:
#if DEBUG
                        fprintf(npuHaspLog, "Port %02x: unrecognized control info in RCB: %u\n",
                                pcbp->claPort, (ch >> 4) & 0x07);
#endif
                        pcbp->controls.hasp.minorState = StHaspMinorRecvDLE2;
                        break;
                    }
                    continue;

                case 0x01: // Operator message display request
                    deviceType = DtCONSOLE;
                    pcbp->controls.hasp.sRCBType = SRCB_CO;
                    break;

                case 0x02: // Operator command
                    deviceType = DtCONSOLE;
                    pcbp->controls.hasp.sRCBType = SRCB_CI;
                    break;

                case 0x03: // Normal input record
                    deviceType = DtCR;
                    pcbp->controls.hasp.sRCBType = SRCB_CR;
                    break;

                case 0x04: // Print record
                    deviceType = DtLP;
                    pcbp->controls.hasp.sRCBType = SRCB_LP;
                    break;

                case 0x05: // Punch record
                    deviceType = DtCP;
                    pcbp->controls.hasp.sRCBType = SRCB_CP;
                    break;

                default:
#if DEBUG
                    fprintf(npuHaspLog, "Port %02x: unrecognized record type in RCB: <%02x>\n",
                            pcbp->claPort, recordType);
#endif
                    deviceType = 0xff;
                    pcbp->controls.hasp.minorState = StHaspMinorRecvDLE2;
                    continue;
                }
                scbp = npuHaspFindStream(pcbp, streamId, deviceType);
                if (scbp != NULL)
                    {
                    pcbp->controls.hasp.designatedStream = scbp;
                    }
                }
            break;

        /*
        **  Read and process Sub-Record Control Byte.
        */
        case StHaspMinorRecvSRCB:
            if ((ch & 0x80) != 0)
                {
                pcbp->controls.hasp.minorState = StHaspMinorRecvSCB0;
                switch (pcbp->controls.hasp.sRCBType)
                {
                case SRCB_RTI: // Request To Initiate transmission
                    streamId   = (ch >> 4) & 0x07;
                    deviceType = ch & 0x0f;
                    switch (deviceType)
                    {
                    case 3: // Reader
                        scbp = npuHaspFindStream(pcbp, streamId, DtCR);
                        break;

                    case 4: // Printer
                        scbp = npuHaspFindStream(pcbp, streamId, DtLP);
                        break;

                    case 5: // Punch
                        scbp = npuHaspFindStream(pcbp, streamId, DtCP);
                        break;

                    default:
#if DEBUG
                        fprintf(npuHaspLog, "Port %02x: invalid stream id (%u) or device type (%u) ignored in RTI\n",
                                pcbp->claPort, streamId, deviceType);
#endif
                        scbp = NULL;
                        break;
                    }
                    if (scbp != NULL)
                        {
                        if ((scbp->tp == NULL)
                            || ((pcbp->ncbp->connType == ConnTypeHasp) && (scbp->isStarted == FALSE)))
                            {
                            scbp->isWaitingPTI = TRUE;
                            }
                        else
                            {
                            ptiRecord[1] = ch;
                            npuHaspAppendRecord(pcbp, ptiRecord, sizeof(ptiRecord));
                            npuHaspAppendOutput(pcbp, blockTrailer, sizeof(blockTrailer));
                            scbp->recordCount = 0;
                            scbp->lastSRCB    = 0;
                            }
                        }
                    break;

                case SRCB_PTI: // Permission To Initiate transmission
                    streamId   = (ch >> 4) & 0x07;
                    deviceType = ch & 0x0f;
                    switch (deviceType)
                    {
                    case 3: // Reader
                        scbp = npuHaspFindStream(pcbp, streamId, DtCR);
                        break;

                    case 4: // Printer
                        scbp = npuHaspFindStream(pcbp, streamId, DtLP);
                        break;

                    case 5: // Punch
                        scbp = npuHaspFindStream(pcbp, streamId, DtCP);
                        break;

                    default:
#if DEBUG
                        fprintf(npuHaspLog, "Port %02x: invalid stream id (%u) or device type (%u) ignored in PTI\n",
                                pcbp->claPort, streamId, deviceType);
#endif
                        scbp = NULL;
                        break;
                    }
                    if (scbp != NULL)
                        {
                        scbp->state       = StHaspStreamReady;
                        scbp->recordCount = 0;
                        scbp->lastSRCB    = 0;
                        }
                    break;

                case SRCB_GCR: // General Control Record
                    switch (ebcdicToAscii[ch])
                    {
                    case 'A': // Signon record
                        pcbp->controls.hasp.minorState = StHaspMinorRecvSignon;
                        break;

                    case 'B': // Signoff record
                    case 'C': // Print initialization record
                    case 'D': // Punch initialization record
                    case 'E': // Input initialization record
                    case 'F': // Data set transmission initialization
                    case 'G': // System configuration status
                    case 'H': // Diagnostic control record
                    default:
                        pcbp->controls.hasp.minorState = StHaspMinorRecvDLE2;
#if DEBUG
                        fprintf(npuHaspLog, "Port %02x: unsupported GCR type %c\n",
                                pcbp->claPort, (char)ebcdicToAscii[ch]);
#endif
                        break;
                    }
                    break;

                case SRCB_LP: // Print record
                    pcbp->controls.hasp.sRCBParam = ch & 0x7f;
                    break;

                case SRCB_BAD_BCB:
#if DEBUG
                    fprintf(npuHaspLog, "Port %02x: bad BCB sent in last block: %02x\n",
                            pcbp->claPort, ch);
#endif
                    pcbp->controls.hasp.downlineBSN = ch % 0x0f;
                    // TODO: retransmit last block with expected BSN
                    break;

                default: // do nothing for other SRCB types
                    break;
                }
                }
            else
                {
#if DEBUG
                fprintf(npuHaspLog, "Port %02x: invalid SRCB byte: <%02x>\n",
                        pcbp->claPort, ch);
#endif
                pcbp->controls.hasp.minorState = StHaspMinorRecvDLE2;
                }
            break;

        /*
        **  Read and process first String Control Byte of a record. If the
        **  first SCB is 0x00, then it might be an EOF indication. EOF is
        **  indicated by a 0x00 SCB followed immediately by a 0x00 RCB.
        */
        case StHaspMinorRecvSCB0:
            pcbp->controls.hasp.minorState = StHaspMinorRecvSCB;
            scbp = pcbp->controls.hasp.designatedStream;
            if (scbp != NULL) // RCB designated a stream
                {
                if (ch == 0)  // possible EOF indication
                    {
                    pcbp->controls.hasp.minorState = StHaspMinorRecvSCB_EOF;
                    continue;
                    }

                /*
                **  If the designated stream is a print stream, process format control.
                */
                if (scbp->tp->deviceType == DtLP)
                    {
                    npuHaspProcessFormatControl(pcbp);
                    }
                }

        // fall through

        /*
        **  Read and process String Control Byte other than the first of a record.
        */
        case StHaspMinorRecvSCB:
            scbp = pcbp->controls.hasp.designatedStream;
            if ((ch & 0x80) == 0) // End of record
                {
                if (scbp != NULL)
                    {
                    if (scbp->tp->deviceType == DtCONSOLE)
                        {
                        npuHaspFlushUplineData(scbp, TRUE);
                        }
                    else
                        {
                        npuHaspFlushUplineData(scbp, FALSE);
                        }
                    }
                pcbp->controls.hasp.minorState = StHaspMinorRecvRCB;
                }
            else if ((ch & 0x40) == 0) // Duplicate character
                {
                numBytes = ch & 0x1f;
                if ((ch & 0x20) == 0) // Duplicate character is blank
                    {
                    for (i = 0; i < numBytes; i++)
                        {
                        buf[i] = EbcdicBlank;
                        }
                    npuHaspStageUplineData(scbp, buf, numBytes);
                    }
                else if (numBytes > 0) // Duplicate is next byte
                    {
                    if (len > 0)
                        {
                        ch   = *dp++;
                        len -= 1;
                        for (i = 0; i < numBytes; i++)
                            {
                            buf[i] = ch;
                            }
                        npuHaspStageUplineData(scbp, buf, numBytes);
                        }
                    else
                        {
                        pcbp->controls.hasp.strLength  = numBytes;
                        pcbp->controls.hasp.minorState = StHaspMinorRecvRC;
                        }
                    }
                }
            else
                {
                pcbp->controls.hasp.strLength = numBytes = ch & 0x3f;
                if (numBytes > 0)
                    {
                    pcbp->controls.hasp.minorState = StHaspMinorRecvStr;
                    }
                }
            break;

        /*
        **  This state is entered after reading a 0x00 SCB byte. Read the next byte, an RCB byte.
        **  If it is 0x00, end of file is indicated. Otherwise, the 0x00 SCB byte indicates a
        **  zero length record.
        */
        case StHaspMinorRecvSCB_EOF:
            dp  -= 1; // cause RCB byte to be reprocessed
            len += 1;
            pcbp->controls.hasp.minorState = StHaspMinorRecvRCB;
            scbp = pcbp->controls.hasp.designatedStream;
            if (ch == 0x00) // if RCB is 0x00 then EOF detected
                {
                if (scbp->tp->deviceType == DtLP && scbp->lastSRCB != 0)
                    {
                    buf[0] = npuHaspTranslateSrcbToFe(scbp->lastSRCB);
                    buf[1] = EbcdicBlank;
                    npuHaspStageUplineData(scbp, buf, 2);
                    }
                npuHaspFlushUplineData(scbp, TRUE);
                }
            else if (scbp->tp->deviceType == DtLP)
                {
                npuHaspProcessFormatControl(pcbp);
                npuHaspFlushUplineData(scbp, FALSE);
                }
            else
                {
                npuHaspFlushUplineData(scbp, FALSE);
                }
            break;

        /*
        **  Read bytes of string.
        */
        case StHaspMinorRecvStr:
            i        = 0;
            numBytes = pcbp->controls.hasp.strLength;
            while (TRUE)
                {
                if (ch == DLE)
                    {
                    if (len < 1)
                        {
                        break;
                        }
                    ch   = *dp++;
                    len -= 1;
                    if (ch != DLE)
                        {
#if DEBUG
                        fprintf(npuHaspLog, "Port %02x: expected byte-stuffed DLE, received <%02x>\n",
                                pcbp->claPort, ch);
#endif
                        dp  -= 2;
                        len += 2;
                        pcbp->controls.hasp.minorState = StHaspMinorRecvDLE2;
                        i = 0;
                        break;
                        }
                    }
                buf[i++] = ch;
                if ((len < 1) || (i >= numBytes))
                    {
                    break;
                    }
                ch   = *dp++;
                len -= 1;
                }
            if (i > 0)
                {
                npuHaspStageUplineData(pcbp->controls.hasp.designatedStream, buf, i);
                }
            pcbp->controls.hasp.strLength -= i;
            if (pcbp->controls.hasp.strLength < 1)
                {
                pcbp->controls.hasp.minorState = StHaspMinorRecvSCB;
                }
            break;

        /*
        **  Read repeated character.
        */
        case StHaspMinorRecvRC:
            pcbp->controls.hasp.minorState = StHaspMinorRecvSCB;
            numBytes = pcbp->controls.hasp.strLength;
            for (i = 0; i < numBytes; i++)
                {
                buf[i] = ch;
                }
            npuHaspStageUplineData(pcbp->controls.hasp.designatedStream, buf, numBytes);
            break;

        /*
        **  Read bytes of a sign-on record.  A sign-on record is usually 80 bytes.
        **  Nevertheless, some systems send shorter records (e.g., they don't pad them
        **  with blanks to 80 bytes), so read bytes until a <00> byte (the terminating
        **  RCB) is detected.
        */
        case StHaspMinorRecvSignon:
#if DEBUG
            i = 0;
#endif
            while (len > 0)
                {
                if (ch == 0)
                    {
                    npuHaspAppendOutput(pcbp, ackIndication, sizeof(ackIndication));
                    pcbp->controls.hasp.minorState = StHaspMinorRecvDLE_Signon;
                    pcbp->controls.hasp.isSignedOn = TRUE;
                    break;
                    }
#if DEBUG
                if (i >= sizeof(buf))
                    {
                    fprintf(npuHaspLog, "Port %02x: received signon record\n", pcbp->claPort);
                    npuHaspLogBytes(buf, i, EBCDIC);
                    npuHaspLogFlush();
                    i = 0;
                    }
                buf[i++] = ch;
#endif
                ch   = *dp++;
                len -= 1;
                }
#if DEBUG
            if (i > 0)
                {
                fprintf(npuHaspLog, "Port %02x: received signon record\n", pcbp->claPort);
                npuHaspLogBytes(buf, i, EBCDIC);
                npuHaspLogFlush();
                }
            if (pcbp->controls.hasp.isSignedOn)
                {
                fprintf(npuHaspLog, "Port %02x: signon complete\n", pcbp->claPort);
                }
#endif
            break;

        /*
        **  Read DLE terminating signon record. A <00> byte is also tolerated because, e.g.,
        **  PRIMOS RJE sends a <00> SCB and a <00> RCB after a signon record instead of sending
        **  only a <00> RCB.
        */
        case StHaspMinorRecvDLE_Signon:
            if (ch == DLE)
                {
                pcbp->controls.hasp.minorState = StHaspMinorRecvETB1;
                }
            else if (ch == 0)
                {
                pcbp->controls.hasp.minorState = StHaspMinorRecvDLE1;
                }
            else
                {
#if DEBUG
                fprintf(npuHaspLog, "Port %02x: expected DLE or <00> after signon record, received <%02x>\n",
                        pcbp->claPort, ch);
#endif
                pcbp->controls.hasp.minorState = StHaspMinorRecvDLE2;
                }
            break;

        /*
        **  Read DLE terminating record.
        */
        case StHaspMinorRecvDLE1:
            if (ch == DLE)
                {
                pcbp->controls.hasp.minorState = StHaspMinorRecvETB1;
                }
            else
                {
#if DEBUG
                fprintf(npuHaspLog, "Port %02x: expected DLE after transmission block terminator, received <%02x>\n",
                        pcbp->claPort, ch);
#endif
                pcbp->controls.hasp.minorState = StHaspMinorRecvDLE2;
                }
            break;

        /*
        **  Read ETB terminating record at end of successfully processed block.
        */
        case StHaspMinorRecvETB1:
            if (ch == ETB)
                {
                pcbp->controls.hasp.majorState = StHaspMajorSendData;
                pcbp->controls.hasp.minorState = StHaspMinorRecvBOF;
                if (pcbp->controls.hasp.outBuf != NULL)
                    {
                    blockLen = pcbp->controls.hasp.outBuf->numBytes;
                    if ((blockLen > 1)
                        && !((pcbp->controls.hasp.outBuf->data[blockLen - 2] == DLE)
                             && ((pcbp->controls.hasp.outBuf->data[blockLen - 1] == ETB)
                                 || (pcbp->controls.hasp.outBuf->data[blockLen - 1] == ACK0)))
                        && !((pcbp->controls.hasp.outBuf->data[blockLen - 2] == SYN)
                             && (pcbp->controls.hasp.outBuf->data[blockLen - 1] == NAK)))
                        {
                        npuHaspAppendOutput(pcbp, blockTrailer, sizeof(blockTrailer));
                        }
                    if (npuHaspFlushBuffer(pcbp))
                        {
                        pcbp->controls.hasp.majorState = StHaspMajorRecvData;
                        }
                    }
                }
            else
                {
#if DEBUG
                fprintf(npuHaspLog, "Port %02x: expected ETB, received <%02x>\n",
                        pcbp->claPort, ch);
#endif
                pcbp->controls.hasp.minorState = StHaspMinorRecvDLE2;
                }
            break;

        /*
        **  Hunt for DLE possibly indicating termination of record. This state is
        **  entered after a protocol error is detected. Bytes are read and discarded
        **  until a DLE or SYN is detected.
        */
        case StHaspMinorRecvDLE2:
            switch (ch)
            {
            case DLE:
                pcbp->controls.hasp.minorState = StHaspMinorRecvETB2;
                break;

            case SYN:
                pcbp->controls.hasp.minorState = StHaspMinorRecvBOF;
                break;

            case SOH:
                npuHaspReleaseLastBlockSent(pcbp);
                pcbp->controls.hasp.minorState = StHaspMinorRecvENQ;
                break;

            default:
                break;
            }
            break;

        /*
        **  Read ETB terminating record while processing protocol error.
        */
        case StHaspMinorRecvETB2:
            if (ch == ETB)
                {
                pcbp->controls.hasp.majorState = StHaspMajorSendData;
                if (pcbp->controls.hasp.outBuf != NULL)
                    {
                    pcbp->controls.hasp.outBuf->numBytes = pcbp->controls.hasp.outBuf->offset = 0;
                    }
                npuHaspAppendOutput(pcbp, nakIndication, sizeof(nakIndication));
                if (npuHaspFlushBuffer(pcbp))
                    {
                    pcbp->controls.hasp.majorState = StHaspMajorRecvData;
                    pcbp->controls.hasp.minorState = StHaspMinorRecvBOF;
                    }
                }
            else
                {
#if DEBUG
                fprintf(npuHaspLog, "Port %02x: expected ETB, received <%02x>\n",
                        pcbp->claPort, ch);
#endif
                pcbp->controls.hasp.minorState = StHaspMinorRecvDLE2;
                }
            break;

        /*
        **  Invalid state.
        */
        case StHaspMinorNIL:
            if (pcbp->controls.hasp.majorState == StHaspMajorInit)
                {
#if DEBUG
                fprintf(npuHaspLog, "Port %02x: not configured yet, data discarded\n",
                        pcbp->claPort);
#endif

                return;
                }

        // else fall through
        default:
            fprintf(stderr, "(npu_hasp) Port %02x: invalid minor state: %02x\n",
                    pcbp->claPort, pcbp->controls.hasp.minorState);
            pcbp->controls.hasp.minorState = StHaspMinorRecvBOF;

            return;
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Closes a stream.
**
**  Parameters:     Name        Description.
**                  tp          pointer to TCB of the stream to close
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuHaspCloseStream(Tcb *tp)
    {
    Pcb *pcbp;
    u8  streamId;

    pcbp     = tp->pcbp;
    streamId = tp->streamId;

#if DEBUG
    fprintf(npuHaspLog, "Port %02x: close stream %d (%.7s)\n", pcbp->claPort,
            streamId, tp->termName);
#endif

    switch (tp->deviceType)
        {
    case DtCR:
        if ((streamId > 0) && (streamId <= MaxHaspStreams))
            {
            pcbp->controls.hasp.readerStreams[streamId - 1].state = StHaspStreamInit;
            pcbp->controls.hasp.readerStreams[streamId - 1].tp    = NULL;
            }
        break;

    case DtLP:
        if ((streamId > 0) && (streamId <= MaxHaspStreams))
            {
            pcbp->controls.hasp.printStreams[streamId - 1].state = StHaspStreamInit;
            pcbp->controls.hasp.printStreams[streamId - 1].tp    = NULL;
            }
        break;

    case DtCP:
    case DtPLOTTER:
        if ((streamId > 0) && (streamId <= MaxHaspStreams))
            {
            pcbp->controls.hasp.punchStreams[streamId - 1].state = StHaspStreamInit;
            pcbp->controls.hasp.punchStreams[streamId - 1].tp    = NULL;
            }
        break;
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
void npuHaspNotifyAck(Tcb *tp, u8 bsn)
    {
    tp->uplineBlockLimit += 1;
#if DEBUG
    fprintf(npuHaspLog, "Port %02x: ack for upline block from %.7s, ubl %d\n",
            tp->pcbp->claPort, tp->termName, tp->uplineBlockLimit);
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
bool npuHaspNotifyNetConnect(Pcb *pcbp, bool isPassive)
    {
#if DEBUG
    fprintf(npuHaspLog, "Port %02x: network connection indication\n", pcbp->claPort);
#endif
    npuHaspResetPcb(pcbp);

    return npuSvmConnectTerminal(pcbp);
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
void npuHaspNotifyNetDisconnect(Pcb *pcbp)
    {
    int i;
    Tcb *tp;

#if DEBUG
    fprintf(npuHaspLog, "Port %02x: network disconnection indication\n", pcbp->claPort);
#endif
    tp = pcbp->controls.hasp.consoleStream.tp;
    if (tp != NULL)
        {
        npuSvmSendDiscRequest(tp);
        }
    else
        {
        npuNetCloseConnection(pcbp);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Handles a notification that NAM has sent an SI (Start
**                  Input) command to start PRU input on a stream.
**
**  Parameters:     Name        Description.
**                  tp          pointer to TCB
**                  sfc         secondary function code
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuHaspNotifyStartInput(Tcb *tp, u8 sfc)
    {
    Scb *scbp;

    if ((sfc == SfcNONTR) && (tp->scbp != NULL))
        {
        scbp = tp->scbp;
        scbp->isDiscardingRecords = FALSE;
        scbp->isStarted           = TRUE;
#if DEBUG
        fprintf(npuHaspLog, "Port %02x: SI/NONTR command received for stream %d (%.7s)\n",
                tp->pcbp->claPort, tp->streamId, tp->termName);
        }
    else if (sfc != SfcNONTR)
        {
        fprintf(npuHaspLog, "Port %02x: Unexpected SFC %u in SI command received for stream %d (%.7s)\n",
                tp->pcbp->claPort, sfc, tp->streamId, tp->termName);
        }
    else
        {
        fprintf(npuHaspLog, "Port %02x: SI/MONTR command received for stream %d (%.7s) that has no SCB\n",
                tp->pcbp->claPort, tp->streamId, tp->termName);
#endif
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Handles a terminal connect notification from SVM.
**
**  Parameters:     Name        Description.
**                  tp          pointer to TCB of connected stream
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuHaspNotifyTermConnect(Tcb *tp)
    {
    u8  deviceType;
    Pcb *pcbp;
    Scb *scbp;
    u8  streamId;

    deviceType = tp->deviceType;
    streamId   = tp->streamId;
    pcbp       = tp->pcbp;
    scbp       = NULL;

#if DEBUG
    fprintf(npuHaspLog, "Port %02x: connect stream %d (%.7s)\n", pcbp->claPort,
            streamId, tp->termName);
#endif

    if (pcbp->connFd <= 0)
        {
        npuSvmSendDiscRequest(tp);
#if DEBUG
        fprintf(npuHaspLog, "Port %02x: no network connection, disconnect stream %d (%.7s)\n",
                pcbp->claPort, streamId, tp->termName);
#endif

        return;
        }

    if (deviceType == DtCONSOLE)
        {
        tp->scbp    = scbp = &pcbp->controls.hasp.consoleStream;
        scbp->state = StHaspStreamReady;
        if ((pcbp->ncbp->connType == ConnTypeRevHasp)
            && (pcbp->controls.hasp.majorState == StHaspMajorInit))
            {
            pcbp->controls.hasp.majorState = StHaspMajorSendENQ;
            }
        }
    else if ((streamId > 0) && (streamId <= MaxHaspStreams))
        {
        switch (deviceType)
            {
        case DtCR:
            tp->scbp    = scbp = &pcbp->controls.hasp.readerStreams[streamId - 1];
            scbp->state = StHaspStreamInit;
            break;

        case DtLP:
            tp->scbp    = scbp = &pcbp->controls.hasp.printStreams[streamId - 1];
            scbp->state = StHaspStreamInit;
            break;

        case DtCP:
        case DtPLOTTER:
            tp->scbp    = scbp = &pcbp->controls.hasp.punchStreams[streamId - 1];
            scbp->state = StHaspStreamInit;
            break;

        default:
#if DEBUG
            fprintf(npuHaspLog, "Port %02x: attempt to initialize stream for invalid device type %u\n",
                    pcbp->claPort, deviceType);
#endif
            npuSvmSendDiscRequest(tp);
            break;
            }
        }
    else
        {
#if DEBUG
        fprintf(npuHaspLog, "Port %02x: attempt to initialize invalid stream id %u\n",
                pcbp->claPort, streamId);
#endif
        npuSvmSendDiscRequest(tp);
        }
    if (scbp != NULL)
        {
        scbp->tp             = tp;
        tp->uplineBlockLimit = tp->params.fvUBL;
#if DEBUG
        fprintf(npuHaspLog, "Port %02x: upline block limit %d\n", pcbp->claPort, tp->uplineBlockLimit);
#endif
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Handles a terminal disconnect event from SVM.
**
**  Parameters:     Name        Description.
**                  tp          pointer to TCB of disconnected stream
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuHaspNotifyTermDisconnect(Tcb *tp)
    {
    int i;
    Pcb *pcbp;
    Tcb *tp2;

    pcbp = tp->pcbp;

    if (tp->deviceType == DtCONSOLE)
        {
        for (i = 0; i < MaxHaspStreams; ++i)
            {
            tp2 = pcbp->controls.hasp.readerStreams[i].tp;
            if (tp2 != NULL)
                {
#if DEBUG
                fprintf(npuHaspLog, "Port %02x: disconnect stream %d (%.7s)\n", pcbp->claPort,
                        tp2->streamId, tp2->termName);
#endif
                npuSvmSendDiscRequest(tp2);
                }
            tp2 = pcbp->controls.hasp.printStreams[i].tp;
            if (tp2 != NULL)
                {
#if DEBUG
                fprintf(npuHaspLog, "Port %02x: disconnect stream %d (%.7s)\n", pcbp->claPort,
                        tp2->streamId, tp2->termName);
#endif
                npuSvmSendDiscRequest(tp2);
                }
            tp2 = pcbp->controls.hasp.punchStreams[i].tp;
            if (tp2 != NULL)
                {
#if DEBUG
                fprintf(npuHaspLog, "Port %02x: disconnect stream %d (%.7s)\n", pcbp->claPort,
                        tp2->streamId, tp2->termName);
#endif
                npuSvmSendDiscRequest(tp2);
                }
            }
        }
    else
        {
        npuHaspCloseStream(tp);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Handles a notification that NAM has sent a TO (Terminate
**                  Output) command to terminate an output stream.
**
**  Parameters:     Name        Description.
**                  tp          pointer to TCB
**                  sfc         secondary function code
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuHaspNotifyTerminateOutput(Tcb *tp, u8 sfc)
    {
    Scb *scbp;

    if ((sfc == SfcMARK) && (tp->scbp != NULL))
        {
        scbp = tp->scbp;
        scbp->isTerminateRequested = TRUE;
#if DEBUG
        fprintf(npuHaspLog, "Port %02x: TO/MARK command received for stream %d (%.7s)\n",
                tp->pcbp->claPort, tp->streamId, tp->termName);
        }
    else if (sfc != SfcMARK)
        {
        fprintf(npuHaspLog, "Port %02x: Unexpected SFC %u in TO command received for stream %d (%.7s)\n",
                tp->pcbp->claPort, sfc, tp->streamId, tp->termName);
        }
    else
        {
        fprintf(npuHaspLog, "Port %02x: TO/MARK command received for stream %d (%.7s) that has no SCB\n",
                tp->pcbp->claPort, tp->streamId, tp->termName);
#endif
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Parse batch device parameter FN/FV string.
**
**  Parameters:     Name        Description.
**                  mp          message pointer
**                  len         message length
**                  tp          pointer to TCB
**
**  Returns:        TRUE if no error, FALSE otherwise.
**
**------------------------------------------------------------------------*/
bool npuHaspParseDevParams(u8 *mp, int len, Tcb *tp)
    {
    BatchParams *pp = &tp->scbp->params;

    while (len > 0)
        {
#if DEBUG
        npuHaspLogDevParam(tp, mp[0], mp[1]);
#endif
        switch (mp[0])
            {
        case FnDevTbsUpper:
            pp->fvDevTBS = (pp->fvDevTBS & 0x00ff) | (mp[1] << 8);
            break;

        case FnDevTbsLower:
            pp->fvDevTBS = (pp->fvDevTBS & 0xff00) | mp[1];
            break;

        case FnDevPW:
            pp->fvDevPW = mp[1];
            break;

        case FnDevPL:
            pp->fvDevPL = mp[1];
            break;

        case FnDevPrintTrain:
            pp->fvDevPrintTrain = mp[1];
            break;

        default:
#if DEBUG
            fprintf(npuHaspLog, "Unknown device FN/FV (%d/%d)[%02x/%02x]\n", mp[0], mp[1], mp[0], mp[1]);
#endif
            break;
            }

        /*
        **  Advance to next FN/FV pair.
        */
        mp  += 2;
        len -= 2;
        }

    return (TRUE);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Parse batch file parameter FN/FV string.
**
**  Parameters:     Name        Description.
**                  mp          message pointer
**                  len         message length
**                  tp          pointer to TCB
**
**  Returns:        TRUE if no error, FALSE otherwise.
**
**------------------------------------------------------------------------*/
bool npuHaspParseFileParams(u8 *mp, int len, Tcb *tp)
    {
    BatchParams *pp = &tp->scbp->params;

    while (len > 0)
        {
#if DEBUG
        npuHaspLogFileParam(tp, mp[0], mp[1]);
#endif
        switch (mp[0])
            {
        case FnFileType:
            pp->fvFileType = mp[1];
            break;

        case FnFileCC:
            pp->fvFileCC = mp[1];
            break;

        case FnFileLace:
            pp->fvFileLace = mp[1];
            break;

        case FnFileLimUpper:
            pp->fvFileLimit = (pp->fvFileLimit & 0x00ff) | (mp[1] << 8);
            break;

        case FnFileLimLower:
            pp->fvFileLimit = (pp->fvFileLimit & 0xff00) | mp[1];
            break;

        case FnFilePunchLimit:
            pp->fvFilePunchLimit = mp[1];
            break;

        default:
#if DEBUG
            fprintf(npuHaspLog, "Unknown file FN/FV (%d/%d)[%02x/%02x]\n", mp[0], mp[1], mp[0], mp[1]);
#endif
            break;
            }

        /*
        **  Advance to next FN/FV pair.
        */
        mp  += 2;
        len -= 2;
        }

    return (TRUE);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Presets HASP controls in a freshly allocated PCB.
**
**  Parameters:     Name        Description.
**                  pcbp        pointer to PCB
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuHaspPresetPcb(Pcb *pcbp)
    {
    int i;
    Scb *scbp;

#if DEBUG
    if (npuHaspLog == NULL)
        {
        npuHaspLog = fopen("hasplog.txt", "wt");
        if (npuHaspLog == NULL)
            {
            fprintf(stderr, "hasplog.txt - aborting\n");
            exit(1);
            }
        npuHaspLogFlush();    // initialize log buffer
        }
#endif

    pcbp->controls.hasp.lastBlockSent         = NULL;
    pcbp->controls.hasp.retries               = 0;
    pcbp->controls.hasp.outBuf                = NULL;
    memset(&pcbp->controls.hasp.consoleStream.uplineQ, 0, sizeof(NpuQueue));

    for (i = 0; i < MaxHaspStreams; ++i)
        {
        scbp = &pcbp->controls.hasp.readerStreams[i];
        memset(&scbp->uplineQ, 0, sizeof(NpuQueue));

        scbp = &pcbp->controls.hasp.printStreams[i];
        if (pcbp->ncbp->connType == ConnTypeHasp)
            {
            scbp->pruFragment = (u8 *)malloc(MaxBuffer);
            if (scbp->pruFragment == NULL)
                {
                fprintf(stderr, "Failed to allocate PRU fragment buffer for HASP print stream\n");
                exit(1);
                }
            }
        else
            {
            scbp->pruFragment = NULL;
            }
        scbp->pruFragment2 = NULL;
        memset(&scbp->uplineQ, 0, sizeof(NpuQueue));

        scbp = &pcbp->controls.hasp.punchStreams[i];
        if (pcbp->ncbp->connType == ConnTypeHasp)
            {
            scbp->pruFragment = (u8 *)malloc(MaxBuffer);
            if (scbp->pruFragment == NULL)
                {
                fprintf(stderr, "Failed to allocate PRU fragment buffer for HASP punch stream\n");
                exit(1);
                }
            }
        else
            {
            scbp->pruFragment = NULL;
            }
        scbp->pruFragment2 = NULL;
        memset(&scbp->uplineQ, 0, sizeof(NpuQueue));
        }
    npuHaspResetPcb(pcbp);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Resets HASP controls in a PCB.
**
**  Parameters:     Name        Description.
**                  pcbp        pointer to PCB
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuHaspResetPcb(Pcb *pcbp)
    {
    int i;

    pcbp->controls.hasp.majorState          = StHaspMajorInit;
    pcbp->controls.hasp.minorState          = StHaspMinorNIL;
    pcbp->controls.hasp.lastRecvTime        = 0;
    pcbp->controls.hasp.recvDeadline        = 0;
    pcbp->controls.hasp.sendDeadline        = 0;
    pcbp->controls.hasp.isSignedOn          = FALSE;
    pcbp->controls.hasp.pauseAllOutput      = FALSE;
    pcbp->controls.hasp.currentOutputStream = NULL;
    pcbp->controls.hasp.designatedStream    = NULL;
    pcbp->controls.hasp.retries             = 0;
    pcbp->controls.hasp.lastRecvFrameType   = 0;
    pcbp->controls.hasp.downlineBSN         = 0;
    pcbp->controls.hasp.uplineBSN           = 0x0f;
    pcbp->controls.hasp.fcsMask             = 0xff;
    if (pcbp->controls.hasp.lastBlockSent != NULL)
        {
        npuBipBufRelease(pcbp->controls.hasp.lastBlockSent);
        pcbp->controls.hasp.lastBlockSent = NULL;
        }
    if (pcbp->controls.hasp.outBuf != NULL)
        {
        npuBipBufRelease(pcbp->controls.hasp.outBuf);
        pcbp->controls.hasp.outBuf = NULL;
        }
    npuHaspResetScb(&pcbp->controls.hasp.consoleStream);
    for (i = 0; i < MaxHaspStreams; ++i)
        {
        npuHaspResetScb(&pcbp->controls.hasp.readerStreams[i]);
        npuHaspResetScb(&pcbp->controls.hasp.printStreams [i]);
        npuHaspResetScb(&pcbp->controls.hasp.punchStreams [i]);
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
**  Purpose:        Closes a HASP or Reverse HASP connection.
**
**  Parameters:     Name        Description.
**                  pcbp        pointer to PCB
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuHaspCloseConnection(Pcb *pcbp)
    {
#if DEBUG
    fprintf(npuHaspLog, "Port %02x: close connection\n", pcbp->claPort);
#endif
    if (pcbp->controls.hasp.consoleStream.tp != NULL)
        {
        npuSvmSendDiscRequest(pcbp->controls.hasp.consoleStream.tp);
        }
    else
        {
        npuNetCloseConnection(pcbp);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Find a stream by stream identifier.
**
**  Parameters:     Name        Description.
**                  pcbp        PCB pointer
**                  streamId    stream identifier
**                  deviceType  device type of stream
**
**  Returns:        Pointer to SCB of stream, or NULL if stream not found.
**
**------------------------------------------------------------------------*/
static Scb *npuHaspFindStream(Pcb *pcbp, u8 streamId, u8 deviceType)
    {
    Scb *scbp;

    scbp = NULL;

    switch (deviceType)
        {
    case DtCONSOLE:
        return &pcbp->controls.hasp.consoleStream;

    case DtCR:
        if ((streamId > 0) && (streamId <= MaxHaspStreams))
            {
            scbp = &pcbp->controls.hasp.readerStreams[streamId - 1];
            }
        break;

    case DtLP:
        if ((streamId > 0) && (streamId <= MaxHaspStreams))
            {
            scbp = &pcbp->controls.hasp.printStreams[streamId - 1];
            }
        break;

    case DtCP:
    case DtPLOTTER:
        if ((streamId > 0) && (streamId <= MaxHaspStreams))
            {
            scbp = &pcbp->controls.hasp.punchStreams[streamId - 1];
            }
        break;
        }

    return (scbp != NULL && scbp->tp != NULL) ? scbp : NULL;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Find a stream with queued output.
**
**  Parameters:     Name        Description.
**                  pcbp        pointer to PCB
**
**  Returns:        Pointer to SCB of stream with queued output, or NULL if
**                  no streams have queued output
**
**------------------------------------------------------------------------*/
static Scb *npuHaspFindStreamWithOutput(Pcb *pcbp)
    {
    u8  i;
    u8  pi;
    u8  pollIndex;
    Scb *scbp;
    Scb *toReqScbp;

    if (pcbp->controls.hasp.pauseAllOutput
        && (pcbp->controls.hasp.pauseDeadline > getMilliseconds()))
        {
        return NULL;
        }

    /*
    **  If this is a HASP connection, and it is not signed on yet,
    **  hold all output until signon has completed.  If it's a
    **  reverse HASP connection, and it is not signed on yet, allow
    **  console output only because the output might be a signon
    **  record.
    */
    if (pcbp->controls.hasp.isSignedOn == FALSE)
        {
        if ((pcbp->ncbp->connType == ConnTypeRevHasp)
            && npuBipQueueNotEmpty(&pcbp->controls.hasp.consoleStream.tp->outputQ))
            {
            return &pcbp->controls.hasp.consoleStream;
            }
        else
            {
            return NULL;
            }
        }

    /*
    **  Console takes precedence over other streams
    */
    if ((pcbp->controls.hasp.consoleStream.tp->xoff == FALSE)
        && npuBipQueueNotEmpty(&pcbp->controls.hasp.consoleStream.tp->outputQ))
        {
        return &pcbp->controls.hasp.consoleStream;
        }

    /*
    **  Round-robin using pollIndex for other streams.  For HASP connections,
    **  output streams are printer and punch streams.  For Reverse HASP
    **  connections, output streams are reader streams.
    */
    pollIndex = pcbp->controls.hasp.pollIndex;
    if (pcbp->ncbp->connType == ConnTypeHasp)
        {
        pcbp->controls.hasp.pollIndex = (pollIndex + 1) % (MaxHaspStreams * 2);
        toReqScbp = NULL;
        for (i = 0; i < MaxHaspStreams * 2; i++)
            {
            pi = (pollIndex + i) % (MaxHaspStreams * 2);
            scbp = (pi < MaxHaspStreams)
                ? &pcbp->controls.hasp.printStreams[pi]
                : &pcbp->controls.hasp.punchStreams[pi - MaxHaspStreams];
            if (scbp->tp != NULL)
                {
                if (scbp->isTerminateRequested)
                    {
                    return toReqScbp = scbp;
                    }
                if ((scbp->state == StHaspStreamReady
                          || scbp->state == StHaspStreamSendRTI
                          || scbp->state == StHaspStreamWaitAcctng)
                         && scbp->tp->xoff == FALSE
                         && npuBipQueueNotEmpty(&scbp->tp->outputQ))
                    {
                    return scbp;
                    }
                }
            }
            if (toReqScbp != NULL) return toReqScbp;
        }
    else // pcbp->ncbp->connType == ConnTypeRevHasp
        {
        pcbp->controls.hasp.pollIndex = (pollIndex + 1) % MaxHaspStreams;
        for (i = 0; i < MaxHaspStreams; i++)
            {
            pi   = (pollIndex + i) % MaxHaspStreams;
            scbp = &pcbp->controls.hasp.readerStreams[pi];
            if ((scbp->tp != NULL)
                && ((scbp->state == StHaspStreamReady) || (scbp->state == StHaspStreamSendRTI))
                && (scbp->tp->xoff == FALSE)
                && npuBipQueueNotEmpty(&scbp->tp->outputQ))
                {
                return scbp;
                }
            }
        }

    return NULL;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Find a stream with a pending request to initiate transmission.
**
**  Parameters:     Name        Description.
**                  pcbp        pointer to PCB
**
**  Returns:        Pointer to SCB of stream with pending RTI, or NULL if
**                  no streams have an RTI for which a PTI can be sent
**
**------------------------------------------------------------------------*/
static Scb *npuHaspFindStreamWithPendingRTI(Pcb *pcbp)
    {
    u8  i;
    Scb *scbp;

    if (pcbp->ncbp->connType == ConnTypeRevHasp)
        {
        for (i = 0; i < MaxHaspStreams; i++)
            {
            scbp = &pcbp->controls.hasp.printStreams[i];
            if ((scbp->tp != NULL) && scbp->isWaitingPTI)
                {
                return scbp;
                }
            scbp = &pcbp->controls.hasp.punchStreams[i];
            if ((scbp->tp != NULL) && scbp->isWaitingPTI)
                {
                return scbp;
                }
            }
        }
    else // pcbp->ncbp->connType == ConnTypeHasp
        {
        for (i = 0; i < MaxHaspStreams; i++)
            {
            scbp = &pcbp->controls.hasp.readerStreams[i];
            if ((scbp->tp != NULL) && scbp->isWaitingPTI && scbp->isStarted)
                {
                return scbp;
                }
            }
        }

    return NULL;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Flush a buffered PRU fragment downline.
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**
**  Returns:        Number of bytes flushed.
**
**------------------------------------------------------------------------*/
static int npuHaspFlushPruFragment(Tcb *tp)
    {
    u8  *fp;
    int len;
    Scb *scbp;
    int size;

    if (tp->deviceType == DtLP)
        {
        scbp = tp->scbp;
        if (scbp->pruFragmentSize < 1)
            {
            scbp->pruFragment[scbp->pruFragmentSize++] = EbcdicBlank;
            }
        return isPostPrint(tp) ? npuHaspFlushPruPostPrintFragment(tp) : npuHaspFlushPruPrePrintFragment(tp);
        }
    else
        {
        scbp = tp->scbp;
        fp   = scbp->pruFragment;
        size = scbp->pruFragmentSize;

        if (tp->deviceType == DtCP && scbp->params.fvFileType == 1) // avoid sending lace card
            {
            scbp->pruFragmentSize       = 0;
            scbp->isPruFragmentComplete = FALSE;
            return 0;
            }
        else if (size < 1)
            {
            fp = blank;
            size = sizeof(blank);
            }
        len  = npuHaspSendRecordHeader(tp, 0);
        len += npuHaspSendRecordStrings(tp, fp, size);
        scbp->recordCount += 1;
        scbp->pruFragmentSize       = 0;
        scbp->isPruFragmentComplete = FALSE;
        return len;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Flush a buffered PRU fragment for a post-print terminal
**                  downline.
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**
**  Returns:        Number of bytes flushed.
**
**------------------------------------------------------------------------*/
static int npuHaspFlushPruPostPrintFragment(Tcb *tp)
    {
    u8  *dp;
    u8  fe;
    u8  *fp;
    int len;
    Scb *scbp;
    int size;
    u8  srcb;

    scbp = tp->scbp;

    if (scbp->pruFragmentSize < 1)
        {
        scbp->isPruFragmentComplete = FALSE;
        return 0;
        }
    if (scbp->pruFragment2 == NULL)
        {
        //
        // The first record of a file has been received, so modify the
        // PRU fragment buffer to appear as if the first record was an
        // empty line and the first record actually received was the
        // next record.
        //
        dp = scbp->pruFragment + scbp->pruFragmentSize + 1;
        fp = dp - 2;
        while (fp >= scbp->pruFragment) *dp-- = *fp--;
        fp = scbp->pruFragment;
        *fp++ = EbcdicBlank;
        *fp++ = EbcdicBlank;
        scbp->pruFragment2 = fp;
        scbp->pruFragmentSize += 2;
        return npuHaspFlushPruPostPrintFragment(tp);
        }
    //
    // A previous record is pending, so handle the following cases:
    //
    //   1) The pending record has a post-print format effector. Flush it
    //      immediately, and make the current record pending.
    //   2) The pending record has a pre-print format effector, and the
    //      current record also has a pre-print format effector. Translate
    //      the current record's format effector to post-print, and flush
    //      the previous record with it. Then, make the current record pending.
    //   3) The pending record has a pre-print format effector, and the
    //      current record has a post-print format effector. Flush the pending
    //      record with a post-print format effector that advances one line.
    //      Then, make the current record pending.
    //
    if (scbp->pruFragment2 >= scbp->pruFragment + scbp->pruFragmentSize)
        {
        //
        // Current record is empty, so treat it as if it is an empty
        // line with the pre-print format effector ' '
        //
        scbp->pruFragment2  = scbp->pruFragment + scbp->pruFragmentSize;
        *scbp->pruFragment2 = EbcdicBlank;
        }
    fe = ebcdicToAscii[*scbp->pruFragment2];
    if (fe >= 'Q' && fe <= 'T') // discard lines with these
        {
        scbp->pruFragmentSize = scbp->pruFragment2 - scbp->pruFragment;
        scbp->isPruFragmentComplete = FALSE;
        return 0;
        }
    fp = scbp->pruFragment;
    fe = ebcdicToAscii[*fp++];
    size = scbp->pruFragment2 - fp;
    switch (fe)
        {
    case 'C': // skip to channel 6 after print
    case 'D': // skip to channel 5 after print
    case 'E': // skip to channel 4 after print
    case 'F': // skip to channel 3 after print
    case 'G': // skip to channel 2 after print
    case 'H': // skip to channel 1 after print
        srcb = 0x11 + ('H' - fe);
        break;

    case '/': // suppress space after print
        srcb = 0x00;
        break;

    default:
        fe = ebcdicToAscii[*scbp->pruFragment2];
        switch (fe)
            {
        case '0': // space two lines after print
            srcb = 0x02;
            break;

        case '1': // skip to channel 1 after print (page eject)
            srcb = 0x11;
            break;

        case '2': // skip to channel 12 after print (end of form)
            srcb = 0x1c;
            break;

        case '3': // skip to channel 6 after print
        case '4': // skip to channel 5 after print
        case '5': // skip to channel 4 after print
        case '6': // skip to channel 3 after print
        case '7': // skip to channel 2 after print
        case '8': // skip to channel 1 after print
          srcb = 0x11 + ('8' - fe);
          break;

        case '-': // space three lines after print
            srcb = 0x03;
            break;

        case '+': // suppress carriage control (overstrike)
            srcb = 0x00;
            break;

        case ' ':
        case 'C':
        case 'D':
        case 'E':
        case 'F':
        case 'G':
        case 'H':
        default:
            srcb = 0x01;
            break;
            }
        break;
        }
    len = npuHaspSendRecordHeader(tp, srcb);
    if (size < 1)
        {
        fp = blank;
        size = sizeof(blank);
        }
    len += npuHaspSendRecordStrings(tp, fp, size);
    scbp->recordCount += 1;
    scbp->pruFragmentSize -= scbp->pruFragment2 - scbp->pruFragment;
    if (scbp->pruFragmentSize > 0)
        {
        memcpy(scbp->pruFragment, scbp->pruFragment2, scbp->pruFragmentSize);
        scbp->pruFragment2 = scbp->pruFragment + scbp->pruFragmentSize;
        }
    else
        {
        scbp->pruFragment2 = NULL;
        }
    scbp->isPruFragmentComplete = FALSE;
    return len;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Flush a buffered PRU fragment for a pre-print terminal
**                  downline.
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**
**  Returns:        Number of bytes flushed.
**
**------------------------------------------------------------------------*/
static int npuHaspFlushPruPrePrintFragment(Tcb *tp)
    {
    u8  fe;
    u8  *fp;
    int len;
    Scb *scbp;
    int size;
    u8  srcb;

    scbp = tp->scbp;
    srcb = 0;
    fp   = scbp->pruFragment;
    size = scbp->pruFragmentSize;

    if (size < 1)
        {
        scbp->isPruFragmentComplete = FALSE;
        return 0;
        }
    fe = ebcdicToAscii[*fp++];
    size -= 1;
    switch (fe)
        {
    case '0': // space one line before print
        srcb = 0x22;
        break;

    case '1': // skip to channel 1 before print (page eject)
        srcb = 0x31;
        break;

    case '2': // skip to channel 12 before print (end of form)
        srcb = 0x3c;
        break;

    case '3': // skip to channel 6 before print
    case '4': // skip to channel 5 before print
    case '5': // skip to channel 4 before print
    case '6': // skip to channel 3 before print
    case '7': // skip to channel 2 before print
    case '8': // skip to channel 1 before print
      srcb = 0x31 + ('8' - fe);
      break;

    case 'C': // skip to channel 6 after print
    case 'D': // skip to channel 5 after print
    case 'E': // skip to channel 4 after print
    case 'F': // skip to channel 3 after print
    case 'G': // skip to channel 2 after print
    case 'H': // skip to channel 1 after print
      srcb = 0x11 + ('H' - fe);
      break;

    case 'Q': // suppress auto-eject
    case 'R': // set auto-eject
    case 'S': // clear 8 LPI
    case 'T': // set 8 LPI
        scbp->pruFragmentSize       = 0;
        scbp->isPruFragmentComplete = FALSE;
        return 0;

    case '-': // space two lines before print
        srcb = 0x23;
        break;

    case '+': // suppress carriage control (overstrike)
        srcb = 0x00;
        break;

    case ' ':
    default:
        srcb = 0x21;
        break;
        }
    if (size < 1)
        {
        fp = blank;
        size = sizeof(blank);
        }
    len  = npuHaspSendRecordHeader(tp, srcb);
    len += npuHaspSendRecordStrings(tp, fp, size);
    scbp->recordCount += 1;
    scbp->pruFragmentSize       = 0;
    scbp->isPruFragmentComplete = FALSE;
    return len;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Flush queued data upline.
**
**  Parameters:     Name        Description.
**                  scbp        SCB pointer
**                  isEof       TRUE if end of file reached
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuHaspFlushUplineData(Scb *scbp, bool isEof)
    {
    int  i;
    bool isEoi;
    bool isEor;
    Ncb  *ncbp;
    int  numBytes;
    int  recordLength;
    Tcb  *tp;

    tp    = scbp->tp;
    ncbp  = tp->pcbp->ncbp;
    isEor = FALSE;

    if (ncbp->connType == ConnTypeHasp)
        {
        if (tp->deviceType == DtCONSOLE) // console input is IVT format
            {
            recordLength = tp->inBufPtr - tp->inBufStart;
            while (recordLength > 0 && *(tp->inBufPtr - 1) == ' ') // remove trailing blanks
                {
                tp->inBufPtr -= 1;
                recordLength -= 1;
                }
            *tp->inBufPtr++ = ChrUS;
            numBytes        = tp->inBufPtr - tp->inBuf;
            }
        else if (scbp->isDiscardingRecords)
            {
            tp->inBufPtr = tp->inBufStart;
            numBytes     = tp->inBufPtr - tp->inBuf;
            }
        else // card reader input is PRU format
            {
            //
            //  Trim trailing blanks
            //
            recordLength = tp->inBufPtr - tp->inBufStart;
            while (recordLength > 0 && *(tp->inBufPtr - 1) == DcBlank)
                {
                tp->inBufPtr -= 1;
                recordLength -= 1;
                }
            //
            //  If record ends with colon (display code 00 byte), append a blank
            //  to avoid misinterpreting the colon as end of line
            //
            if ((recordLength > 0) && (*(tp->inBufPtr - 1) == 000))
                {
                *tp->inBufPtr++ = DcBlank;
                recordLength   += 1;
                }
            //
            //  If record is "/*EOR", then write end-of-record
            //
            if ((recordLength == sizeof(dcEor)) && (memcmp(tp->inBufStart, dcEor, sizeof(dcEor)) == 0))
                {
                isEor        = TRUE;
                tp->inBufPtr = tp->inBufStart;
                }
            //
            //  If record is "/*EOI", then begin discarding records until end of file reached
            //
            else if ((recordLength == sizeof(dcEoi)) && (memcmp(tp->inBufStart, dcEoi, sizeof(dcEoi)) == 0))
                {
                scbp->isDiscardingRecords = TRUE;
                tp->inBufPtr = tp->inBufStart;
                }
            //
            //  Else, append display code 00 bytes to form end of line
            //
            else
                {
                i = 10 - (recordLength % 10);
                if (i == 1)
                    {
                    i = 11;
                    }
                while (i-- > 0)
                    {
                    *tp->inBufPtr++ = 000;
                    }
                tp->inBufStart     = tp->inBufPtr;
                scbp->recordCount += 1;
                }
            }
        numBytes = tp->inBufPtr - tp->inBuf;
        }
    else // ncbp->connType == ConnTypeRevHasp
        {
        scbp->recordCount += 1;
        recordLength       = tp->inBufPtr - tp->inBufStart; // transparent record length
        if ((recordLength > 1) || (isEof == FALSE))
            {
            if (recordLength < 2) // ensure record has at least one character
                {
                *tp->inBufPtr++ = EbcdicBlank;
                recordLength   += 1;
                }
            *tp->inBufStart = recordLength;
            numBytes        = tp->inBufPtr - tp->inBuf;
            tp->inBufStart  = tp->inBufPtr++;
            }
        else
            {
            numBytes = tp->inBufStart - tp->inBuf;
            }
        }
    /*
    **  If end of record, end of information, or buffer threshold reached,
    **  send staged records upline.
    */
    if (isEof || isEor || (numBytes >= InBufThreshold))
        {
        isEoi = FALSE;
        if (ncbp->connType == ConnTypeHasp)
            {
            if (tp->deviceType == DtCONSOLE)
                {
                tp->inBuf[BlkOffDbc] = 0;
                }
            else
                {
                if (numBytes > InBufThreshold)
                    {
                    tp->inBuf[BlkOffDbc] = DbcPRU;
                    tp->inBuf[BlkOffBTBSN] = (tp->uplineBsn << BlkShiftBSN) | BtHTMSG;
                    npuHaspSendUplineData(tp, tp->inBuf, InBufThreshold);
                    npuTipInputReset(tp);
                    numBytes -= InBufThreshold;
                    memcpy(tp->inBuf + HaspPduHdrLen, tp->inBuf + InBufThreshold, numBytes);
                    numBytes += HaspPduHdrLen;
                    tp->inBufPtr = tp->inBuf + numBytes;
                    tp->inBufStart = tp->inBufPtr;
                    if (isEof == FALSE && isEor == FALSE) return;
                    }
                if (isEof)
                    {
                    tp->inBuf[BlkOffDbc] = DbcPRU | DbcEOI;
                    isEoi = TRUE;
                    }
                else if (isEor)
                    {
                    tp->inBuf[BlkOffDbc] = DbcPRU | DbcEOR;
                    }
                else
                    {
                    tp->inBuf[BlkOffDbc] = DbcPRU;
                    }
                }
            tp->inBuf[BlkOffBTBSN] = (tp->uplineBsn << BlkShiftBSN) | BtHTMSG;
            }
        else // ConnTypeRevHasp
            {
            tp->inBuf[BlkOffBTBSN] = (tp->uplineBsn << BlkShiftBSN) | (isEof ? BtHTMSG : BtHTBLK);
            tp->inBuf[BlkOffDbc]   = DbcTransparent;
            }
        npuHaspSendUplineData(tp, tp->inBuf, numBytes);
        npuTipInputReset(tp);
        if (isEoi)
            {
            npuHaspSendUplineEoiAcctg(tp, SfcEOI);
            npuHaspSendUplineEOS(tp);
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Send PCB output buffer to socket.
**
**  Parameters:     Name        Description.
**                  pcbp        PCB pointer
**
**  Returns:        TRUE if all bytes sent.
**
**------------------------------------------------------------------------*/
static bool npuHaspFlushBuffer(Pcb *pcbp)
    {
    NpuBuffer *bp;
    int       numSent;

    bp = pcbp->controls.hasp.outBuf;
    if (bp->numBytes > 0)
        {
        numSent = npuHaspSend(pcbp, bp->data + bp->offset, bp->numBytes);
        }
    else
        {
        numSent = 0;
        }
    bp->numBytes -= numSent;
    bp->offset   += numSent;
    if (bp->numBytes < 1)
        {
        bp->numBytes = bp->offset;
        bp->offset   = 0;
        npuHaspReleaseLastBlockSent(pcbp);
        pcbp->controls.hasp.lastBlockSent = bp;
        pcbp->controls.hasp.outBuf        = NULL;

        return TRUE;
        }
    else
        {
        return FALSE;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Process format control for print stream.
**
**  Parameters:     Name        Description.
**                  pcbp        PCB pointer
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuHaspProcessFormatControl(Pcb *pcbp)
    {
    u8  buf[2];
    u8  cc;
    Scb *scbp;
    u8  param;

    scbp  = pcbp->controls.hasp.designatedStream;
    param = pcbp->controls.hasp.sRCBParam & 0x3f;
    cc = param;
    if ((param & 0x20) == 0) // post-print, use last SRCB
        {
        cc = scbp->lastSRCB;
        scbp->lastSRCB = param;
        }
    else if (scbp->lastSRCB != 0)
        {
        buf[0] = npuHaspTranslateSrcbToFe(scbp->lastSRCB);
        buf[1] = EbcdicBlank;
        npuHaspStageUplineData(scbp, buf, 2);
        npuHaspFlushUplineData(scbp, FALSE);
        scbp->lastSRCB = 0;
        }
    else
        {
        scbp->lastSRCB = 0;
        }
    buf[0] = npuHaspTranslateSrcbToFe(cc);
    npuHaspStageUplineData(scbp, buf, 1);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Releases the last block sent to the peer, if any.
**
**  Parameters:     Name        Description.
**                  pcbp        PCB pointer
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuHaspReleaseLastBlockSent(Pcb *pcbp)
    {
    if (pcbp->controls.hasp.lastBlockSent != NULL)
        {
        npuBipBufRelease(pcbp->controls.hasp.lastBlockSent);
        pcbp->controls.hasp.lastBlockSent = NULL;
        pcbp->controls.hasp.retries       = 0;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Resets a stream control block.
**
**  Parameters:     Name        Description.
**                  scbp        SCB pointer
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuHaspResetScb(Scb *scbp)
    {
    NpuBuffer *bp;

    scbp->state = StHaspStreamInit;
    if (scbp->tp != NULL)
        {
        while ((bp = npuBipQueueExtract(&scbp->tp->outputQ)) != NULL)
            {
            npuBipBufRelease(bp);
            }
        }
    while ((bp = npuBipQueueExtract(&scbp->uplineQ)) != NULL)
        {
        npuBipBufRelease(bp);
        }
    memset(&scbp->params, 0, sizeof(BatchParams));
    scbp->recordCount           = 0;
    scbp->lastSRCB              = 0;
    scbp->isDiscardingRecords   = FALSE;
    scbp->isStarted             = FALSE;
    scbp->isTerminateRequested  = FALSE;
    scbp->isWaitingPTI          = FALSE;
    scbp->isPruFragmentComplete = FALSE;
    scbp->pruFragment2          = NULL;
    scbp->pruFragmentSize       = 0;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Resets the data sending deadline.
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuHaspResetSendDeadline(Tcb *tp)
    {
    NpuBuffer *bp;
    Pcb       *pcbp;

    pcbp = tp->pcbp;
    pcbp->controls.hasp.sendDeadline = 0;
    if (pcbp->controls.hasp.outBuf != NULL)
        {
        bp = pcbp->controls.hasp.outBuf;
        if ((bp->numBytes >= sizeof(ackIndication)) && (bp->offset < 1)
            && (memcmp(bp->data, ackIndication, sizeof(ackIndication)) == 0))
            {
            /*
            **  If the stream output buffer is not empty, and it contains an ACK0 indication that
            **  has not been started yet, discard it.
            **/
            npuBipBufRelease(bp);
            pcbp->controls.hasp.outBuf = NULL;
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Send HASP block header to peer.
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**
**  Returns:        Number of bytes sent.
**
**------------------------------------------------------------------------*/
static int npuHaspSendBlockHeader(Tcb *tp)
    {
    u8  header[16];
    int i;
    Pcb *pcbp;

    pcbp = tp->pcbp;

    /*
    **  Send SYN bytes and Bisync start-of-text
    */
    npuNetSend(tp, blockHeader, sizeof(blockHeader));

    /*
    **  Send BCB byte
    */
    i           = 0;
    header[i++] = 0x80 | (0 << 4) | 0; // sequence number will be inserted at transmission time

    /*
    **  Send FCS bytes
    */
    header[i++] = 0x80 | (0 << 6) | 0x0f; // normal state, all print/punch streams on
    header[i++] = 0x80 | (1 << 6) | 0x0f; // console on, all print/punch streams on
    npuNetSend(tp, header, i);
    return sizeof(blockHeader) + i;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Send HASP block trailer to peer.
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**
**  Returns:        Number of bytes sent.
**
**------------------------------------------------------------------------*/
static int npuHaspSendBlockTrailer(Tcb *tp)
    {
    u8  header[16];
    int i;

    /*
    **  Send RCB indicating end of block
    */
    i           = 0;
    header[i++] = 0;

    /*
    **  Send Bisync end of transmission
    */
    header[i++] = DLE;
    header[i++] = ETB;
    npuNetSend(tp, header, i);
    return i;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Send queued data to socket.
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**
**  Returns:        TRUE if complete block sent.
**
**------------------------------------------------------------------------*/
static bool npuHaspSendDownlineData(Tcb *tp)
    {
    NpuBuffer *bp;
    u8        *data;
    Pcb       *pcbp;
    int       result;

    if ((bp = npuBipQueueExtract(&tp->outputQ)) != NULL)
        {
        pcbp = tp->pcbp;

        /*
        **  If no bytes have been transmitted yet, and the buffer contains
        **  a message, insert a block sequence number into the BCB byte.
        */
        if ((bp->offset < 1) && (bp->numBytes > sizeof(blockHeader)))
            {
            data  = bp->data + sizeof(blockHeader); // point at BCB
            *data = (*data & 0xf0) | pcbp->controls.hasp.downlineBSN++;
            pcbp->controls.hasp.downlineBSN &= 0x0f;
            }
        data = bp->data + bp->offset;

        /*
        **  Don't call into TCP if there is no data to send.
        */
        if (bp->numBytes > 0)
            {
            result        = npuHaspSend(pcbp, data, bp->numBytes);
            bp->offset   += result;
            bp->numBytes -= result;
            }

        if (bp->numBytes < 1)
            {
            /*
            **  The socket took all of the data. If the buffer just sent was
            **  a request to initiate transmission, set the state to waiting
            **  for permission.  Otherwise, if the buffer has a block sequence
            **  number, notify the TIP so that it can acknowledge the block
            **  upline, and if the block type is BtHTMSG, it indicates EOI
            **  so set the stream state to StHaspStreamInit on Reverse HASP
            **  streams.  On HASP streams, block type BtHTMSG indicates either
            **  initial EOI or final accounting record, depending on the
            **  stream stete.  If the stream state is StHaspStreamReady, initial
            **  EOI is indicated.  If the stream state is StHaspStreamWaitActng,
            **  final accounting record is indicated. In either case, an
            **  EOI acknowledgement is sent upline. In case of initial EOI,
            **  stream state is set to StHaspStreamWaitActng to await a final
            **  accounting record from RBF. In case of final accounting record,
            **  stream state is set to StHaspStreamInit.
            */
            if ((tp->scbp != NULL) && (tp->scbp->state == StHaspStreamSendRTI))
                {
                tp->scbp->state = StHaspStreamWaitPTI;
                }
            else if (bp->blockSeqNo != 0)
                {
                npuTipNotifySent(tp, bp->blockSeqNo);
                if (((bp->blockSeqNo & BlkMaskBT) == BtHTMSG) && (tp->deviceType != DtCONSOLE))
                    {
                    if (tp->tipType == TtHASP)
                        {
                        npuHaspSendUplineEoiAcctg(tp, SfcEOI);
                        tp->scbp->state = (tp->scbp->state == StHaspStreamWaitAcctng)
                            ? StHaspStreamInit : StHaspStreamWaitAcctng;
                        }
                    else
                        {
                        tp->scbp->state = StHaspStreamInit;
                        }
                    }
                if ((bp->offset < 1) && (tp->tipType == TtHASP))
                    {
                    /*
                    **  An empty buffer with a sequence number was queued to
                    **  acknowledge an EOI or EOR indication on a HASP PRU stream,
                    **  so release the buffer, nullify the current output stream to
                    **  enable output from other streams,  and return FALSE so that
                    **  the stream remains in its current major state.
                    */
                    npuBipBufRelease(bp);
                    pcbp->controls.hasp.currentOutputStream = NULL;

                    return FALSE;
                    }
                }
            bp->numBytes = bp->offset;
            bp->offset   = 0;
            npuHaspReleaseLastBlockSent(pcbp);
            pcbp->controls.hasp.lastBlockSent = bp;
            pcbp->controls.hasp.retries       = 0;

            return TRUE;
            }

        /*
        **  Not all has been sent. Put the buffer back into the queue.
        */
        npuBipQueuePrepend(bp, &tp->outputQ);
        }

    return FALSE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Send HASP end-of-file record to peer.
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**
**  Returns:        Number of bytes sent.
**
**------------------------------------------------------------------------*/
static int npuHaspSendEofRecord(Tcb *tp)
    {
    u8 data[1];
    int len;

    len = npuHaspSendRecordHeader(tp, 0);
    data[0] = 0;
    npuNetSend(tp, data, 1);
    return len + 1;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Send HASP record header to peer.
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**                  srcb        SRCB byte for print records
**
**  Returns:        Number of bytes sent.
**
**------------------------------------------------------------------------*/
static int npuHaspSendRecordHeader(Tcb *tp, u8 srcb)
    {
    u8  header[16];
    int i;

    /*
    **  Send RCB and SRCB header bytes
    */
    i = 0;

    switch (tp->deviceType)
        {
    case DtCONSOLE:
        if (tp->pcbp->ncbp->connType == ConnTypeHasp)
            {
            header[i++] = 0x80 | (tp->streamId << 4) | 0x01; // Operator message
            header[i++] = 0x80 | (0 << 5) | (0 << 4);        // count units = 1, EBCDIC
            }
        else
            {
            header[i++] = 0x80 | (tp->streamId << 4) | 0x02; // Operator command
            header[i++] = 0x80 | (0 << 5) | (0 << 4);        // count units = 1, EBCDIC
            }
        break;

    case DtCR:
        header[i++] = 0x80 | (tp->streamId << 4) | 0x03; // Normal input record
        header[i++] = 0x80 | (0 << 5) | (0 << 4);        // count units = 1, EBCDIC
        break;

    case DtLP:
        header[i++] = 0x80 | (tp->streamId << 4) | 0x04; // Print record
        header[i++] = 0x80 | (0 << 6) | srcb;            // normal carriage control plus format effector
        break;

    case DtCP:
    case DtPLOTTER:
        header[i++] = 0x80 | (tp->streamId << 4) | 0x05; // Punch record
        header[i++] = 0x80 | (0 << 5) | (0 << 4) | 0;    // count units = 1, EBCDIC, stacker select = 0
        break;

    default:
#if DEBUG
        fprintf(npuHaspLog, "Port %02x: invalid HASP output stream type %u\n",
                tp->pcbp->claPort, tp->deviceType);
#endif

        return 0;
        }
    npuNetSend(tp, header, i);
    return i;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Send HASP record strings to peer.
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**                  data        pointer to the content of the record
**                  len         length of the content
**
**  Returns:        Number of bytes sent.
**
**------------------------------------------------------------------------*/
static int npuHaspSendRecordStrings(Tcb *tp, u8 *data, int len)
    {
    u8  header[16];
    int n;

    /*
    **  Send strings comprising of the record. Each string begins with
    **  an SCB byte definiing the length of the string. The maximum
    **  length of a string is 63 bytes, so a record longer than 63 bytes
    **  must be segmented into multiple strings. The record must end with
    **  an end-of-record SCB.
    */
    n = 0;
    while (len > 0x3f)
        {
        header[0] = (1 << 7) | (1 << 6) | 0x3f; // Non-duplicate string, max length
        npuNetSend(tp, header, 1);
        npuNetSend(tp, data, 63);
        data += 63;
        len  -= 63;
        n += 64;
        }
    if (len > 0)
        {
        header[0] = (1 << 7) | (1 << 6) | len; // Non-duplicate string, remaining record length
        npuNetSend(tp, header, 1);
        npuNetSend(tp, data, len);
        n += len + 1;
        }
    header[0] = 0;
    npuNetSend(tp, header, 1);
    return n + 1;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Send HASP signon record to peer.
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**                  data        pointer to signon record text
**                  len         length of the text
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuHaspSendSignonRecord(Tcb *tp, u8 *data, int len)
    {
    u8  header[16];
    int i;
    int n;
    Pcb *pcbp;

    pcbp = tp->pcbp;

    /*
    **  RCB and SRCB for signon record
    */
    i           = 0;
    header[i++] = 0x80 | (7 << 4) | 0;       // General control record
    header[i++] = 0x80 | asciiToEbcdic['A']; // Initial terminal signon
    npuHaspAppendRecord(pcbp, header, i);

    /*
    **  Append signon card image
    */
    n = (len > 80) ? 80 : len;
    npuHaspAppendOutput(pcbp, data, n);

    /*
    **  Blank-fill signon record to 80 characters.
    **    Note: Normally, TLF ensures that the signon record
    **          is 80 characters, so this loop should rarely,
    **          if ever, be entered.
    */
    while (len < 80) // blank-fill to 80 charcters
        {
        header[0] = asciiToEbcdic[' '];
        npuHaspAppendOutput(pcbp, header, 1);
        len += 1;
        }

    /*
    **  Append block trailer
    */
    npuHaspAppendOutput(pcbp, blockTrailer, sizeof(blockTrailer));
    }

/*--------------------------------------------------------------------------
**  Purpose:        Allocate a block and send or queue it upline.
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**                  data        Pointer to the data (block format)
**                  len         Length of the data
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuHaspSendUplineData(Tcb *tp, u8 *data, int len)
    {
    NpuBuffer *bp;

    bp = npuBipBufGet();
    bp->numBytes = len;
    memcpy(bp->data, data, bp->numBytes);
    npuBipQueueAppend(bp, &tp->scbp->uplineQ);
    npuHaspTransmitQueuedBlocks(tp);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Send EOI accounting indication upline.
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**                  sfc         Secondary function code: SfcEOI or SrcIOT
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuHaspSendUplineEoiAcctg(Tcb *tp, u8 sfc)
    {
    Scb *scbp;

    static u8 command[] =
        {
        0,                            // DN
        0,                            // SN
        0,                            // CN
        (0 << BlkShiftBSN) | BtHTCMD, // BT/BSN/PRIO
        PfcAD,
        SfcEOI,
        0, 0, 0
        };

    command[BlkOffDN]    = npuSvmCouplerNode;
    command[BlkOffSN]    = npuSvmNpuNode;
    command[BlkOffCN]    = tp->cn;
    command[BlkOffSfc]   = sfc;
    command[BlkOffBTBSN] = BtHTCMD | (tp->uplineBsn++ << BlkShiftBSN);
    if (tp->uplineBsn >= 8)
        {
        tp->uplineBsn = 1;
        }
    command[BlkOffP3] = (tp->scbp->recordCount >> 16) & 0xff;
    command[BlkOffP4] = (tp->scbp->recordCount >> 8) & 0xff;
    command[BlkOffP5] = tp->scbp->recordCount & 0xff;
    npuHaspSendUplineData(tp, command, sizeof(command));
    scbp = tp->scbp;
    scbp->recordCount           = 0;
    scbp->lastSRCB              = 0;
    scbp->pruFragmentSize       = 0;
    scbp->isPruFragmentComplete = tp->deviceType == DtLP;
#if DEBUG
    fprintf(npuHaspLog, "Port %02x: send AD/EI command to host for stream %u (%.7s)\n",
            tp->pcbp->claPort, tp->streamId, tp->termName);
    npuHaspLogBytes(command, sizeof(command), ASCII);
    npuHaspLogFlush();
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Send end of stream indication upline.
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuHaspSendUplineEOS(Tcb *tp)
    {
    static u8 commandEOS[] =
        {
        0,                            // DN
        0,                            // SN
        0,                            // CN
        (0 << BlkShiftBSN) | BtHTCMD, // BT/BSN/PRIO
        PfcIS,
        SfcES
        };

    commandEOS[BlkOffDN]    = npuSvmCouplerNode;
    commandEOS[BlkOffSN]    = npuSvmNpuNode;
    commandEOS[BlkOffCN]    = tp->cn;
    commandEOS[BlkOffBTBSN] = BtHTCMD | (tp->uplineBsn++ << BlkShiftBSN);
    if (tp->uplineBsn >= 8)
        {
        tp->uplineBsn = 1;
        }
    npuHaspSendUplineData(tp, commandEOS, sizeof(commandEOS));
#if DEBUG
    fprintf(npuHaspLog, "Port %02x: send end of stream command to host for stream %u (%.7s)\n",
            tp->pcbp->claPort, tp->streamId, tp->termName);
    npuHaspLogBytes(commandEOS, sizeof(commandEOS), ASCII);
    npuHaspLogFlush();
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Stage data for sending upline.
**
**  Parameters:     Name        Description.
**                  scbp        SCB pointer
**                  data        data to send
**                  len         length of data
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuHaspStageUplineData(Scb *scbp, u8 *data, int len)
    {
    int i;
    Ncb *ncbp;
    Tcb *tp;

    tp   = scbp->tp;
    ncbp = tp->pcbp->ncbp;

    if (ncbp->connType == ConnTypeHasp)
        {
        if (tp->deviceType == DtCONSOLE)
            {
            for (i = 0; i < len; i++)
                {
                *tp->inBufPtr++ = ebcdicToAscii[*data++];
                }
            }
        else
            {
            for (i = 0; i < len; i++)
                {
                *tp->inBufPtr++ = asciiToCdc[ebcdicToAscii[*data++]];
                }
            }
        }
    else // ncbp->connType == ConnTypeRevHasp
        {
        if (tp->inBufPtr <= tp->inBufStart)
            {
            tp->inBufPtr += 1; // reserve byte for transparent record length
            }
        for (i = 0; i < len; i++)
            {
            *tp->inBufPtr++ = *data++;
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Translate HASP carriage control indication to format effector
**
**  Parameters:     Name        Description.
**                  cc          carriage control (SRCB)
**
**  Returns:        Format effector (in EBCDIC).
**
**------------------------------------------------------------------------*/
static u8 npuHaspTranslateSrcbToFe(u8 cc)
    {
    u8 channel;
    u8 fe;

    if ((cc & 0x10) != 0) // Skip to channel
        {
        channel = cc & 0x0f;
        switch (channel)
            {
        case 1:
            fe = '1';
            break;
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
            fe = '3' + (6 - channel);
            break;
        case 12:
            fe = '2';
            break;
        default:
            fe = ' ';
            break;
            }
        }
    else // Line space
        {
        switch (cc & 0x03)
            {
        case 0:
            fe = '+';
            break;
        case 1:
        default:
            fe = ' ';
            break;
        case 2:
            fe = '0';
            break;
        case 3:
            fe = '-';
            break;
            }
        }
    return asciiToEbcdic[fe];
    }

/*--------------------------------------------------------------------------
**  Purpose:        Transmit queued blocks upline until upline block limit reached.
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuHaspTransmitQueuedBlocks(Tcb *tp)
    {
    NpuBuffer *bp;
    Scb       *scbp;

    if (tp != NULL && tp->state == StTermConnected)
        {
        scbp = tp->scbp;
        while (tp->uplineBlockLimit > 0
               && (bp = npuBipQueueExtract(&scbp->uplineQ)) != NULL)
            {
            tp->uplineBlockLimit -= 1;
            npuBipRequestUplineTransfer(bp);
#if DEBUG
            fprintf(npuHaspLog, "Port %02x: send upline data to host for stream %u (%.7s), block type %u, block len %d, dbc %02x\n",
                    tp->pcbp->claPort, tp->streamId, tp->termName, bp->data[BlkOffBTBSN] & BlkMaskBT, bp->numBytes,
                    bp->data[BlkOffDbc]);
            if (tp->pcbp->ncbp->connType == ConnTypeHasp)
                {
                npuHaspLogBytes(bp->data, bp->numBytes, (tp->deviceType == DtCONSOLE) ? ASCII : DisplayCode);
                }
            else
                {
                npuHaspLogBytes(bp->data, bp->numBytes, EBCDIC);
                }
            npuHaspLogFlush();
#endif
            }

        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Append output to PCB output buffer.
**
**  Parameters:     Name        Description.
**                  pcbp        PCB pointer
**                  data        data to be queued
**                  len         length of data
**
**  Returns:        Number of bytes actually appended.
**
**------------------------------------------------------------------------*/
static int npuHaspAppendOutput(Pcb *pcbp, u8 *data, int len)
    {
    NpuBuffer *bp;
    u8        *dp;
    u8        *start;

    bp = pcbp->controls.hasp.outBuf;
    if (bp == NULL)
        {
        pcbp->controls.hasp.outBuf = bp = npuBipBufGet();
        }
    dp = start = bp->data + bp->numBytes;
    while (len-- > 0 && bp->numBytes++ < MaxBuffer)
        {
        *dp++ = *data++;
        }

    return dp - start;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Append a record to PCB output buffer.
**
**  Parameters:     Name        Description.
**                  pcbp        PCB pointer
**                  data        data to be queued
**                  len         length of data
**
**  Returns:        Number of bytes actually appended.
**
**------------------------------------------------------------------------*/
static int npuHaspAppendRecord(Pcb *pcbp, u8 *data, int len)
    {
    u8  header[16];
    int i;

    if ((pcbp->controls.hasp.outBuf == NULL) || (pcbp->controls.hasp.outBuf->numBytes < 1))
        {
        /*
        **  Append SYN bytes and Bisync start-of-text
        */
        npuHaspAppendOutput(pcbp, blockHeader, sizeof(blockHeader));

        /*
        **  Append BCB byte
        */
        i           = 0;
        header[i++] = 0x80 | (0 << 4) | pcbp->controls.hasp.downlineBSN++; // normal block, sequence number
        pcbp->controls.hasp.downlineBSN &= 0x0f;

        /*
        **  Append FCS bytes
        */
        header[i++] = 0x80 | (0 << 6) | 0x0f; // normal state, all print/punch streams on
        header[i++] = 0x80 | (1 << 6) | 0x0f; // console on, all print/punch streams on
        npuHaspAppendOutput(pcbp, header, i);
        }

    return npuHaspAppendOutput(pcbp, data, len);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Send bytes to socket.
**
**  Parameters:     Name        Description.
**                  pcbp        PCB pointer
**                  data        data to be sent
**                  len         length of data
**
**  Returns:        Number of bytes actually sent, or -1 if error.
**
**------------------------------------------------------------------------*/
static int npuHaspSend(Pcb *pcbp, u8 *data, int len)
    {
    int n;

    n = send(pcbp->connFd, data, len, 0);
    if (n >= 0)
        {
        pcbp->controls.hasp.recvDeadline = getMilliseconds() + RecvTimeout;
#if DEBUG
        fprintf(npuHaspLog, "Port %02x: sent to terminal\n", pcbp->claPort);
        npuHaspLogBytes(data, n, EBCDIC);
        npuHaspLogFlush();
        }
    else
        {
        fprintf(npuHaspLog, "Port %02x: send failed, rc=%d\n", pcbp->claPort, errno);
        npuHaspLogBytes(data, len, EBCDIC);
        npuHaspLogFlush();
#endif
        }

    return n;
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
static void npuHaspLogFlush(void)
    {
    if (npuHaspLogBytesCol > 0)
        {
        fputs(npuHaspLogBuf, npuHaspLog);
        fputc('\n', npuHaspLog);
        fflush(npuHaspLog);
        }
    npuHaspLogBytesCol = 0;
    memset(npuHaspLogBuf, ' ', LogLineLength);
    npuHaspLogBuf[LogLineLength] = '\0';
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
static void npuHaspLogBytes(u8 *bytes, int len, CharEncoding encoding)
    {
    u8   ac;
    int  ascCol;
    u8   b;
    char hex[3];
    int  hexCol;
    int  i;

    ascCol = AsciiColumn(npuHaspLogBytesCol);
    hexCol = HexColumn(npuHaspLogBytesCol);

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
        memcpy(npuHaspLogBuf + hexCol, hex, 2);
        hexCol += 3;
        npuHaspLogBuf[ascCol++] = ac;
        if (++npuHaspLogBytesCol >= 16)
            {
            npuHaspLogFlush();
            ascCol = AsciiColumn(npuHaspLogBytesCol);
            hexCol = HexColumn(npuHaspLogBytesCol);
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Log device parameter setting
**
**  Parameters:     Name        Description.
**                  tp          pointer to TCB
**                  fn          parameter orginal
**                  fv          parameter value
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuHaspLogDevParam(Tcb *tp, u8 fn, u8 fv)
    {
    char kbuf[5];
    char *kw;

    sprintf(kbuf, "<%02x>", fn);
    kw = kbuf;
    switch (fn)
        {
    case FnDevTbsUpper:
        kw = "TbsUpper";
        break;

    case FnDevTbsLower:
        kw = "TbsLower";
        break;

    case FnDevPW:
        kw = "PW";
        break;

    case FnDevPL:
        kw = "PL";
        break;

    case FnDevPrintTrain:
        kw = "PrintTrain";
        break;

    default:
        sprintf(kbuf, "<%02x>", fn);
        kw = kbuf;
        break;
        }
    fprintf(npuHaspLog, "Set batch device parameter, connection %u (%.7s), %s=<%02x>\n", tp->cn, tp->termName, kw, fv);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Log file parameter setting
**
**  Parameters:     Name        Description.
**                  tp          pointer to TCB
**                  fn          parameter orginal
**                  fv          parameter value
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuHaspLogFileParam(Tcb *tp, u8 fn, u8 fv)
    {
    char kbuf[5];
    char *kw;

    sprintf(kbuf, "<%02x>", fn);
    kw = kbuf;
    switch (fn)
        {
    case FnFileType:
        kw = "FileType";
        break;

    case FnFileCC:
        kw = "CC";
        break;

    case FnFileLace:
        kw = "Lace";
        break;

    case FnFileLimUpper:
        kw = "FileLimitUpper";
        break;

    case FnFileLimLower:
        kw = "FileLimitLower";
        break;

    case FnFilePunchLimit:
        kw = "PunchLimit";
        break;

    default:
        sprintf(kbuf, "<%02x>", fn);
        kw = kbuf;
        break;
        }
    fprintf(npuHaspLog, "Set batch file parameter, connection %u (%.7s), %s=<%02x>\n", tp->cn, tp->termName, kw, fv);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Log a stack trace
**
**  Parameters:     none
**
**  Returns:        nothing
**
**------------------------------------------------------------------------*/
static void npuHaspPrintStackTrace(FILE *fp)
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
