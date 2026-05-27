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
#define US915_NUM_DR         14
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

static const struct region_dr us915_dr_table[US915_NUM_DR] = {
	{SF_10, BW_125_KHZ},  /* DR0 */
	{SF_9,  BW_125_KHZ},  /* DR1 */
	{SF_8,  BW_125_KHZ},  /* DR2 */
	{SF_7,  BW_125_KHZ},  /* DR3 */
	{SF_8,  BW_500_KHZ},  /* DR4 */
	{SF_8,  BW_500_KHZ},  /* DR5 (RFU) */
	{SF_8,  BW_500_KHZ},  /* DR6 (RFU) */
	{SF_8,  BW_500_KHZ},  /* DR7 (RFU) */
	{SF_12, BW_500_KHZ},  /* DR8 */
	{SF_11, BW_500_KHZ},  /* DR9 */
	{SF_10, BW_500_KHZ},  /* DR10 */
	{SF_9,  BW_500_KHZ},  /* DR11 */
	{SF_8,  BW_500_KHZ},  /* DR12 */
	{SF_7,  BW_500_KHZ},  /* DR13 */
};

static const uint16_t us915_dr_max_toa_ms[US915_NUM_DR] = {
	/* DR0  */  400,
	/* DR1  */  250,
	/* DR2  */  150,
	/* DR3  */  100,
	/* DR4  */   60,
	/* DR5  */  100,  /* RFU */
	/* DR6  */  100,  /* RFU */
	/* DR7  */  100,  /* RFU */
	/* DR8  */   30,
	/* DR9  */   20,
	/* DR10 */   20,
	/* DR11 */   20,
	/* DR12 */   20,
	/* DR13 */   20,
};

static const uint8_t us915_max_payload[US915_NUM_DR] = {
	/* DR0  */  19,
	/* DR1  */  61,
	/* DR2  */ 133,
	/* DR3  */ 250,
	/* DR4  */ 250,
	/* DR5  */   0,  /* RFU */
	/* DR6  */   0,  /* RFU */
	/* DR7  */   0,  /* RFU */
	/* DR8  */  61,
	/* DR9  */ 137,
	/* DR10 */ 250,
	/* DR11 */ 250,
	/* DR12 */ 250,
	/* DR13 */ 250,
};

static const uint8_t us915_rx1_dr_map[US915_NUM_DR][US915_NUM_DR] = {
	/* DR0  */ {10, 9, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
	/* DR1  */ {11, 10, 9, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
	/* DR2  */ {12, 11, 10, 9, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
	/* DR3  */ {13, 12, 11, 10, 9, 8, 8, 8, 8, 8, 8, 8, 8, 8},
	/* DR4  */ {13, 13, 12, 11, 10, 9, 8, 8, 8, 8, 8, 8, 8, 8},
	/* DR5  */ {13, 13, 13, 12, 11, 10, 9, 8, 8, 8, 8, 8, 8, 8},
	/* DR6  */ {13, 13, 13, 13, 12, 11, 10, 9, 8, 8, 8, 8, 8, 8},
	/* DR7  */ {13, 13, 13, 13, 13, 12, 11, 10, 9, 8, 8, 8, 8, 8},
	/* DR8  */ { 8,  8,  8,  8,  8,  8,  8,  8,  8, 8, 8, 8, 8, 8},
	/* DR9  */ { 9,  8,  8,  8,  8,  8,  8,  8,  8, 8, 8, 8, 8, 8},
	/* DR10 */ {10,  9,  8,  8,  8,  8,  8,  8,  8, 8, 8, 8, 8, 8},
	/* DR11 */ {11, 10,  9,  8,  8,  8,  8,  8,  8, 8, 8, 8, 8, 8},
	/* DR12 */ {12, 11, 10,  9,  8,  8,  8,  8,  8, 8, 8, 8, 8, 8},
	/* DR13 */ {13, 12, 11, 10,  9,  8,  8,  8,  8, 8, 8, 8, 8, 8},
};

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

static inline int us915_apply_cflist(struct region_ctx *ctx, const uint8_t cflist[16])
{
	return region_cflist_type_b(ctx, cflist);
}

#ifdef __cplusplus
}
#endif

#endif /* LORAWAN_REGION_US915_H_ */
