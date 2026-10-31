#ifndef CCI_H
#define CCI_H

/*--------------------------------------------------------------------------
**
**  Copyright (c) 2024 Joachim Siebold
**
**  Name: cci.h
**
**  Description:
**      This file defines CCI constants, types, variables, macros and
**      function prototypes.
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

#include "npu.h"

/*
**  -------------
**  CCI Constants
**  -------------
*/
#define BlkOffP                 6
#define BlkOffSP                7
#define BlkOffLt                8
#define BlkOffTt                9

#define CciWaitForTcbTimeout    5

/*
**  -------------------
**  CCI Macro Functions
**  -------------------
*/

/*
**  --------------------
**  CCI Type Definitions
**  --------------------
*/

/*
** CCI TIP types
*/
typedef enum
    {
    cciTIP=1,
    cciMODE4,
    cciHASP,
    cciBSC
    } CciTipType;

/*
**  -------------------------------
**  CCI Function Prototypes
**  -------------------------------
*/

/*
**  cci_hip.c
*/
bool cciHipIsReady(NetDevParam *netDevice);

/*
** cci_svm.c
*/
void cciSvmInit(void);
void cciSvmReset(void);
void cciSvmNpuInitResponse(void);
void cciSvmProcessBuffer(NpuBuffer *bp);
bool cciSvmConnectTerminal(Pcb *pp);
void cciSvmSendDiscRequest(Tcb *tp);
bool cciSvmIsReady(NetDevParam *netDevice);

/*
** cci_tip.c
*/
void cciTipInit(void);
void cciTipReset(void);
void cciTipProcessBuffer(NpuBuffer *bp, int priority);
void cciTipNotifySent(Tcb *tp, u8 blockSeqNo);
void cciTipInputReset(Tcb *tp);
void cciTipSendUserBreak(Tcb *tp, u8 bt);
void cciTipDiscardOutputQ(Tcb *tp);
void cciTipConfigureTerminal(Tcb *tp);
Tcb *cciTipFindTcbForCN(u8 cn);
void cciTipSendBlock(Tcb *tp, int len);
void cciTipSendMsg(Tcb *tp, int len);

/*
** cci_async.c
*/
void cciAsyncProcessDownlineData(Tcb *tp, NpuBuffer *bp, bool last);
void cciAsyncProcessUplineNormal(Tcb *tp);

#endif /* CCI_H */
