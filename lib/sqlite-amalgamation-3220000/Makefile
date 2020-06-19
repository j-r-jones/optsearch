# See: https://sqlite.org/howtocompile.html
#
# Which basically gave me:
#
# gcc -DSQLITE_ENABLE_FTS4 -DSQLITE_ENABLE_FTS5 -DSQLITE_ENABLE_JSON1 \
#  -DSQLITE_ENABLE_RTREE -DSQLITE_ENABLE_EXPLAIN_COMMENTS -DHAVE_USLEEP \
#  -DHAVE_READLINE shell.c sqlite3.c -lreadline -lpthread -ldl -lm -I. \
#  -o  sqlite3-shell
#
# Though they'd forgotten -lm

CC = gcc
LD = gcc

CFLAGS = -O2 -DYYSTACKDEPTH=4096 -DSQLITE_ENABLE_FTS4 -DSQLITE_ENABLE_FTS5 -DSQLITE_ENABLE_JSON1 -DSQLITE_ENABLE_RTREE -DSQLITE_ENABLE_EXPLAIN_COMMENTS -DHAVE_USLEEP -DHAVE_READLINE -I./
LDFLAGS = -lreadline -lpthread -ldl -lm
OBJ = sqlite3.o

.SUFFIXES: .c .h .o .so

all: $(OBJ)

.PHONY: shell
shell: sqlite3

sqlite3: shell.c sqlite3.c
	$(CC) -Os -I. -DSQLITE_THREADSAFE=0 -DSQLITE_ENABLE_FTS4 \
		  -DSQLITE_ENABLE_FTS5 -DSQLITE_ENABLE_JSON1 \
		  -DSQLITE_ENABLE_RTREE -DSQLITE_ENABLE_EXPLAIN_COMMENTS \
		  -DHAVE_USLEEP -DHAVE_READLINE \
			shell.c sqlite3.c -ldl -lreadline -lncurses -lm -o sqlite3

.c.o:
	$(CC) $(CFLAGS) -c $<

.PHONY: clean
clean:
	$(RM) $(OBJ) sqlite3

.PHONY: deepclean
deepclean: clean
	$(RM) *~

