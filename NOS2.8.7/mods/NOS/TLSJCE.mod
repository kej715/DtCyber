TLSJCE
*IDENT    TLSJCE WMB.        87/06/30.
*/        ****   NOS 2 OS.
*/        ****   NO PSR.
*/        ****   REQUIRES - NONE.
*/        ****   TIELINE/NP V2-678.
*/        *****  DEFINE DEFAULT DESTINATION LID FOR
*/        NON-CDC SYSTEMS.
*DECK     COMSJCE
*I,27
 COMSJCE  SPACE  4,10
**        NON-CDC SYSTEM DEFAULTS.
  
 NCDL     MICRO  1,,/RH1/    DESTINATION LID
*EDIT     COMSJCE
*/        END OF MODSET.
