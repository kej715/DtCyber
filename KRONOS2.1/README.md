# Installing KRONOS 2.1
This directory tree contains a collection of artifacts facilitating the installation
of the KRONOS 2.1 operating system on *DtCyber*. Following the instructions, below, will produce a working instance of the operating system that supports:

- Batch job submission via a simulated card reader
- Interactive login via your favorite Telnet client
- Remote job entry via [rjecli](../rje-station)

Substantial automation has been provided to make the installation process
as easy as possible. In fact, it's nearly trivial compared to what was possible on
real Control Data computer systems back in the 1970's and 80's.

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
`npm install` manually in folders `automation` and `webterm`.
2. Start the automated installation by executing the following command:

>`node install`

The process initiated by the *node* command in this case will download a preconfigured,
ready-to-run image of KRONOS 2.1 and activate it. Various system configuration
parameters may be customized by defining customized values in a file named `site.cfg`.
The installation process looks for this file and, if found, automatically executes the
`reconfigure.js` script to apply its contents and produce a customized system
configuration. See section [Customizing the KRONOS 2.1 Configuration](#reconfig), below,
for details. If `site.cfg` is not found, `install.js` simply leaves the default
configuration in place.

The system will be left running as a background process when installation is complete,
and the command window will be left at the DtCyber `Operator> ` prompt. Enter the `exit`
command or the `shutdown` command to shutdown the system gracefully when you have
finished playing with it, and you are ready to shut it down.

To start *DtCyber* and KRONOS 2.1 again in the future, enter the following command:

>`node start`

That's it. You have a fully operational Control Data Cyber 173 mainframe computer
system running the KRONOS 2.1 operating system, complete with COBOL, FORTRAN IV, SYMPL,
and COMPASS assembly language. Welcome back to *big-iron* computing in the 1970's!

## Operator Command Extensions
When installation completes successfully, and also when DtCyber is started using 
`start.js`, the set of commands that may be entered at the `Operator> ` prompt is
extended to include the following:

- `exit` : exits the operator interface and initiates graceful shutdown of the
system.
- `make_ds_tape` (alias `mdt`) : creates a new deadstart tape from the file SYSTEM in
the catalog of user INSTALL.
- `reconfigure` (alias `rcfg`) : applies customized system configuration parameters. See
[Customizing the KRONOS 2.1 Configuration](#reconfig) for details.
- `shutdown` : initiates graceful shutdown of the system.


## Login
You may log into the system using your web browser. *DtCyber* is configured to
start a special web server that supports browser-based terminal emulators. For the
KRONOS 2.1 system, this web server listens for connections on TCP port 8004. When you
request your web browser to open the following URL:

>`http://localhost:8004`

it will display a page showing the systems served by the web server, and this will
include only the KRONOS 2.1 system itself. When you click on the `telex` link associated
with the system, a browser-based ANSI X.364 (DEC VT-100 family) terminal emulator will
launch, and you will be invited to login.

You may also log into the KRONOS 2.1 system using your favorite Telnet client. The
system listens for interactive connections on TCP port 6677.

KRONOS logins supported by the system include:

| User Number | Password |
|-------------|----------|
| INSTALL     | INSTALL  |
| GUEST       | GUEST    |

INSTALL is a privileged account, and GUEST is an unprivileged one. When KRONOS 2.1 asks
**RECOVER /SYSTEM:** during login, enter *full* and set your Telnet client to use
character mode. The browser-based terminal emulator operates in a compatible mode by
default. When you see the **/** prompt, the operating system is ready for you to enter
commands.

## Remote Job Entry
The system listens for RJE (remote job entry) connections on TCP port 6678. You
can use [rjecli](../rje-station) and [rjews](../rje-station) to connect to this port and
submit batch jobs via the KRONOS 2.1 *EI200* subsystem. The RJE data communication
protocol supported by KRONOS 2.1 is *MODE4*. The example
[kronos.json](../rje-station/examples/) and
[rjews.json](../rje-station/examples/) configuration files condition
[rjecli](../rje-station) and [rjews](../rje-station), respectively, to use *MODE4* to
connect and interact with KRONOS 2.1.

*DtCyber* is configured to start a special web service that supports browser-based
RJE access to the KRONOS 2.1 system. The web service listens for connections on TCP port
8085. When you request your web browser to open the following URL:

>`http://localhost:8085`

it will display a page showing the RJE hosts served by the web service, and this will
include the KRONOS 2.1 system. It will also indicate that it can provide access to the
[NOS 2.8.7](../NOS2.8.7) and [NOS 1.3](../NOS1.3) systems. However, all of these systems
must be running concurrently in order for you to be able to select all of them
successfully.

When you click on the link associated with any of these systems, a browser-based
RJE station emulator will launch, and you will be presented with its console window.
The console window displays operator messages sent by the RJE host to the RJE station.
It also enables you to enter station operator commands to send to the host, and it
provides a user interface for loading the station's virtual card reader with virtual
card decks (i.e., batch jobs) to submit for execution on the host.

You may also request the browser-based RJE station emulator for KRONOS 2.1 directly by
entering the following URL:

>`http://localhost:8085/rje.html?m=ei200-kronos&t=EI200%20on%20KRONOS%202.1`

An RJE command line interface is available as well. The RJI CLI can be started using
the following commands on Linux/MacOS:

>```
cd rje-station
node rjecli examples/kronos.json
```

On Windows:

>```
cd rje-station
node rjecli examples\kronos.json
```

For more information about RJE, see the [README](rje-station) file in the `rje-station`
directory.

## Installing a Full System from Scratch
It is possible to install a full KRONOS 2.1 system from scratch by specifying the `full`
option when calling the `install.js` script, as in:

```
node install full
```

The ready-to-run KRONOS 2.1 image that is downloaded and activated by default is created
in this way. Note that this option can take as much as ten minutes or more,
depending upon the speed and capacity of your host system.

If the file `site.cfg` exists, the `reconfigure.js` script will be called to apply its
contents. This enables the full, installed-from-scratch system to have a customized
configuration.

## <a id="reconfig"></a>Customizing the KRONOS 2.1 Configuration
Various parameters of the KRONOS 2.1 system configuration may be changed or added to
accommodate personal preferences or local needs. In particular, definitions may be
updated or added in the system CMR deck to change parameters such as the system name,
to add peripheral equipment, or to change peripheral equipment parameters.

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
parameters to be edited into the system's primary CMR deck, CMRDECK. Example:

```
[CMRDECK]
NAME=KRONOS 2.1 ON CYBER 173.
```

## Source Code Customization
After running `install.js`, the KRONOS 2.1 system has the artifacts needed to
facilitate customization of the operating system source code. In particular, the catalog
of user `INSTALL` (UI=1) contains the following files:

- **OPL404** : a *MODIFY* program library containing the source code of the operating
system and various utility programs. Modsets have been pre-applied to correspond with
executables on the supplied deadstart tape.
- **SYSTEM** : a working copy of the system's deadstart file.
