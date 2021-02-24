#ifndef CYBER_CHANNEL_H
#define CYBER_CHANNEL_H
/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**
**  Name: cyber_channel.h
**
**  Description:
**      CDC CYBER and 6600 channel PCI card driver API for Linux.
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

/*
**  ----------------
**  Public Constants
**  ----------------
*/
#define DEVICE_NODE         "/dev/cyber_channel0"
#define IOCTL_FPGA_READ     _IOR('f', 0, struct ioCb *)
#define IOCTL_FPGA_WRITE    _IOR('f', 1, struct ioCb *)

/*
**  ----------------------
**  Public Macro Functions
**  ----------------------
*/

/*
**  ----------------------------------------
**  Public Typedef and Structure Definitions
**  ----------------------------------------
*/
typedef struct ioCb
    {
    int             address;
    unsigned short  data;
    } IoCB;

/*
**  --------------------------
**  Public Function Prototypes
**  --------------------------
*/

/*
**  ----------------
**  Public Variables
**  ----------------
*/

#endif /* CYBER_CHANNEL_H */

/*---------------------------  End Of File  ------------------------------*/

