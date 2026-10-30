/* PSBT nach BIP174 - der Hauptweg. Electrum, Sparrow und Bitcoin Core
 * sprechen ihn.
 *
 * Die Arbeitsweise ist bewusst so gewaehlt, dass das Geraet die
 * Transaktion nicht zweimal im Speicher haelt: beim Lesen entstehen nur
 * Zeiger in den Puffer des Aufrufers. Beim Zurueckgeben werden die
 * gelesenen Abschnitte woertlich kopiert und nur die eigenen
 * Unterschriften eingefuegt - so geht kein Feld verloren, das wir nicht
 * kennen, und es wird auch keines verfaelscht.
 *
 * Grenzen, und was bei Ueberschreitung geschieht: mehr als
 * OSK_PSBT_MAX_IN Eingaenge oder OSK_PSBT_MAX_OUT Ausgaenge werden
 * ABGELEHNT. Nicht teilweise unterschrieben - eine halb unterschriebene
 * Ueberweisung ist schlimmer als keine.
 *
 * Was abgelehnt wird:
 *  - alles ausser SIGHASH_ALL
 *  - Taproot-Eingaenge (BIP341 ist ein eigenes Hashverfahren, siehe
 *    wallet_btc.h)
 *  - ein Zeugen-UTXO, dessen Skript nicht zu dem passt, was wir
 *    unterschreiben wuerden
 *  - ein nicht-Zeugen-UTXO, dessen Transaktionskennung nicht zum Eingang
 *    passt (sonst liesse sich der Betrag und damit die Gebuehr faelschen)
 */
#ifndef WALLET_PSBT_H
#define WALLET_PSBT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"
#include "wallet_btc.h"
#include "wallet_bip32.h"
#include "wallet_confirm.h"
#include "osk_store.h"

#define OSK_PSBT_MAX_IN OSK_BTC_MAX_IN
#define OSK_PSBT_MAX_OUT OSK_BTC_MAX_OUT
#define OSK_PSBT_MAX_DERIV 4
#define OSK_PSBT_MAX_PATH OSK_BIP32_MAX_DEPTH

/* Eine Ableitungsangabe, wie sie in Feld 0x06 steht: zu welchem
 * oeffentlichen Schluessel welcher Pfad unter welchem Geraet gehoert. */
typedef struct {
	uint8_t pub[OSK_EC_PUB_COMPRESSED];
	uint32_t fingerprint;
	uint32_t path[OSK_PSBT_MAX_PATH];
	size_t path_len;
} osk_psbt_deriv_t;

typedef struct {
	const uint8_t *start; /* Abschnitt im Eingabepuffer, ohne Trennbyte */
	size_t len;

	bool has_witness_utxo;
	uint64_t value;		    /* Betrag des Eingangs, 0 wenn unbekannt */
	const uint8_t *utxo_script; /* Skript des Eingangs */
	size_t utxo_script_len;

	const uint8_t *redeem_script;
	size_t redeem_script_len;
	const uint8_t *witness_script;
	size_t witness_script_len;

	uint32_t sighash_type; /* 0 = nicht angegeben, dann SIGHASH_ALL */
	bool already_signed;   /* Feld 0x02 oder 0x07/0x08 vorhanden */

	osk_psbt_deriv_t deriv[OSK_PSBT_MAX_DERIV];
	size_t deriv_count;
} osk_psbt_input_t;

typedef struct {
	const uint8_t *start;
	size_t len;
	osk_psbt_deriv_t deriv[OSK_PSBT_MAX_DERIV];
	size_t deriv_count;
} osk_psbt_output_t;

typedef struct {
	const uint8_t *raw;
	size_t raw_len;
	const uint8_t *global_start;
	size_t global_len;

	osk_btc_tx_t tx;
	size_t in_count;
	size_t out_count;
	osk_psbt_input_t in[OSK_PSBT_MAX_IN];
	osk_psbt_output_t out[OSK_PSBT_MAX_OUT];
} osk_psbt_t;

/* Liest und prueft. Der Puffer muss bis zum letzten Zugriff auf `out`
 * unveraendert liegen bleiben - `out` enthaelt Zeiger darauf. */
osk_result_t osk_psbt_parse(const uint8_t *raw, size_t len, osk_psbt_t *out);

/* Was auf dem Schirm stehen soll: Empfaenger, Betrag, Gebuehr.
 * `master_fp` ist der Fingerabdruck unseres Wurzelschluessels - damit
 * werden eigene Ausgaenge (Wechselgeld) erkannt und nicht als Empfaenger
 * gezeigt.
 * Kann ein Ausgang nicht in eine Adresse umgesetzt werden oder fehlt ein
 * Betrag, wird `readable` auf false gesetzt - und dann unterschreibt
 * wallet_confirm nicht ohne ausdrueckliche Extrafreigabe. */
osk_result_t osk_psbt_view(const osk_psbt_t *psbt, uint32_t master_fp, const char *hrp,
			   uint8_t p2pkh_version, uint8_t p2sh_version,
			   osk_wallet_view_t *view_out);

/* Unterschreibt jede Eingabe, deren Ableitungsangabe auf unseren
 * Wurzelschluessel zeigt, und schreibt das Ergebnis als neues PSBT nach
 * `out`. Fragt VORHER die Freigabe ueber wallet_confirm.
 *
 * `seed_id` ist der Tresoreintrag mit dem Seed; der Seed wird einmal
 * geholt, alle Eingaben werden abgeleitet und unterschrieben, und
 * anschliessend wird alles geloescht.
 *
 * `signed_out` sagt, wie viele Eingaben unterschrieben wurden. Null ist
 * kein Fehler: dann gehoerte keine Eingabe zu uns. */
osk_result_t osk_psbt_sign(const osk_psbt_t *psbt, osk_item_id_t seed_id, const char *asked_by,
			   const char *hrp, uint8_t p2pkh_version, uint8_t p2sh_version,
			   uint8_t *out, size_t cap, size_t *len_out,
			   size_t *signed_out);

#endif
