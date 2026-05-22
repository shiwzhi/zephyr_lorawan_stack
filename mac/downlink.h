#ifndef LORAWAN_DOWNLINK_H_
#define LORAWAN_DOWNLINK_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

void process_downlink(uint8_t *phy, size_t len, int16_t rssi,
		      int8_t snr, bool *ack_received);

#endif /* LORAWAN_DOWNLINK_H_ */
