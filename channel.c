/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**
**  Name: channel.c
**
**  Description:
**      Perform emulation of CDC 6600 channels.
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
ChSlot *channel;
ChSlot *activeChannel;
DevSlot *activeDevice;
u8 channelCount;

/*
**  -----------------
**  Private Variables
**  -----------------
*/
static u8 ch = 0;

/*
**--------------------------------------------------------------------------
**
**  Public Functions
**
**--------------------------------------------------------------------------
*/

/*--------------------------------------------------------------------------
**  Purpose:        Initialise channels.
**
**  Parameters:     Name        Description.
**                  count       channel count
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void channelInit(u8 count)
    {
    /*
    **  Allocate channel structures.
    */
    channelCount = count;
    channel = calloc(MaxChannels, sizeof(ChSlot));
    if (channel == NULL)
        {
        fprintf(stderr, "(channel) Failed to allocate channel control blocks\n");
        exit(1);
        }

    /*
    **  Initialise all channels.
    */
    for (ch = 0; ch < MaxChannels; ch++)
        {
        channel[ch].id = ch;
        }

    /*
    **  Print a friendly message.
    */
    printf("(channel) Initialised (number of channels %o)\n", channelCount);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Display Channel Information.
**
**  Parameters:     Name        Description.
**
**  Returns:        List of All Devices Attached to All Channels.
**
**------------------------------------------------------------------------*/
void channelDisplayContext(void)
{
    DevSlot* dp;
    u8 ch;
    u8 i;
    char *devTypeName;
    u8 devNum;
    u8 devFCB;
	
    printf("\n\n Channel Context Display\n ------- ------- -------\n\n");

	//                  00 Deadstart Panel............. (01) Attached: 00 Files: 00
    printf("    > CH First Device Type........... (DT)  # Devices    # Files\n");
    printf("    > -- ---------------------------- ---- ------------ ---------\n");
    printf("    >\n");
	
    for (ch = 0; ch < channelCount; ch++)
    {
        
        for (dp = channel[ch].firstDevice; dp != NULL; dp = dp->next)
        {
        	//                                                               0....+....1....+....2....+....3..
            if (dp->devType == DtNone)                       { devTypeName = "None ......................."; }
            else if (dp->devType == DtDeadStartPanel)        { devTypeName = "Deadstart Panel............."; }
            else if (dp->devType == DtMt607)                 { devTypeName = "Magnetic Tape 607..........."; }
            else if (dp->devType == DtMt669)                 { devTypeName = "Magnetic Tape 669..........."; }
            else if (dp->devType == DtDd6603)                { devTypeName = "Disk Device 6603............"; }
            else if (dp->devType == DtDd8xx)                 { devTypeName = "Disk Device 8xx............."; }
            else if (dp->devType == DtCr405)                 { devTypeName = "Card Reader 405............."; }
            else if (dp->devType == DtLp1612)                { devTypeName = "Line Printer 1612..........."; }
            else if (dp->devType == DtLp5xx)                 { devTypeName = "Line Printer 5xx............"; }
            else if (dp->devType == DtRtc)                   { devTypeName = "Realtime Clock.............."; }
            else if (dp->devType == DtConsole)               { devTypeName = "Console....................."; }
            else if (dp->devType == DtMux6676)               { devTypeName = "Multiplexer 6676............"; }
            else if (dp->devType == DtCp3446)                { devTypeName = "Card Punch 3446............."; }
            else if (dp->devType == DtCr3447)                { devTypeName = "Card Reader 3447............"; }
            else if (dp->devType == DtDcc6681)               { devTypeName = "Data Channel Converter 6681."; }
            else if (dp->devType == DtTpm)                   { devTypeName = "Two Port Multiplexer........"; }
            else if (dp->devType == DtDdp)                   { devTypeName = "Distributive Data Path......"; }
            else if (dp->devType == DtNiu)                   { devTypeName = "Network Interface Unit......"; }
            else if (dp->devType == DtMt679)                 { devTypeName = "Magnetic Tape 679..........."; }
            else if (dp->devType == DtNpu)                   { devTypeName = "Network Processor Unit......"; }
            else if (dp->devType == DtMt362x)                { devTypeName = "Magnetic Tape 362x.........."; }
            else if (dp->devType == DtMch)                   { devTypeName = "Maintenance Channel........."; }
            else if (dp->devType == DtStatusControlRegister) { devTypeName = "Status Control Register....."; }
            else if (dp->devType == DtInterlockRegister)     { devTypeName = "Interlock Register.........."; }
            else if (dp->devType == DtPciChannel)            { devTypeName = "PCI Channel................."; }
            else if (dp->devType == DtMt362x)                { devTypeName = "Magnetic Tape 362x.........."; }
            else                                             { devTypeName = "Unknown Device.............."; }
            printf("    > %02o %s (%02o)", 
                channel[ch].id,
                devTypeName,
                dp->devType
                );
        	
            devNum = 0;
            devFCB = 0;
            for (i = 0; i < MaxUnits; i++)
            {
                if (dp->context[i] != NULL)
                    devNum++;

                if (dp->fcb[i] != NULL)
                    devFCB++;
            }
        	
        	if (devNum > 0)
        	{
	            printf(" Attached: %02i", devNum);
            } else {
	            printf(" ------------");
            }
        	
            if (devFCB > 0) 
            {
                printf(" Files: %02i", devFCB);
            } else {
                printf(" ---------");
            }
        	printf("\n");

        }
    }
}


/*--------------------------------------------------------------------------
**  Purpose:        Terminate channels.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void channelTerminate(void)
    {
    #pragma warning(push)
    #pragma warning(disable: 6001)
	DevSlot *dp;
    DevSlot *fp;
	#pragma warning(pop)
    u8 i;

    /*
    **  Give some devices a chance to cleanup and free allocated memory of all
    **  devices hanging of this channel.
    */
    for (ch = 0; ch < channelCount; ch++)
        {
        for (dp = channel[ch].firstDevice; dp != NULL; dp = dp->next)
            {
            if (dp->devType == DtDcc6681)
                {
                dcc6681Terminate(dp);
                }

            if (dp->devType == DtMt669)
                {
                mt669Terminate(dp);
                }

            if (dp->devType == DtMt679)
                {
                mt679Terminate(dp);
                }

            /*
            **  Free all unit contexts and close all open files.
            */
            for (i = 0; i < MaxUnits; i++)
                {
                if (dp->context[i] != NULL)
                    {
                    free(dp->context[i]);
                    }

                if (dp->fcb[i] != NULL)
                    {
                    fclose(dp->fcb[i]);
                    }
                }
            }

        for (dp = channel[ch].firstDevice; dp != NULL;)
            {
            /*
            **  Free all device control blocks.
            */
            fp = dp;
            dp = dp->next;
            free(fp);
            }
        }

    /*
    **  Free all channel control blocks.
    */
    free(channel);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Return device control block attached to a channel.
**
**  Parameters:     Name        Description.
**                  channelNo   channel number to attach to.
**                  devType     device type
**
**  Returns:        Pointer to device slot.
**
**------------------------------------------------------------------------*/
DevSlot *channelFindDevice(u8 channelNo, u8 devType)
    {
    DevSlot *device;
    ChSlot *cp;

    cp = channel + channelNo;
    device = cp->firstDevice;

    /*
    **  Try to locate device control block.
    */
    while (device != NULL) 
        {
        if (device->devType == devType)
            {
            return(device);
            }

        device = device->next;
        }

    return(NULL);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Attach device to channel.
**
**  Parameters:     Name        Description.
**                  channelNo   channel number to attach to
**                  eqNo        equipment number
**                  devType     device type
**
**  Returns:        Pointer to device slot.
**
**------------------------------------------------------------------------*/
DevSlot *channelAttach(u8 channelNo, u8 eqNo, u8 devType)
    {
    DevSlot *device;

    activeChannel = channel + channelNo;
    device = activeChannel->firstDevice;

    /*
    **  Try to locate existing device control block.
    */
    while (device != NULL) 
        {
        if (   device->devType == devType
            && device->eqNo    == eqNo)
            {
            return(device);
            }

        device = device->next;
        }

    /*
    **  No device control block of this type found, allocate new one.
    */
    device = calloc(1, sizeof(DevSlot));
    if (device == NULL)
        {
        fprintf(stderr, "(channel) Failed to allocate control block for Channel %d\n",channelNo);
        exit(1);
        }

    /*
    **  Link device control block into the chain of devices hanging off of this channel.
    */
    device->next = activeChannel->firstDevice;
    activeChannel->firstDevice = device;
    device->channel = activeChannel;
    device->devType = devType;
    device->eqNo    = eqNo;

    return(device);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Issue a function code to all attached devices.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void channelFunction(PpWord funcCode)
    {
    FcStatus status = FcDeclined;

    activeChannel->full = FALSE;
    for (activeDevice = activeChannel->firstDevice; activeDevice != NULL; activeDevice = activeDevice->next)
        {
        status = activeDevice->func(funcCode);
        if (status == FcAccepted)
            {
            /*
            **  Device has claimed function code - select it for I/O.
            */
            activeChannel->ioDevice = activeDevice;
            break;
            }

        if (status == FcProcessed)
            {
            /*
            **  Device has processed function code - no need for I/O.
            */
            activeChannel->ioDevice = NULL;
            break;
            }
        }

    if (activeDevice == NULL || status == FcDeclined)
        {
        /*
        **  No device has claimed the function code - keep channel active
        **  and full, but disconnect device.
        */
        activeChannel->ioDevice = NULL;
        activeChannel->full = TRUE;
        activeChannel->active = TRUE;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Activate a channel and let attached device know.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void channelActivate(void)
    {
    activeChannel->active = TRUE;

    if (activeChannel->ioDevice != NULL)
        {
        activeDevice = activeChannel->ioDevice;
        activeDevice->activate();
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Disconnect a channel and let active device know.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void channelDisconnect(void)
    {
    activeChannel->active = FALSE;

    if (activeChannel->ioDevice != NULL)
        {
        activeDevice = activeChannel->ioDevice;
        activeDevice->disconnect();
        }
    else
        {
        activeChannel->full = FALSE;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        IO on a channel.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
void channelIo(void)
    {
    /*
    **  Perform request.
    */
    if (   (activeChannel->active || activeChannel->id == ChClock)
        && activeChannel->ioDevice != NULL)
        {
        activeDevice = activeChannel->ioDevice;
        activeDevice->io();
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Check if PCI channel is active.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void channelCheckIfActive(void)
    {
    if (activeChannel->ioDevice != NULL)
        {
        activeDevice = activeChannel->ioDevice;
        if (activeDevice->devType == DtPciChannel)
            {
            u16 flags = activeDevice->flags();
            activeChannel->active = (flags & MaskActive) != 0;
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Check if channel is full.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void channelCheckIfFull(void)
    {
    if (activeChannel->ioDevice != NULL)
        {
        activeDevice = activeChannel->ioDevice;
        if (activeDevice->devType == DtPciChannel)
            {
            u16 flags = activeDevice->flags();
            activeChannel->full = (flags & MaskFull) != 0;
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Output to channel
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void channelOut(void)
    {
    if (activeChannel->ioDevice != NULL)
        {
        activeDevice = activeChannel->ioDevice;
        if (activeDevice->devType == DtPciChannel)
            {
            activeDevice->out(activeChannel->data);
            return;
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Input from channel
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void channelIn(void)
    {
    if (activeChannel->ioDevice != NULL)
        {
        activeDevice = activeChannel->ioDevice;
        if (activeDevice->devType == DtPciChannel)
            {
            activeChannel->data = activeDevice->in();
            return;
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Set channel full.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void channelSetFull(void)
    {
    if (activeChannel->ioDevice != NULL)
        {
        activeDevice = activeChannel->ioDevice;
        if (activeDevice->devType == DtPciChannel)
            {
            activeDevice->full();
            }
        }

    activeChannel->full = TRUE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Set channel empty.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void channelSetEmpty(void)
    {
    if (activeChannel->ioDevice != NULL)
        {
        activeDevice = activeChannel->ioDevice;
        if (activeDevice->devType == DtPciChannel)
            {
            activeDevice->empty();
            }
        }

    activeChannel->full = FALSE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Handle delayed channel disconnect.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void channelStep(void)
    {
    ChSlot *cc;

    /*
    **  Process any delayed disconnects.
    */
    for (ch = 0; ch < channelCount; ch++)
        {
        cc = &channel[ch];
        if (cc->delayDisconnect != 0)
            {
            cc->delayDisconnect -= 1;
            if (cc->delayDisconnect == 0)
                {
                cc->active = FALSE;
                cc->discAfterInput = FALSE;
                }
            }

        if (cc->delayStatus != 0)
            {
            cc->delayStatus -= 1;
            }
        }
    }

/*---------------------------  End Of File  ------------------------------*/
