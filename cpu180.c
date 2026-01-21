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

#define DEBUG                0
#define DEBUG_INTERRUPT      0
#define DEBUG_SET_PAGE_FLAGS 0
#define DEBUG_SET_STATE_REG  0

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

#if CcDebug > 0

//
//  These macros facilitate tracing specific instructions and/or instructions
//  occurring within a specified range of addresses.
//
//  Define macro TRACE_INST_LIST as an array initializer specifying o list of
//  opcodes. This enables tracing of specific instructions. Macro TRACE_INST_COUNT
//  specifies the number of instructions to trace after TRACE_INST_LIST has
//  triggered tracing.

//  Define TRACE_RANGE_START and TRACE_RANGE_END to enable tracing instructions
//  within the range of addresses they specify.
//
//  Define TRACE_STORE_START and TRACE_STORE_END to trigger tracing when an
//  instruction stores data within the specified range of addresses. TRACE_INST_COUNT
//  defines how many instructions to trace thereafter.
//
//  Define TRACE_KEYPOINT_LIST as an array initializer specifying a list of system
//  keypoints. When the KEYPOINT instruction detects entry into an element in the list,
//  it enables instruction tracing, and when it detects exit, it disables instruction
//  tracing. This allows for tracing the execution of specific procedures within the
//  NOS/VE operating system. See the NOS/VE operating system source to discover the
//  keypoint constant definitions associated with specific procedures.
//
//  The following keypoint identifiers correspond to definitions in the NOS/VE source
//  except that the first "_" in each name is "$" in the NOS/VE source.
//
#define amk_close                          55
#define amk_copy_file                      58
#define amk_get_file_attributes            67
#define amk_get_next                       69
#define amk_open                           75
#define amk_return                         83
#define bak_connected_file_device         152
#define bak_open_file                     154
#define clk_create_file_connection        259
#define clk_declare_variable              260
#define clk_get_line_from_command_file    266
#define clk_open_command_file             274
#define clk_process_command               279
#define clk_read_variable                 284
#define clk_scan_command_file             288
#define clk_scan_command_line             289
#define clk_scan_parameter_list           291
#define clk_include_file                  299
#define cmk_build_interface_tables        301
#define cmk_build_pp_interface_table      303
#define cmk_pc_get_logical_unit           309
#define cmk_pc_get_next_channel           310
#define cmk_get_conf_file                 321
#define cmk_install_conf_file             322
#define clk_include_line                  351
#define ifk_get_terminal_attributes       661
#define lok_load_program                  950
#define lok_load_module_from_library      951
#define lok_satisfy_externals             955
#define lok_load_module                   956
#define mmk_page_fault                   1106
#define mmk_build_lock_rmal              1138
#define mmk_advise_out                   1152
#define mmk_write_modified_pages         1157
#define ofk_screen_input_fap             1351
#define osk_generate_message             1400
#define osk_format_message               1401
#define osk_set_status_abnormal          1403
#define osk_await_activity_completion    1407
#define osk_allocate                     1447
#define pfk_attach                       1500
#define pfk_get_object_information       1539
#define pfk_restricted_attach            1565
#define pfk_return_permanent_file        1567
#define pmk_task_begin_end               1600
#define pmk_task_begin                   1601
#define pmk_pop_all_stack_frames         1602
#define pmk_execute                      1604
#define pmk_exit                         1606
#define pmk_abort                        1607
#define pmk_await_task_termination       1609
#define pmk_establish_condition_handler  1622
#define pmk_disestablish_cond_handler    1623
#define pmk_cause_condition              1624
#define pmk_get_time                     1627
#define pmk_log_message                  1641
#define pmk_log_ascii                    1642
#define pmk_log                          1651
#define pmk_wait                         1655
#define pmk_long_term_wait               1656
#define pmk_enable_system_conditions     1676
#define pmk_establish_ch_in_block        1677
#define pmk_get_binary_processor_id      1683
#define pmk_load_from_library            1689
#define pmk_validate_previous_save_area  1703
#define pmk_push_task_debug_mode         1708
#define pmk_set_task_debug_mode          1710
#define pmk_establish_debug_cff          1712
#define pmk_change_job_library_list      1720
#define pmk_pop_inhibit_termination      1721
#define pmk_push_inhibit_termination     1722
#define pmk_establish_ch_outside_block   1746
#define tmk_switch_task                  1918
#define tmk_send_monitor_fault           1930
#define tmk_process_task_mcr_fault       1934
#define tmk_set_monitor_flag             1936
#define iok_queue_request                2200
#define iok_io_completions               2201
#define iok_allocate_image_request       2203
#define iok_queue_image_request          2204
#define jmk_get_job_status               2602
#define jmk_idle_system                  2615
#define jmk_job_exists                   2627
#define fmk_return_file                  2702
#define fsk_open_file                    2802
#define mtk_job_entry_exit               4001
#define mtk_170_entry_exit               4002
#define mtk_monitor_mode_trap            4003
#define mtk_job_mode_trap                4004
//

//#define TRACE_INST_LIST   { 0x77, 0xe9 }
#define TRACE_INST_COUNT  10

//#define TRACE_RANGE_START 0x405200126c00
//#define TRACE_RANGE_END   0x405200126cff
//#define TRACE_RANGE_START 0xb0440002f800
//#define TRACE_RANGE_END   0xb0440002f8ff

//#define TRACE_STORE_START 0x100a0000b758
//#define TRACE_STORE_END   0x100a0000b758
/*
#define TRACE_KEYPOINT_LIST         \
    {                               \
    osk_set_status_abnormal,        \
    pmk_push_task_debug_mode,       \
    pmk_establish_debug_cff,        \
    pmk_push_inhibit_termination,   \
    pmk_pop_inhibit_termination,    \
    pmk_validate_previous_save_area \
    }
*/
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
**  Memory Register addresses.
*/
#define MemStatusSummary       0x00
#define MemElementId           0x10
#define MemOptionsInstalled    0x12
#define MemEnvControl          0x20
#define MemBounds              0x21
#define MemCEL                 0xA0
#define MemCELd0               0xA0
#define MemCELd1               0xA1
#define MemDELd2               0xA2
#define MemCELd3               0xA3
#define MemUEL1                0xA4
#define MemUEL1d0              0xA4
#define MemUEL1d1              0xA5
#define MemUEL1d2              0xA6
#define MemUEL1d3              0xA7
#define MemUEL2                0xA8
#define MemUEL2d0              0xA8
#define MemUEL2d1              0xA9
#define MemUEL2d2              0xAA
#define MemUEL2d3              0xAB
#define MemFreeRunningCounter  0xB0

/*
**  Processor register addresses.
*/
#define RegStatusSummary       0x00
#define RegElementId           0x10
#define RegProcessorId         0x11
#define RegOptionsInstalled    0x12
#define RegVmCapabilityList    0x13
#define RegPerfMonFacility     0x22
#define RegDepEnvControl       0x30
#define RegCtrlStoreAddr       0x31
#define RegCtrlStoreBreak      0x32
#define RegRegisterP           0x40
#define RegMonitorProcState    0x41
#define RegMonitorCondition    0x42
#define RegUserCondition       0x43
#define RegUntranslatablePtr   0x44
#define RegSegmentTableLen     0x45
#define RegSegmentTableAddr    0x46
#define RegBaseConstant        0x47
#define RegPageTableAddr       0x48
#define RegPageTableLen        0x49
#define RegPageSizeMask        0x4A
#define RegModelDepFlags       0x50
#define RegModelDepWord        0x51
#define RegMonitorMask         0x60
#define RegJobProcessState     0x61
#define RegSystemIntTimer      0x62
#define RegKeypointBuffer      0x63
#define RegFaultStatus0        0x80
#define RegFaultStatus1        0x81
#define RegFaultStatus2        0x82
#define RegFaultStatus3        0x83
#define RegFaultStatus4        0x84
#define RegFaultStatus5        0x85
#define RegFaultStatus6        0x86
#define RegFaultStatus7        0x87
#define RegFaultStatus8        0x88
#define RegFaultStatus9        0x89
#define RegFaultStatusA        0x8A
#define RegFaultStatusB        0x8B
#define RegFaultStatusC        0x8C
#define RegFaultStatusD        0x8D
#define RegFaultStatusE        0x8E
#define RegFaultStatusF        0x8F
#define RegCCEL                0x92
#define RegMCEL                0x93
#define RegTestMode            0xA0
#define RegTrapPointer         0xC4
#define RegDebugList           0xC5
#define RegKeypointMask        0xC6
#define RegProcessIntTimer     0xC9
#define RegDebugIndex          0xE4
#define RegDebugMask           0xE5
#define RegUserMask            0xE6

/*
**  Definitions used in tracing KEYPOINT instructions
*/
#define KeypointEntry          2
#define KeypointExit           3
#define KeypointDebug          4
#define KeypointMtr            5

/*
**  -----------------------
**  Private Macro Functions
**  -----------------------
*/
#define IsInvalidPva(pva) ((pva) >= 0xffff80000000)
#define IsTrapEnabled(ctx) ((ctx->regFlags & Mask2) == 2)

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
static void cpu180ApplyBdpOperator(Cpu180Context *ctx, bool (*operator)(BdpOperand *src, BdpOperand *dst, BdpOperand *result, UserCondition *cond));
static bool cpu180CallIndirect(Cpu180Context *ctx, u64 bsp, u64 cbp, u64 pp, u8 at, u8 xs, u8 xt, bool doSaveCrs, MonitorCondition *cond);
static void cpu180CheckMonitorConditions(Cpu180Context *ctx);
static void cpu180CheckUserConditions(Cpu180Context *ctx);
static void cpu180Exchange(Cpu180Context *activeCpu);
static bool cpu180FindPte(Cpu180Context *ctx, u16 asid, u32 byteNum, bool ignoreValidity, u32 *pti, u8 *count);
static void cpu180Get170State(Cpu180Context *ctx);
static ConditionAction cpu180GetActionForMonitorCondition(Cpu180Context *ctx, MonitorCondition cond);
static ConditionAction cpu180GetActionForTrapCondition(Cpu180Context *ctx, MonitorCondition cond);
static ConditionAction cpu180GetActionForUserCondition(Cpu180Context *ctx, UserCondition cond);
static bool cpu180GetBdpDescriptor(Cpu180Context *ctx, u64 pva, u8 aRegNum, u8 xRegNum, BdpDescriptor *descriptor);
static bool cpu180GetBytes(Cpu180Context *ctx, u64 pva, u8 count, u8 ring, Cpu180AccessMode access, u64 *word);
static u8 cpu180GetCurrentXp(Cpu180Context *ctx);
static bool cpu180GetLock(Cpu180Context *ctx, u64 pva, u8 *lock, MonitorCondition *cond);
static bool cpu180GetParcel(Cpu180Context *ctx, u64 pva, u16 *parcel);
static bool cpu180GetR1(Cpu180Context *ctx, u64 pva, u8 *r1, MonitorCondition *cond);
static bool cpu180GetR2(Cpu180Context *ctx, u64 pva, u8 *r2, MonitorCondition *cond);
static bool cpu180IsBindingSectionRef(Cpu180Context *ctx, u64 pva);
static void cpu180Load170Xp(Cpu180Context *ctx, u32 xpa);
static bool cpu180MulInt32(Cpu180Context *ctx, u32 mltand, u32 mltier, u32 *product);
static bool cpu180MulInt64(Cpu180Context *ctx, u64 mltand, u64 mltier, u64 *product);
static bool cpu180PushFrame(Cpu180Context *ctx, u8 at, u8 xs, u8 xt, bool doSaveCrs, u64 *sfsa, u32 *frameSize, MonitorCondition *cond);
static bool cpu180PutBytes(Cpu180Context *ctx, u64 pva, u8 ring, u64 word, u8 count);
static void cpu180Set170State(Cpu180Context *ctx, u64 regP);
static void cpu180SetRingZeroCondition(Cpu180Context *ctx, u64 pva);
static void cpu180Store180Xp(Cpu180Context *ctx, u32 xpa);
static bool cpu180SubInt32(Cpu180Context *ctx, u32 minend, u32 subend, u32 *diff);
static bool cpu180SubInt64(Cpu180Context *ctx, u64 minend, u64 subend, u64 *diff);
static void cpu180UpdatePageSize(Cpu180Context *ctx);
static bool cpu180ValidateAccess(Cpu180Context *ctx, u64 sde, u8 ring, Cpu180AccessMode access, MonitorCondition *cond);

#if CcDebug > 0

#if defined(TRACE_STORE_START)
static void cpu180CheckTraceStore(Cpu180Context *ctx, u64 pvaStart, u64 pvaEnd);
#endif

#if defined(TRACE_KEYPOINT_LIST)
static char *cpu180KeypointToStr(u16 kpt);
static void cpu180PopKeypoint(Cpu180Context *ctx, u16 kpt);
static void cpu180PushKeypoint(Cpu180Context *ctx, u16 kpt);
static void cpu180ProcessKeypointEntry(Cpu180Context *ctx, u16 kpt);
static void cpu180ProcessKeypointExit(Cpu180Context *ctx, u16 kpt);
static bool cpu180SearchKeypointList(u16 kpt);
#endif

#endif

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
static void cp180OpSBYTS(Cpu180Context *activeCpu, u8 count);

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
    { cp180Op00, jk   }, // 00
    { cp180Op01, jk   }, // 01
    { cp180Op02, jk   }, // 02
    { cp180Op03, jk   }, // 03
    { cp180Op04, jk   }, // 04
    { cp180Op05, jk   }, // 05
    { cp180Op06, jk   }, // 06
    { cp180Op07, jk   }, // 07
    { cp180Op08, jk   }, // 08
    { cp180Op09, jk   }, // 09
    { cp180Op0A, jk   }, // 0A
    { cp180Op0B, jk   }, // 0B
    { cp180Op0C, jk   }, // 0C
    { cp180Op0D, jk   }, // 0D
    { cp180Op0E, jk   }, // 0E
    { cp180Op0F, jk   }, // 0F

    { cp180Op10, jk   }, // 10
    { cp180Op11, jk   }, // 11
    { cp180OpIv, jk   }, // 12
    { cp180OpIv, jk   }, // 13
    { cp180Op14, jk   }, // 14
    { cp180OpIv, jk   }, // 15
    { cp180Op16, jk   }, // 16
    { cp180Op17, jk   }, // 17
    { cp180Op18, jk   }, // 18
    { cp180Op19, jk   }, // 19
    { cp180Op1A, jk   }, // 1A
    { cp180Op1B, jk   }, // 1B
    { cp180Op1C, jk   }, // 1C
    { cp180OpIv, jk   }, // 1D
    { cp180Op1E, jk   }, // 1E
    { cp180Op1F, jk   }, // 1F

    { cp180Op20, jk   }, // 20
    { cp180Op21, jk   }, // 21
    { cp180Op22, jk   }, // 22
    { cp180Op23, jk   }, // 23
    { cp180Op24, jk   }, // 24
    { cp180Op25, jk   }, // 25
    { cp180Op26, jk   }, // 26
    { cp180Op27, jk   }, // 27
    { cp180Op28, jk   }, // 28
    { cp180Op29, jk   }, // 29
    { cp180Op2A, jk   }, // 2A
    { cp180OpIv, jk   }, // 2B
    { cp180Op2C, jk   }, // 2C
    { cp180Op2D, jk   }, // 2D
    { cp180Op2E, jk   }, // 2E
    { cp180Op2F, jk   }, // 2F

    { cp180Op30, jk   }, // 30
    { cp180Op31, jk   }, // 31
    { cp180Op32, jk   }, // 32
    { cp180Op33, jk   }, // 33
    { cp180Op34, jk   }, // 34
    { cp180Op35, jk   }, // 35
    { cp180Op36, jk   }, // 36
    { cp180Op37, jk   }, // 37
    { cp180OpIv, jk   }, // 38
    { cp180Op39, jk   }, // 39
    { cp180Op3A, jk   }, // 3A
    { cp180Op3B, jk   }, // 3B
    { cp180Op3C, jk   }, // 3C
    { cp180Op3D, jk   }, // 3D
    { cp180Op3E, jk   }, // 3E
    { cp180Op3F, jk   }, // 3F

    { cp180Op40, jkiD }, // 40
    { cp180Op41, jkiD }, // 41
    { cp180Op42, jkiD }, // 42
    { cp180Op43, jkiD }, // 43
    { cp180Op44, jkiD }, // 44
    { cp180Op45, jkiD }, // 45
    { cp180OpIv, jkiD }, // 46
    { cp180OpIv, jkiD }, // 47
    { cp180Op48, jkiD }, // 48
    { cp180Op49, jkiD }, // 49
    { cp180Op4A, jkiD }, // 4A
    { cp180Op4B, jkiD }, // 4B
    { cp180Op4C, jkiD }, // 4C
    { cp180Op4D, jkiD }, // 4D
    { cp180OpIv, jkiD }, // 4E
    { cp180OpIv, jkiD }, // 4F

    { cp180Op50, jkiD }, // 50
    { cp180Op51, jkiD }, // 51
    { cp180Op52, jkiD }, // 52
    { cp180Op53, jkiD }, // 53
    { cp180Op54, jkiD }, // 54
    { cp180Op55, jkiD }, // 55
    { cp180Op56, jkiD }, // 56
    { cp180Op57, jkiD }, // 57
    { cp180Op58, jkiD }, // 58
    { cp180Op59, jkiD }, // 59
    { cp180Op5A, jkiD }, // 5A
    { cp180Op5B, jkiD }, // 5B
    { cp180Op5C, jkiD }, // 5C
    { cp180Op5D, jkiD }, // 5D
    { cp180Op5E, jkiD }, // 5E
    { cp180OpIv, jkiD }, // 5F

    { cp180OpIv, jkiD }, // 60
    { cp180OpIv, jkiD }, // 61
    { cp180OpIv, jkiD }, // 62
    { cp180OpIv, jkiD }, // 63
    { cp180OpIv, jkiD }, // 64
    { cp180OpIv, jkiD }, // 65
    { cp180OpIv, jkiD }, // 66
    { cp180OpIv, jkiD }, // 67
    { cp180OpIv, jkiD }, // 68
    { cp180OpIv, jkiD }, // 69
    { cp180OpIv, jkiD }, // 6A
    { cp180OpIv, jkiD }, // 6B
    { cp180OpIv, jkiD }, // 6C
    { cp180OpIv, jkiD }, // 6D
    { cp180OpIv, jkiD }, // 6E
    { cp180OpIv, jkiD }, // 6F

    { cp180Op70, jk   }, // 70
    { cp180Op71, jk   }, // 71
    { cp180Op72, jk   }, // 72
    { cp180Op73, jk   }, // 73
    { cp180Op74, jk   }, // 74
    { cp180Op75, jk   }, // 75
    { cp180Op76, jk   }, // 76
    { cp180Op77, jk   }, // 77
    { cp180OpIv, jk   }, // 78
    { cp180OpIv, jk   }, // 79
    { cp180OpIv, jk   }, // 7A
    { cp180OpIv, jk   }, // 7B
    { cp180OpIv, jk   }, // 7C
    { cp180OpIv, jk   }, // 7D
    { cp180OpIv, jk   }, // 7E
    { cp180OpIv, jk   }, // 7F

    { cp180Op80, jkQ  }, // 80
    { cp180Op81, jkQ  }, // 81
    { cp180Op82, jkQ  }, // 82
    { cp180Op83, jkQ  }, // 83
    { cp180Op84, jkQ  }, // 84
    { cp180Op85, jkQ  }, // 85
    { cp180Op86, jkQ  }, // 86
    { cp180Op87, jkQ  }, // 87
    { cp180Op88, jkQ  }, // 88
    { cp180Op89, jkQ  }, // 89
    { cp180Op8A, jkQ  }, // 8A
    { cp180Op8B, jkQ  }, // 8B
    { cp180Op8C, jkQ  }, // 8C
    { cp180Op8D, jkQ  }, // 8D
    { cp180Op8E, jkQ  }, // 8E
    { cp180Op8F, jkQ  }, // 8F

    { cp180Op90, jkQ  }, // 90
    { cp180Op91, jkQ  }, // 91
    { cp180Op92, jkQ  }, // 92
    { cp180Op93, jkQ  }, // 93
    { cp180Op94, jkQ  }, // 94
    { cp180Op95, jkQ  }, // 95
    { cp180Op96, jkQ  }, // 96
    { cp180Op97, jkQ  }, // 97
    { cp180Op98, jkQ  }, // 98
    { cp180Op99, jkQ  }, // 99
    { cp180Op9A, jkQ  }, // 9A
    { cp180Op9B, jkQ  }, // 9B
    { cp180Op9C, jkQ  }, // 9C
    { cp180Op9D, jkQ  }, // 9D
    { cp180Op9E, jkQ  }, // 9E
    { cp180Op9F, jkQ  }, // 9F

    { cp180OpA0, jkiD }, // A0
    { cp180OpA1, jkiD }, // A1
    { cp180OpA2, jkiD }, // A2
    { cp180OpA3, jkiD }, // A3
    { cp180OpA4, jkiD }, // A4
    { cp180OpA5, jkiD }, // A5
    { cp180OpIv, jkiD }, // A6
    { cp180OpA7, jkiD }, // A7
    { cp180OpA8, jkiD }, // A8
    { cp180OpA9, jkiD }, // A9
    { cp180OpAA, jkiD }, // AA
    { cp180OpIv, jkiD }, // AB
    { cp180OpAC, jkiD }, // AC
    { cp180OpAD, jkiD }, // AD
    { cp180OpAE, jkiD }, // AE
    { cp180OpIv, jkiD }, // AF

    { cp180OpB0, jkQ  }, // B0
    { cp180OpB1, jkQ  }, // B1
    { cp180OpB2, jkQ  }, // B2
    { cp180OpB3, jkQ  }, // B3
    { cp180OpB4, jkQ  }, // B4
    { cp180OpB5, jkQ  }, // B5
    { cp180OpIv, jkQ  }, // B6
    { cp180OpIv, jkQ  }, // B7
    { cp180OpIv, jkQ  }, // B8
    { cp180OpIv, jkQ  }, // B9
    { cp180OpIv, jkQ  }, // BA
    { cp180OpIv, jkQ  }, // BB
    { cp180OpIv, jkQ  }, // BC
    { cp180OpIv, jkQ  }, // BD
    { cp180OpIv, jkQ  }, // BE
    { cp180OpIv, jkQ  }, // BF

    { cp180OpC0, jkiD }, // C0
    { cp180OpC1, jkiD }, // C1
    { cp180OpC2, jkiD }, // C2
    { cp180OpC3, jkiD }, // C3
    { cp180OpC4, jkiD }, // C4
    { cp180OpC5, jkiD }, // C5
    { cp180OpC6, jkiD }, // C6
    { cp180OpC7, jkiD }, // C7
    { cp180OpIv, jkiD }, // C8
    { cp180OpIv, jkiD }, // C9
    { cp180OpIv, jkiD }, // CA
    { cp180OpIv, jkiD }, // CB
    { cp180OpIv, jkiD }, // CC
    { cp180OpIv, jkiD }, // CD
    { cp180OpIv, jkiD }, // CE
    { cp180OpIv, jkiD }, // CF

    { cp180OpD0, jkiD }, // D0
    { cp180OpD1, jkiD }, // D1
    { cp180OpD2, jkiD }, // D2
    { cp180OpD3, jkiD }, // D3
    { cp180OpD4, jkiD }, // D4
    { cp180OpD5, jkiD }, // D5
    { cp180OpD6, jkiD }, // D6
    { cp180OpD7, jkiD }, // D7
    { cp180OpD8, jkiD }, // D8
    { cp180OpD9, jkiD }, // D9
    { cp180OpDA, jkiD }, // DA
    { cp180OpDB, jkiD }, // DB
    { cp180OpDC, jkiD }, // DC
    { cp180OpDD, jkiD }, // DD
    { cp180OpDE, jkiD }, // DE
    { cp180OpDF, jkiD }, // DF

    { cp180OpIv, jkiD }, // E0
    { cp180OpIv, jkiD }, // E1
    { cp180OpIv, jkiD }, // E2
    { cp180OpIv, jkiD }, // E3
    { cp180OpE4, jkiD }, // E4
    { cp180OpE5, jkiD }, // E5
    { cp180OpIv, jkiD }, // E6
    { cp180OpIv, jkiD }, // E7
    { cp180OpIv, jkiD }, // E8
    { cp180OpE9, jkiD }, // E9
    { cp180OpIv, jkiD }, // EA
    { cp180OpEB, jkiD }, // EB
    { cp180OpIv, jkiD }, // EC
    { cp180OpED, jkiD }, // ED
    { cp180OpIv, jkiD }, // EE
    { cp180OpIv, jkiD }, // EF

    { cp180OpIv, jkiD }, // F0
    { cp180OpIv, jkiD }, // F1
    { cp180OpIv, jkiD }, // F2
    { cp180OpF3, jkiD }, // F3
    { cp180OpIv, jkiD }, // F4
    { cp180OpIv, jkiD }, // F5
    { cp180OpIv, jkiD }, // F6
    { cp180OpIv, jkiD }, // F7
    { cp180OpIv, jkiD }, // F8
    { cp180OpF9, jkiD }, // F9
    { cp180OpFA, jkiD }, // FA
    { cp180OpFB, jkiD }, // FB
    { cp180OpIv, jkiD }, // FC
    { cp180OpIv, jkiD }, // FD
    { cp180OpIv, jkiD }, // FE
    { cp180OpIv, jkiD }  // FF
    };

/*
**  Condition action definitions for monitor conditions, indexed by MonitorCondition
*/
static ConditionActionDefn mcrDefns [] =
    {
//                          Job   Mtr   Job    Mtr
//                    P     Mask  Mask  Mask   Mask   NoMask
//      Bit   Cmplt  Stay    TE    TE    TD     TD
//    ------  -----  ----   ----  ----  ----   ----   ------
    { 0x8000, FALSE, TRUE,  Exch, Trap, Exch,  Halt,  Halt  }, /* MCR48 Detected uncorrectable error     */
    { 0x4000, FALSE, TRUE,  Exch, Trap, Exch,  Halt,  Halt  }, /* MCR49 Not assigned                     */
    { 0x2000, TRUE,  FALSE, Exch, Trap, Exch,  Stack, Stack }, /* MCR50 Short warning                    */
    { 0x1000, FALSE, TRUE,  Exch, Trap, Exch,  Halt,  Halt  }, /* MCR51 Instruction specfication error   */
    { 0x0800, FALSE, TRUE,  Exch, Trap, Exch,  Halt,  Halt  }, /* MCR52 Address specification error      */
    { 0x0400, TRUE,  FALSE, Exch, Trap, Exch,  Stack, Stack }, /* MCR53 CYBER 170 state exchange request */
    { 0x0200, FALSE, TRUE,  Exch, Trap, Exch,  Halt,  Halt  }, /* MCR54 Access violation                 */
    { 0x0100, FALSE, TRUE,  Exch, Trap, Exch,  Halt,  Halt  }, /* MCR55 Environment specification error  */
    { 0x0080, TRUE,  FALSE, Exch, Trap, Exch,  Stack, Stack }, /* MCR56 External interrupt               */
    { 0x0040, FALSE, TRUE,  Exch, Trap, Exch,  Halt,  Halt  }, /* MCR57 Page table search without find   */
    { 0x0020, TRUE,  FALSE, Rni,  Rni,  Rni,   Rni,   Rni   }, /* MCR58 System call (status bit)         */
    { 0x0010, TRUE,  FALSE, Exch, Trap, Exch,  Stack, Stack }, /* MCR59 System interval timer            */
    { 0x0008, FALSE, TRUE,  Exch, Trap, Exch,  Halt,  Halt  }, /* MCR60 Invalid segment / Ring number 0  */
    { 0x0004, FALSE, TRUE,  Exch, Trap, Exch,  Halt,  Halt  }, /* MCR61 Outward call / Inward return     */
    { 0x0002, TRUE,  FALSE, Exch, Trap, Exch,  Stack, Stack }, /* MCR62 Soft error                       */
    { 0x0001, FALSE, TRUE,  Rni,  Rni,  Rni,   Rni,   Rni   }  /* MCR63 Trap exception (status bit)      */
    };

/*
**  Condition action definitions for user conditions, indexed by UserCondition
*/
static ConditionActionDefn ucrDefns [] =
    {
//                          Job   Mtr   Job    Mtr
//                    P     Mask  Mask  Mask   Mask   NoMask
//      Bit   Cmplt  Stay    TE    TE    TD     TD
//    ------  -----  ----   ----  ----  ----   ----   ------
    { 0x8000, FALSE, TRUE,  Trap, Trap, Exch,  Halt,  Rni   }, /* UCR48 Privileged instruction fault     */
    { 0x4000, FALSE, TRUE,  Trap, Trap, Exch,  Halt,  Rni   }, /* UCR49 Unimplemented instruction        */
    { 0x2000, TRUE,  FALSE, Trap, Trap, Stack, Stack, Rni   }, /* UCR50 Free flag                        */
    { 0x1000, FALSE, TRUE,  Trap, Trap, Stack, Stack, Rni   }, /* UCR51 Process interval timer           */
    { 0x0800, FALSE, TRUE,  Trap, Trap, Exch,  Halt,  Rni   }, /* UCR52 Inter-ring pop                   */
    { 0x0400, FALSE, TRUE,  Trap, Trap, Exch,  Halt,  Rni   }, /* UCR53 Critical frame flag              */
    { 0x0200, TRUE,  FALSE, Trap, Trap, Stack, Stack, Rni   }, /* UCR54 Reserved                         */
    { 0x0100, FALSE, FALSE, Trap, Trap, Stack, Stack, Stack }, /* UCR55 Divide fault                     */
    { 0x0080, FALSE, TRUE,  Trap, Trap, Stack, Stack, Stack }, /* UCR56 Debug                            */
    { 0x0040, FALSE, TRUE,  Trap, Trap, Stack, Stack, Stack }, /* UCR57 Arithmetic overflow              */
    { 0x0020, TRUE,  TRUE,  Trap, Trap, Stack, Stack, Stack }, /* UCR58 Exponent overflow                */
    { 0x0010, TRUE,  TRUE,  Trap, Trap, Stack, Stack, Stack }, /* UCR59 Exponent underflow               */
    { 0x0008, TRUE,  TRUE,  Trap, Trap, Stack, Stack, Stack }, /* UCR60 FP loss of significance          */
    { 0x0004, FALSE, TRUE,  Trap, Trap, Stack, Stack, Stack }, /* UCR61 FP indefinite                    */
    { 0x0002, FALSE, TRUE,  Trap, Trap, Stack, Stack, Stack }, /* UCR62 Arithmetic loss of significance  */
    { 0x0001, TRUE,  TRUE,  Trap, Trap, Stack, Stack, Stack }  /* UCR63 Invalid BDP data                 */
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
**  Masks used for sign extension in shift instructions
*/
static u32 signExt32[33] =
    {
    0x80000000,
    0xc0000000,
    0xe0000000,
    0xf0000000,
    0xf8000000,
    0xfc000000,
    0xfe000000,
    0xff000000,
    0xff800000,
    0xffc00000,
    0xffe00000,
    0xfff00000,
    0xfff80000,
    0xfffc0000,
    0xfffe0000,
    0xffff0000,
    0xffff8000,
    0xffffc000,
    0xffffe000,
    0xfffff000,
    0xfffff800,
    0xfffffc00,
    0xfffffe00,
    0xffffff00,
    0xffffff80,
    0xffffffc0,
    0xffffffe0,
    0xfffffff0,
    0xfffffff8,
    0xfffffffc,
    0xfffffffe,
    0xffffffff,
    0xffffffff
    };

static u64 signExt64[65] =
    {
    0x8000000000000000,
    0xc000000000000000,
    0xe000000000000000,
    0xf000000000000000,
    0xf800000000000000,
    0xfc00000000000000,
    0xfe00000000000000,
    0xff00000000000000,
    0xff80000000000000,
    0xffc0000000000000,
    0xffe0000000000000,
    0xfff0000000000000,
    0xfff8000000000000,
    0xfffc000000000000,
    0xfffe000000000000,
    0xffff000000000000,
    0xffff800000000000,
    0xffffc00000000000,
    0xffffe00000000000,
    0xfffff00000000000,
    0xfffff80000000000,
    0xfffffc0000000000,
    0xfffffe0000000000,
    0xffffff0000000000,
    0xffffff8000000000,
    0xffffffc000000000,
    0xffffffe000000000,
    0xfffffff000000000,
    0xfffffff800000000,
    0xfffffffc00000000,
    0xfffffffe00000000,
    0xffffffff00000000,
    0xffffffff80000000,
    0xffffffffc0000000,
    0xffffffffe0000000,
    0xfffffffff0000000,
    0xfffffffff8000000,
    0xfffffffffc000000,
    0xfffffffffe000000,
    0xffffffffff000000,
    0xffffffffff800000,
    0xffffffffffc00000,
    0xffffffffffe00000,
    0xfffffffffff00000,
    0xfffffffffff80000,
    0xfffffffffffc0000,
    0xfffffffffffe0000,
    0xffffffffffff0000,
    0xffffffffffff8000,
    0xffffffffffffc000,
    0xffffffffffffe000,
    0xfffffffffffff000,
    0xfffffffffffff800,
    0xfffffffffffffc00,
    0xfffffffffffffe00,
    0xffffffffffffff00,
    0xffffffffffffff80,
    0xffffffffffffffc0,
    0xffffffffffffffe0,
    0xfffffffffffffff0,
    0xfffffffffffffff8,
    0xfffffffffffffffc,
    0xfffffffffffffffe,
    0xffffffffffffffff,
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
**  The ring and segment assigned to CYBER 170 state
*/
static u64 ringSeg170 = 0;

/*
**  Maintenance access information for central memory
*/
static u64 memoryBounds;
static u32 memoryEid;
static u64 memoryEnvControl;
static u64 memoryOptions;
static u16 memoryRegisterAddr;
static u8  memoryRegisterBuf[8];
static u8  memoryRegisterBufIdx;

#if DEBUG
static FILE *cpu180Log = NULL;
#endif

#if CcDebug > 0

static int traceInstCount[2] = { 0, 0 };

#if defined(TRACE_STORE_START)
static u32 traceRmaEnd       = 0;
static u32 traceRmaStart     = 0;
#endif

#if defined(TRACE_INST_LIST)
static u8  traceInstList[]   = TRACE_INST_LIST;
#endif

#if defined(TRACE_KEYPOINT_LIST)
static u16 traceKeypointList[] = TRACE_KEYPOINT_LIST;
#endif

#endif

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
**  Parameters:     Name          Description.
**                  model         CPU model string
**                  serialNumbers CPU serial numbers
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cpu180Init(char *model, u16 *serialNumbers)
    {
    Cpu180Context *activeCpu;
    u8            cpuNum;
    u64           memSizeMask;

#if DEBUG
    if (cpu180Log == NULL)
        {
        cpu180Log = fopen("cpu180log.txt", "wt");
        }
#endif

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
    case 32:
        memSizeMask = 0x0208;
        break;
    case 64:
        memSizeMask = 0x0408;
        break;
    case 128:
        memSizeMask = 0x0808;
        break;
    case 256:
        memSizeMask = 0x1008;
        break;
    case 512:
        memSizeMask = 0x2008;
        break;
    case 1024:
        memSizeMask = 0x4008;
        break;
    case 2048:
        memSizeMask = 0x8008;
        break;
    default:
        logDtError(LogErrorLocation, "Unsupported memory size: %ld", cpuMaxMemory * 8);
        exit(1);
        }

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
        activeCpu                  = &cpus180[cpuNum];
        activeCpu->id              = cpuNum;
        activeCpu->controlStore    = (u8 *)calloc(2048, 16);
        activeCpu->softMemories[3] = (u8 *)calloc(1024, 4);
        activeCpu->softMemories[4] = (u8 *)calloc(1024, 4);
        activeCpu->softMemories[5] = (u8 *)calloc(2048, 4);
        activeCpu->softMemories[6] = (u8 *)calloc(512, 4);
        activeCpu->registerFile    = (u8 *)calloc(64, 8);
        if (activeCpu->controlStore == NULL
            || activeCpu->softMemories[3] == NULL
            || activeCpu->softMemories[4] == NULL
            || activeCpu->softMemories[5] == NULL
            || activeCpu->softMemories[6] == NULL
            || activeCpu->registerFile == NULL)
            {
            fputs("(cpu    ) Failed to allocate memory for CYBER 180 CPU memory\n", stderr);
            exit(1);
            }
        activeCpu->regEid        = 0x00320000  // Elem: 00 (CP), Model: 860, S/N
                                   | ((serialNumbers[cpuNum] / 1000) << 12)
                                   | (((serialNumbers[cpuNum] % 1000) / 100) << 8)
                                   | (((serialNumbers[cpuNum] % 100) / 10) << 4)
                                   | (serialNumbers[cpuNum] % 10);
        activeCpu->regOi         = (cpuCount > 1) ? 0x10 : 0; // Model 860 has optional second processor
        activeCpu->regVmcl       = 0xc000;     // Virtual state and CYBER 170 state
        activeCpu->pendingAction = Rni;
        activeCpu->isStopped     = TRUE;
        activeCpu->isMonitorMode = TRUE;
        activeCpu->regSit        = 0xffffffff; // System Interval Timer
        activeCpu->regPit        = 0xffffffff; // Process Interval Timer
        cpu180UpdatePageSize(activeCpu);
        printf("(cpu    ) CP%d EID " FMT32_08x "\n", activeCpu->id, activeCpu->regEid);
        }

    memoryEid     = 0x01311234; // Elem: 01 (CM),  Model: 850/860, S/N
    memoryOptions = memSizeMask <<= 48;
    printf("(cpu    ) MEM EID " FMT32_08x "\n", memoryEid);
    printf("(cpu    ) MEM OI  " FMT64_016x "\n", memoryOptions);

    /*
    **  Print a friendly message.
    */
    fputs("(cpu    ) CYBER 180 CPU state initialised\n", stdout);
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
#if CcDebug > 0
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
    ctx->regUmr    = (word >> 48) | 0xfe00;
    word           = cpMem[xpa++];
    ctx->regA[3]   = word & Mask48;
    ctx->regMmr    = word >> 48;
    word           = cpMem[xpa++];
    ctx->regA[4]   = word & Mask48;
    ctx->regUcr    = word >> 48;
//  ctx->regUcr    = (word >> 48) & ~ctx->regUmr;
    word           = cpMem[xpa++];
    ctx->regA[5]   = word & Mask48;
    ctx->regMcr    = word >> 48;
//  ctx->regMcr    = (word >> 48) & ~(ctx->regMmr | 0x0021); // clear masked bits and status bits
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
    ctx->regTos[1] = word & Mask48;
    for (i = 2; i < 16; i++)
        {
        ctx->regTos[i] = cpMem[xpa++] & Mask48;
        }
#if CcDebug > 0
    traceExchange180(ctx, xpab, "Load");
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Maintenance access: get a value from a CM maintenance register
**
**  Parameters:     Name        Description.
**                  reg         the register address
**
**  Returns:        64-bit value.
**
**------------------------------------------------------------------------*/
u64 cpu180MacGetCmRegister(u8 reg)
    {
    switch (reg)
        {
    case MemStatusSummary:
    default:
        return 0;
    case MemBounds:
        return memoryBounds;
    case MemElementId:
        return memoryEid;
    case MemEnvControl:
        return memoryEnvControl;
    case MemFreeRunningCounter:
        return cpu180FreeRunningCounter;
    case MemOptionsInstalled:
        return memoryOptions;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Maintenance access: get a value from a CP state register
**
**  Parameters:     Name        Description.
**                  ctx         CPU context
**                  reg         the register address
**
**  Returns:        64-bit value.
**
**------------------------------------------------------------------------*/
u64 cpu180MacGetCpStateRegister(Cpu180Context *ctx, u8 reg)
    {
    u64 byte;

    switch (reg)
        {
    default:
        fprintf(stderr, "cpu180MacGetCpStateRegister: unknown reg %02x\n", reg);
        // fall through
    case RegOptionsInstalled:
        return ctx->regOi;
    case RegStatusSummary:
        byte = 0;
        if (ctx->isStopped)
            {
            byte |= 0x08;
            }
        if (ctx->isMonitorMode)
            {
            byte |= 0x20;
            }
        return (byte << 56) | (byte << 48) | (byte << 40) | (byte << 32)
             | (byte << 24) | (byte << 16) | (byte <<  8) | byte;
    case RegBaseConstant:
        return ctx->regBc;
    case RegCtrlStoreAddr:
        return ctx->controlStoreIdx >> 4; // 16 bytes per control store address
    case RegCtrlStoreBreak:
        return ctx->controlStoreBreak;
    case RegDebugIndex:
        return ctx->regDi;
    case RegDebugList:
        return ctx->regDlp;
    case RegDebugMask:
        return ctx->regDm;
    case RegDepEnvControl:
        return ctx->regDec;
    case RegElementId:
        return ctx->regEid;
    case RegFaultStatus0:
    case RegFaultStatus1:
    case RegFaultStatus2:
    case RegFaultStatus3:
    case RegFaultStatus4:
    case RegFaultStatus5:
    case RegFaultStatus6:
    case RegFaultStatus7:
    case RegFaultStatus8:
    case RegFaultStatus9:
    case RegFaultStatusA:
    case RegFaultStatusB:
    case RegFaultStatusC:
    case RegFaultStatusD:
    case RegFaultStatusE:
    case RegFaultStatusF:
        return 0;
    case RegJobProcessState:
        return ctx->regJps;
    case RegKeypointBuffer:
        return ctx->regKbp;
    case RegKeypointMask:
        return ctx->regKmr;
    case RegModelDepFlags:
        return ctx->regMdf;
    case RegModelDepWord:
        return ctx->regMdw;
    case RegMonitorCondition:
        return ctx->regMcr;
    case RegMonitorMask:
        return ctx->regMmr;
    case RegMonitorProcState:
        return ctx->regMps;
    case RegPageTableAddr:
        return ctx->regPta;
    case RegPageTableLen:
        return ctx->regPtl;
    case RegPageSizeMask:
        return ctx->regPsm;
    case RegProcessIntTimer:
        return ctx->regPit;
    case RegProcessorId:
        return ctx->id;
    case RegRegisterP:
        return ((u64)ctx->key << 48) | ctx->regP;
    case RegSegmentTableLen:
        return ctx->regStl;
    case RegSegmentTableAddr:
        return ctx->regSta;
    case RegSystemIntTimer:
        return ctx->regSit;
    case RegTestMode:
        return ctx->regTm;
    case RegTrapPointer:
        return ctx->regTp;
    case RegUntranslatablePtr:
        return ctx->regUtp;
    case  RegUserCondition:
        return ctx->regUcr;
    case  RegUserMask:
        return ctx->regUmr | 0xfe00;
    case RegVmCapabilityList:
        return ctx->regVmcl;
    //  Trap Enables addresses
    case 0xc0:
    case 0xc1:
    case 0xc2:
    case 0xc3:
        return ctx->regFlags & Mask2;
    //  Keypoint Enable addresses
    case 0xca:
    case 0xcb:
        return (ctx->regFlags >> 13) & 1;
        break;
    //  Critical Frame Flag addresses
    case 0xe0:
    case 0xe1:
        return (ctx->regFlags >> 15) & 1;
    //  On Condition Flag addresses
    case 0xe2:
    case 0xe3:
        return (ctx->regFlags >> 14) & 1;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Maintenance access: halt CP
**
**  Parameters:     Name        Description.
**                  ctx         CP context
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cpu180MacHaltCp(Cpu180Context *ctx)
    {
    ctx->isStopped = TRUE;

#if CcDebug > 0
    traceHaltCpu180(ctx);
#endif
#if DEBUG
    fprintf(cpu180Log, "Halt CPU%d\n", ctx->id);
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Maintenance access: master clear CP
**
**  Parameters:     Name        Description.
**                  ctx         CP context
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cpu180MacMasterClearCp(Cpu180Context *ctx)
    {
    ctx->isMonitorMode   = TRUE;
    ctx->isStopped       = TRUE;
    ctx->lastCsStartAddr = 0;
    cpu180MacSetCpStateRegister(ctx, RegDepEnvControl, 0);
#if CcDebug > 0
    traceMasterClearCpu180(ctx);
#endif
#if DEBUG
    fprintf(cpu180Log, "MasterClear CPU%d\n", ctx->id);
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Maintenance access: read CM data
**
**  Parameters:     Name        Description.
**
**  Returns:        next byte.
**
**------------------------------------------------------------------------*/
u8 cpu180MacReadCm(void)
    {
    u8  i;
    u8  shift;
    u64 word;

    if (memoryRegisterBufIdx < 8)
        {
        if (memoryRegisterBufIdx == 0)
            {
            word  = cpu180MacGetCmRegister((u8)memoryRegisterAddr);
            shift = 56;
            for (i = 0; i < 8; i++)
                {
                memoryRegisterBuf[i] = (word >> shift) & Mask8;
                shift -= 8;
                }
            }
        return memoryRegisterBuf[memoryRegisterBufIdx++];
        }
    else
        {
        return 0;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Maintenance access: read CP data
**
**  Parameters:     Name        Description.
**                  ctx         CP context
**                  type        type of data to read (type code from maintenance channel function)
**
**  Returns:        next byte.
**
**------------------------------------------------------------------------*/
u8 cpu180MacReadCp(Cpu180Context *ctx, u8 type)
    {
    u8  i;
    u8  shift;
    u64 word;

    switch (type)
        {
    case 0:
        if (ctx->macRegisterBufIdx < 8)
            {
            if (ctx->macRegisterBufIdx == 0)
                {
                word  = cpu180MacGetCpStateRegister(ctx, ctx->macRegisterAddr);
                shift = 56;
                for (i = 0; i < 8; i++)
                    {
                    ctx->macRegisterBuf[i] = (word >> shift) & Mask8;
                    shift -= 8;
                    }
                }
            return ctx->macRegisterBuf[ctx->macRegisterBufIdx++];
            }
        break;
    case 1:
        return ctx->controlStore[ctx->controlStoreIdx++];
    case 3:
    case 4:
    case 5:
    case 6:
        return ctx->softMemories[type][ctx->softMemoryIndices[type]++];
    case 7:
        return ctx->registerFile[ctx->registerFileIdx++];
    case 0xa:
        return cpu180MacReadCm();
    default:
        break;
        }

    return 0;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Maintenance access: set CM register location
**
**  Parameters:     Name        Description.
**                  location    the location
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cpu180MacSetCmLocation(u16 location)
    {
    memoryRegisterAddr   = location;
    memoryRegisterBufIdx = 0;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Maintenance access: set CP read/write location
**
**  Parameters:     Name        Description.
**                  ctx         CP context
**                  type        type of location (type code from maintenance channel function)
**                  location    the location
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cpu180MacSetCpLocation(Cpu180Context *ctx, u8 type, u16 location)
    {
    switch (type)
        {
    case 0:
        ctx->macRegisterAddr   = (u8)location;
        ctx->macRegisterBufIdx = 0;
        break;
    case 3:
    case 4:
    case 5:
    case 6:
        ctx->softMemoryIndices[type] = location << 2; // 4 bytes per memory address
        break;
    case 7:
        ctx->registerFileIdx = location << 3;         // 8 bytes per memory address
        break;
    case 0xa:
        cpu180MacSetCmLocation(location);
        break;
    default:
        break;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Maintenance access: set a value in a CM maintenance register
**
**  Parameters:     Name        Description.
**                  reg         the register address
**                  word        the 64-bit value to set
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cpu180MacSetCmRegister(u8 reg, u64 word)
    {
    switch (reg)
        {
    default:
        break;
    case MemBounds:
        memoryBounds = word;
        break;
    case MemElementId:
        memoryEid = (u32)word;
        break;
    case MemEnvControl:
        memoryEnvControl = word;
        break;
    case MemFreeRunningCounter:
        cpu180FreeRunningCounter = word;
        break;
    case MemOptionsInstalled:
        memoryOptions = word;
        break;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Maintenance access: set CP state register to a value
**
**  Parameters:     Name        Description.
**                  ctx         CP context
**                  reg         the register address
**                  word        the 64-bit value to set
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cpu180MacSetCpStateRegister(Cpu180Context *ctx, u8 reg, u64 word)
    {
#if DEBUG && DEBUG_SET_STATE_REG
    fprintf(cpu180Log, "Set CP%d state register %02x " FMT64_016x "\n", ctx->id, reg, word);
#endif
    switch (reg)
        {
    default:
        fprintf(stderr, "cpu180MacSetCpStateRegister: unknown reg %02x : " FMT64_016x "\n", reg, word);
        // fall through
    case RegFaultStatus0:
    case RegFaultStatus1:
    case RegFaultStatus2:
    case RegFaultStatus3:
    case RegFaultStatus4:
    case RegFaultStatus5:
    case RegFaultStatus6:
    case RegFaultStatus7:
    case RegFaultStatus8:
    case RegFaultStatus9:
    case RegFaultStatusA:
    case RegFaultStatusB:
    case RegFaultStatusC:
    case RegFaultStatusD:
    case RegFaultStatusE:
    case RegStatusSummary:
        break;
    case RegBaseConstant:
        ctx->regBc = (u32)word;
        break;
    case RegCtrlStoreAddr:
        ctx->controlStoreIdx = (u32)(word << 4); // 16 bytes per control store address
        break;
    case RegCtrlStoreBreak:
        ctx->controlStoreBreak = (u32)word;
        break;
    case RegDebugIndex:
        ctx->regDi = (u8)(word & Mask8);
        break;
    case RegDebugList:
        ctx->regDlp = word;
        break;
    case RegDebugMask:
        ctx->regDm = (u8)(word & Mask8);
        break;
    case RegDepEnvControl:
        ctx->regDec = word;
        break;
    case RegJobProcessState:
        ctx->regJps = (u32)(word & Mask32);
        break;
    case RegKeypointBuffer:
        ctx->regKbp = word & Mask48;
        break;
    case RegKeypointMask:
        ctx->regKmr = (u16)(word & Mask16);
        break;
    case RegModelDepFlags:
        ctx->regMdf = (u16)(word & Mask16);
        break;
    case RegModelDepWord:
        ctx->regMdw = word;
        break;
    case RegMonitorCondition:
        ctx->regMcr = (u16)(word & Mask16);
#if DEBUG && DEBUG_SET_STATE_REG
        fprintf(cpu180Log, "        MMR %04x MCR %04x\n", ctx->regMmr, ctx->regMcr);
#endif
        break;
    case RegMonitorMask:
        ctx->regMmr = (u16)(word & Mask16);
#if DEBUG && DEBUG_SET_STATE_REG
        fprintf(cpu180Log, "        MMR %04x MCR %04x\n", ctx->regMmr, ctx->regMcr);
#endif
        break;
    case RegMonitorProcState:
        ctx->regMps = (u32)(word & Mask32);
        break;
    case RegPageSizeMask:
        ctx->regPsm = word & Mask7;
        cpu180UpdatePageSize(ctx);
        break;
    case RegPageTableAddr:
        ctx->regPta = (u32)(word & Mask32);
        break;
    case RegPageTableLen:
        ctx->regPtl = (u8)(word & Mask8);
        cpu180UpdatePageSize(ctx);
        break;
    case RegProcessIntTimer:
        ctx->regPit = (u32)(word & Mask32);
#if DEBUG && DEBUG_SET_STATE_REG
        fprintf(cpu180Log, "        PIT " FMT32_08x " (%u)\n", ctx->regPit, ctx->regPit);
#endif
        if (ctx->regPit == 0)
            {
            ctx->regUcr |= ucrDefns[UCR51].bitMask;
            ctx->regPit  = 0xffffffff;
            }
        break;
    case RegRegisterP:
        ctx->key  = (word >> 48) & Mask6;
        ctx->regP = word & Mask48;
        break;
    case RegSegmentTableLen:
        ctx->regStl = (u16)(word & Mask16);
        break;
    case RegSegmentTableAddr:
        ctx->regSta = (u32)(word & Mask32);
        break;
    case RegSystemIntTimer:
        ctx->regSit = (u32)(word & Mask32);
#if DEBUG && DEBUG_SET_STATE_REG
        fprintf(cpu180Log, "        SIT " FMT32_08x " (%u)\n", ctx->regSit, ctx->regSit);
#endif
        if (ctx->regSit == 0)
            {
            ctx->regMcr |= mcrDefns[MCR59].bitMask;
            ctx->regSit  = 0xffffffff;
            }
        break;
    case RegTestMode:
        ctx->regTm = word;
        break;
    case RegTrapPointer:
        ctx->regTp = word & Mask48;
        break;
    case RegUntranslatablePtr:
        ctx->regUtp = word & Mask48;
        break;
    case  RegUserCondition:
        ctx->regUcr = (u16)(word & Mask16);
#if DEBUG && DEBUG_SET_STATE_REG
        fprintf(cpu180Log, "        UMR %04x UCR %04x\n", ctx->regUmr, ctx->regUcr);
#endif
        break;
    case  RegUserMask:
        ctx->regUmr = (u16)((word & Mask16) | 0xfe00);
#if DEBUG && DEBUG_SET_STATE_REG
        fprintf(cpu180Log, "        UMR %04x UCR %04x\n", ctx->regUmr, ctx->regUcr);
#endif
        break;
    case RegVmCapabilityList:
        ctx->regVmcl = (u16)(word & Mask16);
        break;
    //  Trap Enables addresses
    case 0xc0:
    case 0xc1:
    case 0xc2:
    case 0xc3:
        ctx->regFlags = (ctx->regFlags & 0xffc0) | (word & Mask2);
        break;
    //  Keypoint Enable addresses
    case 0xca:
    case 0xcb:
        ctx->regFlags = (u16)((ctx->regFlags & 0xdfff) | ((word & 1) << 13));
        break;
    //  Critical Frame Flag addresses
    case 0xe0:
    case 0xe1:
        ctx->regFlags = (u16)((ctx->regFlags & 0x7fff) | ((word & 1) << 15));
        break;
    //  On Condition Flag addresses
    case 0xe2:
    case 0xe3:
        ctx->regFlags = (u16)((ctx->regFlags & 0xbfff) | ((word & 1) << 14));
        break;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Maintenance access: start CP
**
**  Parameters:     Name        Description.
**                  ctx         CP context
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cpu180MacStartCp(Cpu180Context *ctx)
    {
    MonitorCondition cond;
    u64              csAddr;
    u32              pti;
    u32              rma;
    u32              xpa;

    //
    //  With CIP L826 for 860/870:
    //
    //  - When the CP is started at control store address 0x700,
    //    CIP is verifying control store and expects the CP to
    //    halt at address 0x705.
    //
    //  - When the CP is started at control store address 0x381,
    //    CIP has established the EI and is starting it.
    //
    csAddr = cpu180MacGetCpStateRegister(ctx, RegCtrlStoreAddr);
    if (csAddr == 0x700)
        {
        ctx->isStopped = TRUE; // Processor Halt
        cpu180MacSetCpStateRegister(ctx, RegCtrlStoreAddr, 0x705);
        }
    else if (csAddr == 0x381)
        {
        if (ctx->lastCsStartAddr != csAddr)
            {
            ctx->lastCsStartAddr = (u32)csAddr;
            xpa                  = ctx->regMps >> 3;
            if (xpa >= cpuMaxMemory)
                {
                logDtError(LogErrorLocation, "Failed to start CPU%d: MPS beyond end of memory, MPS " FMT32_08x " mem size %d Mbytes\n",
                    ctx->id, ctx->regMps, (cpuMaxMemory * 8) / OneMegabyte);
                ctx->isStopped = TRUE;
                return;
                }
            cpu180Load180Xp(ctx, ctx->regMps >> 3);
            ctx->nextKey = ctx->key;
            ctx->nextP   = ctx->regP;
            if (cpu180PvaToRma(ctx, ctx->regP, AccessModeNone, &rma, &pti, &cond))
                {
                ctx->isStopped = FALSE; // Processor started
#if CcDebug > 0
                traceStartCpu180(ctx, rma);
#endif
#if DEBUG
                fprintf(cpu180Log, "Start CPU%d at PVA " FMT64_012x " (RMA " FMT32_08x ")\n", ctx->id, ctx->regP, rma);
#endif
                }
            else
                {
                logDtError(LogErrorLocation, "Failed to start CPU%d: failed to translate PVA %012lx to RMA, MCR %04x\n",
                    ctx->id, ctx->regP, mcrDefns[cond].bitMask);
                }
            }
        }
    else
        {
        ctx->isStopped = TRUE; // Processor Halt
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Maintenance access: write CM data
**
**  Parameters:     Name        Description.
**                  byte        the byte to write
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cpu180MacWriteCm(u8 byte)
    {
    u8  i;
    u64 word;

    if (memoryRegisterBufIdx < 8)
        {
        memoryRegisterBuf[memoryRegisterBufIdx++] = byte;
        if (memoryRegisterBufIdx >= 8)
            {
            word = 0;
            for (i = 0; i < 8; i++)
                {
                word = (word << 8) | memoryRegisterBuf[i];
                }
            cpu180MacSetCmRegister((u8)memoryRegisterAddr, word);
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Maintenance access: write CP data
**
**  Parameters:     Name        Description.
**                  ctx         CP context
**                  type        type of data to read (type code from maintenance channel function)
**                  byte        the byte to write
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cpu180MacWriteCp(Cpu180Context *ctx, u8 type, u8 byte)
    {
    u8  i;
    u64 word;

    switch (type)
        {
    case 0:
        if (ctx->macRegisterBufIdx < 8)
            {
            ctx->macRegisterBuf[ctx->macRegisterBufIdx++] = byte;
            if (ctx->macRegisterBufIdx >= 8)
                {
                word = 0;
                for (i = 0; i < 8; i++)
                    {
                    word = (word << 8) | ctx->macRegisterBuf[i];
                    }
                cpu180MacSetCpStateRegister(ctx, ctx->macRegisterAddr, word);
                }
            }
        break;
    case 1:
        ctx->controlStore[ctx->controlStoreIdx++] = byte;
        break;
    case 3:
    case 4:
    case 5:
    case 6:
        ctx->softMemories[type][ctx->softMemoryIndices[type]++] = byte;
        break;
    case 7:
        ctx->registerFile[ctx->registerFileIdx++] = byte;
        break;
    case 0xa:
        cpu180MacWriteCm(byte);
        break;
    default:
        break;
        }
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
    if (address >= cpuMaxMemory)
        {
        if ((features & HasNoCmWrap) != 0)
            {
            *data = ~(CpWord)0;
            return;
            }
        address %= cpuMaxMemory;
        }
    *data = cpMem[address];
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
    if (address >= cpuMaxMemory)
        {
        if ((features & HasNoCmWrap) != 0)
            {
            return;
            }
        address %= cpuMaxMemory;
        }
    cpMem[address] = data;

#if CcDebug > 0 && defined(TRACE_STORE_START)
    address <<= 3;
    if (address >= traceRmaStart && address <= traceRmaEnd)
        {
        u8   b;
        char buf[128];
        char *cp;
        int  shift;

        sprintf(buf, "PP%02o write " FMT32_08x " " FMT64_016x, activePpu->id, address, data);
        cp = buf + strlen(buf);
        *cp++ = ' ';
        for (shift = 56; shift >= 0; shift -= 8)
            {
            b = (u8)((data >> shift) & Mask8);
            *cp++ = (b >= 0x20 && b < 0x7f) ? b : '.';
            }
        *cp = '\0';
        traceCpuPrint(&cpus170[0], buf);
        }
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Translate a PVA (process virtual address) to an RMA (real
**                  memory address).
**
**  Parameters:     Name        Description.
**                  ctx         pointer to C180 CPU context
**                  pva         PVA to be translated
**                  access      mode of memory access (execute, read, write)
**                              (determines whether to mark page as read/written,
**                               not used to validate access)
**                  rma         (out) resulting RMA
**                  pti         (out) page table index of PVA's page
**                  cond        (out) monitor condition, if translation fails
**
**  Returns:        TRUE if translation succeeded
**
**------------------------------------------------------------------------*/
bool cpu180PvaToRma(Cpu180Context *ctx, u64 pva, Cpu180AccessMode access, u32 *rma, u32 *pti, MonitorCondition *cond)
    {
    u32 byteNum;
    u16 asid;
    u8  n;
    u64 pte;
    u64 sde;
    u16 segNum;

#if CcDebug > 0
    tracePva(ctx, pva);
#endif

    if (Is32BitNeg(pva))
        {
        *cond = MCR52; // Address specification error
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
    segNum  = SegmentOf(pva);
    if (segNum > ctx->regStl)
        {
        *cond = MCR60; // Invalid segment
        ctx->regUtp = pva;
        return FALSE;
        }

    sde = cpMem[(ctx->regSta >> 3) + segNum];

#if CcDebug > 0
    traceSde(ctx, segNum, sde);
#endif

    if ((sde >> 63) == 0)
        {
        *cond = MCR60; // Invalid segment
        ctx->regUtp = pva;
        return FALSE;
        }

    asid    = (u16)((sde >> 32) & Mask16);
    byteNum = (u32)(pva & Mask32);

    if (cpu180FindPte(ctx, asid, byteNum, FALSE, pti, &n))
        {
        pte = cpMem[*pti];
        if ((access & AccessModeWrite) != 0)
            {
#if DEBUG && DEBUG_SET_PAGE_FLAGS
            if ((pte & ((u64)3 << 60)) == 0)
                {
                fprintf(cpu180Log, "PVA " FMT64_012x " PTI %08x PTE " FMT64_016x " set used and modified\n", pva, *pti, pte);
                traceStack(cpu180Log);
                }
#endif
            cpMem[*pti] |= (u64)3 << 60; // set page used and modified bits
            }
        else if ((access & (AccessModeRead | AccessModeExecute)) != 0)
            {
#if DEBUG && DEBUG_SET_PAGE_FLAGS
            if ((pte & ((u64)3 << 60)) == 0)
                {
                fprintf(cpu180Log, "PVA " FMT64_012x " PTI %08x PTE " FMT64_016x " set used\n", pva, *pti, pte);
                traceStack(cpu180Log);
                }
#endif
            cpMem[*pti] |= (u64)2 << 60; // set page used bit only
            }
        //
        //  See MIGDS 3-15 for diagram of RMA calculation
        //
        *rma = ((u32)(pte & Mask22) << 9)
            | ((byteNum & 0xfe00U) & ((u32)(~ctx->regPsm & Mask7) << 9))
            | (byteNum & Mask9);

#if CcDebug > 0
        traceRma(ctx, *rma);
#if defined(TRACE_STORE_START)
        if (pva >= (TRACE_STORE_START) && pva <= (TRACE_STORE_END))
            {
            if (*rma < traceRmaStart || traceRmaStart == 0)
                {
                traceRmaStart = *rma;
                }
            if (*rma > traceRmaEnd)
                {
                traceRmaEnd = *rma;
                }
            }
#endif
#endif

        return TRUE;
        }
    /*
    **  Page not found, set page fault
    */
    *cond = MCR57; // Page table search without find
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
    action       = cpu180GetActionForMonitorCondition(ctx, cond);
    if (action > ctx->pendingAction)
        {
        ctx->pendingAction = action;
        if (action > Stack && defn->isThis)
            {
            ctx->nextP = ctx->regP;
            }
        }
#if CcDebug > 0
    traceMonitorCondition(ctx, cond);
    if ((traceMask & TRACECPU(ctx, TraceCpu180 | TraceConditions)) == TRACECPU(ctx, TraceConditions))
        {
        traceMask              |= TRACECPU(ctx, TraceCpu180 | TraceExchange | TraceCallFrame | TraceBlockOp);
        traceInstCount[ctx->id] = 100;
        }

#endif
#if DEBUG && DEBUG_INTERRUPT
    fprintf(cpu180Log, "Set monitor condition MCR%d, MCR %04x MMR %04x Op %02x PVA " FMT64_012x "\n",
        cond + 48, ctx->regMcr, ctx->regMmr, ctx->opCode, ctx->regP);
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
    action       = cpu180GetActionForUserCondition(ctx, cond);

    if (action > ctx->pendingAction)
        {
        ctx->pendingAction = action;
        if (action > Stack && defn->isThis)
            {
            ctx->nextP = ctx->regP;
            }
        }
#if CcDebug > 0
    traceUserCondition(ctx, cond);
    if ((traceMask & TRACECPU(ctx, TraceCpu180 | TraceConditions)) == TRACECPU(ctx, TraceConditions))
        {
        traceMask              |= TRACECPU(ctx, TraceCpu180 | TraceExchange | TraceCallFrame | TraceBlockOp);
        traceInstCount[ctx->id] = 100;
        }
#endif
#if DEBUG && DEBUG_INTERRUPT
    fprintf(cpu180Log, "Set user condition UCR%d, UCR %04x UMR %04x Op %02x PVA " FMT64_012x "\n",
        cond + 48, ctx->regUcr, ctx->regUmr, ctx->opCode, ctx->regP);
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
#if CcDebug > 0
    u64        oldRegP;
#endif

    /*
    **  First, check for interrupt conditions and initiate trap or
    **  exchange operations, or halt the CPU as required.
    */
/*DELETE*/ if ((activeCpu->regMcr & 0x00a0) == 0x00a0)
/*DELETE*/     {
/*DELETE*/     fprintf(stderr, "CPU%d MCR %04x MMR %04x P " FMT64_012x " mtr %d\n", activeCpu->id, activeCpu->regMcr, activeCpu->regMmr, activeCpu->regP, activeCpu->isMonitorMode);
/*DELETE*/     }
    cpu180CheckConditions(activeCpu);

    if (activeCpu->pendingAction > Stack)
        {
        if (activeCpu->pendingAction == Trap)
            {
#if DEBUG && DEBUG_INTERRUPT
            fprintf(cpu180Log, "Trap interrupt: P " FMT64_012x " MCR %04x MMR %04x UCR %04x UMR %04x\n",
                activeCpu->regP, activeCpu->regMcr, activeCpu->regMmr, activeCpu->regUcr, activeCpu->regUmr);
#endif
            cpu180Trap(activeCpu);
            }
        switch (activeCpu->pendingAction)
            {
        case Exch:
#if DEBUG && DEBUG_INTERRUPT
            fprintf(cpu180Log, "Exchange interrupt: P " FMT64_012x " MCR %04x MMR %04x UCR %04x UMR %04x\n",
                activeCpu->regP, activeCpu->regMcr, activeCpu->regMmr, activeCpu->regUcr, activeCpu->regUmr);
#endif
            activeCpu->pendingAction = Rni;
            cpu180Exchange(activeCpu);
            return;

        case Halt:
#if DEBUG && DEBUG_INTERRUPT
            fprintf(cpu180Log, "Halt: P " FMT64_012x " MCR %04x MMR %04x UCR %04x UMR %04x\n",
                activeCpu->regP, activeCpu->regMcr, activeCpu->regMmr, activeCpu->regUcr, activeCpu->regUmr);
#endif
            activeCpu->isStopped = TRUE;
            return;

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

#if CcDebug > 0
        oldRegP = activeCpu->regP;
        if (traceInstCount[activeCpu->id] > 0)
            {
            traceInstCount[activeCpu->id] -= 1;
            if (traceInstCount[activeCpu->id] < 1)
                {
                traceMask &= ~TRACECPU(activeCpu, TraceCpu180 | TraceExchange | TraceCallFrame | TraceBlockOp);
                }
            }
#if defined(TRACE_INST_LIST)
        if ((traceMask & TRACECPU(activeCpu, TraceCpu180)) == 0 && memchr(traceInstList, activeCpu->opCode, sizeof(traceInstList)) != NULL)
            {
            traceMask                    |= TRACECPU(activeCpu, TraceCpu180 | TraceExchange | TraceCallFrame | TraceBlockOp);
            traceInstCount[activeCpu->id] = TRACE_INST_COUNT;
            traceCpuBreak(activeCpu);
            }
#endif
#if defined(TRACE_RANGE_START)
        if (activeCpu->regP >= (TRACE_RANGE_START) && activeCpu->regP <= (TRACE_RANGE_END))
            {
            if ((traceMask & TRACECPU(activeCpu, TraceCpu180)) == 0)
                {
                traceCpuBreak(activeCpu);
                }
            traceMask |= TRACECPU(activeCpu, TraceCpu180 | TraceExchange | TraceCallFrame | TraceBlockOp);
            }
        else if (traceInstCount[activeCpu->id] < 1)
            {
            traceMask &= ~TRACECPU(activeCpu, TraceCpu180 | TraceExchange | TraceCallFrame | TraceBlockOp);
            }
#endif
#endif

        activeCpu->nextKey = activeCpu->key;
        activeCpu->nextP   = activeCpu->regP + length;
        odp->execute(activeCpu);
        activeCpu->key     = activeCpu->nextKey;
        activeCpu->regP    = activeCpu->nextP;

#if CcDebug > 0
        traceCpu180(activeCpu, oldRegP, activeCpu->opCode, activeCpu->opI, activeCpu->opJ, activeCpu->opK, activeCpu->opD, activeCpu->opQ);
#endif
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Store the 170 state exchange package into memory
**                  referenced by a specified real memory word address.
**
**  Parameters:     Name        Description.
**                  ctx         pointer to CYBER 180 CPU context
**                  xpa         word address of exchange package
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
void cpu180Store170Xp(Cpu180Context *ctx, u32 xpa)
    {
    cpu180Get170State(ctx);
    cpu180Store180Xp(ctx, xpa);

#if CcDebug > 0
    traceExchange170(&cpus170[ctx->id], xpa << 3, NULL, (traceMask & TRACECPU(ctx, TraceCpu180)) != 0);
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Translate a sequence of PVA's to RMA's.
**
**  Parameters:     Name        Description.
**                  ctx         Pointer to CPU context
**                  pva         first PVA in sequence
**                  count       number of PVA's in sequence
**                  incr        increment between addresses
**                  ring        ring number for which to validate access
**                              (e.g., caller's ring number)
**                  access      mode of access (execute, read, write)
**                  rmas        (out) sequence of translated RMA's
**                  cond        (out) monitor condition if error condition detected
**
**  Returns:        TRUE if sequence translated successfully. FALSE otherwise,
**                  and MCR set accordingly.
**
**------------------------------------------------------------------------*/
bool cpu180TranslatePvaSequence(Cpu180Context *ctx, u64 pva, u16 count, u8 incr, u8 ring, Cpu180AccessMode access, u32 *rmas, MonitorCondition *cond)
    {
    u16              i;
    u32              pageNum;
    u32              pn;
    u32              pti;
    u32              rma;

    if (count > 0)
        {
        if (cpu180ValidateAccess(ctx, pva, ring, access, cond) == FALSE
            || cpu180PvaToRma(ctx, pva, access, &rma, &pti, cond) == FALSE)
            {
            return FALSE;
            }
        *rmas++ = rma;
        pageNum = PageOf(pva, ctx);
        for (i = 1; i < count; i++)
            {
            pva += incr;
            rma += incr;
            pn   = PageOf(pva, ctx);
            if (pn != pageNum)
                {
                if (cpu180PvaToRma(ctx, pva, access, &rma, &pti, cond) == FALSE)
                    {
                    return FALSE;
                    }
                pageNum = pn;
                }
            *rmas++ = rma;
            }
        }
    return TRUE;
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
void cpu180Trap(Cpu180Context *ctx)
    {
    u64              cbp;
    MonitorCondition cond;
    u32              pti;
    u32              rma;

    if (ctx->regVmid == 1) // if trap from 170 state, map 170 to 180 exchange package
        {
        cpu180Get170State(ctx);
        ctx->nextP = ctx->regP;
        }

#if CcDebug > 0
    traceTrap(ctx);
#endif

    ctx->pendingAction = Rni;

    if (cpu180ValidateAccess(ctx, ctx->regTp, RingOf(ctx->regP), AccessModeRead, &cond) == FALSE
        || cpu180PvaToRma(ctx, ctx->regTp, AccessModeRead, &rma, &pti, &cond) == FALSE)
        {
        cpu180SetMonitorCondition(ctx, cond);
        ctx->regMcr       |= mcrDefns[MCR63].bitMask;
        ctx->pendingAction = cpu180GetActionForTrapCondition(ctx, cond);
        return;
        }
    cbp = cpMem[rma >> 3];
    if (((cbp >> 56) & Mask4) != 0 || ((cbp >> 55) & 1) == 0) // not VMID 0 or not external flag
        {
        cpu180SetMonitorCondition(ctx, MCR55); // Environment specification error
        ctx->regMcr       |= mcrDefns[MCR63].bitMask;
        ctx->pendingAction = cpu180GetActionForTrapCondition(ctx, MCR55);
        return;
        }
    if (cpu180CallIndirect(ctx, ctx->regTp, cbp, ctx->regA[4], 0xf, 0x0, 0xf, TRUE, &cond) == FALSE)
        {
        cpu180SetMonitorCondition(ctx, cond);
        ctx->regMcr       |= mcrDefns[MCR63].bitMask;
        ctx->pendingAction = cpu180GetActionForTrapCondition(ctx, cond);
        return;
        }

    ctx->regP      = ctx->nextP;
    ctx->key       = ctx->nextKey;
    ctx->regFlags &= 0xfffd;       // clear trap enable flag
    ctx->regMcr   &= ~ctx->regMmr; // clear masked bits in MCR
//  ctx->regMcr   &= ~(ctx->regMmr | 0x0021); // clear masked bits and status bits in MCR
    ctx->regUcr   &= ~ctx->regUmr; // clear masked bits in UCR
    }

/*--------------------------------------------------------------------------
**  Purpose:        Update system and process interval timers.
**
**  Parameters:     Name        Description.
**                  delta       number of usecs by which to update timers
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cpu180UpdateIntervalTimers(u32 delta)
    {
    Cpu180Context *ctx;
    int           i;
    u32           oldIt;

    for (i = 0; i < cpuCount; i++)
        {
        ctx          = &cpus180[i];
        oldIt        = ctx->regSit;
        ctx->regSit -= (u32)delta;
        if (ctx->regSit > oldIt || ctx->regSit == 0)
            {
            ctx->regMcr |= mcrDefns[MCR59].bitMask;
            ctx->regSit  = 0xffffffff;
            }
        oldIt        = ctx->regPit;
        ctx->regPit -= (u32)delta;
        if (ctx->regPit > oldIt || ctx->regPit == 0)
            {
            ctx->regUcr |= ucrDefns[UCR51].bitMask;
            ctx->regPit  = 0xffffffff;
            }
        }
    cpu180FreeRunningCounter += delta;
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
**  Returns:        TRUE if arithmetic overflow exception not indicated.
**                  FALSE if overflow detected and user condition register
**                  bit is set.
**
**------------------------------------------------------------------------*/
static bool cpu180AddInt32(Cpu180Context *ctx, u32 augend, u32 addend, u32 *sum)
    {
    u16 mask;

    *sum = augend + addend;
    if ((i32)(augend ^ addend) >= 0 && (i32)(*sum ^ augend) < 0)
        {
        mask = ucrDefns[UCR57].bitMask;
        if ((ctx->regUmr & mask) != 0 && IsTrapEnabled(ctx)) // mask set and trap enabled
            {
            cpu180SetUserCondition(ctx, UCR57);
            return FALSE;
            }
        ctx->regUcr |= mask;
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
**  Returns:        TRUE if arithmetic overflow exception not indicated.
**                  FALSE if overflow detected and user condition register
**                  bit is set.
**
**------------------------------------------------------------------------*/
static bool cpu180AddInt64(Cpu180Context *ctx, u64 augend, u64 addend, u64 *sum)
    {
    u16 mask;

    *sum = augend + addend;
    if ((i64)(augend ^ addend) >= 0 && (i64)(*sum ^ augend) < 0)
        {
        mask = ucrDefns[UCR57].bitMask;
        if ((ctx->regUmr & mask) != 0 && IsTrapEnabled(ctx)) // mask set and trap enabled
            {
            cpu180SetUserCondition(ctx, UCR57);
            return FALSE;
            }
        ctx->regUcr |= mask;
        }

    return TRUE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Apply a BDP operator to the source and destination
**                  descriptors of a BDP instruction
**
**  Parameters:     Name        Description.
**                  ctx         pointer to CPU context
**                  operator    pointer to the operator to apply
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cpu180ApplyBdpOperator(Cpu180Context *ctx, bool (*operator)(BdpOperand *src, BdpOperand *dst, BdpOperand *result, UserCondition *cond))
    {
    UserCondition cond;
    BdpOperand    dstOperand;
    bool          inhOnCond;
    bool          isOk;
    bool          isTruncated;
    BdpOperand    result;
    BdpOperand    srcOperand;

    if (cpu180GetBdpDescriptor(ctx, ctx->regP + 2, ctx->opJ, 0, &ctx->srcDesc)
        && cpu180GetBdpDescriptor(ctx, ctx->regP + 6, ctx->opK, 1, &ctx->dstDesc))
        {
        if (ctx->srcDesc.type > 6 || ctx->dstDesc.type > 6)
            {
            cpu180SetMonitorCondition(ctx, MCR51); // Instruction specification error
            return;
            }
        if (bdp180DecodeOperand(ctx, &ctx->dstDesc, &dstOperand)
            && bdp180DecodeOperand(ctx, &ctx->srcDesc, &srcOperand))
            {
#if CcDebug > 0
            traceMemoryBlock(ctx, ctx->srcDesc.pva, ctx->srcDesc.length, "    source block:");
            traceMemoryBlock(ctx, ctx->dstDesc.pva, ctx->dstDesc.length, "    destination block:");
#endif
            if (ctx->dstDesc.length > 0)
                {
                isOk = (*operator)(&dstOperand, &srcOperand, &result, &cond);
                if (isOk == FALSE)
                    {
                    cpu180SetUserCondition(ctx, cond);
                    inhOnCond = (ctx->regUmr & ucrDefns[cond].bitMask) != 0 && IsTrapEnabled(ctx);
                    if (inhOnCond && cond == UCR55) // Divide fault and execution inhibited
                        {
                        ctx->nextP = ctx->regP;
                        }
                    }
                else
                    {
                    inhOnCond = FALSE;
                    }
                if ((isOk || inhOnCond == FALSE) && bdp180EncodeOperand(ctx, &ctx->dstDesc, &result, inhOnCond, &isTruncated))
                    {
                    if (isTruncated)
                        {
                        cpu180SetUserCondition(ctx, UCR57); // Arithmetic overflow
                        }
                    }
                }
#if CcDebug > 0
            traceMemoryBlock(ctx, ctx->dstDesc.pva, ctx->dstDesc.length, "    result destination block:");
#endif
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Initiate a CYBER 180 indirect procedure call
**
**  Parameters:     Name        Description.
**                  ctx         pointer to CPU context
**                  bsp         binding section pointer
**                  cbp         codebase pointer, prefetched from binding section
**                  pp          parameter pointer (new value for A4)
**                  at          terminating A register
**                  xs          starting X register
**                  xt          terminating X register
**                  doSaveCrs   TRUE if MCR/UCR to be saved in stack frame
**                  cond        (out) monitor condition if error condition detected
**
**  Returns:        TRUE if successful. FALSE if error condition detected.
**
**------------------------------------------------------------------------*/
static bool cpu180CallIndirect(Cpu180Context *ctx, u64 bsp, u64 cbp, u64 pp, u8 at, u8 xs, u8 xt, bool doSaveCrs, MonitorCondition *cond)
    {
    u64  callee;
    bool isExt;
    u32  frameSize;
    u8   lock;
    u32  pti;
    u8   r1;
    u8   r2;
    u8   ringP;
    u32  rma;
    u64  sfsa;
    u8   vmid;

    callee = cbp & Mask48;
    if (RingOf(bsp) > ((cbp >> 48) & Mask4))
        {
        ctx->regUtp = callee;
        *cond       = MCR54; // Access violation
        return FALSE;
        }
    ringP = RingOf(ctx->regP);
    if (cpu180GetR1(ctx, callee, &r1, cond) == FALSE || cpu180GetR2(ctx, callee, &r2, cond) == FALSE)
        {
        return FALSE;
        }
    if (ringP < r1)
        {
        ctx->regUtp = callee;
        *cond       = MCR61; // Outward call
        return FALSE;
        }
    if (ringP >= r2) // call to inner ring
        {
        callee = ((u64)r2 << 44) | (callee & Mask44);
        }
    else // intraring call
        {
        callee = ((u64)ringP << 44) | (callee & Mask44);
        }

#if CcDebug > 0
    if ((traceMask & TRACECPU(ctx, TraceCallFrame)) != 0)
        {
        char buf[128];
        sprintf(buf, "  Callee " FMT64_012x " R1 %x R2 %x Ring P %x TOS[%x] " FMT64_012x, callee, r1, r2, ringP,
            (u8)RingOf(callee), ctx->regTos[RingOf(callee)]);
        traceCpuPrint(&cpus170[ctx->id], buf);
        }
#endif

    if (((callee & Mask3) != 0) || Is32BitNeg(callee))
        {
        ctx->regUtp = callee;
        *cond       = MCR51; // Address specification error
        return FALSE;
        }
    if (cpu180ValidateAccess(ctx, ctx->regA[0], RingOf(ctx->regA[0]), AccessModeWrite, cond) == FALSE)
        {
        return FALSE;
        }
    vmid  = (cbp >> 56) & Mask4;
    isExt = vmid == 0 && ((cbp >> 55) & 1) != 0;
    if (isExt)
        {
        if (cpu180PvaToRma(ctx, bsp + 8, AccessModeRead, &rma, &pti, cond) == FALSE)
            {
            return FALSE;
            }
        bsp = cpMem[rma >> 3] & Mask48;
        }
    if (cpu180PushFrame(ctx, at, xs, xt, doSaveCrs, &sfsa, &frameSize, cond) == FALSE)
        {
        return FALSE;
        }
    ctx->regTos[ringP] = sfsa + frameSize;
    if (ringP > ctx->regLrn)
        {
        ctx->regLrn = ringP;
        }
    if (cpu180GetLock(ctx, callee, &lock, cond) == FALSE)
        {
        return FALSE;
        }
    ctx->nextKey = lock;
    ctx->nextP   = callee;

    if (isExt)
        {
        if ((bsp & RingMask) < (callee & RingMask))
            {
            ctx->regA[3] = (callee & RingMask) | (bsp & Mask44);
            }
        else
            {
            ctx->regA[3] = bsp;
            }
        }
    ctx->regA[0] = ctx->regTos[RingOf(callee)];
    ctx->regA[1] = ctx->regA[0];
    ctx->regA[2] = sfsa;
    if (vmid == 0)
        {
        ctx->regA[4] = pp;
        }
    ctx->regVmid   = vmid;
    ctx->regFlags &= 0x3fff; // clear CFF and OCF

    if (vmid == 1) // call 170 procedure
        {
        cpu180Set170State(ctx, ctx->nextP);
        }

#if CcDebug > 0
    if (doSaveCrs)
        {
        traceTrapFrame(ctx, sfsa);
        }
    else
        {
        traceCallFrame(ctx, sfsa, "pushed");
        }
    traceCall(ctx, callee);
#endif

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
    ConditionAction  action;
    u16              cr;
    u16              mask;
    MonitorCondition mCond;

    cr = ctx->regMcr & ctx->regMmr;
    for (mCond = MCR48; cr != 0 && mCond <= MCR63; mCond++)
        {
        mask = mcrDefns[mCond].bitMask;
        if ((cr & mask) != 0)
            {
            action = cpu180GetActionForMonitorCondition(ctx, mCond);
            if (action > ctx->pendingAction)
                {
                ctx->pendingAction = action;
                }
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
    ConditionAction  action;
    u16              cr;
    u16              mask;
    UserCondition    uCond;

    cr = ctx->regUcr & ctx->regUmr;
    for (uCond = UCR48; cr != 0 && uCond <= UCR63; uCond++)
        {
        mask = ucrDefns[uCond].bitMask;
        if ((cr & mask) != 0)
            {
            action = cpu180GetActionForUserCondition(ctx, uCond);
            if (action > ctx->pendingAction)
                {
                ctx->pendingAction = action;
                }
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
        }
    else if (vmid == 1 && activeCpu->isMonitorMode) // 180 -> 170 state exchange
        {
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
    hash    = (u32)asid ^ (pageNum & Mask16);
    idx     = ((ctx->regPta & 0xfffff000) | ((hash << 4) & ctx->pageLengthMask)) >> 3;
    spid    = ((u64)asid << 22) | ((u64)pageNum << ctx->spidShift);

#if CcDebug > 0
    tracePageInfo(ctx, hash, pageNum, idx, spid);
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

#if CcDebug > 0
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
**  Purpose:        Copy the CYBER 170 state registers to the CYBER 180 state
**                  registers.
**
**  Parameters:     Name        Description.
**                  ctx         pointer to CYBER 180 CPU context
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cpu180Get170State(Cpu180Context *ctx)
    {
    Cpu170Context *ctx170;
    u8            i;
    u64           ring;
    u64           word;

    ctx170       = &cpus170[ctx->id];
    ctx->regP    = ringSeg170
                   | (((u64)ctx170->regRaCm + (u64)ctx170->regP) << 3)
                   | (u64)(((4 - (ctx170->opOffset / 15)) & Mask2) << 1);
    ring         = ctx->regP & RingMask;
    ctx->regA[3] = ring | ((u64)ctx170->exitMode << 20) | ctx170->regRaCm;
    ctx->regA[4] = ring | (ctx170->isMonitorMode ? (u64)1 << 32 : 0) | ctx170->regFlCm;
    ctx->regA[5] = ring | ctx170->regMa;
    ctx->regA[6] = ring | ctx170->regRaEcs;
    ctx->regA[7] = ring | ctx170->regFlEcs;
    for (i = 0; i < 8; i++)
        {
        ctx->regA[i + 8] = ring | ctx170->regA[i];
        }
    for (i = 1; i < 8; i++)
        {
        ctx->regX[i] = ctx170->regB[i];
        }
    for (i = 0; i < 8; i++)
        {
        word = ctx170->regX[i];
        if ((word & 0x0800000000000000) != 0)
            {
            word |= 0xf000000000000000;
            }
        ctx->regX[i + 8] = word;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Get the action associated with a monitor condition
**
**  Parameters:     Name        Description.
**                  ctx         pointer to CPU context
**                  cond        monitor condition ordinal
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static ConditionAction cpu180GetActionForMonitorCondition(Cpu180Context *ctx, MonitorCondition cond)
    {
    ConditionAction     action;
    ConditionActionDefn *defn;

    defn = &mcrDefns[cond];

    if ((ctx->regMmr & defn->bitMask) == 0)
        {
        action = defn->whenNoMask;
        }
    else if (IsTrapEnabled(ctx))
        {
        action = ctx->isMonitorMode ? defn->whenMaskTrapMonitor : defn->whenMaskTrapJob;
        }
    else
        {
        action = ctx->isMonitorMode ? defn->whenMaskNoTrapMonitor : defn->whenMaskNoTrapJob;
        }

    return action;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Get the action associated with a monitor condition that
**                  occurs while handling a trap
**
**  Parameters:     Name        Description.
**                  ctx         pointer to CPU context
**                  cond        monitor condition ordinal
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static ConditionAction cpu180GetActionForTrapCondition(Cpu180Context *ctx, MonitorCondition cond)
    {
    ConditionAction     action;
    ConditionActionDefn *defn;

    defn = &mcrDefns[cond];

    if ((ctx->regMmr & defn->bitMask) == 0)
        {
        action = defn->whenNoMask;
        }
    else
        {
        action = ctx->isMonitorMode ? defn->whenMaskNoTrapMonitor : defn->whenMaskNoTrapJob;
        }

    return action;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Get the action associated with a user condition
**
**  Parameters:     Name        Description.
**                  ctx         pointer to CPU context
**                  cond        user condition ordinal
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static ConditionAction cpu180GetActionForUserCondition(Cpu180Context *ctx, UserCondition cond)
    {
    ConditionAction     action;
    ConditionActionDefn *defn;

    defn = &ucrDefns[cond];

    if ((ctx->regUmr & defn->bitMask) == 0)
        {
        action = defn->whenNoMask;
        }
    else if (IsTrapEnabled(ctx))
        {
        action = ctx->isMonitorMode ? defn->whenMaskTrapMonitor : defn->whenMaskTrapJob;
        }
    else
        {
        action = ctx->isMonitorMode ? defn->whenMaskNoTrapMonitor : defn->whenMaskNoTrapJob;
        }

    return action;
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
    u32 disp;
    u32 operandAddress;

    if (cpu180GetBytes(ctx, pva, 4, RingOf(ctx->regP), AccessModeExecute, &desc))
        {
        descriptor->rawDesc = (u32)desc;
        descriptor->type    = (desc >> 24) & Mask4;
        descriptor->length  = Is32BitNeg(desc) ? ctx->regX[xRegNum] & Mask9 : (desc >> 16) & Mask8;
        operandAddress      = (u32)(desc & Mask16);
        disp                = (operandAddress <= 0x7fff) ? operandAddress : 0xffff0000 | operandAddress;
        descriptor->pva     = (ctx->regA[aRegNum] & RingSegMask) | ((ctx->regA[aRegNum] + disp) & Mask32);
        return TRUE;
        }
    else
        {
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
**                  ring        ring from which access is being made
**                  access      access mode (read or execute)
**                  word        (out) pointer to assembled bytes, right justified
**
**  Returns:        TRUE if successful, FALSE if address specification error,
**                  access violation, or page fault
**
**------------------------------------------------------------------------*/
static bool cpu180GetBytes(Cpu180Context *ctx, u64 pva, u8 count, u8 ring, Cpu180AccessMode access, u64 *word)
    {
    MonitorCondition cond;
    u8               i;
    u32              rma;
    u32              rmas[8];
    u8               shift;

    if ((pva & Mask3) == 0) // optimization: word-aligned load
        {
        if (cpu180TranslatePvaSequence(ctx, pva, 1, count, ring, access, rmas, &cond) == FALSE)
            {
            cpu180SetMonitorCondition(ctx, cond);
            return FALSE;
            }
        if (count < 8)
            {
            *word = cpMem[rmas[0] >> 3] >> ((8 - count) << 3);
            }
        else
            {
            *word = cpMem[rmas[0] >> 3];
            }
        }
    else if (cpu180TranslatePvaSequence(ctx, pva, count, 1, ring, access, rmas, &cond))
        {
        *word = 0;
        for (i = 0; i < count; i++)
            {
            rma   = rmas[i];
            shift = (u8)(56 - ((rma & Mask3) << 3));
            *word = (*word << 8) | ((cpMem[rma >> 3] >> shift) & Mask8);
            }
        }
    else
        {
        cpu180SetMonitorCondition(ctx, cond);
        return FALSE;
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
**                  lock        (out) the lock field
**                  cond        (out) monitor condition if error condition detected
**
**  Returns:        TRUE if success. FALSE if error condition detected.
**
**------------------------------------------------------------------------*/
static bool cpu180GetLock(Cpu180Context *ctx, u64 pva, u8 *lock, MonitorCondition *cond)
    {
    u64 sd;
    u16 segNum;

    segNum  = SegmentOf(pva);
    if (segNum <= ctx->regStl)
        {
        sd = cpMem[(ctx->regSta >> 3) + segNum];
        if ((sd >> 63) == 1)
            {
            *lock = (sd >> 24) & Mask6;
            return TRUE;
            }
        }
    ctx->regUtp = pva;
    *cond       = MCR60; // Invalid segment
    return FALSE;
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
    u32              pti;
    u32              rma;
    u8               shift;
    u64              word;

    if (cpu180PvaToRma(ctx, pva, AccessModeExecute, &rma, &pti, &cond))
        {
        word    = cpMem[rma >> 3];
        shift   = (u8)(48 - ((rma & 6) << 3));
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
**                  r1          (out) the R1 field
**                  cond        (out) monitor condition if error condition detected
**
**  Returns:        TRUE if success. FALSE if error condition detected.
**
**------------------------------------------------------------------------*/
static bool cpu180GetR1(Cpu180Context *ctx, u64 pva, u8 *r1, MonitorCondition *cond)
    {
    u64 sd;
    u16 segNum;

    segNum  = SegmentOf(pva);
    if (segNum <= ctx->regStl)
        {
        sd = cpMem[(ctx->regSta >> 3) + segNum];
        if ((sd >> 63) == 1)
            {
            *r1 = (sd >> 52) & Mask4;
            return TRUE;
            }
        }
    *cond       = MCR60; // Invalid segment
    ctx->regUtp = pva;
    return FALSE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Get the R2 field defined for the segment of a PVA
**
**  Parameters:     Name        Description.
**                  ctx         pointer to C180 CPU context
**                  pva         the PVA for which to obtain associated R2 field
**                  r2          (out) the R2 field
**                  cond        (out) monitor condition if error condition detected
**
**  Returns:        TRUE if success. FALSE if error condition detected.
**
**------------------------------------------------------------------------*/
static bool cpu180GetR2(Cpu180Context *ctx, u64 pva, u8 *r2, MonitorCondition *cond)
    {
    u64 sd;
    u16 segNum;

    segNum  = SegmentOf(pva);
    if (segNum <= ctx->regStl)
        {
        sd = cpMem[(ctx->regSta >> 3) + segNum];
        if ((sd >> 63) == 1)
            {
            *r2 = (sd >> 48) & Mask4;
            return TRUE;
            }
        }
    *cond       = MCR60; // Invalid segment
    ctx->regUtp = pva;
    return FALSE;
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

    segNum = SegmentOf(pva);
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
**  Purpose:        Load the 170 state exchange package referenced by a
**                  specified real memory word address.
**
**  Parameters:     Name        Description.
**                  ctx         pointer to CYBER 180 CPU context
**                  xpa         word address of exchange package
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
static void cpu180Load170Xp(Cpu180Context *ctx, u32 xpa)
    {
    cpu180Load180Xp(ctx, xpa);
    ringSeg170 = cpMem[xpa] & LeftMask;
    cpu180Set170State(ctx, ctx->regP);

#if CcDebug > 0
    traceExchange170(&cpus170[ctx->id], xpa << 3, NULL, (traceMask & TRACECPU(ctx, TraceCpu180)) != 0);
#endif
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
    u32 lower32;
    u16 mask;
    u64 p64;
    u32 upper32;

    if ((i32)mltand < 0)
        {
        if ((i32)mltier < 0)
            {
            p64 = ((u64)mltand | (u64)0xffffffff00000000) * ((u64)mltier | (u64)0xffffffff00000000);
            }
        else
            {
            p64 = (u64)mltier * ((u64)mltand | (u64)0xffffffff00000000);
            }
        }
    else if ((i32)mltier < 0)
        {
        p64 = (u64)mltand * ((u64)mltier | (u64)0xffffffff00000000);
        }
    else
        {
        p64 = (u64)mltand * (u64)mltier;
        }
    upper32 = (u32)(p64 >> 32);
    lower32 = (u32)(p64 & Mask32);
    if ((lower32 <= 0x7fffffffU && upper32 != 0)
        || (lower32 > 0x7fffffffU && upper32 != 0xffffffffU))
        {
        mask = ucrDefns[UCR57].bitMask;
        if ((ctx->regUmr & mask) != 0 && IsTrapEnabled(ctx)) // mask set and trap enabled
            {
            cpu180SetUserCondition(ctx, UCR57);
            return FALSE;
            }
        ctx->regUcr |= mask;
        }
    *product = (u32)(p64 & Mask32);

    return TRUE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Multiply two 64-bit integer quantities and detect overflow
**
**  Parameters:     Name        Description.
**                  ctx         pointer to CPU context
**                  mltand      the multiplicand
**                  mltier      the multiplier
**                  product     (out) pointer to product
**
**  Returns:        TRUE if arithmetic overflow not detected.
**                  FALSE if overflow detected and user condition register
**                  bit set.
**
**------------------------------------------------------------------------*/
static bool cpu180MulInt64(Cpu180Context *ctx, u64 mltand, u64 mltier, u64 *product)
    {
    bool isResultNeg;
    u64  lower64;
    u16  mask;
    u64  upper64;
#if defined(_WIN32)
    u64 carry;
    u64 m128[2];
    u64 t;
#else
    u128 p128;
#endif

    isResultNeg = ((mltand ^ mltier) >> 63) != 0;
    if ((mltand >> 63) != 0)
        {
        mltand = ~mltand + 1;
        }
    if ((mltier >> 63) != 0)
        {
        mltier = ~mltier + 1;
        }

#if defined(_WIN32)
    m128[0] = 0;
    m128[1] = mltand;
    lower64 = 0;
    upper64 = 0;
    while (mltier != 0)
        {
        //
        //  If the LSB of multiplier is 1, add multiplicand to product
        //
        if ((mltier & 1) != 0)
            {
            t        = lower64;
            lower64 += m128[1];
            carry    = lower64 < t;
            upper64 += m128[0] + carry;
            }
        //
        //  Left shift multiplicand (multiply by 2)
        //
        m128[0]   = (m128[0] << 1) | (m128[1] >> 63);
        m128[1] <<= 1;
        //
        //  Right shift multiplier (divide by 2)
        //
        mltier >>= 1;
        }
#else
    p128    = (u128)mltand * (u128)mltier;
    lower64 = (u64)p128;
    upper64 = (u64)(p128 >> 64);
#endif

    if (lower64 > 0x7fffffffffffffff || upper64 != 0)
        {
        if (isResultNeg == FALSE || lower64 != 0x8000000000000000 || upper64 != 0)
            {
            mask = ucrDefns[UCR57].bitMask;
            if ((ctx->regUmr & mask) != 0 && IsTrapEnabled(ctx)) // mask set and trap enabled
                {
                cpu180SetUserCondition(ctx, UCR57);
                return FALSE;
                }
            ctx->regUcr |= mask;
            }
        }

    *product = isResultNeg ? ~lower64 + 1 : lower64;

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
**                  doSaveCrs    TRUE if MCR and UCR to be saved in stack frame
**                  sfsa         (out) PVA of stack frame save area
**                  frameSize    (out) number of bytes stored
**                  cond         (out) monitor condition if error condition detected
**
**  Returns:        TRUE if successful.
**
**------------------------------------------------------------------------*/
static bool cpu180PushFrame(Cpu180Context *ctx, u8 at, u8 xs, u8 xt, bool doSaveCrs, u64 *sfsa, u32 *frameSize, MonitorCondition *cond)
    {
    u8  i;
    u64 pva;
    u8  r;
    u32 rmas[33];
    u32 wordAddrs[33];
    u8  words;

    if (at < 2)
        {
        *cond = MCR51; // Instruction specification error
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
    if (cpu180TranslatePvaSequence(ctx, pva, words, 8, RingOf(pva), AccessModeWrite, rmas, cond) == FALSE)
        {
        return FALSE;
        }
    for (i = 0; i < words; i++)
        {
        wordAddrs[i] = rmas[i] >> 3;
        }
    ctx->regA[0]          = pva;
    i                     = 0;
    cpMem[wordAddrs[i++]] = ((u64)ctx->nextKey << 48) | ctx->nextP;
    cpMem[wordAddrs[i++]] = ((u64)ctx->regVmid << 56) | ctx->regA[0];
    cpMem[wordAddrs[i++]] = ((u64)((ctx->regFlags & 0xd000) | ((u16)xs << 8) | ((u16)at << 4) | (u16)xt) << 48) | ctx->regA[1];
    cpMem[wordAddrs[i++]] = ((u64)ctx->regUmr << 48) | ctx->regA[2];
    for (r = 3; r <= at; r++)
        {
        cpMem[wordAddrs[i++]] = ctx->regA[r];
        }
    for (r = xs; r <= xt; r++)
        {
        cpMem[wordAddrs[i++]] = ctx->regX[r];
        }
    if (doSaveCrs)
        {
        cpMem[wordAddrs[5]] |= (u64)ctx->regUcr << 48;
        cpMem[wordAddrs[6]] |= (u64)ctx->regMcr << 48;
        }
    ctx->regX[0] = (ctx->regX[0] & Mask32) | (cpMem[wordAddrs[0]] & LeftMask);
    *frameSize   = words << 3;

#if CcDebug > 0 && defined(TRACE_STORE_START)
    cpu180CheckTraceStore(ctx, *sfsa, *sfsa + (words >> 3) - 1);
#endif

    return TRUE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Put 1 to 8 bytes in memory at a specified PVA
**
**  Parameters:     Name        Description.
**                  ctx         pointer to CPU context
**                  pva         process virtual address at which to put byte
**                  ring        ring for which to validate access
**                  word        the right-justified bytes
**                  count       the number of bytes
**
**  Returns:        TRUE if successful.
**
**------------------------------------------------------------------------*/
static bool cpu180PutBytes(Cpu180Context *ctx, u64 pva, u8 ring, u64 word, u8 count)
    {
    u64              byte;
    u8               byteShift;
    MonitorCondition cond;
    u8               i;
    u64              mask;
    u32              rma;
    u32              rmas[8];
    u32              wordAddr;
    u8               wordShift;

    static u64 masks[8] =
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

#if CcDebug > 0 && defined(TRACE_STORE_START)
    cpu180CheckTraceStore(ctx, pva, pva + 7);
#endif

    if ((pva & Mask3) == 0) // optimization: word-aligned store
        {
        if (cpu180TranslatePvaSequence(ctx, pva, 1, count, ring, AccessModeWrite, rmas, &cond))
            {
            wordAddr = rmas[0] >> 3;
            if (count < 8)
                {
                wordShift = (u8)((8 - count) << 3);
                word      = (word << wordShift) | (cpMem[wordAddr] & masks[count - 1]);
                }
            cpMem[wordAddr] = word;
            }
        else
            {
            cpu180SetMonitorCondition(ctx, cond);
            return FALSE;
            }
        }
    else if (cpu180TranslatePvaSequence(ctx, pva, count, 1, ring, AccessModeWrite, rmas, &cond))
        {
        i         = 0;
        wordShift = (u8)((count - 1) << 3);
        while (count-- > 0)
            {
            rma             = rmas[i++];
            wordAddr        = rma >> 3;
            byte            = (word >> wordShift) & Mask8;
            byteShift       = (u8)(56 - ((rma & Mask3) << 3));
            mask            = ~((u64)0xff << byteShift);
            cpMem[wordAddr] = (cpMem[wordAddr] & mask) | (byte << byteShift);
            wordShift      -= 8;
            }
        }
    else
        {
        cpu180SetMonitorCondition(ctx, cond);
        return FALSE;
        }

    return TRUE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Set 170 state from current 180 state
**
**  Parameters:     Name        Description.
**                  ctx         pointer to 180 CPU context
**                  regP        P register value
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cpu180Set170State(Cpu180Context *ctx, u64 regP)
    {
    Cpu170Context *ctx170;
    u8            i;
    u32           wordAddr;

    ctx170                = &cpus170[ctx->id];
    ctx170->regRaCm       = ctx->regA[3] & Mask21;
    ctx170->exitMode      = (ctx->regA[3] >> 20) & 0xfff000;
    ctx170->regFlCm       = ctx->regA[4] & Mask21;
    ctx170->isMonitorMode = (ctx->regA[4] >> 32) & 1;
    ctx170->regMa         = ctx->regA[5] & Mask21;
    if ((ctx170->exitMode & EmFlagExpandedAddress) != 0)
        {
        ctx170->regRaEcs = ctx->regA[6] & Mask30Ecs;
        ctx170->regFlEcs = ctx->regA[7] & Mask30Ecs;
        }
    else
        {
        ctx170->regRaEcs = ctx->regA[6] & Mask21Ecs;
        ctx170->regFlEcs = ctx->regA[7] & Mask24Ecs;
        }
    for (i = 0; i < 8; i++)
        {
        ctx170->regA[i] = ctx->regA[i + 8] & Mask18;
        }
    ctx170->regB[0] = 0;
    for (i = 1; i < 8; i++)
        {
        ctx170->regB[i] = ctx->regX[i] & Mask18;
        }
    for (i = 0; i < 8; i++)
        {
        ctx170->regX[i] = ctx->regX[i + 8] & Mask60;
        }
    wordAddr          = (regP & Mask32) >> 3;
    ctx170->regP      = wordAddr - ctx170->regRaCm;
    ctx170->opOffset  = 60 - (((regP & Mask3) >> 1) * 15);
    ctx170->opWord    = cpMem[wordAddr];
    ctx170->isStopped = FALSE;
    if ((features & HasInstructionStack) != 0)
        {
        //
        //  Void the instruction stack.
        //
        cpuVoidIwStack(ctx170, (u32)~0);
        }
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
        defn         = &mcrDefns[MCR60];
        ctx->regMcr |= defn->bitMask;
        ctx->regUtp  = pva;

        if ((ctx->regMmr & defn->bitMask) == 0)
            {
            action = defn->whenNoMask;
            }
        else if (IsTrapEnabled(ctx))
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
#if CcDebug > 0
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
#if CcDebug > 0
    u32 xpab = xpa << 3;
#endif

    cpMem[xpa++] = ((u64)ctx->key << 48) | ctx->regP;
    cpMem[xpa++] = ((u64)ctx->regVmid << 56) | ((u64)ctx->regUvmid << 48) | ctx->regA[0];
    cpMem[xpa++] = ((u64)ctx->regFlags << 48) | ctx->regA[1];
    cpMem[xpa++] = ((u64)(ctx->regUmr | 0xfe00) << 48) | ctx->regA[2];
    cpMem[xpa++] = ((u64)ctx->regMmr << 48) | ctx->regA[3];
    cpMem[xpa++] = ((u64)ctx->regUcr << 48) | ctx->regA[4];
    cpMem[xpa++] = ((u64)ctx->regMcr << 48) | ctx->regA[5];
    cpMem[xpa++] = ((u64)ctx->id << 48) | ctx->regA[6];
    cpMem[xpa++] = ((u64)ctx->regKmr << 48) | ctx->regA[7];
    cpMem[xpa++] = ctx->regA[8];
    cpMem[xpa++] = ctx->regA[9];
    cpMem[xpa++] = ((u64)(ctx->regPit & 0xffff0000U) << 32) | ctx->regA[10];
    cpMem[xpa++] = ((u64)(ctx->regPit & 0x0000ffffU) << 48) | ctx->regA[11];
    cpMem[xpa++] = ((u64)(ctx->regBc & 0xffff0000U) << 32) | ctx->regA[12];
    cpMem[xpa++] = ((u64)(ctx->regBc & 0x0000ffffU) << 48) | ctx->regA[13];
    cpMem[xpa++] = ((u64)ctx->regMdf << 48) | ctx->regA[14];
    cpMem[xpa++] = ((u64)ctx->regStl << 48) | ctx->regA[15];
 
    for (i = 0; i < 16; i++)
        {
        cpMem[xpa++] = ctx->regX[i];
        }

    cpMem[xpa++] = ctx->regMdw;
    cpMem[xpa++] = ((u64)(ctx->regSta & 0xffff0000U) << 32) | ctx->regUtp;
    cpMem[xpa++] = ((u64)(ctx->regSta & 0x0000ffffU) << 48) | ctx->regTp;
    cpMem[xpa++] = ((u64)ctx->regDi << 58) | ((u64)ctx->regDm << 48) | ctx->regDlp;
    cpMem[xpa++] = ((u64)ctx->regLrn << 48) | ctx->regTos[1];

    for (i = 2; i < 16; i++)
        {
        cpMem[xpa++] = ctx->regTos[i];
        }
#if CcDebug > 0
    traceExchange180(ctx, xpab, "Store");
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Subtrace two 32-bit integer quantities and detect overflow
**
**  Parameters:     Name        Description.
**                  ctx         pointer to CPU context
**                  minend      the minuend
**                  subend      the subtrahend
**                  diff        (out) pointer to difference
**
**  Returns:        TRUE if arithmetic overflow exception not indicated.
**                  FALSE if overflow detected and user condition register
**                  bit is set.
**
**------------------------------------------------------------------------*/
static bool cpu180SubInt32(Cpu180Context *ctx, u32 minend, u32 subend, u32 *diff)
    {
    u16 mask;

    *diff = minend - subend;
    if ((i32)(minend ^ subend) < 0 && (i32)(*diff ^ minend) < 0)
        {
        mask = ucrDefns[UCR57].bitMask;
        if ((ctx->regUmr & mask) != 0 && IsTrapEnabled(ctx)) // mask set and trap enabled
            {
            cpu180SetUserCondition(ctx, UCR57);
            return FALSE;
            }
        ctx->regUcr |= mask;
        }

    return TRUE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Subtract two 64-bit integer quantities and detect overflow
**
**  Parameters:     Name        Description.
**                  ctx         pointer to CPU context
**                  minend      the minuend
**                  subend      the subtrahend
**                  diff        (out) pointer to difference
**
**  Returns:        TRUE if arithmetic overflow exception not indicated.
**                  FALSE if overflow detected and user condition register
**                  bit is set.
**
**------------------------------------------------------------------------*/
static bool cpu180SubInt64(Cpu180Context *ctx, u64 minend, u64 subend, u64 *diff)
    {
    u16 mask;

    *diff = minend - subend;
    if ((i64)(minend ^ subend) < 0 && (i64)(*diff ^ minend) < 0)
        {
        mask = ucrDefns[UCR57].bitMask;
        if ((ctx->regUmr & mask) != 0 && IsTrapEnabled(ctx)) // mask set and trap enabled
            {
            cpu180SetUserCondition(ctx, UCR57);
            return FALSE;
            }
        ctx->regUcr |= mask;
        }

    return TRUE;
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
static void cpu180UpdatePageSize(Cpu180Context *ctx)
    {
    u8 mask;

    mask              = ctx->regPsm;
    ctx->pageNumShift = 9;
    while ((mask & 1) == 0 && ctx->pageNumShift < 16)
        {
        ctx->pageNumShift += 1;
        mask             >>= 1;
        }
    ctx->pageLengthMask = ((u32)ctx->regPtl << 12) | 0xfffU;
    ctx->spidShift      = ctx->pageNumShift - 9;

#if CcDebug > 0
    traceVmRegisters(ctx);
#endif
#if DEBUG
    fprintf(cpu180Log, "Update page size: PSM %02x PTL %02x pageLengthMask " FMT32_08x " pageNumShift %d spidShift %d\n",
        ctx->regPsm, ctx->regPtl, ctx->pageLengthMask, ctx->pageNumShift, ctx->spidShift);
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Validate an access mode for a PVA
**
**  Parameters:     Name        Description.
**                  ctx         pointer to C180 CPU context
**                  pva         PVA for which to validate access
**                  ring        ring number for which to validate access
**                              (e.g., caller's ring number)
**                  access      mode of access (execute, read, write)
**                  cond        (out) monitor condition, if invalid segment
**                              or access violation indicated
**
**  Returns:        TRUE if access allowed
**
**------------------------------------------------------------------------*/
static bool cpu180ValidateAccess(Cpu180Context *ctx, u64 pva, u8 ring, Cpu180AccessMode access, MonitorCondition *cond)
    {
    u8  lock;
    u8  pm;
    u64 sde;
    u16 segNum;

    segNum = SegmentOf(pva);
    if (segNum > ctx->regStl)
        {
        *cond = MCR60; // Invalid segment
        ctx->regUtp = pva;
        return FALSE;
        }

    sde  = cpMem[(ctx->regSta >> 3) + segNum];
    if ((sde >> 63) == 0)
        {
        *cond = MCR60; // Invalid segment
        ctx->regUtp = pva;
        return FALSE;
        }
    lock = (sde >> 24) & Mask6;

    /*
    **  Validate access
    */
    if ((access & AccessModeExecute) != 0)
        {
        if (((sde >> 60) & Mask2) == 0       // non-executable segment
            || ring < ((sde >> 52) & Mask4)  // ring < R1
            || ring > ((sde >> 48) & Mask4)  // ring > R2
            || (ctx->key != lock && ctx->key != 0 && lock != 0))
            {
            *cond = MCR54; // Access violation
            ctx->regUtp = pva;
            return FALSE;
            }
        }
    if ((access & AccessModeRead) != 0)
        {
        if (ring > ((sde >> 48) & Mask4))
            {
            *cond = MCR54; // Access violation
            ctx->regUtp = pva;
            return FALSE;
            }
        pm = (sde >> 58) & Mask2;
        switch (pm)
            {
        default:
        case 0: // non-readable segment
            *cond = MCR54; // Access violation
            ctx->regUtp = pva;
            return FALSE;
        case 1: // read controlled by key/lock
            if (ctx->key != lock && ctx->key != 0 && lock != 0)
                {
                *cond = MCR54; // Access violation
                ctx->regUtp = pva;
                return FALSE;
                }
            break;
        case 2: // read not controlled by key/lock
        case 3: // binding section segment
            break;
            }
        }
    if ((access & AccessModeWrite) != 0)
        {
        if (ring > ((sde >> 52) & Mask4))
            {
            *cond = MCR54; // Access violation
            ctx->regUtp = pva;
            return FALSE;
            }
        pm = (sde >> 56) & Mask2;
        switch (pm)
            {
        default:
        case 0: // non-writable segment
        case 3: // reserved
            *cond = MCR54; // Access violation
            ctx->regUtp = pva;
            return FALSE;
        case 1: // write controlled by key/lock
            if (ctx->key != lock && ctx->key != 0 && lock != 0)
                {
                *cond = MCR54; // Access violation
                ctx->regUtp = pva;
                return FALSE;
                }
            break;
        case 2: // write not controlled by key/lock
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
    cpu180SetMonitorCondition(activeCpu, MCR51); // Instruction specification error
    }

static void cp180Op01(Cpu180Context *activeCpu)  // 01  SYNC       MIGDS 2-138
    {
    // do nothing
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
    u64 Xk;

    if (cpu180GetCurrentXp(activeCpu) < 3) // Global privileged mode required
        {
        cpu180SetUserCondition(activeCpu, UCR48); // privileged instruction fault
        return;
        }
    //
    // This instruction is capable of sending interrupts to external memories, depending
    // upon the processor class. For example, P3 (e.g., Cyber 860) can send interrupts
    // to external memories when bit 33 of Xk is set. This is not currently supported.
    //
    // Ordinarily, on processors capable of being connected to more than one memory (e.g., P3),
    // bit 63 of Xk is associated with local memory port 0, and bit 61 is associated with
    // local memory port 2. Local memory port 0 is associated with CPU0, and local memory port 2
    // is associated with CPU 1.
    //
    Xk = activeCpu->regX[activeCpu->opK];
    if ((Xk & 1) != 0)
        {
        cpus180[0].regMcr |= mcrDefns[MCR56].bitMask; // External interrupt
        }
    if ((Xk & 4) != 0 && cpuCount > 1)
        {
        cpus180[1].regMcr |= mcrDefns[MCR56].bitMask; // External interrupt
        }
    }

static void cp180Op04(Cpu180Context *activeCpu)  // 04  RETURN     MIGDS 2-127
    {
    u8               at;
    MonitorCondition cond;
    u16              desc;
    u8               i;
    u64              psap;
    u8               r1;
    u8               ringA2;
    u8               ringNewP;
    u8               ringP;
    u8               r;
    u32              rmas[33];
    u8               vmid;
    u64              word;
    u32              wordAddrs[33];
    u8               words;
    u8               xs;
    u8               xt;

#if CcDebug > 0
    if (traceValidateStack(activeCpu, activeCpu->regA[2], 2, "RETURN") == FALSE)
        {
        traceMask                    |= TRACECPU(activeCpu, TraceCpu180 | TraceCallFrame);
        traceInstCount[activeCpu->id] = TRACE_INST_COUNT;
        }
#endif

    if ((activeCpu->regFlags & 0x8000) != 0)
        {
        cpu180SetUserCondition(activeCpu, UCR53); // critical frame flag
        return;
        }
    psap = activeCpu->regA[2];
    if ((psap & Mask3) != 0)
        {
        cpu180SetMonitorCondition(activeCpu, MCR52); // address specification error
        activeCpu->regUtp = psap;
        return;
        }
    if (cpu180TranslatePvaSequence(activeCpu, psap, 4, 8, RingOf(psap), AccessModeRead, rmas, &cond) == FALSE)
        {
        cpu180SetMonitorCondition(activeCpu, cond);
        return;
        }
    for (i = 0; i < 4; i++)
        {
        wordAddrs[i] = rmas[i] >> 3;
        }
    if ((cpMem[wordAddrs[0]] & RingMask) < (activeCpu->regA[2] & RingMask))
        {
        cpu180SetMonitorCondition(activeCpu, MCR61); // inward return
        return;
        }
    vmid = (cpMem[wordAddrs[1]] >> 56) & Mask4;
    desc = cpMem[wordAddrs[2]] >> 48;
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
    if (cpu180TranslatePvaSequence(activeCpu, psap + 32, words - 4, 8, RingOf(psap), AccessModeRead, &rmas[4], &cond) == FALSE)
        {
        cpu180SetMonitorCondition(activeCpu, cond);
        return;
        }
    for (i = 4; i < words; i++)
        {
        wordAddrs[i] = rmas[i] >> 3;
        }
    ringP  = RingOf(activeCpu->regP);
    ringA2 = RingOf(activeCpu->regA[2]);
    if (cpu180GetR1(activeCpu, activeCpu->regA[2], &r1, &cond) == FALSE)
        {
        cpu180SetMonitorCondition(activeCpu, cond);
        return;
        }
    if (r1 > ringA2)
        {
        ringA2 = r1;
        }
    i                  = 0;
    word               = cpMem[wordAddrs[i++]];
    activeCpu->nextKey = (u8)(word >> 48);
    activeCpu->nextP   = word & Mask48;
    ringNewP           = RingOf(activeCpu->nextP);
    word               = cpMem[wordAddrs[i++]];
    activeCpu->regVmid = (word >> 56) & Mask4;
    activeCpu->regA[0] = word & Mask48;
    activeCpu->regA[1] = cpMem[wordAddrs[i++]] & Mask48;
    word               = cpMem[wordAddrs[i++]];
    activeCpu->regUmr  = (word >> 48) | 0xfe00;
    activeCpu->regA[2] = word & Mask48;
    for (r = 3; r <= at; r++)
        {
        activeCpu->regA[r] = cpMem[wordAddrs[i++]] & Mask48;
        }
    for (r = 0; r <= at; r++)
        {
        if (RingOf(activeCpu->regA[r]) < ringA2)
            {
            activeCpu->regA[r] = ((u64)ringA2 << 44) | (activeCpu->regA[r] & Mask44);
            }
        }
    if (ringP != ringNewP)
        {
        for (r = at + 1; r <= 0xf; r++)
            {
            if (RingOf(activeCpu->regA[r]) < ringNewP)
                {
                activeCpu->regA[r] = ((u64)ringNewP << 44) | (activeCpu->regA[r] & Mask44);
                }
            }
        }
    for (r = xs; r <= xt; r++)
        {
        activeCpu->regX[r] = cpMem[wordAddrs[i++]];
        }
    activeCpu->regFlags        &= 0x3ffe;        // clear CFF, OCF, trap enable, and delay flip-flop
    activeCpu->regFlags        |= desc & 0xc000; // set CFF and OCF per descriptor
    activeCpu->regTos[ringNewP] = activeCpu->regA[1];
    if (ringNewP > activeCpu->regLrn)
        {
        activeCpu->regLrn = ringNewP;
        }

#if CcDebug > 0
    traceCallFrame(activeCpu, psap, "popped");
#endif

    if (vmid == 1)
        {
        cpu180Set170State(activeCpu, activeCpu->nextP);
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
    u8               i;
    u64              psap;
    u64              regA;
    u8               r1;
    u8               ring;
    u8               ringA2;
    u32              rmas[4];
    u32              wordAddrs[4];

#if CcDebug > 0
    if (traceValidateStack(activeCpu, activeCpu->regA[2], 2, "POP") == FALSE)
        {
        traceMask                    |= TRACECPU(activeCpu, TraceCpu180 | TraceCallFrame);
        traceInstCount[activeCpu->id] = TRACE_INST_COUNT;
        }
#endif

    if ((activeCpu->regFlags & 0x8000) != 0)
        {
        cpu180SetUserCondition(activeCpu, UCR53); // critical frame flag
        return;
        }
    psap = activeCpu->regA[2];
    if ((psap & Mask3) != 0)
        {
        cpu180SetMonitorCondition(activeCpu, MCR52); // address specification error
        activeCpu->regUtp = psap;
        return;
        }
    ringA2 = RingOf(psap);
    if (ringA2 != RingOf(activeCpu->regP))
        {
        cpu180SetUserCondition(activeCpu, UCR52); // inter-ring pop
        return;
        }
    if (cpu180TranslatePvaSequence(activeCpu, psap, 4, 8, RingOf(psap), AccessModeRead, rmas, &cond) == FALSE)
        {
        cpu180SetMonitorCondition(activeCpu, cond);
        return;
        }
    for (i = 0; i < 4; i++)
        {
        wordAddrs[i] = rmas[i] >> 3;
        }

/*  MIGDS says this test is optional
    if (psap != (cpMem[wordAddrs[1]] & Mask48))
        {
        cpu180SetMonitorCondition(activeCpu, MCR55); // environment specification error
        return;
        }
*/
    // Load A1
    regA = cpMem[wordAddrs[2]] & Mask48; // A1
    ring = RingOf(regA);
    if (cpu180GetR1(activeCpu, psap, &r1, &cond) == FALSE)
        {
        cpu180SetMonitorCondition(activeCpu, cond);
        return;
        }
    if (ring < ringA2)
        {
        ring = ringA2;
        }
    if (ring < r1)
        {
        ring = r1;
        }
    activeCpu->regA[1] = ((u64)ring << 44) | (regA & Mask44);
    // Load A2
    regA = cpMem[wordAddrs[3]] & Mask48; // A2
    ring = RingOf(regA);
    if (ring < ringA2)
        {
        ring = ringA2;
        }
    if (ring < r1)
        {
        ring = r1;
        }
    activeCpu->regA[2]      = ((u64)ring << 44) | (regA & Mask44);
    activeCpu->regFlags     = (activeCpu->regFlags & 0x3fff) | ((cpMem[wordAddrs[2]] >> 48) & 0xc000);
    ring                    = RingOf(activeCpu->regP);
    activeCpu->regTos[ring] = activeCpu->regA[1];
    if (ring > activeCpu->regLrn)
        {
        activeCpu->regLrn = ring;
        }

#if CcDebug > 0
    traceCallFrame(activeCpu, psap, "popped");
#endif
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

    regId = (u8)(activeCpu->regX[activeCpu->opJ] & Mask8);
    if (regId < 0x10 || (regId >= 0x20 && regId <= 0x3f))
        {
        activeCpu->regX[activeCpu->opK] = 0;
        }
    else
        {
        activeCpu->regX[activeCpu->opK] = cpu180MacGetCpStateRegister(activeCpu, regId);
        }
    }

static void cp180Op0F(Cpu180Context *activeCpu)  // 0F  CPYXS      MIGDS 2-146
    {
    u8 regId;

    regId = (u8)(activeCpu->regX[activeCpu->opJ] & Mask8);
    if (regId < 0x60) // no access
        {
        return;
        }
    else if (regId < 0x80U) // monitor mode required
        {
        if (activeCpu->isMonitorMode == FALSE)
            {
            cpu180SetMonitorCondition(activeCpu, MCR51); // Instruction specification error
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
    cpu180MacSetCpStateRegister(activeCpu, regId, activeCpu->regX[activeCpu->opK]);
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
    u64 diff;

    if (cpu180SubInt64(activeCpu, activeCpu->regX[activeCpu->opK], activeCpu->opJ, &diff))
        {
        activeCpu->regX[activeCpu->opK] = diff;
        }
    }

static void cp180Op14(Cpu180Context *activeCpu)  // 14  LBSET      MIGDS 2-136
    {
    MonitorCondition cond;
    u64              mask;
    u32              offset;
    u32              pti;
    u64              pva;
    u32              rma;
    u8               shift;
    u64              word;
    u32              wordAddr;

    offset = (u32)(activeCpu->regX[0] & Mask32) >> 3;
    if (offset >= 0x10000000)
        {
        offset |= 0xf0000000;
        }
    pva = (activeCpu->regA[activeCpu->opJ] & RingSegMask) | ((activeCpu->regA[activeCpu->opJ] + offset) & Mask32);
    if (cpu180ValidateAccess(activeCpu, pva, RingOf(pva), AccessModeRead | AccessModeWrite, &cond) == FALSE
        || cpu180PvaToRma(activeCpu, pva, AccessModeNone, &rma, &pti, &cond) == FALSE) // cause page fault if page not in memory
        {
        cpu180SetMonitorCondition(activeCpu, cond);
        }
    else
        {
        wordAddr                        = rma >> 3;
        shift                           = (u8)(56 - ((rma & Mask3) << 3)) + (u8)(7 - (activeCpu->regX[0] & Mask3));
        mask                            = (u64)1 << shift;
        cpuAcquireMemoryMutex();
        word                            = cpMem[wordAddr];
        activeCpu->regX[activeCpu->opK] = (word >> shift) & 1;
        cpMem[wordAddr]                 = word | mask;
        cpMem[pti]                     |= (u64)3 << 60; // set page used and modified bits
        cpuReleaseMemoryMutex();

#if CcDebug > 0 && defined(TRACE_STORE_START)
        cpu180CheckTraceStore(activeCpu, pva, pva + 7);
#endif
        }
    }

static void cp180Op16(Cpu180Context *activeCpu)  // 16  TPAGE      MIGDS 2-137
    {
    MonitorCondition cond;
    u32              pti;
    u32              rma;
    u64              utp;

    utp = activeCpu->regUtp;
    if (cpu180PvaToRma(activeCpu, activeCpu->regA[activeCpu->opJ], AccessModeRead, &rma, &pti, &cond))
        {
        activeCpu->regX[activeCpu->opK] = (activeCpu->regX[activeCpu->opK] & LeftMask) | rma;
        }
    else if (cond == MCR57 || cond == MCR54) // page table search w/o find or access violation
        {
        activeCpu->regX[activeCpu->opK] = (activeCpu->regX[activeCpu->opK] & LeftMask) | ((u64)1 << 31);
        activeCpu->regUtp               = utp;
        }
    else
        {
        cpu180SetMonitorCondition(activeCpu, cond);
        }
    }

static void cp180Op17(Cpu180Context *activeCpu)  // 17  LPAGE      MIGDS 2-139
    {
    u16 asid;
    u32 byteNum;
    u8  count;
    u32 pti;

    if (cpu180GetCurrentXp(activeCpu) < 2)
        {
        cpu180SetUserCondition(activeCpu, UCR48);
        return;
        }

    asid    = (u16)((activeCpu->regX[activeCpu->opJ] >> 32) & Mask16);
    byteNum = (u32)(activeCpu->regX[activeCpu->opJ] & Mask32);
    if (Is32BitNeg(byteNum))
        {
        cpu180SetMonitorCondition(activeCpu, MCR52);
        activeCpu->regUtp = ((u64)asid << 32) | byteNum;
        return;
        }
    if (cpu180FindPte(activeCpu, asid, byteNum, TRUE, &pti, &count))
        {
        activeCpu->regX[activeCpu->opK] = (activeCpu->regX[activeCpu->opK] & LeftMask) | ((((u64)pti << 3) - activeCpu->regPta) & Mask32);
        activeCpu->regX[1] = (activeCpu->regX[1] & LeftMask) | ((u64)1 << 31) | (u64)count;
        }
    else
        {
        activeCpu->regX[activeCpu->opK] = (activeCpu->regX[activeCpu->opK] & LeftMask) | ((((u64)pti << 3) - activeCpu->regPta) & Mask32);
        activeCpu->regX[1] = (activeCpu->regX[1] & LeftMask) | (u64)count;
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
        /* 0 */ { 0, 0, 0, 0 },
        /* 1 */ { 0, 0, 0, 1 },
        /* 2 */ { 0, 0, 1, 0 },
        /* 3 */ { 0, 0, 1, 1 },
        /* 4 */ { 0, 1, 0, 0 },
        /* 5 */ { 0, 1, 0, 1 },
        /* 6 */ { 0, 1, 1, 0 },
        /* 7 */ { 0, 1, 1, 1 },
        /* 8 */ { 1, 0, 0, 0 },
        /* 9 */ { 1, 0, 0, 1 },
        /* A */ { 1, 0, 1, 0 },
        /* B */ { 1, 0, 1, 1 },
        /* C */ { 1, 1, 0, 0 },
        /* D */ { 1, 1, 0, 1 },
        /* E */ { 1, 1, 1, 0 },
        /* F */ { 1, 1, 1, 1 }
        };

    activeCpu->regX[activeCpu->opK] = (u64)table[activeCpu->opJ][(activeCpu->regX[1] >> 30) & Mask2] << 63;
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
        if (Is32BitNeg(activeCpu->regX[activeCpu->opK]))
            {
            activeCpu->regX[activeCpu->opK] |= 0xffffffff00000000;
            }
        else
            {
            activeCpu->regX[activeCpu->opK] &= Mask32;
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
    u32 diff;

    if (cpu180SubInt32(activeCpu, activeCpu->regX[activeCpu->opK] & Mask32, activeCpu->regX[activeCpu->opJ] & Mask32, &diff))
        {
        activeCpu->regX[activeCpu->opK] = (activeCpu->regX[activeCpu->opK] & LeftMask) | diff;
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
    i32 XjR;
    i32 XkR;

    XjR = (i32)(activeCpu->regX[activeCpu->opJ] & Mask32);
    XkR = (i32)(activeCpu->regX[activeCpu->opK] & Mask32);
    if (XjR == 0)
        {
        cpu180SetUserCondition(activeCpu, UCR55);
        if ((activeCpu->regUmr & ucrDefns[UCR55].bitMask) != 0 && IsTrapEnabled(activeCpu)) // mask set and trap enabled
            {
            activeCpu->nextP = activeCpu->regP; // inhibit execution
            }
        }
    else if (XjR == -1 && (u32)XkR == 0x80000000U)
        {
        cpu180SetUserCondition(activeCpu, UCR57);
        if ((activeCpu->regUmr & ucrDefns[UCR57].bitMask) != 0 && IsTrapEnabled(activeCpu)) // mask set and trap enabled
            {
            activeCpu->nextP = activeCpu->regP; // inhibit execution
            }
        }
    else
        {
        activeCpu->regX[activeCpu->opK] = (activeCpu->regX[activeCpu->opK] & LeftMask) | ((u64)(XkR / XjR) & Mask32);
        }
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
    u64 diff;

    if (cpu180SubInt64(activeCpu, activeCpu->regX[activeCpu->opK], activeCpu->regX[activeCpu->opJ], &diff))
        {
        activeCpu->regX[activeCpu->opK] = diff;
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
        if ((activeCpu->regUmr & ucrDefns[UCR55].bitMask) != 0 && IsTrapEnabled(activeCpu)) // mask set and trap enabled
            {
            activeCpu->nextP = activeCpu->regP; // inhibit execution
            }
        }
    else if (Xj == -1 && activeCpu->regX[activeCpu->opK] == 0x8000000000000000)
        {
        cpu180SetUserCondition(activeCpu, UCR57);
        if ((activeCpu->regUmr & ucrDefns[UCR57].bitMask) != 0 && IsTrapEnabled(activeCpu)) // mask set and trap enabled
            {
            activeCpu->nextP = activeCpu->regP; // inhibit execution
            }
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
    u32 diff;

    if (cpu180SubInt32(activeCpu, activeCpu->regX[activeCpu->opK] & Mask32, activeCpu->opJ, &diff))
        {
        activeCpu->regX[activeCpu->opK] = (activeCpu->regX[activeCpu->opK] & LeftMask) | diff;
        }
    }

static void cp180Op2A(Cpu180Context *activeCpu)  // 2A  ADDAX      MIGDS 2-29
    {
    activeCpu->regA[activeCpu->opK] = (activeCpu->regA[activeCpu->opK] & RingSegMask)
        | ((activeCpu->regA[activeCpu->opK] + activeCpu->regX[activeCpu->opJ]) & Mask32);
    }

static void cp180Op2C(Cpu180Context *activeCpu)  // 2C  CMPR       MIGDS 2-24
    {
    i32 XjR;
    i32 XkR;

    XjR = (i32)((activeCpu->opJ == 0) ? 0 : (activeCpu->regX[activeCpu->opJ] & Mask32));
    XkR = (i32)((activeCpu->opK == 0) ? 0 : (activeCpu->regX[activeCpu->opK] & Mask32));
    if (XjR == XkR)
        {
        activeCpu->regX[1] &= LeftMask;
        }
    else if (XjR > XkR)
        {
        activeCpu->regX[1] = (activeCpu->regX[1] & LeftMask) | 0x40000000U;
        }
    else
        {
        activeCpu->regX[1] = (activeCpu->regX[1] & LeftMask) | 0xc0000000U;
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
        activeCpu->regX[1] = (activeCpu->regX[1] & LeftMask) | 0x40000000U;
        }
    else
        {
        activeCpu->regX[1] = (activeCpu->regX[1] & LeftMask) | 0xc0000000U;
        }
    }

static void cp180Op2E(Cpu180Context *activeCpu)  // 2E  BRREL      MIGDS 2-27
    {
    activeCpu->nextP = (activeCpu->regP & RingSegMask) | (((activeCpu->regX[activeCpu->opK] << 1) + activeCpu->regP) & Mask32);
    }

static void cp180Op2F(Cpu180Context *activeCpu)  // 2F  BRDIR      MIGDS 2-27
    {
    u64              Aj;
    MonitorCondition cond;
    u8               lock;
    u64              nextP;
    u64              XkR;

    Aj = activeCpu->regA[activeCpu->opJ];
    if ((Aj & 1) != 0)
        {
        cpu180SetMonitorCondition(activeCpu, MCR52); // Address specification error
        activeCpu->regUtp = Aj;
        return;
        }
    XkR   = (activeCpu->opK == 0) ? 0 : activeCpu->regX[activeCpu->opK] & Mask32;
    nextP = (activeCpu->regP & RingMask) | (Aj & SegMask) | ((Aj + (XkR << 1)) & Mask32);
    if (cpu180GetLock(activeCpu, nextP, &lock, &cond))
        {
        activeCpu->nextP   = nextP;
        activeCpu->nextKey = lock;
        }
    else
        {
        cpu180SetMonitorCondition(activeCpu, cond);
        }
    }

static void cp180Op30(Cpu180Context *activeCpu)  // 30  ADDF       MIGDS 2-73
    {
    u64 result;

    if (float180AddFloat(activeCpu, activeCpu->regX[activeCpu->opK], activeCpu->regX[activeCpu->opJ], &result))
        {
        activeCpu->regX[activeCpu->opK] = result;
        activeCpu->nextP                = activeCpu->regP + 2;
        }
    else
        {
        activeCpu->nextP = activeCpu->regP;
        }
    }

static void cp180Op31(Cpu180Context *activeCpu)  // 31  SUBF       MIGDS 2-73
    {
    u64 result;

    if (float180SubFloat(activeCpu, activeCpu->regX[activeCpu->opK], activeCpu->regX[activeCpu->opJ], &result))
        {
        activeCpu->regX[activeCpu->opK] = result;
        activeCpu->nextP                = activeCpu->regP + 2;
        }
    else
        {
        activeCpu->nextP = activeCpu->regP;
        }
    }

static void cp180Op32(Cpu180Context *activeCpu)  // 32  MULF       MIGDS 2-76
    {
    u64 result;

    if (float180MulFloat(activeCpu, activeCpu->regX[activeCpu->opK], activeCpu->regX[activeCpu->opJ], &result))
        {
        activeCpu->regX[activeCpu->opK] = result;
        activeCpu->nextP                = activeCpu->regP + 2;
        }
    else
        {
        activeCpu->nextP = activeCpu->regP;
        }
    }

static void cp180Op33(Cpu180Context *activeCpu)  // 33  DIVF       MIGDS 2-77
    {
    u64 result;

    if (float180DivFloat(activeCpu, activeCpu->regX[activeCpu->opK], activeCpu->regX[activeCpu->opJ], &result))
        {
        activeCpu->regX[activeCpu->opK] = result;
        activeCpu->nextP                = activeCpu->regP + 2;
        }
    else
        {
        activeCpu->nextP = activeCpu->regP;
        }
    }

static void cp180Op34(Cpu180Context *activeCpu)  // 34  ADDD       MIGDS 2-79
    {
    Cpu180Double addend;
    Cpu180Double augend;
    Cpu180Double result;

    augend.leftPart  = activeCpu->regX[activeCpu->opK];
    augend.rightPart = activeCpu->regX[(activeCpu->opK + 1) & Mask4];
    addend.leftPart  = activeCpu->regX[activeCpu->opJ];
    addend.rightPart = activeCpu->regX[(activeCpu->opJ + 1) & Mask4];
    if (float180AddDouble(activeCpu, &augend, &addend, &result))
        {
        activeCpu->regX[activeCpu->opK]               = result.leftPart;
        activeCpu->regX[(activeCpu->opK + 1) & Mask4] = result.rightPart;
        activeCpu->nextP                              = activeCpu->regP + 2;
        }
    else
        {
        activeCpu->nextP = activeCpu->regP;
        }
    }

static void cp180Op35(Cpu180Context *activeCpu)  // 35  SUBD       MIGDS 2-79
    {
    Cpu180Double minend;
    Cpu180Double result;
    Cpu180Double subend;

    minend.leftPart  = activeCpu->regX[activeCpu->opK];
    minend.rightPart = activeCpu->regX[(activeCpu->opK + 1) & Mask4];
    subend.leftPart  = activeCpu->regX[activeCpu->opJ];
    subend.rightPart = activeCpu->regX[(activeCpu->opJ + 1) & Mask4];
    if (float180SubDouble(activeCpu, &minend, &subend, &result))
        {
        activeCpu->regX[activeCpu->opK]               = result.leftPart;
        activeCpu->regX[(activeCpu->opK + 1) & Mask4] = result.rightPart;
        activeCpu->nextP                              = activeCpu->regP + 2;
        }
    else
        {
        activeCpu->nextP = activeCpu->regP;
        }
    }

static void cp180Op36(Cpu180Context *activeCpu)  // 36  MULD       MIGDS 2-82
    {
    Cpu180Double mltand;
    Cpu180Double mltier;
    Cpu180Double result;

    mltand.leftPart  = activeCpu->regX[activeCpu->opK];
    mltand.rightPart = activeCpu->regX[(activeCpu->opK + 1) & Mask4];
    mltier.leftPart  = activeCpu->regX[activeCpu->opJ];
    mltier.rightPart = activeCpu->regX[(activeCpu->opJ + 1) & Mask4];
    if (float180MulDouble(activeCpu, &mltand, &mltier, &result))
        {
        activeCpu->regX[activeCpu->opK]               = result.leftPart;
        activeCpu->regX[(activeCpu->opK + 1) & Mask4] = result.rightPart;
        activeCpu->nextP                              = activeCpu->regP + 2;
        }
    else
        {
        activeCpu->nextP = activeCpu->regP;
        }
    }

static void cp180Op37(Cpu180Context *activeCpu)  // 37  DIVD       MIGDS 2-84
    {
    Cpu180Double dvdend;
    Cpu180Double dvisor;
    Cpu180Double result;

    dvdend.leftPart  = activeCpu->regX[activeCpu->opK];
    dvdend.rightPart = activeCpu->regX[(activeCpu->opK + 1) & Mask4];
    dvisor.leftPart  = activeCpu->regX[activeCpu->opJ];
    dvisor.rightPart = activeCpu->regX[(activeCpu->opJ + 1) & Mask4];
    if (float180DivDouble(activeCpu, &dvdend, &dvisor, &result))
        {
        activeCpu->regX[activeCpu->opK]               = result.leftPart;
        activeCpu->regX[(activeCpu->opK + 1) & Mask4] = result.rightPart;
        activeCpu->nextP                              = activeCpu->regP + 2;
        }
    else
        {
        activeCpu->nextP = activeCpu->regP;
        }
    }

static void cp180Op39(Cpu180Context *activeCpu)  // 39  ENTX       MIGDS 2-31
    {
    activeCpu->regX[1] = ((u64)activeCpu->opJ << 4) | (u64)activeCpu->opK;
    }

static void cp180Op3A(Cpu180Context *activeCpu)  // 3A  CNIF       MIGDS 2-71
    {
    activeCpu->regX[activeCpu->opK] = float180ConvertIntToFloat(activeCpu->regX[activeCpu->opJ]);
    }

static void cp180Op3B(Cpu180Context *activeCpu)  // 3B  CNFI       MIGDS 2-72
    {
    u64 intResult;

    if (float180ConvertFloatToInt(activeCpu, activeCpu->regX[activeCpu->opJ], &intResult))
        {
        activeCpu->regX[activeCpu->opK] = intResult;
        }
    }

static void cp180Op3C(Cpu180Context *activeCpu)  // 3C  CMPF       MIGDS 2-89
    {
    u16 mask;
    u64 minend;
    u64 subend;
    u16 ucr;
    int valence;

    minend = (activeCpu->opJ == 0) ? 0 : activeCpu->regX[activeCpu->opJ];
    subend = (activeCpu->opK == 0) ? 0 : activeCpu->regX[activeCpu->opK];
    ucr    = activeCpu->regUcr;
    if (float180CompareFloat(activeCpu, minend, subend, &valence) == FALSE)
        {
        mask = ucrDefns[UCR61].bitMask; // floating point indefinite
        if (((activeCpu->regUcr ^ ucr) & mask) == mask)
            {
            if ((activeCpu->regUmr & mask) == 0 || !IsTrapEnabled(activeCpu))
                {
                activeCpu->regX[1] = (activeCpu->regX[1] & LeftMask) | 0x80000000U;
                return;
                }
            activeCpu->nextP = activeCpu->regP;
            }
        }
    else
        {
        if (valence == 0)
            {
            activeCpu->regX[1] &= LeftMask;
            }
        else if (valence > 0)
            {
            activeCpu->regX[1] = (activeCpu->regX[1] & LeftMask) | 0x40000000U;
            }
        else
            {
            activeCpu->regX[1] = (activeCpu->regX[1] & LeftMask) | 0xc0000000U;
            }
        }
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
    activeCpu->regX[0] = ((u64)activeCpu->opJ << 4) | (u64)activeCpu->opK;
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
    activeCpu->nextP += 8;
    cpu180ApplyBdpOperator(activeCpu, bdp180Add);
    }

static void cp180Op71(Cpu180Context *activeCpu)  // 71  SUBN       MIGDS 2-47
    {
    activeCpu->nextP += 8;
    cpu180ApplyBdpOperator(activeCpu, bdp180Sub);
    }

static void cp180Op72(Cpu180Context *activeCpu)  // 72  MULN       MIGDS 2-47
    {
    activeCpu->nextP += 8;
    cpu180ApplyBdpOperator(activeCpu, bdp180Mul);
    }

static void cp180Op73(Cpu180Context *activeCpu)  // 73  DIVN       MIGDS 2-47
    {
    activeCpu->nextP += 8;
    cpu180ApplyBdpOperator(activeCpu, bdp180Div);
    }

static void cp180Op74(Cpu180Context *activeCpu)  // 74  CMPN       MIGDS 2-52
    {
    u64        descPva;
    BdpOperand dstOperand;
    u8         result;
    BdpOperand srcOperand;

    descPva           = activeCpu->nextP;
    activeCpu->nextP += 8;
    if (cpu180GetBdpDescriptor(activeCpu, descPva, activeCpu->opJ, 0, &activeCpu->srcDesc)
        && cpu180GetBdpDescriptor(activeCpu, descPva + 4, activeCpu->opK, 1, &activeCpu->dstDesc))
        {
        if (activeCpu->srcDesc.type > 6 || activeCpu->dstDesc.type > 6)
            {
            cpu180SetMonitorCondition(activeCpu, MCR51); // Instruction specification error
            return;
            }
        if (bdp180DecodeOperand(activeCpu, &activeCpu->dstDesc, &dstOperand)
            && bdp180DecodeOperand(activeCpu, &activeCpu->srcDesc, &srcOperand))
            {
            if (srcOperand.sign == dstOperand.sign)
                {
                result = 0;
                if (srcOperand.value[3] != dstOperand.value[3] || srcOperand.value[2] != dstOperand.value[2])
                    {
                    if (srcOperand.sign) // both operands are negative
                        {
                        if (srcOperand.value[2] > dstOperand.value[2])
                            {
                            result = 3;
                            }
                        else if (srcOperand.value[2] == dstOperand.value[2])
                            {
                            if (srcOperand.value[3] > dstOperand.value[3])
                                {
                                result = 3;
                                }
                            else
                                {
                                result = 1;
                                }
                            }
                        }
                    else if (srcOperand.value[2] > dstOperand.value[2])
                        {
                        result = 1;
                        }
                    else if (srcOperand.value[2] == dstOperand.value[2])
                        {
                        if (srcOperand.value[3] > dstOperand.value[3])
                            {
                            result = 1;
                            }
                        else
                            {
                            result = 3;
                            }
                        }
                    else // source < destination
                        {
                        result = 3;
                        }
                    }
                }
            else if (srcOperand.sign) // source is negative, destination is positive
                {
                result = 3;
                }
            else // source is positive, destination is negative
                {
                result = 1;
                }
            activeCpu->regX[1] = (activeCpu->regX[1] & LeftMask) | ((u64)result << 30);

#if CcDebug > 0
            traceMemoryBlock(activeCpu, activeCpu->srcDesc.pva, activeCpu->srcDesc.length, "    source block:");
            traceMemoryBlock(activeCpu, activeCpu->dstDesc.pva, activeCpu->dstDesc.length, "    destination block:");
#endif
            }
        }
    }

static void cp180Op75(Cpu180Context *activeCpu)  // 75  MOVN       MIGDS 2-51
    {
    u64        descPva;
    bool       isTruncated;
    BdpOperand operand;

    descPva           = activeCpu->nextP;
    activeCpu->nextP += 8;
    if (cpu180GetBdpDescriptor(activeCpu, descPva, activeCpu->opJ, 0, &activeCpu->srcDesc)
        && cpu180GetBdpDescriptor(activeCpu, descPva + 4, activeCpu->opK, 1, &activeCpu->dstDesc))
        {
        if (bdp180DecodeOperand(activeCpu, &activeCpu->srcDesc, &operand)
            && bdp180EncodeOperand(activeCpu, &activeCpu->dstDesc, &operand, FALSE, &isTruncated))
            {
#if CcDebug > 0
#if defined(TRACE_STORE_START)
            cpu180CheckTraceStore(activeCpu, activeCpu->dstDesc.pva, activeCpu->dstDesc.pva + activeCpu->dstDesc.length - 1);
#endif
            traceMemoryBlock(activeCpu, activeCpu->srcDesc.pva, activeCpu->srcDesc.length, "    source block:");
            traceMemoryBlock(activeCpu, activeCpu->dstDesc.pva, activeCpu->dstDesc.length, "    destination block:");
#endif
            if (isTruncated)
                {
                cpu180SetUserCondition(activeCpu, UCR62); // Arithmetic loss of significance
                return;
                }
            }
        }
    }

static void cp180Op76(Cpu180Context *activeCpu)  // 76  MOVB       MIGDS 2-55
    {
    u8  buf[256];
    u64 descPva;
    u16 i;

    descPva           = activeCpu->nextP;
    activeCpu->nextP += 8;
    if (cpu180GetBdpDescriptor(activeCpu, descPva, activeCpu->opJ, 0, &activeCpu->srcDesc)
        && cpu180GetBdpDescriptor(activeCpu, descPva + 4, activeCpu->opK, 1, &activeCpu->dstDesc))
        {
        if (activeCpu->srcDesc.length > 256 || activeCpu->dstDesc.length > 256)
            {
            cpu180SetMonitorCondition(activeCpu, MCR51); // Instruction specification error
            return;
            }
        if (bdp180CopyToBuf(activeCpu, activeCpu->srcDesc.pva, activeCpu->srcDesc.length, buf) == FALSE)
            {
            return;
            }
        for (i = activeCpu->srcDesc.length; i < activeCpu->dstDesc.length; i++)
            {
            buf[i] = 0x20;
            }
        if (bdp180CopyFromBuf(activeCpu, activeCpu->dstDesc.pva, activeCpu->dstDesc.length, buf) == FALSE)
            {
            return;
            }

#if CcDebug > 0
#if defined(TRACE_STORE_START)
        cpu180CheckTraceStore(activeCpu, activeCpu->dstDesc.pva, activeCpu->dstDesc.pva + activeCpu->dstDesc.length - 1);
#endif
        traceMemoryBlock(activeCpu, activeCpu->srcDesc.pva, activeCpu->srcDesc.length, "    source block:");
        traceMemoryBlock(activeCpu, activeCpu->dstDesc.pva, activeCpu->dstDesc.length, "    destination block:");
#endif
        }
    }

static void cp180Op77(Cpu180Context *activeCpu)  // 77  CMPB       MIGDS 2-52
    {
    u8  *dp;
    u16 i;
    u8  *sp;
    u64 descPva;
    u8  dstBuf[256];
    u16 n;
    u16 offset;
    u8  result;
    u8  srcBuf[256];

    descPva           = activeCpu->nextP;
    activeCpu->nextP += 8;
    if (cpu180GetBdpDescriptor(activeCpu, descPva, activeCpu->opJ, 0, &activeCpu->srcDesc)
        && cpu180GetBdpDescriptor(activeCpu, descPva + 4, activeCpu->opK, 1, &activeCpu->dstDesc))
        {
        n = (activeCpu->dstDesc.length < activeCpu->srcDesc.length) ? activeCpu->srcDesc.length : activeCpu->dstDesc.length;
        if (n > 256)
            {
            cpu180SetMonitorCondition(activeCpu, MCR51); // Instruction specification error
            return;
            }
        if (bdp180CopyToBuf(activeCpu, activeCpu->srcDesc.pva, activeCpu->srcDesc.length, srcBuf) == FALSE
            || bdp180CopyToBuf(activeCpu, activeCpu->dstDesc.pva, activeCpu->dstDesc.length, dstBuf) == FALSE)
            {
            return;
            }
        for (i = activeCpu->srcDesc.length; i < n; i++)
            {
            srcBuf[i] = 0x20;
            }
        for (i = activeCpu->dstDesc.length; i < n; i++)
            {
            dstBuf[i] = 0x20;
            }
        result = 0;
        offset = 0;
        sp     = srcBuf;
        dp     = dstBuf;
        for (i = 0; i < n; i++)
            {
            if (*sp < *dp)
                {
                result = 3;
                break;
                }
            else if (*sp > *dp)
                {
                result = 1;
                break;
                }
            sp += 1;
            dp += 1;
            }
        if (result != 0)
            {
            offset             = (u16)(sp - srcBuf);
            activeCpu->regX[0] = (activeCpu->regX[0] & LeftMask) | offset;
            }
        activeCpu->regX[1] = (activeCpu->regX[1] & LeftMask) | ((u64)result << 30);

#if CcDebug > 0
        traceMemoryBlock(activeCpu, activeCpu->srcDesc.pva, activeCpu->srcDesc.length, "    source block:");
        traceMemoryBlock(activeCpu, activeCpu->dstDesc.pva, activeCpu->dstDesc.length, "    destination block:");
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
    u64              pva;
    u8               r1;
    u8               ring;
    u32              rmas[32];
    u16              selector;
    u64              word;
    u8               wordCount;
    u8               xs;
    u8               xt;

    pva = activeCpu->regA[activeCpu->opJ];
    if ((pva & Mask3) != 0)
        {
        cpu180SetMonitorCondition(activeCpu, MCR52); // address specification error
        activeCpu->regUtp = pva;
        return;
        }
    disp       = ((activeCpu->opQ <= 0x7fff) ? (u32)activeCpu->opQ : 0xffff0000 | (u32)activeCpu->opQ) << 3;
    pva        = (pva & RingSegMask) | ((pva + disp) & Mask32);
    selector   = (u16)activeCpu->regX[activeCpu->opK];
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
    if (cpu180TranslatePvaSequence(activeCpu, pva, wordCount, 8, RingOf(pva), AccessModeRead, rmas, &cond) == FALSE)
        {
        cpu180SetMonitorCondition(activeCpu, cond);
        return;
        }
    if (cpu180GetR1(activeCpu, pva, &r1, &cond) == FALSE)
        {
        cpu180SetMonitorCondition(activeCpu, cond);
        return;
        }
    ring = RingOf(pva);
    if (r1 > ring)
        {
        ring = r1;
        }
    i = 0;
    while (as <= at)
        {
        word = cpMem[rmas[i++] >> 3] & Mask48;
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
        activeCpu->regX[xs++] = cpMem[rmas[i++] >> 3];
        }

#if CcDebug > 0
        {
        char buf[40];
        sprintf(buf, "   A%X..A%X  X%X..X%X", selector >> 12, at, (selector >> 8) & Mask4, xt);
        traceMemoryBlock(activeCpu, pva, wordCount * 8, buf);
        }
#endif
    }

static void cp180Op81(Cpu180Context *activeCpu)  // 81  SMULT      MIGDS 2-16
    {
    u8               as;
    u8               at;
    MonitorCondition cond;
    u32              disp;
    u8               i;
    u64              pva;
    u32              rmas[32];
    u16              selector;
    u8               wordCount;
    u8               xs;
    u8               xt;

    pva = activeCpu->regA[activeCpu->opJ];
    if ((pva & Mask3) != 0)
        {
        cpu180SetMonitorCondition(activeCpu, MCR52); // address specification error
        activeCpu->regUtp = pva;
        return;
        }
    disp       = ((activeCpu->opQ <= 0x7fff) ? (u32)activeCpu->opQ : 0xffff0000 | (u32)activeCpu->opQ) << 3;
    pva        = (pva & RingSegMask) | ((pva + disp) & Mask32);
    selector   = (u16)activeCpu->regX[activeCpu->opK];
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
    if (cpu180TranslatePvaSequence(activeCpu, pva, wordCount, 8, RingOf(pva), AccessModeWrite, rmas, &cond) == FALSE)
        {
        cpu180SetMonitorCondition(activeCpu, cond);
        return;
        }
    i = 0;
    while (as <= at)
        {
        cpMem[rmas[i++] >> 3] = activeCpu->regA[as++];
        }
    while (xs <= xt)
        {
        cpMem[rmas[i++] >> 3] = activeCpu->regX[xs++];
        }

#if CcDebug > 0
#if defined(TRACE_STORE_START)
    cpu180CheckTraceStore(activeCpu, pva, pva + (wordCount << 3) - 1);
#endif
        {
        char buf[40];
        sprintf(buf, "   A%X..A%X  X%X..X%X", selector >> 12, at, (selector >> 8) & Mask4, xt);
        traceMemoryBlock(activeCpu, pva, wordCount * 8, buf);
        }
#endif
    }

static void cp180Op82(Cpu180Context *activeCpu)  // 82  LX         MIGDS 2-12
    {
    u64              Aj;
    u32              byteNum;
    MonitorCondition cond;
    u32              pti;
    u64              pva;
    u32              rma;

    Aj = activeCpu->regA[activeCpu->opJ];
    if ((Aj & Mask3) != 0)
        {
        cpu180SetMonitorCondition(activeCpu, MCR52);
        activeCpu->regUtp = Aj;
        return;
        }
    if (activeCpu->opQ <= 0x7fff)
        {
        byteNum = (u32)((Aj + ((u64)activeCpu->opQ << 3)) & Mask32);
        }
    else
        {
        byteNum = (u32)((Aj + ((0x1fff0000 | (u64)activeCpu->opQ) << 3)) & Mask32);
        }
    pva = (Aj & RingSegMask) | byteNum;
    if (cpu180ValidateAccess(activeCpu, pva, RingOf(pva), AccessModeRead, &cond) == FALSE
        || cpu180PvaToRma(activeCpu, pva, AccessModeRead, &rma, &pti, &cond) == FALSE)
        {
        cpu180SetMonitorCondition(activeCpu, cond);
        }
    else
        {
        activeCpu->regX[activeCpu->opK] = cpMem[rma >> 3];
        }
    }

static void cp180Op83(Cpu180Context *activeCpu)  // 83  SX         MIGDS 2-12
    {
    u64              Aj;
    u32              byteNum;
    MonitorCondition cond;
    u32              pti;
    u64              pva;
    u32              rma;

    Aj = activeCpu->regA[activeCpu->opJ];
    if ((Aj & Mask3) != 0)
        {
        cpu180SetMonitorCondition(activeCpu, MCR52);
        activeCpu->regUtp = Aj;
        return;
        }
    if (activeCpu->opQ <= 0x7fff)
        {
        byteNum = (u32)((Aj + ((u64)activeCpu->opQ << 3)) & Mask32);
        }
    else
        {
        byteNum = (u32)((Aj + ((0x1fff0000 | (u64)activeCpu->opQ) << 3)) & Mask32);
        }
    pva = (Aj & RingSegMask) | byteNum;
    if (cpu180ValidateAccess(activeCpu, pva, RingOf(pva), AccessModeWrite, &cond) == FALSE
        || cpu180PvaToRma(activeCpu, pva, AccessModeWrite, &rma, &pti, &cond) == FALSE)
        {
        cpu180SetMonitorCondition(activeCpu, cond);
        }
    else
        {
        cpuAcquireMemoryMutex();
        cpMem[rma >> 3] = activeCpu->regX[activeCpu->opK];
        cpuReleaseMemoryMutex();

#if CcDebug > 0 && defined(TRACE_STORE_START)
        cpu180CheckTraceStore(activeCpu, pva, pva + 7);
#endif
        }
    }

static void cp180Op84(Cpu180Context *activeCpu)  // 84  LA         MIGDS 2-15
    {
    u64              addr;
    u64              Aj;
    MonitorCondition cond;
    u32              disp;
    u64              pva;
    u64              r1;
    u64              r2;
    u8               ring;

    Aj   = activeCpu->regA[activeCpu->opJ];
    disp = (activeCpu->opQ <= 0x7fff) ? (u32)activeCpu->opQ : 0xffff0000 | (u32)activeCpu->opQ;
    pva  = (Aj & RingSegMask) | ((Aj + disp) & Mask32);
    if (cpu180GetBytes(activeCpu, pva, 6, RingOf(pva), AccessModeRead, &addr))
        {
        r1 = addr & RingMask;
        if (r1 == 0)
            {
            cpu180SetRingZeroCondition(activeCpu, addr);
            }
        r2 = pva & RingMask;
        if (r2 > r1)
            {
            r1 = r2;
            }
        if (cpu180GetR1(activeCpu, pva, &ring, &cond) == FALSE)
            {
            cpu180SetMonitorCondition(activeCpu, cond);
            return;
            }
        r2 = (u64)ring << 44;
        if (r2 > r1)
            {
            r1 = r2;
            }
        activeCpu->regA[activeCpu->opK] = r1 | (addr & Mask44);
        }
    }

static void cp180Op85(Cpu180Context *activeCpu)  // 85  SA         MIGDS 2-15
    {
    u64 Aj;
    u32 disp;
    u64 pva;

    Aj   = activeCpu->regA[activeCpu->opJ];
    disp = (activeCpu->opQ <= 0x7fff) ? (u32)activeCpu->opQ : 0xffff0000 | (u32)activeCpu->opQ;
    pva  = (Aj & RingSegMask) | ((Aj + disp) & Mask32);
    (void)cpu180PutBytes(activeCpu, pva, RingOf(pva), activeCpu->regA[activeCpu->opK], 6);
    }

static void cp180Op86(Cpu180Context *activeCpu)  // 86  LBYTP,j    MIGDS 2-13
    {
    u32 disp;
    u64 pva;
    u64 word;

    disp = (activeCpu->opQ <= 0x7fff) ? (u32)activeCpu->opQ : 0xffff0000 | (u32)activeCpu->opQ;
    pva  = (activeCpu->regP & RingSegMask) | ((activeCpu->regP + disp) & Mask32);
    if (cpu180GetBytes(activeCpu, pva, (activeCpu->opJ & Mask3) + 1, RingOf(pva), AccessModeExecute, &word))
        {
        activeCpu->regX[activeCpu->opK] = word;
        }
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
    u64 Aj;
    u32 offset;
    u64 pva;
    u32 q;
    u64 word;

    offset = (activeCpu->regX[0] & Mask32) >> 3;
    if (offset > 0x0fffffff)
        {
        offset |= 0xf0000000;
        }
    Aj  = activeCpu->regA[activeCpu->opJ];
    q   = (activeCpu->opQ <= 0x7fff) ? (u32)activeCpu->opQ : 0xffff0000 | (u32)activeCpu->opQ;
    pva = (Aj & RingSegMask) | ((Aj + offset + q) & Mask32);
    if (cpu180GetBytes(activeCpu, pva, 1, RingOf(pva), AccessModeRead, &word))
        {
        activeCpu->regX[activeCpu->opK] = (word >> (7 - (activeCpu->regX[0] & Mask3))) & 1;
        }
    }

static void cp180Op89(Cpu180Context *activeCpu)  // 89  SBIT       MIGDS 2-14
    {
    u64              Aj;
    MonitorCondition cond;
    u64              mask;
    u32              offset;
    u32              pti;
    u64              pva;
    u32              q;
    u32              rma;
    u8               shift;
    u32              wordAddr;

    offset = (u32)((activeCpu->regX[0] & Mask32) >> 3);
    if (offset > 0x0fffffff)
        {
        offset |= 0xf0000000;
        }
    Aj  = activeCpu->regA[activeCpu->opJ];
    q   = (activeCpu->opQ <= 0x7fff) ? (u32)activeCpu->opQ : 0xffff0000 | (u32)activeCpu->opQ;
    pva = (Aj & RingSegMask) | ((Aj + offset + q) & Mask32);
    if (cpu180ValidateAccess(activeCpu, pva, RingOf(pva), AccessModeWrite, &cond) == FALSE
        || cpu180PvaToRma(activeCpu, pva, AccessModeNone, &rma, &pti, &cond) == FALSE) // cause page fault if page not in memory
        {
        cpu180SetMonitorCondition(activeCpu, cond);
        }
    else
        {
        wordAddr        = rma >> 3;
        shift           = (u8)(56 - ((rma & Mask3) << 3)) + (u8)(7 - (activeCpu->regX[0] & Mask3));
        mask            = ~((u64)1 << shift);
        cpuAcquireMemoryMutex();
        cpMem[wordAddr] = (cpMem[wordAddr] & mask) | ((activeCpu->regX[activeCpu->opK] & 1) << shift);
        cpMem[pti]     |= (u64)3 << 60; // set page used and modified bits
        cpuReleaseMemoryMutex();

#if CcDebug > 0 && defined(TRACE_STORE_START)
        cpu180CheckTraceStore(activeCpu, pva, pva + 7);
#endif
        }
    }

static void cp180Op8A(Cpu180Context *activeCpu)  // 8A  ADDRQ      MIGDS 2-22
    {
    u32 sum;

    if (cpu180AddInt32(activeCpu, activeCpu->regX[activeCpu->opJ] & Mask32,
                       (activeCpu->opQ <= 0x7fff) ? (u32)activeCpu->opQ : 0xffff0000 | (u32)activeCpu->opQ, &sum))
        {
        activeCpu->regX[activeCpu->opK] = (activeCpu->regX[activeCpu->opK] & LeftMask) | sum;
        }
    }

static void cp180Op8B(Cpu180Context *activeCpu)  // 8B  ADDXQ      MIGDS 2-20
    {
    u64 sum;

    if (cpu180AddInt64(activeCpu, activeCpu->regX[activeCpu->opJ],
                       (activeCpu->opQ <= 0x7fff) ? (u64)activeCpu->opQ : 0xffffffffffff0000 | (u64)activeCpu->opQ, &sum))
        {
        activeCpu->regX[activeCpu->opK] = sum;
        }
    }

static void cp180Op8C(Cpu180Context *activeCpu)  // 8C  MULRQ      MIGDS 2-23
    {
    u32 product;

    if (cpu180MulInt32(activeCpu, activeCpu->regX[activeCpu->opJ] & Mask32,
                       (activeCpu->opQ <= 0x7fff) ? (u32)activeCpu->opQ : 0xffff0000 | (u32)activeCpu->opQ, &product))
        {
        activeCpu->regX[activeCpu->opK] = (activeCpu->regX[activeCpu->opK] & LeftMask) | product;
        }
    }

static void cp180Op8D(Cpu180Context *activeCpu)  // 8D  ENTE       MIGDS 2-30
    {
    if (activeCpu->opQ <= 0x7fff)
        {
        activeCpu->regX[activeCpu->opK] = (u64)activeCpu->opQ;
        }
    else
        {
        activeCpu->regX[activeCpu->opK] = 0xffffffffffff0000 | (u64)activeCpu->opQ;
        }
    }

static void cp180Op8E(Cpu180Context *activeCpu)  // 8E  ADDAQ      MIGDS 2-29
    {
    u32 disp;

    disp = (activeCpu->opQ <= 0x7fff) ? (u32)activeCpu->opQ : 0xffff0000 | (u32)activeCpu->opQ;
    activeCpu->regA[activeCpu->opK] = (activeCpu->regA[activeCpu->opJ] & RingSegMask) | ((activeCpu->regA[activeCpu->opJ] + disp) & Mask32);
    }

static void cp180Op8F(Cpu180Context *activeCpu)  // 8F  ADDPXQ     MIGDS 2-29
    {
    u32 disp;
    u32 XjR;

    XjR  = (u32)((((activeCpu->opJ == 0) ? 0 : activeCpu->regX[activeCpu->opJ]) << 1) & Mask32);
    disp = ((activeCpu->opQ <= 0x7fff) ? (u32)activeCpu->opQ : 0x7fff0000 | (u32)activeCpu->opQ) << 1;
    activeCpu->regA[activeCpu->opK] = (activeCpu->regP & RingSegMask) | ((activeCpu->regP + XjR + disp) & Mask32);
    }

static void cp180Op90(Cpu180Context *activeCpu)  // 90  BRREQ      MIGDS 2-25
    {
    u32 disp;
    i32 XjR;
    i32 XkR;

    XjR = (i32)((activeCpu->opJ == 0) ? 0 : (activeCpu->regX[activeCpu->opJ] & Mask32));
    XkR = (i32)((activeCpu->opK == 0) ? 0 : (activeCpu->regX[activeCpu->opK] & Mask32));
    if (XjR == XkR)
        {
        disp = ((activeCpu->opQ <= 0x7fff) ? (u32)activeCpu->opQ : 0x7fff0000 | (u32)activeCpu->opQ) << 1;
        activeCpu->nextP = (activeCpu->regP & RingSegMask) | ((activeCpu->regP + disp) & Mask32);
        }
    }

static void cp180Op91(Cpu180Context *activeCpu)  // 91  BRRNE      MIGDS 2-25
    {
    u32 disp;
    i32 XjR;
    i32 XkR;

    XjR = (i32)((activeCpu->opJ == 0) ? 0 : (activeCpu->regX[activeCpu->opJ] & Mask32));
    XkR = (i32)((activeCpu->opK == 0) ? 0 : (activeCpu->regX[activeCpu->opK] & Mask32));
    if (XjR != XkR)
        {
        disp = ((activeCpu->opQ <= 0x7fff) ? (u32)activeCpu->opQ : 0x7fff0000 | (u32)activeCpu->opQ) << 1;
        activeCpu->nextP = (activeCpu->regP & RingSegMask) | ((activeCpu->regP + disp) & Mask32);
        }
    }

static void cp180Op92(Cpu180Context *activeCpu)  // 92  BRRGT      MIGDS 2-25
    {
    u32 disp;
    i32 XjR;
    i32 XkR;

    XjR = (i32)((activeCpu->opJ == 0) ? 0 : (activeCpu->regX[activeCpu->opJ] & Mask32));
    XkR = (i32)((activeCpu->opK == 0) ? 0 : (activeCpu->regX[activeCpu->opK] & Mask32));
    if (XjR > XkR)
        {
        disp = ((activeCpu->opQ <= 0x7fff) ? (u32)activeCpu->opQ : 0x7fff0000 | (u32)activeCpu->opQ) << 1;
        activeCpu->nextP = (activeCpu->regP & RingSegMask) | ((activeCpu->regP + disp) & Mask32);
        }
    }

static void cp180Op93(Cpu180Context *activeCpu)  // 93  BRRGE      MIGDS 2-25
    {
    u32 disp;
    i32 XjR;
    i32 XkR;

    XjR = (i32)((activeCpu->opJ == 0) ? 0 : (activeCpu->regX[activeCpu->opJ] & Mask32));
    XkR = (i32)((activeCpu->opK == 0) ? 0 : (activeCpu->regX[activeCpu->opK] & Mask32));
    if (XjR >= XkR)
        {
        disp = ((activeCpu->opQ <= 0x7fff) ? (u32)activeCpu->opQ : 0x7fff0000 | (u32)activeCpu->opQ) << 1;
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
        disp = ((activeCpu->opQ <= 0x7fff) ? (u32)activeCpu->opQ : 0x7fff0000 | (u32)activeCpu->opQ) << 1;
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
        disp = ((activeCpu->opQ <= 0x7fff) ? (u32)activeCpu->opQ : 0x7fff0000 | (u32)activeCpu->opQ) << 1;
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
        disp = ((activeCpu->opQ <= 0x7fff) ? (u32)activeCpu->opQ : 0x7fff0000 | (u32)activeCpu->opQ) << 1;
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
        disp = ((activeCpu->opQ <= 0x7fff) ? (u32)activeCpu->opQ : 0x7fff0000 | (u32)activeCpu->opQ) << 1;
        activeCpu->nextP = (activeCpu->regP & RingSegMask) | ((activeCpu->regP + disp) & Mask32);
        }
    }

static void cp180Op98(Cpu180Context *activeCpu)  // 98  BRFEQ      MIGDS 2-87
    {
    u32 disp;
    u64 minend;
    u64 subend;
    int valence;

    minend = (activeCpu->opJ == 0) ? 0 : activeCpu->regX[activeCpu->opJ];
    subend = (activeCpu->opK == 0) ? 0 : activeCpu->regX[activeCpu->opK];
    if (float180CompareFloat(activeCpu, minend, subend, &valence))
        {
        if (valence == 0)
            {
            disp             = ((activeCpu->opQ <= 0x7fff) ? (u32)activeCpu->opQ : 0x7fff0000 | (u32)activeCpu->opQ) << 1;
            activeCpu->nextP = (activeCpu->regP & RingSegMask) | ((activeCpu->regP + disp) & Mask32);
            }
        }
    }

static void cp180Op99(Cpu180Context *activeCpu)  // 99  BRFNE      MIGDS 2-87
    {
    u32 disp;
    u64 minend;
    u64 subend;
    int valence;

    minend = (activeCpu->opJ == 0) ? 0 : activeCpu->regX[activeCpu->opJ];
    subend = (activeCpu->opK == 0) ? 0 : activeCpu->regX[activeCpu->opK];
    if (float180CompareFloat(activeCpu, minend, subend, &valence))
        {
        if (valence != 0)
            {
            disp             = ((activeCpu->opQ <= 0x7fff) ? (u32)activeCpu->opQ : 0x7fff0000 | (u32)activeCpu->opQ) << 1;
            activeCpu->nextP = (activeCpu->regP & RingSegMask) | ((activeCpu->regP + disp) & Mask32);
            }
        }
    }

static void cp180Op9A(Cpu180Context *activeCpu)  // 9A  BRFGT      MIGDS 2-87
    {
    u32 disp;
    u64 minend;
    u64 subend;
    int valence;

    minend = (activeCpu->opJ == 0) ? 0 : activeCpu->regX[activeCpu->opJ];
    subend = (activeCpu->opK == 0) ? 0 : activeCpu->regX[activeCpu->opK];
    if (float180CompareFloat(activeCpu, minend, subend, &valence))
        {
        if (valence > 0)
            {
            disp             = ((activeCpu->opQ <= 0x7fff) ? (u32)activeCpu->opQ : 0x7fff0000 | (u32)activeCpu->opQ) << 1;
            activeCpu->nextP = (activeCpu->regP & RingSegMask) | ((activeCpu->regP + disp) & Mask32);
            }
        }
    }

static void cp180Op9B(Cpu180Context *activeCpu)  // 9B  BRFGE      MIGDS 2-87
    {
    u32 disp;
    u64 minend;
    u64 subend;
    int valence;

    minend = (activeCpu->opJ == 0) ? 0 : activeCpu->regX[activeCpu->opJ];
    subend = (activeCpu->opK == 0) ? 0 : activeCpu->regX[activeCpu->opK];
    if (float180CompareFloat(activeCpu, minend, subend, &valence))
        {
        if (valence >= 0)
            {
            disp             = ((activeCpu->opQ <= 0x7fff) ? (u32)activeCpu->opQ : 0x7fff0000 | (u32)activeCpu->opQ) << 1;
            activeCpu->nextP = (activeCpu->regP & RingSegMask) | ((activeCpu->regP + disp) & Mask32);
            }
        }
    }

static void cp180Op9C(Cpu180Context *activeCpu)  // 9C  BRINC      MIGDS 2-26
    {
    u32 disp;
    i64 Xj;

    Xj = (activeCpu->opJ == 0) ? 0 : (i64)activeCpu->regX[activeCpu->opJ];
    if (Xj > (i64)activeCpu->regX[activeCpu->opK])
        {
        disp = ((activeCpu->opQ <= 0x7fff) ? (u32)activeCpu->opQ : 0x7fff0000 | (u32)activeCpu->opQ) << 1;
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
        disp = ((activeCpu->opQ <= 0x7fff) ? (u32)activeCpu->opQ : 0x7fff0000 | (u32)activeCpu->opQ) << 1;
        activeCpu->nextP = (activeCpu->regP & RingSegMask) | ((activeCpu->regP + disp) & Mask32);
        }
    else
        {
        bnj = (i32)(Aj & Mask32);
        bnk = (i32)(Ak & Mask32);
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
    u16 exponent;
    u64 brExit;
    u32 disp;

    disp     = ((activeCpu->opQ <= 0x7fff) ? (u32)activeCpu->opQ : 0x7fff0000 | (u32)activeCpu->opQ) << 1;
    brExit   = (activeCpu->regP & RingSegMask) | ((activeCpu->regP + disp) & Mask32);
    exponent = (u16)((activeCpu->regX[activeCpu->opK] >> 48) & 0x7fff);
    switch (activeCpu->opJ & Mask2)
        {
    case 0:  // Exponent Overflow
        if (exponent >= 0x5000 && exponent <= 0x6fff)
            {
            activeCpu->nextP = brExit;
            }
        break;
    case 1:  // Exponent Underflow
        if (exponent >= 0x0000 && exponent <= 0x2fff)
            {
            activeCpu->nextP = brExit;
            }
        break;
    default: // Indefinite
        if (exponent >= 0x7000 && exponent <= 0x7fff)
            {
            activeCpu->nextP = brExit;
            }
        break;
        }
    }

static void cp180Op9F(Cpu180Context *activeCpu)  // 9F  BRCR       MIGDS 2-142
    {
    u64 brExit;
    u32 disp;
    u16 mask;

    mask   = bitSelectors[activeCpu->opJ];
    disp   = ((activeCpu->opQ <= 0x7fff) ? (u32)activeCpu->opQ : 0x7fff0000 | (u32)activeCpu->opQ) << 1;
    brExit = (activeCpu->regP & RingSegMask) | ((activeCpu->regP + disp) & Mask32);

    switch (activeCpu->opK & Mask3)
        {
    default:
    case 0:
        if (activeCpu->isMonitorMode == FALSE)
            {
            cpu180SetMonitorCondition(activeCpu, MCR51); // Instruction specfication error
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
            cpu180SetMonitorCondition(activeCpu, MCR51); // Instruction specfication error
            return;
            }
        if ((activeCpu->regMcr & mask) == 0)
            {
            activeCpu->regMcr |= mask;
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
    u64              addr;
    u64              Aj;
    MonitorCondition cond;
    u32              byteNum;
    u64              pva;
    u64              r1;
    u64              r2;
    u8               ring;

    Aj      = activeCpu->regA[activeCpu->opJ];
    byteNum = (u32)((Aj + activeCpu->opD) & Mask32);
    if (activeCpu->opI != 0)
        {
        byteNum += (u32)(activeCpu->regX[activeCpu->opI] & Mask32);
        }
    pva = (Aj & RingSegMask) | byteNum;
    if (cpu180GetBytes(activeCpu, pva, 6, RingOf(pva), AccessModeRead, &addr))
        {
        r1 = addr & RingMask;
        if (r1 == 0)
            {
            cpu180SetRingZeroCondition(activeCpu, addr);
            }
        r2 = Aj & RingMask;
        if (r2 > r1)
            {
            r1 = r2;
            }
        if (cpu180GetR1(activeCpu, pva, &ring, &cond) == FALSE)
            {
            cpu180SetMonitorCondition(activeCpu, cond);
            return;
            }
        r2 = (u64)ring << 44;
        if (r2 > r1)
            {
            r1 = r2;
            }
        activeCpu->regA[activeCpu->opK] = r1 | (addr & Mask44);
        }
    }

static void cp180OpA1(Cpu180Context *activeCpu)  // A1  SAI        MIGDS 2-15
    {
    u64 Aj;
    u32 byteNum;
    u64 pva;

    Aj      = activeCpu->regA[activeCpu->opJ];
    byteNum = (u32)((Aj + activeCpu->opD) & Mask32);
    if (activeCpu->opI != 0)
        {
        byteNum += (u32)(activeCpu->regX[activeCpu->opI] & Mask32);
        }
    pva = (Aj & RingSegMask) | byteNum;
    (void)cpu180PutBytes(activeCpu, pva, RingOf(pva), activeCpu->regA[activeCpu->opK], 6);
    }

static void cp180OpA2(Cpu180Context *activeCpu)  // A2  LXI        MIGDS 2-12
    {
    u64              Aj;
    u32              byteNum;
    MonitorCondition cond;
    u32              pti;
    u64              pva;
    u32              rma;

    Aj = activeCpu->regA[activeCpu->opJ];
    if ((Aj & Mask3) != 0)
        {
        cpu180SetMonitorCondition(activeCpu, MCR52);
        activeCpu->regUtp = Aj;
        return;
        }
    byteNum = (u32)((Aj + ((u64)activeCpu->opD << 3)) & Mask32);
    if (activeCpu->opI != 0)
        {
        byteNum += (u32)((activeCpu->regX[activeCpu->opI] << 3) & Mask32);
        }
    pva = (Aj & RingSegMask) | byteNum;
    if (cpu180ValidateAccess(activeCpu, pva, RingOf(pva), AccessModeRead, &cond) == FALSE
        || cpu180PvaToRma(activeCpu, pva, AccessModeRead, &rma, &pti, &cond) == FALSE)
        {
        cpu180SetMonitorCondition(activeCpu, cond);
        }
    else
        {
        activeCpu->regX[activeCpu->opK] = cpMem[rma >> 3];
        }
    }

static void cp180OpA3(Cpu180Context *activeCpu)  // A3  SXI        MIGDS 2-12
    {
    u64              Aj;
    u32              byteNum;
    MonitorCondition cond;
    u32              pti;
    u64              pva;
    u32              rma;

    Aj = activeCpu->regA[activeCpu->opJ];
    if ((Aj & Mask3) != 0)
        {
        cpu180SetMonitorCondition(activeCpu, MCR52);
        activeCpu->regUtp = Aj;
        return;
        }
    byteNum = (u32)((Aj + ((u64)activeCpu->opD << 3)) & Mask32);
    if (activeCpu->opI != 0)
        {
        byteNum += (u32)((activeCpu->regX[activeCpu->opI] << 3) & Mask32);
        }
    pva = (Aj & RingSegMask) | byteNum;
    if (cpu180ValidateAccess(activeCpu, pva, RingOf(pva), AccessModeWrite, &cond) == FALSE
        || cpu180PvaToRma(activeCpu, pva, AccessModeWrite, &rma, &pti, &cond) == FALSE)
        {
        cpu180SetMonitorCondition(activeCpu, cond);
        }
    else
        {
        cpuAcquireMemoryMutex();
        cpMem[rma >> 3] = activeCpu->regX[activeCpu->opK];
        cpuReleaseMemoryMutex();

#if CcDebug > 0 && defined(TRACE_STORE_START)
        cpu180CheckTraceStore(activeCpu, pva, pva + 7);
#endif
        }
    }

static void cp180OpA4(Cpu180Context *activeCpu)  // A4  LBYT,X0    MIGDS 2-13
    {
    cp180OpLBYTS(activeCpu, (activeCpu->regX[0] & Mask3) + 1);
    }

static void cp180OpA5(Cpu180Context *activeCpu)  // A5  SBYT,X0    MIGDS 2-13
    {
    cp180OpSBYTS(activeCpu, (activeCpu->regX[0] & Mask3) + 1);
    }

static void cp180OpA7(Cpu180Context *activeCpu)  // A7  ADDAD      MIGDS 2-30
    {
    u64 Ai;
    u32 byteNum;
    u32 mask;

    Ai                              = activeCpu->regA[activeCpu->opI];
    byteNum                         = (u32)((Ai + activeCpu->opD) & Mask32);
    mask                            = 0xfffffff8 | (activeCpu->opJ & Mask3);
    activeCpu->regA[activeCpu->opK] = (Ai & RingSegMask) | (byteNum & mask);
    }

static void cp180OpA8(Cpu180Context *activeCpu)  // A8  SHFC       MIGDS 2-33
    {
    u64 rightBits;
    u8  shift;

    shift = (u8)((((activeCpu->opI == 0) ? 0 : activeCpu->regX[activeCpu->opI] & Mask8) + activeCpu->opD) & Mask8);
    if (shift < 0x80U)
        {
        shift = 64 - (shift & Mask6);
        }
    else
        {
        shift = (~(shift | 0x40) + 1) & Mask7;
        }
    if ((shift & Mask6) == 0)
        {
        activeCpu->regX[activeCpu->opK] = activeCpu->regX[activeCpu->opJ];
        return;
        }
    rightBits = activeCpu->regX[activeCpu->opJ] & bitMasks[shift - 1];
    activeCpu->regX[activeCpu->opK] = (rightBits << (64 - shift)) | (activeCpu->regX[activeCpu->opJ] >> shift);
    }

static void cp180OpA9(Cpu180Context *activeCpu)  // A9  SHFX       MIGDS 2-33
    {
    u8 shift;

    shift = (u8)((((activeCpu->opI == 0) ? 0 : activeCpu->regX[activeCpu->opI] & Mask8) + activeCpu->opD) & Mask8);
    if (shift < 0x80U)
        {
        activeCpu->regX[activeCpu->opK] = activeCpu->regX[activeCpu->opJ] << (shift & Mask6);
        }
    else
        {
        shift = (~(shift | 0x40) + 1) & Mask6;
        if ((activeCpu->regX[activeCpu->opJ] & 0x8000000000000000) != 0)
            {
            activeCpu->regX[activeCpu->opK] = signExt64[shift] | (activeCpu->regX[activeCpu->opJ] >> shift);
            }
        else
            {
            activeCpu->regX[activeCpu->opK] = activeCpu->regX[activeCpu->opJ] >> shift;
            }
        }
    }

static void cp180OpAA(Cpu180Context *activeCpu)  // AA  SHFR       MIGDS 2-33
    {
    u8  shift;
    u64 XjR;

    XjR   = activeCpu->regX[activeCpu->opJ] & Mask32;
    shift = (u8)((((activeCpu->opI == 0) ? 0 : activeCpu->regX[activeCpu->opI] & Mask8) + activeCpu->opD) & Mask8);
    if (shift < 0x80U)
        {
        activeCpu->regX[activeCpu->opK] = (activeCpu->regX[activeCpu->opK] & LeftMask) | ((XjR << (shift & Mask5)) & Mask32);
        }
    else
        {
        shift = (~(shift | 0x60) + 1) & Mask6;
        activeCpu->regX[activeCpu->opK] = (activeCpu->regX[activeCpu->opK] & LeftMask) | (XjR >> shift);
        if ((XjR & 0x80000000) != 0)
            {
            activeCpu->regX[activeCpu->opK] |= (u64)signExt32[shift];
            }
        }
    }

static void cp180OpAC(Cpu180Context *activeCpu)  // AC  ISOM       MIGDS 2-36
    {
    u32 desc;
    u8  first;
    u8  length;

    desc = (u32)((((activeCpu->opI == 0) ? 0 : activeCpu->regX[activeCpu->opI] & Mask32) + activeCpu->opD) & Mask12);
    first  = (u8)(desc >> 6);
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

    desc = (u32)((((activeCpu->opI == 0) ? 0 : activeCpu->regX[activeCpu->opI] & Mask32) + activeCpu->opD) & Mask12);
    first  = (u8)(desc >> 6);
    length = desc & Mask6;
    if (first + length < 64)
        {
        shift = (63 - first) - length;
        activeCpu->regX[activeCpu->opK] = (activeCpu->regX[activeCpu->opJ] >> shift) & bitMasks[length];
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

    desc = (u32)((((activeCpu->opI == 0) ? 0 : activeCpu->regX[activeCpu->opI] & Mask32) + activeCpu->opD) & Mask12);
    first  = (u8)(desc >> 6);
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
    u64              Aj;
    u64              Ak;
    u8               at;
    u64              callee;
    MonitorCondition cond;
    u32              disp;
    u32              frameSize;
    u64              sfsa;
    u8               xs;
    u8               xt;

    Aj     = activeCpu->regA[activeCpu->opJ];
    Ak     = activeCpu->regA[activeCpu->opK];
    disp   = ((activeCpu->opQ <= 0x7fff) ? (u32)activeCpu->opQ : 0xffff0000 | (u32)activeCpu->opQ) << 3;
    callee = (activeCpu->regP & RingSegMask) | ((activeCpu->regP + disp) & 0xfffffff8);
    xs     = (activeCpu->regX[0] >> 8) & Mask4;
    at     = (activeCpu->regX[0] >> 4) & Mask4;
    xt     = activeCpu->regX[0] & Mask4;
    if (cpu180PushFrame(activeCpu, at, xs, xt, FALSE, &sfsa, &frameSize, &cond) == FALSE)
        {
        cpu180SetMonitorCondition(activeCpu, cond);
        return;
        }

#if CcDebug > 0
    traceCallFrame(activeCpu, sfsa, "pushed");
#endif

    activeCpu->regA[0]   = activeCpu->regTos[RingOf(activeCpu->regP)] = sfsa + frameSize;
    activeCpu->regA[1]   = activeCpu->regA[0];
    activeCpu->regA[2]   = sfsa;
    activeCpu->regA[3]   = Aj;
    activeCpu->regA[4]   = Ak;
    activeCpu->regFlags &= 0x3fff; // clear CFF and OCF
    activeCpu->nextP     = callee;

#if CcDebug > 0
    traceCall(activeCpu, activeCpu->nextP);
#endif
    }

static void cp180OpB1(Cpu180Context *activeCpu)  // B1  KEYPOINT   MIGDS 2-133
    {
    MonitorCondition cond;
    u32              disp;
    u64              kpe;
    u16              mask;
    u32              pti;
    u32              rma;
    u32              XkR;
#if CcDebug > 0 && defined(TRACE_KEYPOINT_LIST)
    char             buf[64];
    u16              kpt;
#endif

    mask = bitSelectors[activeCpu->opJ];
    if ((activeCpu->regKmr & mask) != 0 && (activeCpu->regFlags & 0x2000) != 0)
        {
        XkR  = (u32)((activeCpu->opK == 0) ? 0 : activeCpu->regX[activeCpu->opK] & Mask32);
        disp = (activeCpu->opQ <= 0x7fff) ? (u32)activeCpu->opQ : 0xffff0000 | (u32)activeCpu->opQ;
        kpe  = ((cpu180FreeRunningCounter & Mask28) << 36) | ((u64)activeCpu->opJ << 32) | ((u64)(XkR + disp) & Mask32);
        if (cpu180ValidateAccess(activeCpu, activeCpu->regKbp, RingOf(activeCpu->regKbp), AccessModeWrite, &cond) == FALSE
            || cpu180PvaToRma(activeCpu, activeCpu->regKbp, AccessModeWrite, &rma, &pti, &cond) == FALSE)
            {
            cpu180SetMonitorCondition(activeCpu, cond);
            }
        else
            {
            cpMem[rma >> 3]    = kpe;
            activeCpu->regKbp += 8;

#if CcDebug > 0 && defined(TRACE_STORE_START)
            cpu180CheckTraceStore(activeCpu, activeCpu->regKbp, activeCpu->regKbp + 7);
#endif
            }
        }
#if CcDebug > 0 && defined(TRACE_KEYPOINT_LIST)
    switch (activeCpu->opJ)
        {
    case KeypointEntry:
        cpu180ProcessKeypointEntry(activeCpu, activeCpu->opQ);
        return;
    case KeypointExit:
        cpu180ProcessKeypointExit(activeCpu, activeCpu->opQ);
        return;
    case KeypointDebug:
        sprintf(buf, "Keypoint debug 0x%04x (%s)", activeCpu->opQ, cpu180KeypointToStr(activeCpu->opQ));
        break;
    case KeypointMtr:
        kpt = activeCpu->opQ;
        if (kpt > 4095)
            {
            kpt -= 4096;
            }
        sprintf(buf, "Keypoint mtr 0x%04x (0x%04x %s)", activeCpu->opQ, kpt, cpu180KeypointToStr(kpt));
        break;
    default:
        sprintf(buf, "Keypoint 0x%x 0x%04x (%s)", activeCpu->opJ, activeCpu->opQ, cpu180KeypointToStr(activeCpu->opQ));
        break;
        }
    traceCpuPrint(&cpus170[activeCpu->id], buf);
#endif
    }

static void cp180OpB2(Cpu180Context *activeCpu)  // B2  MULXQ      MIGDS 2-21
    {
    u64 product;

    if (cpu180MulInt64(activeCpu, activeCpu->regX[activeCpu->opJ],
        (activeCpu->opQ <= 0x7fff) ? (u64)activeCpu->opQ : 0xffffffffffff0000 | (u64)activeCpu->opQ, &product))
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
    u32              pti;
    u64              pva;
    u32              rma;
    u64              word;
    u32              wordAddr;
    u64              Xk;

    pva = activeCpu->regA[activeCpu->opJ];
    if ((pva & Mask3) != 0)
        {
        cpu180SetMonitorCondition(activeCpu, MCR52); // address specification error
        activeCpu->regUtp = pva;
        return;
        }
    if (activeCpu->regX[0] == LeftMask)
        {
        cpu180SetMonitorCondition(activeCpu, MCR51); // Instruction specification error
        return;
        }
    if (cpu180ValidateAccess(activeCpu, pva, RingOf(pva), AccessModeRead | AccessModeWrite, &cond) == FALSE
        || cpu180PvaToRma(activeCpu, pva, AccessModeNone, &rma, &pti, &cond) == FALSE) // cause page fault if page not in memory
        {
        cpu180SetMonitorCondition(activeCpu, cond);
        return;
        }
    disp     = ((activeCpu->opQ <= 0x7fff) ? (u32)activeCpu->opQ : 0x7fff0000 | (u32)activeCpu->opQ) << 1;
    Xk       = activeCpu->regX[activeCpu->opK];
    wordAddr = rma >> 3;
    cpuAcquireMemoryMutex();
    word        = cpMem[wordAddr];
    cpMem[pti] |= (u64)2 << 60; // set page used bit
    if ((word & LeftMask) == LeftMask)
        {
        activeCpu->nextP = (activeCpu->regP & RingSegMask) | ((activeCpu->regP + disp) & Mask32);
        }
    else if (Xk == word)
        {
        cpMem[wordAddr]     = activeCpu->regX[0];
        activeCpu->regX[1] &= LeftMask;
        cpMem[pti]         |= (u64)3 << 60; // set page used and modified bits

#if CcDebug > 0 && defined(TRACE_STORE_START)
        cpu180CheckTraceStore(activeCpu, pva, pva + 7);
#endif
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
    u64              Ak;
    u8               at;
    u64              bsp;
    u64              cbp;
    MonitorCondition cond;
    u32              disp;
    u32              pti;
    u8               r2;
    u8               ringBsp;
    u32              rma;
    u8               xs;
    u8               xt;

    Ak   = activeCpu->regA[activeCpu->opK];
    Aj   = activeCpu->regA[activeCpu->opJ];
    disp = ((activeCpu->opQ <= 0x7fff) ? (u32)activeCpu->opQ : 0xffff0000 | (u32)activeCpu->opQ) << 3;
    if ((Aj & Mask3) != 0)
        {
        cpu180SetMonitorCondition(activeCpu, MCR52); // Address specification error
        activeCpu->regUtp = Aj + disp;
        return;
        }
    bsp = (Aj & RingSegMask) | ((Aj + disp) & Mask32);
    if (cpu180IsBindingSectionRef(activeCpu, bsp) == FALSE)
        {
        activeCpu->regUtp = bsp;
        cpu180SetMonitorCondition(activeCpu, MCR54); // access violation
        return;
        }
    ringBsp = RingOf(bsp);
    if (cpu180GetR2(activeCpu, Aj, &r2, &cond) == FALSE)
        {
        cpu180SetMonitorCondition(activeCpu, cond);
        return;
        }
    if (ringBsp > r2)
        {
        activeCpu->regUtp = bsp;
        cpu180SetMonitorCondition(activeCpu, MCR54); // access violation
        return;
        }
    if (cpu180ValidateAccess(activeCpu, bsp, ringBsp, AccessModeRead, &cond) == FALSE
        || cpu180PvaToRma(activeCpu, bsp, AccessModeRead, &rma, &pti, &cond) == FALSE)
        {
        cpu180SetMonitorCondition(activeCpu, cond);
        return;
        }
    cbp = cpMem[rma >> 3];

#if CcDebug > 0
    traceCodebasePointer(activeCpu, bsp, rma, cbp);
#endif

    xs = (activeCpu->regX[0] >> 8) & Mask4;
    at = (activeCpu->regX[0] >> 4) & Mask4;
    xt = activeCpu->regX[0] & Mask4;
    if (cpu180CallIndirect(activeCpu, bsp, cbp, Ak, at, xs, xt, FALSE, &cond) == FALSE)
        {
        cpu180SetMonitorCondition(activeCpu, cond);
        }
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
    u64        descPva;
    bool       inhOnCond;
    bool       isOk;
    bool       isTruncated;
    BdpOperand operand;
    u8         power;
    u8         remainder;

    descPva           = activeCpu->nextP;
    activeCpu->nextP += 8;
    if (cpu180GetBdpDescriptor(activeCpu, descPva, activeCpu->opJ, 0, &activeCpu->srcDesc)
        && cpu180GetBdpDescriptor(activeCpu, descPva + 4, activeCpu->opK, 1, &activeCpu->dstDesc))
        {
        if (activeCpu->srcDesc.type > 6 || activeCpu->dstDesc.type > 6)
            {
            cpu180SetMonitorCondition(activeCpu, MCR51); // Instruction specification error
            return;
            }
        if (bdp180DecodeOperand(activeCpu, &activeCpu->srcDesc, &operand))
            {
            isOk  = TRUE;
            power = (u8)(activeCpu->opD + (activeCpu->opI == 0 ? 0 : activeCpu->regX[activeCpu->opI]));
            if (power < 38)
                {
                while (power-- > 0)
                    {
                    bdp180Mul10(&operand);
                    if (operand.value[1] != 0)
                        {
                        isOk = FALSE;
                        }
                    }
                }
            else if (power < 0x80U)
                {
                isOk = FALSE;
                memset(operand.value, 0, sizeof(operand.value));
                }
            else
                {
                power = (u8)(~power + 1);
                while (power-- > 0)
                    {
                    bdp180Div10(&operand, &remainder);
                    }
                if (operand.value[2] == 0 && operand.value[3] == 0)
                    {
                    operand.sign = 0;
                    }
                }
            if (isOk == FALSE)
                {
                cpu180SetUserCondition(activeCpu, UCR62); // Arithmetic loss of significance
                inhOnCond = (activeCpu->regUmr & ucrDefns[UCR62].bitMask) != 0 && IsTrapEnabled(activeCpu);
                }
            else
                {
                inhOnCond = FALSE;
                }
            if ((isOk || inhOnCond == FALSE) && bdp180EncodeOperand(activeCpu, &activeCpu->dstDesc, &operand, inhOnCond, &isTruncated))
                {
                if (isTruncated)
                    {
                    cpu180SetUserCondition(activeCpu, UCR57); // Arithmetic overflow
                    }
                }
            }
#if CcDebug > 0
        traceMemoryBlock(activeCpu, activeCpu->srcDesc.pva, activeCpu->srcDesc.length, "    source block:");
        traceMemoryBlock(activeCpu, activeCpu->dstDesc.pva, activeCpu->dstDesc.length, "    destination block:");
#endif
        }
    }

static void cp180OpE5(Cpu180Context *activeCpu)  // E5  SCLR       MIGDS 2-49
    {
    u64        descPva;
    bool       inhOnCond;
    bool       isOk;
    bool       isTruncated;
    BdpOperand operand;
    u8         power;
    u8         remainder;

    descPva           = activeCpu->nextP;
    activeCpu->nextP += 8;
    if (cpu180GetBdpDescriptor(activeCpu, descPva, activeCpu->opJ, 0, &activeCpu->srcDesc)
        && cpu180GetBdpDescriptor(activeCpu, descPva + 4, activeCpu->opK, 1, &activeCpu->dstDesc))
        {
        if (activeCpu->srcDesc.type > 6 || activeCpu->dstDesc.type > 6)
            {
            cpu180SetMonitorCondition(activeCpu, MCR51); // Instruction specification error
            return;
            }
        if (bdp180DecodeOperand(activeCpu, &activeCpu->srcDesc, &operand))
            {
            isOk  = TRUE;
            power = (u8)(activeCpu->opD + (activeCpu->opI == 0 ? 0 : activeCpu->regX[activeCpu->opI]));
            if (power < 38)
                {
                while (power-- > 0)
                    {
                    bdp180Mul10(&operand);
                    if (operand.value[1] != 0)
                        {
                        isOk = FALSE;
                        }
                    }
                }
            else if (power < 0x80U)
                {
                isOk = FALSE;
                memset(operand.value, 0, sizeof(operand.value));
                }
            else
                {
                power = (u8)(~power + 1);
                while (power > 1)
                    {
                    bdp180Div10(&operand, &remainder);
                    power -= 1;
                    }
                bdp180AddDigit(&operand, 5);
                bdp180Div10(&operand, &remainder);
                if (operand.value[2] == 0 && operand.value[3] == 0)
                    {
                    operand.sign = 0;
                    }
                }
            if (isOk == FALSE)
                {
                cpu180SetUserCondition(activeCpu, UCR62); // Arithmetic loss of significance
                inhOnCond = (activeCpu->regUmr & ucrDefns[UCR62].bitMask) != 0 && IsTrapEnabled(activeCpu);
                }
            else
                {
                inhOnCond = FALSE;
                }
            if ((isOk || inhOnCond == FALSE) && bdp180EncodeOperand(activeCpu, &activeCpu->dstDesc, &operand, inhOnCond, &isTruncated))
                {
                if (isTruncated)
                    {
                    cpu180SetUserCondition(activeCpu, UCR57); // Arithmetic overflow
                    }
                }
            }
#if CcDebug > 0
        traceMemoryBlock(activeCpu, activeCpu->srcDesc.pva, activeCpu->srcDesc.length, "    source block:");
        traceMemoryBlock(activeCpu, activeCpu->dstDesc.pva, activeCpu->dstDesc.length, "    destination block:");
#endif
        }
    }

static void cp180OpE9(Cpu180Context *activeCpu)  // E9  CMPC       MIGDS 2-52
    {
    u8  db;
    u8  *dp;
    u16 i;
    u8  *sp;
    u64 descPva;
    u8  dstBuf[256];
    u16 n;
    u16 offset;
    u8  result;
    u8  sb;
    u8  srcBuf[256];
    u8  trnBuf[256];
    u64 trnPva;

    descPva           = activeCpu->nextP;
    activeCpu->nextP += 8;
    if (cpu180GetBdpDescriptor(activeCpu, descPva, activeCpu->opJ, 0, &activeCpu->srcDesc)
        && cpu180GetBdpDescriptor(activeCpu, descPva + 4, activeCpu->opK, 1, &activeCpu->dstDesc))
        {
        n = (activeCpu->dstDesc.length < activeCpu->srcDesc.length) ? activeCpu->srcDesc.length : activeCpu->dstDesc.length;
        if (n > 256)
            {
            cpu180SetMonitorCondition(activeCpu, MCR51); // Instruction specification error
            return;
            }
        trnPva = (RingSegMask & activeCpu->regA[activeCpu->opI]) | ((activeCpu->regA[activeCpu->opI] + activeCpu->opD) & Mask32);
        if (bdp180CopyToBuf(activeCpu, activeCpu->srcDesc.pva, activeCpu->srcDesc.length, srcBuf) == FALSE
            || bdp180CopyToBuf(activeCpu, activeCpu->dstDesc.pva, activeCpu->dstDesc.length, dstBuf) == FALSE
            || bdp180CopyToBuf(activeCpu, trnPva, 256, trnBuf) == FALSE)
            {
            return;
            }
        for (i = activeCpu->srcDesc.length; i < n; i++)
            {
            srcBuf[i] = 0x20;
            }
        for (i = activeCpu->dstDesc.length; i < n; i++)
            {
            dstBuf[i] = 0x20;
            }
        result = 0;
        offset = 0;
        sp     = srcBuf;
        dp     = dstBuf;
        for (i = 0; i < n; i++)
            {
            sb = trnBuf[*sp];
            db = trnBuf[*dp];
            if (sb == db)
                {
                sp += 1;
                dp += 1;
                }
            else if (sb < db)
                {
                result = 3;
                break;
                }
            else
                {
                result = 1;
                break;
                }
            }
        if (result != 0)
            {
            offset             = (u16)(sp - srcBuf);
            activeCpu->regX[0] = (activeCpu->regX[0] & LeftMask) | (u64)offset;
            }
        activeCpu->regX[1] = (activeCpu->regX[1] & LeftMask) | ((u64)result << 30);

#if CcDebug > 0
        traceMemoryBlock(activeCpu, activeCpu->srcDesc.pva, activeCpu->srcDesc.length, "    source block:");
        traceMemoryBlock(activeCpu, activeCpu->dstDesc.pva, activeCpu->dstDesc.length, "    destination block:");
        traceMemoryBlock(activeCpu, trnPva, 256, "    translation table:");
#endif
        }
    }

static void cp180OpEB(Cpu180Context *activeCpu)  // EB  TRANB      MIGDS 2-54
    {
    u64 Ai;
    u8  buf[256];
    u64 descPva;
    u16 i;
    u64 pva;
    u8  table[256];

    descPva           = activeCpu->nextP;
    activeCpu->nextP += 8;
    if (cpu180GetBdpDescriptor(activeCpu, descPva, activeCpu->opJ, 0, &activeCpu->srcDesc)
        && cpu180GetBdpDescriptor(activeCpu, descPva + 4, activeCpu->opK, 1, &activeCpu->dstDesc))
        {
        if (activeCpu->srcDesc.length > 256 || activeCpu->dstDesc.length > 256)
            {
            cpu180SetMonitorCondition(activeCpu, MCR51); // Instruction specification error
            return;
            }
        if (bdp180CopyToBuf(activeCpu, activeCpu->srcDesc.pva, activeCpu->srcDesc.length, buf) == FALSE)
            {
            return;
            }
        for (i = activeCpu->srcDesc.length; i < activeCpu->dstDesc.length; i++)
            {
            buf[i] = 0x20;
            }
        Ai  = activeCpu->regA[activeCpu->opI];
        pva = (Ai & RingSegMask) | ((Ai + activeCpu->opD) & Mask32);
        if (bdp180CopyToBuf(activeCpu, pva, 256, table) == FALSE)
            {
            return;
            }
        for (i = 0; i < activeCpu->dstDesc.length; i++)
            {
            buf[i] = table[buf[i]];
            }
        if (bdp180CopyFromBuf(activeCpu, activeCpu->dstDesc.pva, activeCpu->dstDesc.length, buf) == FALSE)
            {
            return;
            }

#if CcDebug > 0
        traceMemoryBlock(activeCpu, activeCpu->srcDesc.pva, activeCpu->srcDesc.length, "    source block:");
        traceMemoryBlock(activeCpu, activeCpu->dstDesc.pva, activeCpu->dstDesc.length, "    destination block:");
        traceMemoryBlock(activeCpu, pva, 256, "    translation table:");
#endif
        }
    }

static void cp180OpED(Cpu180Context *activeCpu)  // ED  EDIT       MIGDS 2-55
    {
    u16        i;
    u64        descPva;
    u8         digit;
    u8         *dLim;
    u8         *dp;
    u8         dstBuf[256];
    bool       es;
    bool       isInvalidData;
    u16        len;
    u8         maskBuf[256];
    u64        maskPva;
    u8         mop;
    u8         *mLim;
    u8         *mp;
    u8         sct[8];
    static u8  sctPreset[8] = { 0x20, 0x20, 0x2b, 0x2d, 0x2c, 0x2e, 0x24, 0x2f };
    u8         *sLim;
    u8         sm[15];
    u8         smLen;
    bool       sn;
    u8         *sp;
    u8         srcBuf[256];
    BdpOperand srcOperand;
    u8         sv;
    bool       zf;

    descPva           = activeCpu->nextP;
    activeCpu->nextP += 8;
    if (cpu180GetBdpDescriptor(activeCpu, descPva, activeCpu->opJ, 0, &activeCpu->srcDesc) == FALSE
        || cpu180GetBdpDescriptor(activeCpu, descPva + 4, activeCpu->opK, 1, &activeCpu->dstDesc) == FALSE)
        {
        return;
        }
    if (activeCpu->srcDesc.type > 9 || activeCpu->dstDesc.length > 256 || activeCpu->srcDesc.length > 256)
        {
        cpu180SetMonitorCondition(activeCpu, MCR51); // Instruction specification error
        return;
        }
    if (activeCpu->srcDesc.type < 9)
        {
        if (bdp180DecodeOperand(activeCpu, &activeCpu->srcDesc, &srcOperand) == FALSE)
            {
            return;
            }
        if (activeCpu->srcDesc.length > 0)
            {
            switch (activeCpu->srcDesc.type)
                {
            default:
            case 0: // Packed Decimal No Sign
                len = activeCpu->srcDesc.length * 2;
                break;
            case 1: // Packed Decimal No Sign Leading Slack Digit
            case 2: // Packed Decimal Signed
                len = (activeCpu->srcDesc.length * 2) - 1;
                break;
            case 3: // Packed Decimal Signed Leading Slack Digit
                len = (activeCpu->srcDesc.length * 2) - 2;
                break;
            case 4: // Unpacked Decimal Unsigned
            case 5: // Unpacked Decimal Trailing Sign Combined Hollerith
            case 7: // Unpacked Decimal Leading Sign Combined Hollerith
                len = activeCpu->srcDesc.length;
                break;
            case 6: // Unpacked Decimal Trailing Sign Separate
            case 8: // Unpacked Decimal Leading Sign Separate
                len = activeCpu->srcDesc.length - 1;
                break;
                }
            sLim = &srcBuf[len];
            sp   = sLim;
            while (len-- > 0)
                {
                bdp180Div10(&srcOperand, &digit);
                sp -= 1;
                *sp = 0x30 + digit;
                }
            }
        else
            {
            sLim = srcBuf;
            }
        sp = srcBuf;
        sn = srcOperand.rawSign;
        }
    else // type == 9
        {
        if (bdp180CopyToBuf(activeCpu, activeCpu->srcDesc.pva, activeCpu->srcDesc.length, srcBuf) == FALSE)
            {
            return;
            }
        sp   = srcBuf;
        sLim = sp + activeCpu->srcDesc.length;
        sn   = FALSE;
        }
    maskPva = (RingSegMask & activeCpu->regA[activeCpu->opI]) | ((activeCpu->regA[activeCpu->opI] + activeCpu->opD) & Mask32);
    if (bdp180CopyToBuf(activeCpu, maskPva, 1, maskBuf) == FALSE) // fetch the length byte
        {
        return;
        }
    len = maskBuf[0];
    if (bdp180CopyToBuf(activeCpu, maskPva, len, maskBuf) == FALSE) // fetch the whole edit mask
        {
        return;
        }
    dp    = dstBuf;
    dLim  = dstBuf + activeCpu->dstDesc.length;
    mp    = &maskBuf[1];
    mLim  = maskBuf + maskBuf[0];
    smLen = 0;
    es    = FALSE;
    zf    = TRUE;
    memcpy(sct, sctPreset, sizeof(sct));

    //
    //  Traverse the edit mask and execute the MOP's in it one by one
    //
    isInvalidData = FALSE;
    while (mp < mLim && isInvalidData == FALSE)
        {
        mop = *mp >> 4;
        sv  = *mp & Mask4;
        mp += 1;
        switch (mop)
            {
        //
        //  MOP 0
        //
        case 0x0:
            if ((activeCpu->srcDesc.type == 9 && sv > 0) || dp + sv > dLim || sp + sv > sLim)
                {
                isInvalidData = TRUE;
                }
            else if (sv > 0)
                {
                es = TRUE;
                while (sv-- > 0)
                    {
                    if (*sp != '0')
                        {
                        zf = FALSE;
                        }
                    *dp++ = *sp++;
                    }
                }
            break;
        //
        //  MOP 1
        //
        case 0x1:
            if ((activeCpu->srcDesc.type != 9 && sv > 0) || dp + sv > dLim || sp + sv > sLim)
                {
                isInvalidData = TRUE;
                }
            else if (sv > 0)
                {
                es = TRUE;
                while (sv-- > 0)
                    {
                    *dp++ = *sp++;
                    }
                }
            break;
        //
        //  MOP 2 and 3
        //
        default:
        case 0x2: // no operation
        case 0x3:
            break;
        //
        //  MOP 4
        //
        case 0x4:
            if (dp + sv > dLim || mp + sv > mLim)
                {
                isInvalidData = TRUE;
                }
            else if (sv > 0)
                {
                while (sv-- > 0)
                    {
                    *dp++ = *mp++;
                    }
                }
            break;
        //
        //  MOP 5
        //
        case 0x5:
            sm[0] = sn ? sct[3] : sct[sv & Mask3];
            smLen = 1;
            break;
        //
        //  MOP 6
        //
        case 0x6:
            if (mp + sv > mLim)
                {
                isInvalidData = TRUE;
                }
            else
                {
                for (i = 0; i < sv; i++)
                    {
                    sm[i] = *mp++;
                    }
                smLen = sv;
                }
            break;
        //
        //  MOP 7
        //
        case 0x7:
            if ((activeCpu->srcDesc.type == 9 && sv > 0) || sp + sv > sLim)
                {
                isInvalidData = TRUE;
                }
            else if (sv > 0)
                {
                while (sv-- > 0)
                    {
                    if (es == FALSE)
                        {
                        if (*sp == '0')
                            {
                            if (dp + 1 > dLim)
                                {
                                isInvalidData = TRUE;
                                }
                            else
                                {
                                *dp++ = sct[1];
                                }
                            }
                        else
                            {
                            es = TRUE;
                            if (dp + smLen + 1 > dLim)
                                {
                                isInvalidData = TRUE;
                                }
                            else
                                {
                                memcpy(dp, sm, smLen);
                                dp   += smLen;
                                *dp++ = *sp;
                                zf    = FALSE;
                                }
                            }
                        }
                    else if (dp + 1 > dLim)
                        {
                        isInvalidData = TRUE;
                        }
                    else
                        {
                        if (*sp != '0')
                            {
                            zf = FALSE;
                            }
                        *dp++ = *sp;
                        }
                    sp += 1;
                    }
                }
            break;
        //
        //  MOP 8
        //
        case 0x8:
            if (es == FALSE)
                {
                es = TRUE;
                if (dp + smLen > dLim)
                    {
                    isInvalidData = TRUE;
                    }
                else
                    {
                    memcpy(dp, sm, smLen);
                    dp   += smLen;
                    smLen = 0;
                    }
                }
            break;
        //
        //  MOP 9
        //
        case 0x9:
            if (sv > 7)
                {
                if (dp + smLen > dLim)
                    {
                    isInvalidData = TRUE;
                    }
                else
                    {
                    memcpy(dp, sm, smLen);
                    dp   += smLen;
                    smLen = 0;
                    }
                }
            else if (dp + 1 > dLim)
                {
                isInvalidData = TRUE;
                }
            else
                {
                *dp++ = sct[sv];
                }
            break;
        //
        //  MOP A
        //
        case 0xa:
            if (sv > 7)
                {
                if (dp + smLen > dLim)
                    {
                    isInvalidData = TRUE;
                    }
                if (sn == FALSE)
                    {
                    memcpy(dp, sm, smLen);
                    dp   += smLen;
                    smLen = 0;
                    }
                else
                    {
                    for (i = 0; i < smLen; i++)
                        {
                        *dp++ = sct[0];
                        }
                    }
                }
            else if (dp + 1 > dLim)
                {
                isInvalidData = TRUE;
                }
            else if (sn == FALSE)
                {
                *dp++ = sct[sv];
                }
            else
                {
                *dp++ = sct[0];
                }
            break;
        //
        //  MOP B
        //
        case 0xb:
            if (sv > 7)
                {
                if (dp + smLen > dLim)
                    {
                    isInvalidData = TRUE;
                    }
                if (sn == TRUE)
                    {
                    memcpy(dp, sm, smLen);
                    dp   += smLen;
                    smLen = 0;
                    }
                else
                    {
                    for (i = 0; i < smLen; i++)
                        {
                        *dp++ = sct[0];
                        }
                    }
                }
            else if (dp + 1 > dLim)
                {
                isInvalidData = TRUE;
                }
            else if (sn == TRUE)
                {
                *dp++ = sct[sv];
                }
            else
                {
                *dp++ = sct[0];
                }
            break;
        //
        //  MOP C
        //
        case 0xc:
            if (sv > 7)
                {
                if (dp + smLen > dLim)
                    {
                    isInvalidData = TRUE;
                    }
                else if (es)
                    {
                    memcpy(dp, sm, smLen);
                    dp   += smLen;
                    smLen = 0;
                    }
                else
                    {
                    for (i = 0; i < smLen; i++)
                        {
                        *dp++ = sct[1];
                        }
                    }
                }
            else if (dp + 1 > dLim)
                {
                isInvalidData = TRUE;
                }
            else if (es)
                {
                *dp++ = sct[sv];
                }
            else
                {
                *dp++ = sct[1];
                }
            break;
        //
        //  MOP D
        //
        case 0xd:
            if (mp + 1 > mLim)
                {
                isInvalidData = TRUE;
                }
            else
                {
                sct[sv & Mask3] = *mp++;
                }
            break;
        //
        //  MOP E
        //
        case 0xe:
            if (dp + sv > dLim)
                {
                isInvalidData = TRUE;
                }
            else
                {
                for (i = 0; i < sv; i++)
                    {
                    *dp++ = sct[1];
                    }
                }
            break;
        //
        //  MOP F
        //
        case 0xf:
            if (sv > 0)
                {
                if (zf == FALSE)
                    {
                    mp = mLim;
                    }
                else
                    {
                    dp = dstBuf;
                    if (dp + sv > dLim)
                        {
                        isInvalidData = TRUE;
                        }
                    else
                        {
                        for (i = 0; i < sv; i++)
                            {
                            *dp++ = sct[1];
                            }
                        }
                    }
                }
            break;
            }
        }

    if (isInvalidData)
        {
        cpu180SetUserCondition(activeCpu, UCR63); // Invalid BDP data
        }
    if (isInvalidData == FALSE || IsTrapEnabled(activeCpu) == FALSE)
        {
        if (bdp180CopyFromBuf(activeCpu, activeCpu->dstDesc.pva, (u16)(dp - dstBuf), dstBuf) == FALSE)
            {
            return;
            }
        }

#if CcDebug > 0
    traceMemoryBlock(activeCpu, activeCpu->srcDesc.pva, activeCpu->srcDesc.length, "    source block:");
    traceMemoryBlock(activeCpu, activeCpu->dstDesc.pva, activeCpu->dstDesc.length, "    destination block:");
    traceMemoryBlock(activeCpu, maskPva, maskBuf[0], "    edit mask:");
#endif
    }

static void cp180OpF3(Cpu180Context *activeCpu)  // F3  SCNB       MIGDS 2-54
    {
    u8  bitIdx;
    u8  bits;
    u64 descPva;
    u8  dstBuf[256];
    u16 i;
    u8  table[32];
    u64 tPva;

    static u8 masks[8] = { 0x80,0x40,0x20,0x10,0x08,0x04,0x02,0x01 };

    descPva           = activeCpu->nextP;
    activeCpu->nextP += 4;
    if (cpu180GetBdpDescriptor(activeCpu, descPva, activeCpu->opK, 1, &activeCpu->dstDesc))
        {
        if (activeCpu->dstDesc.length > 256)
            {
            cpu180SetMonitorCondition(activeCpu, MCR51); // Instruction specification error
            return;
            }
        tPva = (activeCpu->regA[activeCpu->opI] & RingSegMask) | ((activeCpu->regA[activeCpu->opI] + activeCpu->opD) & Mask32);
        if (bdp180CopyToBuf(activeCpu, activeCpu->dstDesc.pva, activeCpu->dstDesc.length, dstBuf) == FALSE
            || bdp180CopyToBuf(activeCpu, tPva, 32, table) == FALSE)
            {
            return;
            }

#if CcDebug > 0
        traceMemoryBlock(activeCpu, activeCpu->dstDesc.pva, activeCpu->dstDesc.length, "    byte string:");
        traceMemoryBlock(activeCpu, tPva, 32, "    bit table:");
#endif

        i = 0;
        while (i < activeCpu->dstDesc.length)
            {
            bitIdx = dstBuf[i];
            bits   = table[bitIdx >> 3];
            if ((bits & masks[bitIdx & Mask3]) != 0)
                {
                activeCpu->regX[0] = (activeCpu->regX[0] & LeftMask) | i;
                activeCpu->regX[1] = (activeCpu->regX[1] & LeftMask) | bitIdx;
                return;
                }
            i += 1;
            }
        activeCpu->regX[0] = (activeCpu->regX[0] & LeftMask) | activeCpu->dstDesc.length;
        activeCpu->regX[1] = (activeCpu->regX[1] & LeftMask) | 0x80000000U;
        }
    }

static void cp180OpF9(Cpu180Context *activeCpu)  // F9  MOVI       MIGDS 2-62
    {
    u8         buf[256];
    u8         byte;
    u64        descPva;
    bool       isTruncated;
    BdpOperand operand;

    descPva           = activeCpu->nextP;
    activeCpu->nextP += 4;
    if (cpu180GetBdpDescriptor(activeCpu, descPva, activeCpu->opK, 1, &activeCpu->dstDesc) == FALSE)
        {
        return;
        }
    byte = (u8)((((activeCpu->opI == 0) ? 0 : activeCpu->regX[activeCpu->opI]) + activeCpu->opD) & Mask8);
    memset(&operand, 0, sizeof(operand));
    operand.value[3] = byte;
    switch (activeCpu->opJ & Mask2)
        {
    default:
    case 0:
        if (activeCpu->dstDesc.type != 10 && activeCpu->dstDesc.type != 11)
            {
            cpu180SetMonitorCondition(activeCpu, MCR51); // Instruction specification error
            return;
            }
        if (bdp180EncodeOperand(activeCpu, &activeCpu->dstDesc, &operand, FALSE, &isTruncated) == FALSE)
            {
            return;
            }
        if (isTruncated)
            {
            cpu180SetUserCondition(activeCpu, UCR62); // Arithmetic loss of significance
            }
        break;
    case 1:
        if (activeCpu->dstDesc.type > 6)
            {
            cpu180SetMonitorCondition(activeCpu, MCR51); // Instruction specification error
            return;
            }
        if (byte < 0x30 || byte > 0x39)
            {
            cpu180SetUserCondition(activeCpu, UCR63); // Invalid BDP data
            return;
            }
        operand.value[3] = (u64)byte - 0x30;
        if (bdp180EncodeOperand(activeCpu, &activeCpu->dstDesc, &operand,
            (activeCpu->regUmr & ucrDefns[UCR62].bitMask) != 0 && IsTrapEnabled(activeCpu), &isTruncated) == FALSE)
            {
            return;
            }
        if (isTruncated)
            {
            cpu180SetUserCondition(activeCpu, UCR62); // Arithmetic loss of significance
            }
        break;
    case 2:
        if (activeCpu->dstDesc.length > 256)
            {
            cpu180SetMonitorCondition(activeCpu, MCR51); // Instruction specification error
            return;
            }
        memset(buf, byte, activeCpu->dstDesc.length);
        if (bdp180CopyFromBuf(activeCpu, activeCpu->dstDesc.pva, activeCpu->dstDesc.length, buf) == FALSE)
            {
            return;
            }
        break;
    case 3:
        if (activeCpu->dstDesc.length > 256)
            {
            cpu180SetMonitorCondition(activeCpu, MCR51); // Instruction specification error
            return;
            }
        memset(buf, ' ', activeCpu->dstDesc.length);
        buf[0] = byte;
        if (bdp180CopyFromBuf(activeCpu, activeCpu->dstDesc.pva, activeCpu->dstDesc.length, buf) == FALSE)
            {
            return;
            }
        break;
        }

#if CcDebug > 0
#if defined(TRACE_STORE_START)
    cpu180CheckTraceStore(activeCpu, activeCpu->dstDesc.pva, activeCpu->dstDesc.pva + activeCpu->dstDesc.length - 1);
#endif
    traceMemoryBlock(activeCpu, activeCpu->dstDesc.pva, activeCpu->dstDesc.length, "    destination block:");
#endif
    }

static void cp180OpFA(Cpu180Context *activeCpu)  // FA  CMPI       MIGDS 2-63
    {
    u8         buf[256];
    u8         byte;
    u64        descPva;
    u16        i;
    u16        len;
    BdpOperand operand;
    u8         result;

    descPva           = activeCpu->nextP;
    activeCpu->nextP += 4;
    if (cpu180GetBdpDescriptor(activeCpu, descPva, activeCpu->opK, 1, &activeCpu->dstDesc) == FALSE)
        {
        return;
        }
    byte   = (u8)((((activeCpu->opI == 0) ? 0 : activeCpu->regX[activeCpu->opI]) + activeCpu->opD) & Mask8);
    result = 0;
    switch (activeCpu->opJ & Mask2)
        {
    default:
    case 0:
        if (activeCpu->dstDesc.type != 10 && activeCpu->dstDesc.type != 11)
            {
            cpu180SetMonitorCondition(activeCpu, MCR51); // Instruction specification error
            return;
            }
        if (bdp180DecodeOperand(activeCpu, &activeCpu->dstDesc, &operand) == FALSE)
            {
            return;
            }
        break;

    case 1:
        if (activeCpu->dstDesc.type > 6)
            {
            cpu180SetMonitorCondition(activeCpu, MCR51); // Instruction specification error
            return;
            }
        if (byte < 0x30 || byte > 0x39)
            {
            cpu180SetUserCondition(activeCpu, UCR63); // Invalid BDP data
            return;
            }
        if (bdp180DecodeOperand(activeCpu, &activeCpu->dstDesc, &operand) == FALSE)
            {
            return;
            }
        byte -= 0x30;
        break;

    case 2:
        if (activeCpu->dstDesc.length > 256)
            {
            cpu180SetMonitorCondition(activeCpu, MCR51); // Instruction specification error
            return;
            }
        if (bdp180CopyToBuf(activeCpu, activeCpu->dstDesc.pva, activeCpu->dstDesc.length, buf) == FALSE)
            {
            return;
            }
        len = activeCpu->dstDesc.length;
        if (len < 1)
            {
            len    = 1;
            buf[0] = 0x20;
            }
        for (i = 0; i < len; i++)
            {
            if (byte < buf[i])
                {
                result = 3;
                break;
                }
            else if (byte > buf[i])
                {
                result = 1;
                break;
                }
            }
        activeCpu->regX[1] = (activeCpu->regX[1] & LeftMask) | ((u64)result << 30);

#if CcDebug > 0
        traceMemoryBlock(activeCpu, activeCpu->dstDesc.pva, activeCpu->dstDesc.length, "    destination block:");
#endif
        return;

    case 3:
        if (activeCpu->dstDesc.length > 256)
            {
            cpu180SetMonitorCondition(activeCpu, MCR51); // Instruction specification error
            return;
            }
        if (bdp180CopyToBuf(activeCpu, activeCpu->dstDesc.pva, activeCpu->dstDesc.length, buf) == FALSE)
            {
            return;
            }
        len = activeCpu->dstDesc.length;
        if (len < 1)
            {
            len    = 1;
            buf[0] = 0x20;
            }
        if (byte == buf[0])
            {
            for (i = 1; i < len; i++)
                {
                if (0x20 < buf[i])
                    {
                    result = 3;
                    break;
                    }
                else if (0x20 > buf[i])
                    {
                    result = 1;
                    break;
                    }
                }
            }
        else if (byte < buf[0])
            {
            result = 3;
            }
        else if (byte > buf[0])
            {
            result = 1;
            }
        activeCpu->regX[1] = (activeCpu->regX[1] & LeftMask) | ((u64)result << 30);

#if CcDebug > 0
        traceMemoryBlock(activeCpu, activeCpu->dstDesc.pva, activeCpu->dstDesc.length, "    destination block:");
#endif
        return;
        }
    if (operand.sign) // operand is negative
        {
        result = 1;
        }
    else if (operand.value[2] != 0 || byte < operand.value[3])
        {
        result = 3;
        }
    else if (byte > operand.value[3])
        {
        result = 1;
        }
    activeCpu->regX[1] = (activeCpu->regX[1] & LeftMask) | ((u64)result << 30);

#if CcDebug > 0
    traceMemoryBlock(activeCpu, activeCpu->dstDesc.pva, activeCpu->dstDesc.length, "    destination block:");
#endif
    }

static void cp180OpFB(Cpu180Context *activeCpu)  // FB  ADDI       MIGDS 2-64
    {
    u8             byte;
    UserCondition cond;
    u64            descPva;
    BdpOperand     dstOperand;
    bool           isTruncated;
    BdpOperand     result;
    BdpOperand     srcOperand;

    descPva           = activeCpu->nextP;
    activeCpu->nextP += 4;
    if (cpu180GetBdpDescriptor(activeCpu, descPva, activeCpu->opK, 1, &activeCpu->dstDesc) == FALSE)
        {
        return;
        }
    byte = (u8)((((activeCpu->opI == 0) ? 0 : activeCpu->regX[activeCpu->opI]) + activeCpu->opD) & Mask8);
    if ((activeCpu->opJ & 1) == 0) // j == 0
        {
        if (activeCpu->dstDesc.type != 10 && activeCpu->dstDesc.type != 11)
            {
            cpu180SetMonitorCondition(activeCpu, MCR51); // Instruction specification error
            return;
            }
        if (bdp180DecodeOperand(activeCpu, &activeCpu->dstDesc, &dstOperand) == FALSE)
            {
            return;
            }
        }
    else // j == 1
        {
        if (activeCpu->dstDesc.type > 6)
            {
            cpu180SetMonitorCondition(activeCpu, MCR51); // Instruction specification error
            return;
            }
        if (byte < 0x30 || byte > 0x39)
            {
            cpu180SetUserCondition(activeCpu, UCR63); // Invalid BDP data
            return;
            }
        if (bdp180DecodeOperand(activeCpu, &activeCpu->dstDesc, &dstOperand) == FALSE)
            {
            return;
            }
        byte -= 0x30;
        }

#if CcDebug > 0
    traceMemoryBlock(activeCpu, activeCpu->dstDesc.pva, activeCpu->dstDesc.length, "    destination block:");
#endif

    memset(&srcOperand, 0, sizeof(BdpOperand));
    srcOperand.value[3] = byte;
    if (bdp180Add(&dstOperand, &srcOperand, &result, &cond) == FALSE)
        {
        cpu180SetUserCondition(activeCpu, cond);
        }
    else if (bdp180EncodeOperand(activeCpu, &activeCpu->dstDesc, &result, TRUE, &isTruncated))
        {
        if (isTruncated)
            {
            cpu180SetUserCondition(activeCpu, UCR57); // Arithmetic overflow
            }
        }

#if CcDebug > 0
    traceMemoryBlock(activeCpu, activeCpu->dstDesc.pva, activeCpu->dstDesc.length, "    destination result block:");
#endif
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

#if DEBUG
    fprintf(stderr,"Invalid instruction %02x (P " FMT64_012x ")\n", activeCpu->opCode, activeCpu->regP);
#endif
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
    u64 Aj;
    u32 byteNum;
    u64 pva;
    u64 word;

    Aj      = activeCpu->regA[activeCpu->opJ];
    byteNum = (u32)((Aj + activeCpu->opD) & Mask32);
    if (activeCpu->opI != 0)
        {
        byteNum += (u32)(activeCpu->regX[activeCpu->opI] & Mask32);
        }
    pva = (Aj & RingSegMask) | byteNum;
    if (cpu180GetBytes(activeCpu, pva, count, RingOf(pva), AccessModeRead, &word))
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
static void cp180OpSBYTS(Cpu180Context *activeCpu, u8 count)
    {
    u64 Aj;
    u32 byteNum;
    u64 pva;

    Aj      = activeCpu->regA[activeCpu->opJ];
    byteNum = (u32)((Aj + activeCpu->opD) & Mask32);
    if (activeCpu->opI != 0)
        {
        byteNum += (u32)(activeCpu->regX[activeCpu->opI] & Mask32);
        }
    pva = (Aj & RingSegMask) | byteNum;
    cpuAcquireMemoryMutex();
    (void)cpu180PutBytes(activeCpu, pva, RingOf(pva), activeCpu->regX[activeCpu->opK], count);
    cpuReleaseMemoryMutex();
    }

#if CcDebug > 0

#if defined(TRACE_STORE_START)
/*--------------------------------------------------------------------------
**  Purpose:        Check whether store operation should be traced
**
**  Parameters:     Name        Description.
**                  ctx         pointer to CPU context
**                  pvaStart    start of PVA range
**                  pvaEnd      end of PVA range
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cpu180CheckTraceStore(Cpu180Context *ctx, u64 pvaStart, u64 pvaEnd)
    {
    if ((traceMask & TRACECPU(ctx, TraceCpu180)) == 0 && pvaStart >= (TRACE_STORE_START) && pvaEnd <= (TRACE_STORE_END))
        {
        if ((traceMask & TRACECPU(ctx, TraceCpu180)) == 0)
            {
            traceCpuBreak(ctx);
            }
        traceMask              |= TRACECPU(ctx, TraceCpu180 | TraceExchange | TraceCallFrame | TraceBlockOp);
        traceInstCount[ctx->id] = TRACE_INST_COUNT;
        }
    }

#endif

#if defined(TRACE_KEYPOINT_LIST)

#define KEYPOINT_STACK_SIZE 2000
static u16 keypointStack[2][KEYPOINT_STACK_SIZE];
static u16 keypointStackPtr[2] = { 0, 0 };

/*--------------------------------------------------------------------------
**  Purpose:        Pop a keypoint identifier from the keypoint stack
**
**  Parameters:     Name        Description.
**                  ctx         pointer to CYBER 180 CPU context
**                  kpt         the keypoint identifier
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cpu180PopKeypoint(Cpu180Context *ctx, u16 kpt)
    {
    u16 stkPtr;

    stkPtr = keypointStackPtr[ctx->id];
    while (stkPtr > 0)
        {
        stkPtr -= 1;
        if (keypointStack[ctx->id][stkPtr] == kpt)
            {
            keypointStackPtr[ctx->id] = stkPtr;
            return;
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Push a keypoint identifier onto the keypoint stack
**
**  Parameters:     Name        Description.
**                  ctx         pointer to CYBER 180 CPU context
**                  kpt         the keypoint identifier
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cpu180PushKeypoint(Cpu180Context *ctx, u16 kpt)
    {
    if (keypointStackPtr[ctx->id] < (KEYPOINT_STACK_SIZE - 1))
        {
        keypointStack[ctx->id][keypointStackPtr[ctx->id]++] = kpt;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Process entry into a system keypoint
**
**  Parameters:     Name        Description.
**                  ctx         pointer to CYBER 180 CPU context
**                  kpt         the keypoint identifier
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cpu180ProcessKeypointEntry(Cpu180Context *ctx, u16 kpt)
    {
    char buf[64];
    u16  stkPtr;

    stkPtr = keypointStackPtr[ctx->id];
    if (stkPtr > 0 && keypointStack[ctx->id][stkPtr - 1] == kpt)
        {
        //
        //  A number of NOS/VE procedures have a bug where they re-issue
        //  keypoint entry when exiting a procedure instead of properly
        //  issuing keypoint exit. Thus, if we detect that the entry on
        //  on the top of the stack matches the keypoint identifier
        //  specified as a parameter, this indicates that the procedure
        //  is actually exiting instead of entering. Note that this assumes
        //  that no NOS/VE procedures issuing keypoint entry are recursive.
        //
        cpu180ProcessKeypointExit(ctx, kpt);
        }
    else
        {
        cpu180PushKeypoint(ctx, kpt);
        if (cpu180SearchKeypointList(kpt))
            {
            sprintf(buf, "Keypoint entry 0x%04x (%s)", kpt, cpu180KeypointToStr(kpt));
            if ((traceMask & TRACECPU(ctx, TraceCpu180)) == 0)
                {
                traceMask              |= TRACECPU(ctx, TraceCpu180 | TraceExchange | TraceCallFrame | TraceBlockOp);
                traceInstCount[ctx->id] = 0;
                }
            traceCpuBreak(ctx);
            traceCpuPrint(&cpus170[ctx->id], buf);
            traceDumpStackFrames(ctx, 8);
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Process exit from a system keypoint
**
**  Parameters:     Name        Description.
**                  ctx         pointer to CYBER 180 CPU context
**                  kpt         the keypoint identifier
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cpu180ProcessKeypointExit(Cpu180Context *ctx, u16 kpt)
    {
    char buf[64];
    u16  stkPtr;
    
    stkPtr = keypointStackPtr[ctx->id];
    if (stkPtr > 0)
        {
        cpu180PopKeypoint(ctx, kpt);
        if (cpu180SearchKeypointList(kpt))
            {
            sprintf(buf, "Keypoint exit 0x%04x (%s)", kpt, cpu180KeypointToStr(kpt));
            traceCpuPrint(&cpus170[ctx->id], buf);
            stkPtr = keypointStackPtr[ctx->id];
            while (stkPtr > 0)
                {
                stkPtr -= 1;
                kpt     = keypointStack[ctx->id][stkPtr];
                sprintf(buf, "  Called from 0x%04x (%s)", kpt, cpu180KeypointToStr(kpt));
                traceCpuPrint(&cpus170[ctx->id], buf);
                if (cpu180SearchKeypointList(kpt)) break;
                }
            if (stkPtr < 1)
                {
                traceInstCount[ctx->id] = TRACE_INST_COUNT;
                }
            keypointStackPtr[ctx->id] = stkPtr;
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Search the keypoint list for a specified keypoint identifier
**
**  Parameters:     Name        Description.
**                  kpt         the keypoint identifier
**
**  Returns:        TRUE if the identifier is in the list.
**
**------------------------------------------------------------------------*/
static bool cpu180SearchKeypointList(u16 kpt)
    {
    u8 i;
    u8 limit;
    limit = sizeof(traceKeypointList) / sizeof(u16);

    for (i = 0; i < limit; i++)
        {
        if (traceKeypointList[i] == kpt)
            {
            return TRUE;
            }
        }
    return FALSE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Map a keypoint identifier to a NOS/VE procedure name
**
**  Parameters:     Name        Description.
**                  kpt         the keypoint identifier
**
**  Returns:        Procedure name.
**
**------------------------------------------------------------------------*/
static char *cpu180KeypointToStr(u16 kpt)
    {
    static char buf[8];

    switch (kpt)
        {
    case amk_close:                       return "amp$close";
    case amk_copy_file:                   return "amp$copy_file";
    case amk_get_file_attributes:         return "amp$get_file_attributes";
    case amk_get_next:                    return "amp$get_next";
    case amk_open:                        return "amp$open";
    case amk_return:                      return "amp$return";
    case bak_connected_file_device:       return "bap$connected_file_device";
    case bak_open_file:                   return "bap$open_file";
    case cmk_build_interface_tables:      return "cmp$build_interface_tables";
    case cmk_build_pp_interface_table:    return "cmp$build_pp_interface_table";
    case cmk_pc_get_logical_unit:         return "cmp$pc_get_logical_unit";
    case cmk_pc_get_next_channel:         return "cmp$pc_get_next_channel";
    case cmk_get_conf_file:               return "cmp$get_conf_file";
    case cmk_install_conf_file:           return "cmp$install_conf_file";
    case clk_create_file_connection:      return "clp$create_file_connection";
    case clk_declare_variable:            return "clp$declare_variable";
    case clk_get_line_from_command_file:  return "clp$get_line_from_command_file";
    case clk_open_command_file:           return "clp$open_command_file";
    case clk_process_command:             return "clp$process_command";
    case clk_read_variable:               return "clp$read_variable";
    case clk_scan_command_file:           return "clp$scan_command_file";
    case clk_scan_command_line:           return "clp$scan_command_line";
    case clk_scan_parameter_list:         return "clp$scan_parameter_list";
    case clk_include_file:                return "clp$include_file";
    case clk_include_line:                return "clp$include_line";
    case ifk_get_terminal_attributes:     return "ifp$get_terminal_attributes";
    case lok_load_program:                return "lop$load_program";
    case lok_load_module_from_library:    return "lop$load_module_from_library";
    case lok_satisfy_externals:           return "lop$satisfy_externals";
    case lok_load_module:                 return "lop$load_module";
    case mmk_page_fault:                  return "mmp$page_fault";
    case mmk_build_lock_rmal:             return "mmp$build_lock_rmal";
    case mmk_advise_out:                  return "mmp$advise_out";
    case mmk_write_modified_pages:        return "mmp$write_modified_pages";
    case ofk_screen_input_fap:            return "ofp$screen_input_fap";
    case osk_generate_message:            return "osp$generate_message";
    case osk_format_message:              return "osp$format_message";
    case osk_set_status_abnormal:         return "osp$set_status_abnormal";
    case osk_await_activity_completion:   return "osp$await_activity_completion";
    case osk_allocate:                    return "osp$allocate";
    case pfk_attach:                      return "pfp$attach";
    case pfk_get_object_information:      return "pfp$get_object_information";
    case pfk_restricted_attach:           return "pfp$restricted_attach";
    case pfk_return_permanent_file:       return "pfp$return_permanent_file";
    case pmk_task_begin_end:              return "pmp$task_begin_end";
    case pmk_task_begin:                  return "pmp$task_begin";
    case pmk_pop_all_stack_frames:        return "pmp$pop_all_stack_frames";
    case pmk_execute:                     return "pmk$execute";
    case pmk_exit:                        return "pmp$exit";
    case pmk_abort:                       return "pmp$abort";
    case pmk_await_task_termination:      return "pmp$await_task_termination";
    case pmk_establish_condition_handler: return "pmp$establish_condition_handler";
    case pmk_disestablish_cond_handler:   return "pmp$disestablish_cond_handler";
    case pmk_cause_condition:             return "pmp$cause_condition";
    case pmk_get_time:                    return "pmp$get_time";
    case pmk_log_message:                 return "pmp$log_message";
    case pmk_log_ascii:                   return "pmp$log_ascii";
    case pmk_log:                         return "pmp$log";
    case pmk_wait:                        return "pmp$wait";
    case pmk_long_term_wait:              return "pmp$long_term_wait";
    case pmk_enable_system_conditions:    return "pmp$enable_system_conditions";
    case pmk_establish_ch_in_block:       return "pmp$establish_ch_in_block";
    case pmk_get_binary_processor_id:     return "pmp$get_binary_processor_id";
    case pmk_load_from_library:           return "pmp$load_from_library";
    case pmk_validate_previous_save_area: return "pmp$validate_previous_save_area";
    case pmk_push_task_debug_mode:        return "pmp$push_task_debug_mode";
    case pmk_set_task_debug_mode:         return "pmp$set_task_debug_mode";
    case pmk_establish_debug_cff:         return "pmp$establish_debug_cff";
    case pmk_change_job_library_list:     return "pmp$change_job_library_list";
    case pmk_pop_inhibit_termination:     return "pmp$pop_inhibit_termination";
    case pmk_push_inhibit_termination:    return "pmp$push_inhibit_termination";
    case pmk_establish_ch_outside_block:  return "pmp$establish_ch_outside_block";
    case tmk_switch_task:                 return "tmp$switch_task";
    case tmk_send_monitor_fault:          return "tmp$send_monitor_fault";
    case tmk_process_task_mcr_fault:      return "tmp$process_task_mcr_fault";
    case tmk_set_monitor_flag:            return "tmp$set_monitor_flag";
    case iok_queue_request:               return "iop$queue_request";
    case iok_io_completions:              return "iop$process_io_completions";
    case iok_allocate_image_request:      return "iop$allocate_image_request";
    case iok_queue_image_request:         return "iop$queue_image_request";
    case jmk_get_job_status:              return "jmp$get_job_status";
    case jmk_idle_system:                 return "jmp$idle_system";
    case jmk_job_exists:                  return "jmp$job_exists";
    case fmk_return_file:                 return "fmp$return_file";
    case fsk_open_file:                   return "fsp$open_file";
    case mtk_job_entry_exit:              return "mtp$job_entry_exit";
    case mtk_170_entry_exit:              return "mtp$170_entry_exit";
    case mtk_monitor_mode_trap:           return "mtp$monitor_mode_trap";
    case mtk_job_mode_trap:               return "mtp$job_mode_trap";
    default:
        sprintf(buf, "%u", kpt);
        return buf;
        }
    }

#endif

#endif

/*---------------------------  End Of File  ------------------------------*/
