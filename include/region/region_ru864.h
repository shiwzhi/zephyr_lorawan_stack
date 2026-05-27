/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 *
 * LoRaWAN RU864 region-specific parameters and functions.
 * Per LoRaWAN RP002-1.0.4 Regional Parameters.
 */

#ifndef LORAWAN_REGION_RU864_H_
#define LORAWAN_REGION_RU864_H_

#include "region.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RU864_NUM_CH         8
#define RU864_NUM_DR         6
#define RU864_RX2_FREQ      869100000U
#define RU864_RX2_DR         0
#define RU864_RX1_DELAY_S    1
#define RU864_RX2_DELAY_S    2
#define RU864_TX_MAX_DUTY    100  /* permille: 1% duty cycle */
#define RU864_DEFAULT_TX_POWER 16

/* RU864 frequency band limits (GKRCh) */
#define RU864_FREQ_MIN       864000000U
#define RU864_FREQ_MAX       870000000U

static const struct region_dr ru864_dr_table[RU864_NUM_DR] = {
	{SF_12, BW_125_KHZ},  /* DR0 */
	{SF_11, BW_125_KHZ},  /* DR1 */
	{SF_10, BW_125_KHZ},  /* DR2 */
	{SF_9,  BW_125_KHZ},  /* DR3 */
	{SF_8,  BW_125_KHZ},  /* DR4 */
	{SF_7,  BW_125_KHZ},  /* DR5 */
};

static const uint16_t ru864_dr_max_toa_ms[RU864_NUM_DR] = {
	/* DR0 */ 1600,
	/* DR1 */  900,
	/* DR2 */  500,
	/* DR3 */  300,
	/* DR4 */  180,
	/* DR5 */  120,
};

static const uint8_t ru864_max_payload[RU864_NUM_DR] = {
	/* DR0 */  59,
	/* DR1 */  59,
	/* DR2 */  59,
	/* DR3 */ 123,
	/* DR4 */ 230,
	/* DR5 */ 230,
};

static const uint8_t ru864_rx1_dr_map[RU864_NUM_DR][RU864_NUM_DR] = {
	/* DR0: */ {0, 0, 0, 0, 0, 0},
	/* DR1: */ {1, 0, 0, 0, 0, 0},
	/* DR2: */ {2, 1, 0, 0, 0, 0},
	/* DR3: */ {3, 2, 1, 0, 0, 0},
	/* DR4: */ {4, 3, 2, 1, 0, 0},
	/* DR5: */ {5, 4, 3, 2, 1, 0},
};

static const uint32_t ru864_ch_freq[RU864_NUM_CH] = {
	868900000U, 869100000U, 869300000U, 869500000U,
	869700000U, 869900000U, 870100000U, 870300000U,
};

static inline uint32_t ru864_get_rx1_freq(uint8_t tx_ch_idx)
{
	ARG_UNUSED(tx_ch_idx);
	return 0;
}

static inline uint8_t ru864_get_rx1_dr(uint8_t tx_dr, uint8_t offset)
{
	if (tx_dr >= RU864_NUM_DR) {
		tx_dr = RU864_NUM_DR - 1;
	}
	if (offset >= RU864_NUM_DR) {
		offset = RU864_NUM_DR - 1;
	}
	return ru864_rx1_dr_map[tx_dr][offset];
}

static inline uint16_t ru864_get_rx_window_timeout_ms(uint8_t dr)
{
	if (dr >= RU864_NUM_DR) {
		return 3000;
	}
	return ru864_dr_max_toa_ms[dr] + 50;
}

static inline int ru864_get_max_payload(uint8_t dr)
{
	if (dr >= RU864_NUM_DR) {
		return -EINVAL;
	}
	return ru864_max_payload[dr];
}

static inline int ru864_apply_cflist(struct region_ctx *ctx, const uint8_t cflist[16])
{
	return region_cflist_type_a(ctx, cflist, 2, RU864_FREQ_MIN, RU864_FREQ_MAX,
				    RU864_NUM_DR, 0);
}

#ifdef __cplusplus
}
#endif

#endif /* LORAWAN_REGION_RU864_H_ */
