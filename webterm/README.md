# Web Terminal Server and Emulators
*webterm-server* is a simple web server designed to serve browser-based terminal
emulators interacting with classic computer system emulators. For example, it can
serve instances of an ANSI X.364 terminal emulator that connects to *DtCyber*,
enabling users to log into and interact with [NOS 1.3](../NOS1.4) and/or
[NOS 2.8.7](../NOS2.8.7).

The types of terminal emulators supported include:

- ANSI X.364, also known as [DEC VT100](https://en.wikipedia.org/wiki/VT100) family
- [IBM 3270](https://en.wikipedia.org/wiki/IBM_3270)
- Pterm and Pterm Classic (PLATO terminal types)
- [Tektronix 4010](https://en.wikipedia.org/wiki/Tektronix_4010) (vector graphics)

The various terminal emulators support scripting as well. This is particularly
useful for creating automated examples demonstrating how to use features of an
operating system. It can also be used for automating login, file upload/download,
and testing.

## Building the Web Terminal Server
The web terminal server is implemented in Javascript using
[Node.js](https://nodejs.org/). To build and run the server, you will need to install
*Node.js* and *npm* on your system, if you don't have them already. The minimum versions
required are:

    Node.js: 16.x.x
    npm : 8.x.x

To build the server, enter the following command from this directory:

    npm install
    
## Starting the Web Terminal Server
To start the web terminal server, enter the following command from
this directory:

    node webterm-server config-file...

where *config-file...* represents references to one or more configuration files. If
omitted, the default configuration file is assumed to be located in the current working
directory and named `webterm.json`. Configuration files are read and processed left to
right when more than one are specified. Also, when multiple configuration files are
specified, and more than one contains definitions for the same configuration
properties, the property values from the last configuration file processed are used.

## Configuration
Web terminal server configuration files contain JSON syntax. Each file defines a single
Javascript object or array. A typical example defines an object with the following
properties:

- **httpRoot** : the pathname of a directory containing static web content (i.e., HTML,
CSS, Javascript, and images) to be served. This includes all of the artifacts
implementing the browser-based terminal emulators.
- **port** : the TCP port number on which the web server should listen for connections.
- **machines** : an array of objects where each object represents the definition of
a machine with which a terminal emulator can interact.

The following example illustrates what a configuration file looks like:
>
```
{
  "port": 8001,
  "httpRoot": "../../webterm/www",
  "machines":
  [
    {
      "id": "cybis",
      "title": "CYBIS on NOS 2.8.7",
      "host": "localhost",
      "port": 8005,
      "protocol": "raw",
      "terminal": "pterm.html?m=cybis&t=CYBIS%20on%20NOS%202.8.7",
      "overview": "@overview.json",
      "login": "@cybis-login.json",
      "lessons": "@cybis-lessons.json",
      "keys": "@cybis-keys.json"
    },
    {
      "id": "m01",
      "title": "NOS 2.8.7",
      "host": "localhost",
      "port": 23,
      "protocol": "telnet",
      "terminal": "aterm.html?m=m01&t=NOS%202.8.7&r=45&c=80&a=1",
      "overview": "@overview.json",
      "login-script":"/machines/nos2/scripts/login.txt",
      "file-transfer-scripts": "@file-transfer-scripts.json",
      "file-charset-options": "@file-charset-options.json",
      "installed": "@../opt/installed.json",
      "examples": "@examples.json",
      "keys": "@tektronix-keys.json"
    }
  ]
}
```

The properties of a machine definition can include:

- **examples** An array of objects. Each object defines an automated example. The array
is presented to users as a list of examples from which they can choose to see how
various features of a machine work. See [examples](#examples), below, for details about
the properties of an automated example definition, and see [Scripts](#scripts) for
details about automated scripts.

- **file-charset-options** An array of objects. The array defines a menu of character
sets from which users can choose when uploading/downloading files to/from a machine.
For example, file upload/download related to a Control Data machine might support
character sets such as Display Code, 6/12 Extended Display Code, 8/12 ASCII, etc.
Each object in the array defines a menu item.
See [file-charset-options](#file-charset-options), below, for details about the
properties of a character set menu item definition.

- **file-transfer-scripts** An object providing references to scripts supporting
file upload/download operations. If this property is omitted from a machine definition,
this indicates that the machine does not support a mechanism for uploading/downloading
files through an interactive connection.
See [file-transfer-scripts](#file-transfer-scripts), below, for details about the
properties of a file transfer definition object, and see [Scripts](#scripts) for
details about automated scripts.

- **host** A string defining the name or address of the machine.

- <a id="mid"></a>**id** A string defining a unique identifier for the machine.

- **installed** An array of strings identifying features (e.g., programming languages,
data communication options, database solutions, etc.) installed on the machine. If
specified, this list is compared against the optional `depends` property of example
definitions to determine whether individual examples are supported by the machine.

- **keys** An object documenting special keyboard keys of which users should be aware.
See [keys](#keys), below, for details about the properties of a special keys definition
object.

- **lessons** An array of objects. Each object typically documents a CYBIS/PLATO
lesson supported by the machine. See [lessons](#lessons), below, for details about
the properties of a lesson definition object.

- **login** An object providing information about how to log into the machine.
See [login](#login), below, for details about the properties of a login definition
object.

- **login-script** A string providing the URL of an automated login script.
See [Scripts](#scripts), below, for details about automated scripts.

- **overview** An object providing basic information about the hardware of the
machine. This is presented to users to provide them with a simple overview of
the machine's hardware and performance characteristics. See [overview](#overview),
below, for details about the properties of an overview definition object.

- **port** An integer defining the TCP port number on which the machine listens for
interactive terminal connections.

- **protocol** A string defining what data communication protocol, if any, the machine
uses to communicate with terminals, usually one of `telnet` or `raw`.

- **terminal** A string providing the URL that identifies and launches the terminal
emulator that will be used to establish an interactive connection with the machine.
See [Terminal Emulators](#terminal-emulators), below, for details about the
terminal emulators supported and the URL's used to launch them. 

If the value of a property begins with the character "@", then the string following
the "@" character is taken as the pathname of a file containing JSON syntax. The file
is read and parsed, and the resulting Javascript object or array is set as the value of
the property.

### <a id="examples"></a> examples
The **examples** property of a machine definition is an array of objects, and each
object defines an automated example. The set of examples represented by the array
is displayed to the user when the associated terminal emulator is loaded. The properties
of each example definition can include:

- **defn** A string providing the URL of a text file containing the formal definition of
the example. The file has two sections separated from each other by a divider line
consisting only of five consecutive "~" characters, i.e.: `~~~~~`. The section preceding
the divider line consists of HTML text describing the feature to which the example
applies. The section following the divider line provides the script that automates
the example. See [Scripts](#scripts), below, for details about automated scripts.

- **depends** An optional string identifying the installed feature on which the example
depends. This identifier is compared against the list of identifiers specified in the
machine's `installed` property. If the `installed` property is defined, and it contains
a matching identifier, then the example will be included in the list of examples
presented to users. Otherwise, it will not. However, if the `installed` property is
not defined, or the `depends` property of the example is not defined, the example
*will* be included in the list.

- **desc** A string providing a short description of the example.

- **id** A string providing a unique identifier for the example.

- **name** A string providing the name that will be presented to users for the example.

Example:
> 
```
  [
    {"name":"ALGOL 60",
     "id":"algol60",
     "desc":"Create and run an ALGOL 60 program",
     "defn":"/machines/nos2/scripts/algol60.txt",
     "depends":"algol5"
    },
    {"name":"ALGOL 68",
     "id":"algol68",
     "desc":"Create and run an ALGOL 68 program",
     "defn":"/machines/nos2/scripts/algol68.txt",
     "depends":"algol68"
    }
  ]
```

### <a id="file-charset-options"></a> file-charset-options
The **file-charset-options** property of a machine definition is an array of objects,
and each object defines a menu item for a menu of character set selections that can
be made when uploading/downloading files over interactive connections. The properties
of each object includes:

- **label** A string defining the label presented for the menu item in the menu.

- **value** A string defining the value associated with the menu item. Typically, the
value is provided to the machine as a parameter of a file transfer command.

Example:
>
```
[
  {"label":"Auto",    "value":"auto"},
  {"label":"Display", "value":"display"},
  {"label":"6/12",    "value":"6/12"},
  {"label":"8/12",    "value":"8/12"}
]
```

### <a id="file-transfer-scripts"></a> file-transfer-scripts
The **file-transfer-scripts** property of a machine definition is an object. Each
property of the object defines a file transfer operation and an associated script that
executes the operation. See [Scripts](#scripts), below, for details about automated
scripts. The properties of the file transfer scripts definition object include:

- **download-binary** A string providing the URL of a script that initiates a binary
file download operation.

- **download-end** A string providing the URL of a script that completes a file
download operation.

- **download-text** A string providing the URL of a script that initiates a text
file download operation.

- **upload-binary** A string providing the URL of a script that initiates a binary
file upload operation.

- **upload-end** A string providing the URL of a script that completes a file
upload operation.

- ** upload-text** A string providing the URL of a script that initiates a text
file upload operation.

Example:
>
```
{
  "upload-text":     "/machines/nos2/scripts/upload-text.txt",
  "upload-binary":   "/machines/nos2/scripts/upload-binary.txt",
  "upload-end":      "/machines/nos2/scripts/updownload-end.txt",
  "download-text":   "/machines/nos2/scripts/download-text.txt",
  "download-binary": "/machines/nos2/scripts/download-binary.txt",
  "download-end":    "/machines/nos2/scripts/updownload-end.txt"
}
```

### <a id="keys"></a> keys
The **keys** property of a machine definition is an object. The properties of the
object document special keyboard keys of which users should be aware. The properties
include:

- **labels** An array containing two strings. The strings are displayed as headings
of a two-column table. Typically, the first column identifies the function of a
special key, and the second column identifies a key on the keyboard.

- **mappings** An array containing an embedded array of string pairs. These represent
keyboard mappings of special keys. Typically, the first string in each pair identifies
the logical function of a special key, and the second string in each pair identifies
the keyboard key to which the function is mapped.

Example:
>
```
{
  "labels": ["Tektronix Keys", "Keyboard Keys"],
  "mappings": [
    ["Page",  "F1"],
    ["Reset", "Shift-F1"]
  ]
}
```

### <a id="lessons"></a> lessons
The **lessons** property of a machine definition is an array of objects. Each object
provides the name and brief description of a CYBIS/PLATO lesson available on the
machine. The properties of each lesson definition object are:

- **description** A string providing a brief description of the lesson.

- **name** A string providing the name of the lesson.

Example:
>
```
[
  {"name":"aids",
   "description":"Main documentation repository"
  },
  {"name":"sysaids",
   "description":"Documents system commands available only to administrators"
  }
]
```

### <a id="login"></a> login
The **login** property of a machine definition is an array of objects. Each object
defines a login prompt and response. Collectively, the set of objects contained in the
array document the prompts that the machine will issue during an interactive login
sequence and associated responses that will enable a user to login successfully.
The properties of each object are:

- **prompt** A string defining a prompt that the machine will issue during an
interactive login sequence.

- **response** A string defining a valid response that a user can give to the prompt.

Example:
>
```
[
  {"prompt": "User name:",  "response":"guest"},
  {"prompt": "User group:", "response":"guests"},
  {"prompt": "Password:",   "response":"public"}
]
```

### <a id="overview"></a> overview
The **overview** property of a machine definition is an object providing a set of
basic machine characteristics. These will be presented to users as an overview of
the machines hardware features and performance level. The properties of an overview
definition object can include:

- **addressSpace** A string documenting the size of the machine's address space, e.g.,
`128K words`.

- **addressWidth** A string documenting the machine's address width, e.g., `18 bits`.

- **cpus** An integer documenting the number of CPU's the machine has.

- **mips** An integer documenting the performance level of the machine in terms of
millions of instructions per second that it can execute, e.g., as documented by
[Computer Speed Claims 1980 to 1996](http://www.roylongbottom.org.uk/mips.htm).

- **model** A string documenting the machine's model name, e.g., `CDC Cyber 865`.

- **os** A string documenting the name of the operating system running on the machine,
e.g., `NOS 2.8.7`.

- **pps** An integer documenting the number of PP's the machine has (applicable to
CDC machines only).

- **wordSize** A string documenting the word size of the machine, e.g., `60 bits`.

Example:
>
```
{
  "model": "CDC Cyber 865",
  "cpus": 2,
  "pps": 20,
  "wordSize": "60 bits",
  "addressWidth": "18 bits",
  "addressSpace": "128K words",
  "mips": 22,
  "os": "NOS 2.8.7"
}
```

## <a id="terminal-emulators"></a> Terminal Emulators
A variety of terminal types are emulated including:

- [ANSI X.364](#aterm)
- [Pterm](#pterm)
- [Pterm Classic](#pterm-classic)
- [IBM 3270](#tn3270)

Each type of emulator has an associated URL that can be used to load the emulator into
a browser and provide it with operational parameters. The URL's and parameters are
described below.

### <a id="aterm"></a> aterm.html
The URL `/aterm.html` references the launcher for the ANSI X.364 terminal emulator.
Operational parameters that can be specified in its query string include:

- **a** : Specifying `a=1` requests the emulator to expose its support of the APL
programming language character set.

- **c** : Specifies the number of columns that the emulator should provide in the
terminal window that it creates. The default is 80.

- **d** : Specifying `d=1` requests the emulator to send an ASCII &lt;DEL&gt; character
when the *Delete* key is pressed. Otherwise, it will send ASCII &lt;BS&gt;. The default
of &lt;BS&gt; is appropriate for CDC machines. &lt;DEL&gt; is better for DEC machines.

- **k** Specifying `k=1` requests the emulator to emulate
[KSR-33](https://en.wikipedia.org/wiki/Teletype_Model_33) terminal behavior.

- **m** Specifies the [unique identifier](#mid) of the machine to which to connect.

- **r** Specifies the number of rows that the emulator should provide in the terminal
window that it creates. The default is 24.

- **t** Specifies the title that should be associated with the HTML page created for
the emulator.

Example:
```
/aterm.html?m=m01&a=1&r=40&c=80&t=Cyber%20865
```

### <a id="pterm"></a> pterm.html
The URL `/pterm.html` references the launcher for the *ASCII* PLATO terminal emulator.
This is an emulator that emulates the type of PLATO terminal that was connected to
CYBIS/PLATO via NAM/PNI and NPU's or CDCNet, i.e., typically a CYBIS/PLATO configuration
hosted by NOS 2.

Operational parameters that can be specified in the query string of this emulator
include:

- **m** Specifies the [unique identifier](#mid) of the machine to which to connect.

- **t** Specifies the title that should be associated with the HTML page created for
the emulator.

Example:
```
/pterm.html?m=cybis&t=CYBIS%20on%20Cyber%20865
```

### <a id="pterm-classic"></a> pterm-classic.html
The URL `/pterm-classic.html` references the launcher for the *classic* PLATO terminal
emulator. This is an emulator that emulates the type of PLATO terminal that was
connected to PLATO via NIU's (Network Interface Units), i.e., typically a PLATO
configuration hosted by NOS 1.

Operational parameters that can be specified in
the query string of this emulator include:

- **m** Specifies the [unique identifier](#mid) of the machine to which to connect.

- **t** Specifies the title that should be associated with the HTML page created for
the emulator.

Example:
```
/pterm-classic.html?m=plato&t=PLATO%20on%20Cyber%20173
```

### <a id="tn3270"></a> tn3270.html
The URL `/tn3270.html` references the launcher for the IBM 3270 terminal emulator.
Operational parameters that can be specified in its query string include:

- **c** : Specifies the number of columns that the emulator should provide in the
terminal window that it creates. The default is 80.

- **m** Specifies the [unique identifier](#mid) of the machine to which to connect.

- **r** Specifies the number of rows that the emulator should provide in the terminal
window that it creates. The default is 43.

- **t** Specifies the title that should be associated with the HTML page created for
the emulator.

- **tt** Specifies the IBM 3270 terminal type identifier that will be sent to the
machine. This provides the 3270 base model and model number. The default is
`IBM-3279-4-E@MOD4`, a color display with 43 lines. 

Example:
```
/tn3270.html?m=cms&r=43&c=80&t=VM/CMS
```

## <a id="scripts"></a> Scripts
A script is a list of instructions, one instruction per line. An optional label,
used for branching, may begin a line. A label is indicated by a colon (":") followed
by a string, and ending with another colon. Each instruction begins with a letter
indicating an action, and the remainder of the instruction provides text or arguments
for the action.

An example of a labeled instruction is:

```
:L1:W^/$
```

An example of an unlabeled instruction is:

```
W^/$
```

Recognized actions are defined below

### <a id="script-a"></a>A : Assign a value to a variable.
The syntax is:

```
     A<variable-name> <value>
```

A variable name must begin with an alphabetic character or underscore. Subsequent
characters of the name may be alphabetic characters, underscores, and digits. The
value can consist of any string of characters not including blank. Use the syntax
`\x20` to include an embedded blank in a value. The embedded syntax `${<index>}` or
`${<variable-name>}` within a value represents a reference to a matched regular
expression substring (see [T instruction](#script-t), below) or variable, respectively,
and the value of the referenced item is interpolated into the value.

Examples:

```
     Aone 1
     Ajsn ${0}
```

In the second example, the value of the matched regular expression substring with
index `0` is assigned to the variable named `jsn`.

### <a id="script-b"></a>B : Click a button
A specified terminal emulator button is actuated as if the user had clicked it. The
syntax is:

```
     B<button-name>
```

This instruction is most often used for actuating emulator buttons with the following
names:

- **charset** In the X.364 terminal emulator, this button switches the terminal between
the ASCII and APL character sets.

- **termType** In the X.364 terminal emulator, this button switches the terminal between
X.364 mode and Tektronix 4010 mode.

Example:

```
     BtermType
```

### <a id="script-d"></a>D : Delay by a specified number of milliseconds
This instruction causes a script to pause for a fixed number of milliseconds.

Example:

```
     D500
```

### <a id="script-d2"></a>d : Set delay between keystrokes
This instruction establishes a new interval, in milliseconds, between keystrokes
sent to the machine. It affects the execution of subsequent [F](#script-f),
[L](#script-l), [S](#script-s), and [I](#script-i) instructions.

Example:

```
     d250
```

### <a id="script-f"></a>F : Execute a function
This instruction executes an intrinsic function with optional arguments and sends the
result of the function as a string to the machine. The syntax is:

```
     F<function-name> [<arg1> ... <argn>]
```

The recognized intrinsic functions are:

**date** : returns the current date

The *date* functions accepts one optional argument specifying the date format to be
returned. The following strings are accepted:
- **MMDDYY** : the current date is returned as a 6-digit value, e.g., `092022`
- **MM/DD/YYYY** : the current date is returned as a 12-character string, e.g.,
`09/20/2022`

If the argument is omitted, the current date is returned as a value formatted according
to the user's locale.

Example:

```
     Fdate MMDDYY
``` 

### <a id="script-g"></a>G : Goto
This instruction branches to an instruction in the script having a specifed label.
The syntax is:

```
     G<label>
```

Example:

```
     Gtop
```

### <a id="script-i"></a>I : Iterate over sequence of lines
This instructions sends a sequence of lines to the machine, each prompted by the same
pattern and followed by the same delay. The sequence of lines to be sent immediately
follows the instruction. The end of the sequence is indicated by a boundary line,
and the boundary is defined as a parameter of the instruction.

For non-IBM 3270, the syntax is:

```
     I<delay> <pattern> <boundary>
```
and for IBM 3270, the syntax is:
```
     I<delay> <boundary>
```

The *delay* value defines a pause, in milliseconds, between lines sent to the machine.
The *boundary* value can be any string not containing blanks. It identifies the end
of the sequence of lines to be sent to the machine. The boundary line itself is _not_
sent to the machine.

Before each line is sent to the machine, it is checked for embedded interpolation
syntax, i.e., `${<index>}` and `${<variable-name>}`. Occurrences of these are replaced
by the referenced values before the line is sent to the machine.

Example:

> ```
I250 \\r =====
      PROGRAM FIB
C
C  CALCULATE FIRST 10 FIBONACCI NUMBERS
C
      I1 = 0
      I2 = 1
      DO 10 N = 1, 10
        PRINT *, N, ': ', I2
        I3 = I1 + I2
        I1 = I2
        I2 = I3
10    CONTINUE
      END
=====
```

### <a id="script-k"></a>K : Send a key
This instruction actuates a named keyboard key, as if the user had pressed it.
Optionally, the name of the key may be prefaced by `Shift`, `Ctrl`, and/or `Alt`.

Example:

```
     KShift F1
```

### <a id="script-l"></a>L : Send a line
This instruction sends a specified string followed by an end-of-line indication to
the machine. Before the line is sent, it is checked for embedded interpolation
syntax, i.e., `${<index>}` and `${<variable-name>}`. Occurrences of these are replaced
by the referenced values before sending to the machine. If no string argument is
specified, only an end-of-line indication is sent.

Example:

```
     Lftn5,i=source,l=listing
```

### <a id="script-s"></a>S : Send a string
This instruction sends a specified string to the machine. It differs from the
[L](#script-l) instruction in that it does _not_ send an end-of-line sequence
automatically. Before the string is sent, it is checked for embedded interpolation
syntax, i.e., `${<index>}` and `${<variable-name>}`. Occurrences of these are replaced
by the referenced values before sending to the machine.

Example:

```
     Sftn5,i=source,
     Sl=listing
     L
```

The three instructions, above, are equivalent to the example given for the
[L](#script-l) instruction itself.

### <a id="script-t"></a>T : Test and conditionally branch
This instruction compares a specified value against a pattern and branches to the
instruction with a specified label if the value matches the pattern. The syntax is:

```
     T<value> <pattern> <label>
```

Before comparing the value against the pattern, both are checked for embedded
interpolation syntax, i.e., `${<index>}` and `${<variable-name>}`. Occurrences of these
are replaced by the referenced values, and then the resulting value is matched against
the resulting pattern.

Example:

```
     T${0} USER\x20NAME: username
```

### <a id="script-u"></a>U : Wait for keyboard unlock (applies to IBM 3270 only)
This instruction waits for the keyboard of the IBM 3270 emulator to unlock, indicating
that the machine is ready to receive input.

### <a id="script-w"></a>W : Wait for pattern
This instruction reads output from the machine and compares it against a specified
pattern. A pattern is a regular expression. The script proceeds with the next
instruction only when a match is made. Parenthesized expressions within the pattern, if
any are specified, represent subpatterns. Matching subpatterns are saved for use in
subsequent string interpolation. The argument pattern itself is interpolated before
being applied to output from the machine. Thus, the pattern applied by one **W**
instruction can be affected by a previously executed **W** instruction.

Examples:

```
     W^/$
     W(FAMILY:|USER NAME:|PASSWORD:)
     T${0} FAMILY: family
     T${0} USER\x20NAME: username
     T${0} PASSWORD: password
```

The first **W** instruction shown above will match the next line received from the
machine that begins with the character "/" and does not have an end-of-line
indication.

The second **W** instruction show above will match the next line received from the
machine that contains any of the strings `FAMILY:`, `USER NAME:`, or `PASSWORD:`. In
addition, when any of these are detected, the matching string is stored in the saved
substring that can be referenced using the interpolation syntax `${0}`. If the **W**
instruction had a second parenthesized subpattern, and the pattern as a whole matched
a received line, then the substring matching that second subpattern would be saved
in the interpolation value referenced using `${1}`. The subsequent [T](#script-t)
instructions shown above reference the `${0}` substring and branch if they match the
value stored in it.
