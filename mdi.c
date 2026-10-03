/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2019, Kevin Jordan
**
**  Name: mdi.c
**
**  Author: Kevin Jordan
**
**  Description:
**      Perform emulation of the Host Interface Protocol in a CDCNet MDI.
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#if !defined(_WIN32)
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
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

/*
**  Direct function codes
*/
#define FcMdiMasterClear          0400
#define FcMdiReqGeneralStatus     0410
#define FcMdiWriteData            0420
#define FcMdiReadData             0430

/*
**  Transparent function codes
*/
#define FcMdiReqDetailedStatus    00001
#define FcMdiReadError            00003
#define FcMdiIfcReset             00004
#define FcMdiStartReg             00005
#define FcMdiStopReg              00006
#define FcMdiReqDiagnostics       00007
#define FcMdiSetProtoVersion      00032
#define FcMdiDiagEchoTimeout      00040
#define FcMdiDiagReadError        00041
#define FcMdiNormalOperation      00042
#define FcMdiNormalFlowCtrlOn     00043
#define FcMdiNormalFlowCtrlOff    00044
#define FcMdiReqProtoVersion      00200

#define FcMdiEqMask               07000

/*
**  MDI status bit masks
*/
#define MdiStatusError            04000
#define MdiStatusMemoryError      02000
#define MdiStatusDataAvailable    01000
#define MdiStatusAcceptingData    00400
#define MdiStatusBusy             00200
#define MdiStatusOperational      00100

/*
**  State values when MDI is not operational
*/
#define MdiStateMdiReset          000
#define MdiStateDiagnostics       010
#define MdiStateStarting          020
#define MdiStateInputAvailable    030
#define MdiStateLoading           040
#define MdiStateMciReset          050
#define MdiStateClosed            060
#define MdiStateDown              070

/*
**  Input available values when MDI is operational
*/
#define MdiIvtInputLe256          000
#define MdiIvtInputGt256          010
#define MdiPruOne                 020
#define MdiPruTwo                 030
#define MdiPruThree               040
#define MdiInlineDiagnostics      050

/*
**  MDI global flow control flags
*/
#define MdiFlowControlOff         0
#define MdiFlowControlOn          1

/*
**  MDI protocol version
*/
#define MdiProtocolVersion        4

/*
**  MDI header
*/
#define MdiHdrOffDstAddr          0
#define MdiHdrOffSrcAddr          6
#define MdiHdrOffBlockLen         12
#define MdiHdrOffDstSAP           14
#define MdiHdrOffSrcSAP           15
#define MdiHdrOffControl          16
#define MdiHdrOffAlignBytes       17
#define MdiHdrLen                 19

/*
**  MDI I/O word state
*/
#define MdiIoStateEvenWord        0
#define MdiIoStateOddWord         1

/*
**  -----------------------
**  Private Macro Functions
**  -----------------------
*/
#if DEBUG
#define HexColumn(x)   (4 * (x) + 1 + 4)
#define AsciiColumn(x) (HexColumn(16) + 2 + (x))
#define LogLineLength  (AsciiColumn(24))
#endif

/*
**  -----------------------------------------
**  Private Typedef and Structure Definitions
**  -----------------------------------------
*/
#define MdiMaxBuffer 3000

typedef enum
    {
    StMdiStarting = 0,
    StMdiSendRegLevel,
    StMdiOperational
    } MdiState;

typedef struct chIoBuffer
    {
    bool       isReady;
    u16        offset;
    u16        numBytes;
    u8         blockSeqNo;
    u8         data[MdiMaxBuffer];
    } ChIoBuffer;

typedef struct mdiParam
    {
    MdiState   state;
    u8         hipRequest;
#define          HipReqNone           0
#define          HipReqNotifyUpline   1
#define          HipReqRegLevel       2
#define          HipReqSupervision    3
    bool       doReset;
    u8         wordState;
    u8         headerIndex;
    u8         header[MdiHdrLen];
    u32        parcel;
    time_t     svDeadline;
    ChIoBuffer uplineData;
    ChIoBuffer downlineData;
    } MdiParam;

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static void mdiCreateThread(void);
static void mdiReset(void);
static FcStatus mdiHipFunc(PpWord funcCode);
static void mdiHipIo(void);
static void mdiHipActivate(void);
static void mdiHipDisconnect(void);
static PpWord mdiHipReadMdiStatus(void);
static bool mdiHipDownlineBlockImpl(NpuBuffer *bp);
static bool mdiHipUplineBlockImpl(NpuBuffer *bp);

#if defined(_WIN32)
static void mdiThread(void *param);
#else
static void *mdiThread(void *param);
#endif

#if DEBUG
static char *mdiHipFunc2String(PpWord funcCode);
static void mdiLogBuffer(u8 *dp);
static void mdiLogChIoBuffer(ChIoBuffer *bp);
static void mdiLogParcel(u32 parcel);
static void mdiLogFlush(void);
static void mdiLogPpWord(PpWord word);
static char *mdiPfc2String(u8 pfc);
static char *mdiSfc2String(u8 sfc);
#endif

/*
**  ----------------
**  Public Variables
**  ----------------
*/

extern bool (*npuHipDownlineBlockFunc)(NpuBuffer *bp);
extern void (*npuHipResetFunc)(void);
extern bool (*npuHipUplineBlockFunc)(NpuBuffer *bp);

#if DEBUG
static FILE *mdiLog;
#endif

/*
**  -----------------
**  Private Variables
**  -----------------
*/
static MdiParam *mdi;

static u8 detailedStartingResponse[] =
    {
    0x05,                               // channel protocol version
    0x07,                               // slot number
    0xE4, 0x31,                         // system version
    0x08, 0x00, 0x25, 0x01, 0x01, 0x01, // system ID
    0x02,                               // last I/O operation
    0x84,                               // last transparent function
    0x01, 0x10,                         // last PPU function
    0x01, 0x18,                         // last but one PPU function
    0x00, 0x00,                         // summary flags and MCI channel status
    0x00,                               // MCI status register one
    0x02,                               // MCI status register three
    0x00, 0x40,                         // software status flags
    0x00, 0x00, 0x08, 0x98,             // maximum PDU size
    0x89                                // not used, padding to make whole 24-bit parcel
    };

static u8 detailedOperationalResponse[] =
    {
    0x04,                               // channel protocol version
    0x07,                               // slot number
    0xE4, 0x31,                         // system version
    0x08, 0x00, 0x25, 0x01, 0x01, 0x01, // system ID
    0x01,                               // last I/O operation
    0x84,                               // last transparent function
    0x00, 0x84,                         // last PPU function
    0x01, 0x18,                         // last but one PPU function
    0x00, 0x00,                         // summary flags and MCI channel status
    0x00,                               // MCI status register one
    0x81,                               // MCI status register three
    0x00, 0x40,                         // software status flags
    0x00, 0x00, 0x08, 0x98,             // maximum PDU size
    0x89                                // not used, padding to make whole 24-bit parcel
    };

static u8 mdiRegLevelIndication[] =
    {
    0x00,                          // DN
    0x00,                          // SN
    0x00,                          // CN
    0x84,                          // high prio service message
    0x01,                          // PFC (logical link)
    0x01,                          // SFC (logical link)
    0x07,                          // CS, regulation level
    0x00                           // unused, padding
    };

#if DEBUG
static char mdiLogBuf[LogLineLength + 1];
static int  mdiLogParcelCol = 0;
static int  mdiLogWordCol   = 0;
#endif

/*
 **--------------------------------------------------------------------------
 **
 **  Public Functions
 **
 **--------------------------------------------------------------------------
 */
/*--------------------------------------------------------------------------
**  Purpose:        Initialise MDI.
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
void mdiInit(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName)
    {
    DevSlot *dp;

#if DEBUG
    if (mdiLog == NULL)
        {
        mdiLog = fopen("mdilog.txt", "wt");
        }
#endif

    /*
    ** set HCP software type, exit if npuSw is not SwUndefined
    */
    if (npuSw != SwUndefined)
        {
        logDtError(LogErrorLocation, "CCP and CCI devices are mutually exclusive\n");
        exit(1);
        }
    npuSw = SwCCP;

    /*
    **  Attach device to channel and initialise device control block.
    */
    dp               = channelAttach(channelNo, eqNo, DtMdi);
    dp->activate     = mdiHipActivate;
    dp->disconnect   = mdiHipDisconnect;
    dp->func         = mdiHipFunc;
    dp->io           = mdiHipIo;
    dp->selectedUnit = unitNo;
    activeDevice     = dp;

    /*
    **  Allocate and initialise MDI parameters.
    */
    mdi = calloc(1, sizeof(MdiParam));
    if (mdi == NULL)
        {
        logDtError(LogErrorLocation, "Failed to allocate mdi context block\n");
        exit(1);
        }
    dp->controllerContext   = mdi;
    npuHipDownlineBlockFunc = mdiHipDownlineBlockImpl;
    npuHipResetFunc         = mdiReset;
    npuHipUplineBlockFunc   = mdiHipUplineBlockImpl;

    mdi->state              = StMdiStarting;

    /*
    **  Initialize HIP
    */
    npuHipInit();

    /*
    **  Create MDI emulation thread
    */
    mdiCreateThread();

    /*
    **  Print a friendly message.
    */
    printf("(mdi    ) MDI initialised on channel %o equipment %o\n", channelNo, eqNo);
    printf("          Host ID: %s\n", npuNetHostID);
    printf("(mdi    ) Coupler node: %u\n", npuSvmCouplerNode);
    printf("          MDI node: %u\n", npuSvmNpuNode);
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
bool mdiHipDownlineBlockImpl(NpuBuffer *bp)
    {
    if (bp == NULL || mdi->downlineData.isReady == FALSE || mdi->downlineData.numBytes < 1)
        {
#if DEBUG
        fprintf(mdiLog, "\n    Downline block rejected, CN=%02X, BT=%02X, PDU size=%d, downline ready %s\n", bp->data[BlkOffCN],
                   bp->data[BlkOffBTBSN] & BlkMaskBT, bp->numBytes, mdi->downlineData.isReady ? "TRUE" : "FALSE");
        traceStack(mdiLog);
#endif
        return FALSE;
        }

    bp->offset     = 0;
    bp->numBytes   = mdi->downlineData.numBytes;
    bp->blockSeqNo = mdi->downlineData.blockSeqNo;
    memcpy(bp->data, mdi->downlineData.data, mdi->downlineData.numBytes);
#if DEBUG
    fputs("\n    Copy downline block from channel buffer", mdiLog);
    mdiLogChIoBuffer(&mdi->downlineData);
#endif
    mdi->downlineData.offset     = 0;
    mdi->downlineData.numBytes   = 0;
    mdi->downlineData.isReady    = FALSE;

    return TRUE;
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
bool mdiHipUplineBlockImpl(NpuBuffer *bp)
    {
    if (mdi->uplineData.isReady)
        {
#if DEBUG
        fprintf(mdiLog, "\n .   Upline block rejected, CN=%02X, BT=%02X, PDU size=%d, upline ready %s\n", bp->data[BlkOffCN],
                   bp->data[BlkOffBTBSN] & BlkMaskBT, bp->numBytes, mdi->uplineData.isReady ? "TRUE" : "FALSE");
        traceStack(mdiLog);
#endif
        return FALSE;
        }

    memcpy(mdi->uplineData.data, bp->data, bp->numBytes);
    mdi->uplineData.offset     = 0;
    mdi->uplineData.numBytes   = bp->numBytes;
    mdi->uplineData.blockSeqNo = bp->blockSeqNo;
#if DEBUG
    fputs("\n    Copy upline block to channel buffer", mdiLog);
    mdiLogChIoBuffer(&mdi->uplineData);
#endif
    mdi->uplineData.isReady    = TRUE;

    return TRUE;
    }

/*
 **--------------------------------------------------------------------------
 **
 **  Private Functions
 **
 **--------------------------------------------------------------------------
 */

/*--------------------------------------------------------------------------
**  Purpose:        Create thread which will emulate an MDI.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mdiCreateThread(void)
    {
#if defined(_WIN32)
    DWORD  dwThreadId;
    HANDLE hThread;

    /*
    **  Create TCP thread.
    */
    hThread = CreateThread(
        NULL,                                       // no security attribute
        0,                                          // default stack size
        (LPTHREAD_START_ROUTINE)mdiThread,
        (LPVOID)NULL,                               // thread parameter
        0,                                          // not suspended
        &dwThreadId);                               // returns thread ID

    if (hThread == NULL)
        {
        logDtError(LogErrorLocation, "Failed to create MDI thread\n");
        exit(1);
        }
#else
    int            rc;
    pthread_t      thread;
    pthread_attr_t attr;

    /*
    **  Create POSIX thread with default attributes.
    */
    pthread_attr_init(&attr);
    rc = pthread_create(&thread, &attr, mdiThread, NULL);
    if (rc < 0)
        {
        logDtError(LogErrorLocation, "Failed to create MDI thread\n");
        exit(1);
        }
#endif
    puts("(mdi    ) MDI thread created");
    }

/*--------------------------------------------------------------------------
**  Purpose:        MDI emulation thread.
**
**  Parameters:     Name        Description.
**                  param       unused
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
#if defined(_WIN32)
static void mdiThread(void *param)
#else
static void *mdiThread(void *param)
#endif
    {
    u8         blockType;
    ChIoBuffer *bp;
    u8         byte;
    u8         prio;

    /*
    **  Initialise BIP, SVC, TIP, and network I/O.
    */
    npuBipInit();
    npuSvmInit();
    npuTipInit();
    npuNetInit();

    for (;;)
        {
        if (mdi->doReset)
            {
            /*
            **  Reset all subsystems - order matters
            */
            cdcnetReset();
            npuNetReset();
            npuTipReset();
            npuSvmReset();
            npuBipReset();
            mdi->doReset = FALSE;
            }
        /*
        ** .Process upline actions, if any
        */
        if (mdi->hipRequest != HipReqNone)
            {
#if DEBUG
            mdiLogFlush();
            fputs("\n    ", mdiLog);
#endif
            switch (mdi->hipRequest)
                {
            case HipReqNotifyUpline:
#if DEBUG
                fputs("Notify upline block sent", mdiLog);
#endif
                npuBipNotifyUplineSent();
                break;

            case HipReqRegLevel:
#if DEBUG
                fputs("Notify upline regulation", mdiLog);
#endif
                npuBipRequestUplineCanned(mdiRegLevelIndication, sizeof(mdiRegLevelIndication));
                break;

            case HipReqSupervision:
#if DEBUG
                fputs("Request supervision", mdiLog);
#endif
                npuSvmRequestSupervision();
                break;

            default:
                break;
                }

            mdi->hipRequest = HipReqNone;
            }

        /*
        **  Process upline block, if any and channel buffer idle
        */
        if (mdi->uplineData.isReady == FALSE)
            {
            npuBipTryUplineBlock();
            }

        /*
        **  Process downline block, if any
        */
        if (mdi->downlineData.isReady)
            {
#if DEBUG
            fputs("\n    ", mdiLog);
#endif
            bp             = &mdi->downlineData;
            byte           = bp->data[BlkOffBTBSN];
            blockType      = byte & BlkMaskBT;
            prio           = (byte >> BlkShiftPRIO) & BlkMaskPRIO;
            bp->blockSeqNo = (byte >> BlkShiftBSN) & BlkMaskBSN;
            if ((blockType == BtHTCMD) && (bp->data[BlkOffCN] == 0))
                {
                if (bp->data[BlkOffPfc] == 0x01) // Link regulation
                    {
#if DEBUG
                    fputs("Notify downline host regulation", mdiLog);
#endif
                    npuSvmNotifyHostRegulation(3 | 0x04); // reg level 3 | CS
                    mdi->downlineData.isReady = FALSE;
                    }
                else
                    {
#if DEBUG
                    fputs("Notify downline service message", mdiLog);
#endif
                    npuBipNotifyServiceMessage();
                    npuBipNotifyDownlineReceived();
                    }
                }
            else
                {
#if DEBUG
                fputs("Notify downline data", mdiLog);
#endif
                npuBipNotifyData(prio);
                npuBipNotifyDownlineReceived();
                }
            }

        /*
        **  Poll network status.
        */
        npuNetCheckConnections();
        npuNetCheckStatus();
        cdcnetCheckStatus();
        }

#if !defined(_WIN32)
    return NULL;
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Set MDI reset request indication.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mdiReset(void)
    {
    mdi->doReset = TRUE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Execute function code on MDI.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        FcStatus
**
**------------------------------------------------------------------------*/
static FcStatus mdiHipFunc(PpWord funcCode)
    {
    ChIoBuffer *bp;
    time_t     currentTime;
    u16        numBytes;

    funcCode &= ~FcMdiEqMask;

#if DEBUG
    mdiLogFlush();
    fprintf(mdiLog, "\nPP:%02o CH:%02o f:%04o T:%-25s  >   ",
            activePpu->id,
            activeChannel->id,
            funcCode,
            mdiHipFunc2String(funcCode));
#endif

    switch (funcCode)
        {
    default:
        if ((funcCode >= FcMdiReqProtoVersion) && (funcCode <= FcMdiReqProtoVersion + 0177))
            {
            mdi->state = StMdiSendRegLevel;
            }
        else
            {
#if DEBUG
            fprintf(mdiLog, " FUNC not implemented & declined!");
#endif

            return FcDeclined;
            }
        break;

    case FcMdiReqGeneralStatus:
        currentTime = getSeconds();
        if (mdi->state == StMdiSendRegLevel)
            {
            mdi->state      = StMdiOperational;
            mdi->hipRequest = HipReqRegLevel;
            mdi->svDeadline = currentTime + (time_t)10; // allow 10 seconds for supervision
            }
        else
            {
            if ((mdi->state == StMdiOperational) && !npuSvmIsReady() && (currentTime >= mdi->svDeadline))
                {
#if DEBUG
                fputs("\n    Supervision timeout", mdiLog);
#endif
                npuLogMessage("Supervision timeout");
                mdi->hipRequest = HipReqSupervision;
                mdi->svDeadline = currentTime + (time_t)5;
                }
            }
        break;

    case FcMdiReqDetailedStatus:
        mdi->headerIndex           = 0;
        mdi->wordState             = MdiIoStateEvenWord;
        mdi->parcel                = 0;
        activeDevice->recordLength = sizeof(detailedOperationalResponse);
        break;

    case FcMdiReadData:
        if (mdi->uplineData.isReady == FALSE)
            {
            /*
            **  Unexpected input request by host.
            */
            activeDevice->recordLength = 0;
            activeDevice->fcode        = 0;

            return FcDeclined;
            }
        bp                                 = &mdi->uplineData;
        numBytes                           = bp->numBytes + (MdiHdrLen - MdiHdrOffDstSAP);
        bp->offset                         = 0;
        mdi->headerIndex                   = 0;
        mdi->header[MdiHdrOffBlockLen]     = numBytes >> 8;
        mdi->header[MdiHdrOffBlockLen + 1] = numBytes & 0xff;
        mdi->wordState                     = MdiIoStateEvenWord;
        mdi->parcel                        = 0;
        activeDevice->recordLength         = MdiHdrLen + bp->numBytes;
        break;

    case FcMdiWriteData:
        if (mdi->downlineData.isReady)
            {
            /*
            **  Unexpected output request by host.
            */
            activeDevice->recordLength = 0;
            activeDevice->fcode        = 0;

            return FcDeclined;
            }
        mdi->downlineData.offset           = 0;
        mdi->headerIndex                   = 0;
        mdi->header[MdiHdrOffBlockLen]     = 0;
        mdi->header[MdiHdrOffBlockLen + 1] = 0;
        mdi->wordState                     = MdiIoStateEvenWord;
        mdi->parcel                        = 0;
        bp                                 = &mdi->downlineData;
        bp->offset                         = 0;
        bp->numBytes                       = 0;
        activeDevice->recordLength         = 0;
        break;

    case FcMdiMasterClear:
        /*
        **  Reset MDI state.
        */
        memset(mdi, 0, sizeof(MdiParam));
        mdi->state   = StMdiStarting;
        mdi->doReset = TRUE;
        break;

    /*
    **  The functions below are not supported and are implemented as dummies.
    */
    case FcMdiReadError:
    case FcMdiStartReg:
    case FcMdiStopReg:
    case FcMdiReqDiagnostics:
    case FcMdiDiagEchoTimeout:
    case FcMdiDiagReadError:
        break;

    case FcMdiSetProtoVersion:
    case FcMdiIfcReset:
    case FcMdiNormalOperation:
    case FcMdiNormalFlowCtrlOn:
    case FcMdiNormalFlowCtrlOff:
    case FcMdiReqProtoVersion:
        return FcProcessed;
        }

    activeDevice->fcode = funcCode;

    return FcAccepted;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Perform I/O on MDI.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mdiHipIo(void)
    {
    ChIoBuffer *bp;
    int        i;
    int        shift;
    u8         *u8p;

    switch (activeDevice->fcode)
        {
    default:
        break;

    case FcMdiReqGeneralStatus:
        activeChannel->data           = mdiHipReadMdiStatus();
        activeChannel->full           = TRUE;
        activeChannel->discAfterInput = TRUE;
        activeDevice->fcode           = 0;
#if DEBUG
        fprintf(mdiLog, " %03X", activeChannel->data);
#endif
        break;

    case FcMdiReqDetailedStatus:
        if (activeChannel->full || (activeDevice->recordLength < 1))
            {
            break;
            }

        if (mdi->wordState == MdiIoStateEvenWord)
            {
            mdi->parcel = 0;
            u8p = (mdi->state == StMdiStarting) ? detailedStartingResponse : detailedOperationalResponse;
            for (i = 0; i < 3; i++)
                {
                mdi->parcel <<= 8;
                mdi->parcel  |= u8p[mdi->headerIndex++];
                }
            activeChannel->data = (PpWord)(mdi->parcel >> 12);
            mdi->wordState      = MdiIoStateOddWord;
            }
        else
            {
            activeChannel->data        = mdi->parcel & 0xfff;
            mdi->wordState             = MdiIoStateEvenWord;
            activeDevice->recordLength = (activeDevice->recordLength > 2) ? activeDevice->recordLength - 3 : 0;
            }

        activeChannel->full = TRUE;

#if DEBUG
        mdiLogPpWord(activeChannel->data);
        if (mdi->wordState == MdiIoStateEvenWord)
            {
            mdiLogParcel(mdi->parcel);
            }
#endif

        if (activeDevice->recordLength < 1)
            {
            /*
            **  Transmission complete.
            */
            activeChannel->discAfterInput = TRUE;
            activeDevice->fcode           = 0;
            }
        break;

    case FcMdiReadData:
        if (activeChannel->full || (mdi->uplineData.isReady == FALSE) || (activeDevice->recordLength < 1))
            {
            break;
            }
        bp = &mdi->uplineData;
        if (mdi->wordState == MdiIoStateEvenWord)
            {
            mdi->parcel = 0;
            for (i = 0; i < 3; i++)
                {
                mdi->parcel <<= 8;
                if (mdi->headerIndex < MdiHdrLen)
                    {
                    mdi->parcel |= mdi->header[mdi->headerIndex++];
                    }
                else if (bp->offset < bp->numBytes)
                    {
                    mdi->parcel |= bp->data[bp->offset++];
                    }
                }
            activeChannel->data = (PpWord)(mdi->parcel >> 12);
            mdi->wordState      = MdiIoStateOddWord;
            }
        else
            {
            activeChannel->data        = mdi->parcel & 0xfff;
            mdi->wordState             = MdiIoStateEvenWord;
            activeDevice->recordLength = (activeDevice->recordLength > 2) ? activeDevice->recordLength - 3 : 0;
            }

        activeChannel->full = TRUE;

#if DEBUG
        mdiLogPpWord(activeChannel->data);
        if (mdi->wordState == MdiIoStateEvenWord)
            {
            mdiLogParcel(mdi->parcel);
            }
#endif

        if (activeDevice->recordLength < 1)
            {
            /*
            **  Transmission complete.
            */
#if DEBUG
            mdiLogFlush();
            mdiLogBuffer(mdi->uplineData.data);
            fprintf(mdiLog, "    PDU size=%d", mdi->uplineData.numBytes);
#endif
            activeChannel->discAfterInput = TRUE;
            activeDevice->fcode           = 0;
            mdi->uplineData.isReady       = FALSE;
            mdi->hipRequest               = HipReqNotifyUpline;
            }
        break;

    case FcMdiWriteData:
        if (activeChannel->full)
            {
            activeChannel->full = FALSE;
            if (mdi->wordState == MdiIoStateEvenWord)
                {
                mdi->parcel    = activeChannel->data << 12;
                mdi->wordState = MdiIoStateOddWord;
                }
            else
                {
                mdi->parcel   |= activeChannel->data;
                mdi->wordState = MdiIoStateEvenWord;
                bp             = &mdi->downlineData;
                for (i = 0, shift = 16; i < 3; shift -= 8, i++)
                    {
                    if (mdi->headerIndex < MdiHdrLen)
                        {
                        mdi->headerIndex           += 1;
                        activeDevice->recordLength += 1;
                        }
                    else if (bp->numBytes < MdiMaxBuffer)
                        {
                        bp->data[bp->numBytes++]    = (mdi->parcel >> shift) & 0xff;
                        activeDevice->recordLength += 1;
                        }
                    }
                }
#if DEBUG
            mdiLogPpWord(activeChannel->data);
            if (mdi->wordState == MdiIoStateEvenWord)
                {
                mdiLogParcel(mdi->parcel);
                }
#endif
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
static void mdiHipActivate(void)
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
static void mdiHipDisconnect(void)
    {
    ChIoBuffer *bp;
    int        i;
    int        shift;

#if DEBUG
    mdiLogFlush();
    fprintf(mdiLog, "\nPP:%02o CH:%02o Disconnect",
            activePpu->id,
            activeChannel->id);
#endif
    //
    //  On write, disconnect indicates end of block
    //
    if (activeDevice->fcode == FcMdiWriteData)
        {
        bp = &mdi->downlineData;
        if (mdi->wordState == MdiIoStateOddWord)
            {
            for (i = 0, shift = 16; i < 2; shift -= 8, i++)
                {
                if (mdi->headerIndex < MdiHdrLen)
                    {
                    mdi->headerIndex += 1;
                    }
                else if (bp->numBytes < MdiMaxBuffer)
                    {
                    bp->data[bp->numBytes++]  = (mdi->parcel >> shift) & 0xff;
                    activeDevice->recordLength += 1;
                    }
                }
#if DEBUG
            mdiLogParcel(mdi->parcel);
#endif
            }
#if DEBUG
        mdiLogFlush();
        mdiLogBuffer(bp->data);
#endif
        if (bp->numBytes >= 2)
            {
            /*
            ** The last two bytes transmitted by PIP provide the true message length including
            ** the 19-byte MDI header.
            */
            bp->numBytes = (u16)(((bp->data[bp->numBytes - 2] << 8) | bp->data[bp->numBytes - 1]) - MdiHdrLen);
#if DEBUG
            fprintf(mdiLog, "    PDU size=%d", bp->numBytes);
#endif
            }
        activeDevice->fcode = 0;
        bp->isReady         = TRUE;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        PP reads MDI status register.
**
**  Parameters:     Name        Description.
**
**  Returns:        MDI status register value.
**
**------------------------------------------------------------------------*/
static PpWord mdiHipReadMdiStatus(void)
    {
    int        bits;
    ChIoBuffer *bp;
    PpWord     mdiStatus;
    int        prus;
    int        words;

    if (mdi->state == StMdiStarting)
        {
        return MdiStateInputAvailable;
        }

    mdiStatus = MdiStatusOperational;

    if (mdi->downlineData.isReady == FALSE)
        {
        mdiStatus |= MdiStatusAcceptingData;
        }

    if (mdi->uplineData.isReady && mdi->hipRequest == HipReqNone)
        {
        mdiStatus |= MdiStatusDataAvailable;
        bp         = &mdi->uplineData;

        if ((bp->numBytes > BlkOffDbc)
            && ((bp->data[BlkOffBTBSN] & BlkMaskBT) == BtHTMSG)
            && ((bp->data[BlkOffDbc] & DbcPRU) == DbcPRU))
            {
            bits  = (bp->numBytes - (BlkOffDbc + 1)) * (((bp->data[BlkOffDbc] & Dbc8Bit) != 0) ? 8 : 6);
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
            if (prus < 2)
                {
                mdiStatus |= MdiPruOne;
                }
            else if (prus < 3)
                {
                mdiStatus |= MdiPruTwo;
                }
            else
                {
                mdiStatus |= MdiPruThree;
                }
            }
        else if (bp->numBytes <= 256)
            {
            mdiStatus |= MdiIvtInputLe256;
            }
        else
            {
            mdiStatus |= MdiIvtInputGt256;
            }
        }

    return mdiStatus;
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
static char *mdiHipFunc2String(PpWord funcCode)
    {
    static char buf[40];

    switch (funcCode)
        {
    case FcMdiMasterClear:
        return "FcMdiMasterClear";

    case FcMdiReqGeneralStatus:
        return "FcMdiReqGeneralStatus";

    case FcMdiWriteData:
        return "FcMdiWriteData";

    case FcMdiReadData:
        return "FcMdiReadData";

    case FcMdiReqDetailedStatus:
        return "FcMdiReqDetailedStatus";

    case FcMdiReadError:
        return "FcMdiReadError";

    case FcMdiStartReg:
        return "FcMdiStartReg";

    case FcMdiStopReg:
        return "FcMdiStopReg";

    case FcMdiReqDiagnostics:
        return "FcMdiReqDiagnostics";

    case FcMdiSetProtoVersion:
        return "FcMdiSetProtoVersion";

    case FcMdiDiagEchoTimeout:
        return "FcMdiDiagEchoTimeout";

    case FcMdiDiagReadError:
        return "FcMdiDiagReadError";

    case FcMdiNormalOperation:
        return "FcMdiNormalOperation";

    case FcMdiNormalFlowCtrlOn:
        return "FcMdiNormalFlowCtrlOn";

    case FcMdiNormalFlowCtrlOff:
        return "FcMdiNormalFlowCtrlOff";

    default:
        if ((funcCode >= FcMdiReqProtoVersion) && (funcCode <= FcMdiReqProtoVersion + 0177))
            {
            sprintf(buf, "FcMdiReqProtoVersion (%04o)", funcCode);

            return buf;
            }
        else
            {
            sprintf(buf, "UNKNOWN: %04o", funcCode);

            return buf;
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Convert primary function code to string.
**
**  Parameters:     Name        Description.
**                  pfc         primary function code
**
**  Returns:        String equivalent of PFC.
**
**------------------------------------------------------------------------*/
static char *mdiPfc2String(u8 pfc)
    {
    static char buf[10];

    switch (pfc)
        {
    case 0x01:
        return "Logical Link Regulation";

    case 0x02:
        return "Initiate Connection";

    case 0x03:
        return "Terminate Connection";

    case 0x04:
        return "Change Terminal Characteristics";

    case 0x0A:
        return "Initialize NPU";

    case 0x0E:
        return "Initiate Supervision";

    case 0x0F:
        return "Configure Terminal";

    case 0x10:
        return "Enable Command(s)";

    case 0x11:
        return "Disable Command(s)";

    case 0x12:
        return "Request NPU Status";

    case 0x13:
        return "Request Logical Link Status";

    case 0x14:
        return "Request Line Status";

    case 0x15:
        return "Request Terminal Status";

    case 0x16:
        return "Request Trunk Status";

    case 0x17:
        return "Request Coupler Status";

    case 0x18:
        return "Request Svc Status";

    case 0x19:
        return "Unsolicited Status";

    case 0x1A:
        return "Statistics";

    case 0x1B:
        return "Message(s)";

    case 0x1C:
        return "Error Log Entry";

    case 0x1D:
        return "Operator Alarm";

    case 0x1E:
        return "Reload NPU";

    case 0x1F:
        return "Count(s)";

    case 0x20:
        return "Online Diagnostics";

    case 0xC1:
        return "Terminal Characteristics";

    case 0xC2:
        return "Batch Device Characteristics";

    case 0xC3:
        return "Batch File Characteristics";

    case 0xC5:
        return "Start Input";

    case 0xC9:
        return "Accounting Datq";

    default:
        sprintf(buf, "<%02X>", pfc);

        return buf;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Convert secondary function code to string.
**
**  Parameters:     Name        Description.
**                  sfc         secondary function code
**
**  Returns:        String equivalent of SFC.
**
**------------------------------------------------------------------------*/
static char *mdiSfc2String(u8 sfc)
    {
    static char buf[10];

    sfc &= 0x3f;

    switch (sfc)
        {
    case 0x00:
        return "NPU";

    case 0x01:
        return "Logical Link";

    case 0x02:
        return "Line";

    case 0x03:
        return "Terminal";

    case 0x04:
        return "Trunk";

    case 0x05:
        return "Coupler";

    case 0x06:
        return "Switched Virtual Circuit";

    case 0x07:
        return "Operator";

    case 0x08:
        return "Terminate Connection";

    case 0x09:
        return "Outbound A-A Connection";

    case 0x0A:
        return "Initiate Supervision";

    case 0x0B:
        return "Dump Option";

    case 0x0C:
        return "Program Block";

    case 0x0D:
        return "Data";

    case 0x0E:
        return "Terminate Diagnostics";

    case 0x0F:
        return "Go";

    case 0x10:
        return "Error(s)";

    case 0x11:
        return "A-A Connection";

    case 0x12:
        return "PB Perform STI";

    case 0x13:
        return "NIP Block Protocol Error";

    case 0x14:
        return "PIP Block Protocol Error";

    default:
        sprintf(buf, "<%02X>", sfc);

        return buf;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Flush incomplete numeric/ascii data line
**
**  Parameters:     Name        Description.
**
**  Returns:        nothing
**
**------------------------------------------------------------------------*/
static void mdiLogFlush(void)
    {
    if (mdiLogWordCol > 0)
        {
        fputs(mdiLogBuf, mdiLog);
        }

    mdiLogWordCol   = 0;
    mdiLogParcelCol = 0;
    memset(mdiLogBuf, ' ', LogLineLength);
    mdiLogBuf[0]             = '\n';
    mdiLogBuf[LogLineLength] = '\0';
    }

/*--------------------------------------------------------------------------
**  Purpose:        Log information about a buffer sent or received
**
**  Parameters:     Name        Description.
**                  dp          pointer to buffer
**
**  Returns:        nothing
**
**------------------------------------------------------------------------*/
static void mdiLogBuffer(u8 *dp)
    {
    u8 blockType;
    u8 byte;
    u8 sfc;

    byte      = dp[BlkOffBTBSN];
    blockType = byte & BlkMaskBT;

    fprintf(mdiLog, "\n    DN=%02X SN=%02X CN=%02X Pri=%d BSN=%d BT=",
            dp[BlkOffDN], dp[BlkOffSN], dp[BlkOffCN],
            (byte >> BlkShiftPRIO) & BlkMaskPRIO,
            (byte >> BlkShiftBSN) & BlkMaskBSN);

    switch (blockType)
        {
    case BtHTBLK:
        fputs("Block\n", mdiLog);
        break;

    case BtHTMSG:
        fputs("Message\n", mdiLog);
        break;

    case BtHTBACK:
        fputs("Back\n", mdiLog);
        break;

    case BtHTCMD:
        fputs("Command\n", mdiLog);
        fprintf(mdiLog, "    PFC=%s\n    SFC=", mdiPfc2String(dp[BlkOffPfc]));
        sfc = dp[BlkOffSfc];
        if ((sfc & SfcResp) != 0)
            {
            fprintf(mdiLog, "Normal Response, %s\n", mdiSfc2String(sfc));
            }
        else if ((sfc & SfcErr) != 0)
            {
            fprintf(mdiLog, "Abnormal Response, %s\n", mdiSfc2String(sfc));
            }
        else
            {
            fprintf(mdiLog, "Request, %s\n", mdiSfc2String(sfc));
            }
        break;

    case BtHTBREAK:
        fputs("Break\n", mdiLog);
        break;

    case BtHTQBLK:
        fputs("Qualified Block\n", mdiLog);
        break;

    case BtHTQMSG:
        fputs("Qualified Message\n", mdiLog);
        break;

    case BtHTRESET:
        fputs("Reset\n", mdiLog);
        break;

    case BtHTRINIT:
        fputs("Initialize Request\n", mdiLog);
        break;

    case BtHTNINIT:
        fputs("Initialize Response\n", mdiLog);
        break;

    case BtHTTERM:
        fputs("Terminate\n", mdiLog);
        break;

    case BtHTICMD:
        fputs("Interrupt Command\n", mdiLog);
        break;

    case BtHTICMR:
        fputs("Interrupt Command Response\n", mdiLog);
        break;

    default:
        fprintf(mdiLog, "<%02X>\n", blockType);
        break;
        }

    fflush(mdiLog);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Log the contents of a channel I/O buffer
**
**  Parameters:     Name        Description.
**                  bp          pointer to channel I/O buffer
**
**  Returns:        nothing
**
**------------------------------------------------------------------------*/
static void mdiLogChIoBuffer(ChIoBuffer *bp)
    {
    u8  *dp;
    u16 i;

    fprintf(mdiLog, "\n    Ready %s SeqNo %d Bytes %d", bp->isReady ? "TRUE" : "FALSE", bp->blockSeqNo, bp->numBytes);
    for (i = 0, dp = bp->data; i < bp->numBytes; i++)
         {
         if ((i & 0x0f) == 0)
             {
             fputs("\n   ", mdiLog);
             }
         fprintf(mdiLog, " %02x", *dp++);
         }
    mdiLogBuffer(bp->data);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Log a word sent/received on a channel in hex form
**
**  Parameters:     Name        Description.
**                  word        12-bit word sent/received on channel
**
**  Returns:        nothing
**
**------------------------------------------------------------------------*/
static void mdiLogPpWord(PpWord word)
    {
    char hex[5];
    int  col;

    col = HexColumn(mdiLogWordCol++);
    sprintf(hex, "%03X ", word);
    memcpy(mdiLogBuf + col, hex, 4);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Log a 24-bit parcel in ascii form
**
**  Parameters:     Name        Description.
**                  parcel      24-bit parcel sent/received on channel
**
**  Returns:        nothing
**
**------------------------------------------------------------------------*/
static void mdiLogParcel(u32 parcel)
    {
    u8  b;
    int col;
    int i;
    int shift;

    col = AsciiColumn(mdiLogParcelCol);

    for (i = 0, shift = 16; i < 3; shift -= 8, i++)
        {
        b = (u8)((parcel >> shift) & 0x7f);
        if ((b < 0x20) || (b >= 0x7f))
            {
            b = '.';
            }

        mdiLogBuf[col++] = b;
        }
    mdiLogParcelCol += 3;
    if (mdiLogParcelCol >= 24)
        {
        mdiLogFlush();
        }
    }

#endif

/*---------------------------  End Of File  ------------------------------*/
