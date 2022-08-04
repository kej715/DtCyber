#--------------------------------------------------------------------------
#
#   Copyright (c) 2022, Steven Zoppi
#
#   Name: MFMakefile.mak (automation/node_modules)
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
PROJECTS = node_modules

#	When calling other makefiles, they must be named the 
#	same as this one ...
THISMAKEFILE := $(lastword $(MAKEFILE_LIST))

#	This is the list of executables upon which this build relies
EXECUTABLES = $(CC) ls npm node
K := $(foreach exec,$(EXECUTABLES),\
        $(if \
            $(shell which $(exec)), \
            $(info "OK: '$(exec)' exists in PATH."), \
            $(error "FAIL: Required executable '$(exec)' not found in PATH.")) \
         )


#------------------------------------------------------------------------------
#	Project Recipes
#------------------------------------------------------------------------------

node_modules:
	npm install


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
	rm -rf node_modules

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
