/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
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
#include "const.h"
#include "types.h"
#include "proto.h"
#include "npu.h"

/*
**  -----------------
**  Private Constants
**  -----------------
*/
#define NumBuffs        1000

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

/*
**  -----------------
**  Private Variables
**  -----------------
*/
static NpuBuffer *buffers = NULL;
static NpuBuffer *bufPool = NULL;
static int bufCount = 0;

static NpuBuffer *bipUplineBuffer = NULL;
static NpuQueue *bipUplineQueue;

static NpuBuffer *bipDownlineBuffer = NULL;

static enum
    {
    BipIdle,
    BipDownSvm,
    BipDownDataLow,
    BipDownDataHigh,
    } bipState = BipIdle;

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
    int count;

    /*
    **  Allocate data buffer pool.
    */
    bufCount = NumBuffs;
    buffers = bufPool = calloc(NumBuffs, sizeof(NpuBuffer));
    if (bufPool == NULL)
        {
        fprintf(stderr, "Failed to allocate NPU data buffer pool\n");
        exit(1);
        }

    /*
    **  Link buffers into pool and link in data.
    */
    bp = bufPool;
    for (count = NumBuffs - 1; count > 0; count--)
        {
        /*
        **  Link to next buffer.
        */
        np = bp + 1;
        bp->next = np;
        bp = np;
        }

    bp->next = NULL;

    /*
    **  Allocate upline buffer queue.
    */
    bipUplineQueue = calloc(1, sizeof(NpuQueue));
    if (bipUplineQueue == NULL)
        {
        fprintf(stderr, "Failed to allocate NPU buffer queue\n");
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
    if (bipUplineBuffer != NULL)
        {
        npuBipBufRelease(bipUplineBuffer);
        }

    while ((bipUplineBuffer = npuBipQueueExtract(bipUplineQueue)) != NULL)
        {
        npuBipBufRelease(bipUplineBuffer);
        }

    bipUplineBuffer = NULL;

    if (bipDownlineBuffer != NULL)
        {
        npuBipBufRelease(bipDownlineBuffer);
        bipDownlineBuffer = NULL;
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
    return (bufCount);
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
    bp = bufPool;
    if (bp != NULL)
        {
        /*
        **  Unlink allocated buffer.
        */
        bufPool = bp->next;
        bufCount -= 1;

        /*
        **  Initialise buffer.
        */
        bp->next = NULL;
        bp->offset = 0;
        bp->numBytes = 0;
        bp->blockSeqNo = 0;
#if DEBUG
        memset(bp->data, 0, MaxBuffer);
#endif
        }
    else
        {
        npuLogMessage("BIP: Out of buffers");
        fprintf(stderr, "Fatal error: BIP: Out of buffers\n");
#if DEBUG
            {
            /*
             *  Dump metadata and first 32 bytes of each buffer to stderr
             */
            u8 *data;
            int i,j,k;

            for (i = 1, bp = buffers; i <= NumBuffs; i++, bp++)
                {
                fprintf(stderr, "\nBuffer %d, offset=%u, numBytes=%u, blockSeqNo=%u\n",
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
                        if (*data >= 0x20 && *data < 0x7f)
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
            data = NULL;
            *data = 0; // force SEGV to facilitate debugger usage
            }
#endif
        exit(1);
        }

    return(bp);
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
        bp->next = bufPool;
        bufPool = bp;
        bufCount += 1;
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
        bp->next = NULL;
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

        bp->next = queue->first;
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
    NpuBuffer *bp = queue->first;

    if (bp != NULL)
        {
        queue->first = bp->next;
        if (queue->first == NULL)
            {
            queue->last = NULL;
            }
        }

    return(bp);
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
    return(queue->last);
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
    return(queue->first != NULL);
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
    bipDownlineBuffer = npuBipBufGet();
    if (npuHipDownlineBlock(bipDownlineBuffer))
        {
        bipState = BipDownSvm;
        }
    else
        {
        npuBipBufRelease(bipDownlineBuffer);
        bipDownlineBuffer = NULL;
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
    bipDownlineBuffer = npuBipBufGet();
    if (npuHipDownlineBlock(bipDownlineBuffer))
        {
        bipState = BipDownDataLow + priority;
        }
    else
        {
        npuBipBufRelease(bipDownlineBuffer);
        bipDownlineBuffer = NULL;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Respond to input retry order word.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuBipRetryInput(void)
    {
    /*
    **  Check if any more upline buffer is pending and send if necessary.
    */
    if (bipUplineBuffer != NULL)
        {
        npuHipUplineBlock(bipUplineBuffer);
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
    NpuBuffer *bp = bipDownlineBuffer;
    u8 dn;

    /*
    **  BIP loses ownership of the downline buffer.
    */
    bipDownlineBuffer = NULL;
    dn = bp->data[BlkOffDN];

    if (dn == npuSvmNpuNode)
        {
        /*
        **  Hand over the buffer to SVM or TIP.
        */
        switch (bipState)
            {
        case BipDownSvm:
            npuSvmProcessBuffer(bp);
            break;

        case BipDownDataLow:
            npuTipProcessBuffer(bp, 0);
            break;

        case BipDownDataHigh:
            npuTipProcessBuffer(bp, 1);
            break;
            }
        }
    else if (dn == cdcnetNode)
        {
        cdcnetProcessDownlineData(bp);
        }
    else
        {
        npuLipProcessDownlineData(bp);
        }

    bipState = BipIdle;

    /*
    **  Check if any more upline buffer is pending and send if necessary.
    */
    if (bipUplineBuffer != NULL)
        {
        npuHipUplineBlock(bipUplineBuffer);
        }
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
    npuBipBufRelease(bipDownlineBuffer);
    bipDownlineBuffer = NULL;
    bipState = BipIdle;

    /*
    **  Check if any more upline buffer is pending and send if necessary.
    */
    if (bipUplineBuffer != NULL)
        {
        npuHipUplineBlock(bipUplineBuffer);
        }
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
    if (bipUplineBuffer != NULL)
        {
        /*
        **  Upline buffer pending, so queue this one for later.
        */
        npuBipQueueAppend(bp, bipUplineQueue);
        return;
        }

    /*
    **  Send this block now.
    */
    bipUplineBuffer = bp;

    if (bipState == BipIdle)
        {
        npuHipUplineBlock(bipUplineBuffer);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Request upline transfer of canned SVM message.
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
    NpuBuffer *bp = npuBipBufGet();
    if (bp == NULL)
        {
        return;
        }

    bp->numBytes = msgSize;
    memcpy(bp->data, msg, bp->numBytes);
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
    npuBipBufRelease(bipUplineBuffer);

    /*
    **  Check if any more upline queued and send if necessary.
    */
    bipUplineBuffer = npuBipQueueExtract(bipUplineQueue);
    if (bipUplineBuffer != NULL)
        {
        npuHipUplineBlock(bipUplineBuffer);
        }
    }

/*
**--------------------------------------------------------------------------
**
**  Private Functions
**
**--------------------------------------------------------------------------
*/

/*---------------------------  End Of File  ------------------------------*/
