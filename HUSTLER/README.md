# Installing SCOPE/HUSTLER
This directory tree contains a collection of artifacts facilitating the installation
of the Michigan State University SCOPE/HUSTLER operating system on *DtCyber*.
Following the instructions, below, will produce a working instance of the operating
system that supports:

- Batch job submission via a simulated card reader
- Interactive login via your favorite Telnet client

Automation has been provided to make the installation process as easy as possible.

## Prerequisites
Before getting started, be aware of some details and prerequisites:

- **Node.js**. The scripts that automate installation of the operating system are
implemented in Javascript and use the Node.js runtime. You will need to have Node.js
version 16.0.0 (or later) and NPM version 8.0.0 or later. Node.js and NPM can be
downloaded from the [Node.js](https://nodejs.org/) website, and most package managers
support it as well.

## Installation Steps
1. If not done already, use the appropriate *Makefile* in this directory's parent
directory to build *DtCyber* and produce the *dtcyber* executable. For Windows, a
Visual Studio solution file is available. On Windows, you will also need to execute
`npm install` manually in folder `automation`.
2. Start the automated installation by executing the following command:

>`node install`

The process initiated by the *node* command will download tape and disk images for
SCOPE/HUSTLER, it will expand them, and then it will initiate a cold deadstart of
the operating system. The system will be left running as a background process when
installation and cold deadstart are complete, and the command window will be left at
the DtCyber `Operator> ` prompt. Enter the `exit` command or the `shutdown` command
to shutdown the system gracefully when you have finished playing with it, and you are
ready to shut it down.

To start *DtCyber* and SCOPE/HUSTLER again in the future, enter the following
command. This will initiate a warm deadstart of the operating system.

>`node start`

That's it. You have a fully operational Control Data Cyber 6400 supercomputer
running the SCOPE/HUSTLER operating system, complete with APL, BASIC, COBOL,
FORTRAN, and COMPASS assembly language. Welcome back to academic supercomputing in
the 1980's!

## Operator Command Extensions
When installation completes successfully, and also when DtCyber is started using 
`start.js`, the set of commands that may be entered at the `Operator> ` prompt is
extended to include the following:

- `exit` : exits the operator interface and initiates graceful shutdown of the
system.
- `shutdown` : initiates graceful shutdown of the system.


## Interactive Login
You should be able to log into the system using your favorite Telnet client. The
system listens for interactive connections on TCP port 6500, so direct your Telnet
client to use that port. SCOPE/HUSTLER will prompt for *password*, *problem number*,
and *user ID*. The following privileged accounts are predefined, and you may use
any of them to login:

| password | problem number | user ID |
|----------|----------------|---------|
| redact   | 0117370        | katz    |
| redact   | 0117371        | renwick |
| redact   | 0117372        | nelson  |
| redact   | 0117373        | bedoll  |
