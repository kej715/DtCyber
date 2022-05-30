/*--- frend2_helpers.c - Utilities used internally by frend2.
 * The routines for interfacing with DtCyber are in msufrend2_util.c.
 *
 * Mark Riordan   24 June 2004
 * Placed in the public domain.
 */
#include <string.h>
#include "stdafx.h"
#include "msufrend_util.h"
#include "frend2_helpers.h"

SOCKET sockFromCyber=0;
extern unsigned short int TCPListenPort; /* We listen for terminal connections on this TCP port */
SOCKET sockTCPListen=0; /* The socket on which we listen for terminal connections */
extern TypSockTCP SockTCPAry[MAX_TCP_SOCKETS];

/*--- function InitSockFromCyber ----------------------
 *  Initialize the socket that frend2 will use to get 
 *  "interrupts" from DtCyber.
 *  Exit:   Returns 0 if OK.
 */
int InitSockFromCyber()
{
   int err;
   static struct sockaddr_in sockaddr_from_cyber;

   do {
#ifdef WIN32
      WSADATA WSAData;
      if (WSAStartup(MAKEWORD(1,1), &WSAData) != 0) {
		   err = WSAGetLastError();
         break;
	   }
#endif
      sockFromCyber = socket(AF_INET, SOCK_DGRAM, 0);

      /* Bind the socket to the port to listen on. */
      memset(&sockaddr_from_cyber,0,sizeof(sockaddr_from_cyber));
      sockaddr_from_cyber.sin_family = AF_INET;
      sockaddr_from_cyber.sin_port = htons(PORT_FREND_LISTEN);
      sockaddr_from_cyber.sin_addr.s_addr = inet_addr("127.0.0.1"); /* INADDR_ANY; */
      err = bind(sockFromCyber, (struct sockaddr *) & sockaddr_from_cyber, sizeof(sockaddr_from_cyber));
      if (err == SOCKET_ERROR) {
         err = GetLastSocketError();
      }
   } while(FALSE);
   if(err) LogOut("==** InitSockFromCyber returned %d", err);
   return err;
}

/*--- function ReadSocketFromCyber ----------------------
 *  Read a UDP packet from DtCyber.
 *  Call this routine only if you are sure (from select()) that
 *  data is ready.
 *  Exit:   Returns the number of bytes read, or 0 if connection closed
 *             cleanly, or SOCKET_ERROR if error.
 *          buf may contain data read from the socket.
 */
int ReadSocketFromCyber(unsigned char *buf, int len)
{
   struct sockaddr_in addr_client;
   TypSockAddrLen size_sockaddr = sizeof(addr_client);
   int nbytes = recvfrom(sockFromCyber, (char *)buf, len, 0, (struct sockaddr *)&addr_client, &size_sockaddr);
   return nbytes;
}

/*--- function InitSockTCPListen ----------------------
 */
int InitSockTCPListen()
{
   int err;
   static struct sockaddr_in sockaddr_listen_terminal;

   memset(SockTCPAry, 0, sizeof(SockTCPAry));

   do {
      sockTCPListen = socket(AF_INET, SOCK_STREAM, 0);

      /* Bind the socket to the port to listen on. */
      memset(&sockaddr_listen_terminal,0,sizeof(sockaddr_listen_terminal));
      sockaddr_listen_terminal.sin_family = AF_INET;
      sockaddr_listen_terminal.sin_port = htons(TCPListenPort);
      sockaddr_listen_terminal.sin_addr.s_addr = INADDR_ANY;
      err = bind(sockTCPListen, (struct sockaddr *) & sockaddr_listen_terminal, sizeof(sockaddr_listen_terminal));
      if (err == SOCKET_ERROR) {
         err = GetLastSocketError();
      } else {
         err = listen(sockTCPListen, 5);
         if (err == SOCKET_ERROR) {
            err = GetLastSocketError();
         } else {
            // We are now listening successfully.
         }         
      }
   } while(FALSE);
   if(err) LogOut("==** InitSockTCPListen returned %d", err);
   return err;
}

int TCPSend(SOCKET sock, unsigned char *buf, int len)
{
   return send(sock, buf, len, 0);
}

typedef struct struct_func_code_to_desc {
   int   fcd_funccode;
   char *fcd_desc;
} TypCodeToDesc;


/*--- function AnyCodeToDesc ----------------------
 *  Get an English description of an integer code.
 *  Entry:  funccode is a numeric code.
 *          pTable   is a TypCodeToDesc table mapping codes to descriptions.
 *
 *  Exit:   Returns a pointer to a zero-terminated function description.
 */
char *AnyCodeToDesc(int funccode, TypCodeToDesc *pTable)
{
   int j;
   char *pszDesc = "Unknown code";
   for(j=0; pTable[j].fcd_funccode != 077777; j++) {
      if(pTable[j].fcd_funccode == funccode) {
         pszDesc = pTable[j].fcd_desc;
         break;
      }
   }
   return pszDesc;
}

/*--- function FuncCodeToDesc ----------------------
 *  Get an English description of a FREND channel function (PP to FREND).
 *  Entry:  funccode is a 12-bit function code from a FAN instruction.
 *
 *  Exit:   Returns a pointer to a zero-terminated function description.
 */
char *FuncCodeToDesc(PpWord funccode)
{
   static TypCodeToDesc FuncCodeToDesc[] = {
      {FcFEFSEL, "FEFSEL - SELECT 6000 CA  "},
      {FcFEFDES, "FEFDES - DESELECT 6000 CA"},
      {FcFEFST,  "FEFST  - READ 6CA STATUS "},
      {FcFEFSAU, "FEFSAU - SET ADDR (UPPER)"},
      {FcFEFSAM, "FEFSAM - SET ADDR (MID)  "},
      {FcFEFHL,  "FEFHL  - HALT-LOAD       "},
      {FcFEFINT, "FEFINT - INTERRUPT       "},
      {FcFEFLP,  "FEFLP  - LOAD INTERF MEM "},
      {FcFEFRM,  "FEFRM  - READ            "},
      {FcFEFWM0, "FEFWM0 - WRITE MODE 0    "},
      {FcFEFWM,  "FEFWM  - WRITE MODE 1    "},
      {FcFEFRSM, "FEFRSM - READ AND SET    "},
      {FcFEFCI,  "FEFCI  - CLR INI STA BIT "},
      {077777, ""}
   };
   return AnyCodeToDesc(funccode, FuncCodeToDesc);
}
/*--- function CmdToDesc ----------------------
 *  Get an English description of a 1FP-to-FREND command.
 *  Entry:  cmd      has a value corresponding to one of the FC.* symbols
 *                   in FREND.
 *  Exit:   Returns a pointer to a description.
 */
char *CmdToDesc(int cmd)
{
   static TypCodeToDesc CmdToDesc[] = {
      {FC_ITOOK, "ITOOK"},
      {FC_HI80,  "HEREIS 80"},
      {FC_HI240, "HEREIS 240"},
      {FC_CPOP,  "CONTROL PORT OPEN"},
      {FC_CPGON, "CONTROL PORT Gone"},
      {077777, ""}
   };
   return AnyCodeToDesc(cmd, CmdToDesc);
}
