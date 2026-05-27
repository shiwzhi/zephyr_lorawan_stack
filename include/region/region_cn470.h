/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 *
 * LoRaWAN CN470-510 region-specific parameters and functions.
 */

#ifndef LORAWAN_REGION_CN470_H_
#define LORAWAN_REGION_CN470_H_

#include "region.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* CN470-510 channel configuration */
#define CN470_NUM_UP_CH      96
#define CN470_NUM_DOWN_CH    48
#ifndef CN470_NUM_DR
#define CN470_NUM_DR         6
#endif

/* CN470-510 frequency configuration */
#define CN470_UP_FREQ_START   470300000U
#define CN470_UP_FREQ_STEP    200000U
#define CN470_DOWN_FREQ_START 500300000U
#define CN470_DOWN_FREQ_STEP  200000U
#define CN470_BEACON_FREQ_START 508300000U

/* CN470-510 frequency band limits */
#define CN470_FREQ_MIN       470000000U
#define CN470_FREQ_MAX       510000000U

/* CN470-510 default RX2 settings */
#define CN470_RX2_FREQ       505300000U
#ifndef CN470_RX2_DR
#define CN470_RX2_DR         0
#endif

/* CN470-510 default TX power (dBm) */
#define CN470_DEFAULT_TX_POWER 14

/* CN470-510 TX power map (LinkADR power index to dBm) */
#define CN470_TX_POWER_MAP {19, 17, 15, 13, 11, 9, 7, 5}

/* CN470-510 RX delay settings */
#define CN470_RX1_DELAY_S    1
#define CN470_RX2_DELAY_S    2

static const struct region_dr cn470_dr_table[CN470_NUM_DR] = {
	{SF_12, BW_125_KHZ},  /* DR0 */
	{SF_11, BW_125_KHZ},  /* DR1 */
	{SF_10, BW_125_KHZ},  /* DR2 */
	{SF_9,  BW_125_KHZ},  /* DR3 */
	{SF_8,  BW_125_KHZ},  /* DR4 */
	{SF_7,  BW_125_KHZ},  /* DR5 */
};

static const uint16_t cn470_dr_max_toa_ms[CN470_NUM_DR] = {
	/* DR0 SF12 */ 1400,
	/* DR1 SF11 */  800,
	/* DR2 SF10 */  450,
	/* DR3 SF9  */  260,
	/* DR4 SF8  */  160,
	/* DR5 SF7  */  100,
};

static const uint8_t cn470_max_payload[CN470_NUM_DR] = {
	/* DR0 */ 51,
	/* DR1 */ 51,
	/* DR2 */ 51,
	/* DR3 */ 115,
	/* DR4 */ 222,
	/* DR5 */ 222,
};

static const uint8_t cn470_rx1_dr_map[CN470_NUM_DR][CN470_NUM_DR] = {
	/* DR0: */ {0, 0, 0, 0, 0, 0},
	/* DR1: */ {1, 0, 0, 0, 0, 0},
	/* DR2: */ {2, 1, 0, 0, 0, 0},
	/* DR3: */ {3, 2, 1, 0, 0, 0},
	/* DR4: */ {4, 3, 2, 1, 0, 0},
	/* DR5: */ {5, 4, 3, 2, 1, 0},
};

static inline uint32_t cn470_get_rx1_freq(uint8_t tx_ch_idx)
{
	uint8_t rx_ch = tx_ch_idx % CN470_NUM_DOWN_CH;
	return CN470_DOWN_FREQ_START + (rx_ch * CN470_DOWN_FREQ_STEP);
}

static inline uint8_t cn470_get_rx1_dr(uint8_t tx_dr, uint8_t offset)
{
	if (tx_dr >= CN470_NUM_DR) {
		tx_dr = CN470_NUM_DR - 1;
	}
	if (offset >= CN470_NUM_DR) {
		offset = CN470_NUM_DR - 1;
	}
	return cn470_rx1_dr_map[tx_dr][offset];
}

static inline uint16_t cn470_get_rx_window_timeout_ms(uint8_t dr)
{
	if (dr >= CN470_NUM_DR) {
		return 3000;
	}
	return cn470_dr_max_toa_ms[dr] + 50;
}

static inline uint32_t cn470_get_beacon_freq(uint32_t beacon_time)
{
	uint32_t beacon_period = 128;
	uint8_t beacon_channel = (beacon_time / beacon_period) % 8;
	return CN470_BEACON_FREQ_START + (beacon_channel * CN470_DOWN_FREQ_STEP);
}

static inline int cn470_get_max_payload(uint8_t dr)
{
	if (dr >= CN470_NUM_DR) {
		return -EINVAL;
	}
	return cn470_max_payload[dr];
}

static inline int cn470_apply_cflist(struct region_ctx *ctx, const uint8_t cflist[16])
{
	return region_cflist_type_b(ctx, cflist);
}

#ifdef __cplusplus
}
#endif

#endif /* LORAWAN_REGION_CN470_H_ */
