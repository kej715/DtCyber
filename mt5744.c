/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2021, Kevin Jordan, Tom Hunter
**
**  Name: mt5744.c
**
**  Description:
**      Perform emulation of CDC 5744 Automated Cartridge Subsystem (ACS).
**      The ACS was a StorageTek 4400 robotic cartridge tape system with
**      two communication paths. One path, the control path, was a UDP/IP
**      path managed by the NOS ATF subsystem. Under the direction of
**      the NOS MAGNET system, ATF sent commands to mount and dismount
**      cartridges across the control path. Application data was streamed
**      across the second path, the data path. The data path used a
**      Cyber channel to connect the mainframe to the StorageTek device
**      via a CCC (Cyber Channel Coupler) connected to a FIPS controller.
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
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#if defined(_WIN32)
#include <winsock.h>
#else
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif
#include <string.h>
#include "const.h"
#include "types.h"
#include "proto.h"

/*
**  -----------------
**  Private Constants
**  -----------------
*/

/*
**  ACS function codes as defined in 1MT (NOS 2.8.7):
**
**     F0001    EQU    0001        RELEASE UNIT
**     F0002    EQU    0002        CONTINUE
**     F0010    EQU    0010        REWIND
**     F0110    EQU    0110        REWIND/UNLOAD
**     F0012    EQU    0012        GENERAL STATUS
**     F0013    EQU    0013        FORESPACE BLOCK
**     F0016    EQU    0016        LOCATE BLOCK
**     F0113    EQU    0113        BACKSPACE BLOCK
**     F0112    EQU    0112        DETAILED STATUS
**     F0212    EQU    0212        READ BLOCK ID
**     F0312    EQU    0312        READ BUFFERED LOG
**     F0020    EQU    0020        CONNECT
**     F0220    EQU    0220        CONNECT AND SELECT DATA COMPRESSION
**     F0040    EQU    0040        READ FORWARD
**     F0140    EQU    0140        READ REVERSE
**     F0050    EQU    0050        WRITE
**     F0250    EQU    0250        SHORT WRITE
**     F0051    EQU    0051        WRITE TAPE MARK
**     F0414    EQU    0414        AUTOLOAD
*/

#define Fc5744Release                        00001
#define Fc5744Continue                       00002
#define Fc5744Rewind                         00010
#define Fc5744RewindUnload                   00110
#define Fc5744GeneralStatus                  00012
#define Fc5744SpaceFwd                       00013
#define Fc5744LocateBlock                    00016
#define Fc5744SpaceBkw                       00113
#define Fc5744DetailedStatus                 00112
#define Fc5744ReadBlockId                    00212
#define Fc5744ReadBufferedLog                00312
#define Fc5744Connect                        00020
#define Fc5744ConnectAndSelectCompression    00220
#define Fc5744ReadFwd                        00040
#define Fc5744ReadBkw                        00140
#define Fc5744Write                          00050
#define Fc5744WriteShort                     00250
#define Fc5744WriteTapeMark                  00051
#define Fc5744Autoload                       00414

/*
**  General status
*/
#define St5744Alert                          04000
#define St5744CommandRetry                   02000
#define St5744NoUnit                         01000
#define St5744BlockNotFound                  00400
#define St5744WriteEnabled                   00200
#define St5744RetryInProgress                00100
#define St5744CharacterFill                  00040
#define St5744TapeMark                       00020
#define St5744EOT                            00010
#define St5744BOT                            00004
#define St5744Busy                           00002
#define St5744Ready                          00001

/*
**  ACS error codes as defined in 1MT (NOS 2.8.7). These
**  values are returned in General Status Word 2. General
**  Status Word 2 is read when General Status Word 1 indicates
**  an alert has occurred (bit 11) and command retry not
**  indicated (bit 10).
**
**     CE001    EQU    1           TRANSPORT NOT ON-LINE
**     CE007    EQU    7           DENSITY MARK/BLOCK ID READ FAILURE
**     CE012    EQU    12          WRITE ERROR AT LOAD POINT
**     CE032    EQU    32          DRIVE BUSY
**     CE033    EQU    33          CONTROL UNIT BUSY
**     CE051    EQU    51          NO TAPE UNIT CONNECTED
*/
#define EcTranportNotOnline                  00001
#define EcBlockIdError                       00007
#define EcWriteErrorAtLoadPoint              00012
#define EcDriveBusy                          00032
#define EcControlUnitBusy                    00033
#define EcNoTapeUnitConnected                00051

#define BlockIdLength                        8
#define BufferedLogLength                    32
#define DetailedStatusLength                 26
#define GeneralStatusLength                  2
#define LocateBlockLength                    3

/*
**  Misc constants.
*/
#define ConnectionRetryInterval              60
#define MaxPpBuf                             40000
#define MaxByteBuf                           60000
#define VolumeNameSize                       6

/*
**  -----------------------
**  Private Macro Functions
**  -----------------------
*/
#if DEBUG
#define HexColumn(x)      (3 * (x) + 4)
#define AsciiColumn(x)    (HexColumn(16) + 2 + (x))
#define LogLineLength    (AsciiColumn(16))
#endif

/*
**  -----------------------------------------
**  Private Typedef and Structure Definitions
**  -----------------------------------------
*/

/*
**  ACS unit state.
*/
typedef enum
    {
    StAcsDisconnected = 0,
    StAcsConnecting,
    StAcsRegistering,
    StAcsReady
    } AcsState;

/*
**  Tape Server I/O buffer
*/
typedef struct tapeBuffer
    {
    u32 in;
    u32 out;
    u8  data[MaxByteBuf + 16];
    } TapeBuffer;

/*
**  ACS controller.
*/
typedef struct ctrlParam
    {
    bool   isWriting;
    bool   isOddFrameCount;
    u8     ioDelay;
    u8     channelNo;
    u8     eqNo;
    PpWord generalStatus[GeneralStatusLength];
    PpWord detailedStatus[DetailedStatusLength];
#if DEBUG
    bool   isJustActivated;
#endif
    } CtrlParam;

/*
**  ACS tape unit.
*/
typedef struct tapeParam
    {
    CtrlParam          *controller;
    struct tapeParam   *nextTape;
    AcsState           state;
    void (*callback)(struct tapeParam *tp);
    time_t             nextConnectionAttempt;
    char               *driveName;
    char               *serverName;
    u8                 channelNo;
    u8                 eqNo;
    u8                 unitNo;
    char               volumeName[VolumeNameSize + 1];
    struct sockaddr_in serverAddr;
#if defined(_WIN32)
    SOCKET             fd;
#else
    int                fd;
#endif
    TapeBuffer         inputBuffer;
    TapeBuffer         outputBuffer;
    bool               isAlert;
    bool               isBlockNotFound;
    bool               isBOT;
    bool               isBusy;
    bool               isCharacterFill;
    bool               isEOT;
    bool               isReady;
    bool               isTapeMark;
    bool               isWriteEnabled;
    PpWord             errorCode;
    u32                recordLength;
    PpWord             ioBuffer[MaxPpBuf];
    PpWord             *bp;
    } TapeParam;

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static void mt5744Activate(void);
static void mt5744CalculateBufferedLog(TapeParam *tp);
static void mt5744CalculateDetailedStatus(TapeParam *tp);
static void mt5744CalculateGeneralStatus(TapeParam *tp);
static void mt5744CheckTapeServer(void);
static void mt5744CloseTapeServerConnection(TapeParam *tp);
static void mt5744ConnectCallback(TapeParam *tp);
static void mt5744DismountRequestCallback(TapeParam *tp);
static FcStatus mt5744Func(PpWord funcCode);
static void mt5744FuncBackspace(void);
static void mt5744FuncForespace(void);
static void mt5744FuncReadBkw(void);
static void mt5744FuncReadFwd(void);
static void mt5744Disconnect(void);
static void mt5744FlushWrite(void);
static void mt5744Io(void);
static void mt5744InitiateConnection(TapeParam *tp);
static void mt5744IssueTapeServerRequest(TapeParam *tp, char *request, void (*callback)(struct tapeParam *tp));
void mt5744LoadTape(TapeParam *tp, bool writeEnable);
static void mt5744LocateBlockRequestCallback(TapeParam *tp);
static int mt5744PackBytes(TapeParam *tp, u8 *rp, int recLen);
static char *mt5744ParseTapeServerResponse(TapeParam *tp, int *status);
static void mt5744ReadBlockIdRequestCallback(TapeParam *tp);
static void mt5744ReadRequestCallback(TapeParam *tp);
static void mt5744ReceiveTapeServerResponse(TapeParam *tp);
static void mt5744RegisterUnit(TapeParam *tp);
static void mt5744RegisterUnitRequestCallback(TapeParam *tp);
static void mt5744ResetInputBuffer(TapeParam *tp, u8 *eor);
static void mt5744ResetStatus(TapeParam *tp);
static void mt5744ResetUnit(TapeParam *tp);
static void mt5744RewindRequestCallback(TapeParam *tp);
static void mt5744RewindUnloadRequestCallback(TapeParam *tp);
static void mt5744SendTapeServerRequest(TapeParam *tp);
static void mt5744SpaceRequestCallback(TapeParam *tp);
void mt5744UnloadTape(TapeParam *tp);
static void mt5744WriteRequestCallback(TapeParam *tp);
static void mt5744WriteMarkRequestCallback(TapeParam *tp);

#if DEBUG
static char *mt5744Func2String(PpWord funcCode);
static void mt5744LogBytes(u8 *bytes, int len);
static void mt5744LogFlush(void);

#endif


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
static TapeParam *firstTape = NULL;
static TapeParam *lastTape  = NULL;

#if DEBUG
static FILE *mt5744Log = NULL;
static char mt5744LogBuf[LogLineLength + 1];
static int  mt5744LogBytesCol = 0;
#endif

/*
 **--------------------------------------------------------------------------
 **
 **  Public Functions
 **
 **--------------------------------------------------------------------------
 */
/*--------------------------------------------------------------------------
**  Purpose:        Initialise 5744 tape drives.
**
**  Parameters:     Name        Description.
**                  eqNo        equipment number
**                  unitNo      number of the unit to initialise
**                  channelNo   channel number the device is attached to
**                  deviceName  optional TCP address of StorageTek 4400 emulator
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void mt5744Init(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName)
    {
    CtrlParam      *cp;
    DevSlot        *dp;
    struct hostent *hp;
    u16            serverPort;
    char           *sp;
    TapeParam      *tp;
    long           value;

    if (deviceName == NULL)
        {
        logDtError(LogErrorLocation, "StorageTek 4400 simulator connection information required for MT5744 on channel %o equipment %o unit %o\n",
                   channelNo, eqNo, unitNo);
        exit(1);
        }

#if DEBUG
    if (mt5744Log == NULL)
        {
        mt5744Log = fopen("mt5744log.txt", "wt");
        mt5744LogFlush();
        }
#endif

    /*
    **  Attach device to channel.
    */
    dp = channelAttach(channelNo, eqNo, DtMt5744);

    /*
    **  Setup channel functions.
    */
    dp->activate     = mt5744Activate;
    dp->disconnect   = mt5744Disconnect;
    dp->func         = mt5744Func;
    dp->io           = mt5744Io;
    dp->selectedUnit = -1;

    /*
    **  Setup controller context.
    */
    if (dp->controllerContext == NULL)
        {
        dp->controllerContext = calloc(1, sizeof(CtrlParam));
        cp            = dp->controllerContext;
        cp->channelNo = channelNo;
        cp->eqNo      = eqNo;
        }

    /*
    **  No file associations on this type of unit
    */
    dp->fcb[unitNo] = NULL;

    /*
    **  Setup tape unit parameter block.
    */
    tp = calloc(1, sizeof(TapeParam));
    if (tp == NULL)
        {
        logDtError(LogErrorLocation, "Failed to allocate MT5744 context block\n");
        exit(1);
        }

    mt5744ResetUnit(tp);
    dp->context[unitNo]       = tp;
    tp->controller            = dp->controllerContext;
    tp->state                 = StAcsDisconnected;
    tp->nextConnectionAttempt = 0;
    tp->fd = 0;

    /*
    **  Set up server connection
    */
    tp->driveName  = NULL;
    tp->serverName = NULL;
    serverPort     = 0;
    sp             = strchr(deviceName, '/');
    if (sp != NULL)
        {
        *sp++         = '\0';
        tp->driveName = (char *)malloc(strlen(sp) + 1);
        strcpy(tp->driveName, sp);
        sp = strchr(deviceName, ':');
        if (sp != NULL)
            {
            *sp++          = '\0';
            tp->serverName = (char *)malloc(strlen(deviceName) + 1);
            strcpy(tp->serverName, deviceName);
            value = strtol(sp, NULL, 10);
            if ((value > 0) && (value < 0x10000))
                {
                serverPort = (u16)value;
                }
            }
        }
    if ((tp->driveName == NULL) || (tp->serverName == NULL) || (serverPort == 0))
        {
        fprintf(stderr,
                "(mt5744 ) Invalid StorageTek 4400 simulator connection specification for MT5744 on channel %o equipment %o unit %o\n",
                channelNo, eqNo, unitNo);
        exit(1);
        }
    hp = gethostbyname(tp->serverName);
    if (hp == NULL)
        {
        logDtError(LogErrorLocation, "Failed to lookup address of StorageTek 4400 simulator host %s\n", tp->serverName);
        exit(1);
        }
    tp->serverAddr.sin_family = AF_INET;
    memcpy(&tp->serverAddr.sin_addr.s_addr, (struct in_addr *)hp->h_addr_list[0], sizeof(struct in_addr));
    tp->serverAddr.sin_port = htons(serverPort);

    /*
    **  Link into list of tape units.
    */
    if (lastTape == NULL)
        {
        firstTape = tp;
        }
    else
        {
        lastTape->nextTape = tp;
        }
    lastTape = tp;

    /*
    **  Setup show_tape values.
    */
    tp->channelNo = channelNo;
    tp->eqNo      = eqNo;
    tp->unitNo    = unitNo;

    /*
    **  Print a friendly message.
    */
    printf("(mt5744 ) initialised on channel %o equipment %o unit %o, drive %s on tape server %s:%d\n",
           channelNo, eqNo, unitNo, tp->driveName, tp->serverName, serverPort);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Show tape status (operator interface).
**
**  Parameters:     Name        Description.
**
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void mt5744ShowTapeStatus()
    {
    TapeParam *tp = firstTape;
    char      outBuf[400];

    while (tp)
        {
        sprintf(outBuf, "    >   %-8s C%02o E%02o U%02o", "5744", tp->channelNo, tp->eqNo, tp->unitNo);
        opDisplay(outBuf);
        switch (tp->state)
            {
        case StAcsDisconnected:
            opDisplay("   (disconnected)\n");
            break;

        case StAcsConnecting:
            opDisplay("   (connecting)\n");
            break;

        case StAcsRegistering:
            opDisplay("   (registering)\n");
            break;

        case StAcsReady:
            if (tp->volumeName[0])
                {
                sprintf(outBuf, " %s %s\n", tp->isWriteEnabled ? "w" : "r", tp->volumeName);
                opDisplay(outBuf);
                }
            else
                {
                opDisplay("   (idle)\n");
                }
            break;

        default:
            opDisplay("   (unknown state)\n");
            break;
            }

        tp = tp->nextTape;
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
**  Purpose:        Handle channel activation.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mt5744Activate(void)
    {
#if DEBUG
    CtrlParam *cp = activeDevice->controllerContext;
    fprintf(mt5744Log, "\n%010u PP:%02o CH:%02o P:%04o Activate",
            traceSequenceNo,
            activePpu->id,
            activeDevice->channel->id,
            activePpu->regP);
    cp->isJustActivated = TRUE;
#endif
    activeChannel->delayStatus = 5;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Set buffered log information.
**
**  Parameters:     Name        Description.
**                  tp          pointer to tape parameters
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
static void mt5744CalculateBufferedLog(TapeParam *tp)
    {
    int i;

    for (i = 0; i < BufferedLogLength; i++)
        {
        tp->ioBuffer[i] = 0;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Set detailed device status based on current tape parameters.
**
**  Parameters:     Name        Description.
**                  tp          pointer to tape parameters
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
static void mt5744CalculateDetailedStatus(TapeParam *tp)
    {
    CtrlParam *cp;
    int       i;

    cp = activeDevice->controllerContext;
    for (i = 0; i < DetailedStatusLength; i++)
        {
        cp->detailedStatus[i] = 0;
        }
    if (tp != NULL)
        {
        cp->detailedStatus[0] = tp->errorCode;
        if (tp->isReady == FALSE)
            {
            cp->detailedStatus[0] |= 00020;
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Set general device status based on current tape parameters.
**
**  Parameters:     Name        Description.
**                  tp          pointer to tape parameters
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
static void mt5744CalculateGeneralStatus(TapeParam *tp)
    {
    CtrlParam *cp;

    cp = activeDevice->controllerContext;
    cp->generalStatus[0] = 0;
    cp->generalStatus[1] = 0;
    if (tp != NULL)
        {
        if ((tp->state == StAcsReady) && (tp->fd > 0) && tp->isReady)
            {
            cp->generalStatus[0] = St5744Ready;
            }
        if (tp->isBOT)
            {
            cp->generalStatus[0] |= St5744BOT;
            }
        if (tp->isBusy)
            {
            cp->generalStatus[0] |= St5744Busy;
            }
        if (tp->isCharacterFill)
            {
            cp->generalStatus[0] |= St5744CharacterFill;
            }
        if (tp->isTapeMark)
            {
            cp->generalStatus[0] |= St5744TapeMark;
            }
        if (tp->isWriteEnabled)
            {
            cp->generalStatus[0] |= St5744WriteEnabled;
            }
        if (tp->isEOT)
            {
            cp->generalStatus[0] |= St5744EOT;
            }
        if (tp->isBlockNotFound)
            {
            cp->generalStatus[0] |= St5744BlockNotFound;
            }
        if (tp->isAlert)
            {
            cp->generalStatus[0] |= St5744Alert;
            }
        cp->generalStatus[1] = tp->errorCode;
        }
    else
        {
        cp->generalStatus[0] = St5744Ready;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Process tape server I/O and state transitions.
**
**  Parameters:     Name        Description.
**                  None.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mt5744CheckTapeServer(void)
    {
#if defined(_WIN32)
    SOCKET maxFd;
#else
    int maxFd;
#endif
    fd_set         readFds;
    int            readySockets;
    struct timeval timeout;
    TapeParam      *tp;
    fd_set         writeFds;

    /*
    **  First, process any tape server connections in progress
    */
    FD_ZERO(&writeFds);
    tp    = firstTape;
    maxFd = 0;

    while (tp)
        {
        if (tp->state == StAcsDisconnected)
            {
            mt5744InitiateConnection(tp);
            }
        else if ((tp->fd > 0) && (tp->state == StAcsConnecting))
            {
            FD_SET(tp->fd, &writeFds);
            if (tp->fd > maxFd)
                {
                maxFd = tp->fd;
                }
            }
        tp = tp->nextTape;
        }
    if (maxFd > 0)
        {
        timeout.tv_sec  = 0;
        timeout.tv_usec = 0;
        readySockets    = select((int)(maxFd + 1), NULL, &writeFds, NULL, &timeout);
        if (readySockets > 0)
            {
            tp = firstTape;
            while (tp)
                {
                if ((tp->fd > 0) && (tp->state == StAcsConnecting) && FD_ISSET(tp->fd, &writeFds))
                    {
                    mt5744ConnectCallback(tp);
                    }
                tp = tp->nextTape;
                }
            }
        }

    /*
    **  Second, process normal I/O for connected tape units
    */
    FD_ZERO(&readFds);
    FD_ZERO(&writeFds);
    tp    = firstTape;
    maxFd = 0;

    while (tp)
        {
        if ((tp->fd > 0) && (tp->state > StAcsConnecting))
            {
            FD_SET(tp->fd, &readFds);
            if (tp->outputBuffer.out < tp->outputBuffer.in)
                {
                FD_SET(tp->fd, &writeFds);
                }
            if (tp->fd > maxFd)
                {
                maxFd = tp->fd;
                }
            }
        tp = tp->nextTape;
        }
    if (maxFd < 1)
        {
        return;
        }

    timeout.tv_sec  = 0;
    timeout.tv_usec = 0;
    readySockets    = select((int)(maxFd + 1), &readFds, &writeFds, NULL, &timeout);
    if (readySockets < 1)
        {
        return;
        }

    tp = firstTape;
    while (tp)
        {
        if ((tp->fd > 0) && (tp->state > StAcsConnecting))
            {
            if (FD_ISSET(tp->fd, &readFds))
                {
                mt5744ReceiveTapeServerResponse(tp);
                }
            if ((tp->fd > 0) && FD_ISSET(tp->fd, &writeFds))
                {
                mt5744SendTapeServerRequest(tp);
                }
            }
        tp = tp->nextTape;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Close connection to StorageTek simulator
**
**  Parameters:     Name        Description.
**                  tp          pointer to tape unit parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mt5744CloseTapeServerConnection(TapeParam *tp)
    {
#if DEBUG
    fprintf(mt5744Log, "\n%010u Close connection on socket %d to %s:%u for CH:%02o u:%d", traceSequenceNo,
            tp->fd, tp->serverName, ntohs(tp->serverAddr.sin_port), tp->channelNo, tp->unitNo);
#endif
    netCloseConnection(tp->fd);
    tp->fd                    = 0;
    tp->isReady               = FALSE;
    tp->isBusy                = FALSE;
    tp->errorCode             = EcTranportNotOnline;
    tp->state                 = StAcsDisconnected;
    tp->nextConnectionAttempt = getSeconds() + (time_t)ConnectionRetryInterval;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Handle a TCP connection event
**
**  Parameters:     Name        Description.
**                  tp          pointer to tape unit parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mt5744ConnectCallback(TapeParam *tp)
    {
    int rc;

    rc = netGetErrorStatus(tp->fd);
    if (rc != 0) // connection failed
        {
#if DEBUG
        fprintf(mt5744Log, "\n%010u Failed to connect on socket %d to %s:%u for CH:%02o u:%d", traceSequenceNo,
                tp->fd, tp->serverName, ntohs(tp->serverAddr.sin_port), tp->channelNo, tp->unitNo);
#endif
        mt5744CloseTapeServerConnection(tp);
        }
    else
        {
#if DEBUG
        fprintf(mt5744Log, "\n%010u Connected on socket %d to %s:%u for CH:%02o u:%d", traceSequenceNo,
                tp->fd, tp->serverName, ntohs(tp->serverAddr.sin_port), tp->channelNo, tp->unitNo);
#endif
        mt5744RegisterUnit(tp);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Handle disconnecting of channel.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mt5744Disconnect(void)
    {
#if DEBUG
    fprintf(mt5744Log, "\n%010u PP:%02o CH:%02o P:%04o Disconnect",
            traceSequenceNo,
            activePpu->id,
            activeDevice->channel->id,
            activePpu->regP);
#endif

    /*
    **  Abort pending device disconnects - the PP is doing the disconnect.
    */
    activeChannel->delayDisconnect = 0;
    activeChannel->discAfterInput  = FALSE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Process a response from the StorageTek simulator to a
**                  DISMOUNT request.
**
**  Parameters:     Name        Description.
**                  tp          pointer to tape unit parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mt5744DismountRequestCallback(TapeParam *tp)
    {
    char *eor;
    int  status;

    eor = mt5744ParseTapeServerResponse(tp, &status);
    if (eor == NULL)
        {
        return;
        }
    tp->isBusy = FALSE;
    if (status == 200)
        {
        tp->isReady = FALSE;
        }
    else
        {
        logDtError(LogErrorLocation, "Unexpected status %d received from StorageTek simulator for DISMOUNT request\n", status);
        mt5744CloseTapeServerConnection(tp);
        }
    mt5744ResetInputBuffer(tp, (u8 *)eor);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Flush accumulated write data.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mt5744FlushWrite(void)
    {
    char      buffer[10];
    CtrlParam *cp = activeDevice->controllerContext;
    u8        *dataStart;
    u32       i;
    PpWord    *ip;
    int       len;
    u32       recLen0;
    u32       recLen2;
    u8        *rp;
    TapeParam *tp;
    i8        unitNo;

    unitNo = activeDevice->selectedUnit;
    tp     = (TapeParam *)activeDevice->context[unitNo];

    if ((unitNo == -1) || !tp->isReady)
        {
        return;
        }

    tp->bp  = tp->ioBuffer;
    recLen0 = 0;
    recLen2 = tp->recordLength;
    ip      = tp->ioBuffer;
    memcpy(&tp->outputBuffer.data[0], "WRITE          \n", 16);
    rp = dataStart = &tp->outputBuffer.data[16];

    for (i = 0; i < recLen2; i += 2)
        {
        *rp++ = ((ip[0] >> 4) & 0xff);
        *rp++ = ((ip[0] << 4) & 0xf0) | ((ip[1] >> 8) & 0x0f);
        *rp++ = ((ip[1] >> 0) & 0xff);
        ip   += 2;
        }

    recLen0 = (u32)(rp - dataStart);

    if ((recLen2 & 1) != 0)
        {
        recLen0 -= 2;
        }
    else if (cp->isOddFrameCount)
        {
        recLen0 -= 1;
        }

    len = sprintf(buffer, "%d", recLen0);
    memcpy(&tp->outputBuffer.data[6], buffer, len);
    tp->outputBuffer.out = 0;
    tp->outputBuffer.in  = recLen0 + 16;
    tp->callback         = mt5744WriteRequestCallback;
    tp->isBusy           = TRUE;
    cp->isWriting        = FALSE;
    cp->isOddFrameCount  = FALSE;
#if DEBUG
    fprintf(mt5744Log, "\n%010u PP:%02o CH:%02o P:%04o Write %d PP words",
            traceSequenceNo,
            activePpu->id,
            activeDevice->channel->id,
            activePpu->regP,
            tp->recordLength);
#endif
    mt5744SendTapeServerRequest(tp);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Execute function code on 5744 tape drives.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        FcStatus
**
**------------------------------------------------------------------------*/
static FcStatus mt5744Func(PpWord funcCode)
    {
    CtrlParam *cp;
    TapeParam *tp;
    i8        unitNo;

    cp     = activeDevice->controllerContext;
    unitNo = activeDevice->selectedUnit;
    if (unitNo != -1)
        {
        tp = (TapeParam *)activeDevice->context[unitNo];
        }
    else
        {
        tp = NULL;
        }

    /*
    **  Flush write data if necessary.
    */
    if (cp->isWriting)
        {
        mt5744FlushWrite();
        }

#if DEBUG
    fprintf(mt5744Log, "\n%010u PP:%02o CH:%02o P:%04o u:%d f:%04o T:%-25s",
            traceSequenceNo,
            activePpu->id,
            activeDevice->channel->id,
            activePpu->regP,
            unitNo,
            funcCode,
            mt5744Func2String(funcCode));
#endif

    /*
    **  Reset function code.
    */
    activeDevice->fcode = 0;
    activeChannel->full = FALSE;

    /*
    **  Process tape function.
    */
    cp->ioDelay = 0;
    switch (funcCode)
        {
    default:
#if DEBUG
        fprintf(mt5744Log, " FUNC not implemented & declined!");
#endif

        return (FcDeclined);

    case Fc5744Continue:
        mt5744ResetStatus(tp);
        break;

    case Fc5744Release:
        activeDevice->selectedUnit = -1;

        return (FcProcessed);

    case Fc5744Rewind:
    case Fc5744RewindUnload:
        if ((unitNo != -1) && tp->isReady)
            {
            mt5744ResetStatus(tp);
            mt5744IssueTapeServerRequest(tp, "REWIND",
                                         (funcCode == Fc5744Rewind) ? mt5744RewindRequestCallback : mt5744RewindUnloadRequestCallback);
            }

        return (FcProcessed);

    case Fc5744GeneralStatus:
        activeDevice->recordLength = GeneralStatusLength;
        break;

    case Fc5744DetailedStatus:
        activeDevice->recordLength = DetailedStatusLength;
        break;

    case Fc5744SpaceFwd:
        if ((unitNo != -1) && tp->isReady)
            {
            mt5744ResetStatus(tp);
            mt5744IssueTapeServerRequest(tp, "SPACEFWD", mt5744SpaceRequestCallback);
            }

        return (FcProcessed);

    case Fc5744SpaceBkw:
        if ((unitNo != -1) && tp->isReady)
            {
            mt5744ResetStatus(tp);
            mt5744IssueTapeServerRequest(tp, "SPACEBKW", mt5744SpaceRequestCallback);
            }

        return (FcProcessed);

    case Fc5744Connect + 0:
    case Fc5744Connect + 1:
    case Fc5744Connect + 2:
    case Fc5744Connect + 3:
    case Fc5744Connect + 4:
    case Fc5744Connect + 5:
    case Fc5744Connect + 6:
    case Fc5744Connect + 7:
    case Fc5744Connect + 010:
    case Fc5744Connect + 011:
    case Fc5744Connect + 012:
    case Fc5744Connect + 013:
    case Fc5744Connect + 014:
    case Fc5744Connect + 015:
    case Fc5744Connect + 016:
    case Fc5744Connect + 017:
    case Fc5744ConnectAndSelectCompression + 0:
    case Fc5744ConnectAndSelectCompression + 1:
    case Fc5744ConnectAndSelectCompression + 2:
    case Fc5744ConnectAndSelectCompression + 3:
    case Fc5744ConnectAndSelectCompression + 4:
    case Fc5744ConnectAndSelectCompression + 5:
    case Fc5744ConnectAndSelectCompression + 6:
    case Fc5744ConnectAndSelectCompression + 7:
    case Fc5744ConnectAndSelectCompression + 010:
    case Fc5744ConnectAndSelectCompression + 011:
    case Fc5744ConnectAndSelectCompression + 012:
    case Fc5744ConnectAndSelectCompression + 013:
    case Fc5744ConnectAndSelectCompression + 014:
    case Fc5744ConnectAndSelectCompression + 015:
    case Fc5744ConnectAndSelectCompression + 016:
    case Fc5744ConnectAndSelectCompression + 017:
        unitNo = funcCode & Mask4;
        tp     = (TapeParam *)activeDevice->context[unitNo];
        if (tp == NULL)
            {
            activeDevice->selectedUnit = -1;
            logDtError(LogErrorLocation, "channel %02o - invalid select: %04o", activeChannel->id, (u32)funcCode);

            return (FcDeclined);
            }
        activeDevice->selectedUnit = unitNo;

        return (FcProcessed);

    case Fc5744ReadFwd:
        if (unitNo != -1)
            {
            mt5744ResetStatus(tp);
            if (tp->isReady)
                {
                mt5744IssueTapeServerRequest(tp, "READFWD", mt5744ReadRequestCallback);
                }
            break;
            }

        return (FcProcessed);

    case Fc5744ReadBkw:
        if (unitNo != -1)
            {
            mt5744ResetStatus(tp);
            if (tp->isReady)
                {
                mt5744IssueTapeServerRequest(tp, "READBKW", mt5744ReadRequestCallback);
                }
            break;
            }

        return (FcProcessed);

    case Fc5744ReadBlockId:
        if (unitNo != -1)
            {
            mt5744ResetStatus(tp);
            if (tp->isReady)
                {
                mt5744IssueTapeServerRequest(tp, "READBLOCKID", mt5744ReadBlockIdRequestCallback);
                }
            break;
            }

        return (FcProcessed);

    case Fc5744ReadBufferedLog:
        if (unitNo != -1)
            {
            mt5744ResetStatus(tp);
            tp->recordLength = BufferedLogLength;
            if (tp->isReady)
                {
                mt5744CalculateBufferedLog(tp);
                }
            break;
            }

        return (FcProcessed);

    case Fc5744LocateBlock:
        if (unitNo != -1)
            {
            mt5744ResetStatus(tp);
            tp->bp           = tp->ioBuffer;
            tp->recordLength = 0;
            break;
            }

        return (FcProcessed);

    case Fc5744Write:
    case Fc5744WriteShort:
        if (unitNo != -1)
            {
            mt5744ResetStatus(tp);
            tp->bp              = tp->ioBuffer;
            tp->recordLength    = 0;
            cp->isWriting       = TRUE;
            cp->isOddFrameCount = funcCode == Fc5744WriteShort;
            break;
            }

        return (FcProcessed);

    case Fc5744WriteTapeMark:
        if (unitNo != -1)
            {
            mt5744ResetStatus(tp);
            if (tp->isReady && tp->isWriteEnabled)
                {
                mt5744IssueTapeServerRequest(tp, "WRITEMARK", mt5744WriteMarkRequestCallback);
                }
            }

        return (FcProcessed);

    case Fc5744Autoload:
        mt5744ResetStatus(tp);
        activeDevice->selectedUnit = -1;
        break;
        }

    activeDevice->fcode = funcCode;

    return (FcAccepted);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Initiate a TCP connection to a StorageTek simulator
**
**  Parameters:     Name        Description.
**                  tp          pointer to tape unit parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mt5744InitiateConnection(TapeParam *tp)
    {
    time_t currentTime;

#if defined(_WIN32)
    SOCKET fd;
#else
    int fd;
#endif

    currentTime = getSeconds();
    if (tp->nextConnectionAttempt > currentTime)
        {
        return;
        }
    tp->nextConnectionAttempt = currentTime + ConnectionRetryInterval;

    fd = netInitiateConnection((struct sockaddr *)&tp->serverAddr);
#if defined(_WIN32)
    if (fd == INVALID_SOCKET)
#else
    if (fd == -1)
#endif
        {
#if DEBUG
        fprintf(mt5744Log, "\n%010u Failed to initiate connection to %s:%u for CH:%02o u:%d, %s", traceSequenceNo,
                tp->serverName, ntohs(tp->serverAddr.sin_port), tp->channelNo, tp->unitNo, strerror(errno));
#endif
        netCloseConnection(fd);
        }
    else // connection in progress
        {
        tp->fd    = fd;
        tp->state = StAcsConnecting;
#if DEBUG
        fprintf(mt5744Log, "\n%010u Initiated connection on socket %d to %s:%u for CH:%02o u%d", traceSequenceNo,
                tp->fd, tp->serverName, ntohs(tp->serverAddr.sin_port), tp->channelNo, tp->unitNo);
#endif
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Perform I/O on MT5744.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mt5744Io(void)
    {
    CtrlParam *cp = activeDevice->controllerContext;
    i8        unitNo;
    TapeParam *tp;
    int       wordNumber;

    /*
    **  The following avoids too rapid changes of the full/empty status
    **  when probed via FJM and EJM PP opcodes. This allows a second PP
    **  to monitor the progress of a transfer (used by 1MT and 1LT to
    **  coordinate the transfer of a large tape record).
    */
    if (activeChannel->delayStatus != 0)
        {
        return;
        }
    activeChannel->delayStatus = 3;

    /*
    **  Handle tape server events and I/O
    */
    mt5744CheckTapeServer();

    /*
    **  Setup selected unit context.
    */
    unitNo = activeDevice->selectedUnit;
    if (unitNo != -1)
        {
        tp = (TapeParam *)activeDevice->context[unitNo];
        if (!tp->isReady)
            {
            switch (activeDevice->fcode)
                {
            case Fc5744ReadBufferedLog:
            case Fc5744ReadFwd:
            case Fc5744ReadBlockId:
            case Fc5744ReadBkw:
                activeDevice->fcode           = 0;
                activeChannel->active         = FALSE;
                activeChannel->discAfterInput = TRUE;
                tp->recordLength = 0;

                return;

            case Fc5744Write:
            case Fc5744WriteShort:
            case Fc5744LocateBlock:
            case Fc5744Autoload:
            case Fc5744Continue:
                activeDevice->fcode   = 0;
                activeChannel->active = FALSE;
                activeChannel->full   = FALSE;

                return;

            default:
                // Do nothing
                break;
                }
            }
#if DEBUG
        else if (cp->isJustActivated)
            {
            cp->isJustActivated = FALSE;
            switch (activeDevice->fcode)
                {
            case Fc5744ReadFwd:
            case Fc5744ReadBkw:
            case Fc5744Write:
            case Fc5744WriteShort:
                fprintf(mt5744Log, "\n%010u PP:%02o CH:%02o P:%04o",
                        traceSequenceNo,
                        activePpu->id,
                        activeDevice->channel->id,
                        activePpu->regP);
                fprintf(mt5744Log, "\n                       0:%04o",
                        activePpu->mem[0] & 07777);
                break;

            default:
                // Do nothing
                break;
                }
            }
#endif
        }
    else
        {
        tp = NULL;
        }

    /*
    **  Perform actual tape I/O according to function issued.
    */
    switch (activeDevice->fcode)
        {
    default:
        logDtError(LogErrorLocation, "channel %02o - unsupported function code: %04o",
                   activeChannel->id, activeDevice->fcode);
        break;

    case Fc5744GeneralStatus:
        if (activeChannel->full == FALSE)
            {
            if ((tp != NULL) && tp->isBusy)
                {
                return;
                }
            if (cp->ioDelay > 0)
                {
                cp->ioDelay -= 1;

                return;
                }
            if (activeDevice->recordLength > 0)
                {
                wordNumber = GeneralStatusLength - activeDevice->recordLength;
                if (wordNumber == 0)
                    {
                    mt5744CalculateGeneralStatus(tp);
                    }
                activeChannel->data         = cp->generalStatus[wordNumber];
                activeDevice->recordLength -= 1;
                if (wordNumber == (GeneralStatusLength - 1))
                    {
                    /*
                    **  Last status word deactivates function.
                    */
                    activeDevice->fcode           = 0;
                    activeChannel->discAfterInput = TRUE;
                    }
                if (tp != NULL)
                    {
                    tp->isAlert = FALSE;
                    }

                activeChannel->full = TRUE;
                cp->ioDelay         = 1;
#if DEBUG
                if (activeDevice->recordLength == GeneralStatusLength - 1)
                    {
                    fputs("\n                             ", mt5744Log);
                    }
                fprintf(mt5744Log, " %04o", activeChannel->data);
#endif
                }
            }
        break;

    case Fc5744DetailedStatus:
        if (activeChannel->full == FALSE)
            {
            if ((tp != NULL) && tp->isBusy)
                {
                return;
                }
            if (cp->ioDelay > 0)
                {
                cp->ioDelay -= 1;

                return;
                }
            if (activeDevice->recordLength > 0)
                {
                wordNumber = DetailedStatusLength - activeDevice->recordLength;
                if (wordNumber == 0)
                    {
                    mt5744CalculateDetailedStatus(tp);
                    }
                activeChannel->data         = cp->detailedStatus[wordNumber];
                activeDevice->recordLength -= 1;
                if (wordNumber == (DetailedStatusLength - 1))
                    {
                    /*
                    **  Last status word deactivates function.
                    */
                    activeDevice->fcode           = 0;
                    activeChannel->discAfterInput = TRUE;
                    }

                activeChannel->full = TRUE;
                cp->ioDelay         = 1;
#if DEBUG
                if ((activeDevice->recordLength == DetailedStatusLength - 1)
                    || ((activeDevice->recordLength > 0) && ((activeDevice->recordLength % 8) == 0)))
                    {
                    fputs("\n                             ", mt5744Log);
                    }
                fprintf(mt5744Log, " %04o", activeChannel->data);
#endif
                }
            }
        break;

    case Fc5744ReadBufferedLog:
        if ((tp != NULL) && (tp->isBusy == FALSE) && (activeChannel->full == FALSE))
            {
            if (cp->ioDelay > 0)
                {
                cp->ioDelay -= 1;

                return;
                }
            if (tp->recordLength > 0)
                {
                wordNumber          = BufferedLogLength - tp->recordLength;
                activeChannel->data = tp->ioBuffer[wordNumber];
                tp->recordLength   -= 1;
                if (wordNumber == (BufferedLogLength - 1))
                    {
                    /*
                    **  Last status word deactivates function.
                    */
                    activeDevice->fcode           = 0;
                    activeChannel->discAfterInput = TRUE;
                    }
                activeChannel->full = TRUE;
                cp->ioDelay         = 1;
#if DEBUG
                if ((tp->recordLength == BufferedLogLength - 1)
                    || ((tp->recordLength > 0) && ((tp->recordLength % 8) == 0)))
                    {
                    fputs("\n                             ", mt5744Log);
                    }
                fprintf(mt5744Log, " %04o", activeChannel->data);
#endif
                }
            }
        break;

    case Fc5744ReadFwd:
    case Fc5744ReadBlockId:
        if ((tp != NULL) && (tp->isBusy == FALSE) && (activeChannel->full == FALSE))
            {
            if (cp->ioDelay > 0)
                {
                cp->ioDelay -= 1;
                }
            else if (tp->recordLength > 0)
                {
                activeChannel->data = *tp->bp++;
                activeChannel->full = TRUE;
                tp->recordLength   -= 1;
                if ((tp->recordLength == 0) && (activeDevice->fcode == Fc5744ReadBlockId))
                    {
                    activeChannel->discAfterInput = TRUE;
                    }
#if DEBUG
                if ((tp->recordLength > 0) && ((tp->recordLength % 8) == 0))
                    {
                    fputs("\n                             ", mt5744Log);
                    }
                fprintf(mt5744Log, " %04o", activeChannel->data);
#endif
                }
            else
                {
                activeChannel->active = FALSE;
                }
            }
        break;

    case Fc5744ReadBkw:
        if ((tp != NULL) && (tp->isBusy == FALSE) && (activeChannel->full == FALSE))
            {
            if (cp->ioDelay > 0)
                {
                cp->ioDelay -= 1;
                }
            else if (tp->recordLength > 0)
                {
                tp->recordLength   -= 1;
                activeChannel->data = tp->ioBuffer[tp->recordLength];
                activeChannel->full = TRUE;
                if (tp->recordLength == 0)
                    {
                    activeChannel->discAfterInput = TRUE;
                    }
#if DEBUG
                if ((tp->recordLength > 0) && ((tp->recordLength % 8) == 0))
                    {
                    fputs("\n                             ", mt5744Log);
                    }
                fprintf(mt5744Log, " %04o", activeChannel->data);
#endif
                }
            else
                {
                activeChannel->active = FALSE;
                }
            }
        break;

    case Fc5744Write:
    case Fc5744WriteShort:
        if ((tp != NULL) && (tp->isBusy == FALSE) && activeChannel->full && (tp->recordLength < MaxPpBuf))
            {
#if DEBUG
            if ((tp->recordLength > 0) && ((tp->recordLength % 8) == 0))
                {
                fputs("\n                             ", mt5744Log);
                }
            fprintf(mt5744Log, " %04o", activeChannel->data);
#endif
            activeChannel->full = FALSE;
            *tp->bp++           = activeChannel->data;
            tp->recordLength   += 1;
            }
        break;

    case Fc5744LocateBlock:
        if ((tp != NULL) && (tp->isBusy == FALSE) && activeChannel->full && (tp->recordLength < MaxPpBuf))
            {
#if DEBUG
            if ((tp->recordLength % 8) == 0)
                {
                fputs("\n                             ", mt5744Log);
                }
            fprintf(mt5744Log, " %04o", activeChannel->data);
#endif
            activeChannel->full = FALSE;
            *tp->bp++           = activeChannel->data;
            tp->recordLength   += 1;
            if (tp->recordLength >= LocateBlockLength)
                {
                char buffer[32];
                sprintf(buffer, "LOCATEBLOCK %lu", ((unsigned long)tp->ioBuffer[1] << 12) | (unsigned long)tp->ioBuffer[2]);
                mt5744IssueTapeServerRequest(tp, buffer, mt5744LocateBlockRequestCallback);
                }
            }
        break;

    case Fc5744Autoload:
    case Fc5744Continue:
        activeChannel->full = FALSE;
        break;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Set up for sending a request to the StorageTek simulator.
**
**  Parameters:     Name        Description.
**                  tp          pointer to tape unit parameters
**                  request     the request to send
**                  callback    pointer to response processor
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mt5744IssueTapeServerRequest(TapeParam *tp, char *request, void (*callback)(struct tapeParam *tp))
    {
    u8   *bp;
    char *sp;

    bp = &tp->outputBuffer.data[0];
    sp = request;
    while (*sp && *sp != '\n')
        {
        *bp++ = (u8) * sp++;
        }
    *bp++ = '\n';
    tp->outputBuffer.out = 0;
    tp->outputBuffer.in  = (u32)(bp - &tp->outputBuffer.data[0]);
    tp->callback         = callback;
    tp->isBusy           = TRUE;
    tp->isAlert          = FALSE;
    mt5744SendTapeServerRequest(tp);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Load a new tape indicated by StorageTek simulator.
**
**  Parameters:     Name        Description.
**                  tp          pointer to tape unit parameters
**                  writeEnable TRUE if tape is write-enabled
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void mt5744LoadTape(TapeParam *tp, bool writeEnable)
    {
    int  len;
    char *sp;
    char *volumeName;

    sp = (char *)&tp->inputBuffer.data[4];
    while (*sp == ' ')
        {
        sp += 1;
        }
    volumeName = sp;
    while (*sp != ' ' && *sp != '\n')
        {
        sp += 1;
        }
    *sp = '\0';
    len = (int)(sp - volumeName);
    if (len < 1)
        {
        fputs("(mt5744 ) Volume name missing from command received from StorageTek simulator\n", stderr);
        mt5744CloseTapeServerConnection(tp);

        return;
        }
    else if (len > 6)
        {
        fputs("(mt5744 ) Invalid volume name received from StorageTek simulator\n", stderr);
        mt5744CloseTapeServerConnection(tp);

        return;
        }
    mt5744ResetUnit(tp);
    strcpy(tp->volumeName, volumeName);
    tp->isBOT          = TRUE;
    tp->isBusy         = FALSE;
    tp->isReady        = TRUE;
    tp->isWriteEnabled = writeEnable;
#if DEBUG
    fprintf(mt5744Log, "\n%010u Mount %s on CH:%02o u%d", traceSequenceNo, tp->volumeName, tp->channelNo, tp->unitNo);
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Process a response from the StorageTek simulator to a
**                  LOCATEBLOCK request.
**
**  Parameters:     Name        Description.
**                  tp          pointer to tape unit parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mt5744LocateBlockRequestCallback(TapeParam *tp)
    {
    char *eor;
    int  status;

    eor = mt5744ParseTapeServerResponse(tp, &status);
    if (eor == NULL)
        {
        return;
        }
    tp->isBusy = FALSE;
    if ((status == 504) || (status == 501))
        {
        tp->isBlockNotFound = TRUE;
        tp->isAlert         = TRUE;
        tp->errorCode       = EcBlockIdError;
        }
    else if (status != 200)
        {
        logDtError(LogErrorLocation, "Unexpected status %d received from StorageTek simulator for LOCATEBLOCK request\n", status);
        mt5744CloseTapeServerConnection(tp);
        }
    mt5744ResetInputBuffer(tp, (u8 *)eor);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Pack and convert 8 bit frames read into channel data.
**
**  Parameters:     Name        Description.
**                  tp          pointer to tape params
**                  rp          pointer to raw 8-bit bytes read from tape
**                  recLen      record length
**
**  Returns:        Number of packed PP words produced.
**
**------------------------------------------------------------------------*/
static int mt5744PackBytes(TapeParam *tp, u8 *rp, int recLen)
    {
    u16 c1, c2, c3;
    int i;
    u16 *op;
    int ppWords;

    /*
    **  Convert the raw data into PP words suitable for a channel.
    */
    op = tp->ioBuffer;

    /*
    **  Fill the last few bytes with zeroes.
    */
    rp[recLen + 0] = 0;
    rp[recLen + 1] = 0;

    for (i = 0; i < recLen; i += 3)
        {
        c1 = *rp++;
        c2 = *rp++;
        c3 = *rp++;

        *op++ = ((c1 << 4) | (c2 >> 4)) & Mask12;
        *op++ = ((c2 << 8) | (c3 >> 0)) & Mask12;
        }

    ppWords             = (int)(op - tp->ioBuffer);
    tp->isCharacterFill = FALSE;

    switch (recLen % 3)
        {
    default:
        break;

    case 1:     /* 2 words + 8 bits */
        ppWords -= 1;
        break;

    case 2:
        tp->isCharacterFill = TRUE;
        break;
        }

    return ppWords;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Parse a response from the StorageTek simulator
**
**  Parameters:     Name        Description.
**                  tp          pointer to tape unit parameters
**                  status      pointer to response status return
**                              -1 returned if bad response status received
**
**  Returns:        Pointer to first byte beyond end of response status line,
**                  NULL if response is incomplete.
**
**------------------------------------------------------------------------*/
static char *mt5744ParseTapeServerResponse(TapeParam *tp, int *status)
    {
    char *limit;
    char *sp;
    char *start;

    *status = 0;
    sp      = start = (char *)tp->inputBuffer.data;
    limit   = start + tp->inputBuffer.in;
    while (sp < limit && *sp != '\n')
        {
        ++sp;
        }
    if (sp >= limit)
        {
        return NULL;
        }
    if ((sp - start > 2) && isdigit(*start))
        {
        *status = (int)strtol(start, NULL, 10);
        if ((*status < 100) || (*status > 599))
            {
            *status = -1;
            }
        }
    else
        {
        *status = -1;
        }
    if (*status == -1)
        {
        fputs("(mt5744 ) Bad response received from StorageTek simulator\n", stderr);
        *sp = '\0';
        logDtError(LogErrorLocation, "%s\n", start);
        *sp = '\n';
        }

    return sp + 1;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Process a response from the StorageTek simulator to a
**                  READBLOCKID request.
**
**  Parameters:     Name        Description.
**                  tp          pointer to tape unit parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mt5744ReadBlockIdRequestCallback(TapeParam *tp)
    {
    char *eor;
    char *ep;
    int  i;
    long id1;
    long id2;
    char *sp;
    int  status;

    eor = mt5744ParseTapeServerResponse(tp, &status);
    if (eor == NULL)
        {
        return;
        }
    tp->isBusy = FALSE;
    if (status == 204)
        {
        sp  = (char *)&tp->inputBuffer.data[4];
        id1 = strtol(sp, &ep, 10);
        id2 = strtol(ep, NULL, 10);
        i   = 0;
        //tp->ioBuffer[i++] = (id1 >> 20) & 0xf0;
        tp->ioBuffer[i++] = 0020;
        tp->ioBuffer[i++] = (id1 >> 12) & 0xff;
        tp->ioBuffer[i++] = id1 & 0xfff;
        while (i < BlockIdLength)
            {
            tp->ioBuffer[i++] = 0;
            }
        tp->recordLength = BlockIdLength;
        tp->bp           = tp->ioBuffer;
        tp->isBOT        = id1 == 0x01000000;
        }
    else
        {
        logDtError(LogErrorLocation, "Unexpected status %d received from StorageTek simulator for READBLOCKID request\n", status);
        mt5744CloseTapeServerConnection(tp);
        }
    mt5744ResetInputBuffer(tp, (u8 *)eor);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Process a response from the StorageTek simulator
**
**  Parameters:     Name        Description.
**                  tp          pointer to tape unit parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mt5744ReceiveTapeServerResponse(TapeParam *tp)
    {
    char *eor;
    int  n;
    int  status;

    n = recv(tp->fd, &tp->inputBuffer.data[tp->inputBuffer.in], sizeof(tp->inputBuffer.data) - tp->inputBuffer.in, 0);
    if (n <= 0)
        {
#if DEBUG
        fprintf(mt5744Log, "\n%010u Disconnected on socket %d from %s:%u for CH:%02o u:%d, %s", traceSequenceNo,
                tp->fd, tp->serverName, ntohs(tp->serverAddr.sin_port), tp->channelNo, tp->unitNo,
                (n == 0) ? "end of stream" : strerror(errno));
#endif
        mt5744CloseTapeServerConnection(tp);
        }
    else
        {
#if DEBUG
        fprintf(mt5744Log, "\n%010u Received %d bytes on socket %d from %s:%u for CH:%02o u:%d\n", traceSequenceNo, n,
                tp->fd, tp->serverName, ntohs(tp->serverAddr.sin_port), tp->channelNo, tp->unitNo);
        mt5744LogBytes(&tp->inputBuffer.data[tp->inputBuffer.in], n);
        mt5744LogFlush();
#endif
        tp->inputBuffer.in += n;
        if (tp->inputBuffer.data[0] == '1') // mount/dismount event
            {
            if (tp->inputBuffer.in > 3)
                {
                eor = mt5744ParseTapeServerResponse(tp, &status);
                if (eor == NULL)
                    {
                    return;
                    }
                switch (status)
                    {
                case 101:
                case 102:
                    mt5744LoadTape(tp, status == 102);
                    break;

                case 103:
                    mt5744UnloadTape(tp);
                    break;

                default:
                    logDtError(LogErrorLocation, "Unrecognized event indication %.3s from %s:%u for CH:%02o u:%d",
                               &tp->inputBuffer.data[0], tp->serverName, ntohs(tp->serverAddr.sin_port), tp->channelNo, tp->unitNo);
                    mt5744CloseTapeServerConnection(tp);
                    break;
                    }
                mt5744ResetInputBuffer(tp, (u8 *)eor);
                }
            }
        else
            {
            tp->callback(tp);
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Process a response from the StorageTek simulator to a
**                  READFWD or READBKW request.
**
**  Parameters:     Name        Description.
**                  tp          pointer to tape unit parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mt5744ReadRequestCallback(TapeParam *tp)
    {
    int  dataIdx;
    char *eor;
    long len;
    char *sp;
    int  status;

    eor = mt5744ParseTapeServerResponse(tp, &status);
    if (eor == NULL)
        {
        return;
        }
    tp->isEOT = FALSE;
    tp->bp    = tp->ioBuffer;
    switch (status)
        {
    case 201:
        sp      = (char *)&tp->inputBuffer.data[4];
        len     = strtol(sp, NULL, 10);
        dataIdx = (int)(eor - (char *)tp->inputBuffer.data);
        if ((long)(tp->inputBuffer.in - dataIdx) < len)
            {
            return;
            }
        tp->recordLength = mt5744PackBytes(tp, (u8 *)eor, len);
        eor      += len;
        tp->isBOT = FALSE;
        break;

    case 202:
        tp->recordLength = 0;
        tp->isBOT        = FALSE;
        tp->isTapeMark   = TRUE;
        break;

    case 203:
        tp->recordLength = 0;
        tp->isBOT        = TRUE;
        break;

    case 505:
        tp->recordLength = 0;
        tp->isBOT        = FALSE;
        //tp->isEOT = TRUE;
        tp->isTapeMark = TRUE; // simulate tape mark instead of end-of-medium
        break;

    default:
        logDtError(LogErrorLocation, "Unexpected status %d received from StorageTek simulator for READFWD/READBKW request\n", status);
        mt5744CloseTapeServerConnection(tp);

        return;
        }
    tp->isBusy = FALSE;
    mt5744ResetInputBuffer(tp, (u8 *)eor);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Sends a tape unit registration request to a StorageTek simulaator
**
**  Parameters:     Name        Description.
**                  tp          pointer to tape unit parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mt5744RegisterUnit(TapeParam *tp)
    {
    u8     *bp;
    size_t len;

    bp  = tp->outputBuffer.data;
    len = strlen(tp->driveName);
    memcpy(bp, "REGISTER ", 9);
    bp += 9;
    memcpy(bp, tp->driveName, len);
    bp += len;
    *bp = '\n';

    tp->outputBuffer.in  = (u32)(len + 10);
    tp->outputBuffer.out = 0;
    tp->state            = StAcsRegistering;
    tp->callback         = mt5744RegisterUnitRequestCallback;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Handles a response to a tape unit registration request
**
**  Parameters:     Name        Description.
**                  tp          pointer to tape unit parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mt5744RegisterUnitRequestCallback(TapeParam *tp)
    {
    char *eor;
    int  status;

    eor = mt5744ParseTapeServerResponse(tp, &status);
    if (eor == NULL)
        {
        return;
        }
    if ((status >= 200) && (status < 300))
        {
        mt5744ResetUnit(tp);
        tp->state = StAcsReady;
        }
    else
        {
        logDtError(LogErrorLocation, "Unexpected status %d received from StorageTek simulator for REGISTER request\n", status);
        mt5744CloseTapeServerConnection(tp);
        }
    mt5744ResetInputBuffer(tp, (u8 *)eor);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Reset input buffer indices to prepare for processing
**                  next available input.
**
**  Parameters:     Name        Description.
**                  tp          pointer to tape parameters
**                  eor         pointer to end + 1 of last request processed
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
static void mt5744ResetInputBuffer(TapeParam *tp, u8 *eor)
    {
    int len;

    if (eor < &tp->inputBuffer.data[tp->inputBuffer.in])
        {
        len = (int)(&tp->inputBuffer.data[tp->inputBuffer.in] - eor);
        memcpy(&tp->inputBuffer.data[0], eor, len);
        tp->inputBuffer.out = 0;
        tp->inputBuffer.in  = len;
        }
    else
        {
        tp->inputBuffer.out = tp->inputBuffer.in = 0;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Reset tape unit status prior to initiating I/O.
**
**  Parameters:     Name        Description.
**                  tp          pointer to tape parameters
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
static void mt5744ResetStatus(TapeParam *tp)
    {
    if (tp != NULL)
        {
        tp->isAlert         = FALSE;
        tp->isBlockNotFound = FALSE;
        tp->isBOT           = FALSE;
        tp->isCharacterFill = FALSE;
        tp->isEOT           = FALSE;
        tp->isTapeMark      = FALSE;
        tp->errorCode       = 0;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Reset tape unit parameters.
**
**  Parameters:     Name        Description.
**                  tp          pointer to tape parameters
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
static void mt5744ResetUnit(TapeParam *tp)
    {
    tp->inputBuffer.out  = tp->inputBuffer.in = 0;
    tp->outputBuffer.out = tp->outputBuffer.in = 0;
    mt5744ResetStatus(tp);
    tp->isBusy         = FALSE;
    tp->isReady        = FALSE;
    tp->isWriteEnabled = FALSE;
    tp->volumeName[0]  = '\0';
    }

/*--------------------------------------------------------------------------
**  Purpose:        Process a response from the StorageTek simulator to a
**                  REWIND request.
**
**  Parameters:     Name        Description.
**                  tp          pointer to tape unit parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mt5744RewindRequestCallback(TapeParam *tp)
    {
    char *eor;
    int  status;

    eor = mt5744ParseTapeServerResponse(tp, &status);
    if (eor == NULL)
        {
        return;
        }
    tp->isBusy = FALSE;
    if (status == 203)
        {
        tp->isBOT = TRUE;
        }
    else
        {
        logDtError(LogErrorLocation, "Unexpected status %d received from StorageTek simulator for REWIND request\n", status);
        mt5744CloseTapeServerConnection(tp);
        }
    mt5744ResetInputBuffer(tp, (u8 *)eor);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Process a response from the StorageTek simulator to a
**                  REWIND request. Unload the tape after rewinding.
**
**  Parameters:     Name        Description.
**                  tp          pointer to tape unit parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mt5744RewindUnloadRequestCallback(TapeParam *tp)
    {
    char *eor;
    int  status;

    eor = mt5744ParseTapeServerResponse(tp, &status);
    if (eor == NULL)
        {
        return;
        }
    tp->isBusy  = FALSE;
    tp->isReady = FALSE;
    if (status != 203)
        {
        logDtError(LogErrorLocation, "Unexpected status %d received from StorageTek simulator for REWIND/UNLOAD request\n", status);
        }
    mt5744ResetInputBuffer(tp, (u8 *)eor);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Send a request to the StorageTek simulator
**
**  Parameters:     Name        Description.
**                  tp          pointer to tape unit parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mt5744SendTapeServerRequest(TapeParam *tp)
    {
    int len;
    int n;

    len = tp->outputBuffer.in - tp->outputBuffer.out;
    n   = send(tp->fd, &tp->outputBuffer.data[tp->outputBuffer.out], len, 0);
    if (n > 0)
        {
#if DEBUG
        fprintf(mt5744Log, "\n%010u Sent %d bytes on socket %d to %s:%u for CH:%02o u:%d\n", traceSequenceNo, n,
                tp->fd, tp->serverName, ntohs(tp->serverAddr.sin_port), tp->channelNo, tp->unitNo);
        mt5744LogBytes(&tp->outputBuffer.data[tp->outputBuffer.out], n);
        mt5744LogFlush();
#endif
        tp->outputBuffer.out += n;
        if (tp->outputBuffer.out >= tp->outputBuffer.in)
            {
            tp->outputBuffer.out = tp->outputBuffer.in = 0;
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Process a response from the StorageTek simulator to a
**                  SPACEBKW or SPACEFWD request.
**
**  Parameters:     Name        Description.
**                  tp          pointer to tape unit parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mt5744SpaceRequestCallback(TapeParam *tp)
    {
    char *eor;
    int  status;

    eor = mt5744ParseTapeServerResponse(tp, &status);
    if (eor == NULL)
        {
        return;
        }
    tp->isBusy = FALSE;
    tp->isEOT  = FALSE;
    tp->isBOT  = FALSE;
    switch (status)
        {
    case 200:
        break;

    case 202:
        tp->isTapeMark = TRUE;
        break;

    case 203:
        tp->isBOT = TRUE;
        break;

    case 505:
        tp->isTapeMark = TRUE;
        //tp->isEOT = TRUE;
        //tp->isAlert = TRUE;
        break;

    default:
        logDtError(LogErrorLocation, "Unexpected status %d received from StorageTek simulator for SPACEFWD/SPACEBKW request\n", status);
        mt5744CloseTapeServerConnection(tp);

        return;
        }
    mt5744ResetInputBuffer(tp, (u8 *)eor);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Unload a mounted tape indicated by StorageTek simulator.
**
**  Parameters:     Name        Description.
**                  tp          pointer to tape unit parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void mt5744UnloadTape(TapeParam *tp)
    {
    mt5744ResetUnit(tp);
#if DEBUG
    fprintf(mt5744Log, "\n%010u Dismount CH:%02o u%d", traceSequenceNo, tp->channelNo, tp->unitNo);
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Process a response from the StorageTek simulator to a
**                  WRITE request.
**
**  Parameters:     Name        Description.
**                  tp          pointer to tape unit parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mt5744WriteRequestCallback(TapeParam *tp)
    {
    char *eor;
    int  status;

    eor = mt5744ParseTapeServerResponse(tp, &status);
    if (eor == NULL)
        {
        return;
        }
    tp->isBusy = FALSE;
    if (status == 200)
        {
        tp->isBOT = FALSE;
        }
    else
        {
        logDtError(LogErrorLocation, "Unexpected status %d received from StorageTek simulator for WRITE request\n", status);
        mt5744CloseTapeServerConnection(tp);
        }
    mt5744ResetInputBuffer(tp, (u8 *)eor);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Process a response from the StorageTek simulator to a
**                  WRITEMARK request.
**
**  Parameters:     Name        Description.
**                  tp          pointer to tape unit parameters
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mt5744WriteMarkRequestCallback(TapeParam *tp)
    {
    char *eor;
    int  status;

    eor = mt5744ParseTapeServerResponse(tp, &status);
    if (eor == NULL)
        {
        return;
        }
    tp->isBusy = FALSE;
    if (status != 200)
        {
        logDtError(LogErrorLocation, "Unexpected status %d received from StorageTek simulator for WRITEMARK request\n", status);
        mt5744CloseTapeServerConnection(tp);
        }
    mt5744ResetInputBuffer(tp, (u8 *)eor);
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
static char *mt5744Func2String(PpWord funcCode)
    {
    static char buf[30];

    switch (funcCode)
        {
    case Fc5744Release:
        return "Release";

    case Fc5744Continue:
        return "Continue";

    case Fc5744Rewind:
        return "Rewind";

    case Fc5744RewindUnload:
        return "RewindUnload";

    case Fc5744GeneralStatus:
        return "GeneralStatus";

    case Fc5744SpaceFwd:
        return "SpaceFwd";

    case Fc5744LocateBlock:
        return "LocateBlock";

    case Fc5744SpaceBkw:
        return "SpaceBkw";

    case Fc5744DetailedStatus:
        return "DetailedStatus";

    case Fc5744ReadBlockId:
        return "ReadBlockId";

    case Fc5744ReadBufferedLog:
        return "ReadBufferedLog";

    case Fc5744Connect + 0:
    case Fc5744Connect + 1:
    case Fc5744Connect + 2:
    case Fc5744Connect + 3:
    case Fc5744Connect + 4:
    case Fc5744Connect + 5:
    case Fc5744Connect + 6:
    case Fc5744Connect + 7:
    case Fc5744Connect + 010:
    case Fc5744Connect + 011:
    case Fc5744Connect + 012:
    case Fc5744Connect + 013:
    case Fc5744Connect + 014:
    case Fc5744Connect + 015:
    case Fc5744Connect + 016:
    case Fc5744Connect + 017:
        return "Connect";

    case Fc5744ConnectAndSelectCompression + 0:
    case Fc5744ConnectAndSelectCompression + 1:
    case Fc5744ConnectAndSelectCompression + 2:
    case Fc5744ConnectAndSelectCompression + 3:
    case Fc5744ConnectAndSelectCompression + 4:
    case Fc5744ConnectAndSelectCompression + 5:
    case Fc5744ConnectAndSelectCompression + 6:
    case Fc5744ConnectAndSelectCompression + 7:
    case Fc5744ConnectAndSelectCompression + 010:
    case Fc5744ConnectAndSelectCompression + 011:
    case Fc5744ConnectAndSelectCompression + 012:
    case Fc5744ConnectAndSelectCompression + 013:
    case Fc5744ConnectAndSelectCompression + 014:
    case Fc5744ConnectAndSelectCompression + 015:
    case Fc5744ConnectAndSelectCompression + 016:
    case Fc5744ConnectAndSelectCompression + 017:
        return "ConnectAndSelectCompression";

    case Fc5744ReadFwd:
        return "ReadFwd";

    case Fc5744ReadBkw:
        return "ReadBkw";

    case Fc5744Write:
        return "Write";

    case Fc5744WriteShort:
        return "WriteShort";

    case Fc5744WriteTapeMark:
        return "WriteTapeMark";

    case Fc5744Autoload:
        return "Autoload";
        }
    sprintf(buf, "UNKNOWN: %04o", funcCode);

    return (buf);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Flush incomplete data line
**
**  Parameters:     Name        Description.
**
**  Returns:        nothing
**
**------------------------------------------------------------------------*/
static void mt5744LogFlush(void)
    {
    if (mt5744LogBytesCol > 0)
        {
        fputs(mt5744LogBuf, mt5744Log);
        fputc('\n', mt5744Log);
        fflush(mt5744Log);
        }
    mt5744LogBytesCol = 0;
    memset(mt5744LogBuf, ' ', LogLineLength);
    mt5744LogBuf[LogLineLength] = '\0';
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
static void mt5744LogBytes(u8 *bytes, int len)
    {
    u8   ac;
    int  ascCol;
    u8   b;
    char hex[3];
    int  hexCol;
    int  i;

    ascCol = AsciiColumn(mt5744LogBytesCol);
    hexCol = HexColumn(mt5744LogBytesCol);

    for (i = 0; i < len; i++)
        {
        b  = bytes[i];
        ac = b;
        if ((ac < 0x20) || (ac >= 0x7f))
            {
            ac = '.';
            }
        sprintf(hex, "%02x", b);
        memcpy(mt5744LogBuf + hexCol, hex, 2);
        hexCol += 3;
        mt5744LogBuf[ascCol++] = ac;
        if (++mt5744LogBytesCol >= 16)
            {
            mt5744LogFlush();
            ascCol = AsciiColumn(mt5744LogBytesCol);
            hexCol = HexColumn(mt5744LogBytesCol);
            }
        }
    }

#endif // DEBUG

/*---------------------------  End Of File  ------------------------------*/
