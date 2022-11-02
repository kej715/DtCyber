# StorageTek 4400 ACS Simulator
The [StorageTek 4400 ACS](http://www-hpc.cea.fr/en/complexe/pages/20-silosSTK.htm)
was an automated cartridge tape system (ACS). A full configuration could manage
thousands of tape cartridges amounting to as much as a petabyte of data storage.
The CDC NOS 2 operating system (e.g., NOS 2.8.7) supported this device. It
communicated with the tape robot controller using
[ONC RPC protocol](https://en.wikipedia.org/wiki/Sun_RPC) over
[UDP/IP](https://en.wikipedia.org/wiki/User_Datagram_Protocol), and it used an
adapter to the device's
[FIPS mainframe channel interface](https://nvlpubs.nist.gov/nistpubs/Legacy/FIPS/fipspub60-1.pdf) to stream data to/from the tape
transports.

*DtCyber* simulates the CDC side of the FIPS channel adapter, and it supports
the NOS implementation of UDP/IP too. The *StorageTek 4400 ACS* simulator
implements the StorageTek side of the connection, including the ONC RPC programs
and procedures called by NOS to request tapes to be mounted and dismounted
automatically. The *StorageTek* simulator also manages a configurable collection of
virtual tape volumes, and it provides a simple web service that reports the current
state of the collection in JSON or text format.

## Building the Simulator
The *StorageTek 4400 ACS* simulator is implemented in Javascript using Node.js. To
build and run the simulator, you will need to install *Node.js* and *npm* on your
system, if you don't have them already. The minimum versions required are:

    Node.js: 16.x.x
    npm : 8.x.x

To build the simulator, enter the following command from this directory:

    npm install
    
## Starting the Simulator
To start the *StorageTek 4400 ACS* simulator, enter the following command from
this directory:

    npm start

## Configuration
The *StorageTek 4400 ACS* simulator has two configuration files, both of which
contain [JSON](https://www.json.org/json-en.html)-formatted text:

- **config.json** : provides information about TCP and UDP ports used and locations of tape images
- **volumes.json** : provides information about all tape volumes known to the simulator

### config.json
This file provides the simulator's base configuration. Its contents look like this:

```
{
"debug": false,
"httpServerPort": 4480,
"portMapperUdpPort": 111,
"tapeRobotPort": 4400,
"tapeServerPort": 4400,
"tapeLibraryRoot": "tapes",
"tapeCacheRoot": "tapes/cache"
}
```

The properties defined in `config.json` are:
- **debug** : if set to *true*, the simulator will log verbose debug information to
the console (stdout).
- **foreignPortMapper** : specifies the host name or address of a foreign ONC RPC
portmapper. When this property is specified, **portMapperUdpPort** is ignored, and
the simulator does not start its built-in portmapper. Instead, it calls the
specified foreign portmapper to register its remotely callable procedures. Specify
this property if the machine on which you are running the simulator has a portmapper
running already and does not need the simulator to start its built-in one. For
example, if the machine is running *NFS* or *rpcbind*, it has a portmapper running
already, and you should specify **foreignPortMapper** with a value of
**"127.0.0.1"**.
- **httpServerPort** : specifies the TCP port on which the simulator's web service
will listen for connections. The default is TCP port `4480`.
- **portMapperUdpPort** : The simulator implements an ONC RPC portmapper, and this
property specifies the UDP port on which it will listen for ONC RPC portmapper
requests. The default is UDP port `111`. On most systems, UDP port 111 is a
privileged port, so the simulator will need to be run using *sudo* on a Linux,
for example, in order for it to bind successfully to this port number.
- **tapeRobotPort** : specifies the UDP port on which the simulator will listen
for ONC RPC calls from *DtCyber* to mount and dismount tapes. The default is UDP port
`4400`.
- **tapeServerPort** : specifies the TCP port on which the simulator will listen
for channel connections from *DtCyber*. The default is TCP port `4400`.
- **tapeLibraryRoot** : specifies the pathname of the directory in which the
simulator will find locally-managed tape volume images. The default is `./tapes`.
- **tapeCacheRoot** : specifies the pathname of the directory in which the
simulator will cache tape volume images downloaded automatically from the web. The
default is `./cache`.

### volumes.json
This file defines all of the tape volumes known to the simulator. Typical contents
look like this:

```
{
"CENSUS":
  {
  "description":"1990 U.S. Census county-to-county commuting flows",
  "owner":"LIBRARY",
  "url":"https://www.dropbox.com/s/ttjzwgdyyg0ewz3/census.tap?dl=1",
  "labeled":true,
  "writeEnabled":false,
  "userOwned":false,
  "systemVsn":true,
  "format":"LI",
  "access":"PUBLIC",
  "readOnly":true,
  "listable":true
  },
"DDN000":
  {
  "description":"ICEM DDN model of SPACELAB",
  "owner":"LIBRARY",
  "url":"https://www.dropbox.com/s/ycx2ax1n6nsbi2z/spacelab.tap?dl=1",
  "labeled":true,
  "writeEnabled":false,
  "userOwned":false,
  "systemVsn":true,
  "format":"I",
  "access":"PUBLIC",
  "readOnly":true,
  "listable":true
  }
}
```

It defines a JSON-formatted map of volume definitions. The key of each volume
definition is the unique name (i.e., logical VSN) of a volume, and the value
associated with each key is a JSON object defining the properties of the
volume identified by the key. The supported properties are:

- **access** : the NOS Tape Management System (TMS) uses this property to
define the access mode of a volume. Its value can be one of *PRIVATE* (the tape
may be accessed by the owner only), *PUBLIC* (the tape may be accessed by anyone),
or *SPRIV* (the tape may be accessed only by users specifically permitted by
the tape's owner). The default is *PRIVATE*.
- **description** : provides a human-readable description of the tape volume. It
may be used by user interfaces to provide information about the contents of the
volume.
- **format** : TMS uses this property to define the NOS tape format. It can
be any of the format designators supported by NOS including *I* (NOS internal),
*LI* (NOS long-block internal), and *SI* (SCOPE internal). The default is *I*.
- **labeled** : TMS uses this boolean property to define whether the tape volume
has an ANSI label, or not. The default is *false*.
- **listable** : TMS uses this boolean property to define whether the NOS *AUDIT*
command will display information about the volume to users other than its owner.
The default is *false*.
- **owner** : TMS uses this property to identify the owner of the volume. Its
value is a NOS username, and there is no default value. If this property is not
defined, the following properties are ignored: *access*, *format*, *labeled*,
*listable*, and *readOnly*.
- **path** : specifies the file system pathname of a locally-managed tape image.
If this property is defined, *url* is ignored. A relative pathname is treated as
referencing a file in the directory specified by *tapeLibraryRoot* in `config.json`.
- **physicalName** : TMS uses this property to define the physical label recorded
on the volume in case it differs from the logical name defined by the volume's key
in the volume map. This property is optional and does not need to be provided
unless the logical and physical names of the volume differ.
- **readOnly** : TMS uses this boolean property to define whether users
other than the owner of the volume may request that the volume be mounted in
write mode. The default is *true* (i.e., volume is read-only).
- **systemVsn** : TMS uses this boolean property to define whether the volume
has a system VSN. A volume with a system VSN cannot be mounted as a scratch
volume. The default is *false* (i.e., does not have a system VSN).
- **url** : specifies the URL of the tape image. When this property is specified,
and the *path* property is not specified, the simulator will use the URL to
connect to the specified web service using HTTP or HTTPS, download the tape
image as needed, and cache it in the directory specified by *tapeCacheRoot* in
`config.json`.
- **userOwned** : TMS uses this property to define whether the volume is owned by
a user or by the data center. The default is *false* (i.e., owned by data center).
- **writeEnabled** : specifies whether the volume is physically write-enabled.
The default is *false* (i.e., the tape cannot be mounted in write mode).

## The Web Service
The *StorageTek 4400 ACS* simulator provides a web service that can be used to
query the volume configuration, and it can report the configuration in JSON format
or plain text format. The plain text format is directly suitable as an input file
to the NOS *TFSP* utility, so it can be used to synchronize (or initialize) the
TMS catalog with the simulator's volume configuration.

The URL format to use in querying the web service is:

```
http://<host>:<port>/volumes[?tfsp]
```

where `<host>` is the IP address or domainname of the host on which the simulator
is running, and `<port>` is the TCP port number defined by *httpServerPort* in `config.json` (default is `4480`). If `?tfsp` is specified on the URL, the web
service will report the configuration in plain text format, suitable as an input
file to *TFSP*. If `?tfsp` is omitted from the URL, the web service will report the
configuration in JSON format.

For example, if the simulator is running on the local host, the following `curl`
command can be used to obtain the volume configuration in plain text (suitable for
*TFSP*) format:

>`curl -o tfsp-input.txt http://localhost:4480/volumes?tfsp`

and the contents of `tfsp-input.txt` would look similar to:

```
VSN=CENSUS,OWNER=CENTER,SYSTEM=YES,SITE=ON,VT=AT,GO.
USER=LIBRARY
FILEV=CENSUS,AC=YES,CT=PUBLIC,D=AE,F=LI,LB=KL,M=READ,GO
GO
VSN=DDN000,OWNER=CENTER,SYSTEM=YES,SITE=ON,VT=AT,GO.
USER=LIBRARY
FILEV=DDN000,AC=YES,CT=PUBLIC,D=AE,F=I,LB=KL,M=READ,GO
GO
```

The following `curl` command can be used to obtain the volume configuration in
JSON format:

>`curl -o report.json http://localhost:4480/volumes`

and the contents of `report.json` would look similar to:

```
{
"CENSUS":{
  "description":"1990 U.S. Census county-to-county commuting flows",
  "owner":"LIBRARY",
  "url":"https://www.dropbox.com/s/ttjzwgdyyg0ewz3/census.tap?dl=1",
  "labeled":true,
  "writeEnabled":false,
  "userOwned":false,
  "systemVsn":true,
  "format":"LI",
  "access":"PUBLIC",
  "readOnly":true,
  "listable":true
  },
"DDN000":{
  "description":"ICEM DDN model of SPACELAB",
  "owner":"LIBRARY",
  "url":"https://www.dropbox.com/s/ycx2ax1n6nsbi2z/spacelab.tap?dl=1",
  "labeled":true,
  "writeEnabled":false,
  "userOwned":false,
  "systemVsn":true,
  "format":"I",
  "access":"PUBLIC",
  "readOnly":true,
  "listable":true
  }
}
```

