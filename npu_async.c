/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter, Paul Koning
**
**  Name: npu_async.c
**
**  Description:
**      Perform emulation of the ASYNC TIP in an NPU consisting of a
**      CDC 2550 HCP running CCP.
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
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#if defined(__FreeBSD__)
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif 
#if defined(__APPLE__)
#include <execinfo.h>
#endif

#include "const.h"
#include "types.h"
#include "proto.h"
#include "npu.h"

/*
**  -----------------
**  Private Constants
**  -----------------
*/
#define MaxIvtData    100
#define Ms200         200000

/*
**  Telnet protocol elements
*/

#define TELNET_IAC              255
#define TELNET_DONT             254
#define TELNET_DO               253
#define TELNET_WONT             252
#define TELNET_WILL             251
#define TELNET_SB               250
#define TELNET_GO_AHEAD         249
#define TELNET_ERASE_LINE       248
#define TELNET_ERASE_CHAR       247
#define TELNET_AYT              246
#define TELNET_ABT_OUTPUT       245
#define TELNET_INTERRUPT        244
#define TELNET_BREAK            243
#define TELNET_DATA_MARK        242
#define TELNET_NO_OP            241
#define TELNET_SE               240

#define TELNET_OPT_BINARY       0
#define TELNET_OPT_ECHO         1
#define TELNET_OPT_SGA          3
#define TELNET_OPT_MSG_SIZE     4
#define TELNET_OPT_STATUS       5
#define TELNET_OPT_LINE_MODE    34

/*
**  -----------------------
**  Private Macro Functions
**  -----------------------
*/
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

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static void npuAsyncDoFeBefore(u8 fe);
static void npuAsyncDoFeAfter(u8 fe);
static Tcb *npuAsyncFindTcb(Pcb *pcbp);
static void npuAsyncProcessUplineTransparent(Tcb *tp);
static void npuAsyncProcessUplineAscii(Tcb *tp);
static void npuAsyncProcessUplineSpecial(Tcb *tp);
static void npuAsyncProcessUplineNormal(Tcb *tp);

#if DEBUG
static void npuAsyncLogBytes(u8 *bytes, int len);
static void npuAsyncLogFlush(void);
static void npuAsyncPrintStackTrace(FILE *fp);
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
static Tcb *npuTp;

static u8 fcSingleSpace[]   = "\r\n";
static u8 fcDoubleSpace[]   = "\r\n\n";
static u8 fcTripleSpace[]   = "\r\n\n\n";
static u8 fcBol[]           = "\r";
static u8 fcTofAnsi[]       = "\r\n\033[H";
static u8 fcTof[]           = "\f";
static u8 fcClearHomeAnsi[] = "\r\n\033[H\033[J";

static u8  netBEL[]  = { ChrBEL };
static u8  netLF[]   = { ChrLF };
static u8  netCR[]   = { ChrCR };
static u8  netCRLF[] = { ChrCR, ChrLF };
static u8  echoBuffer[1000];
static u8  *echoPtr;
static int echoLen;

static u8 telnetDo[3]      = { TELNET_IAC, TELNET_DO, 0 };
static u8 telnetDont[3]    = { TELNET_IAC, TELNET_DONT, 0 };
static u8 telnetWill[6]    = { TELNET_IAC, TELNET_WILL, 0, TELNET_IAC, TELNET_WILL, 0 };
static u8 telnetWont[3]    = { TELNET_IAC, TELNET_WONT, 0 };
static u8 iAmHereMessage[] = "\r\nYes, I am here.\r\n\r\n";

static u8 blockResetConnection[] =
    {
    0,                  // DN
    0,                  // SN
    0,                  // CN
    BtHTRESET,          // BT/BSN/PRIO
    };

#if DEBUG
static FILE *npuAsyncLog = NULL;
static char npuAsyncLogBuf[LogLineLength + 1];
static int  npuAsyncLogBytesCol = 0;
#endif

/*
 **--------------------------------------------------------------------------
 **
 **  Public Functions
 **
 **--------------------------------------------------------------------------
 */

/*--------------------------------------------------------------------------
**  Purpose:        Presets async TIP controls in a freshly allocated PCB.
**
**  Parameters:     Name        Description.
**                  pcbp        pointer to PCB
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuAsyncPresetPcb(Pcb *pcbp)
    {
    pcbp->controls.async.state        = StTelnetData;
    pcbp->controls.async.pendingWills = 0;
    pcbp->controls.async.tp           = NULL;

#if DEBUG
    if (npuAsyncLog == NULL)
        {
        npuAsyncLog = fopen("asynclog.txt", "wt");
        if (npuAsyncLog == NULL)
            {
            fprintf(stderr, "asynclog.txt - aborting\n");
            exit(1);
            }
        npuAsyncLogFlush();    // initialize log buffer
        }
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Process a break indication from the host.
**
**  Parameters:     Name        Description.
**                  tp          pointer to TCB
**                  len         length of data
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuAsyncProcessBreakIndication(Tcb *tp)
    {
    blockResetConnection[BlkOffDN] = npuSvmCouplerNode;
    blockResetConnection[BlkOffSN] = npuSvmNpuNode;
    blockResetConnection[BlkOffCN] = tp->cn;
    npuBipRequestUplineCanned(blockResetConnection, sizeof(blockResetConnection));
#if DEBUG
    fprintf(npuAsyncLog, "Port %02x: break indication for %.7s\n", tp->pcbp->claPort, tp->termName);
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Handle Telnet protocol in data received from network.
**
**  Parameters:     Name        Description.
**                  pcbp        PCB pointer
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuAsyncProcessTelnetData(Pcb *pcbp)
    {
    u8  *dp;
    u8  *lp;
    u8  opt;
    u8  *sp;
    u8  tnOutBuf[MaxBuffer];
    u8  *tnOutLimit;
    u8  *tnOutPtr;
    Tcb *tp;

    tp         = npuAsyncFindTcb(pcbp);
    sp         = pcbp->inputData;
    dp         = sp;
    tnOutPtr   = tnOutBuf;
    tnOutLimit = tnOutPtr + sizeof(tnOutBuf);
    lp         = sp + pcbp->inputCount;

#if DEBUG
    fprintf(npuAsyncLog, "Port %02x: Telnet data received from %.7s, size %d\n", pcbp->claPort, tp->termName, pcbp->inputCount);
    npuAsyncLogBytes(pcbp->inputData, pcbp->inputCount);
    npuAsyncLogFlush();
#endif

    while (sp < lp)
        {
        switch (pcbp->controls.async.state)
            {
        case StTelnetData:
            if (*sp == TELNET_IAC)
                {
                sp++;
                pcbp->controls.async.state = StTelnetProtoElem;
                }
            else if (*sp == 0x0D)
                {
                *dp++ = *sp++;
                pcbp->controls.async.state = StTelnetCR;
                }
            else
                {
                *dp++ = *sp++;
                }
            break;

        case StTelnetProtoElem:
            switch (*sp++)
            {
            case TELNET_IAC:
                *dp++ = TELNET_IAC;
                break;

            case TELNET_DONT:
                pcbp->controls.async.state = StTelnetDont;
                break;

            case TELNET_DO:
                pcbp->controls.async.state = StTelnetDo;
                break;

            case TELNET_WONT:
                pcbp->controls.async.state = StTelnetWont;
                break;

            case TELNET_WILL:
                pcbp->controls.async.state = StTelnetWill;
                break;

            case TELNET_ERASE_LINE:
                if (tp != NULL)
                    {
                    *dp++ = tp->params.fvCN;
                    }
                pcbp->controls.async.state = StTelnetData;
                break;

            case TELNET_ERASE_CHAR:
                if (tp != NULL)
                    {
                    *dp++ = tp->params.fvBS;
                    }
                pcbp->controls.async.state = StTelnetData;
                break;

            case TELNET_AYT:
                if (tnOutPtr + sizeof(iAmHereMessage) <= tnOutLimit)
                    {
                    memcpy(tnOutPtr, iAmHereMessage, sizeof(iAmHereMessage));
                    tnOutPtr += sizeof(iAmHereMessage);
                    }
                pcbp->controls.async.state = StTelnetData;
                break;

            case TELNET_ABT_OUTPUT:
                if (tp != NULL)
                    {
                    *dp++ = tp->params.fvUserBreak1;
                    }
                pcbp->controls.async.state = StTelnetData;
                break;

            case TELNET_INTERRUPT:
                if (tp != NULL)
                    {
                    *dp++ = tp->params.fvUserBreak2;
                    }
                pcbp->controls.async.state = StTelnetData;
                break;

            case TELNET_BREAK:
                if (tp != NULL)
                    {
                    *dp++ = tp->params.fvUserBreak2;
                    }
                pcbp->controls.async.state = StTelnetData;
                break;

            case TELNET_DATA_MARK:
            case TELNET_GO_AHEAD:
            case TELNET_SB:
            case TELNET_SE:
            case TELNET_NO_OP:
            default:
                pcbp->controls.async.state = StTelnetData;
                break;
            }
            break;

        case StTelnetDont:
            opt = *sp++;
            if ((opt < 8) && ((1 << opt) & pcbp->controls.async.pendingWills))
                {
                pcbp->controls.async.pendingWills &= ~(1 << opt);
                }
            else
                {
                if (tnOutPtr + 3 <= tnOutLimit)
                    {
                    telnetWont[2] = opt;
                    memcpy(tnOutPtr, telnetWont, 3);
                    tnOutPtr += 3;
                    }
                }
            pcbp->controls.async.state = StTelnetData;
            break;

        case StTelnetDo:
            opt = *sp++;
            if ((opt < 8) && ((1 << opt) & pcbp->controls.async.pendingWills))
                {
                pcbp->controls.async.pendingWills &= ~(1 << opt);
                }
            else if ((opt == TELNET_OPT_BINARY) || (opt == TELNET_OPT_ECHO) || (opt == TELNET_OPT_SGA))
                {
                if (tnOutPtr + 3 <= tnOutLimit)
                    {
                    telnetWill[2] = opt;
                    memcpy(tnOutPtr, telnetWill, 3);
                    tnOutPtr += 3;
                    }
                }
            else
                {
                if (tnOutPtr + 3 <= tnOutLimit)
                    {
                    telnetWont[2] = opt;
                    memcpy(tnOutPtr, telnetWont, 3);
                    tnOutPtr += 3;
                    }
                }
            pcbp->controls.async.state = StTelnetData;
            break;

        case StTelnetWont:
            if (tnOutPtr + 3 <= tnOutLimit)
                {
                telnetDont[2] = *sp++;
                memcpy(tnOutPtr, telnetDont, 3);
                tnOutPtr += 3;
                }
            pcbp->controls.async.state = StTelnetData;
            break;

        case StTelnetWill:
            opt = *sp++;
            if ((opt == TELNET_OPT_BINARY) || (opt == TELNET_OPT_SGA))
                {
                if (tnOutPtr + 3 <= tnOutLimit)
                    {
                    telnetDo[2] = opt;
                    memcpy(tnOutPtr, telnetDo, 3);
                    tnOutPtr += 3;
                    }
                }
            else
                {
                if (tnOutPtr + 3 <= tnOutLimit)
                    {
                    telnetDont[2] = opt;
                    memcpy(tnOutPtr, telnetDont, 3);
                    tnOutPtr += 3;
                    }
                }
            pcbp->controls.async.state = StTelnetData;
            break;

        case StTelnetCR:
            if ((*sp == 0) || (*sp == 0x0A))
                {
                sp += 1;
                }
            pcbp->controls.async.state = StTelnetData;
            break;
            }
        }

    if (tnOutPtr > tnOutBuf)
        {
        send(pcbp->connFd, tnOutBuf, tnOutPtr - tnOutBuf, 0);
#if DEBUG
        fprintf(npuAsyncLog, "Port %02x: Telnet options sent to %.7s, size %ld\n", pcbp->claPort, tp->termName, tnOutPtr - tnOutBuf);
        npuAsyncLogBytes(tnOutBuf, tnOutPtr - tnOutBuf);
        npuAsyncLogFlush();
#endif
        }

    pcbp->inputCount = dp - pcbp->inputData;
    if ((pcbp->inputCount > 0) && (tp != NULL))
        {
        npuAsyncProcessUplineData(pcbp);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Queues data to be sent to a PLATO terminal.
**
**  Parameters:     Name        Description.
**                  tp          pointer to TCB
**                  data        data to be queued
**                  len         length of data
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuAsyncPtermNetSend(Tcb *tp, u8 *data, int len)
    {
    u8  *p;
    int count;

    /*
    **  Telnet escape processing as required by Pterm
    */
    for (p = data; len > 0; len -= 1)
        {
        switch (*p++)
            {
        case 0xFF:
            /*
            **  Double FF to escape the Telnet IAC code making it a real FF.
            */
            count = p - data;
            npuNetQueueOutput(tp, data, count);
            npuNetQueueOutput(tp, (u8 *)"\xFF", 1);
#if DEBUG
            fprintf(npuAsyncLog, "Port %02x: send Pterm data to %.7s, size %d\n", tp->pcbp->claPort, tp->termName, count + 1);
            npuAsyncLogBytes(data, count);
            npuAsyncLogBytes((u8 *)"\xFF", 1);
            npuAsyncLogFlush();
#endif
            data = p;
            break;

        case 0x0D:
            /*
            **  Append zero to CR otherwise real zeroes will be stripped by Telnet.
            */
            count = p - data;
            npuNetQueueOutput(tp, data, count);
            npuNetQueueOutput(tp, (u8 *)"\x00", 1);
#if DEBUG
            fprintf(npuAsyncLog, "Port %02x: send Pterm data to %.7s, size %d\n", tp->pcbp->claPort, tp->termName, count + 1);
            npuAsyncLogBytes(data, count);
            npuAsyncLogBytes((u8 *)"\x00", 1);
            npuAsyncLogFlush();
#endif
            data = p;
            break;
            }
        }

    if ((count = p - data) > 0)
        {
        npuNetQueueOutput(tp, data, count);
#if DEBUG
        fprintf(npuAsyncLog, "Port %02x: send Pterm data to %.7s, size %d\n", tp->pcbp->claPort, tp->termName, count);
        npuAsyncLogBytes(data, count);
        npuAsyncLogFlush();
#endif
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Resets async TIP controls in a PCB.
**
**  Parameters:     Name        Description.
**                  pcbp        pointer to PCB
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuAsyncResetPcb(Pcb *pcbp)
    {
    NpuBuffer *bp;

    pcbp->controls.async.state        = StTelnetData;
    pcbp->controls.async.pendingWills = 0;
    if (pcbp->controls.async.tp != NULL)
        {
        while ((bp = npuBipQueueExtract(&pcbp->controls.async.tp->outputQ)) != NULL)
            {
            npuBipBufRelease(bp);
            }
        pcbp->controls.async.tp = NULL;
        }

#if DEBUG
    fprintf(npuAsyncLog, "Port %02x: reset PCB\n", pcbp->claPort);
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Queues data to be sent to a Telnet terminal.
**
**  Parameters:     Name        Description.
**                  tp          pointer to TCB
**                  data        data to be queued
**                  len         length of data
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuAsyncTelnetNetSend(Tcb *tp, u8 *data, int len)
    {
    static u8 iac[] = { TELNET_IAC };
    u8        *p;
    int       count;

    /*
    **  Telnet escape processing as required by Telnet protocol.
    */
    for (p = data; len > 0; len -= 1)
        {
        if (*p++ == TELNET_IAC)
            {
            /*
            **  Double FF to escape the Telnet IAC code making it a real FF.
            */
            count = p - data;
            npuNetQueueOutput(tp, data, count);
            npuNetQueueOutput(tp, iac, 1);
#if DEBUG
            fprintf(npuAsyncLog, "Port %02x: send Telnet data to %.7s, size %d\n", tp->pcbp->claPort, tp->termName, count + 1);
            npuAsyncLogBytes(data, count);
            npuAsyncLogBytes(iac, 1);
            npuAsyncLogFlush();
#endif
            data = p;
            }
        }

    if ((count = p - data) > 0)
        {
        npuNetQueueOutput(tp, data, count);
#if DEBUG
        fprintf(npuAsyncLog, "Port %02x: send Telnet data to %.7s, size %d\n", tp->pcbp->claPort, tp->termName, count);
        npuAsyncLogBytes(data, count);
        npuAsyncLogFlush();
#endif
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Try to send any queued data.
**
**  Parameters:     Name        Description.
**                  pcbp        PCB pointer
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuAsyncTryOutput(Pcb *pcbp)
    {
    NpuBuffer *bp;
    u8        *data;
    int       result;
    Tcb       *tp;

    tp = npuAsyncFindTcb(pcbp);
    if (tp == NULL)
        {
        return;
        }

    /*
    **  Handle transparent input timeout.
    */
    if (tp->xInputTimerRunning && ((cycles - tp->xStartCycle) >= Ms200))
        {
#if DEBUG
        fprintf(npuAsyncLog, "Port %02x: transparent input timeout on %.7s\n", pcbp->claPort, tp->termName);
#endif
        npuAsyncFlushUplineTransparent(tp);
        }

    /*
    **  Suspend output while x-off is in effect
    */
    if (tp->xoff == TRUE)
        {
        return;
        }

    /*
    **  Process all queued output buffers.
    */
    while ((bp = npuBipQueueExtract(&tp->outputQ)) != NULL)
        {
        data = bp->data + bp->offset;

        /*
        **  Don't call into TCP if there is no data to send.
        */
        if (bp->numBytes > 0)
            {
            result = send(pcbp->connFd, data, bp->numBytes, 0);
#if DEBUG
            if (result > 0)
                {
                fprintf(npuAsyncLog, "Port %02x: %d bytes sent to %.7s\n", tp->pcbp->claPort, result, tp->termName);
                npuAsyncLogBytes(data, result);
                npuAsyncLogFlush();
                }
#endif
            }
        else
            {
            result = 0;
            }

        if (result >= bp->numBytes)
            {
            /*
            **  The socket took all our data - let TIP know what block sequence
            **  number we processed, free the buffer and then continue.
            */
            if (bp->blockSeqNo != 0)
                {
                npuTipNotifySent(tp, bp->blockSeqNo);
                }

            npuBipBufRelease(bp);
            continue;
            }

        /*
        **  Not all has been sent. Put the buffer back into the queue.
        */
        npuBipQueuePrepend(bp, &tp->outputQ);

        /*
        **  Was there an error?
        */
        if (result < 0)
            {
            /*
            **  Likely this is a "would block" type of error - no need to do
            **  anything here. The select() call will later tell us when we
            **  can send again. Any disconnects or other errors will be handled
            **  by the receive handler.
            */
            return;
            }

        /*
        **  The socket did not take all data - update offset and count.
        */
        if (result > 0)
            {
            bp->offset   += result;
            bp->numBytes -= result;
            }
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
void npuAsyncProcessDownlineData(Tcb *tp, NpuBuffer *bp, bool last)
    {
    u8  *blk = bp->data + BlkOffData;
    int len  = bp->numBytes - BlkOffData;
    u8  dbc;
    u8  fe;
    int textlen;
    u8  *ptrUS;

    npuTp = tp;

    /*
    **  Extract Data Block Clarifier settings.
    */
    dbc  = *blk++;
    len -= 1;
    npuTp->dbcNoEchoplex  = (dbc & DbcEchoplex) != 0;
    npuTp->dbcNoCursorPos = (dbc & DbcNoCursorPos) != 0;

#if DEBUG
    fprintf(npuAsyncLog, "Port %02x: downline data received for %.7s, size %d, block type %u, dbc %02x\n",
            tp->pcbp->claPort, tp->termName, len, bp->data[BlkOffBTBSN] & BlkMaskBT, dbc);
    npuAsyncLogBytes(bp->data, bp->numBytes);
    npuAsyncLogFlush();
#endif

    if ((dbc & DbcTransparent) != 0)
        {
        npuNetSend(npuTp, blk, len);
        npuNetQueueAck(npuTp, (u8)(bp->data[BlkOffBTBSN] & (BlkMaskBSN << BlkShiftBSN)));

        return;
        }

    /*
    **  Process data.
    */
    while (len > 0)
        {
        if ((dbc & DbcNoFe) != 0)
            {
            /*
            **  Format effector is supressed - output is single-spaced.
            */
            fe = ' ';
            }
        else
            {
            fe   = *blk++;
            len -= 1;
            }

        /*
        **  Process leading format effector.
        */
        npuAsyncDoFeBefore(fe);

        if (len == 0)
            {
            /*
            **  End-of-data.
            */
            break;
            }

        /*
        **  Locate the US byte which defines the end-of-line.
        */
        ptrUS = memchr(blk, ChrUS, len);
        if (ptrUS == NULL)
            {
            /*
            **  No US byte in the rest of the buffer, so send the entire
            **  rest to the terminal.
            */
            npuNetSend(npuTp, blk, len);
            break;
            }

        /*
        **  Send the line.
        */
        textlen = ptrUS - blk;
        npuNetSend(npuTp, blk, textlen);

        /*
        **  Process trailing format effector.
        */
        if ((dbc & DbcNoCursorPos) == 0)
            {
            npuAsyncDoFeAfter(fe);
            }

        /*
        **  Advance pointer past the US byte and adjust the length.
        */
        blk += textlen + 1;
        len -= textlen + 1;
        }

    npuNetQueueAck(npuTp, (u8)(bp->data[BlkOffBTBSN] & (BlkMaskBSN << BlkShiftBSN)));
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
void npuAsyncProcessUplineData(Pcb *pcbp)
    {
    Tcb *tp;

    tp = npuAsyncFindTcb(pcbp);
    if ((tp == NULL) || (tp->state != StTermConnected))
        {
        return;
        }

#if DEBUG
    fprintf(npuAsyncLog, "Port %02x: upline data received from %.7s, size %d\n", pcbp->claPort, tp->termName, pcbp->inputCount);
    npuAsyncLogBytes(pcbp->inputData, pcbp->inputCount);
    npuAsyncLogFlush();
#endif

    echoPtr = echoBuffer;

    if (tp->params.fvXInput)
        {
        npuAsyncProcessUplineTransparent(tp);
        }
    else if (tp->params.fvFullASCII)
        {
        npuAsyncProcessUplineAscii(tp);
        }
    else if (tp->params.fvSpecialEdit)
        {
        npuAsyncProcessUplineSpecial(tp);
        }
    else
        {
        npuAsyncProcessUplineNormal(tp);
        }

    /*
    **  Optionally echo characters.
    */
    if (!tp->dbcNoEchoplex)
        {
        echoLen = echoPtr - echoBuffer;
        if (echoLen)
            {
            npuNetSend(tp, echoBuffer, echoLen);
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Flush transparent upline data from terminal.
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuAsyncFlushUplineTransparent(Tcb *tp)
    {
    if (!tp->params.fvXStickyTimeout)
        {
        /*
        **  Terminate transparent mode unless sticky timeout has been selected.
        */
        tp->params.fvXInput = FALSE;
#if DEBUG
        fprintf(npuAsyncLog, "Port %02x: terminate upline transparent mode on %.7s\n", tp->pcbp->claPort, tp->termName);
#endif
        }
#if DEBUG
    else
        {
        fprintf(npuAsyncLog, "Port %02x: continue upline transparent mode on %.7s\n", tp->pcbp->claPort, tp->termName);
        }
#endif

    /*
    **  Send the upline data.
    */
    tp->inBuf[BlkOffDbc] = DbcTransparent;
    npuBipRequestUplineCanned(tp->inBuf, tp->inBufPtr - tp->inBuf);
#if DEBUG
    fprintf(npuAsyncLog, "Port %02x: send upline transparent data for %.7s, size %ld\n",
            tp->pcbp->claPort, tp->termName, tp->inBufPtr - tp->inBuf);
    npuAsyncLogBytes(tp->inBuf, tp->inBufPtr - tp->inBuf);
    npuAsyncLogFlush();
    fprintf(npuAsyncLog, "Port %02x: cancel transparent input timer for %.7s\n", tp->pcbp->claPort, tp->termName);
#endif
    npuTipInputReset(tp);
    tp->xInputTimerRunning = FALSE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Handles a network connect notification from NET.
**
**  Parameters:     Name        Description.
**                  pcbp        pointer to PCB of connected port
**                  isPassive   TRUE if passive connection
**                                (ignored, should always be TRUE)
**
**  Returns:        TRUE if notification processed successfully.
**
**------------------------------------------------------------------------*/
bool npuAsyncNotifyNetConnect(Pcb *pcbp, bool isPassive)
    {
    npuAsyncResetPcb(pcbp);

#if DEBUG
    fprintf(npuAsyncLog, "Port %02x: request terminal connection\n", pcbp->claPort);
#endif

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
void npuAsyncNotifyNetDisconnect(Pcb *pcbp)
    {
    Tcb *tp;

    tp = npuAsyncFindTcb(pcbp);
    if (tp != NULL)
        {
#if DEBUG
        fprintf(npuAsyncLog, "Port %02x: terminal %.7s disconnected\n", pcbp->claPort, tp->termName);
#endif
        npuSvmSendDiscRequest(tp);
        }
    else
        {
#if DEBUG
        fprintf(npuAsyncLog, "Port %02x: terminal disconnected\n", pcbp->claPort);
#endif
        /*
        **  Close socket and reset PCB.
        */
        npuNetCloseConnection(pcbp);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Handles a terminal connect notification from SVM.
**
**  Parameters:     Name        Description.
**                  tp          pointer to TCB of connected terminal
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuAsyncNotifyTermConnect(Tcb *tp)
    {
    Pcb *pcbp;

    pcbp = tp->pcbp;

    if (pcbp->ncbp->connType == ConnTypeTelnet)
        {
        telnetWill[2] = TELNET_OPT_ECHO;
        telnetWill[5] = TELNET_OPT_SGA;
        pcbp->controls.async.pendingWills = (1 << TELNET_OPT_ECHO) | (1 << TELNET_OPT_SGA);
        send(pcbp->connFd, telnetWill, 6, 0);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Handles a terminal disconnect event from SVM.
**
**  Parameters:     Name        Description.
**                  tp          pointer to TCB of disconnected terminal
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuAsyncNotifyTermDisconnect(Tcb *tp)
    {
    // Nothing to be done
    }

/*
 **--------------------------------------------------------------------------
 **
 **  Private Functions
 **
 **--------------------------------------------------------------------------
 */

/*--------------------------------------------------------------------------
**  Purpose:        Process format effector at start of line
**
**  Parameters:     Name        Description.
**                            format effector
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuAsyncDoFeBefore(u8 fe)
    {
    switch (fe)
        {
    case ' ':
        if (npuTp->lastOpWasInput)
            {
            npuNetSend(npuTp, fcBol, sizeof(fcBol) - 1);
            }
        else
            {
            npuNetSend(npuTp, fcSingleSpace, sizeof(fcSingleSpace) - 1);
            }
        break;

    case '0':
        if (npuTp->lastOpWasInput)
            {
            npuNetSend(npuTp, fcSingleSpace, sizeof(fcSingleSpace) - 1);
            }
        else
            {
            npuNetSend(npuTp, fcDoubleSpace, sizeof(fcDoubleSpace) - 1);
            }
        break;

    case '-':
        if (npuTp->lastOpWasInput)
            {
            npuNetSend(npuTp, fcDoubleSpace, sizeof(fcDoubleSpace) - 1);
            }
        else
            {
            npuNetSend(npuTp, fcTripleSpace, sizeof(fcTripleSpace) - 1);
            }
        break;

    case '+':
        npuNetSend(npuTp, fcBol, sizeof(fcBol) - 1);
        break;

    case '*':
        if (npuTp->params.fvTC == TcX364)
            {
            /*
            **  Cursor Home (using ANSI/VT100 control sequences) for VT100.
            */
            npuNetSend(npuTp, fcTofAnsi, sizeof(fcTofAnsi) - 1);
            }
        else
            {
            /*
            **  Formfeed for any other terminal.
            */
            npuNetSend(npuTp, fcTof, sizeof(fcTof) - 1);
            }

        break;

    case '1':
        if (npuTp->params.fvTC == TcX364)
            {
            /*
            **  Cursor Home and Clear (using ANSI/VT100 control sequences) for VT100.
            */
            npuNetSend(npuTp, fcClearHomeAnsi, sizeof(fcClearHomeAnsi) - 1);
            }
        else
            {
            /*
            **  Formfeed for any other terminal.
            */
            npuNetSend(npuTp, fcTof, sizeof(fcTof) - 1);
            }

        break;

    case ',':
        /*
        **  Do not change position.
        */
        break;
        }

    npuTp->lastOpWasInput = FALSE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Process format effector at end of line
**
**  Parameters:     Name        Description.
**                  fe          format effector
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuAsyncDoFeAfter(u8 fe)
    {
    switch (fe)
        {
    case '.':
        npuNetSend(npuTp, fcSingleSpace, sizeof(fcSingleSpace) - 1);
        break;

    case '/':
        npuNetSend(npuTp, fcBol, sizeof(fcBol) - 1);
        break;
        }
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
static Tcb *npuAsyncFindTcb(Pcb *pcbp)
    {
    int i;
    Tcb *tp;

    if (pcbp->controls.async.tp != NULL)
        {
        return pcbp->controls.async.tp;
        }

    for (i = 1; i < MaxTcbs; i++)
        {
        tp = &npuTcbs[i];
        if ((tp->state != StTermIdle) && (tp->pcbp == pcbp))
            {
            pcbp->controls.async.tp = tp;

            return tp;
            }
        }

    return NULL;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Process upline data from terminal.
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuAsyncProcessUplineTransparent(Tcb *tp)
    {
    u8  ch;
    u8  *dp;
    int len;
    int n;
    Pcb *pcbp;

    pcbp = tp->pcbp;
    dp   = pcbp->inputData;
    len  = pcbp->inputCount;

    /*
    **  Cancel transparent input forwarding timeout.
    */
#if DEBUG
    if (tp->xInputTimerRunning)
        {
        fprintf(npuAsyncLog, "Port %02x: cancel transparent input timer on %.7s\n", pcbp->claPort, tp->termName);
        }
#endif
    tp->xInputTimerRunning = FALSE;

    /*
    **  Process transparent input.
    */
    while (len--)
        {
        ch = *dp++;

        if (tp->params.fvEchoplex)
            {
            *echoPtr++ = ch;
            }

        if (tp->params.fvXCharFlag && (ch == tp->params.fvXChar))
            {
            if (!tp->params.fvXModeMultiple)
                {
                /*
                **  Terminate single message transparent mode.
                */
                tp->params.fvXInput = FALSE;
                }

            /*
            **  Send the upline data.
            */
            tp->inBuf[BlkOffDbc] = DbcTransparent;
            npuBipRequestUplineCanned(tp->inBuf, tp->inBufPtr - tp->inBuf);
#if DEBUG
            fprintf(npuAsyncLog, "Port %02x: transparent mode termination character (%02x) detected on %.7s\n", pcbp->claPort, ch, tp->termName);
            fprintf(npuAsyncLog, "Port %02x: send upline transparent data for %.7s, size %ld\n",
                    pcbp->claPort, tp->termName, tp->inBufPtr - tp->inBuf);
            npuAsyncLogBytes(tp->inBuf, tp->inBufPtr - tp->inBuf);
            npuAsyncLogFlush();
            fprintf(npuAsyncLog, "Port %02x: %s upline transparent mode on %.7s\n", pcbp->claPort,
                    tp->params.fvXInput ? "continue" : "terminate", tp->termName);
#endif
            npuTipInputReset(tp);
            }
        else if ((ch == tp->params.fvUserBreak2) && tp->params.fvEnaXUserBreak)
            {
            *tp->inBufPtr++      = ch;
            tp->inBuf[BlkOffDbc] = DbcTransparent;
            npuBipRequestUplineCanned(tp->inBuf, tp->inBufPtr - tp->inBuf);
#if DEBUG
            fprintf(npuAsyncLog, "Port %02x: User Break2 (%02x) detected on %.7s\n", pcbp->claPort, ch, tp->termName);
            fprintf(npuAsyncLog, "Port %02x: send upline transparent data for %.7s, size %ld\n",
                    pcbp->claPort, tp->termName, tp->inBufPtr - tp->inBuf);
            npuAsyncLogBytes(tp->inBuf, tp->inBufPtr - tp->inBuf);
            npuAsyncLogFlush();
#endif
            npuTipInputReset(tp);
            }
        else
            {
            *tp->inBufPtr++ = ch;
            n = tp->inBufPtr - tp->inBufStart;
            if ((n >= tp->params.fvXCnt) || (n >= MaxBuffer - BlkOffDbc - 2))
                {
                if (!tp->params.fvXModeMultiple && n >= tp->params.fvXCnt)
                    {
                    /*
                    **  Terminate single message transparent mode.
                    */
                    tp->params.fvXInput = FALSE;
                    }

                /*
                **  Send the upline data.
                */
                tp->inBuf[BlkOffDbc] = DbcTransparent;
                npuBipRequestUplineCanned(tp->inBuf, tp->inBufPtr - tp->inBuf);
#if DEBUG
                if (n >= tp->params.fvXCnt)
                    {
                    fprintf(npuAsyncLog, "Port %02x: max transparent mode character count (%d) detected on %.7s\n", pcbp->claPort, n, tp->termName);
                    }
                fprintf(npuAsyncLog, "Port %02x: send upline transparent data for %.7s, size %ld\n",
                        pcbp->claPort, tp->termName, tp->inBufPtr - tp->inBuf);
                npuAsyncLogBytes(tp->inBuf, tp->inBufPtr - tp->inBuf);
                npuAsyncLogFlush();
                fprintf(npuAsyncLog, "Port %02x: %s upline transparent mode on %.7s\n", pcbp->claPort,
                        tp->params.fvXInput ? "continue" : "terminate", tp->termName);
#endif
                npuTipInputReset(tp);
                }
            }
        }

    /*
    **  If data is pending, schedule transparent input forwarding timeout.
    */
    if (tp->params.fvXTimeout && (tp->inBufStart != tp->inBufPtr))
        {
        tp->xStartCycle        = cycles;
        tp->xInputTimerRunning = TRUE;
#if DEBUG
        fprintf(npuAsyncLog, "Port %02x: start transparent input timer on %.7s\n", pcbp->claPort, tp->termName);
#endif
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Process upline data from terminal.
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuAsyncProcessUplineAscii(Tcb *tp)
    {
    u8  *dp;
    int len;
    u8  ch;
    Pcb *pcbp;

    pcbp = tp->pcbp;
    dp   = pcbp->inputData;
    len  = pcbp->inputCount;

    /*
    **  Process normalised input.
    */
    tp->inBuf[BlkOffDbc] = 0;   // non-transparent data

    while (len--)
        {
        ch = *dp++ & Mask7;

        /*
        **  Ignore the following characters when at the begin of a line.
        */
        if (tp->inBufPtr - tp->inBufStart == 0)
            {
            switch (ch)
                {
            case ChrNUL:
            case ChrLF:
            case ChrDEL:
                continue;
                }
            }

        if (((ch == ChrDC1) || (ch == ChrDC3))
            && tp->params.fvOutFlowControl)
            {
            /*
            **  Flow control characters if enabled.
            */
            if (ch == ChrDC1)
                {
                /*
                **  XON (turn output on)
                */
                tp->xoff = FALSE;
                }
            else
                {
                /*
                **  XOFF (turn output off)
                */
                tp->xoff = TRUE;
                }

            continue;
            }

        if ((ch == tp->params.fvCN)
            || (ch == tp->params.fvEOL))
            {
            /*
            **  EOL or Cancel entered - send the input upline.
            */
            *tp->inBufPtr++ = ch;
            npuBipRequestUplineCanned(tp->inBuf, tp->inBufPtr - tp->inBuf);
#if DEBUG
            fprintf(npuAsyncLog, "Port %02x: send upline ASCII data for %.7s, size %ld\n",
                    pcbp->claPort, tp->termName, tp->inBufPtr - tp->inBuf);
            npuAsyncLogBytes(tp->inBuf, tp->inBufPtr - tp->inBuf);
            npuAsyncLogFlush();
#endif
            npuTipInputReset(tp);

            /*
            **  Optionally echo characters.
            */
            if (tp->dbcNoEchoplex)
                {
                /*
                **  DBC prevented echoplex for this line.
                */
                tp->dbcNoEchoplex = FALSE;
                echoPtr           = echoBuffer;
                }
            else
                {
                echoLen = echoPtr - echoBuffer;
                if (echoLen)
                    {
                    npuNetSend(tp, echoBuffer, echoLen);
                    echoPtr = echoBuffer;
                    }
                }

            /*
            **  Perform cursor positioning.
            */
            if (tp->dbcNoCursorPos)
                {
                tp->dbcNoCursorPos = FALSE;
                }
            else
                {
                if (tp->params.fvCursorPos)
                    {
                    switch (tp->params.fvEOLCursorPos)
                        {
                    case 0:
                        break;

                    case 1:
                        *echoPtr++ = ChrCR;
                        break;

                    case 2:
                        *echoPtr++ = ChrLF;
                        break;

                    case 3:
                        *echoPtr++ = ChrCR;
                        *echoPtr++ = ChrLF;
                        break;
                        }
                    }
                }

            continue;
            }

        if (tp->params.fvEchoplex)
            {
            *echoPtr++ = ch;
            }

        /*
        **  Store the character for later transmission.
        */
        *tp->inBufPtr++ = ch;

        if (tp->inBufPtr - tp->inBufStart >= (tp->params.fvBlockFactor * MaxIvtData))
            {
            /*
            **  Send long lines.
            */
            tp->inBuf[BlkOffBTBSN] = BtHTBLK | (tp->uplineBsn << BlkShiftBSN);
            npuBipRequestUplineCanned(tp->inBuf, tp->inBufPtr - tp->inBuf);
#if DEBUG
            fprintf(npuAsyncLog, "Port %02x: send upline long ASCII data for %.7s, size %ld\n",
                    pcbp->claPort, tp->termName, tp->inBufPtr - tp->inBuf);
            npuAsyncLogBytes(tp->inBuf, tp->inBufPtr - tp->inBuf);
            npuAsyncLogFlush();
#endif
            npuTipInputReset(tp);
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Process upline data from terminal.
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuAsyncProcessUplineSpecial(Tcb *tp)
    {
    u8  *dp;
    int len;
    u8  ch;
    int i;
    int cnt;
    Pcb *pcbp;

    pcbp = tp->pcbp;
    dp   = pcbp->inputData;
    len  = pcbp->inputCount;

    /*
    **  Process normalised input.
    */
    tp->inBuf[BlkOffDbc] = 0;   // non-transparent data

    while (len--)
        {
        ch = *dp++ & Mask7;

        /*
        **  Ignore the following characters.
        */
        switch (ch)
            {
        case ChrNUL:
        case ChrDEL:
            continue;
            }

        /*
        **  Ignore the following characters when at the begin of a line.
        */
        if (tp->inBufPtr - tp->inBufStart == 0)
            {
            switch (ch)
                {
            case ChrSTX:
                continue;
                }
            }

        if (((ch == ChrDC1) || (ch == ChrDC3))
            && tp->params.fvOutFlowControl)
            {
            /*
            **  Flow control characters if enabled.
            */
            if (ch == ChrDC1)
                {
                /*
                **  XON (turn output on)
                */
                tp->xoff = FALSE;
                }
            else
                {
                /*
                **  XOFF (turn output off)
                */
                tp->xoff = TRUE;
                }

            continue;
            }

        if (ch == tp->params.fvCN)
            {
            /*
            **  Cancel character entered - erase all characters entered
            **  and indicate it to user via "*DEL*". Use the echobuffer
            **  to build and send the sequence.
            */
            echoPtr = echoBuffer;
            cnt     = tp->inBufPtr - tp->inBufStart;
            for (i = cnt; i > 0; i--)
                {
                *echoPtr++ = ChrBS;
                }

            for (i = cnt; i > 0; i--)
                {
                *echoPtr++ = ' ';
                }

            for (i = cnt; i > 0; i--)
                {
                *echoPtr++ = ChrBS;
                }

            *echoPtr++ = '*';
            *echoPtr++ = 'D';
            *echoPtr++ = 'E';
            *echoPtr++ = 'L';
            *echoPtr++ = '*';
            *echoPtr++ = '\r';
            *echoPtr++ = '\n';
            npuNetSend(tp, echoBuffer, echoPtr - echoBuffer);

            /*
            **  Send the line, but signal the cancel character.
            */
            tp->inBuf[BlkOffDbc] = DbcCancel;
            npuBipRequestUplineCanned(tp->inBuf, tp->inBufPtr - tp->inBuf);
#if DEBUG
            fprintf(npuAsyncLog, "Port %02x: send upline special data for %.7s, size %ld\n",
                    pcbp->claPort, tp->termName, tp->inBufPtr - tp->inBuf);
            npuAsyncLogBytes(tp->inBuf, tp->inBufPtr - tp->inBuf);
            npuAsyncLogFlush();
#endif

            /*
            **  Reset input and echoplex buffers.
            */
            npuTipInputReset(tp);
            echoPtr = echoBuffer;
            continue;
            }

        if (ch == tp->params.fvUserBreak1)
            {
            /*
            **  User break 1 (typically Ctrl-I).
            */
            npuTipSendUserBreak(tp, 1);
            continue;
            }

        if (ch == tp->params.fvUserBreak2)
            {
            /*
            **  User break 2 (typically Ctrl-T).
            */
            npuTipSendUserBreak(tp, 2);
            continue;
            }

        if (tp->params.fvEchoplex)
            {
            *echoPtr++ = ch;
            }

        if (ch == tp->params.fvEOL)
            {
            /*
            **  EOL entered - send the input upline.
            */
            npuBipRequestUplineCanned(tp->inBuf, tp->inBufPtr - tp->inBuf);
#if DEBUG
            fprintf(npuAsyncLog, "Port %02x: send upline special data for %.7s, size %ld\n",
                    pcbp->claPort, tp->termName, tp->inBufPtr - tp->inBuf);
            npuAsyncLogBytes(tp->inBuf, tp->inBufPtr - tp->inBuf);
            npuAsyncLogFlush();
#endif
            npuTipInputReset(tp);

            /*
            **  Optionally echo characters.
            */
            if (tp->dbcNoEchoplex)
                {
                /*
                **  DBC prevented echoplex for this line.
                */
                tp->dbcNoEchoplex = FALSE;
                echoPtr           = echoBuffer;
                }
            else
                {
                echoLen = echoPtr - echoBuffer;
                if (echoLen)
                    {
                    npuNetSend(tp, echoBuffer, echoLen);
                    echoPtr = echoBuffer;
                    }
                }

            /*
            **  Perform cursor positioning.
            */
            if (tp->dbcNoCursorPos)
                {
                tp->dbcNoCursorPos = FALSE;
                }
            else
                {
                if (tp->params.fvCursorPos)
                    {
                    switch (tp->params.fvEOLCursorPos)
                        {
                    case 0:
                        break;

                    case 1:
                        npuNetSend(tp, netCR, sizeof(netCR));
                        break;

                    case 2:
                        npuNetSend(tp, netLF, sizeof(netLF));
                        break;

                    case 3:
                        npuNetSend(tp, netCRLF, sizeof(netCRLF));
                        break;
                        }
                    }
                }

            continue;
            }

        /*
        **  Store the character for later transmission.
        */
        *tp->inBufPtr++ = ch;

        if (tp->inBufPtr - tp->inBufStart >= (tp->params.fvBlockFactor * MaxIvtData))
            {
            /*
            **  Send long lines.
            */
            tp->inBuf[BlkOffBTBSN] = BtHTBLK | (tp->uplineBsn << BlkShiftBSN);
            npuBipRequestUplineCanned(tp->inBuf, tp->inBufPtr - tp->inBuf);
            npuTipInputReset(tp);
#if DEBUG
            fprintf(npuAsyncLog, "Port %02x: send upline long special data for %.7s, size %ld\n",
                    pcbp->claPort, tp->termName, tp->inBufPtr - tp->inBuf);
            npuAsyncLogBytes(tp->inBuf, tp->inBufPtr - tp->inBuf);
            npuAsyncLogFlush();
#endif
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Process upline data from terminal.
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuAsyncProcessUplineNormal(Tcb *tp)
    {
    u8  *dp;
    int len;
    u8  ch;
    int i;
    int cnt;
    Pcb *pcbp;

    pcbp = tp->pcbp;
    dp   = pcbp->inputData;
    len  = pcbp->inputCount;

    /*
    **  Process normalised input.
    */
    tp->inBuf[BlkOffDbc] = 0;   // non-transparent data

    while (len--)
        {
        ch = *dp++ & Mask7;

        /*
        **  Ignore the following characters.
        */
        switch (ch)
            {
        case ChrNUL:
        case ChrLF:
        case ChrDEL:
            continue;
            }

        if (((ch == ChrDC1) || (ch == ChrDC3))
            && tp->params.fvOutFlowControl)
            {
            /*
            **  Flow control characters if enabled.
            */
            if (ch == ChrDC1)
                {
                /*
                **  XON (turn output on)
                */
                tp->xoff = FALSE;
                }
            else
                {
                /*
                **  XOFF (turn output off)
                */
                tp->xoff = TRUE;
                }
#if DEBUG
            fprintf(npuAsyncLog, "Port %02x: %s detected on %.7s\n", pcbp->claPort, tp->xoff ? "XOFF" : "XON", tp->termName);
#endif
            continue;
            }

        if (ch == tp->params.fvCN)
            {
            /*
            **  Cancel character entered - erase all characters entered
            **  and indicate it to user via "*DEL*". Use the echobuffer
            **  to build and send the sequence.
            */
            echoPtr = echoBuffer;
            cnt     = tp->inBufPtr - tp->inBufStart;
            for (i = cnt; i > 0; i--)
                {
                *echoPtr++ = ChrBS;
                }

            for (i = cnt; i > 0; i--)
                {
                *echoPtr++ = ' ';
                }

            for (i = cnt; i > 0; i--)
                {
                *echoPtr++ = ChrBS;
                }

            *echoPtr++ = '*';
            *echoPtr++ = 'D';
            *echoPtr++ = 'E';
            *echoPtr++ = 'L';
            *echoPtr++ = '*';
            *echoPtr++ = '\r';
            *echoPtr++ = '\n';
            npuNetSend(tp, echoBuffer, echoPtr - echoBuffer);

            /*
            **  Send the line, but signal the cancel character.
            */
            tp->inBuf[BlkOffDbc] = DbcCancel;
            npuBipRequestUplineCanned(tp->inBuf, tp->inBufPtr - tp->inBuf);
#if DEBUG
            fprintf(npuAsyncLog, "Port %02x: send upline normal data for %.7s, size %ld\n",
                    pcbp->claPort, tp->termName, tp->inBufPtr - tp->inBuf);
            npuAsyncLogBytes(tp->inBuf, tp->inBufPtr - tp->inBuf);
            npuAsyncLogFlush();
#endif

            /*
            **  Reset input and echoplex buffers.
            */
            npuTipInputReset(tp);
            echoPtr = echoBuffer;
            continue;
            }

        if (ch == tp->params.fvUserBreak1)
            {
            /*
            **  User break 1 (typically Ctrl-I).
            */
            npuTipSendUserBreak(tp, 1);
            continue;
            }

        if (ch == tp->params.fvUserBreak2)
            {
            /*
            **  User break 2 (typically Ctrl-T).
            */
            npuTipSendUserBreak(tp, 2);
            continue;
            }

        if (tp->params.fvEchoplex)
            {
            *echoPtr++ = ch;
            }

        if (ch == tp->params.fvEOL)
            {
            /*
            **  EOL entered - send the input upline.
            */
            npuBipRequestUplineCanned(tp->inBuf, tp->inBufPtr - tp->inBuf);
#if DEBUG
            fprintf(npuAsyncLog, "Port %02x: send upline normal data for %.7s, size %ld\n",
                    pcbp->claPort, tp->termName, tp->inBufPtr - tp->inBuf);
            npuAsyncLogBytes(tp->inBuf, tp->inBufPtr - tp->inBuf);
            npuAsyncLogFlush();
#endif
            npuTipInputReset(tp);
            tp->lastOpWasInput = TRUE;

            /*
            **  Optionally echo characters.
            */
            if (tp->dbcNoEchoplex)
                {
                /*
                **  DBC prevented echoplex for this line.
                */
                tp->dbcNoEchoplex = FALSE;
                echoPtr           = echoBuffer;
                }
            else
                {
                echoLen = echoPtr - echoBuffer;
                if (echoLen)
                    {
                    npuNetSend(tp, echoBuffer, echoLen);
                    echoPtr = echoBuffer;
                    }
                }

            /*
            **  Perform cursor positioning.
            */
            if (tp->dbcNoCursorPos)
                {
                tp->dbcNoCursorPos = FALSE;
                }
            else
                {
                if (tp->params.fvCursorPos)
                    {
                    switch (tp->params.fvEOLCursorPos)
                        {
                    case 0:
                        break;

                    case 1:
                        npuNetSend(tp, netCR, sizeof(netCR));
                        break;

                    case 2:
                        npuNetSend(tp, netLF, sizeof(netLF));
                        break;

                    case 3:
                        npuNetSend(tp, netCRLF, sizeof(netCRLF));
                        break;
                        }
                    }
                }

            continue;
            }

        if (ch == tp->params.fvBS)
            {
            /*
            **  Process backspace.
            */
            if (tp->inBufPtr > tp->inBufStart)
                {
                tp->inBufPtr -= 1;
                *echoPtr++    = ' ';
                *echoPtr++    = tp->params.fvBS;
                }
            else
                {
                /*
                **  Beep when trying to go past the start of line.
                */
                npuNetSend(tp, netBEL, 1);
                }

            continue;
            }

        /*
        **  Store the character for later transmission.
        */
        *tp->inBufPtr++ = ch;

        if (tp->inBufPtr - tp->inBufStart >= (tp->params.fvBlockFactor * MaxIvtData))
            {
            /*
            **  Send long lines.
            */
            tp->inBuf[BlkOffBTBSN] = BtHTBLK | (tp->uplineBsn << BlkShiftBSN);
            npuBipRequestUplineCanned(tp->inBuf, tp->inBufPtr - tp->inBuf);
#if DEBUG
            fprintf(npuAsyncLog, "Port %02x: send upline long normal data for %.7s, size %ld\n",
                    pcbp->claPort, tp->termName, tp->inBufPtr - tp->inBuf);
            npuAsyncLogBytes(tp->inBuf, tp->inBufPtr - tp->inBuf);
            npuAsyncLogFlush();
#endif
            npuTipInputReset(tp);
            }
        }
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
static void npuAsyncLogFlush(void)
    {
    if (npuAsyncLogBytesCol > 0)
        {
        fputs(npuAsyncLogBuf, npuAsyncLog);
        fputc('\n', npuAsyncLog);
        fflush(npuAsyncLog);
        }
    npuAsyncLogBytesCol = 0;
    memset(npuAsyncLogBuf, ' ', LogLineLength);
    npuAsyncLogBuf[LogLineLength] = '\0';
    }

/*--------------------------------------------------------------------------
**  Purpose:        Log a sequence of bytes
**
**  Parameters:     Name        Description.
**                  bytes       pointer to sequence of bytes
**                  len         length of the sequence
**
**  Returns:        nothing
**
**------------------------------------------------------------------------*/
static void npuAsyncLogBytes(u8 *bytes, int len)
    {
    u8   ac;
    int  ascCol;
    u8   b;
    char hex[3];
    int  hexCol;
    int  i;

    ascCol = AsciiColumn(npuAsyncLogBytesCol);
    hexCol = HexColumn(npuAsyncLogBytesCol);

    for (i = 0; i < len; i++)
        {
        b  = bytes[i];
        ac = b & 0x7f;
        if ((ac < 0x20) || (ac >= 0x7f))
            {
            ac = '.';
            }
        sprintf(hex, "%02x", b);
        memcpy(npuAsyncLogBuf + hexCol, hex, 2);
        hexCol += 3;
        npuAsyncLogBuf[ascCol++] = ac;
        if (++npuAsyncLogBytesCol >= 16)
            {
            npuAsyncLogFlush();
            ascCol = AsciiColumn(npuAsyncLogBytesCol);
            hexCol = HexColumn(npuAsyncLogBytesCol);
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
static void npuAsyncPrintStackTrace(FILE *fp)
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
