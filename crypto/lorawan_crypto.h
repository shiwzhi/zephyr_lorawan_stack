#ifndef LORAWAN_CRYPTO_H_
#define LORAWAN_CRYPTO_H_

#include <stdint.h>
#include <stddef.h>

void lorawan_mic_data(const uint8_t nwk_skey[16], uint8_t dir,
		      uint32_t dev_addr, uint32_t fcnt,
		      const uint8_t *msg, size_t len, uint8_t mic[4]);

void lorawan_mic_join_req(const uint8_t app_key[16], const uint8_t *payload,
			  size_t len, uint8_t mic[4]);

void lorawan_mic_join_accept(const uint8_t app_key[16],
			     const uint8_t *payload, size_t len,
			     uint8_t mic[4]);

void lorawan_payload_encrypt(const uint8_t key[16], uint8_t dir,
			     uint32_t dev_addr, uint32_t fcnt,
			     uint8_t *payload, size_t len);

void lorawan_join_accept_decrypt(const uint8_t app_key[16], uint8_t *payload,
				 size_t len);

void lorawan_derive_session_keys(const uint8_t app_key[16],
				 const uint8_t join_nonce[3],
				 const uint8_t net_id[3],
				 uint16_t dev_nonce,
				 uint8_t nwk_skey[16],
				 uint8_t app_skey[16]);

#endif /* LORAWAN_CRYPTO_H_ */
