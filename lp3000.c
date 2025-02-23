/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2010, Tom Hunter, Paul Koning
**            (c) 2017       Steven Zoppi 22-Oct-2017
**                           Added Ascii and ANSI support
**                           Added subdirectory support
**            (c) 2022       Kevin Jordan 02-Dec-2022
**                           Redesign to expose format effectors properly
**
**  Name: lp3000.c
**
**  Description:
**      Perform emulation of CDC 3000 series printers/controllers.
**
**      This combines the 501 and 512 printers, and the 3152 and 3555
**      controllers, because they all look pretty similar.
**
**      501 vs. 512 is selected by which Init function is called from
**      init.c via the device table; 3555 is the default but 3152/3256/3659
**      emulation can be selected by supplying a name string of "3152".
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
#include <errno.h>
#include "const.h"
#include "types.h"
#include "proto.h"
#include "dcc6681.h"

/*
**  -----------------
**  Private Constants
**  -----------------
*/

// General

// Flags stored in the context field:
#define Lp3000Type501           000001
#define Lp3000Type512           000002
#define Lp3000Type3152          000010
#define Lp3000Type3555          000020
#define Lp3555FillImageMem      000100
#define Lp3000IntReady          000200     // Same code as int status bit
#define Lp3000IntEnd            000400     // ditto
#define Lp3000IntReadyEna       002000
#define Lp3000IntEndEna         004000
#define Lp3000ExtArray          010000

/*
**      Function codes
*/

// Codes common to 3152/3256/3659 and 3555
#define FcPrintRelease           00000
#define FcPrintSingle            00001
#define FcPrintDouble            00002
#define FcPrintLastLine          00003
#define FcPrintEject             00004
#define FcPrintAutoEject         00005
#define FcPrintNoSpace           00006

// Codes for 3152/3256/3659
#define Fc3152ClearFormat        00010
#define Fc3152PostVFU1           00011
#define Fc3152PostVFU2           00012
#define Fc3152PostVFU3           00013
#define Fc3152PostVFU4           00014
#define Fc3152PostVFU5           00015
#define Fc3152PostVFU6           00016
#define Fc3152SelectPrePrint     00020
#define Fc3152PreVFU1            00021
#define Fc3152PreVFU2            00022
#define Fc3152PreVFU3            00023
#define Fc3152PreVFU4            00024
#define Fc3152PreVFU5            00025
#define Fc3152PreVFU6            00026
#define Fc3152SelIntReady        00030
#define Fc3152RelIntReady        00031
#define Fc3152SelIntEnd          00032
#define Fc3152RelIntEnd          00033
#define Fc3152SelIntError        00034
#define Fc3152RelIntError        00035
#define Fc3152Release2           00040

// Codes for 3555
#define Fc3555CondClearFormat    00007
#define Fc3555Sel8Lpi            00010
#define Fc3555Sel6Lpi            00011
#define Fc3555FillMemory         00012
#define Fc3555SelExtArray        00013
#define Fc3555ClearExtArray      00014
#define Fc3555SelIntReady        00020
#define Fc3555RelIntReady        00021
#define Fc3555SelIntEnd          00022
#define Fc3555RelIntEnd          00023
#define Fc3555SelIntError        00024
#define Fc3555RelIntError        00025
#define Fc3555ReloadMemEnable    00026
#define Fc3555ClearFormat        00030
#define Fc3555PostVFU1           00031
#define Fc3555PostVFU2           00032
#define Fc3555PostVFU3           00033
#define Fc3555PostVFU4           00034
#define Fc3555PostVFU5           00035
#define Fc3555PostVFU6           00036
#define Fc3555PostVFU7           00037
#define Fc3555PostVFU8           00040
#define Fc3555PostVFU9           00041
#define Fc3555PostVFU10          00042
#define Fc3555PostVFU11          00043
#define Fc3555PostVFU12          00044
#define Fc3555SelectPrePrint     00050
#define Fc3555PreVFU1            00051
#define Fc3555PreVFU2            00052
#define Fc3555PreVFU3            00053
#define Fc3555PreVFU4            00054
#define Fc3555PreVFU5            00055
#define Fc3555PreVFU6            00056
#define Fc3555PreVFU7            00057
#define Fc3555PreVFU8            00060
#define Fc3555PreVFU9            00061
#define Fc3555PreVFU10           00062
#define Fc3555PreVFU11           00063
#define Fc3555PreVFU12           00064
#define Fc3555MaintStatus        00065
#define Fc3555ClearMaint         00066

/*
**  Output rendering modes
*/
#define ModeCDC                  0
#define ModeANSI                 1
#define ModeASCII                2

/*
**  Maximum number of characters per line
*/
#define MaxLineSize              140

/*
**      Status reply
**
**  3152/3256/3659 vs. 3555 have different status codes for the
**  most part, but the few we care about are common:
**
*/
#define StPrintReady             00001
#define StPrintIntReady          00200
#define StPrintIntEnd            00400

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

typedef struct lpContext
    {
    /*
    **  Info for show_tape operator command.
    */
    struct lpContext *nextUnit;
    u8               channelNo;
    u8               eqNo;
    u8               unitNo;

    u16              flags;
    bool             isPrinted;
    bool             keepInt;
    u8               renderingMode;      //  one of ModeCDC, modeANSI, or mode ASCII

    u8               prePrintFunc;       //  last pre-print function (0 = no pre-print function specified)
    u8               postPrintFunc;      //  last post-print function (0 = no post-print function specified)
    bool             doAutoEject;        //  auto-eject pages
    bool             doSuppress;         //  suppress next post-print spacing op
    u8               lpi;                //  Lines Per Inch 6 or 8 (usually)
    PpWord           line[MaxLineSize];  //  buffered line
    u8               linePos;            //  current line position

    bool             doBurst;            //  bursting option for forced segmentation at EOJ
    char             path[MaxFSPath];    //  preserve the device folder path
    char             curFileName[MaxFSPath + 128];
    } LpContext;


/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static void     lp3000Init(u16 lpType, u8 eqNo, u8 unitNo, u8 channelNo, char *deviceParams);
static char    *lp3000FeForPostPrint(LpContext *lc, u8 func);
static char    *lp3000FeForPrePrint(LpContext *lc, u8 func);
static FcStatus lp3000Func(PpWord funcCode);
static void     lp3000Io(void);
static void     lp3000Activate(void);
static void     lp3000Disconnect(void);
static void     lp3000PrintANSI(LpContext *lc, FILE *fcb);
static void     lp3000PrintASCII(LpContext *lc, FILE *fcb);
static void     lp3000PrintCDC(LpContext *lc, FILE *fcb);

#if DEBUG
static void     lp3000DebugData(LpContext *lc);
static char    *lp3000Func2String(LpContext *lc, PpWord funcCode);

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

static LpContext *firstUnit = NULL;
static LpContext *lastUnit  = NULL;

static char *postPrintCdcEffectors[] =
    {
    "H", // advance to channel 1
    "G", // advance to channel 2
    "F", // advance to channel 3
    "E", // advance to channel 4
    "D", // advance to channel 5
    "C", // advance to channel 6
    "I", // advance to channel 7
    "J", // advance to channel 8
    "K", // advance to channel 9
    "L", // advance to channel 10
    "M", // advance to channel 11
    "N"  // advance to channel 12
    };

static char *prePrintAnsiEffectors[] =
    {
    "1", // advance to channel 1
    "2", // advance to channel 2
    "3", // advance to channel 3
    "4", // advance to channel 4
    "5", // advance to channel 5
    "6", // advance to channel 6
    "7", // advance to channel 7
    "8", // advance to channel 8
    "9", // advance to channel 9
    "A", // advance to channel 10
    "B", // advance to channel 11
    "C"  // advance to channel 12
    };

static char *prePrintCdcEffectors[] =
    {
    "8", // advance to channel 1
    "7", // advance to channel 2
    "6", // advance to channel 3
    "5", // advance to channel 4
    "4", // advance to channel 5
    "3", // advance to channel 6
    "9", // advance to channel 7
    "X", // advance to channel 8
    "Y", // advance to channel 9
    "Z", // advance to channel 10
    "W", // advance to channel 11
    "U"  // advance to channel 12
    };

static char *renderingModes[] =
    {
    "CDC",
    "ANSI",
    "ASCII"
    };

#if DEBUG
static FILE *lp3000Log = NULL;
#endif

/*
 **--------------------------------------------------------------------------
 **
 **  Public Functions
 **
 **--------------------------------------------------------------------------
 */

/*--------------------------------------------------------------------------
**  Purpose:        Initialise 501 line printer.
**
**  Parameters:     Name        Description.
**                  eqNo        equipment number
**                  unitNo      unit number
**                  channelNo   channel number the device is attached to
**                  deviceParams    Comma Delimited Parameters
**                      <devicePath> Path to Output Files
**                      <deviceType> "3152" to get 3152 controller
**                                   null or "3555" for 3555
**                      <deviceMode> "ansi" or "ascii" for output type
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void lp501Init(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceParams)
    {
    lp3000Init(Lp3000Type501, eqNo, unitNo, channelNo, deviceParams);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Initialise 512 line printer.
**
**  Parameters:     Name        Description.
**                  eqNo        equipment number
**                  unitNo      unit number
**                  channelNo   channel number the device is attached to
**                  deviceParams    Comma Delimited Parameters
**                      <devicePath> Path to Output Files
**                      <deviceType> "3152" to get 3152 controller
**                                   null or "3555" for 3555
**                      <deviceMode> "ansi" or "ascii" for output type
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void lp512Init(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceParams)
    {
    lp3000Init(Lp3000Type512, eqNo, unitNo, channelNo, deviceParams);
    }

/*
 **--------------------------------------------------------------------------
 **
 **  Private Functions
 **
 **--------------------------------------------------------------------------
 */

/*--------------------------------------------------------------------------
**  Purpose:        Initialise 3000 class line printer.
**
**  Parameters:     Name        Description.
**                  lpType      printer type (Lp3000Type501 or Lp3000Type512)
**                  eqNo        equipment number
**                  unitNo      unit number
**                  channelNo   channel number the device is attached to
**                  flags       Printer type flags
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void lp3000Init(u16 lpType, u8 eqNo, u8 unitNo, u8 channelNo, char *deviceParams)
    {
    char      *burstMode;
    char      *deviceMode;
    char      *devicePath;
    char      *deviceType;
    u16       flags;
    bool      isBursting;
    LpContext *lc;
    char      *lpTypeName;
    u8        mode;
    DevSlot   *up;

#if DEBUG
    if (lp3000Log == NULL)
        {
        lp3000Log = fopen("lp3000log.txt", "wt");
        }
#endif

    flags      = lpType;
    lpTypeName = (lpType == Lp3000Type501) ? "LP501" : "LP512",

    /*
    **  When we are called, "deviceParams" is a space terminated string
    **  at the end of the INI entry.
    **
    **  Tokenizing the remainder of the string as comma-delimited
    **  parameters gives us the configuration data that we need.
    **
    **  The format of the remainder of the line is:
    **
    **      <DeviceType>   (NULL(="3555")|"3555"|"3152")
    **      <devicePath>
    **      <OutputMode>   ("CDC"|"ANSI"|"ASCII")
    **      <BurstingMode> ("Burst"|"NoBurst")
    **
    */
    deviceType = strtok(deviceParams, ", "); //  "3555" | "3152"
    devicePath = strtok(NULL, ", ");         //  Get the Path (subdirectory)
    deviceMode = strtok(NULL, ", ");         //  pick up "cdc", "ansi", or "ascii"
    burstMode  = strtok(NULL, ", ");         //  Indication for bursting

    mode = ModeCDC;
    if (deviceMode != NULL)
        {
        if (strcasecmp(deviceMode, "cdc") == 0)
            {
            mode = ModeCDC;
            }
        else if (strcasecmp(deviceMode, "ansi") == 0)
            {
            mode = ModeANSI;
            }
        else if (strcasecmp(deviceMode, "ascii") == 0)
            {
            mode = ModeASCII;
            }
        else
            {
            logDtError(LogErrorLocation, "%s Unrecognized output rendering mode '%s'\n", lpTypeName, deviceMode);
            exit(1);
            }
        }
    fprintf(stdout, "(lp3000 ) %s Output rendering mode '%s'\n", lpTypeName, renderingModes[mode]);

    /*
    **  This is an optional parameter, therefore the default prevails
    **  lack of presence will not constitute failure.
    */
    isBursting = FALSE;
    if (burstMode != NULL)
        {
        if (strcasecmp(burstMode, "burst") == 0)
            {
            if ((strcasecmp(osType, "nos") == 0) || (strcasecmp(osType, "kronos") == 0))
                {
                isBursting = TRUE;
                }
            else
                {
                logDtError(LogErrorLocation, "%s WARNING: BURST mode ignored; applies only to NOS operating systems\n", lpTypeName);
                }
            }
        else if (strcasecmp(burstMode, "noburst") == 0)
            {
            isBursting = FALSE;
            }
        else
            {
            logDtError(LogErrorLocation, "%s Unrecognized BURST mode '%s'\n", lpTypeName, burstMode);
            exit(1);
            }
        }
    fprintf(stdout, "(lp3000 ) %s Burst mode '%s'\n", lpTypeName, isBursting ? "Bursting" : "Non-Bursting");

    if ((deviceType == NULL)
        || (strcmp(deviceType, "3555") == 0))
        {
        flags |= Lp3000Type3555;
        }
    else if (strcmp(deviceType, "3152") == 0)
        {
        flags |= Lp3000Type3152;
        }
    else
        {
        logDtError(LogErrorLocation, "Unrecognized %s controller type %s\n", lpTypeName, deviceType);
        exit(1);
        }

    up = dcc6681Attach(channelNo, eqNo, unitNo, DtLp5xx);

    up->activate   = lp3000Activate;
    up->disconnect = lp3000Disconnect;
    up->func       = lp3000Func;
    up->io         = lp3000Io;

    /*
    **  Only one printer unit is possible per equipment.
    */
    if (up->context[0] != NULL)
        {
        logDtError(LogErrorLocation, "Only one LP5xx unit is possible per equipment\n");
        exit(1);
        }

    lc = (LpContext *)calloc(1, sizeof(LpContext));
    if (lc == NULL)
        {
        logDtError(LogErrorLocation, "Failed to allocate LP5xx context block\n");
        exit(1);
        }

    up->context[0]    = (void *)lc;
    lc->flags         = flags;
    lc->lpi           = 6;               //  Default print density
    lc->linePos       = 0;
    lc->prePrintFunc  = 0;
    lc->postPrintFunc = 0;
    lc->doAutoEject   = FALSE;           //  Clear auto-eject
    lc->doSuppress    = FALSE;           //  Clear suppress space
    lc->doBurst       = isBursting;      //  Whether or not we force segmentation at EOJ
    lc->renderingMode = mode;            //  Indicate how to handle Carriage Control
    lc->channelNo     = channelNo;
    lc->unitNo        = unitNo;
    lc->eqNo          = eqNo;
    lc->path[0]       = '\0';

    /*
    **  Remember the device Path for future fopen calls
    */
    if (devicePath == NULL)
        {
        lc->path[0] = '\0';
        }
    else
        {
        strcpy(lc->path, devicePath);
        if (lc->path[0] != '\0')
            {
            strcat(lc->path, "/");
            }
        }

    /*
    **  Open the device file.
    */
    sprintf(lc->curFileName, "%sLP5xx_C%02o_E%o", lc->path, channelNo, eqNo);

    up->fcb[0] = fopen(lc->curFileName, "w");
    if (up->fcb[0] == NULL)
        {
        logDtError(LogErrorLocation, "Failed to open %s\n", lc->curFileName);
        exit(1);
        }

    /*
    **  Print a friendly message.
    */
    printf("(lp3000 ) LP%d/%d initialised on channel %o equipment %o mode %s filename '%s'\n",
           (flags & Lp3000Type3555) ? 3555 : 3152,
           (flags & Lp3000Type501) ? 501 : 512,
           channelNo, eqNo, renderingModes[mode], lc->curFileName);

    /*
    **  Link into list of Line Printer units.
    */
    if (lastUnit == NULL)
        {
        firstUnit = lc;
        }
    else
        {
        lastUnit->nextUnit = lc;
        }

    lastUnit = lc;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Show Line Printer Status (operator interface).
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void lp3000ShowStatus()
    {
    LpContext *lc;
    char      lpType[10];
    char      outBuf[MaxFSPath + 128];

    for (lc = firstUnit; lc != NULL; lc = lc->nextUnit)
        {
        sprintf(lpType, "%s/%s", (lc->flags & Lp3000Type3555) ? "3555" : "3152", (lc->flags & Lp3000Type501) ? "501" : "512");
        sprintf(outBuf, "    >   %-8s C%02o E%02o U%02o", lpType, lc->channelNo, lc->eqNo, lc->unitNo);
        opDisplay(outBuf);
        sprintf(outBuf, "   %-20s (mode ", lc->curFileName);
        opDisplay(outBuf);
        opDisplay(renderingModes[lc->renderingMode]);
        sprintf(outBuf, ", %d lpi", lc->lpi);
        opDisplay(outBuf);
        if (lc->doBurst)
            {
            opDisplay(", burst");
            }
        opDisplay(")\n");
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Remove the paper (operator interface).
**
**  Parameters:     Name        Description.
**                  params      parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void lp3000RemovePaper(char *params)
    {
    int       channelNo;
    time_t    currentTime;
    DevSlot   *dp;
    int       equipmentNo;
    char      fNameNew[MaxFSPath + 128];
    int       iSuffix;
    LpContext *lc;
    int       numParam;
    char      outBuf[MaxFSPath * 2 + 300];
    bool      renameOK;
    struct tm t;

    /*
    **  Operator wants to remove paper.
    */
    numParam = sscanf(params, "%o,%o,%s", &channelNo, &equipmentNo, fNameNew);

    /*
    **  Check parameters.
    */
    if (numParam < 2)
        {
        opDisplay("(lp3000 ) Not enough or invalid parameters\n");

        return;
        }

    if ((channelNo < 0) || (channelNo >= MaxChannels))
        {
        opDisplay("(lp3000 ) Invalid channel no\n");

        return;
        }

    if ((equipmentNo < 0) || (equipmentNo >= MaxEquipment))
        {
        opDisplay("(lp3000 ) Invalid equipment no\n");

        return;
        }

    /*
    **  Locate the device control block.
    */
    dp = dcc6681FindDevice((u8)channelNo, (u8)equipmentNo, DtLp5xx);
    if (dp == NULL)
        {
        sprintf(outBuf, "(lp3000 ) No printer on channel %o and equipment %o\n", channelNo, equipmentNo);
        opDisplay(outBuf);

        return;
        }

    lc       = (LpContext *)dp->context[0];
    renameOK = FALSE;

    //
    //  This can happen if something goes wrong in the open and the file fails
    //  to be properly re-opened.
    //
    if (dp->fcb[0] == NULL)
        {
        logDtError(LogErrorLocation, "lp3000RemovePaper: FCB is null on channel %o equipment %o\n",
                   dp->channel->id,
                   dp->eqNo);
        //  proceed to attempt to open a new FCB
        }
    else
        {
        fflush(dp->fcb[0]);

        if (ftell(dp->fcb[0]) == 0)
            {
            sprintf(outBuf, "(lp3000 ) No output has been written on channel %o and equipment %o\n", channelNo, equipmentNo);
            opDisplay(outBuf);

            return;
            }

        /*
        **  Close the old device file.
        */
        fclose(dp->fcb[0]);
        dp->fcb[0] = NULL;

        if (numParam > 2)
            {
            if (rename(lc->curFileName, fNameNew) == 0)
                {
                renameOK = TRUE;
                }
            else
                {
                sprintf(outBuf, "(lp3000 ) Rename Failure '%s' to '%s' - (%s).\n", lc->curFileName, fNameNew, strerror(errno));
                opDisplay(outBuf);
                }
            }
        else
            {
            /*
            **  Rename the device file to the format "LP5xx_yyyymmdd_hhmmss_nn.txt".
            */
            for (iSuffix = 0; iSuffix < 100; iSuffix++)
                {
                time(&currentTime);
                t = *localtime(&currentTime);
                sprintf(fNameNew, "%sLP5xx_%04d%02d%02d_%02d%02d%02d_%02d.txt",
                        lc->path,
                        t.tm_year + 1900,
                        t.tm_mon + 1,
                        t.tm_mday,
                        t.tm_hour,
                        t.tm_min,
                        t.tm_sec,
                        iSuffix);

                if (rename(lc->curFileName, fNameNew) == 0)
                    {
                    renameOK = TRUE;
                    break;
                    }
                logDtError(LogErrorLocation, "Rename Failure '%s' to '%s' - (%s). Retrying (%d)...\n",
                           lc->curFileName,
                           fNameNew,
                           strerror(errno),
                           iSuffix);
                }
            }
        }

    /*
    **  Open the device file.
    */

    //  Just append to the old file if the rename didn't happen correctly
    dp->fcb[0] = fopen(lc->curFileName, renameOK ? "w" : "a");

    /*
    **  Check if the open succeeded.
    */
    if (dp->fcb[0] == NULL)
        {
        logDtError(LogErrorLocation, "Failed to open %s\n", lc->curFileName);

        return;
        }
    if (renameOK)
        {
        sprintf(outBuf, "(lp3000 ) Paper removed from 5xx printer and available on '%s'\n", fNameNew);
        opDisplay(outBuf);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Execute function code on 3000 line printer.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        FcStatus
**
**------------------------------------------------------------------------*/
static FcStatus lp3000Func(PpWord funcCode)
    {
    FILE      *fcb;
    LpContext *lc;

    char         dispLpDevId[16];       //  Used for automatically removing printouts at EOJ
    unsigned int channelId;
    unsigned int deviceId;

    fcb = active3000Device->fcb[0];
    lc  = (LpContext *)active3000Device->context[0];

    /*
    **  This can happen if something goes wrong in the open and the file fails
    **  to be properly re-opened.
    */
    if (fcb == NULL)
        {
        logDtError(LogErrorLocation, "lp3000Func: FCB is null on channel %o equipment %o\n",
                   active3000Device->channel->id,
                   active3000Device->eqNo);

        return FcProcessed;
        }

#if DEBUG
    fprintf(lp3000Log, "\n%06d PP:%02o CH:%02o f:%04o T:%-25s  >   ",
            traceSequenceNo,
            activePpu->id,
            activeDevice->channel->id,
            funcCode,
            lp3000Func2String(lc, funcCode));
#endif

    //  Start with the common codes

    switch (funcCode)
        {
    case FcPrintNoSpace:
        lc->doSuppress = TRUE;

        return FcProcessed;

    case FcPrintAutoEject:
        if ((lc->renderingMode != ModeASCII) && (lc->doAutoEject == FALSE))
            {
            fputs("R\n", fcb);
            }
        lc->doAutoEject = TRUE;

        return FcProcessed;

    case Fc6681MasterClear:
        lc->lpi           = 6;
        lc->linePos       = 0;
        lc->prePrintFunc  = 0;
        lc->postPrintFunc = 0;
        lc->doAutoEject   = FALSE;
        lc->doSuppress    = FALSE;
        lc->flags        &= ~Lp3000ExtArray;

        return FcProcessed;

    case FcPrintRelease:
        // clear all interrupt conditions
        lc->flags &= ~(StPrintIntReady | StPrintIntEnd);
        fflush(fcb);

        // Release is sent at end of job, so flush the print file
        if (lc->isPrinted && lc->doBurst)
            {
            channelId = (int)active3000Device->channel->id;
            deviceId  = (int)active3000Device->eqNo;
            sprintf(dispLpDevId, "%o,%o", channelId, deviceId);
            lp3000RemovePaper(dispLpDevId);
            }
        lc->isPrinted = FALSE;

        return FcProcessed;

    case FcPrintSingle:
    case FcPrintDouble:
    case FcPrintLastLine:
    case FcPrintEject:
        if ((lc->prePrintFunc != 0) && (lc->prePrintFunc != FcPrintNoSpace))
            {
            fputs(lp3000FeForPrePrint(lc, lc->prePrintFunc), fcb);
            fputc('\n', fcb);
            }
        lc->prePrintFunc = (u8)funcCode;

        return FcProcessed;

    case Fc6681Output:
        if (lc->flags & Lp3555FillImageMem)
            {
            // Tweak the function code to tell I/O handler to toss this data
            funcCode += 1;
            // Now clear the flag
            lc->flags &= ~Lp3555FillImageMem;
            }
        // Initially clear interrupt status flags
        lc->flags &= ~(StPrintIntReady | StPrintIntEnd);

        // Update interrupt status to reflect what status
        // will be when this transfer is finished.
        // Ok, so that's cheating a bit...
        if (lc->flags & Lp3000IntReadyEna)
            {
            lc->flags |= Lp3000IntReady;
            }
        if (lc->flags & Lp3000IntEndEna)
            {
            lc->flags |= Lp3000IntEnd;
            }

        // Update interrupt summary flag in unit block
        dcc6681Interrupt((lc->flags & (Lp3000IntReady | Lp3000IntEnd)) != 0);
        active3000Device->fcode = funcCode;

        return FcAccepted;

    case Fc6681DevStatusReq:
        active3000Device->fcode = funcCode;

        return FcAccepted;
        }

    if (lc->flags & Lp3000Type3555)             // This is LP3555
        {
        switch (funcCode)
            {
        default:
            logDtError(LogErrorLocation, "Unknown LP3555 function %04o\n", funcCode);

            return FcDeclined;

        case Fc3555Sel8Lpi:
            if ((lc->renderingMode != ModeASCII) && (lc->lpi != 8))
                {
                fputs("T\n", fcb);
                }
            lc->lpi = 8;

            return FcProcessed;

        case Fc3555Sel6Lpi:
            if ((lc->renderingMode != ModeASCII) && (lc->lpi != 6))
                {
                fputs("S\n", fcb);
                }
            lc->lpi = 6;

            return FcProcessed;

        case Fc3555ClearFormat:
            if ((lc->renderingMode != ModeASCII)
                && ((lc->lpi != 6) || lc->doAutoEject))
                {
                fputs("Q\n", fcb);
                }

        case Fc3555CondClearFormat:
            lc->postPrintFunc = 0;
            lc->lpi           = 6;
            lc->doAutoEject   = FALSE;
            lc->doSuppress    = FALSE;

            return FcProcessed;

        case Fc3555PostVFU1:
        case Fc3555PostVFU2:
        case Fc3555PostVFU3:
        case Fc3555PostVFU4:
        case Fc3555PostVFU5:
        case Fc3555PostVFU6:
        case Fc3555PostVFU7:
        case Fc3555PostVFU8:
        case Fc3555PostVFU9:
        case Fc3555PostVFU10:
        case Fc3555PostVFU11:
        case Fc3555PostVFU12:
            lc->postPrintFunc = (u8)funcCode;

            return FcProcessed;

        case Fc3555SelectPrePrint:
            lc->postPrintFunc = 0;

            return FcProcessed;

        case Fc3555PreVFU1:
        case Fc3555PreVFU2:
        case Fc3555PreVFU3:
        case Fc3555PreVFU4:
        case Fc3555PreVFU5:
        case Fc3555PreVFU6:
        case Fc3555PreVFU7:
        case Fc3555PreVFU8:
        case Fc3555PreVFU9:
        case Fc3555PreVFU10:
        case Fc3555PreVFU11:
        case Fc3555PreVFU12:
            lc->prePrintFunc = (u8)funcCode;

            return FcProcessed;

        case Fc3555FillMemory:
            // Remember that we saw this function, but this doesn't actually
            // start any I/O yet.
            lc->flags |= Lp3555FillImageMem;

            return FcProcessed;

        case Fc3555SelIntReady:
            // Enable next int.  If an I/O was done since the last
            // time an int enable was issued, don't clear the
            // current int.  That's because things go very slowly
            // otherwise; printer drivers typically issue the write,
            // then enable the int shortly after.  We've already set
            // "ready" by then, unlike physical printers.
            lc->flags |= Lp3000IntReady | Lp3000IntReadyEna;
            if (lc->keepInt)
                {
                lc->keepInt = FALSE;
                }
            else
                {
                lc->flags &= ~Lp3000IntReady;
                }
            // Update interrupt summary flag in unit block
            dcc6681Interrupt((lc->flags & (Lp3000IntReady | Lp3000IntEnd)) != 0);

            return FcProcessed;

        case Fc3555RelIntReady:
            lc->flags &= ~(Lp3000IntReadyEna | Lp3000IntReady);
            // Update interrupt summary flag in unit block
            dcc6681Interrupt((lc->flags & (Lp3000IntReady | Lp3000IntEnd)) != 0);

            return FcProcessed;

        case Fc3555SelIntEnd:
            lc->flags |= Lp3000IntEnd | Lp3000IntEndEna;
            if (lc->keepInt)
                {
                lc->keepInt = FALSE;
                }
            else
                {
                lc->flags &= ~Lp3000IntEnd;
                }
            // Update interrupt summary flag in unit block
            dcc6681Interrupt((lc->flags & (Lp3000IntReady | Lp3000IntEnd)) != 0);

            return FcProcessed;

        case Fc3555RelIntEnd:
            lc->flags &= ~(Lp3000IntEndEna | Lp3000IntEnd);
            // Update interrupt summary flag in unit block
            dcc6681Interrupt((lc->flags & (Lp3000IntReady | Lp3000IntEnd)) != 0);

            return FcProcessed;

        case Fc3555SelExtArray:
            lc->flags |= Lp3000ExtArray;

            return FcProcessed;

        case Fc3555ClearExtArray:
            lc->flags &= ~Lp3000ExtArray;

            return FcProcessed;

        case Fc3555SelIntError:
        case Fc3555RelIntError:
        case Fc3555ReloadMemEnable:
        case Fc3555MaintStatus:
        case Fc3555ClearMaint:
            // All of the above are no-ops
            return FcProcessed;
            }
        }
    else        // This is LP3152
        {
        switch (funcCode)
            {
        default:
            //
            // 1IO in KRONOS and NOS issues Fc3555SelectPrePrint to test whether the
            // controller is 3152 or 3555, so avoid cluttering the console with messages
            // due to this "normal" behavior.
            //
            if (funcCode != Fc3555SelectPrePrint)
                {
                logDtError(LogErrorLocation, "Unknown LP3152 function %04o\n", funcCode);
                }

            return FcDeclined;

        case Fc3152ClearFormat:
            if ((lc->renderingMode != ModeASCII) && lc->doAutoEject)
                {
                fputs("Q\n", fcb);
                }
            lc->postPrintFunc = 0;
            lc->lpi           = 6;
            lc->doAutoEject   = FALSE;
            lc->doSuppress    = FALSE;

            return FcProcessed;

        case Fc3152PostVFU1:
        case Fc3152PostVFU2:
        case Fc3152PostVFU3:
        case Fc3152PostVFU4:
        case Fc3152PostVFU5:
        case Fc3152PostVFU6:
            lc->postPrintFunc = (u8)funcCode;

            return FcProcessed;

        case Fc3152SelectPrePrint:
            lc->postPrintFunc = 0;

            return FcProcessed;

        case Fc3152PreVFU1:
        case Fc3152PreVFU2:
        case Fc3152PreVFU3:
        case Fc3152PreVFU4:
        case Fc3152PreVFU5:
        case Fc3152PreVFU6:
            lc->prePrintFunc = (u8)funcCode;

            return FcProcessed;

        case Fc3152SelIntReady:
            // Enable next int.  If an I/O was done since the last
            // time an int enable was issued, don't clear the
            // current int.  That's because things go very slowly
            // otherwise; printer drivers typically issue the write,
            // then enable the int shortly after.  We've already set
            // "ready" by then, unlike physical printers.
            lc->flags |= Lp3000IntReady | Lp3000IntReadyEna;
            if (lc->keepInt)
                {
                lc->keepInt = FALSE;
                }
            else
                {
                lc->flags &= ~Lp3000IntReady;
                }
            // Update interrupt summary flag in unit block
            dcc6681Interrupt((lc->flags & (Lp3000IntReady | Lp3000IntEnd)) != 0);

            return FcProcessed;

        case Fc3152RelIntReady:
            lc->flags &= ~(Lp3000IntReadyEna | Lp3000IntReady);
            // Update interrupt summary flag in unit block
            dcc6681Interrupt((lc->flags & (Lp3000IntReady | Lp3000IntEnd)) != 0);

            return FcProcessed;

        case Fc3152SelIntEnd:
            lc->flags |= Lp3000IntEnd | Lp3000IntEndEna;
            if (lc->keepInt)
                {
                lc->keepInt = FALSE;
                }
            else
                {
                lc->flags &= ~Lp3000IntEnd;
                }
            // Update interrupt summary flag in unit block
            dcc6681Interrupt((lc->flags & (Lp3000IntReady | Lp3000IntEnd)) != 0);

            return FcProcessed;

        case Fc3152RelIntEnd:
            lc->flags &= ~(Lp3000IntEndEna | Lp3000IntEnd);
            // Update interrupt summary flag in unit block
            dcc6681Interrupt((lc->flags & (Lp3000IntReady | Lp3000IntEnd)) != 0);

            return FcProcessed;

        case Fc3152SelIntError:
        case Fc3152RelIntError:
        case Fc3152Release2:
            // All of the above are no-ops
            return FcProcessed;
            }
        }

    active3000Device->fcode = funcCode;

    return FcAccepted;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Perform I/O on 3000 line printer.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void lp3000Io(void)
    {
    LpContext *lc = (LpContext *)active3000Device->context[0];

    /*
    **  Process printer I/O.
    */
    switch (active3000Device->fcode)
        {
    default:
        break;

    case Fc6681Output:
        if (activeChannel->full)
            {
#if DEBUG
            if (lc->linePos % 16 == 0)
                {
                fputs("\n   ", lp3000Log);
                }
            fprintf(lp3000Log, " %04o", activeChannel->data);
#endif
            if (lc->linePos < MaxLineSize)
                {
                if (lc->flags & Lp3000ExtArray)
                    {
                    lc->line[lc->linePos++] = activeChannel->data & 0377;
                    }
                else
                    {
                    lc->line[lc->linePos++] = bcdToAscii[(activeChannel->data >> 6) & Mask6];
                    lc->line[lc->linePos++] = bcdToAscii[activeChannel->data & Mask6];
                    }
                }
            activeChannel->full = FALSE;
            lc->isPrinted       = TRUE;
            lc->keepInt         = TRUE;
            }
        break;

    case Fc6681Output + 1:
        // Fill image memory, just ignore that data
        activeChannel->full = FALSE;
        break;

    case Fc6681DevStatusReq:
        // Indicate ready plus whatever interrupts are enabled
        activeChannel->data     = StPrintReady | (lc->flags & (StPrintIntReady | StPrintIntEnd));
        activeChannel->full     = TRUE;
        active3000Device->fcode = 0;
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
static void lp3000Activate(void)
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
static void lp3000Disconnect(void)
    {
    FILE      *fcb = active3000Device->fcb[0];
    LpContext *lc  = (LpContext *)active3000Device->context[0];

    //
    //  This can happen if something goes wrong in the open and the file fails
    //  to be properly re-opened.
    //
    if (fcb == NULL)
        {
        logDtError(LogErrorLocation, "lp3000Disconnect: FCB is null on channel %o equipment %o\n",
                   active3000Device->channel->id,
                   active3000Device->eqNo);

        return;
        }

    if (active3000Device->fcode == Fc6681Output)
        {
#if DEBUG
        lp3000DebugData(lc);
#endif
        switch (lc->renderingMode)
            {
        default:
        case ModeCDC:
            lp3000PrintCDC(lc, fcb);
            break;

        case ModeANSI:
            lp3000PrintANSI(lc, fcb);
            break;

        case ModeASCII:
            lp3000PrintASCII(lc, fcb);
            break;
            }
        lc->linePos             = 0;
        active3000Device->fcode = 0;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Print a buffered line in ANSI mode.
**
**  Parameters:     Name        Description.
**                  lc          pointer to line printer context
**                  fcb         pointer to printer file descriptor
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void lp3000PrintANSI(LpContext *lc, FILE *fcb)
    {
    char *fe;
    u8   i;

    fe = NULL;
    if (lc->prePrintFunc != 0)
        {
        fe = lp3000FeForPrePrint(lc, lc->prePrintFunc);
        }
    if (lc->doSuppress)
        {
        lc->prePrintFunc = FcPrintNoSpace;
        }
    else
        {
        lc->prePrintFunc = 0;
        }
    if ((lc->postPrintFunc != 0) && (lc->doSuppress == FALSE))
        {
        lc->prePrintFunc = (lc->flags & Lp3000Type3555)
            ? (lc->postPrintFunc - Fc3555PostVFU1) + Fc3555PreVFU1
            : (lc->postPrintFunc - Fc3152PostVFU1) + Fc3152PreVFU1;
        }
    lc->doSuppress = FALSE;
    if ((fe == NULL) || (*fe != '+') || (lc->linePos > 0))
        {
        fputs(fe != NULL ? fe : " ", fcb);
        for (i = 0; i < lc->linePos; i++)
            {
            fputc(lc->line[i], fcb);
            }
        fputc('\n', fcb);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Print a buffered line in ASCII mode.
**
**  Parameters:     Name        Description.
**                  lc          pointer to line printer context
**                  fcb         pointer to printer file descriptor
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void lp3000PrintASCII(LpContext *lc, FILE *fcb)
    {
    int i;

    if (lc->prePrintFunc != 0)
        {
        fputs(lp3000FeForPrePrint(lc, lc->prePrintFunc), fcb);
        lc->prePrintFunc = 0;
        }
    for (i = 0; i < lc->linePos; i++)
        {
        fputc(lc->line[i], fcb);
        }
    if (lc->doSuppress)
        {
        fputc('\r', fcb);
        lc->doSuppress = FALSE;
        }
    else
        {
        fputc('\n', fcb);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Print a buffered line in CDC mode.
**
**  Parameters:     Name        Description.
**                  lc          pointer to line printer context
**                  fcb         pointer to printer file descriptor
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void lp3000PrintCDC(LpContext *lc, FILE *fcb)
    {
    u8   i;
    char *postFE;
    char *preFE;

    postFE = NULL;
    preFE  = NULL;
    if (lc->prePrintFunc != 0)
        {
        preFE = lp3000FeForPrePrint(lc, lc->prePrintFunc);
        }
    if (lc->doSuppress)
        {
        lc->prePrintFunc = FcPrintNoSpace;
        }
    else
        {
        lc->prePrintFunc = 0;
        }
    if ((lc->postPrintFunc != 0) && (lc->doSuppress == FALSE))
        {
        postFE = lp3000FeForPostPrint(lc, lc->postPrintFunc);
        }
    lc->doSuppress = FALSE;
    if (preFE != NULL)
        {
        if ((*preFE == '+') && (lc->linePos < 1) && (postFE == NULL))
            {
            return;
            }
        fputs(preFE, fcb);
        if (postFE != NULL)
            {
            fputc('\n', fcb);
            }
        }
    if (postFE != NULL)
        {
        fputs(postFE, fcb);
        }
    if ((preFE == NULL) && (postFE == NULL))
        {
        fputc(' ', fcb);
        if (lc->doSuppress)
            {
            lc->prePrintFunc = FcPrintNoSpace;
            }
        }
    for (i = 0; i < lc->linePos; i++)
        {
        fputc(lc->line[i], fcb);
        }
    fputc('\n', fcb);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Return the format effector for a post-print function.
**
**  Parameters:     Name        Description.
**                  lc          pointer to line printer context
**                  func        function number
**
**  Returns:        Format effector string.
**
**------------------------------------------------------------------------*/
static char *lp3000FeForPostPrint(LpContext *lc, u8 func)
    {
    switch (lc->renderingMode)
        {
    case ModeCDC:
        if (lc->flags & Lp3000Type3555)
            {
            switch (func)
                {
            default:
                return "";

            case Fc3555PostVFU1:
            case Fc3555PostVFU2:
            case Fc3555PostVFU3:
            case Fc3555PostVFU4:
            case Fc3555PostVFU5:
            case Fc3555PostVFU6:
            case Fc3555PostVFU7:
            case Fc3555PostVFU8:
            case Fc3555PostVFU9:
            case Fc3555PostVFU10:
            case Fc3555PostVFU11:
            case Fc3555PostVFU12:
                return postPrintCdcEffectors[lc->postPrintFunc - Fc3555PostVFU1];
                }
            }
        else
            {
            switch (func)
                {
            default:
                return "";

            case Fc3152PostVFU1:
            case Fc3152PostVFU2:
            case Fc3152PostVFU3:
            case Fc3152PostVFU4:
            case Fc3152PostVFU5:
            case Fc3152PostVFU6:
                return postPrintCdcEffectors[lc->postPrintFunc - Fc3152PostVFU1];
                }
            }

    case ModeANSI:
    case ModeASCII:
    default:
        return ""; // print nothing in ANSI and ASCII modes
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Return the format effector for a pre-print function.
**
**  Parameters:     Name        Description.
**                  lc          pointer to line printer context
**                  func        function number
**
**  Returns:        Format effector string.
**
**------------------------------------------------------------------------*/
static char *lp3000FeForPrePrint(LpContext *lc, u8 func)
    {
    switch (lc->renderingMode)
        {
    default:
    case ModeCDC:
        if (lc->flags & Lp3000Type3555)
            {
            switch (func)
                {
            default:
                return " ";

            case FcPrintSingle:
                return "0";

            case FcPrintDouble:
                return "-";

            case FcPrintLastLine:
                return "2";

            case FcPrintEject:
                return "1";

            case FcPrintNoSpace:
                return "+";

            case Fc3555PreVFU1:
            case Fc3555PreVFU2:
            case Fc3555PreVFU3:
            case Fc3555PreVFU4:
            case Fc3555PreVFU5:
            case Fc3555PreVFU6:
            case Fc3555PreVFU7:
            case Fc3555PreVFU8:
            case Fc3555PreVFU9:
            case Fc3555PreVFU10:
            case Fc3555PreVFU11:
            case Fc3555PreVFU12:
                return prePrintCdcEffectors[lc->prePrintFunc - Fc3555PreVFU1];
                }
            }
        else
            {
            switch (lc->prePrintFunc)
                {
            default:
                return " ";

            case FcPrintSingle:
                return "0";

            case FcPrintDouble:
                return "-";

            case FcPrintLastLine:
                return "2";

            case FcPrintEject:
                return "1";

            case FcPrintNoSpace:
                return "+";

            case Fc3152PreVFU1:
            case Fc3152PreVFU2:
            case Fc3152PreVFU3:
            case Fc3152PreVFU4:
            case Fc3152PreVFU5:
            case Fc3152PreVFU6:
                return prePrintCdcEffectors[lc->prePrintFunc - Fc3152PreVFU1];
                }
            }
        break;

    case ModeANSI:
        if (lc->flags & Lp3000Type3555)
            {
            switch (func)
                {
            default:
                return " ";

            case FcPrintSingle:
                return "0";

            case FcPrintDouble:
                return "-";

            case FcPrintLastLine:
                return "C";

            case FcPrintEject:
                return "1";

            case FcPrintNoSpace:
                return "+";

            case Fc3555PreVFU1:
            case Fc3555PreVFU2:
            case Fc3555PreVFU3:
            case Fc3555PreVFU4:
            case Fc3555PreVFU5:
            case Fc3555PreVFU6:
            case Fc3555PreVFU7:
            case Fc3555PreVFU8:
            case Fc3555PreVFU9:
            case Fc3555PreVFU10:
            case Fc3555PreVFU11:
            case Fc3555PreVFU12:
                return prePrintAnsiEffectors[lc->prePrintFunc - Fc3555PreVFU1];
                }
            }
        else
            {
            switch (lc->prePrintFunc)
                {
            default:
                return " ";

            case FcPrintSingle:
                return "0";

            case FcPrintDouble:
                return "-";

            case FcPrintLastLine:
                return "C";

            case FcPrintEject:
                return "1";

            case FcPrintNoSpace:
                return "+";

            case Fc3152PreVFU1:
            case Fc3152PreVFU2:
            case Fc3152PreVFU3:
            case Fc3152PreVFU4:
            case Fc3152PreVFU5:
            case Fc3152PreVFU6:
                return prePrintAnsiEffectors[lc->prePrintFunc - Fc3152PreVFU1];
                }
            }
        break;

    case ModeASCII:
        switch (func)
            {
        default:
            return "";

        case FcPrintSingle:
            return "\n";

        case FcPrintDouble:
            return "\n\n";

        case FcPrintEject:
            return "\f";
            }
        }
    }

#if DEBUG

/*--------------------------------------------------------------------------
**  Purpose:        Dump raw line data.
**
**  Parameters:     Name        Description.
**                  lc          pointer to line printer context
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void lp3000DebugData(LpContext *lc)
    {
    int i;

    if (lc->linePos > 0)
        {
        fprintf(lp3000Log, "\n    prePrintFunc:%04o  postPrintFunc:%04o  doSuppress:%s",
                lc->prePrintFunc, lc->postPrintFunc, lc->doSuppress ? "TRUE" : "FALSE");
        for (i = 0; i < lc->linePos; i++)
            {
            if (i % 136 == 0)
                {
                fputc('\n', lp3000Log);
                }
            fputc(lc->line[i], lp3000Log);
            }
        fputc('\n', lp3000Log);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Convert function code to string.
**
**  Parameters:     Name        Description.
**                  lc          pointer to line printer context
**                  funcCode    function code
**
**  Returns:        String equivalent of function code.
**
**------------------------------------------------------------------------*/
static char *lp3000Func2String(LpContext *lc, PpWord funcCode)
    {
    static char buf[40];

    switch (funcCode)
        {
    default:
        break;

    case FcPrintRelease:
        return "FcPrintRelease";

    case FcPrintSingle:
        return "FcPrintSingle";

    case FcPrintDouble:
        return "FcPrintDouble";

    case FcPrintLastLine:
        return "FcPrintLastLine";

    case FcPrintEject:
        return "FcPrintEject";

    case FcPrintAutoEject:
        return "FcPrintAutoEject";

    case FcPrintNoSpace:
        return "FcPrintNoSpace";

    case Fc6681MasterClear:
        return "Fc6681MasterClear";

    case Fc6681Output:
        return "Fc6681Output";

    case Fc6681DevStatusReq:
        return "Fc6681DevStatusReq";
        }
    if (lc->flags & Lp3000Type3555)             // This is LP3555
        {
        switch (funcCode)
            {
        default:
            break;

        case Fc3555CondClearFormat:
            return "Fc3555CondClearFormat";

        case Fc3555Sel8Lpi:
            return "Fc3555Sel8Lpi";

        case Fc3555Sel6Lpi:
            return "Fc3555Sel6Lpi";

        case Fc3555FillMemory:
            return "Fc3555FillMemory";

        case Fc3555SelExtArray:
            return "Fc3555SelExtArray";

        case Fc3555ClearExtArray:
            return "Fc3555ClearExtArray";

        case Fc3555SelIntReady:
            return "Fc3555SelIntReady";

        case Fc3555RelIntReady:
            return "Fc3555RelIntReady";

        case Fc3555SelIntEnd:
            return "Fc3555SelIntEnd";

        case Fc3555RelIntEnd:
            return "Fc3555RelIntEnd";

        case Fc3555SelIntError:
            return "Fc3555SelIntError";

        case Fc3555RelIntError:
            return "Fc3555RelIntError";

        case Fc3555ReloadMemEnable:
            return "Fc3555ReloadMemEnable";

        case Fc3555ClearFormat:
            return "Fc3555ClearFormat";

        case Fc3555PostVFU1:
            return "Fc3555PostVFU1";

        case Fc3555PostVFU2:
            return "Fc3555PostVFU2";

        case Fc3555PostVFU3:
            return "Fc3555PostVFU3";

        case Fc3555PostVFU4:
            return "Fc3555PostVFU4";

        case Fc3555PostVFU5:
            return "Fc3555PostVFU5";

        case Fc3555PostVFU6:
            return "Fc3555PostVFU6";

        case Fc3555PostVFU7:
            return "Fc3555PostVFU7";

        case Fc3555PostVFU8:
            return "Fc3555PostVFU8";

        case Fc3555PostVFU9:
            return "Fc3555PostVFU9";

        case Fc3555PostVFU10:
            return "Fc3555PostVFU10";

        case Fc3555PostVFU11:
            return "Fc3555PostVFU11";

        case Fc3555PostVFU12:
            return "Fc3555PostVFU12";

        case Fc3555SelectPrePrint:
            return "Fc3555SelectPrePrint";

        case Fc3555PreVFU1:
            return "Fc3555PreVFU1";

        case Fc3555PreVFU2:
            return "Fc3555PreVFU2";

        case Fc3555PreVFU3:
            return "Fc3555PreVFU3";

        case Fc3555PreVFU4:
            return "Fc3555PreVFU4";

        case Fc3555PreVFU5:
            return "Fc3555PreVFU5";

        case Fc3555PreVFU6:
            return "Fc3555PreVFU6";

        case Fc3555PreVFU7:
            return "Fc3555PreVFU7";

        case Fc3555PreVFU8:
            return "Fc3555PreVFU8";

        case Fc3555PreVFU9:
            return "Fc3555PreVFU9";

        case Fc3555PreVFU10:
            return "Fc3555PreVFU10";

        case Fc3555PreVFU11:
            return "Fc3555PreVFU11";

        case Fc3555PreVFU12:
            return "Fc3555PreVFU12";

        case Fc3555MaintStatus:
            return "Fc3555MaintStatus";

        case Fc3555ClearMaint:
            return "Fc3555ClearMaint";
            }
        }
    else
        {
        switch (funcCode)
            {
        default:
            break;

        case Fc3152ClearFormat:
            return "Fc3152ClearFormat";

        case Fc3152PostVFU1:
            return "Fc3152PostVFU1";

        case Fc3152PostVFU2:
            return "Fc3152PostVFU2";

        case Fc3152PostVFU3:
            return "Fc3152PostVFU3";

        case Fc3152PostVFU4:
            return "Fc3152PostVFU4";

        case Fc3152PostVFU5:
            return "Fc3152PostVFU5";

        case Fc3152PostVFU6:
            return "Fc3152PostVFU6";

        case Fc3152SelectPrePrint:
            return "Fc3152SelectPrePrint";

        case Fc3152PreVFU1:
            return "Fc3152PreVFU1";

        case Fc3152PreVFU2:
            return "Fc3152PreVFU2";

        case Fc3152PreVFU3:
            return "Fc3152PreVFU3";

        case Fc3152PreVFU4:
            return "Fc3152PreVFU4";

        case Fc3152PreVFU5:
            return "Fc3152PreVFU5";

        case Fc3152PreVFU6:
            return "Fc3152PreVFU6";

        case Fc3152SelIntReady:
            return "Fc3152SelIntReady";

        case Fc3152RelIntReady:
            return "Fc3152RelIntReady";

        case Fc3152SelIntEnd:
            return "Fc3152SelIntEnd";

        case Fc3152RelIntEnd:
            return "Fc3152RelIntEnd";

        case Fc3152SelIntError:
            return "Fc3152SelIntError";

        case Fc3152RelIntError:
            return "Fc3152RelIntError";

        case Fc3152Release2:
            return "Fc3152Release2";
            }
        }
    sprintf(buf, "Unknown Function: %04o", funcCode);

    return (buf);
    }

#endif

/*---------------------------  End Of File  ------------------------------*/
