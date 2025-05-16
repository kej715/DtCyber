/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**                     2025, Kevin Jordan
**
**  Name: maintenance_channel.c
**
**  Description:
**      Perform emulation of maintenance channel.
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
#ifndef WIN32
#include <unistd.h>
#endif
#include "const.h"
#include "types.h"
#include "proto.h"

/*
**  -----------------
**  Private Constants
**  -----------------
*/

/*
**  Shift counts for Conn, Op, and Type fields
**  of function codes
*/
#define FcConnShift                8
#define FcOpShift                  4
#define FcTypeShift                0

/*
**  Function codes.
*/
#define FcOpHalt                   0x00
#define FcOpStart                  0x01
#define FcOpClearLed               0x03
#define FcOpRead                   0x04
#define FcOpWrite                  0x05
#define FcOpMasterClear            0x06
#define FcOpClearErrors            0x07
#define FcOpEchoData               0x08
#define FcOpStatusSummary          0x0C

/*
**  IOU Register addresses.
*/
#define RegIouStatusSummary        0x00
#define RegIouElementId            0x10
#define RegIouOptionsInstalled     0x12
#define RegIouFaultStatus          0x18
#define RegIouOsBounds             0x21
#define RegIouEnvControl           0x30
#define RegIouStatus               0x40
#define RegIouFaultStatus1         0x80
#define RegIouFaultStatus2         0x81
#define RegIouTestMode             0xA0

/*
**  Memory Register addresses.
*/
#define RegMemStatusSummary        0x00
#define RegMemElementId            0x10
#define RegMemOptionsInstalled     0x12
#define RegMemEnvControl           0x20
#define RegMemCEL                  0xA0
#define RegMemCELd0                0xA0
#define RegMemCELd1                0xA1
#define RegMemDELd2                0xA2
#define RegMemCELd3                0xA3
#define RegMemUEL1                 0xA4
#define RegMemUEL1d0               0xA4
#define RegMemUEL1d1               0xA5
#define RegMemUEL1d2               0xA6
#define RegMemUEL1d3               0xA7
#define RegMemUEL2                 0xA8
#define RegMemUEL2d0               0xA8
#define RegMemUEL2d1               0xA9
#define RegMemUEL2d2               0xAA
#define RegMemUEL2d3               0xAB

/*
**  Processor Register addresses.
*/
#define RegProcStatusSummary       0x00
#define RegProcElementId           0x10
#define RegProcProcessorId         0x11
#define RegProcOptionsInstalled    0x12
#define RegProcVmCapabilityList    0x13
#define RegProcDepEnvControl       0x30
#define RegProcCtrlStoreAddr       0x31
#define RegProcCtrlStoreBreak      0x32
#define RegProcMonitorProcState    0x41
#define RegProcProcessIntTimer     0x44
#define RegProcPageTableAddr       0x48
#define RegProcPageTableLen        0x49
#define RegProcPageSizeMask        0x4A
#define RegProcModelDepWord        0x51
#define RegProcJobProcessState     0x61
#define RegProcSystemIntTimer      0x62
#define RegProcFaultStatus0        0x80
#define RegProcFaultStatus1        0x81
#define RegProcFaultStatus2        0x82
#define RegProcFaultStatus3        0x83
#define RegProcFaultStatus4        0x84
#define RegProcFaultStatus5        0x85
#define RegProcFaultStatus6        0x86
#define RegProcFaultStatus7        0x87
#define RegProcFaultStatus8        0x88
#define RegProcFaultStatus9        0x89
#define RegProcFaultStatusA        0x8A
#define RegProcFaultStatusB        0x8B
#define RegProcFaultStatusC        0x8C
#define RegProcFaultStatusD        0x8D
#define RegProcFaultStatusE        0x8E
#define RegProcFaultStatusF        0x8F
#define RegProcCCEL                0x92
#define RegProcMCEL                0x93
#define RegProcTestMode            0xA0
#define RegProcTestMode0           0xA0
#define RegProcTestMode1           0xA1
#define RegProcTestMode2           0xA2
#define RegProcTestMode3           0xA3

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

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static char *mchCw2String(u8 connCode, u8 typeCode, PpWord location);
static char *mchFn2String(u8 connCode, u8 opCode, u8 typeCode);
static FcStatus mchFunc(PpWord funcCode);
static Cpu180Context *mchGetCpContext(u8 connCode);
static u64  mchGetRegister(u8 reg);
static void mchIo(void);
static void mchActivate(void);
static void mchDisconnect(void);
static bool mchIsConnected(u8 connCode);
static bool mchIsCp(u8 connCode, u8 typeCode);
static char *mchOp2String(u8 opCode);
static void mchSetRegister(u8 reg, u64 word);

static u64  mch860CmGetter(u8 reg);
static u8   mch860CmReader(void);
static void mch860CmSetter(u8 reg, u64 word);
static void mch860CmWriter(u8 byte);
static u64  mch860CpGetter(u8 reg);
static u8   mch860CpReader(void);
static void mch860CpSetter(u8 reg, u64 word);
static void mch860CpWriter(u8 byte);
static u8   mch860CsReader(void);
static void mch860CsWriter(u8 byte);
static void mch860Init(u64 iouOptions, u64 memSizeMask);
static u64  mch860IouGetter(u8 reg);
static u8   mch860IouReader(void);
static void mch860IouSetter(u8 reg, u64 word);
static void mch860IouWriter(u8 byte);
static u8   mch860RfReader(void);
static void mch860RfWriter(u8 byte);
static u8   mch860SmReader(void);
static void mch860SmWriter(u8 byte);

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
static u64  *mchCmRegisters;
static u8   *mchControlStores[2];
static int  mchControlStoreIndices[2];
static u64  *mchCpRegisterGroups[2];
static u64  *mchIouRegisters;
static u64  *mchRegisterFiles[2];
static int  mchRegisterFileIndices[2];
static u8   **mchSoftMemoryGroups[2];
static int  mchSoftMemoryIndices[2][7];

static u8   mchConnCode;
static u8   mchLocation;
static bool mchLocationReady;
static u64  mchRegisterWord;
static u8   mchTypeCode;

static u64  mchTimeout = 0;

//
//  Bit masks identifying PP's in IOU OS Bounds and fault registers
//
static u64  mchPpMasks[20] =
    {
    0x01000000, // PP00
    0x02000000, // PP01
    0x04000000, // PP02
    0x08000000, // PP03
    0x10000000, // PP04
    0x00010000, // PP05
    0x00020000, // PP06
    0x00040000, // PP07
    0x00080000, // PP10
    0x00100000, // PP11
    0x00000100, // PP20
    0x00000200, // PP21
    0x00000400, // PP22
    0x00000800, // PP23
    0x00001000, // PP24
    0x00000001, // PP25
    0x00000002, // PP26
    0x00000004, // PP27
    0x00000008, // PP30
    0x00000010  // PP31
    };

#if DEBUG
static FILE *mchLog    = NULL;
static int  mchBytesIo = 0;
#endif

/*
 **--------------------------------------------------------------------------
 **
 **  Public Functions
 **
 **--------------------------------------------------------------------------
 */

/*--------------------------------------------------------------------------
**  Purpose:        Check whether a maintenance channel timeout has occurred.
**
**                  When a timeout occurs, the channel is set inactive and
**                  empty. Normally, a timeout is established only when a
**                  maintenance channel function has been declined, and this
**                  occurs only when a connection code provided in a function
**                  request is not supported by the machine.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void mchCheckTimeout()
    {
    if (mchTimeout != 0 && mchTimeout < getMilliseconds())
        {
        mchTimeout = 0;
        if (channel[ChMaintenance].full && channel[ChMaintenance].active && channel[ChMaintenance].ioDevice == NULL)
            {
            channel[ChMaintenance].full   = FALSE;
            channel[ChMaintenance].active = FALSE;
#if DEBUG
            fprintf(mchLog, "\n%12d PP:%02o CH:%02o Timeout",
                    traceSequenceNo,
                    activePpu->id,
                    activeDevice->channel->id);
            fflush(mchLog);
#endif
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Get a value from a CP maintenance register
**
**  Parameters:     Name        Description.
**                  ctx         CP context
**                  reg         the register address
**
**  Returns:        64-bit value.
**
**------------------------------------------------------------------------*/
u64 mchGetCpRegister(Cpu180Context *ctx, u8 reg)
    {
    u64 byte;

    switch (reg)
        {
    case RegProcStatusSummary:
        byte = mchCpRegisterGroups[ctx->id][0] & 0xff;
        if (ctx->isStopped)
            {
            byte |= 0x08;
            }
        if (ctx->isMonitorMode)
            {
            byte |= 0x20;
            }
        return (byte << 56) | (byte << 48) | (byte << 40) | (byte << 32)
             | (byte << 24) | (byte << 16) | (byte <<  8) | byte;
    case RegProcCtrlStoreAddr:
        return mchControlStoreIndices[ctx->id] >> 4; // 16 bytes per control store address
    case RegProcJobProcessState:
        return ctx->regJps;
    case RegProcMonitorProcState:
        return ctx->regMps;
    case RegProcPageTableAddr:
        return ctx->regPta;
    case RegProcPageTableLen:
        return ctx->regPtl;
    case RegProcPageSizeMask:
        return ctx->regPsm;
    case RegProcProcessIntTimer:
        return ctx->regPit;
    case RegProcSystemIntTimer:
        return ctx->regSit;
    case RegProcVmCapabilityList:
        return ctx->regVmcl;
    case RegProcModelDepWord:
        return ctx->regMdw;
    case RegProcDepEnvControl:
    default:
        break;
    //  Trap Enables addresses
    case 0xc0:
    case 0xc1:
    case 0xc2:
    case 0xc3:
        return ctx->regFlags & Mask2;
    //  Keypoint Enable addresses
    case 0xca:
    case 0xcb:
        return (ctx->regFlags >> 13) & 1;
        break;
    //  Critical Frame Flag addresses
    case 0xe0:
    case 0xe1:
        return (ctx->regFlags >> 15) & 1;
    //  On Condition Flag addresses
    case 0xe2:
    case 0xe3:
        return (ctx->regFlags >> 14) & 1;
        }

    return mchCpRegisterGroups[ctx->id][reg];
    }

/*--------------------------------------------------------------------------
**  Purpose:        Initialise maintenance channel.
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
void mchInit(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName)
    {
    u64     channelMask;
    DevSlot *dp;
    u64     iouOptionsInstalled;
    u64     memSizeMask;

#if DEBUG
    if (mchLog == NULL)
        {
        mchLog = fopen("mchlog.txt", "wt");
        }
#endif

    dp             = channelAttach(channelNo, eqNo, DtMch);
    dp->activate   = mchActivate;
    dp->disconnect = mchDisconnect;
    dp->func       = mchFunc;
    dp->io         = mchIo;

    channelMask = 0x000000FFAF000000; // channels 00 - 17
    if (channelCount > 16)
        {
        channelMask |= 0x0000000000FF0F00;
        }

    switch ((cpuMaxMemory * 8) / OneMegabyte)
        {
    case 1:
        memSizeMask = 0x8000;
        break;
    case 2:
        memSizeMask = 0x4000;
        break;
    case 3:
        memSizeMask = 0x2000;
        break;
    case 4:
        memSizeMask = 0x1000;
        break;
    case 5:
        memSizeMask = 0x0800;
        break;
    case 6:
        memSizeMask = 0x0400;
        break;
    case 7:
        memSizeMask = 0x0200;
        break;
    case 8:
        memSizeMask = 0x0100;
        break;
    case 10:
        memSizeMask = 0x0080;
        break;
    case 12:
        memSizeMask = 0x0040;
        break;
    case 14:
        memSizeMask = 0x0020;
        break;
    case 16:
        memSizeMask = 0x0010;
        break;
    case 2048:
        memSizeMask = 0x8008;
        break;
    case 1024:
        memSizeMask = 0x4008;
        break;
    case 512:
        memSizeMask = 0x2008;
        break;
    case 256:
        memSizeMask = 0x1008;
        break;
    case 128:
        memSizeMask = 0x0808;
        break;
    case 64:
        memSizeMask = 0x0408;
        break;
    case 32:
        memSizeMask = 0x0208;
        break;
    default:
        logDtError(LogErrorLocation, "Unsupported memory size: %ld", cpuMaxMemory * 8);
        exit(1);
        }
    memSizeMask <<= 48;

    iouOptionsInstalled  = channelMask;
    iouOptionsInstalled |= (u64)0x03 << 40;     // PP's 00 - 11
    if (ppuCount > 10)
        {
        iouOptionsInstalled |= (u64)0x0C << 40; // PP's 20 - 31
        }
    if (tpMuxEnabled)
        {
        iouOptionsInstalled |= 2;
        }
    if (cc545Enabled)
        {
        iouOptionsInstalled |= 1;
        }
    iouOptionsInstalled |= 0x04; // radial interfaces 1,2

    switch (modelType)
        {
    case ModelCyber860:
        mch860Init(iouOptionsInstalled, memSizeMask);
        break;
    default:
        logDtError(LogErrorLocation, "Unsupported machine model: %d", modelType);
        exit(1);
        }

    /*
    **  Print a friendly message.
    */
    printf("(maintenance_channel) Initialised on channel %o\n", channelNo);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Set CP maintenance register to a value
**
**  Parameters:     Name        Description.
**                  ctx         CP context
**                  reg         the register address
**                  word        the 64-bit value to set
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void mchSetCpRegister(Cpu180Context *ctx, u8 reg, u64 word)
    {
    switch (reg)
        {
    case RegProcCtrlStoreAddr:
        mchControlStoreIndices[ctx->id] = word << 4; // 16 bytes per control store address
        break;
    case RegProcJobProcessState:
        ctx->regJps = word & Mask32;
        break;
    case RegProcMonitorProcState:
        ctx->regMps = word & Mask32;
        break;
    case RegProcPageTableAddr:
        ctx->regPta = word & Mask32;
        break;
    case RegProcPageTableLen:
        ctx->regPtl = word & Mask8;
        cpu180UpdatePageSize(ctx);
        break;
    case RegProcPageSizeMask:
        ctx->regPsm = word & Mask7;
        cpu180UpdatePageSize(ctx);
        break;
    case RegProcVmCapabilityList:
        ctx->regVmcl = word & Mask16;
        break;
    case RegProcProcessIntTimer:
        ctx->regPit = word & Mask32;
        break;
    case RegProcSystemIntTimer:
        ctx->regSit = word & Mask32;
        break;
    case RegProcModelDepWord:
        ctx->regMdw = word;
        break;
    case RegProcStatusSummary:
        ctx->isStopped     = (word & 0x08) != 0;
        ctx->isMonitorMode = (word & 0x20) != 0;
    case RegProcDepEnvControl:
    default:
        mchCpRegisterGroups[ctx->id][reg] = word;
        break;
    //  Trap Enables addresses
    case 0xc0:
    case 0xc1:
    case 0xc2:
    case 0xc3:
        ctx->regFlags = (ctx->regFlags & 0xffc0) | (word & Mask2);
        break;
    //  Keypoint Enable addresses
    case 0xca:
    case 0xcb:
        ctx->regFlags = (ctx->regFlags & 0xdfff) | ((word & 1) << 13);
        break;
    //  Critical Frame Flag addresses
    case 0xe0:
    case 0xe1:
        ctx->regFlags = (ctx->regFlags & 0x7fff) | ((word & 1) << 15);
        break;
    //  On Condition Flag addresses
    case 0xe2:
    case 0xe3:
        ctx->regFlags = (ctx->regFlags & 0xbfff) | ((word & 1) << 14);
        break;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Set OS bounds fault flag in IOU FS1 register.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void mchSetOsBoundsFault(PpSlot *pp, u32 address, u32 boundary)
    {
#if DEBUG
    fprintf(mchLog, "\n%12d PP:%02o OS bounds fault, reference to %o is %s boundary %o",
            traceSequenceNo,
            pp->id,
            address,
            activePpu->isBelowOsBound ? "above" : "below",
            boundary);
    fflush(mchLog);
#endif
    mchIouRegisters[RegIouFaultStatus1] |= (mchPpMasks[pp->id] << 32) | 0x040000;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Handle channel activation.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mchActivate(void)
    {
#if DEBUG
    fprintf(mchLog, "\n%12d PP:%02o CH:%02o Activate",
            traceSequenceNo,
            activePpu->id,
            activeDevice->channel->id);
    fflush(mchLog);
    mchBytesIo = 0;
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Handle disconnecting of channel.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mchDisconnect(void)
    {
    Cpu180Context *ctx;

#if DEBUG
    fprintf(mchLog, "\n%12d PP:%02o CH:%02o Disconnect",
            traceSequenceNo,
            activePpu->id,
            activeDevice->channel->id);
    fflush(mchLog);
#endif
    switch ((activeDevice->fcode >> FcOpShift) & Mask4)
        {
    case FcOpRead:
    case FcOpWrite:
        if (mchLocationReady == FALSE)
            {
            mchLocationReady           = TRUE;
            mchConnCode                = (activeDevice->fcode >> FcConnShift) & Mask4;
            mchTypeCode                = (activeDevice->fcode >> FcTypeShift) & Mask4;
            activeDevice->recordLength = 8;
            if (mchIsCp(mchConnCode, mchTypeCode))
                {
                switch (mchTypeCode)
                    {
                case 3:
                case 4:
                case 5:
                case 6:
                    ctx = mchGetCpContext(mchConnCode);
                    mchSoftMemoryIndices[ctx->id][mchTypeCode] = mchLocation << 2; // 4 bytes per memory address
                    break;
                case 7:
                    ctx = mchGetCpContext(mchConnCode);
                    mchRegisterFileIndices[ctx->id] = mchLocation << 3;            // 8 bytes per register file address
                    break;
                default:
                    break;
                    }
                }
            }
    default:
        break;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Get a word from a maintenance register of the currently
**                  selected unit
**
**  Parameters:     Name        Description.
**                  reg         register address
**
**  Returns:        64-bit register value
**
**------------------------------------------------------------------------*/
static u64 mchGetRegister(u8 reg)
    {
    switch (modelType)
        {
    case ModelCyber860:
        switch (mchConnCode)
            {
        case 0:
            return mch860IouGetter(reg);
        case 1: // CP or CM
        case 2:
            switch (mchTypeCode)
                {
            case 0:
                return mch860CpGetter(reg);
            case 0x0a:
                return mch860CmGetter(reg);
            default:
                break;
                }
        default:
            break;
            }
    default:
        break;
        }

    return 0;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Execute function code on maintenance channel.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        FcStatus
**
**------------------------------------------------------------------------*/
static FcStatus mchFunc(PpWord funcCode)
    {
    u64           csAddr;
    Cpu180Context *ctx;
    u8            opCode;
    u64           word;

    mchConnCode = (funcCode >> FcConnShift) & Mask4;
    opCode      = (funcCode >> FcOpShift) & Mask4;
    mchTypeCode = (funcCode >> FcTypeShift) & Mask4;

    /*
    **  Connect codes 0x800 - 0xF00 causes the MCH to be deselected.
    */
    if (mchConnCode >= 8)
        {
#if DEBUG
    fprintf(mchLog, "\n%12d PP:%02o CH:%02o f:0x%03X MCH deselect",
            traceSequenceNo,
            activePpu->id,
            activeDevice->channel->id,
            funcCode);
#endif
        activeDevice->fcode = funcCode;
        mchTimeout = 0;
        return FcProcessed;
        }

#if DEBUG
    fprintf(mchLog, "\n%12d PP:%02o CH:%02o f:0x%03X C:%X O:%X T:%X (%s)",
            traceSequenceNo,
            activePpu->id,
            activeDevice->channel->id,
            funcCode,
            mchConnCode,
            opCode,
            mchTypeCode,
            mchFn2String(mchConnCode, opCode, mchTypeCode));
#endif

    if (mchIsConnected(mchConnCode) == FALSE)
        {
#if DEBUG
        fputs("  Declined", mchLog);
#endif
        mchTimeout = getMilliseconds() + 1;

        return FcDeclined;
        }
    mchTimeout = 0;

    /*
    **  Process operation codes.
    */
    switch (opCode)
        {
    default:
#if DEBUG
        fputs(" : Operation not implemented & declined", mchLog);
#endif
        return FcDeclined;

    case FcOpHalt:
        if (mchIsCp(mchConnCode, mchTypeCode))
            {
            word = mchGetRegister(RegProcStatusSummary) | 0x08;
            mchSetRegister(RegProcStatusSummary, word);
            }
        return FcProcessed;

    case FcOpStart:
        if (mchIsCp(mchConnCode, mchTypeCode))
            {
            //
            //  With CIP L826 for 860/870:
            //
            //  - When the CP is started at control store address 0x700,
            //    CIP is verifying control store and expects the CP to
            //    halt at address 0x705.
            //
            //  - When the CP is started at control store address 0x381,
            //    CIP has established the EI and is starting it.
            //
            word   = mchGetRegister(RegProcStatusSummary) & ~(u64)0x08;
            csAddr = mchGetRegister(RegProcCtrlStoreAddr);
            if (csAddr == 0x700)
                {
                word |= 0x08;  // Processor Halt
                mchSetRegister(RegProcCtrlStoreAddr, 0x705);
                }
            else if (csAddr == 0x381)
                {
                MonitorCondition mcr;
                u32 rma;

                ctx = mchGetCpContext(mchConnCode);
                cpu180Load180Xp(ctx, ctx->regMps >> 3);
                mchSetRegister(RegProcStatusSummary, word);
                if (cpu180PvaToRma(ctx, cpMem[ctx->regMps >> 3] & Mask48, AccessModeExecute, &rma, &mcr) == FALSE)
                    {
                    logDtError(LogErrorLocation, "Failed to start CPU: failed to translate PVA %012lx to RMA, MCR %04x\n",
                        cpMem[ctx->regMps >> 3] & Mask48, mcr);
                    }
#if DEBUG
                else
                    {
                    fprintf(mchLog, "\n%12d PP:%02o CH:%02o Start CPU at RMA %08x",
                    traceSequenceNo,
                    activePpu->id,
                    activeDevice->channel->id,
                    rma);
                    }
#endif
                mchSetRegister(RegProcStatusSummary, word);
                }
            else
                {
                word |= 0x08;  // Processor Halt
                }
            mchSetRegister(RegProcStatusSummary, word);
            }
        return FcProcessed;

    case FcOpMasterClear:
        if (mchIsCp(mchConnCode, mchTypeCode))
            {
            mchSetRegister(RegProcStatusSummary, 0x28); // CYBER 180 monitor mode, Processor Halt
            mchSetRegister(RegProcDepEnvControl, 0);
            }
        return FcProcessed;

    case FcOpClearLed:
    case FcOpClearErrors:
        /*
        **  Do nothing.
        */
        return FcProcessed;

    case FcOpRead:
    case FcOpWrite:
    case FcOpEchoData:
        mchLocation                = 0;
        mchLocationReady           = FALSE;
        activeDevice->recordLength = 2;
        break;

    case FcOpStatusSummary:
        activeDevice->recordLength = 1;
        break;
        }

    activeDevice->fcode = funcCode;

    return FcAccepted;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Get the CPU context associated with a connect code.
**
**  Parameters:     Name        Description.
**                  connCode    connect code
**
**  Returns:        Pointer to CPU context, or NULL if connect code does
**                  not identify a configured CPU.
**
**------------------------------------------------------------------------*/
static Cpu180Context *mchGetCpContext(u8 connCode)
    {
    if (mchConnCode == 1)
        {
        return &cpus180[0];
        }
    else if (cpuCount > 1 && mchConnCode == 2)
        {
        return &cpus180[1];
        }
    else
        {
        return NULL;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Perform I/O on maintenance channel.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mchIo(void)
    {
    u8 opCode;

    mchConnCode = (activeDevice->fcode >> FcConnShift) & Mask4;
    opCode      = (activeDevice->fcode >> FcOpShift) & Mask4;
    mchTypeCode = (activeDevice->fcode >> FcTypeShift) & Mask4;

    if (mchConnCode >= 8)
        {
#if DEBUG
        fprintf(mchLog, "\n%12d PP:%02o CH:%02o I/O while deselected",
        traceSequenceNo,
        activePpu->id,
        activeDevice->channel->id);
#endif
        return;
        }

    switch (opCode)
        {
    default:
#if DEBUG
        fprintf(mchLog, "\n%12d PP:%02o CH:%02o unrecognized op code: %X",
        traceSequenceNo,
        activePpu->id,
        activeDevice->channel->id,
        opCode);
#endif
        break;

    case FcOpRead:
        if (!mchLocationReady)
            {
            if (activeChannel->full)
                {
                activeChannel->full         = FALSE;
                activeDevice->recordLength -= 1;
                mchLocation = (mchLocation << 8) | (activeChannel->data & Mask8);
#if DEBUG
                if (mchBytesIo < 1)
                    {
                    fprintf(mchLog, "\n%12d PP:%02o CH:%02o >",
                    traceSequenceNo,
                    activePpu->id,
                    activeDevice->channel->id);
                    }
                fprintf(mchLog, " %02X", activeChannel->data);
                if (activeDevice->recordLength == 0)
                    {
                    fprintf(mchLog, " (%s)", mchCw2String(mchConnCode, mchTypeCode, mchLocation));
                    mchBytesIo = 0;
                    }
                else
                    {
                    mchBytesIo += 1;
                    }
#endif
                }
            }
        else
            {
            if (!activeChannel->full)
                {
                switch (modelType)
                    {
                case ModelCyber860:
                    switch (mchConnCode)
                        {
                    case 0:
                        activeChannel->data = mch860IouReader();
                        break;
                    case 1: // CP or CM
                        switch (mchTypeCode)
                            {
                        case 0:
                            activeChannel->data = mch860CpReader();
                            break;
                        case 1:
                            activeChannel->data = mch860CsReader();
                            break;
                        case 3:
                        case 4:
                        case 5:
                        case 6:
                            activeChannel->data = mch860SmReader();
                            break;
                        case 7:
                            activeChannel->data = mch860RfReader();
                            break;
                        case 0x0A:
                            activeChannel->data = mch860CmReader();
                            break;
                        default:
                            activeChannel->data = 0;
                            break;
                            }
                        break;
                    default:
                        activeChannel->data = 0;
                        break;
                        }
                    break;
                default:
                    activeChannel->data = 0;
                    break;
                    }

                activeChannel->full = TRUE;
#if DEBUG
                if ((mchBytesIo & 0x1f) == 0)
                    {
                    fprintf(mchLog, "\n%12d PP:%02o CH:%02o <",
                    traceSequenceNo,
                    activePpu->id,
                    activeDevice->channel->id);
                    }
                fprintf(mchLog, " %02X", activeChannel->data);
                mchBytesIo += 1;
#endif
                }
            }

        break;

    case FcOpWrite:
        if (!mchLocationReady)
            {
            if (activeChannel->full)
                {
                activeChannel->full         = FALSE;
                activeDevice->recordLength -= 1;
                mchLocation = (mchLocation << 8) | (activeChannel->data & Mask8);
#if DEBUG
                if (mchBytesIo < 1)
                    {
                    fprintf(mchLog, "\n%12d PP:%02o CH:%02o >",
                    traceSequenceNo,
                    activePpu->id,
                    activeDevice->channel->id);
                    }
                fprintf(mchLog, " %02X", activeChannel->data);
                if (activeDevice->recordLength == 0)
                    {
                    fprintf(mchLog, " (%s)", mchCw2String(mchConnCode, mchTypeCode, mchLocation));
                    mchBytesIo = 0;
                    }
                else
                    {
                    mchBytesIo += 1;
                    }
#endif
                }
            }
        else
            {
            if (activeChannel->full)
                {
#if DEBUG
                if ((mchBytesIo & 0x1f) == 0)
                    {
                    fprintf(mchLog, "\n%12d PP:%02o CH:%02o >",
                    traceSequenceNo,
                    activePpu->id,
                    activeDevice->channel->id);
                    }
                fprintf(mchLog, " %02X", activeChannel->data);
                mchBytesIo += 1;
#endif
                switch (modelType)
                    {
                case ModelCyber860:
                    switch (mchConnCode)
                        {
                    case 0:
                        mch860IouWriter(activeChannel->data);
                        break;
                    case 1: // CP or CM
                        switch (mchTypeCode)
                            {
                        case 0:
                            mch860CpWriter(activeChannel->data);
                            break;
                        case 1:
                            mch860CsWriter(activeChannel->data);
                            break;
                        case 3:
                        case 4:
                        case 5:
                        case 6:
                            mch860SmWriter(activeChannel->data);
                            break;
                        case 7:
                            mch860RfWriter(activeChannel->data);
                            break;
                        case 0x0A:
                            mch860CmWriter(activeChannel->data);
                            break;
                        default:
                            break;
                            }
                        break;
                    default:
                        break;
                        }
                    break;
                default:
                    break;
                    }

                activeChannel->full = FALSE;
                }
            }

        break;

    case FcOpEchoData:
        if (!mchLocationReady)
            {
            if (activeChannel->full)
                {
                activeChannel->full         = FALSE;
                activeDevice->recordLength -= 1;
                mchLocation = (mchLocation << 8) | (activeChannel->data & Mask8);
#if DEBUG
                fprintf(mchLog, " %02X", activeChannel->data);
                if (activeDevice->recordLength == 0)
                    {
                    fputs("\n <", mchLog);
                    }
#endif
                }
            }
        else
            {
            if (!activeChannel->full)
                {
                activeChannel->data = (PpWord)mchLocation;
                activeChannel->full = TRUE;
#if DEBUG
                fprintf(mchLog, " %02X", activeChannel->data);
#endif
                }
            }

        break;

    case FcOpStatusSummary:
        if (!activeChannel->full)
            {
            activeChannel->data = 0;
            activeChannel->full = TRUE;
            }

        break;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Determine whether a connect code represents a unit
**                  supported by this machine.
**
**  Parameters:     Name        Description.
**                  connCode    connect code
**
**  Returns:        TRUE if connect code supported
**
**------------------------------------------------------------------------*/
static bool mchIsConnected(u8 connCode)
    {
    switch (modelType)
        {
    case ModelCyber860:
        switch (connCode)
            {
        case 0:      // IOU
        case 1:      // CP or CM
            return TRUE;
        case 2:
            return cpuCount > 1;
        default:
            break;
            }
        break;
    default:
        break;
        }

    return FALSE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Determine whether a connect code and type code
**                  combination represents a CPU
**
**  Parameters:     Name        Description.
**                  connCode    connect code
**                  typeCode    type code
**
**  Returns:        TRUE if CPU
**
**------------------------------------------------------------------------*/
static bool mchIsCp(u8 connCode, u8 typeCode)
    {
    switch (modelType)
        {
    case ModelCyber860:
        if (typeCode == 0)
            {
            if (connCode == 1)
                {
                return TRUE;
                }
            else if (connCode == 2 && cpuCount > 1)
                {
                return TRUE;
                }
            }
        break;
    default:
        break;
        }

    return FALSE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Set a word into a maintenance register of the currently
**                  selected unit
**
**  Parameters:     Name        Description.
**                  reg         register address
**                  word        64-bit value to set
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mchSetRegister(u8 reg, u64 word)
    {
    switch (modelType)
        {
    case ModelCyber860:
        switch (mchConnCode)
            {
        case 0:
            mch860IouSetter(reg, word);
            break;
        case 1: // CP or CM
            switch (mchTypeCode)
                {
            case 0:
                mch860CpSetter(reg, word);
                break;
            case 0x0a:
                mch860CmSetter(reg, word);
                break;
            default:
                break;
                }
        default:
            break;
            }
    default:
        break;
        }
    }

/*
**  ---------------------------------
**  Model dependent utility functions
**  ---------------------------------
*/

/*--------------------------------------------------------------------------
**  Purpose:        Get a value from a Cyber 860 CM maintenance register
**
**  Parameters:     Name        Description.
**                  reg         the register address
**
**  Returns:        64-bit value.
**
**------------------------------------------------------------------------*/
static u64 mch860CmGetter(u8 reg)
    {
    if (reg == 0) // Status Summary
        {
        u64 byte;
        byte = mchCmRegisters[0] & 0xff;
        return (byte << 56) | (byte << 48) | (byte << 40) | (byte << 32)
             | (byte << 24) | (byte << 16) | (byte <<  8) | byte;
        }
    return mchCmRegisters[reg];
    }

/*--------------------------------------------------------------------------
**  Purpose:        Read a byte from a Cyber 860 CM maintenance register
**
**  Parameters:     Name        Description.
**
**  Returns:        next byte.
**
**------------------------------------------------------------------------*/
static u8 mch860CmReader(void)
    {
    u8 byte;

    if (activeDevice->recordLength == 8)
        {
        mchRegisterWord = mch860CmGetter(mchLocation);
        }
    activeDevice->recordLength -= 1;
    byte = (mchRegisterWord >> (activeDevice->recordLength * 8)) & 0xff;
    if (activeDevice->recordLength == 0)
        {
        activeDevice->recordLength = 8;
        }
    
    return byte;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Set a Cyber 860 CM maintenance register
**
**  Parameters:     Name        Description.
**                  reg         the register address
**                  word        the 64-bit value to set
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mch860CmSetter(u8 reg, u64 word)
    {
    mchCmRegisters[reg] = word;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Write a byte to a Cyber 860 CM maintenance register
**
**  Parameters:     Name        Description.
**                  byte        the byte to write
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mch860CmWriter(u8 byte)
    {
    if (activeDevice->recordLength == 8)
        {
        mchRegisterWord = 0;
        }
    mchRegisterWord = (mchRegisterWord << 8) | byte;
    activeDevice->recordLength -= 1;
    if (activeDevice->recordLength == 0)
        {
        mch860CmSetter(mchLocation, mchRegisterWord);
        activeDevice->recordLength = 8;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Get a value from a Cyber 860 CP maintenance register
**
**  Parameters:     Name        Description.
**                  reg         the register address
**
**  Returns:        64-bit value.
**
**------------------------------------------------------------------------*/
static u64 mch860CpGetter(u8 reg)
    {
    Cpu180Context *ctx;

    ctx = mchGetCpContext(mchConnCode);
    if (ctx == NULL)
        {
        return 0;
        }

    return mchGetCpRegister(ctx, reg);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Read a byte from a Cyber 860 CP maintenance register
**
**  Parameters:     Name        Description.
**
**  Returns:        next byte.
**
**------------------------------------------------------------------------*/
static u8 mch860CpReader(void)
    {
    u8 byte;

    if (activeDevice->recordLength == 8)
        {
        mchRegisterWord = mch860CpGetter(mchLocation);
        }
    activeDevice->recordLength -= 1;
    byte = (mchRegisterWord >> (activeDevice->recordLength * 8)) & 0xff;
    if (activeDevice->recordLength == 0)
        {
        activeDevice->recordLength = 8;
        }

    return byte;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Set a Cyber 860 CP maintenance register
**
**  Parameters:     Name        Description.
**                  reg         the register address
**                  word        the 64-bit value to set
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mch860CpSetter(u8 reg, u64 word)
    {
    Cpu180Context *ctx;

    ctx = mchGetCpContext(mchConnCode);
    if (ctx == NULL)
        {
        return;
        }
    mchSetCpRegister(ctx, reg, word);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Write a byte to a Cyber 860 CP maintenance register
**
**  Parameters:     Name        Description.
**                  byte        the byte to write
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mch860CpWriter(u8 byte)
    {
    if (activeDevice->recordLength == 8)
        {
        mchRegisterWord = 0;
        }
    mchRegisterWord = (mchRegisterWord << 8) | byte;
    activeDevice->recordLength -= 1;
    if (activeDevice->recordLength == 0)
        {
        mch860CpSetter(mchLocation, mchRegisterWord);
        activeDevice->recordLength = 8;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Read a byte from Cyber 860 Control Store
**
**  Parameters:     Name        Description.
**
**  Returns:        next byte.
**
**------------------------------------------------------------------------*/
static u8 mch860CsReader(void)
    {
    Cpu180Context *ctx;

    ctx = mchGetCpContext(mchConnCode);
    if (ctx != NULL)
        {
        return mchControlStores[ctx->id][mchControlStoreIndices[ctx->id]++];
        }
    else
        {
        return 0;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Write a byte to Cyber 860 Control Store
**
**  Parameters:     Name        Description.
**                  byte        the byte to write
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mch860CsWriter(u8 byte)
    {
    Cpu180Context *ctx;

    ctx = mchGetCpContext(mchConnCode);
    if (ctx != NULL)
        {
        mchControlStores[ctx->id][mchControlStoreIndices[ctx->id]++] = byte;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Initialize for model Cyber 860
**
**  Parameters:     Name        Description.
**                  iouOptions  IOU OI maintenance register value
**                  memSizeMask CM EC maintenance register value
**
**  Returns:        64-bit value.
**
**------------------------------------------------------------------------*/
static void mch860Init(u64 iouOptions, u64 memSizeMask)
    {
    u8 i;
    u8 j;
    u8 **softMemories;

    mchIouRegisters = (u64 *)calloc(256, 8);
    mch860IouSetter(RegIouElementId, 0x0000000002201234); // Elem: 02 (IOU), Model: 835-990, S/N
    mch860IouSetter(RegIouOptionsInstalled, iouOptions);

    mchCmRegisters  = (u64 *)calloc(256, 8);
    mch860CmSetter(RegMemElementId, 0x0000000001311234);  // Elem: 01 (CM),  Model: 850/860, S/N
    mch860CmSetter(RegMemOptionsInstalled, memSizeMask);

    mchTypeCode = 0;
    for (i = 0; i < cpuCount; i++)
        {
        mchConnCode               = (i == 0) ? 1 : 2;
        mchControlStores[i]       = (u8 *)calloc(2048, 16);
        mchControlStoreIndices[i] = 0;
        mchCpRegisterGroups[i]    = (u64 *)calloc(256, 8);
        mchRegisterFiles[i]       = (u64 *)calloc(64, 8);
        mchRegisterFileIndices[i] = 0;
        mchSoftMemoryGroups[i]    = (u8 **)calloc(7, sizeof(u8 *));
        softMemories              = mchSoftMemoryGroups[i];
        softMemories[3]           = (u8 *)calloc(1024, 4);
        softMemories[4]           = (u8 *)calloc(1024, 4);
        softMemories[5]           = (u8 *)calloc(2048, 4);
        softMemories[6]           = (u8 *)calloc(512, 4);
        mch860CpSetter(RegProcElementId, 0x0000000000321234); // Elem: 00 (CP),  Model: 860, S/N
        mch860CpSetter(RegProcVmCapabilityList, 0xc000);      // Virtual state and CYBER 170 state
        mch860CpSetter(RegProcStatusSummary, 0x28);           // CYBER 180 Monitor Mode, Processor Halt
        for (j = 0; j < 7; j++)
            {
            mchSoftMemoryIndices[i][j] = 0;
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Get a value from a Cyber 860 IOU maintenance register
**
**  Parameters:     Name        Description.
**                  reg         the register address
**
**  Returns:        64-bit value.
**
**------------------------------------------------------------------------*/
static u64 mch860IouGetter(u8 reg)
    {
    if (reg == 0) // Status Summary
        {
        u64 byte;
        byte = mchIouRegisters[0] & 0xff;
        return (byte << 56) | (byte << 48) | (byte << 40) | (byte << 32)
             | (byte << 24) | (byte << 16) | (byte <<  8) | byte;
        }
    return mchIouRegisters[reg];
    }

/*--------------------------------------------------------------------------
**  Purpose:        Read a byte from a Cyber 860 IOU maintenance register
**
**  Parameters:     Name        Description.
**
**  Returns:        next byte.
**
**------------------------------------------------------------------------*/
static u8 mch860IouReader(void)
    {
    u8 byte;

    if (activeDevice->recordLength == 8)
        {
        mchRegisterWord = mch860IouGetter(mchLocation);
        }
    activeDevice->recordLength -= 1;
    byte = (mchRegisterWord >> (activeDevice->recordLength * 8)) & 0xff;
    if (activeDevice->recordLength == 0)
        {
        activeDevice->recordLength = 8;
        }

    return byte;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Set a Cyber 860 IOU maintenance register
**
**  Parameters:     Name        Description.
**                  reg         the register address
**                  word        the 64-bit value to set
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mch860IouSetter(u8 reg, u64 word)
    {
    mchIouRegisters[reg] = word;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Write a byte to a Cyber 860 IOU maintenance register
**
**  Parameters:     Name        Description.
**                  byte        the byte to write
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mch860IouWriter(u8 byte)
    {
    int chIdx;
    int i;
    int ppIdx;
    u32 ppVector;

    if (activeDevice->recordLength == 8)
        {
        mchRegisterWord = 0;
        }
    mchRegisterWord = (mchRegisterWord << 8) | byte;
    activeDevice->recordLength -= 1;
    if (activeDevice->recordLength == 0)
        {
        mch860IouSetter(mchLocation, mchRegisterWord);
        activeDevice->recordLength = 8;
        if (mchLocation == RegIouEnvControl)
            {
#if DEBUG
            fputs("\n      Write IOU EC register", mchLog);
#endif
            ppIdx = (mchRegisterWord >> 24) & Mask5;
            if (ppIdx > 9)
                {
                ppIdx = (ppIdx - 0x10) + 10;
                }
            chIdx = (mchRegisterWord >> 16) & Mask5;
            ppu[ppIdx].osBoundsCheckEnabled = (mchRegisterWord & 0x08) != 0;
            ppu[ppIdx].isStopEnabled        = (mchRegisterWord & 0x01) != 0;
#if DEBUG
            fprintf(mchLog, "\n        PP%02o OS bounds check: %s", ppIdx < 10 ? ppIdx : (ppIdx - 10) + 020,
                ppu[ppIdx].osBoundsCheckEnabled ? "enabled" : "disabled");
            fprintf(mchLog, "\n                        stop: %s", ppu[ppIdx].isStopEnabled ? "enabled" : "disabled");
#endif
            if ((mchRegisterWord & 0x1000) != 0) // load PP
                {
                ppu[ppIdx].opD        = chIdx;
                channel[chIdx].active = TRUE;

                /*
                **  Set PP to INPUT (71) instruction.
                */
                ppu[ppIdx].opF  = 071;
                ppu[ppIdx].busy = TRUE;

                /*
                **  Clear P register and location zero of PP.
                */
                ppu[ppIdx].regP   = 0;
                ppu[ppIdx].mem[0] = 0;

                /*
                **  Set A register to an input word count of 10000.
                */
                ppu[ppIdx].regA = 010000;
#if DEBUG
                fprintf(mchLog, "\n        Deadstart PP%02o using channel %02o",
                    ppIdx < 10 ? ppIdx : (ppIdx - 10) + 020, chIdx);
#endif
                }
            }
        else if (mchLocation == RegIouOsBounds)
            {
            ppuOsBoundary = (mchRegisterWord & 0x3ffff) << 10;
            ppVector      = mchRegisterWord >> 32;
            for (i = 0; i < 10; i++)
                {
                ppu[i].isBelowOsBound = (ppVector & mchPpMasks[i]) != 0;
                }
            if (ppuCount > 10)
                {
                for (i = 10; i < 20; i++)
                    {
                    ppu[i].isBelowOsBound = (ppVector & mchPpMasks[i]) != 0;
                    }
                }
#if DEBUG
            fputs("\n      Write IOU OS bound register", mchLog);
            fprintf(mchLog, "\n        OS boundary: %010o", ppuOsBoundary);
            for (int i = 0; i < 10; i++)
                {
                fprintf(mchLog, "\n        PP%02o: %s", i, ppu[i].isBelowOsBound ? "below" : "above");
                }
            if (ppuCount > 10)
                {
                for (int i = 10; i < 20; i++)
                    {
                    fprintf(mchLog, "\n        PP%02o: %s", (i - 10) + 020, ppu[i].isBelowOsBound ? "below" : "above");
                    }
                }
#endif
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Read a byte from the Cyber 860 register file
**
**  Parameters:     Name        Description.
**
**  Returns:        next byte.
**
**------------------------------------------------------------------------*/
static u8 mch860RfReader(void)
    {
    Cpu180Context *ctx;

    ctx = mchGetCpContext(mchConnCode);
    if (ctx != NULL)
        {
        return mchRegisterFiles[ctx->id][mchRegisterFileIndices[ctx->id]++];
        }
    else
        {
        return 0;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Write a byte to the Cyber 860 register file
**
**  Parameters:     Name        Description.
**                  byte        the byte to write
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mch860RfWriter(u8 byte)
    {
    Cpu180Context *ctx;

    ctx = mchGetCpContext(mchConnCode);
    if (ctx != NULL)
        {
        mchRegisterFiles[ctx->id][mchRegisterFileIndices[ctx->id]++] = byte;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Read a byte from a Cyber 860 soft memory
**
**  Parameters:     Name        Description.
**
**  Returns:        next byte.
**
**------------------------------------------------------------------------*/
static u8 mch860SmReader(void)
    {
    Cpu180Context *ctx;
    u8            *mp;

    ctx = mchGetCpContext(mchConnCode);
    if (mchTypeCode < 7 && ctx != NULL)
        {
        mp = mchSoftMemoryGroups[ctx->id][mchTypeCode];
        if (mp != NULL)
            {
            return mp[mchSoftMemoryIndices[ctx->id][mchTypeCode]++];
            }
        }

    return 0;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Write a byte to a Cyber 860 soft memory
**
**  Parameters:     Name        Description.
**                  byte        the byte to write
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mch860SmWriter(u8 byte)
    {
    Cpu180Context *ctx;
    u8            *mp;

    ctx = mchGetCpContext(mchConnCode);
    if (mchTypeCode < 7 && ctx != NULL)
        {
        mp = mchSoftMemoryGroups[ctx->id][mchTypeCode];
        if (mp != NULL)
            {
            mp[mchSoftMemoryIndices[ctx->id][mchTypeCode]++] = byte;
            }
        }
    }

/*
**  -----------------
**   Debugging Aids
**  -----------------
*/

/*--------------------------------------------------------------------------
**  Purpose:        Convert location to string.
**
**  Parameters:     Name        Description.
**                  connCode    connection code
**                  typeCode    type code
**                  location    location code
**
**  Returns:        String equivalent of location.
**
**------------------------------------------------------------------------*/
static char *mchCw2String(u8 connCode, u8 typeCode, PpWord location)
    {
#if DEBUG
    switch (modelType)
        {
    case ModelCyber860:
        switch (connCode)
            {
        case 0:
            switch (location)
                {
            case 0x00:
                return "Status Summary";
            case 0x10:
                return "EID";
            case 0x12:
                return "OI";
            case 0x18:
                return "Fault Status Mask";
            case 0x21:
                return "OS Bounds";
            case 0x30:
                return "EC";
            case 0x40:
                return "Status";
            case 0x80:
                return "FS1";
            case 0x81:
                return "FS2";
            case 0xa0:
                return "TM";
            default:
                break;
                }
            break;
        case 1: // CP or CM
        case 2:
            switch (typeCode)
                {
            case 0:
                switch (location)
                    {
                case 0x00:
                    return "Status Summary";
                case 0x10:
                    return "EID";
                case 0x11:
                    return "Processor ID";
                case 0x12:
                    return "OI";
                case 0x13:
                    return "VMCL";
                case 0x30:
                    return "DEC";
                case 0x31:
                    return "Control Store Address";
                case 0x32:
                    return "Control Store Breakpoint";
                case 0x41:
                    return "Monitor Process State";
                case 0x48:
                    return "Page Table Address";
                case 0x49:
                    return "Page Table Length";
                case 0x4a:
                    return "Page Size Mask";
                case 0x51:
                    return "Model Dependent Word";
                case 0x61:
                    return "Job Process State";
                case 0x62:
                    return "System Interval Timer";
                case 0x80:
                    return "PFS0";
                case 0x81:
                    return "PFS1";
                case 0x82:
                    return "PFS2";
                case 0x83:
                    return "PFS3";
                case 0x84:
                    return "PFS4";
                case 0x85:
                    return "PFS5";
                case 0x86:
                    return "PFS6";
                case 0x87:
                    return "PFS7";
                case 0x88:
                    return "PFS8";
                case 0x89:
                    return "PFS9";
                case 0xa0:
                    return "PTM";
                default:
                    break;
                    }
                break;
            case 1:
                return "ignored";
            case 3:
            case 4:
            case 5:
            case 6:
            case 7:
                return "address";
            case 0x0A:
                switch (location)
                    {
                case 0x00:
                    return "Status Summary";
                case 0x10:
                    return "EID";
                case 0x12:
                    return "OI";
                case 0x20:
                    return "EC";
                case 0x21:
                    return "Bounds Register";
                case 0xa0:
                case 0xa1:
                case 0xa2:
                case 0xa3:
                    return "CEL";
                case 0xa4:
                case 0xa5:
                case 0xa6:
                case 0xa7:
                    return "UEL1";
                case 0xa8:
                case 0xa9:
                case 0xaa:
                case 0xab:
                    return "UEL2";
                case 0xb0:
                    return "Free Running Counter";
                default:
                    break;
                    }
                break;
            default:
                break;
                }
            break;
        default:
            break;
            }
        break;
    default:
        break;
        }
#endif

    return "Unknown";
    }

/*--------------------------------------------------------------------------
**  Purpose:        Convert channel function to string.
**
**  Parameters:     Name        Description.
**                  connCode    connection code
**                  opCode      operation code
**                  typeCode    type code
**
**  Returns:        String equivalent of function.
**
**------------------------------------------------------------------------*/
static char *mchFn2String(u8 connCode, u8 opCode, u8 typeCode)
    {
    static char buf[64];
    char *object;

#if DEBUG
    switch (modelType)
        {
    case ModelCyber860:
        switch (connCode)
            {
        case 0:
            object = "IOU";
            break;
        case 1: // CP or CM
            switch (typeCode)
                {
            case 0:
                object = "CP";
                break;
            case 1:
                object = "Control Store";
                break;
            case 3:
                object = "Reference ROM";
                break;
            case 4:
                object = "Soft control memories";
                break;
            case 5:
                object = "BDP control memory";
                break;
            case 6:
                object = "Instruction fetch decode memory";
                break;
            case 7:
                object = "Register file";
                break;
            case 0x0A:
                object = "CM";
                break;
            default:
                object = "Unknown type";
                break;
                }
            break;
        default:
            object = "Unknown unit";
            break;
            }
        break;
    default:
        object = "Unsupported machine type";
        break;
        }

    sprintf(buf, "%s %s", mchOp2String(opCode), object);
#endif

    return buf;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Convert operation code to string.
**
**  Parameters:     Name        Description.
**                  opCode      operation code
**
**  Returns:        String equivalent of operation code.
**
**------------------------------------------------------------------------*/
static char *mchOp2String(u8 opCode)
    {
    static char buf[16];

#if DEBUG
    switch (opCode)
        {
    case FcOpHalt:
        return "Halt";

    case FcOpStart:
        return "Start";

    case FcOpClearLed:
        return "ClearLed";

    case FcOpRead:
        return "Read";

    case FcOpWrite:
        return "Write";

    case FcOpMasterClear:
        return "MasterClear";

    case FcOpClearErrors:
        return "ClearErrors";

    case FcOpEchoData:
        return "EchoData";

    case FcOpStatusSummary:
        return "StatusSummary";
        }
#endif
    sprintf(buf, "Unknown 0x%X", opCode >> 4);

    return buf;
    }

/*---------------------------  End Of File  ------------------------------*/
