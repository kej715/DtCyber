/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**
**  Name: operator.c
**
**  Description:
**      Provide operator interface for CDC 6600 emulation. This is required
**      to enable a human "operator" to change tapes, remove paper from the
**      printer, shutdown etc.
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
#include <sys/types.h>
#include <ctype.h>
#include "const.h"
#include "types.h"
#include "proto.h"
#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif


/*
**  -----------------
**  Private Constants
**  -----------------
*/
#define MaxCardParams 10

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
typedef struct opCmd
    {
    char            *name;               /* command name */
    void            (*handler)(bool help, char *cmdParams);
    } OpCmd;


/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static void opCreateThread(void);
#if defined(_WIN32)
static void opThread(void *param);
#else
static void *opThread(void *param);
#endif

static char *opGetString(char *inStr, char *outStr, int outSize);

static void opCmdDumpMemory(bool help, char *cmdParams);
static void opCmdDumpCM(int fwa, int count);
static void opCmdDumpEM(int fwa, int count);
static void opCmdDumpPP(int pp, int fwa, int count);
static void opHelpDumpMemory(void);

static void opCmdEnterKeys(bool help, char *cmdParams);
static void opHelpEnterKeys(void);
static void opWaitKeyConsume();
static void opWaitKeyInterval(long milliseconds);

static void opCmdHelp(bool help, char *cmdParams);
static void opHelpHelp(void);

static void opCmdLoadCards(bool help, char *cmdParams);
static void opHelpLoadCards(void);
static int opPrepCards(char *fname, FILE *fcb);

static void opCmdLoadTape(bool help, char *cmdParams);
static void opHelpLoadTape(void);

static void opCmdShowTape(bool help, char *cmdParams);
static void opHelpShowTape(void);

static void opCmdUnloadTape(bool help, char *cmdParams);
static void opHelpUnloadTape(void);

static void opCmdRemoveCards(bool help, char *cmdParams);
static void opHelpRemoveCards(void);

static void opCmdRemovePaper(bool help, char *cmdParams);
static void opHelpRemovePaper(void);

static void opCmdSetKeyInterval(bool help, char *cmdParams);
static void opHelpSetKeyInterval(void);

static void opCmdShutdown(bool help, char *cmdParams);
static void opHelpShutdown(void);

static void opCmdPause(bool help, char *cmdParams);
static void opHelpPause(void);

/*
**  ----------------
**  Public Variables
**  ----------------
*/
volatile bool opActive = FALSE;

/*
**  -----------------
**  Private Variables
**  -----------------
*/
static OpCmd decode[] = 
    {
    "d",                        opCmdDumpMemory,
    "dm",                       opCmdDumpMemory,
    "e",                        opCmdEnterKeys,
    "ek",                       opCmdEnterKeys,
    "lc",                       opCmdLoadCards,
    "lt",                       opCmdLoadTape,
    "rc",                       opCmdRemoveCards,
    "rp",                       opCmdRemovePaper,
    "p",                        opCmdPause,
    "ski",                      opCmdSetKeyInterval,
    "st",                       opCmdShowTape,
    "ut",                       opCmdUnloadTape,
    "dump_memory",              opCmdDumpMemory,
    "enter_keys",               opCmdEnterKeys,
    "load_cards",               opCmdLoadCards,
    "load_tape",                opCmdLoadTape,
    "remove_cards",             opCmdRemoveCards,
    "remove_paper",             opCmdRemovePaper,
    "set_key_interval",         opCmdSetKeyInterval,
    "show_tape",                opCmdShowTape,
    "unload_tape",              opCmdUnloadTape,
    "?",                        opCmdHelp,
    "help",                     opCmdHelp,
    "shutdown",                 opCmdShutdown,
    "pause",                    opCmdPause,
    NULL,                       NULL
    };

static void (*opCmdFunction)(bool help, char *cmdParams);
static char opCmdParams[256];
static volatile bool opPaused = FALSE;

/*
**--------------------------------------------------------------------------
**
**  Public Functions
**
**--------------------------------------------------------------------------
*/

/*--------------------------------------------------------------------------
**  Purpose:        Operator interface initialisation.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void opInit(void)
    {
    /*
    **  Create the operator thread which accepts command input.
    */
    opCreateThread();
    }

/*--------------------------------------------------------------------------
**  Purpose:        Operator request handler called from the main emulation
**                  thread to avoid race conditions.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void opRequest(void)
    {
    if (opActive)
        {
        opCmdFunction(FALSE, opCmdParams);
        opActive = FALSE;

        if (emulationActive)
            {
            printf("\nOperator> ");
            }

        fflush(stdout);
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
**  Purpose:        Create operator thread.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void opCreateThread(void)
    {
#if defined(_WIN32)
    DWORD dwThreadId; 
    HANDLE hThread;

    /*
    **  Create operator thread.
    */
    hThread = CreateThread( 
        NULL,                                       // no security attribute 
        0,                                          // default stack size 
        (LPTHREAD_START_ROUTINE)opThread, 
        (LPVOID)NULL,                               // thread parameter 
        0,                                          // not suspended 
        &dwThreadId);                               // returns thread ID 

    if (hThread == NULL)
        {
        fprintf(stderr, "Failed to create operator thread\n");
        exit(1);
        }
#else
    int rc;
    pthread_t thread;
    pthread_attr_t attr;

    /*
    **  Create POSIX thread with default attributes.
    */
    pthread_attr_init(&attr);
    rc = pthread_create(&thread, &attr, opThread, NULL);
    if (rc < 0)
        {
        fprintf(stderr, "Failed to create operator thread\n");
        exit(1);
        }
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Operator thread.
**
**  Parameters:     Name        Description.
**                  param       Thread parameter (unused)
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
#if defined(_WIN32)
#include <conio.h>
static void opThread(void *param)
#else
static void *opThread(void *param)
#endif
    {
    OpCmd *cp;
    char cmd[256];
    bool isOpSection;
    char *line;
    char name[80];
    char *params;
    char *pos;

    printf("\n%s.", DtCyberVersion " - " DtCyberCopyright);
    printf("\n%s.", DtCyberLicense);
    printf("\n%s.", DtCyberLicenseDetails);
    printf("\n\nOperator interface");
    printf("\nPlease enter 'help' to get a list of commands\n");
    printf("\nOperator> ");

    isOpSection = initOpenOperatorSection() == 1;

    while (emulationActive)
        {
        fflush(stdout);

        #if defined(_WIN32)
        if (!kbhit())
            {
            Sleep(50);
            continue;
            }
        #endif

        /*
        **  Wait for command input.
        */
        if (isOpSection)
            {
            if (opPaused || opActive) continue;
            line = initGetNextLine();
            if (line == NULL)
                {
                isOpSection = FALSE;
                continue;
                }
            strcpy(cmd, line);
            fputs(cmd, stdout);
            fputs("\n", stdout);
            fflush(stdout);
            }
        else
            {
            if (fgets(cmd, sizeof(cmd), stdin) == NULL || strlen(cmd) == 0)
                {
                continue;
                }
            }

        if (opPaused)
            {
            /*
            **  Unblock main emulation thread.
            */
            opPaused = FALSE;
            continue;
            }

        if (opActive)
            {
            /*
            **  The main emulation thread is still busy executing the command.
            */
            printf("\nPrevious request still busy");
            continue;
            }

        /*
        **  Replace newline by zero terminator.
        */
        pos = strchr(cmd, '\n');
        if (pos != NULL)
            {
            *pos = 0;
            }

        /*
        **  Extract the command name.
        */
        params = opGetString(cmd, name, sizeof(name));
        if (*name == 0)
            {
            printf("\nOperator> ");
            continue;
            }

        /*
        **  Find the command handler.
        */
        for (cp = decode; cp->name != NULL; cp++)
            {
            if (strcmp(cp->name, name) == 0)
                {
                if (cp->handler == opCmdEnterKeys)
                    {
                    opCmdEnterKeys(FALSE, params);
                    opActive = FALSE;
                    break;
                    }
                /*
                **  Request the main emulation thread to execute the command.
                */
                strcpy(opCmdParams, params);
                opCmdFunction = cp->handler;
                opActive = TRUE;
                break;
                }
            }

        if (cp->name == NULL)
            {
            /*
            **  Try to help user.
            */
            printf("Command not implemented: %s\n\n", name);
            printf("Try 'help' to get a list of commands or 'help <command>'\n");
            printf("to get a brief description of a command.\n");
            printf("\nOperator> ");
            continue;
            }
        }

#if !defined(_WIN32)
    return(NULL);
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Parse command string and return the first string
**                  terminated by whitespace
**
**  Parameters:     Name        Description.
**                  inStr       Input string
**                  outStr      Output string
**                  outSize     Size of output field
**
**  Returns:        Pointer to first character in input string after
**                  the extracted string..
**
**------------------------------------------------------------------------*/
static char *opGetString(char *inStr, char *outStr, int outSize)
    {
    u8 pos = 0;
    u8 len = 0;

    /*
    **  Skip leading whitespace.
    */
    while (inStr[pos] != 0 && isspace(inStr[pos]))
        {
        pos += 1;
        }

    /*
    **  Return pointer to end of input string when there was only
    **  whitespace left.
    */
    if (inStr[pos] == 0)
        {
        *outStr = 0;
        return(inStr + pos);
        }

    /*
    **  Copy string to output buffer.
    */
    while (inStr[pos] != 0 && !isspace(inStr[pos]))
        {
        if (len >= outSize - 1)
            {
            return(NULL);
            }

        outStr[len++] = inStr[pos++];
        }

    outStr[len] = 0;

    /*
    **  Skip trailing whitespace.
    */
    while (inStr[pos] != 0 && isspace(inStr[pos]))
        {
        pos += 1;
        }

    return(inStr + pos);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Dump CM, EM, or PP memory.
**
**  Parameters:     Name        Description.
**                  help        Request only help on this command.
**                  cmdParams   Command parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void opCmdDumpMemory(bool help, char *cmdParams)
    {
    int count;
    char *cp;
    int fwa;
    char *memType;
    int numParam;
    int pp;

    /*
    **  Process help request.
    */
    if (help)
        {
        opHelpDumpMemory();
        return;
        }

    cp = memType = cmdParams;
    while (*cp != '\0' && *cp != ',') ++cp;
    if (*cp != ',')
        {
        printf("Not enough parameters\n");
        return;
        }
    *cp++ = '\0';
    numParam = sscanf(cp, "%o,%d", &fwa, &count);
    if (numParam != 2)
        {
        printf("Not enough or invalid parameters\n");
        return;
        }
    if (strcasecmp(memType, "CM") == 0)
        {
        opCmdDumpCM(fwa, count);
        }
    else if (strcasecmp(memType, "EM") == 0)
        {
        opCmdDumpEM(fwa, count);
        }
    else if (strncasecmp(memType, "PP", 2) == 0)
        {
        numParam = sscanf(memType + 2, "%o", &pp);
        if (numParam != 1)
            {
            printf("Missing or invalid PP number\n");
            }
        opCmdDumpPP(pp, fwa, count);
        }
    else
        {
        printf("Invalid memory type\n");
        }
    }

static void opCmdDumpCM(int fwa, int count)
    {
    char buf[42];
    char *cp;
    int limit;
    int n;
    int shiftCount;
    CpWord word;

    if (fwa < 0 || count < 0 || fwa + count > cpuMaxMemory)
        {
        printf("Invalid CM address or count\n");
        return;
        }
    for (limit = fwa + count; fwa < limit; fwa++)
        {
        word = cpMem[fwa];
        n = sprintf(buf, "%08o %020lo ", fwa, word);
        cp = buf + n;
        for (shiftCount = 54; shiftCount >= 0; shiftCount -= 6)
            {
            *cp++ = cdcToAscii[(word >> shiftCount) & 077];
            }
        *cp++ = '\n';
        *cp = '\0';
        fputs(buf, stdout);
        }
    }

static void opCmdDumpEM(int fwa, int count)
    {
    char buf[42];
    char *cp;
    int limit;
    int n;
    int shiftCount;
    CpWord word;

    if (fwa < 0 || count < 0 || fwa + count > extMaxMemory)
        {
        printf("Invalid EM address or count\n");
        return;
        }
    for (limit = fwa + count; fwa < limit; fwa++)
        {
        word = extMem[fwa];
        n = sprintf(buf, "%08o %020lo ", fwa, word);
        cp = buf + n;
        for (shiftCount = 54; shiftCount >= 0; shiftCount -= 6)
            {
            *cp++ = cdcToAscii[(word >> shiftCount) & 077];
            }
        *cp++ = '\n';
        *cp = '\0';
        fputs(buf, stdout);
        }
    }

static void opCmdDumpPP(int ppNum, int fwa, int count)
    {
    char buf[20];
    char *cp;
    int limit;
    int n;
    PpSlot *pp;
    PpWord word;

    if (ppNum >= 020 && ppNum <= 031)
        {
        ppNum -= 6;
        }
    else if (ppNum < 0 || ppNum > 011)
        {
        printf("Invalid PP number\n");
        return;
        }
    if (ppNum >= ppuCount)
        {
        printf("Invalid PP number\n");
        return;
        }
    if (fwa < 0 || count < 0 || fwa + count > 010000)
        {
        printf("Invalid PP address or count\n");
        return;
        }
    pp = &ppu[ppNum];
    for (limit = fwa + count; fwa < limit; fwa++)
        {
        word = pp->mem[fwa];
        n = sprintf(buf, "%04o %04o ", fwa, word);
        cp = buf + n;
        *cp++ = cdcToAscii[(word >> 6) & 077];
        *cp++ = cdcToAscii[word & 077];
        *cp++ = '\n';
        *cp = '\0';
        fputs(buf, stdout);
        }
    }

static void opHelpDumpMemory(void)
    {
    printf("'dump_memory CM,<fwa>,<count>' dump <count> words of central memory starting from octal address <fwa>.\n");
    printf("'dump_memory EM,<fwa>,<count>' dump <count> words of extended memory starting from octal address <fwa>.\n");
    printf("'dump_memory PP<nn>,<fwa>,<count>' dump <count> words of PP nn's memory starting from octal address <fwa>.\n");
    }

/*--------------------------------------------------------------------------
**  Purpose:        Enter keys on the system console.
**
**  Parameters:     Name        Description.
**                  help        Request only help on this command.
**                  cmdParams   Command parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
char opKeyIn = 0;
long opKeyInterval = 250;

static void opCmdEnterKeys(bool help, char *cmdParams)
    {
    char *bp;
    time_t clock;
    char *cp;
    char keybuf[256];
    char *kp;
    char *limit;
    long msec;
    char timestamp[16];
    struct tm *tmp;

    /*
    **  Process help request.
    */
    if (help)
        {
        opHelpEnterKeys();
        return;
        }

    /*
     *  First, edit the parameter string to subtitute values
     *  for keywords. Keywords are delimited by '%'.
     */
    clock = time(NULL);
    tmp = localtime(&clock);
    sprintf(timestamp, "%02d%02d%02d%02d%02d%02d",
        (u8)tmp->tm_year - 100, (u8)tmp->tm_mon + 1, (u8)tmp->tm_mday,
        (u8)tmp->tm_hour, (u8)tmp->tm_min, (u8)tmp->tm_sec);
    cp = cmdParams;
    bp = keybuf;
    limit = bp + sizeof(keybuf) - 2;
    while (*cp != '\0' && bp < limit)
        {
        if (*cp == '%')
            {
            kp = ++cp;
            while (*cp != '%' && *cp != '\0') cp += 1;
            if (*cp == '%') *cp++ = '\0';
            if (strcasecmp(kp, "year") == 0)
                {
                memcpy(bp, timestamp, 2);
                bp += 2;
                }
            else if (strcasecmp(kp, "mon") == 0)
                {
                memcpy(bp, timestamp + 2, 2);
                bp += 2;
                }
            else if (strcasecmp(kp, "day") == 0)
                {
                memcpy(bp, timestamp + 4, 2);
                bp += 2;
                }
            else if (strcasecmp(kp, "hour") == 0)
                {
                memcpy(bp, timestamp + 6, 2);
                bp += 2;
                }
            else if (strcasecmp(kp, "min") == 0)
                {
                memcpy(bp, timestamp + 8, 2);
                bp += 2;
                }
            else if (strcasecmp(kp, "sec") == 0)
                {
                memcpy(bp, timestamp + 10, 2);
                bp += 2;
                }
            else
                {
                printf("Unrecognized keyword: %%%s%%\n", kp);
                printf("\nOperator> ");
                return;
                }
            }
        else
            {
            *bp++ = *cp++;
            }
        }
    if (bp > limit)
        {
        printf("Key sequence is too long\n");
        printf("\nOperator> ");
        return;
        }
    *bp = '\0';
    /*
     *  Next, traverse the key sequence, supplying keys to the console
     *  one by one. Recognize and process special characters along the
     *  way as follows:
     *
     *    ! - end the sequence, and do not send an <enter> key
     *    ; - send an <enter> key within a sequence
     *    _ - send a blank
     *    ^ - send a backspace
     *    # - delimit a milliseconds value (e.g., #500#) and pause for
     *        the speccified amount of time
     */
    opWaitKeyConsume(); // just in case
    cp = keybuf;
    while (*cp != '\0' && *cp != '!')
        {
        switch (*cp)
            {
        default:
            opKeyIn = *cp;
            break;
        case ';':
            opKeyIn = '\r';
            break;
        case '_':
            opKeyIn = ' ';
            break;
        case '^':
            opKeyIn = '\b';
            break;
        case '#':
            msec = 0;
            cp += 1;
            while (*cp >= '0' && *cp <= '9')
                {
                msec = (msec * 10) + (*cp++ - '0');
                }
            if (*cp != '#') cp -= 1;
            opWaitKeyInterval(msec);
            break;
            }
        cp += 1;
        opWaitKeyInterval(opKeyInterval);
        opWaitKeyConsume();
        }
    if (*cp != '!')
        {
        opKeyIn = '\r';
        opWaitKeyInterval(opKeyInterval);
        opWaitKeyConsume();
        }
    printf("\nOperator> ");
    }

static void opHelpEnterKeys(void)
    {
    printf("'enter_keys <key-sequence>' supply a sequence of key entries to the system console .\n");
    fputs("     Special keys:\n", stdout);
    fputs("       ! - end sequence without sending <enter> key\n", stdout);
    fputs("       ; - send <enter> key within a sequence\n", stdout);
    fputs("       _ - send <blank> key\n", stdout);
    fputs("       ^ - send <backspace> key\n", stdout);
    fputs("       % - keyword delimiter for keywords:\n", stdout);
    fputs("           %year% insert current year\n", stdout);
    fputs("           %mon%  insert current month\n", stdout);
    fputs("           %day%  insert current day\n", stdout);
    fputs("           %hour% insert current hour\n", stdout);
    fputs("           %min%  insert current minute\n", stdout);
    fputs("           %sec%  insert current second\n", stdout);
    fputs("       # - delimiter for milliseconds pause value (e.g., #500#)\n", stdout);
    }

static void opWaitKeyConsume()
    {
    while (opKeyIn != 0 || ppKeyIn != 0)
        {
        #if defined(_WIN32)
        Sleep(100);
        #else
        usleep(100000);
        #endif
        }
    }

static void opWaitKeyInterval(long milliseconds)
    {
    #if defined(_WIN32)
    Sleep(milliseconds);
    #else
    usleep((useconds_t)(milliseconds * 1000));
    #endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Set interval between console key entries.
**
**  Parameters:     Name        Description.
**                  help        Request only help on this command.
**                  cmdParams   Command parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void opCmdSetKeyInterval(bool help, char *cmdParams)
    {
    int msec;
    int numParam;

    /*
    **  Process help request.
    */
    if (help)
        {
        opHelpSetKeyInterval();
        return;
        }
    numParam = sscanf(cmdParams, "%d", &msec);
    if (numParam != 1)
        {
        printf("Missing or invalid parameter\n");
        return;
        }
    opKeyInterval = msec;
    }

static void opHelpSetKeyInterval(void)
    {
    printf("'set_key_interval <millisecs>' set the interval between key entries to the system console .\n");
    }

/*--------------------------------------------------------------------------
**  Purpose:        Pause emulation.
**
**  Parameters:     Name        Description.
**                  help        Request only help on this command.
**                  cmdParams   Command parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void opCmdPause(bool help, char *cmdParams)
    {
    /*
    **  Process help request.
    */
    if (help)
        {
        opHelpPause();
        return;
        }

    /*
    **  Check parameters.
    */
    if (strlen(cmdParams) != 0)
        {
        printf("no parameters expected\n");
        opHelpPause();
        return;
        }

    /*
    **  Process command.
    */
    printf("Emulation paused - hit Enter to resume\n");

    /*
    **  Wait for Enter key.
    */
    opPaused = TRUE;
    while (opPaused)
        {
        /* wait for operator thread to clear the flag */
        #if defined(_WIN32)
        Sleep(500);
        #else
        usleep(500000);
        #endif
        }
    }

static void opHelpPause(void)
    {
    printf("'pause' suspends emulation to reduce CPU load.\n");
    }

/*--------------------------------------------------------------------------
**  Purpose:        Terminate emulation.
**
**  Parameters:     Name        Description.
**                  help        Request only help on this command.
**                  cmdParams   Command parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void opCmdShutdown(bool help, char *cmdParams)
    {
    /*
    **  Process help request.
    */
    if (help)
        {
        opHelpShutdown();
        return;
        }

    /*
    **  Check parameters.
    */
    if (strlen(cmdParams) != 0)
        {
        printf("no parameters expected\n");
        opHelpShutdown();
        return;
        }

    /*
    **  Process command.
    */
    opActive = FALSE;
    emulationActive = FALSE;

    printf("\nThanks for using %s\nGoodbye for now.\n\n", DtCyberVersion);
    }

static void opHelpShutdown(void)
    {
    printf("'shutdown' terminates emulation.\n");
    }

/*--------------------------------------------------------------------------
**  Purpose:        Provide command help.
**
**  Parameters:     Name        Description.
**                  help        Request only help on this command.
**                  cmdParams   Command parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void opCmdHelp(bool help, char *cmdParams)
    {
    OpCmd *cp;

    /*
    **  Process help request.
    */
    if (help)
        {
        opHelpHelp();
        return;
        }

    /*
    **  Check parameters and process command.
    */
    if (strlen(cmdParams) == 0)
        {
        /*
        **  List all available commands.
        */
        printf("\nList of available commands:\n\n");
        for (cp = decode; cp->name != NULL; cp++)
            {
            printf("%s\n", cp->name);
            }

        printf("\nTry 'help <command> to get a brief description of a command.\n");
        return;
        }
    else
        {
        /*
        **  Provide help for specified command.
        */
        for (cp = decode; cp->name != NULL; cp++)
            {
            if (strcmp(cp->name, cmdParams) == 0)
                {
                printf("\n");
                cp->handler(TRUE, NULL);
                return;
                }
            }

        printf("Command not implemented: %s\n", cmdParams);
        }
    }

static void opHelpHelp(void)
    {
    printf("'help'       list all available commands.\n");
    printf("'help <cmd>' provide help for <cmd>.\n");
    }

/*--------------------------------------------------------------------------
**  Purpose:        Load a stack of cards
**
**  Parameters:     Name        Description.
**                  help        Request only help on this command.
**                  cmdParams   Command parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void opCmdLoadCards(bool help, char *cmdParams)
    {
    int channelNo;
    int equipmentNo;
    FILE *fcb;
    char fname[80];
    int numParam;
    int rc;
    static int seqNo = 1;
    static char str[200];

    /*
    **  Process help request.
    */
    if (help)
        {
        opHelpLoadCards();
        return;
        }

    numParam = sscanf(cmdParams,"%o,%o,%s",&channelNo, &equipmentNo, str);

    /*
    **  Check parameters.
    */
    if (numParam < 3)
        {
        printf("Not enough or invalid parameters\n");
        opHelpLoadCards();
        return;
        }

    if (channelNo < 0 || channelNo >= MaxChannels)
        {
        printf("Invalid channel no\n");
        return;
        }

    if (equipmentNo < 0 || equipmentNo >= MaxEquipment)
        {
        printf("Invalid equipment no\n");
        return;
        }

    if (str[0] == 0)
        {
        printf("Invalid file name\n");
        return;
        }
    /*
    **  Create temporary file for preprocessed card deck
    */
    sprintf(fname, "CR_C%o_E%o_%d", channelNo, equipmentNo, seqNo++);
    fcb = fopen(fname, "w");
    if (fcb == NULL)
        {
        printf("Failed to create %s\n", fname);
        return;
        }

    /*
    **  Preprocess card file
    */
    rc = opPrepCards(str, fcb);
    fclose(fcb);
    if (rc == -1)
        {
        unlink(fname);
        return;
        }
    cr405LoadCards(fname, channelNo, equipmentNo);
    cr3447LoadCards(fname, channelNo, equipmentNo);
    }

static void opHelpLoadCards(void)
    {
    printf("'load_cards <channel>,<equipment>,<filename>[,<p1>,<p2>,...,<pn>]' load specified card file with optional parameters.\n");
    }

/*--------------------------------------------------------------------------
**  Purpose:        Preprocess a card file
**
**                  The specified source file is read, nested "~include"
**                  directives are detected and processed recursively,
**                  and embedded parameter references are interpolated.
**
**  Parameters:     Name        Description.
**                  str         Source file path and optional parameters
**                  fcb         handle of output file
**
**  Returns:        0 if success
**                 -1 if failure
**
**------------------------------------------------------------------------*/
static int opPrepCards(char *str, FILE *fcb)
    {
    int argc;
    int argi;
    char *argv[MaxCardParams];
    char *cp;
    char dbuf[400];
    char *dfltVal;
    int dfltValLen;
    char *dp;
    FILE *in;
    char *lastnb;
    char params[100];
    char sbuf[400];
    char *sp;

    /*
    **  The parameter string has the form:
    **
    **    <filepath>,<arg1>,<arg2>,...,<argn>
    **
    **  where the args are optional.
    */
    argc = 0;
    cp = strchr(str, ',');
    if (cp != NULL)
        {
        *cp++ = '\0';
        while (*cp != '\0')
            {
            if (argc < MaxCardParams) argv[argc++] = cp;
            cp = strchr(cp, ',');
            if (cp == NULL) break;
            *cp++ = '\0';
            }
        }
    /*
    **  Open and parse the input file
    */
    in = fopen(str, "r");
    if (in == NULL)
        {
        printf("Failed to open %s\n", str);
        return -1;
        }
    while (TRUE)
        {
        sp = fgets(sbuf, sizeof(sbuf), in);
        if (sp == NULL)
            {
            fclose(in);
            return 0;
            }
        /*
        **  Scan the source line for parameter references and interpolate
        **  any found. A parameter reference has the form "${n}" where "n"
        **  is an integer greater than 0.
        */
        dp = dbuf;
        while (*sp != '\0')
            {
            if (*sp == '$' && *(sp + 1) == '{' && isdigit(*(sp + 2)))
                {
                argi = 0;
                dfltVal = "";
                dfltValLen = 0;
                cp = sp + 2;
                while (isdigit(*cp))
                    {
                    argi = (argi * 10) + (*cp++ - '0');
                    }
                if (*cp == ':')
                    {
                    dfltVal = ++cp;
                    while (*cp != '}' && *cp != '\0') cp += 1;
                    dfltValLen = cp - dfltVal;
                    }
                if (*cp == '}')
                    {
                    sp = cp + 1;
                    argi -= 1;
                    if (argi >= 0 && argi < argc)
                        {
                        cp = argv[argi];
                        while (*cp != '\0') *dp++ = *cp++;
                        }
                    else
                        {
                        while (dfltValLen-- > 0) *dp++ = *dfltVal++;
                        }
                    continue;
                    }
                }
            *dp++ = *sp++;
            }
        *dp = '\0';
        /*
        **  Recognize nested "~include" directives and
        **  process them recursively.
        */
        sp = dbuf;
        if (strncmp(sp, "~include ", 9) == 0)
            {
            sp += 9;
            while (isspace(*sp)) sp += 1;
            if (*sp == '\0')
                {
                printf("File name missing from ~include in %s\n", str);
                fclose(in);
                return -1;
                }
            if (*sp != '/')
                {
                cp = strrchr(str, '/');
                if (cp != NULL)
                    {
                    *cp = '\0';
                    sprintf(params, "%s/%s", str, sp);
                    *cp = '/';
                    sp = params;
                    }
                }
            /*
            **  Trim trailing whitespace from pathname and parameters
            */
            lastnb = sp;
            for (cp = sp; *cp != '\0'; cp++)
                 {
                 if (!isspace(*cp)) lastnb = cp;
                 }
            *(lastnb + 1) = '\0';
            /*
            **  Process nested include file recursively
            */
            if (opPrepCards(sp, fcb) == -1)
                {
                fclose(in);
                return -1;
                }
            }
        /*
        **  Recognize and ignore embedded comments. Embedded comments
        **  are lines beginning with "~*".
        */
        else if (strncmp(sp, "~*", 2) != 0)
            {
            fputs(sp, fcb);
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Load a new tape
**
**  Parameters:     Name        Description.
**                  help        Request only help on this command.
**                  cmdParams   Command parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void opCmdLoadTape(bool help, char *cmdParams)
    {
    /*
    **  Process help request.
    */
    if (help)
        {
        opHelpLoadTape();
        return;
        }

    /*
    **  Check parameters and process command.
    */
    if (strlen(cmdParams) == 0)
        {
        printf("parameters expected\n");
        opHelpLoadTape();
        return;
        }

    mt669LoadTape(cmdParams);
    mt679LoadTape(cmdParams);
    mt362xLoadTape(cmdParams);
    }

static void opHelpLoadTape(void)
    {
    printf("'load_tape <channel>,<equipment>,<unit>,<r|w>,<filename>' load specified tape.\n");
    }

/*--------------------------------------------------------------------------
**  Purpose:        Unload a mounted tape
**
**  Parameters:     Name        Description.
**                  help        Request only help on this command.
**                  cmdParams   Command parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void opCmdUnloadTape(bool help, char *cmdParams)
    {
    /*
    **  Process help request.
    */
    if (help)
        {
        opHelpUnloadTape();
        return;
        }

    /*
    **  Check parameters and process command.
    */
    if (strlen(cmdParams) == 0)
        {
        printf("parameters expected\n");
        opHelpUnloadTape();
        return;
        }

    mt669UnloadTape(cmdParams);
    mt679UnloadTape(cmdParams);
    mt362xUnloadTape(cmdParams);
    }

static void opHelpUnloadTape(void)
    {
    printf("'unload_tape <channel>,<equipment>,<unit>' unload specified tape unit.\n");
    }

/*--------------------------------------------------------------------------
**  Purpose:        Show status of all tape units
**
**  Parameters:     Name        Description.
**                  help        Request only help on this command.
**                  cmdParams   Command parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void opCmdShowTape(bool help, char *cmdParams)
    {
    /*
    **  Process help request.
    */
    if (help)
        {
        opHelpShowTape();
        return;
        }

    /*
    **  Check parameters and process command.
    */
    if (strlen(cmdParams) != 0)
        {
        printf("no parameters expected\n");
        opHelpShowTape();
        return;
        }

    mt669ShowTapeStatus();
    mt679ShowTapeStatus();
    mt362xShowTapeStatus();
    mt5744ShowTapeStatus();
    }

static void opHelpShowTape(void)
    {
    printf("'show_tape' show status of all tape units.\n");
    }

/*--------------------------------------------------------------------------
**  Purpose:        Remove paper from printer.
**
**  Parameters:     Name        Description.
**                  help        Request only help on this command.
**                  cmdParams   Command parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void opCmdRemovePaper(bool help, char *cmdParams)
    {
    /*
    **  Process help request.
    */
    if (help)
        {
        opHelpRemovePaper();
        return;
        }

    /*
    **  Check parameters and process command.
    */
    if (strlen(cmdParams) == 0)
        {
        printf("parameters expected\n");
        opHelpRemovePaper();
        return;
        }

    lp1612RemovePaper(cmdParams);
    lp3000RemovePaper(cmdParams);
    }

static void opHelpRemovePaper(void)
    {
    printf("'remove_paper <channel>,<equipment>[,<filename>]' remover paper from printer.\n");
    }

/*--------------------------------------------------------------------------
**  Purpose:        Remove cards from card puncher.
**
**  Parameters:     Name        Description.
**                  help        Request only help on this command.
**                  cmdParams   Command parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void opCmdRemoveCards(bool help, char *cmdParams)
    {
    /*
    **  Process help request.
    */
    if (help)
        {
        opHelpRemoveCards();
        return;
        }

    /*
    **  Check parameters and process command.
    */
    if (strlen(cmdParams) == 0)
        {
        printf("parameters expected\n");
        opHelpRemoveCards();
        return;
        }

    cp3446RemoveCards(cmdParams);
    }

static void opHelpRemoveCards(void)
    {
    printf("'remove_cards <channel>,<equipment>[,<filename>]' remover cards from card puncher.\n");
    }

/*---------------------------  End Of File  ------------------------------*/
