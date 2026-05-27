#ifndef LORAWAN_AES_CMAC_H_
#define LORAWAN_AES_CMAC_H_

#include <stdint.h>
#include <stddef.h>

void aes128_cmac(const uint8_t key[16], const uint8_t *msg, size_t len,
		 uint8_t mac[16]);

#endif /* LORAWAN_AES_CMAC_H_ */
