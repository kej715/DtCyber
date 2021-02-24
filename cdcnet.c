/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2019, Kevin Jordan, Tom Hunter
**
**  Name: cdcnet_gw.c
**
**  Author: Kevin Jordan
**
**  Description:
**      Perform emulation of the Network Host Products TCP/IP gateway running
**      in a CDCNet MDI.
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
**  If this is set to 1, the DEBUG flags in mdi.c and npu_hip.c must also be set to 1.
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
#include <winsock.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/uio.h>
#include <netdb.h>
#include <signal.h>
#endif

/*
**  -----------------
**  Private Constants
**  -----------------
*/
#define cdcnetInitUpline   0x01
#define cdcnetInitDownline 0x02

/* --- Gateway header types --- */
#define cdcnetGwHTIndication  0
#define cdcnetGwHTRequest     0
#define cdcnetGwHTResponse    1

/* --- Gateway TCP version --- */
#define cdcnetGwTcpVersion   0x10

/* --- Offsets common to all gateway commands and responses --- */
#define BlkOffGwCmdName       5
#define BlkOffGwHeaderType   12
#define BlkOffGwHeaderLen    13
#define BlkOffGwDataLen      15
#define BlkOffGwStatus       17
#define BlkOffGwTcpVersion   19

/* --- Offsets to fields in Open SAP command --- */
#define BlkOffGwOSUserSapId  20
#define BlkOffGwOSTcpIpGwVer 24
#define BlkOffGwOSTcpSapId   28

/* --- Offsets to fields in Close SAP command --- */
#define BlkOffGwCSTcpSapId   20

/* --- Offsets to fields in Active Connect command --- */
#define BlkOffGwACTcpSapId   20
#define BlkOffGwACUserCepId  28
#define BlkOffGwACTcpCepId   35
#define BlkOffGwACSrcAddr    50
#define BlkOffGwACDstAddr    80
#define BlkOffGwACAllocation 110
#define BlkOffGwACIpHeader   125
#define BlkOffGwACIpOptions  155
#define BlkOffGwACUlpTimeout 485
#define BlkOffGwACPushFlag   500
#define BlkOffGwACUrgentFlag 500

/* --- Offsets to fields in Passive Connect command --- */
#define BlkOffGwPCTcpSapId   20
#define BlkOffGwPCUserCepId  28
#define BlkOffGwPCTcpCepId   35
#define BlkOffGwPCSrcAddr    50
#define BlkOffGwPCDstAddr    80
#define BlkOffGwPCAllocation 110
#define BlkOffGwPCIpHeader   125
#define BlkOffGwPCIpOptions  155
#define BlkOffGwPCUlpTimeout 485
#define BlkOffGwPCPushFlag   500
#define BlkOffGwPCUrgentFlag 500

/* --- Offsets to fields in Allocation command --- */
#define BlkOffGwATcpCepId    20
#define BlkOffGwASize        28

/* --- Offsets to fields in Status command --- */
#define BlkOffGwSTcpCepId    20
#define BlkOffGwSUserCepId   28
#define BlkOffGwSTcpSapId    35
#define BlkOffGwSUserSapId   43
#define BlkOffGwSSrcAddr     50
#define BlkOffGwSDstAddr     80
#define BlkOffGwSIpHeader    110
#define BlkOffGwSIpOptions   140
#define BlkOffGwSUlpTimeout  470
#define BlkOffGwSTcpStatRec  485

/* --- Offsets to fields in Disconnect command --- */
#define BlkOffGwDTcpCepId    20

/* --- Offsets to fields in Abort Current Connection command --- */
#define BlkOffGwACCTcpCepId  20

/* --- Offsets to fields in Send Data command --- */
#define BlkOffGwSDTcpCepId   20
#define BlkOffGwSDPushFlag   24
#define BlkOffGwSDUrgent     24
#define BlkOffGwSDUlpTimeout 35

/* --- Offsets to fields in Connection Indication --- */
#define BlkOffGwCIUserCepId  20
#define BlkOffGwCISrcAddr    35
#define BlkOffGwCIDstAddr    65
#define BlkOffGwCIIpHeader   95
#define BlkOffGwCIIpOptions  125
#define BlkOffGwCIUlpTimeout 455
#define cdcnetGwCILength     (470 - BlkOffGwCmdName)

/* --- Offsets to fields in Receive indication --- */
#define BlkOffGwRUserCepId   20
#define BlkOffGwRUrgent      24
#define cdcnetGwRLength      (35 - BlkOffGwCmdName)

/* --- Offsets to fields in Close SAP Indication --- */
#define BlkOffGwCSIUserSapId 20
#define cdcnetGwCSILength    (35 - BlkOffGwCmdName)

/* --- Offsets to fields in Abort Indication --- */
#define BlkOffGwABIUserCepId 20
#define cdcnetGwABILength    (35 - BlkOffGwCmdName)

/* --- Offsets to fields in Disconnect Confirmation --- */
#define BlkOffGwDCUserCepId  20
#define cdcnetGwDCLength     (35 - BlkOffGwCmdName)

/* --- Offsets to fields in Disconnect Indication --- */
#define BlkOffGwDIUserCepId  20
#define cdcnetGwDILength     (35 - BlkOffGwCmdName)

/* --- Offsets to fields in Error Indication --- */
#define BlkOffGwEIUserCepId  20
#define cdcnetGwEILength     (35 - BlkOffGwCmdName)

/* --- IP address fields in use indicators --- */
#define cdcnetIpAddrFieldsNone     0
#define cdcnetIpAddrFieldsNetwork  1
#define cdcnetIpAddrFieldsHost     2
#define cdcnetIpAddrFieldsBoth     3

/* --- Relative offsets in TCP address structures --- */
#define RelOffGwIpAddress          0
#define RelOffGwIpAddrFieldsInUse  0
#define RelOffGwIpAddressNetwork   1
#define RelOffGwIpAddressHost      4
#define RelOffGwPortInUse         15
#define RelOffGwPort              16
#define cdcnetGwTcpAddressLength  30

/* --- Relative offsets in ULP timeout structure --- */
#define RelOffGwUlpTimeout         0
#define RelOffGwUlpTimeoutAbort    4

/* --- Offsets to attributes in NAM A-A connection request --- */
#define BlkOffDwnBlkLimit    12
#define BlkOffDwnBlkSize     13
#define BlkOffUplBlkLimit    16
#define BlkOffUplBlkSize     17
#define BlkOffAppName        29

/* --- Reason codes for NAM A-A connection failure --- */
#define cdcnetErrAppMaxConns 20
#define cdcnetErrAppNotAvail 22

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
typedef enum tcpGwStatus // from CDCNet source file
    {
    tcp_successful,           // Function completed.
    tcp_connection_inuse,     // The connection already exists.
    tcp_connection_not_open,  // Exists, only half open.
    tcp_host_unreachable,     // Unable to route.
    tcp_illegal_workcode,     // Work code illegal.
    tcp_internal_error,       // Internal error occurred.
    tcp_invalid_routine,      // NIL pointer to user cmr or lmr.
    tcp_net_unreachable,      // Unable to route.
    tcp_no_connection,        // Connection doesn't exist.
    tcp_no_resources,         // No memory available.
    tcp_option_error,         // Error in IP header option.
    tcp_port_unreachable,     // Unable to route.
    tcp_protocol_error,       // Pdu has fatal error.
    tcp_protocol_unreachable, // Unable to route.
    tcp_remote_abort,         // Abort received from peer.
    tcp_route_failed,         // Source route option bad.
    tcp_sap_not_open,         // The SAPID is invalid.
    tcp_sap_unavailable,      // No SAPs are available.
    tcp_sec$prec_mismatch,    // security/precedence mismatch.
    tcp_ulp_timeout,          // Pdu timed out.
    tcp_not_configured        // TCP has not been defined.
    } TcpGwStatus;

typedef enum cdcnetGwConnState
    {
    StCdcnetGwIdle,
    StCdcnetGwStartingInit,
    StCdcnetGwInitializing,
    StCdcnetGwConnected,
    StCdcnetGwInitiateTermination,
    StCdcnetGwTerminating,
    StCdcnetGwAwaitTermBlock,
    StCdcnetGwError
    } CdcnetGwConnState;

typedef enum tcpConnState
    {
    StTcpIdle,
    StTcpConnecting,
    StTcpIndicatingConnection,
    StTcpListening,
    StTcpConnected,
    StTcpDisconnecting
    } TcpConnState;

typedef enum tcpConnType
    {
    TypeActive,
    TypePassive
    } TcpConnType;

typedef struct tcpGcb // TCP Gateway Control Block
    {
    u16               ordinal;
    CdcnetGwConnState gwState;
    TcpConnState      tcpState;
    TcpConnType       connType;
    u8                initStatus;
    u8                cn;
    u8                bsn;
    u8                unackedBlocks;
    u16               maxUplineBlockSize;
    u32               tcpSapId;
    u32               tcpCepId;
    u32               userSapId;
    u32               userCepId;
    u8                reasonCode;
    u8                tcpSrcAddress[cdcnetGwTcpAddressLength];
    u8                tcpDstAddress[cdcnetGwTcpAddressLength];
    char              SrcIpAddress[20];
    u16               SrcTcpPort;
    char              DstIpAddress[20];
    u16               DstTcpPort;
#if defined(_WIN32)
    SOCKET            connFd;
#else
    int               connFd;
#endif
    u32               localAddr;
    u16               localPort;
    u32               peerAddr;
    u16               peerPort;
    time_t            deadline;
    NpuQueue          downlineQueue;
    NpuQueue          outputQueue;
    } TcpGcb;

typedef struct pccb // Passive Connection Control Block
    {
    u16               ordinal;
    u16               tcpGcbOrdinal;
    u16               SrcTcpPort;
    u16               DstTcpPort;
#if defined(_WIN32)
    SOCKET            connFd;
#else
    int               connFd;
#endif
    time_t            deadline;
    } Pccb;

typedef struct tcpGwCommand
    {
    char *command;
    bool (*handler)(TcpGcb *tp, NpuBuffer *bp);
    } TcpGwCommand;

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static Pccb   *cdcnetAddPccb();
static TcpGcb *cdcnetAddTcpGcb();
static void    cdcnetCloseConnection(TcpGcb *tp);
#if defined(_WIN32)
static SOCKET  cdcnetCreateSocket();
#else
static int     cdcnetCreateSocket();
#endif
static Pccb   *cdcnetFindPccb(u16 port);
static TcpGcb *cdcnetFindTcpGcb(u8 cn);
static u32     cdcnetGetIdFromMessage(u8 *ip);
static u32     cdcnetGetIpAddress(u8 *ap);
static Pccb   *cdcnetGetPccb();
#if defined(_WIN32)
static bool    cdcnetGetEndpoints(SOCKET sd, u32 *localAddr, u16 *localPort,
                                             u32 *peerAddr,  u16 *peerPort);
#else
static bool    cdcnetGetEndpoints(int sd, u32 *localAddr, u16 *localPort,
                                          u32 *peerAddr,  u16 *peerPort);
#endif
static TcpGcb *cdcnetGetTcpGcb();
static u16     cdcnetGetTcpPort(u8 *ap);
static void    cdcnetPutIdToMessage(u32 id, u8 *mp);
static void    cdcnetPutU32ToMessage(u32 value, u8 *mp);
static void    cdcnetSendBack(TcpGcb *tp, NpuBuffer *bp, u8 bsn);
static bool    cdcnetSendInitializeConnectionRequest(TcpGcb *tp);
static void    cdcnetSendInitializeConnectionResponse(NpuBuffer *bp, TcpGcb *tp);
static void    cdcnetSendInitiateConnectionResponse(NpuBuffer *bp, u8 cn, u8 rc);
static void    cdcnetSendTerminateConnectionBlock(NpuBuffer *bp, u8 cn);
static void    cdcnetSendTerminateConnectionRequest(NpuBuffer *bp, u8 cn);
static void    cdcnetSendTerminateConnectionResponse(NpuBuffer *bp, u8 cn);
static void    cdcnetSetIpAddress(u8 *ap, u32 ipAddr);
static void    cdcnetSetTcpPort(u8 *ap, u16 port);
static bool    cdcnetTcpGwAbortCurrentConnectionHandler(TcpGcb *tp, NpuBuffer *bp);
static bool    cdcnetTcpGwActiveConnectHandler(TcpGcb *tp, NpuBuffer *bp);
static bool    cdcnetTcpGwAllocateHandler(TcpGcb *tp, NpuBuffer *bp);
static bool    cdcnetTcpGwCloseSAPHandler(TcpGcb *tp, NpuBuffer *bp);
static bool    cdcnetTcpGwDisconnectHandler(TcpGcb *tp, NpuBuffer *bp);
static bool    cdcnetTcpGwOpenSAPHandler(TcpGcb *tp, NpuBuffer *bp);
static bool    cdcnetTcpGwPassiveConnectHandler(TcpGcb *tp, NpuBuffer *bp);
static bool    cdcnetTcpGwSendConnectionIndication(TcpGcb *tp);
static bool    cdcnetTcpGwSendErrorIndication(TcpGcb *tp);
static void    cdcnetTcpGwSendDataIndication(TcpGcb *tp);
static void    cdcnetGwRequestUplineTransfer(TcpGcb *tp, NpuBuffer *bp, u8 blockType, u8 headerType, TcpGwStatus status);

/*
**  ----------------
**  Public Variables
**  ----------------
*/
u8  cdcnetNode = 255;
u16 cdcnetPrivilegedPortOffset = 6600;

/*
**  -----------------
**  Private Variables
**  -----------------
*/
static u16     cdcnetPassivePort = 7600;
static Pccb   *cdcnetPccbs       = (Pccb *)NULL;
static u8      cdcnetPccbCount   = 0;
static TcpGcb *cdcnetTcpGcbs     = (TcpGcb *)NULL;
static u8      cdcnetTcpGcbCount = 0;

static TcpGwCommand tcpGwCommands [] =
    {
        {"TCPA   ", cdcnetTcpGwAllocateHandler},
        {"TCPAC  ", cdcnetTcpGwActiveConnectHandler},
        {"TCPACC ", cdcnetTcpGwAbortCurrentConnectionHandler},
        {"TCPCS  ", cdcnetTcpGwCloseSAPHandler},
        {"TCPD   ", cdcnetTcpGwDisconnectHandler},
        {"TCPOS  ", cdcnetTcpGwOpenSAPHandler},
        {"TCPPC  ", cdcnetTcpGwPassiveConnectHandler},
/*
        {"TCPS   ", cdcnetTcpGwStatusHandler},
        {"TCPSD  ", cdcnetTcpGwSendDataHandler},
        {"TCPCSI ", cdcnetTcpGwCloseSAPIndicationHandler},
        {"TCPABI ", cdcnetTcpGwAbortIndicationHandler},
        {"TCPDC  ", cdcnetTcpGwDisconnectConfirmationHandler},
        {"TCPDI  ", cdcnetTcpGwDisconnectIndicationHandler},
*/
        {NULL, NULL}
    };

/*
**--------------------------------------------------------------------------
**
**  Public Functions
**
**--------------------------------------------------------------------------
*/

/*--------------------------------------------------------------------------
**  Purpose:        Reset CDCNet Gateway when network is going down.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cdcnetReset(void)
    {
    int i;
    Pccb *pp;
    TcpGcb *tp;

    if (cdcnetTcpGcbCount < 1) return;

    npuLogMessage("CDCNet: Reset gateway");

    /*
    **  Iterate through all TcpGcbs.
    */
    for (i = 0, tp = cdcnetTcpGcbs; i < cdcnetTcpGcbCount; i++, tp++)
        {
        if (tp->gwState != StCdcnetGwIdle)
            {
            cdcnetCloseConnection(tp);
            }
        }

    /*
    **  Iterate through all Pccbs.
    */
    for (i = 0, pp = cdcnetPccbs; i < cdcnetPccbCount; i++, pp++)
        {
        if (pp->connFd != 0)
            {
            #if defined(_WIN32)
                closesocket(pp->connFd);
            #else
                close(pp->connFd);
            #endif
            pp->connFd = 0;
            pp->DstTcpPort = 0;
            pp->tcpGcbOrdinal = 0;
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Process a downline block.
**
**  Parameters:     Name        Description.
**                  bp          pointer to first downline buffer.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cdcnetProcessDownlineData(NpuBuffer *bp)
    {
    char *appName;
    u8 blockType;
    u8 cn;
    int nameLen;
    u8 pfc;
    u8 rc;
    u8 sfc;
    TcpGcb *tp;

    if (bp == NULL)
        {
        return;
        }

    blockType = bp->data[BlkOffBTBSN] & BlkMaskBT;
    cn = bp->data[BlkOffCN];

    switch (blockType)
        {
    case BtHTBLK:
    case BtHTMSG:
        tp = cdcnetFindTcpGcb(cn);
        if (tp != NULL)
            {
            bp->blockSeqNo = (bp->data[BlkOffBTBSN] >> BlkShiftBSN) & BlkMaskBSN;
            npuBipQueueAppend(bp, &tp->downlineQueue);
            }
        else
            {
            npuLogMessage("CDCNet: Connection not found: %02X", cn);
            npuBipBufRelease(bp);
            }
        break;

    case BtHTBACK:
        tp = cdcnetFindTcpGcb(cn);
        if (tp != NULL && tp->unackedBlocks > 0) tp->unackedBlocks--;
        npuBipBufRelease(bp);
        break;

    case BtHTCMD:
        pfc = bp->data[BlkOffPfc];
        sfc = bp->data[BlkOffSfc];

        switch (pfc)
            {
        case 0x02: // initiate connection
            if (sfc == 0x09) // A-A connection
                {
                rc = 0;
                cn = bp->data[BlkOffP3];
                appName = (char *)bp->data + BlkOffAppName;
                nameLen = bp->numBytes - BlkOffAppName;
                tp = cdcnetGetTcpGcb();

                if (nameLen < 9 || strncmp(appName, "GW_TCPIP_", 9) != 0)
                    {
                    npuLogMessage("CDCNet: Application name does not match GW_TCPIP_ ...");
                    rc = cdcnetErrAppNotAvail;
                    }
                else if (tp == NULL)
                    {
                    npuLogMessage("CDCNet: Unable to allocate TCP/IP control block for %*s (CN=%02X)",
                                  nameLen, appName, cn);
                    rc = cdcnetErrAppMaxConns;
                    }
                else
                    {
                    tp->cn = cn;
                    tp->unackedBlocks = 0;
                    tp->maxUplineBlockSize = *(bp->data + BlkOffUplBlkSize) * 100;
                    tp->gwState = StCdcnetGwStartingInit;
                    tp->initStatus = 0;
                    }

                cdcnetSendInitiateConnectionResponse(bp, cn, rc);
                }
            else
                {
                npuLogMessage("CDCNet: Received unexpected command type: %02X/%02X", pfc, sfc);
                npuBipBufRelease(bp);
                }
            break;

        case 0x03: // terminate connection
            if (sfc == 0x08 || sfc == (0x08|SfcResp)) // terminate connection request or response
                {
                cn = bp->data[BlkOffP3];
                tp = cdcnetFindTcpGcb(cn);
                if (tp != NULL)
                    {
                    if (sfc == 0x08) // terminate connection request
                        {
                        cdcnetSendTerminateConnectionBlock(bp, cn);
                        tp->gwState = StCdcnetGwTerminating;
                        }
                    else            // terminate connection response
                        {
                        cdcnetCloseConnection(tp);
                        npuBipBufRelease(bp);
                        }
                    }
                else
                    {
                    npuLogMessage("CDCNet: Connection not found: %02X", cn);
                    npuBipBufRelease(bp);
                    }
                }
            else
                {
                npuLogMessage("CDCNet: Received unexpected command type: %02X/%02X", pfc, sfc);
                npuBipBufRelease(bp);
                }
            break;

        default:
            npuLogMessage("CDCNet: Received unexpected command type: %02X/%02X", pfc, sfc);
            npuBipBufRelease(bp);
            break;
            }
        break;

    case BtHTQBLK:
    case BtHTQMSG:
        tp = cdcnetFindTcpGcb(cn);
        if (tp != NULL)
            {
            bp->blockSeqNo = (bp->data[BlkOffBTBSN] >> BlkShiftBSN) & BlkMaskBSN;
            npuBipQueueAppend(bp, &tp->downlineQueue);
            }
        else
            {
            npuLogMessage("CDCNet: Connection not found: %02X", cn);
            npuBipBufRelease(bp);
            }
        break;

    case BtHTRINIT:
        tp = cdcnetFindTcpGcb(cn);
        if (tp != NULL)
            {
            cdcnetSendInitializeConnectionResponse(bp, tp);
            tp->initStatus |= cdcnetInitDownline;
            if (tp->initStatus == (cdcnetInitDownline | cdcnetInitUpline))
                {
                tp->gwState = StCdcnetGwConnected;
                #if DEBUG
                    npuLogMessage("CDCNet: TCP/IP gateway connection established, CN=%02X", cn);
                #endif
                }
            }
        else
            {
            npuLogMessage("CDCNet: Connection not found: %02X", cn);
            npuBipBufRelease(bp);
            }
        break;

    case BtHTNINIT:
        tp = cdcnetFindTcpGcb(cn);
        if (tp != NULL)
            {
            tp->initStatus |= cdcnetInitUpline;
            if (tp->initStatus == (cdcnetInitDownline | cdcnetInitUpline))
                {
                tp->gwState = StCdcnetGwConnected;
                #if DEBUG
                    npuLogMessage("CDCNet: TCP/IP gateway connection established, CN=%02X", cn);
                #endif
                }
            }
        else
            {
            npuLogMessage("CDCNet: Connection not found: %02X", cn);
            }
        npuBipBufRelease(bp);
        break;

    case BtHTTERM:
        tp = cdcnetFindTcpGcb(cn);
        if (tp != NULL)
            {
            if (tp->gwState == StCdcnetGwAwaitTermBlock)
                {
                cdcnetSendTerminateConnectionBlock(bp, cn); // echo TERM block
                tp->gwState = StCdcnetGwIdle;
                }
            else if (tp->gwState == StCdcnetGwTerminating)
                {
                cdcnetSendTerminateConnectionResponse(bp, cn);
                cdcnetCloseConnection(tp);
                }
            else
                {
                npuLogMessage("CDCNet: Invalid state %d on reception of TERM block: %02X", tp->gwState, cn);
                npuBipBufRelease(bp);
                }
            }
        else
            {
            npuLogMessage("CDCNet: Connection not found: %02X", cn);
            npuBipBufRelease(bp);
            }
        break;

    default:
        npuLogMessage("CDCNet: Received unexpected block type: %02X, CN=%02X", blockType, cn);
        npuBipBufRelease(bp);
        break;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Check TCP/IP Gateway connection status.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cdcnetCheckStatus(void)
    {
    u8 blockType;
    NpuBuffer *bp;
    NpuBuffer *bp2;
    time_t currentTime;
    struct sockaddr_in from;
    TcpGwCommand *gcp;
    int i;
    u32 localAddr;
    u16 localPort;
    int n;
    int optEnable = 1;
    int optVal;
    u32 peerAddr;
    u16 peerPort;
    Pccb *pp;
    fd_set readFds;
    int rc;
    int readySockets;
    struct timeval timeout;
    TcpGcb *tp;
    fd_set writeFds;
#if defined(_WIN32)
    SOCKET fd;
    u_long blockEnable = 1;
    int fromLen;
    SOCKET maxFd;
    int optLen;
#else
    int fd;
    socklen_t fromLen;
    int maxFd;
    socklen_t optLen;
#endif

#if DEBUG
    validateDataStructures();
#endif

    FD_ZERO(&readFds);
    FD_ZERO(&writeFds);
    currentTime = time(NULL);
    maxFd = 0;

    for (i = 0, pp = cdcnetPccbs; i < cdcnetPccbCount; i++, pp++)
        {
        if (pp->connFd > 0)
            {
            if (pp->tcpGcbOrdinal != 0)
                {
                FD_SET(pp->connFd, &readFds);
                if (pp->connFd > maxFd) maxFd = pp->connFd;
                }
            else if (currentTime >= pp->deadline)
                {
                npuLogMessage("CDCNet: Unassigned listen port timeout, port=%d", pp->DstTcpPort);
                #if defined(_WIN32)
                    closesocket(pp->connFd);
                #else
                    close(pp->connFd);
                #endif
                pp->DstTcpPort = 0;
                pp->connFd = 0;
                }
            }
        }

    for (i = 0, tp = cdcnetTcpGcbs; i < cdcnetTcpGcbCount; i++, tp++)
        {
        switch (tp->gwState)
            {
        case StCdcnetGwStartingInit:
            if (cdcnetSendInitializeConnectionRequest(tp))
                {
                tp->gwState = StCdcnetGwInitializing;
                tp->deadline = currentTime + 10;
                }
            break;

        case StCdcnetGwInitializing:
            if (tp->deadline < currentTime)
                {
                npuLogMessage("CDCNet: Connection initialization timed out, CN=%02X", tp->cn);
                cdcnetCloseConnection(tp);
                }
            break;

        case StCdcnetGwInitiateTermination:
            bp = npuBipBufGet();
            if (bp != NULL)
                {
                cdcnetCloseConnection(tp);
                cdcnetSendTerminateConnectionRequest(bp, tp->cn);
                tp->gwState = StCdcnetGwAwaitTermBlock;
                }
            break;

        case StCdcnetGwConnected:

            bp = npuBipQueueExtract(&tp->downlineQueue);

            if (bp != NULL)
                {
                blockType = bp->data[BlkOffBTBSN] & BlkMaskBT;

                switch (blockType)
                    {
                case BtHTBLK:
                case BtHTMSG:
                    bp->offset = BlkOffDbc + 1;
                    npuBipQueueAppend(bp, &tp->outputQueue);
                    break;

                case BtHTQBLK:
                case BtHTQMSG:
                    if (bp->blockSeqNo)
                        {
                        bp2 = npuBipBufGet();
                        if (bp2 == NULL)
                            {
                            npuBipQueuePrepend(bp, &tp->downlineQueue);
                            break;
                            }
                        cdcnetSendBack(tp, bp2, bp->blockSeqNo);
                        bp->blockSeqNo = 0;
                        }
                    for (gcp = tcpGwCommands; gcp->command; gcp++)
                        {
                        if (strncmp(gcp->command, (char *)bp->data + BlkOffGwCmdName, 7) == 0)
                            {
                            if (!(*gcp->handler)(tp, bp))
                                {
                                npuBipQueuePrepend(bp, &tp->downlineQueue);
                                }
                            break;
                            }
                        }
                    if (!gcp->command)
                        {
                        npuLogMessage("CDCNet: Unrecognized TCP gateway command: %7s", (char *)bp->data + BlkOffGwCmdName);
                        npuBipBufRelease(bp);
                        }
                    break;

                default:
                    npuLogMessage("CDCNet: Unexpected block type in cdcnetCheckStatus: %d", blockType);
                    npuBipBufRelease(bp);
                    break;
                    }
                }

            switch (tp->tcpState)
                {
            case StTcpConnecting:
                FD_SET(tp->connFd, &writeFds);
                if (tp->connFd > maxFd) maxFd = tp->connFd;
                break;

            case StTcpIndicatingConnection:
                if (cdcnetTcpGwSendConnectionIndication(tp))
                    {
                    tp->tcpState = StTcpConnected;
                    }
                break;

            case StTcpConnected:
                if (tp->unackedBlocks < 7)
                    {
                    FD_SET(tp->connFd, &readFds);
                    if (tp->connFd > maxFd) maxFd = tp->connFd;
                    }
                if (tp->outputQueue.first != NULL)
                    {
                    FD_SET(tp->connFd, &writeFds);
                    if (tp->connFd > maxFd) maxFd = tp->connFd;
                    }
                break;

            default: // do nothing
                break;
                }

            break;

        case StCdcnetGwError:
            if (cdcnetTcpGwSendErrorIndication(tp))
                {
                tp->gwState = StCdcnetGwConnected;
                }
            break;
            }
        }

    if (maxFd < 1) return;

    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    readySockets = select(maxFd + 1, &readFds, &writeFds, NULL, &timeout);

    if (readySockets < 1) return;

    for (i = 0, pp = cdcnetPccbs; i < cdcnetPccbCount; i++, pp++)
        {
        if (pp->tcpGcbOrdinal != 0 && pp->connFd > 0 && FD_ISSET(pp->connFd, &readFds))
            {
            fromLen = sizeof(from);
            fd = accept(pp->connFd, (struct sockaddr *)&from, &fromLen);

            if (fd > 0)
                {
                if (!cdcnetGetEndpoints(fd, &localAddr, &localPort, &peerAddr, &peerPort))
                    {
                    #if defined(_WIN32)
                        closesocket(fd);
                    #else
                        close(fd);
                    #endif
                    continue;
                    }

                /*
                **  Set Keepalive option so that we can eventually discover if
                **  a peer has been rebooted.
                */
                setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (void *)&optEnable, sizeof(optEnable));

                /*
                **  Make socket non-blocking.
                */
                #if defined(_WIN32)
                    ioctlsocket(fd, FIONBIO, &blockEnable);
                #else
                    fcntl(fd, F_SETFL, O_NONBLOCK);
                #endif
                /*
                **  If the socket is listening for a connection from a specific client
                **  port (e.g., an FTP data connection), close the listening port and
                **  make the passive connection control block available for re-use.
                */
                tp = cdcnetTcpGcbs + (pp->tcpGcbOrdinal - 1);
                if (pp->SrcTcpPort != 0)
                    {
                    npuLogMessage("CDCNet: Close listening socket, port=%d, CN=%02X", pp->DstTcpPort, tp->cn);
                    #if defined(_WIN32)
                        closesocket(pp->connFd);
                    #else
                        close(pp->connFd);
                    #endif
                    pp->DstTcpPort = 0;
                    pp->SrcTcpPort = 0;
                    pp->connFd = 0;
                    }
                else
                    {
                    npuLogMessage("CDCNet: Leave listening socket open, port=%d, CN=%02X", pp->DstTcpPort, tp->cn);
                    pp->deadline = currentTime + 10;
                    }
                pp->tcpGcbOrdinal = 0;
                tp->connFd = fd;
                tp->localAddr = localAddr;
                tp->localPort = localPort;
                tp->peerAddr = peerAddr;
                tp->peerPort = peerPort;
                sprintf(tp->SrcIpAddress, "%d.%d.%d.%d",
                        (peerAddr >> 24) & 0xff,
                        (peerAddr >> 16) & 0xff,
                        (peerAddr >>  8) & 0xff,
                        peerAddr & 0xff);
                tp->SrcTcpPort = peerPort;
                npuLogMessage("CDCNet: Accepted connection, source addr=%s:%u, dest addr=%s:%u, CN=%02X",
                              tp->SrcIpAddress, tp->SrcTcpPort, tp->DstIpAddress, tp->DstTcpPort, tp->cn);
                if (cdcnetTcpGwSendConnectionIndication(tp))
                    {
                    tp->tcpState = StTcpConnected;
                    }
                else
                    {
                    tp->tcpState = StTcpIndicatingConnection;
                    }
                }
            else
                {
                npuLogMessage("CDCNet: Accept of new connection failed on port %d, CN=%02X",
                              tp->DstTcpPort, tp->cn);
                }
            }
        }

    for (i = 0, tp = cdcnetTcpGcbs; i < cdcnetTcpGcbCount; i++, tp++)
        {
        if (tp->gwState != StCdcnetGwConnected) continue;

        switch (tp->tcpState)
            {
        case StTcpConnecting:
            if (FD_ISSET(tp->connFd, &writeFds))
                {
                #if defined(_WIN32)
                    optLen = sizeof(optVal);
                    rc = getsockopt(tp->connFd, SOL_SOCKET, SO_ERROR, (char *)&optVal, &optLen);
                #else
                    optLen = (socklen_t)sizeof(optVal);
                    rc = getsockopt(tp->connFd, SOL_SOCKET, SO_ERROR, &optVal, &optLen);
                #endif
                if (rc < 0)
                    {
                    npuLogMessage("CDCNet: Failed to get socket status for %s:%u, CN=%02X",
                                  tp->DstIpAddress, tp->DstTcpPort, tp->cn);
                    #if defined(_WIN32)
                        closesocket(tp->connFd);
                    #else
                        close(tp->connFd);
                    #endif
                    tp->connFd = 0;
                    tp->tcpState = StTcpIdle;
                    tp->reasonCode = tcp_internal_error;
                    tp->gwState = StCdcnetGwError;
                    }
                else if (optVal != 0) // connection failed
                    {
                    npuLogMessage("CDCNet: Failed to connect to %s:%u, CN=%02X",
                                  tp->DstIpAddress, tp->DstTcpPort, tp->cn);
                    #if defined(_WIN32)
                        closesocket(tp->connFd);
                    #else
                        close(tp->connFd);
                    #endif
                    tp->connFd = 0;
                    tp->tcpState = StTcpIdle;
                    tp->reasonCode = tcp_host_unreachable;
                    tp->gwState = StCdcnetGwError;
                    }
                else if (cdcnetGetEndpoints(tp->connFd, &localAddr, &localPort, &peerAddr, &peerPort))
                    {
                    npuLogMessage("CDCNet: Connected to %s:%u, CN=%02X", tp->DstIpAddress, tp->DstTcpPort, tp->cn);
                    tp->localAddr = localAddr;
                    tp->localPort = localPort;
                    tp->peerAddr = peerAddr;
                    tp->peerPort = peerPort;
                    if (cdcnetTcpGwSendConnectionIndication(tp))
                        {
                        tp->tcpState = StTcpConnected;
                        }
                    else
                        {
                        tp->tcpState = StTcpIndicatingConnection;
                        }
                    }
                else
                    {
                    #if defined(_WIN32)
                        closesocket(tp->connFd);
                    #else
                        close(tp->connFd);
                    #endif
                    tp->connFd = 0;
                    tp->tcpState = StTcpIdle;
                    tp->reasonCode = tcp_host_unreachable;
                    tp->gwState = StCdcnetGwError;
                    }
                }
            else if (currentTime >= tp->deadline)
                {
                npuLogMessage("CDCNet: Connect to %s:%u timed out, CN=%02X", tp->DstIpAddress, tp->DstTcpPort, tp->cn);
                #if defined(_WIN32)
                    closesocket(tp->connFd);
                #else
                    close(tp->connFd);
                #endif
                tp->connFd = 0;
                tp->tcpState = StTcpIdle;
                tp->reasonCode = tcp_host_unreachable;
                tp->gwState = StCdcnetGwError;
                }
            break;

        case StTcpConnected:
            if (FD_ISSET(tp->connFd, &readFds))
                {
                cdcnetTcpGwSendDataIndication(tp);
                }
            if (FD_ISSET(tp->connFd, &writeFds))
                {
                bp = npuBipQueueExtract(&tp->outputQueue);

                if (bp != NULL)
                    {
                    n = send(tp->connFd, bp->data + bp->offset, bp->numBytes - bp->offset, 0);
                    if (n >= 0)
                        {
                        #if DEBUG
                            npuLogMessage("CDCNet: Sent %d bytes to %s:%u, CN=%02X",
                                          n, tp->DstIpAddress, tp->DstTcpPort, tp->cn);
                        #endif
                        bp->offset += n;
                        if (bp->offset >= bp->numBytes)
                            {
                            if (bp->blockSeqNo)
                                {
                                cdcnetSendBack(tp, bp, bp->blockSeqNo);
                                }
                            else
                                {
                                npuBipBufRelease(bp);
                                }
                            }
                        else
                            {
                            npuBipQueuePrepend(bp, &tp->outputQueue);
                            }
                        }
                    else
                        {
                        npuLogMessage("CDCNet: Failed to write to %s:%u, %s, CN=%02X",
                                      tp->DstIpAddress, tp->DstTcpPort, strerror(errno), tp->cn);
                        npuBipBufRelease(bp);
                        while ((bp = npuBipQueueExtract(&tp->outputQueue)) != NULL)
                            {
                            npuBipBufRelease(bp);
                            }
                        tp->reasonCode = tcp_remote_abort;
                        tp->gwState = StCdcnetGwError;
                        }
                    }
                }
            break;

        default: // do nothing
            break;
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
**  Purpose:        Add a TCP/IP gateway control block.
**
**  Parameters:     Name        Description.
**
**  Returns:        Pointer to new TcpGcb
**                  NULL if insufficient resources
**
**------------------------------------------------------------------------*/
static TcpGcb *cdcnetAddTcpGcb()
    {
    TcpGcb *tp;

    tp = (TcpGcb *)realloc(cdcnetTcpGcbs, (cdcnetTcpGcbCount + 1) * sizeof(TcpGcb));
    if (tp == (TcpGcb *)NULL)
        {
        return((TcpGcb *)NULL);
        }

    cdcnetTcpGcbs = tp;
    tp = tp + cdcnetTcpGcbCount++;
    tp->ordinal = cdcnetTcpGcbCount;
    tp->gwState = StCdcnetGwIdle;
    tp->tcpState = StTcpIdle;
    tp->tcpSapId = 0;
    tp->connFd = 0;
    tp->deadline = (time_t)0;
    tp->cn = 0;
    tp->bsn = 1;
    tp->unackedBlocks = 0;
    tp->maxUplineBlockSize = 1000;
    tp->downlineQueue.first = NULL;
    tp->downlineQueue.last = NULL;
    tp->outputQueue.first = NULL;
    tp->outputQueue.last = NULL;

    return(tp);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Get a free TCP/IP gateway control block.
**
**  Parameters:     Name        Description.
**
**  Returns:        Pointer to free TcpGcb
**                  NULL if insufficient resources
**
**------------------------------------------------------------------------*/
static TcpGcb *cdcnetGetTcpGcb()
    {
    int i;
    TcpGcb *tp;

    for (i = 0, tp = cdcnetTcpGcbs; i < cdcnetTcpGcbCount; i++, tp++)
        {
        if (tp->gwState == StCdcnetGwIdle)
            {
            return(tp);
            }
        }

    return cdcnetAddTcpGcb();
    }

/*--------------------------------------------------------------------------
**  Purpose:        Find a TCP/IP gateway control block by connection number.
**
**  Parameters:     Name        Description.
**                  cn          connection number
**
**  Returns:        Pointer to TcpGcb
**                  NULL if control block not found
**
**------------------------------------------------------------------------*/
static TcpGcb *cdcnetFindTcpGcb(u8 cn)
    {
    int i;
    TcpGcb *tp;

    for (i = 0, tp = cdcnetTcpGcbs; i < cdcnetTcpGcbCount; i++, tp++)
        {
        if (tp->gwState != StCdcnetGwIdle && cn == tp->cn)
            {
            return(tp);
            }
        }

    return NULL;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Add a passive connection control block.
**
**  Parameters:     Name        Description.
**
**  Returns:        Pointer to new Pccb
**                  NULL if insufficient resources
**
**------------------------------------------------------------------------*/
static Pccb *cdcnetAddPccb()
    {
    Pccb *pp;

    pp = (Pccb *)realloc(cdcnetPccbs, (cdcnetPccbCount + 1) * sizeof(Pccb));
    if (pp == (Pccb *)NULL)
        {
        return((Pccb *)NULL);
        }

    cdcnetPccbs = pp;
    pp = pp + cdcnetPccbCount++;
    pp->ordinal = cdcnetPccbCount;
    pp->connFd = 0;
    pp->SrcTcpPort = 0;
    pp->DstTcpPort = 0;
    pp->tcpGcbOrdinal = 0;
    pp->deadline = (time_t)0;

    return(pp);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Get a free passive connection control block.
**
**  Parameters:     Name        Description.
**
**  Returns:        Pointer to free Pccb
**                  NULL if insufficient resources
**
**------------------------------------------------------------------------*/
static Pccb *cdcnetGetPccb()
    {
    int i;
    Pccb *pp;

    for (i = 0, pp = cdcnetPccbs; i < cdcnetPccbCount; i++, pp++)
        {
        if (pp->DstTcpPort == 0)
            {
            return(pp);
            }
        }

    return cdcnetAddPccb();
    }

/*--------------------------------------------------------------------------
**  Purpose:        Find a passive connection control block by port number.
**
**  Parameters:     Name        Description.
**                  port        port number
**
**  Returns:        Pointer to Pccb
**                  NULL if control block not found
**
**------------------------------------------------------------------------*/
static Pccb *cdcnetFindPccb(u16 port)
    {
    int i;
    Pccb *pp;

    for (i = 0, pp = cdcnetPccbs; i < cdcnetPccbCount; i++, pp++)
        {
        if (port == pp->DstTcpPort)
            {
            return(pp);
            }
        }

    return NULL;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Get the IP addresses and port numbers associated with
**                  the endpoints of a socket
**
**  Parameters:     Name        Description.
**                  sd          socket descriptor
**
**  Returns:        TRUE if endpoints queried successfully
**                  FALSE if endpoint query failed
**
**------------------------------------------------------------------------*/
#if defined(_WIN32)
static bool cdcnetGetEndpoints(SOCKET sd, u32 *localAddr, u16 *localPort,
                                          u32 *peerAddr,  u16 *peerPort)
#else
static bool cdcnetGetEndpoints(int sd, u32 *localAddr, u16 *localPort,
                                       u32 *peerAddr,  u16 *peerPort)
#endif
    {
    struct sockaddr_in hostAddr;
    int rc;
#if defined(_WIN32)
    int addrLen;
#else
    socklen_t addrLen;
#endif

    addrLen = sizeof(hostAddr);
    rc = getsockname(sd, (struct sockaddr *)&hostAddr, &addrLen);
    if (rc == 0)
        {
        *localAddr = ntohl(hostAddr.sin_addr.s_addr);
        *localPort = ntohs(hostAddr.sin_port);
        }
    else
        {
        fprintf(stderr, "CDCNet: Failed to get local socket name: %s\n", strerror(errno));
        return(FALSE);
        }

    addrLen = sizeof(hostAddr);
    rc = getpeername(sd, (struct sockaddr *)&hostAddr, &addrLen);
    if (rc == 0)
        {
        *peerAddr = ntohl(hostAddr.sin_addr.s_addr);
        *peerPort = ntohs(hostAddr.sin_port);
        }
    else
        {
        fprintf(stderr, "CDCNet: Failed to get peer socket name: %s\n", strerror(errno));
        return(FALSE);
        }
    return(TRUE);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Close a TCP/IP gateway connection.
**
**  Parameters:     Name        Description.
**                  tp          TcpGcb pointer
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cdcnetCloseConnection(TcpGcb *tp)
    {
    NpuBuffer *bp;
    Pccb *pp;

    if (tp->connFd != 0)
        {
        #if defined(_WIN32)
            closesocket(tp->connFd);
        #else
            close(tp->connFd);
        #endif
        tp->connFd = 0;
        }

    pp = cdcnetFindPccb(tp->DstTcpPort);
    if (pp != NULL && pp->tcpGcbOrdinal == tp->ordinal)
        {
        if (pp->SrcTcpPort != 0)
            {
            npuLogMessage("CDCNet: Close listening socket, port=%d, CN=%02X", pp->DstTcpPort, tp->cn);
            #if defined(_WIN32)
                closesocket(pp->connFd);
            #else
                close(pp->connFd);
            #endif
            pp->DstTcpPort = 0;
            pp->connFd = 0;
            }
        else if (pp->connFd > 0)
            {
            pp->deadline = time(NULL) + 10;
            npuLogMessage("CDCNet: Leave listening socket open, port=%d, CN=%02X", pp->DstTcpPort, tp->cn);
            }
        pp->tcpGcbOrdinal = 0;
        }

    tp->gwState = StCdcnetGwIdle;
    tp->tcpState = StTcpIdle;
    tp->initStatus = 0;
    tp->tcpSapId = 0;

    while ((bp = npuBipQueueExtract(&tp->downlineQueue)) != NULL)
        {
        npuBipBufRelease(bp);
        }
    while ((bp = npuBipQueueExtract(&tp->outputQueue)) != NULL)
        {
        npuBipBufRelease(bp);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Send acknowledgement of reception of a block.
**
**  Parameters:     Name        Description.
**                  tp          TcpGcb pointer
**                  bp          pointer to NpuBuffer to use for upline message
**                  bsn         block sequence number to acknowledge
**
**------------------------------------------------------------------------*/
static void cdcnetSendBack(TcpGcb *tp, NpuBuffer *bp, u8 bsn)
    {
    u8 *mp;

    mp = bp->data;

    *mp++ = npuSvmCouplerNode;                   // DN
    *mp++ = cdcnetNode;                          // SN
    *mp++ = tp->cn;                              // CN
    *mp++ = BlkOffBTBSN | (bsn << BlkShiftBSN);  // BT=Back | BSN

    bp->numBytes = mp - bp->data;
    bp->offset = 0;

    npuBipRequestUplineTransfer(bp);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Send initiate connection response.
**
**  Parameters:     Name        Description.
**                  bp          pointer to NpuBuffer to use for upline message
**                  cn          connection number
**                  rc          reason code (0 if normal, non-0 if abnormal)
**
**  Returns:        nothing.
**
**------------------------------------------------------------------------*/
static void cdcnetSendInitiateConnectionResponse(NpuBuffer *bp, u8 cn, u8 rc)
    {
    u8 *mp;

    mp = bp->data;

    *mp++ = npuSvmCouplerNode;                   // DN
    *mp++ = cdcnetNode;                          // SN
    *mp++ = 0;                                   // CN
    *mp++ = BtHTCMD;                             // BT=CMD
    *mp++ = 0x02;                                // PFC: Initiate connection
    *mp++ = (rc == 0 ? SfcResp : SfcErr) | 0x09; // SFC: A-A connection
    *mp++ = cn;
    *mp++ = rc;

    bp->numBytes = mp - bp->data;
    bp->offset = 0;

    npuBipRequestUplineTransfer(bp);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Send initialize connection request.
**
**  Parameters:     Name        Description.
**                  tp          TcpGcb pointer
**
**  Returns:        TRUE if request sent.
**                  FALSE if insufficient resources
**
**------------------------------------------------------------------------*/
static bool cdcnetSendInitializeConnectionRequest(TcpGcb *tp)
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
    *mp++ = cdcnetNode;                     // SN
    *mp++ = tp->cn;                         // CN
    *mp++ = BtHTRINIT;                      // Initialize request

    bp->numBytes = mp - bp->data;

    npuBipRequestUplineTransfer(bp);

    return(TRUE);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Send initialize connection response.
**
**  Parameters:     Name        Description.
**                  bp          pointer to NpuBuffer to use for upline message
**                  tp          TcpGcb pointer
**
**  Returns:        nothing.
**
**------------------------------------------------------------------------*/
static void cdcnetSendInitializeConnectionResponse(NpuBuffer *bp, TcpGcb *tp)
    {
    u8 *mp;

    mp = bp->data;

    *mp++ = npuSvmCouplerNode;              // DN
    *mp++ = cdcnetNode;                     // SN
    *mp++ = tp->cn;                         // CN
    *mp++ = BtHTNINIT;                      // Initialize response

    bp->numBytes = mp - bp->data;
    bp->offset = 0;

    npuBipRequestUplineTransfer(bp);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Send terminate connection block.
**
**  Parameters:     Name        Description.
**                  bp          pointer to NpuBuffer to use for upline message
**                  cn          connection number
**
**  Returns:        nothing.
**
**------------------------------------------------------------------------*/
static void cdcnetSendTerminateConnectionBlock(NpuBuffer *bp, u8 cn)
    {
    u8 *mp;

    mp = bp->data;

    *mp++ = npuSvmCouplerNode; // DN
    *mp++ = cdcnetNode;        // SN
    *mp++ = cn;                // CN
    *mp++ = BtHTTERM;          // BT=TERM

    bp->numBytes = mp - bp->data;
    bp->offset = 0;

    npuBipRequestUplineTransfer(bp);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Send terminate connection request.
**
**  Parameters:     Name        Description.
**                  bp          pointer to NpuBuffer to use for upline message
**                  cn          connection number
**
**  Returns:        nothing.
**
**------------------------------------------------------------------------*/
static void cdcnetSendTerminateConnectionRequest(NpuBuffer *bp, u8 cn)
    {
    u8 *mp;

    mp = bp->data;

    *mp++ = npuSvmCouplerNode; // DN
    *mp++ = cdcnetNode;        // SN
    *mp++ = 0;                 // CN
    *mp++ = BtHTCMD;           // BT=CMD
    *mp++ = 0x03;              // PFC: Terminate connection
    *mp++ = 0x08;              // SFC: Terminate connection
    *mp++ = cn;

    bp->numBytes = mp - bp->data;
    bp->offset = 0;

    npuBipRequestUplineTransfer(bp);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Send terminate connection response.
**
**  Parameters:     Name        Description.
**                  bp          pointer to NpuBuffer to use for upline message
**                  cn          connection number
**
**  Returns:        nothing.
**
**------------------------------------------------------------------------*/
static void cdcnetSendTerminateConnectionResponse(NpuBuffer *bp, u8 cn)
    {
    u8 *mp;

    mp = bp->data;

    *mp++ = npuSvmCouplerNode; // DN
    *mp++ = cdcnetNode;        // SN
    *mp++ = 0;                 // CN
    *mp++ = BtHTCMD;           // BT=CMD
    *mp++ = 0x03;              // PFC: Terminate connection
    *mp++ = SfcResp | 0x08;    // SFC: Terminate connection
    *mp++ = cn;

    bp->numBytes = mp - bp->data;
    bp->offset = 0;

    npuBipRequestUplineTransfer(bp);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Get an IP address from a gateway message.
**
**  Parameters:     Name        Description.
**                  ap          pointer to gateway-encoded IP address structure
**
**  Returns:        Assembled address.
**
**------------------------------------------------------------------------*/
static u32 cdcnetGetIpAddress(u8 *ap)
    {
    u8 inUse;
    u32 ipAddr;

    inUse = ap[RelOffGwIpAddrFieldsInUse];
    ipAddr = 0;

    if (inUse & 0x40)
        {
        ipAddr = (ap[RelOffGwIpAddressNetwork] << 24)
                 | (ap[RelOffGwIpAddressNetwork + 1] << 16)
                 | (ap[RelOffGwIpAddressNetwork + 2] << 8);
        if ((ipAddr & 0xFFFF0000) == 0) // Class A network
            {
            ipAddr <<= 16;
            }
        else if ((ipAddr & 0xFF000000) == 0) // Class B network
            {
            ipAddr <<= 8;
            }
        }
    if ((ipAddr & 0xC0000000) == 0xC0000000) // Class C address
        {
        ipAddr |= ap[RelOffGwIpAddressHost + 2];
        }
    else if (ipAddr & 0x80000000) // Class B address
        {
        ipAddr |= (ap[RelOffGwIpAddressHost + 1] << 8) | ap[RelOffGwIpAddressHost + 2];
        }
    else // Class A address
        {
        ipAddr |= (ap[RelOffGwIpAddressHost] << 16)
                  | (ap[RelOffGwIpAddressHost + 1] << 8)
                  |  ap[RelOffGwIpAddressHost + 2];
        }

    return(ipAddr);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Set an IP address in a gateway message.
**
**  Parameters:     Name        Description.
**                  ap          pointer to gateway-encoded IP address structure
**                  ipAddr      IP address to set
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cdcnetSetIpAddress(u8 *ap, u32 ipAddr)
    {
    ap[RelOffGwIpAddrFieldsInUse] |= 0xC0;

    if ((ipAddr & 0xC0000000) == 0xC0000000) // Class C address
        {
        ap[RelOffGwIpAddressNetwork]     = (ipAddr >> 24) & 0xff;
        ap[RelOffGwIpAddressNetwork + 1] = (ipAddr >> 16) & 0xff;
        ap[RelOffGwIpAddressNetwork + 2] = (ipAddr >>  8) & 0xff;

        ap[RelOffGwIpAddressHost]     = 0;
        ap[RelOffGwIpAddressHost + 1] = 0;
        ap[RelOffGwIpAddressHost + 2] = ipAddr & 0xff;
        }
    else if (ipAddr & 0x80000000) // Class B address
        {
        ap[RelOffGwIpAddressNetwork]     = 0;
        ap[RelOffGwIpAddressNetwork + 1] = (ipAddr >> 24) & 0xff;
        ap[RelOffGwIpAddressNetwork + 2] = (ipAddr >> 16) & 0xff;

        ap[RelOffGwIpAddressHost]     = 0;
        ap[RelOffGwIpAddressHost + 1] = (ipAddr >> 8) & 0xff;
        ap[RelOffGwIpAddressHost + 2] = ipAddr & 0xff;
        }
    else // Class A address
        {
        ap[RelOffGwIpAddressNetwork]     = 0;
        ap[RelOffGwIpAddressNetwork + 1] = 0;
        ap[RelOffGwIpAddressNetwork + 2] = (ipAddr >> 24) & 0xff;

        ap[RelOffGwIpAddressHost]     = (ipAddr >> 16) & 0xff;
        ap[RelOffGwIpAddressHost + 1] = (ipAddr >>  8) & 0xff;
        ap[RelOffGwIpAddressHost + 2] = ipAddr & 0xff;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Get a TCP port number from a gateway message.
**
**  Parameters:     Name        Description.
**                  ap          pointer to gateway-encoded IP address structure
**
**  Returns:        TCP port number; 0 if no port specified.
**
**------------------------------------------------------------------------*/
static u16 cdcnetGetTcpPort(u8 *ap)
    {
    u8 inUse;
    u16 port;

    inUse = ap[RelOffGwPortInUse] & 0x80;
    port = 0;

    if (inUse != 0)
       {
       port = (ap[RelOffGwPort] << 8) | ap[RelOffGwPort + 1];
       }

    return(port);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Set a TCP port number in a gateway message.
**
**  Parameters:     Name        Description.
**                  ap          pointer to gateway-encoded IP address structure
**                  port        TCP port number to set
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cdcnetSetTcpPort(u8 *ap, u16 port)
    {
    ap[RelOffGwPortInUse] |= 0x80;
    ap[RelOffGwPort] = (port >> 8) & 0xff;
    ap[RelOffGwPort + 1] = port & 0xff;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Get a SAP or CEP id from a gateway message.
**
**  Parameters:     Name        Description.
**                  ip          pointer to gateway-encoded SAP/CEP id
**
**  Returns:        SAP/CEP id.
**
**------------------------------------------------------------------------*/
static u32 cdcnetGetIdFromMessage(u8 *ip)
    {
    return((*ip << 24) | (*(ip + 1) << 16) | (*(ip + 2) << 8) | *(ip + 3));
    }

/*--------------------------------------------------------------------------
**  Purpose:        Put a SAP or CEP id into a gateway message.
**
**  Parameters:     Name        Description.
**                  id          SAP/CEP id
**                  mp          pointer to id field in message
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cdcnetPutIdToMessage(u32 id, u8 *mp)
    {
    cdcnetPutU32ToMessage(id, mp);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Put a u16 value into a gateway message.
**
**  Parameters:     Name        Description.
**                  value       u16 value
**                  mp          pointer to field in message
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cdcnetPutU16ToMessage(u16 value, u8 *mp)
    {
    *mp++ = (value >> 8) & 0xff;
    *mp++ = value & 0xff;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Put a u32 value into a gateway message.
**
**  Parameters:     Name        Description.
**                  value       u32 value
**                  mp          pointer to field in message
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cdcnetPutU32ToMessage(u32 value, u8 *mp)
    {
    *mp++ = (value >> 24) & 0xff;
    *mp++ = (value >> 16) & 0xff;
    *mp++ = (value >>  8) & 0xff;
    *mp++ = value & 0xff;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Create a non-blocking socket.
**
**  Parameters:     Name        Description.
**
**  Returns:        Socket descriptor
**                  -1 if socket cannot be created
**
**------------------------------------------------------------------------*/
#if defined(_WIN32)
static SOCKET cdcnetCreateSocket()
#else
static int cdcnetCreateSocket()
#endif
    {
#if defined(_WIN32)
    SOCKET fd;
    u_long blockEnable = 1;
#else
    int fd;
#endif
    int optEnable = 1;

    fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    #if defined(_WIN32)
        if (fd == INVALID_SOCKET) return -1;
        setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (void *)&optEnable, sizeof(optEnable));
        ioctlsocket(fd, FIONBIO, &blockEnable);
    #else
        if (fd < 0) return -1;
        setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (void *)&optEnable, sizeof(optEnable));
        fcntl(fd, F_SETFL, O_NONBLOCK);
    #endif

    return(fd);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Queue a block for upline transfer.
**
**  Parameters:     Name        Description.
**                  tp          pointer to TCP/IP gateway control block
**                  bp          pointer to NpuBuffer containing block
**                  blockType   block type (BtHTMSG or BtHTQMSG)
**                  headerType  TCP/IP gateway header type
**                  status      TCP/IP gateway response/indication status
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cdcnetGwRequestUplineTransfer(TcpGcb *tp, NpuBuffer *bp, u8 blockType, u8 headerType, TcpGwStatus status)
    {
    bp->data[BlkOffDN] = npuSvmCouplerNode;
    bp->data[BlkOffSN] = cdcnetNode;
    bp->data[BlkOffCN] = tp->cn;
    bp->data[BlkOffBTBSN] = (tp->bsn++ << BlkShiftBSN) | blockType;
    if (tp->bsn > 7) tp->bsn = 1;
    bp->data[BlkOffDbc] = 0;
    if (blockType == BtHTQMSG)
        {
        bp->data[BlkOffGwStatus]     = (status >> 8) & 0xff;
        bp->data[BlkOffGwStatus + 1] = status & 0xff;
        bp->data[BlkOffGwHeaderType] = headerType;
        }
    tp->unackedBlocks++;
    npuBipRequestUplineTransfer(bp);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Handle TCP Gateway Open SAP request.
**
**  Parameters:     Name        Description.
**                  tp          pointer to TCP/IP gateway control block
**                  bp          pointer to NpuBuffer containing request
**
**  Returns:        TRUE if request handled and response sent successfully
**                  FALSE if request should be retried
**
**------------------------------------------------------------------------*/
static bool cdcnetTcpGwOpenSAPHandler(TcpGcb *tp, NpuBuffer *bp)
    {
    tp->userSapId = cdcnetGetIdFromMessage(bp->data + BlkOffGwOSUserSapId);
    npuLogMessage("CDCNet: Open SAP request, gwVersion=%02X, tcpSapId=%d, userSapId=%d, CN=%02X",
                  bp->data[BlkOffGwOSTcpIpGwVer], tp->ordinal, tp->userSapId, tp->cn);
    tp->tcpSapId = (u32)tp->ordinal;
    cdcnetPutIdToMessage(tp->tcpSapId, bp->data + BlkOffGwOSTcpSapId);
    cdcnetGwRequestUplineTransfer(tp, bp, BtHTQMSG, cdcnetGwHTResponse, tcp_successful);
    return(TRUE);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Handle TCP Gateway Close SAP request.
**
**  Parameters:     Name        Description.
**                  tp          pointer to TCP/IP gateway control block
**                  bp          pointer to NpuBuffer containing request
**
**  Returns:        TRUE if request handled and response sent successfully
**                  FALSE if request should be retried
**
**------------------------------------------------------------------------*/
static bool cdcnetTcpGwCloseSAPHandler(TcpGcb *tp, NpuBuffer *bp)
    {
    int i;
    TcpGcb *tp2;
    u32 tcpSapId;

    tcpSapId = cdcnetGetIdFromMessage(bp->data + BlkOffGwCSTcpSapId);
    npuLogMessage("CDCNet: Close SAP request, tcpSapId=%d, CN=%02X", tcpSapId, tp->cn);

    for (i = 0, tp2 = cdcnetTcpGcbs; i < cdcnetTcpGcbCount; i++, tp2++)
        {
        if (tp2->tcpSapId == tcpSapId && tp2->tcpState != StTcpIdle)
            {
            cdcnetCloseConnection(tp2);
            }
        }

    cdcnetGwRequestUplineTransfer(tp, bp, BtHTQMSG, cdcnetGwHTResponse, tcp_successful);
    return(TRUE);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Handle TCP Gateway request to abort the current connection
**
**  Parameters:     Name        Description.
**                  tp          pointer to TCP/IP gateway control block
**                  bp          pointer to NpuBuffer containing request
**
**  Returns:        TRUE if request handled and response sent successfully
**                  FALSE if request should be retried
**
**------------------------------------------------------------------------*/
static bool cdcnetTcpGwAbortCurrentConnectionHandler(TcpGcb *tp, NpuBuffer *bp)
    {
    npuLogMessage("CDCNet: Abort current connection request, tcpCepId=%d, size=%08X, CN=%02X",
                  cdcnetGetIdFromMessage(bp->data + BlkOffGwACCTcpCepId), tp->cn);
    if (tp->connFd != 0)
        {
        #if defined(_WIN32)
            closesocket(tp->connFd);
        #else
            close(tp->connFd);
        #endif
        tp->connFd = 0;
        }
    tp->tcpState = StTcpIdle;
    cdcnetGwRequestUplineTransfer(tp, bp, BtHTQMSG, cdcnetGwHTResponse, tcp_successful);
    return(TRUE);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Handle TCP Gateway Active Connect request.
**
**  Parameters:     Name        Description.
**                  tp          pointer to TCP/IP gateway control block
**                  bp          pointer to NpuBuffer containing request
**
**  Returns:        TRUE if request handled and response sent successfully
**                  FALSE if request should be retried
**
**------------------------------------------------------------------------*/
static bool cdcnetTcpGwActiveConnectHandler(TcpGcb *tp, NpuBuffer *bp)
    {
    u8 *dp;
    u32 dstAddr;
    struct sockaddr_in hostAddr;
    u32 localAddr;
    u16 localPort;
    u32 peerAddr;
    u16 peerPort;
    int rc;
    u32 srcAddr;
    TcpGwStatus status;

    tp->connType = TypeActive;
    tp->tcpSapId = cdcnetGetIdFromMessage(bp->data + BlkOffGwACTcpSapId);
    tp->userCepId = cdcnetGetIdFromMessage(bp->data + BlkOffGwACUserCepId);
    tp->tcpCepId = 0;
    memcpy(tp->tcpSrcAddress, bp->data + BlkOffGwACSrcAddr, cdcnetGwTcpAddressLength);
    memcpy(tp->tcpDstAddress, bp->data + BlkOffGwACDstAddr, cdcnetGwTcpAddressLength);
    dp = bp->data + BlkOffGwACSrcAddr;
    srcAddr = cdcnetGetIpAddress(dp);
    sprintf(tp->SrcIpAddress, "%d.%d.%d.%d", srcAddr >> 24, (srcAddr >> 16) & 0xff, (srcAddr >> 8) & 0xff, srcAddr & 0xff);
    tp->SrcTcpPort = cdcnetGetTcpPort(dp);
    dp = bp->data + BlkOffGwACDstAddr;
    dstAddr = cdcnetGetIpAddress(dp);
    sprintf(tp->DstIpAddress, "%d.%d.%d.%d", dstAddr >> 24, (dstAddr >> 16) & 0xff, (dstAddr >> 8) & 0xff, dstAddr & 0xff);
    tp->DstTcpPort = cdcnetGetTcpPort(dp);
    npuLogMessage("CDCNet: Active Connect request, tcpSapId=%d, userCepId=%d, tcpCepId=%d, CN=%02X\n        source addr=%s:%d, dest addr=%s:%d",
                  tp->tcpSapId, tp->userCepId, tp->tcpCepId, tp->cn,
                  tp->SrcIpAddress, tp->SrcTcpPort, tp->DstIpAddress, tp->DstTcpPort);
    status = tcp_successful;

    if (tp->tcpState == StTcpIdle)
        {
        tp->connFd = cdcnetCreateSocket();
        if (tp->connFd == -1)
            {
            cdcnetGwRequestUplineTransfer(tp, bp, BtHTQMSG, cdcnetGwHTResponse, tcp_no_resources);
            return(TRUE);
            }
        hostAddr.sin_addr.s_addr = htonl(dstAddr);
        hostAddr.sin_family = AF_INET;
        hostAddr.sin_port = htons(tp->DstTcpPort);
        rc = connect(tp->connFd, (struct sockaddr *)&hostAddr, sizeof(hostAddr));
        #if defined(_WIN32)
            if (rc == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)
                {
                npuLogMessage("CDCNet: Failed to connect to host: %s:%u, CN=%02X",
                              tp->DstIpAddress, tp->DstTcpPort, tp->cn);
                closesocket(tp->connFd);
                tp->connFd = 0;
                status = tcp_host_unreachable;
                }
            else if (rc == 0) // connection succeeded immediately
                {
                if (cdcnetGetEndpoints(tp->connFd, &localAddr, &localPort, &peerAddr, &peerPort))
                    {
                    npuLogMessage("CDCNet: Connected to host: %s:%u, CN=%02X", tp->DstIpAddress, tp->DstTcpPort, tp->cn);
                    tp->localAddr = localAddr;
                    tp->localPort = localPort;
                    tp->peerAddr = peerAddr;
                    tp->peerPort = peerPort;
                    if (cdcnetTcpGwSendConnectionIndication(tp))
                        {
                        tp->tcpState = StTcpConnected;
                        }
                    else
                        {
                        tp->tcpState = StTcpIndicatingConnection;
                        }
                    }
                else
                    {
                    closesocket(tp->connFd);
                    tp->connFd = 0;
                    status = tcp_host_unreachable;
                    }
                }
            else // connection in progress
                {
                npuLogMessage("CDCNet: Initiated connection to host: %s:%u, CN=%02X",
                              tp->DstIpAddress, tp->DstTcpPort, tp->cn);
                tp->tcpState = StTcpConnecting;
                tp->deadline = time(NULL) + 60;
                }
        #else
            if (rc < 0 && errno != EINPROGRESS)
                {
                npuLogMessage("CDCNet: Failed to connect to host %s:%u, %s, CN=%02X",
                              tp->DstIpAddress, tp->DstTcpPort, strerror(errno), tp->cn);
                close(tp->connFd);
                tp->connFd = 0;
                status = tcp_host_unreachable;
                }
            else if (rc == 0) // connection succeeded immediately
                {
                if (cdcnetGetEndpoints(tp->connFd, &localAddr, &localPort, &peerAddr, &peerPort))
                    {
                    npuLogMessage("CDCNet: Connected to host: %s:%u, CN=%02X", tp->DstIpAddress, tp->DstTcpPort, tp->cn);
                    tp->localAddr = localAddr;
                    tp->localPort = localPort;
                    tp->peerAddr = peerAddr;
                    tp->peerPort = peerPort;
                    if (cdcnetTcpGwSendConnectionIndication(tp))
                        {
                        tp->tcpState = StTcpConnected;
                        }
                    else
                        {
                        tp->tcpState = StTcpIndicatingConnection;
                        }
                    }
                else
                    {
                    close(tp->connFd);
                    tp->connFd = 0;
                    status = tcp_host_unreachable;
                    }
                }
            else // connection in progress
                {
                npuLogMessage("CDCNet: Initiated connection to host: %s:%u, CN=%02X",
                              tp->DstIpAddress, tp->DstTcpPort, tp->cn);
                tp->tcpState = StTcpConnecting;
                tp->deadline = time(NULL) + 60;
                }
        #endif
        }

    cdcnetGwRequestUplineTransfer(tp, bp, BtHTQMSG, cdcnetGwHTResponse, status);
    return(TRUE);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Handle TCP Gateway Passive Connect request.
**
**  Parameters:     Name        Description.
**                  tp          pointer to TCP/IP gateway control block
**                  bp          pointer to NpuBuffer containing request
**
**  Returns:        TRUE if request handled and response sent successfully
**                  FALSE if request should be retried
**
**------------------------------------------------------------------------*/
static bool cdcnetTcpGwPassiveConnectHandler(TcpGcb *tp, NpuBuffer *bp)
    {
    u8 *dp;
    u32 dstAddr;
    u16 dstPort;
    int optEnable = 1;
    Pccb *pp;
    struct sockaddr_in server;
    u32 srcAddr;
    TcpGwStatus status;

    tp->connType = TypePassive;
    tp->tcpSapId = cdcnetGetIdFromMessage(bp->data + BlkOffGwPCTcpSapId);
    tp->userCepId = cdcnetGetIdFromMessage(bp->data + BlkOffGwPCUserCepId);
    tp->tcpCepId = (u32)tp->ordinal;
    memcpy(tp->tcpDstAddress, bp->data + BlkOffGwPCSrcAddr, cdcnetGwTcpAddressLength);
    memcpy(tp->tcpSrcAddress, bp->data + BlkOffGwPCDstAddr, cdcnetGwTcpAddressLength);
    dp = bp->data + BlkOffGwPCSrcAddr;
    dstAddr = cdcnetGetIpAddress(dp);
    sprintf(tp->DstIpAddress, "%d.%d.%d.%d", dstAddr >> 24, (dstAddr >> 16) & 0xff, (dstAddr >> 8) & 0xff, dstAddr & 0xff);
    dstPort = cdcnetGetTcpPort(dp);
    if (dstPort == 0) // assign next available port
        {
        if (++cdcnetPassivePort >= 10000) cdcnetPassivePort = 7600;
        tp->DstTcpPort = cdcnetPassivePort;
        }
    else if (dstPort < 1024) // add offset to privileged port number
        {
        tp->DstTcpPort = dstPort + cdcnetPrivilegedPortOffset;
        }
    else
        {
        tp->DstTcpPort = dstPort;
        }
    dp = bp->data + BlkOffGwPCDstAddr;
    srcAddr = cdcnetGetIpAddress(dp);
    sprintf(tp->SrcIpAddress, "%d.%d.%d.%d", srcAddr >> 24, (srcAddr >> 16) & 0xff, (srcAddr >> 8) & 0xff, srcAddr & 0xff);
    tp->SrcTcpPort = cdcnetGetTcpPort(dp);
    npuLogMessage("CDCNet: Passive Connect request, tcpSapId=%d, userCepId=%d, tcpCepId=%d, CN=%02X\n        source addr=%s:%d, dest addr=%s:%d",
                  tp->tcpSapId, tp->userCepId, tp->tcpCepId, tp->cn,
                  tp->SrcIpAddress, tp->SrcTcpPort, tp->DstIpAddress, tp->DstTcpPort);
    status = tcp_successful;

    if (tp->tcpState == StTcpIdle)
        {
        pp = cdcnetFindPccb(tp->DstTcpPort);
        if (pp != NULL)
            {
            /*
            ** The gateway is qlready bound and listening to the requested port,
            ** so reserve and use it, unless it's already being used by another
            ** connection.
            */
            if (pp->tcpGcbOrdinal == 0) // available for re-use
                {
                npuLogMessage("CDCNet: Listening for connections (re-use) on port %d, CN=%02X", tp->DstTcpPort, tp->cn);
                pp->SrcTcpPort = tp->SrcTcpPort;
                pp->tcpGcbOrdinal = tp->ordinal;
                tp->tcpState = StTcpListening;
                }
            else // in use by another connection
                {
                npuLogMessage("CDCNet: Connection in use by %02X, port=%u, CN=%02X",
                              (cdcnetTcpGcbs + (pp->tcpGcbOrdinal - 1))->cn, tp->DstTcpPort, tp->cn);
                status = tcp_connection_inuse;
                }
            cdcnetGwRequestUplineTransfer(tp, bp, BtHTQMSG, cdcnetGwHTResponse, status);
            return(TRUE);
            }

        pp = cdcnetGetPccb();
        if (pp == NULL)
            {
            npuLogMessage("CDCNet: Failed to get/create a control block for a passive connection on port %d, CN=%02X",
                          tp->DstTcpPort, tp->cn);
            cdcnetGwRequestUplineTransfer(tp, bp, BtHTQMSG, cdcnetGwHTResponse, tcp_no_resources);
            return(TRUE);
            }

        pp->connFd = cdcnetCreateSocket();
        if (pp->connFd == -1)
            {
            cdcnetGwRequestUplineTransfer(tp, bp, BtHTQMSG, cdcnetGwHTResponse, tcp_no_resources);
            return(TRUE);
            }

        pp->SrcTcpPort = tp->SrcTcpPort;
        pp->DstTcpPort = tp->DstTcpPort;
        pp->tcpGcbOrdinal = tp->ordinal;
        setsockopt(pp->connFd, SOL_SOCKET, SO_REUSEADDR, (void *)&optEnable, sizeof(optEnable));
        memset(&server, 0, sizeof(server));
        server.sin_family = AF_INET;
        server.sin_addr.s_addr = inet_addr("0.0.0.0");

        while (TRUE)
            {
            server.sin_port = htons(pp->DstTcpPort);

            if (bind(pp->connFd, (struct sockaddr *)&server, sizeof(server)) == 0) break;

            npuLogMessage("CDCNet: Can't bind to listening socket on port %d, %s, CN=%02X",
                          pp->DstTcpPort, strerror(errno), tp->cn);
            if (dstPort == 0)
                {
                if (++cdcnetPassivePort >= 10000) cdcnetPassivePort = 7600;
                pp->DstTcpPort = cdcnetPassivePort;
                }
            else
                {
                pp->DstTcpPort = 0;
                pp->tcpGcbOrdinal = 0;
                #if defined(_WIN32)
                    closesocket(pp->connFd);
                #else
                    close(pp->connFd);
                #endif
                cdcnetGwRequestUplineTransfer(tp, bp, BtHTQMSG, cdcnetGwHTResponse, tcp_internal_error);
                return(TRUE);
                }
            }

        tp->DstTcpPort = pp->DstTcpPort;

        /*
        **  Start listening for new connections on this TCP port number
        */
        if (listen(pp->connFd, 10) < 0)
            {
            npuLogMessage("CDCNet: Can't listen, %s, CN=%02X", strerror(errno), tp->cn);
            pp->DstTcpPort = 0;
            pp->tcpGcbOrdinal = 0;
            #if defined(_WIN32)
                closesocket(pp->connFd);
            #else
                close(pp->connFd);
            #endif
            cdcnetGwRequestUplineTransfer(tp, bp, BtHTQMSG, cdcnetGwHTResponse, tcp_internal_error);
            return(TRUE);
            }

        npuLogMessage("CDCNet: Listening for connections on port %d, CN=%02X", tp->DstTcpPort, tp->cn);

        tp->tcpState = StTcpListening;
        cdcnetSetTcpPort(bp->data + BlkOffGwPCSrcAddr, tp->DstTcpPort);
        cdcnetPutIdToMessage(tp->tcpCepId, bp->data + BlkOffGwPCTcpCepId);
        }
    else // connection not idle
        {
        npuLogMessage("CDCNet: Attempted to create passive connection on non-idle connection, CN=%02X", tp->cn);
        status = tcp_connection_inuse;
        }

    cdcnetGwRequestUplineTransfer(tp, bp, BtHTQMSG, cdcnetGwHTResponse, status);
    return(TRUE);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Handle TCP Gateway Allocate request.
**
**  Parameters:     Name        Description.
**                  tp          pointer to TCP/IP gateway control block
**                  bp          pointer to NpuBuffer containing request
**
**  Returns:        TRUE if request handled and response sent successfully
**                  FALSE if request should be retried
**
**------------------------------------------------------------------------*/
static bool cdcnetTcpGwAllocateHandler(TcpGcb *tp, NpuBuffer *bp)
    {
    npuLogMessage("CDCNet: Allocate request, tcpCepId=%d, size=%08X, CN=%02X",
                  cdcnetGetIdFromMessage(bp->data + BlkOffGwATcpCepId),
                  cdcnetGetIdFromMessage(bp->data + BlkOffGwASize), tp->cn);
    cdcnetGwRequestUplineTransfer(tp, bp, BtHTQMSG, cdcnetGwHTResponse, tcp_successful);
    return(TRUE);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Handle TCP Gateway Disconnect request.
**
**  Parameters:     Name        Description.
**                  tp          pointer to TCP/IP gateway control block
**                  bp          pointer to NpuBuffer containing request
**
**  Returns:        TRUE if request handled and response sent successfully
**                  FALSE if request should be retried
**
**------------------------------------------------------------------------*/
static bool cdcnetTcpGwDisconnectHandler(TcpGcb *tp, NpuBuffer *bp)
    {
    Pccb *pp;

    npuLogMessage("CDCNet: Disconnect request, tcpCepId=%d, CN=%02X",
                  cdcnetGetIdFromMessage(bp->data + BlkOffGwDTcpCepId), tp->cn);
    memcpy(bp->data + BlkOffGwCmdName, "TCPDC  ", 7);
    cdcnetPutU16ToMessage(cdcnetGwDCLength, bp->data + BlkOffGwHeaderLen);
    bp->data[BlkOffGwTcpVersion] = cdcnetGwTcpVersion;
    cdcnetPutIdToMessage(tp->userCepId, bp->data + BlkOffGwDCUserCepId);
    bp->numBytes = cdcnetGwDCLength + BlkOffGwCmdName;
    cdcnetGwRequestUplineTransfer(tp, bp, BtHTQMSG, cdcnetGwHTIndication, tcp_successful);

    if (tp->tcpState == StTcpListening)
        {
        pp = cdcnetFindPccb(tp->DstTcpPort);
        if (pp != NULL && pp->tcpGcbOrdinal == tp->ordinal)
            {
            if (pp->SrcTcpPort != 0)
                {
                npuLogMessage("CDCNet: Close listening socket, port=%d, CN=%02X", pp->DstTcpPort, tp->cn);
                #if defined(_WIN32)
                    closesocket(pp->connFd);
                #else
                    close(pp->connFd);
                #endif
                pp->DstTcpPort = 0;
                pp->SrcTcpPort = 0;
                pp->connFd = 0;
                }
#if DEBUG
            else
                {
                npuLogMessage("CDCNet: Leave listening socket open, port=%d, CN=%02X", pp->DstTcpPort, tp->cn);
                }
#endif
            pp->tcpGcbOrdinal = 0;
            }
        }
    else if (tp->connFd != 0)
        {
        npuLogMessage("CDCNet: Close socket, source addr=%s:%d, dest addr=%s:%d, CN=%02X",
                      tp->SrcIpAddress, tp->SrcTcpPort, tp->DstIpAddress, tp->DstTcpPort, tp->cn);
        #if defined(_WIN32)
            closesocket(tp->connFd);
        #else
            close(tp->connFd);
        #endif
        tp->connFd = 0;
        }

    tp->tcpState = StTcpIdle;
    return(TRUE);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Send indication that a TCP connection has been established.
**
**  Parameters:     Name        Description.
**                  tp          pointer to TCP/IP gateway control block
**
**  Returns:        TRUE if indication sent successfully
**                  FALSE if indication should be retried
**
**------------------------------------------------------------------------*/
static bool cdcnetTcpGwSendConnectionIndication(TcpGcb *tp)
    {
    NpuBuffer *bp;

    bp = npuBipBufGet();
    if (bp == NULL) return(FALSE);
    memset(bp->data, 0, cdcnetGwCILength + BlkOffGwCmdName);

    npuLogMessage("CDCNet: Send connection indication, userCepId=%d, CN=%02X", tp->userCepId, tp->cn);
    memcpy(bp->data + BlkOffGwCmdName, "TCPCI  ", 7);
    cdcnetPutU16ToMessage(cdcnetGwCILength, bp->data + BlkOffGwHeaderLen);
    bp->data[BlkOffGwTcpVersion] = cdcnetGwTcpVersion;
    cdcnetPutIdToMessage(tp->userCepId, bp->data + BlkOffGwCIUserCepId);
    if (tp->connType == TypeActive)
        {
        cdcnetSetIpAddress(tp->tcpSrcAddress, tp->localAddr);
        tp->SrcTcpPort = tp->localPort;
        cdcnetSetTcpPort(tp->tcpSrcAddress, tp->localPort);
        cdcnetSetIpAddress(tp->tcpDstAddress, tp->peerAddr);
        tp->DstTcpPort = tp->peerPort;
        cdcnetSetTcpPort(tp->tcpDstAddress, tp->peerPort);
        tp->tcpCepId = (u32)tp->ordinal;
        }
    else
        {
        cdcnetSetIpAddress(tp->tcpSrcAddress, tp->peerAddr);
        tp->SrcTcpPort = tp->peerPort;
        cdcnetSetTcpPort(tp->tcpSrcAddress, tp->peerPort);
        cdcnetSetIpAddress(tp->tcpDstAddress, tp->localAddr);
        if (tp->localPort >= cdcnetPrivilegedPortOffset &&
            tp->localPort <  cdcnetPrivilegedPortOffset + 1024)
            {
            cdcnetSetTcpPort(tp->tcpDstAddress, tp->localPort - cdcnetPrivilegedPortOffset);
            }
        else
            {
            cdcnetSetTcpPort(tp->tcpDstAddress, tp->localPort);
            }
        }
    memcpy(bp->data + BlkOffGwCISrcAddr, tp->tcpSrcAddress, cdcnetGwTcpAddressLength);
    memcpy(bp->data + BlkOffGwCIDstAddr, tp->tcpDstAddress, cdcnetGwTcpAddressLength);
    memset(bp->data + BlkOffGwCIIpHeader, 0, BlkOffGwCIIpOptions - BlkOffGwCIIpHeader);
    memset(bp->data + BlkOffGwCIIpOptions, 0, BlkOffGwCIUlpTimeout - BlkOffGwCIIpOptions);
    memset(bp->data + BlkOffGwCIIpHeader, 0, (cdcnetGwCILength + BlkOffGwCmdName) - BlkOffGwCIUlpTimeout);
    bp->numBytes = cdcnetGwCILength + BlkOffGwCmdName;
    cdcnetGwRequestUplineTransfer(tp, bp, BtHTQMSG, cdcnetGwHTIndication, tcp_successful);
    return(TRUE);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Send an indication that an error has occurred.
**
**  Parameters:     Name        Description.
**                  tp          pointer to TCP/IP gateway control block
**                              reasonCode element provides error code value
**
**  Returns:        TRUE if error indication sent successfully
**                  FALSE if error indication should be retried
**
**------------------------------------------------------------------------*/
static bool cdcnetTcpGwSendErrorIndication(TcpGcb *tp)
    {
    NpuBuffer *bp;

    bp = npuBipBufGet();
    if (bp == NULL) return(FALSE);
    memset(bp->data, 0, cdcnetGwEILength + BlkOffGwCmdName);

    npuLogMessage("CDCNet: Send error indication, userCepId=%d, error=%d, CN=%02X",
                  tp->userCepId, tp->reasonCode, tp->cn);
    memcpy(bp->data + BlkOffGwCmdName, "TCPEI  ", 7);
    cdcnetPutU16ToMessage(cdcnetGwEILength, bp->data + BlkOffGwHeaderLen);
    bp->data[BlkOffGwTcpVersion] = cdcnetGwTcpVersion;
    cdcnetPutIdToMessage(tp->userCepId, bp->data + BlkOffGwEIUserCepId);
    bp->numBytes = cdcnetGwEILength + BlkOffGwCmdName;
    cdcnetGwRequestUplineTransfer(tp, bp, BtHTQMSG, cdcnetGwHTIndication, tp->reasonCode);
    tp->reasonCode = 0;
    return(TRUE);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Send an indication that data has been received.
**
**  Parameters:     Name        Description.
**                  tp          pointer to TCP/IP gateway control block
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cdcnetTcpGwSendDataIndication(TcpGcb *tp)
    {
    u8 blockType;
    NpuBuffer *bp;
    int n;
    int recvSize;
    TcpGwStatus status;

    bp = npuBipBufGet();
    if (bp == NULL) return;

    npuLogMessage("CDCNet: Send data indication, userCepId=%d, CN=%02X", tp->userCepId, tp->cn);
    blockType = BtHTMSG;
    status = tcp_successful;

    recvSize = sizeof(bp->data) - (BlkOffDbc + 1);
    if (recvSize > tp->maxUplineBlockSize) recvSize = tp->maxUplineBlockSize;
    n = recv(tp->connFd, bp->data + BlkOffDbc + 1, recvSize, 0);
    if (n > 0)
        {
        #if DEBUG
            npuLogMessage("CDCNet: Received %d bytes, CN=%02X", n, tp->cn);
        #endif
        bp->numBytes = BlkOffDbc + n + 1;
        }
    else if (n == 0)
        {
        npuLogMessage("CDCNet: Stream closed, CN=%02X", tp->cn);
        blockType = BtHTQMSG;
        memset(bp->data + BlkOffDbc + 1, 0, cdcnetGwDILength + BlkOffGwCmdName - (BlkOffDbc + 1));
        tp->tcpState = StTcpDisconnecting;
        memcpy(bp->data + BlkOffGwCmdName, "TCPDI  ", 7);
        cdcnetPutU16ToMessage(cdcnetGwDILength, bp->data + BlkOffGwHeaderLen);
        bp->data[BlkOffGwTcpVersion] = cdcnetGwTcpVersion;
        cdcnetPutIdToMessage(tp->userCepId, bp->data + BlkOffGwDIUserCepId);
        bp->numBytes = cdcnetGwDILength + BlkOffGwCmdName;
        }
    else
        {
        npuLogMessage("CDCNet: Error reading stream: %s, CN=%02X", strerror(errno), tp->cn);
        blockType = BtHTQMSG;
        memset(bp->data + BlkOffDbc + 1, 0, cdcnetGwEILength + BlkOffGwCmdName - (BlkOffDbc + 1));
        memcpy(bp->data + BlkOffGwCmdName, "TCPEI  ", 7);
        cdcnetPutU16ToMessage(cdcnetGwEILength, bp->data + BlkOffGwHeaderLen);
        status = tcp_internal_error;
        bp->data[BlkOffGwTcpVersion] = cdcnetGwTcpVersion;
        cdcnetPutIdToMessage(tp->userCepId, bp->data + BlkOffGwEIUserCepId);
        bp->numBytes = cdcnetGwEILength + BlkOffGwCmdName;
        }
    cdcnetGwRequestUplineTransfer(tp, bp, blockType, cdcnetGwHTIndication, status);
    }

/*---------------------------  End Of File  ------------------------------*/
