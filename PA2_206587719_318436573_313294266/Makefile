# Makefile for PA2 — builds the node process executable `nodeproc`.
# (Per the course forum, the executable must be named nodeproc.)
# -std=gnu11 exposes the POSIX/BSD socket + clock_gettime prototypes on Linux
# (grading target) and macOS without per-file feature-test macros.

CC      = cc
CFLAGS  = -Wall -Wextra -std=gnu11
OBJS    = nodeproc.o net_util.o msg.o

nodeproc: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

nodeproc.o: nodeproc.c bf.h net_util.h msg.h
	$(CC) $(CFLAGS) -c nodeproc.c

net_util.o: net_util.c net_util.h
	$(CC) $(CFLAGS) -c net_util.c

msg.o: msg.c msg.h
	$(CC) $(CFLAGS) -c msg.c

clean:
	rm -f nodeproc $(OBJS)

.PHONY: clean
