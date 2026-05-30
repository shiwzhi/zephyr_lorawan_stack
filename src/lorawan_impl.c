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
#include "region/region.h"
#include "lorawan_state.h"
#include "crypto/lorawan_crypto.h"
#include "radio/radio_hal.h"

LOG_MODULE_REGISTER(lorawan, CONFIG_LOG_DEFAULT_LEVEL);

/* Forward declarations */
static void process_downlink(uint8_t *phy, size_t len, int16_t rssi,
			     int8_t snr, bool *ack_received,
			     bool is_class_c);

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
		while ((!g_ctx.class_c_active || g_ctx.class_c_paused) &&
		       !g_ctx.class_c_shutdown) {
			k_sleep(K_MSEC(100));
		}
		if (g_ctx.class_c_shutdown) {
			break;
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
/* MAC command processing (from mac_commands.c)                                */
/* -------------------------------------------------------------------------- */

static void process_mac_commands(const uint8_t *payload, size_t len)
{
	size_t i = 0;

	while (i < len) {
		uint8_t cid = payload[i++];

		switch (cid) {
		case 0x02: {
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
		case 0x03: {
			if (i + 4 <= len) {
				uint8_t dr_power = payload[i++];
				uint16_t ch_mask = ((uint16_t)payload[i++]);
				ch_mask |= ((uint16_t)payload[i++] << 8);
				uint8_t redundancy = payload[i++];

				uint8_t dr = (dr_power >> 4) & 0x0F;
				uint8_t power = dr_power & 0x0F;
				uint8_t ch_mask_cntl = (redundancy >> 4) & 0x07;
				uint8_t nb_rep = redundancy & 0x0F;
				g_ctx.nb_trans = (nb_rep == 0) ? 1 : nb_rep;

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
					bool is_usau = (g_ctx.region.region == LORAWAN_REGION_US915 ||
							g_ctx.region.region == LORAWAN_REGION_AU915);
					bool is_cn470 = (g_ctx.region.region == LORAWAN_REGION_CN470);

					for (uint8_t c = 0; c < num_ch; c++) {
						bool en = g_ctx.region.channels[c].enabled;
						if (ch_mask_cntl <= 4) {
							int start_ch = ch_mask_cntl * 16;
							if (c >= start_ch && c < start_ch + 16) {
								en = (ch_mask & (1 << (c - start_ch))) ? true : false;
							}
						} else if (ch_mask_cntl == 5) {
							if (is_usau) {
								uint8_t block = c / 8;
								if (block < 8 && c < 64) {
									en = (ch_mask & (1 << block)) ? true : false;
								} else if (c >= 64 && c < 72) {
									en = (ch_mask & (1 << (c - 64))) ? true : false;
								}
							} else if (is_cn470) {
								if (c >= 80 && c < 96) {
									en = (ch_mask & (1 << (c - 80))) ? true : false;
								}
							} else {
								en = true;
							}
						} else if (ch_mask_cntl == 6) {
							if (is_usau) {
								if (c < 64) {
									en = true;
								} else {
									en = (ch_mask & (1 << (c - 64))) ? true : false;
								}
							} else {
								en = true;
							}
						} else if (ch_mask_cntl == 7) {
							if (is_usau) {
								if (c < 64) {
									en = false;
								} else {
									en = (ch_mask & (1 << (c - 64))) ? true : false;
								}
							} else {
								en = false;
							}
						}
						g_ctx.region.channels[c].enabled = en;
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
		case 0x04: {
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
		case 0x05: {
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

				g_ctx.rx_param_setup_ans_pending = true;

				if (g_ctx.mac_rsp_len + 2 <= sizeof(g_ctx.mac_rsp_buf)) {
					g_ctx.mac_rsp_buf[g_ctx.mac_rsp_len++] = 0x05;
					g_ctx.mac_rsp_buf[g_ctx.mac_rsp_len++] = status;
				}
			} else {
				return;
			}
			break;
		}
		case 0x06: {
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
		case 0x07: {
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
						g_ctx.region.channels[ch_idx].enabled = true;
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
		case 0x08: {
			if (i + 1 <= len) {
				uint8_t delay = payload[i++] & 0x0F;
				g_ctx.rx_timing_delay = (delay == 0) ? 1 : delay;
				LOG_INF("RXTimingSetupReq: RX1 delay = %u s, RX2 delay = %u s",
					g_ctx.rx_timing_delay, g_ctx.rx_timing_delay + 1);
			} else {
				return;
			}
			g_ctx.rx_timing_setup_ans_pending = true;
			if (g_ctx.mac_rsp_len + 1 <= sizeof(g_ctx.mac_rsp_buf)) {
				g_ctx.mac_rsp_buf[g_ctx.mac_rsp_len++] = 0x08;
			}
			break;
		}
		case 0x09: {
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
		case 0x0A: {
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

				g_ctx.dl_channel_ans_pending = true;

				if (g_ctx.mac_rsp_len + 2 <= sizeof(g_ctx.mac_rsp_buf)) {
					g_ctx.mac_rsp_buf[g_ctx.mac_rsp_len++] = 0x0A;
					g_ctx.mac_rsp_buf[g_ctx.mac_rsp_len++] = status;
				}
			} else {
				return;
			}
			break;
		}
		case 0x0D: {
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
		default:
			return;
		}
	}
}

static void append_uplink_mac_cmds(void)
{
	uint8_t *p = g_ctx.mac_rsp_buf + g_ctx.mac_rsp_len;
	int remaining = (int)sizeof(g_ctx.mac_rsp_buf) - (int)g_ctx.mac_rsp_len;

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

	/* Sticky MAC answers: repeat in every uplink until Class A downlink */
	if (g_ctx.rx_param_setup_ans_pending && remaining >= 2) {
		*p++ = 0x05;
		*p++ = 0x07;  /* All ACK bits set */
		g_ctx.mac_rsp_len += 2;
		remaining -= 2;
	}
	if (g_ctx.dl_channel_ans_pending && remaining >= 2) {
		*p++ = 0x0A;
		*p++ = 0x03;  /* Both ACK bits set */
		g_ctx.mac_rsp_len += 2;
		remaining -= 2;
	}
	if (g_ctx.rx_timing_setup_ans_pending && remaining >= 1) {
		*p++ = 0x08;
		g_ctx.mac_rsp_len += 1;
		remaining -= 1;
	}
}

/* -------------------------------------------------------------------------- */
/* Downlink processing (from downlink.c)                                       */
/* -------------------------------------------------------------------------- */

static void process_downlink(uint8_t *phy, size_t len, int16_t rssi,
			     int8_t snr, bool *ack_received,
			     bool is_class_c)
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

	uint16_t diff = fcnt - last_fcnt_lsb;
	if (diff < 32768) {
		reconstructed_fcnt = last_fcnt + diff;
	} else {
		reconstructed_fcnt = last_fcnt - (65536 - diff);
	}

	if (last_fcnt > 0 && reconstructed_fcnt <= last_fcnt) {
		LOG_WRN("Downlink dropped: Replay or out-of-sequence (received: %u, last: %u, reconstructed: %u)",
			fcnt, last_fcnt, reconstructed_fcnt);
		return;
	}

	uint8_t mic[4];
	uint8_t rx_mic[4];

	memcpy(rx_mic, phy + len - 4, 4);

	lorawan_mic_data(g_ctx.nwk_skey, 1, dev_addr, reconstructed_fcnt,
			 phy, len - 4, mic);

	if (memcmp(mic, rx_mic, 4) != 0) {
		LOG_WRN("Downlink MIC mismatch");
		return;
	}

	g_ctx.fcnt_down = reconstructed_fcnt;
	g_ctx.last_snr = snr;

	/* Clear sticky MAC answer flags on Class A downlink */
	g_ctx.rx_param_setup_ans_pending = false;
	g_ctx.dl_channel_ans_pending = false;
	g_ctx.rx_timing_setup_ans_pending = false;

	if (ack_received != NULL && (fctrl & FCTRL_ACK) != 0) {
		*ack_received = true;
	}
	if (ftype == MHDR_CONFIRMED_DOWN) {
		g_ctx.downlink_ack_pending = true;
	}

	if (fopts_len > 0) {
		process_mac_commands(phy + 8, fopts_len);
	}

	size_t hdr_len = 8 + fopts_len;
	uint8_t *payload = NULL;
	size_t payload_len = 0;
	uint8_t port = 0;
	uint8_t flags = 0;

	if (len > hdr_len + 4) {
		port = phy[hdr_len];
		payload = &phy[hdr_len + 1];
		payload_len = len - hdr_len - 1 - 4;

		uint8_t key[16];

		memcpy(key, port == 0 ? g_ctx.nwk_skey : g_ctx.app_skey, 16);
		lorawan_payload_encrypt(key, 1, dev_addr, reconstructed_fcnt,
					payload, payload_len);

		if (port == 0 && payload_len > 0) {
			process_mac_commands(payload, payload_len);
		}

		/* Reset ADR_ACK_CNT when downlink is received */
		g_ctx.adr_ack_cnt = 0;
	}

	if (fctrl & FCTRL_FPENDING) {
		flags |= LORAWAN_DATA_PENDING;
	}

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
/* Join request (from join.c)                                                  */
/* -------------------------------------------------------------------------- */

static int send_join_request(uint8_t *rx_buf, size_t rx_buflen)
{
	uint8_t payload[23];
	uint8_t *p = payload;

	g_ctx.dev_nonce = (uint16_t)sys_rand32_get();

	*p++ = MHDR_JOIN_REQUEST;
	lorawan_memcpy_rev(p, g_ctx.join_eui, 8);
	p += 8;
	lorawan_memcpy_rev(p, g_ctx.dev_eui, 8);
	p += 8;
	*p++ = g_ctx.dev_nonce & 0xFF;
	*p++ = (g_ctx.dev_nonce >> 8) & 0xFF;

	lorawan_mic_join_req(g_ctx.app_key, payload, 19, p);

	const struct lorawan_channel *ch = region_get_join_channel(&g_ctx.region);
	g_ctx.last_tx_channel = (uint8_t)(ch - g_ctx.region.channels);

	uint32_t rx1_freq = region_get_rx1_freq(&g_ctx.region, g_ctx.last_tx_channel);
	uint8_t rx1_dr = region_get_rx1_dr(&g_ctx.region, g_ctx.join_dr, 0);
	uint32_t rx2_freq = get_rx2_freq();
	uint8_t rx2_dr = get_rx2_dr();

	struct lorawan_tx_config tx_cfg = {
		.freq = ch->freq_hz,
		.dr   = g_ctx.join_dr,
		.buf  = payload,
		.len  = sizeof(payload),
	};
	int ret = lorawan_tx(&tx_cfg);
	if (ret < 0) {
		LOG_ERR("Join-Request TX failed: %d", ret);
		return ret;
	}

	int64_t tx_end_time = k_uptime_get();

	LOG_INF("DevNonce=%u)", g_ctx.dev_nonce);

	struct lorawan_rx_config rx_cfg = {
		.rx1_freq = rx1_freq,
		.rx1_dr   = rx1_dr,
		.rx1_timeout_ms = RX_TIMEOUT_MS,
		.rx1_delay_ms   = JOIN_RX1_DELAY_MS,
		.rx2_freq = rx2_freq,
		.rx2_dr   = rx2_dr,
		.rx2_timeout_ms = RX2_TIMEOUT_MS,
		.rx2_delay_ms   = JOIN_RX2_DELAY_MS,
	};
	struct lorawan_rx_result result;
	return lorawan_rx(&rx_cfg, tx_end_time, rx_buf, rx_buflen, &result);
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
	k_mutex_lock(&lorawan_mutex, K_FOREVER);
	sys_slist_append(&g_ctx.dl_callbacks, &cb->node);
	k_mutex_unlock(&lorawan_mutex);
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
		g_ctx.current_dr = g_ctx.join_dr;

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
	g_ctx.join_dr = g_ctx.region.default_join_dr;
	g_ctx.conf_msg_tries = 1;
	g_ctx.nb_trans = 1;
	g_ctx.minor_version = 3;

	g_ctx.rx2_freq_override = 0;
	g_ctx.rx2_dr_override = 0xFF;
	g_ctx.rx_timing_delay = 0;
	g_ctx.rx1_dr_offset = 0;
	g_ctx.max_duty_cycle = 0;
	g_ctx.rx_timing_delay = 0;
	g_ctx.rx1_dr_offset = 0;
	g_ctx.max_eirp = 0;
	g_ctx.adr_power_index = -1;
	g_ctx.uplink_dwell_time = false;
	g_ctx.downlink_dwell_time = false;

	/* ADR ACK defaults per LoRaWAN spec Section 5.2 */
	g_ctx.adr_limit_exp = 6;  /* ADR_ACK_LIMIT = 2^6 = 64 */
	g_ctx.adr_delay_exp = 5;  /* ADR_ACK_DELAY = 2^5 = 32 */

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

	/* ADR ACK backoff logic per LoRaWAN spec §5.2 */
	if (g_ctx.adr_enabled) {
		g_ctx.adr_ack_cnt++;

		if (g_ctx.adr_ack_cnt >= ((1 << g_ctx.adr_limit_exp) + (1 << g_ctx.adr_delay_exp))) {
			/* ADR_ACK_LIMIT + ADR_ACK_DELAY reached — reduce DR */
			if (g_ctx.current_dr > 0) {
				g_ctx.current_dr--;
				if (g_ctx.dr_changed_cb) {
					g_ctx.dr_changed_cb(g_ctx.current_dr);
				}
				LOG_INF("ADR: Reduced DR to %d due to ADR ACK backoff", g_ctx.current_dr);
			}
			g_ctx.adr_ack_cnt = 0;
		}
	}

	/* Pause Class C continuous RX before TX */
	was_class_c = (g_ctx.dev_class == LORAWAN_CLASS_C && g_ctx.class_c_active);
	if (was_class_c) {
		g_ctx.class_c_paused = true;
		k_sem_give(&class_c_data_sem);
		k_msleep(100);
	}

	if (g_ctx.adr_enabled) {
		fctrl |= FCTRL_ADR;
		if (g_ctx.adr_ack_cnt >= (1 << g_ctx.adr_limit_exp)) {
			fctrl |= FCTRL_ADRACKREQ;
		}
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

	struct lorawan_tx_config tx_cfg = {
		.freq = ch->freq_hz,
		.dr   = g_ctx.current_dr,
		.buf  = tx_buf,
		.len  = phy_len,
	};
	ret = lorawan_tx(&tx_cfg);
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

	if (g_ctx.dev_class == LORAWAN_CLASS_A ||
	    g_ctx.dev_class == LORAWAN_CLASS_C) {
		uint8_t rx1_dr = region_get_rx1_dr(&g_ctx.region,
						   g_ctx.current_dr, g_ctx.rx1_dr_offset);
		uint32_t rx1_freq = region_get_rx1_freq(&g_ctx.region,
							g_ctx.last_tx_channel);
		if (g_ctx.dl_channel_valid[g_ctx.last_tx_channel]) {
			rx1_freq = g_ctx.dl_channel_freq[g_ctx.last_tx_channel];
		}
		uint32_t rx2_freq = get_rx2_freq();
		uint8_t rx2_dr = get_rx2_dr();
		uint16_t rx1_timeout = RX_TIMEOUT_MS;
		uint16_t rx2_timeout = RX2_TIMEOUT_MS;
		uint8_t rx1_delay = (g_ctx.rx_timing_delay != 0) ?
				    g_ctx.rx_timing_delay : g_ctx.region.rx1_delay_s;
		int64_t rx1_delay_ms = rx1_delay * 1000;
		int64_t rx2_delay_ms = (rx1_delay + 1) * 1000;

		struct lorawan_rx_config rx_cfg = {
			.rx1_freq = rx1_freq,
			.rx1_dr   = rx1_dr,
			.rx1_timeout_ms = rx1_timeout,
			.rx1_delay_ms   = rx1_delay_ms,
			.rx2_freq = rx2_freq,
			.rx2_dr   = rx2_dr,
			.rx2_timeout_ms = rx2_timeout,
			.rx2_delay_ms   = rx2_delay_ms,
		};

		uint8_t rx_buf[MAX_PHY_PAYLOAD];
		struct lorawan_rx_result result;

		/* Single RX1/RX2 cycle */
		lorawan_rx(&rx_cfg, tx_end_time,
			   rx_buf, sizeof(rx_buf), &result);
		if (result.len > 0) {
			process_downlink(rx_buf, result.len, result.rssi, result.snr,
					&confirmed_ack_received, false);
		}

		/* Class C confirmed: keep listening until ACK or timeout */
		if (g_ctx.dev_class == LORAWAN_CLASS_C && confirmed &&
		    !confirmed_ack_received) {
			int64_t start_time = k_uptime_get();
			int64_t timeout_ms = CLASS_C_RESP_TIMEOUT_MS;

			while (!confirmed_ack_received &&
			       (k_uptime_get() - start_time) < timeout_ms) {
				lorawan_rx(&rx_cfg, tx_end_time,
					   rx_buf, sizeof(rx_buf), &result);
				if (result.len > 0) {
					process_downlink(rx_buf, result.len,
							 result.rssi, result.snr,
							 &confirmed_ack_received, false);
				}
				if (!confirmed_ack_received) {
					k_msleep(100);
				}
			}
		}

		if (confirmed) {
			uint8_t max_tries = g_ctx.conf_msg_tries;
			uint8_t retries = 0;
			bool ack_received = false;
			uint8_t retry_dr = g_ctx.current_dr;

			while (retries < max_tries && !ack_received) {
				if (retries > 0) {
					/* ACK_TIMEOUT: random 1-3 seconds per spec */
					uint32_t rng = sys_rand32_get();
					uint32_t ack_timeout_ms = 1000 + (rng % 2001);
					k_msleep(ack_timeout_ms);

					/* ADR: reduce DR every 2 retries per spec Section 6.3 */
					if (g_ctx.adr_enabled && retry_dr > 0 &&
					    retries >= 2 && (retries % 2) == 0) {
						retry_dr--;
						tx_cfg.dr = retry_dr;
						rx_cfg.rx1_dr = region_get_rx1_dr(&g_ctx.region,
										  retry_dr, g_ctx.rx1_dr_offset);
						LOG_INF("ADR: Reduced retry DR to %d", retry_dr);
					}

					ret = lorawan_tx(&tx_cfg);
					if (ret < 0) {
						LOG_ERR("Uplink TX retry %d failed: %d", retries, ret);
						break;
					}

					int64_t retry_tx_end = k_uptime_get();
					lorawan_rx(&rx_cfg, retry_tx_end,
						   rx_buf, sizeof(rx_buf), &result);
					if (result.len > 0) {
						process_downlink(rx_buf, result.len, result.rssi, result.snr,
								&ack_received, false);
					}
				} else {
					ack_received = confirmed_ack_received;
				}
				retries++;
			}

			if (!ack_received) {
				ret = -ETIMEDOUT;
			}
		}

		if (!confirmed && g_ctx.nb_trans > 1) {
			for (uint8_t i = 1; i < g_ctx.nb_trans; i++) {
				/* Wait for RX windows to expire before retransmit */
				int64_t rx_end = tx_end_time + rx2_delay_ms +
						 (int64_t)rx2_timeout + 100;
				int64_t now = k_uptime_get();
				if (now < rx_end) {
					k_msleep(rx_end - now);
				}

				ret = lorawan_tx(&tx_cfg);
				if (ret < 0) {
					LOG_ERR("Uplink retransmit %d failed: %d", i, ret);
					break;
				}

				tx_end_time = k_uptime_get();
				uint8_t rx_buf_nb[MAX_PHY_PAYLOAD];
				struct lorawan_rx_result result_nb;
				lorawan_rx(&rx_cfg, tx_end_time,
					   rx_buf_nb, sizeof(rx_buf_nb), &result_nb);
				if (result_nb.len > 0) {
					bool ack_nb = false;
					process_downlink(rx_buf_nb, result_nb.len,
							 result_nb.rssi, result_nb.snr,
							 &ack_nb, false);
				}
			}
		}
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
		g_ctx.class_c_shutdown = false;
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
		g_ctx.class_c_shutdown = true;
		k_sem_give(&class_c_data_sem);
		k_msleep(200);

		g_ctx.dev_class = LORAWAN_CLASS_A;
		LOG_INF("Switched to Class A");
	}

	return 0;
}

int lorawan_set_conf_msg_tries(uint8_t tries)
{
	k_mutex_lock(&lorawan_mutex, K_FOREVER);
	g_ctx.conf_msg_tries = tries;
	k_mutex_unlock(&lorawan_mutex);
	return 0;
}

void lorawan_enable_adr(bool enable)
{
	k_mutex_lock(&lorawan_mutex, K_FOREVER);
	g_ctx.adr_enabled = enable;
	k_mutex_unlock(&lorawan_mutex);
}

int lorawan_set_channels_mask(uint16_t *channels_mask,
			      size_t channels_mask_size)
{
	if (channels_mask == NULL || channels_mask_size == 0) {
		return -EINVAL;
	}

	k_mutex_lock(&lorawan_mutex, K_FOREVER);

	uint8_t num_ch = g_ctx.region.max_channels;
	if (num_ch > LORAWAN_MAX_CHANNELS) {
		num_ch = LORAWAN_MAX_CHANNELS;
	}

	for (size_t i = 0; i < num_ch; i++) {
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
	k_mutex_lock(&lorawan_mutex, K_FOREVER);
	if (dr >= g_ctx.region.num_dr) {
		k_mutex_unlock(&lorawan_mutex);
		return -EINVAL;
	}
	g_ctx.current_dr = dr;
	g_ctx.default_dr = dr;
	g_ctx.join_dr = dr;
	k_mutex_unlock(&lorawan_mutex);
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
	if (g_ctx.region.num_channels == 0) {
		LOG_ERR("Unsupported or unconfigured region: %d", region);
		return -EINVAL;
	}
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
