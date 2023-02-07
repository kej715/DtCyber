NCCTLF
*IDENT    NCCTLF    KEJ. 20/08/01
*/
*/  ADD *RSCS* AND *PRIME* AS SUPPORTED REMOTE SPOOLER TYPES,
*/  ADD SUPPORT FOR RETURNING OUTPUT TO THE NOS WAIT QUEUE,
*/  SWITCH LEFT AND RIGHT *K* DISPLAY SCREENS, AND CHANGE THE
*/  NAME OF *DPRINT* TO *TLPRINT* TO AVOID CLASH WITH CYBIS.
*/
*DECK     TLF
*D,416
 .SFWQ    EQU    48          WAIT QUEUE REQUESTED 
*D,704
          VFD    36/0,3/A9EX,3/A6IC,18/0  AS/6/ASCII6
*I,780
*T,QAC+8  24/    , 12/ DC
*I,793
*         DC  = DISPOSITION CODE.
*I,899
          VFD    60/0
          VFD    60/0
          VFD    60/0
*D,919,920
          DATA   0LRSCS      RSCS
          DATA   0LPRIME     PRIME
*I,1626
          SA1    TQAC+8      GET DISPOSITION CODE 
          MX0    -12         DISPOSITION CODE MASK
          SX5    B1 
          LX1    36 
          SX2    2RTO
          BX1    -X0*X1
          SA4    B3+B0       FLAG WORD
          LX5    .SFWQ
          BX1    X1-X2
          BX6    -X5*X4      CLEAR WAIT QUEUE FLAG
          NZ     X1,DDP6.1   IF DISPOSITION CODE NOT *TO*
          BX6    X5+X6       SET WAIT QUEUE FLAG
 DDP6.1   SA6    A4 
*D,1977,1979
          MOVE   5,KDPB,MSA1 CLEAR MESSAGE AREA
          MOVE   5,KDPB,MSA2
          MOVE   5,KBUF,MSA2 SET KEYBOARD INPUT IN DISPLAY
*D,2084,2086
          SA6    MSA1        STORE MESSAGE IN LEFT SCREEN
          LX7    X3 
          SA7    A6+B1
*D,2118,2119
 KDDS     VFD    6/0,18/KBUF,18/LBUF,18/RBDS
 KDLS     VFD    6/0,18/KBUF,18/LBUF,18/RBLS
*D,2124
**        COMMAND DIRECTORY DISPLAY (RIGHT SREEN).
*D,2179,2193
*D,2196
**        DEVICE STATUS DISPLAY (LEFT SCREEN).
*D,2243,2244
          DSL    01,K,(LAST CONSOLE MESSAGE.)
          DSL    01,K+1,(      )
*D,2251
          DSL    01,K+2,(      )
*D,2258
  
*         LINK TO COMMON LEFT SCREEN MESSAGE AREA 
  
          VFD    12/7777B,30/0,18/MSA
*D,2260
**        LINE STATUS DISPLAY (LEFT SCREEN).
*D,2311
  
*         LINK TO COMMON LEFT SCREEN MESSAGE AREA 
  
 MSAL     VFD    12/7777B,30/0,18/MSA
  
**        COMMON LEFT SCREEN MESSAGE AREA
  
 MSA      DSL    01,40,(      )
 MSA1     DATA   2L 
          DATA   2L 
          DATA   2L 
          DATA   2L 
          DATA   2L 
          DSL    01,41,(      )
 MSA2     DATA   2L 
          DATA   2L 
          DATA   2L 
          DATA   2L 
          DATA   2L 
  
          CON    0           END OF DISPLAY
*I,2739
          SA1    MSAL        STORE LINK TO COMMON MESSAGE AREA
          BX7    X1 
*D,3352,3353
 STP12    SB3    6           MAXIMUM NUMBER OF WORDS TO COPY
          SB4    B0          INDEX OF TARGET WORD 
          SA1    X5 
          SB5    RMS1        BASE ADDRESS OF TARGET
 STP13    ZR     X1,STP15    IF END OF MESSAGE
          BX6    X1 
          SA6    B4+B5
          SB4    B4+B1
          SA1    A1+B1
          LT     B4,B3,STP13 IF MORE WORDS TO COPY
          SB4    B0          INDEX OF TARGET WORD 
          SB5    RMS2        BASE ADDRESS OF TARGET
 STP14    ZR     X1,STP15    IF END OF MESSAGE
          BX6    X1 
          SA6    B4+B5
          SB4    B4+B1
          SA1    A1+B1
          LT     B4,B3,STP14 IF MORE WORDS TO COPY
 STP15    SA1    MSAL        STORE LINK TO COMMON MESSAGE AREA
          BX6    X1 
          SA6    B4+B5
*D,3742
*         BEGIN,,ZZZTLNP,    $SSSSLIDW  JJJJJJJJ  MMMMMMM$,$
*I,3746
*         W        = *W* IF OUTPUT TO BE DISPOSED TO WAIT QUEUE
*D,3802
*           COMMENT. /TIELINE/SSSSLIDW  JJJJJJJJ  MMMMMMM
*I,3806
*         W        = *W* IF OUTPUT TO BE DISPOSED TO WAIT QUEUE
*D,3853
*         //*       /TIELINE/SSSSLIDW  JJJJJJJJ  MMMMMMM   N
*I,3857
*         W        = *W* IF OUTPUT TO BE DISPOSED TO WAIT QUEUE
*I,3901
 DRS      SPACE  4,20
**        DRS - PROCESS DEFAULT ROUTING FOR *RSCS*
*
*         DOWNLINE TEXT INSERTED -
*
*                  1         2         3         4         5
*         12345678901234567890123456789012345678901234567890
*                  6         7         8
*
*         ***       /TIELINE/SSSSLIDW  JJJJJJJJ  MMMMMMM   N
*         NNNNNN   FFFFFFF   UUUUUUU
*
*         SSSS     = JOB SEQUENCE NAME. 
*         LID      = SOURCE LID.
*         W        = *W* IF OUTPUT TO BE DISPOSED TO WAIT QUEUE
*         JJJJJJJJ = USER JOB NAME (JOB CARD NAME).
*         MMMMMMM / NNNNNNN = OWNER FAMILY/USER NAME.
*         FFFFFFF / UUUUUUU = DESTINATION FAMILY/USER NAME. 
*
*         ENTRY  (B1) = 1.
*                (B3) = *SSB* ADDRESS.
*
*         EXIT   (X5) = 1 = LINES INSERTED.
*                (B2) = CONVERSION MODE.
*                (PDRA - PDRA+7) = ROUTING TEXT.
*
*         USES   X - 0, 1, 2, 5, 6, 7.
*                B - 2.
*                A - 0, 1, 2, 6, 7.
*
*         CALLS  FDR.
  
  
 DRS      BSS    0           ENTRY
          SA0    PDRA+1      SET *FDR* FWA
          RJ     FDR         FORMAT DOWNLINE ROUTING
  
*         POSITION ROUTING TEXT.
  
          SA1    DRSA        GET ROUTING HEADER
          MX0    54 
          BX7    X1 
          SB2    6           SET NUMBER OF WORDS TO SHIFT
          SX2    1R          PRESET 80TH CHARACTER
 DRS1     SA1    A0+B2       GET NEXT WORD
          LX1    6           POSITION TEXT
          BX6    X0*X1       EXTRACT UPPER
          BX6    X6+X2       INSERT PREVIOUS LOWER
          BX2    -X0*X1      EXTRACT LOWER
          SA6    A1+         STORE SHIFTED TEXT WORD
          SB2    B2-B1       DECREMENT TEXT WORD INDEX
          PL     B2,DRS1     IF NOT ALL TEXT MOVED
          SA7    A0-B1       STORE ROUTING HEADER 
          SX5    B1          SET LINE COUNT
          SB2    B0          SET UPPERCASE CONVERSION
          EQ     PDR1        EXIT
  
  
 DRSA     DATA   10H***
 DRP      SPACE  4,20
**        DRP - PROCESS DEFAULT ROUTING FOR *PRIME RJE*
*
*         DOWNLINE TEXT INSERTED -
*
*                  1         2         3         4         5
*         12345678901234567890123456789012345678901234567890
*                  6         7         8
*
*         /*        /TIELINE/SSSSLIDW  JJJJJJJJ  MMMMMMM   N
*         NNNNNN   FFFFFFF   UUUUUUU
*
*         SSSS     = JOB SEQUENCE NAME. 
*         LID      = SOURCE LID.
*         W        = *W* IF OUTPUT TO BE DISPOSED TO WAIT QUEUE
*         JJJJJJJJ = USER JOB NAME (JOB CARD NAME).
*         MMMMMMM / NNNNNNN = OWNER FAMILY/USER NAME.
*         FFFFFFF / UUUUUUU = DESTINATION FAMILY/USER NAME. 
*
*         ENTRY  (B1) = 1.
*                (B3) = *SSB* ADDRESS.
*
*         EXIT   (X5) = 1 = LINES INSERTED.
*                (B2) = CONVERSION MODE.
*                (PDRA - PDRA+7) = ROUTING TEXT.
*
*         USES   X - 0, 1, 2, 5, 6, 7.
*                B - 2.
*                A - 0, 1, 2, 6, 7.
*
*         CALLS  FDR.
  
  
 DRP      BSS    0           ENTRY
          SA0    PDRA+1      SET *FDR* FWA
          RJ     FDR         FORMAT DOWNLINE ROUTING
  
*         POSITION ROUTING TEXT.
  
          SA1    DRPA        GET ROUTING HEADER
          MX0    54 
          BX7    X1 
          SB2    6           SET NUMBER OF WORDS TO SHIFT
          SX2    1R          PRESET 80TH CHARACTER
 DRP1     SA1    A0+B2       GET NEXT WORD
          LX1    6           POSITION TEXT
          BX6    X0*X1       EXTRACT UPPER
          BX6    X6+X2       INSERT PREVIOUS LOWER
          BX2    -X0*X1      EXTRACT LOWER
          SA6    A1+         STORE SHIFTED TEXT WORD
          SB2    B2-B1       DECREMENT TEXT WORD INDEX
          PL     B2,DRP1     IF NOT ALL TEXT MOVED
          SA7    A0-B1       STORE ROUTING HEADER 
          SX5    B1          SET LINE COUNT
          SB2    B0          SET UPPERCASE CONVERSION
          EQ     PDR1        EXIT
  
  
 DRPA     DATA   10HTYPE     '
*D,3911
*         NOTE,,NR.+ /TIELINE/SSSSLIDW  JJJJJJJJ  MMMMMMM
*I,3915
*         W        = *W* IF OUTPUT TO BE DISPOSED TO WAIT QUEUE
*D,3955
*         *.         /TIELINE/SSSSLIDW  JJJJJJJJ  MMMMMMM
*I,3959
*         W        = *W* IF OUTPUT TO BE DISPOSED TO WAIT QUEUE
*D,TLF9.1 
*            COMMENT./TIELINE/SSSSLIDW  JJJJJJJJ  MMMMMMM
*I,4003
*         W        = *W* IF OUTPUT TO BE DISPOSED TO WAIT QUEUE
*D,4062,4063
          VFD    42/0,18/DRS  IBM/RSCS
          VFD    42/0,18/DRP  PRIME/RJE 
*D,4106
*         ***        /TIELINE/SSSSLIDWFCJJJJJJJJ  MMMMMMM
*I,4110
*         W        = *W* IF OUTPUT TO BE ROUTED TO WAIT QUEUE.
*D,4165
*         ***                  /TIELINE/SSSSLIDWFCJJJJJJJJ
*D,4168
*         ***        /TIELINE/SSSSLIDWFCJJJJJJJJ  MMMMMMM
*I,4172
*         W        = *W* IF OUTPUT TO BE ROUTED TO WAIT QUEUE.
*D,4236
*          /TIELINE/SSSSLIDWFCJJJJJJJJ  MMMMMMM   NNNNNNN
*D,TLF9.11
*                    /TIELINE/SSSSLIDWFCJJJJJJJJ  MMMMMMM
*I,4240
*         W        = *W* IF OUTPUT TO BE ROUTED TO WAIT QUEUE.
*I,4278
          BX3    X1 
          MX0    -6 
          LX3    -12
          SA2    B3+B0       GET FLAG WORD
          BX3    -X0*X3
          SX7    B1 
          SX3    X3-1RW
          LX7    .SFWQ
          BX6    -X7*X2      CLEAR WAIT QUEUE FLAG
          NZ     X3,URN1.1   IF WAIT QUEUE NOT REQUESTED
          BX6    X7+X6       SET WAIT QUEUE FLAG
 URN1.1   SA6    A2 
*D,4368,4369
          VFD    42/0,18/URH  RSCS
          VFD    42/0,18/URN  PRIME
*I,5427
          SX4    B1 
          SA1    B3          GET FLAG WORD
          LX4    .SFWQ       POSITION WAIT QUEUE FLAG
          BX6    -X4*X1      INITIALIZE WAIT QUEUE FLAG
          SA6    A1 
*D,6445
*          /TIELINE/SSSSLIDW  JJJJJJJJ  MMMMMMM   NNNNNNN
*I,6449
*         W        = *W* IF OUTPUT TO BE ROUTED TO WAIT QUEUE.
*I,6476
          SX7    B1 
          SA2    B3+B0       LOAD FLAG WORD
          LX7    .SFWQ
          SX6    1RW
          BX2    X2*X7
          LX6    12 
          ZR     X2,FDR1     IF WAIT QUEUE NOT REQUESTED
          BX1    X1+X6       APPEND *W* TO JSN AND LID
 FDR1     BSS    0
*I,7495
          SA3    B3+B0       GET FLAG WORD
          SX4    B1 
          LX4    .SFWQ
          BX4    X3*X4
          ZR     X4,ROF3.1   IF WAIT QUEUE NOT REQUESTED
          SX7    B1+B1       OFFSET TO DC=WT
 ROF3.1   BSS    0
*I,7626
          VFD    24/0,12/2LWT,24/0  (DC=WT)
*D,TL23000.77
          SB2    X4-.USER2
*D,7736
          GT     B2,STRX     IF SPOOLER ORDINAL OUT OF RANGE
*D,7739
*         *JES2*, "RBF*, *INTERCOM5*, *RSCS*, OR *PRIME*
*D,8559
 TCFFILE  FILEC  0,0,(FET=CFPN),(USN=NETADMN)
*/        END OF MODSET.
~eor
DPRI871
*IDENT    DPRI871
*DECK     DPRINT
*D,1
      PROGRAM TLPRINT(OUTPUT,ZZZZTL,IN=201B,
*/        END OF MODSET.
