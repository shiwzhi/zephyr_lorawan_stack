/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 *
 * LoRaWAN IN865 region implementation.
 * Per LoRaWAN RP002-1.0.4 Regional Parameters.
 */

#include "region/region.h"
#include <errno.h>

#define IN865_NUM_CH         3
#define IN865_NUM_DR         6
#define IN865_RX2_FREQ      866550000U
#define IN865_RX2_DR         2
#define IN865_RX1_DELAY_S    1
#define IN865_RX2_DELAY_S    2
#define IN865_DEFAULT_TX_POWER 30
#define IN865_FREQ_MIN       865000000U
#define IN865_FREQ_MAX       867000000U
#define IN865_CH0_FREQ       865062500U

static const struct region_dr dr_table[IN865_NUM_DR] = {
	{SF_12, BW_125_KHZ}, {SF_11, BW_125_KHZ}, {SF_10, BW_125_KHZ},
	{SF_9,  BW_125_KHZ}, {SF_8,  BW_125_KHZ}, {SF_7,  BW_125_KHZ},
};

static const uint16_t dr_max_toa_ms[IN865_NUM_DR] = {
	1600, 900, 500, 300, 180, 120,
};

static const uint8_t max_payload[IN865_NUM_DR] = {
	59, 59, 59, 123, 230, 230,
};

static const uint8_t rx1_dr_map[IN865_NUM_DR][IN865_NUM_DR] = {
	{0, 0, 0, 0, 0, 0},
	{1, 0, 0, 0, 0, 0},
	{2, 1, 0, 0, 0, 0},
	{3, 2, 1, 0, 0, 0},
	{4, 3, 2, 1, 0, 0},
	{5, 4, 3, 2, 1, 0},
};

static const uint32_t ch_freq[IN865_NUM_CH] = {
	865062500U, 865402500U, 865985000U,
};

void region_init(struct region_ctx *ctx, enum lorawan_region region)
{
	memset(ctx, 0, sizeof(*ctx));
	ctx->region = LORAWAN_REGION_IN865;
	ctx->num_channels = IN865_NUM_CH;
	for (uint8_t i = 0; i < IN865_NUM_CH; i++) {
		ctx->channels[i].freq_hz = ch_freq[i];
		ctx->channels[i].min_dr = 0;
		ctx->channels[i].max_dr = IN865_NUM_DR - 1;
		ctx->channels[i].enabled = true;
	}
	ctx->num_dr = IN865_NUM_DR;
	for (uint8_t i = 0; i < IN865_NUM_DR; i++) {
		ctx->dr_table[i] = dr_table[i];
	}
	ctx->rx2_freq = IN865_RX2_FREQ;
	ctx->rx2_dr = IN865_RX2_DR;
	ctx->rx1_delay_s = IN865_RX1_DELAY_S;
	ctx->rx2_delay_s = IN865_RX2_DELAY_S;
	ctx->default_tx_power = IN865_DEFAULT_TX_POWER;
	region_init_tx_power_map(ctx);
	ctx->freq_min_hz = IN865_FREQ_MIN;
	ctx->freq_max_hz = IN865_FREQ_MAX;
	ctx->max_channels = IN865_NUM_CH;
	ctx->default_join_dr = 0;
	ctx->beacon_freq = 866550000U;
	ctx->rxc_freq = ctx->rx2_freq;
	ctx->rxc_dr = ctx->rx2_dr;
}

uint8_t region_get_rx1_dr(const struct region_ctx *ctx, uint8_t tx_dr,
			  uint8_t offset)
{
	ARG_UNUSED(ctx);
	if (tx_dr >= IN865_NUM_DR) tx_dr = IN865_NUM_DR - 1;
	if (offset <= 5) {
		if (offset >= IN865_NUM_DR) offset = IN865_NUM_DR - 1;
		return rx1_dr_map[tx_dr][offset];
	}
	int dr = (int)tx_dr;
	dr += (offset == 6) ? 1 : 2;
	if (dr >= IN865_NUM_DR) dr = IN865_NUM_DR - 1;
	return (uint8_t)dr;
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
	if (dr >= IN865_NUM_DR) return -EINVAL;
	return max_payload[dr];
}

uint16_t region_get_rx_window_timeout_ms(const struct region_ctx *ctx, uint8_t dr)
{
	ARG_UNUSED(ctx);
	if (dr >= IN865_NUM_DR) return 3000;
	return dr_max_toa_ms[dr] + 50;
}

int region_apply_cflist(struct region_ctx *ctx, const uint8_t cflist[16])
{
	return region_cflist_type_a(ctx, cflist, 3, IN865_FREQ_MIN,
				    IN865_FREQ_MAX, IN865_NUM_DR, 8);
}
