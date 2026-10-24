/* Schreibweisen, die Bitcoin und Ethereum verlangen: Base58Check (Adressen
 * der alten Art, xpub/xprv), Bech32 und Bech32m (SegWit und Taproot) sowie
 * Hex mit Pruefsumme nach EIP-55.
 *
 * Alles mit festen Puffern und immer mit Laengenpruefung vor dem Lesen.
 * Quellen: BIP13/16 (Base58Check), BIP173 (Bech32), BIP350 (Bech32m),
 * EIP-55 (Gross-/Kleinschreibung als Pruefsumme). */
#ifndef WALLET_CODEC_H
#define WALLET_CODEC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"

/* Laengste Nutzlast, die hier vorkommt: 78 Byte erweiterter Schluessel
 * (xpub) plus 4 Byte Pruefsumme. */
#define OSK_BASE58_MAX_BIN 96
#define OSK_BASE58_MAX_TEXT 140

/* Base58Check: haengt SHA-256(SHA-256(x))[0..3] an und kodiert. */
osk_result_t osk_base58check_encode(const uint8_t *in, size_t len, char *out, size_t cap,
				    size_t *len_out);
/* Prueft die Pruefsumme und gibt nur die Nutzlast heraus. */
osk_result_t osk_base58check_decode(const char *in, uint8_t *out, size_t cap, size_t *len_out);

/* Bech32 ohne Pruefsummen-Variante nach aussen: `witness_version` 0 ergibt
 * Bech32, 1 bis 16 ergibt Bech32m - so steht es in BIP350. `program` ist
 * das Zeugenprogramm (20 Byte bei P2WPKH, 32 bei P2WSH und Taproot). */
osk_result_t osk_bech32_encode_address(const char *hrp, uint8_t witness_version,
				       const uint8_t *program, size_t program_len, char *out,
				       size_t cap, size_t *len_out);

/* Liest eine SegWit-Adresse. Prueft Pruefsumme, Variante, Laenge und dass
 * nicht gemischt gross und klein geschrieben ist. */
osk_result_t osk_bech32_decode_address(const char *in, const char *expect_hrp, uint8_t *version_out,
				       uint8_t *program_out, size_t cap, size_t *program_len_out);

/* Ethereum-Adresse als Text: "0x" und 40 Hexstellen, gross/klein nach
 * EIP-55. Braucht 43 Byte Platz. */
osk_result_t osk_eth_address_text(const uint8_t address[20], char *out, size_t cap);

/* Hexstellen lesen, mit Laengenpruefung. Lehnt alles ab, was keine
 * Hexstelle ist - kein Ueberlesen, kein Raten. */
osk_result_t osk_hex_decode(const char *in, size_t in_len, uint8_t *out, size_t cap,
			    size_t *len_out);

#endif
