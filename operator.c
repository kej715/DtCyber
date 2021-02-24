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

static void opCmdHelp(bool help, char *cmdParams);
static void opHelpHelp(void);

static void opCmdLoadCards(bool help, char *cmdParams);
static void opHelpLoadCards(void);

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
    "lc",                       opCmdLoadCards,
    "lt",                       opCmdLoadTape,
    "rc",                       opCmdRemoveCards,
    "rp",                       opCmdRemovePaper,
    "p",                        opCmdPause,
    "st",                       opCmdShowTape,
    "ut",                       opCmdUnloadTape,
    "load_cards",               opCmdLoadCards,
    "load_tape",                opCmdLoadTape,
    "remove_cards",             opCmdRemoveCards,
    "remove_paper",             opCmdRemovePaper,
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
    char name[80];
    char *params;
    char *pos;

    printf("\n%s.", DtCyberVersion " - " DtCyberCopyright);
    printf("\n%s.", DtCyberLicense);
    printf("\n%s.", DtCyberLicenseDetails);
    printf("\n\nOperator interface");
    printf("\nPlease enter 'help' to get a list of commands\n");
    printf("\nOperator> ");

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
        if (fgets(cmd, sizeof(cmd), stdin) == NULL || strlen(cmd) == 0)
            {
            continue;
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
    /*
    **  Process help request.
    */
    if (help)
        {
        opHelpLoadCards();
        return;
        }

    /*
    **  Check parameters and process command.
    */
    if (strlen(cmdParams) == 0)
        {
        printf("parameters expected\n");
        opHelpLoadCards();
        return;
        }

    cr405LoadCards(cmdParams);
    cr3447LoadCards(cmdParams);
    }

static void opHelpLoadCards(void)
    {
    printf("'load_cards <channel>,<equipment>,<filename>' load specified card stack file.\n");
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
    printf("'remove_paper <channel>,<equipment>' remover paper from printer.\n");
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
    printf("'remove_cards <channel>,<equipment>' remover cards from card puncher.\n");
    }

/*---------------------------  End Of File  ------------------------------*/
