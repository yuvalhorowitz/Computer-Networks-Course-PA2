/*
 * msg.c — implementation of BPDU/frame serialization.
 *
 * Purpose: convert between the bf_msg struct and the fixed byte layout described
 * in msg.h. All multi-byte fields use network byte order and are accessed via
 * memcpy, so the code is correct regardless of host endianness or struct
 * padding/alignment.
 */
#include "msg.h"

#include <arpa/inet.h>
#include <string.h>

/*
 * put32() — store a uint32 in network byte order at a byte pointer.
 *   Inputs:  p — destination (>= 4 bytes); v — host-order value.
 *   Returns: nothing (writes 4 bytes at p).
 */
static void put32(uint8_t* p, uint32_t v) {
	uint32_t n = htonl(v);
	memcpy(p, &n, sizeof n);
}

/*
 * get32() — read a network-byte-order uint32 from a byte pointer.
 *   Inputs:  p — source (>= 4 bytes).
 *   Returns: the value converted to host byte order.
 */
static uint32_t get32(const uint8_t* p) {
	uint32_t n;
	memcpy(&n, p, sizeof n);
	return ntohl(n);
}

/*
 * msg_encode() — serialize a bf_msg into a 127-byte payload.
 *   Functionality: zeroes the buffer (so padding bytes 16..126 are 0), then
 *     writes the four fields at their fixed offsets in network byte order.
 *   Inputs:  m       — message to serialize.
 *            payload — output buffer of exactly PAYLOAD_LEN (127) bytes.
 *   Returns: nothing (fills payload).
 */
void msg_encode(const bf_msg* m, uint8_t payload[PAYLOAD_LEN]) {
	memset(payload, 0, PAYLOAD_LEN);
	put32(payload + 0, m->myRoot);
	put32(payload + 4, m->myCost);
	put32(payload + 8, m->myID);
	put32(payload + 12, m->expTime);
}

/*
 * msg_decode() — parse a 127-byte payload into a bf_msg.
 *   Functionality: reads the four fields from their fixed offsets, converting
 *     from network to host byte order.
 *   Inputs:  payload — input buffer of PAYLOAD_LEN (127) bytes.
 *            m       — output message.
 *   Returns: nothing (fills *m).
 */
void msg_decode(const uint8_t payload[PAYLOAD_LEN], bf_msg* m) {
	m->myRoot = get32(payload + 0);
	m->myCost = get32(payload + 4);
	m->myID = get32(payload + 8);
	m->expTime = get32(payload + 12);
}

/*
 * build_send_frame() — assemble a full 128-byte frame to hand to netproc.
 *   Functionality: sets byte 0 to the destination link, then encodes the
 *     payload into bytes 1..127.
 *   Inputs:  frame — output buffer of exactly FRAME_LEN (128) bytes.
 *            link  — destination link index (0-based), or LINK_BROADCAST (0xFF).
 *            m     — message to send.
 *   Returns: nothing (fills frame).
 */
void build_send_frame(uint8_t frame[FRAME_LEN], uint8_t link, const bf_msg* m) {
	frame[0] = link;
	msg_encode(m, frame + 1);
}

/*
 * parse_recv_frame() — split a received 128-byte frame.
 *   Functionality: extracts byte 0 (our ingress link, as stamped by netproc)
 *     and decodes the payload in bytes 1..127.
 *   Inputs:  frame — received buffer of FRAME_LEN (128) bytes.
 *            link  — out: the ingress link number.
 *            m     — out: the decoded message.
 *   Returns: nothing (fills *link and *m).
 */
void parse_recv_frame(const uint8_t frame[FRAME_LEN], uint8_t* link, bf_msg* m) {
	*link = frame[0];
	msg_decode(frame + 1, m);
}
