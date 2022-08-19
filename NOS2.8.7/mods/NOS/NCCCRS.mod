NCCCRS
*IDENT NCCCRS  KEJ.
*/
*/ THIS MODSET ADDS *CRS*, THE CRAY STATION SUBSYSTEM, TO NOS 2.8.7.
*/
*DECK PPCOM
*I NS22000.907
          EJECT
CZ        SPACE  4,10
**        EST ENTRY FORMAT - CRAY STATION COUPLER *CZ*.


          LOC    0
 EQDE     BSS    0           EQUIPMENT DESCRIPTOR WORD

*         FLAGS.

          VFD    1/0         NON-MASS STORAGE DEVICE
          VFD    1/1         ALLOCATABLE DEVICE
          VFD    8/0         RESERVED FOR CDC
          VFD    2/0         DEVICE STATUS

*         CHANNEL 1 DATA.

          VFD    1/1         ACCESS PATH IS ENABLED
          VFD    2/0         CHANNEL STATE (UP)
          VFD    3/0         RESERVED FOR CDC
          VFD    1/0         CONCURRENCY FLAG (NOT CONCURRENT CHANNEL)
          VFD    5/0         CHANNEL NUMBER

*         CHANNEL 2 DATA.

          VFD    12/0        RESERVED FOR CDC

*         DEVICE MNEMONIC.

          VFD    12/2RCZ     DEVICE MNEMONIC

*         HARDWARE/SOFTWARE CHARACTERISTICS.

          VFD    12/0        =0 IF CRAY FEI, <>0 IF NSC HYPERCHANNEL

 EQAE     BSS    0           EQUIPMENT ASSIGNMENT WORD

*         INSTALLATION AREA.

          VFD    12/0        INSTALLATION AREA

*         RESERVED FOR CDC.

          VFD    12/0        RESERVED FOR CDC

*         DEVICE DEPENDENT.

          VFD    18/0        RESERVED FOR CDC

*         ACCESS LEVELS.

          VFD    3/0         LOWER ACCESS LEVEL
          VFD    3/0         UPPER ACCESS LEVEL

*         EJT ORDINAL.

          VFD    12/0        EJT ORDINAL OF OWNING JOB

 ESTE     BSS    0           EST ENTRY SIZE
          LOC    *O
*I 274L797.119
          VFD    12/0        CRS
*EDIT PPCOM
*DECK COMSPRD
*I 274L797.1
 C1CS     EQU    72          CRS
*EDIT COMSPRD
*DECK COMSSSD
*I 274L797.1
          SSID   C1SI        CRS
*I 274L797.10
          SUBSYST  CRS,C1SI,C1CS,,AUTO,,,YES,YES
*EDIT COMSSSD
*DECK COMUCPD
*I 274L797.1
          SMGT   (*CRS *)
*EDIT COMUCPD
*DECK DSD 
*I NS2746.1
 CRS      ENTER  (CRS@),,SDF
*/        END OF MODSET.
