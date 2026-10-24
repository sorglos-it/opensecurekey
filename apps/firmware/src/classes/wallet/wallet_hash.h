/* Rechenbausteine, die nur die Geldboerse braucht: SHA-512, HMAC-SHA512,
 * PBKDF2 (BIP39), RIPEMD-160 (Bitcoin-Adressen) und Keccak-256 (Ethereum).
 *
 * SHA-256, HMAC-SHA256 und das sichere Loeschen kommen aus
 * src/classes/ctap/osk_crypto.h - nicht noch einmal geschrieben.
 *
 * Auf der Zielplattform sollen SHA-256/SHA-512 auf PSA Crypto zeigen;
 * RIPEMD-160 und Keccak-256 kennt PSA nicht, die bleiben hier.
 * Alle Verfahren nach FIPS 180-4, FIPS 202, RFC 2104, RFC 8018 und der
 * RIPEMD-160-Veroeffentlichung von Dobbertin, Bosselaers und Preneel. */
#ifndef WALLET_HASH_H
#define WALLET_HASH_H

#include <stdint.h>
#include <stddef.h>
#include "osk_result.h"

#define OSK_SHA512_LEN 64
#define OSK_RIPEMD160_LEN 20
#define OSK_KECCAK256_LEN 32

typedef struct {
	uint64_t h[8];
	uint8_t block[128];
	size_t block_len;
	uint64_t total_lo; /* Laenge in Byte; ueber 2^64 Byte reicht kein Puffer */
} osk_sha512_t;

void osk_sha512_init(osk_sha512_t *c);
void osk_sha512_update(osk_sha512_t *c, const uint8_t *p, size_t len);
void osk_sha512_final(osk_sha512_t *c, uint8_t out[OSK_SHA512_LEN]);
void osk_sha512(const uint8_t *p, size_t len, uint8_t out[OSK_SHA512_LEN]);

void osk_hmac_sha512(const uint8_t *key, size_t key_len, const uint8_t *msg, size_t msg_len,
		     uint8_t out[OSK_SHA512_LEN]);

/* Zweiteilige Nachricht, ohne sie vorher zusammenkopieren zu muessen -
 * BIP32 haengt staendig ein Byte oder vier an einen Schluessel. */
void osk_hmac_sha512_2(const uint8_t *key, size_t key_len, const uint8_t *a, size_t a_len,
		       const uint8_t *b, size_t b_len, uint8_t out[OSK_SHA512_LEN]);

/* PBKDF2-HMAC-SHA512 nach RFC 8018. BIP39 verlangt genau 2048 Runden und
 * 64 Byte Ausgabe; mehr als 64 Byte gibt diese Fassung nicht her, weil die
 * Geldboerse nicht mehr braucht. */
osk_result_t osk_pbkdf2_sha512(const uint8_t *password, size_t password_len, const uint8_t *salt,
			       size_t salt_len, uint32_t rounds, uint8_t *out, size_t out_len);

void osk_ripemd160(const uint8_t *p, size_t len, uint8_t out[OSK_RIPEMD160_LEN]);

/* Keccak-256 in der Fassung VOR SHA-3: Auffuellbyte 0x01, nicht 0x06.
 * Ethereum benutzt diese; wer SHA3-256 nimmt, bekommt andere Adressen. */
void osk_keccak256(const uint8_t *p, size_t len, uint8_t out[OSK_KECCAK256_LEN]);

/* Bitcoin rechnet fast ueberall doppelt: SHA-256(SHA-256(x)). */
void osk_hash256(const uint8_t *p, size_t len, uint8_t out[32]);

/* RIPEMD-160(SHA-256(x)) - daraus entstehen Adressen und Schluesselkennungen. */
void osk_hash160(const uint8_t *p, size_t len, uint8_t out[OSK_RIPEMD160_LEN]);

#endif
