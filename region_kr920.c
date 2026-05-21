/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 *
 * LoRaWAN KR920 region-specific implementation.
 * Per LoRaWAN RP002-1.0.4 Regional Parameters.
 */

#include "region_kr920.h"

/*
 * KR920 DR to LoRa modem configuration
 * DR0-DR4: SF12-SF8 @ 125 kHz
 */
const struct region_dr kr920_dr_table[KR920_NUM_DR] = {
	{SF_12, BW_125_KHZ},  /* DR0 */
	{SF_11, BW_125_KHZ},  /* DR1 */
	{SF_10, BW_125_KHZ},  /* DR2 */
	{SF_9,  BW_125_KHZ},  /* DR3 */
	{SF_8,  BW_125_KHZ},  /* DR4 */
};

const uint16_t kr920_dr_max_toa_ms[KR920_NUM_DR] = {
	/* DR0 */ 1600,
	/* DR1 */  900,
	/* DR2 */  500,
	/* DR3 */  300,
	/* DR4 */  180,
};

/*
 * KR920 maximum application payload size per DR
 * Per RP002-1.0.4
 */
const uint8_t kr920_max_payload[KR920_NUM_DR] = {
	/* DR0 */  59,
	/* DR1 */  59,
	/* DR2 */  59,
	/* DR3 */ 123,
	/* DR4 */ 230,
};

/*
 * KR920 RX1 DR offset mapping
 */
const uint8_t kr920_rx1_dr_map[KR920_NUM_DR][KR920_NUM_DR] = {
	/* DR0: */ {0, 0, 0, 0, 0},
	/* DR1: */ {1, 0, 0, 0, 0},
	/* DR2: */ {2, 1, 0, 0, 0},
	/* DR3: */ {3, 2, 1, 0, 0},
	/* DR4: */ {4, 3, 2, 1, 0},
};

/*
 * KR920 default channel frequencies: 922.1 - 923.5 MHz, step 200 kHz
 */
const uint32_t kr920_ch_freq[KR920_NUM_CH] = {
	922100000U, 922300000U, 922500000U, 922700000U,
	922900000U, 923100000U, 923300000U, 923500000U,
};
