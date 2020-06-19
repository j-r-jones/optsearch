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
# Copyright 2008-2017 Jessica Jones

include ../make.inc

ifndef $(CC)
CC = mpicc
endif

ifndef $(LD)
LD = mpicc
endif

INCLUDE += -I./ -I../src -I../lib/sqlite -I../lib/wellrng
CFLAGS += -g -fno-builtin -ftrapv -fstack-check -fstack-protector-all  $(INCLUDE)
LDFLAGS += -L./ -L../src
LDLIBS += ../lib/wellrng/WELL512a.o -lyaml ../lib/sqlite/sqlite3.o -lgsl -lgslcblas -lreadline -lpthread -ldl -lm 

.SUFFIXES: .c .h .o .so

# Note that these are in order, for linking
OBJ = 

.PHONY: all
all: runtest

test: test.o ../src/optimiser.o ../src/taskfarm.o ../src/spso.o ../src/data.o ../src/config.o ../src/random.o ../src/stats.o ../src/logging.o ../src/common.o
	cd ../src; $(MAKE)
	$(LD) $(LDFLAGS) $(LDLIBS) -o $@ $^

.PHONY: runtest
runtest: test mpi-helloworld
	rm -f *.sqlite
	cp ../src/helloworld.c .
	mpirun -np 4 ./test

.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

mpi-helloworld: mpi-helloworld.c 
	$(CC) mpi-helloworld.c -o mpi-helloworld

.PHONY: clean
clean:
	$(RM) *.o test helloworld mpi-helloworld *.sqlite

.PHONY: deepclean
deepclean: clean
	$(RM) *~
