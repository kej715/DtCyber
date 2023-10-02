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
*I 380

(************************************************************************)
(*                                                                      *)
(* MAXIMUM NUMBER OF MESSAGES THAT MAY BE QUEUED CONCURRENTLY FOR       *)
(* TRANSMISSION TO NJF.                                                 *)
(*                                                                      *)
(************************************************************************)

  CFR$C_MAX_NJF_QUEUE_LENGTH = 200;

(************************************************************************)
(*                                                                      *)
(* MAXIMUM NUMBER OF UNACKNOWLEDGED MESSAGES THAT MAY BE SENT TO NJF.   *)
(*                                                                      *)
(************************************************************************)

  CFR$C_MAX_NJF_UNACKD_MSGS = 3;
*I 573
  CFR$C_CHARS_8 = '        ';
*I 617
  CFR$T_CHARS_8 = PACKED ARRAY [1..8] OF CHAR;
*I 619
  CFR$T_CHARS_130 = PACKED ARRAY [1..130] OF CHAR; 
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
    DESTINATION_NODE: CFR$T_CHARS_8;
    TEXT_LENGTH: 0..4095;
    TEXT: CFR$T_CHARS_130;
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
    CFR$C_CICT_SUP_MESSAGE_VARIANT);

  CFR$T_NAM_VARIANT_MESSAGE = RECORD
    CASE CFR$T_NAM_MESSAGE_TYPES OF
     CFR$C_WIDE_ASCII_VARIANT       : (WIDE_ASCII:   CFR$T_NETWORK_BUFFER);
     CFR$C_NJF_MESSAGE_VARIANT      : (NJF_MESSAGE:  CFR$T_NJF_MESSAGE);
     CFR$C_CICT_SUP_MESSAGE_VARIANT : (CICT_SUP_MSG: CFR$T_CICT_SUP_MESSAGE);
  END;

  CFR$P_NAM_VARIANT_MESSAGE = ^CFR$T_NAM_VARIANT_MESSAGE;

  CFR$P_NJF_QUEUE_ENTRY = ^CFR$T_NJF_QUEUE_ENTRY;
  CFR$T_NJF_QUEUE_ENTRY = RECORD
    NEXT_ENTRY: CFR$P_NJF_QUEUE_ENTRY;
    MESSAGE: CFR$T_NJF_MESSAGE;
  END;
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
*D 3641
         NETWORK_MESSAGE.WIDE_ASCII [I] := ERRORS [ERROR].MESSAGE [I];
*D 3645
       QTPUT (NETWORK_MESSAGE);
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
*I 10058
  NJF_QUEUE_ENTRY: CFR$P_NJF_QUEUE_ENTRY;
  SUP_MESSAGE: ^CFR$T_NAM_VARIANT_MESSAGE;
  SUP_MESSAGE_CODE: INTEGER;
*D 10080
  QTGET (NETWORK_MESSAGE);
*D 10117
              HANDLE_USER_INPUT (ACN, NETWORK_MESSAGE);
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

        NEW (SUP_MESSAGE);
        SUP_MESSAGE^.CICT_SUP_MSG.PFC := 302B; (* DC   *)
        SUP_MESSAGE^.CICT_SUP_MSG.SFC := 0;    (* CICT *)
        SUP_MESSAGE^.CICT_SUP_MSG.ACN := CFR$V_NJF_ACN;
        SUP_MESSAGE^.CICT_SUP_MSG.NXP := 1;
        SUP_MESSAGE^.CICT_SUP_MSG.SCT := 1;
        SUP_MESSAGE^.CICT_SUP_MSG.ACT := 1; (* 60-BIT A-A CHARS *)
        SUP_MESSAGE_CODE := 0;
        QTSUP (SUP_MESSAGE_CODE, SUP_MESSAGE^);
        DISPOSE (SUP_MESSAGE);
        IF NIT.GLOBAL.RETURN_CODE = 0 THEN
          BEGIN
          NEW (NJF_QUEUE_ENTRY);
          NJF_QUEUE_ENTRY^.MESSAGE.COMMAND := 'I';
          NJF_QUEUE_ENTRY^.MESSAGE.ORIGIN_USER := 'CHAT    ';
          NJF_QUEUE_ENTRY^.MESSAGE.DESTINATION_USER := '        ';
          NJF_QUEUE_ENTRY^.MESSAGE.DESTINATION_NODE := '        ';
          NJF_QUEUE_ENTRY^.MESSAGE.TEXT_LENGTH := 0;
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
