/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 *
 * LoRaWAN EU868 region-specific parameters and functions.
 * Per LoRaWAN RP002-1.0.4 Regional Parameters.
 */

#ifndef LORAWAN_REGION_EU868_H_
#define LORAWAN_REGION_EU868_H_

#include "region.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EU868_NUM_CH         3
#define EU868_NUM_DR         7   /* DR0-DR6 (DR6: SF7/250kHz) */
#define EU868_RX2_FREQ      869525000U
#define EU868_RX2_DR         0
#define EU868_RX1_DELAY_S    1
#define EU868_RX2_DELAY_S    2
#define EU868_TX_MAX_DUTY    100  /* permille: 1% duty cycle */
#define EU868_DEFAULT_TX_POWER 16  /* ETSI limit: +14 dBm, +16 with antenna */

/* EU868 frequency band limits (ETSI) */
#define EU868_FREQ_MIN       863000000U
#define EU868_FREQ_MAX       870000000U

/* EU868 default channels */
#define EU868_CH0_FREQ       868100000U
#define EU868_CH1_FREQ       868300000U
#define EU868_CH2_FREQ       868500000U

/* EU868 join accept channels */
#define EU868_JA_CH0_FREQ    868100000U
#define EU868_JA_CH1_FREQ    868300000U
#define EU868_JA_CH2_FREQ    868500000U

static const struct region_dr eu868_dr_table[EU868_NUM_DR] = {
	{SF_12, BW_125_KHZ},  /* DR0 */
	{SF_11, BW_125_KHZ},  /* DR1 */
	{SF_10, BW_125_KHZ},  /* DR2 */
	{SF_9,  BW_125_KHZ},  /* DR3 */
	{SF_8,  BW_125_KHZ},  /* DR4 */
	{SF_7,  BW_125_KHZ},  /* DR5 */
	{SF_7,  BW_250_KHZ},  /* DR6 */
};

static const uint16_t eu868_dr_max_toa_ms[EU868_NUM_DR] = {
	/* DR0 SF12/125k */ 1600,
	/* DR1 SF11/125k */  900,
	/* DR2 SF10/125k */  500,
	/* DR3 SF9/125k  */  300,
	/* DR4 SF8/125k  */  180,
	/* DR5 SF7/125k  */  120,
	/* DR6 SF7/250k  */   60,
};

static const uint8_t eu868_max_payload[EU868_NUM_DR] = {
	/* DR0 */  59,
	/* DR1 */  59,
	/* DR2 */  59,
	/* DR3 */ 123,
	/* DR4 */ 250,
	/* DR5 */ 250,
	/* DR6 */ 230,
};

static const uint8_t eu868_rx1_dr_map[EU868_NUM_DR][EU868_NUM_DR] = {
	/* DR0: */ {0, 0, 0, 0, 0, 0, 0},
	/* DR1: */ {1, 0, 0, 0, 0, 0, 0},
	/* DR2: */ {2, 1, 0, 0, 0, 0, 0},
	/* DR3: */ {3, 2, 1, 0, 0, 0, 0},
	/* DR4: */ {4, 3, 2, 1, 0, 0, 0},
	/* DR5: */ {5, 4, 3, 2, 1, 0, 0},
	/* DR6: */ {6, 5, 4, 3, 2, 1, 0},
};

static const uint32_t eu868_ch_freq[EU868_NUM_CH] = {
	EU868_CH0_FREQ,
	EU868_CH1_FREQ,
	EU868_CH2_FREQ,
};

static inline uint32_t eu868_get_rx1_freq(uint8_t tx_ch_idx)
{
	/* EU868: RX1 freq = TX freq */
	ARG_UNUSED(tx_ch_idx);
	return EU868_CH0_FREQ; /* actual freq set by channel mask */
}

static inline uint8_t eu868_get_rx1_dr(uint8_t tx_dr, uint8_t offset)
{
	if (tx_dr >= EU868_NUM_DR) {
		tx_dr = EU868_NUM_DR - 1;
	}
	if (offset >= EU868_NUM_DR) {
		offset = EU868_NUM_DR - 1;
	}
	return eu868_rx1_dr_map[tx_dr][offset];
}

static inline uint16_t eu868_get_rx_window_timeout_ms(uint8_t dr)
{
	if (dr >= EU868_NUM_DR) {
		return 3000;
	}
	return eu868_dr_max_toa_ms[dr] + 50;
}

static inline int eu868_get_max_payload(uint8_t dr)
{
	if (dr >= EU868_NUM_DR) {
		return -EINVAL;
	}
	return eu868_max_payload[dr];
}

static inline int eu868_apply_cflist(struct region_ctx *ctx, const uint8_t cflist[16])
{
	return region_cflist_type_a(ctx, cflist, 3, EU868_FREQ_MIN, EU868_FREQ_MAX,
				    EU868_NUM_DR, 8);
}

#ifdef __cplusplus
}
#endif

#endif /* LORAWAN_REGION_EU868_H_ */
