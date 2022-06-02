/* frend2.c : Reimplementation of FREND, an interactive front-end
//  to SCOPE/Hustler, written at Michigan State University in
//  the late 1970's.
//
// This version runs as a separate process, and communicates with
// Desktop Cyber (DtCyber) via events and shared memory. 
// DtCyber has direct access to FREND memory, just as in the
// original 6000 Channel Adapter and FREND.
//
// It really helps to have listings of the original FREND and 1FP 
// when you are following this code.  See http://60bits.net
// As I wrote this code, I made it more and more like FREND.
// Originally, I tried to take shortcuts and implement a simpler
// structure--but that's the hard way if you want to eventually 
// get most things working.  Now even the routine names are
// largely borrowed from FREND.
//
// frend2 source is maintained via CVS (Concurrent Versions System) at:
// http://sourceforge.net/projects/frend2
//
// Symbol names are mostly taken from FREND, with '_' substituted
// for '.'.
// In comments, FWA means "First Word Address".  It means the 
// address of the first byte of a structure.
//
// Use frend2's -s command line parameter if you want DtCyber 
// to wait for frend2 to process an interrupt
// before returning from processing a channel function that causes
// an interrupt to be sent to frend2.  This improves responsiveness
// on single-CPU systems.  I expect it slows overall performance
// on a multiple-CPU system, but I haven't tested that yet.
//
// Tabs are set every 3 columns.
// Lines containing "//!" are notes to myself about places where I took shortcuts.
//
// Mark Riordan  23 June 2004
// Version 61.00, moving the 6CA logic into DtCyber and putting
// FREND memory in shared memory, was done 13 February 2005.
*/

/* This software is subject to the following, which is believed to be
 * a verbatim copy of the well-known MIT license. */

/*-----  Beginning of License  ---------------------------------
Copyright (c) 2005, Mark Riordan

Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or
sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following
conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
-----  End of License  ---------------------------------------*/

/* I am playing with CVS: Concurrent Versions System.
 * I really don't like it--SourceSafe is much easier.
 * But here we go:
 * $Id: frend2.c,v 1.21 2006/02/19 02:36:29 riordanmr Exp $
 */

#ifdef _WIN32
#include <windows.h>
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "../msufrend_util.h"
#include "lmbi.h"
#include "frend2_helpers.h"

char *szFRENDVersion = "62.05";
static char *szAuthor = "Mark Riordan  4513 Gregg Rd  Madison, WI  53705";

/*=====  IP Sockets for communicating with DtCyber  ==========*/
extern SOCKET sockFromCyber;
extern SOCKET sockTCPListen;

/*=====  TCP Sockets for communicating with terminals  =======*/
unsigned short int TCPListenPort=6500;
TypSockTCP SockTCPAry[MAX_TCP_SOCKETS];


/*=====  Function prototypes  =============================== */
void TaskSENDCP(HalfWord PortNum, Byte8 MsgCode);
void TaskSKOTCL(HalfWord SocketNum, FrendAddr fwaMySocket);


/* Definitions for PC keyboard, for debugging.  */
/* These are for with _getch and are not the same as the VK_xxx codes. */
/* These would be the second byte read, after _getch returns 0. */
#define PCKEYCODE_F1  0x3b
#define PCKEYCODE_F2  0x3c
#define PCKEYCODE_F3  0x3d
#define PCKEYCODE_F4  0x3e
#define PCKEYCODE_F5  0x3f
#define PCKEYCODE_F6  0x40
#define PCKEYCODE_F7  0x41
#define PCKEYCODE_F8  0x42
#define PCKEYCODE_F9  0x43
#define PCKEYCODE_F10 0x44
#define PCKEYCODE_F11 0x85
#define PCKEYCODE_F12 0x86

#define PCKEYCODE_F1_SHIFT  0x54
#define PCKEYCODE_F2_SHIFT  0x55
#define PCKEYCODE_F3_SHIFT  0x56
#define PCKEYCODE_F4_SHIFT  0x57
#define PCKEYCODE_F5_SHIFT  0x58
#define PCKEYCODE_F6_SHIFT  0x59
#define PCKEYCODE_F7_SHIFT  0x5a
#define PCKEYCODE_F8_SHIFT  0x5b
#define PCKEYCODE_F9_SHIFT  0x5c
#define PCKEYCODE_F10_SHIFT  0x5d
#define PCKEYCODE_F11_SHIFT  0x87
#define PCKEYCODE_F12_SHIFT  0x88

#define ALIGN_FULLWORD(addr) (0xfffffffc & (3+addr))

/* Sets a 1-bit flag in a halfword */
/* EntryBaseAddr is the FWA of this table entry (NOT the FWA of the whole table). */
/* Name is the name of the flag.  We obtain both offset and bit position from this. */
#define SETHFLAG(EntryBaseAddr, Name) \
   SetHalfWord(EntryBaseAddr+H_##Name, (HalfWord)(GetHalfWord(EntryBaseAddr+H_##Name) | (1<<(15-J_##Name))));

/* Clears a 1-bit flag in a halfword */
/* EntryBaseAddr is the FWA of this table entry (NOT the FWA of the whole table). */
/* Name is the name of the flag.  We obtain both offset and bit position from this. */
#define CLEARHFLAG(EntryBaseAddr, Name) \
   SetHalfWord(EntryBaseAddr+H_##Name, (HalfWord)(GetHalfWord(EntryBaseAddr+H_##Name) & (0xffff - (1<<(15-J_##Name)))));

/* Returns 0 or 1, depending on whether a bit flag is set. */
#define HFLAGISSET(EntryBaseAddr, Name) \
   ((GetHalfWord(EntryBaseAddr+H_##Name)&(1<<(15-J_##Name))) ? 1 : 0)

/* hard-coded socket and port numbers for initial implementation. */
#define FSOCKETCONSOLE       4
#define FPORTCONSOLE         4  /* must be greater than PTN.MAX */
#define FIRSTUSERSOCK   5
#define NSOCKETS        8
#define NPORTS          8

/* Compute FWA of socket entry given socket number.
 * See FREND macro SOCKFWA. */
#define SockNumToFWA(socknum) (fwaSOCK + ((socknum-1) * LE_SOCK))

#define PortNumToFWA(portnum) (fwaPORT + (portnum-1)*LE_PORT)

int DebugL=LL_WARNING;

#define ADDR_IS_BOTTOM(addr) (1 & (addr))
#define ADDR_IS_TOP(addr) !ADDR_IS_BOTTOM(addr)

extern TypFrendInterface *pFrendInt;
TypBool bSendReplyToCyber=FALSE;  /* If non-zero, send response to DyCyber.
											  * -s cmdline param causes this to be TRUE. */

/* First Word Addresses of above tables pointed to by entries
 * in the LMBI POINTER TABLE.  These are indices into
 * FrendState.fr_mem[]. */
FrendAddr fwaMISC, fwaFPCOM, fwaBF80, fwaBF240;
FrendAddr fwaBFREL, fwaBANM, fwaLOGM, fwaSOCK;
FrendAddr fwaDVSK, fwaPORT, fwaPTBUF, fwaMALC, fwaALLOC;
FrendAddr fwaBuffers80, fwaBuffers240;

char *szOperSessFilename = "session.log";
char *szOperSessFilenameOld = "session.oldlog";
FILE *fileOperSess=NULL;  /* File stream to which operator session is being logged; */
                          /* NULL if none. */

TypBool EOLL=FALSE;  /* TRUE if last line ended in end of line */
                  /* It is a copy of the socket's SKOEOL */
HalfWord PORTNUM=0;  /* port number used as communication between GETDATA
                        and other routines */

/*------------------------------------------------------------*/

/*--- function AddrFRENDTo6CA ----------------------
 *  Convert and address from from FREND to 1FP format.
 *  This means dividing by 2, and ORing in the magic value
 *  intended to catch hardware errors.
 */
FrendAddr AddrFRENDTo1FP(FrendAddr addr)
{
   if(addr) {
      return (addr>>1) | (F_PTIN<<24);
   } else {
      return 0;
   }
}

/*--- function Addr1FPToFREND ----------------------
 *  Convert an address from 1FP format to FREND format.
 *  This means multiplying by 2.
 *  To play it safe, we also get rid of the F_PTIN bits.
 */
FrendAddr Addr1FPToFREND(FrendAddr addr)
{
   return (addr & 0xffffff) << 1;  /* clear magic F_PTIN bits */
}

#define ADDR_TO_6CA(addr) (addr >> 1)
#define ADDR_FROM_6CA(addr) (addr << 1)

void SetFullWord(FrendAddr addr, FullWord word)
{
   pFrendInt->FrendState.fr_mem[addr] = (Byte8)(word >> 24);
   pFrendInt->FrendState.fr_mem[1+addr] = (Byte8)(word >> 16);
   pFrendInt->FrendState.fr_mem[2+addr] = (Byte8)(word >> 8);
   pFrendInt->FrendState.fr_mem[3+addr] = (Byte8)(word);
}

FullWord GetFullWord(FrendAddr addr)
{
   return (pFrendInt->FrendState.fr_mem[addr] << 24) | (pFrendInt->FrendState.fr_mem[1+addr] << 16)
     | (pFrendInt->FrendState.fr_mem[2+addr] << 8) | pFrendInt->FrendState.fr_mem[3+addr];

}

void SetHalfWord(FrendAddr addr, HalfWord half)
{
   pFrendInt->FrendState.fr_mem[addr] = (Byte8)(half >> 8);
   pFrendInt->FrendState.fr_mem[1+addr] = (Byte8)(half);
}

HalfWord GetHalfWord(FrendAddr addr)
{
   return (pFrendInt->FrendState.fr_mem[addr] << 8) | (pFrendInt->FrendState.fr_mem[1+addr]);
}

void SetByte(FrendAddr addr, Byte8 byte)
{
   pFrendInt->FrendState.fr_mem[addr] = byte;
}

Byte8 GetByte(FrendAddr addr)
{
   return pFrendInt->FrendState.fr_mem[addr];
}

void DumpMem()
{
   FrendAddr addr;
   FullWord prevword=9999, word;
   for(addr=0; addr<MAX_FREND_BYTES-4; addr+=4) {
      word = GetFullWord(addr);
      if(word != prevword) {
         LogOut("%6x: %-2.2x %-2.2x %-2.2x %-2.2x", addr, GetByte(addr),
            GetByte(1+addr), GetByte(2+addr), GetByte(3+addr));
      }
      prevword = word;
   }
}

void CopyFRENDBytes(FrendAddr dest, FrendAddr source, FullWord nbytes)
{
   while(nbytes) {
      SetByte(dest, GetByte(source));
      dest++;
      source++;
      nbytes--;
   }
}

/*--- function WriteToOperTerm ----------------------
 *  Write a character to the operator terminal, possibly
 *  also logging it to a file.
 */
void WriteToOperTerm(Byte8 ch)
{
   putchar(ch);
   if(fileOperSess) putc(ch, fileOperSess);
}

/*--- function FindFSockFromTCPSock ----------------------
 */
HalfWord FindFSockFromTCPSock(SOCKET sock)
{
   HalfWord fsock=0;
   for(fsock=FIRSTUSERSOCK; fsock<MAX_TCP_SOCKETS; fsock++) {
      if(SockTCPAry[fsock].stcp_socket == sock) {
         return fsock;
      }
   }
   return 0;
}

/*--- function SendToFSock ----------------------
 *  Send a buffer of bytes to a socket.
 *  We special-case the console, which is not connected via TCP.
 *  Exit:   Returns # of bytes sent, or -1 if error.
 *          (The error is usually EWOULDBLOCK--not really an error.)
 *          The # may be less than noutbytes because socket is non-blocking.
 */
int SendToFSock(HalfWord SocketNum, Byte8 bufout[], int noutbytes)
{
   int bytes_sent;
   if(FSOCKETCONSOLE==SocketNum) {
      int j;
      for(j=0; j<noutbytes; j++) {
         WriteToOperTerm(bufout[j]);
      }
      return noutbytes;
   } else {
      SOCKET sock = SockTCPAry[SocketNum].stcp_socket;
      if(sock) {
         /*//! We should handle the case where not all bytes are sent.
          * That would require queuing (fairly simple) and to really 
          * do it right, the ability to stop accepting lines from the
          * host until the output queue is empty. */
         bytes_sent = TCPSend(sock, bufout, noutbytes);
         if(bytes_sent != noutbytes) {
            LogOut("==** SendToFSock: sent %d of %d bytes", bytes_sent, noutbytes);
         }
         return bytes_sent;
      } else {
         printf("*** Error: SendToFSock on fsock %d has 0 TCP socket\n", SocketNum);
         return -1;
      }
   }
}

/*--- function ClearSockTCPEntry ----------------------
 */
void ClearSockTCPEntry(HalfWord fsock)
{
   memset(&SockTCPAry[fsock], 0, sizeof(SockTCPAry[fsock]));
}

/*--- function ClearTCPSockForFSock ----------------------
 */
void ClearTCPSockForFSock(HalfWord fsock)
{
   SOCKET sockUser = SockTCPAry[fsock].stcp_socket;
   closesocket(sockUser);
   ClearSockTCPEntry(fsock);
}

/*=====  Circular List functions.  See lmbi.h for description.  =====*/

/*--- function InitCircList ----------------------
 *  Initialize a 7/32-style circular list.
 *  Entry:  fwa      is the addess of the list.
 *          nslots   is the number of slots in the list.
 *  Exit:   Returns # of bytes in circular list
 */
HalfWord InitCircList(FrendAddr fwaList, HalfWord nslots, const char *szmsg)
{
   HalfWord totbytes =  H_CIRCLIST_HEADER_BYTES + (nslots * CIRCLIST_SLOT_SIZE_BYTES);
   memset(pFrendInt->FrendState.fr_mem + fwaList, 0, totbytes);
   SetHalfWord(fwaList+H_CIRCLIST_N_SLOTS_TOT, nslots);
   if(DebugL>=LL_SOME) LogOut("Initialized circ list at %xH with %d slots for %s", fwaList, nslots, szmsg);
   return totbytes;
}

/*--- function GetListUsedEntries ----------------------
 *  Exit:  Returns # of used entries in the list.
 */
HalfWord GetListUsedEntries(FrendAddr fwaList)
{
   return GetHalfWord(fwaList+H_CIRCLIST_N_USED);
}

/*--- function GetListUsedEntries ----------------------
 *  Exit:  Returns total # of entries (used and free) in the list.
 */
HalfWord GetListTotalEntries(FrendAddr fwaList)
{
   return GetHalfWord(fwaList+H_CIRCLIST_N_SLOTS_TOT);
}

/*--- function GetListFreeEntries ----------------------
 *  Exit:   Returns # of available slots in list.
 */
HalfWord GetListFreeEntries(FrendAddr fwaList)
{
   return GetListTotalEntries(fwaList) - GetListUsedEntries(fwaList);
}

/*--- function FindEntryInList ----------------------
 *  Look for a value in a circular list.  This is used for debugging.
 *  Entry:  fwaList  is the FWA of a circular list.
 *          myword   is the word we are looking for.
 *
 *  Exit:   Returns CIRCLIST_NOT_FOUND if not found, else
 *             slot number found.
 */
HalfWord FindEntryInList(FrendAddr fwaList, FullWord myword)
{
   HalfWord islot;
   HalfWord slots_so_far;
   FullWord thisWord;
   HalfWord nSlotsTot = GetHalfWord(fwaList+H_CIRCLIST_N_SLOTS_TOT);
   HalfWord curTop = GetHalfWord(fwaList+H_CIRCLIST_CUR_TOP);
   HalfWord nextBot = GetHalfWord(fwaList+H_CIRCLIST_NEXT_BOT);
   HalfWord nUsed = GetHalfWord(fwaList+H_CIRCLIST_N_USED);

   for(islot = curTop, slots_so_far=0; slots_so_far < nUsed; slots_so_far++) {
      thisWord = GetFullWord(CircListSlotAddr(fwaList, islot));
      if(thisWord == myword) {
         return islot;
      }
      islot++;
      if(islot >= nSlotsTot) islot = 0;
   }
   return CIRCLIST_NOT_FOUND;
}

/*--- function AddToTopOfList ----------------------
 *  Add a word to a 7/32 circular list, at the top.
 *  Entry:  fwaList  is the FWA of the list.
 *          myword   is the 4-byte word to add.
 */
void AddToTopOfList(FrendAddr fwaList, FullWord myword)
{
   HalfWord curTop;
   HalfWord nSlotsUsed = GetHalfWord(fwaList+H_CIRCLIST_N_USED);
   HalfWord nSlotsTot = GetHalfWord(fwaList+H_CIRCLIST_N_SLOTS_TOT);
   /*//! debugging */
   if(CIRCLIST_NOT_FOUND != FindEntryInList(fwaList, myword)) {
      LogOut("==** AddToTopOfList(%x, %x): value already in list", fwaList, myword);
      return;
   }
   if(nSlotsUsed >= nSlotsTot) {
      /* Don't add if list is full. */
      LogOut("*** Error: AddToTopOfList(%xH, %xH): list is full (capacity %d)", fwaList, myword, nSlotsTot);
   } else {
      nSlotsUsed++;
      SetHalfWord(fwaList+H_CIRCLIST_N_USED, nSlotsUsed);
      /* We add to the top by DECREMENTING the top pointer circularly. */
      curTop = GetHalfWord(fwaList+H_CIRCLIST_CUR_TOP);
      if(0==curTop) {
         curTop = nSlotsTot-1;
      } else {
         curTop--;
      }
      SetFullWord(CircListSlotAddr(fwaList, curTop), myword);
      SetHalfWord(fwaList+H_CIRCLIST_CUR_TOP, curTop);
   }
   if(DebugL>=LL_SOME) LogOut("==AddToTopOfList(%x, %x): nSlotsFree now %d", fwaList, myword, GetListFreeEntries(fwaList));
}

/*--- function AddToBottomOfList ----------------------
 *  Add a word to a 7/32 circular list, at the bottom.
 *  Entry:  fwaList  is the FWA of the list.
 *          myword   is the 4-byte word to add.
 */
void AddToBottomOfList(FrendAddr fwaList, FullWord myword)
{
   HalfWord nextBot;
   HalfWord nSlotsUsed = GetHalfWord(fwaList+H_CIRCLIST_N_USED);
   HalfWord nSlotsTot = GetHalfWord(fwaList+H_CIRCLIST_N_SLOTS_TOT);
   if(nSlotsUsed >= nSlotsTot) {
      /* Don't add if list is full. */
      LogOut("*** Error: AddToBottomOfList(%xH, %xH): list is full", fwaList, myword);
   } else {
      nSlotsUsed++;
      SetHalfWord(fwaList+H_CIRCLIST_N_USED, nSlotsUsed);
      /* We add to the top by INCREMENTING the top pointer circularly. */
      nextBot = GetHalfWord(fwaList+H_CIRCLIST_NEXT_BOT);
      SetFullWord(CircListSlotAddr(fwaList, nextBot), myword);
      if(nextBot >= nSlotsTot) {
         nextBot = 0;
      } else {
         nextBot++;
      }
      SetHalfWord(fwaList+H_CIRCLIST_NEXT_BOT, nextBot);
   }
   if(DebugL>=LL_SOME) LogOut("==AddToBottomOfList(%x,%x): nSlotsFree=%d", fwaList, myword, GetListFreeEntries(fwaList));
}

/*--- function RemoveFromTopOfList ----------------------
 *  Entry:  fwaList  is the FWA of a 7/32 circular list.
 *  Exit:   Returns the current top, and removes it from the list,
 *          or returns 0 if the list was empty.
 */
FullWord RemoveFromTopOfList(FrendAddr fwaList)
{
   FullWord myWord=0;
   HalfWord curTop;
   HalfWord nSlotsUsed = GetHalfWord(fwaList+H_CIRCLIST_N_USED);
   HalfWord nSlotsTot = GetHalfWord(fwaList+H_CIRCLIST_N_SLOTS_TOT);
   if(0 == nSlotsUsed) {
      /* List is empty */
      /* LogOut("*** Error: RemoveFromTopOfList(%xH): list is empty", fwaList); */
   } else {
      curTop = GetHalfWord(fwaList+H_CIRCLIST_CUR_TOP);
      myWord = GetFullWord(CircListSlotAddr(fwaList, curTop));

      /* remove from the top by incrementing the top, toward the bottom. */
      curTop++;
      if(curTop >= nSlotsTot) curTop = 0;
      SetHalfWord(fwaList+H_CIRCLIST_CUR_TOP, curTop);

      nSlotsUsed--;
      SetHalfWord(fwaList+H_CIRCLIST_N_USED, nSlotsUsed);
   }
   if(DebugL>=LL_SOME) LogOut("==RemoveFromTopOfList(%x): returning %x, nSlotsFree=%d", fwaList, myWord, GetListFreeEntries(fwaList));
   return myWord;
}

/*--- function RemoveFromBottomOfList ----------------------
 *  Entry:  fwaList  is the FWA of a 7/32 circular list.
 *  Exit:   Returns the current bottom, and removes it from the list
 *          or returns 0 if the list was empty.
 */
FullWord RemoveFromBottomOfList(FrendAddr fwaList)
{
   FullWord myWord=0;
   HalfWord nextBot, curBot;
   HalfWord nSlotsUsed = GetHalfWord(fwaList+H_CIRCLIST_N_USED);
   HalfWord nSlotsTot = GetHalfWord(fwaList+H_CIRCLIST_N_SLOTS_TOT);
   if(0 == nSlotsUsed) {
      /* List is empty */
      /*LogOut("*** Error: RemoveFromBottomOfList(%xH): list is empty", fwaList);*/
   } else {
      /* The current bottom must be calculated by backing
       * up from the next bottom. */
      nextBot = GetHalfWord(fwaList+H_CIRCLIST_NEXT_BOT);
      if(0==nextBot) {
         curBot = nSlotsTot - 1;
      } else {
         curBot = nextBot - 1;
      }
      /* Fetch this current bottom. */
      myWord = GetFullWord(CircListSlotAddr(fwaList, curBot));
      /* Now the next bottom is what the current bottom used to be. */
      SetHalfWord(fwaList+H_CIRCLIST_NEXT_BOT, curBot);

      nSlotsUsed--;
      SetHalfWord(fwaList+H_CIRCLIST_N_USED, nSlotsUsed);
   }
   if(DebugL>=LL_SOME) LogOut("==RemoveFromBottomOfList(%x): returning %x, nSlotsFree=%d", fwaList, myWord, GetListFreeEntries(fwaList));
   return myWord;
}

/*--- function ListIsEmpty ----------------------
 *  Entry:  fwaList  is the FWA of a 7/32 circular list.
 *  Exit:   Returns TRUE if list is empty, else FALSE.
 */
TypBool ListIsEmpty(FrendAddr fwaList)
{
   return 0 == GetListUsedEntries(fwaList);
}

/*--- function DumpCircList ----------------------
 *  Dump a 7/32 circular list, for debugging.
 */
void DumpCircList(FrendAddr fwaList)
{
   HalfWord islot;
   HalfWord slots_so_far;
   FullWord myWord;
   HalfWord nSlotsTot = GetHalfWord(fwaList+H_CIRCLIST_N_SLOTS_TOT);
   HalfWord curTop = GetHalfWord(fwaList+H_CIRCLIST_CUR_TOP);
   HalfWord nextBot = GetHalfWord(fwaList+H_CIRCLIST_NEXT_BOT);
   HalfWord nUsed = GetHalfWord(fwaList+H_CIRCLIST_N_USED);

   LogOut("CircList at %xH: nSlots=%u nUsed=%u curTop=%u nextBot=%u",
      fwaList,
      GetHalfWord(fwaList+H_CIRCLIST_N_SLOTS_TOT),
      GetHalfWord(fwaList+H_CIRCLIST_N_USED),
      GetHalfWord(fwaList+H_CIRCLIST_CUR_TOP),
      GetHalfWord(fwaList+H_CIRCLIST_NEXT_BOT));
   for(islot = curTop, slots_so_far=0; slots_so_far < nUsed; slots_so_far++) {
      myWord = GetFullWord(CircListSlotAddr(fwaList, islot));
      LogOut("  slot %2d = %x", islot, myWord);
      islot++;
      if(islot >= nSlotsTot) islot = 0;
   }
}

typedef struct struct_sym_to_name {
   int   sn_sym;
   char *sn_name;
} TypSymToName;

TypSymToName SymToNameRecordTypes[] =
{
   {FP_DATA, "FP_DATA"},
   {FP_OPEN, "FP_OPEN"},
   {FP_CLO, "FP_CLO"},
   {FP_ABT, "FP_ABT"},
   {FP_INBS, "FP_INBS"},
   {FP_OTBS, "FP_OTBS"},
   {FP_ORSP, "FP_ORSP"},
   {FP_STAT, "FP_STAT"},
   {FP_FCRP, "FP_FCRP"},
   {FP_EOR, "FP_EOR"},
   {FP_EOF, "FP_EOF"},
   {FP_UNLK, "FP_UNLK"},
   {FP_FEC, "FP_FEC"},
   {FP_CPOPN, "FP_CPOPN"},
   {FP_CPCLO, "FP_CPCLO"},
   {FP_BULK, "FP_BULK"},
   {FP_CAN, "FP_CAN"},
   {FP_EOI, "FP_EOI"},
   {FP_GETO, "FP_GETO"},
   {FP_NEWPR, "FP_NEWPR"},
   {FP_ENDJ, "FP_ENDJ"},
   {FP_INST, "FP_INST"},
   {FP_SKB, "FP_SKB"},
   {FP_SKIP, "FP_SKIP"},
   {FP_ACCT, "FP_ACCT"},
   {FP_BLDAT, "FP_BLDAT"},
   {FP_COPY, "FP_COPY"},
   {FP_EOREI, "FP_EOREI"},
   {FP_FECNE, "FP_FECNE"},
   {FP_CMDPE, "FP_CMDPE"},
   {FP_CMDCY, "FP_CMDCY"},
   {FP_RPYPE, "FP_RPYPE"},
   {FP_RPYCY, "FP_RPYCY"},
   {FP_SCRTR, "FP_SCRTR"},
   {FP_TIME, "FP_TIME"},
   {-1, ""}
};

TypSymToName SymToNameLMBIPT[] =
     {
        {W_PWFWA, "W_PWFWA"},
        {H_PWLE, "H_PWLE"},
        {H_PWNE, "H_PWNE"},
        {H_PWM1, "H_PWM1"},
        {H_PWM2, "H_PWM2"},
     {-1, }};

TypSymToName SymToNameMISC[] =
     {
        {H_MIHR, "H_MIHR"},
        {H_MIMI, "H_MIMI"},
        {H_MISEC, "H_MISEC"},
        {H_MIMON, "H_MIMON"},
        {H_MIDAY, "H_MIDAY"},
        {H_MIYR, "H_MIYR"},
        {W_MIVER, "W_MIVER"},
     {-1, }};
TypSymToName SymToNameFPCOM[] =
     {
        {H_FEDEAD, "H_FEDEAD"},
        {H_FCMDIK, "H_FCMDIK"},
        {C_FCMDVA, "C_FCMDVA"},
        {C_FCMDTY, "C_FCMDTY"},
        {H_FCMDPT, "H_FCMDPT"},
        {C_CPOPT,  "C_CPOPT"},
        {W_LFCNT, "W_LFCNT"},
        {H_NBUFIK, "H_NBUFIK"},
        {H_NOBUF, "H_NOBUF"},
        {W_NBF80, "W_NBF80"},
        {W_NBF240, "W_NBF240"},
     {-1, }};
TypSymToName SymToNameSOCK[] =
     {
        {C_SKTYPE, "C_SKTYPE"},
        {C_SKIBD, "C_SKIBD"},
        {H_SKDEV, "H_SKDEV"},
        {W_SKPNUM, "W_SKPNUM"},
        {H_SKOCBA, "H_SKOCBA"},
        {H_SKNUM, "H_SKNUM"},
        {C_SKSYS, "C_SKSYS"},
        {C_SKBUS, "C_SKBUS"},
        {C_SKNLOG, "C_SKNLOG"},
        {C_SKIOTM, "C_SKIOTM"},
        {C_SKCXST, "C_SKCXST"},
        {C_SKCXBL, "C_SKCXBL"},
        {C_SKIFLG, "C_SKIFLG"},
        {C_SKRSFG, "C_SKRSFG"},
        {C_SKINTT, "C_SKINTT"},
        {C_SKTTY, "C_SKTTY"},
        {C_SKFBD, "C_SKFBD"},
        {C_SKPAR, "C_SKPAR"},
        {C_SKCRC, "C_SKCRC"},
        {C_SKLFC, "C_SKLFC"},
        {C_SKHTC, "C_SKHTC"},
        {C_SKVTC, "C_SKVTC"},
        {C_SKFFC, "C_SKFFC"},
        {C_SKLINE, "C_SKLINE"},
        {C_SKRM, "C_SKRM"},
        {C_SKTLT, "C_SKTLT"},
        {C_SKFECC, "C_SKFECC"},
        {C_SKNPCC, "C_SKNPCC"},
        {H_SKINLE, "H_SKINLE"},
        {C_SKECTB, "C_SKECTB"},
        {C_SKALCH, "C_SKALCH"},
        {W_SKALXL, "W_SKALXL"},
        {W_SKTID1, "W_SKTID1"},
        {C_SKTID2, "C_SKTID2"},
        {W_SKFLAG, "W_SKFLAG"},
        {C_SKVCOL, "C_SKVCOL"},
        {C_SKCT1, "C_SKCT1"},
        {C_SKCT2, "C_SKCT2"},
        {C_SKCTIN, "C_SKCTIN"},
        {H_SKCN1, "H_SKCN1"},
        {H_SKCN2, "H_SKCN2"},
        {H_SKID, "H_SKID"},
        {H_SKMTRP, "H_SKMTRP"},
        {H_SKLIT, "H_SKLIT"},
        {C_SKISTA, "C_SKISTA"},
        {C_SKDCTL, "C_SKDCTL"},
        {W_SKDATA, "W_SKDATA"},
        {W_SKECBF, "W_SKECBF"},
        {W_SKINBF, "W_SKINBF"},
        {H_SKINCC, "H_SKINCC"},
        {H_SKECHO, "H_SKECHO"},
        {W_SKPORD, "W_SKPORD"},
        {W_SKOTCL, "W_SKOTCL"},
     {-1, }};
TypSymToName SymToNamePORT[] =
     {
        {C_PTTYPE, "C_PTTYPE"},
        {H_PTCPN, "H_PTCPN"},
        {C_PTCT1, "C_PTCT1"},
        {H_PTCN1, "H_PTCN1"},
        {H_PTID, "H_PTID"},
        {H_PTWTBF, "H_PTWTBF"},
        {W_PTIN, "W_PTIN"},
        {H_PTINIK, "H_PTINIK"},
        {H_PTNDDT, "H_PTNDDT"},
        {H_PTNDIK, "H_PTNDIK"},
        {H_PTFLAG, "H_PTFLAG"},
        {H_PTFLG2, "H_PTFLG2"},
        {W_PTPBUF, "W_PTPBUF"},
        {W_PTOT, "W_PTOT"},
        {H_PTOTIK, "H_PTOTIK"},
        {H_PTOTNE, "H_PTOTNE"},
        {W_PTOTCL, "W_PTOTCL"},
        {W_PTINCL, "W_PTINCL"},
     {-1, }};

/* Table for producing debug info, mapping LMBI addresses to table names. */
struct struct_LMBI_Pointer_info {
   FullWord       lmpi_fwa;
   char          *lmpi_desc;
   TypSymToName  *lmpi_sn;
} LMPI_Pointer_Descs[] =
{
   {PW_MISC   , "MISC", SymToNameMISC},
   {PW_FPCOM  , "FPCOM", SymToNameFPCOM},
   {PW_BF80   , "BF80", NULL},
   {PW_BF240  , "BF240",NULL},
   {PW_BFREL  , "BFREL",NULL},
   {PW_BANM   , "BANM",NULL},
   {PW_LOGM   , "LOGM",NULL},
   {PW_SOCK   , "SOCK", SymToNameSOCK},
   {PW_DVSK   , "DVSK",NULL},
   {PW_PORT   , "PORT", SymToNamePORT},
   {PW_PTBUF  , "PTBUF",NULL},
   {PW_MALC   , "MALC",NULL},
   {PW_ALLOC  , "ALLOC",NULL},
   {0         , "", NULL}
};

/*--- function GetNameFromOffset ----------------------
 *  Get the textual name for an offset into a table, given
 *  the numeric offset and a table mapping offsets to name.
 */
char *GetNameFromOffset(TypSymToName *psn, int offset)
{
   int j;
   static char szbuf[32];
   for(j=0; psn && psn[j].sn_sym >= 0; j++) {
      if(offset == psn[j].sn_sym) {
         return psn[j].sn_name;
      }
   }
   /* The value wasn't found in the table, so return a printable
    * version of the numeric value. */
   sprintf(szbuf, "%d", offset);
	return szbuf;
}

/*--- function GetDescForAddr ----------------------
 *  Get a textual description of what an address points to,
 *  for debugging.
 */
char *GetDescForAddr(FrendAddr addr)
{
   int j;
   static char szDesc[80];
   TypBool bFound=FALSE;
   if(H_INICMP == addr) {
      sprintf(szDesc, "H_INICMP");
      bFound = TRUE;
   } else if(H_INICMP+1 == addr) {
      sprintf(szDesc, "H_INICMP+1");
      bFound = TRUE;
   }

   if(!bFound) for(j=0; LMPI_Pointer_Descs[j].lmpi_fwa; j++) {
      if(addr >= LMPI_Pointer_Descs[j].lmpi_fwa &&
         addr < (LMPI_Pointer_Descs[j].lmpi_fwa + L_LMBPT)) {
         /* This points to inside the LMBI pointer table. */
         sprintf(szDesc, "LMBIPt of %s+%s", LMPI_Pointer_Descs[j].lmpi_desc,
            GetNameFromOffset(SymToNameLMBIPT,addr-LMPI_Pointer_Descs[j].lmpi_fwa));
         bFound = TRUE;
         break;
      }
   }

   /* If we didn't find it, try looking in the individual tables
    * pointed to by the LMBI pointer table. */
   if(!bFound) {
      for(j=0; LMPI_Pointer_Descs[j].lmpi_fwa; j++) {
         FrendAddr lmbi_entry_fwa = LMPI_Pointer_Descs[j].lmpi_fwa;
         FullWord table_fwa = GetFullWord(lmbi_entry_fwa+W_PWFWA);
         HalfWord len_entry = GetHalfWord(lmbi_entry_fwa+H_PWLE);
         HalfWord nentries = GetHalfWord(lmbi_entry_fwa+H_PWNE);
         FullWord bytes_in_table = len_entry * nentries;
         if(addr >= table_fwa && addr < (table_fwa + bytes_in_table)) {
            /* This points to inside this table.  Figure out which entry. */
            int offset_from_fwa = addr - table_fwa;
            int ientry = offset_from_fwa / len_entry;
            int offset_from_entry = offset_from_fwa - (ientry*len_entry);
            /* This points to inside the LMBI pointer table. */
            sprintf(szDesc, "%s[%d]+%s", LMPI_Pointer_Descs[j].lmpi_desc,
               ientry, GetNameFromOffset(LMPI_Pointer_Descs[j].lmpi_sn, offset_from_entry));
            bFound = TRUE;
            break;
         }
      }
   }

   if(!bFound) {
      sprintf(szDesc, "Unknown");
   }
   return szDesc;
}

void SetPortHalfWord(int portnum, int offset, HalfWord val)
{
   SetHalfWord(PortNumToFWA(portnum) + offset, val);
}

void SetPortFullWord(int portnum, int offset, FullWord val)
{
   SetFullWord(PortNumToFWA(portnum) + offset, val);
}

Byte8 GetPortByte(int portnum, int offset)
{
   return GetByte(PortNumToFWA(portnum) + offset);
}

void DumpPortEntry(int portnum)
{
   FrendAddr fwaMyPort = PortNumToFWA(portnum);
   LogOut("Dump of port %d: type %u ctlport %u c1type %u c1# %u ID %u "
      "PTIN %xH PTINIK %u PTOT %xH PTOTIK %u PTOTNE %u PTOTCL %xH PTINCL %xH",
      portnum, GetByte(fwaMyPort+C_PTTYPE), GetHalfWord(fwaMyPort+H_PTCPN),
      GetByte(fwaMyPort+C_PTCT1), GetHalfWord(fwaMyPort+H_PTCN1),
      GetHalfWord(fwaMyPort+H_PTID), 
      GetFullWord(fwaMyPort+W_PTIN), GetHalfWord(fwaMyPort+H_PTINIK),
      GetFullWord(fwaMyPort+W_PTOT), GetHalfWord(fwaMyPort+H_PTOTIK),
      GetHalfWord(fwaMyPort+H_PTOTNE), 
      GetFullWord(fwaMyPort+W_PTOTCL), GetFullWord(fwaMyPort+W_PTINCL)
      );
}

/*--- function InterlockIsFree ----------------------
 *  Entry:  addr  is the address of a halfword interlock.
 *
 *  Exit:   Returns 1 if the interlock is available, else 0.
 */
TypBool InterlockIsFree(FrendAddr addr)
{
   HalfWord lock = GetHalfWord(addr);
   return (0 == (0x8000 & lock));
}

/*--- function INTRLOC ----------------------
 *  Wait for and obtain an interlock.
 *  Entry:  addr    is the address of a halfword interlock.
 */
void INTRLOC(FrendAddr addr)
{
   /* //! We don't actually wait for the lock--just warn if someone
    * already has it. */
   if(0x8000 & GetHalfWord(addr)) {
      if(DebugL>=LL_SOME) LogOut("==**INTRLOC: Warning: Lock %x already obtained", addr);
   }
   SetHalfWord(addr, 0x8000);
}

/*--- function DropIL ----------------------
 *  Clear an interlock by setting a special value (1 == clear).
 *  Entry:  addr  is the address of the interlock halfword.
 */
void DropIL(FrendAddr addr)
{
   SetHalfWord(addr, CLR_TS);
}

/*--- function Get80 ----------------------
 *  Exit:   Returns the address of a free 80-character buffer.
 */
FrendAddr Get80()
{
   FrendAddr bufaddr = RemoveFromBottomOfList(fwaBF80);
   if(0==bufaddr) LogOut("==** Error: Get80: no free buffers");
   return bufaddr;
}

/*--- function Get240 ----------------------
 *  Exit:   Returns the address of a free 240-character buffer.
 */
FrendAddr Get240()
{
   FrendAddr bufaddr = RemoveFromBottomOfList(fwaBF240);
   if(0==bufaddr) LogOut("==** Error: Get80: no free buffers");
   return bufaddr;
}

/*--- function GetBufferForC ----------------------
 *  Given a C string pointer (NOT in FREND memory),
 *  allocate a FREND buffer, fill it, and return it.
 */
FrendAddr GetBufferForC(const char *szmsg)
{
   Byte8 len=strlen(szmsg);
   /* Technically, I should first check len for >80 */
   FrendAddr bufaddr = Get80();
   if(len>80) len=80; /* //! kludge to play it safe. */
   memcpy(&pFrendInt->FrendState.fr_mem[bufaddr+L_DTAHDR], szmsg, len);
   SetByte(bufaddr+C_DHBCT, (Byte8)(len+L_DTAHDR));
   return bufaddr;
}

/*------------------------------------------------------------*/
void InitLMBI()
{
   HalfWord nBytes, nSlots, islot;
   FrendAddr cur_table_fwa = FWAMBI_1 + 0x1000;
   FrendAddr cur_lmbi_table_entry = FWAMBI_1;

   /* The below was generated by SetupLMBIPointerTable.awk */

   assert(PW_MISC == cur_lmbi_table_entry);
   fwaMISC = cur_table_fwa;
   SetFullWord(cur_lmbi_table_entry+W_PWFWA, cur_table_fwa);
   SetHalfWord(cur_lmbi_table_entry+H_PWLE, L_MISC);
   SetHalfWord(cur_lmbi_table_entry+H_PWNE, 1);
   LogOut("Entry for MISC = 0x%x; table FWA=%x", PW_MISC, cur_table_fwa);
   cur_table_fwa += L_MISC*1;
   cur_table_fwa = ALIGN_FULLWORD(cur_table_fwa);
   cur_lmbi_table_entry += L_LMBPT;

   assert(PW_FPCOM == cur_lmbi_table_entry);
   fwaFPCOM = cur_table_fwa;
   SetFullWord(cur_lmbi_table_entry+W_PWFWA, cur_table_fwa);
   SetHalfWord(cur_lmbi_table_entry+H_PWLE, L_FPCOM);
   SetHalfWord(cur_lmbi_table_entry+H_PWNE, 1);
   LogOut("Entry for FPCOM = 0x%x; table FWA=%x", PW_FPCOM, cur_table_fwa);
   cur_table_fwa += L_FPCOM*1;
   cur_table_fwa = ALIGN_FULLWORD(cur_table_fwa);
   cur_lmbi_table_entry += L_LMBPT;

   assert(PW_BF80 == cur_lmbi_table_entry);
   fwaBF80 = cur_table_fwa;
   SetFullWord(cur_lmbi_table_entry+W_PWFWA, cur_table_fwa);
   SetHalfWord(cur_lmbi_table_entry+H_PWLE, 4);
   nSlots = 40;
   nBytes = InitCircList(fwaBF80, nSlots, "BF80");
   SetHalfWord(cur_lmbi_table_entry+H_PWNE, (HalfWord)(nBytes/4));
   LogOut("Entry for BF80 = 0x%x; table FWA=%x", PW_BF80, cur_table_fwa);
   cur_table_fwa += nBytes;
   cur_table_fwa = ALIGN_FULLWORD(cur_table_fwa);
   cur_lmbi_table_entry += L_LMBPT;

   assert(PW_BF240 == cur_lmbi_table_entry);
   fwaBF240 = cur_table_fwa;
   SetFullWord(cur_lmbi_table_entry+W_PWFWA, cur_table_fwa);
   SetHalfWord(cur_lmbi_table_entry+H_PWLE, 4);
   nBytes = InitCircList(fwaBF240, nSlots, "BF240");
   SetHalfWord(cur_lmbi_table_entry+H_PWNE, (HalfWord)(nBytes/4));
   LogOut("Entry for BF240 = 0x%x; table FWA=%x", PW_BF240, cur_table_fwa);
   cur_table_fwa += nBytes;
   cur_table_fwa = ALIGN_FULLWORD(cur_table_fwa);
   cur_lmbi_table_entry += L_LMBPT;

   assert(PW_BFREL == cur_lmbi_table_entry);
   fwaBFREL = cur_table_fwa;
   SetFullWord(cur_lmbi_table_entry+W_PWFWA, cur_table_fwa);
   SetHalfWord(cur_lmbi_table_entry+H_PWLE, 4);
   nSlots += nSlots;  /* Allocate enough space for all 80 and 240-char buffers */
   nBytes = InitCircList(fwaBFREL, nSlots, "BFREL");
   SetHalfWord(cur_lmbi_table_entry+H_PWNE, nSlots);
   LogOut("Entry for BFREL = 0x%x; table FWA=%x", PW_BFREL, cur_table_fwa);
   cur_table_fwa += nBytes;
   cur_table_fwa = ALIGN_FULLWORD(cur_table_fwa);
   cur_lmbi_table_entry += L_LMBPT;

   assert(PW_BANM == cur_lmbi_table_entry);
   fwaBANM = cur_table_fwa;
   SetFullWord(cur_lmbi_table_entry+W_PWFWA, cur_table_fwa);
   SetHalfWord(cur_lmbi_table_entry+H_PWLE, LE_BANM);
   SetHalfWord(cur_lmbi_table_entry+H_PWNE, NE_BANM);
   LogOut("Entry for BANM = 0x%x; table FWA=%x", PW_BANM, cur_table_fwa);
   cur_table_fwa += LE_BANM*NE_BANM;
   cur_table_fwa = ALIGN_FULLWORD(cur_table_fwa);
   cur_lmbi_table_entry += L_LMBPT;

   assert(PW_LOGM == cur_lmbi_table_entry);
   fwaLOGM = cur_table_fwa;
   SetFullWord(cur_lmbi_table_entry+W_PWFWA, cur_table_fwa);
   SetHalfWord(cur_lmbi_table_entry+H_PWLE, LE_LOGM);
   SetHalfWord(cur_lmbi_table_entry+H_PWNE, NE_LOGM);
   LogOut("Entry for LOGM = 0x%x; table FWA=%x", PW_LOGM, cur_table_fwa);
   cur_table_fwa += LE_LOGM*NE_LOGM;
   cur_table_fwa = ALIGN_FULLWORD(cur_table_fwa);
   cur_lmbi_table_entry += L_LMBPT;

#define MRR_N_SOCKETS 6
   assert(PW_SOCK == cur_lmbi_table_entry);
   fwaSOCK = cur_table_fwa;
   SetFullWord(cur_lmbi_table_entry+W_PWFWA, cur_table_fwa);
   SetHalfWord(cur_lmbi_table_entry+H_PWLE, LE_SOCK);
   SetHalfWord(cur_lmbi_table_entry+H_PWNE, MRR_N_SOCKETS);
   LogOut("Entry for SOCK = 0x%x; table FWA=%x", PW_SOCK, cur_table_fwa);
   cur_table_fwa += LE_SOCK*MRR_N_SOCKETS;
   cur_table_fwa = ALIGN_FULLWORD(cur_table_fwa);
   cur_lmbi_table_entry += L_LMBPT;

   assert(PW_DVSK == cur_lmbi_table_entry);
   fwaDVSK = cur_table_fwa;
   SetFullWord(cur_lmbi_table_entry+W_PWFWA, cur_table_fwa);
   SetHalfWord(cur_lmbi_table_entry+H_PWLE, 2);
   SetHalfWord(cur_lmbi_table_entry+H_PWNE, 5);
   LogOut("Entry for DVSK = 0x%x; table FWA=%x", PW_DVSK, cur_table_fwa);
   cur_table_fwa += 2*5;
   cur_table_fwa = ALIGN_FULLWORD(cur_table_fwa);
   cur_lmbi_table_entry += L_LMBPT;

   assert(PW_PORT == cur_lmbi_table_entry);
   fwaPORT = cur_table_fwa;
   SetFullWord(cur_lmbi_table_entry+W_PWFWA, cur_table_fwa);
   SetHalfWord(cur_lmbi_table_entry+H_PWLE, LE_PORT);
   SetHalfWord(cur_lmbi_table_entry+H_PWNE, 6);
   LogOut("Entry for PORT = 0x%x; table FWA=%x", PW_PORT, cur_table_fwa);
   cur_table_fwa += LE_PORT*6;
   cur_table_fwa = ALIGN_FULLWORD(cur_table_fwa);
   cur_lmbi_table_entry += L_LMBPT;

   assert(PW_PTBUF == cur_lmbi_table_entry);
   fwaPTBUF = cur_table_fwa;
   SetFullWord(cur_lmbi_table_entry+W_PWFWA, cur_table_fwa);
   nBytes = 2000;  /* total # of bytes for all circ lists */
   SetHalfWord(cur_lmbi_table_entry+H_PWLE, nBytes);
   SetHalfWord(cur_lmbi_table_entry+H_PWNE, 5);
   LogOut("Entry for PTBUF = 0x%x; table FWA=%x", PW_PTBUF, cur_table_fwa);
   cur_table_fwa += nBytes;
   cur_table_fwa = ALIGN_FULLWORD(cur_table_fwa);
   cur_lmbi_table_entry += L_LMBPT;

   assert(PW_MALC == cur_lmbi_table_entry);
   fwaMALC = cur_table_fwa;
   SetFullWord(cur_lmbi_table_entry+W_PWFWA, cur_table_fwa);
   SetHalfWord(cur_lmbi_table_entry+H_PWLE, LE_MALC);
   SetHalfWord(cur_lmbi_table_entry+H_PWNE, 5);
   LogOut("Entry for MALC = 0x%x; table FWA=%x", PW_MALC, cur_table_fwa);
   cur_table_fwa += LE_MALC*5;
   cur_table_fwa = ALIGN_FULLWORD(cur_table_fwa);
   cur_lmbi_table_entry += L_LMBPT;

   /* Carve out buffers from the end here and insert them
    * into the 80-char and 240-char buffer circular lists. */
   nSlots = GetHalfWord(fwaBF80+H_CIRCLIST_N_SLOTS_TOT);
   fwaBuffers80 = cur_table_fwa;
   for(islot=0; islot<nSlots; islot++) {
      AddToTopOfList(fwaBF80, cur_table_fwa);
      cur_table_fwa += LE_BF80;
   }

   nSlots = GetHalfWord(fwaBF240+H_CIRCLIST_N_SLOTS_TOT);
   fwaBuffers240 = cur_table_fwa;
   for(islot=0; islot<nSlots; islot++) {
      AddToTopOfList(fwaBF240, cur_table_fwa);
      cur_table_fwa += LE_BF240;
   }

   assert(PW_ALLOC == cur_lmbi_table_entry);
   fwaALLOC = cur_table_fwa;
   SetFullWord(cur_lmbi_table_entry+W_PWFWA, cur_table_fwa);
   SetHalfWord(cur_lmbi_table_entry+H_PWLE, LE_BF80);
   SetHalfWord(cur_lmbi_table_entry+H_PWNE, 5);
   LogOut("Entry for ALLOC = 0x%x; table FWA=%x", PW_ALLOC, cur_table_fwa);
   cur_table_fwa += LE_BF80*5;
   cur_table_fwa = ALIGN_FULLWORD(cur_table_fwa);
   cur_lmbi_table_entry += L_LMBPT;

   /* End of code generated by SetupLMBIPointerTable.awk. */

   /*//! This is a kludge to set a non-zero address.  Maybe I'll leave it in. */
   SetFullWord(fwaFPCOM+W_NBF80, AddrFRENDTo1FP(Get80()));
   SetFullWord(fwaFPCOM+W_NBF240, AddrFRENDTo1FP(Get240()));
}

/*--- function ReturnBuffersInReleaseList ----------------------
 *  Return buffers in the release list to their original 
 *  list of available buffers.  There are two such lists,
 *  for 80 and 240-byte buffers.
 *  Note:  FREND's approach is more complex.  A given piece
 *  of memory can be dynamically allocated as part of an
 *  80- or 240-byte buffer as needed.  A separate ID mechanism
 *  is used to keep track of what length buffer it is.
 *  This is not done in frend2.
 */
void ReturnBuffersInReleaseList()
{
   FrendAddr bufaddr;
   int nFreed=0;
   while(0 != (bufaddr=RemoveFromBottomOfList(fwaBFREL))) {
      if(bufaddr < fwaBuffers240) {
         AddToTopOfList(fwaBF80, bufaddr);
      } else {
         AddToTopOfList(fwaBF240, bufaddr);
      }
      nFreed++;
   }
   if(DebugL>=LL_SOME) LogOut("ReturnBuffersInReleaseList: returned %d buffers", nFreed);
}

/*--- function InitPortFirstTime ----------------------
 *  Initialize a port table entry.  Called only once per port at startup.
 *
 *  Entry:  fwaList  is the FWA of a place to create two consecutive
 *                   circular lists for this port (in and out buffers).
 *          portNum  is the port #.  It can be control or data.
 *  Exit:   Returns the number of PTBUF bytes allocated to this port.
 */
HalfWord InitPortFirstTime(FrendAddr fwaList, HalfWord portNum)
{
   HalfWord nBytes;
   HalfWord nInBufs, nOutBufs;
   char szmsg[80];
   if(portNum <= PTN_MAX) {
      /* control port */
      nInBufs = L_CPIN;
      nOutBufs = L_CPOT;
   } else {
      /* data port */
      nInBufs = L_DTIN;
      nOutBufs = L_DTOT;
   }
   sprintf(szmsg, "In bufs for port %d", portNum);
   nBytes = InitCircList(fwaList, nInBufs, szmsg);
   SetPortFullWord(portNum, W_PTINCL, fwaList);

   fwaList += nBytes;
   sprintf(szmsg, "Out bufs for port %d", portNum);
   nBytes += InitCircList(fwaList, nOutBufs, szmsg);
   SetPortFullWord(portNum, W_PTOTCL, fwaList);
   return nBytes;
}

/*--- function InitPortBufs ----------------------
 *  Initialize the circular lists for the ports, and the pointers
 *  from the ports to these circular lists.
 *  Then do the same for sockets.
 */
void InitPortBufs()
{
   /* Do ports. */
   HalfWord port;
   FrendAddr fwaList = fwaPTBUF;
   HalfWord nBytes = InitPortFirstTime(fwaList, PTN_MAN);
   fwaList += nBytes;
   for(port=FPORTCONSOLE; port<NPORTS; port++) {
      nBytes = InitPortFirstTime(fwaList, port);
      fwaList += nBytes;
   }
}

/*--- function InitSocks ----------------------
 */
void InitSocks()
{
   HalfWord sock;
   char szmsg[80];
   for(sock=FSOCKETCONSOLE; sock<NSOCKETS; sock++) {
      FrendAddr fwaThisSock = SockNumToFWA(sock);
      FrendAddr fwaListSock = fwaThisSock + W_SKOTCL;
      /* Initialize the circular list, which is actually part of the socket entry. */
      sprintf(szmsg, "socket %d out", sock);
      InitCircList(fwaListSock, L_SKOCL, szmsg);
      SetHalfWord(fwaThisSock+H_SKNUM, sock);
   }
}

/*--- function InitFREND ----------------------
 */
void InitFREND()
{
   InitLog("frend.log", "Fr");
   InitSockFromCyber();
	InitReplyToCyber();
   memset(&pFrendInt->FrendState, 0, sizeof(pFrendInt->FrendState));
   memset(SockTCPAry, 0, sizeof(SockTCPAry));
   InitLMBI();
   InitPortBufs();
   InitSocks();
   InitSockTCPListen();
   SetHalfWord(H_INICMP, 1);  /* Initialization complete */
}


/*---  Beginning of non-initialization code.  ----------------*/

/*--- function PUTBUF ----------------------
 *  Return a buffer to the free list.
 */
void PUTBUF(FrendAddr bufaddr)
{
   SetFullWord(bufaddr, 0);  /* Zero first word of buffer */
   AddToTopOfList(fwaBFREL, bufaddr);
}

/*--- function FMTOPEN ----------------------
*  Format an FP.OPEN message to send to 1FP, indicating a new connection.
*
*  FP.OPEN  8/PN, 8/OT, 16/OID, 8/DCP, 8/DID
*    PN = 7/32 DATA PORT NUMBER
*    OT = OPEN ORIGINATOR TYPE (OT.XX)
*    OID = ID SUPPLIED BY OPEN ORIGINATOR, (MEANT
*          TO BE RETURNED IN ORSP)
*    DCP = DESTINATION CONTROL PORT (CTL.X)
*    DID = DESTINATION TYPE (OT.X)
*
*  Entry:   CtlPortNum  is MANAGER's control port number (1)
*           DataPortNum is the data port number
*           SocketNum   is the socket number.
*/
FrendAddr FMTOPEN(HalfWord CtlPortNum, HalfWord DataPortNum, HalfWord SocketNum)
{
   FrendAddr addr = Get80();
   SetByte(addr+C_FPP5, (Byte8)CtlPortNum);
   SetByte(addr+C_FPPT, (Byte8)DataPortNum);
   SetByte(addr+C_FPP2, (Byte8)OT_1200);
   SetHalfWord(addr+C_FPP3, SocketNum);
   SetByte(addr+C_FPP6, 0);  /* DID = 0 */
   /* set fields in record header */
   SetByte(addr+C_DHBCT,NP_OPEN+LE_DTAHDR);
   SetByte(addr+C_DHTYPE, FP_OPEN);
   SetByte(addr+C_DHCHC, 0);
   SetByte(addr+C_DHCTL, 0);
   return addr;
}

/*--- function ADDPORT ----------------------
 *  Add a message buffer address to the output queue for a port.
 *  Corresponds to FREND's ADDPORT.
 *  Formerly named AddMsgToPort.
 */
void ADDPORT(HalfWord PortNum, FrendAddr fwaMsg)
{
   FrendAddr fwaMyPort = PortNumToFWA(PortNum);
   FrendAddr fwaList = GetFullWord(fwaMyPort+W_PTINCL);
   if(DebugL>=LL_SOME) {
      char szmsg[1024], szchars[256];
      int j, len=0, nbytes=(int) (0xff & GetByte(fwaMsg+C_DHBCT));
      szmsg[0] = szchars[0] = '\0';
      if(nbytes < L_DTAHDR) {
         LogOut("==** Error: ADDPORT: [C_DHBCT] = %d", nbytes);
      }
      for(j=0; j<nbytes; j++) len += sprintf(szmsg+len, "%-2.2x ", GetByte(fwaMsg+j));
      for(len=0,j=0; j<(nbytes-L_DTAHDR); j++) {
         char ch = (char)GetByte(fwaMsg+j+L_DTAHDR);
         if(!isprint(ch)) ch = '.';
         len += sprintf(szchars+len, "%c", ch);
      }
      LogOut("==ADDPORT: adding %xH to port %d code %s '%s' [%s]", fwaMsg, PortNum, 
         GetNameFromOffset(SymToNameRecordTypes, GetByte(fwaMsg+C_DHTYPE) & (0xffff^V_EXTREQ)),
         szchars, szmsg);
   }
   AddToTopOfList(fwaList, fwaMsg);

   /* Check to make sure that there is a non-zero buffer address
    * in W.PTIN for that control port.  If not, then remove a msg from
    * bottom of list and put in W.PTIN.  (I suppose in most cases, this
    * will be the message we just added.) */
   /* We should get and then drop H.PTINIK, but we'll ignore this interlock for now. */
   if(0 == GetFullWord(fwaMyPort + W_PTIN)) {
      /* No outbound buffer--so get one. */
      if(!ListIsEmpty(fwaList)) {
         FullWord bufAddr = RemoveFromBottomOfList(fwaList);
         bufAddr = AddrFRENDTo1FP(bufAddr);
         SetFullWord(fwaMyPort+W_PTIN, bufAddr);
         if(DebugL>=LL_SOME) LogOut("==ADDPORT: Put msg %xH (1FP format) in port %d's W.PTIN (%x)", bufAddr, PortNum, fwaMyPort + W_PTIN);
      }
   }
   if(DebugL>=LL_SOME) DumpPortEntry(PortNum);
}

/*--- function SENDPT ----------------------
 *  Send a buffer to a port.
 */
void SENDPT(HalfWord PortNum, FrendAddr fwaMySocket, FrendAddr fwaMsg)
{
   FrendAddr fwaMyPort = PortNumToFWA(PortNum);
   ADDPORT(PortNum, fwaMsg);
   TaskSENDCP(PortNum, FP_INBS);
}

/*--- function SocMsg ----------------------
 *  Queue up a message to send to a socket.
 *  *** not yet used
 */
void SocMsg(FrendAddr fwaMsg)
{
}

/*--- function GETINBF ----------------------
 *  Assign a new buffer to the socket input.
 */
FrendAddr GETINBF(FrendAddr fwaMySock)
{
   FrendAddr bufaddr = Get240();
   SetFullWord(fwaMySock+W_SKINBF, bufaddr);
   /* Empty buffer has length == header size */
   SetByte(bufaddr+C_DHBCT, L_DTAHDR);

   if(!HFLAGISSET(fwaMySock, SKINEL)) {
      /* The "No EOL" flag is not set, so set EOL flag in socket */
      SetByte(bufaddr+C_DHCNEW, V_DHCNEW);
   }
   /* Input char count */
   SetHalfWord(fwaMySock+H_SKINCC, 0);
   return bufaddr;
}

/*--- function GetFRENDVersionMsg ----------------------
 *  Entry:  Returns the address of a freshly-allocated 80-byte 
 *          message containing text to show to the user.
 */
FrendAddr GetFRENDVersionMsg(HalfWord SocketNum)
{
   FullWord len;
   char szDateTime[24], szVersionMsg[80];
   time_t mytime=time(NULL);
   struct tm *ptm=localtime(&mytime);
   /* Should look like:
    *  ddddddddddtttttttttt MSU-Frend   xx.yy   ssssssssss    pppppppp
    * 123456789a123456789b123456789c123456789d123456789e123456789f123456789
    * where the dddddddddd date starts with a blank.
    * This apparently does not match 100% with the text in ISUG p 1.3.
    */
   strftime(szDateTime, sizeof(szDateTime), "%m/%d/%y %H:%M:%S", ptm);
   len = sprintf(szVersionMsg, "  %s  MSU-Frend2  %s   Socket=%3d", szDateTime, szFRENDVersion, SocketNum);
   return GetBufferForC(szVersionMsg);
}

/*--- function WriteToSocketWithCC ----------------------
 *  This needs to be reworked eventually.
 *  It's similar to "CARRC     SUBR" and "INTCC     SUBR"
 *  but probably overly simplistic.
 */
void WriteToSocketWithCC(HalfWord SocketNum, FrendAddr fwaMySocket, FrendAddr fwaMsg)
{
   int ic, len = GetByte(fwaMsg+C_DHBCT), start=L_DTAHDR;
   Byte8 CarrCtl=0; /*//! debug */
   TypBool bDoCarrCtl=TRUE;
   TypPendingBuffer *pbuf = &(SockTCPAry[SocketNum].stcp_buf);
   int noutbytes=0, bytes_sent;
   if(!EOLL) {
      CarrCtl = GetByte(fwaMsg+C_DHCNEW);
      bDoCarrCtl = FALSE;
      if(CarrCtl & V_DHCNEW) bDoCarrCtl=TRUE;
   }
   /*//! not sure about length check here--but we mustn't treat garbage as CC */
   if(bDoCarrCtl && (len>L_DTAHDR)) {
      Byte8 cc = GetByte(fwaMsg+L_DTAHDR);
      start++;
      if('0'==cc) {
         pbuf->spb_buf[noutbytes++] = '\r';
         pbuf->spb_buf[noutbytes++] = '\n';
         pbuf->spb_buf[noutbytes++] = '\n';
      } else {
         /* This case is mostly for space, but it appears that MANAGER
          * sends 'D' as a CC when you enter linenum=text, so we treat
          * ALL other characters as a space carriage control. */
         pbuf->spb_buf[noutbytes++] = '\r';
         pbuf->spb_buf[noutbytes++] = '\n';
      }
   }
   if(DebugL>=LL_SOME) {
      char szmsg[256], thisch, ibuf=0;
      for(ic=L_DTAHDR; ic<len; ic++, ibuf++) {
         thisch = GetByte(fwaMsg+ic);
         szmsg[ibuf] = isprint(thisch) ? thisch : '!';
      }
      szmsg[ibuf] = '\0';
      LogOut("==WriteToSocketWithCC: '%s' I put %d cc chars.  EOLL=%d C_DHCNEW&V_DHCNEW=%xH C_DHCNEW=%xH", szmsg, noutbytes, EOLL, CarrCtl&V_DHCNEW, GetByte(fwaMsg+C_DHCNEW));
   }
   /* Now output the data bytes in the line */
   for(ic=start; ic<len; ic++) {
      pbuf->spb_buf[noutbytes++] = GetByte(fwaMsg+ic);
   }
   bytes_sent = SendToFSock(SocketNum, pbuf->spb_buf, noutbytes);
   if(bytes_sent <= 0) {
      if(EWOULDBLOCK == GetLastOSError()) {
         bytes_sent = 0;
      }
   }
   pbuf->spb_first = bytes_sent;
   pbuf->spb_chars_left = noutbytes - bytes_sent;

   PUTBUF(fwaMsg);

   /* If we sent the entire buffer,
    * simulate a CCB end-of-output interrupt by calling the
    * socket output task again to send the next line.
    * If there's no more data to be sent, it won't do anything.
    * If there IS more data to be sent, don't do anything.
    * We'll rely on the select() on writefds to restart the send.
    */
   if(0 == pbuf->spb_chars_left) {
      TaskSKOTCL(SocketNum, fwaMySocket);
   }
}

/*--- function OTNEUP ----------------------
 *  UPDATE PTOTNE FIELD IN PORT.
 *  IF OUTPUT LIST HAS L.DTOT FREE SLOTS,
 *     SEND FP.OTBS.
 */
void OTNEUP(HalfWord PortNum, FrendAddr fwaMyPort) 
{
   FrendAddr fwaList = GetFullWord(fwaMyPort+W_PTOTCL);
   HalfWord nSlotsAvail = GetListFreeEntries(fwaList);
   SetHalfWord(fwaMyPort+H_PTOTNE, nSlotsAvail);
   if(nSlotsAvail >= L_DTOT) {
      TypBool bSendOTBS=FALSE;
      /* All port slots are available.  */
      /* Tell MANAGER unless there's an OTBS already pending. */
      INTRLOC(fwaMyPort+H_PTNDIK);  /* Not sure I really need this */
      if(!HFLAGISSET(fwaMyPort, PTOTBS)) {
         /* There is NOT an OTBS already pending. */
         SETHFLAG(fwaMyPort, PTOTBS);
         bSendOTBS=TRUE;
      }
      DropIL(fwaMyPort+H_PTNDIK);

      if(bSendOTBS) TaskSENDCP(PortNum, FP_OTBS);
   }
}

/*--- function READPT ----------------------
 *  Try to get a line from the port list, if available.
 *  Exit:   Returns a buffer obtained from the port, else 0.
 */
FrendAddr READPT(HalfWord SocketNum, FrendAddr fwaMySocket)
{
   FrendAddr bufaddr=0, fwaMyPort, fwaList;
   do { /* NOT a loop--just a way to break out of a block */
      HalfWord PortNum = GetHalfWord(fwaMySocket+H_SKCN1);
      if(0==PortNum) break;
      PORTNUM = PortNum;   /* Save for later.  Kludge is from FREND! */
      fwaMyPort = PortNumToFWA(PortNum);
      fwaList = GetFullWord(fwaMyPort+W_PTOTCL);
      bufaddr = RemoveFromBottomOfList(fwaList);
      if(DebugL>=LL_SOME) LogOut("==READPT: removed %x from port %d; nFree now %d", bufaddr, PortNum, GetListFreeEntries(fwaList));
      if(!bufaddr) break;
      /* We removed a buffer.  Update H_PTOTNE */
      OTNEUP(PortNum, fwaMyPort);
   } while(FALSE);
   return bufaddr;
}

/*--- function GETDATA ----------------------
 *  Get the next output buffer destined for this socket.
 *  Entry:  fwaMySocket points to the socket.
 *
 *  Exit:   returns the fwa of a buffer to send, else 0 if none.
 */
FrendAddr GETDATA(HalfWord SocketNum, FrendAddr fwaMySocket)
{
   FrendAddr bufaddr;
   FrendAddr fwaList = fwaMySocket+W_SKOTCL;  /* Not a pointer */
   Byte8 CharCode=0xe5, CtlFlags=0xe5 /*//! debug */;
   EOLL = FALSE;
   PORTNUM = 0;

   /* Try to get next line in socket */
   bufaddr = RemoveFromBottomOfList(fwaList);
   /* If nothing in socket; try to get from port */
   if(!bufaddr) bufaddr = READPT(SocketNum, fwaMySocket);

   if(bufaddr) {
      /*//! I added this code because during LOGIN to a restricted user,
       * a front-end command is sent in between two halves of a line.
       * Don't process EOL flags if it's a FE command.
       */
      Byte8 rectype = GetByte(bufaddr+C_DHTYPE);
      if(!(FP_FECNE == rectype || FP_FEC == rectype)) {
         CtlFlags = GetByte(bufaddr+C_DHCTL);
         EOLL = HFLAGISSET(fwaMySocket, SKOEOL);
         /* CLEAR PREVIOUS EOL FLAG */
         CLEARHFLAG(fwaMySocket, SKOEOL);
         /* Set EOL flag based on fields in header.  Weird stuff. */
         CharCode = GetByte(bufaddr+C_DHCHC);
         if(CC_FDCAS == CharCode || CC_FDCBI==CharCode) {
            SETHFLAG(fwaMySocket, SKOEOL);
         } else {
            if(CtlFlags & V_DHCEOL) SETHFLAG(fwaMySocket, SKOEOL);
         }
      }
   }
   if(DebugL>=LL_SOME) LogOut("==GETDATA for sock %d returning %xH EOLL=%x CtlFlags=%xH CharCode=%xH SKOEOL=%xH", SocketNum, bufaddr, EOLL, CtlFlags, CharCode, GetHalfWord(fwaMySocket+H_SKOEOL));
   return bufaddr;
}

/*--- function CKTHRSH ----------------------
 *  CHECK PORT DATA THRESHOLD
 *  This is a simplication of the FREND routine "CKTHRSH   SUBR".
 *  It assumes the port is interactive.
 *  EXIT     Returns  1 IF THE PORT IS BELOW NEED-DATA THRESHOLD
 *                    0 IF THE PORT IS ABOVE THRESHOLD
 */
int CKTHRSH(FrendAddr fwaMyPort)
{
   HalfWord nEmptySlots = GetHalfWord(fwaMyPort+H_PTOTNE);
   int BelowThres=0;
   if(nEmptySlots >= L_DTOT) BelowThres=1;
   return BelowThres;
}

/*--- function SendOTBSIfNecessary ----------------------
 */
void SendOTBSIfNecessary(HalfWord portNum, FrendAddr fwaMyPort, TypBool bIsExternal)
{
   /* Don't send an OTBS unless either CHTHRSH says to,
    * or PTOTBS is set.  */
   TypBool bSendOTBS=FALSE;
   TypBool bBelowThres = CKTHRSH(fwaMyPort);
   Byte8 MsgCode = FP_OTBS;
   INTRLOC(fwaMyPort+H_PTNDIK);
   if(!HFLAGISSET(fwaMyPort, PTOTBS) || bBelowThres) {
      SETHFLAG(fwaMyPort, PTOTBS);
      bSendOTBS=TRUE;
   }
   DropIL(fwaMyPort+H_PTNDIK);
   if(bIsExternal) MsgCode |= V_EXTREQ;
   if(bSendOTBS) TaskSENDCP(portNum, MsgCode);
}

/*---  Routines that were tasks in the original FREND --------*/

/*--- function TaskMSGCP ----------------------
 *  SEND A PRE-FORMATTED MESSAGE TO CONTROL PORT.
 *  This is basically a wrapper to ADDPORT.
 */
void TaskMSGCP(HalfWord PortNum, FrendAddr fwaMsg)
{
   ADDPORT(PortNum, fwaMsg);
}

/*--- function CHKACT ----------------------
 *  Check for output activity.  
 *  The purpose is to check to see if the socket is too busy
 *  to take another line.  frend2 needs to do this a bit differently
 *  than FREND due to the difference between TCP sockets (which
 *  are buffered by the application) and serial port CCBs
 *  (which are apparently buffered by the hardware).
 *
 *  Exit:   Returns  0 if we are too busy to send another line.
 */
FullWord CHKACT(HalfWord SocketNum, FrendAddr fwaMySocket)
{
   FullWord status=1;
   TypPendingBuffer *pbuf = &(SockTCPAry[SocketNum].stcp_buf);
   if(pbuf->spb_chars_left) status = 0;
   return status;
}

/*--- function TaskSKOTCL ----------------------
 *  Socket Output Control.
 *  Gets a buffer of data for this socket and sends it to the terminal.
 */
void TaskSKOTCL(HalfWord SocketNum, FrendAddr fwaMySocket)
{
   FrendAddr bufaddr;
   /* If there are pending output characters, don't send more lines. */
   if(0==CHKACT(SocketNum, fwaMySocket)) {
      return;
   }

   bufaddr=GETDATA(SocketNum, fwaMySocket);
   if(bufaddr) {
      Byte8 rectype = GetByte(bufaddr+C_DHTYPE);
      if(FP_BULK == rectype) {
         SETHFLAG(fwaMySocket, SKSUPE); /* Set Suppress Echo flag */
      }
      if(FP_FECNE == rectype || FP_FEC == rectype) {
         char szBuf[256];
         Byte8 len=GetByte(bufaddr+C_DHBCT)-L_DTAHDR;
         memcpy(szBuf, &pFrendInt->FrendState.fr_mem[bufaddr+L_DTAHDR], len);
         szBuf[len] = '\0';
         LogOut("==TaskSKOTCL: ignoring FECMD %s", szBuf);
         /* //! Should we call PUTBUF with bufaddr here? */
         PUTBUF(bufaddr);
      } else {
         WriteToSocketWithCC(SocketNum, fwaMySocket, bufaddr);
      }
   }
}

/*--- function TaskSOCMSG ----------------------
 * Cause a message to be sent to a socket.
 */
void TaskSOCMSG(HalfWord SocketNum, FrendAddr fwaMsg)
{
   FullWord nchars = (FullWord) GetByte(fwaMsg+C_DHBCT) - L_DTAHDR;
   SendToFSock(SocketNum, &pFrendInt->FrendState.fr_mem[fwaMsg+L_DTAHDR], nchars);
   SendToFSock(SocketNum, (Byte8 *)"\r\n", 2);
   /* //! really should call this: TaskSKOTCL(SocketNum, fwaMsg); */
}

/*--- function CLRSOC ----------------------
 *  CLeaR SOCket.  See "CLRSOC    SUBR".
 */
void CLRSOC(HalfWord SocketNum, FrendAddr fwaMySocket)
{
   FrendAddr bufaddr;
   SetHalfWord(fwaMySocket+H_SKID, 0);
   SetFullWord(fwaMySocket+W_SKFLAG, 0); /* Clear all flags */

   /* RETURN ALL BUFFERS ON THE OUTPUT STACK */
   do {
      bufaddr = RemoveFromBottomOfList(fwaMySocket+W_SKOTCL);
      if(!bufaddr) break;
      PUTBUF(bufaddr);
   } while(TRUE);

   /* RETURN INPUT BUFFER */
   bufaddr = GetFullWord(fwaMySocket+W_SKINBF);
   if(bufaddr) {
      SetFullWord(fwaMySocket+W_SKINBF, 0);
      PUTBUF(bufaddr);
   }
}

/*--- function LMSOCK ----------------------
 *  Deliver all lines of the login msg to the socket.
 */
void LMSOCK(HalfWord SocketNum)
{
   /* //! flesh this out. */
}

/*--- function SETPORT ----------------------
 * SETUP A FRESH PORT TABLE ENTRY
 */
void SETPORT(HalfWord PortNum, HalfWord ctlPortNum)
{
   FrendAddr fwaList, fwaMyPort = PortNumToFWA(PortNum);
   HalfWord nBufs;
   /* Look for "SETPORT   SUBR" in FREND for the code that inspired what's below. */
   SETHFLAG(fwaMyPort, PTSENB);
   SETHFLAG(fwaMyPort, PTSCNT);
   SETHFLAG(fwaMyPort, PTS65);
   SETHFLAG(fwaMyPort, PTEOL);
   CLEARHFLAG(fwaMyPort, PTOTBS);
   CLEARHFLAG(fwaMyPort, PTXFER);
   /* I don't know the difference between port ID and port number.
    * I will treat them the same. */
   SetHalfWord(fwaMyPort+H_PTID, PortNum);  /* port ID */
   SetHalfWord(fwaMyPort+H_PTCPN, PTN_MAN);  /* set control port number */
   SetFullWord(fwaMyPort+W_PTIN, 0);
   SetFullWord(fwaMyPort+W_PTOT, 0);
   SetFullWord(fwaMyPort+W_PTPBUF, 0);  /* NO PARTIAL LINE BUFFER */
   fwaList = GetFullWord(fwaMyPort+W_PTOTCL);   /* fwa of circ buffer for output */
   nBufs = GetHalfWord(fwaList+H_CLNUM);  /* total num of buffers in list */
   SetHalfWord(fwaMyPort+H_PTOTNE, nBufs);   /* Set all bufs as available */
   DropIL(fwaMyPort+H_PTINIK);
   DropIL(fwaMyPort+H_PTOTIK);
   DropIL(fwaMyPort+H_PTWTBF);
   DropIL(fwaMyPort+H_PTNDIK);
   CLEARHFLAG(fwaMyPort, PTTNX3);
}

/*--- function FINDPT ----------------------
 *  FIND AN EMPTY DATA PORT.
 *  For now, we just use port number == socket number.
 */
HalfWord FINDPT(HalfWord SocketNum)
{
   HalfWord portNum = SocketNum;
   return portNum;
}

/*--- function LINKSOC ----------------------
 *  LINK SOCKET TO PORT
 */
void LINKSOC(HalfWord SocketNum, HalfWord PortNum)
{
   /* Link socket and port */
   FrendAddr fwaMyPort = PortNumToFWA(PortNum);
   FrendAddr fwaMySocket = SockNumToFWA(SocketNum);
   SetHalfWord(fwaMySocket+H_SKCN1, PortNum);
   SetByte(fwaMySocket+C_SKCT1, CT_PORT);

   SetHalfWord(fwaMyPort+H_PTCN1, SocketNum);
   SetByte(fwaMyPort+C_PTCT1, CT_SOCK);
   CLEARHFLAG(fwaMySocket, SKSUPE);  /* Clear suppress echo */
}

/*--- function TaskCONMSG ----------------------
 *  SEND A MESSAGE TO WHATEVER IS CONNECTED TO A PORT
 */
void TaskCONMSG(HalfWord PortNum, FrendAddr fwaMsg)
{
   FrendAddr fwaMyPort = PortNumToFWA(PortNum);
   HalfWord SocketNum = GetHalfWord(fwaMyPort+H_PTCN1);
   TaskSOCMSG(SocketNum, fwaMsg);
}

/*--- function SendCP_OTBS ----------------------
 *  Helper routine for SENDCP to send an FP.OTBS (OuTput Buffer Status)
 *
 *  Exit:   Returns 0 if should not send message now; buffer is released
 *                and task should be called again later.
 *             -1  if message should never be sent.  Buffer is released.
 *             else address of buffer to send.      
 */
FrendAddr SendCP_OTBS(HalfWord portNum, FrendAddr fwaMyPort, FrendAddr bufaddr)
{
   FrendAddr result_addr=bufaddr;
   /*//! I have ignored some interlocking of H.PTNDIK and other complexity. */
   /* See "OTBS      SUBR" in FREND. */
   /* Get number of free entries in this port. */
   HalfWord nFree = GetHalfWord(fwaMyPort+H_PTOTNE);
   SetByte(bufaddr+C_FPP2, (Byte8) nFree);
   if(DebugL>=LL_SOME) {
      HalfWord SocketNum = GetHalfWord(fwaMyPort+H_PTCN1);
      FrendAddr fwaMySocket = SockNumToFWA(SocketNum);
      FrendAddr fwaList = GetFullWord(fwaMyPort+W_PTOTCL);
      LogOut("==SendCP_OTBS port %d's H_PTOTNE=%u; W_PTOTCL nFree=%d", portNum, GetHalfWord(fwaMyPort+H_PTOTNE), GetListFreeEntries(fwaList));
   }
   /* Set buffer length based on # of parameters (2) */
   SetByte(bufaddr+C_DHBCT, L_DTAHDR+NP_OTBS);
   return result_addr;
}

/*--- function TaskSENDCP ----------------------
 *  Send a message to a control port.
 */
void TaskSENDCP(HalfWord PortNum, Byte8 MsgCode)
{
   /*//! I'm not sure I'm handling V_EXTREQ right.
    * I am basically ignoring it. */
   Byte8 MsgCodeWithoutFlag = (Byte8)(MsgCode & (0xff ^ V_EXTREQ));
   FrendAddr fwaMyPort = PortNumToFWA(PortNum);
   HalfWord ctlPort = GetHalfWord(fwaMyPort+H_PTCPN);
   FrendAddr bufaddr = Get80();

   /* Set the message code, clearing the V_EXTREQ bit. */
   SetByte(bufaddr+C_DHTYPE, MsgCodeWithoutFlag);

   /* Format the message as per the message code */
   if(FP_INBS == MsgCodeWithoutFlag) {
      /* Input Buffer Status */
      /* Set param 2 to # of lines ready for input to 1FP */
      FrendAddr fwaList = GetFullWord(fwaMyPort+W_PTINCL);
      HalfWord nSlotsUsed = GetHalfWord(fwaList+H_CIRCLIST_N_USED);
      /* Count the buffer in W.PTIN if it's non-zero because if it's
       * there, it has been removed from the circular list. */
      if(GetFullWord(fwaMyPort + W_PTIN) > 0) nSlotsUsed++;
      SetByte(bufaddr+C_FPP2, (Byte8)nSlotsUsed);
      SetByte(bufaddr+C_DHBCT, L_DTAHDR+2);
   } else if(FP_OTBS == MsgCodeWithoutFlag) {
      SendCP_OTBS(PortNum, fwaMyPort, bufaddr);
   } else if(FP_CLO == MsgCodeWithoutFlag) {
      SetByte(bufaddr+C_DHBCT, L_DTAHDR+2);
      SetByte(bufaddr+C_FPP2, 2); /* this means DISCONNECT */
   }
   /* Send message to control port */
   SetByte(bufaddr+C_FPPT, (Byte8) PortNum);  /* data port number */
   SetByte(bufaddr+C_DHCHC, 0);
   SetByte(bufaddr+C_DHCTL, 0);
   ADDPORT(ctlPort, bufaddr);
}

/*--- function TaskSKINCL ----------------------
 *  Socket Input Control - handles lines typed by user.
 */
void TaskSKINCL(FrendAddr fwaMySocket, FrendAddr bufaddr)
{
   HalfWord PortNum = GetHalfWord(fwaMySocket+H_SKCN1);
   /* Clear "suppress echo" flag. */
   CLEARHFLAG(fwaMySocket,SKSUPE);
   SENDPT(PortNum, fwaMySocket, bufaddr);
}

/*--- function DOABT ----------------------
 *  Send an FP.ABT request, typically because the user hit Esc.
 */
void DOABT(HalfWord portNum, FrendAddr fwaMyPort)
{
   /* Grab a buffer and format a FP.ABT message in it, then send it. */
   HalfWord ctlPort = GetHalfWord(fwaMyPort+H_PTCPN);
   FrendAddr bufaddr = Get80();
   SetByte(bufaddr+C_DHBCT,L_DTAHDR+1);
   SetByte(bufaddr+C_DHTYPE, FP_ABT);
   SetByte(bufaddr+C_FPPT, (Byte8)portNum);
   ADDPORT(ctlPort, bufaddr);
}

/*--- function ZAPPTO ----------------------
 *  Discard all output lines for this port, that do not
 *  have the NTA flag set.
 */
void ZAPPTO(HalfWord portNum, FrendAddr fwaMyPort)
{
   FrendAddr fwaInterlock = fwaMyPort+H_PTOTIK;
   FrendAddr bufaddr;
   HalfWord nSlotsUsed;
   if(InterlockIsFree(fwaInterlock)) {
      FrendAddr fwaList = GetFullWord(fwaMyPort+W_PTOTCL);
      INTRLOC(fwaInterlock);
      nSlotsUsed = GetListUsedEntries(fwaList);
      /* It appears that nSlotsUsed is always zero in the console version,
       * so this FREND-derived code is not necessary.  It might be
       * necessary in the TCP/IP version. */
      for(; nSlotsUsed > 0; nSlotsUsed--) {
         bufaddr = RemoveFromBottomOfList(fwaList);
         if(!bufaddr) break;  /* should never happen */
         if(V_DHCNTA & GetByte(bufaddr+C_DHCNTA)) {
            /* Line should not be discarded, so add it back. */
            AddToTopOfList(fwaList, bufaddr);
         } else {
            /* Discard output line. */
            PUTBUF(bufaddr);
         }
      }
      /* END OF LIST. UPDATE PTOTNE. */
      SetHalfWord(fwaMyPort+H_PTOTNE, GetListFreeEntries(fwaList));
      DropIL(fwaInterlock);

      /* SEND AN OTBS TO THE CONTROL PORT IF ONE HAS NOT
       * ALREADY BEEN SENT. */
      SendOTBSIfNecessary(portNum, fwaMyPort, FALSE);
   }
}

/*--- function TaskINESC ----------------------
 */
void TaskINESC(HalfWord SocketNum)
{
   FrendAddr fwaMySocket = SockNumToFWA(SocketNum);
   HalfWord PortNum = GetHalfWord(fwaMySocket+H_SKCN1);
   if(PortNum) {
      FrendAddr fwaMyPort = PortNumToFWA(PortNum);
      /* Clear pending output lines on port. */
      ZAPPTO(PortNum, fwaMyPort);
      /*//! Should clear port's input here. */
      DOABT(PortNum, fwaMyPort);
   }
   CLEARHFLAG(fwaMySocket, SKESCP); /* ALLOW USER ANOTHER ESCAPE */
}

/*--- function TaskOPENSP ----------------------
 *  OPEN SOCKET TO PORT.
 */
void TaskOPENSP(HalfWord SocketNum, HalfWord ctlPortNum, HalfWord OpenType)
{
   /* Don't worry about OT.XX or BL.xx "baud rate" codes. */
   HalfWord portNum;
   LMSOCK(SocketNum);
   portNum = FINDPT(SocketNum);
   SETPORT(portNum, PTN_MAN);
   LINKSOC(SocketNum, portNum);

   /* Create message to send to 1FP */
   {
      FrendAddr fwaMsg = FMTOPEN(PTN_MAN, portNum, SocketNum);
      TaskMSGCP(PTN_MAN, fwaMsg);
   }
}

/*--- function TaskSKINIT ----------------------
 *  SocKet INITialize.  See "SKINIT    TASK".
 */
void TaskSKINIT(HalfWord SocketNum)
{
   FrendAddr addr, fwaMySocket = SockNumToFWA(SocketNum);
   FullWord save;
   SetByte(fwaMySocket+C_SKNPCC, '%');
   SetHalfWord(fwaMySocket+H_SKINLE, L_LINE);  /* max input line length */

   /* CLEAR THE REST OF THE SOCKET, EXCEPT FOR "SKPORD". */
   save = GetFullWord(fwaMySocket+W_SKPORD);
   for(addr=fwaMySocket+W_SKFLAG; addr<=fwaMySocket+H_CLTOP; addr += 4) {
      SetFullWord(addr, 0);
   }
   SetFullWord(fwaMySocket+W_SKPORD, save);

   /* INITIALIZE THE SOCKET OUTPUT CIRCULAR LIST */
   /*         (MOSTLY ALREADY DONE) */
   SetHalfWord(fwaMySocket+W_SKOTCL+H_CLNUM, L_SKOCL);

   /* SET SOCKET INPUT STATE TO =IDLE= */
   SetByte(fwaMySocket+C_SKISTA, IN_IDLE);
}

/*--- function TaskSKOPEN ----------------------
 *  Open a FREND socket.
 */
void TaskSKOPEN(HalfWord SocketNum)
{
   FrendAddr bufaddr, fwaMySocket = SockNumToFWA(SocketNum);
   SetByte(fwaMySocket+C_SKISTA, IN_IO); /* set input state */

   TaskSKINIT(SocketNum);

   /* This code is from task SKWTNQ */
   TaskOPENSP(SocketNum, PTN_MAN, OT_1200);
   bufaddr = GetFRENDVersionMsg(SocketNum);
   TaskSOCMSG(SocketNum, bufaddr);
}

/*--- function TaskSKCARR ----------------------
 *  "Carrier" has been detected (i.e., we are accepting a new connection).
 */
void TaskSKCARR(HalfWord SocketNum)
{
   FrendAddr fwaMySocket = SockNumToFWA(SocketNum);
   /* //! for the time being, we are setting socketID = socket num */
   SetHalfWord(fwaMySocket+H_SKID, SocketNum);
   TaskSKOPEN(SocketNum);
}

/*--- function DRPCON ----------------------
 *  Drop a socket's connections.
 *
 *  Entry:  callingConn is the # of the port or socket which
 *                owns the connections.
 *          
 */
void DRPCON(HalfWord callingConn, Byte8 connType, HalfWord numPortOrSock)
{
   if(DebugL>=LL_SOME) LogOut("==DRPCON: callingConn=%d, connType=%d, numPortOrSock=%d",callingConn, connType, numPortOrSock);
   /*//! We depart from FREND code and assume that callingConn is a socket,
    * and that we need to close it. */
   /* It appears that connType is 0 when we are called due to LOGOUT,
    * and CT_PORT when we are called due to client disconnect. */
   if(CT_PORT == connType) {
      FrendAddr fwaMyPort = PortNumToFWA(numPortOrSock);
      SetByte(fwaMyPort+C_PTCT1, 0);
      SetHalfWord(fwaMyPort+H_PTCN1, 0);
      CLEARHFLAG(fwaMyPort, PTSCNT);
      /*//! FREND checks for waiting output before deciding to call SENDCP */
      TaskSENDCP(numPortOrSock, FP_CLO);
   }
   ClearTCPSockForFSock(callingConn);
}

/*--- function TaskCLOFSK ----------------------
 *  CLOSE FROM SOCKET (DISCONNECT)
 */
void TaskCLOFSK(HalfWord SocketNum, FrendAddr fwaMySocket)
{
   if(0 == SocketNum) return;
   /*//! We should check for pending output and call ourself
    * with a delay if necessary. */
   DRPCON(SocketNum, GetByte(fwaMySocket+C_SKCT1), GetHalfWord(fwaMySocket+H_SKCN1));
   /* Clear the connection */
   SetByte(fwaMySocket+C_SKCT1, 0);
   SetHalfWord(fwaMySocket+H_SKCN1, 0);

   CLRSOC(SocketNum, fwaMySocket);
   TaskSKINIT(SocketNum);
}

/*--- function CLRPTS ----------------------
 *  CLEAR PORT TO SOCKET CONNECTION IN THE SOCKET.
 *  This is a simplified version of "CLRPTS    SUBR";
 *  it ignores the second connection in the socket.
 */
void CLRPTS(HalfWord portNum, FrendAddr fwaMyPort,
            HalfWord SocketNum, FrendAddr fwaMySocket)
{
   SetHalfWord(fwaMySocket + H_SKCN1, 0);
   SetByte(fwaMySocket + C_SKCT1, 0);
}

/*--- function CLRPORT ----------------------
 *  CLEAR THE PORT (upon logout or disconnect).
 *  See "CLRPORT   SUBR".
 *  Exit:   Returns the number of FP.CLO BUFFERS found.
 *          Apparently if this is non-zero, it means that
 *          we are going to have to send a FP.CLO.
 */
HalfWord CLRPORT(HalfWord portNum, FrendAddr fwaMyPort)
{
   HalfWord nFPCLO=0;
   FrendAddr fwaList, bufaddr;
   if(HFLAGISSET(fwaMyPort, PTNDCL)) {
      nFPCLO = 1;
   }
   /* Clear key fields */
   SetHalfWord(fwaMyPort+H_PTFLAG, 0);
   SetHalfWord(fwaMyPort+H_PTFLG2, 0);
   SetByte(fwaMyPort+C_PTTYPE, 0);
   SetByte(fwaMyPort+C_PTCT1, 0);
   SetHalfWord(fwaMyPort+H_PTCN1, 0);
   SetHalfWord(fwaMyPort+H_PTID, 0);
   SetHalfWord(fwaMyPort+H_PTCPN, 0);
   DropIL(fwaMyPort+H_PTWTBF);
   CLEARHFLAG(fwaMyPort, PTPEOI);  /* Printer EOI */

   /* Return output buffers */
   INTRLOC(fwaMyPort+H_PTOTIK);  /* INTERLOCK OUTPUT SIDE */
   fwaList = GetFullWord(fwaMyPort+W_PTOTCL);
   do {
      bufaddr = RemoveFromBottomOfList(fwaList);
      if(!bufaddr) break;
      if(FP_CLO == GetByte(bufaddr+C_DHTYPE)) {
         nFPCLO++;   /* Increment # of FP.CLO's that we found */
      }
      PUTBUF(bufaddr);  /* Return output buffer */
   } while(TRUE);
   DropIL(fwaMyPort+H_PTOTIK);

   /* Return the port's input buffers */
   INTRLOC(fwaMyPort+H_PTINIK);
   fwaList = GetFullWord(fwaMyPort + W_PTINCL);
   bufaddr = GetFullWord(fwaMyPort+W_PTIN);
   if(bufaddr) {
      /* We have a next input line.  */
      /* Get its address in FREND format */
      bufaddr = Addr1FPToFREND(bufaddr);
      PUTBUF(bufaddr);
      SetFullWord(fwaMyPort+W_PTIN, 0);  /* Clear input line address */
   }
   /* Loop through input buffers, returning them. */
   do {
      bufaddr = RemoveFromBottomOfList(fwaList);
      if(!bufaddr) break;  /* Break if no more buffers in input list */
      PUTBUF(bufaddr);
   } while(TRUE);
   DropIL(fwaMyPort+H_PTINIK);

   return nFPCLO;
}

/*--- function TaskCLOFPT ----------------------
 *  CLOSE FROM PORT (LOGOUT).
 *  This is based on "CLOFPT    TASK", but much simpler.
 *  //! FREND does more, like checking for pending output,
 *   and checking port type.
 */
void TaskCLOFPT(HalfWord portNum, Byte8 CloseType)
{
   FrendAddr fwaMyPort = PortNumToFWA(portNum);
   if(CT_SOCK == GetByte(fwaMyPort+C_PTCT1)) {
      HalfWord SocketNum = GetHalfWord(fwaMyPort+H_PTCN1);
      FrendAddr fwaMySocket = SockNumToFWA(SocketNum);
      CLEARHFLAG(fwaMySocket, SKSWOT); /* CLEAR OUTPUT WAIT FOR CLOSE */
      CLRPTS(portNum, fwaMyPort, SocketNum, fwaMySocket);
      CLRPORT(portNum, fwaMyPort);
      TaskCLOFSK(SocketNum, fwaMySocket);
   }
}

/*--- function PTMSG ----------------------
 *  Issue a "[PORT  xx]" message to the port.
 */
void PTMSG(HalfWord PortNum)
{
   char szmsg[64];
   int len=sprintf(szmsg, "[Port%4d]", PortNum);
   FrendAddr bufaddr=GetBufferForC(szmsg);
   TaskCONMSG(PortNum, bufaddr);
}

/*--- function ProcRecTypeCPOPN ----------------------
 *  Process a CPOPN (Control Port OPeN) record type.
 *  Entry:  portNum  is the port number (always 1 for MANAGER).
 *          bufaddr  is the FREND address of the record from 1FP.
 *                   It starts with a data buffer header, then
 *                   C_FPPT, etc.
 */
void ProcRecTypeCPOPN(HalfWord portNum, FrendAddr bufaddr)
{
   FrendAddr fwaMyPort = PortNumToFWA(portNum);
   SETHFLAG(fwaMyPort, PTS65); /* Set "CONNECTED TO 6500" */

   /* Echo message back to the mainframe. */
   /*//! System hangs if I include this.  Should be a 
    * TaskMSGCP(), but that's just a wrapper to ADDPORT */
   /* ADDPORT(portNum, bufaddr); */

   /* This PUTBUF does not appear in "CPOPN     SUBR", which
    * is slightly more complex. */
   PUTBUF(bufaddr);
}

/*--- function ProcRecTypeCPCLO ----------------------
 *  Process a CPCLO (Control Port CLOse) record type.
 *  Entry:  portNum  is the port number (always 1 for MANAGER).
 *          bufaddr  is the FREND address of the record from 1FP.
 *                   It starts with a data buffer header, then
 *                   C_FPPT, etc.
 */
void ProcRecTypeCPCLO(HalfWord portNum, FrendAddr bufaddr)
{
   FrendAddr fwaMyPort = PortNumToFWA(portNum);
   PUTBUF(bufaddr);
   CLEARHFLAG(fwaMyPort, PTS65); /* CLEAR CONNECTED TO 6500 */
   /* Not sure about this next one--FREND comments say to do it, but 
    * it's not clear it's done in the code. */
   CLEARHFLAG(fwaMyPort, PTSCNT); /* CLEAR "CONNECTED" flag */
}

/*--- function ProcRecTypeORSP ----------------------
 *  Process a ORSP (Open ReSPonse) record type.
 *  Entry:  portNum  is the port number.
 *          bufaddr  is the FREND address of the record from 1FP.
 *                   It starts with a data buffer header, then
 *                   C_FPPT, etc.
 */
void ProcRecTypeORSP(HalfWord portNum, FrendAddr bufaddr)
{
   /*
   FrendAddr fwaMyPort = PortNumToFWA(portNum);
   HalfWord MySocketNum = GetHalfWord(fwaMyPort+H_PTCN1);
   */
   PUTBUF(bufaddr);
   PTMSG(portNum);
}

/*--- function ProcRecTypeOTBS ----------------------
 *  Process an OTBS (OuTput Buffer Status) command.
 *  This is based on FREND's "OTBS      SUBR" and is similar
 *  to OTNEUP().
 */
void ProcRecTypeOTBS(HalfWord portNum, FrendAddr bufaddr, FrendAddr fwaMyPort)
{
   PUTBUF(bufaddr);
   SendOTBSIfNecessary(portNum, fwaMyPort, TRUE);
}

/*--- function ProcRecTypeINBS ----------------------
 *  Process an INBS (INput Buffer Status) command.
 *  See "INBS      SUBR" in FREND.
 */
void ProcRecTypeINBS(HalfWord portNum, FrendAddr bufaddr)
{
   PUTBUF(bufaddr);
   TaskSENDCP(portNum, (Byte8)(FP_INBS+V_EXTREQ));
}

/*--- function ProcRecTypeTIME ----------------------
 *  Process a TIME command.
 *  This is supposed to set the FREND time-of-day, but we ignore it.
 */
void ProcRecTypeTIME(FrendAddr bufaddr)
{
   PUTBUF(bufaddr);
}

/*--- function ProcRecTypeCLO ----------------------
 *  Process a CLO (close port) command.
 *  Entry:  dataPort is the data port specified as an argument
 *                   in the command (NOT the control port).
 *          fwaMyPort   is the FWA of that port's entry.
 *          bufaddr  is the FWA of the command buffer.
 */
void ProcRecTypeCLO(HalfWord dataPort, FrendAddr fwaMyPort, FrendAddr bufaddr)
{
   Byte8 CloseType = GetByte(bufaddr+C_FPP2);
   PUTBUF(bufaddr);
   CLEARHFLAG(fwaMyPort, PTS65);
   TaskCLOFPT(dataPort, CloseType);
}

/*--- function TaskCTLPT ----------------------
 *  Process messages from 1FP on a control port
 */
void TaskCTLPT(HalfWord ctlPort)
{
   FrendAddr fwaCtlPort = PortNumToFWA(ctlPort);
   FrendAddr fwaList = GetFullWord(fwaCtlPort+W_PTOTCL);
   HalfWord  nSlotsAvail;
   FrendAddr bufaddr, fwaMyDataPort;
   Byte8 rectype, dataPort;

   while(TRUE) {
      bufaddr = RemoveFromBottomOfList(fwaList);
      if(!bufaddr) break;
      rectype = GetByte(bufaddr+C_DHTYPE);
      dataPort = GetByte(bufaddr+C_FPPT);
      fwaMyDataPort = 0;
      if(dataPort) {
         fwaMyDataPort = PortNumToFWA(dataPort);
      }
      switch(rectype) {
      case FP_CPOPN:
         ProcRecTypeCPOPN(ctlPort, bufaddr);
         break;
      case FP_CPCLO:
         ProcRecTypeCPCLO(ctlPort, bufaddr);
         break;
      case FP_ORSP:
         ProcRecTypeORSP(dataPort, bufaddr);
         break;
      case FP_OTBS:
         ProcRecTypeOTBS(dataPort, bufaddr, fwaMyDataPort);
         break;
      case FP_INBS:
         ProcRecTypeINBS(dataPort, bufaddr);
         break;
      case FP_TIME:
         ProcRecTypeTIME(bufaddr);
         break;
      case FP_CLO:
         ProcRecTypeCLO(dataPort, fwaMyDataPort, bufaddr);
         break;
      default:
         LogOut("==** TaskCTLPT: unhandled cmd %s", GetNameFromOffset(SymToNameRecordTypes, rectype));
         break;
      }
   }
   /* Update number of buffers available in the control port.
    * COUNT IS NOT CORRECT AS LONG AS THIS TASK IS RUNNING, BUT 
    * IT WILL BE TOO LOW, WHICH IS OK.
    */
   nSlotsAvail = GetListFreeEntries(fwaList);
   SetHalfWord(fwaCtlPort+H_PTOTNE, nSlotsAvail);
}

/*--- function KILLBUF ----------------------
 *  Helper function for processing user-typed Cancel or Escape.
 *
 *  Entry:  SocketNum   is the # of the socket where the user hit Ctrl-X or Esc.
 *          fwaMySocket is the FWA of that socket entry.
 *          bufout      is an already-formatted FREND buffer to send to user.
 */
void KILLBUF(HalfWord SocketNum, FrendAddr fwaMySocket, FrendAddr bufout)
{
   FrendAddr fwaList = fwaMySocket+W_SKOTCL;
   SetHalfWord(fwaMySocket+H_SKINCC, 0);  /* set to 0 chars */
   AddToBottomOfList(fwaList, bufout);
   TaskSKOTCL(SocketNum, fwaMySocket);
   CLEARHFLAG(fwaMySocket, SKETOG);  /* CLEAR TOGGLE ECHOBACK */
   CLEARHFLAG(fwaMySocket, SKOSUP);  /* CLEAR OUTPUT SUSPENDED */
}

/*--- function PALISR ----------------------
 *  Process a character received from the user.
 */
void PALISR(HalfWord SocketNum, Byte8 ch)
{
   FrendAddr fwaMySocket = SockNumToFWA(SocketNum);
   FrendAddr bufaddr = GetFullWord(fwaMySocket+W_SKINBF);
   FrendAddr bufout = 0;
   HalfWord count;
   /* Echo characters if "suppress echo" is not set. */
   TypBool bEcho = !HFLAGISSET(fwaMySocket, SKSUPE);
   if(0==bufaddr) bufaddr = GETINBF(fwaMySocket);

   switch(ch) {
   case '\r':
      /* Set buffer length = # of data chars + header len */
      SetByte(bufaddr+C_DHBCT, (Byte8)(L_DTAHDR+GetHalfWord(fwaMySocket+H_SKINCC)));
      /* clear socket's char count */
      SetHalfWord(fwaMySocket+H_SKINCC, 0);
      /* Handle end of line flag.  Tricky. */
      CLEARHFLAG(fwaMySocket, SKINEL);
      if((!GetByte(bufaddr+C_DHCEOL)) & V_DHCEOL) {
         SETHFLAG(fwaMySocket, SKINEL);
      }
      /* Clear socket's input buffer address, since we are now going to use the buffer. */
      SetFullWord(fwaMySocket+W_SKINBF, 0);
      TaskSKINCL(fwaMySocket, bufaddr);
      break;
   case '\n':
      /* I guess we should ignore LF, because some telnet clients
       * send CR LF when you press Enter.  */
      bEcho = FALSE;
      break;
   case '\b':
      /* backspace: delete previous char, if any. */
      /* //! This will have to be enhanced when we do TCP/IP */
      /* Delete previous char on line, if any. */
      count = GetHalfWord(fwaMySocket+H_SKINCC);
      if(count > 0) {
         SetHalfWord(fwaMySocket+H_SKINCC, (HalfWord)(count-1));
      } else {
         bEcho = FALSE;
      }
      break;
   case '\x18':  /* CANCEL function (erase current input line) */
      bufout = GetBufferForC(" \r\\\\\\\\\r\n");
      KILLBUF(SocketNum, fwaMySocket, bufout);
      bEcho = FALSE;
      break;
   case '\x1b':  /* Escape: abort current program and discard input line */ 
      if(DebugL>=LL_SOME) LogOut("==Escape pressed.");
      bEcho = FALSE;
      CLEARHFLAG(fwaMySocket, SKSUPE);  /* CLEAR 'SUPPRESS ECHO' FLAG */
      if(!HFLAGISSET(fwaMySocket, SKESCP)) {
         /* Escape Pending flag not already set */
         SETHFLAG(fwaMySocket, SKESCP);
         TaskINESC(SocketNum);
         /* Send a message.  It should include \\\\ if chars have been typed. */
         /*//! I shouldn't have to include carriage controls here. */
         if(GetByte(fwaMySocket + H_SKINCC)) {
            bufout = GetBufferForC(" !\r\\\\\\\\\r\n");
         } else {
            bufout = GetBufferForC(" !\r\n");
         }
         KILLBUF(SocketNum, fwaMySocket, bufout);
      }
      break;
   default:
      count = GetHalfWord(fwaMySocket+H_SKINCC);
      SetByte(bufaddr+L_DTAHDR+count, ch);
      count++;
      if(count >= GetHalfWord(fwaMySocket+H_SKINLE)) {
         /* buffer is full */
      } else {
         SetHalfWord(fwaMySocket+H_SKINCC, count);
      }
      break;
   }

   if(bEcho) {
      /* crude echo */
      SendToFSock(SocketNum, &ch, 1);
   }
}

/*---  Processing of control port commands  ------------------*/

/*--- function CmdControlPortOpen ----------------------
 *  Process the ControlPortOpen command (NOT the FP.CPOPN record type
 *  sent by HEREIS).
 */
void CmdControlPortOpen()
{
   Byte8 port = GetByte(fwaFPCOM+C_CPOPT);
   /* Set this port's connection number */
   SetPortHalfWord(port, H_PTCN1, 1);
   /* Set this port's number of empty output slots. */
   SetPortHalfWord(port, H_PTOTNE, 2);
}

/*--- function CmdHereIs ----------------------
 *  Process a HEREIS command from 1FP.
 *  Most commands from the Cyber are HEREIS, with a record type
 *  field in the buffer indicating what is to be done.
 *  So, this function is a gateway to a lot of what goes on in frend2.
 *
 *  Entry:  offset_for_buftype   is W_NBF80 or W_NBF240
 */
void CmdHereIs(HalfWord PortNum, int offset_for_buftype)
{
   /* I don't understand the difference between FPCOM's H.FCMDPT and
    * the buffer's C.FPPT.  Sometimes the former is the control port
    * and the latter the data port. */
   FrendAddr newaddr;
   HalfWord nSlotsAvail, cmdPort;
   FrendAddr fwaMyPort = PortNumToFWA(PortNum);
   FrendAddr fwaList = GetFullWord(fwaMyPort+W_PTOTCL);
   FrendAddr bufaddr = Addr1FPToFREND(GetFullWord(fwaFPCOM+offset_for_buftype));
   HalfWord portRec = GetByte(bufaddr+C_FPPT);
   Byte8 rectype = GetByte(bufaddr + C_DHTYPE);

   /* Clear next buffer interlock */
   DropIL(fwaFPCOM+H_NBUFIK);

   /* Put a fresh buffer into W_NBF80 or 240 */
   /* Byte count must be zero to make 1FP happy. */
   /* See end of 1FP "GETOBUF   ENTRY." */
   newaddr = (offset_for_buftype == W_NBF80) ? Get80() : Get240();
   SetByte(newaddr+C_DHBCT, 0);
   SetFullWord(fwaFPCOM+offset_for_buftype, AddrFRENDTo1FP(newaddr));

   /* Add this newly-received buffer to the list for this port. */
   /* Seems to me we should add to *top* of list; but there is */
   /* conditional code at "HEREIS    .." in FREND to add to bottom of list */
   /* and that is what works. */
   if(GetByte(fwaMyPort+C_DHCASY) & V_DHCASY) {
      /* This is an "asynchronous" message, so add to bottom of list */
      AddToBottomOfList(fwaList, bufaddr);
   } else {
      /* Normal message, so add to top of list */
      AddToTopOfList(fwaList, bufaddr);
   }

   /* Set number of available entries in port */
   nSlotsAvail = GetListFreeEntries(fwaList);
   if(DebugL>=LL_SOME) LogOut("==CmdHereIs: port %d nSlotsAvail=%d", PortNum, nSlotsAvail);
   SetHalfWord(fwaMyPort+H_PTOTNE, nSlotsAvail);

   /* Clear OUTPUT BUFFER INTERLOCK for the command port */
   DropIL(fwaMyPort+H_PTOTIK);

   cmdPort = GetHalfWord(fwaFPCOM+H_FCMDPT);

   if(DebugL>=LL_SOME) {
      /* Display the first few bytes in the record. */
      int j, len=0;
      char szmsg[256];
      for(j=0; j<24; j++) {
         len += sprintf(szmsg+len, "%-2.2x ", GetByte(bufaddr+j));
      }
      LogOut("==HereIs details: FPCOM FCMDPT=%d offset=%xH bufaddr=%xH %s", cmdPort, offset_for_buftype, bufaddr, szmsg);
      LogOut("==CmdHereIs: rectype=%s port=%u rec's port=%u p2=%xH p3=%xH p4=%xH p5=%xH", 
         GetNameFromOffset(SymToNameRecordTypes, rectype),
         PortNum, GetByte(bufaddr+C_FPPT), GetByte(bufaddr+C_FPP2), 
         GetByte(bufaddr+C_FPP3), GetByte(bufaddr+C_FPP4), GetByte(bufaddr+C_FPP5)
         );
   }

   if(cmdPort <= PTN_MAX) {
      /* this is a write to a control port */
      TaskCTLPT(PortNum);
   } else {
      Byte8 connType = GetByte(fwaMyPort+C_PTCT1);
      if(CT_SOCK == connType) {
         HalfWord SocketNum = GetHalfWord(fwaMyPort+H_PTCN1);
         FrendAddr fwaMySocket = SockNumToFWA(SocketNum);
         if(FP_BULK == rectype) {
            /*//! This should be moved to SKOTCL, I think. */
            SETHFLAG(fwaMySocket, SKSUPE);
         }
         TaskSKOTCL(SocketNum, fwaMySocket);
      } else if(CT_PORT == connType) {
         /* We don't handle port-to-port connections. */
         LogOut("==** HereIs: We don't implement port-to-port connections.", connType);
      } else {
         LogOut("==** HereIs: Bad connType: %d", connType);
      }
   }


}

void ClearCmdInterlock()
{
   /* Clear FPCOM interlock by setting to 1, which means OK. */
   DropIL(fwaFPCOM+H_FCMDIK);
}

/*--- function CmdITook ----------------------
 *  Process the ITOOK command, which 1FP sends to FREND to tell
 *  it that 1FP has processed the most recent buffer for this port.
 */
void CmdITook()
{
   FrendAddr fwaList, bufnext, bufnext1FP; /* If this were C++, we could declare these below. */

   /* Free the buffer that was just processed by 1FP */
   HalfWord portNum = GetHalfWord(fwaFPCOM+H_FCMDPT);
   FrendAddr fwaPort = PortNumToFWA(portNum);
   FrendAddr bufaddr = GetFullWord(fwaPort+W_PTIN);
   bufaddr = Addr1FPToFREND(bufaddr);
   if(DebugL>=LL_SOME) {
      Byte8 msgcode = GetByte(bufaddr+C_DHTYPE) & (0xffff ^ V_EXTREQ);
      LogOut("==CmdITook: buffer was from port %u cmd %s", portNum, GetNameFromOffset(SymToNameRecordTypes, msgcode));
   }
   PUTBUF(bufaddr);

   /* MOVE NEXT LINE FROM PORT LIST TO W.PTIN FOR 1FP */

   fwaList = GetFullWord(fwaPort+W_PTINCL);
   bufnext = RemoveFromBottomOfList(fwaList);
   /* bufnext is 0 if no more buffers available. */
   bufnext1FP = AddrFRENDTo1FP(bufnext);
   if(DebugL>=LL_SOME) LogOut("==CmdITook: setting port %d's W_PTIN to %x (FREND addr %x)", portNum, bufnext1FP, bufnext);
   SetFullWord(fwaPort+W_PTIN, bufnext1FP);

   DropIL(fwaPort + H_PTINIK); /* CLEAR PORT INTERLOCK */

   /* If the port is not a control port,
      we should send a FP.INBS message over the control port,
      giving input buffer status.  See 
      SEND FP.INBS OVER CONTROL PORT in ISR65
    */
   if(portNum > PTN_MAX) {
      if(DebugL>=LL_SOME) LogOut("==CmdITook: Calling SENDCP to send INBS");
      TaskSENDCP(portNum, FP_INBS);
   }

   SetHalfWord(fwaFPCOM+H_FCMDTY, 0);  /* Clear type.  Tricky use of H_FCMDTY. */
   SetHalfWord(fwaFPCOM+H_FCMDPT, 0);  /* Clear port number */
   ClearCmdInterlock();
}

/*--- function HandleInterruptFromHost ----------------------
 *  Handle an interrupt function code sent by 1FP.
 */
void HandleInterruptFromHost()
{
   Byte8 cmd = GetByte(fwaFPCOM+C_FCMDTY);
   HalfWord PortNum = GetHalfWord(fwaFPCOM+H_FCMDPT);

   if(DebugL>=LL_SOME) LogOut("== Got Interrupt; processing cmd %s", CmdToDesc(cmd));
   switch(cmd) {
   case FC_ITOOK:
      CmdITook();
      break;
   case FC_HI80:
      CmdHereIs(PortNum, W_NBF80);
      break;
   case FC_HI240:
      CmdHereIs(PortNum, W_NBF240);
      break;
   case FC_CPOP:
      CmdControlPortOpen();
      break;
   case FC_CPGON:
      break;

   }

   ClearCmdInterlock();
   ReturnBuffersInReleaseList();
}

/*--- function ProcessCyberRequest ----------------------
 *  Process a low-level request (channel function or I/O) 
 *  from the Cyber.
 */
void ProcessCyberRequest()
{
   unsigned int entry_addr = pFrendInt->FrendState.fr_addr;
   switch(pFrendInt->cf.cf_reqtype) {
   case REQTYPE_FCN:
      /* We assume that the only function that DtCyber will pass
       * to us is Interrupt. */
      HandleInterruptFromHost();
      break;
   default:
      LogOut("==** Error: unrecognized request: %c", pFrendInt->cf.cf_reqtype);
      break;
   }
}

/*--- function ProcessIncomingConnection ------------------
 *  select() noticed an incoming connection on the listening port.
 *  Accept it and create a new terminal session.
 */
void ProcessIncomingConnection()
{
   struct sockaddr_in user_addr;
   HalfWord fsock;
   TypBool bFound = FALSE;
   Byte8 mybuf[16];
   TypSockAddrLen addr_size = sizeof(user_addr);
   SOCKET sockUser = accept(sockTCPListen, (struct sockaddr *)&user_addr, &addr_size);
   /* Set the socket to non-blocking, so we'll get an error if it's filled up */
   unsigned long lNonBlocking=1;
   if(SOCKET_ERROR == ioctlsocket(sockUser, FIONBIO, &lNonBlocking)) {
      LogOut("**==ProcessIncomingConnection: can't set non-blocking");
   }

   if(DebugL>LL_WARNING) LogOut("Accepted connection from %s; TCP socket=%x", inet_ntoa(user_addr.sin_addr), sockUser);
   /* Assign the user a slot in SockTCPAry, if available. */
   for(fsock=FIRSTUSERSOCK; fsock<MAX_TCP_SOCKETS; fsock++) {
      if(0 == SockTCPAry[fsock].stcp_socket) {
         SockTCPAry[fsock].stcp_socket = sockUser;
         SockTCPAry[fsock].stcp_telnet_state = TELST_NORMAL;
         bFound = TRUE;
         break;
      }
   }
   if(bFound) {
      /* Tell the client to turn off echo */
      int ibyte=0;
      mybuf[ibyte++] = TELCODE_IAC;
      mybuf[ibyte++] = TELCODE_DONT;
      mybuf[ibyte++] = TELCODE_OPT_ECHO;
      mybuf[ibyte++] = TELCODE_IAC;
      mybuf[ibyte++] = TELCODE_WILL;
      mybuf[ibyte++] = TELCODE_OPT_ECHO;
      mybuf[ibyte++] = TELCODE_IAC;
      mybuf[ibyte++] = TELCODE_WILL;
      mybuf[ibyte++] = TELCODE_OPT_SUPPRESS_GO_AHEAD;
      mybuf[ibyte++] = TELCODE_IAC;
      mybuf[ibyte++] = TELCODE_DO;
      mybuf[ibyte++] = TELCODE_OPT_SUPPRESS_GO_AHEAD;
      TCPSend(sockUser, mybuf, ibyte);

      TaskSKCARR(fsock);
   } else {
      char *szSorry = "\r\nSorry, all sockets are in use.";
      TCPSend(sockUser, (unsigned char *)szSorry, strlen(szSorry));
      closesocket(sockUser);
   }
}

/*--- function ProcessInboundTelnet ----------------------
 *  Implement a very simple telnet server.  We basically parse
 *  but ignore all incoming sequences.  You'd be surprised at the
 *  variety of different behaviors between different telnet clients.
 *  When we recognize actual data from the user, call PALISR with it.
 *  The telnet FSA state for this connection is kept in SockTCPAry.
 */
void ProcessInboundTelnet(SOCKET sockUser, unsigned char *buf, int len)
{
   int j;
   Byte8 ch;
   TypSockTCP *pSockTCP;
   HalfWord fsock = FindFSockFromTCPSock(sockUser);
   if(!fsock) return;
   pSockTCP = &SockTCPAry[fsock];

   for(j=0; j<len; j++) {
      ch = buf[j];
      if(DebugL>=LL_SOME) LogOut("=== Got TCP char %-2.2x (%c)", ch, ch);
      switch(pSockTCP->stcp_telnet_state) {
      case TELST_NORMAL:
         if(TELCODE_IAC == ch) {
            pSockTCP->stcp_telnet_state = TELST_GOT_IAC;
         } else {
            PALISR(fsock, ch); /* This is the normal case. */
         }
         break;
      case TELST_GOT_IAC:
         if(TELCODE_IAC == ch) {
            /* IAC IAC is like a single IAC */
            PALISR(fsock, ch);
            pSockTCP->stcp_telnet_state = TELST_NORMAL;
         } else if(ch >= TELCODE_WILL && ch <= TELCODE_DONT) {
            pSockTCP->stcp_telnet_state = TELST_GOT_WILL_OR_SIMILAR;
         } else {
            pSockTCP->stcp_telnet_state = TELST_NORMAL;
         }
         break;
      case TELST_GOT_WILL_OR_SIMILAR:
         pSockTCP->stcp_telnet_state = TELST_NORMAL;
         break;
      }

   }
}

/*--- function WriteNowAvailable ----------------------
 *  We have been alerted that we can now write on this socket.
 */
void WriteNowAvailable(SOCKET sockUser)
{
   HalfWord fsock = FindFSockFromTCPSock(sockUser);
   TypPendingBuffer *pbuf = &(SockTCPAry[fsock].stcp_buf);
   int noutbytes=pbuf->spb_chars_left, bytes_sent;
   bytes_sent = SendToFSock(fsock, pbuf->spb_buf + pbuf->spb_first, noutbytes);
   if(bytes_sent <= 0) {
      if(EWOULDBLOCK == GetLastOSError()) {
         bytes_sent = 0;
      }
   }
   pbuf->spb_first += bytes_sent;
   pbuf->spb_chars_left = noutbytes - bytes_sent;

   /* If we sent all pending characters, then start sending
    * the rest of the buffers (if any). */
   if(0==pbuf->spb_chars_left) {
      FrendAddr fwaMySocket = SockNumToFWA(fsock);
      TaskSKOTCL(fsock, fwaMySocket);
   }
}

/*--- function CloseTCPConnection ----------------------
 */
void CloseTCPConnection(SOCKET sockUser)
{
   HalfWord fsock = FindFSockFromTCPSock(sockUser);
   closesocket(sockUser);
   if(fsock) {
      ClearSockTCPEntry(fsock);
   }
   {
      FrendAddr fwaMySocket = SockNumToFWA(fsock);
      TaskCLOFSK(fsock, fwaMySocket);
   }
}

/*--- function MainLoop ----------------------
 */
void MainLoop()
{
   int nfds, nready, fsock;
	/* FACTOR_INTO_NFDS sets mynfds to mysock if mysock < mynfds.
	 * This is needed to calculate select's nfds parameter only in *nix;
	 * Windows ignores this parameter to select(). */
#ifdef WIN32
#define FACTOR_INTO_NFDS(mysock, mynfds) /* do nothing */
#else
#define FACTOR_INTO_NFDS(mysock, mynfds) \
	if(mysock>mynfds) { mynfds = mysock; }
#endif

   fd_set readfds, writefds, exceptfds;
   struct timeval timeout;
   unsigned char buf[256];

#define MIN_FREE_PORT_BUFFERS 2
   if(DebugL>=LL_MORE)LogOut("Entering main FREND loop.");
   do {
		nfds=0;
      /* Calculate the handles for which select() should listen. */
      /* Do I really need to pass NULL if no handles are included in a set? */
      FD_ZERO(&readfds);
      FD_ZERO(&writefds);
      FD_ZERO(&exceptfds);
      /* Wait for an interrupt from the mainframe--but if we don't get
       * one soon, return anyway so we can clear the FEDEAD deadman timer.
       * The deadman timeout in 1FP is 1 second, but since the clock 
       * isn't accurate, we'll play it safe by waiting less. */
      FD_SET(sockFromCyber, &readfds);
		FACTOR_INTO_NFDS(sockFromCyber, nfds);
      /* Always listen for new user connections. */
      FD_SET(sockTCPListen, &readfds);
		FACTOR_INTO_NFDS(sockTCPListen, nfds);
      /* Listen for bytes for the already-connected users. */
      for(fsock=FIRSTUSERSOCK; fsock<MAX_TCP_SOCKETS; fsock++) {
         if(SockTCPAry[fsock].stcp_socket) {
            /* Don't read from this socket unless the associated port
             * has a few free buffers.  Because each byte read could be
             * an end-of-line, we don't read any more bytes than there
             * are buffers available.  This way, we won't end up with
             * data that we have read and have no place to put. 
             * Note: in separate benchmarking with programs SimpSend
             * and SimpRecv, throughput wasn't bad even with small buffers:
             * -- With 1-byte input buffer and select, 176739 bytes/sec
             * -- With 2-byte input buffer and select, 332468 bytes/sec
             * As you can see, a 2-byte buffer is quite a bit faster
             * than a 1-byte buffer. */
            FrendAddr fwaMySocket = SockNumToFWA(fsock);
            HalfWord PortNum = GetHalfWord(fwaMySocket+H_SKCN1);
            FrendAddr fwaMyPort = PortNumToFWA(PortNum);
            FrendAddr fwaList = GetFullWord(fwaMyPort+W_PTINCL);
            HalfWord nFree = GetListFreeEntries(fwaList);
            if(DebugL>=LL_ALL) LogOut("==MainLoop: fsock %d port %d free inbufs %d", fsock, PortNum, nFree);
            if(GetListFreeEntries(fwaList) > MIN_FREE_PORT_BUFFERS) {
               FD_SET(SockTCPAry[fsock].stcp_socket, &readfds);
					FACTOR_INTO_NFDS(SockTCPAry[fsock].stcp_socket, nfds);
            }
            /* If there are characters pending output on this socket,
             * then we need to hear about write availability. */
            if(SockTCPAry[fsock].stcp_buf.spb_chars_left) {
               FD_SET(SockTCPAry[fsock].stcp_socket, &writefds);
					FACTOR_INTO_NFDS(SockTCPAry[fsock].stcp_socket, nfds);
            }
         }
      }
		/* Set a timeout for select so we can clear the deadman flag. */
		timeout.tv_sec = 0;
		timeout.tv_usec = 100000;
      nready = select(nfds+1, &readfds, &writefds, &exceptfds, &timeout);
      if(nready > 0) {
         if(FD_ISSET(sockFromCyber, &readfds)) {
            /* We got an "interrupt" from the Cyber. */
            /* Read and discard the byte that signalled the interrupt. */
            ReadSocketFromCyber(buf, 1);
            /* Process the interrupt. */
            ProcessCyberRequest();
				if(pFrendInt->sfi_bSendReplyToCyber) {
					ReplyToCyber();
				}
         }
         if(FD_ISSET(sockTCPListen, &readfds)) {
            /* A terminal user is trying to connect. */
            ProcessIncomingConnection();
         }
         for(fsock=FIRSTUSERSOCK; fsock<MAX_TCP_SOCKETS; fsock++) {
            SOCKET sockUser=SockTCPAry[fsock].stcp_socket;
            if(FD_ISSET(sockUser, &readfds)) {
               int nbytes=recv(sockUser, (char *)buf, MIN_FREE_PORT_BUFFERS, 0);
               if(nbytes>0) {
                  ProcessInboundTelnet(sockUser, buf, nbytes);
               } else {
                  LogOut("Socket %d closed", sockUser);
                  CloseTCPConnection(sockUser);
               }
            }
            if(FD_ISSET(sockUser, &writefds)) {
               WriteNowAvailable(sockUser);
            }
         }
      } else if(SOCKET_ERROR == nready) {
         printf("Error %d from select\n", GetLastSocketError());
#ifdef WIN32
         Sleep(2000);
#else
         sleep(2);
#endif
      }
      DropIL(fwaFPCOM+H_FEDEAD); /* Clear "front-end dead" flag */
   } while(TRUE);
}

/*--- function SimulateConnect ----------------------
 *  Simulate a user dialing in.  For debugging this is done when
 *  you press F2; this is about the same as we do in response to a TCP accept().
 */
void SimulateConnect()
{
   if(DebugL>=LL_SOME) LogOut("Simulating connect.");
   /* We'll hard-code this to the same socket and port every time. */
   TaskSKCARR(FSOCKETCONSOLE);
}

void CreateNewSessionLog()
{
   if(fileOperSess) fclose(fileOperSess);
   unlink(szOperSessFilenameOld);
   if(0==rename(szOperSessFilename, szOperSessFilenameOld)) {
      printf("Old log renamed to %s\n", szOperSessFilenameOld);
   }
   fileOperSess = fopen(szOperSessFilename, "wb");
}

#ifdef WIN32
/*--- function KeyboardThread ----------------------
 *  This function runs in a separate thread, waiting for 
 *  keystrokes from the keyboard.  It was originally implemented
 *  for debugging, but I may leave it in.
 */
void KeyboardThread(LPVOID lpParameter)
{
   int ch;
   printf("frend2 %s operator terminal.  Press F1 for help.\n", szFRENDVersion);
   do {
      ch = (0xff & _getch());
      /* 0 and E0 indicate a special character, whose code is given */
      /* by the next char. */
      if(0==ch || 0xE0==ch) {
         /* A special (non-ASCII) key was hit. */
         ch = (0xff & _getch());
         LogOut("==Pressed function key %x", ch);
         if(PCKEYCODE_F1 == ch) {
            printf("F2 = Simulate connect\n");
            printf("F3 = Simulate disconnect\n");
            printf("F4 = Close and reopen session log file\n");
            printf("F10= Exit frend2 immediately\n");
         } else if(PCKEYCODE_F2 == ch) {
            SimulateConnect();
         } else if(PCKEYCODE_F3 == ch) {
            FrendAddr fwaMySocket = SockNumToFWA(FSOCKETCONSOLE);
            if(DebugL>=LL_SOME) LogOut("Simulating disconnect.");
            TaskCLOFSK(FSOCKETCONSOLE, fwaMySocket);
         } else if(PCKEYCODE_F4 == ch) {
            CreateNewSessionLog();
         } else if(PCKEYCODE_F10 == ch) {
            exit(0);
         } else if(PCKEYCODE_F9 == ch) {
            /* No-longer-needed kludge to tell MANAGER that we are ready for more output */
            TaskSENDCP(FPORTCONSOLE, FP_OTBS);
         }
      } else {
         /* A printing character was received.  Act as if a user typed it. */
         PALISR(FSOCKETCONSOLE, (Byte8)ch);
      }

   } while(TRUE);
}

void StartKeyboardThread()
{
   void *pToTask=NULL;
   unsigned long threadid = _beginthread(KeyboardThread, 0, pToTask);
}
#endif

/*--- function ParseArgs ----------------------
 *  Parse the frend2 command line.
 *  Entry:  argc, argv are the original args to main().
 *
 *  Exit:   Returns 0 if OK, else error code, in which case
 *          an error message has been printed.
 */
int ParseArgs(int argc, char *argv[])
{
   int retcode = 0;
   int iarg;
   for(iarg=1; iarg<argc; iarg++) {
      if(0==strcmp("-d", argv[iarg])) {
         iarg++;
         if(iarg >= argc) {
            retcode = 1;
            break;
         } else {
            DebugL = atoi(argv[iarg]);
         }
      } else if(0==strcmp("-m", argv[iarg])) {
         iarg++;
         if(iarg >= argc) {
            retcode = 1;
            break;
         } else {
            SetMaxLogMessages(atol(argv[iarg]));
         }
      } else if(0==strcmp("-e", argv[iarg])) {
         CreateNewSessionLog();
      } else if(0==strcmp("-p", argv[iarg])) {
         iarg++;
         if(iarg>=argc) {
            retcode = 1;
            break;
         } else {
            TCPListenPort = atoi(argv[iarg]);
         }
		} else if(0==strcmp("-s", argv[iarg])) {
			bSendReplyToCyber = TRUE;
      } else {
         retcode = 1;
      }
   }
   if(retcode) {
      puts("frend2: Interactive front-end to SCOPE/Hustler running under DtCyber.");
      puts("See http://60bits.net or http://frend2.sourceforge.net.");
      puts("Usage:  frend2 [-d dbglevel] [-m maxmsgs] [-s] [-e]");
      puts("   [-p tcpport]");
      puts("where dbglevel is a debug level.  Debug goes to text file frend.log.");
      puts("   10 for errors only,");
      puts("   20 for that plus warnings (the default),");
      puts("   30 for that plus fairly verbose debug,");
      puts("   50 for extremely verbose debug.");
      puts("maxmsgs is the maximum # of messages to log.");
		puts("-s means synchronous; frend2 sends reply to dtcyber for each interrupt.");
		puts("   Makes interactive more responsive, at cost of overall throughput.");
      puts("-e means create a session log for the operator terminal (WIN32 only).");
      puts("tcpport is the port on which we listen for terminal connections.");
      puts("      (Default 6500).");
   }
   return retcode;
}

int main(int argc, char* argv[])
{
   int retcode = 0;
   do {  /* This is not really a loop--just a way to exit via break. */
      if(0 != (retcode=ParseArgs(argc, argv))) break;
      if(0 != (retcode = InitFRENDInterface(TRUE))) break;
      InitFREND();
		if(bSendReplyToCyber) pFrendInt->sfi_bSendReplyToCyber=TRUE;
#ifdef WIN32
      StartKeyboardThread();
#endif
      MainLoop();
   } while(FALSE); 
	return retcode;
}
