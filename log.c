/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**
**  Name: log.c
**
**  Description:
**      Perform logging of abnormal conditions.
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
#include <stdarg.h>
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

/*
**  -----------------
**  Private Variables
**  -----------------
*/
static FILE *logF;

/*
**--------------------------------------------------------------------------
**
**  Public Functions
**
**--------------------------------------------------------------------------
*/

/*--------------------------------------------------------------------------
**  Purpose:        Initialize logging.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void logInit(void)
    {
    logF = fopen("log.txt", "wt");
    if (logF == NULL)
        {
        fprintf(stderr, "can't open log file");
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Write a message to the error log.
**
**  Parameters:     Name        Description.
**                  file        file name where error occured
**                  line        line where error occured
**                  fmt         format string
**                  ...         variable length argument list
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void logError(char *file, int line, char *fmt, ...)
    {
    va_list param;

    va_start(param, fmt);
    fprintf(logF, "[%s:%d] ", file, line); 
    vfprintf(logF, fmt, param);
    va_end(param);
    fprintf(logF, "\n");
    fflush(logF);
    }


/*---------------------------  End Of File  ------------------------------*/
