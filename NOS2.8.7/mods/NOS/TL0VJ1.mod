TL0VJ1
*IDENT    TL0VJ1 WMB.        87/06/30.
*/        ****   NOS 2 OS.
*/        ****   NO PSR.
*/        ****   REQUIRES - NONE.
*/        ****   TIELINE/NP V2-678.
*/        *****  DETECT IBM JOB CARDS, AND CALL *0IJ* TO
*/        PROCESS IBM JOB NAME.
*DECK     0VJ
*I,155
*         0IJ - PROCESS *IBM* JOB CARD. 
*I,323
          LDI    CN          CHECK *IBM* JOB
          LMC    2R//
          NJN    VJC0.1      IF NOT *IBM* JOB CARD
  
*         PROCESS *IBM* JOB CARD.
  
          LDC    OIJ0        SET LOAD ADDRESS
          RAD    LA 
          EXECUTE  0IJ,*     LOAD AND EXECUTE *0IJ*
          RJM.   EXR
          LDC    *           RESTORE LOAD ADDRESS 
 VJCA     EQU    *-1         (SET IN *PRS*)
          STD    LA 
          RJM    ISS         INITIALIZE SYSTEM SECTOR
          LJM    RVJX        RETURN
*I,326
 VJC0.1   BSS    0
*/I,355
*I,261L700.61
 OIJ0     SPACE  4,10
 OIJ0     EQU    *+5         *0IJ* LOAD ADDRESS
*I,1226
 ISS      SPACE  4,10
          ERRNG  *-OIJ0-ZIJL *0IJ* OVERFLOW INTO *ISS*
*I,1439
*                (VJCA) = *0VJ* LOAD ADDRESS.
*/I,1474
*I,252L678.40
          LDD    LA          SAVE *0VJ* LOAD ADDRESS
          STM    VJCA
*/        END OF MODSET.
