/*
 * net_util.c — implementation of the socket and timing helpers.
 *
 * Purpose: robust, reusable plumbing used by bfproc — exact-length stream I/O
 * (TCP has no message boundaries), connection setup with the socket options the
 * assignment requires (TCP_NODELAY, and MSG_WAITALL/MSG_NOSIGNAL on the data
 * calls), the ID handshake, and an elapsed-milliseconds clock for event times
 * and timers.
 */
#include "net_util.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

/* macOS lacks MSG_NOSIGNAL; we additionally ignore SIGPIPE in main() so a
 * closed peer never kills the node. On Linux MSG_NOSIGNAL does the same. */
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

/*
 * readn() — read exactly n bytes from a stream socket.
 *   Functionality: TCP delivers a byte stream with no message boundaries, so a
 *     single recv() may return fewer bytes than requested. This loops with
 *     MSG_WAITALL until n bytes are read or the peer closes / an error occurs.
 *   Inputs:  fd  — connected stream socket.
 *            buf — destination buffer (>= n bytes).
 *            n   — number of bytes to read.
 *   Returns: n on success; a value < n if the peer closed early (EOF); -1 on a
 *            socket error.
 */
ssize_t readn(int fd, void* buf, size_t n) {
	size_t left = n;
	char* p = buf;
	while (left > 0) {
		ssize_t r = recv(fd, p, left, MSG_WAITALL);
		if (r < 0) return -1;
		if (r == 0) break; /* peer closed connection */
		left -= (size_t) r;
		p += r;
	}
	return (ssize_t) (n - left);
}

/*
 * writen() — write exactly n bytes to a stream socket.
 *   Functionality: loops over partial sends until all n bytes are written.
 *     Uses MSG_NOSIGNAL so that writing to a peer that has closed returns an
 *     error instead of raising SIGPIPE (which would terminate the process).
 *   Inputs:  fd  — connected stream socket.
 *            buf — source buffer (>= n bytes).
 *            n   — number of bytes to write.
 *   Returns: n on success; -1 on error (including the peer having gone away).
 */
ssize_t writen(int fd, const void* buf, size_t n) {
	size_t left = n;
	const char* p = buf;
	while (left > 0) {
		ssize_t w = send(fd, p, left, MSG_NOSIGNAL);
		if (w <= 0) return -1;
		left -= (size_t) w;
		p += w;
	}
	return (ssize_t) n;
}

/*
 * connect_to_netproc() — open a TCP connection to the netproc emulator.
 *   Functionality: creates an IPv4 stream socket, enables TCP_NODELAY (the
 *     assignment requires small messages not be delayed by Nagle), parses the
 *     dotted-quad address, and connects to addr:port.
 *   Inputs:  addr — netproc's IPv4 address as a string (e.g. "127.0.0.1").
 *            port — TCP port (PORT, i.e. 6789).
 *   Returns: the connected socket fd on success; -1 on any failure (the socket
 *            is closed on failure so no fd leaks).
 */
int connect_to_netproc(const char* addr, int port) {
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) return -1;

	int one = 1;
	setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);

	struct sockaddr_in sa;
	memset(&sa, 0, sizeof sa);
	sa.sin_family = AF_INET;
	sa.sin_port = htons((uint16_t) port);
	if (inet_pton(AF_INET, addr, &sa.sin_addr) != 1) {
		close(fd);
		return -1;
	}
	if (connect(fd, (struct sockaddr*) &sa, sizeof sa) < 0) {
		close(fd);
		return -1;
	}
	return fd;
}

/*
 * send_id() — perform the netproc registration handshake.
 *   Functionality: netproc expects each node, immediately after connecting, to
 *     send its ID as a 4-byte integer in network byte order; it uses this to
 *     bind the socket to the right node in the topology.
 *   Inputs:  fd — connected socket to netproc.
 *            id — this node's ID.
 *   Returns: 0 on success; -1 if the 4 bytes could not be fully written.
 */
int send_id(int fd, uint32_t id) {
	uint32_t nid = htonl(id);
	return writen(fd, &nid, sizeof nid) == (ssize_t) sizeof nid ? 0 : -1;
}

/* t0 captured by clock_start(); read by now_ms(). File-local state. */
static struct timespec t0_;

/*
 * clock_start() — record the program's time origin.
 *   Functionality: stores the current CLOCK_REALTIME as t0, so all later event
 *     times are reported relative to startup.
 *   Inputs:  none.
 *   Returns: nothing (sets the file-local t0_).
 */
void clock_start(void) {
	clock_gettime(CLOCK_REALTIME, &t0_);
}

/*
 * now_ms() — milliseconds elapsed since clock_start().
 *   Functionality: reads CLOCK_REALTIME and returns the difference from t0 in
 *     milliseconds. Used both for the printed "time=" field and for computing
 *     timer deadlines.
 *   Inputs:  none.
 *   Returns: elapsed time since t0 in milliseconds.
 */
long now_ms(void) {
	struct timespec t;
	clock_gettime(CLOCK_REALTIME, &t);
	return (t.tv_sec - t0_.tv_sec) * 1000L + (t.tv_nsec - t0_.tv_nsec) / 1000000L;
}
