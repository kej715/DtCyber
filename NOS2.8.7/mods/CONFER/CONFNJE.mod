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
*D 111
  CFR$C_CONFER_VERSION = 'VERSION 1.1';
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

  CFR$C_MAX_NJF_UNACKD_MSGS = 3;
*I 573
  CFR$C_CHARS_8 = '        ';
*I 617
  CFR$T_CHARS_8 = PACKED ARRAY [1..8] OF CHAR;
*I 619
  CFR$T_CHARS_130 = PACKED ARRAY [1..130] OF CHAR; 
*I 940

  (* MEMBER'S NJE USER NAME AND NODE NAME. THESE ARE BLANK FOR MEMBERS  *)
  (* CONNECTED FROM LOCAL TERMINALS.                                    *)

    NJE_USER: CFR$T_CHARS_8;
    NJE_NODE: CFR$T_CHARS_8;
*I 1298
    CFR$C_NJF_CONNECTION_TASK,       (* MANAGE NJF A-A CONNECTION *)
*I 1308
      CFR_C_NJF_CONNECTION_TASK: (JUNK0: INTEGER);
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
    ORIGIN_NODE: CFR$T_CHARS_8;
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
*D 1537
  NETWORK_MESSAGE: CFR$T_NAM_VARIANT_MESSAGE;
*I 1660

(* NJF A-A CONNECTION MANAGEMENT                                        *)

  CFR$V_NJF_ACN: 0..CFR$C_MAX_CONNECTIONS;
  CFR$V_NAM_CICT_REQUEST: CFR$T_CICT_SUP_MESSAGE;
  CFR$V_NJF_CXN_STATE: CFR$T_NJF_CXN_STATE;
  CFR$V_NJF_CXN_TASK_PTR: CFR$P_TASK_DESCRIPTOR;
  CFR$V_NJF_QUEUE_HEAD: CFR$P_NJF_QUEUE_ENTRY;
  CFR$V_NJF_QUEUE_LENGTH: 0..CFR$C_MAX_NJF_QUEUE_LENGTH;
  CFR$V_NJF_QUEUE_TAIL: CFR$P_NJF_QUEUE_ENTRY;
  CFR$V_NJF_UNACKD_MSGS: 0..CFR$C_MAX_NJF_UNACKD_MSGS;
*I 1807

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
   VAR MESSAGE: CFR$T_NAM_VARIANT_MESSAGE); FORTRAN;
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

FUNCTION FIND_REMOTE_MEMBER
  (NJE_USER: CFR$T_CHARS_8;
   NJE_NODE: CFR$T_CHARS_8): CFR$P_MEMBER_DESCRIPTOR;

VAR
  DONE: BOOLEAN;
  I: 0..CFR$C_MAX_CONFER_MEMBERS;
  MP: CFR$P_MEMBER_DESCRIPTOR;

BEGIN

  DONE := FALSE;
  I := CFR$C_MAX_LOCAL_USERS + 1;
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

*D 3641
         NETWORK_MESSAGE.WIDE_ASCII [I] := ERRORS [ERROR].MESSAGE [I];
*D 3645
       QTPUT (NETWORK_MESSAGE);
*D 3649
       MP := FIND_LOCAL_MEMBER (ACN);
*D 3676
                 NETWORK_MESSAGE.WIDE_ASCII [I] := ERRORS [ERROR].MESSAGE [I];
*D 3680
               QTPUT (NETWORK_MESSAGE);
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
*D 4709,4710
                NETWORK_MESSAGE.WIDE_ASCII [I + 1] := TXT [I];
              NETWORK_MESSAGE.WIDE_ASCII [1] := ASCII_BLANK;
*D 4716
                NETWORK_MESSAGE.WIDE_ASCII [I] := TXT [I];
*D 4720
          QTPUT (NETWORK_MESSAGE);
*D 4847,4848
      COPY_SHORT_TO_WIDE_ASCII (MESSAGE_TEXT, NETWORK_MESSAGE.WIDE_ASCII,
                               LENGTH);
      SEND_TEXT_TO_CONNECTION (MP^.ACN, NETWORK_MESSAGE.WIDE_ASCII, LENGTH,
                              CFR$C_DO_NOT_SHIFT);
*D 5191
  ELSE IF ACN.LOCAL <> CFR$V_NJF_ACN THEN
*D 5650
  ELSE IF CONFIG.UMASS AND (ACN.LOCAL <> CFR$V_NJF_ACN) THEN
*I 6469
              NJE_USER: CFR$T_CHARS_8;
              NJE_NODE: CFR$T_CHARS_8;
*I 6491
      MP^.NJE_USER := NJE_USER;
      MP^.NJE_NODE := NJE_NODE;
*D 9234
      DEFINE_MEMBER (ACN, DATABASE_RECORD, CFR$C_CHARS_8, CFR$C_CHARS_8,
                     MP, ERROR);
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
  (        ACN: CFR$T_ACN;
   VAR MESSAGE: CFR$T_NAM_VARIANT_MESSAGE);
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
*D 10012,10013
                  COPY_SHORT_TO_WIDE_ASCII (MP^.PROMPT.DATA,
                                            NETWORK_MESSAGE.WIDE_ASCII,
                                            MP^.PROMPT.LENGTH);
                  SEND_TEXT_TO_CONNECTION (MP^.ACN, NETWORK_MESSAGE.WIDE_ASCII,
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
  DESTINATION_USER: CFR$T_CHARS_8;
  ORIGIN_NODE: CFR$T_CHARS_8;
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

      (* HANDLE A MESSAGE ADDRESSED TO A SPECIFIC USER OR 'CHAT' *)

      ORIGIN_USER := NJF_MESSAGE.NJF_MESSAGE.ORIGIN_USER;
      ORIGIN_NODE := NJF_MESSAGE.NJF_MESSAGE.ORIGIN_NODE;
      DESTINATION_USER := NJF_MESSAGE.NJF_MESSAGE.DESTINATION_USER;
      IF DESTINATION_USER = 'CHAT    ' THEN
        BEGIN
          MESSAGE ('MESSAGE RECEIVED FOR CHAT');
        END
      ELSE
        BEGIN
          MESSAGE ('MESSAGE RECEIVED FOR USER');
        END;
    END
  ELSE
    BEGIN
      MESSAGE ('UNRECOGNIZED COMMAND CODE RECEIVED FROM NJF');
    END;
END; (* HANDLE_NJF_INPUT *)
*I 10058
  NJF_QUEUE_ENTRY: CFR$P_NJF_QUEUE_ENTRY;
  SUP_MESSAGE_CODE: INTEGER;
*D 10080
  QTGET (NETWORK_MESSAGE);
*D 10117
              HANDLE_USER_INPUT (ACN, NETWORK_MESSAGE);
*D 10119,10120
        ELSE IF NIT.GLOBAL.CONNECTION_NUMBER = CFR$V_NJF_ACN THEN
          BEGIN
            HANDLE_NJF_INPUT (ACN, NETWORK_MESSAGE);
          END;
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
          NJF_QUEUE_ENTRY^.PAYLOAD.REGISTRATION.REGISTERED_USER := 'CHAT    ';
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
  SEND_TEXT_TO_CONNECTION (MP^.ACN, NETWORK_MESSAGE.WIDE_ASCII, LENGTH,
                           CFR$C_DO_NOT_SHIFT);
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
