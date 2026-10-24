/* Die Rechenteile, die Yubico-OTP und Challenge-Response brauchen.
 *
 * Alles selbst geschrieben nach der jeweiligen Spezifikation, nicht
 * uebernommen:
 *   SHA-1       FIPS 180-4, Abschnitt 6.1
 *   HMAC        RFC 2104
 *   AES-128     FIPS 197 (nur Verschluesseln, ein Block, keine Betriebsart)
 *   CRC-16      ISO/IEC 13239, umgedrehtes Polynom 0x8408
 *   ModHex      die Zuordnung der 16 Halbbytes auf Buchstaben
 *
 * Diese Verfahren sind alt und zum Teil gebrochen (SHA-1 als Hash, AES im
 * Einzelblock ohne Betriebsart). Sie stehen hier, weil die Gegenseite -
 * KeePassXC, VeraCrypt, der Yubico-Pruefdienst - nichts anderes versteht.
 * Fuer alles Neue nimmt das Geraet den SE050. Siehe docs/smartcard.md.
 */
#ifndef OSK_OTP_CRYPTO_H
#define OSK_OTP_CRYPTO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define OSK_SHA1_LEN 20u
#define OSK_SHA1_BLOCK 64u
#define OSK_AES_BLOCK 16u
#define OSK_AES128_KEY 16u

typedef struct {
	uint32_t h[5];
	uint64_t total; /* Zahl der Bytes insgesamt */
	uint8_t buf[OSK_SHA1_BLOCK];
	size_t have;
} osk_sha1_t;

void osk_sha1_init(osk_sha1_t *c);
void osk_sha1_update(osk_sha1_t *c, const uint8_t *data, size_t len);
void osk_sha1_final(osk_sha1_t *c, uint8_t out[OSK_SHA1_LEN]);
void osk_sha1(const uint8_t *data, size_t len, uint8_t out[OSK_SHA1_LEN]);

/* HMAC-SHA1 nach RFC 2104. Schluessel laenger als ein Block werden erst
 * gehasht, kuerzere mit Nullen aufgefuellt. */
void osk_hmac_sha1(const uint8_t *key, size_t key_len, const uint8_t *msg, size_t msg_len,
		   uint8_t out[OSK_SHA1_LEN]);

/* Ein Block AES-128, nur verschluesseln. Genau das braucht Yubico-OTP. */
void osk_aes128_encrypt_block(const uint8_t key[OSK_AES128_KEY], const uint8_t in[OSK_AES_BLOCK],
			      uint8_t out[OSK_AES_BLOCK]);

/* CRC-16 nach ISO/IEC 13239: Startwert 0xFFFF, Polynom 0x8408 von rechts,
 * ohne Abschluss-XOR. Pruefwert ueber "123456789": 0x6F91.
 *
 * Angehaengt wird nicht dieser Wert, sondern sein Kehrwert (XOR 0xFFFF) -
 * so macht es die Rahmenpruefung in ISO 13239. Nur dann bleibt beim Rechnen
 * ueber Daten UND Pruefsumme der feste Rest 0xF0B8 stehen, und genau daran
 * erkennt die Gegenseite einen heilen Block. */
uint16_t osk_crc16(const uint8_t *data, size_t len);
#define OSK_CRC16_RESIDUE 0xf0b8u
#define OSK_CRC16_XOROUT 0xffffu

/* ModHex: je Halbbyte ein Buchstabe. Die Zuordnung ist fest:
 * 0123456789abcdef -> cbdefghijklnrtuv.
 * `out` braucht 2*len+1 Bytes und endet auf eine Null. */
bool osk_modhex_encode(const uint8_t *data, size_t len, char *out, size_t cap);

/* Zurueck. Liefert false bei einem Buchstaben, der nicht dazugehoert. */
bool osk_modhex_decode(const char *text, size_t text_len, uint8_t *out, size_t cap,
		       size_t *len_out);

#endif
