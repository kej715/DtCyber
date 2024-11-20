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

Furthermore, *rjews* provides a [web service API](#rjews-api) that may be used in
implementing custom-built RJE applications.

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

The contents of a typical configuration file looks like this:

```
{
  "debug": false,
  "protocol":"hasp",
  "host":"127.0.0.1",
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

Note also that values of the top-level properties, such as the `host` property, may be
specified as special references to values in other property files such as `cyber.ini`
files. The general syntax of these special references is:

```
"%<pathname>|<section-name>|<property-name>|<default-value>"
```

where *pathname* is the relative pathname (relative to the RJE configuration file) of
the target property file, *section-name* is the name of the target section in the
property file, *property-name* is the name of the target property, and *default-value*
is the value to use when the property file, section, or named property cannot be found.

Example:

```
{
  "debug": false,
  "protocol":"hasp",
  "host":"%../../NOS1.3/cyber.ini|cyber|ipAddress|127.0.0.1",
  "port":2553
}
```

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

The contents of a typical configuration file looks like this:

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
    "protocol":"hasp",
    "commandHelp":"/help/rbf.html",
    "sampleJobs":[
      {"title":"FTN5: Generate Fibonacci Series","url":"/samples/fib.job"},
      {"title":"COBOL: Produce accounting report","url":"/samples/acct.job"}
    ]
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
- **commandHelp** : specifies the URL of a file containing help information about the
operator commands that may be entered on the RJE console. The content of the file may be
HTML, e.g., an HTML table describing commands that are recognized. The URL is relative to the
**httpRoot** defined above.
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
- **maxConnections** : the maximum number of concurrent connections supported by the service
on its TCP port. The default is *1*.
- **port** : the TCP port number on which the RJE host service is listening for connections. The default is *2553*.
- **protocol** : specifies the name of the RJE data communication protocol to use in
communicating with the RJE host service. Accepted values are *hasp* and *mode4*. The
default is *hasp*.
- **sampleJobs** : Specifies an array of objects, each of which defines a sample job that can
be loaded into the job composer window supported by the web-based RJE user interface (see
[Using rjews](#using-rjews), below. Each object contains two properties: **title** defining
the name of the sample job, and **url** defining the URL of a file containing the sample job,
where the URL is relative to the **httpRoot** property defined above.
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

### <a id="using-rjews"></a>Using rjews
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

### <a id="rjews-api"></a>rjews API

**rjews** exposes a web service API that may be used in developing custom-built RJE
applications. The API has two principal interfaces:

* **HTTP interface**. HTTP GET requests may be used to obtain information about the RJE
services offered by **rjews**, including the names of each of these services. HTTP GET
requests may also be used to request connection numbers for these services.
* **WebSocket interface**. Having requested a connection number for a specific RJE service,
an application may then use a WebSocket to establish a connection with that RJE service and
interact with it. For example, jobs may be submitted by sending card deck images across the
WebSocket interface, and output from jobs is received across the WebSocket interface.

        **Note**: Libraries providing API's for making HTTP requests and for managing and
        interacting with WebSockets can easily be found for most modern programming languages
        and frameworks, including Python, Node.js, JAVA, C++, etc.

To discover information about the RJE services offered by **rjews**, an application creates
an HTTP connection to the HTTP port number defined in the **rjews** configuration file (e.g., port 8085). It then sends an HTTP GET request specifying the following URL:
```
/machines
```
The same request can be issued interactively from a web browser by entering a URL that looks
like:
```
http://localhost:8085/machines
```
**rjews** will respond with a nested JSON object describing all of the services offered.
The object will contain a property for each service. The name of the property describing a
service will be the name of the service itself. The property's value will be an object
describing details about the service. For example, the response to a `/machines` request
might look like:
```
{
  "max":{
    "id":"max",
    "title":"RBF on NOS 2.8.7 (Cyber 865)",
    "host":"192.168.1.238",
    "port":2556,
    "protocol":"hasp",
    "maxConnections":4,
    "curConnections":0,
    "commandHelp":"/help/rbf.html",
    "sampleJobs":[
      {"title":"FTN5: Generate Fibonacci Series",
       "url":"/samples/ncc-nos2-fib.job"},
      {"title":"Cray X-MP: Compile and run on Cray X-MP supercomputer",
       "url":"/samples/ncc-cos-fib.job"}
    ]
  },
  "moe":{
    "id":"moe",
    "title":"EI200 on NOS 1.3 (Cyber 173)",
    "host":"192.168.1.38",
    "port":6671,
    "name":"GUEST",
    "password":"GUEST",
    "protocol":"mode4",
    "maxConnections":1,
    "curConnections":0,
    "commandHelp":"/help/ei200.html",
    "sampleJobs":[
      {"title":"FTN: Generate Fibonacci Series",
       "url":"/samples/ncc-nos1-fib.job"}
    ]
  }
}
```
This describes two RJE services named *max* and *moe*. *max* is a HASP service running on
NOS 2.8.7, and *moe* is a MODE4 service running on NOS 1.3.

Connecting to these services involves requesting a connection number and then using that
connection number to connect to a WebSocket associated with it. To request a connection
number, an application sends an HTTP GET request to **rjews** specifying a URL that looks
like:
```
/machines?machine=max
```
This requests a connection number for the service named *max*. **rjews** will respond with
a simple number in plain text (content type *text/plain*), e.g.:
```
0
```
The application then uses this to create a WebSocket connection using a WebSocket URL that
looks like:
```
ws://localhost:8085/connections/0
```
where the **0** in **/connections/0** reflects the connection number returned in the HTTP
response previously received.

As soon as the WebSocket connection is established, the application may begin using it to
interact with the RJE service. Typically, the RJE service will send login banner messages
almost immediately after the connection is established, and these will arrive on the RJE
console stream. This is particularly true for RJE connections established with RBF on NOS 2.

Every message exchanged between the application and **rjews** on the WebSocket connection
has four fields, defined as follows:
```
<stream-type> <stream-id> <content-length> <content>
```
* **&lt;stream-type&gt;** One of the numbers *0*, *1*, *2*, or *3*. *0* indicates that the
content is associated with a console stream, *1* indicates that the content is associated with a card reader stream, *2* indicates a line printer stream, and *3* indicates a punch stream.
* **&lt;stream-id&gt;** Identifies the stream number. Valid values are typically in the range
1..7. HASP services can have 1 to 7 card reader streams, 1 to 7 line printer streams, and
1 to 7 punch streams. However, they usually have only one of each. MODE4 connections have only
one card reader and one line printer stream. Both HASP and MODE4 have one console stream.
* **&lt;content-length&gt;** An integer value indicating the length, in number of characters,
of the content field that follows. A value of 0 indicates end of file. It must be sent by the
application to indicate the end of a card deck (i.e., end of job being sent). It is sent by
**rjews** to indicate the end of a print or punch file.
* **&lt;content&gt;** The content. Each record within the content is terminated by a newline
character (ASCII &lt;LF&gt;).

Note also that the four fields are delimited from each other by a blank (ASCII &lt;SP&gt;),
and this applies to the end-of-file indication too. Specifially, the content length value of
**0** that indicates end-of-file must be followed by a blank.

For example, a message received from **rjews** on a console stream would look like:
```
0 1 13 PLEASE LOGIN<LF>
```
Note that *&lt;LF&gt;*, above, represents a newline character.

Similarly, a command sent by an application to **rjews** on a console stream would look like:
```
0 1 12 DISPLAY,DEV<LF>
```
A card image sent by an application to **rjews** on a card reader stream would look like:
```
1 1 9 CATLIST.<LF>
```
It is acceptable to send multiple card images as the content of a single message.

To terminate a session with **rjews**, the application simply closes the WebSocket
connection.