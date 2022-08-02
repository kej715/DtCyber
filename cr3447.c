/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Paul Koning, Tom Hunter
**            (c) 2017-2022  Steven Zoppi 23-Jun-2022
**                           Rewrite Job Submission and support for
**                           dedicated input/output directories.
**
**  Name: cr3447.c
**
**  Description:
**      Perform emulation of CDC 3447 card reader controller.
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

#define DEBUG    0

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
#include "dcc6681.h"

/*
**  -----------------
**  Private Constants
**  -----------------
*/


/*
**  CDC 3447 card reader function and status codes.
*/
#define FcCr3447Deselect        00000
#define FcCr3447Binary          00001
#define FcCr3447BCD             00002
#define FcCr3447SetGateCard     00004
#define FcCr3447Clear           00005
#define FcCr3447IntReady        00020
#define FcCr3447NoIntReady      00021
#define FcCr3447IntEoi          00022
#define FcCr3447NoIntEoi        00023
#define FcCr3447IntError        00024
#define FcCr3447NoIntError      00025

/*
**      Status reply flags
**
**      0001 = Ready
**      0002 = Busy
**      0004 = Binary card (7/9 punch)
**      0010 = File card (7/8 punch)
**      0020 = Jam
**      0040 = Input tray empty
**      0100 = End of file
**      0200 = Ready interrupt
**      0400 = EOI interrupt
**      1000 = Error interrupt
**      2000 = Read compare error
**      4000 = Reserved by other controller (3649 only)
**
*/
#define StCr3447Ready           00201   // includes ReadyInt
#define StCr3447Busy            00002
#define StCr3447Binary          00004
#define StCr3447File            00010
#define StCr3447Empty           00040
#define StCr3447Eof             01540   // includes Empty, EoiInt, ErrorInt
#define StCr3447ReadyInt        00200
#define StCr3447EoiInt          00400
#define StCr3447ErrorInt        01000
#define StCr3447CompareErr      02000
#define StCr3447NonIntStatus    02177

#define Cr3447MaxDecks          10

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

typedef struct crContext
    {
    /*
    **  Info for show_unitrecord operator command.
    */
    struct crContext *nextUnit;
    u8               channelNo;
    u8               eqNo;
    u8               unitNo;

    bool             binary;
    bool             rawCard;
    int              intMask;
    int              status;
    int              col;
    const u16        *table;
    u32              getCardCycle;
    PpWord           card[80];
    int              inDeck;
    int              outDeck;
    char             *decks[Cr3447MaxDecks];
    char             curFileName[MaxFSPath];
    char             dirInput[MaxFSPath];
    char             dirOutput[MaxFSPath];
    int              seqNum;
    bool             isWatched;
    } CrContext;


/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static FcStatus cr3447Func(PpWord funcCode);
static void cr3447Io(void);
static void cr3447Activate(void);
static void cr3447Disconnect(void);
static void cr3447NextCard(DevSlot *up, CrContext *cc);
static char *cr3447Func2String(PpWord funcCode);
static bool cr3447StartNextDeck(DevSlot *up, CrContext *cc);
static void cr3447SwapInOut(CrContext *cc, char *fname);

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

static CrContext *firstCr3447 = NULL;
static CrContext *lastCr3447  = NULL;

#if DEBUG
static FILE *cr3447Log = NULL;
#endif

/*--------------------------------------------------------------------------
**
**  Public Functions
**
**------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------
**  Purpose:        Initialise card reader.
**
**  Parameters:     Name        Description.
**                  eqNo        equipment number
**                  unitNo      unit number
**                  channelNo   channel number the device is attached to
**                  deviceName  optional card source file name, "026" (default)
**                              or "029" to select translation mode
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cr3447Init(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName)
    {
    DevSlot   *up;
    CrContext *cc;

    fswContext *threadParms;

    //  Extensions for Filesystem Watcher
    char        *xlateTable;
    char        *crInput;
    char        *crOutput;
    char        *tokenAuto;
    bool        watchRequested;
    struct stat s;

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
    if (cr3447Log == NULL)
        {
        cr3447Log = fopen("cr3447log.txt", "wt");
        }
#endif
    up             = dcc6681Attach(channelNo, eqNo, 0, DtCr3447);
    up->activate   = cr3447Activate;
    up->disconnect = cr3447Disconnect;
    up->func       = cr3447Func;
    up->io         = cr3447Io;

    /*
    **  Only one card reader unit is possible per equipment.
    */
    if (up->context[0] != NULL)
        {
        fprintf(stderr, "(cr3447 ) Only one CR3447 unit is possible per equipment\n");
        exit(1);
        }

    cc = calloc(1, sizeof(CrContext));
    if (cc == NULL)
        {
        fprintf(stderr, "(cr3447 ) Failed to allocate CR3447 context block\n");
        exit(1);
        }

    up->context[0] = (void *)cc;

    threadParms = calloc(1, sizeof(fswContext));    //  Need to check for null result
    if (cc == NULL)
        {
        fprintf(stderr, "(cr3447 ) Failed to allocate CR3447 FileWatcher Context block\n");
        exit(1);
        }

    if (deviceName == NULL)
        {
        deviceName = "";
        }
    xlateTable = strtok(deviceName, ", ");
    crInput    = strtok(NULL, ", ");
    crOutput   = strtok(NULL, ", ");
    tokenAuto  = strtok(NULL, ", ");

    /*
    **  Process the Request for FileSystem Watcher
    */
    watchRequested = TRUE;     // Default = Run Filewatcher Thread
    if (tokenAuto != NULL)
        {
        tokenAuto = dtStrLwr(tokenAuto);
        if (!strcmp(tokenAuto, "noauto"))
            {
            watchRequested = FALSE;
            }
        else if ((strcmp(tokenAuto, "auto") != 0) && (strcmp(tokenAuto, "*") != 0))
            {
            fprintf(stderr, "(cr3447 ) Unrecognized Automation Type '%s'\n", tokenAuto);
            exit(1);
            }
        }

    fprintf(stdout, "(cr3447 ) File watcher %s Requested'\n", watchRequested ? "Was" : "Was Not");

    /*
    **  Setup character set translation table.
    */

    cc->table     = asciiTo026; // default translation table
    cc->isWatched = FALSE;

    cc->channelNo = channelNo;
    cc->eqNo      = eqNo;
    cc->unitNo    = unitNo;

    strcpy(cc->dirInput, "");
    strcpy(cc->dirOutput, "");

    if (xlateTable != NULL)
        {
        if (strcmp(xlateTable, "029") == 0)
            {
            cc->table = asciiTo029;
            }
        else if ((strcmp(xlateTable, "026") != 0)
                 && (strcmp(xlateTable, "*") != 0)
                 && (strcmp(xlateTable, "") != 0))
            {
            fprintf(stderr, "(cr3447 ) Unrecognized card code name %s\n", xlateTable);
            exit(1);
            }
        }

    fprintf(stdout, "(cr3447 ) Card Code selected '%s'\n", (cc->table == asciiTo029) ? "029" : "026");

    /*
    **  Incorrect specifications for input / output directories
    **  are fatal.  Even though files can still be submitted
    **  through the operator interface, we want the parameters
    **  supplied through the ini file to be correct from the start.
    */

    if ((crOutput != NULL) && (crOutput[0] != '*'))
        {
        if (stat(crOutput, &s) != 0)
            {
            fprintf(stderr, "(cr3447 ) The Output location specified '%s' does not exist.\n", crOutput);
            exit(1);
            }

        if ((s.st_mode & S_IFDIR) == 0)
            {
            fprintf(stderr, "(cr3447 ) The Output location specified '%s' is not a directory.\n", crOutput);
            exit(1);
            }
        strcpy(threadParms->outDoneDir, crOutput);
        strcpy(cc->dirOutput, crOutput);
        fprintf(stdout, "(cr3447 ) Submissions will be preserved in '%s'.\n", crOutput);
        }
    else
        {
        threadParms->outDoneDir[0] = '\0';
        cc->dirOutput[0]           = '\0';
        fputs("(cr3447 ) Submissions will be purged after processing.\n", stdout);
        }

    if ((crInput != NULL) && (crInput[0] != '*'))
        {
        if (stat(crInput, &s) != 0)
            {
            fprintf(stderr, "(cr3447 ) The Input location specified '%s' does not exist.\n", crInput);
            exit(1);
            }

        if ((s.st_mode & S_IFDIR) == 0)
            {
            fprintf(stderr, "(cr3447 ) The Input location specified '%s' is not a directory.\n", crInput);
            exit(1);
            }
        //  We only care about the "Auto" "NoAuto" flag if there is a good input location

        /*
        **  The thread needs to know what directory to watch.
        **
        **  The Card Reader Context needs to remember what directory
        **  will supply the input files so more can be found at EOD.
        */
        strcpy(threadParms->inWatchDir, crInput);
        strcpy(cc->dirInput, crInput);

        threadParms->eqNo      = eqNo;
        threadParms->unitNo    = unitNo;
        threadParms->channelNo = channelNo;
        // threadParms->LoadCards = cr3447LoadCards;
        threadParms->devType = DtCr3447;

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
                printf("(cr3447 ) Unable to create filesystem watch thread for '%s'.\n", crInput);
                printf("          Card Loading is still possible via Operator Console.\n");
                }
            else
                {
                printf("(cr3447 ) Filesystem watch thread for '%s' created successfully.\n", crInput);
                }
            }
        else
            {
            printf("(cr3447 ) Filesystem watch thread not requested for '%s'.\n", crInput);
            printf("          Card Loading is required via Operator Console.\n");
            }
        }


    /*
    **  Print a friendly message.
    */

    printf("(cr3447 ) Initialised on channel %o equipment %o type '%s'\n",
           channelNo,
           eqNo,
           cc->table == asciiTo026 ? "026" : "029");

    if (!cc->isWatched)
        {
        free(threadParms);
        }

    /*
    **  Link into list of 3447 Card Reader units.
    */
    if (lastCr3447 == NULL)
        {
        firstCr3447 = cc;
        }
    else
        {
        lastCr3447->nextUnit = cc;
        }

    lastCr3447 = cc;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Load cards on 3447 card reader.
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

//TODO: Fixup Code Logic for Max Decks Queued

void cr3447LoadCards(char *fname, int channelNo, int equipmentNo, char *params)
    {
    CrContext   *cc;
    DevSlot     *dp;
    int         len;
    char        outBuf[MaxFSPath+128];
    struct stat s;
    char        *sp;

    /*
    **  Locate the device control block.
    */
    dp = dcc6681FindDevice((u8)channelNo, (u8)equipmentNo, DtCr3447);
    if (dp == NULL)
        {
        return;
        }

    cc = (CrContext *)(dp->context[0]);

    /*
    **  Ensure the tray is not full.
    */
    if (((cc->inDeck + 1) % Cr3447MaxDecks) == cc->outDeck)
        {
        opDisplay("(cr3447 ) Input tray full\n");

        return;
        }

    //  At this point we should have a valid(ish) input file
    //  Make sure before enqueueing it.

    if (stat(fname, &s) != 0)
        {
        sprintf(outBuf, "(cr3447 ) Requested file '%s' not found. (%s).\n", fname, strerror(errno));
        opDisplay(outBuf);
        return;
        }

    //  Enqueue the file in the chain of pending files

    len = strlen(fname) + 1;
    sp  = (char *)malloc(len);
    memcpy(sp, fname, len);
    cc->decks[cc->inDeck] = sp;
    cc->inDeck            = (cc->inDeck + 1) % Cr3447MaxDecks;

    if (dp->fcb[0] == NULL)
        {
        if (cr3447StartNextDeck(dp, cc))
            {
            return;
            }
        }

    //  Starting the next deck was not possible

    dp->fcb[0] = NULL;
    cc->status = StCr3447Eof;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Get the next available deck named in the
**                  3447 card reader's input directory.
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

//TODO: Fixup Code Logic for Max Decks Queued

void cr3447GetNextDeck(char *fname, int channelNo, int equipmentNo, char *params)
    {
    CrContext     *cc;
    DIR           *curDir;
    struct dirent *curDirEntry;
    DevSlot       *dp;
    char          fOldest[MaxFSPath*2+2] = "";
    char          outBuf[MaxFSPath*2+64];
    struct stat   s;
    char          strWork[MaxFSPath*2+2] = "";
    time_t        tOldest            = 0;


    //  Safety check, we only respond if the first
    //  character of the filename is an asterisk '*'

    if (fname[0] != '*')
        {
        sprintf(outBuf, "(cr3447 ) GetNextDeck called with improper parameter '%s'.\n", fname);
        opDisplay(outBuf);
        return;
        }

    /*
    **  Locate the device control block.
    */
    dp = dcc6681FindDevice((u8)channelNo, (u8)equipmentNo, DtCr3447);
    if (dp == NULL)
        {
        return;
        }

    cc = (CrContext *)(dp->context[0]);

    /*
    **  Ensure the tray is not full.
    */
    if (((cc->inDeck + 1) % Cr3447MaxDecks) == cc->outDeck)
        {
        opDisplay("(cr3447 ) Input tray full\n");

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

    //  If the input directory isn't specified, we cannot trust the
    //  contents of the input file so the feature isn't used.

    if (cc->dirInput[0] == '\0')
        {
        opDisplay("(cr3447 ) No card reader directory has been specified on the device declaration.\n");
        opDisplay("(cr3447 ) The 'Load Next Deck' request is ignored.\n");

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
            continue;
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
        sprintf(outBuf, "(cr3447 ) Dequeueing Unprocessed File '%s' from '%s'.\n", fOldest, cc->dirInput);
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
        cr3447SwapInOut(cc, fname);
        }
    else
        {
        sprintf(outBuf, "(cr3447 ) No files found in '%s'.\n", cc->dirInput);
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
void cr3447PostProcess(char *fname, int channelNo, int equipmentNo, char *params)
    {
    CrContext *cc;
    DevSlot   *dp;
    char      outBuf[MaxFSPath+64];

    /*
    **  Locate the device control block.
    */

    dp = channelFindDevice((u8)channelNo, DtCr405);
    if (dp == NULL)
        {
        return;
        }

    cc = (CrContext *)(dp->context[0]);

    bool hasNoInputDir = (cc->dirInput[0] == '\0');

    if (hasNoInputDir)
        {
        sprintf(outBuf, "(cr3447 ) Submitted Deck '%s' Processing Complete.\n", fname);
        opDisplay(outBuf);
        return;
        }

    bool isFromInput = (
        strncmp(
            fname, cc->dirInput,
            strlen(cc->dirInput)
            ) == 0);

    //  There should be no expectation that the file needs to be preserved

    if (isFromInput)
        {
        sprintf(outBuf, "(cr3447 ) Purging Submitted Deck '%s'.\n", fname);
        opDisplay(outBuf);
        unlink(fname);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Moves Input Directory File to Output Directory.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cr3447SwapInOut(CrContext *cc, char *fName)
    {
    char fnwork[MaxFSPath*2+32] = "";
    char outBuf[MaxFSPath+2+64];
    

    bool hasNoOutputDir = (cc->dirOutput[0] == '\0');
    bool hasNoInputDir  = (cc->dirInput[0] == '\0');

    //  If either directory isn't specified, just ignore the rename.
    if (hasNoInputDir || hasNoOutputDir)
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

    bool isFromInput = (
        strncmp(
            fName, cc->dirInput,
            strlen(cc->dirInput)
            ) == 0);

    if (!isFromInput)
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
            sprintf(outBuf, "(cr3447 ) Deck '%s' moved to '%s'. (Input Preserved)\n",
                    fName + strlen(cc->dirInput) + 1,
                    fnwork);
            opDisplay(outBuf);
            strcpy(fName, fnwork);
            break;
            }
        else
            {
            sprintf(outBuf, "(cr3447 ) Rename Failure on '%s' - (%s). Retrying (%d)...\n",
                    fName + strlen(cc->dirInput) + 1,
                    strerror(errno),
                    fnindex);
            opDisplay(outBuf);
            }
        fnindex++;
        if (fnindex > 999)
            {
            sprintf(outBuf, "(cr3447 ) Rename Failure on '%s' to '%s'(Retries > 999)\n", fName, fnwork);
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
void cr3447ShowStatus()
    {
    CrContext *cp = firstCr3447;
    char      outBuf[MaxFSPath*2+64];

    if (cp == NULL)
        {
        return;
        }

    opDisplay("\n    > Card Reader (cr3447) Status:\n");

    while (cp)
        {
        sprintf(outBuf, "    >   CH %02o EQ %02o UN %02o Col %02i Mode(%s) Raw(%s) Seq:%i File '%s'\n",
                cp->channelNo,
                cp->eqNo,
                cp->unitNo,
                cp->col,
                cp->binary ? "Char" : "Bin ",
                cp->rawCard ? "Yes" : "No ",
                cp->seqNum,
                cp->curFileName);
        opDisplay(outBuf);

        if (cp->isWatched)
            {
            sprintf(outBuf, "    >   Autoloading from '%s' to '%s'\n",
                    cp->dirInput,
                    cp->dirOutput);
            opDisplay(outBuf);
            }

        cp = cp->nextUnit;
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
**  Purpose:        Execute function code on 3447 card reader.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        FcStatus
**
**------------------------------------------------------------------------*/
static FcStatus cr3447Func(PpWord funcCode)
    {
    CrContext *cc;
    FcStatus  st;

#if DEBUG
    fprintf(cr3447Log, "\n(cr3447 ) %06d PP:%02o CH:%02o f:%04o T:%-25s  >   ",
            traceSequenceNo,
            activePpu->id,
            activeDevice->channel->id,
            funcCode,
            cr3447Func2String(funcCode));
#endif

    cc = (CrContext *)active3000Device->context[0];

    switch (funcCode)
        {
    default:                    // all unrecognized codes are NOPs
#if DEBUG
        fprintf(cr3447Log, "(cr3447 ) FUNC not implemented & silently ignored!");
#endif
        st = FcProcessed;
        break;

    case FcCr3447SetGateCard:
        st = FcProcessed;
        break;

    case Fc6681InputToEor:
    case Fc6681Input:
        st = FcAccepted;
        cc->getCardCycle        = cycles;
        cc->status              = StCr3447Ready;
        active3000Device->fcode = funcCode;
        break;

    case Fc6681DevStatusReq:
        st = FcAccepted;
        active3000Device->fcode = funcCode;
        break;

    case FcCr3447Binary:
        cc->binary = TRUE;
        st         = FcProcessed;
        break;

    case FcCr3447Deselect:
    case FcCr3447Clear:
        cc->intMask = 0;
        cc->binary  = FALSE;
        st          = FcProcessed;
        break;

    case FcCr3447BCD:
        cc->binary = FALSE;
        st         = FcProcessed;
        break;

    case FcCr3447IntReady:
        cc->intMask |= StCr3447ReadyInt;
        cc->status  &= ~StCr3447ReadyInt;
        st           = FcProcessed;
        break;

    case FcCr3447NoIntReady:
        cc->intMask &= ~StCr3447ReadyInt;
        cc->status  &= ~StCr3447ReadyInt;
        st           = FcProcessed;
        break;

    case FcCr3447IntEoi:
        cc->intMask |= StCr3447EoiInt;
        cc->status  &= ~StCr3447EoiInt;
        st           = FcProcessed;
        break;

    case FcCr3447NoIntEoi:
        cc->intMask &= ~StCr3447EoiInt;
        cc->status  &= ~StCr3447EoiInt;
        st           = FcProcessed;
        break;

    case FcCr3447IntError:
        cc->intMask |= StCr3447ErrorInt;
        cc->status  &= ~StCr3447ErrorInt;
        st           = FcProcessed;
        break;

    case FcCr3447NoIntError:
        cc->intMask &= ~StCr3447ErrorInt;
        cc->status  &= ~StCr3447ErrorInt;
        st           = FcProcessed;
        break;
        }

    dcc6681Interrupt((cc->status & cc->intMask) != 0);

    return (st);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Perform I/O on 3447 card reader.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cr3447Io(void)
    {
    CrContext *cc;
    PpWord    c;

    cc = (CrContext *)active3000Device->context[0];

    switch (active3000Device->fcode)
        {
    default:
        printf("(cr3447 ) Unexpected IO for function %04o\n", active3000Device->fcode);
        break;

    case 0:
        break;

    case Fc6681DevStatusReq:
        if (!activeChannel->full)
            {
            activeChannel->data = (cc->status & (cc->intMask | StCr3447NonIntStatus));
            activeChannel->full = TRUE;
#if DEBUG
            fprintf(cr3447Log, " %04o", activeChannel->data);
#endif
            }
        break;

    case Fc6681InputToEor:
    case Fc6681Input:

        // Don't admit to having new data immediately after completing
        // a card, otherwise 1CD may get stuck occasionally.
        // So we simulate card in motion for 20 major cycles.
        if (activeChannel->full
            || (cycles - cc->getCardCycle < 20))
            {
            break;
            }

        if (active3000Device->fcb[0] == NULL)
            {
            cc->status = StCr3447Eof;
            break;
            }

        if (cc->col >= 80)
            {
            // Read the next card.
            // If the function is input to EOR, disconnect to indicate EOR
            cr3447NextCard(active3000Device, cc);
            if (activeDevice->fcode == Fc6681InputToEor)
                {
                // End of card but we're still ready
                cc->status |= StCr3447EoiInt | StCr3447Ready;
                if (cc->status & StCr3447File)
                    {
                    cc->status |= StCr3447ErrorInt;
                    }

                activeChannel->discAfterInput = TRUE;
                }
            }
        else
            {
            c = cc->card[cc->col++];
            if (cc->rawCard)
                {
                activeChannel->data = c;
                }
            else if (cc->binary)
                {
                activeChannel->data = cc->table[c];
                }
            else
                {
                activeChannel->data = asciiToBcd[c] << 6;
                c = cc->card[cc->col++];
                activeChannel->data += asciiToBcd[c];
                }

            activeChannel->full = TRUE;
#if DEBUG
            fprintf(cr3447Log, " %04o", activeChannel->data);
#endif
            }
        break;
        }

    dcc6681Interrupt((cc->status & cc->intMask) != 0);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Handle channel activation.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cr3447Activate(void)
    {
#if DEBUG
    fprintf(cr3447Log, "\n(cr3447 ) %06d PP:%02o CH:%02o Activate",
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
static void cr3447Disconnect(void)
    {
    CrContext *cc;

#if DEBUG
    fprintf(cr3447Log, "\n(cr3447 ) %06d PP:%02o CH:%02o Disconnect",
            traceSequenceNo,
            activePpu->id,
            activeDevice->channel->id);
#endif

    /*
    **  Abort pending device disconnects - the PP is doing the disconnect.
    */
    activeChannel->discAfterInput = FALSE;

    /*
    **  Advance to next card.
    */
    cc = (CrContext *)active3000Device->context[0];
    if (cc != NULL)
        {
        cc->status |= StCr3447EoiInt;
        dcc6681Interrupt((cc->status & cc->intMask) != 0);
        if ((active3000Device->fcb[0] != NULL) && (cc->col != 0))
            {
            cr3447NextCard(active3000Device, cc);
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Start reading next card deck.
**
**  Parameters:     Name        Description.
**
**  Returns:        TRUE if a deck was successfully loaded.
**                  FALSE if not.
**
**------------------------------------------------------------------------*/
static bool cr3447StartNextDeck(DevSlot *up, CrContext *cc)
    {
    char *fname;
    char outBuf[MaxFSPath+128];

    while (cc->outDeck != cc->inDeck)
        {
        fname      = cc->decks[cc->outDeck];
        up->fcb[0] = fopen(fname, "r");
        if (up->fcb[0] != NULL)
            {
            cc->status = StCr3447Eof;
            cc->status = StCr3447Ready;
            strcpy(cc->curFileName, fname);
            cr3447NextCard(up, cc);
            activeDevice = channelFindDevice(up->channel->id, DtDcc6681);
            dcc6681Interrupt((cc->status & cc->intMask) != 0);
            sprintf(outBuf, "\n(cr3447 ) Cards '%s' loaded on card reader C%02o,E%02o\n", cc->curFileName, cc->channelNo, cc->eqNo);
            opDisplay(outBuf);
            return TRUE;
            }
        sprintf(outBuf, "(cr3447 ) Failed to open card deck '%s'\n", fname);
        opDisplay(outBuf);
        unlink(fname);
        free(fname);
        cc->outDeck = (cc->outDeck + 1) % Cr3447MaxDecks;
        }
    up->fcb[0] = NULL;

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
static void cr3447NextCard(DevSlot *up, CrContext *cc)
    {
    static char buffer[326];
    char        c;
    PpWord      col1;
    char        *cp;
    int         i;
    int         j;
    char        outBuf[MaxFSPath+128];
    int         value;

    /*
    **  Initialise read.
    */
    cc->getCardCycle = cycles;
    cc->col          = 0;
    cc->rawCard      = FALSE;

    /*
    **  Read the next card.
    */
    cp = fgets(buffer, sizeof(buffer), up->fcb[0]);
    if (cp == NULL)
        {
        /*
        **  If the last card wasn't a 6/7/8/9 card, fake one.
        */
        if (cc->card[0] != 00017)
            {
            cc->rawCard = TRUE;//    ???? what if we don't read binary (i.e. cc->binary)
            cc->status |= StCr3447Binary;
            memset(cc->card, 0, sizeof(cc->card));
            cc->card[0] = 00017;

            return;
            }

        fclose(up->fcb[0]);
        up->fcb[0] = NULL;
        cc->status = StCr3447Eof;

        sprintf(outBuf, "(cr3447 ) End of Deck '%s' reached on channel %o equipment %o\n",
                cc->curFileName,
                up->channel->id,
                up->eqNo);
        opDisplay(outBuf);

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
            sprintf(outBuf, "(cr3447 ) *WARNING* file '%s' will not be removed\n",
                    cc->curFileName);
            opDisplay(outBuf);
            }

        free(cc->decks[cc->outDeck]);
        cc->outDeck = (cc->outDeck + 1) % Cr3447MaxDecks;

        if (cr3447StartNextDeck(up, cc))
            {
            return;
            }

        cc->curFileName[0] = '\0';

        return;
        } // if (cp == NULL)

    /*
    **  Deal with special first-column codes.
    */
    if (buffer[0] == '}')
        {
        /*
        **  EOI = 6/7/8/9 card.
        */
        cc->rawCard = TRUE;
        cc->status |= StCr3447Binary;
        memset(cc->card, 0, sizeof(cc->card));
        cc->card[0] = 00017;

        return;
        }

    if (buffer[0] == '~')
        {
        if (strcmp(buffer + 1, "eoi\n") == 0)
            {
            /*
            **  EOI = 6/7/8/9 card.
            */
            cc->rawCard = TRUE;
            cc->status |= StCr3447Binary;
            memset(cc->card, 0, sizeof(cc->card));
            cc->card[0] = 00017;

            return;
            }

        if (strcmp(buffer + 1, "eof\n") == 0)
            {
            /*
            **  EOF = 6/7/9 card.
            */
            cc->rawCard = TRUE;
            cc->status |= StCr3447Binary;
            memset(cc->card, 0, sizeof(cc->card));
            cc->card[0] = 00015;

            return;
            }

        if ((strcmp(buffer + 1, "eor\n") == 0) || (buffer[1] == '\n') || (buffer[1] == ' '))
            {
            /*
            **  EOR = 7/8/9 card.
            */
            cc->rawCard = TRUE;
            cc->status |= StCr3447Binary;
            memset(cc->card, 0, sizeof(cc->card));
            cc->card[0] = 00007;

            return;
            }

        if (memcmp(buffer + 1, "raw", 3) == 0)
            {
            /*
            **  Raw binary card.
            */
            cc->rawCard = TRUE;
            col1        = buffer[4] & Mask5;
            if (col1 == 00005)
                {
                cc->status |= StCr3447Binary;
                }
            else if ((col1 == 00006) && !cc->binary)
                {
                cc->status |= StCr3447File;
                }
            }
        }

    if (!cc->rawCard)
        {
        /*
        **  Skip over any characters past column 80 (if line is longer).
        */
        if ((cp = strchr(buffer, '\n')) == NULL)
            {
            do
                {
                c = fgetc(up->fcb[0]);
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
        **  Transfer buffer.
        */
        for (i = 0; i < 80; i++)
            {
            c = buffer[i];

            /*
            **  Convert any non-ASCII characters to blank.
            */
            cc->card[i] = (c & 0x80) ? ' ' : c;
            }
        }
    else
        {
        /*
        **  Skip over any characters past column 324 (if line is longer).
        */
        if ((cp = strchr(buffer, '\n')) == NULL)
            {
            do
                {
                c = fgetc(up->fcb[0]);
                } while (c != '\n' && c != EOF);
            cp = buffer + 324;
            }

        /*
        **  Zero fill line shorter then 320 characters.
        */
        for ( ; cp < buffer + 324; cp++)
            {
            *cp = '0';
            }

        /*
        **  Convert raw binary card (80 x 4 octal digits).
        */
        cp = buffer + 4;
        for (i = 0; i < 80; i++)
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

/*--------------------------------------------------------------------------
**  Purpose:        Convert function code to string.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        String equivalent of function code.
**
**------------------------------------------------------------------------*/
static char *cr3447Func2String(PpWord funcCode)
    {
    static char buf[40];

#if DEBUG
    switch (funcCode)
        {
    case FcCr3447Deselect:

        return "Deselect";

    case FcCr3447Binary:

        return "Binary";

    case FcCr3447BCD:

        return "BCD";

    case FcCr3447SetGateCard:

        return "SetGateCard";

    case FcCr3447Clear:

        return "Clear";

    case FcCr3447IntReady:

        return "IntReady";

    case FcCr3447NoIntReady:

        return "NoIntReady";

    case FcCr3447IntEoi:

        return "IntEoi";

    case FcCr3447NoIntEoi:

        return "NoIntEoi";

    case FcCr3447IntError:

        return "IntError";

    case FcCr3447NoIntError:

        return "NoIntError";

    case Fc6681DevStatusReq:

        return "6681DevStatusReq";

    case Fc6681InputToEor:

        return "6681InputToEor";

    case Fc6681Input:

        return "6681Input";
        }
#endif
    sprintf(buf, "(cr3447 ) Unknown Function: %04o", funcCode);

    return (buf);
    }

/*---------------------------  End Of File  ------------------------------*/
