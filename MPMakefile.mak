#--------------------------------------------------------------------------
#
#   Copyright (c) 2022, Steven Zoppi
#
#   Name: MFMakefile.mak (dtcyber - MAIN project)
#
#   Description:
#       Build ALL versions of Desktop Cyber emulation.
#
#   This program is free software: you can redistribute it and/or modify
#   it under the terms of the GNU General Public License version 3 as
#   published by the Free Software Foundation.
#   
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License version 3 for more details.
#   
#   You should have received a copy of the GNU General Public License
#   version 3 along with this program in file "license-gpl-3.0.txt".
#   If not, see <http://www.gnu.org/licenses/gpl-3.0.txt>.
#
#--------------------------------------------------------------------------

  ####   #    #  #    #          #    #    ##    #    #  ######
 #    #  ##   #  #    #          ##  ##   #  #   #   #   #
 #       # #  #  #    #          # ## #  #    #  ####    #####
 #  ###  #  # #  #    #          #    #  ######  #  #    #
 #    #  #   ##  #    #          #    #  #    #  #   #   #
  ####   #    #   ####           #    #  #    #  #    #  ######

#	The LIST of the projects to be built
PROJECTS = dtcyber automation/node_modules stk/node_modules 

#	When calling other makefiles, they must be named the 
#	same as this one ...
THISMAKEFILE := $(lastword $(MAKEFILE_LIST))

#	This is the list of executables upon which this build relies
EXECUTABLES = $(CC) ls expect
K := $(foreach exec,$(EXECUTABLES),\
        $(if \
		  $(shell which $(exec)), \
		  $(info "OK: '$(exec)' exists in PATH."), \
		  $(error "FAIL: Required executable '$(exec)' not found in PATH.")) \
		  )

#--------------------------------------------------------------------------
# Platform Detection (will also work for MINGW)
#--------------------------------------------------------------------------

uname_s := $(shell uname -s)
$(info uname_s=$(uname_s))
uname_m := $(shell uname -m)
$(info uname_m=$(uname_m))

#--------------------------------------------------------------------------
# Compiler options
#--------------------------------------------------------------------------
# NOTE: ONLY CLONE THE PLATFORM PARAMETERS FOR EACH TARGET PLATFORM

#--------------------------------------------------------------------------
# Platform Parameters (LINUX) [\Makefile.linux32/64]
#--------------------------------------------------------------------------
#	Include Files
INCL.Linux.x86_64          := -I. -I/usr/X11R6/include 
INCL.Linux.i386            := -I. -I/usr/X11R6/include 
						   
#	Library Paths          
LDFLAGS.Linux.x86_64       := -s -L/usr/X11R6/lib64
LDFLAGS.Linux.i386         := -s -L/usr/X11R6/lib
						   
#	Libraries              
LIBS.Linux.x86_64          := -lm -lX11 -lpthread -lrt
LIBS.Linux.i386            := -lm -lX11 -lpthread -lrt
						   
#	Compiler Flags         
CFLAGS.Linux.x86_64        := -O2 -std=gnu99
CFLAGS.Linux.i386          := -O2 -std=gnu99
						   
#	Optional C Flags       
EXTRACFLAGS.Linux.x86_64   := -Wno-switch -Wno-format-security
EXTRACFLAGS.Linux.i386     := -Wno-switch -Wno-format-security

#--------------------------------------------------------------------------
# Platform Parameters (LINUX/aarch64) [\Makefile.linux64-armv8-a]
#--------------------------------------------------------------------------
#	Include Files
INCL.Linux.aarch64          := -I. -I/usr/include/X11 
						   
#	Library Paths          
LDFLAGS.Linux.aarch64       := -s -L/usr/lib
						   
#	Libraries              
LIBS.Linux.aarch64          := -lm -lX11 -lpthread -lrt
						   
#	Compiler Flags         
CFLAGS.Linux.aarch64        := -O2 -std=gnu99 -march=armv8-a
						   
#	Optional C Flags       
EXTRACFLAGS.Linux.aarch64   := -Wno-switch -Wno-format-security

#--------------------------------------------------------------------------
# Platform Parameters (FreeBSD) [\Makefile.freedbsd32/64]
#--------------------------------------------------------------------------
#	Include Files
INCL.FreeBSD.amd64         := -I. -I/usr/local/include 
INCL.FreeBSD.i386          := -I. -I/usr/local/include 
						
#	Library Paths
LDFLAGS.FreeBSD.amd64      := -s -L/usr/local/lib
LDFLAGS.FreeBSD.i386       := -s -L/usr/local/lib
						
#	Libraries
LIBS.FreeBSD.amd64         := -lm -lX11 -lpthread -lrt
LIBS.FreeBSD.i386          := -lm -lX11 -lpthread -lrt
						
#	Compiler Flags
CFLAGS.FreeBSD.amd64       := -O2 -std=gnu99
CFLAGS.FreeBSD.i386        := -O2 -std=gnu99

#	Optional C Flags
EXTRACFLAGS.FreeBSD.amd64  := -Wno-switch -Wno-format-security
EXTRACFLAGS.FreeBSD.i386   := -Wno-switch -Wno-format-security

#--------------------------------------------------------------------------
# Platform Parameters (MacOSX) [\Makefile.macosx]
#--------------------------------------------------------------------------
#	Include Files
INCL.Darwin.x86_64         := -I/opt/X11/include
						
#	Library Paths
LDFLAGS.Darwin.x86_64      := -L/opt/X11/lib
						
#	Libraries
LIBS.Darwin.x86_64         := -lX11
						
#	Compiler Flags
CFLAGS.Darwin.x86_64       := -O3    -std=gnu99
#CFLAGS                     := -O0 -g -std=gnu99 

#	Optional C Flags
EXTRACFLAGS.Darwin.x86_64  := -Wno-switch -Wno-format-security

#	SDK Directory
SDKDIR.Darwin.x86_64       := /Developer/SDKs/MacOSX10.4u.sdk


#--------------------------------------------------------------------------
# Compiler options for THE CURRENT platform
#--------------------------------------------------------------------------


INCL         += $(INCL.$(uname_s).$(uname_m))
LDFLAGS      += $(LDFLAGS.$(uname_s).$(uname_m))
LIBS         += $(LIBS.$(uname_s).$(uname_m))
CFLAGS       += $(CFLAGS.$(uname_s).$(uname_m))
EXTRACFLAGS  += $(EXTRACFLAGS.$(uname_s).$(uname_m))
SDKDIR	     += $(SDKDIR.$(uname_s).$(uname_m))

$(info INCL=$(INCL))
$(info LDFLAGS=$(LDFLAGS))
$(info LIBS=$(LIBS))
$(info CFLAGS=$(CFLAGS))
$(info EXTRACFLAGS=$(EXTRACFLAGS))
$(info SDKDIR=$(SDKDIR))


#--------------------------------------------------------------------------
# Directories
#--------------------------------------------------------------------------
#	Sources are assumed to be rooted at the 
#	same directory as the this makefile.
BIN=bin
OBJ=obj

#--------------------------------------------------------------------------
# Sources and Targets
#--------------------------------------------------------------------------


HDRS    =   const.h                 \
            cyber_channel_linux.h   \
            npu.h                   \
            proto.h                 \
            types.h

BINS    =   $(addprefix $(BIN)/,    \
			dtcyber                 \
			)

OBJS    =   $(addprefix $(OBJ)/,    \
            cdcnet.o                \
            channel.o               \
            charset.o               \
            console.o               \
            cp3446.o                \
            cpu.o                   \
            cr3447.o                \
            cr405.o                 \
            cray_station.o          \
            dcc6681.o               \
            dd6603.o                \
            dd885-42.o              \
            dd8xx.o                 \
            ddp.o                   \
            deadstart.o             \
            device.o                \
            dsa311.o                \
            dump.o                  \
            float.o                 \
            fsmon.o                 \
            init.o                  \
            interlock_channel.o     \
            log.o                   \
            lp1612.o                \
            lp3000.o                \
            main.o                  \
            maintenance_channel.o   \
            msufrend.o              \
            mdi.o                   \
            mt362x.o                \
            mt5744.o                \
            mt607.o                 \
            mt669.o                 \
            mt679.o                 \
            mux6676.o               \
            niu.o                   \
            npu_async.o             \
            npu_bip.o               \
            npu_hasp.o              \
            npu_hip.o               \
            npu_lip.o               \
            npu_net.o               \
            npu_nje.o               \
            npu_svm.o               \
            npu_tip.o               \
            operator.o              \
            pci_channel_linux.o     \
            pci_console_linux.o     \
            pp.o                    \
            rtc.o                   \
            scr_channel.o           \
            shift.o                 \
            time.o                  \
            tpmux.o                 \
            trace.o                 \
            window_x11.o            \
			)


#------------------------------------------------------------------------------
#	Project Recipes
#------------------------------------------------------------------------------

dtcyber: $(OBJS) $(BINS)
	$(CC) $(LDFLAGS) -o $(BIN)/$@ $(OBJS) $(LIBS)

automation/node_modules:
	$(MAKE) -f $(THISMAKEFILE) -C automation

stk/node_modules:
	$(MAKE) -f $(THISMAKEFILE) -C stk


#------------------------------------------------------------------------------
#	Recipe to Ensure that the subdirectories exist
#------------------------------------------------------------------------------

$(OBJS): | $(OBJ)
$(BINS): | $(BIN)

$(OBJ):
	mkdir $(OBJ)

$(BIN):
	mkdir $(BIN)

#------------------------------------------------------------------------------
#	Recipe to generate Object modules
#------------------------------------------------------------------------------

$(OBJ)/%.o : %.c $(HDRS)
	$(CC) $(INCL) $(CFLAGS) $(EXTRACFLAGS) -c $< -o $@


#------------------------------------------------------------------------------
#	Pseudo-Targets: clean (removes binary objects)
#   				all   (rebuilds everything)
#------------------------------------------------------------------------------
$(info Default Goal is $(.DEFAULT_GOAL))

.PHONY: all
all: $(PROJECTS)
	@echo $< Complete

.PHONY: clean
clean:
	@echo Cleaning Object files for $@ 
	rm -f $(OBJS); 
	$(MAKE) -C automation clean; 
	$(MAKE) -C stk clean

.PHONY: info
info:
	@echo #--------------------------------------------------------------------
	@echo The following Features are 
	@echo supported by this version of make:
	@echo $(.FEATURES)
	@echo #--------------------------------------------------------------------
	@echo Makefile Name $(THISMAKEFILE)
	@echo #--------------------------------------------------------------------
	@echo Objects       $(OBJS)
	@echo #--------------------------------------------------------------------
	@echo Objects       $(VARIABLES)


#---------------------------  End Of File  --------------------------------
