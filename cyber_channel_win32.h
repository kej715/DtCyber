#ifndef CYBER_CHANNEL_WIN32_H
#define CYBER_CHANNEL_WIN32_H
/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**
**  Name: cyber_channel_win32.h
**
**  Description:
**      CDC CYBER and 6600 channel PCI card driver API for WIN32.
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

#include <initguid.h>

DEFINE_GUID(GUID_DEVINTERFACE_CYBER_CHANNEL, // Generated using guidgen.exe
0x9519364b, 0x39f3, 0x4354, 0x9b, 0xe, 0x3f, 0xa7, 0x71, 0x5b, 0xa5, 0x61);
// {9519364B-39F3-4354-9B0E-3FA7715BA561}


//
// Define the structures that will be used by the IOCTL 
//  interface to the driver
//

#define IOCTL_INDEX                     0x800
#define FILE_DEVICE_CYBER_CHANNEL       0x65500
#define IOCTL_CYBER_CHANNEL_PUT CTL_CODE(FILE_DEVICE_CYBER_CHANNEL, IOCTL_INDEX + 0, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_CYBER_CHANNEL_GET CTL_CODE(FILE_DEVICE_CYBER_CHANNEL, IOCTL_INDEX + 1, METHOD_BUFFERED, FILE_ANY_ACCESS)

#endif /* CYBER_CHANNEL_WIN32_H */

/*---------------------------  End Of File  ------------------------------*/


