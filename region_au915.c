/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 *
 * LoRaWAN AU915 region-specific implementation.
 * Per LoRaWAN RP002-1.0.4 Regional Parameters.
 */

#include "region_au915.h"

/*
 * AU915 DR to LoRa modem configuration
 * Same DR mapping as US915
 */
const struct region_dr au915_dr_table[AU915_NUM_DR] = {
	{SF_12, BW_125_KHZ},  /* DR0 */
	{SF_11, BW_125_KHZ},  /* DR1 */
	{SF_10, BW_125_KHZ},  /* DR2 */
	{SF_9,  BW_125_KHZ},  /* DR3 */
	{SF_8,  BW_125_KHZ},  /* DR4 */
	{SF_7,  BW_125_KHZ},  /* DR5 (RFU) */
	{SF_8,  BW_500_KHZ},  /* DR6 (RFU) */
	{SF_7,  BW_500_KHZ},  /* DR7 (RFU) */
	{SF_12, BW_500_KHZ},  /* DR8 */
	{SF_8,  BW_500_KHZ},  /* DR9 */
};

const uint16_t au915_dr_max_toa_ms[AU915_NUM_DR] = {
	/* DR0  */  400,
	/* DR1  */  250,
	/* DR2  */  150,
	/* DR3  */  100,
	/* DR4  */   60,
	/* DR5  */  100,
	/* DR6  */  100,
	/* DR7  */  100,
	/* DR8  */   30,
	/* DR9  */   20,
};

const uint8_t au915_max_payload[AU915_NUM_DR] = {
	/* DR0  */  19,
	/* DR1  */  61,
	/* DR2  */ 133,
	/* DR3  */ 250,
	/* DR4  */ 250,
	/* DR5  */   0,
	/* DR6  */   0,
	/* DR7  */   0,
	/* DR8  */  41,
	/* DR9  */ 117,
};

/*
 * AU915 RX1 DR offset mapping
 * Per RP002-1.0.4 (same as US915)
 */
const uint8_t au915_rx1_dr_map[AU915_NUM_DR][AU915_NUM_DR] = {
	/* DR0  */ {10, 9, 8, 8, 8, 8, 8, 8, 8, 8},
	/* DR1  */ {11, 10, 9, 8, 8, 8, 8, 8, 8, 8},
	/* DR2  */ {12, 11, 10, 9, 8, 8, 8, 8, 8, 8},
	/* DR3  */ {13, 12, 11, 10, 9, 8, 8, 8, 8, 8},
	/* DR4  */ {13, 13, 12, 11, 10, 9, 8, 8, 8, 8},
	/* DR5  */ {13, 13, 13, 12, 11, 10, 9, 8, 8, 8},
	/* DR6  */ {13, 13, 13, 13, 12, 11, 10, 9, 8, 8},
	/* DR7  */ {13, 13, 13, 13, 13, 12, 11, 10, 9, 8},
	/* DR8  */ { 8,  8,  8,  8,  8,  8,  8,  8,  8, 8},
	/* DR9  */ { 9,  8,  8,  8,  8,  8,  8,  8,  8, 8},
};
