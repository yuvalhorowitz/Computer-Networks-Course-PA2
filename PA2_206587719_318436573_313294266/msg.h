#ifndef MSG_H_
#define MSG_H_

#include <stdint.h>

/* Wire framing and the BPDU payload layout.
 *
 * These live here (not in bf.h) because bf.h must contain only the provided
 * course constants and may be replaced by the graders' copy — so our protocol
 * definitions have to sit in our own header.
 *
 * Frame layout (128 bytes): [ byte 0 = link ][ bytes 1..127 = payload ].
 * On send, byte 0 is the destination link (0-based) or LINK_BROADCAST (0xFF).
 * On receive, byte 0 is *our* ingress link (netproc rewrites it).
 *
 * Payload layout (127 bytes, network byte order, rest zero-padded):
 *   bytes  0..3   myRoot
 *   bytes  4..7   myCost
 *   bytes  8..11  myID
 *   bytes 12..15  expTime (milliseconds)
 *   bytes 16..126 zero padding
 */
#define FRAME_LEN      128   /* every netproc frame is exactly 128 bytes        */
#define PAYLOAD_LEN    127   /* frame = [1-byte link][127-byte payload]         */
#define LINK_BROADCAST 0xFF  /* dest-link byte meaning "send to all neighbors"  */

/* The BPDU (update message) a node advertises. Compared lexicographically:
 * smaller myRoot, then myCost, then myID. */
typedef struct {
	uint32_t myRoot;  /* smallest root ID known to the sender               */
	uint32_t myCost;  /* sender's cost to myRoot (this is printed distance)  */
	uint32_t myID;    /* sender's own ID                                     */
	uint32_t expTime; /* ms until this root info expires (root-crash recovery)*/
} bf_msg;

/* Encode/decode just the 127-byte payload (no link byte). */
void msg_encode(const bf_msg* m, uint8_t payload[PAYLOAD_LEN]);
void msg_decode(const uint8_t payload[PAYLOAD_LEN], bf_msg* m);

/* Build/parse a full 128-byte frame (link byte + payload). */
void build_send_frame(uint8_t frame[FRAME_LEN], uint8_t link, const bf_msg* m);
void parse_recv_frame(const uint8_t frame[FRAME_LEN], uint8_t* link, bf_msg* m);

#endif /* MSG_H_ */
