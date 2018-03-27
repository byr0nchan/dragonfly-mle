#
# dragonfly build
#

CC = gcc
CFLAGS = -O2 -Wall -g -march=native -Werror 
UNIT_TESTS = -D__DEBUG__ -D_UTEST2_
INCLUDES = -I. -I/usr/include/lua5.1/
LFLAGS = -L.

LIBS = -levent -lpthread -lluajit-5.1 -lhiredis
SRCS = main.c selftest.c lua-hiredis.c dragonfly-cmds.c dragonfly-io.c analyzer-threads.c suricata-cmds.c 
OBJS = $(SRCS:.c=.o)

MAIN = dragonfly

.PHONY: depend clean

all: $(MAIN)

$(MAIN): $(OBJS) 
	$(CC) $(CFLAGS) $(UNIT_TESTS) $(INCLUDES) -o $(MAIN) $(OBJS) $(LFLAGS) $(LIBS) 

.c.o:
	$(CC) $(CFLAGS) $(UNIT_TESTS) $(INCLUDES) -c $<  -o $@ -MMD -MF $(@:.o=.d)

clean:
	$(RM) *.o *.d *~ $(MAIN)
