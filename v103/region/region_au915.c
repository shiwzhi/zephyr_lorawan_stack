/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 *
 * LoRaWAN AU915 region-specific implementation.
 * Per LoRaWAN 1.0.3 Regional Parameters v1.0.3 Rev A.
 */

#include "region_au915.h"

/*
 * AU915 DR to LoRa modem configuration
 * DR0-DR5: SF12-SF7 @ 125 kHz
 * DR6:     SF8 @ 500 kHz
 * DR7:     RFU
 * DR8:     SF12 @ 500 kHz
 * DR9:     SF11 @ 500 kHz
 * DR10:    SF10 @ 500 kHz
 * DR11:    SF9 @ 500 kHz
 * DR12:    SF8 @ 500 kHz
 * DR13:    SF7 @ 500 kHz
 */
const struct region_dr au915_dr_table[AU915_NUM_DR] = {
	{SF_12, BW_125_KHZ},  /* DR0 */
	{SF_11, BW_125_KHZ},  /* DR1 */
	{SF_10, BW_125_KHZ},  /* DR2 */
	{SF_9,  BW_125_KHZ},  /* DR3 */
	{SF_8,  BW_125_KHZ},  /* DR4 */
	{SF_7,  BW_125_KHZ},  /* DR5 */
	{SF_8,  BW_500_KHZ},  /* DR6 */
	{SF_8,  BW_500_KHZ},  /* DR7 (RFU) */
	{SF_12, BW_500_KHZ},  /* DR8 */
	{SF_11, BW_500_KHZ},  /* DR9 */
	{SF_10, BW_500_KHZ},  /* DR10 */
	{SF_9,  BW_500_KHZ},  /* DR11 */
	{SF_8,  BW_500_KHZ},  /* DR12 */
	{SF_7,  BW_500_KHZ},  /* DR13 */
};

/*
 * Maximum ToA per DR (ms) with max payload
 */
const uint16_t au915_dr_max_toa_ms[AU915_NUM_DR] = {
	/* DR0 SF12/125k */ 1600,
	/* DR1 SF11/125k */  900,
	/* DR2 SF10/125k */  500,
	/* DR3 SF9/125k  */  300,
	/* DR4 SF8/125k  */  180,
	/* DR5 SF7/125k  */  120,
	/* DR6 SF8/500k  */   60,
	/* DR7 RFU       */   60,
	/* DR8 SF12/500k */   30,
	/* DR9 SF11/500k */   20,
	/* DR10 SF10/500k */  20,
	/* DR11 SF9/500k  */  20,
	/* DR12 SF8/500k  */  20,
	/* DR13 SF7/500k  */  20,
};

/*
 * AU915 maximum application payload size per DR
 * Per LoRaWAN 1.0.3 Regional Parameters v1.0.3 Rev A Table 39
 */
const uint8_t au915_max_payload[AU915_NUM_DR] = {
	/* DR0  */  59,
	/* DR1  */  59,
	/* DR2  */  59,
	/* DR3  */ 123,
	/* DR4  */ 250,
	/* DR5  */ 250,
	/* DR6  */ 250,
	/* DR7  */   0,  /* RFU */
	/* DR8  */  61,
	/* DR9  */ 137,
	/* DR10 */ 250,
	/* DR11 */ 250,
	/* DR12 */ 250,
	/* DR13 */ 250,
};

/*
 * AU915 RX1 DR offset mapping
 * Per LoRaWAN 1.0.3 Regional Parameters v1.0.3 Rev A Table 40
 */
const uint8_t au915_rx1_dr_map[AU915_NUM_DR][AU915_NUM_DR] = {
	/* DR0  */ { 8,  8,  8,  8,  8,  8,  8,  8,  8, 8, 8, 8, 8, 8},
	/* DR1  */ { 9,  8,  8,  8,  8,  8,  8,  8,  8, 8, 8, 8, 8, 8},
	/* DR2  */ {10,  9,  8,  8,  8,  8,  8,  8,  8, 8, 8, 8, 8, 8},
	/* DR3  */ {11, 10,  9,  8,  8,  8,  8,  8,  8, 8, 8, 8, 8, 8},
	/* DR4  */ {12, 11, 10,  9,  8,  8,  8,  8,  8, 8, 8, 8, 8, 8},
	/* DR5  */ {13, 12, 11, 10,  9,  8,  8,  8,  8, 8, 8, 8, 8, 8},
	/* DR6  */ {13, 13, 12, 11, 10,  9,  8,  8,  8, 8, 8, 8, 8, 8},
	/* DR7  */ { 8,  8,  8,  8,  8,  8,  8,  8,  8, 8, 8, 8, 8, 8},
	/* DR8  */ { 8,  8,  8,  8,  8,  8,  8,  8,  8, 8, 8, 8, 8, 8},
	/* DR9  */ { 9,  8,  8,  8,  8,  8,  8,  8,  8, 8, 8, 8, 8, 8},
	/* DR10 */ {10,  9,  8,  8,  8,  8,  8,  8,  8, 8, 8, 8, 8, 8},
	/* DR11 */ {11, 10,  9,  8,  8,  8,  8,  8,  8, 8, 8, 8, 8, 8},
	/* DR12 */ {12, 11, 10,  9,  8,  8,  8,  8,  8, 8, 8, 8, 8, 8},
	/* DR13 */ {13, 12, 11, 10,  9,  8,  8,  8,  8, 8, 8, 8, 8, 8},
};
