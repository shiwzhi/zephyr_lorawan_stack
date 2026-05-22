/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 *
 * LoRaWAN IN865 region-specific parameters and functions.
 * Per LoRaWAN RP002-1.0.4 Regional Parameters.
 */

#ifndef LORAWAN_REGION_IN865_H_
#define LORAWAN_REGION_IN865_H_

#include "region.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IN865_NUM_CH         3
#define IN865_NUM_DR         6
#define IN865_RX2_FREQ      866550000U
#define IN865_RX2_DR         2
#define IN865_RX1_DELAY_S    1
#define IN865_RX2_DELAY_S    2
#define IN865_DEFAULT_TX_POWER 30

/* IN865 frequency band limits (DoT) */
#define IN865_FREQ_MIN       865000000U
#define IN865_FREQ_MAX       867000000U

/* IN865 default channels */
#define IN865_CH0_FREQ       865062500U
#define IN865_CH1_FREQ       865402500U
#define IN865_CH2_FREQ       865985000U

extern const struct region_dr in865_dr_table[IN865_NUM_DR];
extern const uint16_t in865_dr_max_toa_ms[IN865_NUM_DR];
extern const uint8_t in865_max_payload[IN865_NUM_DR];
extern const uint8_t in865_rx1_dr_map[IN865_NUM_DR][IN865_NUM_DR];
extern const uint32_t in865_ch_freq[IN865_NUM_CH];

static inline uint32_t in865_get_rx1_freq(uint8_t tx_ch_idx)
{
	/* IN865: RX1 freq = TX freq */
	ARG_UNUSED(tx_ch_idx);
	return 0;
}

static inline uint8_t in865_get_rx1_dr(uint8_t tx_dr, uint8_t offset)
{
	if (tx_dr >= IN865_NUM_DR) {
		tx_dr = IN865_NUM_DR - 1;
	}
	if (offset >= IN865_NUM_DR) {
		offset = IN865_NUM_DR - 1;
	}
	return in865_rx1_dr_map[tx_dr][offset];
}

static inline uint16_t in865_get_rx_window_timeout_ms(uint8_t dr)
{
	if (dr >= IN865_NUM_DR) {
		return 3000;
	}
	return in865_dr_max_toa_ms[dr] + 50;
}

static inline int in865_get_max_payload(uint8_t dr)
{
	if (dr >= IN865_NUM_DR) {
		return -EINVAL;
	}
	return in865_max_payload[dr];
}

static inline int in865_apply_cflist(struct region_ctx *ctx, const uint8_t cflist[16])
{
	if (cflist[15] != 0) {
		return -EINVAL;
	}
	for (int i = 0; i < 5; i++) {
		uint32_t freq = ((uint32_t)cflist[i * 3] << 16) |
				((uint32_t)cflist[i * 3 + 1] << 8) |
				(uint32_t)cflist[i * 3 + 2];
		freq *= 100;
		if (freq < IN865_FREQ_MIN || freq > IN865_FREQ_MAX) {
			continue;
		}
		uint8_t idx = 3 + i;
		ctx->channels[idx].freq_hz = freq;
		ctx->channels[idx].min_dr = 0;
		ctx->channels[idx].max_dr = IN865_NUM_DR - 1;
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

#endif /* LORAWAN_REGION_IN865_H_ */
