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
#include <string.h>
#include <ctype.h>
#if defined(_WIN32)
#include <Windows.h>
#else
#include <sys/time.h>
#endif

/*--------------------------------------------------------------------------
**  Purpose:        convert a string to lower case.
**
**  Parameters:     Name        Description.
**                  str         Input string to convert
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
char* dtStrLwr(char* str)
{
    unsigned char* p = (unsigned char*)str;

    while (*p) {
        *p = tolower((unsigned char)*p);
        p++;
    }
    return str;
}
