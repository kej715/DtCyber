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
in the [tapes](tapes) subdirectory for additioal details.
- **bunzip2**. The large binary files in the [tapes](tapes) subdirectory are
delivered in the compressed **bz2** format. You will need to use *bunzip2* or a
similar tool to uncompress the files before they can be used.
- **expect**. Automated installation of NOS 2.8.7 on DtCyber is accomplished using
[expect](https://core.tcl-lang.org/expect/index). You will need to install this tool,
if you don't have it already.
- **Privileged TCP ports**. The instance of *DtCyber* used in this process is
configured by default to use standard TCP ports for services such as Telnet (port 23)
and FTP (ports 21 and 22). These are privileged port numbers that will require you to
run *DtCyber* using *sudo* on Linux and MacOS, for example. Note that it is possible
to change the port numbers used by *DtCyber*, if necessary (see the *cyber.ini*
file).

## Installation Steps
1. If not done already, use the appropriate *Makefile* in this directory's parent
directory to build *DtCyber* and produce the *dtcyber* executable.
2. Start the automated installation by executing the following commands. On
Windows, you will probably need to enable the *dtcyber* application to use TCP ports
21, 22, and 23 too.  The process initiated by the *expect* command will take some
time to complete, perhaps as much as 15 - 20 minutes, depending upon your host
system's speed. You will see *DtCyber* start, and NOS 2.8.7 will be deadstarted and
installed. The system will be shutdown gracefull when the installation process has
completed.

| OS           | Commands                                       |
|--------------|------------------------------------------------|
| Linux/MacOS: | `ln -s ../dtcyber dtcyber`                     |
|              | `sudo expect first-install.exp`                |
|              |                                                |
| Windows:     | `copy ..\dtcyber dtcyber`                      |
|              | `expect first-install.exp`                     |

After step 2 has completed, NOS 2.8.7 is fully installed and ready to use. Enter the
following command to restart *DtCyber* and bring up the freshly installed operating
system. This is the usual way to start *DtCyber* after the initial installation of
NOS 2.8.7. The system should deadstart as it did during the initial installation.
However, it should start the **NAM** (Network Access Method) and **IAF** (InterActive
Facility) subsystems automatically too.

| OS           | Command           |
|--------------|-------------------|
| Linux/MacOS: | `sudo ./dtcyber`  |
| Windows:     | `dtcyber`         |

When the deadstart completes and **NAM** appears to settle down, you should be able
to log into the system using your favorite Telnet client. When it asks for
**FAMILY:**, press return. When it asks for **USER NAME:**, enter *INSTALL*. When it
asks for **PASSWORD:**, enter *INSTALL* again. When you see the **/** prompt, the
operating system is ready for you to enter commands. You may also login using
**GUEST** as username and password. The initial installation process creates
**GUEST** as an ordinary, non-privileged user account.

That's it. You have a fully operational Control Data Cyber 865 supercomputer
running the NOS 2.8.7 operating system, complete with APL, COBOL, FORTRAN IV and V,
PASCAL, SYMPL, and COMPASS assembly language. Welcome back to supercomputing in the
1980's!

## Preparing for Customization
The NOS 2.8.7 installation tape images contain full source code and build procedures
for the operating system, and this directory tree contains automated scripts
enabling you to load the source code and run the procedures. Before the build
procedures can run successfully, however, you must prepare the system for
customization. This step needs to be executed only once after the initial
installation of the operating system. It is a prerequisite for building and
customizing the operating system from source code, and **it is also a prerequisite
for installing most optional software products**.

To prepare the system for customization and installation of optional products,
execute the following command from this directory while DtCyber is running NOS 2.8.7:

>`expect prep-customization.exp`

`prep-customization.exp` executes *SYSGEN(SOURCE)* to load operating system source
code and build procedures on the INSTALL user's account, and then it submits two
batch jobs from from the INSTALL user to set up for building the operating system
and to apply initial corrective code and base modifications to OS source program
library. It will take a few minutes for the script to complete, so be patient and watch it run.

>Note that this and the script for building optional products, described next,
require that you have [netcat](https://en.wikipedia.org/wiki/Netcat) installed on
your host system. MacOS and most Linux distributions include it. You will need to
install it yourself on Windows, if you don't have it already.

## Building Optional Products
A script named `build-product.exp` enables you to build optional software products
for NOS 2.8.7. It also enables you to rebuild the base operating system itself and
other products provided on the initial deadstart tape. 'build-product.exp` is
designed to download tape images automatically from public libraries on the web, as
needed.

To reveal a list of products that `build-product.exp` can build, enter the following
command:

>`expect build-product.exp list`

Each line displayed begins with a product name which is followed by a short description of the product. To build a product, call `build-product.exp` as in:

>`expect build-product.exp` *product-name*

Products currently supported include:

| Product | Description |
|---------|-------------|
| atf     | Rebuild NOS Automated Cartridge Tape Facility (dependency: nam5) |
| cdcs2   | CYBER Database Control System Version 2 |
| cgames  | NOS Console Games (EYE, KAL, LIFE, LUNAR, MIC, PAC, SNK, TTT) |
| nam5    | Rebuild NOS Network Access Method |
| nos     | Rebuild NOS 2.8.7 Base Operating System |
| [ncctcp](ncctcp.post https://www.dropbox.com/s/m172wagepk3lig6/ncctcp.tap?dl=1) | Nostalgic Computing Center TCP/IP Applications (dependency: ses) |
| qu3     | Query Update Utility Version 3 |
| [ses](https://www.dropbox.com/s/ome99ezh4jhz108/SES.tap?dl=1) | Software Engineering Services Tools |
| tcph    | Rebuild NOS FTP Client and Server (dependency: nam5) |

This list will grow, so revisit this page to see what new products are added, and
use `git pull` to update the list in your local repository.

