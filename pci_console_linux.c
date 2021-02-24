/*--------------------------------------------------------------------------
**
**  Copyright (c) 2013, Tom Hunter
**
**  Name: pci_console_linux.c
**
**  Description:
**      Interface to PCI console adapter.
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
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include <sys/ioctl.h>
#include "const.h"
#include "types.h"
#include "proto.h"
#include "cyber_channel_linux.h"

/*
**  -----------------
**  Private Constants
**  -----------------
*/

/*
**  CDC 6612 console functions and status codes.
*/
#define Fc6612Sel64CharLeft     07000
#define Fc6612Sel32CharLeft     07001
#define Fc6612Sel16CharLeft     07002

#define Fc6612Sel512DotsLeft    07010
#define Fc6612Sel512DotsRight   07110
#define Fc6612SelKeyIn          07020

#define Fc6612Sel64CharRight    07100
#define Fc6612Sel32CharRight    07101
#define Fc6612Sel16CharRight    07102

#define PciCmdNop               0x0000
#define PciCmdFunction          0x2000
#define PciCmdFull              0x4000
#define PciCmdEmpty             0x6000
#define PciCmdActive            0x8000
#define PciCmdInactive          0xA000
#define PciCmdClear             0xC000
#define PciCmdMasterClear       0xE000
                                
#define PciStaFull              0x2000
#define PciStaActive            0x4000
#define PciStaBusy              0x8000
                                
#define PciMaskData             0x0FFF

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
typedef struct pciParam
    {
    int         fdPci;
    int         data;
    } PciParam;

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static void consoleFunc(PpWord funcCode);
static void consoleOut(PpWord data);

static FcStatus pciFunc(PpWord funcCode);
static void pciIo(void);
static PpWord pciIn(void);
static void pciOut(PpWord data);
static void pciFull(void);
static void pciEmpty(void);
static void pciActivate(void);
static void pciDisconnect(void);
static u16 pciFlags(void);
static void pciCmd(u16 data);
static u16 pciStatus(void);
static u16 pciParity(PpWord val);

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
static u8 currentFont;
static u16 currentOffset;
static bool emptyDrop = FALSE;

static PciParam *pci;
static IoCB io;

#if DEBUG
static FILE *pciLog = NULL;
static bool active = FALSE;
#endif

/*
**  Map the broken keyboard codes from the LCM's DD60 console to what they should be.
**  Note that other consoles won't need this.
*/
static const u8 serial2ToConsole[64] =
    {
    /* 00-07 */   0,  01,  02,  03,  04,  05,  06,  07,
    /* 10-17 */ 010, 011, 012, 013, 014, 015, 016, 017,
    /* 20-27 */ 020, 021, 022, 023, 024, 025, 026, 027,
    /* 30-37 */ 030, 031, 032,   0,   0, 060,   0,   0,
    /* 40-47 */ 062, 061,   0,   0, 053,   0,   0,   0, 
    /* 50-57 */ 051, 052, 047, 045, 056, 046, 057, 050,
    /* 60-67 */ 033, 034, 035, 036, 037, 040, 041, 042,
    /* 70-77 */   0,   0,   0, 044, 043,   0, 055, 054
    };

/*
**--------------------------------------------------------------------------
**
**  Public Functions
**
**--------------------------------------------------------------------------
*/
/*--------------------------------------------------------------------------
**  Purpose:        Initialise PCI console interface.
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
void pciConsoleInit(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName)
    {
    DevSlot *dp;

    (void)unitNo;
    (void)deviceName;

#if DEBUG
    if (pciLog == NULL)
        {
        pciLog = fopen("pci_console_log.txt", "wt");
        }
#endif

    /*
    **  Attach device to channel and initialise device control block.
    */
    dp = channelAttach(channelNo, eqNo, DtPciChannel);
    dp->activate = pciActivate;
    dp->disconnect = pciDisconnect;
    dp->func = pciFunc;
    dp->io = pciIo;
    dp->flags = pciFlags;
    dp->in = pciIn;
    dp->out = pciOut;
    dp->full = pciFull;
    dp->empty = pciEmpty;

    /*
    **  Allocate and initialise channel parameters.
    */
    pci = calloc(1, sizeof(PciParam));
    if (pci == NULL)
        {
        fprintf(stderr, "Failed to allocate PCI channel context block\n");
        exit(1);
        }

    pci->fdPci = open(DEVICE_NODE, 0);
    if (pci->fdPci < 0)
        {
        fprintf(stderr, "Can't open %s - error %s\n", DEVICE_NODE, strerror(errno));
        exit(1);
        }

    pciCmd(PciCmdMasterClear);
    windowInit();

    /*
    **  Print a friendly message.
    */
    printf("PCI channel interface initialised on channel %o unit %o\n", channelNo, unitNo);
    }

/*
**--------------------------------------------------------------------------
**
**  Private Functions
**
**--------------------------------------------------------------------------
*/

/*--------------------------------------------------------------------------
**  Purpose:        Execute function code on channel.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        FcStatus
**
**------------------------------------------------------------------------*/
static FcStatus pciFunc(PpWord funcCode)
    {
    if (funcCode == 0)
        {
        return(FcDeclined);
        }
#if DEBUG
    fprintf(pciLog, "\n%06d PP:%02o CH:%02o f:%04o >   ",
            traceSequenceNo,
            activePpu->id,
            activeChannel->id,
            funcCode);
#endif

    pciCmd(PciCmdFunction | funcCode);
    consoleFunc(funcCode);

    return(FcAccepted);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Perform I/O on channel (not used).
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void pciIo(void)
    {
    }

/*--------------------------------------------------------------------------
**  Purpose:        Perform input from PCI channel.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static PpWord pciIn(void)
    {
    PpWord data = 0;

    // Comment out the following line if no console is connected
    data = serial2ToConsole[pciStatus() & Mask6];

    if (activeDevice->fcode == Fc6612SelKeyIn)
        {
        data |= asciiToConsole[ppKeyIn];
        activeDevice->fcode = 0;
        ppKeyIn = 0;
        }


#if DEBUG
    fprintf(pciLog, " I(%04o)", data);
#endif

    return(data & Mask6);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Save output for PCI channel.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void pciOut(PpWord data)
    {
    pci->data = data;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Set the channel full with data previously saved
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void pciFull(void)
    {
#if DEBUG
    fprintf(pciLog, " O(%04o)", pci->data);
#endif
    pciCmd(PciCmdFull | pci->data);
    consoleOut(pci->data);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Set the channel empty
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void pciEmpty(void)
    {
#if DEBUG
    fprintf(pciLog, " E");
#endif
    pciCmd(PciCmdEmpty);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Handle channel activation.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void pciActivate(void)
    {
#if DEBUG
    fprintf(pciLog, " A");
    active = TRUE;
#endif
    pciCmd(PciCmdActive);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Handle disconnecting of channel.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void pciDisconnect(void)
    {
#if DEBUG
    fprintf(pciLog, " D");
    active = FALSE;
#endif
    pciCmd(PciCmdInactive);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Update full/active channel flags.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static u16 pciFlags(void)
    {
#if DEBUG
    u16 s = pciStatus();
 //   if (active)
        {
        fprintf(pciLog, " S(0x%04X)", s);
        }
    return(s);
#else
    return(pciStatus());
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Send a PCI command
**
**  Parameters:     Name        Description.
**                  data        command data
**
**  Returns:        nothing
**
**------------------------------------------------------------------------*/
static void pciCmd(u16 data)
    {
    IoCB io;

    io.address = 0;
    do
        {
        ioctl(pci->fdPci, IOCTL_FPGA_READ, &io);
        } while ((io.data & PciStaBusy) != 0);

    io.data = data;
    ioctl(pci->fdPci, IOCTL_FPGA_WRITE, &io);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Get PCI status.
**
**  Parameters:     Name        Description.
**
**  Returns:        PCI status word
**
**------------------------------------------------------------------------*/
static u16 pciStatus(void)
    {
    IoCB io;

    io.address = 0;
    ioctl(pci->fdPci, IOCTL_FPGA_READ, &io);
    return(io.data);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Execute function code on 6612 console.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        FcStatus
**
**------------------------------------------------------------------------*/
static void consoleFunc(PpWord funcCode)
    {
    switch (funcCode)
        {
    default:
        return;

    case Fc6612Sel512DotsLeft:
        currentFont = FontDot;
        currentOffset = OffLeftScreen;
        windowSetFont(currentFont);
        break;

    case Fc6612Sel512DotsRight:
        currentFont = FontDot;
        currentOffset = OffRightScreen;
        windowSetFont(currentFont);
        break;

    case Fc6612Sel64CharLeft:
        currentFont = FontSmall;
        currentOffset = OffLeftScreen;
        windowSetFont(currentFont);
        break;

    case Fc6612Sel32CharLeft:
        currentFont = FontMedium;
        currentOffset = OffLeftScreen;
        windowSetFont(currentFont);
        break;

    case Fc6612Sel16CharLeft:
        currentFont = FontLarge;
        currentOffset = OffLeftScreen;
        windowSetFont(currentFont);
        break;

    case Fc6612Sel64CharRight:
        currentFont = FontSmall;
        currentOffset = OffRightScreen;
        windowSetFont(currentFont);
        break;

    case Fc6612Sel32CharRight:
        currentFont = FontMedium;
        currentOffset = OffRightScreen;
        windowSetFont(currentFont);
        break;

    case Fc6612Sel16CharRight:
        currentFont = FontLarge;
        currentOffset = OffRightScreen;
        windowSetFont(currentFont);
        break;

    case Fc6612SelKeyIn:
        break;
        }

    activeDevice->fcode = funcCode;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Perform I/O on 6612 console.
**
**  Parameters:     Name        Description.
**                  device      Device control block
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void consoleOut(PpWord data)
    {
    u8 ch;

    switch (activeDevice->fcode)
        {
    default:
        break;

    case Fc6612Sel64CharLeft:
    case Fc6612Sel32CharLeft:
    case Fc6612Sel16CharLeft:
    case Fc6612Sel64CharRight:
    case Fc6612Sel32CharRight:
    case Fc6612Sel16CharRight:
        ch = (u8)((data >> 6) & Mask6);

        if (ch >= 060)
            {
            if (ch >= 070)
                {
                /*
                **  Vertical coordinate.
                */
                windowSetY((u16)(activeChannel->data & Mask9));
                }
            else
                {
                /*
                **  Horizontal coordinate.
                */
                windowSetX((u16)((activeChannel->data & Mask9) + currentOffset));
                }
            }
        else
            {
            windowQueue(consoleToAscii[(activeChannel->data >> 6) & Mask6]);
            windowQueue(consoleToAscii[(activeChannel->data >> 0) & Mask6]);
            }

        break;

    case Fc6612Sel512DotsLeft:
    case Fc6612Sel512DotsRight:
        ch = (u8)((activeChannel->data >> 6) & Mask6);

        if (ch >= 060)
            {
            if (ch >= 070)
                {
                /*
                **  Vertical coordinate.
                */
                windowSetY((u16)(activeChannel->data & Mask9));
                windowQueue('.');
                }
            else
                {
                /*
                **  Horizontal coordinate.
                */
                windowSetX((u16)((activeChannel->data & Mask9) + currentOffset));
                }
            }

        break;
        }
    }

/*---------------------------  End Of File  ------------------------------*/

