/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 *
 * LoRaWAN RU864 region implementation.
 * Per LoRaWAN RP002-1.0.4 Regional Parameters.
 */

#include "region/region.h"
#include <errno.h>

#define RU864_NUM_CH         8
#define RU864_NUM_DR         6
#define RU864_RX2_FREQ      869100000U
#define RU864_RX2_DR         0
#define RU864_RX1_DELAY_S    1
#define RU864_RX2_DELAY_S    2
#define RU864_TX_MAX_DUTY    100
#define RU864_DEFAULT_TX_POWER 16
#define RU864_FREQ_MIN       864000000U
#define RU864_FREQ_MAX       870000000U

static const struct region_dr dr_table[RU864_NUM_DR] = {
	{SF_12, BW_125_KHZ}, {SF_11, BW_125_KHZ}, {SF_10, BW_125_KHZ},
	{SF_9,  BW_125_KHZ}, {SF_8,  BW_125_KHZ}, {SF_7,  BW_125_KHZ},
};

static const uint16_t dr_max_toa_ms[RU864_NUM_DR] = {
	1600, 900, 500, 300, 180, 120,
};

static const uint8_t max_payload[RU864_NUM_DR] = {
	59, 59, 59, 123, 230, 230,
};

static const uint8_t rx1_dr_map[RU864_NUM_DR][RU864_NUM_DR] = {
	{0, 0, 0, 0, 0, 0},
	{1, 0, 0, 0, 0, 0},
	{2, 1, 0, 0, 0, 0},
	{3, 2, 1, 0, 0, 0},
	{4, 3, 2, 1, 0, 0},
	{5, 4, 3, 2, 1, 0},
};

static const uint32_t ch_freq[RU864_NUM_CH] = {
	868900000U, 869100000U, 869300000U, 869500000U,
	869700000U, 869900000U, 870100000U, 870300000U,
};

void region_init(struct region_ctx *ctx, enum lorawan_region region)
{
	memset(ctx, 0, sizeof(*ctx));
	ctx->region = LORAWAN_REGION_RU864;
	ctx->num_channels = RU864_NUM_CH;
	for (uint8_t i = 0; i < RU864_NUM_CH; i++) {
		ctx->channels[i].freq_hz = ch_freq[i];
		ctx->channels[i].min_dr = 0;
		ctx->channels[i].max_dr = RU864_NUM_DR - 1;
		ctx->channels[i].enabled = true;
	}
	ctx->num_dr = RU864_NUM_DR;
	for (uint8_t i = 0; i < RU864_NUM_DR; i++) {
		ctx->dr_table[i] = dr_table[i];
	}
	ctx->rx2_freq = RU864_RX2_FREQ;
	ctx->rx2_dr = RU864_RX2_DR;
	ctx->rx1_delay_s = RU864_RX1_DELAY_S;
	ctx->rx2_delay_s = RU864_RX2_DELAY_S;
	ctx->default_tx_power = RU864_DEFAULT_TX_POWER;
	region_init_tx_power_map(ctx);
	ctx->max_tx_duty_cycle = RU864_TX_MAX_DUTY;
	ctx->freq_min_hz = RU864_FREQ_MIN;
	ctx->freq_max_hz = RU864_FREQ_MAX;
	ctx->max_channels = RU864_NUM_CH;
	ctx->default_join_dr = 0;
	ctx->beacon_freq = 869100000U;
	ctx->rxc_freq = ctx->rx2_freq;
	ctx->rxc_dr = ctx->rx2_dr;
}

uint8_t region_get_rx1_dr(const struct region_ctx *ctx, uint8_t tx_dr,
			  uint8_t offset)
{
	ARG_UNUSED(ctx);
	if (tx_dr >= RU864_NUM_DR) tx_dr = RU864_NUM_DR - 1;
	if (offset >= RU864_NUM_DR) offset = RU864_NUM_DR - 1;
	return rx1_dr_map[tx_dr][offset];
}

uint32_t region_get_rx1_freq(const struct region_ctx *ctx, uint8_t tx_ch_idx)
{
	ARG_UNUSED(ctx);
	ARG_UNUSED(tx_ch_idx);
	return 0;
}

int region_get_max_payload(const struct region_ctx *ctx, uint8_t dr)
{
	ARG_UNUSED(ctx);
	if (dr >= RU864_NUM_DR) return -EINVAL;
	return max_payload[dr];
}

uint16_t region_get_rx_window_timeout_ms(const struct region_ctx *ctx, uint8_t dr)
{
	ARG_UNUSED(ctx);
	if (dr >= RU864_NUM_DR) return 3000;
	return dr_max_toa_ms[dr] + 50;
}

int region_apply_cflist(struct region_ctx *ctx, const uint8_t cflist[16])
{
	return region_cflist_type_a(ctx, cflist, 2, RU864_FREQ_MIN,
				    RU864_FREQ_MAX, RU864_NUM_DR, 0);
}
