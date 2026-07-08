/*
 * net_util.h — generic socket + timing helpers for nodeproc.
 *
 * Purpose: keep the reusable, protocol-agnostic plumbing (full-frame stream
 * I/O, connecting to netproc, the ID handshake, and an elapsed-time clock)
 * separate from the Bellman-Ford logic in nodeproc.c. Nothing here knows about
 * BPDUs — it just moves bytes and tracks time.
 *
 * Full per-function documentation lives at each definition in net_util.c.
 */
#ifndef NET_UTIL_H_
#define NET_UTIL_H_

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

/* Read exactly n bytes from a stream socket. Returns n, <n on early EOF, -1 err. */
ssize_t readn(int fd, void* buf, size_t n);

/* Write exactly n bytes to a stream socket. Returns n on success, -1 on error. */
ssize_t writen(int fd, const void* buf, size_t n);

/* Connect a TCP socket to netproc at addr:port (TCP_NODELAY set). Returns fd / -1. */
int connect_to_netproc(const char* addr, int port);

/* Send our node ID as a 4-byte network-order int. Returns 0 / -1. */
int send_id(int fd, uint32_t id);

/* clock_start() records t0; now_ms() returns ms elapsed since t0. */
void clock_start(void);
long now_ms(void);

#endif /* NET_UTIL_H_ */
