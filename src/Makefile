# ------------------------------------------------------------------
# dragonfly build
# ------------------------------------------------------------------

CC = gcc
CFLAGS = -O2 -Wall -march=native -Werror -D__DEBUG__ -pg
INCLUDES = -I. -I./io -I./test -I./lib/pipe -I./resp -I./lib/lua-cjson -I./lib/lua-hiredis \
           -I/usr/include/lua5.1
LIBS = -levent -lpthread -lluajit-5.1 -lhiredis -lm
LFLAGS = -L.

TFLAGS =
DFLAGS =  

# -----------------------------------------------------------------
# core files
# -----------------------------------------------------------------
SRCS = main.c worker-threads.c dragonfly-io.c dragonfly-cmds.c config.c \
	   responder.c

# ------------------------------------------------------------------
# I/O modules
# ------------------------------------------------------------------
SRCS1 = io/io-file.c io/io-pipe.c io/io-tail.c

# ------------------------------------------------------------------
# Responder modules
# ------------------------------------------------------------------
SRCS2 = resp/resp-suricata.c

# ------------------------------------------------------------------
# third party open-source libraries
# ------------------------------------------------------------------
SRCS3 = lib/lua-hiredis/lua-hiredis.c lib/lua-cjson/lua-cjson.c \
        lib/lua-cjson/strbuf.c lib/lua-cjson/fpconv.c lib/pipe/pipe.c

# ------------------------------------------------------------------
# unit test files
# ------------------------------------------------------------------
SRCS4 = test/test.c test/test0.c test/test1.c test/test2.c \
		test/test3.c test/test4.c test/test5.c test/test6.c \
		test/test7.c 

OBJS = $(SRCS:.c=.o) $(SRCS1:.c=.o) $(SRCS2:.c=.o) $(SRCS3:.c=.o) $(SRCS4:.c=.o)

MAIN = dragonfly
DEBUG_BIN = dragonfly-debug
TEST_BIN = dragonfly-test

# ------------------------------------------------------------------
# production build
# ------------------------------------------------------------------
all: $(MAIN)
$(MAIN): $(OBJS) 
	$(CC) $(CFLAGS) $(INCLUDES) -o $(MAIN) $(OBJS) $(LFLAGS) $(LIBS) 

# ------------------------------------------------------------------
# test build
# ------------------------------------------------------------------
test: TEST_FLAGS =-DRUN_UNIT_TESTS -D__DEBUG__ 
test: $(TEST_BIN) 
$(TEST_BIN): $(OBJS) 
	$(CC) $(CFLAGS) $(TFLAGS) $(INCLUDES) -o $(TEST_BIN) $(OBJS) $(LFLAGS) $(LIBS)

# ------------------------------------------------------------------
# debug build
# ------------------------------------------------------------------
debug: DEBUG_FLAGS = -g -D__DEBUG__ 
debug: $(DEBUG_BIN) 
$(DEBUG_BIN): $(OBJS) 
	$(CC) $(CFLAGS) $(DFLAGS) $(INCLUDES) -o $(DEBUG_BIN) $(OBJS) $(LFLAGS) $(LIBS)

# ------------------------------------------------------------------
# misc
# ------------------------------------------------------------------
.c.o:
	$(CC) $(CFLAGS) $(TEST_FLAGS) $(INCLUDES) -c $<  -o $@

.PHONY: test clean

clean:
	$(RM) *.o test/*.o io/*.o resp/*.o lib/lua-cjson/*.o lib/lua-hiredis/*.o *~ $(MAIN) $(DEBUG_BIN) $(TEST_BIN)
