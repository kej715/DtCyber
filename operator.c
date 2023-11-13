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
#include <io.h>
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
    int in;
    int out;
#if defined(_WIN32)
    SOCKET netConn;
#else
    int netConn;
#endif
    char cwd[CwdPathSize];
    } OpCmdStackEntry;

typedef struct opNetTypeEntry
    {
    char *name;
    void (*handler)();
    } OpNetTypeEntry;

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
static bool opHasInput();
static bool opIsAbsolutePath(char *path);
static int  opReadLine(char *buf, int size);
static int  opStartListening(int port);

static void opCmdDisconnectConsole(bool help, char *cmdParams);
static void opHelpDisconnectConsole(void);

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

static void opCmdIdle(bool help, char *cmdParams);

static void opHelpLoadCards(void);
static int opPrepCards(char *fname, FILE *fcb);

static void opCmdLoadDisk(bool help, char *cmdParams);
static void opHelpLoadDisk(void);

static void opCmdLoadTape(bool help, char *cmdParams);
static void opHelpLoadTape(void);

static void opCmdPause(bool help, char *cmdParams);
static void opHelpPause(void);

static void opCmdPrompt(void);

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

static void opCmdShowAll(bool help, char *cmdParams);
static void opHelpShowAll(void);

static void opCmdShowDisk(bool help, char *cmdParams);
static void opHelpShowDisk(void);

static void opCmdShowEquipment(bool help, char *cmdParams);
static void opHelpShowEquipment(void);

static void opCmdShowNetwork(bool help, char *cmdParams);
static void opHelpShowNetwork(void);

static void opCmdShowState(bool help, char *cmdParams);
static void opCmdShowStateCP(u8 cpMask);
static void opCmdShowStatePP(u32 ppMask);
static void opHelpShowState(void);

static void opCmdShowTape(bool help, char *cmdParams);
static void opHelpShowTape(void);

static void opCmdShowUnitRecord(bool help, char *cmdParams);
static void opHelpShowUnitRecord(void);

static void opCmdShowVersion(bool help, char *cmdParams);
static void opHelpShowVersion(void);

static void opCmdShutdown(bool help, char *cmdParams);
static void opHelpShutdown(void);

static void opCmdUnloadDisk(bool help, char *cmdParams);
static void opHelpUnloadDisk(void);

static void opCmdUnloadTape(bool help, char *cmdParams);
static void opHelpUnloadTape(void);

static void opDisplayVersion(void);

/*
**  ----------------
**  Public Variables
**  ----------------
*/
volatile bool opActive = FALSE;
volatile bool opPaused = FALSE;

/*
**  -----------------
**  Private Variables
**  -----------------
*/
static OpCmd decode[] =
    {
    "d",                     opCmdDumpMemory,
    "dc",                    opCmdDisconnectConsole,
    "dm",                    opCmdDumpMemory,
    "e",                     opCmdEnterKeys,
    "ek",                    opCmdEnterKeys,
    "lc",                    opCmdLoadCards,
    "ld",                    opCmdLoadDisk,
    "lt",                    opCmdLoadTape,
    "rc",                    opCmdRemoveCards,
    "rp",                    opCmdRemovePaper,
    "p",                     opCmdPause,
    "sa",                    opCmdShowAll,
    "sd",                    opCmdShowDisk,
    "se",                    opCmdShowEquipment,
    "ski",                   opCmdSetKeyInterval,
    "skwi",                  opCmdSetKeyWaitInterval,
    "sn",                    opCmdShowNetwork,
    "sop",                   opCmdSetOperatorPort,
    "ss",                    opCmdShowState,
    "st",                    opCmdShowTape,
    "sur",                   opCmdShowUnitRecord,
    "sv",                    opCmdShowVersion,
    "ud",                    opCmdUnloadDisk,
    "ut",                    opCmdUnloadTape,
    "disconnect_console",    opCmdDisconnectConsole,
    "dump_memory",           opCmdDumpMemory,
    "enter_keys",            opCmdEnterKeys,
    "load_cards",            opCmdLoadCards,
    "load_disk",             opCmdLoadDisk,
    "load_tape",             opCmdLoadTape,
    "remove_cards",          opCmdRemoveCards,
    "remove_paper",          opCmdRemovePaper,
    "set_key_interval",      opCmdSetKeyInterval,
    "set_key_wait_interval", opCmdSetKeyWaitInterval,
    "set_operator_port",     opCmdSetOperatorPort,
    "show_all",              opCmdShowAll,
    "show_disk",             opCmdShowDisk,
    "show_equipment",        opCmdShowEquipment,
    "show_network",          opCmdShowNetwork,
    "show_state",            opCmdShowState,
    "show_tape",             opCmdShowTape,
    "show_unitrecord",       opCmdShowUnitRecord,
    "show_version",          opCmdShowVersion,
    "unload_disk",           opCmdUnloadDisk,
    "unload_tape",           opCmdUnloadTape,
    "?",                     opCmdHelp,
    "help",                  opCmdHelp,
    "??",                    opCmdHelpAll,
    "help_all",              opCmdHelpAll,
    "shutdown",              opCmdShutdown,
    "pause",                 opCmdPause,
    "idle",                  opCmdIdle,
    NULL,                    NULL
    };

static OpNetTypeEntry netTypes[] =
    {
    "cdcnet",  cdcnetShowStatus,
    "console", consoleShowStatus,
    "crs",     csFeiShowStatus,
    "dsa311",  dsa311ShowStatus,
    "msu",     msufrendShowStatus,
    "mux",     mux6676ShowStatus,
    "niu",     niuShowStatus,
    "npu",     npuNetShowStatus,
    "tpm",     tpMuxShowStatus,
    NULL,      NULL
    };

static void            (*opCmdFunction)(bool help, char *cmdParams);
static char            opCmdParams[256];
static OpCmdStackEntry opCmdStack[MaxCmdStkSize];
static int             opCmdStackPtr = 0;
#if defined(_WIN32)
static SOCKET opListenHandle = 0;
#else
static int opListenHandle = 0;
#endif
static int opListenPort = 0;

static char opInBuf[256];
static int  opInIdx  = 0;
static char opOutBuf[MaxFSPath*2+128];
static int  opOutIdx = 0;

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
**  Purpose:        Display a message to the operator interface.
**
**  Parameters:     Name        Description.
**                  msg         the message to display
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void opDisplay(char *msg)
    {
    OpCmdStackEntry *ep;
    int             n;

    ep = &opCmdStack[opCmdStackPtr];
    if (ep->netConn == 0)
        {
        n = write(ep->out, msg, strlen(msg));
        }
    else
        {
#if defined(_WIN32)
        char *cp;
        char *sp;

        sp = cp = msg;
        while (*sp != '\0')
            {
            while (*cp != '\0' && *cp != '\n') cp += 1;
            if (*cp == '\n')
                {
                if (cp > sp) send(ep->netConn, sp, cp - sp, 0);
                send(ep->netConn, "\r\n", 2, 0);
                cp += 1;
                }
            else if (cp > sp)
                {
                send(ep->netConn, sp, cp - sp, 0);
                }
            sp = cp;
            }
#else
        send(ep->netConn, msg, strlen(msg), 0);
#endif
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Reports whether operator input is currently being taken
**                  from the console.
**
**  Parameters:     Name        Description.
**
**  Returns:        TRUE if operator input currently being taken from conole
**
**------------------------------------------------------------------------*/
bool opIsConsoleInput(void)
    {
    OpCmdStackEntry *ep;

    ep = &opCmdStack[opCmdStackPtr];
    return ep->netConn == 0 && ep->in == fileno(stdin);
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
        fputs("(operator) Failed to create operator thread\n", stderr);
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
        fputs("(operator) Failed to create operator thread\n", stderr);
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
    OpCmdStackEntry *ep;
    bool  isNetConnClosed;
    int   len;
    char  name[80];
    int   newIn;
    char  *params;
    char  path[256];
    char  *pos;
    char  *sp;

    ep = &opCmdStack[opCmdStackPtr];
    ep->in      = fileno(stdin);
    ep->out     = fileno(stdout);
    ep->netConn = 0;

    opDisplayVersion();

    opDisplay("\n\n");
    opDisplay("---------------------------\n");
    opDisplay("DTCYBER: Operator interface\n");
    opDisplay("---------------------------\n\n");
    opDisplay("\nPlease enter 'help' to get a list of commands\n");

    if (getcwd(ep->cwd, CwdPathSize) == NULL)
        {
        fputs("    > Failed to get current working directory path\n", stderr);
        exit(1);
        }

#if defined(_WIN32)
    opToUnixPath(ep->cwd);
#endif

    if (initOpenOperatorSection() == 1)
        {
        opCmdStackPtr += 1;
        ep = &opCmdStack[opCmdStackPtr];
        ep->in      = -1;
        ep->out     = opCmdStack[opCmdStackPtr - 1].out;
        ep->netConn = 0;
        strcpy(ep->cwd, opCmdStack[opCmdStackPtr - 1].cwd);
        }

    while (emulationActive)
        {
        ep = &opCmdStack[opCmdStackPtr];

        /*
        **  Wait for command input.
        */
        len = opReadLine(cmd, sizeof(cmd));
        if (!emulationActive) break;
        if (len <= 0)
            {
            if (opCmdStackPtr == 0)
                {
                emulationActive = FALSE;
                opActive        = FALSE;
                break;
                }
            else if (len == 0)
                {
                opCmdStackPtr -= 1;
                continue;
                }
            else
                {
                isNetConnClosed = FALSE;
                opCmdStackPtr  -= 1;
                while (opCmdStackPtr > 0)
                    {
                    ep = &opCmdStack[opCmdStackPtr];
                    if (ep->netConn == 0)
                        {
                        close(ep->in);
                        }
                    else
                        {
                        netCloseConnection(ep->netConn);
                        isNetConnClosed = TRUE;
                        }
                    opCmdStackPtr -= 1;
                    }
                if (isNetConnClosed) opStartListening(opListenPort);
                continue;
                }
            }

        if (ep->netConn == 0 && ep->in != fileno(stdin))
            {
            opDisplay(cmd);
            opDisplay("\n");
            }

        if (opActive)
            {
            /*
            **  The main emulation thread is still busy executing the previous command.
            */
            opDisplay("\n    > Previous request still busy\n\n");
            continue;
            }

        /*
        **  Extract the command name.
        */
        params = opGetString(cmd, name, sizeof(name));
        if (*name == 0)
            {
            continue;
            }
        else if (*name == '@')
            {
            if (opCmdStackPtr + 1 >= MaxCmdStkSize)
                {
                opDisplay("    > Too many nested command scripts\n");
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
                sprintf(path, "%s/%s", ep->cwd, sp);
                }
            newIn = open(path, O_RDONLY);
            if (newIn >= 0)
                {
                opCmdStackPtr += 1;
                ep = &opCmdStack[opCmdStackPtr];
                ep->in      = newIn;
                ep->out     = opCmdStack[opCmdStackPtr - 1].out;
                ep->netConn = 0;
                pos  = strrchr(path, '/');
                *pos = '\0';
                strcpy(ep->cwd, path);
                }
            else
                {
                sprintf(opOutBuf, "    > Failed to open %s\n", path);
                opDisplay(opOutBuf);
                }
            continue;
            }

        /*
        **  Find the command handler.
        */
        for (cp = decode; cp->name != NULL; cp++)
            {
            if (strcasecmp(cp->name, name) == 0)
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
            sprintf(opOutBuf, "    > Command not implemented: %s\n\n", name);
            opDisplay(opOutBuf);
            opDisplay("    > Try 'help' to get a list of commands or 'help <command>'\n");
            opDisplay("    > to get a brief description of a command.\n");
            continue;
            }
        }

#if !defined(_WIN32)
    return (NULL);
#endif
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

    if (opCmdStackPtr != 0
        && opCmdStack[opCmdStackPtr].netConn == 0
        && opCmdStack[opCmdStackPtr].in != -1) return;

    GetLocalTime(&dt);
    sprintf(opOutBuf, "\n%02d:%02d:%02d [%s] Operator> ", dt.wHour, dt.wMinute, dt.wSecond, displayName);
#else
    time_t    rawtime;
    struct tm *info;
    char      buffer[80];

    if (opCmdStackPtr != 0
        && opCmdStack[opCmdStackPtr].netConn == 0
        && opCmdStack[opCmdStackPtr].in != -1) return;

    time(&rawtime);
    info = localtime(&rawtime);
    strftime(buffer, 80, "%H:%M:%S", info);
    sprintf(opOutBuf, "\n%s [%s] Operator> ", buffer, displayName);
#endif
    opDisplay(opOutBuf);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Read a line from the operator interface.
**
**  Parameters:     Name        Description.
**                  buf         buffer in which to place the line read
**                  size        size of buf
**
**  Returns:        Number of bytes read.
**                  0 if end of input
**                 -1 if error
**
**------------------------------------------------------------------------*/
static int opReadLine(char *buf, int size)
    {
    char *bp;
    char c;
    OpCmdStackEntry *ep;
    int  err;
    char *limit;
    char *line;
    int  lineNo;
    int  n;

    while (opActive)
        {
        sleepMsec(10);
        }
    if (!emulationActive) return 0;

    bp = buf;
    limit = buf + (size - 2);
    opCmdPrompt();
    while (TRUE)
        {
        if (!emulationActive) return 0;
        if (opOutIdx >= opInIdx)
            {
            if (!opHasInput())
                {
                sleepMsec(10);
                continue;
                }
            ep = &opCmdStack[opCmdStackPtr];
            opOutIdx = opInIdx = 0;
            if (ep->netConn != 0)
                {
                n = recv(ep->netConn, opInBuf, sizeof(opInBuf), 0);
                }
            else if (ep->in == -1)
                {
                line = initGetNextLine(&lineNo);
                if (line == NULL)
                    {
                    n = 0;
                    }
                else
                    {
                    strcpy(opInBuf, line);
                    n = strlen(line);
                    if (n == 0) continue;
                    opInBuf[n++] = '\n';
                    }
                }
            else
                {
                n = read(ep->in, opInBuf, sizeof(opInBuf));
                }
            if (n == -1)
                {
                if (ep->netConn != 0)
                    {
#if defined(_WIN32)
                    err = WSAGetLastError();
                    if (err == WSAEWOULDBLOCK)
                        {
                        sleepMsec(10);
                        continue;
                        }
                    fprintf(stderr, "Unexpected error while reading operator input: %d\n", err);
#else
                    if (errno == EWOULDBLOCK)
                        {
                        sleepMsec(10);
                        continue;
                        }
                    perror("Unexpected error while reading operator input");
#endif
                    netCloseConnection(ep->netConn);
                    opStartListening(opListenPort);
                    }
                return -1;
                }
            else if (n == 0)
                {
                if (bp > buf)
                    {
                    *bp = '\0';
                    return bp - buf;
                    }
                if (ep->netConn != 0)
                    {
                    netCloseConnection(ep->netConn);
                    opStartListening(opListenPort);
                    }
                else if (ep->in != -1)
                    {
                    close(ep->in);
                    }
                return 0;
                }
            opInIdx = n;
            }
        while (opOutIdx < opInIdx)
            {
            c = opInBuf[opOutIdx++];
            if (c == '\n')
                {
                opPaused = FALSE;
                if (bp > buf && *(bp - 1) == '\r') bp -= 1;
                *bp = '\0';
                if (bp > buf)
                    {
                    return bp - buf;
                    }
                else
                    {
                    opCmdPrompt();
                    break;
                    }
                }
            if (bp < limit) *bp++ = c;
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Determine whether input is available from the operator
**
**  Parameters:     Name        Description.
**
**  Returns:        TRUE if input is available.
**
**------------------------------------------------------------------------*/
static bool opHasInput()
    {
    OpCmdStackEntry *ep;
    int             fd;
    fd_set          fds;
    int             n;
    struct timeval  timeout;

    ep = &opCmdStack[opCmdStackPtr];
    if (ep->netConn == 0)
        {
        if (ep->in == -1) return TRUE;
#if defined(_WIN32)
        if (ep->in == _fileno(stdin) && _isatty(_fileno(stdin)))
            {
            if (kbhit())
                {
                return TRUE;
                }
            else
                {
                opAcceptConnection();
                return FALSE;
                }
            }
        else
            {
            opAcceptConnection();
            return TRUE;
            }
#else
        fd = ep->in;
#endif
        }
    else
        {
        fd = ep->netConn;
        }

    timeout.tv_sec  = 0;
    timeout.tv_usec = 0;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);

    n = select(fd + 1, &fds, NULL, NULL, &timeout);
    if (n > 0)
        {
        return TRUE;
        }
    else
        {
        opAcceptConnection();
        return FALSE;
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
    int                optEnable = 1;
    struct timeval     timeout;

#if defined(_WIN32)
    int fromLen;
    u_long blockEnable = 1;
#else
    socklen_t fromLen;
#endif

    if (opListenHandle == 0
        || opCmdStackPtr > 0
        || opCmdStack[opCmdStackPtr].netConn != 0)
        {
        return;
        }

    timeout.tv_sec  = 0;
    timeout.tv_usec = 0;

    FD_ZERO(&acceptFds);
    FD_SET(opListenHandle, &acceptFds);

    n = select(opListenHandle + 1, &acceptFds, NULL, NULL, &timeout);
    if (n <= 0 || !FD_ISSET(opListenHandle, &acceptFds))
        {
        return;
        }
    fromLen  = sizeof(from);
    acceptFd = accept(opListenHandle, (struct sockaddr *)&from, &fromLen);
    if (acceptFd >= 0)
        {
        if (opCmdStackPtr + 1 >= MaxCmdStkSize)
            {
            opDisplay("    > Too many nested operator input sources\n");
            netCloseConnection(acceptFd);
            return;
            }
        setsockopt(acceptFd, SOL_SOCKET, SO_KEEPALIVE, (void*)&optEnable, sizeof(optEnable));
#if defined(_WIN32)
        ioctlsocket(acceptFd, FIONBIO, &blockEnable);
#else
        fcntl(acceptFd, F_SETFL, O_NONBLOCK);
#endif
        opDisplay("\nOperator connection accepted\n");
        opCmdStackPtr += 1;
        netCloseConnection(opListenHandle);
        opListenHandle                    = 0;
        opCmdStack[opCmdStackPtr].netConn = acceptFd;
        opCmdStack[opCmdStackPtr].in      = 0;
        opCmdStack[opCmdStackPtr].out     = 0;
        strcpy(opCmdStack[opCmdStackPtr].cwd, opCmdStack[opCmdStackPtr - 1].cwd);
        opCmdPrompt();
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
**  Purpose:        Disconnect Remote Console
**
**  Parameters:     Name        Description.
**                  help        Request only help on this command.
**                  cmdParams   Command parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void opCmdDisconnectConsole(bool help, char *cmdParams)
    {
    /*
    **  Process help request.
    */
    if (help)
        {
        opHelpDisconnectConsole();

        return;
        }

    /*
    **  Check parameters and process command.
    */
    if (strlen(cmdParams) != 0)
        {
        opDisplay("    > No parameters expected\n");
        opHelpDisconnectConsole();

        return;
        }

    consoleCloseRemote();
    }

static void opHelpDisconnectConsole(void)
    {
    opDisplay("    > 'disconnect_console' disconnect a remote console and return control to local console.\n");
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
        opDisplay("    > Not enough parameters\n");

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
        opDisplay("    > Not enough or invalid parameters\n");

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
            opDisplay("    > Missing or invalid PP number\n");
            }
        opCmdDumpPP(pp, fwa, count);
        }
    else
        {
        opDisplay("    > Invalid memory type\n");
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
        opDisplay("    > Invalid CM address or count\n");

        return;
        }
    for (limit = fwa + count; fwa < limit; fwa++)
        {
        word = cpMem[fwa];
        n    = sprintf(buf, "    > %08o " FMT60_020o " ", fwa, word);
        cp   = buf + n;
        for (shiftCount = 54; shiftCount >= 0; shiftCount -= 6)
            {
            *cp++ = cdcToAscii[(word >> shiftCount) & 077];
            }
        *cp++ = '\n';
        *cp   = '\0';
        opDisplay(buf);
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
        opDisplay("    > Invalid EM address or count\n");

        return;
        }
    for (limit = fwa + count; fwa < limit; fwa++)
        {
        word = extMem[fwa];
        n    = sprintf(buf, "%08o " FMT60_020o " ", fwa, word);
        cp   = buf + n;
        for (shiftCount = 54; shiftCount >= 0; shiftCount -= 6)
            {
            *cp++ = cdcToAscii[(word >> shiftCount) & 077];
            }
        *cp++ = '\n';
        *cp   = '\0';
        opDisplay(buf);
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
        opDisplay("    > Invalid PP number\n");

        return;
        }
    if (ppNum >= ppuCount)
        {
        opDisplay("    > Invalid PP number\n");

        return;
        }
    if ((fwa < 0) || (count < 0) || (fwa + count > 010000))
        {
        opDisplay("    > Invalid PP address or count\n");

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
        opDisplay(buf);
        }
    }

static void opHelpDumpMemory(void)
    {
    opDisplay("    > 'dump_memory CM,<fwa>,<count>' dump <count> words of central memory starting from octal address <fwa>.\n");
    opDisplay("    > 'dump_memory EM,<fwa>,<count>' dump <count> words of extended memory starting from octal address <fwa>.\n");
    opDisplay("    > 'dump_memory PP<nn>,<fwa>,<count>' dump <count> words of PP nn's memory starting from octal address <fwa>.\n");
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
                sprintf(opOutBuf, "Unrecognized keyword: %%%s%%\n", kp);
                opDisplay(opOutBuf);
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
        opDisplay("Key sequence is too long\n");
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
        opWaitKeyConsume();
        }
    if (*cp != '!')
        {
        opKeyIn = '\r';
        opWaitKeyConsume();
        }
    }

static void opHelpEnterKeys(void)
    {
    opDisplay("    > 'enter_keys <key-sequence>' supply a sequence of key entries to the system console .\n");
    opDisplay("    >      Special keys:\n");
    opDisplay("    >        ! - end sequence without sending <enter> key\n");
    opDisplay("    >        ; - send <enter> key within a sequence\n");
    opDisplay("    >        _ - send <blank> key\n");
    opDisplay("    >        ^ - send <backspace> key\n");
    opDisplay("    >        % - keyword delimiter for keywords:\n");
    opDisplay("    >            %year% insert current year\n");
    opDisplay("    >            %mon%  insert current month\n");
    opDisplay("    >            %day%  insert current day\n");
    opDisplay("    >            %hour% insert current hour\n");
    opDisplay("    >            %min%  insert current minute\n");
    opDisplay("    >            %sec%  insert current second\n");
    opDisplay("    >        # - delimiter for milliseconds pause value (e.g., #500#)\n");
    }

static void opWaitKeyConsume()
    {
    while (opKeyIn != 0)
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
        opDisplay("    > Missing or invalid parameter\n");
        opHelpSetKeyInterval();

        return;
        }
    opKeyInterval = msec;
    }

static void opHelpSetKeyInterval(void)
    {
    opDisplay("    > 'set_key_interval <millisecs>' set the interval between key entries to the system console.\n");
    sprintf(opOutBuf, "    > [Current key interval is %ld msec.]\n", opKeyInterval);
    opDisplay(opOutBuf);
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
        opDisplay("    > Missing or invalid parameter\n");
        opHelpSetKeyWaitInterval();

        return;
        }
    opKeyWaitInterval = msec;
    }

static void opHelpSetKeyWaitInterval(void)
    {
    opDisplay("    > 'set_keywait_interval <millisecs>' set the interval between keyboard scans of the emulated system console.\n");
    sprintf(opOutBuf, "    > [Current key wait interval is %ld msec.]\n", opKeyWaitInterval);
    opDisplay(opOutBuf);
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
        opDisplay("    > Missing or invalid parameter\n");

        return;
        }
    if ((port < 0) || (port > 65535))
        {
        opDisplay("    > Invalid port number\n");

        return;
        }
    if (opListenHandle != 0)
        {
        netCloseConnection(opListenHandle);
        opListenHandle = 0;
        if (port == 0)
            {
            opDisplay("    > Operator port closed\n");
            }
        }
    if (port != 0)
        {
        if (opStartListening(port))
            {
            opListenPort = port;
            sprintf(opOutBuf, "    > Listening for operator connections on port %d\n", port);
            opDisplay(opOutBuf);
            }
        }
    }

static void opHelpSetOperatorPort(void)
    {
    opDisplay("    > 'set_operator_port <port>' set the TCP port on which to listen for operator connections.\n");
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
    if (port <= 0) return FALSE;

    /*
    **  Start listening for new connections
    */
    opListenHandle = netCreateListener(port);
#if defined(_WIN32)
    if (opListenHandle == INVALID_SOCKET)
#else
    if (opListenHandle == -1)
#endif
        {
        sprintf(opOutBuf, "    > Failed to listen on port %d\n", port);
        opDisplay(opOutBuf);
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
        opDisplay("    > No parameters expected\n");
        opHelpPause();

        return;
        }

    /*
    **  Process command.
    */
    opDisplay("    > Emulation paused - hit Enter to resume\n");

    /*
    **  Wait for Enter key.
    */
    opPaused = TRUE;
    opActive = FALSE;
    while (opPaused)
        {
        /* wait for operator thread to clear the flag */
        sleepMsec(500);
        }
    }

static void opHelpPause(void)
    {
    opDisplay("    > 'pause' suspends emulation to reduce CPU load.\n");
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
        opDisplay("    > No parameters expected\n");
        opHelpShutdown();

        return;
        }

    /*
    **  Process command.
    */
    emulationActive = FALSE;
    opActive        = FALSE;

    sprintf(opOutBuf, "\nThanks for using %s\n", DtCyberVersion);
    opDisplay(opOutBuf);
    }

static void opHelpShutdown(void)
    {
    opDisplay("    > 'shutdown' terminates emulation.\n");
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
        opDisplay("\n\n");
        opDisplay("---------------------------\n");
        opDisplay("List of available commands:\n");
        opDisplay("---------------------------\n\n");
        for (cp = decode; cp->name != NULL; cp++)
            {
            sprintf(opOutBuf, "    > %s\n", cp->name);
            opDisplay(opOutBuf);
            }

        opDisplay("\n    > Try 'help <command>' to get a brief description of a command.\n");

        return;
        }
    else
        {
        /*
        **  Provide help for specified command.
        */
        for (cp = decode; cp->name != NULL; cp++)
            {
            if (strcasecmp(cp->name, cmdParams) == 0)
                {
                opDisplay("\n");
                cp->handler(TRUE, NULL);

                return;
                }
            }

        sprintf(opOutBuf, "\n    > Command not implemented: %s\n", cmdParams);
        opDisplay(opOutBuf);
        }
    }

static void opHelpHelp(void)
    {
    opDisplay("    > 'help'       list all available commands.\n");
    opDisplay("    > 'help <cmd>' provide help for <cmd>.\n");
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
    **  Provide help for each command.
    */
    for (cp = decode; cp->name != NULL; cp++)
        {
        sprintf(opOutBuf, "\n    > Command: %s\n", cp->name);
        opDisplay(opOutBuf);
        cp->handler(TRUE, NULL);
        }
    }

static void opHelpHelpAll(void)
    {
    opDisplay("    > '?\?'       provide help for ALL commands.\n");
    opDisplay("    > 'help_all' \n");
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
        sprintf(opOutBuf, "    > %i parameters supplied. Expected at least 3.\n", numParam);
        opDisplay(opOutBuf);
        opHelpLoadCards();

        return;
        }

    if ((channelNo < 0) || (channelNo >= MaxChannels))
        {
        sprintf(opOutBuf, "    > Invalid channel no %02o. (must be 0 to %02o) \n", channelNo, MaxChannels);
        opDisplay(opOutBuf);
        return;
        }

    if ((equipmentNo < 0) || (equipmentNo >= MaxEquipment))
        {
        sprintf(opOutBuf, "    > Invalid equipment no %02o. (must be 0 to %02o) \n", equipmentNo, MaxEquipment);
        opDisplay(opOutBuf);
        return;
        }

    if (fname[0] == '\0')
        {
        opDisplay("    > Invalid file name\n");

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
    if (strcmp(fname, "*") == 0)
        {
        cr405GetNextDeck(fname, channelNo, equipmentNo, cmdParams);
        }
    if (strcmp(fname, "*") == 0)
        {
        cr3447GetNextDeck(fname, channelNo, equipmentNo, cmdParams);
        }
    if (strcmp(fname, "*") == 0)
        {
        opDisplay("    > No decks available to process.\n");

        return;
        }

    /*
    **  Create temporary file for preprocessed card deck
    */
    sprintf(newDeck, "CR_C%02o_E%02o_%05d", channelNo, equipmentNo, seqNo++);
    fcb = fopen(newDeck, "w");
    if (fcb == NULL)
        {
        sprintf(opOutBuf, "    > Failed to create temporary card deck '%s'\n", newDeck);
        opDisplay(opOutBuf);
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

    sprintf(opOutBuf, "    > Preprocessing for '%s' into submit file '%s' complete.\n", fname, newDeck);
    opDisplay(opOutBuf);

    /*
    **  Do not process any file that results in zero-length submission.
    */
    if (stat(newDeck, &statBuf) != 0)
        {
        sprintf(opOutBuf, "    > Error learning status of file '%s' (%s)\n", newDeck, strerror(errno));
        opDisplay(opOutBuf);
        return;
        }
    if (statBuf.st_size == 0)
        {
        sprintf(opOutBuf, "    > Skipping empty file '%s', and deleting '%s'\n", fname, newDeck);
        opDisplay(opOutBuf);
        unlink(newDeck);

        return;
        }

    cr405PostProcess(fname, channelNo, equipmentNo, cmdParams);
    cr3447PostProcess(fname, channelNo, equipmentNo, cmdParams);

    /*
    **  If an input directory was specified (but there was no
    **  output directory) then we need to give the card reader
    **  a chance to clean up the dedicated input directory.
    */
    cr405LoadCards(newDeck, channelNo, equipmentNo, cmdParams);
    cr3447LoadCards(newDeck, channelNo, equipmentNo, cmdParams);
    }

static void opHelpLoadCards(void)
    {
    opDisplay("    > 'load_cards <channel>,<equipment>,<filename>[,<p1>,<p2>,...,<pn>]' load specified card file with optional parameters.\n");
    opDisplay("    >      If <filename> = '*' and the card reader has been configured with dedicated\n");
    opDisplay("    >      input and output directories, the next file is ingested from the input directory\n");
    opDisplay("    >      and 'ejected' to the output directory as it was originally submitted.\n");
    }

/*--------------------------------------------------------------------------
**  Purpose:        Interpolate a parameter reference into a card image.
**
**  Parameters:     Name        Description.
**                  srcp        Pointer to pointer to parameter reference in source card image
**                  dstp        Pointer to pointer to destination (interpolated) card image
**                  argc        Count of actual parameters
**                  argv        Pointers to actual parameter values
**
**  Returns:        source and destination pointers updated
**
**------------------------------------------------------------------------*/
static void opInterpolateParam(char **srcp, char **dstp, int argc, char *argv[])
    {
    int  argi;
    char *dfltVal;
    int  dfltValLen;
    char *dp;
    char *sp;

    sp         = *srcp + 2;
    dp         = *dstp;
    argi       = 0;
    dfltVal    = "";
    dfltValLen = 0;

    while (isdigit(*sp))
        {
        argi = (argi * 10) + (*sp++ - '0');
        }
    if (*sp == ':')
        {
        dfltVal = ++sp;
        while (*sp != '}' && *sp != '\0')
            {
            sp += 1;
            }
        dfltValLen = sp - dfltVal;
        }
    if (*sp == '}')
        {
        *srcp = sp + 1;
        argi -= 1;
        if ((argi >= 0) && (argi < argc))
            {
            sp = argv[argi];
            while (*sp != '\0')
                {
                *dp++ = *sp++;
                }
            }
        else
            {
            while (dfltValLen-- > 0)
                {
                *dp++ = *dfltVal++;
                }
            }
        }
    else
        {
        *dp++ = '$';
        *dp++ = '{';
        *srcp = *srcp + 2;
        }
    *dstp = dp;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Interpolate a property reference into a card image.
**
**  Parameters:     Name        Description.
**                  srcp        Pointer to pointer to property reference in source card image
**                  dstp        Pointer to pointer to destination (interpolated) card image
**                  srcPath     Pathname of file containing source card image
**
**  Returns:        source and destination pointers updated
**
**------------------------------------------------------------------------*/
static void opInterpolateProp(char **srcp, char **dstp, char *srcPath)
    {
    char *cp;
    char *dfltVal;
    int  dfltValLen;
    char delim;
    char *dp;
    FILE *fp;
    char *pp;
    char propFilePath[256];
    char propFilePath2[300];
    char propName[64];
    char sbuf[400];
    char sectionName[64];
    int  sectionNameLen;
    char *sp;

    sp         = *srcp + 2;
    dp         = *dstp;
    dfltVal    = "";
    dfltValLen = 0;

    pp = propFilePath;
    while (*sp != ':' && *sp != '}' && *sp != '\0')
        {
        *pp++ = *sp++;
        }
    *pp = '\0';

    pp = sectionName;
    if (*sp == ':')
        {
        sp += 1;
        while (*sp != ':' && *sp != '}' && *sp != '\0')
            {
            *pp++ = *sp++;
            }
        }
    *pp = '\0';
    sectionNameLen = pp - sectionName;

    pp = propName;
    if (*sp == ':')
        {
        sp += 1;
        while (*sp != ':' && *sp != '}' && *sp != '\0')
            {
            *pp++ = *sp++;
            }
        }
    *pp = '\0';

    if (*sp == ':')
        {
        sp += 1;
        dfltVal = sp;
        while (*sp != '}' && *sp != '\0') sp += 1;
        dfltValLen = sp - dfltVal;
        }
    
    if (*sp == '}' && propName[0] != '\0' && sectionName[0] != '\0' && propFilePath[0] != '\0')
        {
        *srcp = sp + 1;
#if defined(_WIN32)
        opToUnixPath(propFilePath);
#endif
        pp = propFilePath;
        if (opIsAbsolutePath(propFilePath) == FALSE)
            {
            cp = strrchr(srcPath, '/');
            if (cp != NULL)
                {
                *cp = '\0';
                sprintf(propFilePath2, "%s/%s", srcPath, propFilePath);
                *cp = '/';
                pp = propFilePath2;
                }
            }
        fp = fopen(pp, "r");
        if (fp != NULL)
            {
            //
            //  First, locate the named section in the property file
            //
            sp = NULL;
            while (TRUE)
                {
                sp = fgets(sbuf, sizeof(sbuf), fp);
                if (sp == NULL) break;
                if (*sp == '['
                    && memcmp(sp + 1, sectionName, sectionNameLen) == 0
                    && *(sp + sectionNameLen + 1) == ']') break;
                }
            if (sp != NULL) // named section found
                {
                //
                //  Search section for named property
                //
                while (TRUE)
                    {
                    sp = fgets(sbuf, sizeof(sbuf), fp);
                    if (sp == NULL || *sp == '[') break;
                    cp = strchr(sp, '=');
                    if (cp == NULL) continue;
                    *cp++ = '\0';
                    if (strcmp(propName, sp) == 0)
                        {
                        while (*cp != '\n' && *cp != '\0')
                            {
                            *dp++ = *cp++;
                            }
                        *dstp = dp;
                        fclose(fp);
                        return;
                        }
                    }
                }
            fclose(fp);
            }
        //
        //  Property file not found, or property not found in property file,
        //  so interpolate the default value.
        //
        while (dfltValLen-- > 0)
            {
            *dp++ = *dfltVal++;
            }
        *dstp = dp;
        return;
        }

    *dp++ = '$';
    *dp++ = '{';
    *dstp = dp;
    *srcp = *srcp + 2;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Preprocess a card file
**
**                  The specified source file is read, nested "~include"
**                  directives are detected and processed recursively,
**                  and embedded parameter and property references are
**                  interpolated.
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
    char *argv[MaxCardParams];
    char *cp;
    char dbuf[400];
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
        sprintf(opOutBuf, "    > Failed to open %s\n", path);
        opDisplay(opOutBuf);
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
        **  Scan the source line for parameter and property references and interpolate
        **  any found. A parameter reference has one of the forms:
        **
        **    ${n[:defv]}
        **    ${path:sect:name[:defv]}
        **
        **  where
        **       n  is an integer greater than 0 representing a parameter number
        **    path  is the pathname of a property file
        **    sect  is the name of a section within the property file
        **    name  is a property name
        **    defv  is an optional default value
        */
        dp = dbuf;
        while (*sp != '\0')
            {
            if ((*sp == '$') && (*(sp + 1) == '{'))
                {
                if (isdigit(*(sp + 2)))
                    {
                    opInterpolateParam(&sp, &dp, argc, argv);
                    }
                else
                    {
                    opInterpolateProp(&sp, &dp, path);
                    }
                }
            else
                {
                *dp++ = *sp++;
                }
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
                sprintf(opOutBuf, "    > File name missing from ~include in %s\n", path);
                opDisplay(opOutBuf);
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
**  Purpose:        Load a new disk
**
**  Parameters:     Name        Description.
**                  help        Request only help on this command.
**                  cmdParams   Command parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void opCmdLoadDisk(bool help, char *cmdParams)
    {
    /*
    **  Process help request.
    */
    if (help)
        {
        opHelpLoadDisk();

        return;
        }

    /*
    **  Check parameters and process command.
    */
    if (strlen(cmdParams) == 0)
        {
        opDisplay("    > No parameters supplied.\n");
        opHelpLoadDisk();

        return;
        }

    dd8xxLoadDisk(cmdParams);
    }

static void opHelpLoadDisk(void)
    {
    opDisplay("    > 'load_disk <channel>,<equipment>,<unit>,<filename>' load specified disk.\n");
    }

/*--------------------------------------------------------------------------
**  Purpose:        Unload a mounted disk
**
**  Parameters:     Name        Description.
**                  help        Request only help on this command.
**                  cmdParams   Command parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void opCmdUnloadDisk(bool help, char *cmdParams)
    {
    /*
    **  Process help request.
    */
    if (help)
        {
        opHelpUnloadDisk();

        return;
        }

    /*
    **  Check parameters and process command.
    */
    if (strlen(cmdParams) == 0)
        {
        opDisplay("    > No parameters supplied\n");
        opHelpUnloadDisk();

        return;
        }

    dd8xxUnloadDisk(cmdParams);
    }

static void opHelpUnloadDisk(void)
    {
    opDisplay("    > 'unload_disk <channel>,<equipment>,<unit>' unload specified disk unit.\n");
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
        opDisplay("    > No parameters supplied.\n");
        opHelpLoadTape();

        return;
        }

    mt669LoadTape(cmdParams);
    mt679LoadTape(cmdParams);
    mt362xLoadTape(cmdParams);
    }

static void opHelpLoadTape(void)
    {
    opDisplay("    > 'load_tape <channel>,<equipment>,<unit>,<r|w>,<filename>' load specified tape.\n");
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
        opDisplay("    > No parameters supplied\n");
        opHelpUnloadTape();

        return;
        }

    mt669UnloadTape(cmdParams);
    mt679UnloadTape(cmdParams);
    mt362xUnloadTape(cmdParams);
    }

static void opHelpUnloadTape(void)
    {
    opDisplay("    > 'unload_tape <channel>,<equipment>,<unit>' unload specified tape unit.\n");
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
    u8   cpMask;
    int  cpNum;
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

    cpMask = (cpuCount >  1) ? 0x03 : 0x01;
    ppMask = (ppuCount > 10) ? 0xfffff : 0x3ff;
    if (strlen(cmdParams) > 0)
        {
        cpMask = 0;
        ppMask = 0;
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
            if ((strncasecmp(param, "CP", 2) == 0))
                {
                numParam = sscanf(param + 2, "%d", &cpNum);
                if (numParam != 1)
                    {
                    if (*(param + 2) == '\0')
                        {
                        cpMask = (cpuCount >  1) ? 0x03 : 0x01;
                        }
                    else
                        {
                        opDisplay("    > Missing or invalid CPU number\n");
                        return;
                        }
                    }
                else if ((cpNum >= 0) && (cpNum < cpuCount))
                    {
                    cpMask |= 1 << cpNum;
                    }
                else
                    {
                    opDisplay("    > Invalid CPU number\n");

                    return;
                    }
                }
            else if (strncasecmp(param, "PP", 2) == 0)
                {
                numParam = sscanf(param + 2, "%o", &ppNum);
                if (numParam != 1)
                    {
                    opDisplay("    > Missing or invalid PP number\n");

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
                    opDisplay("    > Invalid PP number\n");

                    return;
                    }
                }
            else
                {
                opDisplay("    > Invalid element type\n");
                }
            }
        }

    if (ppMask != 0)
        {
        opCmdShowStatePP(ppMask);
        }
    if (cpMask != 0)
        {
        opCmdShowStateCP(cpMask);
        }
    }

static void opCmdShowStateCP(u8 cpMask)
    {
    u8 cpNum;
    CpuContext *cpu;
    int i;

    cpMask |= (1 << 2); // stopper
    cpNum   = 0;
    while (cpNum < 2)
        {
        //
        // Find next requested CPU number
        //
        if (((1 << cpNum) & cpMask) == 0)
            {
            cpNum += 1;
            continue;
            }

        if (cpuCount > 1)
            {
            sprintf(opOutBuf, "    > ---------------- CPU%o --------------\n", cpNum);
            opDisplay(opOutBuf);
            }
        else
            {
            opDisplay("    > ---------------- CPU ---------------\n");
            }
        cpu = cpus + cpNum;
        i = 0;
        sprintf(opOutBuf, "    > P       %06o  A%d %06o  B%d %06o\n", cpu->regP, i, cpu->regA[i], i, cpu->regB[i]);
        opDisplay(opOutBuf);
        i++;
        sprintf(opOutBuf, "    > RA    %08o  A%d %06o  B%d %06o\n", cpu->regRaCm, i, cpu->regA[i], i, cpu->regB[i]);
        opDisplay(opOutBuf);
        i++;
        sprintf(opOutBuf, "    > FL    %08o  A%d %06o  B%d %06o\n", cpu->regFlCm, i, cpu->regA[i], i, cpu->regB[i]);
        opDisplay(opOutBuf);
        i++;
        sprintf(opOutBuf, "    > EM    %08o  A%d %06o  B%d %06o\n", cpu->exitMode, i, cpu->regA[i], i, cpu->regB[i]);
        opDisplay(opOutBuf);
        i++;
        sprintf(opOutBuf, "    > RAE   %08o  A%d %06o  B%d %06o\n", cpu->regRaEcs, i, cpu->regA[i], i, cpu->regB[i]);
        opDisplay(opOutBuf);
        i++;
        sprintf(opOutBuf, "    > FLE %010o  A%d %06o  B%d %06o\n", cpu->regFlEcs, i, cpu->regA[i], i, cpu->regB[i]);
        opDisplay(opOutBuf);
        i++;
        sprintf(opOutBuf, "    > MA    %08o  A%d %06o  B%d %06o\n", cpu->regMa, i, cpu->regA[i], i, cpu->regB[i]);
        opDisplay(opOutBuf);
        i++;
        sprintf(opOutBuf, "    > MF           %d  A%d %06o  B%d %06o\n\n", cpu->isMonitorMode, i, cpu->regA[i], i, cpu->regB[i]);
        opDisplay(opOutBuf);
        i++;

        for (i = 0; i < 8; i++)
            {
            sprintf(opOutBuf, "    > X%d  " FMT60_020o "\n", i, cpu->regX[i]);
            opDisplay(opOutBuf);
            }
        opDisplay("\n");
        cpNum += 1;
        }
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
        opDisplay("    > ");
        while (col < 5)
            {
            sprintf(opOutBuf, "  PP%02o          ", (i < 10) ? i : i + 6);
            opDisplay(opOutBuf);
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
        opDisplay("\n");

        i   = ppNum;
        col = 0;
        opDisplay("    > ");
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
            opDisplay(buf);
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
        opDisplay("\n");

        i   = ppNum;
        col = 0;
        opDisplay("    > ");
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
            opDisplay(buf);
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
        opDisplay("\n");

        i   = ppNum;
        col = 0;
        opDisplay("    > ");

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
            opDisplay(buf);
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
        opDisplay("\n");

        if ((features & HasRelocationReg) != 0)
            {
            i   = ppNum;
            col = 0;
            opDisplay("    > ");

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
                opDisplay(buf);
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
            opDisplay("\n");
            }
        opDisplay("\n");
        ppNum = i;
        }
    }

static void opHelpShowState(void)
    {
    opDisplay("    > 'show_state [pp<n>,...][,cp]' show state of PP's and/or CPU.\n");
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
        opDisplay("    > No parameters expected.\n");
        opHelpShowTape();

        return;
        }

    opDisplay("\n    > Magnetic Tape Status:");
    opDisplay("\n    > ---------------------\n");

    mt669ShowTapeStatus();
    mt679ShowTapeStatus();
    mt362xShowTapeStatus();
    mt5744ShowTapeStatus();
    }

static void opHelpShowTape(void)
    {
    opDisplay("    > 'show_tape' show status of all tape units.\n");
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
        opDisplay("    > Parameters expected\n");
        opHelpRemovePaper();

        return;
        }

    lp1612RemovePaper(cmdParams);
    lp3000RemovePaper(cmdParams);
    }

static void opHelpRemovePaper(void)
    {
    opDisplay("    > 'remove_paper <channel>,<equipment>[,<filename>]' remove paper from printer.\n");
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
        opDisplay("    > Parameters expected\n");
        opHelpRemoveCards();

        return;
        }

    cp3446RemoveCards(cmdParams);
    }

static void opHelpRemoveCards(void)
    {
    opDisplay("    > 'remove_cards <channel>,<equipment>[,<filename>]' remove cards from card puncher.\n");
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
    opDisplay("    > 'show_unitrecord' show status of all print and card devices.\n");
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
        sprintf(opOutBuf, "    > No parameters expected.\n");
        opDisplay(opOutBuf);
        opHelpShowUnitRecord();

        return;
        }

    opDisplay("\n    > Unit Record Equipment Status:");
    opDisplay("\n    > -----------------------------\n");

    cr3447ShowStatus();
    cr405ShowStatus();
    cp3446ShowStatus();
    lp3000ShowStatus();
    lp1612ShowStatus();
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
        opDisplay("    > No parameters expected\n");
        opHelpShowDisk();

        return;
        }

    opDisplay("\n    > Disk Drive Status:");
    opDisplay("\n    > ------------------\n");

    dd8xxShowDiskStatus();
    dd885_42ShowDiskStatus();
    dd6603ShowDiskStatus();
    }

static void opHelpShowDisk(void)
    {
    opDisplay("    > 'show_disk' show status of all disk units.\n");
    }

/*--------------------------------------------------------------------------
**  Purpose:        Show status of all equipment
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
        opDisplay("    > No parameters expected\n");
        opHelpShowEquipment();

        return;
        }

    opDisplay("\n    > Channel Context Display:");
    opDisplay("\n    > ------------------------\n");

    channelDisplayContext();
    }

static void opHelpShowEquipment(void)
    {
    opDisplay("    > 'show_equipment' show status of all attached equipment.\n");
    }

/*--------------------------------------------------------------------------
**  Purpose:        Show status of data communication interfaces
**
**  Parameters:     Name        Description.
**                  help        Request only help on this command.
**                  cmdParams   Command parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void opCmdShowNetwork(bool help, char *cmdParams)
    {
    OpNetTypeEntry *entry;
    char *token;

    /*
    **  Process help request.
    */
    if (help)
        {
        opHelpShowNetwork();

        return;
        }

    /*
    **  Check parameters and process command.
    */

    opDisplay("\n    > Data Communication Interface Status:");
    opDisplay("\n    > ------------------------------------\n");

    if (strlen(cmdParams) != 0)
        {
        for (token = strtok(cmdParams, ", "); token != NULL; token = strtok(NULL, ", "))
            {
            for (entry = netTypes; entry->name != NULL && strcasecmp(token, entry->name) != 0; entry++)
                ;
            if (entry->name != NULL)
                {
                if (entry->handler != NULL)
                    (*entry->handler)();
                }
            else
                {
                sprintf(opOutBuf, "    > Unrecognized network type: %s\n", token);
                opDisplay(opOutBuf);
                return;
                }
            }
        }
    else
        {
        for (entry = netTypes; entry->name != NULL; entry++)
            {
            if (entry->handler != NULL)
                (*entry->handler)();
            }
        }
    }

static void opHelpShowNetwork(void)
    {
    OpNetTypeEntry *entry;

    opDisplay("    > 'show_network [<net-type>[,<net-type>...]]' show status of data communication interfaces.\n");
    opDisplay("    >    <net-type> : ");
    for (entry = netTypes; entry->name != NULL; entry++)
        {
        if (entry != netTypes) opDisplay(" | ");
        opDisplay(entry->name);
        }
    opDisplay("\n");
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
        opDisplay("    > No parameters expected\n");
        opHelpShowVersion();

        return;
        }

    opDisplayVersion();
    }

static void opHelpShowVersion(void)
    {
    opDisplay("    > 'sv'           show version of dtCyber.\n");
    opDisplay("    > 'show_version'\n");
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
        opDisplay("    > No parameters expected\n");
        opHelpShowAll();

        return;
        }

    opDisplayVersion();

    opCmdShowEquipment(help, cmdParams);
    opCmdShowDisk(help, cmdParams);
    opCmdShowTape(help, cmdParams);
    opCmdShowUnitRecord(help, cmdParams);
    opCmdShowNetwork(help, cmdParams);
    }

static void opHelpShowAll(void)
    {
    opDisplay("    > 'sa'       show status of all dtCyber Devices.\n");
    opDisplay("    > 'show_all'\n");
    }


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
        opDisplay("    > Idle Loop Throttle\n");
        opDisplay("    > idle <on|off>                   turn NOS idle loop throttle on/off\n");
        opDisplay("    > idle <num_cycles>,<sleep_time>  set number of cycles before sleep and sleep time\n");

        return;
        }

    if (strlen(cmdParams) == 0)
        {
        sprintf(opOutBuf, "    > Idle loop throttling: %s\n", idle ? "ON" : "OFF");
        opDisplay(opOutBuf);
        if (idleDetector == &idleDetectorNone)
            {
            sprintf(opOutBuf, "    > OS handler: None\n");
            opDisplay(opOutBuf);
            }
        else
            {
            sprintf(opOutBuf, "    > OS handler: %s\n", osType);
            opDisplay(opOutBuf);
            }
#ifdef WIN32
        sprintf(opOutBuf, "    > Sleep every %u cycles for %u milliseconds.\n", idleTrigger, idleTime);
        opDisplay(opOutBuf);
#else
        sprintf(opOutBuf, "usleep every %u cycles for %u usec\n", idleTrigger, idleTime);
        opDisplay(opOutBuf);
#endif

        return;
        }
    if (strcasecmp("on", cmdParams) == 0)
        {
        idle = TRUE;

        return;
        }
    if (strcasecmp("off", cmdParams) == 0)
        {
        idle = FALSE;

        return;
        }
    numParam = sscanf(cmdParams, "%u,%u", &newtrigger, &newsleep);

    if (numParam != 2)
        {
        sprintf(opOutBuf, "    > 2 parameters expected - %u provided\n", numParam);
        opDisplay(opOutBuf);
        return;
        }
    if ((newtrigger < 1) || (newsleep < 1))
        {
        sprintf(opOutBuf, "    > No parameters provided (1 or 2 expected)\n");
        opDisplay(opOutBuf);
        return;
        }
    idleTrigger = (u32)newtrigger;
    idleTime    = (u32)newsleep;
    sprintf(opOutBuf, "    > Sleep will now occur every %u cycles for %u milliseconds.\n", idleTrigger, idleTime);
    opDisplay(opOutBuf);
    }


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
    opDisplay("\n--------------------------------------------------------------------------------");
    sprintf(opOutBuf, "\n     %s", DtCyberVersion); opDisplay(opOutBuf);
    sprintf(opOutBuf, "\n     %s", DtCyberCopyright); opDisplay(opOutBuf);
    sprintf(opOutBuf, "\n     %s", DtCyberLicense); opDisplay(opOutBuf);
    sprintf(opOutBuf, "\n     %s", DtCyberLicenseDetails); opDisplay(opOutBuf);
    opDisplay("\n--------------------------------------------------------------------------------");
    sprintf(opOutBuf, "\n     Build date: %s", DtCyberBuildDate); opDisplay(opOutBuf);
    opDisplay("\n--------------------------------------------------------------------------------");
    opDisplay("\n");
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
