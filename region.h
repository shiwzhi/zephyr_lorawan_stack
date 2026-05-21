/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LORAWAN_REGION_H_
#define LORAWAN_REGION_H_

#include <zephyr/lorawan/lorawan.h>
#include <zephyr/drivers/lora.h>
#include <stdint.h>
#include <stdbool.h>

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
};

void region_init(struct region_ctx *ctx, enum lorawan_region region);
int region_dr_to_lora(const struct region_ctx *ctx, uint8_t dr, enum lora_datarate *sf,
		      enum lora_signal_bandwidth *bw);
uint8_t region_get_rx1_dr(const struct region_ctx *ctx, uint8_t tx_dr,
			  uint8_t offset);
uint32_t region_get_rx1_freq(const struct region_ctx *ctx, uint8_t tx_ch_idx);
const struct lorawan_channel *region_get_join_channel(const struct region_ctx *ctx);
int region_get_max_payload(const struct region_ctx *ctx, uint8_t dr);
uint32_t region_get_beacon_freq(uint32_t beacon_time);
uint16_t region_get_rx_window_timeout_ms(const struct region_ctx *ctx, uint8_t dr);

#ifdef __cplusplus
}
#endif

#endif /* LORAWAN_REGION_H_ */
