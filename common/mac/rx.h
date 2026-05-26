#ifndef LORAWAN_RX_H_
#define LORAWAN_RX_H_

#include <stdint.h>
#include <stddef.h>

struct rx_window_config {
	uint32_t freq;
	uint8_t dr;
	uint16_t timeout_ms;
	int64_t delay_ms;
};

int open_rx_windows(int64_t tx_end_time,
		    const struct rx_window_config *rx1,
		    const struct rx_window_config *rx2,
		    uint8_t *buf, size_t buflen,
		    int16_t *rssi, int8_t *snr);

#endif /* LORAWAN_RX_H_ */
