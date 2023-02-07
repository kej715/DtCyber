# Installing NOS 2.8.7
This directory tree contains a collection of artifacts facilitating the installation
of the NOS 2.8.7 operating system on *DtCyber*. 
Following these instructions should produce a working instance of the
operating system that supports:

- Interactive login
- Batch job submission via a simulated card reader
- Remote job entry (RJE)
- Network job entry (NJE)
- E-mail

Substantial automation has been provided in order to make the installation process
as easy as possible. In fact, it's nearly trivial compared to what was possible on
real Control Data computer systems back in the 1980's and 90's.

## Table of Contents
- [Prerequisites](#prereq)
- [Installation Steps](#steps)
- [Login](#login)
- [Remote Job Entry](#rje)
- &nbsp;&nbsp;&nbsp;&nbsp;[TieLine Facility (TLF)](#tlf)
- [Network Job Entry](#nje)
- &nbsp;&nbsp;&nbsp;&nbsp;[Using NJE](#usingnje)
- &nbsp;&nbsp;&nbsp;&nbsp;[NJF vs TLF](#njf-vs-tlf)
- [UMass Mailer](#email)
- &nbsp;&nbsp;&nbsp;&nbsp;[E-mail Reflector](#reflector)
- [Shutdown and Restart](#shutdown)
- [Operator Command Extensions](#opext)
- [Continuing an Interrupted Installation](#continuing)
- [Installing Optional Products](#optprod)
- [Creating a New Deadstart Tape](#newds)
- [Installing a Full System from Scratch](#instfull)
- [Installing a Minimal System](#instmin)
- [Customizing the NOS 2.8.7 Configuration](#reconfig)
- &nbsp;&nbsp;&nbsp;&nbsp;[[CMRDECK]](#cmrdeck)
- &nbsp;&nbsp;&nbsp;&nbsp;[[EQPDECK]](#eqpdeck)
- &nbsp;&nbsp;&nbsp;&nbsp;[[HOSTS]](#hosts)
- &nbsp;&nbsp;&nbsp;&nbsp;[[NETWORK]](#network)
- &nbsp;&nbsp;&nbsp;&nbsp;[[RESOLVER]](#resolver)
- [A Note About Anti-Virus Software](#virus)

## <a id="prereq"></a>Prerequisites
Before getting started, be aware of a few details and prerequisites:

- **Git LFS**. This directory subtree contains some large binary files in the
[tapes](tapes) subdirectory. These are virtual tape images containing operating
system executables and source code. They are managed using **Git LFS**. **Git LFS**
is not usually provided, by default, with standard distributions of Git, so you will
need to install it on your host system, if you don't have it already, and you might
need to execute *git lfs pull* or *git lfs checkout* to inflate the large binary
files properly before *DtCyber* can use them. See the [README](tapes/README.md) file
in the [tapes](tapes) subdirectory for additional details.
- **Node.js**. The scripts that automate installation of the operating system and
products are implemented in Javascript and use the Node.js runtime. You will need
to have Node.js version 16.0.0 (or later) and NPM version 8.0.0 or later. Node.js and
NPM can be downloaded from the [Node.js](https://nodejs.org/) website, and most
package managers support it as well.
- **Privileged TCP ports**. The instance of *DtCyber* used in this process is
configured by default to use standard TCP ports for services such as Telnet (port 23)
and FTP (ports 21 and 22). These are privileged port numbers that will require you to
run *DtCyber* using *sudo* on FreeBSD, Linux and MacOS. Note that it is possible
to change the port numbers used by *DtCyber*, if necessary (see the *cyber.ini*
file).

## <a id="steps"></a>Installation Steps
1. If not done already, use the appropriate *Makefile* in this directory's parent
directory, and specify the `all` target to build *DtCyber* and produce the *dtcyber*
executable with supporting tools. For Windows, a Visual Studio solution file is
available. On Windows, you will also need to execute `npm install` manually in folders
`automation`, `rje-station`, `stk`, and `webterm`.
2. Start the automated installation by executing the following command. On
Windows, you might need to enable the *dtcyber* application to use TCP ports
21, 22, and 23 too.

| OS           | Command             |
|--------------|---------------------|
| Linux/MacOS: | `sudo node install` |
| Windows:     | `node install`      |

The process initiated by the *node* command in this case will download a generic,
ready-to-run image of NOS 2.8.7 and activate it. Various system configuration parameters
may be customized by defining customized values in a file named `site.cfg`. The
installation process looks for this file and, if found, automatically executes the
`reconfigure.js` script to apply its contents and produce a customized system
configuration. See section [Customizing the NOS 2.8.7 Configuration](#reconfig), below,
for details. If `site.cfg` is not found, `install.js` simply leaves the generic
configuration in place.

After `install.js` completes, NOS 2.8.7 and all currently available optional products
will be fully installed and ready to use, and the system will be left running.
CYBIS, the Cyber Instruction System, will also be installed, but it must be started
manually by entering the following command at the NOS 2.8.7 console (i.e., *DSD*):

>`CYBIS.`

This will initiate the five subsystems comprising of CYBIS.

That's it. You have a fully operational Control Data Cyber 865 supercomputer
running the NOS 2.8.7 operating system, complete with ALGOL, APL, BASIC, COBOL, CYBIL,
FORTRAN IV and V, LISP, PASCAL, PL/1, SNOBOL, SPSS, SYMPL, COMPASS assembly language,
and various other goodies including CYBIS, ICEM (an early CAD/CAM system), CDCS (a
relational database subsystem), and GPLOT (a graphics package). Welcome back to
supercomputing in the 1980's!

## <a id="login"></a>Login
You may log into the system using your web browser. *DtCyber* is configured to
start a special web server that supports browser-based terminal emulators. For the NOS
2.8.7 system, this web server listens for connections on TCP port 8001. When you request
your web browser to open the following URL:

>`http://localhost:8001`

it will display a page showing the systems served by the web server, and this will
include the NOS 2.8.7 system itself and the CYBIS subsystem running on it. When you
click on the link associated with either of these systems, an appropriate browser-based
terminal emulator will launch, and you will be invited to login. For example, when
you click on the link for system `iaf` (the NOS 2.8.7 system itself), an ANSI X.364
(DEC VT-100 family) terminal emulator will launch. When you click on the link for
`cybis`, a PLATO terminal emulator will launch.

The browser-based X.364 terminal emulator is supported by a NOS 2 terminal definition
named `WEBTRM`. For example, to use FSE (the NOS Full Screen Editor) with the
browser-based terminal emulator, execute the following NOS command before entering FSE:

>`SCREEN,WEBTRM`

Note that the X.364 terminal emulator supports the APL character set for interacting
with the APL programming language, and it can be switched into Tektronix 4010
terminal emulation mode for use with graphics applications such as GPLOT and ICEM.

You may also log into the NOS 2.8.7 system itself using your favorite Telnet client,
and you may log into CYBIS using a PLATO terminal emulator such as
[pterm](https://www.cyber1.org/pterm.asp). The system listens for Telnet connections
on the default Telnet TCP port, 23, and it listens for CYBIS connections on TCP port
8005.

NOS logins supported by the system include:

| Family | Username | Password |
|--------|----------|----------|
| CYBER  | INSTALL  | INSTALL  |
| CYBER  | GUEST    | GUEST    |

INSTALL is a privileged account, and GUEST is an unprivileged one. The family named
CYBER is the default family, so you may simply press the return key in response to
the **FAMILY:** prompt during login.
.
The following CYBIS users are available:

| User      | Group    | Password | Purpose                               |
|-----------|----------|----------|---------------------------------------|
| admin     | s        | passme   | admin user                            |
| author    | author   | passme   | author                                |
| guest     | guests   | public   | guest account                         |
| nosguide  | cnos     | none     | Lessons on the NOS operating system   |
| nosts     | cnosts   | none     | NOS Timesharing lessons               |
| tutor     | ctutor   | none     | TUTOR programming course              |
| compass   | compass  | none     | CPU COMPASS lessons                   |
| ppu       | cppu     | none     | PP COMPASS lessons                    |
| cobol     | ccrm     | none     | Cyber Record Manager for COBOL users  |
| fortran   | ccrm     | none     | Cyber Record Manager for FORTRAN users|
| fortrancob| ccrm     | none     | Cyber Record Manager combined course  |
| fortran   | cfortran | none     | Cyber FORTRAN lessons                 |
| cobol     | ccobol   | none     | Cyber COBOL lessons                   |

## <a id="rje"></a>Remote Job Entry
The system listens for RJE (remote job entry) connections on TCP port 2553. You
can use [rjecli](../rje-station) and [rjews](../rje-station) to connect to this port and
submit batch jobs via the NOS 2.8.7 *RBF* (Remote Batch Facility) subsystem. The RJE
data communication protocol supported by NOS 2.8.7 is *HASP*. The example
[nos2.json](../rje-station/examples/) and
[rjews.json](../rje-station/examples/) configuration files condition
[rjecli](../rje-station) and [rjews](../rje-station), respectively, to use *HASP* to
connect and interact with NOS 2.8.7.

*DtCyber* is configured to start a special web service that supports browser-based
RJE access to the NOS 2.8.7 system. The web service listens for connections on TCP port
8085. When you request your web browser to open the following URL:

>`http://localhost:8085`

it will display a page showing the RJE hosts served by the web service, and this will
include the NOS 2.8.7 system. It will also indicate that it can provide access to the
[NOS 1.3](../NOS1.3) system. However, both the NOS 1.3 and the NOS 2.8.7 system must
be running concurrently, below) in order for you to be able to select both successfully.

When you click on the link associated with either of these systems, a browser-based
RJE station emulator will launch, and you will be presented with its console window.
The console window displays operator messages sent by the RJE host to the RJE station.
It also enables you to enter station operator commands to send to the host, and it
provides a user interface for loading the station's virtual card reader with virtual
card decks (i.e., batch jobs) to submit for execution on the host.

You may also request the browser-based RJE station emulator for NOS 2.8.7 directly by
entering the following URL:

>`http://localhost:8085/rje.html?m=rbf&t=RBF%20on%20NOS%202.8.7`

An RJE command line interface is available as well. The RJI CLI can be started using
the following commands on Linux/MacOS:

>```
cd rje-station
node rjecli examples/nos2.json
```

On Windows:

>```
cd rje-station
node rjecli examples\nos2.json
```

For more information about RJE, see the [README](rje-station) file in the `rje-station`
directory.

### <a id="tlf"></a>TieLine Facility (TLF)
NOS 2.8.7 also supports TLF, the TieLine Facility. TLF enables NOS 2.8.7 to connect
to other mainframes and play the role of RJE station. For example, TLF enables
NOS 2.8.7 running on *DtCyber* to connect to an IBM MVS/JES2 or IBM VM/CMS/RSCS
system running on the *Hercules* IBM mainframe emulator. NOS 2.8.7 acts as an RJE
station in these cases, enabling users of NOS to send batch jobs to the IBM
systems, and the IBM systems will return the output of these jobs to NOS.

TLF associates a LID (Logical IDentifier) with each mainframe to which it can connect.
Users specify these LID's in `ROUTE` commands or on job cards in order to indicate
that jobs should be sent by TLF to other mainframes. In addition, TLF automatically
interprets the first line of each job as a NOS jobcard and removes it before sending the
job to another mainframe. This allows the LID to be specified on the jobcard, enabling
the NOS `SUBMIT` command, for example, to be used for submitting *foreign* jobs for
execution.

For example, suppose that an IBM MVS/JES2 system running on the *Hercules* IBM mainframe
emulator is connected to *DtCyber*, and the LID *MVS* is associated with it. A job file
for this system might look like:

```
HELLO,ST=MVS.
//FIBFORTH      JOB  USER=GUEST,PASSWORD=PUBL1C,CLASS=A,MSGCLASS=A
//FIBONACI      EXEC FORTHCLG,REGION.FORT=384K
//FORT.SYSLIN   DD   UNIT=SYSDA
//FORT.SYSABEND DD   SYSOUT=A
//FORT.SYSIN    DD   *
      WRITE(6, 10)
 10   FORMAT(12H HELLO WORLD)
      STOP
      END
//
```

The `ST` (STation) parameter in the first line of the file (i.e., the jobcard) specifies
that the job is intended to run on the system with LID `MVS`. If either the NOS `ROUTE`
or `SUBMIT` command is used to submit the file for execution, TLF will process it. TLF
will send it to the IBM MVS/JES2 system for execution. When the job completes, the
MVS/JES2 system will return its output to TLF, and TLF will place it in the appropriate
NOS queue(s) (e.g., print, punch, and/or wait queues).

For example, to submit the job for execution and cause its output to be returned to the
wait queue, a user could enter the following command:

```
ROUTE,lfn,DC=TO
```

See the [[NETWORK]](#network) section, below, for details about configuring TLF.

## <a id="nje"></a>Network Job Entry
The system listens for NJE (network job entry) connections on TCP port 175 by default,
and its default node name in the public NJE network is NCCM01. It will attempt to
establish a connection automatically to NJE node NCCMAX at the
[Nostalgic Computing Center](http://www.nostalgiccomputing.org). However, at most one
node named NCCM01 may connect to NCCMAX at a time, so if another hobbyist system using
a default configuration is already connected to NCCMAX, your system's request to
connect will be rejected. Your system will continue to try establishing a connection to
NCCMAX until the other system disconnects, and then NCCMAX will be able to accept your
system's connection request.

To avoid competing with other hobbyists for access to NCCMAX, and to enroll your
NOS 2.8.7 system in the NJE-based e-mail network (see [UMass Mailer](#email), below),
you may request to register your system with a unique node name in the public NJE
network. Registration is accomplished by sending an email to
admin@nostalgiccomputing.org with a subject of `NJE registration request`. The body of
the email should contain these lines:

```
node: <requested-node-name>
link: <node-name-of-neighbor>
mid: <machine-id>
publicAddress: <IP-address-and-port>
software: <name-of-nje-software>
mailer: <mail-server-address>
```
- **node** : Required. This is the name you want to be registered for your NJE node. It
must begin with a letter and can be up to 8 alphanumeric characters in length.
- **link** : Optional. This is the name of the NJE node to which your node will connect
in order to join the network. The **link** node must have a public address, and its
administrator must agree to have it serve as your **link** node. If this parameter is
not provided, the default is `NCCMAX`.
- **mid** : Optional. Every *DtCyber* node in the network needs a unique 2-character
machine identifier. If you prefer a specific machine identifier for your machine, you
may request it using this parameter. However, if the identifier is already registered
to another node, you will need to choose a different one, or omit this parameter, and a
unique machine identifier will be generated and assigned to your node. The
[topology file](files/nje-topology.json) reflects all currently registered machine
identifiers in the `lid` properties of nodes. The `lid` property of a NOS node will be
a 2-character machine identifier prefaced by the letter `M`, e.g., `M23`.
- **publicAddress** : Optional. If your NJE node has a fixed IP address that can be
reached from other nodes on the internet, then you may register the address using this
attribute. This will enable other nodes to connect to your node and use it as their
**link** node, if you agree to this arrangement. It will also enable your **link**
node to initiate connections to your node. If this parameter is not provided, your
node must initiate connections to its **link** node, and neither the **link** node
nor any other nodes will be able to initiate connections to your node (because they
won't know its address). The syntax of this parameter is
&lt;ip-address&gt;:&lt;tcp-port&gt;, e.g., `128.153.17.31:175`.
- **software** : Optional. This specifies the name of the software implementing the
NJE protocol on your node. `NJEF` is the software that implements the NJE protocol on
NOS 2.8.7, and this is the default software name. Other names accepted are `NJE38`
(NJE software of the IBM MVS operating system), `RSCS` (NJE software of the IBM VM/CMS
operating system), and `JNET` (NJE software of the DEC VAX/VMS operating system).
- **mailer** : Optional. This specifies the NJE name and address of a BSMTP (Batch
Simple Mail Transfer Protocol) server running on your NJE node. The default for a
NOS 2.8.7 system running NJEF and the [UMass Mailer](#email) is `MAILER@node`, where
*node* is the name of your NJE node.

Example:

```
node: SYZYGY
link: NEXTDOOR
publicAddress: 128.153.17.31:175
```

The [topology file](files/nje-topology.json) in this GitHub repository will be updated
in response to your registration request. In addition, an email reply will be sent to
you confirming the registration and providing a set of properties to be edited into your
site configuration file, `NOS2.8.7/site.cfg`. The property definitions will look like:

```
[CMRDECK]
MID=23
[NETWORK]
hostID=SYZYGY
```

These properties combined with the updated NJE [topology file](files/nje-topology.json)
provide all of the information needed to add your node with its unique name to the
public NJE network. To apply the information, follow these steps:

1. Save the property definitions in the email reply to the file `NOS2.8.7/site.cfg`,
if the file doesn't exist already, or edit them into the existing file.
2. Execute `git pull` to update your repository. The update will include a new
topology definition for your node.
3. If you have started *DtCyber* using the `node start` command, enter the
`reconfigure` command at the `Operator>` prompt. Otherwise, execute the command
`node reconfigure` in the NOS2.8.7 directory. This will apply the new machine
identifier (MID in [CMRDECK] section) to your NOS system's configuration.
It will also update your system's NDL configuration to include NJE terminal(s), it will
update your system's NJF host configuration file, it will update your system's PID/LID
configuration to include PID's and LID's for adjacent NJE nodes, it will update the
UMass Mailer configuration to enable exchanging e-mail with any new NJE or TCP/IP
nodes, and it will update the *DtCyber* configuration to include definitions for NJE
terminal(s).
4. If you have started *DtCyber* using the `node start` command, enter the
`make_ds_tape` command at the `Operator>` prompt. Otherwise, execute the command
`node make-ds-tape` in the NOS2.8.7 directory. This will create a new deadstart
tape that includes changes made in step 3. The new deadstart tape image will be
in file NOS2.8.7/tapes/newds.tap.
5. Shutdown the system, rename NOS2.8.7/tapes/newds.tap to NOS2.8.7/tapes/ds.tap, and
restart *DtCyber*. This will activate the system's new machine identifer, NDL, NCF,
PID/LID, and e-mail routing configurations.

As the network topology is likely to change and expand over time, you should execute
`git pull` periodically to ensure that your copy of the
[topology file](files/nje-topology.json) is current, and when it changes, you should
execute `reconfigure` (or `node reconfigure`) to apply the changes, then shutdown
and re-deadstart to activate them.

### <a id="usingnje"></a>Using NJE
NJE (Network Job Entry) enables you to submit a batch job from one system in the
network to another, and the job's output will be returned automatically to the origin
system. NJE also enables you to send data files between users on different systems. An
NJE network is a peer-to-peer network, i.e., every node in the network has the same
capabilities to send and receive jobs and files.

An NJE network is a *store-and-forward* network. This means that you can submit a
job, or send a file, to any node in the network without your local host's needing to be
connected directly to all other nodes. If the target system of a job or file is not
directly connected to your local host, your local host will route it to a directly
connected peer that is closer to the destination and that peer will repeat the process,
recursively, until the job or file reaches the target system.

`NJF` (Network Job Facility) is the subsystem that provides NJE for NOS. It includes a
command, `NJROUTE`, that enables you to send jobs and files to any node in the NJE
network. The basic syntax of the command is:

```
NJROUTE,lfn,DC=dc,DNN=destnode[,DRN=destuser].
```

where:
- *lfn* : local file containing job or data to send
- *dc* : disposition code, e.g., one of IN, TO, PR, or PU. IN sends the file as a job
and causes its output to be returned to a local printer. TO sends the file as a job and
causes its output to be returned to the NOS wait queue. PR sends the file as print
data (i.e., text with lines up to 132 characters in length). PU sends the file as punch
data (i.e., text with lines up to 80 characters in length).
- *destnode* : name of the destination node
- *destuser* : optional username of user to whom file is sent

Example:
```
NJROUTE,MYJOB,DC=TO,DNN=NCCMAX.
```

Note that the public NJE network is a network fundamentally based upon IBM-defined
protocols and principles. In particular, a job submitted from one CDC system to another
across the network needs to begin with a couple of cards resembling the initial cards of
an IBM job. These are discarded by the NJF subsystem on the destination host before
the job is submitted to the input queue. In addition, an end-of-record mark is indicated
by a line consisting of the string `/*EOR`. For example, a job sent from one CDC system
to another might look like:

```
/*
//HELLO
HELLO.
USER,GUEST,GUEST.
FTN5,GO.
/*EOR
      PROGRAM HELLO
      PRINT *, ' HELLO WORLD!'
      END
```

`NJF` also provides interactive utilities enabling you to send/receive large
files and binary files. These utilities automatically encode/decode the files as
needed so that they may be sent/received safely as punch files. The utilities are:

- **NETSEND** : encodes and sends a file to a specified user on a specified node.
- **NETRECV** : decodes a received file. Typically, the `QGET` command is used to
retrieve a received file from the NOS punch queue, and then `NETRECV` is used to decode
it.

The utilities are implemented as interactive CCL procedures, and they provide
context-sensitive help. For example, if you call `NETSEND` as in,

```
NETSEND?
```

it will provide help information to guide you in providing the parameters that it
needs.

The ordinary `ROUTE` command may also be used to send jobs and files to nodes in the
NJE network. However, `ROUTE` requires you to specify the destination node by using
its `ST` parameter to provide the 3-character PID or LID assigned to the node.
Typically, the PID's/LID's configured on your local system will be those associated
with directly connected nodes only, so `ROUTE` will be limited to sending jobs and files
to directly connected nodes only.

In addition, `ROUTE` requires you to specify the destination username by using its `UN`
parameter. The `UN` parameter is limited to 7 characters in length. This doesn't create
a problem for NOS-to-NOS file transfers, but it might prevent you from sending a file to
a user on a directly connected IBM node where usernames can be up to 8 characters in
length.

Example:

```
ROUTE,MYJOB,DC=TO,ST=MAX.
```

### <id="njf-vs-tlf"></a>NJF vs TLF
Both NJF and TLF can send jobs from NOS 2.8.7 to other mainframes for execution, so
why use one instead of the other? NJF uses the NJE protocol to communicate with other
systems, and TLF uses the HASP protocol. NJE is a kind of HASP next generation. NJE
establishes a symmetrical peer relationship between two participating systems, while
HASP defines one participant to be a *station* and the other to be a *host*. NJE
allows both participants to send jobs and/or files to each other, while HASP allows
only a *station* to send jobs to a *host* and a *host* to send output to a *station*.

Ordinarily, if two systems support NJE, they would not need to use HASP to communicate
with each other too. However, the NJE implementations currently supported by MVS
and VM/CMS systems running on the *Hercules* IBM mainframe emulator are incomplete as
they support only exchanging print and punch files; they do not currently support
exchanging job files. TLF on NOS 2.8.7 closes this gap.

When both NJF and TLF are configured to run on NOS 2.8.7, NJF can be used for exchanging
data files and e-mail with IBM systems, and TLF can be used for sending jobs to them.
TLF can also receive jobs from IBM systems. When an IBM system sends a file on a TLF
punch stream, TLF will handle the file as a job and place it in the NOS input queue
instead of placing it in the NOS punch queue. Because TLF uses HASP, and HASP does
not enable a *station* to send output to a *host*, the output produced from a job
received by TLF on a punch stream cannot be routed automatically back to the originating
IBM system. Instead, it will be routed to the NOS output queue and printed on a NOS
printer (or punched on a NOS card punch).

## <a id="email"></a>UMass Mailer
The UMass Mailer consists of two products: `mailer` and `netmail`. These are both
installed and active in a default installation of NOS 2.8.7. The product named `mailer`
provides the base user interface for e-mail and, by itself, enables e-mail messages
to be exchanged between users within a single NOS system. The product named `netmail`
adds support for routing e-mail between systems using NJE and SMTP.

The `netmail` component is configured to use NJE for routing e-mail to systems defined
in the [NJE topology file](files/nje-topology.json). This includes NOS 2.8.7 systems,
IBM VM/CMS systems running on the Hercules IBM 370 emulator, and VAX/VMS systems running
on the SimH VAX emulator. It reproduces the
[BITNet](https://en.wikipedia.org/wiki/BITNET) experience of the 1980's and early
1990's. During that period, the NJE-based BITNet network connected hundreds of mainly
academic institutions around the world and enabled its users to exchange e-mail and
files. BITNet was the predominant world-wide e-mail network until the Internet obsoleted
it around the mid-1990's.

You may enroll your instance of NOS 2.8.7 in a reborn BITNet by requesting
a unique node name for it (see [Network Job Entry](#nje), above) and then running the
`reconfigure.js` tool to activate the node name
(see [Customizing the NOS 2.8.7 Configuration](#reconfig), below). Note also that
NJE node NCCMAX at the [Nostalgic Computing Center](https://www.nostalgiccomputing.org)
serves as a hub for NOS 2.8.7 systems interconnected by NJE, and it is also connected
to [HNET](http://moshix.dynu.net), so enrolling your system in the NJE-based *DtCyber*
network can also provide it with access to HNET.

The `netmail` component is also configured to use
[SMTP](https://en.wikipedia.org/wiki/Simple_Mail_Transfer_Protocol) for routing e-mail
to systems defined in the [[HOSTS]](#hosts) section of the `site.cfg` file and in
`smtpDomain` definitions within the [[NETWORK]](#network) section of the same file.
This enables the UMass Mailer to exchange e-mail with other systems that support SMTP
on a local area network.

## <a id="reflector"></a>E-mail Reflector
The UMass Mailer supports an e-mail reflector. E-mail messages addressed to a system's
relector will be reflected back to the sender. This enables users to test whether a
node in the network is online and processing e-mail. For example, to test whether
node NCCMAX is online and processing e-mail, you may use the UMass Mailer on your
machine to address an e-mail message to:

```
     Reflect@NCCMAX
```

The UMass Mailer on NCCMAX will reply automatically to this message. Be patient, as the
reply could take as many as 10 minutes to arrive.

Every ready-to-run instance of NOS 2.8.7 installed using these instructions, and any
that are installed manually with the `netmail` component, have an e-mail reflector.

## <a id="shutdown"></a>Shutdown and Restart
When the installation completes, NOS 2.8.7 will be running, and the command window will
be left at the DtCyber `Operator> ` prompt. Enter the `exit` command or the `shutdown`
command to shutdown the system gracefully when you have finished playing with it, and
you are ready to shut it down.

To start *DtCyber* and NOS 2.8.7 again in the future, enter the following command:

| OS           | Command           |
|--------------|-------------------|
| Linux/MacOS: | `sudo node start` |
| Windows:     | `node start`      |

## <a id="opext"></a>Operator Command Extensions
When installation completes successfully, and also when DtCyber is started using 
`start.js`, the set of commands that may be entered at the `Operator> ` prompt is
extended to include the following:

- `activate_tms` : activates the NOS Tape Management System. This is recommended if
you intend to use the automated StorageTek 4400 cartridge tape subsystem. You need
to execute this command only once.
- `exit` : exits the operator interface and initiates graceful shutdown of the system.
- `install_product` (aliases `install`, `ip`) : installs one or more optional products
on the system. Use `install help` to display the general syntax of the command
including options and a list of *categories* recognized by it. Use `install list` to
display the full list of individual products available. The command can be used to
install specific products by name or whole categories of products.
- `make_ds_tape` (alias `mdt`) : creates a new deadstart tape that includes products
installed by `install_product`.
- `reconfigure` (alias `rcfg`) : applies customized system configuration parameters. See
[Customizing the NOS 2.8.7 Configuration](#reconfig) for details.
- `njf_configure` (alias `njfc`) : applies the NJE topology definition and customized
configuration parameters to the system. See [Network Job Entry](#nje) for details.
- `shutdown` : initiates graceful shutdown of the system.
- `sync_tms` : synchronizes the NOS Tape Management System catalog with the
cartridge tape definitions specified in the `volumes.json` configuration file
found in the `stk` directory. See [stk/README.md](../stk/README.md) for details.

## <a id="continuing"></a>Continuing an Interrupted Installation
The installation process tracks its progress and can continue from its last successful
step in case of an unexpected network interruption, or due to some other form of
interruption that causes the `install.js` script to exit abnormally. To continue from an
interruption, use the `continue` option as in:

| OS           | Command                      |
|--------------|------------------------------|
| Linux/MacOS: | `sudo node install continue` |
| Windows:     | `node install continue`      |

## <a id="optprod"></a>Installing Optional Products
The `install_product` command extension enables you to re/install optional software
products for NOS 2.8.7. It also enables you to rebuild the base operating system itself
and other products provided on the initial deadstart tape. `install_product` will download tape images automatically from public libraries on the web, as needed.

To reveal a list of products that `install_product` can install, enter the
following command at the `Operator> ` prompt:

>Operator> `install list`

The `install list` command displays product names grouped by category. You may use the
`install` command to install whole categories of products or specific, individual
products. Category names are shown in category headers, and product names are shown
along with short descriptions of them.

For example, to install all products in the category named `lang` (programming languages
and toolchains), execute the following command:

>Operator> `install lang`

To install the specific product named `algol68`, execute the following command:

>Operator> `install algol68`

You may specify more than one product name or category on the command line as well, and
`install_product` will install the multiple products and/or categories requested. By
default, `install_product` will not re-install a product that is already installed. You
can force it to re-install a product or whole category by specifying the `-f` command
line option, as in:

>Operator> `install -f algol68`

Finally, you may request all not-yet-installed products to be installed by
specifying `all` as the product name, as in:

>Operator> `install all`

New products are added to the repository from time to time, so this command is particularly useful for installing products that were not available when you first
installed NOS 2.8.7 (or since the last time you executed the command).

### Category *base*
The following base CDC products are pre-installed and included on the initial
deadstart tape. However, you may re-install them (e.g., after customization), if needed.

| Product | Description |
|---------|-------------|
| atf     | Automated Cartridge Tape Facility |
| iaf     | IAF (InterActive Facility) subsystem |
| nam5    | Network Access Method |
| nos     | NOS 2.8.7 Base Operating System |
| ostext  | Operating System source texts |
| tcph    | FTP Client and Server |

### Category *cybis*

| Product | Description |
|---------|-------------|
| [cybis](https://www.dropbox.com/s/jiythdoifn1f6bm/cybis-bin.tap?dl=1)                         | Cyber Instructional System                     |
| [cybis-lessons](https://drive.google.com/drive/u/0/folders/1SY86n39EfFrX3T8scaoltRr_XMjjZiAb) | Additonal lesson content and patches for CYBIS |

### Category *database*
This category includes database software packages.

| Product | Description |
|---------|-------------|
| cdcs2   | CYBER Database Control System Version 2 |
| qu3     | Query Update Utility Version 3 |

### Category *datacomm*
This category includes data communication software.

| Product | Description |
|---------|-------------|
| [confer](https://www.dropbox.com/s/y2yumlzqjc4qva8/massmail.tap?dl=1) | UMass multi-user CONFERencing application |
| [kermit](https://www.dropbox.com/s/p819tmvs91veoiv/kermit.tap?dl=1) | Kermit file exchange utility |
| [mailer](https://www.dropbox.com/s/y2yumlzqjc4qva8/massmail.tap?dl=1) | UMass Mailer, base e-mail system |
| [ncctcp](https://www.dropbox.com/s/m172wagepk3lig6/ncctcp.tap?dl=1) | TCP/IP Applications (HTTP, NSQUERY, REXEC, SMTP) |
| [netmail](https://www.dropbox.com/s/y2yumlzqjc4qva8/massmail.tap?dl=1) | UMass Mailer, network mail router |
| [njf](https://www.dropbox.com/s/oejtd05qkvqhk9u/NOSL700NJEF.tap?dl=1) | Network Job Facility |
| rbf5    | Remote Batch Facility Version 5 |
| [tlf](https://www.dropbox.com/s/r5qbucmw6qye8vl/tieline.tap?dl=1) | TieLine Facility |

### Category *graphics*
This category includes graphics and CAD/CAM software.

| Product | Description |
|---------|-------------|
| [gplot](https://www.dropbox.com/s/l342xe61dqq8aw5/gplot.tap?dl=1) | GPLOT and ULCC DIMFILM graphics library |
| [icem](https://www.dropbox.com/s/1ufg3504xizju0l/icem.zip?dl=1) | ICEM Design/Drafting and GPL compiler |

### Category *lang*
The following product names represent optional programming languages and associated
toolchains that may be installed. Note that the initial deadstart tape includes the
standard, CDC-provided programming languages APL, BASIC, COBOL, COMPASS, FORTRAN (both
FTN and FTN5), PASCAL, and SYMPL.

| Product | Description |
|---------|-------------|
| [algol5](https://www.dropbox.com/s/jy6bgj2zugz1wke/algol5.tap?dl=1)| ALGOL 5 (ALGOL 60) |
| [algol68](https://www.dropbox.com/s/6uo27ja4r9twaxj/algol68.tap?dl=1) | ALGOL 68 |
| [pascal4](https://www.dropbox.com/s/wsgz02lnrtayfiq/PASCAL6000V410.tap?dl=1) | Pascal-6000 V4.1.0 : this script replaces PASCAL/170 on the base deadstart tape with Pascal4 |
| [pascal4-pf](https://www.dropbox.com/s/wsgz02lnrtayfiq/PASCAL6000V410.tap?dl=1) | Pascal-6000 V4.1.0 : this script installs Pascal4 in the LIBRARY catalog only |
| [pl1](https://www.dropbox.com/s/twbq0xpmkjoakzj/pl1.tap?dl=1) | PL/I |
| [ses](https://www.dropbox.com/s/ome99ezh4jhz108/SES.tap?dl=1) | CYBIL and Software Engineering Services tools |
| [snobol](https://www.dropbox.com/s/4q4thkaghi19sro/snobol.tap?dl=1) | CAL 6000 SNOBOL |
| [utlisp](https://www.dropbox.com/s/0wcq8e7m379ivyw/utlisp.tap?dl=1) | University of Texas LISP |

### Category *misc*
This category contains miscellaneous software packages that don't fit into any other
category.

| Product | Description |
|---------|-------------|
| cgames  | NOS Console Games (EYE, KAL, LIFE, LUNAR, MIC, PAC, SNK, TTT) |
| [i8080](https://www.dropbox.com/s/ovgysfxbgpl18am/i8080.tap?dl=1) | Intel 8080 tools (CPM80, INTRP80, MAC80, PLM80) |
| skedulr | Task scheduler (similar to *cron* in Linux/Unix systems) |
| [spss](https://www.dropbox.com/s/2eo63elqvhi0vwg/NOSSPSS6000V9.tap?dl=1) | SPSS-6000 V9 - Statistical Package for the Social Sciences |

These lists will grow, so revisit this page to see what new products have been added,
use `git pull` to update the lists in your local repository clone, and then use
`install all` to install them.

## <a id="newds"></a>Creating a New Deadstart Tape  
The `reconfigure.js` and `njf-configure.js` tools and jobs initiated by
`install_product` insert the records they produce into the direct access file named
`PRODUCT` in the catalog of user `INSTALL`, and they also update the file named
`DIRFILE` to specify the system libraries with which the binaries are associated. To
create a new deadstart tape that includes the contents of `PRODUCT`, execute the following command:

>Operator> `make_ds_tape`

`make_ds_tape` reads the current deadstart tape, calls `LIBEDIT` to replace or
add the contents of `PRODUCT` as directed by `DIRFILE`, and then writes the resulting
file to a new tape image. The new tape image will be a file with path
`NOS2.8.7/tapes/newds.tap`. To restart NOS 2.8.7 with the new tape image, first shut it
down gracefully using the `shutdown` command:

>Operator> `shutdown`

Then, when shutdown is complete, save the old deadstart tape image and activate the
new one by renaming them, as in:

| OS           | Command                            |
|--------------|------------------------------------|
| Linux/MacOS: | `mv tapes/ds.tap tapes/ods.tap`    |
|              | `mv tapes/newds.tap tapes/ds.tap`  |
| Windows:     | `ren tapes\ds.tap tapes\ods.tap`   |
|              | `ren tapes\newds.tap tapes\ds.tap` |

To restart the system using the new deadstart tape, use the `start.js` script, as in:

| OS           | Command           |
|--------------|-------------------|
| Linux/MacOS: | `sudo node start` |
| Windows:     | `node start`      |

## <a id="instfull"></a>Installing a Full System from Scratch
It is possible to install a full NOS 2.8.7 from scratch by specifying the `full` option
when calling the `install.js` script, as in:

| OS           | Commands                 |
|--------------|--------------------------|
| Linux/MacOS: | `sudo node install full` |
| Windows:     | `node install full`      |

The generic, ready-to-run NOS 2.8.7 image that is downloaded and activated by default
is created in this way. Note that this option can take as much as three hours or more,
depending upon the speed and capacity of your host system. All optional products are
installed, one by one, and this involves building most of them from source code.

If the file `site.cfg` exists, the `reconfigure.js` script will be called to apply its
contents. This enables the full, installed-from-scratch system to have a customized
configuration.

## <a id="instmin"></a>Installing a Minimal System
If you prefer to install a minimal NOS 2.8.7 system with a subset of optional
products, or none of the optional products, you may accomplish this by specifying the
`basic` option when calling the `install.js` script, as in:

| OS           | Commands                  |
|--------------|---------------------------|
| Linux/MacOS: | `sudo node install basic` |
| Windows:     | `node install basic`      |

The `basic` option causes `install.js` to install a minimal NOS 2.8.7 system from
scratch without any optional products. However, if the file `site.cfg` exists, the
`reconfigure.js` script will be called to apply its contents. This enables the basic
system to have a customized configuration.

To install optional products atop the basic system, use the `install_product` command, as in:

>Operator> `install lang`

In case a basic installation is interrupted before completing successfully, use the
`continue` option to proceed from the point of interruption, as in:

| OS           | Commands                           |
|--------------|------------------------------------|
| Linux/MacOS: | `sudo node install basic continue` |
| Windows:     | `node install basic continue`      |

## <a id="reconfig"></a>Customizing the NOS 2.8.7 Configuration
Various parameters of the NOS 2.8.7 system configuration may be changed or added to
accommodate personal preferences or local needs. For example:
- Definitions in the system CMR deck may be updated to change parameters such as the
machine identifier or system name.
- Definitions may be updated or added to the system equipment deck to add peripheral
equipment or change their parameters.
- The system's IP address may be defined.
- The TCP/IP hosts statically known to the system may be defined.
- The TCP/IP resource resolver configuration may be defined.
- The NJE node name of the local host may be defined.
- Private nodes in a local NJE network may be defined.
- Internet e-mail domains may be declared.
- etc.

A script named `reconfigure.js` applies customized configuration. The script looks for a
file named `site.cfg` in the current working directory. This file is where all
customized system configuration parameters are expected to be defined. The script
is called as in:

| OS           | Commands                           |
|--------------|-------------------------|
| Linux/MacOS: | `sudo node reconfigure` |
| Windows:     | `node reconfigure`      |


`site.cfg` may contain one or more sections. Each section begins with a name delimited
by `[` and `]` characters (like *DtCyber's* `cyber.ini` file). For example, here is a
section named `CMRDECK`:

```
[CMRDECK]
MID=AX.
NAME=NCCMAX - CYBER 865 WITH CYBIS
```

The following section names are recognized:

### <a id="cmrdeck"></a>[CMRDECK]
Defines parameters to be edited into the system's primary CMR deck, CMRD01. Any CMR deck
entry defined in the
[NOS 2 Analysis Handbook](http://bitsavers.trailing-edge.com/pdf/cdc/cyber/nos2/60459300U_NOS_2_Analysis_Handbook_Jul94.pdf)
may be specified in this section. If an MID entry is included, `reconfigure.js` will
ensure that all other system configuration parameters dependent upon the MID value
are also updated. This includes LID table definitions, TCPHOST file definitions, etc.

### <a id="eqpdeck"></a>[EQPDECK]
Defines parameters to be edited into the system's primary equipment deck, EQPD01. Any
equipment deck entry defined in the
[NOS 2 Analysis Handbook](http://bitsavers.trailing-edge.com/pdf/cdc/cyber/nos2/60459300U_NOS_2_Analysis_Handbook_Jul94.pdf)
may be specified in this section. Example:

```
[EQPDECK]
EQ015=DL,UN=1,CH=11,ST=OFF.
REMOVE=014,015.
```

### <a id="hosts"></a>[HOSTS]
Defines the IP addresses and names of hosts in the TCP/IP network. These are edited
into the system's TCPHOST file in the catalog of user NETADMN. The definitions may
include the special entry for the local NOS 2.8.7 host, and this entry can be
used to define the NOS 2.8.7 system's public IP address. The special entry for the
local NOS 2.8.7 host is the entry that includes the alias `LOCALHOST_id` where `id`
is the 2-character machine identifier of the host (as defined by MID in the CMR deck).
Example:

```
[HOSTS]
192.168.0.17 nccmax nccmax.nostalgiccomputing.org max LOCALHOST_AX STK
192.168.1.2  vax1 vax1.nostalgiccomputing.com
192.168.1.3  vax2 vax2.nostalgiccomputing.com
192.168.1.4  rsx11m rsx11m.nostalgiccomputing.com
192.168.1.5  tops20 tops20.nostalgiccomputing.com pdp10
192.168.2.2  bsd211 bsd211.nostalgiccomputing.com pdp11 mail-relay
```

When the [HOSTS] section defines hosts other than the local NOS 2.8.7 host and
hosts defined in the NJE topology, the UMass Mailer is configured to use TCP/IP and
the SMTP protocol to exchange e-mail with them. In order for the UMass Mailer to be
able to send e-mail to destinations using TCP/IP and SMTP, one of the hosts in
the [HOSTS] section must be defined with the alias `mail-relay`. The UMass Mailer will
send all SMTP-based mail to that host, and that host is expected to be capable of
forwarding it to other SMTP-based hosts, as needed. Typically, Unix hosts are capable
of serving as mail relays.

### <a id="network"></a>[NETWORK]
Defines private routes and the names of hosts in the CDC and NJE (Network Job Entry)
networks. In particular, the node name of the local host is defined in this section
when the local host is registered in the public NJE network. This enables the NJF
configuration tool, `njf-configure.js`, to recognize the local host in the public
network topology definition, [files/nje-topology.json](files/nje-topology.json).

Definitions in this section are edited into the NOS system's NDL and HCF (NJF host
configuration file) files as well as *DtCyber's* `cyber.ovl` file. Example:

```
[NETWORK]
hostID=MYNODE
njeNode=LOCALCMS,RSCS,CMS,192.168.1.3:175,MYNODE,B8192,MAILER@LOCALCMS
```

The entries that may occur in the `[NETWORK]` section include:

- **defaultRoute** : Specifies the name of the NJE node to which jobs and files should
be routed by default. Typically, this is the name of the adjacent node serving as the
local system's primary link to the public NJE network. Example:

    `defaultRoute=NCCMAX`
    
- **hostID** : Specifies the 1 - 8 character node name of the local host. Example:

    `hostID=MYNODE`

- **njeNode** : Defines the name and routing information for an NJE node within a
private NJE network (i.e., a node that is not registered in the public topology,
[files/nje-topology.json](files/nje-topology.json). The general syntax of this entry
is:

    njeNode=*nodename*,*software*,*lid*,*public-addr*,*link*[,*local-addr*[,B*block-size*][,P*ping-interval*][,*mailer-address*]
    
    | Parameter     | Description |
    |---------------|-------------|
    | nodename      | The unique 1 - 8 character name of the node. |
    | software      | The name of the NJE software used by the node; one of NJEF, RSCS, NJE38, or JNET. |
    | lid           | The unique, 3-character logical identifier assigned to the node. |
    | public-addr   | The public IP address and TCP port number on which the node listens for NJE connections. The format is `ipaddress:portnumber`, e.g., 192.168.1.3:175. If the node does not have a public address, 0.0.0.0:0 is specified. |
    | link          | The name of the NJE node that serves as the defined node's primary link to the NJE network. |
    | local-addr    | Optional IP address of the defined node. If specified, this address will be used by the defined node to identify itself to its NJE peers. If omitted, an appropriate default will be chosen based upon the defined system's physical TCP network configuration. |
    | block-size    | Optional block size, in bytes, to use in communicating with peers. The default is 8192. |
    | ping-interval | Optional ping interval, in seconds, to use in keeping the NJE connection alive. The default is 600 (i.e., 10 minutes). |
    | mailer-address | Optional address of a BSMTP mail server, in the form `mailername@nodename`, e.g., **MAILER@NCCMAX** |

    Example:
    
    `njeNode=LOCALCMS,RSCS,CMS,192.168.1.3:175,MYNODE,B8192,MAILER@LOCALCMS`
    
- **njePorts** : Specifies the range of CLA ports that will be used in defining terminals for use by NJF in the NOS NDL. The general syntax of this entry is:

    njePorts=*cla-port-number*,*port-count*
    
    Where *cla-port-number* is the number of the first CLA port to be used, and
    *port-count* defines the maximum number of ports to be used. The default is
    equivalent to:
    
    `njePorts=0x30,16`
    
- **smtpDomain** : Identifies an internet domain name or suffix to which e-mail will
be routed using the TCP/IP SMTP protocol. Examples:

```
    smtpDomain=.com
    smtpDomain=.org
    smtpDomain=.net
    smtpDomain=local.host.org
```
- **tlfNode** : Defines the name and routing information for a TLF node. The general
syntax of this entry is:

    tlfNode=*nodename*,*lid*,*spooler*,*addr*[,B*block-size*][,R*remote-id*][,P*password*]
    
    | Parameter     | Description |
    |---------------|-------------|
    | nodename      | The name of the node. |
    | lid           | The unique, 3-character logical identifier assigned to the node. |
    | spooler       | The type of output spooler used by the node; one of JES2, NOS, PRIME, or RSCS. |

    | addr   | The IP address and TCP port number on which the node listens for HASP connections. The format is `ipaddress:portnumber`, e.g., 192.168.1.3:175. |
    | block-size    | Optional block size, in bytes, to use in communicating with peers. The default is 400. |
    | remote-id | Optional login identifier that TLF will present to the node when it connects. The default is that no login identifier is sent. |
    | password | Optional password that TLF will present to the node when it connects. The default is that no password is sent. |

    Example:
    
    `tlfNode=NCCMVS,MVS,JES2,192.168.0.17:37803,R001`
    
- **tlfPorts** : Specifies the range of CLA ports that will be used in defining terminals for use by TLF in the NOS NDL. The general syntax of this entry is:

    tlfPorts=*cla-port-number*,*port-count*
    
    Where *cla-port-number* is the number of the first CLA port to be used, and
    *port-count* defines the maximum number of ports to be used. The default is
    equivalent to:
    
    `tlfPorts=0x28,8`

### <a id="resolver"></a>[RESOLVER]
Defines the TCP/IP resource resolver configuration. This is saved as the file named
TCPRSLV in the catalog of user NETADMN. The resource resolver is used by some
applications to look up the IP addresses of hosts dynamically. Example:

```
[RESOLVER]
search nostalgiccomputing.org
nameserver 192.168.0.19
```

## <a id="virus"></a>A Note About Anti-Virus Software
The installation scripts automatically download tape images and other files, as
needed, from the internet. Anti-virus software can interfere with this process
and prevent these files from being downloaded successfully. If an installation script
fails to download a file successfully and exits with an error indication, and
anti-virus software is running on your host system, look into configuring the
anti-virus software such that it does not apply to files being downloaded to the
NOS2.8.7/opt/tapes directory of your *DtCyber* git repository.
