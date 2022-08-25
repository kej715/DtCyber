/*--------------------------------------------------------------------------
**
**  Written by Mark Riordan, and placed in the public domain.  June 2004
**  Version 2 done in February 2005.
**
**  Version 3 done in August 2022 by Kevin Jordan. The Frend2 functionality
**  was integrated into the DtCyber executable instead of running as a
**  separate process.
**
**  Name: msufrend.c
**
**  Description:
**      Perform emulation of Michigan State University's FREND
**      interactive front-end.  The real FREND ran on an Interdata 7/32,
**      but we are emulating only the functionality of FREND and not
**      the 7/32 instruction set.
**
**      This module emulates the functions of the 6000 Channel Adapter (6CA),
**      a custom piece of hardware designed at MSU which allows a 6000 PP to
**      do DMA to the 7/32. An array of bytes implements the 7/32's memory.
**      Like the original 6CA, DtCyber reads and writes FREND memory without
**      disturbing frend3, and issues an interrupt request when it wants
**      frend3 to process commands written into the memory.
**
**      On the deadstart tape Ken Hunter gave to Mark Riordan 5 June 2004,
**      FREND is Equipment Status Table (EST) entry 70 octal, equip 0, unit 0,
**      channel 2, DST 24.
**--------------------------------------------------------------------------
*/

#define DEBUG 0

/*
**  -------------
**  Include Files
**  -------------
*/
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#endif
#include "const.h"
#include "proto.h"
#include "types.h"

#include "msufrend_lmbi.h"

/*
**  -----------------
**  Private Constants
**  -----------------
*/

/*
**  Function codes.
*/
#define FcFEFSEL                          02400 /* SELECT 6000 CHANNEL ADAPTER    */
#define FcFEFDES                          02410 /* DESELECT 6000 CHANNEL ADAPTER  */
#define FcFEFST                           00000 /* READ 6CA STATUS                */
#define FcFEFSAU                          01000 /* SET ADDRESS (UPPER)            */
#define FcFEFSAM                          01400 /* SET ADDRESS (MIDDLE)           */
#define FcFEFHL                           03000 /* HALT-LOAD THE 7/32             */
#define FcFEFINT                          03400 /* INTERRUPT THE 7/32             */
#define FcFEFLP                           06000 /* LOAD INTERFACE MEMORY          */
#define FcFEFRM                           04400 /* READ                           */
#define FcFEFWM0                          07000 /* WRITE MODE 0                   */
#define FcFEFWM                           07400 /* WRITE MODE 1                   */
#define FcFEFRSM                          05000 /* READ AND SET                   */
#define FcFEFCI                           00400 /* CLEAR INITIALIZED STATUS BIT   */

/*
**  Commands from 1FP to FREND.
*/
#define FC_ITOOK                          1
#define FC_HI80                           2
#define FC_HI240                          3
#define FC_CPOP                           4
#define FC_CPGON                          5

/*
** FREND 6000 Channel Adapter bits, for function FcFEFST
*/
#define FCA_STATUS_INITIALIZED            04000
#define FCA_STATUS_NON_EXIST_MEM          02000
#define FCA_STATUS_LAST_BYTE_NO_ERR       00000
#define FCA_STATUS_LAST_BYTE_PAR_ERR      00400
#define FCA_STATUS_LAST_BYTE_MEM_MAL      01000
#define FCA_STATUS_LAST_BYTE_NON_EXIST    01400
#define FCA_STATUS_MODE_WHEN_ERROR        00200
#define FCA_STATUS_READ_WHEN_ERROR        00100
#define FCA_STATUS_WRITE_WHEN_ERROR       00040
#define FCA_STATUS_HALT_LOADING           00020
#define FCA_STATUS_INT_PENDING            00010

#define DEFAULT_MAX_CONNECTIONS           8
#define DEFAULT_TCP_PORT                  6500
#define IoTurnsPerPoll                    4
#define MIN_FREE_PORT_BUFFERS             2
/*
** It appears that the Cyber never tries to access memory beyond this.
*/
#define MAX_FREND_BYTES                   0xc0000

/* hard-coded port numbers for initial implementation. */
#define FPORTCONSOLE      4     /* must be greater than PTN.MAX */
#define RESERVED_PORTS    4
#define FIRSTUSERPORT     (RESERVED_PORTS+1)

/*
**  -----------------------
**  Private Macro Functions
**  -----------------------
*/
/* Interdata 7/32 types */
#define ByteAddr  u32
#define FrendAddr u32
#define FullWord  u32
#define HalfWord  u16

/* Compute FWA of socket entry given port number */
#define portNumToFwa(portNum) (activeFrend->fwaPORT + (portNum - 1) * LE_PORT)
#define sockNumToFwa(sockNum) (activeFrend->fwaSOCK + (sockNum - 1) * LE_SOCK)

#if !defined(_WIN32)
#define SOCKET int
#endif

/*
**  -----------------------------------------
**  Private Typedef and Structure Definitions
**  -----------------------------------------
*/
typedef enum telnetState
    {
    TELST_NORMAL, TELST_GOT_IAC, TELST_GOT_WILL_OR_SIMILAR
    } TelnetState;

#define TELCODE_IAC                      0xff
#define TELCODE_DONT                     0xfe
#define TELCODE_DO                       0xfd
#define TELCODE_WONT                     0xfc
#define TELCODE_WILL                     0xfb

#define TELCODE_OPT_ECHO                 0x01
#define TELCODE_OPT_SUPPRESS_GO_AHEAD    0x03

typedef struct pendingBuffer
    {
    u8    buf[L_LINE + 16];  /* Waiting chars */
    int   first;             /* Index of first char still pending */
    int   charsLeft;        /* # of chars remaining in buffer */
    } PendingBuffer;

typedef struct portContext
    {
    int           id;
    struct frendContext *frend; /* pointer to supporting FREND context */
    bool          active;       /* TRUE if port is connected and active */
    SOCKET        fd;           /* TCP socket descriptor */
    TelnetState   telnetState;  /* telnet state */
    bool          EOLL;         /* TRUE if last line ended in end of line */
    PendingBuffer pbuf;         /* chars pending output.  Normally, this is 0 */
                                /* except when assembling bytes to be sent. */
                                /* If non-zero, then we shouldn't send any */
                                /* more lines until this buffer is sent. */
    } PortContext;

typedef struct frendContext
    {
    int         listenPort;
    SOCKET      listenFd;
    int         portCount;
    bool        doesTelnet;           /* TRUE if Telnet protocol enabled */
    int         ioTurns;
    PortContext *ports;               /* one per supported terminal */
    ByteAddr    addr;                 /* Next byte (not halfword) address to read or write.
                                       * However, when this is set via the 6CA, the bottom
                                       * bit is cleared, because the memory interface between
                                       * FREND and the Cyber specifies addresses halfword addresses */
    bool        nextIsSecond;         /* true if the next byte of I/O is the second in a sequence.
                                       * This is used for READ-AND-SET, which transfers 2 bytes
                                       * but which is not supposed to change the address register. */
    u8          mem[MAX_FREND_BYTES]; /* Contents of FREND memory, in bytes.
                                       * The 7/32 stored in most-significant-byte-first format */
    FrendAddr fwaMISC;
    FrendAddr fwaFPCOM;
    FrendAddr fwaBF80;
    FrendAddr fwaBF240;
    FrendAddr fwaBFREL;
    FrendAddr fwaBANM;
    FrendAddr fwaLOGM;
    FrendAddr fwaSOCK;
    FrendAddr fwaDVSK;
    FrendAddr fwaPORT;
    FrendAddr fwaPTBUF;
    FrendAddr fwaMALC;
    FrendAddr fwaALLOC;
    FrendAddr fwaBuffers80;
    FrendAddr fwaBuffers240;
    } FrendContext;

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static void      msufrendCheckIo(void);
static FcStatus  msufrendFunc(PpWord funcCode);
static void      msufrendIo(void);
static void      msufrendActivate(void);
static void      msufrendDisconnect(void);
#if DEBUG
static char      *msufrendFunc2String(PpWord funcCode);
#endif

static void      handleInterruptFromHost(void);
static HalfWord  initCircList(FrendAddr fwaList, HalfWord nslots);
static void      initLmbi(void);
static void      initPortBufs(void);
static void      setHalfWord(FrendAddr addr, HalfWord half);

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
static FrendContext *activeFrend = NULL;

#if DEBUG
static FILE *msufrendLog = NULL;
#endif

/*
 **--------------------------------------------------------------------------
 **
 **  Public Functions
 **
 **--------------------------------------------------------------------------
 */
/*--------------------------------------------------------------------------
**  Purpose:        Initialise the MSUFREND device.
**
**  Parameters:     Name        Description.
**                  eqNo        equipment number
**                  unitNo      unit number
**                  channelNo   channel number the device is attached to
**                  params      optional device parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void msufrendInit(u8 eqNo, u8 unitNo, u8 channelNo, char *params)
    {
#if defined(_WIN32)
    u_long blockEnable = 1;
#endif
    char               connType[16];
    DevSlot            *dp;
    FrendAddr          fwaThisSock;
    FrendAddr          fwaListSock;
    u8                 i;
    bool               isTelnet;
    int                listenPort;
    int                numParam;
    int                optEnable = 1;
    int                portCount;
    PortContext        *pp;
    struct sockaddr_in server;

#if DEBUG
    if (msufrendLog == NULL)
        {
        msufrendLog = fopen("msufrendlog.txt", "wt");
        }
#endif

    dp               = channelAttach(channelNo, eqNo, DtMSUFrend);
    dp->activate     = msufrendActivate;
    dp->disconnect   = msufrendDisconnect;
    dp->func         = msufrendFunc;
    dp->io           = msufrendIo;
    dp->selectedUnit = unitNo;

    if (dp->context[0] != NULL)
        {
        fputs("(msufrend) Only one unit is possible per equipment\n", stderr);
        exit(1);
        }
    if (params == NULL)
        {
        params = "";
        }
    numParam = sscanf(params, "%d,%d,%s", &listenPort, &portCount, connType);
    if (numParam < 1)
        {
        listenPort = DEFAULT_TCP_PORT;
        portCount  = DEFAULT_MAX_CONNECTIONS;
        isTelnet   = TRUE;
        }
    else if (numParam < 2)
        {
        portCount  = DEFAULT_MAX_CONNECTIONS;
        isTelnet   = TRUE;
        }
    else if (strcasecmp(connType, "telnet") == 0)
        {
        isTelnet   = TRUE;
        }
    else if (strcasecmp(connType, "raw") == 0)
        {
        isTelnet   = FALSE;
        }
    else
        {
        fprintf(stderr, "(msufrend) Invalid connection type: %s, not one of 'telnet' or 'raw'.\n", connType);
        exit(1);
        }
    if ((listenPort < 1) || (listenPort > 65535))
        {
        fprintf(stderr, "(msufrend) Invalid TCP port number: %d\n", listenPort);
        exit(1);
        }
    if (portCount < 1)
        {
        fprintf(stderr, "(msufrend) Invalid port count: %d\n", portCount);
        exit(1);
        }
    activeFrend = (FrendContext *)calloc(1, sizeof(FrendContext));
    if (activeFrend == NULL)
        {
        fputs("(msufrend) Failed to allocate context block\n", stderr);
        exit(1);
        }
    dp->context[0]          = activeFrend;
    activeFrend->listenPort = listenPort;
    activeFrend->portCount  = portCount + RESERVED_PORTS;
    activeFrend->doesTelnet = isTelnet;
    activeFrend->ioTurns    = IoTurnsPerPoll - 1;
    activeFrend->ports      = (PortContext *)calloc(activeFrend->portCount, sizeof(PortContext));
    if (activeFrend->ports == NULL)
        {
        fputs("(msufrend) Failed to allocate port context blocks\n", stderr);
        exit(1);
        }
    initLmbi();

    /*
    **  Initialise port context blocks.
    */
    for (i = 0, pp = activeFrend->ports; i < activeFrend->portCount; i++, pp++)
        {
        pp->id        = i + 1;
        pp->frend     = activeFrend;
        pp->active    = FALSE;
        pp->EOLL      = FALSE;
        fwaThisSock   = portNumToFwa(pp->id);
        fwaListSock   = fwaThisSock + W_SKOTCL;
        /* Initialize the circular list, which is actually part of the socket entry. */
        initCircList(fwaListSock, L_SKOCL);
        setHalfWord(fwaThisSock + H_SKNUM, pp->id);
        }

    initPortBufs();
    setHalfWord(H_INICMP, 1); /* Initialization complete */

    /*
    **  Create socket, bind to specified port, and begin listening for connections
    */
    activeFrend->listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (activeFrend->listenFd < 0)
        {
        fprintf(stderr, "(msufrend) Can't create socket on port %d\n", activeFrend->listenPort);
        exit(1);
        }

    /*
    **  Accept will block if client drops connection attempt between select and accept.
    **  We can't block so make listening socket non-blocking to avoid this condition.
    */
#if defined(_WIN32)
    ioctlsocket(activeFrend->listenFd, FIONBIO, &blockEnable);
#else
    fcntl(activeFrend->listenFd, F_SETFL, O_NONBLOCK);
#endif

    /*
    **  Bind to configured TCP port number
    */
    setsockopt(activeFrend->listenFd, SOL_SOCKET, SO_REUSEADDR, (void *)&optEnable, sizeof(optEnable));
    memset(&server, 0, sizeof(server));
    server.sin_family      = AF_INET;
    server.sin_addr.s_addr = inet_addr("0.0.0.0");
    server.sin_port        = htons(activeFrend->listenPort);
    if (bind(activeFrend->listenFd, (struct sockaddr *)&server, sizeof(server)) < 0)
        {
        fprintf(stderr, "(msufrend) Can't bind to listen socket on port %d\n", activeFrend->listenPort);
        exit(1);
        }

    /*
    **  Start listening for new connections on this TCP port number
    */
    if (listen(activeFrend->listenFd, 5) < 0)
        {
        fprintf(stderr, "(msufrend) Can't listen on port %d\n", activeFrend->listenPort);
        exit(1);
        }

    /*
    **  Print a friendly message.
    */
    printf("(msufrend) initialised on channel %o equipment %o, ports %d, TCP port %d\n",
           channelNo, eqNo, portCount, activeFrend->listenPort);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Execute function code on MSU FREND.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        FcStatus
**
**------------------------------------------------------------------------*/
static FcStatus msufrendFunc(PpWord funcCode)
    {
    u8 data = funcCode & 0xff;

    activeFrend = (FrendContext *)activeDevice->context[0];

    /* Mask off top 4 bits of the PP word.  This contains the function code,
     * more or less.  In some cases, the bottom 8 bits contain data related
     * to the function--typically address bits to set.
     */
    activeDevice->fcode   = funcCode & 07400;

#if DEBUG
    fprintf(msufrendLog, "\n%010u PP:%02o CH:%02o P:%04o f:%04o T:%s",
            traceSequenceNo, activePpu->id, activeDevice->channel->id,
            activePpu->regP, funcCode, msufrendFunc2String(activeDevice->fcode));
#endif

    switch (activeDevice->fcode)
        {
    case FcFEFSEL:    /* SELECT   6000 CHANNEL ADAPTER */
    case FcFEFDES:    /* DESELECT 6000 CHANNEL ADAPTER */
        /* DESELECT is a special case--must recheck original parameter. */
        if (FcFEFDES == funcCode)
            {
            activeDevice->fcode = funcCode;
            }

        /* Technically, this is not correct, because SELECT (2400) and
         * DESELECT (2410) have the same top 4 bits.
         * But I don't think it matters. */
        break;

    case FcFEFST:
        break;

    case FcFEFSAU:
        /* SET ADDRESS (UPPER) - Set upper 3 bits of 19-bit address */
        activeFrend->addr = (activeFrend->addr & 0x1fffe) | ((data & 7) << 17);
#if DEBUG
        fprintf(msufrendLog, " data:%02x", data);
#endif
        break;

    case FcFEFSAM:
        /* SET ADDRESS (MIDDLE) - Set middle byte of address, bits 2**8 thru 2**15 */
        activeFrend->addr = (activeFrend->addr & 0xfe01ff) | (((u32)data) << 9);
#if DEBUG
        fprintf(msufrendLog, " data:%02x", data);
#endif
        break;

    case FcFEFHL:
        /* Halt-Load the 7/32 */
        break;

    case FcFEFINT: /* Interrupt the 7/32 */
        handleInterruptFromHost();
        break;

    case FcFEFLP: /* LOAD INTERFACE MEMORY */
        /* Prepare to accept 8-bit bytes, to be written in "a 16-byte memory"
         * starting at location 0. */
        activeFrend->addr = 0;
        break;

    case FcFEFRM:
        /* READ - I think the data byte is the lower 8 bits of the address. */
        activeFrend->addr = (activeFrend->addr & 0x1fffe00) | ((u32)(data << 1));
#if DEBUG
        fprintf(msufrendLog, " data:%02x addr:%05x <-", data, activeFrend->addr);
#endif
        break;

    case FcFEFWM0:
        /* WRITE MODE 0 - One PP word considered as 2 6-bit bytes, written to a 16-bit FE word. */
        activeFrend->addr = (activeFrend->addr & 0x1fffe00) | ((u32)(data << 1));
#if DEBUG
        fprintf(msufrendLog, " data:%02x addr:%05x ->", data, activeFrend->addr);
#endif
        break;

    case FcFEFWM:
        /* WRITE MODE 1 - Two consecutive PP words (8 in 12) written to a 16-bit FE word. */
        /* First PP word goes to upper 8 bits of FE word, confusingly called bits 0-7. */
        activeFrend->addr = (activeFrend->addr & 0x1fffe00) | ((u32)(data << 1));
#if DEBUG
        fprintf(msufrendLog, " data:%02x addr:%05x ->", data, activeFrend->addr);
#endif
        break;

    case FcFEFRSM:
        /* READ AND SET - test and set on single 16-bit location. */
        /* Mode always forced to 1 so exactly 2 bytes of data will be sent to PPU. */
        /* After second byte, channel is empty until transfer terminated */
        /* by the PPU with a DCN.  Address register is not changed, says */
        /* the 6CA hardware doc. */
        /* Apparently the top bit is set. */
        activeFrend->addr = (activeFrend->addr & 0x1fffe00) | ((u32)(data << 1));
        activeFrend->nextIsSecond = FALSE;
#if DEBUG
        fprintf(msufrendLog, " data:%02x addr:%05x <-", data, activeFrend->addr);
#endif
        break;

    case FcFEFCI:
        /* CLEAR INITIALIZED STATUS BIT   */
        break;
        }

    return (FcAccepted);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Perform I/O on MSU FREND.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**  The rules seem to be:
**  If last function was read, if activeChannel->full is true, do nothing,
**     else if there's data in the device,
**       set activeChannel->data to the next 12-bit data
**       and set activeChannel->full = TRUE;
**     else (no data in device)
**       activeChannel->active = FALSE;
**  If last function was write, if activeChannel->full is false, do nothing,
**     else take activeChannel->data and send it to the device,
**          and set activeChannel->full = FALSE;
**------------------------------------------------------------------------*/
static void msufrendIo(void)
    {
    activeFrend = (FrendContext *)activeDevice->context[0];

    msufrendCheckIo();

    switch (activeDevice->fcode)
        {
    case FcFEFSEL:
        /* I don't know what to do on an I/O after a SELECT */
        activeChannel->full   = TRUE;
        activeChannel->active = TRUE;
        break;

    case FcFEFDES:
        /* DESELECT 6000 CHANNEL ADAPTER  */
        break;

    case FcFEFST:
        /* READ 6CA STATUS */
        if (!activeChannel->full)
            {
            activeChannel->data = FCA_STATUS_INITIALIZED | FCA_STATUS_LAST_BYTE_NO_ERR;
            activeChannel->full = TRUE;
#if DEBUG
            fprintf(msufrendLog, " data:%04o", activeChannel->data);
#endif
            }
        break;

    case FcFEFRM:
        /* READ */
        if (!activeChannel->full)
            {
            activeChannel->data = activeFrend->mem[activeFrend->addr++];
            activeChannel->full = TRUE;
#if DEBUG
            fprintf(msufrendLog, " %04o", activeChannel->data);
#endif
            }
        break;

    case FcFEFRSM:
        /* READ AND SET */
        if (!activeChannel->full)
            {
            /* Return either the top or bottom byte of the address--but do */
            /* not change the address register. */
            if (activeFrend->nextIsSecond)
                {
                activeChannel->data = activeFrend->mem[activeFrend->addr + 1];
                activeFrend->nextIsSecond = FALSE; /* Probably unnecessary. */
                }
            else
                {
                activeChannel->data = activeFrend->mem[activeFrend->addr];
                /* Set top bit of word. */
                activeFrend->mem[activeFrend->addr] |= 0x80;
                activeFrend->nextIsSecond = TRUE;
                }
            activeChannel->full = TRUE;
#if DEBUG
            fprintf(msufrendLog, " %04o", activeChannel->data);
#endif
            }
        break;

    case FcFEFWM0:
        /* WRITE MODE 0 - One PP word considered as 2 6-bit bytes, written to a 16-bit FE word. */
        if (activeChannel->full)
            {
            /*
            **  Output data.
            */
            activeFrend->mem[activeFrend->addr++] = (u8)activeChannel->data >> 8;
            activeFrend->mem[activeFrend->addr++] = (u8)activeChannel->data & 0xff;
            activeChannel->full = FALSE;
#if DEBUG
            fprintf(msufrendLog, " %04o", activeChannel->data);
#endif
            }
        break;

    case FcFEFWM:
        /* WRITE MODE 1 - Two consecutive PP words (8 in 12) written to a 16-bit FE word. */
        /* First PP word goes to upper 8 bits of FE word, confusingly called bits 0-7. */
        if (activeChannel->full)
            {
            /*
            **  Output data.
            */
            activeFrend->mem[activeFrend->addr++] = (u8)activeChannel->data;
            activeChannel->full = FALSE;
#if DEBUG
            fprintf(msufrendLog, " %04o", activeChannel->data);
#endif
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
static void msufrendActivate(void)
    {
    activeChannel->active = TRUE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Handle disconnecting of channel.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void msufrendDisconnect(void)
    {
    activeChannel->active = FALSE;
    }

/*==========================================================================
 * Reimplementation of FREND, an interactive front-end to SCOPE/Hustler,
 * written at Michigan State University in the late 1970's.
 *
 * Mark Riordan  23 June   2004 (original implementation)
 * Kevin Jordan  20 August 2022 (integrated within DtCyber executable)
 *
 * This version is integrated within the DtCyber executable. DtCyber has
 * direct access to FREND memory, just as in the original 6000 Channel Adapter
 * and FREND.
 *
 * It really helps to have listings of the original FREND and 1FP
 * when you are following this code.  See http://60bits.net
 * As I wrote this code, I made it more and more like FREND.
 * Originally, I tried to take shortcuts and implement a simpler
 * structure--but that's the hard way if you want to eventually
 * get most things working.  Now even the routine names are
 * largely borrowed from FREND.
 *
 * Original frend2 source is maintained via CVS (Concurrent Versions System)
 * at:
 *
 * http://sourceforge.net/projects/frend2
 *
 * Symbol names are mostly derived from FREND, with '_' substituted
 * for camel case.
 *
 * In comments, FWA means "First Word Address".  It means the
 * address of the first byte of a structure.
 *
 * This software is subject to the following, which is believed to be
 * a verbatim copy of the well-known MIT license.
 *
 *-----  Beginning of License  ---------------------------------
 *  Copyright (c) 2005, Mark Riordan
 *
 *  Permission is hereby granted, free of charge, to any person
 *  obtaining a copy of this software and associated documentation
 *  files (the "Software"), to deal in the Software without
 *  restriction, including without limitation the rights to use,
 *  copy, modify, merge, publish, distribute, sublicense, and/or
 *  sell copies of the Software, and to permit persons to whom the
 *  Software is furnished to do so, subject to the following
 *  conditions:
 *
 *  The above copyright notice and this permission notice shall be
 *  included in all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 *  OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *  HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 *  OTHER DEALINGS IN THE SOFTWARE.
 *  -----  End of License  ---------------------------------------
 */

static char *frendVersion = "63.01";
static char *author       = "Mark Riordan  4513 Gregg Rd  Madison, WI  53705";

/*=====  Function prototypes  =============================== */
static void taskSENDCP(HalfWord portNum, u8 MsgCode);
static void taskSKOTCL(PortContext *pp, FrendAddr fwaMySocket);

#define ALIGN_FULLWORD(addr)    (0xfffffffc & (3 + addr))

/* Sets a 1-bit flag in a halfword */
/* EntryBaseAddr is the FWA of this table entry (NOT the FWA of the whole table). */
/* Name is the name of the flag.  We obtain both offset and bit position from this. */
#define SETHFLAG(EntryBaseAddr, Name) \
    setHalfWord(EntryBaseAddr + H_ ## Name, (HalfWord)(getHalfWord(EntryBaseAddr + H_ ## Name) | (1 << (15 - J_ ## Name))));

/* Clears a 1-bit flag in a halfword */
/* EntryBaseAddr is the FWA of this table entry (NOT the FWA of the whole table). */
/* Name is the name of the flag.  We obtain both offset and bit position from this. */
#define CLEARHFLAG(EntryBaseAddr, Name) \
    setHalfWord(EntryBaseAddr + H_ ## Name, (HalfWord)(getHalfWord(EntryBaseAddr + H_ ## Name) & (0xffff - (1 << (15 - J_ ## Name)))));

/* Returns 0 or 1, depending on whether a bit flag is set. */
#define HFLAGISSET(EntryBaseAddr, Name) \
    ((getHalfWord(EntryBaseAddr + H_ ## Name) & (1 << (15 - J_ ## Name))) ? 1 : 0)

/*--- function getLastOSError ----------------------
 *  Return the last error on a system call.
 *  Intended as a platform-agnostic wrapper to GetLastError() or whatever.
 */
static int getLastOSError()
    {
#if defined(_WIN32)
    return GetLastError();
#else
    return errno;
#endif
    }

/*--- function getLastSocketError ---------------------
 *  Socket-specific version of getLastOSError().
 */
static int getLastSocketError()
    {
#if defined(_WIN32)
    return WSAGetLastError();
#else
    return errno;
#endif
    }


/*--- function addrFrendTo1FP ----------------------
 *  Convert an address from FREND to 1FP format.
 *  This means dividing by 2, and ORing in the magic value
 *  intended to catch hardware errors.
 */
static FrendAddr addrFrendTo1FP(FrendAddr addr)
    {
    if (addr)
        {
        return (addr >> 1) | (F_PTIN << 24);
        }
    else
        {
        return 0;
        }
    }

/*--- function addr1FPToFrend ----------------------
 *  Convert an address from 1FP format to FREND format.
 *  This means multiplying by 2.
 *  To play it safe, we also get rid of the F_PTIN bits.
 */
static FrendAddr addr1FPToFrend(FrendAddr addr)
    {
    return (addr & 0xffffff) << 1; /* clear magic F_PTIN bits */
    }

static void setFullWord(FrendAddr addr, FullWord word)
    {
    activeFrend->mem[addr]     = (u8)((word >> 24) & 0xff);
    activeFrend->mem[1 + addr] = (u8)((word >> 16) & 0xff);
    activeFrend->mem[2 + addr] = (u8)((word >> 8) & 0xff);
    activeFrend->mem[3 + addr] = (u8)(word & 0xff);
    }

static FullWord getFullWord(FrendAddr addr)
    {
    return   (activeFrend->mem[addr    ] << 24)
           | (activeFrend->mem[1 + addr] << 16)
           | (activeFrend->mem[2 + addr] << 8)
           |  activeFrend->mem[3 + addr];
    }

static void setHalfWord(FrendAddr addr, HalfWord half)
    {
    activeFrend->mem[addr]     = (u8)((half >> 8) & 0xff);
    activeFrend->mem[1 + addr] = (u8)(half & 0xff);
    }

static HalfWord getHalfWord(FrendAddr addr)
    {
    return (activeFrend->mem[addr] << 8) | (activeFrend->mem[1 + addr]);
    }

static void setByte(FrendAddr addr, u8 byte)
    {
    activeFrend->mem[addr] = byte;
    }

static u8 getByte(FrendAddr addr)
    {
    return activeFrend->mem[addr];
    }

/*--- function writeToOperTerm ----------------------
 *  Write a character to the operator terminal, possibly
 *  also logging it to a file.
 */
static void writeToOperTerm(u8 ch)
    {
    char buf[2];

    buf[0] = ch;
    buf[1] = '\0';
    opDisplay(buf);
    }

/*--- function sendToFSock ----------------------
 *  Send a buffer of bytes to a socket.
 *  We special-case the console, which is not connected via TCP.
 *  Exit:   Returns # of bytes sent, or -1 if error.
 *          (The error is usually EWOULDBLOCK--not really an error.)
 *          The # may be less than nOutBytes because socket is non-blocking.
 */
static int sendToFSock(PortContext *pp, u8 bufout[], int nOutBytes)
    {
    int bytesSent;

    if (FPORTCONSOLE == pp->id)
        {
        int j;
        for (j = 0; j < nOutBytes; j++)
            {
            writeToOperTerm(bufout[j]);
            }

        return nOutBytes;
        }
    else
        {
        if (pp->active)
            {
            /*//! We should handle the case where not all bytes are sent.
             * That would require queuing (fairly simple) and to really
             * do it right, the ability to stop accepting lines from the
             * host until the output queue is empty. */
            bytesSent = send(pp->fd, bufout, nOutBytes, 0);
#if DEBUG
            if (bytesSent != nOutBytes)
                {
                fprintf(msufrendLog, "\nsendToFSock: sent %d of %d bytes", bytesSent, nOutBytes);
                }
#endif

            return bytesSent;
            }
        else
            {
#if DEBUG
            fprintf(msufrendLog, "\nsendToFSock on port %d is not active", pp->id);
#endif
            return -1;
            }
        }
    }

/*=====  Circular List functions.  See lmbi.h for description.  =====*/

/*--- function initCircList ----------------------
 *  Initialize a 7/32-style circular list.
 *  Entry:  fwa      is the addess of the list.
 *          nslots   is the number of slots in the list.
 *  Exit:   Returns # of bytes in circular list
 */
static HalfWord initCircList(FrendAddr fwaList, HalfWord nslots)
    {
    HalfWord totbytes = H_CIRCLIST_HEADER_BYTES + (nslots * CIRCLIST_SLOT_SIZE_BYTES);
    memset(activeFrend->mem + fwaList, 0, totbytes);
    setHalfWord(fwaList + H_CIRCLIST_N_SLOTS_TOT, nslots);

    return totbytes;
    }

/*--- function getListUsedEntries ----------------------
 *  Exit:  Returns # of used entries in the list.
 */
static HalfWord getListUsedEntries(FrendAddr fwaList)
    {
    return getHalfWord(fwaList + H_CIRCLIST_N_USED);
    }

/*--- function getListUsedEntries ----------------------
 *  Exit:  Returns total # of entries (used and free) in the list.
 */
static HalfWord getListTotalEntries(FrendAddr fwaList)
    {
    return getHalfWord(fwaList + H_CIRCLIST_N_SLOTS_TOT);
    }

/*--- function getListFreeEntries ----------------------
 *  Exit:   Returns # of available slots in list.
 */
static HalfWord getListFreeEntries(FrendAddr fwaList)
    {
    return getListTotalEntries(fwaList) - getListUsedEntries(fwaList);
    }

/*--- function findEntryInList ----------------------
 *  Look for a value in a circular list.  This is used for debugging.
 *  Entry:  fwaList  is the FWA of a circular list.
 *          myword   is the word we are looking for.
 *
 *  Exit:   Returns CIRCLIST_NOT_FOUND if not found, else
 *             slot number found.
 */
static HalfWord findEntryInList(FrendAddr fwaList, FullWord myword)
    {
    HalfWord islot;
    HalfWord slots_so_far;
    FullWord thisWord;
    HalfWord nSlotsTot = getHalfWord(fwaList + H_CIRCLIST_N_SLOTS_TOT);
    HalfWord curTop    = getHalfWord(fwaList + H_CIRCLIST_CUR_TOP);
    HalfWord nextBot   = getHalfWord(fwaList + H_CIRCLIST_NEXT_BOT);
    HalfWord nUsed     = getHalfWord(fwaList + H_CIRCLIST_N_USED);

    for (islot = curTop, slots_so_far = 0; slots_so_far < nUsed; slots_so_far++)
        {
        thisWord = getFullWord(CircListSlotAddr(fwaList, islot));
        if (thisWord == myword)
            {
            return islot;
            }
        islot++;
        if (islot >= nSlotsTot)
            {
            islot = 0;
            }
        }

    return CIRCLIST_NOT_FOUND;
    }

/*--- function addToTopOfList ----------------------
 *  Add a word to a 7/32 circular list, at the top.
 *  Entry:  fwaList  is the FWA of the list.
 *          myword   is the 4-byte word to add.
 */
static void addToTopOfList(FrendAddr fwaList, FullWord myword)
    {
    HalfWord curTop;
    HalfWord nSlotsUsed = getHalfWord(fwaList + H_CIRCLIST_N_USED);
    HalfWord nSlotsTot  = getHalfWord(fwaList + H_CIRCLIST_N_SLOTS_TOT);

    /*//! debugging */
    if (CIRCLIST_NOT_FOUND != findEntryInList(fwaList, myword))
        {
#if DEBUG
        fprintf(msufrendLog, "\naddToTopOfList(%x, %x): value already in list", fwaList, myword);
#endif
        return;
        }
    if (nSlotsUsed >= nSlotsTot)
        {
        /* Don't add if list is full. */
#if DEBUG
        fprintf(msufrendLog, "\naddToTopOfList(%05x, %04x): list is full (capacity %d)", fwaList, myword, nSlotsTot);
#endif
        }
    else
        {
        nSlotsUsed++;
        setHalfWord(fwaList + H_CIRCLIST_N_USED, nSlotsUsed);
        /* We add to the top by DECREMENTING the top pointer circularly. */
        curTop = getHalfWord(fwaList + H_CIRCLIST_CUR_TOP);
        if (0 == curTop)
            {
            curTop = nSlotsTot - 1;
            }
        else
            {
            curTop--;
            }
        setFullWord(CircListSlotAddr(fwaList, curTop), myword);
        setHalfWord(fwaList + H_CIRCLIST_CUR_TOP, curTop);
        }
    }

/*--- function addToBottomOfList ----------------------
 *  Add a word to a 7/32 circular list, at the bottom.
 *  Entry:  fwaList  is the FWA of the list.
 *          myword   is the 4-byte word to add.
 */
static void addToBottomOfList(FrendAddr fwaList, FullWord myword)
    {
    HalfWord nextBot;
    HalfWord nSlotsUsed = getHalfWord(fwaList + H_CIRCLIST_N_USED);
    HalfWord nSlotsTot  = getHalfWord(fwaList + H_CIRCLIST_N_SLOTS_TOT);

    if (nSlotsUsed >= nSlotsTot)
        {
        /* Don't add if list is full. */
#if DEBUG
        fprintf(msufrendLog, "\naddToBottomOfList(%xH, %xH): list is full", fwaList, myword);
#endif
        }
    else
        {
        nSlotsUsed++;
        setHalfWord(fwaList + H_CIRCLIST_N_USED, nSlotsUsed);
        /* We add to the top by INCREMENTING the top pointer circularly. */
        nextBot = getHalfWord(fwaList + H_CIRCLIST_NEXT_BOT);
        setFullWord(CircListSlotAddr(fwaList, nextBot), myword);
        if (nextBot >= nSlotsTot)
            {
            nextBot = 0;
            }
        else
            {
            nextBot++;
            }
        setHalfWord(fwaList + H_CIRCLIST_NEXT_BOT, nextBot);
        }
    }

/*--- function removeFromBottomOfList ----------------------
 *  Entry:  fwaList  is the FWA of a 7/32 circular list.
 *  Exit:   Returns the current bottom, and removes it from the list
 *          or returns 0 if the list was empty.
 */
static FullWord removeFromBottomOfList(FrendAddr fwaList)
    {
    FullWord myWord = 0;
    HalfWord nextBot, curBot;
    HalfWord nSlotsUsed = getHalfWord(fwaList + H_CIRCLIST_N_USED);
    HalfWord nSlotsTot  = getHalfWord(fwaList + H_CIRCLIST_N_SLOTS_TOT);

    if (0 == nSlotsUsed)
        {
        /* List is empty */
        }
    else
        {
        /* The current bottom must be calculated by backing
         * up from the next bottom. */
        nextBot = getHalfWord(fwaList + H_CIRCLIST_NEXT_BOT);
        if (0 == nextBot)
            {
            curBot = nSlotsTot - 1;
            }
        else
            {
            curBot = nextBot - 1;
            }
        /* Fetch this current bottom. */
        myWord = getFullWord(CircListSlotAddr(fwaList, curBot));
        /* Now the next bottom is what the current bottom used to be. */
        setHalfWord(fwaList + H_CIRCLIST_NEXT_BOT, curBot);

        nSlotsUsed--;
        setHalfWord(fwaList + H_CIRCLIST_N_USED, nSlotsUsed);
        }

    return myWord;
    }

/*--- function isListEmpty ----------------------
 *  Entry:  fwaList  is the FWA of a 7/32 circular list.
 *  Exit:   Returns TRUE if list is empty, else FALSE.
 */
static bool isListEmpty(FrendAddr fwaList)
    {
    return 0 == getListUsedEntries(fwaList);
    }

static void setPortHalfWord(int portNum, int offset, HalfWord val)
    {
    setHalfWord(portNumToFwa(portNum) + offset, val);
    }

static void setPortFullWord(int portNum, int offset, FullWord val)
    {
    setFullWord(portNumToFwa(portNum) + offset, val);
    }

/*--- function interlockIsFree ----------------------
 *  Entry:  addr  is the address of a halfword interlock.
 *
 *  Exit:   Returns 1 if the interlock is available, else 0.
 */
static bool interlockIsFree(FrendAddr addr)
    {
    HalfWord lock = getHalfWord(addr);

    return (0 == (0x8000 & lock));
    }

/*--- function INTRLOC ----------------------
 *  Wait for and obtain an interlock.
 *  Entry:  addr    is the address of a halfword interlock.
 */
static void INTRLOC(FrendAddr addr)
    {
#if DEBUG
    if (0x8000 & getHalfWord(addr))
        {
        fprintf(msufrendLog, "\nWarning: Lock %x already obtained", addr);
        }
#endif
    setHalfWord(addr, 0x8000);
    }

/*--- function dropInterlock ----------------------
 *  Clear an interlock by setting a special value (1 == clear).
 *  Entry:  addr  is the address of the interlock halfword.
 */
static void dropInterlock(FrendAddr addr)
    {
    setHalfWord(addr, CLR_TS);
    }

/*--- function get80 ----------------------
 *  Exit:   Returns the address of a free 80-character buffer.
 */
static FrendAddr get80()
    {
    FrendAddr bufAddr = removeFromBottomOfList(activeFrend->fwaBF80);

    if (0 == bufAddr)
        {
        fprintf(stderr, "(msufrend) get80: no free buffers\n");
        }

    return bufAddr;
    }

/*--- function get240 ----------------------
 *  Exit:   Returns the address of a free 240-character buffer.
 */
static FrendAddr get240()
    {
    FrendAddr bufAddr = removeFromBottomOfList(activeFrend->fwaBF240);

    if (0 == bufAddr)
        {
        fprintf(stderr, "(msufrend) get240: no free buffers\n");
        }

    return bufAddr;
    }

/*--- function getBufferForC ----------------------
 *  Given a C string pointer (NOT in FREND memory),
 *  allocate a FREND buffer, fill it, and return it.
 */
static FrendAddr getBufferForC(const char *szmsg)
    {
    u8 len = strlen(szmsg);
    /* Technically, I should first check len for >80 */
    FrendAddr bufaddr = get80();

    if (len > 80)
        {
        len = 80;     /* //! kludge to play it safe. */
        }
    memcpy(&activeFrend->mem[bufaddr + L_DTAHDR], szmsg, len);
    setByte(bufaddr + C_DHBCT, (u8)(len + L_DTAHDR));

    return bufaddr;
    }

/*------------------------------------------------------------*/
static void initLmbi()
    {
    HalfWord  nBytes, nSlots, islot;
    FrendAddr curTableFwa       = FWAMBI_1 + 0x1000;
    FrendAddr curLmbiTableEntry = FWAMBI_1;

    /* The below was generated by SetupLMBIPointerTable.awk */

    assert(PW_MISC == curLmbiTableEntry);
    activeFrend->fwaMISC = curTableFwa;
    setFullWord(curLmbiTableEntry + W_PWFWA, curTableFwa);
    setHalfWord(curLmbiTableEntry + H_PWLE, L_MISC);
    setHalfWord(curLmbiTableEntry + H_PWNE, 1);
#if DEBUG
    fprintf(msufrendLog, "\nEntry for MISC  = 0x%x; table FWA=%x", PW_MISC, curTableFwa);
#endif
    curTableFwa       += L_MISC * 1;
    curTableFwa        = ALIGN_FULLWORD(curTableFwa);
    curLmbiTableEntry += L_LMBPT;

    assert(PW_FPCOM == curLmbiTableEntry);
    activeFrend->fwaFPCOM = curTableFwa;
    setFullWord(curLmbiTableEntry + W_PWFWA, curTableFwa);
    setHalfWord(curLmbiTableEntry + H_PWLE, L_FPCOM);
    setHalfWord(curLmbiTableEntry + H_PWNE, 1);
#if DEBUG
    fprintf(msufrendLog, "\nEntry for FPCOM = 0x%x; table FWA=%x", PW_FPCOM, curTableFwa);
#endif
    curTableFwa       += L_FPCOM * 1;
    curTableFwa        = ALIGN_FULLWORD(curTableFwa);
    curLmbiTableEntry += L_LMBPT;

    assert(PW_BF80 == curLmbiTableEntry);
    activeFrend->fwaBF80 = curTableFwa;
    setFullWord(curLmbiTableEntry + W_PWFWA, curTableFwa);
    setHalfWord(curLmbiTableEntry + H_PWLE, 4);
    nSlots = 40;
    nBytes = initCircList(activeFrend->fwaBF80, nSlots);
    setHalfWord(curLmbiTableEntry + H_PWNE, (HalfWord)(nBytes / 4));
#if DEBUG
    fprintf(msufrendLog, "\nEntry for BF80  = 0x%x; table FWA=%x", PW_BF80, curTableFwa);
#endif
    curTableFwa       += nBytes;
    curTableFwa        = ALIGN_FULLWORD(curTableFwa);
    curLmbiTableEntry += L_LMBPT;

    assert(PW_BF240 == curLmbiTableEntry);
    activeFrend->fwaBF240 = curTableFwa;
    setFullWord(curLmbiTableEntry + W_PWFWA, curTableFwa);
    setHalfWord(curLmbiTableEntry + H_PWLE, 4);
    nBytes = initCircList(activeFrend->fwaBF240, nSlots);
    setHalfWord(curLmbiTableEntry + H_PWNE, (HalfWord)(nBytes / 4));
#if DEBUG
    fprintf(msufrendLog, "\nEntry for BF240 = 0x%x; table FWA=%x", PW_BF240, curTableFwa);
#endif
    curTableFwa       += nBytes;
    curTableFwa        = ALIGN_FULLWORD(curTableFwa);
    curLmbiTableEntry += L_LMBPT;

    assert(PW_BFREL == curLmbiTableEntry);
    activeFrend->fwaBFREL = curTableFwa;
    setFullWord(curLmbiTableEntry + W_PWFWA, curTableFwa);
    setHalfWord(curLmbiTableEntry + H_PWLE, 4);
    nSlots += nSlots; /* Allocate enough space for all 80 and 240-char buffers */
    nBytes  = initCircList(activeFrend->fwaBFREL, nSlots);
    setHalfWord(curLmbiTableEntry + H_PWNE, nSlots);
#if DEBUG
    fprintf(msufrendLog, "\nEntry for BFREL = 0x%x; table FWA=%x", PW_BFREL, curTableFwa);
#endif
    curTableFwa       += nBytes;
    curTableFwa        = ALIGN_FULLWORD(curTableFwa);
    curLmbiTableEntry += L_LMBPT;

    assert(PW_BANM == curLmbiTableEntry);
    activeFrend->fwaBANM = curTableFwa;
    setFullWord(curLmbiTableEntry + W_PWFWA, curTableFwa);
    setHalfWord(curLmbiTableEntry + H_PWLE, LE_BANM);
    setHalfWord(curLmbiTableEntry + H_PWNE, NE_BANM);
#if DEBUG
    fprintf(msufrendLog, "\nEntry for BANM  = 0x%x; table FWA=%x", PW_BANM, curTableFwa);
#endif
    curTableFwa       += LE_BANM * NE_BANM;
    curTableFwa        = ALIGN_FULLWORD(curTableFwa);
    curLmbiTableEntry += L_LMBPT;

    assert(PW_LOGM == curLmbiTableEntry);
    activeFrend->fwaLOGM = curTableFwa;
    setFullWord(curLmbiTableEntry + W_PWFWA, curTableFwa);
    setHalfWord(curLmbiTableEntry + H_PWLE, LE_LOGM);
    setHalfWord(curLmbiTableEntry + H_PWNE, NE_LOGM);
#if DEBUG
    fprintf(msufrendLog, "\nEntry for LOGM  = 0x%x; table FWA=%x", PW_LOGM, curTableFwa);
#endif
    curTableFwa       += LE_LOGM * NE_LOGM;
    curTableFwa        = ALIGN_FULLWORD(curTableFwa);
    curLmbiTableEntry += L_LMBPT;

    assert(PW_SOCK == curLmbiTableEntry);
    activeFrend->fwaSOCK = curTableFwa;
    setFullWord(curLmbiTableEntry + W_PWFWA, curTableFwa);
    setHalfWord(curLmbiTableEntry + H_PWLE, LE_SOCK);
    setHalfWord(curLmbiTableEntry + H_PWNE, activeFrend->portCount);
#if DEBUG
    fprintf(msufrendLog, "\nEntry for SOCK  = 0x%x; table FWA=%x", PW_SOCK, curTableFwa);
#endif
    curTableFwa       += LE_SOCK * activeFrend->portCount;
    curTableFwa        = ALIGN_FULLWORD(curTableFwa);
    curLmbiTableEntry += L_LMBPT;

    assert(PW_DVSK == curLmbiTableEntry);
    activeFrend->fwaDVSK = curTableFwa;
    setFullWord(curLmbiTableEntry + W_PWFWA, curTableFwa);
    setHalfWord(curLmbiTableEntry + H_PWLE, 2);
    setHalfWord(curLmbiTableEntry + H_PWNE, 5);
#if DEBUG
    fprintf(msufrendLog, "\nEntry for DVSK  = 0x%x; table FWA=%x", PW_DVSK, curTableFwa);
#endif
    curTableFwa       += 2 * 5;
    curTableFwa        = ALIGN_FULLWORD(curTableFwa);
    curLmbiTableEntry += L_LMBPT;

    assert(PW_PORT == curLmbiTableEntry);
    activeFrend->fwaPORT = curTableFwa;
    setFullWord(curLmbiTableEntry + W_PWFWA, curTableFwa);
    setHalfWord(curLmbiTableEntry + H_PWLE, LE_PORT);
    setHalfWord(curLmbiTableEntry + H_PWNE, 6);
#if DEBUG
    fprintf(msufrendLog, "\nEntry for PORT  = 0x%x; table FWA=%x", PW_PORT, curTableFwa);
#endif
    curTableFwa       += LE_PORT * 6;
    curTableFwa        = ALIGN_FULLWORD(curTableFwa);
    curLmbiTableEntry += L_LMBPT;

    assert(PW_PTBUF == curLmbiTableEntry);
    activeFrend->fwaPTBUF = curTableFwa;
    setFullWord(curLmbiTableEntry + W_PWFWA, curTableFwa);
    nBytes = 2000; /* total # of bytes for all circ lists */
    setHalfWord(curLmbiTableEntry + H_PWLE, nBytes);
    setHalfWord(curLmbiTableEntry + H_PWNE, 5);
#if DEBUG
    fprintf(msufrendLog, "\nEntry for PTBUF = 0x%x; table FWA=%x", PW_PTBUF, curTableFwa);
#endif
    curTableFwa       += nBytes;
    curTableFwa        = ALIGN_FULLWORD(curTableFwa);
    curLmbiTableEntry += L_LMBPT;

    assert(PW_MALC == curLmbiTableEntry);
    activeFrend->fwaMALC = curTableFwa;
    setFullWord(curLmbiTableEntry + W_PWFWA, curTableFwa);
    setHalfWord(curLmbiTableEntry + H_PWLE, LE_MALC);
    setHalfWord(curLmbiTableEntry + H_PWNE, 5);
#if DEBUG
    fprintf(msufrendLog, "\nEntry for MALC  = 0x%x; table FWA=%x", PW_MALC, curTableFwa);
#endif
    curTableFwa       += LE_MALC * 5;
    curTableFwa        = ALIGN_FULLWORD(curTableFwa);
    curLmbiTableEntry += L_LMBPT;

    /* Carve out buffers from the end here and insert them
     * into the 80-char and 240-char buffer circular lists. */
    nSlots       = getHalfWord(activeFrend->fwaBF80 + H_CIRCLIST_N_SLOTS_TOT);
    activeFrend->fwaBuffers80 = curTableFwa;
    for (islot = 0; islot < nSlots; islot++)
        {
        addToTopOfList(activeFrend->fwaBF80, curTableFwa);
        curTableFwa += LE_BF80;
        }

    nSlots        = getHalfWord(activeFrend->fwaBF240 + H_CIRCLIST_N_SLOTS_TOT);
    activeFrend->fwaBuffers240 = curTableFwa;
    for (islot = 0; islot < nSlots; islot++)
        {
        addToTopOfList(activeFrend->fwaBF240, curTableFwa);
        curTableFwa += LE_BF240;
        }

    assert(PW_ALLOC == curLmbiTableEntry);
    activeFrend->fwaALLOC = curTableFwa;
    setFullWord(curLmbiTableEntry + W_PWFWA, curTableFwa);
    setHalfWord(curLmbiTableEntry + H_PWLE, LE_BF80);
    setHalfWord(curLmbiTableEntry + H_PWNE, 5);
#if DEBUG
    fprintf(msufrendLog, "\nEntry for ALLOC = 0x%x; table FWA=%x", PW_ALLOC, curTableFwa);
#endif
    curTableFwa       += LE_BF80 * 5;
    curTableFwa        = ALIGN_FULLWORD(curTableFwa);
    curLmbiTableEntry += L_LMBPT;

    /* End of code generated by SetupLMBIPointerTable.awk. */

    /*//! This is a kludge to set a non-zero address.  Maybe I'll leave it in. */
    setFullWord(activeFrend->fwaFPCOM + W_NBF80, addrFrendTo1FP(get80()));
    setFullWord(activeFrend->fwaFPCOM + W_NBF240, addrFrendTo1FP(get240()));
    }

/*--- function returnBuffersInReleaseList ----------------------
 *  Return buffers in the release list to their original
 *  list of available buffers.  There are two such lists,
 *  for 80 and 240-byte buffers.
 *  Note:  FREND's approach is more complex.  A given piece
 *  of memory can be dynamically allocated as part of an
 *  80- or 240-byte buffer as needed.  A separate ID mechanism
 *  is used to keep track of what length buffer it is.
 *  This is not done in frend3.
 */
static void returnBuffersInReleaseList()
    {
    FrendAddr bufaddr;
    int       nFreed = 0;

    while (0 != (bufaddr = removeFromBottomOfList(activeFrend->fwaBFREL)))
        {
        if (bufaddr < activeFrend->fwaBuffers240)
            {
            addToTopOfList(activeFrend->fwaBF80, bufaddr);
            }
        else
            {
            addToTopOfList(activeFrend->fwaBF240, bufaddr);
            }
        nFreed++;
        }
    }

/*--- function initPortFirstTime ----------------------
 *  Initialize a port table entry.  Called only once per port at startup.
 *
 *  Entry:  fwaList  is the FWA of a place to create two consecutive
 *                   circular lists for this port (in and out buffers).
 *          portNum  is the port #.  It can be control or data.
 *  Exit:   Returns the number of PTBUF bytes allocated to this port.
 */
static HalfWord initPortFirstTime(FrendAddr fwaList, HalfWord portNum)
    {
    HalfWord nBytes;
    HalfWord nInBufs, nOutBufs;

    if (portNum <= PTN_MAX)
        {
        /* control port */
        nInBufs  = L_CPIN;
        nOutBufs = L_CPOT;
        }
    else
        {
        /* data port */
        nInBufs  = L_DTIN;
        nOutBufs = L_DTOT;
        }
    nBytes = initCircList(fwaList, nInBufs);
    setPortFullWord(portNum, W_PTINCL, fwaList);

    fwaList += nBytes;
    nBytes += initCircList(fwaList, nOutBufs);
    setPortFullWord(portNum, W_PTOTCL, fwaList);

    return nBytes;
    }

/*--- function initPortBufs ----------------------
 *  Initialize the circular lists for the ports, and the pointers
 *  from the ports to these circular lists.
 *  Then do the same for sockets.
 */
static void initPortBufs()
    {
    /* Do ports. */
    HalfWord  port;
    FrendAddr fwaList = activeFrend->fwaPTBUF;
    HalfWord  nBytes  = initPortFirstTime(fwaList, PTN_MAN);

    fwaList += nBytes;
    for (port = FPORTCONSOLE; port <= activeFrend->portCount; port++)
        {
        nBytes   = initPortFirstTime(fwaList, port);
        fwaList += nBytes;
        }
    }

/*---  Beginning of non-initialization code.  ----------------*/

/*--- function PUTBUF ----------------------
 *  Return a buffer to the free list.
 */
static void PUTBUF(FrendAddr bufaddr)
    {
    setFullWord(bufaddr, 0); /* Zero first word of buffer */
    addToTopOfList(activeFrend->fwaBFREL, bufaddr);
    }

/*--- function FMTOPEN ----------------------
 *  Format an FP.OPEN message to send to 1FP, indicating a new connection.
 *
 *  FP.OPEN  8/PN, 8/OT, 16/OID, 8/DCP, 8/DID
 *    PN = 7/32 DATA PORT NUMBER
 *    OT = OPEN ORIGINATOR TYPE (OT.XX)
 *    OID = ID SUPPLIED BY OPEN ORIGINATOR, (MEANT
 *          TO BE RETURNED IN ORSP)
 *    DCP = DESTINATION CONTROL PORT (CTL.X)
 *    DID = DESTINATION TYPE (OT.X)
 *
 *  Entry:   ctlPortNum  is MANAGER's control port number (1)
 *           dataPortNum is the data port number
 *           socketNum   is the socket number.
 */
static FrendAddr FMTOPEN(HalfWord ctlPortNum, HalfWord dataPortNum, HalfWord socketNum)
    {
    FrendAddr addr = get80();

    setByte(addr + C_FPP5, (u8)ctlPortNum);
    setByte(addr + C_FPPT, (u8)dataPortNum);
    setByte(addr + C_FPP2, (u8)OT_1200);
    setHalfWord(addr + C_FPP3, socketNum);
    setByte(addr + C_FPP6, 0); /* DID = 0 */
    /* set fields in record header */
    setByte(addr + C_DHBCT, NP_OPEN + LE_DTAHDR);
    setByte(addr + C_DHTYPE, FP_OPEN);
    setByte(addr + C_DHCHC, 0);
    setByte(addr + C_DHCTL, 0);

    return addr;
    }

/*--- function ADDPORT ----------------------
 *  Add a message buffer address to the output queue for a port.
 *  Corresponds to FREND's ADDPORT.
 *  Formerly named AddMsgToPort.
 */
static void ADDPORT(PortContext *pp, FrendAddr fwaMsg)
    {
    FrendAddr fwaMyPort = portNumToFwa(pp->id);
    FrendAddr fwaList   = getFullWord(fwaMyPort + W_PTINCL);

    addToTopOfList(fwaList, fwaMsg);

    /* Check to make sure that there is a non-zero buffer address
     * in W.PTIN for that control port.  If not, then remove a msg from
     * bottom of list and put in W.PTIN.  (I suppose in most cases, this
     * will be the message we just added.) */
    /* We should get and then drop H.PTINIK, but we'll ignore this interlock for now. */
    if (0 == getFullWord(fwaMyPort + W_PTIN))
        {
        /* No outbound buffer--so get one. */
        if (!isListEmpty(fwaList))
            {
            FullWord bufAddr = removeFromBottomOfList(fwaList);
            bufAddr = addrFrendTo1FP(bufAddr);
            setFullWord(fwaMyPort + W_PTIN, bufAddr);
            }
        }
    }

/*--- function SENDPT ----------------------
 *  Send a buffer to a port.
 */
static void SENDPT(PortContext *pp, FrendAddr fwaMySocket, FrendAddr fwaMsg)
    {
    FrendAddr fwaMyPort = portNumToFwa(pp->id);

    ADDPORT(pp, fwaMsg);
    taskSENDCP(pp->id, FP_INBS);
    }

/*--- function GETINBF ----------------------
 *  Assign a new buffer to the socket input.
 */
static FrendAddr GETINBF(FrendAddr fwaMySock)
    {
    FrendAddr bufaddr = get240();

    setFullWord(fwaMySock + W_SKINBF, bufaddr);
    /* Empty buffer has length == header size */
    setByte(bufaddr + C_DHBCT, L_DTAHDR);

    if (!HFLAGISSET(fwaMySock, SKINEL))
        {
        /* The "No EOL" flag is not set, so set EOL flag in socket */
        setByte(bufaddr + C_DHCNEW, V_DHCNEW);
        }
    /* Input char count */
    setHalfWord(fwaMySock + H_SKINCC, 0);

    return bufaddr;
    }

/*--- function getFrendVersionMsg ----------------------
 *  Entry:  Returns the address of a freshly-allocated 80-byte
 *          message containing text to show to the user.
 */
static FrendAddr getFrendVersionMsg(PortContext *pp)
    {
    FullWord  len;
    char      dateTime[24], versionMsg[80];
    time_t    mytime = time(NULL);
    struct tm *ptm   = localtime(&mytime);

    /* Should look like:
     *  ddddddddddtttttttttt MSU-Frend   xx.yy   ssssssssss    pppppppp
     * 123456789a123456789b123456789c123456789d123456789e123456789f123456789
     * where the dddddddddd date starts with a blank.
     * This apparently does not match 100% with the text in ISUG p 1.3.
     */
    strftime(dateTime, sizeof(dateTime), "%m/%d/%y %H:%M:%S", ptm);
    len = sprintf(versionMsg, "  %s  MSU-Frend3  %s   Socket=%3d", dateTime, frendVersion, pp->id);

    return getBufferForC(versionMsg);
    }

/*--- function writeToSocketWithCC ----------------------
 *  This needs to be reworked eventually.
 *  It's similar to "CARRC     SUBR" and "INTCC     SUBR"
 *  but probably overly simplistic.
 */
static void writeToSocketWithCC(PortContext *pp, FrendAddr fwaMySocket, FrendAddr fwaMsg)
    {
    int  ic, len = getByte(fwaMsg + C_DHBCT), start = L_DTAHDR;
    u8   CarrCtl = 0;
    bool bDoCarrCtl = TRUE;
    int  nOutBytes = 0, bytesSent;

    if (!pp->EOLL)
        {
        CarrCtl    = getByte(fwaMsg + C_DHCNEW);
        bDoCarrCtl = FALSE;
        if (CarrCtl & V_DHCNEW)
            {
            bDoCarrCtl = TRUE;
            }
        }
    /*//! not sure about length check here--but we mustn't treat garbage as CC */
    if (bDoCarrCtl && (len > L_DTAHDR))
        {
        u8 cc = getByte(fwaMsg + L_DTAHDR);
        start++;
        if ('0' == cc)
            {
            pp->pbuf.buf[nOutBytes++] = '\r';
            pp->pbuf.buf[nOutBytes++] = '\n';
            pp->pbuf.buf[nOutBytes++] = '\n';
            }
        else
            {
            /*
             * This case is mostly for space, but it appears that MANAGER
             * sends 'D' as a CC when you enter linenum=text, so we treat
             * ALL other characters as a space carriage control.
             */
            pp->pbuf.buf[nOutBytes++] = '\r';
            pp->pbuf.buf[nOutBytes++] = '\n';
            }
        }
    /* Now output the data bytes in the line */
    for (ic = start; ic < len; ic++)
        {
        pp->pbuf.buf[nOutBytes++] = getByte(fwaMsg + ic);
        }
    bytesSent = sendToFSock(pp, pp->pbuf.buf, nOutBytes);
    if (bytesSent <= 0)
        {
        if (EWOULDBLOCK == getLastOSError())
            {
            bytesSent = 0;
            }
        }
    pp->pbuf.first     = bytesSent;
    pp->pbuf.charsLeft = nOutBytes - bytesSent;

    PUTBUF(fwaMsg);

    /* If we sent the entire buffer,
     * simulate a CCB end-of-output interrupt by calling the
     * socket output task again to send the next line.
     * If there's no more data to be sent, it won't do anything.
     * If there IS more data to be sent, don't do anything.
     * We'll rely on the select() on writeFds to restart the send.
     */
    if (0 == pp->pbuf.charsLeft)
        {
        taskSKOTCL(pp, fwaMySocket);
        }
    }

/*--- function OTNEUP ----------------------
 *  UPDATE PTOTNE FIELD IN PORT.
 *  IF OUTPUT LIST HAS L.DTOT FREE SLOTS,
 *     SEND FP.OTBS.
 */
static void OTNEUP(HalfWord portNum, FrendAddr fwaMyPort)
    {
    FrendAddr fwaList     = getFullWord(fwaMyPort + W_PTOTCL);
    HalfWord  nSlotsAvail = getListFreeEntries(fwaList);

    setHalfWord(fwaMyPort + H_PTOTNE, nSlotsAvail);
    if (nSlotsAvail >= L_DTOT)
        {
        bool bSendOTBS = FALSE;
        /* All port slots are available.  */
        /* Tell MANAGER unless there's an OTBS already pending. */
        INTRLOC(fwaMyPort + H_PTNDIK); /* Not sure I really need this */
        if (!HFLAGISSET(fwaMyPort, PTOTBS))
            {
            /* There is NOT an OTBS already pending. */
            SETHFLAG(fwaMyPort, PTOTBS);
            bSendOTBS = TRUE;
            }
        dropInterlock(fwaMyPort + H_PTNDIK);

        if (bSendOTBS)
            {
            taskSENDCP(portNum, FP_OTBS);
            }
        }
    }

/*--- function READPT ----------------------
 *  Try to get a line from the port list, if available.
 *  Exit:   Returns a buffer obtained from the port, else 0.
 */
static FrendAddr READPT(FrendAddr fwaMySocket)
    {
    FrendAddr bufaddr = 0, fwaMyPort, fwaList;

    do  /* NOT a loop--just a way to break out of a block */
        {
        HalfWord portNum = getHalfWord(fwaMySocket + H_SKCN1);
        if (0 == portNum)
            {
            break;
            }
        fwaMyPort = portNumToFwa(portNum);
        fwaList   = getFullWord(fwaMyPort + W_PTOTCL);
        bufaddr   = removeFromBottomOfList(fwaList);
        if (!bufaddr)
            {
            break;
            }
        /* We removed a buffer.  Update H_PTOTNE */
        OTNEUP(portNum, fwaMyPort);
        } while (FALSE);

    return bufaddr;
    }

/*--- function GETDATA ----------------------
 *  Get the next output buffer destined for this socket.
 *  Entry:  fwaMySocket points to the socket.
 *
 *  Exit:   returns the fwa of a buffer to send, else 0 if none.
 */
static FrendAddr GETDATA(PortContext *pp, FrendAddr fwaMySocket)
    {
    FrendAddr bufaddr;
    FrendAddr fwaList = fwaMySocket + W_SKOTCL; /* Not a pointer */
    u8     charCode = 0xe5, ctlFlags = 0xe5 /*//! debug */;

    pp->EOLL = FALSE;

    /* Try to get next line in socket */
    bufaddr = removeFromBottomOfList(fwaList);
    /* If nothing in socket; try to get from port */
    if (!bufaddr)
        {
        bufaddr = READPT(fwaMySocket);
        }

    if (bufaddr)
        {
        /*//! I added this code because during LOGIN to a restricted user,
         * a front-end command is sent in between two halves of a line.
         * Don't process EOL flags if it's a FE command.
         */
        u8 recType = getByte(bufaddr + C_DHTYPE);
        if (!((FP_FECNE == recType) || (FP_FEC == recType)))
            {
            ctlFlags = getByte(bufaddr + C_DHCTL);
            pp->EOLL = HFLAGISSET(fwaMySocket, SKOEOL);
            /* CLEAR PREVIOUS EOL FLAG */
            CLEARHFLAG(fwaMySocket, SKOEOL);
            /* Set EOL flag based on fields in header.  Weird stuff. */
            charCode = getByte(bufaddr + C_DHCHC);
            if ((CC_FDCAS == charCode) || (CC_FDCBI == charCode))
                {
                SETHFLAG(fwaMySocket, SKOEOL);
                }
            else
                {
                if (ctlFlags & V_DHCEOL)
                    {
                    SETHFLAG(fwaMySocket, SKOEOL);
                    }
                }
            }
        }

    return bufaddr;
    }

/*--- function CKTHRSH ----------------------
 *  CHECK PORT DATA THRESHOLD
 *  This is a simplication of the FREND routine "CKTHRSH   SUBR".
 *  It assumes the port is interactive.
 *  EXIT     Returns  1 IF THE PORT IS BELOW NEED-DATA THRESHOLD
 *                    0 IF THE PORT IS ABOVE THRESHOLD
 */
static int CKTHRSH(FrendAddr fwaMyPort)
    {
    HalfWord nEmptySlots = getHalfWord(fwaMyPort + H_PTOTNE);
    int      BelowThres  = 0;

    if (nEmptySlots >= L_DTOT)
        {
        BelowThres = 1;
        }

    return BelowThres;
    }

/*--- function sendOTBSIfNecessary ----------------------
 */
static void sendOTBSIfNecessary(HalfWord portNum, FrendAddr fwaMyPort, bool bIsExternal)
    {
    /* Don't send an OTBS unless either CHTHRSH says to,
     * or PTOTBS is set.  */
    bool  bSendOTBS   = FALSE;
    bool  bBelowThres = CKTHRSH(fwaMyPort);
    u8 MsgCode     = FP_OTBS;

    INTRLOC(fwaMyPort + H_PTNDIK);
    if (!HFLAGISSET(fwaMyPort, PTOTBS) || bBelowThres)
        {
        SETHFLAG(fwaMyPort, PTOTBS);
        bSendOTBS = TRUE;
        }
    dropInterlock(fwaMyPort + H_PTNDIK);
    if (bIsExternal)
        {
        MsgCode |= V_EXTREQ;
        }
    if (bSendOTBS)
        {
        taskSENDCP(portNum, MsgCode);
        }
    }

/*---  Routines that were tasks in the original FREND --------*/

/*--- function taskMSGCP ----------------------
 *  SEND A PRE-FORMATTED MESSAGE TO CONTROL PORT.
 *  This is basically a wrapper to ADDPORT.
 */
static void taskMSGCP(PortContext *pp, FrendAddr fwaMsg)
    {
    ADDPORT(pp, fwaMsg);
    }

/*--- function CHKACT ----------------------
 *  Check for output activity.
 *  The purpose is to check to see if the socket is too busy
 *  to take another line.  frend3 needs to do this a bit differently
 *  than FREND due to the difference between TCP sockets (which
 *  are buffered by the application) and serial port CCBs
 *  (which are apparently buffered by the hardware).
 *
 *  Exit:   Returns  FALSE if we are too busy to send another line.
 */
static bool CHKACT(PortContext *pp, FrendAddr fwaMySocket)
    {
    return pp->pbuf.charsLeft < 1;
    }

/*--- function taskSKOTCL ----------------------
 *  Socket Output Control.
 *  Gets a buffer of data for this socket and sends it to the terminal.
 */
static void taskSKOTCL(PortContext *pp, FrendAddr fwaMySocket)
    {
    FrendAddr bufaddr;

    if (pp == NULL) return;

    /* If there are pending output characters, don't send more lines. */
    if (CHKACT(pp, fwaMySocket) == FALSE)
        {
        return;
        }

    bufaddr = GETDATA(pp, fwaMySocket);
    if (bufaddr)
        {
        u8 recType = getByte(bufaddr + C_DHTYPE);
        if (FP_BULK == recType)
            {
            SETHFLAG(fwaMySocket, SKSUPE); /* Set Suppress Echo flag */
            }
        if ((FP_FECNE == recType) || (FP_FEC == recType))
            {
            char  buf[256];
            u8 len = getByte(bufaddr + C_DHBCT) - L_DTAHDR;
            memcpy(buf, &activeFrend->mem[bufaddr + L_DTAHDR], len);
            buf[len] = '\0';
            /* //! Should we call PUTBUF with bufaddr here? */
            PUTBUF(bufaddr);
            }
        else
            {
            writeToSocketWithCC(pp, fwaMySocket, bufaddr);
            }
        }
    }

/*--- function taskSOCMSG ----------------------
 * Cause a message to be sent to a socket.
 */
static void taskSOCMSG(PortContext *pp, FrendAddr fwaMsg)
    {
    FullWord nchars = (FullWord)getByte(fwaMsg + C_DHBCT) - L_DTAHDR;

    sendToFSock(pp, &activeFrend->mem[fwaMsg + L_DTAHDR], nchars);
    sendToFSock(pp, (u8 *)"\r\n", 2);
    /* //! really should call this: taskSKOTCL(pp, fwaMsg); */
    }

/*--- function CLRSOC ----------------------
 *  CLeaR SOCket.  See "CLRSOC    SUBR".
 */
static void CLRSOC(FrendAddr fwaMySocket)
    {
    FrendAddr bufaddr;

    setHalfWord(fwaMySocket + H_SKID, 0);
    setFullWord(fwaMySocket + W_SKFLAG, 0); /* Clear all flags */

    /* RETURN ALL BUFFERS ON THE OUTPUT STACK */
    do
        {
        bufaddr = removeFromBottomOfList(fwaMySocket + W_SKOTCL);
        if (!bufaddr)
            {
            break;
            }
        PUTBUF(bufaddr);
        } while (TRUE);

    /* RETURN INPUT BUFFER */
    bufaddr = getFullWord(fwaMySocket + W_SKINBF);
    if (bufaddr)
        {
        setFullWord(fwaMySocket + W_SKINBF, 0);
        PUTBUF(bufaddr);
        }
    }

/*--- function SETPORT ----------------------
 * SETUP A FRESH PORT TABLE ENTRY
 */
static void SETPORT(HalfWord PortNum, HalfWord ctlPortNum)
    {
    FrendAddr fwaList, fwaMyPort = portNumToFwa(PortNum);
    HalfWord  nBufs;

    /* Look for "SETPORT   SUBR" in FREND for the code that inspired what's below. */
    SETHFLAG(fwaMyPort, PTSENB);
    SETHFLAG(fwaMyPort, PTSCNT);
    SETHFLAG(fwaMyPort, PTS65);
    SETHFLAG(fwaMyPort, PTEOL);
    CLEARHFLAG(fwaMyPort, PTOTBS);
    CLEARHFLAG(fwaMyPort, PTXFER);

    /* I don't know the difference between port ID and port number.
     * I will treat them the same. */
    setHalfWord(fwaMyPort + H_PTID, PortNum);  /* port ID */
    setHalfWord(fwaMyPort + H_PTCPN, PTN_MAN); /* set control port number */
    setFullWord(fwaMyPort + W_PTIN, 0);
    setFullWord(fwaMyPort + W_PTOT, 0);
    setFullWord(fwaMyPort + W_PTPBUF, 0);        /* NO PARTIAL LINE BUFFER */
    fwaList = getFullWord(fwaMyPort + W_PTOTCL); /* fwa of circ buffer for output */
    nBufs   = getHalfWord(fwaList + H_CLNUM);    /* total num of buffers in list */
    setHalfWord(fwaMyPort + H_PTOTNE, nBufs);    /* Set all bufs as available */
    dropInterlock(fwaMyPort + H_PTINIK);
    dropInterlock(fwaMyPort + H_PTOTIK);
    dropInterlock(fwaMyPort + H_PTWTBF);
    dropInterlock(fwaMyPort + H_PTNDIK);
    CLEARHFLAG(fwaMyPort, PTTNX3);
    }

/*--- function LINKSOC ----------------------
 *  LINK SOCKET TO PORT
 */
static void LINKSOC(PortContext *pp)
    {
    /*
     * Link socket and port. Socket and port are
     * aligned in this implementation.
     */
    FrendAddr fwaMyPort   = portNumToFwa(pp->id);
    FrendAddr fwaMySocket = sockNumToFwa(pp->id);

    setHalfWord(fwaMySocket + H_SKCN1, pp->id);
    setByte(fwaMySocket + C_SKCT1, CT_PORT);

    setHalfWord(fwaMyPort + H_PTCN1, pp->id);
    setByte(fwaMyPort + C_PTCT1, CT_SOCK);
    CLEARHFLAG(fwaMySocket, SKSUPE); /* Clear suppress echo */
    }

/*--- function SendCP_OTBS ----------------------
 *  Helper routine for SENDCP to send an FP.OTBS (OuTput Buffer Status)
 *
 *  Exit:   Returns 0 if should not send message now; buffer is released
 *                and task should be called again later.
 *             -1  if message should never be sent.  Buffer is released.
 *             else address of buffer to send.
 */
static FrendAddr SendCP_OTBS(HalfWord portNum, FrendAddr fwaMyPort, FrendAddr bufAddr)
    {
    FrendAddr resultAddr = bufAddr;
    /*//! I have ignored some interlocking of H.PTNDIK and other complexity. */
    /* See "OTBS      SUBR" in FREND. */
    /* Get number of free entries in this port. */
    HalfWord nFree = getHalfWord(fwaMyPort + H_PTOTNE);

    setByte(bufAddr + C_FPP2, (u8)nFree);
    /* Set buffer length based on # of parameters (2) */
    setByte(bufAddr + C_DHBCT, L_DTAHDR + NP_OTBS);

    return resultAddr;
    }

/*--- function taskSENDCP ----------------------
 *  Send a message to a control port.
 */
static void taskSENDCP(HalfWord portNum, u8 MsgCode)
    {
    /*//! I'm not sure I'm handling V_EXTREQ right.
     * I am basically ignoring it. */
    u8     MsgCodeWithoutFlag = (u8)(MsgCode & (0xff ^ V_EXTREQ));
    FrendAddr fwaMyPort = portNumToFwa(portNum);
    HalfWord  ctlPort   = getHalfWord(fwaMyPort + H_PTCPN);
    FrendAddr bufaddr   = get80();

    /* Set the message code, clearing the V_EXTREQ bit. */
    setByte(bufaddr + C_DHTYPE, MsgCodeWithoutFlag);

    /* Format the message as per the message code */
    if (FP_INBS == MsgCodeWithoutFlag)
        {
        /* Input Buffer Status */
        /* Set param 2 to # of lines ready for input to 1FP */
        FrendAddr fwaList    = getFullWord(fwaMyPort + W_PTINCL);
        HalfWord  nSlotsUsed = getHalfWord(fwaList + H_CIRCLIST_N_USED);

        /* Count the buffer in W.PTIN if it's non-zero because if it's
         * there, it has been removed from the circular list. */
        if (getFullWord(fwaMyPort + W_PTIN) > 0)
            {
            nSlotsUsed++;
            }
        setByte(bufaddr + C_FPP2, (u8)nSlotsUsed);
        setByte(bufaddr + C_DHBCT, L_DTAHDR + 2);
        }
    else if (FP_OTBS == MsgCodeWithoutFlag)
        {
        SendCP_OTBS(portNum, fwaMyPort, bufaddr);
        }
    else if (FP_CLO == MsgCodeWithoutFlag)
        {
        setByte(bufaddr + C_DHBCT, L_DTAHDR + 2);
        setByte(bufaddr + C_FPP2, 2); /* this means DISCONNECT */
        }
    /* Send message to control port */
    setByte(bufaddr + C_FPPT, (u8)portNum); /* data port number */
    setByte(bufaddr + C_DHCHC, 0);
    setByte(bufaddr + C_DHCTL, 0);
    ADDPORT(activeFrend->ports + (ctlPort - 1), bufaddr);
    }

/*--- function taskSKINCL ----------------------
 *  Socket Input Control - handles lines typed by user.
 */
static void taskSKINCL(FrendAddr fwaMySocket, FrendAddr bufaddr)
    {
    HalfWord portNum = getHalfWord(fwaMySocket + H_SKCN1);

    /* Clear "suppress echo" flag. */
    CLEARHFLAG(fwaMySocket, SKSUPE);
    SENDPT(activeFrend->ports + (portNum - 1), fwaMySocket, bufaddr);
    }

/*--- function DOABT ----------------------
 *  Send an FP.ABT request, typically because the user hit Esc.
 */
static void DOABT(HalfWord portNum, FrendAddr fwaMyPort)
    {
    /* Grab a buffer and format a FP.ABT message in it, then send it. */
    HalfWord  ctlPort = getHalfWord(fwaMyPort + H_PTCPN);
    FrendAddr bufaddr = get80();

    setByte(bufaddr + C_DHBCT, L_DTAHDR + 1);
    setByte(bufaddr + C_DHTYPE, FP_ABT);
    setByte(bufaddr + C_FPPT, (u8)portNum);
    ADDPORT(activeFrend->ports + (ctlPort - 1), bufaddr);
    }

/*--- function ZAPPTO ----------------------
 *  Discard all output lines for this port, that do not
 *  have the NTA flag set.
 */
static void ZAPPTO(HalfWord portNum, FrendAddr fwaMyPort)
    {
    FrendAddr fwaInterlock = fwaMyPort + H_PTOTIK;
    FrendAddr bufaddr;
    HalfWord  nSlotsUsed;

    if (interlockIsFree(fwaInterlock))
        {
        FrendAddr fwaList = getFullWord(fwaMyPort + W_PTOTCL);
        INTRLOC(fwaInterlock);
        nSlotsUsed = getListUsedEntries(fwaList);

        /* It appears that nSlotsUsed is always zero in the console version,
         * so this FREND-derived code is not necessary.  It might be
         * necessary in the TCP/IP version. */
        for ( ; nSlotsUsed > 0; nSlotsUsed--)
            {
            bufaddr = removeFromBottomOfList(fwaList);
            if (!bufaddr)
                {
                break;        /* should never happen */
                }
            if (V_DHCNTA & getByte(bufaddr + C_DHCNTA))
                {
                /* Line should not be discarded, so add it back. */
                addToTopOfList(fwaList, bufaddr);
                }
            else
                {
                /* Discard output line. */
                PUTBUF(bufaddr);
                }
            }
        /* END OF LIST. UPDATE PTOTNE. */
        setHalfWord(fwaMyPort + H_PTOTNE, getListFreeEntries(fwaList));
        dropInterlock(fwaInterlock);

        /* SEND AN OTBS TO THE CONTROL PORT IF ONE HAS NOT
         * ALREADY BEEN SENT. */
        sendOTBSIfNecessary(portNum, fwaMyPort, FALSE);
        }
    }

/*--- function taskINESC ----------------------
 */
static void taskINESC(PortContext *pp)
    {
    FrendAddr fwaMySocket = sockNumToFwa(pp->id);
    HalfWord  portNum     = getHalfWord(fwaMySocket + H_SKCN1);

    if (portNum)
        {
        FrendAddr fwaMyPort = portNumToFwa(portNum);
        /* Clear pending output lines on port. */
        ZAPPTO(portNum, fwaMyPort);
        /*//! Should clear port's input here. */
        DOABT(portNum, fwaMyPort);
        }
    CLEARHFLAG(fwaMySocket, SKESCP); /* ALLOW USER ANOTHER ESCAPE */
    }

/*--- function taskOPENSP ----------------------
 *  OPEN SOCKET TO PORT.
 */
static void taskOPENSP(PortContext *pp)
    {
    SETPORT(pp->id, PTN_MAN);
    LINKSOC(pp);

    /* Create message to send to 1FP */
        {
        FrendAddr fwaMsg = FMTOPEN(PTN_MAN, pp->id, pp->id);
        taskMSGCP(activeFrend->ports + (PTN_MAN - 1), fwaMsg);
        }
    }

/*--- function taskSKINIT ----------------------
 *  SocKet INITialize.  See "SKINIT    TASK".
 */
static void taskSKINIT(HalfWord socketNum)
    {
    FrendAddr addr, fwaMySocket = sockNumToFwa(socketNum);
    FullWord  save;

    setByte(fwaMySocket + C_SKNPCC, '%');
    setHalfWord(fwaMySocket + H_SKINLE, L_LINE); /* max input line length */

    /* CLEAR THE REST OF THE SOCKET, EXCEPT FOR "SKPORD". */
    save = getFullWord(fwaMySocket + W_SKPORD);
    for (addr = fwaMySocket + W_SKFLAG; addr <= fwaMySocket + H_CLTOP; addr += 4)
        {
        setFullWord(addr, 0);
        }
    setFullWord(fwaMySocket + W_SKPORD, save);

    /* INITIALIZE THE SOCKET OUTPUT CIRCULAR LIST */
    /*         (MOSTLY ALREADY DONE) */
    setHalfWord(fwaMySocket + W_SKOTCL + H_CLNUM, L_SKOCL);

    /* SET SOCKET INPUT STATE TO =IDLE= */
    setByte(fwaMySocket + C_SKISTA, IN_IDLE);
    }

/*--- function taskSKOPEN ----------------------
 *  Open a FREND socket.
 */
static void taskSKOPEN(PortContext *pp)
    {
    FrendAddr bufaddr, fwaMySocket = sockNumToFwa(pp->id);

    setByte(fwaMySocket + C_SKISTA, IN_IO); /* set input state */

    taskSKINIT(pp->id);

    /* This code is from task SKWTNQ */
    taskOPENSP(pp);
    bufaddr = getFrendVersionMsg(pp);
    taskSOCMSG(pp, bufaddr);
    }

/*--- function taskSKCARR ----------------------
 *  "Carrier" has been detected (i.e., we are accepting a new connection).
 */
static void taskSKCARR(PortContext *pp)
    {
    FrendAddr fwaMySocket = sockNumToFwa(pp->id);

    /* //! for the time being, we are setting socketID = socket num */
    setHalfWord(fwaMySocket + H_SKID, pp->id);
    taskSKOPEN(pp);
    }

/*--- function DRPCON ----------------------
 *  Drop a socket's connections.
 *
 *  Entry:  callingConn is the # of the port or socket which
 *                owns the connections.
 *
 */
static void DRPCON(HalfWord callingConn, u8 connType, HalfWord numPortOrSock)
    {
    PortContext *pp;

    /*
     * We depart from FREND code and assume that callingConn is a socket,
     * and that we need to close it.
     *
     * It appears that connType is 0 when we are called due to LOGOUT,
     * and CT_PORT when we are called due to client disconnect.
     */
    if (CT_PORT == connType)
        {
        FrendAddr fwaMyPort = portNumToFwa(numPortOrSock);
        setByte(fwaMyPort + C_PTCT1, 0);
        setHalfWord(fwaMyPort + H_PTCN1, 0);
        CLEARHFLAG(fwaMyPort, PTSCNT);
        /*//! FREND checks for waiting output before deciding to call SENDCP */
        taskSENDCP(numPortOrSock, FP_CLO);
        }
    pp = activeFrend->ports + (callingConn - 1);
    if (pp != NULL)
        {
#if defined(_WIN32)
        closesocket(pp->fd);
#else
        close(pp->fd);
#endif
        pp->active = FALSE;
        }
    }

/*--- function taskCLOFSK ----------------------
 *  CLOSE FROM SOCKET (DISCONNECT)
 */
static void taskCLOFSK(HalfWord socketNum, FrendAddr fwaMySocket)
    {
    if (0 == socketNum)
        {
        return;
        }

    /*//! We should check for pending output and call ourself
     * with a delay if necessary. */
    DRPCON(socketNum, getByte(fwaMySocket + C_SKCT1), getHalfWord(fwaMySocket + H_SKCN1));
    /* Clear the connection */
    setByte(fwaMySocket + C_SKCT1, 0);
    setHalfWord(fwaMySocket + H_SKCN1, 0);

    CLRSOC(fwaMySocket);
    taskSKINIT(socketNum);
    }

/*--- function CLRPTS ----------------------
 *  CLEAR PORT TO SOCKET CONNECTION IN THE SOCKET.
 *  This is a simplified version of "CLRPTS    SUBR";
 *  it ignores the second connection in the socket.
 */
static void CLRPTS(HalfWord portNum, FrendAddr fwaMyPort,
            HalfWord socketNum, FrendAddr fwaMySocket)
    {
    setHalfWord(fwaMySocket + H_SKCN1, 0);
    setByte(fwaMySocket + C_SKCT1, 0);
    }

/*--- function CLRPORT ----------------------
 *  CLEAR THE PORT (upon logout or disconnect).
 *  See "CLRPORT   SUBR".
 *  Exit:   Returns the number of FP.CLO BUFFERS found.
 *          Apparently if this is non-zero, it means that
 *          we are going to have to send a FP.CLO.
 */
static HalfWord CLRPORT(HalfWord portNum, FrendAddr fwaMyPort)
    {
    HalfWord  nFPCLO = 0;
    FrendAddr fwaList, bufaddr;

    if (HFLAGISSET(fwaMyPort, PTNDCL))
        {
        nFPCLO = 1;
        }
    /* Clear key fields */
    setHalfWord(fwaMyPort + H_PTFLAG, 0);
    setHalfWord(fwaMyPort + H_PTFLG2, 0);
    setByte(fwaMyPort + C_PTTYPE, 0);
    setByte(fwaMyPort + C_PTCT1, 0);
    setHalfWord(fwaMyPort + H_PTCN1, 0);
    setHalfWord(fwaMyPort + H_PTID, 0);
    setHalfWord(fwaMyPort + H_PTCPN, 0);
    dropInterlock(fwaMyPort + H_PTWTBF);
    CLEARHFLAG(fwaMyPort, PTPEOI); /* Printer EOI */

    /* Return output buffers */
    INTRLOC(fwaMyPort + H_PTOTIK); /* INTERLOCK OUTPUT SIDE */
    fwaList = getFullWord(fwaMyPort + W_PTOTCL);
    do
        {
        bufaddr = removeFromBottomOfList(fwaList);
        if (!bufaddr)
            {
            break;
            }
        if (FP_CLO == getByte(bufaddr + C_DHTYPE))
            {
            nFPCLO++;    /* Increment # of FP.CLO's that we found */
            }
        PUTBUF(bufaddr); /* Return output buffer */
        } while (TRUE);
    dropInterlock(fwaMyPort + H_PTOTIK);

    /* Return the port's input buffers */
    INTRLOC(fwaMyPort + H_PTINIK);
    fwaList = getFullWord(fwaMyPort + W_PTINCL);
    bufaddr = getFullWord(fwaMyPort + W_PTIN);
    if (bufaddr)
        {
        /* We have a next input line.  */
        /* Get its address in FREND format */
        bufaddr = addr1FPToFrend(bufaddr);
        PUTBUF(bufaddr);
        setFullWord(fwaMyPort + W_PTIN, 0); /* Clear input line address */
        }
    /* Loop through input buffers, returning them. */
    do
        {
        bufaddr = removeFromBottomOfList(fwaList);
        if (!bufaddr)
            {
            break;         /* Break if no more buffers in input list */
            }
        PUTBUF(bufaddr);
        } while (TRUE);
    dropInterlock(fwaMyPort + H_PTINIK);

    return nFPCLO;
    }

/*--- function taskCLOFPT ----------------------
 *  CLOSE FROM PORT (LOGOUT).
 *  This is based on "CLOFPT    TASK", but much simpler.
 *  //! FREND does more, like checking for pending output,
 *   and checking port type.
 */
static void taskCLOFPT(HalfWord portNum, u8 CloseType)
    {
    FrendAddr fwaMyPort = portNumToFwa(portNum);

    if (CT_SOCK == getByte(fwaMyPort + C_PTCT1))
        {
        HalfWord  socketNum   = getHalfWord(fwaMyPort + H_PTCN1);
        FrendAddr fwaMySocket = sockNumToFwa(socketNum);
        CLEARHFLAG(fwaMySocket, SKSWOT); /* CLEAR OUTPUT WAIT FOR CLOSE */
        CLRPTS(portNum, fwaMyPort, socketNum, fwaMySocket);
        CLRPORT(portNum, fwaMyPort);
        taskCLOFSK(socketNum, fwaMySocket);
        }
    }

/*--- function PTMSG ----------------------
 *  Issue a "[PORT  xx]" message to the port.
 */
static void PTMSG(PortContext *pp)
    {
    char      msg[64];
    int       len     = sprintf(msg, "[Port%4d]", pp->id);
    FrendAddr bufaddr = getBufferForC(msg);

    taskSOCMSG(pp, bufaddr);
    }

/*--- function procRecTypeCPOPN ----------------------
 *  Process a CPOPN (Control Port OPeN) record type.
 *  Entry:  portNum  is the port number.
 *          bufaddr  is the FREND address of the record from 1FP.
 *                   It starts with a data buffer header, then
 *                   C_FPPT, etc.
 */
static void procRecTypeCPOPN(HalfWord portNum, FrendAddr bufaddr)
    {
    FrendAddr fwaMyPort = portNumToFwa(portNum);

    SETHFLAG(fwaMyPort, PTS65); /* Set "CONNECTED TO 6500" */

    /* Echo message back to the mainframe. */

    /*//! System hangs if I include this.  Should be a
     * taskMSGCP(), but that's just a wrapper to ADDPORT */
    /* ADDPORT(pp, bufaddr); */

    /* This PUTBUF does not appear in "CPOPN     SUBR", which
     * is slightly more complex. */
    PUTBUF(bufaddr);
#if DEBUG
    fprintf(msufrendLog, "\n  CPOPN port:%04x addr:%05x", portNum, bufaddr);
#endif
    }

/*--- function procRecTypeCPCLO ----------------------
 *  Process a CPCLO (Control Port CLOse) record type.
 *  Entry:  portNum  is the port number.
 *          bufaddr  is the FREND address of the record from 1FP.
 *                   It starts with a data buffer header, then
 *                   C_FPPT, etc.
 */
static void procRecTypeCPCLO(HalfWord portNum, FrendAddr bufaddr)
    {
    FrendAddr fwaMyPort = portNumToFwa(portNum);

    PUTBUF(bufaddr);
    CLEARHFLAG(fwaMyPort, PTS65); /* CLEAR CONNECTED TO 6500 */

    /* Not sure about this next one--FREND comments say to do it, but
     * it's not clear it's done in the code. */
    CLEARHFLAG(fwaMyPort, PTSCNT); /* CLEAR "CONNECTED" flag */
#if DEBUG
    fprintf(msufrendLog, "\n  CPCLO port:%04x addr:%05x", portNum, bufaddr);
#endif
    }

/*--- function procRecTypeORSP ----------------------
 *  Process a ORSP (Open ReSPonse) record type.
 *  Entry:  portNum  is the port number.
 *          bufaddr  is the FREND address of the record from 1FP.
 *                   It starts with a data buffer header, then
 *                   C_FPPT, etc.
 */
static void procRecTypeORSP(HalfWord portNum, FrendAddr bufaddr)
    {
    /*
     * FrendAddr fwaMyPort = portNumToFwa(portNum);
     * HalfWord MySocketNum = getHalfWord(fwaMyPort+H_PTCN1);
     */
    PUTBUF(bufaddr);
    PTMSG(activeFrend->ports + (portNum - 1));
#if DEBUG
    fprintf(msufrendLog, "\n  ORSP port:%04x addr:%05x", portNum, bufaddr);
#endif
    }

/*--- function procRecTypeOTBS ----------------------
 *  Process an OTBS (OuTput Buffer Status) command.
 *  This is based on FREND's "OTBS      SUBR" and is similar
 *  to OTNEUP().
 */
static void procRecTypeOTBS(HalfWord portNum, FrendAddr bufaddr, FrendAddr fwaMyPort)
    {
    PUTBUF(bufaddr);
    sendOTBSIfNecessary(portNum, fwaMyPort, TRUE);
#if DEBUG
    fprintf(msufrendLog, "\n  OTBS port:%04x addr:%05x fwa:%05x", portNum, bufaddr, fwaMyPort);
#endif
    }

/*--- function procRecTypeINBS ----------------------
 *  Process an INBS (INput Buffer Status) command.
 *  See "INBS      SUBR" in FREND.
 */
static void procRecTypeINBS(HalfWord portNum, FrendAddr bufaddr)
    {
    PUTBUF(bufaddr);
    taskSENDCP(portNum, (u8)(FP_INBS + V_EXTREQ));
#if DEBUG
    fprintf(msufrendLog, "\n  INBS port:%04x addr:%05x", portNum, bufaddr);
#endif
    }

/*--- function procRecTypeTIME ----------------------
 *  Process a TIME command.
 *  This is supposed to set the FREND time-of-day, but we ignore it.
 */
static void procRecTypeTIME(FrendAddr bufaddr)
    {
    PUTBUF(bufaddr);
#if DEBUG
    fprintf(msufrendLog, "\n  TIME addr:%05x", bufaddr);
#endif
    }

/*--- function procRecTypeCLO ----------------------
 *  Process a CLO (close port) command.
 *  Entry:  dataPort is the data port specified as an argument
 *                   in the command (NOT the control port).
 *          fwaMyPort   is the FWA of that port's entry.
 *          bufaddr  is the FWA of the command buffer.
 */
static void procRecTypeCLO(HalfWord dataPort, FrendAddr fwaMyPort, FrendAddr bufaddr)
    {
    u8 CloseType = getByte(bufaddr + C_FPP2);

    PUTBUF(bufaddr);
    CLEARHFLAG(fwaMyPort, PTS65);
    taskCLOFPT(dataPort, CloseType);
#if DEBUG
    fprintf(msufrendLog, "\n  CLO port:%04x addr:%05x fwa:%05x", dataPort, bufaddr, fwaMyPort);
#endif
    }

/*--- function taskCTLPT ----------------------
 *  Process messages from 1FP on a control port
 */
static void taskCTLPT(HalfWord ctlPort)
    {
    FrendAddr fwaCtlPort = portNumToFwa(ctlPort);
    FrendAddr fwaList    = getFullWord(fwaCtlPort + W_PTOTCL);
    HalfWord  nSlotsAvail;
    FrendAddr bufaddr, fwaMyDataPort;
    u8        recType, dataPort;

    while (TRUE)
        {
        bufaddr = removeFromBottomOfList(fwaList);
        if (!bufaddr)
            {
            break;
            }
        recType       = getByte(bufaddr + C_DHTYPE);
        dataPort      = getByte(bufaddr + C_FPPT);
        fwaMyDataPort = 0;
        if (dataPort)
            {
            fwaMyDataPort = portNumToFwa(dataPort);
            }
        switch (recType)
            {
        case FP_CPOPN:
            procRecTypeCPOPN(ctlPort, bufaddr);
            break;

        case FP_CPCLO:
            procRecTypeCPCLO(ctlPort, bufaddr);
            break;

        case FP_ORSP:
            procRecTypeORSP(dataPort, bufaddr);
            break;

        case FP_OTBS:
            procRecTypeOTBS(dataPort, bufaddr, fwaMyDataPort);
            break;

        case FP_INBS:
            procRecTypeINBS(dataPort, bufaddr);
            break;

        case FP_TIME:
            procRecTypeTIME(bufaddr);
            break;

        case FP_CLO:
            procRecTypeCLO(dataPort, fwaMyDataPort, bufaddr);
            break;

        default:
            break;
            }
        }

    /* Update number of buffers available in the control port.
     * COUNT IS NOT CORRECT AS LONG AS THIS TASK IS RUNNING, BUT
     * IT WILL BE TOO LOW, WHICH IS OK.
     */
    nSlotsAvail = getListFreeEntries(fwaList);
    setHalfWord(fwaCtlPort + H_PTOTNE, nSlotsAvail);
    }

/*--- function KILLBUF ----------------------
 *  Helper function for processing user-typed Cancel or Escape.
 *
 *  Entry:  pp          is the pointer to the PortContext where the user hit Ctrl-X or Esc.
 *          fwaMySocket is the FWA of that socket entry.
 *          bufout      is an already-formatted FREND buffer to send to user.
 */
static void KILLBUF(PortContext *pp, FrendAddr fwaMySocket, FrendAddr bufout)
    {
    FrendAddr fwaList = fwaMySocket + W_SKOTCL;

    setHalfWord(fwaMySocket + H_SKINCC, 0); /* set to 0 chars */
    addToBottomOfList(fwaList, bufout);
    taskSKOTCL(pp, fwaMySocket);
    CLEARHFLAG(fwaMySocket, SKETOG); /* CLEAR TOGGLE ECHOBACK */
    CLEARHFLAG(fwaMySocket, SKOSUP); /* CLEAR OUTPUT SUSPENDED */
    }

/*--- function PALISR ----------------------
 *  Process a character received from the user.
 */
static void PALISR(PortContext *pp, u8 ch)
    {
    FrendAddr fwaMySocket = sockNumToFwa(pp->id);
    FrendAddr bufaddr     = getFullWord(fwaMySocket + W_SKINBF);
    FrendAddr bufout      = 0;
    HalfWord  count;
    /* Echo characters if "suppress echo" is not set. */
    bool doEcho           = !HFLAGISSET(fwaMySocket, SKSUPE);

    if (0 == bufaddr)
        {
        bufaddr = GETINBF(fwaMySocket);
        }

#if DEBUG
    fprintf(msufrendLog, "\n  PALISR port:%04x addr:%05x fwa:%05x ch:%02x", pp->id, bufaddr, fwaMySocket, ch);
#endif

    switch (ch)
        {
    case '\r':
        /* Set buffer length = # of data chars + header len */
        setByte(bufaddr + C_DHBCT, (u8)(L_DTAHDR + getHalfWord(fwaMySocket + H_SKINCC)));
        /* clear socket's char count */
        setHalfWord(fwaMySocket + H_SKINCC, 0);
        /* Handle end of line flag.  Tricky. */
        CLEARHFLAG(fwaMySocket, SKINEL);
        if ((!getByte(bufaddr + C_DHCEOL)) & V_DHCEOL)
            {
            SETHFLAG(fwaMySocket, SKINEL);
            }
        /* Clear socket's input buffer address, since we are now going to use the buffer. */
        setFullWord(fwaMySocket + W_SKINBF, 0);
        taskSKINCL(fwaMySocket, bufaddr);
        break;

    case '\n':
        /* I guess we should ignore LF, because some telnet clients
         * send CR LF when you press Enter.  */
        doEcho = FALSE;
        break;

    case '\b':
        /* backspace: delete previous char, if any. */
        /* //! This will have to be enhanced when we do TCP/IP */
        /* Delete previous char on line, if any. */
        count = getHalfWord(fwaMySocket + H_SKINCC);
        if (count > 0)
            {
            setHalfWord(fwaMySocket + H_SKINCC, (HalfWord)(count - 1));
            }
        else
            {
            doEcho = FALSE;
            }
        break;

    case '\x18': /* CANCEL function (erase current input line) */
        bufout = getBufferForC(" \r\\\\\\\\\r\n");
        KILLBUF(pp, fwaMySocket, bufout);
        doEcho = FALSE;
        break;

    case '\x1b': /* Escape: abort current program and discard input line */
        doEcho = FALSE;
        CLEARHFLAG(fwaMySocket, SKSUPE); /* CLEAR 'SUPPRESS ECHO' FLAG */
        if (!HFLAGISSET(fwaMySocket, SKESCP))
            {
            /* Escape Pending flag not already set */
            SETHFLAG(fwaMySocket, SKESCP);
            taskINESC(pp);
            /* Send a message.  It should include \\\\ if chars have been typed. */
            /*//! I shouldn't have to include carriage controls here. */
            if (getByte(fwaMySocket + H_SKINCC))
                {
                bufout = getBufferForC(" !\r\\\\\\\\\r\n");
                }
            else
                {
                bufout = getBufferForC(" !\r\n");
                }
            KILLBUF(pp, fwaMySocket, bufout);
            }
        break;

    default:
        count = getHalfWord(fwaMySocket + H_SKINCC);
        setByte(bufaddr + L_DTAHDR + count, ch);
        count++;
        if (count >= getHalfWord(fwaMySocket + H_SKINLE))
            {
            /* buffer is full */
            }
        else
            {
            setHalfWord(fwaMySocket + H_SKINCC, count);
            }
        break;
        }

    if (doEcho)
        {
        /* crude echo */
        sendToFSock(pp, &ch, 1);
        }
    }

/*---  Processing of control port commands  ------------------*/

/*--- function cmdControlPortOpen ----------------------
 *  Process the ControlPortOpen command (NOT the FP.CPOPN record type
 *  sent by HEREIS).
 */
static void cmdControlPortOpen()
    {
    u8 port = getByte(activeFrend->fwaFPCOM + C_CPOPT);

    /* Set this port's connection number */
    setPortHalfWord(port, H_PTCN1, 1);
    /* Set this port's number of empty output slots. */
    setPortHalfWord(port, H_PTOTNE, 2);
#if DEBUG
    fprintf(msufrendLog, "\n  ControlPortOpen port:%02x", port);
#endif
    }

/*--- function cmdHereIs ----------------------
 *  Process a HEREIS command from 1FP.
 *  Most commands from the Cyber are HEREIS, with a record type
 *  field in the buffer indicating what is to be done.
 *  So, this function is a gateway to a lot of what goes on in frend3.
 *
 *  Entry:  offsetForBufType   is W_NBF80 or W_NBF240
 */
static void cmdHereIs(HalfWord portNum, int offsetForBufType)
    {
    /* I don't understand the difference between FPCOM's H.FCMDPT and
     * the buffer's C.FPPT.  Sometimes the former is the control port
     * and the latter the data port. */
    FrendAddr newaddr;
    HalfWord  nSlotsAvail, cmdPort;
    FrendAddr fwaMyPort = portNumToFwa(portNum);
    FrendAddr fwaList   = getFullWord(fwaMyPort + W_PTOTCL);
    FrendAddr bufaddr   = addr1FPToFrend(getFullWord(activeFrend->fwaFPCOM + offsetForBufType));
    HalfWord  portRec   = getByte(bufaddr + C_FPPT);
    u8        recType   = getByte(bufaddr + C_DHTYPE);

#if DEBUG
    fprintf(msufrendLog, "\n  HereIs %s recType:%02x", offsetForBufType == W_NBF80 ? "W_NBF80" : "W_NBF240", recType);
#endif

    /* Clear next buffer interlock */
    dropInterlock(activeFrend->fwaFPCOM + H_NBUFIK);

    /* Put a fresh buffer into W_NBF80 or 240 */
    /* Byte count must be zero to make 1FP happy. */
    /* See end of 1FP "GETOBUF   ENTRY." */
    newaddr = (offsetForBufType == W_NBF80) ? get80() : get240();
    setByte(newaddr + C_DHBCT, 0);
    setFullWord(activeFrend->fwaFPCOM + offsetForBufType, addrFrendTo1FP(newaddr));

    /* Add this newly-received buffer to the list for this port. */
    /* Seems to me we should add to *top* of list; but there is */
    /* conditional code at "HEREIS    .." in FREND to add to bottom of list */
    /* and that is what works. */
    if (getByte(fwaMyPort + C_DHCASY) & V_DHCASY)
        {
        /* This is an "asynchronous" message, so add to bottom of list */
        addToBottomOfList(fwaList, bufaddr);
        }
    else
        {
        /* Normal message, so add to top of list */
        addToTopOfList(fwaList, bufaddr);
        }

    /* Set number of available entries in port */
    nSlotsAvail = getListFreeEntries(fwaList);
    setHalfWord(fwaMyPort + H_PTOTNE, nSlotsAvail);

    /* Clear OUTPUT BUFFER INTERLOCK for the command port */
    dropInterlock(fwaMyPort + H_PTOTIK);

    cmdPort = getHalfWord(activeFrend->fwaFPCOM + H_FCMDPT);
#if DEBUG
        fprintf(msufrendLog, " cmdPort:%04x", cmdPort);
#endif

    if (cmdPort <= PTN_MAX)
        {
        /* this is a write to a control port */
        taskCTLPT(portNum);
        }
    else
        {
        u8 connType = getByte(fwaMyPort + C_PTCT1);
#if DEBUG
        fprintf(msufrendLog, " connType:%02x", connType);
#endif
        if (CT_SOCK == connType)
            {
            HalfWord  socketNum   = getHalfWord(fwaMyPort + H_PTCN1);
            FrendAddr fwaMySocket = sockNumToFwa(socketNum);
            if (FP_BULK == recType)
                {
                /*//! This should be moved to SKOTCL, I think. */
                SETHFLAG(fwaMySocket, SKSUPE);
                }
            taskSKOTCL(activeFrend->ports + (socketNum - 1), fwaMySocket);
            }
        else if (CT_PORT == connType)
            {
            /* We don't handle port-to-port connections. */
            }
        else
            {
            /* unrecognized connection type */
            }
        }
    }

static void clearCmdInterlock()
    {
    /* Clear FPCOM interlock by setting to 1, which means OK. */
    dropInterlock(activeFrend->fwaFPCOM + H_FCMDIK);
    }

/*--- function cmdITook ----------------------
 *  Process the ITOOK command, which 1FP sends to FREND to tell
 *  it that 1FP has processed the most recent buffer for this port.
 */
static void cmdITook()
    {
    FrendAddr fwaList, bufnext, bufnext1FP;

    /* Free the buffer that was just processed by 1FP */
    HalfWord  portNum = getHalfWord(activeFrend->fwaFPCOM + H_FCMDPT);
    FrendAddr fwaPort = portNumToFwa(portNum);
    FrendAddr bufaddr = getFullWord(fwaPort + W_PTIN);

#if DEBUG
    fprintf(msufrendLog, "\n  ITook port:%04x", portNum);
#endif

    bufaddr = addr1FPToFrend(bufaddr);
    PUTBUF(bufaddr);

    /* MOVE NEXT LINE FROM PORT LIST TO W.PTIN FOR 1FP */

    fwaList = getFullWord(fwaPort + W_PTINCL);
    bufnext = removeFromBottomOfList(fwaList);
    /* bufnext is 0 if no more buffers available. */
    bufnext1FP = addrFrendTo1FP(bufnext);
    setFullWord(fwaPort + W_PTIN, bufnext1FP);

    dropInterlock(fwaPort + H_PTINIK); /* CLEAR PORT INTERLOCK */

    /* If the port is not a control port,
     * we should send a FP.INBS message over the control port,
     * giving input buffer status.  See
     * SEND FP.INBS OVER CONTROL PORT in ISR65
     */
    if (portNum > PTN_MAX)
        {
        taskSENDCP(portNum, FP_INBS);
        }

    setHalfWord(activeFrend->fwaFPCOM + H_FCMDTY, 0); /* Clear type.  Tricky use of H_FCMDTY. */
    setHalfWord(activeFrend->fwaFPCOM + H_FCMDPT, 0); /* Clear port number */
    clearCmdInterlock();
    }

/*--- function handleInterruptFromHost ----------------------
 *  Handle an interrupt function code sent by 1FP.
 */
static void handleInterruptFromHost()
    {
    u8       cmd     = getByte(activeFrend->fwaFPCOM + C_FCMDTY);
    HalfWord portNum = getHalfWord(activeFrend->fwaFPCOM + H_FCMDPT);

    switch (cmd)
        {
    case FC_ITOOK:
        cmdITook();
        break;

    case FC_HI80:
        cmdHereIs(portNum, W_NBF80);
        break;

    case FC_HI240:
        cmdHereIs(portNum, W_NBF240);
        break;

    case FC_CPOP:
        cmdControlPortOpen();
        break;

    case FC_CPGON:
    default:
#if DEBUG
        fprintf(msufrendLog, "\n  Command:%02x", cmd);
#endif
        break;
        }

    clearCmdInterlock();
    returnBuffersInReleaseList();
    }

/*--- function processIncomingConnection ------------------
 *  select() noticed an incoming connection on the listening port.
 *  Accept it and create a new terminal session.
 */
static u8 telnetIntro[] = {
    TELCODE_IAC, TELCODE_DONT, TELCODE_OPT_ECHO,
    TELCODE_IAC, TELCODE_WILL, TELCODE_OPT_ECHO,
    TELCODE_IAC, TELCODE_WILL, TELCODE_OPT_SUPPRESS_GO_AHEAD,
    TELCODE_IAC, TELCODE_DO, TELCODE_OPT_SUPPRESS_GO_AHEAD
};

static void processIncomingConnection()
    {
#if defined(_WIN32)
    u_long blockEnable = 1;
#endif
    unsigned char  buf[256];
    SOCKET fd;
    struct sockaddr_in from;
#if defined(_WIN32)
    int fromLen;
#else
    socklen_t fromLen;
#endif
    int            i;
    int            maxFd;
    int            n;
    int            optEnable = 1;
    PortContext    *pp;

    fromLen = sizeof(from);
    fd = accept(activeFrend->listenFd, (struct sockaddr *)&from, &fromLen);
    if (fd < 0) return;

    /*
    **  Set Keepalive option so that we can eventually discover if
    **  a client has been rebooted.
    */
    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (void *)&optEnable, sizeof(optEnable));

    /*
    **  Make socket non-blocking.
    */
#if defined(_WIN32)
    ioctlsocket(fd, FIONBIO, &blockEnable);
#else
    fcntl(fd, F_SETFL, O_NONBLOCK);
#endif

#if DEBUG
    fprintf(msufrendLog, "\nAccepted connection from %s", inet_ntoa(from.sin_addr));
#endif

    /* Assign the user a slot in the port list, if available. */
    for (i = FIRSTUSERPORT, pp = activeFrend->ports + (FIRSTUSERPORT - 1); i <= activeFrend->portCount; i++, pp++)
        {
        if (pp->active == FALSE)
            {
            pp->active = TRUE;
            pp->telnetState = TELST_NORMAL;
            pp->fd = fd;
            break;
            }
        }
    if (i <= activeFrend->portCount)
        {
        if (activeFrend->doesTelnet)
            {
            /* Issue initial Telnet negotiations */
            int n = send(fd, telnetIntro, sizeof(telnetIntro), 0);
            }
        taskSKCARR(pp);
        }
    else
        {
        char *sorry = "\r\nSorry, all sockets are in use. Please try again later.";
        int n = send(fd, (unsigned char *)sorry, strlen(sorry), 0);
#if defined(_WIN32)
        closesocket(fd);
#else
        close(fd);
#endif
        }
    }

/*--- function processInboundTelnet ----------------------
 *  Implement a very simple telnet server.  We basically parse
 *  but ignore all incoming sequences.  You'd be surprised at the
 *  variety of different behaviors between different telnet clients.
 *  When we recognize actual data from the user, call PALISR with it.
 *  The telnet FSA state for this connection is kept in sockTcpArray.
 */
static void processInboundTelnet(PortContext *pp, unsigned char *buf, int len)
    {
    int     i;
    u8      ch;

    for (i = 0; i < len; i++)
        {
        ch = buf[i];
        switch (pp->telnetState)
            {
        case TELST_NORMAL:
            if (activeFrend->doesTelnet && TELCODE_IAC == ch)
                {
                pp->telnetState = TELST_GOT_IAC;
                }
            else
                {
                PALISR(pp, ch); /* This is the normal case. */
                }
            break;

        case TELST_GOT_IAC:
            if (TELCODE_IAC == ch)
                {
                /* IAC IAC is like a single IAC */
                PALISR(pp, ch);
                pp->telnetState = TELST_NORMAL;
                }
            else if ((ch >= TELCODE_WILL) && (ch <= TELCODE_DONT))
                {
                pp->telnetState = TELST_GOT_WILL_OR_SIMILAR;
                }
            else
                {
                pp->telnetState = TELST_NORMAL;
                }
            break;

        case TELST_GOT_WILL_OR_SIMILAR:
            pp->telnetState = TELST_NORMAL;
            break;
            }
        }
    }

/*--- function writeNowAvailable ----------------------
 *  We have been alerted that we can now write on this socket.
 */
static void writeNowAvailable(PortContext *pp)
    {
    int           bytesSent;
    int           nOutBytes = pp->pbuf.charsLeft;

    bytesSent = sendToFSock(pp, pp->pbuf.buf + pp->pbuf.first, nOutBytes);
    if (bytesSent <= 0)
        {
        if (EWOULDBLOCK == getLastOSError())
            {
            bytesSent = 0;
            }
        }
    pp->pbuf.first    += bytesSent;
    pp->pbuf.charsLeft = nOutBytes - bytesSent;

    /*
     * If we sent all pending characters, then start sending
     * the rest of the buffers (if any).
     */
    if (pp->pbuf.charsLeft <= 0)
        {
        FrendAddr fwaMySocket = sockNumToFwa(pp->id);
        taskSKOTCL(pp, fwaMySocket);
        }
    }

/*--- Check for I/O activity and drive frontend emulation ----------------------
 */
static void msufrendCheckIo()
    {
    unsigned char  buf[256];
    int            i;
    int            maxFd;
    int            n;
    PortContext    *pp;
    fd_set         readFds;
    struct timeval timeout;
    fd_set         writeFds;

    activeFrend->ioTurns = (activeFrend->ioTurns + 1) % IoTurnsPerPoll;
    if (activeFrend->ioTurns != 0)
        {
        return;
        }

    FD_ZERO(&readFds);
    FD_ZERO(&writeFds);

    FD_SET(activeFrend->listenFd, &readFds);
    maxFd = activeFrend->listenFd;

    for (i = FIRSTUSERPORT, pp = activeFrend->ports + (FIRSTUSERPORT - 1); i <= activeFrend->portCount; i++, pp++)
        {
        if (pp->active)
            {
            /*
             * Don't read from this socket unless the associated port
             * has a few free buffers.  Because each byte read could be
             * an end-of-line, we don't read any more bytes than there
             * are buffers available.  This way, we won't end up with
             * data that we have read and have no place to put.
             */
            FrendAddr fwaMySocket = sockNumToFwa(pp->id);
            HalfWord  portNum     = getHalfWord(fwaMySocket + H_SKCN1);
            FrendAddr fwaMyPort   = portNumToFwa(portNum);
            FrendAddr fwaList     = getFullWord(fwaMyPort + W_PTINCL);
            HalfWord  nFree       = getListFreeEntries(fwaList);
            if (getListFreeEntries(fwaList) > MIN_FREE_PORT_BUFFERS)
                {
                FD_SET(pp->fd, &readFds);
                if (pp->fd > maxFd) maxFd = pp->fd;
                }
            /*
             * If there are characters pending output on this socket,
             * then we need to hear about write availability.
             */
            if (pp->pbuf.charsLeft > 0)
                {
                FD_SET(pp->fd, &writeFds);
                if (pp->fd > maxFd) maxFd = pp->fd;
                }
            }
        }
    timeout.tv_sec  = 0;
    timeout.tv_usec = 0;
    n = select(maxFd + 1, &readFds, &writeFds, NULL, &timeout);
    if (n > 0)
        {
        if (FD_ISSET(activeFrend->listenFd, &readFds))
            {
            /* A terminal user is trying to connect. */
            processIncomingConnection();
            }
        for (i = 0, pp = activeFrend->ports; i < activeFrend->portCount; i++, pp++)
            {
            if (pp->active)
                {
                if (FD_ISSET(pp->fd, &readFds))
                    {
                    int nbytes = recv(pp->fd, (char *)buf, MIN_FREE_PORT_BUFFERS, 0);
                    if (nbytes > 0)
                        {
                        processInboundTelnet(pp, buf, nbytes);
                        }
                    else
                        {
                        FrendAddr fwaMySocket = sockNumToFwa(pp->id);
                        taskCLOFSK(pp->id, fwaMySocket);
                        }
                    }
                if (FD_ISSET(pp->fd, &writeFds))
                    {
                    writeNowAvailable(pp);
                    }
                }
            }
        }
    else if (n < 0)
        {
        fprintf(stderr, "(msufrend) Error %d from select\n", getLastSocketError());
        sleepMsec(500);
        }
    dropInterlock(activeFrend->fwaFPCOM + H_FEDEAD); /* Clear "front-end dead" flag */
    }

#if DEBUG

/*--------------------------------------------------------------------------
**  Purpose:        Convert function code to string.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        String equivalent of function code.
**
**------------------------------------------------------------------------*/
static char *msufrendFunc2String(PpWord funcCode)
    {
    static char buf[30];

    switch (funcCode)
        {
    case FcFEFSEL: return "SELECT      ";
    case FcFEFDES: return "DESELECT    ";
    case FcFEFST:  return "STATUS      ";
    case FcFEFSAU: return "SET UPPER   ";
    case FcFEFSAM: return "SET MIDDLE  ";
    case FcFEFHL:  return "HALT-LOAD   ";
    case FcFEFINT: return "INTERRUPT   ";
    case FcFEFLP:  return "LOAD IFC MEM";
    case FcFEFRM:  return "READ        ";
    case FcFEFWM0: return "WRITE MODE 0";
    case FcFEFWM:  return "WRITE MODE 1";
    case FcFEFRSM: return "READ AND SET";
    case FcFEFCI:  return "CLEAR INITIALIZED STATUS BIT";
    default:
        sprintf(buf,      "UNKNOWN %04o", funcCode);
        return buf;
        }
    }

#endif

/*---------------------------  End Of File  ------------------------------*/
