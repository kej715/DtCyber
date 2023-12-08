CCI emulation for DtCyber
=========================


Index
-----

* [Description](#description)
* [Features](#features)
* [Build](#build)
* [Configuration](#configuration)
* [Theory of Operation](#theory-of-operation)
* [Installation](#installation)
* [Compatibility](#compatibility)
* [References](#references)
* [Acknowledgements](#acknowledgements)


Description
-----------
This is a first implementation of a 255X Host Communication Processor (HCP) running the Communication
Control Intercom (CCI) version 3 software.


Features
--------
At the moment, the CCI emulation only supports asynchronous terminals on switched lines. As opposed to RDF,
multiple INTERCOM sessions can be opened. However, INTERCOM does not allow  a user to log in to more than one terminal at the same time.

A Telnet client is needed to connect to the CCI emulation.


Build
-----
Check out the "cci-aync" branch of this repository and build DtCyber "as usual".


Configuration
-------------
In cyber.ini:

     HCP,<eq>,<un>,<ch>

Example: configure the CCI HCP on channel 7

     HCP,0,0,7

NOS/BE CMR equipment table configuration :

     FE,CH=<ch>,<EQP>=<eq>

Example: configure the CCI HCP as equipment 30 in the CMR equipment table

     FE,CH=7,ESTO=30

The DtCyber Telnet network access is configured in the npuConnections section of the cyber.ini file.

Example: allow up to 8 Telnet connections on port 6610, using CLA ports 1-8

     [npu.nosbe]
     couplerNode=0
     npuNode=2
     terminals=6610,0x01,8,telnet

Note: setting couplerNode to zero and npuNode to two is mandatory for the CCI emulation.

The multiplexer configuration for CCI is set up with the NOS/BE TDFGEN utility (see 
NOS/BE Installation Handbook).

Example input for TDFGEN: configure 8 asynchronous terminals on switched lines using CLA ports 1 - 8

          MUX2550  EST=30,9
          ASYNC  LT=DU,LS=38400,LO=1
          ASYNC  LT=DU,LS=38400,LO=2
          ASYNC  LT=DU,LS=38400,LO=3
          ASYNC  LT=DU,LS=38400,LO=4
          ASYNC  LT=DU,LS=38400,LO=5
          ASYNC  LT=DU,LS=38400,LO=6
          ASYNC  LT=DU,LS=38400,LO=7
          ASYNC  LT=DU,LS=38400,LO=8

Note: Only asynchronous terminals witch switched lines are supported at the moment. Always use the
maximum speed allowed by INTERCOM. Otherweise, the output speed will be throttled.

In a summary, the INTERCOM configuration consists of:

* DtCyber configuration (file: cyber.ini)
* NOS/BE device configuration (CMR equipment table)
* INTERCOM multiplexer configuration (TDFGEN utility)

All configurations must be consistent.


Theory of Operation
-------------------
There are considerable differences in the functionality of CCI and CCP.

During initialization INTERCOM loads CCI software modules into the HCP. These modules must exist in the NOS/BE system libraries. Then the CCI software is started on the HCP and the
lines, which are specified in the INTERCOM multiplexer configuration, are initialized.

The operation of asynchronous terminals on switched lines is shown in the following diagrams.

SM: service message, LS: line status, commands in parentheses are coupler commands

Initialization:

     HOST                                         HCP                                                       Telnet Client
     ------- (OutMemAddr) -------------> load micro program
     ------- (OutProgram) ------------->
             ....
     ------- (Start) ------------------> run micro program
     <------ (Idle) -------------------- (micro memory loader)
     ------- (OutMemAddr) -------------> load macro program
     ------- (OutProgram) -------------> 
             ....
     ------- (Start) ------------------> run macro program
     <------ Initialize SM ------------- (CCI executable)
     
     for each line specified in the multiplexer configuration:
     ------- Configure Line SM --------> allocate Line Control Block
                                         set line disabled flag
                                         LS: inoperative
     <------ Line Configured SM --------
     
     ------- Enable Line SM -----------> LS: inoperative, wait for ring
     <------ Line Enabled SM -----------
     
     
Telnet Connect:

     HOST                                         HCP                                                       Telnet Client
                                        process connection request,            <----- connect to port ---   Telnet <hostname> <port>
                                        find free CLA port,set wait flag,
                                        do not use CLA ports of disabled
                                        lines
     <----- line status SM ------------ LS: operational
     ------ configure terminal SM ----> configure terminal, clear                                   
                                        CLA port wait flag and                 <----- Telnet negotiation --
                                        begin handle network traffic           ------ Telnet negotiation ->
                                                                               ....
    <------ terminal configured SM --- 
    ------- login banner ------------> send downline data                      ------ banner data -------->
    <------ user input   ------------- send upline data                        <----- user input  --------- enter login data
    ------- system Output -----------> send downline data                      ------ system output ------> 
                                                                               ....
                                                                               Note: a LOGOUT does not terminate the connection

Telnet Disconnect:

     HOST                                         HCP                                                       Telnet Client
                                        close network connection              <----- send zero block ---   close session
     <----- line status SM -----------> LS: inoperative, wait for ring
     ------ delete terminal SM -------> remove terminal configuration
     <----- terminal deleted SM -------
     

Operator Command LINEOFF,&lt;est&gt;,&lt;line&gt;, Telnet session active:


     HOST                                         HCP                                                       Telnet Client
     ------ delete terminal SM -------> close network connection                                           close session
     <----- terminal deleted SM ------- remove terminal configuration
                                        LS: inoperative, wait for ring
     ------ disable line SM ----------> Set line disabled flag
     <----- line disable SM ----------- LS: inoperative                                     
                                        
                                        
Operator Command LINEOFF,&lt;est&gt;,&lt;line&gt; Telnet session not active:


     HOST                                         HCP                                                       Telnet Client
     ------ disconnect line SM -------> LS: inoperative, wait for ring
     <----- line disconnected SM ------
     ------ disable line SM ----------> set line disabled flag
     <----- line disable SM ----------- LS: inoperative

Operator Command LINEON,&lt;est&gt;,&lt;line&gt;


     HOST                                         HCP                                                       Telnet Client
     ------- Enable Line SM -----------> clear line disabled flag
     <------ Line Enabled SM ----------- LS: inoperative, wait for ring

                                        
Installation
------------
The cci branch of the [NOSBE712 Repository](https://github.com/bug400/NOSBE712/tree/cci) contains an environment to build and run a NOS/BE 1.5 Level 712 system with CCI and INTERCOM. The deadstart tape generated with this environment also includes faked load modules for the 255X HCP.

See [How to build NOS/BE 1.5 Level 712 from Scratch](https://codex.retro1.org/cdc:nosbe:building_nos_be_level_712_from_scratch) how to proceed. There is no ready to run NOS/BE INTERCOM system at the moment.


Compatibility
-------------
The cci branch of the DtCyber fork was tested successfully (including according to the CONTRIBUTING.md file) on the following platforms:

* Windows 10, VS 2022
* Linux 64 (Debian Bookworm)
* Linux 64 armv8-a (Debian Bookworm)
* MacOS 10.15
* Linux 32 (Debian Bookworm, only RTR NOS1 and NOS2 systems used for regression tests)

Regression tests included Telnet login and remote batch processing with rjecli.


References
----------

* Communications Control Intercom Version 3 Reference Manual, May 1980
* Communications Control Intercom Version 3 System Programmers Reference Manual, Dec 1979
* Communications Control Intercom Version 3 External Reference Specifications, Dec 1977
* NOS/BE Installation Handbook, Dec 1986


Acknowledgements
----------------

The authors of the existing CCP emulation (c) Tom Hunter, Paul Koning, Kevin Jordan.
