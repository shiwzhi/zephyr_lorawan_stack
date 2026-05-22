#include "downlink.h"
#include "mac_commands.h"
#include "../lorawan_state.h"
#include "../crypto/lorawan_crypto.h"
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(lorawan, CONFIG_LOG_DEFAULT_LEVEL);

void process_downlink(uint8_t *phy, size_t len, int16_t rssi,
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
