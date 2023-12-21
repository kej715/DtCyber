# TCP/IP Applications in NOS 2.8.7

## Table of Contents
- [Overview](#overview)
- [HTF - HTTP Server](#htf)
- [NSQUERY](#nsquery)
- [REXEC Client](#rexec-client)
- [REXEC Server](#rexec-server)
- [SMTP Client](#smtp-client)
- [SMTP Server](#smtp-server)

## <a id="overview"></a>Overview
CDCNet supported the TCP/IP data communication protocol stack and exposed this functionality
to NOS via a component named the *TCP/IP Gateway*. To NOS, the TCP/IP Gateway appeared as a
remote application with which it could interact using NAM's standard
application-to-application communication feature. NOS applications could connect to the
*TCP/IP Gateway* and exchange data and control information with it. Control information, for
example, could include commands to connect via TCP to other systems on the network. Control
Data provided three TCP/IP applications with NOS 2.8.7:

- **ATF subsystem**. The Automated Tape Facility was a NOS subsystem that used UDP and [ONCRPC](https://en.wikipedia.org/wiki/Sun_RPC) to communicate with a StorageTek 4400 automated tape library.
- **FTP server**. This NAM application enabled remote users to exchange files with NOS using the Internet-standard file transfer protocol.
- **FTP client**. This NAM application enabled users of NOS to exchange files with other systems on the network using the Internet-standard file transfer protocol. The FTP client was exposed to users as the NOS `FTP` command.

*DtCyber* emulates the *TCP/IP Gateway*, and this enables the three CDC-provided TCP/IP
applications to function normally on NOS 2.8.7. The
[Nostalgic Computing Center](http://www.nostalgiccomputing.org) has also used the *TCP/IP
Gateway* interface to implement the following additional TCP/IP applications and has
contributed them to this NOS 2.8.7 repository:

- **HTF**. The Hypertext Transfer Facility is an HTTP server providing many of the features typically found in modern HTTP servers.
- **NSQUERY**. NSQUERY is a command line tool enabling NOS 2.8.7 users to lookup the IP addresses of other hosts in the network.
- **REXEC server**. The REXEC server implements the BSD *rexec* protocol, enabling users on other hosts in the network to execute commands remotely on NOS 2.8.7.
- **REXEC client**. The REXEC client implements the BSD *rexec* protocol, enabling users of NOS 2.8.7 to execute commands remotely on other hosts in the network.
- **SMTP server**. The SMTP server implements the Internet-standard RFC821 protocol, enabling NOS 2.8.7 to receive e-mail from other hosts in the network.
- **SMTP client**. The SMTP client implements the Internet-standard RFC821 protocol, enabling NOS 2.8.7 to send e-mail to other hosts in the network.

These applications are described in detail, below.

## <a id="htf"></a>HTF - HTTP Server
HTF (Hypertext Transfer Facility) is an HTTP server for NOS 2.8.7. Ordinarily, it is started
automatically by NAM during the system deadstart process. The general syntax of the command
used by NAM in starting the application is:

```
HTF[,A=<aname>][,CF=<cfgfile>][,D=<dblvl>][,FN=<fam>][,N=<mxconns>][,P=<port>][,UN=<user>].
```

- *aname*. The application name with which HTF should connect to NAM. The default is **HTTPS**.
- *cfgfile*. The name of a file containing configuration parameters. The default is **HTFCONF**. If a file name is specified, HTF looks first for a local file by the specified name. If a local file with the specified name does not exist, HTF looks for a permanent file.
- *dblvl*. The debug logging level under which HTF should run. The default is **0** (no debug logging). The higher the number, the more verbose the logging. The meaningful range is 0 .. 3.
- *fam*. The family name under which jobs created by HTF should run. The default is **CYBER** (ordinarily, the system default family name). HTF creates jobs to process requests by users to execute CCL procedures.
- *mxconns*. The maximum number of concurrent network connections that HTF should support. The default is **30**.
- *port*. The TCP port on which HTF should listen for network connections. The default is **80**.
- *user*. The username under which jobs created by HTF should run. The default is **WWW**. HTF creates jobs to process requests by users to execute CCL procedures.

### HTF Configuration File
When a configuration file is specified via the **CF** command line parameter, or if a default
configuration file named **HTFCONF** exists, parameters defined within it will override any
that might also be defined on the command line. A configuration file is a simple text file
specifying one configuration parameter per line, and each such line has the form:

>`name=value`

The configuration file parameters recognized by HTF are:

- **APPNAME=***aname*
- **CONNS=***mxconns*
- **FN=***fam*
- **PORT=***port*
- **PW=***pass*
- **UN=***user*

The configuration file parameter values have the same meanings as defined in the command line
parameters, above, with the one addition of *pass*. This specifies the password under which
jobs created by HTF should run. The default is **WWWX**. HTF creates jobs to process requests
by users to execute CCL procedures.

### Web Root
The permanent file catalog of the username under which HTF is initiated serves as the public
web root of the web server. Ordinarily, **WWW** is the username under which HTF is initiated.
All of the files in this user's catalog are treated as public web resources, with the
exception of the HTF configuration file.

### URL Format
HTF recognizes the following general URL form:

>`http://host[:port]/[~user/]fname[.ftype][?query-string]`

Simple examples include:

>`http://www.nostalgiccomputing.org/index.html`

>`http://www.nostalgiccomputing.org/sitemap.xml`

>`http://www.nostalgiccomputing.org/ping.exe`

>`http://www.nostalgiccomputing.org/echo.proc?t=$please%20echo%20me$`

>`http://www.nostalgiccomputing.org/~guest/index.html`

The file name (the *fname* part) in a URL is taken literally and must match the name of a
file in the permanent file catalog. The file type (the *ftype* part) is optional. If
specified, it is interpreted as an indication of the type of content in the file. The
following file types are recognized:

- **.a64** The file contains CDC Display Code text (i.e., upper case only text).
- **.bmp** The file contains a BMP image.
- **.css** The file contains HTML style definitions.
- **.enc** The file contains an HTML form.
- **.exe** The file contains a NOS executable. A job is created to execute the file, and the job's output is returned as the URL's content.
- **.gif** The file contains a GIF image.
- **.html** The file contains HTML text.
- **.job** The contains a NOS batch job. The job is executed, and its output, including dayfile, is returned as the URL's content.
- **.jpg** The file contains a JPEG image.
- **.js** The file contains JavaScript.
- **.json** The file contains a JSON object.
- **.pdf** The file contains PDF text.
- **.php** The file contains a PHP script.
- **.png** The file contains a PNG image.
- **.proc** The file contains a NOS CCL procedure. A job is created to execute the procedure, and the job's output is returned as the URL's content. If query string parameters are specified in the URL, they are passed as parameters to the CCL procedure.
- **.txt** The file contains CDC 6/12 Extended Display Code text (i.e., upper and lower case text).
- **.xml** The file contains XML text.

When an HTTP POST request is sent to HTF, HTF will also recognize the following special media
types in the *Content-Type* header of the request and process the body of the POST request
accordingly:

- **application/vnd.cdc-executable** The POST request contains a NOS executable. For example, it is an executable binary file produced by a compiler, or it is a CCL procedure. A job is created to execute the object, and the job's output is returned as the result of the POST request.
- **application/vnd.cdc-job** The POST request contains a NOS batch job. The job is executed, and its output, including dayfile, is returned as the result of the POST request.
- **application/vnd.cdc-proc** The POST request contains a CCL procedure. A job is created to execute the procedure, and the job's output is returned as the result of the POST request. If query string parameters are specified in the URL of the POST request, they are passed as parameters to the CCL procedure.

For example, the following simple Unix/Linux shell script uses the `curl` utility to submit
batch jobs to specified NOS 2.8.7 hosts:

```
#!/bin/bash
#
#. $1 - name of host to which to submit the job
#. $2 - pathname of file containing the job
#
curl -X POST -H 'Content-Type: application/vnd.cdc-job' --data-binary @$2 http://$1/
```

Note that files containing batch jobs can include special lines representing end of record, end of file, and end of information indications. Specifically, lines consisting entirely of the following keywords are recognized:

- **~eof** End of file indication.
- **~eoi** End of information indication.
- **~eor** End of record indication.

Here is a simple example:

```
MYJOB.
USER,GUEST,GUEST.
FTN5,GO.
~eor
      PROGRAM HELLO
      PRINT *, 'HELLO WORLD!'
      END
```

### Binary Files
HTF recognizes the file format used by the NOS 2.8.7 FTP server when users upload binary
files such as files containing icons and graphical images. Specifically, when a user uses
FTP to upload a binary file, the NOS FTP server appends a special record to the end of the
file. This special record defines how many valid bits exist in the last 60-bit word of actual
data in the file. When a browser requests a file from HTF that has been uploaded in this
format, HTF will read the special record and use it to ensure that no extra *garbage* bits are
returned to the browser.

Thus, **the NOS FTP server should be used for uploading binary content to be served by HTF,
such as files containing images and icons**. This will ensure that HTF does not return extra
bits when serving these files back. 


## <a id="nsquery"></a>NSQUERY
NSQUERY is a command line enabling users to lookup the IP addresses of hosts in the network.'
The general syntax of the command is:

```
NSQUERY[,A=<aname>][,D=<dblvl>].domain.name
```

- *aname*. The application name with which NSQUERY should connect to NAM. The default is **TCP**.
- *dblvl*. The debug logging level under which NSQUERY should run. The default is **0** (no debug logging). The higher the number, the more verbose the logging. The meaningful range is 0 .. 3.

Typical usage is:

>`NSQUERY.www.google.com`


## <a id="rexec-client"></a>REXEC Client
REXEC is a user command implementing the Berkeley
[*rexec*](https://en.wikipedia.org/wiki/Berkeley_r-commands) protocol. It enables users of
NOS 2.8.7 to execute commands remotely on other hosts in the network. The general syntax of
the command is:

```
REXEC[,A=<aname>][,D=<dblvl>][,H=<host>][,I=<ifile>][,O=<ofile>][,P=<port>][,Z].[command]
```

- *aname*. The application name with which REXEC should connect to NAM. The default is **REXEC**.
- *dblvl*. The debug logging level under which REXECS should run. The default is **0** (no debug logging). The higher the number, the more verbose the logging. The meaningful range is 0 .. 3.
- *host*. The name of the host to which commands will be sent. This parameter is required.
- *ifile*. Optional name of a file from which to read commands. If this parameter is specified, and the **Z** parameter is not specified, commands are read from this file and sent to the remote host for execution, one by one until end of record is reached. The default is **INPUT**, except that this parameter is ignored when **Z** is specified.
- *ofile*. Optional name of a file on which to write output received from the remote host. The default is **OUTPUT**.
- *port*. The TCP port to which REXEC should connect on the remote host. The default is **512**.

Note that when the **Z** parameter is specified, the command to be sent to the remote host is
taken from the end of the command line, immediately following the command line terminator.

The *rexec* protocol involves connecting to a remote host and then sending a username,
password, and command. The NOS 2.8.7 REXEC command obtains the remote host username and
password from a file in the NOS 2.8.7 user's permanent file catalog named `FTPPRLG`. This file
is also used by the NOS FTP command to support automatic login to remote hosts during file
transfer sessions.

See section 3 of the
[CDCNet TCP/IP Usage Guide](http://bitsavers.trailing-edge.com/pdf/cdc/cyber/comm/cdcnet/60000214B_CDCNET_TCP_IP_Usage_Apr88.pdf)
for details about the FTP prolog file (**FTPPRLG**). In particular, this file usually contains
a sequence of **DEFAL** (DEFINE_AUTO_LOGIN) commands defining the names of remote hosts and
the usernames and passwords associated with them.

Examples of REXEC command usage:

>`REXEC,H=NOS01,Z.CATLIST`

>`REXEC,H=UNIX2,Z.^L^S -^A^L`

>`REXEC,H=VMS1,Z.DIR *.TXT`

The first example represents a request to send a CATLIST command to a remote NOS host named NOS01. The second example represents a request to send the command `ls -al` to a remote Unix host named UNIX2. Note that it is necessary to use 6/12 display code representation explicitly
in order to send lower case letters. The third example represents a request to send a command to a remote VAX/VMS host.

## <a id="rexec-server"></a>REXEC Server
REXECS is an *rexec* server for NOS 2.8.7. It implements the Berkeley
[*rexec*](https://en.wikipedia.org/wiki/Berkeley_r-commands) protocol. It enables users on
other hosts in the network to execute commands remotely on NOS 2.8.7. Ordinarily,
it is started automatically by NAM during the system deadstart process. The general syntax of
the command used by NAM in starting the application is:

```
REXECS[,A=<aname>][,CF=<cfgfile>][,D=<dblvl>][,N=<mxconns>][,P=<port>][,UN=<user>].
```

- *aname*. The application name with which SMTPS should connect to NAM. The default is **REXECS**.
- *cfgfile*. The name of a file containing configuration parameters. The default is **REXCONF**. If a file name is specified, REXECS looks first for a local file by the specified name. If a local file with the specified name does not exist, REXECS looks for a permanent file.
- *dblvl*. The debug logging level under which REXECS should run. The default is **0** (no debug logging). The higher the number, the more verbose the logging. The meaningful range is 0 .. 3.
- *mxconns*. The maximum number of concurrent network connections that REXECS should support. The default is **30**.
- *port*. The TCP port on which REXECS should listen for network connections. The default is **512**.
- *user*. The owning username that REXECS will associate with jobs that is submits to NOS
for execution. This is used in finding output from these jobs. The default is **REXEC**.

### REXECS Configuration File
When a configuration file is specified via the **CF** command line parameter, or if a default
configuration file named **REXCONF** exists, parameters defined within it will override any
that might also be defined on the command line. A configuration file is a simple text file
specifying one configuration parameter per line, and each such line has the form:

>`name=value`

The configuration file parameters recognized by SMTPS are:

- **APPNAME=***aname*
- **CONNS=***mxconns*
- **PORT=***port*
- **UN=***user*

The configuration file parameter values have the same meanings as defined in the command line
parameters, above.


## <a id="smtp-client"></a>SMTP Client
SMTPI (Simple Mail Transfer Protocol Initiator) is an SMTP client for NOS 2.8.7. Ordinarily,
it is started automatically by NAM during the system deadstart process. The general syntax of
the command used by NAM in starting the application is:

```
SMTPI[,A=<aname>][,CF=<cfgfile>][,D=<dblvl>][,DFC=<dfc>][,IFC=<ifc>][,RFC=<rfc>][,UN=<user>].
```

- *aname*. The application name with which SMTPI should connect to NAM. The default is **SMTP**.
- *cfgfile*. The name of a file containing configuration parameters. The default is **SMICONF**. If a file name is specified, SMTPI looks first for a local file by the specified name. If a local file with the specified name does not exist, SMTPI looks for a permanent file.
- *dblvl*. The debug logging level under which SMTPI should run. The default is **0** (no debug logging). The higher the number, the more verbose the logging. The meaningful range is 0 .. 3.
- *dfc*. The forms code with which deferred messages should be queued. If SMTPI is unable to send a mail message to a destination host, and the maximum delivery time has not expired, the message is returned to the NOS output queue with this forms code. The default is **SD**.
- *ifc*. The forms code with which new mail messages to be delivered are queued. Ordinarily, this is the forms code that the UMass Mailer associates with outbound e-mail messages to be delivered using the SMTP protocol. The default is **SM**.
- *rfc*. The forms code with which rejected mail messages should be queued. If SMTPI is unable to send a mail message to a destination host, and the maximum delivery time has expired, SMTPI generates a nondelivery notice and places it in the NOS output queue with this forms code. Ordinarily, this is the forms code that the UMass Mailer associates with inbound e-mail messages. The default is **ID**.
- *user*. The username that SMTPI will associate with mail messages it places in the NOS output queue. The default is **NETOPS**.

### SMTPI Configuration File
When a configuration file is specified via the **CF** command line parameter, or if a default
configuration file named **SMICONF** exists, parameters defined within it will override any
that might also be defined on the command line. A configuration file is a simple text file
specifying one configuration parameter per line, and each such line has the form:

>`name=value`

The configuration file parameters recognized by HTF are:

- **APPNAME=***aname*
- **DFC=***dfc*
- **IFC=***ifc*
- **RFC=***rfc*
- **SC=***svcls*
- **UN=***user*

The configuration file parameter values have the same meanings as defined in the command line
parameters, above, with the one addition of *svcls*. This specifies the service class with
which SMTPI should associate mail messages it places in the NOS output queue. The default is
**SY**.

## <a id="smtp-server"></a>SMTP Server
SMTPS (Simple Mail Transfer Protocol Server) is an SMTP server for NOS 2.8.7. Ordinarily,
it is started automatically by NAM during the system deadstart process. The general syntax of
the command used by NAM in starting the application is:

```
SMTPS[,A=<aname>][,CF=<cfgfile>][,D=<dblvl>][,FC=<fc>][,N=<mxconns>][,P=<port>][,TZ=<tz>][,UN=<user>].
```

- *aname*. The application name with which SMTPS should connect to NAM. The default is **SMPS**.
- *cfgfile*. The name of a file containing configuration parameters. The default is **SMSCONF**. If a file name is specified, SMTPS looks first for a local file by the specified name. If a local file with the specified name does not exist, SMTPS looks for a permanent file.
- *dblvl*. The debug logging level under which SMTPS should run. The default is **0** (no debug logging). The higher the number, the more verbose the logging. The meaningful range is 0 .. 3.
- *fc*. The forms code with which messages received by SMTPS should be queued. Ordinarily, this is the forms code that the UMass Mailer associates with inbound e-mail messages. The default is **ID**.
- *mxconns*. The maximum number of concurrent network connections that SMTPS should support. The default is **30**.
- *port*. The TCP port on which SMTPS should listen for network connections. The default is **25**.
- *tz*. The time zone in which SMTPS is running. The default is **-0500** (US Eastern Standard Time).
- *user*. The username that SMTPS will associate with mail messages it places in the NOS output queue. The default is **NETOPS**.

### SMTPS Configuration File
When a configuration file is specified via the **CF** command line parameter, or if a default
configuration file named **SMSCONF** exists, parameters defined within it will override any
that might also be defined on the command line. A configuration file is a simple text file
specifying one configuration parameter per line, and each such line has the form:

>`name=value`

The configuration file parameters recognized by SMTPS are:

- **APPNAME=***aname*
- **CONNS=***mxconns*
- **FC=***fc*
- **PORT=***port*
- **TZ=***tz*
- **SC=***svcls*
- **UN=***user*

The configuration file parameter values have the same meanings as defined in the command line
parameters, above.

