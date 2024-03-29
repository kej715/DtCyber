SAFFIX
*IDENT SAFFIX   KEJ.  23/02/01
*/
*/  MOD TO CORRECT STORE-AND-FORWARD PROCESSING. WHEN A CDC NODE WAS
*/  AN INTERMEDIATE NODE, AND THE DESTINATION NODE WAS A CDC NODE,
*/  THE DESTINATION NODE WOULD LOSE ROUTING INFO AND CAUSE ERRORS
*/  WHEN CALLING *DSP* TO QUEUE THE RECEIVED FILE.
*/
*DECK DDPNJH
*D 124,134
*DECK UDPDSH
*D 157,158
            UDPUSS;                  # UPDATE USER SYSTEM SECTOR #
*DECK UDPSLR
*D 49
        IF USS$PRN[0] NQ " "               # SET SOURCE *LID* #
        THEN
          BEGIN
          DSP$SLID[0] = GETLID(USS$PRN[0]);
          END
        ELSE
          BEGIN
          DSP$SLID[0] = GETLID(USS$ONN[0]);
          END
*DECK UDPXRF
*D 65
          [12,01,02,03,05,12,07,08,09,10]
