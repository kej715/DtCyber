/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Paul Koning, Tom Hunter
**
**  Name: cr3447.c
**
**  Description:
**      Perform emulation of CDC 3447 card reader controller.
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
#include "const.h"
#include "types.h"
#include "proto.h"
#include "dcc6681.h"

/*
**  -----------------
**  Private Constants
**  -----------------
*/

/*
**  CDC 3447 card reader function and status codes.
*/
#define FcCr3447Deselect         00000
#define FcCr3447Binary           00001
#define FcCr3447BCD              00002
#define FcCr3447SetGateCard      00004
#define FcCr3447Clear            00005
#define FcCr3447IntReady         00020
#define FcCr3447NoIntReady       00021
#define FcCr3447IntEoi           00022
#define FcCr3447NoIntEoi         00023
#define FcCr3447IntError         00024
#define FcCr3447NoIntError       00025

/*
**      Status reply flags
**
**      0001 = Ready
**      0002 = Busy
**      0004 = Binary card (7/9 punch)
**      0010 = File card (7/8 punch)
**      0020 = Jam
**      0040 = Input tray empty
**      0100 = End of file
**      0200 = Ready interrupt
**      0400 = EOI interrupt
**      1000 = Error interrupt
**      2000 = Read compare error
**      4000 = Reserved by other controller (3649 only)
**
*/
#define StCr3447Ready            00201  // includes ReadyInt
#define StCr3447Busy             00002
#define StCr3447Binary           00004
#define StCr3447File             00010
#define StCr3447Empty            00040
#define StCr3447Eof              01540  // includes Empty, EoiInt, ErrorInt
#define StCr3447ReadyInt         00200
#define StCr3447EoiInt           00400
#define StCr3447ErrorInt         01000
#define StCr3447CompareErr       02000
#define StCr3447NonIntStatus     02177

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
    const u16 *table;
    u32     getcardcycle;
    PpWord  card[80];
    } CrContext;

    
/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static FcStatus cr3447Func(PpWord funcCode);
static void cr3447Io(void);
static void cr3447Activate(void);
static void cr3447Disconnect(void);
static void cr3447NextCard(DevSlot *up, CrContext *cc);
static char *cr3447Func2String(PpWord funcCode);

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
static FILE *cr3447Log = NULL;
#endif

/*
**--------------------------------------------------------------------------
**
**  Public Functions
**
**--------------------------------------------------------------------------
*/
/*--------------------------------------------------------------------------
**  Purpose:        Initialise card reader.
**
**  Parameters:     Name        Description.
**                  eqNo        equipment number
**                  unitCount   number of units to initialise
**                  channelNo   channel number the device is attached to
**                  deviceName  optional card source file name, "026" (default)
**                              or "029" to select translation mode
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cr3447Init(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName)
    {
    DevSlot *up;
    CrContext *cc;
    
#if DEBUG
    if (cr3447Log == NULL)
        {
        cr3447Log = fopen("cr3447log.txt", "wt");
        }
#endif

    up = dcc6681Attach(channelNo, eqNo, 0, DtCr3447);

    up->activate = cr3447Activate;
    up->disconnect = cr3447Disconnect;
    up->func = cr3447Func;
    up->io = cr3447Io;

    /*
    **  Only one card reader unit is possible per equipment.
    */
    if (up->context[0] != NULL)
        {
        fprintf(stderr, "Only one CR3447 unit is possible per equipment\n");
        exit (1);
        }

    cc = calloc(1, sizeof (CrContext));
    if (cc == NULL)
        {
        fprintf(stderr, "Failed to allocate CR3447 context block\n");
        exit (1);
        }

    up->context[0] = (void *)cc;

    /*
    **  Setup character set translation table.
    */
    cc->table = asciiTo026;     // default translation table
    if (deviceName != NULL)
        {
        if (strcmp(deviceName, "029") == 0)
            {
            cc->table = asciiTo029;
            }
        else if (strcmp(deviceName, "026") != 0)
            {
            fprintf(stderr, "Unrecognized card code name %s\n", deviceName);
            exit(1);
            }
        }

    /*
    **  Print a friendly message.
    */
    printf("CR3447 initialised on channel %o equipment %o\n", channelNo, eqNo);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Load cards on 3447 card reader.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cr3447LoadCards(char *params)
    {
    CrContext *cc;
    DevSlot *dp;
    int numParam;
    int channelNo;
    int equipmentNo;
    FILE *fcb;
    static char str[200];

    /*
    **  Operator wants to load new card stack.
    */
    numParam = sscanf(params,"%o,%o,%s",&channelNo, &equipmentNo, str);

    /*
    **  Check parameters.
    */
    if (numParam != 3)
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

    if (str[0] == 0)
        {
        printf("Invalid file name\n");
        return;
        }

    /*
    **  Locate the device control block.
    */
    dp = dcc6681FindDevice((u8)channelNo, (u8)equipmentNo, DtCr3447);
    if (dp == NULL)
        {
        return;
        }

    cc = (CrContext *) (dp->context[0]);

    /*
    **  Ensure the tray is empty.
    */
    if (dp->fcb[0] != NULL)
        {
        printf("Input tray full\n");
        return;
        }

    dp->fcb[0] = NULL;
    cc->status = StCr3447Eof;

    fcb = fopen(str, "r");

    /*
    **  Check if the open succeeded.
    */
    if (fcb == NULL)
        {
        printf("Failed to open %s\n", str);
        return;
        }

    dp->fcb[0] = fcb;
    cc->status = StCr3447Ready;
    cr3447NextCard(dp, cc);

    activeDevice = channelFindDevice((u8)channelNo, DtDcc6681);
    dcc6681Interrupt((cc->status & cc->intmask) != 0);

    printf("CR3447 loaded with %s", str);
    }

/*
**--------------------------------------------------------------------------
**
**  Private Functions
**
**--------------------------------------------------------------------------
*/

/*--------------------------------------------------------------------------
**  Purpose:        Execute function code on 3447 card reader.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        FcStatus
**
**------------------------------------------------------------------------*/
static FcStatus cr3447Func(PpWord funcCode)
    {
    CrContext *cc;
    FcStatus st;
    
#if DEBUG
    fprintf(cr3447Log, "\n%06d PP:%02o CH:%02o f:%04o T:%-25s  >   ",
        traceSequenceNo,
        activePpu->id,
        activeDevice->channel->id,
        funcCode,
        cr3447Func2String(funcCode));
#endif

    cc = (CrContext *)active3000Device->context[0];

    switch (funcCode)
        {
    default:                    // all unrecognized codes are NOPs
#if DEBUG
        fprintf(cr3447Log, " FUNC not implemented & silently ignored!");
#endif
        st = FcProcessed;
        break;

    case FcCr3447SetGateCard:
        st = FcProcessed;
        break;

    case Fc6681InputToEor:
    case Fc6681Input:
        st = FcAccepted;
        cc->getcardcycle = cycles;
        cc->status = StCr3447Ready;
        active3000Device->fcode = funcCode;
        break;

    case Fc6681DevStatusReq:
        st = FcAccepted;
        active3000Device->fcode = funcCode;
        break;

    case FcCr3447Binary:
        cc->binary = TRUE;
        st = FcProcessed;
        break;
        
    case FcCr3447Deselect:
    case FcCr3447Clear:
        cc->intmask = 0;
        cc->binary = FALSE;
        st = FcProcessed;
        break;
        
    case FcCr3447BCD:
        cc->binary = FALSE;
        st = FcProcessed;
        break;
        
    case FcCr3447IntReady:
        cc->intmask |= StCr3447ReadyInt;
        cc->status &= ~StCr3447ReadyInt;
        st = FcProcessed;
        break;
        
    case FcCr3447NoIntReady:
        cc->intmask &= ~StCr3447ReadyInt;
        cc->status &= ~StCr3447ReadyInt;
        st = FcProcessed;
        break;
        
    case FcCr3447IntEoi:
        cc->intmask |= StCr3447EoiInt;
        cc->status &= ~StCr3447EoiInt;
        st = FcProcessed;
        break;
        
    case FcCr3447NoIntEoi:
        cc->intmask &= ~StCr3447EoiInt;
        cc->status &= ~StCr3447EoiInt;
        st = FcProcessed;
        break;

    case FcCr3447IntError:
        cc->intmask |=StCr3447ErrorInt;
        cc->status &= ~StCr3447ErrorInt;
        st = FcProcessed;
        break;

    case FcCr3447NoIntError:
        cc->intmask &= ~StCr3447ErrorInt;
        cc->status &= ~StCr3447ErrorInt;
        st = FcProcessed;
        break;
        }

    dcc6681Interrupt((cc->status & cc->intmask) != 0);
    return(st);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Perform I/O on 3447 card reader.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cr3447Io(void)
    {
    CrContext *cc;
    PpWord c;
    
    cc = (CrContext *)active3000Device->context[0];

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
            activeChannel->data = (cc->status & (cc->intmask | StCr3447NonIntStatus));
            activeChannel->full = TRUE;
#if DEBUG
            fprintf(cr3447Log, " %04o", activeChannel->data);
#endif
            }
        break;
        
    case Fc6681InputToEor:
    case Fc6681Input:
        // Don't admit to having new data immediately after completing
        // a card, otherwise 1CD may get stuck occasionally.
        // So we simulate card in motion for 20 major cycles.
        if (   activeChannel->full
            || cycles - cc->getcardcycle < 20)
            {
            break;
            }

        if (active3000Device->fcb[0] == NULL)
            {
            cc->status = StCr3447Eof;
            break;
            }
        
        if (cc->col >= 80)
            {
            // Read the next card.
            // If the function is input to EOR, disconnect to indicate EOR
            cr3447NextCard (active3000Device, cc);
            if (activeDevice->fcode == Fc6681InputToEor)
                {
                // End of card but we're still ready
                cc->status |= StCr3447EoiInt | StCr3447Ready;
                if (cc->status & StCr3447File)
                    {
                    cc->status |= StCr3447ErrorInt;
                    }

                activeChannel->discAfterInput = TRUE;
                }
            }
        else
            {
            c = cc->card[cc->col++];
            if (cc->rawcard)
                {
                activeChannel->data = c;
                }
            else if (cc->binary)
                {
                activeChannel->data = cc->table[c];
                }
            else
                {
                activeChannel->data = asciiToBcd[c] << 6;
                c = cc->card[cc->col++];
                activeChannel->data += asciiToBcd[c];
                }

            activeChannel->full = TRUE;
#if DEBUG
            fprintf(cr3447Log, " %04o", activeChannel->data);
#endif
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
static void cr3447Activate(void)
    {
#if DEBUG
    fprintf(cr3447Log, "\n%06d PP:%02o CH:%02o Activate",
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
static void cr3447Disconnect(void)
    {
    CrContext *cc;
    
#if DEBUG
    fprintf(cr3447Log, "\n%06d PP:%02o CH:%02o Disconnect",
        traceSequenceNo,
        activePpu->id,
        activeDevice->channel->id);
#endif

    /*
    **  Abort pending device disconnects - the PP is doing the disconnect.
    */
    activeChannel->discAfterInput = FALSE;

    /*
    **  Advance to next card.
    */
    cc = (CrContext *)active3000Device->context[0];
    if (cc != NULL)
        {
        cc->status |= StCr3447EoiInt;
        dcc6681Interrupt((cc->status & cc->intmask) != 0);
        if (active3000Device->fcb[0] != NULL && cc->col != 0)
            {
            cr3447NextCard(active3000Device, cc);
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Read next card, update card reader status.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cr3447NextCard (DevSlot *up, CrContext *cc)
    {
    static char buffer[326];
    char *cp;
    char c;
    int value;
    int i;
    int j;
    PpWord col1;

    /* 
    **  Initialise read.
    */
    cc->getcardcycle = cycles;
    cc->col = 0;
    cc->rawcard = FALSE;

    /*
    **  Read the next card.
    */
    cp = fgets(buffer, sizeof(buffer), up->fcb[0]);
    if (cp == NULL)
        {
        /*
        **  If the last card wasn't a 6/7/8/9 card, fake one.
        */
        if (cc->card[0] != 00017)
            {
            cc->rawcard = TRUE;//    ???? what if we don't read binary (i.e. cc->binary)
            cc->status |= StCr3447Binary;
            memset(cc->card, 0, sizeof(cc->card));
            cc->card[0] = 00017;
            return;
            }

        fclose(up->fcb[0]);
        up->fcb[0] = NULL;
        cc->status = StCr3447Eof;
        return;
        }

    /*
    **  Deal with special first-column codes.
    */
    if (buffer[0] == '}')
        {
        /*
        **  EOI = 6/7/8/9 card.
        */
        cc->rawcard = TRUE;
        cc->status |= StCr3447Binary;
        memset(cc->card, 0, sizeof(cc->card));
        cc->card[0] = 00017;
        return;
        }

    if (buffer[0] == '~')
        {
        if (strcmp(buffer + 1, "eoi\n") == 0)
            {
            /*
            **  EOI = 6/7/8/9 card.
            */
            cc->rawcard = TRUE;
            cc->status |= StCr3447Binary;
            memset(cc->card, 0, sizeof(cc->card));
            cc->card[0] = 00017;
            return;
            }

        if (strcmp(buffer + 1, "eof\n") == 0)
            {
            /*
            **  EOF = 6/7/9 card.
            */
            cc->rawcard = TRUE;
            cc->status |= StCr3447Binary;
            memset(cc->card, 0, sizeof(cc->card));
            cc->card[0] = 00015;
            return;
            }

        if (strcmp(buffer + 1, "eor\n") == 0 || buffer[1] == '\n' || buffer[1] == ' ')
            {
            /*
            **  EOR = 7/8/9 card.
            */
            cc->rawcard = TRUE;
            cc->status |= StCr3447Binary;
            memset(cc->card, 0, sizeof(cc->card));
            cc->card[0] = 00007;
            return;
            }

        if (memcmp(buffer + 1, "raw", 3) == 0)
            {
            /*
            **  Raw binary card.
            */
            cc->rawcard = TRUE;
            col1 = buffer[4] & Mask5;
            if (col1 == 00005)
                {
                cc->status |= StCr3447Binary;
                }
            else if (col1 == 00006 && !cc->binary)
                {
                cc->status |= StCr3447File;
                }
            }
        }

    if (!cc->rawcard)
        {
        /*
        **  Skip over any characters past column 80 (if line is longer).
        */
        if ((cp = strchr(buffer, '\n')) == NULL)
            {
            do 
                {
                c = fgetc(up->fcb[0]);
                } while (c != '\n' && c != EOF);
            cp = buffer + 80;
            }

        /*
        **  Blank fill line shorter then 80 characters.
        */
        for ( ; cp < buffer + 80; cp++)
            {
            *cp = ' ';
            }

        /*
        **  Transfer buffer.
        */
        for (i = 0; i < 80; i++)
            {
            cc->card[i] = buffer[i];
            }
        }
    else
        {
        /*
        **  Skip over any characters past column 324 (if line is longer).
        */
        if ((cp = strchr(buffer, '\n')) == NULL)
            {
            do 
                {
                c = fgetc(up->fcb[0]);
                } while (c != '\n' && c != EOF);
            cp = buffer + 324;
            }

        /*
        **  Zero fill line shorter then 320 characters.
        */
        for ( ; cp < buffer + 324; cp++)
            {
            *cp = '0';
            }

        /*
        **  Convert raw binary card (80 x 4 octal digits).
        */
        cp = buffer + 4;
        for (i = 0; i < 80; i++)
            {
            value = 0;
            for (j = 0; j < 4; j++)
                {
                if (cp[j] >= '0' && cp[j] <= '7')
                    {
                    value = (value << 3) | (cp[j] - '0');
                    }
                else
                    {
                    value = 0;
                    break;
                    }
                }

            cc->card[i] = value;

            cp += 4;
            }
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
static char *cr3447Func2String(PpWord funcCode)
    {
    static char buf[30];
#if DEBUG
    switch(funcCode)
        {
    case FcCr3447Deselect             : return "Deselect";
    case FcCr3447Binary               : return "Binary";
    case FcCr3447BCD                  : return "BCD";
    case FcCr3447SetGateCard          : return "SetGateCard";
    case FcCr3447Clear                : return "Clear";
    case FcCr3447IntReady             : return "IntReady";
    case FcCr3447NoIntReady           : return "NoIntReady";
    case FcCr3447IntEoi               : return "IntEoi";
    case FcCr3447NoIntEoi             : return "NoIntEoi";
    case FcCr3447IntError             : return "IntError";
    case FcCr3447NoIntError           : return "NoIntError";
    case Fc6681DevStatusReq           : return "6681DevStatusReq";
    case Fc6681InputToEor             : return "6681InputToEor";
    case Fc6681Input                  : return "6681Input";
        }
#endif
    sprintf(buf, "UNKNOWN: %04o", funcCode);
    return(buf);
    }

/*---------------------------  End Of File  ------------------------------*/
