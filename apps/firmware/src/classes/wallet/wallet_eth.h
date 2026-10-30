/* Ethereum und die uebrigen EVM-Ketten.
 *
 * Unterschrieben wird genau der Bytestrom, den der Rechner geschickt hat -
 * gelesen wird er nur, um ihn anzuzeigen. Der Hash ist Keccak-256 ueber
 * dieselben Bytes; es wird nichts neu zusammengesetzt.
 *
 * Drei Arten von Transaktionen:
 *  - alte Art mit Kettennummer (EIP-155): eine Liste aus neun Werten
 *  - 0x01 (EIP-2930) und 0x02 (EIP-1559): ein Typbyte, dann die Liste
 *
 * Nachrichten:
 *  - EIP-191 "personal_sign": der Text kann angezeigt werden, also wird er
 *    angezeigt und ist damit wirklich geprueft
 *  - EIP-712: das Geraet bekommt nur die zwei fertigen Hashes
 *    (Bereichstrenner und Nachricht). Mehr geht nicht: die vollstaendige
 *    typisierte Struktur zu pruefen hiesse, einen JSON-Leser und das ganze
 *    Typsystem im Geraet zu haben. Darum gilt eine EIP-712-Unterschrift
 *    hier als NICHT lesbar - sie braucht die ausdrueckliche
 *    Extrafreigabe. Das ist die ehrliche Antwort, keine Bequemlichkeit.
 *
 * Was abgelehnt wird: eine Transaktion mit Daten (also ein Vertragsaufruf)
 * gilt als nicht lesbar. Ein Betrag und eine Adresse sagen bei einem
 * Vertragsaufruf nicht, was wirklich geschieht. */
#ifndef WALLET_ETH_H
#define WALLET_ETH_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"
#include "wallet_confirm.h"
#include "wallet_bip32.h"
#include "wallet_seed.h"

#define OSK_ETH_ADDRESS_LEN 20

typedef enum {
	OSK_ETH_LEGACY = 0,    /* EIP-155 */
	OSK_ETH_ACCESS = 1,    /* EIP-2930 */
	OSK_ETH_FEEMARKET = 2, /* EIP-1559 */
} osk_eth_tx_kind_t;

typedef struct {
	osk_eth_tx_kind_t kind;
	uint64_t chain_id;
	uint64_t nonce;
	uint64_t gas_limit;
	uint8_t max_fee[32];	  /* alte Art: der Gaspreis */
	uint8_t max_priority[32]; /* nur bei 0x02 */
	uint8_t to[OSK_ETH_ADDRESS_LEN];
	bool has_to; /* false heisst: ein Vertrag wird angelegt */
	uint8_t value[32];
	size_t data_len;
	bool has_access_list;
} osk_eth_tx_t;

/* Liest eine unsignierte Transaktion. Der Bytestrom ist genau der, der
 * gehasht wird. */
osk_result_t osk_eth_parse_tx(const uint8_t *raw, size_t len, osk_eth_tx_t *out);

/* Hash zum Unterschreiben: Keccak-256 ueber dieselben Bytes. */
osk_result_t osk_eth_tx_hash(const uint8_t *raw, size_t len, uint8_t out[32]);

/* Was auf dem Schirm steht. `unit` ist "ETH", "BNB", "POL" - je Kette ein
 * anderer Name, 18 Nachkommastellen in allen EVM-Ketten.
 * Bei einem Vertragsaufruf oder einem Vertrag ohne Empfaenger bleibt
 * `readable` false. */
osk_result_t osk_eth_view(const osk_eth_tx_t *tx, const char *unit, osk_wallet_view_t *view_out);

/* Hash einer Nachricht nach EIP-191: Keccak-256 ueber
 * "\x19Ethereum Signed Message:\n" + Laenge + Text. */
osk_result_t osk_eth_message_hash(const uint8_t *message, size_t len, uint8_t out[32]);

/* Hash nach EIP-712 aus den beiden fertigen Haelften:
 * Keccak-256(0x19 0x01 || Bereichstrenner || Nachrichtenhash). */
osk_result_t osk_eth_typed_hash(const uint8_t domain[32], const uint8_t message[32],
				uint8_t out[32]);

/* Adresse zu einem Schluessel: die letzten 20 Byte von
 * Keccak-256(oeffentlicher Punkt ohne das Kopfbyte). */
osk_result_t osk_eth_address(const osk_bip32_priv_t *key, uint8_t out[OSK_ETH_ADDRESS_LEN]);

/* Unterschreiben. Liefert r, s und die Kennziffer v, wie die jeweilige Art
 * sie verlangt:
 *  - alte Art mit Kettennummer: v = Kennziffer + 35 + 2 * Kettennummer
 *  - 0x01 und 0x02: v = Kennziffer (0 oder 1)
 *  - Nachrichten: v = Kennziffer + 27
 * Fragt vorher die Freigabe ueber wallet_confirm. */
osk_result_t osk_eth_sign_tx(const uint8_t *raw, size_t len, osk_item_id_t seed_id,
			     const char *asked_by, const char *path, const char *unit,
			     uint8_t r_out[32], uint8_t s_out[32],
			     uint64_t *v_out);

/* Eine Nachricht unterschreiben (EIP-191). Ist der Text druckbar, steht er
 * auf dem Schirm und gilt als lesbar; sonst braucht es die Extrafreigabe. */
osk_result_t osk_eth_sign_message(const uint8_t *message, size_t len, osk_item_id_t seed_id,
				  const char *asked_by, const char *path,
				  uint8_t r_out[32], uint8_t s_out[32], uint64_t *v_out);

/* Eine typisierte Nachricht unterschreiben (EIP-712). Immer mit
 * Extrafreigabe - siehe oben. */
osk_result_t osk_eth_sign_typed(const uint8_t domain[32], const uint8_t message[32],
				osk_item_id_t seed_id, const char *asked_by, const char *path,
				uint8_t r_out[32], uint8_t s_out[32],
				uint64_t *v_out);

#endif
