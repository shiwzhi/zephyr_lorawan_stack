#include "join.h"
#include "rx.h"
#include "../lorawan_state.h"
#include "../region/region.h"
#include "../radio/radio_hal.h"
#include "../crypto/lorawan_crypto.h"
#include <zephyr/drivers/lora.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(lorawan, CONFIG_LOG_DEFAULT_LEVEL);

int send_join_request(uint8_t *rx_buf, size_t rx_buflen)
{
	uint8_t payload[23];
	uint8_t *p = payload;
	int ret;

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

	ret = radio_configure_tx(ch->freq_hz, g_ctx.join_dr);
	if (ret < 0) {
		return ret;
	}

	ret = lora_send(lora_dev, payload, sizeof(payload));
	if (ret < 0) {
		LOG_ERR("Join-Request TX failed: %d", ret);
		return ret;
	}

	int64_t tx_end_time = k_uptime_get();

	LOG_INF("Join-Request sent on %u Hz (DevNonce=%u)",
		ch->freq_hz, g_ctx.dev_nonce);
	g_ctx.dev_nonce++;
	if (g_ctx.dev_nonce == 0) {
		LOG_WRN("DevNonce rolled over to 0 — reusing nonces is a security risk");
	}

	int16_t rssi;
	int8_t snr;

	struct rx_window_config rx1_cfg = {
		.freq = rx1_freq,
		.dr = rx1_dr,
		.timeout_ms = region_get_rx_window_timeout_ms(&g_ctx.region, rx1_dr),
		.delay_ms = JOIN_RX1_DELAY_MS - RX_EARLY_MS,
	};
	struct rx_window_config rx2_cfg = {
		.freq = rx2_freq,
		.dr = rx2_dr,
		.timeout_ms = region_get_rx_window_timeout_ms(&g_ctx.region, rx2_dr),
		.delay_ms = JOIN_RX2_DELAY_MS - RX_EARLY_MS,
	};

	int rx_len = open_rx_windows(tx_end_time, &rx1_cfg, &rx2_cfg,
				     rx_buf, rx_buflen, &rssi, &snr);
	if (rx_len > 0) {
		if (storage_initialized) {
			nvs_write(&fs, NVS_DEV_NONCE_ID, &g_ctx.dev_nonce, sizeof(g_ctx.dev_nonce));
		}
	}
	return rx_len;
}
