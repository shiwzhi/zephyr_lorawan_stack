/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 *
 * LoRaWAN AS923 region-specific implementation.
 * Per LoRaWAN RP002-1.0.4 Regional Parameters.
 */

#include "region_as923.h"

/*
 * AS923 DR to LoRa modem configuration
 * DR0-DR5: SF12-SF7 @ 125 kHz
 */
const struct region_dr as923_dr_table[AS923_NUM_DR] = {
	{SF_12, BW_125_KHZ},  /* DR0 */
	{SF_11, BW_125_KHZ},  /* DR1 */
	{SF_10, BW_125_KHZ},  /* DR2 */
	{SF_9,  BW_125_KHZ},  /* DR3 */
	{SF_8,  BW_125_KHZ},  /* DR4 */
	{SF_7,  BW_125_KHZ},  /* DR5 */
};

const uint16_t as923_dr_max_toa_ms[AS923_NUM_DR] = {
	/* DR0 */ 1600,
	/* DR1 */  900,
	/* DR2 */  500,
	/* DR3 */  300,
	/* DR4 */  180,
	/* DR5 */  120,
};

const uint8_t as923_max_payload[AS923_NUM_DR] = {
	/* DR0 */  59,
	/* DR1 */  59,
	/* DR2 */  59,
	/* DR3 */ 123,
	/* DR4 */ 230,
	/* DR5 */ 230,
};

/*
 * AS923 RX1 DR offset mapping
 */
const uint8_t as923_rx1_dr_map[AS923_NUM_DR][AS923_NUM_DR] = {
	/* DR0: */ {0, 0, 0, 0, 0, 0},
	/* DR1: */ {1, 0, 0, 0, 0, 0},
	/* DR2: */ {2, 1, 0, 0, 0, 0},
	/* DR3: */ {3, 2, 1, 0, 0, 0},
	/* DR4: */ {4, 3, 2, 1, 0, 0},
	/* DR5: */ {5, 4, 3, 2, 1, 0},
};

/*
 * AS923 default channel frequencies: 923.2 - 924.6 MHz, step 200 kHz
 */
const uint32_t as923_ch_freq[AS923_NUM_CH] = {
	923200000U, 923400000U, 923600000U, 923800000U,
	924000000U, 924200000U, 924400000U, 924600000U,
};
