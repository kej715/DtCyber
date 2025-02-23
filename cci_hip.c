/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2023, Tom Hunter, Paul Koning, Joachim Siebold
**
**  Name: cci_hip.c
**
**  Description:
**      Perform emulation of the Host Interface Protocol in an NPU
**      consisting of a CDC 2550 HCP running CCI.
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

#if defined(__FreeBSD__)
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#include "const.h"
#include "types.h"
#include "proto.h"
#include "npu.h"
#include "cci.h"

/*
**  -----------------
**  Private Constants
**  -----------------
*/

/*
**  Function codes.
*/
#define FcNpuInMemAddr0            00000 // from INTERCOM 688 sources
#define FcNpuInMemAddr1            00001 // from INTERCOM 688 sources
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
#define FcNpuClearNpu              00200
#define FcNpuClearCoupler          00400

#define FcNpuEqMask                07000

#define FcNpuNothing               07777

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
#define StNpuReadyOutput           00004
#define StNpuNotReadyOutput        00007
#define StNpuReadyForDump          00010
#define StNpuInputAvailPru         00014
#define StNpuInputAvailLe256       00015
#define StNpuInputAvailGt256       00016
#define StNpuDumpOk                00010

/*
**  NPU order word codes (written by PP which results in
**  StCplrOrderLoaded being set). The LSB contains the
**  block length or the new regulation level.
**  We do not care about StCplrOrderLoaded because we read the Order word immediately.
*/
#define OrdOutServiceMsg           0x1
#define OrdOutPriorHigh            0x2
#define OrdOutPriorLow             0x3
#define OrdNotReadyForInput        0x5

/*
**  Below are the sum of the first 16 bytes of CCI images, which we need to determine
**  which program was loaded if a StartNpu command was issued.
**  Note: the macro images 0D to 0DF are copies of either the 0D1 or 0DB images which were found on
**  the NOS/BE TUB deadstart tape.
*/
#define Fp0D0                      0xAC79  // fingerprint of the micro image
#define Fp0DZ                      0x4A2B  // fingerprint of the dump image
#define Fp0DB                      0x8610  // fingerprint of the 0DB macro image
#define Fp0D1                      0xEC98  // fingerprint of the 0D1 macro image

/*
**  Misc constants.
*/
#define CyclesOneSecond            100000

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
typedef struct cciParam
    {
    PpWord    regCouplerStatus;
    PpWord    regNpuStatus;
    PpWord    regOrder;
    NpuBuffer *buffer;
    u8        *cciData;
    u8        halfWordTransferred;
    u8        tempMemAddr0;
    u16       memoryAddress;
    u16       memory[0xFFFF];
    u16       tempWord;
    u32       lastCommandTime;
    u8        runState;
    } CciParam;

typedef enum
    {
    StHipIdle,
    StHipUpline,
    StHipDownline,
    } CciHipState;

typedef enum
    {
    StHcpNotInitialized,
    StHcpRunning,
    StHcpReset,
    } CciHcpState;

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static void cciReset(void);
static FcStatus cciHipFunc(PpWord funcCode);
static void cciHipIo(void);
static void cciHipActivate(void);
static void cciHipDisconnect(void);
static void cciHipWriteNpuStatus(PpWord status);
static PpWord cciHipReadNpuStatus(void);
static bool cciHipDownlineBlockImpl(NpuBuffer *bp);
static bool cciHipUplineBlockImpl(NpuBuffer *bp);

#if (DEBUG > 1)
static char *cciHipFunc2String(PpWord funcCode);

#endif

/*
**  ----------------
**  Public Variables
**  ----------------
*/
bool (*cciHipDownlineBlockFunc)(NpuBuffer *bp);
void (*cciHipResetFunc)(void);
bool (*cciHipUplineBlockFunc)(NpuBuffer *bp);

FILE *cciLog = NULL;


/*
**  -----------------
**  Private Variables
**  -----------------
*/
static CciParam *cci;

static CciHipState cciHipState = StHipIdle;

static CciHcpState cciHcpState = StHcpNotInitialized;

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
void cciInit(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName)
    {
    DevSlot *dp;

    (void)unitNo;
    (void)deviceName;

    /*
    ** set HCP software type, exit if npuSw is not SwUndefined
    */
    if (npuSw != SwUndefined)
        {
        logDtError(LogErrorLocation, "CCI and CCP devices are mutually exclusive\n");
        exit(1);
        }
    npuSw = SwCCI;

    /*
    ** adjust coupler and npu node address
    */

    if (npuSvmCouplerNode != 0)
        {
        npuSvmCouplerNode = 0;
        logDtError(LogErrorLocation, "set coupler node to 0\n");
        }

    if (npuSvmNpuNode != 2)
        {
        npuSvmNpuNode = 2;
        logDtError(LogErrorLocation, "set npu node to 2\n");
        }

    /*
    **  Attach device to channel and initialise device control block.
    */
    dp               = channelAttach(channelNo, eqNo, DtNpu);
    dp->activate     = cciHipActivate;
    dp->disconnect   = cciHipDisconnect;
    dp->func         = cciHipFunc;
    dp->io           = cciHipIo;
    dp->selectedUnit = unitNo;
    activeDevice     = dp;


    /*
    **  Allocate and initialise NPU parameters.
    */
    cci = calloc(1, sizeof(CciParam));
    if (cci == NULL)
        {
        logDtError(LogErrorLocation, "Failed to allocate cci context block\n");
        exit(1);
        }

    dp->controllerContext    = cci;
    cci->regCouplerStatus    = 0;
    cciHipState              = StHipIdle;
    cciHipDownlineBlockFunc  = cciHipDownlineBlockImpl;
    cciHipResetFunc          = cciReset;
    cciHipUplineBlockFunc    = cciHipUplineBlockImpl;
    cci->halfWordTransferred = FALSE;
    memset(cci->memory, 0, sizeof(cci->memory));

    npuBipInit();
    cciSvmInit();
    cciTipInit();

#if (DEBUG > 0)
    if (cciLog == NULL)
        {
        cciLog = fopen("ccilog.txt", "wt");
        }
#endif

    /*
    **  Print a friendly message.
    */
    printf("(cci_hip) NPU initialised on channel %o equipment %o\n", channelNo, eqNo);
    printf("            Coupler node: %u\n", npuSvmCouplerNode);
    printf("                NPU node: %u\n", npuSvmNpuNode);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Terminate CCI, close log file
**
**  Parameters:     Name        Description.
**
**  Returns:        nothing
**
**------------------------------------------------------------------------*/
void cciHipTerminate(DevSlot *dp)
    {
#if (DEBUG > 0)
    if (cciLog != NULL)
        {
        fclose(cciLog);
        }
#endif
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
bool cciHipUplineBlock(NpuBuffer *bp)
    {
    return (*cciHipUplineBlockFunc)(bp);
    }

bool cciHipUplineBlockImpl(NpuBuffer *bp)
    {
    int bits;
    int prus;
    int words;

    if (cciHipState != StHipIdle)
        {
        return (FALSE);
        }

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
        cciHipWriteNpuStatus(StNpuInputAvailPru | (prus << 10));
        }
    else if (bp->numBytes <= 256)
        {
        cciHipWriteNpuStatus(StNpuInputAvailLe256);
        }
    else
        {
        cciHipWriteNpuStatus(StNpuInputAvailGt256);
        }

    cci->buffer = bp;
    cciHipState = StHipUpline;

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
bool cciHipDownlineBlock(NpuBuffer *bp)
    {
    return (*cciHipDownlineBlockFunc)(bp);
    }

bool cciHipDownlineBlockImpl(NpuBuffer *bp)
    {
    if (cciHipState != StHipIdle)
        {
        return (FALSE);
        }

    if (bp == NULL)
        {
        cciHipWriteNpuStatus(StNpuNotReadyOutput);

        return (FALSE);
        }

    cciHipWriteNpuStatus(StNpuReadyOutput);
    cci->buffer = bp;
    cciHipState = StHipDownline;

    return (TRUE);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Tell, if the macro program is running
**
**  Parameters:     none
**
**  Returns:        TRUE if running, FALSE otherwise.
**
**------------------------------------------------------------------------*/
bool cciHipIsReady(void)
    {
    return (cciHcpState == StHcpRunning);
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
static void cciReset(void)
    {
    if (cciHcpState == StHcpRunning)
        {
        /*
        **  Reset all subsystems - order matters!
        */
        npuNetReset();
        cciTipReset();
        cciSvmReset();
        npuBipReset();
        cciHcpState = StHcpReset;
        }

    /*
    **  Reset HIP state.
    */
    cci->regCouplerStatus    = 0;     // All flags cleared by Master Clear.
    cci->halfWordTransferred = FALSE;
    cciHipState = StHipIdle;
#if (DEBUG > 0)
    fprintf(cciLog, "(cci_hip) NPU reset\n");
    logDtError(LogErrorLocation, "NPU reset\n");
#endif
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
static FcStatus cciHipFunc(PpWord funcCode)
    {
    NpuBuffer   *bp;
    CciHcpState oldHcpState;

    funcCode &= ~FcNpuEqMask;
    u16 sum;

#if (DEBUG > 1)
    if (funcCode != FcNpuInCouplerStatus)
        {
        fprintf(cciLog, "\n(cci_hip) %06d PP:%02o CH:%02o f:%04o T:%-25s  >   ",
                traceSequenceNo,
                activePpu->id,
                activeChannel->id,
                funcCode,
                cciHipFunc2String(funcCode));
        }
#endif

    switch (funcCode)
        {
    default:
#if (DEBUG > 0)
        fprintf(cciLog, "(cci_hip) FUNC not implemented & declined!");
#endif

        return (FcDeclined);

    case FcNpuInCouplerStatus:

        switch (cciHipState)
            {
        case StHipIdle:
            /*
            **  Poll network status.
            */
            npuNetCheckStatus();

            /*
            **  If no upline data pending.
            */
            if ((cciHipState == StHipIdle) && (cciHcpState == StHcpRunning))
                {
                /*
                **  Announce idle state to PIP at intervals of less then one second,
                **  otherwise PIP will assume that the NPU is dead.
                */
                if (cycles - cci->lastCommandTime > CyclesOneSecond)
                    {
                    cciHipWriteNpuStatus(StNpuIdle);
                    }
                }
            break;

        default:
            break;
            }

        break;

    case FcNpuInData:
        bp = cci->buffer;
        if (bp == NULL)
            {
            /*
            **  Unexpected input request by host.
            */
            cciHipState  = StHipIdle;
            cci->cciData = NULL;
            activeDevice->recordLength = 0;
            activeDevice->fcode        = FcNpuNothing;
#if (DEBUG > 0)
            fprintf(cciLog, "(cci-hip) unexpected input reqeust by host\n");
#endif

            return (FcDeclined);
            }

        cci->cciData = bp->data;
        activeDevice->recordLength = bp->numBytes;
        break;

    case FcNpuOutData:
        bp = cci->buffer;
        if (bp == NULL)
            {
            /*
            **  Unexpected output request by host.
            */
            cciHipState  = StHipIdle;
            cci->cciData = NULL;
            activeDevice->recordLength = 0;
            activeDevice->fcode        = FcNpuNothing;
#if (DEBUG > 0)
            fprintf(cciLog, "(cci_hip) unexpected input reqeust by host\n");
#endif

            return (FcDeclined);
            }

        cci->cciData = bp->data;
        activeDevice->recordLength = 0;
        break;

    case FcNpuInNpuStatus:
    case FcNpuInNpuOrder:
    case FcNpuInMemAddr0:
    case FcNpuInMemAddr1:
        break;

    case FcNpuOutNpuOrder:
        cciHipState = StHipIdle;
        cciHipWriteNpuStatus(StNpuIdle);
        break;

    case FcNpuClearNpu:
        cciReset();
        break;

    case FcNpuOutMemAddr0:
    case FcNpuOutMemAddr1:
        cciHipState = StHipIdle;
        break;

    case FcNpuOutProgram:
        cciHipState = StHipIdle;
        break;

    case FcNpuInProgram:
        break;

    case FcNpuClearCoupler:
        cci->regCouplerStatus &= StCplrStatusLoaded;  // clear all status bits except bit 2
        break;

    case FcNpuStartNpu:
        /*
        ** we must know which program has been loaded into NPU memory to simulate the proper response
        ** therefore we check the fingerprint (sum of memory[1..16] of the downloaded image
        */
        sum = 0;
        for (int i = 0; i < 16; i++)
            {
            sum += cci->memory[i];
            }

        switch (sum)
            {
        /*
        ** micro program started, respond with NPU idle state
        */
        case Fp0D0:      // fingerprint of the micro image
#if (DEBUG > 0)
            fprintf(cciLog, "(cci_hip) NPU start micro program\n");
            logDtError(LogErrorLocation, "NPU start micro program\n");
#endif
            cciHipState = StHipIdle;
            oldHcpState = cciHcpState;
            cciHcpState = StHcpRunning;
            cciHipWriteNpuStatus(StNpuIdle);
            cciHcpState = oldHcpState;
            break;

        /*
        ** dump program started, respond with dump ok
        */
        case Fp0DZ:     // fingerprint of the dump program
#if (DEBUG > 0)
            fprintf(cciLog, "(cci_hip) NPU start dump program\n");
            logDtError(LogErrorLocation, "NPU start dump program\n");
#endif
            cciHipState        = StHipIdle;
            cci->memory[0x1FF] = 1024;
            cciHipWriteNpuStatus(StNpuDumpOk);
            break;

        /*
        ** macro program loaded, respond with INIT service message
        */
        case Fp0DB:     // fingerprint of the 0DB image
        case Fp0D1:     // fingerprint of the 0D1 image

            switch (cciHcpState)
                {
            case StHcpNotInitialized:

                /*
                **  Initialise BIP, SVC and TIP because the CCI software is started for the first time
                */
#if (DEBUG > 0)
                fprintf(cciLog, "(cci_hip) NPU start macro program\n");
                logDtError(LogErrorLocation, "NPU start macro program\n");
#endif
                cciHipState = StHipIdle;
                cciHcpState = StHcpRunning;
                cciSvmNpuInitResponse();
                break;

            case StHcpReset:

                /*
                ** This is a restart: reset already done in cciHipReset
                */
#if (DEBUG > 0)
                fprintf(cciLog, "(cci_hip) NPU restart macro program\n");
                logDtError(LogErrorLocation, "NPU restart macro program\n");
#endif
                cciHipState = StHipIdle;
                cciHcpState = StHcpRunning;
                cciSvmNpuInitResponse();
                break;

            case StHcpRunning:

                /*
                ** Error: start NPU while macro program is running
                */
                logDtError(LogErrorLocation, "Fatal: StartNpu called while macro program is running\n");
                break;

            default:

                /*
                ** Error: unknown image loaded
                */
                logDtError(LogErrorLocation, "Fatal: StartNpu called for unknown image\n");
                break;
                }
            break;
            }

        return (FcProcessed);
        }                  // case funCode

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
static void cciHipIo(void)
    {
    PpWord orderLength;
    u8     orderCode;

    switch (activeDevice->fcode)
        {
    default:
        break;

    case FcNpuInNpuStatus:
        activeChannel->data = cciHipReadNpuStatus();
        activeChannel->full = TRUE;
#if (DEBUG > 1)
        fprintf(cciLog, " %03X", activeChannel->data);
#endif
        cci->regCouplerStatus &= 0xFFFB;  // clear NPU status word loaded bit
        break;

    case FcNpuInCouplerStatus:
        activeChannel->data    = cci->regCouplerStatus;
        activeChannel->full    = TRUE;
        cci->regCouplerStatus |= StCplrNpuStatusRead;
#if (DEBUG > 1)
        if (cci->regCouplerStatus != 0)
            {
            fprintf(cciLog, "\n(cci_hip) %06d PP:%02o CH:%02o f:%04o T:%-25s  >    %03X",
                    traceSequenceNo,
                    activePpu->id,
                    activeChannel->id,
                    FcNpuInCouplerStatus,
                    cciHipFunc2String(FcNpuInCouplerStatus),
                    activeChannel->data);
            }
#endif
        break;

    case FcNpuInNpuOrder:
        activeChannel->data = cci->regOrder;
        activeChannel->full = TRUE;
#if (DEBUG > 1)
        fprintf(cciLog, " %03X", activeChannel->data);
#endif
        break;

    case FcNpuInMemAddr0:
        activeChannel->data = cci->memoryAddress >> 8;
        activeChannel->full = TRUE;
#if (DEBUG > 1)
        fprintf(cciLog, " %05X", activeChannel->data);
#endif
        break;

    case FcNpuInMemAddr1:
        activeChannel->data    = cci->memoryAddress & 0xFF;
        activeChannel->full    = TRUE;
        cci->regCouplerStatus |= StCplrAddrLoaded;
#if (DEBUG > 1)
        fprintf(cciLog, " %05X", activeChannel->data);
#endif
        break;


    case FcNpuInData:
        if (activeChannel->full)
            {
            break;
            }

        if (activeDevice->recordLength > 0)
            {
            activeChannel->data = *cci->cciData++;
            activeChannel->full = TRUE;

            activeDevice->recordLength -= 1;
            if (activeDevice->recordLength == 0)
                {
                /*
                **  Transmission complete.
                */
                activeChannel->data          |= 04000;
                activeChannel->discAfterInput = TRUE;
                activeDevice->fcode           = FcNpuNothing;
                cciHipState = StHipIdle;
#if (DEBUG > 0)
                fprintf(cciLog, "(cci-hip) in: ");
                for (int i = 0; i < cci->buffer->numBytes; i++)
                    {
                    fprintf(cciLog, "%x ", cci->buffer->data[i]);
                    }
                fprintf(cciLog, "\n");
#endif
                npuBipNotifyUplineSent();
                }
            }

        break;

    case FcNpuOutData:
        if (activeChannel->full)
            {
            activeChannel->full = FALSE;
            if (activeDevice->recordLength < MaxBuffer)
                {
                *cci->cciData++             = activeChannel->data & Mask8;
                activeDevice->recordLength += 1;
                if ((activeChannel->data & 04000) != 0)
                    {
                    /*
                    **  Top bit set - process message.
                    */
                    cci->buffer->numBytes = activeDevice->recordLength;
                    activeDevice->fcode   = FcNpuNothing;
                    cciHipState           = StHipIdle;
#if (DEBUG > 0)
                    fprintf(cciLog, "(cci-hip) out: ");
                    for (int i = 0; i < cci->buffer->numBytes; i++)
                        {
                        fprintf(cciLog, "%x ", cci->buffer->data[i]);
                        }
                    fprintf(cciLog, "\n");
#endif

                    npuBipNotifyDownlineReceived();
                    }
                else if (activeDevice->recordLength >= MaxBuffer)
                    {
                    /*
                    **  We run out of buffer space before the end of the message.
                    */
                    activeDevice->fcode = FcNpuNothing;
#if (DEBUG > 0)
                    fprintf(cciLog, "(cci-hip) run out of buffer space before end of the message\n");
#endif
                    cciHipState = StHipIdle;
                    npuBipAbortDownlineReceived();
                    }
                }
            }

        break;

    case FcNpuOutNpuOrder:
        if (activeChannel->full)
            {
            cci->regOrder       = activeChannel->data;
            activeChannel->full = FALSE;
            orderCode           = cci->regOrder >> 9;
            orderLength         = cci->regOrder & 0x1FF;
#if (DEBUG > 0)
            fprintf(cciLog, "(cci-hip) Order Word Code %x Length %x\n", orderCode, orderLength);
#endif

            switch (orderCode)
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
                }
            }

        break;

    case FcNpuInProgram:
        if (cci->halfWordTransferred)
            {
            cci->halfWordTransferred = FALSE;
            activeChannel->data      = (PpWord)cci->memory[cci->memoryAddress] & 0xFF;
            cci->memoryAddress++;
            }
        else
            {
            cci->halfWordTransferred = TRUE;
            activeChannel->data      = (PpWord)cci->memory[cci->memoryAddress] >> 8;
            }
        activeChannel->full    = TRUE;
        cci->regCouplerStatus |= StCplrTransferCompleted;

        break;

    case FcNpuOutMemAddr0:
        if (activeChannel->full)
            {
            cci->tempMemAddr0   = (u8)activeChannel->data;
            activeChannel->full = FALSE;
            }
        break;

    case FcNpuOutMemAddr1:
        if (activeChannel->full)
            {
            cci->memoryAddress  = (cci->tempMemAddr0 << 8) | activeChannel->data;
            activeChannel->full = FALSE;
#if (DEBUG > 1)
            fprintf(cciLog, "  %05X", cci->memoryAddress);
#endif
            }
        cci->regCouplerStatus |= StCplrAddrLoaded;
        break;

    case FcNpuOutProgram:
        if (activeChannel->full)
            {
            if (cci->halfWordTransferred)
                {
#if (DEBUG > 1)
                fprintf(cciLog, "writing mem address %x\n", cci->memoryAddress);
#endif
                cci->memory[cci->memoryAddress] = (cci->tempWord << 8) |
                                                  activeChannel->data;

                cci->halfWordTransferred = FALSE;
                cci->memoryAddress++;
                cci->regCouplerStatus |= StCplrTransferCompleted;
                }
            else
                {
                cci->halfWordTransferred = TRUE;
                cci->tempWord            = activeChannel->data;
                }
            activeChannel->full = FALSE;
            }

        break;

    case FcNpuStartNpu:
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
static void cciHipActivate(void)
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
static void cciHipDisconnect(void)
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
static void cciHipWriteNpuStatus(PpWord status)
    {
    cci->lastCommandTime = cycles;
    if (cciHcpState != StHcpRunning)
        {
#if DEBUG > 0
        fprintf(cciLog, "writing NPU status while software not running\n");
#endif
        }
    cci->regNpuStatus      = status;
    cci->regCouplerStatus |= StCplrStatusLoaded;
    }

/*--------------------------------------------------------------------------
**  Purpose:        PP reads NPU status register.
**
**  Parameters:     Name        Description.
**
**  Returns:        NPU status register value.
**
**------------------------------------------------------------------------*/
static PpWord cciHipReadNpuStatus(void)
    {
    PpWord value = cci->regNpuStatus;

    cci->regCouplerStatus &= ~StCplrStatusLoaded;
    cci->regNpuStatus      = StNpuIgnore;

    return (value);
    }

#if (DEBUG > 1)

/*--------------------------------------------------------------------------
**  Purpose:        Convert function code to string.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        String equivalent of function code.
**
**------------------------------------------------------------------------*/
static char *cciHipFunc2String(PpWord funcCode)
    {
    static char buf[40];

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

    case FcNpuClearNpu:
        return "FcNpuClearNpu";

    case FcNpuClearCoupler:
        return "FcNpuClearCoupler";

    case FcNpuInMemAddr0:
        return "FcNpuInMemAddr0";

    case FcNpuInMemAddr1:
        return "FcNpuInMemAddr1";
        }
    sprintf(buf, "(cci_hip) Unknown Function: %04o", funcCode);

    return (buf);
    }

#endif

/*---------------------------  End Of File  ------------------------------*/
