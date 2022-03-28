*IDENT GOPLOT     
*/      
*/      
*/      
*/   THIS MOD TURNS ON THE PLOT PROCEDURE       
*/      
*/      
*/      
*COMPILE CONTRL   
*YANK NOPLOT      
*/      
*/      
*/      
*/ THE FOLLOWING SHOULD BE EDITED AND USED IF NEEDED      
*/ SEE INSTALLATION MANUAL FOR DETAILS
*/      
*/      
*/      
*/ INITIALIZATION 
*/      
*COMPILE PLOTER   
*DELETE PXG0003.1 
      CALL BGNPLT(99)       
*/      
*/ INITIAL ORIGIN POSITION  
*/      
*COMPILE PLOTER   
*DELETE PLOTER.106
      CALL PLOT(1.5,0.0,-3) 
*/      
*/ MAP SYMBOL CODE INTO GRAPHIC CODE  
*/      
*COMPILE PLT3     
*DELETE PLT3.123  
      DATA ISYMB/1,0,3,4,5,2,11,14,10,16,15,6,19,17,18/   
*/      
*/ SET ASPECT RATIO         
*/      
*COMPILE MSTART   
*DELETE MSTART.82 
      HW=...      
*/      
*/ WARNING MESSAGE
*/      
*COMPILE PLT1     
*DELETE PLT1.348,PLT1.359   
      IF (.NOT.QONL) WRITE (IOLP,780) 
  780 FORMAT (//" - - - WARNING - - -"//        
     +        " NO PLOTS WILL BE PRODUCED UNLESS THE PLOT FILE"     
     +        " (TAPE99) IS DISPOSED."/" THE FOLLOWING JOB",        
     +        " SETUP WILL YIELD MEDIUM LINE ON PLAIN PAPER-"//     
     +        "      SPSS."/"      DISPOSE,TAPE99,GP=CP3."//        
     +        " SEE THE CALCOMP MANUAL FOR OTHER PAPER/PEN",        
     +        " COMBINATIONS."/)      
      CALL LINECT(12)       
      IF (QONL) WRITE (IOLP,790)      
  790 FORMAT (/" - - - WARNING - - -"//1X,"NO PLOTS WILL ",         
     +         "BE PRODUCED UNLESS TAPE99 IS DISPOSED ",  
     +         "TO THE PLOTTER")      
*/      
*/ USE COMPASS (CIO) I/O    
*/      
*COMPILE IOSTUF   
*DELETE IOSTUF.49 
 FTN4     EQU    2
*/      
*/ USE FORTRAN WRITE"S      
*/      
*COMPILE IOSTUF   
*DELETE IOSTUF.49 
 FTN4     EQU    1
*/      
*/ ELIMINATE STLD.RM MACRO  
*/      
*COMPILE IOSTUF   
*DELETE IOSTUF.81 
*/      
*/ INCLUDE STLD.RM MACRO    
*/      
*COMPILE IOSTUF   
*DELETE IOSTUF.81 
          SQ       STLD.RM USEBT=(C),USERT=(S),USE=(...),OMIT=(CMM) 
*/      
*/ SPECIFY PLOT FILE FORMAT 
*/      
*COMPILE IOSTUF   
*DELETE IOSTUF.83 
          FILE   LFN=TAPE99,OF=N,CF=N,PD=OUTPUT,FWB=PLB,  
,BFS=513,FO=SQ,RT=S,BT=C    
*/      
*/ AVOID CMM CONFLICT BELOW PSR 472   
*/      
*COMPILE IOSTUF   
*DELETE IOSTUF.83 
          FILE    LFN=TAPE99,OF=N,CF=N,PD=OUTPUT,FWB=PLB, 
,BFS=513,FO=SQ,FET=PLF      
*/ - - - - - - - - - - - - - - - - - - - - - - - - - -  LAST CARD GOPLOT      
