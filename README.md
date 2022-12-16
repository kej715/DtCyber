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

Visit these subdirectories and repositories to find artifacts and information
enabling you to install various operating systems on *DtCyber* after building it:

- [COS](COS) : the Chippewa Operating System (COS) was one of the first operating
systems created by Control Data for CDC 6000 series computer systems.
- [HUSTLER](HUSTLER) : a derivative of the SCOPE operating system implemented and
originally deployed at Michigan State University.
- [KRONOS2.1](KRONOS2.1) : KRONOS 2.1 was the logical ancestor of NOS 1 and a logical
descendent of COS. It supports interactive access and batch operation.
- [NOS1.3](NOS1.3) : the NOS 1.3 operating system was a logical descendent of KRONOS.
The instance provided here supports interactive access, local and remote batch
operation, and a version of
[PLATO](https://en.wikipedia.org/wiki/PLATO_%40computer_system%41).
- [NOS2.8.7](NOS2.8.7) : NOS 2.8.7 was the last operating system formally released
by CDC for its Cyber 170 series supercomputers. In addition to supporting interactive
access and local and remote batch operation, the instance provided here supports a
very rich collection of programming languages and data communication features.
- [NOS/BE](https://github.com/bug400/NOSBE712) : the NOS/BE operation system was a
descendent of COS and SCOPE. It was designed primarily for batch operation but also
supported interactive access. Additional documentation about building a NOS/BE system
can be found [here](https://cdc.sjzoppi.com/doku.php?id=members:nosbe:building_nos_be_level_712_from_scratch)
on the [CDC Community site](https://cdc.sjzoppi.com/doku.php?id=start),
and a ready-to-run NOS/BE package can be found
[here](https://cdc.sjzoppi.com/doku.php?id=members:nosbe:use_a_ready_to_run_nos_be_l_712_system).

Information about configuring and operating *DtCyber* can be found
[here](https://cdc.sjzoppi.com/doku.php?id=dtcyber:v5.8.sz:start) on the
[CDC Community site](https://cdc.sjzoppi.com/doku.php?id=start).

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
