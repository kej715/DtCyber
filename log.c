/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**                2024-2025, Steve Zoppi
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

#if defined(_WIN32)
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#include <errno.h>
#include <time.h>
#endif


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
static void dtNow(char *dtOutBuf, int buflen);

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
static FILE *logF  = NULL;
static char *LogFN = "dtcyberlog.txt";

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
    //  Don't do anything if it's already open
    if (logF != NULL)
        {
        return;
        }
    logF = fopen(LogFN, "wt");
    if (logF == NULL)
        {
        fprintf(stderr, "(log    ) can't open log file %s", LogFN);
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
    if (logF == NULL)
        {
        logInit();
        }

    va_list param;

    va_start(param, fmt);
    fprintf(logF, "[%s:%d] ", file, line);
    vfprintf(logF, fmt, param);
    va_end(param);
    fprintf(logF, "\n");
    fflush(logF);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Retrieve the current Date and Time
**
**  Parameters:     Name        Description.
**                  dtOutBuf    Output Buffer
**                  buflen      Maximum Length of Output Buffer
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void dtNow(char *dtOutBuf, int buflen)
    {
#if defined(_WIN32)
    SYSTEMTIME dt;
    GetLocalTime(&dt);
    sprintf_s(dtOutBuf, buflen,
              "%04d-%02d-%02d %02d:%02d:%02d",
              dt.wYear, dt.wMonth, dt.wDay, dt.wHour, dt.wMinute, dt.wSecond);
#else
    time_t    rawtime;
    struct tm *info;

    if ((opCmdStackPtr != 0)
        && (opCmdStack[opCmdStackPtr].netConn == 0)
        && (opCmdStack[opCmdStackPtr].in != -1))
        {
        return;
        }

    time(&rawtime);
    info = localtime(&rawtime);
    strftime(dtOutBuf, buflen, "%Y-%m-%d %H:%M:%S", info);
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Get the Current Date/Time and prepend it to the msg.
**
**  Parameters:     Name        Description.
**                  file        A string pointing to the full name of the
**                              C file raising the error.
**                  line        The line number which is raising the error.
**                  fmt         The format String to which the variadic
**                              parameters ... are applied.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void logDtError(char *file, int line, char *fmt, ...)
    {
    va_list param;
    char    *ixpos;

#define buflen    32
    char dtOutBuf[buflen];
    char dtFnBuf[128];


#if defined(_WIN32)
    dtNow(dtOutBuf, sizeof(dtOutBuf));
#else
    dtNow(dtOutBuf, buflen);
#endif

    if (logF == NULL)
        {
        logInit();
        }

    fprintf(stderr, "%s ", dtOutBuf);
    fprintf(logF, "%s ", dtOutBuf);

    ixpos = strrchr(file, '\\');
    if (ixpos == NULL)
        {
        ixpos = strrchr(file, '/');
        }
    if (ixpos == NULL)
        {
        ixpos = file;
        }
    else
        {
        ixpos += 1;
        }

    strcpy(dtFnBuf, ixpos);
    ixpos = strrchr(dtFnBuf, '.');
    if (ixpos == NULL)
        {
        ixpos = dtFnBuf;
        }
    else
        {
        ixpos[0] = '\0';
        }

    //  Print the origin of the message
    fprintf(stderr, "(%s:%d) ", dtFnBuf, line);
    fprintf(logF, "(%s:%d) ", dtFnBuf, line);

    va_start(param, fmt);
    vfprintf(stderr, fmt, param);
    vfprintf(logF, fmt, param);
    va_end(param);
    ixpos = strrchr(fmt, '\n');
    if (ixpos == NULL)
        {
        fprintf(stderr, "\n");
        fprintf(logF, "\n");
        }

    fflush(stderr);
    fflush(logF);
    }

/*---------------------------  End Of File  ------------------------------*/
