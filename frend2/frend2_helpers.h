/* frend2_helpers.h - Helper utilities used only by frend2.
 * More utilities are in in ..\DtCyber\msufrend_util.c.
 * Mark Riordan  23 June 2004 and 27 Feb 2005
 */
#ifndef MSUFRENDINT_INCLUDED
#define MSUFRENDINT_INCLUDED

#include "const.h"
#include "types.h"
#include "lmbi.h"


/*====  TCP and UDP section  ====================================== */

/* This enum is used in the telnet server FSA.  We don't do
 * sophisticated telnet processing--but we need to do enough
 * to ignore the garbage that telnet clients send.
 */
typedef enum enum_telnet_state
    {
    TELST_NORMAL, TELST_GOT_IAC, TELST_GOT_WILL_OR_SIMILAR
    } TypTelnetState;

#define TELCODE_IAC                      0xff
#define TELCODE_DONT                     0xfe
#define TELCODE_DO                       0xfd
#define TELCODE_WONT                     0xfc
#define TELCODE_WILL                     0xfb

/* Telnet options.  Unfortunately, these are spread out over many RFCs. */
#define TELCODE_OPT_ECHO                 0x01
#define TELCODE_OPT_SUPPRESS_GO_AHEAD    0x03

/* This structure holds a line that is waiting to be sent to
 * the terminal.  If all TCP sends were guaranteed to be successful,
 * then we would not need this.  However, we must keep our sockets
 * in non-blocking mode to prevent lockups. */
typedef struct struct_pending_buffer
    {
    Byte8 spb_buf[L_LINE + 16];  /* Waiting chars */
    int   spb_first;             /* Index of first char still pending */
    int   spb_chars_left;        /* # of chars remaining in buffer */
    } TypPendingBuffer;

/* This structure holds information for a TCP socket, not to be
* confused with a FREND socket.  This is specific to frend2. */
typedef struct struct_sock_tcp
    {
    SOCKET           stcp_socket;       /* TCP socket */
    TypTelnetState   stcp_telnet_state; /* telnet state */
    TypPendingBuffer stcp_buf;          /* chars pending output.  Normally, this is 0 */
                                        /* except when assembling bytes to be sent. */
                                        /* If non-zero, then we shouldn't send any */
                                        /* more lines until this buffer is sent. */
    } TypSockTCP;

#define MAX_TCP_SOCKETS    10

int TCPSend(SOCKET sock, unsigned char *buf, int len);
int CreateSockToFrend();
int SendToFrend(Byte8 *buf, int len);

int InitSockFromCyber();
int ReadSocketFromCyber(Byte8 *buf, int len);

int InitSockTCPListen();

char *FuncCodeToDesc(PpWord funccode);
char *CmdToDesc(int cmd);

#endif
