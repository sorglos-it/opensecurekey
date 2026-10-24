/* Die Schluessel, mit denen dieses Applet arbeitet - und wo sie liegen.
 *
 * Alles Geheime bleibt im SE050. Dieses Modul rechnet nichts selbst: es
 * baut die Bytefolge (osk_guard_msg.h) und laesst den Chip unterschreiben
 * oder den Schluessel austauschen.
 *
 * Drei Sorten:
 *
 * 1. Geraeteschluessel (Ed25519, OSK_GUARD_KEY_IDENTITY). Damit antwortet
 *    `attest`. Es ist der Schluessel, dessen oeffentlichen Teil der
 *    Hersteller eines geschuetzten Programms einmal mit `osk guard-key`
 *    ausliest und ins Programm eintraegt.
 *
 * 2. Knotenschluessel je Name (`sign`, `agree`). Ein Name wie "prod-eu-1"
 *    zeigt auf ein eigenes Objekt im Chip.
 *
 *    Warum eine Zuordnungstabelle und keine Ableitung aus dem Namen: der
 *    SE05x kann KEIN Schluesselpaar im Chip ableiten. Seine beiden
 *    Ableitungsbefehle (HKDF, PBKDF2) geben das Ergebnis an den Rechner
 *    heraus, und `osk_se050_key_derive_transient()` meldet darum
 *    OSK_ERR_UNSUPPORTED - siehe docs/se050.md. Der naheliegende Weg
 *    "ableiten, dann wieder hineinschreiben" fuehrt den privaten Teil
 *    durch den nRF52840, dessen Leseschutz dieses Projekt als gebrochen
 *    behandelt. Genau die Zusage waere damit weg. Also: je Name ein
 *    dauerhaftes Objekt, angelegt bei der Einrichtung.
 *
 *    Die Objektnummer wird NICHT aus dem Namen gerechnet. Zwei Namen mit
 *    gleichem Kurzwert wuerden sonst stillschweigend denselben Schluessel
 *    benutzen, und eine Unterschrift fuer "test" waere eine fuer "prod".
 *    Ein unbekannter Name bekommt OSK_ERR_NOT_FOUND, nie einen Schluessel.
 *
 * 3. Grundgeheimnis (OSK_GUARD_KEY_ROOT). Aus ihm leitet `derive` ab, im
 *    Chip. Es wird nie herausgegeben; `derive` kann es nicht zurueckgeben,
 *    weil es nur osk_se050_hkdf() aufruft.
 */
#ifndef OSK_GUARD_KEYS_H
#define OSK_GUARD_KEYS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"
#include "osk_se050.h"
#include "osk_guard_msg.h"

/* Objektnummern im Chip. Eigener Bereich, damit sie sich mit CTAP
 * (0x00f1d0xx) und den Kartenanwendungen nicht ueberschneiden. */
#define OSK_GUARD_KEY_ROOT 0x00f1e000u
#define OSK_GUARD_KEY_IDENTITY 0x00f1e001u
#define OSK_GUARD_KEY_NODE_BASE 0x00f1e010u

/* So viele benannte Knotenschluessel passen in die Tabelle. Jeder belegt
 * dauerhaft Platz im Chip (104 kB beim SE051C2), also keine grosse Zahl. */
#define OSK_GUARD_NODE_MAX 8u

/* Was `derive` hoechstens herausgibt und wie lang `info` sein darf. */
#define OSK_GUARD_DERIVE_MAX 64u
#define OSK_GUARD_INFO_MAX 128u

/* Welche Kurve ein Knotenschluessel hat. */
typedef enum {
	OSK_GUARD_CURVE_ED25519 = 0, /* unterschreiben */
	OSK_GUARD_CURVE_X25519,	     /* Schluesselaustausch */
} osk_guard_node_curve_t;

/* Tabelle leeren. Gehoert zum Start und zu den Pruefungen. */
void osk_guard_keys_reset(void);

/* Einen Namen eintragen und sein Objekt im Chip anlegen, falls es fehlt.
 * Das macht die Einrichtung ueber das Verwaltungsprogramm, nicht einer der
 * Befehle 14 bis 18 - ein Server soll sich keine neuen Identitaeten
 * ausstellen koennen.
 * OSK_ERR_FULL, wenn die Tabelle voll ist; OSK_ERR_EXISTS, wenn der Name
 * mit anderer Kurve schon eingetragen ist. */
osk_result_t osk_guard_node_register(const char *label, size_t label_len,
				     osk_guard_node_curve_t curve);

/* Objektnummer eines eingetragenen Namens. OSK_ERR_NOT_FOUND, wenn der
 * Name nicht eingetragen ist - geraten wird nichts. */
osk_result_t osk_guard_node_lookup(const char *label, size_t label_len,
				   osk_guard_node_curve_t curve, osk_key_id_t *id_out);

/* --- Was die Befehle brauchen ------------------------------------------- */

/* `attest`: unterschreibt eine fertige Bytefolge mit dem Geraeteschluessel
 * und liefert den oeffentlichen Teil mit. Der Zweck steht in der Bytefolge
 * schon drin; diese Funktion setzt keinen. */
osk_result_t osk_guard_keys_attest(const uint8_t *msg, size_t msg_len,
				   uint8_t sig_out[OSK_GUARD_SIG_LEN],
				   uint8_t pub_out[OSK_GUARD_PUB_LEN]);

/* `sign`: unterschreibt mit dem Knotenschluessel des Namens. */
osk_result_t osk_guard_keys_node_sign(const char *label, size_t label_len, const uint8_t *msg,
				      size_t msg_len, uint8_t sig_out[OSK_GUARD_SIG_LEN]);

/* `sign` mit `pub`: oeffentlicher Teil eines Knotenschluessels. */
osk_result_t osk_guard_keys_node_public(const char *label, size_t label_len,
					osk_guard_node_curve_t curve,
					uint8_t pub_out[OSK_GUARD_PUB_LEN]);

/* `agree`: X25519 im Chip. Kann der gelieferte Baustein kein ECDH, kommt
 * OSK_ERR_UNSUPPORTED durch - und nichts wird hilfsweise in der Firmware
 * gerechnet. */
osk_result_t osk_guard_keys_agree(const char *label, size_t label_len,
				  const uint8_t peer_pub[OSK_GUARD_PUB_LEN],
				  uint8_t secret_out[OSK_GUARD_PUB_LEN]);

/* `derive`: Ableitung aus dem Grundgeheimnis im Chip. Gibt nie das
 * Grundgeheimnis zurueck. */
osk_result_t osk_guard_keys_derive(const uint8_t *info, size_t info_len, uint8_t *out,
				   size_t out_len);

#endif
