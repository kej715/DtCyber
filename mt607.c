/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**
**  Name: mt607.c
**
**  Description:
**      Perform emulation of CDC 6600 607 tape drives.
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
#include "const.h"
#include "types.h"
#include "proto.h"

/*
**  -----------------
**  Private Constants
**  -----------------
*/

/*
**  CDC 607 tape function and status codes.
**  
**  200x    Select
**  201x    Write Binary
**  202x    Read Binary
**  203x    Backspace
**  206x    Rewind
**  207x    Rewind/Unload
**  2100    Status request
**  221x    Write BCD
**  222x    Read BCD
**  261x    Write File Mark
**  
**  (x = unit = 0 - 7)
*/
#define Fc607UnitMask           07770

#define Fc607SelUnitCode        02000
#define Fc607WrBinary           02010
#define Fc607RdBinary           02020
#define Fc607Backspace          02030
#define Fc607Rewind             02060
#define Fc607RewindUnload       02070
#define Fc607StatusReq          02100
#define Fc607WrBCD              02210
#define Fc607RdBCD              02220
#define Fc607WrFileMark         02610

/*
**  
**  Status Reply:
**
**  0x00 = Ready
**  0x01 = Not Ready
**  0x02 = Parity Error
**  0x04 = Load Point
**  0x10 = End of Tape
**  0x20 = File Mark
**  0x40 = Write Lockout
**  
**  x = 0: 800 bpi
**  x = 1: 556 bpi
**  x = 2: 200 bpi
**  
*/
#define St607DensityMask        0700

#define St607Ready              0
#define St607NotReadyMask       001
#define St607ParityErrorMask    002
#define St607LoadPoint          004
#define St607EOT                010
#define St607FileMark           020
#define St607WriteLockout       040
/*
**  Misc constants.
*/
#define MaxPpBuf                010000
#define MaxByteBuf              014000

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
typedef struct tapeBuf
    {
    PpWord      ioBuffer[MaxPpBuf];
    PpWord      *bp;
    } TapeBuf;

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static FcStatus mt607Func(PpWord funcCode);
static void mt607Io(void);
static void mt607Activate(void);
static void mt607Disconnect(void);
static char *mt607Func2String(PpWord funcCode);

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
static u8 rawBuffer[MaxByteBuf];

#if DEBUG
static FILE *mt607Log = NULL;
#endif

/*
**--------------------------------------------------------------------------
**
**  Public Functions
**
**--------------------------------------------------------------------------
*/
/*--------------------------------------------------------------------------
**  Purpose:        Initialise 607 tape drives.
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
void mt607Init(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName)
    {
    DevSlot *dp;
    char fname[80];

    (void)eqNo;

#if DEBUG
    if (mt607Log == NULL)
        {
        mt607Log = fopen("mt607log.txt", "wt");
        }
#endif

    /*
    **  Attach device to channel.
    */
    dp = channelAttach(channelNo, eqNo, DtMt607);

    /*
    **  Setup channel functions.
    */
    dp->activate = mt607Activate;
    dp->disconnect = mt607Disconnect;
    dp->func = mt607Func;
    dp->io = mt607Io;
    dp->selectedUnit = unitNo;

    /*
    **  Setup controller context.
    */
    dp->context[unitNo] = calloc(1, sizeof(TapeBuf));
    if (dp->context[unitNo] == NULL)
        {
        fprintf(stderr, "Failed to allocate MT607 context block\n");
        exit(1);
        }

    /*
    **  Open the device file.
    */
    if (deviceName == NULL)
        {
        sprintf(fname, "MT607_C%02o_U%o.tap", channelNo, unitNo);
        }
    else
        {
        strcpy(fname, deviceName);
        }

    dp->fcb[unitNo] = fopen(fname, "rb");
    if (dp->fcb[unitNo] == NULL)
        {
        fprintf(stderr, "Failed to open %s\n", fname);
        exit(1);
        }

    /*
    **  Print a friendly message.
    */
    printf("MT607 initialised on channel %o unit %o)\n", channelNo, unitNo);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Execute function code on 607 tape drives.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        FcStatus
**
**------------------------------------------------------------------------*/
static FcStatus mt607Func(PpWord funcCode)
    {
    u32 len;
    u32 recLen0;
    u32 recLen1;
    u32 recLen2;
    u32 i;
    u16 c1, c2, c3;
    u16 *op;
    u8 *rp;
    TapeBuf *tp;

#if DEBUG
    fprintf(mt607Log, "\n%06d PP:%02o CH:%02o u:%d f:%04o T:%-25s  >   ",
        traceSequenceNo,
        activePpu->id,
        activeDevice->channel->id,
        activeDevice->selectedUnit, 
        funcCode,
        mt607Func2String(funcCode));
#endif

    switch (funcCode & Fc607UnitMask)
        {
    default:
#if DEBUG
        fprintf(mt607Log, " FUNC not implemented & declined!");
#endif
        return(FcDeclined);

    case Fc607WrBinary:
    case Fc607Backspace:
    case Fc607RewindUnload:
    case Fc607WrBCD:
    case Fc607RdBCD:
    case Fc607WrFileMark:
        activeDevice->fcode = 0;
        logError(LogErrorLocation, "channel %02o - unsupported function code: %04o", activeChannel->id, (u32)funcCode);
        break;

    case Fc607Rewind:
        activeDevice->fcode = 0;
        fseek(activeDevice->fcb[activeDevice->selectedUnit], 0, SEEK_SET);
        break;

    case Fc607StatusReq:
        activeDevice->fcode = funcCode;
        break;

    case Fc607SelUnitCode:
        activeDevice->fcode = 0;
        activeDevice->selectedUnit = funcCode & 07;
        if (activeDevice->fcb[activeDevice->selectedUnit] == NULL)
            {
            logError(LogErrorLocation, "channel %02o - invalid select: %04o", activeChannel->id, (u32)funcCode);
            }
        break;

    case Fc607RdBinary:
        activeDevice->fcode = funcCode;
        if (activeDevice->recordLength > 0)
            {
            activeChannel->status = St607Ready;
            break;
            }

        activeChannel->status = St607Ready;

        /*
        **  Reset tape buffer pointer.
        */
        tp = (TapeBuf *)activeDevice->context[activeDevice->selectedUnit];
        tp->bp = tp->ioBuffer;

        /*
        **  Read and verify TAP record length header.
        */
        len = fread(&recLen0, sizeof(recLen0), 1, activeDevice->fcb[activeDevice->selectedUnit]);

        if (len != 1)
            {
            activeChannel->status = St607EOT;
            activeDevice->recordLength = 0;
            break;
            }

        /*
        **  The TAP record length is little endian - convert if necessary.
        */
        if (bigEndian)
            {
            recLen1 = initConvertEndian(recLen0);
            }
        else
            {
            recLen1 = recLen0;
            }

        if (recLen1 == 0)
            {
            activeDevice->recordLength = 0;
#if DEBUG
            fprintf(mt607Log, "Tape mark\n");
#endif
            break;
            }

        if (recLen1 > MaxByteBuf)
            {
            logError(LogErrorLocation, "channel %02o - tape record too long: %d", activeChannel->id, recLen1);
            activeChannel->status = St607NotReadyMask;
            activeDevice->recordLength = 0;
            break;
            }

        /*
        **  Read and verify the actual raw data.
        */
        len = fread(rawBuffer, 1, recLen1, activeDevice->fcb[activeDevice->selectedUnit]);

        if (recLen1 != (u32)len)
            {
            logError(LogErrorLocation, "channel %02o - short tape record read: %d", activeChannel->id, len);
            activeChannel->status = St607NotReadyMask;
            activeDevice->recordLength = 0;
            break;
            }

        /*
        **  Read and verify the TAP record length trailer.
        */
        len = fread(&recLen2, sizeof(recLen2), 1, activeDevice->fcb[activeDevice->selectedUnit]);

        if (len != 1 || recLen0 != recLen2)
            {
            logError(LogErrorLocation, "channel %02o - invalid tape record trailer: %08x", activeChannel->id, recLen2);
            activeChannel->status = St607NotReadyMask;
            activeDevice->recordLength = 0;
            break;
            }

        /*
        **  Convert the raw data into PP words suitable for a channel.
        */
        op = tp->ioBuffer;
        rp = rawBuffer;

        for (i = 0; i < recLen1; i += 3)
            {
            c1 = *rp++;
            c2 = *rp++;
            c3 = *rp++;

            *op++ = ((c1 << 4) | (c2 >> 4)) & Mask12;
            *op++ = ((c2 << 8) | (c3 >> 0)) & Mask12;
            }

        activeDevice->recordLength = op - tp->ioBuffer;
        activeChannel->status = St607Ready;

#if DEBUG
        fprintf(mt607Log, "Read fwd %d PP words (%d 8-bit bytes)\n", activeDevice->recordLength, recLen1);
#endif
        break;
        }

    return(FcAccepted);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Perform I/O on MT607.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mt607Io(void)
    {
    TapeBuf *tp;

    switch (activeDevice->fcode & Fc607UnitMask)
        {
    default:
    case Fc607SelUnitCode:
    case Fc607WrBinary:
    case Fc607Backspace:
    case Fc607Rewind:
    case Fc607RewindUnload:
    case Fc607WrBCD:
    case Fc607RdBCD:
    case Fc607WrFileMark:
        logError(LogErrorLocation, "channel %02o - unsupported function code: %04o", activeChannel->id, activeDevice->fcode);
        break;

    case Fc607StatusReq:
        activeChannel->data = activeChannel->status;
        activeChannel->full = TRUE;
#if DEBUG
        fprintf(mt607Log, " %04o", activeChannel->data);
#endif
        break;

    case Fc607RdBinary:
        if (activeChannel->full)
            {
            break;
            }

        if (activeDevice->recordLength == 0)
            {
            activeChannel->active = FALSE;
            }

        tp = (TapeBuf *)activeDevice->context[activeDevice->selectedUnit];

        if (activeDevice->recordLength > 0)
            {
            activeDevice->recordLength -= 1;
            activeChannel->data = *tp->bp++;
            activeChannel->full = TRUE;
            if (activeDevice->recordLength == 0)
                {
//                activeChannel->discAfterInput = TRUE;  <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< fixed COS 
                }
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
static void mt607Activate(void)
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
static void mt607Disconnect(void)
    {
    /*
    **  Abort pending device disconnects - the PP is doing the disconnect.
    */
    activeChannel->discAfterInput = FALSE;
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
static char *mt607Func2String(PpWord funcCode)
    {
    static char buf[30];
#if DEBUG
    switch(funcCode)
        {
    case Fc607SelUnitCode            : return "Fc607SelUnitCode";
    case Fc607WrBinary               : return "Fc607WrBinary";
    case Fc607RdBinary               : return "Fc607RdBinary";
    case Fc607Backspace              : return "Fc607Backspace";
    case Fc607Rewind                 : return "Fc607Rewind";
    case Fc607RewindUnload           : return "Fc607RewindUnload";
    case Fc607StatusReq              : return "Fc607StatusReq";
    case Fc607WrBCD                  : return "Fc607WrBCD";
    case Fc607RdBCD                  : return "Fc607RdBCD";
    case Fc607WrFileMark             : return "Fc607WrFileMark";
        }
#endif
    sprintf(buf, "UNKNOWN: %04o", funcCode);
    return(buf);
    }

/*---------------------------  End Of File  ------------------------------*/
