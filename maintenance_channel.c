/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**
**  Name: maintenance_channel.c
**
**  Description:
**      Perform emulation of maintenance channel.
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

#define DEBUG 0

/*
**  -------------
**  Include Files
**  -------------
*/
#include <stdio.h>
#include <stdlib.h>
#ifndef WIN32
#include <unistd.h>
#endif
#include <string.h>
#include "const.h"
#include "types.h"
#include "proto.h"

/*

**        MAINTENANCE REGISTER EQUIVALENCES.                            COMSIOU    20
                                                                        NS2149      1
*         CONNECT CODES.                                                NS2149      2
                                                                        NS2149      3
 IOUC     EQU    0S8         IOU CONNECT CODE                           NS2149      4
 CMCC     EQU    1S8         MEMORY CONNECT CODE                        NS2149      5
 CPCC     EQU    2S8         PROCESSOR CONNECT CODE                     NS2149      6
                                                                        COMSIOU    22
*         FUNCTIONS.                                                    COMSIOU    23
                                                                        COMSIOU    24
 MRHP     EQU    0#00        HALT PROCESSOR                             COMSIOU    25
 MRSP     EQU    0#10        START PROCESSOR                            COMSIOU    26
 MRRD     EQU    0#40        READ REGISTER                              COMSIOU    27
 MRWT     EQU    0#50        WRITE REGISTER                             COMSIOU    28
 MRMC     EQU    0#60        MASTER CLEAR                               COMSIOU    29
 MRCE     EQU    0#70        CLEAR FAULT STATUS REGISTER                COMSIOU    30
 MREC     EQU    0#80        ECHO DATA                                  COMSIOU    31
 MRSS     EQU    0#C0        REQUEST SUMMARY STATUS                     COMSIOU    32
 MRDC     EQU    0#AE0       DEACTIVATE MAINTENANCE CHANNEL CONTROL     COMSIOU    33
                                                                        COMSIOU    34
*         MODEL INDEPENDENT REGISTER NUMBERS.                           COMSIOU    35
                                                                        COMSIOU    36
 SSMR     EQU    0#00        STATUS SUMMARY                             COMSIOU    37
 EIMR     EQU    0#10        ELEMENT ID                                 COMSIOU    38
 OIMR     EQU    0#12        OPTIONS INSTALLED                          COMSIOU    39
 ECMR     EQU    0#20        ENVIRONMENT CONTROL                        COMSIOU    40
 DEMR     EQU    0#30        DEPENDENT ENVIRONMENT CONTROL              COMSIOU    41
                                                                        COMSIOU    42
*         IOU REGISTERS.                                                COMSIOU    43
                                                                        COMSIOU    44
 IFSM     EQU    0#18        FAULT STATUS MASK                          COMSIOU    45
 IOSB     EQU    0#21        OS BOUNDS                                  COMSIOU    46
 ISTR     EQU    0#40        STATUS REGISTER                            COMSIOU    47
 IFS1     EQU    0#80        FAULT STATUS 1                             COMSIOU    48
 IFS2     EQU    0#81        FAULT STATUS 2                             COMSIOU    49
 ITMR     EQU    0#A0        TEST MODE REGISTER                         COMSIOU    50
 OICR     EQU    0#16        CIO OPTIONS INSTALLED                      242L642     1
 ECCR     EQU    0#34        CIO ENVIRONMENT CONTROL                    242L642     2
 SRCR     EQU    0#44        CIO STATUS REGISTER                        242L642     3
 F1CR     EQU    0#84        CIO FAULT STATUS 1                         242L642     4
 F2CR     EQU    0#85        CIO FAULT STATUS 2                         242L642     5
 TMCR     EQU    0#A4        CIO TEST MODE                              242L642     6
 FMCR     EQU    0#1C        CIO FAULT STATUS MASK                      242L642     7
 OBCR     EQU    0#25        CIO O S BOUNDS                             242L642     8
 C0CR     EQU    0#B0        CIO CHANNEL 0 STATUS                       242L642     9
 C1CR     EQU    0#B1        CIO CHANNEL 1 STATUS                       242L642    10
 C2CR     EQU    0#B2        CIO CHANNEL 2 STATUS                       242L642    11
 C3CR     EQU    0#B3        CIO CHANNEL 3 STATUS                       242L642    12
 C4CR     EQU    0#B4        CIO CHANNEL 4 STATUS                       242L642    13
 C5CR     EQU    0#B5        CIO CHANNEL 5 STATUS                       242L642    14
 C6CR     EQU    0#B6        CIO CHANNEL 6 STATUS                       242L642    15
 C7CR     EQU    0#B7        CIO CHANNEL 7 STATUS                       242L642    16
 C8CR     EQU    0#B8        CIO CHANNEL 10 STATUS                      242L642    17
 C9CR     EQU    0#B9        CIO CHANNEL 11 STATUS                      242L642    18
                                                                        COMSIOU    51
*         MEMORY REGISTERS.                                             COMSIOU    52
                                                                        COMSIOU    53
 MBRG     EQU    0#21        BOUNDS REGISTER                            COMSIOU    54
 MCEL     EQU    0#A0        CORRECTED ERROR LOG                        COMSIOU    55
 MUL1     EQU    0#A4        UNCORRECTED ERROR LOG 1                    COMSIOU    56
 MUL2     EQU    0#A8        UNCORRECTED ERROR LOG 2                    COMSIOU    57
 MFRC     EQU    0#B0        FREE RUNNING COUNTER                       COMSIOU    58
                                                                        COMSIOU    59
*         PROCESSOR REGISTERS.                                          COMSIOU    60
                                                                        COMSIOU    61
 PPID     EQU    0#11        PROCESSOR ID                               COMSIOU    62
 PVCM     EQU    0#13        VIRTUAL MACHINE CAPABILITY LIST            COMSIOU    63
 PMF1     EQU    0#21        PROCESSOR MONITORING FACILITY 1            COMSIOU    64
 PMF2     EQU    0#22        PROCESSOR MINITORING FACILITY 2            COMSIOU    65
 PCSA     EQU    0#31        CONTROL STORE ADDRESS                      COMSIOU    66
 PCSB     EQU    0#32        CONTROL STORE BREAKPOINT                   COMSIOU    67
 PPRG     EQU    0#40        PROGRAM ADDRESS REGISTER                   COMSIOU    68
 PMPS     EQU    0#41        MONITOR PROCESS STATE REGISTER             COMSIOU    69
 PMCR     EQU    0#42        MONITOR STATE CONTROL REGISTER             COMSIOU    70
 PUCR     EQU    0#43        USER STATE CONTROL REGISTER                COMSIOU    71
 PUPR     EQU    0#44        UNTRANSLATABLE POINTER                     COMSIOU    72
 PSTL     EQU    0#45        SEGMENT TABLE LENGTH                       COMSIOU    73
 PSTA     EQU    0#46        SEGMENT TABLE ADDRESS                      COMSIOU    74
 PBCR     EQU    0#47        BASE ADDRESS REGISTER                      COMSIOU    75
 PPTA     EQU    0#48        PAGE TABLE ADDRESS                         COMSIOU    76
 PPTL     EQU    0#49        PAGE TABLE LENGTH                          COMSIOU    77
 PPSM     EQU    0#4A        PAGE SIZE MASK                             COMSIOU    78
 PMDF     EQU    0#50        MODEL DEPENDENT FLAGS                      COMSIOU    79
 PMDW     EQU    0#51        MODEL DEPENDENT WORD                       COMSIOU    80
 PMMR     EQU    0#60        MONITOR MASK                               COMSIOU    81
 PJPS     EQU    0#61        JOB PROCESS STATE REGISTER                 COMSIOU    82
 PSIT     EQU    0#62        SYSTEM INTERVAL TIMER                      COMSIOU    83
 PPFS     EQU    0#80        PROCESSOR FAULT STATUS                     COMSIOU    84
 PCSP     EQU    0#81        CONTROL MEMORY PARITY                      COMSIOU    85
 PRCL     EQU    0#90        RETRY CORRECTED ERROR LOG                  NS2125      1
 PUCS     EQU    0#91        CONTROL STORE ERROR LOG                    V22L602     1
 PCCL     EQU    0#92        CACHE CORRECTED ERROR LOG                  COMSIOU    87
 PMCL     EQU    0#93        MAP CORRECTED ERROR LOG                    COMSIOU    88
 PPTM     EQU    0#A0        TEST MODE                                  COMSIOU    89
 PTPE     EQU    0#C0        TRAP ENABLES                               COMSIOU    90
 PTRP     EQU    0#C4        TRAP POINTER                               COMSIOU    91
 PDLP     EQU    0#C5        DEBUG LIST POINTER                         COMSIOU    92
 PKPM     EQU    0#C6        KEYPOINT HASH                              COMSIOU    93
 PKPC     EQU    0#C7        KEYPOINT CODE                              COMSIOU    94
 PKCN     EQU    0#C8        KEYPOINT CLASS NUMBER                      COMSIOU    95
 PPIT     EQU    0#C9        PROCESSOR INTERVAL TIMER                   COMSIOU    96
 PCCF     EQU    0#E0        CRITICAL FRAME FLAG                        COMSIOU    97
 POCF     EQU    0#E2        ON CONDITION FLAG                          COMSIOU    98
 PDBI     EQU    0#E4        DEBUG INDEX                                COMSIOU    99
 PDBM     EQU    0#E5        DEBUG MASK                                 COMSIOU   100
 PUSM     EQU    0#E6        USER MASK                                  COMSIOU   101
 PRDM     EQU    0#FF        REGISTER FILE DUMP ADDRESS                 COMSIOU   102
                                                                        COMSIOU   103
*         STATUS SUMMARY BITS.                                          COMSIOU   104
                                                                        COMSIOU   105
 SSLW     EQU    0           LONG WARNING (IOU, MEM, PROC)              COMSIOU   106
 SSCE     EQU    1           CORRECTED ERROR (MEM, PROC)                COMSIOU   107
 SSUE     EQU    2           UNCORRECTED ERROR (IOU, MEM, PROC)         COMSIOU   108
 SSPH     EQU    3           PROCESSOR HALT (IOU, PROC)                 COMSIOU   109
 SSSW     EQU    4           SHORT WARNING (PROCESSOR)                  COMSIOU   110
 SSSS     EQU    4           STATUS SUMMARY (IOU)                       COMSIOU   111
 SSMM     EQU    5           EXECUTIVE MONITOR MODE (PROCESSOR)         COMSIOU   112
                                                                        COMSIOU   113
                                                                        COMSIOU   114
*         PMF HARDWARE BYTE DEFINITIONS.                                COMSIOU   115
                                                                        COMSIOU   116
 PMSB     EQU    0           STATUS BYTE                                COMSIOU   117
 PMCB     EQU    1           CONTROL BYTE                               COMSIOU   118
 PMKC     EQU    2           KEYPOINT CONTROL                           COMSIOU   119
 PMCO     EQU    3           COUNTER OVERFLOW CONTROL                   COMSIOU   120
 PMIA     EQU    4           INSTRUCTION ARGUMENT                       COMSIOU   121
 PMIM     EQU    5           INSTRUCTION MASK                           COMSIOU   122
 PA0S     EQU    8D          SELECT A0 COUNTER                          COMSIOU   123
 PB0S     EQU    9D          SELECT B0 COUNTER                          COMSIOU   124
 PA1S     EQU    10D         SELECT A1 COUNTER                          COMSIOU   125
 PB1S     EQU    11D         SELECT B1 COUNTER                          COMSIOU   126
 PA2S     EQU    12D         SELECT A2 COUNTER                          COMSIOU   127
 PB2S     EQU    13D         SELECT B2 COUNTER                          COMSIOU   128
 PA3S     EQU    14D         SELECT A3 COUNTER                          COMSIOU   129
 PB3S     EQU    15D         SELECT B3 COUNTER                          COMSIOU   130
 PA0C     EQU    16D - 19D   A0 COUNTER                                 COMSIOU   131
 PB0C     EQU    20D - 23D   B0 COUNTER                                 COMSIOU   132
 PA1C     EQU    24D - 27D   A1 COUNTER                                 COMSIOU   133
 PB1C     EQU    28D - 31D   B1 COUNTER                                 COMSIOU   134
 PA2C     EQU    32D - 35D   A2 COUNTER                                 COMSIOU   135
 PB2C     EQU    36D - 39D   B2 COUNTER                                 COMSIOU   136
 PA3C     EQU    40D - 43D   A3 COUNTER                                 COMSIOU   137
 PB3C     EQU    44D - 47D   B3 COUNTER                                 COMSIOU   138
                                                                        COMSIOU   139
*/

/*
**  -----------------
**  Private Constants
**  -----------------
*/

/*
**  Connect codes.
*/
#define FcConnIou               0x000
#define FcConnMemory            0x100
#define FcConnCpu               0x200
#define FcConnIdleLow           0x800
#define FcConnIdleHigh          0xF00

#define FcConnMask              0xF00

/*
**  Function codes.
*/
#define FcOpHalt                0x000
#define FcOpStart               0x010
#define FcOpClearLed            0x030
#define FcOpRead                0x040
#define FcOpWrite               0x050
#define FcOpMasterClear         0x060
#define FcOpClearErrors         0x070
#define FcOpEchoData            0x080
#define FcOpRequSummaryStatus   0x0C0

#define FcOpMask                0x0F0

/*
**  Type bits.
*/
#define FcTypeMask              0x00F

/*
**  Register addresses.
*/
#define RegAddrSummaryStatus    0x00
#define RegAddrElementId        0x10
#define RegAddrOptionsInstalled 0x12
#define RegAddrFaultStatus      0x18
#define RegAddrOsBounds         0x21
#define RegAddrEnvControl       0x30
#define RegAddrStatus           0x40
#define RegAddrFaultStatus1     0x80
#define RegAddrFaultStatus2     0x81
#define RegAddrTestMode         0xA0

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

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static FcStatus mchFunc(PpWord funcCode);
static void mchIo(void);
static void mchActivate(void);
static void mchDisconnect(void);
static char *mchConn2String(PpWord connCode);
static char *mchOp2String(PpWord opCode);

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
static u8 mchLocation;
static bool mchAddressReady;
static u64 dataIou[256];
static u64 dataMem[256];
static u64 dataCpu[256];

#if DEBUG
static FILE *mchLog = NULL;
#endif

/*
**--------------------------------------------------------------------------
**
**  Public Functions
**
**--------------------------------------------------------------------------
*/
/*--------------------------------------------------------------------------
**  Purpose:        Initialise maintenance channel.
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
void mchInit(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName)
    {
    DevSlot *dp;

    (void)eqNo;
    (void)deviceName;

#if DEBUG
    if (mchLog == NULL)
        {
        mchLog = fopen("mchlog.txt", "wt");
        }
#endif

    dp = channelAttach(channelNo, eqNo, DtMch);
    dp->activate = mchActivate;
    dp->disconnect = mchDisconnect;
    dp->func = mchFunc;
    dp->io = mchIo;

#if 1
    /*
    **  Initialize registers (representing a 840A with 2 barrels with
    **  PP0 - PP11, channels 0 - 17, TPM and CC545, 16 MB of memory).
    */                             
    //                                   0011223244556677
    dataIou[RegAddrElementId       ] = 0x0000000002201234;
    dataMem[RegAddrElementId       ] = 0x0000000001311234;
    dataCpu[RegAddrElementId       ] = 0x0000000000341234;

    //                                   0011223244556677
    dataIou[RegAddrOptionsInstalled] = 0x000003FFFF00000F;
    dataMem[RegAddrOptionsInstalled] = 0x0010000000000000;
    dataCpu[RegAddrOptionsInstalled] = 0x0000000000000000;
#else
    /*
    **  Initialize registers (representing a 860 with 2 barrels with
    **  PP0 - PP11, channels 0 - 17, TPM and CC545, 16 MB of memory).
    */                             
    //                                   0011223244556677
    dataIou[RegAddrElementId       ] = 0x0000000002201234;
    dataMem[RegAddrElementId       ] = 0x0000000001311234;
    dataCpu[RegAddrElementId       ] = 0x0000000000321234;

    //                                   0011223244556677
    dataIou[RegAddrOptionsInstalled] = 0x000003FFAF000007;
    dataMem[RegAddrOptionsInstalled] = 0x0010000000000000;
    dataCpu[RegAddrOptionsInstalled] = 0x0000000000000000;
#endif

    /*
    **  Print a friendly message.
    */
    printf("Maintenance Channel initialised on channel %o\n", channelNo);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Execute function code on maintenance channel.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        FcStatus
**
**------------------------------------------------------------------------*/
static FcStatus mchFunc(PpWord funcCode)
    {
    PpWord connCode;
    PpWord opCode;
    PpWord typeCode;

    connCode = funcCode & FcConnMask;
    opCode   = funcCode & FcOpMask;
    typeCode = funcCode & FcTypeMask;

#if DEBUG
    fprintf(mchLog, "\n%06d PP:%02o CH:%02o f:0x%03X C:%-25s O:%-25s  >   ",
        traceSequenceNo,
        activePpu->id,
        activeDevice->channel->id,
        funcCode,
        mchConn2String(connCode),
        mchOp2String(opCode));
#endif

    /*
    **  Connect codes 0x800 - 0xF00 causes the MCH to be deselected.
    */
    if (connCode >= 0x800)
        {
        return(FcProcessed);
        }

    /*
    **  Process operation codes.
    */
    switch (opCode)
        {
    default:
#if DEBUG
        fprintf(mchLog, " Operation not implemented & declined!");
#endif
        return(FcDeclined);

    case FcOpHalt:
    case FcOpStart:
    case FcOpClearLed:
    case FcOpMasterClear:
    case FcOpClearErrors:
        /*
        **  Do nothing.
        */
        return(FcProcessed);

    case FcOpRead:
    case FcOpWrite:
    case FcOpEchoData:
        mchLocation = 0;
        mchAddressReady = FALSE;
        activeDevice->recordLength = 2;
        break;
        
    case FcOpRequSummaryStatus:
        activeDevice->recordLength = 1;
        break;
        }

    activeDevice->fcode = funcCode;
    return(FcAccepted);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Perform I/O on maintenance channel.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mchIo(void)
    {
    PpWord connCode;
    PpWord opCode;
    PpWord typeCode;
    u64 *dp;
    int shiftCount;

    connCode = activeDevice->fcode & FcConnMask;
    opCode   = activeDevice->fcode & FcOpMask;
    typeCode = activeDevice->fcode & FcTypeMask;

    switch (opCode)
        {
    default:
        break;

    case FcOpRead:
        if (!mchAddressReady)
            {
            if (activeChannel->full)
                {
#if DEBUG
                fprintf(mchLog, " a%02X", activeChannel->data);
#endif
                activeChannel->full = FALSE;
                activeDevice->recordLength -= 1;

                if (activeDevice->recordLength == 0)
                    {
                    /*
                    **  Ignore the MSB of the address.
                    */
                    mchLocation = (activeChannel->data & Mask8);
                    }
                }
            }
        else
            {
            if (!activeChannel->full)
                {
                switch (connCode)
                    {
                default:
                case FcConnIou:
                    dp = dataIou;
                    break;

                case FcConnMemory:
                    dp = dataMem;
                    break;

                case FcConnCpu:
                    dp = dataCpu;
                    break;
                    }

                switch (mchLocation)
                    {
                case RegAddrElementId:
                case RegAddrOptionsInstalled:
                    typeCode = 0;
                    break;

                default:
                    break;
                    }

                if (activeDevice->recordLength > 0)
                    {
                    activeDevice->recordLength -= 1;
                    shiftCount = ((activeDevice->recordLength + typeCode) % 8) * 8;

                    activeChannel->data = (PpWord)((dp[mchLocation] >> shiftCount) & Mask8);
                    }
                else
                    {
                    activeChannel->data = 0;
                    }
                activeChannel->full = TRUE;
#if DEBUG
                fprintf(mchLog, " i%02X", activeChannel->data);
#endif
                }
            }

        break;

    case FcOpWrite:
        if (!mchAddressReady)
            {
            if (activeChannel->full)
                {
#if DEBUG
                fprintf(mchLog, " a%02X", activeChannel->data);
#endif
                activeChannel->full = FALSE;
                activeDevice->recordLength -= 1;

                if (activeDevice->recordLength == 0)
                    {
                    /*
                    **  Ignore the MSB of the address.
                    */
                    mchLocation = (activeChannel->data & Mask8);
                    }
                }
            }
        else
            {
            if (activeChannel->full)
                {
                switch (connCode)
                    {
                default:
                case FcConnIou:
                    dp = dataIou;
                    break;

                case FcConnMemory:
                    dp = dataMem;
                    break;

                case FcConnCpu:
                    dp = dataCpu;
                    break;
                    }

                switch (mchLocation)
                    {
                case RegAddrElementId:
                case RegAddrOptionsInstalled:
                    typeCode = 0;
                    break;

                default:
                    break;
                    }

                if (activeDevice->recordLength > 0)
                    {
                    activeDevice->recordLength -= 1;
                    shiftCount = ((activeDevice->recordLength + typeCode) % 8) * 8;

                    dp[mchLocation] &= ~((u64)Mask8 << shiftCount);
                    dp[mchLocation] |= ((u64)activeChannel->data & Mask8) << shiftCount;
                    }

#if DEBUG
                fprintf(mchLog, " o%02X", activeChannel->data);
#endif
                
                if (   connCode == FcConnIou
                    && mchLocation == RegAddrEnvControl
                    && activeDevice->recordLength == 0
                    && ((dp[mchLocation] >> 8) & Mask8) == 0x10)
                    {
                    /*
                    **  Deadstart PP.
                    */
                    u8 pi = (u8)(dp[mchLocation] >> 24) & Mask5;
                    u8 ci = (u8)(dp[mchLocation] >> 16) & Mask5;

                    ppu[pi].opD = ci;
                    channel[ci].active = TRUE;

                    /*
                    **  Set PP to INPUT (71) instruction.
                    */
                    ppu[pi].opF = 071;
                    ppu[pi].busy = TRUE;

                    /*
                    **  Clear P register and location zero of PP.
                    */
                    ppu[pi].regP   = 0;
                    ppu[pi].mem[0] = 0;

                    /*
                    **  Set A registers to an input word count of 10000.
                    */
                    ppu[pi].regA = 010000;
                    }

                activeChannel->full = FALSE;
                }
            }

        break;

    case FcOpEchoData:
        if (!mchAddressReady)
            {
            if (activeChannel->full)
                {
#if DEBUG
                fprintf(mchLog, " a%02X", activeChannel->data);
#endif
                activeChannel->full = FALSE;
                activeDevice->recordLength -= 1;

                if (activeDevice->recordLength == 0)
                    {
                    /*
                    **  Ignore the MSB of the address.
                    */
                    mchLocation = (activeChannel->data & Mask8);
                    }
                }
            }
        else
            {
            if (!activeChannel->full)
                {
                activeChannel->data = (PpWord)mchLocation;
                activeChannel->full = TRUE;
#if DEBUG
                fprintf(mchLog, " e%02X", activeChannel->data);
#endif
                }
            }

        break;

    case FcOpRequSummaryStatus:
        if (!activeChannel->full)
            {
            activeChannel->data = 0;
            activeChannel->full = TRUE;
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
static void mchActivate(void)
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
static void mchDisconnect(void)
    {
    switch (activeDevice->fcode & FcOpMask)
        {
    case FcOpRead:
    case FcOpWrite:
        if (!mchAddressReady)
            {
            mchAddressReady = TRUE;
            activeDevice->recordLength = 8;
            }

        break;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Convert connect code to string.
**
**  Parameters:     Name        Description.
**                  connCode    operation code
**
**  Returns:        String equivalent of connect code.
**
**------------------------------------------------------------------------*/
static char *mchConn2String(PpWord connCode)
    {
    static char buf[30];
#if DEBUG
    switch(connCode)
        {
    case FcConnIou         : return "IOU";
    case FcConnMemory      : return "Memory";
    case FcConnCpu         : return "Processor";
        }
#endif
    sprintf(buf, "UNKNOWN: %03X", connCode);
    return(buf);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Convert operation code to string.
**
**  Parameters:     Name        Description.
**                  opCode      operation code
**
**  Returns:        String equivalent of operation code.
**
**------------------------------------------------------------------------*/
static char *mchOp2String(PpWord opCode)
    {
    static char buf[30];
#if DEBUG
    switch(opCode)
        {
    case FcOpHalt              : return "Halt";
    case FcOpStart             : return "Start";
    case FcOpClearLed          : return "ClearLed";
    case FcOpRead              : return "Read";
    case FcOpWrite             : return "Write";
    case FcOpMasterClear       : return "MasterClear";
    case FcOpClearErrors       : return "ClearErrors";
    case FcOpEchoData          : return "EchoData";
    case FcOpRequSummaryStatus : return "RequSummaryStatus";
        }
#endif
    sprintf(buf, "UNKNOWN: %03X", opCode);
    return(buf);
    }

/*---------------------------  End Of File  ------------------------------*/
