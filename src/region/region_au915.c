/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 *
 * LoRaWAN AU915 region implementation.
 * Per LoRaWAN RP002-1.0.4 Regional Parameters.
 * 64 x 125kHz + 8 x 500kHz uplink channels.
 */

#include "region/region.h"
#include <errno.h>

#define AU915_NUM_UP_CH      72
#define AU915_NUM_125K_CH    64
#define AU915_NUM_DOWN_CH    8
#define AU915_NUM_DR         14
#define AU915_RX2_FREQ      923300000U
#define AU915_RX2_DR         8
#define AU915_RX1_DELAY_S    1
#define AU915_RX2_DELAY_S    2
#define AU915_DEFAULT_TX_POWER 30
#define AU915_FREQ_MIN       915000000U
#define AU915_FREQ_MAX       928000000U
#define AU915_UP_FREQ_START  915200000U
#define AU915_UP_FREQ_STEP   200000U
#define AU915_500K_FREQ_START 915900000U
#define AU915_500K_FREQ_STEP 1600000U
#define AU915_DOWN_FREQ_START 923300000U
#define AU915_DOWN_FREQ_STEP  600000U

static const struct region_dr dr_table[AU915_NUM_DR] = {
	{SF_10, BW_125_KHZ}, {SF_9,  BW_125_KHZ}, {SF_8,  BW_125_KHZ},
	{SF_7,  BW_125_KHZ}, {SF_8,  BW_500_KHZ}, {SF_8,  BW_500_KHZ},
	{SF_8,  BW_500_KHZ}, {SF_8,  BW_500_KHZ}, {SF_12, BW_500_KHZ},
	{SF_11, BW_500_KHZ}, {SF_10, BW_500_KHZ}, {SF_9,  BW_500_KHZ},
	{SF_8,  BW_500_KHZ}, {SF_7,  BW_500_KHZ},
};

static const uint16_t dr_max_toa_ms[AU915_NUM_DR] = {
	400, 250, 150, 100, 60, 100, 100, 100,
	30, 20, 20, 20, 20, 20,
};

static const uint8_t max_payload[AU915_NUM_DR] = {
	19, 61, 133, 250, 250, 0, 0, 0,
	61, 137, 250, 250, 250, 250,
};

static const uint8_t rx1_dr_map[AU915_NUM_DR][AU915_NUM_DR] = {
	{10, 9, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
	{11, 10, 9, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
	{12, 11, 10, 9, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
	{13, 12, 11, 10, 9, 8, 8, 8, 8, 8, 8, 8, 8, 8},
	{13, 13, 12, 11, 10, 9, 8, 8, 8, 8, 8, 8, 8, 8},
	{13, 13, 13, 12, 11, 10, 9, 8, 8, 8, 8, 8, 8, 8},
	{13, 13, 13, 13, 12, 11, 10, 9, 8, 8, 8, 8, 8, 8},
	{13, 13, 13, 13, 13, 12, 11, 10, 9, 8, 8, 8, 8, 8},
	{ 8,  8,  8,  8,  8,  8,  8,  8,  8, 8, 8, 8, 8, 8},
	{ 9,  8,  8,  8,  8,  8,  8,  8,  8, 8, 8, 8, 8, 8},
	{10,  9,  8,  8,  8,  8,  8,  8,  8, 8, 8, 8, 8, 8},
	{11, 10,  9,  8,  8,  8,  8,  8,  8, 8, 8, 8, 8, 8},
	{12, 11, 10,  9,  8,  8,  8,  8,  8, 8, 8, 8, 8, 8},
	{13, 12, 11, 10,  9,  8,  8,  8,  8, 8, 8, 8, 8, 8},
};

static uint32_t get_up_ch_freq(uint8_t ch_idx)
{
	if (ch_idx < AU915_NUM_125K_CH)
		return AU915_UP_FREQ_START + ch_idx * AU915_UP_FREQ_STEP;
	return AU915_500K_FREQ_START + (ch_idx - AU915_NUM_125K_CH) * AU915_500K_FREQ_STEP;
}

void region_init(struct region_ctx *ctx, enum lorawan_region region)
{
	uint32_t freqs[AU915_NUM_UP_CH];

	memset(ctx, 0, sizeof(*ctx));
	ctx->region = LORAWAN_REGION_AU915;
	for (uint8_t i = 0; i < AU915_NUM_UP_CH; i++) {
		freqs[i] = get_up_ch_freq(i);
	}
	ctx->num_channels = AU915_NUM_UP_CH;
	for (uint8_t i = 0; i < AU915_NUM_UP_CH; i++) {
		ctx->channels[i].freq_hz = freqs[i];
		ctx->channels[i].min_dr = 0;
		ctx->channels[i].max_dr = (i < AU915_NUM_125K_CH) ? 4 : 9;
		ctx->channels[i].enabled = true;
	}
	ctx->num_dr = AU915_NUM_DR;
	for (uint8_t i = 0; i < AU915_NUM_DR; i++) {
		ctx->dr_table[i] = dr_table[i];
	}
	ctx->rx2_freq = AU915_RX2_FREQ;
	ctx->rx2_dr = AU915_RX2_DR;
	ctx->rx1_delay_s = AU915_RX1_DELAY_S;
	ctx->rx2_delay_s = AU915_RX2_DELAY_S;
	ctx->default_tx_power = AU915_DEFAULT_TX_POWER;
	region_init_tx_power_map(ctx);
	ctx->freq_min_hz = AU915_FREQ_MIN;
	ctx->freq_max_hz = AU915_FREQ_MAX;
	ctx->max_channels = AU915_NUM_UP_CH;
	ctx->default_join_dr = 2;
	ctx->beacon_freq = 923300000U;
	ctx->rxc_freq = ctx->rx2_freq;
	ctx->rxc_dr = ctx->rx2_dr;
}

uint8_t region_get_rx1_dr(const struct region_ctx *ctx, uint8_t tx_dr,
			  uint8_t offset)
{
	ARG_UNUSED(ctx);
	if (tx_dr >= AU915_NUM_DR) tx_dr = AU915_NUM_DR - 1;
	if (offset >= AU915_NUM_DR) offset = AU915_NUM_DR - 1;
	return rx1_dr_map[tx_dr][offset];
}

uint32_t region_get_rx1_freq(const struct region_ctx *ctx, uint8_t tx_ch_idx)
{
	ARG_UNUSED(ctx);
	uint8_t rx_ch;
	if (tx_ch_idx < AU915_NUM_125K_CH)
		rx_ch = tx_ch_idx % AU915_NUM_DOWN_CH;
	else
		rx_ch = tx_ch_idx - AU915_NUM_125K_CH;
	return AU915_DOWN_FREQ_START + rx_ch * AU915_DOWN_FREQ_STEP;
}

int region_get_max_payload(const struct region_ctx *ctx, uint8_t dr)
{
	ARG_UNUSED(ctx);
	if (dr >= AU915_NUM_DR) return -EINVAL;
	return max_payload[dr];
}

uint16_t region_get_rx_window_timeout_ms(const struct region_ctx *ctx, uint8_t dr)
{
	ARG_UNUSED(ctx);
	if (dr >= AU915_NUM_DR) return 3000;
	return dr_max_toa_ms[dr] + 50;
}

int region_apply_cflist(struct region_ctx *ctx, const uint8_t cflist[16])
{
	return region_cflist_type_b(ctx, cflist);
}
