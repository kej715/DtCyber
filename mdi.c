/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2019, Kevin Jordan, Tom Hunter
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

/*
**  If this is set to 1, the DEBUG flag in npu_hip.c must also be set to 1.
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
#include <time.h>


#if defined(__FreeBSD__)
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif 


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
#define MdiStateStarting          030
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
** MDI I/O word state
*/
#define MdiIoStateEvenWord        0
#define MdiIoStateOddWord         1

/*
**  -----------------------
**  Private Macro Functions
**  -----------------------
*/
#if DEBUG
#define HexColumn(x)      (4 * (x) + 1 + 4)
#define AsciiColumn(x)    (HexColumn(16) + 2 + (x))
#define LogLineLength    (AsciiColumn(24))
#endif

/*
**  -----------------------------------------
**  Private Typedef and Structure Definitions
**  -----------------------------------------
*/
#define MdiMaxBuffer    3000

typedef struct mdiBuffer
    {
    u16 offset;
    u16 numBytes;
    u8  blockSeqNo;
    u8  data[MdiMaxBuffer];
    } MdiBuffer;

typedef struct mdiParam
    {
    u8        wordState;
    u8        headerIndex;
    u8        header[MdiHdrLen];
    u32       parcel;
    time_t    svDeadline;
    NpuBuffer *uplineData;
    MdiBuffer downlineData;
    } MdiParam;

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static void mdiReset(void);
static FcStatus mdiHipFunc(PpWord funcCode);
static void mdiHipIo(void);
static void mdiHipActivate(void);
static void mdiHipDisconnect(void);
static PpWord mdiHipReadMdiStatus(void);
static bool mdiHipDownlineBlockImpl(NpuBuffer *bp);
static bool mdiHipUplineBlockImpl(NpuBuffer *bp);
static char *mdiHipFunc2String(PpWord funcCode);

#if DEBUG
static void mdiLogBuffer(u8 *dp);
static void mdiLogBytes(int b);
static void mdiLogFlush(void);
static void mdiLogPpWord(int b);
static char *mdiPfc2String(u8 pfc);
static char *mdiSfc2String(u8 sfc);

#endif
#if defined(__APPLE__)
static void mdiPrintStackTrace(FILE *fp);

#endif

/*
**  ----------------
**  Public Variables
**  ----------------
*/

extern bool (*npuHipDownlineBlockFunc)(NpuBuffer *bp);
extern bool (*npuHipUplineBlockFunc)(NpuBuffer *bp);

#if DEBUG
extern FILE *npuLog;
#endif

/*
**  -----------------
**  Private Variables
**  -----------------
*/
static enum
    {
    StMdiStarting,
    StMdiSendRegLevel,
    StMdiOperational
    }
mdiState = StMdiStarting;

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

static u8 mdiSupervisionRequest[] =
    {
    0x00,             // DN
    0x00,             // SN
    0x00,             // CN
    0x84,             // high prio service message
    0x0E,             // PFC (supervise)
    0x0A,             // SFC (initiate supervision)
    0x00,             // PS
    0x00,             // PL
    0x00,             // RI
    0x00, 0x00, 0x00, // not used
    0x03,             // CCP version
    0x01,             // ...
    0x00,             // CCP level
    0x00,             // ...
    0x00,             // CCP cycle or variant
    0x00,             // ...
    0x00,             // not used
    0x00, 0x00        // NCF version in NDL file (ignored)
    };

#if DEBUG
static char mdiLogBuf[LogLineLength + 1];
static int  mdiLogBytesCol = 0;
static int  mdiLogWordCol  = 0;
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
    if (npuLog == NULL)
        {
        npuLog = fopen("mdilog.txt", "wt");
        }
#endif

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
        fprintf(stderr, "Failed to allocate mdi context block\n");
        exit(1);
        }

    dp->controllerContext   = mdi;
    npuHipDownlineBlockFunc = mdiHipDownlineBlockImpl;
    npuHipUplineBlockFunc   = mdiHipUplineBlockImpl;

    /*
    **  Initialize node numbers in upline canned messages
    */
    mdiSupervisionRequest[BlkOffDN] = npuSvmCouplerNode;
    mdiSupervisionRequest[BlkOffSN] = npuSvmNpuNode;

    /*
    **  Initialise BIP, SVC, TIP, and LIP.
    */
    npuBipInit();
    npuSvmInit();
    npuTipInit();

    mdiState = StMdiStarting;

    /*
    **  Print a friendly message.
    */
    printf("(mdi    ) MDI initialised on channel %o equipment %o\n", channelNo, eqNo);
    printf("          Host ID: %s\n", npuNetHostID);
    printf("(mdi    ) Coupler node: %u\n", npuSvmCouplerNode);
    printf("          MDI node: %u\n", npuSvmNpuNode);
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
    if (mdi->uplineData != NULL)
        {
#if DEBUG
        if ((bp != mdi->uplineData)
            || (bp->data[BlkOffCN] != mdi->uplineData->data[BlkOffCN]))
            {
            fprintf(stderr, "(mdi     ) MDI upline block rejected, CN=%02X, BT=%02X, PDU size=%d\n", bp->data[BlkOffCN],
                    bp->data[BlkOffBTBSN] & BlkMaskBT, bp->numBytes);
            mdiPrintStackTrace(stderr);
            }
#endif

        return (FALSE);
        }

    mdi->uplineData = bp;

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
bool mdiHipDownlineBlockImpl(NpuBuffer *bp)
    {
    if ((bp == NULL) || (mdi->downlineData.numBytes < 1))
        {
        return (FALSE);
        }

    bp->offset     = 0;
    bp->numBytes   = mdi->downlineData.numBytes;
    bp->blockSeqNo = mdi->downlineData.blockSeqNo;
    memcpy(bp->data, mdi->downlineData.data, mdi->downlineData.numBytes);

    return (TRUE);
    }

/*
 **--------------------------------------------------------------------------
 **
 **  Private Functions
 **
 **--------------------------------------------------------------------------
 */

/*--------------------------------------------------------------------------
**  Purpose:        Reset MDI.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mdiReset(void)
    {
    /*
    **  Reset all subsystems - order matters!
    */
    cdcnetReset();
    npuNetReset();
    npuTipReset();
    npuSvmReset();
    npuBipReset();

    /*
    **  Reset HIP state.
    */
    memset(mdi, 0, sizeof(MdiParam));

    mdiState = StMdiStarting;
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
    time_t    currentTime;
    MdiBuffer *mbp;
    NpuBuffer *nbp;
    u16       numBytes;

    funcCode &= ~FcMdiEqMask;

#if DEBUG
    mdiLogFlush();
    fprintf(npuLog, "\n%06d PP:%02o CH:%02o f:%04o T:%-25s  >   ",
            traceSequenceNo,
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
            mdiState = StMdiSendRegLevel;
            }
        else
            {
#if DEBUG
            fprintf(npuLog, " FUNC not implemented & declined!");
#endif

            return (FcDeclined);
            }
        break;

    case FcMdiReqGeneralStatus:
        currentTime = getSeconds();
        if (mdiState == StMdiSendRegLevel)
            {
            npuBipRequestUplineCanned(mdiRegLevelIndication, sizeof(mdiRegLevelIndication));
            mdi->svDeadline = currentTime + (time_t)10; // allow 10 seconds for supervision
            mdiState        = StMdiOperational;
            }
        else
            {
            if ((mdiState == StMdiOperational) && !npuSvmIsReady() && (currentTime >= mdi->svDeadline))
                {
                npuLogMessage("Supervision timeout");
                npuBipRequestUplineCanned(mdiSupervisionRequest, sizeof(mdiSupervisionRequest));
                mdi->svDeadline = currentTime + (time_t)5;
                }

            /*
            **  Poll network status.
            */
            npuNetCheckStatus();
            cdcnetCheckStatus();
            }
        break;

    case FcMdiReqDetailedStatus:
        mdi->headerIndex           = 0;
        mdi->wordState             = MdiIoStateEvenWord;
        mdi->parcel                = 0;
        activeDevice->recordLength = sizeof(detailedOperationalResponse);
        break;

    case FcMdiReadData:
        nbp = mdi->uplineData;
        if (nbp == NULL)
            {
            /*
            **  Unexpected input request by host.
            */
            activeDevice->recordLength = 0;
            activeDevice->fcode        = 0;

            return (FcDeclined);
            }

        numBytes         = nbp->numBytes + (MdiHdrLen - MdiHdrOffDstSAP);
        nbp->offset      = 0;
        mdi->headerIndex = 0;
        mdi->header[MdiHdrOffBlockLen]     = numBytes >> 8;
        mdi->header[MdiHdrOffBlockLen + 1] = numBytes & 0xff;
        mdi->wordState             = MdiIoStateEvenWord;
        mdi->parcel                = 0;
        activeDevice->recordLength = MdiHdrLen + nbp->numBytes;
        break;

    case FcMdiWriteData:
        if (mdi->downlineData.numBytes > 0)
            {
            /*
            **  Unexpected output request by host.
            */
            activeDevice->recordLength = 0;
            activeDevice->fcode        = 0;

            return (FcDeclined);
            }

        mdi->downlineData.offset           = 0;
        mdi->headerIndex                   = 0;
        mdi->header[MdiHdrOffBlockLen]     = 0;
        mdi->header[MdiHdrOffBlockLen + 1] = 0;
        mdi->wordState             = MdiIoStateEvenWord;
        mdi->parcel                = 0;
        mbp                        = &mdi->downlineData;
        mbp->offset                = mbp->numBytes = 0;
        activeDevice->recordLength = 0;
        break;

    case FcMdiMasterClear:
        mdiReset();
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
        return (FcProcessed);
        }

    activeDevice->fcode = funcCode;

    return (FcAccepted);
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
    int       i;
    MdiBuffer *mbp;
    NpuBuffer *nbp;
    int       shift;
    u8        *u8p;

    switch (activeDevice->fcode)
        {
    default:
        break;

    case FcMdiReqGeneralStatus:
        activeChannel->data = mdiHipReadMdiStatus();
        activeChannel->full = TRUE;
#if DEBUG
        fprintf(npuLog, " %03X", activeChannel->data);
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
            u8p         = (mdiState == StMdiStarting) ? detailedStartingResponse : detailedOperationalResponse;
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
            mdiLogBytes(mdi->parcel);
            }
#endif

        if (activeDevice->recordLength < 1)
            {
            /*
            **  Transmission complete.
            */
            activeChannel->discAfterInput = TRUE;
            activeDevice->fcode           = 0;
            mdi->uplineData = NULL;
            npuBipNotifyUplineSent();
            }

        break;

    case FcMdiReadData:
        nbp = mdi->uplineData;
        if (activeChannel->full || (nbp == NULL) || (activeDevice->recordLength < 1))
            {
            break;
            }

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
                else if (nbp->offset < nbp->numBytes)
                    {
                    mdi->parcel |= nbp->data[nbp->offset++];
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
            mdiLogBytes(mdi->parcel);
            }
#endif

        if (activeDevice->recordLength < 1)
            {
            /*
            **  Transmission complete.
            */
#if DEBUG
            mdiLogFlush();
            mdiLogBuffer(mdi->uplineData->data);
            fprintf(npuLog, "    PDU size=%d\n", mdi->uplineData->numBytes);
#endif
            activeChannel->discAfterInput = TRUE;
            activeDevice->fcode           = 0;
            mdi->uplineData = NULL;
            npuBipNotifyUplineSent();
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

                mbp = &mdi->downlineData;
                for (i = 0, shift = 16; i < 3; shift -= 8, i++)
                    {
                    if (mdi->headerIndex < MdiHdrLen)
                        {
                        mdi->headerIndex           += 1;
                        activeDevice->recordLength += 1;
                        }
                    else if (mbp->numBytes < MdiMaxBuffer)
                        {
                        mbp->data[mbp->numBytes++]  = (mdi->parcel >> shift) & 0xff;
                        activeDevice->recordLength += 1;
                        }
                    }
                }
#if DEBUG
            mdiLogPpWord(activeChannel->data);
            if (mdi->wordState == MdiIoStateEvenWord)
                {
                mdiLogBytes(mdi->parcel);
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
    u8        blockType;
    u8        byte;
    int       i;
    MdiBuffer *mbp;
    u8        prio;
    int       shift;

    // on output, marks end of block
    if (activeDevice->fcode == FcMdiWriteData)
        {
        mbp = &mdi->downlineData;
        if (mdi->wordState == MdiIoStateOddWord)
            {
            for (i = 0, shift = 16; i < 2; shift -= 8, i++)
                {
                if (mdi->headerIndex < MdiHdrLen)
                    {
                    mdi->headerIndex += 1;
                    }
                else if (mbp->numBytes < MdiMaxBuffer)
                    {
                    mbp->data[mbp->numBytes++]  = (mdi->parcel >> shift) & 0xff;
                    activeDevice->recordLength += 1;
                    }
                }
#if DEBUG
            mdiLogBytes(mdi->parcel);
#endif
            }
#if DEBUG
        mdiLogFlush();
        mdiLogBuffer(mbp->data);
#endif
        if (mbp->numBytes >= 2)
            {
            /*
            ** The last two bytes transmitted by PIP provide the true message length including
            ** the 19-byte MDI header.
            */
            mbp->numBytes = ((mbp->data[mbp->numBytes - 2] << 8) | mbp->data[mbp->numBytes - 1]) - MdiHdrLen;
#if DEBUG
            fprintf(npuLog, "    PDU size=%d\n", mbp->numBytes);
#endif
            }

        activeDevice->fcode = 0;
        byte            = mbp->data[BlkOffBTBSN];
        blockType       = byte & BlkMaskBT;
        prio            = (byte >> BlkShiftPRIO) & BlkMaskPRIO;
        mbp->blockSeqNo = (byte >> BlkShiftBSN) & BlkMaskBSN;

        if ((blockType == BtHTCMD) && (mbp->data[BlkOffCN] == 0))
            {
            if (mbp->data[BlkOffPfc] == 0x01) // Link regulation
                {
                npuSvmNotifyHostRegulation(3 | 0x04);
                }
            else
                {
                npuBipNotifyServiceMessage();
                npuBipNotifyDownlineReceived();
                }
            }
        else
            {
            npuBipNotifyData(prio);
            npuBipNotifyDownlineReceived();
            }

        mbp->offset = mbp->numBytes = 0;
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
    int       bits;
    NpuBuffer *bp;
    PpWord    mdiStatus;
    int       prus;
    int       words;

    if (mdiState == StMdiStarting)
        {
        return (MdiStateStarting);
        }

    mdiStatus = MdiStatusOperational;

    if (mdi->downlineData.numBytes < 1)
        {
        mdiStatus |= MdiStatusAcceptingData;
        }

    if (mdi->uplineData != NULL)
        {
        mdiStatus |= MdiStatusDataAvailable;
        bp         = mdi->uplineData;

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
    else if (mdiStatus == MdiStatusOperational)
        {
        mdiStatus |= MdiStatusBusy;
        }

    return (mdiStatus);
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
static char *mdiHipFunc2String(PpWord funcCode)
    {
    static char buf[30];

#if DEBUG
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

    case FcMdiReqProtoVersion:
        return "FcMdiReqProtoVersion";
        }
#endif
    if ((funcCode >= FcMdiReqProtoVersion) && (funcCode <= FcMdiReqProtoVersion + 0177))
        {
        return "FcMdiReqProtoVersion";
        }
    else
        {
        sprintf(buf, "(mdi     ) UNKNOWN: %04o", funcCode);

        return (buf);
        }
    }

#if DEBUG

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

    default:
        sprintf(buf, "<%02X>", pfc);

        return (buf);
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

        return (buf);
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
        fputs(mdiLogBuf, npuLog);
        }

    mdiLogWordCol  = 0;
    mdiLogBytesCol = 0;
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

    fprintf(npuLog, "\n    DN=%02X SN=%02X CN=%02X Pri=%d BSN=%d BT=",
            dp[BlkOffDN], dp[BlkOffSN], dp[BlkOffCN],
            (byte >> BlkShiftPRIO) & BlkMaskPRIO,
            (byte >> BlkShiftBSN) & BlkMaskBSN);

    switch (blockType)
        {
    case BtHTBLK:
        fputs("Block\n", npuLog);
        break;

    case BtHTMSG:
        fputs("Message\n", npuLog);
        break;

    case BtHTBACK:
        fputs("Back\n", npuLog);
        break;

    case BtHTCMD:
        fputs("Command\n", npuLog);
        fprintf(npuLog, "    PFC=%s\n    SFC=", mdiPfc2String(dp[BlkOffPfc]));
        sfc = dp[BlkOffSfc];
        if ((sfc & SfcResp) != 0)
            {
            fprintf(npuLog, "Normal Response, %s\n", mdiSfc2String(sfc));
            }
        else if ((sfc & SfcErr) != 0)
            {
            fprintf(npuLog, "Abnormal Response, %s\n", mdiSfc2String(sfc));
            }
        else
            {
            fprintf(npuLog, "Request, %s\n", mdiSfc2String(sfc));
            }
        break;

    case BtHTBREAK:
        fputs("Break\n", npuLog);
        break;

    case BtHTQBLK:
        fputs("Qualified Block\n", npuLog);
        break;

    case BtHTQMSG:
        fputs("Qualified Message\n", npuLog);
        break;

    case BtHTRESET:
        fputs("Reset\n", npuLog);
        break;

    case BtHTRINIT:
        fputs("Initialize Request\n", npuLog);
        break;

    case BtHTNINIT:
        fputs("Initialize Response\n", npuLog);
        break;

    case BtHTTERM:
        fputs("Terminate\n", npuLog);
        break;

    case BtHTICMD:
        fputs("Interrupt Command\n", npuLog);
        break;

    case BtHTICMR:
        fputs("Interrupt Command Response\n", npuLog);
        break;

    default:
        fprintf(npuLog, "<%02X>\n", blockType);
        break;
        }

    fflush(npuLog);
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
static void mdiLogPpWord(int word)
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
static void mdiLogBytes(int parcel)
    {
    int b;
    int col;
    int i;
    int shift;

    col = AsciiColumn(mdiLogBytesCol);

    for (i = 0, shift = 16; i < 3; shift -= 8, i++)
        {
        b = (parcel >> shift) & 0x7f;
        if ((b < 0x20) || (b >= 0x7f))
            {
            b = '.';
            }

        mdiLogBuf[col++] = b;
        }
    mdiLogBytesCol += 3;
    if (mdiLogBytesCol >= 24)
        {
        mdiLogFlush();
        }
    }

#endif

/*--------------------------------------------------------------------------
**  Purpose:        Log a stack trace
**
**  Parameters:     none
**
**  Returns:        nothing
**
**------------------------------------------------------------------------*/
static void mdiPrintStackTrace(FILE *fp)
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

/*---------------------------  End Of File  ------------------------------*/
