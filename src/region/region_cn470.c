/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 *
 * LoRaWAN CN470-510 region implementation.
 * Per LoRaWAN RP002-1.0.4 Regional Parameters.
 * 96 uplink channels, DR0-DR5.
 */

#include "region/region.h"
#include <errno.h>

#define CN470_NUM_UP_CH      96
#define CN470_NUM_DOWN_CH    48
#define CN470_NUM_DR         6
#define CN470_UP_FREQ_START   470300000U
#define CN470_UP_FREQ_STEP    200000U
#define CN470_DOWN_FREQ_START 500300000U
#define CN470_DOWN_FREQ_STEP  200000U
#define CN470_BEACON_FREQ_START 508300000U
#define CN470_FREQ_MIN       470000000U
#define CN470_FREQ_MAX       510000000U
#define CN470_RX2_FREQ       505300000U
#define CN470_RX2_DR         0
#define CN470_RX1_DELAY_S    1
#define CN470_RX2_DELAY_S    2
#define CN470_DEFAULT_TX_POWER 14

static const struct region_dr dr_table[CN470_NUM_DR] = {
	{SF_12, BW_125_KHZ}, {SF_11, BW_125_KHZ}, {SF_10, BW_125_KHZ},
	{SF_9,  BW_125_KHZ}, {SF_8,  BW_125_KHZ}, {SF_7,  BW_125_KHZ},
};

static const uint16_t dr_max_toa_ms[CN470_NUM_DR] = {
	1400, 800, 450, 260, 160, 100,
};

static const uint8_t max_payload[CN470_NUM_DR] = {
	51, 51, 51, 115, 222, 222,
};

static const uint8_t rx1_dr_map[CN470_NUM_DR][CN470_NUM_DR] = {
	{0, 0, 0, 0, 0, 0},
	{1, 0, 0, 0, 0, 0},
	{2, 1, 0, 0, 0, 0},
	{3, 2, 1, 0, 0, 0},
	{4, 3, 2, 1, 0, 0},
	{5, 4, 3, 2, 1, 0},
};

void region_init(struct region_ctx *ctx, enum lorawan_region region)
{
	memset(ctx, 0, sizeof(*ctx));
	ctx->region = LORAWAN_REGION_CN470;
	ctx->num_channels = CN470_NUM_UP_CH;
	for (uint8_t i = 0; i < CN470_NUM_UP_CH; i++) {
		ctx->channels[i].freq_hz = CN470_UP_FREQ_START + (i * CN470_UP_FREQ_STEP);
		ctx->channels[i].min_dr = 0;
		ctx->channels[i].max_dr = CN470_NUM_DR - 1;
		ctx->channels[i].enabled = true;
	}
	ctx->num_dr = CN470_NUM_DR;
	for (uint8_t i = 0; i < CN470_NUM_DR; i++) {
		ctx->dr_table[i] = dr_table[i];
	}
	ctx->rx2_freq = CN470_RX2_FREQ;
	ctx->rx2_dr = CN470_RX2_DR;
	ctx->rx1_delay_s = CN470_RX1_DELAY_S;
	ctx->rx2_delay_s = CN470_RX2_DELAY_S;
	ctx->default_tx_power = CN470_DEFAULT_TX_POWER;
	region_init_tx_power_map(ctx);
	ctx->freq_min_hz = CN470_FREQ_MIN;
	ctx->freq_max_hz = CN470_FREQ_MAX;
	ctx->max_channels = CN470_NUM_UP_CH;
	ctx->default_join_dr = 3;
	ctx->beacon_freq = 508300000U;
	ctx->rxc_freq = ctx->rx2_freq;
	ctx->rxc_dr = ctx->rx2_dr;
}

uint8_t region_get_rx1_dr(const struct region_ctx *ctx, uint8_t tx_dr,
			  uint8_t offset)
{
	ARG_UNUSED(ctx);
	if (tx_dr >= CN470_NUM_DR) tx_dr = CN470_NUM_DR - 1;
	if (offset >= CN470_NUM_DR) offset = CN470_NUM_DR - 1;
	return rx1_dr_map[tx_dr][offset];
}

uint32_t region_get_rx1_freq(const struct region_ctx *ctx, uint8_t tx_ch_idx)
{
	ARG_UNUSED(ctx);
	uint8_t rx_ch = tx_ch_idx % CN470_NUM_DOWN_CH;
	return CN470_DOWN_FREQ_START + (rx_ch * CN470_DOWN_FREQ_STEP);
}

int region_get_max_payload(const struct region_ctx *ctx, uint8_t dr)
{
	ARG_UNUSED(ctx);
	if (dr >= CN470_NUM_DR) return -EINVAL;
	return max_payload[dr];
}

uint16_t region_get_rx_window_timeout_ms(const struct region_ctx *ctx, uint8_t dr)
{
	ARG_UNUSED(ctx);
	if (dr >= CN470_NUM_DR) return 3000;
	return dr_max_toa_ms[dr] + 50;
}

int region_apply_cflist(struct region_ctx *ctx, const uint8_t cflist[16])
{
	return region_cflist_type_b(ctx, cflist);
}
