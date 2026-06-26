/* Throwaway probe to empirically verify netproc's wire protocol (PA2 Step 1).
 *
 * NOT part of the submission — a disposable test client that connects to netproc
 * exactly like a node would, then either dumps received frames or sends one
 * marked frame, so we can observe the 128-byte framing and the byte-0 rewrite.
 *
 * Usage:
 *   probe <addr> <id> recv          connect, then print every received frame
 *   probe <addr> <id> send <link>   connect, then send one marked frame on <link>
 *                                   (<link> = 0-based link index, or 255 = 0xFF broadcast)
 */
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 6789
#define FRAME 128

/* Read exactly n bytes (Stevens readn): TCP is a stream, so loop. */
static ssize_t readn(int fd, void* buf, size_t n) {
	size_t left = n;
	char* p = buf;
	while (left > 0) {
		ssize_t r = read(fd, p, left);
		if (r < 0) return -1;
		if (r == 0) break; /* peer closed */
		left -= (size_t) r;
		p += r;
	}
	return (ssize_t) (n - left);
}

/* Write exactly n bytes (Stevens writen). */
static ssize_t writen(int fd, const void* buf, size_t n) {
	size_t left = n;
	const char* p = buf;
	while (left > 0) {
		ssize_t w = write(fd, p, left);
		if (w <= 0) return -1;
		left -= (size_t) w;
		p += w;
	}
	return (ssize_t) n;
}

int main(int argc, char* argv[]) {
	if (argc < 4) {
		fprintf(stderr,
			"usage: %s <addr> <id> recv | %s <addr> <id> send <link>\n",
			argv[0], argv[0]);
		return 2;
	}
	const char* addr = argv[1];
	uint32_t id = (uint32_t) strtoul(argv[2], NULL, 10);
	const char* mode = argv[3];

	int s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0) { perror("socket"); return 1; }

	struct sockaddr_in sa = {0};
	sa.sin_family = AF_INET;
	sa.sin_port = htons(PORT);
	if (inet_pton(AF_INET, addr, &sa.sin_addr) != 1) {
		fprintf(stderr, "bad addr %s\n", addr);
		return 1;
	}
	if (connect(s, (struct sockaddr*) &sa, sizeof sa) < 0) {
		perror("connect");
		return 1;
	}
	int one = 1;
	setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);

	/* Handshake: send our ID as a 4-byte network-order int. */
	uint32_t nid = htonl(id);
	if (writen(s, &nid, sizeof nid) != (ssize_t) sizeof nid) {
		perror("write id");
		return 1;
	}
	fprintf(stderr, "[probe %u] connected, sent id\n", id);

	if (strcmp(mode, "recv") == 0) {
		uint8_t buf[FRAME];
		for (;;) {
			ssize_t r = readn(s, buf, FRAME);
			if (r != FRAME) {
				fprintf(stderr, "[probe %u] connection closed (r=%zd)\n", id, r);
				break;
			}
			printf("[recv id=%u] byte0(link)=%u (0x%02x)  payload[0..7]="
				   "%02x %02x %02x %02x %02x %02x %02x %02x\n",
				   id, buf[0], buf[0],
				   buf[1], buf[2], buf[3], buf[4],
				   buf[5], buf[6], buf[7], buf[8]);
			fflush(stdout);
		}
	} else if (strcmp(mode, "send") == 0) {
		if (argc < 5) { fprintf(stderr, "send needs <link>\n"); return 2; }
		int link = atoi(argv[4]);
		uint8_t buf[FRAME];
		memset(buf, 0, sizeof buf);
		buf[0] = (uint8_t) link;          /* dest link, or 255 = 0xFF broadcast */
		buf[1] = 0xDE; buf[2] = 0xAD;      /* recognizable payload marker */
		buf[3] = 0xBE; buf[4] = 0xEF;
		for (int i = 5; i < FRAME; i++) buf[i] = (uint8_t) i; /* fill pattern */
		if (writen(s, buf, FRAME) != FRAME) { perror("write frame"); return 1; }
		printf("[send id=%u] wrote frame: byte0(link)=%u (0x%02x) marker=DE AD BE EF\n",
			   id, buf[0], buf[0]);
		fflush(stdout);
		sleep(1); /* let netproc deliver before we close */
	} else {
		fprintf(stderr, "unknown mode %s\n", mode);
		return 2;
	}

	close(s);
	return 0;
}
