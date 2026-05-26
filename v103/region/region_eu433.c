/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 *
 * LoRaWAN EU433 region-specific implementation.
 * Per LoRaWAN 1.0.3 Regional Parameters v1.0.3 Rev A.
 */

#include "region_eu433.h"

/*
 * EU433 DR to LoRa modem configuration
 * DR0-DR5: SF12-SF7 @ 125 kHz
 * DR6: SF7 @ 250 kHz
 */
const struct region_dr eu433_dr_table[EU433_NUM_DR] = {
	{SF_12, BW_125_KHZ},  /* DR0 */
	{SF_11, BW_125_KHZ},  /* DR1 */
	{SF_10, BW_125_KHZ},  /* DR2 */
	{SF_9,  BW_125_KHZ},  /* DR3 */
	{SF_8,  BW_125_KHZ},  /* DR4 */
	{SF_7,  BW_125_KHZ},  /* DR5 */
	{SF_7,  BW_250_KHZ},  /* DR6 */
};

/*
 * Maximum ToA per DR (ms) with max payload
 */
const uint16_t eu433_dr_max_toa_ms[EU433_NUM_DR] = {
	/* DR0 SF12/125k */ 1600,
	/* DR1 SF11/125k */  900,
	/* DR2 SF10/125k */  500,
	/* DR3 SF9/125k  */  300,
	/* DR4 SF8/125k  */  180,
	/* DR5 SF7/125k  */  120,
	/* DR6 SF7/250k  */   60,
};

/*
 * EU433 maximum application payload size per DR
 * Per LoRaWAN 1.0.3 Regional Parameters v1.0.3 Rev A Table 77
 */
const uint8_t eu433_max_payload[EU433_NUM_DR] = {
	/* DR0 */  59,
	/* DR1 */  59,
	/* DR2 */  59,
	/* DR3 */ 123,
	/* DR4 */ 250,
	/* DR5 */ 250,
	/* DR6 */ 230,
};

/*
 * EU433 RX1 DR offset mapping
 * Per LoRaWAN 1.0.3 Regional Parameters v1.0.3 Rev A Table 78
 */
const uint8_t eu433_rx1_dr_map[EU433_NUM_DR][EU433_NUM_DR] = {
	/* DR0: */ {0, 0, 0, 0, 0, 0, 0},
	/* DR1: */ {1, 0, 0, 0, 0, 0, 0},
	/* DR2: */ {2, 1, 0, 0, 0, 0, 0},
	/* DR3: */ {3, 2, 1, 0, 0, 0, 0},
	/* DR4: */ {4, 3, 2, 1, 0, 0, 0},
	/* DR5: */ {5, 4, 3, 2, 1, 0, 0},
	/* DR6: */ {6, 5, 4, 3, 2, 1, 0},
};

/*
 * EU433 default channel frequencies
 */
const uint32_t eu433_ch_freq[EU433_NUM_CH] = {
	EU433_CH0_FREQ,
	EU433_CH1_FREQ,
	EU433_CH2_FREQ,
};
