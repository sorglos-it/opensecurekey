/* Rechenbausteine, die der CTAP-Kern braucht: SHA-256, HMAC, HKDF und
 * AES-256 im CBC-Modus.
 *
 * Wichtig: das Grundgeheimnis und die abgeleiteten Schluessel liegen im
 * SE050 und werden dort benutzt (osk_se050.h). Hier liegen nur die
 * oeffentlichen Verfahren, die das PIN-Protokoll und die Credential-ID
 * verlangen - alles nach FIPS 180-4, FIPS 197, RFC 2104 und RFC 5869.
 *
 * Auf der Zielplattform (nRF Connect SDK) sollen diese Funktionen auf PSA
 * Crypto zeigen; siehe docs/ctap.md. Die Fassung hier ist die, gegen die am
 * PC geprueft wird, und sie ist auch die Rueckfallebene. */
#ifndef OSK_CRYPTO_H
#define OSK_CRYPTO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"
#include "osk_mem.h" /* osk_wipe() - liegt gemeinsam */

#define OSK_SHA256_LEN 32
#define OSK_AES_BLOCK 16

typedef struct {
	uint32_t h[8];
	uint8_t block[64];
	size_t block_len;
	uint64_t total;
} osk_sha256_t;

void osk_sha256_init(osk_sha256_t *c);
void osk_sha256_update(osk_sha256_t *c, const uint8_t *p, size_t len);
void osk_sha256_final(osk_sha256_t *c, uint8_t out[OSK_SHA256_LEN]);
void osk_sha256(const uint8_t *p, size_t len, uint8_t out[OSK_SHA256_LEN]);

void osk_hmac_sha256(const uint8_t *key, size_t key_len, const uint8_t *msg, size_t msg_len,
		     uint8_t out[OSK_SHA256_LEN]);

/* Zweiteiliger Text, ohne ihn vorher zusammenkopieren zu muessen. */
void osk_hmac_sha256_2(const uint8_t *key, size_t key_len, const uint8_t *a, size_t a_len,
		       const uint8_t *b, size_t b_len, uint8_t out[OSK_SHA256_LEN]);

/* HKDF nach RFC 5869, nur mit SHA-256. out_len hoechstens 255*32. */
osk_result_t osk_hkdf_sha256(const uint8_t *salt, size_t salt_len, const uint8_t *ikm,
			     size_t ikm_len, const uint8_t *info, size_t info_len, uint8_t *out,
			     size_t out_len);

/* AES-256-CBC ohne Auffuellen: len muss ein Vielfaches von 16 sein.
 * Ein- und Ausgabepuffer duerfen gleich sein. */
osk_result_t osk_aes256_cbc_encrypt(const uint8_t key[32], const uint8_t iv[OSK_AES_BLOCK],
				    const uint8_t *in, uint8_t *out, size_t len);
osk_result_t osk_aes256_cbc_decrypt(const uint8_t key[32], const uint8_t iv[OSK_AES_BLOCK],
				    const uint8_t *in, uint8_t *out, size_t len);

/* Vergleich ohne Zeitverrat - fuer MACs und PIN-Abdruecke Pflicht. */
bool osk_memeq(const void *a, const void *b, size_t len);

#endif
