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

#if defined(__APPLE__)
#include <execinfo.h>
#endif


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
**  CYBER 170 CPU command adressing modes.
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
**  CYBER 170 CPU register set markers.
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
**  CYBER 180 CPU command adressing modes.
*/
#define VCjk     1
#define VCjkiD   2
#define VCjkQ    3

/*
**  CYBER 180 CPU instruction format markers
*/
#define VF       1
#define VFK      2
#define VFJK     3
#define VFKJ     4
#define VFKJD    5
#define VFKJID   6
#define VFKID    7
#define VFKJQ    8
#define VFJKQ    9
#define VFKQ     10
#define VFKJDJ   11
#define VFQJK    12
#define VFJKID   13
#define VFIDKJ   14
#define VFJK8    15
#define VFJKQ24  16

/*
**  CYBER 180 CPU register set markers.
*/
#define VR           1
#define VRXJ         2
#define VRXK         3
#define VRXKXJ       4
#define VRAKAJ       5
#define VRAKXJ       6
#define VRXKAJ       7
#define VRXKAJX0     8
#define VRXKXJX1     9
#define VRXKX1       10
#define XRX1XJXK     11
#define VRXXKXXJ     12
#define VRX0         13
#define VRX1         14
#define VRAJX0AKX1   15
#define VRXJXK       16
#define VRX1AJAK     17
#define VRAKAJXI     18
#define VRXKAJXI     19
#define VRXKXJXI     20
#define VRAJAK       21
#define VRAJX0AKX1XI 22
#define VRAJX0AKX1AI 23
#define VRX0AKX1AI   24
#define VRXIAKX1     25
#define VRX1XJXK     26
#define VRAJXK       27
#define VRXKXI       28

/*
**  -----------------------
**  Private Macro Functions
**  -----------------------
*/
#define tracePrintPva(f, pva) (             \
    fprintf((f), "%x %03x %08x",           \
            (u8)(((pva) >> 44) & Mask4),   \
            (u16)(((pva) >> 32) & Mask12), \
            (u32)((pva) & Mask32)))

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

typedef struct decCp180Control
    {
    u8   mode;
    char *mnemonic;
    u8   instFmt;
    u8   regSet;
    } DecCp180Control;

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static char *traceMonitorConditionToStr(MonitorCondition cond);

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
    Ad,  "LRD", FALSE, NULL, // 24
    Ad,  "SRD", FALSE, NULL, // 25
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
    CjK, "RT    X%o,%6.6o", RZX                 // 7
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
    CjK, "ID    X%o,%6.6o", RZX                 // 7
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
    Cijk,  "SX%o   B%o-B%o", RXBB               // 77
    };

static DecCp180Control cp180Decode[0x100] =
    {
    VCjk,   "HALT",     VF, VR,                                // 00
    VCjk,   "SYNC",     VF, VR,                                // 01
    VCjk,   "EXCHANGE", VF, VR,                                // 02
    VCjk,   "INTRUPT  X%X", VFK, VRXK,                         // 03
    VCjk,   "RETURN",   VF, VR,                                // 04
    VCjk,   "PURGE    X%X,%d", VFJK, VRXJ,                       // 05
    VCjk,   "POP",      VF, VR,                                // 06
    VCjk,   "PSFSA",    VF, VR,                                // 07
    VCjk,   "CPYTX    X%X,X%X", VFKJ, VRXKXJ,                  // 08
    VCjk,   "CPYAA    A%X,A%X", VFKJ, VRAKAJ,                  // 09
    VCjk,   "CPYXA    A%X,X%X", VFKJ, VRAKXJ,                  // 0A
    VCjk,   "CPYAX    X%X,A%X", VFKJ, VRXKAJ,                  // 0B
    VCjk,   "CPYRR    X%X,X%X", VFKJ, VRXKXJ,                  // 0C
    VCjk,   "CPYXX    X%X,X%X", VFKJ, VRXKXJ,                  // 0D
    VCjk,   "CPYSX    X%X,X%X", VFKJ, VRXKXJ,                  // 0E
    VCjk,   "CPYXS    X%X,X%X", VFKJ, VRXKXJ,                  // 0F

    VCjk,   "INCX     X%X,%d", VFKJ, VRXK,                     // 10
    VCjk,   "DECX     X%X,%d", VFKJ, VRXK,                     // 11
    VCjk,   "Illegal",  VF, VR,                                // 12
    VCjk,   "Illegal",  VF, VR,                                // 13
    VCjk,   "LBSET    X%X,A%X,X0", VFKJ, VRXKAJX0,             // 14
    VCjk,   "Illegal",  VF, VR,                                // 15
    VCjk,   "TPAGE    X%X,A%X", VFKJ, VRXKAJ,                  // 16
    VCjk,   "LPAGE    X%X,X%X,X1", VFKJ, VRXKXJX1,             // 17
    VCjk,   "IORX     X%X,X%X", VFKJ, VRXKXJ,                  // 18
    VCjk,   "XORX     X%X,X%X", VFKJ, VRXKXJ,                  // 19
    VCjk,   "ANDX     X%X,X%X", VFKJ, VRXKXJ,                  // 1A
    VCjk,   "NOTX     X%X,X%X", VFKJ, VRXKXJ,                  // 1B
    VCjk,   "INHX     X%X,X%X", VFKJ, VRXKXJ,                  // 1C
    VCjk,   "Illegal",  VF, VR,                                // 1D
    VCjk,   "MARK     X%X,X1,%d", VFKJ, VRXKX1,                // 1E
    VCjk,   "ENTZ/O/S X%X", VFK, VRXK,                         // 1F

    VCjk,   "ADDR     X%X,X%X", VFKJ, VRXKXJ,                  // 20
    VCjk,   "SUBR     X%X,X%X", VFKJ, VRXKXJ,                  // 21
    VCjk,   "MULR     X%X,X%X", VFKJ, VRXKXJ,                  // 22
    VCjk,   "DIVF     X%X,X%X", VFKJ, VRXKXJ,                  // 23
    VCjk,   "ADDX     X%X,X%X", VFKJ, VRXKXJ,                  // 24
    VCjk,   "SUBX     X%X,X%X", VFKJ, VRXKXJ,                  // 25
    VCjk,   "MULX     X%X,X%X", VFKJ, VRXKXJ,                  // 26
    VCjk,   "DIVX     X%X,X%X", VFKJ, VRXKXJ,                  // 27
    VCjk,   "INCR     X%X,%d", VFKJ, VRXK,                     // 28
    VCjk,   "DECR     X%X,%d", VFKJ, VRXK,                     // 29
    VCjk,   "ADDAX    A%X,X%X", VFKJ,VRAKXJ,                   // 2A
    VCjk,   "Illegal",  VF, VR,                                // 2B
    VCjk,   "CMPR     X1,X%X,X%X", VFJK, VRX1XJXK,             // 2C
    VCjk,   "CMPX     X1,X%X,X%X", VFJK, VRX1XJXK,             // 2D
    VCjk,   "BRREL    X%X", VFK, VRXK,                         // 2E
    VCjk,   "BRDIR    A%X,X%X", VFJK, VRAJXK,                  // 2F

    VCjk,   "ADDF     X%X,X%X", VFKJ, VRXKXJ,                  // 30
    VCjk,   "SUBF     X%X,X%X", VFKJ, VRXKXJ,                  // 31
    VCjk,   "MULF     X%X,X%X", VFKJ, VRXKXJ,                  // 32
    VCjk,   "DIVF     X%X,X%X", VFKJ, VRXKXJ,                  // 33
    VCjk,   "ADDD     XX%X,XX%X", VFKJ, VRXXKXXJ,              // 34
    VCjk,   "SUBD     XX%X,XX%X", VFKJ, VRXXKXXJ,              // 35
    VCjk,   "MULD     XX%X,XX%X", VFKJ, VRXXKXXJ,              // 36
    VCjk,   "DIVD     XX%X,XX%X", VFKJ, VRXXKXXJ,              // 37
    VCjk,   "Illegal",  VF, VR,                                // 38
    VCjk,   "ENTX     X1,%d", VFJK8, VRX1,                     // 39
    VCjk,   "CNIF     X%X,X%X", VFKJ, VRXKXJ,                  // 3A
    VCjk,   "CNFI     X%X,X%X", VFKJ, VRXKXJ,                  // 3B
    VCjk,   "CMPF     X1,X%X,X%X", VFJK, VRX1XJXK,             // 3C
    VCjk,   "ENTP     X%X,%d", VFKJ, VRXK,                     // 3D
    VCjk,   "ENTN     X%X,%d", VFKJ, VRXK,                     // 3E
    VCjk,   "ENTL     X0,%d", VFJK8, VRX0,                     // 3F

    // Vector instructions not decoded yet
    VCjkiD, "Illegal",  VF, VR,                                // 40
    VCjkiD, "Illegal",  VF, VR,                                // 41
    VCjkiD, "Illegal",  VF, VR,                                // 42
    VCjkiD, "Illegal",  VF, VR,                                // 43
    VCjkiD, "Illegal",  VF, VR,                                // 44
    VCjkiD, "Illegal",  VF, VR,                                // 45
    VCjkiD, "Illegal",  VF, VR,                                // 46
    VCjkiD, "Illegal",  VF, VR,                                // 47
    VCjkiD, "Illegal",  VF, VR,                                // 48
    VCjkiD, "Illegal",  VF, VR,                                // 49
    VCjkiD, "Illegal",  VF, VR,                                // 4A
    VCjkiD, "Illegal",  VF, VR,                                // 4B
    VCjkiD, "Illegal",  VF, VR,                                // 4C
    VCjkiD, "Illegal",  VF, VR,                                // 4D
    VCjkiD, "Illegal",  VF, VR,                                // 4E
    VCjkiD, "Illegal",  VF, VR,                                // 4F

    VCjkiD, "Illegal",  VF, VR,                                // 50
    VCjkiD, "Illegal",  VF, VR,                                // 51
    VCjkiD, "Illegal",  VF, VR,                                // 52
    VCjkiD, "Illegal",  VF, VR,                                // 53
    VCjkiD, "Illegal",  VF, VR,                                // 54
    VCjkiD, "Illegal",  VF, VR,                                // 55
    VCjkiD, "Illegal",  VF, VR,                                // 56
    VCjkiD, "Illegal",  VF, VR,                                // 57
    VCjkiD, "Illegal",  VF, VR,                                // 58
    VCjkiD, "Illegal",  VF, VR,                                // 59
    VCjkiD, "Illegal",  VF, VR,                                // 5A
    VCjkiD, "Illegal",  VF, VR,                                // 5B
    VCjkiD, "Illegal",  VF, VR,                                // 5C
    VCjkiD, "Illegal",  VF, VR,                                // 5D
    VCjkiD, "Illegal",  VF, VR,                                // 5E
    VCjkiD, "Illegal",  VF, VR,                                // 5F

    VCjkiD, "Illegal",  VF, VR,                                // 60
    VCjkiD, "Illegal",  VF, VR,                                // 61
    VCjkiD, "Illegal",  VF, VR,                                // 62
    VCjkiD, "Illegal",  VF, VR,                                // 63
    VCjkiD, "Illegal",  VF, VR,                                // 64
    VCjkiD, "Illegal",  VF, VR,                                // 65
    VCjkiD, "Illegal",  VF, VR,                                // 66
    VCjkiD, "Illegal",  VF, VR,                                // 67
    VCjkiD, "Illegal",  VF, VR,                                // 68
    VCjkiD, "Illegal",  VF, VR,                                // 69
    VCjkiD, "Illegal",  VF, VR,                                // 6A
    VCjkiD, "Illegal",  VF, VR,                                // 6B
    VCjkiD, "Illegal",  VF, VR,                                // 6C
    VCjkiD, "Illegal",  VF, VR,                                // 6D
    VCjkiD, "Illegal",  VF, VR,                                // 6E
    VCjkiD, "Illegal",  VF, VR,                                // 6F

    VCjk,   "ADDN,A%X,X0  A%X,X1", VFJK, VRAJX0AKX1,           // 70
    VCjk,   "SUBN,A%X,X0  A%X,X1", VFJK, VRAJX0AKX1,           // 71
    VCjk,   "MULN,A%X,X0  A%X,X1", VFJK, VRAJX0AKX1,           // 72
    VCjk,   "DIVN,A%X,X0  A%X,X1", VFJK, VRAJX0AKX1,           // 73
    VCjk,   "CMPN,A%X,X0  A%X,X1", VFJK, VRAJX0AKX1,           // 74
    VCjk,   "MOVN,A%X,X0  A%X,X1", VFJK, VRAJX0AKX1,           // 75
    VCjk,   "MOVB,A%X,X0  A%X,X1", VFJK, VRAJX0AKX1,           // 76
    VCjk,   "CMPB,A%X,X0  A%X,X1", VFJK, VRAJX0AKX1,           // 77
    VCjk,   "Illegal",  VF, VR,                                // 78
    VCjk,   "Illegal",  VF, VR,                                // 79
    VCjk,   "Illegal",  VF, VR,                                // 7A
    VCjk,   "Illegal",  VF, VR,                                // 7B
    VCjk,   "Illegal",  VF, VR,                                // 7C
    VCjk,   "Illegal",  VF, VR,                                // 7D
    VCjk,   "Illegal",  VF, VR,                                // 7E
    VCjk,   "Illegal",  VF, VR,                                // 7F

    VCjkQ,  "LMULT    X%X,A%X,%d", VFKJQ, VRXKAJ,              // 80
    VCjkQ,  "SMULT    X%X,A%X,%d", VFKJQ, VRXKAJ,              // 81
    VCjkQ,  "LX       X%X,A%X,%d", VFKJQ, VRXKAJ,              // 82
    VCjkQ,  "SX       X%X,A%X,%d", VFKJQ, VRXKAJ,              // 83
    VCjkQ,  "LA       A%X,A%X,%d", VFKJQ, VRAKAJ,              // 84
    VCjkQ,  "SA       A%X,A%X,%d", VFKJQ, VRAKAJ,              // 85
    VCjkQ,  "LBYTP,%d  X%X,%d", VFJKQ, VRXK,                   // 86
    VCjkQ,  "ENTC     X1,%d", VFJKQ24, VRX1,                   // 87
    VCjkQ,  "LBIT     X%X,A%X,%d,X0", VFKJQ, VRXKAJX0,         // 88
    VCjkQ,  "SBIT     X%X,A%X,%d,X0", VFKJQ, VRXKAJX0,         // 89
    VCjkQ,  "ADDRQ    X%X,X%X,%d", VFKJQ, VRXKXJ,              // 8A
    VCjkQ,  "ADDXQ    X%X,X%X,%d", VFKJQ, VRXKXJ,              // 8B
    VCjkQ,  "MULRQ    X%X,X%X,%d", VFKJQ, VRXKXJ,              // 8C
    VCjkQ,  "ENTE     X%X,%d", VFKQ, VRXK,                     // 8D
    VCjkQ,  "ADDAQ    A%X,A%X,%d", VFKJQ, VRAKAJ,              // 8E
    VCjkQ,  "ADDPXQ   A%X,X%X,%d", VFKJQ, VRAKXJ,              // 8F

    VCjkQ,  "BRREQ    X%X,X%X,0x%X", VFJKQ, VRXJXK,            // 90
    VCjkQ,  "BRRNE    X%X,X%X,0x%X", VFJKQ, VRXJXK,            // 91
    VCjkQ,  "BRRGT    X%X,X%X,0x%X", VFJKQ, VRXJXK,            // 92
    VCjkQ,  "BRRGE    X%X,X%X,0x%X", VFJKQ, VRXJXK,            // 93
    VCjkQ,  "BRXEQ    X%X,X%X,0x%X", VFJKQ, VRXJXK,            // 94
    VCjkQ,  "BRXNE    X%X,X%X,0x%X", VFJKQ, VRXJXK,            // 95
    VCjkQ,  "BRXGT    X%X,X%X,0x%X", VFJKQ, VRXJXK,            // 96
    VCjkQ,  "BRXGE    X%X,X%X,0x%X", VFJKQ, VRXJXK,            // 97
    VCjkQ,  "BRFEQ    X%X,X%X,0x%X", VFJKQ, VRXJXK,            // 98
    VCjkQ,  "BRFNE    X%X,X%X,0x%X", VFJKQ, VRXJXK,            // 99
    VCjkQ,  "BRFGT    X%X,X%X,0x%X", VFJKQ, VRXJXK,            // 9A
    VCjkQ,  "BRFGE    X%X,X%X,0x%X", VFJKQ, VRXJXK,            // 9B
    VCjkQ,  "BRINC    X%X,X%X,0x%X", VFJKQ, VRXJXK,            // 9C
    VCjkQ,  "BRSEG    X1,A%X,A%X,0x%X", VFJKQ, VRX1AJAK,       // 9D
    VCjkQ,  "BR---    X%X,0x%X", VFKQ, VRXK,                   // 9E
    VCjkQ,  "BRCR     %d,0x%X,0x%X", VFJKQ, VR,                // 9F

    VCjkiD, "LAI      A%X,A%X,X%X,%d", VFKJID, VRAKAJXI,       // A0
    VCjkiD, "SAI      A%X,A%X,X%X,%d", VFKJID, VRAKAJXI,       // A1
    VCjkiD, "LXI      X%X,A%X,X%X,%d", VFKJID, VRXKAJXI,       // A2
    VCjkiD, "SXI      X%X,A%X,X%X,%d", VFKJID, VRXKAJXI,       // A3
    VCjkiD, "LBYT,X0  X%X,A%X,X%X,%d", VFKJID, VRXKAJXI,       // A4
    VCjkiD, "SBYT,X0  X%X,A%X,X%X,%d", VFKJID, VRXKAJXI,       // A5
    VCjkiD, "Illegal",  VF, VR,                                // A6
    VCjkiD, "ADDAD    A%X,A%X,%d,%d", VFKJDJ, VRAKAJ,          // A7
    VCjkiD, "SHFC     X%X,X%X,X%X,%d", VFKJID, VRXKXJXI,       // A8
    VCjkiD, "SHFX     X%X,X%X,X%X,%d", VFKJID, VRXKXJXI,       // A9
    VCjkiD, "SHFR     X%X,X%X,X%X,%d", VFKJID, VRXKXJXI,       // AA
    VCjkiD, "Illegal",  VF, VR,                                // AB
    VCjkiD, "ISOM     X%X,X%X,%d", VFKID, VRXKXI,              // AC
    VCjkiD, "ISOB     X%X,X%X,X%X,%d", VFKJID, VRXKXJXI,       // AD
    VCjkiD, "INSB     X%X,X%X,X%X,%d", VFKJID, VRXKXJXI,       // AE
    VCjkiD, "Illegal",  VF, VR,                                // AF

    VCjkQ,  "CALLREL  0x%X,A%X,A%X", VFQJK, VRAJAK,            // B0
    VCjkQ,  "KEYPOINT 0x%X,X%X,%d", VFJKQ, VRXK,               // B1
    VCjkQ,  "MULXQ    X%X,X%X,%d", VFKJQ, VRXKXJ,              // B2
    VCjkQ,  "ENTA     X0,%d", VFJKQ24, VRX0,                   // B3
    VCjkQ,  "CMPXA    X%X,A%X,X0,%d", VFKJQ, VRXKAJX0,         // B4
    VCjkQ,  "CALLSEG  0x%X,A%X,A%X", VFQJK, VRAJAK,            // B5
    VCjkQ,  "Illegal",  VF, VR,                                // B6
    VCjkQ,  "Illegal",  VF, VR,                                // B7
    VCjkQ,  "Illegal",  VF, VR,                                // B8
    VCjkQ,  "Illegal",  VF, VR,                                // B9
    VCjkQ,  "Illegal",  VF, VR,                                // BA
    VCjkQ,  "Illegal",  VF, VR,                                // BB
    VCjkQ,  "Illegal",  VF, VR,                                // BC
    VCjkQ,  "Illegal",  VF, VR,                                // BD
    VCjkQ,  "Illegal",  VF, VR,                                // BE
    VCjkQ,  "Illegal",  VF, VR,                                // BF

    VCjkiD, "EXECUTE,0 %X,%X,%X,%d", VFJKID, VR,               // C0
    VCjkiD, "EXECUTE,1 %X,%X,%X,%d", VFJKID, VR,               // C1
    VCjkiD, "EXECUTE,2 %X,%X,%X,%d", VFJKID, VR,               // C2
    VCjkiD, "EXECUTE,3 %X,%X,%X,%d", VFJKID, VR,               // C3
    VCjkiD, "EXECUTE,4 %X,%X,%X,%d", VFJKID, VR,               // C4
    VCjkiD, "EXECUTE,5 %X,%X,%X,%d", VFJKID, VR,               // C5
    VCjkiD, "EXECUTE,6 %X,%X,%X,%d", VFJKID, VR,               // C6
    VCjkiD, "EXECUTE,7 %X,%X,%X,%d", VFJKID, VR,               // C7
    VCjkiD, "Illegal",  VF, VR,                                // C8
    VCjkiD, "Illegal",  VF, VR,                                // C9
    VCjkiD, "Illegal",  VF, VR,                                // CA
    VCjkiD, "Illegal",  VF, VR,                                // CB
    VCjkiD, "Illegal",  VF, VR,                                // CC
    VCjkiD, "Illegal",  VF, VR,                                // CD
    VCjkiD, "Illegal",  VF, VR,                                // CE
    VCjkiD, "Illegal",  VF, VR,                                // CF

    VCjkiD, "LBYTS,1  X%X,A%X,X%X,%d", VFKJID, VRXKAJXI,       // D0
    VCjkiD, "LBYTS,2  X%X,A%X,X%X,%d", VFKJID, VRXKAJXI,       // D1
    VCjkiD, "LBYTS,3  X%X,A%X,X%X,%d", VFKJID, VRXKAJXI,       // D2
    VCjkiD, "LBYTS,4  X%X,A%X,X%X,%d", VFKJID, VRXKAJXI,       // D3
    VCjkiD, "LBYTS,5  X%X,A%X,X%X,%d", VFKJID, VRXKAJXI,       // D4
    VCjkiD, "LBYTS,6  X%X,A%X,X%X,%d", VFKJID, VRXKAJXI,       // D5
    VCjkiD, "LBYTS,7  X%X,A%X,X%X,%d", VFKJID, VRXKAJXI,       // D6
    VCjkiD, "LBYTS,8  X%X,A%X,X%X,%d", VFKJID, VRXKAJXI,       // D7
    VCjkiD, "SBYTS,1  X%X,A%X,X%X,%d", VFKJID, VRXKAJXI,       // D8
    VCjkiD, "SBYTS,2  X%X,A%X,X%X,%d", VFKJID, VRXKAJXI,       // D9
    VCjkiD, "SBYTS,3  X%X,A%X,X%X,%d", VFKJID, VRXKAJXI,       // DA
    VCjkiD, "SBYTS,4  X%X,A%X,X%X,%d", VFKJID, VRXKAJXI,       // DB
    VCjkiD, "SBYTS,5  X%X,A%X,X%X,%d", VFKJID, VRXKAJXI,       // DC
    VCjkiD, "SBYTS,6  X%X,A%X,X%X,%d", VFKJID, VRXKAJXI,       // DD
    VCjkiD, "SBYTS,7  X%X,A%X,X%X,%d", VFKJID, VRXKAJXI,       // DE
    VCjkiD, "SBYTS,8  X%X,A%X,X%X,%d", VFKJID, VRXKAJXI,       // DF

    VCjkiD, "Illegal",  VF, VR,                                // E0
    VCjkiD, "Illegal",  VF, VR,                                // E1
    VCjkiD, "Illegal",  VF, VR,                                // E2
    VCjkiD, "Illegal",  VF, VR,                                // E3
    VCjkiD, "SCLN,A%X,X0 A%X,X1,X%X,%d", VFJKID, VRAJX0AKX1XI, // E4
    VCjkiD, "SCLR,A%X,X0 A%X,X1,X%X,%d", VFJKID, VRAJX0AKX1XI, // E5
    VCjkiD, "Illegal",  VF, VR,                                // E6
    VCjkiD, "Illegal",  VF, VR,                                // E7
    VCjkiD, "Illegal",  VF, VR,                                // E8
    VCjkiD, "CMPC,A%X,X0 A%X,X1,A%X,%d", VFJKID, VRAJX0AKX1AI, // E9
    VCjkiD, "Illegal",  VF, VR,                                // EA
    VCjkiD, "TRANB,A%X,X0 A%X,X1,A%X,%d",VFJKID, VRAJX0AKX1AI, // EB
    VCjkiD, "Illegal",  VF, VR,                                // EC
    VCjkiD, "EDIT,A%X,X0 A%X,X1,A%X,%d", VFJKID, VRAJX0AKX1AI, // ED
    VCjkiD, "Illegal",  VF, VR,                                // EE
    VCjkiD, "Illegal",  VF, VR,                                // EF

    VCjkiD, "Illegal",  VF, VR,                                // F0
    VCjkiD, "Illegal",  VF, VR,                                // F1
    VCjkiD, "Illegal",  VF, VR,                                // F2
    VCjkiD, "SCANB,X0 A%X,X1,A%X,%d", VFKID, VRX0AKX1AI,       // F3
    VCjkiD, "Illegal",  VF, VR,                                // F4
    VCjkiD, "Illegal",  VF, VR,                                // F5
    VCjkiD, "Illegal",  VF, VR,                                // F6
    VCjkiD, "Illegal",  VF, VR,                                // F7
    VCjkiD, "Illegal",  VF, VR,                                // F8
    VCjkiD, "MOVI,X%X,%d A%X,X1,%d", VFIDKJ, VRXIAKX1,         // F9
    VCjkiD, "CMPI,X%X,%d A%X,X1,%d", VFIDKJ, VRXIAKX1,         // FA
    VCjkiD, "ADDI,X%X,%d A%X,X1,%d", VFIDKJ, VRXIAKX1,         // FB
    VCjkiD, "Illegal",  VF, VR,                                // FC
    VCjkiD, "Illegal",  VF, VR,                                // FD
    VCjkiD, "Illegal",  VF, VR,                                // FE
    VCjkiD, "Illegal",  VF, VR                                 // FF
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
**  Purpose:        Output CYBER 170 CPU opcode.
**
**  Parameters:     Name        Description.
**                  cpu         Pointer to CPU context
**                  p           P register value
**                  opFm        Opcode
**                  opI         i
**                  opJ         j
**                  opK         k
**                  opAddress   jk
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void traceCpu(Cpu170Context *cpu, u32 p, u8 opFm, u8 opI, u8 opJ, u8 opK, u32 opAddress)
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
**  Purpose:        Output CYBER 180 CPU opcode.
**
**  Parameters:     Name        Description.
**                  cpu         Pointer to CPU context
**                  p           P register value
**                  opCode      Opcode
**                  opI         i
**                  opJ         j
**                  opK         k
**                  opD         D
**                  opQ         Q
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void traceCpu180(Cpu180Context *cpu, u64 p, u8 opCode, u8 opI, u8 opJ, u8 opK, u16 opD, u16 opQ)
    {
    DecCp180Control *decode = cp180Decode;
    DecCp180Control *entry;
    static char     str[80];
    u64             value;

    /*
    **  Bail out if no trace of the CPU is requested.
    */
    if ((traceMask & TraceCpu) == 0)
        {
        return;
        }

    /*
    **  Print sequence no.
    */
    traceSequenceNo += 1;
    fprintf(cpuF[cpu->id], "%06d ", traceSequenceNo);

    /*
    **  Print program counter and opcode.
    */
    fprintf(cpuF[cpu->id], "%x %03x %08x  ", (u8)((p >> 44) & Mask4), (u16)((p >> 32) & Mask12), (u32)(p & Mask32));
    fprintf(cpuF[cpu->id], "op:%02x ", opCode);

    /*
    **  Print opcode mnemonic and operands.
    */
    entry = &decode[opCode];
    switch (entry->mode)
        {
    default:
    case VCjk:
        fprintf(cpuF[cpu->id], "j:%X k:%X            ", opJ, opK);
        break;
    case VCjkiD:
        fprintf(cpuF[cpu->id], "j:%X k:%X i:%X D:%03x  ", opJ, opK, opI, opD);
        break;
    case VCjkQ:
        fprintf(cpuF[cpu->id], "j:%X k:%X Q:%04x     ", opJ, opK, opQ);
        break;
        }

    switch (decode[opCode].instFmt)
        {
    case VF:
        strcpy(str, entry->mnemonic);
        break;
    case VFK:
        sprintf(str, entry->mnemonic, opK);
        break;
    case VFJK:
        sprintf(str, entry->mnemonic, opJ, opK);
        break;
    case VFKJ:
        sprintf(str, entry->mnemonic, opK, opJ);
        break;
    case VFKJD:
        sprintf(str, entry->mnemonic, opK, opJ, opD);
        break;
    case VFKJID:
        sprintf(str, entry->mnemonic, opK, opJ, opI, opD);
        break;
    case VFKID:
        sprintf(str, entry->mnemonic, opK, opI, opD);
        break;
    case VFKJQ:
        sprintf(str, entry->mnemonic, opK, opJ, opQ);
        break;
    case VFJKQ:
        sprintf(str, entry->mnemonic, opJ, opK, opQ);
        break;
    case VFKQ:
        sprintf(str, entry->mnemonic, opK, opQ);
        break;
    case VFKJDJ:
        sprintf(str, entry->mnemonic, opK, opJ, opD, opJ);
        break;
    case VFQJK:
        sprintf(str, entry->mnemonic, opQ, opJ, opK);
        break;
    case VFJKID:
        sprintf(str, entry->mnemonic, opJ, opK, opI, opD);
        break;
    case VFIDKJ:
        sprintf(str, entry->mnemonic, opI, opD, opK, opJ);
        break;
    case VFJK8:
        sprintf(str, entry->mnemonic, ((u16)opJ << 4) | (u16)opK);
        break;
    case VFJKQ24:
        value = ((u64)opJ << 20) | ((u64)opK << 16) | (u64)opQ;
        if (opJ > 7)
            {
            value |= 0xffffffffff000000;
            }
        sprintf(str, entry->mnemonic, (i64)value);
        break;
    default:
        break;
        }

    fprintf(cpuF[cpu->id], "%-24s", str);

    /*
    **  Dump relevant register set.
    */
    switch (decode[opCode].regSet)
        {
    default:
    case VR:
        break;
    case VRXJ:
        fprintf(cpuF[cpu->id], "X%X=%016lx", opJ, cpu->regX[opJ]);
        break;
    case VRXK:
        fprintf(cpuF[cpu->id], "X%X=%016lx", opK, cpu->regX[opK]);
        break;
    case VRXKXJ:
        fprintf(cpuF[cpu->id], "X%X=%016lx X%X=%016lx", opK, cpu->regX[opK], opJ, cpu->regX[opJ]);
        break;
    case VRAKAJ:
        fprintf(cpuF[cpu->id], "A%X=%012lx     A%X=%012lx", opK, cpu->regA[opK], opJ, cpu->regA[opJ]);
        break;
    case VRAKXJ:
        fprintf(cpuF[cpu->id], "A%X=%012lx     X%X=%016lx", opK, cpu->regA[opK], opJ, cpu->regX[opJ]);
        break;
    case VRXKAJ:
        fprintf(cpuF[cpu->id], "X%X=%016lx A%X=%012lx", opK, cpu->regX[opK], opJ, cpu->regA[opJ]);
        break;
    case VRXKAJX0:
        fprintf(cpuF[cpu->id], "X%X=%016lx A%X=%012lx     X0=%016lx", opK, cpu->regX[opK], opJ, cpu->regA[opJ], cpu->regX[0]);
        break;
    case VRXKXJX1:
        fprintf(cpuF[cpu->id], "X%X=%016lx X%X=%016lx X1=%016lx", opK, cpu->regX[opK], opJ, cpu->regX[opJ], cpu->regX[1]);
        break;
    case VRXKX1:
        fprintf(cpuF[cpu->id], "X%X=%016lx X1=%016lx", opK, cpu->regX[opK], cpu->regX[1]);
        break;
    case XRX1XJXK:
        fprintf(cpuF[cpu->id], "X1=%016lx X%X=%016lx X%X=%016lx", cpu->regX[1], opJ, cpu->regX[opJ], opK, cpu->regX[opK]);
        break;
    case VRXXKXXJ:
        fprintf(cpuF[cpu->id], "XX%X=%016lx %016lx XX%X=%016lx %016lx", opK, cpu->regX[opK], cpu->regX[opK+1], opJ, cpu->regX[opJ], cpu->regX[opJ+1]);
        break;
    case VRX0:
        fprintf(cpuF[cpu->id], "X0=%016lx", cpu->regX[0]);
        break;
    case VRX1:
        fprintf(cpuF[cpu->id], "X1=%016lx", cpu->regX[1]);
        break;
    case VRAJX0AKX1:
        fprintf(cpuF[cpu->id], "A%X=%012lx     X0=%016lx A%X=%012lx     X1=%016lx", opJ, cpu->regA[opJ], cpu->regX[0], opK, cpu->regA[opK], cpu->regX[1]);
        break;
    case VRXJXK:
        fprintf(cpuF[cpu->id], "X%X=%016lx X%X=%016lx", opJ, cpu->regX[opJ], opK, cpu->regX[opK]);
        break;
    case VRX1AJAK:
        fprintf(cpuF[cpu->id], "X1=%016lx A%X=%012lx     A%X=%012lx", cpu->regX[1], opJ, cpu->regA[opJ], opK, cpu->regA[opK]);
        break;
    case VRAKAJXI:
        fprintf(cpuF[cpu->id], "A%X=%012lx     A%X=%012lx     X%X=%016lx", opK, cpu->regA[opK], opJ, cpu->regA[opJ], opI, cpu->regX[opI]);
        break;
    case VRXKAJXI:
        fprintf(cpuF[cpu->id], "X%X=%016lx A%X=%012lx     X%X=%016lx", opK, cpu->regX[opK], opJ, cpu->regA[opJ], opI, cpu->regX[opI]);
        break;
    case VRXKXJXI:
        fprintf(cpuF[cpu->id], "X%X=%016lx X%X=%016lx X%X=%016lx", opK, cpu->regX[opK], opJ, cpu->regX[opJ], opI, cpu->regX[opI]);
        break;
    case VRAJAK:
        fprintf(cpuF[cpu->id], "A%X=%012lx     A%X=%012lx", opJ, cpu->regA[opJ], opK, cpu->regA[opK]);
        break;
    case VRAJX0AKX1XI:
        fprintf(cpuF[cpu->id], "A%X=%012lx     X0=%016lx A%X=%012lx     X1=%016lx X%X=%016lx", opJ, cpu->regA[opJ], cpu->regX[0], opK, cpu->regA[opK], cpu->regX[1], opI, cpu->regX[opI]);
        break;
    case VRAJX0AKX1AI:
        fprintf(cpuF[cpu->id], "A%X=%012lx     X0=%016lx A%X=%012lx     X1=%016lx A%X=%012lx", opJ, cpu->regA[opJ], cpu->regX[0], opK, cpu->regA[opK], cpu->regX[1], opI, cpu->regA[opI]);
        break;
    case VRX0AKX1AI:
        fprintf(cpuF[cpu->id], "X0=%016lx A%X=%012lx     X1=%016lx A%X=%012lx", cpu->regX[0], opK, cpu->regA[opK], cpu->regX[1], opI, cpu->regA[opI]);
        break;
    case VRXIAKX1:
        fprintf(cpuF[cpu->id], "X%X=%016lx A%X=%012lx     X1=%016lx", opI, cpu->regX[opI], opK, cpu->regA[opK], cpu->regX[1]);
        break;
    case VRX1XJXK:
        fprintf(cpuF[cpu->id], "X1=%016lx X%X=%016lx X%X=%016lx", cpu->regX[1], opJ, cpu->regX[opJ], opK, cpu->regX[opK]);
        break;
    case VRAJXK:
        fprintf(cpuF[cpu->id], "A%X=%012lx     X%X=%016lx", opJ, cpu->regA[opJ], opK, cpu->regX[opK]);
        break;
    case VRXKXI:
        fprintf(cpuF[cpu->id], "X%X=%016lx X%X=%016lx", opK, cpu->regX[opK], opI, cpu->regX[opI]);
        break;
        }
    fputs("\n", cpuF[cpu->id]);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Trace a CYBER 170 exchange jump.
**
**  Parameters:     Name        Description.
**                  cpu         Pointer to CPU context
**                  addr        Address of exchange package
**                  title       Title for exchange
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void traceExchange(Cpu170Context *cpu, u32 addr, char *title)
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
    if (title != NULL)
        {
        fprintf(cpuF[cpu->id], "\n%06d Exchange jump with package address %06o (%s)\n", traceSequenceNo, addr, title);
        }
    fputs("\n", cpuF[cpu->id]);
    fprintf(cpuF[cpu->id], "P       %06o  ", cpu->regP);
    fprintf(cpuF[cpu->id], "A%d %06o  ", 0, cpu->regA[0]);
    fprintf(cpuF[cpu->id], "B%d %06o", 0, cpu->regB[0]);
    fputs("\n", cpuF[cpu->id]);

    fprintf(cpuF[cpu->id], "RA     %07o  ", cpu->regRaCm);
    fprintf(cpuF[cpu->id], "A%d %06o  ", 1, cpu->regA[1]);
    fprintf(cpuF[cpu->id], "B%d %06o", 1, cpu->regB[1]);
    fputs("\n", cpuF[cpu->id]);

    fprintf(cpuF[cpu->id], "FL     %07o  ", cpu->regFlCm);
    fprintf(cpuF[cpu->id], "A%d %06o  ", 2, cpu->regA[2]);
    fprintf(cpuF[cpu->id], "B%d %06o", 2, cpu->regB[2]);
    fputs("\n", cpuF[cpu->id]);

    fprintf(cpuF[cpu->id], "RAE   %08o  ", cpu->regRaEcs);
    fprintf(cpuF[cpu->id], "A%d %06o  ", 3, cpu->regA[3]);
    fprintf(cpuF[cpu->id], "B%d %06o", 3, cpu->regB[3]);
    fputs("\n", cpuF[cpu->id]);

    fprintf(cpuF[cpu->id], "FLE   %08o  ", cpu->regFlEcs);
    fprintf(cpuF[cpu->id], "A%d %06o  ", 4, cpu->regA[4]);
    fprintf(cpuF[cpu->id], "B%d %06o", 4, cpu->regB[4]);
    fputs("\n", cpuF[cpu->id]);

    fprintf(cpuF[cpu->id], "EM/FL %08o  ", cpu->exitMode);
    fprintf(cpuF[cpu->id], "A%d %06o  ", 5, cpu->regA[5]);
    fprintf(cpuF[cpu->id], "B%d %06o", 5, cpu->regB[5]);
    fputs("\n", cpuF[cpu->id]);

    fprintf(cpuF[cpu->id], "MA      %06o  ", cpu->regMa);
    fprintf(cpuF[cpu->id], "A%d %06o  ", 6, cpu->regA[6]);
    fprintf(cpuF[cpu->id], "B%d %06o", 6, cpu->regB[6]);
    fputs("\n", cpuF[cpu->id]);

    fprintf(cpuF[cpu->id], "STOP         %d  ", cpu->isStopped ? 1 : 0);
    fprintf(cpuF[cpu->id], "A%d %06o  ", 7, cpu->regA[7]);
    fprintf(cpuF[cpu->id], "B%d %06o  ", 7, cpu->regB[7]);
    fputs("\n", cpuF[cpu->id]);
    fprintf(cpuF[cpu->id], "ECOND       %02o  ", cpu->exitCondition);
    fputs("\n", cpuF[cpu->id]);
    fprintf(cpuF[cpu->id], "MonitorFlag %s", cpu->isMonitorMode ? "TRUE" : "FALSE");
    fputs("\n\n", cpuF[cpu->id]);

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
        fputs("\n", cpuF[cpu->id]);
        }

    fprintf(cpuF[cpu->id], "\n");
    }

/*--------------------------------------------------------------------------
**  Purpose:        Trace a CYBER 180 exchange load or store.
**
**  Parameters:     Name        Description.
**                  cpu         Pointer to CPU context
**                  addr        Address of exchange package
**                  title       Title for exchange
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void traceExchange180(Cpu180Context *cpu, u32 addr, char *title)
    {
    CpWord           data;
    u8               i;
    MonitorCondition cond;
    u32              rma;

    /*
    **  Bail out if no trace of exchange jumps is requested.
    */
    if ((traceMask & TraceExchange) == 0)
        {
        return;
        }
    fprintf(cpuF[cpu->id], "\n%06d %s %08x\n\n", traceSequenceNo, title, addr);
    fprintf(cpuF[cpu->id], " P %02x ", cpu->key);
    tracePrintPva(cpuF[cpu->id], cpu->regP);
    if (cpu180PvaToRma(cpu, cpu->regP & Mask48, AccessModeExecute, &rma, &cond))
        {
        fprintf(cpuF[cpu->id], " (RMA %08x)", rma);
        }
    fputs("\n\n", cpuF[cpu->id]);
    for (i = 0; i < 16; i++)
        {
        data = cpu->regX[i];
        fprintf(cpuF[cpu->id], "A%X ", i);
        tracePrintPva(cpuF[cpu->id], cpu->regA[i]);
        fprintf(cpuF[cpu->id],"   X%X %04x %04x %04x %04x\n", i,
                (PpWord)((data >> 48) & Mask16),
                (PpWord)((data >> 32) & Mask16),
                (PpWord)((data >> 16) & Mask16),
                (PpWord)((data) & Mask16));
        }
    fputs("\n", cpuF[cpu->id]);
    fprintf(cpuF[cpu->id], "VMID %04x  VMCL %04x   LPID %02x\n", cpu->regVmid, cpu->regVmcl, cpu->regLpid);
    fprintf(cpuF[cpu->id], " UMR %04x   MMR %04x  Flags %04x\n", cpu->regUmr, cpu->regMmr, cpu->regFlags);
    fprintf(cpuF[cpu->id], " UCR %04x   MCR %04x    MDF %04x\n", cpu->regUcr, cpu->regMcr, cpu->regMdf);
    fputs("\n", cpuF[cpu->id]);
    fprintf(cpuF[cpu->id], " MPS %08x  SIT %08x\n", cpu->regMps, cpu->regSit);
    fprintf(cpuF[cpu->id], " JPS %08x  PIT %08x\n", cpu->regJps, cpu->regPit);
    fprintf(cpuF[cpu->id], "  BC %08x\n", cpu->regBc);
    fputs("\n", cpuF[cpu->id]);
    fprintf(cpuF[cpu->id], " PTA %08x  STA %08x\n", cpu->regPta, cpu->regSta);
    fprintf(cpuF[cpu->id], " PTL %02x        STL %03x\n", cpu->regPtl, cpu->regStl);
    fprintf(cpuF[cpu->id], " PSM %02x\n", cpu->regPsm);
    fputs("\n", cpuF[cpu->id]);
    fputs(" UTP ", cpuF[cpu->id]);
    tracePrintPva(cpuF[cpu->id], cpu->regUtp);
    fputs("  TP ", cpuF[cpu->id]);
    tracePrintPva(cpuF[cpu->id], cpu->regTp);
    fputs("\n", cpuF[cpu->id]);
    fputs(" DLP ", cpuF[cpu->id]);
    tracePrintPva(cpuF[cpu->id], cpu->regDlp);
    fprintf(cpuF[cpu->id], "  DI %02x\n", cpu->regDi);
    fprintf(cpuF[cpu->id], "                     DM %02x\n", cpu->regDm);
    fputs("\n", cpuF[cpu->id]);
    fprintf(cpuF[cpu->id], " LRN %d\n", cpu->regLrn);
    for (i = 0; i < 15; i++)
        {
        fprintf(cpuF[cpu->id], " TOS[%x] ", i + 1);
        tracePrintPva(cpuF[cpu->id], cpu->regTos[i]);
        fputs("\n", cpuF[cpu->id]);
        }
    fputs("\n", cpuF[cpu->id]);
    fprintf(cpuF[cpu->id], " MDW %016lx  \n", cpu->regMdw);
    fputs("\n", cpuF[cpu->id]);
    fprintf(cpuF[cpu->id], "MonitorFlag  %s\n", cpu->isMonitorMode ? "TRUE" : "FALSE");
    fprintf(cpuF[cpu->id], "STOP         %d\n", cpu->isStopped ? 1 : 0);
    fputs("\n", cpuF[cpu->id]);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Trace a CYBER 170 trap frame.
**
**  Parameters:     Name        Description.
**                  cpu         Pointer to CYBER 180 CPU context
**                  rma         Stack frame address
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void traceTrapFrame170(Cpu180Context *cpu, u32 rma)
    {
    u8  i;
    u64 word;
    u32 wordAddr;

    /*
    **  Bail out if no trace of exchange jumps is requested.
    */
    if ((traceMask & TraceExchange) == 0)
        {
        return;
        }
    fprintf(cpuF[cpu->id], "\n%06d CYBER 170 trap frame pushed at %08x\n\n", traceSequenceNo, rma);
    wordAddr = rma >> 3;
    fprintf(cpuF[cpu->id], "P %lx\n", cpMem[wordAddr++]);
    word = cpMem[wordAddr++];
    fprintf(cpuF[cpu->id], "VMID %04x   A0 %012lx\n", (u16)((word >> 56) & Mask4), word & Mask48);
    word = cpMem[wordAddr++];
    fprintf(cpuF[cpu->id], "Desc %04x   A1 %012lx\n", (u16)((word >> 48) & Mask16), word & Mask48);
    word = cpMem[wordAddr++];
    fprintf(cpuF[cpu->id], "UMR  %04x   A2 %012lx\n", (u16)((word >> 48) & Mask16), word & Mask48);
    word = cpMem[wordAddr++];
    fprintf(cpuF[cpu->id], "EM   %04o   RA %07o\n", (u16)((word >> 32) & Mask12), (u32)(word & Mask32));
    word = cpMem[wordAddr++];
    fprintf(cpuF[cpu->id], "UCR  %04x%s  FL %07o\n", (u16)((word >> 48) & Mask16), (word >> 32) & 1 ? "*" : " ", (u32)(word & Mask32));
    word = cpMem[wordAddr++];
    fprintf(cpuF[cpu->id], "MCR  %04x   MA %07o\n", (u16)((word >> 48) & Mask16), (u32)(word & Mask32));
    fprintf(cpuF[cpu->id], "           RAE %07o\n", (u32)(cpMem[wordAddr++] & Mask32));
    fprintf(cpuF[cpu->id], "           FLE %07o\n\n", (u32)(cpMem[wordAddr++] & Mask32));
    fprintf(cpuF[cpu->id], "A0 %06o  B0 000000\n", (u32)(cpMem[wordAddr] & Mask32));
    for (i = 1; i < 8; i++)
        {
        fprintf(cpuF[cpu->id], "A%d %06o  B%d %06o\n", i, (u32)(cpMem[wordAddr + i] & Mask32), i, (u32)(cpMem[wordAddr + i + 8] & Mask32));
        }
    wordAddr += 16;
    fputs("\n", cpuF[cpu->id]);
    for (i = 0; i < 8; i++)
        {
        fprintf(cpuF[cpu->id], "X%d %020lo\n", i, cpMem[wordAddr++] & Mask60);
        }
    fputs("\n", cpuF[cpu->id]);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Trace a CYBER 180 trap frame.
**
**  Parameters:     Name        Description.
**                  cpu         Pointer to CYBER 180 CPU context
**                  rma         Stack frame address
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void traceTrapFrame180(Cpu180Context *cpu, u32 rma)
    {
    u8  i;
    u64 word;
    u32 wordAddr;

    /*
    **  Bail out if no trace of exchange jumps is requested.
    */
    if ((traceMask & TraceExchange) == 0)
        {
        return;
        }
    fprintf(cpuF[cpu->id], "\n%06d CYBER 180 trap frame pushed at %08x\n\n", traceSequenceNo, rma);
    wordAddr = rma >> 3;
    fprintf(cpuF[cpu->id], "P %lx\n", cpMem[wordAddr++]);
    word = cpMem[wordAddr++];
    fprintf(cpuF[cpu->id], "VMID %04x  A0 %012lx\n", (u8)((word >> 56) & Mask4), word & Mask48);
    word = cpMem[wordAddr++];
    fprintf(cpuF[cpu->id], "Desc %04x  A1 %012lx\n", (u16)((word >> 48) & Mask16), word & Mask48);
    word = cpMem[wordAddr++];
    fprintf(cpuF[cpu->id], "UMR  %04x  A2 %012lx\n", (u16)((word >> 48) & Mask16), word & Mask48);
    fprintf(cpuF[cpu->id], "           A3 %012lx\n", cpMem[wordAddr++] & Mask48);
    word = cpMem[wordAddr++];
    fprintf(cpuF[cpu->id], "UCR  %04x  A4 %012lx\n", (u16)((word >> 48) & Mask16), word & Mask48);
    word = cpMem[wordAddr++];
    fprintf(cpuF[cpu->id], "MCR  %04x  A5 %012lx\n", (u16)((word >> 48) & Mask16), word & Mask48);
    for (i = 6; i < 16; i++)
        {
        fprintf(cpuF[cpu->id], "           A%X %012lx\n", i, cpMem[wordAddr++] & Mask48);
        }
    fputs("\n", cpuF[cpu->id]);
    for (i = 0; i < 16; i++)
        {
        fprintf(cpuF[cpu->id], "X%X %016lx\n", i, cpMem[wordAddr++]);
        }
    fputs("\n", cpuF[cpu->id]);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Trace a CYBER 180 trap pointer.
**
**  Parameters:     Name        Description.
**                  cpu         Pointer to CYBER 180 CPU context
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void traceTrapPointer(Cpu180Context *cpu)
    {
    u64              bsp;
    u64              cbp;
    MonitorCondition cond;
    bool             isExt;
    u32              rma;
    u8               vmid;

    /*
    **  Bail out if no trace of exchange jumps is requested.
    */
    if ((traceMask & TraceExchange) == 0)
        {
        return;
        }
    fprintf(cpuF[cpu->id], "%06d CYBER 180 trap pointer %012lx ", traceSequenceNo, cpu->regTp);
    if (cpu180PvaToRma(cpu, cpu->regTp, AccessModeRead, &rma, &cond) == FALSE)
        {
        fprintf(cpuF[cpu->id], "%s\n", traceMonitorConditionToStr(cond));
        return;
        }
    cbp   = cpMem[rma >> 3];
    vmid  = (cbp >> 56) & Mask4;
    fprintf(cpuF[cpu->id], "RMA %08x VMID %x CBP %lx", rma, vmid, cbp);
    isExt = vmid == 0 && ((cbp >> 55) & 1) != 0;
    if (isExt)
        {
        fprintf(cpuF[cpu->id], "\n         Binding section pointer %lx ", cpu->regTp + 8);
        if (cpu180PvaToRma(cpu, cpu->regTp + 8, AccessModeRead, &rma, &cond) == FALSE)
            {
            fprintf(cpuF[cpu->id], "%s\n", traceMonitorConditionToStr(cond));
            return;
            }
        bsp = cpMem[rma >> 3] & Mask48;
        fprintf(cpuF[cpu->id], "RMA %08x Binding section %012lx", rma, bsp);
        }
    fputs("\n", cpuF[cpu->id]);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Trace a monitor condition.
**
**  Parameters:     Name        Description.
**                  cond        monitor condition ordinal
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static char *traceMonitorConditionToStr(MonitorCondition cond)
    {
    switch (cond)
        {
    case MCR48:
        return "Detected uncorrectable error";
    case MCR49:
        return "Not assigned";
    case MCR50:
        return "Short warning";
    case MCR51:
        return "Instruction specfication error";
    case MCR52:
        return "Address specification error";
    case MCR53:
        return "CYBER 170 state exchange request";
    case MCR54:
        return "Access violation";
    case MCR55:
        return "Environment specification error";
    case MCR56:
        return "External interrupt";
    case MCR57:
        return "Page table search without find";
    case MCR58:
        return "System call (status bit)";
    case MCR59:
        return "System interval timer";
    case MCR60:
        return "Invalid segment / Ring number 0";
    case MCR61:
        return "Outward call / Inward return";
    case MCR62:
        return "Soft error";
    case MCR63:
        return "Trap exception (status bit)";
    default:
        return "";
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Trace a monitor condition.
**
**  Parameters:     Name        Description.
**                  cond        monitor condition ordinal
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void traceMonitorCondition(Cpu180Context *cpu, MonitorCondition cond)
    {
    if ((traceMask & (TracePva | TraceCpu)) != 0)
        {
        fprintf(cpuF[cpu->id], "%06d MCR%02d %s\n", traceSequenceNo, (cond - MCR48) + 48, traceMonitorConditionToStr(cond));
        fprintf(cpuF[cpu->id], "%06d       Action %s, P %012lx\n", traceSequenceNo, traceTranslateAction(cpu->pendingAction), cpu->nextP);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Trace info about a page.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void tracePageInfo(Cpu180Context *cpu, u16 hash, u32 pageNum, u32 pageOffset, u32 pageTableIdx, u64 spid)
    {
    if ((traceMask & TracePva) != 0)
        {
        fprintf(cpuF[cpu->id], "%06d hash %04x pageNum %x pageOffset %x pageTableAddr %08x SPID %lx\n", traceSequenceNo,
            hash, pageNum, pageOffset, pageTableIdx << 3, spid);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Trace a page table entry.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void tracePte(Cpu180Context *cpu, u64 pte)
    {
    if ((traceMask & TracePva) != 0)
        {
        fprintf(cpuF[cpu->id], "%06d PTE V %x C %x U %x M %x SPID %010lx PFA %05x\n", traceSequenceNo,
            (u8)((pte >> 63) & 1), (u8)((pte >> 62) & 1), (u8)((pte >> 61) & 1), (u8)((pte >> 60) & 1),
            (pte >> 22) & Mask38, (u32)((pte >> 2) & Mask20));
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Trace a process virtual address.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void tracePva(Cpu180Context *cpu, u64 pva)
    {
    if ((traceMask & TracePva) != 0)
        {
        fprintf(cpuF[cpu->id], "%06d PVA %x %03x %08x\n", traceSequenceNo,
            (u8)((pva >> 44) & Mask4), (u16)((pva >> 32) & Mask12), (u32)(pva & Mask32));
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Trace a real memory address.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void traceRma(Cpu180Context *cpu, u32 rma)
    {
    if ((traceMask & TracePva) != 0)
        {
        fprintf(cpuF[cpu->id], "%06d RMA %08x\n", traceSequenceNo, rma);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Trace a segment descriptor table entry.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void traceSde(Cpu180Context *cpu, u64 sde)
    {
    if ((traceMask & TracePva) != 0)
        {
        fprintf(cpuF[cpu->id], "%06d SDE VL %x XP %x RP %x WP %x R1 %x R2 %x ASID %04x Lock %02x\n", traceSequenceNo,
            (u8)((sde >> 62) & Mask2), (u8)((sde >> 60) & Mask2), (u8)((sde >> 58) & Mask2), (u8)((sde >> 56) & Mask2),
            (u8)((sde >> 52) & Mask4), (u8)((sde >> 48) & Mask4),
            (u16)((sde >> 32) & Mask16), (u8)((sde >> 24) & Mask6));
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Translate an action indication to a string.
**
**  Parameters:     Name        Description.
**                  action      action ordinal
**
**  Returns:        String representation.
**
**------------------------------------------------------------------------*/
char *traceTranslateAction(ConditionAction action)
    {
    switch (action)
        {
    case Rni:
        return "RNI";
    case Stack:
        return "Stack";
    case Trap:
        return "Trap";
    case Exch:
        return "Exchange";
    case Halt:
        return "Halt";
    default:
        return "Unknown";
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Trace a user condition.
**
**  Parameters:     Name        Description.
**                  cond        user condition ordinal
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void traceUserCondition(Cpu180Context *cpu, UserCondition cond)
    {
    char *s;

    if ((traceMask & (TracePva | TraceCpu)) != 0)
        {
        switch (cond)
            {
        case UCR48:
            s = "Privileged instruction fault";
            break;
        case UCR49:
            s = "Unimplemented instruction";
            break;
        case UCR50:
            s = "Free flag";
            break;
        case UCR51:
            s = "Process interval timer";
            break;
        case UCR52:
            s = "Inter-ring pop";
            break;
        case UCR53:
            s = "Critical frame flag";
            break;
        case UCR54:
            s = "Reserved";
            break;
        case UCR55:
            s = "Divide fault";
            break;
        case UCR56:
            s = "Debug";
            break;
        case UCR57:
            s = "Arithmetic overflow";
            break;
        case UCR58:
            s = "Exponent overflow";
            break;
        case UCR59:
            s = "Exponent underflow";
            break;
        case UCR60:
            s = "FP loss of significance";
            break;
        case UCR61:
            s = "FP indefinite";
            break;
        case UCR62:
            s = "Arithmetic loss of significance";
            break;
        case UCR63:
            s = "Invalid BDP data";
            break;
        default:
            s = "";
            break;
            }
        fprintf(cpuF[cpu->id], "%06d UCR%d %s\n", traceSequenceNo, (cond - MCR48) + 48, s);
        fprintf(cpuF[cpu->id], "%06d       Action %s, P %012lx\n", traceSequenceNo, traceTranslateAction(cpu->pendingAction), cpu->nextP);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Trace virtual address translation registers.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void traceVmRegisters(Cpu180Context *cpu)
    {
    if ((traceMask & TracePva) != 0)
        {
        fprintf(cpuF[cpu->id], "%06d STA %08x STL %d PTA %08x PTL %d PSM %02x pnShift %d poMask %x plMask %x\n",
            traceSequenceNo, cpu->regSta, cpu->regStl, cpu->regPta, cpu->regPtl, cpu->regPsm,
            cpu->pageNumShift, cpu->pageOffsetMask, cpu->pageLengthMask);
        }
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
void traceRegisters(bool isPost)
    {
    u8 op;

    /*
    **  Bail out if no trace of this PPU is requested.
    */
    if ((traceMask & (1 << activePpu->id)) == 0)
        {
        return;
        }

    op = activePpu->opF & 077;

    /*
    **  Print registers.
    */
    fprintf(ppuF[activePpu->id], "P:%04o  ", activePpu->regP);
    fprintf(ppuF[activePpu->id], "A:%06o", activePpu->regA);
    if (isPost && ((features & HasRelocationReg) != 0) && ((op >= 060 && op <= 063) || activePpu->opF == 01000 || activePpu->opF == 01001))
        {
        fprintf(ppuF[activePpu->id], "  R:%o", activePpu->regR);
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
void traceOpcode(void)
    {
    u8           addrMode;
    DecPpControl *decodeTable;
    char         *fmt;
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
    if (isCyber180)
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
        fprintf(ppuF[activePpu->id], "%04o,%02o ", activePpu->mem[activePpu->regP + 1] & Mask12, opD);
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
        fprintf(ppuF[activePpu->id], "%02o%04o  ", opD, activePpu->mem[activePpu->regP + 1] & Mask12);
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
    if (isCyber180)
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
        sprintf(str, "%04o,%02o ", *pm & Mask12, opD);
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
        sprintf(str, "%02o%04o  ", opD, *pm & Mask12);
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
void traceCpuPrint(Cpu170Context *cpu, char *str)
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

    fprintf(ppuF[activePpu->id], "  CH%02o:%c%c%c%c", ch,
            channel[ch].active           ? 'A' : 'D',
            channel[ch].full             ? 'F' : 'E',
            channel[ch].ioDevice == NULL ? 'I' : 'S',
            channel[ch].flag             ? '*' : ' ');
    }

/*--------------------------------------------------------------------------
**  Purpose:        Output data on channel.
**
**  Parameters:     Name        Description.
**                  ch          channel number.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void traceChannelIo(u8 ch)
    {
    /*
    **  Bail out if no trace of this PPU is requested.
    */
    if ((traceMask & (1 << activePpu->id)) == 0)
        {
        return;
        }
    
    if (isCyber180)
        {
        fprintf(ppuF[activePpu->id], "%06o ", channel[ch].data);
        }
    else
        {
        fprintf(ppuF[activePpu->id], "%04o ", channel[ch].data);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Output a 60-bit CM word read/written by a PP.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void traceCmWord(CpWord data)
    {
    /*
    **  Bail out if no trace of this PPU is requested.
    */
    if ((traceMask & (1 << activePpu->id)) == 0)
        {
        return;
        }

    fprintf(ppuF[activePpu->id], "%04o %04o %04o %04o %04o ",
        (PpWord)((data >> 48) & Mask12),
        (PpWord)((data >> 36) & Mask12),
        (PpWord)((data >> 24) & Mask12),
        (PpWord)((data >> 12) & Mask12),
        (PpWord)(data & Mask12));
    }

/*--------------------------------------------------------------------------
**  Purpose:        Output a 64-bit CM word read/written by a PP.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void traceCmWord64(CpWord data)
    {
    /*
    **  Bail out if no trace of this PPU is requested.
    */
    if ((traceMask & (1 << activePpu->id)) == 0)
        {
        return;
        }

    fprintf(ppuF[activePpu->id], "%04x %04x %04x %04x ",
        (PpWord)((data >> 48) & Mask16),
        (PpWord)((data >> 32) & Mask16),
        (PpWord)((data >> 16) & Mask16),
        (PpWord)(data & Mask16));
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

/*--------------------------------------------------------------------------
**  Purpose:        Log a stack trace
**
**  Parameters:     none
**
**  Returns:        nothing
**
**------------------------------------------------------------------------*/
void traceStack(FILE *fp)
    {
#if defined(__APPLE__)
    void *callstack[128];
    int  i;
    int  frames;
    char **strs;

    frames = backtrace(callstack, 128);
    strs   = backtrace_symbols(callstack, frames);
    for (i = 1; i < frames; ++i)
        {
        fprintf(fp, "%s\n", strs[i]);
        }
    free(strs);
#endif
    }


/*---------------------------  End Of File  ------------------------------*/
