#--------------------------------------------------------------------------
#
#   Copyright (c) 2022, Steven Zoppi
#
#   Name: MFMakefile.mak (frend2)
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

#	The LIST of the projects to be built
PROJECTS = frend2

#	When calling other makefiles, they must be named the 
#	same as this one ...
THISMAKEFILE := $(lastword $(MAKEFILE_LIST))

#	This is the list of executables upon which this build relies
EXECUTABLES = $(CC) ls 
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

# NOTE: ONLY CLONE THE PLATFORM PARAMETERS FOR EACH TARGET PLATFORM

#--------------------------------------------------------------------------
# Platform Parameters (LINUX) [\Makefile.linux32/64]
#--------------------------------------------------------------------------
#	Include Files
INCL.Linux.x86_64          := -I. -I..
INCL.Linux.i386            := -I. -I..
						   
#	Library Paths          
LDFLAGS.Linux.x86_64       := -s
LDFLAGS.Linux.i386         := -s
						   
#	Libraries              
LIBS.Linux.x86_64          := -lm -lpthread -lrt
LIBS.Linux.i386            := -lm -lpthread -lrt
						   
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
INCL.Linux.x86_64          := -I. -I..
						   
#	Library Paths          
LDFLAGS.Linux.x86_64       := -s -L/usr/lib
						   
#	Libraries              
LIBS.Linux.x86_64          := -lm -lpthread -lrt
						   
#	Compiler Flags         
CFLAGS.Linux.x86_64        := -O2 -std=gnu99 -march=armv8-a
						   
#	Optional C Flags       
EXTRACFLAGS.Linux.x86_64   := -Wno-switch -Wno-format-security

#--------------------------------------------------------------------------
# Platform Parameters (FreeBSD) [\Makefile.freedbsd32/64]
#--------------------------------------------------------------------------
#	Include Files
INCL.FreeBSD.amd64         := -I. -I..
INCL.FreeBSD.i386          := -I. -I..
						
#	Library Paths
LDFLAGS.FreeBSD.amd64      := -s
LDFLAGS.FreeBSD.i386       := -s
						
#	Libraries
LIBS.FreeBSD.amd64         := -lm -lpthread -lrt
LIBS.FreeBSD.i386          := -lm -lpthread -lrt
						
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
INCL.Darwin.x86_64         := -I. -I..
						
#	Library Paths
LDFLAGS.Darwin.x86_64      := 
						
#	Libraries
LIBS.Darwin.x86_64         := 
						
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

BINS    =   $(addprefix $(BIN)/,    \
			frend2                  \
			)

OBJS    =   $(addprefix $(OBJ)/,    \
            frend2.o                \
            frend2_helpers.o        \
            )                       \
            $(addprefix ../$(OBJ)/, \
			msufrend_util.o         \
			)

#------------------------------------------------------------------------------
#	Project Recipes
#------------------------------------------------------------------------------

frend2: $(OBJS) $(BINS)
	$(CC) $(LDFLAGS) -o $(BIN)/$@ $(OBJS) $(LIBS)

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
	$(CC) $(CFLAGS) $(EXTRACFLAGS) -c $< -o $@

#------------------------------------------------------------------------------
#	Pseudo-Targets: clean (removes binary objects)
#   				all   (rebuilds everything)
#------------------------------------------------------------------------------
$(info Default Goal is $(.DEFAULT_GOAL))

.PHONY: all
all: $(PROJECTS)

.PHONY: clean
clean:
	@echo Cleaning Object files for $@ 
	rm -f $(OBJS); 




#---------------------------  End Of File  --------------------------------