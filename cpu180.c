/*--------------------------------------------------------------------------
**
**  Copyright (c) 2025, Kevin Jordan
**
**  Name: cpu180.c
**
**  Description:
**      Perform emulation of CDC CYBER 180 class CPU.
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

//TODO: See MIGDS 7-27 mapping of RAE and FLE

#define DEBUG 0

/*
**  -------------
**  Include Files
**  -------------
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "const.h"
#include "proto.h"
#include "types.h"
#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

/*
**  -----------------
**  Private Constants
**  -----------------
*/

/*
**  Mask used in preserving left half of X register
*/
#define LeftMask 0xffffffff00000000

/*
**  Masks used in isolating ring and segment in PVA's
*/
#define RingMask    0xf00000000000
#define SegMask     0x0fff00000000
#define RingSegMask 0xffff00000000

/*
**  -----------------------
**  Private Macro Functions
**  -----------------------
*/
#define PageOf(pva, ctx) (((pva) & Mask32) >> (ctx)->pageNumShift)
#define RingOf(pva) (((pva) >> 44) & 0xf)
#define SegmentOf(pva) (((pva) >> 32) & 0xfff)

/*
**  -----------------------------------------
**  Private Typedef and Structure Definitions
**  -----------------------------------------
*/

/*
**  CYBER 180 CPU instruction formats
*/
typedef enum
    {
    jk = 0,
    jkiD,
    jkQ
    } InstFormat;

typedef struct opDispatch
    {
    void (*execute)(Cpu180Context *activeCpu);
    InstFormat format;
    } OpDispatch;

/*
**  Structure defining action decisions on monitor and user conditions
*/
typedef struct conditionActionDefn
    {
    u16  bitMask;                          // condition register bit mask
    bool isCompleted;                      // is execution completed
    bool isThis;                           // does P register remain on this instruction
    ConditionAction whenMaskTrapJob;       // action when mask bit set, trap enabled, job mode
    ConditionAction whenMaskTrapMonitor;   // action when mask bit set, trap enabled, monitor mode
    ConditionAction whenMaskNoTrapJob;     // action when mask bit set, trap disabled, job mode
    ConditionAction whenMaskNoTrapMonitor; // action when mask bit set, trap disabled, monitor mode
    ConditionAction whenNoMask;            // action when mask bit not set
    } ConditionActionDefn;

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static bool cpu180AddInt32(Cpu180Context *ctx, u32 augend, u32 addend, u32 *sum);
static bool cpu180AddInt64(Cpu180Context *ctx, u64 augend, u64 addend, u64 *sum);
static void cpu180CheckMonitorConditions(Cpu180Context *ctx);
static void cpu180CheckUserConditions(Cpu180Context *ctx);
static void cpu180Exchange(Cpu180Context *activeCpu);
static bool cpu180FindPte(Cpu180Context *ctx, u16 asid, u32 byteNum, bool ignoreValidity, u32 *pti, u8 *count);
static bool cpu180GetBdpDescriptor(Cpu180Context *ctx, u64 pva, u8 aRegNum, u8 xRegNum, BdpDescriptor *descriptor);
static bool cpu180GetByte(Cpu180Context *ctx, u64 pva, Cpu180AccessMode access, u8 *byte);
static bool cpu180GetBytes(Cpu180Context *ctx, u64 pva, int count, Cpu180AccessMode access, u64 *word);
static u8 cpu180GetCurrentXp(Cpu180Context *ctx);
static u8 cpu180GetLock(Cpu180Context *ctx, u64 pva);
static bool cpu180GetParcel(Cpu180Context *ctx, u64 pva, u16 *parcel);
static u8 cpu180GetR1(Cpu180Context *ctx, u64 pva);
static u8 cpu180GetR2(Cpu180Context *ctx, u64 pva);
static bool cpu180IsBindingSectionRef(Cpu180Context *ctx, u64 pva);
static bool cpu180IsValidSegment(Cpu180Context *ctx, u64 pva);
static bool cpu180IsValidSegment(Cpu180Context *ctx, u64 pva);
static bool cpu180MulInt64(Cpu180Context *ctx, u64 mltand, u64 mltier, u64 *product);
static bool cpu180PushFrame(Cpu180Context *ctx, u16 at, u16 xs, u16 xt, bool isTrap, u64 *sfsa, u32 *frameSize);
static bool cpu180PutByte(Cpu180Context *ctx, u64 pva, u8 byte);
static bool cpu180PutBytes(Cpu180Context *ctx, u64 pva, u64 word, int count);
static void cpu180SetRingZeroCondition(Cpu180Context *ctx, u64 pva);
static void cpu180Store180Xp(Cpu180Context *ctx, u32 xpa);
static void cpu180Trap(Cpu180Context *ctx);
static bool cpu180ValidateAccess(Cpu180Context *ctx, u64 sde, u8 ring, Cpu180AccessMode access);

/*
**                                                  Op  Mnemonic   MIGDS
**                                                  --  --------   -----   */
static void cp180Op00(Cpu180Context *activeCpu); // 00  HALT       2-122
static void cp180Op01(Cpu180Context *activeCpu); // 01  SYNC       2-138
static void cp180Op02(Cpu180Context *activeCpu); // 02  EXCHANGE   2-132
static void cp180Op03(Cpu180Context *activeCpu); // 03  INTRUPT    2-141
static void cp180Op04(Cpu180Context *activeCpu); // 04  RETURN     2-127
static void cp180Op05(Cpu180Context *activeCpu); // 05  PURGE      2-147
static void cp180Op06(Cpu180Context *activeCpu); // 06  POP        2-129
static void cp180Op07(Cpu180Context *activeCpu); // 07  PSFSA      2-138
static void cp180Op08(Cpu180Context *activeCpu); // 08  CPYTX      2-137
static void cp180Op09(Cpu180Context *activeCpu); // 09  CPYAA      2-28
static void cp180Op0A(Cpu180Context *activeCpu); // 0A  CPYXA      2-28
static void cp180Op0B(Cpu180Context *activeCpu); // 0B  CPYAX      2-28
static void cp180Op0C(Cpu180Context *activeCpu); // 0C  CPYRR      2-28
static void cp180Op0D(Cpu180Context *activeCpu); // 0D  CPYXX      2-28
static void cp180Op0E(Cpu180Context *activeCpu); // 0E  CPYSX      2-146
static void cp180Op0F(Cpu180Context *activeCpu); // 0F  CPYXS      2-146

static void cp180Op10(Cpu180Context *activeCpu); // 10  INCX       2-20
static void cp180Op11(Cpu180Context *activeCpu); // 11  DECX       2-20

static void cp180Op14(Cpu180Context *activeCpu); // 14  LBSET      2-136

static void cp180Op16(Cpu180Context *activeCpu); // 16  TPAGE      2-137
static void cp180Op17(Cpu180Context *activeCpu); // 17  LPAGE      2-139
static void cp180Op18(Cpu180Context *activeCpu); // 18  IORX       2-34
static void cp180Op19(Cpu180Context *activeCpu); // 19  XORX       2-34
static void cp180Op1A(Cpu180Context *activeCpu); // 1A  ANDX       2-34
static void cp180Op1B(Cpu180Context *activeCpu); // 1B  NOTX       2-34
static void cp180Op1C(Cpu180Context *activeCpu); // 1C  INHX       2-35

static void cp180Op1E(Cpu180Context *activeCpu); // 1E  MARK       2-37
static void cp180Op1F(Cpu180Context *activeCpu); // 1F  ENTZ/O/S   2-31

static void cp180Op20(Cpu180Context *activeCpu); // 20  ADDR       2-22
static void cp180Op21(Cpu180Context *activeCpu); // 21  SUBR       2-22
static void cp180Op22(Cpu180Context *activeCpu); // 22  MULR       2-23
static void cp180Op23(Cpu180Context *activeCpu); // 23  DIVR       2-23
static void cp180Op24(Cpu180Context *activeCpu); // 24  ADDX       2-20
static void cp180Op25(Cpu180Context *activeCpu); // 25  SUBX       2-20
static void cp180Op26(Cpu180Context *activeCpu); // 26  MULX       2-21
static void cp180Op27(Cpu180Context *activeCpu); // 27  DIVX       2-21
static void cp180Op28(Cpu180Context *activeCpu); // 28  INCR       2-22
static void cp180Op29(Cpu180Context *activeCpu); // 29  DECR       2-22
static void cp180Op2A(Cpu180Context *activeCpu); // 2A  ADDAX      2-29

static void cp180Op2C(Cpu180Context *activeCpu); // 2C  CMPR       2-24
static void cp180Op2D(Cpu180Context *activeCpu); // 2D  CMPX       2-24
static void cp180Op2E(Cpu180Context *activeCpu); // 2E  BRREL      2-27
static void cp180Op2F(Cpu180Context *activeCpu); // 2F  BRDIR      2-27

static void cp180Op30(Cpu180Context *activeCpu); // 30  ADDF       2-73
static void cp180Op31(Cpu180Context *activeCpu); // 31  SUBF       2-73
static void cp180Op32(Cpu180Context *activeCpu); // 32  MULF       2-76
static void cp180Op33(Cpu180Context *activeCpu); // 33  DIVF       2-77
static void cp180Op34(Cpu180Context *activeCpu); // 34  ADDD       2-79
static void cp180Op35(Cpu180Context *activeCpu); // 35  SUBD       2-79
static void cp180Op36(Cpu180Context *activeCpu); // 36  MULD       2-82
static void cp180Op37(Cpu180Context *activeCpu); // 37  DIVD       2-84

static void cp180Op39(Cpu180Context *activeCpu); // 39  ENTX       2-31
static void cp180Op3A(Cpu180Context *activeCpu); // 3A  CNIF       2-71
static void cp180Op3B(Cpu180Context *activeCpu); // 3B  CNFI       2-72
static void cp180Op3C(Cpu180Context *activeCpu); // 3C  CMPF       2-89
static void cp180Op3D(Cpu180Context *activeCpu); // 3D  ENTP       2-30
static void cp180Op3E(Cpu180Context *activeCpu); // 3E  ENTN       2-30
static void cp180Op3F(Cpu180Context *activeCpu); // 3F  ENTL       2-31

static void cp180Op40(Cpu180Context *activeCpu); // 40  ADDFV      2-209
static void cp180Op41(Cpu180Context *activeCpu); // 41  SUBFV      2-209
static void cp180Op42(Cpu180Context *activeCpu); // 42  MULFV      2-209
static void cp180Op43(Cpu180Context *activeCpu); // 43  DIVFV      2-209
static void cp180Op44(Cpu180Context *activeCpu); // 44  ADDXV      2-207
static void cp180Op45(Cpu180Context *activeCpu); // 45  SUBXV      2-207

static void cp180Op48(Cpu180Context *activeCpu); // 48  IORV       2-209
static void cp180Op49(Cpu180Context *activeCpu); // 49  XORV       2-209
static void cp180Op4A(Cpu180Context *activeCpu); // 4A  ANDV       2-209
static void cp180Op4B(Cpu180Context *activeCpu); // 4B  CNIFV      2-209
static void cp180Op4C(Cpu180Context *activeCpu); // 4C  CNFIV      2-209
static void cp180Op4D(Cpu180Context *activeCpu); // 4D  SHFV       2-208

static void cp180Op50(Cpu180Context *activeCpu); // 50  COMPEQV    2-207
static void cp180Op51(Cpu180Context *activeCpu); // 51  CMPLTV     2-207
static void cp180Op52(Cpu180Context *activeCpu); // 52  CMPGEV     2-207
static void cp180Op53(Cpu180Context *activeCpu); // 53  CMPNEV     2-207
static void cp180Op54(Cpu180Context *activeCpu); // 54  MRGV       2-210
static void cp180Op55(Cpu180Context *activeCpu); // 55  GTHV       2-210
static void cp180Op56(Cpu180Context *activeCpu); // 56  SCTV       2-210
static void cp180Op57(Cpu180Context *activeCpu); // 57  SUMFV      2-210
static void cp180Op58(Cpu180Context *activeCpu); // 58  TPSFV      2-216
static void cp180Op59(Cpu180Context *activeCpu); // 59  TPDFV      2-216
static void cp180Op5A(Cpu180Context *activeCpu); // 5A  TSPFV      2-216
static void cp180Op5B(Cpu180Context *activeCpu); // 5B  TDPFV      2-216
static void cp180Op5C(Cpu180Context *activeCpu); // 5C  SUMPFV     2-216
static void cp180Op5D(Cpu180Context *activeCpu); // 5D  GTHIV      2-217
static void cp180Op5E(Cpu180Context *activeCpu); // 5E  SCTIV      2-217

static void cp180Op70(Cpu180Context *activeCpu); // 70  ADDN       2-47
static void cp180Op71(Cpu180Context *activeCpu); // 71  SUBN       2-47
static void cp180Op72(Cpu180Context *activeCpu); // 72  MULN       2-47
static void cp180Op73(Cpu180Context *activeCpu); // 73  DIVN       2-47
static void cp180Op74(Cpu180Context *activeCpu); // 74  CMPN       2-52
static void cp180Op75(Cpu180Context *activeCpu); // 75  MOVN       2-51
static void cp180Op76(Cpu180Context *activeCpu); // 76  MOVB       2-55
static void cp180Op77(Cpu180Context *activeCpu); // 77  CMPB       2-52

static void cp180Op80(Cpu180Context *activeCpu); // 80  LMULT      2-16
static void cp180Op81(Cpu180Context *activeCpu); // 81  SMULT      2-16
static void cp180Op82(Cpu180Context *activeCpu); // 82  LX         2-12
static void cp180Op83(Cpu180Context *activeCpu); // 83  SX         2-12
static void cp180Op84(Cpu180Context *activeCpu); // 84  LA         2-15
static void cp180Op85(Cpu180Context *activeCpu); // 85  SA         2-15
static void cp180Op86(Cpu180Context *activeCpu); // 86  LBYTP,j    2-13
static void cp180Op87(Cpu180Context *activeCpu); // 87  ENTC       2-31
static void cp180Op88(Cpu180Context *activeCpu); // 88  LBIT       2-14
static void cp180Op89(Cpu180Context *activeCpu); // 89  SBIT       2-14
static void cp180Op8A(Cpu180Context *activeCpu); // 8A  ADDRQ      2-22
static void cp180Op8B(Cpu180Context *activeCpu); // 8B  ADDXQ      2-20
static void cp180Op8C(Cpu180Context *activeCpu); // 8C  MULRQ      2-23
static void cp180Op8D(Cpu180Context *activeCpu); // 8D  ENTE       2-30
static void cp180Op8E(Cpu180Context *activeCpu); // 8E  ADDAQ      2-29
static void cp180Op8F(Cpu180Context *activeCpu); // 8F  ADDPXQ     2-29

static void cp180Op90(Cpu180Context *activeCpu); // 90  BRREQ      2-25
static void cp180Op91(Cpu180Context *activeCpu); // 91  BRRNE      2-25
static void cp180Op92(Cpu180Context *activeCpu); // 92  BRRGT      2-25
static void cp180Op93(Cpu180Context *activeCpu); // 93  BRRGE      2-25
static void cp180Op94(Cpu180Context *activeCpu); // 94  BRXEQ      2-25
static void cp180Op95(Cpu180Context *activeCpu); // 95  BRXNE      2-25
static void cp180Op96(Cpu180Context *activeCpu); // 96  BRXGT      2-25
static void cp180Op97(Cpu180Context *activeCpu); // 97  BRXGE      2-25
static void cp180Op98(Cpu180Context *activeCpu); // 98  BRFEQ      2-87
static void cp180Op99(Cpu180Context *activeCpu); // 99  BRFNE      2-87
static void cp180Op9A(Cpu180Context *activeCpu); // 9A  BRFGT      2-87
static void cp180Op9B(Cpu180Context *activeCpu); // 9B  BRFGE      2-87
static void cp180Op9C(Cpu180Context *activeCpu); // 9C  BRINC      2-26
static void cp180Op9D(Cpu180Context *activeCpu); // 9D  BRSEG      2-26
static void cp180Op9E(Cpu180Context *activeCpu); // 9E  BR---      2-88
static void cp180Op9F(Cpu180Context *activeCpu); // 9F  BRCR       2-142

static void cp180OpA0(Cpu180Context *activeCpu); // A0  LAI        2-15
static void cp180OpA1(Cpu180Context *activeCpu); // A1  SAI        2-15
static void cp180OpA2(Cpu180Context *activeCpu); // A2  LXI        2-12
static void cp180OpA3(Cpu180Context *activeCpu); // A3  SXI        2-12
static void cp180OpA4(Cpu180Context *activeCpu); // A4  LBYT,X0    2-13
static void cp180OpA5(Cpu180Context *activeCpu); // A5  SBYT,X0    2-13

static void cp180OpA7(Cpu180Context *activeCpu); // A7  ADDAD      2-30
static void cp180OpA8(Cpu180Context *activeCpu); // A8  SHFC       2-33
static void cp180OpA9(Cpu180Context *activeCpu); // A9  SHFX       2-33
static void cp180OpAA(Cpu180Context *activeCpu); // AA  SHFR       2-33

static void cp180OpAC(Cpu180Context *activeCpu); // AC  ISOM       2-36
static void cp180OpAD(Cpu180Context *activeCpu); // AD  ISOB       2-36
static void cp180OpAE(Cpu180Context *activeCpu); // AE  INSB       2-36

static void cp180OpB0(Cpu180Context *activeCpu); // B0  CALLREL    2-125
static void cp180OpB1(Cpu180Context *activeCpu); // B1  KEYPOINT   2-133
static void cp180OpB2(Cpu180Context *activeCpu); // B2  MULXQ      2-21
static void cp180OpB3(Cpu180Context *activeCpu); // B3  ENTA       2-31
static void cp180OpB4(Cpu180Context *activeCpu); // B4  CMPXA      2-134
static void cp180OpB5(Cpu180Context *activeCpu); // B5  CALLSEG    2-122

static void cp180OpC0(Cpu180Context *activeCpu); // C0  EXECUTE,0  2-137
static void cp180OpC1(Cpu180Context *activeCpu); // C1  EXECUTE,1  2-137
static void cp180OpC2(Cpu180Context *activeCpu); // C2  EXECUTE,2  2-137
static void cp180OpC3(Cpu180Context *activeCpu); // C3  EXECUTE,3  2-137
static void cp180OpC4(Cpu180Context *activeCpu); // C4  EXECUTE,4  2-137
static void cp180OpC5(Cpu180Context *activeCpu); // C5  EXECUTE,5  2-137
static void cp180OpC6(Cpu180Context *activeCpu); // C6  EXECUTE,6  2-137
static void cp180OpC7(Cpu180Context *activeCpu); // C7  EXECUTE,7  2-137

static void cp180OpD0(Cpu180Context *activeCpu); // D0  LBYTS,1    2-11
static void cp180OpD1(Cpu180Context *activeCpu); // D1  LBYTS,2    2-11
static void cp180OpD2(Cpu180Context *activeCpu); // D2  LBYTS,3    2-11
static void cp180OpD3(Cpu180Context *activeCpu); // D3  LBYTS,4    2-11
static void cp180OpD4(Cpu180Context *activeCpu); // D4  LBYTS,5    2-11
static void cp180OpD5(Cpu180Context *activeCpu); // D5  LBYTS,6    2-11
static void cp180OpD6(Cpu180Context *activeCpu); // D6  LBYTS,7    2-11
static void cp180OpD7(Cpu180Context *activeCpu); // D7  LBYTS,8    2-11
static void cp180OpD8(Cpu180Context *activeCpu); // D8  SBYTS,1    2-11
static void cp180OpD9(Cpu180Context *activeCpu); // D9  SBYTS,2    2-11
static void cp180OpDA(Cpu180Context *activeCpu); // DA  SBYTS,3    2-11
static void cp180OpDB(Cpu180Context *activeCpu); // DB  SBYTS,4    2-11
static void cp180OpDC(Cpu180Context *activeCpu); // DC  SBYTS,5    2-11
static void cp180OpDD(Cpu180Context *activeCpu); // DD  SBYTS,6    2-11
static void cp180OpDE(Cpu180Context *activeCpu); // DE  SBYTS,7    2-11
static void cp180OpDF(Cpu180Context *activeCpu); // DF  SBYTS,8    2-11

static void cp180OpE4(Cpu180Context *activeCpu); // E4  SCLN       2-49
static void cp180OpE5(Cpu180Context *activeCpu); // E5  SCLR       2-49

static void cp180OpE9(Cpu180Context *activeCpu); // E9  CMPC       2-52

static void cp180OpEB(Cpu180Context *activeCpu); // EB  TRANB      2-54

static void cp180OpED(Cpu180Context *activeCpu); // ED  EDIT       2-55

static void cp180OpF3(Cpu180Context *activeCpu); // F3  SCNB       2-54

static void cp180OpF9(Cpu180Context *activeCpu); // F9  MOVI       2-62
static void cp180OpFA(Cpu180Context *activeCpu); // FA  CMPI       2-63
static void cp180OpFB(Cpu180Context *activeCpu); // FB  ADDI       2-64

static void cp180OpIv(Cpu180Context *activeCpu);
static void cp180OpLBYTS(Cpu180Context *activeCpu, u8 count);
static void cp180OpSBYTS(Cpu180Context *activeCpu, int count);

/*
**  ----------------
**  Public Variables
**  ----------------
*/
u64           cpu180FreeRunningCounter = 0;
Cpu180Context *cpus180;

/*
**  -----------------
**  Private Variables
**  -----------------
*/

/*
**  Opcode decode and dispatch table.
*/
static OpDispatch decodeCpu180Opcode[] =
    {
    cp180Op00, jk,   // 00
    cp180Op01, jk,   // 01
    cp180Op02, jk,   // 02
    cp180Op03, jk,   // 03
    cp180Op04, jk,   // 04
    cp180Op05, jk,   // 05
    cp180Op06, jk,   // 06
    cp180Op07, jk,   // 07
    cp180Op08, jk,   // 08
    cp180Op09, jk,   // 09
    cp180Op0A, jk,   // 0A
    cp180Op0B, jk,   // 0B
    cp180Op0C, jk,   // 0C
    cp180Op0D, jk,   // 0D
    cp180Op0E, jk,   // 0E
    cp180Op0F, jk,   // 0F

    cp180Op10, jk,   // 10
    cp180Op11, jk,   // 11
    cp180OpIv, jk,   // 12
    cp180OpIv, jk,   // 13
    cp180Op14, jk,   // 14
    cp180OpIv, jk,   // 15
    cp180Op16, jk,   // 16
    cp180Op17, jk,   // 17
    cp180Op18, jk,   // 18
    cp180Op19, jk,   // 19
    cp180Op1A, jk,   // 1A
    cp180Op1B, jk,   // 1B
    cp180Op1C, jk,   // 1C
    cp180OpIv, jk,   // 1D
    cp180Op1E, jk,   // 1E
    cp180Op1F, jk,   // 1F

    cp180Op20, jk,   // 20
    cp180Op21, jk,   // 21
    cp180Op22, jk,   // 22
    cp180Op23, jk,   // 23
    cp180Op24, jk,   // 24
    cp180Op25, jk,   // 25
    cp180Op26, jk,   // 26
    cp180Op27, jk,   // 27
    cp180Op28, jk,   // 28
    cp180Op29, jk,   // 29
    cp180Op2A, jk,   // 2A
    cp180OpIv, jk,   // 2B
    cp180Op2C, jk,   // 2C
    cp180Op2D, jk,   // 2D
    cp180Op2E, jk,   // 2E
    cp180Op2F, jk,   // 2F

    cp180Op30, jk,   // 30
    cp180Op31, jk,   // 31
    cp180Op32, jk,   // 32
    cp180Op33, jk,   // 33
    cp180Op34, jk,   // 34
    cp180Op35, jk,   // 35
    cp180Op36, jk,   // 36
    cp180Op37, jk,   // 37
    cp180OpIv, jk,   // 38
    cp180Op39, jk,   // 39
    cp180Op3A, jk,   // 3A
    cp180Op3B, jk,   // 3B
    cp180Op3C, jk,   // 3C
    cp180Op3D, jk,   // 3D
    cp180Op3E, jk,   // 3E
    cp180Op3F, jk,   // 3F

    cp180Op40, jkiD, // 40
    cp180Op41, jkiD, // 41
    cp180Op42, jkiD, // 42
    cp180Op43, jkiD, // 43
    cp180Op44, jkiD, // 44
    cp180Op45, jkiD, // 45
    cp180OpIv, jkiD, // 46
    cp180OpIv, jkiD, // 47
    cp180Op48, jkiD, // 48
    cp180Op49, jkiD, // 49
    cp180Op4A, jkiD, // 4A
    cp180Op4B, jkiD, // 4B
    cp180Op4C, jkiD, // 4C
    cp180Op4D, jkiD, // 4D
    cp180OpIv, jkiD, // 4E
    cp180OpIv, jkiD, // 4F

    cp180Op50, jkiD, // 50
    cp180Op51, jkiD, // 51
    cp180Op52, jkiD, // 52
    cp180Op53, jkiD, // 53
    cp180Op54, jkiD, // 54
    cp180Op55, jkiD, // 55
    cp180Op56, jkiD, // 56
    cp180Op57, jkiD, // 57
    cp180Op58, jkiD, // 58
    cp180Op59, jkiD, // 59
    cp180Op5A, jkiD, // 5A
    cp180Op5B, jkiD, // 5B
    cp180Op5C, jkiD, // 5C
    cp180Op5D, jkiD, // 5D
    cp180Op5E, jkiD, // 5E
    cp180OpIv, jkiD, // 5F

    cp180OpIv, jkiD, // 60
    cp180OpIv, jkiD, // 61
    cp180OpIv, jkiD, // 62
    cp180OpIv, jkiD, // 63
    cp180OpIv, jkiD, // 64
    cp180OpIv, jkiD, // 65
    cp180OpIv, jkiD, // 66
    cp180OpIv, jkiD, // 67
    cp180OpIv, jkiD, // 68
    cp180OpIv, jkiD, // 69
    cp180OpIv, jkiD, // 6A
    cp180OpIv, jkiD, // 6B
    cp180OpIv, jkiD, // 6C
    cp180OpIv, jkiD, // 6D
    cp180OpIv, jkiD, // 6E
    cp180OpIv, jkiD, // 6F

    cp180Op70, jk,   // 70
    cp180Op71, jk,   // 71
    cp180Op72, jk,   // 72
    cp180Op73, jk,   // 73
    cp180Op74, jk,   // 74
    cp180Op75, jk,   // 75
    cp180Op76, jk,   // 76
    cp180Op77, jk,   // 77
    cp180OpIv, jk,   // 78
    cp180OpIv, jk,   // 79
    cp180OpIv, jk,   // 7A
    cp180OpIv, jk,   // 7B
    cp180OpIv, jk,   // 7C
    cp180OpIv, jk,   // 7D
    cp180OpIv, jk,   // 7E
    cp180OpIv, jk,   // 7F

    cp180Op80, jkQ,  // 80
    cp180Op81, jkQ,  // 81
    cp180Op82, jkQ,  // 82
    cp180Op83, jkQ,  // 83
    cp180Op84, jkQ,  // 84
    cp180Op85, jkQ,  // 85
    cp180Op86, jkQ,  // 86
    cp180Op87, jkQ,  // 87
    cp180Op88, jkQ,  // 88
    cp180Op89, jkQ,  // 89
    cp180Op8A, jkQ,  // 8A
    cp180Op8B, jkQ,  // 8B
    cp180Op8C, jkQ,  // 8C
    cp180Op8D, jkQ,  // 8D
    cp180Op8E, jkQ,  // 8E
    cp180Op8F, jkQ,  // 8F

    cp180Op90, jkQ,  // 90
    cp180Op91, jkQ,  // 91
    cp180Op92, jkQ,  // 92
    cp180Op93, jkQ,  // 93
    cp180Op94, jkQ,  // 94
    cp180Op95, jkQ,  // 95
    cp180Op96, jkQ,  // 96
    cp180Op97, jkQ,  // 97
    cp180Op98, jkQ,  // 98
    cp180Op99, jkQ,  // 99
    cp180Op9A, jkQ,  // 9A
    cp180Op9B, jkQ,  // 9B
    cp180Op9C, jkQ,  // 9C
    cp180Op9D, jkQ,  // 9D
    cp180Op9E, jkQ,  // 9E
    cp180Op9F, jkQ,  // 9F

    cp180OpA0, jkiD, // A0
    cp180OpA1, jkiD, // A1
    cp180OpA2, jkiD, // A2
    cp180OpA3, jkiD, // A3
    cp180OpA4, jkiD, // A4
    cp180OpA5, jkiD, // A5
    cp180OpIv, jkiD, // A6
    cp180OpA7, jkiD, // A7
    cp180OpA8, jkiD, // A8
    cp180OpA9, jkiD, // A9
    cp180OpAA, jkiD, // AA
    cp180OpIv, jkiD, // AB
    cp180OpAC, jkiD, // AC
    cp180OpAD, jkiD, // AD
    cp180OpAE, jkiD, // AE
    cp180OpIv, jkiD, // AF

    cp180OpB0, jkQ,  // B0
    cp180OpB1, jkQ,  // B1
    cp180OpB2, jkQ,  // B2
    cp180OpB3, jkQ,  // B3
    cp180OpB4, jkQ,  // B4
    cp180OpB5, jkQ,  // B5
    cp180OpIv, jkQ,  // B6
    cp180OpIv, jkQ,  // B7
    cp180OpIv, jkQ,  // B8
    cp180OpIv, jkQ,  // B9
    cp180OpIv, jkQ,  // BA
    cp180OpIv, jkQ,  // BB
    cp180OpIv, jkQ,  // BC
    cp180OpIv, jkQ,  // BD
    cp180OpIv, jkQ,  // BE
    cp180OpIv, jkQ,  // BF

    cp180OpC0, jkiD, // C0
    cp180OpC1, jkiD, // C1
    cp180OpC2, jkiD, // C2
    cp180OpC3, jkiD, // C3
    cp180OpC4, jkiD, // C4
    cp180OpC5, jkiD, // C5
    cp180OpC6, jkiD, // C6
    cp180OpC7, jkiD, // C7
    cp180OpIv, jkiD, // C8
    cp180OpIv, jkiD, // C9
    cp180OpIv, jkiD, // CA
    cp180OpIv, jkiD, // CB
    cp180OpIv, jkiD, // CC
    cp180OpIv, jkiD, // CD
    cp180OpIv, jkiD, // CE
    cp180OpIv, jkiD, // CF

    cp180OpD0, jkiD, // D0
    cp180OpD1, jkiD, // D1
    cp180OpD2, jkiD, // D2
    cp180OpD3, jkiD, // D3
    cp180OpD4, jkiD, // D4
    cp180OpD5, jkiD, // D5
    cp180OpD6, jkiD, // D6
    cp180OpD7, jkiD, // D7
    cp180OpD8, jkiD, // D8
    cp180OpD9, jkiD, // D9
    cp180OpDA, jkiD, // DA
    cp180OpDB, jkiD, // DB
    cp180OpDC, jkiD, // DC
    cp180OpDD, jkiD, // DD
    cp180OpDE, jkiD, // DE
    cp180OpDF, jkiD, // DF

    cp180OpIv, jkiD, // E0
    cp180OpIv, jkiD, // E1
    cp180OpIv, jkiD, // E2
    cp180OpIv, jkiD, // E3
    cp180OpE4, jkiD, // E4
    cp180OpE5, jkiD, // E5
    cp180OpIv, jkiD, // E6
    cp180OpIv, jkiD, // E7
    cp180OpIv, jkiD, // E8
    cp180OpE9, jkiD, // E9
    cp180OpIv, jkiD, // EA
    cp180OpEB, jkiD, // EB
    cp180OpIv, jkiD, // EC
    cp180OpED, jkiD, // ED
    cp180OpIv, jkiD, // EE
    cp180OpIv, jkiD, // EF

    cp180OpIv, jkiD, // F0
    cp180OpIv, jkiD, // F1
    cp180OpIv, jkiD, // F2
    cp180OpF3, jkiD, // F3
    cp180OpIv, jkiD, // F4
    cp180OpIv, jkiD, // F5
    cp180OpIv, jkiD, // F6
    cp180OpIv, jkiD, // F7
    cp180OpIv, jkiD, // F8
    cp180OpF9, jkiD, // F9
    cp180OpFA, jkiD, // FA
    cp180OpFB, jkiD, // FB
    cp180OpIv, jkiD, // FC
    cp180OpIv, jkiD, // FD
    cp180OpIv, jkiD, // FE
    cp180OpIv, jkiD  // FF
    };

/*
**  Condition action definitions for monitor conditions, indexed by MonitorCondition
*/
static ConditionActionDefn mcrDefns [] =
    {
    0x8000, FALSE, TRUE,  Exch, Trap, Exch, Halt,  Halt,  /* MCR48 Detected uncorrectable error     */
    0x4000, FALSE, TRUE,  Exch, Trap, Exch, Halt,  Halt,  /* MCR49 Not assigned                     */
    0x2000, TRUE,  FALSE, Exch, Trap, Exch, Stack, Stack, /* MCR50 Short warning                    */
    0x1000, FALSE, TRUE,  Exch, Trap, Exch, Halt,  Halt,  /* MCR51 Instruction specfication error   */
    0x0800, FALSE, TRUE,  Exch, Trap, Exch, Halt,  Halt,  /* MCR52 Address specification error      */
    0x0400, TRUE,  FALSE, Exch, Trap, Exch, Stack, Stack, /* MCR53 CYBER 170 state exchange request */
    0x0200, FALSE, TRUE,  Exch, Trap, Exch, Halt,  Halt,  /* MCR54 Access violation                 */
    0x0100, FALSE, TRUE,  Exch, Trap, Exch, Halt,  Halt,  /* MCR55 Environment specification error  */
    0x0080, TRUE,  FALSE, Exch, Trap, Exch, Stack, Stack, /* MCR56 External interrupt               */
    0x0040, FALSE, TRUE,  Exch, Trap, Exch, Halt,  Halt,  /* MCR57 Page table search without find   */
    0x0020, TRUE,  FALSE, Rni,  Rni,  Rni,  Rni,   Rni,   /* MCR58 System call (status bit)         */
    0x0010, TRUE,  FALSE, Exch, Trap, Exch, Stack, Stack, /* MCR59 System interval timer            */
    0x0008, FALSE, TRUE,  Exch, Trap, Exch, Halt,  Halt,  /* MCR60 Invalid segment / Ring number 0  */
    0x0004, FALSE, TRUE,  Exch, Trap, Exch, Halt,  Halt,  /* MCR61 Outward call / Inward return     */
    0x0002, TRUE,  FALSE, Exch, Trap, Exch, Stack, Stack, /* MCR62 Soft error                       */
    0x0001, FALSE, TRUE,  Rni,  Rni,  Rni,  Rni,   Rni    /* MCR63 Trap exception (status bit)      */
    };

/*
**  Condition action definitions for user conditions, indexed by UserCondition
*/
static ConditionActionDefn ucrDefns [] =
    {
    0x8000, FALSE, TRUE,  Trap, Trap, Exch,  Halt,  Rni,   /* UCR48 Privileged instruction fault     */
    0x4000, FALSE, TRUE,  Trap, Trap, Exch,  Halt,  Rni,   /* UCR49 Unimplemented instruction        */
    0x2000, TRUE,  FALSE, Trap, Trap, Stack, Stack, Rni,   /* UCR50 Free flag                        */
    0x1000, FALSE, TRUE,  Trap, Trap, Stack, Stack, Rni,   /* UCR51 Process interval timer           */
    0x0800, FALSE, TRUE,  Trap, Trap, Exch,  Halt,  Rni,   /* UCR52 Inter-ring pop                   */
    0x0400, FALSE, TRUE,  Trap, Trap, Exch,  Halt,  Rni,   /* UCR53 Critical frame flag              */
    0x0200, TRUE,  FALSE, Trap, Trap, Stack, Stack, Rni,   /* UCR54 Reserved                         */
    0x0100, FALSE, FALSE, Trap, Trap, Stack, Stack, Stack, /* UCR55 Divide fault                     */
    0x0080, FALSE, TRUE,  Trap, Trap, Stack, Stack, Stack, /* UCR56 Debug                            */
    0x0040, FALSE, TRUE,  Trap, Trap, Stack, Stack, Stack, /* UCR57 Arithmetic overflow              */
    0x0020, TRUE,  TRUE,  Trap, Trap, Stack, Stack, Stack, /* UCR58 Exponent overflow                */
    0x0010, TRUE,  TRUE,  Trap, Trap, Stack, Stack, Stack, /* UCR59 Exponent underflow               */
    0x0008, TRUE,  TRUE,  Trap, Trap, Stack, Stack, Stack, /* UCR60 FP loss of significance          */
    0x0004, FALSE, TRUE,  Trap, Trap, Stack, Stack, Stack, /* UCR61 FP indefinite                    */
    0x0002, FALSE, TRUE,  Trap, Trap, Stack, Stack, Stack, /* UCR62 Arithmetic loss of significance  */
    0x0001, TRUE,  TRUE,  Trap, Trap, Stack, Stack, Stack  /* UCR63 Invalid BDP data                 */
    };

/*
**  Bit masks used in bit field instructions
*/
static u64 bitMasks[] =
    {
    0x0000000000000001,
    0x0000000000000003,
    0x0000000000000007,
    0x000000000000000f,
    0x000000000000001f,
    0x000000000000003f,
    0x000000000000007f,
    0x00000000000000ff,
    0x00000000000001ff,
    0x00000000000003ff,
    0x00000000000007ff,
    0x0000000000000fff,
    0x0000000000001fff,
    0x0000000000003fff,
    0x0000000000007fff,
    0x000000000000ffff,
    0x000000000001ffff,
    0x000000000003ffff,
    0x000000000007ffff,
    0x00000000000fffff,
    0x00000000001fffff,
    0x00000000003fffff,
    0x00000000007fffff,
    0x0000000000ffffff,
    0x0000000001ffffff,
    0x0000000003ffffff,
    0x0000000007ffffff,
    0x000000000fffffff,
    0x000000001fffffff,
    0x000000003fffffff,
    0x000000007fffffff,
    0x00000000ffffffff,
    0x00000001ffffffff,
    0x00000003ffffffff,
    0x00000007ffffffff,
    0x0000000fffffffff,
    0x0000001fffffffff,
    0x0000003fffffffff,
    0x0000007fffffffff,
    0x000000ffffffffff,
    0x000001ffffffffff,
    0x000003ffffffffff,
    0x000007ffffffffff,
    0x00000fffffffffff,
    0x00001fffffffffff,
    0x00003fffffffffff,
    0x00007fffffffffff,
    0x0000ffffffffffff,
    0x0001ffffffffffff,
    0x0003ffffffffffff,
    0x0007ffffffffffff,
    0x000fffffffffffff,
    0x001fffffffffffff,
    0x003fffffffffffff,
    0x007fffffffffffff,
    0x00ffffffffffffff,
    0x01ffffffffffffff,
    0x03ffffffffffffff,
    0x07ffffffffffffff,
    0x0fffffffffffffff,
    0x1fffffffffffffff,
    0x3fffffffffffffff,
    0x7fffffffffffffff,
    0xffffffffffffffff
    };

/*
**  Masks used in testing individual bits in 16-bit values
*/
static u16 bitSelectors[16] =
    {
    0x8000,
    0x4000,
    0x2000,
    0x1000,
    0x0800,
    0x0400,
    0x0200,
    0x0100,
    0x0080,
    0x0040,
    0x0020,
    0x0010,
    0x0008,
    0x0004,
    0x0002,
    0x0001
    };

/*
 **--------------------------------------------------------------------------
 **
 **  Public Functions
 **
 **--------------------------------------------------------------------------
 */

/*--------------------------------------------------------------------------
**  Purpose:        Check monitor and user condition registers for indications
**
**                  Ordinarily, this is called after an exchange or return
**                  operation to check for previously stacked conditions.
**
**  Parameters:     Name        Description.
**                  ctx         pointer to CPU context
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cpu180CheckConditions(Cpu180Context *ctx)
    {
    ctx->pendingAction = Rni;
    cpu180CheckMonitorConditions(ctx);
    cpu180CheckUserConditions(ctx);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Initialise CYBER 180 CPU.
**
**  Parameters:     Name        Description.
**                  model       CPU model string
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cpu180Init(char *model)
    {
    int           cpuNum;
    Cpu180Context *activeCpu;

    /*
    **  Initialize CYBER 180 CPU(s)
    */
    cpus180 = (Cpu180Context *)calloc(cpuCount, sizeof(Cpu180Context));
    if (cpus180 == NULL)
        {
        fputs("(cpu    ) Failed to allocate memory for CYBER 180 CPU contexts\n", stderr);
        exit(1);
        }
    for (cpuNum = 0; cpuNum < cpuCount; cpuNum++)
        {
        activeCpu                       = &cpus180[cpuNum];
        activeCpu->id                   = cpuNum;
        activeCpu->pendingAction        = Rni;
        activeCpu->isStopped            = TRUE;
        activeCpu->isMonitorMode        = TRUE;
        cpu180UpdatePageSize(activeCpu);
        }

    /*
    **  Print a friendly message.
    */
    fputs("(cpu    ) CYBER 180 CPU state initialised\n", stdout);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Load the 170 state exchange package referenced by a
**                  specified real memory word address.
**
**  Parameters:     Name        Description.
**                  ctx180      pointer to CYBER 180 CPU context
**                  xpa         word address of exchange package
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
void cpu180Load170Xp(Cpu180Context *ctx180, u32 xpa)
    {
    Cpu170Context *ctx170;
    int           i;
    u32           wordAddr;

    cpu180Load180Xp(ctx180, xpa);
    ctx180->regP170       = cpMem[xpa];
    ctx170                = &cpus170[ctx180->id];
    ctx170->regRaCm       = ctx180->regA[3] & Mask32;
    ctx170->exitMode      = (ctx180->regA[3] >> 20) & 0xfff000;
    ctx170->regFlCm       = ctx180->regA[4] & Mask32;
    ctx170->isMonitorMode = (ctx180->regA[4] >> 32) & 1;
    ctx170->regMa         = ctx180->regA[5] & Mask32;
    ctx170->regRaEcs      = (ctx180->regA[6] & 0xffffffc0) >> 2;
    ctx170->regFlEcs      = (ctx180->regA[7] & 0xffffffc0) >> 2;
    for (i = 0; i < 8; i++)
        {
        ctx170->regA[i] = ctx180->regA[i + 8] & Mask18;
        }
    ctx170->regB[0] = 0;
    for (i = 1; i < 8; i++)
        {
        ctx170->regB[i] = ctx180->regX[i] & Mask18;
        }
    for (i = 0; i < 8; i++)
        {
        ctx170->regX[i] = ctx180->regX[i + 8] & Mask60;
        }
    wordAddr          = (ctx180->regP & Mask32) >> 3;
    ctx170->regP      = wordAddr - ctx170->regRaCm;
    ctx170->opOffset  = 60 - (((ctx180->regP & Mask3) >> 1) * 15);
    ctx170->opWord    = cpMem[wordAddr];
    ctx170->isStopped = FALSE;
    if ((features & HasInstructionStack) != 0)
        {
        /*
        **  Void the instruction stack.
        */
        cpuVoidIwStack(ctx170, ~0);
        }
#if CcDebug == 1
    traceExchange(ctx170, xpa << 3, NULL);
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Load the 180 state exchange package referenced by a
**                  specified real memory word address.
**
**  Parameters:     Name        Description.
**                  ctx         pointer to CPU context
**                  xpa         word address of exchange package
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
void cpu180Load180Xp(Cpu180Context *ctx, u32 xpa)
    {
    int i;
    u64 word;
#if CcDebug == 1
    u32 xpab = xpa << 3;
#endif

    word           = cpMem[xpa++];
    ctx->key       = (word >> 48) & Mask6;
    ctx->regP      = word & Mask48;
    word           = cpMem[xpa++];
    ctx->regA[0]   = word & Mask48;
    ctx->regVmid   = (u8)(word >> 56) & Mask4;
    ctx->regUvmid  = (u8)(word >> 48) & Mask4;
    word           = cpMem[xpa++];
    ctx->regA[1]   = word & Mask48;
    ctx->regFlags  = word >> 48;
    word           = cpMem[xpa++];
    ctx->regA[2]   = word & Mask48;
    ctx->regUmr    = (word >> 48) | 0xfc00;
    word           = cpMem[xpa++];
    ctx->regA[3]   = word & Mask48;
    ctx->regMmr    = word >> 48;
    word           = cpMem[xpa++];
    ctx->regA[4]   = word & Mask48;
    ctx->regUcr    = (word >> 48) & ~ctx->regUmr;
    word           = cpMem[xpa++];
    ctx->regA[5]   = word & Mask48;
    ctx->regMcr    = (word >> 48) & ~(ctx->regMmr | 0x0021); // clear masked bits and status bits
    word           = cpMem[xpa++];
    ctx->regA[6]   = word & Mask48;
    ctx->regLpid   = (word >> 48) & Mask8;
    word           = cpMem[xpa++];
    ctx->regA[7]   = word & Mask48;
    ctx->regKmr    = (word >> 48) & Mask8;
    ctx->regA[8]   = cpMem[xpa++] & Mask48;
    ctx->regA[9]   = cpMem[xpa++] & Mask48;
    word           = cpMem[xpa++];
    ctx->regA[10]  = word & Mask48;
    ctx->regPit    = (word >> 32) & 0xffff0000;
    word           = cpMem[xpa++];
    ctx->regA[11]  = word & Mask48;
    ctx->regPit   |= word >> 48;
    word           = cpMem[xpa++];
    ctx->regA[12]  = word & Mask48;
    ctx->regBc     = (word >> 32) & 0xffff0000;
    word           = cpMem[xpa++];
    ctx->regA[13]  = word & Mask48;
    ctx->regBc    |= word >> 48;
    word           = cpMem[xpa++];
    ctx->regA[14]  = word & Mask48;
    ctx->regMdf    = word >> 48;
    word           = cpMem[xpa++];
    ctx->regA[15]  = word & Mask48;
    ctx->regStl    = (word >> 48) & Mask12;

    for (i = 0; i < 16; i++)
        {
        ctx->regX[i] = cpMem[xpa++];
        }

    ctx->regMdw    = cpMem[xpa++];

    word           = cpMem[xpa++];
    ctx->regUtp    = word & Mask48;
    ctx->regSta    = (word >> 32) & 0xffff0000;
    word           = cpMem[xpa++];
    ctx->regTp     = word & Mask48;
    ctx->regSta   |= word >> 48;

    word           = cpMem[xpa++];
    ctx->regDlp    = word & Mask48;
    ctx->regDi     = (word >> 58) & Mask6;
    ctx->regDm     = (word >> 48) & Mask7;

    word           = cpMem[xpa++];
    ctx->regLrn    = (word >> 48) & Mask4;
    ctx->regTos[0] = word & Mask48;
    for (i = 1; i < 15; i++)
        {
        ctx->regTos[i] = cpMem[xpa++] & Mask48;
        }
#if CcDebug == 1
    traceExchange180(ctx, xpab, "Load");
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Read 64-bit CPU memory from PP and verify that address is
**                  within limits.
**
**  Parameters:     Name        Description.
**                  address     Absolute CM address to read.
**                  data        Pointer to 64-bit word which gets the data.
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
void cpu180PpReadMem(u32 address, CpWord *data)
    {
    if ((features & HasNoCmWrap) != 0)
        {
        if (address < cpuMaxMemory)
            {
            *data = cpMem[address];
            }
        else
            {
            *data = (~((CpWord)0));
            }
        }
    else
        {
        address %= cpuMaxMemory;
        *data    = cpMem[address];
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Write 64-bit CPU memory from PP and verify that address is
**                  within limits.
**
**  Parameters:     Name        Description.
**                  address     Absolute CM address
**                  data        64-bit word which holds the data to be written.
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
void cpu180PpWriteMem(u32 address, CpWord data)
    {
    if ((features & HasNoCmWrap) != 0)
        {
        if (address < cpuMaxMemory)
            {
            cpMem[address] = data;
            }
        }
    else
        {
        address       %= cpuMaxMemory;
        cpMem[address] = data;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Translate a PVA (process virtual address) to an RMA (real
**                  memory address).
**
**  Parameters:     Name        Description.
**                  ctx         pointer to C180 CPU context
**                  pva         PVA to be translated
**                  access      mode of memory access (execute, read, write)
**                  rma         pointer to resulting RMA
**                  cond        monitor condition, if translation fails
**
**  Returns:        TRUE if translation succeeded
**
**------------------------------------------------------------------------*/
bool cpu180PvaToRma(Cpu180Context *ctx, u64 pva, Cpu180AccessMode access, u32 *rma, MonitorCondition *cond)
    {
    u32 byteNum;
    u16 asid;
    u8  n;
    u32 pti;
    u8  pm;
    u64 pte;
    u8  ring;
    u64 sde;
    u16 segNum;

#if CcDebug == 1
    tracePva(ctx, pva);
#endif

    ring    = RingOf(pva);
    segNum  = SegmentOf(pva);
    byteNum = pva & Mask32;

    if ((byteNum & 0x80000000) != 0)
        {
        *cond = MCR52;
        ctx->regUtp = pva;
        return FALSE;
        }

    /*
    **  Use the segment number in the PVA as an index into the segment
    **  descriptor table to produce an SDE (segment descriptor table entry).
    **  The SDE contains privilege and protection information as well as
    **  an ASID (active segment identifier). The ASID replaces the segment
    **  number in the PVA to produce an SVA. It is also used in producing
    **  a hash code that selects the starting point in the system page
    **  table to search for a matching page table entry.
    **/
    if (segNum > ctx->regStl)
        {
        *cond = MCR60;
        ctx->regUtp = pva;
        return FALSE;
        }

    sde = cpMem[(ctx->regSta >> 3) + segNum];

#if CcDebug == 1
    traceSde(ctx, segNum, sde);
#endif

    if ((sde >> 63) == 0)
        {
        *cond = MCR60;
        ctx->regUtp = pva;
        return FALSE;
        }

    if (cpu180ValidateAccess(ctx, sde, ring, access) == FALSE)
        {
        *cond = MCR54;
        ctx->regUtp = pva;
        return FALSE;
        }

    asid = (sde >> 32) & Mask16;

    if (cpu180FindPte(ctx, asid, byteNum, FALSE, &pti, &n))
        {
        pte = cpMem[pti];
        if ((access & AccessModeWrite) != 0)
            {
            cpMem[pti] |= (u64)3 << 60; // set page used and modified bits
            }
        else
            {
            cpMem[pti] |= (u64)2 << 60; // set page used bit only
            }
        //
        //  See MIGDS 3-15 for diagram of RMA calculation
        //
        *rma = ((pte & Mask22) << 9)
            | ((byteNum & 0xfe00) & ((u16)(~ctx->regPsm & Mask7) << 9))
            | (byteNum & Mask9);

#if CcDebug == 1
        traceRma(ctx, *rma);
#endif

        return TRUE;
        }
    /*
    **  Page not found, set page fault
    */
    *cond = MCR57;
    ctx->regUtp = pva;

    return FALSE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Set a monitor condition
**
**  Parameters:     Name        Description.
**                  ctx         pointer to CPU context
**                  cond        monitor condition ordinal
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cpu180SetMonitorCondition(Cpu180Context *ctx, MonitorCondition cond)
    {
    ConditionAction     action;
    ConditionActionDefn *defn;

    defn         = &mcrDefns[cond];
    ctx->regMcr |= defn->bitMask;

    if ((ctx->regMmr & defn->bitMask) == 0)
        {
        action = defn->whenNoMask;
        }
    else if ((ctx->regFlags & 3) == 2) // trap enabled
        {
        action = ctx->isMonitorMode ? defn->whenMaskTrapMonitor : defn->whenMaskTrapJob;
        }
    else
        {
        action = ctx->isMonitorMode ? defn->whenMaskNoTrapMonitor : defn->whenMaskNoTrapJob;
        }
    if (action > ctx->pendingAction)
        {
        ctx->pendingAction = action;
        if (action > Stack && defn->isThis)
            {
            ctx->nextP = ctx->regP;
            }
        }
#if CcDebug == 1
    traceMonitorCondition(ctx, cond);
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Set a user condition
**
**  Parameters:     Name        Description.
**                  ctx         pointer to CPU context
**                  cond        user condition ordinal
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cpu180SetUserCondition(Cpu180Context *ctx, UserCondition cond)
    {
    ConditionAction     action;
    ConditionActionDefn *defn;

    defn         = &ucrDefns[cond];
    ctx->regUcr |= defn->bitMask;

    if ((ctx->regUmr & defn->bitMask) == 0)
        {
        action = defn->whenNoMask;
        }
    else if ((ctx->regFlags & 3) == 2) // trap enabled
        {
        action = ctx->isMonitorMode ? defn->whenMaskTrapMonitor : defn->whenMaskTrapJob;
        }
    else
        {
        action = ctx->isMonitorMode ? defn->whenMaskNoTrapMonitor : defn->whenMaskNoTrapJob;
        }
    if (action > ctx->pendingAction)
        {
        ctx->pendingAction = action;
        if (action > Stack && defn->isThis)
            {
            ctx->nextP = ctx->regP;
            }
        }
#if CcDebug == 1
    traceUserCondition(ctx, cond);
#endif
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
void cpu180Step(Cpu180Context *activeCpu)
    {
    OpDispatch *odp;
    u8         length;
    u16        parcel;
#if CcDebug == 1
    u64        oldRegP;
#endif

    if (activeCpu->pendingAction != Rni)
        {
        switch (activeCpu->pendingAction)
            {
        case Trap:
            activeCpu->pendingAction = Rni;
            cpu180Trap(activeCpu);
            if (activeCpu->pendingAction > Stack)
                {
                return;
                }
            break;
        case Exch:
            activeCpu->pendingAction = Rni;
            cpu180Exchange(activeCpu);
            if (activeCpu->pendingAction > Stack)
                {
                return;
                }
            break;
        case Halt:
            activeCpu->isStopped = TRUE;
            break;
        case Stack:
        default:
            break;
            }
        }

    if (activeCpu->isStopped)
        {
        return;
        }

    /*
    **  Execute the next instruction.
    */
    activeCpu->pendingAction = Rni;
    if (cpu180GetParcel(activeCpu, activeCpu->regP, &parcel))
        {
        activeCpu->opCode = parcel >> 8;
        activeCpu->opJ    = (parcel >> 4) & Mask4;
        activeCpu->opK    = parcel & Mask4;
        odp               = &decodeCpu180Opcode[activeCpu->opCode];
        switch (odp->format)
            {
        case jk:
            length = 2;
            break;
        case jkiD:
            if (cpu180GetParcel(activeCpu, activeCpu->regP + 2, &parcel))
                {
                activeCpu->opI = parcel >> 12;
                activeCpu->opD = parcel & Mask12;
                length = 4;
                }
            else
                {
                return;
                }
            break;
        case jkQ:
            if (cpu180GetParcel(activeCpu, (activeCpu->regP & Mask48) + 2, &parcel))
                {
                activeCpu->opQ = parcel;
                length = 4;
                }
            else
                {
                return;
                }
            break;
        default:
            logDtError(LogErrorLocation, "Unrecognized CYBER 180 instruction format: %d", odp->format);
            exit(1);
            }
#if CcDebug == 1
        oldRegP = activeCpu->regP;
#endif
        activeCpu->nextKey = activeCpu->key;
        activeCpu->nextP   = activeCpu->regP + length;
        odp->execute(activeCpu);
        activeCpu->key     = activeCpu->nextKey;
        activeCpu->regP    = activeCpu->nextP;

#if CcDebug == 1
        traceCpu180(activeCpu, oldRegP, activeCpu->opCode, activeCpu->opI, activeCpu->opJ, activeCpu->opK, activeCpu->opD, activeCpu->opQ);
#endif
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Store the 170 state exchange package into memory
**                  referenced by a specified real memory word address.
**
**  Parameters:     Name        Description.
**                  ctx180      pointer to CYBER 180 CPU context
**                  xpa         word address of exchange package
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
void cpu180Store170Xp(Cpu180Context *ctx180, u32 xpa)
    {
    Cpu170Context *ctx170;
    int           i;
    u64           ring;
    u64           word;

    ctx170          = &cpus170[ctx180->id];
    ctx180->regP    = (ctx180->regP170 & ~(u64)Mask32)
                      | ((ctx170->regRaCm + ctx170->regP) << 3)
                      | (((4 - (ctx170->opOffset / 15)) & 3) << 1);
    ring            = ctx180->regP & RingMask;
    ctx180->regA[3] = ring | ((u64)ctx170->exitMode << 20) | ctx170->regRaCm;
    ctx180->regA[4] = ring | (ctx170->isMonitorMode ? (u64)1 << 32 : 0) | ctx170->regFlCm;
    ctx180->regA[5] = ring | (ctx170->isStopped ? (u64)1 << 32 : 0) | ctx170->regMa;
    ctx180->regA[6] = ring | (ctx170->regRaEcs << 2);
    ctx180->regA[7] = ring | (ctx170->regFlEcs << 2);
    for (i = 0; i < 8; i++)
        {
        ctx180->regA[i + 8] = ring | ctx170->regA[i];
        }
    for (i = 1; i < 8; i++)
        {
        ctx180->regX[i] = ctx170->regB[i];
        }
    for (i = 0; i < 8; i++)
        {
        word = ctx170->regX[i];
        if ((word & 0x0800000000000000) != 0)
            {
            word |= 0xf000000000000000;
            }
        ctx180->regX[i + 8] = word;
        }
    cpu180Store180Xp(ctx180, xpa);

#if CcDebug == 1
    traceExchange(ctx170, xpa << 3, NULL);
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Update system and process interval timers.
**
**  Parameters:     Name        Description.
**                  delta       number of usecs by which decrement timers
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cpu180UpdateIntervalTimers(u64 delta)
    {
    Cpu180Context *ctx;
    int           i;
    u32           oldIt;

    for (i = 0; i < cpuCount; i++)
        {
        ctx = &cpus180[i];
        oldIt = ctx->regSit;
        ctx->regSit -= (u32)delta;
        if (ctx->regSit > oldIt || ctx->regSit == 0)
            {
            cpu180SetMonitorCondition(ctx, MCR59);
            ctx->regSit = 0xffffffff;
            }
        oldIt = ctx->regPit;
        ctx->regPit -= (u32)delta;
        if (ctx->regPit > oldIt || ctx->regPit == 0)
            {
            cpu180SetUserCondition(ctx, UCR51);
            ctx->regPit = 0xffffffff;
            }
        }
    cpu180FreeRunningCounter += delta;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Update elements related to page size.
**
**  Parameters:     Name        Description.
**                  ctx         pointer to CPU context to be updated
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cpu180UpdatePageSize(Cpu180Context *ctx)
    {
    u8 mask;

    mask              = ctx->regPsm;
    ctx->pageNumShift = 9;
    while ((mask & 1) == 0 && ctx->pageNumShift < 16)
        {
        ctx->pageNumShift += 1;
        mask             >>= 1;
        }
    ctx->byteNumMask    = ((~(u32)ctx->regPsm) << 9) | (u32)0x1ff;
    ctx->pageLengthMask = ((u32)ctx->regPtl << 12) | (u32)0xfff;
    ctx->pageOffsetMask = (((u16)~ctx->regPsm) << 9) | (u16)0x1ff;
    ctx->spidShift      = ctx->pageNumShift - 9;

#if CcDebug == 1
    traceVmRegisters(ctx);
#endif
    }

/*
 **--------------------------------------------------------------------------
 **
 **  Private Functions
 **
 **--------------------------------------------------------------------------
 */

/*--------------------------------------------------------------------------
**  Purpose:        Add two 32-bit integer quantities and detect overflow
**
**  Parameters:     Name        Description.
**                  ctx         pointer to CPU context
**                  augend      the augend
**                  addend      the addend
**                  sum         (out) pointer to sum
**
**  Returns:        TRUE if arithmetic overflow not detected.
**                  FALSE if overflow detected and user condition register
**                  bit is set.
**
**------------------------------------------------------------------------*/
static bool cpu180AddInt32(Cpu180Context *ctx, u32 augend, u32 addend, u32 *sum)
    {
    *sum = augend + addend;
    if ((i32)(augend ^ addend) >= 0 && (i32)(*sum ^ augend) < 0)
        {
        cpu180SetUserCondition(ctx, UCR57);
        return FALSE;
        }

    return TRUE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Add two 64-bit integer quantities and detect overflow
**
**  Parameters:     Name        Description.
**                  ctx         pointer to CPU context
**                  augend      the augend
**                  addend      the addend
**                  sum         (out) pointer to sum
**
**  Returns:        TRUE if arithmetic overflow not detected.
**                  FALSE if overflow detected and user condition register
**                  bit is set.
**
**------------------------------------------------------------------------*/
static bool cpu180AddInt64(Cpu180Context *ctx, u64 augend, u64 addend, u64 *sum)
    {
    *sum = augend + addend;
    if ((i64)(augend ^ addend) >= 0 && (i64)(*sum ^ augend) < 0)
        {
        cpu180SetUserCondition(ctx, UCR57);
        return FALSE;
        }

    return TRUE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Check monitor condition register for indications
**
**                  Ordinarily, this is called after an exchange or return
**                  operation to check for previously stacked conditions.
**
**  Parameters:     Name        Description.
**                  ctx         pointer to CPU context
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cpu180CheckMonitorConditions(Cpu180Context *ctx)
    {
    u16              cr;
    u16              mask;
    MonitorCondition mCond;

    cr = ctx->regMcr & ctx->regMmr;
    for (mCond = MCR48; cr != 0 && mCond <= MCR63; mCond++)
        {
        mask = mcrDefns[mCond].bitMask;
        if ((cr & mask) != 0)
            {
            cpu180SetMonitorCondition(ctx, mCond);
            cr &= ~mask;
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Check user condition register for indications
**
**                  Ordinarily, this is called after an exchange or return
**                  operation to check for previously stacked conditions.
**
**  Parameters:     Name        Description.
**                  ctx         pointer to CPU context
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cpu180CheckUserConditions(Cpu180Context *ctx)
    {
    u16              cr;
    u16              mask;
    UserCondition    uCond;

    cr = ctx->regUcr & ctx->regUmr;
    for (uCond = UCR48; cr != 0 && uCond <= UCR63; uCond++)
        {
        mask = ucrDefns[uCond].bitMask;
        if ((cr & mask) != 0)
            {
            cpu180SetUserCondition(ctx, uCond);
            cr &= ~mask;
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Perform exchange operation.
**
**  Parameters:     Name         Description.
**                  activeCpu    Pointer to CPU context
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cpu180Exchange(Cpu180Context *activeCpu)
    {
    u32 xpa;
    u8  vmid;

    xpa = ((activeCpu->isMonitorMode) ? activeCpu->regJps : activeCpu->regMps) >> 3;
    vmid = ((cpMem[xpa + 1]) >> 56) & Mask4;

    if (vmid == 0) // 180 -> 180 state exchange
        {
        activeCpu->regP = activeCpu->nextP;
        if (activeCpu->isMonitorMode)
            {
            cpu180Store180Xp(activeCpu, activeCpu->regMps >> 3);
            activeCpu->isMonitorMode = FALSE;
            cpu180Load180Xp(activeCpu, activeCpu->regJps >> 3);
            }
        else
            {
            cpu180Store180Xp(activeCpu, activeCpu->regJps >> 3);
            activeCpu->isMonitorMode = TRUE;
            cpu180Load180Xp(activeCpu, activeCpu->regMps >> 3);
            }
        activeCpu->nextKey = activeCpu->key;
        activeCpu->nextP   = activeCpu->regP;
        cpu180CheckConditions(activeCpu);
        }
    else if (vmid == 1 && activeCpu->isMonitorMode) // 180 -> 170 state exchange
        {
/*DELETE*/ //if(activeCpu->regMps == 0x00620080)traceMask|=TraceCpu|TraceExchange|TraceBlockOp|TraceCallFrame;
        activeCpu->regP = activeCpu->nextP;
        cpu180Store180Xp(activeCpu, activeCpu->regMps >> 3);
        activeCpu->isMonitorMode = FALSE;
        cpu180Load170Xp(activeCpu, activeCpu->regJps >> 3);
        }
    else
        {
        cpu180SetMonitorCondition(activeCpu, MCR55); // environment specification error
        activeCpu->regUvmid = vmid;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Search the system page table for the entry associated
**                  with an ASID and byte number.
**
**  Parameters:     Name        Description.
**                  ctx         pointer to CPU context
**                  asid        the ASID
**                  byteNum     the byte number
**                  pti         (out) page table index of entry, if found
**                  count       (out) number of entries searched, if found
**
**  Returns:        TRUE if page table entry found
**
**------------------------------------------------------------------------*/
static bool cpu180FindPte(Cpu180Context *ctx, u16 asid, u32 byteNum, bool ignoreValidity, u32 *pti, u8 *count)
    {
    u8   flags;
    bool found;
    u32  hash;
    u32  idx;
    u8   n;
    u32  pageNum;
    u64  pte;
    u64  spid;

    /*
    **  Calculate the starting page table index, per section 3.5.2 of MIGDS.
    */
    pageNum = byteNum >> ctx->pageNumShift;
    hash    = asid ^ (pageNum & Mask16);
    idx     = ((ctx->regPta & 0xfffff000) | ((hash << 4) & ctx->pageLengthMask)) >> 3;
    spid    = ((u64)asid << 22) | ((u64)pageNum << ctx->spidShift);

#if CcDebug == 1
    tracePageInfo(ctx, hash, pageNum, byteNum & ctx->pageOffsetMask, idx, spid);
#endif

    /*
    **  Search page table for an entry with a matching SPID.
    */
    found = FALSE;
    n     = 1;
    for (;;)
        {
        pte   = cpMem[idx]; // next page table entry
        flags = pte >> 60;

#if CcDebug == 1
        tracePte(ctx, pte);
#endif

        if (((flags & 0x8) != 0 || ignoreValidity) && spid == ((pte >> 22) & Mask38))
            {
            found = TRUE;
            break;
            }
        else if ((flags & 0x4) == 0 || n >= 32)
            {
            break;
            }

        idx += 1;
        n   += 1;
        }

    *pti   = idx;
    *count = n;

    return found;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Get a BDP descriptor from a specified PVA
**
**  Parameters:     Name        Description.
**                  ctx         pointer to CPU context
**                  pva         process virtual address of byte
**                  aRegNum     A register number (Aj or Ak)
**                  xRegNum     X register number (0 or 1)
**                  descriptor  (out) pointer to BDP descriptor
**
**  Returns:        TRUE if successful, FALSE if address specification error
**                  or page fault
**
**------------------------------------------------------------------------*/
static bool cpu180GetBdpDescriptor(Cpu180Context *ctx, u64 pva, u8 aRegNum, u8 xRegNum, BdpDescriptor *descriptor)
    {
    u64 desc;
    u16 operandAddress;

    if (cpu180GetBytes(ctx, pva, 4, AccessModeExecute, &desc))
        {
        descriptor->rawDesc = desc;
        descriptor->type    = (desc >> 24) & Mask4;
        descriptor->length  = (desc < 0x80000000) ? (desc >> 16) & Mask8 : ctx->regX[xRegNum] & Mask9;
        operandAddress      = desc & Mask16;
        descriptor->pva     = (ctx->regA[aRegNum] & RingSegMask)
            | ((ctx->regA[aRegNum] + ((operandAddress < 0x8000) ? operandAddress : 0xffff0000 | operandAddress)) & Mask32);
        return TRUE;
        }
    else
        {
        return FALSE;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Get a byte from a specified PVA
**
**  Parameters:     Name        Description.
**                  ctx         pointer to CPU context
**                  pva         process virtual address of byte
**                  access      access mode (read or execute)
**                  byte        (out) pointer to byte
**
**  Returns:        TRUE if successful, FALSE if address specification error
**                  or page fault
**
**------------------------------------------------------------------------*/
static bool cpu180GetByte(Cpu180Context *ctx, u64 pva, Cpu180AccessMode access, u8 *byte)
    {
    MonitorCondition cond;
    u32              rma;
    u8               shift;
    u64              word;

    if (cpu180PvaToRma(ctx, pva, access, &rma, &cond))
        {
        word  = cpMem[rma >> 3];
        shift = 56 - ((rma & 7) << 3);
        *byte = (word >> shift) & 0xff;
        return TRUE;
        }
    else
        {
        cpu180SetMonitorCondition(ctx, cond);
        return FALSE;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Get one to eight bytes from a specified PVA
**
**  Parameters:     Name        Description.
**                  ctx         pointer to CPU context
**                  pva         process virtual address of first byte
**                  count       number of bytes to get
**                  access      access mode (read or execute)
**                  word        (out) pointer to assembled bytes, right justified
**
**  Returns:        TRUE if successful, FALSE if address specification error
**                  or page fault
**
**------------------------------------------------------------------------*/
static bool cpu180GetBytes(Cpu180Context *ctx, u64 pva, int count, Cpu180AccessMode access, u64 *word)
    {
    MonitorCondition cond;
    u8               i;
    u32              pageNum;
    u32              rma;
    u32              rmas[8];
    u8               shift;
    u32              wordAddr;

    if (cpu180PvaToRma(ctx, pva, access, &rma, &cond) == FALSE)
        {
        cpu180SetMonitorCondition(ctx, cond);
        return FALSE;
        }
    if ((rma & Mask3) == 0) // optimization: word-aligned load
        {
        *word = cpMem[rma >> 3];
        if (count < 8)
            {
            *word >>= (8 - count) << 3;
            }
        return TRUE;
        }
    rmas[0] = rma;
    pageNum = PageOf(pva, ctx);
    for (i = 1; i < count; i++)
        {
        pva += 1;
        rma += 1;
        if (PageOf(pva, ctx) != pageNum)
            {
            if (cpu180PvaToRma(ctx, pva, access, &rma, &cond))
                {
                cpu180SetMonitorCondition(ctx, cond);
                return FALSE;
                }
            pageNum = PageOf(pva, ctx);
            }
        rmas[i] = rma;
        }
    *word = 0;
    for (i = 0; i < count; i++)
        {
        rma      = rmas[i];
        wordAddr = rma >> 3;
        shift    = 56 - ((rma & 7) << 3);
        *word    = (*word << 8) | ((cpMem[wordAddr] >> shift) & Mask8);
        }

    return TRUE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Get execute permission for the segment referenced by
**                  the current P register value.
**
**  Parameters:     Name        Description.
**                  ctx         pointer to C180 CPU context
**
**  Returns:        XP field from segment descriptor table entry
**
**------------------------------------------------------------------------*/
static u8 cpu180GetCurrentXp(Cpu180Context *ctx)
    {
    u16 segNum;

    segNum  = SegmentOf(ctx->regP);
    if (segNum <= ctx->regStl)
        {
        return (cpMem[(ctx->regSta >> 3) + segNum] >> 60) & Mask2;
        }
    else
        {
        return 0;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Get the lock defined for the segment of a PVA
**
**  Parameters:     Name        Description.
**                  ctx         pointer to C180 CPU context
**                  pva         the PVA for which to obtain associated lock
**
**  Returns:        the lock
**
**------------------------------------------------------------------------*/
static u8 cpu180GetLock(Cpu180Context *ctx, u64 pva)
    {
    u16 segNum;

    segNum  = SegmentOf(pva);
    if (segNum <= ctx->regStl)
        {
        return (cpMem[(ctx->regSta >> 3) + segNum] >> 24) & Mask6;
        }
    else
        {
        return 0;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Get a 16-bit instruction parcel from a specified PVA
**
**  Parameters:     Name        Description.
**                  ctx         pointer to CPU context
**                  pva         process virtual address of first byte
**                  parcel      (out) pointer to parcel
**
**  Returns:        TRUE if successful, FALSE if address specification error
**                  or page fault
**
**------------------------------------------------------------------------*/
static bool cpu180GetParcel(Cpu180Context *ctx, u64 pva, u16 *parcel)
    {
    MonitorCondition cond;
    u32              rma;
    u8               shift;
    u64              word;

    if (cpu180PvaToRma(ctx, pva, AccessModeExecute, &rma, &cond))
        {
        word    = cpMem[rma >> 3];
        shift   = 48 - ((rma & 6) << 3);
        *parcel = (word >> shift) & 0xffff;
        return TRUE;
        }
    else
        {
        cpu180SetMonitorCondition(ctx, cond);
        return FALSE;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Get the R1 field defined for the segment of a PVA
**
**  Parameters:     Name        Description.
**                  ctx         pointer to C180 CPU context
**                  pva         the PVA for which to obtain associated R1 field
**
**  Returns:        the R1 field
**
**------------------------------------------------------------------------*/
static u8 cpu180GetR1(Cpu180Context *ctx, u64 pva)
    {
    u16 segNum;

    segNum  = SegmentOf(pva);
    if (segNum <= ctx->regStl)
        {
        return (cpMem[(ctx->regSta >> 3) + segNum] >> 52) & Mask4;
        }
    else
        {
        return 0;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Get the R2 field defined for the segment of a PVA
**
**  Parameters:     Name        Description.
**                  ctx         pointer to C180 CPU context
**                  pva         the PVA for which to obtain associated R2 field
**
**  Returns:        the R2 field
**
**------------------------------------------------------------------------*/
static u8 cpu180GetR2(Cpu180Context *ctx, u64 pva)
    {
    u16 segNum;

    segNum  = SegmentOf(pva);
    if (segNum <= ctx->regStl)
        {
        return (cpMem[(ctx->regSta >> 3) + segNum] >> 48) & Mask4;
        }
    else
        {
        return 0;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Determine whether a PVA is a binding section reference
**
**  Parameters:     Name        Description.
**                  ctx         pointer to C180 CPU context
**                  pva         the PVA to test
**
**  Returns:        TRUE if PVA is a binding section reference
**
**------------------------------------------------------------------------*/
static bool cpu180IsBindingSectionRef(Cpu180Context *ctx, u64 pva)
    {
    u16 segNum;

    segNum  = SegmentOf(pva);
    if (segNum <= ctx->regStl)
        {
        return ((cpMem[(ctx->regSta >> 3) + segNum] >> 58) & Mask2) == 3;
        }
    else
        {
        return FALSE;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Determine whether a PVA references a valid segment
**
**  Parameters:     Name        Description.
**                  ctx         pointer to C180 CPU context
**                  pva         the PVA to test
**
**  Returns:        TRUE if PVA references a valid segment
**
**------------------------------------------------------------------------*/
static bool cpu180IsValidSegment(Cpu180Context *ctx, u64 pva)
    {
    u64 sde;
    u16 segNum;

    segNum = SegmentOf(pva);
    if (segNum > ctx->regStl)
        {
        return FALSE;
        }
    sde = cpMem[(ctx->regSta >> 3) + segNum];

    return (sde >> 63) != 0;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Multiply two 32-bit integer quantities and detect overflow
**
**  Parameters:     Name        Description.
**                  ctx         pointer to CPU context
**                  mltand      the multiplicand
**                  mltier      the multiplier
**                  product     (out) pointer to sum
**
**  Returns:        TRUE if arithmetic overflow not detected.
**                  FALSE if overflow detected and user condition register
**                  bit set.
**
**------------------------------------------------------------------------*/
static bool cpu180MulInt32(Cpu180Context *ctx, u32 mltand, u32 mltier, u32 *product)
    {
    u64 prod64;

    prod64 = (u64)mltand * (u64)mltier;
    if (mltand != 0 && (prod64 / (u64)mltand) != mltier)
        {
        cpu180SetUserCondition(ctx, UCR57);
        return FALSE;
        }
    *product = prod64;

    return TRUE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Multiply two 64-bit integer quantities and detect overflow
**
**  Parameters:     Name        Description.
**                  ctx         pointer to CPU context
**                  mltand      the multiplicand
**                  mltier      the multiplier
**                  product     (out) pointer to sum
**
**  Returns:        TRUE if arithmetic overflow not detected.
**                  FALSE if overflow detected and user condition register
**                  bit set.
**
**------------------------------------------------------------------------*/
static bool cpu180MulInt64(Cpu180Context *ctx, u64 mltand, u64 mltier, u64 *product)
    {
    u64  lower64;
    u128 p128;
    u64  upper64;

    if ((i64)mltand < 0)
        {
        if ((i64)mltier < 0)
            {
            p128 = ((u128)mltand | ((u128)0xffffffffffffffff << 64)) * ((u128)mltier | ((u128)0xffffffffffffffff << 64));
            }
        else
            {
            p128 = (u128)mltier * ((u128)mltand | ((u128)0xffffffffffffffff << 64));
            }
        }
    else if ((i64)mltier < 0)
        {
        p128 = (u128)mltand * ((u128)mltier | ((u128)0xffffffffffffffff << 64));
        }
    else
        {
        p128 = (u128)mltand * (u128)mltier;
        }
    lower64 = p128;
    upper64 = p128 >> 64;
    if ((lower64 < 0x8000000000000000 && upper64 != 0) || (lower64 >= 0x8000000000000000 && upper64 != 0xffffffffffffffff))
        {
        cpu180SetUserCondition(ctx, UCR57);
        return FALSE;
        }
    *product = p128;

    return TRUE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Push a CYBER 180 stack frame for a trap or call operation.
**
**                  See MIGDS 2-116 and 2-180
**
**  Parameters:     Name         Description.
**                  ctx          Pointer to CYBER 180 CPU context
**                  at           terminating A register
**                  xs           starting X register
**                  xt           terminating X register
**                  isTrap       TRUE if trap, FALSE if CALLREL or CALLSEG
**                  sfsa         (out) PVA of stack frame save area
**                  frameSize    (out) number of bytes stored
**
**  Returns:        TRUE if successful.
**
**------------------------------------------------------------------------*/
static bool cpu180PushFrame(Cpu180Context *ctx, u16 at, u16 xs, u16 xt, bool isTrap, u64 *sfsa, u32 *frameSize)
    {
    MonitorCondition cond;
    u8               i;
    u32              pageNum;
    u64              pva;
    u8               r;
    u32              rma;
    u32              wordAddrs[33];
    int              words;

    if (at < 2)
        {
        cpu180SetMonitorCondition(ctx, MCR51); // instruction specification error
        return FALSE;
        }
    pva   = (ctx->regA[0] + 7) & 0xfffffffffff8;
    *sfsa = pva;
    words = 4;
    if (at > 2)
        {
        words += at - 2;
        }
    if (xt >= xs)
        {
        words += (xt - xs) + 1;
        }
    if (cpu180PvaToRma(ctx, pva, AccessModeWrite, &rma, &cond) == FALSE)
        {
        cpu180SetMonitorCondition(ctx, cond);
        return FALSE;
        }
    wordAddrs[0] = rma >> 3;
    pageNum      = PageOf(pva, ctx);
    for (i = 1; i < words; i++)
        {
        pva += 8;
        if (PageOf(pva, ctx) != pageNum)
            {
            if (cpu180PvaToRma(ctx, pva, AccessModeWrite, &rma, &cond) == FALSE)
                {
                cpu180SetMonitorCondition(ctx, cond);
                return FALSE;
                }
            pageNum = PageOf(pva, ctx);
            }
        else
            {
            rma += 8;
            }
        wordAddrs[i] = rma >> 3;
        }
    ctx->regA[0]          = *sfsa;
    i                     = 0;
    cpMem[wordAddrs[i++]] = ((u64)ctx->nextKey << 48) | ctx->nextP;
    cpMem[wordAddrs[i++]] = ((u64)ctx->regVmid << 56) | ctx->regA[0];
    cpMem[wordAddrs[i++]] = ((u64)((ctx->regFlags & 0xd000) | (xs << 8) | (at << 4) | xt) << 48) | ctx->regA[1];
    cpMem[wordAddrs[i++]] = ((u64)ctx->regUmr << 48) | ctx->regA[2];
    for (r = 3; r <= at; r++)
        {
        cpMem[wordAddrs[i++]] = ctx->regA[r];
        }
    for (r = xs; r <= xt; r++)
        {
        cpMem[wordAddrs[i++]] = ctx->regX[r];
        }
    if (isTrap)
        {
        cpMem[wordAddrs[5]] |= (u64)ctx->regUcr << 48;
        cpMem[wordAddrs[6]] |= (u64)ctx->regMcr << 48;
        }
    ctx->regX[0] = (ctx->regX[0] & Mask32) | (cpMem[wordAddrs[0]] & LeftMask);
    *frameSize   = words << 3;

    return TRUE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Put a byte in memory at a specified PVA
**
**  Parameters:     Name        Description.
**                  ctx         pointer to CPU context
**                  pva         process virtual address at which to put byte
**                  byte        the byte
**
**  Returns:        TRUE if successful, FALSE if address specification error
**                  or page fault
**
**------------------------------------------------------------------------*/
static bool cpu180PutByte(Cpu180Context *ctx, u64 pva, u8 byte)
    {
    MonitorCondition cond;
    u64              mask;
    u32              rma;
    u8               shift;
    u32              wordAddr;

    if (cpu180PvaToRma(ctx, pva, AccessModeWrite, &rma, &cond))
        {
        wordAddr        = rma >> 3;
        shift           = 56 - ((rma & 7) << 3);
        mask            = ~((u64)0xff << shift);
        cpMem[wordAddr] = (cpMem[wordAddr] & mask) | ((u64)byte << shift);
        return TRUE;
        }
    else
        {
        cpu180SetMonitorCondition(ctx, cond);
        return FALSE;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Put 1 to 8 bytes in memory at a specified PVA
**
**  Parameters:     Name        Description.
**                  ctx         pointer to CPU context
**                  pva         process virtual address at which to put byte
**                  word        the right-justified bytes
**                  count       the number of bytes
**
**  Returns:        TRUE if successful.
**
**------------------------------------------------------------------------*/
static bool cpu180PutBytes(Cpu180Context *ctx, u64 pva, u64 word, int count)
    {
    MonitorCondition cond;
    u8               i;
    u64              mask;
    u32              pageNum;
    u32              rma;
    u32              rmas[8];
    u8               shift;
    u32              wordAddr;

    static u64       masks[8] =
        {
        0x00ffffffffffffff,
        0x0000ffffffffffff,
        0x000000ffffffffff,
        0x00000000ffffffff,
        0x0000000000ffffff,
        0x000000000000ffff,
        0x00000000000000ff,
        0x0000000000000000,
        };

    if (cpu180PvaToRma(ctx, pva, AccessModeWrite, &rma, &cond) == FALSE)
        {
        cpu180SetMonitorCondition(ctx, cond);
        return FALSE;
        }
    if ((rma & Mask3) == 0) // optimization: word-aligned store
        {
        wordAddr = rma >> 3;
        if (count < 8)
            {
            shift = (8 - count) << 3;
            word = (word << shift) | (cpMem[wordAddr] & masks[count - 1]);
            }
        cpMem[wordAddr] = word;
        return TRUE;
        }
    rmas[0] = rma;
    pageNum = PageOf(pva, ctx);
    for (i = 1; i < count; i++)
        {
        pva += 1;
        rma += 1;
        if (PageOf(pva, ctx) != pageNum)
            {
            if (cpu180PvaToRma(ctx, pva, AccessModeWrite, &rma, &cond) == FALSE)
                {
                cpu180SetMonitorCondition(ctx, cond);
                return FALSE;
                }
            pageNum = PageOf(pva, ctx);
            }
        rmas[i] = rma;
        }
    i     = 0;
    count = (count - 1) << 3;
    while (count >= 0)
        {
        rma             = rmas[i++];
        wordAddr        = rma >> 3;
        shift           = 56 - ((rma & 7) << 3);
        mask            = ~((u64)0xff << shift);
        cpMem[wordAddr] = (cpMem[wordAddr] & mask) | (((word >> count) & Mask8) << shift);
        count          -= 8;
        }

    return TRUE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Conditionally set a ring 0 detected condition
**
**  Parameters:     Name        Description.
**                  ctx         pointer to CPU context
**                  pva         PVA with ring 0
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cpu180SetRingZeroCondition(Cpu180Context *ctx, u64 pva)
    {
    ConditionAction     action;
    ConditionActionDefn *defn;

    if ((features & HasRingZeroTest) != 0)
        {
/*DELETE*/ traceMask |= TraceCpu|TraceExchange|TraceBlockOp;
        defn         = &mcrDefns[MCR60];
        ctx->regMcr |= defn->bitMask;

        if ((ctx->regMmr & defn->bitMask) == 0)
            {
            action = defn->whenNoMask;
            }
        else if ((ctx->regFlags & 3) == 2) // trap enabled
            {
            action = ctx->isMonitorMode ? defn->whenMaskTrapMonitor : defn->whenMaskTrapJob;
            }
        else
            {
            action = ctx->isMonitorMode ? defn->whenMaskNoTrapMonitor : defn->whenMaskNoTrapJob;
            }
        if (action > ctx->pendingAction)
            {
            ctx->pendingAction = action;
            }
#if CcDebug == 1
        traceRingZeroCondition(ctx, pva);
#endif
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Store the 180 state exchange package into memory
**                  referenced by a specified real memory word address.
**
**  Parameters:     Name        Description.
**                  ctx         pointer to CPU context
**                  xpa         word address into which to store exchange package
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
static void cpu180Store180Xp(Cpu180Context *ctx, u32 xpa)
    {
    int i;
#if CcDebug == 1
    u32 xpab = xpa << 3;
#endif

    cpMem[xpa++] = ((u64)ctx->key << 48) | ctx->regP;
    cpMem[xpa++] = ((u64)ctx->regVmid << 56) | ((u64)ctx->regUvmid << 48) | ctx->regA[0];
    cpMem[xpa++] = ((u64)ctx->regFlags << 48) | ctx->regA[1];
    cpMem[xpa++] = ((u64)ctx->regUmr << 48) | ctx->regA[2];
    cpMem[xpa++] = ((u64)ctx->regMmr << 48) | ctx->regA[3];
    cpMem[xpa++] = ((u64)ctx->regUcr << 48) | ctx->regA[4];
    cpMem[xpa++] = ((u64)ctx->regMcr << 48) | ctx->regA[5];
    cpMem[xpa++] = ((u64)ctx->id << 48) | ctx->regA[6];
    cpMem[xpa++] = ((u64)ctx->regKmr << 48) | ctx->regA[7];
    cpMem[xpa++] = ctx->regA[8];
    cpMem[xpa++] = ctx->regA[9];
    cpMem[xpa++] = ((u64)(ctx->regPit & 0xffff0000) << 32) | ctx->regA[10];
    cpMem[xpa++] = ((u64)(ctx->regPit & 0x0000ffff) << 48) | ctx->regA[11];
    cpMem[xpa++] = ((u64)(ctx->regBc & 0xffff0000) << 32) | ctx->regA[12];
    cpMem[xpa++] = ((u64)(ctx->regBc & 0x0000ffff) << 48) | ctx->regA[13];
    cpMem[xpa++] = ((u64)ctx->regMdf << 48) | ctx->regA[14];
    cpMem[xpa++] = ((u64)ctx->regStl << 48) | ctx->regA[15];
 
    for (i = 0; i < 16; i++)
        {
        cpMem[xpa++] = ctx->regX[i];
        }

    cpMem[xpa++] = ctx->regMdw;
    cpMem[xpa++] = ((u64)(ctx->regSta & 0xffff0000) << 32) | ctx->regUtp;
    cpMem[xpa++] = ((u64)(ctx->regSta & 0x0000ffff) << 48) | ctx->regTp;
    cpMem[xpa++] = ((u64)ctx->regDi << 58) | ((u64)ctx->regDm << 48) | ctx->regDlp;
    cpMem[xpa++] = ((u64)ctx->regLrn << 48) | ctx->regTos[0];

    for (i = 1; i < 15; i++)
        {
        cpMem[xpa++] = ctx->regTos[i];
        }
#if CcDebug == 1
    traceExchange180(ctx, xpab, "Store");
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Perform trap operation.
**
**                  See MIGDS 2-180
**
**  Parameters:     Name         Description.
**                  ctx          Pointer to CPU context
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cpu180Trap(Cpu180Context *ctx)
    {
    u64              bsp;
    u64              cbp;
    Cpu170Context    *ctx170;
    MonitorCondition cond;
    int              i;
    bool             isExt;
    u16              flags;
    u32              frameSize;
    u64              pva;
    u8               r2;
    u8               ring;
    u64              ring64;
    u32              rma;
    u64              sfsa;
    u8               vmid;
    u32              wordAddr;

#if CcDebug == 1
    traceTrapPointer(ctx);
#endif

    if (cpu180PvaToRma(ctx, ctx->regTp, AccessModeRead, &rma, &cond) == FALSE)
        {
        flags          = ctx->regFlags; // temporarily disable traps
        ctx->regFlags &= 0xfc;
        cpu180SetMonitorCondition(ctx, MCR63);
        ctx->regFlags = flags;
        return;
        }
    cbp   = cpMem[rma >> 3];
    vmid  = (cbp >> 56) & Mask4;
    isExt = vmid == 0 && ((cbp >> 55) & 1) != 0;
    if (isExt)
        {
        if (cpu180PvaToRma(ctx, ctx->regTp + 8, AccessModeRead, &rma, &cond) == FALSE)
            {
            flags          = ctx->regFlags; // temporarily disable traps
            ctx->regFlags &= 0xfc;
            cpu180SetMonitorCondition(ctx, MCR63);
            ctx->regFlags = flags;
            return;
            }
        bsp = cpMem[rma >> 3] & Mask48;
        }
    if (ctx->regVmid == 1) // map 170 to 180 exchange package
        {
        ctx170       = &cpus170[ctx->id];
        ctx->regP    = (ctx->regP170 & ~(u64)Mask32)
                       | ((ctx170->regRaCm + ctx170->regP) << 3)
                       | (((4 - (ctx170->opOffset / 15)) & 3) << 1);
        ctx->nextP   = ctx->regP;
        ring64       = ctx->regP & RingMask;
        ctx->regA[3] = ring64 | ((u64)ctx170->exitMode << 20) | ctx170->regRaCm;
        ctx->regA[4] = ring64 | (ctx170->isMonitorMode ? (u64)1 << 32 : 0) | ctx170->regFlCm;
        ctx->regA[5] = ring64 | ctx170->regMa;
        ctx->regA[6] = ring64 | ctx170->regRaEcs;
        ctx->regA[7] = ring64 | ctx170->regFlEcs;
        for (i = 0; i < 8; i++)
            {
            ctx->regA[i + 8] = ring64 | ctx170->regA[i];
            }
        for (i = 1; i < 8; i++)
            {
            ctx->regX[i] = ctx170->regB[i];
            }
        for (i = 0; i < 8; i++)
            {
            ctx->regX[i + 8] = ctx170->regX[i];
            }
        }
    if (cpu180PushFrame(ctx, 0xf, 0x0, 0xf, TRUE, &sfsa, &frameSize) == FALSE)
        {
        flags          = ctx->regFlags; // temporarily disable traps
        ctx->regFlags &= 0xfc;
        cpu180SetMonitorCondition(ctx, MCR63);
        ctx->regFlags = flags;
        return;
        }
#if CcDebug == 1
    traceTrapFrame(ctx, sfsa);
#endif
    ring                  = RingOf(ctx->regP);
    ctx->regA[2]          = ctx->regA[0];
    ctx->regA[0]         += frameSize;
    ctx->regTos[ring - 1] = ctx->regA[0];
    if (ring > ctx->regLrn)
        {
        ctx->regLrn = ring;
        }
    pva      = cbp & Mask48;
    ctx->key = cpu180GetLock(ctx, pva);
    r2       = cpu180GetR2(ctx, pva);
    ring     = RingOf(pva);
    if (ring > r2)
        {
        ring = r2;
        }
    ctx->regP = ((u64)ring << 44) | (cbp & Mask44);
    if (isExt)
        {
        if ((bsp & RingMask) < (ctx->regP & RingMask))
            {
            ctx->regA[3] = (bsp & ~RingMask) | (ctx->regP & RingMask);
            }
        else
            {
            ctx->regA[3] = bsp;
            }
        }
    ctx->regA[1]   = ctx->regTos[ring - 1];
    ctx->regA[0]   = ctx->regA[1];
    ctx->regVmid   = vmid;
    ctx->regFlags &= 0x3fff; // clear CCF and OCF
    ctx->regMcr   &= ~(ctx->regMmr | 0x0021); // clear masked bits and status bits
    ctx->regUcr   &= ~ctx->regUmr;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Validate an access mode for a PVA
**
**  Parameters:     Name        Description.
**                  ctx         pointer to C180 CPU context
**                  sde         segment descriptor entry
**                  ring        ring number for which to validate access
**                  access      mode of access (execute, read, write)
**
**  Returns:        TRUE if access allowed
**
**------------------------------------------------------------------------*/
static bool cpu180ValidateAccess(Cpu180Context *ctx, u64 sde, u8 ring, Cpu180AccessMode access)
    {
    u8  lock;
    u8  pm;

    lock = (sde >> 24) & Mask6;

    /*
    **  Validate access
    */
    if ((access & AccessModeExecute) != 0)
        {
        if (((sde >> 60) & Mask2) == 0
            || ring < ((sde >> 52) & Mask4)
            || ring > ((sde >> 48) & Mask4)
            || (ctx->key != lock && ctx->key != 0 && lock != 0))
            {
            return FALSE;
            }
        }
    if ((access & AccessModeRead) != 0)
        {
        if (ring > ((sde >> 48) & Mask4))
            {
            return FALSE;
            }
        pm = (sde >> 58) & Mask2;
        switch (pm)
            {
        default:
        case 0:
            return FALSE;
        case 1:
            if (ctx->key != lock && ctx->key != 0 && lock != 0)
                {
                return FALSE;
                }
            break;
        case 2:
        case 3:
            break;
            }
        }
    if ((access & AccessModeWrite) != 0)
        {
        if (ring > ((sde >> 52) & Mask4))
            {
            return FALSE;
            }
        pm = (sde >> 56) & Mask2;
        switch (pm)
            {
        default:
        case 0:
        case 3:
            return FALSE;
        case 1:
            lock = (sde >> 24) & Mask6;
            if (ctx->key != lock && ctx->key != 0 && lock != 0)
                {
                return FALSE;
                }
            break;
        case 2:
            break;
            }
        }

    return TRUE;
    }

/*--------------------------------------------------------------------------
**
**                       CYBER 180 CPU instructions
**
**------------------------------------------------------------------------*/

static void cp180Op00(Cpu180Context *activeCpu)  // 00  HALT       MIGDS 2-122
    {
    cpu180SetMonitorCondition(activeCpu, MCR51); // instruction specification error
    }

static void cp180Op01(Cpu180Context *activeCpu)  // 01  SYNC       MIGDS 2-138
    {
    cp180OpIv(activeCpu);
    }

static void cp180Op02(Cpu180Context *activeCpu)  // 02  EXCHANGE   MIGDS 2-132
    {
    if (activeCpu->isMonitorMode == FALSE)
        {
        activeCpu->regMcr |= 0x20; // set System Call status bit
        }
    cpu180Exchange(activeCpu);
    }

static void cp180Op03(Cpu180Context *activeCpu)  // 03  INTRUPT    MIGDS 2-141
    {
    cp180OpIv(activeCpu);
    }

static void cp180Op04(Cpu180Context *activeCpu)  // 04  RETURN     MIGDS 2-127
    {
    u8               at;
    MonitorCondition cond;
    Cpu170Context    *ctx170;
    u16              desc;
    u8               i;
    u32              pageNum;
    u64              pva;
    u64              r1;
    u8               ring;
    u64              ringA2;
    u64              ringNewP;
    u64              ringP;
    u8               r;
    u32              rma;
    u8               vmid;
    u64              word;
    u32              wordAddr;
    u32              wordAddrs[33];
    int              words;
    u8               xs;
    u8               xt;

    pva = activeCpu->regA[2];
    if (cpu180PvaToRma(activeCpu, pva, AccessModeRead, &rma, &cond) == FALSE)
        {
        return;
        }
    wordAddrs[0] = rma >> 3;
    pageNum      = PageOf(pva, activeCpu);
    for (i = 1; i < 4; i++)
        {
        pva += 8;
        if (PageOf(pva, activeCpu) != pageNum)
            {
            if (cpu180PvaToRma(activeCpu, pva, AccessModeRead, &rma, &cond) == FALSE)
                {
                return;
                }
            pageNum = PageOf(pva, activeCpu);
            }
        else
            {
            rma += 8;
            }
        wordAddrs[i] = rma >> 3;
        }
    if ((cpMem[wordAddrs[0]] & RingMask) < (activeCpu->regA[2] & RingMask))
        {
        cpu180SetMonitorCondition(activeCpu, MCR61); // inward return
        return;
        }
    vmid = (cpMem[wordAddrs[1]] >> 56) & Mask4;
    desc = cpMem[wordAddrs[2]] >> 48;
    if ((desc & 0x8000) != 0)
        {
        cpu180SetUserCondition(activeCpu, UCR53); // critical frame flag
        return;
        }
    at   = (desc >> 4) & Mask4;
    xs   = (desc >> 8) & Mask4;
    xt   = desc & Mask4;
    if (at < 2 || vmid > 1)
        {
        cpu180SetMonitorCondition(activeCpu, MCR55); // environment specification error
        return;
        }
    if (vmid == 1 && cpu180GetCurrentXp(activeCpu) < 3) // not global privileged
        {
        cpu180SetMonitorCondition(activeCpu, MCR55); // environment specification error
        return;
        }
    words = 4 + (at - 2);
    if (xt >= xs)
        {
        words += (xt - xs) + 1;
        }
    i = 4;
    while (i < words)
        {
        pva += 8;
        if (PageOf(pva, activeCpu) != pageNum)
            {
            if (cpu180PvaToRma(activeCpu, pva, AccessModeRead, &rma, &cond) == FALSE)
                {
                return;
                }
            pageNum = PageOf(pva, activeCpu);
            }
        else
            {
            rma += 8;
            }
        wordAddrs[i++] = rma >> 3;
        }
    ringP              = activeCpu->regP & RingMask;
    ringA2             = activeCpu->regA[2] & RingMask;
    r1                 = (u64)cpu180GetR1(activeCpu, activeCpu->regA[2]) << 44;
    ringA2             = (r1 > ringA2) ? r1 : ringA2;
    i                  = 0;
    word               = cpMem[wordAddrs[i++]];
    activeCpu->nextKey = (word >> 48);
    activeCpu->nextP   = word & Mask48;
    ringNewP           = activeCpu->nextP & RingMask;
    word               = cpMem[wordAddrs[i++]];
    activeCpu->regVmid = (word >> 56) & Mask4;
    activeCpu->regA[0] = word & Mask48;
    activeCpu->regA[1] = cpMem[wordAddrs[i++]] & Mask48;
    word               = cpMem[wordAddrs[i++]];
    activeCpu->regUmr  = word >> 48;
    activeCpu->regA[2] = word & Mask48;
    for (r = 3; r <= at; r++)
        {
        activeCpu->regA[r] = cpMem[wordAddrs[i++]] & Mask48;
        }
    for (r = 0; r <= at; r++)
        {
        if ((activeCpu->regA[r] & RingMask) < ringA2)
            {
            activeCpu->regA[r] = ringA2 | (activeCpu->regA[r] & Mask44);
            }
        }
    if (ringP != ringNewP)
        {
        for (r = at + 1; r <= 0xf; r++)
            {
            if ((activeCpu->regA[r] & RingMask) < ringNewP)
                {
                activeCpu->regA[r] = ringNewP | (activeCpu->regA[r] & Mask44);
                }
            }
        }
    for (r = xs; r <= xt; r++)
        {
        activeCpu->regX[r] = cpMem[wordAddrs[i++]];
        }
    if (vmid == 1)
        {
        ctx170                = &cpus170[activeCpu->id];
        ctx170->exitMode      = (activeCpu->regA[3] >> 20) & 077770000;
        ctx170->regRaCm       = activeCpu->regA[3] & Mask21;
        ctx170->isMonitorMode = (activeCpu->regA[4] >> 32) & 1;
        ctx170->regFlCm       = activeCpu->regA[4] & Mask21;
        ctx170->regMa         = activeCpu->regA[5] & Mask21;
        ctx170->regRaEcs      = activeCpu->regA[6] & Mask24;
        ctx170->regFlEcs      = activeCpu->regA[7] & Mask24;
        for (i = 0; i < 8; i++)
            {
            ctx170->regA[i] = activeCpu->regA[i + 8] & Mask18;
            }
        for (i = 1; i < 8; i++)
            {
            ctx170->regB[i] = activeCpu->regX[i] & Mask18;
            }
        for (i = 0; i < 8; i++)
            {
            ctx170->regX[i] = activeCpu->regX[i + 8] & Mask60;
            }
        wordAddr          = (activeCpu->nextP & Mask32) >> 3;
        ctx170->regP      = wordAddr - ctx170->regRaCm;
        ctx170->opOffset  = 60 - (((activeCpu->nextP & Mask3) >> 1) * 15);
        ctx170->opWord    = cpMem[wordAddr];
        ctx170->isStopped = FALSE;
        if ((features & HasInstructionStack) != 0)
            {
            /*
            **  Void the instruction stack.
            */
            cpuVoidIwStack(ctx170, ~0);
            }
        }
    activeCpu->regFlags        &= 0xfffe; // clear trap enable delay flip-flop
    ring                        = RingOf(activeCpu->nextP);
    activeCpu->regTos[ring - 1] = activeCpu->regA[1];
    if (ring > activeCpu->regLrn)
        {
        activeCpu->regLrn = ring;
        }
    }

static void cp180Op05(Cpu180Context *activeCpu)  // 05  PURGE      MIGDS 2-147
    {
    /*
    ** TODO: implement as/if needed
    **   Xj has SVA or PVA
    **    k defines buffer to purge and range of entries
    */
    }

static void cp180Op06(Cpu180Context *activeCpu)  // 06  POP        MIGDS 2-129
    {
    MonitorCondition cond;
    u16              desc;
    u64              psap;
    u32              last;
    u64              regA;
    u64              r1;
    u64              ring;
    u64              ringA2;
    u32              rma;
    u32              wordAddr;

    psap = activeCpu->regA[2];
    if (cpu180PvaToRma(activeCpu, psap, AccessModeRead, &rma, &cond) == FALSE
        || cpu180PvaToRma(activeCpu, psap + 263, AccessModeRead, &last, &cond) == FALSE)
        {
        cpu180SetMonitorCondition(activeCpu, cond);
        return;
        }
    wordAddr = rma >> 3;
    desc     = cpMem[wordAddr + 2] >> 48;
    if (((desc >> 4) & Mask4) < 2)
        {
        cpu180SetMonitorCondition(activeCpu, MCR55); // environment specification error
        return;
        }
    if ((psap & RingMask) != (activeCpu->regP & RingMask))
        {
        cpu180SetUserCondition(activeCpu, UCR52); // inter-ring pop
        return;
        }
    if ((desc & 0x8000) != 0)
        {
        cpu180SetUserCondition(activeCpu, UCR53); // critical frame flag
        return;
        }
    ringA2 = activeCpu->regA[2] & RingMask;
    // Load A1
    regA   = cpMem[wordAddr + 2] & Mask48;
    ring   = regA & RingMask;
    r1     = (u64)cpu180GetR1(activeCpu, regA) << 44;
    if (ring < ringA2)
        {
        ring = ringA2;
        }
    if (ring < r1)
        {
        ring = r1;
        }
    activeCpu->regA[1] = ring | (regA & Mask44);
    // Load A2
    regA   = cpMem[wordAddr + 3] & Mask48;
    ring   = regA & RingMask;
    r1     = (u64)cpu180GetR1(activeCpu, regA) << 44;
    if (ring < ringA2)
        {
        ring = ringA2;
        }
    if (ring < r1)
        {
        ring = r1;
        }
    activeCpu->regA[2]          = ring | (regA & Mask44);
    activeCpu->regFlags         = (activeCpu->regFlags & 0x3fff) | ((cpMem[wordAddr + 2] >> 48) & 0xc000);
    ring                        = RingOf(activeCpu->nextP);
    activeCpu->regTos[ring - 1] = activeCpu->regA[1];
    if (ring > activeCpu->regLrn)
        {
        activeCpu->regLrn = ring;
        }
    }

static void cp180Op07(Cpu180Context *activeCpu)  // 07  PSFSA      MIGDS 2-138
    {
    // Only Theta-E supports the SFSA push-down. All other models handle this
    // instruction as no-op.
    }

static void cp180Op08(Cpu180Context *activeCpu)  // 08  CPYTX      MIGDS 2-137
    {
    // Bit 33 of Xj is ignored for now. This bit indicates whether an external memory
    // should be addressed instead of the local processor's memory.
    //
    //  Note: MIGDS 4-15, section 4.5.2.1, says that bit 63 of the free running counter
    //        shall increment at a 1 usec rate _AND_ that successive reads shall guarantee
    //        different values. The current implementation is not likely to guarantee
    //        that successive reads will produce different values on a fast, modern host.

    activeCpu->regX[activeCpu->opK] = cpu180FreeRunningCounter;
    }

static void cp180Op09(Cpu180Context *activeCpu)  // 09  CPYAA      MIGDS 2-28
    {
    activeCpu->regA[activeCpu->opK] = activeCpu->regA[activeCpu->opJ];
    }

static void cp180Op0A(Cpu180Context *activeCpu)  // 0A  CPYXA      MIGDS 2-28
    {
    u64 ringP;
    u64 ringX;
    u64 Xj;

    Xj                              = activeCpu->regX[activeCpu->opJ];
    ringX                           = Xj & RingMask;
    ringP                           = activeCpu->regP & RingMask;
    activeCpu->regA[activeCpu->opK] = (Xj & Mask44) | ((ringX > ringP ? ringX : ringP));
    }

static void cp180Op0B(Cpu180Context *activeCpu)  // 0B  CPYAX      MIGDS 2-28
    {
    activeCpu->regX[activeCpu->opK] = activeCpu->regA[activeCpu->opJ];
    }

static void cp180Op0C(Cpu180Context *activeCpu)  // 0C  CPYRR      MIGDS 2-28
    {
    activeCpu->regX[activeCpu->opK] = (activeCpu->regX[activeCpu->opK] & LeftMask) | (activeCpu->regX[activeCpu->opJ] & Mask32);
    }

static void cp180Op0D(Cpu180Context *activeCpu)  // 0D  CPYXX      MIGDS 2-28
    {
    activeCpu->regX[activeCpu->opK] = activeCpu->regX[activeCpu->opJ];
    }

static void cp180Op0E(Cpu180Context *activeCpu)  // 0E  CPYSX      MIGDS 2-146
    {
    u8 regId;

    regId = activeCpu->regX[activeCpu->opJ] & Mask8;
    if (regId < 0x10 || (regId >= 0x20 && regId <= 0x3f))
        {
        activeCpu->regX[activeCpu->opK] = 0;
        }
    else
        {
        activeCpu->regX[activeCpu->opK] = mchGetCpStateRegister(activeCpu, regId);
        }
    }

static void cp180Op0F(Cpu180Context *activeCpu)  // 0F  CPYXS      MIGDS 2-146
    {
    u8 regId;

    regId = activeCpu->regX[activeCpu->opJ] & Mask8;
    if (regId < 0x60) // no access
        {
        return;
        }
    else if (regId < 0x80) // monitor mode required
        {
        if (activeCpu->isMonitorMode == FALSE)
            {
            cpu180SetMonitorCondition(activeCpu, MCR51); // instruction specification error
            return;
            }
        }
    else if (regId < 0xc0) // global privileged mode required
        {
        if (cpu180GetCurrentXp(activeCpu) < 3)
            {
            cpu180SetUserCondition(activeCpu, UCR48); // privileged instruction fault
            return;
            }
        }
    else if (regId < 0xe0) // local privileged mode required
        {
        if (cpu180GetCurrentXp(activeCpu) < 2)
            {
            cpu180SetUserCondition(activeCpu, UCR48); // privileged instruction fault
            return;
            }
        }
    mchSetCpStateRegister(activeCpu, regId, activeCpu->regX[activeCpu->opK]);
    }

static void cp180Op10(Cpu180Context *activeCpu)  // 10  INCX       MIGDS 2-20
    {
    u64 sum;

    if (cpu180AddInt64(activeCpu, activeCpu->regX[activeCpu->opK], activeCpu->opJ, &sum))
        {
        activeCpu->regX[activeCpu->opK] = sum;
        }
    }

static void cp180Op11(Cpu180Context *activeCpu)  // 11  DECX       MIGDS 2-20
    {
    u64 sum;

    if (cpu180AddInt64(activeCpu, activeCpu->regX[activeCpu->opK], -((u64)activeCpu->opJ), &sum))
        {
        activeCpu->regX[activeCpu->opK] = sum;
        }
    }

static void cp180Op14(Cpu180Context *activeCpu)  // 14  LBSET      MIGDS 2-136
    {
    MonitorCondition cond;
    u64              mask;
    u32              offset;
    u64              pva;
    u32              rma;
    u8               shift;
    u32              wordAddr;

    offset   = activeCpu->regX[0] >> 3;
    if (offset > 0x0fffffff)
        {
        offset |= 0xf0000000;
        }
    pva = (activeCpu->regA[activeCpu->opJ] & RingSegMask) | ((activeCpu->regA[activeCpu->opJ] + offset) & Mask32);
    if (cpu180PvaToRma(activeCpu, pva, AccessModeRead | AccessModeWrite, &rma, &cond))
        {
        wordAddr         = rma >> 3;
        shift            = (56 - ((rma & 7) << 3)) + (7 - (activeCpu->regX[0] & 7));
        mask             = (u64)1 << shift;
        cpuAcquireMemoryMutex();
        activeCpu->regX[activeCpu->opK] = (cpMem[wordAddr] & mask) != 0 ? 1 : 0;
        cpMem[wordAddr]                |= mask;
        cpuReleaseMemoryMutex();
        }
    else
        {
        cpu180SetMonitorCondition(activeCpu, cond);
        }
    }

static void cp180Op16(Cpu180Context *activeCpu)  // 16  TPAGE      MIGDS 2-137
    {
    MonitorCondition cond;
    u32              rma;

    if (cpu180PvaToRma(activeCpu, activeCpu->regA[activeCpu->opJ], AccessModeAny, &rma, &cond))
        {
        activeCpu->regX[activeCpu->opK] = (activeCpu->regX[activeCpu->opK] & LeftMask) | rma;
        }
    else
        {
        activeCpu->regX[activeCpu->opK] = (activeCpu->regX[activeCpu->opK] & LeftMask) | (1 << 31);
        }
    }

static void cp180Op17(Cpu180Context *activeCpu)  // 17  LPAGE      MIGDS 2-139
    {
    u16 asid;
    u32 byteNum;
    u8  count;
    u32 pti;
    u8  xp;

    if (cpu180GetCurrentXp(activeCpu) < 2)
        {
        cpu180SetUserCondition(activeCpu, UCR48);
        return;
        }

    asid    = (activeCpu->regX[activeCpu->opJ] >> 32) & Mask16;
    byteNum = activeCpu->regX[activeCpu->opJ] & Mask32;
    if ((byteNum & 0x80000000) != 0)
        {
        activeCpu->regUtp = ((u64)asid << 32) | byteNum;
        cpu180SetMonitorCondition(activeCpu, MCR52);
        return;
        }
    if (cpu180FindPte(activeCpu, asid, byteNum, TRUE, &pti, &count))
        {
        activeCpu->regX[activeCpu->opK] = (activeCpu->regX[activeCpu->opK] & LeftMask) | ((pti << 3) - activeCpu->regPta);
        activeCpu->regX[1] = (activeCpu->regX[1] & LeftMask) | (1 << 31) | count;
        }
    else
        {
        activeCpu->regX[activeCpu->opK] = (activeCpu->regX[activeCpu->opK] & LeftMask) | ((pti << 3) - activeCpu->regPta);
        activeCpu->regX[1] = (activeCpu->regX[1] & LeftMask) | count;
        }
    }

static void cp180Op18(Cpu180Context *activeCpu)  // 18  IORX       MIGDS 2-34
    {
    activeCpu->regX[activeCpu->opK] |= activeCpu->regX[activeCpu->opJ];
    }

static void cp180Op19(Cpu180Context *activeCpu)  // 19  XORX       MIGDS 2-34
    {
    activeCpu->regX[activeCpu->opK] ^= activeCpu->regX[activeCpu->opJ];
    }

static void cp180Op1A(Cpu180Context *activeCpu)  // 1A  ANDX       MIGDS 2-34
    {
    activeCpu->regX[activeCpu->opK] &= activeCpu->regX[activeCpu->opJ];
    }

static void cp180Op1B(Cpu180Context *activeCpu)  // 1B  NOTX       MIGDS 2-34
    {
    activeCpu->regX[activeCpu->opK] = ~activeCpu->regX[activeCpu->opJ];
    }

static void cp180Op1C(Cpu180Context *activeCpu)  // 1C  INHX       MIGDS 2-35
    {
    activeCpu->regX[activeCpu->opK] &= ~activeCpu->regX[activeCpu->opJ];
    }

static void cp180Op1E(Cpu180Context *activeCpu)  // 1E  MARK       MIGDS 2-37
    {
    static u8 table[16][4] =
        {
        /* j */
        /* 0 */ 0, 0, 0, 0,
        /* 1 */ 0, 0, 0, 1,
        /* 2 */ 0, 0, 1, 0,
        /* 3 */ 0, 0, 1, 1,
        /* 4 */ 0, 1, 0, 0,
        /* 5 */ 0, 1, 0, 1,
        /* 6 */ 0, 1, 1, 0,
        /* 7 */ 0, 1, 1, 1,
        /* 8 */ 1, 0, 0, 0,
        /* 9 */ 1, 0, 0, 1,
        /* A */ 1, 0, 1, 0,
        /* B */ 1, 0, 1, 1,
        /* C */ 1, 1, 0, 0,
        /* D */ 1, 1, 0, 1,
        /* E */ 1, 1, 1, 0,
        /* F */ 1, 1, 1, 1,
        };

    activeCpu->regX[activeCpu->opK] = (u64)table[activeCpu->opJ][(activeCpu->regX[1] >> 30) & Mask2] << 31;
    }

static void cp180Op1F(Cpu180Context *activeCpu)  // 1F  ENTZ/O/S   MIGDS 2-31
    {
    switch (activeCpu->opJ & Mask2)
        {
    case 0:
        activeCpu->regX[activeCpu->opK] &= Mask32;
        break;
    case 1:
        activeCpu->regX[activeCpu->opK] |= 0xffffffff00000000;
        break;
    default:
        if ((activeCpu->regX[activeCpu->opK] & 0x80000000) == 0)
            {
            activeCpu->regX[activeCpu->opK] &= Mask32;
            }
        else
            {
            activeCpu->regX[activeCpu->opK] |= 0xffffffff00000000;
            }
        break;
        }
    }

static void cp180Op20(Cpu180Context *activeCpu)  // 20  ADDR       MIGDS 2-22
    {
    u32 sum;

    if (cpu180AddInt32(activeCpu, activeCpu->regX[activeCpu->opK] & Mask32, activeCpu->regX[activeCpu->opJ] & Mask32, &sum))
        {
        activeCpu->regX[activeCpu->opK] = (activeCpu->regX[activeCpu->opK] & LeftMask) | sum;
        }
    }

static void cp180Op21(Cpu180Context *activeCpu)  // 21  SUBR       MIGDS 2-22
    {
    u32 sum;

    if (cpu180AddInt32(activeCpu, activeCpu->regX[activeCpu->opK] & Mask32, (-activeCpu->regX[activeCpu->opJ]) & Mask32, &sum))
        {
        activeCpu->regX[activeCpu->opK] = (activeCpu->regX[activeCpu->opK] & LeftMask) | sum;
        }
    }

static void cp180Op22(Cpu180Context *activeCpu)  // 22  MULR       MIGDS 2-23
    {
    u32 product;

    if (cpu180MulInt32(activeCpu, activeCpu->regX[activeCpu->opJ] & Mask32, activeCpu->regX[activeCpu->opK] & Mask32, &product))
        {
        activeCpu->regX[activeCpu->opK] = (activeCpu->regX[activeCpu->opK] & LeftMask) | product;
        }
    }

static void cp180Op23(Cpu180Context *activeCpu)  // 23  DIVR       MIGDS 2-23
    {
    cp180OpIv(activeCpu);
    }

static void cp180Op24(Cpu180Context *activeCpu)  // 24  ADDX       MIGDS 2-20
    {
    u64 sum;

    if (cpu180AddInt64(activeCpu, activeCpu->regX[activeCpu->opK], activeCpu->regX[activeCpu->opJ], &sum))
        {
        activeCpu->regX[activeCpu->opK] = sum;
        }
    }

static void cp180Op25(Cpu180Context *activeCpu)  // 25  SUBX       MIGDS 2-20
    {
    u64 sum;

    if (cpu180AddInt64(activeCpu, activeCpu->regX[activeCpu->opK], -activeCpu->regX[activeCpu->opJ], &sum))
        {
        activeCpu->regX[activeCpu->opK] = sum;
        }
    }

static void cp180Op26(Cpu180Context *activeCpu)  // 26  MULX       MIGDS 2-21
    {
    u64 product;

    if (cpu180MulInt64(activeCpu, activeCpu->regX[activeCpu->opK], activeCpu->regX[activeCpu->opJ], &product))
        {
        activeCpu->regX[activeCpu->opK] = product;
        }
    }

static void cp180Op27(Cpu180Context *activeCpu)  // 27  DIVX       MIGDS 2-21
    {
    i64 Xj;

    Xj = activeCpu->regX[activeCpu->opJ];
    if (Xj == 0)
        {
        cpu180SetUserCondition(activeCpu, UCR55);
        }
    else if (Xj == -1)
        {
        cpu180SetUserCondition(activeCpu, UCR57);
        }
    else
        {
        activeCpu->regX[activeCpu->opK] = (i64)activeCpu->regX[activeCpu->opK] / Xj;
        }
    }

static void cp180Op28(Cpu180Context *activeCpu)  // 28  INCR       MIGDS 2-22
    {
    u32 sum;

    if (cpu180AddInt32(activeCpu, activeCpu->regX[activeCpu->opK] & Mask32, activeCpu->opJ, &sum))
        {
        activeCpu->regX[activeCpu->opK] = (activeCpu->regX[activeCpu->opK] & LeftMask) | sum;
        }
    }

static void cp180Op29(Cpu180Context *activeCpu)  // 29  DECR       MIGDS 2-22
    {
    u32 sum;

    if (cpu180AddInt32(activeCpu, activeCpu->regX[activeCpu->opK] & Mask32, -((u32)activeCpu->opJ), &sum))
        {
        activeCpu->regX[activeCpu->opK] = (activeCpu->regX[activeCpu->opK] & LeftMask) | sum;
        }
    }

static void cp180Op2A(Cpu180Context *activeCpu)  // 2A  ADDAX      MIGDS 2-29
    {
    activeCpu->regA[activeCpu->opK] = (activeCpu->regA[activeCpu->opK] & RingSegMask)
        | ((activeCpu->regA[activeCpu->opK] + (activeCpu->regX[activeCpu->opJ] & Mask32)) & Mask32);
    }

static void cp180Op2C(Cpu180Context *activeCpu)  // 2C  CMPR       MIGDS 2-24
    {
    i32 XjR;
    i32 XkR;

    XjR = (activeCpu->opJ == 0) ? 0 : (activeCpu->regX[activeCpu->opJ] & Mask32);
    XkR = (activeCpu->opK == 0) ? 0 : (activeCpu->regX[activeCpu->opK] & Mask32);
    if (XjR == XkR)
        {
        activeCpu->regX[1] &= LeftMask;
        }
    else if (XjR > XkR)
        {
        activeCpu->regX[1] = (activeCpu->regX[1] & LeftMask) | 0x40000000;
        }
    else
        {
        activeCpu->regX[1] = (activeCpu->regX[1] & LeftMask) | 0xc0000000;
        }
    }

static void cp180Op2D(Cpu180Context *activeCpu)  // 2D  CMPX       MIGDS 2-24
    {
    i64 Xj;
    i64 Xk;

    Xj = (activeCpu->opJ == 0) ? 0 : activeCpu->regX[activeCpu->opJ];
    Xk = (activeCpu->opK == 0) ? 0 : activeCpu->regX[activeCpu->opK];
    if (Xj == Xk)
        {
        activeCpu->regX[1] &= LeftMask;
        }
    else if (Xj > Xk)
        {
        activeCpu->regX[1] = (activeCpu->regX[1] & LeftMask) | 0x40000000;
        }
    else
        {
        activeCpu->regX[1] = (activeCpu->regX[1] & LeftMask) | 0xc0000000;
        }
    }

static void cp180Op2E(Cpu180Context *activeCpu)  // 2E  BRREL      MIGDS 2-27
    {
    activeCpu->nextP = (activeCpu->regP & RingSegMask) | ((((activeCpu->regX[activeCpu->opK] << 1) & Mask32) + activeCpu->regP) & Mask32);
    }

static void cp180Op2F(Cpu180Context *activeCpu)  // 2F  BRDIR      MIGDS 2-27
    {
    u64 regA;
    u32 XkR;

    regA = activeCpu->regA[activeCpu->opJ];
    XkR  = (activeCpu->opK == 0) ? 0 : activeCpu->regX[activeCpu->opK] & Mask32;
    activeCpu->nextP   = (activeCpu->regP & RingMask) | (regA & SegMask) | ((regA + (XkR << 1)) & Mask32);
    activeCpu->nextKey = cpu180GetLock(activeCpu, activeCpu->nextP);
    }

static void cp180Op30(Cpu180Context *activeCpu)  // 30  ADDF       MIGDS 2-73
    {
    cp180OpIv(activeCpu);
    }

static void cp180Op31(Cpu180Context *activeCpu)  // 31  SUBF       MIGDS 2-73
    {
    cp180OpIv(activeCpu);
    }

static void cp180Op32(Cpu180Context *activeCpu)  // 32  MULF       MIGDS 2-76
    {
    cp180OpIv(activeCpu);
    }

static void cp180Op33(Cpu180Context *activeCpu)  // 33  DIVF       MIGDS 2-77
    {
    cp180OpIv(activeCpu);
    }

static void cp180Op34(Cpu180Context *activeCpu)  // 34  ADDD       MIGDS 2-79
    {
    cp180OpIv(activeCpu);
    }

static void cp180Op35(Cpu180Context *activeCpu)  // 35  SUBD       MIGDS 2-79
    {
    cp180OpIv(activeCpu);
    }

static void cp180Op36(Cpu180Context *activeCpu)  // 36  MULD       MIGDS 2-82
    {
    cp180OpIv(activeCpu);
    }

static void cp180Op37(Cpu180Context *activeCpu)  // 37  DIVD       MIGDS 2-84
    {
    cp180OpIv(activeCpu);
    }

static void cp180Op39(Cpu180Context *activeCpu)  // 39  ENTX       MIGDS 2-31
    {
    activeCpu->regX[1] = ((u16)activeCpu->opJ << 4) | (u16)activeCpu->opK;
    }

static void cp180Op3A(Cpu180Context *activeCpu)  // 3A  CNIF       MIGDS 2-71
    {
    cp180OpIv(activeCpu);
    }

static void cp180Op3B(Cpu180Context *activeCpu)  // 3B  CNFI       MIGDS 2-72
    {
    cp180OpIv(activeCpu);
    }

static void cp180Op3C(Cpu180Context *activeCpu)  // 3C  CMPF       MIGDS 2-89
    {
    cp180OpIv(activeCpu);
    }

static void cp180Op3D(Cpu180Context *activeCpu)  // 3D  ENTP       MIGDS 2-30
    {
    activeCpu->regX[activeCpu->opK] = activeCpu->opJ;
    }

static void cp180Op3E(Cpu180Context *activeCpu)  // 3E  ENTN       MIGDS 2-30
    {
    activeCpu->regX[activeCpu->opK] = -(i64)activeCpu->opJ;
    }

static void cp180Op3F(Cpu180Context *activeCpu)  // 3F  ENTL       MIGDS 2-31
    {
    activeCpu->regX[0] = ((u16)activeCpu->opJ << 4) | (u16)activeCpu->opK;
    }

static void cp180Op40(Cpu180Context *activeCpu)  // 40  ADDFV      MIGDS 2-209
    {
    cp180OpIv(activeCpu);
    }

static void cp180Op41(Cpu180Context *activeCpu)  // 41  SUBFV      MIGDS 2-209
    {
    cp180OpIv(activeCpu);
    }

static void cp180Op42(Cpu180Context *activeCpu)  // 42  MULFV      MIGDS 2-209
    {
    cp180OpIv(activeCpu);
    }

static void cp180Op43(Cpu180Context *activeCpu)  // 43  DIVFV      MIGDS 2-209
    {
    cp180OpIv(activeCpu);
    }

static void cp180Op44(Cpu180Context *activeCpu)  // 44  ADDXV      MIGDS 2-207
    {
    cp180OpIv(activeCpu);
    }

static void cp180Op45(Cpu180Context *activeCpu)  // 45  SUBXV      MIGDS 2-207
    {
    cp180OpIv(activeCpu);
    }

static void cp180Op48(Cpu180Context *activeCpu)  // 48  IORV       MIGDS 2-209
    {
    cp180OpIv(activeCpu);
    }

static void cp180Op49(Cpu180Context *activeCpu)  // 49  XORV       MIGDS 2-209
    {
    cp180OpIv(activeCpu);
    }

static void cp180Op4A(Cpu180Context *activeCpu)  // 4A  ANDV       MIGDS 2-209
    {
    cp180OpIv(activeCpu);
    }

static void cp180Op4B(Cpu180Context *activeCpu)  // 4B  CNIFV      MIGDS 2-209
    {
    cp180OpIv(activeCpu);
    }

static void cp180Op4C(Cpu180Context *activeCpu)  // 4C  CNFIV      MIGDS 2-209
    {
    cp180OpIv(activeCpu);
    }

static void cp180Op4D(Cpu180Context *activeCpu)  // 4D  SHFV       MIGDS 2-208
    {
    cp180OpIv(activeCpu);
    }

static void cp180Op50(Cpu180Context *activeCpu)  // 50  COMPEQV    MIGDS 2-207
    {
    cp180OpIv(activeCpu);
    }

static void cp180Op51(Cpu180Context *activeCpu)  // 51  CMPLTV     MIGDS 2-207
    {
    cp180OpIv(activeCpu);
    }

static void cp180Op52(Cpu180Context *activeCpu)  // 52  CMPGEV     MIGDS 2-207
    {
    cp180OpIv(activeCpu);
    }

static void cp180Op53(Cpu180Context *activeCpu)  // 53  CMPNEV     MIGDS 2-207
    {
    cp180OpIv(activeCpu);
    }

static void cp180Op54(Cpu180Context *activeCpu)  // 54  MRGV       MIGDS 2-210
    {
    cp180OpIv(activeCpu);
    }

static void cp180Op55(Cpu180Context *activeCpu)  // 55  GTHV       MIGDS 2-210
    {
    cp180OpIv(activeCpu);
    }

static void cp180Op56(Cpu180Context *activeCpu)  // 56  SCTV       MIGDS 2-210
    {
    cp180OpIv(activeCpu);
    }

static void cp180Op57(Cpu180Context *activeCpu)  // 57  SUMFV      MIGDS 2-210
    {
    cp180OpIv(activeCpu);
    }

static void cp180Op58(Cpu180Context *activeCpu)  // 58  TPSFV      MIGDS 2-216
    {
    cp180OpIv(activeCpu);
    }

static void cp180Op59(Cpu180Context *activeCpu)  // 59  TPDFV      MIGDS 2-216
    {
    cp180OpIv(activeCpu);
    }

static void cp180Op5A(Cpu180Context *activeCpu)  // 5A  TSPFV      MIGDS 2-216
    {
    cp180OpIv(activeCpu);
    }

static void cp180Op5B(Cpu180Context *activeCpu)  // 5B  TDPFV      MIGDS 2-216
    {
    cp180OpIv(activeCpu);
    }

static void cp180Op5C(Cpu180Context *activeCpu)  // 5C  SUMPFV     MIGDS 2-216
    {
    cp180OpIv(activeCpu);
    }

static void cp180Op5D(Cpu180Context *activeCpu)  // 5D  GTHIV      MIGDS 2-217
    {
    cp180OpIv(activeCpu);
    }

static void cp180Op5E(Cpu180Context *activeCpu)  // 5E  SCTIV      MIGDS 2-217
    {
    cp180OpIv(activeCpu);
    }

static void cp180Op70(Cpu180Context *activeCpu)  // 70  ADDN       MIGDS 2-47
    {
    cp180OpIv(activeCpu);
    }

static void cp180Op71(Cpu180Context *activeCpu)  // 71  SUBN       MIGDS 2-47
    {
    cp180OpIv(activeCpu);
    }

static void cp180Op72(Cpu180Context *activeCpu)  // 72  MULN       MIGDS 2-47
    {
    cp180OpIv(activeCpu);
    }

static void cp180Op73(Cpu180Context *activeCpu)  // 73  DIVN       MIGDS 2-47
    {
    cp180OpIv(activeCpu);
    }

static void cp180Op74(Cpu180Context *activeCpu)  // 74  CMPN       MIGDS 2-52
    {
    cp180OpIv(activeCpu);
    }

static void cp180Op75(Cpu180Context *activeCpu)  // 75  MOVN       MIGDS 2-51
    {
    cp180OpIv(activeCpu);
    }

static void cp180Op76(Cpu180Context *activeCpu)  // 76  MOVB       MIGDS 2-55
    {
    int count;
    u64 dPva;
    u16 i;
    u16 n;
    u64 sPva;
    u64 word;

    if (cpu180GetBdpDescriptor(activeCpu, activeCpu->nextP, activeCpu->opJ, 0, &activeCpu->srcDesc)
        && cpu180GetBdpDescriptor(activeCpu, activeCpu->nextP + 4, activeCpu->opK, 1, &activeCpu->dstDesc))
        {
        n = (activeCpu->dstDesc.length < activeCpu->srcDesc.length) ? activeCpu->dstDesc.length : activeCpu->srcDesc.length;
        if (n > 256)
            {
            cpu180SetMonitorCondition(activeCpu, MCR51); // instruction specification error
            return;
            }
        sPva = activeCpu->srcDesc.pva;
        dPva = activeCpu->dstDesc.pva;
        //
        //  Optimize the block move by:
        //  1. Move enough bytes to word-align the source PVA
        //  2. Iterate moving 8 bytes (a whole word) per iteration
        //
        if ((sPva & Mask3) != 0) // source not word-aligned, so move enough bytes to word-align it
            {
            count = 8 - (sPva & Mask3);
            if (count > n)
                {
                count = n;
                }
            if (cpu180GetBytes(activeCpu, sPva, count, AccessModeRead, &word) == FALSE
                || cpu180PutBytes(activeCpu, dPva, word, count) == FALSE)
                {
                return;
                }
            sPva += count;
            dPva += count;
            n    -= count;
            }
        while (n > 0) // move a word per iteration
            {
            count = (n >= 8) ? 8 : n;
            if (cpu180GetBytes(activeCpu, sPva, count, AccessModeRead, &word) == FALSE
                || cpu180PutBytes(activeCpu, dPva, word, count) == FALSE)
                {
                return;
                }
            sPva += count;
            dPva += count;
            n    -= count;
            }
        if (activeCpu->dstDesc.length > activeCpu->srcDesc.length)
            {
            count = activeCpu->dstDesc.length - activeCpu->srcDesc.length;
            while (count-- > 0)
                {
                if (cpu180PutByte(activeCpu, dPva++, ' ') == FALSE)
                    {
                    return;
                    }
                }
            }
        activeCpu->nextP += 8;

#if CcDebug == 1
        if ((traceMask & TraceBlockOp) != 0)
            {
            u8 b1, b2;
            char message[80];
            sPva = activeCpu->srcDesc.pva;
            dPva = activeCpu->dstDesc.pva;
            count = activeCpu->srcDesc.length < activeCpu->dstDesc.length ? activeCpu->srcDesc.length : activeCpu->dstDesc.length;
            while (count-- > 0)
                {
                if (cpu180GetByte(activeCpu, sPva++, AccessModeRead, &b1) == FALSE) break;
                if (cpu180GetByte(activeCpu, dPva++, AccessModeWrite, &b2) == FALSE) break;
                if (b1 != b2)
                    {
                    sprintf(message, "Block move error: %012lx %02x != %012lx %02x\n",sPva,b1,dPva,b2);
                    traceCpuPrint(&cpus170[activeCpu->id], message);
                    }
                }
            }
        traceBlockOp(activeCpu);
#endif
        }
    }

static void cp180Op77(Cpu180Context *activeCpu)  // 77  CMPB       MIGDS 2-52
    {
    u8  byte;
    int count;
    u64 diff;
    u64 dPva;
    u64 dstWord;
    u16 i;
    u64 mask;
    u16 n;
    u16 offset;
    u8  result;
    int shift;
    u64 sPva;
    u64 srcWord;

    if (cpu180GetBdpDescriptor(activeCpu, activeCpu->nextP, activeCpu->opJ, 0, &activeCpu->srcDesc)
        && cpu180GetBdpDescriptor(activeCpu, activeCpu->nextP + 4, activeCpu->opK, 1, &activeCpu->dstDesc))
        {
        n = (activeCpu->dstDesc.length < activeCpu->srcDesc.length) ? activeCpu->dstDesc.length : activeCpu->srcDesc.length;
        if (n > 256)
            {
            cpu180SetMonitorCondition(activeCpu, MCR51); // instruction specification error
            return;
            }
        sPva = activeCpu->srcDesc.pva;
        dPva = activeCpu->dstDesc.pva;
        //
        //  Optimize the block compare by:
        //  1. Compare enough bytes to word-align the source PVA
        //  2. Iterate comparing 8 bytes (a whole word) per iteration
        //
        offset = 0;
        result = 0;
        if ((sPva & 7) != 0) // source not word-aligned, so compare enough bytes to word-align it
            {
            count = 8 - (sPva & 7);
            if (count > n)
                {
                count = n;
                }
            if (cpu180GetBytes(activeCpu, sPva, count, AccessModeRead, &srcWord) == FALSE
                || cpu180GetBytes(activeCpu, dPva, count, AccessModeRead, &dstWord) == FALSE)
                {
                return;
                }
            sPva += count;
            dPva += count;
            n    -= count;
            if (srcWord == dstWord)
                {
                offset += count;
                }
            else
                {
                diff  = srcWord ^ dstWord;
                shift = (count - 1) << 3;
                while (shift >= 0)
                    {
                    mask = 0xff << shift;
                    if ((diff & mask) != 0)
                        {
                        result = ((srcWord & mask) > (dstWord & mask)) ? 1 : 3;
                        break;
                        }
                    shift  -= 8;
                    offset += 1;
                    }
                }
            }
        while (n > 0 && result == 0) // compare a word per iteration
            {
            count = (n >= 8) ? 8 : n;
            if (cpu180GetBytes(activeCpu, sPva, count, AccessModeRead, &srcWord) == FALSE
                || cpu180GetBytes(activeCpu, dPva, count, AccessModeRead, &dstWord) == FALSE)
                {
                return;
                }
            sPva += count;
            dPva += count;
            n    -= count;
            if (srcWord == dstWord)
                {
                offset += count;
                }
            else
                {
                diff  = srcWord ^ dstWord;
                shift = (count - 1) << 3;
                while (shift >= 0)
                    {
                    mask = 0xff << shift;
                    if ((diff & mask) != 0)
                        {
                        result = ((srcWord & mask) > (dstWord & mask)) ? 1 : 3;
                        break;
                        }
                    shift  -= 8;
                    offset += 1;
                    }
                }
            }
        if (result == 0)
            {
            if (activeCpu->dstDesc.length < activeCpu->srcDesc.length)
                {
                count = activeCpu->dstDesc.length - activeCpu->srcDesc.length;
                while (count-- > 0)
                    {
                    if (cpu180GetByte(activeCpu, sPva++, AccessModeRead, &byte) == FALSE)
                        {
                        return;
                        }
                    if (byte == ' ')
                        {
                        offset += 1;
                        }
                    else
                        {
                        result = (byte > ' ') ? 1 : 3;
                        break;
                        }
                    }
                }
            else if (activeCpu->dstDesc.length > activeCpu->srcDesc.length)
                {
                count = activeCpu->dstDesc.length - activeCpu->srcDesc.length;
                while (count-- > 0)
                    {
                    if (cpu180GetByte(activeCpu, dPva++, AccessModeRead, &byte) == FALSE)
                        {
                        return;
                        }
                    if (byte == ' ')
                        {
                        offset += 1;
                        }
                    else
                        {
                        result = (byte < ' ') ? 1 : 3;
                        break;
                        }
                    }
                }
            }
        activeCpu->regX[0] = (activeCpu->regX[0] & LeftMask) | offset;
        activeCpu->regX[1] = (activeCpu->regX[1] & LeftMask) | (result << 30);
        activeCpu->nextP += 8;

#if CcDebug == 1
        traceBlockOp(activeCpu);
#endif
        }
    }

static void cp180Op80(Cpu180Context *activeCpu)  // 80  LMULT      MIGDS 2-16
    {
    u8               as;
    u8               at;
    MonitorCondition cond;
    u32              disp;
    u8               i;
    u64              p;
    u32              pageNum;
    u64              pva;
    u8               r1;
    u8               ring;
    u32              rma;
    u16              selector;
    u64              word;
    u32              wordAddrs[32];
    u8               wordCount;
    u8               xs;
    u8               xt;

    pva = activeCpu->regA[activeCpu->opJ];
    if ((pva & Mask3) != 0)
        {
        cpu180SetMonitorCondition(activeCpu, MCR52); // address specification error
        return;
        }
    disp       = ((activeCpu->opQ < 0x8000) ? activeCpu->opQ : 0xffff0000 | activeCpu->opQ) << 3;
    pva        = (pva & RingSegMask) | ((pva + disp) & Mask32);
    selector   = activeCpu->regX[activeCpu->opK];
    as         = selector >> 12;
    xs         = (selector >> 8) & Mask4;
    at         = (selector >> 4) & Mask4;
    xt         = selector & Mask4;
    wordCount  = (at >= as) ? (at - as) + 1 : 0;
    wordCount += (xt >= xs) ? (xt - xs) + 1 : 0;
    if (wordCount < 1)
        {
        return;
        }
    if (cpu180PvaToRma(activeCpu, pva, AccessModeRead, &rma, &cond) == FALSE)
        {
        cpu180SetMonitorCondition(activeCpu, cond);
        return;
        }
    pageNum      = PageOf(pva, activeCpu);
    wordAddrs[0] = rma >> 3;
    p            = pva;
    for (i = 1; i < wordCount; i++)
        {
        p += 8;
        if (PageOf(p, activeCpu) != pageNum)
            {
            if (cpu180PvaToRma(activeCpu, p, AccessModeRead, &rma, &cond) == FALSE)
                {
                cpu180SetMonitorCondition(activeCpu, cond);
                return;
                }
            pageNum = PageOf(p, activeCpu);
            }
        else
            {
            rma += 8;
            }
        wordAddrs[i] = rma >> 3;
        }
    ring = RingOf(activeCpu->regA[activeCpu->opJ]);
    r1   = cpu180GetR1(activeCpu, pva);
    if (r1 > ring)
        {
        ring = r1;
        }
    i = 0;
    while (as <= at)
        {
        word = cpMem[wordAddrs[i++]] & Mask48;
        r1   = RingOf(word);
        if (r1 == 0)
            {
            cpu180SetRingZeroCondition(activeCpu, word);
            }
        if (ring > r1)
            {
            r1 = ring;
            }
        activeCpu->regA[as++] = ((u64)r1 << 44) | (word & Mask44);
        }
    while (xs <= xt)
        {
        activeCpu->regX[xs++] = cpMem[wordAddrs[i++]];
        }
    }

static void cp180Op81(Cpu180Context *activeCpu)  // 81  SMULT      MIGDS 2-16
    {
    u8               as;
    u8               at;
    MonitorCondition cond;
    u32              disp;
    u8               i;
    u32              pageNum;
    u64              pva;
    u32              rma;
    u16              selector;
    u64              word;
    u32              wordAddrs[32];
    u8               wordCount;
    u8               xs;
    u8               xt;

    pva = activeCpu->regA[activeCpu->opJ];
    if ((pva & Mask3) != 0)
        {
        cpu180SetMonitorCondition(activeCpu, MCR52); // address specification error
        return;
        }
    disp       = ((activeCpu->opQ < 0x8000) ? activeCpu->opQ : 0xffff0000 | activeCpu->opQ) << 3;
    pva        = (pva & RingSegMask) | ((pva + disp) & Mask32);
    selector   = activeCpu->regX[activeCpu->opK];
    as         = selector >> 12;
    xs         = (selector >> 8) & Mask4;
    at         = (selector >> 4) & Mask4;
    xt         = selector & Mask4;
    wordCount  = (at >= as) ? (at - as) + 1 : 0;
    wordCount += (xt >= xs) ? (xt - xs) + 1 : 0;
    if (wordCount < 1)
        {
        return;
        }
    if (cpu180PvaToRma(activeCpu, pva, AccessModeWrite, &rma, &cond) == FALSE)
        {
        cpu180SetMonitorCondition(activeCpu, cond);
        return;
        }
    pageNum      = PageOf(pva, activeCpu);
    wordAddrs[0] = rma >> 3;
    for (i = 1; i < wordCount; i++)
        {
        pva += 8;
        if (PageOf(pva, activeCpu) != pageNum)
            {
            if (cpu180PvaToRma(activeCpu, pva, AccessModeWrite, &rma, &cond) == FALSE)
                {
                cpu180SetMonitorCondition(activeCpu, cond);
                return;
                }
            pageNum = PageOf(pva, activeCpu);
            }
        else
            {
            rma += 8;
            }
        wordAddrs[i] = rma >> 3;
        }
    i = 0;
    while (as <= at)
        {
        cpMem[wordAddrs[i++]] = activeCpu->regA[as++];
        }
    while (xs <= xt)
        {
        cpMem[wordAddrs[i++]] = activeCpu->regX[xs++];
        }
    }

static void cp180Op82(Cpu180Context *activeCpu)  // 82  LX         MIGDS 2-12
    {
    MonitorCondition cond;
    u64              pva;
    u32              rma;

    pva = activeCpu->regA[activeCpu->opJ];
    if ((pva & Mask3) != 0)
        {
        activeCpu->regUtp = pva;
        cpu180SetMonitorCondition(activeCpu, MCR52);
        return;
        }
    if (activeCpu->opQ < 0x8000)
        {
        pva += (u32)activeCpu->opQ << 3;
        }
    else
        {
        pva = (pva & RingSegMask) | ((pva + ((0x1fff0000 | (u32)activeCpu->opQ) << 3)) & Mask32);
        }
    if (cpu180PvaToRma(activeCpu, pva, AccessModeRead, &rma, &cond))
        {
        activeCpu->regX[activeCpu->opK] = cpMem[rma >> 3];
        }
    else
        {
        cpu180SetMonitorCondition(activeCpu, cond);
        }
    }

static void cp180Op83(Cpu180Context *activeCpu)  // 83  SX         MIGDS 2-12
    {
    MonitorCondition cond;
    u64              pva;
    u32              rma;

    pva = activeCpu->regA[activeCpu->opJ];
    if ((pva & Mask3) != 0)
        {
        activeCpu->regUtp = pva;
        cpu180SetMonitorCondition(activeCpu, MCR52);
        return;
        }
    if (activeCpu->opQ < 0x8000)
        {
        pva += (u32)activeCpu->opQ << 3;
        }
    else
        {
        pva = (pva & RingSegMask) | ((pva + ((0x1fff0000 | (u32)activeCpu->opQ) << 3)) & Mask32);
        }
    if (cpu180PvaToRma(activeCpu, pva, AccessModeWrite, &rma, &cond))
        {
        cpMem[rma >> 3] = activeCpu->regX[activeCpu->opK];
        }
    else
        {
        cpu180SetMonitorCondition(activeCpu, cond);
        }
    }

static void cp180Op84(Cpu180Context *activeCpu)  // 84  LA         MIGDS 2-15
    {
    u64 addr;
    u32 disp;
    u64 pva;
    u64 r1;
    u64 r2;

    disp = (activeCpu->opQ < 0x8000) ? activeCpu->opQ : 0xffff0000 | activeCpu->opQ;
    pva  = (activeCpu->regA[activeCpu->opJ] & RingSegMask) | ((activeCpu->regA[activeCpu->opJ] + disp) & Mask32);
    if (cpu180GetBytes(activeCpu, pva, 6, AccessModeRead, &addr))
        {
        r1 = addr & RingMask;
        if (r1 == 0)
            {
            cpu180SetRingZeroCondition(activeCpu, addr);
            }
        r2 = activeCpu->regA[activeCpu->opJ] & RingMask;
        if (r2 > r1)
            {
            r1 = r2;
            }
        r2 = (u64)cpu180GetR1(activeCpu, pva) << 44;
        if (r2 > r1)
            {
            r1 = r2;
            }
        activeCpu->regA[activeCpu->opK] = r1 | (addr & Mask44);
        }
    }

static void cp180Op85(Cpu180Context *activeCpu)  // 85  SA         MIGDS 2-15
    {
    u32 disp;
    u64 pva;

    disp = (activeCpu->opQ < 0x8000) ? activeCpu->opQ : 0xffff0000 | activeCpu->opQ;
    pva  = (activeCpu->regA[activeCpu->opJ] & RingSegMask) | ((activeCpu->regA[activeCpu->opJ] + disp) & Mask32);
    (void)cpu180PutBytes(activeCpu, pva, activeCpu->regA[activeCpu->opK], 6);
    }

static void cp180Op86(Cpu180Context *activeCpu)  // 86  LBYTP,j    MIGDS 2-13
    {
    cp180OpIv(activeCpu);
    }

static void cp180Op87(Cpu180Context *activeCpu)  // 87  ENTC       MIGDS 2-31
    {
    activeCpu->regX[1] = ((u64)activeCpu->opJ << 20) | ((u64)activeCpu->opK << 16) | (u64)activeCpu->opQ;
    if (activeCpu->opJ > 7)
        {
        activeCpu->regX[1] |= 0xffffffffff000000;
        }
    }

static void cp180Op88(Cpu180Context *activeCpu)  // 88  LBIT       MIGDS 2-14
    {
    u8  byte;
    u32 offset;
    u64 pva;
    u32 q;

    offset   = activeCpu->regX[0];
    offset >>= 3;
    if (offset > 0x0fffffff)
        {
        offset |= 0xf0000000;
        }
    q   = (activeCpu->opQ < 0x8000) ? activeCpu->opQ : 0xffff0000 | activeCpu->opQ;
    pva = (activeCpu->regA[activeCpu->opJ] & RingSegMask) | ((activeCpu->regA[activeCpu->opJ] + offset + q) & Mask32);
    if (cpu180GetByte(activeCpu, pva, AccessModeRead, &byte) == FALSE)
        {
        return;
        }
    activeCpu->regX[activeCpu->opK] = (byte >> (7 - (activeCpu->regX[0] & 7))) & 1;
    }

static void cp180Op89(Cpu180Context *activeCpu)  // 89  SBIT       MIGDS 2-14
    {
    MonitorCondition cond;
    u64              mask;
    u32              offset;
    u64              pva;
    u32              q;
    u32              rma;
    u8               shift;
    u32              wordAddr;

    offset   = activeCpu->regX[0];
    offset >>= 3;
    if (offset > 0x0fffffff)
        {
        offset |= 0xf0000000;
        }
    q   = (activeCpu->opQ < 0x8000) ? activeCpu->opQ : 0xffff0000 | activeCpu->opQ;
    pva = (activeCpu->regA[activeCpu->opJ] & RingSegMask) | ((activeCpu->regA[activeCpu->opJ] + offset + q) & Mask32);
    if (cpu180PvaToRma(activeCpu, pva, AccessModeWrite, &rma, &cond))
        {
        wordAddr         = rma >> 3;
        shift            = (56 - ((rma & 7) << 3)) + (7 - (activeCpu->regX[0] & 7));
        mask             = ~((u64)1 << shift);
        cpuAcquireMemoryMutex();
        cpMem[wordAddr] = (cpMem[wordAddr] & mask) | (activeCpu->regX[activeCpu->opK] & 1) << shift;
        cpuReleaseMemoryMutex();
        }
    else
        {
        cpu180SetMonitorCondition(activeCpu, cond);
        }
    }

static void cp180Op8A(Cpu180Context *activeCpu)  // 8A  ADDRQ      MIGDS 2-22
    {
    u32 sum;

    if (cpu180AddInt32(activeCpu, activeCpu->regX[activeCpu->opJ] & Mask32,
        (activeCpu->opQ < 0x8000) ? activeCpu->opQ : 0xffff0000 | activeCpu->opQ, &sum))
        {
        activeCpu->regX[activeCpu->opK] = (activeCpu->regX[activeCpu->opK] & LeftMask) | sum;
        }
    }

static void cp180Op8B(Cpu180Context *activeCpu)  // 8B  ADDXQ      MIGDS 2-20
    {
    u64 sum;

    if (cpu180AddInt64(activeCpu, activeCpu->regX[activeCpu->opJ],
        (activeCpu->opQ < 0x8000) ? activeCpu->opQ : 0xffffffffffff0000 | activeCpu->opQ, &sum))
        {
        activeCpu->regX[activeCpu->opK] = sum;
        }
    }

static void cp180Op8C(Cpu180Context *activeCpu)  // 8C  MULRQ      MIGDS 2-23
    {
    u32 product;

    if (cpu180MulInt32(activeCpu, activeCpu->regX[activeCpu->opJ] & Mask32,
        (activeCpu->opQ < 0x8000) ? activeCpu->opQ : 0xffff0000 | activeCpu->opQ, &product))
        {
        activeCpu->regX[activeCpu->opK] = (activeCpu->regX[activeCpu->opK] & LeftMask) | product;
        }
    }

static void cp180Op8D(Cpu180Context *activeCpu)  // 8D  ENTE       MIGDS 2-30
    {
    if (activeCpu->opQ < 0x8000)
        {
        activeCpu->regX[activeCpu->opK] = activeCpu->opQ;
        }
    else
        {
        activeCpu->regX[activeCpu->opK] = 0xffffffffffff0000 | (u64)activeCpu->opQ;
        }
    }

static void cp180Op8E(Cpu180Context *activeCpu)  // 8E  ADDAQ      MIGDS 2-29
    {
    u32 disp;

    disp = (activeCpu->opQ < 0x8000) ? activeCpu->opQ : 0xffff0000 | activeCpu->opQ;
    activeCpu->regA[activeCpu->opK] = (activeCpu->regA[activeCpu->opJ] & RingSegMask) | ((activeCpu->regA[activeCpu->opJ] + disp) & Mask32);
    }

static void cp180Op8F(Cpu180Context *activeCpu)  // 8F  ADDPXQ     MIGDS 2-29
    {
    u32 disp;
    i32 XjR;

    XjR  = (((activeCpu->opJ == 0) ? 0 : activeCpu->regX[activeCpu->opJ]) << 1) & Mask32;
    disp = ((activeCpu->opQ < 0x8000) ? activeCpu->opQ : 0x7fff0000 | activeCpu->opQ) << 1;
    activeCpu->regA[activeCpu->opK] = (activeCpu->regP & RingSegMask) | ((activeCpu->regP + XjR + disp) & Mask32);
    }

static void cp180Op90(Cpu180Context *activeCpu)  // 90  BRREQ      MIGDS 2-25
    {
    u32 disp;
    i32 XjR;
    i32 XkR;

    XjR = (activeCpu->opJ == 0) ? 0 : (activeCpu->regX[activeCpu->opJ] & Mask32);
    XkR = (activeCpu->opK == 0) ? 0 : (activeCpu->regX[activeCpu->opK] & Mask32);
    if (XjR == XkR)
        {
        disp = ((activeCpu->opQ < 0x8000) ? activeCpu->opQ : 0x7fff0000 | activeCpu->opQ) << 1;
        activeCpu->nextP = (activeCpu->regP & RingSegMask) | ((activeCpu->regP + disp) & Mask32);
        }
    }

static void cp180Op91(Cpu180Context *activeCpu)  // 91  BRRNE      MIGDS 2-25
    {
    u32 disp;
    i32 XjR;
    i32 XkR;

    XjR = (activeCpu->opJ == 0) ? 0 : (activeCpu->regX[activeCpu->opJ] & Mask32);
    XkR = (activeCpu->opK == 0) ? 0 : (activeCpu->regX[activeCpu->opK] & Mask32);
    if (XjR != XkR)
        {
        disp = ((activeCpu->opQ < 0x8000) ? activeCpu->opQ : 0x7fff0000 | activeCpu->opQ) << 1;
        activeCpu->nextP = (activeCpu->regP & RingSegMask) | ((activeCpu->regP + disp) & Mask32);
        }
    }

static void cp180Op92(Cpu180Context *activeCpu)  // 92  BRRGT      MIGDS 2-25
    {
    u32 disp;
    i32 XjR;
    i32 XkR;

    XjR = (activeCpu->opJ == 0) ? 0 : (activeCpu->regX[activeCpu->opJ] & Mask32);
    XkR = (activeCpu->opK == 0) ? 0 : (activeCpu->regX[activeCpu->opK] & Mask32);
    if (XjR > XkR)
        {
        disp = ((activeCpu->opQ < 0x8000) ? activeCpu->opQ : 0x7fff0000 | activeCpu->opQ) << 1;
        activeCpu->nextP = (activeCpu->regP & RingSegMask) | ((activeCpu->regP + disp) & Mask32);
        }
    }

static void cp180Op93(Cpu180Context *activeCpu)  // 93  BRRGE      MIGDS 2-25
    {
    u32 disp;
    i32 XjR;
    i32 XkR;

    XjR = (activeCpu->opJ == 0) ? 0 : (activeCpu->regX[activeCpu->opJ] & Mask32);
    XkR = (activeCpu->opK == 0) ? 0 : (activeCpu->regX[activeCpu->opK] & Mask32);
    if (XjR >= XkR)
        {
        disp = ((activeCpu->opQ < 0x8000) ? activeCpu->opQ : 0x7fff0000 | activeCpu->opQ) << 1;
        activeCpu->nextP = (activeCpu->regP & RingSegMask) | ((activeCpu->regP + disp) & Mask32);
        }
    }

static void cp180Op94(Cpu180Context *activeCpu)  // 94  BRXEQ      MIGDS 2-25
    {
    u32 disp;
    i64 Xj;
    i64 Xk;

    Xj = (activeCpu->opJ == 0) ? 0 : activeCpu->regX[activeCpu->opJ];
    Xk = (activeCpu->opK == 0) ? 0 : activeCpu->regX[activeCpu->opK];
    if (Xj == Xk)
        {
        disp = ((activeCpu->opQ < 0x8000) ? activeCpu->opQ : 0x7fff0000 | activeCpu->opQ) << 1;
        activeCpu->nextP = (activeCpu->regP & RingSegMask) | ((activeCpu->regP + disp) & Mask32);
        }
    }

static void cp180Op95(Cpu180Context *activeCpu)  // 95  BRXNE      MIGDS 2-25
    {
    u32 disp;
    i64 Xj;
    i64 Xk;

    Xj = (activeCpu->opJ == 0) ? 0 : activeCpu->regX[activeCpu->opJ];
    Xk = (activeCpu->opK == 0) ? 0 : activeCpu->regX[activeCpu->opK];
    if (Xj != Xk)
        {
        disp = ((activeCpu->opQ < 0x8000) ? activeCpu->opQ : 0x7fff0000 | activeCpu->opQ) << 1;
        activeCpu->nextP = (activeCpu->regP & RingSegMask) | ((activeCpu->regP + disp) & Mask32);
        }
    }

static void cp180Op96(Cpu180Context *activeCpu)  // 96  BRXGT      MIGDS 2-25
    {
    u32 disp;
    i64 Xj;
    i64 Xk;

    Xj = (activeCpu->opJ == 0) ? 0 : activeCpu->regX[activeCpu->opJ];
    Xk = (activeCpu->opK == 0) ? 0 : activeCpu->regX[activeCpu->opK];
    if (Xj > Xk)
        {
        disp = ((activeCpu->opQ < 0x8000) ? activeCpu->opQ : 0x7fff0000 | activeCpu->opQ) << 1;
        activeCpu->nextP = (activeCpu->regP & RingSegMask) | ((activeCpu->regP + disp) & Mask32);
        }
    }

static void cp180Op97(Cpu180Context *activeCpu)  // 97  BRXGE      MIGDS 2-25
    {
    u32 disp;
    i64 Xj;
    i64 Xk;

    Xj = (activeCpu->opJ == 0) ? 0 : activeCpu->regX[activeCpu->opJ];
    Xk = (activeCpu->opK == 0) ? 0 : activeCpu->regX[activeCpu->opK];
    if (Xj >= Xk)
        {
        disp = ((activeCpu->opQ < 0x8000) ? activeCpu->opQ : 0x7fff0000 | activeCpu->opQ) << 1;
        activeCpu->nextP = (activeCpu->regP & RingSegMask) | ((activeCpu->regP + disp) & Mask32);
        }
    }

static void cp180Op98(Cpu180Context *activeCpu)  // 98  BRFEQ      MIGDS 2-87
    {
    cp180OpIv(activeCpu);
    }

static void cp180Op99(Cpu180Context *activeCpu)  // 99  BRFNE      MIGDS 2-87
    {
    cp180OpIv(activeCpu);
    }

static void cp180Op9A(Cpu180Context *activeCpu)  // 9A  BRFGT      MIGDS 2-87
    {
    cp180OpIv(activeCpu);
    }

static void cp180Op9B(Cpu180Context *activeCpu)  // 9B  BRFGE      MIGDS 2-87
    {
    cp180OpIv(activeCpu);
    }

static void cp180Op9C(Cpu180Context *activeCpu)  // 9C  BRINC      MIGDS 2-26
    {
    u32 disp;
    i64 Xj;

    Xj = (activeCpu->opJ == 0) ? 0 : (i64)activeCpu->regX[activeCpu->opJ];
    if (Xj > (i64)activeCpu->regX[activeCpu->opK])
        {
        disp = ((activeCpu->opQ < 0x8000) ? activeCpu->opQ : 0x7fff0000 | activeCpu->opQ) << 1;
        activeCpu->nextP = (activeCpu->regP & RingSegMask) | ((activeCpu->regP + disp) & Mask32);
        activeCpu->regX[activeCpu->opK] += 1;
        }
    }

static void cp180Op9D(Cpu180Context *activeCpu)  // 9D  BRSEG      MIGDS 2-26
    {
    u64 Aj;
    u64 Ak;
    i32 bnj;
    i32 bnk;
    u32 disp;

    Aj = activeCpu->regA[activeCpu->opJ];
    Ak = activeCpu->regA[activeCpu->opK];
    if ((Aj & SegMask) != (Ak & SegMask))
        {
        disp = ((activeCpu->opQ < 0x8000) ? activeCpu->opQ : 0x7fff0000 | activeCpu->opQ) << 1;
        activeCpu->nextP = (activeCpu->regP & RingSegMask) | ((activeCpu->regP + disp) & Mask32);
        }
    else
        {
        bnj = Aj & Mask32;
        bnk = Ak & Mask32;
        if (bnj == bnk)
            {
            activeCpu->regX[1] &= LeftMask;
            }
        else if (bnj > bnk)
            {
            activeCpu->regX[1] = (activeCpu->regX[1] & LeftMask) | (u64)0x40000000;
            }
        else
            {
            activeCpu->regX[1] = (activeCpu->regX[1] & LeftMask) | (u64)0xc0000000;
            }
        }
    }

static void cp180Op9E(Cpu180Context *activeCpu)  // 9E  BR---      MIGDS 2-88
    {
    cp180OpIv(activeCpu);
    }

static void cp180Op9F(Cpu180Context *activeCpu)  // 9F  BRCR       MIGDS 2-142
    {
    u64 brExit;
    u32 disp;
    u16 mask;

    mask   = bitSelectors[activeCpu->opJ];
    disp   = ((activeCpu->opQ < 0x8000) ? activeCpu->opQ : 0x7fff0000 | activeCpu->opQ) << 1;
    brExit = (activeCpu->regP & RingSegMask) | ((activeCpu->regP + disp) & Mask32);

    switch (activeCpu->opK & Mask3)
        {
    default:
    case 0:
        if (activeCpu->isMonitorMode == FALSE)
            {
            cpu180SetMonitorCondition(activeCpu, MCR51);
            return;
            }
        if ((activeCpu->regMcr & mask) != 0)
            {
            activeCpu->regMcr &= ~mask;
            activeCpu->nextP   = brExit;
            }
        break;
    case 1:
        if (activeCpu->isMonitorMode == FALSE)
            {
            cpu180SetMonitorCondition(activeCpu, MCR51);
            return;
            }
        if ((activeCpu->regMcr & mask) == 0)
            {
            activeCpu->regMcr |= mask;
            cpu180CheckMonitorConditions(activeCpu);
            activeCpu->regP  = brExit;
            activeCpu->nextP = activeCpu->regP;
            }
        break;
    case 2:
        if ((activeCpu->regMcr & mask) != 0)
            {
            activeCpu->nextP = brExit;
            }
        break;
    case 3:
        if ((activeCpu->regMcr & mask) == 0)
            {
            activeCpu->nextP = brExit;
            }
        break;
    case 4:
        if ((activeCpu->regUcr & mask) != 0)
            {
            activeCpu->regUcr &= ~mask;
            activeCpu->nextP   = brExit;
            }
        break;
    case 5:
        if ((activeCpu->regUcr & mask) == 0)
            {
            activeCpu->regUcr |= mask;
            cpu180CheckUserConditions(activeCpu);
            activeCpu->regP  = brExit;
            activeCpu->nextP = activeCpu->regP;
            }
        break;
    case 6:
        if ((activeCpu->regUcr & mask) != 0)
            {
            activeCpu->nextP = brExit;
            }
        break;
    case 7:
        if ((activeCpu->regUcr & mask) == 0)
            {
            activeCpu->nextP = brExit;
            }
        break;
        }
    }

static void cp180OpA0(Cpu180Context *activeCpu)  // A0  LAI        MIGDS 2-15
    {
    u64 addr;
    u32 disp;
    u64 pva;
    u64 r1;
    u64 r2;

    pva = activeCpu->regA[activeCpu->opJ] + activeCpu->opD;
    if (activeCpu->opI != 0)
        {
        pva = (pva & RingSegMask) | ((pva + (activeCpu->regX[activeCpu->opI] & Mask32)) & Mask32);
        }
    if (cpu180GetBytes(activeCpu, pva, 6, AccessModeRead, &addr))
        {
        r1 = addr & RingMask;
        if (r1 == 0)
            {
            cpu180SetRingZeroCondition(activeCpu, addr);
            }
        r2 = activeCpu->regA[activeCpu->opJ] & RingMask;
        if (r2 > r1)
            {
            r1 = r2;
            }
        r2 = (u64)cpu180GetR1(activeCpu, pva) << 44;
        if (r2 > r1)
            {
            r1 = r2;
            }
        activeCpu->regA[activeCpu->opK] = r1 | (addr & Mask44);
        }
    }

static void cp180OpA1(Cpu180Context *activeCpu)  // A1  SAI        MIGDS 2-15
    {
    u64 pva;

    pva = activeCpu->regA[activeCpu->opJ] + activeCpu->opD;
    if (activeCpu->opI != 0)
        {
        pva = (pva & RingSegMask) | ((pva + (activeCpu->regX[activeCpu->opI] & Mask32)) & Mask32);
        }
    (void)cpu180PutBytes(activeCpu, pva, activeCpu->regA[activeCpu->opK], 6);
    }

static void cp180OpA2(Cpu180Context *activeCpu)  // A2  LXI        MIGDS 2-12
    {
    MonitorCondition cond;
    u64              pva;
    u32              rma;

    pva = activeCpu->regA[activeCpu->opJ];
    if ((pva & Mask3) != 0)
        {
        activeCpu->regUtp = pva;
        cpu180SetMonitorCondition(activeCpu, MCR52);
        return;
        }
    pva += activeCpu->opD << 3;
    if (activeCpu->opI != 0)
        {
        pva = (pva & RingSegMask) | ((pva + ((activeCpu->regX[activeCpu->opI] & Mask32) << 3)) & Mask32);
        }
    if (cpu180PvaToRma(activeCpu, pva, AccessModeRead, &rma, &cond))
        {
        activeCpu->regX[activeCpu->opK] = cpMem[rma >> 3];
        }
    else
        {
        cpu180SetMonitorCondition(activeCpu, cond);
        }
    }

static void cp180OpA3(Cpu180Context *activeCpu)  // A3  SXI        MIGDS 2-12
    {
    MonitorCondition cond;
    u64              pva;
    u32              rma;

    pva = activeCpu->regA[activeCpu->opJ];
    if ((pva & Mask3) != 0)
        {
        activeCpu->regUtp = pva;
        cpu180SetMonitorCondition(activeCpu, MCR52);
        return;
        }
    pva += activeCpu->opD << 3;
    if (activeCpu->opI != 0)
        {
        pva = (pva & RingSegMask) | ((pva + ((activeCpu->regX[activeCpu->opI] & Mask32) << 3)) & Mask32);
        }
    if (cpu180PvaToRma(activeCpu, pva, AccessModeWrite, &rma, &cond))
        {
        cpMem[rma >> 3] = activeCpu->regX[activeCpu->opK];
        }
    else
        {
        cpu180SetMonitorCondition(activeCpu, cond);
        }
    }

static void cp180OpA4(Cpu180Context *activeCpu)  // A4  LBYT,X0    MIGDS 2-13
    {
    cp180OpIv(activeCpu);
    }

static void cp180OpA5(Cpu180Context *activeCpu)  // A5  SBYT,X0    MIGDS 2-13
    {
    cp180OpIv(activeCpu);
    }

static void cp180OpA7(Cpu180Context *activeCpu)  // A7  ADDAD      MIGDS 2-30
    {
    u32 bn;
    u64 mask;

    bn   = ((activeCpu->regA[activeCpu->opI] & Mask32) + activeCpu->opD) & Mask32;
    mask = 0xfffffffffff8 | (activeCpu->opJ & Mask3);
    activeCpu->regA[activeCpu->opK] = ((activeCpu->regA[activeCpu->opI] & RingSegMask) | bn) & mask;
    }

static void cp180OpA8(Cpu180Context *activeCpu)  // A8  SHFC       MIGDS 2-33
    {
    u64 mask;
    u64 rightBits;
    i8  shift;

    shift = (((activeCpu->opI == 0) ? 0 : activeCpu->regX[activeCpu->opI] & Mask8) + (activeCpu->opD & Mask8)) & Mask8;
    if (shift > 0)
        {
        shift = 64 - (shift & Mask6);
        }
    else if (shift < 0)
        {
        shift = -(shift | 0x40);
        }
    else
        {
        activeCpu->regX[activeCpu->opK] = activeCpu->regX[activeCpu->opJ];
        return;
        }
    rightBits = activeCpu->regX[activeCpu->opJ] & bitMasks[shift - 1];
    activeCpu->regX[activeCpu->opK] = (rightBits << (64 - shift)) | (activeCpu->regX[activeCpu->opJ] >> shift);
    }

static void cp180OpA9(Cpu180Context *activeCpu)  // A9  SHFX       MIGDS 2-33
    {
    i8 shift;

    shift = (((activeCpu->opI == 0) ? 0 : activeCpu->regX[activeCpu->opI] & Mask8) + (activeCpu->opD & Mask8)) & Mask8;
    if (shift >= 0)
        {
        activeCpu->regX[activeCpu->opK] = activeCpu->regX[activeCpu->opJ] << (shift & Mask6);
        }
    else
        {
        activeCpu->regX[activeCpu->opK] = activeCpu->regX[activeCpu->opJ] >> (-(shift | 0x40));
        }
    }

static void cp180OpAA(Cpu180Context *activeCpu)  // AA  SHFR       MIGDS 2-33
    {
    i8  shift;
    u32 XjR;

    XjR   = activeCpu->regX[activeCpu->opJ] & Mask32;
    shift = (((activeCpu->opI == 0) ? 0 : activeCpu->regX[activeCpu->opI] & Mask8) + (activeCpu->opD & Mask8)) & Mask8;
    if (shift >= 0)
        {
        activeCpu->regX[activeCpu->opK] = (activeCpu->regX[activeCpu->opK] & LeftMask) | (u64)(XjR << (shift & Mask5));
        }
    else
        {
        activeCpu->regX[activeCpu->opK] = (activeCpu->regX[activeCpu->opK] & LeftMask) | (u64)(XjR >> (-(shift | 0x60)));
        }
    }

static void cp180OpAC(Cpu180Context *activeCpu)  // AC  ISOM       MIGDS 2-36
    {
    u32 desc;
    u8  first;
    u8  length;

    desc = (((activeCpu->opI == 0) ? 0 : activeCpu->regX[activeCpu->opI] & Mask32) + activeCpu->opD) & Mask12;
    first  = desc >> 6;
    length = desc & Mask6;
    if (first + length < 64)
        {
        activeCpu->regX[activeCpu->opK] = bitMasks[length] << ((63 - first) - length);
        }
    else
        {
        cpu180SetMonitorCondition(activeCpu, MCR51);
        }
    }

static void cp180OpAD(Cpu180Context *activeCpu)  // AD  ISOB       MIGDS 2-36
    {
    u32 desc;
    u8  first;
    u8  length;
    u8  shift;

    desc = (((activeCpu->opI == 0) ? 0 : activeCpu->regX[activeCpu->opI] & Mask32) + activeCpu->opD) & Mask12;
    first  = desc >> 6;
    length = desc & Mask6;
    if (first + length < 64)
        {
        shift = (63 - first) - length;
        activeCpu->regX[activeCpu->opK] = (activeCpu->regX[activeCpu->opJ] & (bitMasks[length] << shift)) >> shift;
        }
    else
        {
        cpu180SetMonitorCondition(activeCpu, MCR51);
        }
    }

static void cp180OpAE(Cpu180Context *activeCpu)  // AE  INSB       MIGDS 2-36
    {
    u64 bits;
    u32 desc;
    u8  first;
    u8  length;
    u64 mask;
    u8  shift;

    desc = (((activeCpu->opI == 0) ? 0 : activeCpu->regX[activeCpu->opI] & Mask32) + activeCpu->opD) & Mask12;
    first  = desc >> 6;
    length = desc & Mask6;
    if (first + length < 64)
        {
        shift = (63 - first) - length;
        mask  = bitMasks[length] << shift;
        bits  = (activeCpu->regX[activeCpu->opJ] & bitMasks[length]) << shift;
        activeCpu->regX[activeCpu->opK] = (activeCpu->regX[activeCpu->opK] & ~mask) | bits;
        }
    else
        {
        cpu180SetMonitorCondition(activeCpu, MCR51);
        }
    }

static void cp180OpB0(Cpu180Context *activeCpu)  // B0  CALLREL    MIGDS 2-125
    {
    u8               at;
    u64              callee;
    u32              disp;
    u32              frameSize;
    u8               ring;
    u32              rma;
    u64              sfsa;
    u8               xs;
    u8               xt;

    disp   = ((activeCpu->opQ < 0x8000) ? activeCpu->opQ : 0xffff0000 | activeCpu->opQ) << 3;
    callee = (activeCpu->regP & RingSegMask) | ((activeCpu->regP + disp) & 0xfffffff8);
    xs     = (activeCpu->regX[0] >> 8) & Mask4;
    at     = (activeCpu->regX[0] >> 4) & Mask4;
    xt     = activeCpu->regX[0] & Mask4;
    if (cpu180PushFrame(activeCpu, at, xs, xt, FALSE, &sfsa, &frameSize) == FALSE)
        {
        return;
        }

#if CcDebug == 1
    traceCallFrame(activeCpu, sfsa);
#endif

    activeCpu->regA[2]          = activeCpu->regA[0];
    activeCpu->regA[0]         += frameSize;
    activeCpu->regA[1]          = activeCpu->regA[0];
    ring                        = RingOf(activeCpu->regP);
    activeCpu->regTos[ring - 1] = activeCpu->regA[0];
    activeCpu->regFlags        &= 0x3fff; // clear CCF and OCF
    activeCpu->regA[3]          = activeCpu->regA[activeCpu->opJ];
    activeCpu->regA[4]          = activeCpu->regA[activeCpu->opK];
    activeCpu->nextP            = callee;

#if CcDebug == 1
    traceCall(activeCpu, activeCpu->nextP);
#endif
    }

static void cp180OpB1(Cpu180Context *activeCpu)  // B1  KEYPOINT   MIGDS 2-133
    {
    MonitorCondition cond;
    u32              disp;
    u64              kpe;
    u16              mask;
    u32              rma;
    u32              XkR;

    mask = bitSelectors[activeCpu->opJ];
    if ((activeCpu->regKmr & mask) != 0 && (activeCpu->regFlags & (1 << 13)) != 0)
        {
        XkR  = (activeCpu->opK == 0) ? 0 : activeCpu->regX[activeCpu->opK];
        disp = (activeCpu->opQ < 0x8000) ? activeCpu->opQ : 0xffff0000 | activeCpu->opQ;
        kpe  = ((cpu180FreeRunningCounter & Mask28) << 36) | ((u64)activeCpu->opJ << 32) | ((XkR + disp) & Mask32);
        if (cpu180PvaToRma(activeCpu, activeCpu->regKbp, AccessModeWrite, &rma, &cond) == FALSE)
            {
            cpu180SetMonitorCondition(activeCpu, cond);
            return;
            }
        cpMem[rma >> 3]    = kpe;
        activeCpu->regKbp += 8;
        }
    }

static void cp180OpB2(Cpu180Context *activeCpu)  // B2  MULXQ      MIGDS 2-21
    {
    u64 product;

    if (cpu180MulInt64(activeCpu, activeCpu->regX[activeCpu->opJ],
        (activeCpu->opQ < 0x8000) ? activeCpu->opQ : 0xffffffffffff0000 | activeCpu->opQ, &product))
        {
        activeCpu->regX[activeCpu->opK] = product;
        }
    }

static void cp180OpB3(Cpu180Context *activeCpu)  // B3  ENTA       MIGDS 2-31
    {
    activeCpu->regX[0] = ((u64)activeCpu->opJ << 20) | ((u64)activeCpu->opK << 16) | (u64)activeCpu->opQ;
    if (activeCpu->opJ > 7)
        {
        activeCpu->regX[0] |= 0xffffffffff000000;
        }
    }

static void cp180OpB4(Cpu180Context *activeCpu)  // B4  CMPXA      MIGDS 2-134
    {
    MonitorCondition cond;
    u32              disp;
    u64              pva;
    u32              rma;
    u64              word;
    u32              wordAddr;
    u64              Xk;

    pva = activeCpu->regA[activeCpu->opJ];
    if ((pva & Mask3) != 0)
        {
        cpu180SetMonitorCondition(activeCpu, MCR52); // address specification error
        return;
        }
    if (activeCpu->regX[0] == LeftMask)
        {
        cpu180SetMonitorCondition(activeCpu, MCR51); // instruction specification error
        return;
        }
    if (cpu180PvaToRma(activeCpu, pva, AccessModeRead | AccessModeWrite, &rma, &cond) == FALSE)
        {
        cpu180SetMonitorCondition(activeCpu, cond);
        return;
        }
    disp     = ((activeCpu->opQ < 0x8000) ? activeCpu->opQ : 0x7fff0000 | activeCpu->opQ) << 1;
    wordAddr = rma >> 3;
    Xk       = activeCpu->regX[activeCpu->opK];
    cpuAcquireMemoryMutex();
    word = cpMem[wordAddr];
    if ((word & LeftMask) == LeftMask)
        {
        activeCpu->nextP = (activeCpu->regP & RingSegMask) | ((activeCpu->regP + disp) & Mask32);
        }
    else if (Xk == word)
        {
        cpMem[wordAddr]     = activeCpu->regX[0];
        activeCpu->regX[1] &= LeftMask;
        }
    else
        {
        activeCpu->regX[activeCpu->opK] = word;
        if ((i64)Xk > (i64)word)
            {
            activeCpu->regX[1] = (activeCpu->regX[1] & LeftMask) | (u64)0x40000000;
            }
        else
            {
            activeCpu->regX[1] = (activeCpu->regX[1] & LeftMask) | (u64)0xc0000000;
            }
        }
    cpuReleaseMemoryMutex();
    }

static void cp180OpB5(Cpu180Context *activeCpu)  // B5  CALLSEG    MIGDS 2-122
    {
    u64              Aj;
    u8               at;
    u64              bsp;
    u64              callee;
    u64              cbp;
    MonitorCondition cond;
    u32              disp;
    bool             isExt;
    u32              frameSize;
    u8               r1;
    u8               r2;
    u8               ring;
    u8               ringBsp;
    u32              rma;
    u64              sde;
    u64              sfsa;
    u8               vmid;
    u8               xs;
    u8               xt;

    Aj = activeCpu->regA[activeCpu->opJ];
    if ((Aj & 7) != 0)
        {
        cpu180SetMonitorCondition(activeCpu, MCR52);
        return;
        }
    disp = ((activeCpu->opQ < 0x8000) ? activeCpu->opQ : 0xffff0000 | activeCpu->opQ) << 3;
    bsp  = (Aj & RingSegMask) | (((Aj & Mask32) + disp) & Mask32);
    if (cpu180IsBindingSectionRef(activeCpu, bsp) == FALSE)
        {
        activeCpu->regUtp = bsp;
        cpu180SetMonitorCondition(activeCpu, MCR54); // access violation
        return;
        }
    ringBsp = RingOf(bsp);
    if (ringBsp > cpu180GetR2(activeCpu, Aj))
        {
        activeCpu->regUtp = bsp;
        cpu180SetMonitorCondition(activeCpu, MCR54); // access violation
        return;
        }
    if (cpu180PvaToRma(activeCpu, bsp, AccessModeRead, &rma, &cond) == FALSE)
        {
        cpu180SetMonitorCondition(activeCpu, cond);
        return;
        }
    cbp = cpMem[rma >> 3];

#if CcDebug == 1
    traceCodebasePointer(activeCpu, bsp, rma, cbp);
#endif

    callee = cbp & Mask48;
    r1     = cpu180GetR1(activeCpu, callee);
    ring   = RingOf(activeCpu->regP);
    if (r1 > ring)
        {
        activeCpu->regUtp = callee;
        cpu180SetMonitorCondition(activeCpu, MCR61);
        return;
        }
    r2 = cpu180GetR2(activeCpu, callee);
    if (ring > r2)
        {
        ring   = r2;
        callee = ((u64)ring << 44) | (callee & Mask44);
        }
    if (((callee & 7) != 0) || (callee & 0x80000000) != 0)
        {
        activeCpu->regUtp = callee;
        cpu180SetMonitorCondition(activeCpu, MCR52);
        return;
        }
    if (ringBsp > ((cbp >> 48) & Mask4))
        {
        activeCpu->regUtp = callee;
        cpu180SetMonitorCondition(activeCpu, MCR54);
        return;
        }
    if (cpu180IsValidSegment(activeCpu, callee) == FALSE)
        {
        activeCpu->regUtp = callee;
        cpu180SetMonitorCondition(activeCpu, MCR60);
        return;
        }
    sde = cpMem[(activeCpu->regSta >> 3) + SegmentOf(callee)];
    if (cpu180ValidateAccess(activeCpu, sde, RingOf(callee), AccessModeExecute) == FALSE)
        {
        activeCpu->regUtp = callee;
        cpu180SetMonitorCondition(activeCpu, MCR54);
        return;
        }
    if (cpu180IsValidSegment(activeCpu, activeCpu->regA[0]) == FALSE)
        {
        activeCpu->regUtp = (activeCpu->regA[0] + 7) & ~(u64)7;
        cpu180SetMonitorCondition(activeCpu, MCR60);
        return;
        }
    sde = cpMem[(activeCpu->regSta >> 3) + SegmentOf(activeCpu->regA[0])];
    if (cpu180ValidateAccess(activeCpu, sde, RingOf(activeCpu->regA[0]), AccessModeWrite) == FALSE)
        {
        activeCpu->regUtp = (activeCpu->regA[0] + 7) & ~(u64)7;
        cpu180SetMonitorCondition(activeCpu, MCR54);
        return;
        }
    vmid  = (cbp >> 56) & Mask4;
    isExt = vmid == 0 && ((cbp >> 55) & 1) != 0;
    if (isExt)
        {
        if (cpu180PvaToRma(activeCpu, bsp + 8, AccessModeRead, &rma, &cond) == FALSE)
            {
            cpu180SetMonitorCondition(activeCpu, cond);
            return;
            }
        bsp = cpMem[rma >> 3] & Mask48;
        }
    xs = (activeCpu->regX[0] >> 8) & Mask4;
    at = (activeCpu->regX[0] >> 4) & Mask4;
    xt = activeCpu->regX[0] & Mask4;
    if (cpu180PushFrame(activeCpu, at, xs, xt, FALSE, &sfsa, &frameSize) == FALSE)
        {
        return;
        }

#if CcDebug == 1
    traceCallFrame(activeCpu, sfsa);
#endif

    activeCpu->regA[2]          = activeCpu->regA[0];
    activeCpu->regA[0]         += frameSize;
    activeCpu->regTos[ring - 1] = activeCpu->regA[0];
    activeCpu->nextKey          = cpu180GetLock(activeCpu, callee);
    if (ring > activeCpu->regLrn)
        {
        activeCpu->regLrn = ring;
        }
    activeCpu->nextP = callee;
    if (isExt)
        {
        if ((bsp & RingMask) < (activeCpu->nextP & RingMask))
            {
            bsp = (bsp & ~RingMask) | (activeCpu->nextP & RingMask);
            }
        activeCpu->regA[3] = bsp;
        }
    activeCpu->regA[1]   = activeCpu->regTos[ring - 1];
    activeCpu->regA[0]   = activeCpu->regA[1];
    if (vmid == 0)
        {
        activeCpu->regA[4] = activeCpu->regA[activeCpu->opK];
        }
    activeCpu->regVmid   = vmid;
    activeCpu->regFlags &= 0x3fff; // clear CCF and OCF

#if CcDebug == 1
    traceCall(activeCpu, activeCpu->nextP);
#endif
    }

static void cp180OpC0(Cpu180Context *activeCpu)  // C0  EXECUTE,0  MIGDS 2-137
    {
    cp180OpIv(activeCpu);
    }

static void cp180OpC1(Cpu180Context *activeCpu)  // C1  EXECUTE,1  MIGDS 2-137
    {
    cp180OpIv(activeCpu);
    }

static void cp180OpC2(Cpu180Context *activeCpu)  // C2  EXECUTE,2  MIGDS 2-137
    {
    cp180OpIv(activeCpu);
    }

static void cp180OpC3(Cpu180Context *activeCpu)  // C3  EXECUTE,3  MIGDS 2-137
    {
    cp180OpIv(activeCpu);
    }

static void cp180OpC4(Cpu180Context *activeCpu)  // C4  EXECUTE,4  MIGDS 2-137
    {
    cp180OpIv(activeCpu);
    }

static void cp180OpC5(Cpu180Context *activeCpu)  // C5  EXECUTE,5  MIGDS 2-137
    {
    cp180OpIv(activeCpu);
    }

static void cp180OpC6(Cpu180Context *activeCpu)  // C6  EXECUTE,6  MIGDS 2-137
    {
    cp180OpIv(activeCpu);
    }

static void cp180OpC7(Cpu180Context *activeCpu)  // C7  EXECUTE,7  MIGDS 2-137
    {
    cp180OpIv(activeCpu);
    }

static void cp180OpD0(Cpu180Context *activeCpu)  // D0  LBYTS,1    MIGDS 2-11
    {
    cp180OpLBYTS(activeCpu, 1);
    }

static void cp180OpD1(Cpu180Context *activeCpu)  // D1  LBYTS,2    MIGDS 2-11
    {
    cp180OpLBYTS(activeCpu, 2);
    }

static void cp180OpD2(Cpu180Context *activeCpu)  // D2  LBYTS,3    MIGDS 2-11
    {
    cp180OpLBYTS(activeCpu, 3);
    }

static void cp180OpD3(Cpu180Context *activeCpu)  // D3  LBYTS,4    MIGDS 2-11
    {
    cp180OpLBYTS(activeCpu, 4);
    }

static void cp180OpD4(Cpu180Context *activeCpu)  // D4  LBYTS,5    MIGDS 2-11
    {
    cp180OpLBYTS(activeCpu, 5);
    }

static void cp180OpD5(Cpu180Context *activeCpu)  // D5  LBYTS,6    MIGDS 2-11
    {
    cp180OpLBYTS(activeCpu, 6);
    }

static void cp180OpD6(Cpu180Context *activeCpu)  // D6  LBYTS,7    MIGDS 2-11
    {
    cp180OpLBYTS(activeCpu, 7);
    }

static void cp180OpD7(Cpu180Context *activeCpu)  // D7  LBYTS,8    MIGDS 2-11
    {
    cp180OpLBYTS(activeCpu, 8);
    }

static void cp180OpD8(Cpu180Context *activeCpu)  // D8  SBYTS,1    MIGDS 2-11
    {
    cp180OpSBYTS(activeCpu, 1);
    }

static void cp180OpD9(Cpu180Context *activeCpu)  // D9  SBYTS,2    MIGDS 2-11
    {
    cp180OpSBYTS(activeCpu, 2);
    }

static void cp180OpDA(Cpu180Context *activeCpu)  // DA  SBYTS,3    MIGDS 2-11
    {
    cp180OpSBYTS(activeCpu, 3);
    }

static void cp180OpDB(Cpu180Context *activeCpu)  // DB  SBYTS,4    MIGDS 2-11
    {
    cp180OpSBYTS(activeCpu, 4);
    }

static void cp180OpDC(Cpu180Context *activeCpu)  // DC  SBYTS,5    MIGDS 2-11
    {
    cp180OpSBYTS(activeCpu, 5);
    }

static void cp180OpDD(Cpu180Context *activeCpu)  // DD  SBYTS,6    MIGDS 2-11
    {
    cp180OpSBYTS(activeCpu, 6);
    }

static void cp180OpDE(Cpu180Context *activeCpu)  // DE  SBYTS,7    MIGDS 2-11
    {
    cp180OpSBYTS(activeCpu, 7);
    }

static void cp180OpDF(Cpu180Context *activeCpu)  // DF  SBYTS,8    MIGDS 2-11
    {
    cp180OpSBYTS(activeCpu, 8);
    }

static void cp180OpE4(Cpu180Context *activeCpu)  // E4  SCLN       MIGDS 2-49
    {
    cp180OpIv(activeCpu);
    }

static void cp180OpE5(Cpu180Context *activeCpu)  // E5  SCLR       MIGDS 2-49
    {
    cp180OpIv(activeCpu);
    }

static void cp180OpE9(Cpu180Context *activeCpu)  // E9  CMPC       MIGDS 2-52
    {
    cp180OpIv(activeCpu);
    }

static void cp180OpEB(Cpu180Context *activeCpu)  // EB  TRANB      MIGDS 2-54
    {
    cp180OpIv(activeCpu);
    }

static void cp180OpED(Cpu180Context *activeCpu)  // ED  EDIT       MIGDS 2-55
    {
    cp180OpIv(activeCpu);
    }

static void cp180OpF3(Cpu180Context *activeCpu)  // F3  SCNB       MIGDS 2-54
    {
    cp180OpIv(activeCpu);
    }

static void cp180OpF9(Cpu180Context *activeCpu)  // F9  MOVI       MIGDS 2-62
    {
    cp180OpIv(activeCpu);
    }

static void cp180OpFA(Cpu180Context *activeCpu)  // FA  CMPI       MIGDS 2-63
    {
    cp180OpIv(activeCpu);
    }

static void cp180OpFB(Cpu180Context *activeCpu)  // FB  ADDI       MIGDS 2-64
    {
    cp180OpIv(activeCpu);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Process invalid/unimplmented instruction
**
**  Parameters:     Name        Description.
**                  ctx         pointer to CPU context
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
static void cp180OpIv(Cpu180Context *activeCpu) 
    {
    cpu180SetUserCondition(activeCpu, UCR49);
/*DELETE*/fprintf(stderr,"Invalid instruction %02x (P %016lx)\n", activeCpu->opCode, activeCpu->regP);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Process LBYTS instruction (MIGDS 2-11)
**
**  Parameters:     Name        Description.
**                  ctx         pointer to CPU context
**                  count       number of bytes to load
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
static void cp180OpLBYTS(Cpu180Context *activeCpu, u8 count)
    {
    u64 pva;
    u64 word;

    pva = activeCpu->regA[activeCpu->opJ] + activeCpu->opD;
    if (activeCpu->opI != 0)
        {
        pva = (pva & RingSegMask) | ((pva + (activeCpu->regX[activeCpu->opI] & Mask32)) & Mask32);
        }
    if (cpu180GetBytes(activeCpu, pva, count, AccessModeRead, &word))
        {
        activeCpu->regX[activeCpu->opK] = word;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Process SBYTS instruction (MIGDS 2-11)
**
**  Parameters:     Name        Description.
**                  ctx         pointer to CPU context
**                  count       number of bytes to store
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cp180OpSBYTS(Cpu180Context *activeCpu, int count)
    {
    u64 pva;

    pva = activeCpu->regA[activeCpu->opJ] + activeCpu->opD;
    if (activeCpu->opI != 0)
        {
        pva = (pva & RingSegMask) | ((pva + (activeCpu->regX[activeCpu->opI] & Mask32)) & Mask32);
        }
    (void)cpu180PutBytes(activeCpu, pva, activeCpu->regX[activeCpu->opK], count);
    }

/*---------------------------  End Of File  ------------------------------*/
