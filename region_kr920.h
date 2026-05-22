/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 *
 * LoRaWAN KR920 region-specific parameters and functions.
 * Per LoRaWAN RP002-1.0.4 Regional Parameters.
 */

#ifndef LORAWAN_REGION_KR920_H_
#define LORAWAN_REGION_KR920_H_

#include "region.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KR920_NUM_CH         8
#define KR920_NUM_DR         5
#define KR920_RX2_FREQ      921900000U
#define KR920_RX2_DR         0
#define KR920_RX1_DELAY_S    1
#define KR920_RX2_DELAY_S    2
#define KR920_TX_MAX_DUTY    100  /* permille: 1% duty cycle */
#define KR920_DEFAULT_TX_POWER 14

/* KR920 frequency band limits (KCC) */
#define KR920_FREQ_MIN       920000000U
#define KR920_FREQ_MAX       923000000U

extern const struct region_dr kr920_dr_table[KR920_NUM_DR];
extern const uint16_t kr920_dr_max_toa_ms[KR920_NUM_DR];
extern const uint8_t kr920_max_payload[KR920_NUM_DR];
extern const uint8_t kr920_rx1_dr_map[KR920_NUM_DR][KR920_NUM_DR];
extern const uint32_t kr920_ch_freq[KR920_NUM_CH];

static inline uint32_t kr920_get_rx1_freq(uint8_t tx_ch_idx)
{
	/* KR920: RX1 freq = TX freq */
	ARG_UNUSED(tx_ch_idx);
	return 0; /* actual freq set by channel mask */
}

static inline uint8_t kr920_get_rx1_dr(uint8_t tx_dr, uint8_t offset)
{
	if (tx_dr >= KR920_NUM_DR) {
		tx_dr = KR920_NUM_DR - 1;
	}
	if (offset >= KR920_NUM_DR) {
		offset = KR920_NUM_DR - 1;
	}
	return kr920_rx1_dr_map[tx_dr][offset];
}

static inline uint16_t kr920_get_rx_window_timeout_ms(uint8_t dr)
{
	if (dr >= KR920_NUM_DR) {
		return 3000;
	}
	return kr920_dr_max_toa_ms[dr] + 50;
}

static inline int kr920_get_max_payload(uint8_t dr)
{
	if (dr >= KR920_NUM_DR) {
		return -EINVAL;
	}
	return kr920_max_payload[dr];
}

static inline int kr920_apply_cflist(struct region_ctx *ctx, const uint8_t cflist[16])
{
	if (cflist[15] != 0) {
		return -EINVAL;
	}
	for (int i = 0; i < 5; i++) {
		uint32_t freq = ((uint32_t)cflist[i * 3] << 16) |
				((uint32_t)cflist[i * 3 + 1] << 8) |
				(uint32_t)cflist[i * 3 + 2];
		freq *= 100;
		if (freq < KR920_FREQ_MIN || freq > KR920_FREQ_MAX) {
			continue;
		}
		uint8_t idx = 3 + i;
		ctx->channels[idx].freq_hz = freq;
		ctx->channels[idx].min_dr = 0;
		ctx->channels[idx].max_dr = KR920_NUM_DR - 1;
		ctx->channels[idx].enabled = true;
	}
	return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* LORAWAN_REGION_KR920_H_ */
