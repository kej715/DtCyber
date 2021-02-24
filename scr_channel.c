/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**
**  Name: scr_channel.c
**
**  Description:
**      Perform emulation of status and control register on channel 16.
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
#ifndef WIN32
#include <unistd.h>
#endif
#include <string.h>
#include "const.h"
#include "types.h"
#include "proto.h"

/*
**  -----------------
**  Private Constants
**  -----------------
*/
#define StatusAndControlWords   021

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
static FcStatus scrFunc(PpWord funcCode);
static void scrIo(void);
static void scrActivate(void);
static void scrDisconnect(void);
static void scrExecute(PpWord func);
static void scrSetBit(PpWord *scrRegister, u16 bit);
static void scrClrBit(PpWord *scrRegister, u16 bit);

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
static FILE *scrLog = NULL;
#endif

/*
**--------------------------------------------------------------------------
**
**  Public Functions
**
**--------------------------------------------------------------------------
*/
/*--------------------------------------------------------------------------
**  Purpose:        Initialise status and control register channel.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void scrInit(u8 channelNo)
    {
    DevSlot *dp;

#if DEBUG
    if (scrLog == NULL)
        {
        scrLog = fopen("scrlog.txt", "wt");
        }
#endif

    dp = channelAttach(channelNo, 0, DtStatusControlRegister);
    dp->activate = scrActivate;
    dp->disconnect = scrDisconnect;
    dp->func = scrFunc;
    dp->io = scrIo;

    channel[channelNo].active = TRUE;
    channel[channelNo].ioDevice = dp;
    channel[channelNo].hardwired = TRUE;

    dp->context[0] = calloc(StatusAndControlWords, sizeof(PpWord));
    if (dp->context[0] == NULL)
        {
        fprintf(stderr, "Failed to allocate Status/Control Register context block\n");
        exit(1);
        }


    /*
    **  Print a friendly message.
    */
    printf("Status/Control Register initialised on channel %o\n", channelNo);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Execute function code on status and control register channel.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        FcStatus
**
**------------------------------------------------------------------------*/
static FcStatus scrFunc(PpWord funcCode)
    {
    return(FcAccepted);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Perform I/O on status and control register channel.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void scrIo(void)
    {
    /*
    **  This function relies on pp.c only calling it when doing an OAN. The
    **  IAN will not block as the response to the SCR function request is
    **  made available immediately (i.e. the channel is full).
    */
    if (!activeChannel->inputPending && activeChannel->full)
        {
        activeChannel->inputPending = TRUE;
        scrExecute(activeChannel->data);
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
static void scrActivate(void)
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
static void scrDisconnect(void)
    {
    }

/*--------------------------------------------------------------------------
**  Purpose:        Execute status and control register request.
**
**  Parameters:     Name        Description.
**                  func        function word
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void scrExecute(PpWord func)
    {
    u8 code;
    u8 designator;
    u8 word;
    u8 bit;
    PpWord *scrRegister = (PpWord *)activeDevice->context[0];

// <<<<<<<<<<<<<<<<<<<< change this to be an array of bytes with:
// value, read_enable, write_enable, status, control (as bits)

    code = (func >> 9) & 7;
    designator = func & 0377;

    /*
    **  If this is a read or test, work out which word is used.
    */
    switch (code)
        {
    case 0:
        /*
        **  Read word.
        */
        word = designator;
        break;

    case 1:
    case 3:
    case 5:
        /*
        **  Test bit.
        **  Test bit and leave clear.
        **  Test bit and leave set.
        */
        word = designator / 12;
        break;

    default:
        word = 0xFF;
        break;
        }

    /*
    **  Set a few dynamic bits.
    */
    switch (word)
        {
    case 05:
        /*
        **  P register of PP selected by bits 170B to 173B only if bit 123B is clear.
        */
        if ((scrRegister[06] & 04000) == 0)
            {
            u8 ppSelectCode = scrRegister[012] & Mask4;

            if (ppSelectCode < 012)
                {
                if (activeChannel->id == ChStatusAndControl)
                    {
                    scrRegister[05] = ppu[ppSelectCode].regP;
                    }
                else
                    {
                    if (ppuCount == 024)
                        {
                        scrRegister[05] = ppu[ppSelectCode + 012].regP;
                        }
                    else
                        {
                        scrRegister[05] = 0;
                        }
                    }
                }
            else
                {
                scrRegister[05] = 0;
                }
            }

        break;

    case 06:
        /*
        **  Locked PP code bits (PP which hit breakpoint). Must be clear to avoid
        **  Mainframe Attribute Determinator from thinking this is a Cyber 176.
        */
        scrRegister[06] &= ~Mask4;
        break;

    case 016:
        if (modelType == ModelCyber865)
            {
            /*
            **  Select appropriate CM configuration quadrants.
            */
            switch (cpuMaxMemory)
                {
            case 01000000:
                scrSetBit(scrRegister, 0260);
                scrClrBit(scrRegister, 0261);
                scrClrBit(scrRegister, 0262);
                scrClrBit(scrRegister, 0263);
                break;

            case 02000000:
                scrSetBit(scrRegister, 0260);
                scrSetBit(scrRegister, 0261);
                scrClrBit(scrRegister, 0262);
                scrClrBit(scrRegister, 0263);
                break;

            case 03000000:
                scrSetBit(scrRegister, 0260);
                scrSetBit(scrRegister, 0261);
                scrSetBit(scrRegister, 0262);
                scrClrBit(scrRegister, 0263);
                break;

            case 04000000:
                scrSetBit(scrRegister, 0260);
                scrSetBit(scrRegister, 0261);
                scrSetBit(scrRegister, 0262);
                scrSetBit(scrRegister, 0263);
                break;

            default:
                scrClrBit(scrRegister, 0260);
                scrClrBit(scrRegister, 0261);
                scrClrBit(scrRegister, 0262);
                scrClrBit(scrRegister, 0263);
                break;
                }
            }

        break;

    case 017:
        if (modelType == ModelCyber865)
            {
            /*
            **  Enable "is a 865 or 875" bit.
            */
            scrSetBit(scrRegister, 0264);

            /*
            **  Disable "is a 875" bit.
            */
            scrClrBit(scrRegister, 0265);

            /*
            **  Disable "has CP1" bit.
            */
            scrClrBit(scrRegister, 0266);
            }

        break;

    case 020:
        if (cpuStopped)
            {
            scrSetBit(scrRegister, 0300);
            }
        else
            {
            scrClrBit(scrRegister, 0300);
            }

        scrClrBit(scrRegister, 0301);

        if (cpu.monitorMode)
            {
            scrSetBit(scrRegister, 0303);
            }
        else
            {
            scrClrBit(scrRegister, 0303);
            }

        scrClrBit(scrRegister, 0304);

        if (modelType == ModelCyber865)
            {
            if ((cpu.exitMode & EmFlagExpandedAddress) != 0)
                {
                scrSetBit(scrRegister, 0312);
                }
            else
                {
                scrClrBit(scrRegister, 0312);
                }
            }

        scrClrBit(scrRegister, 0313);

        break;
        }

    switch (code)
        {
    case 0:
        /*
        **  Read word.
        */
        if (designator < StatusAndControlWords)
            {
            activeChannel->data = scrRegister[designator] & Mask12;
            }
        else
            {
            activeChannel->data = 0;
            }

        break;

    case 1:
        /*
        **  Test bit.
        */
        word = designator / 12;
        bit = designator % 12;

        if (word < StatusAndControlWords)
            {
            activeChannel->data = (scrRegister[word] & (1 << bit)) != 0 ? 1 : 0;
            }
        else
            {
            activeChannel->data = 0;
            }

        break;

    case 2:
        /*
        **  Clear bit.
        */
        word = designator / 12;
        bit = designator % 12;

        if (word < StatusAndControlWords)
            {
            scrRegister[word] &= ~(1 << bit);
            }

        activeChannel->data = 0;
        break;

    case 3:
        /*
        **  Test bit and leave clear.
        */
        word = designator / 12;
        bit = designator % 12;

        if (word < StatusAndControlWords)
            {
            activeChannel->data = (scrRegister[word] & (1 << bit)) != 0 ? 1 : 0;
            scrRegister[word] &= ~(1 << bit);
            }
        else
            {
            activeChannel->data = 0;
            }

        break;

    case 4:
        /*
        **  Set bit.
        */
        word = designator / 12;
        bit = designator % 12;

        if (word < StatusAndControlWords)
            {
            scrRegister[word] |= (1 << bit);
            }

        activeChannel->data = 0;
        break;

    case 5:
        /*
        **  Test bit and leave set.
        */
        word = designator / 12;
        bit = designator % 12;

        if (word < StatusAndControlWords)
            {
            activeChannel->data = (scrRegister[word] & (1 << bit)) != 0 ? 1 : 0;
            scrRegister[word] |= (1 << bit);
            }
        else
            {
            activeChannel->data = 0;
            }

        break;

    case 6:
        /*
        **  Clear all bits.
        */
        for (word = 0; word < StatusAndControlWords; word++)
            {
            scrRegister[word] = 0;
            }

        activeChannel->data = 0;
        break;

    case 7:
        /*
        **  Test all error bits and return one if any set.
        */
        activeChannel->data = 0;
        for (word = 0; word < 4; word++)
            {
            if (word == 3)
                {
                if ((scrRegister[word] & 017) != 0)
                    {
                    activeChannel->data = 1;
                    break;
                    }
                }
            else if (scrRegister[word] != 0)
                {
                activeChannel->data = 1;
                break;
                }
            }

        break;
        }

    activeChannel->full = TRUE;

#if DEBUG
    {
    static char *codeString[] =
        {
        "read word",
        "test bit",
        "clear bit",
        "test & clear bit",
        "set bit",
        "test & set bit",
        "clear all",
        "test all"
        };

    fprintf(scrLog, "%06d ppu[%02o] P=%04o S&C Reg: addr %03o %s result: %04o\n", traceSequenceNo, activePpu->id, activePpu->regP, designator, codeString[code], activeChannel->data);
    }
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Set a bit in the status and control register
**
**  Parameters:     Name        Description.
**                  bit         bit number
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void scrSetBit(PpWord *scrRegister, u16 bit)
    {
    scrRegister[bit / 12] |= (1 << (bit % 12));
    }

/*--------------------------------------------------------------------------
**  Purpose:        Clear a bit in the status and control register
**
**  Parameters:     Name        Description.
**                  bit         bit number
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void scrClrBit(PpWord *scrRegister, u16 bit)
    {
    scrRegister[bit / 12] &= ~(1 << (bit % 12));
    }

/*---------------------------  End Of File  ------------------------------*/
