USERMSG
*IDENT USERMSG   KEJ.  23/07/04
*/
*/  MOD TO IMPLEMENT FEATURE ENABLING OTHER APPLICATIONS TO CONNECT
*/  TO NJF AND SEND/RECEIVE NJE COMMANDS AND MESSAGES TO/FROM USERS
*/  ON OTHER NJE NODES.
*/
*DECK COMXNJF
*I 32
      DEF DLAABL$       # 17 #;      # DOWNLINE A-A BLOCK LENGTH #
*D 156
      DEF MAXCMD$       # 100 #;     # MAX NUMBER OF QUEUED COMMANDS #
*I 238
      DEF A2ALN$        # 2 #;       # A-A APPLICATION LIST NUMBER #
*EDIT COMXNJF
*/
*DECK COMYCBD
*I 46

      BASED
      ARRAY UTA$ [0:0] U;            # USER COMMAND/MESSAGE #
        BEGIN
        ITEM TACMD      C<01>;       # COMMAND CHAR # 
        ITEM TAOUSER    C<08>;       # ORIGIN USER ID #
        ITEM TARS2      C<01>;       # IGNORED #
        ITEM TADUSER    C<08>;       # DESTINATION USER ID #
        ITEM TARS3      C<02>;       # IGNORED #
        ITEM TADNODE    C<08>;       # DESTINATION NODE NAME # 
        ITEM TATEXTL    U<12>;       # MESSAGE TEXT LENGTH #
        ITEM TATEXT     C<130>;      # MESSAGE TEXT #
        END
*EDIT COMYCBD
*/
*DECK COMYCQD
*I 58
        ITEM CQH$APO    B<01,+>;     # LOCAL APP ORIGINATED FLAG #
*EDIT COMYCQD
*/
*DECK COMYLIT
*D 301
        ITEM LPT$RS1    U<03>;       # RESERVED #
*I 304
        ITEM LPT$ANM    C<07>;       # A-A PEER NAME #
        ITEM LPT$ABN    U<18>;       # APPLICATION BLOCK NUMBER #
        ITEM LPT$OUSER  C<08>;       # ORIGIN USER ID #
        ITEM LPT$RS2    U<11>;       # RESERVED #
        ITEM LPT$A2A    B<01>;       # A-A CONNECTION #
*EDIT COMYLIT
*/
*DECK COMYMSC
*I 49

      ITEM CFRACN       U;           # ACN OF *CONFER* CONNECTION #
*EDIT COMYMSC
*/
*DECK COMYUDP
*I 16
      DEF ULTA2L$       # 410 #;     # UPLINE A-A TEXT LENGTH WORDS #
*D 74
      ARRAY ULTA [0:ULTA2L$] U;;     # UPLINE TEXT AREA #
*EDIT COMYUDP
*/
*DECK CMD
*D 223
          IF NOT CQH$APO
          THEN
            BEGIN
            CQH$FLB[0] = CQH$FLB[0] LAN X"7F"; # CLEAR COMMAND BIT #
            END
*D 238
          ELSE IF NOT CQH$APO        # PROCESS CMD TEXT #
          THEN
*/
*DECK CMDDNA
*I 128
              AND NOT LPT$A2A[I]
*/
*DECK CMDDUP
*I 74
             AND NOT LPT$A2A[I]
*/
*DECK CMDOUT
*D 183,184
          C<0,8>TEXTA =C<1,8>TIME;   # SET TIME IN MSG #
          C<8,MSGL>TEXTA =           # MOVE MSG TO TEXTA #
*D 186
          MSGL = MSGL + 8;           # INCREASE MSG LENGTH #
*/
*DECK CMDSTN
*I 108
           AND NOT LPT$A2A[I]
*/
*DECK DDP
*I 78
            AND NOT LPT$A2A[I]
*/
*DECK GETAID
*I 56
              AND NOT LPT$A2A[J]
*I 84
              AND NOT LPT$A2A[J]
*/
*DECK GETEBA
*I 25
      ITEM BLEN         U;           # BLOCK LENGTH #
*I 33

      IF LPT$A2A[LIT$ACN[LINDX]]
      THEN
        BEGIN
        BLEN = DLAABL$;
        END
      ELSE
        BEGIN
        BLEN = DLBL$;
        END
*D 35,37
      IF (LIT$IN[LINDX] EQ (LIT$OUT[LINDX]-BLEN))
        OR ((LIT$IN[LINDX] EQ (LIT$LIMIT[LINDX]-BLEN))
          AND (LIT$OUT[LINDX] EQ LIT$FIRST[LINDX]))
*D 45
      LIT$IN[LINDX] = LIT$IN[LINDX] + BLEN;   # ADVANCE *IN* PTR #
*/
*DECK GETFBA
*I 24
      ITEM BLEN         U;           # BLOCK LENGTH #
*I 40

      IF LPT$A2A[LIT$ACN[LINDX]]
      THEN
        BEGIN
        BLEN = DLAABL$;
        END
      ELSE
        BEGIN
        BLEN = DLBL$;
        END
*D 43
      LIT$PUT[LINDX] = LIT$PUT[LINDX] + BLEN;   # ADVANCE *PUT* PTR #
*/
*DECK LNE
*I 58
           AND NOT LPT$A2A[I]
*/
*DECK PRSHCF
*I 241

#
*     INITIALIZE REMAINDER OF *LIT*
#

      SLOWFOR I = LSTLINDX + 1 STEP 1 UNTIL MAXLINE$
      DO
        BEGIN
        LIT$NAM[I] = "       ";
        LIT$NUM[I] = "        ";
        LIT$ACN[I] = 0;
        LIT$FWD[I] = 0;
        LIT$BAL[I] = FALSE;
        LIT$BQD[I] = FALSE;
        LIT$ACT[I] = FALSE;
        END
*/
*DECK PRSMSC
*I 58
      CFRACN = 0;                    # CLEAR *CONFER* ACN #
*/
*DECK PRSOFF
*I 121
           AND NOT LPT$A2A[I]
*/
*DECK QCP
*I 110
           AND NOT LPT$A2A[I]
*/
*DECK SMPACK
*D NJV2065.4
        IF LPT$A2A[LIT$ACN[LINDX]]
        THEN
          BEGIN
          LIT$OUT[LINDX] = LIT$OUT[LINDX] + DLAABL$;
          END
        ELSE
          BEGIN
          LIT$OUT[LINDX] = LIT$OUT[LINDX] + DLBL$;
          END
*/
*DECK SMPCB
*D 63,NJV2112.1
      IF LINDX LQ LSTLINDX
      THEN
        BEGIN
        CB$NUM[0] = LIT$NUM[LINDX];  # SET LINE NUMBER IN MESSAGE #
        MESSAGE(CBMSG[0],SYSDF$);    # ISSUE DAYFILE MESSAGE #
        WRTKDS(LOC(CBMSG[0]),28,999,2); # WRITE CB MSG TO K-DISPLAY #
        END
*D 74,75
      IF LINDX LQ LSTLINDX
      THEN
        BEGIN
        REQXMTL(LINDX);              # REQUEUE TRANSMIT FILES #
        PRGRCVL(LINDX);              # PURGE RECEIVE FILE #
        END
*/
*DECK SMPEND
*I 46

      ARRAY A2AMSG [0:0] U;
        BEGIN
        ITEM A2A$TXT    C<30> =
          [
          " AAAAAAA DISCONNECTED.",
          ];
        ITEM A2A$ZBT    U<12> = [1(0)];
        ITEM A2A$FL1    C<01,A2A$TXT>;  # FILLER #
        ITEM A2A$ANM    C<07,+>;        # APPLICATION NAME #
        END
*D 67,78
      IF LPT$A2A[LIT$ACN[LINDX]]
      THEN
        BEGIN
        A2A$ANM[0] = LPT$ANM[LIT$ACN[LINDX]]; # SET APP NAME #
        MESSAGE(A2AMSG[0],USRDF$);   # ISSUE DAYFILE MESSAGE #
        IF LPT$ANM[LIT$ACN[LINDX]] EQ "CONFER"
        THEN
          BEGIN
          CFRACN = 0;
          END
        END
      ELSE
        BEGIN
        END$NUM[0] = LIT$NUM[LINDX]; # SET LINE NUMBER IN MESSAGE #
        END$NUM[1] = LIT$NUM[LINDX]; # SET LINE NUMBER IN MESSAGE #
        END$NUM[2] = LIT$NUM[LINDX]; # SET LINE NUMBER IN MESSAGE #

        END$KCC[1] = XCFD(LIT$TCC[LINDX]); # CONVERT XMT CHAR COUNT #
        END$KCC[2] = XCFD(LIT$RCC[LINDX]); # CONVERT RCV CHAR COUNT #

        MESSAGE(ENDMSG[0],SYSDF$);   # ISSUE DAYFILE MESSAGE #

        WRTKDS(LOC(ENDMSG[0]),28,999,2); # WRITE END MSG TO K-DIS #
        MESSAGE(ENDMSG[1],USRDF$);   # ISSUE DAYFILE MESSAGE #
        MESSAGE(ENDMSG[2],USRDF$);   # ISSUE DAYFILE MESSAGE #
        END
*I 84
      LIT$ACN[LINDX] = 0;            # CLEAR ACN #
*/
*DECK SMPINI
*I 53
      ARRAY A2AMSG [0:0] U;          # APPLICATION CONNECTED MESSAGE #
        BEGIN
        ITEM A2A$TXT    C<20> =
          [
          " AAAAAAA CONNECTED.",
          ];
        ITEM A2A$ZBT    U<12> = [1(0)];
        ITEM A2A$FL1    C<01,A2A$TXT>;  # FILLER #
        ITEM A2A$ANM    C<07,+>;        # APPLICATION NAME #
        END
*D 114,NJV2112.1
      IF LPT$A2A[LIT$ACN[LINDX]]
      THEN
        BEGIN
        A2A$ANM[0] = LPT$ANM[LIT$ACN[LINDX]]; # SET APP NAME #
        MESSAGE(A2AMSG[0],USRDF$);            # ISSUE DAYFILE MESSAGE #
        END
      ELSE
        BEGIN
        INI$NUM[0] = LIT$NUM[LINDX];          # SET LINE NUMBER #
        MESSAGE(INIMSG[0],USRDF$);            # ISSUE DAYFILE MESSAGE #
        WRTKDS(LOC(INIMSG[0]),28,999,0);      # WRITE TO K-DISPLAY #
        END
*/
*DECK SMPNAK
*I 52

      ARRAY A2AMSG [0:0] U;          # APPLICATION LOST BLOCK MESSAGE #
        BEGIN
        ITEM A2A$TXT    C<30> =
          [
          " AAAAAAA LOST BLOCK, ABN = NNN.",
          ];
        ITEM A2A$ZBT    U<12> = [1(0)];
        ITEM A2A$FL1    C<01,A2A$TXT>;  # FILLER #
        ITEM A2A$ANM    C<07,+>;        # APPLICATION NAME #
        ITEM A2A$FL2    C<19,+>;        # FILLER #
        ITEM A2A$ABN    C<03,+>;        # BLOCK NUMBER #
        END
*D 69,72
      IF LPT$A2A[LIT$ACN[LINDX]]
      THEN
        BEGIN
        A2A$ABN[0] = C<7,3>ABN;      # SET BLOCK NUMBER IN MESSAGE #
        A2A$ANM[0] = LPT$ANM[LIT$ACN[LINDX]]; # SET APP NAME #
        MESSAGE(A2AMSG[0],USRDF$);   # ISSUE DAYFILE MESSAGE #
        END
      ELSE
        BEGIN
        NAK$ABN[0] = C<7,3>ABN;      # SET BLOCK NUMBER IN MESSAGE #
        NAK$NUM[0] = LIT$NUM[LINDX]; # SET LINE NUMBER IN MESSAGE #
        MESSAGE(NAKMSG[0],USRDF$);   # ISSUE DAYFILE MESSAGE #
        END
*/
*DECK SMPREQ
*I 86

      IF CONDT[0] EQ 5 OR CONDT[0] EQ 6  # A-A CONNECTION REQUEST #
      THEN
        BEGIN
        SLOWFOR I = LSTLINDX + 1 STEP 1 UNTIL MAXLINE$
        DO
          BEGIN
          IF LIT$ACN[I] EQ 0
          THEN
            BEGIN
            LPT$IDX[ACN]   = I;         # SET *LIT* INDEX #
            LPT$NTX[ACN]   = 0;         # CLR NEXT-TO-TRANSMIT INDEX #
            LPT$QTX[ACN]   = 0;         # CLR QUEUE TABLE INDEX #
            LPT$CFG[ACN]   = FALSE;     # CLR TERMINAL CONFIG FLAG #
            LPT$NXT[ACN]   = FALSE;     # CLR NEXT-TO-TRANSMIT FLAG #
            LPT$ANM[ACN]   = CONTNM[0]; # SET A-A PEER NAME #
            LPT$ABN[ACN]   = 0;         # INIT APP BLOCK NUMBER #
            LPT$OUSER[ACN] = "        ";# SET A-A USER ID #
            LPT$A2A[ACN]   = TRUE;      # SET A2A FLAG #
            LIT$ABL[I]     = CONABL[0]; # SET BLOCK LIMIT #
            LIT$OBC[I]     = 0;         # CLR OUTSTANDING BLOCK COUNT #
            LIT$SLCS[I]    = 0;         # CLR *SM* LAST CASE/STATE #     
            LIT$SCCS[I]    = 0;         # CLR *SM* CUR  CASE/STATE #  
            LIT$LLCS[I]    = 0;         # CLR *LINE* LAST CASE/STATE #   
            LIT$LCCS[I]    = 0;         # CLR *LINE* CUR  CASE/STATE #
            LIT$LS[I]      = 0;         # CLR LINE STATUS #              
            LIT$SI[I]      = 0;         # CLR STATUS INPUTS #            
            LIT$ACN[I]     = ACN;       # SET CONNECTION NUMBER #          
            LIT$TCC[I]     = 0;         # CLR XMIT CHARACTER COUNT #     
            LIT$RCC[I]     = 0;         # CLR RCVR CHARACTER COUNT #     
            LIT$ASC[I]     = 0;         # CLR ACTIVE STREAM COUNT #      
            LIT$ABC[I] = LIT$ABL[I] + MINBLK$; # SET BLOCK COUNT #       
            LIT$RBZ[I] = DLAABL$ * 60;  # SET BUFFER SIZE #         
            LIT$SL[I]      = CONSL[0];  # SET SECURITY LEVEL #             
            LIT$AUT[I]     = FALSE;     # NO AUTO-SIGNON #
            CONCNT         = CONCNT + 1;# INCREMENT CONNECTION COUNT #
            RCODE          = S"NORMAL"; # SET REPSONSE CODE #
            SMPRSP(RTYPE,RCODE);        # SEND NORMAL REPLY #
            IF LPT$ANM[ACN] EQ "CONFER"
            THEN
              BEGIN
              CFRACN = ACN;
              END
            RETURN;                     # EXIT #
            END
          END

        RCODE = S"CONLIMIT";         # SET REPSONSE CODE #
        SMPRSP(RTYPE,RCODE);         # REJECT CONNECTION #
        RETURN;                      # EXIT #
        END
*I 104
          LPT$A2A[ACN] = FALSE;      # CLEAR A2A FLAG #
*/
*DECK SMPRSP
*I 42
      ITEM ACN          U;           # APPLICATION CONNECTION NUMBER #
*I 63
*CALLC COMYLIT
*D 100,101
          ACN = CONACN[0];           # EXTRACT CONNECTION NUMBER #
          IF LPT$A2A[ACN]
          THEN
            BEGIN
            CONACT[0] = CT60XP$;     # SET CHARACTER TYPE #
            CONALN[0] = A2ALN$;      # SET LIST NUMBER #
            END
          ELSE
            BEGIN
            CONACT[0] = CT8IN8$;     # SET CHARACTER TYPE #
            CONALN[0] = ALN$;        # SET LIST NUMBER #
            END
*/
*DECK UDP
*I 23
        PROC GETEBA;                 # GET EMPTY BLOCK BUFFER #
        PROC GETEQA;                 # GET EMPTY CMD QUEUE BUFFER #
*I 24
        PROC MOVEI;                  # MOVE DATA BLOCK INDIRECT #
*I 27
        PROC ZFILL;                  # ZERO FILL ARRAY #
*I 37
*CALLC COMYCIT
*CALLC COMYCMD
*CALLC COMYCQD
*CALLC COMYLIT
*CALLC COMYNIT
*I 43
      ITEM A2ASENT      U;           # COUNT OF A-A MESSAGES SENT #
      ITEM ACN          U;           # APPLICATION CONNECTION NUMBER #
      ITEM ACN2         U;           # APPLICATION CONNECTION NUMBER #
      ITEM HADR         U;           # HEADER ADDRESS #
*I 44
      ITEM LINDX        U;           # *LIT* INDEX #
      ITEM TEXTL        U;           # MESSAGE TEXT LENGTH #
      ITEM QADDR        I;           # COMMAND QUEUE ADDRESS #
      ITEM TADR         U;           # TEXT AREA ADDRESS #
*I 45

      ARRAY A2ARSP [0:0] U;
        BEGIN
        ITEM RSP$TYPE   C<01>;          # RESPONSE TYPE #
        ITEM RSP$CODE   U<54>;          # RESPONSE CODE #
        END
*D NJV2057.7
          GOTO A2A;                  # PROCESS A-A INPUT #
*I 80

#
*     PROCESS UPLINE A-A DATA BLOCKS
#

A2A:
      REPEAT WHILE NSUP$I
      DO
        BEGIN

        NETGETL(A2ALN$,ULHA[0],ULTA[0],ULTA2L$); # GET DATA BLOCK #
        P<ABH$> = LOC(ULHA[0]);      # SET *ABH* POINTER #

        IF ABHABT[0] EQ BLKTYPE"NULBLK"
        THEN                         # NULL BLOCK READ #
          BEGIN
          RETURN;                    # EXIT #
          END

        P<UTA$>  = LOC(ULTA[0]);
        ACN      = ABHADR[0];        # EXTRACT CONNECTION NUMBER #
        TEXTL    = ABHTLC[0];        # EXTRACT TEXT LENGTH #

        LPT$OUSER[ACN] = TAOUSER;

        RSP$TYPE = "S";
        RSP$CODE = 0; # PRESET SUCCESS #

        IF TACMD EQ "M"              # SEND MESSAGE #
        THEN
          BEGIN
          IF TADNODE EQ OWNNODE          # MESSAGE FOR THIS NODE #
          THEN
            BEGIN
            #
            *  PASS 1. ENSURE THAT SUFFICIENT RESOURCES ARE
            *  AVAILABLE TO SEND MESSAGE TO ALL RECIPIENTS
            #
            SLOWFOR ACN2 = MINACN$ STEP 1
              UNTIL MAXACN$
            DO
              BEGIN
              LINDX = LPT$IDX[ACN2];
              IF LPT$A2A[ACN2]
                 AND TADUSER EQ LPT$OUSER[ACN2]
                 AND LPT$OUSER[ACN2] NQ "        "
                 AND ((LIT$IN[LINDX] EQ (LIT$OUT[LINDX]-DLAABL$))
                      OR ((LIT$IN[LINDX] EQ (LIT$LIMIT[LINDX]-DLAABL$))
                          AND (LIT$OUT[LINDX] EQ LIT$FIRST[LINDX])))
              THEN
                BEGIN
                RSP$CODE = 2; # RESOURCE EXHAUSTION #
                END
              END
            #
            *  PASS 2. SEND NESSAGES IF SUFFICIENT RESOURCES
            *  ARE AVAILABLE
            #
            IF RSP$CODE EQ 0
            THEN
              BEGIN
              A2ASENT = 0;
              SLOWFOR ACN2 = MINACN$ STEP 1
                UNTIL MAXACN$
              DO
                BEGIN
                LINDX = LPT$IDX[ACN2];
                IF LPT$A2A[ACN2] AND
                   TADUSER EQ LPT$OUSER[ACN2] AND
                   LPT$OUSER[ACN2] NQ "        "
                THEN
                  BEGIN
                  GETEBA(LINDX,HADR);
                  TADR = HADR + 1;
                  LPT$ABN[ACN2] = (LPT$ABN[ACN2] + 1) LAN O"777777";
                  P<ABH$> = HADR;          # SET *ABH* POINTER #
                  ABHWRD  = 0;             # CLEAR HEADER WORD #
                  ABHABT  = BLKTYPE"MSGBLK"; # SET BLOCK TYPE #
                  ABHADR  = ACN2;          # SET CONNECTION NUMBER #
                  ABHABN  = LPT$ABN[ACN2]; # SET BLOCK NUMBER #
                  ABHACT  = CT60XP$;       # SET CHARACTER TYPE #
                  ABHTLC  = TEXTL;         # SET TEXT LENGTH #
                  MOVEI(TEXTL,LOC(ULTA[0]),TADR); # SET TEXT AREA #
                  A2ASENT = A2ASENT + 1;
                  END
                END
              IF A2ASENT EQ 0 AND CFRACN NQ 0
              THEN
                BEGIN
                LINDX = LPT$IDX[CFRACN];
                GETEBA(LINDX,HADR);
                IF HADR NQ 0
                THEN
                  BEGIN
                  TADR = HADR + 1;
                  LPT$ABN[CFRACN] =
                    (LPT$ABN[CFRACN] + 1) LAN O"777777";
                  P<ABH$> = HADR;          # SET *ABH* POINTER #
                  ABHWRD  = 0;             # CLEAR HEADER WORD #
                  ABHABT  = BLKTYPE"MSGBLK"; # SET BLOCK TYPE #
                  ABHADR  = CFRACN;        # SET CONNECTION NUMBER #
                  ABHABN  = LPT$ABN[CFRACN]; # SET BLOCK NUMBER #
                  ABHACT  = CT60XP$;       # SET CHARACTER TYPE #
                  ABHTLC  = TEXTL;         # SET TEXT LENGTH #
                  MOVEI(TEXTL,LOC(ULTA[0]),TADR); # SET TEXT AREA #
                  END
                ELSE
                  BEGIN
                  RSP$CODE = 2; # RESOURCE EXHAUSTION #
                  END
                END
              END
            END
          ELSE                           # MESSAGE FOR REMOTE NODE #
            BEGIN
            GETEQA(QADDR);               # GET EMPTY CMD QUEUE BUFFER #
            IF QADDR EQ 0 OR             # BUFFER UNAVAILABLE #
               CIT$OBI[0] EQ MSGMAX$ - 1 # OUTPUT BUFFER FULL #
            THEN
              BEGIN
              RSP$CODE = 2; # RESOURCE EXHAUSTION #
              END
            ELSE
              BEGIN
              P<CQH$> = QADDR;           # SET *CQH$* POINTER #
              ZFILL(CQH$[0],CQBL$);      # CLEAR COMMAND BUFFER #
              CQH$TXT[0] = TRUE;         # SET TEXT COMMAND FLAG #
              CQH$APO[0] = TRUE;         # SET APP ORIGINATED FLAG #
              CQH$FNN[0] = OWNNODE;      # SET COMMAND ORIGIN NODE #
              CQH$LEN[0] = 80;           # SET COMMAND LENGTH #
              CQH$FNN[0] = TADNODE;      # SET DESTINATION NODE NAME #
              B<0,8>CQH$LO1[0] = X"10";  # SET FOR ALL CONSOLES #
              B<16,8>CQH$LO1[0] = X"01";
              CQH$USR[0] = TADUSER;      # SET DEST USER ID # 
              CQH$FLB[0] = X"20";        # SET FLAG BYTE FOR USER #
              P<COUT$> = COUTP$;         # SET *COUT$* POINTER #
              C<0,8>COUT$TXT[CIT$OBI[0]] = TAOUSER; # SET SENDER #
              C<8,TATEXTL>COUT$TXT[CIT$OBI[0]] = TATEXT;
              COUT$LEN[CIT$OBI[0]] = TATEXTL + 8;
              CIT$OBI[0] = CIT$OBI[0] + 1; # MOVE OUTPUT POINTER # 
              CQH$PRI[0] = X"77";        # SET PRIORITY #
              CQH$TYP[0] = X"08";        # HAS SENDING USER ID #
              END
            END
          END
        ELSE IF TACMD EQ "C"         # SEND COMMAND #
        THEN
          BEGIN
          IF TADNODE NQ OWNNODE          # MESSAGE FOR REMOTE NODE #
          THEN
            BEGIN
            GETEQA(QADDR);               # GET EMPTY CMD QUEUE BUFFER #
            IF QADDR EQ 0 OR             # BUFFER UNAVAILABLE #
               CIT$OBI[0] EQ MSGMAX$ - 1 # OUTPUT BUFFER FULL #
            THEN
              BEGIN
              RSP$CODE = 2; # RESOURCE EXHAUSTION #
              END
            ELSE
              BEGIN
              P<CQH$> = QADDR;           # SET *CQH$* POINTER #
              ZFILL(CQH$[0],CQBL$);      # CLEAR COMMAND BUFFER #
              CQH$TXT[0] = TRUE;         # SET TEXT COMMAND FLAG #
              CQH$APO[0] = TRUE;         # SET APP ORIGINATED FLAG #
              CQH$LEN[0] = TATEXTL;      # SET COMMAND LENGTH #
              CQH$FNN[0] = TADNODE;      # SET DESTINATION NODE NAME #
              CQH$USR[0] = TAOUSER;      # SET ORIGIN USER ID #
              CQH$FLB[0] = X"A0";        # SET FLAG BYTE #
              P<COUT$> = COUTP$;         # SET *COUT$* POINTER #
              C<0,TATEXTL>COUT$TXT[CIT$OBI[0]] = TATEXT;
              COUT$LEN[CIT$OBI[0]] = TATEXTL;
              CIT$OBI[0] = CIT$OBI[0] + 1; # MOVE OUTPUT POINTER #
              CQH$TYP[0] = X"00";        # SET PRIORITY #
              CQH$PRI[0] = X"77";        # SET PRIORITY #
              END
            END
          ELSE
            BEGIN
            RSP$CODE = 1; # BAD REQUEST - COMMAND FOR THIS NODE #
            END
          END
        ELSE IF TACMD NQ "I"  # UNRECOGNIZED REQUEST #
        THEN
          BEGIN
          RSP$CODE = 1; # BAD REQUEST #
          END

        #
        * RETURN STATUS TO CALLER
        #
        LINDX = LPT$IDX[ACN];
        GETEBA(LINDX,HADR);
        IF HADR NQ 0
        THEN
          BEGIN
          TADR    = HADR + 1;
          LPT$ABN[ACN] = (LPT$ABN[ACN] + 1) LAN O"777777";
          P<ABH$> = HADR;            # SET *ABH* POINTER #
          ABHWRD  = 0;               # CLEAR HEADER WORD #
          ABHABT  = BLKTYPE"MSGBLK"; # SET BLOCK TYPE #
          ABHADR  = ACN;             # SET CONNECTION NUMBER #
          ABHABN  = LPT$ABN[ACN];    # SET BLOCK NUMBER #
          ABHACT  = CT60XP$;         # SET CHARACTER TYPE #
          ABHTLC  = 1;               # SET TEXT LENGTH #
          MOVEI(2,LOC(A2ARSP[0]),TADR); # SET TEXT AREA #
          END

        END  # A-A INPUT LOOP #
*/
*DECK UDPMDP
*I 26
        PROC GETEBA;                 # GET EMPTY BLOCK BUFFER #
        PROC MOVEI;                  # MOVE DATA BLOCK INDIRECT #
*I 38
*CALLC COMYABH
*I 40
*CALLC COMYLIT
*CALLC COMYMSC
*I NJ24000.11
      ITEM A2ASENT      U;
      ITEM ACN          U;
      ITEM FNODE        C(10);       # FROM NODE #
      ITEM FUSER        C(10);       # FROM USER #
      ITEM HADR         U;           # HEADER ADDRESS #
      ITEM LINDX        U;           # *LIT* INDEX #
      ITEM SPOS         I;           # SAVED START POSITION #
      ITEM TADR         U;           # TEXT AREA ADDRESS #
      ITEM TEXTL        U;           # TEXT LENGTH #
      ITEM TUSER        C(10);       # TO USER #

      ARRAY MSGBUF [0:19] U;;        # DOWNLINE MESSAGE BUFFER #
*I 57
      FNODE = " ";
      FUSER = " ";
      TUSER = " ";

*D NJ24000.16
      CVTESN(FADDR,8,FNODE);         # CONVERT FROM NODE NAME #
*D NJ24000.18
      C<1,8>CMDTXT = FNODE;          # INSERT FROM NODE NAME #
*D NJ24000.34,NJ24000.35
        CVTESN(FADDR,8,TUSER);       # CONVERT TSO/VM ID #
        C<TPOS,9>CMDTXT = TUSER;     # INSERT USER ID IN MESSAGE #
*D NJ24000.44
        FPOS = FPOS + 8;             # SKIP TIME STAMP #
        EPOS = EPOS + 8;             # LEN DOESNT INCLUDE TIME STAMP #
*I NJ24000.45

      IF SBA$TXT[BCMDTYP$] LAN X"08" NQ 0
      THEN                           # FROM USER EXISTS #
        BEGIN
        FADDR = LOC(SBA$TXT[FPOS]);
        CVTESN(FADDR,8,FUSER);       # CONVERT FROM USER #
        FPOS = FPOS + 8;
        END
*/ DON'T SKIP BLANKS OR IGNORE NULL MESSAGES
*D NJ24000.47,NJ24000.68
*I NJ24000.69
      SPOS = TPOS;

*I NJ24000.76

      IF TUSER NQ "          "
      THEN
        BEGIN
        A2ASENT = 0;
        SLOWFOR ACN = MINACN$ STEP 1
          UNTIL MAXACN$
        DO
          BEGIN
          IF LPT$A2A[ACN] AND
             C<0,8>TUSER EQ LPT$OUSER[ACN] AND
             LPT$OUSER[ACN] NQ "        "
          THEN
            BEGIN
            LINDX = LPT$IDX[ACN];
            GETEBA(LINDX,HADR);
            IF HADR NQ 0
            THEN
              BEGIN
              TADR    = HADR + 1;
              ZFILL(MSGBUF[0],20);
              P<UTA$> = LOC(MSGBUF[0]);
              TACMD   = "M";
              TAOUSER = FUSER;
              TADUSER = TUSER;
              TADNODE = FNODE;
              TATEXTL = TPOS - SPOS;
              C<0,TATEXTL>TATEXT = C<SPOS,TATEXTL>CMDTXT;
              LPT$ABN[ACN] = (LPT$ABN[ACN] + 1) LAN O"777777";
              P<ABH$> = HADR;               # SET *ABH* POINTER #
              ABHWRD  = 0;                  # CLEAR HEADER WORD #
              ABHABT  = BLKTYPE"MSGBLK";    # SET BLOCK TYPE #
              ABHADR  = ACN;                # SET CONNECTION NUMBER #
              ABHABN  = LPT$ABN[ACN];       # SET BLOCK NUMBER #
              ABHACT  = CT60XP$;            # SET CHARACTER TYPE #
              TEXTL   = ((TATEXTL+9)/10)+3; # CALCULATE TEXT LENGTH #
              ABHTLC  = TEXTL;              # SET TEXT LENGTH #
              MOVEI(TEXTL,LOC(MSGBUF[0]),TADR); # SET TEXT AREA #
              A2ASENT = A2ASENT + 1;
              END
            END
          END
        IF A2ASENT EQ 0 AND CFRACN NQ 0
        THEN
          BEGIN
          LINDX = LPT$IDX[CFRACN];
          GETEBA(LINDX,HADR);
          IF HADR NQ 0
          THEN
            BEGIN
            TADR = HADR + 1;
            ZFILL(MSGBUF[0],20);
            P<UTA$> = LOC(MSGBUF[0]);
            TACMD   = "M";
            TAOUSER = FUSER;
            TADUSER = TUSER;
            TADNODE = FNODE;
            TATEXTL = TPOS - SPOS;
            C<0,TATEXTL>TATEXT = C<SPOS,TATEXTL>CMDTXT;
            LPT$ABN[CFRACN] = (LPT$ABN[CFRACN] + 1) LAN O"777777";
            P<ABH$> = HADR;             # SET *ABH* POINTER #
            ABHWRD  = 0;                  # CLEAR HEADER WORD #
            ABHABT  = BLKTYPE"MSGBLK";    # SET BLOCK TYPE #
            ABHADR  = CFRACN;             # SET CONNECTION NUMBER #
            ABHABN  = LPT$ABN[CFRACN];    # SET BLOCK NUMBER #
            ABHACT  = CT60XP$;            # SET CHARACTER TYPE #
            TEXTL   = ((TATEXTL+9)/10)+3; # CALCULATE TEXT LENGTH #
            ABHTLC  = TEXTL;              # SET TEXT LENGTH #
            MOVEI(TEXTL,LOC(MSGBUF[0]),TADR); # SET TEXT AREA #
            END
          END

        IF A2ASENT GR 0 OR CFRACN NQ 0
        THEN
          BEGIN
          RETURN; # AVOID DAYFILE AND K-DISPLAY #
          END
        END
*/
*/      END OF MODSET
