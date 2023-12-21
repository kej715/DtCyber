CCI emulation for DtCyber
=========================


Index
-----

* [Description](#description)
* [Features](#features)
* [Configuration](#configuration)
* [Theory of Operation](#theory-of-operation)
* [Implementation](#implementation)
* [Installation](#installation)
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


Configuration
-------------
In cyber.ini (equipment section):

     HCP,<eq>,<un>,<ch>

Example: configure the CCI HCP on channel 7

     HCP,0,0,7

NOS/BE CMR equipment table configuration :

     FE,CH=<ch>,<EQP>=<eq>,<ESTO>=<esto>

Example: configure the CCI HCP, channel 7, port 0 as equipment ordinal 30 in the CMR equipment table

     FE,CH=7,ESTO=30

The DtCyber Telnet network access is configured in the npuConnections section of the cyber.ini file.

Example: allow up to 8 Telnet connections on port 6610, using CLA ports 1 - 8

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
                                        skip CLA ports of disabled lines
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
                                                                               Note: after a LOGOUT the connection is terminated
                                                                               automatically after a few minutes of inactivity
    ------- delete terminal SM ------> remover terminal configuration                                       close session
    <------ terminal deleted SM ------
    ------- disconnect line SM ------> LS: inoperative, wait for ring
    <------ line disconnected SM -----
                                                                               

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
                                        
                                        
Operator Command LINEOFF,&lt;est&gt;,&lt;line&gt;, Telnet session not active:


     HOST                                         HCP                                                       Telnet Client
     ------ disconnect line SM -------> LS: inoperative, wait for ring
     <----- line disconnected SM ------
     ------ disable line SM ----------> set line disabled flag
     <----- line disable SM ----------- LS: inoperative

Operator Command LINEON,&lt;est&gt;,&lt;line&gt;


     HOST                                         HCP                                                       Telnet Client
     ------- Enable Line SM -----------> clear line disabled flag
     <------ Line Enabled SM ----------- LS: inoperative, wait for ring


Implementation
--------------
An attempt was made to use as much code as possible from the existing CCP implementation and at the same time to intervene as little as possible in the flow logic of CCP.
CCI and CCP can only run mutually exclusive. A global variable <em>npuSw</em> controls wheter CCI or CCP is being executed. In the routines used jointly by CCI and CCP, the system-specific subprograms are called via function pointers.

Beyond that, the CCP code was only modified in the following cases:

* npu.h: some CCI specific variables were added to the TCB structure, which have no effect, if CCP is running.

* init.c: The minimum value of coulper node was changed to 0, which is required for a singele CCI NPU configuration.

* npu_async.c: Make *npuAsyncLog global, because it is also used in cci_async.c. Modify debug output statement to consider the case, that a TCB has not yet been configured.

* npu_net.c: Some changes were needed to consider that no TCB exists at that time, when a telnet connection is currently being established (see above). This required some additional variables in the PCB structure which have no effect, if CCP is running.


Installation
------------
The [NOSBE712 Repository](https://github.com/bug400/NOSBE712) contains an environment to build and run a NOS/BE 1.5 Level 712 system with CCI and INTERCOM. The deadstart tape generated with this environment also includes faked load modules for the 255X HCP. See 
[How to build NOS/BE 1.5 Level 712 from Scratch](https://codex.retro1.org/cdc:nosbe:building_nos_be_level_712_from_scratch) how to proceed.

There are ready to run NOS/BE INTERCOM systems available, see [How to use a ready to run NOS/BE Level 712 system](https://codex.retro1.org/cdc:nosbe:use_a_ready_to_run_nos_be_l_712_system).


References
----------

* Communications Control Intercom Version 3 Reference Manual, May 1980
* Communications Control Intercom Version 3 System Programmers Reference Manual, Dec 1979
* Communications Control Intercom Version 3 External Reference Specifications, Dec 1977
* NOS/BE Installation Handbook, Dec 1986


Acknowledgements
----------------

The authors of the existing CCP emulation (c) Tom Hunter, Paul Koning, Kevin Jordan.
