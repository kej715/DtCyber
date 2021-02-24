/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
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

#define FcPrintSelect           00600
#define FcPrintSingleSpace      00601
#define FcPrintDoubleSpace      00602
#define FcPrintMoveChannel7     00603
#define FcPrintMoveTOF          00604
#define FcPrintPrint            00605
#define FcPrintSuppressLF       00606
#define FcPrintStatusReq        00607
#define FcPrintClearFormat      00610
#define FcPrintFormat1          00611
#define FcPrintFormat2          00612
#define FcPrintFormat3          00613
#define FcPrintFormat4          00614
#define FcPrintFormat5          00615
#define FcPrintFormat6          00616

/*
**      Status reply
**
**      0000 = Not Ready
**      4000 = Ready
**
*/
#define StPrintReady            04000
#define StPrintNotReady         00000

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
    DevSlot *dp;
    char fname[80];

    (void)deviceName;

    if (eqNo != 0)
        {
        fprintf(stderr, "Invalid equipment number - LP1612 is hardwired to equipment number 0\n");
        exit(1);
        }

    if (unitNo != 0)
        {
        fprintf(stderr, "Invalid unit number - LP1612 is hardwired to unit number 0\n");
        exit(1);
        }

    dp = channelAttach(channelNo, eqNo, DtLp1612);

    dp->activate = lp1612Activate;
    dp->disconnect = lp1612Disconnect;
    dp->func = lp1612Func;
    dp->io = lp1612Io;
    dp->selectedUnit = 0;

    /*
    **  Open the device file.
    */
    sprintf(fname, "LP1612_C%02o", channelNo);
    dp->fcb[0] = fopen(fname, "w+t");
    if (dp->fcb[0] == NULL)
        {
        fprintf(stderr, "Failed to open %s\n", fname);
        exit(1);
        }

    /*
    **  Print a friendly message.
    */
    printf("LP1612 initialised on channel %o\n", channelNo);
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
    DevSlot *dp;
    int numParam;
    int channelNo;
    int equipmentNo;
    time_t currentTime;
    struct tm t;
    char fname[80];
    char fnameNew[80];

    /*
    **  Operator wants to remove paper.
    */
    numParam = sscanf(params,"%o,%o",&channelNo, &equipmentNo);

    /*
    **  Check parameters.
    */
    if (numParam != 2)
        {
        printf("Not enough or invalid parameters\n");
        return;
        }

    if (channelNo < 0 || channelNo >= MaxChannels)
        {
        printf("Invalid channel no\n");
        return;
        }

    if (equipmentNo < 0 || equipmentNo >= MaxEquipment)
        {
        printf("Invalid equipment no\n");
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

    /*
    **  Close the old device file.
    */
    fflush(dp->fcb[0]);
    fclose(dp->fcb[0]);
    dp->fcb[0] = NULL;

    /*
    **  Rename the device file to the format "LP1612_yyyymmdd_hhmmss".
    */
    sprintf(fname, "LP1612_C%02o", channelNo);

    time(&currentTime);
    t = *localtime(&currentTime);
    sprintf(fnameNew, "LP1612_%04d%02d%02d_%02d%02d%02d",
        t.tm_year + 1900,
        t.tm_mon + 1,
        t.tm_mday,
        t.tm_hour,
        t.tm_min,
        t.tm_sec);

    if (rename(fname, fnameNew) != 0)
        {
        printf("Could not rename %s to %s - %s\n", fname, fnameNew, strerror(errno));
        return;
        }

    /*
    **  Open the device file.
    */
    dp->fcb[0] = fopen(fname, "w");

    /*
    **  Check if the open succeeded.
    */
    if (dp->fcb[0] == NULL)
        {
        printf("Failed to open %s\n", fname);
        return;
        }

    printf("Paper removed from 1612 printer\n");
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
    FILE *fcb = activeDevice->fcb[0];

    switch (funcCode)
        {
    default:
        return(FcDeclined);

    case FcPrintSelect:
        break;

    case FcPrintSingleSpace:
        fprintf(fcb, "\n");
        break;

    case FcPrintDoubleSpace:
        fprintf(fcb, "\n\n");
        break;

    case FcPrintMoveChannel7:
        fprintf(fcb, "\n");
        break;

    case FcPrintMoveTOF:
        fprintf(fcb, "\f");
        break;

    case FcPrintPrint:
        fprintf(fcb, "\n");
        break;

    case FcPrintSuppressLF:
        return(FcProcessed);

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
    return(FcAccepted);
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
        activeChannel->data = activeChannel->status;
        activeChannel->full = TRUE;
        activeDevice->fcode = 0;
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
    }

/*---------------------------  End Of File  ------------------------------*/
