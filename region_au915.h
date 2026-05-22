/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 *
 * LoRaWAN AU915 region-specific parameters and functions.
 * Per LoRaWAN RP002-1.0.4 Regional Parameters.
 *
 * AU915 uses 64 x 125kHz + 8 x 500kHz uplink channels.
 * Downlink uses 8 x 500kHz channels at 923.3 MHz.
 * RX1 channel = uplink channel modulo 8.
 */

#ifndef LORAWAN_REGION_AU915_H_
#define LORAWAN_REGION_AU915_H_

#include "region.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AU915_NUM_UP_CH      72   /* 64 x 125kHz + 8 x 500kHz */
#define AU915_NUM_125K_CH    64
#define AU915_NUM_500K_CH    8
#define AU915_NUM_DOWN_CH    8
#define AU915_NUM_DR         10
#define AU915_RX2_FREQ      923300000U
#define AU915_RX2_DR         8
#define AU915_RX1_DELAY_S    1
#define AU915_RX2_DELAY_S    2
#define AU915_DEFAULT_TX_POWER 30

/* AU915 frequency band limits (ACMA) */
#define AU915_FREQ_MIN       915000000U
#define AU915_FREQ_MAX       928000000U

/* AU915 uplink frequency ranges */
#define AU915_UP_FREQ_START  915200000U
#define AU915_UP_FREQ_STEP   200000U
#define AU915_500K_FREQ_START 915900000U
#define AU915_500K_FREQ_STEP 1600000U

/* AU915 downlink frequency range */
#define AU915_DOWN_FREQ_START 923300000U
#define AU915_DOWN_FREQ_STEP  600000U

extern const struct region_dr au915_dr_table[AU915_NUM_DR];
extern const uint16_t au915_dr_max_toa_ms[AU915_NUM_DR];
extern const uint8_t au915_max_payload[AU915_NUM_DR];
extern const uint8_t au915_rx1_dr_map[AU915_NUM_DR][AU915_NUM_DR];

static inline uint32_t au915_get_up_ch_freq(uint8_t ch_idx)
{
	if (ch_idx < AU915_NUM_125K_CH) {
		return AU915_UP_FREQ_START + ch_idx * AU915_UP_FREQ_STEP;
	}
	return AU915_500K_FREQ_START + (ch_idx - AU915_NUM_125K_CH) * AU915_500K_FREQ_STEP;
}

static inline uint32_t au915_get_rx1_freq(uint8_t tx_ch_idx)
{
	uint8_t rx_ch;
	if (tx_ch_idx < AU915_NUM_125K_CH) {
		rx_ch = tx_ch_idx % AU915_NUM_DOWN_CH;
	} else {
		rx_ch = tx_ch_idx - AU915_NUM_125K_CH;
	}
	return AU915_DOWN_FREQ_START + rx_ch * AU915_DOWN_FREQ_STEP;
}

static inline uint8_t au915_get_rx1_dr(uint8_t tx_dr, uint8_t offset)
{
	if (tx_dr >= AU915_NUM_DR) {
		tx_dr = AU915_NUM_DR - 1;
	}
	if (offset >= AU915_NUM_DR) {
		offset = AU915_NUM_DR - 1;
	}
	return au915_rx1_dr_map[tx_dr][offset];
}

static inline uint16_t au915_get_rx_window_timeout_ms(uint8_t dr)
{
	if (dr >= AU915_NUM_DR) {
		return 3000;
	}
	return au915_dr_max_toa_ms[dr] + 50;
}

static inline int au915_get_max_payload(uint8_t dr)
{
	if (dr >= AU915_NUM_DR) {
		return -EINVAL;
	}
	return au915_max_payload[dr];
}

static inline int au915_apply_cflist(struct region_ctx *ctx, const uint8_t cflist[16])
{
	if (cflist[15] != 1) {
		return -EINVAL;
	}
	for (uint8_t block = 0; block < 6; block++) {
		uint16_t mask = (uint16_t)cflist[block * 2] |
			       ((uint16_t)cflist[block * 2 + 1] << 8);
		int start = block * 16;
		for (int c = start; c < start + 16 && c < ctx->num_channels; c++) {
			ctx->channels[c].enabled = (mask & (1 << (c - start))) ? true : false;
		}
	}
	return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* LORAWAN_REGION_AU915_H_ */
