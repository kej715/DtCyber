/* lmbi.h - Definitions for the LMBI tables.
 * LMBI is a region of memory in the Interdata 7/32.  
 * Multiple 7/32's can access memory that is on LMBI boards,
 * but this capability is not used in Desktop Cyber.
 * Our interest in LMBI is that it's where a bunch of FREND
 * tables are stored.
 *
 * All "addresses" are actually indices to FrendState.fr_mem[].
 * 
 * Mark Riordan  July 4, 2004
 */

#ifndef LMBI_H_INCLUDED
#define LMBI_H_INCLUDED

/* Symbols in FREND that are missing from frendsym.h */
#define ADLWPG   0x8000
#define MAXCORE  0x8004
#define MAXLC    0x8008
#define CURB80   0x800c
#define CURB240  0x8010
#define MAXB80   0x8012
#define MAXB240  0x8014

/*--- End of symbols missing from frendsym.h */

#define FWAMBI_1  0x40000        /* FWA OF FIRST BANK OF COMMON MEMORY (LMBI) */
#define FWAMBI_2  0x80000        /* FWA OF SECOND BANK OF COMMON MEMORY (LMBI) */

#define CLR_TS   0x1             /* Special value to store into interlock to drop it. */

/* Definition of Interdata 7/32 circular list, as implemented by 
 * the ATL (Add to top of list), ABL,
 * RBL (Remove from bottom of list) and RTL instructions.
 * A list has an 8-byte header, immediately followed by 4-byte slots.
 *   Bytes 0-1: # of slots
 *         2-3: Number used; 0 means empty.  If = # of slots, list is full.
 *         4-5: Current top
 *         5-6: Next bottom
 *      remain: consecutive slots of 4 bytes.  The first is #'ed 0.
 */
#define H_CIRCLIST_N_SLOTS_TOT   0
#define H_CLNUM                  0 /* FESYM macro for the same thing. */
#define H_CIRCLIST_N_USED        2
#define H_CLUSED                 2
#define H_CIRCLIST_CUR_TOP       4
#define H_CLTOP                  4
#define H_CIRCLIST_NEXT_BOT      6
#define H_CLBOT                  6
#define H_CIRCLIST_HEADER_BYTES  8
#define CIRCLIST_SLOT_SIZE_BYTES 4
#define CircListSlotAddr(fwaList, islot) (fwaList + H_CIRCLIST_HEADER_BYTES + islot*CIRCLIST_SLOT_SIZE_BYTES)
#define CIRCLIST_NOT_FOUND       0xffff

#define LE_BF80   84              /* # of bytes in a 80-character header */
#define LE_BF240  3*LE_BF80       /* 240 CHARACTER BUFFER (+HHEADER) */
#define LE_BANM   LE_BF80         /* LENGTH OF BANNER MESSAGE LINES */
#define NE_BANM   5               /* NUMBER OF BANNER MESSAGE LINES */
#define LE_LOGM   LE_BF80         /* LENGTH OF LOGIN MESSAGE LINES */
#define NE_LOGM   1
#define LE_MALC   2               /* MEMORY ALLOCATION TABLE ENTRY SIZE */

/* Initialization Complete flag is the MISC 1 halfword of the 
 * first entry in the LMBI pointer table.  We set it to 1.
 */
#define H_INICMP  (FWAMBI_1+H_PWM1)

/* Fields for the LMBI POINTER TABLE.  Each entry in this 
 * table describes another table in LMBI. */
#define W_PWFWA   0 /* FWA OF TABLE */
#define H_PWLE    4 /* LENGTH OF A TABLE ENTRY */
#define H_PWNE    6 /* NUMBER OF TABLE ENTRIES */
#define H_PWM1    8 /* MISC. 1 */
#define H_PWM2   10 /* MISC. 2 */
/* Length of a LMBPT table entry in bytes: */
#define L_LMBPT 12

/* Offsets from FWAMBI_1 of entries in the LMBI POINTER TABLE for various tables.
 * To get the actual FWA of each table, you must dereference the 
 * W_PWFWA field in that table's entry in the LMBIPT. */
#define PW_MISC   0x40000
#define PW_FPCOM  0x4000C
#define PW_BF80   0x40018
#define PW_BF240  0x40024
#define PW_BFREL  0x40030
#define PW_BANM   0x4003C
#define PW_LOGM   0x40048
#define PW_SOCK   0x40054
#define PW_DVSK   0x40060
#define PW_PORT   0x4006C
#define PW_PTBUF  0x40078
#define PW_MALC   0x40084
#define PW_ALLOC  0x40090

/***       /DATAHDR - PORT BUFFER DATA HEADER
 *
 *         THE DATA HEADER IS A UNIVERSAL STRUCTURE WHICH
 *         PREFIXES ALL DATA BUFFERS IN THE 7/32.
 *
 *         DHBCT    THE NUMBER OF BYTES OF DATA FOLLOWING THE
 *                  HEADER, INCLUDING THE HEADER.  DATA STARTS
 *                  IMMEDIATELY FOLLOWING THE HEADER.
 *
 *         DHTYPE   RECORD TYPE IS ONE OF THE FP.XXX TYPES
 *                  DEFINED SUBSEQUENTLY.  THIS HAS MEANING TO
 *                  VARIOUS RECORD PROCESSORS IN THE 7/32.
 *                  ITS MOST IMPORTANT MEANING, HOWEVER, IS TO
 *                  IDENTIFY THE TYPES OF PROTOCOL AND DATA RECORDS
 *                  PASSED BETWEEN FREND AND THE 6500.
 *
 *         DHCHC    CHARACTER CODE, ONE OF THE CC.FDCXX
 *                  CODES. */
/* Fields for DH */
#define C_DHBCT   0 /* BYTE COUNT (including header) */
#define C_DHTYPE  1 /* RECORD TYPE */
#define C_DHCHC   2 /* CHARACTER CODE */
#define  CC_FDCAF 0x3
#define  CC_FDCAS 0x2
#define  CC_FDCBF 0x5
#define  CC_FDCBI 0x4
#define  CC_FDCOM 0x0
#define  CC_FDMAX 0x5
#define C_DHCTL   3 /* CONTROL INFORMATION (flags) */
/* flags in C_DHCTL */
#define C_DHCNEW  3
#define C_DHCEOL  3
#define V_DHCASY 0x10
#define V_DHCECH 0x4
#define V_DHCEOL 0x80  /*  DHCEOL   SET IF LINE HAD AN END-OF-LINE.  THIS               
                           CAUSES THE 1ST CHARACTER OF THE NEXT LINE           
                           TO BE INTERPERTED AS A CARRIAGE CONTROL             
                           IF DHCHC IS OM OR AF. */
#define V_DHCNEW 0x40  /*  DHCNEW   NEW LINE FLAG.  IF SET, THE 1ST                     
                           CHARACTER OF THIS LINE IS ALWAYS INTERPERTED        
                           AS A CARRIAGE CONTROL IF DHCHC IS OM OR AF.         
                           CHECKED BY SKOTCL.    */
#define C_DHCNTA 0x3
#define V_DHCNTA 0x20  /*  DHCNTA   NON-THROW-AWAY. IF SET, THE RECORD WILL             
                           NOT BE THROWN AWAY IF IT IS ON THE PORT OUTPUT      
                           STACK, AND THE USER DOES AN ESCAPE.                 
                           CHECKED BY INESC.     */
#define V_DHCRES 0x8    /* Used in printing only */
#define V_DHCSS  0x2    /* If set, force single space */
#define C_DHCASY 0x3
#define V_DHCASY 0x10   /* If set, message is asychronous (do right away) */

/* Length of a DTAHDR table entry in bytes: */
#define L_DTAHDR  4
#define LE_DTAHDR 4

/* FRONT-END PROTOCOL RECORD DEFINITIONS
 * Messages to/from FREND are sent in "protocol records"
 * for both control and data ports.  These symbols define the
 * fields.  The first 4 bytes are a data record header.*/
#define C_FPPT   0x4 /* offset of PORT NUMBER from fwa of record */
#define C_FPP2   0x5 /* offset of second parameter */
#define C_FPP3   0x6
#define C_FPP4   0x7
#define C_FPP5   0x8
#define C_FPP6   0x9
#define C_FPP7   0xA
#define C_FPP12  0xF
#define C_FPP13  0x10

/*  SYMBOLS FOR THE NUMBER OF PARAMETERS IN PROTOCOL RECORDS
 *  EXCLUDING THE DATA HEADER BYTES                         
 *  VARIABLE LENGTH RECORDS ARE OMITTED.*/
#define NP_OPEN    6                                              
#define NP_CLO     2                                              
#define NP_ABT     1                                              
#define NP_INBS    2                                              
#define NP_OTBS    2                                              
#define NP_ORSP    4                                              
#define NP_STAT    1                                              
#define NP_FCRP    2                                              
#define NP_EOR     2                                              
#define NP_EOF     0                                              
#define NP_CPOPN   1                                              
#define NP_CPCLO   2                                              
#define NP_TIME    13                                             
#define NP_CAN     1                                              
#define NP_INST    6                                              
#define NP_GETO    7                                              
#define NP_NEWPR   14                                             
#define NP_REWJ    1                                              
#define NP_ENDJ    1                                              
#define NP_EOI     1                                              
#define NP_SKB     3                                              
#define NP_SKIP    3                                              
#define NP_ACCT    8                                              
#define NP_COPY    1                                              
#define NP_EOREI   0                                              
#define NP_MAX     14    /* MAXIMUM PROTOCOL RECORD SIZE  */

#define OT_1200    7   /* 7/32 1200 BAUD */

/*--- #defines generated by MkSymFromListing.awk */

/* Fields for MI */
#define H_MIHR    0x0 /* CURRENT TIME, HOURS (ASCII) */
#define H_MIMI    0x2 /* MINUTES (ASCII) */
#define H_MISEC   0x4 /* SECONDS (ASCII) */
#define H_MIMON   0x6 /* CURRENT DATE, MONTH (ASCII) */
#define H_MIDAY   0x8 /* DAY (ASCII) */
#define H_MIYR    0xA /* YEAR (ASCII) */
#define W_MIVER   0xC /* FREND VERSION NUMBER (ASCII) */
/* Length of a MISC table entry in bytes: */
#define L_MISC 0x10

/* Fields for FPCOM */
#define H_FEDEAD  0x0 /* 7/32 MUST CLEAR THIS PERIODICALLY */
#define H_FCMDIK  0x2 /* 1FP COMMAND INTERLOCK */
#define W_FCMD    0x4 /* 1FP COMMAND */
#define H_FCMDTY  W_FCMD /* Used in ISR65; not the same as C_FCMDTY */
#define W_LFCNT   0x8 /* TOTAL LINE FREQUENCY INTERRUPT COUNT */
#define H_NBUFIK  0xC /* NEXT BUFFER INTERLOCK */
#define H_NOBUF   0xE /* 1=TOO FEW BUFFERS FOR 1FP TO WRITE */
#define W_NBF80  0x14 /* NEXT 80 CHAR BUFFER */
#define W_NBF240 0x18 /* NEXT 240 CHARACTER BUFFER */
/* More FPCOM values */
#define C_FCMDVA      4  /* Command value from 1FP (only for some cmds) */
#define C_FCMDTY      5  /* Command type from 1FP.  See FC_* codes. */
#define H_FCMDPT      6  /* port number */
#define C_CPOPT       7  /* port number for FC.CPOP; in this special case, */
                         /* it's a byte and not a halfword. */
/* Length of a FPCOM table entry in bytes: */
#define L_FPCOM 0x1C

/* Fields for SK:  SOCK table */
#define C_SKTYPE  0x0 /* DT.PALLS, DT.PALHS, DT.TTY, ETC. */
#define C_SKIBD   0x1 /* INITIAL BAUD: BL.AUTO, BL.100, ETC. */
#define H_SKDEV   0x2 /* DEVICE ADDRESS (RECEIVE SIDE) */
#define W_SKPNUM  0x4 /* PHONE NUMBER IN PSEUDO-BCD */
#define H_SKOCBA  0x8 /* OUTPUT CCB ADDRESS (USE LDHL TO LOAD */
#define H_SKNUM   0xA /* SOCKET NUMBER (1...N) */
#define C_SKSYS   0xC /* FREND SUBSYSTEM TYPE (SYS.INT) */
#define C_SKBUS   0xD /* BUS IDENTIFIER */
#define C_SKNLOG  0xE /* NON-ZERO IF NO AUTO _LOGIN */
#define C_SKIOTM  0xF /* NON-ZERO IF SOCKET IS AN I/O TERMINA */
#define C_SKCXST 0x10 /* CMUX STRAP OPTION (0,1,2) */
#define C_SKCXBL 0x11 /* DEFAULT 'HIGH SPEED' DATA RATE (BL.* */
#define C_SKIFLG 0x12 /* INITIALIZATION TIME FLAGS */
#define C_SKRSFG 0x13 /* RESTRICTED INTERACTIVE FLAGS */
#define C_SKINTT 0x14 /* INITIAL TERMINAL TYPE (0 = NONE) */
#define C_SKTTY  0x18 /* TERMINAL TYPE */
#define C_SKFBD  0x19 /* FINAL BAUD RATE (SEE IBD) */
#define C_SKPAR  0x1A /* PARITY:  PTY.NO, PTY.EV, ETC. */
#define C_SKCRC  0x1B /* CR DELAY IS CHARACTERS */
#define C_SKLFC  0x1C /* LF DELAY IN CHARACTERS */
#define C_SKHTC  0x1D /* HORIZONTAL TAB DELAY IN CHARACTERS */
#define C_SKVTC  0x1E /* VERTICAL TAB DELAY IN CHARACTERS */
#define C_SKFFC  0x1F /* FORM-FEED DELAY IN CHARACTERS */
#define C_SKLINE 0x20 /* SCREEN SIZE IN LINES */
#define C_SKRM   0x22 /* RIGHT MARGIN */
#define C_SKTLT  0x23 /* LT    TRANSLATE TABLE (NON-STANDARD) */
#define C_SKFECC 0x45 /* FRONT-END COMMAND CHAR CODE */
#define C_SKNPCC 0x46 /* NPC INTERPRETATION MODE */
#define H_SKINLE 0x48 /* INPUT LINE LENGTH */
#define   L_LINE   240  /* max input line length */
#define C_SKECTB 0x4A /* CTB   ECHO TABLE (NON-STANDARD) */
#define C_SKALCH 0x4B /* ALTERNATE CHARACTER SET */
#define W_SKALXL 0x50 /* XLATE TABLE ADDR FOR ALT. CHAR. SET */
#define W_SKTID1 0x54 /* ID/4  TERMINAL ID, FULLWORD PART */
#define C_SKTID2 0x5C /* TERMINAL ID, LEFTOVER TWO CHARACTERS */
#define W_SKFLAG 0x60 /* ALL SOCKET FLAGS */
#define  H_SKOEOL 0x60
#define  J_SKOEOL 0x6
#define  H_SKOSUP 0x60
#define  J_SKOSUP 0x5
#define  H_SKINEL 0x60
#define  J_SKINEL 0x4
#define C_SKVCOL 0x64 /* VIRTUAL COLUMN NUMBER (I) */
#define C_SKCT1  0x65 /* CONNECTION TYPE (CT.SK, CT.PT) */
#define C_SKCT2  0x66 /* CONNECTION TYPE (SECOND CONNECTION) */
#define C_SKCTIN 0x67 /* COUNT OF QUEUED 'SKINCL' TASKS */
#define H_SKCN1  0x68 /* CONNECTION NUMBER (usually is a port number) */
#define H_SKCN2  0x6A /* CONNECTION NUMBER (SECOND CONNECTION */
#define H_SKID   0x6C /* ID NUMBER */
#define H_SKMTRP 0x6E /* MONITOR PORT NUMBER */
#define H_SKLIT  0x70 /* NON-ZERO FOR NO INPUT XLATION */
#define C_SKISTA 0x72 /* INPUT STATE (IN.OFF, IN.IDLE, ETC.) */
/* Values for C_SKISTA */
#define     IN_IDLE  0x1
#define     IN_IO    0x4
#define     IN_OFF   0x0
#define     IN_WAIT  0x2

#define C_SKDCTL 0x73 /* DHCTL BYTE FROM CURRENT OUTPUT LINE */
#define W_SKDATA 0x74 /* INPUT DATA FOR DEBUGGING (I) */
#define W_SKECBF 0x78 /* ADDRESS OF ECHO BUFFER (I) */
#define W_SKINBF 0x7C /* INPUT BUFFER FWA (I) */
#define H_SKSWOT 0x60 /* WAITING FOR OUTPUT ON LOGOUT.  WHEN SET, */
#define J_SKSWOT 0x3  /* WILL CAUSE ALL PENDING OUTPUT TO BE PRINTED */
#define H_SKINCC 0x80 /* INPUT CHARACTER COUNT (I) */
#define H_SKECHO 0x82 /* NONZERO TO BLOCK ECHO (I) */
#define H_SKSUPE 0x82 
#define J_SKSUPE 0x2  /* Suppress echo on next input line */
#define H_SKETOG 0x82 /* TOGGLE ECHOBACK */
#define J_SKETOG 0x1
#define H_SKESCP 0x62 /* 1 WHEN ESCAPE IS PENDING */
#define J_SKESCP 0x6

#define W_SKPORD 0x84 /* PASSWORD ORDINAL (IF RESTRICTED) */
#define W_SKOTCL 0x88 /* actual SOCKET OUTPUT CIRCULAR LIST--not a pointer */
/* Length of a SOCK table entry in bytes: */
#define LE_SOCK 0xA4
#define L_SKOCL    5   /* # ENTries IN OUTPUT CIRC LIST for this socket */

/*  THESE VALUES APPEAR IN C.SKCT1, C.SKCT2, C.PTCT1.
 *  THEY INDICATE WHETHER THE CORRESPONDING H.XXCN FIELD
 *  IS A SOCKET NUMBER (CT.SOCK) OR A PORT NUMBER  (CT.PORT).
 */
#define CT_PORT  1   /* PORT CONNECTION */
#define CT_SOCK  2   /* SOCKET CONNECTION */


#define PTN_MAX   3   /* port numbers <= this are control ports */
#define PTN_MAN   1   /* control port # for MANAGER */
/* Fields for PT:  PORT table */
#define C_PTTYPE  0x0 /* PORT TYPE */
#define H_PTCPN   0x2 /* CONTROL PORT NUMBER(0 FOR CONT PT) */
#define C_PTCT1   0x4 /* CONNECTION 1 TYPE */
#define H_PTCN1   0x6 /* CONNECTION 1 NUMBER */
#define H_PTID    0x8 /* ID NUMBER */
#define H_PTWTBF  0xA /* SET IF 1FP NEEDS BUFFERS */
#define W_PTIN    0xC /* INPUT BUFFER POINTER FOR 1FP */
#define H_PTINIK 0x10 /* INPUT BUFFER INTERLOCK */
#define H_PTNDDT 0x12 /* NEED-DATA FLAGS */
#define   H_PTXFER 0x12
#define   J_PTXFER 0xE
#define   H_PTOTBS 0x12
#define   J_PTOTBS 0xF

#define F_PTIN   0xAA   /* FREND WILL PLACE THIS VALUE IN THE FIRST BYTE OF
                         * W.PTIN WHEN IT PUTS A BUFFER ADDRESS IN THE LOWER THREE BYTES.    
                         * 1FP WILL IGNORE ANY NONZERO DATA IN W.PTIN UNLESS THE FLAG
                         * VALUE IS THERE.  THIS IS DONE AS A WORK-AROUND FOR A HARDWARE 
                         * PROBLEM, IN WHICH 1FP SOMETIMES SEES NOISE DATA IN THIS WORD. */


#define H_PTNDIK 0x14 /* NEED-DATA INTERLOCK */
#define H_PTFLAG 0x18 /* ALL PORT FLAGS */
   /* Shift counts and offsets (from beg of port) for single-bit flags in H.PTFLAG */
#define   H_PTCPDT 0x18
#define   J_PTCPDT 0x5
#define   H_PTEOL  0x18
#define   J_PTEOL  0x4
#define   H_PTFCLO 0x18
#define   J_PTFCLO 0xF
#define   H_PTLKON 0x18
#define   J_PTLKON 0x9
#define   H_PTLKPO 0x18
#define   J_PTLKPO 0xA
#define   H_PTNDCL 0x18
#define   J_PTNDCL 0xE
#define   H_PTNLEP 0x18
#define   J_PTNLEP 0xC
#define   H_PTNLON 0x18
#define   J_PTNLON 0xB
#define   H_PTPEOI 0x18
#define   J_PTPEOI 0x6
#define   H_PTS65  0x18
#define   J_PTS65  0x3
#define   H_PTSCNT 0x18
#define   J_PTSCNT 0x1
#define   H_PTSENB 0x18
#define   J_PTSENB 0x0
#define   H_PTSWOT 0x18
#define   J_PTSWOT 0x2
#define   H_PTTNX3 0x18
#define   J_PTTNX3 0x8
#define   H_PTWTIB 0x18
#define   J_PTWTIB 0x7
#define   H_PTWTLC 0x18
#define   J_PTWTLC 0xD
#define H_PTFLG2 0x1A /* SECOND HALFWORD OF PORT FLAGS */
   /* Shift counts and offsets (from beg of port) for single-bit flags in H.PTFLG2 */
#define   H_PTABIN 0x1A
#define   J_PTABIN 0x0

#define W_PTPBUF 0x1C /* PARTIAL LINE BUFFER ADDRESS */
#define W_PTOT   0x20 /* OUTPUT BUFFER POINTER FROM 1FP (I) */
#define H_PTOTIK 0x24 /* OUTPUT BUFFER INTERLOCK */
#define H_PTOTNE 0x26 /* NUMBER OF EMPTY OUTPUT SLOTS (I) */
#define W_PTOTCL 0x28 /* ADDRESS OF OUTPUT CIRC LIST */
#define W_PTINCL 0x2C /* ADDRESS OF INPUT CIRC LIST */
/* Length of a PORT table entry in bytes: */
#define LE_PORT 0x30
/* Shift counts for flags in H.PTFLAG and H.PTFLG2 */
#define J_PTCPDT 0x5
#define J_PTEOL  0x4
#define J_PTFCLO 0xF
#define J_PTLKON 0x9
#define J_PTLKPO 0xA
#define J_PTNDCL 0xE
#define J_PTNLEP 0xC
#define J_PTNLON 0xB
#define J_PTOTBS 0xF
#define J_PTPEOI 0x6
#define J_PTS65  0x3
#define J_PTSCNT 0x1
#define J_PTSENB 0x0
#define J_PTSWOT 0x2
#define J_PTTNX3 0x8
#define J_PTWTIB 0x7
#define J_PTWTLC 0xD
#define J_PTXFER 0xE

#define H_PTCPDT 0x18
#define H_PTEOL  0x18
#define H_PTFCLO 0x18
#define H_PTFLAG 0x18
#define H_PTLKON 0x18
#define H_PTLKPO 0x18
#define H_PTNDCL 0x18
#define H_PTNLEP 0x18
#define H_PTNLON 0x18
#define H_PTPEOI 0x18
#define H_PTS65  0x18
#define H_PTSCNT 0x18
#define H_PTSENB 0x18
#define H_PTSWOT 0x18
#define H_PTTNX3 0x18
#define H_PTWTIB 0x18
#define H_PTWTLC 0x18

/*--- End of #defines generated by MkSymFromListing.awk */

/* Numbers of buffers in port circular lists */
#define L_DTIN    10   /* DATA PORT INPUT BUFFERS -- if port is data port*/
#define L_DTOT    20   /* DATA PORT OUTPUT BUFFERS */
#define L_CPIN    64   /* CONTROL PORT INPUT BUFFERS -- if port is control port. */
#define L_CPOT    10   /* CONTROL PORT OUTPUT BUFFERS */

/* Definition of buffer header.  This is a 4-byte header that goes */
/* at the front of every data buffer--for example, the 80- and 240-byte */
/* buffers. */
#define C_DHBCT  0   /* BYTE COUNT */
#define C_DHTYPE 1   /* Record type */
#define C_DHCHC  2   /* Character code */
#define C_DHCTL  3   /* Many bit fields go in this byte */

/* Symbols for commands in HERE IS records.  They refer to the */
/* C.DHTYPE fields in 80 or 240 char records from 1FP. */
/* These are taken from /FP in FESYM.  Some apply to control */
/* ports and some apply to data ports. */
#define FP_DATA 0  /* NORMAL DATA */
#define FP_OPEN 1  /* OPEN A CONNECTION */
#define FP_CLO 2  /* CLOSE A CONNECTION */
#define FP_ABT 3  /* ABORT A PROCESS */
#define FP_INBS 4  /* INPUT BUFFER STATUS */
#define FP_OTBS 5  /* OUTPUT BUFFER STATUS */
#define FP_ORSP 6  /* OPEN RESPONSE */
#define FP_STAT 7  /* REQUEST FOR STATUS MESSAGE */
#define FP_FCRP 8  /* REPLY CODE TO F.E. COMMAND */
#define FP_EOR 9  /* END-OF-RECORD */
#define FP_EOF 10  /* END-OF-FILE */
#define FP_UNLK 11  /* UNLOCK AND PROMPT */
#define FP_FEC 12  /* F.E. COMMAND */
#define FP_CPOPN 13  /* CONTROL PORT OPEN */
#define FP_CPCLO 14  /* CONTROL PORT CLOSE */
#define FP_BULK 15  /* UNLOCK WITH BLANKING */
#define FP_CAN 16  /* CANCEL INPUT LINE */
#define FP_EOI 17  /* END-OF-INFORMATION */
#define FP_GETO 18  /* GET A PRINT FILE */
#define FP_NEWPR 19  /* HERE IS A PRINT FILE */
#define FP_ENDJ 21  /* JOB ENDED BY OPERATOR */
#define FP_INST 22  /* INSTRUMENTATION */
#define FP_SKB 23  /* SKIP BACKWARD ON FILE */
#define FP_SKIP 24  /* SKIP FORWARD ON FILE */
#define FP_ACCT 25  /* ACCOUNTING DATA */
#define FP_BLDAT 27  /* BLOCK DATA */
#define FP_COPY 28  /* PRINT ANOTHER COPY */
#define FP_EOREI 29  /* EOR W/ ERROR INDICATION */
#define FP_FECNE 30  /* F.E. COMMAND (NO REPLY) */
#define FP_CMDPE 31  /* PLUTO CMD FROM F.E. */
#define FP_CMDCY 32  /* PLUTO CMD FROM CYBER */
#define FP_RPYPE 33  /* PLUTO REPLY TO F.E. */
#define FP_RPYCY 34  /* PLUTO REPLY TO CYBER */
#define FP_SCRTR 35  /* SEND SCREDIT TO TERM MODE */
#define FP_TIME 36  /* TIME/DATE */

#define   V_EXTREQ  0x8000  /* Set in request code if it's an external request. */
#endif
