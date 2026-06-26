/* Throwaway round-trip test for the msg serialization (PA2 Step 3).
 * No sockets, so it runs anywhere. Build:
 *     cc -std=gnu11 -Wall -Wextra -I. -o build/test_msg build/test_msg.c msg.c
 * Run: ./build/test_msg
 */
#include "msg.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
	bf_msg in = {.myRoot = 7416, .myCost = 85, .myID = 56908, .expTime = 6000};

	uint8_t frame[FRAME_LEN];
	build_send_frame(frame, 2, &in); /* link 2 */

	uint8_t link;
	bf_msg out;
	parse_recv_frame(frame, &link, &out);

	assert(link == 2);
	assert(out.myRoot == in.myRoot);
	assert(out.myCost == in.myCost);
	assert(out.myID == in.myID);
	assert(out.expTime == in.expTime);
	for (int i = 17; i < FRAME_LEN; i++) assert(frame[i] == 0); /* padding */

	printf("round-trip OK: link=%u root=%u cost=%u id=%u exp=%u\n",
		   link, out.myRoot, out.myCost, out.myID, out.expTime);
	printf("frame[0] (link) = %u\n", frame[0]);
	printf("payload[0..15]  =");
	for (int i = 1; i <= 16; i++) printf(" %02x", frame[i]);
	printf("\npadding (bytes 17..127) all zero: OK\n");
	return 0;
}
