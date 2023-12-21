/*-------------------------------------------------------------------------
**
**  Copyright (c) 2003-2023, Tom Hunter, Joachim Siebold
**
**  Name: cci_svm.c
**
**  Description:
**      Perform emulation of the Terminal Interfacei Protocol (TIP) subsystem in an NPU
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
#define BtBlk     1
#define BtMsg     2
#define BtBack    3
#define BtCmd     4

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
static void cciTipSetupDefaultTc0(void);
void cciTipSendAck(Tcb *tp);

/*
**  ----------------
**  Public Variables
**  ----------------
*/

/*
**  -----------------
**  Private Variables
**  -----------------
*/

static u8 resBack[] =
    {
    0,                // DN
    0,                // SN
    0,                // CN
    0,                // BSN/BT
    };

static u8 blockAck[] =
    {
    0,                  // DN
    0,                  // SN
    0,                  // CN
    BtHTBACK,           // BT/BSN/PRIO
    };

static u8 resTermOperational[] =
    {
    0,                  // DN
    0,                  // SN
    0,                  // Service Channel
    4,                  // BT
    6,                  // PFC
    3,                  // SFC
    0,                  // P
    0,                  // SP
    0,                  // CA
    0,                  // TA
    0,                  // DT
    0,                  // RC
    0,                  // DN
    0,                  // SN
    0,                  // CN
    1                   // TOT
    };

static TipParams defaultTc0;


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
**  Purpose:        Initialize TIP.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cciTipInit(void)
    {
    int i;
    Tcb *tp;

    resBack[BlkOffDN] = npuSvmCouplerNode;
    resBack[BlkOffSN] = npuSvmNpuNode;

    blockAck[BlkOffDN] = npuSvmCouplerNode;
    blockAck[BlkOffSN] = npuSvmNpuNode;

    resTermOperational[BlkOffDN] = npuSvmCouplerNode;
    resTermOperational[BlkOffSN] = npuSvmNpuNode;

#if DEBUG
    if (cciLog == NULL)
        {
        cciLog = fopen("ccilog.txt", "wt");
        }
#endif

    /*
    ** Terminal definition
    */
    cciTipSetupDefaultTc0();

    /*
    **  Initialise TCBs.
    */
    for (i = 0; i < MaxTcbs; i++)
        {
        tp = &npuTcbs[i];
        memset(tp, 0, sizeof(Tcb));
        tp->cn    = i;
        tp->state = StTermIdle;
        cciTipInputReset(tp);
        }

    /*
    **  Initialise network.
    */
    npuNetInit(TRUE);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Reset TIP.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cciTipReset(void)
    {
    int i;
    Tcb *tp;

    /*
    **  Initialise TCBs.
    */
    for (i = 0; i < MaxTcbs; i++)
        {
        tp = &npuTcbs[i];
        memset(tp, 0, sizeof(Tcb));
        tp->cn    = i;
        tp->state = StTermIdle;
        npuTipInputReset(tp);
        }

    /*
    **  Re-initialise network.
    */
    npuNetInit(FALSE);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Process service message from host.
**
**  Parameters:     Name        Description.
**                  bp          buffer with service message.
**                  priority    priority
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cciTipProcessBuffer(NpuBuffer *bp, int priority)
    {
    u8   *block = bp->data;
    Tcb  *tp;
    u8   cn;
    bool last;

    /*
    **  Find associated terminal control block.
    */
    cn = block[BlkOffCN];
    tp = cciTipFindTcbForCN(cn);
    if (tp == NULL)
        {
#if DEBUG
        fprintf(cciLog, "Unexpected connection number %u in message, PFC/SFC %02x/%02x\n", cn,
                block[BlkOffPfc], block[BlkOffSfc]);
#endif

        return;
        }

    switch (block[BlkOffBTBSN] & BlkMaskBT)
        {
    /*
    ** command block
    */
    case BtHTCMD:
        switch (block[BlkOffPfc])
            {
        case 7:
            /*
            **  Resume output marker after user break 1 or 2.
            */
            tp->breakPending = FALSE;
            break;


        default:
#if DEBUG
                {
                int i;
                fprintf(cciLog, "Unrecognized command received on connection %u, %02x/%02x\n   ",
                        cn, block[BlkOffPfc], block[BlkOffSfc]);
                for (i = 0; i < bp->numBytes; i++)
                    {
                    fprintf(stderr, " %02x", bp->data[i]);
                    }
                fputs("\n", stderr);
                }
#endif
            break;
            }

        /*
        **  Acknowledge any command (although most are ignored).
        */
        cciTipSendAck(tp);
        break;

    case BtHTBLK:
    case BtHTMSG:
        if (tp->state == StTermConnected)
            {
            last = (block[BlkOffBTBSN] & BlkMaskBT) == BtHTMSG;
            switch (tp->tipType)
                {
            case TtASYNC:
                cciAsyncProcessDownlineData(tp, bp, last);
                break;


            default:
                fprintf(stderr, "(npu_tip) Downline data for unrecognized TIP type %u on connection %u\n",
                        tp->tipType, tp->cn);
                }
            cciTipSendAck(tp);
            break;
            }
        else
            {
            /*
            **  Handle possible race condition while disconnecting. Acknowledge any
            **  packets arriving during this time, but discard the contents.
            */
            cciTipSendAck(tp);
            }
        break;

    case BtHTBACK:
        break;

    default:
#if DEBUG
            {
            int i;
            fprintf(cciLog, "Unrecognized block type: %02X\n   ", block[BlkOffBTBSN] & BlkMaskBT);
            for (i = 0; i < bp->numBytes; i++)
                {
                fprintf(cciLog, " %02x", block[i]);
                }
            fputs("\n", cciLog);
            }
#endif
        break;
        }

    /*
    **  Release downline buffer.
    */
    npuBipBufRelease(bp);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Send Data Block, apply correct BSN
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**                  len         length of block
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cciTipSendBlock(Tcb *tp, int len)

    {
    tp->inBuf[BlkOffBTBSN] = BtHTBLK | (tp->uplineBsn << BlkShiftBSN);
    npuBipRequestUplineCanned(tp->inBuf, len);
    tp->uplineBsn += 1;
    if (tp->uplineBsn > 7)
        {
        tp->uplineBsn = 0;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Send Data Msg, apply correct BSN
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**                  len         length of block
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cciTipSendMsg(Tcb *tp, int len)

    {
    tp->inBuf[BlkOffBTBSN] = BtHTMSG | (tp->uplineBsn << BlkShiftBSN);
    npuBipRequestUplineCanned(tp->inBuf, len);
    tp->uplineBsn += 1;
    if (tp->uplineBsn > 7)
        {
        tp->uplineBsn = 0;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Send Acknowlegement
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cciTipSendAck(Tcb *tp)
    {
    blockAck[BlkOffCN]     = tp->cn;
    blockAck[BlkOffBTBSN] &= BlkMaskBT;
    blockAck[BlkOffBTBSN] |= (tp->uplineBsn << BlkShiftBSN);
    npuBipRequestUplineCanned(blockAck, sizeof(blockAck));
    tp->uplineBsn += 1;
    if (tp->uplineBsn > 7)
        {
        tp->uplineBsn = 0;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Network has sent the data - generate acknowledgement.
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**                  blockSeqNo  block sequence number
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cciTipNotifySent(Tcb *tp, u8 blockSeqNo)
    {
    blockAck[BlkOffCN]     = tp->cn;
    blockAck[BlkOffBTBSN] &= BlkMaskBT;
    blockAck[BlkOffBTBSN] |= (blockSeqNo << BlkShiftBSN);
    npuBipRequestUplineCanned(blockAck, sizeof(blockAck));
    }

/*--------------------------------------------------------------------------
**  Purpose:        Reset the input buffer state.
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cciTipInputReset(Tcb *tp)
    {
    u8 *mp = tp->inBuf;

    /*
    **  Build upline data header.
    */
    *mp++ = npuSvmCouplerNode;                        // DN
    *mp++ = npuSvmNpuNode;                            // SN
    *mp++ = tp->cn;                                   // CN
    *mp++ = 0;                                        // BT
    *mp++ = 5;                                        // DBC
    *mp++ = 0;                                        // TCS
    *mp++ = 0;                                        // TCS
    *mp++ = 0;                                        // LV

    /*
    **  Initialise buffer pointers.
    */
    tp->inBufStart = mp;
    tp->inBufPtr   = mp;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Send user break 1 or 2 to host (dummy routine for CCI)
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**                  bt          break type (1 or 2)
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cciTipSendUserBreak(Tcb *tp, u8 bt)
    {
    }

/*--------------------------------------------------------------------------
**  Purpose:        Discard the pending output queue, but generate required
**                  acknowledgments.
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cciTipDiscardOutputQ(Tcb *tp)
    {
    NpuBuffer *bp;

    while ((bp = npuBipQueueExtract(&tp->outputQ)) != NULL)
        {
/*
 *      if (bp->blockSeqNo != 0)
 *          {
 *          blockAck[BlkOffCN]     = tp->cn;
 *          blockAck[BlkOffBTBSN] &= BlkMaskBT;
 *          blockAck[BlkOffBTBSN] |= bp->blockSeqNo;
 *          npuBipRequestUplineCanned(blockAck, sizeof(blockAck));
 *          }
 */
        npuBipBufRelease(bp);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Configure Terminal
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**                  cciTT       cci Terminal type (from line control block)
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cciTipConfigureTerminal(Tcb *tp)
    {
    /*
    ** map CCI device type to CCP device type
    */
    switch (tp->cciDeviceType >> 5)
        {
    case 0:
        tp->deviceType = DtCONSOLE;
        tp->params     = defaultTc0;
        break;

    case 1:
        tp->deviceType = DtCR;
        break;

    case 2:
        tp->deviceType = DtLP;
        break;

    case 3:
        tp->deviceType = DtCP;
        break;

    case 4:
        tp->deviceType = DtPLOTTER;
        break;
        }
    tp->codeSet = 0;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Find the TCB assigned to a terminal connection
**
**  Parameters:     Name        Description.
**                  cn          Terminal connection number
**
**  Returns:        pointer to TCB, or NULL if TCB not found.
**
**------------------------------------------------------------------------*/
Tcb *cciTipFindTcbForCN(u8 cn)
    {
    return &npuTcbs[cn];
    }

/*
 **--------------------------------------------------------------------------
 **
 **  Private Functions
 **
 **--------------------------------------------------------------------------
 */

/*--------------------------------------------------------------------------
**  Purpose:        Setup Default Terminal class parameters
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cciTipSetupDefaultTc0(void)
    {
    defaultTc0.fvAbortBlock     = 'X' - 0x40;
    defaultTc0.fvBlockFactor    = 1;
    defaultTc0.fvBreakAsUser    = FALSE;
    defaultTc0.fvBS             = ChrBS;
    defaultTc0.fvUserBreak1     = 'P' - 0x40;
    defaultTc0.fvUserBreak2     = 'T' - 0x40;
    defaultTc0.fvEnaXUserBreak  = FALSE;
    defaultTc0.fvCI             = 0;
    defaultTc0.fvCIAuto         = FALSE;
    defaultTc0.fvCN             = 'X' - 0x40;
    defaultTc0.fvCursorPos      = TRUE;
    defaultTc0.fvCT             = ChrESC;
    defaultTc0.fvXCharFlag      = FALSE;
    defaultTc0.fvXCnt           = 2043;
    defaultTc0.fvXChar          = ChrCR;
    defaultTc0.fvXTimeout       = FALSE;
    defaultTc0.fvXModeMultiple  = FALSE;
    defaultTc0.fvEOB            = ChrEOT;
    defaultTc0.fvEOBterm        = 2;
    defaultTc0.fvEOBCursorPos   = 3;
    defaultTc0.fvEOL            = ChrCR;
    defaultTc0.fvEOLTerm        = 1;
    defaultTc0.fvEOLCursorPos   = 2;
    defaultTc0.fvEchoplex       = TRUE;
    defaultTc0.fvFullASCII      = FALSE;
    defaultTc0.fvInFlowControl  = FALSE;
    defaultTc0.fvXInput         = FALSE;
    defaultTc0.fvInputDevice    = 0;
    defaultTc0.fvLI             = 0;
    defaultTc0.fvLIAuto         = FALSE;
    defaultTc0.fvLockKeyboard   = FALSE;
    defaultTc0.fvOutFlowControl = FALSE;
    defaultTc0.fvOutputDevice   = 1;
    defaultTc0.fvParity         = 2;
    defaultTc0.fvPG             = FALSE;
    defaultTc0.fvPL             = 24;
    defaultTc0.fvPW             = 80;
    defaultTc0.fvSpecialEdit    = FALSE;
    defaultTc0.fvTC             = Tc721;
    defaultTc0.fvXStickyTimeout = FALSE;
    defaultTc0.fvXModeDelimiter = 0;
    defaultTc0.fvDuplex         = FALSE;
    defaultTc0.fvSolicitInput   = FALSE;
    defaultTc0.fvCIDelay        = 0;
    defaultTc0.fvLIDelay        = 0;
    defaultTc0.fvHostNode       = npuSvmCouplerNode;
    defaultTc0.fvAutoConnect    = FALSE;
    defaultTc0.fvPriority       = 1;
    defaultTc0.fvUBL            = 7;
    defaultTc0.fvUBZ            = 100;
    defaultTc0.fvABL            = 2;
    defaultTc0.fvDBL            = 2;
    defaultTc0.fvDBZ            = 940;
    defaultTc0.fvRIC            = 0;
    defaultTc0.fvSDT            = 0;
    defaultTc0.fvDO             = 1;
    }
