#include "mac_commands.h"
#include "../lorawan_state.h"
#include "../region/region.h"
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(lorawan, CONFIG_LOG_DEFAULT_LEVEL);

void process_mac_commands(const uint8_t *payload, size_t len)
{
	size_t i = 0;

	while (i < len) {
		uint8_t cid = payload[i++];

		switch (cid) {
		case 0x01: {
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

				if (g_ctx.mac_rsp_len + 2 <= sizeof(g_ctx.mac_rsp_buf)) {
					g_ctx.mac_rsp_buf[g_ctx.mac_rsp_len++] = 0x0A;
					g_ctx.mac_rsp_buf[g_ctx.mac_rsp_len++] = status;
				}
			} else {
				return;
			}
			break;
		}
		case 0x0C: {
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
		case 0x0E: {
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
		case 0x0F: {
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

void append_uplink_mac_cmds(void)
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
