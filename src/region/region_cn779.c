/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 *
 * LoRaWAN CN779 region implementation.
 * Per LoRaWAN RP002-1.0.4 Regional Parameters.
 */

#include "region/region.h"
#include <errno.h>

#define CN779_NUM_CH         3
#define CN779_NUM_DR         6
#define CN779_RX2_FREQ      786000000U
#define CN779_RX2_DR         0
#define CN779_RX1_DELAY_S    1
#define CN779_RX2_DELAY_S    2
#define CN779_DEFAULT_TX_POWER 12
#define CN779_FREQ_MIN       779000000U
#define CN779_FREQ_MAX       787000000U
#define CN779_CH0_FREQ       779500000U

static const struct region_dr dr_table[CN779_NUM_DR] = {
	{SF_12, BW_125_KHZ}, {SF_11, BW_125_KHZ}, {SF_10, BW_125_KHZ},
	{SF_9,  BW_125_KHZ}, {SF_8,  BW_125_KHZ}, {SF_7,  BW_125_KHZ},
};

static const uint16_t dr_max_toa_ms[CN779_NUM_DR] = {
	1600, 900, 500, 300, 180, 120,
};

static const uint8_t max_payload[CN779_NUM_DR] = {
	59, 59, 59, 123, 230, 230,
};

static const uint8_t rx1_dr_map[CN779_NUM_DR][CN779_NUM_DR] = {
	{0, 0, 0, 0, 0, 0},
	{1, 0, 0, 0, 0, 0},
	{2, 1, 0, 0, 0, 0},
	{3, 2, 1, 0, 0, 0},
	{4, 3, 2, 1, 0, 0},
	{5, 4, 3, 2, 1, 0},
};

static const uint32_t ch_freq[CN779_NUM_CH] = {
	779500000U, 779700000U, 779900000U,
};

void region_init(struct region_ctx *ctx, enum lorawan_region region)
{
	memset(ctx, 0, sizeof(*ctx));
	ctx->region = LORAWAN_REGION_CN779;
	ctx->num_channels = CN779_NUM_CH;
	for (uint8_t i = 0; i < CN779_NUM_CH; i++) {
		ctx->channels[i].freq_hz = ch_freq[i];
		ctx->channels[i].min_dr = 0;
		ctx->channels[i].max_dr = CN779_NUM_DR - 1;
		ctx->channels[i].enabled = true;
	}
	ctx->num_dr = CN779_NUM_DR;
	for (uint8_t i = 0; i < CN779_NUM_DR; i++) {
		ctx->dr_table[i] = dr_table[i];
	}
	ctx->rx2_freq = CN779_RX2_FREQ;
	ctx->rx2_dr = CN779_RX2_DR;
	ctx->rx1_delay_s = CN779_RX1_DELAY_S;
	ctx->rx2_delay_s = CN779_RX2_DELAY_S;
	ctx->default_tx_power = CN779_DEFAULT_TX_POWER;
	region_init_tx_power_map(ctx);
	ctx->freq_min_hz = CN779_FREQ_MIN;
	ctx->freq_max_hz = CN779_FREQ_MAX;
	ctx->max_channels = CN779_NUM_CH;
	ctx->default_join_dr = 0;
	ctx->beacon_freq = 785000000U;
	ctx->rxc_freq = ctx->rx2_freq;
	ctx->rxc_dr = ctx->rx2_dr;
}

uint8_t region_get_rx1_dr(const struct region_ctx *ctx, uint8_t tx_dr,
			  uint8_t offset)
{
	ARG_UNUSED(ctx);
	if (tx_dr >= CN779_NUM_DR) tx_dr = CN779_NUM_DR - 1;
	if (offset >= CN779_NUM_DR) offset = CN779_NUM_DR - 1;
	return rx1_dr_map[tx_dr][offset];
}

uint32_t region_get_rx1_freq(const struct region_ctx *ctx, uint8_t tx_ch_idx)
{
	ARG_UNUSED(ctx);
	ARG_UNUSED(tx_ch_idx);
	return CN779_CH0_FREQ;
}

int region_get_max_payload(const struct region_ctx *ctx, uint8_t dr)
{
	ARG_UNUSED(ctx);
	if (dr >= CN779_NUM_DR) return -EINVAL;
	return max_payload[dr];
}

uint16_t region_get_rx_window_timeout_ms(const struct region_ctx *ctx, uint8_t dr)
{
	ARG_UNUSED(ctx);
	if (dr >= CN779_NUM_DR) return 3000;
	return dr_max_toa_ms[dr] + 50;
}

int region_apply_cflist(struct region_ctx *ctx, const uint8_t cflist[16])
{
	return region_cflist_type_a(ctx, cflist, 3, CN779_FREQ_MIN,
				    CN779_FREQ_MAX, CN779_NUM_DR, 8);
}
