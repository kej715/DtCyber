*IDENT NCCPPA   KEJ.
*/
*/ THIS MODSET CHANGES PPA SO THAT THE JOBS IT CREATES FOR THE
*/ PLATO SUBSYSTEMS DO NOT CALL *DIS* AFTER ABNORMAL EXIT. IT
*/ ALSO MODIFIES THE RAX AND FLX VALUES IT REQUESTS TO PREVENT
*/ ERROR EXIT DUE TO ATTEMPTING TO WRITE BEYOND THE END OF ECS
*/ FIELD LENGTH.
*/
*DECK PPA
*D 873
*         VFD    60/4LDIS.
*D 887
*         VFD    60/4LDIS.
*D 901
*         VFD    60/4LDIS.
*D 1061,1062
 RAX      VFD    24/10000B*100B
 FLX      VFD    24/30000B*100B
*EDIT PPA