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
#if defined(_WIN32)
#include <winsock.h>
#else
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
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
#define StNpuDumpOk                00010
#define StNpuInputAvailPru         00014
#define StNpuInputAvailLe256       00015
#define StNpuInputAvailGt256       00016

/*
**  NPU order word codes (written by PP which results in StCplrOrderLoaded being set).
**  The LSB contains the block length or the new regulation level. We do not care about
**  StCplrOrderLoaded because we read the Order word immediately.
*/
#define OrdOutServiceMsg           0x1
#define OrdOutPriorHigh            0x2
#define OrdOutPriorLow             0x3
#define OrdNotReadyForInput        0x5

#define OrdShiftType               9

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
**  -----------------------
**  Private Macro Functions
**  -----------------------
*/

/*
**  -----------------------------------------
**  Private Typedef and Structure Definitions
**  -----------------------------------------
*/
typedef enum
    {
    StHipIdle = 0,
    StHipUpline,
    StHipDownline
    } CciHipState;

typedef enum
    {
    StHcpNotInitialized = 0,
    StHcpRunning,
    StHcpReset
    } CciHcpState;

typedef struct cciParam
    {
    u8          id;
    CciHipState hipState;
    CciHcpState hcpState;
    PpWord      regCouplerStatus;
    PpWord      regNpuStatus;
    PpWord      regOrder;
    u8          halfWordTransferred;
    u8          tempMemAddr0;
    u16         memoryAddress;
    u16         memory[64*1024];
    u16         tempWord;
    u32         lastCommandTime;
    u8          hipRequest;
#define           HipReqNone           0
#define           HipReqInitResponse   1
#define           HipReqNotifyUpline   2
    } CciParam;

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static void cciHipCreateThread(NetDevParam *netDevice);
static FcStatus cciHipFunc(PpWord funcCode);
static void cciHipIo(void);
static void cciHipActivate(void);
static void cciHipDisconnect(void);
static void cciHipIndicateUplineBlock(u8 *block, u16 numBytes);
static PpWord cciHipReadNpuStatus(CciParam *cci);
static void cciHipSetNpuStatus(CciParam *cci, PpWord status);

#if defined(_WIN32)
static void cciThread(void *param);
#else
static void *cciThread(void *param);
#endif

#if DEBUG
static char *cciHcpState2String(CciHcpState state);
static char *cciHipState2String(CciHipState state);
static char *cciHipFunc2String(PpWord funcCode);
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
static u8 cciCount = 0;;

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
    CciParam    *cci;
    DevSlot     *dp;
    NetDevParam *netDevice;

    if (netDevCount >= MaxNetDevices)
        {
        logDtError(LogErrorLocation, "Maximum number of network device definitions (%d) exceeded\n", MaxNetDevices);
        exit(1);
        }

    /*
    **  Attach device to channel and initialise device control block.
    */
    dp               = channelAttach(channelNo, eqNo, DtHcp);
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

    cci->id       = cciCount++;
    cci->hipState = StHipIdle;

    /*
    **  Allocate and initialise general network device parameters.
    */
    netDevice = &netDevices[netDevCount++];
    memset(netDevice, 0, sizeof(NetDevParam));
    netDevice->id        = cci->id;
    netDevice->type      = NPU_CCI;
    netDevice->channelNo = channelNo;
    netDevice->eqNo      = eqNo;
    netDevice->unitNo    = unitNo;
    sprintf(netDevice->name, "hcp%d", cci->id);
    if (deviceName != NULL && strlen(deviceName) > 0)
        {
        netDevice->sectionName = (char *)malloc(strlen(deviceName) + 1);
        if (netDevice->sectionName == NULL)
            {
            logDtError(LogErrorLocation, "Failed to allocate cci context block\n");
            exit(1);
            }
        strcpy(netDevice->sectionName, deviceName);
        }
    netDevice->devDependent = cci;
    dp->controllerContext   = netDevice;

#if DEBUG
    {
    char fName[16];
    sprintf(fName, "%slog.txt", netDevice->name);
    netDevice->log = fopen(fName, "wt");
    if (netDevice->log == NULL)
        {
        perror(fName);
        exit(1);
        }
    fprintf(netDevice->log, "HCP%d initialised on channel %o equipment %o\n", cci->id, channelNo, eqNo);
    }
#endif
    /*
    **  Print a friendly message.
    */
    printf("(cci_hip) HCP%d initialised on channel %o equipment %o\n", cci->id, channelNo, eqNo);

    /*
    **  Create NPU emulation thread
    */
    cciHipCreateThread(netDevice);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Process completion of downline block transfer.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
void cciHipNotifyDownlineCopied(void)
    {
    CciParam *cci;

    cci = (CciParam *)netDevice->devDependent;
    cciHipSetNpuStatus(cci, StNpuReadyOutput);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Initiate trasnfer of downline block.
**
**  Parameters:     Name        Description.
**                  bp          pointer to NPU buffer.
**
**  Returns:        TRUE if block can be accepted, FALSE otherwise.
**
**------------------------------------------------------------------------*/
bool cciHipStartDownlineTransfer(NpuBuffer *bp)
    {
    CciParam *cci;

    cci = (CciParam *)netDevice->devDependent;
    if (netDevice->downlineData.isReady == FALSE || netDevice->downlineData.numBytes < 1 || bp == NULL || cci->hipState != StHipIdle)
        {
#if DEBUG
        if (bp != NULL)
            {
            fprintf(netDevice->log, "\n  Downline block rejected, CN=%02X, BT=%02X, PDU size=%d, HCP state %d, downline ready %s\n", netDevice->downlineData.data[BlkOffCN],
                netDevice->downlineData.data[BlkOffBTBSN] & BlkMaskBT, netDevice->downlineData.numBytes, cci->hipState, netDevice->downlineData.isReady ? "TRUE" : "FALSE");
            traceStack(netDevice->log);
            }
#endif
        return FALSE;
        }

    return TRUE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Initiate transfer of upline block.
**
**  Parameters:     Name        Description.
**                  bp          pointer to NPU buffer.
**
**  Returns:        TRUE if block can be accepted, FALSE otherwise.
**
**------------------------------------------------------------------------*/
bool cciHipStartUplineTransfer(NpuBuffer *bp)
    {
    CciParam *cci;

    cci = (CciParam *)netDevice->devDependent;
    if (netDevice->uplineData.isReady || bp == NULL || cci->hipState != StHipIdle)
        {
#if DEBUG
        if (bp != NULL)
            {
            fprintf(netDevice->log, "\n  Upline block rejected, CN=%02X, BT=%02X, PDU size=%d, HCP state %d, upline ready %s\n", bp->data[BlkOffCN],
                bp->data[BlkOffBTBSN] & BlkMaskBT, bp->numBytes, cci->hipState, netDevice->uplineData.isReady ? "TRUE" : "FALSE");
            traceStack(netDevice->log);
            }
#endif
        return FALSE;
        }

    cciHipIndicateUplineBlock(bp->data, bp->numBytes);

    return TRUE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Indicate whether the macro program is running
**
**  Parameters:     netDevice   pointer to network device parameters
**
**  Returns:        TRUE if running, FALSE otherwise.
**
**------------------------------------------------------------------------*/
bool cciHipIsReady(NetDevParam *netDevice)
    {
    CciParam *cci;

    cci = (CciParam *)netDevice->devDependent;

    return cci->hcpState == StHcpRunning;
    }

/*
 **--------------------------------------------------------------------------
 **
 **  Private Functions
 **
 **--------------------------------------------------------------------------
 */

/*--------------------------------------------------------------------------
**  Purpose:        Create thread which will emulate an NPU.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cciHipCreateThread(NetDevParam *param)
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
        (LPTHREAD_START_ROUTINE)cciThread,
        (LPVOID)param,                              // thread parameter
        0,                                          // not suspended
        &dwThreadId);                               // returns thread ID

    if (hThread == NULL)
        {
        logDtError(LogErrorLocation, "Failed to create HCP thread\n");
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
    rc = pthread_create(&thread, &attr, cciThread, param);
    if (rc < 0)
        {
        logDtError(LogErrorLocation, "Failed to create HCP thread\n");
        exit(1);
        }
#endif
    puts("(cci_hip) HCP thread created");
    }

/*--------------------------------------------------------------------------
**  Purpose:        NPU emulation thread.
**
**  Parameters:     Name        Description.
**                  param       unused
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
#if defined(_WIN32)
static void cciThread(void *param)
#else
static void *cciThread(void *param)
#endif
    {
    CciParam *cci;
    char     *sectionName;
    u8       orderCode;

    /*
    **  Parse data communication configuration
    */
    netDevice   = (NetDevParam *)param;
    cci         = (CciParam *)netDevice->devDependent;
    sectionName = netDevice->sectionName;
    if (sectionName == NULL)
        {
        sectionName = npuConnections;
        if (strlen(sectionName) == 0)
            {
            sectionName = NULL;
            }
        }
    initNpuConnections(sectionName);

    /*
    ** Adjust coupler and npu node address
    */
    if (netDevice->couplerNode != 0)
        {
        netDevice->couplerNode = 0;
        logDtError(LogErrorLocation, "set coupler node to 0\n");
        }

    if (netDevice->npuNode != 2)
        {
        netDevice->npuNode = 2;
        logDtError(LogErrorLocation, "set npu node to 2\n");
        }

    /*
    **  Print a friendly message.
    */
    printf("(cci_hip) HCP%d host ID:      %s\n", cci->id, npuNetHostID);
    printf("(cci_hip) HCP%d coupler node: %u\n", cci->id, netDevice->couplerNode);
    printf("(cci_hip) HCP%d NPU node:     %u\n", cci->id, netDevice->npuNode);

    /*
    **  Initialise BIP, SVC, and TIP.
    */
    npuBipInit();
    cciSvmInit();
    cciTipInit();
    npuNetInit();

    for (;;)
        {
        if (netDevice->doReset)
            {
            /*
            **  Reset all subsystems - order matters
            */
            npuNetReset();
            cciTipReset();
            cciSvmReset();
            npuBipReset();
            netDevice->doReset = FALSE;
            }

        /*
        **  Process downline block, if any
        */
        if (netDevice->downlineData.isReady)
            {
            orderCode = cci->regOrder >> 9;
            switch (orderCode)
                {
            default:
                break;

            case OrdOutServiceMsg:
                npuBipNotifyServiceMessage();
                npuBipNotifyDownlineReceived();
                break;

            case OrdOutPriorHigh:
                npuBipNotifyData(1);
                npuBipNotifyDownlineReceived();
                break;

            case OrdOutPriorLow:
                npuBipNotifyData(0);
                npuBipNotifyDownlineReceived();
                break;
                }
            }

        /*
        ** .Process upline actions, if any
        */
        if (cci->hipRequest != HipReqNone)
            {
            switch (cci->hipRequest)
                {
            case HipReqInitResponse:
#if DEBUG
                fputs("\n  Send initialization response", netDevice->log);
#endif
                cciSvmNpuInitResponse();
                break;

            case HipReqNotifyUpline:
#if DEBUG
                fputs("\n  Notify upline sent", netDevice->log);
#endif
                npuBipNotifyUplineSent();
                netDevice->uplineData.isReady = FALSE;
                break;

            default:
                break;
                }

            cci->hipRequest = HipReqNone;
            }

        /*
        **  Process upline block, if any
        */
        if (netDevice->uplineData.isReady == FALSE)
            {
            npuBipTryUplineBlock();
            }
        else if (cci->hipState == StHipIdle && (cci->regCouplerStatus & StCplrStatusLoaded) == 0 && netDevice->uplineData.offset == 0)
            {
            cciHipIndicateUplineBlock(netDevice->uplineData.data, netDevice->uplineData.numBytes);
            }

        /*
        **  Poll network status.
        */
        npuNetCheckConnections();
        npuNetCheckStatus();
        }

#if !defined(_WIN32)
    return NULL;
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
    ChIoBuffer  *bp;
    CciParam    *cci;
    NetDevParam *netDevice;
    CciHcpState oldHcpState;
    u16         sum;

    funcCode &= ~FcNpuEqMask;
    netDevice = (NetDevParam *)activeDevice->controllerContext;
    cci       = (CciParam *)netDevice->devDependent;

#if DEBUG
    if (funcCode != FcNpuInCouplerStatus)
        {
        fprintf(netDevice->log, "\nPP:%02o CH:%02o f:%04o T:%-25s  >   ",
                activePpu->id,
                activeChannel->id,
                funcCode,
                cciHipFunc2String(funcCode));
        }
#endif

    activeDevice->fcode = funcCode;

    switch (funcCode)
        {
    default:
#if DEBUG
        fputs("FUNC not implemented & declined", netDevice->log);
#endif
        return FcDeclined;

    case FcNpuInCouplerStatus:
        if (cci->hipState == StHipIdle)
            {
            /*
            **  If no upline data pending.
            */
            if (cci->hcpState == StHcpRunning && cci->hipState == StHipIdle)
                {
                /*
                **  Announce idle state to PIP at intervals of less then one second,
                **  otherwise PIP will assume that the NPU is dead.
                */
                if (cycles - cci->lastCommandTime > CyclesOneSecond)
                    {
                    cciHipSetNpuStatus(cci, StNpuIdle);
                    }
                }
            }
        break;

    case FcNpuInData:
        if (netDevice->uplineData.isReady == FALSE)
            {
            /*
            **  Unexpected input request by host.
            */
            cci->hipState              = StHipIdle;
            activeDevice->recordLength = 0;
            activeDevice->fcode        = FcNpuNothing;
#if DEBUG
            fputs("\n  Unexpected input reqeust by host\n", netDevice->log);
#endif

            return FcDeclined;
            }
        bp                         = &netDevice->uplineData;
        bp->offset                 = 0;
        activeDevice->recordLength = bp->numBytes;
        break;

    case FcNpuOutData:
        if (netDevice->downlineData.isReady)
            {
            /*
            **  Unexpected output request by host.
            */
            cci->hipState              = StHipIdle;
            activeDevice->recordLength = 0;
            activeDevice->fcode        = FcNpuNothing;
#if DEBUG
            fputs("\n  Unexpected output reqeust by host\n", netDevice->log);
#endif

            return FcDeclined;
            }
        bp                         = &netDevice->downlineData;
        bp->offset                 = 0;
        bp->numBytes               = 0;
        activeDevice->recordLength = 0;
        cci->hipState              = StHipDownline;
#if DEBUG
        fprintf(netDevice->log, "\n  hipState -> %s", cciHipState2String(cci->hipState));
#endif
        break;

    case FcNpuInNpuStatus:
    case FcNpuInNpuOrder:
    case FcNpuInMemAddr0:
    case FcNpuInMemAddr1:
    case FcNpuInProgram:
        break;

    case FcNpuOutNpuOrder:
        cci->hipState = StHipIdle;
        cciHipSetNpuStatus(cci, StNpuIdle);
#if DEBUG
        fprintf(netDevice->log, "\n  hipState -> %s", cciHipState2String(cci->hipState));
#endif
        break;

    case FcNpuClearNpu:
        /*
        **  Reset HIP state.
        */
        cci->hipState            = StHipIdle;
        cci->regCouplerStatus    = 0;     // All flags cleared by Master Clear.
        cci->halfWordTransferred = FALSE;
        cci->hipRequest          = HipReqNone;
#if DEBUG
        fprintf(netDevice->log, "\n  hipState -> %s", cciHipState2String(cci->hipState));
#endif
        if (cci->hcpState == StHcpRunning)
            {
            cci->hcpState      = StHcpReset;
            netDevice->doReset = TRUE;
#if DEBUG
            fprintf(netDevice->log, "\n  hcpState -> %s", cciHcpState2String(cci->hcpState));
#endif
            }
        break;

    case FcNpuOutMemAddr0:
    case FcNpuOutMemAddr1:
        cci->hipState = StHipIdle;
        break;

    case FcNpuOutProgram:
        cci->hipState = StHipIdle;
#if DEBUG
        fprintf(netDevice->log, "\n  hipState -> %s", cciHipState2String(cci->hipState));
#endif
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
#if DEBUG
            fputs("\n  Start micro program", netDevice->log);
            logDtError(LogErrorLocation, "HCP%d start micro program\n", cci->id);
#endif
            cci->hipState = StHipIdle;
            oldHcpState   = cci->hcpState;
            cci->hcpState = StHcpRunning;
            cciHipSetNpuStatus(cci, StNpuIdle);
            cci->hcpState = oldHcpState;
#if DEBUG
            fprintf(netDevice->log, "\n  hipState -> %s", cciHipState2String(cci->hipState));
#endif
            break;

        /*
        ** dump program started, respond with dump ok
        */
        case Fp0DZ:     // fingerprint of the dump program
#if DEBUG
            fputs("\n  Start dump program", netDevice->log);
            logDtError(LogErrorLocation, "HCP%d start dump program\n", cci->id);
#endif
            cci->hipState      = StHipIdle;
            cci->memory[0x1FF] = 1024;
            oldHcpState   = cci->hcpState;
            cci->hcpState = StHcpRunning;
            cciHipSetNpuStatus(cci, StNpuDumpOk);
            cci->hcpState = oldHcpState;
#if DEBUG
            fprintf(netDevice->log, "\n  hipState -> %s", cciHipState2String(cci->hipState));
#endif
            break;

        /*
        ** macro program loaded, respond with INIT service message
        */
        case Fp0DB:     // fingerprint of the 0DB image
        case Fp0D1:     // fingerprint of the 0D1 image

            switch (cci->hcpState)
                {
            case StHcpNotInitialized:

                /*
                **  Initialise BIP, SVC and TIP because the CCI software is started for the first time
                */
#if DEBUG
                fputs("\n  Start macro program", netDevice->log);
                logDtError(LogErrorLocation, "HCP%d start macro program\n", cci->id);
#endif
                cci->hipState   = StHipIdle;
                cci->hcpState   = StHcpRunning;
                cci->hipRequest = HipReqInitResponse;
#if DEBUG
                fprintf(netDevice->log, "\n  hipState -> %s", cciHipState2String(cci->hipState));
                fprintf(netDevice->log, "\n  hcpState -> %s", cciHcpState2String(cci->hcpState));
#endif
                break;

            case StHcpReset:
                if (netDevice->doReset == FALSE)
                    {
                    /*
                    ** This is a restart: when doReset is FALSE, reset has been completed
                    */
#if DEBUG
                    fputs("\n  Restart macro program", netDevice->log);
                    logDtError(LogErrorLocation, "HCP%d restart macro program\n", cci->id);
#endif
                    cci->hipState   = StHipIdle;
                    cci->hcpState   = StHcpRunning;
                    cci->hipRequest = HipReqInitResponse;
#if DEBUG
                    fprintf(netDevice->log, "\n  hipState -> %s", cciHipState2String(cci->hipState));
                    fprintf(netDevice->log, "\n  hcpState -> %s", cciHcpState2String(cci->hcpState));
#endif
                    }
                break;

            case StHcpRunning:
                /*
                ** Error: start NPU while macro program is running
                */
                logDtError(LogErrorLocation, "HCP%d Fatal: StartNpu called while macro program is running\n", cci->id);
#if DEBUG
                fputs("  StartNpu called while macro program is running", netDevice->log);
#endif
                break;

            default:
                /*
                ** Error: unknown HCP state
                */
                logDtError(LogErrorLocation, "HCP%d Fatal: StartNpu called for macro program in unknown state %d\n", cci->hcpState);
#if DEBUG
                fprintf(netDevice->log, "\n  StartNpu called for macro program in unknown state %d", cci->hcpState);
#endif
                break;
                }
            break;

        default:
            /*
            ** Error: unknown image loaded
            */
            logDtError(LogErrorLocation, "HCP%d Fatal: StartNpu called for unknown image with sum %04X\n", sum);
#if DEBUG
            fprintf(netDevice->log, "\n  StartNpu called for unknown image with sum %04X", sum);
#endif
            break;
            }

        return FcProcessed;
        }

    return FcAccepted;
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
    ChIoBuffer  *bp;
    CciParam    *cci;
    NetDevParam *netDevice;

    netDevice = (NetDevParam *)activeDevice->controllerContext;
    cci       = (CciParam *)netDevice->devDependent;

    switch (activeDevice->fcode)
        {
    default:
        break;

    case FcNpuInNpuStatus:
        activeChannel->data    = cciHipReadNpuStatus(cci);
        activeChannel->full    = TRUE;
#if DEBUG
        fprintf(netDevice->log, " %03X", activeChannel->data);
#endif
        break;

    case FcNpuInCouplerStatus:
        activeChannel->data    = cci->regCouplerStatus;
        activeChannel->full    = TRUE;
        cci->regCouplerStatus |= StCplrNpuStatusRead;
#if DEBUG
        if (cci->regCouplerStatus != 0)
            {
            fprintf(netDevice->log, "\nPP:%02o CH:%02o f:%04o T:%-25s  >    %03X",
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
#if DEBUG
        fprintf(netDevice->log, " %03X", activeChannel->data);
#endif
        break;

    case FcNpuInMemAddr0:
        activeChannel->data = cci->memoryAddress >> 8;
        activeChannel->full = TRUE;
#if (DEBUG > 1)
        fprintf(netDevice->log, " %02X", (u8)activeChannel->data);
#endif
        break;

    case FcNpuInMemAddr1:
        activeChannel->data    = cci->memoryAddress & 0xFF;
        activeChannel->full    = TRUE;
        cci->regCouplerStatus |= StCplrAddrLoaded;
#if (DEBUG > 1)
        fprintf(netDevice->log, " %02X", (u8)activeChannel->data);
#endif
        break;

    case FcNpuInData:
        if (activeChannel->full || (netDevice->uplineData.isReady == FALSE) || (activeDevice->recordLength < 1))
            {
            break;
            }
        if (activeDevice->recordLength > 0)
            {
            bp                          = &netDevice->uplineData;
            activeChannel->data         = bp->data[bp->offset++];
            activeChannel->full         = TRUE;
#if (DEBUG > 1)
            fprintf(netDevice->log, "%03X ", activeChannel->data);
#endif
            activeDevice->recordLength -= 1;
            if (activeDevice->recordLength == 0)
                {
                /*
                **  Transmission complete.
                */
                activeChannel->data          |= 04000;
                activeChannel->discAfterInput = TRUE;
                activeDevice->fcode           = FcNpuNothing;
                cci->hipState                 = StHipIdle;
                cci->hipRequest               = HipReqNotifyUpline;
#if DEBUG
                fprintf(netDevice->log, "\n  hipState -> %s", cciHipState2String(cci->hipState));
#endif
                }
            }
        break;

    case FcNpuOutData:
        if (activeChannel->full)
            {
            activeChannel->full = FALSE;
            bp                  = &netDevice->downlineData;
            if (bp->numBytes < MaxBuffer)
                {
                bp->data[bp->numBytes++] = activeChannel->data & Mask8;
                }
#if (DEBUG > 1)
            fprintf(netDevice->log, "%03X ", activeChannel->data);
#endif
            activeDevice->recordLength += 1;
            if ((activeChannel->data & 04000) != 0)
                {
                /*
                **  Top bit set - process message.
                */
                activeDevice->fcode = FcNpuNothing;
                cci->hipState       = StHipIdle;
                bp->isReady         = TRUE;
#if DEBUG
                fprintf(netDevice->log, "\n  hipState -> %s", cciHipState2String(cci->hipState));
#endif
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

            fprintf(netDevice->log, " Order word %03X - function %d : %s",
                activeChannel->data, activeChannel->data >> OrdShiftType, orderCode[(activeChannel->data >> OrdShiftType) & 7]);
#endif
            cci->regOrder       = activeChannel->data;
            activeChannel->full = FALSE;
            switch ((cci->regOrder >> OrdShiftType) & Mask3)
                {
            default:
                break;

            case OrdOutServiceMsg:
            case OrdOutPriorHigh:
            case OrdOutPriorLow:
                if (netDevice->downlineData.isReady)
                    {
                    cciHipSetNpuStatus(cci, StNpuNotReadyOutput);
                    }
                else
                    {
                    cciHipSetNpuStatus(cci, StNpuReadyOutput);
                    }
                break;
                }
            }
        break;

    case FcNpuInProgram:
        if (cci->halfWordTransferred)
            {
            cci->halfWordTransferred = FALSE;
            activeChannel->data      = (PpWord)cci->memory[cci->memoryAddress] & 0xFF;
            cci->regCouplerStatus   |= StCplrTransferCompleted;
#if (DEBUG > 1)
            fprintf(netDevice->log, "\n  mem[%04X] : %04X", cci->memoryAddress, cci->memory[cci->memoryAddress]);
#endif
            cci->memoryAddress++;
            }
        else
            {
            cci->halfWordTransferred = TRUE;
            activeChannel->data      = (PpWord)cci->memory[cci->memoryAddress] >> 8;
            }
        activeChannel->full = TRUE;

        break;

    case FcNpuOutMemAddr0:
        if (activeChannel->full)
            {
            cci->tempMemAddr0   = (u8)(activeChannel->data & 0xFF);
            activeChannel->full = FALSE;
#if (DEBUG > 1)
            fprintf(netDevice->log, " %02X", activeChannel->data & 0xFF);
#endif
            }
        break;

    case FcNpuOutMemAddr1:
        if (activeChannel->full)
            {
            cci->memoryAddress  = (u16)((cci->tempMemAddr0 << 8) | (activeChannel->data & 0xFF));
            activeChannel->full = FALSE;
#if (DEBUG > 1)
            fprintf(netDevice->log, " %02X", activeChannel->data & 0xFF);
#endif
            }
        cci->regCouplerStatus |= StCplrAddrLoaded;
        break;

    case FcNpuOutProgram:
        if (activeChannel->full)
            {
            if (cci->halfWordTransferred)
                {
                cci->memory[cci->memoryAddress] = (u16)((cci->tempWord << 8) | (activeChannel->data & 0xFF));
                cci->halfWordTransferred        = FALSE;
                cci->regCouplerStatus          |= StCplrTransferCompleted;
#if (DEBUG > 1)
                fprintf(netDevice->log, "\n  mem[%04X] : %04X", cci->memoryAddress, cci->memory[cci->memoryAddress]);
#endif
                cci->memoryAddress++;
                }
            else
                {
                cci->halfWordTransferred = TRUE;
                cci->tempWord            = activeChannel->data & 0xFF;
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
**  Purpose:        Indicate upline block available
**
**  Parameters:     Name        Description.
**                  block       pointer to block
**                  numBytes    number of bytes in block
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
static void cciHipIndicateUplineBlock(u8 *block, u16 numBytes)
    {
    int      bits;
    CciParam *cci;
    int      prus;
    PpWord   status;
    int      words;

    if ((numBytes > BlkOffDbc)
        && ((block[BlkOffBTBSN] & BlkMaskBT) == BtHTMSG)
        && ((block[BlkOffDbc] & DbcPRU) == DbcPRU))
        {
        bits  = (numBytes - (BlkOffDbc + 1)) * (((block[BlkOffDbc] & Dbc8Bit) != 0) ? 8 : 6);
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
        status = (PpWord)(StNpuInputAvailPru | (prus << 10));
        }
    else if (numBytes <= 256)
        {
        status = StNpuInputAvailLe256;
        }
    else
        {
        status = StNpuInputAvailGt256;
        }
    cci = (CciParam *)netDevice->devDependent;
    cciHipSetNpuStatus(cci, status);
    cci->hipState = StHipUpline;

#if DEBUG
    fprintf(netDevice->log, "\n  hipState -> %s", cciHipState2String(cci->hipState));
#endif
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
static void cciHipSetNpuStatus(CciParam *cci, PpWord status)
    {
#if DEBUG
    FILE *log;

    if (netDevice == NULL)
        {
        log = ((NetDevParam *)activeDevice->controllerContext)->log;
        }
    else
        {
        log = netDevice->log;
        }
    if (cci->hcpState != StHcpRunning)
        {
        fputs("\n  WARNING: Setting NPU status while software not running", log);
        }
#endif

    cci->lastCommandTime   = cycles;
    cci->regNpuStatus      = status;
    cci->regCouplerStatus |= StCplrStatusLoaded;

#if DEBUG
    fprintf(log, "\n  Set NPU status %04o", status);
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        PP reads NPU status register.
**
**  Parameters:     Name        Description.
**
**  Returns:        NPU status register value.
**
**------------------------------------------------------------------------*/
static PpWord cciHipReadNpuStatus(CciParam *cci)
    {
    PpWord value;

    value                  = cci->regNpuStatus;
    cci->regCouplerStatus &= ~StCplrStatusLoaded;
    cci->regNpuStatus      = StNpuIgnore;

    return value;
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
static char *cciHipFunc2String(PpWord funcCode)
    {
    static thread_local char buf[40];

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

    default:
        sprintf(buf, "Unknown Function: %04o", funcCode);
        return buf;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Convert HIP state value to string.
**
**  Parameters:     Name        Description.
**                  state       state value
**
**  Returns:        String equivalent of state value.
**
**------------------------------------------------------------------------*/
static char *cciHipState2String(CciHipState state)
    {
    static thread_local char buf[40];

    switch (state)
        {
    case StHipIdle:
        return "StHipIdle";

    case StHipUpline:
        return "StHipUpline";

    case StHipDownline:
        return "StHipDownline";

    default:
        sprintf(buf, "Unknown state: %d", state);
        return buf;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Convert HCP state value to string.
**
**  Parameters:     Name        Description.
**                  state       state value
**
**  Returns:        String equivalent of state value.
**
**------------------------------------------------------------------------*/
static char *cciHcpState2String(CciHcpState state)
    {
    static thread_local char buf[40];

    switch (state)
        {
    case StHcpNotInitialized:
        return "StHcpNotInitialized";

    case StHcpRunning:
        return "StHcpRunning";

    case StHcpReset:
        return "StHcpReset";

    default:
        sprintf(buf, "Unknown state: %d", state);
        return buf;
        }
    }

#endif

/*---------------------------  End Of File  ------------------------------*/
