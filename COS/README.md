# Installing COS
This directory tree contains a collection of artifacts enabling *DtCyber* to run the
Chippewa Operating System (COS). Following the instructions, below, will produce a working instance of the operating system that supports batch job submission. COS is
historically significant as it was one of the first operating systems created by
Control Data for CDC 6000 systems. COS is a relatively primitive operating system
supporting batch operation only. However, its fundamental design laid the foundation
for subsequent operating systems including SCOPE, KRONOS, and NOS.

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
`npm install` manually in folder `automation`.
2. Start the automated installation by executing the following command:

>`node start`

The process initiated by the *node* command should take less than a minute to complete.
You will see the deadstart tape image expanded as needed, then *DtCyber* will start,
and COS will be deadstarted. The system will be left running as a background process
when the deadstart is complete, and the command window will be left at the *DtCyber*
`Operator> ` prompt. Enter the `exit` command or the `shutdown` command to shutdown the
system when you have finished playing with it, and you are ready to shut it down.

To start *DtCyber* and COS again in the future, enter the same command:

>`node start`

That's it. You have a fully operational Control Data Cyber 6400 supercomputer
running the COS operating system. Welcome back to supercomputing in the 1960's!

## Web Console
*DtCyber* is configured to start a special web server that supports browser-based user
interfaces to the system console. For the COS system, this web server listens for
connections on TCP port 18002. When you request your web browser to open the following
URL:

>`http://localhost:18002`

it will display a page showing the systems served by the web-based console server and the
types of service provided. Ordinarily, two entries will be shown, and they represent two
types of access to the system console of the COS system. One provides a 2-dimensional
representation of the console, and the other provides a 3-dimensional one. When you click on either link, a browser-based console emulator will launch, and the local console window
(either the X-Windows interface on Linux/Unix, or the Windows interface on Microsoft operating
systems) will suspend after issuing a message indicating *Remote console active*.

You may use the browser-based interface to interact with the system in the same way that you
use the local console interface. The COS system itself cannot distinguish between the
two forms of user interface. When you close the browser window in which the system console
interface is running, control will revert automatically to the local console user interface.
