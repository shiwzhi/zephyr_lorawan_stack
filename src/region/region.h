/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LORAWAN_REGION_H_
#define LORAWAN_REGION_H_

#include <zephyr/lorawan/lorawan.h>
#include <zephyr/drivers/lora.h>
#include <zephyr/random/random.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Max channels per region (CN470 has 96 uplink channels) */
#define LORAWAN_MAX_CHANNELS 96

/* Max supported data rates */
#define LORAWAN_MAX_DR 15

struct lorawan_channel {
	uint32_t freq_hz;
	uint8_t  min_dr;
	uint8_t  max_dr;
	bool     enabled;
};

struct region_dr {
	enum lora_datarate datarate;
	enum lora_signal_bandwidth bandwidth;
};

struct region_ctx {
	enum lorawan_region region;
	uint8_t num_channels;
	uint8_t num_dr;
	struct lorawan_channel channels[LORAWAN_MAX_CHANNELS];
	struct region_dr dr_table[LORAWAN_MAX_DR + 1];
	uint32_t rx2_freq;
	uint8_t rx2_dr;
	uint8_t rx1_delay_s;
	uint8_t rx2_delay_s;
	uint8_t default_tx_power;
	uint8_t tx_power_map[8];    /* LinkADR power index 0-7 to dBm */
	uint16_t max_tx_duty_cycle;  /* permille, e.g. 1000 = 100% */

	/* MAC command validation limits (set per region) */
	uint32_t freq_min_hz;    /* Minimum valid frequency */
	uint32_t freq_max_hz;    /* Maximum valid frequency */
	uint16_t max_channels;   /* Number of channel entries */

	/* Class C RXC parameters (defaults to RX2 values) */
	uint32_t rxc_freq;
	uint8_t  rxc_dr;

	/* Region-specific defaults */
	uint8_t  default_join_dr;  /* Default DR for join requests */
	uint32_t beacon_freq;      /* Default beacon frequency */
};

/* Per-region API — implemented in the selected region_xxxxx.c */
void region_init(struct region_ctx *ctx, enum lorawan_region region);
uint8_t region_get_rx1_dr(const struct region_ctx *ctx, uint8_t tx_dr,
			  uint8_t offset);
uint32_t region_get_rx1_freq(const struct region_ctx *ctx, uint8_t tx_ch_idx);
int region_get_max_payload(const struct region_ctx *ctx, uint8_t dr);
uint16_t region_get_rx_window_timeout_ms(const struct region_ctx *ctx, uint8_t dr);
int region_apply_cflist(struct region_ctx *ctx, const uint8_t cflist[16]);

/* Generic helpers — inline, use only ctx fields */

static inline int region_dr_to_lora(const struct region_ctx *ctx, uint8_t dr,
				    enum lora_datarate *sf,
				    enum lora_signal_bandwidth *bw)
{
	if (dr >= ctx->num_dr) {
		return -EINVAL;
	}
	*sf = ctx->dr_table[dr].datarate;
	*bw = ctx->dr_table[dr].bandwidth;
	return 0;
}

static inline const struct lorawan_channel *region_get_join_channel(
		const struct region_ctx *ctx)
{
	uint8_t enabled_indices[LORAWAN_MAX_CHANNELS];
	uint8_t count = 0;

	for (uint8_t i = 0; i < ctx->num_channels; i++) {
		if (ctx->channels[i].enabled) {
			enabled_indices[count++] = i;
		}
	}

	if (count > 0) {
		uint32_t r = sys_rand32_get();
		return &ctx->channels[enabled_indices[r % count]];
	}

	return &ctx->channels[0];
}

static inline uint32_t region_get_beacon_freq(const struct region_ctx *ctx)
{
	return ctx->beacon_freq;
}

/* Shared CFList helpers used by per-region implementations */

static inline int region_cflist_type_a(struct region_ctx *ctx,
				       const uint8_t cflist[16],
				       uint8_t start_idx,
				       uint32_t freq_min, uint32_t freq_max,
				       uint8_t num_dr, uint8_t min_channels)
{
	if (cflist[15] != 0) {
		return -EINVAL;
	}
	for (int i = 0; i < 5; i++) {
		uint32_t freq = ((uint32_t)cflist[i * 3] << 16) |
				((uint32_t)cflist[i * 3 + 1] << 8) |
				(uint32_t)cflist[i * 3 + 2];
		freq *= 100;
		if (freq < freq_min || freq > freq_max) {
			continue;
		}
		uint8_t idx = start_idx + i;
		ctx->channels[idx].freq_hz = freq;
		ctx->channels[idx].min_dr = 0;
		ctx->channels[idx].max_dr = num_dr - 1;
		ctx->channels[idx].enabled = true;
	}
	if (min_channels > 0) {
		if (ctx->num_channels < min_channels) {
			ctx->num_channels = min_channels;
		}
		if (ctx->max_channels < min_channels) {
			ctx->max_channels = min_channels;
		}
	}
	return 0;
}

static inline int region_cflist_type_b(struct region_ctx *ctx,
				       const uint8_t cflist[16])
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

static inline void region_init_tx_power_map(struct region_ctx *ctx)
{
	for (uint8_t i = 0; i < 8; i++) {
		int p = (int)ctx->default_tx_power - 2 * (int)i;
		ctx->tx_power_map[i] = (p < 0) ? 0 : (uint8_t)p;
	}
}

#ifdef __cplusplus
}
#endif

#endif /* LORAWAN_REGION_H_ */
