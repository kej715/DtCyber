/*--------------------------------------------------------------------------
**
**  Copyright (c) 2021, Kevin Jordan, Tom Hunter
**
**  Name: time.c
**
**  Description:
**      This module provides functions for obtaining time values.
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
#if defined(_WIN32)
#include <Windows.h>
#else
#include <sys/time.h>
#endif
#include <stdio.h>
#include <time.h>
#include "const.h"
#include "types.h"
#include "proto.h"

/*--------------------------------------------------------------------------
**  Purpose:        Returns the current system millisecond clock value.
**
**  Parameters:     Name        Description.
**
**  Returns:        Current millisecond clock value.
**
**------------------------------------------------------------------------*/
u64 getMilliseconds(void)
    {
#if defined(_WIN32)
    SYSTEMTIME systemTime;
    FILETIME   fileTime;

    GetSystemTime(&systemTime);
    SystemTimeToFileTime(&systemTime, &fileTime);
    return (((u64)fileTime.dwHighDateTime << 32) + (u64)fileTime.dwLowDateTime) / (u64)10000
        + (u64)systemTime.wMilliseconds;
#else
    struct timeval tod;

    gettimeofday(&tod, NULL);
    return ((u64)tod.tv_sec * (u64)1000) + ((u64)tod.tv_usec / (u64)1000);
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Returns the current system second clock value.
**
**  Parameters:     Name        Description.
**
**  Returns:        Current second clock value.
**
**------------------------------------------------------------------------*/
time_t getSeconds(void)
    {
    return time(NULL);
    }

