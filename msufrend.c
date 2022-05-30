/*--------------------------------------------------------------------------
**
**  Written by Mark Riordan, and placed in the public domain.  June 2004
**  Version 2 done in February 2005.
**
**  Name: msufrend.c
**
**  Description:
**      Perform emulation of Michigan State University's FREND
**      interactive front-end.  The real FREND ran on an Interdata 7/32,
**      but we are emulating only the functionality of FREND and not
**      the 7/32 instruction set.  The actual emulation of FREND is done
**      by the separate program frend2.
**
**      This is the second version of msufrend.c.  Unlike the first
**      (which did little but hand off the real work to frend2, with a
**      lot of context switching), this emulates the functions of the 
**      6000 Channel Adapter (6CA), a custom piece of hardware designed 
**      at MSU which allows a 6000 PP to do DMA to the 7/32.  
**      A block of memory shared between DtCyber and frend2 implements 
**      the 7/32's memory.  Like the original 6CA, this allows DtCyber to
**      read and write FREND memory without disturbing frend2.
**      However, the real front-end work is done by the separate frend2.
**
**      DtCyber sends a UDP packet to frend2 to signify an "interrupt".
**      Unlike the real Cyber, it waits for frend2 to reply (via SetEvent)
**      before continuing.  This improves interactive responsiveness,
**      probably at the cost of overall DtCyber performance (during the
**      time that 1FP is talking to FREND).
**
**      On the deadstart tape Ken Hunter gave me 5 June 2004, FREND is
**      Equipment Status Table (EST) entry 70 octal, equip 0, unit 0,
**      channel 2, DST 24.
**--------------------------------------------------------------------------
*/

/*
**  -------------
**  Include Files
**  -------------
*/
#include "msufrend_util.h"

/*
**  -----------------
**  Private Constants
**  -----------------
*/

/*
**  Function codes.
*/
/* See frendint.h */

/*
**  -----------------------
**  Private Macro Functions
**  -----------------------
*/
#define TraceFrend 0

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
static FcStatus msufrendFunc(PpWord funcCode);
static void msufrendIo(void);
static void msufrendActivate(void);
static void msufrendDisconnect(void);

/*
**  ----------------
**  Public Variables
**  ----------------
*/
u16 telnetPort;
u16 telnetConns;

/*
**  -----------------
**  Private Variables
**  -----------------
*/
extern TypFrendInterface *pFrendInt;

/*
**--------------------------------------------------------------------------
**
**  Public Functions
**
**--------------------------------------------------------------------------
*/
/*--------------------------------------------------------------------------
**  Purpose:        Initialise the MSUFREND device.
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
void msufrendInit(u8 eqNo, u8 unitNo, u8 channelNo, char *deviceName)
{
   DevSlot *dp;

   (void)eqNo;
   (void)deviceName;

   if(TraceFrend) {
    InitLog("msufrend.trc", "Cy");
   }
   dp = channelAttach(channelNo, eqNo, DtMSUFrend);
   dp->activate = msufrendActivate;
   dp->disconnect = msufrendDisconnect;
   dp->func = msufrendFunc;
   dp->io = msufrendIo;
   dp->selectedUnit = unitNo;

   InitFRENDInterface(FALSE);
   CreateSockToFrend();
	InitWaitForFrend();

   /*
   **  Print a friendly message.
   */
   printf("MSUFREND initialised on channel %o unit %o\n", channelNo, unitNo);
}

/*--------------------------------------------------------------------------
**  Purpose:        The PP has requested an operation that we cannot handle
**                  inside DtCyber.exe--we must forward it to frend2.exe.
**                  This routine does this.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void TalkToFrend()
{
   /* Set FREND's copy of data, and active and full flags. */
   pFrendInt->cf.cf_func = activeDevice->fcode;

   /* SetEvent(hEventCyberToFrend); */
   SendToFrend((Byte8 *)"f", 1);
	if(pFrendInt->sfi_bSendReplyToCyber) {
		WaitForFrend();
   }
}

/*--------------------------------------------------------------------------
**  Purpose:        Send an "interrupt" to frend2.exe.
**                  We do not wait for a response.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
void SendInterruptToFrend()
{
   TalkToFrend();
}

/*--------------------------------------------------------------------------
**  Purpose:        Execute function code on MSU FREND.
**
**  Parameters:     Name        Description.
**                  funcCode    function code
**
**  Returns:        FcStatus
**
**------------------------------------------------------------------------*/
static FcStatus msufrendFunc(PpWord funcCode)
{
   unsigned int data = funcCode & 0xff;
   /* Mask off top 4 bits of the PP word.  This contains the function code,
    * more or less.  In some cases, the bottom 8 bits contain data related
    * to the function--typically address bits to set. */
   PpWord funccode_masked = 07400 & funcCode;
   pFrendInt->FrendState.fr_last_func_code = funccode_masked;

   if(TraceFrend) LogOut("Func; %4o active=%d full=%d", funcCode, 
      activeChannel->active, activeChannel->full);

   activeDevice->fcode = funcCode;
   pFrendInt->cf.cf_reqtype = REQTYPE_FCN;

   switch(funccode_masked) {
      case FcFEFSEL:  /* SELECT 6000 CHANNEL ADAPTER */
      case FcFEFDES:  /* DESELECT 6000 CHANNEL ADAPTER */
         /* DESELECT is a special case--must recheck original parameter. */
         if(FcFEFDES == funcCode) {
            pFrendInt->FrendState.fr_last_func_code = funcCode;
            if(TraceFrend)LogOut("FREND: Got DESELECT");
         }
         /* Technically, this is not correct, because SELECT (2400) and
          * DESELECT (2410) have the same top 4 bits.
          * But I don't think it matters. */
         break;
      case FcFEFST:
         /* READ 6CA STATUS */
         /*activeChannel->data = FCA_STATUS_INITIALIZED | FCA_STATUS_LAST_BYTE_NO_ERR;*/
         break;
      case FcFEFSAU:
         /* SET ADDRESS (UPPER) - Set upper 3 bits of 19-bit address */
         pFrendInt->FrendState.fr_addr = (pFrendInt->FrendState.fr_addr & 0x1fffe) | (((unsigned int) 7 & data)<<17);
         break;
      case FcFEFSAM:
         /* SET ADDRESS (MIDDLE) - Set middle byte of address, bits 2**8 thru 2**15 */
         pFrendInt->FrendState.fr_addr = (pFrendInt->FrendState.fr_addr & 0xfe01ff) | (((unsigned int) data)<<9);
         break;
      case FcFEFHL:
         /* Halt-Load the 7/32 */
         break;
      case FcFEFINT:
         /* Interrupt the 7/32 */
         SendInterruptToFrend();
         break;
      case FcFEFLP:
         /* LOAD INTERFACE MEMORY */
         /* Prepare to accept 8-bit bytes, to be written in "a 16-byte memory"
          * starting at location 0. */
         pFrendInt->FrendState.fr_addr = 0;
         break;
      case FcFEFRM:
         /* READ - I think the data byte is the lower 8 bits of the address. */
         pFrendInt->FrendState.fr_addr = (pFrendInt->FrendState.fr_addr & 0x1fffe00) | ((unsigned int) (data<<1));
         break;
      case FcFEFWM0:
         /* WRITE MODE 0 - One PP word considered as 2 6-bit bytes, written to a 16-bit FE word. */
         pFrendInt->FrendState.fr_addr = (pFrendInt->FrendState.fr_addr & 0x1fffe00) | ((unsigned int) (data<<1));
         break;
      case FcFEFWM:
         /* WRITE MODE 1 - Two consecutive PP words (8 in 12) written to a 16-bit FE word. */
         /* First PP word goes to upper 8 bits of FE word, confusingly called bits 0-7. */
         pFrendInt->FrendState.fr_addr = (pFrendInt->FrendState.fr_addr & 0x1fffe00) | ((unsigned int) (data<<1));
         break;
      case FcFEFRSM:
         /* READ AND SET - test and set on single 16-bit location. */
         /* Mode always forced to 1 so exactly 2 bytes of data will be sent to PPU. */
         /* After second byte, channel is empty until transfer terminated */
         /* by the PPU with a DCN.  Address register is not changed, says */
         /* the 6CA hardware doc. */
         /* Apparently the top bit is set. */
         pFrendInt->FrendState.fr_addr = (pFrendInt->FrendState.fr_addr & 0x1fffe00) | ((unsigned int) (data<<1));
         pFrendInt->FrendState.fr_next_is_second = FALSE;
         break;
      case FcFEFCI:
         /* CLEAR INITIALIZED STATUS BIT   */
         break;
   }

   return(FcAccepted);
}

/*--------------------------------------------------------------------------
**  Purpose:        Perform I/O on MSU FREND.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**  The rules seem to be:
**  If last function was read, if activeChannel->full is true, do nothing,
**     else if there's data in the device,
**       set activeChannel->data to the next 12-bit data
**       and set activeChannel->full = TRUE;
**     else (no data in device)
**       activeChannel->active = FALSE;
**  If last function was write, if activeChannel->full is false, do nothing,
**     else take activeChannel->data and send it to the device,
**          and set activeChannel->full = FALSE;
**------------------------------------------------------------------------*/
static void msufrendIo(void)
{
   /* PpWord PrevFunc = 07400 & activeDevice->fcode; */
   if(TraceFrend) LogOut("IO; fcode=%o B data=%4o active=%d full=%d", 
      activeDevice->fcode, activeChannel->data, activeChannel->active, activeChannel->full);
   switch(pFrendInt->FrendState.fr_last_func_code) {
      case FcFEFSEL:
         /* I don't know what to do on an I/O after a SELECT */
         activeChannel->full = TRUE;
         activeChannel->active = TRUE;
         break;
      case FcFEFDES:
         /* DESELECT 6000 CHANNEL ADAPTER  */
         break;
      case FcFEFST:
         /* READ 6CA STATUS */
         /* It's a read-type I/O */
         /* We should not do the read unless the channel is "not full" */
         if (!activeChannel->full) {
            activeChannel->data = FCA_STATUS_INITIALIZED | FCA_STATUS_LAST_BYTE_NO_ERR;
            activeChannel->full = TRUE;
         }
         break;
      case FcFEFRM:
         /* READ */
         if (!activeChannel->full) {
            activeChannel->data = pFrendInt->FrendState.fr_mem[CUR_BYTE_ADDR()];
            pFrendInt->FrendState.fr_addr++;
            activeChannel->full = TRUE;
         }
         break;
      case FcFEFRSM:
         /* READ AND SET */
         if (!activeChannel->full) {
            /* Return either the top or bottom byte of the address--but do */
            /* not change the address register. */
            if(pFrendInt->FrendState.fr_next_is_second) {
               activeChannel->data = pFrendInt->FrendState.fr_mem[1+CUR_BYTE_ADDR()];
               pFrendInt->FrendState.fr_next_is_second = FALSE;  /* Probably unnecessary. */
            } else {
               activeChannel->data = pFrendInt->FrendState.fr_mem[CUR_BYTE_ADDR()];
               /* Set top bit of word. */
               pFrendInt->FrendState.fr_mem[CUR_BYTE_ADDR()] |= 0x80;
               pFrendInt->FrendState.fr_next_is_second = TRUE;
            }
            activeChannel->full = TRUE;
         }
         break;
      case FcFEFWM0:
         /* WRITE MODE 0 - One PP word considered as 2 6-bit bytes, written to a 16-bit FE word. */
         /* It's a write-type I/O */
         if (activeChannel->full) {
            /*
            **  Output data.
            */
            activeChannel->full = FALSE;
         }
         break;
      case FcFEFWM:
         /* WRITE MODE 1 - Two consecutive PP words (8 in 12) written to a 16-bit FE word. */
         /* First PP word goes to upper 8 bits of FE word, confusingly called bits 0-7. */
         if (activeChannel->full) {
            /*
            **  Output data.
            */
            pFrendInt->FrendState.fr_mem[CUR_BYTE_ADDR()] = (Byte8)activeChannel->data;
            pFrendInt->FrendState.fr_addr++;
            activeChannel->full = FALSE;
         }
         break;
      default:
         if(TraceFrend>=LL_WARNING) LogOut("FREND: Did not process I/O; prev func=%4o", pFrendInt->FrendState.fr_last_func_code);
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
static void msufrendActivate(void)
{
   if(TraceFrend) LogOut("Activate");
   activeChannel->active = TRUE;
}

/*--------------------------------------------------------------------------
**  Purpose:        Handle disconnecting of channel.
**
**  Parameters:     Name        Description.
**
**  Returns:        Nothing.
**
**------------------------------------------------------------------------*/
static void msufrendDisconnect(void)
{
   if(TraceFrend) LogOut("Disconnect");
   activeChannel->active = FALSE;
}

/*---------------------------  End Of File  ------------------------------*/
