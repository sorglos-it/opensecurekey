/* Sicherheitschip SE050: alles Geheime entsteht hier und bleibt hier.
 * Diese Schnittstelle ist bewusst klein - sie sagt, WAS der Chip tun soll,
 * nicht wie die APDU dafuer aussieht.
 *
 * Umsetzung: src/classes/se050/ (T=1' ueber I2C, APDU nach AN12413, SCP03) */
#ifndef OSK_SE050_H
#define OSK_SE050_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"

/* Kennung eines Objekts im Chip. Wird vom Aufrufer vergeben und bleibt stabil. */
typedef uint32_t osk_key_id_t;

typedef enum {
	OSK_CURVE_P256, /* FIDO2, PIV, TLS */
	OSK_CURVE_P384,
	OSK_CURVE_SECP256K1, /* Bitcoin, Ethereum */
	OSK_CURVE_ED25519,   /* Signieren: SSH, Solana, Firmware */
	OSK_CURVE_X25519,    /* Schluesselaustausch Server zu Server */
} osk_curve_t;

typedef struct {
	bool exportable;  /* false = verlaesst den Chip nie (Vorgabe) */
	bool needs_touch; /* Tastendruck je Benutzung verlangen */
} osk_key_policy_t;

/* Chip anmelden und den verschluesselten Kanal aufbauen.
 * Ohne erfolgreiches open() antwortet keine andere Funktion. */
osk_result_t osk_se050_open(void);
void osk_se050_close(void);

/* Welche Kurven dieser Chip wirklich kann - wird beim Start abgefragt und
 * nicht angenommen. Liefert eine Bitmaske aus (1 << osk_curve_t). */
osk_result_t osk_se050_curves(uint32_t *mask_out);

osk_result_t osk_se050_random(uint8_t *out, size_t len);

osk_result_t osk_se050_key_create(osk_key_id_t id, osk_curve_t curve,
				  const osk_key_policy_t *policy);
osk_result_t osk_se050_key_delete(osk_key_id_t id);
osk_result_t osk_se050_key_public(osk_key_id_t id, uint8_t *out, size_t cap, size_t *len_out);

/* Signiert den bereits gebildeten Hash. Das Bilden des Hashes gehoert
 * zum Aufrufer, damit die Anzeige genau das zeigt, was unterschrieben wird. */
osk_result_t osk_se050_sign(osk_key_id_t id, const uint8_t *hash, size_t hash_len, uint8_t *sig_out,
			    size_t cap, size_t *len_out);

/* Gemeinsames Geheimnis mit einem fremden oeffentlichen Schluessel. */
osk_result_t osk_se050_ecdh(osk_key_id_t id, const uint8_t *peer_pub, size_t peer_len,
			    uint8_t *secret_out, size_t cap, size_t *len_out);

/* Ableitung im Chip: daraus entstehen FIDO-Schluessel und der Tresorschluessel,
 * ohne dass das Grundgeheimnis je herauskommt. */
osk_result_t osk_se050_hkdf(osk_key_id_t id, const uint8_t *info, size_t info_len, uint8_t *out,
			    size_t out_len);

/* Fluechtiges Schluesselpaar im Chip aus dem Grundgeheimnis ableiten.
 * Noetig fuer FIDO2: je Dienst ein eigener Schluessel, aber nichts davon wird
 * gespeichert - und der private Teil darf den Chip nicht verlassen, weshalb
 * osk_se050_hkdf() allein nicht reicht.
 * Das Ergebnis liegt in `slot` und gilt bis zum naechsten Ableiten oder bis
 * osk_se050_close(). Gleiches `info` ergibt immer denselben Schluessel. */
osk_result_t osk_se050_key_derive_transient(osk_key_id_t root, osk_curve_t curve,
					    const uint8_t *info, size_t info_len,
					    osk_key_id_t slot);
osk_result_t osk_se050_hmac(osk_key_id_t id, const uint8_t *msg, size_t msg_len, uint8_t *mac_out,
			    size_t cap, size_t *len_out);

/* PIN wird im Chip geprueft, der Fehlversuchszaehler liegt ebenfalls dort. */
osk_result_t osk_se050_pin_verify(const uint8_t *pin, size_t len);
osk_result_t osk_se050_pin_set(const uint8_t *old_pin, size_t old_len, const uint8_t *new_pin,
			       size_t new_len);
osk_result_t osk_se050_pin_tries_left(uint8_t *tries_out);

/* PIN ganz entfernen, ohne die alte zu kennen - fuer das Zuruecksetzen auf
 * Werkszustand. Danach meldet CTAP `clientPin=false`. Ruft der Aufrufer das
 * ohne vorherige Freigabe am Geraet auf, ist das ein Fehler im Aufrufer:
 * osk_se050_pin_clear() prueft das nicht, es loescht. */
osk_result_t osk_se050_pin_clear(void);

#endif
