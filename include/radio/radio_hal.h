#ifndef LORAWAN_RADIO_HAL_H_
#define LORAWAN_RADIO_HAL_H_

#include <stdint.h>
#include <stddef.h>

struct lorawan_channel;

struct lorawan_tx_config {
	uint32_t freq;
	uint8_t  dr;
	uint8_t *buf;
	size_t   len;
};

struct lorawan_rx_config {
	uint32_t rx1_freq;
	uint8_t  rx1_dr;
	uint16_t rx1_timeout_ms;
	int64_t  rx1_delay_ms;

	uint32_t rx2_freq;
	uint8_t  rx2_dr;
	uint16_t rx2_timeout_ms;
	int64_t  rx2_delay_ms;
};

struct lorawan_rx_result {
	int      len;
	int16_t  rssi;
	int8_t   snr;
};

uint32_t get_rx2_freq(void);
uint8_t get_rx2_dr(void);

int radio_configure_rx(uint32_t freq, uint8_t dr);
int8_t get_tx_power_dbm(void);
int radio_configure_tx(uint32_t freq, uint8_t dr);

int lorawan_tx(const struct lorawan_tx_config *cfg);
int lorawan_rx(const struct lorawan_rx_config *cfg,
	       int64_t tx_end_time,
	       uint8_t *rx_buf, size_t rx_buflen,
	       struct lorawan_rx_result *result);

static inline void lorawan_memcpy_rev(uint8_t *dst, const uint8_t *src, uint8_t len)
{
	for (uint8_t i = 0; i < len; i++) {
		dst[i] = src[len - 1 - i];
	}
}

void build_fhdr(uint8_t *buf, uint32_t dev_addr, uint8_t fctrl, uint16_t fcnt);

const struct lorawan_channel *get_data_channel(void);

#endif /* LORAWAN_RADIO_HAL_H_ */
