/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**
**  Name: mux6676.c
**
**  Description:
**      Perform emulation of CDC 6676 data set controller (terminal mux).
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
#include "const.h"
#include "types.h"
#include "proto.h"
#include <sys/types.h>
#include <memory.h>
#if defined(_WIN32)
#include <winsock.h>
#else
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
/*
**  -----------------
**  Private Constants
**  -----------------
*/

/*
**  Function codes.
*/
#define Fc6676Output            00001
#define Fc6676Status            00002
#define Fc6676Input             00003

#define Fc6676EqMask            07000
#define Fc6676EqShift           9

#define St6676ServiceFailure    00001
#define St6676InputRequired     00002
#define St6676ChannelAReserved  00004

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
typedef struct portParam
    {
    u8          id;
    bool        active;
    int         connFd;
    } PortParam;

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static FcStatus mux6676Func(PpWord funcCode);
static void mux6676Io(void);
static void mux6676Activate(void);
static void mux6676Disconnect(void);
static void mux6676CreateThread(DevSlot *dp);
static int mux6676CheckInput(PortParam *mp);
static bool mux6676InputRequired(void);
#if defined(_WIN32)
static void mux6676Thread(void *param);
#else
static void *mux6676Thread(void *param);
#endif

/*
**  ----------------
**  Public Variables
**  ----------------
*/
u16 mux6676TelnetPort;
u16 mux6676TelnetConns;

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
**  Purpose:        Initialise terminal multiplexer.
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
void mux6676Init(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName)
    {
    DevSlot *dp;
    PortParam *mp;
    u8 i;

    (void)unitNo;
    (void)deviceName;

    dp = channelAttach(channelNo, eqNo, DtMux6676);

    dp->activate = mux6676Activate;
    dp->disconnect = mux6676Disconnect;
    dp->func = mux6676Func;
    dp->io = mux6676Io;

    /*
    **  Only one MUX6676 unit is possible per equipment.
    */
    if (dp->context[0] != NULL)
        {
        fprintf (stderr, "Only one MUX6676 unit is possible per equipment\n");
        exit (1);
        }

    mp = calloc(1, sizeof(PortParam) * mux6676TelnetConns);
    if (mp == NULL)
        {
        fprintf(stderr, "Failed to allocate MUX6676 context block\n");
        exit(1);
        }

    dp->context[0] = mp;

    /*
    **  Initialise port control blocks.
    */
    for (i = 0; i < mux6676TelnetConns; i++)
        {
        mp->active = FALSE;
        mp->connFd = 0;
        mp->id = i;
        mp += 1;
        }

    /*
    **  Create the thread which will deal with TCP connections.
    */
    mux6676CreateThread(dp);

    /*
    **  Print a friendly message.
    */
    printf("MUX6676 initialised on channel %o equipment %o\n", channelNo, eqNo);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Execute function code on 6676 mux.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        FcStatus
**
**------------------------------------------------------------------------*/
static FcStatus mux6676Func(PpWord funcCode)
    {
    u8 eqNo;
    PortParam *mp = (PortParam *)activeDevice->context[0];

    eqNo = (funcCode & Fc6676EqMask) >> Fc6676EqShift;
    if (eqNo != activeDevice->eqNo)
        {
        /*
        **  Equipment not configured.
        */
        return(FcDeclined);
        }

    funcCode &= ~Fc6676EqMask;

    switch (funcCode)
        {
    default:
        return(FcDeclined);

    case Fc6676Output:
    case Fc6676Status:
    case Fc6676Input:
        activeDevice->recordLength = 0;
        break;
        }

    activeDevice->fcode = funcCode;
    return(FcAccepted);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Perform I/O on 6676 mux.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mux6676Io(void)
    {
    PortParam *cp = (PortParam *)activeDevice->context[0];
    PortParam *mp;
    PpWord function;
    u8 portNumber;
    char x;
    int in;

    switch (activeDevice->fcode)
        {
    default:
        break;

    case Fc6676Output:
        if (activeChannel->full)
            {
            /*
            **  Output data.
            */
            activeChannel->full = FALSE;
            portNumber = (u8)activeDevice->recordLength++;
            if (portNumber < mux6676TelnetConns)
                {
                mp = cp + portNumber;
                if (mp->active)
                    {
                    /*
                    **  Port with active TCP connection.
                    */
                    function = activeChannel->data >> 9;
                    switch (function)
                        {
                    case 4:
                        /*
                        **  Send data with parity stripped off.
                        */
                        x = (activeChannel->data >> 1) & 0x7f;
                        send(mp->connFd, &x, 1, 0);
                        break;

                    case 6:
                        /*
                        **  Disconnect.
                        */
                    #if defined(_WIN32)
                        closesocket(mp->connFd);
                    #else
                        close(mp->connFd);
                    #endif
                        mp->active = FALSE;
                        printf("mux6676: Host closed connection on port %d\n", mp->id);
                        break;

                    default:
                        break;
                        }
                    }
                }
            }
        break;
        
    case Fc6676Input:
        if (!activeChannel->full)
            {
            activeChannel->data = 0;
            activeChannel->full = TRUE;
            portNumber = (u8)activeDevice->recordLength++;
            if (portNumber < mux6676TelnetConns)
                {
                mp = cp + portNumber;
                if (mp->active)
                    {
                    /*
                    **  Port with active TCP connection.
                    */
                    activeChannel->data |= 01000;
                    if ((in = mux6676CheckInput(mp)) > 0)
                        {
                        activeChannel->data |= ((in & 0x7F) << 1) | 04000;
                        }
                    }
                }
            }
        break;

    case Fc6676Status:
        activeChannel->data = St6676ChannelAReserved;
        if (mux6676InputRequired())
            {
            activeChannel->data |= St6676InputRequired;
            }

        activeChannel->full = TRUE;
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
static void mux6676Activate(void)
    {
    }

/*--------------------------------------------------------------------------
**  Purpose:        Handle disconnecting of channel.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mux6676Disconnect(void)
    {
    }

/*--------------------------------------------------------------------------
**  Purpose:        Create thread which will deal with all TCP
**                  connections.
**
**  Parameters:     Name        Description.
**                  dp          pointer to device descriptor
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mux6676CreateThread(DevSlot *dp)
    {
#if defined(_WIN32)
    static bool firstMux = TRUE;
    DWORD dwThreadId; 
    HANDLE hThread;

    if (firstMux)
        {
        firstMux = FALSE;
        }

    /*
    **  Create TCP thread.
    */
    hThread = CreateThread( 
        NULL,                                       // no security attribute 
        0,                                          // default stack size 
        (LPTHREAD_START_ROUTINE)mux6676Thread, 
        (LPVOID)dp,                                 // thread parameter 
        0,                                          // not suspended 
        &dwThreadId);                               // returns thread ID 

    if (hThread == NULL)
        {
        fprintf(stderr, "Failed to create mux6676 thread\n");
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
    rc = pthread_create(&thread, &attr, mux6676Thread, dp);
    if (rc < 0)
        {
        fprintf(stderr, "Failed to create mux6676 thread\n");
        exit(1);
        }
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        TCP thread.
**
**  Parameters:     Name        Description.
**                  mp          pointer to mux parameters.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
#if defined(_WIN32)
static void mux6676Thread(void *param)
#else
static void *mux6676Thread(void *param)
#endif
    {
    DevSlot *dp = (DevSlot *)param;
    int listenFd;
    struct sockaddr_in server;
    struct sockaddr_in from;
    PortParam *mp;
    u8 i;
    int reuse = 1;
#if defined(_WIN32)
    int fromLen;
#else
    socklen_t fromLen;
#endif

    /*
    **  Create TCP socket and bind to specified port.
    */
    listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd < 0)
        {
        printf("mux6676: Can't create socket\n");
#if defined(_WIN32)
        return;
#else
        return(NULL);
#endif
        }

    setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse));
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr("0.0.0.0");
    server.sin_port = htons(mux6676TelnetPort);

    if (bind(listenFd, (struct sockaddr *)&server, sizeof(server)) < 0)
        {
        printf("mux6676: Can't bind to socket\n");
#if defined(_WIN32)
        return;
#else
        return(NULL);
#endif
        }

    if (listen(listenFd, 5) < 0)
        {
        printf("mux6676: Can't listen\n");
#if defined(_WIN32)
        return;
#else
        return(NULL);
#endif
        }

    while (1)
        {
        /*
        **  Find a free port control block.
        */
        mp = (PortParam *)dp->context[dp->selectedUnit];
        for (i = 0; i < mux6676TelnetConns; i++)
            {
            if (!mp->active)
                {
                break;
                }
            mp += 1;
            }

        if (i == mux6676TelnetConns)
            {
            /*
            **  No free port found - wait a bit and try again.
            */
        #if defined(_WIN32)
            Sleep(1000);
        #else
            sleep(1);
        #endif
            continue;
            }

        /*
        **  Wait for a connection.
        */
        fromLen = sizeof(from);
        mp->connFd = accept(listenFd, (struct sockaddr *)&from, &fromLen);

        /*
        **  Mark connection as active.
        */
        mp->active = TRUE;
        printf("mux6676: Received connection on port %d\n", mp->id);
        }

#if !defined(_WIN32)
    return(NULL);
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Check for input.
**
**  Parameters:     Name        Description.
**                  mp          pointer to mux parameters.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static int mux6676CheckInput(PortParam *mp)
    {
    int i;
    fd_set readFds;
    fd_set exceptFds;
    struct timeval timeout;
    char data;

    FD_ZERO(&readFds);
    FD_ZERO(&exceptFds);
    FD_SET(mp->connFd, &readFds);
    FD_SET(mp->connFd, &exceptFds);

    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    select(mp->connFd + 1, &readFds, NULL, &exceptFds, &timeout);
    if (FD_ISSET(mp->connFd, &readFds))
        {
        i = recv(mp->connFd, &data, 1, 0);
        if (i == 1)
            {
            return(data);
            }
        else
            {
        #if defined(_WIN32)
            closesocket(mp->connFd);
        #else
            close(mp->connFd);
        #endif
            mp->active = FALSE;
            printf("mux6676: Connection dropped on port %d\n", mp->id);
            return(-1);
            }
        }
    else if (FD_ISSET(mp->connFd, &exceptFds))
        {
    #if defined(_WIN32)
        closesocket(mp->connFd);
    #else
        close(mp->connFd);
    #endif
        mp->active = FALSE;
        printf("mux6676: Connection dropped on port %d\n", mp->id);
        return(-1);
        }
    else
        {
        return(0);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Determine if input is required.
**
**  Parameters:     Name        Description.
**
**  Returns:        TRUE if input is required, FALSE otherwise.
**
**------------------------------------------------------------------------*/
static bool mux6676InputRequired(void)
    {
    PortParam *cp = (PortParam *)activeDevice->context[0];
    PortParam *mp;
    int i;
    fd_set readFds;
    fd_set exceptFds;
    struct timeval timeout;
    int numSocks;

    FD_ZERO(&readFds);
    FD_ZERO(&exceptFds);
    for (i = 0; i < mux6676TelnetConns; i++)
        {
        mp = cp + i;
        if (mp->active)
            {
            FD_SET(mp->connFd, &readFds);
            FD_SET(mp->connFd, &exceptFds);
            }
        }

    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    numSocks = select(mp->connFd + 1, &readFds, NULL, &exceptFds, &timeout);

    return(numSocks > 0);
    }

/*---------------------------  End Of File  ------------------------------*/
