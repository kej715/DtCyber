# Installing NOS 1.3
This directory tree contains a collection of artifacts facilitating the installation
of the NOS 1.3 operating system on *DtCyber*. This distribution of the
NOS 1.3 operating system is based upon a customized version preserved from Florida
State University. Following the instructions, below, will produce a working instance
of the operating system that supports:

- Batch job submission via a simulated card reader
- Interactive login via your favorite Telnet client
- Remote job entry via [rjecli](../rje-station)
- Job submission from NOS 1.3 to RBF on [NOS 2.8.7](../NOS2.8.7), or to JES2 on IBM
MVS hosted by the [Hercules](https://sdl-hercules-390.github.io/html/) IBM mainframe
emulator
- PLATO

Substantial automation has been provided to make the installation process
as easy as possible. In fact, it's nearly trivial compared to what was possible on
real Control Data computer systems back in the 1980's and 90's.

## Prerequisites
Before getting started, be aware of some details and prerequisites:

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

## Installation Steps
1. If not done already, use the appropriate *Makefile* in this directory's parent
directory to build *DtCyber* and produce the *dtcyber* executable. For Windows, a
Visual Studio solution file is available. On Windows, you will also need to execute
`npm install` manually in folders `automation`, `rje-station`, and `webterm`.
2. Start the automated installation by executing the following command:

>`node install`

The process initiated by the *node* command in this case will download a preconfigured,
ready-to-run image of NOS 1.3 and activate it. Various system configuration parameters
may be customized by defining customized values in a file named `site.cfg`. The
installation process looks for this file and, if found, automatically executes the
`reconfigure.js` script to apply its contents and produce a customized system
configuration. See section [Customizing the NOS 1.3 Configuration](#reconfig), below,
for details. If `site.cfg` is not found, `install.js` simply leaves the default
configuration in place.

The system will be left running as a background process when installation is complete,
and the command window will be left at the DtCyber `Operator> ` prompt. Enter the `exit`
command or the `shutdown` command to shutdown the system gracefully when you have
finished playing with it, and you are ready to shut it down.

To start *DtCyber* and NOS 1.3 again in the future, enter the following command:

>`node start`

That's it. You have a fully operational Control Data Cyber 173 supercomputer
running the NOS 1.3 operating system, complete with BASIC, COBOL, FORTRAN IV,
LISP, PASCAL, SYMPL, COMPASS assembly language, and PLATO. Welcome back to
supercomputing in the 1980's!

## Operator Command Extensions
When installation completes successfully, and also when DtCyber is started using 
`start.js`, the set of commands that may be entered at the `Operator> ` prompt is
extended to include the following:

- `exit` : exits the operator interface and initiates graceful shutdown of the
system.
- `make_ds_tape` (alias `mdt`) : creates a new deadstart tape from the file SYSTEM in
the catalog of user INSTALL.
- `reconfigure` (alias `rcfg`) : applies customized system configuration parameters. See
[Customizing the NOS 1.3 Configuration](#reconfig) for details.
- `shutdown` : initiates graceful shutdown of the system.

## Login
You may log into the system using your web browser. *DtCyber* is configured to
start a special web server that supports browser-based terminal emulators. For the NOS
1.3 system, this web server listens for connections on TCP port 8003. When you request
your web browser to open the following URL:

>`http://localhost:8003`

it will display a page showing the systems served by the web server, and this will
include the NOS 1.3 system itself and the PLATO subsystem running on it. When you
click on the link associated with either of these systems, an appropriate browser-based
terminal emulator will launch, and you will be invited to login. For example, when
you click on the link for system `telex` (the NOS 1.3 system itself), an ANSI X.364
(DEC VT-100 family) terminal emulator will launch. When you click on the link for
`plato`, a PLATO terminal emulator will launch.

You may also log into the NOS 1.3 system itself using your favorite Telnet client,
and you may log into PLATO using a PLATO terminal emulator such as
[pterm](https://www.cyber1.org/pterm.asp). The system listens for interactive
connections on TCP port 6676, and it listens for PLATO connections on TCP port
5004.

NOS logins supported by the system include:

| User Number | Password |
|-------------|----------|
| INSTALL     | INSTALL  |
| GUEST       | GUEST    |

INSTALL is a privileged account, and GUEST is an unprivileged one. When NOS 1.3 asks
**RECOVER /SYSTEM:** during login, enter *full* and set your Telnet client to use
character mode. The browser-based terminal emulator operates in a compatible mode by
default. When you see the **/** prompt, the operating system is ready for you to enter
commands.

PLATO logins supported by the system include:

| PLATO name   | Course/Group      | Password |
|--------------|-------------------|----------|
| admin        | s                 | admin    |
| guest        | guests            | guest    |

*admin* is a privileged login. *guest* is an author in the *guests* course/group.

## Web Console
*DtCyber* is configured to start a special web server that supports browser-based user
interfaces to the system console. For the NOS 1.3 system, this web server listens for
connections on TCP port 18003. When you request your web browser to open the following
URL:

>`http://localhost:18003`

it will display a page showing the systems served by the web-based console server and the
types of service provided. Ordinarily, two entries will be shown, and they represent two
types of access to the system console of the NOS 1.3 system. One provides a 2-dimensional
representation of the console, and the other provides a 3-dimensional one. When you click on either link, a browser-based console emulator will launch, and the local console window
(either the X-Windows interface on Linux/Unix, or the Windows interface on Microsoft operating
systems) will suspend after issuing a message indicating *Remote console active*.

You may use the browser-based interface to interact with the system in the same way that you
use the local console interface. The NOS 1.3 system itself cannot distinguish between the
two forms of user interface. When you close the browser window in which the system console
interface is running, control will revert automatically to the local console user interface.

## Remote Job Entry
The system listens for RJE (remote job entry) connections on TCP port 6671. You
can use [rjecli](../rje-station) and [rjews](../rje-station) to connect to this port and
submit batch jobs via the NOS 1.3 *EI200* subsystem. The RJE data communication
protocol supported by NOS 1.3 is *MODE4*. The example
[nos1.json](../rje-station/examples/) and
[rjews.json](../rje-station/examples/) configuration files condition
[rjecli](../rje-station) and [rjews](../rje-station), respectively, to use *MODE4* to
connect and interact with NOS 1.3.

*DtCyber* is configured to start a special web service that supports browser-based
RJE access to the NOS 1.3 system. The web service listens for connections on TCP port
8085. When you request your web browser to open the following URL:

>`http://localhost:8085`

it will display a page showing the RJE hosts served by the web service, and this will
include the NOS 1.3 system. It will also indicate that it can provide access to the
[NOS 2.8.7](../NOS2.8.7) and [KRONOS 2.1](../KRONOS2.1) systems. However, all of these
systems must be running concurrently (see [Remote Batch Networking(#rbn), below) in
order for you to be able to select all of them successfully.

When you click on the link associated with any of these systems, a browser-based
RJE station emulator will launch, and you will be presented with its console window.
The console window displays operator messages sent by the RJE host to the RJE station.
It also enables you to enter station operator commands to send to the host, and it
provides a user interface for loading the station's virtual card reader with virtual
card decks (i.e., batch jobs) to submit for execution on the host.

You may also request the browser-based RJE station emulator for NOS 1.3 directly by
entering the following URL:

>`http://localhost:8085/rje.html?m=ei200-nos1&t=EI200%20on%20NOS%201.3`

An RJE command line interface is available as well. The RJI CLI can be started using
the following commands on Linux/MacOS:

>```
cd rje-station
node rjecli examples/nos1.json
```

On Windows:

>```
cd rje-station
node rjecli examples\nos1.json
```

For more information about RJE, see the [README](rje-station) file in the `rje-station`
directory.

## <a id="rbn"></a>Remote Batch Networking
After running `install.js`, the system attempts to connect to an RJE HASP
service on the local host at TCP port 2554. The [NOS 2.8.7](../NOS2.8.7) system will
listen for connections on this port if the RBF product is installed, so if the NOS
1.3 and NOS 2.8.7 systems are started on the same host machine, the TIELINE
subsystem of NOS 1.3 will connect automatically to the RBF subsystem of NOS 2.8.7,
a user logged into either of the two systems may submit batch jobs to the other
system.

The `DSA311` definition in the `cyber.ini` file of the NOS 1.3 system defines the
host and port to which TIELINE will attempt to connect. After running
`install.js`, the `DSA311` entry looks like this:

>`DSA311,5,10,20,localhost:2554`

If you want TIELINE to connect to a HASP service on a different host and/or port,
change `localhost:2554` accordingly. Note also that after running
`install.js`, TIELINE is conditioned to interoperate with RBF on NOS 2. If
you want it to interoperate with an IBM HASP service such as JES2 or RSCS running on
the Hercules IBM mainframe emulator, you must modify TIELINE and rebuild it. A job
is available for doing this. See the **Customization** section, below, for details.

## Installing a Full System from Scratch
It is possible to install a full NOS 1.3 from scratch by specifying the `full` option
when calling the `install.js` script, as in:

```
node install full
```

The ready-to-run NOS 1.3 image that is downloaded and activated by default is created in
this way. Note that this option can take as much as twenty minutes or more,
depending upon the speed and capacity of your host system.

If the file `site.cfg` exists, the `reconfigure.js` script will be called to apply its
contents. This enables the full, installed-from-scratch system to have a customized
configuration.

## Installing a Minimal System
If you prefer to install a minimal NOS 1.3 system without PLATO, you may accomplish this
by specifying the `basic` option when calling the `install.js` script, as in:

```
node install basic
```

The `basic` option causes `install.js` to install a minimal NOS 1.3 system from
scratch without PLATO. However, if the file `site.cfg` exists, the `reconfigure.js`
script will be called to apply its contents. This enables the basic system to have a
customized configuration.

## <a id="reconfig"></a>Customizing the NOS 1.3 Configuration
Various parameters of the NOS 1.3 system configuration may be changed or added to
accommodate personal preferences or local needs. In particular, definitions may be
updated or added in the system CMR deck to change parameters such as the machine
identifier or system name, to add peripheral equipment, or to change peripheral
equipment parameters.

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
file). Currently, only a section named `CMRDECK` is recognized, and it defines
parameters to be edited into the system's primary CMR deck, CMRDECK. Any CMR deck
entry defined in the
[NOS 1 Installation Handbook](http://bitsavers.trailing-edge.com/pdf/cdc/cyber/nos/60435700D_NOS_Version_1_Installation_Handbook_Jul1976.pdf)
may be specified in the section. Example:

```
[CMRDECK]
MID=OE.
NAME=MOE - CYBER 173 WITH PLATO
```

## Source Code Customization
After running `install.js`, the NOS 1.3 system has the artifacts needed to
facilitate customization of the operating system source code. In particular, the catalog
of user `INSTALL` (UI=1) contains the following files:

- **OPL485** : a *MODIFY* program library containing the source code of the operating
system and various utility programs. Modsets have been pre-applied to correspond with
executables on the supplied deadstart tape.
- **SYSTEM** : a working copy of the system's deadstart file.

In addition, the system's hardware configuration includes an auxiliary disk pack
named `PLATODV` containing files and CCL procedures enabling development and
customization of the PLATO subsystem. The following files are available in the
catalog of user `INSTALL` (UI=1) on this auxiliary pack:

- **MKCOND** : a CCL procedure to build the PLATO *CONDEN* subsystem.
- **MKFRAM** : a CCL procedure to build the PLATO *FRAMAT* subsystem.
- **MKPLAT** : a CCL procedure to build the PLATO *PLATO* subsystem.
- **MKTOOLS** : a CCL procedure to build PLATO tools.
- **POPL** : a *MODIFY* program library containing the source code of the PLATO
subsystem.

In addition to the artifacts, above, resident in the NOS 1.3 system itself, some
job templates to facilitate building various NOS 1.3 components are available in
the [decks](decks) directory of this git repository. These include:

- **build-ei200.job** : a job to build the *EI200* (aka Export/Import) subsystem.
- **build-plato.job** : a job to build the *PLATO* subsystems from the `PLATODV` auxiliary pack.
- **build-tieline-ibm.job** : a job to build the *TIELINE* subsystem such that it will interoperate with JES2 or RSCS running on the Hercules IBM emulator.
- **build-tieline-nos2.job** : a job to build the *TIELINE* subsystem such that it
will interoperate with RBF on NOS 2.

The released deadstart tape used by `install.js` was built using all of these
jobs except **build-tieline-ibm.job**, so there is no need to run any of these jobs
unless you want to modify the associated customizations, **or you want to modify
the system to make TIELINE interoperate with JES2 or RSCS on MVS or VM/CMS instead of
interoperating with RBF on NOS 2**.

To run any of the jobs, execute the `load_cards` command (alias `lc`) at *DtCyber's*
`Operator> ` prompt. For example, to run **build-tieline-ibm.job**, enter the following command:

>`Operator> lc 11,4,decks/build-tieline-ibm.job`

and watch the DSD *A* display to see it execute. The *A* display will show
`*** MOD1TM COMPLETE` when it completes successfully. It will show
`*** MOD1TM FAILED` if it fails.

All of these jobs automatically edit the executables they produce into the file named
`SYSTEM` in the catalog of user **INSTALL**. 

## Creating a New Deadstart Tape  
The `reconfigure.js` tool and customization jobs insert the binaries they produce into
the direct access file named `SYSTEM` in the catalog of user `INSTALL`. To create a new
deadstart tape that includes the contents of `SYSTEM`, execute the following command at
the `Operator> ` prompt:

>Operator> `make_ds_tape`

`make_ds_tape` copies `SYSTEM` to a new tape image. The new tape image will be a
file with relative path `tapes/newds.tap`. To restart NOS 1.3 with the new tape
image, first shut it down gracefully using the `shutdown` command:

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

>`node start`

## A Note About Anti-Virus Software
The installation scripts automatically download tape images and other files, as
needed, from the internet. Anti-virus software can interfere with this process
and prevent these files from being downloaded successfully. If an installation script
fails to download a file successfully and exits with an error indication, and
anti-virus software is running on your host system, look into configuring the
anti-virus software such that it does not apply to files being downloaded to the
NOS1.3/tapes directory of your *DtCyber* git repository.
