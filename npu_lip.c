/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2019, Kevin Jordan, Tom Hunter
**
**  Name: npu_lip.c
**
**  Author: Kevin Jordan
**
**  Description:
**      Implements the LIP protocal for a CDC 2550 HCP running CCP.
**      The LIP protocol enables hosts running NAM on NOS to communicate
**      with each other. In particular, it enables application-to-application
**      connections across hosts.
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
**  If not, see <htlp://www.gnu.org/licenses/gpl-3.0.txt>.
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
#include "const.h"
#include "types.h"
#include "proto.h"
#include "npu.h"
#include <sys/types.h>
#include <memory.h>
#include <time.h>
#if defined(_WIN32)
#include <string.h>
#include <winsock.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/uio.h>
#include <strings.h>
#endif

#if defined(__APPLE__)
#include <execinfo.h>
#endif

/*
**  -----------------
**  Private Constants
**  -----------------
*/
#define MaxIdleTime 15
#define MaxTrunks   16

/*
**  -----------------------
**  Private Macro Functions
**  -----------------------
*/
#if DEBUG
#define HexColumn(x) (3 * (x) + 4)
#define AsciiColumn(x) (HexColumn(16) + 2 + (x))
#define LogLineLength (AsciiColumn(16))
#endif
#if defined(_WIN32)
#define strcasecmp _stricmp
#endif

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
static bool npuLipActivateTrunk(Pcb *pcbp);
static bool npuLipDeactivateTrunk(Pcb *pcbp);
static bool npuLipProcessConnectRequest(Pcb *pcbp);
static bool npuLipProcessConnectResponse(Pcb *pcbp);
static bool npuLipSendConnectRequest(Pcb *pcbp);
static void npuLipSendQueuedData(Pcb *pcbp);
#if DEBUG
static void npuLipLogBytes (u8 *bytes, int len);
static void npuLipLogFlush (void);
static void npuLipPrintStackTrace(FILE *fp);
#endif

/*
**  ----------------
**  Public Variables
**  ----------------
*/
u8 npuLipTrunkCount = 0;

/*
**  -----------------
**  Private Variables
**  -----------------
*/
#if DEBUG
static FILE *npuLipLog = NULL;
static char npuLipLogBuf[LogLineLength + 1];
static int npuLipLogBytesCol = 0;
#endif

/*
**--------------------------------------------------------------------------
**
**  Public Functions
**
**--------------------------------------------------------------------------
*/

/*--------------------------------------------------------------------------
**  Purpose:        Handles notification of a network connection from NET.
**
**  Parameters:     Name        Description.
**                  pcbp        pointer to PCB
**                  isPassive   TRUE if passive connection
**
**  Returns:        TRUE is notification processed successfully.
**
**------------------------------------------------------------------------*/
bool npuLipNotifyNetConnect(Pcb *pcbp, bool isPassive)
    {
    npuLipResetPcb(pcbp);
    pcbp->controls.lip.lastExchange = getSeconds();
    if (isPassive)
        {
        pcbp->controls.lip.state = StTrunkRcvConnReq;
#if DEBUG
        fprintf(npuLipLog, "Port %02x: trunk connection indication\n", pcbp->claPort);
#endif
        }
    else
        {
        pcbp->controls.lip.state = StTrunkSndConnReq;
#if DEBUG
        fprintf(npuLipLog, "Port %02x: trunk connection request completed\n", pcbp->claPort);
#endif
        }
    return TRUE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Handles notification of a network disconnection from NET.
**
**  Parameters:     Name        Description.
**                  pcbp        pointer to PCB
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuLipNotifyNetDisconnect(Pcb *pcbp)
    {
    if (pcbp->controls.lip.state > StTrunkSndConnReq)
        {
        npuLipDeactivateTrunk(pcbp);
        }
    npuNetCloseConnection(pcbp);
    pcbp->controls.lip.state = StTrunkDisconnected;
#if DEBUG
    fprintf(npuLipLog, "Port %02x: trunk disconnection indication\n", pcbp->claPort);
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Presets LIP controls in a freshly allocated PCB.
**
**  Parameters:     Name        Description.
**                  pcbp        pointer to PCB
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuLipPresetPcb(Pcb *pcbp)
    {
#if DEBUG
    if (npuLipLog == NULL)
        {
        npuLipLog = fopen("liplog.txt", "wt");
        if (npuLipLog == NULL)
            {
            fprintf(stderr, "liplog.txt - aborting\n");
            exit(1);
            }
        npuLipLogFlush(); // initialize log buffer
        }
#endif
    pcbp->controls.lip.stagingBuf = (u8 *)malloc(MaxBuffer);
    if (pcbp->controls.lip.stagingBuf == NULL)
        {
        fputs("LIP: Failed to allocate buffer for LCB\n", stderr);
        exit(1);
        }
    pcbp->controls.lip.stagingBufPtr = pcbp->controls.lip.stagingBuf;
    npuLipResetPcb(pcbp);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Process upline data from trunk.
**
**  Parameters:     Name        Description.
**                  pcbp        PCB pointer
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuLipProcessUplineData(Pcb *pcbp)
    {
    int inputRemainder;
    int n;
    int stagingCount;

#if DEBUG
    fprintf(npuLipLog, "Port %02x: received data\n", pcbp->claPort);
    npuLipLogBytes(pcbp->inputData, pcbp->inputCount);
    npuLipLogFlush();
#endif

    pcbp->controls.lip.lastExchange = getSeconds();
    pcbp->controls.lip.inputIndex = 0;

    while (pcbp->controls.lip.inputIndex < pcbp->inputCount)
        {
        switch (pcbp->controls.lip.state)
            {
        case StTrunkDisconnected:
            // discard any data received while disconnected
            return;
        case StTrunkRcvConnReq:
            if ((pcbp->controls.lip.stagingBufPtr - pcbp->controls.lip.stagingBuf) + pcbp->inputCount < MaxBuffer)
                {
                memcpy(pcbp->controls.lip.stagingBufPtr, pcbp->inputData, pcbp->inputCount);
                pcbp->controls.lip.stagingBufPtr += pcbp->inputCount;
                *pcbp->controls.lip.stagingBufPtr = 0;
                if (*(pcbp->controls.lip.stagingBufPtr - 1) == (u8)'\n')
                    {
                    if (npuLipProcessConnectRequest(pcbp) == FALSE)
                        {
                        npuNetCloseConnection(pcbp);
                        pcbp->controls.lip.state = StTrunkDisconnected;
                        }
                    }
                }
            else
                {
                fputs("LIP: Staging buffer overflow during connection establishment\n", stderr);
                pcbp->controls.lip.state = StTrunkDisconnected;
                }
            return;
        case StTrunkRcvConnResp:
            if ((pcbp->controls.lip.stagingBufPtr - pcbp->controls.lip.stagingBuf) + pcbp->inputCount < MaxBuffer)
                {
                memcpy(pcbp->controls.lip.stagingBufPtr, pcbp->inputData, pcbp->inputCount);
                pcbp->controls.lip.stagingBufPtr += pcbp->inputCount;
                *pcbp->controls.lip.stagingBufPtr = 0;
                if (*(pcbp->controls.lip.stagingBufPtr - 1) == (u8)'\n')
                    {
                    if (npuLipProcessConnectResponse(pcbp))
                        {
                        pcbp->controls.lip.state = StTrunkRcvBlockLengthHi;
                        }
                    else
                        {
                        npuNetCloseConnection(pcbp);
                        pcbp->controls.lip.state = StTrunkDisconnected;
                        }
                    }
                }
            else
                {
                fputs("LIP: Staging buffer overflow during connection establishiment\n", stderr);
                pcbp->controls.lip.state = StTrunkDisconnected;
                }
            return;
        case StTrunkRcvBlockLengthHi:
            pcbp->controls.lip.state = StTrunkRcvBlockLengthLo;
            pcbp->controls.lip.stagingBufPtr = pcbp->controls.lip.stagingBuf;
            *pcbp->controls.lip.stagingBufPtr = pcbp->inputData[pcbp->controls.lip.inputIndex++];
            break;
        case StTrunkRcvBlockLengthLo:
            pcbp->controls.lip.state = StTrunkRcvBlockContent;
            pcbp->controls.lip.blockLength = (*pcbp->controls.lip.stagingBuf << 8)
                | pcbp->inputData[pcbp->controls.lip.inputIndex++];
            if (pcbp->controls.lip.blockLength > MaxBuffer)
                {
                fprintf(stderr, "LIP: Invalid block length %d received from %s\n",
                    pcbp->controls.lip.blockLength, pcbp->ncbp->hostName);
#if DEBUG
                fprintf(npuLipLog, "Port %02x: invalid block length %d received from %s\n", pcbp->claPort,
                    pcbp->controls.lip.blockLength, pcbp->ncbp->hostName);
#endif
                npuLipNotifyNetDisconnect(pcbp);
                return;
                }
            else if (pcbp->controls.lip.blockLength < 1)
                {
#if DEBUG
                fprintf(npuLipLog, "Port %02x: received ping from %s\n", pcbp->claPort, pcbp->ncbp->hostName);
#endif
                pcbp->controls.lip.state = StTrunkRcvBlockLengthHi;
                }
            break;
        case StTrunkRcvBlockContent:
            stagingCount = pcbp->controls.lip.stagingBufPtr - pcbp->controls.lip.stagingBuf;
            inputRemainder = pcbp->inputCount - pcbp->controls.lip.inputIndex;
            n = (stagingCount + inputRemainder <= pcbp->controls.lip.blockLength)
                ? inputRemainder : pcbp->controls.lip.blockLength - stagingCount;
            memcpy(pcbp->controls.lip.stagingBufPtr, pcbp->inputData + pcbp->controls.lip.inputIndex, n);
            pcbp->controls.lip.stagingBufPtr += n;
            pcbp->controls.lip.inputIndex += n;
            if (pcbp->controls.lip.stagingBufPtr - pcbp->controls.lip.stagingBuf >= pcbp->controls.lip.blockLength)
                {
                npuBipRequestUplineCanned(pcbp->controls.lip.stagingBuf, pcbp->controls.lip.blockLength);
                pcbp->controls.lip.state = StTrunkRcvBlockLengthHi;
                }
            break;
        default:
#if DEBUG
            fprintf(npuLipLog, "Port %02x: invalid LIP state: %d\n", pcbp->claPort, pcbp->controls.lip.state);
#endif
            break;
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Resets LIP controls in a PCB.
**
**  Parameters:     Name        Description.
**                  pcbp        pointer to PCB
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuLipResetPcb(Pcb *pcbp)
    {
    NpuBuffer *bp;

    pcbp->controls.lip.state = StTrunkDisconnected;
    pcbp->controls.lip.lastExchange = 0;
    pcbp->controls.lip.blockLength = 0;
    pcbp->controls.lip.inputIndex = 0;
    pcbp->controls.lip.stagingBufPtr = pcbp->controls.lip.stagingBuf;
    while ((bp = npuBipQueueExtract(&pcbp->controls.lip.outputQ)) != NULL)
        {
        npuBipBufRelease(bp);
        }
#if DEBUG
    fprintf(npuLipLog, "Port %02x: trunk PCB reset\n", pcbp->claPort);
#endif
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
void npuLipTryOutput(Pcb *pcbp)
    {
    switch (pcbp->controls.lip.state)
        {
    case StTrunkDisconnected:
        // do not send output during these states
        break;
    case StTrunkRcvConnReq:
    case StTrunkRcvConnResp:
        if (pcbp->controls.lip.lastExchange > 0
            && (getSeconds() - pcbp->controls.lip.lastExchange) > MaxIdleTime)
            {
#if DEBUG
            fprintf(npuLipLog, "Port %02x: timeout while establishing connection with %s\n",
                pcbp->claPort, pcbp->ncbp->hostName);
#endif
            npuNetCloseConnection(pcbp);
            pcbp->controls.lip.state = StTrunkDisconnected;
            }
        break;
    case StTrunkSndConnReq:
        if (npuLipSendConnectRequest(pcbp))
            {
            pcbp->controls.lip.state = StTrunkRcvConnResp;
            }
        else
            {
            npuNetCloseConnection(pcbp);
            pcbp->controls.lip.state = StTrunkDisconnected;
            }
        break;
    default:
        npuLipSendQueuedData(pcbp);
        break;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Prepare to send data to remote host.
**
**  Parameters:     Name        Description.
**                  data        data address
**                  len         data length
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void npuLipProcessDownlineData(NpuBuffer *bp)
    {
    int claPort;
    u8 dn;
    Pcb *pcbp;

    dn = bp->data[BlkOffDN];

    if (dn == npuSvmCouplerNode)
        {
        npuBipRequestUplineTransfer(bp);
        }
    else
        {
        for (claPort = 0; claPort <= npuNetMaxClaPort; claPort++)
            {
            pcbp = npuNetFindPcb(claPort);
            if (pcbp->ncbp && pcbp->ncbp->connType == ConnTypeTrunk
                && pcbp->controls.lip.remoteNode == dn && pcbp->connFd > 0)
                {
                npuBipQueueAppend(bp, &pcbp->controls.lip.outputQ);
                return;
                }
            }
        fprintf(stderr, "LIP: Block received for unknown or disconnected node %02x\n", dn);
#if DEBUG
        fprintf(npuLipLog, "Block received for unknown or disconnected node: %02x\n", dn);
#endif
        npuBipBufRelease(bp);
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
**  Purpose:        Send a LIP CONNECT request to a peer.  The CONNECT
**                  command specifies the name of the peer and provides
**                  its view of the logical link.  The syntax is:
**
**                    CONNECT <local-name> <local-node> <peer-node>
**
**                  where:
**                    <local-name> : the name of this node (e.g., its PID)
**                    <local-node> : node number of this node's coupler
**                    <peer-node>  : node number of peer node's coupler
**
**  Parameters:     Name        Description.
**                  pcbp        pointer to PCB
**
**  Returns:        TRUE if CONNECT request sent successfully
**
**------------------------------------------------------------------------*/
static bool npuLipSendConnectRequest(Pcb *pcbp)
    {
    char request[256];
    int len;
    int n;

    len = sprintf(request, "CONNECT %s %u %u\n", npuNetHostID, npuSvmCouplerNode, pcbp->controls.lip.remoteNode);
    n = send(pcbp->connFd, request, len, 0);
    if (n == len)
        {
#if DEBUG
        fprintf(npuLipLog, "Port %02x: connect request sent to %s: %s", pcbp->claPort, pcbp->ncbp->hostName, request);
#endif
        return TRUE;
        }
    else
        {
#if DEBUG
        fprintf(npuLipLog, "Port %02x: failed to send connect request to %s: %s", pcbp->claPort, pcbp->ncbp->hostName, request);
#endif
        return FALSE;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Parse a CONNECT request from a peer. The CONNECT request
**                  specifies the name of the peer and provides its view of
**                  the logical link.  The syntax is:
**
**                    CONNECT <peer-name> <peer-node> <local-node>
**
**                  where:
**                    <peer-name>  : the name of the peer (e.g., its node name)
**                    <peer-node>  : node number of peer host's coupler
**                    <local-node> : node number of this host's coupler
**
**  Parameters:     Name        Description.
**                  pcbp        pointer to PCB
**
**  Returns:        TRUE if CONNECT processed successfully
**
**------------------------------------------------------------------------*/
static bool npuLipProcessConnectRequest(Pcb *pcbp)
    {
    int claPort;
    char hostID[HostIdSize];
    int len;
    u8 localNode;
    u8 peerNode;
    char response[256];
    int status;
    char *token;
    Pcb *trunkPcbp;
    long value;

#if DEBUG
    fprintf(npuLipLog, "Port %02x: connect request received: %s\n", pcbp->claPort, pcbp->controls.lip.stagingBuf);
#endif

    /*
    **  Parse CONNECT request
    */
    token = strtok((char *)pcbp->controls.lip.stagingBuf, " \r\n");
    if (token == NULL || strcasecmp(token, "CONNECT") != 0)
        {
        return FALSE;
        }

    /*
    **  Parse peer name.
    */
    token = strtok(NULL, " \r\n");
    len = strlen(token);
    if (token == NULL || len >= sizeof(hostID))
        {
        return FALSE;
        }
    memcpy(hostID, token, len + 1);

    /*
    **  Parse peer's coupler node number.
    */
    peerNode = 0;
    token = strtok(NULL, " ");
    if (token != NULL)
        {
        value = strtol(token, NULL, 10);
        if (value < 1 || value > 255)
            {
            return FALSE;
            }
        peerNode = (u8)value;
        }

    /*
    **  Parse this host's coupler node number, from the peer's perspective.
    */
    localNode = 0;
    token = strtok(NULL, " ");
    if (token != NULL)
        {
        value = strtol(token, NULL, 10);
        if (value < 1 || value > 255)
            {
            return FALSE;
            }
        localNode = (u8)value;
        }

    /*
    **  Find matching trunk definition in Pcb table
    */
    for (claPort = 0; claPort <= npuNetMaxClaPort; claPort++)
        {
        trunkPcbp = npuNetFindPcb(claPort);
        if (trunkPcbp->ncbp != NULL && trunkPcbp->ncbp->connType == ConnTypeTrunk
            && strcasecmp(trunkPcbp->ncbp->hostName, hostID) == 0
            && peerNode == trunkPcbp->controls.lip.remoteNode)
            {
            break;
            }
        }

    if (claPort <= npuNetMaxClaPort)
        {
        if (npuSvmCouplerNode != localNode)
            {
            status = 402;
            len = sprintf(response, "%d %s %u %u unrecognized trunk\n",
                status, npuNetHostID, localNode, peerNode);
            }
        else if (trunkPcbp->connFd > 0 && trunkPcbp != pcbp)
            {
            status = 301;
            len = sprintf(response, "%d %s %u %u already connected\n",
                status, npuNetHostID, npuSvmCouplerNode, peerNode);
            }
        else if (!npuSvmIsReady())
            {
            status = 302;
            len = sprintf(response, "%d %s %u %u not ready\n",
                status, npuNetHostID, npuSvmCouplerNode, peerNode);
            }
        else if (!npuLipActivateTrunk(trunkPcbp))
            {
            status = 501;
            len = sprintf(response, "%d %s %u %u resources unavailable\n",
                status, npuNetHostID, npuSvmCouplerNode, peerNode);
            }
        else
            {
            status = 200;
            len = sprintf(response, "%d %s %u %u connected\n",
                status, npuNetHostID, npuSvmCouplerNode, peerNode);
            }
        }
    else
        {
        status = 401;
        len = sprintf(response, "%d %s %u unknown peer\n", status, hostID, peerNode);
        }

    if (send(pcbp->connFd, response, len, 0) == len)
        {
        if (status == 200)
            {
            if (trunkPcbp != pcbp)
                {
#if DEBUG
                fprintf(npuLipLog, "Port %02x: connection reassigned to port %02x\n", pcbp->claPort, trunkPcbp->claPort);
#endif
                npuLipResetPcb(trunkPcbp);
                trunkPcbp->connFd = pcbp->connFd;
                pcbp->connFd = 0;
                pcbp = trunkPcbp;
                }
            pcbp->controls.lip.state = StTrunkRcvBlockLengthHi;
            pcbp->ncbp->state = StConnConnected;
#if DEBUG
            fprintf(npuLipLog, "Port %02x: connect response sent: %s", pcbp->claPort, response);
#endif
            return TRUE;
            }
        else
            {
#if DEBUG
            fprintf(npuLipLog, "Port %02x: connect response sent: %s", pcbp->claPort, response);
#endif
            return FALSE;
            }
        }
    else
        {
#if DEBUG
        fprintf(npuLipLog, "Port %02x: failed to send connect response\n", pcbp->claPort);
#endif
        return FALSE;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Process a CONNECT response received from a peer.
**
**  Parameters:     Name        Description.
**                  pcbp        pointer to PCB
**
**  Returns:        TRUE if response processed successfully
**
**------------------------------------------------------------------------*/
static bool npuLipProcessConnectResponse(Pcb *pcbp)
    {
    char *token;
    long value;

#if DEBUG
    fprintf(npuLipLog, "Port %02x: received connect response from %s\n", pcbp->claPort, pcbp->ncbp->hostName);
    fprintf(npuLipLog, "  %s", pcbp->controls.lip.stagingBuf);
#endif

    token = strtok((char *)pcbp->controls.lip.stagingBuf, " \r\n");
    if (token == NULL) return -1;

    value = strtol(token, NULL, 10);
    if (value != 200)
        {
        return FALSE;
        }

    /*
    **  Parse peer name.
    */
    token = strtok(NULL, " \r\n");
    if (token == NULL)
        {
        return FALSE;
        }

    if (strcasecmp(token, pcbp->ncbp->hostName) != 0)
        {
#if DEBUG
        fprintf(npuLipLog, "Port %02x: received incorrect host ID '%s' from %s\n", pcbp->claPort,
            token, pcbp->ncbp->hostName);
#endif
        return FALSE;
        }

    /*
    **  Parse peer's coupler node number
    */
    token = strtok(NULL, " \r\n");
    if (token == NULL)
        {
        return FALSE;
        }

    value = strtol(token, NULL, 10);
    if (value != pcbp->controls.lip.remoteNode)
        {
#if DEBUG
        fprintf(npuLipLog, "Port %02x: received incorrect remote node number %ld from %s, expected %d\n", pcbp->claPort,
            value, pcbp->ncbp->hostName, pcbp->controls.lip.remoteNode);
#endif
        return FALSE;
        }

    /*
    **  Parse this host's coupler node number, from peer's perspective
    */
    token = strtok(NULL, " \r\n");
    if (token == NULL)
        {
        return FALSE;
        }

    value = strtol(token, NULL, 10);
    if (value != npuSvmCouplerNode)
        {
#if DEBUG
        fprintf(npuLipLog, "Port %02x: received incorrect local node number %ld from %s, expected %d\n", pcbp->claPort,
            value, pcbp->ncbp->hostName, npuSvmCouplerNode);
#endif
        return FALSE;
        }

    if (npuLipActivateTrunk(pcbp) == FALSE)
        {
#if DEBUG
        fprintf(npuLipLog, "Port %02x: resource exhaustion prevented activation of trunk to %s\n", pcbp->claPort,
            pcbp->ncbp->hostName);
#endif
        return FALSE;
        }

    return TRUE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Send unsolicited logical link regulation level to signal
**                  that host-to-host logical link is available.
**
**  Parameters:     Name        Description.
**                  pcbp        PCB pointer
**
**  Returns:        TRUE if notification queued successfully.
**
**------------------------------------------------------------------------*/
static bool npuLipActivateTrunk(Pcb *pcbp)
    {
    NpuBuffer *bp;
    u8 *mp;

    bp = npuBipBufGet();
    if (bp == NULL)
        {
        return(FALSE);
        }

    mp = bp->data;

    *mp++ = npuSvmCouplerNode;              // DN
    *mp++ = pcbp->controls.lip.remoteNode;  // SN
    *mp++ = 0;                              // CN
    *mp++ = 4;                              // BT=CMD
    *mp++ = 0x01;                           // PFC: Regulation level
    *mp++ = 0x01;                           // SFC: Logical link
    *mp++ = 0x0f;                           // NS=1, CS=1, Regulation level=3

    bp->numBytes = mp - bp->data;

    npuBipRequestUplineTransfer(bp);

#if DEBUG
    fprintf(npuLipLog, "Port %02x: trunk to %s activated\n", pcbp->claPort, pcbp->ncbp->hostName);
#endif

    return TRUE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Send unsolicited logical link regulation level to signal
**                  that host-to-host logical link is unavailable.
**
**  Parameters:     Name        Description.
**                  pcbp        PCB pointer
**
**  Returns:        TRUE if notification queued successfully.
**
**------------------------------------------------------------------------*/
static bool npuLipDeactivateTrunk(Pcb *pcbp)
    {
    NpuBuffer *bp;
    u8 *mp;

    bp = npuBipBufGet();
    if (bp == NULL)
        {
        return(FALSE);
        }

    mp = bp->data;

    *mp++ = npuSvmCouplerNode;              // DN
    *mp++ = pcbp->controls.lip.remoteNode;  // SN
    *mp++ = 0;                              // CN
    *mp++ = 4;                              // BT=CMD
    *mp++ = 0x01;                           // PFC: Regulation level
    *mp++ = 0x01;                           // SFC: Logical link
    *mp++ = 0x0c;                           // NS=1, CS=1, Regulation level=0

    bp->numBytes = mp - bp->data;
 
    npuBipRequestUplineTransfer(bp);

#if DEBUG
    fprintf(npuLipLog, "Port %02x: trunk to %s deactivated\n", pcbp->claPort, pcbp->ncbp->hostName);
#endif

    return TRUE;
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
static void npuLipSendQueuedData(Pcb *pcbp)
    {
    u8 blockLen[2];
    NpuBuffer *bp;
    time_t currentTime;
    int n;
    static u8 ping[] = {0, 0};
#if !defined(_WIN32)
    int i;
    struct iovec vec[2];
#endif

    currentTime = getSeconds();

    /*
    **  Process all queued output buffers.
    */
    if (npuBipQueueNotEmpty(&pcbp->controls.lip.outputQ))
        {
        pcbp->controls.lip.lastExchange = currentTime;
        }
    else
        {
        if (pcbp->controls.lip.lastExchange > 0
            && (currentTime - pcbp->controls.lip.lastExchange) > MaxIdleTime)
            {
            /*
            **  Max idle time exceeded, so try pinging the peer
            */
            if (send(pcbp->connFd, ping, sizeof(ping), 0) == sizeof(ping))
                {
                pcbp->controls.lip.lastExchange = currentTime;
#if DEBUG
                fprintf(npuLipLog, "Port %02x: sent ping to %s\n", pcbp->claPort, pcbp->ncbp->hostName);
#endif
                }
            else
                {
                npuLipNotifyNetDisconnect(pcbp);
                }
            }
        return;
        }
    while ((bp = npuBipQueueExtract(&pcbp->controls.lip.outputQ)) != NULL)
        {
#if defined(_WIN32)
        /*
        **  If the buffer offset is 0, the block length has not been sent yet.
        */
        n = 0;
        if (bp->offset < 1)
            {
            blockLen[0] = bp->numBytes >> 8;
            blockLen[1] = bp->numBytes & 0xff;
            n = send(pcbp->connFd, blockLen, 2, 0);
            if (n < 2)
                {
                n = -1;
                }
#if DEBUG
            else
                {
                fprintf(npuLipLog, "Port %02x: sent data to %s\n", pcbp->claPort, pcbp->ncbp->hostName);
                npuLipLogBytes(blockLen, 2);
                }
#endif
            }
        if (n >= 0)
            {
            n = send(pcbp->connFd, bp->data + bp->offset, bp->numBytes - bp->offset, 0);
#if DEBUG
            if (n > 0)
                {
                npuLipLogBytes(bp->data + bp->offset, bp->numBytes - bp->offset);
                npuLipLogFlush();
                }
#endif
            }
#else
        /*
        **  If the buffer offset is 0, the block length has not been sent yet.
        */
        i = 0;
        if (bp->offset < 1)
            {
            blockLen[0] = bp->numBytes >> 8;
            blockLen[1] = bp->numBytes & 0xff;
            vec[i].iov_base = blockLen;
            vec[i++].iov_len = 2;
            }
        vec[i].iov_base = bp->data + bp->offset;
        n = bp->numBytes - bp->offset;
        vec[i++].iov_len = n;

        n = (n > 0) ? writev(pcbp->connFd, vec, i) : 0;
#if DEBUG
        if (n > 0)
            {
            fprintf(npuLipLog, "Port %02x: sent data to %s\n", pcbp->claPort, pcbp->ncbp->hostName);
            npuLipLogBytes(blockLen, 2);
            npuLipLogBytes(vec[1].iov_base, vec[1].iov_len);
            npuLipLogFlush();
            }
#endif
#endif
        if (n < 0)
            {
            /*
            **  Likely this is a "would block" type of error - requeue the
            **  buffer. The select() call will later tell us when we can send
            **  again. Any disconnects or other errors will be handled by
            **  the receive handler.
            */
            npuBipQueuePrepend(bp, &pcbp->controls.lip.outputQ);
            return;
            }
#if !defined(_WIN32)
        if (i > 1)
            {
            n -= 2; // deduct block length bytes
            if (n < 0)
                {
                npuBipBufRelease(bp);
                fprintf(stderr, "LIP: Failed to send whole block length to %s\n",
                    pcbp->ncbp->hostName);
                npuLipNotifyNetDisconnect(pcbp);
                return;
                }
            }
#endif

        bp->offset += n;

        if (bp->offset >= bp->numBytes)
            {
            npuBipBufRelease(bp);
            }
        else
            {
            /*
            **  Not all has been sent. Put the buffer back into the queue.
            */
            npuBipQueuePrepend(bp, &pcbp->controls.lip.outputQ);
            }
        }
    }

#if DEBUG
/*--------------------------------------------------------------------------
**  Purpose:        Flush incomplete numeric/ascii data line
**
**  Parameters:     Name        Description.
**
**  Returns:        nothing
**
**------------------------------------------------------------------------*/
static void npuLipLogFlush(void)
    {
    if (npuLipLogBytesCol > 0)
        {
        fputs(npuLipLogBuf, npuLipLog);
        fputc('\n', npuLipLog);
        fflush(npuLipLog);
        }
    npuLipLogBytesCol = 0;
    memset(npuLipLogBuf, ' ', LogLineLength);
    npuLipLogBuf[LogLineLength] = '\0';
    }

/*--------------------------------------------------------------------------
**  Purpose:        Log a sequence of EBCDIC bytes in ASCII form
**
**  Parameters:     Name        Description.
**                  bytes       pointer to sequence of bytes
**                  len         length of the sequence
**
**  Returns:        nothing
**
**------------------------------------------------------------------------*/
static void npuLipLogBytes(u8 *bytes, int len)
    {
    int ascCol;
    u8 b;
    char hex[3];
    int hexCol;
    int i;

    ascCol = AsciiColumn(npuLipLogBytesCol);
    hexCol = HexColumn(npuLipLogBytesCol);

    for (i = 0; i < len; i++)
        {
        b = bytes[i];
        sprintf(hex, "%02x", b);
        memcpy(npuLipLogBuf + hexCol, hex, 2);
        hexCol += 3;
        npuLipLogBuf[ascCol++] = (b < 0x20 || b >= 0x7f) ? '.' : b;
        if (++npuLipLogBytesCol >= 16)
            {
            npuLipLogFlush();
            ascCol = AsciiColumn(npuLipLogBytesCol);
            hexCol = HexColumn(npuLipLogBytesCol);
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Log a stack trace
**
**  Parameters:     none
**
**  Returns:        nothing
**
**------------------------------------------------------------------------*/
static void npuLipPrintStackTrace(FILE *fp)
    {
#if defined(__APPLE__)
    void* callstack[128];
    int i;
    int frames;
    char **strs;

    frames = backtrace(callstack, 128);
    strs = backtrace_symbols(callstack, frames);
    for (i = 1; i < frames; ++i)
        {
        fprintf(fp, "%s\n", strs[i]);
        }
    free(strs);
#endif
    }
#endif // DEBUG

/*---------------------------  End Of File  ------------------------------*/
