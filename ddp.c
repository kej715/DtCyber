/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Paul Koning, Tom Hunter
**
**  Name: ddp.c
**
**  Description:
**      Perform emulation of CDC Distributive Data Path
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
#include <string.h>
#include "const.h"
#include "types.h"
#include "proto.h"

/*
**  -----------------
**  Private Constants
**  -----------------
*/

/*
**  DDP function and status codes.
*/
#define FcDdpReadECS             05001
#define FcDdpWriteECS            05002
#define FcDdpStatus              05004
#define FcDdpMasterClear         05010
#define FcDdpClearMaintMode      05020
#define FcDdpClearDdpPort        05030
#define FcDdpFlagRegister        05040
#define FcDdpSelectEsmMode       05404

/*
**      Status reply flags
**
**      0001 = ECS abort
**      0002 = ECS accept
**      0004 = ECS parity error
**      0010 = ECS write pending
**      0020 = Channel parity error
**      0040 = 6640 parity error
*/
#define StDdpAbort               00001
#define StDdpAccept              00002
#define StDdpParErr              00004
#define StDdpWrite               00010
#define StDdpChParErr            00020
#define StDdp6640ParErr          00040

/*
**  DDP magical ECS address bits
*/
#define DdpAddrMaint             (1 << 21)
#define DdpAddrReadOne           (1 << 22)
#define DdpAddrFlagReg           (1 << 23)

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

typedef struct
    {
    CpWord  curword;
    u32     addr;
    int     dbyte;
    int     abyte;
    int     endaddrcycle;
    PpWord  stat;
    } DdpContext;

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static FcStatus ddpFunc(PpWord funcCode);
static void ddpIo(void);
static void ddpActivate(void);
static void ddpDisconnect(void);
static char *ddpFunc2String(PpWord funcCode);

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

#if DEBUG
static FILE *ddpLog = NULL;
#endif

/*
**--------------------------------------------------------------------------
**
**  Public Functions
**
**--------------------------------------------------------------------------
*/

/*--------------------------------------------------------------------------
**  Purpose:        Initialise DDP.
**
**  Parameters:     Name        Description.
**                  model       Cyber model number
**                  increment   clock increment per iteration.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void ddpInit(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName)
    {
    DevSlot *dp;
    DdpContext *dc;
    
    (void)eqNo;
    (void)unitNo;
    (void)deviceName;

    if (extMaxMemory == 0)
        {
        fprintf (stderr, "(ddp    ) Cannot configure DDP, no ECS configured\n");
        exit (1);
        }
    
#if DEBUG
    if (ddpLog == NULL)
        {
        ddpLog = fopen("ddplog.txt", "wt");
        }
#endif

    dp = channelAttach(channelNo, eqNo, DtDdp);

    dp->activate = ddpActivate;
    dp->disconnect = ddpDisconnect;
    dp->func = ddpFunc;
    dp->io = ddpIo;

    dc = calloc(1, sizeof (DdpContext));
    if (dc == NULL)
        {
        fprintf(stderr, "(ddp    ) Failed to allocate DDP context block\n");
        exit(1);
        }

    dp->context[0] = dc;
    dc->stat = StDdpAccept;

    /*
    **  Print a friendly message.
    */
    printf("(ddp    ) Initialised on channel %o\n", channelNo);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Execute function code on DDP device.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static FcStatus ddpFunc(PpWord funcCode)
    {
    DdpContext *dc;

    dc = (DdpContext *)(activeDevice->context[0]);

#if DEBUG
    fprintf(ddpLog, "\n(ddp    ) %06d PP:%02o CH:%02o f:%04o T:%-25s  >   ",
        traceSequenceNo,
        activePpu->id,
        activeDevice->channel->id,
        funcCode,
        ddpFunc2String(funcCode));
#endif

    switch (funcCode)
        {
    default:
#if DEBUG
        fprintf(ddpLog, " FUNC not implemented & declined!");
#endif
        break;

    case FcDdpWriteECS:
        dc->curword = 0;
    case FcDdpReadECS:
    case FcDdpStatus:
    case FcDdpFlagRegister:
        dc->abyte = 0;
        dc->dbyte = 0;
        dc->addr = 0;
        activeDevice->fcode = funcCode;
        return(FcAccepted);

    case FcDdpMasterClear:
        activeDevice->fcode = 0;
        dc->stat = StDdpAccept;
        return(FcProcessed);

    case FcDdpSelectEsmMode:
        return(FcProcessed);
        }

    return(FcDeclined);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Perform I/O.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void ddpIo(void)
    {
    DdpContext *dc;

    dc = (DdpContext *) (activeDevice->context[0]);

    switch (activeDevice->fcode)
        {
    default:
        return;
        
    case FcDdpStatus:
        if (!activeChannel->full)
            {
            activeChannel->data = dc->stat;
            activeChannel->full = TRUE;
            activeDevice->fcode = 0;
            // ? activeChannel->discAfterInput = TRUE;
#if DEBUG
                fprintf(ddpLog, " %04o", activeChannel->data);
#endif
            }
        break;
        
    case FcDdpReadECS:
    case FcDdpWriteECS:
    case FcDdpFlagRegister:
        if (dc->abyte < 2)
            {
            /*
            **  We need to get two address bytes from the PPU.
            */
            if (activeChannel->full)
                {
                dc->addr <<= 12;
                dc->addr += activeChannel->data;
                dc->abyte++;
                activeChannel->full = FALSE;
                }

            if (dc->abyte == 2)
                {
#if DEBUG
                fprintf(ddpLog, " ECS addr: %08o Data: ", dc->addr);
#endif

                if (activeDevice->fcode == FcDdpReadECS || activeDevice->fcode == FcDdpFlagRegister)
                    {
                    /*
                    **  Delay a bit before we set channel full.
                    */
                    dc->endaddrcycle = cycles;

                    /*
                    **  A flag register reference occurs when bit 23 is set in address.
                    */
                    if ((dc->addr & DdpAddrFlagReg) != 0 || activeDevice->fcode == FcDdpFlagRegister)
                        {
#if DEBUG
                        fputs("(flag register operation: ", ddpLog);
#endif
                        if (cpuEcsFlagRegister(dc->addr))
                            {
                            dc->stat = StDdpAccept;
#if DEBUG
                            fputs("accept)", ddpLog);
#endif
                            }
                        else
                            {
                            /* activeChannel->discAfterInput = TRUE; */
                            dc->stat = StDdpAbort;
#if DEBUG
                            fputs("abort)", ddpLog);
#endif
                            }

                        dc->dbyte = 0;
                        dc->curword = 0;
                        return;
                        }
                    else
                        {
                        dc->dbyte = -1;
#if DEBUG
                        if ((dc->addr & DdpAddrReadOne) != 0)
                            {
                            fputs("(one reference) ", ddpLog);
                            }
                        else if ((dc->addr & DdpAddrMaint) != 0)
                            {
                            fputs("(maintenance mode) ", ddpLog);
                            }
#endif
                        }
                    }
                }

            break;
            }

        if (activeDevice->fcode == FcDdpReadECS)
            {
            if (!activeChannel->full && cycles - dc->endaddrcycle > 20)
                {
                if (dc->dbyte == -1)
                    {
                    /*
                    **  Fetch next 60 bits from ECS.
                    */
                    if (cpuDdpTransfer(dc->addr & Mask21, &dc->curword, FALSE))
                        {
                        dc->stat = StDdpAccept;
                        }
                    else
                        {
#if DEBUG
                        fputs(" abort", ddpLog);
#endif
                        activeChannel->discAfterInput = TRUE;
                        dc->stat = StDdpAbort;
                        }

                    dc->dbyte = 0;
                    }

                /*
                **  Return next byte to PPU.
                */
                activeChannel->data = (PpWord)((dc->curword >> 48) & Mask12);
                activeChannel->full = TRUE;

#if DEBUG
                fprintf(ddpLog, " %04o", activeChannel->data);
#endif

                /*
                **  Update admin stuff.
                */
                dc->curword <<= 12;
                if (++dc->dbyte == 5)
                    {
                    if ((dc->addr & DdpAddrReadOne) != 0)
                        {
                        activeChannel->discAfterInput = TRUE;
                        }

                    dc->dbyte = -1;
                    dc->addr++;
                    }
                }
            }
        else if (activeChannel->full)
            {
            dc->stat = StDdpAccept;
            dc->curword <<= 12;
            dc->curword += activeChannel->data;
            activeChannel->full = FALSE;

#if DEBUG
            fprintf(ddpLog, " %04o", activeChannel->data);
#endif

            if (++dc->dbyte == 5)
                {
                /*
                **  Write next 60 bit to ECS.
                */
                if (!cpuDdpTransfer(dc->addr, &dc->curword, TRUE))
                    {
#if DEBUG
                    fputs(" abort", ddpLog);
#endif
                    activeChannel->active = FALSE;
                    dc->stat = StDdpAbort;
                    return;
                    }

                dc->curword = 0;
                dc->dbyte = 0;
                dc->addr++;
                }
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Handle channel activation.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void ddpActivate(void)
    {
#if DEBUG
    fprintf(ddpLog, "\n%06d PP:%02o CH:%02o Activate",
        traceSequenceNo,
        activePpu->id,
        activeDevice->channel->id);
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Handle disconnecting of channel.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void ddpDisconnect(void)
    {
    DdpContext *dc;

    dc = (DdpContext *) (activeDevice->context[0]);

#if DEBUG
    fprintf(ddpLog, "\n%06d PP:%02o CH:%02o Disconnect",
        traceSequenceNo,
        activePpu->id,
        activeDevice->channel->id);
#endif

    if (activeDevice->fcode == FcDdpWriteECS && dc->dbyte != 0)
        {
        /*
        **  Write final 60 bit to ECS padded with zeros.
        */
        dc->curword <<= 5 - dc->dbyte;
        if (!cpuDdpTransfer(dc->addr, &dc->curword, TRUE))
            {
            activeChannel->active = FALSE;
            dc->stat = StDdpAbort;
#if DEBUG
            fputs(" abort", ddpLog);
#endif
            return;
            }

        dc->curword = 0;
        dc->dbyte = 0;
        dc->addr++;
        }

    /*
    **  Abort pending device disconnects - the PP is doing the disconnect.
    */
    activeChannel->discAfterInput = FALSE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Convert function code to string.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        String equivalent of function code.
**
**------------------------------------------------------------------------*/
static char *ddpFunc2String(PpWord funcCode)
    {
    static char buf[30];
#if DEBUG
    switch(funcCode)
        {
    case FcDdpReadECS                 : return "FcDdpReadECS";
    case FcDdpWriteECS                : return "FcDdpWriteECS";
    case FcDdpStatus                  : return "FcDdpStatus";
    case FcDdpMasterClear             : return "FcDdpMasterClear";
    case FcDdpClearMaintMode          : return "FcDdpClearMaintMode";
    case FcDdpClearDdpPort            : return "FcDdpClearDdpPort";
    case FcDdpFlagRegister            : return "FcDdpFlagRegister";
    case FcDdpSelectEsmMode           : return "FcDdpSelectEsmMode";
        }
#endif
    sprintf(buf, "(ddp    ) Unknown Function: %04o", funcCode);
    return(buf);
    }

/*---------------------------  End Of File  ------------------------------*/
