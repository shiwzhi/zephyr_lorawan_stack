/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 *
 * LoRaWAN KR920 region-specific parameters and functions.
 * Per LoRaWAN RP002-1.0.4 Regional Parameters.
 */

#ifndef LORAWAN_REGION_KR920_H_
#define LORAWAN_REGION_KR920_H_

#include "region.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KR920_NUM_CH         13
#define KR920_NUM_DR         6
#define KR920_RX2_FREQ      921900000U
#define KR920_RX2_DR         0
#define KR920_RX1_DELAY_S    1
#define KR920_RX2_DELAY_S    2
#define KR920_TX_MAX_DUTY    100  /* permille: 1% duty cycle */
#define KR920_DEFAULT_TX_POWER 14

/* KR920 frequency band limits (KCC) */
#define KR920_FREQ_MIN       920000000U
#define KR920_FREQ_MAX       923000000U

static const struct region_dr kr920_dr_table[KR920_NUM_DR] = {
	{SF_12, BW_125_KHZ},  /* DR0 */
	{SF_11, BW_125_KHZ},  /* DR1 */
	{SF_10, BW_125_KHZ},  /* DR2 */
	{SF_9,  BW_125_KHZ},  /* DR3 */
	{SF_8,  BW_125_KHZ},  /* DR4 */
	{SF_7,  BW_125_KHZ},  /* DR5 */
};

static const uint16_t kr920_dr_max_toa_ms[KR920_NUM_DR] = {
	/* DR0 */ 1600,
	/* DR1 */  900,
	/* DR2 */  500,
	/* DR3 */  300,
	/* DR4 */  180,
	/* DR5 */  120,
};

static const uint8_t kr920_max_payload[KR920_NUM_DR] = {
	/* DR0 */  59,
	/* DR1 */  59,
	/* DR2 */  59,
	/* DR3 */ 123,
	/* DR4 */ 230,
	/* DR5 */ 230,
};

static const uint8_t kr920_rx1_dr_map[KR920_NUM_DR][KR920_NUM_DR] = {
	/* DR0: */ {0, 0, 0, 0, 0, 0},
	/* DR1: */ {1, 0, 0, 0, 0, 0},
	/* DR2: */ {2, 1, 0, 0, 0, 0},
	/* DR3: */ {3, 2, 1, 0, 0, 0},
	/* DR4: */ {4, 3, 2, 1, 0, 0},
	/* DR5: */ {5, 4, 3, 2, 1, 0},
};

static const uint32_t kr920_ch_freq[KR920_NUM_CH] = {
	920900000U, 921100000U, 921300000U, 921500000U,
	921700000U, 921900000U, 922100000U, 922300000U,
	922500000U, 922700000U, 922900000U, 923100000U,
	923300000U,
};

static inline uint32_t kr920_get_rx1_freq(uint8_t tx_ch_idx)
{
	ARG_UNUSED(tx_ch_idx);
	return 0; /* actual freq set by channel mask */
}

static inline uint8_t kr920_get_rx1_dr(uint8_t tx_dr, uint8_t offset)
{
	if (tx_dr >= KR920_NUM_DR) {
		tx_dr = KR920_NUM_DR - 1;
	}
	if (offset >= KR920_NUM_DR) {
		offset = KR920_NUM_DR - 1;
	}
	return kr920_rx1_dr_map[tx_dr][offset];
}

static inline uint16_t kr920_get_rx_window_timeout_ms(uint8_t dr)
{
	if (dr >= KR920_NUM_DR) {
		return 3000;
	}
	return kr920_dr_max_toa_ms[dr] + 50;
}

static inline int kr920_get_max_payload(uint8_t dr)
{
	if (dr >= KR920_NUM_DR) {
		return -EINVAL;
	}
	return kr920_max_payload[dr];
}

static inline int kr920_apply_cflist(struct region_ctx *ctx, const uint8_t cflist[16])
{
	return region_cflist_type_a(ctx, cflist, 3, KR920_FREQ_MIN, KR920_FREQ_MAX,
				    KR920_NUM_DR, 0);
}

#ifdef __cplusplus
}
#endif

#endif /* LORAWAN_REGION_KR920_H_ */
