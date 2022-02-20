TLROUT1
*IDENT    TLROUT1  WMB.      87/06/30.
*/        ****   NOS 2 OS.
*/        ****   NO PSR.
*/        ****   REQUIRES - NONE.
*/        ****   TIELINE/NP V2-678.
*/        *****  PERMIT *TLF* CONVERSION MODE (CM) SPECIFICATION
*/        ON ROUTE TO INPUT (STORED IN DATA DECLARATION).
*DECK     ROUTE
*I,25
*         CM=XX - *TLF* CONVERSION MODE.
*I,93
*         ROUTE INCORRECT *CM* PARAMETER.
*I,170
*T, W7    12/ DD, 24/ , 6/0, 18/ EFLAGS 
*T,W10-16 60/
*I,188
*         DD - DATA DECLARATION.
*         EFLAGS - EXTENDED *DSP* FLAGS.
*I,198
          CON    0
          CON    0
          CON    0
          CON    0
          CON    0
          CON    0
          CON    0
          CON    0
          CON    0
          CON    0
          CON    0
 TDSPL    EQU    *-TDSP
          ERRNZ  TDSPL-EPBL  *DSP* EXTENDED BLOCK LENGTH ERROR
*I,219
 TCDD     SPACE  4,15
**        TCDD - *TLF* CONVERSION MODE/DATA DECLARATION TABLE.
*
*         42/ CM, 6/ 0, 12/ DD
*
*         CM     = *TLF* CONVERSION MODE (DISPLAY CODE).
*         DD     = DATA DECLARATION (DISPLAY CODE).
  
  
 TCDD     BSS
          VFD    42/0LUC,6/0,12/0LC6      UPPER CASE DISPLAY CODE
          VFD    42/0LUCDC,6/0,12/0LC6    UPPER CASE DISPLAY CODE
          VFD    42/0LUL,6/0,12/0LC8      UPPER/LOWER CASE DISPLAY CODE
          VFD    42/0LULDC,6/0,12/0LC8    UPPER/LOWER CASE DISPLAY CODE
          VFD    42/0LAS,6/0,12/0LUS      ASCII8 (7/12)
          VFD    42/0LASCII8,6/0,12/0LUS  ASCII8 (7/12)
          VFD    42/0LEB,6/0,12/0LUU      EBCDIC (8/12)
          VFD    42/0LEBCDIC,6/0,12/0LUU  EBCDIC (8/12)
 TCDDL    EQU    *-TCDD
*D,553,554
 KCM      SPACE  4,10
**        KCM - PROCESS *CM=XX* (CONVERSION MODE).
*
*         EXIT   DATA DECLARATION EQUIVALENCE STORED.
  
  
 KCM      BSS
          SB2    TCDD        SET EQUIVALENCE TABLE FWA
          SB3    TCDD+TCDDL  SET EQUIVALENCE TABLE LWA+1
          SX0    7777B
          SA5    TDSP+1
 KCM1     GE     B2,B3,KCM2  IF CONVERSION MODE NOT FOUND
          SA1    B2          GET TABLE ENTRY
          BX2    -X0*X1
          SB2    B2+B1       ADVANCE TABLE ADDRESS
          BX2    X2-X3
          NZ     X2,KCM1     IF NAMES DO NOT COMPARE
          R=     X3,FREB     SET EXTENDED FLAG
          BX6    X5+X3
          SA6    A5 
          BX2    X0*X1       EXTRACT DATA DECLARATION
          R=     X3,EFDD     SET DATA DECLARATION EXTENDED FLAG
          SA5    TDSP+7
          LX2    48          POSITION DATA DECLARATION
          BX6    X5+X3
          BX6    X6+X2       INSERT DATA DECLARATION
          SA6    A5 
          EQ     EPRX        RETURN
  
 KCM2     MESSAGE  (=C+ ROUTE INCORRECT *CM* PARAMETER.+),3,R
          EQ     ABT1        ABORT
*/        END OF MODSET.
