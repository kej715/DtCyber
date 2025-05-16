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

#include <memory.h>
#include <time.h>
#include <sys/types.h>
#include "const.h"
#include "types.h"

#if defined(__FreeBSD__)
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#if defined(_WIN32)
#include <winsock.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

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
**  cdcnet.c
*/
void cdcnetShowStatus(void);

/*
**  console.c
*/
void consoleCloseRemote(void);
void consoleCloseWindow(void);
void consoleInit(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
bool consoleIsRemoteActive(void);
void consoleOpenWindow(void);
void consoleShowStatus(void);

/*
**  cpu.c
*/
void cpuAcquireExchangeMutex(void);
void cpuAcquireMemoryMutex(void);
bool cpuDdpTransfer(u32 ecsAddress, CpWord *data, bool writeToEcs);
bool cpuEcsFlagRegister(u32 ecsAddress);
u32  cpuGetP(u8 cpuNum);
void cpuInit(char *model, u32 memory, u32 emBanks, ExtMemory emType);
void cpuPpReadMem(u32 address, CpWord *data);
void cpuPpWriteMem(u32 address, CpWord data);
void cpuReleaseExchangeMutex(void);
void cpuReleaseMemoryMutex(void);
void cpuStep(Cpu170Context *activeCpu);
void cpuTerminate(void);
void cpuVoidIwStack(Cpu170Context *activeCpu, u32 branchAddr);

/*
**  cpu180.c
*/
void cpu180CheckConditions(Cpu180Context *ctx);
void cpu180Init(char *model);
void cpu180Load180Xp(Cpu180Context *ctx, u32 xpa);
void cpu180PpReadMem(u32 address, CpWord *data);
void cpu180PpWriteMem(u32 address, CpWord data);
bool cpu180PvaToRma(Cpu180Context *ctx, u64 pva, Cpu180AccessMode access, u32 *rma, MonitorCondition *cond);
void cpu180SetMonitorCondition(Cpu180Context *ctx, MonitorCondition cond);
void cpu180SetUserCondition(Cpu180Context *ctx, UserCondition cond);
void cpu180Step(Cpu180Context *activeCpu);
void cpu180Store170Xp(Cpu180Context *ctx, u32 xpa);
void cpu180UpdateIntervalTimers(u64 delta);
void cpu180UpdatePageSize(Cpu180Context *ctx);

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
void csFeiShowStatus();

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
void dd8xxLoadDisk(char *params);
void dd8xxUnloadDisk(char *params);
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
void dsa311ShowStatus();

/*
**  dump.c
*/
void dumpInit(void);
void dumpTerminate(void);
void dumpAll(void);
void dumpCpu(void);
void dumpPpu(u8 pp, PpWord first, PpWord limit);
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
void  initStartup(char *config, char *configFile);
u32   initConvertEndian(u32 value);
char *initGetNextLine(int *lineNo);
int   initOpenHelpersSection(void);
int   initOpenOperatorSection(void);

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
void logDtError(char *file, int line, char *fmt, ...);
void logError(char *file, int line, char *fmt, ...);
void logInit(void);


/*
**  main.c
*/
int  runHelper(char* command);
void startHelpers(void);
void stopHelpers(void);

/*
**  maintenance_channel.c
*/
void mchCheckTimeout(void);
u64 mchGetCpRegister(Cpu180Context *ctx, u8 reg);
void mchInit(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
void mchSetCpRegister(Cpu180Context *ctx, u8 reg, u64 word);
void mchSetOsBoundsFault(PpSlot *pp, u32 address, u32 boundary);

/*
**  mdi.c
*/
void mdiInit(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);

/*
**  msufrend.c
*/
void msufrendInit(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
void msufrendShowStatus();

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
void mux6676ShowStatus();

/*
** cci_hip.c
*/
void cciHipTerminate(DevSlot *dp);
void cciInit(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);

/*
**  net_util.c
*/
#if defined(_WIN32)
SOCKET netAcceptConnection(SOCKET sd);
void   netCloseConnection(SOCKET sd);
SOCKET netCreateListener(int port);
SOCKET netCreateSocket(int port, bool isReuse);
int    netGetErrorStatus(SOCKET sd);
char  *netGetLocalTcpAddress(SOCKET sd);
char  *netGetPeerTcpAddress(SOCKET sd);
SOCKET netInitiateConnection(struct sockaddr *sap);
#else
int    netAcceptConnection(int sd);
void   netCloseConnection(int sd);
int    netCreateListener(int port);
int    netCreateSocket(int port, bool isReuse);
int    netGetErrorStatus(int sd);
char  *netGetLocalTcpAddress(int sd);
char  *netGetPeerTcpAddress(int sd);
int    netInitiateConnection(struct sockaddr *sap);
#endif

/*
**  niu.c
*/
typedef void niuProcessOutput(int, u32);
void niuInInit(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
void niuOutInit(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
bool niuPresent(void);
void niuLocalKey(u16 key, int stat);
void niuSetOutputHandler(niuProcessOutput *h, int stat);
void niuShowStatus();

/*
**  npu.c
*/
void npuInit(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName);
int npuBipBufCount(void);
bool npuBipIsBusy(void);
void npuNetShowStatus();

/*
**  operator.c
*/
void opCmdLoadCards(bool help, char *cmdParams);
void opDisplay(char *msg);
void opInit(void);
bool opIsConsoleInput(void);
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
void tpMuxShowStatus();

/*
**  trace.c
*/
void traceChannel(u8 ch);
void traceChannelFunction(PpWord funcCode);
void traceChannelIo(u8 ch);
void traceCmWord(CpWord data);
void traceCmWord64(CpWord data);
void traceCpu(Cpu170Context *cpu, u32 p, u8 opFm, u8 opI, u8 opJ, u8 opK, u32 opAddress);
void traceCpu180(Cpu180Context *cpu, u64 p, u8 opFm, u8 opI, u8 opJ, u8 opK, u16 opD, u16 opQ);
void traceCpuPrint(Cpu170Context *cpu, char *str);
u8 traceDisassembleOpcode(char *str, PpWord *pm);
void traceEnd(void);
void traceExchange(Cpu170Context *cpu, u32 addr, char *title);
void traceExchange180(Cpu180Context *cpu, u32 addr, char *title);
void traceInit(void);
void traceMonitorCondition(Cpu180Context *cpu, MonitorCondition cond);
void traceOpcode(void);
void tracePageInfo(Cpu180Context *cpu, u16 hash, u32 pageNum, u32 pageOffset, u32 pageTableIdx, u64 spid);
void tracePrint(char *str);
void tracePte(Cpu180Context *cpu, u64 pte);
void tracePva(Cpu180Context *cpu, u64 pva);
void traceRegisters(bool isPost);
void traceRma(Cpu180Context *cpu, u32 rma);
void traceSde(Cpu180Context *cpu, u64 sde);
void traceSequence(void);
void traceStack(FILE *fp);
void traceTerminate(void);
char *traceTranslateAction(ConditionAction action);
void traceTrapFrame170(Cpu180Context *cpu, u32 rma);
void traceTrapFrame180(Cpu180Context *cpu, u32 rma);
void traceTrapPointer(Cpu180Context *cpu);
void traceUserCondition(Cpu180Context *cpu, UserCondition cond);
void traceVmRegisters(Cpu180Context *cpu);

/*
**  window_{win32,x11}.c
*/
void windowInit(void);
void windowSetFont(u8 font);
void windowSetX(u16 x);
void windowSetY(u16 y);
void windowQueue(u8 ch);
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
extern bool                cc545Enabled;
extern const char          cdcToAscii[64];
extern ChSlot              *channel;
extern u8                  channelCount;
#ifdef WIN32
extern long                colorBG;                         // Console
extern long                colorFG;                         // Console
#else
extern char                colorBG[32];                     // Console
extern char                colorFG[32];                     // Console
#endif
extern const char          consoleToAscii[64];
extern CpWord              *cpMem;
extern Cpu170Context       *cpus170;
extern Cpu180Context       *cpus180;
extern int                 cpuCount;
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
extern ExtMemory           extMemType;
extern ModelFeatures       features;
extern long                fontHeightLarge;                 // Console
extern long                fontHeightMedium;                // Console
extern long                fontHeightSmall;                 // Console
extern long                fontLarge;                       // Console
extern long                fontMedium;                      // Console
extern long                fontSmall;                       // Console
extern char                fontName[];                      // Console
extern long                heightPX;                        // Console
extern bool                isCyber180;
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
extern NpuSoftware         npuSw;
extern u8                  npuSvmNpuNode;
extern char                *npuSvmTermStates[];
extern volatile bool       opActive;
extern char                opKeyIn;
extern long                opKeyInterval;
extern volatile bool       opPaused;
extern char                persistDir[];
extern u16                 platoConns;
extern u16                 platoPort;
extern const unsigned char platoStringToAscii[4][65];
extern char                ppKeyIn;
extern PpSlot              *ppu;
extern u8                  ppuCount;
extern u32                 ppuOsBoundary;
extern u32                 readerScanSecs;
extern u32                 rtcClock;
extern u64                 rtcClockDelta;
extern bool                rtcClockIsCurrent;
extern long                scaleX;                          // Console
extern long                scaleY;                          // Console
extern long                timerRate;                       // Console
extern bool                tpMuxEnabled;
extern u32                 traceMask;
extern u32                 traceSequenceNo;
extern long                widthPX;                         // Console


/* Idle Loop throttle */
extern bool idle;
extern bool (*idleDetector)(Cpu170Context *ctx);
extern u32  idleNetBufs;
extern u32  idleTime;
extern u32  idleTrigger;
extern char ipAddress[];
extern char networkInterface[];
extern char networkInterfaceMgr[];
extern char osType[];

bool idleCheckBusy();
bool idleDetectorNone(Cpu170Context *ctx);
bool idleDetectorCOS(Cpu170Context *ctx);   /* COS */
bool idleDetectorMACE(Cpu170Context *ctx);  /* KRONOS1 or MACE, possibly SCOPE too) */
bool idleDetectorNOS(Cpu170Context *ctx);   /* KRONOS2.1 - NOS 2.8.7 */
bool idleDetectorNOSBE(Cpu170Context *ctx); /* NOS/BE (only tested with TUB) */
void idleThrottle(Cpu170Context *ctx);

#endif /* PROTO_H */
/*---------------------------  End Of File  ------------------------------*/
