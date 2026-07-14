[NOS/VE](https://en.wikipedia.org/wiki/NOS/VE) is the operating system and related software products for [CDC's Cyber 180 architecture](https://en.wikipedia.org/wiki/CDC_Cyber#Cyber_180_series).  DtCyber supports running NOS/VE in dual-state mode, in which NOS and NOS/VE share the same machine, mediated by a hypervisor.

# Installing and Running

First, build DtCyber with the instructions in BUILDING.README.

NOS/VE 1.8.3  is included in the NOS 2.8.7 image.  Download and install it with:

​	cd NOS2.8.7
​        node install rtr dual-state-860

This script will immediately deadstart NOS and NOS/VE after the install.  To subsequently bypass the install step and immediately dead start the machine:

​	node start

This may have to be done under sudo on some systems.

README.md in the NOS2.8.7 directory describes the NOS installation in detail.

# Shutdown

It's prudent to gracefully shut down both NOS/VE and NOS before exiting DtCyber (to ensure that data on the emulated disks is consistent).  On the NOS/VE console:

​	terminate_system

On the DtCyber operator console:

​	shutdown

# Manuals Covering NOS/VE

(This list was compiled by William Schaub.)

[NOS/VE System Usage](https://bitsavers.org/pdf/cdc/cyber/nos_ve/manuals/60464014H_NOS_VE_System_Usage_198804.pdf).  This is like the NOS Version 2 system usage and it is basically a fairly detailed run down on how to use NOS/VE.

[Migration from NOS to NOS/VE](https://bitsavers.org/pdf/cdc/cyber/nos_ve/manuals/60489503F_Migration_from_NOS_to_NOS_VE_198706.pdf).  How to use NOS/VE for people familiar with NOS, also covers how to migrate data back and forth between the dual-state barrier on a dual-state system. 

[NOS/VE Operations](https://bitsavers.org/pdf/cdc/cyber/nos_ve/manuals/60463914J_NOS_VE_Operations_198812.pdf) .  Everything about using the NOS/VE console and most frequently used operator tasks. 

[NOS/VE System Performance and Maint Vol 1](https://bitsavers.org/pdf/cdc/cyber/nos_ve/manuals/60463915J_NOS_VE_System_Performance_and_Maint_Vol_1_Performance_198912.pdf), [NOS/VE System Performance and Maint Vol 2](https://bitsavers.org/pdf/cdc/cyber/nos_ve/manuals/60463925J_NOS_VE_System_Performance_and_Maint_Vol_2_Maintenance_198912.pdf).  These are equivalent to the NOS 2 Analysis Handbook and have tuning and system maintenance information for site analysts. 

[User validation manuals and the various CYBIL manuals](https://bitsavers.org/pdf/cdc/cyber/nos_ve/manuals/ ).

Finally there's a lot of up-to-date manuals online, just type explain at the NOS/VE prompt (you need to be logged in through VEIAF using the web terminal) 