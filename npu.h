#ifndef NPU_H
#define NPU_H

/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**
**  Name: npu.h
**
**  Description:
**      This file defines NPU constants, types, variables, macros and
**      function prototypes.
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
#if defined(_WIN32)
#include <winsock.h>
#else
#include <netdb.h>
#endif

/*
**  -------------
**  NPU Constants
**  -------------
*/

/*
**  Block offsets.
*/
#define BlkOffDN                   0
#define BlkOffSN                   1
#define BlkOffCN                   2
#define BlkOffBTBSN                3
#define BlkOffData                 4
#define BlkOffDbc                  4
#define BlkOffPfc                  4
#define BlkOffSfc                  5
#define BlkOffP3                   6
#define BlkOffP4                   7
#define BlkOffP5                   8
#define BlkOffL7DBC                5
#define BlkOffL7BL                 6
#define BlkOffL7BN                 8
#define BlkOffL7UB                 10

#define BlkShiftBT                 0
#define BlkMaskBT                  017

#define BlkShiftBSN                4
#define BlkMaskBSN                 7

#define BlkShiftPRIO               7
#define BlkMaskPRIO                1

/*
**  Block types
*/
#define BtHTBLK                    0x1 // Block type
#define BtHTMSG                    0x2 // Message type
#define BtHTBACK                   0x3 // Back type
#define BtHTCMD                    0x4 // Command type
#define BtHTBREAK                  0x5 // Break type
#define BtHTQBLK                   0x6 // Qualified block type
#define BtHTQMSG                   0x7 // Qualified message type
#define BtHTRESET                  0x8 // Reset type
#define BtHTRINIT                  0x9 // Request initialize type
#define BtHTNINIT                  0xA // Initialize response type
#define BtHTTERM                   0xB // Terminate type
#define BtHTICMD                   0xC // Interrupt command
#define BtHTICMR                   0xD // Interrupt command response

/*
**  Secondary function flag bits.
*/
#define SfcReq                     (0 << 6) // Request
#define SfcResp                    (1 << 6) // Normal response
#define SfcErr                     (2 << 6) // Abnormal response

/*
**  Primary/secondary function codes for terminal commands.
*/
#define PfcCTRL                    0xC1 // terminal characteristics
#define   SfcDEF                   0x04 // define characteristics
#define   SfcCHAR                  0x08 // define multiple char.
#define   SfcRTC                   0x09 // request terminal istics
#define   SfcTCD                   0x0A // term istics definition

#define PfcBD                      0xC2 // batch device characteristics
#define   SfcCHG                   0x00 // change

#define PfcBF                      0xC3 // batch file characteristics
//#define SfcCHG     0x00       // change

#define PfcTO                      0xC4 // terminate output marker
#define   SfcMARK                  0x00 // marker

#define PfcSI                      0xC5 // start input
#define   SfcNONTR                 0x01 // non-transparent
#define   SfcRSM                   0x02 // resume
#define   SfcTRAN                  0x03 // transparent

#define PfcAI                      0xC6 // abort input
#define   SfcTERM                  0x00 // terminal

#define PfcIS                      0xC7 // input stream
#define   SfcBI                    0x01 // batch interrupt
#define   SfcSC                    0x02 // slipped card
#define   SfcES                    0x03 // end of stream
#define   SfcNR                    0x04 // not ready

#define PfcOS                      0xC8 // output stopped
//#define SfcBI      0x01       // batch interrupt
#define   SfcPM                    0x02 // printer message
#define   SfcFLF                   0x03 // file limit
//#define SfcNR      0x04       // not ready

#define PfcAD                      0xC9 // accounting data
#define   SfcEOI                   0x01 // EOI processed
#define   SfcIOT                   0x02 // I/O terminated
#define   SfcTF                    0x03 // terminal failure

#define PfcBI                      0xCA // break indication marker
//#define SfcMARK    0x00       // marker

#define PfcRO                      0xCB // resume output marker
//#define SfcMARK    0x00       // marker

#define PfcFT                      0xCC // A - A using PRU type data
#define   SfcON                    0x00 // turn on PRU mode
#define   SfcOFF                   0x01 // turn off PRU mode

/*
**  TIP types
*/
#define TtASYNC                    1  // ASYNC TIP
#define TtMODE4                    2  // MODE 4 TIP
#define TtHASP                     3  // HASP TIP
#define TtX25                      4  // X.25
#define TtBSC                      5  // BSC
#define TtTT12                     12 // Site-defined TIP 12 (used by TLF for reverse HASP)
#define TtTT13                     13 // Site-defined TIP 13 (used by NJF for NJE)
#define TtTT14                     14 // Site-defined TIP 14
#define Tt3270                     15 // 3270

/*
**  SubTIP types
*/
#define StM4A                      1 // M4A
#define StM4C                      2 // M4C
#define StN2741                    1 // N2741
#define St2741                     2 // 2741
#define StPOST                     1 // POST
#define StPRE                      2 // PRE
#define StPAD                      1 // PAD
#define StUSER                     6 // USER
#define StXAA                      3 // XAA
#define St2780                     1 // 2780
#define St3780                     2 // 3780

/*
**  Device types
*/
#define DtCONSOLE                  0 // Normal terminal or console
#define DtCR                       1 // Card reader
#define DtLP                       2 // Line printer
#define DtCP                       3 // Card punch
#define DtPLOTTER                  4 // Plotter

/*
**  Line speed codes
*/
#define LsNA                       0  //  N/A (i.e. synchronous)
#define Ls110                      1  //  110
#define Ls134                      2  //  134.5
#define Ls150                      3  //  150
#define Ls300                      4  //  300
#define Ls600                      5  //  600
#define Ls1200                     6  //  1200
#define Ls2400                     7  //  2400
#define Ls4800                     8  //  4800
#define Ls9600                     9  //  9600
#define Ls19200                    10 //  19200
#define Ls38400                    11 //  38400

/*
**  Line type codes
*/
#define LtS1                       1  // S1
#define LtS2                       2  // S2
#define LtS3                       3  // S3
#define LtS4                       11 // S4
#define LtA1                       6  // A1
#define LtA2                       7  // A2
#define LtA6                       9  // A6
#define LtH1                       10 // H1
#define LtH2                       12 // H2

/*
**  Code set codes
*/
#define CsBCD                      1 //  BCD - MODE 4A BCD
#define CsASCII                    2 //  ASCII - ASCII for ASYNC or MODE 4A ASCII
#define CsMODE4C                   3 //  MODE 4C
#define CsTYPPAPL                  3 //  Typewriter-paired APL-ASCII
#define CsBITPAPL                  4 //  Bit-paired APL-ASCII
#define CsEBCDAPL                  5 //  EBCD
#define CsEAPLAPL                  6 //  EBCD APL
#define CsCORR                     7 //  Correspondence
#define CsCORAPL                   8 //  Correspondence APL
#define CsEBCDIC                   9 //  EBCDIC

/*
**  Terminal class
*/
#define TcNA                       0  //  N/A
#define TcM33                      1  //  ASYNC  - M33,M35,M37,M38
#define Tc713                      2  //         - CDC 713, 751-1, 752, 756
#define Tc721                      3  //         - CDC 721
#define Tc2741                     4  //         - IBM 2741
#define TcM40                      5  //         - M40
#define TcH2000                    6  //         - HAZELTINE 2000
#define TcX364                     7  //         - X3.64 type terminals (ANSI e.g. VT100)
#define TcT4014                    8  //         - TEXTRONIX 4014
#define TcHASP                     9  //  HASP   - POST
#define Tc200UT                    10 //  MODE 4 - 200UT, 214
#define Tc71430                    11 //         - 714-30
#define Tc711                      12 //         - 711-10
#define Tc714                      13 //         - 714
#define TcHPRE                     14 //  HASP   - PRE
#define Tc734                      15 //         - 734
#define Tc2780                     16 //  BSC 2780
#define Tc3780                     17 //  BSC 3780
#define Tc327E                     18 //  3270 EBCDIC
#define TcTCOUPLER                 19 //  Coupler
#define TcTCONSOLE                 20 //  Console
#define TcTHDLC                    21 //  HDLC LIP
#define TcTDIAG                    22 //  On-line diagnostics
#define TcSYNAUTO                  23 //  SYNC AUTO
#define TcUTC1                     28 //  User terminal class 1 (used by TLF for reverse HASP)
#define TcUTC2                     29 //  User terminal class 2 (used by NJF for NJE)
#define TcUTC3                     30 //  User terminal class 3
#define TcUTC4                     31 //  User terminal class 4

/*
**  IVT Data Block Clarifier.
*/
#define DbcNoCursorPos             0x10 // Cursor pos off next input
#define DbcNoFe                    0x08 // Disable Format Effectors - display FE byte as data
#define DbcTransparent             0x04 // Transparent data
#define DbcEchoplex                0x02 // Echoplex off next input
#define DbcCancel                  0x02 // Cancel upline message

/*
**  PRU Data BlockClarifier
*/
#define DbcPRU                     0x80 // PRU data
#define DbcEOI                     0x40 // EOI flag
#define DbcEOR                     0x20 // EOR flag
#define DbcAcctg                   0x60 // Accounting record indication
#define Dbc8Bit                    0x10 // 8-bit data flag
#define DbcLvlMask                 0x0f // Level number mask (bottom 4 bits of DBC are level number)

/*
**  NPU connection types.
*/
#define ConnTypeRaw                0
#define ConnTypePterm              1
#define ConnTypeRs232              2
#define ConnTypeTelnet             3
#define ConnTypeHasp               4
#define ConnTypeRevHasp            5
#define ConnTypeNje                6
#define ConnTypeTrunk              7

/*
**  npuNetRegisterConnType() return codes
*/
#define NpuNetRegOk                0
#define NpuNetRegOvfl              1
#define NpuNetRegDupTcp            2
#define NpuNetRegDupCla            3
#define NpuNetRegNoMem             4

/*
**  Miscellaneous constants.
*/
#define ConnectionRetryInterval    30
#define DefaultHaspBlockSize       640
#define DefaultNjeBlockSize        8192
#define DefaultNjePingInterval     600
#define DefaultRevHaspBlockSize    640
#define HostIdSize                 9
#define MaxBuffer                  2048
#define MaxHaspStreams             7
#define MaxTcbs                    256
#define MaxTermDefs                64
#define MinNjeBlockSize            1024

/*
**  Character definitions.
*/
#define ChrNUL                     0x00 // Null
#define ChrSTX                     0x02 // Start text
#define ChrEOT                     0x04 // End of transmission
#define ChrBEL                     0x07 // Bell
#define ChrBS                      0x08 // Backspace
#define ChrTAB                     0x09 // Tab
#define ChrLF                      0x0A // Line feed
#define ChrFF                      0x0C // Form feed
#define ChrCR                      0x0D // Carriage return
#define ChrDC1                     0x11 // DC1 (X-ON)
#define ChrDC3                     0x13 // DC3 (X-OFF)
#define ChrESC                     0x1B // Escape
#define ChrUS                      0x1f // End of record marker
#define ChrDEL                     0x7F // Del

/*
**  NPU buffer flags
*/
#define NpuBufNeedsAck             0x01

/*
**  -------------------
**  NPU Macro Functions
**  -------------------
*/

/*
**  --------------------
**  NPU Type Definitions
**  --------------------
*/

/*
**  NPU buffers.
*/
typedef struct npuBuffer
    {
    struct npuBuffer *next;
    u16              offset;
    u16              numBytes;
    u8               blockSeqNo;
    u8               data[MaxBuffer];
    } NpuBuffer;

/*
**  NPU buffer queue.
*/
typedef struct npuQueue
    {
    struct npuBuffer *first;
    struct npuBuffer *last;
    } NpuQueue;

/*
**  Network connection control block.
*/

#define DefaultBlockSize    640
#define MaxBlockSize        2048
#define MinBlockSize        256

typedef enum
    {
    StConnInit = 0,
    StConnConnecting,
    StConnConnected,
    StConnBusy
    } ConnectionState;

typedef struct ncb
    {
    ConnectionState    state;                  // state of connection
    u8                 connType;               // connection type
    u16                tcpPort;                // TCP port on which to listen or connect
    u8                 claPort;                // first CLA port number
    int                numPorts;               // number of CLA ports
    char               *hostName;              // name of remote host to which to connect
    struct sockaddr_in hostAddr;               // network address of remote host
    time_t             connectionDeadline;     // deadline for successful connection
    time_t             nextConnectionAttempt;  // time at which to attempt to connect
#if defined(_WIN32)
    SOCKET             connFd;                 // descriptor for connection socket
    SOCKET             lstnFd;                 // descriptor for listen socket
#else
    int                connFd;
    int                lstnFd;
#endif
    } Ncb;

/*
**  Async TIP control block
*/
typedef enum
    {
    StTelnetData = 0,
    StTelnetProtoElem,
    StTelnetCR,
    StTelnetDont,
    StTelnetDo,
    StTelnetWont,
    StTelnetWill
    } TelnetState;

typedef enum
    {
    TermRecoNonAuto = 0,
    TermRecoAuto,
    TermRecoXauto
    } TermRecoType;

typedef struct acb
    {
    TelnetState  state;
    TermRecoType recoType;
    u32          pendingWills;
    struct tcb   *tp;
    } Acb;

/*
**  HASP TIP control block
*/
typedef enum
    {
    StHaspMajorInit = 0,
    StHaspMajorRecvData,
    StHaspMajorSendData,
    StHaspMajorSendENQ,
    StHaspMajorWaitENQ,
    StHaspMajorWaitSignon,
    StHaspMajorSendSignon
    } HaspMajorState;

typedef enum
    {
    StHaspMinorNIL = 0,
    StHaspMinorRecvBOF,
    StHaspMinorRecvSTX,
    StHaspMinorRecvENQ_Resp,
    StHaspMinorRecvACK0,
    StHaspMinorRecvSOH,
    StHaspMinorRecvENQ,
    StHaspMinorRecvBCB,
    StHaspMinorRecvFCS1,
    StHaspMinorRecvFCS2,
    StHaspMinorRecvRCB,
    StHaspMinorRecvSRCB,
    StHaspMinorRecvSCB0,
    StHaspMinorRecvSCB,
    StHaspMinorRecvSCB_EOF,
    StHaspMinorRecvStr,
    StHaspMinorRecvRC,
    StHaspMinorRecvSignon,
    StHaspMinorRecvDLE_Signon,
    StHaspMinorRecvDLE1,
    StHaspMinorRecvETB1,
    StHaspMinorRecvDLE2,
    StHaspMinorRecvETB2
    } HaspMinorState;

typedef enum
    {
    StHaspStreamInit = 0,
    StHaspStreamSendRTI,
    StHaspStreamWaitPTI,
    StHaspStreamReady,
    StHaspStreamWaitAcctng
    } HaspStreamState;

typedef struct batchParams
    {
    /*
    **  Device parameters
    */
    u8  fvDevPW;          // Page width
    u8  fvDevPL;          // Page length
    u16 fvDevTBS;         // TBS upper 3 bits
    u8  fvDevPrintTrain;  // Print train type

    /*
    **  File parameters
    */
    u8  fvFileType;       // file type (DO26,DO29,ASC,TRANS6,TRANS8)
    u8  fvFileCC;         // carriage control (RST,SUP)
    u8  fvFileLace;       // lace card punching (SUP,RST)
    u8  fvFileLimit;      // file limit upper
    u16 fvFilePunchLimit; // punch limit (SUP,RST)
    } BatchParams;

typedef struct scb
    {
    HaspStreamState state;
    struct tcb      *tp;
    BatchParams     params;
    u32             recordCount;
    bool            isDiscardingRecords;
    bool            isStarted;
    bool            isWaitingPTI;
    bool            isPruFragmentComplete;
    int             pruFragmentSize;
    u8              *pruFragment;
    } Scb;

typedef struct hcb
    {
    HaspMajorState majorState;
    HaspMinorState minorState;
    u64            lastRecvTime;
    u64            recvDeadline;
    u64            sendDeadline;
    bool           isSignedOn;
    bool           pauseAllOutput;
    u64            pauseDeadline;
    u8             lastRecvFrameType;
    u8             retries;
    u8             downlineBSN;
    u8             uplineBSN;
    u8             fcsMask;
    u8             sRCBType;
    u8             sRCBParam;
    u8             strLength;
    int            blockSize;
    NpuBuffer      *lastBlockSent;
    NpuBuffer      *outBuf;
    u8             pollIndex;
    Scb            *currentOutputStream;
    Scb            *designatedStream;
    Scb            consoleStream;
    Scb            readerStreams[MaxHaspStreams];
    Scb            printStreams[MaxHaspStreams];
    Scb            punchStreams[MaxHaspStreams];
    } Hcb;

/*
**  NJE TIP control block
*/
typedef enum
    {
    StNjeDisconnected = 0,
    StNjeRcvOpen,
    StNjeRcvSOH_ENQ,
    StNjeSndOpen,
    StNjeRcvAck,
    StNjeRcvSignon,
    StNjeRcvResponseSignon,
    StNjeExchangeData
    } NJEConnState;

typedef struct njecb
    {
    NJEConnState state;
    struct tcb   *tp;
    u32          localIP;          // local  NJE node IPv4 address
    u32          remoteIP;         // remote NJE node IPv4 address
    bool         isPassive;        // TRUE if connection established as passive
    u8           downlineBSN;      // downline NJE block sequence number
    u8           uplineBSN;        // upline   NJE block sequence number
    u8           uplineBlockLimit; // max unacknowledged blocks queueable upline
    int          blockSize;        // NJE/TCP block sizw
    int          maxRecordSize;    // maximum NJE/TCP record size
    u8           lastDownlineRCB;  // last downline RCB processed
    u8           lastDownlineSRCB; // last downline SRCB processed
    int          retries;          // count of upline block retransmission attempts
    time_t       lastXmit;         // timestamp of last data transmission to peer
    int          pingInterval;     // interval in seconds between pings during idle periods
    u8           *inputBuf;        // NJE/TCP block input buffer
    u8           *inputBufPtr;     // pointer to next storage location
    u8           *outputBuf;       // NJE/TCP block output buffer
    u8           *outputBufPtr;    // pointer to next storage location
    u8           *ttrp;            // pointer to last TTR in output buffer
    NpuQueue     uplineQ;
    } NJEcb;

/*
**  LIP control block
*/
typedef enum
    {
    StTrunkDisconnected,
    StTrunkRcvConnReq,
    StTrunkRcvConnResp,
    StTrunkSndConnReq,
    StTrunkRcvBlockLengthHi,
    StTrunkRcvBlockLengthLo,
    StTrunkRcvBlockContent
    } LipState;

typedef struct lcb
    {
    LipState state;
    time_t   lastExchange;   // time of last data exchange
    u8       remoteNode;     // remote coupler node number, from this host's perspective
    u16      blockLength;    // length of LIP network protocol block
    int      inputIndex;     // index of next byte in PCB inputData buffer
    u8       *stagingBuf;    // protocol input staging buffer
    u8       *stagingBufPtr; // pointer to next storage location
    NpuQueue outputQ;
    } Lcb;

/*
**  CLA Port Controls
*/
typedef union portControls
    {
    Acb   async;
    Hcb   hasp;
    Lcb   lip;
    NJEcb nje;
    } PortControls;

/*
**  CLA port control block.
*/
typedef struct pcb
    {
    u8           claPort;                 // CLA port number
    Ncb          *ncbp;                   // pointer to network connection control block
    u8           *inputData;              // buffer for data received from network
    int          inputCount;              // number of bytes in buffer
    PortControls controls;                // TIP-dependent controls
#if defined(_WIN32)
    SOCKET       connFd;                  // connected socket descriptor
#else
    int          connFd;
#endif
    } Pcb;

/*
**  TIP parameters.
*/
typedef struct tipParams
    {
    u8   fvAbortBlock;                    // Abort block character
    u8   fvBlockFactor;                   // Blocking factor; multiple of 100 chars (upline block)
    bool fvBreakAsUser;                   // Break as user break 1; yes (1), no (0)
    u8   fvBS;                            // Backspace character
    u8   fvUserBreak1;                    // User break 1 character
    u8   fvUserBreak2;                    // User break 2 character
    bool fvEnaXUserBreak;                 // Enable transparent user break commands; yes (1), no (0)
    u8   fvCI;                            // Carriage return idle count
    bool fvCIAuto;                        // Carriage return idle count - TIP calculates suitable number
    u8   fvCN;                            // Cancel character
    bool fvCursorPos;                     // Cursor positioning; yes (1), no (0)
    u8   fvCT;                            // Network control character
    bool fvXCharFlag;                     // Transparent input delimiter character specified flag; yes (1), no (0)
    u16  fvXCnt;                          // Transparent input delimiter count
    u8   fvXChar;                         // Transparent input delimiter character
    bool fvXTimeout;                      // Transparent input delimiter timeout; yes (1), no (0)
    bool fvXModeMultiple;                 // Transparent input mode; multiple (1), singe (0)
    u8   fvEOB;                           // End of block character
    u8   fvEOBterm;                       // End of block terminator; EOL (1), EOB (2)
    u8   fvEOBCursorPos;                  // EOB cursor pos; no (0), CR (1), LF (2), CR & LF (3)
    u8   fvEOL;                           // End of line character
    u8   fvEOLTerm;                       // End of line terminator; EOL (1), EOB (2)
    u8   fvEOLCursorPos;                  // EOL cursor pos; no (0), CR (1), LF (2), CR & LF (3)
    bool fvEchoplex;                      // Echoplex mode
    bool fvFullASCII;                     // Full ASCII input; yes (1), no (0)
    bool fvInFlowControl;                 // Input flow control; yes (1), no (0)
    bool fvXInput;                        // Transparent input; yes (1), no (0)
    u8   fvInputDevice;                   // Keyboard (0), paper tape (1), block mode (2)
    u8   fvLI;                            // Line feed idle count
    bool fvLIAuto;                        // Line feed idle count - TIP calculates suitable number
    bool fvLockKeyboard;                  // Lockout unsolicited input from keyboard
    bool fvOutFlowControl;                // Output flow control; yes (1), no (0)
    u8   fvOutputDevice;                  // Printer (0), display (1), paper tape (2)
    u8   fvParity;                        // Zero (0), odd (1), even (2), none (3), ignore (4)
    bool fvPG;                            // Page waiting; yes (1), no (0)
    u8   fvPL;                            // Page length in lines; 0, 8 - FF
    u8   fvPW;                            // Page width in columns; 0, 20 - FF
    bool fvSpecialEdit;                   // Special editing mode; yes (1), no (0)
    u8   fvTC;                            // Terminal class
    bool fvXStickyTimeout;                // Sticky transparent input forward on timeout; yes (1), no (0)
    u8   fvXModeDelimiter;                // Transparent input mode delimiter character
    bool fvDuplex;                        // full (1), half (0)
    bool fvSolicitInput;                  // yes (1), no (0)
    u8   fvCIDelay;                       // Carriage return idle delay in 4 ms increments
    u8   fvLIDelay;                       // Line feed idle delay in 4 ms increments
    u8   fvHostNode;                      // Selected host node
    bool fvAutoConnect;                   // yes (1), no (0)
    u8   fvPriority;                      // Terminal priority
    u8   fvUBL;                           // Upline block count limit
    u16  fvUBZ;                           // Uplineblock size
    u8   fvABL;                           // Application block count limit
    u8   fvDBL;                           // Downline block count limit
    u16  fvDBZ;                           // Downline block size
    u8   fvRIC;                           // Restriced interactive console (RBF)
    u8   fvSDT;                           // Subdevice type
    u8   fvDO;                            // Device ordinal
    } TipParams;

/*
**  Terminal connection state.
*/
typedef enum
    {
    StTermIdle = 0,            // not configured or connected
    StTermRequestConnection,   // connection request sent
    StTermHostConnected,       // configured and connected
    StTermRequestDisconnect,   // disconnect request sent
    StTermRequestTerminate     // connection terminate block sent
    } TermConnState;

/*
**  Terminal control block.
*/
typedef struct tcb
    {
    /*
    **  Connection state.
    */
    TermConnState state;
    u8            cn;
    Pcb           *pcbp;
    Scb           *scbp;
    bool          active;
    bool          hostDisconnect;
    bool          breakPending;

    /*
    **  Configuration.
    */
    struct tcb    *owningConsole;
    bool          enabled;
    char          termName[7];
    u8            tipType;
    u8            subTip;
    u8            deviceType;
    u8            streamId;
    u8            codeSet;

    /*
    **  Active TIP parameters.
    */
    TipParams     params;

    /*
    **  Input state.
    */
    u8            uplineBsn;
    u8            inBuf[MaxBuffer];
    u8            *inBufPtr;
    u8            *inBufStart;

    bool          xInputTimerRunning;
    u32           xStartCycle;

    /*
    **  Output state.
    */
    NpuQueue      outputQ;
    bool          xoff;
    bool          dbcNoEchoplex;
    bool          dbcNoCursorPos;
    bool          lastOpWasInput;
    } Tcb;

/*
**  -------------------------------
**  NPU and MDI Function Prototypes
**  -------------------------------
*/

/*
**  cdcnet.c
*/
void cdcnetCheckStatus();
void cdcnetProcessDownlineData(NpuBuffer *bp);
void cdcnetReset();

/*
**  npu_hip.c
*/
bool npuHipUplineBlock(NpuBuffer *bp);
bool npuHipDownlineBlock(NpuBuffer *bp);
void npuLogMessage(char *format, ...);

/*
**  npu_bip.c
*/
void npuBipInit(void);
void npuBipReset(void);
NpuBuffer *npuBipBufGet(void);
void npuBipBufRelease(NpuBuffer *bp);
void npuBipQueueAppend(NpuBuffer *bp, NpuQueue *queue);
void npuBipQueuePrepend(NpuBuffer *bp, NpuQueue *queue);
NpuBuffer *npuBipQueueExtract(NpuQueue *queue);
NpuBuffer *npuBipQueueGetLast(NpuQueue *queue);
bool npuBipQueueNotEmpty(NpuQueue *queue);
void npuBipNotifyServiceMessage(void);
void npuBipNotifyData(int priority);
void npuBipRetryInput(void);
void npuBipNotifyDownlineReceived(void);
void npuBipAbortDownlineReceived(void);
void npuBipRequestUplineTransfer(NpuBuffer *bp);
void npuBipRequestUplineCanned(u8 *msg, int msgSize);
void npuBipNotifyUplineSent(void);

/*
**  npu_svm.c
*/
void npuSvmInit(void);
void npuSvmReset(void);
void npuSvmNotifyHostRegulation(u8 regLevel);
void npuSvmProcessBuffer(NpuBuffer *bp);
bool npuSvmConnectTerminal(Pcb *pp);
void npuSvmDiscRequestTerminal(Tcb *tp);
void npuSvmDiscReplyTerminal(Tcb *tp);
bool npuSvmIsReady(void);
void npuSvmSendDiscRequest(Tcb *tp);
void npuSvmSendTermBlock(Tcb *tp);

/*
**  npu_tip.c
*/
void npuTipDiscardOutputQ(Tcb *tp);
Tcb *npuTipFindFreeTcb(void);
Tcb *npuTipFindTcbForCN(u8 cn);
void npuTipInit(void);
void npuTipInputReset(Tcb *tp);
void npuTipNotifySent(Tcb *tp, u8 blockSeqNo);
bool npuTipParseFnFv(u8 *mp, int len, Tcb *tp);
void npuTipProcessBuffer(NpuBuffer *bp, int priority);
void npuTipReset(void);
void npuTipSetupTerminalClass(Tcb *tp, u8 tc);
void npuTipSendUserBreak(Tcb *tp, u8 bt);

/*
**  npu_net.c
*/
void npuNetCloseConnection(Pcb *pcbp);
Pcb *npuNetFindPcb(int portNumber);
void npuNetInit(bool startup);
void npuNetPreset(void);
void npuNetReset(void);
void npuNetConnected(Tcb *tp);
void npuNetDisconnected(Tcb *tp);
int npuNetRegisterConnType(int tcpPort, int claPort, int numPorts, int connType, Ncb **ncbpp);
void npuNetSend(Tcb *tp, u8 *data, int len);
void npuNetSetMaxCN(u8 cn);
void npuNetQueueAck(Tcb *tp, u8 blockSeqNo);
void npuNetQueueOutput(Tcb *tp, u8 *data, int len);
void npuNetCheckStatus(void);

/*
**  npu_async.c
*/
void npuAsyncFlushUplineTransparent(Tcb *tp);
bool npuAsyncNotifyNetConnect(Pcb *pcbp, bool isPassive);
void npuAsyncNotifyNetDisconnect(Pcb *pcbp);
void npuAsyncNotifyTermConnect(Tcb *tp);
void npuAsyncNotifyTermDisconnect(Tcb *tp);
void npuAsyncPresetPcb(Pcb *pcbp);
void npuAsyncProcessBreakIndication(Tcb *tp);
void npuAsyncProcessDownlineData(Tcb *tp, NpuBuffer *bp, bool last);
void npuAsyncProcessTelnetData(Pcb *pcbp);
void npuAsyncProcessUplineData(Pcb *pcbp);
void npuAsyncPtermNetSend(Tcb *tp, u8 *data, int len);
void npuAsyncResetPcb(Pcb *pcbp);
void npuAsyncTelnetNetSend(Tcb *tp, u8 *data, int len);
void npuAsyncTryOutput(Pcb *pcbp);

/*
**  npu_hasp.c
*/
void npuHaspCloseStream(Tcb *tp);
bool npuHaspParseDevParams(u8 *mp, int len, Tcb *tp);
bool npuHaspParseFileParams(u8 *mp, int len, Tcb *tp);
bool npuHaspNotifyNetConnect(Pcb *pcbp, bool isPassive);
void npuHaspNotifyNetDisconnect(Pcb *pcbp);
void npuHaspNotifyStartInput(Tcb *tp, u8 sfc);
void npuHaspNotifyTermConnect(Tcb *tp);
void npuHaspNotifyTermDisconnect(Tcb *tp);
void npuHaspPresetPcb(Pcb *pcbp);
void npuHaspProcessDownlineData(Tcb *tp, NpuBuffer *bp, bool last);
void npuHaspProcessUplineData(Pcb *pcbp);
void npuHaspResetPcb(Pcb *pcbp);
void npuHaspTryOutput(Pcb *pcbp);

/*
**  npu_nje.c
*/
void npuNjeNotifyAck(Tcb *tp, u8 bsn);
bool npuNjeNotifyNetConnect(Pcb *pcbp, bool isPassive);
void npuNjeNotifyNetDisconnect(Pcb *pcbp);
void npuNjeNotifyTermConnect(Tcb *tp);
void npuNjeNotifyTermDisconnect(Tcb *tp);
void npuNjePresetPcb(Pcb *pcbp);
void npuNjeProcessDownlineData(Tcb *tp, NpuBuffer *bp, bool last);
void npuNjeProcessUplineData(Pcb *pcbp);
void npuNjeResetPcb(Pcb *pcbp);
void npuNjeTryOutput(Pcb *pcbp);

/*
**  npu_lip.c
*/
bool npuLipNotifyNetConnect(Pcb *pcbp, bool isPassive);
void npuLipNotifyNetDisconnect(Pcb *pcbp);
void npuLipPresetPcb(Pcb *pcbp);
void npuLipProcessDownlineData(NpuBuffer *bp);
void npuLipProcessUplineData(Pcb *pcbp);
void npuLipReset(void);
void npuLipResetPcb(Pcb *pcbp);
void npuLipTryOutput(Pcb *pcbp);

/*
**  ----------------------------
**  Global NPU and MDI variables
**  ----------------------------
*/
extern u8  cdcnetNode;
extern u16 cdcnetPrivilegedTcpPortOffset;
extern u16 cdcnetPrivilegedUdpPortOffset;
extern u8  npuLipTrunkCount;
extern u8  npuNetMaxClaPort;
extern u8  npuNetMaxCN;
extern Tcb npuTcbs[];

#endif /* NPU_H */
/*---------------------------  End Of File  ------------------------------*/
