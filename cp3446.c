/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Paul Koning, Tom Hunter
**
**  Name: cp3446.c
**
**  Description:
**      Perform emulation of CDC 3446 card punch controller.
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
#include <errno.h>
#include <string.h>
#include <time.h>
#include "const.h"
#include "types.h"
#include "proto.h"
#include "dcc6681.h"

/*
**  -----------------
**  Private Constants
**  -----------------
*/

#define CP_LC   0

/*
**  CDC 3446 card punch function and status codes.
*/
#define FcCp3446Deselect         00000
#define FcCp3446Binary           00001
#define FcCp3446BCD              00002
#define FcCp3446SelectOffset     00003
#define FcCp3446CheckLastCard    00004
#define FcCp3446Clear            00005
#define FcCp3446IntReady         00020
#define FcCp3446NoIntReady       00021
#define FcCp3446IntEoi           00022
#define FcCp3446NoIntEoi         00023
#define FcCp3446IntError         00024
#define FcCp3446NoIntError       00025

/*
**      Status reply flags
**
**      0001 = Ready
**      0002 = Busy
**      0100 = Failed to feed
**      0200 = Ready interrupt
**      0400 = EOI interrupt
**      1000 = Error interrupt
**      2000 = Compare error
**      4000 = Reserved by other controller (3644 only)
**
*/
#define StCp3446Ready            00201  // includes ReadyInt
#define StCp3446Busy             00002
#define StCp3446ReadyInt         00200
#define StCp3446EoiInt           00400
#define StCp3446ErrorInt         01000
#define StCp3446CompareErr       02000
#define StCp3446NonIntStatus     02177

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

typedef struct
    {
    bool    binary;
    bool    rawcard;
    int     intmask;
    int     status;
    int     col;
    int     lastnbcol;
    char    convtable[4096];
    u32     getcardcycle;
    char    card[322];
    } CpContext;

    
/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static FcStatus cp3446Func(PpWord funcCode);
static void cp3446Io(void);
static void cp3446Activate(void);
static void cp3446Disconnect(void);
static void cp3446FlushCard(DevSlot *up, CpContext *cc);
static char *cp3446Func2String(PpWord funcCode);

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

#if DEBUG
static FILE *cp3446Log = NULL;
#endif

/*
**--------------------------------------------------------------------------
**
**  Public Functions
**
**--------------------------------------------------------------------------
*/
/*--------------------------------------------------------------------------
**  Purpose:        Initialise card punch.
**
**  Parameters:     Name        Description.
**                  eqNo        equipment number
**                  unitCount   number of units to initialise
**                  channelNo   channel number the device is attached to
**                  deviceName  optional card output file name, 
**                              may be followed by comma and"026" (default)
**                              or "029" to select translation mode
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cp3446Init(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName)
    {
    DevSlot *up;
    char fname[80];
    CpContext *cc;
    const PpWord *charset;
    PpWord hol;
    int i;
    
#if DEBUG
    if (cp3446Log == NULL)
        {
        cp3446Log = fopen("cp3446log.txt", "wt");
        }
#endif

    up = dcc6681Attach(channelNo, eqNo, 0, DtCp3446);

    up->activate = cp3446Activate;
    up->disconnect = cp3446Disconnect;
    up->func = cp3446Func;
    up->io = cp3446Io;

    /*
    **  Only one card punch unit is possible per equipment.
    */
    if (up->context[0] != NULL)
        {
        fprintf (stderr, "Only one CP3446 unit is possible per equipment\n");
        exit (1);
        }

    cc = calloc (1, sizeof (CpContext));
    if (cc == NULL)
        {
        fprintf (stderr, "Failed to allocate CP3446 context block\n");
        exit (1);
        }

    up->context[0] = (void *)cc;
    cc->lastnbcol = -1;
    cc->col = 0;
    cc->status = StCp3446Ready;
    
    /*
    **  Open the device file.
    */
    sprintf(fname, "CP3446_C%02o_E%o", channelNo, eqNo);
    up->fcb[0] = fopen(fname, "w");
    if (up->fcb[0] == NULL)
        {
        fprintf(stderr, "Failed to open %s\n", fname);
        exit(1);
        }

    /*
    **  Setup character set translation table.
    */
    charset = asciiTo026;     // default translation table
    if (deviceName != NULL)
        {
        if (strcmp (deviceName, "029") == 0)
            {
            charset = asciiTo029;
            }
        else if (strcmp (deviceName, "026") != 0)
            {
            fprintf (stderr, "Unrecognized card code name %s\n", deviceName);
            exit (1);
            }
        }

    memset(cc->convtable, ' ', sizeof(cc->convtable));
    for (i = 040; i < 0177; i++)
        {
        hol = charset[i] & Mask12;
        if (hol != 0)
            {
            cc->convtable[hol] = i;
            }
        }
    
    /*
    **  Print a friendly message.
    */
    printf("CP3446 initialised on channel %o equipment %o\n", channelNo, eqNo);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Remove cards from 3446 card punch.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cp3446RemoveCards(char *params)
    {
    CpContext *cc;
    DevSlot *dp;
    int numParam;
    int channelNo;
    int equipmentNo;
    time_t currentTime;
    struct tm t;
    char fname[80];
    char fnameNew[80];
    static char msgBuf[80];

    /*
    **  Operator wants to remove cards.
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
    dp = dcc6681FindDevice((u8)channelNo, (u8)equipmentNo, DtCp3446);
    if (dp == NULL)
        {
        printf("No card punch on channel %o and equipment %o\n", channelNo, equipmentNo);
        return;
        }

    /*
    **  Close the old device file.
    */
    cc = (CpContext *) (dp->context[0]);
    cp3446FlushCard (dp, cc);
    fflush(dp->fcb[0]);
    fclose(dp->fcb[0]);
    dp->fcb[0] = NULL;

    /*
    **  Rename the device file to the format "CP3446_yyyymmdd_hhmmss".
    */
    sprintf(fname, "CP3446_C%02o_E%o", channelNo, equipmentNo);

    time(&currentTime);
    t = *localtime(&currentTime);
    sprintf(fnameNew, "CP3446_%04d%02d%02d_%02d%02d%02d",
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

    printf("Punch cards removed from 3446 card puncher\n");
    }

/*
**--------------------------------------------------------------------------
**
**  Private Functions
**
**--------------------------------------------------------------------------
*/

/*--------------------------------------------------------------------------
**  Purpose:        Execute function code on 3446 card punch.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        FcStatus
**
**------------------------------------------------------------------------*/
static FcStatus cp3446Func(PpWord funcCode)
    {
    CpContext *cc;
    FcStatus st;
    
#if DEBUG
    fprintf(cp3446Log, "\n%06d PP:%02o CH:%02o f:%04o T:%-25s  >   ",
        traceSequenceNo,
        activePpu->id,
        activeDevice->channel->id,
        funcCode,
        cp3446Func2String(funcCode));
#endif

    cc = (CpContext *)active3000Device->context[0];

    switch (funcCode)
        {
    default:                    // all unrecognized codes are NOPs
#if DEBUG
        fprintf(cp3446Log, " FUNC not implemented & silently ignored!");
#endif
        st = FcProcessed;
        break;

    case FcCp3446CheckLastCard:
    case FcCp3446SelectOffset:
    case Fc6681MasterClear:
        st = FcProcessed;
        break;

    case Fc6681Output:
        cc->status = StCp3446Ready;
        active3000Device->fcode = funcCode;
        st = FcAccepted;
        break;

    case Fc6681DevStatusReq:
        active3000Device->fcode = funcCode;
        st = FcAccepted;
        break;

    case FcCp3446Binary:
        cc->binary = TRUE;
        st = FcProcessed;
        break;
        
    case FcCp3446Deselect:
    case FcCp3446Clear:
        cc->intmask = 0;
        cc->binary = FALSE;
        st = FcProcessed;
        break;
        
    case FcCp3446BCD:
        cc->binary = FALSE;
        st = FcProcessed;
        break;
        
    case FcCp3446IntReady:
        cc->intmask |= StCp3446ReadyInt;
        cc->status &= ~StCp3446ReadyInt;
        st = FcProcessed;
        break;
        
    case FcCp3446NoIntReady:
        cc->intmask &= ~StCp3446ReadyInt;
        cc->status &= ~StCp3446ReadyInt;
        st = FcProcessed;
        break;
        
    case FcCp3446IntEoi:
        cc->intmask |= StCp3446EoiInt;
        cc->status &= ~StCp3446EoiInt;
        st = FcProcessed;
        break;
        
    case FcCp3446NoIntEoi:
        cc->intmask &= ~StCp3446EoiInt;
        cc->status &= ~StCp3446EoiInt;
        st = FcProcessed;
        break;

    case FcCp3446IntError:
        cc->intmask |=StCp3446ErrorInt;
        cc->status &= ~StCp3446ErrorInt;
        st = FcProcessed;
        break;

    case FcCp3446NoIntError:
        cc->intmask &= ~StCp3446ErrorInt;
        cc->status &= ~StCp3446ErrorInt;
        st = FcProcessed;
        break;
        }

    dcc6681Interrupt((cc->status & cc->intmask) != 0);
    return(st);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Perform I/O on 3446 card punch.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cp3446Io(void)
    {
    CpContext *cc;
    char c;
    PpWord p;
    
    cc = (CpContext *)active3000Device->context[0];

    switch (active3000Device->fcode)
        {
    default:
        printf("unexpected IO for function %04o\n", active3000Device->fcode); 
        break;

    case 0:
        break;

    case Fc6681DevStatusReq:
        if (!activeChannel->full)
            {
            activeChannel->data = (cc->status & (cc->intmask | StCp3446NonIntStatus));
            activeChannel->full = TRUE;
#if DEBUG
            fprintf(cp3446Log, " %04o", activeChannel->data);
#endif
            }
        break;
        
    case Fc6681Output:
        /*
        **  Don't admit to having new data immediately after completing
        **  a card, otherwise 1CD may get stuck occasionally.
        **  So we simulate card in motion for 20 major cycles.
        */
        if (   !activeChannel->full
            || cycles - cc->getcardcycle < 20)
            {
            break;
            }

        if (!cc->rawcard && cc->col >= 80)
            {
            cp3446FlushCard (active3000Device, cc);
            }
        else if (cc->rawcard && cc->col >= (80 * 4))
            {
            cp3446FlushCard (active3000Device, cc);
            }
        else
            {
            p = activeChannel->data & Mask12;
            activeChannel->full = FALSE;
            
#if DEBUG
            fprintf(cp3446Log, " %04o", activeChannel->data);
#endif
            /*
            **  If rows 7 and 9 in column 1 are set and we are in binary mode,
            **  then we have a raw binary card.
            */
            if (cc->col == 0)
                {
                if (cc->binary && (p & Mask5) == 00005)
                    {
                    cc->rawcard = TRUE;
                    }
                else
                    {
                    cc->rawcard = FALSE;
                    }
                }

            if (cc->rawcard)
                {
                sprintf(cc->card + cc->col, "%04o", p);
                cc->col += 4;
                }
            else if (cc->binary)
                {
                c = cc->convtable[p];
#if (CP_LC == 1)
                c = tolower(c);
#endif
                if ((cc->card[cc->col] = c) != ' ')
                    {
                    cc->lastnbcol = cc->col;
                    }

                cc->col++;
                }
            else
                {
                c = bcdToAscii[(p >> 6) & Mask6];
#if (CP_LC == 1)
                c = tolower(c);
#endif
                if ((cc->card[cc->col] = c) != ' ')
                    {
                    cc->lastnbcol = cc->col;
                    }

                cc->col++;

                c = bcdToAscii[(p >> 0) & Mask6];
#if (CP_LC == 1)
                c = tolower(c);
#endif
                if ((cc->card[cc->col] = c) != ' ')
                    {
                    cc->lastnbcol = cc->col;
                    }

                cc->col++;
                }
            }
        break;
        }

    dcc6681Interrupt((cc->status & cc->intmask) != 0);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Handle channel activation.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cp3446Activate(void)
    {
#if DEBUG
    fprintf(cp3446Log, "\n%06d PP:%02o CH:%02o Activate",
        traceSequenceNo,
        activePpu->id,
        activeDevice->channel->id);
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
static void cp3446Disconnect(void)
    {
    CpContext *cc;
    
#if DEBUG
    fprintf(cp3446Log, "\n%06d PP:%02o CH:%02o Disconnect",
        traceSequenceNo,
        activePpu->id,
        activeDevice->channel->id);
#endif

    cc = (CpContext *)active3000Device->context[0];
    if (cc != NULL)
        {
        cc->status |= StCp3446EoiInt;
        dcc6681Interrupt((cc->status & cc->intmask) != 0);
        if (active3000Device->fcb[0] != NULL && cc->col != 0)
            {
            cp3446FlushCard(active3000Device, cc);
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Punch current card, update card punch status.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cp3446FlushCard(DevSlot *up, CpContext *cc)
    {
    int lc;
    
    if (cc->col == 0)
        {
        return;
        }
    
    /*
    **  Remember the cycle counter when the card punch started.
    */
    cc->getcardcycle = cycles;
    
    if (cc->binary && cc->rawcard)
        {
        fputs("~raw", up->fcb[0]);
        lc = cc->col;
        cc->card[lc++] = '\n';
        }
    else
        {
        /*
        **  Omit trailing blanks.
        */
        lc = cc->lastnbcol + 1;
        cc->card[lc++] = '\n';
        }

    /*
    **  Write the card and reset for next card.
    */
    fwrite(cc->card, 1, lc, up->fcb[0]);
    cc->col = 0;
    cc->lastnbcol = -1;
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
static char *cp3446Func2String(PpWord funcCode)
    {
    static char buf[30];
#if DEBUG
    switch(funcCode)
        {
    case FcCp3446Deselect             : return "Deselect";
    case FcCp3446Binary               : return "Binary";
    case FcCp3446BCD                  : return "BCD";
    case FcCp3446SelectOffset         : return "SelectOffset";
    case FcCp3446CheckLastCard        : return "CheckLastCard";
    case FcCp3446Clear                : return "Clear";
    case FcCp3446IntReady             : return "IntReady";
    case FcCp3446NoIntReady           : return "NoIntReady";
    case FcCp3446IntEoi               : return "IntEoi";
    case FcCp3446NoIntEoi             : return "NoIntEoi";
    case FcCp3446IntError             : return "IntError";
    case FcCp3446NoIntError           : return "NoIntError";
    case Fc6681DevStatusReq           : return "6681DevStatusReq";
    case Fc6681Output                 : return "6681Output";
    case Fc6681MasterClear            : return "6681MasterClear";
        }
#endif
    sprintf(buf, "UNKNOWN: %04o", funcCode);
    return(buf);
    }

/*---------------------------  End Of File  ------------------------------*/

