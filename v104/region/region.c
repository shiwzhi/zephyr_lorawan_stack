/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 *
 * Region abstraction layer.
 * Dispatches to region-specific implementations.
 */

#include "region.h"

#ifdef CONFIG_LORAWAN_CUSTOM_REGION_CN470
#include "region_cn470.h"
#endif
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_EU868
#include "region_eu868.h"
#endif
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_US915
#include "region_us915.h"
#endif
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_AU915
#include "region_au915.h"
#endif
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_KR920
#include "region_kr920.h"
#endif
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_IN865
#include "region_in865.h"
#endif
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_AS923
#include "region_as923.h"
#endif
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_CN779
#include "region_cn779.h"
#endif
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_RU864
#include "region_ru864.h"
#endif

#include <string.h>
#include <zephyr/random/random.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(region, CONFIG_LOG_DEFAULT_LEVEL);

/* -------------------------------------------------------------------------- */
/* Internal helpers                                                           */
/* -------------------------------------------------------------------------- */

static void init_channels(struct region_ctx *ctx, uint8_t num_ch,
			  const uint32_t *freqs, uint8_t min_dr, uint8_t max_dr)
{
	ctx->num_channels = num_ch;
	for (uint8_t i = 0; i < num_ch; i++) {
		ctx->channels[i].freq_hz = freqs[i];
		ctx->channels[i].min_dr = min_dr;
		ctx->channels[i].max_dr = max_dr;
		ctx->channels[i].enabled = true;
	}
}

static void init_dr_table(struct region_ctx *ctx, uint8_t num_dr,
			  const struct region_dr *table)
{
	ctx->num_dr = num_dr;
	for (uint8_t i = 0; i < num_dr; i++) {
		ctx->dr_table[i] = table[i];
	}
}

static void init_tx_power_map(struct region_ctx *ctx)
{
	for (uint8_t i = 0; i < 8; i++) {
		int p = (int)ctx->default_tx_power - 2 * (int)i;
		ctx->tx_power_map[i] = (p < 0) ? 0 : (uint8_t)p;
	}
}

#ifdef CONFIG_LORAWAN_CUSTOM_REGION_EU868
static void region_setup_eu868(struct region_ctx *ctx)
{
	ctx->region = LORAWAN_REGION_EU868;
	init_channels(ctx, EU868_NUM_CH, eu868_ch_freq, 0, EU868_NUM_DR - 1);
	init_dr_table(ctx, EU868_NUM_DR, eu868_dr_table);
	ctx->rx2_freq = EU868_RX2_FREQ;
	ctx->rx2_dr = EU868_RX2_DR;
	ctx->rx1_delay_s = EU868_RX1_DELAY_S;
	ctx->rx2_delay_s = EU868_RX2_DELAY_S;
	ctx->default_tx_power = EU868_DEFAULT_TX_POWER;
	init_tx_power_map(ctx);
	ctx->max_tx_duty_cycle = EU868_TX_MAX_DUTY;
	ctx->freq_min_hz = EU868_FREQ_MIN;
	ctx->freq_max_hz = EU868_FREQ_MAX;
	ctx->max_channels = EU868_NUM_CH;
}
#endif

#ifdef CONFIG_LORAWAN_CUSTOM_REGION_US915
static void region_setup_us915(struct region_ctx *ctx)
{
	uint32_t freqs[US915_NUM_UP_CH];

	ctx->region = LORAWAN_REGION_US915;
	for (uint8_t i = 0; i < US915_NUM_UP_CH; i++) {
		freqs[i] = us915_get_up_ch_freq(i);
	}
	init_channels(ctx, US915_NUM_UP_CH, freqs, 0, 4);
	for (uint8_t i = US915_NUM_125K_CH; i < US915_NUM_UP_CH; i++) {
		ctx->channels[i].max_dr = 9;
	}
	init_dr_table(ctx, US915_NUM_DR, us915_dr_table);
	ctx->rx2_freq = US915_RX2_FREQ;
	ctx->rx2_dr = US915_RX2_DR;
	ctx->rx1_delay_s = US915_RX1_DELAY_S;
	ctx->rx2_delay_s = US915_RX2_DELAY_S;
	ctx->default_tx_power = US915_DEFAULT_TX_POWER;
	init_tx_power_map(ctx);
	ctx->max_tx_duty_cycle = 0;
	ctx->freq_min_hz = US915_FREQ_MIN;
	ctx->freq_max_hz = US915_FREQ_MAX;
	ctx->max_channels = US915_NUM_UP_CH;
}
#endif

#ifdef CONFIG_LORAWAN_CUSTOM_REGION_AU915
static void region_setup_au915(struct region_ctx *ctx)
{
	uint32_t freqs[AU915_NUM_UP_CH];

	ctx->region = LORAWAN_REGION_AU915;
	for (uint8_t i = 0; i < AU915_NUM_UP_CH; i++) {
		freqs[i] = au915_get_up_ch_freq(i);
	}
	init_channels(ctx, AU915_NUM_UP_CH, freqs, 0, 5);
	for (uint8_t i = AU915_NUM_125K_CH; i < AU915_NUM_UP_CH; i++) {
		ctx->channels[i].max_dr = 9;
	}
	init_dr_table(ctx, AU915_NUM_DR, au915_dr_table);
	ctx->rx2_freq = AU915_RX2_FREQ;
	ctx->rx2_dr = AU915_RX2_DR;
	ctx->rx1_delay_s = AU915_RX1_DELAY_S;
	ctx->rx2_delay_s = AU915_RX2_DELAY_S;
	ctx->default_tx_power = AU915_DEFAULT_TX_POWER;
	init_tx_power_map(ctx);
	ctx->max_tx_duty_cycle = 0;
	ctx->freq_min_hz = AU915_FREQ_MIN;
	ctx->freq_max_hz = AU915_FREQ_MAX;
	ctx->max_channels = AU915_NUM_UP_CH;
}
#endif

#ifdef CONFIG_LORAWAN_CUSTOM_REGION_CN470
static void region_setup_cn470(struct region_ctx *ctx)
{
	uint32_t freqs[CN470_NUM_UP_CH];

	ctx->region = LORAWAN_REGION_CN470;
	for (uint8_t i = 0; i < CN470_NUM_UP_CH; i++) {
		freqs[i] = CN470_UP_FREQ_START + (i * CN470_UP_FREQ_STEP);
	}
	init_channels(ctx, CN470_NUM_UP_CH, freqs, 0, CN470_NUM_DR - 1);
	init_dr_table(ctx, CN470_NUM_DR, cn470_dr_table);
	ctx->rx2_freq = CN470_RX2_FREQ;
	ctx->rx2_dr = CN470_RX2_DR;
	ctx->rx1_delay_s = CN470_RX1_DELAY_S;
	ctx->rx2_delay_s = CN470_RX2_DELAY_S;
	ctx->default_tx_power = CN470_DEFAULT_TX_POWER;
	init_tx_power_map(ctx);
	ctx->max_tx_duty_cycle = 0;
	ctx->freq_min_hz = CN470_FREQ_MIN;
	ctx->freq_max_hz = CN470_FREQ_MAX;
	ctx->max_channels = CN470_NUM_UP_CH;
}
#endif

#ifdef CONFIG_LORAWAN_CUSTOM_REGION_KR920
static void region_setup_kr920(struct region_ctx *ctx)
{
	ctx->region = LORAWAN_REGION_KR920;
	init_channels(ctx, KR920_NUM_CH, kr920_ch_freq, 0, KR920_NUM_DR - 1);
	init_dr_table(ctx, KR920_NUM_DR, kr920_dr_table);
	ctx->rx2_freq = KR920_RX2_FREQ;
	ctx->rx2_dr = KR920_RX2_DR;
	ctx->rx1_delay_s = KR920_RX1_DELAY_S;
	ctx->rx2_delay_s = KR920_RX2_DELAY_S;
	ctx->default_tx_power = KR920_DEFAULT_TX_POWER;
	init_tx_power_map(ctx);
	ctx->max_tx_duty_cycle = KR920_TX_MAX_DUTY;
	ctx->freq_min_hz = KR920_FREQ_MIN;
	ctx->freq_max_hz = KR920_FREQ_MAX;
	ctx->max_channels = KR920_NUM_CH;
}
#endif

#ifdef CONFIG_LORAWAN_CUSTOM_REGION_IN865
static void region_setup_in865(struct region_ctx *ctx)
{
	ctx->region = LORAWAN_REGION_IN865;
	init_channels(ctx, IN865_NUM_CH, in865_ch_freq, 0, IN865_NUM_DR - 1);
	init_dr_table(ctx, IN865_NUM_DR, in865_dr_table);
	ctx->rx2_freq = IN865_RX2_FREQ;
	ctx->rx2_dr = IN865_RX2_DR;
	ctx->rx1_delay_s = IN865_RX1_DELAY_S;
	ctx->rx2_delay_s = IN865_RX2_DELAY_S;
	ctx->default_tx_power = IN865_DEFAULT_TX_POWER;
	init_tx_power_map(ctx);
	ctx->max_tx_duty_cycle = 0;
	ctx->freq_min_hz = IN865_FREQ_MIN;
	ctx->freq_max_hz = IN865_FREQ_MAX;
	ctx->max_channels = IN865_NUM_CH;
}
#endif

#ifdef CONFIG_LORAWAN_CUSTOM_REGION_AS923
static void region_setup_as923(struct region_ctx *ctx)
{
	ctx->region = LORAWAN_REGION_AS923;
	init_channels(ctx, AS923_NUM_CH, as923_ch_freq, 0, AS923_NUM_DR - 1);
	init_dr_table(ctx, AS923_NUM_DR, as923_dr_table);
	ctx->rx2_freq = AS923_RX2_FREQ;
	ctx->rx2_dr = AS923_RX2_DR;
	ctx->rx1_delay_s = AS923_RX1_DELAY_S;
	ctx->rx2_delay_s = AS923_RX2_DELAY_S;
	ctx->default_tx_power = AS923_DEFAULT_TX_POWER;
	init_tx_power_map(ctx);
	ctx->max_tx_duty_cycle = 0;
	ctx->freq_min_hz = AS923_FREQ_MIN;
	ctx->freq_max_hz = AS923_FREQ_MAX;
	ctx->max_channels = AS923_NUM_CH;
}
#endif

#ifdef CONFIG_LORAWAN_CUSTOM_REGION_CN779
static void region_setup_cn779(struct region_ctx *ctx)
{
	ctx->region = LORAWAN_REGION_CN779;
	init_channels(ctx, CN779_NUM_CH, cn779_ch_freq, 0, CN779_NUM_DR - 1);
	init_dr_table(ctx, CN779_NUM_DR, cn779_dr_table);
	ctx->rx2_freq = CN779_RX2_FREQ;
	ctx->rx2_dr = CN779_RX2_DR;
	ctx->rx1_delay_s = CN779_RX1_DELAY_S;
	ctx->rx2_delay_s = CN779_RX2_DELAY_S;
	ctx->default_tx_power = CN779_DEFAULT_TX_POWER;
	init_tx_power_map(ctx);
	ctx->max_tx_duty_cycle = 0;
	ctx->freq_min_hz = CN779_FREQ_MIN;
	ctx->freq_max_hz = CN779_FREQ_MAX;
	ctx->max_channels = CN779_NUM_CH;
}
#endif

#ifdef CONFIG_LORAWAN_CUSTOM_REGION_RU864
static void region_setup_ru864(struct region_ctx *ctx)
{
	ctx->region = LORAWAN_REGION_RU864;
	init_channels(ctx, RU864_NUM_CH, ru864_ch_freq, 0, RU864_NUM_DR - 1);
	init_dr_table(ctx, RU864_NUM_DR, ru864_dr_table);
	ctx->rx2_freq = RU864_RX2_FREQ;
	ctx->rx2_dr = RU864_RX2_DR;
	ctx->rx1_delay_s = RU864_RX1_DELAY_S;
	ctx->rx2_delay_s = RU864_RX2_DELAY_S;
	ctx->default_tx_power = RU864_DEFAULT_TX_POWER;
	init_tx_power_map(ctx);
	ctx->max_tx_duty_cycle = RU864_TX_MAX_DUTY;
	ctx->freq_min_hz = RU864_FREQ_MIN;
	ctx->freq_max_hz = RU864_FREQ_MAX;
	ctx->max_channels = RU864_NUM_CH;
}
#endif

/* -------------------------------------------------------------------------- */
/* Public API                                                                 */
/* -------------------------------------------------------------------------- */
void region_init(struct region_ctx *ctx, enum lorawan_region region)
{
	memset(ctx, 0, sizeof(*ctx));

	switch (region) {
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_EU868
	case LORAWAN_REGION_EU868:
		region_setup_eu868(ctx);
		break;
#endif
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_US915
	case LORAWAN_REGION_US915:
		region_setup_us915(ctx);
		break;
#endif
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_AU915
	case LORAWAN_REGION_AU915:
		region_setup_au915(ctx);
		break;
#endif
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_CN470
	case LORAWAN_REGION_CN470:
		region_setup_cn470(ctx);
		break;
#endif
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_KR920
	case LORAWAN_REGION_KR920:
		region_setup_kr920(ctx);
		break;
#endif
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_IN865
	case LORAWAN_REGION_IN865:
		region_setup_in865(ctx);
		break;
#endif
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_AS923
	case LORAWAN_REGION_AS923:
		region_setup_as923(ctx);
		break;
#endif
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_CN779
	case LORAWAN_REGION_CN779:
		region_setup_cn779(ctx);
		break;
#endif
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_RU864
	case LORAWAN_REGION_RU864:
		region_setup_ru864(ctx);
		break;
#endif
	default:
		LOG_ERR("Unsupported region: %d", region);
		break;
	}

	ctx->rxc_freq = ctx->rx2_freq;
	ctx->rxc_dr = ctx->rx2_dr;
}

int region_dr_to_lora(const struct region_ctx *ctx, uint8_t dr,
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

uint8_t region_get_rx1_dr(const struct region_ctx *ctx, uint8_t tx_dr,
			  uint8_t offset)
{
	uint8_t dr;

	switch (ctx->region) {
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_CN470
	case LORAWAN_REGION_CN470:
		dr = cn470_get_rx1_dr(tx_dr, offset);
		break;
#endif
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_EU868
	case LORAWAN_REGION_EU868:
		dr = eu868_get_rx1_dr(tx_dr, offset);
		break;
#endif
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_US915
	case LORAWAN_REGION_US915:
		dr = us915_get_rx1_dr(tx_dr, offset);
		break;
#endif
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_AU915
	case LORAWAN_REGION_AU915:
		dr = au915_get_rx1_dr(tx_dr, offset);
		break;
#endif
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_KR920
	case LORAWAN_REGION_KR920:
		dr = kr920_get_rx1_dr(tx_dr, offset);
		break;
#endif
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_IN865
	case LORAWAN_REGION_IN865:
		dr = in865_get_rx1_dr(tx_dr, offset);
		break;
#endif
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_AS923
	case LORAWAN_REGION_AS923:
		dr = as923_get_rx1_dr(tx_dr, offset);
		break;
#endif
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_CN779
	case LORAWAN_REGION_CN779:
		dr = cn779_get_rx1_dr(tx_dr, offset);
		break;
#endif
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_RU864
	case LORAWAN_REGION_RU864:
		dr = ru864_get_rx1_dr(tx_dr, offset);
		break;
#endif
	default:
		return 0;
	}

	if (dr >= ctx->num_dr) {
		dr = ctx->num_dr - 1;
	}

	return dr;
}

uint32_t region_get_rx1_freq(const struct region_ctx *ctx,
			       uint8_t tx_ch_idx)
{
	switch (ctx->region) {
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_CN470
	case LORAWAN_REGION_CN470:
		return cn470_get_rx1_freq(tx_ch_idx);
#endif
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_US915
	case LORAWAN_REGION_US915:
		return us915_get_rx1_freq(tx_ch_idx);
#endif
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_AU915
	case LORAWAN_REGION_AU915:
		return au915_get_rx1_freq(tx_ch_idx);
#endif
	default:
		return ctx->channels[tx_ch_idx % ctx->num_channels].freq_hz;
	}
}

const struct lorawan_channel *region_get_join_channel(const struct region_ctx *ctx)
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

int region_get_max_payload(const struct region_ctx *ctx, uint8_t dr)
{
	switch (ctx->region) {
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_CN470
	case LORAWAN_REGION_CN470:
		return cn470_get_max_payload(dr);
#endif
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_EU868
	case LORAWAN_REGION_EU868:
		return eu868_get_max_payload(dr);
#endif
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_US915
	case LORAWAN_REGION_US915:
		return us915_get_max_payload(dr);
#endif
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_AU915
	case LORAWAN_REGION_AU915:
		return au915_get_max_payload(dr);
#endif
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_KR920
	case LORAWAN_REGION_KR920:
		return kr920_get_max_payload(dr);
#endif
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_IN865
	case LORAWAN_REGION_IN865:
		return in865_get_max_payload(dr);
#endif
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_AS923
	case LORAWAN_REGION_AS923:
		return as923_get_max_payload(dr);
#endif
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_CN779
	case LORAWAN_REGION_CN779:
		return cn779_get_max_payload(dr);
#endif
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_RU864
	case LORAWAN_REGION_RU864:
		return ru864_get_max_payload(dr);
#endif
	default:
		if (dr >= LORAWAN_MAX_DR) {
			return -EINVAL;
		}
		return 50;
	}
}

uint32_t region_get_beacon_freq(uint32_t beacon_time)
{
	ARG_UNUSED(beacon_time);
	return 869525000U;
}

uint16_t region_get_rx_window_timeout_ms(const struct region_ctx *ctx, uint8_t dr)
{
	switch (ctx->region) {
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_CN470
	case LORAWAN_REGION_CN470:
		return cn470_get_rx_window_timeout_ms(dr);
#endif
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_EU868
	case LORAWAN_REGION_EU868:
		return eu868_get_rx_window_timeout_ms(dr);
#endif
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_US915
	case LORAWAN_REGION_US915:
		return us915_get_rx_window_timeout_ms(dr);
#endif
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_AU915
	case LORAWAN_REGION_AU915:
		return au915_get_rx_window_timeout_ms(dr);
#endif
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_KR920
	case LORAWAN_REGION_KR920:
		return kr920_get_rx_window_timeout_ms(dr);
#endif
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_IN865
	case LORAWAN_REGION_IN865:
		return in865_get_rx_window_timeout_ms(dr);
#endif
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_AS923
	case LORAWAN_REGION_AS923:
		return as923_get_rx_window_timeout_ms(dr);
#endif
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_CN779
	case LORAWAN_REGION_CN779:
		return cn779_get_rx_window_timeout_ms(dr);
#endif
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_RU864
	case LORAWAN_REGION_RU864:
		return ru864_get_rx_window_timeout_ms(dr);
#endif
	default:
		if (dr >= LORAWAN_MAX_DR) {
			return 3000;
		}
		return 1550;
	}
}

int region_apply_cflist(struct region_ctx *ctx, const uint8_t cflist[16])
{
	switch (ctx->region) {
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_CN470
	case LORAWAN_REGION_CN470:
		return cn470_apply_cflist(ctx, cflist);
#endif
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_EU868
	case LORAWAN_REGION_EU868:
		return eu868_apply_cflist(ctx, cflist);
#endif
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_US915
	case LORAWAN_REGION_US915:
		return us915_apply_cflist(ctx, cflist);
#endif
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_AU915
	case LORAWAN_REGION_AU915:
		return au915_apply_cflist(ctx, cflist);
#endif
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_KR920
	case LORAWAN_REGION_KR920:
		return kr920_apply_cflist(ctx, cflist);
#endif
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_IN865
	case LORAWAN_REGION_IN865:
		return in865_apply_cflist(ctx, cflist);
#endif
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_AS923
	case LORAWAN_REGION_AS923:
		return as923_apply_cflist(ctx, cflist);
#endif
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_CN779
	case LORAWAN_REGION_CN779:
		return cn779_apply_cflist(ctx, cflist);
#endif
#ifdef CONFIG_LORAWAN_CUSTOM_REGION_RU864
	case LORAWAN_REGION_RU864:
		return ru864_apply_cflist(ctx, cflist);
#endif
	default:
		return -EINVAL;
	}
}

int region_cflist_type_a(struct region_ctx *ctx, const uint8_t cflist[16],
			  uint8_t start_idx, uint32_t freq_min, uint32_t freq_max,
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

int region_cflist_type_b(struct region_ctx *ctx, const uint8_t cflist[16])
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
