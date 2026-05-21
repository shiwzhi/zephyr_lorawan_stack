/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 *
 * LoRaWAN CN470-510 region-specific implementation.
 * Per LoRaWAN 1.0.3 Regional Parameters v1.0.3 Rev A.
 */

#include "region.h"
#include "region_cn470.h"

/*
 * CN470-510 DR to LoRa modem configuration
 * DR0-DR5: SF12 to SF7, all 125 kHz BW, coding rate 4/5
 */
const struct region_dr cn470_dr_table[CN470_NUM_DR] = {
	{SF_12, BW_125_KHZ},  /* DR0 */
	{SF_11, BW_125_KHZ},  /* DR1 */
	{SF_10, BW_125_KHZ},  /* DR2 */
	{SF_9,  BW_125_KHZ},  /* DR3 */
	{SF_8,  BW_125_KHZ},  /* DR4 */
	{SF_7,  BW_125_KHZ},  /* DR5 */
};

/*
 * Maximum ToA (Time on Air) for each DR with max payload (255 bytes)
 * Values in milliseconds for CN470
 * Includes preamble (8 symbols) + payload overhead
 */
const uint16_t cn470_dr_max_toa_ms[CN470_NUM_DR] = {
	/* DR0 SF12 */ 1400,
	/* DR1 SF11 */  800,
	/* DR2 SF10 */  450,
	/* DR3 SF9  */  260,
	/* DR4 SF8  */  160,
	/* DR5 SF7  */  100,
};

/*
 * CN470-510 maximum application payload size per DR
 * Per Table 44: CN470-510 maximum application payload size (N)
 */
const uint8_t cn470_max_payload[CN470_NUM_DR] = {
	/* DR0 */ 51,
	/* DR1 */ 51,
	/* DR2 */ 51,
	/* DR3 */ 115,
	/* DR4 */ 222,
	/* DR5 */ 222,
};

/*
 * CN470 RX1 DR mapping (Table 46)
 * Row = TX DR, Col = RX1DROffset
 */
const uint8_t cn470_rx1_dr_map[CN470_NUM_DR][CN470_NUM_DR] = {
	/* DR0: */ {0, 0, 0, 0, 0, 0},
	/* DR1: */ {1, 0, 0, 0, 0, 0},
	/* DR2: */ {2, 1, 0, 0, 0, 0},
	/* DR3: */ {3, 2, 1, 0, 0, 0},
	/* DR4: */ {4, 3, 2, 1, 0, 0},
	/* DR5: */ {5, 4, 3, 2, 1, 0},
};
