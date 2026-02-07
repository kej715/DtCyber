/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**                     2025, Kevin Jordan
**
**  Name: pp.c
**
**  Description:
**      Perform emulation of CDC 6600, CYBER 170, and CYBER 180 PPs
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

#define DEBUG          0
#define DEBUG_CM_WRITE 0

/*
**  -------------
**  Include Files
**  -------------
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "const.h"
#include "types.h"
#include "proto.h"

/*
**  -----------------
**  Private Constants
**  -----------------
*/
#define PPC 07400

/*
**  IOU Register addresses.
*/
#define RegStatusSummary     0x00
#define RegElementId         0x10
#define RegOptionsInstalled  0x12
#define RegFaultStatusMask   0x18
#define RegOsBounds          0x21
#define RegEnvControl        0x30
#define RegStatus            0x40
#define RegFaultStatus1      0x80
#define RegFaultStatus2      0x81
#define RegTestMode          0xA0

/*
**  -----------------------
**  Private Macro Functions
**  -----------------------
*/
#define PpIncrement(word) (word) = (((word) + 1) & Mask12)
#define PpDecrement(word) (word) = (((word) - 1) & Mask12)

///// the program (PPU ?) stops when "from" is 000 or 077 a deadstart is necessary - see 6600 RM page 4-22 (UJN)
#define PpAddOffset(to, from)         \
        {                             \
        (to) = ((to) - 1) & Mask12;   \
        if (from < 040)               \
        (to) = ((to) + (from));       \
        else                          \
        (to) = ((to) + (from) - 077); \
        if (((to) & Overflow12) != 0) \
            {                         \
            (to) += 1;                \
            }                         \
        (to) &= Mask12;               \
        }

#define IndexLocation                                                     \
    if (activePpu->opD != 0)                                              \
        {                                                                 \
        location = activePpu->mem[activePpu->opD] + activePpu->mem[activePpu->regP]; \
        }                                                                 \
    else                                                                  \
        {                                                                 \
        location = activePpu->mem[activePpu->regP];                       \
        }                                                                 \
    if ((location & Overflow12) != 0 || (location & Mask12) == 07777)     \
        {                                                                 \
        location += 1;                                                    \
        }                                                                 \
    location &= Mask12;                                                   \
    PpIncrement(activePpu->regP);

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
static bool ppCheckOsBounds(u32 address);

static void ppOpPSN(void);    // 00
static void ppOpLJM(void);    // 01
static void ppOpRJM(void);    // 02
static void ppOpUJN(void);    // 03
static void ppOpZJN(void);    // 04
static void ppOpNJN(void);    // 05
static void ppOpPJN(void);    // 06
static void ppOpMJN(void);    // 07
static void ppOpSHN(void);    // 10
static void ppOpLMN(void);    // 11
static void ppOpLPN(void);    // 12
static void ppOpSCN(void);    // 13
static void ppOpLDN(void);    // 14
static void ppOpLCN(void);    // 15
static void ppOpADN(void);    // 16
static void ppOpSBN(void);    // 17
static void ppOpLDC(void);    // 20
static void ppOpADC(void);    // 21
static void ppOpLPC(void);    // 22
static void ppOpLMC(void);    // 23
static void ppOpLRD(void);    // 24
static void ppOpSRD(void);    // 25
static void ppOpEXN(void);    // 26
static void ppOpRPN(void);    // 27
static void ppOpLDD(void);    // 30
static void ppOpADD(void);    // 31
static void ppOpSBD(void);    // 32
static void ppOpLMD(void);    // 33
static void ppOpSTD(void);    // 34
static void ppOpRAD(void);    // 35
static void ppOpAOD(void);    // 36
static void ppOpSOD(void);    // 37
static void ppOpLDI(void);    // 40
static void ppOpADI(void);    // 41
static void ppOpSBI(void);    // 42
static void ppOpLMI(void);    // 43
static void ppOpSTI(void);    // 44
static void ppOpRAI(void);    // 45
static void ppOpAOI(void);    // 46
static void ppOpSOI(void);    // 47
static void ppOpLDM(void);    // 50
static void ppOpADM(void);    // 51
static void ppOpSBM(void);    // 52
static void ppOpLMM(void);    // 53
static void ppOpSTM(void);    // 54
static void ppOpRAM(void);    // 55
static void ppOpAOM(void);    // 56
static void ppOpSOM(void);    // 57
static void ppOpCRD(void);    // 60
static void ppOpCRM(void);    // 61
static void ppOpCWD(void);    // 62
static void ppOpCWM(void);    // 63
static void ppOpAJM(void);    // 64
static void ppOpIJM(void);    // 65
static void ppOpFJM(void);    // 66
static void ppOpEJM(void);    // 67
static void ppOpIAN(void);    // 70
static void ppOpIAM(void);    // 71
static void ppOpOAN(void);    // 72
static void ppOpOAM(void);    // 73
static void ppOpACN(void);    // 74
static void ppOpDCN(void);    // 75
static void ppOpFAN(void);    // 76
static void ppOpFNC(void);    // 77

static void ppOpRDSL(void);   // 1000
static void ppOpRDCL(void);   // 1001
static void ppOpLPDL(void);   // 1022
static void ppOpLPIL(void);   // 1023
static void ppOpLPML(void);   // 1024
static void ppOpINPN(void);   // 1026
static void ppOpLDDL(void);   // 1030
static void ppOpADDL(void);   // 1031
static void ppOpSBDL(void);   // 1032
static void ppOpLMDL(void);   // 1033
static void ppOpSTDL(void);   // 1034
static void ppOpRADL(void);   // 1035
static void ppOpAODL(void);   // 1036
static void ppOpSODL(void);   // 1037
static void ppOpLDIL(void);   // 1040
static void ppOpADIL(void);   // 1041
static void ppOpSBIL(void);   // 1042
static void ppOpLMIL(void);   // 1043
static void ppOpSTIL(void);   // 1044
static void ppOpRAIL(void);   // 1045
static void ppOpAOIL(void);   // 1046
static void ppOpSOIL(void);   // 1047
static void ppOpLDML(void);   // 1050
static void ppOpADML(void);   // 1051
static void ppOpSBML(void);   // 1052
static void ppOpLMML(void);   // 1053
static void ppOpSTML(void);   // 1054
static void ppOpRAML(void);   // 1055
static void ppOpAOML(void);   // 1056
static void ppOpSOML(void);   // 1057
static void ppOpCRDL(void);   // 1060
static void ppOpCRML(void);   // 1061
static void ppOpCWDL(void);   // 1062
static void ppOpCWML(void);   // 1063
static void ppOpFSJM(void);   // 1064
static void ppOpFCJM(void);   // 1065
static void ppOpIAPM(void);   // 1071
static void ppOpOAPM(void);   // 1073

static u32 ppAdd18(u32 op1, u32 op2);
static void ppFlushIoBuf(void);
static void ppResetIoBuf(void);
static u32 ppSubtract18(u32 op1, u32 op2);

#if DEBUG_CM_WRITE
static void ppValidateCmWrite(char *inst, u32 address, CpWord data);
#endif

/*
**  ----------------
**  Public Variables
**  ----------------
*/
u32    iouOsBoundary;
PpSlot *ppu;
PpSlot *activePpu;
u8     ppuCount;

/*
**  -----------------
**  Private Variables
**  -----------------
*/
static FILE   *ppHandle;
static u8     pp = 0;
static PpWord location;
static u32    acc18;
static bool   noHang;

//
//  Maintenance access information for CYBER 180 IOU
//
static u32 iouEid;
static u64 iouEnvControl;
static u64 iouFaultStatus1;
static u64 iouFaultStatusMask;
static u64 iouOptions;
static u64 iouOsBoundaryReg;
static u16 iouRegisterAddr;
static u8  iouRegisterBuf[8];
static u8  iouRegisterBufIdx;
static u64 iouStatus;
static u64 iouTestMode;
//
//  Bit masks identifying PP's in IOU OS Bounds and fault registers
//
static u64 iouPpMasks[20] =
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

//
//  Instruction processors
//
static void (*ppOp170[])(void) =
    {
    ppOpPSN,    // 00
    ppOpLJM,    // 01
    ppOpRJM,    // 02
    ppOpUJN,    // 03
    ppOpZJN,    // 04
    ppOpNJN,    // 05
    ppOpPJN,    // 06
    ppOpMJN,    // 07
    ppOpSHN,    // 10
    ppOpLMN,    // 11
    ppOpLPN,    // 12
    ppOpSCN,    // 13
    ppOpLDN,    // 14
    ppOpLCN,    // 15
    ppOpADN,    // 16
    ppOpSBN,    // 17
    ppOpLDC,    // 20
    ppOpADC,    // 21
    ppOpLPC,    // 22
    ppOpLMC,    // 23
    ppOpLRD,    // 24
    ppOpSRD,    // 25
    ppOpEXN,    // 26
    ppOpRPN,    // 27
    ppOpLDD,    // 30
    ppOpADD,    // 31
    ppOpSBD,    // 32
    ppOpLMD,    // 33
    ppOpSTD,    // 34
    ppOpRAD,    // 35
    ppOpAOD,    // 36
    ppOpSOD,    // 37
    ppOpLDI,    // 40
    ppOpADI,    // 41
    ppOpSBI,    // 42
    ppOpLMI,    // 43
    ppOpSTI,    // 44
    ppOpRAI,    // 45
    ppOpAOI,    // 46
    ppOpSOI,    // 47
    ppOpLDM,    // 50
    ppOpADM,    // 51
    ppOpSBM,    // 52
    ppOpLMM,    // 53
    ppOpSTM,    // 54
    ppOpRAM,    // 55
    ppOpAOM,    // 56
    ppOpSOM,    // 57
    ppOpCRD,    // 60
    ppOpCRM,    // 61
    ppOpCWD,    // 62
    ppOpCWM,    // 63
    ppOpAJM,    // 64
    ppOpIJM,    // 65
    ppOpFJM,    // 66
    ppOpEJM,    // 67
    ppOpIAN,    // 70
    ppOpIAM,    // 71
    ppOpOAN,    // 72
    ppOpOAM,    // 73
    ppOpACN,    // 74
    ppOpDCN,    // 75
    ppOpFAN,    // 76
    ppOpFNC     // 77
    };

static void (*ppOp180[])(void) =
    {
    ppOpRDSL,   // 1000
    ppOpRDCL,   // 1001
    ppOpPSN,    // 1002
    ppOpPSN,    // 1003
    ppOpPSN,    // 1004
    ppOpPSN,    // 1005
    ppOpPSN,    // 1006
    ppOpPSN,    // 1007
    ppOpPSN,    // 1010
    ppOpPSN,    // 1011
    ppOpPSN,    // 1012
    ppOpPSN,    // 1013
    ppOpPSN,    // 1014
    ppOpPSN,    // 1015
    ppOpPSN,    // 1016
    ppOpPSN,    // 1017
    ppOpPSN,    // 1020
    ppOpPSN,    // 1021
    ppOpLPDL,   // 1022
    ppOpLPIL,   // 1023
    ppOpLPML,   // 1024
    ppOpPSN,    // 1025
    ppOpINPN,   // 1026
    ppOpPSN,    // 1027
    ppOpLDDL,   // 1030
    ppOpADDL,   // 1031
    ppOpSBDL,   // 1032
    ppOpLMDL,   // 1033
    ppOpSTDL,   // 1034
    ppOpRADL,   // 1035
    ppOpAODL,   // 1036
    ppOpSODL,   // 1037
    ppOpLDIL,   // 1040
    ppOpADIL,   // 1041
    ppOpSBIL,   // 1042
    ppOpLMIL,   // 1043
    ppOpSTIL,   // 1044
    ppOpRAIL,   // 1045
    ppOpAOIL,   // 1046
    ppOpSOIL,   // 1047
    ppOpLDML,   // 1050
    ppOpADML,   // 1051
    ppOpSBML,   // 1052
    ppOpLMML,   // 1053
    ppOpSTML,   // 1054
    ppOpRAML,   // 1055
    ppOpAOML,   // 1056
    ppOpSOML,   // 1057
    ppOpCRDL,   // 1060
    ppOpCRML,   // 1061
    ppOpCWDL,   // 1062
    ppOpCWML,   // 1063
    ppOpFSJM,   // 1064
    ppOpFCJM,   // 1065
    ppOpPSN,    // 1066
    ppOpPSN,    // 1067
    ppOpPSN,    // 1070
    ppOpIAPM,   // 1071
    ppOpPSN,    // 1072
    ppOpOAPM,   // 1073
    ppOpPSN,    // 1074
    ppOpPSN,    // 1075
    ppOpPSN,    // 1076
    ppOpPSN     // 1077
    };

#if DEBUG || DEBUG_CM_WRITE
static FILE *ppLog = NULL;
#endif

/*
 **--------------------------------------------------------------------------
 **
 **  Public Functions
 **
 **--------------------------------------------------------------------------
 */

/*--------------------------------------------------------------------------
**  Purpose:        Initialise PP subsystem.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void ppInit(u8 count)
    {
    /*
    **  Allocate ppu structures.
    */
    ppuCount = count;
    ppu      = calloc(count, sizeof(PpSlot));
    if (ppu == NULL)
        {
        logDtError(LogErrorLocation, "Failed to allocate ppu control blocks\n");
        exit(1);
        }

    /*
    **  Optionally read in persistent CM and ECS contents.
    */
    if (*persistDir != '\0')
        {
        char fileName[256];

        /*
        **  Try to open existing CM file.
        */
        strcpy(fileName, persistDir);
        strcat(fileName, "/ppStore");
        ppHandle = fopen(fileName, "r+b");
        if (ppHandle != NULL)
            {
            /*
            **  Read PPM contents.
            */
            if (fread(ppu, sizeof(PpSlot), count, ppHandle) != count)
                {
                printf("(pp     ) Unexpected length of PPM backing file, clearing PPM\n");
                memset(ppu, 0, count * sizeof(PpSlot));
                }
            }
        else
            {
            /*
            **  Create a new file.
            */
            ppHandle = fopen(fileName, "w+b");
            if (ppHandle == NULL)
                {
                logDtError(LogErrorLocation, "Failed to create PPM backing file\n");
                exit(1);
                }
            }
        }

    /*
    **  Initialise all ppus.
    */
    for (pp = 0; pp < ppuCount; pp++)
        {
        ppu[pp].id                   = pp;
        ppu[pp].busy                 = FALSE;
        ppu[pp].exchangingCpu        = -1;
        ppu[pp].isStopped            = FALSE;
        ppu[pp].isStopEnabled        = FALSE;
        ppu[pp].isIdle               = FALSE;
        ppu[pp].isDump               = FALSE;
        ppu[pp].isLoad               = FALSE;
        ppu[pp].osBoundsCheckEnabled = FALSE;
        ppu[pp].isBelowOsBound       = FALSE;
        }

    pp = 0;

    /*
    **  Print a friendly message.
    */
    printf("(pp     ) PPs initialised (number of PPUs %o)\n", ppuCount);

#if DEBUG || DEBUG_CM_WRITE
    if (ppLog == NULL)
        {
        ppLog = fopen("pplog.txt", "wt");
        }
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Maintenance access: get a value from an IOU maintenance register
**
**  Parameters:     Name        Description.
**                  reg         the register address
**
**  Returns:        64-bit value.
**
**------------------------------------------------------------------------*/
u64 ppMacGetIouRegister(u8 reg)
    {
    u8  ppIdx;
    u8  regSelect;
    u32 regVal;

    switch (reg)
        {
    case RegStatusSummary:
    default:
        return 0;
    case RegElementId:
        return iouEid;
    case RegEnvControl:
        return iouEnvControl;
    case RegFaultStatus1:
        return iouFaultStatus1;
    case RegFaultStatusMask:
        return iouFaultStatusMask;
    case RegOptionsInstalled:
        return iouOptions;
    case RegOsBounds:
        return iouOsBoundaryReg;
    case RegStatus:
        ppIdx     = (iouEnvControl >> 24) & Mask5;
        regSelect = (iouEnvControl >> 8) & Mask2;
        if (ppIdx >= 020)
            {
            ppIdx = (ppIdx - 020) + 10;
            }
        switch (regSelect)
            {
        default:
        case 0: // A register
            regVal = ppu[ppIdx].regA;
            break;
        case 1: // P register
            regVal = ppu[ppIdx].regP;
            break;
        case 2: // K register
            regVal = ppu[ppIdx].isIdle ? 0107700 : ppu[ppIdx].regK;
            break;
        case 3: // Q register
            regVal = ppu[ppIdx].regQ;
            break;
            }
        return (iouStatus & Mask8) | (regVal << 8);
    case RegTestMode:
        return iouTestMode;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Maintenance access: initialize IOU maintenance registers
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void ppMacInit(void)
    {
    /*
    **  Initialize maintenance access information
    */
    iouEid     = 0x02201234; // Elem: 02 (IOU), Model: 835-990, S/N

    iouOptions = 0x000000FFAF000000; // channels 00 - 17
    if (channelCount > 16)
        {
        iouOptions |= 0x0000000000FF0F00;
        }
    iouOptions |= (u64)0x03 << 40;     // PP's 00 - 11
    if (ppuCount > 10)
        {
        iouOptions |= (u64)0x0C << 40; // PP's 20 - 31
        }
    if (tpMuxEnabled)
        {
        iouOptions |= 2;
        }
    if (cc545Enabled)
        {
        iouOptions |= 1;
        }
    iouOptions |= 0x04; // radial interfaces 1,2

    /*
    **  Print a friendly message.
    */
    fputs("(pp     ) IOU maintenance registers initialised\n", stdout);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Maintenance access: read IOU data
**
**  Parameters:     Name        Description.
**
**  Returns:        next byte.
**
**------------------------------------------------------------------------*/
u8 ppMacReadIou(void)
    {
    u8  i;
    u8  shift;
    u64 word;

    if (iouRegisterBufIdx < 8)
        {
        if (iouRegisterBufIdx == 0)
            {
            word  = ppMacGetIouRegister((u8)iouRegisterAddr);
            shift = 56;
            for (i = 0; i < 8; i++)
                {
                iouRegisterBuf[i] = (word >> shift) & Mask8;
                shift -= 8;
                }
            }
        return iouRegisterBuf[iouRegisterBufIdx++];
        }
    else
        {
        return 0;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Maintenance access: set IOU register location
**
**  Parameters:     Name        Description.
**                  location    the location
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void ppMacSetIouLocation(u16 location)
    {
    iouRegisterAddr   = location;
    iouRegisterBufIdx = 0;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Maintenance access: set a value in an IOU maintenance register
**
**  Parameters:     Name        Description.
**                  reg         the register address
**                  word        the 64-bit value to set
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void ppMacSetIouRegister(u8 reg, u64 word)
    {
    u8     chIdx;
    int    i;
    PpSlot *pp;
    u8     ppIdx;
    u32    ppVector;

    switch (reg)
        {
    default:
        break;

    case RegEnvControl:
        iouEnvControl = word;
        chIdx         = (word >> 16) & Mask5;
        ppIdx         = (word >> 24) & Mask5;
        if (ppIdx >= 020)
            {
            ppIdx = (ppIdx - 020) + 10;
            }
        pp                       = &ppu[ppIdx];
        pp->osBoundsCheckEnabled = (word & 0x08) != 0;
        pp->isStopEnabled        = (word & 0x01) != 0;
        pp->isStopped            = FALSE;

        if ((word & 0x0020) != 0) // load/dump/idle PP
            {
            pp->isIdle = FALSE;
            pp->isDump = FALSE;
            pp->isLoad = FALSE;

            if ((word & 0x1000) != 0) // load PP
                {
                pp->isLoad            = TRUE;
                pp->opD               = chIdx;
                channel[chIdx].active = TRUE;

                /*
                **  Set PP to INPUT (IAM) instruction.
                */
                pp->opF  = 071;
                pp->busy = TRUE;
                pp->regK = (PpWord)((pp->opF << 6) | pp->opD);

                /*
                **  Clear P register and location zero of PP.
                */
                pp->regP   = 0;
                pp->mem[0] = 0;

                /*
                **  Set A register to an input word count of 10000.
                */
                pp->regA = 010000;
                }
            if ((word & 0x0800) != 0) // dump PP
                {
                pp->isDump = TRUE;
                pp->opD    = chIdx;

                /*
                **  Set PP to OUTPUT (OAM) instruction.
                */
                pp->opF  = 073;
                pp->busy = channel[chIdx].active;
                pp->regK = (PpWord)((pp->opF << 6) | pp->opD);

                /*
                **  Clear P register and location zero of PP.
                */
                pp->regP   = 0;
                pp->mem[0] = 0;

                /*
                **  Set A register to an output word count of 10000.
                */
                pp->regA = 010000;
                }
            if ((word & 0x0400) != 0) // idle PP
                {
                pp->isIdle = TRUE;
                }
            }
        else
            {
            if ((pp->isLoad || pp->isDump) && pp->busy)
                {
                pp->busy = FALSE;
                pp->regP = pp->mem[0];
                PpIncrement(pp->regP);
                }
            pp->isIdle = FALSE;
            pp->isLoad = FALSE;
            pp->isDump = FALSE;
            }
#if DEBUG
        fputs("Write IOU EC register\n", ppLog);
        fprintf(ppLog, "  PP%02o\n", ppIdx < 10 ? ppIdx : (ppIdx - 10) + 020);
        fprintf(ppLog, "          Auto mode: %s\n", (word & 0x20000000) != 0 ? "enabled" : "disabled");
        fprintf(ppLog, "    Register select: %c\n", "APKQ"[(word >> 8) & Mask2]);
        fprintf(ppLog, "    OS bounds check: %s\n", pp->osBoundsCheckEnabled ? "enabled" : "disabled");
        fprintf(ppLog, "      Stop on error: %s\n", pp->isStopEnabled ? "enabled" : "disabled");
        fputs(         "     Load/Dump/Idle: ", ppLog);
        if ((word & 0x0020) != 0)
            {
            fputs("enabled\n", ppLog);
            fprintf(ppLog, "            Channel: %02o\n", chIdx);
            fprintf(ppLog, "               Load: %s\n", pp->isLoad ? "yes" : "no");
            fprintf(ppLog, "               Dump: %s\n", pp->isDump ? "yes" : "no");
            fprintf(ppLog, "               Idle: %s\n", pp->isIdle ? "yes" : "no");
            }
        else
            {
            fputs("disabled\n", ppLog);
            }
#endif
        break;

    case RegFaultStatus1:
        iouFaultStatus1 = word;
        break;

    case RegFaultStatusMask:
        iouFaultStatusMask = word;
        break;

    case RegOsBounds:
        iouOsBoundaryReg = word;
        iouOsBoundary    = (u32)(((word & 0x3ffff) << 10) & Mask32);
        ppVector         = word >> 32;
        for (i = 0; i < 10; i++)
            {
            ppu[i].isBelowOsBound = (ppVector & iouPpMasks[i]) != 0;
            }
        if (ppuCount > 10)
            {
            for (i = 10; i < 20; i++)
                {
                ppu[i].isBelowOsBound = (ppVector & iouPpMasks[i]) != 0;
                }
            }
#if DEBUG
        fputs("Write IOU OS bound register\n", ppLog);
        fprintf(ppLog, "  OS boundary: %010o\n", iouOsBoundary);
        for (int i = 0; i < 10; i++)
            {
            fprintf(ppLog, "  PP%02o: %s\n", i, ppu[i].isBelowOsBound ? "below" : "above");
            }
        if (ppuCount > 10)
            {
            for (int i = 10; i < 20; i++)
                {
                fprintf(ppLog, "  PP%02o: %s\n", (i - 10) + 020, ppu[i].isBelowOsBound ? "below" : "above");
                }
            }
#endif
        break;
    case RegTestMode:
        iouTestMode = word;
        break;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Maintenance access: write IOU data
**
**  Parameters:     Name        Description.
**                  byte        the byte to write
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void ppMacWriteIou(u8 byte)
    {
    u8  i;
    u64 word;

    if (iouRegisterBufIdx < 8)
        {
        iouRegisterBuf[iouRegisterBufIdx++] = byte;
        if (iouRegisterBufIdx >= 8)
            {
            word = 0;
            for (i = 0; i < 8; i++)
                {
                word = (word << 8) | iouRegisterBuf[i];
                }
            ppMacSetIouRegister((u8)iouRegisterAddr, word);
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Terminate PP subsystem.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void ppTerminate(void)
    {
    /*
    **  Optionally save PPM.
    */
    if (ppHandle != NULL)
        {
        fseek(ppHandle, 0, SEEK_SET);
        if (fwrite(ppu, sizeof(PpSlot), ppuCount, ppHandle) != ppuCount)
            {
            logDtError(LogErrorLocation, "Error writing PPM backing file\n");
            }

        fclose(ppHandle);
        }

    /*
    **  Free allocated memory.
    */
    free(ppu);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Execute one instruction in an active PPU.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void ppStep(void)
    {
    u8 i;

    /*
    **  Exercise each PP in the barrel.
    */
    for (i = 0; i < ppuCount; i++)
        {
        /*
        **  Advance to next PPU.
        */
        activePpu = ppu + i;

        if (activePpu->isStopped || (activePpu->isIdle && !activePpu->busy))
            {
            continue;
            }

        if (activePpu->exchangingCpu >= 0)
            {
            cpuAcquireExchangeMutex();
            if (cpus170[activePpu->exchangingCpu].ppRequestingExchange == activePpu->id)
                {
                cpuReleaseExchangeMutex();
                continue;
                }
            else
                {
                activePpu->exchangingCpu = -1;
                cpuReleaseExchangeMutex();
                }
            }

        if (!activePpu->busy)
            {
            /*
            **  Extract next PPU instruction.
            */
            activePpu->regK = activePpu->mem[activePpu->regP];
 
            if (isCyber180)
                {
                activePpu->opF = (activePpu->regK >> 6) & 01777;
                if ((activePpu->opF & 0700) != 0)
                    {
                    activePpu->opF = 0;
                    }
                }
            else
                {
                activePpu->opF = (activePpu->regK >> 6) & 077;
                }
            activePpu->opD = activePpu->regK & 077;

#if CcDebug == 1
            /*
            **  Trace instructions.
            */
            traceSequence();
            traceRegisters(FALSE);
            traceOpcode();
#endif

            /*
            **  Increment register P.
            */
            PpIncrement(activePpu->regP);

            /*
            **  Execute PPU instruction.
            */
            if ((activePpu->opF & 01000) == 0)
                {
                ppOp170[activePpu->opF]();
                }
            else
                {
                ppOp180[activePpu->opF & 077]();
                }
            }
        else
            {
            /*
            **  Resume PPU instruction.
            */
            if ((activePpu->opF & 01000) == 0)
                {
                ppOp170[activePpu->opF]();
                }
            else
                {
                ppOp180[activePpu->opF & 077]();
                }
            }

#if CcDebug == 1
        if (!activePpu->busy)
            {
            /*
            **  Trace result.
            */
            traceRegisters(TRUE);

            /*
            **  Trace new channel status.
            */
            if ((activePpu->opF & 077) >= 064)
                {
                traceChannel((u8)(activePpu->opD & 037));
                }

            traceEnd();
            }
#endif
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
**  Purpose:        18 bit ones-complement addition with subtractive adder
**
**  Parameters:     Name        Description.
**                  op1         18 bit operand1
**                  op2         18 bit operand2
**
**  Returns:        18 bit result.
**
**------------------------------------------------------------------------*/
static u32 ppAdd18(u32 op1, u32 op2)
    {
    acc18 = (op1 & Mask18) - (~op2 & Mask18);
    if ((acc18 & Overflow18) != 0)
        {
        acc18 -= 1;
        }

    return (acc18 & Mask18);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Check a CM reference against the Cyber 180 OS bounds register
**
**  Parameters:     Name        Description.
**                  address     absolute CM address
**
**  Returns:        TRUE if OS bounds register violation.
**
**------------------------------------------------------------------------*/
static bool ppCheckOsBounds(u32 address)
    {
    u64 word;

    if (isCyber180 && activePpu->osBoundsCheckEnabled)
        {
        if ((activePpu->isBelowOsBound && (address >= iouOsBoundary))
            || ((activePpu->isBelowOsBound == FALSE) && (address < iouOsBoundary)))
            {
            word = ppMacGetIouRegister(RegFaultStatus1);
            ppMacSetIouRegister(RegFaultStatus1, word | (iouPpMasks[activePpu->id] << 32) | 0x040000);
#if DEBUG
            fprintf(ppLog, "PP:%02o OS bounds fault, reference to %o is %s boundary %o",
                activePpu->id,
                address,
                activePpu->isBelowOsBound ? "above" : "below",
                iouOsBoundary);
            fflush(ppLog);
#endif
            return TRUE;
            }
        }
    return FALSE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        18 bit ones-complement subtraction
**
**  Parameters:     Name        Description.
**                  op1         18 bit operand1
**                  op2         18 bit operand2
**
**  Returns:        18 bit result.
**
**------------------------------------------------------------------------*/
static u32 ppSubtract18(u32 op1, u32 op2)
    {
    acc18 = (op1 & Mask18) - (op2 & Mask18);
    if ((acc18 & Overflow18) != 0)
        {
        acc18 -= 1;
        }

    return (acc18 & Mask18);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Functions to implement all opcodes
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void ppOpPSN(void)     // 00
    {
    /*
    **  Do nothing.
    */
    }

static void ppOpLJM(void)     // 01
    {
    IndexLocation;
    activePpu->regP = location;
    }

static void ppOpRJM(void)     // 02
    {
    IndexLocation;
    activePpu->mem[location] = activePpu->regP;
    PpIncrement(location);
    activePpu->regP = location;
    }

static void ppOpUJN(void)     // 03
    {
    PpAddOffset(activePpu->regP, activePpu->opD);
    }

static void ppOpZJN(void)     // 04
    {
    if (activePpu->regA == 0)
        {
        PpAddOffset(activePpu->regP, activePpu->opD);
        }
    }

static void ppOpNJN(void)     // 05
    {
    if (activePpu->regA != 0)
        {
        PpAddOffset(activePpu->regP, activePpu->opD);
        }
    }

static void ppOpPJN(void)     // 06
    {
    if (activePpu->regA < 0400000)
        {
        PpAddOffset(activePpu->regP, activePpu->opD);
        }
    }

static void ppOpMJN(void)     // 07
    {
    if (activePpu->regA > 0377777)
        {
        PpAddOffset(activePpu->regP, activePpu->opD);
        }
    }

static void ppOpSHN(void)     // 10
    {
    u64 acc;

    if (activePpu->opD < 040)
        {
        activePpu->opD  = activePpu->opD % 18;
        acc             = activePpu->regA & Mask18;
        acc           <<= activePpu->opD;
        activePpu->regA = (u32)((acc & Mask18) | (acc >> 18));
        }
    else if (activePpu->opD > 037)
        {
        activePpu->opD    = 077 - activePpu->opD;
        activePpu->regA >>= activePpu->opD;
        }
    }

static void ppOpLMN(void)     // 11
    {
    activePpu->regA ^= activePpu->opD;
    }

static void ppOpLPN(void)     // 12
    {
    activePpu->regA &= activePpu->opD;
    }

static void ppOpSCN(void)     // 13
    {
    activePpu->regA &= ~(activePpu->opD & 077);
    }

static void ppOpLDN(void)     // 14
    {
    activePpu->regA = activePpu->opD;
    }

static void ppOpLCN(void)     // 15
    {
    activePpu->regA = ~activePpu->opD & Mask18;
    }

static void ppOpADN(void)     // 16
    {
    activePpu->regA = ppAdd18(activePpu->regA, activePpu->opD);
    }

static void ppOpSBN(void)     // 17
    {
    activePpu->regA = ppSubtract18(activePpu->regA, activePpu->opD);
    }

static void ppOpLDC(void)     // 20
    {
    activePpu->regA = (activePpu->opD << 12) | (activePpu->mem[activePpu->regP] & Mask12);
    PpIncrement(activePpu->regP);
    }

static void ppOpADC(void)     // 21
    {
    activePpu->regA = ppAdd18(activePpu->regA, (activePpu->opD << 12) | (activePpu->mem[activePpu->regP] & Mask12));
    PpIncrement(activePpu->regP);
    }

static void ppOpLPC(void)     // 22
    {
    activePpu->regA &= (activePpu->opD << 12) | (activePpu->mem[activePpu->regP] & Mask12);
    PpIncrement(activePpu->regP);
    }

static void ppOpLMC(void)     // 23
    {
    activePpu->regA ^= (activePpu->opD << 12) | (activePpu->mem[activePpu->regP] & Mask12);
    PpIncrement(activePpu->regP);
    }

static void ppOpLRD(void)     // 24
    {
    if (activePpu->opD != 0)
        {
        if ((features & HasRelocationRegShort) != 0)
            {
            /*
            **  LRD.
            */
            activePpu->regR = ((u32)(activePpu->mem[activePpu->opD] & Mask4) << 18)
                            | ((u32)(activePpu->mem[activePpu->opD + 1] & Mask12) << 6);
            }
        else if ((features & HasRelocationRegLong) != 0)
            {
            /*
            **  LRD.
            */
            activePpu->regR = ((u32)(activePpu->mem[activePpu->opD] & Mask10) << 18)
                            | ((u32)(activePpu->mem[activePpu->opD + 1] & Mask12) << 6);
            }
        }
    }

static void ppOpSRD(void)     // 25
    {
    if (activePpu->opD != 0)
        {
        if ((features & HasRelocationRegShort) != 0)
            {
            /*
            **  SRD.
            */
            activePpu->mem[activePpu->opD]     = (PpWord)(activePpu->regR >> 18) & Mask4;
            activePpu->mem[activePpu->opD + 1] = (PpWord)(activePpu->regR >> 6) & Mask12;
            }
        else if ((features & HasRelocationRegLong) != 0)
            {
            /*
            **  SRD.
            */
            activePpu->mem[activePpu->opD]     = (PpWord)(activePpu->regR >> 18) & Mask10;
            activePpu->mem[activePpu->opD + 1] = (PpWord)(activePpu->regR >> 6) & Mask12;
            }
        }
    }

static void ppOpEXN(void)     // 26
    {
    Cpu170Context *cpu170;
    Cpu180Context *cpu180;
    int           cpuNum;
    bool          doChangeMode;
    bool          isExchangePending;
    u32           exchangeAddress;

    cpuNum = (cpuCount > 1) ? (activePpu->opD & 001) : 0;
    cpu170 = cpus170 + cpuNum;
    if (isCyber180)
        {
        cpu180 = cpus180 + cpuNum;
        }

    cpuAcquireExchangeMutex();
    isExchangePending = cpu170->ppRequestingExchange != -1;

    if (((activePpu->opD & 070) == 0) || ((features & HasNoCejMej) != 0))
        {
        /*
        **  EXN or MXN/MAN with CEJ/MEJ disabled.
        */
        if (isExchangePending)
            {
            // Release mutex and arrange to retry instruction
            cpuReleaseExchangeMutex();
            PpDecrement(activePpu->regP);

            return;
            }
        doChangeMode = FALSE;
        if (((activePpu->regA & Sign18) != 0) && ((features & HasRelocationReg) != 0))
            {
            exchangeAddress = activePpu->regR + (activePpu->regA & Mask17);
            if ((features & HasRelocationRegShort) != 0)
                {
                exchangeAddress &= Mask18;
                }
            }
        else
            {
            exchangeAddress = activePpu->regA & Mask18;
            }
        }
    else
        {
        doChangeMode = TRUE;
        if ((activePpu->opD & 070) == 010)
            {
            /*
            **  MXN.
            */

            if (((activePpu->regA & Sign18) != 0) && ((features & HasRelocationReg) != 0))
                {
                exchangeAddress = activePpu->regR + (activePpu->regA & Mask17);
                if ((features & HasRelocationRegShort) != 0)
                    {
                    exchangeAddress &= Mask18;
                    }
                }
            else
                {
                exchangeAddress = activePpu->regA & Mask18;
                }
            }
        else if ((activePpu->opD & 070) == 020)
            {
            /*
            **  MAN.
            */
            exchangeAddress = cpu170->regMa & Mask18;
            }
        else
            {
            /*
            **  Pass.
            */
            cpuReleaseExchangeMutex();

            return;
            }
        if (isExchangePending)
            {
            /*
            **  Pass.
            */
            cpuReleaseExchangeMutex();

            return;
            }
        if (cpu170->isMonitorMode)
            {
            /*
            **  Pass.
            */
            cpuReleaseExchangeMutex();
            //
            //  If this is a CYBER 180, and the machine is in 180
            //  mode, set the CYBER 170 Exchange bit to request
            //  an exchange back to 170 mode. This should enable the
            //  PP to exchange the 170 machine as soon as possible.
            //
            if (isCyber180 && cpu180->regVmid == 0)
                {
                cpu180->regMcr |= 0x0400; // set MCR53
                }

            return;
            }
        }
    if (ppCheckOsBounds(exchangeAddress))
        {
        cpuReleaseExchangeMutex();
        if (activePpu->isStopEnabled)
            {
            activePpu->isStopped = TRUE;
            PpDecrement(activePpu->regP);
            }
        }
    else
        {
        /*
        **  Request the exchange, and wait for it to complete.
        */
        cpu170->ppExchangeAddress    = exchangeAddress;
        cpu170->doChangeMode         = doChangeMode;
        cpu170->ppRequestingExchange = activePpu->id;
        activePpu->exchangingCpu     = cpu170->id;
        if (isCyber180)
            {
            cpu180->regMcr |= 0x0400; // set MCR53
            }
        cpuReleaseExchangeMutex();
        }
    }

static void ppOpRPN(void)     // 27
    {
    u8 cpuNum;

    /*
    **  RPN on 170's and 865/875, KPT on other 800 series models, and PSN on all other models.
    **  Even on 800 series, KPT behaves as PSN. On real hardware, it allows test-point
    **  sensing by external monitoring equipment.
    */
    if (((features & IsSeries800) == 0) || (modelType == ModelCyber865))
        {
        cpuNum          = (cpuCount > 1) ? (activePpu->opD & 001) : 0;
        activePpu->regA = cpuGetP(cpuNum);
        }
    }

static void ppOpLDD(void)     // 30
    {
    activePpu->regA  = activePpu->mem[activePpu->opD] & Mask12;
    }

static void ppOpADD(void)     // 31
    {
    activePpu->regA = ppAdd18(activePpu->regA, activePpu->mem[activePpu->opD] & Mask12);
    }

static void ppOpSBD(void)     // 32
    {
    activePpu->regA = ppSubtract18(activePpu->regA, activePpu->mem[activePpu->opD] & Mask12);
    }

static void ppOpLMD(void)     // 33
    {
    activePpu->regA ^= activePpu->mem[activePpu->opD] & Mask12;
    }

static void ppOpSTD(void)     // 34
    {
    activePpu->mem[activePpu->opD] = (PpWord)activePpu->regA & Mask12;
    }

static void ppOpRAD(void)     // 35
    {
    activePpu->regA                = ppAdd18(activePpu->regA, activePpu->mem[activePpu->opD] & Mask12);
    activePpu->mem[activePpu->opD] = (PpWord)activePpu->regA & Mask12;
    }

static void ppOpAOD(void)     // 36
    {
    activePpu->regA                = ppAdd18(activePpu->mem[activePpu->opD] & Mask12, 1);
    activePpu->mem[activePpu->opD] = (PpWord)activePpu->regA & Mask12;
    }

static void ppOpSOD(void)     // 37
    {
    activePpu->regA                = ppSubtract18(activePpu->mem[activePpu->opD] & Mask12, 1);
    activePpu->mem[activePpu->opD] = (PpWord)activePpu->regA & Mask12;
    }

static void ppOpLDI(void)     // 40
    {
    location        = activePpu->mem[activePpu->opD] & Mask12;
    activePpu->regA = activePpu->mem[location] & Mask12;
    }

static void ppOpADI(void)     // 41
    {
    location        = activePpu->mem[activePpu->opD] & Mask12;
    activePpu->regA = ppAdd18(activePpu->regA, activePpu->mem[location] & Mask12);
    }

static void ppOpSBI(void)     // 42
    {
    location        = activePpu->mem[activePpu->opD] & Mask12;
    activePpu->regA = ppSubtract18(activePpu->regA, activePpu->mem[location] & Mask12);
    }

static void ppOpLMI(void)     // 43
    {
    location         = activePpu->mem[activePpu->opD] & Mask12;
    activePpu->regA ^= activePpu->mem[location] & Mask12;
    }

static void ppOpSTI(void)     // 44
    {
    location = activePpu->mem[activePpu->opD] & Mask12;
    activePpu->mem[location] = (PpWord)activePpu->regA & Mask12;
    }

static void ppOpRAI(void)     // 45
    {
    location                 = activePpu->mem[activePpu->opD] & Mask12;
    activePpu->regA          = ppAdd18(activePpu->regA, activePpu->mem[location] & Mask12);
    activePpu->mem[location] = (PpWord)activePpu->regA & Mask12;
    }

static void ppOpAOI(void)     // 46
    {
    location                 = activePpu->mem[activePpu->opD] & Mask12;
    activePpu->regA          = ppAdd18(activePpu->mem[location] & Mask12, 1);
    activePpu->mem[location] = (PpWord)activePpu->regA & Mask12;
    }

static void ppOpSOI(void)     // 47
    {
    location                 = activePpu->mem[activePpu->opD] & Mask12;
    activePpu->regA          = ppSubtract18(activePpu->mem[location] & Mask12, 1);
    activePpu->mem[location] = (PpWord)activePpu->regA & Mask12;
    }

static void ppOpLDM(void)     // 50
    {
    IndexLocation;
    activePpu->regA = activePpu->mem[location] & Mask12;
    }

static void ppOpADM(void)     // 51
    {
    IndexLocation;
    activePpu->regA = ppAdd18(activePpu->regA, activePpu->mem[location] & Mask12);
    }

static void ppOpSBM(void)     // 52
    {
    IndexLocation;
    activePpu->regA = ppSubtract18(activePpu->regA, activePpu->mem[location] & Mask12);
    }

static void ppOpLMM(void)     // 53
    {
    IndexLocation;
    activePpu->regA ^= activePpu->mem[location] & Mask12;
    }

static void ppOpSTM(void)     // 54
    {
    IndexLocation;
    activePpu->mem[location] = (PpWord)activePpu->regA & Mask12;
    }

static void ppOpRAM(void)     // 55
    {
    IndexLocation;
    activePpu->regA          = ppAdd18(activePpu->regA, activePpu->mem[location] & Mask12);
    activePpu->mem[location] = (PpWord)activePpu->regA & Mask12;
    }

static void ppOpAOM(void)     // 56
    {
    IndexLocation;
    activePpu->regA          = ppAdd18(activePpu->mem[location] & Mask12, 1);
    activePpu->mem[location] = (PpWord)activePpu->regA & Mask12;
    }

static void ppOpSOM(void)     // 57
    {
    IndexLocation;
    activePpu->regA          = ppSubtract18(activePpu->mem[location] & Mask12, 1);
    activePpu->mem[location] = (PpWord)activePpu->regA & Mask12;
    }

static void ppOpCRD(void)     // 60
    {
    u32    address;
    CpWord data;

    if (((activePpu->regA & Sign18) != 0) && ((features & HasRelocationReg) != 0))
        {
        address = activePpu->regR + (activePpu->regA & Mask17);
        }
    else
        {
        address = activePpu->regA & Mask18;
        }
    cpuPpReadMem(address, &data);
    activePpu->mem[activePpu->opD]     = (PpWord)((data >> 48) & Mask12);
    activePpu->mem[activePpu->opD + 1] = (PpWord)((data >> 36) & Mask12);
    activePpu->mem[activePpu->opD + 2] = (PpWord)((data >> 24) & Mask12);
    activePpu->mem[activePpu->opD + 3] = (PpWord)((data >> 12) & Mask12);
    activePpu->mem[activePpu->opD + 4] = (PpWord)(data & Mask12);

#if CcDebug == 1
    traceCmWord(data);
#endif
    }

static void ppOpCRM(void)     // 61
    {
    u32    address;
    CpWord data;

    if (!activePpu->busy)
        {
        activePpu->regQ   = activePpu->mem[activePpu->opD] & Mask12;
        activePpu->busy   = TRUE;
        activePpu->mem[0] = activePpu->regP;
        activePpu->regP   = activePpu->mem[activePpu->regP] & Mask12;
        }

    if (((activePpu->regA & Sign18) != 0) && ((features & HasRelocationReg) != 0))
        {
        address = activePpu->regR + (activePpu->regA & Mask17);
        }
    else
        {
        address = activePpu->regA & Mask18;
        }
    cpuPpReadMem(address, &data);
    activePpu->mem[activePpu->regP] = (PpWord)((data >> 48) & Mask12);
    PpIncrement(activePpu->regP);

    activePpu->mem[activePpu->regP] = (PpWord)((data >> 36) & Mask12);
    PpIncrement(activePpu->regP);

    activePpu->mem[activePpu->regP] = (PpWord)((data >> 24) & Mask12);
    PpIncrement(activePpu->regP);

    activePpu->mem[activePpu->regP] = (PpWord)((data >> 12) & Mask12);
    PpIncrement(activePpu->regP);

    activePpu->mem[activePpu->regP] = (PpWord)(data & Mask12);
    PpIncrement(activePpu->regP);

    activePpu->regA = (activePpu->regA + 1) & Mask18;
    PpDecrement(activePpu->regQ);

    if (activePpu->regQ == 0)
        {
        activePpu->regP = activePpu->mem[0];
        PpIncrement(activePpu->regP);
        activePpu->busy = FALSE;
        }

#if CcDebug == 1
    traceCmWord(data);
#endif
    }

static void ppOpCWD(void)     // 62
    {
    u32    address;
    CpWord data;

    if (((activePpu->regA & Sign18) != 0) && ((features & HasRelocationReg) != 0))
        {
        address = activePpu->regR + (activePpu->regA & Mask17);
        }
    else
        {
        address = activePpu->regA & Mask18;
        }
#if DEBUG_CM_WRITE
    ppValidateCmWrite("CWD", address, data);
#endif
    if (ppCheckOsBounds(address))
        {
        if (activePpu->isStopEnabled)
            {
            activePpu->isStopped = TRUE;
            PpDecrement(activePpu->regP);
            }
        }
    else
        {
        data = ((CpWord)(activePpu->mem[activePpu->opD] & Mask12) << 48)
             | ((CpWord)(activePpu->mem[activePpu->opD + 1] & Mask12) << 36)
             | ((CpWord)(activePpu->mem[activePpu->opD + 2] & Mask12) << 24)
             | ((CpWord)(activePpu->mem[activePpu->opD + 3] & Mask12) << 12)
             | (CpWord)(activePpu->mem[activePpu->opD + 4] & Mask12);
        cpuPpWriteMem(address, data);

#if CcDebug == 1
        traceCmWord(data);
#endif
        }
    }

static void ppOpCWM(void)     // 63
    {
    u32    address;
    CpWord data;

    if (!activePpu->busy)
        {
        activePpu->regQ   = activePpu->mem[activePpu->opD] & Mask12;
        activePpu->busy   = TRUE;
        activePpu->mem[0] = activePpu->regP;
        activePpu->regP   = activePpu->mem[activePpu->regP] & Mask12;
        }
    data = (CpWord)(activePpu->mem[activePpu->regP] & Mask12) << 48;
    PpIncrement(activePpu->regP);

    data |= (CpWord)(activePpu->mem[activePpu->regP] & Mask12) << 36;
    PpIncrement(activePpu->regP);

    data |= (CpWord)(activePpu->mem[activePpu->regP] & Mask12) << 24;
    PpIncrement(activePpu->regP);

    data |= (CpWord)(activePpu->mem[activePpu->regP] & Mask12) << 12;
    PpIncrement(activePpu->regP);

    data |= activePpu->mem[activePpu->regP] & Mask12;
    PpIncrement(activePpu->regP);

    if (((activePpu->regA & Sign18) != 0) && ((features & HasRelocationReg) != 0))
        {
        address = activePpu->regR + (activePpu->regA & Mask17);
        }
    else
        {
        address = activePpu->regA & Mask18;
        }
#if DEBUG_CM_WRITE
    ppValidateCmWrite("CWM", address, data);
#endif
    if (ppCheckOsBounds(address))
        {
        if (activePpu->isStopEnabled)
            {
            activePpu->isStopped = TRUE;
            PpDecrement(activePpu->regP);
            PpDecrement(activePpu->regP);
            PpDecrement(activePpu->regP);
            PpDecrement(activePpu->regP);
            PpDecrement(activePpu->regP);
            return;
            }
        }
    else
        {
        cpuPpWriteMem(address, data);

#if CcDebug == 1
        traceCmWord(data);
#endif
        }
    activePpu->regA = (activePpu->regA + 1) & Mask18;
    PpDecrement(activePpu->regQ);

    if (activePpu->regQ == 0)
        {
        activePpu->regP = activePpu->mem[0];
        PpIncrement(activePpu->regP);
        activePpu->busy = FALSE;
        }
    }

static void ppOpAJM(void)     // 64
    {
    location  = activePpu->mem[activePpu->regP] & Mask12;
    PpIncrement(activePpu->regP);

    if (((activePpu->opD & 040) != 0)
        && ((features & HasChannelFlag) != 0))
        {
        /*
        **  SCF.
        */
        activePpu->opD &= 037;
        if (activePpu->opD < channelCount)
            {
            if (channel[activePpu->opD].flag)
                {
                activePpu->regP = location;
                }
            else
                {
                channel[activePpu->opD].flag = TRUE;
                }
            }

        return;
        }

    activePpu->opD &= 037;
    if (activePpu->opD < channelCount)
        {
        activeChannel = channel + activePpu->opD;
        channelCheckIfActive();
        if (activeChannel->active)
            {
            activePpu->regP = location;
            }
        }
    }

static void ppOpIJM(void)     // 65
    {
    location  = activePpu->mem[activePpu->regP] & Mask12;
    PpIncrement(activePpu->regP);

    if (((activePpu->opD & 040) != 0)
        && ((features & HasChannelFlag) != 0))
        {
        /*
        **  CCF.
        */
        activePpu->opD &= 037;
        if (activePpu->opD < channelCount)
            {
            channel[activePpu->opD].flag = FALSE;
            }

        return;
        }

    activePpu->opD &= 037;
    if (activePpu->opD >= channelCount)
        {
        activePpu->regP = location;
        }
    else
        {
        activeChannel = channel + activePpu->opD;
        channelCheckIfActive();
        if (!activeChannel->active)
            {
            activePpu->regP = location;
            }
        }
    }

static void ppOpFJM(void)     // 66
    {
    location  = activePpu->mem[activePpu->regP] & Mask12;
    PpIncrement(activePpu->regP);

    if (((activePpu->opD & 040) != 0)
        && ((features & HasErrorFlag) != 0))
        {
        /*
        **  SFM - we never have errors, so this is just a pass.
        */
        return;
        }

    activePpu->opD &= 037;
    if (activePpu->opD < channelCount)
        {
        activeChannel = channel + activePpu->opD;
        channelIo();
        channelCheckIfFull();
        if (activeChannel->full)
            {
            activePpu->regP = location;
            }
        }
    }

static void ppOpEJM(void)     // 67
    {
    location  = activePpu->mem[activePpu->regP] & Mask12;
    PpIncrement(activePpu->regP);

    if (((activePpu->opD & 040) != 0)
        && ((features & HasErrorFlag) != 0))
        {
        /*
        **  CFM - we never have errors, so we always jump.
        */
        activePpu->opD &= 037;
        if (activePpu->opD < channelCount)
            {
            activePpu->regP = location;
            }

        return;
        }

    activePpu->opD &= 037;
    if (activePpu->opD >= channelCount)
        {
        activePpu->regP = location;
        }
    else
        {
        activeChannel = channel + activePpu->opD;
        channelIo();
        channelCheckIfFull();
        if (!activeChannel->full)
            {
            activePpu->regP = location;
            }
        }
    }

static void ppOpIAN(void)     // 70
    {
    if (!activePpu->busy)
        {
        activeChannel->delayStatus = 0;
        }

    noHang          = (activePpu->opD & 040) != 0;
    activeChannel   = channel + (activePpu->opD & 037);
    activePpu->busy = TRUE;

    channelCheckIfActive();
    if (!activeChannel->active && (activeChannel->id != ChClock))
        {
        if (noHang)
            {
            activePpu->regA = 0;
            activePpu->busy = FALSE;
            }

        return;
        }

    channelCheckIfFull();
    if (!activeChannel->full)
        {
        /*
        **  Handle possible input.
        */
        channelIo();
        }

    if (activeChannel->full || (activeChannel->id == ChClock))
        {
        /*
        **  Handle input (note that the clock channel has always data pending,
        **  but appears full on some models, empty on others).
        */
        channelIn();
        channelSetEmpty();
        if (isCyber180)
            {
            activePpu->regA = activeChannel->data & Mask16;
            }
        else
            {
            activePpu->regA = activeChannel->data & Mask12;
            }
        activeChannel->inputPending = FALSE;
        if (activeChannel->discAfterInput)
            {
            activeChannel->discAfterInput  = FALSE;
            activeChannel->delayDisconnect = 0;
            activeChannel->active          = FALSE;
            activeChannel->ioDevice        = NULL;
            }

        activePpu->busy = FALSE;
        }
    }

static void ppOpIAM(void)     // 71
    {
    activeChannel = channel + (activePpu->opD & 037);
    if (!activePpu->busy)
        {
        activePpu->busy            = TRUE;
        activePpu->mem[0]          = activePpu->regP;
        activePpu->regP            = activePpu->mem[activePpu->regP] & Mask12;
        activeChannel->delayStatus = 0;
        }

    channelCheckIfActive();
    if (!activeChannel->active)
        {
        /*
        **  Disconnect device except for hardwired devices.
        */
        if (!activeChannel->hardwired)
            {
            activeChannel->ioDevice = NULL;
            }

        /*
        **  Channel becomes empty (must not call channelSetEmpty(), otherwise we
        **  get a spurious empty pulse).
        */
        activeChannel->full = FALSE;

        /*
        **  Terminate transfer and set next location to zero.
        */
        activePpu->mem[activePpu->regP] = 0;
        activePpu->regP = activePpu->mem[0];
        PpIncrement(activePpu->regP);
        activePpu->busy   = FALSE;
        activePpu->isLoad = FALSE;

        return;
        }

    channelCheckIfFull();
    if (!activeChannel->full)
        {
        /*
        **  Handle possible input.
        */
        channelIo();
        }

    if (activeChannel->full || (activeChannel->id == ChClock))
        {
        /*
        **  Handle input (note that the clock channel has always data pending,
        **  but appears full on some models, empty on others).
        */
        channelIn();
        channelSetEmpty();
        if (isCyber180)
            {
            activePpu->mem[activePpu->regP] = activeChannel->data & Mask16;
            }
        else
            {
            activePpu->mem[activePpu->regP] = activeChannel->data & Mask12;
            }
        activePpu->regP             = (activePpu->regP + 1) & Mask12;
        activePpu->regA             = (activePpu->regA - 1) & Mask18;
        activeChannel->inputPending = FALSE;

        if (activeChannel->discAfterInput)
            {
            activeChannel->discAfterInput  = FALSE;
            activeChannel->delayDisconnect = 0;
            activeChannel->active          = FALSE;
            activeChannel->ioDevice        = NULL;
            if (activePpu->regA != 0)
                {
                activePpu->mem[activePpu->regP] = 0;
                }
            activePpu->regP = activePpu->mem[0];
            PpIncrement(activePpu->regP);
            activePpu->busy = FALSE;
            }
        else if (activePpu->regA == 0)
            {
            activePpu->regP = activePpu->mem[0];
            PpIncrement(activePpu->regP);
            activePpu->busy   = FALSE;
            activePpu->isLoad = FALSE;
            }
        }
    }

static void ppOpOAN(void)     // 72
    {
    if (!activePpu->busy)
        {
        activeChannel->delayStatus = 0;
        }

    noHang          = (activePpu->opD & 040) != 0;
    activeChannel   = channel + (activePpu->opD & 037);
    activePpu->busy = TRUE;

    channelCheckIfActive();
    if (!activeChannel->active)
        {
        if (noHang)
            {
            activePpu->busy = FALSE;
            }

        return;
        }

    channelCheckIfFull();
    if (!activeChannel->full)
        {
        if (isCyber180)
            {
            activeChannel->data = (PpWord)activePpu->regA & Mask16;
            }
        else
            {
            activeChannel->data = (PpWord)activePpu->regA & Mask12;
            }
        channelOut();
        channelSetFull();
        activePpu->busy = FALSE;
        }

    /*
    **  Handle possible output.
    */
    channelIo();
    }

static void ppOpOAM(void)     // 73
    {
    activeChannel = channel + (activePpu->opD & 037);
    if (!activePpu->busy)
        {
        activePpu->busy            = TRUE;
        activePpu->mem[0]          = activePpu->regP;
        activePpu->regP            = activePpu->mem[activePpu->regP] & Mask12;
        activeChannel->delayStatus = 0;
        }

    channelCheckIfActive();
    if (!activeChannel->active)
        {
        /*
        **  Disconnect device except for hardwired devices.
        */
        if (!activeChannel->hardwired)
            {
            activeChannel->ioDevice = NULL;
            }

        /*
        **  Channel becomes empty (must not call channelSetEmpty(), otherwise we
        **  get a spurious empty pulse).
        */
        activeChannel->full = FALSE;

        /*
        **  Terminate transfer.
        */
        activePpu->regP = activePpu->mem[0];
        PpIncrement(activePpu->regP);
        activePpu->busy = FALSE;

        return;
        }

    channelCheckIfFull();
    if (!activeChannel->full)
        {
        if (isCyber180)
            {
            activeChannel->data = activePpu->mem[activePpu->regP] & Mask16;
            }
        else
            {
            activeChannel->data = activePpu->mem[activePpu->regP] & Mask12;
            }
        activePpu->regP = (activePpu->regP + 1) & Mask12;
        activePpu->regA = (activePpu->regA - 1) & Mask18;
        channelOut();
        channelSetFull();

        if (activePpu->regA == 0)
            {
            activePpu->regP = activePpu->mem[0];
            PpIncrement(activePpu->regP);
            activePpu->busy            = FALSE;
            activePpu->isDump          = FALSE;
            activeChannel->delayStatus = 0; // ensure last byte is written
            }
        }

    /*
    **  Handle possible output.
    */
    channelIo();
    }

static void ppOpACN(void)     // 74
    {
    noHang        = (activePpu->opD & 040) != 0;
    activeChannel = channel + (activePpu->opD & 037);

    channelCheckIfActive();
    if (activeChannel->active)
        {
        if (!noHang)
            {
            activePpu->busy = TRUE;
            }

        return;
        }

    channelActivate();
    activePpu->busy = FALSE;
    }

static void ppOpDCN(void)     // 75
    {
    noHang        = (activePpu->opD & 040) != 0;
    activeChannel = channel + (activePpu->opD & 037);

    /*
    **  RTC, Interlock and S/C register channel can not be deactivated.
    */
    if (activeChannel->id == ChClock)
        {
        return;
        }

    if ((activeChannel->id == ChInterlock) && ((features & HasInterlockReg) != 0))
        {
        return;
        }

    if ((activeChannel->id == ChStatusAndControl) && ((features & HasStatusAndControlReg) != 0))
        {
        return;
        }

    channelCheckIfActive();
    if (!activeChannel->active)
        {
        if (!noHang)
            {
            activePpu->busy = TRUE;
            }

        return;
        }

    channelDisconnect();
    activePpu->busy = FALSE;
    }

static void ppOpFAN(void)     // 76
    {
    noHang        = (activePpu->opD & 040) != 0;
    activeChannel = channel + (activePpu->opD & 037);

    /*
    **  Interlock register channel ignores functions.
    */
    if ((activeChannel->id == ChInterlock) && ((features & HasInterlockReg) != 0))
        {
        return;
        }

    channelCheckIfActive();
    if (activeChannel->active)
        {
        if (!noHang)
            {
            activePpu->busy = TRUE;
            }

        return;
        }
    if (isCyber180)
        {
        channelFunction((PpWord)(activePpu->regA & Mask16));
        }
    else
        {
        channelFunction((PpWord)(activePpu->regA & Mask12));
        }
    activePpu->busy = FALSE;
    }

static void ppOpFNC(void)     // 77
    {
    noHang        = (activePpu->opD & 040) != 0;
    activeChannel = channel + (activePpu->opD & 037);

    /*
    **  Interlock register channel ignores functions.
    */
    if ((activeChannel->id == ChInterlock) && ((features & HasInterlockReg) != 0))
        {
        return;
        }

    channelCheckIfActive();
    if (activeChannel->active)
        {
        if (!noHang)
            {
            activePpu->busy = TRUE;
            }

        return;
        }

    channelFunction((PpWord)(activePpu->mem[activePpu->regP] & Mask12));
    PpIncrement(activePpu->regP);
    activePpu->busy = FALSE;
    }

static void ppOpRDSL(void)    // 1000
    {
    u32    address;
    CpWord cmData;
    CpWord ppData;

    if (((activePpu->regA & Sign18) != 0) && ((features & HasRelocationReg) != 0))
        {
        address = activePpu->regR + (activePpu->regA & Mask17);
        }
    else
        {
        address = activePpu->regA & Mask18;
        }
    if (ppCheckOsBounds(address))
        {
        if (activePpu->isStopEnabled)
            {
            activePpu->isStopped = TRUE;
            PpDecrement(activePpu->regP);
            }
        }
    else
        {
        ppData = (((CpWord)activePpu->mem[activePpu->opD]) << 48)
               | (((CpWord)activePpu->mem[activePpu->opD + 1]) << 32)
               | (((CpWord)activePpu->mem[activePpu->opD + 2]) << 16)
               | ((CpWord)activePpu->mem[activePpu->opD + 3]);
        cpuAcquireMemoryMutex();
        cpu180PpReadMem(address, &cmData);
        cpu180PpWriteMem(address, cmData | ppData);
        cpuReleaseMemoryMutex();
        activePpu->mem[activePpu->opD]     = (PpWord)((cmData >> 48) & Mask16);
        activePpu->mem[activePpu->opD + 1] = (PpWord)((cmData >> 32) & Mask16);
        activePpu->mem[activePpu->opD + 2] = (PpWord)((cmData >> 16) & Mask16);
        activePpu->mem[activePpu->opD + 3] = (PpWord)((cmData) & Mask16);

#if CcDebug == 1
        traceCmWord64(cmData | ppData);
#endif
        }
    }

static void ppOpRDCL(void)    // 1001
    {
    u32    address;
    CpWord cmData;
    CpWord ppData;

    if (((activePpu->regA & Sign18) != 0) && ((features & HasRelocationReg) != 0))
        {
        address = activePpu->regR + (activePpu->regA & Mask17);
        }
    else
        {
        address = activePpu->regA & Mask18;
        }
    if (ppCheckOsBounds(address))
        {
        if (activePpu->isStopEnabled)
            {
            activePpu->isStopped = TRUE;
            PpDecrement(activePpu->regP);
            }
        }
    else
        {
        ppData = (((CpWord)activePpu->mem[activePpu->opD] & Mask12) << 48)
               | (((CpWord)activePpu->mem[activePpu->opD + 1] & Mask12) << 32)
               | (((CpWord)activePpu->mem[activePpu->opD + 2] & Mask12) << 16)
               | ((CpWord)activePpu->mem[activePpu->opD + 3] & Mask12);
        cpuAcquireMemoryMutex();
        cpu180PpReadMem(address, &cmData);
        cpu180PpWriteMem(address, cmData & ppData);
        cpuReleaseMemoryMutex();
        activePpu->mem[activePpu->opD]     = (PpWord)((cmData >> 48) & Mask16);
        activePpu->mem[activePpu->opD + 1] = (PpWord)((cmData >> 32) & Mask16);
        activePpu->mem[activePpu->opD + 2] = (PpWord)((cmData >> 16) & Mask16);
        activePpu->mem[activePpu->opD + 3] = (PpWord)((cmData) & Mask16);

#if CcDebug == 1
        traceCmWord64(cmData & ppData);
#endif
        }
    }

static void ppOpLPDL(void)    // 1022
    {
    activePpu->regA &= activePpu->mem[activePpu->opD] & Mask16;
    }

static void ppOpLPIL(void)    // 1023
    {
    location         = activePpu->mem[activePpu->opD] & Mask12;
    activePpu->regA &= activePpu->mem[location] & Mask16;
    }

static void ppOpLPML(void)    // 1024
    {
    IndexLocation;
    activePpu->regA &= activePpu->mem[location] & Mask16;
    }

static void ppOpINPN(void)    // 1026
    {
/*DELETE*/if ((activePpu->opD & 1) == 0) {fprintf(stderr,"PP%02o INPN %o\n",activePpu->id < 10 ? activePpu->id : (activePpu->id - 10) + 020,activePpu->opD);fflush(stderr);}
    if ((activePpu->opD & 1) != 0) // memory port 0 selected
        {
        cpus180[0].regMcr |= 0x0080; // set MCR56
        }
    if ((activePpu->opD & 4) != 0 && cpuCount > 1) // memory port 2 selected
        {
        cpus180[1].regMcr |= 0x0080;
        }
#if DEBUG
    else
        {
        fprintf(ppLog, "  PP%02o Unexpected memory port specified: INPN %o\n",
            activePpu->id < 10 ? activePpu->id : (activePpu->id - 10) + 020, activePpu->opD);
        }
#endif
    }

static void ppOpLDDL(void)    // 1030
    {
    activePpu->regA = activePpu->mem[activePpu->opD] & Mask16;
    }

static void ppOpADDL(void)    // 1031
    {
    activePpu->regA = ppAdd18(activePpu->regA, activePpu->mem[activePpu->opD] & Mask16);
    }

static void ppOpSBDL(void)    // 1032
    {
    activePpu->regA = ppSubtract18(activePpu->regA, activePpu->mem[activePpu->opD] & Mask16);
    }

static void ppOpLMDL(void)    // 1033
    {
    activePpu->regA ^= activePpu->mem[activePpu->opD] & Mask16;
    }

static void ppOpSTDL(void)    // 1034
    {
    activePpu->mem[activePpu->opD] = (PpWord)activePpu->regA & Mask16;
    }

static void ppOpRADL(void)    // 1035
    {
    activePpu->regA                = ppAdd18(activePpu->regA, activePpu->mem[activePpu->opD] & Mask16);
    activePpu->mem[activePpu->opD] = (PpWord)activePpu->regA & Mask16;
    }

static void ppOpAODL(void)    // 1036
    {
    activePpu->regA                = ppAdd18(activePpu->mem[activePpu->opD] & Mask16, 1);
    activePpu->mem[activePpu->opD] = (PpWord)activePpu->regA & Mask16;
    }

static void ppOpSODL(void)    // 1037
    {
    activePpu->regA                = ppSubtract18(activePpu->mem[activePpu->opD] & Mask16, 1);
    activePpu->mem[activePpu->opD] = (PpWord)activePpu->regA & Mask16;
    }

static void ppOpLDIL(void)    // 1040
    {
    location        = activePpu->mem[activePpu->opD] & Mask12;
    activePpu->regA = activePpu->mem[location] & Mask16;
    }

static void ppOpADIL(void)    // 1041
    {
    location        = activePpu->mem[activePpu->opD] & Mask12;
    activePpu->regA = ppAdd18(activePpu->regA, activePpu->mem[location] & Mask16);
    }

static void ppOpSBIL(void)    // 1042
    {
    location        = activePpu->mem[activePpu->opD] & Mask12;
    activePpu->regA = ppSubtract18(activePpu->regA, activePpu->mem[location] & Mask16);
    }

static void ppOpLMIL(void)    // 1043
    {
    location         = activePpu->mem[activePpu->opD] & Mask12;
    activePpu->regA ^= activePpu->mem[location] & Mask16;
    }

static void ppOpSTIL(void)    // 1044
    {
    location = activePpu->mem[activePpu->opD] & Mask12;
    activePpu->mem[location] = (PpWord)activePpu->regA & Mask16;
    }

static void ppOpRAIL(void)    // 1045
    {
    location                 = activePpu->mem[activePpu->opD] & Mask12;
    activePpu->regA          = ppAdd18(activePpu->regA, activePpu->mem[location] & Mask16);
    activePpu->mem[location] = (PpWord)activePpu->regA & Mask16;
    }

static void ppOpAOIL(void)    // 1046
    {
    location                 = activePpu->mem[activePpu->opD] & Mask12;
    activePpu->regA          = ppAdd18(activePpu->mem[location] & Mask16, 1);
    activePpu->mem[location] = (PpWord)activePpu->regA & Mask16;
    }

static void ppOpSOIL(void)    // 1047
    {
    location                 = activePpu->mem[activePpu->opD] & Mask12;
    activePpu->regA          = ppSubtract18(activePpu->mem[location] & Mask16, 1);
    activePpu->mem[location] = (PpWord)activePpu->regA & Mask16;
    }

static void ppOpLDML(void)    // 1050
    {
    IndexLocation;
    activePpu->regA = activePpu->mem[location] & Mask16;
    }

static void ppOpADML(void)    // 1051
    {
    IndexLocation;
    activePpu->regA = ppAdd18(activePpu->regA, activePpu->mem[location] & Mask16);
    }

static void ppOpSBML(void)    // 1052
    {
    IndexLocation;
    activePpu->regA = ppSubtract18(activePpu->regA, activePpu->mem[location] & Mask16);
    }

static void ppOpLMML(void)    // 1053
    {
    IndexLocation;
    activePpu->regA ^= activePpu->mem[location] & Mask16;
    }

static void ppOpSTML(void)    // 1054
    {
    IndexLocation;
    activePpu->mem[location] = (PpWord)activePpu->regA & Mask16;
    }

static void ppOpRAML(void)    // 1055
    {
    IndexLocation;
    activePpu->regA          = ppAdd18(activePpu->regA, activePpu->mem[location] & Mask16);
    activePpu->mem[location] = (PpWord)activePpu->regA & Mask16;
    }

static void ppOpAOML(void)    // 1056
    {
    IndexLocation;
    activePpu->regA          = ppAdd18(activePpu->mem[location] & Mask16, 1);
    activePpu->mem[location] = (PpWord)activePpu->regA & Mask16;
    }

static void ppOpSOML(void)    // 1057
    {
    IndexLocation;
    activePpu->regA          = ppSubtract18(activePpu->mem[location] & Mask16, 1);
    activePpu->mem[location] = (PpWord)activePpu->regA & Mask16;
    }

static void ppOpCRDL(void)    // 1060
    {
    u32    address;
    CpWord data;

    if (((activePpu->regA & Sign18) != 0) && ((features & HasRelocationReg) != 0))
        {
        address = activePpu->regR + (activePpu->regA & Mask17);
        }
    else
        {
        address = activePpu->regA & Mask18;
        }
    cpu180PpReadMem(address, &data);
    activePpu->mem[activePpu->opD]     = (PpWord)((data >> 48) & Mask16);
    activePpu->mem[activePpu->opD + 1] = (PpWord)((data >> 32) & Mask16);
    activePpu->mem[activePpu->opD + 2] = (PpWord)((data >> 16) & Mask16);
    activePpu->mem[activePpu->opD + 3] = (PpWord)((data) & Mask16);

#if CcDebug == 1
    traceCmWord64(data);
#endif
    }

static void ppOpCRML(void)    // 1061
    {
    u32    address;
    CpWord data;

    if (!activePpu->busy)
        {
        activePpu->regQ   = activePpu->mem[activePpu->opD] & Mask12;
        activePpu->busy   = TRUE;
        activePpu->mem[0] = activePpu->regP;
        activePpu->regP   = activePpu->mem[activePpu->regP] & Mask12;
        }

    if (((activePpu->regA & Sign18) != 0) && ((features & HasRelocationReg) != 0))
        {
        address = activePpu->regR + (activePpu->regA & Mask17);
        }
    else
        {
        address = activePpu->regA & Mask18;
        }
    cpu180PpReadMem(address, &data);
    activePpu->mem[activePpu->regP] = (PpWord)((data >> 48) & Mask16);
    PpIncrement(activePpu->regP);

    activePpu->mem[activePpu->regP] = (PpWord)((data >> 32) & Mask16);
    PpIncrement(activePpu->regP);

    activePpu->mem[activePpu->regP] = (PpWord)((data >> 16) & Mask16);
    PpIncrement(activePpu->regP);

    activePpu->mem[activePpu->regP] = (PpWord)((data) & Mask16);
    PpIncrement(activePpu->regP);

    activePpu->regA = (activePpu->regA + 1) & Mask18;
    PpDecrement(activePpu->regQ);

    if (activePpu->regQ == 0)
        {
        activePpu->regP = activePpu->mem[0];
        PpIncrement(activePpu->regP);
        activePpu->busy = FALSE;
        }

#if CcDebug == 1
    traceCmWord64(data);
#endif
    }

static void ppOpCWDL(void)    // 1062
    {
    u32    address;
    CpWord data;

    if (((activePpu->regA & Sign18) != 0) && ((features & HasRelocationReg) != 0))
        {
        address = activePpu->regR + (activePpu->regA & Mask17);
        }
    else
        {
        address = activePpu->regA & Mask18;
        }
    if (ppCheckOsBounds(address))
        {
        if (activePpu->isStopEnabled)
            {
            activePpu->isStopped = TRUE;
            PpDecrement(activePpu->regP);
            }
        }
    else
        {
        data = ((CpWord)(activePpu->mem[activePpu->opD] & Mask16) << 48)
             | ((CpWord)(activePpu->mem[activePpu->opD + 1] & Mask16) << 32)
             | ((CpWord)(activePpu->mem[activePpu->opD + 2] & Mask16) << 16)
             | (CpWord)(activePpu->mem[activePpu->opD + 3] & Mask16);
        cpu180PpWriteMem(address, data);

#if CcDebug == 1
        traceCmWord64(data);
#endif
        }
    }

static void ppOpCWML(void)    // 1063
    {
    u32    address;
    CpWord data;

    if (!activePpu->busy)
        {
        activePpu->regQ   = activePpu->mem[activePpu->opD] & Mask12;
        activePpu->busy   = TRUE;
        activePpu->mem[0] = activePpu->regP;
        activePpu->regP   = activePpu->mem[activePpu->regP] & Mask12;
        }
    data = (CpWord)(activePpu->mem[activePpu->regP] & Mask16) << 48;
    PpIncrement(activePpu->regP);

    data |= (CpWord)(activePpu->mem[activePpu->regP] & Mask16) << 32;
    PpIncrement(activePpu->regP);

    data |= (CpWord)(activePpu->mem[activePpu->regP] & Mask16) << 16;
    PpIncrement(activePpu->regP);

    data |= activePpu->mem[activePpu->regP] & Mask16;
    PpIncrement(activePpu->regP);

    if (((activePpu->regA & Sign18) != 0) && ((features & HasRelocationReg) != 0))
        {
        address = activePpu->regR + (activePpu->regA & Mask17);
        }
    else
        {
        address = activePpu->regA & Mask18;
        }
    if (ppCheckOsBounds(address))
        {
        if (activePpu->isStopEnabled)
            {
            activePpu->isStopped = TRUE;
            PpDecrement(activePpu->regP);
            PpDecrement(activePpu->regP);
            PpDecrement(activePpu->regP);
            PpDecrement(activePpu->regP);
            return;
            }
        }
    else
        {
        cpu180PpWriteMem(address, data);

#if CcDebug == 1
        traceCmWord64(data);
#endif
        }
    activePpu->regA = (activePpu->regA + 1) & Mask18;
    PpDecrement(activePpu->regQ);

    if (activePpu->regQ == 0)
        {
        activePpu->regP = activePpu->mem[0];
        PpIncrement(activePpu->regP);
        activePpu->busy = FALSE;
        }
    }

static void ppOpFSJM(void)    // 1064
    {
    location  = activePpu->mem[activePpu->regP] & Mask12;
    PpIncrement(activePpu->regP);

    activePpu->opD &= 037;
    if (activePpu->opD < channelCount)
        {
        if (channel[activePpu->opD].flag)
            {
            activePpu->regP = location;
            }
        }
    }

static void ppOpFCJM(void)    // 1065
    {
    location  = activePpu->mem[activePpu->regP] & Mask12;
    PpIncrement(activePpu->regP);

    activePpu->opD &= 037;
    if (activePpu->opD < channelCount)
        {
        if (channel[activePpu->opD].flag == FALSE)
            {
            activePpu->regP = location;
            }
        }
    }

static void ppFlushIoBuf(void)
    {
    if (activePpu->ioBufIdx > 1)
        {
        activePpu->mem[activePpu->regP] = (PpWord)((activePpu->ioBuf[0] << 4) | (activePpu->ioBuf[1] >> 8));
        PpIncrement(activePpu->regP);

        if (activePpu->ioBufIdx > 2)
            {
            activePpu->mem[activePpu->regP] = (PpWord)(((activePpu->ioBuf[1] & Mask8) << 8) | (activePpu->ioBuf[2] >> 4));
            PpIncrement(activePpu->regP);

            if (activePpu->ioBufIdx > 3)
                {
                activePpu->mem[activePpu->regP] = (PpWord)(((activePpu->ioBuf[2] & Mask4) << 12) | activePpu->ioBuf[3]);
                PpIncrement(activePpu->regP);
                }
            }
        }
    }

static void ppResetIoBuf(void)
    {
    activePpu->ioBufIdx = 0;
    memset(activePpu->ioBuf, 0, sizeof(activePpu->ioBuf));
    }

static void ppOpIAPM(void)    // 1071
    {
    activeChannel = channel + (activePpu->opD & 037);
    if (!activePpu->busy)
        {
        activePpu->busy            = TRUE;
        activePpu->mem[0]          = activePpu->regP;
        activePpu->regP            = activePpu->mem[activePpu->regP] & Mask12;
        activeChannel->delayStatus = 0;
        ppResetIoBuf();
        }

    channelCheckIfActive();
    if (!activeChannel->active)
        {
        /*
        **  Disconnect device except for hardwired devices.
        */
        if (!activeChannel->hardwired)
            {
            activeChannel->ioDevice = NULL;
            }

        /*
        **  Channel becomes empty (must not call channelSetEmpty(), otherwise we
        **  get a spurious empty pulse).
        */
        activeChannel->full = FALSE;

        /*
        **  Terminate transfer and zero-fill 3-word block
        */
        activePpu->ioBufIdx = 4;
        ppFlushIoBuf();
        activePpu->regP = activePpu->mem[0];
        PpIncrement(activePpu->regP);
        activePpu->busy = FALSE;

        return;
        }

    channelCheckIfFull();
    if (!activeChannel->full)
        {
        /*
        **  Handle possible input.
        */
        channelIo();
        }

    if (activeChannel->full || (activeChannel->id == ChClock))
        {
        channelIn();
        channelSetEmpty();
        activePpu->ioBuf[activePpu->ioBufIdx++] = activeChannel->data & Mask12;
        if (activePpu->ioBufIdx > 3)
            {
            ppFlushIoBuf();
            ppResetIoBuf();
            }
        activePpu->regA             = (activePpu->regA - 1) & Mask18;
        activeChannel->inputPending = FALSE;

        if (activeChannel->discAfterInput)
            {
            if (activePpu->ioBufIdx > 0)
                {
                activePpu->ioBufIdx = 4; // zero-fill to next 3-word boundary
                ppFlushIoBuf();
                }
            activeChannel->discAfterInput  = FALSE;
            activeChannel->delayDisconnect = 0;
            activeChannel->active          = FALSE;
            activeChannel->ioDevice        = NULL;
            activePpu->regP = activePpu->mem[0];
            PpIncrement(activePpu->regP);
            activePpu->busy = FALSE;
            }
        else if (activePpu->regA == 0)
            {
            if (activePpu->ioBufIdx > 0)
                {
                ppFlushIoBuf();
                }
            activePpu->regP = activePpu->mem[0];
            PpIncrement(activePpu->regP);
            activePpu->busy = FALSE;
            }
        }
    }

static void ppOpOAPM(void)    // 1073
    {
    activeChannel = channel + (activePpu->opD & 037);
    if (!activePpu->busy)
        {
        activePpu->busy            = TRUE;
        activePpu->mem[0]          = activePpu->regP;
        activePpu->regP            = activePpu->mem[activePpu->regP] & Mask12;
        activeChannel->delayStatus = 0;
        activePpu->ioBufIdx        = 4;
        }

    channelCheckIfActive();
    if (!activeChannel->active)
        {
        /*
        **  Disconnect device except for hardwired devices.
        */
        if (!activeChannel->hardwired)
            {
            activeChannel->ioDevice = NULL;
            }

        /*
        **  Channel becomes empty (must not call channelSetEmpty(), otherwise we
        **  get a spurious empty pulse).
        */
        activeChannel->full = FALSE;

        /*
        **  Terminate transfer.
        */
        activePpu->regP = activePpu->mem[0];
        PpIncrement(activePpu->regP);
        activePpu->busy = FALSE;

        return;
        }

    channelCheckIfFull();
    if (!activeChannel->full)
        {
        if (activePpu->ioBufIdx > 3)
            {
            activePpu->ioBuf[0]  = activePpu->mem[activePpu->regP] >> 4;
            activePpu->ioBuf[1]  = (PpWord)((activePpu->mem[activePpu->regP] & Mask4) << 8);
            PpIncrement(activePpu->regP);
            activePpu->ioBuf[1] |= activePpu->mem[activePpu->regP] >> 8;
            activePpu->ioBuf[2]  = (PpWord)((activePpu->mem[activePpu->regP] & Mask8) << 4);
            PpIncrement(activePpu->regP);
            activePpu->ioBuf[2] |= activePpu->mem[activePpu->regP] >> 12;
            activePpu->ioBuf[3]  = activePpu->mem[activePpu->regP] & Mask12;
            PpIncrement(activePpu->regP);
            activePpu->ioBufIdx = 0;
            }
        activeChannel->data = activePpu->ioBuf[activePpu->ioBufIdx++];
        activePpu->regA     = (activePpu->regA - 1) & Mask18;
        channelOut();
        channelSetFull();

        if (activePpu->regA == 0)
            {
            activePpu->regP = activePpu->mem[0];
            PpIncrement(activePpu->regP);
            activePpu->busy            = FALSE;
            activeChannel->delayStatus = 0; // ensure last byte is written
            }
        }

    /*
    **  Handle possible output.
    */
    channelIo();
    }

#if DEBUG_CM_WRITE

/*--------------------------------------------------------------------------
**  Purpose:        Check that a write to CM appears to be legitimate.
**                  This code is very specific to OS type and version, and
**                  applies only to NOS 2.8.7 initially.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
#define CPA     00200
#define FLSW    023

static char *ppMonitored[] =
    {
    "1AJ", "LDR", "LDQ", "TCS",
    NULL
    };

static void ppValidateCmWrite(char *inst, u32 address, CpWord data)
    {
    u8     cpn;
    u32    fl;
    int    i;
    u32    irAddress;
    u32    nfl;
    char   *np;
    char   ppName[4];
    u32    ra;
    CpWord word;
    u32    xpAddress;

    if (activePpu->id < 2)
        {
        return;                    // MTR and DSD are not checked
        }
    irAddress = PPC + ((activePpu->id) * 8);
    word      = cpMem[irAddress] & Mask60;
    ppName[0] = cdcToAscii[(word >> 54) & 077];
    ppName[1] = cdcToAscii[(word >> 48) & 077];
    ppName[2] = cdcToAscii[(word >> 42) & 077];
    ppName[3] = '\0';
    np        = NULL;
    for (i = 0; ppMonitored[i] != NULL; i++)
        {
        if (strcmp(ppName, ppMonitored[i]) == 0)
            {
            np = ppMonitored[i];
            break;
            }
        }
    if (np == NULL)
        {
        return;
        }
    cpn       = (word >> 36) & 037;
    xpAddress = cpn * 0200;
    nfl       = ((cpMem[xpAddress + FLSW] >> 48) & 07777) << 6;
    ra        = (cpMem[xpAddress + 1] >> 36) & Mask21;
    fl        = (cpMem[xpAddress + 2] >> 36) & Mask21;
    if (address < 0200)
        {
        return;                                                     // write to CMR
        }
    if ((address >= irAddress) && (address < irAddress + 8))
        {
        return;                                                     // write to PP comm area
        }
    if ((address >= xpAddress) && (address < xpAddress + 0200))
        {
        return;                                                     // write to job's control point area
        }
    if ((address >= (ra - nfl)) && (address < ra + fl))
        {
        return;                                                     // write within job field length
        }
    if ((address >= 041200) && (address < 041300))
        {
        return;                                                     // write to ????
        }
    if (strcmp(inst, "CWD") == 0)
        {
        fprintf(ppLog, "%s : PP%02o CWD P:%04o, write " FMT60_020o " to %08o\n",
                ppName, activePpu->id, activePpu->regP, data, address);
        }
    else
        {
        fprintf(ppLog, "%s : PP%02o CWM P:%04o Q:%04o (0):%04o, write " FMT60_020o " to %08o\n",
                ppName, activePpu->id, activePpu->regP, activePpu->regQ, activePpu->mem[0], data, address);
        }
    fprintf(ppLog, "      CP%02o RA:%o FL:%o NFL:%o\n", cpn, ra, fl, nfl);
    }

#endif

/*---------------------------  End Of File  ------------------------------*/
