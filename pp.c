/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**
**  Name: pp.c
**
**  Description:
**      Perform emulation of CDC 6600 PPs
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

/*
**  -----------------------
**  Private Macro Functions
**  -----------------------
*/
#define PpIncrement(word) (word) = (((word) + 1) & Mask12)
#define PpDecrement(word) (word) = (((word) - 1) & Mask12)

///// the program (PPU ?) stops when "from" is 000 or 077 a deadstart is necessary - see 6600 RM page 4-22 (UJN)
#define PpAddOffset(to, from)                                               \
    {                                                                       \
    (to) = ((to) - 1) & Mask12;                                             \
    if (from < 040)                                                         \
        (to) = ((to) + (from));                                             \
    else                                                                    \
        (to) = ((to) + (from) - 077);                                       \
    if (((to) & Overflow12) != 0)                                           \
        {                                                                   \
        (to) += 1;                                                          \
        }                                                                   \
    (to) &= Mask12;                                                         \
    }

#define IndexLocation                                                       \
    if (opD != 0)                                                           \
        {                                                                   \
        location = activePpu->mem[opD] + activePpu->mem[activePpu->regP];   \
        }                                                                   \
    else                                                                    \
        {                                                                   \
        location = activePpu->mem[activePpu->regP];                         \
        }                                                                   \
    if ((location & Overflow12) != 0 || (location & Mask12) == 07777)       \
        {                                                                   \
        location += 1;                                                      \
        }                                                                   \
    location &= Mask12;                                                     \
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
static void ppOpPSN24(void);  // 24
static void ppOpPSN25(void);  // 25
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

static u32 ppAdd18(u32 op1, u32 op2);
static u32 ppSubtract18(u32 op1, u32 op2);
static void ppInterlock(PpWord func);

/*
**  ----------------
**  Public Variables
**  ----------------
*/
PpSlot *ppu;
PpSlot *activePpu;
u8 ppuCount;
FILE *devF;

/*
**  -----------------
**  Private Variables
**  -----------------
*/
static FILE *ppHandle;
static u8 pp = 0;
static PpByte opF;
static PpByte opD;
static PpWord location;
static u32 acc18;
static bool noHang;

static void (*decodePpuOpcode[])(void) = 
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
    ppOpPSN24,  // 24
    ppOpPSN25,  // 25
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
    ppu = calloc(count, sizeof(PpSlot));
    if (ppu == NULL)
        {
        fprintf(stderr, "Failed to allocate ppu control blocks\n");
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
                printf("Unexpected length of PPM backing file, clearing PPM\n");
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
                fprintf(stderr, "Failed to create PPM backing file\n");
                exit(1);
                }
            }
        }

    /*
    **  Initialise all ppus.
    */
    for (pp = 0; pp < ppuCount; pp++)
        {
        ppu[pp].id = pp;
        }

    pp = 0;

    /*
    **  Print a friendly message.
    */
    printf("PPs initialised (number of PPUs %o)\n", ppuCount);
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
            fprintf(stderr, "Error writing PPM backing file\n");
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
    PpWord opCode;

    /*
    **  Exercise each PP in the barrel.
    */
    for (i = 0; i < ppuCount; i++)
        {
        /*
        **  Advance to next PPU.
        */
        activePpu = ppu + i;

        if (!activePpu->busy)
            {
            /*
            **  Extract next PPU instruction.
            */
            opCode = activePpu->mem[activePpu->regP];
            opF = (opCode >> 6) & 077;
            opD = opCode & 077;

#if CcDebug == 1
            /*
            **  Save opF and opD for post-instruction trace.
            */
            activePpu->opF = opF;
            activePpu->opD = opD;

            /*
            **  Trace instructions.
            */
            traceSequence();
            traceRegisters();
            traceOpcode();
#else
traceSequenceNo += 1;
#endif

            /*
            **  Increment register P.
            */
            PpIncrement(activePpu->regP);

            /*
            **  Execute PPU instruction.
            */
            decodePpuOpcode[opF]();
            }
        else
            {
            /*
            **  Resume PPU instruction.
            */
            decodePpuOpcode[activePpu->opF]();
            }

#if CcDebug == 1
        if (!activePpu->busy)
            {
            /*
            **  Trace result.
            */
            traceRegisters();

            /*
            **  Trace new channel status.
            */
            if (activePpu->opF >= 064)
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

    return(acc18 & Mask18);
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

    return(acc18 & Mask18);
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
    PpAddOffset(activePpu->regP, opD);
    }

static void ppOpZJN(void)     // 04
    {
    if (activePpu->regA == 0)
        {
        PpAddOffset(activePpu->regP, opD);
        }
    }

static void ppOpNJN(void)     // 05
    {
    if (activePpu->regA != 0)
        {
        PpAddOffset(activePpu->regP, opD);
        }
    }

static void ppOpPJN(void)     // 06
    {
    if (activePpu->regA < 0400000)
        {
        PpAddOffset(activePpu->regP, opD);
        }
    }

static void ppOpMJN(void)     // 07
    {
    if (activePpu->regA > 0377777)
        {
        PpAddOffset(activePpu->regP, opD);
        }
    }

static void ppOpSHN(void)     // 10
    {
    u64 acc;

    if (opD < 040)
        {
        opD = opD % 18;
        acc = activePpu->regA & Mask18;
        acc <<= opD;
        activePpu->regA = (u32)((acc & Mask18) | (acc >> 18));
        }
    else if (opD > 037)
        {
        opD = 077 - opD;
        activePpu->regA >>= opD;
        }
    }

static void ppOpLMN(void)     // 11
    {
    activePpu->regA ^= opD;
    }

static void ppOpLPN(void)     // 12
    {
    activePpu->regA &= opD;
    }

static void ppOpSCN(void)     // 13
    {
    activePpu->regA &= ~(opD & 077);
    }

static void ppOpLDN(void)     // 14
    {
    activePpu->regA = opD;
    }

static void ppOpLCN(void)     // 15
    {
    activePpu->regA = ~opD & Mask18;
    }

static void ppOpADN(void)     // 16
    {
    activePpu->regA = ppAdd18(activePpu->regA, opD);
    }

static void ppOpSBN(void)     // 17
    {
    activePpu->regA = ppSubtract18(activePpu->regA, opD);
    }

static void ppOpLDC(void)     // 20
    {
    activePpu->regA = (opD << 12) | (activePpu->mem[activePpu->regP] & Mask12);
    PpIncrement(activePpu->regP);
    }

static void ppOpADC(void)     // 21
    {
    activePpu->regA = ppAdd18(activePpu->regA, (opD << 12) | (activePpu->mem[activePpu->regP] & Mask12));
    PpIncrement(activePpu->regP);
    }

static void ppOpLPC(void)     // 22
    {
    activePpu->regA &= (opD << 12) | (activePpu->mem[activePpu->regP] & Mask12);
    PpIncrement(activePpu->regP);
    }

static void ppOpLMC(void)     // 23
    {
    activePpu->regA ^= (opD << 12) | (activePpu->mem[activePpu->regP] & Mask12);
    PpIncrement(activePpu->regP);
    }

static void ppOpPSN24(void)     // 24
    {
    if (opD != 0)
        {
        if ((features & HasRelocationRegShort) != 0)
            {
            /*
            **  LRD.
            */
//            activePpu->regR  = (u32)(activePpu->mem[opD    ] & Mask4 ) << 18; // 875
            activePpu->regR  = (u32)(activePpu->mem[opD    ] & Mask3 ) << 18; // 865
            activePpu->regR |= (u32)(activePpu->mem[opD + 1] & Mask12) << 6;
            }
        else if ((features & HasRelocationRegLong) != 0)
            {
            /*
            **  LRD.
            */
            activePpu->regR  = (u32)(activePpu->mem[opD    ] & Mask10) << 18;
            activePpu->regR |= (u32)(activePpu->mem[opD + 1] & Mask12) << 6;
            }
        }

    /*
    **  Do nothing.
    */
    }

static void ppOpPSN25(void)     // 25
    {
    if (opD != 0)
        {
        if ((features & HasRelocationRegShort) != 0)
            {
            /*
            **  SRD.
            */
//            activePpu->mem[opD    ] = (PpWord)(activePpu->regR >> 18) & Mask4; // 875
            activePpu->mem[opD    ] = (PpWord)(activePpu->regR >> 18) & Mask3; // 865
            activePpu->mem[opD + 1] = (PpWord)(activePpu->regR >>  6) & Mask12;
            }
        else if ((features & HasRelocationRegLong) != 0)
            {
            /*
            **  SRD.
            */
            activePpu->mem[opD    ] = (PpWord)(activePpu->regR >> 18) & Mask10;
            activePpu->mem[opD + 1] = (PpWord)(activePpu->regR >>  6) & Mask12;
            }
        }

    /*
    **  Do nothing.
    */
    }

static void ppOpEXN(void)     // 26
    {
    u32 exchangeAddress;

    if ((opD & 070) == 0 || (features & HasNoCejMej) != 0)
        {
        /*
        **  EXN or MXN/MAN with CEJ/MEJ disabled.
        */
        if ((activePpu->regA & Sign18) != 0 && (features & HasRelocationReg) != 0)
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
        if (cpu.monitorMode)
            {
            /*
            **  Pass.
            */
            return;
            }

        if ((opD & 070) == 010)
            {
            /*
            **  MXN.
            */
            cpu.monitorMode = TRUE;

            if ((activePpu->regA & Sign18) != 0 && (features & HasRelocationReg) != 0)
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
        else if ((opD & 070) == 020)
            {
            /*
            **  MAN.
            */
            cpu.monitorMode = TRUE;

            exchangeAddress = cpu.regMa & Mask18;
            }
        else
            {
            /*
            **  Pass.
            */
            return;
            }
        }

    /*
    **  Perform the exchange, but wait until the last parcel of the
    **  current instruction word has been executed.
    */
    while (!cpuExchangeJump(exchangeAddress))
        {
        cpuStep();
        }
    }

static void ppOpRPN(void)     // 27
    {
    /*
    **  RPN except for series 800 (865 has it though).
    */
    if ((features & IsSeries800) == 0 || modelType == ModelCyber865)
        {
        activePpu->regA = cpuGetP();
        }
    }

static void ppOpLDD(void)     // 30
    {
    activePpu->regA = activePpu->mem[opD] & Mask12;
    activePpu->regA &= Mask18;
    }

static void ppOpADD(void)     // 31
    {
    activePpu->regA = ppAdd18(activePpu->regA, activePpu->mem[opD] & Mask12);
    }

static void ppOpSBD(void)     // 32
    {
    activePpu->regA = ppSubtract18(activePpu->regA, activePpu->mem[opD] & Mask12);
    }

static void ppOpLMD(void)     // 33
    {
    activePpu->regA ^= activePpu->mem[opD] & Mask12;
    activePpu->regA &= Mask18;
    }

static void ppOpSTD(void)     // 34
    {
    activePpu->mem[opD] = (PpWord)activePpu->regA & Mask12;
    }

static void ppOpRAD(void)     // 35
    {
    activePpu->regA = ppAdd18(activePpu->regA, activePpu->mem[opD] & Mask12);
    activePpu->mem[opD] = (PpWord)activePpu->regA & Mask12;
    }

static void ppOpAOD(void)     // 36
    {
    activePpu->regA = ppAdd18(activePpu->mem[opD] & Mask12, 1);
    activePpu->mem[opD] = (PpWord)activePpu->regA & Mask12;
    }

static void ppOpSOD(void)     // 37
    {
    activePpu->regA = ppSubtract18(activePpu->mem[opD] & Mask12, 1);
    activePpu->mem[opD] = (PpWord)activePpu->regA & Mask12;
    }

static void ppOpLDI(void)     // 40
    {
    location = activePpu->mem[opD] & Mask12;
    activePpu->regA = activePpu->mem[location] & Mask12;
    }

static void ppOpADI(void)     // 41
    {
    location = activePpu->mem[opD] & Mask12;
    activePpu->regA = ppAdd18(activePpu->regA, activePpu->mem[location] & Mask12);
    }

static void ppOpSBI(void)     // 42
    {
    location = activePpu->mem[opD] & Mask12;
    activePpu->regA = ppSubtract18(activePpu->regA, activePpu->mem[location] & Mask12);
    }

static void ppOpLMI(void)     // 43
    {
    location = activePpu->mem[opD] & Mask12;
    activePpu->regA ^= activePpu->mem[location] & Mask12;
    activePpu->regA &= Mask18;
    }

static void ppOpSTI(void)     // 44
    {
    location = activePpu->mem[opD] & Mask12;
    activePpu->mem[location] = (PpWord)activePpu->regA & Mask12;
    }

static void ppOpRAI(void)     // 45
    {
    location = activePpu->mem[opD] & Mask12;
    activePpu->regA = ppAdd18(activePpu->regA, activePpu->mem[location] & Mask12);
    activePpu->mem[location] = (PpWord)activePpu->regA & Mask12;
    }

static void ppOpAOI(void)     // 46
    {
    location = activePpu->mem[opD] & Mask12;
    activePpu->regA = ppAdd18(activePpu->mem[location] & Mask12, 1);
    activePpu->mem[location] = (PpWord)activePpu->regA & Mask12;
    }

static void ppOpSOI(void)     // 47
    {
    location = activePpu->mem[opD] & Mask12;
    activePpu->regA = ppSubtract18(activePpu->mem[location] & Mask12, 1);
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
    activePpu->regA = ppAdd18(activePpu->regA, activePpu->mem[location] & Mask12);
    activePpu->mem[location] = (PpWord)activePpu->regA & Mask12;
    }

static void ppOpAOM(void)     // 56
    {
    IndexLocation;
    activePpu->regA = ppAdd18(activePpu->mem[location] & Mask12, 1);
    activePpu->mem[location] = (PpWord)activePpu->regA & Mask12;
    }

static void ppOpSOM(void)     // 57
    {
    IndexLocation;
    activePpu->regA = ppSubtract18(activePpu->mem[location] & Mask12, 1);
    activePpu->mem[location] = (PpWord)activePpu->regA & Mask12;
    }

static void ppOpCRD(void)     // 60
    {
    CpWord data;

    if ((activePpu->regA & Sign18) != 0 && (features & HasRelocationReg) != 0)
        {
        cpuPpReadMem(activePpu->regR + (activePpu->regA & Mask17), &data);
        }
    else
        {
        cpuPpReadMem(activePpu->regA & Mask18, &data);
        }

    activePpu->mem[opD++ & Mask12] = (PpWord)((data >> 48) & Mask12);
    activePpu->mem[opD++ & Mask12] = (PpWord)((data >> 36) & Mask12);
    activePpu->mem[opD++ & Mask12] = (PpWord)((data >> 24) & Mask12);
    activePpu->mem[opD++ & Mask12] = (PpWord)((data >> 12) & Mask12);
    activePpu->mem[opD   & Mask12] = (PpWord)((data      ) & Mask12);
    }

static void ppOpCRM(void)     // 61
    {
    CpWord data;

    if (!activePpu->busy)
        {
        activePpu->opF = opF;
        activePpu->regQ = activePpu->mem[opD] & Mask12;

        activePpu->busy = TRUE;

        activePpu->mem[0] = activePpu->regP;
        activePpu->regP = activePpu->mem[activePpu->regP] & Mask12;
        }

    if (activePpu->regQ--)
        {
        if ((activePpu->regA & Sign18) != 0 && (features & HasRelocationReg) != 0)
            {
            cpuPpReadMem(activePpu->regR + (activePpu->regA & Mask17), &data);
            }
        else
            {
            cpuPpReadMem(activePpu->regA & Mask18, &data);
            }

        activePpu->mem[activePpu->regP++ & Mask12] = (PpWord)((data >> 48) & Mask12);
        activePpu->mem[activePpu->regP++ & Mask12] = (PpWord)((data >> 36) & Mask12);
        activePpu->mem[activePpu->regP++ & Mask12] = (PpWord)((data >> 24) & Mask12);
        activePpu->mem[activePpu->regP++ & Mask12] = (PpWord)((data >> 12) & Mask12);
        activePpu->mem[activePpu->regP++ & Mask12] = (PpWord)((data      ) & Mask12);

        activePpu->regA += 1;
        activePpu->regA &= Mask18;
        }

    if (activePpu->regQ == 0)
        {
        activePpu->regP = activePpu->mem[0];
        PpIncrement(activePpu->regP);
        activePpu->busy = FALSE;
        }
    }

static void ppOpCWD(void)     // 62
    {
    CpWord data;

    data  = activePpu->mem[opD++ & Mask12] & Mask12;
    data <<= 12;

    data |= activePpu->mem[opD++ & Mask12] & Mask12;
    data <<= 12;

    data |= activePpu->mem[opD++ & Mask12] & Mask12;
    data <<= 12;

    data |= activePpu->mem[opD++ & Mask12] & Mask12;
    data <<= 12;

    data |= activePpu->mem[opD   & Mask12] & Mask12;

    if ((activePpu->regA & Sign18) != 0 && (features & HasRelocationReg) != 0)
        {
        cpuPpWriteMem(activePpu->regR + (activePpu->regA & Mask17), data);
        }
    else
        {
        cpuPpWriteMem(activePpu->regA & Mask18, data);
        }
    }

static void ppOpCWM(void)     // 63
    {
    CpWord data;

    if (!activePpu->busy)
        {
        activePpu->opF = opF;
        activePpu->regQ = activePpu->mem[opD] & Mask12;

        activePpu->busy = TRUE;

        activePpu->mem[0] = activePpu->regP;
        activePpu->regP = activePpu->mem[activePpu->regP] & Mask12;
        }

    if (activePpu->regQ--)
        {
        data  = activePpu->mem[activePpu->regP++ & Mask12] & Mask12;
        data <<= 12;

        data |= activePpu->mem[activePpu->regP++ & Mask12] & Mask12;
        data <<= 12;

        data |= activePpu->mem[activePpu->regP++ & Mask12] & Mask12;
        data <<= 12;

        data |= activePpu->mem[activePpu->regP++ & Mask12] & Mask12;
        data <<= 12;

        data |= activePpu->mem[activePpu->regP++ & Mask12] & Mask12;

        if ((activePpu->regA & Sign18) != 0 && (features & HasRelocationReg) != 0)
            {
            cpuPpWriteMem(activePpu->regR + (activePpu->regA & Mask17), data);
            }
        else
            {
            cpuPpWriteMem(activePpu->regA & Mask18, data);
            }

        activePpu->regA += 1;
        activePpu->regA &= Mask18;
        }

    if (activePpu->regQ == 0)
        {
        activePpu->regP = activePpu->mem[0];
        PpIncrement(activePpu->regP);
        activePpu->busy = FALSE;
        }
    }

static void ppOpAJM(void)     // 64
    {
    location = activePpu->mem[activePpu->regP];
    location &= Mask12;
    PpIncrement(activePpu->regP);

    if (   (opD & 040) != 0
        && (features & HasChannelFlag) != 0)
        {
        /*
        **  SCF.
        */
        opD &= 037;
        if (opD < channelCount)
            {
            if (channel[opD].flag)
                {
                activePpu->regP = location;
                }
            else
                {
                channel[opD].flag = TRUE;
                }
            }

        return;
        }

    opD &= 037;
    if (opD < channelCount)
        {
        activeChannel = channel + opD;
        channelCheckIfActive();
        if (activeChannel->active)
            {
            activePpu->regP = location;
            }
        }

    }

static void ppOpIJM(void)     // 65
    {
    location = activePpu->mem[activePpu->regP];
    location &= Mask12;
    PpIncrement(activePpu->regP);

    if (   (opD & 040) != 0
        && (features & HasChannelFlag) != 0)
        {
        /*
        **  CCF.
        */
        opD &= 037;
        if (opD < channelCount)
            {
            channel[opD].flag = FALSE;
            }

        return;
        }

    opD &= 037;
    if (opD >= channelCount)
        {
        activePpu->regP = location;
        }
    else
        {
        activeChannel = channel + opD;
        channelCheckIfActive();
        if (!activeChannel->active)
            {
            activePpu->regP = location;
            }
        }
    }

static void ppOpFJM(void)     // 66
    {
    location = activePpu->mem[activePpu->regP];
    location &= Mask12;
    PpIncrement(activePpu->regP);

    if (   (opD & 040) != 0
        && (features & HasErrorFlag) != 0)
        {
        /*
        **  SFM - we never have errors, so this is just a pass.
        */
        return;
        }

    opD &= 037;
    if (opD < channelCount)
        {
        activeChannel = channel + opD;
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
    location = activePpu->mem[activePpu->regP];
    location &= Mask12;
    PpIncrement(activePpu->regP);

    if (   (opD & 040) != 0
        && (features & HasErrorFlag) != 0)
        {
        /*
        **  CFM - we never have errors, so we always jump.
        */
        opD &= 037;
        if (opD < channelCount)
            {
            activePpu->regP = location;
            }

        return;
        }

    opD &= 037;
    if (opD >= channelCount)
        {
        activePpu->regP = location;
        }
    else
        {
        activeChannel = channel + opD;
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
        activePpu->opF = opF;
        activePpu->opD = opD;
        activeChannel->delayStatus = 0;
        }

    noHang = (activePpu->opD & 040) != 0;
    activeChannel = channel + (activePpu->opD & 037);
    activePpu->busy = TRUE;

    channelCheckIfActive();
    if (!activeChannel->active && activeChannel->id != ChClock)
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

    if (activeChannel->full || activeChannel->id == ChClock)
        {
        /*
        **  Handle input (note that the clock channel has always data pending,
        **  but appears full on some models, empty on others).
        */
        channelIn();
        channelSetEmpty();
        activePpu->regA = activeChannel->data & Mask12;
        activeChannel->inputPending = FALSE;
        if (activeChannel->discAfterInput)
            {
            activeChannel->discAfterInput = FALSE;
            activeChannel->delayDisconnect = 0;
            activeChannel->active = FALSE;
            activeChannel->ioDevice = NULL;
            }

        activePpu->busy = FALSE;
        }
    }

static void ppOpIAM(void)     // 71
    {
    if (!activePpu->busy)
        {
        activePpu->opF = opF;
        activePpu->opD = opD;

        activeChannel = channel + (activePpu->opD & 037);
        activePpu->busy = TRUE;

        activePpu->mem[0] = activePpu->regP;
        activePpu->regP = activePpu->mem[activePpu->regP] & Mask12;
        activeChannel->delayStatus = 0;
        }
    else
        {
        activeChannel = channel + (activePpu->opD & 037);
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

    if (activeChannel->full || activeChannel->id == ChClock)
        {
        /*
        **  Handle input (note that the clock channel has always data pending,
        **  but appears full on some models, empty on others).
        */
        channelIn();
        channelSetEmpty();
        activePpu->mem[activePpu->regP] = activeChannel->data & Mask12;
        activePpu->regP = (activePpu->regP + 1) & Mask12;
        activePpu->regA = (activePpu->regA - 1) & Mask18;
        activeChannel->inputPending = FALSE;

        if (activeChannel->discAfterInput)
            {
            activeChannel->discAfterInput = FALSE;
            activeChannel->delayDisconnect = 0;
            activeChannel->active = FALSE;
            activeChannel->ioDevice = NULL;
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
            activePpu->busy = FALSE;
            }
        }
    }

static void ppOpOAN(void)     // 72
    {
    if (!activePpu->busy)
        {
        activePpu->opF = opF;
        activePpu->opD = opD;
        activeChannel->delayStatus = 0;
        }

    noHang = (activePpu->opD & 040) != 0;
    activeChannel = channel + (activePpu->opD & 037);
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
        activeChannel->data = (PpWord)activePpu->regA & Mask12;
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
    if (!activePpu->busy)
        {
        activePpu->opF = opF;
        activePpu->opD = opD;

        activeChannel = channel + (activePpu->opD & 037);
        activePpu->busy = TRUE;

        activePpu->mem[0] = activePpu->regP;
        activePpu->regP = activePpu->mem[activePpu->regP] & Mask12;
        activeChannel->delayStatus = 0;
        }
    else
        {
        activeChannel = channel + (activePpu->opD & 037);
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
        activeChannel->data = activePpu->mem[activePpu->regP] & Mask12;
        activePpu->regP = (activePpu->regP + 1) & Mask12;
        activePpu->regA = (activePpu->regA - 1) & Mask18;
        channelOut();
        channelSetFull();

        if (activePpu->regA == 0)
            {
            activePpu->regP = activePpu->mem[0];
            PpIncrement(activePpu->regP);
            activePpu->busy = FALSE;
            }
        }

    /*
    **  Handle possible output.
    */
    channelIo();
    }

static void ppOpACN(void)     // 74
    {
    if (!activePpu->busy)
        {
        activePpu->opF = opF;
        activePpu->opD = opD;
        }

    noHang = (activePpu->opD & 040) != 0;
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
    if (!activePpu->busy)
        {
        activePpu->opF = opF;
        activePpu->opD = opD;
        }

    noHang = (activePpu->opD & 040) != 0;
    activeChannel = channel + (activePpu->opD & 037);

    /*
    **  RTC, Interlock and S/C register channel can not be deactivated.
    */
    if (activeChannel->id == ChClock)
        {
        return;
        }

    if (activeChannel->id == ChInterlock && (features & HasInterlockReg) != 0)
        {
        return;
        }

    if (activeChannel->id == ChStatusAndControl && (features & HasStatusAndControlReg) != 0)
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
    if (!activePpu->busy)
        {
        activePpu->opF = opF;
        activePpu->opD = opD;
        }

    noHang = (activePpu->opD & 040) != 0;
    activeChannel = channel + (activePpu->opD & 037);

    /*
    **  Interlock register channel ignores functions.
    */
    if (activeChannel->id == ChInterlock && (features & HasInterlockReg) != 0)
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

    channelFunction((PpWord)(activePpu->regA & Mask12));
    activePpu->busy = FALSE;
    }

static void ppOpFNC(void)     // 77
    {
    if (!activePpu->busy)
        {
        activePpu->opF = opF;
        activePpu->opD = opD;
        }

    noHang = (activePpu->opD & 040) != 0;
    activeChannel = channel + (activePpu->opD & 037);

    /*
    **  Interlock register channel ignores functions.
    */
    if (activeChannel->id == ChInterlock && (features & HasInterlockReg) != 0)
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

/*---------------------------  End Of File  ------------------------------*/
