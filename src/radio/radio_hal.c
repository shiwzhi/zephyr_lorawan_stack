#include "radio/radio_hal.h"
#include "lorawan_state.h"
#include "region/region.h"
#include <zephyr/drivers/lora.h>
#include <zephyr/random/random.h>
#include <zephyr/logging/log.h>
#include <inttypes.h>
#include <errno.h>

LOG_MODULE_DECLARE(lorawan, CONFIG_LOG_DEFAULT_LEVEL);

uint32_t get_rx2_freq(void)
{
	if (g_ctx.rx2_freq_override != 0) {
		return g_ctx.rx2_freq_override;
	}
	return g_ctx.region.rx2_freq;
}

uint8_t get_rx2_dr(void)
{
	if (g_ctx.rx2_dr_override != 0xFF) {
		return g_ctx.rx2_dr_override;
	}
	return g_ctx.region.rx2_dr;
}

int radio_configure_rx(uint32_t freq, uint8_t dr)
{
	struct lora_modem_config cfg = {
		.frequency = freq,
		.coding_rate = CR_4_5,
		.preamble_len = 8,
		.tx_power = g_ctx.region.default_tx_power,
		.tx = false,
		.iq_inverted = true,
		.public_network = true,
	};
	int ret;

	ret = region_dr_to_lora(&g_ctx.region, dr, &cfg.datarate, &cfg.bandwidth);
	if (ret < 0) {
		return ret;
	}

	return lora_config(lora_dev, &cfg);
}

int8_t get_tx_power_dbm(void)
{
	int8_t dbm;

	dbm = (int8_t)g_ctx.region.default_tx_power;

	if (g_ctx.adr_power_index >= 0 && g_ctx.adr_power_index < 8) {
		dbm = (int8_t)g_ctx.region.tx_power_map[g_ctx.adr_power_index];
	}

	if (g_ctx.max_eirp > 0) {
		/* LoRaWAN TxParamSetupReq MaxEIRP encoding (Table 27):
		 * 0=8, 1=10, 2=12, 3=13, 4=14, 5=16, 6=18, 7=20,
		 * 8=21, 9=24, 10=26, 11=27, 12=29, 13=30, 14=33, 15=36
		 */
		static const int8_t eirp_table[16] = {
			8, 10, 12, 13, 14, 16, 18, 20,
			21, 24, 26, 27, 29, 30, 33, 36
		};
		uint8_t idx = g_ctx.max_eirp & 0x0F;
		int8_t ceiling = eirp_table[idx];
		if (dbm > ceiling) {
			dbm = ceiling;
		}
	}

	return dbm;
}

int radio_configure_tx(uint32_t freq, uint8_t dr)
{
	struct lora_modem_config cfg = {
		.frequency = freq,
		.coding_rate = CR_4_5,
		.preamble_len = 8,
		.tx_power = get_tx_power_dbm(),
		.tx = true,
		.iq_inverted = false,
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

void build_fhdr(uint8_t *buf, uint32_t dev_addr, uint8_t fctrl, uint16_t fcnt)
{
	buf[0] = dev_addr & 0xFF;
	buf[1] = (dev_addr >> 8) & 0xFF;
	buf[2] = (dev_addr >> 16) & 0xFF;
	buf[3] = (dev_addr >> 24) & 0xFF;
	buf[4] = fctrl;
	buf[5] = fcnt & 0xFF;
	buf[6] = (fcnt >> 8) & 0xFF;
}

const struct lorawan_channel *get_data_channel(void)
{
	uint8_t num_channels = g_ctx.region.num_channels;
	uint8_t enabled_indices[LORAWAN_MAX_CHANNELS];
	uint8_t count = 0;

	for (uint8_t i = 0; i < num_channels; i++) {
		if (g_ctx.region.channels[i].enabled) {
			enabled_indices[count++] = i;
		}
	}

	if (count > 0) {
		uint32_t r = sys_rand32_get();
		return &g_ctx.region.channels[enabled_indices[r % count]];
	}

	return region_get_join_channel(&g_ctx.region);
}

int lorawan_tx(const struct lorawan_tx_config *cfg)
{
	int ret = radio_configure_tx(cfg->freq, cfg->dr);
	if (ret < 0) {
		return ret;
	}
	ret = lora_send(lora_dev, cfg->buf, cfg->len);
	if (ret == 0) {
		enum lora_datarate sf = 0;
		enum lora_signal_bandwidth bw = 0;
		region_dr_to_lora(&g_ctx.region, cfg->dr, &sf, &bw);
		LOG_INF("TX: freq=%u Hz, SF=%d, BW=%d kHz, power=%d dBm, size=%u bytes",
			cfg->freq, (int)sf, (int)bw, get_tx_power_dbm(), (unsigned)cfg->len);
	}
	return ret;
}

int lorawan_rx(const struct lorawan_rx_config *cfg,
	       int64_t tx_end_time,
	       uint8_t *rx_buf, size_t rx_buflen,
	       struct lorawan_rx_result *result)
{
	int ret;
	int64_t target;
	int64_t now;
	enum lora_datarate sf = 0;
	enum lora_signal_bandwidth bw = 0;

	/* === RX1 === */
	radio_configure_rx(cfg->rx1_freq, cfg->rx1_dr);
	region_dr_to_lora(&g_ctx.region, cfg->rx1_dr, &sf, &bw);

	target = tx_end_time + cfg->rx1_delay_ms - RX_EARLY_MS;
	LOG_INF("RX1: freq=%u Hz, SF=%d, BW=%d kHz, target at t=%lld ms",
		cfg->rx1_freq, (int)sf, (int)bw, (long long)target);
	now = k_uptime_get();
	if (now < target) {
		k_msleep(target - now);
	}

	ret = lora_recv(lora_dev, rx_buf, rx_buflen,
			K_MSEC(cfg->rx1_timeout_ms), &result->rssi, &result->snr);
	if (ret > 0) {
		LOG_INF("RX1: received %d bytes, RSSI=%d, SNR=%d",
			ret, result->rssi, result->snr);
		result->len = ret;
		return ret;
	}
	LOG_INF("RX1: timeout");

	/* === RX2 === */
	radio_configure_rx(cfg->rx2_freq, cfg->rx2_dr);
	region_dr_to_lora(&g_ctx.region, cfg->rx2_dr, &sf, &bw);

	target = tx_end_time + cfg->rx2_delay_ms - RX_EARLY_MS;
	LOG_INF("RX2: freq=%u Hz, SF=%d, BW=%d kHz, target at t=%lld ms",
		cfg->rx2_freq, (int)sf, (int)bw, (long long)target);
	now = k_uptime_get();
	if (now < target) {
		k_msleep(target - now);
	}

	ret = lora_recv(lora_dev, rx_buf, rx_buflen,
			K_MSEC(cfg->rx2_timeout_ms), &result->rssi, &result->snr);
	if (ret > 0) {
		LOG_INF("RX2: received %d bytes, RSSI=%d, SNR=%d",
			ret, result->rssi, result->snr);
		result->len = ret;
		return ret;
	}
	LOG_INF("RX2: timeout");

	result->len = 0;
	return -ETIMEDOUT;
}
