# Installing NOS 1.3
This directory tree contains a collection of artifacts facilitating the installation
from scratch of the NOS 1.3 operating system on *DtCyber*. This distribution of the
NOS 1.3 operating system is based upon a customized version preserved from Florida
State Universsity. Following the instructions, below, will produce a working instance
of the operating system that supports:

- Batch job submission via a simulated card reader
- Interactive login via your favorite Telnet client
- Remote job entry via [rjecli](../rje-station)
- Job submission from NOS 1.3 to RBF on [NOS 2.8.7](../NOS2.8.7), or to JES2 on IBM
MVS hosted by the [Hercules](https://sdl-hercules-390.github.io/html/) IBM mainframe
emulator
- PLATO

Substantial automation has been provided in order to make the installation process
as easy as possible. In fact, it's nearly trivial compared to what was possible on
real Control Data computer systems back in the 1980's and 90's.

## Prerequisites
Before getting started, be aware of a few details and prerequisites:

- **Git LFS**. This directory subtree contains some large binary files in the
[tapes](tapes) subdirectory. These are virtual tape images containing operating
system executables and source code. They are managed using **Git LFS**. **Git LFS**
is not usually provided, by default, with standard distributions of Git, so you will
need to install it on your host system, if you don't have it already, and you might
need to execute *git lfs pull* or *git lfs checkout* to inflate the large binary
files properly before *DtCyber* can use them. See the [README](tapes/README.md) file
in the [tapes](tapes) subdirectory for additional details.
- **bunzip2**. The large binary files in the [tapes](tapes) subdirectory are
delivered in the compressed **bz2** format. You will need to use *bunzip2* or a
similar tool to uncompress the files before they can be used.
- **Node.js**. The scripts that automate installation of the operating system and
products are implemented in Javascript and use the Node.js runtime. You will need
to have Node.js version 16.0.0 (or later) and NPM version 8.0.0 or later. Node.js and
NPM can be downloaded from the [Node.js](https://nodejs.org/) website, and most
package managers support it as well.

## Installation Steps
1. If not done already, use the appropriate *Makefile* in this directory's parent
directory to build *DtCyber* and produce the *dtcyber* executable.
2. Start the automated installation by executing the following commands. The process
initiated by the *node* command will take some time to complete, perhaps as much as
15 - 20 minutes, depending upon your host system's speed. You will see *DtCyber*
start, and NOS 1.3 will be deadstarted and installed. The system will be shutdown
gracefully when the installation process has completed.

| OS           | Commands                                       |
|--------------|------------------------------------------------|
| Linux/MacOS: | `ln -s ../dtcyber dtcyber`                     |
|              | `node first-install`                           |
|              |                                                |
| Windows:     | `copy ..\dtcyber dtcyber`                      |
|              | `node first-install`                           |

After `first-install.js` completes, NOS 1.3 is fully installed and ready to use.
Enter the following command to restart *DtCyber* and bring up the freshly installed
operating system. This is the usual way to start *DtCyber* after the initial
installation of NOS 1.3. The system should deadstart as it did during the initial
installation. However, it should start the **PLATO** system too.

| OS           | Command           |
|--------------|-------------------|
| Linux/MacOS: | `./dtcyber`       |
| Windows:     | `dtcyber`         |


That's it. You have a fully operational Control Data Cyber 173 supercomputer
running the NOS 1.3 operating system, complete with APL, BASIC, COBOL, FORTRAN IV,
LISP, PASCAL, SNOBOL, SYMPL, COMPASS assembly language, and PLATO. Welcome back to
supercomputing in the 1980's!

## Interactive Login
You should be able to log into the system using your favorite Telnet client. The
system listens for interactive connections on TCP port 6676, so direct your Telnet
client to use that port. When NOS 1.3 asks for **USER NUMBER**, enter *INSTALL*, when
it asks for **PASSWORD**, enter *INSTALL* again, and when it asks
**RECOVER /SYSTEM:**, enter *HALF*. When you see the **/** prompt, the operating
system is ready for you to enter commands. You may also login using **GUEST** as user
number and password. The initial installation process creates **GUEST** as an
ordinary, non-privileged user account.

## PLATO
The system listens for PLATO connections on TCP port 5004. To log into PLATO, you
will need a PLATO terminal emulator such as [pterm](https://www.cyber1.org/pterm.asp).
The following PLATO logins are available after `first-install.js` completes:

| PLATO name   | Course/Grou       | Password |
|--------------|-------------------|----------|
| admin        | s                 | admin    |
| guest        | guests            | guest    |

*admin* is a privileged login. *guest* is an author in the *guests* course/group.

## Remote Job Entry
The system listens for RJE (remote job entry) connections on TCP port 6671. You
can use [rjecli](../rje-station) to connect to this port and submit batch jobs via
the NOS 1.3 *Export/Import* subsystem. The RJE data communication protocol supported
by NOS 1.3 is *MODE4*. The example [mode4.json](../rje-station/examples/)
configuration file conditions [rjecli](../rje-station) to use *MODE4* to connect and
interact with NOS 1.3.

## Remote Batch Networking
After running `first-install.js`, the system attempts to connect to an RJE HASP
service on the local host at TCP port 2553. The [NOS 2.8.7](../NOS2.8.7) system will
listen for connections on this port if the RBF product is installed, so if the NOS
1.3 and NOS 2.8.7 systems are started on the same host machine, the TIELINE
subsystem of NOS 1.3 will connect automatically to the RBF subsystem of NOS 2.8.7,
a user logged into either of the two systems may submit batch jobs to the other
system.

The `DSA311` definition in the `cyber.ini` file of the NOS 1.3 system defines the
host and port to which TIELINE will attempt to connect. After running
`first-install.js`, the `DSA311` entry looks like this:

>`DSA311,5,10,20,localhost:2553`

If you want TIELINE to connect to a HASP service on a different host and/or port,
change `localhost:2553` accordingly. Note also that after running
`first-install.js`, TIELINE is conditioned to interoperate with RBF on NOS 2. If
you want it to interoperate with an IBM HASP service such as JES2 or RSCS running on
the Hercules IBM mainframe emulator, you must modify TIELINE and rebuild it. A job
is available for doing this. See the **Customization** section, below, for details.

## Customization
After running `first-install.js`, the NOS 1.3 system has the artifacts needed to
facilitate customization. In particular, the catalog of user `INSTALL` (UI=1)
contains the following files:

- **OPL485** : a *MODIFY* program library containing the source code of the operating
system and various utility programs.
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
- **customize-auto.job** : a job that modifies PP programs *1DS* and *1SJ* so that
they will operate correctly for this NOS 1.3 distribution.
- **customize-plato.job** : a job to build the *PLATO* subsystem PP program named
*PPA* so that it will operate correctly for this NOS 1.3 distribution.
- **customize-runner.job** : a job to modify the *RUNNER* utility and associated
management commands so that they will operate correctly for this NOS 1.3
distribution.
- **customize-sysui.job** : a job to modify the *SUI* command so that it will operate
correctly for this NOS 1.3 distribution.

The released deadstart tape used by `first-install.js` was built using all of these
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
The customization jobs insert the binaries they produce into the direct access file
named `SYSTEM` in the catalog of user `INSTALL`. To create a new deadstart tape that includes the contents of `SYSTEM`, execute the following command:

>`node make-ds-tape`

`make-ds-tape.js` copies `SYSTEM` to a new tape image. The new tape image will be a
file with relative path `tapes/newds.tap`. To restart NOS 1.3 with the new tape
image, execute the following commands:

>`node shutdown`

>`mv tapes/ds.tap tapes/oldds.tap`

>`mv tapes/newds.tap tapes/ds.tap`

>`./dtcyber`
