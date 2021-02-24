/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**
**  Name: cpu.c
**
**  Description:
**      Perform emulation of CDC 6600 or CYBER class CPU.
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

#define EMDEBUG 0

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

/* Only enable this for testing to pass section 4.A of EJT (divide break-in test) */
#define CcSMM_EJT               1

/*
**  CPU exit conditions.
*/
#define EcNone                  00
#define EcAddressOutOfRange     01
#define EcOperandOutOfRange     02
#define EcIndefiniteOperand     04

/*
**  ECS bank size taking into account the 5k reserve.
*/
#define EcsBankSize             (131072 - 5120)
#define EsmBankSize             131072

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
typedef struct opDispatch
    {
    void (*execute)(void);
    u8   length;
    } OpDispatch;

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static void cpuOpIllegal(void);
static bool cpuCheckOpAddress(u32 address, u32 *location);
static void cpuFetchOpWord(u32 address, CpWord *data);
static void cpuVoidIwStack(u32 branchAddr);
static bool cpuReadMem(u32 address, CpWord *data);
static bool cpuWriteMem(u32 address, CpWord *data);
static void cpuRegASemantics(void);
static u32 cpuAddRa(u32 op);
static u32 cpuAdd18(u32 op1, u32 op2);
static u32 cpuAdd24(u32 op1, u32 op2);
static u32 cpuSubtract18(u32 op1, u32 op2);
static void cpuUemWord(bool writeToUem);
static void cpuEcsWord(bool writeToEcs);
static void cpuUemTransfer(bool writeToUem);
static void cpuEcsTransfer(bool writeToEcs);
static bool cpuCmuGetByte(u32 address, u32 pos, u8 *byte);
static bool cpuCmuPutByte(u32 address, u32 pos, u8 byte);
static void cpuCmuMoveIndirect(void);
static void cpuCmuMoveDirect(void);
static void cpuCmuCompareCollated(void);
static void cpuCmuCompareUncollated(void);
static void cpuFloatCheck(CpWord value);
static void cpuFloatExceptionHandler(void);

static void cpOp00(void);
static void cpOp01(void);
static void cpOp02(void);
static void cpOp03(void);
static void cpOp04(void);
static void cpOp05(void);
static void cpOp06(void);
static void cpOp07(void);
static void cpOp10(void);
static void cpOp11(void);
static void cpOp12(void);
static void cpOp13(void);
static void cpOp14(void);
static void cpOp15(void);
static void cpOp16(void);
static void cpOp17(void);
static void cpOp20(void);
static void cpOp21(void);
static void cpOp22(void);
static void cpOp23(void);
static void cpOp24(void);
static void cpOp25(void);
static void cpOp26(void);
static void cpOp27(void);
static void cpOp30(void);
static void cpOp31(void);
static void cpOp32(void);
static void cpOp33(void);
static void cpOp34(void);
static void cpOp35(void);
static void cpOp36(void);
static void cpOp37(void);
static void cpOp40(void);
static void cpOp41(void);
static void cpOp42(void);
static void cpOp43(void);
static void cpOp44(void);
static void cpOp45(void);
static void cpOp46(void);
static void cpOp47(void);
static void cpOp50(void);
static void cpOp51(void);
static void cpOp52(void);
static void cpOp53(void);
static void cpOp54(void);
static void cpOp55(void);
static void cpOp56(void);
static void cpOp57(void);
static void cpOp60(void);
static void cpOp61(void);
static void cpOp62(void);
static void cpOp63(void);
static void cpOp64(void);
static void cpOp65(void);
static void cpOp66(void);
static void cpOp67(void);
static void cpOp70(void);
static void cpOp71(void);
static void cpOp72(void);
static void cpOp73(void);
static void cpOp74(void);
static void cpOp75(void);
static void cpOp76(void);
static void cpOp77(void);

/*
**  ----------------
**  Public Variables
**  ----------------
*/
CpWord *cpMem;
CpWord *extMem;
u32 ecsFlagRegister;
CpuContext cpu;
bool cpuStopped = TRUE;
u32 cpuMaxMemory;
u32 extMaxMemory;

/*
**  -----------------
**  Private Variables
**  -----------------
*/
static FILE *cmHandle;
static FILE *ecsHandle;
static u8 opOffset;
static CpWord opWord;
static u8 opFm;
static u8 opI;
static u8 opJ;
static u8 opK;
static u8 opLength;
static u32 opAddress;
static u32 oldRegP;
static CpWord acc60;
static u32 acc18;
static u32 acc21;
static u32 acc24;
static bool floatException = FALSE;

static int debugCount = 0;

#if CcSMM_EJT
static int skipStep = 0;
#endif

#if EMDEBUG
static FILE *emLog = NULL;
#endif

/*
**  Opcode decode and dispatch table.
*/
static OpDispatch decodeCpuOpcode[] =
    {
    cpOp00, 15,
    cpOp01, 0,
    cpOp02, 30,
    cpOp03, 30,
    cpOp04, 30,
    cpOp05, 30,
    cpOp06, 30,
    cpOp07, 30,
    cpOp10, 15,
    cpOp11, 15,
    cpOp12, 15,
    cpOp13, 15,
    cpOp14, 15,
    cpOp15, 15,
    cpOp16, 15,
    cpOp17, 15,
    cpOp20, 15,
    cpOp21, 15,
    cpOp22, 15,
    cpOp23, 15,
    cpOp24, 15,
    cpOp25, 15,
    cpOp26, 15,
    cpOp27, 15,
    cpOp30, 15,
    cpOp31, 15,
    cpOp32, 15,
    cpOp33, 15,
    cpOp34, 15,
    cpOp35, 15,
    cpOp36, 15,
    cpOp37, 15,
    cpOp40, 15,
    cpOp41, 15,
    cpOp42, 15,
    cpOp43, 15,
    cpOp44, 15,
    cpOp45, 15,
    cpOp46, 15,
    cpOp47, 15,
    cpOp50, 30,
    cpOp51, 30,
    cpOp52, 30,
    cpOp53, 15,
    cpOp54, 15,
    cpOp55, 15,
    cpOp56, 15,
    cpOp57, 15,
    cpOp60, 30,
    cpOp61, 30,
    cpOp62, 30,
    cpOp63, 15,
    cpOp64, 15,
    cpOp65, 15,
    cpOp66, 15,
    cpOp67, 15,
    cpOp70, 30,
    cpOp71, 30,
    cpOp72, 30,
    cpOp73, 15,
    cpOp74, 15,
    cpOp75, 15,
    cpOp76, 15,
    cpOp77, 15
    };

static u8 cpOp01Length[8] = { 30, 30, 30, 30, 15, 15, 15, 15 };

/*
**--------------------------------------------------------------------------
**
**  Public Functions
**
**--------------------------------------------------------------------------
*/


/*--------------------------------------------------------------------------
**  Purpose:        Initialise CPU.
**
**  Parameters:     Name        Description.
**                  model       CPU model string
**                  memory      configured central memory
**                  emBanks     configured number of extended memory banks
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cpuInit(char *model, u32 memory, u32 emBanks, ExtMemory emType)
    {
    u32 extBanksSize;

    /*
    **  Allocate configured central memory.
    */
    cpMem = calloc(memory, sizeof(CpWord));
    if (cpMem == NULL)
        {
        fprintf(stderr, "Failed to allocate CPU memory\n");
        exit(1);
        }

    cpuMaxMemory = memory;

    switch (emType)
        {
    case ECS:
        extBanksSize = EcsBankSize;
        break;

    case ESM:
        extBanksSize = EsmBankSize;
        break;
        }

    /*
    **  Allocate configured ECS memory.
    */
    extMem = calloc(emBanks * extBanksSize, sizeof(CpWord));
    if (extMem == NULL)
        {
        fprintf(stderr, "Failed to allocate ECS memory\n");
        exit(1);
        }

    extMaxMemory = emBanks * extBanksSize;

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
        strcat(fileName, "/cmStore");
        cmHandle = fopen(fileName, "r+b");
        if (cmHandle != NULL)
            {
            /*
            **  Read CM contents.
            */
            if (fread(cpMem, sizeof(CpWord), cpuMaxMemory, cmHandle) != cpuMaxMemory)
                {
                printf("Unexpected length of CM backing file, clearing CM\n");
                memset(cpMem, 0, cpuMaxMemory);
                }
            }
        else
            {
            /*
            **  Create a new file.
            */
            cmHandle = fopen(fileName, "w+b");
            if (cmHandle == NULL)
                {
                fprintf(stderr, "Failed to create CM backing file\n");
                exit(1);
                }
            }

        /*
        **  Try to open existing ECS file.
        */
        strcpy(fileName, persistDir);
        strcat(fileName, "/ecsStore");
        ecsHandle = fopen(fileName, "r+b");
        if (ecsHandle != NULL)
            {
            /*
            **  Read ECS contents.
            */
            if (fread(extMem, sizeof(CpWord), extMaxMemory, ecsHandle) != extMaxMemory)
                {
                printf("Unexpected length of ECS backing file, clearing ECS\n");
                memset(extMem, 0, extMaxMemory);
                }
            }
        else
            {
            /*
            **  Create a new file.
            */
            ecsHandle = fopen(fileName, "w+b");
            if (ecsHandle == NULL)
                {
                fprintf(stderr, "Failed to create ECS backing file\n");
                exit(1);
                }
            }
        }

    /*
    **  Print a friendly message.
    */
    printf("CPU model %s initialised (CM: %o, ECS: %o)\n", model, cpuMaxMemory, extMaxMemory);

#if EMDEBUG
    if (emLog == NULL)
        {
        emLog = fopen("emlog.txt", "wt");
        }
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Terminate CPU and optionally persist CM.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cpuTerminate(void)
    {
    /*
    **  Optionally save CM.
    */
    if (cmHandle != NULL)
        {
        fseek(cmHandle, 0, SEEK_SET);
        if (fwrite(cpMem, sizeof(CpWord), cpuMaxMemory, cmHandle) != cpuMaxMemory)
            {
            fprintf(stderr, "Error writing CM backing file\n");
            }

        fclose(cmHandle);
        }

    /*
    **  Optionally save ECS.
    */
    if (ecsHandle != NULL)
        {
        fseek(ecsHandle, 0, SEEK_SET);
        if (fwrite(extMem, sizeof(CpWord), extMaxMemory, ecsHandle) != extMaxMemory)
            {
            fprintf(stderr, "Error writing ECS backing file\n");
            }

        fclose(ecsHandle);
        }

    /*
    **  Free allocated memory.
    */
    free(cpMem);
    free(extMem);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Return CPU P register.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
u32 cpuGetP(void)
    {
    return((cpu.regP) & Mask18);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Read CPU memory from PP and verify that address is
**                  within limits.
**
**  Parameters:     Name        Description.
**                  address     Absolute CM address to read.
**                  data        Pointer to 60 bit word which gets the data.
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
void cpuPpReadMem(u32 address, CpWord *data)
    {
    if ((features & HasNoCmWrap) != 0)
        {
        if (address < cpuMaxMemory)
            {
            *data = cpMem[address] & Mask60;
            }
        else
            {
            *data = (~((CpWord)0)) & Mask60;
            }
        }
    else
        {
        address %= cpuMaxMemory;
        *data = cpMem[address] & Mask60;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Write CPU memory from PP and verify that address is
**                  within limits.
**
**  Parameters:     Name        Description.
**                  address     Absolute CM address
**                  data        60 bit word which holds the data to be written.
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
void cpuPpWriteMem(u32 address, CpWord data)
    {
    if ((features & HasNoCmWrap) != 0)
        {
        if (address < cpuMaxMemory)
            {
            cpMem[address] = data & Mask60;
            }
        }
    else
        {
        address %= cpuMaxMemory;
        cpMem[address] = data & Mask60;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Perform exchange jump.
**
**  Parameters:     Name        Description.
**                  addr        Exchange jump address.
**
**  Returns:        TRUE if exchange jump can be performed, FALSE otherwise.
**
**------------------------------------------------------------------------*/
bool cpuExchangeJump(u32 addr)
    {
    CpuContext tmp;
    CpWord *mem;

    /*
    **  Only perform exchange jump on instruction boundary or when stopped.
    */
    if (opOffset != 60 && !cpuStopped)
        {
        return(FALSE);
        }

#if CcDebug == 1
    traceExchange(&cpu, addr, "Old");
#endif

    /*
    **  Clear any spurious address bits.
    */
    addr &= Mask18;

    /*
    **  Verify if exchange package is within configured memory.
    */
    if (addr + 020 >= cpuMaxMemory)
        {
        /*
        **  Pretend that exchange worked, but the address is bad.
        */
        return(TRUE);
        }

    /*
    **  Save current context.
    */
    tmp = cpu;

    /*
    **  Setup new context.
    */
    mem = cpMem + addr;

    cpu.regP     = (u32)((*mem >> 36) & Mask18);
    cpu.regA[0]  = (u32)((*mem >> 18) & Mask18);
    cpu.regB[0]  = 0;

    mem += 1;
    cpu.regRaCm  = (u32)((*mem >> 36) & Mask24);
    cpu.regA[1]  = (u32)((*mem >> 18) & Mask18);
    cpu.regB[1]  = (u32)((*mem      ) & Mask18);

    mem += 1;
    cpu.regFlCm  = (u32)((*mem >> 36) & Mask24);
    cpu.regA[2]  = (u32)((*mem >> 18) & Mask18);
    cpu.regB[2]  = (u32)((*mem      ) & Mask18);

    mem += 1;
    cpu.exitMode = (u32)((*mem >> 36) & Mask24);
    cpu.regA[3]  = (u32)((*mem >> 18) & Mask18);
    cpu.regB[3]  = (u32)((*mem      ) & Mask18);

    mem += 1;
    if (   (features & IsSeries800) != 0
        && (cpu.exitMode & EmFlagExpandedAddress) != 0)
        {
        cpu.regRaEcs = (u32)((*mem >> 30) & Mask30Ecs);
        }
    else
        {
        cpu.regRaEcs = (u32)((*mem >> 36) & Mask24Ecs);
        }

    cpu.regA[4]  = (u32)((*mem >> 18) & Mask18);
    cpu.regB[4]  = (u32)((*mem      ) & Mask18);

    mem += 1;
    if (   (features & IsSeries800) != 0
        && (cpu.exitMode & EmFlagExpandedAddress) != 0)
        {
        cpu.regFlEcs = (u32)((*mem >> 30) & Mask30Ecs);
        }
    else
        {
        cpu.regFlEcs = (u32)((*mem >> 36) & Mask24Ecs);
        }

    cpu.regA[5]  = (u32)((*mem >> 18) & Mask18);
    cpu.regB[5]  = (u32)((*mem      ) & Mask18);

    mem += 1;
    cpu.regMa    = (u32)((*mem >> 36) & Mask24);
    cpu.regA[6]  = (u32)((*mem >> 18) & Mask18);
    cpu.regB[6]  = (u32)((*mem      ) & Mask18);

    mem += 1;
    cpu.regSpare = (u32)((*mem >> 36) & Mask24);
    cpu.regA[7]  = (u32)((*mem >> 18) & Mask18);
    cpu.regB[7]  = (u32)((*mem      ) & Mask18);

    mem += 1;
    cpu.regX[0]  = *mem++ & Mask60;
    cpu.regX[1]  = *mem++ & Mask60;
    cpu.regX[2]  = *mem++ & Mask60;
    cpu.regX[3]  = *mem++ & Mask60;
    cpu.regX[4]  = *mem++ & Mask60;
    cpu.regX[5]  = *mem++ & Mask60;
    cpu.regX[6]  = *mem++ & Mask60;
    cpu.regX[7]  = *mem++ & Mask60;

    cpu.exitCondition = EcNone;

#if CcDebug == 1
    traceExchange(&cpu, addr, "New");
#endif

    /*
    **  Save old context.
    */
    mem = cpMem + addr;

    *mem++ = ((CpWord)(tmp.regP     & Mask18) << 36) | ((CpWord)(tmp.regA[0] & Mask18) << 18);
    *mem++ = ((CpWord)(tmp.regRaCm  & Mask24) << 36) | ((CpWord)(tmp.regA[1] & Mask18) << 18) | ((CpWord)(tmp.regB[1] & Mask18));
    *mem++ = ((CpWord)(tmp.regFlCm  & Mask24) << 36) | ((CpWord)(tmp.regA[2] & Mask18) << 18) | ((CpWord)(tmp.regB[2] & Mask18));
    *mem++ = ((CpWord)(tmp.exitMode & Mask24) << 36) | ((CpWord)(tmp.regA[3] & Mask18) << 18) | ((CpWord)(tmp.regB[3] & Mask18));

    if (   (features & IsSeries800) != 0
        && (tmp.exitMode & EmFlagExpandedAddress) != 0)
        {
        *mem++ = ((CpWord)(tmp.regRaEcs & Mask30Ecs) << 30) | ((CpWord)(tmp.regA[4] & Mask18) << 18) | ((CpWord)(tmp.regB[4] & Mask18));
        }
    else
        {
        *mem++ = ((CpWord)(tmp.regRaEcs & Mask24Ecs) << 36) | ((CpWord)(tmp.regA[4] & Mask18) << 18) | ((CpWord)(tmp.regB[4] & Mask18));
        }

    if (   (features & IsSeries800) != 0
        && (tmp.exitMode & EmFlagExpandedAddress) != 0)
        {
        *mem++ = ((CpWord)(tmp.regFlEcs & Mask30Ecs) << 30) | ((CpWord)(tmp.regA[5] & Mask18) << 18) | ((CpWord)(tmp.regB[5] & Mask18));
        }
    else
        {
        *mem++ = ((CpWord)(tmp.regFlEcs & Mask24Ecs) << 36) | ((CpWord)(tmp.regA[5] & Mask18) << 18) | ((CpWord)(tmp.regB[5] & Mask18));
        }

    *mem++ = ((CpWord)(tmp.regMa    & Mask24) << 36) | ((CpWord)(tmp.regA[6] & Mask18) << 18) | ((CpWord)(tmp.regB[6] & Mask18));
    *mem++ = ((CpWord)(tmp.regSpare & Mask24) << 36) | ((CpWord)(tmp.regA[7] & Mask18) << 18) | ((CpWord)(tmp.regB[7] & Mask18));
    *mem++ = tmp.regX[0] & Mask60;
    *mem++ = tmp.regX[1] & Mask60;
    *mem++ = tmp.regX[2] & Mask60;
    *mem++ = tmp.regX[3] & Mask60;
    *mem++ = tmp.regX[4] & Mask60;
    *mem++ = tmp.regX[5] & Mask60;
    *mem++ = tmp.regX[6] & Mask60;
    *mem++ = tmp.regX[7] & Mask60;

    if ((features & HasInstructionStack) != 0)
        {
        /*
        **  Void the instruction stack.
        */
        cpuVoidIwStack(~0);
        }

    /*
    **  Activate CPU.
    */
    cpuStopped = FALSE;
    cpuFetchOpWord(cpu.regP, &opWord);

    return(TRUE);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Execute next instruction in the CPU.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cpuStep(void)
    {
    if (cpuStopped)
        {
        return;
        }

#if CcSMM_EJT
    if (skipStep != 0)
        {
        skipStep -= 1;
        return;
        }
#endif

    /*
    **  Execute one CM word atomically.
    */
    do
        {
        /*
        **  Decode based on type.
        */
        opFm = (u8)((opWord >> (opOffset -  6)) & Mask6);
        opI  = (u8)((opWord >> (opOffset -  9)) & Mask3);
        opJ  = (u8)((opWord >> (opOffset - 12)) & Mask3);
        opLength = decodeCpuOpcode[opFm].length;

        if (opLength == 0)
            {
            opLength = cpOp01Length[opI];
            }

        if (opLength == 15)
            {
            opK       = (u8)((opWord >> (opOffset - 15)) & Mask3);
            opAddress = 0;

            opOffset -= 15;
            }
        else
            {
            if (opOffset == 15)
                {
                /*
                **  Invalid packing is handled as illegal instruction.
                */
                cpuOpIllegal();
                return;
                }

            opK       = 0;
            opAddress = (u32)((opWord >> (opOffset - 30)) & Mask18);

            opOffset -= 30;
            }

        oldRegP = cpu.regP;

        /*
        **  Force B0 to 0.
        */
        cpu.regB[0] = 0;

        /*
        **  Execute instruction.
        */
        decodeCpuOpcode[opFm].execute();

        /*
        **  Force B0 to 0.
        */
        cpu.regB[0] = 0;

#if CcDebug == 1
        traceCpu(oldRegP, opFm, opI, opJ, opK, opAddress);
#endif

        if (cpuStopped)
            {
            if (opOffset == 0)
                {
                cpu.regP = (cpu.regP + 1) & Mask18;
                }
#if CcDebug == 1
            traceCpuPrint("Stopped\n");
#endif
            return;
            }

        /*
        **  Fetch next instruction word if necessary.
        */
        if (opOffset == 0)
            {
            cpu.regP = (cpu.regP + 1) & Mask18;
            cpuFetchOpWord(cpu.regP, &opWord);
            }
        } while (opOffset != 60);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Perform ECS flag register operation.
**
**  Parameters:     Name        Description.
**                  ecsAddress  ECS address (flag register function and data)
**
**  Returns:        TRUE if accepted, FALSE otherwise.
**
**------------------------------------------------------------------------*/
bool cpuEcsFlagRegister(u32 ecsAddress)
    {
    u32 flagFunction = (ecsAddress >> 21) & Mask3;
    u32 flagWord = ecsAddress & Mask18;

    switch (flagFunction)
        {
    case 4:
        /*
        **  Ready/Select.
        */
        if ((ecsFlagRegister & flagWord ) != 0)
            {
            /*
            **  Error exit.
            */
            return(FALSE);
            }

        ecsFlagRegister |= flagWord;
        break;

    case 5:
        /*
        **  Selective set.
        */
        ecsFlagRegister |= flagWord;
        break;

    case 6:
        /*
        **  Status.
        */
        if ((ecsFlagRegister & flagWord ) != 0)
            {
            /*
            **  Error exit.
            */
            return(FALSE);
            }

        break;

    case 7:
        /*
        **  Selective clear,
        */
        ecsFlagRegister = (ecsFlagRegister & ~flagWord) & Mask18;
        break;
        }

    /*
    **  Normal exit.
    */
    return(TRUE);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Transfer one 60 bit word to/from DDP/ECS.
**
**  Parameters:     Name        Description.
**                  ecsAddress  ECS word address
**                  data        Pointer to 60 bit word
**                  writeToEcs  TRUE if this is a write to ECS, FALSE if
**                              this is a read.
**
**  Returns:        TRUE if accepted, FALSE otherwise.
**
**------------------------------------------------------------------------*/
bool cpuDdpTransfer(u32 ecsAddress, CpWord *data, bool writeToEcs)
    {
    /*
    **  Normal (non flag-register) access must be within ECS boundaries.
    */
    if (ecsAddress >= extMaxMemory)
        {
        /*
        **  Abort.
        */
        return(FALSE);
        }

    /*
    **  Perform the transfer.
    */
    if (writeToEcs)
        {
        extMem[ecsAddress] = *data & Mask60;
        }
    else
        {
        *data = extMem[ecsAddress] & Mask60;
        }

    /*
    **  Normal accept.
    */
    return(TRUE);
    }

/*
**--------------------------------------------------------------------------
**
**  Private Functions
**
**--------------------------------------------------------------------------
*/

/*--------------------------------------------------------------------------
**  Purpose:        Print the current register set (exchange package) and
**                  a block of memory around the current P register value
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
#if EMDEBUG
static char cmRegAstr[24];

static char *readCmRegA(int regNum)
    {
    int addr;
    CpWord word;

    addr = cpu.regA[regNum];

    if (addr < cpu.regFlCm)
        {
        word = cpMem[cpu.regRaCm + addr];
        sprintf(cmRegAstr, "%05lo %05lo %05lo %05lo",
                (word >> 45) & 077777,
                (word >> 30) & 077777,
                (word >> 15) & 077777,
                 word & 077777);
        return cmRegAstr;
        }
    else
        {
        return "----- ----- ----- -----";
        }
    }
static void dumpXP(int before, int after)
    {
    int i;
    u32 p;
    CpWord word;

    fprintf(emLog, "            Monitor mode: %d\n", cpu.monitorMode);
    fprintf(emLog, "   Expanded Address mode: %d\n", (features & IsSeries800) != 0
        && (cpu.exitMode & EmFlagExpandedAddress) != 0);
    fprintf(emLog, "Enhanced Block Copy mode: %d\n\n", (features & IsSeries800) != 0
        && (cpu.exitMode & EmFlagEnhancedBlockCopy) != 0);
    
    i = 0;
    fprintf(emLog, "P       %06o  A%d %06o [%s]  B%d %06o\n",cpu.regP,   i, cpu.regA[i], readCmRegA(i), i, cpu.regB[i]); i++;
    fprintf(emLog, "RA    %08o  A%d %06o [%s]  B%d %06o\n",cpu.regRaCm,  i, cpu.regA[i], readCmRegA(i), i, cpu.regB[i]); i++;
    fprintf(emLog, "FL    %08o  A%d %06o [%s]  B%d %06o\n",cpu.regFlCm,  i, cpu.regA[i], readCmRegA(i), i, cpu.regB[i]); i++;
    fprintf(emLog, "EM    %08o  A%d %06o [%s]  B%d %06o\n",cpu.exitMode, i, cpu.regA[i], readCmRegA(i), i, cpu.regB[i]); i++;
    fprintf(emLog, "RAE   %08o  A%d %06o [%s]  B%d %06o\n",cpu.regRaEcs, i, cpu.regA[i], readCmRegA(i), i, cpu.regB[i]); i++;
    fprintf(emLog, "FLE %010o  A%d %06o [%s]  B%d %06o\n",cpu.regFlEcs,  i, cpu.regA[i], readCmRegA(i), i, cpu.regB[i]); i++;
    fprintf(emLog, "MA    %08o  A%d %06o [%s]  B%d %06o\n",cpu.regMa,    i, cpu.regA[i], readCmRegA(i), i, cpu.regB[i]); i++;
    fprintf(emLog, "                A%d %06o [%s]  B%d %06o\n\n",        i, cpu.regA[i], readCmRegA(i), i, cpu.regB[i]); i++;

    for (i = 0; i < 8; i++)
        {
        fprintf(emLog, "X%d  %020lo\n", i, cpu.regX[i]);
        }
    fputs("\n", emLog);
    p = (cpu.regP >= before) ? (cpu.regP + cpu.regRaCm) - before : cpu.regRaCm;
    for (i = 0; i <= before + after; i++)
        {
        word = cpMem[p + i];
        fprintf(emLog, "%06o: %05lo %05lo %05lo %05lo\n", p + i,
                (word >> 45) & 077777,
                (word >> 30) & 077777,
                (word >> 15) & 077777,
                 word & 077777);
        }
    }
#endif

/*--------------------------------------------------------------------------
**  Purpose:        Handle illegal instruction
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
static void cpuOpIllegal(void)
    {
    cpuStopped = TRUE;
    if (cpu.regRaCm < cpuMaxMemory)
        {
        cpMem[cpu.regRaCm] = ((CpWord)cpu.exitCondition << 48) | ((CpWord)(cpu.regP + 1) << 30);
        }

    cpu.regP = 0;

    if ((features & (HasNoCejMej | IsSeries6x00)) == 0 && !cpu.monitorMode)
        {
        /*
        **  Exchange jump to MA.
        */
        cpu.monitorMode = TRUE;
        cpuExchangeJump(cpu.regMa);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Check if CPU instruction word address is within limits.
**
**  Parameters:     Name        Description.
**                  address     RA relative address to read.
**                  location    Pointer to u32 which will contain absolute address.
**
**  Returns:        TRUE if validation failed, FALSE otherwise;
**
**------------------------------------------------------------------------*/
static bool cpuCheckOpAddress(u32 address, u32 *location)
    {
    /*
    **  Calculate absolute address.
    */
    *location = cpuAddRa(address);
    
    if (address >= cpu.regFlCm || (*location >= cpuMaxMemory && (features & HasNoCmWrap) != 0))
        {
        /*
        **  Exit mode is always selected for RNI or branch.
        */
        cpuStopped = TRUE;

        cpu.exitCondition |= EcAddressOutOfRange;
        if (cpu.regRaCm < cpuMaxMemory)
            {
            // not need for RNI or branch - how about other uses?
            if ((cpu.exitMode & EmAddressOutOfRange) != 0)    // <<<<<<<<<<< probably need more of these too
                {
                cpMem[cpu.regRaCm] = ((CpWord)cpu.exitCondition << 48) | ((CpWord)(cpu.regP) << 30);
                }
            }

        cpu.regP = 0;
    
        if ((features & (HasNoCejMej | IsSeries6x00)) == 0 && !cpu.monitorMode)
            {
            /*
            **  Exchange jump to MA.
            */
            cpu.monitorMode = TRUE;
            cpuExchangeJump(cpu.regMa);
            }

        return(TRUE);
        }

    /*
    **  Calculate absolute address with wraparound.
    */
    *location %= cpuMaxMemory;
    
    return(FALSE);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Read CPU instruction word and verify that address is
**                  within limits.
**
**  Parameters:     Name        Description.
**                  address     RA relative address to read.
**                  data        Pointer to 60 bit word which gets the data.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cpuFetchOpWord(u32 address, CpWord *data)
    {
    u32 location;

    if (cpuCheckOpAddress(address, &location))
        {
        return;
        }

    if ((features & HasInstructionStack) != 0)
        {
        int i;

        /*
        **  Check if instruction word is in stack.
        **  (optimise this by starting search from last match).
        */
        for (i = 0; i < MaxIwStack; i++)
            {
            if (cpu.iwValid[i] && cpu.iwAddress[i] == location)
                {
                *data = cpu.iwStack[i];
                break;
                }
            }

        if (i == MaxIwStack)
            {
            /*
            **  No hit, fetch the instruction from CM and enter it into the stack.
            */
            cpu.iwRank = (cpu.iwRank + 1) % MaxIwStack;
            cpu.iwAddress[cpu.iwRank] = location;
            cpu.iwStack[cpu.iwRank] = cpMem[location] & Mask60;
            cpu.iwValid[cpu.iwRank] = TRUE;
            *data = cpu.iwStack[cpu.iwRank];
            }

        if ((features & HasIStackPrefetch) != 0 && (i == MaxIwStack || i == cpu.iwRank))
            {
#if 0
            /*
            **  Prefetch two instruction words. <<<< _two_ appears to be too greedy and causes FL problems >>>>
            */
            for (i = 2; i > 0; i--)
                {
                address += 1;
                if (cpuCheckOpAddress(address, &location))
                    {
                    return;
                    }

                cpu.iwRank = (cpu.iwRank + 1) % MaxIwStack;
                cpu.iwAddress[cpu.iwRank] = location;
                cpu.iwStack[cpu.iwRank] = cpMem[location] & Mask60;
                cpu.iwValid[cpu.iwRank] = TRUE;
                }
#else
            /*
            **  Prefetch one instruction word.
            */
            address += 1;
            if (cpuCheckOpAddress(address, &location))
                {
                return;
                }

            cpu.iwRank = (cpu.iwRank + 1) % MaxIwStack;
            cpu.iwAddress[cpu.iwRank] = location;
            cpu.iwStack[cpu.iwRank] = cpMem[location] & Mask60;
            cpu.iwValid[cpu.iwRank] = TRUE;
#endif
            }
        }
    else
        {
        /*
        **  Fetch the instruction from CM.
        */
        *data = cpMem[location] & Mask60;
        }

    opOffset = 60;

    return;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Void the instruction stack unless branch target is
**                  within stack (or unconditionally if address is ~0).
**
**  Parameters:     Name        Description.
**                  branchAddr  Target location for a branch or ~0 for
**                              unconditional voiding.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cpuVoidIwStack(u32 branchAddr)
    {
    u32 location;
    int i;

    if (branchAddr != ~0)
        {
        location = cpuAddRa(branchAddr);

        for (i = 0; i < MaxIwStack; i++)
            {
            if (cpu.iwValid[i] && cpu.iwAddress[i] == location)
                {
                /*
                **  Branch target is within stack - do nothing.
                */
                return;
                }
            }
        }

    /*
    **  Branch target is NOT within stack or unconditional voiding required.
    */
    for (i = 0; i < MaxIwStack; i++)
        {
        cpu.iwValid[i] = FALSE;
        }

    cpu.iwRank = 0;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Read CPU memory and verify that address is within limits.
**
**  Parameters:     Name        Description.
**                  address     RA relative address to read.
**                  data        Pointer to 60 bit word which gets the data.
**
**  Returns:        TRUE if access failed, FALSE otherwise;
**
**------------------------------------------------------------------------*/
static bool cpuReadMem(u32 address, CpWord *data)
    {
    u32 location;

    if (address >= cpu.regFlCm)
        {
        cpu.exitCondition |= EcAddressOutOfRange;

        /*
        **  Clear the data.
        */
        *data = 0;

        if ((cpu.exitMode & EmAddressOutOfRange) != 0)
            {
            /*
            **  Exit mode selected.
            */
            cpuStopped = TRUE;

            if (cpu.regRaCm < cpuMaxMemory)
                {
                cpMem[cpu.regRaCm] = ((CpWord)cpu.exitCondition << 48) | ((CpWord)(cpu.regP + 1) << 30);
                }

            cpu.regP = 0;

            if ((features & IsSeries170) == 0)
                {
                /*
                **  All except series 170 clear the data.
                */
                *data = 0;
                }

            if ((features & (HasNoCejMej | IsSeries6x00)) == 0 && !cpu.monitorMode)
                {
                /*
                **  Exchange jump to MA.
                */
                cpu.monitorMode = TRUE;
                cpuExchangeJump(cpu.regMa);
                }

            return(TRUE);
            }

        return(FALSE);
        }

    /*
    **  Calculate absolute address.
    */
    location = cpuAddRa(address);

    /*
    **  Wrap around or fail gracefully if wrap around is disabled.
    */
    if (location >= cpuMaxMemory)
        {
        if ((features & HasNoCmWrap) != 0)
            {
            *data = (~((CpWord)0)) & Mask60;
            return(FALSE);
            }

        location %= cpuMaxMemory;
        }

    /*
    **  Fetch the data.
    */
    *data = cpMem[location] & Mask60;

    return(FALSE);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Write CPU memory and verify that address is within limits.
**
**  Parameters:     Name        Description.
**                  address     RA relative address to write.
**                  data        Pointer to 60 bit word which holds the data.
**
**  Returns:        TRUE if access failed, FALSE otherwise;
**
**------------------------------------------------------------------------*/
static bool cpuWriteMem(u32 address, CpWord *data)
    {
    u32 location;

    if (address >= cpu.regFlCm)
        {
        cpu.exitCondition |= EcAddressOutOfRange;

        if ((cpu.exitMode & EmAddressOutOfRange) != 0)
            {
            /*
            **  Exit mode selected.
            */
            cpuStopped = TRUE;

            if (cpu.regRaCm < cpuMaxMemory)
                {
                cpMem[cpu.regRaCm] = ((CpWord)cpu.exitCondition << 48) | ((CpWord)(cpu.regP + 1) << 30);
                }

            cpu.regP = 0;

            if ((features & (HasNoCejMej | IsSeries6x00)) == 0 && !cpu.monitorMode)
                {
                /*
                **  Exchange jump to MA.
                */
                cpu.monitorMode = TRUE;
                cpuExchangeJump(cpu.regMa);
                }

            return(TRUE);
            }

        return(FALSE);
        }

    /*
    **  Calculate absolute address.
    */
    location = cpuAddRa(address);

    /*
    **  Wrap around or fail gracefully if wrap around is disabled.
    */
    if (location >= cpuMaxMemory)
        {
        if ((features & HasNoCmWrap) != 0)
            {
            return(FALSE);
            }

        location %= cpuMaxMemory;
        }

    /*
    **  Store the data.
    */
    cpMem[location] = *data & Mask60;

    return(FALSE);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Implement A register sematics.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cpuRegASemantics(void)
    {
    if (opI == 0)
        {
        return;
        }

    if (opI <= 5)
        {
        /*
        **  Read semantics.
        */
        cpuReadMem(cpu.regA[opI], cpu.regX + opI);
        }
    else
        {
        /*
        **  Write semantics.
        */
        if ((cpu.exitMode & EmFlagStackPurge) != 0)
            {
            /*
            **  Instruction stack purge flag is set - do an
            **  unconditional void.
            */
            cpuVoidIwStack(~0);
            }

        cpuWriteMem(cpu.regA[opI], cpu.regX + opI);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Addition of 18 or 21 bit RA and 18 bit offset in
**                  ones-complement with subtractive adder
**
**  Parameters:     Name        Description.
**                  op          18 bit offset
**
**  Returns:        18 or 21 bit result.
**
**------------------------------------------------------------------------*/
static u32 cpuAddRa(u32 op)
    {
    if ((features & IsSeries800) != 0)
        {
        acc21 = (cpu.regRaCm & Mask21) - (~op & Mask21);
        if ((acc21 & Overflow21) != 0)
            {
            acc21 -= 1;
            }

        return(acc21 & Mask21);
        }

    acc18 = (cpu.regRaCm & Mask18) - (~op & Mask18);
    if ((acc18 & Overflow18) != 0)
        {
        acc18 -= 1;
        }

    return(acc18 & Mask18);
    }

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
static u32 cpuAdd18(u32 op1, u32 op2)
    {
    acc18 = (op1 & Mask18) - (~op2 & Mask18);
    if ((acc18 & Overflow18) != 0)
        {
        acc18 -= 1;
        }

    return(acc18 & Mask18);
    }

/*--------------------------------------------------------------------------
**  Purpose:        24 bit ones-complement addition with subtractive adder
**
**  Parameters:     Name        Description.
**                  op1         24 bit operand1
**                  op2         24 bit operand2
**
**  Returns:        24 bit result.
**
**------------------------------------------------------------------------*/
static u32 cpuAdd24(u32 op1, u32 op2)
    {
    acc24 = (op1 & Mask24) - (~op2 & Mask24);
    if ((acc24 & Overflow24) != 0)
        {
        acc24 -= 1;
        }

    return(acc24 & Mask24);
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
static u32 cpuSubtract18(u32 op1, u32 op2)
    {
    acc18 = (op1 & Mask18) - (op2 & Mask18);
    if ((acc18 & Overflow18) != 0)
        {
        acc18 -= 1;
        }

    return(acc18 & Mask18);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Transfer word to/from UEM initiated by a CPU instruction.
**
**  Parameters:     Name        Description.
**                  writeToUem  TRUE if this is a write to UEM, FALSE if
**                              this is a read.
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
static void cpuUemWord(bool writeToUem)
    {
    u32 uemAddress;

    /*
    **  Calculate source or destination addresses.
    */
    if ((cpu.exitMode & EmFlagExpandedAddress) != 0)
        {
        uemAddress = (u32)(cpu.regX[opK] & Mask24);
        }
    else
        {
        uemAddress = (u32)(cpu.regX[opK] & Mask21);
        }

    /*
    **  Check for UEM range.
    */
    if (cpu.regFlEcs <= uemAddress)
        {
        cpu.exitCondition |= EcAddressOutOfRange;
        if ((cpu.exitMode & EmAddressOutOfRange) != 0)
            {
            /*
            **  Exit mode selected.
            */
            cpuStopped = TRUE;

            if (cpu.regRaCm < cpuMaxMemory)
                {
                cpMem[cpu.regRaCm] = ((CpWord)cpu.exitCondition << 48) | ((CpWord)(cpu.regP + 1) << 30);
                }

            cpu.regP = 0;

            if ((features & (HasNoCejMej | IsSeries6x00)) == 0 && !cpu.monitorMode)
                {
                /*
                **  Exchange jump to MA.
                */
                cpu.monitorMode = TRUE;
                cpuExchangeJump(cpu.regMa);
                }
            }

        return;
        }

    /*
    **  Add base address.
    */
    uemAddress += cpu.regRaEcs;

    /*
    **  Perform the transfer.
    */
    if (writeToUem)
        {
        if (uemAddress < cpuMaxMemory && (uemAddress & (3 << 21)) == 0)
            {
            cpMem[uemAddress++] = cpu.regX[opJ] & Mask60;
            }
        }
    else
        {
        if (uemAddress >= cpuMaxMemory || (uemAddress & (3 << 21)) != 0)
            {
            /*
            **  If bits 21 or 22 are non-zero, zero Xj.
            */
            cpu.regX[opJ] = 0;
            }
        else
            {
            cpu.regX[opJ] = cpMem[uemAddress] & Mask60;
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Transfer word to/from ECS initiated by a CPU instruction.
**
**  Parameters:     Name        Description.
**                  writeToEcs  TRUE if this is a write to ECS, FALSE if
**                              this is a read.
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
static void cpuEcsWord(bool writeToEcs)
    {
    u32 ecsAddress;
    u32 flEcs;
    bool isZeroFill;
    u32 raEcs;

    /*
    **  ECS must exist.
    */
    if (extMaxMemory == 0)
        {
        cpuOpIllegal();
        return;
        }

    isZeroFill = FALSE;

    if (   (features & IsSeries800) != 0
        && (cpu.exitMode & EmFlagExpandedAddress) != 0)
        {
        ecsAddress = (u32)(cpu.regX[opK] & Mask30);
        raEcs = (u32)(cpu.regRaEcs & Mask24);
        flEcs = (u32)(cpu.regFlEcs & Mask30);
        isZeroFill = (modelType == ModelCyber865) && (((cpu.regX[opK] + cpu.regRaEcs) & (021 << 24)) != 0);
        }
    else
        {
        ecsAddress = (u32)(cpu.regX[opK] & Mask24);
        raEcs = (u32)(cpu.regRaEcs & Mask21);
        flEcs = (u32)(cpu.regFlEcs & Mask23);
        isZeroFill = (modelType == ModelCyber865) && (((cpu.regX[opK] + cpu.regRaEcs) & (01 << 21)) != 0);
        }

    /*
    **  Check for ECS range.
    */
    if (flEcs <= ecsAddress)
        {
        cpu.exitCondition |= EcAddressOutOfRange;
        if ((cpu.exitMode & EmAddressOutOfRange) != 0)
            {
            /*
            **  Exit mode selected.
            */
            cpuStopped = TRUE;

            if (cpu.regRaCm < cpuMaxMemory)
                {
                cpMem[cpu.regRaCm] = ((CpWord)cpu.exitCondition << 48) | ((CpWord)(cpu.regP + 1) << 30);
                }

            cpu.regP = 0;

            if ((features & (HasNoCejMej | IsSeries6x00)) == 0 && !cpu.monitorMode)
                {
                /*
                **  Exchange jump to MA.
                */
                cpu.monitorMode = TRUE;
                cpuExchangeJump(cpu.regMa);
                }
            }

        return;
        }

    /*
    **  Add base address.
    */
    ecsAddress += raEcs;

    /*
    **  Perform the transfer.
    */
    if (writeToEcs)
        {
        if (ecsAddress < extMaxMemory)
            {
            extMem[ecsAddress] = cpu.regX[opJ] & Mask60;
            }
        }
    else
        {
        if (isZeroFill)
            {
            /*
            **  Zero Xj, then full exit to next instruction word.
            */
            cpu.regX[opJ] = 0;
            cpu.regP = (cpu.regP + 1) & Mask18;
            cpuFetchOpWord(cpu.regP, &opWord);
            }
        else if (ecsAddress >= cpuMaxMemory)
            {
            /*
            **  Zero Xj.
            */
            cpu.regX[opJ] = 0;
            }
        else
            {
            cpu.regX[opJ] = extMem[ecsAddress] & Mask60;
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Transfer block to/from UEM initiated by a CPU instruction.
**
**  Parameters:     Name        Description.
**                  writeToUem  TRUE if this is a write to UEM, FALSE if
**                              this is a read.
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
static void cpuUemTransfer(bool writeToUem)
    {
    u32 wordCount;
    u32 uemAddress;
    u32 cmAddress;
    u32 flEcs;
    bool isFlagRegister;
    bool isZeroFill;
    u32 raEcs;

    /*
    **  Instruction must be located in the upper 30 bits.
    */
    if (opOffset != 30)
        {
        cpuOpIllegal();
        return;
        }

    isZeroFill = FALSE;     // for Cyber 865/875
    isFlagRegister = FALSE; // flag register applies to ECS/ESM only

    /*
    **  Calculate word count, source and destination addresses.
    */
    wordCount = cpuAdd18(cpu.regB[opJ], opAddress);

    if (   (features & IsSeries800) != 0
        && (cpu.exitMode & EmFlagExpandedAddress) != 0)
        {
        uemAddress = (u32)(cpu.regX[0] & Mask30);
        raEcs = (u32)(cpu.regRaEcs & Mask24);
        flEcs = (u32)(cpu.regFlEcs & Mask30);
        isFlagRegister =    (cpu.regX[0]  & (1 << 29)) != 0
                         && (cpu.regFlEcs & (1 << 29)) != 0;
        isZeroFill = (modelType == ModelCyber865) && (((cpu.regX[0] + cpu.regRaEcs) & (01 << 28)) != 0);
        }
    else
        {
        uemAddress = (u32)(cpu.regX[0] & Mask24);
        raEcs = (u32)(cpu.regRaEcs & Mask21);
        flEcs = (u32)(cpu.regFlEcs & Mask23);
        isFlagRegister =    (cpu.regX[0]  & (1 << 23)) != 0
                         && (cpu.regFlEcs & (1 << 23)) != 0;
        isZeroFill = (modelType == ModelCyber865) && (((cpu.regX[0] + cpu.regRaEcs) & (03 << 21)) != 0);
        }

    if ((cpu.exitMode & EmFlagEnhancedBlockCopy) != 0)
        {
        cmAddress = (u32)((cpu.regX[0] >> 30) & Mask21);
        }
    else
        {
        cmAddress = cpu.regA[0] & Mask18;
        }

    /*
    **  Deal with possible negative zero word count.
    */
    if (wordCount == Mask18)
        {
        wordCount = 0;
        }

    /*
    **  Check for positive word count, CM and UEM range.
    */
    if (   (wordCount & Sign18) != 0
        || cpu.regFlCm  < cmAddress  + wordCount
        || flEcs < uemAddress + wordCount
        || isFlagRegister)
        {
#if EMDEBUG
        fprintf(emLog, "   UEM AddressOutOfRange: EM %010o  FLE %010o  CM %06o  FL %08o  Words %d\n",
            uemAddress, flEcs, cmAddress, cpu.regFlCm, wordCount);
        dumpXP(020, 017);
#endif
        cpu.exitCondition |= EcAddressOutOfRange;
        if ((cpu.exitMode & EmAddressOutOfRange) != 0)
            {
            /*
            **  Exit mode selected.
            */
            cpuStopped = TRUE;

            if (cpu.regRaCm < cpuMaxMemory)
                {
                cpMem[cpu.regRaCm] = ((CpWord)cpu.exitCondition << 48) | ((CpWord)(cpu.regP + 1) << 30);
                }

            cpu.regP = 0;

            if ((features & (HasNoCejMej | IsSeries6x00)) == 0 && !cpu.monitorMode)
                {
                /*
                **  Exchange jump to MA.
                */
                cpu.monitorMode = TRUE;
                cpuExchangeJump(cpu.regMa);
                }
            }
        else
            {
            cpu.regP = (cpu.regP + 1) & Mask18;
            cpuFetchOpWord(cpu.regP, &opWord);
            }

        return;
        }

    /*
    **  Add base addresses.
    */
    cmAddress = cpuAddRa(cmAddress);
    cmAddress %= cpuMaxMemory;

    uemAddress += raEcs;

    /*
    **  Perform the transfer.
    */
    if (writeToUem)
        {
        while (wordCount--)
            {
            if (uemAddress >= cpuMaxMemory || (uemAddress & (3 << 21)) != 0)
                {
                /*
                **  If bits 21 or 22 are non-zero, error exit to lower
                **  30 bits of instruction word.
                */
                return;
                }

            cpMem[uemAddress++] = cpMem[cmAddress] & Mask60;

            /*
            **  Increment CM address.
            */
            cmAddress = cpuAdd24(cmAddress, 1);
            cmAddress %= cpuMaxMemory;
            }
        }
    else
        {
        bool takeErrorExit = FALSE;

        while (wordCount--)
            {
            if (isZeroFill || uemAddress >= cpuMaxMemory)
                {
                cpMem[cmAddress] = 0;
                takeErrorExit = TRUE;
                }
            else
                {
                cpMem[cmAddress] = cpMem[uemAddress++] & Mask60;
                }

            /*
            **  Increment CM address.
            */
            cmAddress = cpuAdd24(cmAddress, 1);
            cmAddress %= cpuMaxMemory;
            }

        if (takeErrorExit)
            {
            /*
            **  Error exit to lower 30 bits of instruction word.
            */
            return;
            }
        }

    /*
    **  Normal exit to next instruction word.
    */
    cpu.regP = (cpu.regP + 1) & Mask18;
    cpuFetchOpWord(cpu.regP, &opWord);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Transfer block to/from ECS initiated by a CPU instruction.
**
**  Parameters:     Name        Description.
**                  writeToEcs  TRUE if this is a write to ECS, FALSE if
**                              this is a read.
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
static void cpuEcsTransfer(bool writeToEcs)
    {
    u32 wordCount;
    u32 ecsAddress;
    u32 cmAddress;
    u32 flEcs;
    bool isFlagRegister;
    bool isZeroFill;
    u32 raEcs;

    /*
    **  ECS must exist and instruction must be located in the upper 30 bits.
    */
    if (extMaxMemory == 0 || opOffset != 30)
        {
        cpuOpIllegal();
        return;
        }

    isFlagRegister = FALSE;
    isZeroFill = FALSE; // for Cyber 865/875

    /*
    **  Calculate word count, source and destination addresses.
    */
    wordCount = cpuAdd18(cpu.regB[opJ], opAddress);

    if (   (features & IsSeries800) != 0
        && (cpu.exitMode & EmFlagExpandedAddress) != 0)
        {
        ecsAddress = (u32)(cpu.regX[0] & Mask30);
        raEcs = (u32)(cpu.regRaEcs & Mask24);
        flEcs = (u32)(cpu.regFlEcs & Mask30);
        isFlagRegister =    (cpu.regX[0]  & (1 << 29)) != 0
                         && (cpu.regFlEcs & (1 << 29)) != 0;
        isZeroFill = (modelType == ModelCyber865) && (((cpu.regX[0] + cpu.regRaEcs) & (021 << 24)) != 0);
        }
    else
        {
        ecsAddress = (u32)(cpu.regX[0] & Mask24);
        raEcs = (u32)(cpu.regRaEcs & Mask21);
        flEcs = (u32)(cpu.regFlEcs & Mask23);
        isFlagRegister =    (cpu.regX[0]  & (1 << 23)) != 0
                         && (cpu.regFlEcs & (1 << 23)) != 0;
        isZeroFill = (modelType == ModelCyber865) && (((cpu.regX[0] + cpu.regRaEcs) & (01 << 21)) != 0);
        }

    if (   (features & IsSeries800) != 0
        && (cpu.exitMode & EmFlagEnhancedBlockCopy) != 0)
        {
        cmAddress = (u32)((cpu.regX[0] >> 30) & Mask24);
        }
    else
        {
        cmAddress = cpu.regA[0] & Mask18;
        }

    /*
    **  Check if this is a flag register access.
    **
    **  The ECS book (60225100) says that a flag register reference occurs
    **  when bit 23 is set in the relative address AND in the ECS FL.
    **
    **  Note that the ECS RA is NOT added to the relative address.
    */
    if (isFlagRegister)
        {
        if (!cpuEcsFlagRegister(ecsAddress))
            {
            return;
            }

        /*
        **  Normal exit.
        */
        cpu.regP = (cpu.regP + 1) & Mask18;
        cpuFetchOpWord(cpu.regP, &opWord);
        return;
        }

    /*
    **  Deal with possible negative zero word count.
    */
    if (wordCount == Mask18)
        {
        wordCount = 0;
        }

    /*
    **  Check for positive word count, CM and ECS range.
    */
    if (   (wordCount & Sign18) != 0
        || cpu.regFlCm  < cmAddress  + wordCount
        || flEcs < ecsAddress + wordCount)
        {
#if EMDEBUG
        fprintf(emLog, "   ECS AddressOutOfRange: EM %010o  FLE %010o  CM %06o  FL %08o  Words %d\n",
            ecsAddress, flEcs, cmAddress, cpu.regFlCm, wordCount);
        dumpXP(020,017);
#endif
        cpu.exitCondition |= EcAddressOutOfRange;
        if ((cpu.exitMode & EmAddressOutOfRange) != 0)
            {
            /*
            **  Exit mode selected.
            */
            cpuStopped = TRUE;

            if (cpu.regRaCm < cpuMaxMemory)
                {
                cpMem[cpu.regRaCm] = ((CpWord)cpu.exitCondition << 48) | ((CpWord)(cpu.regP + 1) << 30);
                }

            cpu.regP = 0;

            if ((features & (HasNoCejMej | IsSeries6x00)) == 0 && !cpu.monitorMode)
                {
                /*
                **  Exchange jump to MA.
                */
                cpu.monitorMode = TRUE;
                cpuExchangeJump(cpu.regMa);
                }
            }
        else
            {
            cpu.regP = (cpu.regP + 1) & Mask18;
            cpuFetchOpWord(cpu.regP, &opWord);
            }

        return;
        }

    /*
    **  Add base addresses.
    */
    cmAddress = cpuAddRa(cmAddress);
    cmAddress %= cpuMaxMemory;

    ecsAddress += raEcs;

    /*
    **  Perform the transfer.
    */
    if (writeToEcs)
        {
        while (wordCount--)
            {
            if (ecsAddress >= extMaxMemory)
                {
                /*
                **  Error exit to lower 30 bits of instruction word.
                */
                return;
                }

            extMem[ecsAddress++] = cpMem[cmAddress] & Mask60;

            /*
            **  Increment CM address.
            */
            cmAddress = cpuAdd24(cmAddress, 1);
            cmAddress %= cpuMaxMemory;
            }
        }
    else
        {
        bool takeErrorExit = FALSE;

        while (wordCount--)
            {
            if (isZeroFill || ecsAddress >= extMaxMemory)
                {
                /*
                **  Zero CM, but take error exit to lower 30 bits once zeroing is finished.
                */
                cpMem[cmAddress] = 0;
                takeErrorExit = TRUE;
                }
            else
                {
                cpMem[cmAddress] = extMem[ecsAddress++] & Mask60;
                }

            /*
            **  Increment CM address.
            */
            cmAddress = cpuAdd24(cmAddress, 1);
            cmAddress %= cpuMaxMemory;
            }

        if (takeErrorExit)
            {
            /*
            **  Error exit to lower 30 bits of instruction word.
            */
            return;
            }
        }

    /*
    **  Normal exit to next instruction word.
    */
    cpu.regP = (cpu.regP + 1) & Mask18;
    cpuFetchOpWord(cpu.regP, &opWord);
    }

/*--------------------------------------------------------------------------
**  Purpose:        CMU get a byte.
**
**  Parameters:     Name        Description.
**                  address     CM word address
**                  pos         character position
**                  byte        pointer to byte
**
**  Returns:        TRUE if access failed, FALSE otherwise.
**
**------------------------------------------------------------------------*/
static bool cpuCmuGetByte(u32 address, u32 pos, u8 *byte)
    {
    u32 location;
    CpWord data;

    /*
    **  Validate access.
    */
    if (address >= cpu.regFlCm || cpu.regRaCm + address >= cpuMaxMemory)
        {
        cpu.exitCondition |= EcAddressOutOfRange;
        if ((cpu.exitMode & EmAddressOutOfRange) != 0)
            {
            /*
            **  Exit mode selected.
            */
            cpuStopped = TRUE;

            if (cpu.regRaCm < cpuMaxMemory)
                {
                cpMem[cpu.regRaCm] = ((CpWord)cpu.exitCondition << 48) | ((CpWord)(cpu.regP + 1) << 30);
                }

            cpu.regP = 0;

            if ((features & (HasNoCejMej | IsSeries6x00)) == 0 && !cpu.monitorMode)
                {
                /*
                **  Exchange jump to MA.
                */
                cpu.monitorMode = TRUE;
                cpuExchangeJump(cpu.regMa);
                }
            }

        return(TRUE);
        }

    /*
    **  Calculate absolute address with wraparound.
    */
    location = cpuAddRa(address);
    location %= cpuMaxMemory;

    /*
    **  Fetch the word.
    */
    data = cpMem[location] & Mask60;

    /*
    **  Extract and return the byte.
    */
    *byte = (u8)((data >> ((9 - pos) * 6)) & Mask6);

    return(FALSE);
    }

/*--------------------------------------------------------------------------
**  Purpose:        CMU put a byte.
**
**  Parameters:     Name        Description.
**                  address     CM word address
**                  pos         character position
**                  byte        data byte to put
**
**  Returns:        TRUE if access failed, FALSE otherwise.
**
**------------------------------------------------------------------------*/
static bool cpuCmuPutByte(u32 address, u32 pos, u8 byte)
    {
    u32 location;
    CpWord data;

    /*
    **  Validate access.
    */
    if (address >= cpu.regFlCm || cpu.regRaCm + address >= cpuMaxMemory)
        {
        cpu.exitCondition |= EcAddressOutOfRange;
        if ((cpu.exitMode & EmAddressOutOfRange) != 0)
            {
            /*
            **  Exit mode selected.
            */
            cpuStopped = TRUE;

            if (cpu.regRaCm < cpuMaxMemory)
                {
                cpMem[cpu.regRaCm] = ((CpWord)cpu.exitCondition << 48) | ((CpWord)(cpu.regP + 1) << 30);
                }

            cpu.regP = 0;

            if ((features & (HasNoCejMej | IsSeries6x00)) == 0 && !cpu.monitorMode)
                {
                /*
                **  Exchange jump to MA.
                */
                cpu.monitorMode = TRUE;
                cpuExchangeJump(cpu.regMa);
                }
            }

        return(TRUE);
        }

    /*
    **  Calculate absolute address with wraparound.
    */
    location = cpuAddRa(address);
    location %= cpuMaxMemory;

    /*
    **  Fetch the word.
    */
    data = cpMem[location] & Mask60;

    /*
    **  Mask the destination position.
    */
    data &= ~(((CpWord)Mask6) << ((9 - pos) * 6));

    /*
    **  Store byte into position
    */
    data |= (((CpWord)byte) << ((9 - pos) * 6));

    /*
    **  Store the word.
    */
    cpMem[location] = data & Mask60;

    return(FALSE);
    }

/*--------------------------------------------------------------------------
**  Purpose:        CMU move indirect.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
static void cpuCmuMoveIndirect(void)
    {
    CpWord descWord;
    u32 k1, k2;
    u32 c1, c2;
    u32 ll;
    u8 byte;
    bool failed;

    //<<<<<<<<<<<<<<<<<<<<<<<< don't forget to optimise c1 == c2 cases.

    /*
    **  Fetch the descriptor word.
    */
    opAddress = (u32)((opWord >> 30) & Mask18);
    opAddress = cpuAdd18(cpu.regB[opJ], opAddress);
    failed = cpuReadMem(opAddress, &descWord);
    if (failed)
        {
        return;
        }

    /*
    **  Decode descriptor word.
    */
    k1 = (u32)(descWord >> 30) & Mask18;
    k2 = (u32)(descWord >>  0) & Mask18;
    c1 = (u32)(descWord >> 22) & Mask4;
    c2 = (u32)(descWord >> 18) & Mask4;
    ll = (u32)((descWord >> 26) & Mask4) | (u32)((descWord >> (48 - 4)) & (Mask9 << 4));

    /*
    **  Check for address out of range.
    */
    if (c1 > 9 || c2 > 9)
        {
        cpu.exitCondition |= EcAddressOutOfRange;
        if ((cpu.exitMode & EmAddressOutOfRange) != 0)
            {
            /*
            **  Exit mode selected.
            */
            cpuStopped = TRUE;

            if (cpu.regRaCm < cpuMaxMemory)
                {
                cpMem[cpu.regRaCm] = ((CpWord)cpu.exitCondition << 48) | ((CpWord)(cpu.regP + 1) << 30);
                }

            cpu.regP = 0;

            if ((features & (HasNoCejMej | IsSeries6x00)) == 0 && !cpu.monitorMode)
                {
                /*
                **  Exchange jump to MA.
                */
                cpu.monitorMode = TRUE;
                cpuExchangeJump(cpu.regMa);
                }

            return;
            }

        /*
        **  No transfer.
        */
        ll = 0;
        }

    /*
    **  Perform the actual move.
    */
    while (ll--)
        {
        /*
        **  Transfer one byte, but abort if access fails.
        */
        if (   cpuCmuGetByte(k1, c1, &byte)
            || cpuCmuPutByte(k2, c2, byte))
            {
            if (cpuStopped) //????????????????????????
                {
                return;
                }

            /*
            **  Exit to next instruction.
            */
            break;
            }

        /*
        **  Advance addresses.
        */
        if (++c1 > 9)
            {
            c1  = 0;
            k1 += 1;
            }

        if (++c2 > 9)
            {
            c2  = 0;
            k2 += 1;
            }
        }

    /*
    **  Clear register X0 after the move.
    */
    cpu.regX[0] = 0;

    /*
    **  Normal exit to next instruction word.
    */
    cpu.regP = (cpu.regP + 1) & Mask18;
    cpuFetchOpWord(cpu.regP, &opWord);
    }

/*--------------------------------------------------------------------------
**  Purpose:        CMU move direct.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
static void cpuCmuMoveDirect(void)
    {
    u32 k1, k2;
    u32 c1, c2;
    u32 ll;
    u8 byte;

    //<<<<<<<<<<<<<<<<<<<<<<<< don't forget to optimise c1 == c2 cases.

    /*
    **  Decode opcode word.
    */
    k1 = (u32)(opWord >> 30) & Mask18;
    k2 = (u32)(opWord >>  0) & Mask18;
    c1 = (u32)(opWord >> 22) & Mask4;
    c2 = (u32)(opWord >> 18) & Mask4;
    ll = (u32)((opWord >> 26) & Mask4) | (u32)((opWord >> (48 - 4)) & (Mask3 << 4));

    /*
    **  Check for address out of range.
    */
    if (c1 > 9 || c2 > 9)
        {
        cpu.exitCondition |= EcAddressOutOfRange;
        if ((cpu.exitMode & EmAddressOutOfRange) != 0)
            {
            /*
            **  Exit mode selected.
            */
            cpuStopped = TRUE;

            if (cpu.regRaCm < cpuMaxMemory)
                {
                cpMem[cpu.regRaCm] = ((CpWord)cpu.exitCondition << 48) | ((CpWord)(cpu.regP + 1) << 30);
                }

            cpu.regP = 0;

            if ((features & (HasNoCejMej | IsSeries6x00)) == 0 && !cpu.monitorMode)
                {
                /*
                **  Exchange jump to MA.
                */
                cpu.monitorMode = TRUE;
                cpuExchangeJump(cpu.regMa);
                }

            return;
            }

        /*
        **  No transfer.
        */
        ll = 0;
        }

    /*
    **  Perform the actual move.
    */
    while (ll--)
        {
        /*
        **  Transfer one byte, but abort if access fails.
        */
        if (   cpuCmuGetByte(k1, c1, &byte)
            || cpuCmuPutByte(k2, c2, byte))
            {
            if (cpuStopped) //?????????????????????
                {
                return;
                }

            /*
            **  Exit to next instruction.
            */
            break;
            }

        /*
        **  Advance addresses.
        */
        if (++c1 > 9)
            {
            c1  = 0;
            k1 += 1;
            }

        if (++c2 > 9)
            {
            c2  = 0;
            k2 += 1;
            }
        }

    /*
    **  Clear register X0 after the move.
    */
    cpu.regX[0] = 0;

    /*
    **  Normal exit to next instruction word.
    */
    cpu.regP = (cpu.regP + 1) & Mask18;
    cpuFetchOpWord(cpu.regP, &opWord);
    }

/*--------------------------------------------------------------------------
**  Purpose:        CMU compare collated.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
static void cpuCmuCompareCollated(void)
    {
    CpWord result = 0;
    u32 k1, k2;
    u32 c1, c2;
    u32 ll;
    u32 collTable;
    u8 byte1, byte2;

    /*
    **  Decode opcode word.
    */
    k1 = (u32)(opWord >> 30) & Mask18;
    k2 = (u32)(opWord >>  0) & Mask18;
    c1 = (u32)(opWord >> 22) & Mask4;
    c2 = (u32)(opWord >> 18) & Mask4;
    ll = (u32)((opWord >> 26) & Mask4) | (u32)((opWord >> (48 - 4)) & (Mask3 << 4));

    /*
    **  Setup collating table.
    */
    collTable = cpu.regA[0];

    /*
    **  Check for addresses and collTable out of range.
    */
    if (c1 > 9 || c2 > 9 || collTable >= cpu.regFlCm || cpu.regRaCm + collTable >= cpuMaxMemory)
        {
        cpu.exitCondition |= EcAddressOutOfRange;
        if ((cpu.exitMode & EmAddressOutOfRange) != 0)
            {
            /*
            **  Exit mode selected.
            */
            cpuStopped = TRUE;

            if (cpu.regRaCm < cpuMaxMemory)
                {
                cpMem[cpu.regRaCm] = ((CpWord)cpu.exitCondition << 48) | ((CpWord)(cpu.regP + 1) << 30);
                }

            cpu.regP = 0;

            if ((features & (HasNoCejMej | IsSeries6x00)) == 0 && !cpu.monitorMode)
                {
                /*
                **  Exchange jump to MA.
                */
                cpu.monitorMode = TRUE;
                cpuExchangeJump(cpu.regMa);
                }

            return;
            }

        /*
        **  No compare.
        */
        ll = 0;
        }

    /*
    **  Perform the actual compare.
    */
    while (ll--)
        {
        /*
        **  Check the two bytes raw.
        */
        if (   cpuCmuGetByte(k1, c1, &byte1)
            || cpuCmuGetByte(k2, c2, &byte2))
            {
            if (cpuStopped) //?????????????????????
                {
                return;
                }

            /*
            **  Exit to next instruction.
            */
            break;
            }

        if (byte1 != byte2)
            {
            /*
            **  Bytes differ - check using collating table.
            */
            if (   cpuCmuGetByte(collTable + ((byte1 >> 3) & Mask3), byte1 & Mask3, &byte1)
                || cpuCmuGetByte(collTable + ((byte2 >> 3) & Mask3), byte2 & Mask3, &byte2))
                {
                if (cpuStopped) //??????????????????????
                    {
                    return;
                    }

                /*
                **  Exit to next instruction.
                */
                break;
                }

            if (byte1 != byte2)
                {
                /*
                **  Bytes differ in their collating sequence as well - terminate comparision
                **  and calculate result.
                */
                result = ll + 1;
                if (byte1 < byte2)
                    {
                    result = ~result & Mask60;
                    }

                break;
                }
            }

        /*
        **  Advance addresses.
        */
        if (++c1 > 9)
            {
            c1  = 0;
            k1 += 1;
            }

        if (++c2 > 9)
            {
            c2  = 0;
            k2 += 1;
            }
        }

    /*
    **  Store result in X0.
    */
    cpu.regX[0] = result;

    /*
    **  Normal exit to next instruction word.
    */
    cpu.regP = (cpu.regP + 1) & Mask18;
    cpuFetchOpWord(cpu.regP, &opWord);
    }

/*--------------------------------------------------------------------------
**  Purpose:        CMU compare uncollated.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
static void cpuCmuCompareUncollated(void)
    {
    CpWord result = 0;
    u32 k1, k2;
    u32 c1, c2;
    u32 ll;
    u8 byte1, byte2;

    /*
    **  Decode opcode word.
    */
    k1 = (u32)(opWord >> 30) & Mask18;
    k2 = (u32)(opWord >>  0) & Mask18;
    c1 = (u32)(opWord >> 22) & Mask4;
    c2 = (u32)(opWord >> 18) & Mask4;
    ll = (u32)((opWord >> 26) & Mask4) | (u32)((opWord >> (48 - 4)) & (Mask3 << 4));

    /*
    **  Check for address out of range.
    */
    if (c1 > 9 || c2 > 9)
        {
        cpu.exitCondition |= EcAddressOutOfRange;
        if ((cpu.exitMode & EmAddressOutOfRange) != 0)
            {
            /*
            **  Exit mode selected.
            */
            cpuStopped = TRUE;

            if (cpu.regRaCm < cpuMaxMemory)
                {
                cpMem[cpu.regRaCm] = ((CpWord)cpu.exitCondition << 48) | ((CpWord)(cpu.regP + 1) << 30);
                }

            cpu.regP = 0;

            if ((features & (HasNoCejMej | IsSeries6x00)) == 0 && !cpu.monitorMode)
                {
                /*
                **  Exchange jump to MA.
                */
                cpu.monitorMode = TRUE;
                cpuExchangeJump(cpu.regMa);
                }

            return;
            }

        /*
        **  No compare.
        */
        ll = 0;
        }

    /*
    **  Perform the actual compare.
    */
    while (ll--)
        {
        /*
        **  Check the two bytes raw.
        */
        if (   cpuCmuGetByte(k1, c1, &byte1)
            || cpuCmuGetByte(k2, c2, &byte2))
            {
            if (cpuStopped) //?????????????????
                {
                return;
                }

            /*
            **  Exit to next instruction.
            */
            break;
            }

        if (byte1 != byte2)
            {
            /*
            **  Bytes differ - terminate comparision
            **  and calculate result.
            */
            result = ll + 1;
            if (byte1 < byte2)
                {
                result = ~result & Mask60;
                }

            break;
            }

        /*
        **  Advance addresses.
        */
        if (++c1 > 9)
            {
            c1  = 0;
            k1 += 1;
            }

        if (++c2 > 9)
            {
            c2  = 0;
            k2 += 1;
            }
        }

    /*
    **  Store result in X0.
    */
    cpu.regX[0] = result;

    /*
    **  Normal exit to next instruction word.
    */
    cpu.regP = (cpu.regP + 1) & Mask18;
    cpuFetchOpWord(cpu.regP, &opWord);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Check parameter for floating point infinite and
**                  indefinite and set exit condition accordingly.
**
**  Parameters:     Name        Description.
**                  value       Floating point value to check
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cpuFloatCheck(CpWord value)
    {
    int exponent = ((int)(value >> 48)) & Mask12;

    if (exponent == 03777 || exponent == 04000)
        {
        cpu.exitCondition |= EcOperandOutOfRange;
        floatException = TRUE;
        }
    else if (exponent == 01777 || exponent == 06000)
        {
        cpu.exitCondition |= EcIndefiniteOperand;
        floatException = TRUE;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Handle floating point exception
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cpuFloatExceptionHandler(void)
    {
    if (floatException)
        {
        floatException = FALSE;

        if ((cpu.exitMode & (cpu.exitCondition << 12)) != 0)
            {
            /*
            **  Exit mode selected.
            */
            cpuStopped = TRUE;

            if (cpu.regRaCm < cpuMaxMemory)
                {
                cpMem[cpu.regRaCm] = ((CpWord)cpu.exitCondition << 48) | ((CpWord)(cpu.regP + 1) << 30);
                }

            cpu.regP = 0;

            if ((features & (HasNoCejMej | IsSeries6x00)) == 0 && !cpu.monitorMode)
                {
                /*
                **  Exchange jump to MA.
                */
                cpu.monitorMode = TRUE;
                cpuExchangeJump(cpu.regMa);
                }
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Functions to implement all opcodes
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/

static void cpOp00(void)
    {
    /*
    **  PS or Error Exit to MA.
    */
    if ((features & (HasNoCejMej | IsSeries6x00)) != 0 || cpu.monitorMode)
        {
        cpuStopped = TRUE;
        }
    else
        {
        cpuOpIllegal();
        }
    }

static void cpOp01(void)
    {
    switch (opI)
        {
    case 0:
        /*
        **  RJ  K
        */
        acc60 = ((CpWord)0400 << 48) | ((CpWord)((cpu.regP + 1) & Mask18) << 30);
        if (cpuWriteMem(opAddress, &acc60))
            {
            return;
            }

        cpu.regP = opAddress;
        opOffset = 0;

        if ((features & HasInstructionStack) != 0)
            {
            /*
            **  Void the instruction stack.
            */
            cpuVoidIwStack(~0);
            }

        break;

    case 1:
        /*
        **  REC  Bj+K
        */
        if ((cpu.exitMode & EmFlagUemEnable) != 0)
            {
            cpuUemTransfer(FALSE);
            }
        else
            {
            cpuEcsTransfer(FALSE);
            }

        if ((features & HasInstructionStack) != 0)
            {
            /*
            **  Void the instruction stack.
            */
            cpuVoidIwStack(~0);
            }

        break;

    case 2:
        /*
        **  WEC  Bj+K
        */
        if ((cpu.exitMode & EmFlagUemEnable) != 0)
            {
            cpuUemTransfer(TRUE);
            }
        else
            {
            cpuEcsTransfer(TRUE);
            }

        break;

    case 3:
        /*
        **  XJ  K
        */
        if ((features & HasNoCejMej) != 0 || opOffset != 30)
            {
            /*
            **  CEJ/MEJ must be enabled and the instruction must be in parcel 0,
            **  if not, it is interpreted as an illegal instruction.
            */
            cpuOpIllegal();
            return;
            }

        cpu.regP = (cpu.regP + 1) & Mask18;
        cpuStopped = TRUE;

        if (cpu.monitorMode)
            {
            cpu.monitorMode = FALSE;
            cpuExchangeJump(opAddress + cpu.regB[opJ]);
            }
        else
            {
            cpu.monitorMode = TRUE;
            cpuExchangeJump(cpu.regMa);
            }

        break;

    case 4:
        if (modelType != ModelCyber865)
            {
            cpuOpIllegal();
            return;
            }

        /*
        **  RXj  Xk
        */
        if ((cpu.exitMode & EmFlagUemEnable) != 0)
            {
            cpuUemWord(FALSE);
            }
        else
            {
            cpuEcsWord(FALSE);
            }

        break;

    case 5:
        if (modelType != ModelCyber865)
            {
            cpuOpIllegal();
            return;
            }

        /*
        **  WXj  Xk
        */
        if ((cpu.exitMode & EmFlagUemEnable) != 0)
            {
            cpuUemWord(TRUE);
            }
        else
            {
            cpuEcsWord(TRUE);
            }

        break;

    case 6:
        if ((features & HasMicrosecondClock) != 0)
            {
            /*
            **  RC  Xj
            */
            rtcReadUsCounter();
            cpu.regX[opJ] = rtcClock;
            }
        else
            {
            cpuOpIllegal();
            }

        break;

    case 7:
        /*
        **  7600 instruction (invalid in our context).
        */
        cpuOpIllegal();
        break;
        }
    }

static void cpOp02(void)
    {
    /*
    **  JP  Bi+K
    */
    cpu.regP = cpuAdd18(cpu.regB[opI], opAddress);

    if ((features & HasInstructionStack) != 0)
        {
        /*
        **  Void the instruction stack.
        */
        cpuVoidIwStack(~0);
        }

    cpuFetchOpWord(cpu.regP, &opWord);
    }

static void cpOp03(void)
    {
    bool jump = FALSE;

    switch (opI)
        {
    case 0:
        /*
        **  ZR  Xj K
        */
        jump = cpu.regX[opJ] == 0 || cpu.regX[opJ] == NegativeZero;
        break;

    case 1:
        /*
        **  NZ  Xj K
        */
        jump = cpu.regX[opJ] != 0 && cpu.regX[opJ] != NegativeZero;
        break;

    case 2:
        /*
        **  PL  Xj K
        */
        jump = (cpu.regX[opJ] & Sign60) == 0;
        break;

    case 3:
        /*
        **  NG  Xj K
        */
        jump = (cpu.regX[opJ] & Sign60) != 0;
        break;

    case 4:
        /*
        **  IR  Xj K
        */
        acc60 = cpu.regX[opJ] >> 48;
        jump = acc60 != 03777 && acc60 != 04000;
        break;

    case 5:
        /*
        **  OR  Xj K
        */
        acc60 = cpu.regX[opJ] >> 48;
        jump = acc60 == 03777 || acc60 == 04000;
        break;

    case 6:
        /*
        **  DF  Xj K
        */
        acc60 = cpu.regX[opJ] >> 48;
        jump = acc60 != 01777 && acc60 != 06000;
        break;

    case 7:
        /*
        **  ID  Xj K
        */
        acc60 = cpu.regX[opJ] >> 48;
        jump = acc60 == 01777 || acc60 == 06000;
        break;
        }

    if (jump)
        {
        if ((features & HasInstructionStack) != 0)
            {
            /*
            **  Void the instruction stack.
            */
            if ((cpu.exitMode & EmFlagStackPurge) != 0)
                {
                /*
                **  Instruction stack purge flag is set - do an
                **  unconditional void.
                */
                cpuVoidIwStack(~0);
                }
            else
                {
                /*
                **  Normal conditional void.
                */
                cpuVoidIwStack(opAddress);
                }
            }

        cpu.regP = opAddress;
        cpuFetchOpWord(cpu.regP, &opWord);
        }
    }

static void cpOp04(void)
    {
    /*
    **  EQ  Bi Bj K
    */
    if (cpu.regB[opI] == cpu.regB[opJ])
        {
        if ((features & HasInstructionStack) != 0)
            {
            /*
            **  Void the instruction stack.
            */
            cpuVoidIwStack(opAddress);
            }

        cpu.regP = opAddress;
        cpuFetchOpWord(cpu.regP, &opWord);
        }
    }

static void cpOp05(void)
    {
    /*
    **  NE  Bi Bj K
    */
    if (cpu.regB[opI] != cpu.regB[opJ])
        {
        if ((features & HasInstructionStack) != 0)
            {
            /*
            **  Void the instruction stack.
            */
            cpuVoidIwStack(opAddress);
            }

        cpu.regP = opAddress;
        cpuFetchOpWord(cpu.regP, &opWord);
        }
    }

static void cpOp06(void)
    {
    /*
    **  GE  Bi Bj K
    */
    i32 signDiff = (cpu.regB[opI] & Sign18) - (cpu.regB[opJ] & Sign18);
    if (signDiff > 0)
        {
        return;
        }

    if (signDiff == 0)
        {
        acc18 = (cpu.regB[opI] & Mask18) - (cpu.regB[opJ] & Mask18);
        if ((acc18 & Overflow18) != 0 && (acc18 & Mask18) != 0)
            {
            acc18 -= 1;
            }

        if ((acc18 & Sign18) != 0)
            {
            return;
            }
        }

    if ((features & HasInstructionStack) != 0)
        {
        /*
        **  Void the instruction stack.
        */
        cpuVoidIwStack(opAddress);
        }

    cpu.regP = opAddress;
    cpuFetchOpWord(cpu.regP, &opWord);
    }

static void cpOp07(void)
    {
    /*
    **  LT  Bi Bj K
    */
    i32 signDiff = (cpu.regB[opI] & Sign18) - (cpu.regB[opJ] & Sign18);
    if (signDiff < 0)
        {
        return;
        }

    if (signDiff == 0)
        {
        acc18 = (cpu.regB[opI] & Mask18) - (cpu.regB[opJ] & Mask18);
        if ((acc18 & Overflow18) != 0 && (acc18 & Mask18) != 0)
            {
            acc18 -= 1;
            }

        if ((acc18 & Sign18) == 0 || acc18 == 0)
            {
            return;
            }
        }

    if ((features & HasInstructionStack) != 0)
        {
        /*
        **  Void the instruction stack.
        */
        cpuVoidIwStack(opAddress);
        }

    cpu.regP = opAddress;
    cpuFetchOpWord(cpu.regP, &opWord);
    }

static void cpOp10(void)
    {
    /*
    **  BXi Xj
    */
    cpu.regX[opI] = cpu.regX[opJ] & Mask60;
    }

static void cpOp11(void)
    {
    /*
    **  BXi Xj*Xk
    */
    cpu.regX[opI] = (cpu.regX[opJ] & cpu.regX[opK]) & Mask60;
    }

static void cpOp12(void)
    {
    /*
    **  BXi Xj+Xk
    */
    cpu.regX[opI] = (cpu.regX[opJ] | cpu.regX[opK]) & Mask60;
    }

static void cpOp13(void)
    {
    /*
    **  BXi Xj-Xk
    */
    cpu.regX[opI] = (cpu.regX[opJ] ^ cpu.regX[opK]) & Mask60;
    }

static void cpOp14(void)
    {
    /*
    **  BXi -Xj
    */
    cpu.regX[opI] = ~cpu.regX[opK] & Mask60;
    }

static void cpOp15(void)
    {
    /*
    **  BXi -Xk*Xj
    */
    cpu.regX[opI] = (cpu.regX[opJ] & ~cpu.regX[opK]) & Mask60;
    }

static void cpOp16(void)
    {
    /*
    **  BXi -Xk+Xj
    */
    cpu.regX[opI] = (cpu.regX[opJ] | ~cpu.regX[opK]) & Mask60;
    }

static void cpOp17(void)
    {
    /*
    **  BXi -Xk-Xj
    */
    cpu.regX[opI] = (cpu.regX[opJ] ^ ~cpu.regX[opK]) & Mask60;
    }

static void cpOp20(void)
    {
    /*
    **  LXi jk
    */
    u8 jk;

    jk = (u8)((opJ << 3) | opK);
    cpu.regX[opI] = shiftLeftCircular(cpu.regX[opI] & Mask60, jk);
    }

static void cpOp21(void)
    {
    /*
    **  AXi jk
    */
    u8 jk;

    jk = (u8)((opJ << 3) | opK);
    cpu.regX[opI] = shiftRightArithmetic(cpu.regX[opI] & Mask60, jk);
    }

static void cpOp22(void)
    {
    /*
    **  LXi Bj Xk
    */
    u32 count;

    count = cpu.regB[opJ] & Mask18;
    acc60 = cpu.regX[opK] & Mask60;

    if ((count & Sign18) == 0)
        {
        count &= Mask6;
        cpu.regX[opI] = shiftLeftCircular(acc60, count);
        }
    else
        {
        count = ~count;
        count &= Mask11;
        if ((count & ~Mask6) != 0)
            {
            cpu.regX[opI] = 0;
            }
        else
            {
            cpu.regX[opI] = shiftRightArithmetic(acc60, count);
            }
        }
    }

static void cpOp23(void)
    {
    /*
    **  AXi Bj Xk
    */
    u32 count;

    count = cpu.regB[opJ] & Mask18;
    acc60 = cpu.regX[opK] & Mask60;

    if ((count & Sign18) == 0)
        {
        count &= Mask11;
        if ((count & ~Mask6) != 0)
            {
            cpu.regX[opI] = 0;
            }
        else
            {
            cpu.regX[opI] = shiftRightArithmetic(acc60, count);
            }
        }
    else
        {
        count = ~count;
        count &= Mask6;
        cpu.regX[opI] = shiftLeftCircular(acc60, count);
        }
    }

static void cpOp24(void)
    {
    /*
    **  NXi Bj Xk
    */
    cpuFloatCheck(cpu.regX[opK]);
    cpu.regX[opI] = shiftNormalize(cpu.regX[opK], &cpu.regB[opJ], FALSE);
    cpuFloatExceptionHandler();
    }

static void cpOp25(void)
    {
    /*
    **  ZXi Bj Xk
    */
    cpuFloatCheck(cpu.regX[opK]);
    cpu.regX[opI] = shiftNormalize(cpu.regX[opK], &cpu.regB[opJ], TRUE);
    cpuFloatExceptionHandler();
    }

static void cpOp26(void)
    {
    /*
    **  UXi Bj Xk
    */
    if (opJ == 0)
        {
        cpu.regX[opI] = shiftUnpack(cpu.regX[opK], NULL);
        }
    else
        {
        cpu.regX[opI] = shiftUnpack(cpu.regX[opK], &cpu.regB[opJ]);
        }
    }

static void cpOp27(void)
    {
    /*
    **  PXi Bj Xk
    */
    if (opJ == 0)
        {
        cpu.regX[opI] = shiftPack(cpu.regX[opK], 0);
        }
    else
        {
        cpu.regX[opI] = shiftPack(cpu.regX[opK], cpu.regB[opJ]);
        }
    }

static void cpOp30(void)
    {
    /*
    **  FXi Xj+Xk
    */
    cpuFloatCheck(cpu.regX[opJ]);
    cpuFloatCheck(cpu.regX[opK]);
    cpu.regX[opI] = floatAdd(cpu.regX[opJ], cpu.regX[opK], FALSE, FALSE);
    cpuFloatExceptionHandler();
    }

static void cpOp31(void)
    {
    /*
    **  FXi Xj-Xk
    */
    cpuFloatCheck(cpu.regX[opJ]);
    cpuFloatCheck(cpu.regX[opK]);
    cpu.regX[opI] = floatAdd(cpu.regX[opJ], (~cpu.regX[opK] & Mask60), FALSE, FALSE);
    cpuFloatExceptionHandler();
    }

static void cpOp32(void)
    {
    /*
    **  DXi Xj+Xk
    */
    cpuFloatCheck(cpu.regX[opJ]);
    cpuFloatCheck(cpu.regX[opK]);
    cpu.regX[opI] = floatAdd(cpu.regX[opJ], cpu.regX[opK], FALSE, TRUE);
    cpuFloatExceptionHandler();
    }

static void cpOp33(void)
    {
    /*
    **  DXi Xj-Xk
    */
    cpuFloatCheck(cpu.regX[opJ]);
    cpuFloatCheck(cpu.regX[opK]);
    cpu.regX[opI] = floatAdd(cpu.regX[opJ], (~cpu.regX[opK] & Mask60), FALSE, TRUE);
    cpuFloatExceptionHandler();
    }

static void cpOp34(void)
    {
    /*
    **  RXi Xj+Xk
    */
    cpuFloatCheck(cpu.regX[opJ]);
    cpuFloatCheck(cpu.regX[opK]);
    cpu.regX[opI] = floatAdd(cpu.regX[opJ], cpu.regX[opK], TRUE, FALSE);
    cpuFloatExceptionHandler();
    }

static void cpOp35(void)
    {
    /*
    **  RXi Xj-Xk
    */
    cpuFloatCheck(cpu.regX[opJ]);
    cpuFloatCheck(cpu.regX[opK]);
    cpu.regX[opI] = floatAdd(cpu.regX[opJ], (~cpu.regX[opK] & Mask60), TRUE, FALSE);
    cpuFloatExceptionHandler();
    }

static void cpOp36(void)
    {
    /*
    **  IXi Xj+Xk
    */
    acc60 = (cpu.regX[opJ] & Mask60) - (~cpu.regX[opK] & Mask60);
    if ((acc60 & Overflow60) != 0)
        {
        acc60 -= 1;
        }

    cpu.regX[opI] = acc60 & Mask60;
    }

static void cpOp37(void)
    {
    /*
    **  IXi Xj-Xk
    */
    acc60 = (cpu.regX[opJ] & Mask60) - (cpu.regX[opK] & Mask60);
    if ((acc60 & Overflow60) != 0)
        {
        acc60 -= 1;
        }

    cpu.regX[opI] = acc60 & Mask60;
    }

static void cpOp40(void)
    {
    /*
    **  FXi Xj*Xk
    */
    cpuFloatCheck(cpu.regX[opJ]);
    cpuFloatCheck(cpu.regX[opK]);
    cpu.regX[opI] = floatMultiply(cpu.regX[opJ], cpu.regX[opK], FALSE, FALSE);
    cpuFloatExceptionHandler();
    }

static void cpOp41(void)
    {
    /*
    **  RXi Xj*Xk
    */
    cpuFloatCheck(cpu.regX[opJ]);
    cpuFloatCheck(cpu.regX[opK]);
    cpu.regX[opI] = floatMultiply(cpu.regX[opJ], cpu.regX[opK], TRUE, FALSE);
    cpuFloatExceptionHandler();
    }

static void cpOp42(void)
    {
    /*
    **  DXi Xj*Xk
    */
    cpuFloatCheck(cpu.regX[opJ]);
    cpuFloatCheck(cpu.regX[opK]);
    cpu.regX[opI] = floatMultiply(cpu.regX[opJ], cpu.regX[opK], FALSE, TRUE);
    cpuFloatExceptionHandler();
    }

static void cpOp43(void)
    {
    /*
    **  MXi jk
    */
    u8 jk;

    jk = (u8)((opJ << 3) | opK);
    cpu.regX[opI] = shiftMask(jk);
    }

static void cpOp44(void)
    {
    /*
    **  FXi Xj/Xk
    */
    cpuFloatCheck(cpu.regX[opJ]);
    cpuFloatCheck(cpu.regX[opK]);
    cpu.regX[opI] = floatDivide(cpu.regX[opJ], cpu.regX[opK], FALSE);
    cpuFloatExceptionHandler();
#if CcSMM_EJT
    skipStep = 20;
#endif
    }

static void cpOp45(void)
    {
    /*
    **  RXi Xj/Xk
    */
    cpuFloatCheck(cpu.regX[opJ]);
    cpuFloatCheck(cpu.regX[opK]);
    cpu.regX[opI] = floatDivide(cpu.regX[opJ], cpu.regX[opK], TRUE);
    cpuFloatExceptionHandler();
    }

static void cpOp46(void)
    {
    switch (opI)
        {
    default:
        /*
        **  NO (pass).
        */
        return;

    case 4:
    case 5:
    case 6:
    case 7:
        if ((features & HasCMU) == 0)
            {
            cpuOpIllegal();
            return;
            }

        if (opOffset != 45)
            {
            if ((features & IsSeries70) == 0)
                {
                /*
                **  Instruction must be in parcel 0, if not, it is interpreted as a
                **  pass instruction (NO) on Cyber 70 series or as illegal on anything
                **  else.
                */
                cpuOpIllegal();
                }

            return;
            }
        break;
        }

    switch (opI)
        {
    case 4:
        /*
        **  Move indirect.
        */
        cpuCmuMoveIndirect();
        break;

    case 5:
        /*
        **  Move direct.
        */
        cpuCmuMoveDirect();
        break;

    case 6:
        /*
        **  Compare collated.
        */
        cpuCmuCompareCollated();
        break;

    case 7:
        /*
        **  Compare uncollated.
        */
        cpuCmuCompareUncollated();
        break;
        }
    }

static void cpOp47(void)
    {
    /*
    **  CXi Xk
    */
    acc60 = cpu.regX[opK] & Mask60;
    acc60 = ((acc60 & 0xAAAAAAAAAAAAAAAA) >>  1) + (acc60 & 0x5555555555555555);
    acc60 = ((acc60 & 0xCCCCCCCCCCCCCCCC) >>  2) + (acc60 & 0x3333333333333333);
    acc60 = ((acc60 & 0xF0F0F0F0F0F0F0F0) >>  4) + (acc60 & 0x0F0F0F0F0F0F0F0F);
    acc60 = ((acc60 & 0xFF00FF00FF00FF00) >>  8) + (acc60 & 0x00FF00FF00FF00FF);
    acc60 = ((acc60 & 0xFFFF0000FFFF0000) >> 16) + (acc60 & 0x0000FFFF0000FFFF);
    acc60 = ((acc60 & 0xFFFFFFFF00000000) >> 32) + (acc60 & 0x00000000FFFFFFFF);
    cpu.regX[opI] = acc60 & Mask60;
    }

static void cpOp50(void)
    {
    /*
    **  SAi Aj+K
    */
    cpu.regA[opI] = cpuAdd18(cpu.regA[opJ], opAddress);

    cpuRegASemantics();
    }

static void cpOp51(void)
    {
    /*
    **  SAi Bj+K
    */
    cpu.regA[opI] = cpuAdd18(cpu.regB[opJ], opAddress);

    cpuRegASemantics();
    }

static void cpOp52(void)
    {
    /*
    **  SAi Xj+K
    */
    cpu.regA[opI] = cpuAdd18((u32)cpu.regX[opJ], opAddress);

    cpuRegASemantics();
    }

static void cpOp53(void)
    {
    /*
    **  SAi Xj+Bk
    */
    cpu.regA[opI] = cpuAdd18((u32)cpu.regX[opJ], cpu.regB[opK]);

    cpuRegASemantics();
    }

static void cpOp54(void)
    {
    /*
    **  SAi Aj+Bk
    */
    cpu.regA[opI] = cpuAdd18(cpu.regA[opJ], cpu.regB[opK]);

    cpuRegASemantics();
    }

static void cpOp55(void)
    {
    /*
    **  SAi Aj-Bk
    */
    cpu.regA[opI] = cpuSubtract18(cpu.regA[opJ], cpu.regB[opK]);

    cpuRegASemantics();
    }

static void cpOp56(void)
    {
    /*
    **  SAi Bj+Bk
    */
    cpu.regA[opI] = cpuAdd18(cpu.regB[opJ], cpu.regB[opK]);

    cpuRegASemantics();
    }

static void cpOp57(void)
    {
    /*
    **  SAi Bj-Bk
    */
    cpu.regA[opI] = cpuSubtract18(cpu.regB[opJ], cpu.regB[opK]);

    cpuRegASemantics();
    }

static void cpOp60(void)
    {
    /*
    **  SBi Aj+K
    */
    cpu.regB[opI] = cpuAdd18(cpu.regA[opJ], opAddress);
    }

static void cpOp61(void)
    {
    /*
    **  SBi Bj+K
    */
    cpu.regB[opI] = cpuAdd18(cpu.regB[opJ], opAddress);
    }

static void cpOp62(void)
    {
    /*
    **  SBi Xj+K
    */
    cpu.regB[opI] = cpuAdd18((u32)cpu.regX[opJ], opAddress);
    }

static void cpOp63(void)
    {
    /*
    **  SBi Xj+Bk
    */
    cpu.regB[opI] = cpuAdd18((u32)cpu.regX[opJ], cpu.regB[opK]);
    }

static void cpOp64(void)
    {
    /*
    **  SBi Aj+Bk
    */
    cpu.regB[opI] = cpuAdd18(cpu.regA[opJ], cpu.regB[opK]);
    }

static void cpOp65(void)
    {
    /*
    **  SBi Aj-Bk
    */
    cpu.regB[opI] = cpuSubtract18(cpu.regA[opJ], cpu.regB[opK]);
    }

static void cpOp66(void)
    {
    if (opI == 0 && (features & IsSeries800) != 0)
        {
        /*
        **  CR Xj,Xk
        */
        cpuReadMem((u32)(cpu.regX[opK]) & Mask21, cpu.regX + opJ);
        return;
        }

    /*
    **  SBi Bj+Bk
    */
    cpu.regB[opI] = cpuAdd18(cpu.regB[opJ], cpu.regB[opK]);
    }

static void cpOp67(void)
    {
    if (opI == 0 && (features & IsSeries800) != 0)
        {
        /*
        **  CW Xj,Xk
        */
        cpuWriteMem((u32)(cpu.regX[opK]) & Mask21, cpu.regX + opJ);
        return;
        }

    /*
    **  SBi Bj-Bk
    */
    cpu.regB[opI] = cpuSubtract18(cpu.regB[opJ], cpu.regB[opK]);
    }

static void cpOp70(void)
    {
    /*
    **  SXi Aj+K
    */
    acc60 = (CpWord)cpuAdd18(cpu.regA[opJ], opAddress);

    if ((acc60 & 0400000) != 0)
        {
        acc60 |= SignExtend18To60;
        }

    cpu.regX[opI] = acc60 & Mask60;
    }

static void cpOp71(void)
    {
    /*
    **  SXi Bj+K
    */
    acc60 = (CpWord)cpuAdd18(cpu.regB[opJ], opAddress);

    if ((acc60 & 0400000) != 0)
        {
        acc60 |= SignExtend18To60;
        }

    cpu.regX[opI] = acc60 & Mask60;
    }

static void cpOp72(void)
    {
    /*
    **  SXi Xj+K
    */
    acc60 = (CpWord)cpuAdd18((u32)cpu.regX[opJ], opAddress);

    if ((acc60 & 0400000) != 0)
        {
        acc60 |= SignExtend18To60;
        }

    cpu.regX[opI] = acc60 & Mask60;
    }

static void cpOp73(void)
    {
    /*
    **  SXi Xj+Bk
    */
    acc60 = (CpWord)cpuAdd18((u32)cpu.regX[opJ], cpu.regB[opK]);

    if ((acc60 & 0400000) != 0)
        {
        acc60 |= SignExtend18To60;
        }

    cpu.regX[opI] = acc60 & Mask60;
    }

static void cpOp74(void)
    {
    /*
    **  SXi Aj+Bk
    */
    acc60 = (CpWord)cpuAdd18(cpu.regA[opJ], cpu.regB[opK]);

    if ((acc60 & 0400000) != 0)
        {
        acc60 |= SignExtend18To60;
        }

    cpu.regX[opI] = acc60 & Mask60;
    }

static void cpOp75(void)
    {
    /*
    **  SXi Aj-Bk
    */
    acc60 = (CpWord)cpuSubtract18(cpu.regA[opJ], cpu.regB[opK]);


    if ((acc60 & 0400000) != 0)
        {
        acc60 |= SignExtend18To60;
        }

    cpu.regX[opI] = acc60 & Mask60;
    }

static void cpOp76(void)
    {
    /*
    **  SXi Bj+Bk
    */
    acc60 = (CpWord)cpuAdd18(cpu.regB[opJ], cpu.regB[opK]);

    if ((acc60 & 0400000) != 0)
        {
        acc60 |= SignExtend18To60;
        }

    cpu.regX[opI] = acc60 & Mask60;
    }

static void cpOp77(void)
    {
    /*
    **  SXi Bj-Bk
    */
    acc60 = (CpWord)cpuSubtract18(cpu.regB[opJ], cpu.regB[opK]);

    if ((acc60 & 0400000) != 0)
        {
        acc60 |= SignExtend18To60;
        }

    cpu.regX[opI] = acc60 & Mask60;
    }

/*---------------------------  End Of File  ------------------------------*/
