/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 *
 * LoRaWAN EU868 region-specific implementation.
 * Per LoRaWAN RP002-1.0.4 Regional Parameters.
 */

#include "region_eu868.h"

/*
 * EU868 DR to LoRa modem configuration
 * DR0-DR5: SF12-SF7 @ 125 kHz
 * DR6: SF7 @ 250 kHz (LoRaWAN 1.0.4)
 */
const struct region_dr eu868_dr_table[EU868_NUM_DR] = {
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
const uint16_t eu868_dr_max_toa_ms[EU868_NUM_DR] = {
	/* DR0 SF12/125k */ 1600,
	/* DR1 SF11/125k */  900,
	/* DR2 SF10/125k */  500,
	/* DR3 SF9/125k  */  300,
	/* DR4 SF8/125k  */  180,
	/* DR5 SF7/125k  */  120,
	/* DR6 SF7/250k  */   60,
};

/*
 * EU868 maximum application payload size per DR
 * Per RP002-1.0.4 Table 7
 */
const uint8_t eu868_max_payload[EU868_NUM_DR] = {
	/* DR0 */  59,
	/* DR1 */  59,
	/* DR2 */  59,
	/* DR3 */ 123,
	/* DR4 */ 230,
	/* DR5 */ 230,
	/* DR6 */ 230,
};

/*
 * EU868 RX1 DR offset mapping
 * Per RP002-1.0.4 Table 8
 */
const uint8_t eu868_rx1_dr_map[EU868_NUM_DR][EU868_NUM_DR] = {
	/* DR0: */ {0, 0, 0, 0, 0, 0, 0},
	/* DR1: */ {1, 0, 0, 0, 0, 0, 0},
	/* DR2: */ {2, 1, 0, 0, 0, 0, 0},
	/* DR3: */ {3, 2, 1, 0, 0, 0, 0},
	/* DR4: */ {4, 3, 2, 1, 0, 0, 0},
	/* DR5: */ {5, 4, 3, 2, 1, 0, 0},
	/* DR6: */ {6, 5, 4, 3, 2, 1, 0},
};

/*
 * EU868 default channel frequencies
 */
const uint32_t eu868_ch_freq[EU868_NUM_CH] = {
	EU868_CH0_FREQ,
	EU868_CH1_FREQ,
	EU868_CH2_FREQ,
};
