/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 *
 * LoRaWAN IN865 region-specific implementation.
 * Per LoRaWAN RP002-1.0.4 Regional Parameters.
 */

#include "region_in865.h"

/*
 * IN865 DR to LoRa modem configuration
 * DR0-DR5: SF12-SF7 @ 125 kHz
 */
const struct region_dr in865_dr_table[IN865_NUM_DR] = {
	{SF_12, BW_125_KHZ},  /* DR0 */
	{SF_11, BW_125_KHZ},  /* DR1 */
	{SF_10, BW_125_KHZ},  /* DR2 */
	{SF_9,  BW_125_KHZ},  /* DR3 */
	{SF_8,  BW_125_KHZ},  /* DR4 */
	{SF_7,  BW_125_KHZ},  /* DR5 */
};

const uint16_t in865_dr_max_toa_ms[IN865_NUM_DR] = {
	/* DR0 */ 1600,
	/* DR1 */  900,
	/* DR2 */  500,
	/* DR3 */  300,
	/* DR4 */  180,
	/* DR5 */  120,
};

/*
 * IN865 maximum application payload size per DR
 * Per RP002-1.0.4
 */
const uint8_t in865_max_payload[IN865_NUM_DR] = {
	/* DR0 */  59,
	/* DR1 */  59,
	/* DR2 */  59,
	/* DR3 */ 123,
	/* DR4 */ 230,
	/* DR5 */ 230,
};

/*
 * IN865 RX1 DR offset mapping
 */
const uint8_t in865_rx1_dr_map[IN865_NUM_DR][IN865_NUM_DR] = {
	/* DR0: */ {0, 0, 0, 0, 0, 0},
	/* DR1: */ {1, 0, 0, 0, 0, 0},
	/* DR2: */ {2, 1, 0, 0, 0, 0},
	/* DR3: */ {3, 2, 1, 0, 0, 0},
	/* DR4: */ {4, 3, 2, 1, 0, 0},
	/* DR5: */ {5, 4, 3, 2, 1, 0},
};

const uint32_t in865_ch_freq[IN865_NUM_CH] = {
	IN865_CH0_FREQ,
	IN865_CH1_FREQ,
	IN865_CH2_FREQ,
};
