/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**            (c) 2017       Steven Zoppi 22-Oct-2017
**                           Added Ascii and ANSI support
**                           Added subdirectory support
**            (c) 2022       Kevin Jordan 02-Dec-2022
**                           Redesign to expose format effectors properly
**
**  Name: lp1612.c
**
**  Description:
**      Perform emulation of CDC 6600 1612 line printer.
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
#include <time.h>
#include <errno.h>
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
**      Function codes
**
**      ----------------------------------
**      |  Equip select  |   function    |
**      ----------------------------------
**      11              6 5             0
**
**      06x0 = Select printer
**      06x1 = Single space
**      06x2 = Double space
**      06x3 = Move paper to format channel 7
**      06x4 = Move paper to top of form
**      06x5 = Print
**      06x6 = Suppress line advance after next print
**      06x7 = Status request
**
**      x = printer unit # on channel
*/

#define FcPrintSelect          00600
#define FcPrintSingle          00601
#define FcPrintDouble          00602
#define FcPrintMoveChannel7    00603
#define FcPrintEject           00604
#define FcPrintPrint           00605
#define FcPrintNoSpace         00606
#define FcPrintStatusReq       00607
#define FcPrintClearFormat     00610
#define FcPrintFormat1         00611
#define FcPrintFormat2         00612
#define FcPrintFormat3         00613
#define FcPrintFormat4         00614
#define FcPrintFormat5         00615
#define FcPrintFormat6         00616

/*
**      Status reply
**
**      0000 = Not Ready
**      4000 = Ready
**
*/
#define StPrintReady           04000
#define StPrintNotReady        00000

/*
**  Output rendering modes
*/
#define ModeCDC                  0
#define ModeANSI                 1
#define ModeASCII                2

/*
**  Maximum number of characters per line
*/
#define MaxLineSize              120

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
**      This is a simple device so we need a minimal context block.
*/
typedef struct lpContext
    {
    /*
    **  Info for show_tape operator command.
    */
    struct lpContext    *nextUnit;
    u8                   channelNo;
    u8                   eqNo;
    u8                   unitNo;

    u8                   renderingMode;
    PpWord               prePrintFunc;       //  last pre-print function (0 = no pre-print function specified)
    PpWord               postPrintFunc;      //  last post-print function (0 = no post-print function specified)
    bool                 doSuppress;         //  suppress next post-print spacing op
    PpWord               line[MaxLineSize];  //  buffered line
    u8                   linePos;            //  current line position

    char                 path[MaxFSPath];
    char                 curFileName[MaxFSPath+128];
    } LpContext;

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static char    *lp1612FeForPostPrint(LpContext *lc, PpWord func);
static char    *lp1612FeForPrePrint(LpContext *lc, PpWord func);
static FcStatus lp1612Func(PpWord funcCode);
static void     lp1612Io(void);
static void     lp1612Activate(void);
static void     lp1612Disconnect(void);
static void     lp1612PrintANSI(LpContext *lc, FILE *fcb);
static void     lp1612PrintASCII(LpContext *lc, FILE *fcb);
static void     lp1612PrintCDC(LpContext *lc, FILE *fcb);

#if DEBUG
static void lp1612DebugData(LpContext *lc);
static char *lp1612Func2String(PpWord funcCode);
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

static LpContext *firstLp1612 = NULL;
static LpContext *lastLp1612  = NULL;

static char *prePrintAnsiEffectors[] = {
    " ", // print format 1
    "2", // print format 2
    "3", // print format 3
    "4", // print format 4
    "5", // print format 5
    "6", // print format 6
};

static char *postPrintCdcEffectors[] = {
    " ", // print format 1
    "G", // print format 2
    "F", // print format 3
    "E", // print format 4
    "D", // print format 5
    "C", // print format 6
};

static char *renderingModes[] = {
    "CDC",
    "ANSI",
    "ASCII"
};

#if DEBUG
static FILE *lp1612Log = NULL;
#endif

/*
 **--------------------------------------------------------------------------
 **
 **  Public Functions
 **
 **--------------------------------------------------------------------------
 */

/*--------------------------------------------------------------------------
**  Purpose:        Initialise 1612 line printer.
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
void lp1612Init(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName)
    {
    char      *deviceMode;
    char      *devicePath;
    DevSlot   *dp;
    LpContext *lc;
    u8        mode;

#if DEBUG
    if (lp1612Log == NULL)
        {
        lp1612Log = fopen("lp1612log.txt", "wt");
        }
#endif

    if (eqNo != 0)
        {
        fprintf(stderr, "(lp1612 ) Invalid equipment number - LP1612 is hardwired to equipment number 0\n");
        exit(1);
        }

    if (unitNo != 0)
        {
        fprintf(stderr, "(lp1612 ) Invalid unit number - LP1612 is hardwired to unit number 0\n");
        exit(1);
        }

    dp = channelAttach(channelNo, eqNo, DtLp1612);

    dp->activate     = lp1612Activate;
    dp->disconnect   = lp1612Disconnect;
    dp->func         = lp1612Func;
    dp->io           = lp1612Io;
    dp->selectedUnit = 0;
    lc = (LpContext *)calloc(1, sizeof(LpContext));
    if (lc == NULL)
        {
        fprintf(stderr, "(lp1612 ) Failed to allocate LP1612 context block\n");
        exit(1);
        }

    /*
    **  When we are called, "deviceParams" is a space terminated string
    **  at the end of the INI entry.
    **
    **  Tokenizing the remainder of the string as comma-delimited
    **  parameters gives us the configuration data that we need.
    **
    **  The format of the remainder of the line is:
    **
    **      <DevicePath>
    **      <OutputMode> ("CDC"|"ANSI"|"ASCII")
    **
    */
    devicePath = strtok(deviceName, ", ");     //  Get the Path (subdirectory)
    deviceMode = strtok(NULL, ", ");

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
            fprintf(stderr, "(lp1612 ) Unrecognized output rendering mode '%s'\n", deviceMode);
            exit(1);
            }
        }

    dp->context[0]    = (void *)lc;
    lc->renderingMode = mode;
    lc->channelNo     = channelNo;
    lc->unitNo        = unitNo;
    lc->eqNo          = eqNo;
    lc->prePrintFunc  = 0;
    lc->postPrintFunc = 0;
    lc->doSuppress    = FALSE;

    //  Remember the device path for future fopen calls
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
    sprintf(lc->curFileName, "%sLP1612_C%02o", lc->path, channelNo);
    dp->fcb[0] = fopen(lc->curFileName, "w+t");

    if (dp->fcb[0] == NULL)
        {
        fprintf(stderr, "(lp1612 ) Failed to open %s\n", lc->curFileName);
        exit(1);
        }

    /*
    **  Print a friendly message.
    */
    printf("(lp1612 ) Iinitialised on channel %o equipment %o mode %s filename '%s'\n",
           channelNo, eqNo, renderingModes[mode], lc->curFileName);

    /*
    **  Link into list of lp1612 Line Printer units.
    */
    if (lastLp1612 == NULL)
        {
        firstLp1612 = lc;
        }
    else
        {
        lastLp1612->nextUnit = lc;
        }

    lastLp1612 = lc;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Show Line Printer Status (operator interface).
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void lp1612ShowStatus()
    {
    LpContext *lc;
    char      outBuf[MaxFSPath+128];

    if (lc == NULL)
        {
        return;
        }

    for (lc = firstLp1612; lc != NULL; lc = lc->nextUnit)
        {
        sprintf(outBuf, "    >   %-8s C%02o E%02o U%02o", "1612", lc->channelNo, lc->eqNo, lc->unitNo);
        opDisplay(outBuf);
        sprintf(outBuf, "   %-20s (mode ", lc->curFileName);
        opDisplay(outBuf);
        opDisplay(renderingModes[lc->renderingMode]);
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
void lp1612RemovePaper(char *params)
    {
    int       channelNo;
    time_t    currentTime;
    DevSlot   *dp;
    int       equipmentNo;
    char      fNameNew[MaxFSPath+128];
    int       iSuffix;
    LpContext *lc;
    int       numParam;
    char      outBuf[MaxFSPath*2+300];
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
        opDisplay("(lp1612 ) Not enough or invalid parameters\n");

        return;
        }

    if ((channelNo < 0) || (channelNo >= MaxChannels))
        {
        opDisplay("(lp1612 ) Invalid channel no\n");

        return;
        }

    if ((equipmentNo < 0) || (equipmentNo >= MaxEquipment))
        {
        opDisplay("(lp1612 ) Invalid equipment no\n");

        return;
        }

    /*
    **  Locate the device control block.
    */
    dp = channelFindDevice((u8)channelNo, DtLp1612);
    if (dp == NULL)
        {
        return;
        }

    lc = (LpContext *)dp->context[0];

    renameOK = FALSE;

    //  SZoppi: this can happen if something goes wrong in the open
    //          and the file fails to be properly re-opened.
    if (dp->fcb[0] == NULL)
        {
        fprintf(stderr, "(lp1612 ) lp1612RemovePaper: FCB is null on channel %o equipment %o\n",
                dp->channel->id,
                dp->eqNo);
        //  proceed to attempt to open a new FCB
        }
    else
        {
        fflush(dp->fcb[0]);

        if (ftell(dp->fcb[0]) == 0)
            {
            sprintf(outBuf, "(lp1612 ) No output has been written on channel %o and equipment %o\n", channelNo, equipmentNo);
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
                sprintf(outBuf, "(lp1612 ) Rename Failure '%s' to '%s' - (%s).\n", lc->curFileName, fNameNew, strerror(errno));
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

                fprintf(stderr, "(lp1612 ) Rename Failure '%s' to '%s' - (%s). Retrying (%d)...\n",
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
    dp->fcb[0] = fopen(lc->curFileName, renameOK ? "w" : "a");

    /*
    **  Check if the open succeeded.
    */
    if (dp->fcb[0] == NULL)
        {
        fprintf(stderr, "(lp1612 ) Failed to open %s\n", lc->curFileName);

        return;
        }

    if (renameOK)
        {
        sprintf(outBuf, "(lp1612 ) Paper removed and available on '%s'\n", fNameNew);
        opDisplay(outBuf);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Execute function code on 1612 line printer.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        FcStatus
**
**------------------------------------------------------------------------*/
static FcStatus lp1612Func(PpWord funcCode)
    {
    LpContext *lc = (LpContext *)activeDevice->context[0];
    FILE *fcb         = activeDevice->fcb[0];

    //
    //  This can happen if something goes wrong in the open and the file fails
    //  to be properly re-opened.
    //
    if (fcb == NULL)
        {
        fprintf(stderr, "(lp1612 ) lp1612Io: FCB is null on channel %o equipment %o\n",
                activeDevice->channel->id,
                activeDevice->eqNo);

        return FcProcessed;
        }

#if DEBUG
    fprintf(lp1612Log, "\n%06d PP:%02o CH:%02o f:%04o T:%-25s  >   ",
            traceSequenceNo,
            activePpu->id,
            activeDevice->channel->id,
            funcCode,
            lp1612Func2String(funcCode));
#endif

    activeDevice->fcode = funcCode;

    switch (funcCode)
        {
    default:
        return FcDeclined;

    case FcPrintSelect:
        break;

    case FcPrintPrint:
#if DEBUG
        lp1612DebugData(lc);
#endif
        switch (lc->renderingMode)
            {
        default:
        case ModeCDC:
            lp1612PrintCDC(lc, fcb);
            break;

        case ModeANSI:
            lp1612PrintANSI(lc, fcb);
            break;

        case ModeASCII:
            lp1612PrintASCII(lc, fcb);
            break;
            }
        lc->linePos = 0;
        break;

    case FcPrintSingle:
    case FcPrintDouble:
    case FcPrintMoveChannel7:
    case FcPrintEject:
        if (lc->prePrintFunc != 0 && lc->prePrintFunc != FcPrintNoSpace)
            {
            fputs(lp1612FeForPrePrint(lc, lc->prePrintFunc), fcb);
            fputc('\n', fcb);
            }
        lc->prePrintFunc = funcCode;
        fflush(fcb);
        break;

    case FcPrintNoSpace:
        lc->doSuppress = TRUE;
        break;

    case FcPrintStatusReq:
        activeChannel->status = StPrintReady;
        break;

    case FcPrintClearFormat:
        lc->postPrintFunc = 0;
        lc->doSuppress    = FALSE;
        break;

    case FcPrintFormat1:
    case FcPrintFormat2:
    case FcPrintFormat3:
    case FcPrintFormat4:
    case FcPrintFormat5:
    case FcPrintFormat6:
        lc->postPrintFunc = funcCode;
        break;
        }

    return FcAccepted;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Perform I/O on 1612 line printer.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void lp1612Io(void)
    {
    LpContext *lc = (LpContext *)activeDevice->context[0];
    FILE *fcb         = activeDevice->fcb[0];

    //
    //  This can happen if something goes wrong in the open and the file fails
    //  to be properly re-opened.
    //
    if (fcb == NULL)
        {
        fprintf(stderr, "(lp1612 ) lp1612Io: FCB is null on channel %o equipment %o\n",
                activeDevice->channel->id,
                activeDevice->eqNo);

        return;
        }

    if (activeDevice->fcode == FcPrintStatusReq)
        {
        activeChannel->data   = activeChannel->status;
        activeChannel->full   = TRUE;
        activeDevice->fcode   = 0;
        activeChannel->status = 0;
        }
    else if (activeChannel->full)
        {
#if DEBUG
       if (lc->linePos % 16 == 0) fputs("\n   ", lp1612Log);
       fprintf(lp1612Log, " %04o", activeChannel->data);
#endif
        if (lc->linePos < MaxLineSize)
            {
            lc->line[lc->linePos++] = extBcdToAscii[activeChannel->data & 077];
            }
        activeChannel->full = FALSE;
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
static void lp1612Activate(void)
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
static void lp1612Disconnect(void)
    {
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
static void lp1612PrintANSI(LpContext *lc, FILE *fcb)
    {
    char *fe;
    u8   i;

    fe = NULL;
    if (lc->prePrintFunc != 0)
        {
        fe = lp1612FeForPrePrint(lc, lc->prePrintFunc);
        }
    if (lc->doSuppress)
        {
        lc->prePrintFunc = FcPrintNoSpace;
        }
    else
        {
        lc->prePrintFunc = 0;
        }
    if (lc->postPrintFunc != 0 && lc->postPrintFunc != FcPrintFormat1 && lc->doSuppress == FALSE)
        {
        lc->prePrintFunc = lc->postPrintFunc;
        }
    lc->doSuppress = FALSE;
    if (fe == NULL || *fe != '+' || lc->linePos > 0)
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
static void lp1612PrintASCII(LpContext *lc, FILE *fcb)
    {
    int i;

    if (lc->prePrintFunc != 0)
        {
        fputs(lp1612FeForPrePrint(lc, lc->prePrintFunc), fcb);
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
static void lp1612PrintCDC(LpContext *lc, FILE *fcb)
    {
    u8   i;
    char *postFE;
    char *preFE;

    postFE = NULL;
    preFE  = NULL;
    if (lc->prePrintFunc != 0)
        {
        preFE = lp1612FeForPrePrint(lc, lc->prePrintFunc);
        }
    if (lc->doSuppress)
        {
        lc->prePrintFunc = FcPrintNoSpace;
        }
    else
        {
        lc->prePrintFunc = 0;
        }
    if (lc->postPrintFunc != 0 && lc->postPrintFunc != FcPrintFormat1 && lc->doSuppress == FALSE)
        {
        postFE = lp1612FeForPostPrint(lc, lc->postPrintFunc);
        }
    lc->doSuppress = FALSE;
    if (preFE != NULL)
        {
        if (*preFE == '+' && lc->linePos < 1 && postFE == NULL) return;
        fputs(preFE, fcb);
        if (postFE != NULL) fputc('\n', fcb);
        }
    if (postFE != NULL)
        {
        fputs(postFE, fcb);
        }
    if (preFE == NULL && postFE == NULL)
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
static char *lp1612FeForPostPrint(LpContext *lc, PpWord func)
    {
    switch (lc->renderingMode)
        {
    case ModeCDC:
        switch (func)
            {
        default            : return "";
        case FcPrintFormat1:
        case FcPrintFormat2:
        case FcPrintFormat3:
        case FcPrintFormat4:
        case FcPrintFormat5:
        case FcPrintFormat6:
            return postPrintCdcEffectors[lc->postPrintFunc - FcPrintFormat1];
            }
        break;
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
static char *lp1612FeForPrePrint(LpContext *lc, PpWord func)
    {
    switch (lc->renderingMode)
        {
    default:
    case ModeCDC:
        switch (func)
            {
        default                  : return " ";
        case FcPrintSingle       : return "0";
        case FcPrintDouble       : return "-";
        case FcPrintMoveChannel7 : return "2";
        case FcPrintEject        : return "1";
        case FcPrintNoSpace      : return "+";
            }
        break;

    case ModeANSI:
        switch (func)
            {
        default                  : return " ";
        case FcPrintSingle       : return "0";
        case FcPrintDouble       : return "-";
        case FcPrintMoveChannel7 : return "7";
        case FcPrintEject        : return "1";
        case FcPrintNoSpace      : return "+";
        case FcPrintFormat1:
        case FcPrintFormat2:
        case FcPrintFormat3:
        case FcPrintFormat4:
        case FcPrintFormat5:
        case FcPrintFormat6:
            return prePrintAnsiEffectors[lc->postPrintFunc - FcPrintFormat1];
            }
        break;

    case ModeASCII:
        switch (func)
            {
        default            : return "";
        case FcPrintSingle : return "\n";
        case FcPrintDouble : return "\n\n";
        case FcPrintEject  : return "\f";
            }
        break;
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
static void lp1612DebugData(LpContext *lc)
    {
    int i;

    if (lc->linePos > 0)
        {
        fprintf(lp1612Log, "\n    prePrintFunc:%04o  postPrintFunc:%04o  doSuppress:%s",
            lc->prePrintFunc, lc->postPrintFunc, lc->doSuppress ? "TRUE" : "FALSE");
        for (i = 0; i < lc->linePos; i++)
            {
            if (i % 136 == 0)
                {
                fputc('\n', lp1612Log);
                }
            fputc(lc->line[i], lp1612Log);
            }
        fputc('\n', lp1612Log);
        }
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
static char *lp1612Func2String(PpWord funcCode)
    {
    static char buf[40];

    switch (funcCode)
        {
    default: break;
    case FcPrintSelect       : return "FcPrintSelect";
    case FcPrintSingle       : return "FcPrintSingle";
    case FcPrintDouble       : return "FcPrintDouble";
    case FcPrintMoveChannel7 : return "FcPrintMoveChannel7";
    case FcPrintEject        : return "FcPrintEject";
    case FcPrintPrint        : return "FcPrintPrint";
    case FcPrintNoSpace      : return "FcPrintNoSpace";
    case FcPrintStatusReq    : return "FcPrintStatusReq";
    case FcPrintClearFormat  : return "FcPrintClearFormat";
    case FcPrintFormat1      : return "FcPrintFormat1";
    case FcPrintFormat2      : return "FcPrintFormat2";
    case FcPrintFormat3      : return "FcPrintFormat3";
    case FcPrintFormat4      : return "FcPrintFormat4";
    case FcPrintFormat5      : return "FcPrintFormat5";
    case FcPrintFormat6      : return "FcPrintFormat6";
        }
    sprintf(buf, "Unknown Function: %04o", funcCode);

    return (buf);
    }
#endif

/*---------------------------  End Of File  ------------------------------*/
