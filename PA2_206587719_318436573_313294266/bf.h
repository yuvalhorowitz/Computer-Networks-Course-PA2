#ifndef BF_H_
#define BF_H_

/* Provided course constants only.
 *
 * Per the course staff, bf.h "should not include anything except the constant
 * definitions" and will be replaced by the graders' own copy at test time.
 * Everything else our protocol needs (frame sizes, the BPDU struct) therefore
 * lives in msg.h, not here. */
#define PORT          6789
#define HELLO_TIMEOUT 2                   /* seconds */
#define ROOT_TIMEOUT  (3 * HELLO_TIMEOUT) /* seconds */

#endif /* BF_H_ */
