# Automation Examples
This directory contains a number of example programs illustrating how to use classes
in the parent directory to automate interaction with a virtual machine such as
*DtCyber*. The example programs are implemented in Javascript using
[Node.js](https://nodejs.org/). To build and run them, you will need to install
*Node.js* and *npm* on your system, if you don't have them already. The minimum versions
required are:

    Node.js: 16.x.x
    npm : 8.x.x

The examples provided in this directory include:

- **cybis.js** Demonstrates how to connect and login to CYBIS on NOS 2.8.7, launch a
lesson, exit from it, and logout.
- **nos1-cmd-list.js** Demonstrates how to connect and login to Telex on NOS 1.3,
execute a short list of commands, and logout.
- **nos1-script.js** Demonstrates how to connect and login to Telex on NOS 1.3,
run a terminal script, and logout.
- **nos2-cmd-list.js** Demonstrates how to connect and login to Telex on NOS 2.8.7,
execute a short list of commands, and logout.
- **nos2-script.js** Demonstrates how to connect and login to Telex on NOS 2.8.7,
run a terminal script, and logout.
- **plato.js** Demonstrates how to connect and login to PLATO on NOS 1.3, launch a
lesson, exit from it, and logout.