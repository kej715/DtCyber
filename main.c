/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**
**  Modifications:
**            2017  Steven Zoppi 11-Nov-2017
**                  Control-C Intercept
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

#define DEBUG    0

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "const.h"
#include "types.h"
#include "proto.h"

#if defined(_WIN32)
#include <Windows.h>
#include <winsock.h>
#include <timeapi.h>
#else
#include <signal.h>
#include <unistd.h>
#endif

#if defined(_WIN32)
extern int _write(
    int          fd,
    const void   *buffer,
    unsigned int count
    );
extern int _isatty(int fd);

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
static void tracePpuCalls(void);
static void waitTerminationMessage(void);

static void INThandler(int);
static void opExit(void);

/*
**  ----------------
**  Public Variables
**  ----------------
*/
char ppKeyIn         = 0;
bool emulationActive = TRUE;
u32  cycles;
u32  readerScanSecs = 3;

bool idle = FALSE;   /* Idle loop detection */
u32  idleNetBufs;    /* threshold of network buffers in use indicating network is busy */
u32  idleTrigger;    /* sleep every <idletrigger> cycles of the idle loop */
u32  idleTime;       /* microseconds to sleep when idle */
char ipAddress[16];
char networkInterface[16];
char networkInterfaceMgr[64];
char osType[16];
bool (*idleDetector)(CpuContext *) = &idleDetectorNone;

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
    **  Pause for user upon exit so display doesn't disappear
    */
    atexit(opExit);
    timeBeginPeriod(8);

    /*
    **  Select WINSOCK 1.1.
    */
    WORD    versionRequested;
    WSADATA wsaData;
    int     err;

    versionRequested = MAKEWORD(1, 1);

    err = WSAStartup(versionRequested, &wsaData);
    if (err != 0)
        {
        logDtError(LogErrorLocation, "\n(main) Error in WSAStartup: %d\n", err);
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

    /*
    **  20171110: SZoppi - Added Filesystem Watcher Support
    **  Setup exit handling.
    **  we need to enable running threads to shutdown gracefully so
    **  the wait time specified during the waitTerminationMessage routine
    **  must be longer than the longest running thread.
    **
    **  The card readers can start a filesystem watcher thread and they
    **  may need time to quiesce.
    */

    atexit(waitTerminationMessage);

    //  Don't let the user press Ctrl-C by accident.
    signal(SIGINT, INThandler);

    /*
    **  Allow optional command line parameter to specify section to run in "cyber.ini".
    */
    if (argc > 2)
        {
        initStartup(argv[1], argv[2]);
        }
    else if (argc > 1)
        {
        if ((stricmp(argv[1], "-?") == 0) | (stricmp(argv[1], "/?") == 0))
            {
            printf("Command Format:\n\n");
            printf("    %s <parameters>\n\n", argv[0]);
            printf("    <parameters> can be either:\n");
            printf("        ( /? | -? ) displays command format\n");
            printf("      or:\n");
            printf("        ( <section> ( <filename> ) )\n\n");
            printf("    where:\n");
            printf("      <section>  identifier of section within configuration file [default 'cyber']\n");
            printf("      <filename> file name of configuration file                 [default 'cyber.ini']\n");
            printf("\n      > It is recommended that 'legal' parameters for");
            printf("\n      > <section> or <filename> contain no spaces.");
            printf("\n    ---------------------------------------------------------------------------------\n");
            exit(-1);
            }
        initStartup(argv[1], "cyber.ini");
        }
    else
        {
        initStartup("cyber", "cyber.ini");
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

    fputs("(cpu    ) CPU0 started\n", stdout);

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
        rtcTick();
        ppStep();

        cpuStep(cpus);
        cpuStep(cpus);
        cpuStep(cpus);
        cpuStep(cpus);

        channelStep();

        idleThrottle(cpus);

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

    opDisplay("Goodbye for now.\n\n");

    exit(0);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Return CPU cycles to host if idle package is seen
**                  and the trigger conditions are met.
**
**  Parameters:     Name        Description.
**                  ctx         Cpu context to throttle.
**
**  Returns:        void
**
**------------------------------------------------------------------------*/
void idleThrottle(CpuContext *ctx)
    {
    if (idle)
        {
        if ((*idleDetector)(ctx))
            {
            ctx->idleCycles++;
            if ((ctx->idleCycles % idleTrigger) == 0)
                {
                if (ctx->id == 0)
                    {
                    if (idleCheckBusy() || npuBipIsBusy() || (rtcClockIsCurrent == FALSE))
                        {
                        return;
                        }
                    }
                sleepUsec(idleTime);
                }
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Check for busy PPs for idle throttle.
**
**  Parameters:     None.
**
**  Returns:        TRUE for busy, FALSE for not busy.
**
**------------------------------------------------------------------------*/
bool idleCheckBusy()
    {
    bool busyFlag = FALSE;

    for (u8 i = 0; i < ppuCount; i++)
        {
        if (ppu[i].busy)
            {
            busyFlag = TRUE;
            break;
            }
        }

    return busyFlag;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Dummy idle cycle detector
**                  always returns false.
**
**  Parameters:     Name        Description.
**                  ctx         Cpu context to check for idle.
**
**  Returns:        FALSE
**
**------------------------------------------------------------------------*/
bool idleDetectorNone(CpuContext *ctx)
    {
    return FALSE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        COS idle cycle detector
**
**  Parameters:     Name        Description.
**                  ctx         Cpu context to check for idle.
**
**  Returns:        TRUE if in idle, FALSE if not.
**
**------------------------------------------------------------------------*/
bool idleDetectorCOS(CpuContext *ctx)
    {
    if ((!ctx->isMonitorMode) && (ctx->regP == 02) && (ctx->regFlCm == 020))
        {
        return TRUE;
        }

    return FALSE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        NOS idle cycle detector
**
**  Parameters:     Name        Description.
**                  ctx         Cpu context to check for idle.
**
**  Returns:        TRUE if in idle, FALSE if not.
**
**------------------------------------------------------------------------*/
bool idleDetectorNOS(CpuContext *ctx)
    {
    if ((!ctx->isMonitorMode) && (ctx->regP == 02) && (ctx->regFlCm == 05))
        {
        return TRUE;
        }

    return FALSE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        NOS/BE idle cycle detector
**
**  Parameters:     Name        Description.
**                  ctx         Cpu context to check for idle.
**
**  Returns:        TRUE if in idle, FALSE if not.
**
**------------------------------------------------------------------------*/
bool idleDetectorNOSBE(CpuContext *ctx)
    {
    /* Based on observing CPU state on the NOS/BE TUB ready to run package */
    if ((!ctx->isMonitorMode) && (ctx->regP == 02) && (ctx->regFlCm == 010))
        {
        return TRUE;
        }

    return FALSE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        MACE idle cycle detector
**
**  Parameters:     Name        Description.
**                  ctx         Cpu context to check for idle.
**
**  Returns:        TRUE if in idle, FALSE if not.
**
**------------------------------------------------------------------------*/
bool idleDetectorMACE(CpuContext *ctx)
    {
    /* This is based on the KRONOS1 source code for CPUMTR
     * I have no working system to test this on, it may also work
     * for other early cyber operating systems, YMMV */
    if ((!ctx->isMonitorMode) && (ctx->regP == 02) && (ctx->regFlCm == 03))
        {
        return TRUE;
        }

    return FALSE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Run a helper process.
**
**  Parameters:     Name        Description.
**                  command     Helper command to run
**
**  Returns:        0 if successful, error code otherwise.
**
**------------------------------------------------------------------------*/
int runHelper(char *command)
    {
#if defined (_WIN32)
    char                *dp;
    PROCESS_INFORMATION pi;
    STARTUPINFO         si;
    char                *sp;
    char                winCmdLine[210];

    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    memset(&pi, 0, sizeof(pi));
    strcpy(winCmdLine, "/c ");
    sp = command;
    dp = winCmdLine + 3;
    while (*sp != '\0')
        {
        *dp++ = (*sp == '/') ? '\\' : *sp;
        sp   += 1;
        }
    *dp = '\0';

    if (!CreateProcessA("C:\\Windows\\System32\\cmd.exe", // Module name
                        winCmdLine,                       // Command line
                        NULL,                             // Process handle not inheritable
                        NULL,                             // Thread handle not inheritable
                        FALSE,                            // Set handle inheritance to FALSE
                        CREATE_NEW_CONSOLE,               // Creation flags
                        NULL,                             // Use parent's environment block
                        NULL,                             // Use parent's starting directory
                        &si,                              // Pointer to STARTUPINFO structure
                        &pi)                              // Pointer to PROCESS_INFORMATION structure
        )
        {
        return GetLastError();
        }
    else
        {
        return 0;
        }
#else
    return system(command);
#endif
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
void startHelpers(void)
    {
    char command[200];
    char *line;
    int  lineNo = 0;
    int  rc;

    if (initOpenHelpersSection() != 1)
        {
        return;
        }

    while ((line = initGetNextLine(&lineNo)) != NULL)
        {
        sprintf(command, "%s start", line);
        rc = runHelper(command);
        if (rc == 0)
            {
            printf("(main   ) Started helper: %s\n", line);
            }
        else
            {
            printf("(main   ) Failed to start helper \"%s\", rc = %d\n", line, rc);
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
void stopHelpers(void)
    {
    char command[200];
    char *line;
    int  lineNo = 0;
    int  rc;

    if (initOpenHelpersSection() == 1)
        {
        while ((line = initGetNextLine(&lineNo)) != NULL)
            {
            sprintf(command, "%s stop", line);
            rc = runHelper(command);
            if (rc == 0)
                {
                printf("\n(main   ) Stopped helper: %s\n", line);
                }
            else
                {
                printf("\n(main   ) Failed to stop helper \"%s\", rc = %d\n", line, rc);
                }
            }
        }
    if (strlen(networkInterfaceMgr) > 0)
        {
        sprintf(command, "%s %s %s stop", networkInterfaceMgr, networkInterface, ipAddress);
        rc = runHelper(command);
        if (rc == 0)
            {
            printf("\n(main   ) Stopped helper: %s\n", networkInterfaceMgr);
            }
        else
            {
            printf("\n(main   ) Failed to stop helper \"%s\", rc = %d\n", networkInterfaceMgr, rc);
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
    static u64  ppIrStatus[10] = { 0 };
    static u64  l;
    static u64  r;
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
        r = ppIrStatus[pp] & ((CpWord)Mask18 << (59 - 18));
        if (l != r)
            {
            ppIrStatus[pp] = l;
            if (l != 0)
                {
                l >>= (59 - 17);
                fprintf(f, "%c", cdcToAscii[(l >> 12) & Mask6]);
                fprintf(f, "%c", cdcToAscii[(l >> 6) & Mask6]);
                fprintf(f, "%c", cdcToAscii[(l >> 0) & Mask6]);
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
    sleepMsec(readerScanSecs * 1000);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Control-C Intercept for Main Loop.
**
**  Parameters:     Name        Description.
**                  sig         Signal causing this invocation.
**
**  Returns:        Zero.
**
**------------------------------------------------------------------------*/
static void INThandler(int sig)
    {
    char c;

    signal(sig, SIG_IGN);
    //  Use Async-safe output function.
    //                        1 234567890....+....2....+....3..
    _write(STD_OUTPUT_HANDLE, "\n*WARNING*:=====================", 32);
    _write(STD_OUTPUT_HANDLE, "\n*WARNING*: Ctrl-C Intercepted! ", 32);
    _write(STD_OUTPUT_HANDLE, "\n*WARNING*:=====================", 32);
    //                        1 2 34567890....+....2....+....3....+.
    _write(STD_OUTPUT_HANDLE, "\n\nDo you really want to quit? [y/n] ", 36);
    c = (char)getchar();
    if ((c == 'y') || (c == 'Y'))
        {
        exit(0);
        }
    else
        {
        signal(SIGINT, INThandler);
        }
    }

#if defined(_WIN32)
static void opExit()
    {
    char check;

    timeEndPeriod(8);
    if (_isatty(_fileno(stdin)) && opIsConsoleInput())
        {
        printf("Press ENTER to Exit");
        check = (char)getchar();
        }
    }

#endif

/*---------------------------  End Of File  ------------------------------*/
