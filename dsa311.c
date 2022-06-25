/*--------------------------------------------------------------------------
**
**  Copyright (c) 2022, Tom Hunter, Kevin Jordan
**
**  Name: dsa311.c
**
**  Description:
**      Perform emulation of CDC 3000 series 3266 mux with 311 digital
**      serial adapter. The equipment number must be in the range 4 - 7,
**      and the unit number must be even and in the range 0 - 76 (octal).
**
**      This module is designed to interoperate with the Hercules 2703
**      BSC simulator and the DtCyber HASP (npu_hasp.c) module. This
**      enables the NOS 1 TIELINE subsystem to interoperate with JES2
**      on IBM MVS, RSCS on IBM VM/CMS, and RBF on NOS 2.
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
#if defined(_WIN32)
#include <winsock.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
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
#define SOH                          0x01
#define STX                          0x02
#define DLE                          0x10
#define ETB                          0x26
#define ENQ                          0x2d
#define SYN                          0x32
#define NAK                          0x3d
#define ACK0                         0x70

/*
**      DSA 311 specific function codes
*/
#define FcDsa311DisableInterrupts    0300

/*
**      Special output characters
*/
#define Dsa311RequestSend            0042
#define Dsa311Resync                 0045

/*
**      Status reply bits
*/
#define Dsa311InputReady             04000
#define Dsa311OutputReady            02000
#define Dsa311InputLost              01000
#define Dsa311ErrorMask              00007

#define IoTurnsPerPoll               4
#define PpInBufSize                  1032
#define SktInBufSize                 1024
#define SktOutBufSize                1024

#define ConnectionRetryInterval      30

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

typedef enum
    {
    StDsa311MajDisconnected = 0,
    StDsa311MajConnecting,
    StDsa311MajConnected
    } Dsa311MajorState;

typedef enum
    {
    StDsa311InpDLE1,
    StDsa311InpSTX,
    StDsa311InpDLE2,
    StDsa311InpETB
    } Dsa311InputState;

typedef enum
    {
    StDsa311OutSOH = 0,
    StDsa311OutENQ,
    StDsa311OutDLE1,
    StDsa311OutDLE2,
    StDsa311OutETB1,
    StDsa311OutETB2,
    StDsa311OutCRC1,
    StDsa311OutCRC2
    } Dsa311OutputState;

typedef struct buffer
    {
    int in;
    int out;
    u8  *data;
    } Buffer;

typedef struct dsa311Context
    {
    Dsa311MajorState   majorState;
    Dsa311InputState   inputState;
    Dsa311OutputState  outputState;
    struct sockaddr_in serverAddr;
#if defined(_WIN32)
    SOCKET             fd;
#else
    int                fd;
#endif
    int                ioTurns;
    time_t             nextConnectAttempt;
    bool               isRTS;
    u16                crc;
    Buffer             sktInBuf;
    Buffer             ppInBuf;
    Buffer             sktOutBuf;
    } Dsa311Context;

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static void dsa311Activate(void);
static void dsa311CheckIo(Dsa311Context *cp);
static void dsa311CloseConnection(Dsa311Context *cp);
static void dsa311Disconnect(void);
static FcStatus dsa311Func(PpWord funcCode);
static void dsa311InitiateConnection(Dsa311Context *cp);
static void dsa311Io(void);
static void dsa311Receive(Dsa311Context *cp);
static void dsa311Reset(Dsa311Context *cp);
static void dsa311Send(Dsa311Context *cp);
static void dsa311UpdateCRC(Dsa311Context *cp, u8 b);

#if DEBUG
static char *dsa311Func2String(PpWord funcCode);
static void dsa311LogBytes(u8 *bytes, int len);
static void dsa311LogFlush(void);

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
#if DEBUG
static u64 baseTime;
static u64 elapsedTime;
#endif

/*
**  Table for CRC-16 based upon polynomial x^16 + x^15 + x^2 + 1
*/
static u16 crc16Table[256] =
    {
    0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
    0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
    0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
    0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
    0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
    0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
    0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
    0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
    0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
    0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
    0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
    0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
    0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
    0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
    0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
    0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
    0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
    0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
    0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
    0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
    0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
    0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
    0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
    0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
    0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
    0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
    0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
    0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
    0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
    0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
    0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
    0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040
    };

#if DEBUG
static FILE *dsa311Log = NULL;
static char dsa311LogBuf[LogLineLength + 1];
static int  dsa311LogBytesCol = 0;
#endif

/*
 **--------------------------------------------------------------------------
 **
 **  Public Functions
 **
 **--------------------------------------------------------------------------
 */

/*--------------------------------------------------------------------------
**  Purpose:        Initialise 3266/DSA 311.
**
**  Parameters:     Name        Description.
**                  eqNo        equipment number
**                  unitNo      unit number
**                  channelNo   channel number the device is attached to
**                  params      connection parameters (i.e., hostname:port)
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void dsa311Init(u8 eqNo, u8 unitNo, u8 channelNo, char *params)
    {
    Dsa311Context  *cp;
    DevSlot        *dp;
    struct hostent *hp;
    char           *serverName;
    u16            serverPort;
    char           *sp;
    long           value;

#if DEBUG
    if (dsa311Log == NULL)
        {
        dsa311Log = fopen("dsa311log.txt", "wt");
        }
    baseTime = getMilliseconds();
#endif

    if ((eqNo < 4) || (eqNo > 7))
        {
        fputs("(dsa311 ) Equipment number must be 4, 5, 6, or 7\n", stderr);
        exit(1);
        }
    if (unitNo >= MaxUnits2)
        {
        fprintf(stderr, "(dsa311 ) Unit number must be less than %o for DSA311 on channel %o equipment %o unit %o\n",
                MaxUnits2, channelNo, eqNo, unitNo);
        exit(1);
        }
    if ((unitNo & 1) != 0)
        {
        fprintf(stderr, "(dsa311 ) Unit number must be even for DSA311 on channel %o equipment %o unit %o\n",
                channelNo, eqNo, unitNo);
        exit(1);
        }
    if (params == NULL)
        {
        fprintf(stderr, "(dsa311 ) HASP host connection information required for DSA311 on channel %o equipment %o unit %o\n",
                channelNo, eqNo, unitNo);
        exit(1);
        }

    dp = dcc6681Attach(channelNo, eqNo, unitNo, DtDsa311);

    if (dp->context[unitNo] != NULL)
        {
        fprintf(stderr, "(dsa311 ) Duplicate DSA311 unit number %o on channel %o equipment %o\n",
                unitNo, channelNo, eqNo);
        exit(1);
        }

    dp->activate   = dsa311Activate;
    dp->disconnect = dsa311Disconnect;
    dp->func       = dsa311Func;
    dp->io         = dsa311Io;

    cp = (Dsa311Context *)calloc(1, sizeof(Dsa311Context));
    if (cp == NULL)
        {
        fprintf(stderr, "(dsa311 ) Failed to allocate DSA311 context block\n");
        exit(1);
        }

    dp->context[unitNo]     = (void *)cp;
    dp->context[unitNo + 1] = (void *)cp;
    cp->majorState          = StDsa311MajDisconnected;
    cp->ioTurns             = IoTurnsPerPoll - 1;
    cp->nextConnectAttempt  = 0;
    cp->fd             = 0;
    cp->isRTS          = FALSE;
    cp->ppInBuf.data   = (u8 *)calloc(1, PpInBufSize);
    cp->sktInBuf.data  = (u8 *)calloc(1, SktInBufSize);
    cp->sktOutBuf.data = (u8 *)calloc(1, SktOutBufSize);
    if ((cp->ppInBuf.data == NULL) || (cp->sktInBuf.data == NULL) || (cp->sktOutBuf.data == NULL))
        {
        fprintf(stderr, "(dsa311 ) Failed to allocate DSA311 I/O buffer\n");
        exit(1);
        }
    dsa311Reset(cp);

    /*
    **  Set up server connection
    */
    serverName = params;
    serverPort = 0;
    sp         = strchr(params, ':');
    if (sp != NULL)
        {
        *sp++ = '\0';
        value = strtol(sp, NULL, 10);
        if ((value > 0) && (value < 0x10000))
            {
            serverPort = (u16)value;
            }
        }
    if (serverPort == 0)
        {
        fprintf(stderr,
                "(dsa311 ) Invalid HASP host connection specification for DSA311 on channel %o equipment %o unit %o\n",
                channelNo, eqNo, unitNo);
        exit(1);
        }
    hp = gethostbyname(serverName);
    if (hp == NULL)
        {
        fprintf(stderr, "(dsa311 ) Failed to lookup address of DSA311 HASP host %s\n", serverName);
        exit(1);
        }
    cp->serverAddr.sin_family = AF_INET;
    memcpy(&cp->serverAddr.sin_addr.s_addr, (struct in_addr *)hp->h_addr_list[0], sizeof(struct in_addr));
    cp->serverAddr.sin_port = htons(serverPort);

    /*
    **  Print a friendly message.
    */
    printf("(dsa311 ) Initialised on channel %o equipment %o unit %o\n",
           channelNo, eqNo, unitNo);
    }

/*
 **--------------------------------------------------------------------------
 **
 **  Private Functions
 **
 **--------------------------------------------------------------------------
 */

/*--------------------------------------------------------------------------
**  Purpose:        Execute function code on DSA 311.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        FcStatus
**
**------------------------------------------------------------------------*/
static FcStatus dsa311Func(PpWord funcCode)
    {
    Dsa311Context *cp;
    i8            unitNo;

#if DEBUG
    elapsedTime = getMilliseconds() - baseTime;
#endif

    unitNo = active3000Device->selectedUnit;
    if (unitNo < MaxUnits2)
        {
        cp = (Dsa311Context *)active3000Device->context[unitNo];
        }
    else
        {
        cp = NULL;
        }
    if (cp == NULL)
        {
        return (FcDeclined);
        }

    if ((cp->majorState == StDsa311MajDisconnected) && (time(0) >= cp->nextConnectAttempt))
        {
        dsa311InitiateConnection(cp);
        }

    // Start with the common codes
    switch (funcCode)
        {
    case FcDsa311DisableInterrupts:
#if DEBUG
        fprintf(dsa311Log, "\n%010lu PP:%02o CH:%02o U:%02o f:%04o T:%s",
                elapsedTime, activePpu->id, activeDevice->channel->id, unitNo,
                funcCode, dsa311Func2String(funcCode));
#endif
        //
        // 1TL disables interrupts during its preset phase, so use this as an
        // indication that the connection to the HASP host should be re-established
        // if it has been established already.
        //
        if (cp->majorState == StDsa311MajConnected)
            {
            dsa311CloseConnection(cp);
            }

        return (FcProcessed);

    case Fc6681Input:
    case Fc6681Output:
        active3000Device->fcode = funcCode;

        return (FcAccepted);

    case Fc6681MasterClear:
#if DEBUG
        fprintf(dsa311Log, "\n%010lu PP:%02o CH:%02o U:%02o f:%04o T:%s  >  ",
                elapsedTime, activePpu->id, activeDevice->channel->id, unitNo,
                funcCode, dsa311Func2String(funcCode));
#endif
        active3000Device->selectedUnit = -1;
        for (unitNo = 0; unitNo < MaxUnits2; unitNo += 2)
            {
            cp = (Dsa311Context *)active3000Device->context[unitNo];
            if (cp != NULL)
                {
                dsa311Reset(cp);
                }
            }

        return (FcProcessed);

    default:
#if DEBUG
        fprintf(dsa311Log, "\n%010lu PP:%02o CH:%02o U:%02o f:%04o T:%s  >  ",
                elapsedTime, activePpu->id, activeDevice->channel->id, unitNo,
                funcCode, dsa311Func2String(funcCode));
#endif

        return (FcDeclined);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Perform I/O on DSA 311.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void dsa311Io(void)
    {
    u8            ch;
    Dsa311Context *cp;
    i8            unitNo;

#if DEBUG
    elapsedTime = getMilliseconds() - baseTime;
#endif

    /*
    **  Setup selected unit context.
    */
    unitNo = active3000Device->selectedUnit;
    if (unitNo < MaxUnits2)
        {
        cp = (Dsa311Context *)active3000Device->context[unitNo];
        }
    else
        {
        cp = NULL;
        }

    if (cp == NULL)
        {
        return;
        }

    dsa311CheckIo(cp);

    /*
    **  Process DSA 311 I/O.
    */
    switch (active3000Device->fcode)
        {
    default:
        activeChannel->full = FALSE;
        break;

    case Fc6681Input:
        activeChannel->full = TRUE;
        if ((unitNo & 1) == 1)       // input control port
            {
            activeChannel->data = 0; // bit 9 indicates data lost
#if DEBUG
            fprintf(dsa311Log, "\n%010lu PP:%02o CH:%02o U:%02o f:%04o T:%s  <  %04o",
                    elapsedTime, activePpu->id, activeDevice->channel->id, unitNo,
                    active3000Device->fcode, dsa311Func2String(active3000Device->fcode),
                    activeChannel->data);
#endif
            }
        else // input data port
            {
            activeChannel->data = 0;
            if (cp->majorState == StDsa311MajConnected)
                {
                if ((cp->isRTS == TRUE) && (cp->sktOutBuf.in < SktOutBufSize))
                    {
                    activeChannel->data |= Dsa311OutputReady;
                    }
                if (cp->ppInBuf.out < cp->ppInBuf.in)
                    {
                    activeChannel->data |= Dsa311InputReady
                                           | cp->ppInBuf.data[cp->ppInBuf.out++];
                    if (cp->ppInBuf.out >= cp->ppInBuf.in)
                        {
                        cp->ppInBuf.out = cp->ppInBuf.in = 0;
                        }
                    }
                }
#if DEBUG
            if ((activeChannel->data & Dsa311InputReady) != 0)
                {
                fprintf(dsa311Log, "\n%010lu PP:%02o CH:%02o U:%02o f:%04o T:%s  <  %04o",
                        elapsedTime, activePpu->id, activeDevice->channel->id, unitNo,
                        active3000Device->fcode, dsa311Func2String(active3000Device->fcode),
                        activeChannel->data);
                }
#endif
            }
        break;

    case Fc6681Output:
        if (activeChannel->full)
            {
#if DEBUG
            fprintf(dsa311Log, "\n%010lu PP:%02o CH:%02o U:%02o f:%04o T:%s  >  %04o",
                    elapsedTime, activePpu->id, activeDevice->channel->id, unitNo,
                    active3000Device->fcode, dsa311Func2String(active3000Device->fcode),
                    activeChannel->data);
#endif
            ch = activeChannel->data & 0xff;
            if ((unitNo & 1) == 1) // output control port
                {
                // activeChannel->data is typically one of:
                //   Dsa311RequestSend 0042
                //   Dsa311Resync      0045
                //
                if (ch == Dsa311RequestSend)
                    {
                    cp->isRTS = TRUE;
                    }
                else if (ch == Dsa311Resync)
                    {
                    cp->isRTS = FALSE;
                    }
                }
            else // output data port
                {
                switch (cp->outputState)
                {
                //
                //  Look for beginning of a message, and discard characters
                //  until SOH or DLE is seen.
                //    SOH indicates beginning of communication (SOH-ENQ), or
                //    beginning of a non-transparent message (SOH-STX).  DLE
                //    indicates beginning of transparent message.
                //
                case StDsa311OutSOH:
                    if (ch == SOH)
                        {
                        cp->outputState = StDsa311OutENQ;
                        }
                    else if (ch == DLE)
                        {
                        cp->sktOutBuf.data[cp->sktOutBuf.in++] = ch;
                        cp->outputState = StDsa311OutDLE1;
                        }
                    else if (ch == NAK)
                        {
                        cp->sktOutBuf.data[cp->sktOutBuf.in++] = SYN;
                        cp->sktOutBuf.data[cp->sktOutBuf.in++] = ch;
                        }
                    break;

                //
                //  ENQ indicates SOH-ENQ (beginning of communication), and
                //  STX indicates beginning of non-transparent message.
                //
                case StDsa311OutENQ:
                    if (ch == ENQ)
                        {
                        cp->sktOutBuf.data[cp->sktOutBuf.in++] = SOH;
                        cp->sktOutBuf.data[cp->sktOutBuf.in++] = ch;
                        cp->outputState = StDsa311OutSOH;
                        }
                    else if (ch == STX)
                        {
                        cp->sktOutBuf.data[cp->sktOutBuf.in++] = DLE;
                        cp->sktOutBuf.data[cp->sktOutBuf.in++] = ch;
                        cp->outputState = StDsa311OutETB1;
                        }
                    else
                        {
                        cp->outputState = StDsa311OutSOH;
                        }
                    break;

                //
                //  Look for non-transparent end of message, ETB
                //
                case StDsa311OutETB1:
                    if (cp->sktOutBuf.in + 1 < SktOutBufSize)
                        {
                        switch (ch)
                        {
                        case SYN:
                            // discard trailing SYN's
                            break;

                        case SOH:
                            cp->outputState = StDsa311OutSOH;
                            break;

                        case ETB:
                            cp->outputState = StDsa311OutCRC1;

                        case STX:
                        case DLE:
                            cp->sktOutBuf.data[cp->sktOutBuf.in++] = DLE;

                        // fall through
                        default:
                            cp->sktOutBuf.data[cp->sktOutBuf.in++] = ch;
                            break;
                        }
                        }
                    break;

                //
                //  Process character following DLE. If it is ACK0, then the
                //  the message is a simple acknowledgement. Otherwise, it is
                //  a transparent escape, so output the next character and
                //  look for end of message.
                //
                case StDsa311OutDLE1:
                    if (cp->sktOutBuf.in < SktOutBufSize)
                        {
                        cp->sktOutBuf.data[cp->sktOutBuf.in++] = ch;
                        }
                    if (ch == ACK0)
                        {
                        cp->outputState = StDsa311OutSOH;
                        }
                    else
                        {
                        cp->outputState = StDsa311OutDLE2;
                        }
                    break;

                //
                //  Look for transparent end of message, DLE-ETB
                //
                case StDsa311OutDLE2:
                    if (cp->sktOutBuf.in < SktOutBufSize)
                        {
                        cp->sktOutBuf.data[cp->sktOutBuf.in++] = ch;
                        }
                    if (ch == DLE)
                        {
                        cp->outputState = StDsa311OutETB2;
                        }
                    break;

                case StDsa311OutETB2:
                    if (cp->sktOutBuf.in < SktOutBufSize)
                        {
                        cp->sktOutBuf.data[cp->sktOutBuf.in++] = ch;
                        }
                    cp->outputState = (ch == ETB) ? StDsa311OutCRC1 : StDsa311OutDLE2;
                    break;

                //
                //  Discard two CRC bytes
                //
                case StDsa311OutCRC1:     //discard CRC byte
                    cp->outputState = StDsa311OutCRC2;
                    break;

                case StDsa311OutCRC2:     //discard CRC byte
                    cp->outputState = StDsa311OutSOH;
                    break;

                default:
#if DEBUG
                    fprintf(dsa311Log, "\n%010lu unrecognized output state: %d", elapsedTime, cp->outputState);
#endif
                    cp->outputState = StDsa311OutSOH;
                    break;
                }
                }
            activeChannel->full = FALSE;
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
static void dsa311Activate(void)
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
static void dsa311Disconnect(void)
    {
    if (active3000Device->fcode == Fc6681Output)
        {
        active3000Device->fcode = 0;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Compute a CRC-16 against the PP input buffer and append
**                  it to the PP input buffer.
**
**  Parameters:     Name        Description.
**                  cp          pointer to DSA311 context.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void dsa311AppendCRC(Dsa311Context *cp)
    {
    cp->ppInBuf.data[cp->ppInBuf.in++] = cp->crc >> 8;
    cp->ppInBuf.data[cp->ppInBuf.in++] = cp->crc & 0xff;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Check for I/O availability.
**
**  Parameters:     Name        Description.
**                  cp          pointer to DSA311 context.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void dsa311CheckIo(Dsa311Context *cp)
    {
    int maxFd;
    int n;
    int optEnable = 1;

#if defined(_WIN32)
    u_long blockEnable = 1;
    int    optLen;
    int    optVal;
#else
    socklen_t optLen;
    int       optVal;
#endif
    int            rc;
    fd_set         readFds;
    struct timeval timeout;
    fd_set         writeFds;

    cp->ioTurns = (cp->ioTurns + 1) % IoTurnsPerPoll;
    if (cp->ioTurns != 0)
        {
        return;
        }

    if (cp->majorState == StDsa311MajConnecting)
        {
        FD_ZERO(&writeFds);
        FD_SET(cp->fd, &writeFds);
        timeout.tv_sec  = 0;
        timeout.tv_usec = 0;
        n = select(cp->fd + 1, NULL, &writeFds, NULL, &timeout);
        if (n <= 0)
            {
#if DEBUG
            if (n < 0)
                {
                fprintf(dsa311Log, "\n%010lu select failed", elapsedTime);
                }
#endif

            return;
            }
        if (FD_ISSET(cp->fd, &writeFds) == FALSE)
            {
            return;
            }
#if defined(_WIN32)
        optLen = sizeof(optVal);
        rc     = getsockopt(cp->fd, SOL_SOCKET, SO_ERROR, (char *)&optVal, &optLen);
#else
        optLen = (socklen_t)sizeof(optVal);
        rc     = getsockopt(cp->fd, SOL_SOCKET, SO_ERROR, &optVal, &optLen);
#endif
        if (rc < 0)
            {
#if DEBUG
            fprintf(dsa311Log, "\n%010lu failed to get socket status", elapsedTime);
#endif
            dsa311CloseConnection(cp);

            return;
            }
        else if (optVal != 0) // connection failed
            {
#if DEBUG
            fprintf(dsa311Log, "\n%010lu failed to connect", elapsedTime);
#endif
            dsa311CloseConnection(cp);

            return;
            }
#if DEBUG
        fprintf(dsa311Log, "\n%010lu connected", elapsedTime);
#endif

        /*
        **  Set Keepalive option so that we can eventually discover if
        **  a client has been rebooted.
        */
        setsockopt(cp->fd, SOL_SOCKET, SO_KEEPALIVE, (void *)&optEnable, sizeof(optEnable));

        /*
        **  Make socket non-blocking.
        */
#if defined(_WIN32)
        ioctlsocket(cp->fd, FIONBIO, &blockEnable);
#else
        fcntl(cp->fd, F_SETFL, O_NONBLOCK);
#endif
        cp->majorState = StDsa311MajConnected;
        dsa311Reset(cp);
        }

    FD_ZERO(&readFds);
    FD_ZERO(&writeFds);
    maxFd = 0;
    if (cp->sktInBuf.in < SktInBufSize)
        {
        FD_SET(cp->fd, &readFds);
        maxFd = cp->fd;
        }
    if (cp->sktOutBuf.out < cp->sktOutBuf.in)
        {
        FD_SET(cp->fd, &writeFds);
        maxFd = cp->fd;
        }
    if (maxFd == 0)
        {
        return;
        }

    timeout.tv_sec  = 0;
    timeout.tv_usec = 0;

    n = select(maxFd + 1, &readFds, &writeFds, NULL, &timeout);

    if (n <= 0)
        {
#if DEBUG
        if (n < 0)
            {
            fprintf(dsa311Log, "\n%010lu select failed", elapsedTime);
            }
#endif

        return;
        }
    if (FD_ISSET(cp->fd, &readFds))
        {
        dsa311Receive(cp);
        }
    if (FD_ISSET(cp->fd, &writeFds))
        {
        dsa311Send(cp);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Close a connection
**
**  Parameters:     Name        Description.
**                  cp          pointer to DSA311 context.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void dsa311CloseConnection(Dsa311Context *cp)
    {
#if defined(_WIN32)
    closesocket(cp->fd);
#else
    close(cp->fd);
#endif
    cp->fd = 0;
    cp->nextConnectAttempt = time(0) + (time_t)ConnectionRetryInterval;
    cp->majorState         = StDsa311MajDisconnected;
#if DEBUG
    fprintf(dsa311Log, "\n%010lu closed connection", elapsedTime);
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Initiate a non-blocking TCP connect request
**
**  Parameters:     Name        Description.
**                  cp          pointer to DSA311 context.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void dsa311InitiateConnection(Dsa311Context *cp)
    {
#if defined(_WIN32)
    u_long blockEnable = 1;
#endif
    int optEnable = 1;
    int rc;

    cp->nextConnectAttempt = time(0) + (time_t)ConnectionRetryInterval;
    cp->fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#if defined(_WIN32)
    if (cp->fd == INVALID_SOCKET)
        {
#if DEBUG
        fprintf(dsa311Log, "\n%010lu failed to create socket", elapsedTime);
#endif

        return;
        }
    setsockopt(cp->fd, SOL_SOCKET, SO_KEEPALIVE, (void *)&optEnable, sizeof(optEnable));
    ioctlsocket(cp->fd, FIONBIO, &blockEnable);
    rc = connect(cp->fd, (struct sockaddr *)&cp->serverAddr, sizeof(cp->serverAddr));
    if ((rc == SOCKET_ERROR) && (WSAGetLastError() != WSAEWOULDBLOCK))
        {
#if DEBUG
        fprintf(dsa311Log, "\n%010lu connect request failed", elapsedTime);
#endif
        closesocket(cp->fd);
        }
    else // connection in progress
        {
#if DEBUG
        fprintf(dsa311Log, "\n%u010u connection initiated", elapsedTime);
#endif
        cp->majorState = StDsa311MajConnecting;
        }
#else
    if (cp->fd < 0)
        {
#if DEBUG
        fprintf(dsa311Log, "\n%010lu failed to create socket", elapsedTime);
#endif

        return;
        }
    setsockopt(cp->fd, SOL_SOCKET, SO_KEEPALIVE, (void *)&optEnable, sizeof(optEnable));
    fcntl(cp->fd, F_SETFL, O_NONBLOCK);
    rc = connect(cp->fd, (struct sockaddr *)&cp->serverAddr, sizeof(cp->serverAddr));
    if ((rc < 0) && (errno != EINPROGRESS))
        {
#if DEBUG
        fprintf(dsa311Log, "\n%010lu connect request failed", elapsedTime);
#endif
        close(cp->fd);
        }
    else // connection in progress
        {
#if DEBUG
        fprintf(dsa311Log, "\n%010lu connection initiated", elapsedTime);
#endif
        cp->majorState = StDsa311MajConnecting;
        }
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Process data received from TCP socket.
**
**  Parameters:     Name        Description.
**                  cp          pointer to DSA311 context
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void dsa311Receive(Dsa311Context *cp)
    {
    u8  b;
    int n;

    n = recv(cp->fd, &cp->sktInBuf.data[cp->sktInBuf.in], SktInBufSize - cp->sktInBuf.in, 0);
    if (n <= 0)
        {
#if DEBUG
        if (n < 0)
            {
            fprintf(dsa311Log, "\n%010lu recv failed", elapsedTime);
            }
        else
            {
            fprintf(dsa311Log, "\n%010lu connection closed", elapsedTime);
            }
#endif
        dsa311CloseConnection(cp);

        return;
        }
#if DEBUG
    fprintf(dsa311Log, "\n%010lu received data", elapsedTime);
    dsa311LogBytes(&cp->sktInBuf.data[cp->sktInBuf.in], n);
#endif
    cp->sktInBuf.in += n;
    while (cp->sktInBuf.out < cp->sktInBuf.in
           && cp->ppInBuf.in + 2 < PpInBufSize)
        {
        b = cp->sktInBuf.data[cp->sktInBuf.out++];
        if (cp->sktInBuf.out >= cp->sktInBuf.in)
            {
            cp->sktInBuf.out = cp->sktInBuf.in = 0;
            }
        switch (cp->inputState)
            {
        case StDsa311InpDLE1:
            if (b == NAK)
                {
                cp->ppInBuf.data[cp->ppInBuf.in++] = SYN;
                cp->ppInBuf.data[cp->ppInBuf.in++] = b;
                }
            else if (b == DLE)
                {
                cp->inputState = StDsa311InpSTX;
                }
            break;

        case StDsa311InpSTX:
            if (b == STX)
                {
                cp->ppInBuf.data[cp->ppInBuf.in++] = SOH;
                cp->ppInBuf.data[cp->ppInBuf.in++] = b;
                cp->crc = 0;
                dsa311UpdateCRC(cp, SOH);
                dsa311UpdateCRC(cp, b);
                cp->inputState = StDsa311InpDLE2;
                }
            else if (b == ACK0)
                {
                cp->ppInBuf.data[cp->ppInBuf.in++] = DLE;
                cp->ppInBuf.data[cp->ppInBuf.in++] = b;
                cp->inputState = StDsa311InpDLE1;
                }
            break;

        case StDsa311InpDLE2:
            if (b == DLE)
                {
                cp->inputState = StDsa311InpETB;
                }
            else
                {
                cp->ppInBuf.data[cp->ppInBuf.in++] = b;
                dsa311UpdateCRC(cp, b);
                }
            break;

        case StDsa311InpETB:
            cp->ppInBuf.data[cp->ppInBuf.in++] = b;
            dsa311UpdateCRC(cp, b);
            if (b == ETB)
                {
                dsa311AppendCRC(cp);
                cp->inputState = StDsa311InpDLE1;
                }
            break;

        default:
#if DEBUG
            fprintf(dsa311Log, "\n%010lu unrecognized input state: %d", elapsedTime, cp->inputState);
#endif
            cp->inputState = StDsa311InpDLE1;
            break;
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Reset DSA 311 context.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void dsa311Reset(Dsa311Context *cp)
    {
    cp->ppInBuf.in   = cp->ppInBuf.out = 0;
    cp->sktInBuf.in  = cp->sktInBuf.out = 0;
    cp->sktOutBuf.in = cp->sktOutBuf.out = 0;
    cp->inputState   = StDsa311InpDLE1;
    cp->outputState  = StDsa311OutSOH;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Process data to be sent on TCP socket.
**
**  Parameters:     Name        Description.
**                  cp          pointer to DSA311 context
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void dsa311Send(Dsa311Context *cp)
    {
    int n;

    n = send(cp->fd, &cp->sktOutBuf.data[cp->sktOutBuf.out], cp->sktOutBuf.in - cp->sktOutBuf.out, 0);
    if (n < 0)
        {
#if DEBUG
        fprintf(dsa311Log, "\n%010lu send failed", elapsedTime);
#endif
        dsa311CloseConnection(cp);

        return;
        }
#if DEBUG
    fprintf(dsa311Log, "\n%010lu sent data", elapsedTime);
    dsa311LogBytes(&cp->sktOutBuf.data[cp->sktOutBuf.out], n);
#endif
    cp->sktOutBuf.out += n;
    if (cp->sktOutBuf.out >= cp->sktOutBuf.in)
        {
        cp->sktOutBuf.out = cp->sktOutBuf.in = 0;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Update the CRC assoicated with a connection
**
**  Parameters:     Name        Description.
**                  cp          pointer to DSA311 context
**                  b           byte with which to update CRC
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void dsa311UpdateCRC(Dsa311Context *cp, u8 b)
    {
    cp->crc = (cp->crc >> 8) ^ crc16Table[(cp->crc ^ b) & 0xff];
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
static char *dsa311Func2String(PpWord funcCode)
    {
    static char buf[30];

    switch (funcCode)
        {
    case FcDsa311DisableInterrupts:
        return "DisableInterrupts";

    case Fc6681FunctionMode2:
        return "FunctionMode2    ";

    case Fc6681Input:
        return "Input            ";

    case Fc6681Output:
        return "Output           ";

    case Fc6681MasterClear:
        return "MasterClear      ";

    default:
        sprintf(buf, "UNKNOWN %04o ", funcCode);

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
static void dsa311LogFlush(void)
    {
    if (dsa311LogBytesCol > 0)
        {
        fprintf(dsa311Log, "\n%010lu %s", elapsedTime, dsa311LogBuf);
        fflush(dsa311Log);
        }
    dsa311LogBytesCol = 0;
    memset(dsa311LogBuf, ' ', LogLineLength);
    dsa311LogBuf[LogLineLength] = '\0';
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
static void dsa311LogBytes(u8 *bytes, int len)
    {
    u8   ac;
    int  ascCol;
    u8   b;
    char hex[3];
    int  hexCol;
    int  i;

    dsa311LogBytesCol = 0;
    dsa311LogFlush(); // initialize the log buffer
    ascCol = AsciiColumn(dsa311LogBytesCol);
    hexCol = HexColumn(dsa311LogBytesCol);

    for (i = 0; i < len; i++)
        {
        b  = bytes[i];
        ac = ebcdicToAscii[b];
        if ((ac < 0x20) || (ac >= 0x7f))
            {
            ac = '.';
            }
        sprintf(hex, "%02x", b);
        memcpy(dsa311LogBuf + hexCol, hex, 2);
        hexCol += 3;
        dsa311LogBuf[ascCol++] = ac;
        if (++dsa311LogBytesCol >= 16)
            {
            dsa311LogFlush();
            ascCol = AsciiColumn(dsa311LogBytesCol);
            hexCol = HexColumn(dsa311LogBytesCol);
            }
        }
    dsa311LogFlush();
    }

#endif

/*---------------------------  End Of File  ------------------------------*/
