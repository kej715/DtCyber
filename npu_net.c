/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**
**  Name: npu_net.c
**
**  Description:
**      Provides TCP/IP networking interface to the ASYNC TIP in an NPU
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


/*
**  -------------
**  Include Files
**  -------------
*/
#include <stdio.h>
#include <stdlib.h>

#if defined(__FreeBSD__)
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif 


#include "const.h"
#include "types.h"
#include "proto.h"
#include "npu.h"
#include <sys/types.h>
#include <memory.h>
#include <time.h>
#if defined(_WIN32)
#include <winsock.h>
#else
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

/*
**  -----------------
**  Private Constants
**  -----------------
*/
#define MaxClaPorts       128
#define NamStartupTime    30

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
static int npuNetAcceptConnections(fd_set *selectFds, int maxFd);
static int npuNetCreateConnections(void);
static bool npuNetCreateListeningSocket(Ncb *ncbp);
static void npuNetCreateThread(void);
static bool npuNetProcessNewConnection(int connFd, Ncb *ncbp, bool isPassive);
static int npuNetRegisterClaPort(Ncb *ncbp);
static void npuNetSendConsoleMsg(int connFd, int connType, char *msg);
static void npuNetTryOutput(Pcb *pcbp);

#if defined(_WIN32)
static void npuNetThread(void *param);

#else
static void *npuNetThread(void *param);

#endif

/*
**  ----------------
**  Public Variables
**  ----------------
*/
char npuNetHostID[HostIdSize];
u32  npuNetHostIP     = 0;
u8   npuNetMaxClaPort = 0;
u8   npuNetMaxCN      = 0;

/*
**  -----------------
**  Private Variables
**  -----------------
*/
static char abortMsg[]        = "\r\nConnection aborted\r\n";
static char connectingMsg[]   = "\r\nConnecting to host - please wait ...";
static char connectedMsg[]    = "\r\nConnected\r\n";
static char *connStates[]     =
    {
    //
    // Indexed by connection type
    //
    "idle",          // StConnInit
    "connecting",    // StConnConnecting
    "connected",     // StConnConnected
    "disconnecting", // StConnDisconnecting
    "busy"           // StConnBusy
    };
static char *connTypes[]      =
    {
    //
    // Indexed by connection type
    //
    "raw",    // ConnTypeRaw
    "pterm",  // ConnTypePterm
    "rs232",  // ConnTypeRs232
    "telnet", // ConnTypeTelnet
    "hasp",   // ConnTypeHasp
    "rhasp",  // ConnTypeRevHasp
    "nje",    // ConnTypeNje
    "trunk"   // ConnTypeTrunk
    };
static char networkDownMsg[]  = "\r\nNetwork going down - connection aborted\r\n";
static char notReadyMsg[]     = "\r\nHost not ready to accept connections - please try again later.\r\n";
static char noPortsAvailMsg[] = "\r\nNo free ports available - please try again later.\r\n";

static Pcb  pcbs[MaxClaPorts];
static bool isPcbsPreset = FALSE;

static Ncb ncbs[MaxTermDefs];
static int numNcbs = 0;

static int pollIndex = 0;

/*
**  Table of functions that queue data for sending to the network,
**  indexed by connection type
*/
static void (*netSend[])(Tcb *tp, u8 *data, int len) =
    {
    npuNetQueueOutput,        // ConnTypeRaw
    npuAsyncPtermNetSend,     // ConnTypePterm
    npuNetQueueOutput,        // ConnTypeRs232
    npuAsyncTelnetNetSend,    // ConnTypeTelnet
    npuNetQueueOutput,        // ConnTypeHasp
    npuNetQueueOutput,        // ConnTypeRevHasp
    npuNetQueueOutput,        // ConnTypeNje
    npuNetQueueOutput         // ConnTypeTrunk
    };

/*
**  Table of functions that notify of network connection, indexed by connection type
*/
static bool (*notifyNetConnect[])(Pcb *pcbp, bool isPassive) =
    {
    npuAsyncNotifyNetConnect, // ConnTypeRaw
    npuAsyncNotifyNetConnect, // ConnTypePterm
    npuAsyncNotifyNetConnect, // ConnTypeRs232
    npuAsyncNotifyNetConnect, // ConnTypeTelnet
    npuHaspNotifyNetConnect,  // ConnTypeHasp
    npuHaspNotifyNetConnect,  // ConnTypeRevHasp
    npuNjeNotifyNetConnect,   // ConnTypeNje
    npuLipNotifyNetConnect    // ConnTypeTrunk
    };

/*
**  Table of functions that notify of network disconnection, indexed by connection type
*/
static void (*notifyNetDisconnect[])(Pcb *pcbp) =
    {
    npuAsyncNotifyNetDisconnect, // ConnTypeRaw
    npuAsyncNotifyNetDisconnect, // ConnTypePterm
    npuAsyncNotifyNetDisconnect, // ConnTypeRs232
    npuAsyncNotifyNetDisconnect, // ConnTypeTelnet
    npuHaspNotifyNetDisconnect,  // ConnTypeHasp
    npuHaspNotifyNetDisconnect,  // ConnTypeRevHasp
    npuNjeNotifyNetDisconnect,   // ConnTypeNje
    npuLipNotifyNetDisconnect    // ConnTypeTrunk
    };

/*
**  Table of functions that preset PCB's, indexed by connection type
*/
static void (*presetPcb[])(Pcb *pcbp) =
    {
    npuAsyncPresetPcb, // ConnTypeRaw
    npuAsyncPresetPcb, // ConnTypePterm
    npuAsyncPresetPcb, // ConnTypeRs232
    npuAsyncPresetPcb, // ConnTypeTelnet
    npuHaspPresetPcb,  // ConnTypeHasp
    npuHaspPresetPcb,  // ConnTypeRevHasp
    npuNjePresetPcb,   // ConnTypeNje
    npuLipPresetPcb    // ConnTypeTrunk
    };

/*
**  Table of functions that process data received from network, indexed by connection type
*/
static void (*processUplineData[])(Pcb *pcbp) =
    {
    npuAsyncProcessUplineData, // ConnTypeRaw
    npuAsyncProcessUplineData, // ConnTypePterm
    npuAsyncProcessUplineData, // ConnTypeRs232
    npuAsyncProcessTelnetData, // ConnTypeTelnet
    npuHaspProcessUplineData,  // ConnTypeHasp
    npuHaspProcessUplineData,  // ConnTypeRevHasp
    npuNjeProcessUplineData,   // ConnTypeNje
    npuLipProcessUplineData    // ConnTypeTrunk
    };

/*
**  Table of functions that reset PCB's, indexed by connection type
*/
static void (*resetPcb[])(Pcb *pcbp) =
    {
    npuAsyncResetPcb,  // ConnTypeRaw
    npuAsyncResetPcb,  // ConnTypePterm
    npuAsyncResetPcb,  // ConnTypeRs232
    npuAsyncResetPcb,  // ConnTypeTelnet
    npuHaspResetPcb,   // ConnTypeHasp
    npuHaspResetPcb,   // ConnTypeRevHasp
    npuNjeResetPcb,    // ConnTypeNje
    npuLipResetPcb     // ConnTypeTrunk
    };

/*
**  Table of functions that attempt network output, indexed by connection type
*/
static void (*tryOutput[])(Pcb *pcbp) =
    {
    npuAsyncTryOutput,  // ConnTypeRaw
    npuAsyncTryOutput,  // ConnTypePterm
    npuAsyncTryOutput,  // ConnTypeRs232
    npuAsyncTryOutput,  // ConnTypeTelnet
    npuHaspTryOutput,   // ConnTypeHasp
    npuHaspTryOutput,   // ConnTypeRevHasp
    npuNjeTryOutput,    // ConnTypeNje
    npuLipTryOutput     // ConnTypeTrunk
    };

/*
 **--------------------------------------------------------------------------
 **
 **  Public Functions
 **
 **--------------------------------------------------------------------------
 */

/*--------------------------------------------------------------------------
**  Purpose:        Register connection type
**
**  Parameters:     Name        Description.
**                  tcpPort     TCP port number for listening or client connections
**                  claPort     Starting CLA port number on NPU
**                  numPorts    Number of CLA ports on this TCP port
**                  connType    Connection type (hasp/nje/pterm/raw/rhasp/rs232/telnet)
**                  ncbpp       Pointer to Ncb pointer (return value)
**
**  Returns:        NpuNetRegOk: successfully registered
**                  NpuNetRegOvfl: too many connection types
**                  NpuNetRegDupTcp: duplicate TCP port specified
**                  NpuNetRegDupCla: duplicate CLA port specified
**
**------------------------------------------------------------------------*/
int npuNetRegisterConnType(int tcpPort, int claPort, int numPorts, int connType, Ncb **ncbpp)
    {
    int i;
    Ncb *ncbp;
    int status;

    /*
    ** Check for too many registrations.
    */
    if (numNcbs >= MaxTermDefs)
        {
        return NpuNetRegOvfl;
        }

    /*
    **  Check for duplicate TCP ports.
    */
    if (tcpPort != 0)
        {
        for (i = 0; i < numNcbs; i++)
            {
            ncbp = &ncbs[i];
            if (ncbp->tcpPort == tcpPort)
                {
                /*
                **  Different connection types may not share a port number.
                **  More than one NJE terminal definition may share the same
                **  port number, and more than one Trunk definition may share
                **  the same port number. All others must be unique.
                */
                if ((ncbp->connType != connType)
                    || ((connType != ConnTypeNje) && (connType != ConnTypeTrunk)))
                    {
                    return NpuNetRegDupTcp;
                    }
                }
            }
        }

    ncbp = &ncbs[numNcbs];

    if (ncbpp != NULL)
        {
        *ncbpp = ncbp;
        }

    /*
    **  Register this port.
    */
    ncbp->state    = StConnInit;
    ncbp->tcpPort  = tcpPort;
    ncbp->claPort  = claPort;
    ncbp->numPorts = numPorts;
    ncbp->connType = connType;
    ncbp->connFd   = 0;
    ncbp->lstnFd   = 0;
    ncbp->hostName = NULL;
    ncbp->nextConnectionAttempt = getSeconds() + (time_t)NamStartupTime;

    /*
    **  Register CLA ports associated with this connection and check for duplicates.
    */
    status = npuNetRegisterClaPort(ncbp);
    if (status != NpuNetRegOk)
        {
        return status;
        }

    numNcbs += 1;

    return NpuNetRegOk;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Close the connection associated with a PCB
**
**  Parameters:     Name        Description.
**                  pcbp        pointer to PCB
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuNetCloseConnection(Pcb *pcbp)
    {
    Ncb *ncbp;

    if ((pcbp != NULL) && (pcbp->connFd > 0))
        {
        netCloseConnection(pcbp->connFd);
        ncbp = pcbp->ncbp;
        if (ncbp != NULL)
            {
            if ((pcbp->connFd == ncbp->connFd) || (ncbp->state == StConnBusy))
                {
                ncbp->state = StConnInit;
                ncbp->nextConnectionAttempt = getSeconds() + (time_t)ConnectionRetryInterval;
                }
            resetPcb[ncbp->connType](pcbp);
            }
        }
    pcbp->connFd = 0;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Find the PCB for a CLA port number
**
**  Parameters:     Name        Description.
**                  claPort     CLA port number
**
**  Returns:        pointer to PCB, or NULL if PCB not found.
**
**------------------------------------------------------------------------*/
Pcb *npuNetFindPcb(int claPort)
    {
    if ((claPort >= 0) && (claPort < MaxClaPorts))
        {
        return &pcbs[claPort];
        }

    return NULL;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Set the current highest active connection number
**
**  Parameters:     Name        Description.
**                  cn          Connection number of connection just
**                              created or terminated
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuNetSetMaxCN(u8 cn)
    {
    Tcb *tp;

    tp = &npuTcbs[cn];
    if ((tp->state == StTermIdle) && (cn >= npuNetMaxCN))
        {
        while (cn > 0)
            {
            if (npuTcbs[cn].state != StTermIdle)
                {
                npuNetMaxCN = cn;
                break;
                }
            cn--;
            }
        }
    else if (cn > npuNetMaxCN)
        {
        npuNetMaxCN = cn;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Initialise network connection handler.
**
**  Parameters:     Name        Description.
**                  startup     FALSE when restarting (NAM restart),
**                              TRUE on first call during initialisation.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuNetInit(bool startup)
    {
    /*
    **  Setup for input data processing.
    */
    pollIndex = 0;

    /*
    **  Only do the following when the emulator starts up.
    */
    if (startup)
        {
        /*
        **  Create the thread which will deal with TCP connections.
        */
        npuNetCreateThread();
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Preset network data structures during DtCyber
**                  initialization.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuNetPreset(void)
    {
    int i;

    for (i = 0; i < MaxClaPorts; i++)
        {
        memset(&pcbs[i], 0, sizeof(Pcb));
        pcbs[i].claPort = (u8)i;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Reset network connection handler when network is going
**                  down.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuNetReset(void)
    {
    int i;
    Ncb *ncbp;
    Pcb *pcbp;
    Tcb *tp;

    /*
    **  Iterate through all TCBs.
    */
    for (i = npuNetMaxCN; i > 0; i--)
        {
        tp   = &npuTcbs[i];
        pcbp = tp->pcbp;
        if ((tp->state != StTermIdle) && (pcbp != NULL) && (pcbp->connFd > 0))
            {
            /*
            **  Notify user that network is going down and then disconnect.
            */
            ncbp = pcbp->ncbp;
            if ((ncbp != NULL) && (ncbp->connType != ConnTypePterm)
                && (tp->deviceType == DtCONSOLE))
                {
                npuNetSendConsoleMsg(pcbp->connFd, ncbp->connType, networkDownMsg);
                }
            npuNetCloseConnection(pcbp);
            tp->state = StTermIdle;
            npuNetSetMaxCN(tp->cn);
            }
        }

    /*
    **  Iterate over PCBs and close any remaining open non-listening connections.
    */
    for (i = 0; i <= npuNetMaxClaPort; i++)
        {
        npuNetCloseConnection(&pcbs[i]);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Signal from host that connection has been established.
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuNetConnected(Tcb *tp)
    {
    if (tp->deviceType == DtCONSOLE)
        {
        npuNetSendConsoleMsg(tp->pcbp->connFd, tp->pcbp->ncbp->connType, connectedMsg);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Signal from host that connection has been terminated.
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuNetDisconnected(Tcb *tp)
    {
    if (tp->deviceType == DtCONSOLE)
        {
        /*
        **  Received disconnect - close socket.
        */
        npuNetCloseConnection(tp->pcbp);
        }

    /*
    **  Cleanup connection.
    */
    npuNetSetMaxCN(tp->cn);
    npuLogMessage("(npu_net) Connection %02x dropped on port %d", tp->cn, tp->pcbp->claPort);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Prepare to send data to terminal.
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**                  data        data address
**                  len         data length
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuNetSend(Tcb *tp, u8 *data, int len)
    {
    netSend[tp->pcbp->ncbp->connType](tp, data, len);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Store block sequence number to acknowledge when send
**                  has completed in last buffer.
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**                  blockSeqNo  block sequence number to acknowledge.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuNetQueueAck(Tcb *tp, u8 blockSeqNo)
    {
    NpuBuffer *bp;

    /*
    **  Try to use the last pending buffer unless it carries a sequence number
    **  which must be acknowledged. If there is none, get a new one and queue it.
    */
    bp = npuBipQueueGetLast(&tp->outputQ);
    if ((bp == NULL) || (bp->blockSeqNo != 0))
        {
        bp = npuBipBufGet();
        npuBipQueueAppend(bp, &tp->outputQ);
        }
    if (bp != NULL)
        {
        bp->blockSeqNo = blockSeqNo;
        }

    /*
    **  Try to output the data on the network connection.
    */
    npuNetTryOutput(tp->pcbp);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Check for network status.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuNetCheckStatus(void)
    {
    Pcb            *pcbp;
    fd_set         readFds;
    int            readySockets = 0;
    struct timeval timeout;
    fd_set         writeFds;

    timeout.tv_sec  = 0;
    timeout.tv_usec = 0;

    while (pollIndex <= npuNetMaxClaPort)
        {
        pcbp = &pcbs[pollIndex++];
        if (pcbp->connFd <= 0
            || (pcbp->ncbp != NULL && pcbp->ncbp->state == StConnDisconnecting))
            {
            continue;
            }

        /*
        **  Handle network traffic.
        */
        FD_ZERO(&readFds);
        FD_ZERO(&writeFds);
        FD_SET(pcbp->connFd, &readFds);
        readySockets = select(pcbp->connFd + 1, &readFds, NULL, NULL, &timeout);

        if ((readySockets > 0) && FD_ISSET(pcbp->connFd, &readFds))
            {
            /*
            **  Receive a block of data.
            */
            pcbp->inputCount = recv(pcbp->connFd, pcbp->inputData, MaxBuffer, 0);
            if (pcbp->inputCount <= 0)
                {
                notifyNetDisconnect[pcbp->ncbp->connType](pcbp);
                continue;
                }
            processUplineData[pcbp->ncbp->connType](pcbp);
            }

        if (pcbp->connFd > 0)
            {
            FD_ZERO(&writeFds);
            FD_SET(pcbp->connFd, &writeFds);
            readySockets = select(pcbp->connFd + 1, NULL, &writeFds, NULL, &timeout);
            if ((readySockets > 0) && FD_ISSET(pcbp->connFd, &writeFds))
                {
                /*
                **  Try sending data if any is pending.
                */
                npuNetTryOutput(pcbp);
                }
            }

        /*
        **  The following return ensures that we resume with polling the next
        **  connection in sequence otherwise low-numbered connections would get
        **  preferential treatment.
        */
        return;
        }
    pollIndex = 0;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Show status of NPU/MDI data communication  (operator interface).
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuNetShowStatus()
    {
    u8       channelNo;
    char     chEqStr[10];
    DevSlot *dp;
    char     *dts;
    u8       eqNo;
    int      i;
    u32      ipAddr;
    Ncb      *ncbp;
    Pcb      *pcbp;
    char     peerAddress[24];
    u16      port;
    char     outBuf[200];

    dp = NULL;
    for (channelNo = 0; channelNo < MaxChannels; channelNo++)
        {
        dp = channelFindDevice(channelNo, DtMdi);
        if (dp != NULL)
            {
            dts = "MDI    ";
            break;
            }
        dp = channelFindDevice(channelNo, DtNpu);
        if (dp != NULL)
            {
            dts = "2550   ";
            break;
            }
        }
    if (dp == NULL) return;

    sprintf(chEqStr, "C%02o E%02o",  dp->channel->id, dp->eqNo);
    for (i = 0; i < numNcbs; i++)
        {
        ncbp = &ncbs[i];
        switch (ncbp->connType)
            {
        case ConnTypeRaw:
        case ConnTypePterm:
        case ConnTypeRs232:
        case ConnTypeTelnet:
        case ConnTypeHasp:
        case ConnTypeNje:
        case ConnTypeTrunk:
            if (ncbp->lstnFd > 0)
                {
                sprintf(outBuf, "    >   %-8s %-7s     "FMTNETSTATUS"\n", dts, chEqStr, netGetLocalTcpAddress(ncbp->lstnFd), "",
                    connTypes[ncbp->connType], "listening");
                opDisplay(outBuf);
                chEqStr[0] = '\0';
                }
            break;

        case ConnTypeRevHasp:
            ipAddr = ntohl(ncbp->hostAddr.sin_addr.s_addr);
            port   = ntohs(ncbp->hostAddr.sin_port);
            sprintf(peerAddress, "%d.%d.%d.%d:%d",
              (ipAddr >> 24) & 0xff,
              (ipAddr >> 16) & 0xff,
              (ipAddr >>  8) & 0xff,
              ipAddr         & 0xff,
              port);
            if (ncbp->state == StConnConnecting)
                {
                sprintf(outBuf, "    >   %-8s %-7s     "FMTNETSTATUS"\n", dts, chEqStr, netGetLocalTcpAddress(ncbp->connFd),
                    peerAddress, connTypes[ncbp->connType], "connecting");
                opDisplay(outBuf);
                chEqStr[0] = '\0';
                }
            else if (ncbp->state != StConnConnected)
                {
                sprintf(outBuf, "    >   %-8s %-7s     "FMTNETSTATUS"\n", dts, chEqStr, ipAddress, peerAddress,
                    connTypes[ncbp->connType], "disconnected");
                opDisplay(outBuf);
                chEqStr[0] = '\0';
                }
            break;

        default:
            break;
            }
        }
    for (i = 0; i < MaxClaPorts; i++)
        {
        pcbp = &pcbs[i];
        if (pcbp->ncbp != NULL && pcbp->connFd > 0)
            {
            sprintf(outBuf, "    >   %-8s %-7s P%02x "FMTNETSTATUS"\n", dts, chEqStr, pcbp->claPort, netGetLocalTcpAddress(pcbp->connFd),
                netGetPeerTcpAddress(pcbp->connFd), connTypes[pcbp->ncbp->connType], connStates[pcbp->ncbp->state]),
            opDisplay(outBuf);
            chEqStr[0] = '\0';
            }
        }
    }

/*
 **--------------------------------------------------------------------------
 **
 **  Private Functions
 **
 **--------------------------------------------------------------------------
 */

/*--------------------------------------------------------------------------
**  Purpose:        Register CLA port numbers and associated connection types
**
**  Parameters:     Name        Description.
**                  ncbp        pointer to network connection control block to be
**                              associated with the ports. The NCB provides the
**                              starting CLA port number, number of CLA ports, and
**                              connection type.
**
**  Returns:        NpuNetRegOk: successfully registered
**                  NpuNetRegOvfl: invalid CLA port number
**                  NpuNetRegNoMem: memory exhausted
**                  NpuNetRegDupCla: duplicate CLA port specified
**
**------------------------------------------------------------------------*/
static int npuNetRegisterClaPort(Ncb *ncbp)
    {
    int i;
    int limit;
    Pcb *pcbp;

    if (isPcbsPreset == FALSE)
        {
        for (i = 0; i < MaxClaPorts; i++)
            {
            memset(&pcbs[i], 0, sizeof(Pcb));
            }
        isPcbsPreset = TRUE;
        }

    limit = ncbp->claPort + ncbp->numPorts;

    if ((ncbp->claPort < 1) || (limit > MaxClaPorts))
        {
        return NpuNetRegOvfl;
        }

    for (i = ncbp->claPort; i < limit; i++)
        {
        pcbp = &pcbs[i];
        if (pcbp->claPort == 0)
            {
            pcbp->claPort   = i;
            pcbp->ncbp      = ncbp;
            pcbp->inputData = (u8 *)malloc(MaxBuffer);
            if (pcbp->inputData == NULL)
                {
                return NpuNetRegNoMem;
                }
            presetPcb[ncbp->connType](pcbp);
            }
        else
            {
            return NpuNetRegDupCla;
            }
        }
    if (limit > npuNetMaxClaPort)
        {
        npuNetMaxClaPort = (u8)(limit - 1);
        }

    return NpuNetRegOk;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Send a message to a console device
**
**  Parameters:     Name        Description.
**                  connFd      Socket descriptor
**                  connType    Connection type (raw/pterm/telnet/hasp/etc.)
**                  msg         Pointer to message
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuNetSendConsoleMsg(int connFd, int connType, char *msg)
    {
    switch (connType)
        {
    case ConnTypeRaw:
    case ConnTypePterm:
    case ConnTypeRs232:
    case ConnTypeTelnet:
        send(connFd, msg, strlen(msg), 0);
        break;

    case ConnTypeHasp:
    case ConnTypeRevHasp:
    case ConnTypeNje:
    case ConnTypeTrunk:
        // discard messages to HASP/Reverse HASP/NJE/LIP
        break;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Accepts connections pending on listening sockets.
**
**  Parameters:     Name        Description.
**                  selectFds   pointer to set of listening socket descriptors
**                  maxFd       maximum socket descriptor value in the set
**
**  Returns:        number of connections accepted
**
**------------------------------------------------------------------------*/
static int npuNetAcceptConnections(fd_set *selectFds, int maxFd)
    {
    int                acceptFd;
    fd_set             acceptFds;
    struct sockaddr_in from;
    int                i;
    int                n;
    Ncb                *ncbp;
    int                rc;
    struct timeval     timeout;

#if defined(_WIN32)
    int fromLen;
#else
    socklen_t fromLen;
#endif

    timeout.tv_sec  = 1;
    timeout.tv_usec = 0;

    memcpy(&acceptFds, selectFds, sizeof(fd_set));

    rc = select(maxFd + 1, &acceptFds, NULL, NULL, &timeout);
    if (rc < 0)
        {
        fprintf(stderr, "NET: select returned unexpected %d\n", rc);
        sleepMsec(1000);
        }
    else if (rc < 1)
        {
        return 0;
        }

    /*
    **  Find the listening socket(s) with pending connections and accept them.
    */
    n = 0;
    for (i = 0; i < numNcbs; i++)
        {
        ncbp = &ncbs[i];
        switch (ncbp->connType)
            {
        case ConnTypeRaw:
        case ConnTypePterm:
        case ConnTypeRs232:
        case ConnTypeTelnet:
        case ConnTypeHasp:
        case ConnTypeNje:
        case ConnTypeTrunk:
            if ((ncbp->lstnFd > 0) && FD_ISSET(ncbp->lstnFd, &acceptFds))
                {
                fromLen  = sizeof(from);
                acceptFd = accept(ncbp->lstnFd, (struct sockaddr *)&from, &fromLen);
                if (acceptFd < 0)
                    {
                    fputs("(npu_net) spurious connection attempt\n", stderr);
                    }
                else if (npuNetProcessNewConnection(acceptFd, ncbp, TRUE))
                    {
                    n += 1;
                    }
                }
            break;

        case ConnTypeRevHasp:
            break;

        default:
            fprintf(stderr, "(npu_net) Invalid connection type: %u\n", ncbp->connType);
            break;
            }
        }

    return n;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Creates connections for client sockets.
**
**  Parameters:     Name        Description.
**
**  Returns:        number of connections created
**
**------------------------------------------------------------------------*/
static int npuNetCreateConnections(void)
    {
    time_t         currentTime;
#if defined(_WIN32)
    SOCKET         fd;
#else
    int            fd;
#endif
    int            i;
    int            n;
    Ncb            *ncbp;
    int            rc;
    fd_set         selectFds;
    struct timeval timeout;

    /*
    **  Attempt to create connections only when NAM is ready.
    */
    if (!npuSvmIsReady())
        {
        return 0;
        }

    /*
    **  Find client sockets ready for initiating connections and initiate
    **  them, and find client sockets with pending connection requests and
    **  determine whether connections have been completed.
    */
    currentTime = getSeconds();
    n           = 0;

    for (i = 0; i < numNcbs; i++)
        {
        ncbp = &ncbs[i];
        switch (ncbp->connType)
            {
        case ConnTypeRaw:
        case ConnTypePterm:
        case ConnTypeRs232:
        case ConnTypeTelnet:
        case ConnTypeHasp:
            break;

        case ConnTypeTrunk:
        case ConnTypeNje:
            if (ncbp->hostAddr.sin_port == 0) continue; // listen-only
            // fall through
        case ConnTypeRevHasp:
            switch (ncbp->state)
            {
            case StConnInit:
                if (currentTime < ncbp->nextConnectionAttempt)
                    {
                    continue;
                    }
                ncbp->nextConnectionAttempt = currentTime + (time_t)ConnectionRetryInterval;
                fd = netInitiateConnection((struct sockaddr *)&ncbp->hostAddr);
#if defined(_WIN32)
                if (fd == INVALID_SOCKET)
#else
                if (fd == -1)
#endif
                    {
                    npuLogMessage("(npu_net) Failed to initiate connection to: %s\n", ncbp->hostName);
                    continue;
                    }
                else // connection in progress
                    {
                    npuLogMessage("(npu_net) Initiated connection to host: %s:%u", ncbp->hostName, ncbp->tcpPort);
                    ncbp->connFd = fd;
                    ncbp->state  = StConnConnecting;
                    }
                break;

            case StConnConnecting:
                FD_ZERO(&selectFds);
                FD_SET(ncbp->connFd, &selectFds);
                timeout.tv_sec  = 0;
                timeout.tv_usec = 0;
                rc = select(ncbp->connFd + 1, NULL, &selectFds, NULL, &timeout);
                if (rc < 0)
                    {
                    fprintf(stderr, "NET: select returned unexpected %d\n", rc);
                    sleepMsec(1000);
                    }
                else if (rc > 0)
                    {
                    if (FD_ISSET(ncbp->connFd, &selectFds))
                        {
                        rc = netGetErrorStatus(ncbp->connFd);
                        if (rc != 0) // connection failed
                            {
                            npuLogMessage("(npu_net) Failed to connect to host: %s:%u", ncbp->hostName, ncbp->tcpPort);
                            netCloseConnection(ncbp->connFd);
                            ncbp->connFd = 0;
                            ncbp->nextConnectionAttempt = currentTime + (time_t)ConnectionRetryInterval;
                            ncbp->state = StConnInit;
                            }
                        else
                            {
                            npuLogMessage("(npu_net) Connected to host: %s:%u", ncbp->hostName, ncbp->tcpPort);
                            if (npuNetProcessNewConnection(ncbp->connFd, ncbp, FALSE))
                                {
                                n += 1;
                                }
                            else
                                {
                                ncbp->nextConnectionAttempt = currentTime + (time_t)ConnectionRetryInterval;
                                }
                            }
                        }
                    }
                else if (currentTime > ncbp->connectionDeadline)
                    {
                    npuLogMessage("(npu_net) Connection timeout to host: %s:%u", ncbp->hostName, ncbp->tcpPort);
                    netCloseConnection(ncbp->connFd);
                    ncbp->connFd = 0;
                    ncbp->nextConnectionAttempt = currentTime + (time_t)ConnectionRetryInterval;
                    ncbp->state = StConnInit;
                    }
                break;

            default:
                break;
            }
            break;

        default:
            fprintf(stderr, "(npu_net) Invalid connection type: %u\n", ncbp->connType);
            break;
            }
        }

    return n;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Create a listening socket for a network connection
**                  control block.
**
**  Parameters:     Name        Description.
**                  ncbp        pointer to network connection control block
**
**  Returns:        TRUE if socket created successfully
**
**------------------------------------------------------------------------*/
static bool npuNetCreateListeningSocket(Ncb *ncbp)
    {
#if defined(_WIN32)
    SOCKET sd;

    sd = netCreateListener(ncbp->tcpPort);
    if (sd == INVALID_SOCKET)
#else
    int    sd;

    sd = netCreateListener(ncbp->tcpPort);
    if (sd == -1)
#endif
        {
        fprintf(stderr, "(npu_net) Can't create socket for port %d\n", ncbs->tcpPort);
        return FALSE;
        }
    ncbp->lstnFd = sd;

    return TRUE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Create thread which will deal with all TCP
**                  connections.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuNetCreateThread(void)
    {
#if defined(_WIN32)
    DWORD  dwThreadId;
    HANDLE hThread;

    /*
    **  Create TCP thread.
    */
    hThread = CreateThread(
        NULL,                                       // no security attribute
        0,                                          // default stack size
        (LPTHREAD_START_ROUTINE)npuNetThread,
        (LPVOID)NULL,                               // thread parameter
        0,                                          // not suspended
        &dwThreadId);                               // returns thread ID

    if (hThread == NULL)
        {
        fprintf(stderr, "Failed to create npuNet thread\n");
        exit(1);
        }
#else
    int            rc;
    pthread_t      thread;
    pthread_attr_t attr;

    /*
    **  Create POSIX thread with default attributes.
    */
    pthread_attr_init(&attr);
    rc = pthread_create(&thread, &attr, npuNetThread, NULL);
    if (rc < 0)
        {
        fprintf(stderr, "Failed to create npuNet thread\n");
        exit(1);
        }
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        TCP network connection thread.
**
**  Parameters:     Name        Description.
**                  param       unused
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
#if defined(_WIN32)
static void npuNetThread(void *param)
#else
static void *npuNetThread(void *param)
#endif
    {
    fd_set listenFds;
    int    i;
    int    j;
    Ncb    *ncbp;

#if defined(_WIN32)
    SOCKET maxFd = 0;
#else
    int maxFd = 0;
#endif

    FD_ZERO(&listenFds);

    /*
    **  Create a listening socket for every configured connection type that listens
    **  for connections.
    */
    for (i = 0; i < numNcbs; i++)
        {
        ncbp = &ncbs[i];
        switch (ncbp->connType)
            {
        case ConnTypeTrunk:
        case ConnTypeNje:
            if (ncbp->tcpPort == 0)
                {
                continue;
                }
            j = 0;
            while (j < i)
                {
                if (ncbs[j].tcpPort == ncbp->tcpPort)
                    {
                    break;
                    }
                j += 1;
                }
            if (j < i)
                {
                continue;        // already listening on this port
                }

        // else fall through
        case ConnTypeRaw:
        case ConnTypePterm:
        case ConnTypeRs232:
        case ConnTypeTelnet:
        case ConnTypeHasp:
            if (npuNetCreateListeningSocket(ncbp) == TRUE)
                {
                /*
                **  Add to set of listening FDs to be polled
                */
                FD_SET(ncbp->lstnFd, &listenFds);

                /*
                **  Determine highest FD for later select
                */
                if (maxFd < ncbp->lstnFd)
                    {
                    maxFd = ncbp->lstnFd;
                    }
                }
            else
                {
#if defined(_WIN32)
                return;
#else
                return NULL;
#endif
                }
            break;

        case ConnTypeRevHasp:
            break;

        default:
            fprintf(stderr, "(npu_net) Invalid connection type: %u\n", ncbp->connType);
            break;
            }
        }

    for ( ; ;)
        {
        npuNetAcceptConnections(&listenFds, maxFd);
        npuNetCreateConnections();
        }

#if !defined(_WIN32)
    return NULL;
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Process new TCP connection
**
**  Parameters:     Name        Description.
**                  connFd      new connection's FD
**                  ncbp        pointer to NCB
**                  isPassive   TRUE if the connection is passive (accept vs connect)
**
**  Returns:        TRUE if new connection processed successfully.
**
**------------------------------------------------------------------------*/
static bool npuNetProcessNewConnection(int connFd, Ncb *ncbp, bool isPassive)
    {
    u8  i;
    int limit;
    int optEnable = 1;
    Pcb *pcbp;
    Pcb *pcbp2;

#if defined(_WIN32)
    u_long blockEnable = 1;
#endif

    /*
    **  Set Keepalive option so that we can eventually discover if
    **  a client has been rebooted.
    */
    setsockopt(connFd, SOL_SOCKET, SO_KEEPALIVE, (void *)&optEnable, sizeof(optEnable));

    /*
    **  Make socket non-blocking.
    */
#if defined(_WIN32)
    ioctlsocket(connFd, FIONBIO, &blockEnable);
#else
    fcntl(connFd, F_SETFL, O_NONBLOCK);
#endif

    /*
    **  Check if the host is ready to accept connections.
    */
    if (!npuSvmIsReady())
        {
        /*
        **  Tell the user.
        */
        npuNetSendConsoleMsg(connFd, ncbp->connType, notReadyMsg);
        netCloseConnection(connFd);
        if (isPassive == FALSE)
            {
            ncbp->connFd = 0;
            }
        ncbp->state = StConnInit;

        return FALSE;
        }

    /*
    **  Find a free PCB in the set of PCB's associated with this NCB.
    */
    pcbp  = NULL;
    limit = ncbp->claPort + ncbp->numPorts;
    for (i = ncbp->claPort; i < limit; i++)
        {
        if (pcbs[i].connFd < 1)
            {
            pcbp = &pcbs[i];
            break;
            }
        }

    /*
    **  If a free PCB hasn't been found yet, and this connection is a passive
    **  one, and it's a trunk or NJE connection, then look for a free
    **  PCB of the same type that's associated with the same listening port.
    **/
    if ((pcbp == NULL) && isPassive
        && ((ncbp->connType == ConnTypeTrunk) || (ncbp->connType == ConnTypeNje)))
        {
        for (i = 0; i <= npuNetMaxClaPort; i++)
            {
            pcbp2 = &pcbs[i];
            if ((pcbp2->connFd < 1)
                && (pcbp2->ncbp != NULL) && (pcbp2->ncbp->connType == ncbp->connType)
                && (pcbp2->ncbp->tcpPort == ncbp->tcpPort))
                {
                pcbp = pcbp2;
                break;
                }
            }
        }

    /*
    **  If a free PCB was not found, inform the user and close the socket.
    */
    if (pcbp == NULL)
        {
        npuNetSendConsoleMsg(connFd, ncbp->connType, noPortsAvailMsg);
        netCloseConnection(connFd);
        if (isPassive)
            {
            ncbp->state = StConnInit;
            }
        else
            {
            npuLogMessage("(npu_net) Free PCB not found for active connection of type %d (%s)",
                          ncbp->connType, ncbp->hostName);
            ncbp->connFd = 0;
            ncbp->state  = StConnBusy;
            }

        return FALSE;
        }

    /*
    **  Initialize the connection and mark it as active.
    */
    pcbp->connFd = connFd;
    if (notifyNetConnect[ncbp->connType](pcbp, isPassive))
        {
        npuNetSendConsoleMsg(connFd, ncbp->connType, connectingMsg);
        pcbp->ncbp->state = StConnConnected;

        return TRUE;
        }

    /*
    **  If execution reaches here, the associated TIP rejected the connection,
    **  so notify the user and close the socket.
    */
    npuNetSendConsoleMsg(connFd, ncbp->connType, abortMsg);
    netCloseConnection(connFd);
    pcbp->connFd = 0;
    if (isPassive)
        {
        pcbp->ncbp->state = StConnInit;
        }
    else
        {
        pcbp->ncbp->connFd = 0;
        pcbp->ncbp->state  = StConnBusy;
        }

    return FALSE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Queue output to terminal.
**
**  Parameters:     Name        Description.
**                  tp          TCB pointer
**                  data        data address
**                  len         data length
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuNetQueueOutput(Tcb *tp, u8 *data, int len)
    {
    NpuBuffer *bp;
    u8        *startAddress;
    int       byteCount;

    /*
    **  Try to use the last pending buffer unless it carries a sequence number
    **  which must be acknowledged. If there is none, get a new one and queue it.
    */
    bp = npuBipQueueGetLast(&tp->outputQ);
    if ((bp == NULL) || (bp->blockSeqNo != 0))
        {
        bp = npuBipBufGet();
        npuBipQueueAppend(bp, &tp->outputQ);
        }

    while (bp != NULL && len > 0)
        {
        /*
        **  Append data to the buffer.
        */
        startAddress = bp->data + bp->numBytes;
        byteCount    = MaxBuffer - bp->numBytes;
        if (byteCount >= len)
            {
            byteCount = len;
            }

        memcpy(startAddress, data, byteCount);
        data         += byteCount;
        bp->numBytes += byteCount;

        /*
        **  If there is still data left get a new buffer, queue it and
        **  copy what is left.
        */
        len -= byteCount;
        if (len > 0)
            {
            bp = npuBipBufGet();
            npuBipQueueAppend(bp, &tp->outputQ);
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Try to send any queued data.
**
**  Parameters:     Name        Description.
**                  pcbp        PCB pointer
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void npuNetTryOutput(Pcb *pcbp)
    {
    tryOutput[pcbp->ncbp->connType](pcbp);
    }

/*---------------------------  End Of File  ------------------------------*/
