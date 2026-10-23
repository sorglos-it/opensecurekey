/* Krypto-Anschluss des Tresors. Hier steht nur, WAS gerechnet wird - gerechnet
 * wird woanders:
 *  - Auf dem Geraet: osk_crypto_psa.c, also die PSA-Crypto-Schnittstelle des
 *    nRF Connect SDK (AES-256-GCM, SHA-256, HKDF, Argon2id). Eigene Krypto
 *    wird nicht geschrieben.
 *  - Im Test am PC: apps/firmware/test/store/fake_crypto.c - eine
 *    nachvollziehbare Attrappe ohne jede Sicherheit. Sie prueft den Aufbau des
 *    Tresors, nicht die Verschluesselung.
 *
 * Der Tresorschluessel selbst kommt aus dem SE050 (osk_se050_hkdf) und liegt
 * nur als Sitzungsschluessel im RAM. */
#ifndef OSK_CRYPTO_H
#define OSK_CRYPTO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"

#define OSK_KEY_LEN 32	 /* AES-256 */
#define OSK_NONCE_LEN 12 /* GCM */
#define OSK_TAG_LEN 16
#define OSK_HASH_LEN 32 /* SHA-256 */
#define OSK_SALT_LEN 16

/* Sitzungsschluessel. Lebt nur im RAM und wird mit osk_crypto_wipe geloescht. */
typedef struct {
	uint8_t b[OSK_KEY_LEN];
} osk_key_t;

/* AES-256-GCM. ct_out braucht plain_len + OSK_TAG_LEN Platz; der Pruefwert
 * steht hinten im Kryptotext. aad darf NULL sein. */
osk_result_t osk_crypto_seal(const osk_key_t *key, const uint8_t nonce[OSK_NONCE_LEN],
			     const uint8_t *aad, size_t aad_len, const uint8_t *plain,
			     size_t plain_len, uint8_t *ct_out, size_t ct_cap, size_t *ct_len_out);

/* Entschluesselt und prueft. Bei falschem Pruefwert: OSK_ERR_FORMAT und
 * plain_out bleibt unberuehrt - es gibt nie halb entschluesselte Daten. */
osk_result_t osk_crypto_open(const osk_key_t *key, const uint8_t nonce[OSK_NONCE_LEN],
			     const uint8_t *aad, size_t aad_len, const uint8_t *ct, size_t ct_len,
			     uint8_t *plain_out, size_t plain_cap, size_t *plain_len_out);

osk_result_t osk_crypto_sha256(const uint8_t *msg, size_t len, uint8_t out[OSK_HASH_LEN]);

/* HKDF-SHA256 fuer Schluessel, die aus einem bereits geheimen Wert entstehen
 * (ECDH-Ergebnis, Sicherungsschluessel). Nicht fuer Kennwoerter. */
osk_result_t osk_crypto_hkdf(const uint8_t *ikm, size_t ikm_len, const uint8_t *salt,
			     size_t salt_len, const uint8_t *info, size_t info_len, uint8_t *out,
			     size_t out_len);

/* Argon2id fuer das Hauptkennwort der Sicherungsdatei. m_cost in KiB. */
typedef struct {
	uint32_t m_cost_kib;
	uint32_t t_cost;
	uint32_t lanes;
} osk_argon2_params_t;

osk_result_t osk_crypto_argon2id(const osk_argon2_params_t *p, const char *passphrase,
				 size_t pass_len, const uint8_t salt[OSK_SALT_LEN], uint8_t *out,
				 size_t out_len);

osk_result_t osk_crypto_random(uint8_t *out, size_t len);

/* Vergleich ohne Laufzeitunterschied - fuer Pruefwerte und Kennzahlen. */
bool osk_crypto_equal(const uint8_t *a, const uint8_t *b, size_t len);

/* Loescht einen Puffer so, dass der Uebersetzer es nicht wegoptimiert. */
void osk_crypto_wipe(void *buf, size_t len);

#endif
