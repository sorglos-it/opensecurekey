/* Abgeleitete Schluessel: die Naht zwischen CTAP und dem Sicherheitschip.
 *
 * Es wird KEIN Schluessel je Dienst gespeichert. Aus dem Grundgeheimnis im
 * SE050 entsteht fuer jeden Berechtigungsnachweis ein eigener Schluessel;
 * die Credential-ID traegt nur das Salz, aus dem er wieder entsteht.
 *
 * Alles Geheime bleibt dabei im Chip: der CTAP-Kern sieht nur den
 * oeffentlichen Punkt und fertige Unterschriften. Diese Naht gibt es
 * genau deshalb - sie laesst sich in den Tests durch eine Attrappe
 * ersetzen, ohne dass der Kern etwas davon merkt. */
#ifndef OSK_CTAP_KEY_H
#define OSK_CTAP_KEY_H

#include <stdint.h>
#include <stddef.h>
#include "osk_result.h"

/* Objektnummern im SE050, die der CTAP-Kern benutzt. */
#define OSK_KEY_ID_ROOT      0x00f1d000u  /* Grundgeheimnis, nur zum Ableiten */
#define OSK_KEY_ID_KEYAGREE  0x00f1d001u  /* Schluesseltausch fuer das PIN-Protokoll */

/* Ableitungszwecke. Jeder Zweck hat seinen eigenen Text, damit zwei
 * Ableitungen nie denselben Wert liefern koennen - die largeBlobKey darf
 * laut Spezifikation nicht ueber hmac-secret erreichbar sein. */
#define OSK_INFO_CREDID      "osk/fido/credid/v1"
#define OSK_INFO_CREDKEY     "osk/fido/key/v1"
#define OSK_INFO_HMAC_UV     "osk/fido/hmac-uv/v1"
#define OSK_INFO_HMAC_NOUV   "osk/fido/hmac-nouv/v1"
#define OSK_INFO_LARGEBLOB   "osk/fido/largeblob/v1"

/* Laenge des Salzes in der Credential-ID. */
#define OSK_KEY_SALT_LEN 16

/* Oeffentlicher Schluessel des abgeleiteten Paares: X || Y, je 32 Bytes. */
osk_result_t osk_ctap_key_public(const uint8_t salt[OSK_KEY_SALT_LEN],
				 const uint8_t rp_id_hash[32],
				 uint8_t pub_out[64]);

/* Unterschreibt einen bereits gebildeten SHA-256-Hash. Die Unterschrift
 * kommt als ASN.1-DER (so verlangen es WebAuthn und U2F). */
osk_result_t osk_ctap_key_sign(const uint8_t salt[OSK_KEY_SALT_LEN],
			       const uint8_t rp_id_hash[32],
			       const uint8_t hash[32],
			       uint8_t *sig_out, size_t cap, size_t *len_out);

/* Geheimnisse, die nicht unterschreiben, sondern Schluessel fuer andere
 * Verfahren sind: CredRandom fuer hmac-secret und der largeBlobKey. */
osk_result_t osk_ctap_key_secret(const char *purpose,
				 const uint8_t salt[OSK_KEY_SALT_LEN],
				 const uint8_t rp_id_hash[32],
				 uint8_t out[32]);

/* Schluessel, mit dem die Credential-ID verpackt wird. Wird bei jedem
 * Gebrauch neu aus dem Chip geholt und danach geloescht - im RAM soll
 * kein dauerhaftes Geheimnis liegen. */
osk_result_t osk_ctap_key_wrap_keys(uint8_t enc_out[32], uint8_t mac_out[32]);

/* ---- Was der SE050-Strang noch beisteuern muss -------------------------- */
/* Der Kern braucht fuer abgeleitete Signierschluessel einen Weg, im Chip ein
 * fluechtiges Schluesselobjekt aus dem Grundgeheimnis zu bilden. Das steht
 * noch nicht in osk_se050.h; bis es dort steht, wird dieser Name hier
 * erwartet und von der Attrappe der Tests gestellt.
 *
 * Erwartete Bedeutung: leitet im Chip aus dem Objekt root mit info ein
 * P-256-Schluesselpaar ab, legt es in das fluechtige Objekt slot und loescht
 * dabei dessen vorherigen Inhalt. Der private Teil verlaesst den Chip nie. */
osk_result_t osk_se050_key_derive_transient(uint32_t root, const uint8_t *info,
					    size_t info_len, uint32_t slot);

/* Zweite Luecke: authenticatorReset muss die PIN loeschen, nicht nur
 * aendern - danach meldet GetInfo clientPin = false. Mit
 * osk_se050_pin_set() allein geht das nicht, weil es die alte PIN braucht.
 * Erwartete Bedeutung: loescht PIN und Fehlversuchszaehler im Chip. */
osk_result_t osk_se050_pin_clear(void);

#endif
