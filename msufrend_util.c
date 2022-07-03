/*--- msufrend_util.c -- functions for interfacing with frend2.
 *  This file is compiled into both DtCyber and frend2.
 *
 *  Mark Riordan  June 2004 & Feb 2005
 */

#include "msufrend_util.h"
#include <fcntl.h>           /* For O_* constants */
#include <string.h>
#include <sys/stat.h>        /* For mode constants */

TypFrendInterface         *pFrendInt  = NULL;
static SOCKET             sockToFrend = 0; /* Used only by DtCyber */
static struct sockaddr_in sockAddrToFrend; /* Used only by DtCyber */

#if defined(WIN32)
HANDLE hEventFrendToCyber = NULL;
#else
static SOCKET             sockFromFrend    = 0; /* DtCyber reads from this to get response from frend2 */
static SOCKET             sockReplyToCyber = 0; /* Used only by frend2 */
static struct sockaddr_un sockAddrToCyber;      /* Used only by frend2 to reply to DtCyber. */
#endif

/*=====  OS portability section ==============================*/

/*--- function msuFrendGetLastOSError ----------------------
 *  Return the last error on a system call.
 *  Intended as a platform-agnostic wrapper to GetLastError() or whatever.
 */
int msuFrendGetLastOSError()
    {
#ifdef WIN32
    return GetLastError();
#else
    return errno;
#endif
    }

/*--- function msuFrendGetLastSocketError ---------------------
 *  Socket-specific version of msuFrendGetLastOSError().
 */
int msuFrendGetLastSocketError()
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

FILE *fileLog = NULL;
char szLogTag[16];
/* For performance reasons, we don't log any more messages than this. */
static long nMaxMessages = 64000;   /* Can be overriden by msuFrendSetMaxLogMessages */
static long nMessages    = 0;

/*--- function msuFrendInitLog ----------------------
 *  Open the debug log file.
 */
void msuFrendInitLog(const char *szFilename, const char *szTag)
    {
    fileLog = fopen(szFilename, "wb");
    strncpy(szLogTag, szTag, sizeof(szLogTag));
    szLogTag[sizeof(szLogTag) - 1] = '\0';
    msuFrendLog("FREND log initialized.");
    }

/*--- function msuFrendSetMaxLogMessages -------------------
 *  Set the max # of lines that can be written to the debug log.
 */
void msuFrendSetMaxLogMessages(long maxMessages)
    {
    nMaxMessages = maxMessages;
    }

/*--- function msuFrendLog ----------------------
 *  Write a line to the debug log, with print-style arguments.
 */
void msuFrendLog(const char *fmt, ...)
    {
    if (nMessages++ < nMaxMessages)
        {
        char    msgtext[1024];
        int     len, j;
        va_list args;
#ifdef WIN32
        /* Start the line with message number and time, with centisecond resolution */
        SYSTEMTIME localtime;
        GetLocalTime(&localtime);
        len  = sprintf(msgtext, "%5d ", nMessages);
        len += sprintf(msgtext + len, "%4u-%-2.2u-%-2.2u %-2.2u:%-2.2u:%-2.2u.%-2.2u ",
                       localtime.wYear, localtime.wMonth, localtime.wDay,
                       localtime.wHour, localtime.wMinute, localtime.wSecond,
                       localtime.wMilliseconds / 10);
#else
        /* Start the line with message number and time, with second resolution */
        time_t    mytime = time(NULL);
        struct tm *mytm  = localtime(&mytime);
        len  = sprintf(msgtext, "%5ld ", nMessages);
        len += strftime(msgtext + len, sizeof(msgtext), "%Y-%m-%d %H:%M:%S ", mytm);
#endif
        for (j = 0; szLogTag[j]; j++)
            {
            msgtext[len++] = szLogTag[j];
            }
        msgtext[len++] = ':';
        msgtext[len++] = ' ';

        va_start(args, fmt);
        len += _vsnprintf(msgtext + len, sizeof(msgtext) - 3 - len, fmt, args);
        va_end(args);
        msgtext[len++] = '\r';
        msgtext[len++] = '\n';
        msgtext[len]   = '\0';
        if (fileLog)
            {
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
    int retval = 0;
    PSECURITY_DESCRIPTOR pSD;

    pSD = (PSECURITY_DESCRIPTOR)LocalAlloc(LPTR,
                                           SECURITY_DESCRIPTOR_MIN_LENGTH);

    if (pSD == NULL)
        {
        retval = 2;
        }
    else if (!InitializeSecurityDescriptor(pSD, SECURITY_DESCRIPTOR_REVISION))
        {
        retval = 3;
        }
    else
    /* Add a NULL DACL to the security descriptor. */
    if (!SetSecurityDescriptorDacl(pSD, TRUE, (PACL)NULL, FALSE))
        {
        retval = 4;
        }

    psa->nLength = sizeof(*psa);
    psa->lpSecurityDescriptor = pSD;
    psa->bInheritHandle       = TRUE;

    return retval;
    }

/*--- function msuFrendMapMemory ---------------------------------------------
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
void *msuFrendMapMemory(char *lpMappingName, DWORD dwNumberOfBytesToMap)
    {
    LPVOID addr = 0;
    HANDLE hFileMappingObject;
    HANDLE hMappingFile = (HANDLE)-1;  // was 0xffffffff; changed for 64-bit compatibility.
    DWORD  flProtect = PAGE_READWRITE;
    DWORD  dwMaximumSizeHigh = 0, dwMaximumSizeLow = dwNumberOfBytesToMap;

    DWORD                 dwDesiredAccess = FILE_MAP_ALL_ACCESS; /* access mode */
    DWORD                 dwFileOffsetHigh = 0;                  /* high-order 32 bits of file offset */
    DWORD                 dwFileOffsetLow = 0;                   /* low-order 32 bits of file offset */
    LPSECURITY_ATTRIBUTES lpFileMappingAttributes;

    /* Create permissive security attributes */
    SECURITY_ATTRIBUTES sa;

    CreateNullSecurity(&sa);
    lpFileMappingAttributes = &sa;

    hFileMappingObject = CreateFileMapping(
        hMappingFile,            /* handle to file to map */
        lpFileMappingAttributes, /* optional security attributes */
        flProtect,               /* protection for mapping object */
        dwMaximumSizeHigh,       /* high-order 32 bits of object size */
        dwMaximumSizeLow,        /* low-order 32 bits of object size */
        lpMappingName            /* name of file-mapping object */
        );

    if (!hFileMappingObject)
        {
        msuFrendLog("msuFrendMapMemory: can't create mapping");
        }
    else
        {
        if (ERROR_ALREADY_EXISTS == GetLastError())
            {
            msuFrendLog("msuFrendMapMemory: file mapping already exists.  This is probably OK.");
            }
        else
            {
            msuFrendLog("msuFrendMapMemory: creating new mapping.");
            }

        /* Now that the mapping has been created, we must commit the
         * /* mapping to our address space. */

        addr = (void *)MapViewOfFile(
            hFileMappingObject,  /* file-mapping object to map into address space */
            dwDesiredAccess,     /* access mode */
            dwFileOffsetHigh,    /* high-order 32 bits of file offset */
            dwFileOffsetLow,     /* low-order 32 bits of file offset */
            dwNumberOfBytesToMap /* number of bytes to map */
            );
        if (!addr)
            {
            msuFrendLog("msuFrendMapMemory: couldn't MapViewOfFile");
            }
        }

    return addr;
    }

#else

/*--- function msuFrendMapMemory ---------------------
 *
 *  Map some memory for use as shared memory in Linux.
 *
 *  Entry:     lpMappingName        is the name by which processes will
 *                                  refer to this region.
 *             numberOfBytesToMap   is the amount of memory we want.
 *
 *  Exit:      Returns the process-local address of the mapped region,
 *             else NULL if failure.
 */
void *msuFrendMapMemory(char *lpMappingName, int numberOfBytesToMap)
    {
    mode_t mode    = S_IRWXU | S_IRWXG;
    int    oflag   = O_RDWR;
    void   *pStart = NULL;

    do
        { /* not a loop */
        int handle = shm_open(lpMappingName, oflag, mode);
        if (-1 == handle)
            {
            /* Mapping file could not be opened.  Try creating it. */
            oflag |= O_CREAT;
            handle = shm_open(lpMappingName, oflag, mode);
            if (-1 == handle)
                {
                /* error */
                perror("shm_open");
                msuFrendLog("shm_open: error %d", errno);
                break;
                }
            /* Give the shared object a non-zero size */
            if (-1 == ftruncate(handle, numberOfBytesToMap))
                {
                perror("ftruncate");
                msuFrendLog("ftruncate: error %d", errno);
                break;
                }
#if defined(__FreeBSD__)
            /* FreeBSD recommends writing zeros to the ftruncated file
             * to avoid fragmentation. */
            char *pzeros = (char *)malloc(numberOfBytesToMap);
            memset(pzeros, 0, numberOfBytesToMap);
            if (write(handle, pzeros, numberOfBytesToMap) == -1)
                {
                perror("write");
                msuFrendLog("msuFrendMapMemory: shared memory write error %d", errno);
                break;
                }
            free(pzeros);
#endif
            }
        pStart = mmap(NULL, numberOfBytesToMap, PROT_READ | PROT_WRITE, MAP_SHARED, handle, 0);
        if (pStart == MAP_FAILED)
            {
            perror("mmap");
            msuFrendLog("mmap: error %d", errno);
            pStart = NULL;
            break;
            }
        }while (FALSE);

    return pStart;
    }

#endif

/*--- function msuFrendInitInterface ----------------------
 *  Initialize the interface between FREND and DtCyber.
 *  Entry:  isThisFrend   is TRUE if this call is being made
 *                        from frend2.  (DtCyber also calls this.)
 *                        This tells us whether to clear memory.
 *  Exit:   Returns 0 if OK, else error code.
 */
int msuFrendInitInterface(bool isThisFrend)
    {
    pFrendInt = msuFrendMapMemory(MAP_FREND_INT, sizeof(TypFrendInterface));
    if (pFrendInt == NULL)
        {
        msuFrendLog("Cannot map memory.");

        return 2;
        }
    if (isThisFrend)
        {
        /* initialize to zeros */
        memset(pFrendInt, 0, sizeof(TypFrendInterface));
        }

    return 0;
    }

/*=====  End of shared memory routines   ==================== */

/*=====  Routines called only by DtCyber  ====================*/

/*--- function msuFrendCreateSocket ----------------------
 *  Create the socket used by DtCyber to send interrupts to frend2.
 *  Exit:   Returns 0 if OK.
 */
int msuFrendCreateSocket()
    {
    int retval = 0;

    sockToFrend = socket(PF_INET, SOCK_DGRAM, 0);
    do
        {
        if (INVALID_SOCKET == sockToFrend)
            {
            retval = 4;
            break;
            }
        sockAddrToFrend.sin_addr.s_addr = inet_addr("127.0.0.1");
        sockAddrToFrend.sin_family      = AF_INET;
        sockAddrToFrend.sin_port        = htons(PORT_FREND_LISTEN);
        }while (FALSE);
    if (retval)
        {
        msuFrendLog("==**msuFrendCreateSocket got error %d", retval);
        }

    return retval;
    }

/*--- function msuFrendSend ----------------------
 *  Send a message (always an "interrupt") to frend2.
 *  Exit:   Returns 0 if OK.
 */
int msuFrendSend(Byte8 *buf, int len)
    {
    int retval = sendto(sockToFrend, (char *)buf, len, 0, (struct sockaddr *)&sockAddrToFrend, sizeof(sockAddrToFrend));

    if (retval == SOCKET_ERROR)
        {
        retval = msuFrendGetLastSocketError();
        }
    else if (retval == len)
        {
        retval = 0;
        }
    else
        {
        retval = 5;
        }
    if (retval)
        {
        msuFrendLog("==**msuFrendSend got error %d", retval);
        }

    return retval;
    }

/*--- function msuFrendInitWait ---------------------
 *  Initialize the variables used to respond from frend2 to dtcyber.
 *  This is optional and is used only if frend2's "-s" command line parameter
 *  is used.  Called only by DtCyber.
 */
int msuFrendInitWait()
    {
    int retval = 0;

#if defined(WIN32)
    hEventFrendToCyber = CreateEvent(NULL, FALSE, FALSE, EVENT_FREND_TO_CYBER);
    if (NULL == hEventFrendToCyber)
        {
        retval = 3;
        }
#else
    /* Listen on a UNIX-mode pipe. */
    int err;
    static struct sockaddr_un addrFromCyber;
    sockFromFrend = socket(PF_UNIX, SOCK_DGRAM, 0);
    memset(&addrFromCyber, 0, sizeof(addrFromCyber));
    addrFromCyber.sun_family = AF_UNIX;
    strncpy(addrFromCyber.sun_path + 1, PIPE_FREND_TO_CYBER, sizeof(addrFromCyber.sun_path) - 1);
    err = bind(sockFromFrend, (struct sockaddr *)&addrFromCyber, sizeof(addrFromCyber));
    if (-1 == err)
        {
        perror("bind");
        msuFrendLog("msuFrendInitWait: Cannot bind: error %d", errno);
        retval = 3;
        }
#endif

    return retval;
    }

/*--- function msuFrendWait ---------------------
 *  Wait for the response from FREND.  This is optional and is used
 *  to increase interactive responsiveness, especially on a single-CPU host.
 */
void msuFrendWait()
    {
#if defined(WIN32)
    DWORD dwRetcode = WaitForSingleObject(hEventFrendToCyber, 5000);
    if (WAIT_TIMEOUT == dwRetcode)
        {
        msuFrendLog("FREND did not respond");
        }
#else
    char bufrecv[4];
    int  nbytes = recv(sockFromFrend, bufrecv, sizeof(bufrecv), 0);
    if (-1 == nbytes)
        {
        perror("msuFrendWait recv");
        msuFrendLog("msuFrendWait got recv error %d", errno);
        }
#endif
    }

/*=====  End of routines used only by DtCyber  ===============*/

/*=====  Beginning of routines used only by frend2  ==========*/

/*--- function msuFrendInitReplyToCyber ---------------------
 *  Called only by frend2.
 */
int msuFrendInitReplyToCyber()
    {
    int retval = 0;

#if defined(WIN32)
    hEventFrendToCyber = CreateEvent(NULL, FALSE, FALSE, EVENT_FREND_TO_CYBER);
    if (NULL == hEventFrendToCyber)
        {
        return 3;
        }
#else
    int err;

    sockReplyToCyber           = socket(PF_UNIX, SOCK_DGRAM, 0);
    sockAddrToCyber.sun_family = AF_UNIX;
    memset(sockAddrToCyber.sun_path, 0, sizeof(sockAddrToCyber.sun_path));
    strncpy(sockAddrToCyber.sun_path + 1, PIPE_FREND_TO_CYBER, sizeof(sockAddrToCyber.sun_path) - 1);
    do
        {
        err = connect(sockReplyToCyber, (struct sockaddr *)&sockAddrToCyber, sizeof(sockAddrToCyber));
        if (-1 == err)
            {
            printf("Waiting for other dtcyber...\n");
            sleep(2);
            }
        } while (-1 == err);
#endif

    return retval;
    }

/*--- function msuFrendReplyToCyber ---------------------
 *  Reply to DtCyber, saying that frend2 has processed the interrupt.
 *  Call to this is optional.  This is called only by frend2.
 */
void msuFrendReplyToCyber()
    {
#if defined(WIN32)
    SetEvent(hEventFrendToCyber);
#else
    int len    = 1;
    int retval = sendto(sockReplyToCyber, "r", len, 0, (struct sockaddr *)&sockAddrToCyber, sizeof(sockAddrToCyber));
    if (retval == SOCKET_ERROR)
        {
        retval = msuFrendGetLastSocketError();
        }
    else if (retval == len)
        {
        retval = 0;
        }
    else
        {
        retval = 5;
        }
    if (retval)
        {
        msuFrendLog("==**msuFrendReplyToCyber got error %d", retval);
        }
#endif
    }

/*=====  End of routines used only by frend2  ================*/
