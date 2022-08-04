#ifndef PROTO_H
#define PROTO_H

/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**
**  Name: func.h
**
**  Description:
**      This file defines external function prototypes and variables.
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

#include <time.h>
#include "types.h"

/*
**  --------------------
**  Function Prototypes.
**  --------------------
**  These declarations are sufficiently numerous that they should
**  be maintained in alpha-order by Module name unless there are
**  reasons to the contrary.
*/

/*
**  channel.c
*/
void channelInit(u8 count);
void channelTerminate(void);
DevSlot * channelFindDevice(u8 channelNo, u8 devType);
DevSlot * channelAttach(u8 channelNo, u8 eqNo, u8 devType);
void channelFunction(PpWord funcCode);
void channelActivate(void);
void channelDisconnect(void);
void channelIo(void);
void channelCheckIfActive(void);
void channelCheckIfFull(void);
void channelOut(void);
void channelIn(void);
void channelSetFull(void);
void channelSetEmpty(void);
void channelStep(void);
void channelDisplayContext();

/*
**  console.c
*/
void consoleInit(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);

/*
**  cpu.c
*/
void cpuInit(char *model, u32 memory, u32 emBanks, ExtMemory emType);
void cpuTerminate(void);
u32 cpuGetP(void);
bool cpuExchangeJump(u32 addr);
void cpuStep(void);
bool cpuEcsFlagRegister(u32 ecsAddress);
bool cpuDdpTransfer(u32 ecsAddress, CpWord *data, bool writeToEcs);
void cpuPpReadMem(u32 address, CpWord *data);
void cpuPpWriteMem(u32 address, CpWord data);

/*
**  cr405.c
*/
void cr405Init(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
void cr405GetNextDeck(char *fname, int channelNo, int equipmentNo, char *params);
void cr405PostProcess(char *fname, int channelNo, int equipmentNo, char *params);
void cr405LoadCards(char *fname, int channelNo, int equipmentNo, char *params);
void cr405ShowStatus();

/*
**  cp3446.c
*/
void cp3446Init(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
void cp3446RemoveCards(char *params);
void cp3446ShowStatus();

/*
**  cr3447.c
*/
void cr3447Init(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
void cr3447GetNextDeck(char *fname, int channelNo, int equipmentNo, char *params);
void cr3447PostProcess(char *fname, int channelNo, int equipmentNo, char *params);
void cr3447LoadCards(char *fname, int channelNo, int equipmentNo, char *params);
void cr3447ShowStatus();

/*
**  cray_station.c
*/
void csFeiInit(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);

/*
**  dd6603.c
*/
void dd6603Init(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
void dd6603ShowDiskStatus();

/*
**  dd8xx.c
*/
void dd844Init_2(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
void dd844Init_4(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
void dd885Init_1(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
void dd8xxShowDiskStatus();


/*
**  dd885_42.c
*/
void dd885_42Init(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
void dd885_42ShowDiskStatus();

/*
**  dcc6681.c
*/
void dcc6681Terminate(DevSlot *dp);
void dcc6681Interrupt(bool status);

/*
**  ddp.c
*/
void ddpInit(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);

/*
**  deadstart.c
*/
void deadStart(void);

/*
**  dsa311.c
*/
void dsa311Init(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);

/*
**  dump.c
*/
void dumpInit(void);
void dumpTerminate(void);
void dumpAll(void);
void dumpCpu(void);
void dumpPpu(u8 pp);
void dumpDisassemblePpu(u8 pp);
void dumpRunningPpu(u8 pp);
void dumpRunningCpu(void);

/*
**  float.c
*/
CpWord floatAdd(CpWord v1, CpWord v2, bool doRound, bool doDouble);
CpWord floatMultiply(CpWord v1, CpWord v2, bool doRound, bool doDouble);
CpWord floatDivide(CpWord v1, CpWord v2, bool doRound);

/*
**  fsmon.c
*/
bool fsCreateThread(fswContext *parms);

/*
**  init.c
*/
void initStartup(char *config, char *configFile);
u32 initConvertEndian(u32 value);
char * initGetNextLine(void);
int initOpenHelpersSection(void);
int initOpenOperatorSection(void);

/*
**  interlock_channel.c
*/
void ilrInit(u8 registerSize);

/*
**  lp1612.c
*/
void lp1612Init(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
void lp1612RemovePaper(char *params);
void lp1612ShowStatus();

/*
**  lp3000.c
*/
void lp501Init(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceParams);
void lp512Init(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceParams);
void lp3000RemovePaper(char *params);
void lp3000ShowStatus();

/*
**  log.c
*/
void logInit(void);
void logError(char *file, int line, char *fmt, ...);

/*
**  maintenance_channel.c
*/
void mchInit(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);

/*
**  mdi.c
*/
void mdiInit(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);

/*
**  msufrend.c
*/
void msuFrendInit(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
void msuFrendTalkToFrend(void);
void msuFrendSendInterruptToFrend(void);

/*
**  mt362x.c
*/
void mt362xInit_7(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
void mt362xInit_9(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
void mt362xLoadTape(char *params);
void mt362xUnloadTape(char *params);
void mt362xShowTapeStatus();

/*
**  mt607.c
*/
void mt607Init(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);

/*
**  mt669.c
*/
void mt669Init(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
void mt669Terminate(DevSlot *dp);
void mt669LoadTape(char *params);
void mt669UnloadTape(char *params);
void mt669ShowTapeStatus();

/*
**  mt679.c
*/
void mt679Init(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
void mt679Terminate(DevSlot *dp);
void mt679LoadTape(char *params);
void mt679UnloadTape(char *params);
void mt679ShowTapeStatus();

/*
**  mt5744.c
*/
void mt5744Init(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
void mt5744ShowTapeStatus();

/*
**  mux6676.c
*/
void mux6671Init(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
void mux6676Init(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);

/*
**  niu.c
*/
typedef void niuProcessOutput(int, u32);
void niuInInit(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
void niuOutInit(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
bool niuPresent(void);
void niuLocalKey(u16 key, int stat);
void niuSetOutputHandler(niuProcessOutput *h, int stat);


/*
**  npu.c
*/
void npuInit(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
int npuBipBufCount(void);

/*
**  operator.c
*/
void opCmdLoadCards(bool help, char *cmdParams);
void opDisplay(char *msg);
void opInit(void);
void opRequest(void);

/*
**  pci_channel_{win32,linux}.c
*/
void pciInit(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);

/*
**  pci_console_linux.c
*/
void pciConsoleInit(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);

/*
**  pp.c
*/
void ppInit(u8 count);
void ppTerminate(void);
void ppStep(void);

/*
**  rtc.c
*/
void rtcInit(u8 increment, u32 setMHz);
void rtcTick(void);
void rtcStartTimer(void);
double rtcStopTimer(void);
void rtcReadUsCounter(void);

/*
**  scr_channel.c
*/
void scrInit(u8 channelNo);

/*
**  shift.c
*/
CpWord shiftLeftCircular(CpWord data, u32 count);
CpWord shiftRightArithmetic(CpWord data, u32 count);
CpWord shiftPack(CpWord coeff, u32 expo);
CpWord shiftUnpack(CpWord number, u32 *expo);
CpWord shiftNormalize(CpWord number, u32 *shift, bool round);
CpWord shiftMask(u8 count);

/*
**  string.c
*/
char * dtStrLwr(char *str);

/*
**  time.c
*/
u64 getMilliseconds(void);
time_t getSeconds(void);
void sleepMsec(u32 msec);
void sleepUsec(u64 usec);

/*
**  tpmux.c
*/
void tpMuxInit(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);

/*
**  trace.c
*/
void traceInit(void);
void traceTerminate(void);
void traceSequence(void);
void traceRegisters(void);
void traceOpcode(void);
u8 traceDisassembleOpcode(char *str, PpWord *pm);
void traceChannelFunction(PpWord funcCode);
void tracePrint(char *str);
void traceCpuPrint(char *str);
void traceChannel(u8 ch);
void traceEnd(void);
void traceCpu(u32 p, u8 opFm, u8 opI, u8 opJ, u8 opK, u32 opAddress);
void traceExchange(CpuContext *cc, u32 addr, char *title);

/*
**  window_{win32,x11}.c
*/
void windowInit(void);
void windowSetFont(u8 font);
void windowSetX(u16 x);
void windowSetY(u16 y);
void windowQueue(u8 ch);
void windowUpdate(void);
void windowGetChar(void);
void windowTerminate(void);

/*
**  -----------------
**  Global variables.
**  -----------------
**  This list is long enough that it should be kept in
**  alpha-order unless there is a good reason not to.
*/

extern DevSlot             *active3000Device;
extern ChSlot              *activeChannel;
extern DevSlot             *activeDevice;
extern PpSlot              *activePpu;
extern const i8            altKeyToPlato[128];
extern const u16           asciiTo026[256];
extern const u16           asciiTo029[256];
extern const u8            asciiToBcd[256];
extern const u8            asciiToCdc[256];
extern const u8            asciiToConsole[256];
extern const u8            asciiToEbcdic[256];
extern const int           asciiToPlatoString[256];
extern const i8            asciiToPlato[128];
extern const char          bcdToAscii[64];
extern bool                bigEndian;
extern const char          cdcToAscii[64];
extern ChSlot              *channel;
extern u8                  channelCount;
extern const char          consoleToAscii[64];
extern CpWord              *cpMem;
extern CpuContext          cpu;
extern u32                 cpuMaxMemory;
extern bool                cpuStopped;
extern u32                 cycles;
extern u8                  deviceCount;
extern DevDesc             deviceDesc[];
extern char                displayName[];
extern const u8            ebcdicToAscii[256];
extern bool                emulationActive;
extern const char          extBcdToAscii[64];
extern u32                 extMaxMemory;
extern CpWord              *extMem;
extern ModelFeatures       features;
extern ModelType           modelType;
extern u16                 mux6676TelnetConns;
extern u16                 mux6676TelnetPort;
extern u8                  npuLipTrunkCount;
extern u16                 npuLipTrunkPort;
extern char                npuNetHostID[];
extern u32                 npuNetHostIP;
extern u16                 npuNetTcpConns;
extern u16                 npuNetTelnetPort;
extern u8                  npuSvmCouplerNode;
extern u8                  npuSvmNpuNode;
extern volatile bool       opActive;
extern char                opKeyIn;
extern long                opKeyInterval;
extern char                persistDir[];
extern u16                 platoConns;
extern u16                 platoPort;
extern const unsigned char platoStringToAscii[4][65];
extern char                ppKeyIn;
extern PpSlot              *ppu;
extern u8                  ppuCount;
extern u32                 readerScanSecs;
extern u32                 rtcClock;
extern u32                 traceMask;
extern u32                 traceSequenceNo;

#ifdef IdleThrottle
/* NOS Idle Loop throttle */
extern bool NOSIdle;
extern u32  idletime;
extern u32  idletrigger;
#endif

#endif /* PROTO_H */
/*---------------------------  End Of File  ------------------------------*/
