/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 *
 * LoRaWAN IN865 region-specific parameters and functions.
 * Per LoRaWAN RP002-1.0.4 Regional Parameters.
 */

#ifndef LORAWAN_REGION_IN865_H_
#define LORAWAN_REGION_IN865_H_

#include "region.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IN865_NUM_CH         3
#define IN865_NUM_DR         6
#define IN865_RX2_FREQ      866550000U
#define IN865_RX2_DR         2
#define IN865_RX1_DELAY_S    1
#define IN865_RX2_DELAY_S    2
#define IN865_DEFAULT_TX_POWER 30

/* IN865 frequency band limits (DoT) */
#define IN865_FREQ_MIN       865000000U
#define IN865_FREQ_MAX       867000000U

/* IN865 default channels */
#define IN865_CH0_FREQ       865062500U
#define IN865_CH1_FREQ       865402500U
#define IN865_CH2_FREQ       865985000U

static const struct region_dr in865_dr_table[IN865_NUM_DR] = {
	{SF_12, BW_125_KHZ},  /* DR0 */
	{SF_11, BW_125_KHZ},  /* DR1 */
	{SF_10, BW_125_KHZ},  /* DR2 */
	{SF_9,  BW_125_KHZ},  /* DR3 */
	{SF_8,  BW_125_KHZ},  /* DR4 */
	{SF_7,  BW_125_KHZ},  /* DR5 */
};

static const uint16_t in865_dr_max_toa_ms[IN865_NUM_DR] = {
	/* DR0 */ 1600,
	/* DR1 */  900,
	/* DR2 */  500,
	/* DR3 */  300,
	/* DR4 */  180,
	/* DR5 */  120,
};

static const uint8_t in865_max_payload[IN865_NUM_DR] = {
	/* DR0 */  59,
	/* DR1 */  59,
	/* DR2 */  59,
	/* DR3 */ 123,
	/* DR4 */ 230,
	/* DR5 */ 230,
};

static const uint8_t in865_rx1_dr_map[IN865_NUM_DR][IN865_NUM_DR] = {
	/* DR0: */ {0, 0, 0, 0, 0, 0},
	/* DR1: */ {1, 0, 0, 0, 0, 0},
	/* DR2: */ {2, 1, 0, 0, 0, 0},
	/* DR3: */ {3, 2, 1, 0, 0, 0},
	/* DR4: */ {4, 3, 2, 1, 0, 0},
	/* DR5: */ {5, 4, 3, 2, 1, 0},
};

static const uint32_t in865_ch_freq[IN865_NUM_CH] = {
	IN865_CH0_FREQ,
	IN865_CH1_FREQ,
	IN865_CH2_FREQ,
};

static inline uint32_t in865_get_rx1_freq(uint8_t tx_ch_idx)
{
	ARG_UNUSED(tx_ch_idx);
	return 0;
}

static inline uint8_t in865_get_rx1_dr(uint8_t tx_dr, uint8_t offset)
{
	if (tx_dr >= IN865_NUM_DR) {
		tx_dr = IN865_NUM_DR - 1;
	}
	if (offset <= 5) {
		if (offset >= IN865_NUM_DR) {
			offset = IN865_NUM_DR - 1;
		}
		return in865_rx1_dr_map[tx_dr][offset];
	}
	/* RX1DROffset 6 → effective offset -1, 7 → effective offset -2
	 * Per IN865 spec: Downstream DR = MIN(5, tx_dr - effective_offset)
	 */
	int dr = (int)tx_dr;
	dr += (offset == 6) ? 1 : 2;
	if (dr >= IN865_NUM_DR) {
		dr = IN865_NUM_DR - 1;
	}
	return (uint8_t)dr;
}

static inline uint16_t in865_get_rx_window_timeout_ms(uint8_t dr)
{
	if (dr >= IN865_NUM_DR) {
		return 3000;
	}
	return in865_dr_max_toa_ms[dr] + 50;
}

static inline int in865_get_max_payload(uint8_t dr)
{
	if (dr >= IN865_NUM_DR) {
		return -EINVAL;
	}
	return in865_max_payload[dr];
}

static inline int in865_apply_cflist(struct region_ctx *ctx, const uint8_t cflist[16])
{
	return region_cflist_type_a(ctx, cflist, 3, IN865_FREQ_MIN, IN865_FREQ_MAX,
				    IN865_NUM_DR, 8);
}

#ifdef __cplusplus
}
#endif

#endif /* LORAWAN_REGION_IN865_H_ */
