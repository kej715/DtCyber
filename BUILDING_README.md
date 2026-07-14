
# Building dtCYBER on Linux.

DtCyber uses X11 to emulate the CYBER 170 display console.  Wayland is not supported.
If you're uncertain how your system is configured, enter:
```
loginctl show-session self -p Type
```
***Some people have successfully used Xwayland to work around this limitation,
but this is neither tested nor supported.***
    
1. To clone the DtCyber repo:
    ```
    sudo apt install -y git git-lfs
    git clone <dtCyber repo of your choice>
    ```  
1. Install packages needed for building and running:
    ```
    sudo apt install -y build-essential libxft-dev xterm libx11-dev x11-utils xfonts-base 
    sudo apt install -y xfonts-75dpi xfonts-100dpi xfonts-utils libfreetype6-dev
    curl -sSL https://deb.nodesource.com/setup_lts.x | sudo bash -
    sudo apt-get install -y nodejs
    ```
1. Reboot (or at least log out and back in again) so X11 discovers the new fonts.

1. Select the makefile appropriate for your platform and do:
    ```
    cd DtCyber
    make -f <makefile of your choice>  all
    ```
    Remember to make 'all' to install the NodeJS library

***See below for hints specific to Raspberry Pi.***

# Building dtCYBER on macOS.

***These instructions have only been partially tested.  In particular, they  may be missing some prerequisites.***

For recent versions of macOS (e.g. macOS 10.8 or later):

1. Install Apple's development environment XCode from the App Store.

1. Install XCode's commandline utilities:
    ```
    xcode-select --install
    ```
1. Download the DtCyber source repository.
    ```
    git clone <dtCyber repo of your choice>
    ```
1. Install [Homebrew](https://brew.sh) or your preferred package manager:
    ```
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    ```
1. Install [XQuartz](https://www.xquartz.org/) and X11 development tools via Homebrew:
    ```
    brew install --cask xquartz
    brew install freetype pkg-config
    ```
1. Install the current LTS version of Node.js and npm, and link it 
so it becomes your default 'node' command
    ```
    brew install node@20
    brew link --overwrite node@20
    ```
1. Build DtCyber:
    ```
    make -f Makefile.macosx all
    ```

# Building dtCYBER on Raspberry Pi OS

## Background
Based on starting with clean default image of 32 or 64 bit Raspberry Pi OS with desktop.
    
    No benefit seen with 64-bit OS version.

    Tested with Pi OS Bullseye 32 bit, released May 3rd 2023.
    Tested with Kevin Jordan's repo at https://github.com/kej715/DtCyber
    Tested on: Pi model 3B, 3B+, 4B, Zero 2 W
        The Pi Zero 2 W runs dtCyber well and is good value at US $15
        See notes below if you want to run on older ARM7 or ARM 6 models of the Pi
    Install tested by Digby R.S. Tarvin on:
        RaspberryPi 5 aarch64 (Debian GNU/Linux 12 (bookworm))
        RaspberryPi 5 aarch64 (Ubuntu 24.04.4 LTS)
        nvidia AGX Orin aarch64 (Ubuntu 20.04.6 LTS)
        Advantech MIC-770V2 x86_64 (Ubuntu 24.04.4 LTS)

## Suggested Steps

Update, upgrade and reboot:
```
sudo apt-get update -y
sudo apt dist-upgrade -y
sudo reboot
```
Install and test any display drivers or VNC server now.

Increase swap size (only strictly necessary for 512K and 1GB memory, but just do it).
```
sudo dphys-swapfile swapoff
sudo nano /etc/dphys-swapfile
    Edit CONF_SWAPSIZE=100 -> CONF_SWAPSIZE=1024
sudo dphys-swapfile setup
sudo dphys-swapfile swapon
```

Install Node 16.x:
```
curl -sSL https://deb.nodesource.com/setup_16.x | sudo bash -
sudo apt-get install -y nodejs
node -v
```
Continue with normal Linux setup ... E.g. Git clone repo and switch to dtcyber directory and build.  

The generic makefile for 64-bit Linux platforms, "Makefile.linux64", is sufficient for Raspberry Pi 
platforms. Using "Makefile.linux64-armv8-a", however, provides extra information to the gcc compiler. 
It informs the compiler of the specific target platform and enables it to generate code that 
leverages CPU instructions and, possibly other features, provided by that specific platform.

Notes:
1) On most Pi you will need to edit the 'cyber.ini' file for the appropriate
        OS version you are starting to slow down the key entry to allow the date and time to
        be set correctly. For example on emulated NOS2.8.7 on Pi 2 Zero W the following revised
        values may work better (some further tweaking needed):
                set_operator_port 6662 (unchanged)
                set_key_wait_interval 500
                enter_keys #4000#
                enter_keys #25000#%year%%mon%%day%
                enter_keys #3000#%hour%%min%%sec%

## Notes for Older Models of Raspberry Pi

FYI, DtCyber will run on the original Pi Zero but is very slow to both compile and run, and
the console flickers. However, if you want to try ...

ARM6: Raspberry Pi 1 Model A, A+, B, B+ and Zero (Single-core ARM1176JZF-S)

1. Install unofficial build of NodeJS for Arm6. E.g.
    ```wget https://unofficial-builds.nodejs.org/download/release/v16.20.0/node-v16.20.0-linux-armv6l.tar.xz
    tar xvfJ node-v16.20.0-linux-armv6l.tar.xz
    sudo cp -R node-v16.20.0-linux-armv6l/* /usr/local
    node -v
    ```
1. Set the cflags on the Makefile to do best build for this CPU (this works well on almost all Linux machines)
    ```
    CFLAGS  = -O3 -I. $(INCL) $(EXTRACFLAGS) -std=gnu99 -march=native -mcpu=native -mtune=native
    ```

Arm 7: Raspberry Pi 2 Model B (Quad-core ARM Cortex-A7)

Provided you have installed the ARM7 version of NodeJS you need only change the CFLAGS as above.

Note, however, if you follow the steps for ARM6 build above with the CFLAGS change you can swap the SD
card between any Pi and just 'make ... clean', 'make ... all' to rebuild the code for the installed CPU.
