/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 *
 * Custom LoRaWAN Class A/C end-device stack.
 * Implements the API declared in <zephyr/lorawan/lorawan.h>
 * using the raw Zephyr LoRa driver (<zephyr/drivers/lora.h>).
 *
 * Based on:
 *   - LoRaWAN L2 1.0.3 Specification
 *   - LoRaWAN Regional Parameters RP1-1.0.3 Rev A
 */

#include <zephyr/lorawan/lorawan.h>
#include <zephyr/drivers/lora.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>
#include <zephyr/kvss/nvs.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include "region/region.h"
#include "lorawan_state.h"
#include "crypto/lorawan_crypto.h"
#include "radio/radio_hal.h"
#include "mac/mac_commands.h"
#include "mac/downlink.h"
#include "mac/rx.h"
#include "mac/join.h"

LOG_MODULE_REGISTER(lorawan, CONFIG_LOG_DEFAULT_LEVEL);

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

struct lorawan_ctx g_ctx;
const struct device *lora_dev = LORA_DEV;
struct nvs_fs fs;
bool storage_initialized = false;
K_MUTEX_DEFINE(lorawan_mutex);

/* Class C continuous RX2 receive thread */
static K_THREAD_STACK_DEFINE(class_c_stack, 2048);
static struct k_thread class_c_thread_data;
static k_tid_t class_c_tid;
static struct k_sem class_c_data_sem;
static uint8_t class_c_rx_buf[MAX_PHY_PAYLOAD];
static int class_c_rx_len;
static int16_t class_c_rssi;
static int8_t class_c_snr;

static void class_c_rx_cb(const struct device *dev, uint8_t *data,
			  uint16_t size, int16_t rssi, int8_t snr,
			  void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(user_data);

	if (size > 0 && g_ctx.class_c_active && !g_ctx.class_c_paused) {
		memcpy(class_c_rx_buf, data, size);
		class_c_rx_len = size;
		class_c_rssi = rssi;
		class_c_snr = snr;
		k_sem_give(&class_c_data_sem);
	}
}

static void class_c_rx_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1) {
		while (!g_ctx.class_c_active || g_ctx.class_c_paused) {
			k_sleep(K_MSEC(100));
		}

		uint32_t rx2_freq = (g_ctx.region.rxc_freq != 0) ?
				    g_ctx.region.rxc_freq : get_rx2_freq();
		uint8_t rx2_dr = (g_ctx.region.rxc_dr != 0xFF) ?
				 g_ctx.region.rxc_dr : get_rx2_dr();

		int ret = radio_configure_rx(rx2_freq, rx2_dr);
		if (ret < 0) {
			LOG_ERR("Class C RX config failed: %d", ret);
			k_msleep(1000);
			continue;
		}

		class_c_rx_len = 0;
		ret = lora_recv_async(lora_dev, class_c_rx_cb, NULL);
		if (ret < 0) {
			LOG_ERR("Class C recv_async failed: %d", ret);
			k_msleep(1000);
			continue;
		}

		k_sem_take(&class_c_data_sem, K_FOREVER);

		lora_recv_async(lora_dev, NULL, NULL);

		if (!g_ctx.class_c_active || g_ctx.class_c_paused) {
			continue;
		}

		if (class_c_rx_len > 0) {
			k_mutex_lock(&lorawan_mutex, K_FOREVER);
			LOG_INF("Class C downlink received (%d bytes)", class_c_rx_len);
			bool ack = false;
			process_downlink(class_c_rx_buf, class_c_rx_len,
					 class_c_rssi, class_c_snr, &ack, true);
			if (ack) {
				k_work_schedule(&g_ctx.class_c_ack_work,
						K_MSEC(100));
			}
			k_mutex_unlock(&lorawan_mutex);
		}
	}
}

static void class_c_ack_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);
	LOG_INF("Class C ack work: sending immediate uplink ACK");
	int ret = lorawan_send(0, NULL, 0, LORAWAN_MSG_UNCONFIRMED);
	if (ret < 0) {
		LOG_ERR("Class C ACK uplink failed: %d", ret);
	}
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
		g_ctx.state = STATE_JOINING;

		uint8_t rx_buf[33];
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

		uint8_t *ja = rx_buf + 1;
		size_t ja_len = rx_len - 1;

		lorawan_join_accept_decrypt(g_ctx.app_key, ja, ja_len);

		uint8_t mic[4];

		lorawan_mic_join_accept(g_ctx.app_key, rx_buf, rx_len - 4, mic);
		if (memcmp(mic, rx_buf + rx_len - 4, 4) != 0) {
			LOG_ERR("Join-Accept MIC mismatch");
			ret = -EIO;
			goto out;
		}

		uint8_t join_nonce[3];
		uint8_t net_id[3];
		uint32_t dev_addr;
		uint16_t dev_nonce_le = g_ctx.dev_nonce;

		join_nonce[0] = ja[0];
		join_nonce[1] = ja[1];
		join_nonce[2] = ja[2];
		net_id[0] = ja[3];
		net_id[1] = ja[4];
		net_id[2] = ja[5];
		dev_addr = ((uint32_t)ja[6]) | (((uint32_t)ja[7]) << 8) |
			   (((uint32_t)ja[8]) << 16) | (((uint32_t)ja[9]) << 24);

		g_ctx.rx1_dr_offset = (ja[10] >> 4) & 0x07;
		uint8_t rx2_dr = ja[10] & 0x0F;
		if (rx2_dr < g_ctx.region.num_dr) {
			g_ctx.rx2_dr_override = rx2_dr;
		}
		LOG_INF("JoinAccept DLSettings: RX1 DR offset=%u, RX2 DR=%u",
			g_ctx.rx1_dr_offset, rx2_dr);

		uint8_t rx_delay = ja[11];
		g_ctx.rx_timing_delay = (rx_delay == 0) ? 1 : rx_delay;

		if (rx_len >= 33) {
			region_apply_cflist(&g_ctx.region, ja + 12);
		}

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

	struct region_ctx saved_region_ctx;
	bool region_was_set = g_ctx.region.num_channels > 0;
	if (region_was_set) {
		memcpy(&saved_region_ctx, &g_ctx.region, sizeof(g_ctx.region));
	}

	memset(&g_ctx, 0, sizeof(g_ctx));
	sys_slist_init(&g_ctx.dl_callbacks);

	if (region_was_set) {
		memcpy(&g_ctx.region, &saved_region_ctx, sizeof(g_ctx.region));
	}

	g_ctx.started = true;
	g_ctx.dev_class = LORAWAN_CLASS_A;
	k_sem_init(&class_c_data_sem, 0, 1);
	k_work_init_delayable(&g_ctx.class_c_ack_work, class_c_ack_work_handler);
	g_ctx.current_dr = LORAWAN_DR_0;
	g_ctx.default_dr = LORAWAN_DR_0;
	g_ctx.join_dr = LORAWAN_DR_0;
	g_ctx.conf_msg_tries = 1;
	g_ctx.minor_version = 3;

	g_ctx.rx2_freq_override = 0;
	g_ctx.rx2_dr_override = 0xFF;
	g_ctx.rx_timing_delay = 0;
	g_ctx.rx1_dr_offset = 0;
	g_ctx.max_duty_cycle = 0;
	g_ctx.max_eirp = 0;
	g_ctx.adr_power_index = -1;
	g_ctx.uplink_dwell_time = false;
	g_ctx.downlink_dwell_time = false;

	struct flash_pages_info info;
	const struct device *flash_dev;
	int rc;

	flash_dev = PARTITION_DEVICE(STORAGE_PARTITION);
	if (device_is_ready(flash_dev)) {
		fs.flash_device = flash_dev;
		fs.offset = PARTITION_OFFSET(STORAGE_PARTITION);

		rc = flash_get_page_info_by_offs(flash_dev, fs.offset, &info);
		if (rc == 0) {
			fs.sector_size = info.size;
			fs.sector_count = 3U;

			rc = nvs_mount(&fs);
			if (rc == 0) {
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

	k_mutex_unlock(&lorawan_mutex);

	LOG_INF("Custom LoRaWAN stack started (Class A)");
	return 0;
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
	bool was_class_c = false;

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

	/* Pause Class C continuous RX before TX */
	was_class_c = (g_ctx.dev_class == LORAWAN_CLASS_C && g_ctx.class_c_active);
	if (was_class_c) {
		g_ctx.class_c_paused = true;
		k_sem_give(&class_c_data_sem);
		k_msleep(100);
	}

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

	const struct lorawan_channel *ch = get_data_channel();
	g_ctx.last_tx_channel = (uint8_t)(ch - g_ctx.region.channels);

	ret = radio_configure_tx(ch->freq_hz, g_ctx.current_dr);
	if (ret < 0) {
		goto out;
	}
	LOG_INF("TX: freq=%u, SF=%d, BW=%d kHz, power=%d dBm",
		ch->freq_hz,
		g_ctx.region.dr_table[g_ctx.current_dr].datarate,
		g_ctx.region.dr_table[g_ctx.current_dr].bandwidth,
		get_tx_power_dbm());

	uint8_t rx1_dr = 0;
	uint32_t rx1_freq = 0;
	uint16_t rx1_timeout = 0;
	uint32_t rx2_freq = 0;
	uint8_t rx2_dr = 0;
	uint16_t rx2_timeout = 0;
	int64_t rx1_delay_ms = 0;
	int64_t rx2_delay_ms = 0;
	bool open_rx = false;

	if (g_ctx.dev_class == LORAWAN_CLASS_A ||
	    g_ctx.dev_class == LORAWAN_CLASS_C) {
		rx1_dr = region_get_rx1_dr(&g_ctx.region,
					   g_ctx.current_dr, g_ctx.rx1_dr_offset);
		rx1_freq = region_get_rx1_freq(&g_ctx.region,
						g_ctx.last_tx_channel);
		rx2_freq = get_rx2_freq();
		rx2_dr = get_rx2_dr();
		rx1_timeout = region_get_rx_window_timeout_ms(&g_ctx.region, rx1_dr);
		rx2_timeout = region_get_rx_window_timeout_ms(&g_ctx.region, rx2_dr);
		uint8_t rx1_delay = (g_ctx.rx_timing_delay != 0) ?
				    g_ctx.rx_timing_delay : g_ctx.region.rx1_delay_s;
		rx1_delay_ms = rx1_delay * 1000 - RX_EARLY_MS;
		rx2_delay_ms = (rx1_delay + 1) * 1000 - RX_EARLY_MS;
		open_rx = true;
		LOG_INF("RX1: freq=%u, SF=%d, BW=%d kHz",
			rx1_freq,
			g_ctx.region.dr_table[rx1_dr].datarate,
			g_ctx.region.dr_table[rx1_dr].bandwidth);
		LOG_INF("RX2: freq=%u, SF=%d, BW=%d kHz",
			rx2_freq,
			g_ctx.region.dr_table[rx2_dr].datarate,
			g_ctx.region.dr_table[rx2_dr].bandwidth);
	}

	ret = lora_send(lora_dev, tx_buf, phy_len);
	if (ret < 0) {
		LOG_ERR("Uplink TX failed: %d", ret);
		goto out;
	}

	int64_t tx_end_time = k_uptime_get();

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

	if (open_rx) {
		uint8_t rx_buf[MAX_PHY_PAYLOAD];
		int16_t rssi;
		int8_t snr;

		struct rx_window_config rx1_cfg = {
			.freq = rx1_freq,
			.dr = rx1_dr,
			.timeout_ms = rx1_timeout,
			.delay_ms = rx1_delay_ms,
		};
		struct rx_window_config rx2_cfg = {
			.freq = rx2_freq,
			.dr = rx2_dr,
			.timeout_ms = rx2_timeout,
			.delay_ms = rx2_delay_ms,
		};

		LOG_DBG("RX1: target=%lld, freq=%u, dr=%u, to=%u",
			tx_end_time + rx1_delay_ms, rx1_freq, rx1_dr, rx1_timeout);
		LOG_DBG("RX2: target=%lld, freq=%u, dr=%u, to=%u",
			tx_end_time + rx2_delay_ms, rx2_freq, rx2_dr, rx2_timeout);

		int rx_len = open_rx_windows(tx_end_time, &rx1_cfg, &rx2_cfg,
					      rx_buf, sizeof(rx_buf), &rssi, &snr);
		if (rx_len > 0) {
			process_downlink(rx_buf, rx_len, rssi, snr,
					 &confirmed_ack_received, false);
		}
	}

	if (confirmed && !confirmed_ack_received) {
		ret = -ETIMEDOUT;
	}

out:
	/* Resume Class C continuous RX after TX+RX windows */
	if (was_class_c) {
		g_ctx.class_c_paused = false;
	}

	k_mutex_unlock(&lorawan_mutex);
	return ret;
}

int lorawan_set_class(enum lorawan_class dev_class)
{
	if (dev_class == LORAWAN_CLASS_B) {
		LOG_ERR("Class B not supported");
		return -ENOTSUP;
	}

	if (dev_class == g_ctx.dev_class) {
		return 0;
	}

	if (dev_class == LORAWAN_CLASS_C) {
		g_ctx.class_c_active = true;
		g_ctx.class_c_paused = false;
		g_ctx.dev_class = LORAWAN_CLASS_C;

		class_c_tid = k_thread_create(
			&class_c_thread_data, class_c_stack,
			K_THREAD_STACK_SIZEOF(class_c_stack),
			class_c_rx_thread, NULL, NULL, NULL,
			K_PRIO_PREEMPT(8), 0, K_NO_WAIT);
		k_thread_name_set(class_c_tid, "lorawan_class_c");

		LOG_INF("Switched to Class C (continuous RX listening)");
	} else {
		g_ctx.class_c_active = false;
		g_ctx.class_c_paused = true;
		k_sem_give(&class_c_data_sem);
		k_msleep(200);
		k_thread_abort(class_c_tid);

		g_ctx.dev_class = LORAWAN_CLASS_A;
		LOG_INF("Switched to Class A");
	}

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
	g_ctx.join_dr = dr;
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
