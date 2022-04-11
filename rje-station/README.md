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
such protocol implemented by many computer system manufacturers in the 1970's, 80's, and 90's. For example, CDC operating systems such as NOS and NOS/BE supported HASP.
The NOS 2 operating system provided HASP in its RBF (Remote Batch Facility)
subsystem.

This directory provides Javascript classes implementing HASP and
[BSC](https://en.wikipedia.org/wiki/Binary_Synchronous_Communications) protocols over
TCP/IP. The implementation is designed to execute using Node.js, and it interoperates
with *DtCyber* as well as the *Hercules* IBM mainframe simulator. In addition, a
command line application named **rjecli** is provided.

**rjecli** enables a user to submit jobs to RBF on NOS 2 (running on *DtCyber*), and
JES2 on IBM MVS or RCSC on IBM CMS (running on *Hercules*). Normally, ordinary print
and punch output produced by these jobs will be returned automatically to **rjecli**.
**rjecli** creates a file with a unique name in its spooling directory for each
print and punch file returned. The default spooling directory pathname is `./spool`.
A different pathname can be defined in the **rjecli** configuration file (see below). 

## Installing rjecli
Execute the following commands to install and run **rjecli** from this directory:

    npm install
    node rjecli

**rjecli** accepts an optional parameter specifying the pathname of a configuration file. The default configuration file pathname is `./config.json`. To specify a different configuration file, execute **rjecli** as in:

    node rjecli /path/to/file.json

## Configuring rjecli
**rjecli** uses a configuration file to define the host and TCP port where the RJE
host service is located. The configuration file can also define other optional 
parameters. The default pathname of the configuration file is `./config.json`.

The contents of the configuration file look like this:

```
{
  "debug": false,
  "host":"localhost",
  "port":2555,
  "bsc":{
    "debug":false
  },
  "hasp":{
    "debug":true,
    "LP1":{
      "isPostPrint":true
    }
  }
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
- **spoolDir** : specifies the pathname of a directory in which print and punch files
received from the RJE host service will be stored. The default is *./spool*.
- **bsc** : an optional object defining configuration properties specific to the BSC data communication layer.
- **bsc.debug** : if set to *true*, the BSC layer will log debugging information to the console. The default is the value specified by the top-level **debug**
property.
- **hasp** : an optional object defining configuration properties specific to the HASP data communication layer.
- **hasp.debug** : if set to *true*, the HASP layer will log debugging information to the console. The default is the value specified by the top-level **debug**
property.
- **hasp.LP1** : an optional object defining configuration properties specific to the LP1 printer stream.
- **hasp.LP1.isPostPrint** : if set to *true* (the default value), format effectors
for LP1 are handled as post-print (i.e., after the line with which they're associated
in HASP protocol elements). If set to *false*, format effectors for LP1 are handled
as pre-print. The value chosen must match the corresponding terminal definition
on the CDC NOS operating system (i.e., in the *NDL* file).
- **hasp.maxBlockSize** : specifies the maximum number of bytes to send to the HASP
service host in each HASP protocol block. The default is *400*, which works well for
IBM MVS and CMS hosts.

## Using rjecli
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

  example: `c display,alld`
