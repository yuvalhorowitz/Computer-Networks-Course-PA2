/*
 * msg.h — (de)serialization of BPDU messages and 128-byte netproc frames.
 *
 * Purpose: the single place that knows the on-the-wire byte layout, so the rest
 * of bfproc works with the bf_msg struct and never touches raw offsets. Keeping
 * this separate also lets the format be unit-tested without sockets.
 *
 * Wire payload layout (127 bytes, network byte order, rest zero-padded):
 *   bytes  0..3   myRoot
 *   bytes  4..7   myCost
 *   bytes  8..11  myID
 *   bytes 12..15  expTime (milliseconds)
 *   bytes 16..126 zero padding
 *
 * Frame layout (128 bytes): [ byte 0 = link ][ bytes 1..127 = payload ].
 * On send, byte 0 is the destination link (0-based) or LINK_BROADCAST (0xFF).
 * On receive, byte 0 is *our* ingress link (netproc rewrites it).
 *
 * Full per-function documentation lives at each definition in msg.c.
 */
#ifndef MSG_H_
#define MSG_H_

#include <stdint.h>

#include "bf.h"

/* Encode/decode just the 127-byte payload (no link byte). */
void msg_encode(const bf_msg* m, uint8_t payload[PAYLOAD_LEN]);
void msg_decode(const uint8_t payload[PAYLOAD_LEN], bf_msg* m);

/* Build/parse a full 128-byte frame (link byte + payload). */
void build_send_frame(uint8_t frame[FRAME_LEN], uint8_t link, const bf_msg* m);
void parse_recv_frame(const uint8_t frame[FRAME_LEN], uint8_t* link, bf_msg* m);

#endif /* MSG_H_ */
