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
- NOS-to-NOS networking
- Cray supercomputer interaction

Automation has been provided in order to make the installation process
as easy as possible. In fact, it's nearly trivial compared to what was possible on
real Control Data computer systems back in the 1980's and 90's.

## Table of Contents
- [Prerequisites](#prereq)
- [Installation Steps](#steps)
- &nbsp;&nbsp;&nbsp;&nbsp;[Other Installation Options](#installopts)
- [Login](#login)
- [Web Console](#web-console)
- [Remote Job Entry](#rje)
- &nbsp;&nbsp;&nbsp;&nbsp;[TieLine Facility (TLF)](#tlf)
- [Network Job Entry](#nje)
- &nbsp;&nbsp;&nbsp;&nbsp;[Using NJE](#usingnje)
- &nbsp;&nbsp;&nbsp;&nbsp;[NJE Services](#njeservices)
- &nbsp;&nbsp;&nbsp;&nbsp;[CONFER and NJE-based Chat](#confer)
- &nbsp;&nbsp;&nbsp;&nbsp;[NJF vs TLF](#njf-vs-tlf)
- [UMass Mailer](#email)
- &nbsp;&nbsp;&nbsp;&nbsp;[E-mail Reflector](#reflector)
- [NOS to NOS networking (RHP - Remote Host Products)](#rhp)
- &nbsp;&nbsp;&nbsp;&nbsp;[Using RHP](#usingrhp)
- [TCP/IP Applications](#tcpip)
- [Cray Station](#cray)
- &nbsp;&nbsp;&nbsp;&nbsp;[COS Tools](#cos-tools)
- [Shutdown and Restart](#shutdown)
- [Operator Command Extensions](#opext)
- [Continuing an Interrupted Installation](#continuing)
- [Installing Optional Products](#optprod)
- [Creating a New Deadstart Tape](#newds)
- [Installing a Full System from Scratch](#instfull)
- [Installing a Minimal System](#instmin)
- [Upgrading Cyber 865 to Cyber 875](#upgrade875)
- [Customizing the NOS 2.8.7 Configuration](#reconfig)
- &nbsp;&nbsp;&nbsp;&nbsp;[[CMRDECK]](#cmrdeck)
- &nbsp;&nbsp;&nbsp;&nbsp;[[EQPDECK]](#eqpdeck)
- &nbsp;&nbsp;&nbsp;&nbsp;[[HOSTS]](#hosts)
- &nbsp;&nbsp;&nbsp;&nbsp;[[NETWORK]](#network)
- &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;[crayStation](#crayStation)
- &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;[defaultRoute](#defaultRoute)
- &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;[haspPorts](#haspPorts)
- &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;[haspTerminal](#haspTerminal)
- &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;[hostID](#hostID)
- &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;[networkInterface](#networkInterface)
- &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;[njeNode](#njeNode)
- &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;[njePorts](#njePorts)
- &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;[rhpNode](#rhpNode)
- &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;[safNode](#safNode)
- &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;[smtpDomain](#smtpDomain)
- &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;[stkDrivePath](#stkDrivePath)
- &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;[tlfNode](#tlfNode)
- &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;[tlfPorts](#tlfPorts)
- &nbsp;&nbsp;&nbsp;&nbsp;[[PASSWORDS]](#passwords)
- &nbsp;&nbsp;&nbsp;&nbsp;[[RESOLVER]](#resolver)
- [Customized Configuration Examples](#cfgexamples)
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

### <a id="installopts"></a>Other Installation Options
The installation tool provides options for installing other variants of the operating system, including installation of fully customized sets of programming languages and other
features. The general syntax of a call to the tool is:

```
[sudo] node install [basic | full | (readytorun | rtr) [<image name>]][(continue | cont)]
  basic      : install a basic system from scratch without any optional products
  full       : install a full system from scratch with all optional products
  readytorun : (alias rtr) install a ready-to-run system image
               <image name> is one of:
                 nos287-full-865       : full NOS 2.8.7 system running on a Cyber 865 (default)
                 nos287-full-875       : full NOS 2.8.7 system running on a Cyber 875
                 nos287-full-875-beast : full NOS 2.8.7 system running on a Cyber 875
                                         with 16M ESM and four 885-42 disks
                 nos287-most-175       : full NOS 2.8.7 system (except CYBIS) running
                                         on a Cyber 175 with four 885-42 disks

  continue   : (alias cont) continue basic or full installation from last point of interruption
```

See [Installing a Minimal System](#instmin) for a description of the `basic` option, and see [Installing a Full System from Scratch](#instfull) for a description of the `full` option.

The default option is `readytorun` (alias `rtr`). This option downloads and installs a
previously created system image by name. The names of currently available images are
shown in the table, below. The name of the default image is `nos287-full-865`.

| Name            | Description                                                      |
|-----------------|------------------------------------------------------------------|
| nos287&minus;full&minus;865 | This image includes all available programming languages and other features, and it runs on a **Cyber 865** mainframe with a full complement (2M words) of central memory and 2M words of ESM. **This is the default system image.** |
| nos287&minus;full&minus;875 | This image includes all available programming languages and other features, and it runs on a **Cyber 875** mainframe with a full complement (4M words) of central memory and 2M words of ESM. |
| nos287&minus;full&minus;875&minus;beast | This image includes all available programming languages and other features, and it runs on a **Cyber 875** mainframe with a full complement (4M words) of central memory, 16M words of ESM, and four 885-42 buffered disk drives. |
| nos287-most-175 | This image includes all available programming languages and other features, except CYBIS. It runs on a **Cyber 175** mainframe with a full complement (256K words) of central memory, 2M words of ESM, and four 885-42 buffered disk drives. |

Example:
```
sudo node install rtr nos287-full-875
```

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

## <a id="web-console"></a>Web Console
*DtCyber* is configured to start a special web server that supports browser-based user
interfaces to the system console. For the NOS 2.8.7 system, this web server listens for
connections on TCP port 18001. When you request your web browser to open the following
URL:

>`http://localhost:18001`

it will display a page showing the systems served by the web-based console server and the
types of service provided. Ordinarily, two entries will be shown, and they represent two
types of access to the system console of the NOS 2.8.7 system. One provides a 2-dimensional
representation of the console, and the other provides a 3-dimensional one. When you click on either link, a browser-based console emulator will launch, and the local console window
(either the X-Windows interface on Linux/Unix, or the Windows interface on Microsoft operating
systems) will suspend after issuing a message indicating *Remote console active*.

You may use the browser-based interface to interact with the system in the same way that you
use the local console interface. The NOS 2.8.7 system itself cannot distinguish between the
two forms of user interface. When you close the browser window in which the system console
interface is running, control will revert automatically to the local console user interface.

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
NOS 2.8.7 running on *DtCyber* to connect to an IBM MVS/JES2
(e.g., [TK4-](https://wotho.ethz.ch/tk4-/)) or
IBM VM/CMS/RSCS (e.g., [VM370CE](http://www.vm370.org/VM/V1R1.2))
system running on the [Hercules](https://github.com/SDL-Hercules-390/hyperion) IBM
mainframe emulator. NOS 2.8.7 acts as an RJE station in these cases, enabling users of
NOS to send batch jobs to the IBM systems, and the IBM systems will return the output
of these jobs to NOS. 

TLF associates a LID (Logical IDentifier) with each mainframe to which it can connect.
Users specify these LID's in `ROUTE` commands or on job cards in order to indicate
that jobs should be sent by TLF to other mainframes. In addition, TLF automatically
interprets the first line of each job as a NOS jobcard and removes it before sending
the job to another mainframe. This allows the LID to be specified on the jobcard,
enabling the NOS `SUBMIT` command, for example, to be used for submitting *foreign*
jobs for execution.

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
- **node** : Required. This is the name you want to be registered for your NJE node. It should
be in the form *llooommm*, where *ll* is the 2-letter country code, province, or state
abbreviation reflecting the location of your node (e.g., GB, DE, TX, ME), *ooo* is a
3-letter identifier of your node's operating system (e.g., NOS, MVS, VMS, CMS), and *mmm*
is a 3-letter identifier of your choice (e.g., the PID/LID of your NOS 2.8.7 system). For example, a NOS 2.8.7 system located in the US state of Maine and having a machine
identifier (see **mid**, below) of EL might have MENOSMEL as its NJE node name.
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
MID=23.
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
terminal(s). Finally, it will create a new deadstart tape, and then it will shutdown
the system and re-deadstart it to use the new tape. This will activate the system's new
machine identifer, NDL, NCF, PID/LID, and e-mail routing configurations.

As the network topology is likely to change and expand over time, you should execute
`git pull` periodically to ensure that your copy of the
[topology file](files/nje-topology.json) is current, and when it changes, you should
execute `reconfigure` (or `node reconfigure`) to apply and activate the changes.

### <a id="usingnje"></a>Using NJE
NJE (Network Job Entry) enables you to submit a batch job from one system in the
network to another, and the job's output will be returned automatically to the origin
system. NJE also enables you to send messages and data files between users on different
systems. An NJE network is a peer-to-peer network, i.e., every node in the network has the
same capabilities to send and receive jobs, messages, and files.

An NJE network is a *store-and-forward* network. This means that you can submit a
job, or send a message or file, to any node in the network without your local host's
needing to be connected directly to all other nodes. If the target system of a job,
message, or file is not directly connected to your local host, your local host will route
it to a directly connected peer that is closer to the destination and that peer will repeat
the process, recursively, until the job, message, or file reaches the target system.

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

NJF also provides commands named `NJCMD` and `NJMSG`. These commands enable users to send
commands and short messages, respectively, across the NJE network to remote services and
users. In each case, such commands and messages comprise of a single line of text with
a maximum length of about 130 characters. The general syntax of the commands is:

```
NJCMD[,F=origuser][,I=ilfn][,N=destnode][,O=olfn][,W=secs].[node][command text]
```
```
NJMSG[,F=origuser][,I=ilfn][,N=destnode][,O=olfn][,U=destuser][,W=secs].[user@node][message text]
```

where:
- *origuser* : optional 1 - 8 character name of the originating user or service, default is the NOS username of the calling user.
- *ilfn* : optional name of a local input file. If this argument is specified, the text of the command or message to be sent is read from the specified local file. If the file contains more than one line, multiple commands or messages will be sent, one per line.
- *destnode* : optional 1 - 8 character name of the destination NJE node. If this argument is omitted, the destination user and node names must be specified after the command terminator.
- *olfn* : optional name of a local output file. If this argument is specified, any messages received from the destination node will be written to the specified local file. Otherwise, any messages received will be written to OUTPUT. Specifying *O=0* suppresses output.
- *destuser* : optional 1 - 8 charaacter name of the destination user or service. If this argument is omitted, the destination user/service and node names must be specified after the command terminator. Note that this parameter pertains only to `NJMSG` because NJE messages may be addressed to specific users or services. By contrast, NJE commands are processed directly by the NJE subsystem of the destination node, so `NJCMD` does not require a destination user or service name to be supplied.
- *secs* : maximum number of seconds to wait for response messages after sending message(s). The default is 10.
- *node* or *user@node* : if the **N=** and **U=** arguments are not provided, the first token after the command terminator is assumed to have the form `node` for `NJCMD` and `user@node` for `NJMSG`, and it is taken as the specification of the destination user/service and node names to which commands or messages will be sent.
- *message text* : if the **I=** argument is not provided, the command or message to be sent
is taken from text following the command terminator.

Examples:
```
NJMSG.INFO@NCCMAX /INFO
NJMSG.GUEST@NCCM01 HELLO, ARE YOU THERE?
NJMSG,U=GUEST,N=NCCM01,I=MSGS,W=0.

NJCMD.RELAY INFO
NJCMD,N=NCCCMS.CPQ TIME
```

The first example sends the message `/INFO` to a service (presumably) named `INFO` on NJE node `NCCMAX`, and it waits 10 seconds for a response. The second example sends the message
`HELLO, ARE YOU THERE?` to a user named `GUEST` on NJE node `NCCM01`, and it waits 10
seconds for a response. The third example sends possibly multiple messages (one per line)
from the local file named `MSGS` to a user named `GUEST` on NJE node `NCCM01`, and it does
not wait for responses. The fourth example sends the command `INFO` to NJE node `RELAY`, and
the last example sends the command `CPQ TIME` to NJE node `NCCCMS`.

### <a id="njeservices"></a>NJE Services
Network services may be built using NJE's messaging capability, and two such services are
provided by the default installation of NOS 2.8.7. The services are named `ECHO` and
`INFO`. `ECHO` is a service that simply echoes each message it receives back to the
sender. It can be used to test that a NOS 2.8.7 node is available on the NJE network. For
example, to send a message to the `ECHO` service on node `NCCMAX`, enter the following
command:

```
NJMSG.ECHO@NCCMAX PLEASE ECHO ME
```

The `INFO` service provides information about the NOS 2.8.7 system on which it is running.
It accepts some simple commands and responds by sending corresponding information. All
commands begin with the character **/**. Send it a `HELP` command to see what commands it
accepts, as in:

```
NJMSG.INFO@NCCMAX /HELP
```

The response will look like:
```
 COMMANDS AVAILABLE:
   /HELP  : SHOW THIS HELP
   /INFO  : SHOW ALL AVAILABLE INFO
   /STATS : SHOW STATISTICS
   /TIME  : SHOW START TIME AND UPTIME
```

To obtain all available information about node `NCCMAX`, enter the following command:
```
NJMSG.INFO@NCCMAX /INFO
```
and the response will look like:
```
    TITLE: (MAX)IMUM - CYBER 875.
       OS: NOS 2.8.7 871/871.
 SOFTWARE: NJEF 2.7.1, NJMD 1.0
  STARTED: 23/09/21. 21.26.07.
      NOW: 23/09/26. 10.47.04.
   UPTIME: 4 DAYS, 13 HOURS, 20 MINUTES, 57 SECONDS
 REQUESTS: 34
 MESSAGES: 181
```

A NOS application named `NJMD` (NJE Messaging Daemon) implements NJE services, and it
supports creation of custom services. Two files named `NJMDCF` and `NJMDLIB` in the catalog
of username `NJF` provide `NJMD`'s configuration. `NJMDCF` defines each service, with one
service definition per line. Each line has three fields, as follows:

- *name*. The first field defines the 1 - 8 character name of the service.
- *command*. The second field specifies the command that implements the service. Ordinarily, it is a `BEGIN` command that calls a CCL procedure found in `NJMDLIB`.
- *authentication*. The username and password to be used in executing the command.

Each time `NJMD` receives a message for a service it supports, it creates and submits a
batch job to respond to the message. The username and password for the batch job are taken
from the *authentication* field of the service's entry in `NJMDCF`. The primary command
executed by the batch job is taken from the *command* field of the entry, and `NJMD`
arranges to pass three arguments to it:

1. the name of the service itself
2. the name of the originating user
3. the name of the originating NJE node.

`NJMD` also arranges to provide the text of the message as the job's `INPUT` file.

Typically, the command implementing the service will read and process the `INPUT` file and
write its response to an output file. It will then use the `NJMSG` command to return its
response to the originating user.

As an example, the `ECHO` service is defined as a CCL procedure. Its source code can be
found in procedure library `NJMDLIB` (a file in the catalog of username `NJF`).

### <a id="confer"></a>CONFER and NJE-based Chat
`CONFER` is a NOS network application implemented in the mid-1980's by the University of
Massachusetts (UMass).
UMass distributed it freely to many other universities and research institutions around the
world. It was intended to provide online teleconferencing services (hence, its name),
although it's probably more easily recognized now as a *group chat* application.

Originally, `CONFER` supported only local users, i.e., users connected directly to a NOS
system via local interactive terminals attached to NPU's or CDCNet. Since the original
implementation, however, `CONFER` has been enhanced to use the NJE messaging services
exposed by `NJF`. This enables `CONFER` to serve as a network group chat application.

One way to access `CONFER` is to log into NOS 2.8.7 as usual, then use the `HELLO` command
to exit `IAF` and enter `CONFER`, as in:

```
HELLO,CONFER
```

`CONFER` will present a welcome dialog and invite you to begin interacting with it. For
example, to send a public message to everyone in your conference, simply enter the text
you want to send, then enter an empty line to send it. To send a personal message to a
specific user, use the `/whisper` (abbreviated `/w`) command, as in:

```
What are your plans today?
/w Name of User
```

The `/whisper` command can also be used to send a message to a specific user on another
host in the NJE network, as in:

```
What are your plans today?
/w Guest@NCCMAX
```

Note the syntax in this case: *username***@***nodename*, where *username* is the 1 - 8
character username of the remote NJE user, and *nodename* is the 1 - 8 character name
of the NJE user's host computer system.

While you are logged into `CONFER`, users logged into other hosts in the NJE network may
send personal messages to you too. They address you using your NOS login username and the
NJE node name of your NOS 2.8.7 system. For example, if you are logged into `CONFER` as
username `GUEST`, and your system's NJE node name is `NOSHOBBY`, a user of an IBM VM/CMS
system running RSCS could send you a personal message by entering the following command:

```
smsg rscs m noshobby guest What's up, doc?
```

Users on other NJE nodes may also send public messages and commands to `CONFER` by
addressing them to the service named `CHAT` at your host's NJE node name. For example, a
user on an IBM VM/CMS node running RSCS could send a public message to users of `CONFER`
on node `NCCMAX` by entering the following command:

```
smsg rscs m nccmax chat Is anyone attending the symposium this weekend?
```

Commands begin with the character **/**. To ask for help information from `CONFER` on
node `NCCMAX`, an IBM VM/CMS user would enter the following command:

```
smsg rscs m nccmax chat /help
```

Similarly, a user of a remote NOS 2.8.7 system could ask for information about who is
using `CONFER` on node `NCCMAX` by entering the following command:

```
NJMSG.CHAT@NCCMAX /SHOW
```

### <a id="njf-vs-tlf"></a>NJF vs TLF
Both NJF and TLF can send jobs from NOS 2.8.7 to other mainframes for execution, so
why use one instead of the other? NJF uses the NJE protocol to communicate with other
systems, and TLF uses the HASP protocol. NJE is a kind of HASP next generation. NJE
establishes a symmetrical peer relationship between two participating systems, while
HASP defines one participant to be a *station* and the other to be a *host*. NJE
allows both participants to send jobs, messages, and/or files to each other, while HASP
allows only a *station* to send jobs to a *host* and a *host* to send output to a
*station*, and HASP does not support messaging.

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
files. BITNet was the predominant world-wide e-mail network until the Internet obsoleted it around the mid-1990's.

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

## <a id="rhp"></a>NOS to NOS networking (RHP - Remote Host Products)
NOS 2 systems can communicate with each other using data communication protocols
created by Control Data. For example, the NPU's of two mainframes running NOS 2 can
be connected to each other, and this type of connection can be used to exchange
queued and permanent files between the systems. It can also be used more generally
for application to application communication between mainframes.

A point-to-point link between two NPU's is called a trunk. In a real environment,
a trunk was established by connecting a physical data communication port on one NPU to
a physical data communication port on the other NPU using a cable. Ordinarily, this
was a synchronous serial connection operating at 19.2 Kbps, or perhaps even 56 Kbps.

In a DtCyber environment, a virtual data communication port on one NPU is connected to a virtual data communication port on the other NPU using TCP/IP. Consequently, a
trunk between two DtCyber systems operates at modern networking speeds. It is an
effective substitute for Control Data's RHF/LCN (Remote Host Facility / Loosely Coupled
Network) technology of the 1970's and 1980's which operated at 50 Mbps.

The nodes in an RHP network were NPU's and hosts. A real Control Data mainframe
could be connected via channels to multiple NPU's. These were treated as point-to-point
network links. The NPU end of a channel was assigned a unique address, and the host end
of the channel was also assigned a unique address. These addresses were 8-bit numbers
in the range 1-255, and they were called *node numbers*.

DtCyber supports only one NPU (or MDI) connected by channel to a mainframe. This
conceptually simplifies RHP configuration a bit in a DtCyber environment. In
particular, it simplifies assignment of RHP network addresses (i.e., node numbers).
Each NPU (or MDI) must be assigned one unique node number, and each mainframe must be
assigned one unique number. The node number assigned to a mainframe is called a
*coupler node number* (it *couples* the mainframe to the network).

RHP-based NOS to NOS networking is configured and activated by adding `rhpNode`
definitions to the `[NETWORK]` section of the `site.cfg` file. For example, the
following `site.cfg` describes a network of two NOS 2 systems from the perspective
of the host identified as `NCCM11` with machine ID `11`:

```
[CMRDECK]
MID=11.
NAME=MAINFRAME ELEVEN
[NETWORK]
hostID=NCCM11
rhpNode=NCCM11,M11,192.168.0.17:2550,1,2,NCCM12,1
rhpNode=NCCM12,M12,192.168.0.19:2550,3,4,NCCM11,1
```
The parameter values specified in the first `rhpNode` definition are:

| Parameter Value | Meaning           |
|--------------|-------------------|
| NCCM11 | Identifier of the host being described |
| M11    | 3-character NOS LID (Logical Identifier) assigned to the host. Ordinarily, this is the same as the host's PID (Physical Identifier) which is the letter `M` followed by the host's 2-character machine identifier, as defined by the MID value in the [CMRDECK] section. |
| 192.168.0.17:2550 | The TCP address on which the host listens for RHP connections |
| 1 | The node number assigned to the mainframe (i.e., the host coupler node number) |
| 2 | The node number assigned to the mainframe's NPU |
| NCCM12 | The identifier of a host to which host NCCM11 is linked |
| 1 | The NPU port number on NCCM11's NPU used for creating the trunk to NCCM12's NPU |

The second `rhpNode` definition specifies information about host NCCM12. 

The site.cfg file associated with NCCM02's instance of DtCyber would look like the
following. Note that the `rhpNode` definitions are identical to the ones in NCCM11's
site.cfg file because they describe the same network.

```
[CMRDECK]
MID=12.
NAME=MAINFRAME TWELVE
[NETWORK]
hostID=NCCM12
rhpNode=NCCM11,M11,192.168.0.17:2550,1,2,NCCM12,1
rhpNode=NCCM12,M12,192.168.0.19:2550,3,4,NCCM11,1
```
Note that the rightmost elements of an `rhpNode` definition are pairs of host
identifiers and NPU port numbers, and more than one such pair can occur in a
definition. This enables describing the topologies of networks having more than
two hosts. For example, the following definitions describe an RHP network of three NOS
hosts where each host is directly connected to the other two hosts:

```
[CMRDECK]
MID=11.
NAME=MAINFRAME ELEVEN
[NETWORK]
hostID=NCCM11
rhpNode=NCCM11,M11,192.168.0.29:2550,1,2,NCCM12,1,NCCM13,2
rhpNode=NCCM12,M12,192.168.0.17:2550,3,4,NCCM11,1,NCCM13,2
rhpNode=NCCM13,M13,192.168.0.19:2550,5,6,NCCM11,1,NCCM12,2

```
### <a id="usingrhp"></a>Using RHP
RHP (Remote Host Products) enables you to exchange jobs, queued files, and permanent
files between NOS 2 systems. Furthermore, if you send a job from one system to another,
RHP will arrange for the job's output to be returned automatically to the originating
system. RHP will preserve all file structure and content in files sent between NOS 2
systems.

Use the `ST` parameter of the NOS `ROUTE` command to send a job or queued file to
another system. For example:

```
ROUTE,lfn,DC=TO,ST=M02.
```

will cause file *lfn* to be sent to execute on the host with logical identifier `M02`.
Because `DC=TO` is specified, job's output will be returned to the originating system's
wait queue. If `DC=IN` had been specified instead, the job's output would be returned
to the originating system's print queue.

Similarly, to send a file to the wait queue of a specific user on another host, you
could execute a command such as:

```
ROUTE,lfn,DC=WT,ST=M02,UN=usernam.
```
Use the NOS `MFLINK` command to send or retrieve permanent files from other hosts.
You can also use `MFLINK` to delete permanent files on other hosts. For example, to
send a file to another host and save it as a permanent file, use `MFLINK` as in:

```
MFLINK,lfn,ST=M02
* USER,usernam,passwor
* SAVE,pfn
*
```
To retrieve a file:
```
MFLINK,lfn,ST=M02
* USER,usernam,passwor
* GET,pfn
*
```
See [NOS 2 Reference Set Volume 3 System Commands](http://bitsavers.trailing-edge.com/pdf/cdc/Tom_Hunter_Scans/NOS_2_Reference_Set_Vol_3_System_Commands_60459680L_Dec88.pdf)
for more details about the `ROUTE` and `MFLINK` commands.

## <a id="tcpip"></a>TCP/IP Applications
NOS 2.8.7 supports the TCP/IP data communication protocol suite and various client and server
applications that use it. These include:

- **DNS lookup utility**. Included in the *ncctcp* product provided by the Nostalgic Computing Center.
- **FTP client and server**. Provided by CDC and included in the standard NOS 2.8.7 release materials.
- **HTTP server**. Included in the *ncctcp* product provided by the Nostalgic Computing Center.
- **REXEC client and server**. Included in the *ncctcp* product provided by the Nostalgic Computing Center.
- **SMTP client and server**. Included in the *ncctcp* product provided by the Nostalgic Computing Center.

See [TCP/IP Applications](TCPIP.md) for details.

## <a id="cray"></a>Cray Station
NOS 2.8.7 supports the `Cray Station` subsystem (CRS), and this enables it to interact
with a Cray supercomputer. The implementation in *DtCyber* interoperates with Andras
Tantos' [Cray-XMP emulator](https://github.com/andrastantos/cray-sim) running the COS
1.17 operating system. The `Cray Station` software supports exchange of jobs and files,
and it also supports interactive login to COS 1.17 via the NOS 2.8.7 application named
`ICF` (Interactive Cray Facility).

The ready-to-run NOS 2.8.7 package does not have CRS enabled by default as enabling
CRS requires some site-specific configuration. This is accomplished by adding a
`crayStation` entry to the `[NETWORK]` section of `site.cfg`, the site configuration
file. For example:

```
crayStation=NCCXMP,XMP,24,192.168.0.28:9000,SFE,CC1
```

The entry, above, specifies the following:

| Parameter Value | Meaning           |
|--------------|-------------------|
| NCCXMP | Name assigned to the Cray Station interface (used only in configuration file comments) |
| XMP    | LID (Logical IDentifier) associated with the Cray Station interface |
| 24     | The channel to which the Cray frontend device is attached |
| 192.168.0.28:9000 | The TCP address on which the Cray supercomputer emulator listens for station connections |
| SFE    | The station identifier, as known to the Cray mainframe, is `FE` |
| CC1    | The Cray mainframe identifier, as known to the Cray mainframe, is `C1` |

See [Customizing the NOS 2.8.7 Configuration](#reconfig) for additional details about
site-specific configuration.

To configure and activate the Cray Station subsystem, perform the following steps:

1. Add a `crayStation` entry like the example, above, to the `[NETWORK]` section of
the `site.cfg` file
2. If *DtCyber* was started using `node start`, run the `reconfigure.js` tool by
executing the command `reconfigure` at the `Operator>` prompt. Otherwise, run the
tool by executing the command `node reconfigure`.

### <a id="cos-tools"></a>COS Tools
NOS 2.8.7 supports an optionally installed product named `cos-tools`. This product is _not_
installed by default. It requires _Cray Station_ to be installed, configured, activated, and
connected to a Cray X-MP system running COS 1.17.

Use the `install-product.js` tool to install the `cos-tools` product. If *DtCyber* was
started using `node start`, run the `install-product.js` tool by executing the command
`install -f cos-tools` at the `Operator>` prompt. Otherwise, run the tool by executing the
command `node install-product -f cos-tools`.

Installing `cos-tools` causes the following to occur:

- A collection of tools and utilities (see details, below) is installed on the connected
Cray X-MP system.
- A CCL procedure library named `CRAY` is saved in the catalog of user INSTALL. This
library contains handy procedures for performing tasks such as transferring files to the
Cray X-MP system, installing software there, assembling/compiling/running programs there, etc.
Further details are provided, below.
- A CCL procedure library named `CRAY` is saved in the LIBRARY catalog. This procedure library
is a subset of the one installed in INSTALL's catalog. However, it is publicly available to
all other users of the NOS 2.8.7 system.

The COS 1.17 operating system image provided with Andras Tantos'
[Cray-XMP emulator](https://github.com/andrastantos/cray-sim) was recovered from a physical
disk originally installed on an actual Cray X-MP computer. This is the only surviving COS 1.17
image currently known to exist. It includes the base operating system only. Unfortunately, it
does not include any programming language compilers, and it also lacks many useful commands
supported by COS.

The `cos-tools` product provides reproductions of many of these missing features. These
reproductions originate from the GitHub [COS Tools](https://github.com/kej715/COS-Tools)
repository. The tools and utilities installed by `cos-tools` on the Cray X-MP system include:

- __CAL__ : Cray Assembly Language assembler
- __DASM__ : a disassembler
- __KFTC__ : FORTRAN 77 compiler
- __LDR__ : linking loader
- __LIB__ : library manager
- __LISPF4__ : interpreter of the InterLisp dialect of the LISP programming language
- __CHARGES__ : utility run automatically by COS at the end of each job to report resource consumption information
- __COPYD__ : copies blocked datasets
- __COPYF__ : copies files of blocked datasets
- __COPYR__ : copies records of blocked datasets
- __NOTE__ : writes text to a dataset
- __SKIPD__ : skips blocked datasets
- __SKIPF__ : skips files of blocked datasets
- __SKIPR__ : skips records of blocked datasets

Visit the [COS Tools](https://github.com/kej715/COS-Tools) repository for additional details.

The `CRAY` CCL procedure library installed in the catalog of INSTALL includes the following
interactive CCL procedures:

- __AUDIT__ : produces a listing of all permanent files stored on the Cray X-MP system
- __CAL__ : assembles, links, and executes an assembly language program on the Cray X-MP system
- __DELETE__ : deletes a permanent file stored on the Cray X-MP system
- __FTN__ : compiles, links, and executes a FORTRAN 77 program on the Cray X-MP system
- __INSTALL__ : installs an executable file as a command on the Cray X-MP system
- __LISP__ : runs a LISP session on the Cray X-MP system
- __QGET__ : retrieves a file from the NOS wait queue and converts it from 8/12 ASCII encoding to 6/12 extended display code
- __REPLACE__ : replaces a permanent file on the Cray X-MP system
- __RUN__ : transfers an executable file to the Cray X-MP system and runs it there
- __SAVE__ : saves a permanent file on the Cray X-MP system

The `CRAY` CCL procedure library installed in the LIBRARY catalog includes the __CAL__,
__FTN__, __LISP__, and __QGET__ procedures described above.

For example, after installing the `cos-tools` product, a file on NOS 2.8.7 named `HELLO` containing a FORTRAN 77 program may be sent to the Cray X-MP system to be compiled and
executed there by entering the following NOS command:

```
BEGIN,FTN,CRAY,I=HELLO.
```
This will submit a job to the Cray X-MP, and the job's output will be returned to the user's
wait queue. The output may be retrieved from the wait queue and converted to 6/12 Extended
Display Code by entering:

```
BEGIN,QGET,CRAY,<jsn>.
```
where *&lt;jsn&gt;* is the JSN of the output file returned to the wait queue.

The __FTN__ procedure also supports sending a data input file along with a FORTRAN program
(e.g., to be read as input by the program). For example:
```
BEGIN,FTN,CRAY,I=HELLO,D=HLODATA.
```
In this case, the contents of HLODATA will be associated with the __$IN__ dataset on COS.
To send a data file and associate it with a different local dataset on COS (e.g., to be
opened using a FORTRAN _OPEN_ statement), use the __DN__ parameter to provide the name of
the local dataset. For example:
```
BEGIN,FTN,CRAY,I=HELLO,D=HLODATA,DN=DATA.
```
The __LISP__ procedure operates similarly. To run a __LISP__ session on COS, call the
procedure as in:
```
BEGIN,LISP,CRAY,I=EXPRS.
```
where _EXPRS_ is a NOS file containing expressions to be read and executed by the LISP
interpreter. Output of the LISP job will be returned by default to the user's wait queue, and
the __QGET__ procedure can then be used to retrieve it.

The __LISP__ procedure also accepts a __D__ parameter. This can be used to specify the name
of a NOS file that will be sent to COS along with the LISP job. The contents of the NOS file
will be copied to a local dataset with the same name on the COS side. Features of LISP can
then be used to open and read the file.

Note also that, as a general rule, all text files sent to COS using these procedures are
assumed to be encoded in 6/12 Extended Display Code, so they may contain mixed case.

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
- `mail_configure` (alias `mailc`) : applies the NJE topology definition and customized
configuration parameters to the UMass Mailer and e-mail routing system.
See [UMass Mailer](#mailer) for details.
- `make_ds_tape` (alias `mdt`) : creates a new deadstart tape that includes products
installed by `install_product`.
- `modopl` : applies all local modifications to the NOS system source library, `OPL871`. This command is particularly useful when new operating system modifications have been added to the DtCyber repository, and you want to update a previously installed system to include them.
- `njf_configure` (alias `njfc`) : applies the NJE topology definition and customized
configuration parameters to the system. See [Network Job Entry](#nje) for details.
- `reconfigure` (alias `rcfg`) : applies all customized system configuration
parameters including mail, NJE, RHP, and TLF parameters, creates a new deadstart tape,
shuts down, and then re-deadstarts the system to activate the updated configuration.
See [Customizing the NOS 2.8.7 Configuration](#reconfig) for details.
- `rhp_configure` (alias `rhpc`) : applies the RHP topology definitions and customized
configuration parameters to the system. See [Remote Host Products](#rhp) for details.
- `shutdown` : initiates graceful shutdown of the system.
- `sync_tms` : synchronizes the NOS Tape Management System catalog with the
cartridge tape definitions specified in the `volumes.json` configuration file
found in the `stk` directory. See [stk/README.md](../stk/README.md) for details.
- `tlf_configure` (alias `tlfc`) : applies the TLF node definitions and customized
configuration parameters to the system. See [TieLine Facility](#tlf) for details.

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
| [cybis-base](https://www.dropbox.com/s/jiythdoifn1f6bm/cybis-bin.tap?dl=1)                         | The base Cyber Instructional System                     |
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
| [cos-tools](https://www.dropbox.com/scl/fi/4wisss3zo5ydnpagk50ko/cos-tools.tap?rlkey=t9qtypvv6u1bjt6yfljnq2i71&dl=1) | Programming tools and utilities for COS 1.17 (Cray Operating System) |
| [crs](https://www.dropbox.com/s/olr1hz6ys5mavjt/cray-station.tap?dl=1) | Cray Station subsystem |
| [kermit](https://www.dropbox.com/s/p819tmvs91veoiv/kermit.tap?dl=1) | Kermit file exchange utility |
| [mailer](https://www.dropbox.com/s/y2yumlzqjc4qva8/massmail.tap?dl=1) | UMass Mailer, base e-mail system |
| nccnje  | Messaging service extensions for `njf` |
| [ncctcp](https://www.dropbox.com/s/m172wagepk3lig6/ncctcp.tap?dl=1) | TCP/IP Applications (HTTP, NSQUERY, REXEC, SMTP) |
| [netmail](https://www.dropbox.com/s/y2yumlzqjc4qva8/massmail.tap?dl=1) | UMass Mailer, network mail router |
| [njf](https://www.dropbox.com/s/oejtd05qkvqhk9u/NOSL700NJEF.tap?dl=1) | Network Job Facility |
| rbf5    | Remote Batch Facility Version 5 |
| rhp     | Remote Host Products (QTF/PTF)  |
| [tlf](https://www.dropbox.com/s/r5qbucmw6qye8vl/tieline.tap?dl=1) | TieLine Facility |

### Category *games*
This category includes games. In most cases, the games run on the system console. In some
cases (e.g., Chess games), the games can also be run at an ordinary user terminal.

| Product  | Description |
|----------|-------------|
| cgames   | NOS Console Games (EYE, KAL, LIFE, LUNAR, MIC, PAC, SNK, TTT) |
| [chess30](https://www.dropbox.com/scl/fi/8hkccuwebfa8ebzq82130/chess30.tap?rlkey=m06hk1pg6oy50et1679p22pew&dl=1) | CHESS 3.0 - [historic Chess game](https://www.chessprogramming.org/Chess_%28Program%29) |
| [chess35](https://www.dropbox.com/scl/fi/iomq72j2f67vibltpefzx/chess35.tap?rlkey=qnxgybhq5l60wkzhqiil6to2n&dl=1) | CHESS 3.5 - [historic Chess game](https://www.chessprogramming.org/Chess_%28Program%29) |
| [chess46](https://www.dropbox.com/scl/fi/amc8hurdu7q5ijx9yy85w/chess46.tap?rlkey=5jinr6rzowzk2p43lzhdtcbok&dl=1) | CHESS 4.6 - [historic Chess game](https://www.chessprogramming.org/Chess_%28Program%29) |
| [chess49](https://www.dropbox.com/scl/fi/4kuwf7keau3z0ff1tdn5d/chess49.tap?rlkey=uq9b5x9la3twuiiwpkae6trhr&dl=1) | CHESS 4.9 - [historic Chess game](https://www.chessprogramming.org/Chess_%28Program%29) |

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
| [i8080](https://www.dropbox.com/s/ovgysfxbgpl18am/i8080.tap?dl=1) | Intel 8080 tools (CPM80, INTRP80, MAC80, PLM80) |
| skedulr | Task scheduler (similar to *cron* in Linux/Unix systems) |
| [spss](https://www.dropbox.com/s/2eo63elqvhi0vwg/NOSSPSS6000V9.tap?dl=1) | SPSS-6000 V9 - Statistical Package for the Social Sciences |

These lists will grow, so revisit this page to see what new products have been added,
use `git pull` to update the lists in your local repository clone, and then use
`install all` to install them.

## <a id="newds"></a>Creating a New Deadstart Tape  
The `install_product` tool and the various configuration management tools insert
the records they produce into the direct access file named `PRODUCT` in the catalog of
user `INSTALL`, and some tools also update the file named `DIRFILE` to specify the
system libraries with which the binaries are associated. To create a new deadstart tape
that includes the contents of `PRODUCT`, execute the following command:

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
installed, one by one, and this involves building many of them from source code.

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

or

>Operator> `install ncctcp`

In case a basic installation is interrupted before completing successfully, use the
`continue` option to proceed from the point of interruption, as in:

| OS           | Commands                           |
|--------------|------------------------------------|
| Linux/MacOS: | `sudo node install basic continue` |
| Windows:     | `node install basic continue`      |

## <a id="upgrade875"></a>Upgrading Cyber 865 to Cyber 875
The `basic` and `full` installation options create systems that run on a Cyber 865 machine
with a full complement of central memory (1M words). Ordinarily, this is sufficient for
most hobbyist usage. A Cyber 875, however, can be configured with four times as
much central memory as a Cyber 865 (4M words vs 1M words). In cases where CYBIS will be
running and/or it is desirable to support a large number of concurrent batch and/or
interactive jobs, a Cyber 875 will probably deliver better performance than an 865.

It is possible to upgrade a Cyber 865 image to a Cyber 875 image by running the
`upgrade-to-875` tool, as in:

| OS           | Commands                   |
|--------------|----------------------------|
| Linux/MacOS: | `sudo node upgrade-to-875` |
| Windows:     | `node upgrade-to-875`      |

Run this tool when *DtCyber* is **not** already running. The tool will modify the
*DtCyber* and NOS 2.8.7 configurations to run on a Cyber 875. In the process of making
the necessary changes, it will deadstart the machine a couple of times, and it will leave
the system running as a Cyber 875. Subsequent deadstarts will bring the system up as a
Cyber 875 as well.

## <a id="reconfig"></a>Customizing the NOS 2.8.7 Configuration
Various parameters of the NOS 2.8.7 system configuration may be changed or added to
accommodate local requirements and personal preferences. For example:
- Definitions in the system CMR deck may be updated to change parameters such as the
machine identifier and system name.
- Definitions may be updated or added to the system equipment deck to add peripheral
equipment or change their parameters.
- The system's IP address may be defined.
- The TCP/IP hosts statically known to the system may be defined.
- The TCP/IP resource resolver configuration may be defined.
- The network node name of the local host may be defined.
- Private nodes in a local NJE network may be defined.
- Networks of NOS 2 hosts supporting CDC Remote Hosts Products protocols may be
described.
- Remote hosts to which the local host may connect using HASP may be defined. 
- Internet e-mail domains may be declared.
- The Cray Station interface may be configured and activated.
- etc.

A tool named `reconfigure.js` applies customized configuration. The tool looks for a
file named `site.cfg` in the current working directory. This file is where all
customized system configuration parameters are expected to be defined. If *DtCyber*
is started using the command `node start`, the tool may be called by entering the
command `reconfigure` at the *DtCyber* `Operator>` prompt. Otherwise, the tool
may be called as in:

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
include special entries for NOS 2.8.7 hosts. These include an alias of the form:

```
LOCALHOST_id
```

where *id* is the 2-character machine identifier (i.e., `MID` value in `[CMRDECK]`) of
the respective NOS 2.8.7 system. At least one entry in the system's TCPHOST file
(or this `[HOSTS]` section) must include this special alias for the local NOS 2.8.7
system. This entry enables the NOS system to discover its own IP address.

**The entry with the special `LOCALHOST_id` alias also informs *DtCyber* of its IP
address, and it is the source IP address to which *DtCyber* will bind all of its
network sockets.** See also the [networkInterface](#networkInterface) definition in the
[[NETWORK]](#network) section. The [networkInterface](#networkInterface) definition
informs *DtCyber* of a command that it can execute to automatically start/stop a
network interface supplying this IP address.

In addition, one entry in the `TCPHOST` file (or this `[HOSTS]` section) must include
the special alias `STK`. This enables the NOS 2.8.7 system to discover the
IP address of the StorageTek 4400 cartridge tape server and RPC ONC port mapper.

To enable the UMass Mailer to use the SMTP e-mail protocol for routing messages to
other SMTP-based hosts, the `TCPHOST` file (or this `[HOSTS]` section) should also
include one entry that has the special alias `mail-relay`. This informs the
UMass Mailer where to send e-mail for SMTP-based hosts. It assumes that the machine
with this alias is capable of relaying mail to other SMTP-based destinations.
Typically, Unix hosts are capable of serving as SMTP mail relays.

Example:

```
[HOSTS]
192.168.0.17 nccmax nccmax.nostalgiccomputing.org max LOCALHOST_AX STK
192.168.0.29 nccmed nccmed.nostalgiccomputing.org med LOCALHOST_ED
192.168.0.19 nccmin nccmin.nostalgiccomputing.org min LOCALHOST_IN
192.168.1.2  vax1   vax1.nostalgiccomputing.com
192.168.1.3  vax2   vax2.nostalgiccomputing.com
192.168.1.4  rsx11m rsx11m.nostalgiccomputing.com
192.168.1.5  tops20 tops20.nostalgiccomputing.com pdp10
192.168.2.2  bsd211 bsd211.nostalgiccomputing.com pdp11 mail-relay
192.168.2.3  unicos unicos.nostalgiccomputing.com sv1
```

### <a id="network"></a>[NETWORK]
Defines site-specific routing information and parameters related to data communication
and networking. The configuration and operation of various NOS 2.8.7 data communication
subsystems derives from this section. This includes:

- **CRS**. Configuration information for the `Cray Station` interface.
- **HASP**. Information about mainframes to which NOS 2.8.7 can connect using
IBM's `HASP` data communication protocol.
- **Host ID**. The identifier of the local NOS 2.8.7 host.
- **Network Interface**. Declaration of a network interface used by the host computer
to support *DtCyber's* data communication.
- **NJE**. Information defining private routes in a network of mainframes
interconnected by IBM's `NJE` (Network Job Entry) data communication protocol.
- **RHP**. Information defining the topology of a network of mainframes running
NOS 2.8.7 and interconnected by Control Data's `RHP` (Remote Host Products) data
communication protocols.
- **SMTP**. Information about e-mail destinations that can be reached using the
TCP/IP `Simple Mail Transfer Protocol`.
- **STK**. Information about the StorageTek 4400 cartridge tape server.

Example:

```
[NETWORK]
hostID=MARS
rhpNode=MARS,MAR,localhost:2551,1,2,VENUS,1
rhpNode=VENUS,MVE,192.168.0.17:2551,3,4,MARS,1
crayStation=CRAYXMP,XMP,24,192.168.0.28:9001
tlfNode=JES2,JES,JES2,192.168.0.17:37803,R001
```

The entries that may occur in the `[NETWORK]` section include:

- <a id="crayStation"></a>**crayStation** : Activates the Cray Station interface and
defines parameters associated with it. The general syntax of this entry is:

    crayStation=*name*,*lid*,*channel*,*addr*,[,S*station-id*][,C*cray-id*]
    
    | Parameter     | Description |
    |---------------|-------------|
    | name          | The name assigned to the interface. |
    | lid           | The unique, 3-character logical identifier assigned to the interface. This is used in commands that route files and jobs to the Cray mainframe connected by the interface. |
    | channel | The number of the DtCyber channel to which the Cray frontend interface is connected. |
    | addr | The IP address and TCP port number on which the Cray mainframe emulator listens for connections. |
    | station-id | Optional. Specifies the 2-character identifier by which the station is known to the Cray mainframe. The default is `FE`. |
    | cray-id | Optional. Specifies the 2-character identifier by which the Cray mainframe is known to the Cray mainframe itself. The default is `C1`. |
    
	Example:
	
	`crayStation=NCCXMP,XMP,24,192.168.0.28:9000`

- <a id="defaultRoute"></a>**defaultRoute** : Specifies the name of the NJE node to
which jobs and files should be routed by default. Typically, this is the name of the
adjacent node serving as the local system's primary link to the public NJE network.
Example:

    `defaultRoute=NCCMAX`
    
	Ordinarily, this entry is needed only when an NJE node has multiple directly
	connected neighbors, so one of them needs to be designated as the node to which
	files will be sent when the route to a destination cannot be calculated
	automatically using the [topology file](files/nje-topology.json).
    
- <a id="haspPorts"></a>**haspPorts** : Specifies the range of CLA ports that will be
used in defining additional HASP terminals for use by RBF in the NOS NDL. The general
syntax of this entry is:

    haspPorts=*cla-port-number*,*port-count*
    
    Where *cla-port-number* is the number of the first CLA port to be used, and
    *port-count* defines the maximum number of ports to be used. The default is
    equivalent to:
    
    `haspPorts=0x26,2`
    
- <a id="haspTerminal"></a>**haspTerminal** : Defines the name and connection information
for a HASP terminal. Two generic HASP terminals are defined in the base system, and this
entry allows you to define additional ones. The general syntax of this entry is:

    haspTerminal=*username*,*tcp-port*[,B*block-size*][,T*terminal-class*]
    
    | Parameter  | Description |
    |------------|-------------|
    | username   | The username associated with the terminal. |
    | tcp-port   | The TCP port on which DtCyber will listen for connections to this terminal. |
    | block-size | Optional block size, in bytes, to use in communicating with peers. The default is 400. |
    | terminal-class | Optional terminal class name. Usually one of **HPRE** or **HASP**. **HPRE** is the default and specifies that the HASP station supports pre-print format control. **HASP** specifies that the HASP station supports only post-print format control. |

    Examples:
    
    `haspTerminal=MOE,2555`
    
    `haspTerminal=PST,2557,THASP`
    
- <a id="hostID"></a>**hostID** : Specifies the 1 - 8 character node name of the local
host. Example:

    `hostID=MARS`
    
- <a id="networkInterface"></a>**networkInterface** : Defines the name of the network
interface used by the host computer to support *DtCyber's* data communication and,
optionally, the command to be called for starting and stopping the interface.
Ordinarily, this needs to be specified only when *DtCyber's* IP address differs from
the primary IP address of the host computer on which it is running, and it is
necessary or desirable to start/stop the interface dynamically. See the description
of the [[HOSTS]](#hosts) section for details about defining *DtCyber's* IP address.

	The general syntax of this entry is:

	networkInterface=*device*[,*manager*]
	
    | Parameter | Description |
    |-----------|-------------|
    | device    | The name of the network interface device on the host computer, e.g. `tap0`. |
    | manager   | Optional pathname of a command or script that will be called to start and stop the interface. The default is `../ifcmgrs/tapmgr.type` where `type` is one of `bat`, `linux`, or `macos`, depending upon the type of operating system on which *DtCyber* is running. *DtCyber* will pass the interface device name and IP address (from the [HOSTS] section) as parameters. |

	Example:

    `networkInterface=tap0`

- <a id="njeNode"></a>**njeNode** : Defines the name and routing information for an NJE
node within a private NJE network (i.e., a node that is not registered in the public
topology, [files/nje-topology.json](files/nje-topology.json). The general syntax of
this entry is:

    njeNode=*nodename*,*software*,*lid*,*public-addr*,*link*[,*local-addr*][,B*block-size*][,P*ping-interval*][,*mailer-address*]
    
    | Parameter     | Description |
    |---------------|-------------|
    | nodename      | The unique 1 - 8 character name of the node. |
    | software      | The name of the NJE software used by the node; one of NJEF, RSCS, NJE38, or JNET. |
    | lid           | The unique, 3-character logical identifier assigned to the node. |
    | public-addr   | The public IP address and TCP port number on which the node listens for NJE connections. The format is `ipaddress:portnumber`, e.g., 192.168.1.3:175. If the node does not have a public address, 0.0.0.0:0 is specified. |
    | link          | The name of the NJE node that serves as the defined node's primary link to the NJE network. |
    | local-addr    | Optional IP address of the local node. If specified, this address will be used by the local node to identify itself to the defined node. If omitted, an appropriate default will be chosen based upon the local system's physical TCP network configuration. |
    | block-size    | Optional block size, in bytes, to use in communicating with the defined node. The default is 8192. |
    | ping-interval | Optional ping interval, in seconds, to use in keeping the NJE connection alive. The default is 600 (i.e., 10 minutes). |
    | mailer-address | Optional address of a BSMTP mail server on the defined node, in the form `mailername@nodename`, e.g., **MAILER@NCCMAX** |

    Example:
    
    `njeNode=LOCALCMS,RSCS,CMS,192.168.1.3:175,MYNODE,B8192,MAILER@LOCALCMS`
    
- <a id="njePorts"></a>**njePorts** : Specifies the range of CLA ports on the local
NPU that will be used in defining terminals for use by NJF in the NOS NDL. The general
syntax of this entry is:

    njePorts=*cla-port-number*,*port-count*
    
    Where *cla-port-number* is the number of the first CLA port to be used, and
    *port-count* defines the maximum number of ports to be used. The default is
    equivalent to:
    
    `njePorts=0x30,16`
    
- <a id="rhpNode"></a>**rhpNode** : Defines the name and linkage information for a node
in a Control Data `Remote Host Products` network. RHP provides the QTF (Queued file
Transfer Facility) and PTF (Permanent file Transfer Facility) applications that enable
NOS 2.8.7 systems to exchange jobs and files. The general syntax of this entry is:

    rhpNode=*nodename*,*lid*,*addr*,*coupler-node*,*npu-node*,*peername*,*cla-port*[,*peername*,*cla-port*...]
    
    | Parameter     | Description |
    |---------------|-------------|
    | nodename      | The name assigned to the node. |
    | lid           | The unique, 3-character logical identifier assigned to the node. |
    | addr          | The IP address and TCP port number on which the node listens for RHP connections. The format is `ipaddress:portnumber`, e.g., 192.168.0.17:2551. |
    | coupler-node  | The node number of the channel coupler that links the node's NPU to its mainframe. Essentially, this is the RHP node number of the mainframe |
    | npu-node      | The node number of the node's NPU itself. |
    | peername      | The name of a peer to which the mainframe is directly linked. This must match the *nodename* parameter of another *rhpNode* definition |
    | cla-port      | The number of the CLA port on the mainframe's NPU that is used for communicating with the peer. |
    
	Note that multiple *peername*/*cla-port* pairs may be defined to indicate that a node
is connected to multiple peers.

- <a id="safNode"></a>**safNode** : Defines the name and routing information for a
*store-and-forward* node. A store-and-forward node is a node that is reached by forwarding
files to an adjacent node serving as an intermediate relay. Ordinarily, the relay node
is an RHP node defined in an [rhpNode](#rhpNode) entry. Often, the store-and-forward node
is a TLF node or a Cray station directly connected to the relay node.

	The general syntax of this entry is:

    safNode=*nodename*,*relay-node*,*lid*

    | Parameter     | Description |
    |---------------|-------------|
    | nodename      | The name assigned to the store-and-forward node. |
    | relay-node    | The name assigned to the intermediate relay node through which the sotre-and-forward node is reached. |
    | lid           | The unique, 3-character logical identifier assigned to the store-and-forward node. |

    Example:
    
    `safNode=NCCCMS,NCCMIN,CMS`
    
- <a id="smtpDomain"></a>**smtpDomain** : Identifies an internet domain name or suffix
to which e-mail will be routed using the TCP/IP SMTP protocol. Examples:

```
    smtpDomain=.com
    smtpDomain=.org
    smtpDomain=.net
    smtpDomain=some.host.org
```

- <a id="stkDrivePath"></a>**stkDrivePath** : Defines the path of the first automated
cartridge tape drive used by this NOS 2.8.7 system on the StorageTek 4400 tape server.
Optionally, also devices the TCP port on which the tape server listens for connections.
The general syntax of the entry is:

    stkDrivePath=[*tcp-port*/]M*module*P*panel*D*drive*
    
    | Parameter | Description |
    |-----------|-------------|
    | tcp-port  | The TCP port on which the StorageTek 4400 tape server listens for connections. Ths parameter is optional, and the default is 4400 |
    | module | The module number of the first tape drive used by this system (0 - 17 octal) |
    | panel  | The panel number of the first tape drive used by this system (0 - 13 octal) |
    | drive  | The drive number of the first tape drive used by this system (0 - 3) |

The default is:
```
    stkDrivePath=4400/M0P0D0
```
meaning the tape server listens for connections on TCP port 4400, and the first automated
cartridge tape drive used by this system is Module 0, Panel 0, Drive 0. The module, panel,
and drive numbers are octal values. Module numbers may range between 0 and 17, panel
numbers may range between 0 and 13, and drive numbers may range between 0 and 3.

The host on which the StorageTek 4400 tape server is running is defined by an entry in
the NOS 2.8.7 TCPHOST file, located in the catalog of user NETADMN. The entry having a
host alias **STK** identifies the host where the tape server is running. The default
is the local host (i.e., the host on which *DtCyber* itself is running). See also [[HOSTS]](#hosts) because this section, when present, defines the contents of the TCPHOST file.

When a StorageTek 4400 tape server is shared amongst a set of NOS 2.8.7 systems in an
RHP network, each system in the network needs to have a unique drive path (i.e.,
systems may share the tape server as a whole and its library of tapes, but they can not
share individual tape drives). Typically, it is sufficient to assign a unique
panel value to each NOS system. Example:

```
    stkDrivePath=M0P1D0
```

- <a id="tlfNode"></a>**tlfNode** : Defines the name and routing information for a TLF
node. The general syntax of this entry is:

    tlfNode=*nodename*,*lid*,*spooler*,*addr*[,B*block-size*][,R*remote-id*][,P*password*]
    
    | Parameter     | Description |
    |---------------|-------------|
    | nodename      | The human-readable name assigned to the node. |
    | lid           | The unique, 3-character logical identifier assigned to the node. |
    | spooler       | The type of output spooler used by the node; one of JES2, NOS, PRIME, or RSCS. |
    | addr   | The IP address and TCP port number on which the node listens for HASP connections. The format is `ipaddress:portnumber`, e.g., 192.168.0.17:37803. |
    | block-size    | Optional block size, in bytes, to use in communicating with peers. The default is 400. |
    | remote-id | Optional login identifier that TLF will present to the node when it connects. A typical value for MVS/JES2 is a 3-digit identifier like `001`. A typical value for VM/CMS/RSCS is a 2-digit identifier like `01`. The value to specify depends upon the target host's configuration. The default is that no login identifier is sent. |
    | password | Optional password that TLF will present to the node when it connects. The default is that no password is sent. |

    Example:
    
    `tlfNode=NCCJES2,JES,JES2,192.168.0.17:37803,R001`
    
- <a id="tlfPorts"></a>**tlfPorts** : Specifies the range of CLA ports that will be
used in defining terminals for use by TLF in the NOS NDL. The general syntax of this
entry is:

    tlfPorts=*cla-port-number*,*port-count*
    
    Where *cla-port-number* is the number of the first CLA port to be used, and
    *port-count* defines the maximum number of ports to be used. The default is
    equivalent to:
    
    `tlfPorts=0x28,8`

### <a id="passwords"></a>[PASSWORDS]
Enables passwords of user accounts in the NOS 2.8.7 system to be re/defined. Each line of
this section specifies a username and the password to be defined for it. The general syntax
of each line is:

*username*=*password*

The following example illustrates how the section can be used to customize the passwords
associated with usernames in a system. All of the usernames defined in a full, ready-to-run
system are shown along with the default passwords defined for each of them. To redefine
the passwords used in an installed system, you may copy this example to a `[PASSWORDS]`
section in your `site.cfg` file, edit the passwords that you want to change, then run
the `reconfigure.js` tool to change the edited passwords while the system is running.

Additionally, if a `[PASSWORDS]` section occurs in a `site.cfg` file that exists when a
ready-to-run system is first installed using the `install.js` tool with the `rtr` option,
the indicated password changes will be applied when `install.js` calls `reconfigure.js`
implicitly. Furthermore, if you use `install.js` to install a system from scratch (i.e., by
specifying the `full` option), the customized passwords will be applied during the scratch
installation.

Example:
```
[PASSWORDS]
BCSCRAY=CRAYOPN
CDCS=CDCS
CYBIS=CYBIS
CYBISMF=CYBISMF
DBCNTLX=DBCNTLX
GUEST=GUEST
INSTALL=INSTALL
NETADMN=NETADMN
NETOPS=NETOPSX
MAILER=MAILER
NJF=NJFX
PLATO=PLATO
PLATOMF=PLATOMF
PRINT01=PRINT01
PRINT02=PRINT02
PRINT03=PRINT03
PRINT04=PRINT04
PRINT05=PRINT05
PRINT06=PRINT06
PRINT07=PRINT07
PRINT08=PRINT08
PRINT09=PRINT09
PRINT10=PRINT10
PRINT11=PRINT11
PRINT12=PRINT12
PRINTS=PRINTS
REXEC=REXECX
RJE1=RJE1
RJE2=RJE2
SES=SESX
SYS=SYSX
SYSTEMX=SYSTEMX
TIELINE=TIELINE
WWW=WWWX
```

Note that the following usernames are associated with user indices having values greater than or equal to 377770B.
The system does not allow users to log into these accounts interactively, nor can they be used for running ordinary
batch jobs. A job may be run from these usernames only if it is initiated from the system console as a system origin job.
Consequently, there is little to be gained from changing the passwords of these accounts.

```
CYBISMF
NETOPS
PLATOMF
SYSTEMX
```

### <a id="resolver"></a>[RESOLVER]
Defines the TCP/IP resource resolver configuration. This is saved as the file named
TCPRSLV in the catalog of user NETADMN. The resource resolver is used by some
applications to look up the IP addresses of hosts dynamically. Example:

```
[RESOLVER]
search nostalgiccomputing.org
nameserver 192.168.0.19
```

## <a id="cfgexamples"></a>Customized Configuration Examples
The following example of the content of a `site.cfg` file defines a NOS machine
identifier and network host identifier for a machine, e.g., perhaps after registering
the machine in the public NJE network:

```
[CMRDECK]
MID=17.
[NETWORK]
hostID=CECCNOS1
```

The following example builds upon the previous example by adding a definition for a
HASP connection to an IBM MVS/JES2 system:

```
[CMRDECK]
MID=17.
[NETWORK]
hostID=CECCNOS1
tlfNode=NCCJES2,JES,JES2,192.168.0.17:37803,R001
```

The following example adds definitions describing an RHP network of three nodes:

```
[CMRDECK]
MID=17.
[NETWORK]
hostID=CECCNOS1
rhpNode=CECCNOS1,M17,84.0.25.17:2550,1,2,NCCMAX,1,UCCVENUS,2
rhpNode=NCCMAX,MAX,98.0.40.244:2550,3,4,CECCNOS1,1,UCCVENUS,2
rhpNode=UCCVENUS,M23,128.172.3.7:2550,5,6,CECCNOS1,2,NCCMAX,1
tlfNode=NCCJES2,JES,JES2,192.168.0.17:37803,R001
```

The following example adds a Cray Station interface:

```
[CMRDECK]
MID=17.
[NETWORK]
hostID=CECCNOS1
rhpNode=CECCNOS1,M17,84.0.25.17:2550,1,2,NCCMAX,1,UCCVENUS,2
rhpNode=NCCMAX,MAX,98.0.40.244:2550,3,4,CECCNOS1,1,UCCVENUS,2
rhpNode=UCCVENUS,M23,128.172.3.7:2550,5,6,CECCNOS1,2,NCCMAX,1
tlfNode=NCCJES2,JES,JES2,192.168.0.17:37803,R001
crayStation=CECCXMP,XMP,24,192.168.0.28:9001
```

The following example adds TCP/IP host definitions, a network interface declaration,
and a DNS resolver definition. One or more of the other *DtCyber* systems indicated by
the `LOCALHOST_id` entries might run on the same physical host machine, and those
machines `site.cfg` files would also have `networkInterface` definitions identifying
the names of the network interfaces they use. If the three instances of *DtCyber*
share the same content of the [HOSTS] section, then they would all share the
StorageTek 4400 tape server running on the system that is hosting the machine named
`CECCNOS2`.

```
[CMRDECK]
MID=17.
[NETWORK]
hostID=CECCNOS1
networkInterface=tap0
rhpNode=CECCNOS1,M17,84.0.25.17:2550,1,2,NCCMAX,1,UCCVENUS,2
rhpNode=NCCMAX,MAX,98.0.40.244:2550,3,4,CECCNOS1,1,UCCVENUS,2
rhpNode=UCCVENUS,M23,128.172.3.7:2550,5,6,CECCNOS1,2,NCCMAX,1
tlfNode=NCCJES2,JES,JES2,192.168.0.17:37803,R001
crayStation=CECCXMP,XMP,24,192.168.0.28:9001
stkDrivePath=M0P1D0
[HOSTS]
192.168.1.1 ceccnos1 nos1 nos1.cecc.org LOCALHOST_17
192.168.1.2 ceccnos2 nos1 nos1.cecc.org LOCALHOST_19 STK
192.168.1.3 ceccnos3 nos1 nos1.cecc.org LOCALHOST_23
192.168.2.1 vax1 vax1.cecc.org
192.168.2.2 bsd bsd.cecc.org mail-relay
192.168.2.3 unicos unicos.cecc.org
[RESOLVER]
search cecc.org
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
