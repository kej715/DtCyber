/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**
**  Name: dd6603.c
**
**  Description:
**      Perform emulation of CDC 6603 disk drives.
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
**  CDC 6603 disk drive function and status codes.
**  
**  10xx    Read sector xx (00-77)
**  11xx    Read sector xx (100-177)
**  12xx    Write sector xx (00-77)
**  13xx    Write sector xx (100-177)
**  14xx    Select tracks xx (00-77)
**  15xx    Select tracks xx (100-177)
**  16xy    Select head group y (x is read sampling time - ignore)
**  1700    Status request
*/
#define Fc6603CodeMask          07600
#define Fc6603SectMask          0177
#define Fc6603TrackMask         0177
#define Fc6603HeadMask          07

#define Fc6603ReadSector        01000
#define Fc6603WriteSector       01200
#define Fc6603SelectTrack       01400
#define Fc6603SelectHead        01600
#define Fc6603StatusReq         01700

/*
**  
**  Status Reply:
**
**  0xysSS
**  x=0     Ready
**  x=1     Not ready
**  y=0     No parity error
**  Y=1     Parity error
**  sSS     Sector number (bits 6-0)
*/
#define St6603StatusMask        07000
#define St6603StatusValue       00000
#define St6603SectMask          0177
#define St6603ParityErrorMask   0200
#define St6603ReadyMask         0400

/*
**  Physical dimensions of disk.
*/
#define MaxTracks               0200
#define MaxHeads                8
#define MaxOuterSectors         128
#define MaxInnerSectors         100
#define SectorSize              (322 + 16)


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
typedef struct diskParam
    {
    i32         sector;
    i32         track;
    i32         head;
    } DiskParam;

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static FcStatus dd6603Func(PpWord funcCode);
static void dd6603Io(void);
static void dd6603Activate(void);
static void dd6603Disconnect(void);
static i32 dd6603Seek(i32 track, i32 head, i32 sector);
static char *dd6603Func2String(PpWord funcCode);

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
static u8 logColumn;

#if DEBUG
static FILE *dd6603Log = NULL;
#endif

#if DEBUG
#define OctalColumn(x) (5 * (x) + 1 + 5)
#define AsciiColumn(x) (OctalColumn(5) + 2 + (2 * x))
#define LogLineLength (AsciiColumn(5))
#endif

#if DEBUG
static void dd6603LogFlush (void);
static void dd6603LogByte (int b);
#endif

#if DEBUG
static char dd6603LogBuf[LogLineLength + 1];
static int dd6603LogCol = 0;
#endif

#if DEBUG
/*--------------------------------------------------------------------------
**  Purpose:        Flush incomplete numeric/ascii data line
**
**  Parameters:     Name        Description.
**
**  Returns:        nothing
**
**------------------------------------------------------------------------*/
static void dd6603LogFlush(void)
    {
    if (dd6603LogCol != 0)
        {
        fputs(dd6603LogBuf, dd6603Log);
        }

    dd6603LogCol = 0;
    memset(dd6603LogBuf, ' ', LogLineLength);
    dd6603LogBuf[0] = '\n';
    dd6603LogBuf[LogLineLength] = '\0';
    }

/*--------------------------------------------------------------------------
**  Purpose:        Log a byte in octal/ascii form
**
**  Parameters:     Name        Description.
**
**  Returns:        nothing
**
**------------------------------------------------------------------------*/
static void dd6603LogByte(int b)
    {
    char octal[10];
    int col;
    
    col = OctalColumn(dd6603LogCol);
    sprintf(octal, "%04o ", b);
    memcpy(dd6603LogBuf + col, octal, 5);

    col = AsciiColumn(dd6603LogCol);
    dd6603LogBuf[col + 0] = cdcToAscii[(b >> 6) & Mask6];
    dd6603LogBuf[col + 1] = cdcToAscii[(b >> 0) & Mask6];
    if (++dd6603LogCol == 5)
        {
        dd6603LogFlush();
        }
    }
#endif

/*
**--------------------------------------------------------------------------
**
**  Public Functions
**
**--------------------------------------------------------------------------
*/

/*--------------------------------------------------------------------------
**  Purpose:        Initialise 6603 disk drive.
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
void dd6603Init(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName)
    {
    DevSlot *dp;
    FILE *fcb;
    char fname[80];

    (void)eqNo;
    (void)unitNo;
    (void)deviceName;

#if DEBUG
    if (dd6603Log == NULL)
        {
        dd6603Log = fopen("dd6603log.txt", "wt");
        }
#endif

    dp = channelAttach(channelNo, eqNo, DtDd6603);

    dp->activate = dd6603Activate;
    dp->disconnect = dd6603Disconnect;
    dp->func = dd6603Func;
    dp->io = dd6603Io;
    dp->selectedUnit = unitNo;

    dp->context[unitNo] = calloc(1, sizeof(DiskParam));
    if (dp->context[unitNo] == NULL)
        {
        fprintf(stderr, "Failed to allocate dd6603 context block\n");
        exit(1);
        }

    sprintf(fname, "DD6603_C%02oU%1o", channelNo,unitNo);
    fcb = fopen(fname, "r+b");
    if (fcb == NULL)
        {
        fcb = fopen(fname, "w+b");
        if (fcb == NULL)
            {
            fprintf(stderr, "Failed to open %s\n", fname);
            exit(1);
            }
        }

    dp->fcb[unitNo] = fcb;

    /*
    **  Print a friendly message.
    */
    printf("DD6603 initialised on channel %o unit %o \n", channelNo, unitNo);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Execute function code on 6603 disk drive.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        FcStatus
**
**------------------------------------------------------------------------*/
static FcStatus dd6603Func(PpWord funcCode)
    {
    FILE *fcb = activeDevice->fcb[activeDevice->selectedUnit];
    DiskParam *dp = (DiskParam *)activeDevice->context[activeDevice->selectedUnit];
    i32 pos;

#if DEBUG
    dd6603LogFlush();
    fprintf(dd6603Log, "\n%06d PP:%02o CH:%02o f:%04o T:%-25s  >   ",
        traceSequenceNo,
        activePpu->id,
        activeDevice->channel->id,
        funcCode,
        dd6603Func2String(funcCode));
#endif

    switch (funcCode & Fc6603CodeMask)
        {
    default:
        return(FcDeclined);

    case Fc6603ReadSector:
        activeDevice->fcode = funcCode;
        dp->sector = funcCode & Fc6603SectMask;
        pos = dd6603Seek(dp->track, dp->head, dp->sector);
        if (pos < 0)
            {
            return(FcDeclined);
            }
        fseek(fcb, pos, SEEK_SET);
        logColumn = 0;
        break;

    case Fc6603WriteSector:
        activeDevice->fcode = funcCode;
        dp->sector = funcCode & Fc6603SectMask;
        pos = dd6603Seek(dp->track, dp->head, dp->sector);
        if (pos < 0)
            {
            return(FcDeclined);
            }
        fseek(fcb, pos, SEEK_SET);
        logColumn = 0;
        break;

    case Fc6603SelectTrack:
        dp->track = funcCode & Fc6603TrackMask;
        return(FcProcessed);
        break;

    case Fc6603SelectHead:
        if (funcCode == Fc6603StatusReq)
            {
            activeDevice->fcode = funcCode;
            activeChannel->status = (u16)dp->sector;

            /*
            **  Simulate the moving disk - seems strange but is required.
            */
            dp->sector = (dp->sector + 1) & 0177;
            }
        else
            {
            /*
            **  Select head.
            */
            dp->head = funcCode & Fc6603HeadMask;
            return(FcProcessed);
            }
        break;
        }

    return(FcAccepted);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Perform I/O on 6603 disk drive.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void dd6603Io(void)
    {
    FILE *fcb = activeDevice->fcb[activeDevice->selectedUnit];
    DiskParam *dp = (DiskParam *)activeDevice->context[activeDevice->selectedUnit];

    switch (activeDevice->fcode & Fc6603CodeMask)
        {
    default:
        logError(LogErrorLocation, "channel %02o - invalid function code: %4.4o", activeChannel->id, (u32)activeDevice->fcode);
        break;

    case 0:
        break;

    case Fc6603ReadSector:
        if (!activeChannel->full)
            {
            fread(&activeChannel->data, 2, 1, fcb);
            activeChannel->full = TRUE;

#if DEBUG
            dd6603LogByte(activeChannel->data);
#endif
            }
        break;

    case Fc6603WriteSector:
        if (activeChannel->full)
            {
            fwrite(&activeChannel->data, 2, 1, fcb);
            activeChannel->full = FALSE;

#if DEBUG
            dd6603LogByte(activeChannel->data);
#endif
            }
        break;

    case Fc6603SelectTrack:
        break;

    case Fc6603SelectHead:
        if (activeDevice->fcode == Fc6603StatusReq)
            {
            activeChannel->data = activeChannel->status;
            activeChannel->full = TRUE;

#if 0
            activeChannel->status = (u16)dp->sector;

            /*
            **  Simulate the moving disk - seems strange but is required.
            */
            dp->sector = (dp->sector + 1) & 0177;
#else
            activeChannel->status = 0;
            activeDevice->fcode = 0;
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
static void dd6603Activate(void)
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
static void dd6603Disconnect(void)
    {
    }

/*--------------------------------------------------------------------------
**  Purpose:        Work out seek offset.
**
**  Parameters:     Name        Description.
**                  track       Track number.
**                  head        Head group.
**                  sector      Sector number.
**
**  Returns:        Byte offset (not word!) or -1 when seek target
**                  is invalid.
**
**------------------------------------------------------------------------*/
static i32 dd6603Seek(i32 track, i32 head, i32 sector)
    {
    i32 result;
    i32 sectorsPerTrack = MaxOuterSectors;

    if (track >= MaxTracks)
        {
        logError(LogErrorLocation, "ch %o, track %o invalid", activeChannel->id, track);
        return(-1);
        }

    if (head >= MaxHeads)
        {
        logError(LogErrorLocation, "ch %o, head %o invalid", activeChannel->id, head);
        return(-1);
        }

    if (sector >= sectorsPerTrack)
        {
        logError(LogErrorLocation, "ch %o, sector %o invalid", activeChannel->id, sector);
        return(-1);
        }

    result  = track * MaxHeads * sectorsPerTrack;
    result += head * sectorsPerTrack;
    result += sector;
    result *= SectorSize * 2;

    return(result);
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
static char *dd6603Func2String(PpWord funcCode)
    {
    static char buf[30];
#if DEBUG
    switch(funcCode & Fc6603CodeMask)
        {
    case Fc6603ReadSector             : return "Fc6603ReadSector";
    case Fc6603WriteSector            : return "Fc6603WriteSector";
    case Fc6603SelectTrack            : return "Fc6603SelectTrack";
    case Fc6603SelectHead             : return "Fc6603SelectHead";
    case Fc6603StatusReq              : return "Fc6603StatusReq";
        }
#endif
    sprintf(buf, "UNKNOWN: %04o", funcCode);
    return(buf);
    }

/*---------------------------  End Of File  ------------------------------*/
