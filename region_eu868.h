/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 *
 * LoRaWAN EU868 region-specific parameters and functions.
 * Per LoRaWAN RP002-1.0.4 Regional Parameters.
 */

#ifndef LORAWAN_REGION_EU868_H_
#define LORAWAN_REGION_EU868_H_

#include "region.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EU868_NUM_CH         3
#define EU868_NUM_DR         7   /* DR0-DR6 (DR6: SF7/250kHz) */
#define EU868_RX2_FREQ      869525000U
#define EU868_RX2_DR         0
#define EU868_RX1_DELAY_S    1
#define EU868_RX2_DELAY_S    2
#define EU868_TX_MAX_DUTY    100  /* permille: 1% duty cycle */
#define EU868_DEFAULT_TX_POWER 16  /* ETSI limit: +14 dBm, +16 with antenna */

/* EU868 frequency band limits (ETSI) */
#define EU868_FREQ_MIN       863000000U
#define EU868_FREQ_MAX       870000000U

/* EU868 default channels */
#define EU868_CH0_FREQ       868100000U
#define EU868_CH1_FREQ       868300000U
#define EU868_CH2_FREQ       868500000U

/* EU868 join accept channels */
#define EU868_JA_CH0_FREQ    868100000U
#define EU868_JA_CH1_FREQ    868300000U
#define EU868_JA_CH2_FREQ    868500000U

extern const struct region_dr eu868_dr_table[EU868_NUM_DR];
extern const uint16_t eu868_dr_max_toa_ms[EU868_NUM_DR];
extern const uint8_t eu868_max_payload[EU868_NUM_DR];
extern const uint8_t eu868_rx1_dr_map[EU868_NUM_DR][EU868_NUM_DR];
extern const uint32_t eu868_ch_freq[EU868_NUM_CH];

static inline uint32_t eu868_get_rx1_freq(uint8_t tx_ch_idx)
{
	/* EU868: RX1 freq = TX freq */
	ARG_UNUSED(tx_ch_idx);
	return EU868_CH0_FREQ; /* actual freq set by channel mask */
}

static inline uint8_t eu868_get_rx1_dr(uint8_t tx_dr, uint8_t offset)
{
	if (tx_dr >= EU868_NUM_DR) {
		tx_dr = EU868_NUM_DR - 1;
	}
	if (offset >= EU868_NUM_DR) {
		offset = EU868_NUM_DR - 1;
	}
	return eu868_rx1_dr_map[tx_dr][offset];
}

static inline uint16_t eu868_get_rx_window_timeout_ms(uint8_t dr)
{
	if (dr >= EU868_NUM_DR) {
		return 3000;
	}
	return eu868_dr_max_toa_ms[dr] + 50;
}

static inline int eu868_get_max_payload(uint8_t dr)
{
	if (dr >= EU868_NUM_DR) {
		return -EINVAL;
	}
	return eu868_max_payload[dr];
}

static inline int eu868_apply_cflist(struct region_ctx *ctx, const uint8_t cflist[16])
{
	if (cflist[15] != 0) {
		return -EINVAL;
	}
	for (int i = 0; i < 5; i++) {
		uint32_t freq = ((uint32_t)cflist[i * 3] << 16) |
				((uint32_t)cflist[i * 3 + 1] << 8) |
				(uint32_t)cflist[i * 3 + 2];
		freq *= 100;
		if (freq < EU868_FREQ_MIN || freq > EU868_FREQ_MAX) {
			continue;
		}
		uint8_t idx = 3 + i;
		ctx->channels[idx].freq_hz = freq;
		ctx->channels[idx].min_dr = 0;
		ctx->channels[idx].max_dr = EU868_NUM_DR - 1;
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

#endif /* LORAWAN_REGION_EU868_H_ */
