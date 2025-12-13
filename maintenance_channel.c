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
    MacConnType_IOU, MacConnType_CP, MacConnType_CM, MacConnType_Unassigned
    } MacConnType;

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static char *mchCw2String(u8 connCode, u8 typeCode, PpWord location);
static FcStatus mchFunc(PpWord funcCode);
static MacConnType mchGetConnType(u8 connCode, u8 *cpId);
static void mchIo(void);
static void mchActivate(void);
static void mchDisconnect(void);
static bool mchIsConnected(u8 connCode);
static char *mchOp2String(u8 opCode);

#if DEBUG
static char *mchFn2String(u8 connCode, u8 opCode, u8 typeCode);
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
static u8   mchConnCode;
static u8   mchLocation;
static bool mchLocationReady;
static u8   mchTypeCode;

static u64  mchTimeout = 0;

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
    DevSlot *dp;

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

    /*
    **  Print a friendly message.
    */
    printf("(maintenance_channel) Initialised on channel %o\n", channelNo);
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
    u8 cpId;

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
            switch (mchGetConnType(mchConnCode, &cpId))
                {
            case MacConnType_IOU:
                ppMacSetIouLocation(mchLocation);
                break;
            case MacConnType_CP:
                cpu180MacSetCpLocation(&cpus180[cpId], mchTypeCode, mchLocation);
                break;
            case MacConnType_CM:
                cpu180MacSetCmLocation(mchLocation);
                break;
            default:
                break;
                }
            }
    default:
        break;
        }
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
    MacConnType connType;
    u8          cpId;
    u8          opCode;

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
        mchTimeout          = 0;
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
        connType = mchGetConnType(mchConnCode, &cpId);
        if (connType == MacConnType_CP)
            {
            cpu180MacHaltCp(&cpus180[cpId]);
            }
        return FcProcessed;

    case FcOpStart:
        connType = mchGetConnType(mchConnCode, &cpId);
        if (connType == MacConnType_CP)
            {
            cpu180MacStartCp(&cpus180[cpId]);
            }
        return FcProcessed;

    case FcOpMasterClear:
        connType = mchGetConnType(mchConnCode, &cpId);
        if (connType == MacConnType_CP)
            {
            cpu180MacMasterClearCp(&cpus180[cpId]);
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
**  Purpose:        Determine the type of connection represented by a
**                  connection code
**
**  Parameters:     Name        Description.
**                  connCode    connect code
**                  cpId       (out) CP (0 or 1) if connection type is CP
**
**  Returns:        connection type
**
**------------------------------------------------------------------------*/
static MacConnType mchGetConnType(u8 connCode, u8 *cpId)
    {
    switch (modelType)
        {
    case ModelCyber860:
        switch (connCode)
            {
        case 0:      // IOU
            return MacConnType_IOU;
        case 1:      // CP or CM
            *cpId = 0;
            return MacConnType_CP;
        case 3:
            if (cpuCount > 1)
                {
                *cpId = 1;
                return MacConnType_CP;
                }
            // fall through
        default:
            break;
            }
        break;
    default:
        break;
        }

    return MacConnType_Unassigned;
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
    MacConnType connType;
    u8          cpId;
    u8          opCode;

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
                connType = mchGetConnType(mchConnCode, &cpId);
                switch (connType)
                    {
                case MacConnType_IOU:
                    activeChannel->data = ppMacReadIou();
                    break;
                case MacConnType_CP:
                    activeChannel->data = cpu180MacReadCp(&cpus180[cpId], mchTypeCode);
                    break;
                case MacConnType_CM:
                    activeChannel->data = cpu180MacReadCm();
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
                connType = mchGetConnType(mchConnCode, &cpId);
                switch (connType)
                    {
                case MacConnType_IOU:
                    ppMacWriteIou((u8)(activeChannel->data & Mask8));
                    break;
                case MacConnType_CP:
                    cpu180MacWriteCp(&cpus180[cpId], mchTypeCode, (u8)(activeChannel->data & Mask8));
                    break;
                case MacConnType_CM:
                    cpu180MacWriteCm((u8)(activeChannel->data & Mask8));
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
        case 3:
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
        case 3:
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

#if DEBUG
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

    switch (modelType)
        {
    case ModelCyber860:
        switch (connCode)
            {
        case 0:
            object = "IOU";
            break;
        case 1: // CP or CM
        case 3: // CP or CM
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

    return buf;
    }
#endif

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
