/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 *
 * LoRaWAN CN779 region-specific parameters and functions.
 * Per LoRaWAN RP002-1.0.4 Regional Parameters.
 */

#ifndef LORAWAN_REGION_CN779_H_
#define LORAWAN_REGION_CN779_H_

#include "region.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CN779_NUM_CH         3
#define CN779_NUM_DR         6
#define CN779_RX2_FREQ      786000000U
#define CN779_RX2_DR         0
#define CN779_RX1_DELAY_S    1
#define CN779_RX2_DELAY_S    2
#define CN779_DEFAULT_TX_POWER 12

/* CN779 frequency band limits (SRRC) */
#define CN779_FREQ_MIN       779000000U
#define CN779_FREQ_MAX       787000000U

/* CN779 default channels */
#define CN779_CH0_FREQ       779500000U
#define CN779_CH1_FREQ       779700000U
#define CN779_CH2_FREQ       779900000U

extern const struct region_dr cn779_dr_table[CN779_NUM_DR];
extern const uint16_t cn779_dr_max_toa_ms[CN779_NUM_DR];
extern const uint8_t cn779_max_payload[CN779_NUM_DR];
extern const uint8_t cn779_rx1_dr_map[CN779_NUM_DR][CN779_NUM_DR];
extern const uint32_t cn779_ch_freq[CN779_NUM_CH];

static inline uint32_t cn779_get_rx1_freq(uint8_t tx_ch_idx)
{
	ARG_UNUSED(tx_ch_idx);
	return 0;
}

static inline uint8_t cn779_get_rx1_dr(uint8_t tx_dr, uint8_t offset)
{
	if (tx_dr >= CN779_NUM_DR) {
		tx_dr = CN779_NUM_DR - 1;
	}
	if (offset >= CN779_NUM_DR) {
		offset = CN779_NUM_DR - 1;
	}
	return cn779_rx1_dr_map[tx_dr][offset];
}

static inline uint16_t cn779_get_rx_window_timeout_ms(uint8_t dr)
{
	if (dr >= CN779_NUM_DR) {
		return 3000;
	}
	return cn779_dr_max_toa_ms[dr] + 50;
}

static inline int cn779_get_max_payload(uint8_t dr)
{
	if (dr >= CN779_NUM_DR) {
		return -EINVAL;
	}
	return cn779_max_payload[dr];
}

static inline int cn779_apply_cflist(struct region_ctx *ctx, const uint8_t cflist[16])
{
	if (cflist[15] != 0) {
		return -EINVAL;
	}
	for (int i = 0; i < 5; i++) {
		uint32_t freq = ((uint32_t)cflist[i * 3] << 16) |
				((uint32_t)cflist[i * 3 + 1] << 8) |
				(uint32_t)cflist[i * 3 + 2];
		freq *= 100;
		if (freq < CN779_FREQ_MIN || freq > CN779_FREQ_MAX) {
			continue;
		}
		uint8_t idx = 3 + i;
		ctx->channels[idx].freq_hz = freq;
		ctx->channels[idx].min_dr = 0;
		ctx->channels[idx].max_dr = CN779_NUM_DR - 1;
		ctx->channels[idx].enabled = true;
	}
	if (ctx->num_channels < 8) {
		ctx->num_channels = 8;
	}
	if (ctx->max_channels < 8) {
		ctx->max_channels = 8;
	}
	return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* LORAWAN_REGION_CN779_H_ */
