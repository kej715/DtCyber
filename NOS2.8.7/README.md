# Installing NOS 2.8.7
This directory tree contains a collection of artifacts facilitating the installation
from scratch of the NOS 2.8.7 operating system on *DtCyber*.
Following these instructions should produce a working instance of the
operating system that supports batch job submission via a simulated card reader
as well as interactive login via your favorite Telnet client.
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

## Installation Steps
1. If not done already, use the appropriate *Makefile* in this directory's parent
directory to build *DtCyber* and produce the *dtcyber* executable. For Windows, a
Visual Studio solution file is available. On Windows, you will also need to execute
`npm install` manually in folders `stk` and `automation`.
2. Start the automated installation by executing the following command. On
Windows, you will probably need to enable the *dtcyber* application to use TCP ports
21, 22, and 23 too.

| OS           | Command             |
|--------------|---------------------|
| Linux/MacOS: | `sudo node install` |
| Windows:     | `node install`      |

The process initiated by the *node* command will take some time to complete, perhaps as
much as 90 minutes or more, depending upon your host system's speed and load. You will
see console interaction, jobs submitted via the virtual card reader, and *DtCyber*
shutting down and restarting a number of times to install the base NOS 2.8.7 operating
system and all of the currently available optional products.

After `install.js` completes, NOS 2.8.7 and all currently available optional products
will be fully installed and ready to use, and the system will be left running.
You should be able to log into the system using your favorite Telnet client.
When it asks for **FAMILY:**, press return. When it asks for **USER NAME:**, enter
*INSTALL*. When it asks for **PASSWORD:**, enter *INSTALL* again. When you see the **/**
prompt, the operating system is ready for you to enter commands. You may also login
using **GUEST** as username and password. The installation process creates
**GUEST** as an ordinary, non-privileged user account.

When the installation completes, NOS 2.8.7 will be running, and the command window will
be left at the DtCyber `Operator> ` prompt. Enter the `exit` command or the `shutdown`
command to shutdown the system gracefully when you have finished playing with it, and
you are ready to shut it down.

To start *DtCyber* and NOS 2.8.7 again in the future, enter the following command:

| OS           | Command           |
|--------------|-------------------|
| Linux/MacOS: | `sudo node start` |
| Windows:     | `node start`      |

That's it. You have a fully operational Control Data Cyber 865 supercomputer
running the NOS 2.8.7 operating system, complete with ALGOL, APL, BASIC, COBOL, CYBIL,
FORTRAN IV and V, LISP, PASCAL, PL/1, SNOBOL, SYMPL, COMPASS assembly language, and
various other goodies. Welcome back to supercomputing in the 1980's!

## Operator Command Extensions
When installation completes successfully, and also when DtCyber is started using 
`start.js`, the set of commands that may be entered at the `Operator> ` prompt is
extended to include the following:

- `activate_tms` : activates the NOS Tape Management System. This is recommended if
you intend to use the automated StorageTek 4400 cartridge tape subsystem. You need
to execute this command only once.
- `exit` : exits the operator interface and initiates graceful shutdown of the system.
- `install_product` (aliases `install`, `ip`) : installs one or more optional products.
on the system. Use `install list` to display the list of products available.
- `make_ds_tape` (alias `mdt`) : creates a new deadstart tape that includes products
installed by `install_product`.
- `shutdown` : initiates graceful shutdown of the system.
- `sync_tms` : synchronizes the NOS Tape Management System catalog with the
cartridge tape definitions specified in the `volumes.json` configuration file
found in the `stk` directory. See [stk/README.md](../stk/README.md) for details.

## Continuing an Interrupted Installation
The installation process tracks its progress and can continue from its last successful
step in case of an unexpected network interruption, or due to some other form of
interruption that causes the `install.js` script to exit abnormally. To continue from an
interruption, use the `continue` option as in:

| OS           | Command                      |
|--------------|------------------------------|
| Linux/MacOS: | `sudo node install continue` |
| Windows:     | `node install continue`      |

## Installing Optional Products
The `install_product` command extension enables you to re/install optional software
products for NOS 2.8.7. It also enables you to rebuild the base operating system itself
and other products provided on the initial deadstart tape. `install_product` will download tape images automatically from public libraries on the web, as needed.

To reveal a list of products that `install_product` can install, enter the
following command at the `Operator> ` prompt:

>Operator> `install list`

Each line displayed begins with a product name which is followed by a short description of the product. To install a product, call `install_product` as in:

>Operator> `install` *product-name*

For example:

>Operator> `install algol68`

You may specify more than one product name on the command line as well, and
`install_product` will install the multiple products requested. By default,
`install_product` will not re-install a product that is already installed. You
can force it to re-install a product by specifying the `-f` command line option, as
in:

>Operator> `install -f algol68`

Finally, you may request all not-yet-installed products to be installed by
specifying `all` as the product name, as in:

>Operator> `install all`

New products are added to the repository from time to time, so this command is particularly useful for installing products that were not available when you first
installed NOS 2.8.7 (or since the last time you executed the command).

#### Standard CDC Products
The following standard CDC products are pre-installed and included on the initial
deadstart tape. However, you may re-install them (e.g., after customization), if needed.

| Product | Description |
|---------|-------------|
| atf  | Automated Cartridge Tape Facility (dependency: nam5) |
| iaf  | IAF (InterActive Facility) subsystem |
| nam5 | Network Access Method |
| nos  | NOS 2.8.7 Base Operating System |
| tcph | FTP Client and Server (dependency: nam5) |

#### Optional Programming Languages and Toolchains
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

#### Optional CDC Products
The following optional CDC-provided products may be installed:

| Product | Description |
|---------|-------------|
| cdcs2   | CYBER Database Control System Version 2 |
| [icem](https://www.dropbox.com/s/1ufg3504xizju0l/icem.zip?dl=1) | ICEM Design/Drafting and GPL compiler |
| qu3     | Query Update Utility Version 3 |
| rbf5    | Remote Batch Facility Version 5|

#### Optional 3rd-Party Products
The following 3rd-party products may be installed.

| Product | Description |
|---------|-------------|
| cgames  | NOS Console Games (EYE, KAL, LIFE, LUNAR, MIC, PAC, SNK, TTT) |
| [gplot](https://www.dropbox.com/s/l342xe61dqq8aw5/gplot.tap?dl=1) | GPLOT and ULCC DIMFILM graphics library |
| [i8080](https://www.dropbox.com/s/ovgysfxbgpl18am/i8080.tap?dl=1) | Intel 8080 tools (CPM80, INTRP80, MAC80, PLM80) |
| [kermit](https://www.dropbox.com/s/p819tmvs91veoiv/kermit.tap?dl=1) | Kermit file exchange utility |
| [ncctcp](https://www.dropbox.com/s/m172wagepk3lig6/ncctcp.tap?dl=1) | TCP/IP Applications (HTTP, NSQUERY, REXEC, SMTP) |
| [spss](https://www.dropbox.com/s/2eo63elqvhi0vwg/NOSSPSS6000V9.tap?dl=1) | SPSS-6000 V9 - Statistical Package for the Social Sciences |

These lists will grow, so revisit this page to see what new products have been added,
use `git pull` to update the lists in your local repository clone, and then use
`install all` to install them.

## Creating a New Deadstart Tape  
Jobs initiated by `install_product` insert the binaries they produce into the
direct access file named `PRODUCT` in the catalog of user `INSTALL`, and they also
update the file named `DIRFILE` to specify the system libraries with which the
binaries are associated. To create a new deadstart tape that includes the contents
of `PRODUCT`, execute the following command:

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

## Installing a Minimal System
If you prefer to install a minimal NOS 2.8.7 system with a subset of optional
products, or none of the optional products, you may accomplish this by specifying the
`basic` option when calling the `install.js` script, as in:

| OS           | Commands                  |
|--------------|---------------------------|
| Linux/MacOS: | `sudo node install basic` |
| Windows:     | `node install basic`      |

The `basic` option causes `install.js` to install a minimal NOS 2.8.7 system without any
optional products. To install optional products atop the basic system, use the
`install_product` command, as in:

>Operator> `install rbf5`

In case a basic installation is interrupted before completing successfully, use the
`continue` option to proceed from the point of interruption, as in:

| OS           | Commands                           |
|--------------|------------------------------------|
| Linux/MacOS: | `sudo node install basic continue` |
| Windows:     | `node install basic continue`      |

## A Note About Anti-Virus Software
The installation scripts automatically download tape images and other files, as
needed, from the internet. Anti-virus software can interfere with this process
and prevent these files from being downloaded successfully. If an installation script
fails to download a file successfully and exits with an error indication, and
anti-virus software is running on your host system, look into configuring the
anti-virus software such that it does not apply to files being downloaded to the
NOS2.8.7/opt/tapes directory of your *DtCyber* git repository.