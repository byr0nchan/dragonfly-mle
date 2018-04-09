#
# dragonfly build
#

CC = gcc
CFLAGS = -O2 -Wall -march=native -Werror -D__DEBUG__ 
# 
#UNIT_TESTS = -DRUN_UNIT_TESTS -D_UTEST2_
INCLUDES = -I. -I/usr/include/lua5.1/
LFLAGS = -L.

LIBS = -levent -lpthread -lluajit-5.1 -lhiredis -lm
SRCS = main.c selftest.c lua-hiredis.c dragonfly-cmds.c analyzer-threads.c suricata-cmds.c \
       lua-cjson.c strbuf.c fpconv.c io-file.c io-pipe.c dragonfly-io.c
OBJS = $(SRCS:.c=.o)

MAIN = dragonfly

.PHONY: depend clean

all: $(MAIN)

$(MAIN): $(OBJS) 
	$(CC) $(CFLAGS) $(UNIT_TESTS) $(INCLUDES) -o $(MAIN) $(OBJS) $(LFLAGS) $(LIBS) 

.c.o:
	$(CC) $(CFLAGS) $(UNIT_TESTS) $(INCLUDES) -c $<  -o $@

clean:
	$(RM) *.o *~ $(MAIN)
