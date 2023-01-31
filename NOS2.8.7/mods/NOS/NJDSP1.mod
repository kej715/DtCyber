NJDSP1
*IDENT    NJDSP1 DOM.        87/04/21.
*/        ****   NOS 2.5.2 OS.
*/        ****   NJEF/NHP V2-678.
*/        *****  *DSP* MODS FOR *NJEF*. 
*DECK     DSP
*I,542
*         0NJ - COPY *NJEF* TRAILER INFORMATION.
*D,DSP43.3
*D,747
 DSP5.1   RJM    PIR         PROCESS IMMEDIATE ROUTE
*I,776
*         FOR *NJEF* JOBS, COPY *NJEF* TRAILING INFORMATION.
*I,778
          RJM    CNT         COPY *NJEF* TRAILER (IF NECESSARY)
*I,4279
 CNT      SPACE  4,10
**        CNT - COPY *NJEF* TRAILER INFORMATION.
*
*         *CNT* CALLS *0NJ* TO COPY THE *NJEF* TRAILER INFORMATION
*         UNLESS THE FILE IS AN OUTPUT FILE DESTINED FOR THE LOCAL
*         HOST, THE FILE IS BEING ROUTED BY *NJEF* OR THERE IS NO
*         TRAILER INFORMATION TO COPY.
*
*         CALLS  *0NJ*
  
  
 CNT      SUBR               ENTRY/EXIT 
          LDD    QT          CHECK FILE QUEUE TYPE
          LMK    INQT
          ZJN    CNT1        JIF ROUTE TO INPUT QUEUE
          LDM    DLAT        CHECK FILE DLID ATTRIBUTES
          SHN    21-13
          PJN    CNT1        JIF NOT TO LOCAL HOST
          LDM    NJSS+4      *NJF* TRAILER INFORMATION FLAG 
          SCN    1           CLEAR FLAG 
          STM    NJSS+4      RESTORE CLEARED *NJF* TRAILER BYTE
 CNT1     BSS
          LDM    NJSS+4      CHECK *NJEF* TRAILING INFORMATION FLAG
          LPN    1
          ZJN    CNTX        RETURN IF NOT *NJEF* FILE
          LDM    EBIT+1      EXTENDED *DSP* FLAG BITS
          SHN    21-13
          MJN    CNTX        RETURN - *DSP* BY *NJEF*
          LDC    501B        SAVE SYSTEM SECTOR
          STD    T1 
 CNT2     BSS
          LDM    BFMS,T1     GET SYSTEM SECTOR FROM SYSTEM SECTOR AREA
          STM    EBUF,T1     SAVE SYSTEM SECTOR TO *EOI* SECTOR AREA
          SOD    T1 
          PJN    CNT2        JIF MORE TO SAVE
          EXECUTE  0NJ,OVL3  COPY *NJEF* TRAILER
          LDC    501B        RESTORE SYSTEM SECTOR
          STD    T1 
 CNT3     BSS
          LDM    EBUF,T1     GET SYSTEM SECTOR FROM *EOI* SECTOR AREA 
          STM    BFMS,T1     RESTORE OLD SYSTEM SECTOR AREA 
          SOD    T1 
          PJN    CNT3        JIF MORE TO RESTORE
          LJM    CNTX        RETURN
*I,4340
 PIR      SPACE  4,10
**        PIR - PROCESS IMMEDIATE ROUTE.
*
*         ENTRY  (EBIT - EBIT+1) = EXTENDED FLAG BITS.
*                (ST) = STATUS WORD.
*
*         EXIT   IF *DSP* BY *NJEF*, SYSTEM SECTOR FLAG SET.
*                (ST) - FLAG SET TO ISSUE *UADM* FUNCTION.
*
  
  
 PIR      SUBR               ENTRY/EXIT 
          LDN    IUFL        SET FLAG TO ISSUE *UADM* FUNCTION
          RAD    ST 
          LDM    EBIT+1      EXTENDED *DSP* FLAG BITS
          SHN    21-13
          PJN    PIRX        RETURN NOT *DSP* BY *NJEF*
          LDM    NJSS+4      SET *NJEF* TRAILING INFORMATION FLAG
          SCN    1
          ADN    1
          STM    NJSS+4
          UJN    PIRX        RETURN
*I,251L664.136
 CNT      EQU    /3DC/CNT
*I,251L664.138
 PIR      EQU    /3DC/PIR
*/        END OF MODSET.
