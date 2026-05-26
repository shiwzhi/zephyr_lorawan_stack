/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 *
 * LoRaWAN US915 region-specific implementation.
 * Per LoRaWAN RP002-1.0.4 Regional Parameters.
 */

#include "region_us915.h"

/*
 * US915 DR to LoRa modem configuration
 * DR0-DR4: SF10-SF8 @ 125 kHz (uplink)
 * DR5-DR6: LR-FHSS (placeholder SF8/500)
 * DR7: RFU (placeholder SF8/500)
 * DR8-DR13: SF12-SF7 @ 500 kHz (uplink/downlink)
 */
const struct region_dr us915_dr_table[US915_NUM_DR] = {
	{SF_10, BW_125_KHZ},  /* DR0 */
	{SF_9,  BW_125_KHZ},  /* DR1 */
	{SF_8,  BW_125_KHZ},  /* DR2 */
	{SF_7,  BW_125_KHZ},  /* DR3 */
	{SF_8,  BW_500_KHZ},  /* DR4 */
	{SF_8,  BW_500_KHZ},  /* DR5 (LR-FHSS CR1/3 placeholder) */
	{SF_8,  BW_500_KHZ},  /* DR6 (LR-FHSS CR2/3 placeholder) */
	{SF_8,  BW_500_KHZ},  /* DR7 (RFU) */
	{SF_12, BW_500_KHZ},  /* DR8 */
	{SF_11, BW_500_KHZ},  /* DR9 */
	{SF_10, BW_500_KHZ},  /* DR10 */
	{SF_9,  BW_500_KHZ},  /* DR11 */
	{SF_8,  BW_500_KHZ},  /* DR12 */
	{SF_7,  BW_500_KHZ},  /* DR13 */
};

const uint16_t us915_dr_max_toa_ms[US915_NUM_DR] = {
	/* DR0  */  400,
	/* DR1  */  250,
	/* DR2  */  150,
	/* DR3  */  100,
	/* DR4  */   60,
	/* DR5  */  100,  /* LR-FHSS placeholder */
	/* DR6  */  100,  /* LR-FHSS placeholder */
	/* DR7  */  100,  /* RFU */
	/* DR8  */   30,
	/* DR9  */   20,
	/* DR10 */   20,
	/* DR11 */   20,
	/* DR12 */   20,
	/* DR13 */   20,
};

/*
 * US915 maximum application payload size per DR
 * Per RP002-1.0.4 Table 20 (Not Repeater Compatible)
 */
const uint8_t us915_max_payload[US915_NUM_DR] = {
	/* DR0  */  19,
	/* DR1  */  61,
	/* DR2  */ 133,
	/* DR3  */ 250,
	/* DR4  */ 250,
	/* DR5  */  58,  /* LR-FHSS CR1/3 */
	/* DR6  */ 133,  /* LR-FHSS CR2/3 */
	/* DR7  */   0,  /* RFU */
	/* DR8  */  61,
	/* DR9  */ 137,
	/* DR10 */ 250,
	/* DR11 */ 250,
	/* DR12 */ 250,
	/* DR13 */ 250,
};

/*
 * US915 RX1 DR offset mapping
 * Per RP002-1.0.4 Table 21
 */
const uint8_t us915_rx1_dr_map[US915_NUM_DR][US915_NUM_DR] = {
	/* DR0  */ {10, 9, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
	/* DR1  */ {11, 10, 9, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
	/* DR2  */ {12, 11, 10, 9, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
	/* DR3  */ {13, 12, 11, 10, 9, 8, 8, 8, 8, 8, 8, 8, 8, 8},
	/* DR4  */ {13, 13, 12, 11, 10, 9, 8, 8, 8, 8, 8, 8, 8, 8},
	/* DR5  */ {10, 9, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
	/* DR6  */ {11, 10, 9, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
	/* DR7  */ { 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
	/* DR8  */ { 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
	/* DR9  */ { 9, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
	/* DR10 */ {10, 9, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
	/* DR11 */ {11, 10, 9, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
	/* DR12 */ {12, 11, 10, 9, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
	/* DR13 */ {13, 12, 11, 10, 9, 8, 8, 8, 8, 8, 8, 8, 8, 8},
};
