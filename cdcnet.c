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

#define DEBUG    0

/*
**  -------------
**  Include Files
**  -------------
*/
#include <stdio.h>
#include <stdlib.h>
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
#include "const.h"
#include "types.h"
#include "proto.h"
#include "npu.h"

#if defined(__APPLE__)
#include <execinfo.h>
#endif

/*
**  -----------------
**  Private Constants
**  -----------------
*/
#define cdcnetInitUpline              0x01
#define cdcnetInitDownline            0x02

/* --- TCP Gateway header types --- */
#define cdcnetTcpHTIndication         0
#define cdcnetTcpHTRequest            0
#define cdcnetTcpHTResponse           1

/* --- Gateway TCP version --- */
#define cdcnetTcpVersion              0x10
#define cdcnetUdpVersion              0x02

/* --- Offsets common to all TCP gateway commands and responses --- */
#define BlkOffTcpCmdName              5
#define BlkOffTcpHeaderType           12
#define BlkOffTcpHeaderLen            13
#define BlkOffTcpDataLen              15
#define BlkOffTcpStatus               17
#define BlkOffTcpTcpVersion           19

/* --- Offsets to fields in TCP Open SAP command --- */
#define BlkOffTcpOSUserSapId          20
#define BlkOffTcpOSTcpIpGwVer         24
#define BlkOffTcpOSTcpSapId           28

/* --- Offsets to fields in TCP Close SAP command --- */
#define BlkOffTcpCSTcpSapId           20

/* --- Offsets to fields in TCP Active Connect command --- */
#define BlkOffTcpACTcpSapId           20
#define BlkOffTcpACUserCepId          28
#define BlkOffTcpACTcpCepId           35
#define BlkOffTcpACSrcAddr            50
#define BlkOffTcpACDstAddr            80
#define BlkOffTcpACAllocation         110
#define BlkOffTcpACIpHeader           125
#define BlkOffTcpACIpOptions          155
#define BlkOffTcpACUlpTimeout         485
#define BlkOffTcpACPushFlag           500
#define BlkOffTcpACUrgentFlag         500

/* --- Offsets to fields in TCP Passive Connect command --- */
#define BlkOffTcpPCTcpSapId           20
#define BlkOffTcpPCUserCepId          28
#define BlkOffTcpPCTcpCepId           35
#define BlkOffTcpPCSrcAddr            50
#define BlkOffTcpPCDstAddr            80
#define BlkOffTcpPCAllocation         110
#define BlkOffTcpPCIpHeader           125
#define BlkOffTcpPCIpOptions          155
#define BlkOffTcpPCUlpTimeout         485
#define BlkOffTcpPCPushFlag           500
#define BlkOffTcpPCUrgentFlag         500

/* --- Offsets to fields in TCP Allocation command --- */
#define BlkOffTcpATcpCepId            20
#define BlkOffTcpASize                28

/* --- Offsets to fields in TCP Status command --- */
#define BlkOffTcpSTcpCepId            20
#define BlkOffTcpSUserCepId           28
#define BlkOffTcpSTcpSapId            35
#define BlkOffTcpSUserSapId           43
#define BlkOffTcpSSrcAddr             50
#define BlkOffTcpSDstAddr             80
#define BlkOffTcpSIpHeader            110
#define BlkOffTcpSIpOptions           140
#define BlkOffTcpSUlpTimeout          470
#define BlkOffTcpSTcpStatRec          485

/* --- Offsets to fields in TCP Disconnect command --- */
#define BlkOffTcpDTcpCepId            20

/* --- Offsets to fields in TCP Abort Current Connection command --- */
#define BlkOffTcpACCTcpCepId          20

/* --- Offsets to fields in Send Data command --- */
#define BlkOffTcpSDTcpCepId           20
#define BlkOffTcpSDPushFlag           24
#define BlkOffTcpSDUrgent             24
#define BlkOffTcpSDUlpTimeout         35

/* --- Offsets to fields in TCP Connection Indication --- */
#define BlkOffTcpCIUserCepId          20
#define BlkOffTcpCISrcAddr            35
#define BlkOffTcpCIDstAddr            65
#define BlkOffTcpCIIpHeader           95
#define BlkOffTcpCIIpOptions          125
#define BlkOffTcpCIUlpTimeout         455
#define cdcnetTcpCILength             (470 - BlkOffTcpCmdName)

/* --- Offsets to fields in TCP Receive indication --- */
#define BlkOffTcpRUserCepId           20
#define BlkOffTcpRUrgent              24
#define cdcnetTcpRLength              (35 - BlkOffTcpCmdName)

/* --- Offsets to fields in TCP Close SAP Indication --- */
#define BlkOffTcpCSIUserSapId         20
#define cdcnetTcpCSILength            (35 - BlkOffTcpCmdName)

/* --- Offsets to fields in TCP Abort Indication --- */
#define BlkOffTcpABIUserCepId         20
#define cdcnetTcpABILength            (35 - BlkOffTcpCmdName)

/* --- Offsets to fields in TCP Disconnect Confirmation --- */
#define BlkOffTcpDCUserCepId          20
#define cdcnetTcpDCLength             (35 - BlkOffTcpCmdName)

/* --- Offsets to fields in TCP Disconnect Indication --- */
#define BlkOffTcpDIUserCepId          20
#define cdcnetTcpDILength             (35 - BlkOffTcpCmdName)

/* --- Offsets to fields in TCP Error Indication --- */
#define BlkOffTcpEIUserCepId          20
#define cdcnetTcpEILength             (35 - BlkOffTcpCmdName)

/* --- IP address fields in use indicators --- */
#define cdcnetIpAddrFieldsNone        0
#define cdcnetIpAddrFieldsNetwork     1
#define cdcnetIpAddrFieldsHost        2
#define cdcnetIpAddrFieldsBoth        3

/* --- Relative offsets in TCP IP address structures --- */
#define RelOffTcpIpAddress            0
#define RelOffTcpIpAddrFieldsInUse    0
#define RelOffTcpIpAddressNetwork     1
#define RelOffTcpIpAddressHost        4
#define cdcnetTcpIpAddressLength      7

/* --- Relative offsets in TCP IP address structures --- */
#define RelOffTcpPortInUse            15
#define RelOffTcpPort                 16
#define cdcnetTcpAddressLength        30

/* --- Relative offsets in ULP timeout structure --- */
#define RelOffGwUlpTimeout            0
#define RelOffGwUlpTimeoutAbort       4

/* --- Offsets to attributes in NAM A-A connection request --- */
#define BlkOffDwnBlkLimit             12
#define BlkOffDwnBlkSize              13
#define BlkOffUplBlkLimit             16
#define BlkOffUplBlkSize              17
#define BlkOffAppName                 29

/* --- Reason codes for NAM A-A connection failure --- */
#define cdcnetErrAppMaxConns          20
#define cdcnetErrAppNotAvail          22

/* --- UDP gateway request primitives --- */
#define cdcnetUdpCallRequest          0x10
#define cdcnetUdpDataRequest          0x11
#define cdcnetUdpDataRequestDest      0x12
#define cdcnetUdpDataIndication       0x13
#define cdcnetUdpCallResponse         0x14

/* --- UDP gateway error codes --- */
#define cdcnetUdpHostUnreachable      0x3f
#define cdcnetUdpNetUnreachable       0x4f
#define cdcnetUdpProtoUnreachable     0x5f
#define cdcnetUdpPortUnreachable      0x6f
#define cdcnetUdpRouteFailed          0x7f
#define cdcnetUdpPortInUse            0x8f
#define cdcnetUdpNoResources          0x9f
#define cdcnetUdpSapNotOpen           0xcf
#define cdcnetUdpSapAlreadyOpen       0xdf

/* --- Relative offsets in UDP address structures --- */
#define RelOffUdpIpAddress            0
#define RelOffUdpIpAddrFieldsInUse    0
#define RelOffUdpIpAddressNetwork     1
#define RelOffUdpIpAddressHost        5
#define RelOffUdpPortInUse            9
#define RelOffUdpPort                 10
#define cdcnetUdpAddressLength        13

/* --- UDP gateway common header offsets --- */
#define BlkOffUdpHeader               5
#define BlkOffUdpRequestType          5
#define BlkOffUdpVersion              6

/* --- UDP gateway Open SAP request offsets --- */
#define BlkOffUdpOpenSapUnused        7
#define BlkOffUdpOpenSapSrcAddr       8
#define BlkOffUdpOpenSapDstAddr       21

/* --- UDP gateway Data Indication offsets --- */
#define BlkOffUdpDataIndUnused        7
#define BlkOffUdpDataIndSrcAddr       8
#define BlkOffUdpDataIndData          21

/* --- UDP gateway Data Request offsets --- */
#define BlkOffUdpDataReqDstAddr       10
#define BlkOffUdpDataReqData          23

/*
**  -----------------------
**  Private Macro Functions
**  -----------------------
*/
#if DEBUG
#define HexColumn(x)      (3 * (x) + 4)
#define AsciiColumn(x)    (HexColumn(16) + 2 + (x))
#define LogLineLength    (AsciiColumn(16))
#endif

/*
**  -----------------------------------------
**  Private Typedef and Structure Definitions
**  -----------------------------------------
*/
typedef enum tcpGwStatus      // from CDCNet source file
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

typedef enum gwConnState
    {
    StGwIdle,
    StGwStartingInit,
    StGwInitializing,
    StGwConnected,
    StGwInitiateTermination,
    StGwTerminating,
    StGwAwaitTermBlock,
    StGwError
    } GwConnState;

typedef enum tcpUdpConnState
    {
    StTcpUdpIdle,
    StTcpConnecting,
    StTcpIndicatingConnection,
    StTcpListening,
    StTcpConnected,
    StTcpDisconnecting,
    StUdpBound
    } TcpUdpConnState;

typedef enum gwConnType
    {
    TypeTcpActive,
    TypeTcpPassive,
    TypeUdp
    } GwConnType;

typedef struct gcb // Gateway Control Block
    {
    u16             ordinal;
    GwConnState     gwState;
    TcpUdpConnState tcpUdpState;
    GwConnType      connType;
    u8              initStatus;
    u8              cn;
    u8              bsn;
    u8              unackedBlocks;
    u16             maxUplineBlockSize;
    u32             tcpSapId;
    u32             tcpCepId;
    u32             userSapId;
    u32             userCepId;
    u8              reasonCode;
    u8              tcpSrcAddress[cdcnetTcpAddressLength];
    u8              tcpDstAddress[cdcnetTcpAddressLength];
    char            srcIpAddress[20];
    u16             srcPort;
    char            dstIpAddress[20];
    u16             dstPort;
#if defined(_WIN32)
    SOCKET          connFd;
#else
    int             connFd;
#endif
    u32             localAddr;
    u16             localPort;
    u32             peerAddr;
    u16             peerPort;
    time_t          deadline;
    NpuQueue        downlineQueue;
    NpuQueue        outputQueue;
    } Gcb;

typedef struct pccb // Passive Connection Control Block
    {
    u16    ordinal;
    u16    tcpGcbOrdinal;
    u16    srcPort;
    u16    dstPort;
#if defined(_WIN32)
    SOCKET connFd;
#else
    int    connFd;
#endif
    time_t deadline;
    } Pccb;

typedef struct tcpGwCommand
    {
    char *command;
    bool (*handler)(Gcb *gp, NpuBuffer *bp);
    } TcpGwCommand;

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static Pccb *cdcnetAddPccb();
static Gcb *cdcnetAddGcb();
static void cdcnetCloseConnection(Gcb *gp);

#if defined(_WIN32)
static SOCKET cdcnetCreateTcpSocket();

#else
static int cdcnetCreateTcpSocket();

#endif
static Pccb *cdcnetFindPccb(u16 port);
static Gcb *cdcnetFindGcb(u8 cn);
static u32 cdcnetGetIdFromMessage(u8 *ip);
static Pccb *cdcnetGetPccb();

#if defined(_WIN32)
static bool cdcnetGetEndpoints(SOCKET sd, u32 *localAddr, u16 *localPort,
                               u32 *peerAddr, u16 *peerPort);

#else
static bool cdcnetGetEndpoints(int sd, u32 *localAddr, u16 *localPort,
                               u32 *peerAddr, u16 *peerPort);

#endif
static Gcb *cdcnetGetGcb();
static void cdcnetPutIdToMessage(u32 id, u8 *mp);
static void cdcnetPutU32ToMessage(u32 value, u8 *mp);
static void cdcnetSendBack(Gcb *gp, NpuBuffer *bp, u8 bsn);
static bool cdcnetSendInitializeConnectionRequest(Gcb *gp);
static void cdcnetSendInitializeConnectionResponse(NpuBuffer *bp, Gcb *gp);
static void cdcnetSendInitiateConnectionResponse(NpuBuffer *bp, u8 cn, u8 rc);
static void cdcnetSendTerminateConnectionBlock(NpuBuffer *bp, u8 cn);
static void cdcnetSendTerminateConnectionRequest(NpuBuffer *bp, u8 cn);
static void cdcnetSendTerminateConnectionResponse(NpuBuffer *bp, u8 cn);
static bool cdcnetTcpAbortCurrentConnectionHandler(Gcb *gp, NpuBuffer *bp);
static bool cdcnetTcpActiveConnectHandler(Gcb *gp, NpuBuffer *bp);
static bool cdcnetTcpAllocateHandler(Gcb *gp, NpuBuffer *bp);
static bool cdcnetTcpCloseSAPHandler(Gcb *gp, NpuBuffer *bp);
static bool cdcnetTcpDisconnectHandler(Gcb *gp, NpuBuffer *bp);
static u32 cdcnetTcpGetIpAddress(u8 *ap);
static u16 cdcnetTcpGetPort(u8 *ap);
static bool cdcnetTcpOpenSAPHandler(Gcb *gp, NpuBuffer *bp);
static bool cdcnetTcpPassiveConnectHandler(Gcb *gp, NpuBuffer *bp);
static void cdcnetTcpRequestUplineTransfer(Gcb *gp, NpuBuffer *bp, u8 blockType, u8 headerType, TcpGwStatus status);
static bool cdcnetTcpSendConnectionIndication(Gcb *gp);
static void cdcnetTcpSendDataIndication(Gcb *gp);
static bool cdcnetTcpSendErrorIndication(Gcb *gp);
static void cdcnetTcpSetIpAddress(u8 *ap, u32 ipAddr);
static void cdcnetTcpSetPort(u8 *ap, u16 port);
static bool cdcnetUdpBindAddress(Gcb *gp, NpuBuffer *bp);
static u32 cdcnetUdpGetIpAddress(u8 *ap);
static u16 cdcnetUdpGetPort(u8 *ap);
static bool cdcnetUdpSendDownlineData(Gcb *gp, NpuBuffer *bp);
static void cdcnetUdpSendUplineData(Gcb *gp);
static void cdcnetUdpSetAddress(u8 *dp, u32 ipAddress, u16 port);

#if DEBUG
static void cdcnetLogBytes(u8 *bytes, int len);
static void cdcnetLogFlush(void);
static void cdcnetPrintStackTrace(FILE *fp);

#endif

/*
**  ----------------
**  Public Variables
**  ----------------
*/
u8  cdcnetNode = 255;
u16 cdcnetPrivilegedTcpPortOffset = 6600;
u16 cdcnetPrivilegedUdpPortOffset = 6600;

/*
**  -----------------
**  Private Variables
**  -----------------
*/
static u16  cdcnetPassivePort = 7600;
static Pccb *cdcnetPccbs      = (Pccb *)NULL;
static u8   cdcnetPccbCount   = 0;
static Gcb  *cdcnetGcbs       = (Gcb *)NULL;
static u8   cdcnetGcbCount    = 0;

static TcpGwCommand tcpGwCommands [] =
    {
        { "TCPA   ", cdcnetTcpAllocateHandler               },
        { "TCPAC  ", cdcnetTcpActiveConnectHandler          },
        { "TCPACC ", cdcnetTcpAbortCurrentConnectionHandler },
        { "TCPCS  ", cdcnetTcpCloseSAPHandler               },
        { "TCPD   ", cdcnetTcpDisconnectHandler             },
        { "TCPOS  ", cdcnetTcpOpenSAPHandler                },
        { "TCPPC  ", cdcnetTcpPassiveConnectHandler         },

/*
 *      {"TCPS   ", cdcnetTcpStatusHandler},
 *      {"TCPSD  ", cdcnetTcpSendDataHandler},
 *      {"TCPCSI ", cdcnetTcpCloseSAPIndicationHandler},
 *      {"TCPABI ", cdcnetTcpAbortIndicationHandler},
 *      {"TCPDC  ", cdcnetTcpDisconnectConfirmationHandler},
 *      {"TCPDI  ", cdcnetTcpDisconnectIndicationHandler},
 */
        { NULL,      NULL                                   }
    };

#if DEBUG
static FILE *cdcnetLog = NULL;
static char cdcnetLogBuf[LogLineLength + 1];
static int  cdcnetLogBytesCol = 0;
#endif

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
    int  i;
    Pccb *pp;
    Gcb  *gp;

    if (cdcnetGcbCount < 1)
        {
        return;
        }

#if DEBUG
    fputs("Reset gateway\n", cdcnetLog);
#endif

    /*
    **  Iterate through all Gcbs.
    */
    for (i = 0, gp = cdcnetGcbs; i < cdcnetGcbCount; i++, gp++)
        {
        if (gp->gwState != StGwIdle)
            {
            cdcnetCloseConnection(gp);
            }
        }

    /*
    **  Iterate through all Pccbs.
    */
    for (i = 0, pp = cdcnetPccbs; i < cdcnetPccbCount; i++, pp++)
        {
        if (pp->connFd != 0)
            {
            netCloseConnection(pp->connFd);
            pp->connFd        = 0;
            pp->dstPort       = 0;
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
    u8   blockType;
    u8   cn;
    int  nameLen;
    u8   pfc;
    u8   rc;
    u8   sfc;
    Gcb  *gp;

    if (bp == NULL)
        {
        return;
        }

#if DEBUG
    if (cdcnetLog == NULL)
        {
        cdcnetLog = fopen("cdcnetlog.txt", "wt");
        if (cdcnetLog == NULL)
            {
            logDtError(LogErrorLocation, "Cannot Open cdcnetlog.txt - aborting\n");
            exit(1);
            }
        cdcnetLogFlush();    // initialize log buffer
        }
    fprintf(cdcnetLog, "Process downline block (%d bytes)\n", bp->numBytes);
    cdcnetLogBytes(bp->data, bp->numBytes);
    cdcnetLogFlush();
#endif

    blockType = bp->data[BlkOffBTBSN] & BlkMaskBT;
    cn        = bp->data[BlkOffCN];

    switch (blockType)
        {
    case BtHTBLK:
    case BtHTMSG:
        gp = cdcnetFindGcb(cn);
        if (gp != NULL)
            {
            bp->blockSeqNo = (bp->data[BlkOffBTBSN] >> BlkShiftBSN) & BlkMaskBSN;
            npuBipQueueAppend(bp, &gp->downlineQueue);
            }
        else
            {
#if DEBUG
            fprintf(cdcnetLog, "Connection not found: %02X\n", cn);
#endif
            npuBipBufRelease(bp);
            }
        break;

    case BtHTBACK:
        gp = cdcnetFindGcb(cn);
        if ((gp != NULL) && (gp->unackedBlocks > 0))
            {
            gp->unackedBlocks--;
            }
        npuBipBufRelease(bp);
        break;

    case BtHTCMD:
        pfc = bp->data[BlkOffPfc];
        sfc = bp->data[BlkOffSfc];

        switch (pfc)
            {
        case 0x02:           // initiate connection
            if (sfc == 0x09) // A-A connection
                {
                rc      = 0;
                cn      = bp->data[BlkOffP3];
                appName = (char *)bp->data + BlkOffAppName;
                nameLen = bp->numBytes - BlkOffAppName;
                gp      = cdcnetGetGcb();

                if ((nameLen < 9) || (strncmp(appName, "GW_TCPIP_", 9) != 0))
                    {
#if DEBUG
                    fprintf(cdcnetLog, "Application name '%.9s' does not match GW_TCPIP_ ...\n", appName);
#endif
                    rc = cdcnetErrAppNotAvail;
                    }
                else if (gp == NULL)
                    {
#if DEBUG
                    fprintf(cdcnetLog, "Unable to allocate TCP/IP control block for %*s (CN=%02X)\n",
                            nameLen, appName, cn);
#endif
                    rc = cdcnetErrAppMaxConns;
                    }
                else
                    {
                    gp->cn                 = cn;
                    gp->unackedBlocks      = 0;
                    gp->maxUplineBlockSize = *(bp->data + BlkOffUplBlkSize) * 100;
                    gp->gwState            = StGwStartingInit;
                    gp->initStatus         = 0;
                    }

                cdcnetSendInitiateConnectionResponse(bp, cn, rc);
                }
            else
                {
#if DEBUG
                fprintf(cdcnetLog, "Received unexpected command type: %02X/%02X\n", pfc, sfc);
#endif
                npuBipBufRelease(bp);
                }
            break;

        case 0x03:                                          // terminate connection
            if ((sfc == 0x08) || (sfc == (0x08 | SfcResp))) // terminate connection request or response
                {
                cn = bp->data[BlkOffP3];
                gp = cdcnetFindGcb(cn);
                if (gp != NULL)
                    {
                    if (sfc == 0x08) // terminate connection request
                        {
                        cdcnetSendTerminateConnectionBlock(bp, cn);
                        gp->gwState = StGwTerminating;
                        }
                    else            // terminate connection response
                        {
                        cdcnetCloseConnection(gp);
                        npuBipBufRelease(bp);
                        }
                    }
                else
                    {
#if DEBUG
                    fprintf(cdcnetLog, "Connection not found: %02X\n", cn);
#endif
                    npuBipBufRelease(bp);
                    }
                }
            else
                {
#if DEBUG
                fprintf(cdcnetLog, "Received unexpected command type: %02X/%02X\n", pfc, sfc);
#endif
                npuBipBufRelease(bp);
                }
            break;

        default:
#if DEBUG
            fprintf(cdcnetLog, "Received unexpected command type: %02X/%02X\n", pfc, sfc);
#endif
            npuBipBufRelease(bp);
            break;
            }
        break;

    case BtHTQBLK:
    case BtHTQMSG:
        gp = cdcnetFindGcb(cn);
        if (gp != NULL)
            {
            bp->blockSeqNo = (bp->data[BlkOffBTBSN] >> BlkShiftBSN) & BlkMaskBSN;
            npuBipQueueAppend(bp, &gp->downlineQueue);
            }
        else
            {
#if DEBUG
            fprintf(cdcnetLog, "Connection not found: %02X\n", cn);
#endif
            npuBipBufRelease(bp);
            }
        break;

    case BtHTRINIT:
        gp = cdcnetFindGcb(cn);
        if (gp != NULL)
            {
            cdcnetSendInitializeConnectionResponse(bp, gp);
            gp->initStatus |= cdcnetInitDownline;
            if (gp->initStatus == (cdcnetInitDownline | cdcnetInitUpline))
                {
                gp->gwState = StGwConnected;
#if DEBUG
                fprintf(cdcnetLog, "Gateway connection established, CN=%02X\n", cn);
#endif
                }
            }
        else
            {
#if DEBUG
            fprintf(cdcnetLog, "Connection not found: %02X\n", cn);
#endif
            npuBipBufRelease(bp);
            }
        break;

    case BtHTNINIT:
        gp = cdcnetFindGcb(cn);
        if (gp != NULL)
            {
            gp->initStatus |= cdcnetInitUpline;
            if (gp->initStatus == (cdcnetInitDownline | cdcnetInitUpline))
                {
                gp->gwState = StGwConnected;
#if DEBUG
                fprintf(cdcnetLog, "Gateway connection established, CN=%02X\n", cn);
#endif
                }
            }
#if DEBUG
        else
            {
            fprintf(cdcnetLog, "Connection not found: %02X\n", cn);
            }
#endif
        npuBipBufRelease(bp);
        break;

    case BtHTTERM:
        gp = cdcnetFindGcb(cn);
        if (gp != NULL)
            {
            if (gp->gwState == StGwAwaitTermBlock)
                {
                cdcnetSendTerminateConnectionBlock(bp, cn); // echo TERM block
                gp->gwState = StGwIdle;
                }
            else if (gp->gwState == StGwTerminating)
                {
                cdcnetSendTerminateConnectionResponse(bp, cn);
                cdcnetCloseConnection(gp);
                }
            else
                {
#if DEBUG
                fprintf(cdcnetLog, "Invalid state %d on reception of TERM block: CN=%02X\n", gp->gwState, cn);
#endif
                npuBipBufRelease(bp);
                }
            }
        else
            {
#if DEBUG
            fprintf(cdcnetLog, "Connection not found: %02X\n", cn);
#endif
            npuBipBufRelease(bp);
            }
        break;

    default:
#if DEBUG
        fprintf(cdcnetLog, "Received unexpected block type: %02X, CN=%02X\n", blockType, cn);
#endif
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
    u8                 b;
    u8                 blockType;
    NpuBuffer          *bp;
    NpuBuffer          *bp2;
    time_t             currentTime;
    struct sockaddr_in from;
    TcpGwCommand       *gcp;
    int                i;
    u32                localAddr;
    u16                localPort;
    int                n;
    int                optEnable = 1;
    u32                peerAddr;
    u16                peerPort;
    Pccb               *pp;
    fd_set             readFds;
    int                rc;
    int                readySockets;
    struct timeval     timeout;
    Gcb                *gp;
    fd_set             writeFds;

#if defined(_WIN32)
    SOCKET fd;
    u_long blockEnable = 1;
    int    fromLen;
    SOCKET maxFd;
#else
    int       fd;
    socklen_t fromLen;
    int       maxFd;
#endif

    FD_ZERO(&readFds);
    FD_ZERO(&writeFds);
    currentTime = getSeconds();
    maxFd       = 0;

    for (i = 0, pp = cdcnetPccbs; i < cdcnetPccbCount; i++, pp++)
        {
        if (pp->connFd > 0)
            {
            if (pp->tcpGcbOrdinal != 0)
                {
                FD_SET(pp->connFd, &readFds);
                if (pp->connFd > maxFd)
                    {
                    maxFd = pp->connFd;
                    }
                }
            else if (currentTime >= pp->deadline)
                {
#if DEBUG
                fprintf(cdcnetLog, "Unassigned listen port timeout, port=%d\n", pp->dstPort);
#endif
                netCloseConnection(pp->connFd);
                pp->dstPort = 0;
                pp->connFd  = 0;
                }
            }
        }

    for (i = 0, gp = cdcnetGcbs; i < cdcnetGcbCount; i++, gp++)
        {
        switch (gp->gwState)
            {
        case StGwStartingInit:
            if (cdcnetSendInitializeConnectionRequest(gp))
                {
                gp->gwState  = StGwInitializing;
                gp->deadline = currentTime + 10;
                }
            break;

        case StGwInitializing:
            if (gp->deadline < currentTime)
                {
#if DEBUG
                fprintf(cdcnetLog, "Connection initialization timed out, CN=%02X\n", gp->cn);
#endif
                cdcnetCloseConnection(gp);
                }
            break;

        case StGwInitiateTermination:
            bp = npuBipBufGet();
            if (bp != NULL)
                {
                cdcnetCloseConnection(gp);
                cdcnetSendTerminateConnectionRequest(bp, gp->cn);
                gp->gwState = StGwAwaitTermBlock;
                }
            break;

        case StGwConnected:

            bp = npuBipQueueExtract(&gp->downlineQueue);

            if (bp != NULL)
                {
                blockType = bp->data[BlkOffBTBSN] & BlkMaskBT;

                switch (blockType)
                    {
                case BtHTBLK:
                case BtHTMSG:
                    bp->offset = BlkOffDbc + 1;
                    npuBipQueueAppend(bp, &gp->outputQueue);
                    break;

                case BtHTQBLK:
                case BtHTQMSG:
                    if (bp->blockSeqNo)
                        {
                        bp2 = npuBipBufGet();
                        if (bp2 == NULL)
                            {
                            npuBipQueuePrepend(bp, &gp->downlineQueue);
                            break;
                            }
                        cdcnetSendBack(gp, bp2, bp->blockSeqNo);
                        bp->blockSeqNo = 0;
                        }
                    for (gcp = tcpGwCommands; gcp->command; gcp++)
                        {
                        if (strncmp(gcp->command, (char *)bp->data + BlkOffTcpCmdName, 7) == 0)
                            {
                            if (!(*gcp->handler)(gp, bp))
                                {
                                npuBipQueuePrepend(bp, &gp->downlineQueue);
                                }
                            break;
                            }
                        }
                    if (!gcp->command)
                        {
                        /*
                        **  If the message does not have a recognized TCP gateway command,
                        **  check for a UDP gateway primitive.
                        */
                        b = *(bp->data + BlkOffUdpRequestType);
                        if ((b >= cdcnetUdpCallRequest) && (b <= cdcnetUdpDataRequestDest)
                            && (*(bp->data + BlkOffUdpVersion) == cdcnetUdpVersion))
                            {
                            if (b == cdcnetUdpCallRequest)
                                {
                                if (!cdcnetUdpBindAddress(gp, bp))
                                    {
                                    npuBipQueuePrepend(bp, &gp->downlineQueue);
                                    }
                                }
                            else if (!cdcnetUdpSendDownlineData(gp, bp))
                                {
                                npuBipQueuePrepend(bp, &gp->downlineQueue);
                                }
                            }
                        else
                            {
#if DEBUG
                            fprintf(cdcnetLog, "Unrecognized TCP gateway command: %7s\n", (char *)bp->data + BlkOffTcpCmdName);
#endif
                            npuBipBufRelease(bp);
                            }
                        }
                    break;

                default:
#if DEBUG
                    fprintf(cdcnetLog, "Unexpected block type in cdcnetCheckStatus: %d\n", blockType);
#endif
                    npuBipBufRelease(bp);
                    break;
                    }
                }

            switch (gp->tcpUdpState)
                {
            case StTcpConnecting:
                FD_SET(gp->connFd, &writeFds);
                if (gp->connFd > maxFd)
                    {
                    maxFd = gp->connFd;
                    }
                break;

            case StTcpIndicatingConnection:
                if (cdcnetTcpSendConnectionIndication(gp))
                    {
                    gp->tcpUdpState = StTcpConnected;
                    }
                break;

            case StTcpConnected:
            case StUdpBound:
                if (gp->unackedBlocks < 7)
                    {
                    FD_SET(gp->connFd, &readFds);
                    if (gp->connFd > maxFd)
                        {
                        maxFd = gp->connFd;
                        }
                    }
                if (gp->outputQueue.first != NULL)
                    {
                    FD_SET(gp->connFd, &writeFds);
                    if (gp->connFd > maxFd)
                        {
                        maxFd = gp->connFd;
                        }
                    }
                break;

            default: // do nothing
                break;
                }

            break;

        case StGwError:
            if (cdcnetTcpSendErrorIndication(gp))
                {
                gp->gwState = StGwConnected;
                }
            break;
            }
        }

    if (maxFd < 1)
        {
        return;
        }

    timeout.tv_sec  = 0;
    timeout.tv_usec = 0;
    readySockets    = select((int)(maxFd + 1), &readFds, &writeFds, NULL, &timeout);

    if (readySockets < 1)
        {
        return;
        }

    for (i = 0, pp = cdcnetPccbs; i < cdcnetPccbCount; i++, pp++)
        {
        if ((pp->tcpGcbOrdinal != 0) && (pp->connFd > 0) && FD_ISSET(pp->connFd, &readFds))
            {
            fromLen = sizeof(from);
            fd      = accept(pp->connFd, (struct sockaddr *)&from, &fromLen);

            if (fd > 0)
                {
                if (!cdcnetGetEndpoints(fd, &localAddr, &localPort, &peerAddr, &peerPort))
                    {
                    netCloseConnection(fd);
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
                gp = cdcnetGcbs + (pp->tcpGcbOrdinal - 1);
                if (pp->srcPort != 0)
                    {
#if DEBUG
                    fprintf(cdcnetLog, "Close listening socket, port=%d, CN=%02X\n", pp->dstPort, gp->cn);
#endif
                    netCloseConnection(pp->connFd);
                    pp->dstPort = 0;
                    pp->srcPort = 0;
                    pp->connFd  = 0;
                    }
                else
                    {
#if DEBUG
                    fprintf(cdcnetLog, "Leave listening socket open, port=%d, CN=%02X\n", pp->dstPort, gp->cn);
#endif
                    pp->deadline = currentTime + 10;
                    }
                pp->tcpGcbOrdinal = 0;
                gp->connFd        = fd;
                gp->localAddr     = localAddr;
                gp->localPort     = localPort;
                gp->peerAddr      = peerAddr;
                gp->peerPort      = peerPort;
                sprintf(gp->srcIpAddress, "%d.%d.%d.%d",
                        (peerAddr >> 24) & 0xff,
                        (peerAddr >> 16) & 0xff,
                        (peerAddr >> 8) & 0xff,
                        peerAddr & 0xff);
                gp->srcPort = peerPort;
#if DEBUG
                fprintf(cdcnetLog, "Accepted connection, source addr=%s:%u, dest addr=%s:%u, CN=%02X\n",
                        gp->srcIpAddress, gp->srcPort, gp->dstIpAddress, gp->dstPort, gp->cn);
#endif
                if (cdcnetTcpSendConnectionIndication(gp))
                    {
                    gp->tcpUdpState = StTcpConnected;
                    }
                else
                    {
                    gp->tcpUdpState = StTcpIndicatingConnection;
                    }
                }
#if DEBUG
            else
                {
                fprintf(cdcnetLog, "Accept of new connection failed on port %d, CN=%02X\n",
                        gp->dstPort, gp->cn);
                }
#endif
            }
        }

    for (i = 0, gp = cdcnetGcbs; i < cdcnetGcbCount; i++, gp++)
        {
        if (gp->gwState != StGwConnected)
            {
            continue;
            }

        switch (gp->tcpUdpState)
            {
        case StTcpConnecting:
            if (FD_ISSET(gp->connFd, &writeFds))
                {
                rc = netGetErrorStatus(gp->connFd);
                if (rc != 0) // connection failed
                    {
#if DEBUG
                    fprintf(cdcnetLog, "Failed to connect to %s:%u, CN=%02X\n",
                            gp->dstIpAddress, gp->dstPort, gp->cn);
#endif
                    netCloseConnection(gp->connFd);
                    gp->connFd      = 0;
                    gp->tcpUdpState = StTcpUdpIdle;
                    gp->reasonCode  = tcp_host_unreachable;
                    gp->gwState     = StGwError;
                    }
                else if (cdcnetGetEndpoints(gp->connFd, &localAddr, &localPort, &peerAddr, &peerPort))
                    {
#if DEBUG
                    fprintf(cdcnetLog, "Connected to %s:%u, CN=%02X\n", gp->dstIpAddress, gp->dstPort, gp->cn);
#endif
                    gp->localAddr = localAddr;
                    gp->localPort = localPort;
                    gp->peerAddr  = peerAddr;
                    gp->peerPort  = peerPort;
                    if (cdcnetTcpSendConnectionIndication(gp))
                        {
                        gp->tcpUdpState = StTcpConnected;
                        }
                    else
                        {
                        gp->tcpUdpState = StTcpIndicatingConnection;
                        }
                    }
                else
                    {
                    netCloseConnection(gp->connFd);
                    gp->connFd      = 0;
                    gp->tcpUdpState = StTcpUdpIdle;
                    gp->reasonCode  = tcp_host_unreachable;
                    gp->gwState     = StGwError;
                    }
                }
            else if (currentTime >= gp->deadline)
                {
#if DEBUG
                fprintf(cdcnetLog, "Connect to %s:%u timed out, CN=%02X\n", gp->dstIpAddress, gp->dstPort, gp->cn);
#endif
                netCloseConnection(gp->connFd);
                gp->connFd      = 0;
                gp->tcpUdpState = StTcpUdpIdle;
                gp->reasonCode  = tcp_host_unreachable;
                gp->gwState     = StGwError;
                }
            break;

        case StTcpConnected:
            if (FD_ISSET(gp->connFd, &readFds))
                {
                cdcnetTcpSendDataIndication(gp);
                }
            if (FD_ISSET(gp->connFd, &writeFds))
                {
                bp = npuBipQueueExtract(&gp->outputQueue);

                if (bp != NULL)
                    {
                    n = send(gp->connFd, bp->data + bp->offset, bp->numBytes - bp->offset, 0);
                    if (n >= 0)
                        {
#if DEBUG
                        fprintf(cdcnetLog, "Sent %d bytes to %s:%u, CN=%02X\n", n, gp->dstIpAddress, gp->dstPort, gp->cn);
#endif
                        bp->offset += n;
                        if (bp->offset >= bp->numBytes)
                            {
                            if (bp->blockSeqNo)
                                {
                                cdcnetSendBack(gp, bp, bp->blockSeqNo);
                                }
                            else
                                {
                                npuBipBufRelease(bp);
                                }
                            }
                        else
                            {
                            npuBipQueuePrepend(bp, &gp->outputQueue);
                            }
                        }
                    else
                        {
#if DEBUG
                        fprintf(cdcnetLog, "Failed to write to %s:%u, %s, CN=%02X\n",
                                gp->dstIpAddress, gp->dstPort, strerror(errno), gp->cn);
#endif
                        npuBipBufRelease(bp);
                        while ((bp = npuBipQueueExtract(&gp->outputQueue)) != NULL)
                            {
                            npuBipBufRelease(bp);
                            }
                        gp->reasonCode = tcp_remote_abort;
                        gp->gwState    = StGwError;
                        }
                    }
                }
            break;

        case StUdpBound:
            if (FD_ISSET(gp->connFd, &readFds))
                {
                cdcnetUdpSendUplineData(gp);
                }
            break;

        default: // do nothing
            break;
            }
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Display CDCNet Gateway data communication status (operator interface).
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void cdcnetShowStatus(void)
    {
    u8      channelNo;
    char    chEqStr[8];
    DevSlot *dp;
    int     i;
    Pccb    *pp;
    Gcb     *gp;
    char    outBuf[200];
    char    *state;

    if (cdcnetGcbCount < 1)
        {
        return;
        }
    dp = NULL;
    for (channelNo = 0; channelNo < MaxChannels; channelNo++)
        {
        dp = channelFindDevice(channelNo, DtMdi);
        if (dp != NULL)
            {
            break;
            }
        }
    if (dp == NULL)
        {
        return;
        }

    sprintf(chEqStr, "C%02o E%02o", dp->channel->id, dp->eqNo);

    /*
    **  Iterate through all Pccbs.
    */
    for (i = 0, pp = cdcnetPccbs; i < cdcnetPccbCount; i++, pp++)
        {
        if (pp->connFd != 0)
            {
            sprintf(outBuf, "    >   %-8s %-7s     "FMTNETSTATUS "\n", "CDCNet", chEqStr, netGetLocalTcpAddress(pp->connFd), "",
                    "tcp", "listening");
            opDisplay(outBuf);
            chEqStr[0] = '\0';
            }
        }

    /*
    **  Iterate through all Gcbs.
    */
    for (i = 0, gp = cdcnetGcbs; i < cdcnetGcbCount; i++, gp++)
        {
        if ((gp->gwState == StGwIdle) || (gp->connType == TypeTcpPassive))
            {
            continue;
            }
        switch (gp->tcpUdpState)
            {
        case StTcpUdpIdle:
            continue;

        case StTcpConnecting:
            state = "connecting";
            break;

        case StTcpIndicatingConnection:
            state = "indicating connection";
            break;

        case StTcpListening:
            state = "listening";
            break;

        case StTcpConnected:
            state = "connected";
            break;

        case StTcpDisconnecting:
            state = "disconnecting";
            break;

        case StUdpBound:
            state = "bound";
            break;

        default:
            state = "unknown";
            break;
            }
        sprintf(outBuf, "    >   %-8s %-7s     "FMTNETSTATUS "\n", "CDCNet", chEqStr, netGetLocalTcpAddress(gp->connFd),
                netGetPeerTcpAddress(gp->connFd), gp->connType == TypeUdp ? "udp" : "tcp", state);
        opDisplay(outBuf);
        chEqStr[0] = '\0';
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
**  Purpose:        Add a gateway control block.
**
**  Parameters:     Name        Description.
**
**  Returns:        Pointer to new Gcb
**                  NULL if insufficient resources
**
**------------------------------------------------------------------------*/
static Gcb *cdcnetAddGcb()
    {
    Gcb *gp;

    gp = (Gcb *)realloc(cdcnetGcbs, (cdcnetGcbCount + 1) * sizeof(Gcb));
    if (gp == (Gcb *)NULL)
        {
        return ((Gcb *)NULL);
        }

    cdcnetGcbs              = gp;
    gp                      = gp + cdcnetGcbCount++;
    gp->ordinal             = cdcnetGcbCount;
    gp->gwState             = StGwIdle;
    gp->tcpUdpState         = StTcpUdpIdle;
    gp->tcpSapId            = 0;
    gp->connFd              = 0;
    gp->deadline            = (time_t)0;
    gp->cn                  = 0;
    gp->bsn                 = 1;
    gp->unackedBlocks       = 0;
    gp->maxUplineBlockSize  = 1000;
    gp->downlineQueue.first = NULL;
    gp->downlineQueue.last  = NULL;
    gp->outputQueue.first   = NULL;
    gp->outputQueue.last    = NULL;

    return (gp);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Get a free gateway control block.
**
**  Parameters:     Name        Description.
**
**  Returns:        Pointer to free Gcb
**                  NULL if insufficient resources
**
**------------------------------------------------------------------------*/
static Gcb *cdcnetGetGcb()
    {
    int i;
    Gcb *gp;

    for (i = 0, gp = cdcnetGcbs; i < cdcnetGcbCount; i++, gp++)
        {
        if (gp->gwState == StGwIdle)
            {
            return (gp);
            }
        }

    return cdcnetAddGcb();
    }

/*--------------------------------------------------------------------------
**  Purpose:        Find a gateway control block by connection number.
**
**  Parameters:     Name        Description.
**                  cn          connection number
**
**  Returns:        Pointer to Gcb
**                  NULL if control block not found
**
**------------------------------------------------------------------------*/
static Gcb *cdcnetFindGcb(u8 cn)
    {
    int i;
    Gcb *gp;

    for (i = 0, gp = cdcnetGcbs; i < cdcnetGcbCount; i++, gp++)
        {
        if ((gp->gwState != StGwIdle) && (cn == gp->cn))
            {
            return (gp);
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
        return ((Pccb *)NULL);
        }

    cdcnetPccbs       = pp;
    pp                = pp + cdcnetPccbCount++;
    pp->ordinal       = cdcnetPccbCount;
    pp->connFd        = 0;
    pp->srcPort       = 0;
    pp->dstPort       = 0;
    pp->tcpGcbOrdinal = 0;
    pp->deadline      = (time_t)0;

    return (pp);
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
    int  i;
    Pccb *pp;

    for (i = 0, pp = cdcnetPccbs; i < cdcnetPccbCount; i++, pp++)
        {
        if (pp->dstPort == 0)
            {
            return (pp);
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
    int  i;
    Pccb *pp;

    for (i = 0, pp = cdcnetPccbs; i < cdcnetPccbCount; i++, pp++)
        {
        if (port == pp->dstPort)
            {
            return (pp);
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
                               u32 *peerAddr, u16 *peerPort)
#else
static bool cdcnetGetEndpoints(int sd, u32 *localAddr, u16 *localPort,
                               u32 *peerAddr, u16 *peerPort)
#endif
    {
    struct sockaddr_in hostAddr;
    int                rc;

#if defined(_WIN32)
    int addrLen;
#else
    socklen_t addrLen;
#endif

    addrLen = sizeof(hostAddr);
    rc      = getsockname(sd, (struct sockaddr *)&hostAddr, &addrLen);
    if (rc == 0)
        {
        *localAddr = ntohl(hostAddr.sin_addr.s_addr);
        *localPort = ntohs(hostAddr.sin_port);
        }
    else
        {
        logDtError(LogErrorLocation, "CDCNet: Failed to get local socket name: %s\n", strerror(errno));

        return (FALSE);
        }

    addrLen = sizeof(hostAddr);
    rc      = getpeername(sd, (struct sockaddr *)&hostAddr, &addrLen);
    if (rc == 0)
        {
        *peerAddr = ntohl(hostAddr.sin_addr.s_addr);
        *peerPort = ntohs(hostAddr.sin_port);
        }
    else
        {
        logDtError(LogErrorLocation, "CDCNet: Failed to get peer socket name: %s\n", strerror(errno));

        return (FALSE);
        }

    return (TRUE);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Close a gateway connection.
**
**  Parameters:     Name        Description.
**                  gp          Gcb pointer
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cdcnetCloseConnection(Gcb *gp)
    {
    NpuBuffer *bp;
    Pccb      *pp;

    if (gp->connFd != 0)
        {
        netCloseConnection(gp->connFd);
        gp->connFd = 0;
        }

    pp = cdcnetFindPccb(gp->dstPort);
    if ((pp != NULL) && (pp->tcpGcbOrdinal == gp->ordinal))
        {
        if (pp->srcPort != 0)
            {
#if DEBUG
            fprintf(cdcnetLog, "Close listening socket, port=%d, CN=%02X\n", pp->dstPort, gp->cn);
#endif
            netCloseConnection(pp->connFd);
            pp->dstPort = 0;
            pp->connFd  = 0;
            }
        else if (pp->connFd > 0)
            {
            pp->deadline = getSeconds() + 10;
#if DEBUG
            fprintf(cdcnetLog, "Leave listening socket open, port=%d, CN=%02X\n", pp->dstPort, gp->cn);
#endif
            }
        pp->tcpGcbOrdinal = 0;
        }

    gp->gwState     = StGwIdle;
    gp->tcpUdpState = StTcpUdpIdle;
    gp->initStatus  = 0;
    gp->tcpSapId    = 0;

    while ((bp = npuBipQueueExtract(&gp->downlineQueue)) != NULL)
        {
        npuBipBufRelease(bp);
        }
    while ((bp = npuBipQueueExtract(&gp->outputQueue)) != NULL)
        {
        npuBipBufRelease(bp);
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Send acknowledgement of reception of a block.
**
**  Parameters:     Name        Description.
**                  gp          Gcb pointer
**                  bp          pointer to NpuBuffer to use for upline message
**                  bsn         block sequence number to acknowledge
**
**------------------------------------------------------------------------*/
static void cdcnetSendBack(Gcb *gp, NpuBuffer *bp, u8 bsn)
    {
    u8 *mp;

    mp = bp->data;

    *mp++ = npuSvmCouplerNode;                   // DN
    *mp++ = cdcnetNode;                          // SN
    *mp++ = gp->cn;                              // CN
    *mp++ = BlkOffBTBSN | (bsn << BlkShiftBSN);  // BT=Back | BSN

    bp->numBytes = (u16)(mp - bp->data);
    bp->offset   = 0;

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

    bp->numBytes = (u16)(mp - bp->data);
    bp->offset   = 0;

    npuBipRequestUplineTransfer(bp);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Send initialize connection request.
**
**  Parameters:     Name        Description.
**                  gp          Gcb pointer
**
**  Returns:        TRUE if request sent.
**                  FALSE if insufficient resources
**
**------------------------------------------------------------------------*/
static bool cdcnetSendInitializeConnectionRequest(Gcb *gp)
    {
    NpuBuffer *bp;
    u8        *mp;

    bp = npuBipBufGet();
    if (bp == NULL)
        {
        return (FALSE);
        }

    mp = bp->data;

    *mp++ = npuSvmCouplerNode;              // DN
    *mp++ = cdcnetNode;                     // SN
    *mp++ = gp->cn;                         // CN
    *mp++ = BtHTRINIT;                      // Initialize request

    bp->numBytes = (u16)(mp - bp->data);

    npuBipRequestUplineTransfer(bp);

    return (TRUE);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Send initialize connection response.
**
**  Parameters:     Name        Description.
**                  bp          pointer to NpuBuffer to use for upline message
**                  gp          Gcb pointer
**
**  Returns:        nothing.
**
**------------------------------------------------------------------------*/
static void cdcnetSendInitializeConnectionResponse(NpuBuffer *bp, Gcb *gp)
    {
    u8 *mp;

    mp = bp->data;

    *mp++ = npuSvmCouplerNode;              // DN
    *mp++ = cdcnetNode;                     // SN
    *mp++ = gp->cn;                         // CN
    *mp++ = BtHTNINIT;                      // Initialize response

    bp->numBytes = (u16)(mp - bp->data);
    bp->offset   = 0;

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

    bp->numBytes = (u16)(mp - bp->data);
    bp->offset   = 0;

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

    bp->numBytes = (u16)(mp - bp->data);
    bp->offset   = 0;

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

    bp->numBytes = (u16)(mp - bp->data);
    bp->offset   = 0;

    npuBipRequestUplineTransfer(bp);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Get an IP address from a TCP gateway message.
**
**  Parameters:     Name        Description.
**                  ap          pointer to gateway-encoded IP address structure
**
**  Returns:        Assembled address.
**
**------------------------------------------------------------------------*/
static u32 cdcnetTcpGetIpAddress(u8 *ap)
    {
    u8  inUse;
    u32 ipAddr;

    inUse  = ap[RelOffTcpIpAddrFieldsInUse];
    ipAddr = 0;

    if (inUse & 0x40)
        {
        ipAddr = (ap[RelOffTcpIpAddressNetwork] << 24)
                 | (ap[RelOffTcpIpAddressNetwork + 1] << 16)
                 | (ap[RelOffTcpIpAddressNetwork + 2] << 8);
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
        ipAddr |= ap[RelOffTcpIpAddressHost + 2];
        }
    else if (ipAddr & 0x80000000) // Class B address
        {
        ipAddr |= (ap[RelOffTcpIpAddressHost + 1] << 8) | ap[RelOffTcpIpAddressHost + 2];
        }
    else // Class A address
        {
        ipAddr |= (ap[RelOffTcpIpAddressHost] << 16)
                  | (ap[RelOffTcpIpAddressHost + 1] << 8)
                  | ap[RelOffTcpIpAddressHost + 2];
        }

    return (ipAddr);
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
static void cdcnetTcpSetIpAddress(u8 *ap, u32 ipAddr)
    {
    ap[RelOffTcpIpAddrFieldsInUse] |= 0xC0;

    if ((ipAddr & 0xC0000000) == 0xC0000000) // Class C address
        {
        ap[RelOffTcpIpAddressNetwork]     = (ipAddr >> 24) & 0xff;
        ap[RelOffTcpIpAddressNetwork + 1] = (ipAddr >> 16) & 0xff;
        ap[RelOffTcpIpAddressNetwork + 2] = (ipAddr >> 8) & 0xff;

        ap[RelOffTcpIpAddressHost]     = 0;
        ap[RelOffTcpIpAddressHost + 1] = 0;
        ap[RelOffTcpIpAddressHost + 2] = ipAddr & 0xff;
        }
    else if (ipAddr & 0x80000000) // Class B address
        {
        ap[RelOffTcpIpAddressNetwork]     = 0;
        ap[RelOffTcpIpAddressNetwork + 1] = (ipAddr >> 24) & 0xff;
        ap[RelOffTcpIpAddressNetwork + 2] = (ipAddr >> 16) & 0xff;

        ap[RelOffTcpIpAddressHost]     = 0;
        ap[RelOffTcpIpAddressHost + 1] = (ipAddr >> 8) & 0xff;
        ap[RelOffTcpIpAddressHost + 2] = ipAddr & 0xff;
        }
    else // Class A address
        {
        ap[RelOffTcpIpAddressNetwork]     = 0;
        ap[RelOffTcpIpAddressNetwork + 1] = 0;
        ap[RelOffTcpIpAddressNetwork + 2] = (ipAddr >> 24) & 0xff;

        ap[RelOffTcpIpAddressHost]     = (ipAddr >> 16) & 0xff;
        ap[RelOffTcpIpAddressHost + 1] = (ipAddr >> 8) & 0xff;
        ap[RelOffTcpIpAddressHost + 2] = ipAddr & 0xff;
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
static u16 cdcnetTcpGetPort(u8 *ap)
    {
    u8  inUse;
    u16 port;

    inUse = ap[RelOffTcpPortInUse] & 0x80;
    port  = 0;

    if (inUse != 0)
        {
        port = (ap[RelOffTcpPort] << 8) | ap[RelOffTcpPort + 1];
        }

    return (port);
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
static void cdcnetTcpSetPort(u8 *ap, u16 port)
    {
    ap[RelOffTcpPortInUse] |= 0x80;
    ap[RelOffTcpPort]       = (port >> 8) & 0xff;
    ap[RelOffTcpPort + 1]   = port & 0xff;
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
    return ((*ip << 24) | (*(ip + 1) << 16) | (*(ip + 2) << 8) | *(ip + 3));
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
    *mp++ = (value >> 8) & 0xff;
    *mp++ = value & 0xff;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Queue a TCP block for upline transfer.
**
**  Parameters:     Name        Description.
**                  gp          pointer to gateway control block
**                  bp          pointer to NpuBuffer containing block
**                  blockType   block type (BtHTMSG or BtHTQMSG)
**                  headerType  TCP/IP gateway header type
**                  status      TCP/IP gateway response/indication status
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cdcnetTcpRequestUplineTransfer(Gcb *gp, NpuBuffer *bp, u8 blockType, u8 headerType, TcpGwStatus status)
    {
    bp->data[BlkOffDN]    = npuSvmCouplerNode;
    bp->data[BlkOffSN]    = cdcnetNode;
    bp->data[BlkOffCN]    = gp->cn;
    bp->data[BlkOffBTBSN] = (gp->bsn++ << BlkShiftBSN) | blockType;
    if (gp->bsn > 7)
        {
        gp->bsn = 1;
        }
    bp->data[BlkOffDbc] = 0;
    if (blockType == BtHTQMSG)
        {
        bp->data[BlkOffTcpStatus]     = (status >> 8) & 0xff;
        bp->data[BlkOffTcpStatus + 1] = status & 0xff;
        bp->data[BlkOffTcpHeaderType] = headerType;
        }
#if DEBUG
    fprintf(cdcnetLog, "Send TCP block upline (%d bytes)\n", bp->numBytes);
    cdcnetLogBytes(bp->data, bp->numBytes);
    cdcnetLogFlush();
#endif
    gp->unackedBlocks++;
    npuBipRequestUplineTransfer(bp);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Handle TCP Gateway Open SAP request.
**
**  Parameters:     Name        Description.
**                  gp          pointer to gateway control block
**                  bp          pointer to NpuBuffer containing request
**
**  Returns:        TRUE if request handled and response sent successfully
**                  FALSE if request should be retried
**
**------------------------------------------------------------------------*/
static bool cdcnetTcpOpenSAPHandler(Gcb *gp, NpuBuffer *bp)
    {
    gp->userSapId = cdcnetGetIdFromMessage(bp->data + BlkOffTcpOSUserSapId);
#if DEBUG
    fprintf(cdcnetLog, "Open SAP request, gwVersion=%02X, tcpSapId=%d, userSapId=%d, CN=%02X\n",
            bp->data[BlkOffTcpOSTcpIpGwVer], gp->ordinal, gp->userSapId, gp->cn);
#endif
    gp->tcpSapId = (u32)gp->ordinal;
    cdcnetPutIdToMessage(gp->tcpSapId, bp->data + BlkOffTcpOSTcpSapId);
    cdcnetTcpRequestUplineTransfer(gp, bp, BtHTQMSG, cdcnetTcpHTResponse, tcp_successful);

    return (TRUE);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Handle TCP Gateway Close SAP request.
**
**  Parameters:     Name        Description.
**                  gp          pointer to gateway control block
**                  bp          pointer to NpuBuffer containing request
**
**  Returns:        TRUE if request handled and response sent successfully
**                  FALSE if request should be retried
**
**------------------------------------------------------------------------*/
static bool cdcnetTcpCloseSAPHandler(Gcb *gp, NpuBuffer *bp)
    {
    int i;
    Gcb *gp2;
    u32 tcpSapId;

    tcpSapId = cdcnetGetIdFromMessage(bp->data + BlkOffTcpCSTcpSapId);

#if DEBUG
    fprintf(cdcnetLog, "Close SAP request, tcpSapId=%d, CN=%02X\n", tcpSapId, gp->cn);
#endif

    for (i = 0, gp2 = cdcnetGcbs; i < cdcnetGcbCount; i++, gp2++)
        {
        if ((gp2->tcpSapId == tcpSapId) && (gp2->tcpUdpState != StTcpUdpIdle))
            {
            cdcnetCloseConnection(gp2);
            }
        }

    cdcnetTcpRequestUplineTransfer(gp, bp, BtHTQMSG, cdcnetTcpHTResponse, tcp_successful);

    return (TRUE);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Handle TCP Gateway request to abort the current connection
**
**  Parameters:     Name        Description.
**                  gp          pointer to gateway control block
**                  bp          pointer to NpuBuffer containing request
**
**  Returns:        TRUE if request handled and response sent successfully
**                  FALSE if request should be retried
**
**------------------------------------------------------------------------*/
static bool cdcnetTcpAbortCurrentConnectionHandler(Gcb *gp, NpuBuffer *bp)
    {
#if DEBUG
    fprintf(cdcnetLog, "Abort current connection request, tcpCepId=%d, CN=%02X\n",
            cdcnetGetIdFromMessage(bp->data + BlkOffTcpACCTcpCepId), gp->cn);
#endif
    if (gp->connFd != 0)
        {
        netCloseConnection(gp->connFd);
        gp->connFd = 0;
        }
    gp->tcpUdpState = StTcpUdpIdle;
    cdcnetTcpRequestUplineTransfer(gp, bp, BtHTQMSG, cdcnetTcpHTResponse, tcp_successful);

    return (TRUE);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Handle TCP Gateway Active Connect request.
**
**  Parameters:     Name        Description.
**                  gp          pointer to gateway control block
**                  bp          pointer to NpuBuffer containing request
**
**  Returns:        TRUE if request handled and response sent successfully
**                  FALSE if request should be retried
**
**------------------------------------------------------------------------*/
static bool cdcnetTcpActiveConnectHandler(Gcb *gp, NpuBuffer *bp)
    {
    u8                 *dp;
    u32                dstAddr;
    struct sockaddr_in hostAddr;
    u32                srcAddr;
    TcpGwStatus        status;

    gp->connType  = TypeTcpActive;
    gp->tcpSapId  = cdcnetGetIdFromMessage(bp->data + BlkOffTcpACTcpSapId);
    gp->userCepId = cdcnetGetIdFromMessage(bp->data + BlkOffTcpACUserCepId);
    gp->tcpCepId  = 0;
    memcpy(gp->tcpSrcAddress, bp->data + BlkOffTcpACSrcAddr, cdcnetTcpAddressLength);
    memcpy(gp->tcpDstAddress, bp->data + BlkOffTcpACDstAddr, cdcnetTcpAddressLength);
    dp      = bp->data + BlkOffTcpACSrcAddr;
    srcAddr = cdcnetTcpGetIpAddress(dp);
    sprintf(gp->srcIpAddress, "%d.%d.%d.%d", srcAddr >> 24, (srcAddr >> 16) & 0xff, (srcAddr >> 8) & 0xff, srcAddr & 0xff);
    gp->srcPort = cdcnetTcpGetPort(dp);
    dp          = bp->data + BlkOffTcpACDstAddr;
    dstAddr     = cdcnetTcpGetIpAddress(dp);
    sprintf(gp->dstIpAddress, "%d.%d.%d.%d", dstAddr >> 24, (dstAddr >> 16) & 0xff, (dstAddr >> 8) & 0xff, dstAddr & 0xff);
    gp->dstPort = cdcnetTcpGetPort(dp);
#if DEBUG
    fprintf(cdcnetLog, "Active Connect request, tcpSapId=%d, userCepId=%d, tcpCepId=%d, CN=%02X\n        source addr=%s:%d, dest addr=%s:%d\n",
            gp->tcpSapId, gp->userCepId, gp->tcpCepId, gp->cn,
            gp->srcIpAddress, gp->srcPort, gp->dstIpAddress, gp->dstPort);
#endif
    status = tcp_successful;

    if (gp->tcpUdpState == StTcpUdpIdle)
        {
        hostAddr.sin_addr.s_addr = htonl(dstAddr);
        hostAddr.sin_family      = AF_INET;
        hostAddr.sin_port        = htons(gp->dstPort);
        gp->connFd = netInitiateConnection((struct sockaddr *)&hostAddr);
#if defined(_WIN32)
        if (gp->connFd == INVALID_SOCKET)
#else
        if (gp->connFd == -1)
#endif
            {
#if DEBUG
            fprintf(cdcnetLog, "Failed to initiate connection to host: %s:%u, CN=%02X\n", gp->dstIpAddress, gp->dstPort, gp->cn);
#endif
            gp->connFd = 0;
            status     = tcp_host_unreachable;
            }
        else     // connection in progress
            {
#if DEBUG
            fprintf(cdcnetLog, "Initiated connection to host: %s:%u, CN=%02X\n",
                    gp->dstIpAddress, gp->dstPort, gp->cn);
#endif
            gp->tcpUdpState = StTcpConnecting;
            gp->deadline    = getSeconds() + 60;
            }
        }

    cdcnetTcpRequestUplineTransfer(gp, bp, BtHTQMSG, cdcnetTcpHTResponse, status);

    return (TRUE);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Handle TCP Gateway Passive Connect request.
**
**  Parameters:     Name        Description.
**                  gp          pointer to gateway control block
**                  bp          pointer to NpuBuffer containing request
**
**  Returns:        TRUE if request handled and response sent successfully
**                  FALSE if request should be retried
**
**------------------------------------------------------------------------*/
static bool cdcnetTcpPassiveConnectHandler(Gcb *gp, NpuBuffer *bp)
    {
    u8          *dp;
    u32         dstAddr;
    u16         dstPort;
    int         optEnable = 1;
    Pccb        *pp;
    u32         srcAddr;
    TcpGwStatus status;

    gp->connType  = TypeTcpPassive;
    gp->tcpSapId  = cdcnetGetIdFromMessage(bp->data + BlkOffTcpPCTcpSapId);
    gp->userCepId = cdcnetGetIdFromMessage(bp->data + BlkOffTcpPCUserCepId);
    gp->tcpCepId  = (u32)gp->ordinal;
    memcpy(gp->tcpDstAddress, bp->data + BlkOffTcpPCSrcAddr, cdcnetTcpAddressLength);
    memcpy(gp->tcpSrcAddress, bp->data + BlkOffTcpPCDstAddr, cdcnetTcpAddressLength);
    dp      = bp->data + BlkOffTcpPCSrcAddr;
    dstAddr = cdcnetTcpGetIpAddress(dp);
    sprintf(gp->dstIpAddress, "%d.%d.%d.%d", dstAddr >> 24, (dstAddr >> 16) & 0xff, (dstAddr >> 8) & 0xff, dstAddr & 0xff);
    dstPort = cdcnetTcpGetPort(dp);
    if (dstPort == 0) // assign next available port
        {
        if (++cdcnetPassivePort >= 10000)
            {
            cdcnetPassivePort = 7600;
            }
        gp->dstPort = cdcnetPassivePort;
        }
    else if (dstPort < 1024) // add offset to privileged port number
        {
        gp->dstPort = dstPort + cdcnetPrivilegedTcpPortOffset;
        }
    else
        {
        gp->dstPort = dstPort;
        }
    dp      = bp->data + BlkOffTcpPCDstAddr;
    srcAddr = cdcnetTcpGetIpAddress(dp);
    sprintf(gp->srcIpAddress, "%d.%d.%d.%d", srcAddr >> 24, (srcAddr >> 16) & 0xff, (srcAddr >> 8) & 0xff, srcAddr & 0xff);
    gp->srcPort = cdcnetTcpGetPort(dp);
#if DEBUG
    fprintf(cdcnetLog, "Passive Connect request, tcpSapId=%d, userCepId=%d, tcpCepId=%d, CN=%02X\n        source addr=%s:%d, dest addr=%s:%d\n",
            gp->tcpSapId, gp->userCepId, gp->tcpCepId, gp->cn,
            gp->srcIpAddress, gp->srcPort, gp->dstIpAddress, gp->dstPort);
#endif
    status = tcp_successful;

    if (gp->tcpUdpState == StTcpUdpIdle)
        {
        pp = cdcnetFindPccb(gp->dstPort);
        if (pp != NULL)
            {
            /*
            ** The gateway is qlready bound and listening to the requested port,
            ** so reserve and use it, unless it's already being used by another
            ** connection.
            */
            if (pp->tcpGcbOrdinal == 0) // available for re-use
                {
#if DEBUG
                fprintf(cdcnetLog, "Listening for connections (re-use) on port %d, CN=%02X\n", gp->dstPort, gp->cn);
#endif
                pp->srcPort       = gp->srcPort;
                pp->tcpGcbOrdinal = gp->ordinal;
                gp->tcpUdpState   = StTcpListening;
                }
            else // in use by another connection
                {
#if DEBUG
                fprintf(cdcnetLog, "Connection in use by %02X, port=%u, CN=%02X\n",
                        (cdcnetGcbs + (pp->tcpGcbOrdinal - 1))->cn, gp->dstPort, gp->cn);
#endif
                status = tcp_connection_inuse;
                }
            cdcnetTcpRequestUplineTransfer(gp, bp, BtHTQMSG, cdcnetTcpHTResponse, status);

            return (TRUE);
            }

        pp = cdcnetGetPccb();
        if (pp == NULL)
            {
#if DEBUG
            fprintf(cdcnetLog, "Failed to get/create a control block for a passive connection on port %d, CN=%02X\n",
                    gp->dstPort, gp->cn);
#endif
            cdcnetTcpRequestUplineTransfer(gp, bp, BtHTQMSG, cdcnetTcpHTResponse, tcp_no_resources);

            return (TRUE);
            }

        pp->srcPort       = gp->srcPort;
        pp->dstPort       = gp->dstPort;
        pp->tcpGcbOrdinal = gp->ordinal;

        while (TRUE)
            {
            pp->connFd = netCreateListener(pp->dstPort);
#if defined(_WIN32)
            if (pp->connFd != INVALID_SOCKET)
                {
                break;
                }
#else
            if (pp->connFd != -1)
                {
                break;
                }
#endif
#if DEBUG
            fprintf(cdcnetLog, "Can't bind to listening socket on port %d, %s, CN=%02X\n",
                    pp->dstPort, strerror(errno), gp->cn);
#endif
            if (dstPort == 0)
                {
                if (++cdcnetPassivePort >= 10000)
                    {
                    cdcnetPassivePort = 7600;
                    }
                pp->dstPort = cdcnetPassivePort;
                }
            else
                {
                pp->dstPort       = 0;
                pp->tcpGcbOrdinal = 0;
                netCloseConnection(pp->connFd);
                pp->connFd = 0;
                cdcnetTcpRequestUplineTransfer(gp, bp, BtHTQMSG, cdcnetTcpHTResponse, tcp_internal_error);

                return (TRUE);
                }
            }
        gp->dstPort = pp->dstPort;
#if DEBUG
        fprintf(cdcnetLog, "Listening for connections on port %d, CN=%02X\n", gp->dstPort, gp->cn);
#endif
        gp->tcpUdpState = StTcpListening;
        cdcnetTcpSetPort(bp->data + BlkOffTcpPCSrcAddr, gp->dstPort);
        cdcnetPutIdToMessage(gp->tcpCepId, bp->data + BlkOffTcpPCTcpCepId);
        }
    else // connection not idle
        {
#if DEBUG
        fprintf(cdcnetLog, "Attempted to create passive connection on non-idle connection, CN=%02X\n", gp->cn);
#endif
        status = tcp_connection_inuse;
        }

    cdcnetTcpRequestUplineTransfer(gp, bp, BtHTQMSG, cdcnetTcpHTResponse, status);

    return (TRUE);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Handle TCP Gateway Allocate request.
**
**  Parameters:     Name        Description.
**                  gp          pointer to gateway control block
**                  bp          pointer to NpuBuffer containing request
**
**  Returns:        TRUE if request handled and response sent successfully
**                  FALSE if request should be retried
**
**------------------------------------------------------------------------*/
static bool cdcnetTcpAllocateHandler(Gcb *gp, NpuBuffer *bp)
    {
#if DEBUG
    fprintf(cdcnetLog, "Allocate request, tcpCepId=%d, size=%08X, CN=%02X\n",
            cdcnetGetIdFromMessage(bp->data + BlkOffTcpATcpCepId),
            cdcnetGetIdFromMessage(bp->data + BlkOffTcpASize), gp->cn);
#endif
    cdcnetTcpRequestUplineTransfer(gp, bp, BtHTQMSG, cdcnetTcpHTResponse, tcp_successful);

    return (TRUE);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Handle TCP Gateway Disconnect request.
**
**  Parameters:     Name        Description.
**                  gp          pointer to gateway control block
**                  bp          pointer to NpuBuffer containing request
**
**  Returns:        TRUE if request handled and response sent successfully
**                  FALSE if request should be retried
**
**------------------------------------------------------------------------*/
static bool cdcnetTcpDisconnectHandler(Gcb *gp, NpuBuffer *bp)
    {
    Pccb *pp;

#if DEBUG
    fprintf(cdcnetLog, "Disconnect request, tcpCepId=%d, CN=%02X\n", cdcnetGetIdFromMessage(bp->data + BlkOffTcpDTcpCepId), gp->cn);
#endif
    memcpy(bp->data + BlkOffTcpCmdName, "TCPDC  ", 7);
    cdcnetPutU16ToMessage(cdcnetTcpDCLength, bp->data + BlkOffTcpHeaderLen);
    bp->data[BlkOffTcpTcpVersion] = cdcnetTcpVersion;
    cdcnetPutIdToMessage(gp->userCepId, bp->data + BlkOffTcpDCUserCepId);
    bp->numBytes = cdcnetTcpDCLength + BlkOffTcpCmdName;
    cdcnetTcpRequestUplineTransfer(gp, bp, BtHTQMSG, cdcnetTcpHTIndication, tcp_successful);

    if (gp->tcpUdpState == StTcpListening)
        {
        pp = cdcnetFindPccb(gp->dstPort);
        if ((pp != NULL) && (pp->tcpGcbOrdinal == gp->ordinal))
            {
            if (pp->srcPort != 0)
                {
#if DEBUG
                fprintf(cdcnetLog, "Close listening socket, port=%d, CN=%02X\n", pp->dstPort, gp->cn);
#endif
                netCloseConnection(pp->connFd);
                pp->dstPort = 0;
                pp->srcPort = 0;
                pp->connFd  = 0;
                }
#if DEBUG
            else
                {
                fprintf(cdcnetLog, "Leave listening socket open, port=%d, CN=%02X\n", pp->dstPort, gp->cn);
                }
#endif
            pp->tcpGcbOrdinal = 0;
            }
        }
    else if (gp->connFd != 0)
        {
#if DEBUG
        fprintf(cdcnetLog, "Close socket, source addr=%s:%d, dest addr=%s:%d, CN=%02X\n",
                gp->srcIpAddress, gp->srcPort, gp->dstIpAddress, gp->dstPort, gp->cn);
#endif
        netCloseConnection(gp->connFd);
        gp->connFd = 0;
        }

    gp->tcpUdpState = StTcpUdpIdle;

    return (TRUE);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Send indication that a TCP connection has been established.
**
**  Parameters:     Name        Description.
**                  gp          pointer to gateway control block
**
**  Returns:        TRUE if indication sent successfully
**                  FALSE if indication should be retried
**
**------------------------------------------------------------------------*/
static bool cdcnetTcpSendConnectionIndication(Gcb *gp)
    {
    NpuBuffer *bp;

    bp = npuBipBufGet();
    if (bp == NULL)
        {
        return (FALSE);
        }
    memset(bp->data, 0, cdcnetTcpCILength + BlkOffTcpCmdName);

#if DEBUG
    fprintf(cdcnetLog, "Send connection indication, userCepId=%d, CN=%02X\n", gp->userCepId, gp->cn);
#endif
    memcpy(bp->data + BlkOffTcpCmdName, "TCPCI  ", 7);
    cdcnetPutU16ToMessage(cdcnetTcpCILength, bp->data + BlkOffTcpHeaderLen);
    bp->data[BlkOffTcpTcpVersion] = cdcnetTcpVersion;
    cdcnetPutIdToMessage(gp->userCepId, bp->data + BlkOffTcpCIUserCepId);
    if (gp->connType == TypeTcpActive)
        {
        cdcnetTcpSetIpAddress(gp->tcpSrcAddress, gp->localAddr);
        gp->srcPort = gp->localPort;
        cdcnetTcpSetPort(gp->tcpSrcAddress, gp->localPort);
        cdcnetTcpSetIpAddress(gp->tcpDstAddress, gp->peerAddr);
        gp->dstPort = gp->peerPort;
        cdcnetTcpSetPort(gp->tcpDstAddress, gp->peerPort);
        gp->tcpCepId = (u32)gp->ordinal;
        }
    else
        {
        cdcnetTcpSetIpAddress(gp->tcpSrcAddress, gp->peerAddr);
        gp->srcPort = gp->peerPort;
        cdcnetTcpSetPort(gp->tcpSrcAddress, gp->peerPort);
        cdcnetTcpSetIpAddress(gp->tcpDstAddress, gp->localAddr);
        if ((gp->localPort >= cdcnetPrivilegedTcpPortOffset)
            && (gp->localPort < cdcnetPrivilegedTcpPortOffset + 1024))
            {
            cdcnetTcpSetPort(gp->tcpDstAddress, gp->localPort - cdcnetPrivilegedTcpPortOffset);
            }
        else
            {
            cdcnetTcpSetPort(gp->tcpDstAddress, gp->localPort);
            }
        }
    memcpy(bp->data + BlkOffTcpCISrcAddr, gp->tcpSrcAddress, cdcnetTcpAddressLength);
    memcpy(bp->data + BlkOffTcpCIDstAddr, gp->tcpDstAddress, cdcnetTcpAddressLength);
    memset(bp->data + BlkOffTcpCIIpHeader, 0, BlkOffTcpCIIpOptions - BlkOffTcpCIIpHeader);
    memset(bp->data + BlkOffTcpCIIpOptions, 0, BlkOffTcpCIUlpTimeout - BlkOffTcpCIIpOptions);
    memset(bp->data + BlkOffTcpCIIpHeader, 0, (cdcnetTcpCILength + BlkOffTcpCmdName) - BlkOffTcpCIUlpTimeout);
    bp->numBytes = cdcnetTcpCILength + BlkOffTcpCmdName;
    cdcnetTcpRequestUplineTransfer(gp, bp, BtHTQMSG, cdcnetTcpHTIndication, tcp_successful);

    return (TRUE);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Send an indication that an error has occurred.
**
**  Parameters:     Name        Description.
**                  gp          pointer to gateway control block
**                              reasonCode element provides error code value
**
**  Returns:        TRUE if error indication sent successfully
**                  FALSE if error indication should be retried
**
**------------------------------------------------------------------------*/
static bool cdcnetTcpSendErrorIndication(Gcb *gp)
    {
    NpuBuffer *bp;

    bp = npuBipBufGet();
    if (bp == NULL)
        {
        return (FALSE);
        }
    memset(bp->data, 0, cdcnetTcpEILength + BlkOffTcpCmdName);

#if DEBUG
    fprintf(cdcnetLog, "Send error indication, userCepId=%d, error=%d, CN=%02X\n", gp->userCepId, gp->reasonCode, gp->cn);
#endif
    memcpy(bp->data + BlkOffTcpCmdName, "TCPEI  ", 7);
    cdcnetPutU16ToMessage(cdcnetTcpEILength, bp->data + BlkOffTcpHeaderLen);
    bp->data[BlkOffTcpTcpVersion] = cdcnetTcpVersion;
    cdcnetPutIdToMessage(gp->userCepId, bp->data + BlkOffTcpEIUserCepId);
    bp->numBytes = cdcnetTcpEILength + BlkOffTcpCmdName;
    cdcnetTcpRequestUplineTransfer(gp, bp, BtHTQMSG, cdcnetTcpHTIndication, gp->reasonCode);
    gp->reasonCode = 0;

    return (TRUE);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Send an indication that TCP data has been received.
**
**  Parameters:     Name        Description.
**                  gp          pointer to gateway control block
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cdcnetTcpSendDataIndication(Gcb *gp)
    {
    u8          blockType;
    NpuBuffer   *bp;
    int         n;
    int         recvSize;
    TcpGwStatus status;

    bp = npuBipBufGet();
    if (bp == NULL)
        {
        return;
        }

#if DEBUG
    fprintf(cdcnetLog, "Send data indication, userCepId=%d, CN=%02X\n", gp->userCepId, gp->cn);
#endif
    blockType = BtHTMSG;
    status    = tcp_successful;

    recvSize = sizeof(bp->data) - (BlkOffDbc + 1);
    if (recvSize > gp->maxUplineBlockSize)
        {
        recvSize = gp->maxUplineBlockSize;
        }
    n = recv(gp->connFd, bp->data + BlkOffDbc + 1, recvSize, 0);
    if (n > 0)
        {
#if DEBUG
        fprintf(cdcnetLog, "Received %d bytes, CN=%02X\n", n, gp->cn);
#endif
        bp->numBytes = BlkOffDbc + n + 1;
        }
    else if (n == 0)
        {
#if DEBUG
        fprintf(cdcnetLog, "Stream closed, CN=%02X\n", gp->cn);
#endif
        blockType = BtHTQMSG;
        memset(bp->data + BlkOffDbc + 1, 0, cdcnetTcpDILength + BlkOffTcpCmdName - (BlkOffDbc + 1));
        gp->tcpUdpState = StTcpDisconnecting;
        memcpy(bp->data + BlkOffTcpCmdName, "TCPDI  ", 7);
        cdcnetPutU16ToMessage(cdcnetTcpDILength, bp->data + BlkOffTcpHeaderLen);
        bp->data[BlkOffTcpTcpVersion] = cdcnetTcpVersion;
        cdcnetPutIdToMessage(gp->userCepId, bp->data + BlkOffTcpDIUserCepId);
        bp->numBytes = cdcnetTcpDILength + BlkOffTcpCmdName;
        }
    else
        {
#if DEBUG
        fprintf(cdcnetLog, "Error reading stream: %s, CN=%02X\n", strerror(errno), gp->cn);
#endif
        blockType = BtHTQMSG;
        memset(bp->data + BlkOffDbc + 1, 0, cdcnetTcpEILength + BlkOffTcpCmdName - (BlkOffDbc + 1));
        memcpy(bp->data + BlkOffTcpCmdName, "TCPEI  ", 7);
        cdcnetPutU16ToMessage(cdcnetTcpEILength, bp->data + BlkOffTcpHeaderLen);
        status = tcp_internal_error;
        bp->data[BlkOffTcpTcpVersion] = cdcnetTcpVersion;
        cdcnetPutIdToMessage(gp->userCepId, bp->data + BlkOffTcpEIUserCepId);
        bp->numBytes = cdcnetTcpEILength + BlkOffTcpCmdName;
        }
    cdcnetTcpRequestUplineTransfer(gp, bp, blockType, cdcnetTcpHTIndication, status);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Create a non-blocking UDP socket.
**
**  Parameters:     Name        Description.
**
**  Returns:        Socket descriptor
**                  -1 if socket cannot be created
**
**------------------------------------------------------------------------*/
#if defined(_WIN32)
static SOCKET cdcnetCreateUdpSocket()
#else
static int cdcnetCreateUdpSocket()
#endif
    {
#if defined(_WIN32)
    SOCKET fd;
    u_long blockEnable = 1;
#else
    int fd;
#endif

    fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#if defined(_WIN32)
    if (fd == INVALID_SOCKET)
        {
        return -1;
        }
    ioctlsocket(fd, FIONBIO, &blockEnable);
#else
    if (fd < 0)
        {
        return -1;
        }
    fcntl(fd, F_SETFL, O_NONBLOCK);
#endif

    return (fd);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Get an IP address from a UDP gateway message.
**
**  Parameters:     Name        Description.
**                  ap          pointer to gateway-encoded IP address structure
**
**  Returns:        Assembled address.
**
**------------------------------------------------------------------------*/
static u32 cdcnetUdpGetIpAddress(u8 *ap)
    {
    u8  inUse;
    u32 ipAddr;

    inUse  = ap[RelOffUdpIpAddrFieldsInUse];
    ipAddr = 0;

    if (inUse & 0x01)
        {
        ipAddr = (ap[RelOffUdpIpAddressNetwork + 1] << 24)
                 | (ap[RelOffUdpIpAddressNetwork + 2] << 16)
                 | (ap[RelOffUdpIpAddressNetwork + 3] << 8);
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
        ipAddr |= ap[RelOffUdpIpAddressHost + 3];
        }
    else if (ipAddr & 0x80000000) // Class B address
        {
        ipAddr |= (ap[RelOffUdpIpAddressHost + 2] << 8) | ap[RelOffUdpIpAddressHost + 3];
        }
    else // Class A address
        {
        ipAddr |= (ap[RelOffUdpIpAddressHost + 1] << 16)
                  | (ap[RelOffUdpIpAddressHost + 2] << 8)
                  | ap[RelOffUdpIpAddressHost + 3];
        }

    return (ipAddr);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Get a UDP port number from a gateway message.
**
**  Parameters:     Name        Description.
**                  ap          pointer to gateway-encoded IP address structure
**
**  Returns:        TCP port number; 0 if no port specified.
**
**------------------------------------------------------------------------*/
static u16 cdcnetUdpGetPort(u8 *ap)
    {
    u16 port;

    port = 0;

    if (ap[RelOffUdpPortInUse] != 0)
        {
        port = (ap[RelOffUdpPort + 1] << 8) | ap[RelOffUdpPort + 2];
        }

    return (port);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Set a UDP address and port number in a gateway message.
**
**  Parameters:     Name        Description.
**                  dp          pointer to gateway-encoded IP address structure
**                  ipAddress   32-bit IP address
**                  port        16-bit port number
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cdcnetUdpSetAddress(u8 *dp, u32 ipAddress, u16 port)
    {
    *dp++ = 0x03;                               // fields in use (both net and host)
    *dp++ = 0;                                  // most significant byte of net

    if ((ipAddress & 0xC0000000) == 0xC0000000) // Class C address
        {
        *dp++ = (ipAddress >> 24) & 0xff;
        *dp++ = (ipAddress >> 16) & 0xff;
        *dp++ = (ipAddress >> 8) & 0xff;

        *dp++ = 0;
        *dp++ = 0;
        *dp++ = 0;
        *dp++ = ipAddress & 0xff;
        }
    else if (ipAddress & 0x80000000) // Class B address
        {
        *dp++ = 0;
        *dp++ = (ipAddress >> 24) & 0xff;
        *dp++ = (ipAddress >> 16) & 0xff;

        *dp++ = 0;
        *dp++ = 0;
        *dp++ = (ipAddress >> 8) & 0xff;
        *dp++ = ipAddress & 0xff;
        }
    else // Class A address
        {
        *dp++ = 0;
        *dp++ = 0;
        *dp++ = (ipAddress >> 24) & 0xff;

        *dp++ = 0;
        *dp++ = (ipAddress >> 16) & 0xff;
        *dp++ = (ipAddress >> 8) & 0xff;
        *dp++ = ipAddress & 0xff;
        }
    *dp++ = 0x01; // port in use
    *dp++ = 0;
    *dp++ = (port >> 8) & 0xff;
    *dp++ = port & 0xff;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Queue a UDP block for upline transfer.
**
**  Parameters:     Name        Description.
**                  gp          pointer to gateway control block
**                  bp          pointer to NpuBuffer containing block
**                  blockType   block type (BtHTMSG or BtHTQMSG)
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cdcnetUdpRequestUplineTransfer(Gcb *gp, NpuBuffer *bp, u8 blockType)
    {
    bp->data[BlkOffDN]    = npuSvmCouplerNode;
    bp->data[BlkOffSN]    = cdcnetNode;
    bp->data[BlkOffCN]    = gp->cn;
    bp->data[BlkOffBTBSN] = (gp->bsn++ << BlkShiftBSN) | blockType;
    if (gp->bsn > 7)
        {
        gp->bsn = 1;
        }
    bp->data[BlkOffDbc] = 0;
#if DEBUG
    fprintf(cdcnetLog, "Send UDP block upline (%d bytes), CN=%02X\n", bp->numBytes, gp->cn);
    cdcnetLogBytes(bp->data, bp->numBytes);
    cdcnetLogFlush();
#endif
    gp->unackedBlocks++;
    npuBipRequestUplineTransfer(bp);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Bind a UDP address to a gateway control block.
**
**  Parameters:     Name        Description.
**                  gp          pointer to gateway control block
**                  bp          pointer to NpuBuffer containing request
**
**  Returns:        TRUE if request handled and response sent successfully
**                  FALSE if request should be retried
**
**------------------------------------------------------------------------*/
static bool cdcnetUdpBindAddress(Gcb *gp, NpuBuffer *bp)
    {
#if defined(_WIN32)
    int addrLen;
#else
    socklen_t addrLen;
#endif
    u32                ipAddress;
    u16                port;
    struct sockaddr_in server;

    ipAddress = cdcnetUdpGetIpAddress(bp->data + BlkOffUdpOpenSapSrcAddr);
    port      = cdcnetUdpGetPort(bp->data + BlkOffUdpOpenSapSrcAddr);
#if DEBUG
    fprintf(cdcnetLog, "Bind UDP address, %d.%d.%d.%d:%d, CN=%02X\n",
            ipAddress >> 24, (ipAddress >> 16) & 0xff, (ipAddress >> 8) & 0xff, ipAddress & 0xff, port, gp->cn);
#endif
    gp->connFd = cdcnetCreateUdpSocket();
    if (gp->connFd == -1)
        {
        npuBipBufRelease(bp);

        return TRUE; // TODO: return some kind of error status upline?
        }
    memset(&server, 0, sizeof(server));
    server.sin_family      = AF_INET;
    server.sin_addr.s_addr = htonl(ipAddress);
    if ((port > 0) && (port < 1024))
        {
        port += cdcnetPrivilegedUdpPortOffset;
        }
    if (bind(gp->connFd, (struct sockaddr *)&server, sizeof(server)) == -1)
        {
#if DEBUG
        fprintf(cdcnetLog, "Can't bind to UDP socket on port %d, %s, CN=%02X\n",
                port, strerror(errno), gp->cn);
#endif
        netCloseConnection(gp->connFd);
        gp->connFd = 0;
        npuBipBufRelease(bp);

        return TRUE; // TODO: return some kind of error status upline?
        }
    addrLen = sizeof(server);
    if (getsockname(gp->connFd, (struct sockaddr *)&server, &addrLen) != 0)
        {
        logDtError(LogErrorLocation, "CDCNet: Failed to get local UDP socket name: %s\n", strerror(errno));
        netCloseConnection(gp->connFd);
        gp->connFd = 0;

        return FALSE;
        }
    ipAddress = ntohl(server.sin_addr.s_addr);
    port      = ntohs(server.sin_port);
    if ((port >= cdcnetPrivilegedUdpPortOffset) && (port < cdcnetPrivilegedUdpPortOffset + 1024))
        {
        port -= cdcnetPrivilegedUdpPortOffset;
        }
    cdcnetUdpSetAddress(bp->data + BlkOffUdpOpenSapSrcAddr, ipAddress, port);
    gp->srcPort     = port;
    gp->connType    = TypeUdp;
    gp->tcpUdpState = StUdpBound;
    bp->data[BlkOffUdpRequestType] = cdcnetUdpCallResponse;
    cdcnetUdpRequestUplineTransfer(gp, bp, BtHTMSG);

    return TRUE;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Send data to a UDP address.
**
**  Parameters:     Name        Description.
**                  gp          pointer to gateway control block
**                  bp          pointer to NpuBuffer containing request
**
**  Returns:        TRUE if request handled and response sent successfully
**                  FALSE if request should be retried
**
**------------------------------------------------------------------------*/
static bool cdcnetUdpSendDownlineData(Gcb *gp, NpuBuffer *bp)
    {
    u32                ipAddress;
    int                len;
    int                n;
    u16                port;
    struct sockaddr_in server;

    ipAddress = cdcnetUdpGetIpAddress(bp->data + BlkOffUdpDataReqDstAddr);
    port      = cdcnetUdpGetPort(bp->data + BlkOffUdpDataReqDstAddr);
#if DEBUG
    fprintf(cdcnetLog, "Send UDP data to %d.%d.%d.%d:%d, CN=%02X\n",
            ipAddress >> 24, (ipAddress >> 16) & 0xff, (ipAddress >> 8) & 0xff, ipAddress & 0xff, port, gp->cn);
#endif
    if (gp->tcpUdpState != StUdpBound)
        {
#if DEBUG
        fprintf(cdcnetLog, "Invalid UDP state %d, CN=%02X\n", gp->tcpUdpState, gp->cn);
#endif
        npuBipBufRelease(bp);

        return TRUE; // TODO: return some kind of error status upline?
        }
    memset(&server, 0, sizeof(server));
    server.sin_family      = AF_INET;
    server.sin_addr.s_addr = htonl(ipAddress);
    server.sin_port        = htons(port);
    len = bp->numBytes - BlkOffUdpDataReqData;
    n   = sendto(gp->connFd, bp->data + BlkOffUdpDataReqData, len, 0, (struct sockaddr *)&server, sizeof(server));
    if (n == len)
        {
#if DEBUG
        cdcnetLogBytes(bp->data + BlkOffUdpDataReqData, len);
        cdcnetLogFlush();
#endif
        npuBipBufRelease(bp);

        return TRUE;
        }
    else if (n == -1)
        {
#if DEBUG
        fprintf(cdcnetLog, "Failed to send UDP data: %s, CN=%02X\n", strerror(errno), gp->cn);
#endif
        npuBipBufRelease(bp);

        return TRUE;
        }
    else
        {
#if DEBUG
        fprintf(cdcnetLog, "Failed to send complete UDP packet (%d of %d bytes), CN=%02X\n", n, len, gp->cn);
#endif

        return FALSE;
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Send an indication that UDP data has been received.
**
**  Parameters:     Name        Description.
**                  gp          pointer to gateway control block
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void cdcnetUdpSendUplineData(Gcb *gp)
    {
    NpuBuffer          *bp;
    struct sockaddr_in client;
    u8                 *dp;
    u32                ipAddress;

#if defined(_WIN32)
    int len = 0;
#else
    socklen_t len;
#endif
    int n;
    u16 port;
    int recvSize;

    bp = npuBipBufGet();
    if (bp == NULL)
        {
        return;
        }

    recvSize = sizeof(bp->data) - BlkOffUdpDataIndData;
    len      = sizeof(client);
    n        = recvfrom(gp->connFd, bp->data + BlkOffUdpDataIndData, recvSize, 0, (struct sockaddr *)&client, &len);
    if (n < 1)
        {
#if DEBUG
        if (n == -1)
            {
#if defined(_WIN32)
            fprintf(cdcnetLog, "socket error %d: CN=%02X\n", WSAGetLastError(), gp->cn);
#else
            fprintf(cdcnetLog, "%s, CN=%02X\n", strerror(errno), gp->cn);
#endif
            }
#endif
        npuBipBufRelease(bp);

        return;
        }
    ipAddress = ntohl(client.sin_addr.s_addr);
    port      = ntohs(client.sin_port);
#if DEBUG
    fprintf(cdcnetLog, "Received %d bytes from UDP client %d.%d.%d.%d:%d, CN=%02X\n",
            n, ipAddress >> 24, (ipAddress >> 16) & 0xff, (ipAddress >> 8) & 0xff, ipAddress & 0xff, port, gp->cn);
    cdcnetLogBytes(bp->data + BlkOffUdpDataIndData, n);
    cdcnetLogFlush();
#endif
    dp    = bp->data + BlkOffDbc;
    *dp++ = 0;
    *dp++ = cdcnetUdpDataIndication;
    *dp++ = cdcnetUdpVersion;
    *dp++ = 0;     // unused byte
    cdcnetUdpSetAddress(dp, ipAddress, port);
    bp->numBytes = n + BlkOffUdpDataIndData;
    cdcnetUdpRequestUplineTransfer(gp, bp, BtHTMSG);
    }

#if DEBUG

/*--------------------------------------------------------------------------
**  Purpose:        Flush incomplete data line
**
**  Parameters:     Name        Description.
**
**  Returns:        nothing
**
**------------------------------------------------------------------------*/
static void cdcnetLogFlush(void)
    {
    if (cdcnetLogBytesCol > 0)
        {
        fputs(cdcnetLogBuf, cdcnetLog);
        fputc('\n', cdcnetLog);
        fflush(cdcnetLog);
        }
    cdcnetLogBytesCol = 0;
    memset(cdcnetLogBuf, ' ', LogLineLength);
    cdcnetLogBuf[LogLineLength] = '\0';
    }

/*--------------------------------------------------------------------------
**  Purpose:        Log a sequence of bytes
**
**  Parameters:     Name        Description.
**                  bytes       pointer to sequence of bytes
**                  len         length of the sequence
**
**  Returns:        nothing
**
**------------------------------------------------------------------------*/
static void cdcnetLogBytes(u8 *bytes, int len)
    {
    u8   ac;
    int  ascCol;
    u8   b;
    char hex[3];
    int  hexCol;
    int  i;

    ascCol = AsciiColumn(cdcnetLogBytesCol);
    hexCol = HexColumn(cdcnetLogBytesCol);

    for (i = 0; i < len; i++)
        {
        b  = bytes[i];
        ac = b;
        if ((ac < 0x20) || (ac >= 0x7f))
            {
            ac = '.';
            }
        sprintf(hex, "%02x", b);
        memcpy(cdcnetLogBuf + hexCol, hex, 2);
        hexCol += 3;
        cdcnetLogBuf[ascCol++] = ac;
        if (++cdcnetLogBytesCol >= 16)
            {
            cdcnetLogFlush();
            ascCol = AsciiColumn(cdcnetLogBytesCol);
            hexCol = HexColumn(cdcnetLogBytesCol);
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
static void cdcnetPrintStackTrace(FILE *fp)
    {
#if defined(__APPLE__)
    void *callstack[128];
    int  i;
    int  frames;
    char **strs;

    frames = backtrace(callstack, 128);
    strs   = backtrace_symbols(callstack, frames);
    for (i = 1; i < frames; ++i)
        {
        fprintf(fp, "%s\n", strs[i]);
        }
    free(strs);
#endif
    }

#endif // DEBUG

/*---------------------------  End Of File  ------------------------------*/
