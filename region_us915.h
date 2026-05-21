/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 *
 * LoRaWAN US915 region-specific parameters and functions.
 * Per LoRaWAN RP002-1.0.4 Regional Parameters.
 *
 * US915 uses 64 x 125kHz + 8 x 500kHz uplink channels.
 * Downlink uses 8 x 500kHz channels at 923.3 MHz.
 * RX1 channel = uplink channel modulo 8 (for 125kHz) or modulo 8 (for 500kHz).
 */

#ifndef LORAWAN_REGION_US915_H_
#define LORAWAN_REGION_US915_H_

#include "region.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define US915_NUM_UP_CH      72   /* 64 x 125kHz + 8 x 500kHz */
#define US915_NUM_125K_CH    64
#define US915_NUM_500K_CH    8
#define US915_NUM_DOWN_CH    8
#define US915_NUM_DR         10
#define US915_RX2_FREQ      923300000U
#define US915_RX2_DR         8
#define US915_RX1_DELAY_S    1
#define US915_RX2_DELAY_S    2
#define US915_DEFAULT_TX_POWER 20

/* US915 frequency band limits (FCC) */
#define US915_FREQ_MIN       902000000U
#define US915_FREQ_MAX       928000000U

/* US915 uplink frequency ranges */
#define US915_UP_FREQ_START  902300000U
#define US915_UP_FREQ_STEP   200000U
#define US915_500K_FREQ_START 903000000U
#define US915_500K_FREQ_STEP 1600000U

/* US915 downlink frequency range */
#define US915_DOWN_FREQ_START 923300000U
#define US915_DOWN_FREQ_STEP  600000U

extern const struct region_dr us915_dr_table[US915_NUM_DR];
extern const uint16_t us915_dr_max_toa_ms[US915_NUM_DR];
extern const uint8_t us915_max_payload[US915_NUM_DR];
extern const uint8_t us915_rx1_dr_map[US915_NUM_DR][US915_NUM_DR];

static inline uint32_t us915_get_up_ch_freq(uint8_t ch_idx)
{
	if (ch_idx < US915_NUM_125K_CH) {
		return US915_UP_FREQ_START + ch_idx * US915_UP_FREQ_STEP;
	}
	/* 500kHz channels: 903.0 + (ch_idx - 64) * 1.6 MHz */
	return US915_500K_FREQ_START + (ch_idx - US915_NUM_125K_CH) * US915_500K_FREQ_STEP;
}

static inline uint32_t us915_get_rx1_freq(uint8_t tx_ch_idx)
{
	/*
	 * US915 RX1 channel mapping:
	 * For 125kHz uplink (ch 0-63): RX1 = 923.3 + (ch % 8) * 0.6 MHz
	 * For 500kHz uplink (ch 64-71): RX1 = 923.3 + (ch - 64) * 0.6 MHz
	 */
	uint8_t rx_ch;
	if (tx_ch_idx < US915_NUM_125K_CH) {
		rx_ch = tx_ch_idx % US915_NUM_DOWN_CH;
	} else {
		rx_ch = tx_ch_idx - US915_NUM_125K_CH;
	}
	return US915_DOWN_FREQ_START + rx_ch * US915_DOWN_FREQ_STEP;
}

static inline uint8_t us915_get_rx1_dr(uint8_t tx_dr, uint8_t offset)
{
	if (tx_dr >= US915_NUM_DR) {
		tx_dr = US915_NUM_DR - 1;
	}
	if (offset >= US915_NUM_DR) {
		offset = US915_NUM_DR - 1;
	}
	return us915_rx1_dr_map[tx_dr][offset];
}

static inline uint16_t us915_get_rx_window_timeout_ms(uint8_t dr)
{
	if (dr >= US915_NUM_DR) {
		return 3000;
	}
	return us915_dr_max_toa_ms[dr] + 50;
}

static inline int us915_get_max_payload(uint8_t dr)
{
	if (dr >= US915_NUM_DR) {
		return -EINVAL;
	}
	return us915_max_payload[dr];
}

#ifdef __cplusplus
}
#endif

#endif /* LORAWAN_REGION_US915_H_ */
