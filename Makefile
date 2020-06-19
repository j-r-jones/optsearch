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

include make.inc

.PHONY: all
all: optsearch
	#strip build/optsearch
	@echo Done

.PHONY: optsearch
optsearch: sqlite wellrng
	cd src; make
	cp src/helloworld.c build/

.PHONY: sqlite
sqlite:
	cd lib/sqlite; make

.PHONY: sqlite3
sqlite3: build/sqlite3

build/sqlite3: sqlite
	cd lib/sqlite; make sqlite3
	cp lib/sqlite/sqlite3 build/

.PHONY: wellrng
wellrng:
	cd lib/wellrng; make

.PHONY: clean
clean:
	cd src; make clean
	cd test; make clean
	rm -f build/*

.PHONY: deepclean
deepclean: clean
	$(RM) *~
	cd lib/sqlite; make clean
	cd lib/wellrng; make clean
	cd src; make deepclean

.PHONY: doc
doc:
	@doxygen

.PHONY: test
test: optsearch
	cd test; $(MAKE)

