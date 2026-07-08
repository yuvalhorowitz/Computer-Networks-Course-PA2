# Makefile for PA2 — builds the node process `bfproc`.
# Portable POSIX/BSD socket + clock_gettime usage: -std=gnu11 exposes the
# POSIX/BSD prototypes on both Linux (grading target) and macOS without
# per-file feature-test macros.

CC      = cc
CFLAGS  = -Wall -Wextra -std=gnu11
OBJS    = bfproc.o net_util.o msg.o

bfproc: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

bfproc.o: bfproc.c bf.h net_util.h msg.h
	$(CC) $(CFLAGS) -c bfproc.c

net_util.o: net_util.c net_util.h
	$(CC) $(CFLAGS) -c net_util.c

msg.o: msg.c msg.h bf.h
	$(CC) $(CFLAGS) -c msg.c

clean:
	rm -f bfproc $(OBJS)

.PHONY: clean
