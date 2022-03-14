/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**
**  Name: main.c
**
**  Description:
**      Perform emulation of CDC 6600 mainframe system.
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
#if defined(_WIN32)
#include <windows.h>
#include <winsock.h>
#else
#include <signal.h>
#include <unistd.h>
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
static void startHelpers(void);
static void stopHelpers(void);
static void tracePpuCalls(void);
static void waitTerminationMessage(void);

/*
**  ----------------
**  Public Variables
**  ----------------
*/
char ppKeyIn = 0;
bool emulationActive = TRUE;
u32 cycles;
#if CcCycleTime
double cycleTime;
#endif

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

/*--------------------------------------------------------------------------
**  Purpose:        System initialisation and main program loop.
**
**  Parameters:     Name        Description.
**                  argc        Argument count.
**                  argv        Array of argument strings.
**
**  Returns:        Zero.
**
**------------------------------------------------------------------------*/
int main(int argc, char **argv)
    {
#if defined(_WIN32)
    /*
    **  Select WINSOCK 1.1.
    */ 
    WORD versionRequested;
    WSADATA wsaData;
    int err;

    versionRequested = MAKEWORD(1, 1);

    err = WSAStartup(versionRequested, &wsaData);
    if (err != 0)
        {
        fprintf(stderr, "\r\nError in WSAStartup: %d\r\n", err);
        exit(1);
        }
#else
    /*
    **  Ignore SIGPIPE signals
    */
    struct sigaction act;

    act.sa_handler = SIG_IGN;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGPIPE, &act, NULL);
#endif

    (void)argc;
    (void)argv;

    /*
    **  Setup exit handling.
    */
    atexit(waitTerminationMessage);

    /*
    **  Setup error logging.
    */
    logInit();

    /*
    **  Allow optional command line parameter to specify section to run in "cyber.ini".
    */
    if (argc == 2)
        {
        initStartup(argv[1]);
        }
    else
        {
        initStartup("cyber");
        }

    /*
    **  Setup debug support.
    */
#if CcDebug == 1
    traceInit();
#endif

    /*
    **  Start helpers, if any
    */
    startHelpers();

    /*
    **  Setup operator interface.
    */
    opInit();

    /*
    **  Initiate deadstart sequence.
    */
    deadStart();

    /*
    **  Emulation loop.
    */
    while (emulationActive)
        {
#if CcCycleTime
        rtcStartTimer();
#endif

        /*
        **  Count major cycles.
        */
        cycles++;

        /*
        **  Deal with operator interface requests.
        */
        if (opActive)
            {
            opRequest();
            }

        /*
        **  Execute PP, CPU and RTC.
        */
        ppStep();

        cpuStep();
        cpuStep();
        cpuStep();
        cpuStep();

        channelStep();
        rtcTick();

#if CcCycleTime
        cycleTime = rtcStopTimer();
#endif
        }

#if CcDebug == 1
    /*
    **  Example post-mortem dumps.
    */
    dumpInit();
    dumpAll();
#endif

    /*
    **  Shut down debug support.
    */
#if CcDebug == 1
    traceTerminate();
    dumpTerminate();
#endif

    /*
    **  Shut down emulation.
    */
    windowTerminate();
    cpuTerminate();
    ppTerminate();
    channelTerminate();

    /*
    **  Stop helpers, if any
    */
    stopHelpers();

    exit(0);
    }


/*
**--------------------------------------------------------------------------
**
**  Private Functions
**
**--------------------------------------------------------------------------
*/

/*--------------------------------------------------------------------------
**  Purpose:        Start helper processes.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void startHelpers(void)
    {
    char command[200];
    char *line;
    int rc;

    if (initOpenHelpersSection() != 1) return;

    while ((line = initGetNextLine()) != NULL)
        {
        sprintf(command, "%s start", line);
        rc = system(command);
        if (rc == 0)
            {
            printf("Started helper: %s\n", line);
            }
        else
            {
            printf("Failed to start helper \"%s\", rc = %d\n", line, rc);
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Stop helper processes.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void stopHelpers(void)
    {
    char command[200];
    char *line;
    int rc;

    if (initOpenHelpersSection() != 1) return;

    while ((line = initGetNextLine()) != NULL)
        {
        sprintf(command, "%s stop", line);
        rc = system(command);
        if (rc == 0)
            {
            printf("Stopped helper: %s\n", line);
            }
        else
            {
            printf("Failed to stop helper \"%s\", rc = %d\n", line, rc);
            }
        }
    fflush(stdout);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Trace SCOPE 3.1 PPU calls (debug only).
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void tracePpuCalls(void)
    {
    static u64 ppIrStatus[10] = {0};
    static u64 l;
    static u64 r;
    static FILE *f = NULL;

    int pp;

    if (f == NULL)
        {
        f = fopen("ppcalls.txt", "w");
        if (f == NULL)
            {
            return;
            }
        }
    for (pp = 1; pp < 10; pp++)
        {
        l = cpMem[050 + (pp * 010)] & ((CpWord)Mask18 << (59 - 18));
        r = ppIrStatus[pp]          & ((CpWord)Mask18 << (59 - 18));
        if (l != r)
            {
            ppIrStatus[pp] = l;
            if (l != 0)
                {
                l >>= (59 - 17);
                fprintf(f, "%c", cdcToAscii[(l >> 12) & Mask6]);
                fprintf(f, "%c", cdcToAscii[(l >>  6) & Mask6]);
                fprintf(f, "%c", cdcToAscii[(l >>  0) & Mask6]);
                fprintf(f, "\n");
                }
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Wait to display shutdown message.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void waitTerminationMessage(void)
    {
    fflush(stdout);
    #if defined(_WIN32)
        Sleep(3000);
    #else
        sleep(3);
    #endif
    }

/*---------------------------  End Of File  ------------------------------*/
