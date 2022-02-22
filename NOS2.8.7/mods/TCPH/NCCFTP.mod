*IDENT NCCFTP  KEJ.
*/
*/  THIS MODSET ADDS *CWD* AND *PWD* COMMANDS TO THE NOS FTP SERVER
*/  TO IMPROVE INTEROPERABILITY WITH COMMONLY USED FTP CLIENTS.
*/
*DELETE FFSIBCT.423 
*     CWD /
*DELETE FFSIBCT.425 
      FFS2ICT (CWD,CWD$CMD,SUPPORTED,NOSPF);
        FFS2IPT ( 0 ,ALNUM07,REQUIRED, 0, 0 );
*DELETE FFSIBCT.514 
*     PWD 
*DELETE FFSIBCT.516 
      FFS2ICT (PWD,PWD$CMD,SUPPORTED,NOTNOSPF);
*INSERT TCH0103.246 
            FTE$CWD,                   # CWD CMD                    50 #
            FTE$PWD,                   # PWD CMD                    51 #
*INSERT TCH0103.247 
            FTA$CWD,                   # PROCESS CWD COMMAND           #
            FTA$PWD,                   # PROCESS PWD COMMAND           #
*DELETE TCH0103.245 
      DEF FTP$MX$EVT   #51#;       # MAXIMUM TRIGGER TO FTP STATE TABLE#
*DELETE TCH0103.279 
            # WDCE,    SITE  # S"FTA$I503" ]
  
          [ # IDLE,    CWD   # S"FTA$ABORT",
            # WTUV,    CWD   # S"FTA$I530",
            # URWP,    CWD   # S"FTA$I530",
            # 2UWP,    CWD   # S"FTA$I530",
            # WDNC,    CWD   # S"FTA$ABORT",
            # UVWA,    CWD   # S"FTA$I530",
            # UVWC,    CWD   # S"FTA$ABORT",
            # UVWF,    CWD   # S"FTA$CWD",
            # WPCE,    CWD   # S"FTA$I503",
            # WPCS,    CWD   # S"FTA$CWD",
            # WCCP,    CWD   # S"FTA$I503",
            # WCCC,    CWD   # S"FTA$NOOP",
            # XFER,    CWD   # S"FTA$I503",
            # DXQP,    CWD   # S"FTA$I503",
            # WPCP,    CWD   # S"FTA$I503",
            # WDCE,    CWD   # S"FTA$I503" ]
  
          [ # IDLE,    PWD   # S"FTA$ABORT",
            # WTUV,    PWD   # S"FTA$I530",
            # URWP,    PWD   # S"FTA$I530",
            # 2UWP,    PWD   # S"FTA$I530",
            # WDNC,    PWD   # S"FTA$ABORT",
            # UVWA,    PWD   # S"FTA$I530",
            # UVWC,    PWD   # S"FTA$ABORT",
            # UVWF,    PWD   # S"FTA$PWD",
            # WPCE,    PWD   # S"FTA$I503",
            # WPCS,    PWD   # S"FTA$PWD",
            # WCCP,    PWD   # S"FTA$I503",
            # WCCC,    PWD   # S"FTA$NOOP",
            # XFER,    PWD   # S"FTA$I503",
            # DXQP,    PWD   # S"FTA$I503",
            # WPCP,    PWD   # S"FTA$I503",
            # WDCE,    PWD   # S"FTA$I503" ]];
*DELETE TCH0103.297 
            # WDCE,    SITE  # S"FTS$WDCE" ]
  
          [ # IDLE,    CWD   # S"FTS$IDLE",
            # WTUV,    CWD   # S"FTS$WTUV",
            # URWP,    CWD   # S"FTS$URWP",
            # 2UWP,    CWD   # S"FTS$2UWP",
            # WDNC,    CWD   # S"FTS$IDLE",
            # UVWA,    CWD   # S"FTS$UVWA",
            # UVWC,    CWD   # S"FTS$IDLE",
            # UVWF,    CWD   # S"FTS$UVWF",
            # WPCE,    CWD   # S"FTS$WPCE",
            # WPCS,    CWD   # S"FTS$WPCS",
            # WCCP,    CWD   # S"FTS$WCCP",
            # WCCC,    CWD   # S"FTS$WCCC",
            # XFER,    CWD   # S"FTS$XFER",
            # DXQP,    CWD   # S"FTS$DXQP",
            # WPCP,    CWD   # S"FTS$WPCP",
            # WDCE,    CWD   # S"FTS$WDCE" ]
  
          [ # IDLE,    PWD   # S"FTS$IDLE",
            # WTUV,    PWD   # S"FTS$WTUV",
            # URWP,    PWD   # S"FTS$URWP",
            # 2UWP,    PWD   # S"FTS$2UWP",
            # WDNC,    PWD   # S"FTS$IDLE",
            # UVWA,    PWD   # S"FTS$UVWA",
            # UVWC,    PWD   # S"FTS$IDLE",
            # UVWF,    PWD   # S"FTS$UVWF",
            # WPCE,    PWD   # S"FTS$WPCE",
            # WPCS,    PWD   # S"FTS$WPCS",
            # WCCP,    PWD   # S"FTS$WCCP",
            # WCCC,    PWD   # S"FTS$WCCC",
            # XFER,    PWD   # S"FTS$XFER",
            # DXQP,    PWD   # S"FTS$DXQP",
            # WPCP,    PWD   # S"FTS$WPCP",
            # WDCE,    PWD   # S"FTS$WDCE" ]];
*DELETE FFSLFLH.1637
           # CWD$CMD     # FTE$CDS"FTE$CWD",
*DELETE FFSLFLH.1650
           # PWD$CMD     # FTE$CDS"FTE$PWD",
*INSERT TCH0103.299 
           A$CWD   : FTA$CWD,          # PROCESS CWD COMMAND           #
           A$PWD   : FTA$PWD,          # PROCESS PWD COMMAND           #
*INSERT TCH0103.321 
A$CWD: 
#
*     PROCESS THE CWD COMMAND 
#
        IF FFSUPCP
        THEN
          BEGIN
          IF PAR$VALUE[1] EQ "/"
          THEN
            BEGIN
            FFSUSRM(RSPCT, MSG$200, NOPFM$);  # NORMAL 200             #
            END
          ELSE
            BEGIN
            FFSUSRM(RSPCT, MSG$431, NOPFM$);  # ABNORMAL 431           #
            END
          END
        ELSE
          BEGIN
          FFSUSRM(RSPCT, MSG$501, NOPFM$);   # ABNORMAL 501            #
          END
  
        GOTO A$END; 
  
A$PWD: 
#
*     PROCESS THE PWD COMMAND 
#
        FFSUSRM(RSPCT, MSG$257, NOPFM$);  # 257 "/" CURRENT CATALOG  #
  
        GOTO A$END; 
  
*DELETE FFSLFLH.1932
         CMD$END,                        # CWD  - TRIGGER FROM CMD$MAP #
*DELETE FFSLFLH.1945
         CMD$END,                        # PWD  - TRIGGER FROM CMD$MAP #
*DELETE FFSIIPV.136 
        "257 ""/"" CURRENT CATALOG.                                 ";
*DELETE TCH0136.9
      IF (FTPSTATE EQ FTS$CDS"FTS$UVWF" 
          OR FTPSTATE EQ FTS$CDS"FTS$WPCS"
          OR EPTFLAG EQ FTPC$)
*DELETE TCH0136.15
      IF (FTPSTATE EQ FTS$CDS"FTS$UVWF" 
          OR FTPSTATE EQ FTS$CDS"FTS$WPCS"
          OR EPTFLAG EQ FTPC$)
*DELETE TCH0136.21
      IF (FTPSTATE EQ FTS$CDS"FTS$UVWF" 
          OR FTPSTATE EQ FTS$CDS"FTS$WPCS"
          OR EPTFLAG EQ FTPC$)
*DELETE TCH0061.12
      DEF FTPNMSG$   # 44 # ;          # NUMBER OF RESPONSE MESSAGES-1 #
*INSERT TCH0061.26
      DEF MSG$431    #44# ;
*INSERT FFSIIPV.176 
      FTP$MSG[MSG$431] =
        "431 NO SUCH CATALOG.                                     ";
*INSERT FFSUPFC.459 
      IF (TKN$LEN EQ 0) AND (SEPARATOR EQ SLASH$0) # IGNORE LEADING / #
      THEN
        BEGIN
        FFS2GNT;
        END
*DELETE FFSUPFC.465,FFSUPFC.467
        IF (TKN$LEN EQ 0) AND (CMDNUM EQ CWD$CMD) 
        THEN
          BEGIN
          PAR$VALUE[1] = "/"; 
          TKN$20CHAR = "/";
          TKN$LEN = 1;
          END
        ELSE
          BEGIN
          CMDNUM = ERR$501;              # RETURN ERROR CODE,         #
          ERRORCODE = 1;
          RETURN;                        # AND GET OUT OF THIS ROUTINE#
          END
