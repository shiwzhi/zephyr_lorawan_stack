/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 *
 * LoRaWAN AS923 region-specific parameters and functions.
 * Per LoRaWAN RP002-1.0.4 Regional Parameters.
 *
 * AS923 has sub-bands: AS923-1 (Taiwan), AS923-2 (Japan),
 * AS923-3 (Korea), AS923-4 (Vietnam).
 * Default implementation covers AS923-1 (923.2 MHz).
 */

#ifndef LORAWAN_REGION_AS923_H_
#define LORAWAN_REGION_AS923_H_

#include "region.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AS923_NUM_CH         8
#define AS923_NUM_DR         7
#define AS923_RX2_FREQ      923200000U
#define AS923_RX2_DR         2
#define AS923_RX1_DELAY_S    1
#define AS923_RX2_DELAY_S    2
#define AS923_DEFAULT_TX_POWER 16

/* AS923 frequency band limits */
#define AS923_FREQ_MIN       915000000U
#define AS923_FREQ_MAX       928000000U

/* AS923 default channels: 923.2 - 924.6 MHz, step 200 kHz */
#define AS923_CH_FREQ_START  923200000U
#define AS923_CH_FREQ_STEP   200000U

/* AS923-Japan variant */
#define AS923JP_NUM_CH       2
#define AS923JP_RX2_FREQ     921400000U
#define AS923JP_RX2_DR       2

static const struct region_dr as923_dr_table[AS923_NUM_DR] = {
	{SF_12, BW_125_KHZ},  /* DR0 */
	{SF_11, BW_125_KHZ},  /* DR1 */
	{SF_10, BW_125_KHZ},  /* DR2 */
	{SF_9,  BW_125_KHZ},  /* DR3 */
	{SF_8,  BW_125_KHZ},  /* DR4 */
	{SF_7,  BW_125_KHZ},  /* DR5 */
	{SF_7,  BW_250_KHZ},  /* DR6 */
};

static const uint16_t as923_dr_max_toa_ms[AS923_NUM_DR] = {
	/* DR0 */ 1600,
	/* DR1 */  900,
	/* DR2 */  500,
	/* DR3 */  300,
	/* DR4 */  180,
	/* DR5 */  120,
	/* DR6 */   60,
};

static const uint8_t as923_max_payload[AS923_NUM_DR] = {
	/* DR0 */  59,
	/* DR1 */  59,
	/* DR2 */  59,
	/* DR3 */ 123,
	/* DR4 */ 230,
	/* DR5 */ 230,
	/* DR6 */ 230,
};

static const uint8_t as923_rx1_dr_map[AS923_NUM_DR][AS923_NUM_DR] = {
	/* DR0: */ {0, 0, 0, 0, 0, 0, 0},
	/* DR1: */ {1, 0, 0, 0, 0, 0, 0},
	/* DR2: */ {2, 1, 0, 0, 0, 0, 0},
	/* DR3: */ {3, 2, 1, 0, 0, 0, 0},
	/* DR4: */ {4, 3, 2, 1, 0, 0, 0},
	/* DR5: */ {5, 4, 3, 2, 1, 0, 0},
	/* DR6: */ {6, 5, 4, 3, 2, 1, 0},
};

static const uint32_t as923_ch_freq[AS923_NUM_CH] = {
	923200000U, 923400000U, 923600000U, 923800000U,
	924000000U, 924200000U, 924400000U, 924600000U,
};

static inline uint32_t as923_get_rx1_freq(uint8_t tx_ch_idx)
{
	ARG_UNUSED(tx_ch_idx);
	return AS923_CH_FREQ_START;
}

static inline uint8_t as923_get_rx1_dr(uint8_t tx_dr, uint8_t offset)
{
	if (tx_dr >= AS923_NUM_DR) {
		tx_dr = AS923_NUM_DR - 1;
	}
	if (offset <= 5) {
		if (offset >= AS923_NUM_DR) {
			offset = AS923_NUM_DR - 1;
		}
		return as923_rx1_dr_map[tx_dr][offset];
	}
	/* RX1DROffset 6 → effective offset -1, 7 → effective offset -2
	 * Per AS923 spec: Downstream DR = MIN(5, tx_dr - effective_offset)
	 */
	int dr = (int)tx_dr;
	dr += (offset == 6) ? 1 : 2;
	if (dr >= AS923_NUM_DR) {
		dr = AS923_NUM_DR - 1;
	}
	return (uint8_t)dr;
}

static inline uint16_t as923_get_rx_window_timeout_ms(uint8_t dr)
{
	if (dr >= AS923_NUM_DR) {
		return 3000;
	}
	return as923_dr_max_toa_ms[dr] + 50;
}

static inline int as923_get_max_payload(uint8_t dr)
{
	if (dr >= AS923_NUM_DR) {
		return -EINVAL;
	}
	return as923_max_payload[dr];
}

static inline int as923_apply_cflist(struct region_ctx *ctx, const uint8_t cflist[16])
{
	return region_cflist_type_a(ctx, cflist, 2, AS923_FREQ_MIN, AS923_FREQ_MAX,
				    AS923_NUM_DR, 0);
}

#ifdef __cplusplus
}
#endif

#endif /* LORAWAN_REGION_AS923_H_ */
