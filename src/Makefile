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

include ../make.inc

ifndef $(CC)
CC = mpicc
endif

ifndef $(LD)
LD = mpicc
endif

INCLUDE += -I./ -I../lib/sqlite -I../lib/wellrng
# Note that these are GCC specific
CFLAGS += -fno-builtin -ftrapv -fstack-check -fstack-protector-all $(INCLUDE)
LDFLAGS += -L./
# The readline and ncurses libraries are for features of SQLite that we are not using in OptSearch, but were using during debugging
#LDLIBS += ../lib/wellrng/WELL512a.o ../lib/sqlite/sqlite3.o -lyaml -lgsl -lgslcblas -lreadline -lncurses -lpthread -ldl -lm
LDLIBS += ../lib/wellrng/WELL512a.o ../lib/sqlite/sqlite3.o -lyaml -lgsl -lgslcblas -lpthread -ldl -lm

.SUFFIXES: .c .h .o .so

# Note that these are in order, for linking
OBJ = optsearch.o optimiser.o data.o spso.o taskfarm.o config.o stats.o random.o logging.o common.o

.PHONY: all
all: optsearch

.PHONY: optsearch
optsearch: ../build/optsearch $(OBJ)

../build/optsearch:   $(OBJ)
	mkdir -p ../build
	$(LD) $(LDFLAGS) -o $@ $^ $(LDLIBS)

.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

.PHONY: clean
clean:
	$(RM) *.o ../build/optsearch

.PHONY: deepclean
deepclean: clean
	$(RM) *~
