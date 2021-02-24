/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**
**  Name: pci_channel.c
**
**  Description:
**      Interface to PCI channel adapter.
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

#define PciCmdNop           0x0000
#define PciCmdFunction      0x2000
#define PciCmdFull          0x4000
#define PciCmdEmpty         0x6000
#define PciCmdActive        0x8000
#define PciCmdInactive      0xA000
#define PciCmdClear         0xC000
#define PciCmdMasterClear   0xE000

#define PciStaFull          0x2000
#define PciStaActive        0x4000
#define PciStaBusy          0x8000

#define PciMaskData         0x0FFF
#define PciMaskParity       0x1000
#define PciShiftParity      12

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
static PciParam *pci;
static IoCB io;

#if DEBUG
static FILE *pciLog = NULL;
static bool active = FALSE;
#endif

/*
**--------------------------------------------------------------------------
**
**  Public Functions
**
**--------------------------------------------------------------------------
*/
/*--------------------------------------------------------------------------
**  Purpose:        Initialise PCI channel interface.
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
void pciInit(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName)
    {
    DevSlot *dp;

    (void)unitNo;
    (void)deviceName;

#if DEBUG
    if (pciLog == NULL)
        {
        pciLog = fopen("pcilog.txt", "wt");
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
#if DEBUG
    fprintf(pciLog, "\n%06d PP:%02o CH:%02o f:%04o >   ",
            traceSequenceNo,
            activePpu->id,
            activeChannel->id,
            funcCode);
#endif

    pciCmd(PciCmdFunction | funcCode | (pciParity(funcCode) << PciShiftParity));

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
    PpWord data = pciStatus() & Mask12;

#if DEBUG
    fprintf(pciLog, " I(%03X)", data);
#endif

    return(data);
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
    fprintf(pciLog, " O(%03X)", pci->data);
#endif
    pciCmd(PciCmdFull | pci->data | (pciParity(pci->data) << PciShiftParity));
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
    if (active)
        {
//        fprintf(pciLog, " S(%04X)", s);
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
**  Purpose:        Calculate odd parity over 12 bit PP word.
**
**  Parameters:     Name        Description.
**                  data        12 bit PP word
**
**  Returns:        Parity
**
**------------------------------------------------------------------------*/
static u16 pciParity(PpWord data)
    {
    u16 ret = 1;

    while (data != 0)
        {
        ret ^= data & 1;
        data >>= 1;
        }

    return(ret);
    }

/*---------------------------  End Of File  ------------------------------*/

