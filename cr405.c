/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**            (c) 2017       Steven Zoppi 10-Nov-2017
**                           Added status messaging support
**
**  Name: cr405b.c
**
**  Description:
**      Perform emulation of channel-connected CDC 405-B card reader.
**      It does not use a 3000 series channel converter.
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
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#if defined(_WIN32)

//  Filesystem Watcher Machinery

#include <windows.h>
#include "dirent_win.h"
#else
#include <dirent.h>
#include <unistd.h>
#include <ctype.h>
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
**  CDC 405 card reader function and status codes.
**
**      Function codes
**
**      ----------------------------------
**      |  Equip select  |   function    |
**      ----------------------------------
**      11              6 5             0
**
**      0700 = Deselect
**      0701 = Gate Card to Secondary bin
**      0702 = Read Non-stop
**      0704 = Status request
**
**      Note: To read one card, execute successive S702 and
**      S704 functions.
**      One column of card data per 12-bit data word.
*/
#define FcCr405Deselect       00700
#define FcCr405GateToSec      00701
#define FcCr405ReadNonStop    00702
#define FcCr405StatusReq      00704

/*
**      Status reply
**
**      0000 = Ready
**      0001 = Not ready
**      0002 = End of file
**      0004 = Compare error
**
*/
#define StCr405Ready          00000
#define StCr405NotReady       00001
#define StCr405EOF            00002
#define StCr405CompareErr     00004

#define Cr405MaxDecks         128

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
typedef struct cr405Context
    {
    /*
    **  Info for show_tape operator command.
    */
    struct cr405Context *nextUnit;
    u8                  channelNo;
    u8                  eqNo;
    u8                  unitNo;

    const u16           *table;
    u32                 getCardCycle;
    int                 col;
    PpWord              card[80];
    int                 inDeck;
    int                 outDeck;
    char                *decks[Cr405MaxDecks];

    char                *curFileName;
    char                *dirInput;
    char                *dirOutput;
    int                 seqNum;
    bool                isWatched;
    } Cr405Context;

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static FcStatus cr405Func(PpWord funcCode);
static void cr405Io(void);
static void cr405Activate(void);
static void cr405Disconnect(void);
static void cr405NextCard(DevSlot *dp);
static bool cr405StartNextDeck(DevSlot *dp, Cr405Context *cc);
static void cr405SwapInOut(Cr405Context *cc, char *fname);

/*
**  ----------------
**  Public Variables
**  ----------------
*/
#if DEBUG
static FILE *cr405Log = NULL;
#endif

/*
**  -----------------
**  Private Variables
**  -----------------
*/
static Cr405Context *firstCr405 = NULL;
static Cr405Context *lastCr405  = NULL;

/*
 **--------------------------------------------------------------------------
 **
 **  Public Functions
 **
 **--------------------------------------------------------------------------
 */

/*--------------------------------------------------------------------------
**  Purpose:        Initialise card reader.
**
**  Parameters:     Name        Description.
**                  eqNo        equipment number
**                  unitNo      unit number
**                  channelNo   channel number the device is attached to
**                  deviceName  optional device file name
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cr405Init(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName)
    {
    Cr405Context *cc;
    char         *crInput;
    char         *crOutput;
    DevSlot      *dp;
    int          len;
    struct stat  s;
    fswContext   *threadParms;
    char         *tokenAuto;
    bool         watchRequested;
    char         *xlateTable;

    /*
    **  For FileSystem Watcher
    **
    **  We pass the fswContext structure to be used by the thread.
    **
    **  deviceName is the "space delimited" remainder of the .ini
    **  file line.
    **
    **  It can have up to three additional parameters of which NONE
    **  may contain a space.  Specifying Parameters 2 or 3 REQUIRES
    **  specification of the Previous parameter.  You may not specify
    **  Parameter 2 without Parameter 1.
    **
    **  Parameter   Description
    **  ---------   -------------------------------------------------------
    **
    **      1       <Optional:> "*"=NULL Placeholder|"026"|"029" Default="026"
    **              "026" or "029" <Translate Table Specification>
    **
    **      2       <optional> "*"=NULL Placeholder|CRInputFolder
    **
    **              The directory of the card reader's virtual "hopper"
    **              although the user can still load cards directly through
    **              the operator interface, a directory can be specified
    **              into which card decks can be submitted for sequential
    **              processing based on create date.
    **
    **              NO Thread will be created if this parameter doesn't exist.
    **
    **      3       <optional> "*"=NULL Placeholder|CROutputFolder
    **
    **              If specified, indicates the directory into which the
    **              processed cards will be deposited.  Naming conflicts
    **              will be serialized with a suffix.
    **
    **              If not specified, all input files will simply be deleted
    **              upon closure.
    **
    **      4       <optional> "*"=NULL Placeholder|"AUTO"|"NOAUTO" Default = "Auto"
    **              If a Virtual Card Hopper is defined, then this
    **              parameter indicates whether or not to initiate a Filewatcher
    **              thread to automatically submit jobs in file creation order
    **              from the CRInputFolder
    **
    **  The context can be declared locally because it's just
    **  a structure used to marshal the parameters into the
    **  thread's context.
    */

#if DEBUG
    if (cr405Log == NULL)
        {
        cr405Log = fopen("cr405Log.txt", "wt");
        }
#endif

    if (eqNo != 0)
        {
        logDtError(LogErrorLocation, "Invalid equipment number - hardwired to equipment number 0\n");
        exit(1);
        }

    if (unitNo != 0)
        {
        logDtError(LogErrorLocation, "Invalid unit number - hardwired to unit number 0\n");
        exit(1);
        }

    dp = channelAttach(channelNo, eqNo, DtCr405);

    dp->activate     = cr405Activate;
    dp->disconnect   = cr405Disconnect;
    dp->func         = cr405Func;
    dp->io           = cr405Io;
    dp->selectedUnit = 0;

    /*
    **  Only one card reader unit is possible per equipment.
    */
    if (dp->context[0] != NULL)
        {
        logDtError(LogErrorLocation, "Only one unit is possible per equipment\n");
        exit(1);
        }

    cc = calloc(1, sizeof(Cr405Context));
    if (cc == NULL)
        {
        logDtError(LogErrorLocation, "Failed to allocate context block\n");
        exit(1);
        }

    dp->context[0] = (void *)cc;

    threadParms = calloc(1, sizeof(fswContext));    //  Need to check for null result
    if (cc == NULL)
        {
        logDtError(LogErrorLocation, "Failed to allocate CR3447 FileWatcher Context block\n");
        exit(1);
        }

    // threadParms->LoadCards = 0;
    if (deviceName == NULL)
        {
        deviceName = "";
        }
    xlateTable = strtok(deviceName, ", ");
    crInput    = strtok(NULL, ", ");
    crOutput   = strtok(NULL, ", ");
    tokenAuto  = strtok(NULL, ", ");

    /*
    **  Process the request for file system watcher
    */
    watchRequested = TRUE;     // Default = uun filewatcher thread
    if (tokenAuto != NULL)
        {
        if (strcasecmp(tokenAuto, "noauto") == 0)
            {
            watchRequested = FALSE;
            }
        else if ((strcasecmp(tokenAuto, "auto") != 0) && (strcasecmp(tokenAuto, "*") != 0))
            {
            logDtError(LogErrorLocation, "Unrecognized Automation Type '%s'\n", tokenAuto);
            exit(1);
            }
        }

    fprintf(stdout, "(cr405  ) File watcher %s requested\n", watchRequested ? "was" : "was not");


    /*
    **  Setup character set translation table.
    */
    cc->table     = asciiTo026; // default translation table
    cc->isWatched = FALSE;

    cc->channelNo = channelNo;
    cc->eqNo      = eqNo;
    cc->unitNo    = unitNo;
    cc->dirInput  = NULL;
    cc->dirOutput = NULL;

    if (xlateTable != NULL)
        {
        if (strcasecmp(xlateTable, "029") == 0)
            {
            cc->table = asciiTo029;
            }
        else if ((strcasecmp(xlateTable, "026") != 0)
                 && (strcmp(xlateTable, "*") != 0)
                 && (strcmp(xlateTable, "") != 0))
            {
            logDtError(LogErrorLocation, "Unrecognized card code name %s\n", xlateTable);
            exit(1);
            }
        }

    fprintf(stdout, "(cr405  ) Card code selected '%s'\n", xlateTable);

    /*
    **  Incorrect specifications for input / output directories
    **  are fatal.  Even though files can still be submitted
    **  through the operator interface, we want the parameters
    **  supplied through the ini file to be correct from the start.
    */

    if ((crOutput != NULL) && (strcmp(crOutput, "*") != 0))
        {
        if (stat(crOutput, &s) != 0)
            {
            logDtError(LogErrorLocation, "The Output location specified '%s' does not exist.\n", crOutput);
            exit(1);
            }

        if ((s.st_mode & S_IFDIR) == 0)
            {
            logDtError(LogErrorLocation, "The Output location specified '%s' is not a directory.\n", crOutput);
            exit(1);
            }
        len = (int)strlen(crOutput);
        threadParms->outDoneDir = (char *)calloc(len + 1, 1);
        cc->dirOutput           = (char *)calloc(len + 1, 1);
        if ((threadParms->outDoneDir == NULL) || (cc->dirOutput == NULL))
            {
            fputs("(cr405  ) Failed to allocate storage for output directory path\n", stderr);
            exit(1);
            }
        memcpy(threadParms->outDoneDir, crOutput, len);
        memcpy(cc->dirOutput, crOutput, len);
        fprintf(stdout, "(cr405  ) Submissions will be preserved in '%s'.\n", crOutput);
        }
    else
        {
        fputs("(cr405  ) Submissions will be purged after processing.\n", stdout);
        }

    if ((crInput != NULL) && (strcmp(crInput, "*") != 0))
        {
        if (stat(crInput, &s) != 0)
            {
            logDtError(LogErrorLocation, "The Input location specified '%s' does not exist.\n", crInput);
            exit(1);
            }

        if ((s.st_mode & S_IFDIR) == 0)
            {
            logDtError(LogErrorLocation, "The Input location specified '%s' is not a directory.\n", crInput);
            exit(1);
            }
        //  We only care about the "Auto" "NoAuto" flag if there is a good input location

        /*
        **  The thread needs to know what directory to watch.
        **
        **  The Card Reader Context needs to remember what directory
        **  will supply the input files so more can be found at EOD.
        */
        len = (int)strlen(crInput);
        threadParms->inWatchDir = (char *)calloc(len + 1, 1);
        cc->dirInput            = (char *)calloc(len + 1, 1);
        if ((threadParms->inWatchDir == NULL) || (cc->dirInput == NULL))
            {
            fputs("(cr405  ) Failed to allocate storage for input directory path\n", stderr);
            exit(1);
            }
        memcpy(threadParms->inWatchDir, crInput, len);
        memcpy(cc->dirInput, crInput, len);

        threadParms->eqNo      = eqNo;
        threadParms->unitNo    = unitNo;
        threadParms->channelNo = channelNo;
        // threadParms->LoadCards = cr405LoadCards;
        threadParms->devType = DtCr405;

        /*
        **  At this point, we should have a completed context
        **  and should be ready to launch the thread and pass
        **  the context along.  We don't free the block, the
        **  thread will do that if it is launched correctly.
        */

        /*
        **  Now establish the filesystem watcher thread.
        **
        **  It is non-fatal if the filesystem watcher thread
        **  cannot be started.
        */
        cc->isWatched = FALSE;
        if (watchRequested)
            {
            cc->isWatched = fsCreateThread(threadParms);
            if (!cc->isWatched)
                {
                printf("(cr405  ) Unable to create filesystem watch thread for '%s'.\n", crInput);
                printf("          Card Loading is still possible via Operator Console.\n");
                }
            else
                {
                printf("(cr405  ) Filesystem watch thread for '%s' created successfully.\n", crInput);
                }
            }
        else
            {
            printf("(cr405  ) Filesystem watch thread not requested for '%s'.\n", crInput);
            printf("          Card Loading is required via Operator Console.\n");
            }
        }

    cc->col = 80;

    /*
    **  Print a friendly message.
    */
    printf("(cr405  ) Initialised on channel %o equipment %o type '%s'\n",
           channelNo,
           eqNo,
           cc->table == asciiTo026 ? "026" : "029");

    if (!cc->isWatched)
        {
        free(threadParms);
        }

    /*
    **  Link into list of 405 Card Reader units.
    */
    if (lastCr405 == NULL)
        {
        firstCr405 = cc;
        }
    else
        {
        lastCr405->nextUnit = cc;
        }

    lastCr405 = cc;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Load cards on 405 card reader.
**
**  Parameters:     Name        Description.
**                  fname       Pathname of file containing card deck
**                  channelNo   Channel number of card reader
**                  equipmentNo Equipment number of card reader
**                  params      Parameter list supplied on command line
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cr405LoadCards(char *fname, int channelNo, int equipmentNo, char *params)
    {
    Cr405Context *cc;
    DevSlot      *dp;
    int          len;
    char         outBuf[MaxFSPath + 128];
    struct stat  s;
    char         *sp;

    /*
    **  Locate the device control block.
    */
    dp = channelFindDevice((u8)channelNo, DtCr405);
    if (dp == NULL)
        {
        return;
        }

    cc = (Cr405Context *)(dp->context[0]);

    /*
    **  Ensure the tray is not full.
    */
    if (((cc->inDeck + 1) % Cr405MaxDecks) == cc->outDeck)
        {
        opDisplay("(cr405  ) Input tray full\n");

        return;
        }

    //  At this point we should have a valid(ish) input file
    //  Make sure before enqueueing it.

    if (stat(fname, &s) != 0)
        {
        sprintf(outBuf, "(cr405  ) Requested file '%s' not found. (%s).\n", fname, strerror(errno));
        opDisplay(outBuf);

        return;
        }

    //  Enqueue the file in the chain of pending files

    len = (int)strlen(fname) + 1;
    sp  = (char *)malloc(len);
    memcpy(sp, fname, len);
    cc->decks[cc->inDeck] = sp;
    cc->inDeck            = (cc->inDeck + 1) % Cr405MaxDecks;

    if (dp->fcb[0] == NULL)
        {
        if (cr405StartNextDeck(dp, cc))
            {
            return;
            }
        }

    //  Starting the next deck was not possible

    dp->fcb[0] = NULL;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Get the next available deck named in the
**                  405 card reader's input directory.
**
**  Parameters:     Name        Description.
**                  fname       (in/out) string buffer for next file name
**                              we don't change anything input
**                              unless there's an existing file.
**                  channelNo   Channel number of card reader
**                  equipmentNo Equipment number of card reader
**                  params      Parameter list supplied on command line
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cr405GetNextDeck(char *fname, int channelNo, int equipmentNo, char *params)
    {
    Cr405Context  *cc;
    DIR           *curDir;
    struct dirent *curDirEntry;
    DevSlot       *dp;
    char          fOldest[MaxFSPath * 2 + 2] = "";
    char          outBuf[MaxFSPath * 2 + 128];
    struct stat   s;
    char          strWork[MaxFSPath * 2 + 2] = "";
    time_t        tOldest = 0;

    //  Safety check, we only respond if the first
    //  character of the filename is an asterisk '*'

    if (*fname != '*')
        {
        sprintf(outBuf, "(cr405  ) GetNextDeck called with improper parameter '%s'.\n", fname);
        opDisplay(outBuf);

        return;
        }

    /*
    **  Locate the device control block.
    */
    dp = channelFindDevice((u8)channelNo, DtCr405);
    if (dp == NULL)
        {
        return;
        }

    cc = (Cr405Context *)(dp->context[0]);

    /*
    **  Ensure the tray is not full.
    */
    if (((cc->inDeck + 1) % Cr405MaxDecks) == cc->outDeck)
        {
        opDisplay("(cr405  ) Input tray full\n");

        return;
        }

    /*
    **  The special filename, asterisk(*)
    **  means "Load the next deck" from the dirInput
    **  directory (if it's defined).
    **
    **  This routine is called from operator.c during
    **  preparation of the input card deck when it detects
    **  the filename specified as "*".
    **
    **  in that case, we don't link the deck into the chain.
    **  this flow in date order.
    **
    **  The asterisk convention works even if the
    **  filewatcher thread cannot be started.  It
    **  simply means: "Pick the next oldest file
    **  found in the input directory.
    */
    if (cc->dirInput == NULL)
        {
        opDisplay("(cr405  ) No card reader directory has been specified on the device declaration.\n");
        opDisplay("(cr405  ) The 'Load Next Deck' request is ignored.\n");

        return;
        }

    curDir = opendir(cc->dirInput);

    /*
    **  Scan the input directory (if specified)
    **  for the oldest file queued.
    **  If one is found, we replace the
    **  asterisk with the name of the found file.
    **  and continue processing.
    */
    do
        {
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

        sprintf(strWork, "%s/%s", cc->dirInput, curDirEntry->d_name);
        stat(strWork, &s);
        if (fOldest[0] == '\0')
            {
            strcpy(fOldest, strWork);
            tOldest = s.st_ctime;
            }
        else
            {
            if (s.st_ctime > tOldest)
                {
                strcpy(fOldest, strWork);
                tOldest = s.st_ctime;
                }
            }
        } while (curDirEntry != NULL);

    if (fOldest[0] != '\0')
        {
        sprintf(outBuf, "(cr405  ) Dequeueing unprocessed file '%s' from '%s'.\n", fOldest, cc->dirInput);
        opDisplay(outBuf);

        /*
        **  To complement the functionality of the operator.c process,
        **  there is an expectation that the file provided must be
        **  preprocessed.
        **
        **  When the input file is dequeued, and if there exists a
        **  crOutDir, then we MOVE the file to the output directory
        **  before dequeueing it.
        **
        **  if there is no output directory, we don't bother changing
        **  the name.
        */
        strcpy(fname, fOldest);
        cr405SwapInOut(cc, fname);
        }
    else
        {
        sprintf(outBuf, "(cr405  ) No files found in '%s'.\n", cc->dirInput);
        opDisplay(outBuf);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Cleanup files located in the dirInput
**                  virtual card reader hopper.
**
**  Parameters:     Name        Description.
**                  fname       (in) string buffer for file name
**                              to be tested for removal.
**                  channelNo   Channel number of card reader
**                  equipmentNo Equipment number of card reader
**                  params      Parameter list supplied on command line
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cr405PostProcess(char *fname, int channelNo, int equipmentNo, char *params)
    {
    Cr405Context *cc;
    DevSlot      *dp;
    char         outBuf[MaxFSPath * 2 + 128];

    /*
    **  Locate the device control block.
    */

    dp = channelFindDevice((u8)channelNo, DtCr405);
    if (dp == NULL)
        {
        return;
        }

    cc = (Cr405Context *)(dp->context[0]);

    if (cc->dirInput == NULL)
        {
        sprintf(outBuf, "(cr405  ) Submitted deck '%s' processing complete.\n", fname);
        opDisplay(outBuf);

        return;
        }

    bool isFromInput = strncmp(fname, cc->dirInput, strlen(cc->dirInput)) == 0;

    //  There should be no expectation that the file needs to be preserved
    if (isFromInput)
        {
        sprintf(outBuf, "(cr405  ) Purging Submitted Deck '%s'.\n", fname);
        opDisplay(outBuf);
        unlink(fname);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Moves Input Directory File to Output Directory.
**
**                  This step is called once a file is selected from
**                  the input directory.  BEFORE PREPROCESSING.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cr405SwapInOut(Cr405Context *cc, char *fName)
    {
    char fnwork[MaxFSPath * 2 + 32] = "";
    char outBuf[MaxFSPath * 2 + 128];

    //  If either directory isn't specified, just ignore the rename.
    if ((cc->dirOutput == NULL) || (cc->dirInput == NULL))
        {
        return;
        }

    /*
    **  We have both Input and Output directories
    **      then we move it to the "output" directory
    **      and we return the changed name in fname
    **
    **  Don't touch any files that aren't from the input directory
    */
    if (strncmp(fName, cc->dirInput, strlen(cc->dirInput)) != 0)
        {
        return;
        }

    /*
    **  Perform the rename of the current file to the "Processed"
    **  directory.  This rename will ALSO trigger the filechange
    **  watcher.  Otherwise the operator will need to use the load
    **  command from the console.
    */

    int fnindex = 0;

    while (TRUE)
        {
        //  Create the new filename
        sprintf(fnwork, "%s/%s_%04i",
                cc->dirOutput,
                fName + strlen(cc->dirInput) + 1,
                fnindex);

        if (rename(fName, fnwork) == 0)
            {
            sprintf(outBuf, "(cr405  ) Deck '%s' moved to '%s'. (Input preserved)\n",
                    fName + strlen(cc->dirInput) + 1,
                    fnwork);
            opDisplay(outBuf);
            strcpy(fName, fnwork);
            break;
            }
        else
            {
            sprintf(outBuf, "(cr3447 ) Rename failure on '%s' - (%s). Retrying (%d)...\n",
                    fName + strlen(cc->dirInput) + 1,
                    strerror(errno),
                    fnindex);
            opDisplay(outBuf);
            }
        fnindex += 1;
        if (fnindex > 999)
            {
            sprintf(outBuf, "(cr3447 ) Rename failure on '%s' to '%s' (retries > 999)\n", fName, fnwork);
            opDisplay(outBuf);
            break;
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Show card reader status (operator interface).
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cr405ShowStatus()
    {
    Cr405Context *cp;
    char         outBuf[MaxFSPath * 2 + 64];

    for (cp = firstCr405; cp != NULL; cp = cp->nextUnit)
        {
        sprintf(outBuf, "    >   %-8s C%02o E%02o U%02o", "405", cp->channelNo, cp->eqNo, cp->unitNo);
        opDisplay(outBuf);
        sprintf(outBuf, "   %-20s", cp->curFileName != NULL ? cp->curFileName : "");
        opDisplay(outBuf);
        sprintf(outBuf, " (seq %d", cp->seqNum);
        opDisplay(outBuf);
        if (cp->isWatched)
            {
            sprintf(outBuf, ", in %s/", cp->dirInput);
            opDisplay(outBuf);
            if (cp->dirOutput != NULL)
                {
                sprintf(outBuf, ", out %s/", cp->dirOutput);
                opDisplay(outBuf);
                }
            }
        opDisplay(")\n");
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
**  Purpose:        Execute function code on 405 card reader.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        FcStatus
**
**------------------------------------------------------------------------*/
static FcStatus cr405Func(PpWord funcCode)
    {
    switch (funcCode)
        {
    default:

        return (FcDeclined);

    case FcCr405Deselect:
    case FcCr405GateToSec:
        activeDevice->fcode = 0;

        return (FcProcessed);

    case FcCr405ReadNonStop:
    case FcCr405StatusReq:
        activeDevice->fcode = funcCode;
        break;
        }

    return (FcAccepted);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Perform I/O on 405 card reader.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cr405Io(void)
    {
    Cr405Context *cc = activeDevice->context[0];

    switch (activeDevice->fcode)
        {
    default:
    case FcCr405Deselect:
    case FcCr405GateToSec:
        break;

    case FcCr405StatusReq:
        if ((activeDevice->fcb[0] == NULL) && (cc->col >= 80))
            {
            activeChannel->data = StCr405NotReady;
            }
        else
            {
            activeChannel->data = StCr405Ready;
            }
        activeChannel->full = TRUE;
        break;

    case FcCr405ReadNonStop:

        /*
        **  Simulate card in motion for 20 major cycles.
        */
        if (cycles - cc->getCardCycle < 20)
            {
            break;
            }

        if (activeChannel->full)
            {
            break;
            }

        activeChannel->data = cc->card[cc->col++] & Mask12;
        activeChannel->full = TRUE;

        if (cc->col >= 80)
            {
            cr405NextCard(activeDevice);
            }

        break;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Handle channel activation.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cr405Activate(void)
    {
#if DEBUG
    fprintf(cr405Log, "\n(cr405  ) %06d PP:%02o CH:%02o Activate",
            traceSequenceNo,
            activePpu->id,
            activeDevice->channel->id);
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Handle disconnecting of channel.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cr405Disconnect(void)
    {
#if DEBUG
    fprintf(cr405Log, "\n(cr405  ) %06d PP:%02o CH:%02o Disconnect",
            traceSequenceNo,
            activePpu->id,
            activeDevice->channel->id);
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Start reading next card deck.
**
**  Parameters:     Name        Description.
**
**  Returns:        TRUE if the deck is successfully loaded.
**                  FALSE if not.
**
**------------------------------------------------------------------------*/
static bool cr405StartNextDeck(DevSlot *dp, Cr405Context *cc)
    {
    char *fname;
    char outBuf[MaxFSPath + 128];

    while (cc->outDeck != cc->inDeck)
        {
        fname      = cc->decks[cc->outDeck];
        dp->fcb[0] = fopen(fname, "r");
        if (dp->fcb[0] != NULL)
            {
            cc->curFileName = fname;
            cr405NextCard(dp);
            sprintf(outBuf, "Cards '%s' loaded on card reader C%o,E%o\n", cc->curFileName, cc->channelNo, cc->eqNo);
            opDisplay(outBuf);

            return TRUE;
            }
        logDtError(LogErrorLocation, "Failed to open card deck '%s'\n", fname);
        unlink(fname);
        free(fname);
        cc->outDeck = (cc->outDeck + 1) % Cr405MaxDecks;
        }
    cc->curFileName = NULL;
    dp->fcb[0]      = NULL;

    return FALSE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Read next card, update card reader status.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cr405NextCard(DevSlot *dp)
    {
    static char  buffer[322];
    bool         binaryCard;
    char         c;
    Cr405Context *cc = dp->context[0];
    char         *cp;
    int          i;
    int          j;
    char         outBuf[MaxFSPath + 128];
    int          value;

    if (dp->fcb[0] == NULL)
        {
        return;
        }

    /*
    **  Initialise read.
    */
    cc->getCardCycle = cycles;
    cc->col          = 0;
    binaryCard       = FALSE;

    /*
    **  Read the next card.
    */
    cp = fgets(buffer, sizeof(buffer), dp->fcb[0]);
    if (cp == NULL)
        {
        /*
        **  If the last card wasn't a 6/7/8/9 card, fake one.
        */
        if (cc->card[0] != 00017)
            {
            memset(cc->card, 0, sizeof(cc->card));
            cc->card[0] = 00017;
            }
        else
            {
            cc->col = 80;
            }

        fclose(dp->fcb[0]);
        dp->fcb[0] = NULL;

        /*
        **  At end of file, it is assumed that ALL decks have been
        **  passed through the preprocessor and therefore have
        **  new names of the format: CR_C%02o_E%02o_%05d
        **  (See operator.c)
        **
        **  So we do a cursory test BEFORE we delete the file
        */
        if (strncmp("CR_", cc->curFileName, 3) == 0)
            {
            unlink(cc->decks[cc->outDeck]);
            }
        else
            {
            sprintf(outBuf, "(cr405 ) *WARNING* We are not removing file '%s'\n",
                    cc->curFileName);
            opDisplay(outBuf);
            }

        free(cc->curFileName);
        cc->outDeck = (cc->outDeck + 1) % Cr405MaxDecks;

        cr405StartNextDeck(dp, cc);

        return;
        } // if (cp == NULL)

    /*
    **  Deal with special first-column codes.
    */
    if (buffer[0] == '~')
        {
        if (memcmp(buffer + 1, "eoi\n", 4) == 0)
            {
            /*
            **  EOI = 6/7/8/9 card.
            */
            memset(cc->card, 0, sizeof(cc->card));
            cc->card[0] = 00017;

            return;
            }

        if (memcmp(buffer + 1, "eof\n", 4) == 0)
            {
            /*
            **  EOF = 6/7/9 card.
            */
            memset(cc->card, 0, sizeof(cc->card));
            cc->card[0] = 00015;

            return;
            }

        if (memcmp(buffer + 1, "eor\n", 4) == 0)
            {
            /*
            **  EOR = 7/8/9 card.
            */
            memset(cc->card, 0, sizeof(cc->card));
            cc->card[0] = 00007;

            return;
            }

        if (memcmp(buffer + 1, "bin", 3) == 0)
            {
            /*
            **  Binary = 7/9 card.
            */
            binaryCard  = TRUE;
            cc->card[0] = 00005;
            }
        }

    /*
    **  Convert cards.
    */
    if (!binaryCard)
        {
        /*
        **  Skip over any characters past column 80 (if line is longer).
        */
        if ((cp = strchr(buffer, '\n')) == NULL)
            {
            do
                {
                c = fgetc(dp->fcb[0]);
                } while (c != '\n' && c != EOF);
            cp = buffer + 80;
            }

        /*
        **  Blank fill line shorter then 80 characters.
        */
        for ( ; cp < buffer + 80; cp++)
            {
            *cp = ' ';
            }

        /*
        **  Convert ASCII card.
        */
        for (i = 0; i < 80; i++)
            {
            cc->card[i] = cc->table[buffer[i]];
            }
        }
    else
        {
        /*
        **  Skip over any characters past column 320 (if line is longer).
        */
        if ((cp = strchr(buffer, '\n')) == NULL)
            {
            do
                {
                c = fgetc(dp->fcb[0]);
                } while (c != '\n' && c != EOF);
            cp = buffer + 320;
            }

        /*
        **  Zero fill line shorter then 320 characters.
        */
        for ( ; cp < buffer + 320; cp++)
            {
            *cp = '0';
            }

        /*
        **  Convert binary card (79 x 4 octal digits).
        */
        cp = buffer + 4;
        for (i = 1; i < 80; i++)
            {
            value = 0;
            for (j = 0; j < 4; j++)
                {
                if ((cp[j] >= '0') && (cp[j] <= '7'))
                    {
                    value = (value << 3) | (cp[j] - '0');
                    }
                else
                    {
                    value = 0;
                    break;
                    }
                }

            cc->card[i] = value;

            cp += 4;
            }
        }
    }

/*---------------------------  End Of File  ------------------------------*/
