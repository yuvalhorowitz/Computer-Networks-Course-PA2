/*
 * bfproc.c — the PA2 node process: main program, node state, and the
 * distributed Bellman-Ford logic.
 *
 * Purpose: one running bfproc represents one node in the simulated network. It
 * connects to netproc, learns its neighbors' advertisements, runs the
 * distributed Bellman-Ford computation, and reports its Root / parent / distance
 * on stdout. This file owns the node's runtime state, the relaxation + send
 * logic, the expTime aging, and the event loop; byte-level message handling
 * lives in msg.c and socket/timing helpers in net_util.c.
 *
 * Build-up by step:
 *   Step 2: connection lifecycle (connect, register ID, initial state, shutdown).
 *   Step 3: per-link neighbor table + message decode (parse & store frames).
 *   Step 4: Bellman-Ford relaxation + send rule (view-change + HELLO sends).
 *   Step 5: expTime aging, root-expiry timer, and root-crash recovery.
 */
#include "bf.h"
#include "msg.h"
#include "net_util.h"

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <unistd.h>

/*
 * link_t — what we last heard from the neighbor reached over one link.
 *   The array index equals the link number (the byte-0 value netproc stamps on
 *   arrival, and the same order as the cost arguments on the command line).
 *   `cost` is static input; the nbr* fields and expDeadline track the latest
 *   advertisement and its freshness.
 */
typedef struct {
	uint32_t cost;        /* static link cost, from argv                   */
	int      heard;       /* have we received a message on this link yet?   */
	uint32_t nbrID;       /* neighbor's ID (learned from the payload's myID) */
	uint32_t nbrRoot;     /* neighbor's last advertised myRoot               */
	uint32_t nbrCost;     /* neighbor's last advertised myCost               */
	uint32_t nbrExp;      /* neighbor's last advertised expTime (ms, debug)  */
	long     recvMs;      /* now_ms() at last receipt                        */
	long     expDeadline; /* absolute ms time this root info expires for us  */
} link_t;

/*
 * node_t — this node's current view (the BPDU it would advertise) plus its
 *   links. myRoot/myCost/parent* are the values printed as Root/distance/parent;
 *   parentLink == -1 means the node is its own root (parent=NULL).
 */
typedef struct {
	uint32_t myID;
	uint32_t myRoot;
	uint32_t myCost;
	int      parentLink; /* -1 if self-root; else index of the root port  */
	uint32_t parentID;   /* neighbor ID on the root port (for printing)    */
	int      nlinks;
	link_t*  link;       /* array[nlinks]                                  */
} node_t;

/*
 * print_state() — emit the current Root/parent/distance event line.
 *   Functionality: prints the spec's exact state format with the elapsed time;
 *     a self-root node prints the literal "parent=NULL", otherwise the parent
 *     neighbor's ID.
 *   Inputs:  node — the node whose view to print.
 *   Returns: nothing (writes one line to stdout).
 */
static void print_state(const node_t* node) {
	if (node->parentLink < 0)
		printf("time=%.01f\tRoot=%d\tparent=NULL\tdistance=%d\n",
			   now_ms() / 1e3f, (int) node->myRoot, (int) node->myCost);
	else
		printf("time=%.01f\tRoot=%d\tparent=%d\tdistance=%d\n",
			   now_ms() / 1e3f, (int) node->myRoot, (int) node->parentID,
			   (int) node->myCost);
	fflush(stdout);
}

/*
 * relax() — run one Bellman-Ford relaxation over self + all live links.
 *   Functionality: chooses the best candidate path to a root, comparing
 *     lexicographically by (root, cost, next-hop neighbor ID). Self offers
 *     (myID, 0); a link j is a candidate only if it has been heard from AND its
 *     root info is still live (now < expDeadline) — expired links are ignored,
 *     which is how a crashed root is dropped and the node falls back to itself.
 *     Updates myRoot/myCost and the parent.
 *   Inputs:  node — node state (link table read; view fields written).
 *   Returns: 1 if the view (myRoot or myCost) changed, else 0.
 */
static int relax(node_t* node) {
	long now = now_ms();
	uint32_t oldRoot = node->myRoot;
	uint32_t oldCost = node->myCost;

	uint32_t bestRoot = node->myID; /* self candidate: (myID, 0) */
	uint32_t bestCost = 0;
	uint32_t bestNbr = 0;
	int bestLink = -1;

	for (int j = 0; j < node->nlinks; j++) {
		link_t* e = &node->link[j];
		if (!e->heard) continue;
		if (now >= e->expDeadline) continue; /* root info aged out -> ignore */
		uint32_t r = e->nbrRoot;
		uint32_t c = e->nbrCost + e->cost;

		int better = 0;
		if (r < bestRoot)
			better = 1;
		else if (r == bestRoot && c < bestCost)
			better = 1;
		else if (r == bestRoot && c == bestCost && bestLink >= 0 && e->nbrID < bestNbr)
			better = 1; /* tie-break: smaller next-hop neighbor ID */

		if (better) {
			bestRoot = r;
			bestCost = c;
			bestNbr = e->nbrID;
			bestLink = j;
		}
	}

	node->myRoot = bestRoot;
	node->myCost = bestCost;
	node->parentLink = bestLink;
	node->parentID = (bestLink >= 0) ? bestNbr : 0;

	return node->myRoot != oldRoot || node->myCost != oldCost;
}

/*
 * root_expiry() — when the node's current root information will expire.
 *   Functionality: the deadline the event loop must wake for. A self-root never
 *     expires (LONG_MAX); otherwise it is the parent link's expDeadline.
 *   Inputs:  node — node state.
 *   Returns: absolute ms deadline, or LONG_MAX if self-root.
 */
static long root_expiry(const node_t* node) {
	if (node->parentLink < 0) return LONG_MAX;
	return node->link[node->parentLink].expDeadline;
}

/*
 * send_update() — broadcast this node's current BPDU to all neighbors.
 *   Functionality: builds a bf_msg from the node's view, frames it with the
 *     broadcast link byte (0xFF), writes it to netproc, prints the "Message
 *     sent" event, and resets the HELLO deadline. expTime is the freshness this
 *     node passes on: ROOT_TIMEOUT for a self-root (the "infinite" source), else
 *     the remaining time on the current root info (expDeadline - now), i.e. the
 *     received value adjusted for how long we have held it.
 *   Inputs:  node          — node whose view to advertise.
 *            fd            — connected socket to netproc.
 *            helloDeadline — out: reset to now + HELLO_TIMEOUT (ms).
 *   Returns: nothing (sends a frame, prints one line, updates *helloDeadline).
 */
static void send_update(node_t* node, int fd, long* helloDeadline) {
	bf_msg m;
	m.myRoot = node->myRoot;
	m.myCost = node->myCost;
	m.myID = node->myID;
	if (node->parentLink < 0) {
		m.expTime = (uint32_t) (ROOT_TIMEOUT * 1000);
	} else {
		long rem = node->link[node->parentLink].expDeadline - now_ms();
		if (rem < 0) rem = 0;
		m.expTime = (uint32_t) rem;
	}

	uint8_t frame[FRAME_LEN];
	build_send_frame(frame, LINK_BROADCAST, &m);
	writen(fd, frame, FRAME_LEN);

	printf("time=%.01f\tMessage sent to all neighbors\n", now_ms() / 1e3f);
	fflush(stdout);

	*helloDeadline = now_ms() + HELLO_TIMEOUT * 1000L;
}

/*
 * main() — entry point: parse arguments, run the node, shut down at lifetime.
 *   Functionality: validates argv; builds the link table from the cost
 *     arguments; ignores SIGPIPE; connects to netproc and registers the ID;
 *     prints and announces the initial self-root state; then runs a select()
 *     loop until lifetime, blocking on the nearest of {HELLO, root-expiry,
 *     lifetime}. Each iteration it stores any received frame (applying the
 *     expTime increase-only refresh), re-relaxes (catching both new info and
 *     time-based expiry), reports + re-advertises on a view change, and HELLO-
 *     keepalives. On exit it prints the shutdown line and frees resources.
 *   Inputs (argv): <netproc_addr> <node_id> <lifetime_seconds> <cost1> [cost2 ...]
 *   Outputs: the required event lines on stdout. Returns 0 on a clean run, 1 on
 *     a usage/startup error.
 */
int main(int argc, char* argv[]) {
	if (argc < 5) {
		fprintf(stderr,
			"usage: %s <netproc_addr> <node_id> <lifetime> <cost1> [cost2 ...]\n",
			argv[0]);
		return 1;
	}

	signal(SIGPIPE, SIG_IGN); /* never die from writing to a closed peer */

	const char* addr = argv[1];
	int lifetime = atoi(argv[3]); /* whole seconds, per the spec */

	node_t node;
	node.myID = (uint32_t) strtoul(argv[2], NULL, 10);
	node.nlinks = argc - 4;
	node.link = calloc((size_t) node.nlinks, sizeof *node.link);
	if (!node.link) {
		perror("calloc");
		return 1;
	}
	for (int i = 0; i < node.nlinks; i++)
		node.link[i].cost = (uint32_t) strtoul(argv[4 + i], NULL, 10);

	/* Initial belief: every node starts as its own root. */
	node.myRoot = node.myID;
	node.myCost = 0;
	node.parentLink = -1;
	node.parentID = 0;

	clock_start(); /* t0 = now; printed times are elapsed seconds since here */

	int fd = connect_to_netproc(addr, PORT);
	if (fd < 0) {
		perror("connect_to_netproc");
		free(node.link);
		return 1;
	}
	if (send_id(fd, node.myID) < 0) {
		perror("send_id");
		close(fd);
		free(node.link);
		return 1;
	}

	long lifetime_ms = (long) lifetime * 1000L;
	long helloDeadline;

	print_state(&node);                      /* initial state                 */
	send_update(&node, fd, &helloDeadline);  /* initial announcement + HELLO  */

	for (;;) {
		long now = now_ms();
		if (now >= lifetime_ms) break;

		/* Block until the nearest of {HELLO, root-expiry, lifetime}. */
		long next = helloDeadline;
		long re = root_expiry(&node);
		if (re < next) next = re;
		if (lifetime_ms < next) next = lifetime_ms;
		long remaining = next - now;
		if (remaining < 0) remaining = 0;

		fd_set rfds;
		FD_ZERO(&rfds);
		FD_SET(fd, &rfds);
		struct timeval tv;
		tv.tv_sec = remaining / 1000;
		tv.tv_usec = (remaining % 1000) * 1000;

		int n = select(fd + 1, &rfds, NULL, NULL, &tv);
		if (n < 0) {
			if (errno == EINTR) continue;
			perror("select");
			break;
		}

		if (n > 0 && FD_ISSET(fd, &rfds)) {
			uint8_t frame[FRAME_LEN];
			if (readn(fd, frame, FRAME_LEN) != FRAME_LEN) break; /* netproc closed */

			uint8_t link;
			bf_msg m;
			parse_recv_frame(frame, &link, &m);
			if (link < node.nlinks) {
				long nowm = now_ms();
				link_t* e = &node.link[link];
				long cand = nowm + (long) m.expTime; /* implied expiry */

				if (e->heard && e->nbrRoot == m.myRoot) {
					/* same root: expTime may increase only (fresher message) */
					if (cand > e->expDeadline) e->expDeadline = cand;
				} else {
					/* new root info on this link: fresh deadline */
					e->expDeadline = cand;
				}
				e->heard = 1;
				e->nbrID = m.myID;
				e->nbrRoot = m.myRoot;
				e->nbrCost = m.myCost;
				e->nbrExp = m.expTime;
				e->recvMs = nowm;
#ifdef BF_DEBUG
				fprintf(stderr,
					"[bfproc %u] recv on link %u from #%u: root=%u cost=%u exp=%u\n",
					node.myID, link, m.myID, m.myRoot, m.myCost, m.expTime);
#endif
			}
		}

		/* Re-relax every iteration: reflects newly received info AND time-based
		 * expiry of the current parent's root (root-crash recovery). */
		if (relax(&node)) {
			print_state(&node);
			send_update(&node, fd, &helloDeadline);
		}

		/* HELLO keep-alive: re-advertise at least every HELLO_TIMEOUT. */
		if (now_ms() >= helloDeadline)
			send_update(&node, fd, &helloDeadline);
	}

	printf("time=%.01f\tLifetime expired. Shutting down.\n", now_ms() / 1e3f);
	fflush(stdout);

	close(fd);
	free(node.link);
	return 0;
}
