#ifndef LORAWAN_JOIN_H_
#define LORAWAN_JOIN_H_

#include <stdint.h>
#include <stddef.h>

int send_join_request(uint8_t *rx_buf, size_t rx_buflen);

#endif /* LORAWAN_JOIN_H_ */
