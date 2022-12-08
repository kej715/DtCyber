# Installing KRONOS 2.1
This directory tree contains a collection of artifacts facilitating the installation
from scratch of the KRONOS 2.1 operating system on *DtCyber*. Following the instructions, below, will produce a working instance of the operating system that supports:

- Batch job submission via a simulated card reader
- Interactive login via your favorite Telnet client

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

The process initiated by the *node* command should take less than 5 minutes to complete,
depending upon your host system's speed. You will see *DtCyber* start, and KRONOS 2.1
will be deadstarted and installed. The system will be left running as a background
process when installation is complete, and the command window will be left at the
*DtCyber* `Operator> ` prompt. Enter the `exit` command or the `shutdown` command to
shutdown the system gracefully when you have finished playing with it, and you are ready
to shut it down.

To start *DtCyber* and KRONOS 2.1 again in the future, enter the following command:

>`node start`

That's it. You have a fully operational Control Data Cyber 173 supercomputer
running the KRONOS 2.1 operating system, complete with COBOL, FORTRAN IV, SYMPL,
and COMPASS assembly language. Welcome back to supercomputing in the 1970's!

## Operator Command Extensions
When installation completes successfully, and also when DtCyber is started using 
`start.js`, the set of commands that may be entered at the `Operator> ` prompt is
extended to include the following:

- `exit` : exits the operator interface and initiates graceful shutdown of the
system.
- `shutdown` : initiates graceful shutdown of the system.


## Login
You may log into the system using your web browser. *DtCyber* is configured to
start a special web server that supports browser-based terminal emulators. For the
KRONOS 2.1 system, this web server listens for connections on TCP port 8004. When you
request your web browser to open the following URL:

>`http://localhost:8004`

it will display a page showing the systems served by the web server, and this will
include only the KRONOS 2.1 system itself. When you click on the `m04` link associated
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

## Customization
After running `install.js`, the KRONOS 1.3 system has the artifacts needed to
facilitate customization. In particular, the catalog of user `INSTALL` (UI=1)
contains the following file:

- **OPL404** : a *MODIFY* program library containing the source code of the operating
system and various utility programs. Modsets have been pre-applied to correspond with
executables on the supplied deadstart tape.
