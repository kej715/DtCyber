/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Gerard J van der Grinten, Tom Hunter
**
**  Name: tpmux.c
**
**  Description:
**      Perform emulation of the two port multiplexer.
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
#include <string.h>
#include <sys/types.h>
#include "const.h"
#include "types.h"
#include "proto.h"
#if defined(_WIN32)
#include <winsock.h>
#else
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#define DEBUG 0
/*
**  -----------------
**  Private Constants
**  -----------------
*/

/*
**
**         THE FOLLOWING IS A DESCIPTION OF TWO-PORT MULTIPLEXER        
**         STATUS AND DATA FORMATS.                                     
**                                                                      
**                                                                      
**         TWO-PORT MULTIPLEXER STATUS CODES-                           
**                                                                      
**                BIT         DESCRIPTION                               
**                ---         -----------                               
**                                                                      
**               11-5         NOT USED.                                 
**                  4         OUTPUT BUFFER READY (NOT FULL).           
**                  3         INPUT READY.                              
**                  2         CARRIER ON.                               
**                  1         DATA SET READY.                           
**                  0         RING INDICATOR.                           
**                                                                      
**                                                                      
**         TWO-PORT MULTIPLEXER MODE SELECTION FUNCTION CODES-          
**                                                                      
**                BIT         DESCRIPTION                               
**                ---         -----------                               
**                                                                      
**               11-6         NOT USED.                                 
**                  5         ENABLE LOOP-BACK.                         
**                  4         DISABLE PARITY.                           
**                  3         NUMBER OF STOP BITS-                      
**                                0 = 1 STOP BIT.                       
**                                1 = 2 STOP BITS.                      
**                2-1         DATA BITS PER CHARACTER-                  
**                               00 = 5 BITS.                           
**                               01 = 6 BITS.                           
**                               10 = 7 BITS.                           
**                               11 = 8 BITS.                           
**                  0         PARITY-                                   
**                                0 = ODD PARITY.                       
**                                1 = EVEN PARITY.                      
**                                                                      
**                                                                      
**         INPUT DATA BYTE FORMAT-                                      
**                                                                      
**                BIT         DESCRIPTION                               
**                ---         -----------                               
**                                                                      
**                 11         DATA SET READY.                           
**                 10         DATA SET READY .AND. CARRIER ON.          
**                  9         LOST DATA.                                
**                  8         FRAMING ERROR OR PARITY ERROR.            
**                7-0         DATA BITS.                                
**                                                                      
**                                                                      
**         OUTPUT DATA BYTE FORMAT-                                     
**                                                                      
**                BIT         DESCRIPTION                               
**                ---         -----------                               
**                                                                      
**               11-8         NOT USED.                                 
**                  7         DATA PARITY.                              
**                6-0         DATA BITS (LEAST SIGNIFICANT DATA BIT     
**                            IN BIT POSITION 0).                       
**
**
**         TWO-PORT MULTIPLEXER EST ENTRY.                              
**                                                                      
**         THE FORMAT OF THE TWO-PORT MULTIPLEXER EQUIPMENT STATUS      
**         TABLE ENTRY IS AS SHOWN BELOW.                               
**                                                                      
**         EST    12/0, 12/CH, 12/0, 12/RM, 11/0, 1/N                          
**                                                                      
**                CH = CHANNEL NUMBER.                                  
**                                                                      
**                RM = MNEMONIC *RM*.                                   
**                                                                      
**                N  = PORT NUMBER, 0 OR 1.                             
**                                                                      
**                                                                      
**         THE FORMAT OF THE *EQPDECK* ENTRY FOR THE TWO-PORT           
**         MULTIPLEXER IS AS SHOWN BELOW.                               
**                                                                      
**         EQXXX=RM,ST=ON/OFF,CH=NN,PT=N.                               
**                                                                      
**                *EQ* = MNEMONIC *EQ*.                                 
**                                                                      
**                *RM* = MNEMONIC *RM*.                                 
**                                                                      
**                ST   = STATUS, *ON* OR *OFF*.                         
**                                                                      
**                PN   = PORT NUMBER, *0* OR *1*.                       
**                                                                      
**                CH   = CHANNEL NUMBER.                                
**                                                                      
**         EXAMPLE -                                                    
**                                                                      
**         EQ765=RM,ST=ON,CH=15,PT=0.                                   
**                                                                      
**                THE ABOVE ENTRY DEFINES EST ORDINAL 765 AS PORT 0 OF  
**                THE TWO-PORT MULTIPLEXER, LOGICALLY *ON* IN THE EST.  
**  
**  
**  
**           TWO PORT MULTIPLEXOR EQUIVALENCES.                         
**                                                                      
**           FUNCTIONS.                                                 
**                                                                      
**  MXSS     EQU    0000        STATUS SUMMARY                          
**  MXRD     EQU    0100        READ CHARACTER                          
**  MXWT     EQU    0200        WRITE CHARACTERS                        
**  MXSM     EQU    0300        SET TERMINAL OPERATION MODE             
**  MXDR     EQU    0400        SET/CLEAR DATA TERMINAL READY SIGNAL    
**  MXRTS    EQU    0500        SET/CLEAR REQUEST TO SEND SIGNAL        
**           EQU    0600        (NOT USED)                              
**  MXMC     EQU    0700        MASTER CLEAR                            
**  MXDM     EQU    6000        DESELECT TERMINAL                       
**  MXPT     EQU    7000        CONNECT TO PORT                         
**                                                                      
**           STATUS SUMMARY BIT DEFINITIONS.                            
**                                                                      
**  OBRB     EQU    4           OUTPUT BUFFER NOT FULL                  
**  INRB     EQU    3           INPUT READY                             
**  DCDB     EQU    2           DATA CARRIER DETECT                     
**  DSRB     EQU    1           DATA SET READY                          
**  RNGB     EQU    0           RING INDICATOR                          
**                                                                      
**           INPUT CHARACTER BIT DEFINITIONS.                           
**                                                                      
**  RDSR     EQU    13          DATA SET READY                          
**  RDSC     EQU    12          DATA SET READY AND DATA CHARACTER DETECT
**  ROVR     EQU    11          OVER RUN                                
**  RFPE     EQU    10          FRAMING PARITY ERROR                    
**  CHAR     EQU    0-7         DATA CHARACTER                          
**                                                                      
**           OPERATION MODE SELECTIONS.                                 
**                                                                      
**  SPTY     EQU    0020        NO PARITY                               
**  SSTP     EQU    0010        SELECT ADDITIONAL STOP BIT              
**  S8BC     EQU    0006        SELECT 8 DATA BITS PER CHARACTER        
**  S7BC     EQU    0004        SELECT 7 DATA BITS PER CHARACTER        
**  S6BC     EQU    0002        SELECT 6 DATA BITS PER CHARACTER        
**  S5BC     EQU    0000        SELECT 5 DATA BITS PER CHARACTER        
**  SODD     EQU    0001        SELECT ODD PARITY                       
**
*/

/*
**  Function codes.
*/
#define FcTpmStatusSumary        00000
#define FcTpmReadChar            00100
#define FcTpmWriteChar           00200
#define FcTpmSetTerminal         00300
#define FcTpmFlipDTR             00400
#define FcTpmFlipRTS             00500
#define FcTpmNotUsed             00600
#define FcTpmMasterClear         00700
#define FcTpmDeSelect            06000
#define FcTpmConPort             07000

#define StTpmOutputNotFull       00020
#define StTpmInputReady          00010
#define StTpmDataCharDet         00004
#define StTpmDataSetReady        00002
#define StTpmRingIndicator       00001

#define IcTpmDataSetReady        04000
#define IcTpmDsrAndDcd           02000
#define IcTpmDataOverrun         01000
#define IcTpmFramingError        00400

#define TpmSystemConsole         00000
#define TpmMaintConsole          00001

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
typedef struct portParam
    {
    u8          id;
    bool        active;
    int         connFd;
    PpWord      status;
    char        input;
    char        output[512];
    } PortParam;

/*
**  ---------------------------
**  Private Function Prototypes
**  ---------------------------
*/
static FcStatus tpMuxFunc(PpWord funcCode);
static void tpMuxIo(void);
static void tpMuxActivate(void);
static void tpMuxDisconnect(void);
static void tpMuxCreateThread(DevSlot *dp);
static int tpMuxCheckInput(PortParam *mp);
#if defined(_WIN32)
static void tpMuxThread(void *param);
#else
static void *tpMuxThread(void *param);
#endif

/*
**  ----------------
**  Public Variables
**  ----------------
*/
static u16 telnetPort = 6602;
static u16 telnetConns = 2;
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
**  Purpose:        Initialise terminal multiplexer.
**
**  Parameters:     Name        Description.
**                  eqNo        equipment number
**                  unitNo      unit number
**                  channelNo   channel number the device is attached to
**                  deviceName  optional device file name
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void tpMuxInit(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName)
    {
    DevSlot *dp;
    PortParam *mp;
    u8 i;

    (void)eqNo;
    (void)deviceName;

    dp = channelAttach(channelNo, eqNo, DtTpm);
    dp->activate = tpMuxActivate;
    dp->disconnect = tpMuxDisconnect;
    dp->func = tpMuxFunc;
    dp->io = tpMuxIo;
    dp->selectedUnit = -1;

    mp = calloc(1, sizeof(PortParam) * telnetConns);
    if (mp == NULL)
        {
        fprintf(stderr, "Failed to allocate two port mux context block\n");
        exit(1);
        }

    dp->context[0] = mp;

    /*
    **  Initialise port control blocks.
    */
    for (i = 0; i < telnetConns; i++)
        {
        mp->status = 00026;
        mp->active = FALSE;
        mp->connFd = 0;
        mp->id = i;
        mp += 1;
        }

    /*
    **  Create the thread which will deal with TCP connections.
    */
    tpMuxCreateThread(dp);

    /*
    **  Print a friendly message.
    */
    printf("Two port MUX initialised on channel %o\n", channelNo);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Execute function code on two-port mux.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        FcStatus
**
**------------------------------------------------------------------------*/
static FcStatus tpMuxFunc(PpWord funcCode)
    {
    int funcParam;

    funcParam = funcCode & 077;
    switch (funcCode & 07700)
        {
    default:
#if DEBUG
        printf("Function on tpm %04o Declined \n", funcCode);
#endif
        return(FcDeclined);

    case FcTpmStatusSumary:
        break;

    case FcTpmReadChar:
        break;

    case FcTpmWriteChar:
        activeDevice->recordLength = 0;
        break;

    case FcTpmSetTerminal:
#if DEBUG
        printf("Set Terminal mode %03o (unit %d)\n", funcParam, activeDevice->selectedUnit);
#endif
        break;

    case FcTpmFlipDTR:
#if DEBUG
        printf("%s DTR (unit %d)\n",funcParam == 0 ? "Clear" : "Set", activeDevice->selectedUnit);
#endif
        break;

    case FcTpmFlipRTS:
#if DEBUG
        printf("%s RTS (unit %d)\n",funcParam == 0 ? "Clear" : "Set", activeDevice->selectedUnit);
#endif
        break;

    case FcTpmMasterClear:
        return(FcProcessed);

    case FcTpmDeSelect:
        activeDevice->selectedUnit = -1;
        return(FcProcessed);

    case FcTpmConPort:
        activeDevice->selectedUnit = 1 - (funcParam & 1);
        return(FcProcessed);
        }

    activeDevice->fcode = funcCode;
    return(FcAccepted);
    }

/*--------------------------------------------------------------------------
**  Purpose:        Perform I/O on two-port mux.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void tpMuxIo(void)
    {
    PortParam *mp;

    if (activeDevice->selectedUnit < 0)
        {
        return;
        }

    mp = (PortParam *)activeDevice->context[0] + activeDevice->selectedUnit;

    switch (activeDevice->fcode & 00700)
        {
    case FcTpmStatusSumary:
        if (!activeChannel->full)
            {
            if (   mp->active
                && (mp->input = tpMuxCheckInput(mp)) > 0)
                {
                mp->status |= 00010;
                }
            else
                {
                mp->status &= ~00010;
                }

            activeChannel->data = mp->status;
            activeChannel->full = TRUE;
            }
        break;

    case FcTpmReadChar:
        if (!activeChannel->full && (mp->status & 00010) != 0)
            {
            activeChannel->data = (PpWord)mp->input | 06000;
            activeChannel->full = TRUE;
            mp->status &= ~00010;
#if DEBUG
            printf("read port %d -  %04o\n",mp->id, activeChannel->data);
#endif
            }
        break;

    case FcTpmWriteChar:
        if (activeChannel->full)
            {
            /*
            **  Output data.
            */
            activeChannel->full = FALSE;

            if (mp->active)
                {
                /*
                **  Port with active TCP connection.
                */
                mp->output[activeDevice->recordLength] = (char)activeChannel->data & 0177;
                activeDevice->recordLength++;
                if (activeDevice->recordLength == sizeof(mp->output))
                   {
                   send(mp->connFd, mp->output, activeDevice->recordLength, 0);
                   activeDevice->recordLength = 0;
                   }
#if DEBUG
                printf("write port %d - %04o\n", mp->id, activeChannel->data);
#endif
                }
            }
        break;
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
static void tpMuxActivate(void)
    {
    }

/*--------------------------------------------------------------------------
**  Purpose:        Handle disconnecting of channel.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void tpMuxDisconnect(void)
    {
    PortParam *mp;

    if (activeDevice->selectedUnit < 0)
        {
        return;
        }

    mp = (PortParam *)activeDevice->context[0] + activeDevice->selectedUnit;

    if (activeDevice->fcode == FcTpmWriteChar)
        {
        if (mp->active)
            {
            send(mp->connFd, mp->output, activeDevice->recordLength, 0);
            activeDevice->recordLength = 0;
            }
        }
    }


/*--------------------------------------------------------------------------
**  Purpose:        Create WIN32 thread which will deal with all TCP
**                  connections.
**
**  Parameters:     Name        Description.
**                  dp          pointer to device descriptor
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void tpMuxCreateThread(DevSlot *dp)
    {
#if defined(_WIN32)
    static bool firstMux = TRUE;
    DWORD dwThreadId; 
    HANDLE hThread;

    if (firstMux)
        {
        firstMux = FALSE;
        }

    /*
    **  Create TCP thread.
    */
    hThread = CreateThread( 
        NULL,                                       // no security attribute 
        0,                                          // default stack size 
        (LPTHREAD_START_ROUTINE)tpMuxThread, 
        (LPVOID)dp,                                 // thread parameter 
        0,                                          // not suspended 
        &dwThreadId);                               // returns thread ID 

    if (hThread == NULL)
        {
        fprintf(stderr, "Failed to create tpMux thread\n");
        exit(1);
        }
#else
    int rc;
    pthread_t thread;
    pthread_attr_t attr;

    /*
    **  Create POSIX thread with default attributes.
    */
    pthread_attr_init(&attr);
    rc = pthread_create(&thread, &attr, tpMuxThread, dp);
    if (rc < 0)
        {
        fprintf(stderr, "Failed to create tpMux thread\n");
        exit(1);
        }
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        TCP thread.
**
**  Parameters:     Name        Description.
**                  param       pointer to mux parameters.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
#if defined(_WIN32)
static void tpMuxThread(void *param)
#else
static void *tpMuxThread(void *param)
#endif
    {
    DevSlot *dp = (DevSlot *)param;
    int listenFd;
    struct sockaddr_in server;
    struct sockaddr_in from;
    PortParam *mp;
    u8 i;
    int reuse = 1;
#if defined(_WIN32)
    int fromLen;
#else
    socklen_t fromLen;
#endif

    /*
    **  Create TCP socket and bind to specified port.
    */
    listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd < 0)
        {
        printf("tpMux: Can't create socket\n");
#if defined(_WIN32)
        return;
#else
        return(NULL);
#endif
        }

    setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse));
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr("0.0.0.0");
    server.sin_port = htons(telnetPort);

    if (bind(listenFd, (struct sockaddr *)&server, sizeof(server)) < 0)
        {
        printf("tpMux: Can't bind to socket\n");
#if defined(_WIN32)
        return;
#else
        return(NULL);
#endif
        }

    if (listen(listenFd, 5) < 0)
        {
        printf("tpMux: Can't listen\n");
#if defined(_WIN32)
        return;
#else
        return(NULL);
#endif
        }

    while (1)
        {
        /*
        **  Find a free port control block.
        */
        mp = (PortParam *)dp->context[0];
        for (i = 0; i < telnetConns; i++)
            {
            if (!mp->active)
                {
                break;
                }
            mp += 1;
            }

        if (i == telnetConns)
            {
            /*
            **  No free port found - wait a bit and try again.
            */
        #if defined(_WIN32)
            Sleep(1000);
        #else
            //usleep(10000000);
            sleep(1);
        #endif
            continue;
            }

        /*
        **  Wait for a connection.
        */
        fromLen = sizeof(from);
        mp->connFd = accept(listenFd, (struct sockaddr *)&from, &fromLen);

        /*
        **  Mark connection as active.
        */
        mp->active = TRUE;
        printf("tpMux: Received connection on port %d\n", mp->id);
        }

#if !defined(_WIN32)
    return(NULL);
#endif
    }

/*--------------------------------------------------------------------------
**  Purpose:        Check for input.
**
**  Parameters:     Name        Description.
**                  mp          pointer to mux parameters.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static int tpMuxCheckInput(PortParam *mp)
    {
    int i;
    fd_set readFds;
    fd_set exceptFds;
    struct timeval timeout;
    char data;

    FD_ZERO(&readFds);
    FD_ZERO(&exceptFds);
    FD_SET(mp->connFd, &readFds);
    FD_SET(mp->connFd, &exceptFds);

    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    select(mp->connFd + 1, &readFds, NULL, &exceptFds, &timeout);
    if (FD_ISSET(mp->connFd, &readFds))
        {
        i = recv(mp->connFd, &data, 1, 0);
        if (i == 1)
            {
            return(data);
            }
        else
            {
        #if defined(_WIN32)
            closesocket(mp->connFd);
        #else
            close(mp->connFd);
        #endif
            mp->active = FALSE;
            printf("tpMux: Connection dropped on port %d\n", mp->id);
            return(-1);
            }
        }
    else if (FD_ISSET(mp->connFd, &exceptFds))
        {
    #if defined(_WIN32)
        closesocket(mp->connFd);
    #else
        close(mp->connFd);
    #endif
        mp->active = FALSE;
        printf("tpMux: Connection dropped on port %d\n", mp->id);
        return(-1);
        }
    else
        {
        return(0);
        }
    }

/*---------------------------  End Of File  ------------------------------*/
