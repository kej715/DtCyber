#ifndef TYPES_H
#define TYPES_H

/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**
**  Name: types.h
**
**  Description:
**      This file defines global types.
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
**  -----------------------
**  Public Type Definitions
**  -----------------------
*/

/*
**  Basic types used in emulation.
*/
#if defined(_WIN32)

#define HOST_OS_TYPE       "windows"

/*
**  MS Win32 systems
*/
typedef signed char        i8;
typedef signed short       i16;
typedef signed long        i32;
typedef signed __int64     i64;
typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned long      u32;
typedef unsigned __int64   u64;
#define FMT60_020o    "%020I64o"
#elif defined (__GNUC__) || defined(__SunOS)
#if defined(__amd64) || defined(__amd64__) || defined(__alpha__) || defined(__powerpc64__) || defined(__ppc64__) \
    || (defined(__sparc64__) && defined(__arch64__)) || defined(__aarch64__)

/*
**  64 bit systems
*/
typedef signed char         i8;
typedef signed short        i16;
typedef signed int          i32;
typedef unsigned long int   i64;
typedef unsigned char       u8;
typedef unsigned short      u16;
typedef unsigned int        u32;
typedef unsigned long int   u64;
#define FMT60_020o    "%020lo"
#elif defined(__i386) || defined(__i386__) || defined(__powerpc__) || defined(__ppc__)   \
    || defined(__sparc__) || defined(__hppa__) || defined(__APPLE__) || defined(__arm__) \
    || defined(__ARM_ARCH_6__) || defined(__ARM_ARCH_7__) || defined(__ARM_ARCH_8__)

/*
**  32 bit systems
*/
typedef signed char              i8;
typedef signed short             i16;
typedef signed int               i32;
typedef unsigned long long int   i64;
typedef unsigned char            u8;
typedef unsigned short           u16;
typedef unsigned int             u32;
typedef unsigned long long int   u64;
#define FMT60_020o    "%020llo"
#else
#error "Unable to determine size of basic data types"
#endif
#else
#error "Unable to determine size of basic data types"
#endif

#if (!defined(__cplusplus) && !defined(bool) && !defined(CURSES) && !defined(CURSES_H) && !defined(_CURSES_H))
typedef int bool;
#endif

#if defined(__APPLE__)
#include <stdbool.h>
#endif

typedef u16 PpWord;                     /* 12 bit PP word */
typedef u8  PpByte;                     /* 6 bit PP word */
typedef u64 CpWord;                     /* 60 bit CPU word */

/*
**  Format used in displaying status of data communication interfaces (operator interface).
**
**  Fields, left to right, are:
**    local TCP address, peer TCP address, connection type (e.g., async, hasp, nje, etc.), connection state
*/
#define FMTNETSTATUS "%-21s %-21s %-8s %s"

/*
**  Function code processing status.
*/
typedef enum
    {
    FcDeclined, FcAccepted, FcProcessed
    } FcStatus;

/*
**  Device descriptor.
*/
typedef struct
    {
    char id[10];                         /* device id */
    void (*init)(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
    } DevDesc;

/*
**  Filesystem Watcher Thread Context Block.
**      20171110: SZoppi - Added Windows Support
**      20220623: SZoppi - Added Linux Support
*/
typedef struct
    {
    char id[10];                        /* device id */
    u8   eqNo;                          /* equipment number */
    u8   unitNo;                        /* unit number */
    u8   channelNo;                     /* channel number */
    int  devType;                       /* device type */
    char *inWatchDir;                   /* input directory "hopper" */
    char *outDoneDir;                   /* processed directory "hopper" */
    // void (*LoadCards)(char* fname, int channelNo, int equipmentNo, FILE* out, char* params);     /* address of load routine */
    } fswContext;


/*
**  Device control block.
*/
typedef struct devSlot
    {
    struct devSlot *next;               /* next device attached to this channel or converter */
    struct chSlot  *channel;            /* channel this device is attached to */
    FILE           *fcb[MaxUnits2];     /* unit data file control block */
    void (*activate)(void);             /* channel activation function */
    void (*disconnect)(void);           /* channel deactivation function */
    FcStatus (*func)(PpWord);           /* function request handler */
    void (*io)(void);                   /* I/O request handler */
    PpWord (*in)(void);                 /* PCI channel input request */
    void (*out)(PpWord);                /* PCI channel output request */
    void (*full)(void);                 /* PCI channel full request */
    void (*empty)(void);                /* PCI channel empty request */
    u16 (*flags)(void);                 /* PCI channel flags request */
    void           *context[MaxUnits2]; /* device specific context data */
    void           *controllerContext;  /* controller specific context data */
    PpWord         status;              /* device status */
    PpWord         fcode;               /* device function code */
    PpWord         recordLength;        /* length of read record */
    u8             devType;             /* attached device type */
    u8             eqNo;                /* equipment number */
    i8             selectedUnit;        /* selected unit */
    } DevSlot;

/*
**  Channel control block.
*/
typedef struct chSlot
    {
    DevSlot *firstDevice;               /* linked list of devices attached to this channel */
    DevSlot *ioDevice;                  /* device which deals with current function */
    PpWord  data;                       /* channel data */
    PpWord  status;                     /* channel status */
    bool    active;                     /* channel active flag */
    bool    full;                       /* channel full flag */
    bool    discAfterInput;             /* disconnect channel after input flag */
    bool    flag;                       /* optional channel flag */
    bool    inputPending;               /* input pending flag */
    bool    hardwired;                  /* hardwired devices */
    u8      id;                         /* channel number */
    u8      delayStatus;                /* time to delay change of empty/full status */
    u8      delayDisconnect;            /* time to delay disconnect */
    } ChSlot;

/*
**  PPU control block.
*/
typedef struct
    {
    u32    regA;                        /* register A (18 bit) */
    u32    regR;                        /* register R (28 bit) */
    PpWord regP;                        /* program counter (12 bit) */
    PpWord regQ;                        /* register Q (12 bit) */
    PpWord mem[PpMemSize];              /* PP memory */
    bool   busy;                        /* instruction execution state */
    int    exchangingCpu;               /* CPU for which exchange initiated */
    u8     id;                          /* PP number */
    PpByte opF;                         /* current opcode */
    PpByte opD;                         /* current opcode */
    } PpSlot;

/*
**  CPU control block.
*/
typedef struct
    {
    int           id;                   /* CPU ordinal */
    CpWord        regX[010];            /* data registers (60 bit) */
    u32           regA[010];            /* address registers (18 bit) */
    u32           regB[010];            /* index registers (18 bit) */
    u32           regP;                 /* program counter */
    u32           regRaCm;              /* reference address CM */
    u32           regFlCm;              /* field length CM */
    u32           regRaEcs;             /* reference address ECS */
    u32           regFlEcs;             /* field length ECS */
    u32           regMa;                /* monitor address */
    u32           regSpare;             /* reserved */
    u32           exitMode;             /* CPU exit mode (24 bit) */
    volatile bool isMonitorMode;        /* TRUE if CPU is in monitor mode */
    volatile bool isStopped;            /* TRUE if CPU is stopped */
    volatile int  ppRequestingExchange; /* PP number of PP requesting exchange, -1 if none */
    u32           ppExchangeAddress;    /* PP-requested exchange address */
    bool          doChangeMode;         /* TRUE if monitor mode flag should be changed by PP exchange jump */
    volatile bool isErrorExitPending;   /* TRUE if error exit pending */
    u8            exitCondition;        /* pending error exit conditions */
    CpWord        opWord;               /* Current instruction word */
    u8            opOffset;             /* Bit offset to current instruction */
    u8            opFm;                 /* Opcode field (first 6 bits) */
    u8            opI;                  /* I field of current instruction */
    u8            opJ;                  /* J field of current instruction */
    u8            opK;                  /* K field (first 3 bits only) */
    u32           opAddress;            /* K field (18 bits) */
    bool          floatException;       /* TRUE if CPU detected float exception */
    /*
    **  Instruction word stack.
    */
    CpWord        iwStack[MaxIwStack];
    u32           iwAddress[MaxIwStack];
    bool          iwValid[MaxIwStack];
    u8            iwRank;
    volatile u32 idleCycles;            /* Counter for how many times we've seen the idle loop */
    } CpuContext;

/*
**  Model specific feature set.
*/
typedef enum
    {
    HasInterlockReg        = 0x00000001,
    HasStatusAndControlReg = 0x00000002,
    HasMaintenanceChannel  = 0x00000004,
    HasTwoPortMux          = 0x00000008,
    HasChannelFlag         = 0x00000010,
    HasErrorFlag           = 0x00000020,
    HasRelocationRegShort  = 0x00000040,
    HasRelocationRegLong   = 0x00000080,
    HasRelocationReg       = 0x000000C0,
    HasMicrosecondClock    = 0x00000100,
    HasInstructionStack    = 0x00000200,
    HasIStackPrefetch      = 0x00000400,
    HasCMU                 = 0x00000800,
    HasFullRTC             = 0x00001000,
    HasNoCmWrap            = 0x00002000,
    HasNoCejMej            = 0x00004000,
    Has175Float            = 0x00008000,

    IsSeries6x00           = 0x01000000,
    IsSeries70             = 0x02000000,
    IsSeries170            = 0x04000000,
    IsSeries800            = 0x08000000,
    IsCyber875             = 0x10000000,
    } ModelFeatures;

typedef enum
    {
    Model6400,
    ModelCyber73,
    ModelCyber173,
    ModelCyber175,
    ModelCyber840A,
    ModelCyber865,
    } ModelType;

typedef enum
    {
    ECS,
    ESM
    } ExtMemory;

#endif /* TYPES_H */
/*---------------------------  End Of File  ------------------------------*/
