#ifndef LORAWAN_STATE_H_
#define LORAWAN_STATE_H_

#include <zephyr/lorawan/lorawan.h>
#include <zephyr/drivers/lora.h>
#include <zephyr/kernel.h>
#include <zephyr/kvss/nvs.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "region/region.h"

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

/* Open RX windows early to cover radio setup */
#define RX_EARLY_MS             50

/* Join RX window delays (per LoRaWAN spec) */
#define JOIN_RX1_DELAY_MS      5000
#define JOIN_RX2_DELAY_MS      6000

#define STORAGE_PARTITION storage_partition
#define NVS_DEV_NONCE_ID 1

enum lorawan_state {
	STATE_IDLE,
	STATE_JOINING,
	STATE_JOINED,
};

#define CLASS_C_RESP_TIMEOUT_MS 8000

	struct lorawan_ctx {
	bool started;
	enum lorawan_state state;
	enum lorawan_class dev_class;
	bool class_c_active;
	bool class_c_paused;
	struct k_sem class_c_pause_sem;
	struct k_work_delayable class_c_ack_work;
	uint8_t last_tx_channel;

	uint8_t dev_eui[8];
	uint8_t join_eui[8];
	uint8_t app_key[16];
	uint16_t dev_nonce;

	uint32_t dev_addr;
	uint8_t nwk_skey[16];
	uint8_t app_skey[16];

	uint32_t fcnt_up;
	uint32_t fcnt_down;

	enum lorawan_datarate current_dr;
	enum lorawan_datarate default_dr;
	uint8_t join_dr;
	bool adr_enabled;
	uint8_t conf_msg_tries;
	struct region_ctx region;

	lorawan_battery_level_cb_t battery_level_cb;
	lorawan_dr_changed_cb_t dr_changed_cb;
	lorawan_link_check_ans_cb_t link_check_cb;
	sys_slist_t dl_callbacks;
	transport_descriptor_cb frag_desc_cb;
	void (*frag_finished_cb)(void);

	bool clock_synced;
	uint32_t clock_gps_time;

	int8_t last_snr;
	uint8_t rx1_dr_offset;
	bool downlink_ack_pending;
	uint8_t mac_rsp_buf[15];
	uint8_t mac_rsp_len;

	uint32_t rx2_freq_override;
	uint8_t rx2_dr_override;

	uint8_t max_duty_cycle;
	uint8_t rx_timing_delay;
	bool uplink_dwell_time;
	bool downlink_dwell_time;
	uint32_t max_eirp;
	int8_t adr_power_index;
	uint8_t adr_limit_exp;
	uint8_t adr_delay_exp;
	uint8_t rejoin_periodicity;
	uint8_t rejoin_max_count;
	uint8_t rejoin_max_time;
	uint8_t minor_version;

	bool reset_ind_pending;
	bool link_check_req_pending;
	bool device_time_req_pending;

	uint32_t dl_channel_freq[16];
	bool dl_channel_valid[16];
};

extern struct lorawan_ctx g_ctx;
extern const struct device *lora_dev;
extern struct nvs_fs fs;
extern bool storage_initialized;
extern struct k_mutex lorawan_mutex;

#endif /* LORAWAN_STATE_H_ */
