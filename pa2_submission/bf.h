/*
 * bf.h — shared protocol definitions for the PA2 node process (bfproc).
 *
 * Purpose: the single header included by every translation unit. It pins down
 * the values that must be identical across all nodes and netproc — the course
 * constants, the on-the-wire framing sizes, and the BPDU payload struct — so
 * the protocol is defined in exactly one place.
 *
 * This file declares no functions (it is data/constants only).
 */
#ifndef BF_H_
#define BF_H_

#include <stdint.h>

/* ---- Provided course constants (from the original bf.h) — do not change. ---- */
#define PORT          6789                /* TCP port netproc listens on        */
#define HELLO_TIMEOUT 2                   /* seconds between keep-alive sends    */
#define ROOT_TIMEOUT  (3 * HELLO_TIMEOUT) /* seconds a self-root advertises live */

/* ---- Wire framing (verified empirically in Step 1). ---- */
#define FRAME_LEN      128   /* every netproc frame is exactly 128 bytes        */
#define PAYLOAD_LEN    127   /* frame = [1-byte link][127-byte payload]         */
#define LINK_BROADCAST 0xFF  /* dest-link byte meaning "send to all neighbors"  */

/*
 * bf_msg — the BPDU (update message) one node advertises to its neighbors.
 *
 * Carried in bytes 1..127 of a frame, serialized in network byte order (see
 * msg.c). Two advertisements are compared lexicographically: smaller myRoot
 * wins, then smaller myCost, then smaller myID.
 *
 * Fields:
 *   myRoot  — smallest root ID the sender currently believes in.
 *   myCost  — the sender's cost to myRoot (this is the printed "distance").
 *   myID    — the sender's own node ID (lets a receiver identify the neighbor).
 *   expTime — milliseconds until this root information expires (root-crash
 *             recovery / aging; a self-root sends ROOT_TIMEOUT worth of ms).
 */
typedef struct {
	uint32_t myRoot;
	uint32_t myCost;
	uint32_t myID;
	uint32_t expTime;
} bf_msg;

#endif /* BF_H_ */
