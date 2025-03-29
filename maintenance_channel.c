/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**                     2025, Kevin Jordan
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

#define DEBUG    1

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
#include "const.h"
#include "types.h"
#include "proto.h"

/*
 *
 **        MAINTENANCE REGISTER EQUIVALENCES.                            COMSIOU    20
 **                                                                      NS2149      1
 *         CONNECT CODES.                                                NS2149      2
 *                                                                       NS2149      3
 * IOUC     EQU    0S8         IOU CONNECT CODE                          NS2149      4
 * CMCC     EQU    1S8         MEMORY CONNECT CODE                       NS2149      5
 * CPCC     EQU    2S8         PROCESSOR CONNECT CODE                    NS2149      6
 *                                                                       COMSIOU    22
 *         FUNCTIONS.                                                    COMSIOU    23
 *                                                                       COMSIOU    24
 * MRHP     EQU    0#00        HALT PROCESSOR                            COMSIOU    25
 * MRSP     EQU    0#10        START PROCESSOR                           COMSIOU    26
 * MRRD     EQU    0#40        READ REGISTER                             COMSIOU    27
 * MRWT     EQU    0#50        WRITE REGISTER                            COMSIOU    28
 * MRMC     EQU    0#60        MASTER CLEAR                              COMSIOU    29
 * MRCE     EQU    0#70        CLEAR FAULT STATUS REGISTER               COMSIOU    30
 * MREC     EQU    0#80        ECHO DATA                                 COMSIOU    31
 * MRSS     EQU    0#C0        REQUEST SUMMARY STATUS                    COMSIOU    32
 * MRDC     EQU    0#AE0       DEACTIVATE MAINTENANCE CHANNEL CONTROL    COMSIOU    33
 *                                                                       COMSIOU    34
 *         MODEL INDEPENDENT REGISTER NUMBERS.                           COMSIOU    35
 *                                                                       COMSIOU    36
 * SSMR     EQU    0#00        STATUS SUMMARY                            COMSIOU    37
 * EIMR     EQU    0#10        ELEMENT ID                                COMSIOU    38
 * OIMR     EQU    0#12        OPTIONS INSTALLED                         COMSIOU    39
 * ECMR     EQU    0#20        ENVIRONMENT CONTROL                       COMSIOU    40
 * DEMR     EQU    0#30        DEPENDENT ENVIRONMENT CONTROL             COMSIOU    41
 *                                                                       COMSIOU    42
 *         IOU REGISTERS.                                                COMSIOU    43
 *                                                                       COMSIOU    44
 * IFSM     EQU    0#18        FAULT STATUS MASK                         COMSIOU    45
 * IOSB     EQU    0#21        OS BOUNDS                                 COMSIOU    46
 * ISTR     EQU    0#40        STATUS REGISTER                           COMSIOU    47
 * IFS1     EQU    0#80        FAULT STATUS 1                            COMSIOU    48
 * IFS2     EQU    0#81        FAULT STATUS 2                            COMSIOU    49
 * ITMR     EQU    0#A0        TEST MODE REGISTER                        COMSIOU    50
 * OICR     EQU    0#16        CIO OPTIONS INSTALLED                     242L642     1
 * ECCR     EQU    0#34        CIO ENVIRONMENT CONTROL                   242L642     2
 * SRCR     EQU    0#44        CIO STATUS REGISTER                       242L642     3
 * F1CR     EQU    0#84        CIO FAULT STATUS 1                        242L642     4
 * F2CR     EQU    0#85        CIO FAULT STATUS 2                        242L642     5
 * TMCR     EQU    0#A4        CIO TEST MODE                             242L642     6
 * FMCR     EQU    0#1C        CIO FAULT STATUS MASK                     242L642     7
 * OBCR     EQU    0#25        CIO O S BOUNDS                            242L642     8
 * C0CR     EQU    0#B0        CIO CHANNEL 0 STATUS                      242L642     9
 * C1CR     EQU    0#B1        CIO CHANNEL 1 STATUS                      242L642    10
 * C2CR     EQU    0#B2        CIO CHANNEL 2 STATUS                      242L642    11
 * C3CR     EQU    0#B3        CIO CHANNEL 3 STATUS                      242L642    12
 * C4CR     EQU    0#B4        CIO CHANNEL 4 STATUS                      242L642    13
 * C5CR     EQU    0#B5        CIO CHANNEL 5 STATUS                      242L642    14
 * C6CR     EQU    0#B6        CIO CHANNEL 6 STATUS                      242L642    15
 * C7CR     EQU    0#B7        CIO CHANNEL 7 STATUS                      242L642    16
 * C8CR     EQU    0#B8        CIO CHANNEL 10 STATUS                     242L642    17
 * C9CR     EQU    0#B9        CIO CHANNEL 11 STATUS                     242L642    18
 *                                                                       COMSIOU    51
 *         MEMORY REGISTERS.                                             COMSIOU    52
 *                                                                       COMSIOU    53
 * MBRG     EQU    0#21        BOUNDS REGISTER                           COMSIOU    54
 * MCEL     EQU    0#A0        CORRECTED ERROR LOG                       COMSIOU    55
 * MUL1     EQU    0#A4        UNCORRECTED ERROR LOG 1                   COMSIOU    56
 * MUL2     EQU    0#A8        UNCORRECTED ERROR LOG 2                   COMSIOU    57
 * MFRC     EQU    0#B0        FREE RUNNING COUNTER                      COMSIOU    58
 *                                                                       COMSIOU    59
 *         PROCESSOR REGISTERS.                                          COMSIOU    60
 *                                                                       COMSIOU    61
 * PPID     EQU    0#11        PROCESSOR ID                              COMSIOU    62
 * PVCM     EQU    0#13        VIRTUAL MACHINE CAPABILITY LIST           COMSIOU    63
 * PMF1     EQU    0#21        PROCESSOR MONITORING FACILITY 1           COMSIOU    64
 * PMF2     EQU    0#22        PROCESSOR MINITORING FACILITY 2           COMSIOU    65
 * PCSA     EQU    0#31        CONTROL STORE ADDRESS                     COMSIOU    66
 * PCSB     EQU    0#32        CONTROL STORE BREAKPOINT                  COMSIOU    67
 * PPRG     EQU    0#40        PROGRAM ADDRESS REGISTER                  COMSIOU    68
 * PMPS     EQU    0#41        MONITOR PROCESS STATE REGISTER            COMSIOU    69
 * PMCR     EQU    0#42        MONITOR STATE CONTROL REGISTER            COMSIOU    70
 * PUCR     EQU    0#43        USER STATE CONTROL REGISTER               COMSIOU    71
 * PUPR     EQU    0#44        UNTRANSLATABLE POINTER                    COMSIOU    72
 * PSTL     EQU    0#45        SEGMENT TABLE LENGTH                      COMSIOU    73
 * PSTA     EQU    0#46        SEGMENT TABLE ADDRESS                     COMSIOU    74
 * PBCR     EQU    0#47        BASE ADDRESS REGISTER                     COMSIOU    75
 * PPTA     EQU    0#48        PAGE TABLE ADDRESS                        COMSIOU    76
 * PPTL     EQU    0#49        PAGE TABLE LENGTH                         COMSIOU    77
 * PPSM     EQU    0#4A        PAGE SIZE MASK                            COMSIOU    78
 * PMDF     EQU    0#50        MODEL DEPENDENT FLAGS                     COMSIOU    79
 * PMDW     EQU    0#51        MODEL DEPENDENT WORD                      COMSIOU    80
 * PMMR     EQU    0#60        MONITOR MASK                              COMSIOU    81
 * PJPS     EQU    0#61        JOB PROCESS STATE REGISTER                COMSIOU    82
 * PSIT     EQU    0#62        SYSTEM INTERVAL TIMER                     COMSIOU    83
 * PPFS     EQU    0#80        PROCESSOR FAULT STATUS                    COMSIOU    84
 * PCSP     EQU    0#81        CONTROL MEMORY PARITY                     COMSIOU    85
 * PRCL     EQU    0#90        RETRY CORRECTED ERROR LOG                 NS2125      1
 * PUCS     EQU    0#91        CONTROL STORE ERROR LOG                   V22L602     1
 * PCCL     EQU    0#92        CACHE CORRECTED ERROR LOG                 COMSIOU    87
 * PMCL     EQU    0#93        MAP CORRECTED ERROR LOG                   COMSIOU    88
 * PPTM     EQU    0#A0        TEST MODE                                 COMSIOU    89
 * PTPE     EQU    0#C0        TRAP ENABLES                              COMSIOU    90
 * PTRP     EQU    0#C4        TRAP POINTER                              COMSIOU    91
 * PDLP     EQU    0#C5        DEBUG LIST POINTER                        COMSIOU    92
 * PKPM     EQU    0#C6        KEYPOINT HASH                             COMSIOU    93
 * PKPC     EQU    0#C7        KEYPOINT CODE                             COMSIOU    94
 * PKCN     EQU    0#C8        KEYPOINT CLASS NUMBER                     COMSIOU    95
 * PPIT     EQU    0#C9        PROCESSOR INTERVAL TIMER                  COMSIOU    96
 * PCCF     EQU    0#E0        CRITICAL FRAME FLAG                       COMSIOU    97
 * POCF     EQU    0#E2        ON CONDITION FLAG                         COMSIOU    98
 * PDBI     EQU    0#E4        DEBUG INDEX                               COMSIOU    99
 * PDBM     EQU    0#E5        DEBUG MASK                                COMSIOU   100
 * PUSM     EQU    0#E6        USER MASK                                 COMSIOU   101
 * PRDM     EQU    0#FF        REGISTER FILE DUMP ADDRESS                COMSIOU   102
 *                                                                       COMSIOU   103
 *         STATUS SUMMARY BITS.                                          COMSIOU   104
 *                                                                       COMSIOU   105
 * SSLW     EQU    0           LONG WARNING (IOU, MEM, PROC)             COMSIOU   106
 * SSCE     EQU    1           CORRECTED ERROR (MEM, PROC)               COMSIOU   107
 * SSUE     EQU    2           UNCORRECTED ERROR (IOU, MEM, PROC)        COMSIOU   108
 * SSPH     EQU    3           PROCESSOR HALT (IOU, PROC)                COMSIOU   109
 * SSSW     EQU    4           SHORT WARNING (PROCESSOR)                 COMSIOU   110
 * SSSS     EQU    4           STATUS SUMMARY (IOU)                      COMSIOU   111
 * SSMM     EQU    5           EXECUTIVE MONITOR MODE (PROCESSOR)        COMSIOU   112
 *                                                                       COMSIOU   113
 *                                                                       COMSIOU   114
 *         PMF HARDWARE BYTE DEFINITIONS.                                COMSIOU   115
 *                                                                       COMSIOU   116
 * PMSB     EQU    0           STATUS BYTE                               COMSIOU   117
 * PMCB     EQU    1           CONTROL BYTE                              COMSIOU   118
 * PMKC     EQU    2           KEYPOINT CONTROL                          COMSIOU   119
 * PMCO     EQU    3           COUNTER OVERFLOW CONTROL                  COMSIOU   120
 * PMIA     EQU    4           INSTRUCTION ARGUMENT                      COMSIOU   121
 * PMIM     EQU    5           INSTRUCTION MASK                          COMSIOU   122
 * PA0S     EQU    8D          SELECT A0 COUNTER                         COMSIOU   123
 * PB0S     EQU    9D          SELECT B0 COUNTER                         COMSIOU   124
 * PA1S     EQU    10D         SELECT A1 COUNTER                         COMSIOU   125
 * PB1S     EQU    11D         SELECT B1 COUNTER                         COMSIOU   126
 * PA2S     EQU    12D         SELECT A2 COUNTER                         COMSIOU   127
 * PB2S     EQU    13D         SELECT B2 COUNTER                         COMSIOU   128
 * PA3S     EQU    14D         SELECT A3 COUNTER                         COMSIOU   129
 * PB3S     EQU    15D         SELECT B3 COUNTER                         COMSIOU   130
 * PA0C     EQU    16D - 19D   A0 COUNTER                                COMSIOU   131
 * PB0C     EQU    20D - 23D   B0 COUNTER                                COMSIOU   132
 * PA1C     EQU    24D - 27D   A1 COUNTER                                COMSIOU   133
 * PB1C     EQU    28D - 31D   B1 COUNTER                                COMSIOU   134
 * PA2C     EQU    32D - 35D   A2 COUNTER                                COMSIOU   135
 * PB2C     EQU    36D - 39D   B2 COUNTER                                COMSIOU   136
 * PA3C     EQU    40D - 43D   A3 COUNTER                                COMSIOU   137
 * PB3C     EQU    44D - 47D   B3 COUNTER                                COMSIOU   138
 *                                                                       COMSIOU   139
 */

/*
**  -----------------
**  Private Constants
**  -----------------
*/

/*
**  Unit numbers in connect codes
*/
#define UnitIou                    0
#define UnitMem                    1
#define UnitCp0                    2
#define UnitCp1                    3

/*
**  Connect codes.
*/
#define FcConnIou                  0x00

#define FcConnMask                 0xF00

/*
**  Function codes.
*/
#define FcOpHalt                   0x00
#define FcOpStart                  0x01
#define FcOpClearLed               0x03
#define FcOpRead                   0x04
#define FcOpWrite                  0x05
#define FcOpMasterClear            0x06
#define FcOpClearErrors            0x07
#define FcOpEchoData               0x08
#define FcOpStatusSummary          0x0C

#define FcOpMask                   0x0F0

/*
**  Type bits.
*/
#define FcTypeMask                 0x00F

/*
**  IOU Register addresses.
*/
#define RegIouStatusSummary        0x00
#define RegIouElementId            0x10
#define RegIouOptionsInstalled     0x12
#define RegIouFaultStatus          0x18
#define RegIouOsBounds             0x21
#define RegIouEnvControl           0x30
#define RegIouStatus               0x40
#define RegIouFaultStatus1         0x80
#define RegIouFaultStatus2         0x81
#define RegIouTestMode             0xA0

/*
**  Memory Register addresses.
*/
#define RegMemStatusSummary        0x00
#define RegMemElementId            0x10
#define RegMemOptionsInstalled     0x12
#define RegMemEnvControl           0x20
#define RegMemCEL                  0xA0
#define RegMemCELd0                0xA0
#define RegMemCELd1                0xA1
#define RegMemDELd2                0xA2
#define RegMemCELd3                0xA3
#define RegMemUEL1                 0xA4
#define RegMemUEL1d0               0xA4
#define RegMemUEL1d1               0xA5
#define RegMemUEL1d2               0xA6
#define RegMemUEL1d3               0xA7
#define RegMemUEL2                 0xA8
#define RegMemUEL2d0               0xA8
#define RegMemUEL2d1               0xA9
#define RegMemUEL2d2               0xAA
#define RegMemUEL2d3               0xAB

/*
**  Processor Register addresses.
*/
#define RegProcStatusSummary       0x00
#define RegProcElementId           0x10
#define RegProcOptionsInstalled    0x12
#define RegProcDEC                 0x30
#define RegProcFaultStatus0        0x80
#define RegProcFaultStatus1        0x81
#define RegProcFaultStatus2        0x82
#define RegProcFaultStatus3        0x83
#define RegProcFaultStatus4        0x84
#define RegProcFaultStatus5        0x85
#define RegProcFaultStatus6        0x86
#define RegProcFaultStatus7        0x87
#define RegProcFaultStatus8        0x88
#define RegProcFaultStatus9        0x89
#define RegProcFaultStatusA        0x8A
#define RegProcFaultStatusB        0x8B
#define RegProcFaultStatusC        0x8C
#define RegProcFaultStatusD        0x8D
#define RegProcFaultStatusE        0x8E
#define RegProcFaultStatusF        0x8F
#define RegProcCCEL                0x92
#define RegProcMCEL                0x93
#define RegProcTestMode            0xA0
#define RegProcTestMode0           0xA0
#define RegProcTestMode1           0xA1
#define RegProcTestMode2           0xA2
#define RegProcTestMode3           0xA3

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
static char *mchCw2String(PpWord connCode, PpWord typeCode, PpWord location);
static char *mchFn2String(PpWord connCode, PpWord opCode, PpWord typeCode);
static FcStatus mchFunc(PpWord funcCode);
static u64 mchGetRegister(u8 connCode, u8 typeCode, u8 location);
static u8 *mchGetRegisterAddress(u8 connCode, u8 typeCode, u8 location, u16 *index, u16 *mask, u16 *limit, u8 **block, int *size);
static u8 *mchGetUnitRegisters(u8 connCode, u8 typeCode, int *size);
static void mchIo(void);
static void mchActivate(void);
static void mchDisconnect(void);
static bool mchIsConnected(PpWord connCode);
static char *mchOp2String(PpWord opCode);
static void mchPutWord(u8 *address, u64 word);
static void mchSetRegister(u8 connCode, u8 typeCode, u8 location, u64 word);

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
static u8   *iouRegisters = NULL;
static u16  mchIndex;
static u16  mchIndexMask;
static u16  mchIndexLimit;
static u8   mchLocation;
static bool mchLocationReady;
static u8   *mchRegBlockAddress;
static int  mchRegBlockSize;
static u8   *mchRegisterAddress;
static u8   *mchRegisters[16] =
    {
    NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,
    NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL
    };
static int  mchRegisterSizes[16];
static u64  mchTimeout = 0;

//
//  Bit masks identifying PP's in IOU OS Bounds and fault registers
//
static u32  mchPpMasks[20] =
    {
    0x01000000, // PP00
    0x02000000, // PP01
    0x04000000, // PP02
    0x08000000, // PP03
    0x10000000, // PP04
    0x00010000, // PP05
    0x00020000, // PP06
    0x00040000, // PP07
    0x00080000, // PP10
    0x00100000, // PP11
    0x00000100, // PP20
    0x00000200, // PP21
    0x00000400, // PP22
    0x00000800, // PP23
    0x00001000, // PP24
    0x00000001, // PP25
    0x00000002, // PP26
    0x00000004, // PP27
    0x00000008, // PP30
    0x00000010  // PP31
    };

#if DEBUG
static FILE *mchLog    = NULL;
static int  mchBytesIo = 0;
#endif

/*
 **--------------------------------------------------------------------------
 **
 **  Public Functions
 **
 **--------------------------------------------------------------------------
 */

/*--------------------------------------------------------------------------
**  Purpose:        Check whether a maintenance channel timeout has occurred.
**
**                  When a timeout occurs, the channel is set inactive and
**                  empty. Normally, a timeout is established only when a
**                  maintenance channel function has been declined, and this
**                  occurs only when a connection code provided in a function
**                  request is not supported by the machine.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void mchCheckTimeout()
    {
    if (mchTimeout != 0 && mchTimeout < getMilliseconds())
        {
        mchTimeout = 0;
        if (channel[ChMaintenance].full && channel[ChMaintenance].active && channel[ChMaintenance].ioDevice == NULL)
            {
            channel[ChMaintenance].full   = FALSE;
            channel[ChMaintenance].active = FALSE;
#if DEBUG
            fprintf(mchLog, "\n%12d PP:%02o CH:%02o Timeout",
                    traceSequenceNo,
                    activePpu->id,
                    activeDevice->channel->id);
            fflush(mchLog);
#endif
            }
        }
    }

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
    u8      *block;
    u64     channelMask;
    DevSlot *dp;
    u16     index;
    u64     iouOptionsInstalled;
    u16     limit;
    u16     mask;
    u64     memSizeMask;
    int     size;

#if DEBUG
    if (mchLog == NULL)
        {
        mchLog = fopen("mchlog.txt", "wt");
        }
#endif

    dp             = channelAttach(channelNo, eqNo, DtMch);
    dp->activate   = mchActivate;
    dp->disconnect = mchDisconnect;
    dp->func       = mchFunc;
    dp->io         = mchIo;

    channelMask = 0x000000FFAF000000; // channels 00 - 17
    if (channelCount > 16)
        {
        channelMask |= 0x0000000000FF0F00;
        }

    switch ((cpuMaxMemory * 8) / OneMegabyte)
        {
    case 1:
        memSizeMask = 0x8000;
        break;
    case 2:
        memSizeMask = 0x4000;
        break;
    case 3:
        memSizeMask = 0x2000;
        break;
    case 4:
        memSizeMask = 0x1000;
        break;
    case 5:
        memSizeMask = 0x0800;
        break;
    case 6:
        memSizeMask = 0x0400;
        break;
    case 7:
        memSizeMask = 0x0200;
        break;
    case 8:
        memSizeMask = 0x0100;
        break;
    case 10:
        memSizeMask = 0x0080;
        break;
    case 12:
        memSizeMask = 0x0040;
        break;
    case 14:
        memSizeMask = 0x0020;
        break;
    case 16:
        memSizeMask = 0x0010;
        break;
    case 2048:
        memSizeMask = 0x8008;
        break;
    case 1024:
        memSizeMask = 0x4008;
        break;
    case 512:
        memSizeMask = 0x2008;
        break;
    case 256:
        memSizeMask = 0x1008;
        break;
    case 128:
        memSizeMask = 0x0808;
        break;
    case 64:
        memSizeMask = 0x0408;
        break;
    case 32:
        memSizeMask = 0x0208;
        break;
    default:
        logDtError(LogErrorLocation, "Unsupported memory size: %ld", cpuMaxMemory * 8);
        exit(1);
        }
    memSizeMask <<= 48;

    iouOptionsInstalled  = channelMask;
    iouOptionsInstalled |= (u64)0x03 << 40;     // PP's 00 - 11
    if (ppuCount > 10)
        {
        iouOptionsInstalled |= (u64)0x0C << 40; // PP's 20 - 31
        }
    if (tpMuxEnabled)
        {
        iouOptionsInstalled |= 2;
        }
    if (cc545Enabled)
        {
        iouOptionsInstalled |= 1;
        }
    iouOptionsInstalled |= 0x04; // radial interfaces 1,2

    switch (modelType)
        {
    case ModelCyber860:
        mchSetRegister(0, 0, RegIouElementId, 0x0000000002201234);
        mchSetRegister(0, 0, RegIouOptionsInstalled, iouOptionsInstalled);

        mchSetRegister(1, 0, RegProcStatusSummary, 0x0000000000000008);
        mchSetRegister(1, 0, RegProcElementId, 0x0000000000321234);

        mchSetRegister(1, 0x0a, RegMemElementId, 0x0000000001311234);
        mchSetRegister(1, 0x0a, RegMemOptionsInstalled, memSizeMask);

        if (cpuCount > 1)
            {
            mchSetRegister(2, 0, RegProcElementId, 0x0000000000321234);
            }
        break;
    default:
        logDtError(LogErrorLocation, "Unsupported machine model: %d", modelType);
        exit(1);
        }

    /*
    **  Print a friendly message.
    */
    printf("(maintenance_channel) Initialised on channel %o\n", channelNo);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Set OS bounds fault flag in IOU FS1 register.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void mchSetOsBoundsFault(PpSlot *pp)
    {
    u8  *block;
    u16 index;
    u16 limit;
    u16 mask;
    u8  *rp;
    int size;

#if DEBUG
    fprintf(mchLog, "\n%12d PP:%02o OS bounds fault",
            traceSequenceNo,
            pp->id);
    fflush(mchLog);
#endif
    rp = mchGetRegisterAddress(0, 0, RegIouFaultStatus1, &index, &mask, &limit, &block, &size);
    switch (pp->id)
        {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
        rp[0] |= mchPpMasks[pp->id] >> 24;
        break;
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
        rp[1] |= (mchPpMasks[pp->id] >> 16) & 0xff;
        break;
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
        rp[2] |= (mchPpMasks[pp->id] >> 8) & 0xff;
        break;
    case 15:
    case 16:
    case 17:
    case 18:
    case 19:
        rp[3] |= mchPpMasks[pp->id] & 0xff;
        break;
    default:
        break;
        }
    rp[5] |= 0x04;
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
#if DEBUG
    fprintf(mchLog, "\n%12d PP:%02o CH:%02o Activate",
            traceSequenceNo,
            activePpu->id,
            activeDevice->channel->id);
    fflush(mchLog);
    mchBytesIo = 0;
#endif
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
    PpWord connCode;
    PpWord typeCode;

#if DEBUG
    fprintf(mchLog, "\n%12d PP:%02o CH:%02o Disconnect",
            traceSequenceNo,
            activePpu->id,
            activeDevice->channel->id);
    fflush(mchLog);
#endif
    switch ((activeDevice->fcode & FcOpMask) >> 4)
        {
    case FcOpRead:
    case FcOpWrite:
        if (mchLocationReady == FALSE)
            {
            mchLocationReady           = TRUE;
            connCode                   = (activeDevice->fcode & FcConnMask) >> 8;
            typeCode                   = activeDevice->fcode & FcTypeMask;
            mchRegisterAddress         = mchGetRegisterAddress(connCode, typeCode, mchLocation,
                                             &mchIndex, &mchIndexMask, &mchIndexLimit, &mchRegBlockAddress, &mchRegBlockSize);
            activeDevice->recordLength = 8;
            }
        break;
        }
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
    u64    word;

    connCode = (funcCode & FcConnMask) >> 8;
    opCode   = (funcCode & FcOpMask) >> 4;
    typeCode = funcCode & FcTypeMask;

    /*
    **  Connect codes 0x800 - 0xF00 causes the MCH to be deselected.
    */
    if (connCode >= 8)
        {
#if DEBUG
    fprintf(mchLog, "\n%12d PP:%02o CH:%02o f:0x%03X MCH deselect",
            traceSequenceNo,
            activePpu->id,
            activeDevice->channel->id,
            funcCode);
#endif
        activeDevice->fcode = funcCode;
        mchTimeout = 0;
        return FcProcessed;
        }

#if DEBUG
    fprintf(mchLog, "\n%12d PP:%02o CH:%02o f:0x%03X C:%X O:%X T:%X (%s)",
            traceSequenceNo,
            activePpu->id,
            activeDevice->channel->id,
            funcCode,
            connCode,
            opCode,
            typeCode,
            mchFn2String(connCode, opCode, typeCode));
#endif

    if (mchIsConnected(connCode) == FALSE)
        {
#if DEBUG
        fputs("  Declined", mchLog);
#endif
        mchTimeout = getMilliseconds() + 1;

        return FcDeclined;
        }
    mchTimeout = 0;

    /*
    **  Process operation codes.
    */
    switch (opCode)
        {
    default:
#if DEBUG
        fputs(" : Operation not implemented & declined", mchLog);
#endif
        return FcDeclined;

    case FcOpHalt:
        word = mchGetRegister(connCode, typeCode, 0) | 0x08;
        mchSetRegister(connCode, typeCode, 0, word);
        return FcProcessed;

    case FcOpStart:
        word = mchGetRegister(connCode, typeCode, 0) & ~(u64)0x08;
        mchSetRegister(connCode, typeCode, 0, word);
        return FcProcessed;

    case FcOpClearLed:
    case FcOpMasterClear:
    case FcOpClearErrors:
        /*
        **  Do nothing.
        */
        return FcProcessed;

    case FcOpRead:
    case FcOpWrite:
    case FcOpEchoData:
        mchLocation                = 0;
        mchLocationReady           = FALSE;
        activeDevice->recordLength = 2;
        break;

    case FcOpStatusSummary:
        activeDevice->recordLength = 1;
        break;
        }

    activeDevice->fcode = funcCode;

    return FcAccepted;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Gets a 64-bit word from a register
**
**  Parameters:     Name        Description.
**                  connCode    unit connect code
**                  typeCode    type code
**                  location    location ordinal (word address)
**
**  Returns:        64-bit register value.
**
**------------------------------------------------------------------------*/
static u64 mchGetRegister(u8 connCode, u8 typeCode, u8 location)
    {
    u8  *block;
    int i;
    u16 index;
    u16 limit;
    u16 mask;
    u8  *rp;
    int size;
    u64 word;

    word = 0;
    rp = mchGetRegisterAddress(connCode, typeCode, location, &index, &mask, &limit, &block, &size);
    if (rp != NULL)
        {
        for (i = 0; i < 8; i++)
            {
            word = (word << 8) | rp[i];
            }
        }

    return word;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Calculate the address of a location in a register block.
**
**  Parameters:     Name        Description.
**                  connCode    unit connect code
**                  typeCode    type code
**                  location    location ordinal (word address)
**                  index       (out) index of first byte to read/write from register block
**                  mask        (out) mask to apply to index when incremented
**                  limit       (out) maximum index value plus 1; 0 indicates no limit
**                  block       (out) pointer to register block
**                  size        (out) size, in bytes, of selected register block
**
**  Returns:        address of location in register block
**
**------------------------------------------------------------------------*/
static u8 *mchGetRegisterAddress(u8 connCode, u8 typeCode, u8 location, u16 *index, u16 *mask, u16 *limit, u8 **block, int *size)
    {
    u8 *rp;

    *size = 0;
    rp = mchGetUnitRegisters(connCode, typeCode, size);
    if (rp == NULL)
        {
        return NULL;
        }
    *block = rp;

    // set default values
    *index = 0;
    *mask  = 0xffff;
    *limit = 8;

    switch (modelType)
        {
    case ModelCyber860:
        switch (connCode)
            {
        case 0:               // IOU
            *index = (u16)typeCode;
            *mask  = 0x0007;
            break;
        case 1: // CP or CM
            switch (typeCode)
                {
            case 1:           // Control Store
            case 3:           // ROM
            case 4:           // Soft control memory
            case 5:           // BDP control memory
            case 6:           // Instruction fetch decode memory
            case 7:           // Register file
                *limit = 0;
                break;
            case 0:           // CP
            case 0x0A:        // CM
            default:
                break;
                }
            break;
        default:
            break;
            }
        break;
    default:
        break;
        }

    return rp + (location << 3);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Calculate the address of a unit's block of registers.
**
**  Parameters:     Name        Description.
**                  connCode    unit connect code
**                  typeCode    type code
**                  size        (in/out) if non-NULL and value > 0, register
**                              block is reallocated to specified size; if
**                              non-NULL and value == 0, current size returned
**
**  Returns:        address of register block
**
**------------------------------------------------------------------------*/
static u8 *mchGetUnitRegisters(u8 connCode, u8 typeCode, int *size)
    {
    switch (modelType)
        {
    case ModelCyber860:
        switch (connCode)
            {
        case 0: // IOU
            if (iouRegisters == NULL)
                {
                iouRegisters = (u8 *)calloc(256, 8);
                }
            if (size != NULL)
                {
                *size = 256 * 8;
                }
            return iouRegisters;
        case 1: // CP or CM
            switch (typeCode)
                {
            case 0: // CP
            case 1: // Control Store
            case 3: // ROM
            case 4: // Soft control memory
            case 5: // BDP control memory
            case 6: // Instruction fetch decode memory
            case 7: // Register file
            case 0x0A: // CM
                if (mchRegisters[typeCode] == NULL)
                    {
                    mchRegisters[typeCode]     = (u8 *)calloc(256, 8);
                    mchRegisterSizes[typeCode] = 256 * 8;
                    }
                else if (size != NULL && *size > 0)
                    {
                    mchRegisters[typeCode]     = (u8 *)realloc(mchRegisters[typeCode], *size);
                    mchRegisterSizes[typeCode] = *size;
                    }
                if (size != NULL)
                    {
                    *size = mchRegisterSizes[typeCode];
                    }
                return mchRegisters[typeCode];
            default:
                break;
                }
            break;
        default:
            break;
            }
        break;
    default:
        break;
        }

    return NULL;
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
    u8     chIdx;
    PpWord connCode;
    int    i;
    PpWord opCode;
    PpWord typeCode;
    u32    osBoundary;
    u8     ppIdx;
    u32    ppVector;
    u16    regIdx;
    u16    regLimit;
    u16    regMask;
    u8     *rp;
    int    size;

    connCode = (activeDevice->fcode & FcConnMask) >> 8;
    opCode   = (activeDevice->fcode & FcOpMask) >> 4;
    typeCode = activeDevice->fcode & FcTypeMask;

    if (connCode >= 8)
        {
#if DEBUG
        fprintf(mchLog, "\n%12d PP:%02o CH:%02o I/O while deselected",
        traceSequenceNo,
        activePpu->id,
        activeDevice->channel->id);
#endif
        return;
        }

    switch (opCode)
        {
    default:
#if DEBUG
        fprintf(mchLog, "\n%12d PP:%02o CH:%02o unrecognized op code: %X",
        traceSequenceNo,
        activePpu->id,
        activeDevice->channel->id,
        opCode);
#endif
        break;

    case FcOpRead:
        if (!mchLocationReady)
            {
            if (activeChannel->full)
                {
                activeChannel->full         = FALSE;
                activeDevice->recordLength -= 1;
                mchLocation = (mchLocation << 8) | (activeChannel->data & Mask8);
#if DEBUG
                if (mchBytesIo < 1)
                    {
                    fprintf(mchLog, "\n%12d PP:%02o CH:%02o >",
                    traceSequenceNo,
                    activePpu->id,
                    activeDevice->channel->id);
                    }
                fprintf(mchLog, " %02X", activeChannel->data);
                if (activeDevice->recordLength == 0)
                    {
                    fprintf(mchLog, " (%s)", mchCw2String(connCode, typeCode, mchLocation));
                    mchBytesIo = 0;
                    }
                else
                    {
                    mchBytesIo += 1;
                    }
#endif
                }
            }
        else
            {
            if (!activeChannel->full)
                {
                if (mchIndex < mchIndexLimit || mchIndexLimit == 0)
                    {
                    activeChannel->data = mchRegisterAddress != NULL ? mchRegisterAddress[mchIndex] : 0;
                    mchIndex = (mchIndex + 1) & mchIndexMask;
                    }
                else
                    {
                    activeChannel->data = 0;
                    }
                activeChannel->full = TRUE;
#if DEBUG
                if (mchBytesIo < 1)
                    {
                    fprintf(mchLog, "\n%12d PP:%02o CH:%02o <",
                    traceSequenceNo,
                    activePpu->id,
                    activeDevice->channel->id);
                    }
                fprintf(mchLog, " %02X", activeChannel->data);
                mchBytesIo += 1;
#endif
                }
            }

        break;

    case FcOpWrite:
        if (!mchLocationReady)
            {
            if (activeChannel->full)
                {
                activeChannel->full         = FALSE;
                activeDevice->recordLength -= 1;
                mchLocation = (mchLocation << 8) | (activeChannel->data & Mask8);
#if DEBUG
                if (mchBytesIo < 1)
                    {
                    fprintf(mchLog, "\n%12d PP:%02o CH:%02o >",
                    traceSequenceNo,
                    activePpu->id,
                    activeDevice->channel->id);
                    }
                fprintf(mchLog, " %02X", activeChannel->data);
                if (activeDevice->recordLength == 0)
                    {
                    fprintf(mchLog, " (%s)", mchCw2String(connCode, typeCode, mchLocation));
                    mchBytesIo = 0;
                    }
                else
                    {
                    mchBytesIo += 1;
                    }
#endif
                }
            }
        else
            {
            if (activeChannel->full)
                {
                if (mchRegisterAddress != NULL)
                    {
                    if (mchIndex < mchIndexLimit)
                        {
                        mchRegisterAddress[mchIndex] = activeChannel->data;
                        mchIndex                     = (mchIndex + 1) & mchIndexMask;
                        activeDevice->recordLength  -= 1;
                        }
                    else if (mchIndexLimit == 0)
                        {
                        if ((mchRegisterAddress + mchIndex) >= (mchRegBlockAddress + mchRegBlockSize))
                            {
                            mchRegBlockSize   += 1024;
                            mchRegBlockAddress = mchGetUnitRegisters(connCode, typeCode, &mchRegBlockSize);
                            mchRegisterAddress = mchRegBlockAddress + (mchLocation << 3);
                            }
                        mchRegisterAddress[mchIndex] = activeChannel->data;
                        mchIndex                     = (mchIndex + 1) & mchIndexMask;
                        }
                    }

                activeChannel->full = FALSE;
#if DEBUG
                if (mchBytesIo < 1)
                    {
                    fprintf(mchLog, "\n%12d PP:%02o CH:%02o >",
                    traceSequenceNo,
                    activePpu->id,
                    activeDevice->channel->id);
                    }
                fprintf(mchLog, " %02X", activeChannel->data);
                mchBytesIo += 1;
#endif
                if ((connCode == 0) && (activeDevice->recordLength == 0))
                    {
                    if (mchLocation == RegIouEnvControl)
                        {
#if DEBUG
                        fputs("\n      Write IOU EC register", mchLog);
#endif
                        ppIdx = mchRegisterAddress[4] & Mask5;
                        if (ppIdx > 9)
                            {
                            ppIdx = (ppIdx - 0x10) + 10;
                            }
                        chIdx = mchRegisterAddress[5] & Mask5;
                        ppu[ppIdx].osBoundsCheckEnabled = (mchRegisterAddress[7] & 0x08) != 0;
                        ppu[ppIdx].isStopEnabled        = (mchRegisterAddress[7] & 0x01) != 0;
#if DEBUG
                        fprintf(mchLog, "\n        PP%02o OS bounds check: %s", ppIdx < 10 ? ppIdx : (ppIdx - 10) + 020,
                            ppu[ppIdx].osBoundsCheckEnabled ? "enabled" : "disabled");
                        fprintf(mchLog, "\n                        stop: %s", ppu[ppIdx].isStopEnabled ? "enabled" : "disabled");
#endif
                        if ((mchRegisterAddress[6] & 0x10) != 0) // load PP
                            {
                            ppu[ppIdx].opD        = chIdx;
                            channel[chIdx].active = TRUE;

                            /*
                            **  Set PP to INPUT (71) instruction.
                            */
                            ppu[ppIdx].opF  = 071;
                            ppu[ppIdx].busy = TRUE;

                            /*
                            **  Clear P register and location zero of PP.
                            */
                            ppu[ppIdx].regP   = 0;
                            ppu[ppIdx].mem[0] = 0;

                            /*
                            **  Set A register to an input word count of 10000.
                            */
                            ppu[ppIdx].regA = 010000;
#if DEBUG
                            fprintf(mchLog, "\n        Deadstart PP%02o using channel %02o",
                                ppIdx < 10 ? ppIdx : (ppIdx - 10) + 020, chIdx);
#endif
                            }
                        }
                    else if (mchLocation == RegIouOsBounds)
                        {
                        ppuOsBoundary = ((mchRegisterAddress[5] & 0x03) << 16)
                                      | (mchRegisterAddress[6] << 8)
                                      | mchRegisterAddress[7];
                        ppVector      = (mchRegisterAddress[0] << 24)
                                      | (mchRegisterAddress[1] << 16)
                                      | (mchRegisterAddress[2] << 8)
                                      | mchRegisterAddress[3];
                        for (i = 0; i < 10; i++)
                            {
                            ppu[i].isBelowOsBound = (ppVector & mchPpMasks[i]) != 0;
                            }
                        if (ppuCount > 10)
                            {
                            for (i = 10; i < 20; i++)
                                {
                                ppu[i].isBelowOsBound = (ppVector & mchPpMasks[i]) != 0;
                                }
                            }
#if DEBUG
                        fputs("\n      Write IOU OS bound register", mchLog);
                        fprintf(mchLog, "\n        OS boundary: %010o", ppuOsBoundary);
                        for (int i = 0; i < 10; i++)
                            {
                            fprintf(mchLog, "\n        PP%02o: %s", i, ppu[i].isBelowOsBound ? "below" : "above");
                            }
                        if (ppuCount > 10)
                            {
                            for (int i = 10; i < 20; i++)
                                {
                                fprintf(mchLog, "\n        PP%02o: %s", (i - 10) + 020, ppu[i].isBelowOsBound ? "below" : "above");
                                }
                            }
#endif
                        }
                    }
                }
            }

        break;

    case FcOpEchoData:
        if (!mchLocationReady)
            {
            if (activeChannel->full)
                {
                activeChannel->full         = FALSE;
                activeDevice->recordLength -= 1;
                mchLocation = (mchLocation << 8) | (activeChannel->data & Mask8);
#if DEBUG
                fprintf(mchLog, " %02X", activeChannel->data);
                if (activeDevice->recordLength == 0)
                    {
                    fputs("\n <", mchLog);
                    }
#endif
                }
            }
        else
            {
            if (!activeChannel->full)
                {
                activeChannel->data = (PpWord)mchLocation;
                activeChannel->full = TRUE;
#if DEBUG
                fprintf(mchLog, " %02X", activeChannel->data);
#endif
                }
            }

        break;

    case FcOpStatusSummary:
        if (!activeChannel->full)
            {
            activeChannel->data = 0;
            activeChannel->full = TRUE;
            }

        break;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Determine whether a connect code represents a unit
**                  supported by this machine.
**
**  Parameters:     Name        Description.
**                  connCode    connect code
**
**  Returns:        TRUE if connect code supported
**
**------------------------------------------------------------------------*/
static bool mchIsConnected(PpWord connCode)
    {
    switch (modelType)
        {
    case ModelCyber860:
        switch (connCode)
            {
        case 0:      // IOU
        case 1:      // CP or CM
            return TRUE;
        case 2:
            return cpuCount > 1;
        default:
            break;
            }
        break;
    default:
        break;
        }

    return FALSE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Put a 64-bit word into 8 bytes of a register
**
**  Parameters:     Name        Description.
**                  address     address of register location
**                  word        the word to write
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mchPutWord(u8 *address, u64 word)
    {
    int shift;

    if (address != NULL)
        {
        for (shift = 56; shift >= 0; shift -=8)
             {
             *address++ = (word >> shift) & 0xff;
             }
         }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Set a 64-bit word into a register
**
**  Parameters:     Name        Description.
**                  connCode    unit connect code
**                  typeCode    type code
**                  location    location ordinal (word address)
**                  word        the word to write
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void mchSetRegister(u8 connCode, u8 typeCode, u8 location, u64 word)
    {
    u8  *block;
    u16 index;
    u16 limit;
    u16 mask;
    int size;

    mchPutWord(mchGetRegisterAddress(connCode, typeCode, location, &index, &mask, &limit, &block, &size), word);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Convert location to string.
**
**  Parameters:     Name        Description.
**                  connCode    connection code
**                  typeCode    type code
**                  location    location code
**
**  Returns:        String equivalent of location.
**
**------------------------------------------------------------------------*/
static char *mchCw2String(PpWord connCode, PpWord typeCode, PpWord location)
    {
#if DEBUG
    switch (modelType)
        {
    case ModelCyber860:
        switch (connCode)
            {
        case 0:
            switch (location)
                {
            case 0x00:
                return "Status Summary";
            case 0x10:
                return "EID";
            case 0x12:
                return "OI";
            case 0x18:
                return "Fault Status Mask";
            case 0x21:
                return "OS Bounds";
            case 0x30:
                return "EC";
            case 0x40:
                return "Status";
            case 0x80:
                return "FS1";
            case 0x81:
                return "FS2";
            case 0xa0:
                return "TM";
            default:
                break;
                }
            break;
        case 1: // CP or CM
        case 2:
            switch (typeCode)
                {
            case 0:
            case 1:
            case 3:
            case 4:
            case 5:
            case 6:
            case 7:
                switch (location)
                    {
                case 0x00:
                    return "Status Summary";
                case 0x10:
                    return "EID";
                case 0x12:
                    return "OI";
                case 0x30:
                    return "DEC";
                case 0x80:
                    return "PFS0";
                case 0x81:
                    return "PFS1";
                case 0x82:
                    return "PFS2";
                case 0x83:
                    return "PFS3";
                case 0x84:
                    return "PFS4";
                case 0x85:
                    return "PFS5";
                case 0x86:
                    return "PFS6";
                case 0x87:
                    return "PFS7";
                case 0x88:
                    return "PFS8";
                case 0x89:
                    return "PFS9";
                case 0xa0:
                    return "PTM";
                default:
                    break;
                    }
                break;
            case 0x0A:
                switch (location)
                    {
                case 0x00:
                    return "Status Summary";
                case 0x10:
                    return "EID";
                case 0x12:
                    return "OI";
                case 0x20:
                    return "EC";
                case 0xa0:
                case 0xa1:
                case 0xa2:
                case 0xa3:
                    return "CEL";
                case 0xa4:
                case 0xa5:
                case 0xa6:
                case 0xa7:
                    return "UEL1";
                case 0xa8:
                case 0xa9:
                case 0xaa:
                case 0xab:
                    return "UEL2";
                    }
                break;
            default:
                break;
                }
            break;
        default:
            break;
            }
        break;
    default:
        break;
        }
#endif

    return "Unknown";
    }

/*--------------------------------------------------------------------------
**  Purpose:        Convert channel function to string.
**
**  Parameters:     Name        Description.
**                  connCode    connection code
**                  opCode      operation code
**                  typeCode    type code
**
**  Returns:        String equivalent of function.
**
**------------------------------------------------------------------------*/
static char *mchFn2String(PpWord connCode, PpWord opCode, PpWord typeCode)
    {
    static char buf[64];
    char *object;

#if DEBUG
    switch (modelType)
        {
    case ModelCyber860:
        switch (connCode)
            {
        case 0:
            object = "IOU";
            break;
        case 1: // CP or CM
            switch (typeCode)
                {
            case 0:
                object = "CP";
                break;
            case 1:
                object = "Control store";
                break;
            case 3:
                object = "ROM";
                break;
            case 4:
                object = "Soft control memories";
                break;
            case 5:
                object = "BDP control memories";
                break;
            case 6:
                object = "Instruction fetch decode memories";
                break;
            case 7:
                object = "Register file";
                break;
            case 0x0A:
                object = "CM control";
                break;
            default:
                object = "Unknown type";
                break;
                }
            break;
        default:
            object = "Unknown unit";
            break;
            }
        break;
    default:
        object = "Unsupported machine type";
        break;
        }

    sprintf(buf, "%s %s", mchOp2String(opCode), object);
#endif

    return buf;
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
    static char buf[16];

#if DEBUG
    switch (opCode)
        {
    case FcOpHalt:
        return "Halt";

    case FcOpStart:
        return "Start";

    case FcOpClearLed:
        return "ClearLed";

    case FcOpRead:
        return "Read";

    case FcOpWrite:
        return "Write";

    case FcOpMasterClear:
        return "MasterClear";

    case FcOpClearErrors:
        return "ClearErrors";

    case FcOpEchoData:
        return "EchoData";

    case FcOpStatusSummary:
        return "StatusSummary";
        }
#endif
    sprintf(buf, "Unknown 0x%X", opCode >> 4);

    return buf;
    }

/*---------------------------  End Of File  ------------------------------*/
