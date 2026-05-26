#ifndef LORAWAN_RADIO_HAL_H_
#define LORAWAN_RADIO_HAL_H_

#include <stdint.h>
#include <stddef.h>

struct lorawan_channel;

uint32_t get_rx2_freq(void);
uint8_t get_rx2_dr(void);

int radio_configure_rx(uint32_t freq, uint8_t dr);
int8_t get_tx_power_dbm(void);
int radio_configure_tx(uint32_t freq, uint8_t dr);

static inline void lorawan_memcpy_rev(uint8_t *dst, const uint8_t *src, uint8_t len)
{
	for (uint8_t i = 0; i < len; i++) {
		dst[i] = src[len - 1 - i];
	}
}

void build_fhdr(uint8_t *buf, uint32_t dev_addr, uint8_t fctrl, uint16_t fcnt);

const struct lorawan_channel *get_data_channel(void);

#endif /* LORAWAN_RADIO_HAL_H_ */
