/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2023, Tom Hunter, Paul Koning, Joachim Siebold
**
**  Name: cci_async.c
**
**  Description:
**      Perform emulation of the ASYNC TIP in an NPU consisting of a
**      CDC 2550 HCP running CCI.
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

/*
** Note: if DEBUG is enabled here, it must also be activated in npu_async.c
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
#if defined(__APPLE__)
#include <execinfo.h>
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
#define MaxIvtData    100
#define ChrBlank      0x20
#define ChrPercent    0x25

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

/*
**  ----------------
**  Public Variables
**  ----------------
*/
extern FILE *npuAsyncLog;

/*
**  -----------------
**  Private Variables
**  -----------------
*/
static Tcb *npuTp;

static u8 fcSingleSpace[] = "\r\n";
static u8 fcTripleSpace[] = "\r\n\n\n";
static u8 fcBol[]         = "\r";

static u8  netBEL[]  = { ChrBEL };
static u8  netLF[]   = { ChrLF };
static u8  netCR[]   = { ChrCR };
static u8  netCRLF[] = { ChrCR, ChrLF };
static u8  netBS[]   = { ChrBS, ChrBlank, ChrBS };
static u8  echoBuffer[1000];
static u8  *echoPtr = echoBuffer;
static int echoLen;

/*
 **--------------------------------------------------------------------------
 **
 **  Public Functions
 **
 **--------------------------------------------------------------------------
 */
/*--------------------------------------------------------------------------
**  Purpose:        Process downline data from host for CCI
**
**  Parameters:     Name        Description.
**                  tp          pointer to TCB
**                  bp          buffer with downline data message.
**                  last        last buffer.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cciAsyncProcessDownlineData(Tcb *tp, NpuBuffer *bp, bool last)
    {
    u8  *blk = bp->data + BlkOffData;
    int len  = bp->numBytes - BlkOffData;
    u8  dbc;
    u8  i;

    npuTp = tp;

    /*
    **  Extract Data Block Clarifier settings.
    */
    dbc = *blk++;

    len -= 1;

    /*
    ** skip over the timestamp and level bytes
    */
    blk += 3;
    len -= 3;

    /*
    ** process CCI dbc
    */
    switch (dbc & 0x07)
        {
    case 0:
    case 2:
    case 3:
        npuNetSend(npuTp, fcSingleSpace, sizeof(fcSingleSpace) - 1);
        break;

    case 1:
        npuNetSend(npuTp, fcTripleSpace, sizeof(fcTripleSpace) - 1);
        break;

    case 4:
        npuNetSend(npuTp, fcBol, sizeof(fcBol) - 1);
        break;

    default:
        break;
        }

    /*
    ** remove parity bit
    */
    for (i = 0; i < len; i++)
        {
        blk[i] &= Mask7;
        }

    /*
    ** remove end of record
    */
    if (blk[len - 1] == ':')
        {
        len--;
        }
#if DEBUG
    fprintf(npuAsyncLog, "Send to terminal : ");
    for (int i = 0; i < len ; i++)
        {
        fprintf(npuAsyncLog, "%c", blk[i]);
        }
    fprintf(npuAsyncLog, "\n");
#endif

    /*
    **  Process data.
    */
    npuNetSend(npuTp, blk, len);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Process upline data from terminal for a CCI dumb terminal
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cciAsyncProcessUplineNormal(Tcb *tp)
    {
    u8  *dp;
    int len;
    u8  ch;
    Pcb *pcbp;

    pcbp = tp->pcbp;
    dp   = pcbp->inputData;
    len  = pcbp->inputCount;

    /*
    **  Process input for CCI
    */
    tp->inBuf[BlkOffDbc] = 5;   // non-transparent data

    while (len--)
        {
        ch = *dp++ & Mask7;

        /*
        **  Ignore the following characters.
        */
        switch (ch)
            {
        case ChrNUL:
        case ChrLF:
        case ChrDEL:
            continue;
            }

        if (ch == tp->params.fvBS)
            {
            /*
            **  Process backspace.
            */
            if (tp->inBufPtr > tp->inBufStart)
                {
                tp->inBufPtr -= 1;
                npuNetSend(tp, netBS, sizeof(netBS));
                }
            else
                {
                /*
                **  Beep when trying to go past the start of line.
                */
                npuNetSend(tp, netBEL, 1);
                }

            continue;
            }

        /*
        **  Echo characters.
        */
        *echoPtr++ = ch;
        echoLen    = (int)(echoPtr - echoBuffer);
        if (echoLen)
            {
            npuNetSend(tp, echoBuffer, echoLen);
            echoPtr = echoBuffer;
            }

        if (ch == tp->params.fvEOL)
            {
            /*
            **  EOL entered - send the input upline.
            */
            cciTipSendMsg(tp, (int)(tp->inBufPtr - tp->inBuf));
#if DEBUG
            fprintf(npuAsyncLog, "Port %02x: send upline normal data for %.7s, size %ld\n",
                    pcbp->claPort, tp->termName, tp->inBufPtr - tp->inBuf);
#endif
            cciTipInputReset(tp);
            tp->lastOpWasInput = TRUE;

            /*
            **  Perform cursor positioning.
            */
            if (tp->dbcNoCursorPos)
                {
                tp->dbcNoCursorPos = FALSE;
                }
            else
                {
                if (tp->params.fvCursorPos)
                    {
                    switch (tp->params.fvEOLCursorPos)
                        {
                    case 0:
                        break;

                    case 1:
                        npuNetSend(tp, netCR, sizeof(netCR));
                        break;

                    case 2:
                        npuNetSend(tp, netLF, sizeof(netLF));
                        break;

                    case 3:
                        npuNetSend(tp, netCRLF, sizeof(netCRLF));
                        break;
                        }
                    }
                }

            continue;
            }

        if (ch == tp->params.fvBS)
            {
            /*
            **  Process backspace.
            */
            if (tp->inBufPtr > tp->inBufStart)
                {
                tp->inBufPtr -= 1;
                *echoPtr++    = ' ';
                *echoPtr++    = tp->params.fvBS;
                }
            else
                {
                /*
                **  Beep when trying to go past the start of line.
                */
                npuNetSend(tp, netBEL, 1);
                }

            continue;
            }

        /*
        **  Store the character for later transmission.
        */
        *tp->inBufPtr++ = ch;

        if (tp->inBufPtr - tp->inBufStart >= (tp->params.fvBlockFactor * MaxIvtData))
            {
            /*
            **  Send long lines.
            */
            cciTipSendMsg(tp, (int)(tp->inBufPtr - tp->inBuf));
#if DEBUG
            fprintf(npuAsyncLog, "Port %02x: send upline long normal data for %.7s, size %ld\n",
                    pcbp->claPort, tp->termName, tp->inBufPtr - tp->inBuf);
#endif
            cciTipInputReset(tp);
            }
        }
    }
