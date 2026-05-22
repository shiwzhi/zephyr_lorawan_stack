#include "rx.h"
#include "../lorawan_state.h"
#include "../radio/radio_hal.h"
#include "../region/region.h"
#include <zephyr/drivers/lora.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(lorawan, CONFIG_LOG_DEFAULT_LEVEL);

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

int open_rx_windows(int64_t tx_end_time,
		    const struct rx_window_config *rx1,
		    const struct rx_window_config *rx2,
		    uint8_t *buf, size_t buflen,
		    int16_t *rssi, int8_t *snr)
{
	int64_t gap = rx2->delay_ms - rx1->delay_ms;
	uint16_t rx1_to = rx1->timeout_ms;
	if (gap > 10 && rx1_to > (uint16_t)(gap - 10)) {
		rx1_to = (uint16_t)(gap - 10);
	}
	if (rx1_to < 10) {
		rx1_to = 10;
	}

	int64_t now;

	int64_t target = tx_end_time + rx1->delay_ms;
	now = k_uptime_get();
	if (now < target) {
		k_msleep(target - now);
	}
	int rx_len = open_rx_window(rx1->freq, rx1->dr, buf, buflen, rssi, snr, rx1_to);
	if (rx_len > 0) {
		LOG_INF("Downlink received in RX1 window (%d bytes)", rx_len);
		return rx_len;
	}

	target = tx_end_time + rx2->delay_ms;
	now = k_uptime_get();
	if (now < target) {
		k_msleep(target - now);
	}
	rx_len = open_rx_window(rx2->freq, rx2->dr, buf, buflen, rssi, snr, rx2->timeout_ms);
	if (rx_len > 0) {
		LOG_INF("Downlink received in RX2 window (%d bytes)", rx_len);
	}
	return rx_len;
}
