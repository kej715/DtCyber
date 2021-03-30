/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**
**  Name: device.c
**
**  Description:
**      Device support for CDC 6600.
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

/*
**  ----------------
**  Public Variables
**  ----------------
*/
DevDesc deviceDesc[] =
    {
    "MT607",    mt607Init,
    "MT669",    mt669Init,
    "MT679",    mt679Init,
    "MT362x-7", mt362xInit_7,
    "MT362x-9", mt362xInit_9,
    "MT5744",   mt5744Init,
    "DD6603",   dd6603Init,
    "DD844-2",  dd844Init_2,
    "DD844-4",  dd844Init_4,
    "DD844",    dd844Init_4,
    "DD885-1",  dd885Init_1,
    "DD885",    dd885Init_1,
    "DD885-42", dd885_42Init,
    "CR405",    cr405Init,
    "LP1612",   lp1612Init,
    "LP501",    lp501Init,
    "LP512",    lp512Init,
    "CO6612",   consoleInit,
    "MUX6676",  mux6676Init,
    "CP3446",   cp3446Init,
    "CR3447",   cr3447Init,
    "TPM",      tpMuxInit,
    "DDP",      ddpInit,
    "NPU",      npuInit,
    "MDI",      mdiInit,
#if defined(__linux__) || defined(__gnu_linux__) || defined(linux) || defined(_WIN32)
	/* CYBER channel support only on some platforms */
    "PCICH",    pciInit,
#endif
#if defined(__linux__) || defined(__gnu_linux__) || defined(linux)
	/* CYBER channel support only on some platforms */
    "PCICON",    pciConsoleInit,
#endif
    };

u8 deviceCount = sizeof(deviceDesc) / sizeof(deviceDesc[0]);

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


/*---------------------------  End Of File  ------------------------------*/


