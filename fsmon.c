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
#if defined(__linux__)
#include <sys/inotify.h>
#endif

/*
**  -----------------
**  Private Constants
**  -----------------
*/

#define BUFSIZE    4096

/*
**  -----------------------
**  Private Macro Functions
**  -----------------------
*/
#if !defined(_WIN32)
#define MAX_EVENTS       1024 /* Maximum number of events to process*/
#define LEN_NAME         16   /* Assuming that the length of the filename won't exceed 16 bytes*/
#define EVENT_SIZE       (sizeof(struct inotify_event))
#define EVENT_BUF_LEN    (MAX_EVENTS * (EVENT_SIZE + LEN_NAME))
#endif

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
static void *fsWatchDir(void *args);

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
    rc = pthread_create(&thread, &attr, &fsWatchDir, parms);
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
    sprintf(crDevId, "%o,%o,*",
            parms->channelNo,
            parms->eqNo);

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

    strcpy(lpDir, buffer);
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

    //  Build the additional information for the operator command
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
            **  TODO: Refactor this and the Windows code
            **        as they are functionally equal
            **
            **  In Windows, we were awaked by a filesystem
            **  event or a timeout event, so we should
            **  process all of the files that we can find.
            */

            /*
            **  Ensure the tray is empty.
            **  TODO: attempt to QUEUE the file if space
            **        on the input list is available.
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
                    //  parms->LoadCards("*",parms->channelNo,parms->eqNo,stdout,"");
                    opCmdLoadCards(FALSE, crDevId);
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

#elif defined(__linux__)
static void * fsWatchDir(void *arg)
    {
    fswContext *parms = arg;

    int           length;
    int           i = 0;
    int           fd;
    int           wd;
    int           flags;
    char          eventBuffer[EVENT_BUF_LEN];
    char          pathBuffer[_MAX_PATH];
    char          lpDir[_MAX_PATH] = "";
    char          crDevId[16]      = "";
    struct dirent *curDirEntry;
    DIR           *curDir;
    DevSlot       *dp = NULL;

    char *cur_event_filename    = NULL;
    char *cur_event_file_or_dir = NULL;

    //  Bring the Parameter List into the thread context
    sprintf(crDevId, "%o,%o,*",
            parms->channelNo,
            parms->eqNo);

    // Retrieve the full path name.
    realpath(parms->inWatchDir, pathBuffer);

    /* This is the watch descriptor the event occurred on */

    printf("(fsmon  ) Watching requested for path '%s'\n", pathBuffer);

    /*
     * creating the INOTIFY instance
     */
    fd = inotify_init();
    if (fd < 0)
        {
        printf("(fsmon  ) ERROR: inotify_init FAILED. '%s'\n", strerror(errno));

        return 0;
        }

    /*
     * The suggestion is to validate the existence
     * of the directory before adding into monitoring list.
     */
    wd = inotify_add_watch(fd, parms->inWatchDir, IN_CREATE | IN_DELETE);
    if (wd == -1)
        {
        printf("(fsmon  ) ERROR: Could not Watch : '%s'\n", parms->inWatchDir);

        return 0;
        }
    else
        {
        printf("(fsmon  ) Watching : %s WD=%d\n", parms->inWatchDir, wd);
        }

    //  Build the additional information for the operator command
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

        return 0;
        }


    while (emulationActive)
        {
        /*
         * Read to ensure that the event change happens on directory.
         * This read will block until the change event occurs
         */

        length = read(fd, eventBuffer, EVENT_BUF_LEN);

        /*
         * checking for error
         */
        if (length < 0)
            {
            printf("(fsmon  ) ERROR: inotify 'read' FAILED. '%s'\n", strerror(errno));
            }

        printf("(fsmon  ) READ: File Descriptor:%lX Length:%lX\n",
               fd, length);

        /*
         * 'read' returns the list of change events that happened.
         * Read the change event one by one and process it accordingly.
         */
        i = 0;
        while (i < length)
            {
            struct inotify_event *event = (struct inotify_event *)&eventBuffer[i];

            /*
            **  Snag local copies for debugging
            */

            printf("(fsmon  ) EVENT: MASK:%lX %s \"%s\" on WD #%i\n",
                   event->mask, (event->mask & IN_ISDIR) ? "Dir" : "File", event->name, event->wd);

#if defined (DEBUG)
            u32 cur_event_wd     = event->wd;
            u32 cur_event_cookie = event->cookie;
            u32 cur_event_len    = event->len;
            u32 cur_event_mask   = event->mask;

            if (event->len)
                {
                cur_event_filename = event->name;
                }
            if (event->mask & IN_ISDIR)
                {
                cur_event_file_or_dir = "Dir";
                }
            else
                {
                cur_event_file_or_dir = "File";
                }

            flags = event->mask &
                    ~(IN_ALL_EVENTS | IN_UNMOUNT | IN_Q_OVERFLOW | IN_IGNORED);

            /* Perform event dependent handler routines */
            /* The mask is the magic that tells us what file operation occurred */

            switch (event->mask & (IN_ALL_EVENTS | IN_UNMOUNT | IN_Q_OVERFLOW | IN_IGNORED))
                {
            /* File was accessed */
            case IN_ACCESS:
                printf("(fsmon  ) INFO: ACCESS: %s \"%s\" on WD #%i\n",
                       cur_event_file_or_dir, cur_event_filename, cur_event_wd);
                break;

            /* File changed attributes */
            case IN_ATTRIB:
                printf("(fsmon  ) INFO: ATTRIB: %s \"%s\" on WD #%i\n",
                       cur_event_file_or_dir, cur_event_filename, cur_event_wd);
                break;

            /* File open for writing was closed */
            case IN_CLOSE_WRITE:
                printf("(fsmon  ) INFO: CLOSE_WRITE: %s \"%s\" on WD #%i\n",
                       cur_event_file_or_dir, cur_event_filename, cur_event_wd);
                break;

            /* File open read-only was closed */
            case IN_CLOSE_NOWRITE:
                printf("(fsmon  ) INFO: CLOSE_NOWRITE: %s \"%s\" on WD #%i\n",
                       cur_event_file_or_dir, cur_event_filename, cur_event_wd);
                break;

            /* File was created from X */
            case IN_CREATE:
                printf("(fsmon  ) INFO: CREATED: %s \"%s\" on WD #%i. Cookie=%d\n",
                       cur_event_file_or_dir, cur_event_filename, cur_event_wd,
                       cur_event_cookie);
                break;

            /* File was created from X */
            case IN_DELETE:
                printf("(fsmon  ) INFO: DELETED: %s \"%s\" on WD #%i. Cookie=%d\n",
                       cur_event_file_or_dir, cur_event_filename, cur_event_wd,
                       cur_event_cookie);
                break;

            case IN_DELETE_SELF:
                printf("(fsmon  ) INFO: DELETED_SELF: %s \"%s\" on WD #%i. Cookie=%d\n",
                       cur_event_file_or_dir, cur_event_filename, cur_event_wd,
                       cur_event_cookie);
                break;

            /* File was modified */
            case IN_MODIFY:
                printf("(fsmon  ) INFO: MODIFY: %s \"%s\" on WD #%i\n",
                       cur_event_file_or_dir, cur_event_filename, cur_event_wd);
                break;

            case IN_MOVE_SELF:
                printf("(fsmon  ) INFO: MOVE_SELF: %s \"%s\" on WD #%i\n",
                       cur_event_file_or_dir, cur_event_filename, cur_event_wd);
                break;

            /* File was moved from X */
            case IN_MOVED_FROM:
                printf("(fsmon  ) INFO: MOVED_FROM: %s \"%s\" on WD #%i. Cookie=%d\n",
                       cur_event_file_or_dir, cur_event_filename, cur_event_wd,
                       cur_event_cookie);
                break;

            case IN_MOVED_TO:
                printf("(fsmon  ) INFO: MOVED_TO: %s \"%s\" on WD #%i. Cookie=%d\n",
                       cur_event_file_or_dir, cur_event_filename, cur_event_wd,
                       cur_event_cookie);
                break;

            /* File was opened */
            case IN_OPEN:
                printf("(fsmon  ) INFO: OPEN: %s \"%s\" on WD #%i\n",
                       cur_event_file_or_dir, cur_event_filename, cur_event_wd);
                break;

            /*
            **  Watch was removed explicitly by
            **  inotify_rm_watch or automatically
            **  because file was deleted,
            **  or file system was unmounted.
            */
            case IN_IGNORED:
                printf("(fsmon  ) INFO: IGNORED: WD #%d\n", cur_event_wd);
                break;

            /* Some unknown message received */
            default:
                printf("(fsmon  ) INFO: UNKNOWN EVENT \"%X\" OCCURRED for file \"%s\" on WD #%i\n",
                       event->mask, cur_event_filename, cur_event_wd);
                break;
                } // switch

            /* If any flags were set other than IN_ISDIR, report the flags */
            if (flags & (~IN_ISDIR))
                {
                flags = event->mask;
                printf("(fsmon  ) INFO: Flags=%lX\n", flags);
                }
#endif
            i += EVENT_SIZE + event->len;
            } // while ( i < length )

        /*
        **  TODO: Refactor this and the Windows code
        **        as they are functionally equal
        **
        **  We were awaked by SOMETHING so we should
        **  process all of the files that we can find.
        **
        **  The inotify API doesn't support a "Timeout"
        **  interval so we will need to figure out a way
        **  to periodically poll the target.
        */

        /*
        **  Ensure the tray is empty.
        **  TODO: attempt to QUEUE the file if space
        **        on the input list is available.
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
                //  TODO: We can remove the prototype from
                // parms->LoadCards("*",parms->channelNo,parms->eqNo,stdout,"");
                opCmdLoadCards(FALSE, crDevId);
                break;
                }
            } while (curDirEntry != NULL);
        closedir(curDir);


        printf("(fsmon  ) Continuing to watch ... '%s'.\n", parms->inWatchDir);
        } // while (emulationactive)

    /*
     * removing the directory from the watch list.
     */
    inotify_rm_watch(fd, wd);

    /*
     * closing the INOTIFY instance
     */
    close(fd);

    printf("(fsmon  ) Terminating Monitor Thread '%s'.\n", parms->inWatchDir);
    free(parms);

    return 0;
    }

#else
static void * fsWatchDir(void *arg)
    {
    fputs("(fsmon  ) Terminating Monitor Thread - not yet supported.\n", stdout);

    return 0;
    }

#endif
