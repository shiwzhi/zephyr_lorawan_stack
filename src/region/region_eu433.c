/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 *
 * LoRaWAN EU433 region implementation.
 * Per LoRaWAN 1.0.3 Regional Parameters v1.0.3 Rev A.
 */

#include "region/region.h"
#include <errno.h>

#define EU433_NUM_CH         3
#define EU433_NUM_DR         7
#define EU433_RX2_FREQ      434665000U
#define EU433_RX2_DR         0
#define EU433_RX1_DELAY_S    1
#define EU433_RX2_DELAY_S    2
#define EU433_TX_MAX_DUTY    100
#define EU433_DEFAULT_TX_POWER 12
#define EU433_FREQ_MIN       433050000U
#define EU433_FREQ_MAX       434790000U
#define EU433_CH0_FREQ       433175000U

static const struct region_dr dr_table[EU433_NUM_DR] = {
	{SF_12, BW_125_KHZ}, {SF_11, BW_125_KHZ}, {SF_10, BW_125_KHZ},
	{SF_9,  BW_125_KHZ}, {SF_8,  BW_125_KHZ}, {SF_7,  BW_125_KHZ},
	{SF_7,  BW_250_KHZ},
};

static const uint16_t dr_max_toa_ms[EU433_NUM_DR] = {
	1600, 900, 500, 300, 180, 120, 60,
};

static const uint8_t max_payload[EU433_NUM_DR] = {
	59, 59, 59, 123, 230, 230, 230,
};

static const uint8_t rx1_dr_map[EU433_NUM_DR][EU433_NUM_DR] = {
	{0, 0, 0, 0, 0, 0, 0},
	{1, 0, 0, 0, 0, 0, 0},
	{2, 1, 0, 0, 0, 0, 0},
	{3, 2, 1, 0, 0, 0, 0},
	{4, 3, 2, 1, 0, 0, 0},
	{5, 4, 3, 2, 1, 0, 0},
	{6, 5, 4, 3, 2, 1, 0},
};

static const uint32_t ch_freq[EU433_NUM_CH] = {
	433175000U, 433375000U, 433575000U,
};

void region_init(struct region_ctx *ctx, enum lorawan_region region)
{
	memset(ctx, 0, sizeof(*ctx));
	ctx->region = LORAWAN_REGION_EU433;
	ctx->num_channels = EU433_NUM_CH;
	for (uint8_t i = 0; i < EU433_NUM_CH; i++) {
		ctx->channels[i].freq_hz = ch_freq[i];
		ctx->channels[i].min_dr = 0;
		ctx->channels[i].max_dr = EU433_NUM_DR - 1;
		ctx->channels[i].enabled = true;
	}
	ctx->num_dr = EU433_NUM_DR;
	for (uint8_t i = 0; i < EU433_NUM_DR; i++) {
		ctx->dr_table[i] = dr_table[i];
	}
	ctx->rx2_freq = EU433_RX2_FREQ;
	ctx->rx2_dr = EU433_RX2_DR;
	ctx->rx1_delay_s = EU433_RX1_DELAY_S;
	ctx->rx2_delay_s = EU433_RX2_DELAY_S;
	ctx->default_tx_power = EU433_DEFAULT_TX_POWER;
	region_init_tx_power_map(ctx);
	ctx->max_tx_duty_cycle = EU433_TX_MAX_DUTY;
	ctx->freq_min_hz = EU433_FREQ_MIN;
	ctx->freq_max_hz = EU433_FREQ_MAX;
	ctx->max_channels = 16;
	ctx->default_join_dr = 0;
	ctx->beacon_freq = 434665000U;
	ctx->rxc_freq = ctx->rx2_freq;
	ctx->rxc_dr = ctx->rx2_dr;
}

uint8_t region_get_rx1_dr(const struct region_ctx *ctx, uint8_t tx_dr,
			  uint8_t offset)
{
	ARG_UNUSED(ctx);
	if (tx_dr >= EU433_NUM_DR) tx_dr = EU433_NUM_DR - 1;
	if (offset >= EU433_NUM_DR) offset = EU433_NUM_DR - 1;
	return rx1_dr_map[tx_dr][offset];
}

uint32_t region_get_rx1_freq(const struct region_ctx *ctx, uint8_t tx_ch_idx)
{
	ARG_UNUSED(ctx);
	ARG_UNUSED(tx_ch_idx);
	return EU433_CH0_FREQ;
}

int region_get_max_payload(const struct region_ctx *ctx, uint8_t dr)
{
	ARG_UNUSED(ctx);
	if (dr >= EU433_NUM_DR) return -EINVAL;
	return max_payload[dr];
}

uint16_t region_get_rx_window_timeout_ms(const struct region_ctx *ctx, uint8_t dr)
{
	ARG_UNUSED(ctx);
	if (dr >= EU433_NUM_DR) return 3000;
	return dr_max_toa_ms[dr] + 50;
}

int region_apply_cflist(struct region_ctx *ctx, const uint8_t cflist[16])
{
	return region_cflist_type_a(ctx, cflist, 3, EU433_FREQ_MIN,
				    EU433_FREQ_MAX, EU433_NUM_DR, 8);
}
