# Examples
This directory contains the following example batch jobs and configuration files:

- **mvs.job** : An example of a job that may be submitted to JES2 on IBM MVS.
You will probably need to change the username and/or password specified in the
job before it will run successfully on your MVS system.
- **mvs.json** : An example of an **rjecli** configuration file for interacting
with JES2 on IBM MVS. You will probably need to change the `host` and/or `port`
properties to match what your *Hercules* based MVS system expects. You might
also need to change the `name` property to match what JES2 expects for the
`REMOTEnnn` name of the RJE terminal associated with `port`.
- **nos.job** : An example of a job that may be submitted to RBF on CDC
NOS 2. You might need to change the username and/or password on the USER
statement before it will run successfully on your NOS system. However, the
username and password specified in this example match the default values
used in the NOS 2.8.7 `install-from-scratch` system of the NOS2.8.7 subtree
of this directory's parent repository.
- **nos1.json** : An example of an **rjecli** configuration file for interacting
with EI200 on CDC NOS 1. You might need to change the `host` and/or `port`
properties to match what your *DtCyber* based NOS 1 system expects, and you might
also need to change the `name` and `password` properties to match what EI200
will accept for the login username and password of the RJE terminal. However, the
property values specified in this example match the default values
used in the NOS 1.3 `install-from-scratch` system of the NOS1.3 subtree
of this directory's parent repository.
- **nos2.json** : An example of an **rjecli** configuration file for interacting
with RBF on CDC NOS 2. You might need to change the `host` and/or `port`
properties to match what your *DtCyber* based NOS system expects. However, the
property values specified in this example match the default values
used in the NOS 2.8.7 `install-from-scratch` system of the NOS2.8.7 subtree
of this directory's parent repository.