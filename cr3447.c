/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Paul Koning, Tom Hunter
**            (c) 2017       Steven Zoppi 10-Nov-2017
**                           Added status messaging support
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
#include "dirent.h"
#else
#include <dirent.h>
#include <unistd.h>
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
    **  Info for show_tape operator command.
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
    char             curFileName[_MAX_PATH];
    char             dirInput[_MAX_PATH];
    char             dirOutput[_MAX_PATH];
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
static void cr3447NextCard(DevSlot *up, CrContext *cc, FILE *out);
static char *cr3447Func2String(PpWord funcCode);
static bool cr3447StartNextDeck(DevSlot *up, CrContext *cc, FILE *out);

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
**                  unitCount   number of units to initialise
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
    char *xlateTable;
    char *crInput;
    char *crOutput;
    char *tokenAuto;
    bool bWatchRequested;
    char tokenString[80] = " ";     //  Silly workaround because of strtok
                                    //  treating multiple consecutive delims
                                    //  as one.
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

    threadParms->LoadCards = 0;
    if (deviceName != NULL)
        {
        strcat(tokenString, deviceName);
        }
    xlateTable = strtok(tokenString, ",");
    crInput    = strtok(NULL, ",");
    crOutput   = strtok(NULL, ",");
    tokenAuto  = strtok(NULL, ",");

    /*
    **  Process the Request for FileSystem Watcher
    */
    bWatchRequested = TRUE;     // Default = Run Filewatcher Thread
    if (tokenAuto != NULL)
        {
        strlwr(tokenAuto);
        if (!strcmp(tokenAuto, "noauto"))
            {
            bWatchRequested = FALSE;
            }
        else if ((strcmp(tokenAuto, "auto") != 0) && (strcmp(tokenAuto, "*") != 0))
            {
            fprintf(stderr, "(cr3447 ) Unrecognized Automation Type '%s'\n", tokenAuto);
            exit(1);
            }
        }

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
        if (strcmp(xlateTable, " 029") == 0)
            {
            cc->table = asciiTo029;
            }
        else if ((strcmp(xlateTable, " 026") != 0)
                 && (strcmp(xlateTable, " *") != 0)
                 && (strcmp(xlateTable, " ") != 0))
            {
            fprintf(stderr, "(cr3447 ) Unrecognized card code name %s\n", xlateTable);
            exit(1);
            }
        }

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
        strcpy_s(threadParms->outDoneDir, sizeof(threadParms->outDoneDir), crOutput);
        strcpy_s(cc->dirOutput, sizeof(cc->dirOutput), crOutput);
        fprintf(stderr, "(cr3447 ) Submissions will be preserved in '%s'.\n", crOutput);
        }
    else
        {
        threadParms->outDoneDir[0] = '\0';
        cc->dirOutput[0]           = '\0';
        fprintf(stderr, "(cr3447 ) Submissions will be purged after processing.\n");
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
        strcpy_s(threadParms->inWatchDir, sizeof(threadParms->inWatchDir), crInput);
        strcpy_s(cc->dirInput, sizeof(cc->dirInput), crInput);

        threadParms->eqNo      = eqNo;
        threadParms->unitNo    = unitNo;
        threadParms->channelNo = channelNo;
        threadParms->LoadCards = cr3447LoadCards;
        threadParms->devType   = DtCr3447;

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
        if (bWatchRequested)
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
**                  out         DtCyber operator output file
**                  params      Parameter list supplied on command line
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/

//TODO: Fixup Code Logic for Max Decks Queued
//TODO: Fixup Initiator for FileWatcher Thread

void cr3447LoadCards(char *fname, int channelNo, int equipmentNo, FILE *out, char *params)
    {
    CrContext *cc;
    DevSlot   *dp;
    int       len;
    char      *sp;

    int  numParam;
    FILE *fcb;

    static char str[_MAX_PATH]     = "";
    static char strWork[_MAX_PATH] = "";
    static char fOldest[_MAX_PATH] = "";
    time_t      tOldest;

    struct stat   s;
    struct dirent *curDirEntry;

    DIR *curDir;

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
        printf("(cr3447 ) Input tray full\n");

        return;
        }

    len = strlen(fname) + 1;
    sp  = (char *)malloc(len);
    memcpy(sp, fname, len);
    cc->decks[cc->inDeck] = sp;
    cc->inDeck            = (cc->inDeck + 1) % Cr3447MaxDecks;

    if (dp->fcb[0] == NULL)
        {
        if (cr3447StartNextDeck(dp, cc, out))
            {
            return;
            }
        }


    dp->fcb[0] = NULL;
    cc->status = StCr3447Eof;

    /*
    **  If the string for the filename = "*" Then we
    **  invoke the logic that selects the next file from
    **  the input directory (if one exists) in create
    **  date order.
    **
    **  The asterisk convention works even if the
    **  filewatcher thread cannot be started.  It
    **  simply means: "Pick the next oldest file
    **  found in the input directory.
    **
    **  For anything other than an asterisk, the
    **  string is assumed to be a filename.
    */
    if ((strcmp(str, "*") == 0) && (cc->dirInput[0] != '\0'))
        {
        curDir = opendir(cc->dirInput);

        /*
        **  Scan the input directory (if specified)
        **  for the oldest file queued.
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

            sprintf_s(strWork, sizeof(strWork), "%s/%s", cc->dirInput, curDirEntry->d_name);
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
            printf("(cr3447 ) Dequeueing Unprocessed File '%s'.\n", fOldest);
            strcpy(str, fOldest);
            }
        }

    if (stat(str, &s) != 0)
        {
        fprintf(stderr, "(cr3447 ) The Input location specified '%s' does not exist.\n", str);

        return;
        }


    fcb = fopen(str, "r");

    /*
    **  Check if the open succeeded.
    */
    if (fcb == NULL)
        {
        printf("(cr3447 ) Failed to open %s\n", str);

        return;
        }

    dp->fcb[0] = fcb;
    cc->status = StCr3447Ready;
    cr3447NextCard(dp, cc, out);

    activeDevice = channelFindDevice((u8)channelNo, DtDcc6681);
    dcc6681Interrupt((cc->status & cc->intMask) != 0);

    strcpy_s(cc->curFileName, sizeof(cc->curFileName), str);
    printf("(cr3447 ) Loaded with '%s'\n", str);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Show card reader status (operator interface).
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cr3447ShowStatus(void)
    {
    CrContext *cp = firstCr3447;

    if (cp == NULL)
        {
        return;
        }

    printf("\n    > Card Reader (cr3447) Status:\n");

    while (cp)
        {
        printf("    >   CH %02o EQ %02o UN %02o Col %02i Mode(%s) Raw(%s) Seq:%i File '%s'\n",
               cp->channelNo,
               cp->eqNo,
               cp->unitNo,
               cp->col,
               cp->binary ? "Char" : "Bin ",
               cp->rawCard ? "Yes" : "No ",
               cp->seqNum,
               cp->curFileName);

        if (cp->isWatched)
            {
            printf("    >   Autoloading from '%s' to '%s'\n",
                   cp->dirInput,
                   cp->dirOutput);
            }

        cp = cp->nextUnit;
        }
    printf("\n");
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
            cr3447NextCard(active3000Device, cc, stdout);
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
            cr3447NextCard(active3000Device, cc, stdout);
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
static bool cr3447StartNextDeck(DevSlot *up, CrContext *cc, FILE *out)
    {
    char *fname;

    while (cc->outDeck != cc->inDeck)
        {
        fname      = cc->decks[cc->outDeck];
        up->fcb[0] = fopen(fname, "r");
        if (up->fcb[0] != NULL)
            {
            cc->status = StCr3447Eof;
            cc->status = StCr3447Ready;
            cr3447NextCard(up, cc, out);
            activeDevice = channelFindDevice(up->channel->id, DtDcc6681);
            dcc6681Interrupt((cc->status & cc->intMask) != 0);
            fprintf(out, "\n(cr3447 ) Cards loaded on card reader C%o,E%o\n", cc->channelNo, cc->eqNo);

            return TRUE;
            }
        printf("\n(cr3447 ) Failed to open card deck %s\n", fname);
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
static void cr3447NextCard(DevSlot *up, CrContext *cc, FILE *out)
    {
    static char buffer[326];
    char        *cp;
    char        c;
    int         value;
    int         i;
    int         j;
    PpWord      col1;

    static char fnwork[_MAX_PATH] = "";

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


        printf("(cr3447 ) End of Deck '%s' reached on channel %o equipment %o\n",
               cc->curFileName,
               up->channel->id,
               up->eqNo);

        unlink(cc->decks[cc->outDeck]);
        free(cc->decks[cc->outDeck]);
        cc->outDeck = (cc->outDeck + 1) % Cr3447MaxDecks;
        if (cr3447StartNextDeck(up, cc, out))
            {
            return;
            }

//TODO: Check this next phase and continue fixing up messages

        /*
        **  If the current file comes from the "input" directory specified
        **      then
        **          if the output directory exists
        **              then we move it to the "output" directory if specified
        **              else we remove the file from the "input" directory
        **      else we leave it alone
        **
        **  clear the filename string.
        **
        **  If the "output" directory is specified, move the file
        **  to the "processed" state
        */
        bool isFromInput = (!strncmp(cc->curFileName, cc->dirInput, strlen(cc->dirInput)));
        if (isFromInput)
            {
            //  Files from the input directory are handled specially
            if (cc->dirOutput[0] == '\0')
                {
                //  Once a file is closed, we can simply delete it
                //  from the input directory.  This will also trigger
                //  the filechange watcher who will initiate the next
                //  file load automatically.

                remove(cc->curFileName);
                }
            else
                {
                //  perform the rename of the current file to the "Processed"
                //  directory.  This rename will ALSO trigger the filechange
                //  watcher.  Otherwise the operator will need to use the load
                //  command from the console.

                int fnindex = 0;

                while (TRUE)
                    {
                    //  create the file's new name
                    sprintf_s(fnwork, sizeof(fnwork), "%s/%s_%04i", cc->dirOutput, cc->curFileName + strlen(cc->dirInput) + 1, fnindex);
                    if (rename(cc->curFileName, fnwork) == 0)
                        {
                        printf("(cr3447 ) Deck '%s' moved to '%s'. (Input Preserved)\n",
                               cc->curFileName + strlen(cc->dirInput) + 1,
                               fnwork);
                        break;
                        }
                    else
                        {
                        printf("(cr3447 ) Rename Failure on '%s' - (%s). Retrying (%d)...\n",
                               cc->curFileName + strlen(cc->dirInput) + 1,
                               strerror(errno),
                               fnindex);
                        }
                    fnindex++;
                    } // while(TRUE)
                if (fnindex > 0)
                    {
                    printf("\n");
                    }
                } // else
            }     // if (isFromInput)

        cc->curFileName[0] = '\0';

        return;
        }

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
    static char buf[30];

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
