/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**
**  Name: npu_svm.c
**
**  Description:
**      Perform emulation of the Service Message (SVM) subsystem in an NPU
**      consisting of a CDC 2550 HCP running CCP.
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

#define DEBUG 0

/*
**  -------------
**  Include Files
**  -------------
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#if defined(__FreeBSD__)
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif 

#include "const.h"
#include "types.h"
#include "proto.h"
#include "npu.h"

/*
**  -----------------
**  Private Constants
**  -----------------
*/

/*
**  Primary Service Message function codes.
*/
#define PfcREG               0x1  // logical link regulation
#define PfcICN               0x2  // initiate connection
#define PfcTCN               0x3  // terminate connection
#define PfcCHC               0x4  // change terminal characteristics
#define PfcNPU               0xA  // initialize npu
#define PfcSUP               0xE  // initiate supervision
#define PfcCNF               0xF  // configure terminal
#define PfcENB               0x10 // enable command(s)
#define PfcDIB               0x11 // disable command(s)
#define PfcNPS               0x12 // npu status request
#define PfcLLS               0x13 // ll status request
#define PfcLIS               0x14 // line status request
#define PfcTES               0x15 // term status request
#define PfcTRS               0x16 // trunk status request
#define PfcCPS               0x17 // coupler status request
#define PfcVCS               0x18 // svc status request
#define PfcSTU               0x19 // unsolicated statuses
#define PfcSTI               0x1A // statistics
#define PfcMSG               0x1B // message(s)
#define PfcLOG               0x1C // error log entry
#define PfcALM               0x1D // operator alarm
#define PfcNPI               0x1E // reload npu
#define PfcCDI               0x1F // count(s)
#define PfcOLD               0x20 // on-line diagnostics

/*
**  Secondary Service Message function codes.
*/
#define SfcNP                0x0  // npu
#define SfcLL                0x1  // logical link
#define SfcLI                0x2  // line
#define SfcTE                0x3  // terminal
#define SfcTR                0x4  // trunk
#define SfcCP                0x5  // coupler
#define SfcVC                0x6  // switched virtual circuit
#define SfcOP                0x7  // operator
#define SfcTA                0x8  // terminate connection
#define SfcAP                0x9  // outbound a-a connection
#define SfcIN                0xA  // initiate supervision
#define SfcDO                0xB  // dump option
#define SfcPB                0xC  // program block
#define SfcDT                0xD  // data
#define SfcTM                0xE  // terminate diagnostics
#define SfcLD                0xE  // load
#define SfcGO                0xF  // go
#define SfcER                0x10 // error(s)
#define SfcEX                0x11 // a to a connection
#define SfcNQ                0x12 // sfc for *pbperform* sti
#define SfcNE                0x13 // nip block protocol error
#define SfcPE                0x14 // pip block protocol error
#define SfcRC                0x11 // reconfigure terminal

/*
**  Regulation level change bit masks.
*/
#define RegLvlBuffers        0x03
#define RegLvlCsAvailable    0x04
#define RegLvlNsAvailable    0x08

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

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static Tcb *npuSvmFindOwningConsole(Tcb *tp);
static bool npuSvmRequestTerminalConfig(Pcb *pp);
static Tcb *npuSvmProcessTerminalConfig(u8 claPort, NpuBuffer *bp);
static bool npuSvmRequestTerminalConnection(Tcb *tp);

/*
**  ----------------
**  Public Variables
**  ----------------
*/
u8 npuSvmCouplerNode = 1;
u8 npuSvmNpuNode     = 2;

/*
**  Table of terminal connection state names, indexed by terminal
**  connection state value (TermConnState).
*/
char *npuSvmTermStates[] = {
    "Idle",
    "Request Connection",
    "Connected",
    "NPU Request Disconnect",
    "Host Request Disconnect"
};

extern void (*npuHipResetFunc)(void);

/*
**  -----------------
**  Private Variables
**  -----------------
*/
static u8 linkRegulation[] =
    {
    0,          // DN
    0,          // SN
    0,          // CN
    BtHTCMD,    // BT=CMD
    PfcREG,     // PFC
    SfcLL,      // SFC
    0x0F,       // NS=1, CS=1, Regulation level=3
    0, 0, 0, 0, // not used
    0, 0, 0,    // not used
    };

static u8 requestSupervision[] =
    {
    0,       // DN
    0,       // SN
    0,       // CN
    BtHTCMD, // BT=CMD
    PfcSUP,  // PFC
    SfcIN,   // SFC
    0,       // PS
    0,       // PL
    0,       // RI
    0, 0, 0, // not used
    3,       // CCP version
    1,       // ...
    0,       // CCP level
    0,       // ...
    0,       // CCP cycle or variant
    0,       // ...
    0,       // not used
    0, 0     // NCF version in NDL file (ignored)
    };

static u8 responseNpuStatus[] =
    {
    0,                  // DN
    0,                  // SN
    0,                  // CN
    BtHTCMD,            // BT=CMD
    PfcNPS,             // PFC
    SfcNP | SfcResp,    // SFC
    };

static u8 responseTerminateConnection[] =
    {
    0,                  // DN
    0,                  // SN
    0,                  // CN
    BtHTCMD,            // BT=CMD
    PfcTCN,             // PFC
    SfcTA | SfcResp,    // SFC
    0,                  // CN
    };

static u8 requestTerminateConnection[] =
    {
    0,                  // DN
    0,                  // SN
    0,                  // CN
    BtHTCMD,            // BT=CMD
    PfcTCN,             // PFC
    SfcTA,              // SFC
    0,                  // CN
    };

static u8 blockTerminateConnection[] =
    {
    0,                  // DN
    0,                  // SN
    0,                  // CN
    BtHTTERM,           // BT/BSN/PRIO
    };

static enum
    {
    StIdle = 0,
    StWaitSupervision,
    StReady,
    }
svmState = StIdle;

static char *svmStates[] = {
    "StIdle",
    "StWaitSupervision",
    "StReady"
};

static u8 oldRegLevel = 0;

/*
**  Table of functions that notify TIPs of terminal connection,
**  indexed by connection type.
*/
static void (*notifyTermConnect[])(Tcb *tp) =
    {
    npuAsyncNotifyTermConnect,     // ConnTypeRaw
    npuAsyncNotifyTermConnect,     // ConnTypePterm
    npuAsyncNotifyTermConnect,     // ConnTypeRs232
    npuAsyncNotifyTermConnect,     // ConnTypeTelnet
    npuHaspNotifyTermConnect,      // ConnTypeHasp
    npuHaspNotifyTermConnect,      // ConnTypeRevHasp
    npuNjeNotifyTermConnect,       // ConnTypeNje
    NULL                           // ConnTypeTrunk
    };

/*
**  Table of functions that notify TIPs of terminal disconnection,
**  indexed by connection type.
*/
static void (*notifyTermDisconnect[])(Tcb *tp) =
    {
    npuAsyncNotifyTermDisconnect,  // ConnTypeRaw
    npuAsyncNotifyTermDisconnect,  // ConnTypePterm
    npuAsyncNotifyTermDisconnect,  // ConnTypeRs232
    npuAsyncNotifyTermDisconnect,  // ConnTypeTelnet
    npuHaspNotifyTermDisconnect,   // ConnTypeHasp
    npuHaspNotifyTermDisconnect,   // ConnTypeRevHasp
    npuNjeNotifyTermDisconnect,    // ConnTypeNje
    NULL                           // ConnTypeTrunk
    };

#if DEBUG
static FILE *npuSvmLog = NULL;
#endif

/*
 **--------------------------------------------------------------------------
 **
 **  Public Functions
 **
 **--------------------------------------------------------------------------
 */

/*--------------------------------------------------------------------------
**  Purpose:        Initialize SVM.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuSvmInit(void)
    {
    /*
    **  Set initial state.
    */
    svmState = StIdle;

    linkRegulation[BlkOffDN] = npuSvmCouplerNode;
    linkRegulation[BlkOffSN] = npuSvmNpuNode;

    requestSupervision[BlkOffDN] = npuSvmCouplerNode;
    requestSupervision[BlkOffSN] = npuSvmNpuNode;

    responseNpuStatus[BlkOffDN] = npuSvmCouplerNode;
    responseNpuStatus[BlkOffSN] = npuSvmNpuNode;

    responseTerminateConnection[BlkOffDN] = npuSvmCouplerNode;
    responseTerminateConnection[BlkOffSN] = npuSvmNpuNode;

    requestTerminateConnection[BlkOffDN] = npuSvmCouplerNode;
    requestTerminateConnection[BlkOffSN] = npuSvmNpuNode;

    blockTerminateConnection[BlkOffDN] = npuSvmCouplerNode;
    blockTerminateConnection[BlkOffSN] = npuSvmNpuNode;

#if DEBUG
    npuSvmLog = fopen("svmlog.txt", "wt");
    if (npuSvmLog == NULL)
        {
        fprintf(stderr, "svmlog.txt - aborting\n");
        exit(1);
        }
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Reset SVM.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuSvmReset(void)
    {
    /*
    **  Set initial state.
    */
    svmState    = StIdle;
    oldRegLevel = 0;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Notify TIP of terminal disconnection.
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
void npuSvmNotifyTermDisconnect(Tcb *tp)
    {
#if DEBUG
    fprintf(npuSvmLog, "Notify TIP of %.7s disconnect in state %s\n", tp->termName, npuSvmTermStates[tp->state]);
#endif

    notifyTermDisconnect[tp->pcbp->ncbp->connType](tp);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Process regulation order word.
**
**  Parameters:     Name        Description.
**                  regLevel    regulation level
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuSvmNotifyHostRegulation(u8 regLevel)
    {
#if DEBUG
    fprintf(npuSvmLog, "Regulation level %02x, SVM state is %d\n", regLevel, svmState);
#endif

    if ((svmState == StIdle) || (regLevel != oldRegLevel))
        {
        oldRegLevel = regLevel;
        linkRegulation[BlkOffP3] = regLevel;
        npuBipRequestUplineCanned(linkRegulation, sizeof(linkRegulation));
        }

    if ((svmState == StIdle) && ((regLevel & RegLvlCsAvailable) != 0))
        {
        npuBipRequestUplineCanned(requestSupervision, sizeof(requestSupervision));
        svmState = StWaitSupervision;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Start host connection sequence.
**
**  Parameters:     Name        Description.
**                  pcbp        PCB pointer
**
**  Returns:        TRUE if sequence started, FALSE otherwise.
**
**------------------------------------------------------------------------*/
bool npuSvmConnectTerminal(Pcb *pcbp)
    {
#if DEBUG
    fprintf(npuSvmLog, "Connect terminal on CLA port %02x\n", pcbp->claPort);
#endif

    if (npuSvmRequestTerminalConfig(pcbp))
        {
        return (TRUE);
        }

    return (FALSE);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Process service message from host.
**
**  Parameters:     Name        Description.
**                  bp          buffer with service message.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuSvmProcessBuffer(NpuBuffer *bp)
    {
    u8  *block = bp->data;
    Tcb *tp;
    u8  cn;
    u8  claPort;

    /*
    **  Ensure there is at least a minimal service message.
    */
    if (bp->numBytes < BlkOffSfc + 1)
        {
        if ((bp->numBytes == BlkOffBTBSN + 1) && (block[BlkOffCN] != 0))
            {
            /*
            **  Exception to minimal service message:
            **  For some strange reason NAM sends an input acknowledgment
            **  as a SVM - forward it to the TIP which is better equipped
            **  to deal with this.
            */
            npuTipProcessBuffer(bp, 0);

            return;
            }

        /*
        **  Service message must be at least DN/SN/0/BSN/PFC/SFC.
        */
        npuLogMessage("(npu_svm) Short message in state %s", svmStates[svmState]);

        /*
        **  Release downline buffer and return.
        */
        npuBipBufRelease(bp);

        return;
        }

    /*
    **  Connection number for all service messages must be zero.
    */
    cn = block[BlkOffCN];
    if (cn != 0)
        {
        /*
        **  Connection number out of range.
        */
        npuLogMessage("(npu_svm) Connection number is %u but must be zero in SVM messages %02X/%02X",
                      cn, block[BlkOffPfc], block[BlkOffSfc]);

        /*
        **  Release downline buffer and return.
        */
        npuBipBufRelease(bp);

        return;
        }

    /*
    **  Extract the true connection number for those messages which carry it in P3.
    */
    switch (block[BlkOffPfc])
        {
    case PfcICN:
    case PfcTCN:
        if (bp->numBytes < BlkOffP3 + 1)
            {
            /*
            **  Message too short.
            */
            npuLogMessage("(npu_svm) Message %02X/%02X is too short and has no required P3", block[BlkOffPfc], block[BlkOffSfc]);
            npuBipBufRelease(bp);

            return;
            }

        cn = block[BlkOffP3];
        tp = npuTipFindTcbForCN(cn);
        if (tp == NULL)
            {
            /*
            **  Connection number out of range.
            */
            npuLogMessage("(npu_svm) Unexpected connection number %u in message %02X/%02X", cn, block[BlkOffPfc], block[BlkOffSfc]);
            npuBipBufRelease(bp);

            return;
            }
        break;
        }

    /*
    **  Process message.
    */
#if DEBUG
    fprintf(npuSvmLog, "Process downline message PFC %02x SFC %02x for CN %02x\n", block[BlkOffPfc], block[BlkOffSfc], block[BlkOffCN]);
#endif

    switch (block[BlkOffPfc])
        {
    case PfcSUP:
        if (block[BlkOffSfc] == (SfcIN | SfcResp))
            {
            if (svmState != StWaitSupervision)
                {
                npuLogMessage("(npu_svm) Unexpected Supervision Reply in state %s", svmStates[svmState]);
                break;
                }

            /*
            **  Host (CS) has agreed to supervise us, we are now ready to handle network
            **  connection attempts.
            */
            svmState = StReady;
            }
        else
            {
            npuLogMessage("(npu_svm) Unexpected message %02X/%02X in state %s", block[BlkOffPfc], block[BlkOffSfc], svmStates[svmState]);
            }
        break;

    case PfcNPS:
        if (block[BlkOffSfc] == SfcNP)
            {
            npuBipRequestUplineCanned(responseNpuStatus, sizeof(responseNpuStatus));
            }
        else
            {
            npuLogMessage("(npu_svm) Unexpected message %02X/%02X in state %s", block[BlkOffPfc], block[BlkOffSfc], svmStates[svmState]);
            }
        break;

    case PfcCNF:
        if (bp->numBytes < BlkOffP3 + 1)
            {
            /*
            **  Message too short.
            */
            npuLogMessage("(npu_svm) Message %02X/%02X is too short and has no required P3", block[BlkOffPfc], block[BlkOffSfc]);
            npuBipBufRelease(bp);

            return;
            }

        claPort = block[BlkOffP3];

        if (block[BlkOffSfc] == (SfcTE | SfcResp))
            {
            /*
            **  Process configuration reply and if all is well, issue
            **  terminal connection request.
            */
            tp = npuSvmProcessTerminalConfig(claPort, bp);
            if (tp != NULL)
                {
                if (npuSvmRequestTerminalConnection(tp))
                    {
                    tp->state = StTermRequestConnection;
                    npuNetSetMaxCN(tp->cn);
                    }
                else
                    {
                    npuNetCloseConnection(tp->pcbp);
                    tp->state = StTermIdle;
                    }
                }
            else
                {
                npuNetCloseConnection(npuNetFindPcb(claPort));
                }
            }
        else if (block[BlkOffSfc] == (SfcTE | SfcErr))
            {
            /*
            **  This port appears to be unknown to the host.
            */
            npuLogMessage("(npu_svm) Terminal on port %d not configured", claPort);
            npuNetCloseConnection(npuNetFindPcb(claPort));
            }
        else
            {
            npuLogMessage("(npu_svm) Unexpected message %02X/%02X with port %u",
                          block[BlkOffPfc], block[BlkOffSfc], claPort);
            npuNetCloseConnection(npuNetFindPcb(claPort));
            }
        break;

    case PfcICN:
        if (tp->state != StTermRequestConnection)
            {
            npuLogMessage("(npu_svm) Unexpected terminal connection reply in state %s", npuSvmTermStates[tp->state]);
            break;
            }

        if (block[BlkOffSfc] == (SfcTE | SfcResp))
            {
            tp->state = StTermConnected;
            notifyTermConnect[tp->pcbp->ncbp->connType](tp);
            npuNetConnected(tp);
            }
        else if (block[BlkOffSfc] == (SfcTE | SfcErr))
            {
            npuLogMessage("(npu_svm) Terminal Connection Rejected - reason 0x%02X", block[BlkOffP4]);
            tp->state = StTermIdle;
            npuNetDisconnected(tp);
            }
        else
            {
            npuLogMessage("(npu_svm) Unexpected message %02X/%02X with CN %d", block[BlkOffPfc], block[BlkOffSfc], cn);
            tp->state = StTermIdle;
            npuNetDisconnected(tp);
            }
        break;

    case PfcTCN:
        if (block[BlkOffSfc] == SfcTA)
            {
#if DEBUG
            fprintf(npuSvmLog, "TCN/TA/R received for %.7s in state %s\n", tp->termName, npuSvmTermStates[tp->state]);
#endif
            /*
            **  Host requests terminal disconnection.
            */
            if (tp->state == StTermConnected)
                {
                /*
                **  Send a TERM block. The host will reply with a TERM block.
                */
                npuSvmSendTermBlock(tp);
                tp->state = StTermHostRequestDisconnect;
                }
            else
                {
                npuSvmSendDiscReply(tp);
                }
            }
        else if (block[BlkOffSfc] == (SfcTA | SfcResp))
            {
#if DEBUG
            fprintf(npuSvmLog, "TCN/TA/N received for %.7s in state %s\n", tp->termName, npuSvmTermStates[tp->state]);
#endif
            if (tp->state == StTermNpuRequestDisconnect)
                {
                npuSvmNotifyTermDisconnect(tp);
                /*
                **  Reset connection state.
                */
                tp->state = StTermIdle;
                /*
                **  and disconnect the network.
                */
                npuNetDisconnected(tp);
                }
            else
                {
                npuLogMessage("(npu_svm) Unexpected TCN/TA/N for CN %d received in state %s", cn, npuSvmTermStates[tp->state]);
                }
            }
        else
            {
            npuLogMessage("(npu_svm) Unexpected message %02X/%02X with CN %d", block[BlkOffPfc], block[BlkOffSfc], cn);
            }
        break;

    case PfcNPI:
        npuLogMessage("(npu_svm) NPU reload request, SFC %02X", block[BlkOffSfc]);
        fprintf(stderr, "(npu_svm) NPU reload request, SFC %02X\n", block[BlkOffSfc]);
        (*npuHipResetFunc)();
        break;

    default:
        npuLogMessage("(npu_svm) Unrecognized message %02X/%02X", block[BlkOffPfc], block[BlkOffSfc]);
        fprintf(stderr, "(npu_svm) Unrecognized message %02X/%02X\n", block[BlkOffPfc], block[BlkOffSfc]);
        break;
        }

    /*
    **  Release downline buffer.
    */
    npuBipBufRelease(bp);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Process a TERM block received from host.
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
void npuSvmProcessTermBlock(Tcb *tp)
    {
#if DEBUG
    fprintf(npuSvmLog, "Process TERM block for %.7s in state %s\n", tp->termName, npuSvmTermStates[tp->state]);
#endif

    if (tp->state == StTermHostRequestDisconnect)
        {
        /*
        **  Host has echoed a TERM block sent previously, now send an TCN/TA/N to host.
        */
        npuSvmSendDiscReply(tp);
        npuSvmNotifyTermDisconnect(tp);
        tp->state = StTermIdle;
        /*
        **  and disconnect the network.
        */
        npuNetDisconnected(tp);
        }
    else if (tp->state == StTermNpuRequestDisconnect)
        {
        /*
        **  Host has sent TERM block in reponse to TCN/TA/R sent from NPU/MDI. Echo it.
        */
        npuSvmSendTermBlock(tp);
        }
#if DEBUG
    else
        {
        fputs("TERM block ignored.\n", npuSvmLog);
        }
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Send a TCN/TA/N to host.
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
void npuSvmSendDiscReply(Tcb *tp)
    {
#if DEBUG
    fprintf(npuSvmLog, "Send TCN/TA/N for %.7s in state %s\n", tp->termName, npuSvmTermStates[tp->state]);
#endif

    responseTerminateConnection[BlkOffP3] = tp->cn;
    npuBipRequestUplineCanned(responseTerminateConnection, sizeof(responseTerminateConnection));
    }

/*--------------------------------------------------------------------------
**  Purpose:        Send a TCN/TA/R (terminal disconnect) to host.
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
void npuSvmSendDiscRequest(Tcb *tp)
    {
#if DEBUG
    fprintf(npuSvmLog, "Send TCN/TA/R for %.7s in state %s\n", tp->termName, npuSvmTermStates[tp->state]);
#endif

    switch (tp->state)
        {
    case StTermConnected:         // terminal is connected
        /*
        **  Clean up flow control state and discard any pending output.
        */
        tp->xoff = FALSE;
        npuTipDiscardOutputQ(tp);

        /*
        **  Send TCN/TA/R message to request termination of connection.
        */
        requestTerminateConnection[BlkOffP3] = tp->cn;
        npuBipRequestUplineCanned(requestTerminateConnection, sizeof(requestTerminateConnection));
        tp->state = StTermNpuRequestDisconnect;
        if (tp->pcbp != NULL && tp->pcbp->ncbp != NULL)
            {
            tp->pcbp->ncbp->state = StConnDisconnecting;
            }
        break;

    case StTermIdle:                   // terminal is not yet configured or connected
    case StTermRequestConnection:      // awaiting response to terminal connection request
    case StTermNpuRequestDisconnect:   // disconnection has been requested by NPU/MDI
    case StTermHostRequestDisconnect:  // disconnection has been requested by host
        fprintf(stderr, "Warning - disconnect request ignored for %.7s in state %s\n",
                tp->termName, npuSvmTermStates[tp->state]);
        break;

    default:
        fprintf(stderr, "(npu_svm) Unrecognized state %d during %.7s disconnect request\n",
                tp->state, tp->termName);
        break;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Determine if host is ready for connection request
**
**  Parameters:     Name        Description.
**
**  Returns:        TRUE if host is ready to accept connections, FALSE
**                  otherwise.
**
**------------------------------------------------------------------------*/
bool npuSvmIsReady(void)
    {
    return (svmState == StReady);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Send a connection termination block to the host.
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**
**  Returns:        Nothing
**
**------------------------------------------------------------------------*/
void npuSvmSendTermBlock(Tcb *tp)
    {
#if DEBUG
    fprintf(npuSvmLog, "Send TERM block for %.7s in state %s\n", tp->termName, npuSvmTermStates[tp->state]);
#endif

    blockTerminateConnection[BlkOffCN] = tp->cn;
    npuBipRequestUplineCanned(blockTerminateConnection, sizeof(blockTerminateConnection));
    }

/*
 **--------------------------------------------------------------------------
 **
 **  Private Functions
 **
 **--------------------------------------------------------------------------
 */

/*--------------------------------------------------------------------------
**  Purpose:        Send terminal configuration request to host.
**
**  Parameters:     Name        Description.
**                  pcbp        PCB pointer
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static bool npuSvmRequestTerminalConfig(Pcb *pcbp)
    {
    NpuBuffer *bp;
    u8        *mp;

#if DEBUG
    fprintf(npuSvmLog, "Request terminal configuration for CLA port %02x\n", pcbp->claPort);
#endif

    bp = npuBipBufGet();
    if (bp == NULL)
        {
        return (FALSE);
        }
    /*
    **  Assemble configure request.
    */
    mp = bp->data;

    *mp++ = npuSvmCouplerNode;  // DN
    *mp++ = npuSvmNpuNode;      // SN
    *mp++ = 0;                  // CN
    *mp++ = 4;                  // BT=CMD
    *mp++ = PfcCNF;             // PFC
    *mp++ = SfcTE;              // SFC
    *mp++ = pcbp->claPort;      // non-zero port number from "PORT=" parameter in NDL source
    *mp++ = 0;                  // sub-port number (always 0 for async ports)
    switch (pcbp->ncbp->connType)
        {
    case ConnTypeRaw:
    case ConnTypePterm:
    case ConnTypeRs232:
    case ConnTypeTelnet:
        if (pcbp->controls.async.recoType == TermRecoNonAuto)
            {
            *mp++ = (0 << 7) | (TtASYNC << 3); // no auto recognition and TIP type
            }
        else
            {
            *mp++ = (1 << 7) | (TtASYNC << 3);                                         // auto recognition and TIP type
            *mp++ = (pcbp->controls.async.recoType == TermRecoAuto) ? Ls1200 : Ls9600; // line speed
            *mp++ = CsASCII;                                                           // code set ASCII
            *mp++ = StN2741;                                                           // sub-tip N2741
            *mp++ = 0;                                                                 // A1
            *mp++ = 1;                                                                 // number of terminals
            *mp++ = 0;                                                                 // A2
            *mp++ = DtCONSOLE;                                                         // device type
            }
        break;

    case ConnTypeHasp:
        *mp++ = (0 << 7) | (TtHASP << 3);  // no auto recognition; TIP type; speed range 0
        break;

    case ConnTypeRevHasp:
        *mp++ = (0 << 7) | (TtTT12 << 3);  // no auto recognition; TIP type; speed range 0
        break;

    case ConnTypeNje:
        *mp++ = (0 << 7) | (TtTT13 << 3);  // no auto recognition; TIP type; speed range 0
        break;
        }

    bp->numBytes = mp - bp->data;

    /*
    **  Send the request.
    */
    npuBipRequestUplineTransfer(bp);

    return (TRUE);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Process terminal configuration reply from host.
**
**  Parameters:     Name        Description.
**                  claPort     CLA port number
**                  bp          buffer with service message.
**
**  Returns:        pointer to TCB if successful, NULL otherwise.
**
**------------------------------------------------------------------------*/
static Tcb *npuSvmProcessTerminalConfig(u8 claPort, NpuBuffer *bp)
    {
    u8  cn;
    u8  *mp = bp->data;
    int len = bp->numBytes;
    u8  port;
    u8  subPort;
    u8  address1;
    u8  address2;
    u8  deviceType;
    Pcb *pcbp;
    u8  subTip;
    u8  termName[7];
    u8  termClass;
    Tcb *tp;
    u8  status;
    u8  lastResp;
    u8  codeSet;

#if DEBUG
    fprintf(npuSvmLog, "Process terminal configuration reply for CLA port %02x\n", claPort);
#endif

    pcbp = npuNetFindPcb(claPort);
    if (pcbp == NULL)
        {
#if DEBUG
        fprintf(npuSvmLog, "PCB not found for port 0x%02x\n", claPort);
#endif
        return NULL;
        }

    if (pcbp->connFd <= 0)
        {
#if DEBUG
        fprintf(npuSvmLog, "TCB not allocated for port 0x%02x because network connection is closed\n", claPort);
#endif
        return NULL;
        }

    tp = npuTipFindFreeTcb();
    if (tp == NULL)
        {
#if DEBUG
        fprintf(npuSvmLog, "No free TCB available for port 0x%02x\n", claPort);
#endif
        return NULL;
        }

    /*
    **  Extract configuration.
    */
    mp        += BlkOffP3;
    port       = *mp++;  // should be same as claPort
    subPort    = *mp++;
    address1   = *mp++;
    address2   = *mp++;
    deviceType = *mp++;
    subTip     = *mp++;

    memcpy(termName, mp, sizeof(termName));
    mp += sizeof(termName);

    termClass = *mp++;
    status    = *mp++;
    lastResp  = *mp++;
    codeSet   = *mp++;

    /*
    **  Verify minimum length;
    */
    len -= mp - bp->data;
    if (len < 0)
        {
#if DEBUG
        fprintf(npuSvmLog, "Short Terminal Configuration response with length %d\n", bp->numBytes);
#endif
        return NULL;
        }

    /*
    **  Reset TCB
    */
    cn = tp->cn;
    memset(tp, 0, sizeof(Tcb));
    tp->cn = cn;

    /*
    **  Link TCB to its supporting PCB
    */
    tp->pcbp = pcbp;

    /*
    **  Set TIP type
    */
    switch (tp->pcbp->ncbp->connType)
        {
    case ConnTypeRaw:
    case ConnTypePterm:
    case ConnTypeRs232:
    case ConnTypeTelnet:
        tp->tipType = TtASYNC;
        break;

    case ConnTypeHasp:
        tp->tipType = TtHASP;
        break;

    case ConnTypeRevHasp:
        tp->tipType = TtTT12;
        break;

    case ConnTypeNje:
        tp->tipType = TtTT13;
        break;

    default:
#if DEBUG
        fprintf(npuSvmLog, "Invalid connection type for terminal configuration: %d\n",
            tp->pcbp->ncbp->connType);
#endif
        return NULL;
        }

    /*
    **  Transfer configuration to TCB.
    */
    tp->enabled = status == 0;
    memcpy(tp->termName, termName, sizeof(termName));
    tp->deviceType  = deviceType;
    tp->streamId    = address2;
    tp->subTip      = subTip;
    tp->codeSet     = codeSet;
    tp->params.fvTC = termClass;

    /*
    **  Find owning console
    */
    tp->owningConsole = npuSvmFindOwningConsole(tp);
    if (tp->owningConsole == NULL)
        {
#if DEBUG
        fprintf(npuSvmLog, "Failed to find owning console for %.7s, port 0x%02x\n", tp->termName, claPort);
#endif
        return NULL;
        }
    if (tp->owningConsole->state > StTermConnected) // owning console is disconnecting
        {
        return NULL;
        }

    /*
    **  Setup default operating parameters for the specified terminal class.
    */
    npuTipSetupTerminalClass(tp, termClass);

    /*
    **  Setup TCB with supported FN/FV values.
    */
    npuTipParseFnFv(mp, len, tp);

    /*
    **  Reset user break 2 status.
    */
    tp->breakPending = FALSE;

    /*
    **  Reset input buffer controls
    */
    npuTipInputReset(tp);

    /*
    **  Update maximum active connection number
    */
    npuNetSetMaxCN(tp->cn);

    return tp;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Find the TCB of the owning console associated with a
**                  given TCB.  The owning console of an async terminal is
**                  itself, but the owning console of a HASP or Reverse HASP
**                  device is the first console device found for the given
**                  device's CLA port.
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**
**  Returns:        pointer to TCB.
**
**------------------------------------------------------------------------*/
static Tcb *npuSvmFindOwningConsole(Tcb *tp)
    {
    int i;
    u8  claPort;
    Tcb *tp2;

    if ((tp->tipType == TtASYNC) || (tp->deviceType == DtCONSOLE))
        {
        return tp;
        }

    claPort = tp->pcbp->claPort;
    for (i = 1; i <= npuNetMaxCN; i++)
        {
        tp2 = &npuTcbs[i];
        if ((tp2->state != StTermIdle) && (tp2->pcbp->claPort == claPort) && (tp2->deviceType == DtCONSOLE))
            {
            return tp2;
            }
        }
    npuLogMessage("(npu_svm) No owning console found for connection %u (%.7s)", tp->cn, tp->termName);

    return NULL; // owning console not found
    }

/*--------------------------------------------------------------------------
**  Purpose:        Send connect request to host.
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**
**  Returns:        TRUE if request sent, FALSE otherwise.
**
**------------------------------------------------------------------------*/
static bool npuSvmRequestTerminalConnection(Tcb *tp)
    {
    NpuBuffer *bp;
    u8        *mp;

#if DEBUG
    fprintf(npuSvmLog, "Request connection %02x for terminal %.7s\n", tp->cn, tp->termName);
#endif

    bp = npuBipBufGet();
    if (bp == NULL)
        {
        return (FALSE);
        }

    /*
    **  Assemble connect request.
    */
    mp = bp->data;

    *mp++ = npuSvmCouplerNode;                  // DN
    *mp++ = npuSvmNpuNode;                      // SN
    *mp++ = 0;                                  // CN
    *mp++ = 4;                                  // BT=CMD
    *mp++ = PfcICN;                             // PFC
    *mp++ = SfcTE;                              // SFC
    *mp++ = tp->cn;                             // CN
    *mp++ = tp->params.fvTC;                    // TC
    *mp++ = tp->params.fvPL;                    // page length
    *mp++ = tp->params.fvPW;                    // page width
    *mp++ = tp->deviceType;                     // device type
    *mp++ = tp->params.fvDBL;                   // downline block limit

    memcpy(mp, tp->termName, 7);                // terminal name
    mp += 7;

    *mp++ = tp->params.fvABL;                   // application block limit
    *mp++ = tp->params.fvDBZ >> 8;              // downline block size
    *mp++ = tp->params.fvDBZ & 0xff;
    *mp++ = 0;                                  // auto login indicator
    *mp++ = tp->params.fvDO;                    // device ordinal
    *mp++ = tp->params.fvUBZ >> 8;              // transmission block size
    *mp++ = tp->params.fvUBZ & 0xff;
    *mp++ = tp->params.fvSDT;                   // sub device type

    memcpy(mp, tp->owningConsole->termName, 7); // owning console
    mp += 7;

    *mp++ = 7;                                  // security level
    *mp++ = tp->params.fvPriority;              // priority
    *mp++ = (tp->tipType == TtHASP)             // interactive capability
            ? tp->params.fvRIC : 0;
    *mp++ = tp->params.fvEchoplex;              // echoplex
    *mp++ = 1;                                  // upline block size
    *mp++ = 1;                                  // hardwired indicator
    *mp++ = 0;                                  // fill
    *mp++ = 0;                                  // VTP level
    *mp++ = 0;                                  // calling DTE address length
    *mp++ = 0;                                  // called DTE address length

    bp->numBytes = mp - bp->data;

    /*
    **  Send the request.
    */
    npuBipRequestUplineTransfer(bp);

    return (TRUE);
    }

/*---------------------------  End Of File  ------------------------------*/
