/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**                2019-2026, Kevin Jordan
**
**  Name: npu_bip.c
**
**  Description:
**      Perform emulation of the Block Interface Protocol (BIP) in an NPU
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
#include "cci.h"

/*
**  -----------------
**  Private Constants
**  -----------------
*/
#define NumBuffs    1000

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
static bool npuBipCopyDownlineBlock(NpuBuffer *bp);
static bool npuBipCopyUplineBlock(NpuBuffer *bp);

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
static enum
    {
    BipIdle,
    BipDownSvm,
    BipDownDataLow,
    BipDownDataHigh,
    }
bipState = BipIdle;

/*
**  Function tables to interface to either CCP, CCI, or MDI functions,
**  indexed by NetDevType
*/
static void (*hipNotifyDownlineCopied[])(void) =
    {
    npuHipNotifyDownlineCopied,
    cciHipNotifyDownlineCopied,
    mdiHipNotifyDownlineCopied
    };

static bool (*hipStartDownlineTransfer[])(NpuBuffer *np) =
    {
    npuHipStartDownlineTransfer,
    cciHipStartDownlineTransfer,
    mdiHipStartDownlineTransfer
    };

static bool (*hipStartUplineTransfer[])(NpuBuffer *np) =
    {
    npuHipStartUplineTransfer,
    cciHipStartUplineTransfer,
    mdiHipStartUplineTransfer
    };

static void (*svmProcessBuffer[])(NpuBuffer *np) =
    {
    npuSvmProcessBuffer,
    cciSvmProcessBuffer,
    npuSvmProcessBuffer
    };

static void (*tipProcessBuffer[])(NpuBuffer *np, int priority) =
    {
    npuTipProcessBuffer,
    cciTipProcessBuffer,
    npuTipProcessBuffer
    };

/*
 **--------------------------------------------------------------------------
 **
 **  Public Functions
 **
 **--------------------------------------------------------------------------
 */

/*--------------------------------------------------------------------------
**  Purpose:        Initialize BIP.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuBipInit(void)
    {
    NpuBuffer *bp;
    NpuBuffer *np;
    int       count;

    /*
    **  Allocate data buffer pool.
    */
    netDevice->bufCount = 0;
    netDevice->bufPool  = calloc(NumBuffs, sizeof(NpuBuffer));
    if (netDevice->bufPool == NULL)
        {
        logDtError(LogErrorLocation, "Failed to allocate NPU data buffer pool\n");
        exit(1);
        }
    netDevice->buffers = netDevice->bufPool;

    /*
    **  Link buffers into pool and link in data.
    */
    bp = netDevice->bufPool;
    for (count = NumBuffs - 1; count > 0; count--)
        {
        /*
        **  Link to next buffer.
        */
        np       = bp + 1;
        bp->next = np;
        bp       = np;
        }

    bp->next = NULL;

    /*
    **  Allocate upline buffer queue.
    */
    netDevice->uplineQueue = calloc(1, sizeof(NpuQueue));
    if (netDevice->uplineQueue == NULL)
        {
        logDtError(LogErrorLocation, "(npu_bip) Failed to allocate NPU buffer queue\n");
        exit(1);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Reset BIP.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuBipReset(void)
    {
    if (netDevice->uplineBuffer != NULL)
        {
        npuBipBufRelease(netDevice->uplineBuffer);
        }

    while ((netDevice->uplineBuffer = npuBipQueueExtract(netDevice->uplineQueue)) != NULL)
        {
        npuBipBufRelease(netDevice->uplineBuffer);
        }

    netDevice->uplineBuffer = NULL;

    if (netDevice->downlineBuffer != NULL)
        {
        npuBipBufRelease(netDevice->downlineBuffer);
        netDevice->downlineBuffer = NULL;
        }

    bipState = BipIdle;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Return current buffer count.
**
**  Parameters:     Name        Description.
**
**  Returns:        Current buffer count.
**
**------------------------------------------------------------------------*/
int npuBipBufCount(void)
    {
    return netDevice->bufCount;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Allocate NPU buffer from pool.
**
**  Parameters:     Name        Description.
**
**  Returns:        Pointer to newly allocated buffer or NULL if pool
**                  is empty.
**
**------------------------------------------------------------------------*/
NpuBuffer *npuBipBufGet(void)
    {
    NpuBuffer *bp;

    /*
    **  Allocate buffer from pool.
    */
    bp = netDevice->bufPool;
    if (bp != NULL)
        {
        /*
        **  Unlink allocated buffer.
        */
        netDevice->bufPool   = bp->next;
        netDevice->bufCount += 1;

        /*
        **  Initialise buffer.
        */
        bp->next       = NULL;
        bp->offset     = 0;
        bp->numBytes   = 0;
        bp->blockSeqNo = 0;
#if DEBUG
        memset(bp->data, 0, MaxBuffer);
#endif
        }
    else
        {
        npuLogMessage("(npu_bip) Out of buffers");
        fputs("(npu_bip) Fatal error: Out of buffers\n", stderr);
#if DEBUG
            {
            /*
             *  Dump metadata and first 32 bytes of each buffer to stderr
             */
            u8  *data;
            int i, j, k;

            for (i = 1, bp = netDevice->buffers; i <= NumBuffs; i++, bp++)
                {
                logDtError(LogErrorLocation, "\nBuffer %d, offset=%u, numBytes=%u, blockSeqNo=%u\n",
                           i, bp->offset, bp->numBytes, bp->blockSeqNo);
                data = bp->data;
                for (j = 0; j < 2; j++)
                    {
                    for (k = 0; k < 16; k++)
                        {
                        fprintf(stderr, "%02X ", *data++);
                        }
                    data -= 16;
                    for (k = 0; k < 16; k++)
                        {
                        if ((*data >= 0x20) && (*data < 0x7f))
                            {
                            fprintf(stderr, "%c", *data);
                            }
                        else
                            {
                            fputc('.', stderr);
                            }
                        data++;
                        }
                    fputc('\n', stderr);
                    }
                }
            data  = NULL;
            *data = 0; // force SEGV to facilitate debugger usage
            }
#endif
        exit(1);
        }

    return (bp);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Report whether the network is busy.
**
**  Parameters:     Name        Description.
**
**  Returns:        TRUE if the network is busy.
**
**------------------------------------------------------------------------*/
bool npuBipIsBusy(void)
    {
    u8 i;

    for (i = 0; i < netDevCount; i++)
        {
        if (netDevices[i].bufCount >= (int)idleNetBufs)
            {
            return TRUE;
            }
        }
    return FALSE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Free NPU buffer back to pool.
**
**  Parameters:     Name        Description.
**                  bp          Pointer to NPU buffer.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuBipBufRelease(NpuBuffer *bp)
    {
    if (bp != NULL)
        {
        /*
        **  Link buffer back into the pool.
        */
        bp->next             = netDevice->bufPool;
        netDevice->bufPool   = bp;
        netDevice->bufCount -= 1;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Enqueue a buffer at the tail of a queue.
**
**  Parameters:     Name        Description.
**                  bp          Pointer to NPU buffer.
**                  queue       Pointer to queue.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuBipQueueAppend(NpuBuffer *bp, NpuQueue *queue)
    {
    if (bp != NULL)
        {
        if (queue->first == NULL)
            {
            queue->first = bp;
            }
        else
            {
            queue->last->next = bp;
            }

        queue->last = bp;
        bp->next    = NULL;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Enqueue a buffer at the head of a queue.
**
**  Parameters:     Name        Description.
**                  bp          Pointer to NPU buffer.
**                  queue       Pointer to queue.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuBipQueuePrepend(NpuBuffer *bp, NpuQueue *queue)
    {
    if (bp != NULL)
        {
        if (queue->first == NULL)
            {
            queue->last = bp;
            }

        bp->next     = queue->first;
        queue->first = bp;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Extract a buffer from the head of the queue.
**
**  Parameters:     Name        Description.
**                  queue       Pointer to queue.
**
**  Returns:        Pointer to a buffer or NULL if queue is empty.
**
**------------------------------------------------------------------------*/
NpuBuffer *npuBipQueueExtract(NpuQueue *queue)
    {
    NpuBuffer *bp;

    bp  = queue->first;
    if (bp != NULL)
        {
        queue->first = bp->next;
        if (queue->first == NULL)
            {
            queue->last = NULL;
            }
        }

    return bp;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Get a pointer to the last buffer in queue (but don't
**                  extract it).
**
**  Parameters:     Name        Description.
**                  queue       Pointer to queue.
**
**  Returns:        Pointer to a buffer or NULL if queue is empty.
**
**------------------------------------------------------------------------*/
NpuBuffer *npuBipQueueGetLast(NpuQueue *queue)
    {
    return (queue->last);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Determine if queue has anything in it.
**
**  Parameters:     Name        Description.
**                  queue       Pointer to queue.
**
**  Returns:        TRUE if queue is empty, FALSE otherwise.
**                  queue is empty.
**
**------------------------------------------------------------------------*/
bool npuBipQueueNotEmpty(NpuQueue *queue)
    {
    return (queue->first != NULL);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Respond to service message order word
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuBipNotifyServiceMessage(void)
    {
    netDevice->downlineBuffer = npuBipBufGet();
    if (npuBipCopyDownlineBlock(netDevice->downlineBuffer))
        {
        bipState = BipDownSvm;
        }
    else
        {
        npuBipBufRelease(netDevice->downlineBuffer);
        netDevice->downlineBuffer = NULL;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Respond to output order word.
**
**  Parameters:     Name        Description.
**                  priority    output priority
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuBipNotifyData(int priority)
    {
    netDevice->downlineBuffer = npuBipBufGet();
    if (npuBipCopyDownlineBlock(netDevice->downlineBuffer))
        {
        bipState = BipDownDataLow + priority;
        }
    else
        {
        npuBipBufRelease(netDevice->downlineBuffer);
        netDevice->downlineBuffer = NULL;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Iniitiate sending upline block, if one is available.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuBipTryUplineBlock(void)
    {
    /*
    **  Check if any more upline buffer is pending and send if necessary.
    */
    if (netDevice->uplineBuffer != NULL && netDevice->uplineData.isReady == FALSE)
        {
        npuBipCopyUplineBlock(netDevice->uplineBuffer);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Process downline message.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuBipNotifyDownlineReceived(void)
    {
    NpuBuffer *bp;
    u8        dn;

    /*
    **  BIP loses ownership of the downline buffer.
    */
    bp                        = netDevice->downlineBuffer;
    netDevice->downlineBuffer = NULL;
    dn                        = bp->data[BlkOffDN];

    if (dn == netDevice->npuNode)
        {
        /*
        **  Hand over the buffer to SVM or TIP.
        */
        switch (bipState)
            {
        default:
            break;

        case BipDownSvm:
            svmProcessBuffer[netDevice->type](bp);
            break;

        case BipDownDataLow:
            tipProcessBuffer[netDevice->type](bp, 0);
            break;

        case BipDownDataHigh:
            tipProcessBuffer[netDevice->type](bp, 1);
            break;
            }
        }
    else if (dn == netDevice->cdcnetNode)
        {
        cdcnetProcessDownlineData(bp);
        }
    else
        {
        npuLipProcessDownlineData(bp);
        }

    bipState = BipIdle;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Abort downline message.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuBipAbortDownlineReceived(void)
    {
    /*
    **  Free buffer and reset state.
    */
    npuBipBufRelease(netDevice->downlineBuffer);
    netDevice->downlineBuffer = NULL;
    bipState                  = BipIdle;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Request upline transfer.
**
**  Parameters:     Name        Description.
**                  bp          pointer to NPU buffer to be transferred
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuBipRequestUplineTransfer(NpuBuffer *bp)
    {
    if (netDevice->uplineBuffer != NULL)
        {
        /*
        **  Upline buffer pending, so queue this one for later.
        */
        npuBipQueueAppend(bp, netDevice->uplineQueue);

        return;
        }

    /*
    **  Send this block now.
    */
    netDevice->uplineBuffer = bp;
    if (bipState == BipIdle)
        {
        npuBipTryUplineBlock();
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Request upline transfer of canned message.
**
**                  The message is copied unmodified to an NpuBuffer.
**
**  Parameters:     Name        Description.
**                  msg         pointer to canned message data
**                  msgSize     number of bytes in the message
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuBipRequestUplineCanned(u8 *msg, int msgSize)
    {
    NpuBuffer *bp;

    bp           = npuBipBufGet();
    bp->numBytes = (u16)msgSize;
    memcpy(bp->data, msg, bp->numBytes);
    npuBipRequestUplineTransfer(bp);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Request upline transfer of static message 
**
**                  The message is copied to an NpuBuffer and the destination
**                  and source node numbers are inserted.
**
**  Parameters:     Name        Description.
**                  msg         pointer to static message data
**                  msgSize     number of bytes in the message
**                  idx         message index at which to store a value
**                  value       value to store at message index
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuBipRequestUplineStatic(u8 *msg, int msgSize, int idx, u8 value)
    {
    NpuBuffer *bp;

    bp           = npuBipBufGet();
    bp->numBytes = (u16)msgSize;
    memcpy(bp->data, msg, bp->numBytes);
    bp->data[BlkOffDN] = netDevice->couplerNode;
    bp->data[BlkOffSN] = netDevice->npuNode;
    if (idx != 0)
        {
        bp->data[idx] = value;
        }
    npuBipRequestUplineTransfer(bp);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Respond to completion of upline transfer.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuBipNotifyUplineSent(void)
    {
    /*
    **  Transfer finished, so release the buffer.
    */
    npuBipBufRelease(netDevice->uplineBuffer);

    /*
    **  Check if any more upline queued and send if necessary.
    */
    netDevice->uplineBuffer = npuBipQueueExtract(netDevice->uplineQueue);
    npuBipTryUplineBlock();
    }

/*
 **--------------------------------------------------------------------------
 **
 **  Private Functions
 **
 **--------------------------------------------------------------------------
 */

/*--------------------------------------------------------------------------
**  Purpose:        Copy downline block from channel buffer to NPU buffer.
**
**  Parameters:     Name        Description.
**                  bp          pointer to NPU buffer.
**
**  Returns:        TRUE if buffer can be copied, FALSE otherwise.
**
**------------------------------------------------------------------------*/
static bool npuBipCopyDownlineBlock(NpuBuffer *bp)
    {
    if (hipStartDownlineTransfer[netDevice->type](bp) == FALSE)
        {
        return FALSE;
        }

    bp->offset     = 0;
    bp->numBytes   = netDevice->downlineData.numBytes;
    bp->blockSeqNo = netDevice->downlineData.blockSeqNo;
    memcpy(bp->data, netDevice->downlineData.data, netDevice->downlineData.numBytes);
    netDevice->downlineData.offset   = 0;
    netDevice->downlineData.numBytes = 0;
    netDevice->downlineData.isReady  = FALSE;

    hipNotifyDownlineCopied[netDevice->type]();

    return TRUE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Copy NPU buffer to channel buffer
**
**  Parameters:     Name        Description.
**                  bp          pointer to NPU buffer.
**
**  Returns:        TRUE if buffer can be copied, FALSE otherwise.
**
**------------------------------------------------------------------------*/
static bool npuBipCopyUplineBlock(NpuBuffer *bp)
    {
    if (hipStartUplineTransfer[netDevice->type](bp) == FALSE)
        {
        return FALSE;
        }

    memcpy(netDevice->uplineData.data, bp->data, bp->numBytes);
    netDevice->uplineData.offset     = 0;
    netDevice->uplineData.numBytes   = bp->numBytes;
    netDevice->uplineData.blockSeqNo = bp->blockSeqNo;
    netDevice->uplineData.isReady    = TRUE;

    return TRUE;
    }

/*---------------------------  End Of File  ------------------------------*/
