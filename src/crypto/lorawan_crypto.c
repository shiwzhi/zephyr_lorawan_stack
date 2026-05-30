#include "crypto/lorawan_crypto.h"
#include "crypto/aes128.h"
#include "crypto/aes_cmac.h"
#include <string.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(lorawan, CONFIG_LOG_DEFAULT_LEVEL);

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
