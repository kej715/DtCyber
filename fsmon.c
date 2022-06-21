/*--------------------------------------------------------------------------
**
**  Copyright (c) 2017, Steven Zoppi sjzoppi@gmail.com
**
**  Name: fsmon.cpp
**
**  Description:
**      Extends the functionality of the operator thread to also watch for
**      filesystem changes in the directories specified during initialization
**
**  20171110: SZoppi - Added Filesystem Watcher Support
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
#endif

/*
**  -----------------
**  Private Constants
**  -----------------
*/

#define BUFSIZE          4096
#define LONG_DIR_NAME    TEXT("c:\\longdirectoryname")

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
static void fsWatchDir(fswContext *parms);

#else
static void *fsWatchDir(fswContext *parms);

#endif

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
#if defined(_WIN32)
#include <conio.h>
#endif

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
#if defined(_WIN32)
    DWORD  dwThreadId;
    HANDLE hThread;

    /*
    **  Create filesystem watcher thread.
    */
    hThread = CreateThread(
        NULL,                               // no security attribute
        0,                                  // default stack size
        (LPTHREAD_START_ROUTINE)fsWatchDir,
        (LPTSTR)parms,                      // thread parameter
        0,                                  // not suspended
        &dwThreadId);                       // returns thread ID

    if (hThread == NULL)
        {
        fprintf(stderr, "(fsmon  ) Failed to create Filesystem Watcher thread\n");

        return (FALSE);
        }

    //  Success is when the returned thread is not equal to zero
    return (TRUE);
#else
    int            rc;
    pthread_t      thread;
    pthread_attr_t attr;

    /*
    **  Create POSIX thread with default attributes.
    */
    pthread_attr_init(&attr);
    rc = pthread_create(&thread, &attr, fsWatchDir, NULL);
    if (rc < 0)
        {
        fprintf(stderr, "(fsmon  ) Failed to create Filesystem Watcher thread\n");

        return (FALSE);
        }

    //  Success is when the returned thread is equal to zero
    return (TRUE);
#endif
    }

#if defined(_WIN32)
static void fsWatchDir(fswContext *parms)
    {
    DWORD dwWaitStatus;

    /*
    **  Note that this thread will need to check for emulation termination
    **  in a window LESS than the wait time specified in main.c.
    **  main.c (as of this update) waits for 3 seconds.
    */
    DWORD   dwWaitTimeout = (DWORD)1000;
    HANDLE  dwChangeHandles[2];
    DevSlot *dp = NULL;


    //  Translate the input filename to a windows name

    DWORD         retval           = 0;
    TCHAR         buffer[BUFSIZE]  = TEXT("");
    TCHAR         **lppPart        = { NULL };
    char          lpDir[_MAX_PATH] = "";
    char          crDevId[16]      = "";
    struct dirent *curDirEntry;
    DIR           *curDir;

    //  Bring the Parameter List into the thread context
#if defined(SAFECALLS)
    sprintf_s(crDevId, sizeof(crDevId), "%o,%o,*",
              parms->channelNo,
              parms->eqNo);
#else
    sprintf(crDevId, "%o,%o,*",
        parms->channelNo,
        parms->eqNo);
#endif

    // Retrieve the full path name.

    retval = GetFullPathName(parms->inWatchDir,
                             BUFSIZE,
                             buffer,
                             lppPart);

    if (retval == 0)
        {
        // Handle an error condition.
        printf("(fsmon  ) GetFullPathName failed (%d)\n", GetLastError());

        return;
        }
    else
        {
        // printf("(fsmon  ) The full file name is:  %s\n", buffer);
        if ((lppPart != NULL) && (*lppPart != 0))
            {
            printf("(fsmon  ) Final Component in Name:  %s\n", *lppPart);
            }
        }

#if defined(SAFECALLS)
    strcpy_s(lpDir, sizeof(lpDir), buffer);
#else
    strcpy(lpDir, buffer);
#endif
    printf("(fsmon  ) Watching Directory:  %s\n", lpDir);

    //  File Change Watch Invocation code.
    //  Watch the directory for file creation and deletion.

    dwChangeHandles[0] = FindFirstChangeNotification(
        lpDir,                          // directory to watch
        TRUE,                           // do not watch subtree
        FILE_NOTIFY_CHANGE_FILE_NAME);  // watch file name changes

    if (dwChangeHandles[0] == INVALID_HANDLE_VALUE)
        {
        printf("\n(fsmon  ) FindFirstChangeNotification function failed. '%s'\n", strerror(GetLastError()));

        return;
        }

    // Watch the subtree for directory creation and deletion.

    dwChangeHandles[1] = FindFirstChangeNotification(
        lpDir,                         // directory to watch
        FALSE,                         // watch the subtree
        FILE_NOTIFY_CHANGE_DIR_NAME);  // watch dir name changes

    if (dwChangeHandles[0] == INVALID_HANDLE_VALUE)
        {
        printf("\n(fsmon  ) FindFirstChangeNotification function failed. '%s'\n", strerror(GetLastError()));

        return;
        }


    // Make a final validation check on our handles.

    if ((dwChangeHandles[1] == NULL))
        {
        printf("\n(fsmon  ) Unexpected NULL from FindFirstChangeNotification.\n");

        return;
        }
    switch (parms->devType)
        {
    case DtCr3447:
        dp = dcc6681FindDevice((u8)parms->channelNo, (u8)parms->eqNo, (u8)parms->devType);
        break;

    case DtCr405:
        dp = channelFindDevice((u8)parms->channelNo, (u8)parms->devType);
        break;

    default:
        break;
        }
    if (dp == NULL)
        {
        printf("\n(fsmon  ) Cannot find device in Equipment Table"
               " Channel %o Equipment %o DeviceType %o"
               ".\n",
               parms->channelNo,
               parms->eqNo,
               parms->devType);

        return;
        }


    // Change notification is set. Now wait on both notification
    // handles and refresh accordingly.

    printf("(fsmon  ) Waiting for Change Notification ...\n");

    while (emulationActive)
        {
        // Wait for notification.
        dwWaitStatus = WaitForMultipleObjects(2, dwChangeHandles,
                                              FALSE, dwWaitTimeout);

        switch (dwWaitStatus)
            {
        case WAIT_TIMEOUT:

        //  A timeout occurred, this would happen if some value other
        //  than INFINITE is used in the Wait call and no changes occur.
        //  In a single-threaded environment you might not want an
        //  INFINITE wait.

        //  Fall Through ...
        case WAIT_OBJECT_0:

            // A file was created, renamed, or deleted in the directory.
            // Refresh this directory and restart the notification.

            /*
            **  Ensure the tray is empty.
            */
            if (dp->fcb[0] != NULL)
                {
                continue;
                }

            curDir = opendir(parms->inWatchDir);
            do
                {
                //  See if there are files in the directory
                curDirEntry = readdir(curDir);
                if (curDirEntry == NULL)
                    {
                    continue;
                    }
                //  Pop over the dot (.) directories
                if (curDirEntry->d_name[0] == '.')
                    {
                    continue;
                    }
                else
                    {
                    //  We have found an unprocessed file
                    //  invoke the card load command
                    parms->LoadCards(crDevId);
                    break;
                    }
                } while (curDirEntry != NULL);
            closedir(curDir);


            if (FindNextChangeNotification(dwChangeHandles[0]) == FALSE)
                {
                printf("(fsmon  ) ERROR: FindFirstChangeNotification function failed. '%s'\n", strerror(GetLastError()));
                }
            break;

        case WAIT_OBJECT_0 + 1:

            // A directory was created, renamed, or deleted.
            // Refresh the tree and restart the notification.

            printf("(fsmon  ) Directory tree (%s) changed.\n", lpDir);
            if (FindNextChangeNotification(dwChangeHandles[1]) == FALSE)
                {
                printf("(fsmon  ) ERROR: FindFirstChangeNotification function failed. '%s'\n", strerror(GetLastError()));
                }
            break;

        default:
            printf("(fsmon  ) ERROR: Unhandled dwWaitStatus 0x%08x.\n", dwWaitStatus);
            break;
            }
        }

    /*
    **  The expectation is that we were passed a "calloc"ed
    **  context block.  So we must free it at the end of the
    **  thread's life
    */
    printf("(fsmon  ) Terminating Monitor Thread '%s'.\n", parms->inWatchDir);

    free(parms);
    }

#else
static void *fsWatchDir(fswContext *parms)
    {
    fprintf(stderr, "(fsmon  ) Filesystem Watcher Not Implemented For This Platform.\n");
    }

#endif
