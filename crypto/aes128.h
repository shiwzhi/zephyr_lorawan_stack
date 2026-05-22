#ifndef LORAWAN_AES128_H_
#define LORAWAN_AES128_H_

#include <stdint.h>
#include <stddef.h>

void aes128_encrypt_block(const uint8_t key[16], const uint8_t in[16],
			  uint8_t out[16]);

#endif /* LORAWAN_AES128_H_ */
