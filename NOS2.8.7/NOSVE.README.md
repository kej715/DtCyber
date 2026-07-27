[NOS/VE](https://en.wikipedia.org/wiki/NOS/VE) is the operating system and related software products for [CDC's Cyber 180 architecture](https://en.wikipedia.org/wiki/CDC_Cyber#Cyber_180_series).  DtCyber supports running NOS/VE in dual-state mode, in which NOS and NOS/VE share the same machine, mediated by a hypervisor.

This release for DtCyber contains NOS/VE v1.8.3AA.  It was built from deadstart and product release tapes, including a BCU tape that included corrections for Y2K, so it was probably released in the late 1990's.  The emulated machine is the Cyber 870 (because hardware-specific tape for it survived).

# Installing and Deadstarting

First, build DtCyber with the instructions in BUILDING.README.

NOS/VE support is included in the NOS 2.8.7 image.  Download and install NOS/VE with:
```
cd NOS2.8.7
sudo ​node install rtr dual-state-860
```
This script will immediately deadstart (CDC's word for booting) NOS and NOS/VE after the install.  

README.md in the NOS2.8.7 directory describes the NOS installation in detail.

>Note:  The following message is written to the NOS console a number of times and then stops:
>
>CHANNEL/EQUIPMENT/UNIT NOT IN EST
>
>This is normal. NVE issues this message when a device or channel used by NOS/VE is not defined in the NOS equipment deck. Defining NOS/VE devices in the NOS equipment deck is optional, and they are not defined in the NOS configuration of the ready-to-run "dual-state-860" package.

During startup, a number of windows will open including one with the title "TPM 0". This is a window that runs a CDC Viking 721 terminal emulator connected to port 0 of the Cyber 860's two-port multiplexor (hence "TPM 0").  TPM port 0 is configured to be the NOS/VE console window.

Near the end of the NOS deadstart, it initiates its subsystems, and one of the subsystems in the ready-to-run package is named "NVE". It is started at control point 3. "NVE" is a NOS subsystem that initiates the deadstart and shutdown of NOS/VE, and it monitors NOS/VE while it is running. Thus, immediately after the "NVE" subsystem has started, the "TPM 0" window begins displaying NOS/VE deadstart messages. Eventually, the NOS/VE deadstart will complete, and the "TPM 0" window will display the following prompt:
```
sou/
```
"sou" is the NOS/VE System Operator Utility. When this prompt appears, the NOS/VE deadstart is complete. To see it do something, enter:
```
display_catalog
```
>Note:  The TPM 1 window connects to port 1 of the two-port mux. In the ready-to-run package, port 1 is not configured to be active. However, it is possible to use CIP at deadstart time to activate the port for use by DFT, the dedicated fault tolerance PP. You can then use it to interact with DFT. It will respond to maintenance-related commands and display associated responses.

# Interactive Usage
From NOS interactive:
```
telnet localhost 23
FAMILY:<return>
USER:  guest
PASSWORD:  guest

HELLO,VEIAF
```

# Running Batch Jobs

In the ready-to-run package, NOS/VE is assigned the LID "NVE", and you can use this LID with the ROUTE command to submit batch jobs to it. For example, here is a trivial NOS/VE job file that will list the contents of the $LOCAL catalog:

```
login guest guest
disc $local
```
To submit it to NOS/VE from NOS, use this command:
```
ROUTE,lfn,DC=IN,ST=NVE
```
Note that NOS/VE uses 8-bit characters (e.g., ASCII), so you can submit jobs to it from NOS using mixed case (6/12 Extended Display Code on NOS).

Using a card reader, you can read a card deck that looks like:
```
JOB.
USER,GUEST,GUEST.
COPY,INPUT,JOB.
ROUTE,JOB,DC=IN,ST=NVE.
~eor
LOGIN GUEST GUEST
DISC $LOCAL
```
Note that you can't easily submit mixed case characters through a card reader, but that's OK because NOS/VE's command language is not case sensitive.

# Shutdown

It's prudent to gracefully shut down both NOS/VE and NOS before exiting DtCyber (to ensure that data on the emulated disks is consistent).  On the NOS/VE console (window TPM 0):
```
​terminate_system
```
Then follow the instructions in the Shutdown section of NOS2.8.7/README.md to cleanly shut down NOS.
# Manuals Covering NOS/VE

(This list was compiled by William Schaub.)

[NOS/VE System Usage](https://bitsavers.org/pdf/cdc/cyber/nos_ve/manuals/60464014H_NOS_VE_System_Usage_198804.pdf).  This is like the NOS Version 2 system usage and it is basically a fairly detailed run down on how to use NOS/VE.

[Migration from NOS to NOS/VE](https://bitsavers.org/pdf/cdc/cyber/nos_ve/manuals/60489503F_Migration_from_NOS_to_NOS_VE_198706.pdf).  How to use NOS/VE for people familiar with NOS, also covers how to migrate data back and forth between the dual-state barrier on a dual-state system. 

[NOS/VE Operations](https://bitsavers.org/pdf/cdc/cyber/nos_ve/manuals/60463914J_NOS_VE_Operations_198812.pdf) .  Everything about using the NOS/VE console and most frequently used operator tasks. 

[NOS/VE System Performance and Maint Vol 1](https://bitsavers.org/pdf/cdc/cyber/nos_ve/manuals/60463915J_NOS_VE_System_Performance_and_Maint_Vol_1_Performance_198912.pdf), [NOS/VE System Performance and Maint Vol 2](https://bitsavers.org/pdf/cdc/cyber/nos_ve/manuals/60463925J_NOS_VE_System_Performance_and_Maint_Vol_2_Maintenance_198912.pdf).  These are equivalent to the NOS 2 Analysis Handbook and have tuning and system maintenance information for site analysts. 

[User validation manuals and the various CYBIL manuals](https://bitsavers.org/pdf/cdc/cyber/nos_ve/manuals/ ).

Finally there's a lot of up-to-date manuals online, just type explain at the NOS/VE prompt (you need to be logged in through VEIAF using the web terminal) 