/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter, Paul Koning
**
**  Name: npu_hip.c
**
**  Description:
**      Perform emulation of the Host Interface Protocol in an NPU
**      consisting of a CDC 2550 HCP running CCP.
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
#include <stdarg.h>
#include "const.h"
#include "types.h"
#include "proto.h"
#include "npu.h"

/*
**  -----------------
**  Private Constants
**  -----------------
*/

/*
**  Function codes.
*/
#define FcNpuInData                00003
#define FcNpuInNpuStatus           00004
#define FcNpuInCouplerStatus       00005
#define FcNpuInNpuOrder            00006
#define FcNpuInProgram             00007

#define FcNpuOutMemAddr0           00010
#define FcNpuOutMemAddr1           00011
#define FcNpuOutData               00014
#define FcNpuOutProgram            00015
#define FcNpuOutNpuOrder           00016

#define FcNpuStartNpu              00040
#define FcNpuHaltNpu               00100
#define FcNpuClearNpu              00200
#define FcNpuClearCoupler          00400

#define FcNpuEqMask                07000

/*
**  Coupler status bits (read by PP).
*/
#define StCplrStatusLoaded         (1 << 2)
#define StCplrAddrLoaded           (1 << 3)
#define StCplrTransferCompleted    (1 << 5)
#define StCplrHostTransferTerm     (1 << 7) // PP has disconnected during the transfer
#define StCplrOrderLoaded          (1 << 8)
#define StCplrNpuStatusRead        (1 << 9)
#define StCplrTimeout              (1 << 10)

/*
**  NPU status values (read by PP when StCplrStatusLoaded is set).
*/
#define StNpuIgnore                00000
#define StNpuIdle                  00001
#define StNpuReadyOutput           00002
#define StNpuNotReadyOutput        00003
#define StNpuInputAvailLe256       00004
#define StNpuInputAvailGt256       00005
#define StNpuInputAvailPru         00006
#define StNpuInitRequest           00007
#define StNpuInitCompleted         00010

/*
**  NPU order word codes (written by PP which results in
**  StCplrOrderLoaded being set). The LSB contains the
**  block length or the new regulation level.
*/
#define OrdOutServiceMsg           0x100
#define OrdOutPriorHigh            0x200
#define OrdOutPriorLow             0x300
#define OrdNotReadyForInput        0x400
#define OrdRegulationLvlChange     0x500
#define OrdInitRequestAck          0x600

#define OrdMaskType                0xF00
#define OrdMaskValue               0x0FF

/*
**  Misc constants.
*/
#define CyclesOneSecond            100000
#define ReportInitCount            4

#if DEBUG
#define HexColumn(x)      (4 * (x) + 1 + 4)
#define AsciiColumn(x)    (HexColumn(16) + 2 + (x))
#define LogLineLength    (AsciiColumn(16))
#endif

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
typedef struct npuParam
    {
    PpWord    regCouplerStatus;
    PpWord    regNpuStatus;
    PpWord    regOrder;
    NpuBuffer *buffer;
    u8        *npuData;
    u32       lastCommandTime;
    } NpuParam;

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static void npuReset(void);
static FcStatus npuHipFunc(PpWord funcCode);
static void npuHipIo(void);
static void npuHipActivate(void);
static void npuHipDisconnect(void);
static void npuHipWriteNpuStatus(PpWord status);
static PpWord npuHipReadNpuStatus(void);
static bool npuHipDownlineBlockImpl(NpuBuffer *bp);
static bool npuHipUplineBlockImpl(NpuBuffer *bp);
static char *npuHipFunc2String(PpWord funcCode);

#if DEBUG
static void npuLogFlush(void);
static void npuLogByte(int b);

#endif

/*
**  ----------------
**  Public Variables
**  ----------------
*/
bool (*npuHipDownlineBlockFunc)(NpuBuffer *bp);
bool (*npuHipUplineBlockFunc)(NpuBuffer *bp);

#if DEBUG
FILE *npuLog = NULL;
#endif

/*
**  -----------------
**  Private Variables
**  -----------------
*/
static int      initCount = ReportInitCount;
static NpuParam *npu;

static enum
    {
    StHipInit,
    StHipIdle,
    StHipUpline,
    StHipDownline,
    }
hipState = StHipInit;

#if DEBUG
static char npuLogBuf[LogLineLength + 1];
static int  npuLogCol = 0;
#endif

/*
 **--------------------------------------------------------------------------
 **
 **  Public Functions
 **
 **--------------------------------------------------------------------------
 */
/*--------------------------------------------------------------------------
**  Purpose:        Initialise NPU.
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
void npuInit(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName)
    {
    DevSlot *dp;

    (void)unitNo;
    (void)deviceName;

#if DEBUG
    if (npuLog == NULL)
        {
        npuLog = fopen("npulog.txt", "wt");
        }
#endif

    /*
    **  Attach device to channel and initialise device control block.
    */
    dp               = channelAttach(channelNo, eqNo, DtNpu);
    dp->activate     = npuHipActivate;
    dp->disconnect   = npuHipDisconnect;
    dp->func         = npuHipFunc;
    dp->io           = npuHipIo;
    dp->selectedUnit = unitNo;
    activeDevice     = dp;

    /*
    **  Allocate and initialise NPU parameters.
    */
    npu = calloc(1, sizeof(NpuParam));
    if (npu == NULL)
        {
        fprintf(stderr, "(npu_hip) Failed to allocate npu context block\n");
        exit(1);
        }

    dp->controllerContext = npu;
    npu->regCouplerStatus = 0;
    hipState = StHipInit;
    npuHipDownlineBlockFunc = npuHipDownlineBlockImpl;
    npuHipUplineBlockFunc   = npuHipUplineBlockImpl;

    /*
    **  Initialise BIP, SVC and TIP.
    */
    npuBipInit();
    npuSvmInit();
    npuTipInit();

    /*
    **  Print a friendly message.
    */
    printf("(npu_hip) NPU initialised on channel %o equipment %o\n", channelNo, eqNo);
    printf("                 Host ID: %s\n", npuNetHostID);
    printf("            Coupler node: %u\n", npuSvmCouplerNode);
    printf("                NPU node: %u\n", npuSvmNpuNode);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Request sending of upline block.
**
**  Parameters:     Name        Description.
**                  bp          pointer to first upline buffer.
**
**  Returns:        TRUE if buffer can be accepted, FALSE otherwise.
**
**------------------------------------------------------------------------*/
bool npuHipUplineBlock(NpuBuffer *bp)
    {
    return (*npuHipUplineBlockFunc)(bp);
    }

bool npuHipUplineBlockImpl(NpuBuffer *bp)
    {
    int bits;
    int prus;
    int words;

    if (hipState != StHipIdle)
        {
        return (FALSE);
        }

    if ((bp->numBytes > BlkOffL7UB)
        && ((bp->data[BlkOffBTBSN] & BlkMaskBT) == BtHTMSG)
        && ((bp->data[BlkOffDbc] & DbcPRU) == DbcPRU))
        {
        bits  = ((bp->data[BlkOffL7BL] << 8) | bp->data[BlkOffL7BL + 1]) * 8 - bp->data[BlkOffL7UB];
        words = (bits / 60);
        if (bits % 60)
            {
            words++;
            }
        prus = words / 64;
        if (words % 64)
            {
            prus++;
            }
        if (prus < 1)
            {
            prus = 1;
            }
        npuHipWriteNpuStatus(StNpuInputAvailPru | (prus << 10));
        }
    else if (bp->numBytes <= 256)
        {
        npuHipWriteNpuStatus(StNpuInputAvailLe256);
        }
    else
        {
        npuHipWriteNpuStatus(StNpuInputAvailGt256);
        }

    npu->buffer = bp;
    hipState    = StHipUpline;

    return (TRUE);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Request reception of downline block.
**
**  Parameters:     Name        Description.
**                  bp          pointer to first downline buffer.
**
**  Returns:        TRUE if buffer can be accepted, FALSE otherwise.
**
**------------------------------------------------------------------------*/
bool npuHipDownlineBlock(NpuBuffer *bp)
    {
    return (*npuHipDownlineBlockFunc)(bp);
    }

bool npuHipDownlineBlockImpl(NpuBuffer *bp)
    {
    if (hipState != StHipIdle)
        {
        return (FALSE);
        }

    if (bp == NULL)
        {
        npuHipWriteNpuStatus(StNpuNotReadyOutput);

        return (FALSE);
        }

    npuHipWriteNpuStatus(StNpuReadyOutput);
    npu->buffer = bp;
    hipState    = StHipDownline;

    return (TRUE);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Write to the error log
**
**  Parameters:     Name        Description.
**                  format      format specifier (as in printf)
**                  ...         variable argument list
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuLogMessage(char *format, ...)
    {
#if DEBUG
    va_list args;

    npuLogFlush();
    fprintf(npuLog, "\n\n");
    va_start(args, format);
    vfprintf(npuLog, format, args);
    va_end(args);
    fprintf(npuLog, "\n");
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
**  Purpose:        Reset NPU.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuReset(void)
    {
    /*
    **  Reset all subsystems - order matters!
    */
    npuNetReset();
    npuTipReset();
    npuSvmReset();
    npuBipReset();

    /*
    **  Reset HIP state.
    */
    memset(npu, 0, sizeof(NpuParam));
    initCount = ReportInitCount;
    hipState  = StHipInit;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Execute function code on NPU.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        FcStatus
**
**------------------------------------------------------------------------*/
static FcStatus npuHipFunc(PpWord funcCode)
    {
    NpuBuffer *bp;

    funcCode &= ~FcNpuEqMask;

#if DEBUG
    npuLogFlush();
    if (funcCode != FcNpuInCouplerStatus)
        {
        fprintf(npuLog, "\n(npu_hip) %06d PP:%02o CH:%02o f:%04o T:%-25s  >   ",
                traceSequenceNo,
                activePpu->id,
                activeChannel->id,
                funcCode,
                npuHipFunc2String(funcCode));
        }
#endif

    switch (funcCode)
        {
    default:
#if DEBUG
        fprintf(npuLog, " FUNC not implemented & declined!");
#endif

        return (FcDeclined);

    case FcNpuInCouplerStatus:
        switch (hipState)
            {
        case StHipInit:
            if (initCount > 0)
                {
                /*
                **  Tell PIP a few times that the NPU has initialized.
                */
                initCount -= 1;
                npuHipWriteNpuStatus(StNpuInitCompleted);
                }
            else
                {
                hipState = StHipIdle;
                npuHipWriteNpuStatus(StNpuIdle);
                }

            break;

        case StHipIdle:
            /*
            **  Poll network status.
            */
            npuNetCheckStatus();

            /*
            **  If no upline data pending.
            */
            if (hipState == StHipIdle)
                {
                /*
                **  Announce idle state to PIP at intervals of less then one second,
                **  otherwise PIP will assume that the NPU is dead.
                */
                if (cycles - npu->lastCommandTime > CyclesOneSecond)
                    {
                    npuHipWriteNpuStatus(StNpuIdle);
                    }
                }
            break;

        default:
            break;
            }

        break;

    case FcNpuInData:
        bp = npu->buffer;
        if (bp == NULL)
            {
            /*
            **  Unexpected input request by host.
            */
            hipState     = StHipIdle;
            npu->npuData = NULL;
            activeDevice->recordLength = 0;
            activeDevice->fcode        = 0;

            return (FcDeclined);
            }

        npu->npuData = bp->data;
        activeDevice->recordLength = bp->numBytes;
        break;

    case FcNpuOutData:
        bp = npu->buffer;
        if (bp == NULL)
            {
            /*
            **  Unexpected output request by host.
            */
            hipState     = StHipIdle;
            npu->npuData = NULL;
            activeDevice->recordLength = 0;
            activeDevice->fcode        = 0;

            return (FcDeclined);
            }

        npu->npuData = bp->data;
        activeDevice->recordLength = 0;
        break;

    case FcNpuInNpuStatus:
    case FcNpuInNpuOrder:
        break;

    case FcNpuOutNpuOrder:
        hipState = StHipIdle;
        npuHipWriteNpuStatus(StNpuIdle);
        break;

    case FcNpuClearNpu:
        npuReset();
        break;

    /*
    **  The functions below are not supported and are implemented as dummies.
    */
    case FcNpuInProgram:
    case FcNpuOutMemAddr0:
    case FcNpuOutMemAddr1:
    case FcNpuOutProgram:
        break;

    case FcNpuStartNpu:
    case FcNpuHaltNpu:
    case FcNpuClearCoupler:
        return (FcProcessed);
        }

    activeDevice->fcode = funcCode;

    return (FcAccepted);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Perform I/O on NPU.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuHipIo(void)
    {
    PpWord orderType;
    u8     orderValue;

    switch (activeDevice->fcode)
        {
    default:
        break;

    case FcNpuInNpuStatus:
        activeChannel->data = npuHipReadNpuStatus();
        activeChannel->full = TRUE;
#if DEBUG
        fprintf(npuLog, " %03X", activeChannel->data);
#endif
        break;

    case FcNpuInCouplerStatus:
        activeChannel->data = npu->regCouplerStatus;
        activeChannel->full = TRUE;
#if DEBUG
        if (npu->regCouplerStatus != 0)
            {
            fprintf(npuLog, "\n(npu_hip) %06d PP:%02o CH:%02o f:%04o T:%-25s  >    %03X",
                    traceSequenceNo,
                    activePpu->id,
                    activeChannel->id,
                    FcNpuInCouplerStatus,
                    npuHipFunc2String(FcNpuInCouplerStatus),
                    activeChannel->data);
            }
#endif
        break;

    case FcNpuInNpuOrder:
        activeChannel->data = npu->regOrder;
        activeChannel->full = TRUE;
#if DEBUG
        fprintf(npuLog, " %03X", activeChannel->data);
#endif
        break;

    case FcNpuInData:
        if (activeChannel->full)
            {
            break;
            }

        if (activeDevice->recordLength > 0)
            {
            activeChannel->data = *npu->npuData++;
            activeChannel->full = TRUE;

            activeDevice->recordLength -= 1;
            if (activeDevice->recordLength == 0)
                {
                /*
                **  Transmission complete.
                */
                activeChannel->data          |= 04000;
                activeChannel->discAfterInput = TRUE;
                activeDevice->fcode           = 0;
                hipState = StHipIdle;
                npuBipNotifyUplineSent();
                }
#if DEBUG
            npuLogByte(activeChannel->data);
#endif
            }

        break;

    case FcNpuOutData:
        if (activeChannel->full)
            {
#if DEBUG
            npuLogByte(activeChannel->data);
#endif
            activeChannel->full = FALSE;
            if (activeDevice->recordLength < MaxBuffer)
                {
                *npu->npuData++             = activeChannel->data & Mask8;
                activeDevice->recordLength += 1;
                if ((activeChannel->data & 04000) != 0)
                    {
                    /*
                    **  Top bit set - process message.
                    */
                    npu->buffer->numBytes = activeDevice->recordLength;
                    activeDevice->fcode   = 0;
                    hipState = StHipIdle;
                    npuBipNotifyDownlineReceived();
                    }
                else if (activeDevice->recordLength >= MaxBuffer)
                    {
                    /*
                    **  We run out of buffer space before the end of the message.
                    */
                    activeDevice->fcode = 0;
                    hipState            = StHipIdle;
                    npuBipAbortDownlineReceived();
                    }
                }
            }

        break;

    case FcNpuOutNpuOrder:
        if (activeChannel->full)
            {
#if DEBUG
            static char *orderCode[] =
                {
                "",
                "output level one - service messages",
                "output level two - high priority",
                "output level three - low priority",
                "driver not ready for input",
                "regulation level change",
                "initialization request acknowledgment",
                ""
                };

            fprintf(npuLog, " Order word %03X - function %02X : %s",
                    activeChannel->data, activeChannel->data >> 8, orderCode[(activeChannel->data >> 8) & 7]);
#endif
            npu->regOrder       = activeChannel->data;
            orderType           = activeChannel->data & OrdMaskType;
            orderValue          = (u8)(activeChannel->data & OrdMaskValue);
            activeChannel->full = FALSE;

            switch (orderType)
                {
            case OrdOutServiceMsg:
                npuBipNotifyServiceMessage();
                break;

            case OrdOutPriorHigh:
                npuBipNotifyData(1);
                break;

            case OrdOutPriorLow:
                npuBipNotifyData(0);
                break;

            case OrdNotReadyForInput:
                npuBipRetryInput();
                break;

            case OrdRegulationLvlChange:
                npuSvmNotifyHostRegulation(orderValue);

                /*
                **  Send any pending upline blocks.
                */
                npuBipRetryInput();
                break;

            case OrdInitRequestAck:
                /*
                **  Ignore because we don't support loading, but send
                **  any pending upline blocks.
                */
                npuBipRetryInput();
                break;
                }
            }

        break;

    case FcNpuInProgram:
        /*
        **  Dummy data because we don't support dumping.
        */
        activeChannel->data = 0;
        activeChannel->full = TRUE;
        break;

    case FcNpuOutMemAddr0:
    case FcNpuOutMemAddr1:
    case FcNpuOutProgram:
        /*
        **  Ignore data because we don't support loading and dumping.
        */
        activeChannel->full = FALSE;
        break;

    case FcNpuStartNpu:
    case FcNpuHaltNpu:
    case FcNpuClearNpu:
    case FcNpuClearCoupler:
        /*
        **  Ignore loading and dumping related functions.
        */
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
static void npuHipActivate(void)
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
static void npuHipDisconnect(void)
    {
    }

/*--------------------------------------------------------------------------
**  Purpose:        NPU writes NPU status register.
**
**  Parameters:     Name        Description.
**                  status      new status register value.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuHipWriteNpuStatus(PpWord status)
    {
    npu->lastCommandTime   = cycles;
    npu->regNpuStatus      = status;
    npu->regCouplerStatus |= StCplrStatusLoaded;
    }

/*--------------------------------------------------------------------------
**  Purpose:        PP reads NPU status register.
**
**  Parameters:     Name        Description.
**
**  Returns:        NPU status register value.
**
**------------------------------------------------------------------------*/
static PpWord npuHipReadNpuStatus(void)
    {
    PpWord value = npu->regNpuStatus;

    npu->regCouplerStatus &= ~StCplrStatusLoaded;
    npu->regNpuStatus      = StNpuIgnore;

    return (value);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Convert function code to string.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        String equivalent of function code.
**
**------------------------------------------------------------------------*/
static char *npuHipFunc2String(PpWord funcCode)
    {
    static char buf[40];

#if DEBUG
    switch (funcCode)
        {
    case FcNpuInData:
        return "FcNpuInData";

    case FcNpuInNpuStatus:
        return "FcNpuInNpuStatus";

    case FcNpuInCouplerStatus:
        return "FcNpuInCouplerStatus";

    case FcNpuInNpuOrder:
        return "FcNpuInNpuOrder";

    case FcNpuInProgram:
        return "FcNpuInProgram";

    case FcNpuOutMemAddr0:
        return "FcNpuOutMemAddr0";

    case FcNpuOutMemAddr1:
        return "FcNpuOutMemAddr1";

    case FcNpuOutData:
        return "FcNpuOutData";

    case FcNpuOutProgram:
        return "FcNpuOutProgram";

    case FcNpuOutNpuOrder:
        return "FcNpuOutNpuOrder";

    case FcNpuStartNpu:
        return "FcNpuStartNpu";

    case FcNpuHaltNpu:
        return "FcNpuHaltNpu";

    case FcNpuClearNpu:
        return "FcNpuClearNpu";

    case FcNpuClearCoupler:
        return "FcNpuClearCoupler";
        }
#endif
    sprintf(buf, "(npu_hip) Unknown Function: %04o", funcCode);

    return (buf);
    }

#if DEBUG

/*--------------------------------------------------------------------------
**  Purpose:        Flush incomplete numeric/ascii data line
**
**  Parameters:     Name        Description.
**
**  Returns:        nothing
**
**------------------------------------------------------------------------*/
static void npuLogFlush(void)
    {
    if (npuLogCol != 0)
        {
        fputs(npuLogBuf, npuLog);
        }

    npuLogCol = 0;
    memset(npuLogBuf, ' ', LogLineLength);
    npuLogBuf[0]             = '\n';
    npuLogBuf[LogLineLength] = '\0';
    }

/*--------------------------------------------------------------------------
**  Purpose:        Log a byte in hex/ascii form
**
**  Parameters:     Name        Description.
**
**  Returns:        nothing
**
**------------------------------------------------------------------------*/
static void npuLogByte(int b)
    {
    char hex[5];
    int  col;

    col = HexColumn(npuLogCol);
    sprintf(hex, "%03X ", b);
    memcpy(npuLogBuf + col, hex, 4);

    col = AsciiColumn(npuLogCol);
    b  &= 0x7f;
    if ((b < 0x20) || (b >= 0x7f))
        {
        b = '.';
        }

    npuLogBuf[col] = b;
    if (++npuLogCol == 16)
        {
        npuLogFlush();
        }
    }

#endif

/*---------------------------  End Of File  ------------------------------*/
