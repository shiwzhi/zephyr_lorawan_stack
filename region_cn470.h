/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 *
 * LoRaWAN CN470-510 region-specific parameters and functions.
 * Per LoRaWAN 1.0.3 Regional Parameters v1.0.3 Rev A.
 */

#ifndef LORAWAN_REGION_CN470_H_
#define LORAWAN_REGION_CN470_H_

#include <zephyr/drivers/lora.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* CN470-510 channel configuration */
#define CN470_NUM_UP_CH      96
#define CN470_NUM_DOWN_CH    48
#define CN470_NUM_DR         6

/* CN470-510 frequency configuration */
#define CN470_UP_FREQ_START   470300000U
#define CN470_UP_FREQ_STEP    200000U
#define CN470_DOWN_FREQ_START 500300000U
#define CN470_DOWN_FREQ_STEP  200000U
#define CN470_BEACON_FREQ_START 508300000U

/* CN470-510 frequency band limits */
#define CN470_FREQ_MIN       470000000U
#define CN470_FREQ_MAX       510000000U

/* CN470-510 default RX2 settings */
#define CN470_RX2_FREQ       505300000U
#define CN470_RX2_DR         0

/* CN470-510 default TX power (dBm) */
#define CN470_DEFAULT_TX_POWER 14

/* CN470-510 TX power map (LinkADR power index to dBm) */
#define CN470_TX_POWER_MAP {19, 17, 15, 13, 11, 9, 7, 5}

/* CN470-510 RX delay settings */
#define CN470_RX1_DELAY_S    1
#define CN470_RX2_DELAY_S    2

/* CN470-510 DR to LoRa modem mapping */
extern const struct region_dr cn470_dr_table[CN470_NUM_DR];

/* CN470-510 maximum ToA (Time on Air) per DR in milliseconds */
extern const uint16_t cn470_dr_max_toa_ms[CN470_NUM_DR];

/* CN470-510 maximum application payload size per DR (bytes) */
extern const uint8_t cn470_max_payload[CN470_NUM_DR];

/* CN470-510 RX1 DR mapping table [tx_dr][rx1_dr_offset] */
extern const uint8_t cn470_rx1_dr_map[CN470_NUM_DR][CN470_NUM_DR];

/*
 * Get the RX1 downlink frequency for a given uplink channel number.
 * CN470: RX1 Channel = Uplink Channel modulo 48
 */
static inline uint32_t cn470_get_rx1_freq(uint8_t tx_ch_idx)
{
	uint8_t rx_ch = tx_ch_idx % CN470_NUM_DOWN_CH;
	return CN470_DOWN_FREQ_START + (rx_ch * CN470_DOWN_FREQ_STEP);
}

/*
 * Get RX1 data rate based on TX data rate and RX1 DR offset.
 */
static inline uint8_t cn470_get_rx1_dr(uint8_t tx_dr, uint8_t offset)
{
	if (tx_dr >= CN470_NUM_DR) {
		tx_dr = CN470_NUM_DR - 1;
	}
	if (offset >= CN470_NUM_DR) {
		offset = CN470_NUM_DR - 1;
	}
	return cn470_rx1_dr_map[tx_dr][offset];
}

/*
 * Get RX window timeout in milliseconds for a given DR.
 */
static inline uint16_t cn470_get_rx_window_timeout_ms(uint8_t dr)
{
	if (dr >= CN470_NUM_DR) {
		return 3000;
	}
	return cn470_dr_max_toa_ms[dr] + 50;
}

/*
 * Get beacon frequency for Class B operation.
 */
static inline uint32_t cn470_get_beacon_freq(uint32_t beacon_time)
{
	uint32_t beacon_period = 128;
	uint8_t beacon_channel = (beacon_time / beacon_period) % 8;
	return CN470_BEACON_FREQ_START + (beacon_channel * CN470_DOWN_FREQ_STEP);
}

/*
 * Get maximum payload size for a given DR.
 */
static inline int cn470_get_max_payload(uint8_t dr)
{
	if (dr >= CN470_NUM_DR) {
		return -EINVAL;
	}
	return cn470_max_payload[dr];
}

#ifdef __cplusplus
}
#endif

#endif /* LORAWAN_REGION_CN470_H_ */
