# Automation Support
This directory contains two [Node.js](https://nodejs.org) modules supporting automation:

- **DtCyber.js** This module provides a class named *DtCyber* that enables programmed
interaction with the operator interface exposed by the DtCyber executable. It is used by
the automated installation tools in the [HUSTLER](../HUSTLER), [NOS1.3](../NOS1.3),
and [NOS2.8.7](../NOS2.8.7) directories. It can be used to create custom tools that
interact with the DtCyber emulator too.

- **Terminal.js** This module provides classes that enable programmed interaction with
terminal interfaces exposed by virtual machines such as DtCyber. The classes of the
module include:
<ul>
<li>
<b>AnsiTerminal</b> : Emulates an ANSI X.364 (DEC VT100 family) terminal and implements
methods specific to ANSI X.364 terminals.
</li>
<li>
<b>BaseTerminal</b> : Base class providing common methods inherited by the other
classes.
</li>
<li>
<b>CybisTerminal</b> : Emulates the type of PLATO terminal that was typically connected
to a NAM/PNI based terminal network, and implements methods specific to this type of
terminal.
</li>
<li>
<b>PlatoTerminal</b> : Emulates the type of PLATO terminal that was typically connected
to NIU's (Network Interface Units) on a NOS 1 system running PLATO, and implements
methods specific to this type of terminal.
</li>
</ul>

See comments in the source code of the modules to understand the methods provided by the classes they export. See the [examples](examples) directory for examples of programs
that use the `Terminal.js` module to interact with Telex and PLATO on NOS 1 and IAF
and CYBIS on NOS 2.

These modules and the example programs are implemented in Javascript using
[Node.js](https://nodejs.org/). To build and run them, you will need to install
*Node.js* and *npm* on your system, if you don't have them already. The minimum versions
required are:

    Node.js: 16.x.x
    npm : 8.x.x

To build the modules in this directory, enter the following command:

    npm install
