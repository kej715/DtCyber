/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**
**  Name: deadstart.c
**
**  Description:
**      Perform emulation of CDC 6600 deadstart.
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
#include "const.h"
#include "types.h"
#include "proto.h"

/*
**  -----------------
**  Private Constants
**  -----------------
*/

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
static FcStatus deadFunc(PpWord funcCode);
static void deadIo(void);
static void deadActivate(void);
static void deadDisconnect(void);

/*
**  ----------------
**  Public Variables
**  ----------------
*/
u16 deadstartPanel[MaxDeadStart];
u8 deadstartCount;


/*
**  -----------------
**  Private Variables
**  -----------------
*/
static u8 dsSequence;       /* deadstart sequencer */

/*
**--------------------------------------------------------------------------
**
**  Public Functions
**
**--------------------------------------------------------------------------
*/

/*--------------------------------------------------------------------------
**  Purpose:        Execute deadstart function.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void deadStart(void)
    {
    DevSlot *dp;
    u8 pp;
    u8 ch;

    dp = channelAttach(0, 0, DtDeadStartPanel);

    dp->activate = deadActivate;
    dp->disconnect = deadDisconnect;
    dp->func = deadFunc;
    dp->io = deadIo;
    dp->selectedUnit = 0;

    /*
    **  Set all normal channels to active and empty.
    */
    for (ch = 0; ch < channelCount; ch++)
        {
        if (ch <= 013 || (ch >= 020 && ch <= 033))
            {
            channel[ch].active = TRUE;
            }
        }

    /*
    **  Set special channels appropriately.
    */
    channel[ChInterlock  ].active = (features & HasInterlockReg) != 0;
    channel[ChMaintenance].active = FALSE;

    /*
    **  Reset deadstart sequencer.
    */
    dsSequence = 0;

    for (pp = 0; pp < ppuCount; pp++)
        {
        /*
        **  Assign PPs to the corresponding channels.
        */
        if (pp < 012)
            {
            ppu[pp].opD = pp;
            channel[pp].active = TRUE;
            }
        else
            {
            ppu[pp].opD = pp - 012 + 020;
            channel[pp - 012 + 020].active = TRUE;
            }

        /*
        **  Set all PPs to INPUT (71) instruction.
        */
        ppu[pp].opF = 071;
        ppu[pp].busy = TRUE;

        /*
        **  Clear P registers and location zero of each PP.
        */
        ppu[pp].regP   = 0;
        ppu[pp].mem[0] = 0;

        /*
        **  Set all A registers to an input word count of 10000.
        */
        ppu[pp].regA = 010000;
        }

    /*
    **  Start load of PPU0.
    */
    channel[0].ioDevice = dp;
    channel[0].active = TRUE;
    channel[0].full = TRUE;
    channel[0].data = 0;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Execute function code on deadstart pseudo device.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        FcStatus
**
**------------------------------------------------------------------------*/
static FcStatus deadFunc(PpWord funcCode)
    {
    (void)funcCode;

    return(FcDeclined);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Perform I/O on deadstart panel.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void deadIo(void)
    {
    if (!activeChannel->full)
        {
        if (dsSequence == deadstartCount)
            {
            activeChannel->active = FALSE;
            }
        else
            {
            activeChannel->data = deadstartPanel[dsSequence++] & Mask12;
            activeChannel->full = TRUE;
            }
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
static void deadActivate(void)
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
static void deadDisconnect(void)
    {
    }

/*---------------------------  End Of File  ------------------------------*/
