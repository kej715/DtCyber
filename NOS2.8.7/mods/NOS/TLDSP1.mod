TLDSP1
*IDENT    TLDSP1 WMB/SCC.    87/06/30.
*/        ****   NOS 2 OS.
*/        ****   NO PSR.
*/        ****   REQUIRES - NS2397.
*/        ****   TIELINE/NP V2-678.
*/        *****  PROVIDE CREATING-JSN *DSP* ROUTING
*/        PARAMETER FOR *TLF* OUTPUT FILES.
*/
*/        *****  ALLOW *TLF* *EBCDIC* INTERNAL CHARACTERISTIC.
*DECK     DSP
*I,47
*         NOTE - FOR *TLF* CALLS, ADDITIONAL INFORMATION IS 
*                PROVIDED IN (FET+15).
*
*T,FET+15 24/ OJSN, 36/ 0
*
*I,322
*
*         OJSN = ORIGINAL JOB SEQUENCE NAME (JSN).
*I,855
 TLFC     CON    0           *TLF* CALL 
*D,2448
          PJN    PRS0.1      IF NOT SUBSYSTEM
          ADN    TLSI-LSSI
          NJN    PRS1        IF NOT *TLF* CALL
          RJM    GFA         SET PARAMETER BLOCK ADDRESS
          ADN    1           GET TERMINATION FLAGS
          CRD    CM 
          LDD    CM 
          NJN    PRS1        IF *TLF* SUBSYSTEM TERMINATION 
          AOM    TLFC        FLAG NORMAL *TLF* CALL
          UJN    PRS1        CHECK ENTRY POINTS
  
 PRS0.1   BSS
*/D,4082
*/        SBN    RRIC+1
*/D,NS2397.17                 (4140)
*D,NS2580.12
  
*         SET ADDITIONAL *TIELINE/NP* ROUTING.
  
 SOD15    LDM    TLFC
          ZJN    SOD16       IF NOT *TLF* CALL
          RJM    GFA         SET PARAMETER BLOCK ADDRESS
          ADN    15 
          CRD    CM          SET CREATION JSN
          LDD    CM 
          ZJN    SOD16       IF NO JSN SPECIFIED
          LMC    2R 
          ZJN    SOD16       IF BLANK JSN SPECIFIED
          LDD    CM 
          STM    CJSS
          LDD    CM+1
          STM    CJSS+1
 SOD16    UJP    SODX        RETURN
*/        END OF MODSET.
