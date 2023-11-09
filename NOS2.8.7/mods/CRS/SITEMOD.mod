*IDENT,SITEMOD   MSM/JP/BMW  88/07/26
*/        ****   CRAY NOS STATION 1.18
*/        ****   OPTIONAL SITE MODIFICATIONS.
*/        *****  MODIFICATIONS FOR SITE-SPECIFIC PARAMETERS.
*/        THESE INCLUDE: 
*/        -- CHANGING THE SEGMENT SIZE. 
*/        -- LIMITING THE MAXIMUM STREAM COUNTS.
*/        -- CHANGING THE SPOOLING TASK DEFAULT JOB AND USER STATEMENT
*/        -- CHANGING THE DEFAULT JCL FOR STAGING TASKS
*/           THIS INCLUDES JOB AND USER STATEMENT 
*/        -- CHANGING THE MAXIMUM NUMBER OF CONTROL CARDS IN TEXT FIELD.
*/        -- CHANGING THE ORIGIN TYPE FOR STAGING TASKS.
*/        -- CHANGING THE SERVICE CLASS FOR STAGING AND SPOOLING TASKS.
*/        -- CHANGING THE STATION AUTO INITIALIZATION MODE. 
*/        -- CHANGING THE DEFAULT STAGING MODE.
*/        -- CHANGING THE SIZE OF THE STATION SLOT.
*/        -- CHANGING THE STATION-S DEFAULT LOGICAL ID(S).
*/        -- CHANGING THE STATION-S IDENTIFIER AS KNOWN TO COS.
*/        -- CHANGING THE NUMBER OF INTERACTIVE TERMINALS ICF SUPPORTS.
*/        -- CHANGING THE MAXIMUM PROCESS NUMBER ICF SUPPORTS.
*/        -- CHANGING THE MAXIMUM PLAY FILE SIZE FOR ICF.
*/        -- ENABLE / DISABLE ICF DEBUG MODE AND DAYFILE TRACES.
*/        -- GENERATE THE STATION FOR A 63 CHARACTER SET NOS SYSTEM.
*/        -- FORCE ALL CRAY JOB OUTPUT TO BE CONVERTED TO DISPLAY CODE
*/        -- CHANGE THE TEXT FIELD LITERAL DELIMITER CHARACTER
*/        -- DETERMINE WHETHER A USER CAN SEE ONLY THE STATUS
*/           OF HIS OWN CRAY JOBS
*/
*/        THE MODSET, AS IT IS, CHANGES ALL PARAMETERS TO THEIR DEFAULT
*/        VALUES.  IF AN INSTALLATION WISHES ANY OF THESE PARAMETERS
*/        CHANGED TO NEW VALUES, ALL THAT IS NEEDED IS TO MODIFY THIS 
*/        DECK; NO SEARCHING THROUGH CODE IS NECESSARY.
*I HISTORY.11
*
*         86/11/21. 
*         SITE-SPECIFIC PARAMETER MODIFICATIONS.  THESE INCLUDE: 
*         -- CHANGING THE SEGMENT SIZE. 
*         -- LIMITING THE MAXIMUM STREAM COUNTS.
*         -- CHANGING THE SPOOLING TASK DEFAULT JOB AND USER STATEMENT
*         -- CHANGING THE DEFAULT JCL FOR STAGING TASKS
*            THIS INCLUDES JOB AND USER STATEMENT 
*         -- CHANGING THE MAXIMUM NUMBER OF CONTROL CARDS IN TEXT FIELD.
*         -- CHANGING THE ORIGIN TYPE FOR STAGING TASKS.
*         -- CHANGING THE SERVICE CLASS FOR STAGING AND SPOOLING TASKS.
*         -- CHANGING THE STATION AUTO INITIALIZATION MODE. 
*         -- CHANGING THE DEFAULT STAGING MODE.
*         -- CHANGING THE SIZE OF THE STATION SLOT.
*         -- CHANGING THE STATION-S DEFAULT LOGICAL ID(S).
*         -- CHANGING THE STATION-S IDENTIFIER AS KNOWN TO COS.
*         -- CHANGING THE NUMBER OF INTERACTIVE TERMINALS ICF SUPPORTS.
*         -- CHANGING THE MAXIMUM PROCESS NUMBER ICF SUPPORTS.
*         -- CHANGING THE MAXIMUM PLAY FILE SIZE FOR ICF.
*         -- ENABLE / DISABLE ICF DEBUG MODE AND DAYFILE TRACES.
*         -- GENERATE THE STATION FOR A 63 CHARACTER SET NOS SYSTEM.
*         -- FORCE ALL CRAY JOB OUTPUT TO BE CONVERTED TO DISPLAY CODE
*         -- CHANGE THE TEXT FIELD LITERAL DELIMITER CHARACTER
*         -- DETERMINE WHETHER A USER CAN SEE ONLY THE STATUS
*            OF HIS OWN CRAY JOBS
*
*C HISTORY
*/
*/  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/  *                                                                 *
*/  *   CHANGING THE SEGMENT SIZE.                                    *
*/  *     THE SIZE OF A SEGMENT, IN BITS, MUST BE DIVISABLE BY        *
*/  *     BOTH 60 AND 64, SO THAT IT WILL BE A MUTIPLE OF BOTH CYBER  *
*/  *     AND CRAY WORD SIZES.                                        *
*/  *     THE SMALLEST SEGMENT SIZE SUPPORTED IS 176 CYBER WORDS      *
*/  *     WHICH IS 165 CRAY WORDS.                                    *
*/  *     NOTE THAT AS THE CYBER STATION ONLY SUPPORTS SEGMENTS OF    *
*/  *     ONE SUBSEGMENT, SEGMENT SIZE = SUBSEGMENT SIZE.             *
*/  *                                                                 *
*/  *   NOTE: THE CHANGE TO COMMON DECK COMCONST MUST FOLLOW          *
*/  *         EXACTLY THE SYNTAX GIVEN, BECAUSE COMCONST IS CALLED    *
*/  *         FROM PASCAL AND COMPASS DECKS.                          *
*/  *                                                                 *
*/  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/
*D,COMCONST.14,20
 SEGSIZE  =      3840        ; (* SEGMENT SIZE IN CYBER WORDS         *)
                               (* THE SEGMENT SIZE IN BITS MUST BE A  *)
                               (* MULTIPLE OF 60 AND OF 64.           *)
                               (* IF *SEGSIZE* IS CHANGED, THE        *)
                               (* DEFINITION OF *SEGBUFS* MUST BE     *)
                               (* CHANGED ACCORDINGLY                 *)
  
*D COMCONST.21,COMCONST.22   NEAR COMCONST.20
 SEGBUFS  =      28805       ; (* ICF SEGMENT BUFFER SIZE IN BYTES    *)
                               (* SEGBUFS MUST BE 7.5*SEGSIZE+5       *)
*C COMCONST
*/
*/  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/  *                                                                 *
*/  *   CHANGING THE SPOOLING TASK DEFAULT JOB AND USER STATEMENT     *
*/  *     SPOOLED STATION TASKS DO NOT RUN UNDER THE USER             *
*/  *     OF THE FILE OWNER, THE WAY STAGED TASKS DO.  THE SITE       *
*/  *     MUST DEFINE A USER UNDER WHICH THESE TASKS ARE TO RUN.      *
*/  *                                                                 *
*/  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/
*D BLDTSK.729,BLDTSK.730
          DIS    ,*CTASK,T7777.*
          DIS    ,*USER,BCSCRAY,${../../site.cfg:PASSWORDS:BCSCRAY:CRAYOPN}.*
*/
*/  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/  *                                                                 *
*/  *   CHANGING THE STAGING TASK DEFAULT JCL                         *
*/  *     THE DEFAULT STAGING TASK JCL IS COPIED BEFORE THE JCL       *
*/  *     STATEMENTS THE USER SPECIFIES IN THE TEXT FIELD.            *
*/  *     THE USER STATEMENT IS TAKEN FROM THE TEXT FIELD OR FROM     *
*/  *     THE STATION SLOT INFORMATION IF PRESENT.                    *
*/  *     A SITE CAN TAILOR THIS JCL TO THEIR NEEDS                   *
*/  *     - CHANGE THE JOB TIME LIMIT FOR STAGING TASKS               *
*/  *     - CHANGE THE DEFAULT USER STATEMENT FOR STAGING TASKS       *
*/  *     - CHANGE OR REMOVE THE CHARGE STATEMENT                     *
*/  *     - INSERT A CALL TO SITE DEFINED PROCEDURE                   *
*/  *                                                                 *
*/  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/
*D,BLDTSK.740,BLDTSK.745
          DIS    ,*CTASK,T777.*
 JCLUSER  CON    10H_USER,             USER STATEMENT
 JCLUS    CON    10H_BCSCRAY  ,        DEFAULT USER NUMBER
 JCLPW    CON    10H_CRAYOPN  ,        DEFAULT PASSWORD
 JCLFM    CON    10H_         .        DEFAULT FAMILY
          DATA   1H ,1H ,1H ,1H ,0     FILL LINE TO 80 CHAR + ZERO
*  INSERT STATEMENTS BEFORE THIS LINE
*C BLDTSK 
*/
*/  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/  *                                                                 *
*/  *   CHANGING THE MAXIMUM NUMBER OF CONTROL CARDS IN TEXT FIELD.   *
*/  *                                                                 *
*/  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/
*D PSCCON.182
 MAXLINE = 6;             (* MAXIMUM NUMBER OF CONTROL CARDS IN TEXT *)
*C PSCCON 
*D CIPARMS.4
 MAXLINE  EQU    6           MAXIMUM NUMBER OF CONTROL CARDS IN TEXT FIELD
*C CIPARMS
*/
*/  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/  *                                                                 *
*/  *   CHANGING THE ORIGIN TYPE FOR STAGING TASKS.                   *
*/  *   CHANGING THE SERVICE CLASS FOR STAGING AND SPOOLING TASKS.    *
*/  *                                                                 *
*/  *   THE ORIGIN TYPE OF SPOOLONG TASKS MUST BE SYOT                *
*/  *   YOU MUST ALSO MAKE SURE THAT THE ORIGIN TYPE / SERVICE CLASS  *
*/  *   COMBINATION FOR STAGING TASKS IS VALID FOR THE USER.          *
*/  *                                                                 *
*/  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/
*D CRSTEXT.53,CRSTEXT.55
 STAGEOT  EQU    BCOT        ORIGIN TYPE FOR STAGING TASKS = BATCH
 SPOOLSC  EQU    2RSY        SERVICE CLASS FOR SPOOLING TASKS = SYSTEM
 STAGESC  EQU    2RBC        SERVICE CLASS FOR STAGING TASKS = BATCH
*/
*/  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/  *                                                                 *
*/  *   CHANGING THE DEFAULT STATION INITIALIZATION MODE.             *
*/  *   CHANGING THE DEFAULT STAGING MODE.                            *
*/  *                                                                 *
*/  *   THE DEFAULT STATION INITIALIZATION MODE CAN ALSO BE CHANGED   *
*/  *   ON THE *CRSTAT* CONTROL STATMENT, WHICH IS CALLED BY          *
*/  *   THE STATION STARTUP PROCEDURE CRS, CR1 OR CR2.                *
*/  *                                                                 *
*/  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/
*D CRSTEXT.126,CRSTEXT.133
*         THE FOLLOWING DEFINE THE INSTALLATION DEFAULT STATION
*         ACTIVITY.  IF I.WAIT IS NON-ZERO, THE STATION WAITS FOR
*         OPERATOR INTERVENTION (K.GO) BEFORE INITIALIZING. 
*         A ZERO VALUE FOR I.WAIT ALLOWS FOR AUTOMATIC INITIALIZATION.
*
*         I.WAIT IS USED BY THE CONTROL CARD CRACKER, *CRACKCC*.
  
 I.WAIT   EQU    0           DEFAULT IS AUTO-INITIALIZATION 
*D CRSTEXT.135,CRSTEXT.150
**        INITIAL STAGING MODE
*
*         I.STAGE DEFINES THE INITIAL STAGING MODE
*                1 = JOB STAGING ENABLED
*                2 = FILE STAGING ENABLED
*                3 = JOB AND FILE STAGING ENABLED 
*
*         IF I.STAGE IS ZERO, THE OPERATOR COMMAND K.STAGE,ON,XX
*         IS REQUIRED BEFORE STAGING IS ALLOWED.
*         K.STAGE,ON,JOBS   ALLOWS ONLY JOB STAGING.
*         K.STAGE,ON,FILES  ALLOWS ONLY FILE STAGING.
*         K.STAGE,ON,ALL    ALLOWS JOB AND FILE STAGING.
*         K.STAGE,ON        ALLOWS JOB AND FILE STAGING.
*
*         I.STAGE IS USED BY THE K-DISPLAY DRIVER, *KDISP*. 
 I.STAGE  EQU    3           DEFAULT IS JOB AND FILE STAGING ON
*C CRSTEXT
*/
*/  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/  *                                                                 *
*/  *   CHANGING MAXIMUM STREAM COUNTS.                               *
*/  *     BY DEFAULT, THE STATION IS ALLOWED UP TO 16 ACTIVE STREAMS. *
*/  *     AT SITES WISHING TO LIMIT THE AMOUNT OF SYSTEM RESOURCES    *
*/  *     WHICH MAY, AS A MAXIMUM, BE DEDICATED TO THE STATION,       *
*/  *     THE MAXIMUM STREAM COUNTS ARE OFTEN REDUCED.                *
*/  *                                                                 *
*/  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/
*D PSCCON.157,PSCCON.159
   INPUTMAX  = 8;            (* MAXIMUM ALLOWED INPUT STREAMS         *)
   OUTPUTMAX = 8;            (* MAXIMUM ALLOWED OUTPUT STREAMS        *)
   ACTIVEMAX = 16;           (* MAXIMUM ALLOWED ACTIVE STREAMS        *)
*C CRSTAT 
*/
*/  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/  *                                                                 *
*/  *   CHANGING THE STATION-S DEFAULT LID-S.                         *
*/  *     THE STATION WILL ONLY PROCESS SPOOLED OUTPUT JOBS DESTINED  *
*/  *     FOR SPECIFIC LID-S.  THESE LID-S MAY BE ASSEMBLED INTO THE  *
*/  *     STATION, SPECIFIED ON THE *CRSTAT* CONTROL STATEMENT, OR    *
*/  *     CHANGED BY OPERATOR TYPE-IN (THE *SET,LID* COMMAND).        *
*/  *                                                                 *
*/  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/
*D CRSTEXT.21,CRSTEXT.24
 LID1     MICRO  1,3, ${1}    DEFAULT STATION LID
 LID2     MICRO  1,3,        NO DEFAULT LID 2
 LID3     MICRO  1,3,        NO DEFAULT LID 3
 LID4     MICRO  1,3,        NO DEFAULT LID 4
*C CRSTEXT
*D ICFEX.72
  DEFAULTLID    = '${1}';                (* DEFAULT LOGICAL ID         *)
*D ICFEX.74
  ICFAPNAME     = 'ICF    ';            (* NAM APPLIC NAME FOR ICF    *)
*C ICFEX
*/
*/  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/  *                                                                 *
*/  *   CHANGING THE STATION SLOT SIZE.                               *
*/  *     THE STATION SLOT SIZE IS SPECIFIED IN CYBER WORDS.          *
*/  *     THE LARGER THE STATION SLOT, THE MORE INFORMATION A SITE    *
*/  *     CAN STUFF INSIDE OF IT (ROUTING INFORMATION, ACCOUNTING,    *
*/  *     ETC.).  NOTE THAT, WHEN CHANGING THE SLOT SIZE, ONE MUST    *
*/  *     ALSO CHANGE THE DEFINITIONS IN PASCAL OF THE SIZE OF THE    *
*/  *     DATASET HEADER, BOTH IN WORDS AND IN BITS.                  *
*/  *                                                                 *
*/  *   NOTE: THE CHANGE TO COMMON DECK COMCONST MUST FOLLOW          *
*/  *         EXACTLY THE SYNTAX GIVEN, BECAUSE COMCONST IS CALLED    *
*/  *         FROM PASCAL AND COMPASS DECKS.                          *
*/  *                                                                 *
*/  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/
*D,COMCONST.24
 SLOTSIZ  =      64          ; (* STATION SLOT SIZE IN CYBER WORDS    *)
*C,COMCONST
*/
*/  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/  *                                                                 *
*/  *   CHANGING THE STATION IDENTIFIER.                              *
*/  *     THE STATION IS KNOWN TO COS BY A TWO CHARACTER STATION      *
*/  *     IDENTIFIER, WHICH MUST BE UNIQUE FOR EACH STATION.  THIS    *
*/  *     IDENTIFIER CAN BE SET AT ASSEMBLY TIME, BY A *CRSTAT*       *
*/  *     STATEMENT PARAMETER, OR BY OPERATOR TYPE-IN (*SET,ID*       *
*/  *     COMMAND) BEFORE STATION-CRAY LOGON.  THE ID CANNOT BE       *
*/  *     CHANGED ONCE THE STATION IS LOGGED ONTO THE CRAY.           *
*/  *                                                                 *
*/  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/
*D VALUES.103,VALUES.104
   STATIONID = '${2}';         (* STATION ID AS KNOWN TO COS            *)
   CRAYID    = '${3}';         (* CRAY ID AS KNOWN TO COS               *)
*C CRSTAT 
*/
*/  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/  *                                                                 *
*/  *   CHANGING THE NUMBER OF TERMINALS ICF SUPPORTS.                *
*/  *                                                                 *
*/  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/
*D ICFEX.55,56
  MAXCON      = 10;                     (* MAX NUMBER OF NETWORK      *)
                                        (* CONNECTIONS                *)
*/
*/  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/  *                                                                 *
*/  *   CHANGING THE MAXIMUM PROCESS NUMBER ICF SUPPORTS.             *
*/  *                                                                 *
*/  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/
*D ICFEX.58,60
  MAXPN       = 40;                     (* MAX PROCESS NUMBER         *)
                                        (* MUST BE AT LEAST AS LARGE  *)
                                        (* AS I@IAAUT IN COS          *)
*C ICFEX
*/
*/  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/  *                                                                 *
*/  *   CHANGING THE MAXIMUM PLAY FILE SIZE FOR ICF.                  *
*/  *                                                                 *
*/  *   THE FILE SIZE IS GIVEN IN DISK SECTORS.                       *
*/  *   IT MUST NOT BE BIGGER THAN THE NOS IPRDECK PARAMETER CPTT.    *
*/  *                                                                 *
*/  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/
*D CRSTEXT.38,CRSTEXT.41
 I.MAXPF  EQU    50D         MAXIMUM SIZE OF AN ICF PLAY FILE
*                            IN DISK SECTORS. THIS MUST NOT BE
*                            BIGGER THAN THE NOS IPRDECK
*                            PARAMETER CPTT.
*C CRSTEXT
*/
*/  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/  *                                                                 *
*/  *   ENABLE / DISABLE ICF DEBUG MODE.                              *
*/  *   ENABLE / DISABLE ICF DAYFILE TRACES.                          *
*/  *                                                                 *
*/  *    - SEGMENT TRACE                                              *
*/  *    - TERMINAL MESSAGE TRACE                                     *
*/  *    - RECORD TRACE                                               *
*/  *    - NETWORK BLOCK TRACE                                        *
*/  *                                                                 *
*/  *    THE TRACE CAN GENERATE A LARGE AMOUNT OF DAYFILE MESSAGES    *
*/  *    AND SHOULD ONLY BE TURNED ON FOR DEBUGGING PURPORSES.        *
*/  *                                                                 *
*/  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/
*D,ICFEX.67,71
  ICFDEBUG      = FALSE;                (* NOT IN DEBUG MODE          *)
  ICFSEGTRACE   = FALSE;                (* NO SEGMENT TRACE           *)
  ICFTMTRACE    = FALSE;                (* NO TERMINAL MESSAGE TRACE  *)
  ICFRECTRACE   = FALSE;                (* NO RECORD TRACE            *)
  ICFNWBLKTRACE = FALSE;                (* NO NETWORK BLOCK TRACE     *)
*C ICFEX
*/
*/  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/  *                                                                 *
*/  *   GENERATE THE STATION FOR A 63 CHARACTER SET NOS SYSTEM        *
*/  *                                                                 *
*/  *   ACTIVATE THE FOLLOWING SYMBOL DEFINITION TO GENERATE THE      *
*/  *   STATION FOR A 63 CHARACTER SET NOS SYSTEM.                    *
*/  *                                                                 *
*/  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/
*D CRSTEXT.84,CRSTEXT.88
*         THE FOLLOWING COMMENT LINE SHOULD BE CHANGED INTO A SYMBOL
*         DEFINITION IF THE HOST SITE IS RUNNING WITH THE CDC
*         63-CHARACTER CHARACTER SET.
  
*CHR63    EQU    1           63-CHAR CHAR SET IF CHR63 DEFINED
*/
*/  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/  *                                                                 *
*/  *   FORCE ALL CRAY JOB OUTPUT TO BE CONVERTED TO DISPLAY CODE     *
*/  *     FOR DETAILS SEE THE FOLLOWING COMMENT                       *
*/  *                                                                 *
*/  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/
*D CRSTEXT.57,CRSTEXT.82
***       CHARACTER CONVERSION MODE FOR SPOOL AND STAGING FILES.
*
*         THE CHARACTER SET OF A CRAY INPUT JOB CAN BE EITHER
*         DISPLAY CODE OR ASCII8/12 CODE. IT IS DETERMINED
*         DYNAMICALLY BY THE OUTPUT SPOOLER *SCO* AND IS SAVED
*         IN THE STATION SLOT FIELD *SLTCV*.
*
*         THE CONVERSION MODE FOR ALL FILE STAGING OPERATIONS
*         IN CHARACTER BLOCKED FORMAT INITIATED BY THE JOB
*         IS DETERMINED BY THIS STATION SLOT FIELD.
*
*         THE CONVERSION MODE FOR THE CRAY JOB OUTPUT IS ALSO
*         DETERMINED BY THIS STATION SLOT FIELD.
*
*         IF THE STATION SLOT SYMBOL *SLTCV* IS NOT DEFINED OR IF
*         THE JOB ORIGINATED FROM A DIFFERENT MAINFRAME, THE
*         CONVERSION MODE USED IS DISPLAY CODE FOR A COS SYSTEM
*         AND ASCII8/12 FOR A UNICOS SYSTEM.
*
*         THE FOLLOWING SYMBOL *FORCEDIS* ALLOWS A SITE TO
*         FORCE THE CONVERSION MODE OF ALL CRAY JOB OUTPUT
*         FILES TO DISPLAY CODE. THIS IS INTENDED FOR SITES WHICH
*         DO NOT HAVE A 96 CHARACTER PRINT TRAIN. 
          SPACE  4
 FORCEDIS EQU    0           0 = CONVERSION MODE DETERMINED BY SLOT
                             1 = CONVERSION MODE = DISPLAY CODE
*/
*/  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/  *                                                                 *
*/  *   CHANGE THE TEXT FIELD LITERAL DELIMITER CHARACTER             *
*/  *     THE TEXT FIELD LITERAL DELIMITER CHARACTER (DEFAULT $)      *
*/  *     DELIMITS LITERAL STRINGS IN THE TEXT FIELD WHICH CONTAIN    *
*/  *     CHARACTERS WHICH ARE NORMALLY NOT ALLOWED IN NOS CONTROL    *
*/  *     STATEMENTS OR WHICH HAVE SPECIAL MEANING E.G. THE PERIOD.   *
*/  *                                                                 *
*/  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/
*D CRSTEXT.37
 I.TXLC   EQU    1R_$        TEXT FIELD LITERAL DELIMITER CHARACTER
*D CRSTEXT.152,CRSTEXT.162
*/
*/  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/  *                                                                 *
*/  *   DETERMINE WHETHER A USER CAN SEE THE STATUS OF ALL CRAY JOBS  *
*/  *     OR ONLY HIS OWN JOBS.                                       *
*/  *                                                                 *
*/  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/
**        USER STATUS CAPABILITY
*         USUALLY USERS CAN SEE THE STATUS OF ALL CRAY JOBS 
*         USING THE *CSTAT* OR *CJOB* COMMANDS.
*         IF THE PARAMETER I.STATAL IS SET TO ZERO
*         USERS MAY ONLY SEE THE STATUS OF THEIR OWN JOBS.
*         A USER WHO HAS THE SYSTEM ORIGIN PRIVILEGES BIT
*         *CSOJ* SET IN THE VALIDATION FILE CAN ALWAYS
*         SEE THE STATUS OF ALL JOBS.
  
 I.STATAL EQU    1           1 = USERS CAN SEE ALL JOBS
                             0 = USERS CAN SEE ONLY THEIR OWN JOBS
*/
*/  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/  *                                                                 *
*/  *   ESTABLISH THE OPERATING SYSTEM TYPE.                          *
*/  *                                                                 *
*/  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/
*I CRSTEXT.16
 NOS      EQU    1
*/
*/  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/  *                                                                 *
*/  *   ESTABLISH THE SUBSYSTEM INDEX.                                *
*/  *                                                                 *
*/  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/
*D CRSTEXT.51
 KSSI     EQU    C1SI
*/
*/  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/  *                                                                 *
*/  *   CHANGE ORDER OF LIBRARIES TO RESOLVE UNSATIFIED EXTERNALS     *
*/  *                                                                 *
*/  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/
*I BLDJOB.24
REWIND,PASCLST.
*D BLDJOB.29
LDSET,LIB=PASCLIB/LIB,PRESET=ZERO,MAP=SEXB/LISTOUT.
*/        END OF MODSET SITEMOD
