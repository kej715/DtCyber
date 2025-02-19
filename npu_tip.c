/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**
**  Name: npu_tip.c
**
**  Description:
**      Perform emulation of the Terminal Interface Protocol in an NPU
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

/*
**  -----------------
**  Private Constants
**  -----------------
*/

/*
**  Field name codes, used in various packets such as CNF/TE. These are
**  defined in "NAM 1 Host Application Progr. RM (60499500W 1987)" on
**  pages 3-59 to 3-62.
*/
#define FnTdAbortBlock        0x29  // Abort block character
#define FnTdBlockFactor       0x19  // Blocking factor; multiple of 100 chars (upline block)
#define FnTdBreakAsUser       0x33  // Break as user break 1; yes (1), no (0)
#define FnTdBS                0x27  // Backspace character
#define FnTdUserBreak1        0x2A  // User break 1 character
#define FnTdUserBreak2        0x2B  // User break 2 character
#define FnTdEnaXUserBreak     0x95  // Enable transparent user break commands; yes (1), no (0)
#define FnTdCI                0x2C  // Carriage return idle count
#define FnTdCIAuto            0x2E  // Carriage return idle count - TIP calculates suitable number
#define FnTdCN                0x26  // Cancel character
#define FnTdCursorPos         0x47  // Cursor positioning; yes (1), no (0)
#define FnTdCT                0x28  // Network control character
#define FnTdXCharFlag         0x38  // Transparent input delimiter character specified flag; yes (1), no (0)
#define FnTdXCntMSB           0x39  // Transparent input delimiter count MSB
#define FnTdXCntLSB           0x3A  // Transparent input delimiter count LSB
#define FnTdXChar             0x3B  // Transparent input delimiter character
#define FnTdXTimeout          0x3C  // Transparent input mode delimiter timeout; yes (1), no (0)
#define FnTdXModeMultiple     0x46  // Transparent input mode; multiple (1), single (0)
#define FnTdEOB               0x40  // End of block character
#define FnTdEOBterm           0x41  // End of block terminator; EOL (1), EOB (2)
#define FnTdEOBCursorPos      0x42  // EOB cursor pos; no (0), CR (1), LF (2), CR & LF (3)
#define FnTdEOL               0x3D  // End of line character
#define FnTdEOLTerm           0x3E  // End of line terminator; EOL (1), EOB (2)
#define FnTdEOLCursorPos      0x3F  // EOL cursor pos; no (0), CR (1), LF (2), CR & LF (3)
#define FnTdEchoplex          0x31  // Echoplex mode
#define FnTdFullASCII         0x37  // Full ASCII input; yes (1), no (0)
#define FnTdInFlowControl     0x43  // Input flow control; yes (1), no (0)
#define FnTdXInput            0x34  // Transparent input; yes (1), no (0)
#define FnTdInputDevice       0x35  // Keyboard (0), paper tape (1), block mode (2)
#define FnTdLI                0x2D  // Line feed idle count
#define FnTdLIAuto            0x2F  // Line feed idle count - TIP calculates suitable number; yes (1), no (0)
#define FnTdLockKeyboard      0x20  // Lockout unsolicited input from keyboard; yes (1), no (0)
#define FnTdOutFlowControl    0x44  // Output flow control; yes (1), no (0)
#define FnTdOutputDevice      0x36  // Printer (0), display (1), paper tape (2)
#define FnTdParity            0x32  // Zero (0), odd (1), even (2), none (3), ignore (4)
#define FnTdPG                0x25  // Page waiting; yes (1), no (0)
#define FnTdPL                0x24  // Page length in lines; 0, 8 - FF
#define FnTdPW                0x23  // Page width in columns; 0, 20 - FF
#define FnTdSpecialEdit       0x30  // Special editing mode; yes (1), no (0)
#define FnTdTC                0x22  // Terminal class
#define FnTdXStickyTimeout    0x92  // Sticky transparent input forward on timeout; yes (1), no (0)
#define FnTdXModeDelimiter    0x45  // Transparent input mode delimiter character
#define FnSDT                 0x4C  // Subdevice type
#define FnDO1                 0x50  // Device ordinal
#define FnTdDuplex            0x57  // full (1), half (0)
#define FnTdUBZMSB            0x1E  // Upline block size MSB
#define FnTdUBZLSB            0x1F  // Upline block size LSB
#define FnTdSolicitInput      0x70  // yes (1), no (0)
#define FnTdCIDelay           0x93  // Carriage return idle delay in 4 ms increments
#define FnTdLIDelay           0x94  // Line feed idle delay in 4 ms increments

/*
**  The Field Name values below are not documented in the NAM manual.
*/
#define FnTdHostNode          0x14  // Selected host node
#define FnTdAutoConnect       0x16  // yes (1), no (0)
#define FnTdPriority          0x17  // Terminal priority
#define FnTdUBL               0x18  // Upline block count limit
#define FnTdABL               0x1A  // Application block count limit
#define FnTdDBL               0x1B  // Downline block count limit
#define FnTdDBZMSB            0x1C  // Downline block size MSB
#define FnTdDBZLSB            0x1D  // Downline block size LSB
#define FnTdRIC               0x4D  // Restricted interactive console (RBF)

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
static void npuTipNotifyAck(Tcb *tp, u8 bsn);
static void npuTipSetupDefaultTc2(void);
static void npuTipSetupDefaultTc3(void);
static void npuTipSetupDefaultTc7(void);
static void npuTipSetupDefaultTc9(void);
static void npuTipSetupDefaultTc14(void);
static void npuTipSetupDefaultTc28(void);
static void npuTipSetupDefaultTc29(void);

#if DEBUG
static void npuTipLogFnFv(Tcb *tp, u8 fn, u8 fv);

#endif

/*
**  ----------------
**  Public Variables
**  ----------------
*/
Tcb npuTcbs[MaxTcbs];

/*
**  -----------------
**  Private Variables
**  -----------------
*/
static u8 ackInitBt[] =
    {
    0,                             // DN
    0,                             // SN
    0,                             // CN
    (0 << BlkShiftBSN) | BtHTBACK, // BT/BSN/PRIO
    };

static u8 respondToInitBt[] =
    {
    0,                              // DN
    0,                              // SN
    0,                              // CN
    (0 << BlkShiftBSN) | BtHTNINIT, // BT/BSN/PRIO
    };

static u8 requestInitBt[] =
    {
    0,                              // DN
    0,                              // SN
    0,                              // CN
    (0 << BlkShiftBSN) | BtHTRINIT, // BT/BSN/PRIO
    };

static u8 blockAck[] =
    {
    0,                  // DN
    0,                  // SN
    0,                  // CN
    BtHTBACK,           // BT/BSN/PRIO
    };

static u8 intrRsp[4] =
    {
    0,                  // DN
    0,                  // SN
    0,                  // CN
    BtHTICMR,           // BT/BSN/PRIO
    };

static TipParams defaultTc2;
static TipParams defaultTc3;
static TipParams defaultTc7;
static TipParams defaultTc9;
static TipParams defaultTc14;
static TipParams defaultTc28;
static TipParams defaultTc29;

/*
**  Table of functions that notify of upline block acknowledgement, indexed by connection type
*/
static void (*notifyAck[])(Tcb *tp, u8 bsn) =
    {
    npuTipNotifyAck,  // ConnTypeRaw
    npuTipNotifyAck,  // ConnTypePterm
    npuTipNotifyAck,  // ConnTypeRs232
    npuTipNotifyAck,  // ConnTypeTelnet
    npuHaspNotifyAck, // ConnTypeHasp
    npuHaspNotifyAck, // ConnTypeRevHasp
    npuNjeNotifyAck,  // ConnTypeNje
    npuTipNotifyAck   // ConnTypeTrunk
    };

#if DEBUG
static FILE *npuTipLog = NULL;
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
void npuTipInit(void)
    {
    int i;
    Tcb *tp;

    ackInitBt[BlkOffDN] = npuSvmCouplerNode;
    ackInitBt[BlkOffSN] = npuSvmNpuNode;

    respondToInitBt[BlkOffDN] = npuSvmCouplerNode;
    respondToInitBt[BlkOffSN] = npuSvmNpuNode;

    requestInitBt[BlkOffDN] = npuSvmCouplerNode;
    requestInitBt[BlkOffSN] = npuSvmNpuNode;

    blockAck[BlkOffDN] = npuSvmCouplerNode;
    blockAck[BlkOffSN] = npuSvmNpuNode;

    intrRsp[BlkOffDN] = npuSvmCouplerNode;
    intrRsp[BlkOffSN] = npuSvmNpuNode;

    /*
    **  Initialize default terminal class parameters.
    */
    npuTipSetupDefaultTc2();
    npuTipSetupDefaultTc3();
    npuTipSetupDefaultTc7();
    npuTipSetupDefaultTc9();
    npuTipSetupDefaultTc14();
    npuTipSetupDefaultTc28();
    npuTipSetupDefaultTc29();

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
    **  Initialise network.
    */
    npuNetInit(TRUE);

#if DEBUG
    npuTipLog = fopen("tiplog.txt", "wt");
    if (npuTipLog == NULL)
        {
        fprintf(stderr, "tiplog.txt - aborting\n");
        exit(1);
        }
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Reset TIP.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuTipReset(void)
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
**  Purpose:        Find a free, unassigned TCB
**
**  Parameters:     Name        Description.
**
**  Returns:        pointer to TCB, or NULL if TCB not found.
**
**------------------------------------------------------------------------*/
Tcb *npuTipFindFreeTcb(void)
    {
    int i;

    for (i = 1; i < MaxTcbs; i++)
        {
        if (npuTcbs[i].state == StTermIdle)
            {
            return &npuTcbs[i];
            }
        }

    return NULL;
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
Tcb *npuTipFindTcbForCN(u8 cn)
    {
    return &npuTcbs[cn];
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
void npuTipProcessBuffer(NpuBuffer *bp, int priority)
    {
    static int count  = 0;
    u8         *block = bp->data;
    Tcb        *tp;
    u8         cn;
    bool       last;

    /*
    **  Find associated terminal control block.
    */
    cn = block[BlkOffCN];
    tp = npuTipFindTcbForCN(cn);
    if (tp == NULL)
        {
#if DEBUG
        fprintf(npuTipLog, "Unexpected connection number %u in message, PFC/SFC %02x/%02x\n", cn,
                block[BlkOffPfc], block[BlkOffSfc]);
#endif

        return;
        }
#if DEBUG
    {
    int i;
    fprintf(npuTipLog, "Connection %u received block type %02x, PFC %02x, SFC %02x\n   ",
            cn, block[BlkOffBTBSN] & BlkMaskBT, block[BlkOffPfc], block[BlkOffSfc]);
    for (i = 0; i < bp->numBytes; i++)
        {
        fprintf(npuTipLog, " %02x", bp->data[i]);
        }
    fputs("\n", npuTipLog);
    }
#endif

    switch (block[BlkOffBTBSN] & BlkMaskBT)
        {
    case BtHTRINIT:
        ackInitBt[BlkOffCN] = block[BlkOffCN];
        npuBipRequestUplineCanned(ackInitBt, sizeof(ackInitBt));

        respondToInitBt[BlkOffCN] = block[BlkOffCN];
        npuBipRequestUplineCanned(respondToInitBt, sizeof(respondToInitBt));

        requestInitBt[BlkOffCN] = block[BlkOffCN];
        npuBipRequestUplineCanned(requestInitBt, sizeof(requestInitBt));
        break;

    case BtHTNINIT:
        // Init response, nothing to be done
        break;

    case BtHTCMD:
        switch (block[BlkOffPfc])
            {
        case PfcCTRL:
            if (block[BlkOffSfc] == SfcCHAR)
                {
                /*
                **  Terminal characteristics define multiple characteristics -
                **  setup TCB with supported FN/FV values.
                */
                npuTipParseFnFv(block + BlkOffP3, bp->numBytes - BlkOffP3, tp);
                }
            break;

        case PfcRO:
            if (block[BlkOffSfc] == SfcMARK)
                {
                /*
                **  Resume output marker after user break 1 or 2.
                */
                tp->breakPending = FALSE;
                }
            break;

        case PfcBD:
            if ((tp->tipType == TtHASP) && (block[BlkOffSfc] == SfcCHG))
                {
                /*
                **  Batch device characteristics define multiple characteristics -
                **  setup PCB with supported FN/FV values.
                */
                npuHaspParseDevParams(block + BlkOffP3, bp->numBytes - BlkOffP3, tp);
                }
            break;

        case PfcBF:
            if ((tp->tipType == TtHASP) && (block[BlkOffSfc] == SfcCHG))
                {
                /*
                **  Batch file characteristics define multiple characteristics -
                **  setup PCB with supported FN/FV values.
                */
                npuHaspParseFileParams(block + BlkOffP3, bp->numBytes - BlkOffP3, tp);
                }
            break;

        case PfcTO:
            /*
            **  The TO (Terminate Output) command is sent to HASP print and punch streams to indicate
            **  that an operator has requested an output to terminate.  Normally, the SFC will be SfcMARK.
            */
            if (tp->tipType == TtHASP)
                {
                npuHaspNotifyTerminateOutput(tp, block[BlkOffSfc]);
                }
#if DEBUG
            else
                {
                fputs("Unexpected TO command for non-HASP connection\n", npuTipLog);
                }
#endif
            break;

        case PfcSI:
            /*
            **  The SI (Start Input) command is sent to HASP card reader streams to indicate
            **  that RBF is ready to receive PRU data.  Normally, the SFC will be SfcNONTR.
            */
            if (tp->tipType == TtHASP)
                {
                npuHaspNotifyStartInput(tp, block[BlkOffSfc]);
                }
#if DEBUG
            else
                {
                fputs("Unexpected SI command for non-HASP connection\n", npuTipLog);
                }
#endif
            break;

        default:
#if DEBUG
            fputs("Unrecognized TIP command\n", npuTipLog);
#endif
            break;
        }

        /*
        **  Acknowledge any command (although most are ignored).
        */
        blockAck[BlkOffCN]     = block[BlkOffCN];
        blockAck[BlkOffBTBSN] &= BlkMaskBT;
        blockAck[BlkOffBTBSN] |= block[BlkOffBTBSN] & (BlkMaskBSN << BlkShiftBSN);
        npuBipRequestUplineCanned(blockAck, sizeof(blockAck));
        break;

    case BtHTBLK:
    case BtHTMSG:
        if (tp->state == StTermConnected)
            {
            last = (block[BlkOffBTBSN] & BlkMaskBT) == BtHTMSG;
            switch (tp->tipType)
            {
            case TtASYNC:
                npuAsyncProcessDownlineData(tp, bp, last);
                break;

            case TtHASP:
            case TtTT12:
                npuHaspProcessDownlineData(tp, bp, last);
                break;

            case TtTT13:
                npuNjeProcessDownlineData(tp, bp, last);
                break;

            default:
                fprintf(stderr, "(npu_tip) Downline data for unrecognized TIP type %u on connection %u\n",
                        tp->tipType, tp->cn);
                blockAck[BlkOffCN]     = block[BlkOffCN];
                blockAck[BlkOffBTBSN] &= BlkMaskBT;
                blockAck[BlkOffBTBSN] |= block[BlkOffBTBSN] & (BlkMaskBSN << BlkShiftBSN);
                npuBipRequestUplineCanned(blockAck, sizeof(blockAck));
                break;
            }
            }
        else
            {
            /*
            **  Handle possible race condition while not fully connected. Acknowledge any
            **  packets arriving during this time, but discard the contents.
            */
            blockAck[BlkOffCN]     = block[BlkOffCN];
            blockAck[BlkOffBTBSN] &= BlkMaskBT;
            blockAck[BlkOffBTBSN] |= block[BlkOffBTBSN] & (BlkMaskBSN << BlkShiftBSN);
            npuBipRequestUplineCanned(blockAck, sizeof(blockAck));
            }
        break;

    case BtHTQBLK:
    case BtHTQMSG:
        fprintf(stderr, "(npu_tip) Qualified block/message ignored, port=%02x\n", tp->pcbp->claPort);
        break;

    case BtHTBACK:
        notifyAck[tp->pcbp->ncbp->connType](tp, (block[BlkOffBTBSN] >> BlkShiftBSN) & BlkMaskBSN);
        break;

    case BtHTTERM:
        npuSvmProcessTermBlock(tp);
        break;

    case BtHTICMD:
        /*
        **  Interrupt command.  Discard any pending output.
        */
        tp->xoff = FALSE;
        npuTipDiscardOutputQ(tp);
        intrRsp[BlkOffCN]     = block[BlkOffCN];
        intrRsp[BlkOffBTBSN] &= BlkMaskBT;
        intrRsp[BlkOffBTBSN] |= block[BlkOffBTBSN] & (BlkMaskBSN << BlkShiftBSN);
        npuBipRequestUplineCanned(intrRsp, sizeof(intrRsp));
        break;

    case BtHTICMR:
        /*
        **  Ignore interrupt response.
        */
        break;

    case BtHTBREAK:
        if (tp->tipType == TtASYNC)
            {
            npuAsyncProcessBreakIndication(tp);
            }
        break;

    default:
#if DEBUG
        fputs("Unrecognized block type\n", npuTipLog);
#endif
        break;
        }

    /*
    **  Release downline buffer.
    */
    npuBipBufRelease(bp);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Setup default parameters for the specified terminal class.
**
**  Parameters:     Name        Description.
**                  tp          pointer to TCB
**                  tc          terminal class
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuTipSetupTerminalClass(Tcb *tp, u8 tc)
    {
#if DEBUG
    fprintf(npuTipLog, "Connection %u set terminal class to %u\n", tp->cn, tc);
#endif

    switch (tc)
        {
    case Tc713:
        tp->params = defaultTc2;  // CDC 713, 751-1, 752, 756
        break;

    case Tc721:
        tp->params = defaultTc3;  // CDC 721
        break;

    case TcX364:
        tp->params = defaultTc7;  // X3.64 (VT-100)
        break;

    case TcHASP:
        tp->params = defaultTc9;  // HASP Post
        break;

    case TcHPRE:
        tp->params = defaultTc14; // HASP Pre
        break;

    case TcUTC1:
        tp->params = defaultTc28; // TIELINE (Reverse HASP)
        break;

    case TcUTC2:
        tp->params = defaultTc29; // NJE
        break;

    default:
        switch (tp->tipType)
        {
        default:
        case TtASYNC:
            tp->params = defaultTc3;
            break;

        case TtHASP:
            tp->params = defaultTc9;
            break;

        case TtTT12:
            tp->params = defaultTc28;
            break;

        case TtTT13:
            tp->params = defaultTc29;
            break;
        }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Parse FN/FV string.
**
**  Parameters:     Name        Description.
**                  mp          message pointer
**                  len         message length
**                  tp          pointer to TCB
**
**  Returns:        TRUE if no error, FALSE otherwise.
**
**------------------------------------------------------------------------*/
bool npuTipParseFnFv(u8 *mp, int len, Tcb *tp)
    {
    TipParams *pp = &tp->params;

    while (len > 0)
        {
#if DEBUG
        npuTipLogFnFv(tp, mp[0], mp[1]);
#endif
        switch (mp[0])
            {
        case FnTdAbortBlock:     // Abort block character
            pp->fvAbortBlock = mp[1];
            break;

        case FnTdBlockFactor:    // Blocking factor; multiple of 100 chars (upline block)
            /*
            **  Only accept a sane blocking factor. The resulting block must not
            **  be larger then NPU buffer. This will also protect us from buffer
            **  overruns in the upline functions of the ASYNC TIP.
            */
            if ((mp[1] > 0) && (mp[1] <= 20))
                {
                pp->fvBlockFactor = mp[1];
                }
            break;

        case FnTdBreakAsUser:    // Break as user break 1; yes (1), no (0)
            pp->fvBreakAsUser = mp[1] != 0;
            break;

        case FnTdBS:             // Backspace character
            pp->fvBS = mp[1];
            break;

        case FnTdUserBreak1:     // User break 1 character
            pp->fvUserBreak1 = mp[1];
            break;

        case FnTdUserBreak2:     // User break 2 character
            pp->fvUserBreak2 = mp[1];
            break;

        case FnTdEnaXUserBreak:  // Enable transparent user break commands; yes (1), no (0)
            pp->fvEnaXUserBreak = mp[1] != 0;
            break;

        case FnTdCI:             // Carriage return idle count
            pp->fvCI = mp[1];
            break;

        case FnTdCIAuto:         // Carriage return idle count - TIP calculates suitable number
            pp->fvCIAuto = mp[1] != 0;
            break;

        case FnTdCN:             // Cancel character
            pp->fvCN = mp[1];
            break;

        case FnTdCursorPos:      // Cursor positioning; yes (1), no (0)
            pp->fvCursorPos = mp[1] != 0;
            break;

        case FnTdCT:             // Network control character
            pp->fvCT = mp[1];
            break;

        case FnTdXCharFlag:      // Transparent input delimiter character specified flag; yes (1), no (0)
            pp->fvXCharFlag = mp[1] != 0;
            break;

        case FnTdXCntMSB:        // Transparent input delimiter count MSB
            pp->fvXCnt &= 0x00FF;
            pp->fvXCnt |= mp[1] << 8;
            break;

        case FnTdXCntLSB:        // Transparent input delimiter count LSB
            pp->fvXCnt &= 0xFF00;
            pp->fvXCnt |= mp[1] << 0;
            break;

        case FnTdXChar:          // Transparent input delimiter character
            pp->fvXChar = mp[1];
            break;

        case FnTdXTimeout:       // Transparent input delimiter timeout; yes (1), no (0)
            pp->fvXTimeout = mp[1] != 0;
            break;

        case FnTdXModeMultiple:  // Transparent input mode; multiple (1), single (0)
            pp->fvXModeMultiple = mp[1] != 0;
            break;

        case FnTdEOB:            // End of block character
            pp->fvEOB = mp[1];
            break;

        case FnTdEOBterm:        // End of block terminator; EOL (1), EOB (2)
            pp->fvEOBterm = mp[1];
            break;

        case FnTdEOBCursorPos:   // EOB cursor pos; no (0), CR (1), LF (2), CR & LF (3)
            pp->fvEOBCursorPos = mp[1];
            break;

        case FnTdEOL:            // End of line character
            pp->fvEOL = mp[1];
            break;

        case FnTdEOLTerm:        // End of line terminator; EOL (1), EOB (2)
            pp->fvEOLTerm = mp[1];
            break;

        case FnTdEOLCursorPos:   // EOL cursor pos; no (0), CR (1), LF (2), CR & LF (3)
            pp->fvEOLCursorPos = mp[1];
            break;

        case FnTdEchoplex:       // Echoplex mode
            pp->fvEchoplex = mp[1] != 0;
            break;

        case FnTdFullASCII:      // Full ASCII input; yes (1), no (0)
            pp->fvFullASCII = mp[1] != 0;
            break;

        case FnTdInFlowControl:  // Input flow control; yes (1), no (0)
            pp->fvInFlowControl = mp[1] != 0;
            break;

        case FnTdXInput:         // Transparent input; yes (1), no (0)
            pp->fvXInput = mp[1] != 0;
            break;

        case FnTdInputDevice:    // Keyboard (0), paper tape (1), block mode (2)
            pp->fvInputDevice = mp[1];
            break;

        case FnTdLI:             // Line feed idle count
            pp->fvLI = mp[1];
            break;

        case FnTdLIAuto:         // Line feed idle count - TIP calculates suitable number; yes (1), no (0)
            pp->fvLIAuto = mp[1] != 0;
            break;

        case FnTdLockKeyboard:   // Lockout unsolicited input from keyboard; yes (1), no (0)
            pp->fvLockKeyboard = mp[1] != 0;
            break;

        case FnTdOutFlowControl: // Output flow control; yes (1), no (0)
            pp->fvOutFlowControl = mp[1] != 0;
            if (!pp->fvOutFlowControl)
                {
                /*
                **  If flow control is now disabled, turn off the xoff flag
                **  if it was set.
                */
                tp->xoff = FALSE;
                }
            break;

        case FnTdOutputDevice:   // Printer (0), display (1), paper tape (2)
            pp->fvOutputDevice = mp[1];
            break;

        case FnTdParity:         // Zero (0), odd (1), even (2), none (3), ignore (4)
            pp->fvParity = mp[1];
            // printf("Term = %u, Parity = %u\n", tp->pcbp->claPort, mp[1]); fflush(stdout);
            break;

        case FnTdPG:             // Page waiting; yes (1), no (0)
            pp->fvPG = mp[1] != 0;
            break;

        case FnTdPL:             // Page length in lines; 0, 8 - FF
            pp->fvPL = mp[1];
            break;

        case FnTdPW:             // Page width in columns; 0, 20 - FF
            pp->fvPW = mp[1];
            break;

        case FnTdSpecialEdit:    // Special editing mode; yes (1), no (0)
            pp->fvSpecialEdit = mp[1] != 0;
            break;

        case FnTdTC:             // Terminal class
            if (pp->fvTC != mp[1])
                {
                pp->fvTC = mp[1];
                npuTipSetupTerminalClass(tp, pp->fvTC);
                }
            break;

        case FnTdXStickyTimeout:// Sticky transparent input forward on timeout; yes (1), no (0)
            pp->fvXStickyTimeout = mp[1] != 0;
            break;

        case FnTdXModeDelimiter: // Transparent input mode delimiter character
            pp->fvXModeDelimiter = mp[1];
            break;

        case FnTdDuplex:         // full (1), half (0)
            pp->fvDuplex = mp[1] != 0;
            break;

        case FnTdUBZMSB:         // Terminal transmission block size MSB
            pp->fvUBZ &= 0x00FF;
            pp->fvUBZ |= mp[1] << 8;
            break;

        case FnTdUBZLSB:         // Terminal transmission block size LSB
            pp->fvUBZ &= 0xFF00;
            pp->fvUBZ |= mp[1];
            break;

        case FnTdSolicitInput:   // yes (1), no (0)
            pp->fvSolicitInput = mp[1] != 0;
            break;

        case FnTdCIDelay:        // Carriage return idle delay in 4 ms increments
            pp->fvCIDelay = mp[1];
            break;

        case FnTdLIDelay:        // Line feed idle delay in 4 ms increments
            pp->fvLIDelay = mp[1];
            break;

        case FnTdHostNode:       // Selected host node
            pp->fvHostNode = mp[1];
            break;

        case FnTdAutoConnect:    // yes (1), no (0)
            pp->fvAutoConnect = mp[1] != 0;
            break;

        case FnTdPriority:       // Terminal priority
            pp->fvPriority = mp[1];
            break;

        case FnTdUBL:            // Upline block count limit
            pp->fvUBL = mp[1];
            break;

        case FnTdABL:            // Application block count limit
            pp->fvABL = mp[1];
            break;

        case FnTdDBL:            // Downline block count limit
            pp->fvDBL = mp[1];
            break;

        case FnTdDBZMSB:      // Downline block size MSB
            pp->fvDBZ &= 0x00FF;
            pp->fvDBZ |= mp[1] << 8;
            break;

        case FnTdDBZLSB:      // Downline block size LSB
            pp->fvDBZ &= 0xFF00;
            pp->fvDBZ |= mp[1];
            break;

        case FnTdRIC:           // Restricted interactive console (RBF)
            pp->fvRIC = mp[1];
            break;

        case FnSDT:             // Subdevice type
            pp->fvSDT = mp[1];
            break;

        case FnDO1:            // Device ordinal
            pp->fvDO = mp[1];
            break;

        default:
#if DEBUG
            fprintf(npuTipLog, "Unknown FN/FV (%d/%d)[%02x/%02x]\n", mp[0], mp[1], mp[0], mp[1]);
#endif
            break;
            }

        /*
        **  Advance to next FN/FV pair.
        */
        mp  += 2;
        len -= 2;
        }

    return (TRUE);
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
void npuTipInputReset(Tcb *tp)
    {
    u8 *mp = tp->inBuf;

    /*
    **  Increment BSN.
    */
    tp->uplineBsn += 1;
    if (tp->uplineBsn >= 8)
        {
        tp->uplineBsn = 1;
        }

    /*
    **  Build upline data header.
    */
    *mp++ = npuSvmCouplerNode;                        // DN
    *mp++ = npuSvmNpuNode;                            // SN
    *mp++ = tp->cn;                                   // CN
    *mp++ = BtHTMSG | (tp->uplineBsn << BlkShiftBSN); // BT=MSG
    *mp++ = 0;                                        // DBC

    /*
    **  Initialise buffer pointers.
    */
    tp->inBufStart = mp;
    tp->inBufPtr   = mp;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Send user break 1 or 2 to host.
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**                  bt          break type (1 or 2)
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuTipSendUserBreak(Tcb *tp, u8 bt)
    {
    u8 *mp;

    /*
    **  Ignore user break if previous break has not yet been processed.
    */
    if (tp->breakPending)
        {
        return;
        }

    tp->breakPending = TRUE;

    /*
    **  Build upline ICMD.
    */
    mp    = tp->inBuf;
    *mp++ = npuSvmCouplerNode;                         // DN
    *mp++ = npuSvmNpuNode;                             // SN
    *mp++ = tp->cn;                                    // CN
    *mp++ = BtHTICMD | (tp->uplineBsn << BlkShiftBSN); // BT=BRK
    *mp++ = (1 << (bt - 1)) + 2;

    /*
    **  Send the ICMD.
    */
    npuBipRequestUplineCanned(tp->inBuf, mp - tp->inBuf);

    /*
    **  Increment BSN.
    */
    tp->uplineBsn += 1;
    if (tp->uplineBsn == 8)
        {
        tp->uplineBsn = 1;
        }

    /*
    **  Build upline BI/MARK.
    */
    mp    = tp->inBuf;
    *mp++ = npuSvmCouplerNode;                        // DN
    *mp++ = npuSvmNpuNode;                            // SN
    *mp++ = tp->cn;                                   // CN
    *mp++ = BtHTCMD | (tp->uplineBsn << BlkShiftBSN); // BT=BRK
    *mp++ = PfcBI;
    *mp++ = SfcMARK;

    /*
    **  Send the BI/MARK.
    */
    npuBipRequestUplineCanned(tp->inBuf, mp - tp->inBuf);

    /*
    **  Purge output and send back all acknowledgments.
    */
    npuTipDiscardOutputQ(tp);

    /*
    **  Reset input buffer.
    */
    npuTipInputReset(tp);
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
void npuTipDiscardOutputQ(Tcb *tp)
    {
    NpuBuffer *bp;

    while ((bp = npuBipQueueExtract(&tp->outputQ)) != NULL)
        {
        if (bp->blockSeqNo != 0)
            {
            blockAck[BlkOffCN]     = tp->cn;
            blockAck[BlkOffBTBSN] &= BlkMaskBT;
            blockAck[BlkOffBTBSN] |= bp->blockSeqNo;
            npuBipRequestUplineCanned(blockAck, sizeof(blockAck));
            }

        npuBipBufRelease(bp);
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
void npuTipNotifySent(Tcb *tp, u8 blockSeqNo)
    {
    blockAck[BlkOffCN]     = tp->cn;
    blockAck[BlkOffBTBSN] &= BlkMaskBT;
    blockAck[BlkOffBTBSN] |= blockSeqNo & (BlkMaskBSN << BlkShiftBSN);
    npuBipRequestUplineCanned(blockAck, sizeof(blockAck));
    }

/*
 **--------------------------------------------------------------------------
 **
 **  Private Functions
 **
 **--------------------------------------------------------------------------
 */

/*--------------------------------------------------------------------------
**  Purpose:        Handle upline block acknowledgement
**
**  Parameters:     Name        Description.
**                  tp          Pointer to TCB
**                  bsn         Block sequence number of acknowledged block
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuTipNotifyAck(Tcb *tp, u8 bsn)
    {
    // Do nothing for now
    }

/*--------------------------------------------------------------------------
**  Purpose:        Setup CDC 713 defaults (terminal class 2)
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuTipSetupDefaultTc2(void)
    {
    defaultTc2.fvAbortBlock     = 'X' - 0x40;
    defaultTc2.fvBlockFactor    = 1;
    defaultTc2.fvBreakAsUser    = FALSE;
    defaultTc2.fvBS             = ChrBS;
    defaultTc2.fvUserBreak1     = 'P' - 0x40;
    defaultTc2.fvUserBreak2     = 'T' - 0x40;
    defaultTc2.fvEnaXUserBreak  = FALSE;
    defaultTc2.fvCI             = 0;
    defaultTc2.fvCIAuto         = FALSE;
    defaultTc2.fvCN             = 'X' - 0x40;
    defaultTc2.fvCursorPos      = TRUE;
    defaultTc2.fvCT             = ChrESC;
    defaultTc2.fvXCharFlag      = FALSE;
    defaultTc2.fvXCnt           = 2043;
    defaultTc2.fvXChar          = ChrCR;
    defaultTc2.fvXTimeout       = FALSE;
    defaultTc2.fvXModeMultiple  = FALSE;
    defaultTc2.fvEOB            = ChrEOT;
    defaultTc2.fvEOBterm        = 2;
    defaultTc2.fvEOBCursorPos   = 3;
    defaultTc2.fvEOL            = ChrCR;
    defaultTc2.fvEOLTerm        = 1;
    defaultTc2.fvEOLCursorPos   = 2;
    defaultTc2.fvEchoplex       = TRUE;
    defaultTc2.fvFullASCII      = FALSE;
    defaultTc2.fvInFlowControl  = FALSE;
    defaultTc2.fvXInput         = FALSE;
    defaultTc2.fvInputDevice    = 0;
    defaultTc2.fvLI             = 0;
    defaultTc2.fvLIAuto         = FALSE;
    defaultTc2.fvLockKeyboard   = FALSE;
    defaultTc2.fvOutFlowControl = FALSE;
    defaultTc2.fvOutputDevice   = 1;
    defaultTc2.fvParity         = 2;
    defaultTc2.fvPG             = FALSE;
    defaultTc2.fvPL             = 24;
    defaultTc2.fvPW             = 80;
    defaultTc2.fvSpecialEdit    = FALSE;
    defaultTc2.fvTC             = Tc721;
    defaultTc2.fvXStickyTimeout = FALSE;
    defaultTc2.fvXModeDelimiter = 0;
    defaultTc2.fvDuplex         = FALSE;
    defaultTc2.fvSolicitInput   = FALSE;
    defaultTc2.fvCIDelay        = 0;
    defaultTc2.fvLIDelay        = 0;
    defaultTc2.fvHostNode       = npuSvmCouplerNode;
    defaultTc2.fvAutoConnect    = FALSE;
    defaultTc2.fvPriority       = 1;
    defaultTc2.fvUBL            = 7;
    defaultTc2.fvUBZ            = 100;
    defaultTc2.fvABL            = 2;
    defaultTc2.fvDBL            = 2;
    defaultTc2.fvDBZ            = 940;
    defaultTc2.fvRIC            = 0;
    defaultTc2.fvSDT            = 0;
    defaultTc2.fvDO             = 1;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Setup CDC 721 defaults (terminal class 3)
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuTipSetupDefaultTc3(void)
    {
    defaultTc3.fvAbortBlock     = 'X' - 0x40;
    defaultTc3.fvBlockFactor    = 1;
    defaultTc3.fvBreakAsUser    = FALSE;
    defaultTc3.fvBS             = ChrBS;
    defaultTc3.fvUserBreak1     = 'P' - 0x40;
    defaultTc3.fvUserBreak2     = 'T' - 0x40;
    defaultTc3.fvEnaXUserBreak  = FALSE;
    defaultTc3.fvCI             = 0;
    defaultTc3.fvCIAuto         = FALSE;
    defaultTc3.fvCN             = 'X' - 0x40;
    defaultTc3.fvCursorPos      = TRUE;
    defaultTc3.fvCT             = ChrESC;
    defaultTc3.fvXCharFlag      = FALSE;
    defaultTc3.fvXCnt           = 2043;
    defaultTc3.fvXChar          = ChrCR;
    defaultTc3.fvXTimeout       = FALSE;
    defaultTc3.fvXModeMultiple  = FALSE;
    defaultTc3.fvEOB            = ChrEOT;
    defaultTc3.fvEOBterm        = 2;
    defaultTc3.fvEOBCursorPos   = 3;
    defaultTc3.fvEOL            = ChrCR;
    defaultTc3.fvEOLTerm        = 1;
    defaultTc3.fvEOLCursorPos   = 2;
    defaultTc3.fvEchoplex       = TRUE;
    defaultTc3.fvFullASCII      = FALSE;
    defaultTc3.fvInFlowControl  = FALSE;
    defaultTc3.fvXInput         = FALSE;
    defaultTc3.fvInputDevice    = 0;
    defaultTc3.fvLI             = 0;
    defaultTc3.fvLIAuto         = FALSE;
    defaultTc3.fvLockKeyboard   = FALSE;
    defaultTc3.fvOutFlowControl = FALSE;
    defaultTc3.fvOutputDevice   = 1;
    defaultTc3.fvParity         = 2;
    defaultTc3.fvPG             = FALSE;
    defaultTc3.fvPL             = 24;
    defaultTc3.fvPW             = 80;
    defaultTc3.fvSpecialEdit    = FALSE;
    defaultTc3.fvTC             = Tc721;
    defaultTc3.fvXStickyTimeout = FALSE;
    defaultTc3.fvXModeDelimiter = 0;
    defaultTc3.fvDuplex         = FALSE;
    defaultTc3.fvSolicitInput   = FALSE;
    defaultTc3.fvCIDelay        = 0;
    defaultTc3.fvLIDelay        = 0;
    defaultTc3.fvHostNode       = npuSvmCouplerNode;
    defaultTc3.fvAutoConnect    = FALSE;
    defaultTc3.fvPriority       = 1;
    defaultTc3.fvUBL            = 7;
    defaultTc3.fvUBZ            = 100;
    defaultTc3.fvABL            = 2;
    defaultTc3.fvDBL            = 2;
    defaultTc3.fvDBZ            = 940;
    defaultTc3.fvRIC            = 0;
    defaultTc3.fvSDT            = 0;
    defaultTc3.fvDO             = 1;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Setup ANSI X3.64 defaults (VT100 terminal class 7)
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuTipSetupDefaultTc7(void)
    {
    defaultTc7.fvAbortBlock     = 'X' - 0x40;
    defaultTc7.fvBlockFactor    = 1;
    defaultTc7.fvBreakAsUser    = FALSE;
    defaultTc7.fvBS             = ChrBS;
    defaultTc7.fvUserBreak1     = 'P' - 0x40;
    defaultTc7.fvUserBreak2     = 'T' - 0x40;
    defaultTc7.fvEnaXUserBreak  = FALSE;
    defaultTc7.fvCI             = 0;
    defaultTc7.fvCIAuto         = FALSE;
    defaultTc7.fvCN             = 'X' - 0x40;
    defaultTc7.fvCursorPos      = TRUE;
    defaultTc7.fvCT             = '%';
    defaultTc7.fvXCharFlag      = FALSE;
    defaultTc7.fvXCnt           = 2043;
    defaultTc7.fvXChar          = ChrCR;
    defaultTc7.fvXTimeout       = FALSE;
    defaultTc7.fvXModeMultiple  = FALSE;
    defaultTc7.fvEOB            = ChrEOT;
    defaultTc7.fvEOBterm        = 2;
    defaultTc7.fvEOBCursorPos   = 3;
    defaultTc7.fvEOL            = ChrCR;
    defaultTc7.fvEOLTerm        = 1;
    defaultTc7.fvEOLCursorPos   = 2;
    defaultTc7.fvEchoplex       = TRUE;
    defaultTc7.fvFullASCII      = FALSE;
    defaultTc7.fvInFlowControl  = TRUE;
    defaultTc7.fvXInput         = FALSE;
    defaultTc7.fvInputDevice    = 0;
    defaultTc7.fvLI             = 0;
    defaultTc7.fvLIAuto         = FALSE;
    defaultTc7.fvLockKeyboard   = FALSE;
    defaultTc7.fvOutFlowControl = TRUE;
    defaultTc7.fvOutputDevice   = 1;
    defaultTc7.fvParity         = 2;
    defaultTc7.fvPG             = FALSE;
    defaultTc7.fvPL             = 24;
    defaultTc7.fvPW             = 80;
    defaultTc7.fvSpecialEdit    = FALSE;
    defaultTc7.fvTC             = TcX364;
    defaultTc7.fvXStickyTimeout = FALSE;
    defaultTc7.fvXModeDelimiter = 0;
    defaultTc7.fvDuplex         = FALSE;
    defaultTc7.fvSolicitInput   = FALSE;
    defaultTc7.fvCIDelay        = 0;
    defaultTc7.fvLIDelay        = 0;
    defaultTc7.fvHostNode       = npuSvmCouplerNode;
    defaultTc7.fvAutoConnect    = FALSE;
    defaultTc7.fvPriority       = 1;
    defaultTc7.fvUBL            = 7;
    defaultTc7.fvUBZ            = 100;
    defaultTc7.fvABL            = 2;
    defaultTc7.fvDBL            = 2;
    defaultTc7.fvDBZ            = 940;
    defaultTc7.fvRIC            = 0;
    defaultTc7.fvSDT            = 0;
    defaultTc7.fvDO             = 1;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Setup HASP Post defaults (terminal class 9)
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuTipSetupDefaultTc9(void)
    {
    defaultTc9.fvAbortBlock     = 0;
    defaultTc9.fvBlockFactor    = 0;
    defaultTc9.fvBreakAsUser    = FALSE;
    defaultTc9.fvBS             = 0;
    defaultTc9.fvUserBreak1     = ':';
    defaultTc9.fvUserBreak2     = ')';
    defaultTc9.fvEnaXUserBreak  = FALSE;
    defaultTc9.fvCI             = 0;
    defaultTc9.fvCIAuto         = FALSE;
    defaultTc9.fvCN             = '(';
    defaultTc9.fvCursorPos      = FALSE;
    defaultTc9.fvCT             = '%';
    defaultTc9.fvXCharFlag      = FALSE;
    defaultTc9.fvXCnt           = 0;
    defaultTc9.fvXChar          = 0;
    defaultTc9.fvXTimeout       = FALSE;
    defaultTc9.fvXModeMultiple  = FALSE;
    defaultTc9.fvEOB            = 0;
    defaultTc9.fvEOBterm        = 0;
    defaultTc9.fvEOBCursorPos   = 0;
    defaultTc9.fvEOL            = 0;
    defaultTc9.fvEOLTerm        = 0;
    defaultTc9.fvEOLCursorPos   = 0;
    defaultTc9.fvEchoplex       = FALSE;
    defaultTc9.fvFullASCII      = FALSE;
    defaultTc9.fvInFlowControl  = FALSE;
    defaultTc9.fvXInput         = FALSE;
    defaultTc9.fvInputDevice    = 0;
    defaultTc9.fvLI             = 0;
    defaultTc9.fvLIAuto         = FALSE;
    defaultTc9.fvLockKeyboard   = FALSE;
    defaultTc9.fvOutFlowControl = FALSE;
    defaultTc9.fvOutputDevice   = 0;
    defaultTc9.fvParity         = 0;
    defaultTc9.fvPG             = FALSE;
    defaultTc9.fvPL             = 0;
    defaultTc9.fvPW             = 80;
    defaultTc9.fvSpecialEdit    = FALSE;
    defaultTc9.fvTC             = TcHASP;
    defaultTc9.fvXStickyTimeout = FALSE;
    defaultTc9.fvXModeDelimiter = 0;
    defaultTc9.fvDuplex         = FALSE;
    defaultTc9.fvSolicitInput   = FALSE;
    defaultTc9.fvCIDelay        = 0;
    defaultTc9.fvLIDelay        = 0;
    defaultTc9.fvHostNode       = npuSvmCouplerNode;
    defaultTc9.fvAutoConnect    = FALSE;
    defaultTc9.fvPriority       = 1;
    defaultTc9.fvUBL            = 7;
    defaultTc9.fvUBZ            = 640;
    defaultTc9.fvABL            = 2;
    defaultTc9.fvDBL            = 2;
    defaultTc9.fvDBZ            = 640;
    defaultTc9.fvRIC            = 1;
    defaultTc9.fvSDT            = 0;
    defaultTc9.fvDO             = 1;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Setup HASP Pre defaults (terminal class 14)
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuTipSetupDefaultTc14(void)
    {
    defaultTc14.fvAbortBlock     = 0;
    defaultTc14.fvBlockFactor    = 0;
    defaultTc14.fvBreakAsUser    = FALSE;
    defaultTc14.fvBS             = 0;
    defaultTc14.fvUserBreak1     = ':';
    defaultTc14.fvUserBreak2     = ')';
    defaultTc14.fvEnaXUserBreak  = FALSE;
    defaultTc14.fvCI             = 0;
    defaultTc14.fvCIAuto         = FALSE;
    defaultTc14.fvCN             = '(';
    defaultTc14.fvCursorPos      = FALSE;
    defaultTc14.fvCT             = '%';
    defaultTc14.fvXCharFlag      = FALSE;
    defaultTc14.fvXCnt           = 0;
    defaultTc14.fvXChar          = 0;
    defaultTc14.fvXTimeout       = FALSE;
    defaultTc14.fvXModeMultiple  = FALSE;
    defaultTc14.fvEOB            = 0;
    defaultTc14.fvEOBterm        = 0;
    defaultTc14.fvEOBCursorPos   = 0;
    defaultTc14.fvEOL            = 0;
    defaultTc14.fvEOLTerm        = 0;
    defaultTc14.fvEOLCursorPos   = 0;
    defaultTc14.fvEchoplex       = FALSE;
    defaultTc14.fvFullASCII      = FALSE;
    defaultTc14.fvInFlowControl  = FALSE;
    defaultTc14.fvXInput         = FALSE;
    defaultTc14.fvInputDevice    = 0;
    defaultTc14.fvLI             = 0;
    defaultTc14.fvLIAuto         = FALSE;
    defaultTc14.fvLockKeyboard   = FALSE;
    defaultTc14.fvOutFlowControl = FALSE;
    defaultTc14.fvOutputDevice   = 0;
    defaultTc14.fvParity         = 0;
    defaultTc14.fvPG             = FALSE;
    defaultTc14.fvPL             = 0;
    defaultTc14.fvPW             = 80;
    defaultTc14.fvSpecialEdit    = FALSE;
    defaultTc14.fvTC             = TcHPRE;
    defaultTc14.fvXStickyTimeout = FALSE;
    defaultTc14.fvXModeDelimiter = 0;
    defaultTc14.fvDuplex         = FALSE;
    defaultTc14.fvSolicitInput   = FALSE;
    defaultTc14.fvCIDelay        = 0;
    defaultTc14.fvLIDelay        = 0;
    defaultTc14.fvHostNode       = npuSvmCouplerNode;
    defaultTc14.fvAutoConnect    = FALSE;
    defaultTc14.fvPriority       = 1;
    defaultTc14.fvUBL            = 7;
    defaultTc14.fvUBZ            = 640;
    defaultTc14.fvABL            = 2;
    defaultTc14.fvDBL            = 2;
    defaultTc14.fvDBZ            = 640;
    defaultTc14.fvRIC            = 1;
    defaultTc14.fvSDT            = 0;
    defaultTc14.fvDO             = 1;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Setup User Class 1 (TIELINE) defaults (terminal class 28)
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuTipSetupDefaultTc28(void)
    {
    defaultTc28.fvAbortBlock     = 0;
    defaultTc28.fvBlockFactor    = 0;
    defaultTc28.fvBreakAsUser    = FALSE;
    defaultTc28.fvBS             = 0;
    defaultTc28.fvUserBreak1     = ':';
    defaultTc28.fvUserBreak2     = ')';
    defaultTc28.fvEnaXUserBreak  = FALSE;
    defaultTc28.fvCI             = 0;
    defaultTc28.fvCIAuto         = FALSE;
    defaultTc28.fvCN             = '(';
    defaultTc28.fvCursorPos      = FALSE;
    defaultTc28.fvCT             = '%';
    defaultTc28.fvXCharFlag      = FALSE;
    defaultTc28.fvXCnt           = 0;
    defaultTc28.fvXChar          = 0;
    defaultTc28.fvXTimeout       = FALSE;
    defaultTc28.fvXModeMultiple  = FALSE;
    defaultTc28.fvEOB            = 0;
    defaultTc28.fvEOBterm        = 0;
    defaultTc28.fvEOBCursorPos   = 0;
    defaultTc28.fvEOL            = 0;
    defaultTc28.fvEOLTerm        = 0;
    defaultTc28.fvEOLCursorPos   = 0;
    defaultTc28.fvEchoplex       = FALSE;
    defaultTc28.fvFullASCII      = FALSE;
    defaultTc28.fvInFlowControl  = FALSE;
    defaultTc28.fvXInput         = FALSE;
    defaultTc28.fvInputDevice    = 0;
    defaultTc28.fvLI             = 0;
    defaultTc28.fvLIAuto         = FALSE;
    defaultTc28.fvLockKeyboard   = FALSE;
    defaultTc28.fvOutFlowControl = FALSE;
    defaultTc28.fvOutputDevice   = 0;
    defaultTc28.fvParity         = 0;
    defaultTc28.fvPG             = FALSE;
    defaultTc28.fvPL             = 0;
    defaultTc28.fvPW             = 80;
    defaultTc28.fvSpecialEdit    = FALSE;
    defaultTc28.fvTC             = TcUTC1;
    defaultTc28.fvXStickyTimeout = FALSE;
    defaultTc28.fvXModeDelimiter = 0;
    defaultTc28.fvDuplex         = FALSE;
    defaultTc28.fvSolicitInput   = FALSE;
    defaultTc28.fvCIDelay        = 0;
    defaultTc28.fvLIDelay        = 0;
    defaultTc28.fvHostNode       = npuSvmCouplerNode;
    defaultTc28.fvAutoConnect    = FALSE;
    defaultTc28.fvPriority       = 1;
    defaultTc28.fvUBL            = 7;
    defaultTc14.fvUBZ            = 640;
    defaultTc28.fvABL            = 2;
    defaultTc28.fvDBL            = 2;
    defaultTc28.fvDBZ            = 640;
    defaultTc28.fvRIC            = 1;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Setup User Class 2 (NJE) defaults (terminal class 29)
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuTipSetupDefaultTc29(void)
    {
    defaultTc29.fvAbortBlock     = 0;
    defaultTc29.fvBlockFactor    = 0;
    defaultTc29.fvBreakAsUser    = FALSE;
    defaultTc29.fvBS             = 0;
    defaultTc29.fvUserBreak1     = ':';
    defaultTc29.fvUserBreak2     = ')';
    defaultTc29.fvEnaXUserBreak  = FALSE;
    defaultTc29.fvCI             = 0;
    defaultTc29.fvCIAuto         = FALSE;
    defaultTc29.fvCN             = '(';
    defaultTc29.fvCursorPos      = FALSE;
    defaultTc29.fvCT             = '%';
    defaultTc29.fvXCharFlag      = FALSE;
    defaultTc29.fvXCnt           = 0;
    defaultTc29.fvXChar          = 0;
    defaultTc29.fvXTimeout       = FALSE;
    defaultTc29.fvXModeMultiple  = FALSE;
    defaultTc29.fvEOB            = 0;
    defaultTc29.fvEOBterm        = 0;
    defaultTc29.fvEOBCursorPos   = 0;
    defaultTc29.fvEOL            = 0;
    defaultTc29.fvEOLTerm        = 0;
    defaultTc29.fvEOLCursorPos   = 0;
    defaultTc29.fvEchoplex       = FALSE;
    defaultTc29.fvFullASCII      = FALSE;
    defaultTc29.fvInFlowControl  = FALSE;
    defaultTc29.fvXInput         = FALSE;
    defaultTc29.fvInputDevice    = 0;
    defaultTc29.fvLI             = 0;
    defaultTc29.fvLIAuto         = FALSE;
    defaultTc29.fvLockKeyboard   = FALSE;
    defaultTc29.fvOutFlowControl = FALSE;
    defaultTc29.fvOutputDevice   = 0;
    defaultTc29.fvParity         = 0;
    defaultTc29.fvPG             = FALSE;
    defaultTc29.fvPL             = 0;
    defaultTc29.fvPW             = 80;
    defaultTc29.fvSpecialEdit    = FALSE;
    defaultTc29.fvTC             = TcUTC2;
    defaultTc29.fvXStickyTimeout = FALSE;
    defaultTc29.fvXModeDelimiter = 0;
    defaultTc29.fvDuplex         = FALSE;
    defaultTc29.fvSolicitInput   = FALSE;
    defaultTc29.fvCIDelay        = 0;
    defaultTc29.fvLIDelay        = 0;
    defaultTc29.fvHostNode       = npuSvmCouplerNode;
    defaultTc29.fvAutoConnect    = FALSE;
    defaultTc29.fvPriority       = 1;
    defaultTc29.fvUBL            = 7;
    defaultTc14.fvUBZ            = 640;
    defaultTc29.fvABL            = 2;
    defaultTc29.fvDBL            = 2;
    defaultTc29.fvDBZ            = 640;
    defaultTc29.fvRIC            = 1;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Log terminal parameter setting
**
**  Parameters:     Name        Description.
**                  tp          pointer to TCB
**                  fn          parameter orginal
**                  fv          parameter value
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
#if DEBUG
static void npuTipLogFnFv(Tcb *tp, u8 fn, u8 fv)
    {
    char kbuf[5];
    char *kw;

    sprintf(kbuf, "<%02x>", fn);
    kw = kbuf;
    switch (fn)
        {
    case FnTdAbortBlock:
        kw = "AbortBlockChar";
        break;

    case FnTdBlockFactor:
        kw = "BlockingFactor";
        break;

    case FnTdBreakAsUser:
        kw = "BreakAsUser";
        break;

    case FnTdBS:
        kw = "BS";
        break;

    case FnTdUserBreak1:
        kw = "UserBreak1";
        break;

    case FnTdUserBreak2:
        kw = "UserBreak2";
        break;

    case FnTdEnaXUserBreak:
        kw = "EnaXUserBreak";
        break;

    case FnTdCI:
        kw = "CI";
        break;

    case FnTdCIAuto:
        kw = "CIAuto";
        break;

    case FnTdCN:
        kw = "CN";
        break;

    case FnTdCursorPos:
        kw = "CursorPos";
        break;

    case FnTdCT:
        kw = "CT";
        break;

    case FnTdXCharFlag:
        kw = "XCharFlag";
        break;

    case FnTdXCntMSB:
        kw = "XCntMSB";
        break;

    case FnTdXCntLSB:
        kw = "XCntLSB";
        break;

    case FnTdXChar:
        kw = "XChar";
        break;

    case FnTdXTimeout:
        kw = "XTimeout";
        break;

    case FnTdXModeMultiple:
        kw = "XModeMultiple";
        break;

    case FnTdEOB:
        kw = "EOB";
        break;

    case FnTdEOBterm:
        kw = "EOBterm";
        break;

    case FnTdEOBCursorPos:
        kw = "EOBCursorPos";
        break;

    case FnTdEOL:
        kw = "EOL";
        break;

    case FnTdEOLTerm:
        kw = "EOLTerm";
        break;

    case FnTdEOLCursorPos:
        kw = "EOLCursorPos";
        break;

    case FnTdEchoplex:
        kw = "EP";
        break;

    case FnTdFullASCII:
        kw = "FullASCII";
        break;

    case FnTdInFlowControl:
        kw = "InFlowControl";
        break;

    case FnTdXInput:
        kw = "XInput";
        break;

    case FnTdInputDevice:
        kw = "InputDevice";
        break;

    case FnTdLI:
        kw = "LI";
        break;

    case FnTdLIAuto:
        kw = "LIAuto";
        break;

    case FnTdLockKeyboard:
        kw = "LockKeyboard";
        break;

    case FnTdOutFlowControl:
        kw = "OutFlowControl";
        break;

    case FnTdOutputDevice:
        kw = "OutputDevice";
        break;

    case FnTdParity:
        kw = "PA";
        break;

    case FnTdPG:
        kw = "PG";
        break;

    case FnTdPL:
        kw = "PL";
        break;

    case FnTdPW:
        kw = "PW";
        break;

    case FnTdSpecialEdit:
        kw = "SpecialEdit";
        break;

    case FnTdTC:
        kw = "TC";
        break;

    case FnTdXStickyTimeout:
        kw = "XStickyTimeout";
        break;

    case FnTdXModeDelimiter:
        kw = "XModeDelimiter";
        break;

    case FnTdDuplex:
        kw = "Duplex";
        break;

    case FnTdUBZMSB:
        kw = "UBZ(MSB)";
        break;

    case FnTdUBZLSB:
        kw = "UBZ(LSB)";
        break;

    case FnTdSolicitInput:
        kw = "SolicitInput";
        break;

    case FnTdCIDelay:
        kw = "CIDelay";
        break;

    case FnTdLIDelay:
        kw = "LIDelay";
        break;

    case FnTdHostNode:
        kw = "HN";
        break;

    case FnTdAutoConnect:
        kw = "AutoConnect";
        break;

    case FnTdPriority:
        kw = "Priority";
        break;

    case FnTdUBL:
        kw = "UBL";
        break;

    case FnTdABL:
        kw = "ABL";
        break;

    case FnTdDBL:
        kw = "DBL";
        break;

    case FnTdDBZMSB:
        kw = "DBZ(MSB)";
        break;

    case FnTdDBZLSB:
        kw = "DBZ(LSB)";
        break;

    case FnTdRIC:
        kw = "RIC";
        break;

    case FnSDT:
        kw = "SDT";
        break;

    case FnDO1:
        kw = "DO";
        break;

    default:
        sprintf(kbuf, "<%02x>", fn);
        kw = kbuf;
        break;
        }
    fprintf(npuTipLog, "Connection %u set terminal parameter %s=<%02x>\n", tp->cn, kw, fv);
    }

#endif

/*---------------------------  End Of File  ------------------------------*/
