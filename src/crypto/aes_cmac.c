#include "crypto/aes_cmac.h"
#include "crypto/aes128.h"
#include <stdbool.h>

static const uint8_t rb[16] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x87
};

static void leftshift_onebit(const uint8_t *in, uint8_t *out)
{
	uint8_t overflow = 0;

	for (int i = 15; i >= 0; i--) {
		out[i] = (in[i] << 1) | overflow;
		overflow = (in[i] >> 7) & 1;
	}
}

static void generate_subkeys(const uint8_t key[16], uint8_t k1[16],
			     uint8_t k2[16])
{
	uint8_t l[16];
	uint8_t zero[16] = {0};

	aes128_encrypt_block(key, zero, l);

	leftshift_onebit(l, k1);
	if (l[0] & 0x80) {
		for (int i = 0; i < 16; i++) {
			k1[i] ^= rb[i];
		}
	}

	leftshift_onebit(k1, k2);
	if (k1[0] & 0x80) {
		for (int i = 0; i < 16; i++) {
			k2[i] ^= rb[i];
		}
	}
}

void aes128_cmac(const uint8_t key[16], const uint8_t *msg, size_t len,
		 uint8_t mac[16])
{
	uint8_t k1[16], k2[16];
	uint8_t block[16] = {0};
	uint8_t y[16] = {0};
	size_t n;
	bool last_complete;

	generate_subkeys(key, k1, k2);

	if (len == 0) {
		n = 1;
		last_complete = false;
	} else {
		n = (len + 15) / 16;
		last_complete = ((len % 16) == 0);
	}

	for (size_t i = 0; i < n - 1; i++) {
		for (int j = 0; j < 16; j++) {
			block[j] = y[j] ^ msg[i * 16 + j];
		}
		aes128_encrypt_block(key, block, y);
	}

	if (last_complete) {
		for (int j = 0; j < 16; j++) {
			block[j] = y[j] ^ msg[(n - 1) * 16 + j] ^ k1[j];
		}
	} else {
		size_t rem = len % 16;

		for (size_t j = 0; j < rem; j++) {
			block[j] = y[j] ^ msg[(n - 1) * 16 + j];
		}
		block[rem] = y[rem] ^ 0x80;
		for (size_t j = rem + 1; j < 16; j++) {
			block[j] = y[j];
		}
		for (int j = 0; j < 16; j++) {
			block[j] ^= k2[j];
		}
	}

	aes128_encrypt_block(key, block, mac);
}
