/* msufrend_util.h - interface between DtCyber and frend2.
 * Mark Riordan  23 June 2004;  27 February 2005;  12 February 2006
 */
#ifndef MSUFREND_UTIL_INCLUDED
#define MSUFREND_UTIL_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "const.h"
#include "types.h"
#include "proto.h"
#include <sys/types.h>
#include <time.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <stdarg.h>
#endif

/* Definitions of Windows types, for non-Windows systems. */
#if !defined(WIN32)
#define closesocket(sock) close(sock);
#define ioctlsocket(sock,cmd,argp) ioctl(sock,cmd,argp)
#define SOCKET_ERROR (-1)
#define INVALID_SOCKET (-1)
#define _vsnprintf vsnprintf
#endif

/*====  OS portability section ===============================*/
#ifdef WIN32
/* Windows and Linux use different signed vs. unsigned for the size of sockaddr. */
typedef int TypSockAddrLen;
#else
#define SOCKET int
typedef socklen_t TypSockAddrLen;
#endif

/* I don't want to use "bool" because it is a keyword to C++. */
typedef int TypBool;
#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

int GetLastOSError();
int GetLastSocketError();

/*=====  End of OS portability section =======================*/

/* Debug log levels. If DebugL is >= this, we print errors. */
#define LL_ERROR     10
#define LL_WARNING   20
#define LL_SOME      30
#define LL_MORE      40
#define LL_ALL       50

/* The REQTYPE_* symbols define the type of request being
 *  made by the PP hardware, usually a Function or I/O request. */
/*//! These are no longer needed.  */
#define REQTYPE_ACN 'a'
#define REQTYPE_DCN 'd'
#define REQTYPE_FCN 'f'
#define REQTYPE_IO  'x'

/*
**  Function codes.
*/
#define FcFEFSEL    02400 /* SELECT 6000 CHANNEL ADAPTER    */
#define FcFEFDES    02410 /* DESELECT 6000 CHANNEL ADAPTER  */
#define FcFEFST     00000 /* READ 6CA STATUS                */
#define FcFEFSAU    01000 /* SET ADDRESS (UPPER)            */
#define FcFEFSAM    01400 /* SET ADDRESS (MIDDLE)           */
#define FcFEFHL     03000 /* HALT-LOAD THE 7/32             */
#define FcFEFINT    03400 /* INTERRUPT THE 7/32             */
#define FcFEFLP     06000 /* LOAD INTERFACE MEMORY          */
#define FcFEFRM     04400 /* READ                           */
#define FcFEFWM0    07000 /* WRITE MODE 0                   */
#define FcFEFWM     07400 /* WRITE MODE 1                   */
#define FcFEFRSM    05000 /* READ AND SET                   */
#define FcFEFCI     00400 /* CLEAR INITIALIZED STATUS BIT   */

/*
**  Commands from 1FP to FREND.
*/
#define FC_ITOOK   1  
#define FC_HI80    2  
#define FC_HI240   3  
#define FC_CPOP    4  
#define FC_CPGON   5  

/*
** FREND 6000 Channel Adapter bits, for function FcFEFST
*/
#define FCA_STATUS_INITIALIZED         04000
#define FCA_STATUS_NON_EXIST_MEM       02000
#define FCA_STATUS_LAST_BYTE_NO_ERR    00000
#define FCA_STATUS_LAST_BYTE_PAR_ERR   00400
#define FCA_STATUS_LAST_BYTE_MEM_MAL   01000
#define FCA_STATUS_LAST_BYTE_NON_EXIST 01400
#define FCA_STATUS_MODE_WHEN_ERROR     00200
#define FCA_STATUS_READ_WHEN_ERROR     00100
#define FCA_STATUS_WRITE_WHEN_ERROR    00040
#define FCA_STATUS_HALT_LOADING        00020
#define FCA_STATUS_INT_PENDING         00010

/* Interdata 7/32 types */
typedef unsigned int FrendAddr;
typedef unsigned int FullWord;
typedef unsigned short int HalfWord;
typedef unsigned char Byte8;

/* It appears that the Cyber never tries to access memory beyond this. */
#define MAX_FREND_BYTES 0xc0000

/* Macro for the current FREND address set in the 6000 Channel Adapter */
#define CUR_BYTE_ADDR() pFrendInt->FrendState.fr_addr

/* TypFRENDState holds the state of FREND.  It mostly contains 
 * the memory of the machine. */
typedef struct struct_frend_state {
   Byte8 fr_mem[MAX_FREND_BYTES];  /* Contents of FREND memory, in bytes.
                                    * The 7/32 stored in most-significant-byte-first format
                                    * (SPARC format), and so do we. */
   unsigned int  fr_addr;          /* Next byte (not halfword) address to read or write.
                                    * However, when this is set via the 6CA, the bottom
                                    * bit is cleared, because the memory interface between
                                    * FREND and the Cyber specifies addresses halfword addresses */
   bool fr_next_is_second;         /* true if the next byte of I/O is the second in a sequence.
                                    * This is used for READ-AND-SET, which transfers 2 bytes
                                    * but which is not supposed to change the address register. */
   PpWord fr_last_func_code;       /* Last function code sent from PP. */
} TypFRENDState;

typedef struct struct_cyber_to_frend {
   char     cf_reqtype;  /* REQTYPE_xxx */
   char     cf_ignore;  /* for alignment */
   PpWord   cf_func;    /* Function code, if request is a function */
} TypCyberToFrend;

typedef struct struct_frend_int {
	TypBool  sfi_bSendReplyToCyber;	/* TRUE if Cyber waits for reply from each interrupt to frend2. */
   TypFRENDState  FrendState;
   TypCyberToFrend   cf;
} TypFrendInterface;

/* Name of mapping used for memory shared between FREND and DtCyber. */
#define MAP_FREND_INT "FRENDINT"

/* Names of Windows events used to communicate between FREND and DtCyber. */
#define EVENT_FREND_TO_CYBER "EventFrendToCyber"
/* The use of EVENT_CYBER_TO_FREND was replaced by a short UDP send. */
/* #define EVENT_CYBER_TO_FREND "EventCyberToFrend" */
#define PIPE_FREND_TO_CYBER "pipefrend2cyber"

/* IP port number on which FREND listens for datagrams from DtCyber */
/* 6732 = 6000 + 732 */
#define PORT_FREND_LISTEN  6732

void InitLog(const char *szFilename, const char *szTag);
void SetMaxLogMessages(long maxMessages);
void LogOut(const char *fmt, ...);
#if defined(WIN32)
void *
FrendMapMemoryWindows(char *lpMappingName, DWORD dwNumberOfBytesToMap);
#else
void *
FrendMapMemoryLinux(char *lpMappingName, int NumberOfBytesToMap);
#endif
int InitFRENDInterface(TypBool bThisIsFREND);
int CreateSockToFrend();
int SendToFrend(Byte8 *buf, int len);
int GetLastSocketError();

int InitWaitForFrend();
void WaitForFrend();
int InitReplyToCyber();
void ReplyToCyber();

#endif
