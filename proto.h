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
*/

/*
**  init.c
*/
void initStartup(char *);
u32 initConvertEndian(u32 value);
char *initGetNextLine(void);
int initOpenHelpersSection(void);
int initOpenOperatorSection(void);

/*
**  deadstart.c
*/
void deadStart(void);

/*
**  rtc.c
*/
void rtcInit(u8 increment, u32 setMHz);
void rtcTick(void);
void rtcStartTimer(void);
double rtcStopTimer(void);
void rtcReadUsCounter(void);

/*
**  channel.c
*/
void channelInit(u8 count);
void channelTerminate(void);
DevSlot *channelFindDevice(u8 channelNo, u8 devType);
DevSlot *channelAttach(u8 channelNo, u8 eqNo, u8 devType);
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

/*
**  pp.c
*/
void ppInit(u8 count);
void ppTerminate(void);
void ppStep(void);

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
**  mt362x.c
*/
void mt362xInit_7(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
void mt362xInit_9(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
void mt362xLoadTape(char *params, FILE *out);
void mt362xUnloadTape(char *params, FILE *out);
void mt362xShowTapeStatus(FILE *out);

/*
**  mt607.c
*/
void mt607Init(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);

/*
**  mt669.c
*/
void mt669Init(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
void mt669Terminate(DevSlot *dp);
void mt669LoadTape(char *params, FILE *out);
void mt669UnloadTape(char *params, FILE *out);
void mt669ShowTapeStatus(FILE *out);

/*
**  mt679.c
*/
void mt679Init(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
void mt679Terminate(DevSlot *dp);
void mt679LoadTape(char *params, FILE *out);
void mt679UnloadTape(char *params, FILE *out);
void mt679ShowTapeStatus(FILE *out);

/*
**  mt5744.c
*/
void mt5744Init(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
void mt5744ShowTapeStatus(FILE *out);

/*
**  cr405.c
*/
void cr405Init(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
void cr405LoadCards(char *fname, int channelNo, int equipmentNo, FILE *out);

/*
**  cp3446.c
*/
void cp3446Init(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
void cp3446RemoveCards(char *params, FILE *out);

/*
**  cr3447.c
*/
void cr3447Init(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
void cr3447LoadCards(char *fname, int channelNo, int equipmentNo, FILE *out);

/*
**  lp1612.c
*/
void lp1612Init(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
void lp1612RemovePaper(char *params, FILE *out);

/*
**  lp3000.c
*/
void lp501Init(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
void lp512Init(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
void lp3000RemovePaper(char *params, FILE *out);

/*
**  console.c
*/
void consoleInit(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);

/*
**  dd6603.c
*/
void dd6603Init(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);

/*
**  dd8xx.c
*/
void dd844Init_2(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
void dd844Init_4(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
void dd885Init_1(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);

/*
**  dd885_42.c
*/
void dd885_42Init(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);

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
**  dsa311.c
*/
void dsa311Init(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);

/*
**  mux6676.c
*/
void mux6671Init(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
void mux6676Init(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);

/*
**  npu.c
*/
void npuInit(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
int npuBipBufCount(void);

/*
**  mdi.c
*/
void mdiInit(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);

/*
**  cray_station.c
*/
void csFeiInit(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);

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
**  msufrend.c
*/
void msuFrendInit(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
void msuFrendTalkToFrend(void);
void msuFrendSendInterruptToFrend(void);

/*
**  pci_channel_{win32,linux}.c
*/
void pciInit(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);

/*
**  pci_console_linux.c
*/
void pciConsoleInit(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);

/*
**  time.c
*/
u64 getMilliseconds(void);
time_t getSeconds(void);

/*
**  tpmux.c
*/
void tpMuxInit(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);

/*
**  maintenance_channel.c
*/
void mchInit(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);

/*
**  scr_channel.c
*/
void scrInit(u8 channelNo);

/*
**  interlock_channel.c
*/
void ilrInit(u8 registerSize);

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
**  shift.c
*/
CpWord shiftLeftCircular(CpWord data, u32 count);
CpWord shiftRightArithmetic(CpWord data, u32 count);
CpWord shiftPack(CpWord coeff, u32 expo);
CpWord shiftUnpack(CpWord number, u32 *expo);
CpWord shiftNormalize(CpWord number, u32 *shift, bool round);
CpWord shiftMask(u8 count);

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
**  operator.c
*/
void opInit(void);
void opRequest(void);

/*
**  log.c
*/
void logInit(void);
void logError(char *file, int line, char *fmt, ...);

/*
**  -----------------
**  Global variables.
**  -----------------
*/

extern bool emulationActive;
extern PpSlot *ppu;
extern ChSlot *channel;
extern u8 ppuCount;
extern u8 channelCount;
extern PpSlot *activePpu;
extern ChSlot *activeChannel;
extern DevSlot *activeDevice;
extern DevSlot *active3000Device;
extern CpuContext cpu;
extern bool cpuStopped;
extern CpWord *cpMem;
extern u32 cpuMaxMemory;
extern u32 extMaxMemory;
extern CpWord *extMem;
extern char opKeyIn;
extern long opKeyInterval;
extern char ppKeyIn;
extern const u8 asciiToCdc[256];
extern const char cdcToAscii[64];
extern const u8 asciiToConsole[256];
extern const char consoleToAscii[64];
extern const u16 asciiTo026[256];
extern const u16 asciiTo029[256];
extern const u8  asciiToBcd[256];
extern const u8  asciiToEbcdic[256];
extern const u8  ebcdicToAscii[256];
extern const char bcdToAscii[64];
extern const char extBcdToAscii[64];
extern const i8 asciiToPlato[128];
extern const i8 altKeyToPlato[128];
extern u32 traceMask;
extern u32 traceSequenceNo;
extern DevDesc deviceDesc[];
extern u8 deviceCount;
extern bool bigEndian;
extern volatile bool opActive;
extern u16 mux6676TelnetPort;
extern u16 mux6676TelnetConns;
extern u16 platoPort;
extern u16 platoConns;
extern u32 cycles;
extern u32 rtcClock;
extern ModelFeatures features;
extern ModelType modelType;
extern char persistDir[];
extern u16 npuNetTelnetPort;
extern u16 npuNetTcpConns;
extern u16 npuLipTrunkPort;
extern u8 npuLipTrunkCount;
extern char npuNetHostID[];
extern u32 npuNetHostIP;
extern u8 npuSvmCouplerNode;
extern u8 npuSvmNpuNode;

#endif /* PROTO_H */
/*---------------------------  End Of File  ------------------------------*/

