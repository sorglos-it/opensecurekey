/* Ableitung nach BIP32, BIP44 und SLIP-0010 - in der Firmware.
 *
 * Warum in der Firmware und nicht im Chip: der SE05x kann kein
 * Schluesselpaar intern ableiten (AN12413, belegt in docs/se050.md). Der
 * Seed liegt darum im Tresor, und die Ableitung rechnet hier. Zum
 * UNTERSCHREIBEN geht der abgeleitete Schluessel an den SE050
 * (wallet_signer.h); kann der Chip die Kurve nicht, wird nicht unterschrieben
 * und das Programm sagt es.
 *
 * Zwei Kurven:
 *  - secp256k1: Bitcoin, Ethereum und alle EVM-Ketten. BIP32 und SLIP-0010
 *    rechnen hier gleich; der Unterschied ist nur, was bei dem einen Fall
 *    aus 2^127 geschieht (siehe wallet_bip32.c).
 *  - ed25519 nach SLIP-0010: nur gehaertete Schritte, das ist dort so
 *    festgelegt. Ob der SE050 diese Kurve kann, fragt die Signaturebene -
 *    ein geliefertes Stueck kann EdDSA ab Werk nicht haben.
 *
 * Nichts in diesem Modul fragt nach einer Freigabe und nichts zeigt etwas
 * an. Das macht wallet_confirm.h, und zwar immer vor dem Unterschreiben. */
#ifndef WALLET_BIP32_H
#define WALLET_BIP32_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"
#include "wallet_ec.h"

#define OSK_BIP32_KEY_LEN 32
#define OSK_BIP32_CHAIN_LEN 32
#define OSK_BIP32_MAX_DEPTH 8
#define OSK_BIP32_SERIALIZED 78
#define OSK_BIP32_TEXT_MAX 120
#define OSK_BIP32_HARDENED 0x80000000u

/* Versionsbytes der Hauptkette. Andere Werte (ypub, zpub, tpub) gibt der
 * Aufrufer an - die Zahl gehoert zur Anwendung, nicht zur Ableitung. */
#define OSK_BIP32_XPRV_MAIN 0x0488ADE4u
#define OSK_BIP32_XPUB_MAIN 0x0488B21Eu

typedef enum {
	OSK_BIP32_SECP256K1 = 0,
	OSK_BIP32_ED25519 = 1,
} osk_bip32_curve_t;

typedef struct {
	osk_bip32_curve_t curve;
	uint8_t key[OSK_BIP32_KEY_LEN];
	uint8_t chain[OSK_BIP32_CHAIN_LEN];
	uint8_t depth;
	uint32_t child;	    /* Index des Schrittes, der zu diesem Knoten fuehrte */
	uint32_t parent_fp; /* Fingerabdruck des Elternknotens, 0 an der Wurzel */
} osk_bip32_priv_t;

typedef struct {
	uint8_t pub[OSK_EC_PUB_COMPRESSED];
	uint8_t chain[OSK_BIP32_CHAIN_LEN];
	uint8_t depth;
	uint32_t child;
	uint32_t parent_fp;
} osk_bip32_pub_t;

/* Wurzel aus dem BIP39-Seed. seed_len 16 bis 64. */
osk_result_t osk_bip32_master(const uint8_t *seed, size_t seed_len, osk_bip32_curve_t curve,
			      osk_bip32_priv_t *out);

/* Ein Schritt. Index ab OSK_BIP32_HARDENED ist gehaertet.
 * Bei ed25519 ist ein nicht gehaerteter Schritt OSK_ERR_UNSUPPORTED - so
 * steht es in SLIP-0010, und es still durchzulassen ergaebe Schluessel, die
 * keine andere Geldboerse findet. */
osk_result_t osk_bip32_derive_child(const osk_bip32_priv_t *parent, uint32_t index,
				    osk_bip32_priv_t *out);

/* Ein Schritt ohne Geheimnis, aus einem xpub. Nur nicht gehaertet - das ist
 * der Sinn der Haertung. */
osk_result_t osk_bip32_pub_derive_child(const osk_bip32_pub_t *parent, uint32_t index,
					osk_bip32_pub_t *out);

/* Pfad aus Text: "m/44'/0'/0'/0/0". Erlaubt sind ' und h fuer gehaertet.
 * Hoechstens OSK_BIP32_MAX_DEPTH Schritte. */
osk_result_t osk_bip32_path_parse(const char *text, uint32_t *out, size_t cap, size_t *len_out);

osk_result_t osk_bip32_derive_path(const osk_bip32_priv_t *master, const char *path,
				   osk_bip32_priv_t *out);

/* Oeffentlicher Teil. Bei ed25519 steht in pub[0] ein 0x00 und dahinter der
 * 32-Byte-Punkt - so legt es SLIP-0010 fest. Diese Fassung liefert fuer
 * ed25519 OSK_ERR_UNSUPPORTED, weil der oeffentliche Punkt dort nicht ohne
 * eine zweite Kurvenrechnung entsteht und der SE050 sie uns liefern soll. */
osk_result_t osk_bip32_neuter(const osk_bip32_priv_t *in, osk_bip32_pub_t *out);

/* Erste vier Byte von RIPEMD-160(SHA-256(pub)) - so zeigt BIP32 auf den
 * Elternknoten, und PSBT findet damit heraus, welche Eingabe uns gehoert. */
uint32_t osk_bip32_fingerprint(const uint8_t pub[OSK_EC_PUB_COMPRESSED]);

osk_result_t osk_bip32_serialize_pub(const osk_bip32_pub_t *in, uint32_t version, char *out,
				     size_t cap, size_t *len_out);
osk_result_t osk_bip32_serialize_priv(const osk_bip32_priv_t *in, uint32_t version, char *out,
				      size_t cap, size_t *len_out);

/* Liest einen xpub zurueck. Prueft Laenge, Version und dass das Byte vor
 * dem Punkt zu einem oeffentlichen Schluessel passt. */
osk_result_t osk_bip32_parse_pub(const char *text, uint32_t expect_version, osk_bip32_pub_t *out);

void osk_bip32_wipe(osk_bip32_priv_t *key);

#endif
