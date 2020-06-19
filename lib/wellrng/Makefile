# This file is part of OptSearch.
#
# OptSearch is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# OptSearch is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with OptSearch.  If not, see <http://www.gnu.org/licenses/>.
#
# Copyright 2007-2018 Jessica Jones

include ../../make.inc

ifndef $(CC)
CC = gcc
endif

ifndef $(LD)
LD = gcc
endif

INCLUDE += -I./
CFLAGS += $(INCLUDE) -ftrapv

.SUFFIXES: .c .h .o

OBJ = WELL512a.o

.PHONY: all
all: $(OBJ)

.c.o:
	$(CC) $(CFLAGS) -c $<

.PHONY: clean
clean:
	$(RM) $(OBJ)

.PHONY: deepclean
deepclean: clean
	$(RM) *~
