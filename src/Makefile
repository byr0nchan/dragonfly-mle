# ------------------------------------------------------------------
# dragonfly build
# ------------------------------------------------------------------

# For FreeBSD
#CC = cc
#CFLAGS = -O2 -Wall -march=native -Werror -Wno-error=tautological-pointer-compare 

CC = gcc
CFLAGS = -O2 -g -Wall -march=native -Werror

INCLUDES = -I. -I/usr/local/include -I./io -I./test -I./lib/msgqueue -I./resp \
	   -I./lib/lua-cjson -I./lib/lua-hiredis -I./lib/lua-cmsgpack

LIBS = -lpthread -lluajit-5.1 -lhiredis -lm -lcurl -lrt -lz
LFLAGS = -L. -L/usr/local/lib

TFLAGS =
DFLAGS =  

RM=/bin/rm
DRAGONFLY_ROOT=/usr/local/dragonfly-mle

# -----------------------------------------------------------------
# core files
# -----------------------------------------------------------------
SRCS = main.c worker-threads.c dragonfly-io.c dragonfly-cmds.c config.c \
	   responder.c

# ------------------------------------------------------------------
# I/O modules
# ------------------------------------------------------------------
SRCS1 = io/io-file.c io/io-pipe.c io/io-tail.c io/io-kafka.c \
        io/io-syslog.c io/io-zfile.c

# ------------------------------------------------------------------
# Responder modules
# ------------------------------------------------------------------
SRCS2 = resp/resp-suricata.c

# ------------------------------------------------------------------
# third party open-source libraries
# ------------------------------------------------------------------
SRCS3 = lib/lua-hiredis/lua-hiredis.c lib/lua-cjson/lua-cjson.c \
        lib/lua-cjson/strbuf.c lib/lua-cjson/fpconv.c \
        lib/msgqueue/msgqueue.c lib/lua-cmsgpack/lua_cmsgpack.c

# ------------------------------------------------------------------
# unit test files
# ------------------------------------------------------------------
SRCS4 = test/test.c test/test0.c test/test1.c test/test2.c \
		test/test3.c test/test4.c test/test5.c test/test6.c \
		test/test7.c test/test8.c test/test9.c

OBJS = $(SRCS:.c=.o) $(SRCS1:.c=.o) $(SRCS2:.c=.o) $(SRCS3:.c=.o) $(SRCS4:.c=.o)

MAIN = dragonfly-mle
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

test: TFLAGS =-DRUN_UNIT_TESTS -D__DEBUG__
test: $(TEST_BIN) 
$(TEST_BIN): $(OBJS) 
	$(CC) $(CFLAGS) $(TFLAGS) $(INCLUDES) -o $(TEST_BIN) $(OBJS) $(LFLAGS) $(LIBS)
#	valgrind --leak-check=yes ./$(TEST_BIN) -r /tmp

# ------------------------------------------------------------------
# install
# ------------------------------------------------------------------

install:  
	@echo installing files
	install -d $(DRAGONFLY_ROOT)/bin/ 
	install -d $(DRAGONFLY_ROOT)/config/
	install -d $(DRAGONFLY_ROOT)/filter/
	install -d $(DRAGONFLY_ROOT)/analyzer/
	install ./dragonfly-mle $(DRAGONFLY_ROOT)/bin/
	install ./analyzer/* $(DRAGONFLY_ROOT)/analyzer/
	install ./filter/* $(DRAGONFLY_ROOT)/filter/
	install ./config/* $(DRAGONFLY_ROOT)/config/

# ------------------------------------------------------------------
# misc
# ------------------------------------------------------------------
.c.o:
	$(CC) $(CFLAGS) $(TFLAGS) $(INCLUDES) -c $<  -o $@

.PHONY: test clean

clean:
	$(RM) -f $(OBJS) $(MAIN) $(DEBUG_BIN) $(TEST_BIN) 
