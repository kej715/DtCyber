/*--------------------------------------------------------------------------
**
**  Copyright (c) 2019, Kevin Jordan, Tom Hunter,and Gerard van der Grinten
**
**  Name: dd885_42.c
**
**  Description:
**      Perform emulation of the CDC 885-42 disk drive and 7155-401 controller.
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
#include <time.h>
#include "const.h"
#include "types.h"
#include "proto.h"

/*
**  -----------------
**  Private Constants
**  -----------------
*/

#define DiskType885_42                     3

/*
**  CDC 885-42 disk drive function and status codes.
*/
#define Fc885_42Seek                       00001
#define Fc885_42Read                       00004
#define Fc885_42Write                      00005
#define Fc885_42OpComplete                 00010
#define Fc885_42GeneralStatus              00012
#define Fc885_42Continue                   00014
#define Fc885_42DetailedStatus             00023
#define Fc885_42ReadFactoryData            00030
#define Fc885_42ReadUtilityMap             00031
#define Fc885_42ReadProtectedSector        00034
#define Fc885_42ExtendedGeneralStatus      00066
#define Fc885_42InterlockAutoload          00067
#define Fc885_42Autoload                   00414

/*
**  General status bits.
*/
#define St885_42Abnormal                   04000
#define St885_42ControllerReserved         02000
#define St885_42NonrecoverableError        01000
#define St885_42Recovering                 00400
#define St885_42CheckwordError             00200
#define St885_42CorrectableAddressError    00100
#define St885_42DriveMalfunction           00020
#define St885_42DriveReservied             00010
#define St885_42AutoloadError              00004
#define St885_42Busy                       00002
#define St885_42ControllerRecovery         00001

/*
**  Detailed status.
*/

/*
**  Physical geometry of 885-42 disk.
**  256 words/sector + 2 12-bit control bytes
**   32 sectors/track
**   10 tracks/cylinder (heads/unit)
**  841 cylinders/unit
*/
#define MaxCylinders       843
#define MaxTracks          10
#define MaxSectors         32
#define SectorSize         256
#define ShortSectorSize    64

#define AutoloadSize       16870

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

typedef struct sector
    {
    PpWord control[2];
    CpWord data[SectorSize];
    } Sector;

typedef struct diskParam
    {
    /*
    **  Info for show_disk operator command.
    */
    struct diskParam *nextDisk;
    u8               channelNo;
    u8               eqNo;
    char             fileName[MaxFSPath];

    /*
    **  Parameter Table
    */
    i32              sector;
    i32              track;
    i32              cylinder;
    PpWord           generalStatus[5];
    PpWord           detailedStatus[20];
    u8               unitNo;
    PpWord           emAddress[2];
    PpWord           writeParams[4];
    Sector           buffer;
    } DiskParam;

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static FcStatus dd885_42Func(PpWord funcCode);
static void dd885_42Io(void);
static void dd885_42Activate(void);
static void dd885_42Disconnect(void);
static i32 dd885_42Seek(DiskParam *dp);
static i32 dd885_42SeekNext(DiskParam *dp);
static bool dd885_42Read(DiskParam *dp, FILE *fcb);
static bool dd885_42Write(DiskParam *dp, FILE *fcb);
static char * dd885_42Func2String(PpWord funcCode);

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
static DiskParam *firstDisk = NULL;
static DiskParam *lastDisk  = NULL;

#if DEBUG
#define OctalColumn(x)    (5 * (x) + 1 + 5)
#define AsciiColumn(x)    (OctalColumn(5) + 2 + (2 * x))
#define LogLineLength    (AsciiColumn(5))

static FILE *dd885_42Log = NULL;
static void dd885_42LogFlush(void);
static void dd885_42LogByte(int b);

static char dd885_42LogBuf[LogLineLength + 1];
static int  dd885_42LogCol = 0;

/*--------------------------------------------------------------------------
**  Purpose:        Flush incomplete numeric/ascii data line
**
**  Parameters:     Name        Description.
**
**  Returns:        nothing
**
**------------------------------------------------------------------------*/
static void dd885_42LogFlush(void)
    {
    if (dd885_42LogCol != 0)
        {
        fputs(dd885_42LogBuf, dd885_42Log);
        }

    dd885_42LogCol = 0;
    memset(dd885_42LogBuf, ' ', LogLineLength);
    dd885_42LogBuf[0]             = '\n';
    dd885_42LogBuf[LogLineLength] = '\0';
    }

/*--------------------------------------------------------------------------
**  Purpose:        Log a byte in octal/ascii form
**
**  Parameters:     Name        Description.
**
**  Returns:        nothing
**
**------------------------------------------------------------------------*/
static void dd885_42LogByte(int b)
    {
    char octal[10];
    int  col;

    col = OctalColumn(dd885_42LogCol);
    sprintf(octal, "%04o ", b);
    memcpy(dd885_42LogBuf + col, octal, 5);

    col = AsciiColumn(dd885_42LogCol);
    dd885_42LogBuf[col + 0] = cdcToAscii[(b >> 6) & Mask6];
    dd885_42LogBuf[col + 1] = cdcToAscii[(b >> 0) & Mask6];
    if (++dd885_42LogCol == 5)
        {
        dd885_42LogFlush();
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

/*
 **--------------------------------------------------------------------------
 **
 **  Private Functions
 **
 **--------------------------------------------------------------------------
 */

/*--------------------------------------------------------------------------
**  Purpose:        Initialise specified disk drive.
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
void dd885_42Init(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName)
    {
    DevSlot   *ds;
    FILE      *fcb;
    char      fname[MaxFSPath];
    DiskParam *dp;
    time_t    mTime;
    struct tm *lTime;
    u8        yy, mm, dd;

    char *opt = NULL;

    if (extMaxMemory == 0)
        {
        fprintf(stderr, "(dd885-42) Cannot configure 885-42 disk, no ECS configured\n");
        exit(1);
        }

#if DEBUG
    if (dd885_42Log == NULL)
        {
        dd885_42Log = fopen("dd885_42log.txt", "wt");
        if (dd885_42Log == NULL)
            {
            fprintf(stderr, "dd885_42log.txt - aborting\n");
            exit(1);
            }
        }
#endif

    /*
    **  Setup channel functions.
    */
    ds             = channelAttach(channelNo, eqNo, DtDd885_42);
    activeDevice   = ds;
    ds->activate   = dd885_42Activate;
    ds->disconnect = dd885_42Disconnect;
    ds->func       = dd885_42Func;
    ds->io         = dd885_42Io;

    /*
    **  Save disk parameters.
    */
    ds->selectedUnit = -1;
    dp = (DiskParam *)calloc(1, sizeof(DiskParam));
    if (dp == NULL)
        {
        fprintf(stderr, "(dd885-42) Failed to allocate dd885_42 context block\n");
        exit(1);
        }

    dp->unitNo = unitNo;

    /*
    **  Determine if any options have been specified.
    */
    if (deviceName != NULL)
        {
        opt = strchr(deviceName, ',');
        }

    if (opt != NULL)
        {
        /*
        **  Process options.
        */
        *opt++ = '\0';
        fprintf(stderr, "(dd885-42) Unrecognized option name %s\n", opt);
        exit(1);
        }

    /*
    **  Initialize detailed status.
    */
    dp->detailedStatus[0]  = 0;                   // strobe offset & address error status
    dp->detailedStatus[1]  = 0371;                // checkword error status, 885 sequence latches, motor at speed
    dp->detailedStatus[2]  = 0;                   // current function & error bits
    dp->detailedStatus[3]  = 07700 | unitNo;      // 7155 controlware, revision #, drive unit number
    dp->detailedStatus[4]  = 0;                   // track flaw, factory flaw map flaw, cylinder # bits 9-4
    dp->detailedStatus[5]  = 0;                   // cylinder # bits 3-0, track # bits 7-0
    dp->detailedStatus[6]  = 010;                 // logical sector number bits 7-0, ready&safe, system maint mode, drive # bits 7-6
    dp->detailedStatus[7]  = (unitNo << 6) | 037; // drive # bits 5-0, write enable, online, air switch, start switch, motor at speed
    dp->detailedStatus[8]  = 01640;               // DSU status
    dp->detailedStatus[9]  = 07201;               // DSU status
    dp->detailedStatus[10] = 0;                   // DSU status
    dp->detailedStatus[11] = 0;                   // DSU status
    dp->detailedStatus[12] = 0;                   // command causing error or word address of correctable read data
    dp->detailedStatus[13] = 0;                   // first word of correction vector
    dp->detailedStatus[14] = 0;                   // second word of correction vector
    dp->detailedStatus[15] = 0;                   // DSC operating status word
    dp->detailedStatus[16] = 0;                   // coupler buffer status
    dp->detailedStatus[17] = 0400;                // access A is connected & last function
    dp->detailedStatus[18] = 0;                   // 2nd and 3rd to last functions
    dp->detailedStatus[19] = 0;                   // 3rd to last function

    /*
    **  Link device parameters.
    */
    ds->context[unitNo] = dp;

    /*
    **  Link into list of disk units.
    */
    if (lastDisk == NULL)
        {
        firstDisk = dp;
        }
    else
        {
        lastDisk->nextDisk = dp;
        }

    lastDisk = dp;

    /*
    **  Open or create disk image.
    */
    if (deviceName == NULL)
        {
        /*
        **  Construct a name.
        */
        sprintf(fname, "DD885_42_C%02ou%1o", channelNo, unitNo);
        }
    else
        {
        strcpy(fname, deviceName);
        }

    /*
    **  Try to open existing disk image.
    */
    fcb = fopen(fname, "r+b");
    if (fcb == NULL)
        {
        /*
        **  Disk does not yet exist - manufacture one.
        */
        fcb = fopen(fname, "w+b");
        if (fcb == NULL)
            {
            fprintf(stderr, "(dd885-42) Failed to open %s\n", fname);
            exit(1);
            }

        /*
        **  Write last disk sector to reserve the space.
        */
        memset(&dp->buffer, 0, sizeof dp->buffer);
        dp->cylinder = MaxCylinders - 1;
        dp->track    = MaxTracks - 1;
        dp->sector   = MaxSectors - 1;
        fseek(fcb, dd885_42Seek(dp), SEEK_SET);
        fwrite(&dp->buffer, sizeof dp->buffer, 1, fcb);

        /*
        **  Position to cylinder with the disk's factory and utility
        **  data areas.
        */
        dp->cylinder = MaxCylinders - 2;

        /*
        **  Zero entire cylinder containing factory and utility data areas.
        */
        memset(&dp->buffer, 0, sizeof dp->buffer);
        for (dp->track = 0; dp->track < MaxTracks; dp->track++)
            {
            for (dp->sector = 0; dp->sector < MaxSectors; dp->sector++)
                {
                fseek(fcb, dd885_42Seek(dp), SEEK_SET);
                fwrite(&dp->buffer, sizeof dp->buffer, 1, fcb);
                }
            }

        /*
        **  Write serial number and date of manufacture.
        */
        dp->buffer.data[0] = (CpWord)(((channelNo & 070) << (8 - 3))
                                      | ((channelNo & 007) << (4 - 0))
                                      | ((unitNo & 070) >> (3 - 0))) << 48;
        dp->buffer.data[0] |= (CpWord)(((unitNo & 007) << (8 - 0))
                                       | ((DiskType885_42 & 070) << (4 - 3))
                                       | ((DiskType885_42 & 007) << (0 - 0))) << 36;

        mTime = getSeconds();
        lTime = localtime(&mTime);
        yy    = lTime->tm_year % 100;
        mm    = lTime->tm_mon + 1;
        dd    = lTime->tm_mday;

        dp->buffer.data[0] |= ((dd / 10) << 8 | (dd % 10) << 4 | mm / 10) << 24;
        dp->buffer.data[0] |= ((mm % 10) << 8 | (yy / 10) << 4 | yy % 10) << 12;

        dp->track  = 0;
        dp->sector = 0;
        fseek(fcb, dd885_42Seek(dp), SEEK_SET);
        fwrite(&dp->buffer, sizeof dp->buffer, 1, fcb);
        }

    ds->fcb[unitNo] = fcb;

    /*
    **  For Operator Show Status Command
    */
    strcpy(dp->fileName, fname);

    dp->channelNo = channelNo;
    dp->unitNo    = unitNo;

    /*
    **  Reset disk seek position.
    */
    dp->cylinder = 0;
    dp->track    = 0;
    dp->sector   = 0;
    fseek(fcb, dd885_42Seek(dp), SEEK_SET);

    /*
    **  Print a friendly message.
    */
    printf("(dd885-42) Disk with %d cylinders initialised on channel %o unit %o\n",
           MaxCylinders, channelNo, unitNo);
    }

/*
 **--------------------------------------------------------------------------
 **
 **  Private Functions
 **
 **--------------------------------------------------------------------------
 */

/*--------------------------------------------------------------------------
**  Purpose:        Execute function code on 885-42 disk drive.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        FcStatus
**
**------------------------------------------------------------------------*/
static FcStatus dd885_42Func(PpWord funcCode)
    {
    i8        unitNo;
    FILE      *fcb;
    int       ignore;
    DiskParam *dp;

    unitNo = activeDevice->selectedUnit;
    if (unitNo != -1)
        {
        dp  = (DiskParam *)activeDevice->context[unitNo];
        fcb = activeDevice->fcb[unitNo];
        }
    else
        {
        dp  = NULL;
        fcb = NULL;
        }

#if DEBUG
    dd885_42LogFlush();
    if (dp != NULL)
        {
        fprintf(dd885_42Log, "\n%06d PP:%02o CH:%02o f:%04o T:%-25s   c:%3d t:%2d s:%2d  >   ",
                traceSequenceNo,
                activePpu->id,
                activeDevice->channel->id,
                funcCode,
                dd885_42Func2String(funcCode),
                dp->cylinder,
                dp->track,
                dp->sector);
        }
    else
        {
        fprintf(dd885_42Log, "\n%06d PP:%02o CH:%02o DSK:? f:%04o T:%-25s  >   ",
                traceSequenceNo,
                activePpu->id,
                activeDevice->channel->id,
                funcCode,
                dd885_42Func2String(funcCode));
        }

    fflush(dd885_42Log);
#endif

    /*
    **  Catch functions which try to operate on not selected drives.
    */
    if (unitNo == -1)
        {
        switch (funcCode)
            {
        case Fc885_42Seek:
        case Fc885_42OpComplete:
        case Fc885_42GeneralStatus:
        case Fc885_42ExtendedGeneralStatus:
        case Fc885_42InterlockAutoload:
        case Fc885_42Autoload:
            /*
            **  These functions are OK - do nothing.
            */
            break;

        default:
            /*
            **  All remaining functions are declined if no drive is selected.
            */
#if DEBUG
            fprintf(dd885_42Log, " No drive selected, function %s declined ", dd885_42Func2String(funcCode));
#endif

            return (FcDeclined);
            }
        }

    /*
    **  Process function request.
    */
    switch (funcCode)
        {
    default:
#if DEBUG
        fprintf(dd885_42Log, " !!!!!FUNC %s not implemented & declined!!!!!! ", dd885_42Func2String(funcCode));
#endif

        return (FcDeclined);

    case Fc885_42Seek:
        /*
        **  Expect unit number, cylinder, track and sector.
        */
        activeDevice->recordLength = 4;
        break;

    case Fc885_42Read:
        /*
        **  Expect EM buffer address.
        */
        activeDevice->recordLength = 2;
        break;

    case Fc885_42Write:
        /*
        **  Expect EM buffer address, disk address, flags, and link byte
        */
        activeDevice->recordLength = 6;
        break;

    case Fc885_42OpComplete:
        return (FcProcessed);

    case Fc885_42GeneralStatus:
        activeDevice->recordLength = 1;
        break;

    case Fc885_42ExtendedGeneralStatus:
        /* TODO: Fix this. Define a static area for extended general status when no unit selected */
        if (dp)
            {
            dp->generalStatus[0] = activeDevice->status;
            }
        activeDevice->recordLength = 5;
        break;

    case Fc885_42DetailedStatus:
        dp->detailedStatus[2] = (funcCode << 4) & 07760;
        dp->detailedStatus[4] = (dp->cylinder >> 4) & 077;
        dp->detailedStatus[5] = ((dp->cylinder << 8) | dp->track) & 07777;
        dp->detailedStatus[6] = ((dp->sector << 4) | 010) & 07777;
        if ((dp->track & 1) != 0)
            {
            dp->detailedStatus[9] |= 2;              /* odd track */
            }
        else
            {
            dp->detailedStatus[9] &= ~2;
            }
        activeDevice->recordLength = 20;
        break;

    case Fc885_42Continue:
#if DEBUG
        fprintf(dd885_42Log, " !!!!!FUNC %s not implemented but accepted!!!!!! ", dd885_42Func2String(funcCode));
#endif
        logError(LogErrorLocation, "ch %o, function %s not implemented\n", activeChannel->id, dd885_42Func2String(funcCode));
        break;

    case Fc885_42InterlockAutoload:
    case Fc885_42Autoload:
        activeDevice->recordLength = AutoloadSize;
        break;

    case Fc885_42ReadFactoryData:
    case Fc885_42ReadUtilityMap:
    case Fc885_42ReadProtectedSector:
        ignore = fread(&dp->buffer, sizeof dp->buffer, 1, fcb);
        activeDevice->recordLength = ShortSectorSize * 5 + 2;
        break;
        }

    activeDevice->fcode = funcCode;

#if DEBUG
    fflush(dd885_42Log);
#endif

    return (FcAccepted);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Perform I/O on 885-42 disk drive.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void dd885_42Io(void)
    {
    u16       byteIndex;
    DiskParam *dp;
    FILE      *fcb;
    i32       pos;
    u16       shiftCount;
    i8        unitNo;
    u16       wordIndex;

    unitNo = activeDevice->selectedUnit;
    if (unitNo != -1)
        {
        dp  = (DiskParam *)activeDevice->context[unitNo];
        fcb = activeDevice->fcb[unitNo];
        }
    else
        {
        dp  = NULL;
        fcb = NULL;
        }

    switch (activeDevice->fcode)
        {
    case Fc885_42Seek:
        if (activeChannel->full)
            {
            switch (activeDevice->recordLength--)
            {
            case 4:
                unitNo = activeChannel->data & 07;
                if (unitNo != activeDevice->selectedUnit)
                    {
                    if (activeDevice->fcb[unitNo] != NULL)
                        {
                        activeDevice->selectedUnit = unitNo;
                        dp = (DiskParam *)activeDevice->context[unitNo];
                        dp->detailedStatus[12] &= ~01000;
                        }
                    else
                        {
                        logError(LogErrorLocation, "channel %02o - invalid select: %4.4o",
                                 activeChannel->id, (u32)activeDevice->fcode);
                        activeDevice->selectedUnit = -1;
                        }
                    }
                else
                    {
                    dp->detailedStatus[12] |= 01000;
                    }
                break;

            case 3:
                if (dp != NULL)
                    {
                    dp->cylinder = activeChannel->data;
                    }
                break;

            case 2:
                if (dp != NULL)
                    {
                    dp->track = activeChannel->data;
                    }
                break;

            case 1:
                if (dp != NULL)
                    {
                    dp->sector = activeChannel->data;
                    pos        = dd885_42Seek(dp);
                    if ((pos >= 0) && (fcb != NULL))
                        {
                        fseek(fcb, pos, SEEK_SET);
                        }
                    }
                else
                    {
                    activeDevice->status = 05020;
                    }
                break;

            default:
                activeDevice->recordLength = 0;
                break;
            }

#if DEBUG
            fprintf(dd885_42Log, " %04o[%d]", activeChannel->data, activeChannel->data);
#endif

            activeChannel->full = FALSE;
            }
        break;

    case Fc885_42Read:
        if (activeChannel->full)
            {
            if (dp != NULL)
                {
                switch (activeDevice->recordLength--)
                {
                case 2:
                    dp->emAddress[0] = activeChannel->data;
                    break;

                case 1:
                    dp->emAddress[1] = activeChannel->data;
                    if (dd885_42Read(dp, fcb))
                        {
                        pos = dd885_42SeekNext(dp);
                        if ((pos >= 0) && (fcb != NULL))
                            {
                            fseek(fcb, pos, SEEK_SET);
                            }
                        }
                    break;

                default:
                    activeDevice->recordLength = 0;
                    break;
                }
                }
#if DEBUG
            fprintf(dd885_42Log, " %04o", activeChannel->data);
#endif

            activeChannel->full = FALSE;
            }
        break;

    case Fc885_42Write:
        if (activeChannel->full)
            {
            if (dp != NULL)
                {
                switch (activeDevice->recordLength--)
                {
                case 6:
                    dp->emAddress[0] = activeChannel->data;
                    break;

                case 5:
                    dp->emAddress[1] = activeChannel->data;
                    break;

                case 4:
                    dp->writeParams[0] = activeChannel->data;
                    break;

                case 3:
                    dp->writeParams[1] = activeChannel->data;
                    break;

                case 2:
                    dp->writeParams[2] = activeChannel->data;
                    break;

                case 1:
                    dp->writeParams[3] = activeChannel->data;
                    if (dd885_42Write(dp, fcb))
                        {
                        pos = dd885_42SeekNext(dp);
                        if ((pos >= 0) && (fcb != NULL))
                            {
                            fseek(fcb, pos, SEEK_SET);
                            }
                        }
                    break;

                default:
                    activeDevice->recordLength = 0;
                    break;
                }
                }

#if DEBUG
            fprintf(dd885_42Log, " %04o", activeChannel->data);
#endif

            activeChannel->full = FALSE;
            }
        break;

    case Fc885_42GeneralStatus:
        if (!activeChannel->full)
            {
            activeChannel->data = activeDevice->status;
            activeChannel->full = TRUE;

#if DEBUG
            fprintf(dd885_42Log, " %04o", activeChannel->data);
#endif

            if (--activeDevice->recordLength == 0)
                {
                activeChannel->discAfterInput = TRUE;
                }
            }
        break;

    case Fc885_42ExtendedGeneralStatus:
        if (!activeChannel->full)
            {
            /* TODO: Fix this. Use a static area when no unit selected */
            if (dp)
                {
                activeChannel->data = dp->generalStatus[5 - activeDevice->recordLength];
                }
            else
                {
                activeChannel->data = 0;
                }
            activeChannel->full = TRUE;

#if DEBUG
            fprintf(dd885_42Log, " %04o", activeChannel->data);
#endif

            if (--activeDevice->recordLength == 0)
                {
                activeChannel->discAfterInput = TRUE;
                }
            }
        break;

    case Fc885_42DetailedStatus:
        if (!activeChannel->full)
            {
            activeChannel->data = dp->detailedStatus[20 - activeDevice->recordLength];
            activeChannel->full = TRUE;

#if DEBUG
            fprintf(dd885_42Log, " %04o", activeChannel->data);
#endif

            if (--activeDevice->recordLength == 0)
                {
                activeChannel->discAfterInput = TRUE;
                }
            }
        break;

    case Fc885_42InterlockAutoload:
    case Fc885_42Autoload:
#if DEBUG
        if (activeChannel->full)
            {
            dd885_42LogByte(activeChannel->data);
            if (--activeDevice->recordLength == 0)
                {
                dd885_42LogFlush();
                fprintf(dd885_42Log, "\n     Autoload complete: %d bytes loaded", AutoloadSize);
                }
            }
#endif
        activeChannel->full = FALSE;
        break;

    case Fc885_42OpComplete:
    case Fc885_42Continue:
        if (activeChannel->full)
            {
            activeChannel->full = FALSE;
#if DEBUG
            fprintf(dd885_42Log, " %04o", activeChannel->data);
#endif
            }
        break;

    case Fc885_42ReadFactoryData:
    case Fc885_42ReadUtilityMap:
    case Fc885_42ReadProtectedSector:
        if (!activeChannel->full)
            {
            if (activeDevice->recordLength > (ShortSectorSize * 5 + 1))
                {
                activeChannel->data = dp->buffer.control[0];
                --activeDevice->recordLength;
                }
            else if (activeDevice->recordLength > (ShortSectorSize * 5))
                {
                activeChannel->data = dp->buffer.control[1];
                --activeDevice->recordLength;
                }
            else
                {
                byteIndex           = (ShortSectorSize * 5) - activeDevice->recordLength;
                wordIndex           = byteIndex / 5;
                shiftCount          = 48 - (wordIndex * 12);
                activeChannel->data = (dp->buffer.data[wordIndex] >> shiftCount) & 07777;
                if (--activeDevice->recordLength == 0)
                    {
                    activeChannel->discAfterInput = TRUE;
                    }
                }
            activeChannel->full = TRUE;
#if DEBUG
            dd885_42LogByte(activeChannel->data);
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
static void dd885_42Activate(void)
    {
#if DEBUG
    fprintf(dd885_42Log, "\n%06d PP:%02o CH:%02o Activate",
            traceSequenceNo,
            activePpu->id,
            activeDevice->channel->id);

    fflush(dd885_42Log);
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
static void dd885_42Disconnect(void)
    {
    /*
    **  Abort pending device disconnects - the PP is doing the disconnect.
    */
    activeChannel->discAfterInput = FALSE;

#if DEBUG
    fprintf(dd885_42Log, "\n%06d PP:%02o CH:%02o Disconnect",
            traceSequenceNo,
            activePpu->id,
            activeDevice->channel->id);

    fflush(dd885_42Log);
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Work out seek offset.
**
**  Parameters:     Name        Description.
**                  dp          Disk parameters (context).
**
**  Returns:        Byte offset (not word!) or -1 when seek target
**                  is invalid.
**
**------------------------------------------------------------------------*/
static i32 dd885_42Seek(DiskParam *dp)
    {
    i32 result;

    activeDevice->status  = 0;
    dp->detailedStatus[2] = Fc885_42Seek << 4;

    if (dp->cylinder >= MaxCylinders)
        {
#if DEBUG
        fprintf(dd885_42Log, "ch %o, cylinder %d invalid\n", activeChannel->id, dp->cylinder);
#endif
        logError(LogErrorLocation, "ch %o, cylinder %d invalid\n", activeChannel->id, dp->cylinder);
        activeDevice->status   = St885_42Abnormal | St885_42NonrecoverableError;
        dp->detailedStatus[2] |= 0010;

        return (-1);
        }

    if (dp->track >= MaxTracks)
        {
#if DEBUG
        fprintf(dd885_42Log, "ch %o, track %d invalid\n", activeChannel->id, dp->track);
#endif
        logError(LogErrorLocation, "ch %o, track %d invalid\n", activeChannel->id, dp->track);
        activeDevice->status   = St885_42Abnormal | St885_42NonrecoverableError;
        dp->detailedStatus[2] |= 0010;

        return (-1);
        }

    if (dp->sector >= MaxSectors)
        {
#if DEBUG
        fprintf(dd885_42Log, "ch %o, sector %d invalid\n", activeChannel->id, dp->sector);
#endif
        logError(LogErrorLocation, "ch %o, sector %d invalid\n", activeChannel->id, dp->sector);
        activeDevice->status   = St885_42Abnormal | St885_42NonrecoverableError;
        dp->detailedStatus[2] |= 0010;

        return (-1);
        }

    dp->detailedStatus[4] = (PpWord)(dp->cylinder >> 4);
    dp->detailedStatus[5] = (PpWord)(((dp->cylinder & 0x0f) << 8) | dp->track);
    dp->detailedStatus[6] = (PpWord)((dp->sector << 4) | 0010);

    result  = dp->cylinder * MaxTracks * MaxSectors;
    result += dp->track * MaxSectors;
    result += dp->sector;
    result *= sizeof(Sector);

    return (result);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Advance to next sequential disk position.
**
**  Parameters:     Name        Description.
**                  dp          Disk parameters (context).
**
**  Returns:        Byte offset (not word!) or -1 when seek target
**                  is invalid.
**
**------------------------------------------------------------------------*/
static i32 dd885_42SeekNext(DiskParam *dp)
    {
    if (++dp->sector >= MaxSectors)
        {
        dp->sector = 0;
        if (++dp->track >= MaxTracks)
            {
            dp->track = 0;
            }
        }

    return dd885_42Seek(dp);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Perform a read operation from a disk container.
**
**  Parameters:     Name        Description.
**                  dp          Disk parameters (context).
**                  fcb         File control block.
**
**  Returns:        TRUE if successful
**
**------------------------------------------------------------------------*/
static bool dd885_42Read(DiskParam *dp, FILE *fcb)
    {
    CpWord *data;
    u32    emAddress;
    int    i;
    int    ignore;

    activeDevice->status  = 0;
    dp->detailedStatus[2] = Fc885_42Read << 4;

    ignore = fread(&dp->buffer, sizeof dp->buffer, 1, fcb);
    activeDevice->status = 0;
    dp->generalStatus[3] = dp->buffer.control[0];
    dp->generalStatus[4] = dp->buffer.control[1];
    emAddress            = (dp->emAddress[0] << 12) | dp->emAddress[1];
    data = dp->buffer.data;

#if DEBUG
    fprintf(dd885_42Log, "\n%06d PP:%02o CH:%02o f:%04o T:%-25s   c:%3d t:%2d s:%2d em:%08o >   ",
            traceSequenceNo,
            activePpu->id,
            activeDevice->channel->id,
            Fc885_42Read,
            dd885_42Func2String(Fc885_42Read),
            dp->cylinder,
            dp->track,
            dp->sector,
            emAddress);
#endif

    if (emAddress + SectorSize <= extMaxMemory)
        {
        for (i = 0; i < SectorSize; i++)
            {
            extMem[emAddress++] = *data++ & Mask60;
            }
        }
    else
        {
        logError(LogErrorLocation, "ch %o, ECS transfer from 885-42 rejected, address: %08o\n", activeChannel->id, emAddress);
        activeDevice->status   = St885_42Abnormal | St885_42NonrecoverableError;
        dp->detailedStatus[2] |= 0010;

        return FALSE;
        }

    return TRUE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Perform a write operation to a disk container.
**
**  Parameters:     Name        Description.
**                  dp          Disk parameters (context).
**                  fcb         File control block.
**
**  Returns:        TRUE if successful
**
**------------------------------------------------------------------------*/
static bool dd885_42Write(DiskParam *dp, FILE *fcb)
    {
    CpWord *data;
    u32    emAddress;
    int    i;

    activeDevice->status  = 0;
    dp->detailedStatus[2] = Fc885_42Write << 4;

    activeDevice->status  = 0;
    dp->buffer.control[0] = dp->writeParams[2];
    dp->buffer.control[1] = dp->writeParams[3];
    emAddress             = (dp->emAddress[0] << 12) | dp->emAddress[1];
    data = dp->buffer.data;

#if DEBUG
    fprintf(dd885_42Log, "\n%06d PP:%02o CH:%02o f:%04o T:%-25s   c:%3d t:%2d s:%2d em:%08o flags:%04o link:%04o >   ",
            traceSequenceNo,
            activePpu->id,
            activeDevice->channel->id,
            Fc885_42Write,
            dd885_42Func2String(Fc885_42Write),
            dp->cylinder,
            dp->track,
            dp->sector,
            emAddress,
            dp->buffer.control[0],
            dp->buffer.control[1]);
#endif

    if (emAddress + SectorSize <= extMaxMemory)
        {
        for (i = 0; i < SectorSize; i++)
            {
            *data++ = extMem[emAddress++] & Mask60;
            }
        }
    else
        {
        logError(LogErrorLocation, "ch %o, ECS transfer from 885-42 rejected, address: %08o\n", activeChannel->id, emAddress);
        activeDevice->status   = St885_42Abnormal | St885_42NonrecoverableError;
        dp->detailedStatus[2] |= 0010;

        return FALSE;
        }

    fwrite(&dp->buffer, sizeof dp->buffer, 1, fcb);

    return TRUE;
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
static char dd885_42FuncString[10];

static char *dd885_42Func2String(PpWord funcCode)
    {
    switch (funcCode)
        {
    case Fc885_42Seek:
        return "Seek";

    case Fc885_42Read:
        return "Read";

    case Fc885_42Write:
        return "Write";

    case Fc885_42OpComplete:
        return "OpComplete";

    case Fc885_42GeneralStatus:
        return "GeneralStatus";

    case Fc885_42DetailedStatus:
        return "DetailedStatus";

    case Fc885_42ReadFactoryData:
        return "ReadFactoryData";

    case Fc885_42ReadUtilityMap:
        return "ReadUtilityMap";

    case Fc885_42ReadProtectedSector:
        return "ReadProtectedSector";

    case Fc885_42Continue:
        return "Continue";

    case Fc885_42ExtendedGeneralStatus:
        return "ExtendedGeneralStatus";

    case Fc885_42InterlockAutoload:
        return "Autoload";

    case Fc885_42Autoload:
        return "Autoload";
        }
    sprintf(dd885_42FuncString, "%04o", funcCode);

    return dd885_42FuncString;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Show disk status (operator interface).
**
**  Parameters:     Name        Description.
**
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void dd885_42ShowDiskStatus()
    {
    DiskParam *dp = firstDisk;
    char      outBuf[MaxFSPath+128];

    if (dp == NULL)
        {
        return;
        }

    while (dp)
        {
        sprintf(outBuf, "    >   %-7s C%02o E%02o U%02o   %-20s (cyl 0x%06x trk 0x%06o)\n",
                "885-42",
                dp->channelNo,
                dp->eqNo,
                dp->unitNo,
                dp->fileName,
                dp->cylinder,
                dp->track);
        opDisplay(outBuf);
        dp = dp->nextDisk;
        }
    }

/*---------------------------  End Of File  ------------------------------*/
