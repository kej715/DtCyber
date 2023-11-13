/*--------------------------------------------------------------------------
**
**  Copyright (c) 2023, Kevin Jordan
**
**  Name: net_util.c
**
**  Description:
**      Provides TCP/IP utility functions that are independent of the
**      underlying host operating system.
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
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#if defined(_WIN32)
#include <winsock.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include "proto.h"

/*
**  -----------------
**  Private Constants
**  -----------------
*/
#define MaxListenBacklog 100

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

/*
 **--------------------------------------------------------------------------
 **
 **  Public Functions
 **
 **--------------------------------------------------------------------------
 */

/*--------------------------------------------------------------------------
**  Purpose:        Accepts a network connection
**
**  Parameters:     Name        Description.
**                  sd          socket descriptor
**
**  Returns:        network descriptor, or invalid socket indication on error.
**
**------------------------------------------------------------------------*/
#if defined(_WIN32)
SOCKET netAcceptConnection(SOCKET sd)
    {
    SOCKET             acceptFd;
    struct sockaddr_in from;
    int                fromLen;

    fromLen  = sizeof(from);
    acceptFd = accept(sd, (struct sockaddr *)&from, &fromLen);
    if (acceptFd == INVALID_SOCKET)
        {
        fprintf(stderr, "(net_util) accept failed, rc=%d\n", WSAGetLastError());
        }
    return acceptFd;
    }
#else
int netAcceptConnection(int sd)
    {
    int                acceptFd;
    struct sockaddr_in from;
    socklen_t          fromLen;

    fromLen  = sizeof(from);
    acceptFd = accept(sd, (struct sockaddr *)&from, &fromLen);
    if (acceptFd < 0)
        {
        perror("(net_util) accept");
        }
    return acceptFd;
    }
#endif

/*--------------------------------------------------------------------------
**  Purpose:        Closes a network connection
**
**  Parameters:     Name        Description.
**                  sd          socket descriptor
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
#if defined(_WIN32)
void netCloseConnection(SOCKET sd)
    {
    closesocket(sd);
    }
#else
void netCloseConnection(int sd)
    {
    close(sd);
    }
#endif

/*--------------------------------------------------------------------------
**  Purpose:        Create a listening socket
**
**  Parameters:     Name        Description.
**                  port        port number on which to listen
**
**  Returns:        socket descriptor, or -1 on error
**
**------------------------------------------------------------------------*/
#if defined(_WIN32)
SOCKET netCreateListener(int port)
#else
int    netCreateListener(int port)
#endif
    {
#if defined(_WIN32)
    SOCKET sd;
#else
    int    sd;
#endif

    sd = netCreateSocket(port, TRUE);

#if defined(_WIN32)
    if (sd == INVALID_SOCKET) return sd;
#else
    if (sd == -1) return sd;
#endif

    /*
    **  Start listening for new connections on this TCP port number
    */
    if (listen(sd, MaxListenBacklog) == -1)
        {
#if DEBUG
        perror("(net_util) listen");
#endif
#if defined(_WIN32)
        closesocket(sd);
        return INVALID_SOCKET;
#else
        close(sd);
        return -1;
#endif
        }

    return sd;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Create a non-blocking socket bound to a specified port.
**
**  Parameters:     Name        Description.
**                  port        port number on which to bind - 0 if operating
**                              system should assign it
**                  isReuse     TRUE is SO_REUSEADDR to be set
**
**  Returns:        socket descriptor, or -1 on error
**
**------------------------------------------------------------------------*/
#if defined(_WIN32)
SOCKET netCreateSocket(int port, bool isReuse)
#else
int    netCreateSocket(int port, bool isReuse)
#endif
    {
    int                optEnable = 1;
    int                rc;
    struct sockaddr_in srcAddr;

#if defined(_WIN32)
    u_long    blockEnable = 1;
    int       optLen;
    int       optVal;
    SOCKET    sd;
#else
    socklen_t optLen;
    int       optVal;
    int       sd;
#endif

    sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#if defined(_WIN32)
    if (sd == INVALID_SOCKET) return sd;
#else
    if (sd == -1) return sd;
#endif

    if (isReuse)
        {
        setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (void *)&optEnable, sizeof(optEnable));
        }

    memset(&srcAddr, 0, sizeof(srcAddr));
    srcAddr.sin_family      = AF_INET;
    srcAddr.sin_addr.s_addr = inet_addr(ipAddress);
    srcAddr.sin_port        = htons(port);
    if (bind(sd, (struct sockaddr *)&srcAddr, sizeof(srcAddr)) == -1)
        {
#if DEBUG
        perror("(net_util) bind");
#endif
#if defined(_WIN32)
        closesocket(sd);
        return INVALID_SOCKET;
#else
        close(sd);
        return -1;
#endif
        }

    setsockopt(sd, SOL_SOCKET, SO_KEEPALIVE, (void *)&optEnable, sizeof(optEnable));

#if defined(_WIN32)
    ioctlsocket(sd, FIONBIO, &blockEnable);
#else
    fcntl(sd, F_SETFL, O_NONBLOCK);
#endif

    return sd;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Get and clear current socket error status.
**
**  Parameters:     Name        Description.
**                  sd          socket descriptor
**
**  Returns:        current error status
**
**------------------------------------------------------------------------*/
#if defined(_WIN32)
int netGetErrorStatus(SOCKET sd)
    {
    int optLen;
#else
int netGetErrorStatus(int sd)
    {
    socklen_t optLen;
#endif
    int optVal;
    int rc;

#if defined(_WIN32)
    optLen = sizeof(optVal);
    rc     = getsockopt(sd, SOL_SOCKET, SO_ERROR, (char *)&optVal, &optLen);
#else
    optLen = (socklen_t)sizeof(optVal);
    rc     = getsockopt(sd, SOL_SOCKET, SO_ERROR, &optVal, &optLen);
#endif

    return (rc == -1) ? -1 : optVal;
    }

/*--------------------------------------------------------------------------
**  Purpose:        Get the local IP address and port number of a socket.
**
**  Parameters:     Name        Description.
**                  sd          socket descriptor
**
**  Returns:        IP address and port number as a string
**
**------------------------------------------------------------------------*/
#if defined(_WIN32)
char *netGetLocalTcpAddress(SOCKET sd)
#else
char *netGetLocalTcpAddress(int sd)
#endif
    {
    struct sockaddr_in hostAddr;
    u32                ipAddr;
    static char        outBuf[32];
    u16                port;
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
        ipAddr = ntohl(hostAddr.sin_addr.s_addr);
        port   = ntohs(hostAddr.sin_port);
        sprintf(outBuf, "%d.%d.%d.%d:%d",
          (ipAddr >> 24) & 0xff,
          (ipAddr >> 16) & 0xff,
          (ipAddr >>  8) & 0xff,
          ipAddr         & 0xff,
          port);
        return outBuf;
        }
    else
        {
        return "";
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Get the peer IP address and port number of a socket.
**
**  Parameters:     Name        Description.
**                  sd          socket descriptor
**
**  Returns:        IP address and port number as a string
**
**------------------------------------------------------------------------*/
#if defined(_WIN32)
char *netGetPeerTcpAddress(SOCKET sd)
#else
char *netGetPeerTcpAddress(int sd)
#endif
    {
    struct sockaddr_in hostAddr;
    u32                ipAddr;
    static char        outBuf[32];
    u16                port;
    int                rc;

#if defined(_WIN32)
    int addrLen;
#else
    socklen_t addrLen;
#endif

    addrLen = sizeof(hostAddr);
    rc      = getpeername(sd, (struct sockaddr *)&hostAddr, &addrLen);
    if (rc == 0)
        {
        ipAddr = ntohl(hostAddr.sin_addr.s_addr);
        port   = ntohs(hostAddr.sin_port);
        sprintf(outBuf, "%d.%d.%d.%d:%d",
          (ipAddr >> 24) & 0xff,
          (ipAddr >> 16) & 0xff,
          (ipAddr >>  8) & 0xff,
          ipAddr         & 0xff,
          port);
        return outBuf;
        }
    else
        {
        return "";
        }
    }

/*--------------------------------------------------------------------------
**  Purpose:        Initiate a connection to a TCP service.
**
**  Parameters:     Name        Description.
**                  sap         pointer to sockaddr structure definining
**                              IP address and port to which to connect
**
**  Returns:        socket descriptor, or -1 on error
**
**------------------------------------------------------------------------*/
#if defined(_WIN32)
SOCKET netInitiateConnection(struct sockaddr *sap)
#else
int    netInitiateConnection(struct sockaddr *sap)
#endif
    {
    int rc;

#if defined(_WIN32)
    SOCKET    sd;
#else
    int       sd;
#endif

    sd = netCreateSocket(0, FALSE);

#if defined(_WIN32)
    if (sd == INVALID_SOCKET) return sd;
    rc = connect(sd, sap, sizeof(*sap));
    if ((rc == SOCKET_ERROR) && (WSAGetLastError() != WSAEWOULDBLOCK))
        {
#if DEBUG
        perror("(net_util) connect");
#endif
        closesocket(sd);
        return INVALID_SOCKET;
        }
#else
    if (sd == -1) return sd;
    rc = connect(sd, sap, sizeof(*sap));
    if ((rc == -1) && (errno != EINPROGRESS))
        {
#if DEBUG
        perror("(net_util) connect");
#endif
        close(sd);
        return -1;
        }
#endif

    return sd;
    }

/*
 **--------------------------------------------------------------------------
 **
 **  Private Functions
 **
 **--------------------------------------------------------------------------
 */

/*---------------------------  End Of File  ------------------------------*/
