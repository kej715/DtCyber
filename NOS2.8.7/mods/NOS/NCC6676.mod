NCC6676
*IDENT NCC6676  KEJ.
*/
*/ THIS MODSET ENABLES *IAF* TO USE 1TN TO DRIVE A 6676 MULTIPLEXOR
*/ AS WAS DONE BY *TELEX* ON KRONOS AND NOS 1.
*/
*DECK 1TN 
*D 795,806
*I NS22000.48 
          LDC    OBUF        SET FIRST OUTPUT BUFFER ADDRESS
          STM    SVMA 
*I 838
          ADM    TNTD,ME     SET LIMIT ADDRESS
          STM    /CTL/MGRA
*D NS22000.56 
  
*         PROCESS MULTIPLEXER OUTPUT/INPUT. 
  
 SVM      BSS    0
          LDM    TNTD-1,ME   UPDATE OUTPUT BUFFER POINTER 
          RAM    SVMA 
          SBN    1
          STD    DO 
          LDN    0
          STD    SM 
          LDD    CM+4        SET SELECT CODE
          LPC    7000 
          STD    T3 
          LMN    1           ISSUE WRITE FUNCTION 
          FAN    MC 
          IJM    SVM2,MC     IF INACTIVE CONTINUE 
          IJM    SVM2,MC     IF INACTIVE CONTINUE 
          UJN    SVM3        FUNCTION TIMEOUT 
  
 SVM1     LDC    ITDA        REPORT EQUIPMENT MALFUNCTION
          RJM    DFM
          LJM    MXE
  
 SVM2     LDM    TNTD,ME     WRITE OUTPUT DATA
          ACN    MC 
          OAM    *,MC 
 SVMA     EQU    *-1
          NJN    SVM1        IF INCOMPLETE TRANSFER 
          FJM    *,MC 
          DCN    MC+40
          LDD    T3          ISSUE READ FUNCTION
          LMN    3
          FAN    MC 
          IJM    SVM4,MC     IF INACTIVE CONTINUE 
          IJM    SVM4,MC     IF INACTIVE CONTINUE 
 SVM3     DCN    MC+40
          LJM    MXE0        FUNCTION TIMEOUT 
  
 SVM4     BSS    0
          LDM    TNTD,ME     READ INPUT BUFFER
          ACN    MC 
          IAM    IBUF,MC
          NJN    SVM1        IF INPUT NOT COMPLETE
          DCN    MC+40
          RJM    /CTL/MGRX   PROCESS TERMINALS
*D 935,979
  
 ITDA     DATA   C*I/O INCOMPLETE.*
*D NS22000.67 
 MGR      AOD    DI          INCREMENT DATA IN BUFFER (STM) 
          LMC    *
 MGRA     EQU    *-1
          NJN    MGR1        IF NOT LAST TERMINAL ON THIS MUX 
          LJM    *           ENTRY/EXIT 
 MGRX     EQU    *-1
  
 MGR1     AOD    DO          INCREMENT DATA OUT POINTER 
          LDI    DI 
*/        END OF MODSET.
