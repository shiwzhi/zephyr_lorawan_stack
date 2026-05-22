#include "radio_hal.h"
#include "../lorawan_state.h"
#include "../region.h"
#include <zephyr/drivers/lora.h>
#include <zephyr/logging/log.h>

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
		int8_t ceiling = (int8_t)(6 + 2 * g_ctx.max_eirp);
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
	return region_get_join_channel(&g_ctx.region);
}
