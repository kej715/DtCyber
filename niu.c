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

#define DEBUG_PP       0
#define DEBUG_NET      0
#define REAL_TIMING    1

/*
**  -------------
**  Include Files
**  -------------
*/
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
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
#include "const.h"
#include "types.h"
#include "proto.h"

/*
**  -----------------
**  Private Constants
**  -----------------
*/
#define NiuLocalStations    32              // range reserved for local stations
#define NiuLocalBufSize     50              // size of local input buffer

#define IoTurnsPerPoll      4
#define InBufSize           32
#define OutBufSize          256

/*
**  Function codes.
*/
#define FcNiuOutput         00000
#define FcNiuInput          00040

/*
**  -----------------------
**  Private Macro Functions
**  -----------------------
*/
#if DEBUG_PP || DEBUG_NET
#define HexColumn(x)      (3 * (x) + 4)
#define AsciiColumn(x)    (HexColumn(16) + 2 + (x))
#define LogLineLength    (AsciiColumn(16))
#endif

/*
**  -----------------------------------------
**  Private Typedef and Structure Definitions
**  -----------------------------------------
*/
typedef struct portParam
    {
    int  id;
    int  connFd;
    u16  currInput;
    u8   ibytes;             // how many bytes have been assembled into currInput (0..2)
    bool active;
    int  inInIdx;
    int  inOutIdx;
    u8   inBuffer[InBufSize];
    int  outInIdx;
    int  outOutIdx;
    u8   outBuffer[OutBufSize];
    } PortParam;

typedef struct localRing
    {
    u8  buf[NiuLocalBufSize];
    int get;
    int put;
    } LocalRing;

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static void niuClose(PortParam *pp);
static FcStatus niuInFunc(PpWord funcCode);
static void niuInIo(void);
static void niuInit(void);
static void niuLogBytes(u8 *bytes, int len);
static void niuLogFlush(void);
static FcStatus niuOutFunc(PpWord funcCode);
static void niuOutIo(void);
static void niuActivate(void);
static void niuDisconnect(void);
static void niuCheckIo(void);
static void niuWelcome(int stat);
static void niuSend(int stat, int word);
static void niuSendstr(int stat, const char *p);

#if DEBUG_PP || DEBUG_NET
static char *niuFunc2String(PpWord funcCode);

#endif

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
static int              currInPort;
static u32              currOutput;
static DevSlot          *in = NULL;
static int              ioTurns = IoTurnsPerPoll - 1;
static DevSlot          *out = NULL;
static int              lastInPort;
static int              listenFd = 0;
static LocalRing        localInput[NiuLocalStations];
static int              obytes;
static niuProcessOutput *outputHandler[NiuLocalStations];
static PortParam        *portVector;

#if REAL_TIMING
static bool frameStart;
static u32  lastFrame;
#endif

#if DEBUG_PP || DEBUG_NET
static FILE *niuLog = NULL;
static char niuLogBuf[LogLineLength + 1];
static int  niuLogBytesCol = 0;
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
**                  params      optional device parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void niuInInit(u8 eqNo, u8 unitNo, u8 channelNo, char *params)
    {
    int listenPort;
    int numParam;
    int portCount;

    if (in != NULL)
        {
        fputs("Multiple NIUs not supported\n", stderr);
        exit(1);
        }

    in             = channelAttach(channelNo, eqNo, DtNiu);
    in->activate   = niuActivate;
    in->disconnect = niuDisconnect;
    in->func       = niuInFunc;
    in->io         = niuInIo;

    if (params == NULL)
        {
        params = "";
        }
    numParam = sscanf(params, "%d,%d", &listenPort, &portCount);
    if (numParam > 1)
        {
        platoPort  = listenPort;
        platoConns = portCount;
        }
    else if (numParam > 0)
        {
        platoPort = listenPort;
        }
    if ((platoPort < 1) || (platoPort > 65535))
        {
        fprintf(stderr, "(niu    ) Invalid TCP port number in NIU definition: %d\n", platoPort);
        exit(1);
        }
    if (platoConns < 1)
        {
        fprintf(stderr, "(niu    ) Invalid connection count in NIU definition: %d\n", platoConns);
        exit(1);
        }

    if (out != NULL)
        {
        niuInit();
        }

    /*
    **  Print a friendly message.
    */
    printf("(niu    ) Initialised with  input channel %o, max connections %d, TCP port %d\n", channelNo, platoConns, platoPort);
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
        fputs("Multiple NIUs not supported\n", stderr);
        exit(1);
        }

    out             = channelAttach(channelNo, eqNo, DtNiu);
    out->activate   = niuActivate;
    out->disconnect = niuDisconnect;
    out->func       = niuOutFunc;
    out->io         = niuOutIo;

    if (in != NULL)
        {
        niuInit();
        }

    /*
    **  Print a friendly message.
    */
    printf("(niu    ) Initialised with output channel %o\n", channelNo);
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
    int       nextput;
    LocalRing *rp;

    if (stat >= NiuLocalStations)
        {
        fprintf(stderr, "Local station number out of range: %d\n", stat);
        exit(1);
        }
    rp = &localInput[stat];

    nextput = rp->put + 1;
    if (nextput == NiuLocalBufSize)
        {
        nextput = 0;
        }

    if (nextput != rp->get)
        {
        rp->buf[rp->put] = (u8)key;
        rp->put          = nextput;
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
        fprintf(stderr, "Local station number out of range: %d\n", stat);
        exit(1);
        }
    outputHandler[stat] = h;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Show NIU status (operator interface).
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void niuShowStatus()
    {
    int       g;
    int       i;
    char      outBuf[200];
    PortParam *pp;

    if (listenFd > 0)
        {
        sprintf(outBuf, "    >   %-8s C%02o E%02o     ", "NIU", in->channel->id, in->eqNo);
        opDisplay(outBuf);
        sprintf(outBuf, FMTNETSTATUS"\n", netGetLocalTcpAddress(listenFd), "", "plato", "listening");
        opDisplay(outBuf);
        for (i = 0, pp = portVector; i < platoConns; i++, pp++)
            {
            if (pp->active && pp->connFd > 0)
                {
                sprintf(outBuf, "    >   %-8s         P%02o ", "NIU", pp->id);
                opDisplay(outBuf);
                sprintf(outBuf, FMTNETSTATUS"\n", netGetLocalTcpAddress(pp->connFd), netGetPeerTcpAddress(pp->connFd), "plato", "connected");
                opDisplay(outBuf);
                }
            }
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
    u8                 i;
    PortParam          *pp;

#if DEBUG_PP || DEBUG_NET
    if (niuLog == NULL)
        {
        niuLog = fopen("niuLog.txt", "wt");
        fputs("\nniuLog opened.\n", niuLog);
        }
#endif

    pp = portVector = calloc(platoConns, sizeof(PortParam));
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
        pp->id     = i;
        pp->active = FALSE;
        pp->connFd = 0;
        pp->ibytes = 0;
        pp++;
        }

    for (i = 0; i < NiuLocalStations; i++)
        {
        localInput[i].get = localInput[i].put = 0;
        }

    currInPort = -1;
    lastInPort = 0;
    ioTurns    = IoTurnsPerPoll - 1;

    /*
    **  Start listening for new connections on the configured PLATO port number
    */
    listenFd = netCreateListener(platoPort);
#if defined(_WIN32)
    if (listenFd == INVALID_SOCKET)
#else
    if (listenFd == -1)
#endif
        {
        fprintf(stderr, "(niu    ) Can't listen for NIU on port %d\n", platoPort);
        exit(1);
        }

    fprintf(stdout, "(niu    ) Listening on port %d (%d connections permitted).\n", platoPort, platoConns);

#if REAL_TIMING
    frameStart = FALSE;
#endif
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
#if DEBUG_PP
    fprintf(niuLog, "\n%06d PP:%02o CH:%02o f:%04o T:%-8s >   ",
            traceSequenceNo,
            activePpu->id,
            activeDevice->channel->id,
            funcCode,
            niuFunc2String(funcCode));
#endif
    niuCheckIo();
    if (funcCode == FcNiuInput)
        {
        currInPort          = -1;
        activeDevice->fcode = funcCode;

        return (FcAccepted);
        }
    else
        {
        return (FcDeclined);
        }
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
#if DEBUG_PP
    fprintf(niuLog, "\n%06d PP:%02o CH:%02o f:%04o T:%-8s >   ",
            traceSequenceNo,
            activePpu->id,
            activeDevice->channel->id,
            funcCode,
            niuFunc2String(funcCode));
#endif
    niuCheckIo();
    if (funcCode == FcNiuOutput)
        {
        obytes = 0;
        activeDevice->fcode = funcCode;

        return (FcAccepted);
        }
    else
        {
        return (FcDeclined);
        }
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
    int       port;
    int       in;
    int       nextget;
    PortParam *pp;
    LocalRing *rp;

    if ((activeDevice->fcode != FcNiuInput) || activeChannel->full)
        {
        return;
        }

    if (currInPort < 0)
        {
        // We're at the first of the two-word input sequence; find a
        // port with data.
        port = lastInPort;
        for ( ; ;)
            {
            if (++port >= NiuLocalStations + platoConns)
                {
                port = 0;
                }
            if (port < NiuLocalStations)
                {
                // check for local terminal input
                rp = &localInput[port];
                if (rp->get != rp->put)
                    {
                    currInPort          = lastInPort = port;
                    activeChannel->data = 04000 + currInPort;
                    activeChannel->full = TRUE;

                    return;
                    }
                if (port == lastInPort)
                    {
                    return;         // No input, leave channel empty
                    }
                continue;
                }
            pp = portVector + (port - NiuLocalStations);
            if (pp->active && (pp->inOutIdx < pp->inInIdx))
                {
                /*
                **  Port with active TCP connection has data available
                */
                in = pp->inBuffer[pp->inOutIdx++];
                if (pp->inOutIdx >= pp->inInIdx)
                    {
                    pp->inOutIdx = pp->inInIdx = 0;
                    }
#if DEBUG_PP
                fprintf(niuLog, "\n%010u input byte %d %03o on port %d",
                        traceSequenceNo, pp->ibytes, in, pp->id);
#endif
                // Connection has data -- assemble it and see if we have
                // a complete input word
                if (pp->ibytes != 0)
                    {
                    if ((in & 0200) == 0)
                        {
                        // Sequence error, drop the byte
#if DEBUG_PP || DEBUG_NET
                        fprintf(niuLog, "\n%010u input sequence error, second byte %03o, port %d",
                                traceSequenceNo, in, port);
#endif
                        continue;
                        }
                    pp->currInput      |= (in & 0177);
                    currInPort          = lastInPort = port;
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
#if DEBUG_PP || DEBUG_NET
                        fprintf(niuLog, "\n%010u input sequence error, first byte %03o, port %d",
                                traceSequenceNo, in, port);
#endif
                        continue;
                        }
                    pp->currInput = in << 7;
                    pp->ibytes    = 1;
                    }
                }
            if (port == lastInPort)
                {
                return;         // No input, leave channel empty
                }
            }
        }
    // We have a current port, so we already sent the port number;
    // now send its data.
    if (currInPort < NiuLocalStations)
        {
        // local input, send a keypress format input word
        rp      = &localInput[currInPort];
        nextget = rp->get + 1;
        if (nextget == NiuLocalBufSize)
            {
            nextget = 0;
            }
        in                  = rp->buf[rp->get];
        rp->get             = nextget;
        activeChannel->data = in << 1;
        }
    else
        {
        pp = portVector + (currInPort - NiuLocalStations);
        activeChannel->data = pp->currInput << 1;
        pp->ibytes          = 0;
        }
    activeChannel->full = TRUE;
    currInPort          = -1;
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
    int    port;

    if ((activeDevice->fcode != FcNiuOutput) || !activeChannel->full)
        {
        return;
        }

    /*
    **  Output data.
    */
    d = activeChannel->data;
    activeChannel->full = FALSE;

    if (obytes == 0)
        {
        // first word of the triple
#if REAL_TIMING
        if (frameStart)
            {
            if (rtcClock - lastFrame < (u32)16667)
                {
                activeChannel->full = TRUE;

                return;
                }
            lastFrame = rtcClock;
            }
        frameStart = FALSE;
#endif
#if DEBUG_PP
        fprintf(niuLog, "\n%010u output byte %d %04o", traceSequenceNo, obytes, d);
#endif
#if DEBUG_PP || DEBUG_NET
        if ((d & 06000) != 04000)
            {
            fprintf(niuLog, "\n%010u output out of sync, first word %04o",
                    traceSequenceNo, d);
            }
#endif
        currOutput = (d & 01777) << 9;
        obytes     = 1;

        return;
        }

#if DEBUG_PP
    fprintf(niuLog, "\n%010u output byte %d %04o", traceSequenceNo, obytes, d);
#endif

    if (obytes == 1)
        {
        // second word of the triple
#if DEBUG_PP || DEBUG_NET
        if ((d & 06001) != 0)
            {
            fprintf(niuLog, "\n%010u output out of sync, second word %04o",
                    traceSequenceNo, d);
            }
#endif
        currOutput |= d >> 1;
        obytes      = 2;

        return;
        }
    // Third word of the triple
#if DEBUG_PP || DEBUG_NET
    if ((d & 04000) != 0)
        {
        fprintf(niuLog, "\n%010u output out of sync, third word %04o",
                traceSequenceNo, d);
        }
#endif
#if REAL_TIMING
    /*
    **  If end of frame bit is set, remember that so the next
    **  output word is recognized as the start of a new frame.
    */
    if ((d & 02000) != 0)
        {
        frameStart = TRUE;
        }
#endif

    port = (d & 01777);
    niuSend(port, currOutput);
    obytes = 0;
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
**  Purpose:        Check for I/O availability.
**
**  Parameters:     Name        Description.
**                  Nothing.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void niuCheckIo(void)
    {
    PortParam      *availablePort;
#if defined(_WIN32)
    u_long         blockEnable = 1;
#endif
    struct sockaddr_in from;
#if defined(_WIN32)
    int            fromLen;
#else
    socklen_t      fromLen;
#endif
    int            i;
    int            maxFd;
    int            n;
    int            optEnable = 1;
    PortParam      *pp;
    fd_set         readFds;
    struct timeval timeout;
    fd_set         writeFds;

    ioTurns = (ioTurns + 1) % IoTurnsPerPoll;
    if (ioTurns != 0)
        {
        return;
        }

    FD_ZERO(&readFds);
    FD_ZERO(&writeFds);
    maxFd         = 0;
    availablePort = NULL;

    for (i = 0, pp = portVector; i < platoConns; i++, pp++)
        {
        if (pp->active)
            {
            if (pp->inInIdx < InBufSize)
                {
                FD_SET(pp->connFd, &readFds);
                if (pp->connFd > maxFd)
                    {
                    maxFd = pp->connFd;
                    }
                }
            if (pp->outInIdx > pp->outOutIdx)
                {
                FD_SET(pp->connFd, &writeFds);
                if (pp->connFd > maxFd)
                    {
                    maxFd = pp->connFd;
                    }
                }
            }
        else if (availablePort == NULL)
            {
            availablePort = pp;
            FD_SET(listenFd, &readFds);
            if (listenFd > maxFd)
                {
                maxFd = listenFd;
                }
            }
        }

    if (maxFd < 1)
        {
        return;
        }

    timeout.tv_sec  = 0;
    timeout.tv_usec = 0;
    n = select(maxFd + 1, &readFds, &writeFds, NULL, &timeout);
    if (n < 1)
        {
        return;
        }

    for (i = 0, pp = portVector; i < platoConns; i++, pp++)
        {
        if (pp->active)
            {
            if (FD_ISSET(pp->connFd, &readFds))
                {
                n = recv(pp->connFd, &pp->inBuffer[pp->inInIdx], InBufSize - pp->inInIdx, 0);
                if (n > 0)
                    {
#if DEBUG_NET
                    fprintf(niuLog, "\n%010u received %d bytes on port %02o",
                            traceSequenceNo, n, pp->id);
                    niuLogBytes(&pp->inBuffer[pp->inInIdx], n);
#endif
                    pp->inInIdx += n;
                    }
                else
                    {
                    niuClose(pp);
                    }
                }
            if (FD_ISSET(pp->connFd, &writeFds) && (pp->outOutIdx < pp->outInIdx))
                {
                n = send(pp->connFd, &pp->outBuffer[pp->outOutIdx], pp->outInIdx - pp->outOutIdx, 0);
                if (n >= 0)
                    {
#if DEBUG_NET
                    fprintf(niuLog, "\n%010u sent %d bytes to port %02o",
                            traceSequenceNo, n, pp->id);
                    niuLogBytes(&pp->outBuffer[pp->outOutIdx], n);
#endif
                    pp->outOutIdx += n;
                    if (pp->outOutIdx >= pp->outInIdx)
                        {
                        pp->outInIdx  = 0;
                        pp->outOutIdx = 0;
                        }
                    }
                }
            }
        }
    if ((availablePort != NULL) && FD_ISSET(listenFd, &readFds))
        {
        fromLen = sizeof(from);
        availablePort->connFd = accept(listenFd, (struct sockaddr *)&from, &fromLen);
        if (availablePort->connFd > 0)
            {
            availablePort->active    = TRUE;
            availablePort->inInIdx   = 0;
            availablePort->inOutIdx  = 0;
            availablePort->outInIdx  = 0;
            availablePort->outOutIdx = 0;

            /*
            **  Set Keepalive option so that we can eventually discover if
            **  a client has been rebooted.
            */
            setsockopt(availablePort->connFd, SOL_SOCKET, SO_KEEPALIVE, (void *)&optEnable, sizeof(optEnable));

            /*
            **  Make socket non-blocking.
            */
#if defined(_WIN32)
            ioctlsocket(availablePort->connFd, FIONBIO, &blockEnable);
#else
            fcntl(availablePort->connFd, F_SETFL, O_NONBLOCK);
#endif
#if DEBUG_NET
            fprintf(niuLog, "\n%010u accepted connection on port %02o",
                    traceSequenceNo, availablePort->id);
#endif
            niuWelcome(availablePort->id + NiuLocalStations);
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Close a port and mark it inactive.
**
**  Parameters:     Name        Description.
**                  pp          pointer to mux port parameters.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void niuClose(PortParam *pp)
    {
    netCloseConnection(pp->connFd);
    pp->active = FALSE;
    pp->connFd = 0;
#if DEBUG_NET
    fprintf(niuLog, "\n%010u connection closed on port %02o",
            traceSequenceNo, pp->id);
#endif
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

    sprintf(msg, "Connected to Plato station %d-%d", stat >> 5, stat & 037);
    niuSend(stat, 0042000 + stat); // NOP with station number in it
    niuSend(stat, 0100033);        // mode 3, mode rewrite, screen
    niuSend(stat, 0201200);        // load Y = 128
    niuSend(stat, 0200200);        // load X = 128
    niuSendstr(stat, msg);
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
    int  cc    = 2;
    int  w     = 017720;
    bool shift = FALSE;
    char c;

    while ((c = *p++) != 0)
        {
        if (isupper(c))
            {
            c = tolower(c);
            if (!shift)
                {
                w = (w << 6 | 077);
                if (++cc == 3)
                    {
                    cc = 0;
                    niuSend(stat, w);
                    w = 1;
                    }
                w = (w << 6 | 021);
                if (++cc == 3)
                    {
                    cc = 0;
                    niuSend(stat, w);
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
                niuSend(stat, w);
                w = 1;
                }
            w = (w << 6 | 020);
            if (++cc == 3)
                {
                cc = 0;
                niuSend(stat, w);
                w = 1;
                }
            shift = FALSE;
            }
        w = (w << 6 | asciiToCdc[c]);
        if (++cc == 3)
            {
            cc = 0;
            niuSend(stat, w);
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
        niuSend(stat, w);
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
    PortParam *pp;

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
        if (stat < platoConns)
            {
            pp = portVector + stat;
            if (pp->active)
                {
                if (pp->outInIdx + 2 < OutBufSize)
                    {
                    pp->outBuffer[pp->outInIdx++] = word >> 12;
                    pp->outBuffer[pp->outInIdx++] = ((word >> 6) & 077) | 0200;
                    pp->outBuffer[pp->outInIdx++] = (word & 077) | 0300;
                    }
#if DEBUG_PP || DEBUG_NET
                else
                    {
                    fprintf(niuLog, "\n%010u output buffer overflow, port %d",
                            traceSequenceNo, pp->id);
                    }
#endif
                }
            }
        }
    }

#if DEBUG_PP || DEBUG_NET

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

    switch (funcCode)
        {
    case FcNiuInput:
        return "Input";

    case FcNiuOutput:
        return "Output";

    default:
        sprintf(buf, "UNKNOWN: %04o", funcCode);

        return (buf);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Flush incomplete data line
**
**  Parameters:     Name        Description.
**
**  Returns:        nothing
**
**------------------------------------------------------------------------*/
static void niuLogFlush(void)
    {
    if (niuLogBytesCol > 0)
        {
        fprintf(niuLog, "\n%010u ", traceSequenceNo);
        fputs(niuLogBuf, niuLog);
        fflush(niuLog);
        }
    niuLogBytesCol = 0;
    memset(niuLogBuf, ' ', LogLineLength);
    niuLogBuf[LogLineLength] = '\0';
    }

/*--------------------------------------------------------------------------
**  Purpose:        Log a sequence of ASCII bytes
**
**  Parameters:     Name        Description.
**                  bytes       pointer to sequence of bytes
**                  len         length of the sequence
**
**  Returns:        nothing
**
**------------------------------------------------------------------------*/
static void niuLogBytes(u8 *bytes, int len)
    {
    u8   ac;
    int  ascCol;
    u8   b;
    char hex[3];
    int  hexCol;
    int  i;

    niuLogBytesCol = 0;
    niuLogFlush(); // initialize the log buffer
    ascCol = AsciiColumn(niuLogBytesCol);
    hexCol = HexColumn(niuLogBytesCol);

    for (i = 0; i < len; i++)
        {
        b  = bytes[i];
        ac = b;
        if ((ac < 0x20) || (ac >= 0x7f))
            {
            ac = '.';
            }
        sprintf(hex, "%02x", b);
        memcpy(niuLogBuf + hexCol, hex, 2);
        hexCol += 3;
        niuLogBuf[ascCol++] = ac;
        if (++niuLogBytesCol >= 16)
            {
            niuLogFlush();
            ascCol = AsciiColumn(niuLogBytesCol);
            hexCol = HexColumn(niuLogBytesCol);
            }
        }
    niuLogFlush();
    }

#endif

/*---------------------------  End Of File  ------------------------------*/
