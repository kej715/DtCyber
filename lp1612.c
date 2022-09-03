/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**            (c) 2017       Steven Zoppi 22-Oct-2017
**                           Added Ascii and ANSI support
**                           Added subdirectory support
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
#define FcPrintSingleSpace     00601
#define FcPrintDoubleSpace     00602
#define FcPrintMoveChannel7    00603
#define FcPrintMoveTOF         00604
#define FcPrintPrint           00605
#define FcPrintSuppressLF      00606
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
**  (SZoppi) 22-Oct-2017
**      These extensions are to properly simulate the behavior of the
**      line printers.  Line space handling (pre and post) messes with
**      translation to proper PDF format (the new purpose for this).
**
**      This is a simple device so we need a minimal context block.
*/
typedef struct lpContext1612
    {
    /*
    **  Info for show_tape operator command.
    */
    struct lpContext1612 *nextUnit;
    u8                   channelNo;
    u8                   eqNo;
    u8                   unitNo;

    bool                 extUseANSI;
    char                 extPath[MaxFSPath];
    } LpContext1612;

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static FcStatus lp1612Func(PpWord funcCode);
static void lp1612Io(void);
static void lp1612Activate(void);
static void lp1612Disconnect(void);

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

static LpContext1612 *firstLp1612 = NULL;
static LpContext1612 *lastLp1612  = NULL;

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
    DevSlot       *dp;
    LpContext1612 *lc;
    bool          useANSI = TRUE;

    char fname[MaxFSPath];
    char *deviceType;
    char *devicePath;
    char *deviceMode;

    (void)deviceName;

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
    lc = (LpContext1612 *)calloc(1, sizeof(LpContext1612));
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
    **      <DeviceType> (NULL(="3555")|"3555"|"3512")
    **      <OutputMode> ("ASCII"|"ANSI")
    **
    */
    deviceType = strtok(deviceName, ",");     //  Get Device Type
    devicePath = strtok(NULL, ",");           //  Get the Path (subdirectory)
    deviceMode = strtok(NULL, ",");

    if ((deviceMode) != NULL)
        {
        useANSI    = FALSE;
        if (strcasecmp(deviceMode, "ansi") == 0)
            {
            useANSI = TRUE;
            }
        else if (strcasecmp(deviceMode, "ascii") == 0)
            {
            useANSI = FALSE;
            }
        else
            {
            useANSI = FALSE;
            }
        }

    dp->context[0] = (void *)lc;
    lc->extUseANSI = useANSI;
    lc->channelNo  = channelNo;
    lc->unitNo     = unitNo;
    lc->eqNo       = eqNo;


    //  Remember the device Path for future fopen calls
    if (devicePath == NULL)
        {
        lc->extPath[0] = '\0';
        }
    else
        {
        strcpy(lc->extPath, devicePath);
        if (lc->extPath[0] != '\0')
            {
            strcat(lc->extPath, "/");
            }
        }

    /*
    **  Open the device file.
    */

    sprintf(fname, "%sLP1612_C%02o", lc->extPath, channelNo);
    dp->fcb[0] = fopen(fname, "w+t");

    if (dp->fcb[0] == NULL)
        {
        fprintf(stderr, "(lp1612 ) Failed to open %s\n", fname);
        exit(1);
        }

    /*
    **  Print a friendly message.
    */
    printf("(lp1612 ) Initialised on channel %o equipment %o filename %s\n",
           channelNo,
           eqNo,
           fname);

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
    LpContext1612 *lc = firstLp1612;
    char          outBuf[MaxFSPath+128];

    if (lc == NULL)
        {
        return;
        }

    opDisplay("\n    > Line Printer (lp1612) Status:\n");

    while (lc)
        {
        sprintf(outBuf, "    > CH %02o EQ %02o UN %02o Mode %s Path '%s'\n",
                lc->channelNo,
                lc->eqNo,
                lc->unitNo,
                lc->extUseANSI ? "ANSI" : "ASCII",
                lc->extPath);
        opDisplay(outBuf);
        lc = lc->nextUnit;
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
    int channelNo;
    time_t        currentTime;
    DevSlot       *dp;
    int equipmentNo;
    char fName[MaxFSPath+128];
    char fNameNew[MaxFSPath+128];
    int iSuffix;
    LpContext1612 *lc;
    int numParam;
    char outBuf[MaxFSPath*2+256];
    bool renameOK;
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

    lc = (LpContext1612 *)dp->context[0];
    sprintf(fName, "%sLP1612_C%02o", lc->extPath, channelNo);

    renameOK = FALSE;

    //  SZoppi: this can happen if something goes wrong in the open
    //          and the file fails to be properly re-opened.
    if (dp->fcb[0] == NULL)
        {
        fprintf(stderr, "(lp1612 ) lp1612RemovePaper: FCB is Null on channel %o equipment %o\n",
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
            if (rename(fName, fNameNew) == 0)
                {
                renameOK = TRUE;
                }
            else
                {
                sprintf(outBuf, "(lp1612 ) Rename Failure '%s' to '%s' - (%s).\n", fName, fNameNew, strerror(errno));
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
                        lc->extPath,
                        t.tm_year + 1900,
                        t.tm_mon + 1,
                        t.tm_mday,
                        t.tm_hour,
                        t.tm_min,
                        t.tm_sec,
                        iSuffix);

                if (rename(fName, fNameNew) == 0)
                    {
                    renameOK = TRUE;
                    break;
                    }

                fprintf(stderr, "(lp1612 ) Rename Failure '%s' to '%s' - (%s). Retrying (%d)...\n",
                        fName,
                        fNameNew,
                        strerror(errno),
                        iSuffix);
                }
            }
        }

    /*
    **  Open the device file.
    */
    dp->fcb[0] = fopen(fName, renameOK ? "w" : "a");

    /*
    **  Check if the open succeeded.
    */
    if (dp->fcb[0] == NULL)
        {
        fprintf(stderr, "(lp1612 ) Failed to open %s\n", fName);

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
    FILE          *fcb = activeDevice->fcb[0];
    LpContext1612 *lc;

    lc = (LpContext1612 *)activeDevice->context[0];

    //  SZoppi: this can happen if something goes wrong in the open
    //          and the file fails to be properly re-opened.
    if (activeDevice->fcb[0] == NULL)
        {
        fprintf(stderr, "(lp1612 ) lp1612Func: FCB is Null on channel %o equipment %o\n",
                activeDevice->channel->id,
                activeDevice->eqNo);

        return (FcProcessed);
        }

    switch (funcCode)
        {
    default:

        return (FcDeclined);

    case FcPrintSelect:
        break;

    case FcPrintSingleSpace:
        if (lc->extUseANSI)
            {
            fprintf(fcb, "\n ");
            }
        else
            {
            fprintf(fcb, "\n");
            }
        break;

    case FcPrintDoubleSpace:
        if (lc->extUseANSI)
            {
            fprintf(fcb, "\n0");
            }
        else
            {
            fprintf(fcb, "\n\n");
            }
        break;

    case FcPrintMoveChannel7:
        if (lc->extUseANSI)
            {
            fprintf(fcb, "\n ");
            }
        else
            {
            fprintf(fcb, "\n");
            }
        break;

    case FcPrintMoveTOF:
        if (lc->extUseANSI)
            {
            fprintf(fcb, "\n1");
            }
        else
            {
            fprintf(fcb, "\f");
            }
        break;

    case FcPrintPrint:
        if (lc->extUseANSI)
            {
            fprintf(fcb, "\n ");
            }
        else
            {
            fprintf(fcb, "\n");
            }
        break;

    case FcPrintSuppressLF:
        if (lc->extUseANSI)
            {
            fprintf(fcb, "\n+");
            }
        else
            {
            fprintf(fcb, "\r");
            }

        return (FcProcessed);

    case FcPrintStatusReq:
        activeChannel->status = StPrintReady;
        break;

    case FcPrintClearFormat:
    case FcPrintFormat1:
    case FcPrintFormat2:
    case FcPrintFormat3:
    case FcPrintFormat4:
    case FcPrintFormat5:
    case FcPrintFormat6:
        break;
        }

    activeDevice->fcode = funcCode;

    return (FcAccepted);
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
    FILE *fcb = activeDevice->fcb[0];

    //  SZoppi: this can happen if something goes wrong in the open
    //          and the file fails to be properly re-opened.
    if (activeDevice->fcb[0] == NULL)
        {
        fprintf(stderr, "(lp1612 ) lp1612Io: FCB is Null on channel %o equipment %o\n",
                activeDevice->channel->id,
                activeDevice->eqNo);

        return;
        }

    switch (activeDevice->fcode)
        {
    default:
    case FcPrintSelect:
    case FcPrintSingleSpace:
    case FcPrintDoubleSpace:
    case FcPrintMoveChannel7:
    case FcPrintMoveTOF:
    case FcPrintPrint:
    case FcPrintSuppressLF:
    case FcPrintClearFormat:
    case FcPrintFormat1:
    case FcPrintFormat2:
    case FcPrintFormat3:
    case FcPrintFormat4:
    case FcPrintFormat5:
    case FcPrintFormat6:
        if (activeChannel->full)
            {
            fputc(extBcdToAscii[activeChannel->data & 077], fcb);
            activeChannel->full = FALSE;
            }
        break;

    case FcPrintStatusReq:
        activeChannel->data   = activeChannel->status;
        activeChannel->full   = TRUE;
        activeDevice->fcode   = 0;
        activeChannel->status = 0;
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
    FILE *fcb = activeDevice->fcb[0];

    //  SZoppi: this can happen if something goes wrong in the open
    //          and the file fails to be properly re-opened.
    if (activeDevice->fcb[0] == NULL)
        {
        fprintf(stderr, "(lp1612 ) lp1612Disconnect: FCB is Null on channel %o equipment %o\n",
                activeDevice->channel->id,
                activeDevice->eqNo);

        return;
        }

    LpContext1612 *lc;

    lc = (LpContext1612 *)activeDevice->context[0];

    if (lc->extUseANSI)
        {
        fprintf(fcb, "\n ");
        }
    else
        {
        fputc('\n', fcb);
        }
    }

/*---------------------------  End Of File  ------------------------------*/
