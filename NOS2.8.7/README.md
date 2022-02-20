# NOS 2.8.7
This directory tree contains a collection of artifacts facilitating the installation from scratch
of the NOS 2.8.7 operating system on *DtCyber*. Substantial automation has been provided in order
to make the installation process as easy as possible. In fact, it's significantly easier than
was possible on real hardware back in the 1980's and 90's.

Following the step-by-step instructions, below, should produce a working instance of the
operating system that supports batch job submission via a simulated card reader as well as
interactive access via your favorite Telnet client. Before getting started, be aware of a
couple of things:

- **Git LFS**. This directory subtree contains a few large binary files (e.g., tape images)
that are managed using **Git LFS**. **Git LFS** is not usually provided, by default, with standard
distributions of Git, so you might need to install it on your host system and execute *git lfs pull*
or *git lfs checkout* in order to inflate the large binary files properly before *DtCyber* can use
them. You will also need to use *bunzip2* or a similar tool to uncompress the files before they
can be used. See the [README](tapes/README.md) file in the [tapes](tapes) subdirectory for details.
- **Telnet port**. The instance of *DtCyber* used in this process is configured by default to use
standard TCP ports for services such as Telnet (port 23) and FTP (ports 21 and 22). These are
privileged port numbers that will require you to run *DtCyber* using *sudo* on Linux and MacOS,
for example. Note that it is possible to change the port numbers used by *DtCyber*, if necessary
(see the *cyber.ini* file).

Assuming that you have built *DtCyber* successfully already using one of the *Makefiles* in the
parent directory of this one, you may use it to begin the NOS 2.8.7 installation process by running
it from this directory by following these steps:

1. Start *DtCyber* using the following command. Omit *sudo* on Windows. However, on Windows, you will
probably need to enable the *dtcyber* application to use TCP ports 21, 22, and 23. *DtCyber* should
proceed automatically through a deadstart sequence that initializes its disk drives and brings up
an empty system.
>`sudo ../dtcyber init`
2. After the deadstart completes and the system appears idle, install a basic set of usernames and
files by entering the following command on the system console (i.e., on the main green and black
window that is displayed). The process initiated by this command will take some time to complete,
perhaps as much as 15 - 20 minutes, depending upon your host system's speed. Wait for this to complete
before proceeding to the next step.
>`X.SYSGEN(FULL)`
3. Enter the following command at the *DtCyber* **operator>** prompt (in the window from which
you executed step 1, above). This command will load a batch job into *DtCyber's* card reader, and
the job will create an *NDL* (Network Definition Language) file defining the interactive terminals that
will be supported by the operating system. Wait for this to complete before proceeding to the next step.
>`lc 12,4,decks/create-ndlopl.job`
4. Enter the following command at the *DtCyber* **operator>** prompt. This command will load another
batch job into *DtCyber's* card reader, and the job will compile and install the network definition
created in the previous step. Wait for this to complete before proceeding to the next step.
>`lc 12,4,decks/compile-ndlopl.job`
5. Enter the following command on the system console (on the main green and black window). This
command unlocks the console to enable entering the subsequent commands. Note that you will need
only to enter the first few characters before the system recognizes the command and completes it
automatically for you.
>`UNLOCK.`
6. Enter the following command on the system console. This command initiates a system shutdown.
Wait for the command to complete before proceeding to the next step.
>`CHECK POINT SYSTEM.`
7. Enter the following command on the system console. This will quiesce the system.
>`STEP.`
8. Enter the following command at the *DtCyber* **operator>** prompt. This will shut down *DtCyber*
gracefully.
>`shutdown`
9. Enter the following command to restart *DtCyber* and bring up the freshly installed operating
system. Note that this differs from step 1 in that *init* is **deliberately not** specified as a parameter.
This is the usual way to start *DtCyber* after the initial installation of NOS 2.8.7.
>`sudo ../dtcyber`

The system should deadstart as before. However, it should start the **NAM** (Network Access Method)
and **IAF** (InterActive Facility) subsystems automatically now. When the deadstart completes and **NAM**
appears to settle down, you should be able to log into the system using your favorite Telnet client. When
it asks for **FAMILY:**, press return. When it asks for **USER NAME:**, enter *INSTALL*. When it asks for
**PASSWORD:**, enter *INSTALL* again. When you see the **/** prompt, the operating system is ready for
you to enter commands.

