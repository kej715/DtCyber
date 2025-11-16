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
static bool rtcInitTick(bool doVirtual);
static u64 rtcGetTick(void);
static void rtcReadUsCounter(void);

/*
**  ----------------
**  Public Variables
**  ----------------
*/
u32  rtcClock          = 0;
u32  rtcClockDelta     = 0;
bool rtcClockIsCurrent = TRUE;

/*
**  -----------------
**  Private Variables
**  -----------------
*/
static double Hz;
static bool   rtcFull;
static u8     rtcIncrement;

#if defined(_WIN32)
static LARGE_INTEGER rtcFrequency;
static u64           rtcMicroseconds = 0;
static LARGE_INTEGER rtcTimeReference;
#else
static clockid_t     rtcClockId;
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
**                  doVirtual   whether to use virtual or real time
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void rtcInit(u8 increment, bool doVirtual)
    {
    DevSlot *dp;

    dp = channelAttach(ChClock, 0, DtRtc);

    dp->activate     = rtcActivate;
    dp->disconnect   = rtcDisconnect;
    dp->func         = rtcFunc;
    dp->io           = rtcIo;
    dp->selectedUnit = 0;

    activeChannel->ioDevice  = dp;
    activeChannel->hardwired = TRUE;

    if (increment == 0)
        {
        if (!rtcInitTick(doVirtual))
            {
            printf("(rtc    ) Invalid clock increment 0, defaulting to 1\n");
            increment = 1;
            }
        }
    rtcIncrement = increment;

    /*
    **  RTC channel may be active or inactive and empty or full
    **  depending on model.
    */
    rtcFull               = (features & HasFullRTC) != 0;
    activeChannel->full   = rtcFull;
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
    if (rtcIncrement == 0)
        {
        rtcReadUsCounter();
        }
    else
        {
        rtcClock += rtcIncrement;
        }
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

        return ((double)(endTime - startTime) / (Hz / 1000000.0L));
        }
    else
        {
        return 0.0;
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

#define MaxMicroseconds    1000

static void rtcReadUsCounter(void)
    {
    static bool first = TRUE;
    static u64  old   = 0;
    u64         new;

    if (rtcIncrement != 0)
        {
        return;
        }

    if (first)
        {
        first = FALSE;
        old   = rtcGetTick();
        }

    new = rtcGetTick();

    if (new > old)
        {
        rtcClockDelta = (u32)(new - old);
        if (rtcClockDelta > MaxMicroseconds)
            {
            rtcClockDelta     = MaxMicroseconds;
            rtcClockIsCurrent = FALSE;
            }
        else
            {
            rtcClockIsCurrent = TRUE;
            }
        old      += rtcClockDelta;
        rtcClock += rtcClockDelta;
        }
    else
        {
        rtcClockDelta = 0;
        }
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

    return (FcAccepted);
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
static bool rtcInitTick(bool doVirtual)
    {
    LARGE_INTEGER ctr;

    if (!QueryPerformanceFrequency(&rtcFrequency))
        {
        printf("(rtc    ) No high resolution hardware clock, using emulation cycle counter\n");

        return FALSE;
        }

    Hz = (double)rtcFrequency.QuadPart;
    printf("(rtc    ) Using QueryPerformanceCounter() clock at %f Hz\n", Hz);

    QueryPerformanceCounter(&rtcTimeReference);

    return TRUE;
    }

static u64 rtcGetTick(void)
    {
    LARGE_INTEGER ctr;

    QueryPerformanceCounter(&ctr);
    rtcMicroseconds += ((ctr.QuadPart - rtcTimeReference.QuadPart) * 1000000) / rtcFrequency.QuadPart;
    rtcTimeReference = ctr;

    return rtcMicroseconds;
    }

#elif defined(__GNUC__) && (defined(__linux__) || defined (__FreeBSD__) || defined (__APPLE__))

/*--------------------------------------------------------------------------
**  Purpose:        Low-level microsecond tick functions.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static bool rtcInitTick(bool doVirtual)
    {
    struct timespec ts;

    Hz         = 1000000.0L;
    rtcClockId = doVirtual ? CLOCK_PROCESS_CPUTIME_ID : CLOCK_MONOTONIC_RAW;
    if (clock_gettime(rtcClockId, &ts) != -1)
        {
        fprintf(stdout, "(rtc    ) Using %s time with clock_gettime()\n", doVirtual ? "process CPU" : "monotonic raw");

        return TRUE;
        }
    else
        {
        fputs("(rtc    ) No high resolution hardware clock, using emulation cycle counter\n", stdout);

        return FALSE;
        }
    }

static u64 rtcGetTick(void)
    {
    struct timespec ts;

    clock_gettime(rtcClockId, &ts);

    return ((u64)ts.tv_sec * (u64)1000000) + ((u64)ts.tv_nsec / (u64)1000);
    }

#else

static bool rtcInitTick(bool doVirtual)
    {
    fputs("(rtc    ) No high resolution hardware clock, using emulation cycle counter\n", stdout);

    return FALSE;
    }

#endif

/*---------------------------  End Of File  ------------------------------*/
