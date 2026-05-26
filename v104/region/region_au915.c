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
 * DR0-DR6: SF12-SF8 @ 125 kHz (DR5=SF7/125)
 * DR7: LR-FHSS (placeholder SF8/500)
 * DR8-DR13: SF12-SF7 @ 500 kHz
 */
const struct region_dr au915_dr_table[AU915_NUM_DR] = {
	{SF_12, BW_125_KHZ},  /* DR0 */
	{SF_11, BW_125_KHZ},  /* DR1 */
	{SF_10, BW_125_KHZ},  /* DR2 */
	{SF_9,  BW_125_KHZ},  /* DR3 */
	{SF_8,  BW_125_KHZ},  /* DR4 */
	{SF_7,  BW_125_KHZ},  /* DR5 */
	{SF_8,  BW_500_KHZ},  /* DR6 */
	{SF_8,  BW_500_KHZ},  /* DR7 (LR-FHSS placeholder) */
	{SF_12, BW_500_KHZ},  /* DR8 */
	{SF_11, BW_500_KHZ},  /* DR9 */
	{SF_10, BW_500_KHZ},  /* DR10 */
	{SF_9,  BW_500_KHZ},  /* DR11 */
	{SF_8,  BW_500_KHZ},  /* DR12 */
	{SF_7,  BW_500_KHZ},  /* DR13 */
};

const uint16_t au915_dr_max_toa_ms[AU915_NUM_DR] = {
	/* DR0  */  400,
	/* DR1  */  250,
	/* DR2  */  150,
	/* DR3  */  100,
	/* DR4  */   60,
	/* DR5  */  100,
	/* DR6  */   60,
	/* DR7  */  100,  /* LR-FHSS placeholder */
	/* DR8  */   30,
	/* DR9  */   20,
	/* DR10 */   20,
	/* DR11 */   20,
	/* DR12 */   20,
	/* DR13 */   20,
};

/*
 * AU915 maximum application payload size per DR
 * Per RP002-1.0.4 Table 36 (Not Repeater Compatible)
 */
const uint8_t au915_max_payload[AU915_NUM_DR] = {
	/* DR0  */  59,
	/* DR1  */  59,
	/* DR2  */  59,
	/* DR3  */ 123,
	/* DR4  */ 250,
	/* DR5  */ 250,
	/* DR6  */ 250,
	/* DR7  */  58,  /* LR-FHSS */
	/* DR8  */  61,
	/* DR9  */ 137,
	/* DR10 */ 250,
	/* DR11 */ 250,
	/* DR12 */ 250,
	/* DR13 */ 250,
};

/*
 * AU915 RX1 DR offset mapping
 * Per RP002-1.0.4 Table 37
 */
const uint8_t au915_rx1_dr_map[AU915_NUM_DR][AU915_NUM_DR] = {
	/* DR0  */ { 8, 8, 9, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
	/* DR1  */ { 9, 8, 8, 9, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
	/* DR2  */ {10, 8, 8, 8, 9, 8, 8, 8, 8, 8, 8, 8, 8, 8},
	/* DR3  */ {11, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
	/* DR4  */ {12, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
	/* DR5  */ {13, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
	/* DR6  */ {13, 8, 8, 8, 8, 8, 9, 8, 8, 8, 8, 8, 8, 8},
	/* DR7  */ { 9, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
	/* DR8  */ { 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
	/* DR9  */ { 9, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
	/* DR10 */ {10, 9, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
	/* DR11 */ {11, 10, 9, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
	/* DR12 */ {12, 11, 10, 9, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
	/* DR13 */ {13, 12, 11, 10, 9, 8, 8, 8, 8, 8, 8, 8, 8, 8},
};
