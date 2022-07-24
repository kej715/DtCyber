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
#include <sys/stat.h>
#include <ctype.h>
#include <fcntl.h>
#include "const.h"
#include "types.h"
#include "proto.h"

#if defined(_WIN32)
#include <direct.h>
#include <windows.h>
#include <winsock.h>
#else
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#endif


/*
**  -----------------
**  Private Constants
**  -----------------
*/
#define CwdPathSize      256
#define MaxCardParams    10
#define MaxCmdStkSize    10

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
    char *name;                          /* command name */
    void (*handler)(bool help, char *cmdParams);
    } OpCmd;

typedef struct opCmdStackEntry
    {
    FILE *in;
    FILE *out;
    bool isNetConn;
    char cwd[CwdPathSize];
    } OpCmdStackEntry;

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static void opCreateThread(void);

#if defined(_WIN32)
static void opThread(void *param);
static void opToUnixPath(char *path);

#else
static void *opThread(void *param);

#endif

static void opAcceptConnection(void);
static char *opGetString(char *inStr, char *outStr, int outSize);
static bool opHasInput(FILE *fp);
static bool opIsAbsolutePath(char *path);
static int  opStartListening(int port);

static void opCmdDumpMemory(bool help, char *cmdParams);
static void opCmdDumpCM(int fwa, int count);
static void opCmdDumpEM(int fwa, int count);
static void opCmdDumpPP(int pp, int fwa, int count);
static void opHelpDumpMemory(void);

static void opCmdEnterKeys(bool help, char *cmdParams);
static void opHelpEnterKeys(void);
static void opWaitKeyConsume();

static void opCmdHelp(bool help, char *cmdParams);
static void opHelpHelp(void);

static void opCmdHelpAll(bool help, char *cmdParams);
static void opHelpHelpAll(void);

static void opHelpLoadCards(void);
static int opPrepCards(char *fname, FILE *fcb);

static void opCmdLoadTape(bool help, char *cmdParams);
static void opHelpLoadTape(void);

static void opCmdPrompt(void);

static void opCmdShowState(bool help, char *cmdParams);
static void opCmdShowStateCP(void);
static void opCmdShowStatePP(u32 ppMask);
static void opHelpShowState(void);

static void opCmdShowAll(bool help, char *cmdParams);
static void opHelpShowAll(void);

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

static void opCmdSetKeyWaitInterval(bool help, char *cmdParams);
static void opHelpSetKeyWaitInterval(void);

static void opCmdSetOperatorPort(bool help, char *cmdParams);
static void opHelpSetOperatorPort(void);

static void opCmdShutdown(bool help, char *cmdParams);
static void opHelpShutdown(void);

static void opCmdPause(bool help, char *cmdParams);
static void opHelpPause(void);

static void opCmdShowUnitRecord(bool help, char *cmdParams);
static void opHelpShowUnitRecord(void);

static void opCmdShowDisk(bool help, char *cmdParams);
static void opHelpShowDisk(void);

static void opCmdShowEquipment(bool help, char *cmdParams);
static void opHelpShowEquipment(void);

static void opCmdShowVersion(bool help, char *cmdParams);
static void opHelpShowVersion(void);

static void opDisplayVersion(void);

#ifdef IdleThrottle
static void opCmdIdle(bool help, char *cmdParams);
#endif

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
    "d",                     opCmdDumpMemory,
    "dm",                    opCmdDumpMemory,
    "e",                     opCmdEnterKeys,
    "ek",                    opCmdEnterKeys,
    "lc",                    opCmdLoadCards,
    "lt",                    opCmdLoadTape,
    "rc",                    opCmdRemoveCards,
    "rp",                    opCmdRemovePaper,
    "p",                     opCmdPause,
    "sa",                    opCmdShowAll,
    "sd",                    opCmdShowDisk,
    "se",                    opCmdShowEquipment,
    "ski",                   opCmdSetKeyInterval,
    "skwi",                  opCmdSetKeyWaitInterval,
    "sop",                   opCmdSetOperatorPort,
    "ss",                    opCmdShowState,
    "st",                    opCmdShowTape,
    "sur",                   opCmdShowUnitRecord,
    "sv",                    opCmdShowVersion,
    "ut",                    opCmdUnloadTape,
    "dump_memory",           opCmdDumpMemory,
    "enter_keys",            opCmdEnterKeys,
    "load_cards",            opCmdLoadCards,
    "load_tape",             opCmdLoadTape,
    "remove_cards",          opCmdRemoveCards,
    "remove_paper",          opCmdRemovePaper,
    "show_all",              opCmdShowAll,
    "show_disk",             opCmdShowDisk,
    "show_equipment",        opCmdShowEquipment,
    "set_key_wait_interval", opCmdSetKeyWaitInterval,
    "set_key_interval",      opCmdSetKeyInterval,
    "set_operator_port",     opCmdSetOperatorPort,
    "show_state",            opCmdShowState,
    "show_tape",             opCmdShowTape,
    "show_unitrecord",       opCmdShowUnitRecord,
    "show_version",          opCmdShowVersion,
    "unload_tape",           opCmdUnloadTape,
    "?",                     opCmdHelp,
    "help",                  opCmdHelp,
    "??",                    opCmdHelpAll,
    "help_all",              opCmdHelpAll,
    "shutdown",              opCmdShutdown,
    "pause",                 opCmdPause,
#ifdef IdleThrottle
    "idle",                  opCmdIdle,
#endif
    NULL,                    NULL
    };

static FILE *in;
static FILE *out;

static void            (*opCmdFunction)(bool help, char *cmdParams);
static char            opCmdParams[256];
static OpCmdStackEntry opCmdStack[MaxCmdStkSize];
static int             opCmdStackPtr = 0;
#if defined(_WIN32)
static SOCKET opListenHandle = 0;
#else
static int opListenHandle = 0;
#endif
static int           opListenPort = 0;
static volatile bool opPaused     = FALSE;

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
            opCmdPrompt();
            }

        fflush(out);
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
    DWORD  dwThreadId;
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
        fprintf(stderr, "(operator) Failed to create operator thread\n");
        exit(1);
        }
#else
    int            rc;
    pthread_t      thread;
    pthread_attr_t attr;

    /*
    **  Create POSIX thread with default attributes.
    */
    pthread_attr_init(&attr);
    rc = pthread_create(&thread, &attr, opThread, NULL);
    if (rc < 0)
        {
        fprintf(stderr, "(operator) Failed to create operator thread\n");
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
    char  cmd[256];
    char  *line;
    char  name[80];
    FILE  *newIn;
    char  *params;
    char  path[256];
    char  *pos;
    char  *sp;

    opCmdStack[opCmdStackPtr].in        = in = stdin;
    opCmdStack[opCmdStackPtr].out       = out = stdout;
    opCmdStack[opCmdStackPtr].isNetConn = FALSE;

    opDisplayVersion();

    fputs("\n\n", out);
    fputs("---------------------------\n", out);
    fputs("DTCYBER: Operator interface\n", out);
    fputs("---------------------------\n\n", out);
    fputs("\nPlease enter 'help' to get a list of commands\n", out);
    opCmdPrompt();

    if (getcwd(opCmdStack[opCmdStackPtr].cwd, CwdPathSize) == NULL)
        {
        fputs("    > Failed to get current working directory path\n", stderr);
        exit(1);
        }

#if defined(_WIN32)
    opToUnixPath(opCmdStack[opCmdStackPtr].cwd);
#endif

    if (initOpenOperatorSection() == 1)
        {
        opCmdStackPtr += 1;
        opCmdStack[opCmdStackPtr].in        = NULL;
        opCmdStack[opCmdStackPtr].out       = out;
        opCmdStack[opCmdStackPtr].isNetConn = FALSE;
        strcpy(opCmdStack[opCmdStackPtr].cwd, opCmdStack[opCmdStackPtr - 1].cwd);
        }

    while (emulationActive)
        {
        opAcceptConnection();

        in  = opCmdStack[opCmdStackPtr].in;
        out = opCmdStack[opCmdStackPtr].out;
        fflush(out);

        if (opHasInput(in) == FALSE)
            {
            sleepMsec(10);
            continue;
            }

        /*
        **  Wait for command input.
        */
        if (in == NULL)
            {
            line = initGetNextLine();
            if (line == NULL)
                {
                opCmdStackPtr -= 1;
                continue;
                }
            strcpy(cmd, line);
            }
        else if (fgets(cmd, sizeof(cmd), in) == NULL)
            {
#if defined(_WIN32)
            if (opCmdStack[opCmdStackPtr].isNetConn && (WSAGetLastError() == WSAEWOULDBLOCK))
                {
                continue;
                }
#else
            if (opCmdStack[opCmdStackPtr].isNetConn && (errno == EWOULDBLOCK))
                {
                continue;
                }
#endif
            if (opCmdStackPtr > 0)
                {
                fclose(in);
                if (opCmdStack[opCmdStackPtr].isNetConn)
                    {
                    fclose(out);
                    opStartListening(opListenPort);
                    }
                opCmdStackPtr -= 1;
                if (opCmdStackPtr == 0)
                    {
                    in  = opCmdStack[opCmdStackPtr].in;
                    out = opCmdStack[opCmdStackPtr].out;
                    opCmdPrompt();
                    fflush(out);
                    }
                }
            else
                {
                fputs("\n    > Console closed\n", out);
                emulationActive = FALSE;
                }
            continue;
            }
        else if (strlen(cmd) == 0)
            {
            continue;
            }

        if ((in != stdin) && (opCmdStack[opCmdStackPtr].isNetConn == FALSE))
            {
            fputs(cmd, out);
            fputs("\n", out);
            fflush(out);
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
            **  The main emulation thread is still busy executing the previous command.
            */
            fputs("\n    > Previous request still busy\n\n", out);
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
            opCmdPrompt();
            continue;
            }
        else if (*name == '@')
            {
            if (opCmdStackPtr + 1 >= MaxCmdStkSize)
                {
                fputs("    > Too many nested command scripts\n", out);
                opCmdPrompt();
                continue;
                }
            sp = name + 1;
#if defined(_WIN32)
            opToUnixPath(sp);
#endif
            if (opIsAbsolutePath(sp))
                {
                strcpy(path, sp);
                }
            else
                {
                sprintf(path, "%s/%s", opCmdStack[opCmdStackPtr].cwd, sp);
                }
            newIn = fopen(path, "r");
            if (newIn != NULL)
                {
                opCmdStackPtr += 1;
                opCmdStack[opCmdStackPtr].in        = newIn;
                opCmdStack[opCmdStackPtr].out       = out;
                opCmdStack[opCmdStackPtr].isNetConn = FALSE;
                pos  = strrchr(path, '/');
                *pos = '\0';
                strcpy(opCmdStack[opCmdStackPtr].cwd, path);
                }
            else
                {
                fprintf(out, "    > Failed to open %s\n", path);
                }
            opCmdPrompt();
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
                opActive      = TRUE;
                break;
                }
            }

        if (cp->name == NULL)
            {
            /*
            **  Try to help user.
            */
            fprintf(out, "    > Command not implemented: %s\n\n", name);
            fputs("    > Try 'help' to get a list of commands or 'help <command>'\n", out);
            fputs("    > to get a brief description of a command.\n", out);
            opCmdPrompt();
            continue;
            }
        }

#if !defined(_WIN32)
    return (NULL);
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Determine whether input is available from a file stream
**
**  Parameters:     Name        Description.
**                  fp          file stream pointer
**
**  Returns:        TRUE if input is available.
**
**------------------------------------------------------------------------*/
static bool opHasInput(FILE *fp)
    {
    int            fd;
    fd_set         fds;
    int            n;
    struct timeval timeout;

    if (fp == NULL)
        {
        return TRUE;
        }

#if defined(_WIN32)
    if (fp == stdin && _isatty(_fileno(stdin)))
        {
        return kbhit();
        }
#endif

    timeout.tv_sec  = 0;
    timeout.tv_usec = 0;
    fd = fileno(fp);
    FD_ZERO(&fds);
    FD_SET(fd, &fds);

    n = select(fd + 1, &fds, NULL, NULL, &timeout);
    if (n > 0)
        {
        return TRUE;
        }
    else if (n == 0)
        {
        return FALSE;
        }
    else
        {
#if defined(_WIN32)
        //
        // Windows doesn't support select on pipes, and it will
        // return an error condition.
        //
        return TRUE;
#else
        return FALSE;
#endif
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Accept operator connection.
**
**  Parameters:     Name        Description.
**                  param       Thread parameter (unused)
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void opAcceptConnection(void)
    {
    int                acceptFd;
    fd_set             acceptFds;
    struct sockaddr_in from;
    int                n;
    struct timeval     timeout;

#if defined(_WIN32)
    int fromLen;
#else
    socklen_t fromLen;
#endif

    if (opListenHandle == 0)
        {
        return;
        }

    timeout.tv_sec  = 0;
    timeout.tv_usec = 0;

    FD_ZERO(&acceptFds);
    FD_SET(opListenHandle, &acceptFds);

    n = select(opListenHandle + 1, &acceptFds, NULL, NULL, &timeout);
    if (n <= 0)
        {
        return;
        }
    fromLen  = sizeof(from);
    acceptFd = accept(opListenHandle, (struct sockaddr *)&from, &fromLen);
    if (acceptFd >= 0)
        {
        if (opCmdStackPtr + 1 >= MaxCmdStkSize)
            {
            fputs("    > Too many nested operator input sources\n", out);
#if defined(_WIN32)
            closesocket(acceptFd);
#else
            close(acceptFd);
#endif
            return;
            }
        fputs("\nOperator connection accepted\n", out);
        fflush(out);
        opCmdStackPtr += 1;
#if defined(WIN32)
        in  = fdopen(_open_osfhandle((SOCKET)acceptFd, _O_RDONLY), "r");
        out = fdopen(_open_osfhandle((SOCKET)acceptFd, _O_WRONLY), "w");
        closesocket(opListenHandle);
#else
        in  = fdopen(acceptFd, "r");
        out = fdopen(acceptFd, "w");
        close(opListenHandle);
#endif
        opCmdStack[opCmdStackPtr].in        = in;
        opCmdStack[opCmdStackPtr].out       = out;
        opCmdStack[opCmdStackPtr].isNetConn = TRUE;
        strcpy(opCmdStack[opCmdStackPtr].cwd, opCmdStack[opCmdStackPtr - 1].cwd);
        opListenHandle = 0;
        opCmdPrompt();
        fflush(out);
        }
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

        return (inStr + pos);
        }

    /*
    **  Copy string to output buffer.
    */
    while (inStr[pos] != 0 && !isspace(inStr[pos]))
        {
        if (len >= outSize - 1)
            {
            return (NULL);
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

    return (inStr + pos);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Determine whether a pathname is absolute or relative.
**
**  Parameters:     Name        Description.
**                  path        the pathname to test
**
**  Returns:        TRUE if the pathname is absolute.
**
**------------------------------------------------------------------------*/
static bool opIsAbsolutePath(char *path)
    {
    if (*path == '/') return TRUE;
#if defined(_WIN32)
    if ((*(path + 1) == ':')
        && (   (*path >= 'A' && *path <= 'Z')
            || (*path >= 'a' && *path <= 'z'))) return TRUE;

#endif
    return FALSE;
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
    int  count;
    char *cp;
    int  fwa;
    char *memType;
    int  numParam;
    int  pp;

    /*
    **  Process help request.
    */
    if (help)
        {
        opHelpDumpMemory();

        return;
        }

    cp = memType = cmdParams;
    while (*cp != '\0' && *cp != ',')
        {
        ++cp;
        }
    if (*cp != ',')
        {
        fputs("    > Not enough parameters\n", out);

        return;
        }
    *cp++    = '\0';
    numParam = sscanf(cp, "%o,%d", &fwa, &count);
    if (numParam == 1)
        {
        count = 1;
        }
    else if (numParam != 2)
        {
        fputs("    > Not enough or invalid parameters\n", out);

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
            fputs("    > Missing or invalid PP number\n", out);
            }
        opCmdDumpPP(pp, fwa, count);
        }
    else
        {
        fputs("    > Invalid memory type\n", out);
        }
    }

static void opCmdDumpCM(int fwa, int count)
    {
    char   buf[60];
    char   *cp;
    int    limit;
    int    n;
    int    shiftCount;
    CpWord word;

    if ((fwa < 0) || (count < 0) || (fwa + count > cpuMaxMemory))
        {
        fputs("    > Invalid CM address or count\n", out);

        return;
        }
    for (limit = fwa + count; fwa < limit; fwa++)
        {
        word = cpMem[fwa];
        n    = sprintf(buf, "    > %08o %020lo ", fwa, word);
        cp   = buf + n;
        for (shiftCount = 54; shiftCount >= 0; shiftCount -= 6)
            {
            *cp++ = cdcToAscii[(word >> shiftCount) & 077];
            }
        *cp++ = '\n';
        *cp   = '\0';
        fputs(buf, out);
        }
    }

static void opCmdDumpEM(int fwa, int count)
    {
    char   buf[42];
    char   *cp;
    int    limit;
    int    n;
    int    shiftCount;
    CpWord word;

    if ((fwa < 0) || (count < 0) || (fwa + count > extMaxMemory))
        {
        fputs("    > Invalid EM address or count\n", out);

        return;
        }
    for (limit = fwa + count; fwa < limit; fwa++)
        {
        word = extMem[fwa];
        n    = sprintf(buf, "%08o %020lo ", fwa, word);
        cp   = buf + n;
        for (shiftCount = 54; shiftCount >= 0; shiftCount -= 6)
            {
            *cp++ = cdcToAscii[(word >> shiftCount) & 077];
            }
        *cp++ = '\n';
        *cp   = '\0';
        fputs(buf, out);
        }
    }

static void opCmdDumpPP(int ppNum, int fwa, int count)
    {
    char   buf[20];
    char   *cp;
    int    limit;
    int    n;
    PpSlot *pp;
    PpWord word;

    if ((ppNum >= 020) && (ppNum <= 031))
        {
        ppNum -= 6;
        }
    else if ((ppNum < 0) || (ppNum > 011))
        {
        fputs("    > Invalid PP number\n", out);

        return;
        }
    if (ppNum >= ppuCount)
        {
        fputs("    > Invalid PP number\n", out);

        return;
        }
    if ((fwa < 0) || (count < 0) || (fwa + count > 010000))
        {
        fputs("    > Invalid PP address or count\n", out);

        return;
        }
    pp = &ppu[ppNum];
    for (limit = fwa + count; fwa < limit; fwa++)
        {
        word  = pp->mem[fwa];
        n     = sprintf(buf, "%04o %04o ", fwa, word);
        cp    = buf + n;
        *cp++ = cdcToAscii[(word >> 6) & 077];
        *cp++ = cdcToAscii[word & 077];
        *cp++ = '\n';
        *cp   = '\0';
        fputs(buf, out);
        }
    }

static void opHelpDumpMemory(void)
    {
    fputs("    > 'dump_memory CM,<fwa>,<count>' dump <count> words of central memory starting from octal address <fwa>.\n", out);
    fputs("    > 'dump_memory EM,<fwa>,<count>' dump <count> words of extended memory starting from octal address <fwa>.\n", out);
    fputs("    > 'dump_memory PP<nn>,<fwa>,<count>' dump <count> words of PP nn's memory starting from octal address <fwa>.\n", out);
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
char opKeyIn           = 0;
long opKeyInterval     = 250;
long opKeyWaitInterval = 100;

static void opCmdEnterKeys(bool help, char *cmdParams)
    {
    char      *bp;
    time_t    clock;
    char      *cp;
    char      keybuf[256];
    char      *kp;
    char      *limit;
    long      msec;
    char      timestamp[20];
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
    tmp   = localtime(&clock);
    sprintf(timestamp, "%02d%02d%02d%02d%02d%02d",
            (u8)tmp->tm_year - 100, (u8)tmp->tm_mon + 1, (u8)tmp->tm_mday,
            (u8)tmp->tm_hour, (u8)tmp->tm_min, (u8)tmp->tm_sec);
    cp    = cmdParams;
    bp    = keybuf;
    limit = bp + sizeof(keybuf) - 2;
    while (*cp != '\0' && bp < limit)
        {
        if (*cp == '%')
            {
            kp = ++cp;
            while (*cp != '%' && *cp != '\0')
                {
                cp += 1;
                }
            if (*cp == '%')
                {
                *cp++ = '\0';
                }
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
                fprintf(out, "Unrecognized keyword: %%%s%%\n", kp);
                opCmdPrompt();

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
        fputs("Key sequence is too long\n", out);
        opCmdPrompt();

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
            cp  += 1;
            while (*cp >= '0' && *cp <= '9')
                {
                msec = (msec * 10) + (*cp++ - '0');
                }
            if (*cp != '#')
                {
                cp -= 1;
                }
            sleepMsec(msec);
            break;
            }
        cp += 1;
        sleepMsec(opKeyInterval);
        // 20220609: sjz Removing this - because the above should be "it" // opWaitKeyConsume();
        }
    if (*cp != '!')
        {
        opKeyIn = '\r';
        sleepMsec(opKeyInterval);
        // 20220609: sjz Removing this - because the above should be "it" // opWaitKeyConsume();
        }
    opCmdPrompt();
    }

static void opHelpEnterKeys(void)
    {
    fputs("    > 'enter_keys <key-sequence>' supply a sequence of key entries to the system console .\n", out);
    fputs("    >      Special keys:\n", out);
    fputs("    >        ! - end sequence without sending <enter> key\n", out);
    fputs("    >        ; - send <enter> key within a sequence\n", out);
    fputs("    >        _ - send <blank> key\n", out);
    fputs("    >        ^ - send <backspace> key\n", out);
    fputs("    >        % - keyword delimiter for keywords:\n", out);
    fputs("    >            %year% insert current year\n", out);
    fputs("    >            %mon%  insert current month\n", out);
    fputs("    >            %day%  insert current day\n", out);
    fputs("    >            %hour% insert current hour\n", out);
    fputs("    >            %min%  insert current minute\n", out);
    fputs("    >            %sec%  insert current second\n", out);
    fputs("    >        # - delimiter for milliseconds pause value (e.g., #500#)\n", out);
    }

static void opWaitKeyConsume()
    {
    while (opKeyIn != 0 || ppKeyIn != 0)
        {
        sleepMsec(opKeyWaitInterval);
        }
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
        fputs("    > Missing or invalid parameter\n", out);
        opHelpSetKeyInterval();

        return;
        }
    opKeyInterval = msec;
    }

static void opHelpSetKeyInterval(void)
    {
    fputs("    > 'set_key_interval <millisecs>' set the interval between key entries to the system console.\n", out);
    fprintf(out, "    > [Current Key Interval is %ld msec.]\n", opKeyInterval);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Set interval between console key scans.
**
**  Parameters:     Name        Description.
**                  help        Request only help on this command.
**                  cmdParams   Command parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void opCmdSetKeyWaitInterval(bool help, char *cmdParams)
    {
    int msec;
    int numParam;

    /*
    **  Process help request.
    */
    if (help)
        {
        opHelpSetKeyWaitInterval();

        return;
        }
    numParam = sscanf(cmdParams, "%d", &msec);
    if (numParam != 1)
        {
        fputs("    > Missing or invalid parameter\n", out);
        opHelpSetKeyWaitInterval();

        return;
        }
    opKeyWaitInterval = msec;
    }

static void opHelpSetKeyWaitInterval(void)
    {
    fputs("    > 'set_keywait_interval <millisecs>' set the interval between keyboard scans of the emulated system console.\n", out);
    fprintf(out, "    > [Current Key Wait Interval is %ld msec.]\n", opKeyWaitInterval);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Set TCP port on which to listen for operator connections
**
**  Parameters:     Name        Description.
**                  help        Request only help on this command.
**                  cmdParams   Command parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void opCmdSetOperatorPort(bool help, char *cmdParams)
    {
    int numParam;
    int port;

    /*
    **  Process help request.
    */
    if (help)
        {
        opHelpSetOperatorPort();

        return;
        }
    numParam = sscanf(cmdParams, "%d", &port);
    if (numParam != 1)
        {
        fputs("    > Missing or invalid parameter\n", out);

        return;
        }
    if ((port < 0) || (port > 65535))
        {
        fputs("    > Invalid port number\n", out);

        return;
        }
    if (opListenHandle != 0)
        {
#if defined(_WIN32)
        closesocket(opListenHandle);
#else
        close(opListenHandle);
#endif
        opListenHandle = 0;
        if (port == 0)
            {
            fputs("    > Operator port closed\n", out);
            }
        }
    if (port == 0)
        {
        return;
        }

    if (opStartListening(port))
        {
        opListenPort = port;
        fprintf(out, "    > Listening for operator connections on port %d\n", port);
        }
    }

static void opHelpSetOperatorPort(void)
    {
    fputs("    > 'set_operator_port <port>' set the TCP port on which to listen for operator connections.\n", out);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Start listening for operator connections
**
**  Parameters:     Name        Description.
**                  port        TCP port number on which to listen
**
**  Returns:        TRUE  if success
**                  FALSE if failure
**
**------------------------------------------------------------------------*/
static int opStartListening(int port)
    {
#if defined(_WIN32)
    u_long blockEnable = 1;
#endif
    int                optEnable = 1;
    struct sockaddr_in server;

    /*
    **  Create TCP socket and bind to specified port.
    */
    opListenHandle = socket(AF_INET, SOCK_STREAM, 0);
    if (opListenHandle < 0)
        {
        fprintf(out, "    > Failed to create socket for port %d\n", port);
        opListenHandle = 0;

        return FALSE;
        }

    /*
    **  Accept will block if client drops connection attempt between select and accept.
    **  We can't block so make listening socket non-blocking to avoid this condition.
    */
#if defined(_WIN32)
    ioctlsocket(opListenHandle, FIONBIO, &blockEnable);
#else
    fcntl(opListenHandle, F_SETFL, O_NONBLOCK);
#endif

    /*
    **  Bind to configured TCP port number
    */
    setsockopt(opListenHandle, SOL_SOCKET, SO_REUSEADDR, (void *)&optEnable, sizeof(optEnable));
    memset(&server, 0, sizeof(server));
    server.sin_family      = AF_INET;
    server.sin_addr.s_addr = inet_addr("0.0.0.0");
    server.sin_port        = htons(port);

    if (bind(opListenHandle, (struct sockaddr *)&server, sizeof(server)) < 0)
        {
        fprintf(out, "    > Failed to bind to listen socket for port %d\n", port);
#if defined(_WIN32)
        closesocket(opListenHandle);
#else
        close(opListenHandle);
#endif
        opListenHandle = 0;

        return FALSE;
        }

    /*
    **  Start listening for new connections on this TCP port number
    */
    if (listen(opListenHandle, 1) < 0)
        {
        fprintf(out, "    > Failed to listen on port %d\n", port);
#if defined(_WIN32)
        closesocket(opListenHandle);
#else
        close(opListenHandle);
#endif
        opListenHandle = 0;

        return FALSE;
        }

    return TRUE;
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
        fputs("    > No parameters expected\n", out);
        opHelpPause();

        return;
        }

    /*
    **  Process command.
    */
    fputs("    > Emulation paused - hit Enter to resume\n", out);

    /*
    **  Wait for Enter key.
    */
    opPaused = TRUE;
    while (opPaused)
        {
        /* wait for operator thread to clear the flag */
        sleepMsec(500);
        }
    }

static void opHelpPause(void)
    {
    fputs("    > 'pause' suspends emulation to reduce CPU load.\n", out);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Issue a command prompt.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void opCmdPrompt(void)
    {
#if defined(_WIN32)
    SYSTEMTIME dt;

    GetLocalTime(&dt);
    fprintf(out, "\n%02d:%02d:%02d [%s] Operator> ", dt.wHour, dt.wMinute, dt.wSecond, displayName);
#else
    time_t    rawtime;
    struct tm *info;
    char      buffer[80];

    time(&rawtime);

    info = localtime(&rawtime);
    strftime(buffer, 80, "%H:%M:%S", info);
    fprintf(out, "\n%s [%s] Operator> ", buffer, displayName);
#endif
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
        fputs("    > No parameters expected\n", out);
        opHelpShutdown();

        return;
        }

    /*
    **  Process command.
    */
    opActive        = FALSE;
    emulationActive = FALSE;

    fprintf(out, "\nThanks for using %s\nGoodbye for now.\n\n", DtCyberVersion);
    }

static void opHelpShutdown(void)
    {
    fputs("    > 'shutdown' terminates emulation.\n", out);
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
        fputs("\n\n", out);
        fputs("---------------------------\n", out);
        fputs("List of available commands:\n", out);
        fputs("---------------------------\n\n", out);
        for (cp = decode; cp->name != NULL; cp++)
            {
            fprintf(out, "    > %s\n", cp->name);
            }

        fputs("\n    > Try 'help <command>' to get a brief description of a command.\n", out);

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
                fputs("\n", out);
                cp->handler(TRUE, NULL);

                return;
                }
            }

        fprintf(out, "\n    > Command not implemented: %s\n", cmdParams);
        }
    }

static void opHelpHelp(void)
    {
    fputs("    > 'help'       list all available commands.\n", out);
    fputs("    > 'help <cmd>' provide help for <cmd>.\n", out);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Provide command help for ALL commands.
**
**  Parameters:     Name        Description.
**                  help        Request only help on this command.
**                  cmdParams   Command parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void opCmdHelpAll(bool help, char *cmdParams)
    {
    OpCmd *cp;

    /*
    **  Process help request.
    */
    if (help)
        {
        opHelpHelpAll();

        return;
        }

    /*
    **  Display Provide help for each command.
    */
    for (cp = decode; cp->name != NULL; cp++)
        {
        fprintf(out, "\n    > Command: %s\n", cp->name);
        cp->handler(TRUE, NULL);
        }
    }

static void opHelpHelpAll(void)
    {
    fputs("    > '?\?'       provide help for ALL commands.\n", out);
    fputs("    > 'help_all' \n", out);
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
void opCmdLoadCards(bool help, char *cmdParams)
    {
    int         channelNo;
    int         equipmentNo;
    FILE        *fcb;
    char        fname[MaxFSPath];
    char        newDeck[MaxFSPath];
    int         numParam;
    int         rc;
    static int  seqNo = 1;
    struct stat statBuf;

    /*
    **  Process help request.
    */
    if (help)
        {
        opHelpLoadCards();

        return;
        }

    numParam = sscanf(cmdParams, "%o,%o,%s", &channelNo, &equipmentNo, fname);

    /*
    **  Check parameters.
    */
    if (numParam < 3)
        {
        fprintf(out, "    > %i parameters supplied. Expected at least 3.\n", numParam);
        opHelpLoadCards();

        return;
        }

    if ((channelNo < 0) || (channelNo >= MaxChannels))
        {
        fprintf(out, "    > Invalid channel no %02o. (must be 0 to %02o) \n", channelNo, MaxChannels);

        return;
        }

    if ((equipmentNo < 0) || (equipmentNo >= MaxEquipment))
        {
        fprintf(out, "    > Invalid equipment no %02o. (must be 0 to %02o) \n", equipmentNo, MaxEquipment);

        return;
        }

    if (fname[0] == 0)
        {
        fputs("    > Invalid file name\n", out);

        return;
        }

    /*
    **  As long as the name of the file isn't the
    **  special identifier "*" (processed by the card reader as
    **  "retrieve next deck from input directory")
    **  xxxxGetNextDeck moves the file from the input directory
    **  to the output directory if it was specified.  Otherwise
    **  the file remains in the input directory until it is
    **  pre-processed.
    **
    **  After Pre-processing xxxxPostProcess is called to
    **  unlink any file that originates from the input directory.
    **
    **  Calls to xxxxGetNextDeck leave 'fname' unmodified unless
    **  a suitable file is found.
    */

    if (fname[0] == '*')
        {
        cr405GetNextDeck(fname, channelNo, equipmentNo, out, cmdParams);
        }

    if (fname[0] == '*')
        {
        cr3447GetNextDeck(fname, channelNo, equipmentNo, out, cmdParams);
        }

    if (fname[0] == '*')
        {
        fputs("    > No decks available to process.\n", out);

        return;
        }

    /*
    **  Create temporary file for preprocessed card deck
    */
    sprintf(newDeck, "CR_C%02o_E%02o_%05d", channelNo, equipmentNo, seqNo++);
    fcb = fopen(newDeck, "w");
    if (fcb == NULL)
        {
        fprintf(out, "    > Failed to create temporary card deck '%s'\n", newDeck);

        return;
        }

    /*
    **  Preprocess card file
    */
    rc = opPrepCards(fname, fcb);
    fclose(fcb);
    if (rc == -1)
        {
        unlink(newDeck);

        return;
        }

    fprintf(out, "    > Preprocessing for '%s' into submit file '%s' complete.\n", fname, newDeck);

    /*
    **  Do not process any file that results in zero-length submission.
    */
    if (stat(newDeck, &statBuf) != 0)
        {
        fprintf(out, "    > Error learning status of file '%s' (%s)\n", newDeck, strerror(errno));

        return;
        }
    if (statBuf.st_size == 0)
        {
        fprintf(out, "    > Skipping Zero-Length file '%s' (Removing '%s')\n", fname, newDeck);
        unlink(newDeck);

        return;
        }

    /*
    **  If an input directory was specified (but there was no
    **  output directory) then we need to give the card reader
    **  a chance to clean up the dedicated input directory.
    **
    **  This event should trigger the filesystem monitor thread
    **  (if active) and prompt it to load the next deck.
    */
    cr405PostProcess(fname, channelNo, equipmentNo, out, cmdParams);
    cr3447PostProcess(fname, channelNo, equipmentNo, out, cmdParams);

    /*
    **  If an input directory was specified (but there was no
    **  output directory) then we need to give the card reader
    **  a chance to clean up the dedicated input directory.
    */
    cr405LoadCards(newDeck, channelNo, equipmentNo, out, cmdParams);
    cr3447LoadCards(newDeck, channelNo, equipmentNo, out, cmdParams);
    }

static void opHelpLoadCards(void)
    {
    fputs("    > 'load_cards <channel>,<equipment>,<filename>[,<p1>,<p2>,...,<pn>]' load specified card file with optional parameters.\n", out);
    fputs("    >      If <filename> = '*' and the card reader has been configured with dedicated\n", out);
    fputs("    >      input and output directories, the next file is ingested from the input directory\n", out);
    fputs("    >      and 'ejected' to the output directory as it was originally submitted.\n", out);
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
    int  argc;
    int  argi;
    char *argv[MaxCardParams];
    char *cp;
    char dbuf[400];
    char *dfltVal;
    int  dfltValLen;
    char *dp;
    FILE *in;
    char *lastnb;
    char params[400];
    char path[256];
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
    cp   = strchr(str, ',');
    if (cp != NULL)
        {
        *cp++ = '\0';
        while (*cp != '\0')
            {
            if (argc < MaxCardParams)
                {
                argv[argc++] = cp;
                }
            cp = strchr(cp, ',');
            if (cp == NULL)
                {
                break;
                }
            *cp++ = '\0';
            }
        }
#if defined(_WIN32)
    opToUnixPath(str);
#endif

    /*
    **  Open and parse the input file
    */
    if (opIsAbsolutePath(str))
        {
        strcpy(path, str);
        }
    else
        {
        sprintf(path, "%s/%s", opCmdStack[opCmdStackPtr].cwd, str);
        }
    in = fopen(path, "r");
    if (in == NULL)
        {
        fprintf(out, "    > Failed to open %s\n", path);

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
            if ((*sp == '$') && (*(sp + 1) == '{') && isdigit(*(sp + 2)))
                {
                argi       = 0;
                dfltVal    = "";
                dfltValLen = 0;
                cp         = sp + 2;
                while (isdigit(*cp))
                    {
                    argi = (argi * 10) + (*cp++ - '0');
                    }
                if (*cp == ':')
                    {
                    dfltVal = ++cp;
                    while (*cp != '}' && *cp != '\0')
                        {
                        cp += 1;
                        }
                    dfltValLen = cp - dfltVal;
                    }
                if (*cp == '}')
                    {
                    sp    = cp + 1;
                    argi -= 1;
                    if ((argi >= 0) && (argi < argc))
                        {
                        cp = argv[argi];
                        while (*cp != '\0')
                            {
                            *dp++ = *cp++;
                            }
                        }
                    else
                        {
                        while (dfltValLen-- > 0)
                            {
                            *dp++ = *dfltVal++;
                            }
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
            while (isspace(*sp))
                {
                sp += 1;
                }
            if (*sp == '\0')
                {
                fprintf(out, "    > File name missing from ~include in %s\n", path);
                fclose(in);

                return -1;
                }
#if defined(_WIN32)
            opToUnixPath(sp);
#endif
            if (opIsAbsolutePath(sp) == FALSE)
                {
                cp = strrchr(path, '/');
                if (cp != NULL)
                    {
                    *cp = '\0';
                    sprintf(params, "%s/%s", path, sp);
                    *cp = '/';
                    sp  = params;
                    }
                }

            /*
            **  Trim trailing whitespace from pathname and parameters
            */
            lastnb = sp;
            for (cp = sp; *cp != '\0'; cp++)
                {
                if (!isspace(*cp))
                    {
                    lastnb = cp;
                    }
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
        printf("    > No parameters supplied.\n");
        opHelpLoadTape();

        return;
        }

    mt669LoadTape(cmdParams, out);
    mt679LoadTape(cmdParams, out);
    mt362xLoadTape(cmdParams, out);
    }

static void opHelpLoadTape(void)
    {
    fputs("    > 'load_tape <channel>,<equipment>,<unit>,<r|w>,<filename>' load specified tape.\n", out);
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
        fputs("    > No parameters supplied\n", out);
        opHelpUnloadTape();

        return;
        }

    mt669UnloadTape(cmdParams, out);
    mt679UnloadTape(cmdParams, out);
    mt362xUnloadTape(cmdParams, out);
    }

static void opHelpUnloadTape(void)
    {
    fputs("    > 'unload_tape <channel>,<equipment>,<unit>' unload specified tape unit.\n", out);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Show status of PP's and/or CPU
**
**  Parameters:     Name        Description.
**                  help        Request only help on this command.
**                  cmdParams   Command parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void opCmdShowState(bool help, char *cmdParams)
    {
    char *cp;
    bool isCP;
    int  numParam;
    char *param;
    u32  ppMask;
    int  ppNum;

    /*
    **  Process help request.
    */
    if (help)
        {
        opHelpShowState();

        return;
        }

    ppMask = (ppuCount > 10) ? 0xfffff : 0x3ff;
    isCP   = TRUE;
    if (strlen(cmdParams) > 0)
        {
        ppMask = 0;
        isCP   = FALSE;
        cp     = cmdParams;
        while (*cp != '\0')
            {
            param = cp;
            while (*cp != '\0' && *cp != ',')
                {
                ++cp;
                }
            if (*cp == ',')
                {
                *cp++ = '\0';
                }
            if ((strcasecmp(param, "CP") == 0) || (strcasecmp(param, "CPU") == 0))
                {
                isCP = TRUE;
                }
            else if (strncasecmp(param, "PP", 2) == 0)
                {
                numParam = sscanf(param + 2, "%o", &ppNum);
                if (numParam != 1)
                    {
                    fputs("    > Missing or invalid PP number\n", out);

                    return;
                    }
                if ((ppNum >= 0) && (ppNum < 012))
                    {
                    ppMask |= 1 << ppNum;
                    }
                else if ((ppuCount > 10) && (ppNum >= 020) && (ppNum < 032))
                    {
                    ppMask |= 1 << (ppNum - 6);
                    }
                else
                    {
                    fputs("    > Invalid PP number\n", out);

                    return;
                    }
                }
            else
                {
                fputs("    > Invalid element type\n", out);
                }
            }
        }

    if (ppMask != 0)
        {
        opCmdShowStatePP(ppMask);
        }
    if (isCP)
        {
        opCmdShowStateCP();
        }
    }

static void opCmdShowStateCP(void)
    {
    int i;

    i = 0;
    fputs("    >|--------------- CPU ----------------|\n", out);
    fprintf(out, "    > P       %06o  A%d %06o  B%d %06o\n", cpu.regP, i, cpu.regA[i], i, cpu.regB[i]);
    i++;
    fprintf(out, "    > RA    %08o  A%d %06o  B%d %06o\n", cpu.regRaCm, i, cpu.regA[i], i, cpu.regB[i]);
    i++;
    fprintf(out, "    > FL    %08o  A%d %06o  B%d %06o\n", cpu.regFlCm, i, cpu.regA[i], i, cpu.regB[i]);
    i++;
    fprintf(out, "    > EM    %08o  A%d %06o  B%d %06o\n", cpu.exitMode, i, cpu.regA[i], i, cpu.regB[i]);
    i++;
    fprintf(out, "    > RAE   %08o  A%d %06o  B%d %06o\n", cpu.regRaEcs, i, cpu.regA[i], i, cpu.regB[i]);
    i++;
    fprintf(out, "    > FLE %010o  A%d %06o  B%d %06o\n", cpu.regFlEcs, i, cpu.regA[i], i, cpu.regB[i]);
    i++;
    fprintf(out, "    > MA    %08o  A%d %06o  B%d %06o\n", cpu.regMa, i, cpu.regA[i], i, cpu.regB[i]);
    i++;
    fprintf(out, "    >                 A%d %06o  B%d %06o\n\n", i, cpu.regA[i], i, cpu.regB[i]);
    i++;

    for (i = 0; i < 8; i++)
        {
        fprintf(out, "    > X%d  %020lo\n", i, cpu.regX[i]);
        }
    fputs("\n", out);
    }

static void opCmdShowStatePP(u32 ppMask)
    {
    char   buf[20];
    int    col;
    int    i;
    int    len;
    PpSlot *pp;
    int    ppNum;

    ppMask |= (1 << 20); // stopper
    ppNum   = 0;
    while (ppNum < 20)
        {
        //
        // Find next requested PP number
        //
        if (((1 << ppNum) & ppMask) == 0)
            {
            ppNum += 1;
            continue;
            }
        //
        // Display next four requested PP's
        //
        i   = ppNum;
        col = 0;
        fputs("    > ", out);
        while (col < 5)
            {
            fprintf(out, "  PP%02o          ", (i < 10) ? i : i + 6);
            i += 1;
            while (((1 << i) & ppMask) == 0)
                {
                i += 1;
                }
            if (i >= 20)
                {
                break;
                }
            col += 1;
            }
        fputs("\n", out);

        i   = ppNum;
        col = 0;
        fputs("    > ", out);
        while (col < 5)
            {
            pp = &ppu[i++];
            sprintf(buf, "P %04o", pp->regP);
            len = strlen(buf);
            while (len < 16)
                {
                buf[len++] = ' ';
                }
            buf[len] = '\0';
            fputs(buf, out);
            while (((1 << i) & ppMask) == 0)
                {
                i += 1;
                }
            if (i >= 20)
                {
                break;
                }
            col += 1;
            }
        fputs("\n", out);

        i   = ppNum;
        col = 0;
        fputs("    > ", out);
        while (col < 5)
            {
            pp = &ppu[i++];
            sprintf(buf, "A %06o", pp->regA);
            len = strlen(buf);
            while (len < 16)
                {
                buf[len++] = ' ';
                }
            buf[len] = '\0';
            fputs(buf, out);
            while (((1 << i) & ppMask) == 0)
                {
                i += 1;
                }
            if (i >= 20)
                {
                break;
                }
            col += 1;
            }
        fputs("\n", out);

        i   = ppNum;
        col = 0;
        fputs("    > ", out);

        while (col < 5)
            {
            pp = &ppu[i++];
            sprintf(buf, "Q %04o", pp->regQ);
            len = strlen(buf);
            while (len < 16)
                {
                buf[len++] = ' ';
                }
            buf[len] = '\0';
            fputs(buf, out);
            while (((1 << i) & ppMask) == 0)
                {
                i += 1;
                }
            if (i >= 20)
                {
                break;
                }
            col += 1;
            }
        fputs("\n", out);

        if ((features & HasRelocationReg) != 0)
            {
            i   = ppNum;
            col = 0;
            fputs("    > ", out);

            while (col < 5)
                {
                pp = &ppu[i++];
                if ((features & HasRelocationRegShort) != 0)
                    {
                    sprintf(buf, "R %06o", pp->regR);
                    }
                else
                    {
                    sprintf(buf, "R %010o", pp->regR);
                    }
                len = strlen(buf);
                while (len < 16)
                    {
                    buf[len++] = ' ';
                    }
                buf[len] = '\0';
                fputs(buf, out);
                while (((1 << i) & ppMask) == 0)
                    {
                    i += 1;
                    }
                if (i >= 20)
                    {
                    break;
                    }
                col += 1;
                }
            fputs("\n", out);
            }
        fputs("\n", out);
        ppNum = i;
        }
    }

static void opHelpShowState(void)
    {
    fputs("    > 'show_state [pp<n>,...][,cp]' show state of PP's and/or CPU.\n", out);
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
        fputs("    > No parameters expected.\n", out);
        opHelpShowTape();

        return;
        }

    mt669ShowTapeStatus(out);
    mt679ShowTapeStatus(out);
    mt362xShowTapeStatus(out);
    mt5744ShowTapeStatus(out);
    }

static void opHelpShowTape(void)
    {
    fputs("    > 'show_tape' show status of all tape units.\n", out);
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
        fputs("    > Parameters expected\n", out);
        opHelpRemovePaper();

        return;
        }

    lp1612RemovePaper(cmdParams, out);
    lp3000RemovePaper(cmdParams, out);
    }

static void opHelpRemovePaper(void)
    {
    fputs("    > 'remove_paper <channel>,<equipment>[,<filename>]' remove paper from printer.\n", out);
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
        fputs("    > Parameters expected\n", out);
        opHelpRemoveCards();

        return;
        }

    cp3446RemoveCards(cmdParams, out);
    }

static void opHelpRemoveCards(void)
    {
    fputs("    > 'remove_cards <channel>,<equipment>[,<filename>]' remove cards from card puncher.\n", out);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Show Unit-Record Status (Printers and Card Devices)
**
**  Parameters:     Name        Description.
**                  help        Request only help on this command.
**                  cmdParams   Command parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void opHelpShowUnitRecord(void)
    {
    fputs("    > 'show_unitrecord' show status of all print and card devices.\n", out);
    }

static void opCmdShowUnitRecord(bool help, char *cmdParams)
    {
    /*
    **  Process help request.
    */
    if (help)
        {
        opHelpShowUnitRecord();

        return;
        }

    /*
    **  Check parameters and process command.
    */
    if (strlen(cmdParams) != 0)
        {
        fprintf(out, "    > No Parameters Expected.\n");
        opHelpShowUnitRecord();

        return;
        }
    cr3447ShowStatus(out);
    cr405ShowStatus(out);
    cp3446ShowStatus(out);
    lp3000ShowStatus(out);
    lp1612ShowStatus(out);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Show status of all disk units
**
**  Parameters:     Name        Description.
**                  help        Request only help on this command.
**                  cmdParams   Command parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void opCmdShowDisk(bool help, char *cmdParams)
    {
    /*
    **  Process help request.
    */
    if (help)
        {
        opHelpShowDisk();

        return;
        }

    /*
    **  Check parameters and process command.
    */
    if (strlen(cmdParams) != 0)
        {
        fputs("    > No parameters expected\n", out);
        opHelpShowDisk();

        return;
        }

    dd8xxShowDiskStatus(out);
    dd6603ShowDiskStatus(out);
    }

static void opHelpShowDisk(void)
    {
    fputs("    > 'show_disk' show status of all disk units.\n", out);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Show status of all Equipment
**
**  Parameters:     Name        Description.
**                  help        Request only help on this command.
**                  cmdParams   Command parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void opCmdShowEquipment(bool help, char *cmdParams)
    {
    /*
    **  Process help request.
    */
    if (help)
        {
        opHelpShowEquipment();

        return;
        }

    /*
    **  Check parameters and process command.
    */
    if (strlen(cmdParams) != 0)
        {
        fputs("    > No parameters expected\n", out);
        opHelpShowEquipment();

        return;
        }

    channelDisplayContext(out);
    }

static void opHelpShowEquipment(void)
    {
    printf("    > 'show_equipment' show status of all attached equipment.\n");
    }

/*--------------------------------------------------------------------------
**  Purpose:        Show Version of dtCyber
**
**  Parameters:     Name        Description.
**                  help        Request only help on this command.
**                  cmdParams   Command parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void opCmdShowVersion(bool help, char *cmdParams)
    {
    /*
    **  Process help request.
    */
    if (help)
        {
        opHelpShowVersion();

        return;
        }

    /*
    **  Check parameters and process command.
    */
    if (strlen(cmdParams) != 0)
        {
        fputs("    > No parameters expected\n", out);
        opHelpShowVersion();

        return;
        }

    opDisplayVersion();
    }

static void opHelpShowVersion(void)
    {
    fputs("    > 'sv'           show version of dtCyber.\n", out);
    fputs("    > 'show_version'\n", out);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Show All Status
**
**  Parameters:     Name        Description.
**                  help        Request only help on this command.
**                  cmdParams   Command parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void opCmdShowAll(bool help, char *cmdParams)
    {
    /*
    **  Process help request.
    */
    if (help)
        {
        opHelpShowAll();

        return;
        }

    /*
    **  Check parameters and process command.
    */
    if (strlen(cmdParams) != 0)
        {
        fputs("    > No parameters expected\n", out);
        opHelpShowAll();

        return;
        }

    opDisplayVersion();

    opCmdShowEquipment(help, cmdParams);
    opCmdShowDisk(help, cmdParams);
    opCmdShowTape(help, cmdParams);
    opCmdShowUnitRecord(help, cmdParams);
    }

static void opHelpShowAll(void)
    {
    fputs("    > 'sa'       show status of all dtCyber Devices.\n", out);
    fputs("    > 'show_all'\n", out);
    }

#ifdef IdleThrottle

/*--------------------------------------------------------------------------
**  Purpose:        control NOS idle loop throttle.
**
**  Parameters:     Name        Description.
**                  help        Request only help on this command.
**                  cmdParams   Command parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void opCmdIdle(bool help, char *cmdParams)
    {
    int          numParam;
    unsigned int newtrigger;
    unsigned int newsleep;

    if (help)
        {
        fputs("    > NOS Idle Loop Throttle\n", out);
        fputs("    > idle <on|off>                   turn NOS idle loop throttle on/off\n", out);
        fputs("    > idle <num_cycles>,<sleep_time>  set number of cycles before sleep and sleep time\n", out);

        return;
        }

    if (strlen(cmdParams) == 0)
        {
        fprintf(out, "    > Idle loop throttling: %s\n", NOSIdle ? "ON" : "OFF");

#ifdef WIN32
        fprintf(out, "    > Sleep every %u cycles for %u milliseconds.\n", idletrigger, idletime);
#else
        fprintf(out, "usleep every %u cycles for %u usec\n", idletrigger, idletime);
#endif

        return;
        }
    if (strcmp("on", dtStrLwr(cmdParams)) == 0)
        {
        NOSIdle = TRUE;

        return;
        }
    if (strcmp("off", dtStrLwr(cmdParams)) == 0)
        {
        NOSIdle = FALSE;

        return;
        }
    numParam = sscanf(cmdParams, "%u,%u", &newtrigger, &newsleep);

    if (numParam != 2)
        {
        fprintf(out, "    > 2 Parameters Expected - %u Provided\n", numParam);

        return;
        }
    if ((newtrigger < 1) || (newsleep < 1))
        {
        fprintf(out, "    > No Parameters Provided (1 or 2 Expected)\n");

        return;
        }
    idletrigger = (u32)newtrigger;
    idletime    = (u32)newsleep;
    fprintf(out, "    > Sleep will now occur every %u cycles for %u milliseconds.\n", idletrigger, idletime);
    }

#endif

/*--------------------------------------------------------------------------
**  Purpose:        Display the DtCyber Version
**
**  Parameters:     Name        Description.
**                  help        Request only help on this command.
**                  cmdParams   Command parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void opDisplayVersion(void)
    {
    fputs("\n--------------------------------------------------------------------------------", out);
    fprintf(out, "\n     %s", DtCyberVersion);
    fprintf(out, "\n     %s", DtCyberCopyright);
    fprintf(out, "\n     %s", DtCyberLicense);
    fprintf(out, "\n     %s", DtCyberLicenseDetails);
    fputs("\n--------------------------------------------------------------------------------", out);
    fprintf(out, "\n     Build Date: %s", DtCyberBuildDate);
    fputs("\n--------------------------------------------------------------------------------", out);
    fputs("\n", out);
    }

#if defined(_WIN32)
/*--------------------------------------------------------------------------
**  Purpose:        Translate a Windows-style pathname to a Unix-style one
**
**  Parameters:     Name        Description.
**                  path        Windows-style pathname
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void opToUnixPath(char *path)
    {
    char *cp;

    for (cp = path; *cp != '\0'; cp++)
        {
        if (*cp == '\\') *cp = '/';
        }
    }
#endif

/*---------------------------  End Of File  ------------------------------*/
