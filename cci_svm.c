/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2023, Tom Hunter, Joachim Siebold
**
**  Name: cci_svm.c
**
**  Description:
**      Perform emulation of the Service Message (SVM) subsystem in an NPU
**      consisting of a CDC 2550 HCP running CCI.
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
#define DEBUG    0

/*
**  -------------
**  Include Files
**  -------------
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>

#if defined(__FreeBSD__)
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#include "const.h"
#include "types.h"
#include "proto.h"
#include "npu.h"
#include "cci.h"

/*
**  -----------------
**  Private Constants
**  -----------------
*/

/*
** Service Messages Function Codes
*/
#define FcConfLine                     0x0300
#define FcDelLine                      0x0301
#define FcConfTerm                     0x0302
#define FcRConfTerm                    0x0303
#define FcDelTerm                      0x0304


#define FcEnaLine                      0x0800
#define FcDisaLine                     0x0801
#define FcDiscLine                     0x0802

/*
** Service Messages Error Codes
*/
#define rcConfInvalidLineNumber        02
#define rcConfLineAlreadyConfigured    03
#define rcConfLineInvalidLineType      04
#define rcConfLineInvalidTermType      05

#define rcLnStatInvalidLineNumber      01
#define rcLnStatRequestInProgress      02
#define rcLnStatLineInvalidState       03

#define rcConfTermInvalidLine          02
#define rcConfTermAlreadyConfigured    03
#define rcConfTermNotConfigured        03
#define rcConfTermNoBufferForTcb       04
#define rcConfTermLineInoperative      06

#define rcDelTermInvalidLine           02
#define rcDelTermNotConfigured         03

#define rcTermOperational              00
#define rcTermInoperative              04

/*
** SFC return code masks
*/
#define SfcRetCodeSuccess              0x40;
#define SfcRetCodeError                0x80;

/*
** Service Message field codes
*/
#define BZOWNER                        5
#define BZLNSPD                        21

#define BSTCLASS                       5
#define BSOWNER                        12
#define BSCN                           13
#define BSNPU                          14
#define BSHOST                         15
#define BSNBL                          16
#define BSIPRI                         19
#define BSPGWIDTH                      28
#define BSBLKLL                        30
#define BSBLKLM                        31
#define BS2629                         32
#define NSNUMR                         33
#define BSSUPCC                        34
#define BSBAN                          35
#define BSEM                           36
#define BSCODE                         37

/*
** CCI response block offsets
*/
#define CciBlkOffLRC                   10  // Line respons code
#define CciBlkOffLLT                   11  // Line line type
#define CciBlkOffLCS                   12  // Configuration state
#define CciBlkOffNT                    13  // Number of terminals

#define MaxLineDefs                    128 // same as MaxClaPort

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

void cciSvmParseLnFnFv(u8 *block, int start, int end, cciLcb *lp);
void cciSvmParseTFnFv(u8 *block, int start, int end, Tcb *tp);
static Tcb *cciSvmProcessTerminalConfig(u8 claPort, NpuBuffer *bp, cciLcb *lp);
static Tcb *cciSvmFindOwningConsole(Tcb *tp);
void cciSvmNotifyTermDisconnect(Tcb *tp);
Pcb *cciFindPcb(int port);
u8 cciGetPortFromPcb(Pcb *pcbp);
cciLcb *cciSvmGetLcbForPort(u8 port);

/*
**  ----------------
**  Public Variables
**  ----------------
*/
cciLcb cciLcbs[MaxLineDefs];

/*
**  -----------------
**  Private Variables
**  -----------------
*/
static u8 cciInitMsg[] =
    {
    0,             // DN
    0,             // SN
    0,             // CN
    4,             // BT = COMMAND
    1,             // PFC
    2,             // SFC
    3,             // CCP version
    1,             // CCP cycle
    1,             // CCP level
    };


static u8 cciUnsoliLineStatusResponse[] =
    {
    0,             // DN
    0,             // SN
    0,             // CN
    4,             // BT = COMMAND
    6,             // PFC
    2,             // SFC Line status unsolicited response
    0,             // P
    0,             // SP
    0,             // RC
    0,             // LT
    0,             // CFS
    0,             // NT
    };

static char disconnectMsg[] = "\r\nHost requests disconnect\r\n";

static u8 numLines;

#if DEBUG
extern FILE *cciLog;
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
void cciSvmInit(void)
    {
    int    i;
    cciLcb *lp;

    cciInitMsg[BlkOffDN] = npuSvmCouplerNode;
    cciInitMsg[BlkOffSN] = npuSvmNpuNode;

    cciUnsoliLineStatusResponse[BlkOffDN] = npuSvmCouplerNode;
    cciUnsoliLineStatusResponse[BlkOffSN] = npuSvmNpuNode;

    /*
    ** Initialize LCBs.
    */
    for (i = 0; i < MaxLineDefs; i++)
        {
        lp = &cciLcbs[i];
        memset(lp, 0, sizeof(cciLcb));
        lp->configState = cciLnConfNotConfigured;
        }

    numLines = 0;
#if DEBUG
    if (cciLog == NULL)
        {
        cciLog = fopen("ccilog.txt", "wt");
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
void cciSvmReset(void)
    {
    int    i;
    cciLcb *lp;

    /*
    ** Initialize LCBs.
    */
    for (i = 0; i < MaxLineDefs; i++)
        {
        lp = &cciLcbs[i];
        memset(lp, 0, sizeof(cciLcb));
        lp->configState = cciLnConfNotConfigured;
        }

    numLines = 0;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Start host connection sequence, send line enabled unsolicited status response to host
**
**  Parameters:     Name        Description.
**                  pcbp        PCB pointer
**
**  Returns:        TRUE if sequence started, FALSE otherwise.
**
**------------------------------------------------------------------------*/
bool cciSvmConnectTerminal(Pcb *pcbp)
    {
    u8     port;
    cciLcb *lp;

    port = cciGetPortFromPcb(pcbp);
#if DEBUG
    fprintf(cciLog, "Connect terminal on CLA port %02x activate line %02x\n", port, port);
#endif

    /*
    ** Build unsolicited line status response, line enabled
    */
    lp = cciSvmGetLcbForPort(port);
    if (lp->configState == cciLnConfInoperativeWaiting)
        {
        lp->configState = cciLnConfOperationalNoTcbs;
        lp->lineState   = cciLnStatOperational;
        cciUnsoliLineStatusResponse[6]  = port;
        cciUnsoliLineStatusResponse[7]  = 0;
        cciUnsoliLineStatusResponse[8]  = cciLnStatOperational;
        cciUnsoliLineStatusResponse[9]  = lp->lineType;
        cciUnsoliLineStatusResponse[10] = cciLnConfOperationalNoTcbs;
        cciUnsoliLineStatusResponse[11] = 0; // no terminals
        npuBipRequestUplineCanned(cciUnsoliLineStatusResponse, sizeof(cciUnsoliLineStatusResponse));

        return (TRUE);
        }

    return (FALSE);
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
void cciSvmSendDiscRequest(Tcb *tp)
    {
    cciLcb *lp;

#if DEBUG
    fprintf(cciLog, "Send TCN/TA/R for %.7s in state %u\n", tp->termName, tp->state);
#endif

    switch (tp->state)
        {
    case StTermConnected:         // terminal is connected
        /*
        **  Clean up flow control state and discard any pending output.
        */
        tp->xoff = FALSE;
        cciTipDiscardOutputQ(tp);

        /*
        **  Send TCN/TA/R message to request termination of connection.
        */
        lp = cciSvmGetLcbForPort(tp->cciPort);
        if (lp->configState == cciLnConfOperationalTcbsConfigured)
            {
            lp->configState = cciLnConfInoperativeTcbsConfigured;
            lp->lineState   = cciLnStatInoperative;
            cciUnsoliLineStatusResponse[6]  = tp->cciPort;
            cciUnsoliLineStatusResponse[7]  = 0;
            cciUnsoliLineStatusResponse[8]  = cciLnStatInoperative;
            cciUnsoliLineStatusResponse[9]  = lp->lineType;
            cciUnsoliLineStatusResponse[10] = cciLnConfInoperativeTcbsConfigured;
            cciUnsoliLineStatusResponse[11] = lp->numTerminals;

            npuBipRequestUplineCanned(cciUnsoliLineStatusResponse, sizeof(cciUnsoliLineStatusResponse));
            tp->state = StTermNpuRequestDisconnect;
            }
        break;

    case StTermIdle:                   // terminal is not yet configured or connected
    case StTermHostRequestDisconnect:  // disconnection has been requested by host
        fprintf(stderr, "Warning - disconnect request ignored for %.7s in state %s\n",
                tp->termName, npuSvmTermStates[tp->state]);

    case StTermNpuRequestDisconnect:   // disconnection has been requested by NPU/MDI
        break;

    default:
        fprintf(stderr, "(cci_svm) Unrecognized state %d during %.7s disconnect request\n",
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
bool cciSvmIsReady(void)
    {
    return (cciHipIsReady());
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
void cciSvmProcessBuffer(NpuBuffer *bp)
    {
    u8        *block = bp->data;
    u8        *rp;
    u8        *rpHead;
    u8        port = 0;
    u8        cn;
    u8        rc;
    bool      err;
    u16       fCode;
    cciLcb    *lp = (cciLcb *)NULL;
    Tcb       *tp;
    Pcb       *pcbp = (Pcb *)NULL;
    u8        i;
    u8        tcbCn;
    NpuBuffer *respBuf;

    fCode = (block[BlkOffPfc] << 8) | block[BlkOffSfc];

    /*
    ** precheck
    */
    /*
    **  Ensure there is at least a minimal service message.
    */
    if (bp->numBytes < BlkOffSfc + 1)
        {
        if ((bp->numBytes == BlkOffBTBSN + 1) && (block[BlkOffCN] != 0))
            {
            /*
            **  Exception to minimal service message:
            **  For some strange reason INTERCOM sends an input acknowledgment
            **  as a SVM - forward it to the TIP which is better equipped
            **  to deal with this.
            */
            cciTipProcessBuffer(bp, 0);

            return;
            }

        /*
        **  Service message must be at least DN/SN/0/BSN/PFC/SFC.
        */
        fprintf(stderr, "(cci_svm) Short message: ");
        for (int i = 0; i < bp->numBytes; i++)
            {
            fprintf(stderr, " %x", bp->data[i]);
            }
        fprintf(stderr, "\n");

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
        fprintf(stderr, "(cci_svm) Connection number is %u but must be zero in SVM messages %02X/%02X\n",
                cn, block[BlkOffPfc], block[BlkOffSfc]);

        /*
        **  Release downline buffer and return.
        */
        npuBipBufRelease(bp);

        return;
        }


    switch (fCode)
        {
    case FcConfLine:
    case FcEnaLine:
    case FcDiscLine:
    case FcDisaLine:
    case FcConfTerm:
    case FcRConfTerm:
    case FcDelTerm:
        port = block[BlkOffP];
        assert(block[BlkOffSP] == 0);

        if (port > MaxLineDefs)
            {
            /*
            **  Connection number out of range.
            */
            fprintf(stderr, "(cci_svm) Port number out of range %02X/%02X\n",
                    block[BlkOffPfc], block[BlkOffSfc]);

            /*
            **  Release downline buffer and return.
            */
            npuBipBufRelease(bp);

            return;
            }
        pcbp = cciFindPcb(port);
        if (pcbp == NULL)
            {
            /*
            **  illegal CLA port number
            */
            fprintf(stderr, "(cci_svm) illegal CLA port %02X number  %02X/%02X\n",
                    port, block[BlkOffPfc], block[BlkOffSfc]);

            /*
            **  Release downline buffer and return.
            */
            npuBipBufRelease(bp);

            return;
            }

        if (pcbp->ncbp == NULL)
            {
            /*
            **  associated CLA port configured
            */
            fprintf(stderr, "(cci_svm) CLA port %02X not configured %02X/%02X\n",
                    port, block[BlkOffPfc], block[BlkOffSfc]);

            /*
            **  Release downline buffer and return.
            */
            npuBipBufRelease(bp);

            return;
            }
        lp = cciSvmGetLcbForPort(port);
        }

    /*
    ** allocate response buffer
    */
    respBuf = npuBipBufGet();
    if (respBuf == NULL)
        {
        /*
        ** No NPU buffer available for response
        */
        fprintf(stderr, "(cci_svm) No response buffer availabel for SVM messages %02X/%02X\n",
                block[BlkOffPfc], block[BlkOffSfc]);

        /*
        **  Release downline buffer and return.
        */
        npuBipBufRelease(bp);

        return;
        }

    rp     = respBuf->data;
    rpHead = rp;
    *rp++  = npuSvmCouplerNode;
    *rp++  = npuSvmNpuNode;
    *rp++  = cn;
    *rp++  = BtHTCMD;

    /*
    ** process message
    */
    rc  = 0;
    err = FALSE;

    switch (fCode)
        {
    /*
    ** configure line
    */
    case FcConfLine:

        if (lp->configState != cciLnConfNotConfigured)
            {
            /*
            ** Line already configured
            */
            rc  = rcConfLineAlreadyConfigured;
            err = TRUE;
            break;
            }

        /*
        ** We only support switched asynchronous lines at the moment
        */
        if (block[BlkOffLt] != 6)
            {
#if DEBUG
            fprintf(stderr, "(cci_svm) port: %u illegal line type %u\n", port, block[BlkOffLt]);
#endif
            rc  = rcConfLineInvalidLineType;
            err = TRUE;
            break;
            }

        /*
        ** We only support tip type TTY with autorecognition at the moment
        */
/*
 *          if (block[BlkOffTt] != 0x88)
 *              {
 #if DEBUG
 *                  fprintf(stderr,"(cci_svm) port: %u illegal terminal type %u\n",port,block[BlkOffTt]);
 #endif
 *                  rc= rcConfLineInvalidTermType;
 *                  err = TRUE;
 *                  break;
 *              }
 */
        pcbp->cciWaitForTcb = TRUE;
        lp->configState     = cciLnConfConfigured;
        lp->port            = port;
        lp->lineType        = block[BlkOffLt];
        lp->terminalType    = block [BlkOffTt];
        lp->lineState       = cciLnStatInoperative;
        pcbp->cciIsDisabled = TRUE;
        cciSvmParseLnFnFv(block, 10, bp->numBytes, lp);
        numLines++;

#if DEBUG
        fprintf(cciLog, "\n(cci_svm) line config P: %d  LT %d TT: %d RC: %d\n", port, block[BlkOffLt],
                block[BlkOffTt], rc);
#endif
        break;


    /*
    ** enable line
    */
    case FcEnaLine:
        if (lp->configState != cciLnConfConfigured)
            {
            /*
            ** Line not configured
            */
            rc  = rcLnStatLineInvalidState;
            err = TRUE;
            break;
            }

        /*
        ** we already checked the line type above
        */
        if (lp->lineType == 6)
            {
            lp->configState     = cciLnConfInoperativeWaiting;
            pcbp->cciIsDisabled = FALSE;
            lp->lineState       = cciLnStatNoRing;
            rc = lp->lineState;
            }
#if DEBUG
        fprintf(cciLog, "\n(cci_svm) line enable P %d CNF %d RC %d\n", port, lp->configState, rc);
#endif
        break;

    case FcDisaLine:
        if (lp->configState != cciLnConfInoperativeWaiting)
            {
            /*
            ** Line not configured
            */
            rc  = rcLnStatLineInvalidState;
            err = TRUE;
            break;
            }

        /*
        ** we already checked the line type above
        */
        if (lp->lineType == 6)
            {
            lp->configState     = cciLnConfConfigured;
            lp->lineState       = cciLnStatInoperative;
            pcbp->cciIsDisabled = TRUE;
            rc = lp->lineState;
            }
#if DEBUG
        fprintf(cciLog, "\n(cci_svm) line disable P %d CNF %d RC %d\n", port, lp->configState, rc);
#endif
        break;

    /*
    ** disconnect line
    */
    case FcDiscLine:
        if (lp->configState == cciLnConfNotConfigured)
            {
            /*
            ** Line not configured
            */
            rc  = rcLnStatLineInvalidState;
            err = TRUE;
            break;
            }

        /*
        ** we already checked the line type above
        */
        if (lp->lineType == 6)

        /*
        ** check, if we have already configured terminals
        */
            {
            if (lp->numTerminals != 0)

            /*
            ** line disconnect requested by host (not implemented)
            */
                {
#if DEBUG
                fprintf(stderr, "unhandled host line disconnect request P: %d State %d numTerm %d\n",
                        port, lp->lineState, lp->numTerminals);
#endif
                rc = cciLnStatInoperative;
                break;
                }

            /*
            ** no terminals, set line to enabled state waiting for ring
            */
            lp->configState     = cciLnConfInoperativeWaiting;
            lp->lineState       = cciLnStatNoRing;
            pcbp->cciIsDisabled = FALSE;
            rc = lp->lineState;
            }

        else

        /*
        ** unsupported line type, set line to inoperative
        */
            {
            lp->configState     = cciLnConfConfigured;
            lp->lineState       = cciLnStatInoperative;
            pcbp->cciIsDisabled = TRUE;
            rc = cciLnStatInoperative;
            break;
            }
#if DEBUG
        fprintf(cciLog, "(cci_svm) line disconnect P %d CFS %d RC %d\n", port, lp->configState, rc);
#endif
        break;

    /*
    ** configure terminal
    */
    case FcConfTerm:

        if (lp->configState != cciLnConfOperationalNoTcbs)
            {
            rc  = rcConfTermLineInoperative;
            err = TRUE;
            break;
            }

        tp = cciSvmProcessTerminalConfig(port, bp, lp);
        if (tp != NULL)
            {
            tp->state = StTermConnected;
            npuNetSetMaxCN(tp->cn);

            /*
            ** notify connection, to async only at the moment
            */
            npuAsyncNotifyTermConnect(tp);
            npuNetConnected(tp);
            }
        else
            {
            rc  = rcConfTermNoBufferForTcb;
            err = TRUE;
            break;
            }

        lp->configState = cciLnConfOperationalTcbsConfigured;
        lp->numTerminals++;
        pcbp->cciWaitForTcb = FALSE;

#if DEBUG
        fprintf(cciLog, "\n(cci_svm) configure terminal P %d CN %d RC %d\n", port, tp->cn, rc);
#endif
        break;

    /*
    ** reconfigure terminal
    */
    case FcRConfTerm:

        tcbCn = block[11];
        tp    = cciTipFindTcbForCN(tcbCn);

        /*
        ** line not configured
        */
        if (lp->configState == cciLnConfNotConfigured)
            {
            rc  = rcDelTermInvalidLine;
            err = TRUE;
            break;
            }

        if (tp->state == StTermIdle)
            {
            rc  = rcDelTermNotConfigured;
            err = TRUE;
            break;
            }

        /*
        ** we have to check what to do here
        */
#if DEBUG
        fprintf(cciLog, "\n(cci_svm) reconfigure terminal P %d CN %d RC %d\n", port, tp->cn, rc);
#endif
        break;

    /*
    ** delete terminal
    */
    case FcDelTerm:
        tcbCn = block[11];
        tp    = cciTipFindTcbForCN(tcbCn);

        /*
        ** line not configured
        */
        if (lp->configState == cciLnConfNotConfigured)
            {
            rc  = rcDelTermInvalidLine;
            err = TRUE;
            break;
            }

        if (tp->state == StTermIdle)
            {
            rc  = rcDelTermNotConfigured;
            err = TRUE;
            break;
            }

        if (tp->state == StTermConnected)
            {
            /*
            ** Host requests disconnect, output message to terminal
            */
            send(tp->pcbp->connFd, disconnectMsg, strlen(disconnectMsg), 0);
            }

        if (tp->state == StTermNpuRequestDisconnect)

        /*
        ** notify disconnect if device type is DtCONSOLE
        */
            {
            cciSvmNotifyTermDisconnect(tp);
            }
        npuNetDisconnected(tp);
        pcbp->cciWaitForTcb = TRUE;

        /*
        ** reset Tcb
        */
        tcbCn = tp->cn;
        memset(tp, 0, sizeof(Tcb));
        tp->cn    = tcbCn;
        tp->state = StTermIdle;
        cciTipInputReset(tp);

        lp->numTerminals--;
        if (lp->numTerminals == 0)
            {
            lp->configState = cciLnConfInoperativeWaiting;
            lp->lineState   = cciLnStatNoRing;
            }

#if DEBUG
        fprintf(cciLog, "\n(cci_svm) delete terminal P %d CN: %d rc %d\n", port, cn, rc);
#endif
        break;

#if DEBUG
    default:
        fprintf(cciLog, "\n(cci_svm) Unhandled service message request %x\n", fCode);
#endif
        }

    /*
    ** build response, process Sfc, add port and subport
    */
    *rp++ = block[BlkOffPfc];
    if (err)
        {
        *rp++ = block[BlkOffSfc] | SfcRetCodeError;
        }
    else
        {
        *rp++ = block[BlkOffSfc] | SfcRetCodeSuccess;
        }

    /*
    ** add port and subport, in this case lp points to the line control block
    */
    if (lp != (cciLcb *)NULL)
        {
        *rp++ = port;
        *rp++ = 0;
        }

    switch (fCode)
        {
    /*
    ** configure line, we do not consider invalid FN/FV at the moment
    */
    case FcConfLine:
        for (i = 0; i < 2; i++)
            {
            *rp++ = block[BlkOffP5 + i];                     // LT / TT
            }
        *rp++ = rc;
        break;

    /*
    ** enable line
    */
    case FcEnaLine:
        *rp++ = rc;
        if (err)
            {
            break;
            }
        *rp++ = lp->lineType;
        *rp++ = lp->configState;
        *rp++ = 0;
        break;

    /*
    **  disconnect line
    **  normal response is enable line response
    **  SFC is 0x80 for line inoperative
    */
    case FcDiscLine:
        if (err)
            {
            if (rc == cciLnStatInoperative)
                {
                *(rpHead + BlkOffSfc) = 0x80;
                }
            *rp = rc;
            }
        else
            {
            *(rpHead + BlkOffSfc) = 0x40;
            *rp++ = rc;
            *rp++ = lp->lineType;
            *rp++ = lp->configState;
            *rp++ = lp->numTerminals;
            }
        break;

    /*
    **  disable line
    */
    case FcDisaLine:
        if (err)
            {
            *rp++ = rc;
            break;
            }
        *rp++ = 0;
        *rp++ = lp->lineType;
        *rp++ = lp->configState;
        *rp++ = lp->numTerminals;
        break;

    /*
    ** we do not care about invalid FN/FV at the moment
    */
    case FcConfTerm:
    case FcRConfTerm:
    case FcDelTerm:
        for (i = 0; i < 4; i++)
            {
            *rp++ = block[BlkOffP5 + i];                     // CA / TA / DT /CN
            }
        *rp++ = rc;
        break;
        }

#if DEBUG
    if (port > 0)
        {
        fprintf(cciLog, "lcb port %x cfs  %x lTp %x termTp %x speedIdx %x  numTerminals %x\n",
                port, lp->configState, lp->lineType, lp->terminalType, lp->speedIndex,
                lp->numTerminals);
        }
    if (cn > 0)
        {
        fprintf(cciLog, "tcb port %x ca  %x ta %x dt %x cn %x\n",
                port, tp->cciClusterAddress, tp->cciTerminalAddress, tp->cciDeviceType, cn);
        }
#endif

    /*
    ** send response and relase buffer
    */
    respBuf->numBytes = rp - rpHead;
    npuBipRequestUplineTransfer(respBuf);

    npuBipBufRelease(bp);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Parse Line FN/FV for Service Messages
**
**  Parameters:     Name        Description.
**                  block       SM buffer address
**                  start       start index of FNFV SM tail
**                  end         length of SM
**                  lp          address of LCB
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cciSvmParseLnFnFv(u8 *block, int start, int end, cciLcb *lp)
    {
    int i;

    block += start;
    for (i = start; i < end ; i += 2)
        {
        switch (*block)
            {
        case BZLNSPD:
            lp->speedIndex = *(block + 1);
            break;

        case BZOWNER:
            break;

        default:
            break;
            }
        block += 2;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Parse Terminal FN/FV for Service Messages
**
**  Parameters:     Name        Description.
**                  block       SM buffer address
**                  start       start index of FNFV SM tail
**                  end         length of SM
**                  tp          address of TCB
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cciSvmParseTFnFv(u8 *block, int start, int end, Tcb *tp)
    {
    int i;

    block += start;
#if DEBUG
    fprintf(cciLog, "(cci_svm) terminal params: ");
#endif
    for (i = start; i < end ; i += 2)
        {
#if DEBUG
        fprintf(cciLog, " %d %x ", *block, *(block + 1));
#endif
        switch (*block)
            {
        case BSTCLASS:
            break;

        case BSOWNER:
            break;

        case BSCN:
            break;

        case BSNPU:
            break;

        case BSHOST:
            break;

        case BSNBL:
            break;

        case BSIPRI:
            break;

        case BSPGWIDTH:
            break;

        case BSBLKLL:
            break;

        case BSBLKLM:
            break;

        case BS2629:
            break;

        case NSNUMR:
            break;

        case BSSUPCC:
            break;

        case BSBAN:
            break;

        case BSEM:
            break;

        case BSCODE:
            break;

        default:
            break;
            }
        block += 2;
        }
#if DEBUG
    fprintf(cciLog, "\n");
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Send NPU initialized service message
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cciSvmNpuInitResponse(void)
    {
    npuBipRequestUplineCanned(cciInitMsg, sizeof(cciInitMsg));
    }

/*--------------------------------------------------------------------------
**  Purpose:        Process terminal configuration sm from host.
**
**  Parameters:     Name        Description.
**                  port        port number
**                  bp          buffer with service message.
**
**  Returns:        pointer to TCB if successful, NULL otherwise.
**
**------------------------------------------------------------------------*/
static Tcb *cciSvmProcessTerminalConfig(u8 claPort, NpuBuffer *bp, cciLcb *lp)
    {
    u8   cn;
    Pcb  *pcbp;
    Tcb  *tp;
    u8   *block = bp->data;
    char tName[8];

#if DEBUG
    fprintf(cciLog, "Process terminal configuration reply for port %02x\n", claPort);
#endif

    pcbp = cciFindPcb(claPort);
    if (pcbp == NULL)
        {
#if DEBUG
        fprintf(cciLog, "PCB not found for port 0x%02x\n", claPort);
#endif

        return NULL;
        }

    if (pcbp->connFd <= 0)
        {
#if DEBUG
        fprintf(cciLog, "TCB not allocated for port 0x%02x because network connection is closed\n", claPort);
#endif

        return NULL;
        }

    /*
    **  Reset Tcb and extract configuration.
    */
    cn = block[11];
    tp = cciTipFindTcbForCN(cn);
    if (tp->state != StTermIdle)
        {
#if DEBUG
        fprintf(cciLog, "TCB for port 0x%02x not available\n", claPort);
#endif

        return NULL;
        }

    memset(tp, 0, sizeof(Tcb));
    tp->cn                 = cn;
    tp->cciPort            = block[6];
    tp->cciClusterAddress  = block[8];
    tp->cciTerminalAddress = block[9];
    tp->cciDeviceType      = block[10];

    /*
    **  terminal name
    */
    sprintf(tName, "C%2.2X%2.2X%2.2X", tp->cciPort, tp->cciClusterAddress, tp->cciTerminalAddress);
    memcpy(tp->termName, tName, 7);

    /*
    **  Link TCB to its supporting PCB
    */
    tp->pcbp = pcbp;

    /*
    ** map CCI tip to CCP tip
    */
    switch ((lp->terminalType >> 3) & 0xF)
        {
    case 1:
        tp->tipType = TtASYNC;
        break;

/*
 *      case 2:
 *           tp->tipType= TtMODE4;
 *           break;
 *      case 3:
 *           tp->tipType= TtHASP;
 *           break;
 *      case 4:
 *           tp->tipType= TtBSC;
 *           break;
 */
    default:
#if DEBUG
        fprintf(cciLog, "Invalid connection type for terminal configuration: %d\n",
                tp->pcbp->ncbp->connType);
#endif

        return NULL;
        }
    tp->subTip = 0;

    tp->streamId = 0;    // not used for ASYNC

    /*
    ** configure Terminal (set deviceType, code set and assign params)
    */
    cciTipConfigureTerminal(tp);
    tp->params.fvTC = Tc721;

    /*
    **  Find owning console
    */
    tp->owningConsole = cciSvmFindOwningConsole(tp);
    if (tp->owningConsole == NULL)
        {
#if DEBUG
        fprintf(cciLog, "Failed to find owning console for %.7s, port 0x%02x\n", tp->termName, claPort);
#endif

        return NULL;
        }
    if (tp->owningConsole->state > StTermConnected) // owning console is disconnecting
        {
        return NULL;
        }

    /*
    **  Setup TCB with supported FN/FV values.
    */
    cciSvmParseTFnFv(block, 12, bp->numBytes, tp);

    /*
    **  Reset user break 2 status.
    */
    tp->breakPending = FALSE;

    /*
    **  Reset input buffer controls
    */
    cciTipInputReset(tp);

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
static Tcb *cciSvmFindOwningConsole(Tcb *tp)
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
#if DEBUG
    fprintf(stderr, "(cci_svm) No owning console found for connection %u (%.7s)", tp->cn, tp->termName);
#endif

    return NULL; // owning console not found
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
void cciSvmNotifyTermDisconnect(Tcb *tp)
    {
#if DEBUG
    fprintf(cciLog, "Notify TIP of %.7s disconnect in state %s\n", tp->termName, npuSvmTermStates[tp->state]);
#endif

    /*
    ** ASYNC only at the moment
    */
    npuAsyncNotifyTermDisconnect(tp);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Find the PCB for a CCI port number
**
**  Parameters:     Name        Description.
**                  port        CCI port number
**
**  Returns:        pointer to PCB, or NULL if PCB not found.
**
**------------------------------------------------------------------------*/
Pcb *cciFindPcb(int port)
    {
    return npuNetFindPcb(port);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Get the CCI port number from ap PCB structure
**
**  Parameters:     Name        Description.
**                  pcbp        pointer to PCB
**
**  Returns:        CCI port number
**
**------------------------------------------------------------------------*/
u8 cciGetPortFromPcb(Pcb *pcbp)
    {
    return (pcbp->claPort);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Get the line control block for a port number
**
**  Parameters:     Name        Description.
**                  pcbp        pointer to PCB
**
**  Returns:        CCI port number
**
**------------------------------------------------------------------------*/
cciLcb * cciSvmGetLcbForPort(u8 port)
    {
    return (&cciLcbs[port]);
    }
