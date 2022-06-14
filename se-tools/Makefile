#--------------------------------------------------------------------------
#
#   Copyright (c) 2022, Kevin Jordan
#
#   Name: Makefile
#
#   Description:
#       Build software engineering tools used in maintaining the
#       Desktop Cyber source code repository.
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

CFLAGS  = -O3

HDRS	=   

OBJS    =   pp.o

all: pp

pp: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $+ $(LIBS)

clean:
	rm -f *.o

%.o : %.c $(HDRS)
	$(CC) $(CFLAGS) -c $<

#---------------------------  End Of File  --------------------------------
