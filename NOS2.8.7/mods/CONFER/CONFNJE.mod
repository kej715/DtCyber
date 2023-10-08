CONFNJE
*IDENT CONFNJE
*/
*/  THIS MODSET ENHANCES CONFER TO CREATE AN A-A CONNECTION
*/  WITH NJF AND USE IT TO EXCHANGE MESSAGES WITH USERS ON
*/  THE NJE NETWORK.
*/
*/      K. E. JORDAN   9/28/2023
*/
*DECK CONFER
*I 31
(*                                                                      *)
(* INTERFACE TO NJF AND NJE 'CHAT' SERVICE ADDED BY KEVIN JORDAN IN     *)
(* SEPTEMBER OF 2023.                                                   *)
*D 111
  CFR$C_CONFER_VERSION = 'VERSION 1.1';
*D 125
  CFR$C_CONFER_VERSION_INTERNAL = '#INTERNAL VERSION #I';
*I 134

(************************************************************************)
(*                                                                      *)
(* THIS IS THE RESERVED USERNAME ASSOCIATED WITH THE NJE CHAT SERVIC    *)
(* PROVIDED BY CONFER.                                                  *)
(*                                                                      *)
(************************************************************************)

  CFR$C_CHAT_USERNAME = 'CHAT    ';

(************************************************************************)
(*                                                                      *)
(* THIS IS THE PSEUDO-USERNAME ASSIGNED TO MEMBERS CONNECTED VIA NJE    *)
(*                                                                      *)
(************************************************************************)

  CFR$C_NETWORK_USERNAME = '-NETWK-';
*I 185

(************************************************************************)
(*                                                                      *)
(* MAXIMUM NUMBER OF USERS CONNECTED ON LOCAL TERMINALS                 *)
(*                                                                      *)
(************************************************************************)

  CFR$C_MAX_LOCAL_USERS = 50;

(************************************************************************)
(*                                                                      *)
(* MAXIMUM NUMBER OF USERS CONNECTED FROM REMOTE SYSTEMS                *)
(*                                                                      *)
(************************************************************************)

  CFR$C_MAX_REMOTE_USERS = 50;
*D 197,198
  CFR$C_MAX_CONNECTIONS = 51; (* CFR$C_MAX_LOCAL_USERS + 1 *)
*D 202,206
(* ABSOLUTE MAXIMUM NUMBER OF USERS THAT MAY CONNECT TO CONFER AT ONCE  *)
*D 209
  CFR$C_MAX_CONFER_MEMBERS = 100;
                          (* CFR$C_MAX_LOCAL_USERS + CFR$C_MAX_REMOTE_USERS *)
*I 380

(************************************************************************)
(*                                                                      *)
(* MAXIMUM SIZE OF MESSAGE SENT TO NJF                                  *)
(*                                                                      *)
(************************************************************************)

  CFR$C_MAX_NJF_MESSAGE_SIZE = 130;

(************************************************************************)
(*                                                                      *)
(* MAXIMUM NUMBER OF MESSAGES THAT MAY BE QUEUED CONCURRENTLY FOR       *)
(* TRANSMISSION TO NJF                                                  *)
(*                                                                      *)
(************************************************************************)

  CFR$C_MAX_NJF_QUEUE_LENGTH = 200;

(************************************************************************)
(*                                                                      *)
(* MAXIMUM NUMBER OF UNACKNOWLEDGED MESSAGES THAT MAY BE SENT TO NJF    *)
(*                                                                      *)
(************************************************************************)

  CFR$C_MAX_NJF_UNACKD_MSGS = 1;
*I 506
  ASCII_AT            =  100B;
*I 573
  CFR$C_CHARS_8 = '        ';
*I 617
  CFR$T_CHARS_8 = PACKED ARRAY [1..8] OF CHAR;
*I 619
  CFR$T_CHARS_130 = PACKED ARRAY [1..130] OF CHAR;
*I 753
             PRIVATES,
*D 855
  CFR$T_MESSAGE_TYPE = (
    CFR$C_BROADCAST,
    CFR$C_PRIVATE,
    CFR$C_PUBLIC,
    CFR$C_WHISPER);
*I 892
         PRIVATES: INTEGER;
*I 940

  (* MEMBER'S NJE USER NAME AND NODE NAME. THESE ARE BLANK FOR MEMBERS  *)
  (* CONNECTED FROM LOCAL TERMINALS.                                    *)

    NJE_USER: CFR$T_CHARS_8;
    NJE_NODE: CFR$T_CHARS_8;
*I 1074
                   PRIVATES: INTEGER;
*I 1298
    CFR$C_NJF_CONNECTION_TASK,       (* MANAGE NJF A-A CONNECTION *)
*I 1308
      CFR$C_NJF_CONNECTION_TASK: (JUNK0: INTEGER);
*I 1404

(************************************************************************)
(*                                                                      *)
(* NJF A-A CONNECTION DEFINITIONS                                       *)
(*                                                                      *)
(************************************************************************)

TYPE
  CFR$T_NJF_CXN_STATE = (
    CFR$C_DISCONNECTED,
    CFR$C_CONNECTING,
    CFR$C_CONNECTED);

  CFR$T_NJF_MESSAGE = PACKED RECORD
    COMMAND: CHAR;
    ORIGIN_USER: CFR$T_CHARS_8;
    JUNK1: CHAR;
    DESTINATION_USER: CFR$T_CHARS_8;
    JUNK2: CFR$T_CHARS_2;
    NODE: CFR$T_CHARS_8;
    TEXT_LENGTH: 0..4095;
    TEXT: CFR$T_CHARS_130;
  END;

  CFR$T_NJF_REGISTRATION = PACKED RECORD
    COMMAND: CHAR;
    REGISTERED_USER: CFR$T_CHARS_8;
    JUNK1: CHAR;
  END;

  CFR$T_NJF_RESPONSE = PACKED RECORD
    COMMAND: CHAR;
    UNUSED: PACKED ARRAY [0..7] OF 0..63;
    STATUS: 0..63;
  END;

  CFR$T_CICT_SUP_MESSAGE = PACKED RECORD
    PFC: 0..255;        (*  8 BITS *)
    SFC: 0..255;        (*  8 BITS *)
    UNUSED1: 0..255;    (*  8 BITS *)
    ACN: 0..4095;       (* 12 BITS *)
    UNUSED2: 0..65535;  (* 16 BITS *)
    NXP: 0..1;          (*  1 BIT  *)
    SCT: 0..1;          (*  1 BIT  *)
    ACT: 0..63;         (*  6 BITS *)
  END;

  CFR$T_NAM_MESSAGE_TYPES = (
    CFR$C_WIDE_ASCII_VARIANT,
    CFR$C_NJF_MESSAGE_VARIANT,
    CFR$C_NJF_REGISTRATION_VARIANT,
    CFR$C_NJF_RESPONSE_VARIANT,
    CFR$C_CICT_SUP_MESSAGE_VARIANT);

  CFR$T_NAM_VARIANT_MESSAGE = RECORD
    CASE CFR$T_NAM_MESSAGE_TYPES OF
      CFR$C_WIDE_ASCII_VARIANT      : (WIDE_ASCII     : CFR$T_NETWORK_BUFFER);
      CFR$C_NJF_MESSAGE_VARIANT     : (NJF_MESSAGE    : CFR$T_NJF_MESSAGE);
      CFR$C_NJF_REGISTRATION_VARIANT: (NJF_REGISTRATION:CFR$T_NJF_REGISTRATION);
      CFR$C_NJF_RESPONSE_VARIANT    : (NJF_RESPONSE   : CFR$T_NJF_RESPONSE);
      CFR$C_CICT_SUP_MESSAGE_VARIANT: (CICT_SUP_MSG   : CFR$T_CICT_SUP_MESSAGE);
  END;

  CFR$P_NAM_VARIANT_MESSAGE = ^CFR$T_NAM_VARIANT_MESSAGE;

  CFR$T_NJF_QUEUE_ENTRY_TYPES = (
    CFR$C_NJF_QUEUE_M_VARIANT,
    CFR$C_NJF_QUEUE_R_VARIANT);

  CFR$T_NJF_VARIANT_MESSAGE = RECORD
    CASE CFR$T_NJF_QUEUE_ENTRY_TYPES OF
      CFR$C_NJF_QUEUE_M_VARIANT: (MESSAGE     : CFR$T_NJF_MESSAGE);
      CFR$C_NJF_QUEUE_R_VARIANT: (REGISTRATION: CFR$T_NJF_REGISTRATION);
  END;

  CFR$P_NJF_QUEUE_ENTRY = ^CFR$T_NJF_QUEUE_ENTRY;
  CFR$T_NJF_QUEUE_ENTRY = RECORD
    NEXT_ENTRY: CFR$P_NJF_QUEUE_ENTRY;
    PAYLOAD: CFR$T_NJF_VARIANT_MESSAGE;
  END;
*D 1494,1495
(* THIS ARRAY IS INDEXED BY A MEMBER'S MEMBER NUMBER TO RETRIEVE A      *)
(* POINTER TO THE MEMBER'S CONFERENCE.                                  *)
*D 1499,1500
(* THIS ARRAY IS INDEXED BY A MEMBER'S MEMBER NUMBER TO RETRIEVE A      *)
(* POINTER TO THE MEMBER'S DESCRIPTOR.                                  *)
*I 1502

(* THIS DUMMY MEMBER IS USED FOR ISSUING MESSAGES FOR MEMBERS           *)
(* THAT ARE NOT FULLY CONNECTED.                                        *)

  DUMMY_MEMBER: CFR$P_MEMBER_DESCRIPTOR;
*D 1537
  NETWORK_MESSAGE: CFR$T_NAM_VARIANT_MESSAGE;
*I 1660

(* PSEUDO-USERNAME ASSIGNED TO MEMBERS CONNECTED VIA NJE                *)

  CFR$V_NETWORK_USERNAME: CFR$T_CHARS_7;

(* NJF A-A CONNECTION MANAGEMENT                                        *)

  CFR$V_NJF_ACN: 0..CFR$C_MAX_CONNECTIONS;
  CFR$V_NAM_CICT_REQUEST: CFR$T_CICT_SUP_MESSAGE;
  CFR$V_NJF_CXN_STATE: CFR$T_NJF_CXN_STATE;
  CFR$V_NJF_CXN_TASK_PTR: CFR$P_TASK_DESCRIPTOR;
  CFR$V_NJF_QUEUE_HEAD: CFR$P_NJF_QUEUE_ENTRY;
  CFR$V_NJF_QUEUE_LENGTH: 0..CFR$C_MAX_NJF_QUEUE_LENGTH;
  CFR$V_NJF_QUEUE_TAIL: CFR$P_NJF_QUEUE_ENTRY;
  CFR$V_NJF_UNACKD_MSGS: 0..CFR$C_MAX_NJF_UNACKD_MSGS;
*D 1759
     6 OF FALSE,    (* COLON - QUESTION MARK *)
     1 OF TRUE,     (* COMMERCIAL-AT *)
*I 1807

  CFR$V_NETWORK_USERNAME = CFR$C_NETWORK_USERNAME;

  CFR$V_NJF_ACN = 0;

  CFR$V_NJF_CXN_STATE = CFR$C_DISCONNECTED;

  CFR$V_NJF_QUEUE_HEAD = NIL;

  CFR$V_NJF_QUEUE_LENGTH = 0;

  CFR$V_NJF_QUEUE_TAIL = NIL;

  CFR$V_NJF_UNACKD_MSGS = 0;
*D 1824
  (VAR MESSAGE: CFR$T_NAM_VARIANT_MESSAGE);  FORTRAN;
*D 1828
  (VAR MESSAGE: CFR$T_NAM_VARIANT_MESSAGE);  FORTRAN;
*I 1832

PROCEDURE QTLINK;  FORTRAN;

PROCEDURE QTSUP
  (VAR SUP_MSG_CODE: INTEGER;
   VAR      MESSAGE: CFR$T_NAM_VARIANT_MESSAGE); FORTRAN;
*I 2620
(*$L'PARSE_NJE_ADDRESS' *)

(************************************************************************)
(*                                                                      *)
(* PARSE AN NJE ADDRESS                                                 *)
(*                                                                      *)
(* PARSES AN NJE ADDRESS, SEPARATING IT INTO ITS USERNAME AND NODE      *)
(* COMPONENTS.                                                          *)
(*                                                                      *)
(************************************************************************)

PROCEDURE PARSE_NJE_ADDRESS
  (VAR    NJE_ADDRESS: CFR$T_NAME;
   VAR       NJE_USER: CFR$T_CHARS_8;
   VAR       NJE_NODE: CFR$T_CHARS_8;
   VAR IS_NJE_ADDRESS: BOOLEAN);

VAR
  AC: CFR$T_ASCII_CHAR;
  DC: CHAR;
  DI: INTEGER;
  DONE: BOOLEAN;
  SI: CFR$T_NAME_INDEX;

BEGIN

  IS_NJE_ADDRESS := TRUE;
  NJE_USER := CFR$C_CHARS_8;
  NJE_NODE := CFR$C_CHARS_8;

  SI := 1;
  DI := 1;
  DONE := FALSE;

  WHILE (SI <= NJE_ADDRESS.LENGTH) AND NOT DONE DO
    BEGIN
      AC := NJE_ADDRESS.DATA [SI];
      SI := SI + 1;
      IF AC = ASCII_AT THEN
        BEGIN
          DONE := TRUE;
        END
      ELSE IF DI <= 8 THEN
        BEGIN
          NJE_USER [DI] := CHR (CFR$V_ASCII_TO_DISPLAY [AC]);
          DI := DI + 1;
        END
      ELSE
        BEGIN
          IS_NJE_ADDRESS := FALSE;
          DONE := TRUE;
        END;
    END;

  IF IS_NJE_ADDRESS THEN
    BEGIN
      DI := 1;
      DONE := FALSE;
      WHILE (SI <= NJE_ADDRESS.LENGTH) AND NOT DONE DO
        BEGIN
          AC := NJE_ADDRESS.DATA [SI];
          SI := SI + 1;
          IF DI <= 8 THEN
            BEGIN
              NJE_NODE [DI] := CHR (CFR$V_ASCII_TO_DISPLAY [AC]);
              DI := DI + 1;
            END
          ELSE
            BEGIN
              IS_NJE_ADDRESS := FALSE;
              DONE := TRUE;
            END;
        END;
    END;
END;
*I 2859
(*$LT'NJF MESSAGE QUEUE MANAGEMENT' *)

(************************************************************************)
(*                                                                      *)
(* NJF MESSAGE QUEUE MANAGEMENT ROUTINES                                *)
(*                                                                      *)
(* - ADD_NJF_QUEUE_ENTRY                                                *)
(* - DISCARD_ALL_NJF_QUEUE_ENTRIES                                      *)
(*                                                                      *)
(************************************************************************)

PROCEDURE ADD_NJF_QUEUE_ENTRY
  (QUEUE_ENTRY: CFR$P_NJF_QUEUE_ENTRY);

BEGIN

  QUEUE_ENTRY^.NEXT_ENTRY := NIL;

  IF CFR$V_NJF_QUEUE_HEAD <> NIL THEN
    BEGIN
    CFR$V_NJF_QUEUE_TAIL^.NEXT_ENTRY := QUEUE_ENTRY;
    END
  ELSE
    BEGIN
    CFR$V_NJF_QUEUE_HEAD := QUEUE_ENTRY;
    END;

  CFR$V_NJF_QUEUE_TAIL := QUEUE_ENTRY;
  CFR$V_NJF_QUEUE_LENGTH := CFR$V_NJF_QUEUE_LENGTH + 1;

END; (* ADD_NJF_QUEUE_ENTRY *)

PROCEDURE DISCARD_ALL_NJF_QUEUE_ENTRIES;

VAR
  QUEUE_ENTRY: CFR$P_NJF_QUEUE_ENTRY;

BEGIN

  WHILE CFR$V_NJF_QUEUE_HEAD <> NIL DO
    BEGIN
      QUEUE_ENTRY := CFR$V_NJF_QUEUE_HEAD;
      CFR$V_NJF_QUEUE_HEAD := QUEUE_ENTRY^.NEXT_ENTRY;
      DISPOSE (QUEUE_ENTRY);
    END;

  CFR$V_NJF_QUEUE_TAIL := NIL;
  CFR$V_NJF_QUEUE_LENGTH := 0;

END; (* DISCARD_ALL_NJF_QUEUE_ENTRIES *)
(*$LT'MEMBER DESCRIPTOR LOOKUP FUNCTIONS' *)

(************************************************************************)
(*                                                                      *)
(* MEMBER DESCRIPTOR LOOKUP FUNCTIONS                                   *)
(*                                                                      *)
(* - FIND_LOCAL_MEMBER                                                  *)
(* - FIND_MEMBER_BY_USERNAME                                            *)
(* - FIND_REMOTE_MEMBER                                                 *)
(*                                                                      *)
(************************************************************************)

FUNCTION FIND_LOCAL_MEMBER
  (ACN: CFR$T_ACN): CFR$P_MEMBER_DESCRIPTOR;

VAR
  MEMBER_NUMBER: CFR$T_MEMBER_NUMBER;

BEGIN

  MEMBER_NUMBER := CFR$V_ACN_TO_MEMBER_NUMBER [ACN.LOCAL];

  IF MEMBER_NUMBER <> CFR$C_NIL_MEMBER_NUMBER THEN
    BEGIN
      FIND_LOCAL_MEMBER := MEMBER_POINTERS [MEMBER_NUMBER];
    END
  ELSE
    BEGIN
      FIND_LOCAL_MEMBER := NIL;
    END;

END; (* FIND_LOCAL_MEMBER *)

FUNCTION FIND_MEMBER_BY_USERNAME
  (USERNAME: CFR$T_CHARS_8): CFR$P_MEMBER_DESCRIPTOR;

VAR
  DONE: BOOLEAN;
  I: INTEGER;
  MATCH: BOOLEAN;
  MI: 0..CFR$C_MAX_CONFER_MEMBERS;
  MP: CFR$P_MEMBER_DESCRIPTOR;

BEGIN

  DONE := FALSE;
  MI := 1;
  MP := NIL;

  WHILE (MI <= CFR$C_MAX_LOCAL_USERS) AND NOT DONE DO
    BEGIN
      MP := MEMBER_POINTERS [MI];
      IF MP <> NIL THEN
        BEGIN
          MATCH := TRUE;
          FOR I := 1 TO 7 DO
            BEGIN
              MATCH := MATCH AND
                (USERNAME [I] = CHR (CFR$V_ASCII_TO_DISPLAY [MP^.USERNAME[I]]));
            END;
          IF MATCH THEN
            DONE := TRUE
          ELSE
            MP := NIL;
        END;
      MI := MI + 1;
    END;

  FIND_MEMBER_BY_USERNAME := MP;

END; (* FIND_MEMBER_BY_USERNAME *)

FUNCTION FIND_REMOTE_MEMBER
  (NJE_USER: CFR$T_CHARS_8;
   NJE_NODE: CFR$T_CHARS_8): CFR$P_MEMBER_DESCRIPTOR;

VAR
  DONE: BOOLEAN;
  I: 0..CFR$C_MAX_CONFER_MEMBERS;
  MP: CFR$P_MEMBER_DESCRIPTOR;

BEGIN

  DONE := FALSE;
  I := CFR$C_MAX_LOCAL_USERS + 2;
  MP := NIL;

  WHILE (I <= CFR$C_MAX_CONFER_MEMBERS) AND NOT DONE DO
    BEGIN
      MP := MEMBER_POINTERS [I];
      IF MP <> NIL THEN
        BEGIN
          IF (MP^.NJE_USER = NJE_USER) AND (MP^.NJE_NODE = NJE_NODE) THEN
            BEGIN
              DONE := TRUE;
            END
          ELSE
            BEGIN
              MP := NIL;
            END;
        END;
      I := I + 1;
    END;

  FIND_REMOTE_MEMBER := MP;

END; (* FIND_REMOTE_MEMBER *)
*I 3102
  WRITELN (TOTAL.PRIVATES:10,      ' TOTAL PRIVATES');
*D 3109
           MAX (1, TOTAL.WHISPERS + TOTAL.PUBLICS + TOTAL.PRIVATES),
*D 3115
           MAX (1, TOTAL.WHISPERS + TOTAL.PUBLICS + TOTAL.PRIVATES),
*D 3422
    INTEGER_TO_RESULT (RESULT, -(PD.YEAR + 1970), 1);
*D 3605
  (   MP: CFR$P_MEMBER_DESCRIPTOR;
*I 3608
  AC: CFR$T_ASCII_CHAR;
  DC: CHAR;
  DC_LENGTH: INTEGER;
*D 3610
*I 3611
  QUEUE_ENTRY: CFR$P_NJF_QUEUE_ENTRY;
*D 3620
       WRITE (' SENDING CANNED MESSAGE TO ACN ', MP^.ACN.LOCAL:1, ': ');
*D 3628,3629
   IF (MP^.ACN.LOCAL <> CFR$V_NJF_ACN) AND
      (CFR$V_ACN_TO_MEMBER_NUMBER [MP^.ACN.LOCAL] =
       CFR$C_NIL_MEMBER_NUMBER) THEN  (* IF MEMBER IS NOT FULLY LOGGED IN YET *)
*D 3636
             WRITE (NIT.LOCAL [MP^.ACN.LOCAL].USERNAME [I]);
*D 3640,3645
       IF MP^.ACN.LOCAL <> CFR$V_NJF_ACN THEN
         BEGIN
           FOR I := 1 TO LENGTH DO
             NETWORK_MESSAGE.WIDE_ASCII [I] := ERRORS [ERROR].MESSAGE [I];
           NIT.GLOBAL.CONNECTION_NUMBER := MP^.ACN.LOCAL;
           NIT.GLOBAL.CURRENT_TRANS_SIZE := LENGTH;
           NIT.GLOBAL.CHARACTER_SET := 11;
           QTPUT (NETWORK_MESSAGE);
         END
       ELSE
         BEGIN
           NEW (QUEUE_ENTRY);
           QUEUE_ENTRY^.PAYLOAD.MESSAGE.COMMAND := 'M';
           QUEUE_ENTRY^.PAYLOAD.MESSAGE.ORIGIN_USER := CFR$C_CHAT_USERNAME;
           QUEUE_ENTRY^.PAYLOAD.MESSAGE.DESTINATION_USER := MP^.NJE_USER;
           QUEUE_ENTRY^.PAYLOAD.MESSAGE.NODE := MP^.NJE_NODE;
           DC_LENGTH := 0;
           FOR I := 1 TO LENGTH DO
             BEGIN
               AC := ERRORS [ERROR].MESSAGE [I];
               DC := CHR (CFR$V_ASCII_TO_DISPLAY [AC]);
               IF AC = ASCII_COLON THEN
                 BEGIN
                   DC_LENGTH := DC_LENGTH + 1;
                   QUEUE_ENTRY^.PAYLOAD.MESSAGE.TEXT [DC_LENGTH] := DC;
                   IF I >= LENGTH THEN (* DON'T LET LINE END IN COLON *)
                     BEGIN
                       DC_LENGTH := DC_LENGTH + 1;
                       QUEUE_ENTRY^.PAYLOAD.MESSAGE.TEXT [DC_LENGTH] := ' ';
                     END;
                 END
               ELSE IF DC <> ':' THEN
                 BEGIN
                   DC_LENGTH := DC_LENGTH + 1;
                   QUEUE_ENTRY^.PAYLOAD.MESSAGE.TEXT [DC_LENGTH] := DC;
                 END;
             END;
           QUEUE_ENTRY^.PAYLOAD.MESSAGE.TEXT_LENGTH := DC_LENGTH;
           ADD_NJF_QUEUE_ENTRY(QUEUE_ENTRY);
         END;
*D 3649,3652
       IF MP^.PENDING_ERROR <> CFR$E_NO_ERROR THEN
*D 3675,3680
               IF MP^.ACN.LOCAL <> CFR$V_NJF_ACN THEN
                 BEGIN
                   FOR I := 1 TO LENGTH DO
                     NETWORK_MESSAGE.WIDE_ASCII [I] :=
                       ERRORS [ERROR].MESSAGE [I];
                   NIT.GLOBAL.CONNECTION_NUMBER := MP^.ACN.LOCAL;
                   NIT.GLOBAL.CURRENT_TRANS_SIZE := LENGTH;
                   NIT.GLOBAL.CHARACTER_SET := 11;
                   QTPUT (NETWORK_MESSAGE);
                 END
               ELSE
                 BEGIN
                   NEW (QUEUE_ENTRY);
                   QUEUE_ENTRY^.PAYLOAD.MESSAGE.COMMAND := 'M';
                   QUEUE_ENTRY^.PAYLOAD.MESSAGE.ORIGIN_USER :=
                     CFR$C_CHAT_USERNAME;
                   QUEUE_ENTRY^.PAYLOAD.MESSAGE.DESTINATION_USER :=
                     MP^.NJE_USER;
                   QUEUE_ENTRY^.PAYLOAD.MESSAGE.NODE := MP^.NJE_NODE;
                   QUEUE_ENTRY^.PAYLOAD.MESSAGE.TEXT_LENGTH := LENGTH;
                   DC_LENGTH := 0;
                   FOR I := 1 TO LENGTH DO
                     BEGIN
                     AC := ERRORS [ERROR].MESSAGE [I];
                     DC := CHR (CFR$V_ASCII_TO_DISPLAY [AC]);
                     IF AC = ASCII_COLON THEN
                       BEGIN
                         DC_LENGTH := DC_LENGTH + 1;
                         QUEUE_ENTRY^.PAYLOAD.MESSAGE.TEXT [DC_LENGTH] := DC;
                         IF I >= LENGTH THEN (* DON'T LET LINE END IN COLON *)
                           BEGIN
                             DC_LENGTH := DC_LENGTH + 1;
                             QUEUE_ENTRY^.PAYLOAD.MESSAGE.TEXT [DC_LENGTH] :=
                               ' ';
                           END;
                       END
                     ELSE IF DC <> ':' THEN
                       BEGIN
                         DC_LENGTH := DC_LENGTH + 1;
                         QUEUE_ENTRY^.PAYLOAD.MESSAGE.TEXT [I] := DC;
                       END;
                     END;
                   ADD_NJF_QUEUE_ENTRY(QUEUE_ENTRY);
                 END;
*I 4186

    CFR$C_PRIVATE: BEGIN
      DISPLAY_TO_ASCII ('<PRIVATE> ', NEXT, BLOCK^.DATA);
    END;
*D 4530
(*$L'FILL_NETWORK_MESSAGE' *)
*D 4534
(* FILL THE NETWORK MESSAGE WITH AS MANY MESSAGE BLOCKS AS POSSIBLE.    *)
*D 4539
PROCEDURE FILL_NETWORK_MESSAGE
*D 4540,4543
  (                 MP: CFR$P_MEMBER_DESCRIPTOR;
   VAR NETWORK_MESSAGE: CFR$T_NAM_VARIANT_MESSAGE;
   VAR          LENGTH: INTEGER);
*D 4563,4564
      IF NEXT + P^.NEXT_CHAR - 2 > CFR$C_MAX_NETWORK_BUFFER_SIZE THEN
        DONE := TRUE (* IF BLOCK WILL NOT FIT *)
*D 4572
              NETWORK_MESSAGE.WIDE_ASCII [NEXT + I - 1] := ASCII_BLANK
*D 4574
              NETWORK_MESSAGE.WIDE_ASCII [NEXT + I - 1] := P^.DATA [I];
*D 4589
END;  (* FILL_NETWORK_MESSAGE *)
*D 4640
        ISSUE_MESSAGE (MP, CFR$E_NOT_ENOUGH_MEMORY)
*D 4683
  (       MP: CFR$P_MEMBER_DESCRIPTOR;
*D 4685,4686
       CHARS: INTEGER);
*I 4688
  AC: CFR$T_WIDE_ASCII_CHAR;
  DC: CHAR;
  DC_LENGTH: INTEGER;
*I 4690
  QUEUE_ENTRY: CFR$P_NJF_QUEUE_ENTRY;
*D 4694
  IF CONNECTION_BUSY (MP^.ACN) THEN
*D 4698
      IF MP^.ACN.REMOTE <> 0 THEN
*D 4700,4721
      ELSE IF MP^.ACN.LOCAL <> CFR$V_NJF_ACN THEN
        BEGIN
          MAX := MIN (CHARS, CFR$C_MAX_NETWORK_BUFFER_SIZE);
          FOR I := 1 TO MAX DO
            NETWORK_MESSAGE.WIDE_ASCII [I] := TXT [I];
          NIT.GLOBAL.CONNECTION_NUMBER := MP^.ACN.LOCAL;
          NIT.GLOBAL.CURRENT_TRANS_SIZE := MAX;
          NIT.GLOBAL.CHARACTER_SET := 11;
          QTPUT (NETWORK_MESSAGE);
        END
      ELSE
        BEGIN
          I := 1;
          DC_LENGTH := 0;
          NEW (QUEUE_ENTRY);

          WHILE I <= CHARS DO
            BEGIN
              AC := TXT [I];
              I := I + 1;
              DC := CHR (CFR$V_ASCII_TO_DISPLAY [AC]);
              IF AC = ASCII_CR THEN
                BEGIN
                  IF DC_LENGTH < 1 THEN
                    BEGIN
                      DC_LENGTH := DC_LENGTH + 1;
                      QUEUE_ENTRY^.PAYLOAD.MESSAGE.TEXT [DC_LENGTH] := ' ';
                    END;
                  QUEUE_ENTRY^.PAYLOAD.MESSAGE.COMMAND := 'M';
                  QUEUE_ENTRY^.PAYLOAD.MESSAGE.ORIGIN_USER :=
                    CFR$C_CHAT_USERNAME;
                  QUEUE_ENTRY^.PAYLOAD.MESSAGE.DESTINATION_USER := MP^.NJE_USER;
                  QUEUE_ENTRY^.PAYLOAD.MESSAGE.NODE := MP^.NJE_NODE;
                  QUEUE_ENTRY^.PAYLOAD.MESSAGE.TEXT_LENGTH := DC_LENGTH;
                  ADD_NJF_QUEUE_ENTRY(QUEUE_ENTRY);
                  DC_LENGTH := 0;
                  NEW (QUEUE_ENTRY);
                END
              ELSE IF DC_LENGTH < CFR$C_MAX_NJF_MESSAGE_SIZE THEN
                BEGIN
                  IF AC = ASCII_COLON THEN
                    BEGIN
                      DC_LENGTH := DC_LENGTH + 1;
                      QUEUE_ENTRY^.PAYLOAD.MESSAGE.TEXT [DC_LENGTH] := DC;
                      IF (I > CHARS) AND
                         (DC_LENGTH < CFR$C_MAX_NJF_MESSAGE_SIZE) THEN
                        BEGIN
                          DC_LENGTH := DC_LENGTH + 1;
                          QUEUE_ENTRY^.PAYLOAD.MESSAGE.TEXT [DC_LENGTH] := ' ';
                        END;
                    END
                  ELSE IF DC <> ':' THEN
                    BEGIN
                      DC_LENGTH := DC_LENGTH + 1;
                      QUEUE_ENTRY^.PAYLOAD.MESSAGE.TEXT [DC_LENGTH] := DC;
                    END;
                END;
            END;
          IF DC_LENGTH > 0 THEN
            BEGIN
              QUEUE_ENTRY^.PAYLOAD.MESSAGE.COMMAND := 'M';
              QUEUE_ENTRY^.PAYLOAD.MESSAGE.ORIGIN_USER := CFR$C_CHAT_USERNAME;
              QUEUE_ENTRY^.PAYLOAD.MESSAGE.DESTINATION_USER := MP^.NJE_USER;
              QUEUE_ENTRY^.PAYLOAD.MESSAGE.NODE := MP^.NJE_NODE;
              QUEUE_ENTRY^.PAYLOAD.MESSAGE.TEXT_LENGTH := DC_LENGTH;
              ADD_NJF_QUEUE_ENTRY(QUEUE_ENTRY);
            END
          ELSE
            BEGIN
              DISPOSE (QUEUE_ENTRY);
            END;
        END;
*D 4847,4848
      COPY_SHORT_TO_WIDE_ASCII (MESSAGE_TEXT, NETWORK_MESSAGE.WIDE_ASCII,
                               LENGTH);
      SEND_TEXT_TO_CONNECTION (MP, NETWORK_MESSAGE.WIDE_ASCII, LENGTH);
*D 4960
      ISSUE_MESSAGE (MP, CFR$E_NOT_ENOUGH_MEMORY);
*I 5135
(*$L'SEND_PRIVATE_MESSAGE' *)

(************************************************************************)
(*                                                                      *)
(* *SEND A PRIVATE MESSAGE TO A SINGLE USER.*                           *)
(*                                                                      *)
(************************************************************************)

PROCEDURE SEND_PRIVATE_MESSAGE
  (FROM: CFR$P_MEMBER_DESCRIPTOR;
   DEST: CFR$P_MEMBER_DESCRIPTOR);

VAR
  AC: CFR$T_ASCII_CHAR;
  BI: CFR$T_MESSAGE_BLOCK_INDEX;
  BLOCKS, LINES, CHARS: INTEGER;  (* FOR STATISTICS *)
  DC: CHAR;
  DC_LENGTH: INTEGER;
  FROM_CP: CFR$P_CONFERENCE_DESCRIPTOR;
  I: INTEGER;
  ORIGIN_USER: CFR$T_CHARS_8;
  P: CFR$P_MESSAGE_BLOCK;
  QUEUE_ENTRY: CFR$P_NJF_QUEUE_ENTRY;
  ROUTING_SLIP: CFR$T_ROUTING_SLIP;

BEGIN

  CALCULATE_MESSAGE_SIZE (FROM^.FIRST_INPUT_BLOCK, BLOCKS, LINES, CHARS);

  IF DEST^.ACN.LOCAL <> CFR$V_NJF_ACN THEN
    BEGIN
      PUT_MESSAGE_CHAR (ASCII_CR, FROM);
      PUT_MESSAGE_CHAR (ASCII_LF, FROM);
      BUILD_PREFIX_TEXT (FROM, CFR$C_PRIVATE);
    END;

  P := FROM^.FIRST_INPUT_BLOCK;

(* UPDATE STATISTICS *)

  TOTAL.MESSAGE_BLOCKS    := TOTAL.MESSAGE_BLOCKS + BLOCKS;
  TOTAL.MESSAGE_LINES     := TOTAL.MESSAGE_LINES + LINES;
  TOTAL.MESSAGE_CHARS     := TOTAL.MESSAGE_CHARS + CHARS;
  TOTAL.PRIVATES          := TOTAL.PRIVATES + 1;

  FROM_CP := CONFERENCE_POINTERS [FROM^.MEMBER_NUMBER];
  FROM^.STATS.PRIVATES    := FROM^.STATS.PRIVATES + 1;
  FROM_CP^.STATS.PRIVATES := FROM_CP^.STATS.PRIVATES + 1;

  IF DEST^.ACN.LOCAL <> CFR$V_NJF_ACN THEN
    BEGIN

    (* SELECT THE SPECIFIED MEMBER TO RECEIVE THIS MESSAGE *)

      FOR I := 1 TO CFR$C_MAX_ROUTING_SLIP_SIZE DO
        ROUTING_SLIP [I] := FALSE;

      ROUTING_SLIP [DEST^.MEMBER_NUMBER] := TRUE;

    (* POP THE MESSAGE IN THE MESSAGE-QUEUE *)

      ENQUEUE_MESSAGE (P, FROM^.MEMBER_NUMBER, ROUTING_SLIP);
    END
  ELSE
    BEGIN

      ORIGIN_USER := CFR$C_CHARS_8;
      FOR I := 1 TO 7 DO
        ORIGIN_USER [I] := CHR (CFR$V_ASCII_TO_DISPLAY [FROM^.USERNAME [I]]);

      BI := 1;
      DC_LENGTH := 0;
      NEW (QUEUE_ENTRY);

      WHILE P <> NIL DO
        BEGIN
          WHILE BI < P^.NEXT_CHAR DO
            BEGIN
              AC := P^.DATA [BI];
              BI := BI + 1;
              DC := CHR (CFR$V_ASCII_TO_DISPLAY [AC]);
              IF AC = ASCII_CR THEN
                BEGIN
                  IF DC_LENGTH < 1 THEN
                    BEGIN
                      DC_LENGTH := DC_LENGTH + 1;
                      QUEUE_ENTRY^.PAYLOAD.MESSAGE.TEXT [DC_LENGTH] := ' ';
                    END;
                  QUEUE_ENTRY^.PAYLOAD.MESSAGE.COMMAND := 'M';
                  QUEUE_ENTRY^.PAYLOAD.MESSAGE.ORIGIN_USER := ORIGIN_USER;
                  QUEUE_ENTRY^.PAYLOAD.MESSAGE.DESTINATION_USER :=
                    DEST^.NJE_USER;
                  QUEUE_ENTRY^.PAYLOAD.MESSAGE.NODE := DEST^.NJE_NODE;
                  QUEUE_ENTRY^.PAYLOAD.MESSAGE.TEXT_LENGTH := DC_LENGTH;
                  ADD_NJF_QUEUE_ENTRY(QUEUE_ENTRY);
                  DC_LENGTH := 0;
                  NEW (QUEUE_ENTRY);
                END
              ELSE IF DC_LENGTH < CFR$C_MAX_NJF_MESSAGE_SIZE THEN
                BEGIN
                  IF AC = ASCII_COLON THEN
                    BEGIN
                      DC_LENGTH := DC_LENGTH + 1;
                      QUEUE_ENTRY^.PAYLOAD.MESSAGE.TEXT [DC_LENGTH] := DC;
                      IF (BI >= P^.NEXT_CHAR) AND
                         (DC_LENGTH < CFR$C_MAX_NJF_MESSAGE_SIZE) THEN
                        BEGIN
                          DC_LENGTH := DC_LENGTH + 1;
                          QUEUE_ENTRY^.PAYLOAD.MESSAGE.TEXT [DC_LENGTH] := ' ';
                        END;
                    END
                  ELSE IF DC <> ':' THEN
                    BEGIN
                      DC_LENGTH := DC_LENGTH + 1;
                      QUEUE_ENTRY^.PAYLOAD.MESSAGE.TEXT [DC_LENGTH] := DC;
                    END;
                END;
            END;
          FROM^.FIRST_INPUT_BLOCK := P^.NEXT_BLOCK;
          DISPOSE (P);
          P := FROM^.FIRST_INPUT_BLOCK;
          BI := 1;
        END;
      IF DC_LENGTH > 0 THEN
        BEGIN
          QUEUE_ENTRY^.PAYLOAD.MESSAGE.COMMAND := 'M';
          QUEUE_ENTRY^.PAYLOAD.MESSAGE.ORIGIN_USER := ORIGIN_USER;
          QUEUE_ENTRY^.PAYLOAD.MESSAGE.DESTINATION_USER := DEST^.NJE_USER;
          QUEUE_ENTRY^.PAYLOAD.MESSAGE.NODE := DEST^.NJE_NODE;
          QUEUE_ENTRY^.PAYLOAD.MESSAGE.TEXT_LENGTH := DC_LENGTH;
          ADD_NJF_QUEUE_ENTRY(QUEUE_ENTRY);
        END
      ELSE
        BEGIN
          DISPOSE (QUEUE_ENTRY);
        END;
      FROM^.CURRENT_INPUT_BLOCK := NIL;
      FROM^.LINE_COUNT := 0;
    END;

END;  (* SEND_PRIVATE_MESSAGE *)
*D 5162
  IF ACN.LOCAL = CFR$V_NJF_ACN THEN
    BUSY := FALSE
  ELSE IF ACN.REMOTE = 0 THEN   (* IF USER IS ON MASTER NODE *)
*D 5191
  ELSE IF ACN.LOCAL <> CFR$V_NJF_ACN THEN
*D 5371
  (       ACN: CFR$T_ACN;
   VAR NUMBER: INTEGER);
*D 5379,5397
  IF ACN.LOCAL <> CFR$V_NJF_ACN THEN
    BEGIN
      I := 1;
      DONE := FALSE;
      WHILE (I <= CFR$C_MAX_LOCAL_USERS) AND NOT DONE DO
        BEGIN
          IF NOT CFR$V_MEMBER_NUMBERS [I] THEN
            BEGIN
              CFR$V_MEMBER_NUMBERS [I] := TRUE;
              NUMBER := I;
              DONE := TRUE;
            END
          ELSE
            I := I + 1;
        END;

      IF NOT DONE THEN
        BEGIN
          MESSAGE ('...ERROR...RAN OUT OF MEMBER NUMBERS...USING 1');
          NUMBER := 1;
        END;
    END
  ELSE
    BEGIN
      I := CFR$C_MAX_LOCAL_USERS + 2;
      DONE := FALSE;
      WHILE (I <= CFR$C_MAX_CONFER_MEMBERS) AND NOT DONE DO
        BEGIN
          IF NOT CFR$V_MEMBER_NUMBERS [I] THEN
            BEGIN
              CFR$V_MEMBER_NUMBERS [I] := TRUE;
              NUMBER := I;
              DONE := TRUE;
            END
          ELSE
            I := I + 1;
        END;

      IF NOT DONE THEN
        BEGIN
          MESSAGE ('...ERROR...RAN OUT OF MEMBER NUMBERS...USING 1');
          NUMBER := CFR$C_MAX_LOCAL_USERS + 1;
        END;
    END;
*D 5650
  ELSE IF CONFIG.UMASS AND (ACN.LOCAL <> CFR$V_NJF_ACN) THEN
*I 5742
  CP^.STATS.PRIVATES      := 0;
*I 6469
              NJE_USER: CFR$T_CHARS_8;
              NJE_NODE: CFR$T_CHARS_8;
*I 6474
  J: INTEGER;
  DONE: BOOLEAN;
*I 6491
      MP^.NJE_USER := NJE_USER;
      MP^.NJE_NODE := NJE_NODE;
*D 6488,6489
      IF ACN.LOCAL = CFR$V_NJF_ACN THEN
        BEGIN
          FOR I := 1 TO 7 DO
            MP^.USERNAME [I] :=
              CFR$V_DISPLAY_TO_ASCII [ORD (CFR$V_NETWORK_USERNAME [I])];
          I := 1;
          J := 1;
          DONE := FALSE;
          WHILE (J <= 8) AND NOT DONE DO
            BEGIN
            IF NJE_USER [J] = ' ' THEN
              DONE := TRUE
            ELSE
              BEGIN
                MP^.NAME.DATA [I] :=
                  CFR$V_DISPLAY_TO_ASCII [ORD (NJE_USER [J])];
                I := I + 1;
                J := J + 1;
              END;
            END;
          MP^.NAME.DATA [I] := CFR$V_DISPLAY_TO_ASCII [ORD ('@')];
          I := I + 1;
          J := 1;
          DONE := FALSE;
          WHILE (J <= 8) AND NOT DONE DO
            BEGIN
            IF NJE_NODE [J] = ' ' THEN
              DONE := TRUE
            ELSE
              BEGIN
                MP^.NAME.DATA [I] :=
                  CFR$V_DISPLAY_TO_ASCII [ORD (NJE_NODE [J])];
                I := I + 1;
                J := J + 1;
              END;
            END;
          MP^.NAME.LENGTH := I - 1;
        END
      ELSE
        BEGIN
          FOR I := 1 TO 7 DO
            MP^.USERNAME [I] :=
              CFR$V_DISPLAY_TO_ASCII [ORD (NIT.LOCAL [ACN.LOCAL].USERNAME [I])];
          GENERATE_MEMBER_NAME (ACN, DATABASE_RECORD, MEMBER_NAME);
          MP^.NAME := MEMBER_NAME;
        END;

*D 6492,6496
*D 6504
      GENERATE_MEMBER_NUMBER (ACN, I);
*I 6508
      MP^.STATS.PRIVATES := 0;
*D 6563
      IF ACN.LOCAL <> CFR$V_NJF_ACN THEN
        CFR$V_ACN_TO_MEMBER_NUMBER [ACN.LOCAL] := MP^.MEMBER_NUMBER;
*D 6680
  IF MP^.ACN.LOCAL <> CFR$V_NJF_ACN THEN
    CFR$V_ACN_TO_MEMBER_NUMBER [MP^.ACN.LOCAL] := CFR$C_NIL_MEMBER_NUMBER;
*I 6776

  DISPLAY_TO_RESULT (RESULT, '    #PRIVATES: ');
   INTEGER_TO_RESULT (RESULT, TOTAL.PRIVATES, 1);
   DISPLAY_TO_RESULT (RESULT, ' (TOTAL)$');
*D 6838
                                  MAX (1,
                                       TOTAL.WHISPERS +
                                       TOTAL.PUBLICS  +
                                       TOTAL.PRIVATES), 1);
*D 6856
                                  MAX (1,
                                       TOTAL.WHISPERS +
                                       TOTAL.PUBLICS +
                                       TOTAL.PRIVATES), 1);
*I 6928

(* DISPLAY TOTAL PRIVATES SENT *)

  IF (INFO.MP = MP) OR INFO.MP^.FLAG.CONTROLLER THEN
    BEGIN
      DISPLAY_TO_RESULT (RESULT, '    #PRIVATES SENT: ');
      INTEGER_TO_RESULT (RESULT, MP^.STATS.PRIVATES, 1);
      DISPLAY_TO_RESULT (RESULT, ' $');
    END;
*I 7111

(* DISPLAY TOTAL PRIVATES SENT *)

  DISPLAY_TO_RESULT (RESULT, '    #PRIVATES: ');
  INTEGER_TO_RESULT (RESULT, CP^.STATS.PRIVATES, 1);
  DISPLAY_TO_RESULT (RESULT, ' $');
*D 7310
  (        MP: CFR$P_MEMBER_DESCRIPTOR;
   VAR RESULT: CFR$T_COMMAND_RESULT);
*I 7321
  IF MP^.ACN.LOCAL <> CFR$V_NJF_ACN THEN
*I 7327
  IF MP^.ACN.LOCAL <> CFR$V_NJF_ACN THEN
*I 7351
  IF MP^.ACN.LOCAL <> CFR$V_NJF_ACN THEN
*I 7377
  IF MP^.ACN.LOCAL <> CFR$V_NJF_ACN THEN
*I 7395
  IF MP^.ACN.LOCAL <> CFR$V_NJF_ACN THEN
  BEGIN
*I 7403
  END;
*I 7627
  CH: CFR$T_ASCII_CHAR;
  DONE: BOOLEAN;
*D 7640,7641
  IF MP = DUMMY_MEMBER THEN
    BEGIN

    (* SET ORIGIN AS NJE_USER@NJE_NODE *)

      DONE := FALSE;
      I := 1;
      WHILE (I <= 8) AND NOT DONE DO
        BEGIN
          CH := CFR$V_DISPLAY_TO_ASCII [ORD (MP^.NJE_USER [I])];
          I := I + 1;
          IF CH = ASCII_BLANK THEN
            DONE := TRUE
          ELSE
            PUT_RESULT_CHAR (RESULT, CH);
        END;
      PUT_RESULT_CHAR (RESULT, ASCII_AT);
      DONE := FALSE;
      I := 1;
      WHILE (I <= 8) AND NOT DONE DO
        BEGIN
          CH := CFR$V_DISPLAY_TO_ASCII [ORD (MP^.NJE_NODE [I])];
          I := I + 1;
          IF CH = ASCII_BLANK THEN
            DONE := TRUE
          ELSE
            PUT_RESULT_CHAR (RESULT, CH);
        END;
    END
  ELSE
    BEGIN
      FOR I := 1 TO MP^.NAME.LENGTH DO
        PUT_RESULT_CHAR (RESULT, MP^.NAME.DATA [I]);
    END;
*D 7774
    ISSUE_MESSAGE (INFO.MP, CFR$E_NO_SUCH_COMMAND)
*D 7778
        ISSUE_MESSAGE (INFO.MP, CFR$E_NO_MESSAGE)
*D 7811
    ISSUE_MESSAGE (INFO.MP, CFR$E_NO_SUCH_COMMAND)
*D 7822
        ISSUE_MESSAGE (INFO.MP, CFR$E_NO_SUCH_COMMAND)
*D 7841
            ISSUE_MESSAGE (INFO.MP, CFR$E_NO_SUCH_COMMAND)
*D 7845
              ISSUE_MESSAGE (INFO.MP, CFR$M_OK);
*D 7895
        ISSUE_MESSAGE (INFO.MP, CFR$E_INVALID_NAME)
*D 7900
            ISSUE_MESSAGE (INFO.MP, ERROR)
*D 7948
          ISSUE_MESSAGE (INFO.MP, CFR$E_INVALID_NAME);
*D 7959
              ISSUE_MESSAGE (INFO.MP, CFR$E_NO);
*D 7997
    ISSUE_MESSAGE (INFO.MP, CFR$E_PARAMETER_MISSING)
*D 8003
        ISSUE_MESSAGE (INFO.MP, CFR$E_INVALID_NAME)
*D 8016
                  ISSUE_MESSAGE (INFO.MP, ERROR);
*D 8024
                ISSUE_MESSAGE (INFO.MP, ERROR)
*D 8037
                  ISSUE_MESSAGE (INFO.MP, CFR$M_OK);
*D 8067
    ISSUE_MESSAGE (INFO.MP, CFR$E_PARAMETER_MISSING)
*D 8072
        ISSUE_MESSAGE (INFO.MP, CFR$E_INVALID_NAME)
*D 8081
            ISSUE_MESSAGE (INFO.MP, ERROR)
*D 8085
                ISSUE_MESSAGE (MP, CFR$E_MANAGER_DROP)
*D 8087
                ISSUE_MESSAGE (MP, CFR$E_MODERATOR_DROP);
*D 8101
                ISSUE_MESSAGE (INFO.MP, CFR$M_OK);
*D 8128
    ISSUE_MESSAGE (INFO.MP, CFR$E_EMPTY_MESSAGE)
*D 8133
        ISSUE_MESSAGE (INFO.MP, ERROR)
*D 8162
    ISSUE_MESSAGE (INFO.MP, CFR$E_PARAMETER_MISSING)
*D 8167
        ISSUE_MESSAGE (INFO.MP, CFR$E_INVALID_NAME)
*D 8172
            ISSUE_MESSAGE (INFO.MP, ERROR)
*D 8176
              ISSUE_MESSAGE (INFO.MP, CFR$M_OK);
*D 8177
              ISSUE_MESSAGE (MP, CFR$M_YOU_ARE_MODERATOR);
*D 8243
          ISSUE_MESSAGE (INFO.MP, CFR$M_OK);
*D 8246
        ISSUE_MESSAGE (INFO.MP, CFR$E_INVALID_FLAG);
*D 8274
      FORMAT_HELP_OUTPUT (INFO.MP, RESULT);
*D 8284
        ISSUE_MESSAGE (INFO.MP, CFR$E_INVALID_NAME)
*D 8290
            ISSUE_MESSAGE (INFO.MP, ERROR)
*D 8293
              ISSUE_MESSAGE (INFO.MP, CFR$M_OK);
*D 8321
    ISSUE_MESSAGE (INFO.MP, CFR$E_PARAMETER_MISSING)
*D 8326
        ISSUE_MESSAGE (INFO.MP, CFR$E_INVALID_NAME)
*D 8331
            ISSUE_MESSAGE (INFO.MP, ERROR)
*D 8336
                ISSUE_MESSAGE (INFO.MP, ERROR)
*D 8339
                  ISSUE_MESSAGE (INFO.MP, CFR$M_OK);
*D 8371
    ISSUE_MESSAGE (INFO.MP, CFR$E_PARAMETER_MISSING)
*D 8377
        ISSUE_MESSAGE (INFO.MP, CFR$E_INVALID_NAME)
*D 8390
                  ISSUE_MESSAGE (INFO.MP, ERROR);
*D 8398
                ISSUE_MESSAGE (INFO.MP, ERROR)
*D 8408
                      ISSUE_MESSAGE (INFO.MP, ERROR);
*D 8419
                      ISSUE_MESSAGE (INFO.MP, CFR$M_OK);
*D 8443
    ISSUE_MESSAGE (INFO.MP, CFR$E_NO_MESSAGE)
*D 8457
      ISSUE_MESSAGE (INFO.MP, CFR$M_OK);
*D 8497
    ISSUE_MESSAGE (INFO.MP, CFR$E_NOT_ENOUGH_MEMBERS_TO_LOCK)
*D 8501
      ISSUE_MESSAGE (INFO.MP, CFR$M_OK);
*I 8518
  HAS_AT: BOOLEAN;
  I: INTEGER;
*D 8524
    ISSUE_MESSAGE (INFO.MP, CFR$E_PARAMETER_MISSING)
*D 8535
        ISSUE_MESSAGE (INFO.MP, CFR$E_INVALID_NAME)
*I 8537

        (* MAKE SURE THE NAME DOESN'T CONTAIN COMMERCIAL AT *)

          HAS_AT := FALSE;
          FOR I := 1 TO NAME.LENGTH DO
            BEGIN
              IF NAME.DATA [I] = ASCII_AT THEN
                HAS_AT := TRUE;
            END;

          IF HAS_AT THEN
            ISSUE_MESSAGE (INFO.MP, CFR$E_INVALID_NAME)
*D 8541,8543
          ELSE IF (UNIQUE_MEMBER_NAME (NAME) OR
                   EXACTLY_SAME_NAME (NAME, INFO.MP^.NAME)) AND
                   UNIQUE_CONFERENCE_NAME (NAME) THEN
*D 8549
              ISSUE_MESSAGE (INFO.MP, CFR$M_OK);
*D 8555
            ISSUE_MESSAGE (INFO.MP, CFR$E_NAME_IN_USE)
*D 8584
      ISSUE_MESSAGE (INFO.MP, CFR$M_OK);
*D 8594
      ISSUE_MESSAGE (INFO.MP, CFR$M_OK);
*D 8603
        ISSUE_MESSAGE (INFO.MP, CFR$E_INVALID_NAME)
*D 8608
          ISSUE_MESSAGE (INFO.MP, CFR$M_OK);
*D 8658
    ISSUE_MESSAGE (INFO.MP, CFR$M_OK);
*D 8680
    ISSUE_MESSAGE (INFO.MP, CFR$E_PARAMETER_MISSING)
*D 8685
        ISSUE_MESSAGE (INFO.MP, CFR$E_INVALID_NAME)
*D 8690
            ISSUE_MESSAGE (INFO.MP, CFR$E_NAME_IN_USE)
*D 8694
              ISSUE_MESSAGE (INFO.MP, CFR$M_OK);
*D 8751
        ISSUE_MESSAGE (INFO.MP, CFR$E_INVALID_NAME)
*D 8756
            ISSUE_MESSAGE (INFO.MP, ERROR)
*D 8805
  ISSUE_MESSAGE (INFO.MP, CFR$M_OK);
*I 8826
  IS_NJE_ADDRESS: BOOLEAN;
*D 8833
    ISSUE_MESSAGE (INFO.MP, CFR$E_PARAMETER_MISSING)
*D 8838
        ISSUE_MESSAGE (INFO.MP, CFR$E_INVALID_NAME)
*I 8841
          IF (ERROR = CFR$E_NO_SUCH_MEMBER) AND
             (CFR$V_NJF_CXN_STATE = CFR$C_CONNECTED) THEN
            BEGIN
              PARSE_NJE_ADDRESS (NAME, DUMMY_MEMBER^.NJE_USER,
                                 DUMMY_MEMBER^.NJE_NODE, IS_NJE_ADDRESS);
              IF IS_NJE_ADDRESS THEN
                BEGIN
                  MP := DUMMY_MEMBER;
                  MP^.ACN.LOCAL := CFR$V_NJF_ACN;
                  MP^.ACN.REMOTE := 0;
                  MP^.FLAG.W := TRUE;
                  MP^.IGNORED_MEMBERS := NIL;
                  ERROR := CFR$E_NO_ERROR;
                END;
            END;
*D 8843
            ISSUE_MESSAGE (INFO.MP, ERROR)
*D 8847
                ISSUE_MESSAGE (INFO.MP, CFR$E_EMPTY_MESSAGE)
*D 8851
                    ISSUE_MESSAGE (INFO.MP, CFR$E_WHISPERS_TURNED_OFF)
*D 8856
                        ISSUE_MESSAGE (INFO.MP, CFR$E_IGNORING_YOU)
*D 8859
                          IF MP^.ACN.LOCAL <> CFR$V_NJF_ACN THEN
                            SEND_WHISPER_MESSAGE (INFO.MP, MP)
                          ELSE
                            SEND_PRIVATE_MESSAGE (INFO.MP, MP);
*D 8992
  INFO.MP        := MP;
*D 9042
    ISSUE_MESSAGE (INFO.MP, ERROR)
*D 9049
        ISSUE_MESSAGE (INFO.MP, CFR$E_NO_SUCH_COMMAND)
*D 9052
        ISSUE_MESSAGE (INFO.MP, CFR$E_NO_SUCH_COMMAND)
*D 9055
        ISSUE_MESSAGE (INFO.MP, CFR$E_NO_SUCH_COMMAND)
*D 9059
        ISSUE_MESSAGE (INFO.MP, CFR$E_NOT_MODERATOR)
*D 9144
              ISSUE_MESSAGE (MP, CFR$E_INACTIVE_TIMEOUT);
*D 9163
(* - HANDLE_MEMBER_CONNECT                                              *)
*D 9198
(*$L'HANDLE_MEMBER_CONNECT' *)
*D 9202
(* PROCESS THE CONNECTION OF A MEMBER  .                                *)
*D 9207,9208
PROCEDURE HANDLE_MEMBER_CONNECT
  (     ACN: CFR$T_ACN;
   NJE_USER: CFR$T_CHARS_8;
   NJE_NODE: CFR$T_CHARS_8);
*I 9216

(* SET DUMMY_MEMBER, IN CASE IT'S NEEDED *)

  DUMMY_MEMBER^.ACN := ACN;
  DUMMY_MEMBER^.NJE_USER := NJE_USER;
  DUMMY_MEMBER^.NJE_NODE := NJE_NODE;
*D 9223
      ISSUE_MESSAGE (DUMMY_MEMBER, ERROR);
*D 9234
      DEFINE_MEMBER (ACN, DATABASE_RECORD, NJE_USER, NJE_NODE, MP, ERROR);
*D 9237
          ISSUE_MESSAGE (DUMMY_MEMBER, ERROR);
*D 9255,9257
          IF ACN.LOCAL <> CFR$V_NJF_ACN THEN
            BEGIN
              PRESET_COMMAND_RESULT (RESULT);
              FORMAT_INTRO_MESSAGE (RESULT);
              DELIVER_COMMAND_RESULT (MP, RESULT);
            END;
*D 9262
          IF ((TOTAL.USERS MOD 5000) = 0) AND
             (ACN.LOCAL <> CFR$V_NJF_ACN) THEN
*D 9272,9275
          IF ACN.LOCAL <> CFR$V_NJF_ACN THEN
            BEGIN
              WRITE ('      ENTER CONFER:  ACN = ', ACN.LOCAL:2,
                     ', USERNAME = ');
              FOR I := 1 TO 7 DO
                WRITE (NIT.LOCAL [ACN.LOCAL].USERNAME [I]);
              WRITE (', TERMINAL NAME = ', NIT.LOCAL [ACN.LOCAL].TERMINAL_NAME);
            END
          ELSE
            BEGIN
              WRITE ('      ENTER CONFER:  ACN = ', ACN.LOCAL:2,
                     ', USERNAME = ', NJE_USER);
              WRITE (', NODE = ', NJE_NODE);
            END;
*D 9282
END;  (* HANDLE_MEMBER_CONNECT *)
*I 9346
(* - NJF_CONNECTION_TASK                                                *)
*I 9355
(*$L'NJF_CONNECTION_TASK' *)

(************************************************************************)
(*                                                                      *)
(* NJF CONNECTION MANAGEMENT TASK.                                      *)
(*                                                                      *)
(************************************************************************)

PROCEDURE NJF_CONNECTION_TASK
  (TP: CFR$P_TASK_DESCRIPTOR);

VAR
  I: INTEGER;

BEGIN

  CFR$V_NJF_CXN_TASK_PTR := TP;

  CASE CFR$V_NJF_CXN_STATE OF

    CFR$C_DISCONNECTED: BEGIN
      NIT.GLOBAL.REQUESTED_APPLICATION_NAME := 'NJF    ';
      FOR I := 1 TO 3 DO
        NIT.GLOBAL.DESTINATION_HOST[I] := ':';
      QTLINK;
      IF NIT.GLOBAL.RETURN_CODE = 0 THEN
        BEGIN
          CFR$V_NJF_CXN_STATE := CFR$C_CONNECTING;
          SET_TIMER (TP^.NEXT_INVOCATION, 10000);
        END
      ELSE
        BEGIN
          MESSAGE ('QTLINK FAILED');
          CFR$V_NJF_CXN_STATE := CFR$C_DISCONNECTED;
          SET_TIMER (TP^.NEXT_INVOCATION, 10000);
        END;
      END;

    CFR$C_CONNECTING: BEGIN
      SET_TIMER (TP^.NEXT_INVOCATION, 5000);
      END;

    CFR$C_CONNECTED: BEGIN
      SET_TIMER (TP^.NEXT_INVOCATION, 30000);
      END;

  END;

END;  (* NJF_CONNECTION_TASK *)
*I 9801
            CFR$C_NJF_CONNECTION_TASK:       NJF_CONNECTION_TASK (TP);
*I 9826

(* CREATE THE NJF CONNECTION MANAGEMENT TASK *)

  CREATE_TASK (CFR$C_NJF_CONNECTION_TASK,
               0,                 (* BECOME AVAILABLE IMMEDIATELY *)
               CFR$C_HUGE_TIMER,  (* NEVER END *)
               0);                (* START IMMEDIATELY *)
*D 9927,9928
  (         MP: CFR$P_MEMBER_DESCRIPTOR;
   VAR MESSAGE: CFR$T_NAM_VARIANT_MESSAGE);
*D 9933
*D 9942,9944
  CP := CONFERENCE_POINTERS [MP^.MEMBER_NUMBER];
*D 9962
          MESSAGE.WIDE_ASCII [LENGTH] := ASCII_US;
*D 9967
          MESSAGE.WIDE_ASCII [LENGTH] := ASCII_US;
*D 9972
          MESSAGE.WIDE_ASCII [LENGTH] := ASCII_US;
*D 9979
        IF MESSAGE.WIDE_ASCII [I] > 127 THEN 
*D 9982,9983
            WRITELN ('...WARNING...INPUT CHARACTER > 127: ',
                     MESSAGE.WIDE_ASCII [I]:1);
            MESSAGE.WIDE_ASCII [I] :=
              MESSAGE.WIDE_ASCII [I] MOD 128;  (* USE ONLY LOW 7 BITS *)
*D 9987
        MESSAGE.WIDE_ASCII [I] := 0;
*D 9991,9992
      IF MESSAGE.WIDE_ASCII [1] = ASCII_SLASH THEN 
        HANDLE_COMMAND (MP, MESSAGE.WIDE_ASCII, NIT.GLOBAL.CURRENT_TRANS_SIZE,
                       ERROR)
*I 9994

        (* IF MESSAGE ORIGINATED FROM NJF                                 *)

          IF MP^.ACN.LOCAL = CFR$V_NJF_ACN THEN
            BEGIN
              ADD_TEXT_LINE (MP, MESSAGE.WIDE_ASCII,
                             NIT.GLOBAL.CURRENT_TRANS_SIZE);
              SEND_PUBLIC_MESSAGE (MP, NIL, CP, MP^.FIRST_INPUT_BLOCK);
              MP^.FIRST_INPUT_BLOCK := NIL;
              MP^.CURRENT_INPUT_BLOCK := NIL;
              MP^.LINE_COUNT := 0;
            END
*D 9998
          ELSE IF (NIT.GLOBAL.CURRENT_TRANS_SIZE = 1) AND
                  (MP^.CURRENT_INPUT_BLOCK <> NIL) THEN
*D 10010
              IF (MP^.PROMPT.LENGTH <> 0) AND
                 (MP^.ACN.LOCAL <> CFR$V_NJF_ACN) THEN
*D 10012,10014
                  COPY_SHORT_TO_WIDE_ASCII (MP^.PROMPT.DATA,
                                            NETWORK_MESSAGE.WIDE_ASCII,
                                            MP^.PROMPT.LENGTH);
                  SEND_TEXT_TO_CONNECTION (MP, NETWORK_MESSAGE.WIDE_ASCII,
                                           MP^.PROMPT.LENGTH);
*D 10021
            ISSUE_MESSAGE (MP, CFR$E_MESSAGE_TOO_LONG)
*D 10026
            ADD_TEXT_LINE (MP, MESSAGE.WIDE_ASCII,
                           NIT.GLOBAL.CURRENT_TRANS_SIZE);
*I 10030
(*$L'HANDLE_NJF_INPUT' *)

(************************************************************************)
(*                                                                      *)
(* PROCESS INPUT RECEIVED FROM AN A-A CONNECTION TO NJF.                *)
(*                                                                      *)
(************************************************************************)

PROCEDURE HANDLE_NJF_INPUT
  (            ACN: CFR$T_ACN;
   VAR NJF_MESSAGE: CFR$T_NAM_VARIANT_MESSAGE);

VAR
  DEST_MP: CFR$P_MEMBER_DESCRIPTOR;
  DEST_USER: CFR$T_CHARS_8;
  ERROR: CFR$T_CANNED_MESSAGE;
  I: INTEGER;
  MP: CFR$P_MEMBER_DESCRIPTOR;
  NET_MESSAGE: CFR$T_NAM_VARIANT_MESSAGE;
  NODE: CFR$T_CHARS_8;
  ORIGIN_USER: CFR$T_CHARS_8;
  QUEUE_ENTRY: CFR$P_NJF_QUEUE_ENTRY;

BEGIN
  IF NJF_MESSAGE.NJF_MESSAGE.COMMAND = 'S' THEN
    BEGIN
      IF NJF_MESSAGE.NJF_RESPONSE.STATUS = 0 THEN
        BEGIN

          (* MESSAGE AT HEAD OF QUEUE ACCEPTED AND PROCESSED *)

          QUEUE_ENTRY := CFR$V_NJF_QUEUE_HEAD;
          IF QUEUE_ENTRY <> NIL THEN
            BEGIN
              CFR$V_NJF_QUEUE_HEAD := QUEUE_ENTRY^.NEXT_ENTRY;
              DISPOSE (QUEUE_ENTRY);
              IF CFR$V_NJF_UNACKD_MSGS > 0 THEN
                CFR$V_NJF_UNACKD_MSGS := CFR$V_NJF_UNACKD_MSGS - 1;
            END;
        END
      ELSE IF NJF_MESSAGE.NJF_RESPONSE.STATUS = 2 THEN
        BEGIN

          (* RESOURCE EXHAUSTION TRYING TO PROCESS MESSAGE AT HEAD *)
          (* OF QUEUE. CAUSE IT TO BE RESENT.                      *)

          CFR$V_NJF_UNACKD_MSGS := 0;
        END
      ELSE
        BEGIN
          MESSAGE ('BAD MESSAGE SENT TO NJF');
        END;
    END
  ELSE IF NJF_MESSAGE.NJF_MESSAGE.COMMAND = 'M' THEN
    BEGIN

      (* HANDLE A MESSAGE ADDRESSED TO A SPECIFIC USER OR THE CHAT SERVICE *)

      ORIGIN_USER := NJF_MESSAGE.NJF_MESSAGE.ORIGIN_USER;
      NODE := NJF_MESSAGE.NJF_MESSAGE.NODE;
      DEST_USER := NJF_MESSAGE.NJF_MESSAGE.DESTINATION_USER;

      IF ORIGIN_USER = CFR$C_CHAT_USERNAME THEN
        BEGIN

        (* A SIMPLE STRATEGY IS USED IN AVOIDING POSSIBLE FEEDBACK *)
        (* LOOPS -- IGNORE ANY MESSAGES RECEIVED FROM USERS WITH   *)
        (* USERNAMES MATCHING THE USERNAME OF THE CHAT SERVUCE.    *)

          LOG_TIME_STAMP;
          WRITELN ('      POSSIBLE LOOP DETECTED: FROM = ', ORIGIN_USER,
                   ', TO = ', DEST_USER, ', AT = ', NODE);
        END
      ELSE
        BEGIN
          FOR I := 1 TO NJF_MESSAGE.NJF_MESSAGE.TEXT_LENGTH DO
            NET_MESSAGE.WIDE_ASCII [I] :=
              CFR$V_DISPLAY_TO_ASCII [ORD (NJF_MESSAGE.NJF_MESSAGE.TEXT [I])];
          NIT.GLOBAL.CURRENT_TRANS_SIZE := NJF_MESSAGE.NJF_MESSAGE.TEXT_LENGTH;
          MP := FIND_REMOTE_MEMBER (ORIGIN_USER, NODE);
          IF MP = NIL THEN
            BEGIN
              HANDLE_MEMBER_CONNECT (ACN, ORIGIN_USER, NODE);
              MP := FIND_REMOTE_MEMBER (ORIGIN_USER, NODE);
              IF MP = NIL THEN
                BEGIN
                  MESSAGE ('FAILED TO CONNECT REMOTE MEMBER');
                END;
            END;
          IF MP <> NIL THEN
            BEGIN
              IF DEST_USER = CFR$C_CHAT_USERNAME THEN
                BEGIN
                  HANDLE_USER_INPUT (MP, NET_MESSAGE);
                END
              ELSE
                BEGIN
                  DEST_MP := FIND_MEMBER_BY_USERNAME (DEST_USER);
                  IF DEST_MP = NIL THEN
                    ISSUE_MESSAGE (MP, CFR$E_NO_SUCH_MEMBER)
                  ELSE
                    BEGIN
                      ADD_TEXT_LINE (MP, NET_MESSAGE.WIDE_ASCII,
                                     NJF_MESSAGE.NJF_MESSAGE.TEXT_LENGTH);
                          SEND_PRIVATE_MESSAGE (MP, DEST_MP);
                      MP^.FIRST_INPUT_BLOCK := NIL;
                      MP^.CURRENT_INPUT_BLOCK := NIL;
                      MP^.LINE_COUNT := 0;
                    END;
                END;
            END;
        END;
    END
  ELSE
    BEGIN
      MESSAGE ('UNRECOGNIZED COMMAND CODE RECEIVED FROM NJF');
    END;
END; (* HANDLE_NJF_INPUT *)
*I 10056
  MP: CFR$P_MEMBER_DESCRIPTOR;
*I 10058
  NJF_QUEUE_ENTRY: CFR$P_NJF_QUEUE_ENTRY;
  SUP_MESSAGE_CODE: INTEGER;
*D 10080
  QTGET (NETWORK_MESSAGE);
*D 10117
              BEGIN
                MP := FIND_LOCAL_MEMBER (ACN);
                HANDLE_USER_INPUT (MP, NETWORK_MESSAGE);
              END;
*D 10119,10120
        ELSE IF NIT.GLOBAL.CONNECTION_NUMBER = CFR$V_NJF_ACN THEN
          BEGIN
            HANDLE_NJF_INPUT (ACN, NETWORK_MESSAGE);
          END;
*D 10132
          HANDLE_MEMBER_CONNECT (ACN, CFR$C_CHARS_8, CFR$C_CHARS_8);
*D 10148
          (* HANDLE NJF DISCONNECT *)
          BEGIN
            IF NIT.GLOBAL.CONNECTION_NUMBER = CFR$V_NJF_ACN THEN
              BEGIN
                MESSAGE ('DISCONNECTED FROM NJF');
                DISCARD_ALL_NJF_QUEUE_ENTRIES;
                CFR$V_NJF_CXN_STATE := CFR$C_DISCONNECTED;
                CFR$V_NJF_ACN := 0;
                SET_TIMER (CFR$V_NJF_CXN_TASK_PTR^.NEXT_INVOCATION, 10000);
              END;
          END
*D 10154
        DUMMY_MEMBER^.ACN := ACN;
        DUMMY_MEMBER^.NJE_USER := CFR$C_CHARS_8;
        DUMMY_MEMBER^.NJE_NODE := CFR$C_CHARS_8;
        ISSUE_MESSAGE (DUMMY_MEMBER, CFR$E_USER_BREAK_1);
*D 10158
        DUMMY_MEMBER^.ACN := ACN;
        DUMMY_MEMBER^.NJE_USER := CFR$C_CHARS_8;
        DUMMY_MEMBER^.NJE_NODE := CFR$C_CHARS_8;
        ISSUE_MESSAGE (DUMMY_MEMBER, CFR$E_USER_BREAK_2);
*I 10194

  13: BEGIN  (* A-A CONNECTION REQUEST FAILED *)
        MESSAGE ('FAILED TO CONNECT TO NJF');
        CFR$V_NJF_CXN_STATE := CFR$C_DISCONNECTED;
        SET_TIMER (CFR$V_NJF_CXN_TASK_PTR^.NEXT_INVOCATION, 30000);
      END;

  14: BEGIN  (* A-A CONNECTION COMPLETED *)
        MESSAGE ('CONNECTED TO NJF');
        CFR$V_NJF_ACN := NIT.GLOBAL.CONNECTION_NUMBER;
        CFR$V_NJF_CXN_STATE := CFR$C_CONNECTED;

        (* REQUEST NAM TO CHANGE THE CHARACTER TYPE OF THIS CONNECTION *)
        (* TO 60-BIT A-A CHARACTERS.                                   *)

        NETWORK_MESSAGE.CICT_SUP_MSG.PFC := 302B; (* DC   *)
        NETWORK_MESSAGE.CICT_SUP_MSG.SFC := 0;    (* CICT *)
        NETWORK_MESSAGE.CICT_SUP_MSG.ACN := CFR$V_NJF_ACN;
        NETWORK_MESSAGE.CICT_SUP_MSG.NXP := 1;
        NETWORK_MESSAGE.CICT_SUP_MSG.SCT := 1;
        NETWORK_MESSAGE.CICT_SUP_MSG.ACT := 1; (* 60-BIT A-A CHARS *)
        SUP_MESSAGE_CODE := 0;
        NIT.GLOBAL.CURRENT_TRANS_SIZE := 1;
        QTSUP (SUP_MESSAGE_CODE, NETWORK_MESSAGE);
        IF NIT.GLOBAL.RETURN_CODE = 0 THEN
          BEGIN
          NEW (NJF_QUEUE_ENTRY);
          NJF_QUEUE_ENTRY^.PAYLOAD.REGISTRATION.COMMAND := 'I';
          NJF_QUEUE_ENTRY^.PAYLOAD.REGISTRATION.REGISTERED_USER :=
            CFR$C_CHAT_USERNAME;
          ADD_NJF_QUEUE_ENTRY(NJF_QUEUE_ENTRY);
          END
        ELSE
          BEGIN
            MESSAGE ('FAILED TO SET NJF A-A CHARSET');
          END;
      END;
*D 10234
        NETWORK_MESSAGE.WIDE_ASCII [I] :=
          ERRORS [MP^.PENDING_ERROR].MESSAGE [I];
*D 10244
      NETWORK_MESSAGE.WIDE_ASCII [1] := ASCII_BLANK;
*D 10246
        NETWORK_MESSAGE.WIDE_ASCII [I + 1] := MP^.PROMPT.DATA [I];
*D 10252
      FILL_NETWORK_MESSAGE (MP, NETWORK_MESSAGE, LENGTH);
*D 10256
  SEND_TEXT_TO_CONNECTION (MP, NETWORK_MESSAGE.WIDE_ASCII, LENGTH);
*I 10277
  I: 0..CFR$C_MAX_NJF_UNACKD_MSGS;
*I 10281
  NEXT_QUEUE_ENTRY: CFR$P_NJF_QUEUE_ENTRY;
*I 10323

(* SEND QUEUED MESSAGES TO NJF *)

  IF CFR$V_NJF_CXN_STATE = CFR$C_CONNECTED THEN
    BEGIN
      NEXT_QUEUE_ENTRY := CFR$V_NJF_QUEUE_HEAD;
      I := 0;
      WHILE (NEXT_QUEUE_ENTRY <> NIL) AND (I < CFR$V_NJF_UNACKD_MSGS) DO
        BEGIN
        NEXT_QUEUE_ENTRY := NEXT_QUEUE_ENTRY^.NEXT_ENTRY;
        I := I + 1;
        END;
      WHILE (NEXT_QUEUE_ENTRY <> NIL) AND
            (NIT.LOCAL [CFR$V_NJF_ACN].ABL > 0) AND
            (CFR$V_NJF_UNACKD_MSGS < CFR$C_MAX_NJF_UNACKD_MSGS) DO
        BEGIN
          NIT.GLOBAL.CONNECTION_NUMBER := CFR$V_NJF_ACN;
          IF NEXT_QUEUE_ENTRY^.PAYLOAD.MESSAGE.COMMAND = 'M' THEN
            BEGIN
              NIT.GLOBAL.CURRENT_TRANS_SIZE :=
                ((NEXT_QUEUE_ENTRY^.PAYLOAD.MESSAGE.TEXT_LENGTH + 9) DIV 10)+3;
              NETWORK_MESSAGE.NJF_MESSAGE := NEXT_QUEUE_ENTRY^.PAYLOAD.MESSAGE;
            END
          ELSE (* ASSUME REGISTRATION REQUEST *)
            BEGIN
              NIT.GLOBAL.CURRENT_TRANS_SIZE := 1;
              NETWORK_MESSAGE.NJF_REGISTRATION :=
                NEXT_QUEUE_ENTRY^.PAYLOAD.REGISTRATION;
            END;
          NIT.GLOBAL.CHARACTER_SET := 1;
          QTPUT (NETWORK_MESSAGE);
          IF NIT.GLOBAL.RETURN_CODE = 0 THEN
            BEGIN
            CFR$V_NJF_UNACKD_MSGS := CFR$V_NJF_UNACKD_MSGS + 1;
            NEXT_QUEUE_ENTRY := NEXT_QUEUE_ENTRY^.NEXT_ENTRY;
            END
          ELSE
            BEGIN
            MESSAGE ('FAILED TO SEND MESSAGE TO NJF');
            NEXT_QUEUE_ENTRY := NIL; (* TERMINATE LOOP *)
            END;
        END;
    END;
*I 10636

(* CREATE DUMMY MEMBER *)

  NEW (DUMMY_MEMBER);
  DUMMY_MEMBER^.NAME.DATA [1] := CFR$V_DISPLAY_TO_ASCII [ORD ('.')];
  DUMMY_MEMBER^.NAME.LENGTH := 1;
  FOR I := 1 TO 7 DO
    DUMMY_MEMBER^.USERNAME [I] := CFR$V_DISPLAY_TO_ASCII [ORD ('.')];
  DUMMY_MEMBER^.MEMBER_NUMBER := CFR$C_MAX_LOCAL_USERS + 1;
  DUMMY_MEMBER^.ACN.LOCAL := 0;
  DUMMY_MEMBER^.ACN.REMOTE := 0;
  DUMMY_MEMBER^.FIRST_INPUT_BLOCK := NIL;
  DUMMY_MEMBER^.CURRENT_INPUT_BLOCK := NIL;
  DUMMY_MEMBER^.LINE_COUNT := 0;
  CFR$V_MEMBER_NUMBERS [DUMMY_MEMBER^.MEMBER_NUMBER] := TRUE;
  MEMBER_POINTERS [DUMMY_MEMBER^.MEMBER_NUMBER] := DUMMY_MEMBER;
  CONFERENCE_POINTERS [DUMMY_MEMBER^.MEMBER_NUMBER] :=
    CFR$V_LIMBO_CONFERENCE_POINTER;
*I 10531
      PRIVATES := 0;
