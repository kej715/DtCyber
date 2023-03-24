/*--------------------------------------------------------------------------
**
**  Copyright (c) 2017-2022, Steven Zoppi sjzoppi@gmail.com
**
**  Name: fsmon.cpp
**
**  Description:
**      Extends the functionality of the operator thread to also watch for
**      filesystem changes in the directories specified during initialization
**
**  20171110: SZoppi - Added Filesystem Watcher Support
**  20220701: SZoppi - Rewrite for platform independence
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
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "const.h"
#include "types.h"
#include "proto.h"
#include "dcc6681.h"

#if defined(_WIN32)
#include <windows.h>
#include <tchar.h>
#include "dirent_win.h"

#else

#include <pthread.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>

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
#if defined(_WIN32)
static void fsWatchThread(void *parms);

#else
static void *fsWatchThread(void *parms);

#endif

/*
**  ----------------
**  Public Variables
**  ----------------
*/
#if !defined(_WIN32)
extern int errno;
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
**  Purpose:        Extension of operator interface to watch for files in
**                  the card reader directory.
**
**  The flow here is:
**
**      Ordinarily, the card readers will be enhanced to check the input
**      directory "CRInput" for any remaining (unprocessed) files.  They
**      will be processed (in date order), then deposited into the "CROutput"
**      directory when completed.
**
**      When there are no files available in the "CRInput" directory, the card
**      reader routines (cr405 and cr3447) will cease to auto-load files and
**      will then need to be kick-started when a new file arrives.
**
**      This routine performs that notification in the form of a simulated
**      crXXXXLoadCards command dispatched to the appropriate handler.
**
**      Limitations:    We prefer to watch directories which are subordinate to the
**                      .ini file location to prevent a bunch of silliness that
**                      arises from file path parsing issues across various
**                      platforms.  This way, the .ini specification of a
**                      directory can be left "relative" and this will still
**                      work correctly.
**
**      On Windows      Watch for events in identified directory.
**
**                      When a notification comes in for a given
**                      card reader, we check the FCB for the device to see
**                      if it's busy processing and simply exit if so.  We
**                      can do this because the EOJ processing on the card
**                      reader will pick up the next file in line.
**
**                      If the FCB is NULL then we perform a LOAD CARDS
**                      call with the string parameter of * (asterisk)
**                      which tells the enhanced driver to pick up the next
**                      file in date order from the "CRInput" queue.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------
**  Purpose:        Create Filesystem Watcher thread.
**
**  Parameters:     Name        Description.
**                  parms       Pointer to the Thread Context Block
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
bool fsCreateThread(fswContext *parms)
    {
    bool noLaunch = TRUE;

#if (defined(_WIN32) || defined(__CYGWIN__))
    DWORD  dwThreadId;
    HANDLE hThread;

    /*
    **  Create filesystem watcher thread.
    */
    hThread = CreateThread(
        NULL,                               // no security attribute
        0,                                  // default stack size
        (LPTHREAD_START_ROUTINE)fsWatchThread,
        (LPTSTR)parms,                      // thread parameter
        0,                                  // not suspended
        &dwThreadId);                       // returns thread ID

    noLaunch = (hThread == NULL);
#else
    int            rc;
    pthread_t      thread;
    pthread_attr_t attr;

    /*
    **  Create POSIX thread with default attributes.
    */
    pthread_attr_init(&attr);
    rc       = pthread_create(&thread, &attr, fsWatchThread, parms);
    noLaunch = (rc != 0);
#endif

    if (noLaunch)
        {
        fprintf(stderr, "(fsmon  ) Failed to create Filesystem Watcher thread\n");

        return FALSE;
        }

    //  Success is when the returned thread is equal to zero
    return TRUE;
    }

#if defined(_WIN32)
static void fsWatchThread(void *parms)
#define fsReturn
#else
static void *fsWatchThread(void * parms)
#define fsReturn    0
#endif
    {
    /*
    **  Note that this thread will need to check for emulation termination
    **  in a window LESS than the wait time specified in main.c.
    **  main.c (as of this update) waits for 3 seconds.
    **
    **  So we ensure that it does.
    */
    fswContext *lparms = (fswContext *)parms;

    DevSlot *dp = NULL;

    int  dwChangeHandles[2] = { 0, 0 };
    u32  waitStatus         = 0;
    char *retPath;

    int  retval           = 0;
    char lpDir[MaxFSPath] = { "" };
    //      Just need to be large enough to hold the unit spec.
    char crDevId[16];

    struct dirent *curDirEntry;
    DIR           *curDir;

    //  Bring the Parameter List into the thread context
    sprintf(crDevId, "%02o,%02o,*",
            lparms->channelNo,
            lparms->eqNo);

    // Retrieve the full path name.
    retPath = realpath(lparms->inWatchDir, lpDir);

    if (retPath == NULL)
        {
        //  Handle an error condition.
        printf("(fsmon  ) 'realpath' function failed (%s)\n",
               strerror(errno));

        return fsReturn;
        }

    printf("(fsmon  ) Watching Directory:  %s\n", lpDir);

    //  Build the additional information for the operator command
    switch (lparms->devType)
        {
    case DtCr3447:
        dp = dcc6681FindDevice((u8)lparms->channelNo, (u8)lparms->eqNo, (u8)lparms->devType);
        break;

    case DtCr405:
        dp = channelFindDevice((u8)lparms->channelNo, (u8)lparms->devType);
        break;

    default:
        break;
        }

    if (dp == NULL)
        {
        printf("\n(fsmon  ) Cannot find device in Equipment Table"
               " Channel %o Equipment %o DeviceType %o"
               ".\n",
               lparms->channelNo,
               lparms->eqNo,
               lparms->devType);

        return fsReturn;
        }

    /*
    **  After exploring Operating System specific APIs for
    **  various filesystem watch strategies, Polling proved
    **  to be the most effective AND the most portable.
    **
    **  ALL of the OS-Specific APIs had serious draw-backs.
    */

    printf("(fsmon  ) Waiting ...\n");

    while (emulationActive)
        {
        u32 waitTimeout = readerScanSecs * 1000 * 2 / 3;
        sleepMsec(waitTimeout);

        /*
        **  Ensure the tray is empty/card reader isn't busy
        */
        if (dp->fcb[0] != NULL)
            {
            continue;
            }

        curDir = opendir(lparms->inWatchDir);
        do
            {
            //  See if there are files in the directory
            curDirEntry = readdir(curDir);
            if (curDirEntry == NULL)
                {
                break;
                }
            //  Pop over the dot (.) directories
            if (curDirEntry->d_name[0] == '.')
                {
                continue;
                }
            else
                {
                /*
                **  We have found at least ONE unprocessed file
                **  Therefore we invoke the card load command
                **  to pre-process and queue the deck.
                */
                opCmdLoadCards(FALSE, crDevId);
                break;
                }
            } while (curDirEntry != NULL);
        closedir(curDir);
        }

    /*
    **  The expectation is that we were passed a "calloc"ed
    **  context block.  So we must free it at the end of the
    **  thread's life
    */
    printf("(fsmon  ) Terminating Monitor Thread '%s'.\n", lparms->inWatchDir);

    free(lparms);
    return fsReturn;
    }
