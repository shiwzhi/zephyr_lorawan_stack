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
#define AS923_NUM_DR         6
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

extern const struct region_dr as923_dr_table[AS923_NUM_DR];
extern const uint16_t as923_dr_max_toa_ms[AS923_NUM_DR];
extern const uint8_t as923_max_payload[AS923_NUM_DR];
extern const uint8_t as923_rx1_dr_map[AS923_NUM_DR][AS923_NUM_DR];
extern const uint32_t as923_ch_freq[AS923_NUM_CH];

static inline uint32_t as923_get_rx1_freq(uint8_t tx_ch_idx)
{
	/* AS923: RX1 freq = TX freq */
	ARG_UNUSED(tx_ch_idx);
	return AS923_CH_FREQ_START;
}

static inline uint8_t as923_get_rx1_dr(uint8_t tx_dr, uint8_t offset)
{
	if (tx_dr >= AS923_NUM_DR) {
		tx_dr = AS923_NUM_DR - 1;
	}
	if (offset >= AS923_NUM_DR) {
		offset = AS923_NUM_DR - 1;
	}
	return as923_rx1_dr_map[tx_dr][offset];
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

#ifdef __cplusplus
}
#endif

#endif /* LORAWAN_REGION_AS923_H_ */
