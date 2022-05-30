/*--- msufrend_util.c -- functions for interfacing with frend2.
 *  This file is compiled into both DtCyber and frend2.
 *
 *  Mark Riordan  June 2004 & Feb 2005
 */

#include "msufrend_util.h"
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */
#include <string.h>

TypFrendInterface *pFrendInt = NULL;
static SOCKET sockToFrend=0; /* Used only by DtCyber */
static struct sockaddr_in sockaddr_to_frend;  /* Used only by DtCyber */

#if defined(WIN32)
HANDLE hEventFrendToCyber = NULL;
#else
static SOCKET sockFromFrend=0;  /* DtCyber reads from this to get response from frend2 */
static SOCKET sockReplyToCyber=0;  /* Used only by frend2 */
static struct sockaddr_un sockaddr_to_cyber;  /* Used only by frend2 to reply to DtCyber. */
#endif

/*=====  OS portability section ==============================*/

/*--- function GetLastOSError ----------------------
 *  Return the last error on a system call.
 *  Intended as a platform-agnostic wrapper to GetLastError() or whatever.
 */
int GetLastOSError()
{
#ifdef WIN32
   return GetLastError();
#else
   return errno;
#endif
}

/*--- function GetLastSocketError ---------------------
 *  Socket-specific version of GetLastOSError().
 */
int GetLastSocketError()
{
#ifdef WIN32
   return WSAGetLastError();
#else
   return errno;
#endif
}

/*=====  End of OS portability section =======================*/

/*=====  Logging variables and functions  ====================*/
/* These are used by both DtCyber and frend2.
 * I don't really need these in DtCyber anymore, but I'll leave
 * them in just in case.  */
      
FILE *fileLog=NULL;
char szLogTag[16];
/* For performance reasons, we don't log any more messages than this. */
static long nMaxMessages = 64000;   /* Can be overriden by SetMaxLogMessages */
static long nMessages = 0;

/*--- function InitLog ----------------------
 *  Open the debug log file.
 */
void InitLog(const char *szFilename, const char *szTag)
{
   fileLog = fopen(szFilename, "wb");
   strncpy(szLogTag, szTag, sizeof(szLogTag));
   szLogTag[sizeof(szLogTag)-1] = '\0';
   LogOut("FREND log initialized.");
}

/*--- function SetMaxLogMessages -------------------
 *  Set the max # of lines that can be written to the debug log.
 */
void SetMaxLogMessages(long maxMessages)
{
   nMaxMessages = maxMessages;
}

/*--- function LogOut ----------------------
 *  Write a line to the debug log, with print-style arguments.
 */
void LogOut(const char *fmt, ...)
{
   if(nMessages++ < nMaxMessages) {
      char msgtext[1024];
      int len, j;
      va_list args;
#ifdef WIN32
      /* Start the line with message number and time, with centisecond resolution */
      SYSTEMTIME localtime;
      GetLocalTime(&localtime);
      len = sprintf(msgtext, "%5d ", nMessages);
      len += sprintf(msgtext+len,"%4u-%-2.2u-%-2.2u %-2.2u:%-2.2u:%-2.2u.%-2.2u ",
         localtime.wYear, localtime.wMonth,  localtime.wDay, 
         localtime.wHour, localtime.wMinute, localtime.wSecond, 
         localtime.wMilliseconds/10);
#else
      /* Start the line with message number and time, with second resolution */
      time_t mytime = time(NULL);
      struct tm *mytm = localtime(&mytime);
      len = sprintf(msgtext, "%5ld ", nMessages);
      len += strftime(msgtext+len, sizeof(msgtext), "%Y-%m-%d %H:%M:%S ", mytm);
#endif
      for(j=0; szLogTag[j]; j++) msgtext[len++] = szLogTag[j];
      msgtext[len++] = ':';
      msgtext[len++] = ' ' ;

      va_start(args, fmt);
      len += _vsnprintf(msgtext+len, sizeof(msgtext)-3-len, fmt, args);
      va_end(args);
      msgtext[len++] = '\r';
      msgtext[len++] = '\n';
      msgtext[len] = '\0';
      if(fileLog) {
         fputs(msgtext, fileLog);
         fflush(fileLog);
      }
      /* OutputDebugString(msgtext); */
   }
}

/*=====  End of logging variables and functions        ===== */

/*=====  Beginning of shared memory routines           ===== */

#if defined(WIN32)
/*--- function CreateNullSecurity ---------------------------
 *  Create a permissive Windows security descriptor so we can
 *  share memory between DtCyber and frend2.
 */
int CreateNullSecurity(SECURITY_ATTRIBUTES *psa)
{
   int retval=0;
   PSECURITY_DESCRIPTOR    pSD;
 
   pSD = (PSECURITY_DESCRIPTOR) LocalAlloc( LPTR,
                  SECURITY_DESCRIPTOR_MIN_LENGTH);

   if (pSD == NULL){
      retval=2;
   } else if (!InitializeSecurityDescriptor(pSD, SECURITY_DESCRIPTOR_REVISION))  {
      retval=3;
   } else 
   /* Add a NULL DACL to the security descriptor. */
   if (!SetSecurityDescriptorDacl(pSD, TRUE, (PACL) NULL, FALSE))
   {
      retval=4;
   }

   psa->nLength = sizeof(*psa);
   psa->lpSecurityDescriptor = pSD;
   psa->bInheritHandle = TRUE;

   return retval;
}

/*--- function FrendMapMemoryWindows ---------------------------------------------
 *
 *  Map some memory from the paging file for use as shared memory.
 *
 *  Entry:     lpMappingName        is the name by which processes will
 *                                  refer to this region
 *             dwNumberOfBytesToMap is the amount of memory we want.
 *
 *  Exit:      Returns the process-local address of the mapped region,
 *             else NULL if failure.
 */
void *
FrendMapMemoryWindows(char *lpMappingName, DWORD dwNumberOfBytesToMap)
{
   LPVOID addr=0;
   HANDLE hFileMappingObject;
   HANDLE hMappingFile = (HANDLE) -1;  // was 0xffffffff; changed for 64-bit compatibility.
   DWORD flProtect = PAGE_READWRITE;
   DWORD dwMaximumSizeHigh = 0, dwMaximumSizeLow= dwNumberOfBytesToMap;

   DWORD dwDesiredAccess = FILE_MAP_ALL_ACCESS;	/* access mode */
   DWORD dwFileOffsetHigh=0;	/* high-order 32 bits of file offset */
   DWORD dwFileOffsetLow=0;	/* low-order 32 bits of file offset */
   DWORD dwGetVer;
   LPSECURITY_ATTRIBUTES lpFileMappingAttributes;

   /* Create permissive security attributes */
   SECURITY_ATTRIBUTES     sa;
   CreateNullSecurity(&sa);
   lpFileMappingAttributes = &sa;
   dwGetVer = GetVersion();
   /* Windows 95 gets no security attributes */
   if ((dwGetVer & 0x80000000)) lpFileMappingAttributes = NULL;

   hFileMappingObject = CreateFileMapping(
     hMappingFile,	/* handle to file to map */
     lpFileMappingAttributes,	/* optional security attributes */
     flProtect,	/* protection for mapping object */
     dwMaximumSizeHigh,	/* high-order 32 bits of object size */
     dwMaximumSizeLow,	/* low-order 32 bits of object size */
     lpMappingName 	/* name of file-mapping object */
   );

   if(!hFileMappingObject) {
      LogOut("FrendMapMemoryWindows: can't create mapping");
   } else {
      if(ERROR_ALREADY_EXISTS == GetLastError()) {
         LogOut("FrendMapMemoryWindows: file mapping already exists.  This is probably OK.");
      } else {
         LogOut("FrendMapMemoryWindows: creating new mapping.");
      }

      /* Now that the mapping has been created, we must commit the
      /* mapping to our address space. */
      
      addr = (void *)MapViewOfFile(
       hFileMappingObject,	/* file-mapping object to map into address space */
       dwDesiredAccess,	/* access mode */
       dwFileOffsetHigh,	/* high-order 32 bits of file offset */
       dwFileOffsetLow,	/* low-order 32 bits of file offset */
       dwNumberOfBytesToMap 	/* number of bytes to map */
      );
      if(!addr) {
         LogOut("FrendMapMemoryWindows: couldn't MapViewOfFile");
      }
   }
   return addr;
}
#else
/*--- function FrendMapMemoryLinux ---------------------
 *
 *  Map some memory for use as shared memory in Linux.
 *
 *  Entry:     lpMappingName        is the name by which processes will
 *                                  refer to this region.  
 *             NumberOfBytesToMap	is the amount of memory we want.
 *
 *  Exit:      Returns the process-local address of the mapped region,
 *             else NULL if failure.
 */
void *
FrendMapMemoryLinux(char *lpMappingName, int NumberOfBytesToMap)
{
	void *pStart=NULL;
   int sharedsize = NumberOfBytesToMap;
	do { /* not a loop */
      off_t offset=0;
      int oflag = O_RDWR;
      mode_t mode = S_IRWXU | S_IRWXG;
      int handle = shm_open(lpMappingName, oflag , mode);
		TypBool bMappingNeededToBeCreated=FALSE;
      if(-1 == handle) {
			/* Mapping file could not be opened.  Try creating it. */
			oflag |= O_CREAT;
			handle = shm_open(lpMappingName, oflag , mode);
			if(-1 == handle) {
				/* error */
				LogOut("shm_open: error %d", errno);
				perror("shm_open");
				break;
			}
			bMappingNeededToBeCreated = TRUE;
      }

      /* Give the shared object a non-zero size */
      if(ftruncate(handle, sharedsize)) {
			LogOut("ftruncate: error %d", errno);
         perror("ftruncate");
         break;
      }
      /* FreeBSD recommends writing zeros to the ftruncated file
       * to avoid fragmentation. */
      if(bMappingNeededToBeCreated) {
         char *pzeros = (char *)malloc(sharedsize);
         memset(pzeros, 0, sharedsize);
         if (write(handle, pzeros, sharedsize) == -1)
	   perror("FrendMapMemoryLinux(): shared memory write failed");
	 break;
      }
		/* We don't use MAP_ANONYMOUS, or MAP_NOSYNC,
		 * because they are not supported in Linux (though they are in BSD). */
		/* It's said that you can use /dev/zero instead of MAP_ANONYMOUS, */
		/* but I couldn't get that to work. */
		/* Thus, shared memory in Linux will probably be a little less */
		/* efficient than it is in Windows. */
      pStart = mmap(pStart, sharedsize,
        PROT_READ|PROT_WRITE, MAP_SHARED /*| MAP_NOSYNC*/, handle, offset);
	} while(FALSE);
	return pStart;
}
#endif

/*--- function InitFRENDInterface ----------------------
 *  Initialize the interface between FREND and DtCyber.
 *  Entry:  bThisIsFREND   is TRUE if this call is being made
 *                         from frend2.  (DtCyber also calls this.)
 *									This tells us whether to clear memory.
 *  Exit:   Returns 0 if OK, else error code.
 */
int InitFRENDInterface(TypBool bThisIsFREND)
{
   int retcode = 0;
#if defined(WIN32)
   pFrendInt = FrendMapMemoryWindows(MAP_FREND_INT, sizeof(TypFrendInterface));
#else
   pFrendInt = FrendMapMemoryLinux(MAP_FREND_INT, sizeof(TypFrendInterface));
#endif
   if(!pFrendInt) {
      LogOut("Cannot map memory.");
      return 2;
   }
   if(bThisIsFREND) {
      /* initialize to zeros */
      memset(pFrendInt,0,sizeof(TypFrendInterface));
   }
	/* hEventCyberToFrend is no longer used; we use UDP instead.
    * hEventCyberToFrend = CreateEvent(NULL, FALSE, FALSE, EVENT_CYBER_TO_FREND);
    * if(NULL==hEventCyberToFrend) return 3;
	 */
   return retcode;
}

/*=====  End of shared memory routines   ==================== */

/*=====  Routines called only by DtCyber  ====================*/

/*--- function CreateSockToFrend ----------------------
 *  Create the socket used by DtCyber to send interrupts to frend2.
 *  Exit:   Returns 0 if OK.
 */
int CreateSockToFrend()
{
   int retval=0;
   sockToFrend = socket(PF_INET,SOCK_DGRAM,0);
   do {
      if(INVALID_SOCKET == sockToFrend) {
         retval = 4;
         break;
      }
      sockaddr_to_frend.sin_addr.s_addr = inet_addr("127.0.0.1");
	   sockaddr_to_frend.sin_family = AF_INET;
	   sockaddr_to_frend.sin_port = htons(PORT_FREND_LISTEN);
   } while(FALSE);
   if(retval) LogOut("==**CreateSockToFrend got error %d", retval);
   return retval;
}

/*--- function SendToFrend ----------------------
 *  Send a message (always an "interrupt") to frend2.
 *  Exit:   Returns 0 if OK.
 */
int SendToFrend(Byte8 *buf, int len)
{
   int retval = sendto(sockToFrend, (char *)buf, len, 0, (struct sockaddr *)&sockaddr_to_frend, sizeof(sockaddr_to_frend));
   if(retval==SOCKET_ERROR) {
      retval = GetLastSocketError();
   } else if(retval == len) {
      retval = 0;
   } else {
      retval = 5;
   }
   if(retval) LogOut("==**SendToFrend got error %d", retval);
   return retval;
}

/*--- function InitWaitForFrend ---------------------
 *  Initialize the variables used to respond from frend2 to dtcyber.
 *  This is optional and is used only if frend2's "-s" command line parameter
 *  is used.  Called only by DtCyber.
 */
int InitWaitForFrend()
{
	int retval=0;
#if defined(WIN32)
   hEventFrendToCyber = CreateEvent(NULL, FALSE, FALSE, EVENT_FREND_TO_CYBER);   
   if(NULL==hEventFrendToCyber) retval = 3;
#else
	/* Listen on a UNIX-mode pipe. */
	int err;
	static struct sockaddr_un addr_from_cyber;
	sockFromFrend = socket(PF_UNIX, SOCK_DGRAM, 0);
	memset(&addr_from_cyber, 0, sizeof(addr_from_cyber));
	addr_from_cyber.sun_family = AF_UNIX;
	strncpy(addr_from_cyber.sun_path+1, PIPE_FREND_TO_CYBER, sizeof(addr_from_cyber.sun_path)-1);
	err = bind(sockFromFrend, (struct sockaddr *)&addr_from_cyber, sizeof(addr_from_cyber));
	if(-1 == err) {
		perror("bind");
		LogOut("InitWaitForFrend: Cannot bind: error %d", errno);
		retval = 3;
	}
#endif
	return retval;
}

/*--- function WaitForFrend ---------------------
 *  Wait for the response from FREND.  This is optional and is used
 *  to increase interactive responsiveness, especially on a single-CPU host.
 */
void WaitForFrend()
{
#if defined(WIN32)
   DWORD dwRetcode = WaitForSingleObject(hEventFrendToCyber, 5000);
   if(WAIT_TIMEOUT == dwRetcode) {
      LogOut("FREND did not respond");
   }
#else
	char bufrecv[4];
	int nbytes = recv(sockFromFrend, bufrecv, sizeof(bufrecv), 0);
	if(-1 == nbytes) {
		perror("WaitForFrend recv");
		LogOut("WaitForFrend got recv error %d", errno);
	}
#endif
}

/*=====  End of routines used only by DtCyber  ===============*/

/*=====  Beginning of routines used only by frend2  ==========*/

/*--- function InitReplyToCyber ---------------------
 *  Called only by frend2.
 */
int InitReplyToCyber()
{
	int retval=0;
#if defined(WIN32)
   hEventFrendToCyber = CreateEvent(NULL, FALSE, FALSE, EVENT_FREND_TO_CYBER);   
   if(NULL==hEventFrendToCyber) return 3;
#else
	int err;

	sockReplyToCyber = socket(PF_UNIX, SOCK_DGRAM, 0);
	sockaddr_to_cyber.sun_family = AF_UNIX;
	memset(sockaddr_to_cyber.sun_path, 0, sizeof(sockaddr_to_cyber.sun_path));
	strncpy(sockaddr_to_cyber.sun_path+1, PIPE_FREND_TO_CYBER, sizeof(sockaddr_to_cyber.sun_path)-1);
	do {
		err = connect(sockReplyToCyber, (struct sockaddr *)&sockaddr_to_cyber, sizeof(sockaddr_to_cyber));
		if(-1 == err) {
			printf("Waiting for other dtcyber...\n");
			usleep(1500000);
		}
	} while(-1 == err);
#endif
	return retval;
}

/*--- function ReplyToCyber ---------------------
 *  Reply to DtCyber, saying that frend2 has processed the interrupt.
 *  Call to this is optional.  This is called only by frend2.
 */
void ReplyToCyber()
{
#if defined(WIN32)
   SetEvent(hEventFrendToCyber);
#else
	int len=1;
   int retval = sendto(sockReplyToCyber, "r", len, 0, (struct sockaddr *)&sockaddr_to_cyber, sizeof(sockaddr_to_cyber));
   if(retval==SOCKET_ERROR) {
      retval = GetLastSocketError();
   } else if(retval == len) {
      retval = 0;
   } else {
      retval = 5;
   }
   if(retval) LogOut("==**ReplyToCyber got error %d", retval);
#endif
}

/*=====  End of routines used only by frend2  ================*/
