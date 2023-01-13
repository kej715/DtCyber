VFPMFIX
*IDENT VFPMFIX
*/
*/  THIS MODSET CORRECTS *MLR* TO SET UP ITS CALL TO MONITOR
*/  FUNCTION *VFMN* CORRECTLY FOR NOS 2.8.7.
*/
*DECK MLR
*D 875,877
          STD    CM+3
          LDD    IR+4
          STD    CM+4
*D 879
          STD    CM+1
          LDD    CP
          SHN    -7
          STD    CM+2
