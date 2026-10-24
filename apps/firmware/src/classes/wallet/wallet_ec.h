/* secp256k1 - die Kurve von Bitcoin und Ethereum.
 *
 * Was hier NICHT steht: Signieren. Unterschrieben wird im SE050
 * (wallet_signer.h). Hier liegt nur, was ohne den privaten Schluessel nicht
 * geht und was der Chip nicht anbietet:
 *  - den oeffentlichen Punkt zu einem abgeleiteten Schluessel rechnen
 *    (BIP32 braucht ihn fuer jede Adresse und fuer xpub)
 *  - einen oeffentlichen Punkt um einen Wert verschieben (nicht gehaertete
 *    Ableitung aus einem xpub, ganz ohne Geheimnis)
 *  - aus einer fertigen Signatur den Punkt zurueckrechnen; Ethereum braucht
 *    dafuer die Kennziffer v, und die steht in keiner ECDSA-Signatur drin
 *
 * Alle Zahlen kommen und gehen als 32 Byte, hoechstwertiges Byte zuerst. */
#ifndef WALLET_EC_H
#define WALLET_EC_H

#include <stdint.h>
#include <stdbool.h>
#include "wallet_bn.h"
#include "osk_result.h"

#define OSK_EC_PUB_COMPRESSED 33
#define OSK_EC_PUB_UNCOMPRESSED 65

typedef struct {
	osk_bn_t x;
	osk_bn_t y;
	bool infinity;
} osk_ec_point_t;

/* Ordnung n und Koerpergroesse p - gebraucht fuer Gueltigkeitspruefungen
 * und fuer die Regel "kleines s" (BIP62, EIP-2). */
const osk_bn_t *osk_ec_order(void);
const osk_bn_t *osk_ec_field(void);

/* Ist der Wert ein brauchbarer privater Schluessel? 0 und alles ab n nicht.
 * BIP32 verlangt genau diese Pruefung bei jedem Ableitungsschritt. */
bool osk_ec_privkey_valid(const uint8_t key[32]);

/* Oeffentlicher Punkt zu einem privaten Schluessel. */
osk_result_t osk_ec_public_from_private(const uint8_t priv[32], osk_ec_point_t *out);

/* 33 Byte (02/03 + x) oder 65 Byte (04 + x + y). */
osk_result_t osk_ec_point_encode(const osk_ec_point_t *p, bool compressed, uint8_t *out, size_t cap,
				 size_t *len_out);
osk_result_t osk_ec_point_decode(const uint8_t *in, size_t len, osk_ec_point_t *out);

/* out = a + b. Beide duerfen gleich sein. */
void osk_ec_point_add(osk_ec_point_t *out, const osk_ec_point_t *a, const osk_ec_point_t *b);

/* out = k * G */
osk_result_t osk_ec_point_mul_base(const osk_bn_t *k, osk_ec_point_t *out);

/* out = p + t*G - der Schritt der nicht gehaerteten Ableitung aus einem xpub.
 * OSK_ERR_FORMAT, wenn dabei der unendlich ferne Punkt herauskommt; BIP32
 * sagt, dass dann der naechste Index zu nehmen ist. */
osk_result_t osk_ec_pub_tweak_add(const osk_ec_point_t *p, const uint8_t tweak[32],
				  osk_ec_point_t *out);

/* out = (a + b) mod n, mit derselben Pruefung wie oben. */
osk_result_t osk_ec_priv_tweak_add(const uint8_t key[32], const uint8_t tweak[32], uint8_t out[32]);

/* Die Kennziffer v einer fertigen Signatur. Ethereum braucht sie, damit der
 * Empfaenger den Absender zurueckrechnen kann; der SE050 liefert sie nicht
 * mit, also wird sie hier gesucht: beide moeglichen Punkte zurueckrechnen
 * und mit dem bekannten oeffentlichen Schluessel vergleichen.
 * Ergebnis 0 oder 1. OSK_ERR_FORMAT, wenn keiner passt - dann gehoert die
 * Signatur nicht zu diesem Schluessel. */
osk_result_t osk_ec_recovery_id(const uint8_t hash[32], const uint8_t r[32], const uint8_t s[32],
				const osk_ec_point_t *expected, uint8_t *recid_out);

/* Prueft eine Signatur gegen einen oeffentlichen Punkt. Die Geldboerse
 * prueft damit jede Antwort des SE050 nach, bevor sie sie herausgibt -
 * eine falsche Signatur auf einer Ueberweisung kostet das Geld zweimal. */
bool osk_ec_verify(const uint8_t hash[32], const uint8_t r[32], const uint8_t s[32],
		   const osk_ec_point_t *pub);

/* Ist s groesser als n/2? Dann verlangen BIP62 und EIP-2 n - s. */
bool osk_ec_s_is_high(const uint8_t s[32]);
void osk_ec_s_negate(const uint8_t s[32], uint8_t out[32]);

#endif
