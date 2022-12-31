# RJE Station
[Remote Job Entry](https://en.wikipedia.org/wiki/Remote_job_entry) (RJE) was a method
for submitting batch jobs to mainframe computers from devices known as *RJE
stations*. RJE stations were connected to mainframes using synchronous serial
communication lines, and a typical RJE station would have a local console and at
least one card reader, printer, and card punch. A batch job read using the RJE
station's card reader would be submitted by the station to the mainframe and output
produced by the job would be returned automatically by the mainframe to the
submitting station where it would be printed on the station's printer and/or punched
on the station's card punch.

Various data communication protocols were defined for implementing RJE.
[HASP](https://en.wikipedia.org/wiki/Houston_Automatic_Spooling_Priority) was one
such protocol implemented by many computer system manufacturers in the 1970's, 80's, and 90's. For example, CDC's NOS 2 operating system provided HASP in its RBF (Remote Batch
Facility) subsystem. Another RJE protocol known as MODE4 was created by CDC and
supported by its KRONOS, SCOPE, NOS 1, NOS 2, and NOS/BE operating systems.

This directory provides Javascript classes implementing HASP,
[BSC](https://en.wikipedia.org/wiki/Binary_Synchronous_Communications), and MODE4
protocols over TCP/IP. The implementation is designed to execute using Node.js, and it
interoperates with *DtCyber* as well as the *Hercules* IBM mainframe simulator. In
addition, two applications are provided:

- [rjecli](#rjecli) : an RJE command line interpreter
- [rjews](#rjews) : an RJE web service

These applications enable users to submit batch jobs to RBF on NOS2, EI200 on NOS1,
JES2 on IBM MVS, and RSCS on IBM VM/CMS.

## Installing rjecli and rjews
Execute the following command from this directory to install the applications:

    npm install

## <a id="rjecli"></a>rjecli
**rjecli** is a command line interface enabling users to submit batch jobs to host
computer systems. Normally, ordinary print and punch output produced by these jobs will
be returned automatically to **rjecli**. **rjecli** creates a file with a unique name in
its spooling directory for each print and punch file returned. The default spooling
directory pathname is ./spool`. A different pathname can be defined in the **rjecli**
[configuration file](#rjecli-config).
    
### Starting rjecli
Execute the following command to start rjecli:

    node rjecli

**rjecli** accepts an optional parameter specifying the pathname of a configuration file. The default configuration file pathname is `./config.json`. To specify a different configuration file, execute **rjecli** as in:

    node rjecli /path/to/file.json

### <a id="rjecli-config"></a>Configuring rjecli
**rjecli** uses a configuration file to define the host and TCP port where the RJE
host service is located. The configuration file can also define other optional 
parameters. The default pathname of the configuration file is `./config.json`.

The contents of a typical configuration file look like this:

```
{
  "debug": false,
  "protocol":"hasp",
  "host":"localhost",
  "port":2553
}
```

The properties defined in `config.json` are:
- **debug** : if set to *true*, **rjecli** will log debugging information to the console. The default is *false*. This also sets the default values of the **bsc.debug** and **hasp.debug** properties.
- **host** : specifies the host name or IP address of the host where the RJE host service is located (e.g., the name of the host on which *DtCyber* is running). The default is *localhost*.
- **name** : specifies the username or terminal name used in the *SIGNON* request
sent to the RJE host service. The default is none.
- **password** : specifies the password used in the *SIGNON* request sent to the RJE
host service. The default is none.
- **port** : the TCP port number on which the RJE host service is listening for connections. The default is *2553*.
- **protocol** : specifies the name of the RJE data communication protocol to use in
communicating with the RJE host service. Accepted values are *hasp* and *mode4*. The
default is *hasp*. 
- **spoolDir** : specifies the pathname of a directory in which print and punch files
received from the RJE host service will be stored. The default is *./spool*.
- **bsc** : an optional object defining configuration properties specific to the BSC data communication layer.
- **bsc.debug** : if set to *true*, the BSC layer will log debugging information to `bsc.log.txt`. The default is the value specified by the top-level **debug**
property.
- **hasp** : an optional object defining configuration properties specific to the HASP data communication layer.
- **hasp.debug** : if set to *true*, the HASP layer will log debugging information to file `hasp.log.txt`. The default is the value specified by the top-level **debug**
property.
- **hasp.maxBlockSize** : specifies the maximum number of bytes to send to the HASP
service host in each HASP protocol block. The default is *400*, which works well for
IBM MVS and CMS hosts.
- **mode4** : an optional object defining configuration properties specific to the MODE4 data communication layer.
- **mode4.debug** : if set to *true*, the MODE4 layer will log debugging information to file `mode4.log.txt`. The default is the value specified by the top-level
**debug** property.
- **mode4.siteId** : specifies the MODE4 layer site identifier (aka site address)
of the RJE station. Valid values are in the range 0x70 - 0x7f. The default value
is `0x7a`.

### Using rjecli
When **rjecli** starts, it attempts to connect to the RJE host service specified by the `host` and `port` properties defined in the configuration file. When the
connection is complete and **rjecli** is ready to accept commands, it issues this
prompt:

    Operator> 

It recognizes the following commands:

- **help** or **?** : Displays help text.

- **load_cards** or **lc** : Load a file containing a batch job on the card reader.

  example: `lc /path/to/batch.job`

- **quit** or **q** : Exit from the RJE CLI.

- **send_command** or **sc** or **c** : Send an operator command to the RJE host.

  example: `c display dev`

The [examples](examples) directory contains example configuration and job files for the
CDC NOS and IBM MVS operating systems. Here is a transcript of an **rjecli** session
using the HASP protocol to communicate with RBF on CDC NOS 2.8.7. It uses the **c**
command to display the status of all devices associated with the RJE station, and it
uses the **lc** command to submit a job from the `examples` directory
(`examples/nos2.job`) for execution:
```
rje-station % node rjecli examples/nos2.json

RJE CLI starting ...

Operator>
WELCOME TO THE NOS SOFTWARE SYSTEM.
COPYRIGHT CONTROL DATA SYSTEMS INC. 1994.
22/04/11. 10.55.18. CO02P14
M01 - CYBER 865.                        NOS 2.8.7 871/871.
Operator>
RBF  VER 1.8-STARTED 22/04/11. 10.55.24.
READY
Operator> c display dev
Operator>
RBF DEVICES OF USER RJE1   /CYBER
EQ  STAT    NAME     FS RP FC/TR OPT WID   BS

CR1 GO                           ACK
LP1 GO                       /A9 ALL 132  400
CP1 GO                           A/B      400
*** END DISPLAY ***
Operator> lc examples/nos2.job

Operator>
Reading examples/nos2.job on CR1 ...
Done    examples/nos2.job on CR1 ...
10.56.01. CR1 JOB NAME AABO ENTERED INPUT QUEUE
Operator>
10.56.17. LP1 JOB NAME AABO
         6 PRU-S
Created ./spool/LP1_20220411105620457.txt
Operator>
Closed ./spool/LP1_20220411105620457.txt
END LP1
Operator> q
rje-station %
```
## <a id="rjews"></a>rjews
**rjews** is a web service enabling users to submit batch jobs to host
computer systems from their web browsers. It can interact with multiple host system and
multiple users concurrently, and each user may create multiple concurrent RJE
sessions. Normally, ordinary print and punch output produced by jobs submitted to
hosts from *rjews** will be returned automatically to it. **rjews** will
display these files as they are being received, and when reception is complete, it
will store them in the user's browser download directory. It creates a unique name
for each one.
    
### Starting rjews
Execute the following command to start rjews:

    node rjews

**rjews** accepts optional arguments specifying the pathnames of configuration files
and other operational parameters. The default configuration file pathname is
`./rjews.json`. The full syntax of the rjews startup command is:

```
node rjews [-h][--help][-p path][-v][--version] [path ...]
  -h, --help     display this usage information
  -p             pathname of file in which to write process id
  -v, --version  display program version number
  path           pathname(s) of configuration file(s)
```

### <a id="rjews-config"></a>Configuring rjews
**rjews** uses configuration files to define various operational parameters such
as the port number on which it will listen for browser connections, as well as
information about the RJE hosts (machines) to which it can create connections. The
default pathname of the configuration file is `./rjews.json`.

The contents of a typical configuration file look like this:

```
{
  "port":8085,
  "httpRoot":"www",
  "machines":[
    {
    "id":"nos1",
    "title":"EI200 on NOS 1.3 (Cyber 173)",
    "host":"localhost",
    "port":6671,
    "name":"GUEST",
    "password":"GUEST",
    "protocol":"mode4"
    },
    {
    "id":"nos2",
    "title":"RBF on NOS 2.8.7 (Cyber 865)",
    "host":"localhost",
    "port":2553,
    "protocol":"hasp"
    }
  ]
}
```

#### Common Configuration Properties
The common properties defined in an **rjews** configuration file are:
- **httpRoot** : specifies the pathname of the directory where **rjews** can find its
web artifacts (i.e., HTML, Javascript, CSS, etc. files).
- **port** : specifies the TCP port number on which **rjews** will listen for
connections from web browsers.
- **machines** : an array of definitions for RJE hosts to which **rjews** may connect.
Each element of the array is an object defining an RJE host, and the properties that
can be defined for each are described in
[machine configurartion properties](#rjews-machine), below.

#### <a id="rjews-machine"></a>Machine Configuration Properties
- **debug** : if set to *true*, **rjews** will log debugging information to the console
for the machine. The default is *false*. This also sets the default values of the
**bsc.debug** and **hasp.debug** properties.
- **host** : specifies the host name or IP address of the host where the RJE host service is located (e.g., the name of the host on which *DtCyber* is running). The default is *localhost*.
- **id** : specifies a unique identifier to be associated with the machine. This is
used internally by **rjews** to distinguish between machines, and it is displayed in
log messages.
- **name** : specifies the username or terminal name used in the *SIGNON* request
sent to the RJE host service. The default is none.
- **password** : specifies the password used in the *SIGNON* request sent to the RJE
host service. The default is none.
- **port** : the TCP port number on which the RJE host service is listening for connections. The default is *2553*.
- **protocol** : specifies the name of the RJE data communication protocol to use in
communicating with the RJE host service. Accepted values are *hasp* and *mode4*. The
default is *hasp*. 
- **title** : specifies a string providing a brief title or description of the machine.
This title/description is presented to users in windows titles and console messages. 
- **bsc** : an optional object defining configuration properties specific to the BSC data communication layer.
- **bsc.debug** : if set to *true*, the BSC layer will log debugging information to `bsc.log.txt`. The default is the value specified by the top-level **debug**
property.
- **hasp** : an optional object defining configuration properties specific to the HASP data communication layer.
- **hasp.debug** : if set to *true*, the HASP layer will log debugging information to file `hasp.log.txt`. The default is the value specified by the top-level **debug**
property.
- **hasp.maxBlockSize** : specifies the maximum number of bytes to send to the HASP
service host in each HASP protocol block. The default is *400*, which works well for
IBM MVS and CMS hosts.
- **mode4** : an optional object defining configuration properties specific to the MODE4 data communication layer.
- **mode4.debug** : if set to *true*, the MODE4 layer will log debugging information to file `mode4.log.txt`. The default is the value specified by the top-level
**debug** property.
- **mode4.siteId** : specifies the MODE4 layer site identifier (aka site address)
of the RJE station. Valid values are in the range 0x70 - 0x7f. The default value
is `0x7a`.

### Using rjews
When **rjews** starts, it begins listening for browser connection requests on the
port number defined in its [configuration](#rjews-config). For example, if the port
number is defined as 8085, a user on the local machine may enter the following URL
from their web browser to begin interacting with *rjews*:

```
http://localhost:8085
```

In this case, *rjews* will present a web page displaying a list of RJE hosts to which
it can establish connections. The user may then click on any of the links presented to
begin an RJE session with the associated host.

It is also possible to create a connection directly to a host if the unique identifier
associated with the host (in the machine configuration) is known. This is accomplished
using a URL of the form:

```
http://localhost:8085/rje.html?m=<machine-id>&t=<title>
```

where *&lt;machine-id&gt;* is the unique identifier defined for the machine in the configuration file, and *&lt;title&gt;* is a title for the machine that should be
presented in web pages. If the *t* parameter is omitted, the default title used is
*Unnamed Machine*.

When a machine has been selected in either of these ways, **rjews** will produce a
web page that emulates an RJE station and includes the following elements:


- **Console display window**. This is where console messages produced by the host will
appear. Connection status messages will appear here also.
- **Console command entry field**. This is where the user may enter RJE commands to be sent
to the host.
- **Card deck selection dialog**. This is a file selection dialog. Normally, when its
*choose file* button is pressed, the user's browser will present a file browsing
interface that allows the user to choose a local file. In this case, the local file
is presumed to contain the commands and data of a batch job to be submitted for
execution on the host. It represents a card deck that will be read using the virtual
card reader emulated by *rjews*.
- **Card deck submission button**. When this button is pressed, the last local file
selected using the card deck selection dialog is sent to the host. It initiates
emulation of the virtual card reader.

Soon after a card deck is sent to a host for execution, the host will return the
output produced by the job represented by the deck. As the output arrives, *rjews*
will present a pop-up window displaying it. When the output is complete, *rjews*
will create a uniquely named file in the browser's downloads directory containing the content, and it will close the pop-up window.

The [examples](examples) directory contains an example `rjews.json` configuration file
for *rjews* and example card decks (job files) for the CDC NOS and IBM MVS operating systems.
