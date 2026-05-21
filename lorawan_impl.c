/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 *
 * Custom LoRaWAN Class A end-device stack.
 * Implements the API declared in <zephyr/lorawan/lorawan.h>
 * using the raw Zephyr LoRa driver (<zephyr/drivers/lora.h>).
 *
 * Based on:
 *   - LoRaWAN L2 1.0.4 Specification (TS001-1.0.4)
 *   - LoRaWAN Regional Parameters RP002-1.0.4
 *
 * Includes standalone AES-128 encryption + AES-CMAC (RFC4493)
 * for LoRaWAN cryptographic operations.
 */

#include <zephyr/lorawan/lorawan.h>
#include <zephyr/drivers/lora.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>
#include <string.h>
#include <zephyr/kvss/nvs.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <stdbool.h>
#include "region.h"

LOG_MODULE_REGISTER(lorawan_impl, CONFIG_LOG_DEFAULT_LEVEL);

/* -------------------------------------------------------------------------- */
/* AES-128 S-box (FIPS-197)                                                    */
/* -------------------------------------------------------------------------- */
static const uint8_t sbox[256] = {
	0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b,
	0xfe, 0xd7, 0xab, 0x76, 0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0,
	0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0, 0xb7, 0xfd, 0x93, 0x26,
	0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
	0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2,
	0xeb, 0x27, 0xb2, 0x75, 0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0,
	0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84, 0x53, 0xd1, 0x00, 0xed,
	0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
	0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f,
	0x50, 0x3c, 0x9f, 0xa8, 0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5,
	0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2, 0xcd, 0x0c, 0x13, 0xec,
	0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
	0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14,
	0xde, 0x5e, 0x0b, 0xdb, 0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c,
	0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79, 0xe7, 0xc8, 0x37, 0x6d,
	0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
	0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f,
	0x4b, 0xbd, 0x8b, 0x8a, 0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e,
	0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e, 0xe1, 0xf8, 0x98, 0x11,
	0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
	0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f,
	0xb0, 0x54, 0xbb, 0x16
};

static const uint8_t rcon[11] = {
	0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36
};

static void sub_bytes(uint8_t *state)
{
	for (int i = 0; i < 16; i++) {
		state[i] = sbox[state[i]];
	}
}

static void shift_rows(uint8_t *state)
{
	uint8_t tmp;

	/* row 1: shift left 1 */
	tmp = state[1];
	state[1] = state[5];
	state[5] = state[9];
	state[9] = state[13];
	state[13] = tmp;

	/* row 2: shift left 2 */
	tmp = state[2];
	state[2] = state[10];
	state[10] = tmp;
	tmp = state[6];
	state[6] = state[14];
	state[14] = tmp;

	/* row 3: shift left 3 */
	tmp = state[15];
	state[15] = state[11];
	state[11] = state[7];
	state[7] = state[3];
	state[3] = tmp;
}

static uint8_t xtime(uint8_t x)
{
	return ((x << 1) ^ (((x >> 7) & 1) * 0x1b));
}

static void mix_columns(uint8_t *state)
{
	uint8_t tmp, tm, t;

	for (int i = 0; i < 4; i++) {
		t = state[i * 4 + 0];
		tmp = state[i * 4 + 0] ^ state[i * 4 + 1] ^
		      state[i * 4 + 2] ^ state[i * 4 + 3];
		tm = state[i * 4 + 0] ^ state[i * 4 + 1];
		tm = xtime(tm);
		state[i * 4 + 0] ^= tm ^ tmp;
		tm = state[i * 4 + 1] ^ state[i * 4 + 2];
		tm = xtime(tm);
		state[i * 4 + 1] ^= tm ^ tmp;
		tm = state[i * 4 + 2] ^ state[i * 4 + 3];
		tm = xtime(tm);
		state[i * 4 + 2] ^= tm ^ tmp;
		tm = state[i * 4 + 3] ^ t;
		tm = xtime(tm);
		state[i * 4 + 3] ^= tm ^ tmp;
	}
}

static void add_round_key(uint8_t *state, const uint8_t *round_key)
{
	for (int i = 0; i < 16; i++) {
		state[i] ^= round_key[i];
	}
}

static void key_expansion(const uint8_t *key, uint8_t *round_keys)
{
	uint32_t temp;
	int i = 0;

	while (i < 4) {
		round_keys[i * 4 + 0] = key[i * 4 + 0];
		round_keys[i * 4 + 1] = key[i * 4 + 1];
		round_keys[i * 4 + 2] = key[i * 4 + 2];
		round_keys[i * 4 + 3] = key[i * 4 + 3];
		i++;
	}

	for (i = 4; i < 44; i++) {
		temp = ((uint32_t)round_keys[(i - 1) * 4 + 0] << 24) |
		       ((uint32_t)round_keys[(i - 1) * 4 + 1] << 16) |
		       ((uint32_t)round_keys[(i - 1) * 4 + 2] << 8) |
		       ((uint32_t)round_keys[(i - 1) * 4 + 3]);

		if (i % 4 == 0) {
			/* RotWord */
			temp = ((temp << 8) | (temp >> 24)) & 0xFFFFFFFFU;
			/* SubWord */
			temp = ((uint32_t)sbox[(temp >> 24) & 0xFF] << 24) |
			       ((uint32_t)sbox[(temp >> 16) & 0xFF] << 16) |
			       ((uint32_t)sbox[(temp >> 8) & 0xFF] << 8) |
			       ((uint32_t)sbox[(temp) & 0xFF]);
			temp ^= ((uint32_t)rcon[i / 4] << 24);
		}

		round_keys[i * 4 + 0] =
			round_keys[(i - 4) * 4 + 0] ^ ((temp >> 24) & 0xFF);
		round_keys[i * 4 + 1] =
			round_keys[(i - 4) * 4 + 1] ^ ((temp >> 16) & 0xFF);
		round_keys[i * 4 + 2] =
			round_keys[(i - 4) * 4 + 2] ^ ((temp >> 8) & 0xFF);
		round_keys[i * 4 + 3] =
			round_keys[(i - 4) * 4 + 3] ^ ((temp) & 0xFF);
	}
}

static void aes128_encrypt_block(const uint8_t key[16], const uint8_t in[16],
				 uint8_t out[16])
{
	uint8_t state[16];
	uint8_t round_keys[176];

	key_expansion(key, round_keys);

	for (int i = 0; i < 16; i++) {
		state[i] = in[i];
	}

	add_round_key(state, round_keys);

	for (int round = 1; round <= 9; round++) {
		sub_bytes(state);
		shift_rows(state);
		mix_columns(state);
		add_round_key(state, round_keys + round * 16);
	}

	sub_bytes(state);
	shift_rows(state);
	add_round_key(state, round_keys + 10 * 16);

	for (int i = 0; i < 16; i++) {
		out[i] = state[i];
	}
}

/* AES-CMAC (RFC4493) */
static const uint8_t rb[16] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x87
};

static void leftshift_onebit(const uint8_t *in, uint8_t *out)
{
	uint8_t overflow = 0;

	for (int i = 15; i >= 0; i--) {
		out[i] = (in[i] << 1) | overflow;
		overflow = (in[i] >> 7) & 1;
	}
}

static void generate_subkeys(const uint8_t key[16], uint8_t k1[16],
			     uint8_t k2[16])
{
	uint8_t l[16];
	uint8_t zero[16] = {0};

	aes128_encrypt_block(key, zero, l);

	leftshift_onebit(l, k1);
	if (l[0] & 0x80) {
		for (int i = 0; i < 16; i++) {
			k1[i] ^= rb[i];
		}
	}

	leftshift_onebit(k1, k2);
	if (k1[0] & 0x80) {
		for (int i = 0; i < 16; i++) {
			k2[i] ^= rb[i];
		}
	}
}

void aes128_cmac(const uint8_t key[16], const uint8_t *msg, size_t len,
		 uint8_t mac[16])
{
	uint8_t k1[16], k2[16];
	uint8_t block[16] = {0};
	uint8_t y[16] = {0};
	size_t n;
	bool last_complete;

	generate_subkeys(key, k1, k2);

	if (len == 0) {
		n = 1;
		last_complete = false;
	} else {
		n = (len + 15) / 16;
		last_complete = ((len % 16) == 0);
	}

	for (size_t i = 0; i < n - 1; i++) {
		for (int j = 0; j < 16; j++) {
			block[j] = y[j] ^ msg[i * 16 + j];
		}
		aes128_encrypt_block(key, block, y);
	}

	if (last_complete) {
		for (int j = 0; j < 16; j++) {
			block[j] = y[j] ^ msg[(n - 1) * 16 + j] ^ k1[j];
		}
	} else {
		size_t rem = len % 16;

		for (size_t j = 0; j < rem; j++) {
			block[j] = y[j] ^ msg[(n - 1) * 16 + j];
		}
		block[rem] = y[rem] ^ 0x80;
		for (size_t j = rem + 1; j < 16; j++) {
			block[j] = y[j];
		}
		for (int j = 0; j < 16; j++) {
			block[j] ^= k2[j];
		}
	}

	aes128_encrypt_block(key, block, mac);
}

/* LoRaWAN-specific crypto helpers */
void lorawan_mic_data(const uint8_t nwk_skey[16], uint8_t dir,
		      uint32_t dev_addr, uint32_t fcnt,
		      const uint8_t *msg, size_t len, uint8_t mic[4])
{
	uint8_t b0[16];
	uint8_t cmac[16];

	b0[0] = 0x49;
	b0[1] = 0x00;
	b0[2] = 0x00;
	b0[3] = 0x00;
	b0[4] = 0x00;
	b0[5] = dir;
	b0[6] = dev_addr & 0xFF;
	b0[7] = (dev_addr >> 8) & 0xFF;
	b0[8] = (dev_addr >> 16) & 0xFF;
	b0[9] = (dev_addr >> 24) & 0xFF;
	b0[10] = fcnt & 0xFF;
	b0[11] = (fcnt >> 8) & 0xFF;
	b0[12] = (fcnt >> 16) & 0xFF;
	b0[13] = (fcnt >> 24) & 0xFF;
	b0[14] = 0x00;
	b0[15] = (uint8_t)len;

	uint8_t buf[16 + 256];

	memcpy(buf, b0, 16);
	memcpy(buf + 16, msg, len);

	aes128_cmac(nwk_skey, buf, 16 + len, cmac);
	memcpy(mic, cmac, 4);
}

void lorawan_mic_join_req(const uint8_t app_key[16], const uint8_t *payload,
			  size_t len, uint8_t mic[4])
{
	uint8_t cmac[16];

	aes128_cmac(app_key, payload, len, cmac);
	memcpy(mic, cmac, 4);
}

void lorawan_mic_join_accept(const uint8_t app_key[16],
			     const uint8_t *payload, size_t len,
			     uint8_t mic[4])
{
	uint8_t cmac[16];

	aes128_cmac(app_key, payload, len, cmac);
	memcpy(mic, cmac, 4);
}

void lorawan_payload_encrypt(const uint8_t key[16], uint8_t dir,
			       uint32_t dev_addr, uint32_t fcnt,
			       uint8_t *payload, size_t len)
{
	size_t k = (len + 15) / 16;

	for (size_t i = 1; i <= k; i++) {
		uint8_t a[16];
		uint8_t s[16];

		a[0] = 0x01;
		a[1] = 0x00;
		a[2] = 0x00;
		a[3] = 0x00;
		a[4] = 0x00;
		a[5] = dir;
		a[6] = dev_addr & 0xFF;
		a[7] = (dev_addr >> 8) & 0xFF;
		a[8] = (dev_addr >> 16) & 0xFF;
		a[9] = (dev_addr >> 24) & 0xFF;
		a[10] = fcnt & 0xFF;
		a[11] = (fcnt >> 8) & 0xFF;
		a[12] = (fcnt >> 16) & 0xFF;
		a[13] = (fcnt >> 24) & 0xFF;
		a[14] = 0x00;
		a[15] = (uint8_t)i;

		aes128_encrypt_block(key, a, s);

		size_t block_len = (i == k) ? (len - (k - 1) * 16) : 16;

		for (size_t j = 0; j < block_len; j++) {
			payload[(i - 1) * 16 + j] ^= s[j];
		}
	}
}

void lorawan_join_accept_decrypt(const uint8_t app_key[16], uint8_t *payload,
				 size_t len)
{
	uint8_t block[16];

	for (size_t i = 0; i < len / 16; i++) {
		aes128_encrypt_block(app_key, payload + i * 16, block);
		memcpy(payload + i * 16, block, 16);
	}
}

void lorawan_derive_session_keys(const uint8_t app_key[16],
				 const uint8_t join_nonce[3],
				 const uint8_t net_id[3],
				 uint16_t dev_nonce,
				 uint8_t nwk_skey[16],
				 uint8_t app_skey[16])
{
	uint8_t buf[16];

	buf[0] = 0x01;
	buf[1] = join_nonce[0];
	buf[2] = join_nonce[1];
	buf[3] = join_nonce[2];
	buf[4] = net_id[0];
	buf[5] = net_id[1];
	buf[6] = net_id[2];
	buf[7] = dev_nonce & 0xFF;
	buf[8] = (dev_nonce >> 8) & 0xFF;
	memset(buf + 9, 0x00, 7);
	aes128_encrypt_block(app_key, buf, nwk_skey);

	buf[0] = 0x02;
	aes128_encrypt_block(app_key, buf, app_skey);
}

#define STORAGE_PARTITION storage_partition
#define NVS_DEV_NONCE_ID 1
static struct nvs_fs fs;
static bool storage_initialized = false;

/* -------------------------------------------------------------------------- */
/* Internal constants                                                          */
/* -------------------------------------------------------------------------- */
#if DT_NODE_HAS_STATUS(DT_ALIAS(lora0), okay)
#define LORA_NODE DT_ALIAS(lora0)
#elif DT_NODE_HAS_STATUS(DT_CHOSEN(zephyr_lorawan_transceiver), okay)
#define LORA_NODE DT_CHOSEN(zephyr_lorawan_transceiver)
#elif DT_NODE_HAS_STATUS(DT_NODELABEL(lora), okay)
#define LORA_NODE DT_NODELABEL(lora)
#else
#error "No enabled LoRa device found. Define alias lora0 or chosen zephyr,lorawan-transceiver."
#endif

#define LORA_DEV DEVICE_DT_GET(LORA_NODE)

#define MAX_PHY_PAYLOAD 255
#define RX1_DELAY_S     1
#define RX2_DELAY_S     2



/* MHDR field values */
#define MHDR_JOIN_REQUEST       0x00
#define MHDR_JOIN_ACCEPT        0x20
#define MHDR_UNCONFIRMED_UP     0x40
#define MHDR_UNCONFIRMED_DOWN   0x60
#define MHDR_CONFIRMED_UP       0x80
#define MHDR_CONFIRMED_DOWN     0xA0
#define MHDR_PROPRIETARY        0xE0

/* FCtrl bits (uplink) */
#define FCTRL_ADR       BIT(7)
#define FCTRL_ADRACKREQ BIT(6)
#define FCTRL_ACK       BIT(5)
#define FCTRL_CLASSB    BIT(4)
#define FCTRL_FOPTSLEN  0x0F

/* FCtrl bits (downlink) */
#define FCTRL_FPENDING  BIT(4)

/* -------------------------------------------------------------------------- */
/* Internal state                                                              */
/* -------------------------------------------------------------------------- */
enum lorawan_state {
	STATE_IDLE,
	STATE_JOINING,
	STATE_JOINED,
};

struct lorawan_ctx {
	bool started;
	enum lorawan_state state;
	enum lorawan_class dev_class;
	uint8_t last_tx_channel;

	/* Identity / keys */
	uint8_t dev_eui[8];
	uint8_t join_eui[8];
	uint8_t app_key[16];
	uint16_t dev_nonce;

	uint32_t dev_addr;
	uint8_t nwk_skey[16];
	uint8_t app_skey[16];

	/* Frame counters (32-bit internal, 16-bit on air) */
	uint32_t fcnt_up;
	uint32_t fcnt_down;

	/* Configuration */
	enum lorawan_datarate current_dr;
	enum lorawan_datarate default_dr;
	uint8_t join_dr;  /* Data rate for OTAA join (default: DR0) */
	bool adr_enabled;
	uint8_t conf_msg_tries;
	struct region_ctx region;

	/* Callbacks */
	lorawan_battery_level_cb_t battery_level_cb;
	lorawan_dr_changed_cb_t dr_changed_cb;
	lorawan_link_check_ans_cb_t link_check_cb;
	sys_slist_t dl_callbacks;
	transport_descriptor_cb frag_desc_cb;
	void (*frag_finished_cb)(void);

	/* Clock sync (stub) */
	bool clock_synced;
	uint32_t clock_gps_time;

	/* CN470 Regional spec compliance */
	int8_t last_snr;
	uint8_t rx1_dr_offset;
	bool downlink_ack_pending;
	uint8_t mac_rsp_buf[15];
	uint8_t mac_rsp_len;

	/* RXParamSetupReq overrides (0/0xFF = use region default) */
	uint32_t rx2_freq_override;
	uint8_t rx2_dr_override;

	/* MAC command state (downlink received) */
	uint8_t max_duty_cycle;
	uint8_t rx_timing_delay;
	bool uplink_dwell_time;
	bool downlink_dwell_time;
	uint32_t max_eirp;
	int8_t adr_power_index;    /* -1 = unset, 0-7 from LinkADRReq */
	uint8_t adr_limit_exp;
	uint8_t adr_delay_exp;
	uint8_t rejoin_periodicity;
	uint8_t rejoin_max_count;
	uint8_t rejoin_max_time;
	uint8_t minor_version;

	/* Uplink-initiated MAC command pending flags */
	bool reset_ind_pending;
	bool link_check_req_pending;
	bool device_time_req_pending;

	/* Dwelling channels (DlChannelReq) */
	uint32_t dl_channel_freq[16];
	bool dl_channel_valid[16];
};

static struct lorawan_ctx g_ctx;
static const struct device *lora_dev;

K_MUTEX_DEFINE(lorawan_mutex);

/* -------------------------------------------------------------------------- */
/* Helpers: RX2 parameter access (with override support)                       */
/* -------------------------------------------------------------------------- */
static uint32_t get_rx2_freq(void)
{
	if (g_ctx.rx2_freq_override != 0) {
		return g_ctx.rx2_freq_override;
	}
	return g_ctx.region.rx2_freq;
}

static uint8_t get_rx2_dr(void)
{
	if (g_ctx.rx2_dr_override != 0xFF) {
		return g_ctx.rx2_dr_override;
	}
	return g_ctx.region.rx2_dr;
}

/* -------------------------------------------------------------------------- */
/* Helpers: radio configuration                                                */
/* -------------------------------------------------------------------------- */
static int radio_configure_rx(uint32_t freq, uint8_t dr)
{
	struct lora_modem_config cfg = {
		.frequency = freq,
		.coding_rate = CR_4_5,
		.preamble_len = 8,
		.tx_power = g_ctx.region.default_tx_power,
		.tx = false,
		.iq_inverted = true,   /* LoRaWAN downlink uses inverted IQ */
		.public_network = true,
	};
	int ret;

	ret = region_dr_to_lora(&g_ctx.region, dr, &cfg.datarate, &cfg.bandwidth);
	if (ret < 0) {
		return ret;
	}

	return lora_config(lora_dev, &cfg);
}

static int8_t get_tx_power_dbm(void)
{
	int8_t dbm;

	/* Start with region default */
	dbm = (int8_t)g_ctx.region.default_tx_power;

	/* Apply LinkADR power index if set */
	if (g_ctx.adr_power_index >= 0 && g_ctx.adr_power_index < 8) {
		dbm = (int8_t)g_ctx.region.tx_power_map[g_ctx.adr_power_index];
	}

	/* Apply TxParamSetupReq max_eirp ceiling if set */
	if (g_ctx.max_eirp > 0) {
		int8_t ceiling = (int8_t)(6 + 2 * g_ctx.max_eirp);
		if (dbm > ceiling) {
			dbm = ceiling;
		}
	}

	return dbm;
}

static int radio_configure_tx(uint32_t freq, uint8_t dr)
{
	struct lora_modem_config cfg = {
		.frequency = freq,
		.coding_rate = CR_4_5,
		.preamble_len = 8,
		.tx_power = get_tx_power_dbm(),
		.tx = true,
		.iq_inverted = false,  /* LoRaWAN uplink uses normal IQ */
		.public_network = true,
	};
	int ret;

	ret = region_dr_to_lora(&g_ctx.region, dr, &cfg.datarate, &cfg.bandwidth);
	if (ret < 0) {
		LOG_ERR("region_dr_to_lora failed: dr=%u, ret=%d", dr, ret);
		return ret;
	}

	LOG_DBG("TX: freq=%u, dr=%u, power=%d dBm", freq, dr, cfg.tx_power);

	ret = lora_config(lora_dev, &cfg);
	if (ret < 0) {
		LOG_ERR("lora_config failed: freq=%u, dr=%u, ret=%d", freq, dr, ret);
	}
	return ret;
}

/* -------------------------------------------------------------------------- */
/* TX configure */
/* -------------------------------------------------------------------------- */
static void memcpyr(uint8_t *dst, const uint8_t *src, uint8_t len)
{
	for (uint8_t i = 0; i < len; i++) {
		dst[i] = src[len - 1 - i];
	}
}

static void build_fhdr(uint8_t *buf, uint32_t dev_addr, uint8_t fctrl,
		       uint16_t fcnt)
{
	buf[0] = dev_addr & 0xFF;
	buf[1] = (dev_addr >> 8) & 0xFF;
	buf[2] = (dev_addr >> 16) & 0xFF;
	buf[3] = (dev_addr >> 24) & 0xFF;
	buf[4] = fctrl;
	buf[5] = fcnt & 0xFF;
	buf[6] = (fcnt >> 8) & 0xFF;
}

/* -------------------------------------------------------------------------- */
/* Helpers: channel selection                                                   */
/* -------------------------------------------------------------------------- */
static const struct lorawan_channel *get_data_channel(void)
{
	/*
	 * Reuse region_get_join_channel for data uplink as well.
	 * It selects a random enabled channel from the region's channel list,
	 * which is the correct behavior for both join and data transmissions.
	 */
	return region_get_join_channel(&g_ctx.region);
}

/* -------------------------------------------------------------------------- */
/* RX window handling (Class A)                                                */
/* -------------------------------------------------------------------------- */
static int open_rx_window(uint32_t freq, uint8_t dr, uint8_t *buf,
			  size_t maxlen, int16_t *rssi, int8_t *snr,
			  uint16_t timeout_ms)
{
	int ret;

	ret = radio_configure_rx(freq, dr);
	if (ret < 0) {
		LOG_ERR("RX config failed: %d", ret);
		return ret;
	}

	ret = lora_recv(lora_dev, buf, maxlen, K_MSEC(timeout_ms), rssi, snr);
	return ret;
}

static void process_mac_commands(const uint8_t *payload, size_t len)
{
	size_t i = 0;

	while (i < len) {
		uint8_t cid = payload[i++];

		switch (cid) {
		case 0x01: { // ResetConf (LoRaWAN 1.0.4 §6.2)
			if (i + 1 <= len) {
				uint8_t serv_version = payload[i++] & 0x0F;
				if (serv_version == g_ctx.minor_version) {
					g_ctx.state = STATE_IDLE;
					LOG_INF("ResetConf accepted (version=%u), rejoining",
						serv_version);
				} else {
					LOG_WRN("ResetConf version mismatch: server=%u, local=%u",
						serv_version, g_ctx.minor_version);
				}
			} else {
				return;
			}
			break;
		}
		case 0x02: { // LinkCheckAns (LoRaWAN 1.0.4 §6.2)
			if (i + 2 <= len) {
				uint8_t margin = payload[i++];
				uint8_t gw_cnt = payload[i++];
				if (g_ctx.link_check_cb) {
					g_ctx.link_check_cb(margin, gw_cnt);
				}
			} else {
				return;
			}
			break;
		}
		case 0x03: { // LinkADRReq (LoRaWAN 1.0.4 §6.3)
			if (i + 4 <= len) {
				uint8_t dr_power = payload[i++];
				uint16_t ch_mask = ((uint16_t)payload[i++]);
				ch_mask |= ((uint16_t)payload[i++] << 8);
				uint8_t redundancy = payload[i++];

				uint8_t dr = (dr_power >> 4) & 0x0F;
				uint8_t power = dr_power & 0x0F;
				uint8_t ch_mask_cntl = (redundancy >> 4) & 0x07;
				uint8_t nb_rep = redundancy & 0x0F;
				ARG_UNUSED(nb_rep);

				uint8_t status = 0x07;

				if (dr != 0x0F && dr >= g_ctx.region.num_dr) {
					status &= ~0x02;
				}
				if (power != 0x0F && power > 7) {
					status &= ~0x04;
				}
				if (ch_mask_cntl > 6) {
				status &= ~0x01;
			}

			if ((status & 0x01) != 0) {
				int enabled_count = 0;
				uint8_t num_ch = g_ctx.region.num_channels;
				for (uint8_t c = 0; c < num_ch; c++) {
					bool en = g_ctx.region.channels[c].enabled;
					if (ch_mask_cntl <= 5) {
						int start_ch = ch_mask_cntl * 16;
						if (c >= start_ch && c < start_ch + 16) {
							en = (ch_mask & (1 << (c - start_ch))) ? true : false;
						}
					} else if (ch_mask_cntl == 6) {
						en = true;
					}
					/* ch_mask_cntl == 7 is RFU - ignore, keep current state */
					if (en) {
						enabled_count++;
					}
				}
				if (enabled_count == 0) {
					status &= ~0x01;
				}
			}

				if (status == 0x07) {
					if (dr != 0x0F) {
						g_ctx.current_dr = dr;
					}
					if (power != 0x0F && power <= 7) {
						g_ctx.adr_power_index = (int8_t)power;
						if (g_ctx.dr_changed_cb) {
							g_ctx.dr_changed_cb(g_ctx.current_dr);
						}
					}
					uint8_t num_ch = g_ctx.region.num_channels;
					for (uint8_t c = 0; c < num_ch; c++) {
						if (ch_mask_cntl <= 5) {
							int start_ch = ch_mask_cntl * 16;
							if (c >= start_ch && c < start_ch + 16) {
								g_ctx.region.channels[c].enabled = (ch_mask & (1 << (c - start_ch))) ? true : false;
							}
						} else if (ch_mask_cntl == 6) {
							g_ctx.region.channels[c].enabled = true;
						}
					}
				}

				if (g_ctx.mac_rsp_len + 2 <= sizeof(g_ctx.mac_rsp_buf)) {
					g_ctx.mac_rsp_buf[g_ctx.mac_rsp_len++] = 0x03;
					g_ctx.mac_rsp_buf[g_ctx.mac_rsp_len++] = status;
				} else {
					LOG_WRN("MAC response buffer full, cannot queue LinkADRAns");
				}
			} else {
				return;
			}
			break;
		}
		case 0x04: { // DutyCycleReq (LoRaWAN 1.0.4 §6.4)
			if (i + 1 <= len) {
				uint8_t duty_cycle = payload[i++] & 0x0F;
				g_ctx.max_duty_cycle = duty_cycle;
			} else {
				return;
			}
			if (g_ctx.mac_rsp_len + 1 <= sizeof(g_ctx.mac_rsp_buf)) {
				g_ctx.mac_rsp_buf[g_ctx.mac_rsp_len++] = 0x04;
			}
			break;
		}
		case 0x05: { // RXParamSetupReq (LoRaWAN 1.0.4 §6.5)
			if (i + 4 <= len) {
				uint8_t dl_settings = payload[i++];
				uint32_t freq_100hz = ((uint32_t)payload[i++]);
				freq_100hz |= ((uint32_t)payload[i++] << 8);
				freq_100hz |= ((uint32_t)payload[i++] << 16);
				uint32_t freq_hz = freq_100hz * 100;

				uint8_t rx1_dr_offset = (dl_settings >> 4) & 0x07;
				uint8_t rx2_dr = dl_settings & 0x0F;

				uint8_t status = 0x03;

				if (rx2_dr >= g_ctx.region.num_dr) {
					status &= ~0x02;
				}
				if (rx1_dr_offset >= g_ctx.region.num_dr) {
					status &= ~0x02;
				}
				if (freq_hz < g_ctx.region.freq_min_hz || freq_hz > g_ctx.region.freq_max_hz) {
					status &= ~0x01;
				}

				if (status == 0x03) {
				g_ctx.rx1_dr_offset = rx1_dr_offset;
				g_ctx.rx2_freq_override = freq_hz;
				g_ctx.rx2_dr_override = rx2_dr;
				LOG_INF("RXParamSetupReq applied: RX2 freq=%u, DR=%u, offset=%u",
					freq_hz, rx2_dr, rx1_dr_offset);
			}

				if (g_ctx.mac_rsp_len + 2 <= sizeof(g_ctx.mac_rsp_buf)) {
					g_ctx.mac_rsp_buf[g_ctx.mac_rsp_len++] = 0x05;
					g_ctx.mac_rsp_buf[g_ctx.mac_rsp_len++] = status;
				}
			} else {
				return;
			}
			break;
		}
		case 0x06: { // DevStatusReq (LoRaWAN 1.0.4 §6.6)
			uint8_t batt = 255;
			if (g_ctx.battery_level_cb) {
				batt = g_ctx.battery_level_cb();
			}
			int8_t margin = g_ctx.last_snr;
			if (margin < -32) margin = -32;
			if (margin > 31) margin = 31;
			uint8_t margin_byte = (uint8_t)(margin & 0x3F);

			if (g_ctx.mac_rsp_len + 3 <= sizeof(g_ctx.mac_rsp_buf)) {
				g_ctx.mac_rsp_buf[g_ctx.mac_rsp_len++] = 0x06;
				g_ctx.mac_rsp_buf[g_ctx.mac_rsp_len++] = batt;
				g_ctx.mac_rsp_buf[g_ctx.mac_rsp_len++] = margin_byte;
			}
			break;
		}
		case 0x07: { // NewChannelReq (LoRaWAN 1.0.4 §6.7)
			if (i + 5 <= len) {
				uint8_t ch_idx = payload[i++];
				uint32_t freq_100hz = ((uint32_t)payload[i++]);
				freq_100hz |= ((uint32_t)payload[i++] << 8);
				freq_100hz |= ((uint32_t)payload[i++] << 16);
				uint32_t freq_hz = freq_100hz * 100;
				uint8_t dr_range = payload[i++];
				uint8_t max_dr = (dr_range >> 4) & 0x0F;
				uint8_t min_dr = dr_range & 0x0F;

				uint8_t status = 0x03;

				if (ch_idx >= g_ctx.region.max_channels) {
					status &= ~0x01;
				}
				if (freq_hz == 0) {
					status &= ~0x01;
				}
				if (freq_hz < g_ctx.region.freq_min_hz || freq_hz > g_ctx.region.freq_max_hz) {
					status &= ~0x01;
				}
				if (min_dr > max_dr || min_dr >= g_ctx.region.num_dr) {
					status &= ~0x02;
				}

				if (status == 0x03) {
					if (ch_idx < 96) {
						g_ctx.region.channels[ch_idx].freq_hz = freq_hz;
						g_ctx.region.channels[ch_idx].min_dr = min_dr;
						g_ctx.region.channels[ch_idx].max_dr = max_dr;
					}
				}

				if (g_ctx.mac_rsp_len + 2 <= sizeof(g_ctx.mac_rsp_buf)) {
					g_ctx.mac_rsp_buf[g_ctx.mac_rsp_len++] = 0x07;
					g_ctx.mac_rsp_buf[g_ctx.mac_rsp_len++] = status;
				}
			} else {
				return;
			}
			break;
		}
		case 0x08: { // RXTimingSetupReq (LoRaWAN 1.0.4 §6.8)
			if (i + 1 <= len) {
				uint8_t delay = payload[i++] & 0x0F;
				/* Store in rx_timing_delay (0 = use region default of 1s) */
				g_ctx.rx_timing_delay = delay;
				uint8_t rx1_delay = (delay == 0) ? 1 : delay;
				LOG_INF("RXTimingSetupReq: RX1 delay = %u s, RX2 delay = %u s",
					rx1_delay, rx1_delay + 1);
			} else {
				return;
			}
			if (g_ctx.mac_rsp_len + 1 <= sizeof(g_ctx.mac_rsp_buf)) {
				g_ctx.mac_rsp_buf[g_ctx.mac_rsp_len++] = 0x08;
			}
			break;
		}
		case 0x09: { // TxParamSetupReq (LoRaWAN 1.0.4 §6.9)
			if (i + 1 <= len) {
				uint8_t tx_param = payload[i++];
				uint8_t max_eirp = tx_param & 0x0F;
				bool uplink_dwell = (tx_param >> 4) & 0x01;
				bool dl_dwell = (tx_param >> 5) & 0x01;

				g_ctx.max_eirp = max_eirp;
				g_ctx.uplink_dwell_time = uplink_dwell;
				g_ctx.downlink_dwell_time = dl_dwell;
				LOG_INF("TxParamSetupReq: MaxEIRP=%u, UplinkDwell=%d, DownlinkDwell=%d",
					max_eirp, uplink_dwell, dl_dwell);
			} else {
				return;
			}
			if (g_ctx.mac_rsp_len + 1 <= sizeof(g_ctx.mac_rsp_buf)) {
				g_ctx.mac_rsp_buf[g_ctx.mac_rsp_len++] = 0x09;
			}
			break;
		}
		case 0x0A: { // DlChannelReq (LoRaWAN 1.0.4 §6.10)
			if (i + 4 <= len) {
				uint8_t ch_idx = payload[i++];
				uint32_t freq_100hz = ((uint32_t)payload[i++]);
				freq_100hz |= ((uint32_t)payload[i++] << 8);
				freq_100hz |= ((uint32_t)payload[i++] << 16);
				uint32_t freq_hz = freq_100hz * 100;

				uint8_t status = 0x03;

				if (ch_idx >= 16) {
					status &= ~0x01;
				}
				if (freq_hz < g_ctx.region.freq_min_hz || freq_hz > g_ctx.region.freq_max_hz) {
					status &= ~0x03;
				}

				if ((status & 0x01) != 0 && ch_idx < 16) {
					g_ctx.dl_channel_freq[ch_idx] = freq_hz;
					g_ctx.dl_channel_valid[ch_idx] = true;
				}

				if (g_ctx.mac_rsp_len + 2 <= sizeof(g_ctx.mac_rsp_buf)) {
					g_ctx.mac_rsp_buf[g_ctx.mac_rsp_len++] = 0x0A;
					g_ctx.mac_rsp_buf[g_ctx.mac_rsp_len++] = status;
				}
			} else {
				return;
			}
			break;
		}
		case 0x0C: { // ADRParamSetupReq (LoRaWAN 1.0.4 §6.12)
			if (i + 1 <= len) {
				uint8_t param = payload[i++];
				g_ctx.adr_limit_exp = (param >> 4) & 0x0F;
				g_ctx.adr_delay_exp = param & 0x0F;
				LOG_INF("ADRParamSetupReq: LimitExp=%u, DelayExp=%u",
					g_ctx.adr_limit_exp,
					g_ctx.adr_delay_exp);
			} else {
				return;
			}
			if (g_ctx.mac_rsp_len + 1 <= sizeof(g_ctx.mac_rsp_buf)) {
				g_ctx.mac_rsp_buf[g_ctx.mac_rsp_len++] = 0x0C;
			}
			break;
		}
		case 0x0D: { // DeviceTimeAns (LoRaWAN 1.0.4 §6.13)
			if (i + 5 <= len) {
				uint32_t seconds = ((uint32_t)payload[i++]);
				seconds |= ((uint32_t)payload[i++] << 8);
				seconds |= ((uint32_t)payload[i++] << 16);
				seconds |= ((uint32_t)payload[i++] << 24);
				uint8_t fractional = payload[i++];

				g_ctx.clock_synced = true;
				g_ctx.clock_gps_time = seconds;
				LOG_INF("DeviceTimeAns: GPS time = %u s + %u/256 s",
					seconds, fractional);
			} else {
				return;
			}
			break;
		}
		case 0x0E: { // ForceRejoinReq (LoRaWAN 1.0.4 §6.14)
			if (i + 2 <= len) {
				uint16_t rejoin_req = ((uint16_t)payload[i++]);
				rejoin_req |= ((uint16_t)payload[i++] << 8);

				g_ctx.rejoin_periodicity = rejoin_req & 0x07;
				g_ctx.rejoin_max_count = (rejoin_req >> 3) & 0x07;
				uint8_t rejoin_dr = (rejoin_req >> 7) & 0x1F;

				LOG_WRN("ForceRejoinReq: period=%u, maxRetries=%u, DR=%u",
					g_ctx.rejoin_periodicity,
					g_ctx.rejoin_max_count, rejoin_dr);

				g_ctx.state = STATE_IDLE;
				LOG_INF("Forced rejoin triggered");
			} else {
				return;
			}
			break;
		}
		case 0x0F: { // RejoinParamSetupReq (LoRaWAN 1.0.4 §6.15)
			if (i + 1 <= len) {
				uint8_t param = payload[i++];
				g_ctx.rejoin_max_time = param & 0x0F;
				g_ctx.rejoin_max_count = (param >> 4) & 0x0F;

				LOG_INF("RejoinParamSetupReq: MaxTime=%u, MaxCount=%u",
					g_ctx.rejoin_max_time,
					g_ctx.rejoin_max_count);
			} else {
				return;
			}
			if (g_ctx.mac_rsp_len + 2 <= sizeof(g_ctx.mac_rsp_buf)) {
				g_ctx.mac_rsp_buf[g_ctx.mac_rsp_len++] = 0x0F;
				g_ctx.mac_rsp_buf[g_ctx.mac_rsp_len++] = 0x01;
			}
			break;
		}
		default:
			return;
		}
	}
}

/* -------------------------------------------------------------------------- */
/* Downlink processing                                                         */
/* -------------------------------------------------------------------------- */
static void process_downlink(uint8_t *phy, size_t len, int16_t rssi,
			     int8_t snr, bool *ack_received)
{
	if (len < 12) {
		LOG_WRN("Downlink too short: %zu bytes", len);
		return;
	}

	uint8_t mhdr = phy[0];
	uint8_t ftype = mhdr & 0xE0;

	if (ftype != MHDR_UNCONFIRMED_DOWN &&
	    ftype != MHDR_CONFIRMED_DOWN) {
		return;
	}

	uint32_t dev_addr = ((uint32_t)phy[1]) | (((uint32_t)phy[2]) << 8) |
			    (((uint32_t)phy[3]) << 16) | (((uint32_t)phy[4]) << 24);
	if (dev_addr != g_ctx.dev_addr) {
		return;
	}

	uint8_t fctrl = phy[5];
	uint16_t fcnt = ((uint16_t)phy[6]) | (((uint16_t)phy[7]) << 8);
	uint8_t fopts_len = fctrl & FCTRL_FOPTSLEN;

	if (len < (8 + fopts_len + 4)) {
		return;
	}

	uint32_t reconstructed_fcnt;
	uint32_t last_fcnt = g_ctx.fcnt_down;
	uint16_t last_fcnt_lsb = (uint16_t)(last_fcnt & 0xFFFF);

	/* 32-bit roll-over sequence number reconstruction */
	uint16_t diff = fcnt - last_fcnt_lsb;
	if (diff < 32768) {
		reconstructed_fcnt = last_fcnt + diff;
	} else {
		reconstructed_fcnt = last_fcnt - (65536 - diff);
	}

	// Replay Attack Protection: Ensure reconstructed FCnt is strictly greater
	if (last_fcnt > 0 && reconstructed_fcnt <= last_fcnt) {
		LOG_WRN("Downlink dropped: Replay or out-of-sequence (received: %u, last: %u, reconstructed: %u)",
			fcnt, last_fcnt, reconstructed_fcnt);
		return;
	}

	/* MIC check using reconstructed 32-bit counter */
	uint8_t mic[4];
	uint8_t rx_mic[4];

	memcpy(rx_mic, phy + len - 4, 4);

	lorawan_mic_data(g_ctx.nwk_skey, 1, dev_addr, reconstructed_fcnt,
			 phy, len - 4, mic);

	if (memcmp(mic, rx_mic, 4) != 0) {
		LOG_WRN("Downlink MIC mismatch");
		return;
	}

	/* Update FCntDown internally only after MIC passes */
	g_ctx.fcnt_down = reconstructed_fcnt;
	g_ctx.last_snr = snr;

	if (ack_received != NULL && (fctrl & FCTRL_ACK) != 0) {
		*ack_received = true;
	}
	if (ftype == MHDR_CONFIRMED_DOWN) {
		g_ctx.downlink_ack_pending = true;
	}

	/* Parse FOpts if present */
	if (fopts_len > 0) {
		process_mac_commands(phy + 8, fopts_len);
	}

	/* Parse FPort / FRMPayload */
	size_t hdr_len = 8 + fopts_len;
	uint8_t *payload = NULL;
	size_t payload_len = 0;
	uint8_t port = 0;
	uint8_t flags = 0;

	if (len > hdr_len + 4) {
		port = phy[hdr_len];
		payload = &phy[hdr_len + 1];
		payload_len = len - hdr_len - 1 - 4;

		/* Decrypt payload using reconstructed 32-bit counter */
		uint8_t key[16];

		memcpy(key, port == 0 ? g_ctx.nwk_skey : g_ctx.app_skey, 16);
		lorawan_payload_encrypt(key, 1, dev_addr, reconstructed_fcnt,
					payload, payload_len);

		/* If port is 0, the payload is raw MAC commands */
		if (port == 0 && payload_len > 0) {
			process_mac_commands(payload, payload_len);
		}
	}

	if (fctrl & FCTRL_FPENDING) {
		flags |= LORAWAN_DATA_PENDING;
	}

	/* Dispatch callbacks */
	struct lorawan_downlink_cb *cb;

	SYS_SLIST_FOR_EACH_CONTAINER(&g_ctx.dl_callbacks, cb, node) {
		if ((cb->port == LW_RECV_PORT_ANY) ||
		    (cb->port == port)) {
			cb->cb(port, flags, rssi, snr, payload_len,
			       payload);
		}
	}
}

/* -------------------------------------------------------------------------- */
/* OTAA Join procedure - optimized for precise RX window timing                */
/* -------------------------------------------------------------------------- */

/* Join RX window delays (per LoRaWAN spec) */
#define JOIN_RX1_DELAY_MS      5000  /* RX1 opens 5s after TX end */
#define JOIN_RX2_DELAY_MS      6000  /* RX2 opens 6s after TX end */
#define JOIN_RX_EARLY_MS       100   /* Open slightly early to compensate setup */
#define JOIN_RX_TIMEOUT_MS     3000  /* Join uses DR0, max ToA ~1400ms */

static int send_join_request(uint8_t *rx_buf, size_t rx_buflen)
{
	uint8_t payload[23]; /* MHDR(1) + JoinEUI(8) + DevEUI(8) + DevNonce(2) + MIC(4) */
	uint8_t *p = payload;
	int ret;

	*p++ = MHDR_JOIN_REQUEST;
	memcpyr(p, g_ctx.join_eui, 8);
	p += 8;
	memcpyr(p, g_ctx.dev_eui, 8);
	p += 8;
	*p++ = g_ctx.dev_nonce & 0xFF;
	*p++ = (g_ctx.dev_nonce >> 8) & 0xFF;

	lorawan_mic_join_req(g_ctx.app_key, payload, 19, p);

	/* Pick a join channel */
	const struct lorawan_channel *ch = region_get_join_channel(&g_ctx.region);
	g_ctx.last_tx_channel = (uint8_t)(ch - g_ctx.region.channels);

	/* Pre-compute RX1 parameters BEFORE TX */
	uint32_t rx1_freq = region_get_rx1_freq(&g_ctx.region, g_ctx.last_tx_channel);
	uint8_t rx1_dr = region_get_rx1_dr(&g_ctx.region, g_ctx.join_dr, 0);
	uint32_t rx2_freq = get_rx2_freq();
	uint8_t rx2_dr = get_rx2_dr();

	ret = radio_configure_tx(ch->freq_hz, g_ctx.join_dr);
	if (ret < 0) {
		return ret;
	}

	/* ---- TX ---- */
	ret = lora_send(lora_dev, payload, sizeof(payload));
	if (ret < 0) {
		LOG_ERR("Join-Request TX failed: %d", ret);
		return ret;
	}

	int64_t tx_end_time = k_uptime_get();

	/* ---- Fast bookkeeping (no I/O) ---- */
	LOG_INF("Join-Request sent on %u Hz (DevNonce=%u)",
		ch->freq_hz, g_ctx.dev_nonce);
	g_ctx.dev_nonce++;
	if (g_ctx.dev_nonce == 0) {
		LOG_WRN("DevNonce rolled over to 0 — reusing nonces is a security risk");
	}

	/* ---- RX windows: only k_msleep between TX and recv ---- */
	int16_t rssi;
	int8_t snr;
	int rx_len;

	/* RX1 window */
	int64_t rx1_target = tx_end_time + JOIN_RX1_DELAY_MS - JOIN_RX_EARLY_MS;
	int64_t now = k_uptime_get();
	if (now < rx1_target) {
		k_msleep(rx1_target - now);
	}
	rx_len = open_rx_window(rx1_freq, rx1_dr,
				rx_buf, rx_buflen, &rssi, &snr, JOIN_RX_TIMEOUT_MS);
	if (rx_len > 0) {
		LOG_INF("Join-Accept received in RX1 window");
		/* Persist DevNonce AFTER successful join (not before) */
		if (storage_initialized) {
			nvs_write(&fs, NVS_DEV_NONCE_ID, &g_ctx.dev_nonce, sizeof(g_ctx.dev_nonce));
		}
		return rx_len;
	}

	/* RX2 window */
	int64_t rx2_target = tx_end_time + JOIN_RX2_DELAY_MS - JOIN_RX_EARLY_MS;
	now = k_uptime_get();
	if (now < rx2_target) {
		k_msleep(rx2_target - now);
	}
	rx_len = open_rx_window(rx2_freq, rx2_dr,
				rx_buf, rx_buflen, &rssi, &snr, JOIN_RX_TIMEOUT_MS);
	if (rx_len > 0) {
		LOG_INF("Join-Accept received in RX2 window");
		if (storage_initialized) {
			nvs_write(&fs, NVS_DEV_NONCE_ID, &g_ctx.dev_nonce, sizeof(g_ctx.dev_nonce));
		}
	}
	return rx_len;
}

/* -------------------------------------------------------------------------- */
/* API implementation                                                          */
/* -------------------------------------------------------------------------- */
void lorawan_register_battery_level_callback(lorawan_battery_level_cb_t cb)
{
	g_ctx.battery_level_cb = cb;
}

void lorawan_register_downlink_callback(struct lorawan_downlink_cb *cb)
{
	sys_slist_append(&g_ctx.dl_callbacks, &cb->node);
}

void lorawan_register_dr_changed_callback(lorawan_dr_changed_cb_t cb)
{
	g_ctx.dr_changed_cb = cb;
}

void lorawan_register_link_check_ans_callback(lorawan_link_check_ans_cb_t cb)
{
	g_ctx.link_check_cb = cb;
}

int lorawan_join(const struct lorawan_join_config *config)
{
	int ret = 0;

	k_mutex_lock(&lorawan_mutex, K_FOREVER);

	if (!g_ctx.started) {
		ret = -EINVAL;
		goto out;
	}

	memcpy(g_ctx.dev_eui, config->dev_eui, 8);

	if (config->mode == LORAWAN_ACT_OTAA) {
		memcpy(g_ctx.join_eui, config->otaa.join_eui, 8);
		memcpy(g_ctx.app_key, config->otaa.app_key, 16);
		if (storage_initialized) {
			LOG_INF("Using NVS persisted DevNonce: %u", g_ctx.dev_nonce);
		} else {
			g_ctx.dev_nonce = config->otaa.dev_nonce;
			LOG_INF("Using config DevNonce (NVS unavailable): %u",
				g_ctx.dev_nonce);
		}
		g_ctx.state = STATE_JOINING;

		uint8_t rx_buf[33]; /* Join-Accept max size */
		int rx_len = send_join_request(rx_buf, sizeof(rx_buf));

		if (rx_len < 0) {
			ret = rx_len;
			goto out;
		}
		if (rx_len < 17) {
			LOG_ERR("Join-Accept too short (%d bytes)", rx_len);
			ret = -EIO;
			goto out;
		}

		/* Decrypt Join-Accept (server used AES-decrypt, we use AES-encrypt) */
		uint8_t *ja = rx_buf + 1; /* skip MHDR */
		size_t ja_len = rx_len - 1;

		lorawan_join_accept_decrypt(g_ctx.app_key, ja, ja_len);

		/* Verify MIC */
		uint8_t mic[4];

		lorawan_mic_join_accept(g_ctx.app_key, rx_buf, rx_len - 4, mic);
		if (memcmp(mic, rx_buf + rx_len - 4, 4) != 0) {
			LOG_ERR("Join-Accept MIC mismatch");
			ret = -EIO;
			goto out;
		}

		/* Parse Join-Accept */
		uint8_t join_nonce[3];
		uint8_t net_id[3];
		uint32_t dev_addr;
		uint16_t dev_nonce_le = (g_ctx.dev_nonce > 0) ?
					 (g_ctx.dev_nonce - 1) : 0xFFFF;

		join_nonce[0] = ja[0];
		join_nonce[1] = ja[1];
		join_nonce[2] = ja[2];
		net_id[0] = ja[3];
		net_id[1] = ja[4];
		net_id[2] = ja[5];
		dev_addr = ((uint32_t)ja[6]) | (((uint32_t)ja[7]) << 8) |
			   (((uint32_t)ja[8]) << 16) | (((uint32_t)ja[9]) << 24);

		g_ctx.dev_addr = dev_addr;
		lorawan_derive_session_keys(g_ctx.app_key, join_nonce, net_id,
					    dev_nonce_le, g_ctx.nwk_skey,
					    g_ctx.app_skey);
		g_ctx.fcnt_up = 0;
		g_ctx.fcnt_down = 0;
		g_ctx.state = STATE_JOINED;

		LOG_INF("Joined! DevAddr=%08x", dev_addr);

		if (g_ctx.dr_changed_cb != NULL) {
			g_ctx.dr_changed_cb(g_ctx.current_dr);
		}
	} else if (config->mode == LORAWAN_ACT_ABP) {
		g_ctx.dev_addr = config->abp.dev_addr;
		memcpy(g_ctx.nwk_skey, config->abp.nwk_skey, 16);
		memcpy(g_ctx.app_skey, config->abp.app_skey, 16);
		g_ctx.fcnt_up = 0;
		g_ctx.fcnt_down = 0;
		g_ctx.state = STATE_JOINED;
		LOG_INF("ABP activated! DevAddr=%08x", g_ctx.dev_addr);
	} else {
		ret = -EINVAL;
	}

out:
	k_mutex_unlock(&lorawan_mutex);
	return ret;
}

int lorawan_start(void)
{
	if (!device_is_ready(lora_dev)) {
		LOG_ERR("LoRa device not ready");
		return -ENODEV;
	}

	k_mutex_lock(&lorawan_mutex, K_FOREVER);

	/* Save region config across reset (set by lorawan_set_region before start) */
	struct region_ctx saved_region_ctx;
	bool region_was_set = g_ctx.region.num_channels > 0;
	if (region_was_set) {
		memcpy(&saved_region_ctx, &g_ctx.region, sizeof(g_ctx.region));
	}

	memset(&g_ctx, 0, sizeof(g_ctx));
	sys_slist_init(&g_ctx.dl_callbacks);

	/* Restore region config */
	if (region_was_set) {
		memcpy(&g_ctx.region, &saved_region_ctx, sizeof(g_ctx.region));
	}

	g_ctx.started = true;
	g_ctx.dev_class = LORAWAN_CLASS_A;
	g_ctx.current_dr = LORAWAN_DR_0;
	g_ctx.default_dr = LORAWAN_DR_0;
	g_ctx.join_dr = LORAWAN_DR_0;  /* Default join DR (can be changed via API) */
	g_ctx.conf_msg_tries = 1;
	g_ctx.minor_version = 4;

	/* Initialize MAC command state */
	g_ctx.rx2_freq_override = 0;      /* 0 = use region default */
	g_ctx.rx2_dr_override = 0xFF;     /* 0xFF = use region default */
	g_ctx.rx_timing_delay = 0;        /* 0 = use region default (1s) */
	g_ctx.rx1_dr_offset = 0;
	g_ctx.max_duty_cycle = 0;
	g_ctx.max_eirp = 0;
	g_ctx.adr_power_index = -1;
	g_ctx.uplink_dwell_time = false;
	g_ctx.downlink_dwell_time = false;

	/* Initialize persistent storage for DevNonce inside stack */
	struct flash_pages_info info;
	const struct device *flash_dev;
	int rc;

	flash_dev = PARTITION_DEVICE(STORAGE_PARTITION);
	if (device_is_ready(flash_dev)) {
		fs.flash_device = flash_dev;
		fs.offset = PARTITION_OFFSET(STORAGE_PARTITION);
		/* Suppress deprecation warnings - these macros are still
		 * widely used in Zephyr 4.4 and will be updated when
		 * the flash_map API stabilizes.
		 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

		rc = flash_get_page_info_by_offs(flash_dev, fs.offset, &info);
		if (rc == 0) {
			fs.sector_size = info.size;
			fs.sector_count = 3U;

			rc = nvs_mount(&fs);
			if (rc == 0) {
				uint16_t loaded_nonce = 0;
				rc = nvs_read(&fs, NVS_DEV_NONCE_ID, &loaded_nonce, sizeof(loaded_nonce));
				if (rc < 0) {
					loaded_nonce = 0;
					nvs_write(&fs, NVS_DEV_NONCE_ID, &loaded_nonce, sizeof(loaded_nonce));
					LOG_INF("Initialized persistent DevNonce in stack to 0");
				} else {
					LOG_INF("Loaded persistent DevNonce in stack: %u", loaded_nonce);
				}
				g_ctx.dev_nonce = loaded_nonce;
				storage_initialized = true;
			} else {
				LOG_ERR("NVS Mount failed inside stack: %d", rc);
			}
		} else {
			LOG_ERR("Unable to get flash page info inside stack: %d", rc);
		}
	} else {
		LOG_ERR("MTD flash device not ready inside stack for persistent storage");
	}
#pragma GCC diagnostic pop

	k_mutex_unlock(&lorawan_mutex);

	LOG_INF("Custom LoRaWAN stack started (Class A)");
	return 0;
}

static void append_uplink_mac_cmds(void)
{
	uint8_t *p = g_ctx.mac_rsp_buf + g_ctx.mac_rsp_len;
	int remaining = (int)sizeof(g_ctx.mac_rsp_buf) - (int)g_ctx.mac_rsp_len;

	if (g_ctx.reset_ind_pending && remaining >= 2) {
		*p++ = 0x01;
		*p++ = g_ctx.minor_version;
		g_ctx.mac_rsp_len += 2;
		remaining -= 2;
		g_ctx.reset_ind_pending = false;
	}

	if (g_ctx.link_check_req_pending && remaining >= 1) {
		*p++ = 0x02;
		g_ctx.mac_rsp_len += 1;
		remaining -= 1;
		g_ctx.link_check_req_pending = false;
	}

	if (g_ctx.device_time_req_pending && remaining >= 1) {
		*p++ = 0x0D;
		g_ctx.mac_rsp_len += 1;
		remaining -= 1;
		g_ctx.device_time_req_pending = false;
	}
}

int lorawan_send(uint8_t port, uint8_t *data, uint8_t len,
		 enum lorawan_message_type type)
{
	uint8_t tx_buf[MAX_PHY_PAYLOAD];
	uint8_t fctrl = 0;
	uint8_t *p = tx_buf;
	uint8_t mac_rsp_len;
	int ret;
	bool confirmed = (type == LORAWAN_MSG_CONFIRMED);
	bool sent_downlink_ack = false;
	bool confirmed_ack_received = false;

	if (len > 0 && port == 0) {
		return -EINVAL;
	}

	k_mutex_lock(&lorawan_mutex, K_FOREVER);

	if (g_ctx.state != STATE_JOINED) {
		ret = -EAGAIN;
		goto out;
	}

	int max_len = region_get_max_payload(&g_ctx.region, g_ctx.current_dr);
	if (max_len >= 0 && len > max_len) {
		LOG_ERR("Payload size %d exceeds max allowed %d for DR%d", len, max_len, g_ctx.current_dr);
		ret = -EMSGSIZE;
		goto out;
	}

	append_uplink_mac_cmds();

	if (g_ctx.adr_enabled) {
		fctrl |= FCTRL_ADR;
	}

	if (g_ctx.downlink_ack_pending) {
		fctrl |= FCTRL_ACK;
		sent_downlink_ack = true;
	}

	mac_rsp_len = g_ctx.mac_rsp_len;
	if (mac_rsp_len > 0) {
		fctrl |= (mac_rsp_len & FCTRL_FOPTSLEN);
	}

	/* Build uplink frame */
	*p++ = confirmed ? MHDR_CONFIRMED_UP : MHDR_UNCONFIRMED_UP;

	uint16_t fcnt = g_ctx.fcnt_up & 0xFFFF;

	build_fhdr(p, g_ctx.dev_addr, fctrl, fcnt);
	p += 7;

	if (mac_rsp_len > 0) {
		memcpy(p, g_ctx.mac_rsp_buf, mac_rsp_len);
		p += mac_rsp_len;
	}

	if (len > 0) {
		*p++ = port;
		memcpy(p, data, len);
		/* FPort 0: MAC commands encrypted with NwkSKey */
		/* FPort 1-255: payload encrypted with AppSKey */
		const uint8_t *enc_key = (port == 0) ? g_ctx.nwk_skey : g_ctx.app_skey;
		lorawan_payload_encrypt(enc_key, 0, g_ctx.dev_addr,
					g_ctx.fcnt_up, p, len);
		p += len;
	}

	size_t msg_len = p - tx_buf;
	uint8_t mic[4];

	lorawan_mic_data(g_ctx.nwk_skey, 0, g_ctx.dev_addr, g_ctx.fcnt_up,
			 tx_buf, msg_len, mic);
	memcpy(p, mic, 4);
	p += 4;

	size_t phy_len = p - tx_buf;

	/* Configure TX on current channel and DR */
	const struct lorawan_channel *ch = get_data_channel();
	g_ctx.last_tx_channel = (uint8_t)(ch - g_ctx.region.channels);

	ret = radio_configure_tx(ch->freq_hz, g_ctx.current_dr);
	if (ret < 0) {
		goto out;
	}

	/* ---- Pre-compute all RX window parameters BEFORE TX ---- */
	uint8_t rx1_dr = 0;
	uint32_t rx1_freq = 0;
	uint16_t rx1_timeout = 0;
	uint32_t rx2_freq = 0;
	uint8_t rx2_dr = 0;
	uint16_t rx2_timeout = 0;
	int64_t rx1_delay_ms = 0;
	int64_t rx2_delay_ms = 0;
	bool open_rx = false;

	if (g_ctx.dev_class == LORAWAN_CLASS_A) {
		rx1_dr = region_get_rx1_dr(&g_ctx.region,
					   g_ctx.current_dr, g_ctx.rx1_dr_offset);
		rx1_freq = region_get_rx1_freq(&g_ctx.region,
						g_ctx.last_tx_channel);
		rx2_freq = get_rx2_freq();
		rx2_dr = get_rx2_dr();
		rx1_timeout = region_get_rx_window_timeout_ms(rx1_dr) + 100;
		rx2_timeout = region_get_rx_window_timeout_ms(rx2_dr) + 100;
		/* Use rx_timing_delay if set by RXTimingSetupReq, else region default */
		uint8_t rx1_delay = (g_ctx.rx_timing_delay != 0) ?
				    g_ctx.rx_timing_delay : g_ctx.region.rx1_delay_s;
		rx1_delay_ms = rx1_delay * 1000 - 100;
		rx2_delay_ms = (rx1_delay + 1) * 1000 - 100;  /* RX2 = RX1 + 1s */
		open_rx = true;
	}

	/* ---- TX ---- */
	ret = lora_send(lora_dev, tx_buf, phy_len);
	if (ret < 0) {
		LOG_ERR("Uplink TX failed: %d", ret);
		goto out;
	}

	int64_t tx_end_time = k_uptime_get();

	/* ---- Post-TX bookkeeping (fast, no I/O) ---- */
	g_ctx.fcnt_up++;
	if (g_ctx.fcnt_up == 0) {
		LOG_WRN("FCntUp rolled over — network server may reject subsequent frames");
	}
	if (sent_downlink_ack) {
		g_ctx.downlink_ack_pending = false;
	}
	if (mac_rsp_len > 0) {
		if (g_ctx.mac_rsp_len > mac_rsp_len) {
			memmove(g_ctx.mac_rsp_buf, g_ctx.mac_rsp_buf + mac_rsp_len,
				g_ctx.mac_rsp_len - mac_rsp_len);
			g_ctx.mac_rsp_len -= mac_rsp_len;
		} else {
			g_ctx.mac_rsp_len = 0;
		}
	}

	/* ---- RX windows: only k_msleep between TX and recv ---- */
	if (open_rx) {
		uint8_t rx_buf[MAX_PHY_PAYLOAD];
		int16_t rssi;
		int8_t snr;
		int rx_len;

		/* RX1 window */
		int64_t rx1_target = tx_end_time + rx1_delay_ms;
		int64_t now = k_uptime_get();
		LOG_INF("RX1: target=%lld, now=%lld, freq=%u, dr=%u, to=%u",
			rx1_target, now, rx1_freq, rx1_dr, rx1_timeout);
		if (now < rx1_target) {
			k_msleep(rx1_target - now);
		}
		rx_len = open_rx_window(rx1_freq, rx1_dr, rx_buf,
				       sizeof(rx_buf), &rssi, &snr, rx1_timeout);
		if (rx_len <= 0) {
			/* RX2 window */
			int64_t rx2_target = tx_end_time + rx2_delay_ms;
			now = k_uptime_get();
			LOG_INF("RX2: target=%lld, now=%lld, freq=%u, dr=%u, to=%u",
				rx2_target, now, rx2_freq, rx2_dr, rx2_timeout);
			if (now < rx2_target) {
				k_msleep(rx2_target - now);
			}
			rx_len = open_rx_window(rx2_freq, rx2_dr,
						rx_buf, sizeof(rx_buf),
						&rssi, &snr, rx2_timeout);
		}

		if (rx_len > 0) {
			process_downlink(rx_buf, rx_len, rssi, snr,
					 &confirmed_ack_received);
		}
	}

	if (confirmed && !confirmed_ack_received) {
		ret = -ETIMEDOUT;
	}

out:
	k_mutex_unlock(&lorawan_mutex);
	return ret;
}

int lorawan_set_class(enum lorawan_class dev_class)
{
	if (dev_class == LORAWAN_CLASS_B) {
		LOG_ERR("Class B not supported");
		return -ENOTSUP;
	}
	g_ctx.dev_class = dev_class;
	return 0;
}

int lorawan_set_conf_msg_tries(uint8_t tries)
{
	g_ctx.conf_msg_tries = tries;
	return 0;
}

void lorawan_enable_adr(bool enable)
{
	g_ctx.adr_enabled = enable;
}

int lorawan_set_channels_mask(uint16_t *channels_mask,
			      size_t channels_mask_size)
{
	if (channels_mask == NULL || channels_mask_size == 0) {
		return -EINVAL;
	}

	k_mutex_lock(&lorawan_mutex, K_FOREVER);

	/* CN470 has 96 channels, mapping to up to 6 uint16_t mask elements */
	for (size_t i = 0; i < 96; i++) {
		size_t mask_idx = i / 16;
		size_t bit_idx = i % 16;

		if (mask_idx < channels_mask_size) {
			g_ctx.region.channels[i].enabled =
				(channels_mask[mask_idx] & (1 << bit_idx)) ? true : false;
		} else {
			g_ctx.region.channels[i].enabled = false;
		}
	}

	k_mutex_unlock(&lorawan_mutex);
	return 0;
}

int lorawan_set_datarate(enum lorawan_datarate dr)
{
	if (dr >= g_ctx.region.num_dr) {
		return -EINVAL;
	}
	g_ctx.current_dr = dr;
	g_ctx.default_dr = dr;
	g_ctx.join_dr = dr;  /* Also set join DR */
	return 0;
}

enum lorawan_datarate lorawan_get_min_datarate(void)
{
	return LORAWAN_DR_0;
}

void lorawan_get_payload_sizes(uint8_t *max_next_payload_size,
			       uint8_t *max_payload_size)
{
	int max_len = region_get_max_payload(&g_ctx.region, g_ctx.current_dr);
	if (max_len < 0) {
		max_len = 51;
	}
	*max_next_payload_size = max_len;
	*max_payload_size = max_len;
}

int lorawan_set_region(enum lorawan_region region)
{


	region_init(&g_ctx.region, region);
	return 0;
}

int lorawan_request_device_time(bool force_request)
{
	k_mutex_lock(&lorawan_mutex, K_FOREVER);
	g_ctx.device_time_req_pending = true;
	k_mutex_unlock(&lorawan_mutex);

	if (force_request) {
		return lorawan_send(0, NULL, 0, LORAWAN_MSG_UNCONFIRMED);
	}
	return 0;
}

int lorawan_device_time_get(uint32_t *gps_time)
{
	if (!g_ctx.clock_synced) {
		return -EAGAIN;
	}
	*gps_time = g_ctx.clock_gps_time;
	return 0;
}

int lorawan_request_link_check(bool force_request)
{
	k_mutex_lock(&lorawan_mutex, K_FOREVER);
	g_ctx.link_check_req_pending = true;
	k_mutex_unlock(&lorawan_mutex);

	if (force_request) {
		return lorawan_send(0, NULL, 0, LORAWAN_MSG_UNCONFIRMED);
	}
	return 0;
}

int lorawan_clock_sync_run(void)
{
	return -ENOTSUP;
}

int lorawan_clock_sync_get(uint32_t *gps_time)
{
	return lorawan_device_time_get(gps_time);
}

void lorawan_frag_transport_register_descriptor_callback(
				transport_descriptor_cb cb)
{
	g_ctx.frag_desc_cb = cb;
}

int lorawan_frag_transport_run(void (*transport_finished_cb)(void))
{
	g_ctx.frag_finished_cb = transport_finished_cb;
	return -ENOTSUP;
}

/* -------------------------------------------------------------------------- */
/* Init: resolve radio device                                                  */
/* -------------------------------------------------------------------------- */
static int lorawan_impl_init(void)
{
	lora_dev = LORA_DEV;
	return 0;
}

SYS_INIT(lorawan_impl_init, POST_KERNEL, 0);
