# DtCyber
*DtCyber* is a high fidelity simulator of
[Control Data](https://en.wikipedia.org/wiki/Control_Data_Corporation)
[6000](https://en.wikipedia.org/wiki/CDC_6000_series),
[70, 170, and 700](https://en.wikipedia.org/wiki/CDC_Cyber#Cyber_70_and_170_series),
series supercomputers. This version of *DtCyber* is a direct derivative of
*Desktop CYBER 5.5.1* created by Tom Hunter. This version simulates additional
types of peripheral I/O devices, includes additional networking features, and
supports the [Nostalgic Computing Center](http://www.nostalgiccomputing.org).
This repository also provides automation and artifacts intended to make it easy for
anyone to build the simulator and run historic software on it.

Visit these subdirectories to find artifacts and information enabling you to install
operating systems on *DtCyber* after building it:

- [NOS2.8.7](NOS2.8.7) : NOS 2.8.7 was the last operating system formally released
by CDC for its Cyber 170 series supercomputers.
- [NOS1.3](NOS1.3) : the NOS 1.3 operating system and a corresponding version of
PLATO.

Visit the [doc](doc) directory to find the somewhat outdated *Basic Operation* guide
for *DtCyber*. This guide is oriented toward operation of an older version of the
operating system. Nevertheless, most of the basic concepts apply to NOS 2.8.7 and
NOS 1.3, and most information about operating *DtCyber* itself still applies.

Visit the [CDC documentation archives at Bitsavers](http://bitsavers.trailing-edge.com/pdf/cdc/)
for a wealth of preserved documentation on Control Data hardware and software. In
particular, the [scans contributed by Tom Hunter](http://bitsavers.trailing-edge.com/pdf/cdc/Tom_Hunter_Scans/)
provide a wealth of information about the CDC 6000, 70, 170, and 700 series machines
and the software that ran on them (and continues to run on *DtCyber*).

## Building the simulator
This root directory contains the source code for the simulator and *makefiles* for
many types of machines and operating systems that can host it. Some of the most
commonly used ones are:

- **Makefile.linux32** A *makefile* for 32-bit Linux systems that produces a 32-bit
    *DtCyber* executable.
- **Makefile.linux64** A *makefile* for Linux systems that produces a 64-bit
    *DtCyber* executable.
- **Makefile.linux64-armv8** A *makefile* for ARM-based Linux systems (e.g.,
    Raspberry Pi running 32-bit OS) that produces a 64-bit executable.
- **Makefile.linux64-armv8-a** A *makefile* for ARM-based Linux systems
    running a 64-bit OS (e.g., Raspberry Pi4 running 64-bit Linux) that produces
    a 64-bit executable.
- **Makefile.macosx** A *makefile* for 64-bit, Intel-based MacOS systems.

Project (*DtCyber.vcxproj*) and solution definition (*DtCyber.sln*) files are
provided for Microsoft Visual Studio too.

For example, to build the simulator on MacOS, execute the following command:

>`make -f Makefile.macosx all`

The `all` target builds both *dtcyber* (the mainframe simulator) and *stk* (the
StorageTek 4400 automated cartridge tape system simulator). If you want to build only *dtcyber*, execute either of the following commands:

> `make -f Makefile.macosx dtcyber`

> `make -f Makefile.macosx`

To build *dtcyber* on Windows, open the solution file (*DtCyber.sln*) in Visual
Studio and build the project defined by it.

## Contributing
See [CONTRIBUTING.md](CONTRIBUTING.md) for information about contributing new
features, enhancements, and bug fixes to the simulator.
