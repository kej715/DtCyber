# Installing NOS 2.8.7
This directory tree contains a collection of artifacts facilitating the installation
of the NOS 2.8.7 operating system on *DtCyber*. 
Following these instructions should produce a working instance of the
operating system that supports:

- Interactive login
- Batch job submission via a simulated card reader
- Remote job entry (RJE)
- Network job entry (NJE)

Substantial automation has been provided in order to make the installation process
as easy as possible. In fact, it's nearly trivial compared to what was possible on
real Control Data computer systems back in the 1980's and 90's.

## Index
- [Prerequisites](#prereq)
- [Installation Steps](#steps)
- [Login](#login)
- [Remote Job Entry](#rje)
- [Network Job Entry](#nje)
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

To avoid competing with other hobbyists, you may request to register your system with
a unique node name in the public NJE network. Registration is accomplished by sending an
email to admin@nostalgiccomputing.org with a subject of `NJE registration request`. The
body of the email should contain these lines:

```
node: <requested-node-name>
link: <node-name-of-neighbor>
publicAddress: <IP-address-and-port>
software: <name-of-nje-software>
```
- **node** : Required. This is the name you want to be registered for your NJE node. It
must begin with a letter and can be up to 8 alphanumeric characters in length.
- **link** : Optional. This is the name of the NJE node to which your node will connect
in order to join the network. The **link** node must have a public address, and its
administrator must agree to have it serve as your **link* node. If this parameter is
not provided, the default is `NCCMAX`.
- **publicAddress** : Optional. If your NJE node has a fixed IP address that can be
reached from other nodes on the internet, then you may register it using this
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

Example:

```
node: SYZYGY
link: NEXTDOOR
publicAddress: 128.153.17.31:175
```

The file `NOS2.8.7/files/nje-topology.json` in this GitHub repository will be updated
in response to your registration request. In addition, an email reply will be sent to
you confirming the registration and providing a set of properties to be edited into your
site configuration file, `NOS2.8.7/site.cfg`. The property definitions will look like:

```
[CMRDECK]
MID=23
[NETWORK]
hostID=SYZYGY
```

These properties combined with the updated NJE topology file provide all of the
information needed to add your node with its unique name to the public NJE network.
To apply the information, follow these steps:

1. Execute `git pull` to update your repository, including the updated topology
definition.
2. Save the property definitions in the email reply to the file `NOS2.8.7/site.cfg`,
if the file doesn't exist already, or edit them into the existing file.
3. If you have started *DtCyber* using the `node start` command, enter the
`reconfigure` command at the `Operator>` prompt. Otherwise, execute the command
`node reconfigure` in the NOS2.8.7 directory. This will apply the new machine
identifier (MID in [CMRDECK] section) to your NOS system's configuration.
4. If you have started *DtCyber* using the `node start` command, enter the
`njf_configure` command at the `Operator>` prompt. Otherwise, execute the command
`node njf-configure` in the NOS2.8.7 directory. This will update your system's NDL
configuration to include NJE terminal(s), it will update your system's NJF host
configuration file, it will update your system's PID/LID configuration to include
PID's and LID's for adjacent NJE nodes, and it will update the *DtCyber* configuration
to include definitions for NJE terminal(s).
5. Shutdown the system and re-deadstart it. This will activate the updated NDL, HCF,
and PID/LID configurations.

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
| [kermit](https://www.dropbox.com/s/p819tmvs91veoiv/kermit.tap?dl=1) | Kermit file exchange utility |
| [ncctcp](https://www.dropbox.com/s/m172wagepk3lig6/ncctcp.tap?dl=1) | TCP/IP Applications (HTTP, NSQUERY, REXEC, SMTP) |
| [njf](https://www.dropbox.com/s/oejtd05qkvqhk9u/NOSL700NJEF.tap?dl=1) | Network Job Entry Facility |
| rbf5    | Remote Batch Facility Version 5 |

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
| [spss](https://www.dropbox.com/s/2eo63elqvhi0vwg/NOSSPSS6000V9.tap?dl=1) | SPSS-6000 V9 - Statistical Package for the Social Sciences |

These lists will grow, so revisit this page to see what new products have been added,
use `git pull` to update the lists in your local repository clone, and then use
`install all` to install them.

## <a id="newds"></a>Creating a New Deadstart Tape  
The `reconfigure.js` tool and jobs initiated by `install_product` insert the binaries
they produce into the direct access file named `PRODUCT` in the catalog of user
`INSTALL`, and they also update the file named `DIRFILE` to specify the system libraries
with which the binaries are associated. To create a new deadstart tape that includes the
contents of `PRODUCT`, execute the following command:

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
- etc.

A script named `reconfigure.js` applies customized configuration. It accepts zero or
more command line arguments, each of which is taken as the pathname of a file
containing configuration parameter definitions. If no command line arguments are
provided, the script looks for a file named `site.cfg` in the current working
directory, and if no such file exists, the script does nothing.

The simplest way to use the script is to define all customized configuration parameters
in a file named `site.cfg` and then invoke the script, as in:

```
node reconfigure
```

Each file of configuration parameters may contain one or more sections. Each section
begins with a name delimited by `[` and `]` characters (like *DtCyber's* `cyber.ini`
file). For example, here is a section named `CMRDECK`:

```
[CMRDECK]
MID=AX.
NAME=MAX - CYBER 865 WITH CYBIS
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
192.168.0.17 max.nostalgiccomputing.org max LOCALHOST_AX
192.168.0.19 min.nostalgiccomputing.org min LOCALHOST_IN
192.168.1.2  vax1.nostalgiccomputing.com vax1
192.168.1.3  vax2.nostalgiccomputing.com vax2
192.168.1.4  rsx11m.nostalgiccomputing.com rsx11m
192.168.1.5  tops20.nostalgiccomputing.com tops20 pdp10
192.168.2.2  bsd211.nostalgiccomputing.com bsd211 pdp11
```

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
njeNode=LOCALMVS,RSCS,LMV,192.168.1.3:175,MYNODE
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

    njeNode=*nodename*,*software*,*lid*,*public-addr*,*link*[,*local-addr*[,B*block-size*][,P*ping-interval*]]
    
    | Parameter     | Description |
    |---------------|-------------|
    | nodename      | The unique 1 - 8 character name of the node. |
    | software      | The name of the NJE software used by the node; one of NJEF, RSCS, NJE38, or JNET. |
    | lid           | The unique, 3-character logical identifier assigned to the node. |
    | public-addr   | The public IP address and TCP port number on which the node listens for NJE connections. The format is `ipaddress:portnumber`, e.g., 192.168.1.3:175. |
    | link          | The name of the NJE node that serves as the defined node's primary link to the NJE network. |
    | local-addr    | Optional IP address of the defined node. If specified, this address will be used by the defined node to identify itself to its NJE peers. If omitted, an appropriate default will be chosen based upon the defined system's physical TCP network configuration. |
    | block-size    | Optional block size, in bytes, to use in communicating with peers. The default is 8192. |
    | ping-interval | Optional ping interval, in seconds, to use in keeping the NJE connection alive. The default is 600 (i.e., 10 minutes). |

    Example:
    
    `njeNode=LOCALMVS,RSCS,LMV,192.168.1.3:175,MYNODE`
    
- **njePorts** : Specifies the range of CLA ports that will be used in defining terminals for use by NJF in the NOS NDL. The general syntax of this entry is:

    njePorts=*cla-port-number*,*port-count*
    
    Where *cla-port-number* is the number of the first CLA port to be used, and
    *port-count* defines the maximum number of ports to be used. The default is
    equivalent to:
    
    `njePorts=0x30,16`

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
