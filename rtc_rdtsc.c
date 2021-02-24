/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter, Paul Koning
**
**  Name: rtc.c
**
**  Description:
**      Perform emulation of CDC 6600 real-time clock.
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
#include <math.h>
#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <Carbon.h>
#elif defined(__GNUC__) || defined(__SunOS)
#include <sys/time.h>
#include <unistd.h>
#endif

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
static FcStatus rtcFunc(PpWord funcCode);
static void rtcIo(void);
static void rtcActivate(void);
static void rtcDisconnect(void);
static bool rtcInitTick (u32 setMHz);
#ifndef _WIN32
static u64 rtcGetTick(void);
#endif

/*
**  ----------------
**  Public Variables
**  ----------------
*/
u32 rtcClock = 0;


/*
**  -----------------
**  Private Variables
**  -----------------
*/
static u8 rtcIncrement;
static bool rtcFull;
static u64 Hz;
static double MHz;
#ifdef _WIN32
static u64 (*rtcGetTick)(void);
#endif
#if CcCycleTime
static u64 startTime;
#endif


/*
**--------------------------------------------------------------------------
**
**  Public Functions
**
**--------------------------------------------------------------------------
*/

/*--------------------------------------------------------------------------
**  Purpose:        Initialise RTC.
**
**  Parameters:     Name        Description.
**                  increment   clock increment per iteration.
**                  setMHz      cycle counter frequency in MHz.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void rtcInit(u8 increment, u32 setMHz)
    {
    DevSlot *dp;

    dp = channelAttach(ChClock, 0, DtRtc);

    dp->activate = rtcActivate;
    dp->disconnect = rtcDisconnect;
    dp->func = rtcFunc;
    dp->io = rtcIo;
    dp->selectedUnit = 0;

    activeChannel->ioDevice = dp;
    activeChannel->hardwired = TRUE;

    if (increment == 0)
        {
        if (!rtcInitTick(setMHz))
            {
            printf("Invalid clock increment 0, defaulting to 1\n");
            increment = 1;
            }
        }

    rtcIncrement = increment;

    /*
    **  RTC channel may be active or inactive and empty or full
    **  depending on model.
    */
    rtcFull = (features & HasFullRTC) != 0;
    activeChannel->full = rtcFull;
    activeChannel->active = (features & HasFullRTC) != 0;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Do a clock tick
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void rtcTick(void)
    {
    rtcClock += rtcIncrement;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Start timing measurement.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
#if CcCycleTime
void rtcStartTimer(void)
    {
    if (rtcIncrement == 0)
        {
        startTime = rtcGetTick();
        }
    }
#endif

/*--------------------------------------------------------------------------
**  Purpose:        Complete timing measurement.
**
**  Parameters:     Name        Description.
**
**  Returns:        Time in microseconds.
**
**------------------------------------------------------------------------*/
#if CcCycleTime
double rtcStopTimer(void)
    {
    u64 endTime;
    if (rtcIncrement == 0)
        {
        endTime = rtcGetTick();
//        return((double)(int)(endTime - startTime) / ((double)(int)Hz / 1000000.0L));
        return((double)(int)(endTime - startTime) / ((double)(i64)Hz / 1000000.0L));
        }
    else
        {
        return(0.0);
        }
    }
#endif

/*--------------------------------------------------------------------------
**  Purpose:        Read current 32-bit microsecond counter and store in
**                  global variable rtcClock.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/

#define MaxMicroseconds 400.0L

void rtcReadUsCounter(void)
    {
    static bool first = TRUE;
    static u64 old = 0;
    static double fraction = 0.0L;
    static double delayedMicroseconds = 0.0L;
    u64 new;
    u64 difference;
    double microseconds;
    double result;

    if (rtcIncrement != 0)
        {
        return;
        }

    if (first)
        {
        first = FALSE;
        old = rtcGetTick();
        }

    new = rtcGetTick();

    difference = new - old;
    old = new;

    microseconds = (double)(i64)difference / MHz;
    microseconds += fraction + delayedMicroseconds;
    delayedMicroseconds = 0.0;

    if (microseconds > MaxMicroseconds)
        {
        delayedMicroseconds = microseconds - MaxMicroseconds;
        microseconds = MaxMicroseconds;
        }

    result = floor(microseconds);
    fraction = microseconds - result;

    rtcClock += (u32)result;
    }

/*
**--------------------------------------------------------------------------
**
**  Private Functions
**
**--------------------------------------------------------------------------
*/

/*--------------------------------------------------------------------------
**  Purpose:        Execute function code on RTC pseudo device.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static FcStatus rtcFunc(PpWord funcCode)
    {
    (void)funcCode;

    return(FcAccepted);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Perform I/O.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void rtcIo(void)
    {
    rtcReadUsCounter();
    activeChannel->full = rtcFull;
    activeChannel->data = (PpWord)rtcClock & Mask12;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Handle channel activation.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void rtcActivate(void)
    {
    }

/*--------------------------------------------------------------------------
**  Purpose:        Handle disconnecting of channel.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void rtcDisconnect(void)
    {
    }

#if defined(_WIN32)

/*--------------------------------------------------------------------------
**  Purpose:        Low-level microsecond tick functions.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static u64 rtcGetTickWindows(void)
    {
    LARGE_INTEGER ctr;

    QueryPerformanceCounter(&ctr);
    return(ctr.QuadPart);
    }

static __declspec(naked) u64 rtcGetTickCpu(void)
    {
    __asm
        {
        rdtsc
        ret ; return value at EDX:EAX
        }
    }

static bool rtcInitTick(u32 setMHz)
    {
    LARGE_INTEGER lhz;

    if (QueryPerformanceFrequency(&lhz))
        {
        Hz = lhz.QuadPart;
        rtcGetTick = rtcGetTickWindows;
        }
    else
        {
        rtcGetTick = rtcGetTickCpu;
        printf("Windows performance counter not available, using Pentium's RDTSC\n");
        if (setMHz == 0)
            {
            u64 first = rtcGetTick();
            Sleep(1000);
            Hz = rtcGetTick() - first;
            }
        else
            {
            Hz = setMHz * 1000000;
            }
        }

    MHz = (double)(i64)Hz / 1000000.0;

    printf("Using high resolution hardware clock at %f MHz\n", MHz);

    return(TRUE);
    }

#elif defined(__GNUC__) && defined(__linux__) && (defined(__x86_64) || defined(__i386))

/*--------------------------------------------------------------------------
**  Purpose:        Low-level microsecond tick functions.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static u64 rtcGetTick(void)
    {
     u32 a, d; 
     __asm__ __volatile__("rdtsc" : "=a" (a), "=d" (d)); 
     return((u64)a | ((u64)d << 32)); 
    }

static bool rtcInitTick(u32 setMHz)
    {
    FILE *procf;
    char procbuf[512];
    u64 now, prev;
    char *p;
    double procMHz;
    u64 hz = 0;

    if (setMHz == 0)
        {
        procf = fopen("/proc/cpuinfo", "r");
        if (procf != NULL)
            {
            fread(procbuf, sizeof(procbuf), 1, procf);
            p = strstr(procbuf, "cpu MHz");
            if (p != NULL)
                {
                p = strchr(p, ':') + 1;
                }
            if (p != NULL)
                {
                sscanf(p, " %lf", &procMHz);
                hz = procMHz * 1000000.0;
                }
            fclose(procf);
            }

        if (hz == 0)
            {
            sleep(1);
            prev = rtcGetTick();
            sleep(1);
            now = rtcGetTick();
            hz = now - prev;
            }

        Hz = hz;
        MHz = (double)Hz / 1000000.0;
        }
    else
        {
        MHz = (double)setMHz;
        Hz = setMHz * 1000000;
        }

    printf("Using high resolution hardware clock at %f MHz\n", MHz);

    return(TRUE);
    }

#elif defined(__GNUC__) && (defined(__linux__) || defined(__SunOS) || defined (__FreeBSD__))

/*--------------------------------------------------------------------------
**  Purpose:        Low-level microsecond tick functions.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static bool rtcInitTick(u32 setMHz)
    {
    (void)setMHz;

    Hz = 1000000;
    MHz = 1.0;
    printf("Using high resolution hardware clock at %f MHz\n", MHz);
    return(TRUE);
    }

static u64 rtcGetTick(void)
    {
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return(tv.tv_sec * 1000000 + tv.tv_usec);
    }

#elif defined(__GNUC__) && defined(__APPLE__)

/*--------------------------------------------------------------------------
**  Purpose:        Low-level microsecond tick functions.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static bool rtcInitTick(u32 setMHz)
    {
    (void)setMHz;

    Hz = 1000000000;     /* 10^9, because timer is in ns */
    MHz = 1000.0;
    printf("Using high resolution hardware clock at %f MHz\n", MHz);
    return(TRUE);
    }

static u64 rtcGetTick(void)
    {
    return(UnsignedWideToUInt64(AbsoluteToNanoseconds(UpTime())));
    }

#else

/*--------------------------------------------------------------------------
**  Purpose:        Low-level microsecond tick functions.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static bool rtcInitTick(u32 setMHz)
    {
    (void)setMHz;

    printf("No high resolution hardware clock, using emulation cycle counter\n");
    return(FALSE);
    }

#endif

/*---------------------------  End Of File  ------------------------------*/
