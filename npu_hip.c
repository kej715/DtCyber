/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter, Paul Koning
**                2019-2025, Kevin Jordan
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
#define FcNpuHaltNpu               00100
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
#define OrdOutServiceMsg           1
#define OrdOutPriorHigh            2
#define OrdOutPriorLow             3
#define OrdNotReadyForInput        4
#define OrdRegulationLvlChange     5
#define OrdInitRequestAck          6

#define OrdShiftType               8
#define OrdMaskValue               0x0FF

/*
**  Misc constants.
*/
#define ReportInitCount            4

#if DEBUG
#define HexColumn(x)   (4 * (x) + 1 + 4)
#define AsciiColumn(x) (HexColumn(16) + 2 + (x))
#define LogLineLength  (AsciiColumn(16))
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
typedef enum
    {
    StHipInit = 0,
    StHipIdle,
    StHipUpline,
    StHipDownline
    } NpuState;

typedef struct npuParam
    {
    u8        id;
    NpuState  state;
    PpWord    regCouplerStatus;
    PpWord    regNpuStatus;
    PpWord    regOrder;
    bool      isOrderReady;
    u8        initCount;
    u32       lastCommandTime;
    u8        hipRequest;
#define          HipReqNone           0
#define          HipReqNotifyUpline   1
#if DEBUG
    char      logBuf[LogLineLength + 1];
    int       logCol;
#endif
    } NpuParam;

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static void npuHipCreateThread(NetDevParam *param);
static FcStatus npuHipFunc(PpWord funcCode);
static void npuHipIo(void);
static void npuHipActivate(void);
static void npuHipDisconnect(void);
static void npuHipIndicateUplineBlock(u8 *block, u16 numBytes);
static PpWord npuHipReadNpuStatus(NpuParam *npu);
static void npuHipSetNpuStatus(NpuParam *npu, PpWord status);

#if defined(_WIN32)
static void npuThread(void *param);
#else
static void *npuThread(void *param);
#endif

#if DEBUG
static char *npuHipFunc2String(PpWord funcCode);
static void npuLogFlush(FILE *log, NpuParam *npu);
static void npuLogByte(FILE *log, NpuParam *npu, int b);
static char *npuHipOrder2String(PpWord orderCode);
#endif

/*
**  ----------------
**  Public Variables
**  ----------------
*/
u8                       netDevCount = 0;
thread_local NetDevParam *netDevice = NULL;
NetDevParam              netDevices[MaxNetDevices];

/*
**  -----------------
**  Private Variables
**  -----------------
*/
static u8 npuCount = 0;

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
    DevSlot     *dp;
    NetDevParam *netDevice;
    NpuParam    *npu;

    if (netDevCount >= MaxNetDevices)
        {
        logDtError(LogErrorLocation, "Maximum number of network device definitions (%d) exceeded\n", MaxNetDevices);
        exit(1);
        }

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
    **  Allocate and initialise NPU specific parameters.
    */
    npu = calloc(1, sizeof(NpuParam));
    if (npu == NULL)
        {
        logDtError(LogErrorLocation, "Failed to allocate npu context block\n");
        exit(1);
        }

    npu->id        = npuCount++;
    npu->state     = StHipInit;
    npu->initCount = ReportInitCount;

    /*
    **  Allocate and initialise general network device parameters.
    */
    netDevice = &netDevices[netDevCount++];
    memset(netDevice, 0, sizeof(NetDevParam));
    netDevice->id        = npu->id;
    netDevice->type      = NPU_CCP;
    netDevice->channelNo = channelNo;
    netDevice->eqNo      = eqNo;
    netDevice->unitNo    = unitNo;
    sprintf(netDevice->name, "npu%d", npu->id);
    if (deviceName != NULL && strlen(deviceName) > 0)
        {
        netDevice->sectionName = (char *)malloc(strlen(deviceName) + 1);
        if (netDevice->sectionName == NULL)
            {
            logDtError(LogErrorLocation, "Failed to allocate npu context block\n");
            exit(1);
            }
        strcpy(netDevice->sectionName, deviceName);
        }
    netDevice->devDependent = npu;
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
    fprintf(netDevice->log, "NPU%d initialised on channel %o equipment %o\n", npu->id, channelNo, eqNo);
    }
#endif
    /*
    **  Print a friendly message.
    */
    printf("(npu_hip) NPU%d initialised on channel %o equipment %o\n", npu->id, channelNo, eqNo);

    /*
    **  Create NPU emulation thread
    */
    npuHipCreateThread(netDevice);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Process completion of downline block transfer.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
void npuHipNotifyDownlineCopied(void)
    {
    NpuParam *npu;

    npu = (NpuParam *)netDevice->devDependent;
    npuHipSetNpuStatus(npu, StNpuReadyOutput);
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
bool npuHipStartDownlineTransfer(NpuBuffer *bp)
    {
    NpuParam *npu;

    npu = (NpuParam *)netDevice->devDependent;
    if (netDevice->downlineData.isReady == FALSE || netDevice->downlineData.numBytes < 1 || bp == NULL || npu->state != StHipIdle)
        {
#if DEBUG
        if (bp != NULL)
            {
            fprintf(netDevice->log, "\nDownline block rejected, CN=%02X, BT=%02X, PDU size=%d, NPU state %d, downline ready %s\n", netDevice->downlineData.data[BlkOffCN],
                netDevice->downlineData.data[BlkOffBTBSN] & BlkMaskBT, netDevice->downlineData.numBytes, npu->state, netDevice->downlineData.isReady ? "TRUE" : "FALSE");
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
bool npuHipStartUplineTransfer(NpuBuffer *bp)
    {
    NpuParam *npu;

    npu = (NpuParam *)netDevice->devDependent;
    if (netDevice->uplineData.isReady || bp == NULL || npu->state != StHipIdle)
        {
#if DEBUG
        if (bp != NULL)
            {
            fprintf(netDevice->log, "\nUpline block rejected, CN=%02X, BT=%02X, PDU size=%d, NPU state %d, upline ready %s\n", bp->data[BlkOffCN],
                bp->data[BlkOffBTBSN] & BlkMaskBT, bp->numBytes, npu->state, netDevice->uplineData.isReady ? "TRUE" : "FALSE");
            traceStack(netDevice->log);
            }
#endif
        return FALSE;
        }

    npuHipIndicateUplineBlock(bp->data, bp->numBytes);

    return TRUE;
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

    if (netDevice != NULL && netDevice->log != NULL)
        {
        fprintf(netDevice->log, "\n\n");
        va_start(args, format);
        vfprintf(netDevice->log, format, args);
        va_end(args);
        fprintf(netDevice->log, "\n");
        }
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
**  Purpose:        Create thread which will emulate an NPU.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuHipCreateThread(NetDevParam *param)
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
        (LPTHREAD_START_ROUTINE)npuThread,
        (LPVOID)param,                              // thread parameter
        0,                                          // not suspended
        &dwThreadId);                               // returns thread ID

    if (hThread == NULL)
        {
        logDtError(LogErrorLocation, "Failed to create NPU thread\n");
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
    rc = pthread_create(&thread, &attr, npuThread, param);
    if (rc < 0)
        {
        logDtError(LogErrorLocation, "Failed to create NPU thread\n");
        exit(1);
        }
#endif
    puts("(npu_hip) NPU thread created");
    }

/*--------------------------------------------------------------------------
**  Purpose:        NPU emulation thread.
**
**  Parameters:     Name        Description.
**                  param       Pointer to network device parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
#if defined(_WIN32)
static void npuThread(void *param)
#else
static void *npuThread(void *param)
#endif
    {
    NpuParam *npu;
    PpWord   orderType;
    u8       orderValue;
    char     *sectionName;

    /*
    **  Parse data communication configuration
    */
    netDevice   = (NetDevParam *)param;
    npu         = (NpuParam *)netDevice->devDependent;
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
    **  Print a friendly message.
    */
    printf("(npu_hip) NPU%d host ID:      %s\n", npu->id, npuNetHostID);
    printf("(npu_hip) NPU%d coupler node: %u\n", npu->id, netDevice->couplerNode);
    printf("(npu_hip) NPU%d NPU node:     %u\n", npu->id, netDevice->npuNode);
#if DEBUG
    fprintf(netDevice->log, "NPU%d host ID:      %s\n", npu->id, npuNetHostID);
    fprintf(netDevice->log, "NPU%d coupler node: %u\n", npu->id, netDevice->couplerNode);
    fprintf(netDevice->log, "NPU%d NPU node:     %u\n", npu->id, netDevice->npuNode);
#endif

    /*
    **  Initialise BIP, SVC, and TIP.
    */
    npuBipInit();
    npuSvmInit();
    npuTipInit();
    npuNetInit();

    for (;;)
        {
        if (netDevice->doReset)
            {
            /*
            **  Reset all subsystems - order matters
            */
            npuNetReset();
            npuTipReset();
            npuSvmReset();
            npuBipReset();
            netDevice->doReset = FALSE;
            }

        /*
        **  Process downline order, if any
        */
        if (npu->isOrderReady)
            {
#if DEBUG
            npuLogFlush(netDevice->log, npu);
            fprintf(netDevice->log, "\n    Order %04o %s", npu->regOrder, npuHipOrder2String(npu->regOrder));
#endif
            orderType         = (npu->regOrder >> OrdShiftType) & Mask3;
            orderValue        = (u8)(npu->regOrder & OrdMaskValue);
            npu->isOrderReady = FALSE;

            switch (orderType)
                {
            default:
                break;

            case OrdRegulationLvlChange:
#if DEBUG
                fprintf(netDevice->log, " : %o", orderValue);
#endif
                npuSvmNotifyHostRegulation(orderValue);
                break;
                }
            }

        /*
        **  Process downline block, if any
        */
        if (netDevice->downlineData.isReady)
            {
            orderType = (npu->regOrder >> OrdShiftType) & Mask3;
            switch (orderType)
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
        if (npu->hipRequest != HipReqNone)
            {
#if DEBUG
            npuLogFlush(netDevice->log, npu);
            fputs("\n    ", netDevice->log);
#endif
            switch (npu->hipRequest)
                {
            case HipReqNotifyUpline:
#if DEBUG
                fputs("Notify upline block sent", netDevice->log);
#endif
                npuBipNotifyUplineSent();
                netDevice->uplineData.isReady = FALSE;
                break;

            default:
                break;
                }

            npu->hipRequest = HipReqNone;
            }

        /*
        **  Process upline block, if any
        */
        if (netDevice->uplineData.isReady == FALSE)
            {
            npuBipTryUplineBlock();
            }
        else if (npu->state == StHipIdle && (npu->regCouplerStatus & StCplrStatusLoaded) == 0 && netDevice->uplineData.offset == 0)
            {
            npuHipIndicateUplineBlock(netDevice->uplineData.data, netDevice->uplineData.numBytes);
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
static FcStatus npuHipFunc(PpWord funcCode)
    {
    ChIoBuffer  *bp;
    u8          id;
    NetDevParam *netDevice;
    NpuParam    *npu;

    funcCode &= ~FcNpuEqMask;
    netDevice = (NetDevParam *)activeDevice->controllerContext;
    npu       = (NpuParam *)netDevice->devDependent;

#if DEBUG
    npuLogFlush(netDevice->log, npu);
    if (funcCode != FcNpuInCouplerStatus)
        {
        fprintf(netDevice->log, "\nPP:%02o CH:%02o f:%04o T:%-25s  >   ",
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
        fputs(" FUNC not implemented & declined!", netDevice->log);
#endif

        return FcDeclined;

    case FcNpuInCouplerStatus:
        switch (npu->state)
            {
        case StHipInit:
            if (npu->initCount > 0)
                {
                /*
                **  Tell PIP a few times that the NPU has initialized.
                */
                npu->initCount -= 1;
                npuHipSetNpuStatus(npu, StNpuInitCompleted);
                }
            else
                {
                npu->state = StHipIdle;
                npuHipSetNpuStatus(npu, StNpuIdle);
                }

            break;

        case StHipIdle:
            /*
            **  Announce idle state to PIP at intervals of less then one second,
            **  otherwise PIP will assume that the NPU is dead.
            */
            if (cycles - npu->lastCommandTime > CyclesOneSecond)
                {
                npuHipSetNpuStatus(npu, StNpuIdle);
                }
            break;

        default:
            break;
            }

        break;

    case FcNpuInData:
        if (netDevice->uplineData.isReady == FALSE)
            {
            /*
            **  Unexpected input request by host.
            */
            npu->state                 = StHipIdle;
            activeDevice->recordLength = 0;
            activeDevice->fcode        = FcNpuNothing;

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
            npu->state                 = StHipIdle;
            activeDevice->recordLength = 0;
            activeDevice->fcode        = FcNpuNothing;

            return FcDeclined;
            }
        bp                         = &netDevice->downlineData;
        bp->offset                 = 0;
        bp->numBytes               = 0;
        activeDevice->recordLength = 0;
        npu->state                 = StHipDownline;
        break;

    case FcNpuInNpuStatus:
    case FcNpuInNpuOrder:
        break;

    case FcNpuOutNpuOrder:
        npu->state = StHipIdle;
        npuHipSetNpuStatus(npu, StNpuIdle);
        break;

    case FcNpuClearNpu:
        /*
        **  Reset HIP state.
        */
        id = npu->id;
        memset(npu, 0, sizeof(NpuParam));
        npu->id            = id;
        npu->initCount     = ReportInitCount;
        npu->state         = StHipInit;
        netDevice->doReset = TRUE;
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
        return FcProcessed;
        }

    activeDevice->fcode = funcCode;

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
static void npuHipIo(void)
    {
    ChIoBuffer  *bp;
    NetDevParam *netDevice;
    NpuParam    *npu;

    netDevice = (NetDevParam *)activeDevice->controllerContext;
    npu       = (NpuParam *)netDevice->devDependent;

    switch (activeDevice->fcode)
        {
    default:
        break;

    case FcNpuInNpuStatus:
        activeChannel->data = npuHipReadNpuStatus(npu);
        activeChannel->full = TRUE;
#if DEBUG
        fprintf(netDevice->log, " %03X", activeChannel->data);
#endif
        break;

    case FcNpuInCouplerStatus:
        activeChannel->data    = npu->regCouplerStatus;
        activeChannel->full    = TRUE;
        npu->regCouplerStatus |= StCplrNpuStatusRead;
#if DEBUG
        if (npu->regCouplerStatus != 0)
            {
            fprintf(netDevice->log, "\nPP:%02o CH:%02o f:%04o T:%-25s  >    %03X",
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
        fprintf(netDevice->log, " %03X", activeChannel->data);
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
            activeDevice->recordLength -= 1;
            if (activeDevice->recordLength == 0)
                {
                /*
                **  Transmission complete.
                */
                activeChannel->data          |= 04000;
                activeChannel->discAfterInput = TRUE;
                activeDevice->fcode           = FcNpuNothing;
                npu->state                    = StHipIdle;
                npu->hipRequest               = HipReqNotifyUpline;
                }
#if DEBUG
            npuLogByte(netDevice->log, npu, activeChannel->data);
#endif
            }
        break;

    case FcNpuOutData:
        if (activeChannel->full)
            {
#if DEBUG
            npuLogByte(netDevice->log, npu, activeChannel->data);
#endif
            activeChannel->full = FALSE;
            bp                  = &netDevice->downlineData;
            if (bp->numBytes < MaxBuffer)
                {
                bp->data[bp->numBytes++] = activeChannel->data & Mask8;
                }
            activeDevice->recordLength += 1;
            if ((activeChannel->data & 04000) != 0)
                {
                /*
                **  Top bit set - process message.
                */
                activeDevice->fcode = FcNpuNothing;
                npu->state          = StHipIdle;
                bp->isReady         = TRUE;
                }
            }
        break;

    case FcNpuOutNpuOrder:
        if (activeChannel->full)
            {
            npu->regOrder = activeChannel->data;
            switch ((npu->regOrder >> OrdShiftType) & Mask3)
                {
            default:
                break;

            case OrdOutServiceMsg:
            case OrdOutPriorHigh:
            case OrdOutPriorLow:
                npuHipSetNpuStatus(npu, StNpuReadyOutput);
                break;
                }
            npu->isOrderReady   = TRUE;
            activeChannel->full = FALSE;
#if DEBUG
            fprintf(netDevice->log, " Order word %03X - function %02X : %s",
                activeChannel->data, activeChannel->data >> 8, npuHipOrder2String(activeChannel->data));
#endif
            }
        break;

    case FcNpuInMemAddr0:
    case FcNpuInMemAddr1:
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
**  Purpose:        Indicate upline block available
**
**  Parameters:     Name        Description.
**                  block       pointer to block
**                  numBytes    number of bytes in block
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
static void npuHipIndicateUplineBlock(u8 *block, u16 numBytes)
    {
    int      bits;
    NpuParam *npu;
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
    npu = (NpuParam *)netDevice->devDependent;
    npuHipSetNpuStatus(npu, status);
    npu->state = StHipUpline;
    }

/*--------------------------------------------------------------------------
**  Purpose:        NPU sets NPU status register.
**
**  Parameters:     Name        Description.
**                  npu         pointer to NPU parameters
**                  status      new status register value.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuHipSetNpuStatus(NpuParam *npu, PpWord status)
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
static PpWord npuHipReadNpuStatus(NpuParam *npu)
    {
    PpWord value;

    value                  = npu->regNpuStatus;
    npu->regCouplerStatus &= ~StCplrStatusLoaded;
    npu->regNpuStatus      = StNpuIgnore;

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
static char *npuHipFunc2String(PpWord funcCode)
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

    case FcNpuHaltNpu:
        return "FcNpuHaltNpu";

    case FcNpuClearNpu:
        return "FcNpuClearNpu";

    case FcNpuClearCoupler:
        return "FcNpuClearCoupler";
        }
    sprintf(buf, "Unknown Function: %04o", funcCode);

    return buf;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Flush incomplete numeric/ascii data line
**
**  Parameters:     Name        Description.
**
**  Returns:        nothing
**
**------------------------------------------------------------------------*/
static void npuLogFlush(FILE *log, NpuParam *npu)
    {
    if (npu->logCol != 0)
        {
        fputs(npu->logBuf, log);
        }
    npu->logCol = 0;
    memset(npu->logBuf, ' ', LogLineLength);
    npu->logBuf[0]             = '\n';
    npu->logBuf[LogLineLength] = '\0';
    }

/*--------------------------------------------------------------------------
**  Purpose:        Log a byte in hex/ascii form
**
**  Parameters:     Name        Description.
**
**  Returns:        nothing
**
**------------------------------------------------------------------------*/
static void npuLogByte(FILE *log, NpuParam *npu, int b)
    {
    char hex[5];
    int  col;

    col = HexColumn(npu->logCol);
    sprintf(hex, "%03X ", b);
    memcpy(npu->logBuf + col, hex, 4);

    col = AsciiColumn(npu->logCol);
    b  &= 0x7f;
    if ((b < 0x20) || (b >= 0x7f))
        {
        b = '.';
        }

    npu->logBuf[col] = (u8)b;
    if (++npu->logCol == 16)
        {
        npuLogFlush(log, npu);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Convert order code to string.
**
**  Parameters:     Name        Description.
**                  orderCode   order code
**
**  Returns:        String equivalent of order code.
**
**------------------------------------------------------------------------*/
static char *npuHipOrder2String(PpWord orderCode)
    {
    static char *orderStr[] =
        {
        "",
        "output level one - service message",
        "output level two - high priority",
        "output level three - low priority",
        "driver not ready for input",
        "regulation level change",
        "initialization request acknowledgment",
        ""
        };

    return orderStr[(orderCode >> OrdShiftType) & Mask3];
    }

#endif

/*---------------------------  End Of File  ------------------------------*/
