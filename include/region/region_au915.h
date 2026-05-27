/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 *
 * LoRaWAN AU915 region-specific parameters and functions.
 * Per LoRaWAN RP002-1.0.4 Regional Parameters.
 *
 * AU915 uses 64 x 125kHz + 8 x 500kHz uplink channels.
 * Downlink uses 8 x 500kHz channels at 923.3 MHz.
 * RX1 channel = uplink channel modulo 8.
 */

#ifndef LORAWAN_REGION_AU915_H_
#define LORAWAN_REGION_AU915_H_

#include "region.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AU915_NUM_UP_CH      72   /* 64 x 125kHz + 8 x 500kHz */
#define AU915_NUM_125K_CH    64
#define AU915_NUM_500K_CH    8
#define AU915_NUM_DOWN_CH    8
#define AU915_NUM_DR         14
#define AU915_RX2_FREQ      923300000U
#define AU915_RX2_DR         8
#define AU915_RX1_DELAY_S    1
#define AU915_RX2_DELAY_S    2
#define AU915_DEFAULT_TX_POWER 30

/* AU915 frequency band limits (ACMA) */
#define AU915_FREQ_MIN       915000000U
#define AU915_FREQ_MAX       928000000U

/* AU915 uplink frequency ranges */
#define AU915_UP_FREQ_START  915200000U
#define AU915_UP_FREQ_STEP   200000U
#define AU915_500K_FREQ_START 915900000U
#define AU915_500K_FREQ_STEP 1600000U

/* AU915 downlink frequency range */
#define AU915_DOWN_FREQ_START 923300000U
#define AU915_DOWN_FREQ_STEP  600000U

static const struct region_dr au915_dr_table[AU915_NUM_DR] = {
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

static const uint16_t au915_dr_max_toa_ms[AU915_NUM_DR] = {
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

static const uint8_t au915_max_payload[AU915_NUM_DR] = {
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

static const uint8_t au915_rx1_dr_map[AU915_NUM_DR][AU915_NUM_DR] = {
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

static inline uint32_t au915_get_up_ch_freq(uint8_t ch_idx)
{
	if (ch_idx < AU915_NUM_125K_CH) {
		return AU915_UP_FREQ_START + ch_idx * AU915_UP_FREQ_STEP;
	}
	return AU915_500K_FREQ_START + (ch_idx - AU915_NUM_125K_CH) * AU915_500K_FREQ_STEP;
}

static inline uint32_t au915_get_rx1_freq(uint8_t tx_ch_idx)
{
	uint8_t rx_ch;
	if (tx_ch_idx < AU915_NUM_125K_CH) {
		rx_ch = tx_ch_idx % AU915_NUM_DOWN_CH;
	} else {
		rx_ch = tx_ch_idx - AU915_NUM_125K_CH;
	}
	return AU915_DOWN_FREQ_START + rx_ch * AU915_DOWN_FREQ_STEP;
}

static inline uint8_t au915_get_rx1_dr(uint8_t tx_dr, uint8_t offset)
{
	if (tx_dr >= AU915_NUM_DR) {
		tx_dr = AU915_NUM_DR - 1;
	}
	if (offset >= AU915_NUM_DR) {
		offset = AU915_NUM_DR - 1;
	}
	return au915_rx1_dr_map[tx_dr][offset];
}

static inline uint16_t au915_get_rx_window_timeout_ms(uint8_t dr)
{
	if (dr >= AU915_NUM_DR) {
		return 3000;
	}
	return au915_dr_max_toa_ms[dr] + 50;
}

static inline int au915_get_max_payload(uint8_t dr)
{
	if (dr >= AU915_NUM_DR) {
		return -EINVAL;
	}
	return au915_max_payload[dr];
}

static inline int au915_apply_cflist(struct region_ctx *ctx, const uint8_t cflist[16])
{
	return region_cflist_type_b(ctx, cflist);
}

#ifdef __cplusplus
}
#endif

#endif /* LORAWAN_REGION_AU915_H_ */
