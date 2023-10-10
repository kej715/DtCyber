USERMSG
*IDENT USERMSG   KEJ.  23/07/04
*/
*/  MOD TO IMPLEMENT FEATURE ENABLING OTHER APPLICATIONS TO CONNECT
*/  TO NJF AND SEND/RECEIVE NJE MESSAGES TO/FROM USERS ON OTHER NJE
*/  NODES.
*/
*DECK COMXNJF
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
        ITEM LPT$ABL    U<03>;       # A-A APP BLOCK LIMIT #
        ITEM LPT$OBC    U<03>;       # A-A OUTSTANDING BLOCK COUNT #
        ITEM LPT$RS2    U<05>;       # RESERVED #
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
*D 238
          ELSE IF NOT CQH$APO        # PROCESS CMD TEXT #
          THEN
*/
*DECK CMDOUT
*D 183,184
          C<0,8>TEXTA =C<1,8>TIME;   # SET TIME IN MSG #
          C<8,MSGL>TEXTA =           # MOVE MSG TO TEXTA #
*D 186
          MSGL = MSGL + 8;           # INCREASE MSG LENGTH #
*/
*DECK PRSMSC
*I 58
      CFRACN = 0;                    # CLEAR *CONFER* ACN #
*/
*DECK SMPREQ
*I 29
        PROC MESSAGE;                # ISSUE DAYFILE MESSAGE #
*I 47

      ARRAY A2AMSG [0:0] U;
        BEGIN
        ITEM A2A$TXT    C<20> =
          [
          " AAAAAAA CONNECT",
          ];
        ITEM A2A$ZBT    U<12> = [1(0)];
        ITEM A2A$FL1    C<01,A2A$TXT>;  # FILLER #
        ITEM A2A$ANM    C<07,+>;        # APPLICATION NAME #
        END
*I 72

      ACN = CONACN[0];               # EXTRACT CONNECTTION NUMBER #

      IF CONDT[0] EQ 5 OR CONDT[0] EQ 6  # A-A CONNECTION REQUEST #
      THEN
        BEGIN
        LPT$IDX[ACN]   = 0;          # SET *LIT* INDEX #
        LPT$NTX[ACN]   = 0;          # CLEAR NEXT-TO-TRANSMIT INDEX #
        LPT$QTX[ACN]   = 0;          # CLEAR QUEUE TABLE INDEX #
        LPT$CFG[ACN]   = FALSE;      # CLEAR TERMINAL CONFIG FLAG #
        LPT$NXT[ACN]   = FALSE;      # CLEAR NEXT-TO-TRANSMIT FLAG #
        LPT$ANM[ACN]   = CONTNM[0];  # SET A-A PEER NAME #
        LPT$ABN[ACN]   = 0;          # INIT APP BLOCK NUMBER #
        LPT$ABL[ACN]   = CONABL[0];  # INIT APP BLOCK LIMIT #
        LPT$OBC[ACN]   = 0;          # INIT OUTSTANDING BLOCK COUNT #
        LPT$OUSER[ACN] = "        "; # SET A-A USER ID #
        LPT$A2A[ACN]   = TRUE;       # SET A2A FLAG #
        CONCNT = CONCNT + 1;         # INCREMENT CONNECTION COUNT #
        RCODE = S"NORMAL";           # SET REPSONSE CODE #
        SMPRSP(RTYPE,RCODE);         # SEND NORMAL REPLY #
        IF LPT$ANM[ACN] EQ "CONFER"
        THEN
          BEGIN
          CFRACN = ACN;
          END
        A2A$ANM[0] = LPT$ANM[ACN];   # LOG CONNECTION #
        MESSAGE(A2AMSG[0],USRDF$);
        RETURN;                      # EXIT #
        END
*D 78,79
*I 104
          LPT$A2A[ACN] = FALSE;      # CLEAR A2A FLAG #
*DECK SMPPSM
*I 33
        PROC NETPUT;
        PROC SMPRSP;
*I 43
*CALLC COMYABH
*I 44
*CALLC COMYMSC
*I 51
      ITEM RCODE        S:RSPCODE;   # RESPONSE CODE #
      ITEM RTYPE        S:RSPTYPE;   # RESPONSE TYPE #
*I 63

      ARRAY A2AMSG [0:0] U;
        BEGIN
        ITEM A2A$TXT    C<20> =
          [
          " AAAAAAA DISCONNECT",
          ];
        ITEM A2A$ZBT    U<12> = [1(0)];
        ITEM A2A$FL1    C<01,A2A$TXT>;  # FILLER #
        ITEM A2A$ANM    C<07,+>;        # APPLICATION NAME #
        END
*D 104,105
          IF LPT$A2A[ACN]
          THEN
            BEGIN
            IF PFCSFC EQ FCACK
            THEN
              BEGIN
              IF LPT$OBC[ACN] GR 0
              THEN
                BEGIN
                LPT$OBC[ACN] = LPT$OBC[ACN] - 1;
                END
              END
            ELSE IF PFCSFC EQ FCINIT
            THEN
              BEGIN
              RTYPE = S"INITIALIZE";     # SET RESPONSE TYPE #
              RCODE = S"NORMAL";         # SET RESPONSE CODE #
              SMPRSP(RTYPE,RCODE);       # SEND NORMAL RESPONSE #
              END
            ELSE IF PFCSFC EQ CONENDN
            THEN
              BEGIN
              LPT$A2A[ACN]   = FALSE;    # CLEAR A-A FLAG AND USER  #
              LPT$OUSER[ACN] = "        "; 
              CONCNT = CONCNT - 1;       # DECREMENT CONNECTION COUNT #
              IF LPT$ANM[ACN] EQ "CONFER"
              THEN
                BEGIN
                CFRACN = 0;
                END
              A2A$ANM[0] = LPT$ANM[ACN]; # LOG DISCONNECTION #
              MESSAGE(A2AMSG[0],USRDF$);
              END
            ELSE IF PFCSFC EQ CONCB
            THEN
              BEGIN
              LPT$OUSER[ACN] = "        "; 
              P<ABH$> = LOC(SMHA[0]);    # SET *ABH* POINTER #
              ABHWRD[0] = 0;             # CLEAR HEADER WORD #
              ABHABT[0] = BLKTYPE"SUPMSG"; # SET BLOCK TYPE #
              ABHACT[0] = CT60XP$;       # SET CHARACTER TYPE #
              ABHTLC[0] = 2;             # SET TEXT LENGTH #
          #
          *   FORMAT TEXT AREA.
          #
              P<SM$> = LOC(SMTA[0]);     # SET *SM* POINTER #
              P<CON$> = P<SM$>;          # SET *CON* POINTER #
              SPMSG0[0] = 0;             # CLEAR TEXT AREA WORD 0 #
              SPMSG1[0] = 0;             # CLEAR TEXT AREA WORD 1 #
              PFCSFC[0] = CONEND;        # SET FUNCTION CODE #
              CONACN[0] = ACN;           # SET CONNECTION NUMBER #
          #
          *   SEND SUPERVISORY MESSAGE.
          #
              NETPUT(SMHA[0],SMTA[0]);   # SEND BLOCK TO *NIP* #
              END
            END
          ELSE
            BEGIN
            LINDX = LPT$IDX[ACN];        # SET *LIT* INDEX #
            SMPXSF(CASE,LINDX);          # EXECUTE *SM* FUNCTION #
            END

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
        PROC BLDABH;                 # BUILD APP BLOCK HEADER #
        PROC GETEQA;                 # GET EMPTY CMD QUEUE BUFFER #
*I 24
        PROC MESSAGE;                # ISSUE DAYFILE MESSAGE #
*I 25
        PROC NETPUT;                 # SEND BLOCK TO *NIP* #
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
*I 44
      ITEM TEXTL        U;           # MESSAGE TEXT LENGTH #
      ITEM QADDR        I;           # COMMAND QUEUE ADDRESS #
*I 45

      ARRAY A2ARSP [0:0] U;
        BEGIN
        ITEM RSP$TYPE   C<01>;          # RESPONSE TYPE #
        ITEM RSP$RSVD   U<48>;          # RESERVED      #
        ITEM RSP$CODE   U<06>;          # RESPONSE CODE #
        END

      ARRAY DLHA [0:0]    U;;        # APPLICATION BLOCK HEADER #
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
              IF LPT$A2A[ACN2] AND
                 TADUSER EQ LPT$OUSER[ACN2] AND
                 LPT$OUSER[ACN2] NQ "        " AND
                 LPT$OBC[ACN2] GQ LPT$ABL[ACN2]
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
                IF LPT$A2A[ACN2] AND
                   TADUSER EQ LPT$OUSER[ACN2] AND
                   LPT$OUSER[ACN2] NQ "        "
                THEN
                  BEGIN
                  LPT$ABN[ACN2] = (LPT$ABN[ACN2] + 1) LAN O"777777";
                  P<ABH$> = LOC(DLHA[0]);  # SET *ABH* POINTER #
                  ABHWRD  = 0;             # CLEAR HEADER WORD #
                  ABHABT  = BLKTYPE"MSGBLK"; # SET BLOCK TYPE #
                  ABHADR  = ACN2;          # SET CONNECTION NUMBER #
                  ABHABN  = LPT$ABN[ACN2]; # SET BLOCK NUMBER #
                  ABHACT  = CT60XP$;       # SET CHARACTER TYPE #
                  ABHTLC  = TEXTL;         # SET TEXT LENGTH #
                  NETPUT(DLHA[0],ULTA[0]);
                  LPT$OBC[ACN2] = LPT$OBC[ACN2] + 1;
                  A2ASENT = A2ASENT + 1;
                  END
                END
              IF A2ASENT EQ 0 AND CFRACN NQ 0
              THEN
                BEGIN
                IF LPT$OBC[CFRACN] LS LPT$ABL[CFRACN]
                THEN
                  BEGIN
                  LPT$ABN[CFRACN] =
                    (LPT$ABN[CFRACN] + 1) LAN O"777777";
                  P<ABH$> = LOC(DLHA[0]);  # SET *ABH* POINTER #
                  ABHWRD  = 0;             # CLEAR HEADER WORD #
                  ABHABT  = BLKTYPE"MSGBLK"; # SET BLOCK TYPE #
                  ABHADR  = CFRACN;        # SET CONNECTION NUMBER #
                  ABHABN  = LPT$ABN[CFRACN]; # SET BLOCK NUMBER #
                  ABHACT  = CT60XP$;       # SET CHARACTER TYPE #
                  ABHTLC  = TEXTL;         # SET TEXT LENGTH #
                  NETPUT(DLHA[0],ULTA[0]);
                  LPT$OBC[CFRACN] = LPT$OBC[CFRACN] + 1;
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
        ELSE IF TACMD NQ "I"  # UNRECOGNIZED REQUEST #
        THEN
          BEGIN
          RSP$CODE = 1; # BAD REQUEST #
          END

        LPT$ABN[ACN] = (LPT$ABN[ACN] + 1) LAN O"777777";
        P<ABH$> = LOC(DLHA[0]);          # SET *ABH* POINTER #
        ABHWRD  = 0;                     # CLEAR HEADER WORD #
        ABHABT  = BLKTYPE"MSGBLK";       # SET BLOCK TYPE #
        ABHADR  = ACN;                   # SET CONNECTION NUMBER #
        ABHABN  = LPT$ABN[ACN];          # SET BLOCK NUMBER #
        ABHACT  = CT60XP$;               # SET CHARACTER TYPE #
        ABHTLC  = 1;                     # SET TEXT LENGTH #
        NETPUT(DLHA[0],A2ARSP[0]);
        LPT$OBC[ACN] = LPT$OBC[ACN] + 1;

        END  # A-A INPUT LOOP #
*/
*DECK UDPMDP
*I 29
        PROC NETPUT;                 # SEND BLOCK TO *NIP* #
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
      ITEM SPOS         I;           # SAVED START POSITION #
      ITEM TUSER        C(10);       # TO USER #

      ARRAY DLHA [0:0]  U;;          # APPLICATION BLOCK HEADER #
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
        CVTESN(FADDR,8,FUSER);       # CONVERT FRON USER #
        FPOS = FPOS + 8;
        END
*D NJ24000.48
        WHILE BLANKS AND FPOS LS EPOS
*/ DON'T IGNORE NULL MESSAGES
*D NJ24000.63,NJ24000.68
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
            IF LPT$OBC[ACN] LS LPT$ABL[ACN]
            THEN
              BEGIN
              ZFILL(MSGBUF[0],20);
              P<UTA$> = LOC(MSGBUF[0]);
              TACMD   = "M";
              TAOUSER = FUSER;
              TADUSER = TUSER;
              TADNODE = FNODE;
              TATEXTL = TPOS - SPOS;
              C<0,TATEXTL>TATEXT = C<SPOS,TATEXTL>CMDTXT;
              LPT$ABN[ACN] = (LPT$ABN[ACN] + 1) LAN O"777777";
              P<ABH$> = LOC(DLHA[0]);       # SET *ABH* POINTER #
              ABHWRD  = 0;                  # CLEAR HEADER WORD #
              ABHABT  = BLKTYPE"MSGBLK";    # SET BLOCK TYPE #
              ABHADR  = ACN;                # SET CONNECTION NUMBER #
              ABHABN  = LPT$ABN[ACN];       # SET BLOCK NUMBER #
              ABHACT  = CT60XP$;            # SET CHARACTER TYPE #
              ABHTLC  = ((TATEXTL+9)/10)+3; # SET TEXT LENGTH #
              NETPUT(DLHA[0],MSGBUF[0]);
              LPT$OBC[ACN] = LPT$OBC[ACN] + 1;
              A2ASENT = A2ASENT + 1;
              END
            END
          END
        IF A2ASENT EQ 0 AND
           CFRACN NQ 0 AND
           LPT$OBC[CFRACN] LS LPT$ABL[CFRACN]
        THEN
          BEGIN
          ZFILL(MSGBUF[0],20);
          P<UTA$> = LOC(MSGBUF[0]);
          TACMD   = "M";
          TAOUSER = FUSER;
          TADUSER = TUSER;
          TADNODE = FNODE;
          TATEXTL = TPOS - SPOS;
          C<0,TATEXTL>TATEXT = C<SPOS,TATEXTL>CMDTXT;
          LPT$ABN[CFRACN] = (LPT$ABN[CFRACN] + 1) LAN O"777777";
          P<ABH$> = LOC(DLHA[0]);       # SET *ABH* POINTER #
          ABHWRD  = 0;                  # CLEAR HEADER WORD #
          ABHABT  = BLKTYPE"MSGBLK";    # SET BLOCK TYPE #
          ABHADR  = CFRACN;             # SET CONNECTION NUMBER #
          ABHABN  = LPT$ABN[CFRACN];    # SET BLOCK NUMBER #
          ABHACT  = CT60XP$;            # SET CHARACTER TYPE #
          ABHTLC  = ((TATEXTL+9)/10)+3; # SET TEXT LENGTH #
          NETPUT(DLHA[0],MSGBUF[0]);
          LPT$OBC[CFRACN] = LPT$OBC[CFRACN] + 1;
          END
        END
*/
*/      END OF MODSET
