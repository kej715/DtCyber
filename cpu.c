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

#define DEBUG_DDP 0
#define DEBUG_ECS 0
#define DEBUG_UEM 0

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
#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

/*
**  -----------------
**  Private Constants
**  -----------------
*/

/* Only enable this for testing to pass section 4.A of EJT (divide break-in test) */
#define CcSMM_EJT              0

/*
**  CPU exit conditions.
*/
#define EcNone                 00
#define EcAddressOutOfRange    01
#define EcOperandOutOfRange    02
#define EcIndefiniteOperand    04

/*
**  ECS bank size taking into account the 5k reserve.
*/
#define EcsBankSize            (131072 - 5120)
#define EsmBankSize            131072

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
    void (*execute)(CpuContext *activeCpu);
    u8 length;
    } OpDispatch;

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static void cpuCreateThread(int cpuNum);

#if defined(_WIN32)
static void cpuThread(void *param);
static void cpuAcquireMutex(HANDLE *mutexp);
static void cpuReleaseMutex(HANDLE *mutexp);

#else
static void *cpuThread(void *param);
static void cpuAcquireMutex(pthread_mutex_t *mutexp);
static void cpuReleaseMutex(pthread_mutex_t *mutexp);

#endif

static u32  cpuAdd18(u32 op1, u32 op2);
static u32  cpuAdd24(u32 op1, u32 op2);
static u32  cpuAddRa(CpuContext *activeCpu, u32 op);
static void cpuCmuCompareCollated(CpuContext *activeCpu);
static void cpuCmuCompareUncollated(CpuContext *activeCpu);
static bool cpuCmuGetByte(CpuContext *activeCpu, u32 address, u32 pos, u8 *byte);
static void cpuCmuMoveDirect(CpuContext *activeCpu);
static void cpuCmuMoveIndirect(CpuContext *activeCpu);
static bool cpuCmuPutByte(CpuContext *activeCpu, u32 address, u32 pos, u8 byte);
static void cpuEcsTransfer(CpuContext *activeCpu, bool writeToEcs);
static void cpuEcsWord(CpuContext *activeCpu, bool writeToEcs);
static void cpuExchangeJump(CpuContext *activeCpu, u32 address, bool doChangeMode);
static void cpuFetchOpWord(CpuContext *activeCpu);
static void cpuFloatCheck(CpuContext *activeCpu, CpWord value);
static void cpuFloatExceptionHandler(CpuContext *activeCpu);
static void cpuOpIllegal(CpuContext *activeCpu);
static bool cpuReadMem(CpuContext *activeCpu, u32 address, CpWord *data);
static void cpuRegASemantics(CpuContext *activeCpu);
static u32  cpuSubtract18(u32 op1, u32 op2);
static void cpuUemTransfer(CpuContext *activeCpu, bool writeToUem);
static void cpuUemWord(CpuContext *activeCpu, bool writeToUem);
static void cpuVoidIwStack(CpuContext *activeCpu, u32 branchAddr);
static bool cpuWriteMem(CpuContext *activeCpu, u32 address, CpWord *data);

static void cpOp00(CpuContext *activeCpu);
static void cpOp01(CpuContext *activeCpu);
static void cpOp02(CpuContext *activeCpu);
static void cpOp03(CpuContext *activeCpu);
static void cpOp04(CpuContext *activeCpu);
static void cpOp05(CpuContext *activeCpu);
static void cpOp06(CpuContext *activeCpu);
static void cpOp07(CpuContext *activeCpu);
static void cpOp10(CpuContext *activeCpu);
static void cpOp11(CpuContext *activeCpu);
static void cpOp12(CpuContext *activeCpu);
static void cpOp13(CpuContext *activeCpu);
static void cpOp14(CpuContext *activeCpu);
static void cpOp15(CpuContext *activeCpu);
static void cpOp16(CpuContext *activeCpu);
static void cpOp17(CpuContext *activeCpu);
static void cpOp20(CpuContext *activeCpu);
static void cpOp21(CpuContext *activeCpu);
static void cpOp22(CpuContext *activeCpu);
static void cpOp23(CpuContext *activeCpu);
static void cpOp24(CpuContext *activeCpu);
static void cpOp25(CpuContext *activeCpu);
static void cpOp26(CpuContext *activeCpu);
static void cpOp27(CpuContext *activeCpu);
static void cpOp30(CpuContext *activeCpu);
static void cpOp31(CpuContext *activeCpu);
static void cpOp32(CpuContext *activeCpu);
static void cpOp33(CpuContext *activeCpu);
static void cpOp34(CpuContext *activeCpu);
static void cpOp35(CpuContext *activeCpu);
static void cpOp36(CpuContext *activeCpu);
static void cpOp37(CpuContext *activeCpu);
static void cpOp40(CpuContext *activeCpu);
static void cpOp41(CpuContext *activeCpu);
static void cpOp42(CpuContext *activeCpu);
static void cpOp43(CpuContext *activeCpu);
static void cpOp44(CpuContext *activeCpu);
static void cpOp45(CpuContext *activeCpu);
static void cpOp46(CpuContext *activeCpu);
static void cpOp47(CpuContext *activeCpu);
static void cpOp50(CpuContext *activeCpu);
static void cpOp51(CpuContext *activeCpu);
static void cpOp52(CpuContext *activeCpu);
static void cpOp53(CpuContext *activeCpu);
static void cpOp54(CpuContext *activeCpu);
static void cpOp55(CpuContext *activeCpu);
static void cpOp56(CpuContext *activeCpu);
static void cpOp57(CpuContext *activeCpu);
static void cpOp60(CpuContext *activeCpu);
static void cpOp61(CpuContext *activeCpu);
static void cpOp62(CpuContext *activeCpu);
static void cpOp63(CpuContext *activeCpu);
static void cpOp64(CpuContext *activeCpu);
static void cpOp65(CpuContext *activeCpu);
static void cpOp66(CpuContext *activeCpu);
static void cpOp67(CpuContext *activeCpu);
static void cpOp70(CpuContext *activeCpu);
static void cpOp71(CpuContext *activeCpu);
static void cpOp72(CpuContext *activeCpu);
static void cpOp73(CpuContext *activeCpu);
static void cpOp74(CpuContext *activeCpu);
static void cpOp75(CpuContext *activeCpu);
static void cpOp76(CpuContext *activeCpu);
static void cpOp77(CpuContext *activeCpu);

/*
**  ----------------
**  Public Variables
**  ----------------
*/
u32          cpuMaxMemory;
CpWord       *cpMem;
int          cpuCount = 1;
CpuContext   *cpus;
u32          extMaxMemory;
CpWord       *extMem;
ExtMemory    extMemType = ECS;

/*
**  -----------------
**  Private Variables
**  -----------------
*/
static FILE   *cmHandle;
static FILE   *ecsHandle;

static volatile u32 ecsFlagRegister = 0;
static volatile u8 ecs16Kx4bitFlagRegisters[16384];

static volatile int monitorCpu = -1;

#if CcSMM_EJT
static int skipStep = 0;
#endif

#if DEBUG_ECS || DEBUG_UEM || DEBUG_DDP
static FILE *emLog = NULL;
#endif

#if defined(_WIN32)
static HANDLE exchangeMutex;
static HANDLE flagRegMutex;
#else
static pthread_mutex_t exchangeMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t flagRegMutex = PTHREAD_MUTEX_INITIALIZER;
#endif

/*
**  Opcode decode and dispatch table.
*/
static OpDispatch decodeCpuOpcode[] =
    {
    cpOp00, 15,
    cpOp01,  0,
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
    int cpuNum;
    u32 extBanksSize = 0;
    int i;

    /*
    **  Allocate configured central memory.
    */
    cpMem = calloc(memory, sizeof(CpWord));
    if (cpMem == NULL)
        {
        fprintf(stderr, "(cpu    ) Failed to allocate CPU memory\n");
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
        fprintf(stderr, "(cpu    ) Failed to allocate ECS memory\n");
        exit(1);
        }

    extMaxMemory = emBanks * extBanksSize;
    extMemType   = emType;

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
                printf("(cpu    ) Unexpected length of CM backing file, clearing CM\n");
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
                fprintf(stderr, "(cpu    ) Failed to create CM backing file\n");
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
                printf("(cpu    ) Unexpected length of ECS backing file, clearing ECS\n");
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
                fprintf(stderr, "(cpu    ) Failed to create ECS backing file\n");
                exit(1);
                }
            }
        }

    /*
    **  Initialize CPU(s)
    */
    cpus = (CpuContext *)calloc(cpuCount, sizeof(CpuContext));
    if (cpus == NULL)
        {
        fputs("(cpu    ) Failed to allocate memory for CPU contexts\n", stderr);
        exit(1);
        }
    for (cpuNum = 0; cpuNum < cpuCount; cpuNum++)
        {
        cpus[cpuNum].id                   = cpuNum;
        cpus[cpuNum].isStopped            = TRUE;
        cpus[cpuNum].ppRequestingExchange = -1;
        cpus[cpuNum].idleCycles = 0;
        if (cpuNum > 0)
            {
            cpuCreateThread(cpuNum);
            }
        }

    /*
    **  Initialize 16K x 4-bit EM flag registers. Currently, only models 865 and 875
    **  have this feature.
    */

    for (i = 0; i < sizeof(ecs16Kx4bitFlagRegisters); i++)
         ecs16Kx4bitFlagRegisters[i] = 0;

    /*
    **  Print a friendly message.
    */
    printf("(cpu    ) CPU model %s initialised (%d CPU%s, CM: %o, ECS: %o)\n",
           model, cpuCount, cpuCount > 1 ? "'s" : "", cpuMaxMemory, extMaxMemory);
#if DEBUG_ECS || DEBUG_UEM || DEBUG_DDP
    if (emLog == NULL)
        {
        emLog = fopen("emlog.txt", "wt");
        }
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Acquire lock on exchange mutex
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cpuAcquireExchangeMutex(void)
    {
    cpuAcquireMutex(&exchangeMutex);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Return CPU P register.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
u32 cpuGetP(u8 cpuNum)
    {
    if (cpuNum >= cpuCount)
        {
        cpuNum = 0;
        }
    return((cpus[cpuNum].regP) & Mask18);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Release lock on exchange mutex
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cpuReleaseExchangeMutex(void)
    {
    cpuReleaseMutex(&exchangeMutex);
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
            fprintf(stderr, "(cpu    ) Error writing CM backing file\n");
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
            fprintf(stderr, "(cpu    ) Error writing ECS backing file\n");
            }

        fclose(ecsHandle);
        }
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
        *data    = cpMem[address] & Mask60;
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
        address       %= cpuMaxMemory;
        cpMem[address] = data & Mask60;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Execute next instruction in the CPU.
**
**  Parameters:     Name        Description.
**                  activeCpu   pointer to CPU context
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cpuStep(CpuContext *activeCpu)
    {
    u32 length;
    u32 oldRegP;

    /*
    **  If this CPU needs to be exchanged, do that first.
    **  This check must come BEFORE the "stopped" check.
    */
    if (activeCpu->ppRequestingExchange != -1)
        {
        cpuAcquireExchangeMutex();
        if ((monitorCpu == -1 || activeCpu->doChangeMode == FALSE)
            && (activeCpu->opOffset == 60 || activeCpu->isStopped))
            {
            cpuExchangeJump(activeCpu, activeCpu->ppExchangeAddress, activeCpu->doChangeMode);
            activeCpu->ppRequestingExchange = -1;
            cpuReleaseExchangeMutex();
            }
        else
            {
            cpuReleaseExchangeMutex();
            }
        }

    if (activeCpu->isStopped)
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
    activeCpu->isErrorExitPending = FALSE;
    do
        {
        /*
        **  Decode based on type.
        */
        activeCpu->opFm = (u8)((activeCpu->opWord >> (activeCpu->opOffset - 6)) & Mask6);
        activeCpu->opI  = (u8)((activeCpu->opWord >> (activeCpu->opOffset -  9)) & Mask3);
        activeCpu->opJ  = (u8)((activeCpu->opWord >> (activeCpu->opOffset - 12)) & Mask3);
        length = decodeCpuOpcode[activeCpu->opFm].length;

        if (length == 0)
            {
            length = cpOp01Length[activeCpu->opI];
            }

        if (length == 15)
            {
            activeCpu->opK       = (u8)((activeCpu->opWord >> (activeCpu->opOffset - 15)) & Mask3);
            activeCpu->opAddress = 0;
            activeCpu->opOffset -= 15;
            }
        else
            {
            if (activeCpu->opOffset == 15)
                {
                /*
                **  Invalid packing is handled as illegal instruction.
                */
                cpuOpIllegal(activeCpu);
                break;
                }
            activeCpu->opK       = 0;
            activeCpu->opAddress = (u32)((activeCpu->opWord >> (activeCpu->opOffset - 30)) & Mask18);
            activeCpu->opOffset -= 30;
            }

        oldRegP = activeCpu->regP;

        /*
        **  Force B0 to 0.
        */
        activeCpu->regB[0] = 0;

        /*
        **  Execute instruction.
        */
        decodeCpuOpcode[activeCpu->opFm].execute(activeCpu);

        /*
        **  Force B0 to 0.
        */
        activeCpu->regB[0] = 0;

#if CcDebug == 1
        traceCpu(activeCpu, oldRegP, activeCpu->opFm, activeCpu->opI, activeCpu->opJ, activeCpu->opK, activeCpu->opAddress);
#endif

        if (activeCpu->isStopped)
            {
            if (activeCpu->opOffset == 0)
                {
                activeCpu->regP = (activeCpu->regP + 1) & Mask18;
                }
#if CcDebug == 1
            traceCpuPrint(activeCpu, "Stopped\n");
#endif

            break;
            }

        /*
        **  Fetch next instruction word if necessary.
        */
        if (activeCpu->opOffset == 0)
            {
            activeCpu->regP = (activeCpu->regP + 1) & Mask18;
            cpuFetchOpWord(activeCpu);
            }
        } while (activeCpu->opOffset != 60 && !activeCpu->isStopped);

    if (activeCpu->isErrorExitPending)
        {
        cpuAcquireExchangeMutex();
        cpuExchangeJump(activeCpu, activeCpu->regMa, TRUE);
        cpuReleaseExchangeMutex();
        }
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
    u32  flagFunction;
    u16  flagRegisterAddress;
    u32  flagWord;
    bool isExtendedFlag;
    bool result;

#if DEBUG_ECS
    fprintf(emLog, "\nECS flag register address: EM %010o", ecsAddress);
#endif

    result = TRUE;

    cpuAcquireMutex(&flagRegMutex);

    if (((ecsAddress & (1 << 29)) != 0 && (ecsAddress & (1 << 20)) != 0))
        {
        flagFunction        = (ecsAddress >> 18) & Mask5;
        flagRegisterAddress = (ecsAddress >>  4) & Mask14;
        flagWord            =  ecsAddress & Mask4;
        switch (flagFunction)
            {
        case 006:
            /*
            **  Zero/Select.
            */
#if DEBUG_ECS
            fprintf(emLog, "\n    Zero/Select: addr %05o, flag register %02o, flag word %02o",
               flagRegisterAddress, ecs16Kx4bitFlagRegisters[flagRegisterAddress], flagWord);
#endif
            if (ecs16Kx4bitFlagRegisters[flagRegisterAddress] == 0)
                {
                ecs16Kx4bitFlagRegisters[flagRegisterAddress] = flagWord;
                }
            else
                {
                /*
                **  Error exit.
                */
                result = FALSE;
                }
            break;

        case 025:
            /*
            **  Detected Error Status.
            **
            **  DtCyber doesn't currently generate or detect any errors
            **  in the ESM side door channel.
            */
#if DEBUG_ECS
            fprintf(emLog, "\n    Detected Error Status: addr %05o, flag word %02o", flagRegisterAddress, flagWord);
#endif
            break;

        case 026:
            /*
            **  Equality Status.
            */
#if DEBUG_ECS
            fprintf(emLog, "\n    Equality Status: addr %05o, flag register %02o, flag word %02o",
               flagRegisterAddress, ecs16Kx4bitFlagRegisters[flagRegisterAddress], flagWord);
#endif
            result = ecs16Kx4bitFlagRegisters[flagRegisterAddress] == flagWord;
            break;
            }
        }
    else
        {
        flagFunction = (ecsAddress >> 21) & Mask2;
        flagWord     =  ecsAddress & Mask18;
        switch (flagFunction)
            {
        case 0:
            /*
            **  Ready/Select.
            */
#if DEBUG_ECS
            fprintf(emLog, "\n    Ready/Select: flag register %06o, flag word %06o", ecsFlagRegister, flagWord);
#endif
            if ((ecsFlagRegister & flagWord) != 0)
                {
                /*
                **  Error exit.
                */
                result = FALSE;
                }
            else
                {
                ecsFlagRegister |= flagWord;
                }
            break;

        case 1:
            /*
            **  Selective Set.
            */
#if DEBUG_ECS
            fprintf(emLog, "\n    Selective Set: flag register %06o, flag word %06o", ecsFlagRegister, flagWord);
#endif
            ecsFlagRegister |= flagWord;
            break;

        case 2:
            /*
            **  Status.
            */
#if DEBUG_ECS
            fprintf(emLog, "\n    Status: flag register %06o, flag word %06o", ecsFlagRegister, flagWord);
#endif
            if ((ecsFlagRegister & flagWord) != 0)
                {
                /*
                **  Error exit.
                */
                result = FALSE;
                }
            break;

        case 3:
            /*
            **  Selective Clear,
            */
#if DEBUG_ECS
            fprintf(emLog, "\n    Selective Clear: flag register %06o, flag word %06o", ecsFlagRegister, flagWord);
#endif
            ecsFlagRegister = (ecsFlagRegister & ~flagWord) & Mask18;
            break;
            }
        }

    cpuReleaseMutex(&flagRegMutex);

    return result;
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
#if DEBUG_DDP
        fprintf(emLog, "\nDDP AddressOutOfRange on %s: EM addr %010o  EM size %010o",
            writeToEcs ? "write" : "read", ecsAddress, extMaxMemory);
#endif
        /*
        **  Abort.
        */
        return FALSE;
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
    return TRUE;
    }

/*
 **--------------------------------------------------------------------------
 **
 **  Private Functions
 **
 **--------------------------------------------------------------------------
 */

/*--------------------------------------------------------------------------
**  Purpose:        Create operator thread.
**
**  Parameters:     Name        Description.
**                  cpuNum      ordinal of CPU for which to create thread
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cpuCreateThread(int cpuNum)
    {
#if defined(_WIN32)
    DWORD  dwThreadId;
    HANDLE hThread;

    /*
    **  Create mutexes
    */
    exchangeMutex = CreateMutex(NULL, FALSE, NULL);
    flagRegMutex  = CreateMutex(NULL, FALSE, NULL);
    if (exchangeMutex == NULL || flagRegMutex == NULL)
        {
        fputs("(cpu     ) Failed to create mutex\n", stderr);
        exit(1);
        }

    /*
    **  Create operator thread.
    */
    hThread = CreateThread(
        NULL,                                       // no security attribute
        0,                                          // default stack size
        (LPTHREAD_START_ROUTINE)cpuThread,
        (CpuContext *)cpus + cpuNum,                // thread parameter
        0,                                          // not suspended
        &dwThreadId);                               // returns thread ID

    if (hThread == NULL)
        {
        fprintf(stderr, "(cpu     ) Failed to create thread for CPU %d\n", cpuNum);
        exit(1);
        }
#else
    int            rc;
    pthread_t      thread;
    pthread_attr_t attr;

    /*
    **  Create POSIX thread with default attributes.
    */
    pthread_attr_init(&attr);
    rc = pthread_create(&thread, &attr, cpuThread, cpus + cpuNum);
    if (rc < 0)
        {
        fprintf(stderr, "(cpu     ) Failed to create thread for CPU %d\n", cpuNum);
        exit(1);
        }
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Acquires a lock on a mutex
**
**  Parameters:     Name        Description.
**                  mutexp      pointer to mutex/handle
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
#if defined(_WIN32)
static void cpuAcquireMutex(HANDLE *mutexp)
    {
    WaitForSingleObject(*mutexp, INFINITE);
    }
#else
static void cpuAcquireMutex(pthread_mutex_t *mutexp)
    {
    pthread_mutex_lock(mutexp);
    }
#endif

/*--------------------------------------------------------------------------
**  Purpose:        Releases a lock on a mutex
**
**  Parameters:     Name        Description.
**                  mutexp      pointer to mutex/handle
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
#if defined(_WIN32)
static void cpuReleaseMutex(HANDLE *mutexp)
    {
    ReleaseMutex(*mutexp);
    }
#else
static void cpuReleaseMutex(pthread_mutex_t *mutexp)
    {
    pthread_mutex_unlock(mutexp);
    }
#endif

/*--------------------------------------------------------------------------
**  Purpose:        Thread execution function for a CPU
**
**  Parameters:     Name        Description.
**                  param       pointer to CPU context
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
#if defined(_WIN32)
static void cpuThread(void *param)
#else
static void *cpuThread(void *param)
#endif
    {
    CpuContext *activeCpu = (CpuContext *)param;
    printf("(cpu    ) CPU%o started\n",  activeCpu->id);

    while (emulationActive)
        {
        while (opPaused)
            {
            /* wait for operator thread to clear the flag */
            sleepMsec(500);
            }
        cpuStep(activeCpu);
        idleThrottle(activeCpu);
        }
#if !defined(_WIN32)
    return (NULL);
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Perform exchange jump.
**
**  Parameters:     Name         Description.
**                  activeCpu    Pointer to CPU context
**                  address      Exchange address
**                  doChangeMode TRUE if monitor mode flag should change
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cpuExchangeJump(CpuContext *activeCpu, u32 address, bool doChangeMode)
    {
    CpWord     *mem;
    CpuContext tmp;

    /*
    **  Only perform exchange jump on instruction boundary or when stopped.
    */
/*
    if ((activeCpu->opOffset != 60) && !activeCpu->isStopped)
        {
        return;
        }
*/

#if CcDebug == 1
    traceExchange(activeCpu, address, "Old");
#endif

    /*
    **  Clear any spurious address bits.
    */
    address &= Mask18;

    /*
    **  Verify if exchange package is within configured memory.
    */
    if (address + 020 >= cpuMaxMemory)
        {
        /*
        **  Pretend that exchange worked, but the address is bad.
        */
        return;
        }

    /*
    **  Save current context.
    */
    tmp = *activeCpu;

    /*
    **  Setup new context.
    */
    mem = cpMem + address;

    activeCpu->regP    = (u32)((*mem >> 36) & Mask18);
    activeCpu->regA[0] = (u32)((*mem >> 18) & Mask18);
    activeCpu->regB[0] = 0;

    mem        += 1;
    activeCpu->regRaCm = (u32)((*mem >> 36) & Mask24);
    activeCpu->regA[1] = (u32)((*mem >> 18) & Mask18);
    activeCpu->regB[1] = (u32)((*mem) & Mask18);

    mem        += 1;
    activeCpu->regFlCm = (u32)((*mem >> 36) & Mask24);
    activeCpu->regA[2] = (u32)((*mem >> 18) & Mask18);
    activeCpu->regB[2] = (u32)((*mem) & Mask18);

    mem         += 1;
    activeCpu->exitMode = (u32)((*mem >> 36) & Mask24);
    activeCpu->regA[3]  = (u32)((*mem >> 18) & Mask18);
    activeCpu->regB[3]  = (u32)((*mem) & Mask18);

    mem += 1;
    if (((features & IsSeries800) != 0)
        && ((activeCpu->exitMode & EmFlagExpandedAddress) != 0))
        {
        activeCpu->regRaEcs = (u32)((*mem >> 30) & Mask30Ecs);
        }
    else
        {
        activeCpu->regRaEcs = (u32)((*mem >> 36) & Mask24Ecs);
        }

    activeCpu->regA[4] = (u32)((*mem >> 18) & Mask18);
    activeCpu->regB[4] = (u32)((*mem) & Mask18);

    mem += 1;
    if (((features & IsSeries800) != 0)
        && ((activeCpu->exitMode & EmFlagExpandedAddress) != 0))
        {
        activeCpu->regFlEcs = (u32)((*mem >> 30) & Mask30Ecs);
        }
    else
        {
        activeCpu->regFlEcs = (u32)((*mem >> 36) & Mask24Ecs);
        }

    activeCpu->regA[5] = (u32)((*mem >> 18) & Mask18);
    activeCpu->regB[5] = (u32)((*mem) & Mask18);

    mem        += 1;
    activeCpu->regMa   = (u32)((*mem >> 36) & Mask24);
    activeCpu->regA[6] = (u32)((*mem >> 18) & Mask18);
    activeCpu->regB[6] = (u32)((*mem) & Mask18);

    mem         += 1;
    activeCpu->regSpare = (u32)((*mem >> 36) & Mask24);
    activeCpu->regA[7]  = (u32)((*mem >> 18) & Mask18);
    activeCpu->regB[7]  = (u32)((*mem) & Mask18);

    mem        += 1;
    activeCpu->regX[0] = *mem++ & Mask60;
    activeCpu->regX[1] = *mem++ & Mask60;
    activeCpu->regX[2] = *mem++ & Mask60;
    activeCpu->regX[3] = *mem++ & Mask60;
    activeCpu->regX[4] = *mem++ & Mask60;
    activeCpu->regX[5] = *mem++ & Mask60;
    activeCpu->regX[6] = *mem++ & Mask60;
    activeCpu->regX[7] = *mem++ & Mask60;

    activeCpu->exitCondition = EcNone;

#if CcDebug == 1
    traceExchange(activeCpu, address, "New");
#endif

    /*
    **  Save old context.
    */
    mem = cpMem + address;

    *mem++ = ((CpWord)(tmp.regP & Mask18) << 36) | ((CpWord)(tmp.regA[0] & Mask18) << 18);
    *mem++ = ((CpWord)(tmp.regRaCm & Mask24) << 36) | ((CpWord)(tmp.regA[1] & Mask18) << 18) | ((CpWord)(tmp.regB[1] & Mask18));
    *mem++ = ((CpWord)(tmp.regFlCm & Mask24) << 36) | ((CpWord)(tmp.regA[2] & Mask18) << 18) | ((CpWord)(tmp.regB[2] & Mask18));
    *mem++ = ((CpWord)(tmp.exitMode & Mask24) << 36) | ((CpWord)(tmp.regA[3] & Mask18) << 18) | ((CpWord)(tmp.regB[3] & Mask18));

    if (((features & IsSeries800) != 0)
        && ((tmp.exitMode & EmFlagExpandedAddress) != 0))
        {
        *mem++ = ((CpWord)(tmp.regRaEcs & Mask30Ecs) << 30) | ((CpWord)(tmp.regA[4] & Mask18) << 18) | ((CpWord)(tmp.regB[4] & Mask18));
        }
    else
        {
        *mem++ = ((CpWord)(tmp.regRaEcs & Mask24Ecs) << 36) | ((CpWord)(tmp.regA[4] & Mask18) << 18) | ((CpWord)(tmp.regB[4] & Mask18));
        }

    if (((features & IsSeries800) != 0)
        && ((tmp.exitMode & EmFlagExpandedAddress) != 0))
        {
        *mem++ = ((CpWord)(tmp.regFlEcs & Mask30Ecs) << 30) | ((CpWord)(tmp.regA[5] & Mask18) << 18) | ((CpWord)(tmp.regB[5] & Mask18));
        }
    else
        {
        *mem++ = ((CpWord)(tmp.regFlEcs & Mask24Ecs) << 36) | ((CpWord)(tmp.regA[5] & Mask18) << 18) | ((CpWord)(tmp.regB[5] & Mask18));
        }

    *mem++ = ((CpWord)(tmp.regMa & Mask24) << 36) | ((CpWord)(tmp.regA[6] & Mask18) << 18) | ((CpWord)(tmp.regB[6] & Mask18));
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
        cpuVoidIwStack(activeCpu, ~0);
        }

    /*
    **  Activate CPU.
    */
    activeCpu->isStopped = FALSE;

    if (doChangeMode)
        {
        activeCpu->isMonitorMode = !activeCpu->isMonitorMode;
        }
    if (activeCpu->isMonitorMode)
        {
        if (monitorCpu == -1) monitorCpu = activeCpu->id;
        }
    else if (monitorCpu == activeCpu->id)
        {
        monitorCpu = -1;
        }

    cpuFetchOpWord(activeCpu);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Print the current register set (exchange package) and
**                  a block of memory around the current P register value
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
#if DEBUG_ECS || DEBUG_UEM
static char cmRegAstr[24];

static char *readCmRegA(CpuContext *activeCpu, int regNum)
    {
    int    addr;
    CpWord word;

    addr = activeCpu->regA[regNum];

    if (addr < activeCpu->regFlCm)
        {
        word = cpMem[activeCpu->regRaCm + addr];
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

static void dumpXP(CpuContext *activeCpu, int before, int after)
    {
    u32    abs;
    int    i;
    u32    rel;
    CpWord word;

    fprintf(emLog, "\n            Monitor mode: %d", activeCpu->isMonitorMode);
    fprintf(emLog, "\n   Expanded Address mode: %d", (features & IsSeries800) != 0
            && (activeCpu->exitMode & EmFlagExpandedAddress) != 0);
    fprintf(emLog, "\nEnhanced Block Copy mode: %d\n", (features & IsSeries800) != 0
            && (activeCpu->exitMode & EmFlagEnhancedBlockCopy) != 0);

    i = 0;
    fprintf(emLog, "\nP       %06o  A%d %06o [%s]  B%d %06o", activeCpu->regP, i, activeCpu->regA[i], readCmRegA(activeCpu, i), i, activeCpu->regB[i]);
    i++;
    fprintf(emLog, "\nRA    %08o  A%d %06o [%s]  B%d %06o", activeCpu->regRaCm, i, activeCpu->regA[i], readCmRegA(activeCpu, i), i, activeCpu->regB[i]);
    i++;
    fprintf(emLog, "\nFL    %08o  A%d %06o [%s]  B%d %06o", activeCpu->regFlCm, i, activeCpu->regA[i], readCmRegA(activeCpu, i), i, activeCpu->regB[i]);
    i++;
    fprintf(emLog, "\nEM    %08o  A%d %06o [%s]  B%d %06o", activeCpu->exitMode, i, activeCpu->regA[i], readCmRegA(activeCpu, i), i, activeCpu->regB[i]);
    i++;
    fprintf(emLog, "\nRAE   %08o  A%d %06o [%s]  B%d %06o", activeCpu->regRaEcs, i, activeCpu->regA[i], readCmRegA(activeCpu, i), i, activeCpu->regB[i]);
    i++;
    fprintf(emLog, "\nFLE %010o  A%d %06o [%s]  B%d %06o", activeCpu->regFlEcs, i, activeCpu->regA[i], readCmRegA(activeCpu, i), i, activeCpu->regB[i]);
    i++;
    fprintf(emLog, "\nMA    %08o  A%d %06o [%s]  B%d %06o", activeCpu->regMa, i, activeCpu->regA[i], readCmRegA(activeCpu, i), i, activeCpu->regB[i]);
    i++;
    fprintf(emLog, "\n                A%d %06o [%s]  B%d %06o\n", i, activeCpu->regA[i], readCmRegA(activeCpu, i), i, activeCpu->regB[i]);
    i++;

    for (i = 0; i < 8; i++)
        {
        fprintf(emLog, "\nX%d  " FMT60_020o, i, activeCpu->regX[i]);
        }
    fputs("\n", emLog);
    abs = (activeCpu->regP >= before) ? (activeCpu->regP + activeCpu->regRaCm) - before : activeCpu->regRaCm;
    rel = abs - activeCpu->regRaCm;
    for (i = 0; i <= before + after; i++)
        {
        word = cpMem[abs + i];
        fprintf(emLog, "\n%06o: %05lo %05lo %05lo %05lo", rel + i,
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
static void cpuOpIllegal(CpuContext *activeCpu)
    {
    activeCpu->isStopped = TRUE;
    if (activeCpu->regRaCm < cpuMaxMemory)
        {
        cpMem[activeCpu->regRaCm] = ((CpWord)activeCpu->exitCondition << 48) | ((CpWord)(activeCpu->regP + 1) << 30);
        }

    activeCpu->regP = 0;

    if (((features & (HasNoCejMej | IsSeries6x00)) == 0) && !activeCpu->isMonitorMode)
        {
        activeCpu->isErrorExitPending = TRUE;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Check if CPU instruction word address is within limits.
**
**  Parameters:     Name        Description.
**                  activeCpu   Pointer to CPU context
**                  address     RA relative address to read.
**                  location    Pointer to u32 which will contain absolute address.
**
**  Returns:        TRUE if validation failed, FALSE otherwise;
**
**------------------------------------------------------------------------*/
static bool cpuCheckOpAddress(CpuContext *activeCpu, u32 address, u32 *location)
    {
    /*
    **  Calculate absolute address.
    */
    *location = cpuAddRa(activeCpu, address);

    if ((address >= activeCpu->regFlCm) || ((*location >= cpuMaxMemory) && ((features & HasNoCmWrap) != 0)))
        {
        /*
        **  Exit mode is always selected for RNI or branch.
        */
        activeCpu->isStopped = TRUE;
        activeCpu->exitCondition |= EcAddressOutOfRange;
        if (activeCpu->regRaCm < cpuMaxMemory)
            {
            // not need for RNI or branch - how about other uses?
            if ((activeCpu->exitMode & EmAddressOutOfRange) != 0)    // <<<<<<<<<<< probably need more of these too
                {
                cpMem[activeCpu->regRaCm] = ((CpWord)activeCpu->exitCondition << 48) | ((CpWord)(activeCpu->regP) << 30);
                }
            }

        activeCpu->regP = 0;

        if (((features & (HasNoCejMej | IsSeries6x00)) == 0) && !activeCpu->isMonitorMode)
            {
            activeCpu->isErrorExitPending = TRUE;
            }

        return (TRUE);
        }

    /*
    **  Calculate absolute address with wraparound.
    */
    *location %= cpuMaxMemory;

    return (FALSE);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Read CPU instruction word and verify that address is
**                  within limits.
**
**  Parameters:     Name        Description.
**                  activeCpu   Pointer to CPU context
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cpuFetchOpWord(CpuContext *activeCpu)
    {
    u32 location;

    if (cpuCheckOpAddress(activeCpu, activeCpu->regP, &location))
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
            if (activeCpu->iwValid[i] && (activeCpu->iwAddress[i] == location))
                {
                activeCpu->opWord = activeCpu->iwStack[i];
                break;
                }
            }

        if (i == MaxIwStack)
            {
            /*
            **  No hit, fetch the instruction from CM and enter it into the stack.
            */
            activeCpu->iwRank = (activeCpu->iwRank + 1) % MaxIwStack;
            activeCpu->iwAddress[activeCpu->iwRank] = location;
            activeCpu->iwStack[activeCpu->iwRank]   = cpMem[location] & Mask60;
            activeCpu->iwValid[activeCpu->iwRank]   = TRUE;
            activeCpu->opWord = activeCpu->iwStack[activeCpu->iwRank];
            }

        if (((features & HasIStackPrefetch) != 0) && ((i == MaxIwStack) || (i == activeCpu->iwRank)))
            {
#if 0
            /*
            **  Prefetch two instruction words. <<<< _two_ appears to be too greedy and causes FL problems >>>>
            */
            for (i = 2; i > 0; i--)
                {
                address += 1;
                if (cpuCheckOpAddress(activeCpu, address, &location))
                    {
                    return;
                    }

                activeCpu->iwRank = (activeCpu->iwRank + 1) % MaxIwStack;
                activeCpu->iwAddress[activeCpu->iwRank] = location;
                activeCpu->iwStack[activeCpu->iwRank]   = cpMem[location] & Mask60;
                activeCpu->iwValid[activeCpu->iwRank]   = TRUE;
                }
#else
            /*
            **  Prefetch one instruction word.
            */
            if (cpuCheckOpAddress(activeCpu, activeCpu->regP + 1, &location))
                {
                return;
                }

            activeCpu->iwRank = (activeCpu->iwRank + 1) % MaxIwStack;
            activeCpu->iwAddress[activeCpu->iwRank] = location;
            activeCpu->iwStack[activeCpu->iwRank]   = cpMem[location] & Mask60;
            activeCpu->iwValid[activeCpu->iwRank]   = TRUE;
#endif
            }
        }
    else
        {
        /*
        **  Fetch the instruction from CM.
        */
        activeCpu->opWord = cpMem[location] & Mask60;
        }

    activeCpu->opOffset = 60;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Void the instruction stack unless branch target is
**                  within stack (or unconditionally if address is ~0).
**
**  Parameters:     Name        Description.
**                  activeCpu   Pointer to CPU context
**                  branchAddr  Target location for a branch or ~0 for
**                              unconditional voiding.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cpuVoidIwStack(CpuContext *activeCpu, u32 branchAddr)
    {
    u32 location;
    int i;

    if (branchAddr != ~0)
        {
        location = cpuAddRa(activeCpu, branchAddr);

        for (i = 0; i < MaxIwStack; i++)
            {
            if (activeCpu->iwValid[i] && (activeCpu->iwAddress[i] == location))
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
        activeCpu->iwValid[i] = FALSE;
        }

    activeCpu->iwRank = 0;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Read CPU memory and verify that address is within limits.
**
**  Parameters:     Name        Description.
**                  activeCpu   Pointer to CPU context
**                  address     RA relative address to read.
**                  data        Pointer to 60 bit word which gets the data.
**
**  Returns:        TRUE if access failed, FALSE otherwise;
**
**------------------------------------------------------------------------*/
static bool cpuReadMem(CpuContext *activeCpu, u32 address, CpWord *data)
    {
    u32 location;

    if (address >= activeCpu->regFlCm)
        {
        activeCpu->exitCondition |= EcAddressOutOfRange;

        /*
        **  Clear the data.
        */
        *data = 0;

        if ((activeCpu->exitMode & EmAddressOutOfRange) != 0)
            {
            /*
            **  Exit mode selected.
            */
            activeCpu->isStopped = TRUE;

            if (activeCpu->regRaCm < cpuMaxMemory)
                {
                cpMem[activeCpu->regRaCm] = ((CpWord)activeCpu->exitCondition << 48) | ((CpWord)(activeCpu->regP + 1) << 30);
                }

            activeCpu->regP = 0;

            if ((features & IsSeries170) == 0)
                {
                /*
                **  All except series 170 clear the data.
                */
                *data = 0;
                }

            if (((features & (HasNoCejMej | IsSeries6x00)) == 0) && !activeCpu->isMonitorMode)
                {
                activeCpu->isErrorExitPending = TRUE;
                }

            return (TRUE);
            }

        return (FALSE);
        }

    /*
    **  Calculate absolute address.
    */
    location = cpuAddRa(activeCpu, address);

    /*
    **  Wrap around or fail gracefully if wrap around is disabled.
    */
    if (location >= cpuMaxMemory)
        {
        if ((features & HasNoCmWrap) != 0)
            {
            *data = (~((CpWord)0)) & Mask60;

            return (FALSE);
            }

        location %= cpuMaxMemory;
        }

    /*
    **  Fetch the data.
    */
    *data = cpMem[location] & Mask60;

    return (FALSE);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Write CPU memory and verify that address is within limits.
**
**  Parameters:     Name        Description.
**                  activeCpu   Pointer to CPU context
**                  address     RA relative address to write.
**                  data        Pointer to 60 bit word which holds the data.
**
**  Returns:        TRUE if access failed, FALSE otherwise;
**
**------------------------------------------------------------------------*/
static bool cpuWriteMem(CpuContext *activeCpu, u32 address, CpWord *data)
    {
    u32 location;

    if (address >= activeCpu->regFlCm)
        {
        activeCpu->exitCondition |= EcAddressOutOfRange;

        if ((activeCpu->exitMode & EmAddressOutOfRange) != 0)
            {
            /*
            **  Exit mode selected.
            */
            activeCpu->isStopped = TRUE;

            if (activeCpu->regRaCm < cpuMaxMemory)
                {
                cpMem[activeCpu->regRaCm] = ((CpWord)activeCpu->exitCondition << 48) | ((CpWord)(activeCpu->regP + 1) << 30);
                }

            activeCpu->regP = 0;

            if (((features & (HasNoCejMej | IsSeries6x00)) == 0) && !activeCpu->isMonitorMode)
                {
                activeCpu->isErrorExitPending = TRUE;
                }

            return (TRUE);
            }

        return (FALSE);
        }

    /*
    **  Calculate absolute address.
    */
    location = cpuAddRa(activeCpu, address);

    /*
    **  Wrap around or fail gracefully if wrap around is disabled.
    */
    if (location >= cpuMaxMemory)
        {
        if ((features & HasNoCmWrap) != 0)
            {
            return (FALSE);
            }

        location %= cpuMaxMemory;
        }

    /*
    **  Store the data.
    */
    cpMem[location] = *data & Mask60;

    return (FALSE);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Implement A register sematics.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cpuRegASemantics(CpuContext *activeCpu)
    {
    if (activeCpu->opI == 0)
        {
        return;
        }

    if (activeCpu->opI <= 5)
        {
        /*
        **  Read semantics.
        */
        cpuReadMem(activeCpu, activeCpu->regA[activeCpu->opI], activeCpu->regX + activeCpu->opI);
        }
    else
        {
        /*
        **  Write semantics.
        */
        if ((activeCpu->exitMode & EmFlagStackPurge) != 0)
            {
            /*
            **  Instruction stack purge flag is set - do an
            **  unconditional void.
            */
            cpuVoidIwStack(activeCpu, ~0);
            }

        cpuWriteMem(activeCpu, activeCpu->regA[activeCpu->opI], activeCpu->regX + activeCpu->opI);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Addition of 18 or 21 bit RA and 18 bit offset in
**                  ones-complement with subtractive adder
**
**  Parameters:     Name        Description.
**                  activeCpu   Pointer to CPU context
**                  op          18 bit offset
**
**  Returns:        18 or 21 bit result.
**
**------------------------------------------------------------------------*/
static u32 cpuAddRa(CpuContext *activeCpu, u32 op)
    {
    u32 acc18;
    u32 acc21;

    if ((features & IsSeries800) != 0)
        {
        acc21 = (activeCpu->regRaCm & Mask21) - (~op & Mask21);
        if ((acc21 & Overflow21) != 0)
            {
            acc21 -= 1;
            }

        return (acc21 & Mask21);
        }

    acc18 = (activeCpu->regRaCm & Mask18) - (~op & Mask18);
    if ((acc18 & Overflow18) != 0)
        {
        acc18 -= 1;
        }

    return (acc18 & Mask18);
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
    u32 acc18;

    acc18 = (op1 & Mask18) - (~op2 & Mask18);
    if ((acc18 & Overflow18) != 0)
        {
        acc18 -= 1;
        }

    return (acc18 & Mask18);
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
    u32 acc24;

    acc24 = (op1 & Mask24) - (~op2 & Mask24);
    if ((acc24 & Overflow24) != 0)
        {
        acc24 -= 1;
        }

    return (acc24 & Mask24);
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
    u32 acc18;

    acc18 = (op1 & Mask18) - (op2 & Mask18);
    if ((acc18 & Overflow18) != 0)
        {
        acc18 -= 1;
        }

    return (acc18 & Mask18);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Transfer word to/from UEM initiated by a CPU instruction.
**
**  Parameters:     Name        Description.
**                  activeCpu   Pointer to CPU context
**                  writeToUem  TRUE if this is a write to UEM, FALSE if
**                              this is a read.
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
static void cpuUemWord(CpuContext *activeCpu, bool writeToUem)
    {
    u32  absUemAddr;
    u32  flEcs;
    bool isExpandedAddress;
    u32  raEcs;
    u32  uemAddress;

    isExpandedAddress = (activeCpu->exitMode & EmFlagExpandedAddress) != 0;

    uemAddress = (u32)(activeCpu->regX[activeCpu->opK] & Mask30);

    if (isExpandedAddress)
        {
        raEcs      = (u32)(activeCpu->regRaEcs & Mask24);
        flEcs      = (u32)(activeCpu->regFlEcs & Mask30);
        absUemAddr = uemAddress + raEcs;
        }
    else
        {
        raEcs      = (u32)(activeCpu->regRaEcs & Mask21);
        flEcs      = (u32)(activeCpu->regFlEcs & Mask23);
        absUemAddr = uemAddress + raEcs;
        }

#if DEBUG_UEM
    fprintf(emLog, "\nUEM %s one  : %010o  RAE %010o, FLE %010o",
        writeToUem ? "write" : "read ", uemAddress, raEcs, flEcs);
    if (isExpandedAddress) fputs("  exp", emLog);
#endif

    /*
    **  Check for UEM range.
    */
    if (flEcs <= uemAddress)
        {
        activeCpu->exitCondition |= EcAddressOutOfRange;
#if DEBUG_UEM
        fprintf(emLog, "\n   UEM AddressOutOfRange: %010o  FLE %010o  %s word",
                uemAddress, flEcs, writeToUem ? "write" : "read");
        dumpXP(activeCpu, 020, 017);
#endif
        if ((activeCpu->exitMode & EmAddressOutOfRange) != 0)
            {
            /*
            **  Exit mode selected.
            */
            activeCpu->isStopped = TRUE;

            if (activeCpu->regRaCm < cpuMaxMemory)
                {
                cpMem[activeCpu->regRaCm] = ((CpWord)activeCpu->exitCondition << 48) | ((CpWord)(activeCpu->regP + 1) << 30);
                }

            activeCpu->regP = 0;

            if (((features & (HasNoCejMej | IsSeries6x00)) == 0) && !activeCpu->isMonitorMode)
                {
                activeCpu->isErrorExitPending = TRUE;
                }
            }

        return;
        }

    /*
    **  Perform the transfer.
    */
    if (writeToUem)
        {
        if (absUemAddr < cpuMaxMemory)
            {
            cpMem[absUemAddr] = activeCpu->regX[activeCpu->opJ] & Mask60;
            }
#if DEBUG_UEM
        else
            {
            fprintf(emLog, "  overflow (%010o >= %010o)", absUemAddr, cpuMaxMemory);
            }
#endif
        }
    else
        {
        if (absUemAddr < cpuMaxMemory)
            {
            activeCpu->regX[activeCpu->opJ] = cpMem[absUemAddr] & Mask60;
            }
#if DEBUG_UEM
        else
            {
            fprintf(emLog, "  overflow (%010o >= %010o)", absUemAddr, cpuMaxMemory);
            }
#endif
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Transfer word to/from ECS initiated by a CPU instruction.
**
**  Parameters:     Name        Description.
**                  activeCpu   Pointer to CPU context
**                  writeToEcs  TRUE if this is a write to ECS, FALSE if
**                              this is a read.
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
static void cpuEcsWord(CpuContext *activeCpu, bool writeToEcs)
    {
    u32  absEcsAddr;
    u32  ecsAddress;
    u32  flEcs;
    bool isExpandedAddress;
    bool isFlagRegister = FALSE;
    bool isMaintenance  = FALSE;
    bool isZeroFill     = FALSE;
    u32  raEcs;

    /*
    **  ECS must exist.
    */
    if (extMaxMemory == 0)
        {
        cpuOpIllegal(activeCpu);

        return;
        }
    
    isExpandedAddress = (activeCpu->exitMode & EmFlagExpandedAddress) != 0;

    ecsAddress = (u32)(activeCpu->regX[activeCpu->opK] & Mask30);

    if (isExpandedAddress)
        {
        raEcs          = (u32)(activeCpu->regRaEcs & Mask24);
        flEcs          = (u32)(activeCpu->regFlEcs & Mask30);
        absEcsAddr     = ecsAddress + raEcs;
        isFlagRegister = (ecsAddress & (1 << 29)) != 0
                         && (activeCpu->regFlEcs & (1 << 29)) != 0;
        if (isFlagRegister == FALSE && modelType == ModelCyber865)
            {
            if ((absEcsAddr & (5 << 22)) == (4 << 22)
                || (absEcsAddr & (3 << 28)) == (1 << 28))
                {
                isZeroFill = TRUE;
                }
            else if ((absEcsAddr & (5 << 22)) == (5 << 22))
                {
                isMaintenance = TRUE;
                }
            }
        }
    else
        {
        raEcs          = (u32)(activeCpu->regRaEcs & Mask21);
        flEcs          = (u32)(activeCpu->regFlEcs & Mask23);
        absEcsAddr     = ecsAddress + raEcs;
        isFlagRegister = (ecsAddress & (1 << 23)) != 0
                         && (activeCpu->regFlEcs & (1 << 23)) != 0;
        if (isFlagRegister == FALSE && modelType == ModelCyber865)
            {
            if ((absEcsAddr & (7 << 21)) == (1 << 21))
                {
                isZeroFill = TRUE;
                }
            else if ((absEcsAddr & (3 << 22)) == (1 << 22))
                {
                isMaintenance = TRUE;
                }
            }
        }

#if DEBUG_ECS
    fprintf(emLog, "\nECS %s one  : %010o  RAE %010o, FLE %010o",
        writeToEcs ? "write" : "read ", ecsAddress, raEcs, flEcs);
    if (isExpandedAddress) fputs("  exp", emLog);
    if (isZeroFill)        fputs("  zero fill", emLog);
#endif

    /*
    **  Check for ECS range.
    */
    if (flEcs <= ecsAddress)
        {
        activeCpu->exitCondition |= EcAddressOutOfRange;
#if DEBUG_ECS
        fprintf(emLog, "\n   ECS AddressOutOfRange: %010o  FLE %010o  %s word",
                ecsAddress, flEcs, writeToEcs ? "write" : "read");
        dumpXP(activeCpu, 020, 017);
#endif
        if ((activeCpu->exitMode & EmAddressOutOfRange) != 0)
            {
            /*
            **  Exit mode selected.
            */
            activeCpu->isStopped = TRUE;

            if (activeCpu->regRaCm < cpuMaxMemory)
                {
                cpMem[activeCpu->regRaCm] = ((CpWord)activeCpu->exitCondition << 48) | ((CpWord)(activeCpu->regP + 1) << 30);
                }

            activeCpu->regP = 0;

            if (((features & (HasNoCejMej | IsSeries6x00)) == 0) && !activeCpu->isMonitorMode)
                {
                activeCpu->isErrorExitPending = TRUE;
                }
            }

        return;
        }

    /*
    **  Perform the transfer.
    */
    if (writeToEcs)
        {
        if (isZeroFill || absEcsAddr >= extMaxMemory)
            {
#if DEBUG_ECS
            if (isZeroFill == FALSE) fprintf(emLog, "  overflow (%010o >= %010o)", absEcsAddr, extMaxMemory);
#endif
            /*
            **  No transfer and full exit to next instruction word.
            */
            activeCpu->regP = (activeCpu->regP + 1) & Mask18;
            cpuFetchOpWord(activeCpu);
            }
        else
            {
            extMem[absEcsAddr] = activeCpu->regX[activeCpu->opJ] & Mask60;
            }
        }
    else
        {
        if (isZeroFill || absEcsAddr >= extMaxMemory)
            {
#if DEBUG_ECS
            if (isZeroFill == FALSE) fprintf(emLog, "  overflow (%010o >= %010o)", absEcsAddr, extMaxMemory);
#endif
            /*
            **  Zero Xj, then full exit to next instruction word.
            */
            activeCpu->regX[activeCpu->opJ] = 0;
            activeCpu->regP                 = (activeCpu->regP + 1) & Mask18;
            cpuFetchOpWord(activeCpu);
            }
        else
            {
            activeCpu->regX[activeCpu->opJ] = extMem[absEcsAddr] & Mask60;
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Transfer block to/from UEM initiated by a CPU instruction.
**
**  Parameters:     Name        Description.
**                  activeCpu   Pointer to CPU context
**                  writeToUem  TRUE if this is a write to UEM, FALSE if
**                              this is a read.
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
static void cpuUemTransfer(CpuContext *activeCpu, bool writeToUem)
    {
    u32  absUemAddr;
    u32  cmAddress;
    u32  flEcs;
    bool isExpandedAddress;
    bool isZeroFill;
    u32  raEcs;
    u32  uemAddress;
    u32  wordCount;

    /*
    **  Instruction must be located in the upper 30 bits.
    */
    if (activeCpu->opOffset != 30)
        {
        cpuOpIllegal(activeCpu);

        return;
        }

    isExpandedAddress = (activeCpu->exitMode & EmFlagExpandedAddress) != 0;

    uemAddress = (u32)(activeCpu->regX[0] & Mask30);

    /*
    **  Calculate word count, source and destination addresses.
    */
    wordCount  = cpuAdd18(activeCpu->regB[activeCpu->opJ], activeCpu->opAddress);

    if (((features & IsSeries800) != 0) && isExpandedAddress)
        {
        raEcs      = (u32)(activeCpu->regRaEcs & Mask24);
        flEcs      = (u32)(activeCpu->regFlEcs & Mask30);
        absUemAddr = uemAddress + raEcs;
        isZeroFill = modelType == ModelCyber865
                     && ((absUemAddr & (3 << 28)) == (1 << 28));
        }
    else
        {
        raEcs      = (u32)(activeCpu->regRaEcs & Mask21);
        flEcs      = (u32)(activeCpu->regFlEcs & Mask24);
        absUemAddr = uemAddress + raEcs;
        isZeroFill = modelType == ModelCyber865
                     && (((absUemAddr & (5 << 21)) == (1 << 21))
                         || ((absUemAddr & (3 << 22)) == (1 << 22)));
        }

    if ((activeCpu->exitMode & EmFlagEnhancedBlockCopy) != 0)
        {
        cmAddress = (u32)((activeCpu->regX[0] >> 30) & Mask21);
        }
    else
        {
        cmAddress = activeCpu->regA[0] & Mask18;
        }

    /*
    **  Deal with possible negative zero word count.
    */
    if (wordCount == Mask18)
        {
        wordCount = 0;
        }

#if DEBUG_UEM
    fprintf(emLog, "\nUEM block %s: %010o  RAE %010o, FLE %010o  CM %07o  RA %08o  FL %08o  Words %d",
            writeToUem ? "write" : "read ",
            uemAddress, raEcs, flEcs, cmAddress, activeCpu->regRaCm, activeCpu->regFlCm, wordCount);
    if (isExpandedAddress) fputs("  exp", emLog);
    if (isZeroFill)        fputs("  zero fill", emLog);
#endif

    /*
    **  Check for positive word count, CM and UEM range.
    */
    if (((wordCount & Sign18) != 0)
        || (activeCpu->regFlCm < cmAddress + wordCount)
        || (flEcs < uemAddress + wordCount))
        {
#if DEBUG_UEM
        fprintf(emLog, "\n   UEM AddressOutOfRange: EM %010o  FLE %010o  CM %06o  FL %08o  Words %d",
                uemAddress, flEcs, cmAddress, activeCpu->regFlCm, wordCount);
        dumpXP(activeCpu, 020, 017);
#endif
        activeCpu->exitCondition |= EcAddressOutOfRange;
        if ((activeCpu->exitMode & EmAddressOutOfRange) != 0)
            {
            /*
            **  Exit mode selected.
            */
            activeCpu->isStopped = TRUE;

            if (activeCpu->regRaCm < cpuMaxMemory)
                {
                cpMem[activeCpu->regRaCm] = ((CpWord)activeCpu->exitCondition << 48) | ((CpWord)(activeCpu->regP + 1) << 30);
                }

            activeCpu->regP = 0;

            if (((features & (HasNoCejMej | IsSeries6x00)) == 0) && !activeCpu->isMonitorMode)
                {
                activeCpu->isErrorExitPending = TRUE;
                }
            }
        else
            {
            activeCpu->regP = (activeCpu->regP + 1) & Mask18;
            cpuFetchOpWord(activeCpu);
            }

        return;
        }

    /*
    **  Add base addresses.
    */
    cmAddress  = cpuAddRa(activeCpu, cmAddress);
    cmAddress %= cpuMaxMemory;

    /*
    **  Perform the transfer.
    */
    if (writeToUem)
        {
        while (wordCount--)
            {
            if (absUemAddr >= cpuMaxMemory)
                {
#if DEBUG_UEM
                fprintf(emLog, "  overflow (%010o >= %010o)", absUemAddr, cpuMaxMemory);
#endif
                return;
                }

            cpMem[absUemAddr++] = cpMem[cmAddress] & Mask60;

            /*
            **  Increment CM address.
            */
            cmAddress  = cpuAdd24(cmAddress, 1);
            cmAddress %= cpuMaxMemory;
            }
        }
    else
        {
        bool takeErrorExit = FALSE;

        while (wordCount--)
            {
            if (isZeroFill || (absUemAddr >= cpuMaxMemory))
                {
#if DEBUG_UEM
                if (isZeroFill == FALSE)
                    {
                    fprintf(emLog, "  overflow (%010o >= %010o)", absUemAddr, cpuMaxMemory);
                    isZeroFill = TRUE;
                    }
#endif
                /*
                **  Zero CM, but take error exit to lower 30 bits once zeroing is finished.
                */
                cpMem[cmAddress] = 0;
                takeErrorExit    = TRUE;
                }
            else
                {
                cpMem[cmAddress] = cpMem[absUemAddr++] & Mask60;
                }

            /*
            **  Increment CM address.
            */
            cmAddress  = cpuAdd24(cmAddress, 1);
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
    activeCpu->regP = (activeCpu->regP + 1) & Mask18;
    cpuFetchOpWord(activeCpu);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Transfer block to/from ECS initiated by a CPU instruction.
**
**  Parameters:     Name        Description.
**                  activeCpu   Pointer to CPU context
**                  writeToEcs  TRUE if this is a write to ECS, FALSE if
**                              this is a read.
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
static void cpuEcsTransfer(CpuContext *activeCpu, bool writeToEcs)
    {
    u32  absEcsAddr;
    u32  wordCount;
    u32  ecsAddress;
    u32  cmAddress;
    u32  flEcs;
    bool isExpandedAddress;
    bool isFlagRegister;
    bool isMaintenance;
    bool isZeroFill;
    u32  raEcs;

    /*
    **  ECS must exist and instruction must be located in the upper 30 bits.
    */
    if ((extMaxMemory == 0) || (activeCpu->opOffset != 30))
        {
        cpuOpIllegal(activeCpu);

        return;
        }

    ecsAddress = (u32)(activeCpu->regX[0] & Mask30);

    isExpandedAddress = ((features & IsSeries800) != 0)
        && ((activeCpu->exitMode & EmFlagExpandedAddress) != 0);
    isFlagRegister    = FALSE;
    isMaintenance     = FALSE; // for Cyber 865/875
    isZeroFill        = FALSE; // for Cyber 865/875

    /*
    **  Calculate word count, source and destination addresses.
    */
    wordCount = cpuAdd18(activeCpu->regB[activeCpu->opJ], activeCpu->opAddress);

    if (isExpandedAddress)
        {
        raEcs          = (u32)(activeCpu->regRaEcs & Mask24);
        flEcs          = (u32)(activeCpu->regFlEcs & Mask30);
        absEcsAddr     = ecsAddress + raEcs;
        isFlagRegister = (ecsAddress & (1 << 29)) != 0 && (flEcs & (1 << 29)) != 0;
        if (isFlagRegister == FALSE && modelType == ModelCyber865)
            {
            if ((absEcsAddr & (5 << 22)) == (4 << 22)
                || (absEcsAddr & (3 << 28)) == (1 << 28))
                {
                isZeroFill = TRUE;
                }
            else if ((absEcsAddr & (5 << 22)) == (5 << 22))
                {
                isMaintenance = TRUE;
                }
            }
        }
    else
        {
        raEcs          = (u32)(activeCpu->regRaEcs & Mask21);
        flEcs          = (u32)(activeCpu->regFlEcs & Mask24);
        absEcsAddr     = ecsAddress + raEcs;
        isFlagRegister = (ecsAddress & (1 << 23)) != 0 && (flEcs & (1 << 23)) != 0;
        if (isFlagRegister == FALSE && modelType == ModelCyber865)
            {
            if ((absEcsAddr & (7 << 21)) == (1 << 21))
                {
                isZeroFill = TRUE;
                }
            else if ((absEcsAddr & (3 << 22)) == (1 << 22))
                {
                isMaintenance = TRUE;
                }
            }
        }

    if (((features & IsSeries800) != 0)
        && ((activeCpu->exitMode & EmFlagEnhancedBlockCopy) != 0))
        {
        cmAddress = (u32)((activeCpu->regX[0] >> 30) & Mask30);
        }
    else
        {
        cmAddress = activeCpu->regA[0] & Mask18;
        }

#if DEBUG_ECS
    fprintf(emLog, "\nECS block %s: %010o  RAE %010o, FLE %010o  CM %07o  RA %08o  FL %08o  Words %d",
            writeToEcs ? "write" : "read ",
            ecsAddress, raEcs, flEcs, cmAddress, activeCpu->regRaCm, activeCpu->regFlCm, wordCount);
    if (isExpandedAddress) fputs("  exp", emLog);
    if (isFlagRegister)    fputs("  flag", emLog);
    if (isZeroFill)        fputs("  zero fill", emLog);
    if (isMaintenance)     fputs("  maint", emLog);
#endif

    /*
    **  Check if this is a flag register access.
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
        activeCpu->regP = (activeCpu->regP + 1) & Mask18;
        cpuFetchOpWord(activeCpu);

        return;
        }

    /*
    **  Check if this is a maintenance operation.
    **
    **  DtCyber doesn't currently implement maintenance operations.
    */
    if (isMaintenance)
        {
        /*
        **  Normal exit.
        */
        activeCpu->regP = (activeCpu->regP + 1) & Mask18;
        cpuFetchOpWord(activeCpu);

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
    if (((wordCount & Sign18) != 0)
        || (activeCpu->regFlCm < cmAddress + wordCount)
        || (flEcs < ecsAddress + wordCount))
        {
#if DEBUG_ECS
        fprintf(emLog, "\n   ECS AddressOutOfRange: EM %010o  FLE %010o  CM %06o  FL %08o  Words %d",
                ecsAddress, flEcs, cmAddress, activeCpu->regFlCm, wordCount);
        dumpXP(activeCpu, 020, 017);
#endif
        activeCpu->exitCondition |= EcAddressOutOfRange;
        if ((activeCpu->exitMode & EmAddressOutOfRange) != 0)
            {
            /*
            **  Exit mode selected.
            */
            activeCpu->isStopped = TRUE;

            if (activeCpu->regRaCm < cpuMaxMemory)
                {
                cpMem[activeCpu->regRaCm] = ((CpWord)activeCpu->exitCondition << 48) | ((CpWord)(activeCpu->regP + 1) << 30);
                }

            activeCpu->regP = 0;

            if (((features & (HasNoCejMej | IsSeries6x00)) == 0) && !activeCpu->isMonitorMode)
                {
                activeCpu->isErrorExitPending = TRUE;
                }
            }
        else
            {
            activeCpu->regP = (activeCpu->regP + 1) & Mask18;
            cpuFetchOpWord(activeCpu);
            }

        return;
        }

    /*
    **  Add base addresses.
    */
    cmAddress  = cpuAddRa(activeCpu, cmAddress);
    cmAddress %= cpuMaxMemory;

    /*
    **  Perform the transfer.
    */
    if (writeToEcs)
        {
        while (wordCount--)
            {
            if (absEcsAddr >= extMaxMemory)
                {
#if DEBUG_ECS
                if (isZeroFill == FALSE)
                    {
                    fprintf(emLog, "  overflow (%010o >= %010o)", absEcsAddr, extMaxMemory);
                    isZeroFill = TRUE;
                    }
#endif
                /*
                **  Error exit to lower 30 bits of instruction word.
                */
                return;
                }

            extMem[absEcsAddr++] = cpMem[cmAddress] & Mask60;

            /*
            **  Increment CM address.
            */
            cmAddress  = cpuAdd24(cmAddress, 1);
            cmAddress %= cpuMaxMemory;
            }
        }
    else
        {
        bool takeErrorExit = FALSE;

        while (wordCount--)
            {
            if (isZeroFill || (absEcsAddr >= extMaxMemory))
                {
#if DEBUG_ECS
                if (isZeroFill == FALSE)
                    {
                    fprintf(emLog, "  overflow (%010o >= %010o)", absEcsAddr, extMaxMemory);
                    isZeroFill = TRUE;
                    }
#endif
                /*
                **  Zero CM, but take error exit to lower 30 bits once zeroing is finished.
                */
                cpMem[cmAddress] = 0;
                takeErrorExit    = TRUE;
                }
            else
                {
                cpMem[cmAddress] = extMem[absEcsAddr++] & Mask60;
                }

            /*
            **  Increment CM address.
            */
            cmAddress  = cpuAdd24(cmAddress, 1);
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
    activeCpu->regP = (activeCpu->regP + 1) & Mask18;
    cpuFetchOpWord(activeCpu);
    }

/*--------------------------------------------------------------------------
**  Purpose:        CMU get a byte.
**
**  Parameters:     Name        Description.
**                  activeCpu   Pointer to CPU context
**                  address     CM word address
**                  pos         character position
**                  byte        pointer to byte
**
**  Returns:        TRUE if access failed, FALSE otherwise.
**
**------------------------------------------------------------------------*/
static bool cpuCmuGetByte(CpuContext *activeCpu, u32 address, u32 pos, u8 *byte)
    {
    u32    location;
    CpWord data;

    /*
    **  Validate access.
    */
    if ((address >= activeCpu->regFlCm) || (activeCpu->regRaCm + address >= cpuMaxMemory))
        {
        activeCpu->exitCondition |= EcAddressOutOfRange;
        if ((activeCpu->exitMode & EmAddressOutOfRange) != 0)
            {
            /*
            **  Exit mode selected.
            */
            activeCpu->isStopped = TRUE;

            if (activeCpu->regRaCm < cpuMaxMemory)
                {
                cpMem[activeCpu->regRaCm] = ((CpWord)activeCpu->exitCondition << 48) | ((CpWord)(activeCpu->regP + 1) << 30);
                }

            activeCpu->regP = 0;

            if (((features & (HasNoCejMej | IsSeries6x00)) == 0) && !activeCpu->isMonitorMode)
                {
                activeCpu->isErrorExitPending = TRUE;
                }
            }

        return (TRUE);
        }

    /*
    **  Calculate absolute address with wraparound.
    */
    location  = cpuAddRa(activeCpu, address);
    location %= cpuMaxMemory;

    /*
    **  Fetch the word.
    */
    data = cpMem[location] & Mask60;

    /*
    **  Extract and return the byte.
    */
    *byte = (u8)((data >> ((9 - pos) * 6)) & Mask6);

    return (FALSE);
    }

/*--------------------------------------------------------------------------
**  Purpose:        CMU put a byte.
**
**  Parameters:     Name        Description.
**                  activeCpu   Pointer to CPU context
**                  address     CM word address
**                  pos         character position
**                  byte        data byte to put
**
**  Returns:        TRUE if access failed, FALSE otherwise.
**
**------------------------------------------------------------------------*/
static bool cpuCmuPutByte(CpuContext *activeCpu, u32 address, u32 pos, u8 byte)
    {
    u32    location;
    CpWord data;

    /*
    **  Validate access.
    */
    if ((address >= activeCpu->regFlCm) || (activeCpu->regRaCm + address >= cpuMaxMemory))
        {
        activeCpu->exitCondition |= EcAddressOutOfRange;
        if ((activeCpu->exitMode & EmAddressOutOfRange) != 0)
            {
            /*
            **  Exit mode selected.
            */
            activeCpu->isStopped = TRUE;

            if (activeCpu->regRaCm < cpuMaxMemory)
                {
                cpMem[activeCpu->regRaCm] = ((CpWord)activeCpu->exitCondition << 48) | ((CpWord)(activeCpu->regP + 1) << 30);
                }

            activeCpu->regP = 0;

            if (((features & (HasNoCejMej | IsSeries6x00)) == 0) && !activeCpu->isMonitorMode)
                {
                activeCpu->isErrorExitPending = TRUE;
                }
            }

        return (TRUE);
        }

    /*
    **  Calculate absolute address with wraparound.
    */
    location  = cpuAddRa(activeCpu, address);
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

    return (FALSE);
    }

/*--------------------------------------------------------------------------
**  Purpose:        CMU move indirect.
**
**  Parameters:     Name        Description.
**                  activeCpu   Pointer to CPU context
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
static void cpuCmuMoveIndirect(CpuContext *activeCpu)
    {
    CpWord descWord;
    u32    k1, k2;
    u32    c1, c2;
    u32    ll;
    u8     byte;
    bool   failed;

    //<<<<<<<<<<<<<<<<<<<<<<<< don't forget to optimise c1 == c2 cases.

    /*
    **  Fetch the descriptor word.
    */
    activeCpu->opAddress = (u32)((activeCpu->opWord >> 30) & Mask18);
    activeCpu->opAddress = cpuAdd18(activeCpu->regB[activeCpu->opJ], activeCpu->opAddress);
    failed    = cpuReadMem(activeCpu, activeCpu->opAddress, &descWord);
    if (failed)
        {
        return;
        }

    /*
    **  Decode descriptor word.
    */
    k1 = (u32)(descWord >> 30) & Mask18;
    k2 = (u32)(descWord >> 0) & Mask18;
    c1 = (u32)(descWord >> 22) & Mask4;
    c2 = (u32)(descWord >> 18) & Mask4;
    ll = (u32)((descWord >> 26) & Mask4) | (u32)((descWord >> (48 - 4)) & (Mask9 << 4));

    /*
    **  Check for address out of range.
    */
    if ((c1 > 9) || (c2 > 9))
        {
        activeCpu->exitCondition |= EcAddressOutOfRange;
        if ((activeCpu->exitMode & EmAddressOutOfRange) != 0)
            {
            /*
            **  Exit mode selected.
            */
            activeCpu->isStopped = TRUE;

            if (activeCpu->regRaCm < cpuMaxMemory)
                {
                cpMem[activeCpu->regRaCm] = ((CpWord)activeCpu->exitCondition << 48) | ((CpWord)(activeCpu->regP + 1) << 30);
                }

            activeCpu->regP = 0;

            if (((features & (HasNoCejMej | IsSeries6x00)) == 0) && !activeCpu->isMonitorMode)
                {
                activeCpu->isErrorExitPending = TRUE;
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
        if (cpuCmuGetByte(activeCpu, k1, c1, &byte)
            || cpuCmuPutByte(activeCpu, k2, c2, byte))
            {
            if (activeCpu->isStopped) //????????????????????????
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
    activeCpu->regX[0] = 0;

    /*
    **  Normal exit to next instruction word.
    */
    activeCpu->regP = (activeCpu->regP + 1) & Mask18;
    cpuFetchOpWord(activeCpu);
    }

/*--------------------------------------------------------------------------
**  Purpose:        CMU move direct.
**
**  Parameters:     Name        Description.
**                  activeCpu   Pointer to CPU context
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
static void cpuCmuMoveDirect(CpuContext *activeCpu)
    {
    u32 k1, k2;
    u32 c1, c2;
    u32 ll;
    u8  byte;

    //<<<<<<<<<<<<<<<<<<<<<<<< don't forget to optimise c1 == c2 cases.

    /*
    **  Decode opcode word.
    */
    k1 = (u32)(activeCpu->opWord >> 30) & Mask18;
    k2 = (u32)(activeCpu->opWord >> 0) & Mask18;
    c1 = (u32)(activeCpu->opWord >> 22) & Mask4;
    c2 = (u32)(activeCpu->opWord >> 18) & Mask4;
    ll = (u32)((activeCpu->opWord >> 26) & Mask4) | (u32)((activeCpu->opWord >> (48 - 4)) & (Mask3 << 4));

    /*
    **  Check for address out of range.
    */
    if ((c1 > 9) || (c2 > 9))
        {
        activeCpu->exitCondition |= EcAddressOutOfRange;
        if ((activeCpu->exitMode & EmAddressOutOfRange) != 0)
            {
            /*
            **  Exit mode selected.
            */
            activeCpu->isStopped = TRUE;

            if (activeCpu->regRaCm < cpuMaxMemory)
                {
                cpMem[activeCpu->regRaCm] = ((CpWord)activeCpu->exitCondition << 48) | ((CpWord)(activeCpu->regP + 1) << 30);
                }

            activeCpu->regP = 0;

            if (((features & (HasNoCejMej | IsSeries6x00)) == 0) && !activeCpu->isMonitorMode)
                {
                activeCpu->isErrorExitPending = TRUE;
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
        if (cpuCmuGetByte(activeCpu, k1, c1, &byte)
            || cpuCmuPutByte(activeCpu, k2, c2, byte))
            {
            if (activeCpu->isStopped) //?????????????????????
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
    activeCpu->regX[0] = 0;

    /*
    **  Normal exit to next instruction word.
    */
    activeCpu->regP = (activeCpu->regP + 1) & Mask18;
    cpuFetchOpWord(activeCpu);
    }

/*--------------------------------------------------------------------------
**  Purpose:        CMU compare collated.
**
**  Parameters:     Name        Description.
**                  activeCpu   Pointer to CPU context
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
static void cpuCmuCompareCollated(CpuContext *activeCpu)
    {
    CpWord result = 0;
    u32    k1, k2;
    u32    c1, c2;
    u32    ll;
    u32    collTable;
    u8     byte1, byte2;

    /*
    **  Decode opcode word.
    */
    k1 = (u32)(activeCpu->opWord >> 30) & Mask18;
    k2 = (u32)(activeCpu->opWord >> 0) & Mask18;
    c1 = (u32)(activeCpu->opWord >> 22) & Mask4;
    c2 = (u32)(activeCpu->opWord >> 18) & Mask4;
    ll = (u32)((activeCpu->opWord >> 26) & Mask4) | (u32)((activeCpu->opWord >> (48 - 4)) & (Mask3 << 4));

    /*
    **  Setup collating table.
    */
    collTable = activeCpu->regA[0];

    /*
    **  Check for addresses and collTable out of range.
    */
    if ((c1 > 9) || (c2 > 9) || (collTable >= activeCpu->regFlCm) || (activeCpu->regRaCm + collTable >= cpuMaxMemory))
        {
        activeCpu->exitCondition |= EcAddressOutOfRange;
        if ((activeCpu->exitMode & EmAddressOutOfRange) != 0)
            {
            /*
            **  Exit mode selected.
            */
            activeCpu->isStopped = TRUE;

            if (activeCpu->regRaCm < cpuMaxMemory)
                {
                cpMem[activeCpu->regRaCm] = ((CpWord)activeCpu->exitCondition << 48) | ((CpWord)(activeCpu->regP + 1) << 30);
                }

            activeCpu->regP = 0;

            if (((features & (HasNoCejMej | IsSeries6x00)) == 0) && !activeCpu->isMonitorMode)
                {
                activeCpu->isErrorExitPending = TRUE;
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
        if (cpuCmuGetByte(activeCpu, k1, c1, &byte1)
            || cpuCmuGetByte(activeCpu, k2, c2, &byte2))
            {
            if (activeCpu->isStopped) //?????????????????????
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
            if (cpuCmuGetByte(activeCpu, collTable + ((byte1 >> 3) & Mask3), byte1 & Mask3, &byte1)
                || cpuCmuGetByte(activeCpu, collTable + ((byte2 >> 3) & Mask3), byte2 & Mask3, &byte2))
                {
                if (activeCpu->isStopped) //??????????????????????
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
    activeCpu->regX[0] = result;

    /*
    **  Normal exit to next instruction word.
    */
    activeCpu->regP = (activeCpu->regP + 1) & Mask18;
    cpuFetchOpWord(activeCpu);
    }

/*--------------------------------------------------------------------------
**  Purpose:        CMU compare uncollated.
**
**  Parameters:     Name        Description.
**                  activeCpu   Pointer to CPU context
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
static void cpuCmuCompareUncollated(CpuContext *activeCpu)
    {
    CpWord result = 0;
    u32    k1, k2;
    u32    c1, c2;
    u32    ll;
    u8     byte1, byte2;

    /*
    **  Decode opcode word.
    */
    k1 = (u32)(activeCpu->opWord >> 30) & Mask18;
    k2 = (u32)(activeCpu->opWord >> 0) & Mask18;
    c1 = (u32)(activeCpu->opWord >> 22) & Mask4;
    c2 = (u32)(activeCpu->opWord >> 18) & Mask4;
    ll = (u32)((activeCpu->opWord >> 26) & Mask4) | (u32)((activeCpu->opWord >> (48 - 4)) & (Mask3 << 4));

    /*
    **  Check for address out of range.
    */
    if ((c1 > 9) || (c2 > 9))
        {
        activeCpu->exitCondition |= EcAddressOutOfRange;
        if ((activeCpu->exitMode & EmAddressOutOfRange) != 0)
            {
            /*
            **  Exit mode selected.
            */
            activeCpu->isStopped = TRUE;

            if (activeCpu->regRaCm < cpuMaxMemory)
                {
                cpMem[activeCpu->regRaCm] = ((CpWord)activeCpu->exitCondition << 48) | ((CpWord)(activeCpu->regP + 1) << 30);
                }

            activeCpu->regP = 0;

            if (((features & (HasNoCejMej | IsSeries6x00)) == 0) && !activeCpu->isMonitorMode)
                {
                activeCpu->isErrorExitPending = TRUE;
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
        if (cpuCmuGetByte(activeCpu, k1, c1, &byte1)
            || cpuCmuGetByte(activeCpu, k2, c2, &byte2))
            {
            if (activeCpu->isStopped) //?????????????????
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
    activeCpu->regX[0] = result;

    /*
    **  Normal exit to next instruction word.
    */
    activeCpu->regP = (activeCpu->regP + 1) & Mask18;
    cpuFetchOpWord(activeCpu);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Check parameter for floating point infinite and
**                  indefinite and set exit condition accordingly.
**
**  Parameters:     Name        Description.
**                  activeCpu   Pointer to CPU context
**                  value       Floating point value to check
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cpuFloatCheck(CpuContext *activeCpu, CpWord value)
    {
    int exponent = ((int)(value >> 48)) & Mask12;

    if ((exponent == 03777) || (exponent == 04000))
        {
        activeCpu->exitCondition |= EcOperandOutOfRange;
        activeCpu->floatException = TRUE;
        }
    else if ((exponent == 01777) || (exponent == 06000))
        {
        activeCpu->exitCondition |= EcIndefiniteOperand;
        activeCpu->floatException = TRUE;
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
static void cpuFloatExceptionHandler(CpuContext *activeCpu)
    {
    if (activeCpu->floatException)
        {
        activeCpu->floatException = FALSE;

        if ((activeCpu->exitMode & (activeCpu->exitCondition << 12)) != 0)
            {
            /*
            **  Exit mode selected.
            */
            activeCpu->isStopped = TRUE;

            if (activeCpu->regRaCm < cpuMaxMemory)
                {
                cpMem[activeCpu->regRaCm] = ((CpWord)activeCpu->exitCondition << 48) | ((CpWord)(activeCpu->regP + 1) << 30);
                }

            activeCpu->regP = 0;

            if (((features & (HasNoCejMej | IsSeries6x00)) == 0) && !activeCpu->isMonitorMode)
                {
                activeCpu->isErrorExitPending = TRUE;
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
static void cpOp00(CpuContext *activeCpu)
    {
    /*
    **  PS or Error Exit to MA.
    */
    if (((features & (HasNoCejMej | IsSeries6x00)) != 0) || activeCpu->isMonitorMode)
        {
        activeCpu->isStopped = TRUE;
        }
    else
        {
        cpuOpIllegal(activeCpu);
        }
    }

static void cpOp01(CpuContext *activeCpu)
    {
    CpWord acc60;

    switch (activeCpu->opI)
        {
    case 0:
        /*
        **  RJ  K
        */
        acc60 = ((CpWord)0400 << 48) | ((CpWord)((activeCpu->regP + 1) & Mask18) << 30);
        if (cpuWriteMem(activeCpu, activeCpu->opAddress, &acc60))
            {
            return;
            }

        activeCpu->regP = activeCpu->opAddress;
        activeCpu->opOffset = 0;

        if ((features & HasInstructionStack) != 0)
            {
            /*
            **  Void the instruction stack.
            */
            cpuVoidIwStack(activeCpu, ~0);
            }

        break;

    case 1:
        /*
        **  REC  Bj+K
        */
        if ((activeCpu->exitMode & EmFlagUemEnable) != 0)
            {
            cpuUemTransfer(activeCpu, FALSE);
            }
        else
            {
            cpuEcsTransfer(activeCpu, FALSE);
            }

        if ((features & HasInstructionStack) != 0)
            {
            /*
            **  Void the instruction stack.
            */
            cpuVoidIwStack(activeCpu, ~0);
            }

        break;

    case 2:
        /*
        **  WEC  Bj+K
        */
        if ((activeCpu->exitMode & EmFlagUemEnable) != 0)
            {
            cpuUemTransfer(activeCpu, TRUE);
            }
        else
            {
            cpuEcsTransfer(activeCpu, TRUE);
            }

        break;

    case 3:
        /*
        **  XJ  Bj+K
        */
        if (((features & HasNoCejMej) != 0) || (activeCpu->opOffset != 30))
            {
            /*
            **  CEJ/MEJ must be enabled and the instruction must be in parcel 0,
            **  if not, it is interpreted as an illegal instruction.
            */
            cpuOpIllegal(activeCpu);
            return;
            }

        cpuAcquireExchangeMutex();
        if (activeCpu->ppRequestingExchange == -1
            && (monitorCpu == -1 || monitorCpu == activeCpu->id))
            {
            activeCpu->regP      = (activeCpu->regP + 1) & Mask18;
            activeCpu->isStopped = TRUE;
            cpuExchangeJump(activeCpu,
                activeCpu->isMonitorMode ? activeCpu->opAddress + activeCpu->regB[activeCpu->opJ] : activeCpu->regMa,
                TRUE);
            }
        else
            {
            activeCpu->opOffset = 60; // arrange to re-execute XJ
            }
        cpuReleaseExchangeMutex();
        break;

    case 4:
        if (modelType != ModelCyber865)
            {
            cpuOpIllegal(activeCpu);

            return;
            }

        /*
        **  RXj  Xk
        */
        if ((activeCpu->exitMode & EmFlagUemEnable) != 0)
            {
            cpuUemWord(activeCpu, FALSE);
            }
        else
            {
            cpuEcsWord(activeCpu, FALSE);
            }

        break;

    case 5:
        if (modelType != ModelCyber865)
            {
            cpuOpIllegal(activeCpu);

            return;
            }

        /*
        **  WXj  Xk
        */
        if ((activeCpu->exitMode & EmFlagUemEnable) != 0)
            {
            cpuUemWord(activeCpu, TRUE);
            }
        else
            {
            cpuEcsWord(activeCpu, TRUE);
            }

        break;

    case 6:
        if ((features & HasMicrosecondClock) != 0)
            {
            /*
            **  RC  Xj
            */
            //rtcReadUsCounter();
            activeCpu->regX[activeCpu->opJ] = rtcClock;
            }
        else
            {
            cpuOpIllegal(activeCpu);
            }

        break;

    case 7:
        /*
        **  7600 instruction (invalid in our context).
        */
        cpuOpIllegal(activeCpu);
        break;
        }
    }

static void cpOp02(CpuContext *activeCpu)
    {
    /*
    **  JP  Bi+K
    */
    activeCpu->regP = cpuAdd18(activeCpu->regB[activeCpu->opI], activeCpu->opAddress);

    if ((features & HasInstructionStack) != 0)
        {
        /*
        **  Void the instruction stack.
        */
        cpuVoidIwStack(activeCpu, ~0);
        }

    cpuFetchOpWord(activeCpu);
    }

static void cpOp03(CpuContext *activeCpu)
    {
    CpWord acc60;

    bool jump = FALSE;

    switch (activeCpu->opI)
        {
    case 0:
        /*
        **  ZR  Xj K
        */
        jump = activeCpu->regX[activeCpu->opJ] == 0 || activeCpu->regX[activeCpu->opJ] == NegativeZero;
        break;

    case 1:
        /*
        **  NZ  Xj K
        */
        jump = activeCpu->regX[activeCpu->opJ] != 0 && activeCpu->regX[activeCpu->opJ] != NegativeZero;
        break;

    case 2:
        /*
        **  PL  Xj K
        */
        jump = (activeCpu->regX[activeCpu->opJ] & Sign60) == 0;
        break;

    case 3:
        /*
        **  NG  Xj K
        */
        jump = (activeCpu->regX[activeCpu->opJ] & Sign60) != 0;
        break;

    case 4:
        /*
        **  IR  Xj K
        */
        acc60 = activeCpu->regX[activeCpu->opJ] >> 48;
        jump  = acc60 != 03777 && acc60 != 04000;
        break;

    case 5:
        /*
        **  OR  Xj K
        */
        acc60 = activeCpu->regX[activeCpu->opJ] >> 48;
        jump  = acc60 == 03777 || acc60 == 04000;
        break;

    case 6:
        /*
        **  DF  Xj K
        */
        acc60 = activeCpu->regX[activeCpu->opJ] >> 48;
        jump  = acc60 != 01777 && acc60 != 06000;
        break;

    case 7:
        /*
        **  ID  Xj K
        */
        acc60 = activeCpu->regX[activeCpu->opJ] >> 48;
        jump  = acc60 == 01777 || acc60 == 06000;
        break;
        }

    if (jump)
        {
        if ((features & HasInstructionStack) != 0)
            {
            /*
            **  Void the instruction stack.
            */
            if ((activeCpu->exitMode & EmFlagStackPurge) != 0)
                {
                /*
                **  Instruction stack purge flag is set - do an
                **  unconditional void.
                */
                cpuVoidIwStack(activeCpu, ~0);
                }
            else
                {
                /*
                **  Normal conditional void.
                */
                cpuVoidIwStack(activeCpu, activeCpu->opAddress);
                }
            }

        activeCpu->regP = activeCpu->opAddress;
        cpuFetchOpWord(activeCpu);
        }
    }

static void cpOp04(CpuContext *activeCpu)
    {
    /*
    **  EQ  Bi Bj K
    */
    if (activeCpu->regB[activeCpu->opI] == activeCpu->regB[activeCpu->opJ])
        {
        if ((features & HasInstructionStack) != 0)
            {
            /*
            **  Void the instruction stack.
            */
            cpuVoidIwStack(activeCpu, activeCpu->opAddress);
            }

        activeCpu->regP = activeCpu->opAddress;
        cpuFetchOpWord(activeCpu);
        }
    }

static void cpOp05(CpuContext *activeCpu)
    {
    /*
    **  NE  Bi Bj K
    */
    if (activeCpu->regB[activeCpu->opI] != activeCpu->regB[activeCpu->opJ])
        {
        if ((features & HasInstructionStack) != 0)
            {
            /*
            **  Void the instruction stack.
            */
            cpuVoidIwStack(activeCpu, activeCpu->opAddress);
            }

        activeCpu->regP = activeCpu->opAddress;
        cpuFetchOpWord(activeCpu);
        }
    }

static void cpOp06(CpuContext *activeCpu)
    {
    u32 acc18;

    /*
    **  GE  Bi Bj K
    */
    i32 signDiff = (activeCpu->regB[activeCpu->opI] & Sign18) - (activeCpu->regB[activeCpu->opJ] & Sign18);

    if (signDiff > 0)
        {
        return;
        }

    if (signDiff == 0)
        {
        acc18 = (activeCpu->regB[activeCpu->opI] & Mask18) - (activeCpu->regB[activeCpu->opJ] & Mask18);
        if (((acc18 & Overflow18) != 0) && ((acc18 & Mask18) != 0))
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
        cpuVoidIwStack(activeCpu, activeCpu->opAddress);
        }

    activeCpu->regP = activeCpu->opAddress;
    cpuFetchOpWord(activeCpu);
    }

static void cpOp07(CpuContext *activeCpu)
    {
    u32 acc18;

    /*
    **  LT  Bi Bj K
    */
    i32 signDiff = (activeCpu->regB[activeCpu->opI] & Sign18) - (activeCpu->regB[activeCpu->opJ] & Sign18);

    if (signDiff < 0)
        {
        return;
        }

    if (signDiff == 0)
        {
        acc18 = (activeCpu->regB[activeCpu->opI] & Mask18) - (activeCpu->regB[activeCpu->opJ] & Mask18);
        if (((acc18 & Overflow18) != 0) && ((acc18 & Mask18) != 0))
            {
            acc18 -= 1;
            }

        if (((acc18 & Sign18) == 0) || (acc18 == 0))
            {
            return;
            }
        }

    if ((features & HasInstructionStack) != 0)
        {
        /*
        **  Void the instruction stack.
        */
        cpuVoidIwStack(activeCpu, activeCpu->opAddress);
        }

    activeCpu->regP = activeCpu->opAddress;
    cpuFetchOpWord(activeCpu);
    }

static void cpOp10(CpuContext *activeCpu)
    {
    /*
    **  BXi Xj
    */
    activeCpu->regX[activeCpu->opI] = activeCpu->regX[activeCpu->opJ] & Mask60;
    }

static void cpOp11(CpuContext *activeCpu)
    {
    /*
    **  BXi Xj*Xk
    */
    activeCpu->regX[activeCpu->opI] = (activeCpu->regX[activeCpu->opJ] & activeCpu->regX[activeCpu->opK]) & Mask60;
    }

static void cpOp12(CpuContext *activeCpu)
    {
    /*
    **  BXi Xj+Xk
    */
    activeCpu->regX[activeCpu->opI] = (activeCpu->regX[activeCpu->opJ] | activeCpu->regX[activeCpu->opK]) & Mask60;
    }

static void cpOp13(CpuContext *activeCpu)
    {
    /*
    **  BXi Xj-Xk
    */
    activeCpu->regX[activeCpu->opI] = (activeCpu->regX[activeCpu->opJ] ^ activeCpu->regX[activeCpu->opK]) & Mask60;
    }

static void cpOp14(CpuContext *activeCpu)
    {
    /*
    **  BXi -Xj
    */
    activeCpu->regX[activeCpu->opI] = ~activeCpu->regX[activeCpu->opK] & Mask60;
    }

static void cpOp15(CpuContext *activeCpu)
    {
    /*
    **  BXi -Xk*Xj
    */
    activeCpu->regX[activeCpu->opI] = (activeCpu->regX[activeCpu->opJ] & ~activeCpu->regX[activeCpu->opK]) & Mask60;
    }

static void cpOp16(CpuContext *activeCpu)
    {
    /*
    **  BXi -Xk+Xj
    */
    activeCpu->regX[activeCpu->opI] = (activeCpu->regX[activeCpu->opJ] | ~activeCpu->regX[activeCpu->opK]) & Mask60;
    }

static void cpOp17(CpuContext *activeCpu)
    {
    /*
    **  BXi -Xk-Xj
    */
    activeCpu->regX[activeCpu->opI] = (activeCpu->regX[activeCpu->opJ] ^ ~activeCpu->regX[activeCpu->opK]) & Mask60;
    }

static void cpOp20(CpuContext *activeCpu)
    {
    /*
    **  LXi jk
    */
    u8 jk;

    jk = (u8)((activeCpu->opJ << 3) | activeCpu->opK);
    activeCpu->regX[activeCpu->opI] = shiftLeftCircular(activeCpu->regX[activeCpu->opI] & Mask60, jk);
    }

static void cpOp21(CpuContext *activeCpu)
    {
    /*
    **  AXi jk
    */
    u8 jk;

    jk = (u8)((activeCpu->opJ << 3) | activeCpu->opK);
    activeCpu->regX[activeCpu->opI] = shiftRightArithmetic(activeCpu->regX[activeCpu->opI] & Mask60, jk);
    }

static void cpOp22(CpuContext *activeCpu)
    {
    CpWord acc60;

    /*
    **  LXi Bj Xk
    */
    u32 count;

    count = activeCpu->regB[activeCpu->opJ] & Mask18;
    acc60 = activeCpu->regX[activeCpu->opK] & Mask60;

    if ((count & Sign18) == 0)
        {
        count        &= Mask6;
        activeCpu->regX[activeCpu->opI] = shiftLeftCircular(acc60, count);
        }
    else
        {
        count  = ~count;
        count &= Mask11;
        if ((count & ~Mask6) != 0)
            {
            activeCpu->regX[activeCpu->opI] = 0;
            }
        else
            {
            activeCpu->regX[activeCpu->opI] = shiftRightArithmetic(acc60, count);
            }
        }
    }

static void cpOp23(CpuContext *activeCpu)
    {
    CpWord acc60;

    /*
    **  AXi Bj Xk
    */
    u32 count;

    count = activeCpu->regB[activeCpu->opJ] & Mask18;
    acc60 = activeCpu->regX[activeCpu->opK] & Mask60;

    if ((count & Sign18) == 0)
        {
        count &= Mask11;
        if ((count & ~Mask6) != 0)
            {
            activeCpu->regX[activeCpu->opI] = 0;
            }
        else
            {
            activeCpu->regX[activeCpu->opI] = shiftRightArithmetic(acc60, count);
            }
        }
    else
        {
        count         = ~count;
        count        &= Mask6;
        activeCpu->regX[activeCpu->opI] = shiftLeftCircular(acc60, count);
        }
    }

static void cpOp24(CpuContext *activeCpu)
    {
    /*
    **  NXi Bj Xk
    */
    cpuFloatCheck(activeCpu, activeCpu->regX[activeCpu->opK]);
    activeCpu->regX[activeCpu->opI] = shiftNormalize(activeCpu->regX[activeCpu->opK], &activeCpu->regB[activeCpu->opJ], FALSE);
    cpuFloatExceptionHandler(activeCpu);
    }

static void cpOp25(CpuContext *activeCpu)
    {
    /*
    **  ZXi Bj Xk
    */
    cpuFloatCheck(activeCpu, activeCpu->regX[activeCpu->opK]);
    activeCpu->regX[activeCpu->opI] = shiftNormalize(activeCpu->regX[activeCpu->opK], &activeCpu->regB[activeCpu->opJ], TRUE);
    cpuFloatExceptionHandler(activeCpu);
    }

static void cpOp26(CpuContext *activeCpu)
    {
    /*
    **  UXi Bj Xk
    */
    if (activeCpu->opJ == 0)
        {
        activeCpu->regX[activeCpu->opI] = shiftUnpack(activeCpu->regX[activeCpu->opK], NULL);
        }
    else
        {
        activeCpu->regX[activeCpu->opI] = shiftUnpack(activeCpu->regX[activeCpu->opK], &activeCpu->regB[activeCpu->opJ]);
        }
    }

static void cpOp27(CpuContext *activeCpu)
    {
    /*
    **  PXi Bj Xk
    */
    if (activeCpu->opJ == 0)
        {
        activeCpu->regX[activeCpu->opI] = shiftPack(activeCpu->regX[activeCpu->opK], 0);
        }
    else
        {
        activeCpu->regX[activeCpu->opI] = shiftPack(activeCpu->regX[activeCpu->opK], activeCpu->regB[activeCpu->opJ]);
        }
    }

static void cpOp30(CpuContext *activeCpu)
    {
    /*
    **  FXi Xj+Xk
    */
    cpuFloatCheck(activeCpu, activeCpu->regX[activeCpu->opJ]);
    cpuFloatCheck(activeCpu, activeCpu->regX[activeCpu->opK]);
    activeCpu->regX[activeCpu->opI] = floatAdd(activeCpu->regX[activeCpu->opJ], activeCpu->regX[activeCpu->opK], FALSE, FALSE);
    cpuFloatCheck(activeCpu, activeCpu->regX[activeCpu->opI]);
    cpuFloatExceptionHandler(activeCpu);
    }

static void cpOp31(CpuContext *activeCpu)
    {
    /*
    **  FXi Xj-Xk
    */
    cpuFloatCheck(activeCpu, activeCpu->regX[activeCpu->opJ]);
    cpuFloatCheck(activeCpu, activeCpu->regX[activeCpu->opK]);
    activeCpu->regX[activeCpu->opI] = floatAdd(activeCpu->regX[activeCpu->opJ], (~activeCpu->regX[activeCpu->opK] & Mask60), FALSE, FALSE);
    cpuFloatCheck(activeCpu, activeCpu->regX[activeCpu->opI]);
    cpuFloatExceptionHandler(activeCpu);
    }

static void cpOp32(CpuContext *activeCpu)
    {
    /*
    **  DXi Xj+Xk
    */
    cpuFloatCheck(activeCpu, activeCpu->regX[activeCpu->opJ]);
    cpuFloatCheck(activeCpu, activeCpu->regX[activeCpu->opK]);
    activeCpu->regX[activeCpu->opI] = floatAdd(activeCpu->regX[activeCpu->opJ], activeCpu->regX[activeCpu->opK], FALSE, TRUE);
    cpuFloatCheck(activeCpu, activeCpu->regX[activeCpu->opI]);
    cpuFloatExceptionHandler(activeCpu);
    }

static void cpOp33(CpuContext *activeCpu)
    {
    /*
    **  DXi Xj-Xk
    */
    cpuFloatCheck(activeCpu, activeCpu->regX[activeCpu->opJ]);
    cpuFloatCheck(activeCpu, activeCpu->regX[activeCpu->opK]);
    activeCpu->regX[activeCpu->opI] = floatAdd(activeCpu->regX[activeCpu->opJ], (~activeCpu->regX[activeCpu->opK] & Mask60), FALSE, TRUE);
    cpuFloatCheck(activeCpu, activeCpu->regX[activeCpu->opI]);
    cpuFloatExceptionHandler(activeCpu);
    }

static void cpOp34(CpuContext *activeCpu)
    {
    /*
    **  RXi Xj+Xk
    */
    cpuFloatCheck(activeCpu, activeCpu->regX[activeCpu->opJ]);
    cpuFloatCheck(activeCpu, activeCpu->regX[activeCpu->opK]);
    activeCpu->regX[activeCpu->opI] = floatAdd(activeCpu->regX[activeCpu->opJ], activeCpu->regX[activeCpu->opK], TRUE, FALSE);
    cpuFloatCheck(activeCpu, activeCpu->regX[activeCpu->opI]);
    cpuFloatExceptionHandler(activeCpu);
    }

static void cpOp35(CpuContext *activeCpu)
    {
    /*
    **  RXi Xj-Xk
    */
    cpuFloatCheck(activeCpu, activeCpu->regX[activeCpu->opJ]);
    cpuFloatCheck(activeCpu, activeCpu->regX[activeCpu->opK]);
    activeCpu->regX[activeCpu->opI] = floatAdd(activeCpu->regX[activeCpu->opJ], (~activeCpu->regX[activeCpu->opK] & Mask60), TRUE, FALSE);
    cpuFloatCheck(activeCpu, activeCpu->regX[activeCpu->opI]);
    cpuFloatExceptionHandler(activeCpu);
    }

static void cpOp36(CpuContext *activeCpu)
    {
    CpWord acc60;

    /*
    **  IXi Xj+Xk
    */
    acc60 = (activeCpu->regX[activeCpu->opJ] & Mask60) - (~activeCpu->regX[activeCpu->opK] & Mask60);
    if ((acc60 & Overflow60) != 0)
        {
        acc60 -= 1;
        }

    activeCpu->regX[activeCpu->opI] = acc60 & Mask60;
    }

static void cpOp37(CpuContext *activeCpu)
    {
    CpWord acc60;

    /*
    **  IXi Xj-Xk
    */
    acc60 = (activeCpu->regX[activeCpu->opJ] & Mask60) - (activeCpu->regX[activeCpu->opK] & Mask60);
    if ((acc60 & Overflow60) != 0)
        {
        acc60 -= 1;
        }

    activeCpu->regX[activeCpu->opI] = acc60 & Mask60;
    }

static void cpOp40(CpuContext *activeCpu)
    {
    /*
    **  FXi Xj*Xk
    */
    cpuFloatCheck(activeCpu, activeCpu->regX[activeCpu->opJ]);
    cpuFloatCheck(activeCpu, activeCpu->regX[activeCpu->opK]);
    activeCpu->regX[activeCpu->opI] = floatMultiply(activeCpu->regX[activeCpu->opJ], activeCpu->regX[activeCpu->opK], FALSE, FALSE);
    cpuFloatCheck(activeCpu, activeCpu->regX[activeCpu->opI]);
    cpuFloatExceptionHandler(activeCpu);
    }

static void cpOp41(CpuContext *activeCpu)
    {
    /*
    **  RXi Xj*Xk
    */
    cpuFloatCheck(activeCpu, activeCpu->regX[activeCpu->opJ]);
    cpuFloatCheck(activeCpu, activeCpu->regX[activeCpu->opK]);
    activeCpu->regX[activeCpu->opI] = floatMultiply(activeCpu->regX[activeCpu->opJ], activeCpu->regX[activeCpu->opK], TRUE, FALSE);
    cpuFloatCheck(activeCpu, activeCpu->regX[activeCpu->opI]);
    cpuFloatExceptionHandler(activeCpu);
    }

static void cpOp42(CpuContext *activeCpu)
    {
    /*
    **  DXi Xj*Xk
    */
    cpuFloatCheck(activeCpu, activeCpu->regX[activeCpu->opJ]);
    cpuFloatCheck(activeCpu, activeCpu->regX[activeCpu->opK]);
    activeCpu->regX[activeCpu->opI] = floatMultiply(activeCpu->regX[activeCpu->opJ], activeCpu->regX[activeCpu->opK], FALSE, TRUE);
    cpuFloatCheck(activeCpu, activeCpu->regX[activeCpu->opI]);
    cpuFloatExceptionHandler(activeCpu);
    }

static void cpOp43(CpuContext *activeCpu)
    {
    /*
    **  MXi jk
    */
    u8 jk;

    jk = (u8)((activeCpu->opJ << 3) | activeCpu->opK);
    activeCpu->regX[activeCpu->opI] = shiftMask(jk);
    }

static void cpOp44(CpuContext *activeCpu)
    {
    /*
    **  FXi Xj/Xk
    */
    cpuFloatCheck(activeCpu, activeCpu->regX[activeCpu->opJ]);
    cpuFloatCheck(activeCpu, activeCpu->regX[activeCpu->opK]);
    activeCpu->regX[activeCpu->opI] = floatDivide(activeCpu->regX[activeCpu->opJ], activeCpu->regX[activeCpu->opK], FALSE);
    cpuFloatCheck(activeCpu, activeCpu->regX[activeCpu->opI]);
    cpuFloatExceptionHandler(activeCpu);
#if CcSMM_EJT
    skipStep = 20;
#endif
    }

static void cpOp45(CpuContext *activeCpu)
    {
    /*
    **  RXi Xj/Xk
    */
    cpuFloatCheck(activeCpu, activeCpu->regX[activeCpu->opJ]);
    cpuFloatCheck(activeCpu, activeCpu->regX[activeCpu->opK]);
    activeCpu->regX[activeCpu->opI] = floatDivide(activeCpu->regX[activeCpu->opJ], activeCpu->regX[activeCpu->opK], TRUE);
    cpuFloatCheck(activeCpu, activeCpu->regX[activeCpu->opI]);
    cpuFloatExceptionHandler(activeCpu);
    }

static void cpOp46(CpuContext *activeCpu)
    {
    switch (activeCpu->opI)
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
            cpuOpIllegal(activeCpu);

            return;
            }

        if (activeCpu->opOffset != 45)
            {
            if ((features & IsSeries70) == 0)
                {
                /*
                **  Instruction must be in parcel 0, if not, it is interpreted as a
                **  pass instruction (NO) on Cyber 70 series or as illegal on anything
                **  else.
                */
                cpuOpIllegal(activeCpu);
                }

            return;
            }
        break;
        }

    switch (activeCpu->opI)
        {
    case 4:
        /*
        **  Move indirect.
        */
        cpuCmuMoveIndirect(activeCpu);
        break;

    case 5:
        /*
        **  Move direct.
        */
        cpuCmuMoveDirect(activeCpu);
        break;

    case 6:
        /*
        **  Compare collated.
        */
        cpuCmuCompareCollated(activeCpu);
        break;

    case 7:
        /*
        **  Compare uncollated.
        */
        cpuCmuCompareUncollated(activeCpu);
        break;
        }
    }

static void cpOp47(CpuContext *activeCpu)
    {
    CpWord acc60;

    /*
    **  CXi Xk
    */
    acc60         = activeCpu->regX[activeCpu->opK] & Mask60;
    acc60         = ((acc60 & 0xAAAAAAAAAAAAAAAA) >> 1) + (acc60 & 0x5555555555555555);
    acc60         = ((acc60 & 0xCCCCCCCCCCCCCCCC) >> 2) + (acc60 & 0x3333333333333333);
    acc60         = ((acc60 & 0xF0F0F0F0F0F0F0F0) >> 4) + (acc60 & 0x0F0F0F0F0F0F0F0F);
    acc60         = ((acc60 & 0xFF00FF00FF00FF00) >> 8) + (acc60 & 0x00FF00FF00FF00FF);
    acc60         = ((acc60 & 0xFFFF0000FFFF0000) >> 16) + (acc60 & 0x0000FFFF0000FFFF);
    acc60         = ((acc60 & 0xFFFFFFFF00000000) >> 32) + (acc60 & 0x00000000FFFFFFFF);
    activeCpu->regX[activeCpu->opI] = acc60 & Mask60;
    }

static void cpOp50(CpuContext *activeCpu)
    {
    /*
    **  SAi Aj+K
    */
    activeCpu->regA[activeCpu->opI] = cpuAdd18(activeCpu->regA[activeCpu->opJ], activeCpu->opAddress);

    cpuRegASemantics(activeCpu);
    }

static void cpOp51(CpuContext *activeCpu)
    {
    /*
    **  SAi Bj+K
    */
    activeCpu->regA[activeCpu->opI] = cpuAdd18(activeCpu->regB[activeCpu->opJ], activeCpu->opAddress);

    cpuRegASemantics(activeCpu);
    }

static void cpOp52(CpuContext *activeCpu)
    {
    /*
    **  SAi Xj+K
    */
    activeCpu->regA[activeCpu->opI] = cpuAdd18((u32)activeCpu->regX[activeCpu->opJ], activeCpu->opAddress);

    cpuRegASemantics(activeCpu);
    }

static void cpOp53(CpuContext *activeCpu)
    {
    /*
    **  SAi Xj+Bk
    */
    activeCpu->regA[activeCpu->opI] = cpuAdd18((u32)activeCpu->regX[activeCpu->opJ], activeCpu->regB[activeCpu->opK]);

    cpuRegASemantics(activeCpu);
    }

static void cpOp54(CpuContext *activeCpu)
    {
    /*
    **  SAi Aj+Bk
    */
    activeCpu->regA[activeCpu->opI] = cpuAdd18(activeCpu->regA[activeCpu->opJ], activeCpu->regB[activeCpu->opK]);

    cpuRegASemantics(activeCpu);
    }

static void cpOp55(CpuContext *activeCpu)
    {
    /*
    **  SAi Aj-Bk
    */
    activeCpu->regA[activeCpu->opI] = cpuSubtract18(activeCpu->regA[activeCpu->opJ], activeCpu->regB[activeCpu->opK]);

    cpuRegASemantics(activeCpu);
    }

static void cpOp56(CpuContext *activeCpu)
    {
    /*
    **  SAi Bj+Bk
    */
    activeCpu->regA[activeCpu->opI] = cpuAdd18(activeCpu->regB[activeCpu->opJ], activeCpu->regB[activeCpu->opK]);

    cpuRegASemantics(activeCpu);
    }

static void cpOp57(CpuContext *activeCpu)
    {
    /*
    **  SAi Bj-Bk
    */
    activeCpu->regA[activeCpu->opI] = cpuSubtract18(activeCpu->regB[activeCpu->opJ], activeCpu->regB[activeCpu->opK]);

    cpuRegASemantics(activeCpu);
    }

static void cpOp60(CpuContext *activeCpu)
    {
    /*
    **  SBi Aj+K
    */
    activeCpu->regB[activeCpu->opI] = cpuAdd18(activeCpu->regA[activeCpu->opJ], activeCpu->opAddress);
    }

static void cpOp61(CpuContext *activeCpu)
    {
    /*
    **  SBi Bj+K
    */
    activeCpu->regB[activeCpu->opI] = cpuAdd18(activeCpu->regB[activeCpu->opJ], activeCpu->opAddress);
    }

static void cpOp62(CpuContext *activeCpu)
    {
    /*
    **  SBi Xj+K
    */
    activeCpu->regB[activeCpu->opI] = cpuAdd18((u32)activeCpu->regX[activeCpu->opJ], activeCpu->opAddress);
    }

static void cpOp63(CpuContext *activeCpu)
    {
    /*
    **  SBi Xj+Bk
    */
    activeCpu->regB[activeCpu->opI] = cpuAdd18((u32)activeCpu->regX[activeCpu->opJ], activeCpu->regB[activeCpu->opK]);
    }

static void cpOp64(CpuContext *activeCpu)
    {
    /*
    **  SBi Aj+Bk
    */
    activeCpu->regB[activeCpu->opI] = cpuAdd18(activeCpu->regA[activeCpu->opJ], activeCpu->regB[activeCpu->opK]);
    }

static void cpOp65(CpuContext *activeCpu)
    {
    /*
    **  SBi Aj-Bk
    */
    activeCpu->regB[activeCpu->opI] = cpuSubtract18(activeCpu->regA[activeCpu->opJ], activeCpu->regB[activeCpu->opK]);
    }

static void cpOp66(CpuContext *activeCpu)
    {
    if ((activeCpu->opI == 0) && ((features & IsSeries800) != 0))
        {
        /*
        **  CR Xj,Xk
        */
        cpuReadMem(activeCpu, (u32)(activeCpu->regX[activeCpu->opK]) & Mask21, activeCpu->regX + activeCpu->opJ);

        return;
        }

    /*
    **  SBi Bj+Bk
    */
    activeCpu->regB[activeCpu->opI] = cpuAdd18(activeCpu->regB[activeCpu->opJ], activeCpu->regB[activeCpu->opK]);
    }

static void cpOp67(CpuContext *activeCpu)
    {
    if ((activeCpu->opI == 0) && ((features & IsSeries800) != 0))
        {
        /*
        **  CW Xj,Xk
        */
        cpuWriteMem(activeCpu, (u32)(activeCpu->regX[activeCpu->opK]) & Mask21, activeCpu->regX + activeCpu->opJ);

        return;
        }

    /*
    **  SBi Bj-Bk
    */
    activeCpu->regB[activeCpu->opI] = cpuSubtract18(activeCpu->regB[activeCpu->opJ], activeCpu->regB[activeCpu->opK]);
    }

static void cpOp70(CpuContext *activeCpu)
    {
    CpWord acc60;

    /*
    **  SXi Aj+K
    */
    acc60 = (CpWord)cpuAdd18(activeCpu->regA[activeCpu->opJ], activeCpu->opAddress);

    if ((acc60 & 0400000) != 0)
        {
        acc60 |= SignExtend18To60;
        }

    activeCpu->regX[activeCpu->opI] = acc60 & Mask60;
    }

static void cpOp71(CpuContext *activeCpu)
    {
    CpWord acc60;

    /*
    **  SXi Bj+K
    */
    acc60 = (CpWord)cpuAdd18(activeCpu->regB[activeCpu->opJ], activeCpu->opAddress);

    if ((acc60 & 0400000) != 0)
        {
        acc60 |= SignExtend18To60;
        }

    activeCpu->regX[activeCpu->opI] = acc60 & Mask60;
    }

static void cpOp72(CpuContext *activeCpu)
    {
    CpWord acc60;

    /*
    **  SXi Xj+K
    */
    acc60 = (CpWord)cpuAdd18((u32)activeCpu->regX[activeCpu->opJ], activeCpu->opAddress);

    if ((acc60 & 0400000) != 0)
        {
        acc60 |= SignExtend18To60;
        }

    activeCpu->regX[activeCpu->opI] = acc60 & Mask60;
    }

static void cpOp73(CpuContext *activeCpu)
    {
    CpWord acc60;

    /*
    **  SXi Xj+Bk
    */
    acc60 = (CpWord)cpuAdd18((u32)activeCpu->regX[activeCpu->opJ], activeCpu->regB[activeCpu->opK]);

    if ((acc60 & 0400000) != 0)
        {
        acc60 |= SignExtend18To60;
        }

    activeCpu->regX[activeCpu->opI] = acc60 & Mask60;
    }

static void cpOp74(CpuContext *activeCpu)
    {
    CpWord acc60;

    /*
    **  SXi Aj+Bk
    */
    acc60 = (CpWord)cpuAdd18(activeCpu->regA[activeCpu->opJ], activeCpu->regB[activeCpu->opK]);

    if ((acc60 & 0400000) != 0)
        {
        acc60 |= SignExtend18To60;
        }

    activeCpu->regX[activeCpu->opI] = acc60 & Mask60;
    }

static void cpOp75(CpuContext *activeCpu)
    {
    CpWord acc60;

    /*
    **  SXi Aj-Bk
    */
    acc60 = (CpWord)cpuSubtract18(activeCpu->regA[activeCpu->opJ], activeCpu->regB[activeCpu->opK]);


    if ((acc60 & 0400000) != 0)
        {
        acc60 |= SignExtend18To60;
        }

    activeCpu->regX[activeCpu->opI] = acc60 & Mask60;
    }

static void cpOp76(CpuContext *activeCpu)
    {
    CpWord acc60;

    /*
    **  SXi Bj+Bk
    */
    acc60 = (CpWord)cpuAdd18(activeCpu->regB[activeCpu->opJ], activeCpu->regB[activeCpu->opK]);

    if ((acc60 & 0400000) != 0)
        {
        acc60 |= SignExtend18To60;
        }

    activeCpu->regX[activeCpu->opI] = acc60 & Mask60;
    }

static void cpOp77(CpuContext *activeCpu)
    {
    CpWord acc60;

    /*
    **  SXi Bj-Bk
    */
    acc60 = (CpWord)cpuSubtract18(activeCpu->regB[activeCpu->opJ], activeCpu->regB[activeCpu->opK]);

    if ((acc60 & 0400000) != 0)
        {
        acc60 |= SignExtend18To60;
        }

    activeCpu->regX[activeCpu->opI] = acc60 & Mask60;
    }

/*---------------------------  End Of File  ------------------------------*/
