/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**
**  Name: cr405b.c
**
**  Description:
**      Perform emulation of channel-connected CDC 405-B card reader.
**      It does not use a 3000 series channel converter.
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
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#else
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
**  Function codes.
*/
#define FcCr405Deselect         00700
#define FcCr405GateToSec        00701
#define FcCr405ReadNonStop      00702
#define FcCr405StatusReq        00704

/*
**  Status codes.
*/
#define StCr405Ready            00000
#define StCr405NotReady         00001
#define StCr405EOF              00002
#define StCr405CompareErr       00004

#define Cr405MaxDecks           10

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
typedef struct cr405Context
    {
    const u16 *table;
    u32     getCardCycle;
    int     col;
    PpWord  card[80];
    int     inDeck;
    int     outDeck;
    char    *decks[Cr405MaxDecks];
    } Cr405Context;

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static FcStatus cr405Func(PpWord funcCode);
static void cr405Io(void);
static void cr405Activate(void);
static void cr405Disconnect(void);
static void cr405NextCard (DevSlot *dp);
static void cr405StartNextDeck(DevSlot *up, Cr405Context *cc);

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
**  Purpose:        Initialise card reader.
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
void cr405Init(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName)
    {
    Cr405Context *cc;
    DevSlot *dp;

    (void)deviceName;

    if (eqNo != 0)
        {
        fprintf(stderr, "Invalid equipment number - CR405 is hardwired to equipment number 0\n");
        exit(1);
        }

    if (unitNo != 0)
        {
        fprintf(stderr, "Invalid unit number - CR405 is hardwired to unit number 0\n");
        exit(1);
        }

    dp = channelAttach(channelNo, eqNo, DtCr405);

    dp->activate = cr405Activate;
    dp->disconnect = cr405Disconnect;
    dp->func = cr405Func;
    dp->io = cr405Io;
    dp->selectedUnit = 0;

    /*
    **  Only one card reader unit is possible per equipment.
    */
    if (dp->context[0] != NULL)
        {
        fprintf(stderr, "Only one CR405 unit is possible per equipment\n");
        exit(1);
        }

    cc = calloc(1, sizeof(Cr405Context));
    if (cc == NULL)
        {
        fprintf(stderr, "Failed to allocate CR405 context block\n");
        exit(1);
        }

    dp->context[0] = (void *)cc;

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

    cc->col = 80;

    /*
    **  Print a friendly message.
    */
    printf("CR405 initialised on channel %o\n", channelNo);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Load cards on 405 card reader.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cr405LoadCards(char *fname, int channelNo, int equipmentNo)
    {
    Cr405Context *cc;
    DevSlot *dp;
    int len;
    char *sp;

    /*
    **  Locate the device control block.
    */
    dp = channelFindDevice((u8)channelNo, DtCr405);
    if (dp == NULL)
        {
        return;
        }

    cc = (Cr405Context *) (dp->context[0]);

    /*
    **  Ensure the tray is not full.
    */
    if (((cc->inDeck + 1) % Cr405MaxDecks) == cc->outDeck)
        {
        printf("Input tray full\n");
        return;
        }
    len = strlen(fname) + 1;
    sp = (char *)malloc(len);
    memcpy(sp, fname, len);
    cc->decks[cc->inDeck] = sp;
    cc->inDeck = (cc->inDeck + 1) % Cr405MaxDecks;

    if (dp->fcb[0] == NULL)
        {
        cr405StartNextDeck(dp, cc);
        }

    printf("Cards loaded on card reader C%o,E%o\n", channelNo, equipmentNo);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Execute function code on 405 card reader.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        FcStatus
**
**------------------------------------------------------------------------*/
static FcStatus cr405Func(PpWord funcCode)
    {
    switch (funcCode)
        {
    default:
        return(FcDeclined);

    case FcCr405Deselect:
    case FcCr405GateToSec:
        activeDevice->fcode = 0;
        return(FcProcessed);

    case FcCr405ReadNonStop:
    case FcCr405StatusReq:
        activeDevice->fcode = funcCode;
        break;
        }

    return(FcAccepted);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Perform I/O on 405 card reader.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cr405Io(void)
    {
    Cr405Context *cc = activeDevice->context[0];

    switch (activeDevice->fcode)
        {
    default:
    case FcCr405Deselect:
    case FcCr405GateToSec:
        break;

    case FcCr405StatusReq:
        if (activeDevice->fcb[0] == NULL && cc->col >= 80)
            {
            activeChannel->data = StCr405NotReady;
            }
        else
            {
            activeChannel->data = StCr405Ready;
            }
        activeChannel->full = TRUE;
        break;

    case FcCr405ReadNonStop:
        /*
        **  Simulate card in motion for 20 major cycles.
        */
        if (cycles - cc->getCardCycle < 20)
            {
            break;
            }

        if (activeChannel->full)
            {
            break;
            }

        activeChannel->data = cc->card[cc->col++] & Mask12;
        activeChannel->full = TRUE;

        if (cc->col >= 80)
            {
            cr405NextCard(activeDevice);
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
static void cr405Activate(void)
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
static void cr405Disconnect(void)
    {
    }

/*--------------------------------------------------------------------------
**  Purpose:        Start reading next card deck.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cr405StartNextDeck(DevSlot *dp, Cr405Context *cc)
    {
    char *fname;

    while (cc->outDeck != cc->inDeck)
        {
        fname = cc->decks[cc->outDeck];
        dp->fcb[0] = fopen(fname, "r");
        if (dp->fcb[0] != NULL)
            {
            cr405NextCard(dp);
            return;
            }
        printf("Failed to open card deck %s\n", fname);
        unlink(fname);
        free(fname);
        cc->outDeck = (cc->outDeck + 1) % Cr405MaxDecks;
        }
    dp->fcb[0] = NULL;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Read next card, update card reader status.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cr405NextCard(DevSlot *dp)
    {
    Cr405Context *cc = dp->context[0];
    static char buffer[322];
    bool binaryCard;
    char *cp;
    char c;
    int value;
    int i;
    int j;

    if (dp->fcb[0] == NULL)
        {
        return;
        }
    
    /* 
    **  Initialise read.
    */
    cc->getCardCycle = cycles;
    cc->col = 0;
    binaryCard = FALSE;

    /*
    **  Read the next card.
    */
    cp = fgets(buffer, sizeof(buffer), dp->fcb[0]);
    if (cp == NULL)
        {
        /*
        **  If the last card wasn't a 6/7/8/9 card, fake one.
        */
        if (cc->card[0] != 00017)
            {
            memset(cc->card, 0, sizeof(cc->card));
            cc->card[0] = 00017;
            }
        else
            {
            cc->col = 80;
            }

        fclose(dp->fcb[0]);
        dp->fcb[0] = NULL;
        unlink(cc->decks[cc->outDeck]);
        free(cc->decks[cc->outDeck]);
        cc->outDeck = (cc->outDeck + 1) % Cr405MaxDecks;
        cr405StartNextDeck(dp, cc);
        return;
        }

    /*
    **  Deal with special first-column codes.
    */
    if (buffer[0] == '~')
        {
        if (memcmp(buffer + 1, "eoi\n", 4) == 0)
            {
            /*
            **  EOI = 6/7/8/9 card.
            */
            memset(cc->card, 0, sizeof(cc->card));
            cc->card[0] = 00017;
            return;
            }

        if (memcmp(buffer + 1, "eof\n", 4) == 0)
            {
            /*
            **  EOF = 6/7/9 card.
            */
            memset(cc->card, 0, sizeof(cc->card));
            cc->card[0] = 00015;
            return;
            }

        if (memcmp(buffer + 1, "eor\n", 4) == 0)
            {
            /*
            **  EOR = 7/8/9 card.
            */
            memset(cc->card, 0, sizeof(cc->card));
            cc->card[0] = 00007;
            return;
            }

        if (memcmp(buffer + 1, "bin", 3) == 0)
            {
            /*
            **  Binary = 7/9 card.
            */
            binaryCard = TRUE;
            cc->card[0] = 00005;
            }
        }

    /*
    **  Convert cards.
    */
    if (!binaryCard)
        {
        /*
        **  Skip over any characters past column 80 (if line is longer).
        */
        if ((cp = strchr(buffer, '\n')) == NULL)
            {
            do 
                {
                c = fgetc(dp->fcb[0]);
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
        **  Convert ASCII card.
        */
        for (i = 0; i < 80; i++)
            {
            cc->card[i] = cc->table[buffer[i]];
            }
        }
    else
        {
        /*
        **  Skip over any characters past column 320 (if line is longer).
        */
        if ((cp = strchr(buffer, '\n')) == NULL)
            {
            do 
                {
                c = fgetc(dp->fcb[0]);
                } while (c != '\n' && c != EOF);
            cp = buffer + 320;
            }

        /*
        **  Zero fill line shorter then 320 characters.
        */
        for ( ; cp < buffer + 320; cp++)
            {
            *cp = '0';
            }

        /*
        **  Convert binary card (79 x 4 octal digits).
        */
        cp = buffer + 4;
        for (i = 1; i < 80; i++)
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

/*---------------------------  End Of File  ------------------------------*/
