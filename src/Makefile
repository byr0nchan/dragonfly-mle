#
# dragonfly build
#

CC = gcc
CFLAGS = -Wall -g -march=native  -D __DEBUG__
INCLUDES = -I. -I/usr/include/lua5.1/ 
LFLAGS = -L. 

LIBS = -levent -lpthread -lluajit-5.1
SRCS = main.c dragonfly-cmds.c dragonfly-io.c analyzer-threads.c suricata-cmds.c
OBJS = $(SRCS:.c=.o)

MAIN = dragonfly

.PHONY: depend clean

all:    $(MAIN)

$(MAIN): $(OBJS) 
	$(CC) $(CFLAGS) $(INCLUDES) -o $(MAIN) $(OBJS) $(LFLAGS) $(LIBS)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@ -MMD -MF $(@:.o=.d)

clean:
	$(RM) *.o *~ $(MAIN)
