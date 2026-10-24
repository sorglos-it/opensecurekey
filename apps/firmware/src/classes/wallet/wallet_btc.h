/* Bitcoin: Transaktion lesen, Adresse anzeigen, Hash zum Unterschreiben
 * bilden.
 *
 * Feste Grenzen, damit nichts auf dem Haufen liegt. Eine Ueberweisung mit
 * mehr Eingaengen oder Ausgaengen wird abgelehnt, nicht teilweise
 * bearbeitet - der Nutzer kann sie dann aufteilen.
 *
 * Was hier NICHT drin ist, und warum:
 *  - Taproot (BIP341). Das ist ein eigenes Hashverfahren, und eine halbe
 *    Umsetzung waere gefaehrlicher als keine. Eine Taproot-Eingabe wird
 *    erkannt und abgelehnt.
 *  - Fertigstellen der Transaktion. Das Geraet gibt Unterschriften heraus;
 *    zusammengesetzt wird am Rechner. So muss das Geraet die fertige
 *    Transaktion nie im Speicher halten.
 *
 * Quellen: BIP143 (Hash fuer SegWit v0), BIP16/141/173, und die
 * Serialisierung aus der Referenzbeschreibung des Formats. */
#ifndef WALLET_BTC_H
#define WALLET_BTC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"

#define OSK_BTC_MAX_IN 8
#define OSK_BTC_MAX_OUT 8
#define OSK_BTC_SCRIPT_MAX 128
#define OSK_BTC_ADDRESS_MAX 96

/* Hashtyp. Diese Fassung unterschreibt nur SIGHASH_ALL - jeder andere Wert
 * laesst Teile der Transaktion austauschbar, und das kann der Nutzer auf
 * einem kleinen Schirm nicht beurteilen. */
#define OSK_BTC_SIGHASH_ALL 0x01u

typedef enum {
	OSK_BTC_UNKNOWN = 0,
	OSK_BTC_P2PKH,	/* Adresse beginnt mit 1 */
	OSK_BTC_P2SH,	/* Adresse beginnt mit 3 */
	OSK_BTC_P2WPKH, /* bc1q..., 20 Byte */
	OSK_BTC_P2WSH,	/* bc1q..., 32 Byte */
	OSK_BTC_P2TR,	/* bc1p... - erkannt, aber nicht unterschrieben */
	OSK_BTC_OPRETURN,
} osk_btc_script_kind_t;

typedef struct {
	uint8_t txid[32]; /* so wie im Rohformat: niederwertig zuerst */
	uint32_t vout;
	uint32_t sequence;
} osk_btc_input_t;

typedef struct {
	uint64_t value; /* Satoshi */
	uint8_t script[OSK_BTC_SCRIPT_MAX];
	size_t script_len;
} osk_btc_output_t;

typedef struct {
	uint32_t version;
	uint32_t locktime;
	size_t in_count;
	size_t out_count;
	osk_btc_input_t in[OSK_BTC_MAX_IN];
	osk_btc_output_t out[OSK_BTC_MAX_OUT];
} osk_btc_tx_t;

/* Eine unsignierte Transaktion lesen. Lehnt alles ab, was nicht genau
 * passt: zu viele Ein- oder Ausgaenge, Reste am Ende, Skripte ueber der
 * Grenze, Eingaenge mit Skript (eine unsignierte Transaktion hat keine). */
osk_result_t osk_btc_parse_tx(const uint8_t *raw, size_t len, osk_btc_tx_t *out);

/* Dieselben Bytes wieder herstellen - damit die Pruefungen zeigen koennen,
 * dass beim Lesen nichts verloren geht. */
osk_result_t osk_btc_serialize_tx(const osk_btc_tx_t *tx, uint8_t *out, size_t cap,
				  size_t *len_out);

osk_btc_script_kind_t osk_btc_script_kind(const uint8_t *script, size_t len);

/* Adresse als Text, aus dem Ausgabeskript. `hrp` ist "bc" fuer das
 * Hauptnetz, "tb" fuer das Testnetz; `p2pkh_version` und `p2sh_version`
 * sind 0x00 und 0x05 im Hauptnetz.
 * Kann ein Skript nicht in eine Adresse umgesetzt werden, kommt
 * OSK_ERR_UNSUPPORTED - und dann wird nach der Regel aus wallet_confirm.h
 * nicht unterschrieben. */
osk_result_t osk_btc_address_from_script(const uint8_t *script, size_t len, const char *hrp,
					 uint8_t p2pkh_version, uint8_t p2sh_version, char *out,
					 size_t cap, size_t *len_out);

/* Skripte bauen. */
osk_result_t osk_btc_script_p2pkh(const uint8_t hash160[20], uint8_t *out, size_t cap,
				  size_t *len_out);
osk_result_t osk_btc_script_p2wpkh(const uint8_t hash160[20], uint8_t *out, size_t cap,
				   size_t *len_out);

/* Hash der alten Art (vor SegWit). `script_code` ist das Skript der
 * Eingabe, die unterschrieben wird; alle anderen Eingaben bekommen ein
 * leeres Skript. */
osk_result_t osk_btc_sighash_legacy(const osk_btc_tx_t *tx, size_t index,
				    const uint8_t *script_code, size_t script_len,
				    uint32_t hash_type, uint8_t out[32]);

/* Hash nach BIP143 (SegWit v0). Braucht zusaetzlich den Betrag der
 * Eingabe - genau das ist der Fortschritt von BIP143: der Betrag steht im
 * Hash, und darum kann das Geraet die Gebuehr ausrechnen. */
osk_result_t osk_btc_sighash_segwit(const osk_btc_tx_t *tx, size_t index,
				    const uint8_t *script_code, size_t script_len, uint64_t value,
				    uint32_t hash_type, uint8_t out[32]);

/* Hilfen fuer das Lesen von Zahlen im Bitcoin-Format. Jede prueft vorher,
 * ob noch genug Bytes da sind. */
bool osk_btc_read_u32(const uint8_t *p, size_t len, size_t *pos, uint32_t *out);
bool osk_btc_read_u64(const uint8_t *p, size_t len, size_t *pos, uint64_t *out);
bool osk_btc_read_varint(const uint8_t *p, size_t len, size_t *pos, uint64_t *out);
size_t osk_btc_write_varint(uint64_t value, uint8_t *out);

#endif
