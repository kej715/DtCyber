/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**
**  Name: trace.c
**
**  Description:
**      Trace execution.
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
**  PPU command adressing modes.
*/
#define AN       1
#define Amd      2
#define Ar       3
#define Ad       4
#define Adm      5

/*
**  CPU command adressing modes.
*/
#define CN       1
#define CK       2
#define Ci       3
#define Cij      4
#define CiK      5
#define CjK      6
#define Cijk     7
#define Cik      8
#define Cikj     9
#define CijK     10
#define Cjk      11
#define Cj       12
#define CLINK    100

/*
**  CPU register set markers.
*/
#define R        1
#define RAA      2
#define RAAB     3
#define RAB      4
#define RABB     5
#define RAX      6
#define RAXB     7
#define RBA      8
#define RBAB     9
#define RBB      10
#define RBBB     11
#define RBX      12
#define RBXB     13
#define RX       14
#define RXA      15
#define RXAB     16
#define RXB      17
#define RXBB     18
#define RXBX     19
#define RXX      20
#define RXXB     21
#define RXXX     22
#define RZB      23
#define RZX      24
#define RXNX     25
#define RNXX     26
#define RNXN     27

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
typedef struct decPpControl
    {
    u8   mode;
    char *mnemonic;
    bool hasOp2;
    char *mnemonic2;
    } DecPpControl;

typedef struct decCpControl
    {
    u8   mode;
    char *mnemonic;
    u8   regSet;
    } DecCpControl;

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/

/*
**  ----------------
**  Public Variables
**  ----------------
*/
u32 traceMask = 0;
u32 traceSequenceNo;

/*
**  -----------------
**  Private Variables
**  -----------------
*/
FILE        **cpuF;
static FILE *devF;
static FILE **ppuF;

static DecPpControl ppDecode170[] =
    {
    AN,  "PSN", FALSE, NULL, // 00
    Amd, "LJM", FALSE, NULL, // 01
    Amd, "RJM", FALSE, NULL, // 02
    Ar,  "UJN", FALSE, NULL, // 03
    Ar,  "ZJN", FALSE, NULL, // 04
    Ar,  "NJN", FALSE, NULL, // 05
    Ar,  "PJN", FALSE, NULL, // 06
    Ar,  "MJN", FALSE, NULL, // 07

    Ar,  "SHN", FALSE, NULL, // 10
    Ad,  "LMN", FALSE, NULL, // 11
    Ad,  "LPN", FALSE, NULL, // 12
    Ad,  "SCN", FALSE, NULL, // 13
    Ad,  "LDN", FALSE, NULL, // 14
    Ad,  "LCN", FALSE, NULL, // 15
    Ad,  "ADN", FALSE, NULL, // 16
    Ad,  "SBN", FALSE, NULL, // 17

    Adm, "LDC", FALSE, NULL, // 20
    Adm, "ADC", FALSE, NULL, // 21
    Adm, "LPC", FALSE, NULL, // 22
    Adm, "LMC", FALSE, NULL, // 23
    AN,  "PSN", FALSE, NULL, // 24
    AN,  "PSN", FALSE, NULL, // 25
    Ad,  "EXN", FALSE, NULL, // 26
    Ad,  "RPN", FALSE, NULL, // 27

    Ad,  "LDD", FALSE, NULL, // 30
    Ad,  "ADD", FALSE, NULL, // 31
    Ad,  "SBD", FALSE, NULL, // 32
    Ad,  "LMD", FALSE, NULL, // 33
    Ad,  "STD", FALSE, NULL, // 34
    Ad,  "RAD", FALSE, NULL, // 35
    Ad,  "AOD", FALSE, NULL, // 36
    Ad,  "SOD", FALSE, NULL, // 37

    Ad,  "LDI", FALSE, NULL, // 40
    Ad,  "ADI", FALSE, NULL, // 41
    Ad,  "SBI", FALSE, NULL, // 42
    Ad,  "LMI", FALSE, NULL, // 43
    Ad,  "STI", FALSE, NULL, // 44
    Ad,  "RAI", FALSE, NULL, // 45
    Ad,  "AOI", FALSE, NULL, // 46
    Ad,  "SOI", FALSE, NULL, // 47

    Amd, "LDM", FALSE, NULL, // 50
    Amd, "ADM", FALSE, NULL, // 51
    Amd, "SBM", FALSE, NULL, // 52
    Amd, "LMM", FALSE, NULL, // 53
    Amd, "STM", FALSE, NULL, // 54
    Amd, "RAM", FALSE, NULL, // 55
    Amd, "AOM", FALSE, NULL, // 56
    Amd, "SOM", FALSE, NULL, // 57

    Ad,  "CRD", FALSE, NULL, // 60
    Amd, "CRM", FALSE, NULL, // 61
    Ad,  "CWD", FALSE, NULL, // 62
    Amd, "CWM", FALSE, NULL, // 63
    Amd, "AJM", TRUE, "SCF", // 64
    Amd, "IJM", TRUE, "CCF", // 65
    Amd, "FJM", TRUE, "SFM", // 66
    Amd, "EJM", TRUE, "CFM", // 67

    Ad,  "IAN", FALSE, NULL, // 70
    Amd, "IAM", FALSE, NULL, // 71
    Ad,  "OAN", FALSE, NULL, // 72
    Amd, "OAM", FALSE, NULL, // 73
    Ad,  "ACN", FALSE, NULL, // 74
    Ad,  "DCN", FALSE, NULL, // 75
    Ad,  "FAN", FALSE, NULL, // 76
    Amd, "FNC", FALSE, NULL  // 77
    };

static DecPpControl ppDecode180[] =
    {
    Ad,  "RDSL", FALSE, NULL, // 1000
    Ad,  "RDCL", FALSE, NULL, // 1001
    AN,  "PSN",  FALSE, NULL, // 1002
    AN,  "PSN",  FALSE, NULL, // 1003
    AN,  "PSN",  FALSE, NULL, // 1004
    AN,  "PSN",  FALSE, NULL, // 1005
    AN,  "PSN",  FALSE, NULL, // 1006
    AN,  "PSN",  FALSE, NULL, // 1007

    AN,  "PSN",  FALSE, NULL, // 1010
    AN,  "PSN",  FALSE, NULL, // 1011
    AN,  "PSN",  FALSE, NULL, // 1012
    AN,  "PSN",  FALSE, NULL, // 1013
    AN,  "PSN",  FALSE, NULL, // 1014
    AN,  "PSN",  FALSE, NULL, // 1015
    AN,  "PSN",  FALSE, NULL, // 1016
    AN,  "PSN",  FALSE, NULL, // 1017

    AN,  "PSN",  FALSE, NULL, // 1020
    AN,  "PSN",  FALSE, NULL, // 1021
    Ad,  "LPDL", FALSE, NULL, // 1022
    Ad,  "LPIL", FALSE, NULL, // 1023
    Amd, "LPML", FALSE, NULL, // 1024
    AN,  "PSN",  FALSE, NULL, // 1025
    AN,  "PSN",  FALSE, NULL, // 1026
    AN,  "PSN",  FALSE, NULL, // 1027

    Ad,  "LDDL", FALSE, NULL, // 1030
    Ad,  "ADDL", FALSE, NULL, // 1031
    Ad,  "SBDL", FALSE, NULL, // 1032
    Ad,  "LMDL", FALSE, NULL, // 1033
    Ad,  "STDL", FALSE, NULL, // 1034
    Ad,  "RADL", FALSE, NULL, // 1035
    Ad,  "AODL", FALSE, NULL, // 1036
    Ad,  "SODL", FALSE, NULL, // 1037

    Ad,  "LDIL", FALSE, NULL, // 1040
    Ad,  "ADIL", FALSE, NULL, // 1041
    Ad,  "SBIL", FALSE, NULL, // 1042
    Ad,  "LMIL", FALSE, NULL, // 1043
    Ad,  "STIL", FALSE, NULL, // 1044
    Ad,  "RAIL", FALSE, NULL, // 1045
    Ad,  "AOIL", FALSE, NULL, // 1046
    Ad,  "SOIL", FALSE, NULL, // 1047

    Amd, "LDML", FALSE, NULL, // 1050
    Amd, "ADML", FALSE, NULL, // 1051
    Amd, "SBML", FALSE, NULL, // 1052
    Amd, "LMML", FALSE, NULL, // 1053
    Amd, "STML", FALSE, NULL, // 1054
    Amd, "RAML", FALSE, NULL, // 1055
    Amd, "AOML", FALSE, NULL, // 1056
    Amd, "SOML", FALSE, NULL, // 1057

    Ad,  "CRDL", FALSE, NULL, // 1060
    Amd, "CRML", FALSE, NULL, // 1061
    Ad,  "CWDL", FALSE, NULL, // 1062
    Amd, "CWML", FALSE, NULL, // 1063
    Amd, "FSJM", FALSE, NULL, // 1064
    Amd, "FCJM", FALSE, NULL, // 1065
    AN,  "PSN",  FALSE, NULL, // 1066
    AN,  "PSN",  FALSE, NULL, // 1067

    AN,  "PSN",  FALSE, NULL, // 1070
    Amd, "IAPM", FALSE, NULL, // 1071
    AN,  "PSN",  FALSE, NULL, // 1072
    Amd, "OAPM", FALSE, NULL, // 1073
    AN,  "PSN",  FALSE, NULL, // 1074
    AN,  "PSN",  FALSE, NULL, // 1075
    AN,  "PSN",  FALSE, NULL, // 1076
    AN , "PSN",  FALSE, NULL  // 1077
    };

static DecCpControl rjDecode[010] =
    {
    CK,  "RJ    %6.6o", R,                      // 0
    CjK, "REC   B%o+%6.6o", RZB,                // 1
    CjK, "WEC   B%o+%6.6o", RZB,                // 2
    CK,  "XJ    %6.6o", R,                      // 3
    Cjk, "RX    X%o,X%o", RNXX,                 // 4
    Cjk, "WX    X%o,X%o", RNXX,                 // 5
    Cj,  "RC    X%o", RNXN,                     // 6
    CN,  "Illegal", R,                          // 7
    };

static DecCpControl cjDecode[010] =
    {
    CjK, "ZR    X%o,%6.6o", RZX,                // 0
    CjK, "NZ    X%o,%6.6o", RZX,                // 1
    CjK, "PL    X%o,%6.6o", RZX,                // 2
    CjK, "NG    X%o,%6.6o", RZX,                // 3
    CjK, "IR    X%o,%6.6o", RZX,                // 4
    CjK, "OR    X%o,%6.6o", RZX,                // 5
    CjK, "DF    X%o,%6.6o", RZX,                // 6
    CjK, "ID    X%o,%6.6o", RZX,                // 7
    };

static DecCpControl cpDecode[0100] =
    {
    CN,    "PS", R,                             // 00
    CLINK, (char *)rjDecode, R,                 // 01
    CiK,   "JP    %6.6o", R,                    // 02
    CLINK, (char *)cjDecode, R,                 // 03
    CijK,  "EQ    B%o,B%o,%6.6o", RBB,          // 04
    CijK,  "NE    B%o,B%o,%6.6o", RBB,          // 05
    CijK,  "GE    B%o,B%o,%6.6o", RBB,          // 06
    CijK,  "LT    B%o,B%o,%6.6o", RBB,          // 07

    Cij,   "BX%o   X%o", RXX,                   // 10
    Cijk,  "BX%o   X%o*X%o", RXXX,              // 11
    Cijk,  "BX%o   X%o+X%o", RXXX,              // 12
    Cijk,  "BX%o   X%o-X%o", RXXX,              // 13
    Cik,   "BX%o   -X%o", RXXX,                 // 14
    Cikj,  "BX%o   -X%o*X%o", RXXX,             // 15
    Cikj,  "BX%o   -X%o+X%o", RXXX,             // 16
    Cikj,  "BX%o   -X%o-X%o", RXXX,             // 17

    Cijk,  "LX%o   %o%o", RX,                   // 20
    Cijk,  "AX%o   %o%o", RX,                   // 21
    Cijk,  "LX%o   B%o,X%o", RXBX,              // 22
    Cijk,  "AX%o   B%o,X%o", RXBX,              // 23
    Cijk,  "NX%o   B%o,X%o", RXBX,              // 24
    Cijk,  "ZX%o   B%o,X%o", RXBX,              // 25
    Cijk,  "UX%o   B%o,X%o", RXBX,              // 26
    Cijk,  "PX%o   B%o,X%o", RXBX,              // 27

    Cijk,  "FX%o   X%o+X%o", RXXX,              // 30
    Cijk,  "FX%o   X%o-X%o", RXXX,              // 31
    Cijk,  "DX%o   X%o+X%o", RXXX,              // 32
    Cijk,  "DX%o   X%o-X%o", RXXX,              // 33
    Cijk,  "RX%o   X%o+X%o", RXXX,              // 34
    Cijk,  "RX%o   X%o-X%o", RXXX,              // 35
    Cijk,  "IX%o   X%o+X%o", RXXX,              // 36
    Cijk,  "IX%o   X%o-X%o", RXXX,              // 37

    Cijk,  "FX%o   X%o*X%o", RXXX,              // 40
    Cijk,  "RX%o   X%o*X%o", RXXX,              // 41
    Cijk,  "DX%o   X%o*X%o", RXXX,              // 42
    Cijk,  "MX%o   %o%o", RX,                   // 43
    Cijk,  "FX%o   X%o/X%o", RXXX,              // 44
    Cijk,  "RX%o   X%o/X%o", RXXX,              // 45
    CN,    "NO", R,                             // 46
    Cik,   "CX%o   X%o", RXNX,                  // 47

    CijK,  "SA%o   A%o+%6.6o", RAA,             // 50
    CijK,  "SA%o   B%o+%6.6o", RAB,             // 51
    CijK,  "SA%o   X%o+%6.6o", RAX,             // 52
    Cijk,  "SA%o   X%o+B%o", RAXB,              // 53
    Cijk,  "SA%o   A%o+B%o", RAAB,              // 54
    Cijk,  "SA%o   A%o-B%o", RAAB,              // 55
    Cijk,  "SA%o   B%o+B%o", RABB,              // 56
    Cijk,  "SA%o   B%o-B%o", RABB,              // 57

    CijK,  "SB%o   A%o+%6.6o", RBA,             // 60
    CijK,  "SB%o   B%o+%6.6o", RBB,             // 61
    CijK,  "SB%o   X%o+%6.6o", RBX,             // 62
    Cijk,  "SB%o   X%o+B%o", RBXB,              // 63
    Cijk,  "SB%o   A%o+B%o", RBAB,              // 64
    Cijk,  "SB%o   A%o-B%o", RBAB,              // 65
    Cijk,  "SB%o   B%o+B%o", RBBB,              // 66
    Cijk,  "SB%o   B%o-B%o", RBBB,              // 67

    CijK,  "SX%o   A%o+%6.6o", RXA,             // 70
    CijK,  "SX%o   B%o+%6.6o", RXB,             // 71
    CijK,  "SX%o   X%o+%6.6o", RXX,             // 72
    Cijk,  "SX%o   X%o+B%o", RXXB,              // 73
    Cijk,  "SX%o   A%o+B%o", RXAB,              // 74
    Cijk,  "SX%o   A%o-B%o", RXAB,              // 75
    Cijk,  "SX%o   B%o+B%o", RXBB,              // 76
    Cijk,  "SX%o   B%o-B%o", RXBB,              // 77
    };

/*
 **--------------------------------------------------------------------------
 **
 **  Public Functions
 **
 **--------------------------------------------------------------------------
 */

/*--------------------------------------------------------------------------
**  Purpose:        Initialise execution trace.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void traceInit(void)
    {
    u8   cp;
    u8   pp;
    char fileName[20];

    devF = fopen("device.trc", "wt");
    if (devF == NULL)
        {
        logDtError(LogErrorLocation, "Can't open device.trc - aborting\n");
        exit(1);
        }

    cpuF = calloc(cpuCount, sizeof(FILE *));
    if (cpuF == NULL)
        {
        logDtError(LogErrorLocation, "Failed to allocate CPU trace FILE pointers - aborting\n");
        exit(1);
        }
    for (cp = 0; cp < cpuCount; cp++)
        {
        sprintf(fileName, "cpu%o.trc", cp);
        cpuF[cp] = fopen(fileName, "wt");
        if (cpuF[cp] == NULL)
            {
            logDtError(LogErrorLocation, "Can't open cpu[%o] trace (%s) - aborting\n", cp, fileName);
            exit(1);
            }
        }

    ppuF = calloc(ppuCount, sizeof(FILE *));
    if (ppuF == NULL)
        {
        logDtError(LogErrorLocation, "Failed to allocate PP trace FILE pointers - aborting\n");
        exit(1);
        }

    for (pp = 0; pp < ppuCount; pp++)
        {
        sprintf(fileName, "ppu%02o.trc", pp < 10 ? pp : (pp - 10) + 020);
        ppuF[pp] = fopen(fileName, "wt");
        if (ppuF[pp] == NULL)
            {
            logDtError(LogErrorLocation, "Can't open ppu[%02o] trace (%s) - aborting\n", pp, fileName);
            exit(1);
            }
        }

    traceSequenceNo = 0;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Terminate all traces.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void traceTerminate(void)
    {
    u8 cp;
    u8 pp;

    for (cp = 0; cp < cpuCount; cp++)
        {
        if (cpuF[cp] != NULL)
            {
            fclose(cpuF[cp]);
            }
        }

    free(cpuF);

    for (pp = 0; pp < ppuCount; pp++)
        {
        if (ppuF[pp] != NULL)
            {
            fclose(ppuF[pp]);
            }
        }

    free(ppuF);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Output CPU opcode.
**
**  Parameters:     Name        Description.
**                  cpu         Pointer to CPU context
**                  opFm        Opcode
**                  opI         i
**                  opJ         j
**                  opK         k
**                  opAddress   jk
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void traceCpu(CpuContext *cpu, u32 p, u8 opFm, u8 opI, u8 opJ, u8 opK, u32 opAddress)
    {
    u8           addrMode;
    bool         link    = TRUE;
    static bool  oneIdle = TRUE;
    DecCpControl *decode = cpDecode;
    static char  str[80];

    /*
    **  Bail out if no trace of the CPU is requested.
    */
    if ((traceMask & TraceCpu) == 0)
        {
        return;
        }

#if 0
    /*
    **  Don't trace Scope 3.1 idle loop.
    */
    if ((cpu->regRaCm == 02020) && (cpu->regP == 2))
        {
        if (!oneIdle)
            {
            return;
            }
        else
            {
            oneIdle = FALSE;
            }
        }
    else
        {
        oneIdle = TRUE;
        }
#endif

#if 0
    for (i = 0; i < 8; i++)
        {
        data = cpu->regX[i];
        fprintf(cpuF[cpu->id], "        A%d %06.6o  X%d %04.4o %04.4o %04.4o %04.4o %04.4o   B%d %06.6o\n",
                i, cpu->regA[i], i,
                (PpWord)((data >> 48) & Mask12),
                (PpWord)((data >> 36) & Mask12),
                (PpWord)((data >> 24) & Mask12),
                (PpWord)((data >> 12) & Mask12),
                (PpWord)((data) & Mask12),
                i, cpu->regB[i]);
        }
#endif

    /*
    **  Print sequence no.
    */
    traceSequenceNo += 1;
    fprintf(cpuF[cpu->id], "%06d ", traceSequenceNo);

    /*
    **  Print program counter and opcode.
    */
    fprintf(cpuF[cpu->id], "%6.6o  ", p);
    fprintf(cpuF[cpu->id], "%02o %o %o %o   ", opFm, opI, opJ, opK);        // << not quite correct, but still nice for debugging

    /*
    **  Print opcode mnemonic and operands.
    */
    addrMode = decode[opFm].mode;

    if ((opFm == 066) && (opI == 0))
        {
        sprintf(str, "CRX%o  X%o", opJ, opK);
        fprintf(cpuF[cpu->id], "%-30s", str);
        fprintf(cpuF[cpu->id], "X%d=" FMT60_020o "   ", opJ, cpu->regX[opJ]);
        fprintf(cpuF[cpu->id], "X%d=" FMT60_020o "   ", opK, cpu->regX[opK]);
        fprintf(cpuF[cpu->id], "\n");

        return;
        }

    if ((opFm == 067) && (opI == 0))
        {
        sprintf(str, "CWX%o  X%o", opJ, opK);
        fprintf(cpuF[cpu->id], "%-30s", str);
        fprintf(cpuF[cpu->id], "X%d=" FMT60_020o "   ", opJ, cpu->regX[opJ]);
        fprintf(cpuF[cpu->id], "X%d=" FMT60_020o "   ", opK, cpu->regX[opK]);
        fprintf(cpuF[cpu->id], "\n");

        return;
        }

    while (link)
        {
        link = FALSE;

        switch (addrMode)
            {
        case CN:
            strcpy(str, decode[opFm].mnemonic);
            break;

        case CK:
            sprintf(str, decode[opFm].mnemonic, opAddress);
            break;

        case Ci:
            sprintf(str, decode[opFm].mnemonic, opI);
            break;

        case Cij:
            sprintf(str, decode[opFm].mnemonic, opI, opJ);
            break;

        case CiK:
            sprintf(str, decode[opFm].mnemonic, cpu->regB[opI] + opAddress);
            break;

        case CjK:
            sprintf(str, decode[opFm].mnemonic, opJ, opAddress);
            break;

        case Cijk:
            sprintf(str, decode[opFm].mnemonic, opI, opJ, opK);
            break;

        case Cik:
            sprintf(str, decode[opFm].mnemonic, opI, opK);
            break;

        case Cikj:
            sprintf(str, decode[opFm].mnemonic, opI, opK, opJ);
            break;

        case CijK:
            sprintf(str, decode[opFm].mnemonic, opI, opJ, opAddress);
            break;

        case Cjk:
            sprintf(str, decode[opFm].mnemonic, opJ, opK);
            break;

        case Cj:
            sprintf(str, decode[opFm].mnemonic, opJ);
            break;

        case CLINK:
            decode   = (DecCpControl *)decode[opFm].mnemonic;
            opFm     = opI;
            addrMode = decode[opFm].mode;
            link     = TRUE;
            break;

        default:
            sprintf(str, "unsupported mode %02o", opFm);
            break;
            }
        }

    fprintf(cpuF[cpu->id], "%-30s", str);

    /*
    **  Dump relevant register set.
    */
    switch (decode[opFm].regSet)
        {
    case R:
        break;

    case RAA:
        fprintf(cpuF[cpu->id], "A%d=%06o    ", opI, cpu->regA[opI]);
        fprintf(cpuF[cpu->id], "A%d=%06o    ", opJ, cpu->regA[opJ]);
        fprintf(cpuF[cpu->id], "X%d=" FMT60_020o "   ", opI, cpu->regX[opI]);
        break;

    case RAAB:
        fprintf(cpuF[cpu->id], "A%d=%06o    ", opI, cpu->regA[opI]);
        fprintf(cpuF[cpu->id], "A%d=%06o    ", opJ, cpu->regA[opJ]);
        fprintf(cpuF[cpu->id], "B%d=%06o    ", opK, cpu->regB[opK]);
        fprintf(cpuF[cpu->id], "X%d=" FMT60_020o "   ", opI, cpu->regX[opI]);
        break;

    case RAB:
        fprintf(cpuF[cpu->id], "A%d=%06o    ", opI, cpu->regA[opI]);
        fprintf(cpuF[cpu->id], "B%d=%06o    ", opJ, cpu->regB[opJ]);
        fprintf(cpuF[cpu->id], "X%d=" FMT60_020o "   ", opI, cpu->regX[opI]);
        break;

    case RABB:
        fprintf(cpuF[cpu->id], "A%d=%06o    ", opI, cpu->regA[opI]);
        fprintf(cpuF[cpu->id], "B%d=%06o    ", opJ, cpu->regB[opJ]);
        fprintf(cpuF[cpu->id], "B%d=%06o    ", opK, cpu->regB[opK]);
        fprintf(cpuF[cpu->id], "X%d=" FMT60_020o "   ", opI, cpu->regX[opI]);
        break;

    case RAX:
        fprintf(cpuF[cpu->id], "A%d=%06o    ", opI, cpu->regA[opI]);
        fprintf(cpuF[cpu->id], "X%d=" FMT60_020o "   ", opJ, cpu->regX[opJ]);
        fprintf(cpuF[cpu->id], "X%d=" FMT60_020o "   ", opI, cpu->regX[opI]);
        break;

    case RAXB:
        fprintf(cpuF[cpu->id], "A%d=%06o    ", opI, cpu->regA[opI]);
        fprintf(cpuF[cpu->id], "X%d=" FMT60_020o "   ", opJ, cpu->regX[opJ]);
        fprintf(cpuF[cpu->id], "B%d=%06o    ", opK, cpu->regB[opK]);
        fprintf(cpuF[cpu->id], "X%d=" FMT60_020o "   ", opI, cpu->regX[opI]);
        break;

    case RBA:
        fprintf(cpuF[cpu->id], "B%d=%06o    ", opI, cpu->regB[opI]);
        fprintf(cpuF[cpu->id], "A%d=%06o    ", opJ, cpu->regA[opJ]);
        break;

    case RBAB:
        fprintf(cpuF[cpu->id], "B%d=%06o    ", opI, cpu->regB[opI]);
        fprintf(cpuF[cpu->id], "A%d=%06o    ", opJ, cpu->regA[opJ]);
        fprintf(cpuF[cpu->id], "B%d=%06o    ", opK, cpu->regB[opK]);
        break;

    case RBB:
        fprintf(cpuF[cpu->id], "B%d=%06o    ", opI, cpu->regB[opI]);
        fprintf(cpuF[cpu->id], "B%d=%06o    ", opJ, cpu->regB[opJ]);
        break;

    case RBBB:
        fprintf(cpuF[cpu->id], "B%d=%06o    ", opI, cpu->regB[opI]);
        fprintf(cpuF[cpu->id], "B%d=%06o    ", opJ, cpu->regB[opJ]);
        fprintf(cpuF[cpu->id], "B%d=%06o    ", opK, cpu->regB[opK]);
        break;

    case RBX:
        fprintf(cpuF[cpu->id], "B%d=%06o    ", opI, cpu->regB[opI]);
        fprintf(cpuF[cpu->id], "X%d=" FMT60_020o "   ", opJ, cpu->regX[opJ]);
        break;

    case RBXB:
        fprintf(cpuF[cpu->id], "B%d=%06o    ", opI, cpu->regB[opI]);
        fprintf(cpuF[cpu->id], "X%d=" FMT60_020o "   ", opJ, cpu->regX[opJ]);
        fprintf(cpuF[cpu->id], "B%d=%06o    ", opK, cpu->regB[opK]);
        break;

    case RX:
        fprintf(cpuF[cpu->id], "X%d=" FMT60_020o "   ", opI, cpu->regX[opI]);
        break;

    case RXA:
        fprintf(cpuF[cpu->id], "X%d=" FMT60_020o "   ", opI, cpu->regX[opI]);
        fprintf(cpuF[cpu->id], "A%d=%06o    ", opJ, cpu->regA[opJ]);
        break;

    case RXAB:
        fprintf(cpuF[cpu->id], "X%d=" FMT60_020o "   ", opI, cpu->regX[opI]);
        fprintf(cpuF[cpu->id], "A%d=%06o    ", opJ, cpu->regA[opJ]);
        fprintf(cpuF[cpu->id], "B%d=%06o    ", opK, cpu->regB[opK]);
        break;

    case RXB:
        fprintf(cpuF[cpu->id], "X%d=" FMT60_020o "   ", opI, cpu->regX[opI]);
        fprintf(cpuF[cpu->id], "B%d=%06o    ", opJ, cpu->regB[opJ]);
        break;

    case RXBB:
        fprintf(cpuF[cpu->id], "X%d=" FMT60_020o "   ", opI, cpu->regX[opI]);
        fprintf(cpuF[cpu->id], "B%d=%06o    ", opJ, cpu->regB[opJ]);
        fprintf(cpuF[cpu->id], "B%d=%06o    ", opK, cpu->regB[opK]);
        break;

    case RXBX:
        fprintf(cpuF[cpu->id], "X%d=" FMT60_020o "   ", opI, cpu->regX[opI]);
        fprintf(cpuF[cpu->id], "B%d=%06o    ", opJ, cpu->regB[opJ]);
        fprintf(cpuF[cpu->id], "X%d=" FMT60_020o "   ", opK, cpu->regX[opK]);
        break;

    case RXX:
        fprintf(cpuF[cpu->id], "X%d=" FMT60_020o "   ", opI, cpu->regX[opI]);
        fprintf(cpuF[cpu->id], "X%d=" FMT60_020o "   ", opJ, cpu->regX[opJ]);
        break;

    case RXXB:
        fprintf(cpuF[cpu->id], "X%d=" FMT60_020o "   ", opI, cpu->regX[opI]);
        fprintf(cpuF[cpu->id], "X%d=" FMT60_020o "   ", opJ, cpu->regX[opJ]);
        fprintf(cpuF[cpu->id], "B%d=%06o    ", opK, cpu->regB[opK]);
        break;

    case RXXX:
        fprintf(cpuF[cpu->id], "X%d=" FMT60_020o "   ", opI, cpu->regX[opI]);
        fprintf(cpuF[cpu->id], "X%d=" FMT60_020o "   ", opJ, cpu->regX[opJ]);
        fprintf(cpuF[cpu->id], "X%d=" FMT60_020o "   ", opK, cpu->regX[opK]);
        break;

    case RZB:
        fprintf(cpuF[cpu->id], "B%d=%06o    ", opJ, cpu->regB[opJ]);
        break;

    case RZX:
        fprintf(cpuF[cpu->id], "X%d=" FMT60_020o "   ", opJ, cpu->regX[opJ]);
        break;

    case RXNX:
        fprintf(cpuF[cpu->id], "X%d=" FMT60_020o "   ", opI, cpu->regX[opI]);
        fprintf(cpuF[cpu->id], "X%d=" FMT60_020o "   ", opK, cpu->regX[opK]);
        break;

    case RNXX:
        fprintf(cpuF[cpu->id], "X%d=" FMT60_020o "   ", opJ, cpu->regX[opJ]);
        fprintf(cpuF[cpu->id], "X%d=" FMT60_020o "   ", opK, cpu->regX[opK]);
        break;

    case RNXN:
        fprintf(cpuF[cpu->id], "X%d=" FMT60_020o "   ", opJ, cpu->regX[opJ]);
        break;

    default:
        fprintf(cpuF[cpu->id], "unsupported register set %d", decode[opFm].regSet);
        break;
        }

    fprintf(cpuF[cpu->id], "\n");
    }

/*--------------------------------------------------------------------------
**  Purpose:        Trace a exchange jump.
**
**  Parameters:     Name        Description.
**                  cpu         Pointer to CPU context
**                  addr        Address of exchange package
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void traceExchange(CpuContext *cpu, u32 addr, char *title)
    {
    CpWord data;
    u8     i;

    /*
    **  Bail out if no trace of exchange jumps is requested.
    */
    if ((traceMask & TraceExchange) == 0)
        {
        return;
        }

    fprintf(cpuF[cpu->id], "\n%06d Exchange jump with package address %06o (%s)\n\n", traceSequenceNo, addr, title);
    fprintf(cpuF[cpu->id], "P       %06o  ", cpu->regP);
    fprintf(cpuF[cpu->id], "A%d %06o  ", 0, cpu->regA[0]);
    fprintf(cpuF[cpu->id], "B%d %06o", 0, cpu->regB[0]);
    fprintf(cpuF[cpu->id], "\n");

    fprintf(cpuF[cpu->id], "RA      %06o  ", cpu->regRaCm);
    fprintf(cpuF[cpu->id], "A%d %06o  ", 1, cpu->regA[1]);
    fprintf(cpuF[cpu->id], "B%d %06o", 1, cpu->regB[1]);
    fprintf(cpuF[cpu->id], "\n");

    fprintf(cpuF[cpu->id], "FL      %06o  ", cpu->regFlCm);
    fprintf(cpuF[cpu->id], "A%d %06o  ", 2, cpu->regA[2]);
    fprintf(cpuF[cpu->id], "B%d %06o", 2, cpu->regB[2]);
    fprintf(cpuF[cpu->id], "\n");

    fprintf(cpuF[cpu->id], "RAE   %08o  ", cpu->regRaEcs);
    fprintf(cpuF[cpu->id], "A%d %06o  ", 3, cpu->regA[3]);
    fprintf(cpuF[cpu->id], "B%d %06o", 3, cpu->regB[3]);
    fprintf(cpuF[cpu->id], "\n");

    fprintf(cpuF[cpu->id], "FLE   %08o  ", cpu->regFlEcs);
    fprintf(cpuF[cpu->id], "A%d %06o  ", 4, cpu->regA[4]);
    fprintf(cpuF[cpu->id], "B%d %06o", 4, cpu->regB[4]);
    fprintf(cpuF[cpu->id], "\n");

    fprintf(cpuF[cpu->id], "EM/FL %08o  ", cpu->exitMode);
    fprintf(cpuF[cpu->id], "A%d %06o  ", 5, cpu->regA[5]);
    fprintf(cpuF[cpu->id], "B%d %06o", 5, cpu->regB[5]);
    fprintf(cpuF[cpu->id], "\n");

    fprintf(cpuF[cpu->id], "MA      %06o  ", cpu->regMa);
    fprintf(cpuF[cpu->id], "A%d %06o  ", 6, cpu->regA[6]);
    fprintf(cpuF[cpu->id], "B%d %06o", 6, cpu->regB[6]);
    fprintf(cpuF[cpu->id], "\n");

    fprintf(cpuF[cpu->id], "STOP         %d  ", cpu->isStopped ? 1 : 0);
    fprintf(cpuF[cpu->id], "A%d %06o  ", 7, cpu->regA[7]);
    fprintf(cpuF[cpu->id], "B%d %06o  ", 7, cpu->regB[7]);
    fprintf(cpuF[cpu->id], "\n");
    fprintf(cpuF[cpu->id], "ECOND       %02o  ", cpu->exitCondition);
    fprintf(cpuF[cpu->id], "\n");
    fprintf(cpuF[cpu->id], "MonitorFlag %s", cpu->isMonitorMode ? "TRUE" : "FALSE");
    fprintf(cpuF[cpu->id], "\n");
    fprintf(cpuF[cpu->id], "\n");

    for (i = 0; i < 8; i++)
        {
        fprintf(cpuF[cpu->id], "X%d ", i);
        data = cpu->regX[i];
        fprintf(cpuF[cpu->id], "%04o %04o %04o %04o %04o   ",
                (PpWord)((data >> 48) & Mask12),
                (PpWord)((data >> 36) & Mask12),
                (PpWord)((data >> 24) & Mask12),
                (PpWord)((data >> 12) & Mask12),
                (PpWord)((data) & Mask12));
        fprintf(cpuF[cpu->id], "\n");
        }

    fprintf(cpuF[cpu->id], "\n\n");
    }

/*--------------------------------------------------------------------------
**  Purpose:        Output sequence number.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void traceSequence(void)
    {
    /*
    **  Increment sequence number here.
    */
    traceSequenceNo += 1;

    /*
    **  Bail out if no trace of this PPU is requested.
    */
    if ((traceMask & (1 << activePpu->id)) == 0)
        {
        return;
        }

    /*
    **  Print sequence no and PPU number.
    */
    fprintf(ppuF[activePpu->id], "%06d [%2o]    ", traceSequenceNo, activePpu->id);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Output registers.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void traceRegisters(void)
    {
    /*
    **  Bail out if no trace of this PPU is requested.
    */
    if ((traceMask & (1 << activePpu->id)) == 0)
        {
        return;
        }

    /*
    **  Print registers.
    */
    fprintf(ppuF[activePpu->id], "P:%04o  ", activePpu->regP);
    fprintf(ppuF[activePpu->id], "A:%06o", activePpu->regA);
    fprintf(ppuF[activePpu->id], "    ");
    }

/*--------------------------------------------------------------------------
**  Purpose:        Output opcode.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void traceOpcode(void)
    {
    u8           addrMode;
    DecPpControl *decodeTable;
    char         *fmt;
    bool         is180;
    PpWord       opCode;
    u8           opD;
    u8           opF;

    /*
    **  Bail out if no trace of this PPU is requested.
    */
    if ((traceMask & (1 << activePpu->id)) == 0)
        {
        return;
        }

    /*
    **  Print opcode.
    */
    opCode = activePpu->mem[activePpu->regP];
    opF    = opCode >> 6;
    opD    = opCode & 077;
    is180  = (features & IsCyber180) != 0;
    if (is180)
        {
        decodeTable = (opCode & 0100000) != 0 ? ppDecode180 : ppDecode170;
        fmt = "O:%06o   %-4.4s ";
        }
    else
        {
        decodeTable = ppDecode170;
        fmt = "O:%04o   %3.3s ";
        }
    opF     &= 077;
    addrMode = decodeTable[opF].mode;

    if (decodeTable[opF].hasOp2 && ((opD & 040) != 0) && ((features & HasChannelFlag) != 0))
        {
        fprintf(ppuF[activePpu->id], fmt, opCode, decodeTable[opF].mnemonic2);
        }
    else
        {
        fprintf(ppuF[activePpu->id], fmt, opCode, decodeTable[opF].mnemonic);
        }

    switch (addrMode)
        {
    case AN:
        fprintf(ppuF[activePpu->id], "        ");
        break;

    case Amd:
        fprintf(ppuF[activePpu->id], "%04o,%02o ", activePpu->mem[activePpu->regP + 1], opD);
        break;

    case Ar:
        if (opD < 040)
            {
            fprintf(ppuF[activePpu->id], "+%02o     ", opD);
            }
        else
            {
            fprintf(ppuF[activePpu->id], "-%02o     ", 077 - opD);
            }
        break;

    case Ad:
        fprintf(ppuF[activePpu->id], "%02o      ", opD);
        break;

    case Adm:
        fprintf(ppuF[activePpu->id], "%02o%04o  ", opD, activePpu->mem[activePpu->regP + 1]);
        break;
        }

    fprintf(ppuF[activePpu->id], "    ");
    }

/*--------------------------------------------------------------------------
**  Purpose:        Output opcode.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
u8 traceDisassembleOpcode(char *str, PpWord *pm)
    {
    u8           addrMode;
    DecPpControl *decodeTable;
    char         *fmt;
    bool         is180;
    PpWord       opCode;
    u8           opD;
    u8           opF;
    u8           result = 1;

    /*
    **  Print opcode.
    */
    opCode = *pm++;
    opF    = opCode >> 6;
    opD    = opCode & 077;
    is180  = (features & IsCyber180) != 0;
    if (is180)
        {    
        decodeTable = (opCode & 0100000) != 0 ? ppDecode180 : ppDecode170;
        fmt = "%-4.4s  ";
        }
    else
        {
        decodeTable = ppDecode170;
        fmt = "%3.3s  ";
        }
    opF     &= 077;
    addrMode = decodeTable[opF].mode;

    if (decodeTable[opF].hasOp2 && ((opD & 040) != 0) && ((features & HasChannelFlag) != 0))
        {
        str += sprintf(str, fmt, decodeTable[opF].mnemonic2);
        }
    else
        {
        str += sprintf(str, fmt, decodeTable[opF].mnemonic);
        }

    switch (addrMode)
        {
    case AN:
        sprintf(str, "        ");
        break;

    case Amd:
        sprintf(str, "%04o,%02o ", *pm, opD);
        result = 2;
        break;

    case Ar:
        if (opD < 040)
            {
            sprintf(str, "+%02o     ", opD);
            }
        else
            {
            sprintf(str, "-%02o     ", 077 - opD);
            }
        break;

    case Ad:
        sprintf(str, "%02o      ", opD);
        break;

    case Adm:
        sprintf(str, "%02o%04o  ", opD, *pm);
        result = 2;
        break;
        }

    return (result);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Output channel unclaimed function info.
**
**  Parameters:     Name        Description.
**                  funcCode    Function code.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void traceChannelFunction(PpWord funcCode)
    {
    fprintf(devF, "%06d [%02o]    ", traceSequenceNo, activePpu->id);
    fprintf(devF, "Unclaimed function code %04o on CH%02o\n", funcCode, activeChannel->id);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Output string for PPU.
**
**  Parameters:     Name        Description.
**                  str         String to output.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void tracePrint(char *str)
    {
    fputs(str, ppuF[activePpu->id]);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Output string for CPU.
**
**  Parameters:     Name        Description.
**                  cpu         Pointer to CPU context
**                  str         String to output.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void traceCpuPrint(CpuContext *cpu, char *str)
    {
    fputs(str, cpuF[cpu->id]);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Output status of channel.
**
**  Parameters:     Name        Description.
**                  ch          channel number.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void traceChannel(u8 ch)
    {
    /*
    **  Bail out if no trace of this PPU is requested.
    */
    if ((traceMask & (1 << activePpu->id)) == 0)
        {
        return;
        }

    fprintf(ppuF[activePpu->id], "  CH%02o:%c%c%c", ch,
            channel[ch].active           ? 'A' : 'D',
            channel[ch].full             ? 'F' : 'E',
            channel[ch].ioDevice == NULL ? 'I' : 'S');
    if (channel[ch].flag)
        {
        fputc('*', ppuF[activePpu->id]);
        }
    if (channel[ch].full)
        {
        if ((features & IsCyber180) != 0)
            {
            fprintf(ppuF[activePpu->id], "  %06o", channel[ch].data);
            }
        else
            {
            fprintf(ppuF[activePpu->id], "  %04o", channel[ch].data);
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Output end-of-line.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void traceEnd(void)
    {
    /*
    **  Bail out if no trace of this PPU is requested.
    */
    if ((traceMask & (1 << activePpu->id)) == 0)
        {
        return;
        }

    /*
    **  Print newline.
    */
    fprintf(ppuF[activePpu->id], "\n");
    }

/*---------------------------  End Of File  ------------------------------*/
