# NOS 2.8.7
This directory tree contains a collection of artifacts facilitating the installation from
scratch of the NOS 2.8.7 operating system on *DtCyber*.
Following the step-by-step instructions, below, should produce a working instance of the
operating system that supports batch job submission via a simulated card reader as well as
interactive login via your favorite Telnet client.
Substantial automation has been provided in order to make the installation process as easy
as possible. In fact, it's nearly trivial compared to what was possible on real Control Data
computer systems back in the 1980's and 90's.

## Prerequisites
Before getting started, be aware of a few details and prerequisites:

- **Git LFS**. This directory subtree contains some large binary files in the [tapes](tapes)
subdirectory. These are tape images containing operating system executables and source code.
They are managed using **Git LFS**. **Git LFS** is not usually provided, by default, with standard
distributions of Git, so you will need to install it on your host system, if you don't have it
already, and you might need to execute *git lfs pull* or *git lfs checkout* to inflate the large
binary files properly before *DtCyber* can use them. See the [README](tapes/README.md) file in
the [tapes](tapes) subdirectory for additioal details.
- **bunzip2**. The large binary files in the [tapes](tapes) subdirectory are delivered in
the compressed **bz2** format. You will need to use *bunzip2* or a similar tool to uncompress
the files before they can be used.
- **expect**. Automated installation of NOS 2.8.7 on DtCyber is accomplished using
[expect](https://core.tcl-lang.org/expect/index). You will need to install this tool, if you
don't have it already.
- **Telnet port**. The instance of *DtCyber* used in this process is configured by default to use
standard TCP ports for services such as Telnet (port 23) and FTP (ports 21 and 22). These are
privileged port numbers that will require you to run *DtCyber* using *sudo* on Linux and MacOS,
for example. Note that it is possible to change the port numbers used by *DtCyber*, if necessary
(see the *cyber.ini* file).

## Installation Steps
1. It not done already, use the appropriate *Makefile* in this directory's parent directory
to build *DtCyber* and produce the *dtcyber* executable.
2. Start the automated installation by executing the following *expect* command. On Windows, you
will probably need to enable the *dtcyber* application to use TCP ports 21, 22, and 23, you 
will need to set your PATH environment variant to include this directory's parent using the *SET*
command, and you would omit *sudo* from the command line. The process initiated by this command
will take some time to complete, perhaps as much as 15 - 20 minutes, depending upon your host
system's speed. You will see *DtCyber* start, NOS 2.8.7 will be deadstarted and installed,
and then shutdown gracefully when installation completes.
>`PATH=../:$PATH sudo expect first-install.exp`
3. Enter the following command to restart *DtCyber* and bring up the freshly installed operating
system. This is the usual way to start *DtCyber* after the initial installation of NOS 2.8.7.
The system should deadstart as it did during the initial installation. However, it should start
the **NAM** (Network Access Method) and **IAF** (InterActive Facility) subsystems automatically.
When the deadstart completes and **NAM** appears to settle down, you should be able to log into
the system using your favorite Telnet client. When it asks for **FAMILY:**, press return. When it
asks for **USER NAME:**, enter *INSTALL*. When it asks for **PASSWORD:**, enter *INSTALL* again.
When you see the **/** prompt, the operating system is ready for you to enter commands.
>`sudo ../dtcyber`

That's it. Welcome back to supercomputing in the 1980's!
