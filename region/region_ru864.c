/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 *
 * LoRaWAN RU864 region-specific implementation.
 * Per LoRaWAN RP002-1.0.4 Regional Parameters.
 */

#include "region_ru864.h"

/*
 * RU864 DR to LoRa modem configuration
 * DR0-DR5: SF12-SF7 @ 125 kHz
 */
const struct region_dr ru864_dr_table[RU864_NUM_DR] = {
	{SF_12, BW_125_KHZ},  /* DR0 */
	{SF_11, BW_125_KHZ},  /* DR1 */
	{SF_10, BW_125_KHZ},  /* DR2 */
	{SF_9,  BW_125_KHZ},  /* DR3 */
	{SF_8,  BW_125_KHZ},  /* DR4 */
	{SF_7,  BW_125_KHZ},  /* DR5 */
};

const uint16_t ru864_dr_max_toa_ms[RU864_NUM_DR] = {
	/* DR0 */ 1600,
	/* DR1 */  900,
	/* DR2 */  500,
	/* DR3 */  300,
	/* DR4 */  180,
	/* DR5 */  120,
};

const uint8_t ru864_max_payload[RU864_NUM_DR] = {
	/* DR0 */  59,
	/* DR1 */  59,
	/* DR2 */  59,
	/* DR3 */ 123,
	/* DR4 */ 230,
	/* DR5 */ 230,
};

const uint8_t ru864_rx1_dr_map[RU864_NUM_DR][RU864_NUM_DR] = {
	/* DR0: */ {0, 0, 0, 0, 0, 0},
	/* DR1: */ {1, 0, 0, 0, 0, 0},
	/* DR2: */ {2, 1, 0, 0, 0, 0},
	/* DR3: */ {3, 2, 1, 0, 0, 0},
	/* DR4: */ {4, 3, 2, 1, 0, 0},
	/* DR5: */ {5, 4, 3, 2, 1, 0},
};

/*
 * RU864 default channel frequencies: 868.9 - 869.4 MHz
 */
const uint32_t ru864_ch_freq[RU864_NUM_CH] = {
	868900000U, 869100000U, 869300000U, 869500000U,
	869700000U, 869900000U, 870100000U, 870300000U,
};
