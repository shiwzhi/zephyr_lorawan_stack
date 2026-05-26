/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 *
 * LoRaWAN EU433 region-specific parameters and functions.
 * Per LoRaWAN 1.0.3 Regional Parameters v1.0.3 Rev A.
 */

#ifndef LORAWAN_REGION_EU433_H_
#define LORAWAN_REGION_EU433_H_

#include "region.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EU433_NUM_CH         3
#define EU433_NUM_DR         7   /* DR0-DR6 (DR6: SF7/250kHz) */
#define EU433_RX2_FREQ      869525000U
#define EU433_RX2_DR         0
#define EU433_RX1_DELAY_S    1
#define EU433_RX2_DELAY_S    2
#define EU433_TX_MAX_DUTY    100  /* permille: 1% duty cycle */
#define EU433_DEFAULT_TX_POWER 16

/* EU433 frequency band limits */
#define EU433_FREQ_MIN       863000000U
#define EU433_FREQ_MAX       870000000U

/* EU433 default channels */
#define EU433_CH0_FREQ       868100000U
#define EU433_CH1_FREQ       868300000U
#define EU433_CH2_FREQ       868500000U

/* EU433 join accept channels */
#define EU433_JA_CH0_FREQ    868100000U
#define EU433_JA_CH1_FREQ    868300000U
#define EU433_JA_CH2_FREQ    868500000U

extern const struct region_dr eu433_dr_table[EU433_NUM_DR];
extern const uint16_t eu433_dr_max_toa_ms[EU433_NUM_DR];
extern const uint8_t eu433_max_payload[EU433_NUM_DR];
extern const uint8_t eu433_rx1_dr_map[EU433_NUM_DR][EU433_NUM_DR];
extern const uint32_t eu433_ch_freq[EU433_NUM_CH];

static inline uint32_t eu433_get_rx1_freq(uint8_t tx_ch_idx)
{
	ARG_UNUSED(tx_ch_idx);
	return EU433_CH0_FREQ;
}

static inline uint8_t eu433_get_rx1_dr(uint8_t tx_dr, uint8_t offset)
{
	if (tx_dr >= EU433_NUM_DR) {
		tx_dr = EU433_NUM_DR - 1;
	}
	if (offset >= EU433_NUM_DR) {
		offset = EU433_NUM_DR - 1;
	}
	return eu433_rx1_dr_map[tx_dr][offset];
}

static inline uint16_t eu433_get_rx_window_timeout_ms(uint8_t dr)
{
	if (dr >= EU433_NUM_DR) {
		return 3000;
	}
	return eu433_dr_max_toa_ms[dr] + 50;
}

static inline int eu433_get_max_payload(uint8_t dr)
{
	if (dr >= EU433_NUM_DR) {
		return -EINVAL;
	}
	return eu433_max_payload[dr];
}

static inline int eu433_apply_cflist(struct region_ctx *ctx, const uint8_t cflist[16])
{
	return region_cflist_type_a(ctx, cflist, 3, EU433_FREQ_MIN, EU433_FREQ_MAX,
				    EU433_NUM_DR, 8);
}

#ifdef __cplusplus
}
#endif

#endif /* LORAWAN_REGION_EU433_H_ */
