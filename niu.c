/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003, Tom Hunter, Paul Koning (see license.txt)
**
**  Name: niu.c
**
**  Description:
**      Perform emulation of Plato NIU (terminal controller)
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
#include <ctype.h>
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
#define DEBUG 1

#define NiuLocalStations        32          // range reserved for local stations
#define NiuLocalBufSize         50          // size of local input buffer

/*
**  Function codes.
*/
#define FcNiuOutput             00000
#define FcNiuInput              00040

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
    int         connFd;
    u16         currInput;
    u8          ibytes;      // how many bytes have been assembled into currInput (0..2)
    bool        active;
    } PortParam;

typedef struct localRing
    {
    u8 buf[NiuLocalBufSize];
    int get;
    int put;
    } LocalRing;

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static FcStatus niuInFunc(PpWord funcCode);
static void niuInIo(void);
static void niuInit(void);
static FcStatus niuOutFunc(PpWord funcCode);
static void niuOutIo(void);
static void niuActivate(void);
static void niuDisconnect(void);
static void niuCreateThread(void);
static int niuCheckInput(PortParam *portVector);
#if defined(_WIN32)
static void niuThread(void *param);
#else
static void *niuThread(void *param);
#endif
static void niuWelcome(int stat);
static void niuSendstr(int stat, const char *p);
static void niuSend(int stat, int word);
static char *niuFunc2String(PpWord funcCode);

/*
**  ----------------
**  Public Variables
**  ----------------
*/
u16 platoPort;
u16 platoConns;

/*
**  -----------------
**  Private Variables
**  -----------------
*/
static DevSlot *in;
static DevSlot *out;
static PortParam *portVector;
static int currInPort;
static int lastInPort;
static int obytes;
static u32 currOutput;
static u32 lastFrame;
static bool frameStart;
static LocalRing localInput[NiuLocalStations];
static niuProcessOutput *outputHandler[NiuLocalStations];
#if !defined(_WIN32)
static pthread_t niu_thread;
#endif
#if DEBUG
static FILE *niuLog = NULL;
#define MaxLine 1000
static PpWord lineData[MaxLine];
static int linePos = 0;
#endif

/*
**--------------------------------------------------------------------------
**
**  Public Functions
**
**--------------------------------------------------------------------------
*/

/*--------------------------------------------------------------------------
**  Purpose:        Initialise NIU-IN
**
**  Parameters:     Name        Description.
**                  eqNo        equipment number
**                  unitNo      normally unit number, here abused as input channel.
**                  channelNo   output channel number.
**                  deviceName  optional device file name
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void niuInInit(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName)
    {
    if (in != NULL)
        {
        fputs("Multiple NIUs not supported\n", stderr);
        exit (1);
        }
    
    in = channelAttach(channelNo, eqNo, DtNiu);
    in->activate = niuActivate;
    in->disconnect = niuDisconnect;
    in->func = niuInFunc;
    in->io = niuInIo;

    if (out != NULL) niuInit();

    /*
    **  Print a friendly message.
    */
    printf("NIU initialised with  input channel %o\n", channelNo);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Initialise NIU-OUT
**
**  Parameters:     Name        Description.
**                  eqNo        equipment number
**                  unitNo      normally unit number, here abused as input channel.
**                  channelNo   output channel number.
**                  deviceName  optional device file name
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void niuOutInit(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName)
    {
    if (out != NULL)
        {
        fputs ("Multiple NIUs not supported\n", stderr);
        exit (1);
        }

    out = channelAttach(channelNo, eqNo, DtNiu);
    out->activate = niuActivate;
    out->disconnect = niuDisconnect;
    out->func = niuOutFunc;
    out->io = niuOutIo;

    if (in != NULL) niuInit();

    /*
    **  Print a friendly message.
    */
    printf("NIU initialised with output channel %o\n", channelNo);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Report if NIU is configured
**
**  Parameters:     Name        Description.
**
**  Returns:        TRUE if it is.
**
**------------------------------------------------------------------------*/
bool niuPresent(void)
    {
    return (portVector != NULL);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Process local Plato mode input
**
**  Parameters:     Name        Description.
**                  key         Plato key code for station
**                  stat        Station number
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void niuLocalKey(u16 key, int stat)
    {
    int nextput;
    LocalRing *rp;

    if (stat >= NiuLocalStations)
        {
        fprintf (stderr, "Local station number out of range: %d\n", stat);
        exit (1);
        }
    rp = &localInput[stat];

    nextput = rp->put + 1;
    if (nextput == NiuLocalBufSize)
        nextput = 0;

    if (nextput != rp->get)
        {
        rp->buf[rp->put] = (u8)key;
        rp->put = nextput;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Set handler address for local station output
**
**  Parameters:     Name        Description.
**                  h           Output handler function
**                  stat        Station number
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void niuSetOutputHandler(niuProcessOutput *h, int stat)
    {
    if (stat >= NiuLocalStations)
        {
        fprintf (stderr, "Local station number out of range: %d\n", stat);
        exit (1);
        }
    outputHandler[stat] = h;
    }

/*
**--------------------------------------------------------------------------
**
**  Private Functions
**
**--------------------------------------------------------------------------
*/

/*--------------------------------------------------------------------------
**  Purpose:        Execute function code on NIU input channel.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        FcStatus
**
**------------------------------------------------------------------------*/
static void niuInit(void)
    {
    u8 i;
    PortParam *mp;

    #if DEBUG
    if (niuLog == NULL)
        {
        niuLog = fopen("niuLog.txt", "wt");
        fputs("\nniuLog opened.\n", niuLog);
        }
    #endif

    mp = portVector = calloc(1, sizeof(PortParam) * platoConns);
    if (portVector == NULL)
        {
        fputs("Failed to allocate NIU context block\n", stderr);
        exit(1);
        }

    in->context[0] = portVector;

    /*
    **  Initialise port control blocks.
    */
    for (i = 0; i < platoConns; i++)
        {
        mp->active = FALSE;
        mp->connFd = 0;
        mp->ibytes = 0;
        mp++;
        }

    for (i = 0; i < NiuLocalStations; i++)
        {
        localInput[i].get = localInput[i].put = 0;
        }

    currInPort = -1;
    lastInPort = 0;

    /*
    **  Create the thread which will deal with TCP connections.
    */
    niuCreateThread();

    frameStart = FALSE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Execute function code on NIU input channel.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        FcStatus
**
**------------------------------------------------------------------------*/
static FcStatus niuInFunc(PpWord funcCode)
    {
#if DEBUG
    fprintf(niuLog, "\n%06d PP:%02o CH:%02o f:%04o T:%-25s  >   ",
        traceSequenceNo,
        activePpu->id,
        activeDevice->channel->id,
        funcCode,
        niuFunc2String(funcCode));
#endif

    switch (funcCode)
        {
    default:
#if DEBUG
        fprintf(niuLog, "\nNIU function code %02o", funcCode);
#endif
        return(FcDeclined);

    case FcNiuInput:
        currInPort = -1;
        break;
        }

    activeDevice->fcode = funcCode;
    return(FcAccepted);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Execute function code on NIU output channel.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        FcStatus
**
**------------------------------------------------------------------------*/
static FcStatus niuOutFunc(PpWord funcCode)
    {
    switch (funcCode)
        {
    default:
        return(FcDeclined);

    case FcNiuOutput:
        obytes = 0;
        break;
        }

    activeDevice->fcode = funcCode;
    return(FcAccepted);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Perform I/O on NIU input channel.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void niuInIo(void)
    {
    int port;
    PortParam *mp;
    int in;
    int nextget;
    LocalRing *rp;
    
    if (activeDevice->fcode != FcNiuInput || activeChannel->full)
        return;
    
    if (currInPort < 0)
        {
        // We're at the first of the two-word input sequence; find a
        // port with data.
        port = lastInPort;
        for (;;)
            {
            if (++port >= NiuLocalStations + platoConns)
                port = 0;
            if (port < NiuLocalStations)
                {
                // check for local terminal input
                rp = &localInput[port];
                if (rp->get != rp->put)
                    {
                    currInPort = lastInPort = port;
                    activeChannel->data = 04000 + currInPort;
                    activeChannel->full = TRUE;
                    return;
                    }
                if (port == lastInPort)
                    return;         // No input, leave channel empty
                continue;
                }
            mp = portVector + (port - NiuLocalStations);
            if (mp->active)
                {
                /*
                **  Port with active TCP connection.
                */
                if ((in = niuCheckInput(mp)) >= 0)
                    {
#ifdef DEBUG
                    fprintf(niuLog, "NIU input byte %d %03o\n", mp->ibytes, in);
#endif
                    // Connection has data -- assemble it and see if we have
                    // a complete input word
                    if (mp->ibytes != 0)
                        {
                        if ((in & 0200) == 0)
                            {
                            // Sequence error, drop the byte
                            fprintf(niuLog, "niu input sequence error, second byte %03o, port %d\n",
                                    in, port);
                            continue;
                            }
                        mp->currInput |= (in & 0177);
                        currInPort = lastInPort = port;
                        activeChannel->data = 04000 + currInPort;
                        activeChannel->full = TRUE;
                        return;
                        }
                    else
                        {
                        // first byte of key data, save it for later
                        if ((in & 370) != 0)
                            {
                            // sequence error, drop the byte
                            fprintf(niuLog, "niu input sequence error, first byte %03o, port %d\n",
                                    in, port);
                            continue;
                            }
                        mp->currInput = in << 7;
                        mp->ibytes = 1;
                        }
                    }
                }
            if (port == lastInPort)
                return;         // No input, leave channel empty
            }
        }
    // We have a current port, so we already sent the port number;
    // now send its data.
    if (currInPort < NiuLocalStations)
        {
        // local input, send a keypress format input word
        rp = &localInput[currInPort];
        nextget = rp->get + 1;
        if (nextget == NiuLocalBufSize)
            nextget = 0;
        in = rp->buf[rp->get];
        rp->get = nextget;
        activeChannel->data = in << 1;
        }
    else
        {
        mp = portVector + (currInPort - NiuLocalStations);
        activeChannel->data = mp->currInput << 1;
        mp->ibytes = 0;
        }
    activeChannel->full = TRUE;
    currInPort = -1;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Perform I/O on NIU output channel.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void niuOutIo(void)
    {
    PpWord d;
    int port;
    
    if (activeDevice->fcode != FcNiuOutput || !activeChannel->full)
        return;
    
    /*
    **  Output data.
    */
    d = activeChannel->data;
    if (obytes == 0)
        {
        /* edit in Pauls real timing mod */
        if(frameStart)
            {
            if(rtcClock - lastFrame < (u32)16667)
                {
                return;
                }
            lastFrame = rtcClock;
            }
        frameStart = FALSE;
        // first word of the triple
        activeChannel->full = FALSE;
        #ifdef DEBUG
        if ((d & 06000) != 04000)
            {
            fprintf(niuLog, "NIU output out of sync, first word %04o\n", d);
            return;
            }
        #endif
        currOutput = (d & 01777) << 9;
        obytes = 1;
        return;
        }
    activeChannel->full = FALSE;
    if (obytes == 1)
        {
        // second word of the triple
        #ifdef DEBUG
        if ((d & 06001) != 0)
            {
            fprintf(niuLog, "NIU output out of sync, second word %04o\n", d);
            return;
            }
        #endif
        currOutput |= d >> 1;
        obytes = 2;
        return;
        }
    // Third word of the triple
    #ifdef DEBUG
    if ((d & 04000) != 0)
        {
        fprintf(niuLog, "NIU output out of sync, third word %04o\n", d);
        return;
        }
    #endif
    /* edit in Pauls real timing feature */
    /*
    **  If end of frame bit is set, remember that so the
    **  next output word is recognized as the start of a new frame.
    */
    if ((d & 02000) != 0)
        {    
        frameStart = TRUE;
        }    
    port = (d & 01777);
    
    // Now that we have a complete output triple, discard it
    // if it's for a station number out of range (larger than
    // what we're configured to support) or without an 
    // active TCP connection.
    niuSend (port, currOutput);
    obytes = 0;

    /* edit in pauls real timing feature */
#if 0
    if(frameStart)
        {
        /* Frame just ended -- send pending output words */
        for (port = 0; port < niuStations; port++)
            {
            mp = portVector + port;
            if(mp->currOutput != 0 )
                {
                niuSendWord(IDX2STAT (port), mp->currOutput);
                mp->currOutput = 0;
                }
            }
        }
#endif 
    }

/*--------------------------------------------------------------------------
**  Purpose:        Handle channel activation.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void niuActivate(void)
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
static void niuDisconnect(void)
    {
    }

/*--------------------------------------------------------------------------
**  Purpose:        Create thread which will deal with all TCP
**                  connections.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void niuCreateThread(void)
    {
#if defined(_WIN32)
    static bool firstMux = TRUE;
    WORD versionRequested;
    WSADATA wsaData;
    int err;
    DWORD dwThreadId; 
    HANDLE hThread;

    if (firstMux)
        {
        firstMux = FALSE;

        /*
        **  Select WINSOCK 1.1.
        */ 
        versionRequested = MAKEWORD(1, 1);
 
        err = WSAStartup(versionRequested, &wsaData);
        if (err != 0)
            {
            fprintf(stderr, "\r\nError in WSAStartup: %d\r\n", err);
            exit(1);
            }
        }

    /*
    **  Create TCP thread.
    */
    hThread = CreateThread( 
        NULL,                                       // no security attribute 
        0,                                          // default stack size 
        (LPTHREAD_START_ROUTINE)niuThread, 
        (LPVOID)0,                                  // thread parameter 
        0,                                          // not suspended 
        &dwThreadId);                               // returns thread ID 

    if (hThread == NULL)
        {
        fprintf(stderr, "Failed to create niu thread\n");
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
    rc = pthread_create(&thread, &attr, niuThread, 0);
    if (rc < 0)
        {
        fprintf(stderr, "Failed to create niu thread\n");
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
static void niuThread(void *param)
#else
static void *niuThread(void *param)
#endif
    {
    int listenFd;
    struct sockaddr_in server;
    struct sockaddr_in from;
#if defined(_WIN32)
    int fromLen;
#else
    socklen_t fromLen;
#endif
    PortParam *mp;
    u8 i;
    int reuse = 1;

    /*
    **  Create TCP socket and bind to specified port.
    */
    listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd < 0)
        {
        fputs("niu: Can't create socket\n", stderr);
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
    server.sin_port = htons(platoPort);

    if (bind(listenFd, (struct sockaddr *)&server, sizeof(server)) < 0)
        {
        fputs("niu: Can't bind to socket\n", stderr);
#if defined(_WIN32)
        return;
#else
        return(NULL);
#endif
        }

    if (listen(listenFd, 5) < 0)
        {
        fputs("niu: Can't listen\n", stderr);
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
        mp = portVector;
        for (i = 0; i < platoConns; i++)
            {
            if (!mp->active)
                {
                break;
                }
            mp += 1;
            }

        if (i == platoConns)
            {
            /*
            **  No free port found - wait a bit and try again.
            */
#if defined(_WIN32)
            Sleep(1000);
#else
            //usleep(10000000);
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
#if DEBUG
        printf("NIU: Received connection on port %d\n", (int)(mp - portVector));
#endif
        niuWelcome (mp - portVector + NiuLocalStations);
        }

#if !defined(_WIN32)
    return(NULL);
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Check for input.
**
**  Parameters:     Name        Description.
**                  mp          pointer to PortParam struct for the connection.
**
**  Returns:        data byte received, or -1 if there is no data.
**
**------------------------------------------------------------------------*/
static int niuCheckInput(PortParam *mp)
    {
    int i;
    fd_set readFds;
    fd_set exceptFds;
    struct timeval timeout;
    u8 data;

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
#if DEBUG
            printf("NIU: Connection dropped on port %d\n", (int)(mp - portVector));
#endif
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
#if DEBUG
        printf("NIU: Connection dropped on port %d\n", (int)(mp - portVector));
#endif
        return(-1);
        }
    else
        {
        return(-1);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Send a welcome message to a station
**
**  Parameters:     Name        Description.
**                  stat        Station number
**
**  Returns:        nothing.
**
**------------------------------------------------------------------------*/
static void niuWelcome(int stat)
    {
    char msg[100];
    
    sprintf (msg, "Connected to Plato station %d-%d", stat >> 5, stat & 037);
	niuSend (stat, 0042000 + stat); /* NOP with station number in it */
    niuSend (stat, 0100033);        // mode 3, mode rewrite, screen
    niuSend (stat, 0201200);        // load Y = 128
    niuSend (stat, 0200200);        // load X = 128
    niuSendstr (stat, msg);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Send a string to a station
**
**  Parameters:     Name        Description.
**                  stat        Station number
**                  p           pointer to ASCII string
**
**  Returns:        nothing.
**
**------------------------------------------------------------------------*/
static void niuSendstr(int stat, const char *p)
    {
    int cc = 2;
    int w = 017720;
    bool shift = FALSE;
    char c;
    
    while ((c = *p++) != 0)
        {
        if (isupper (c))
            {
            c = tolower (c);
            if (!shift)
                {
                w = (w << 6 | 077);
                if (++cc == 3)
                    {
                    cc = 0;
                    niuSend (stat, w);
                    w = 1;
                    }
                w = (w << 6 | 021);
                if (++cc == 3)
                    {
                    cc = 0;
                    niuSend (stat, w);
                    w = 1;
                    }
                shift = TRUE;
                }
            }
        else if (shift)
            {
            w = (w << 6 | 077);
            if (++cc == 3)
                {
                cc = 0;
                niuSend (stat, w);
                w = 1;
                }
            w = (w << 6 | 020);
            if (++cc == 3)
                {
                cc = 0;
                niuSend (stat, w);
                w = 1;
                }
            shift = FALSE;
            }
        w = (w << 6 | asciiToCdc[c]);
        if (++cc == 3)
            {
            cc = 0;
            niuSend (stat, w);
            w = 1;
            }
        }
    if (cc > 0)
        {
        while (cc < 3)
            {
            w = (w << 6 | 077);
            cc++;
            }
        niuSend (stat, w);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Send an output word to a station
**
**  Parameters:     Name        Description.
**                  stat        Station number
**                  word        NIU data word
**
**  Returns:        nothing.
**
**------------------------------------------------------------------------*/
static void niuSend(int stat, int word)
    {
    PortParam *mp;
    u8 data[3];

    if (stat < NiuLocalStations)
        {
        if (outputHandler[stat] != NULL)
            {
            (*outputHandler[stat]) (stat, word);
            }
        }
    else
        {
        stat -= NiuLocalStations;
        if (stat >= platoConns)
            return;
        mp = portVector + stat;
        if (!mp->active)
            return;
        data[0] = word >> 12;
        data[1] = ((word >> 6) & 077) | 0200;
        data[2] = (word & 077) | 0300;
#ifdef DEBUG
        fprintf (niuLog, "NIU output %03o %03o %03o\n",
                data[0], data[1], data[2]);
#endif
        send(mp->connFd, data, 3, 0);
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
static char *niuFunc2String(PpWord funcCode)
    {
    static char buf[30];
#if DEBUG
    switch(funcCode)
        {
    default:
        break;
        }
#endif
    sprintf(buf, "UNKNOWN: %04o", funcCode);
    return(buf);
    }

/*---------------------------  End Of File  ------------------------------*/
