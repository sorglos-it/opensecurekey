/* Stick zu Stick: den Tresor auf ein zweites Geraet bringen.
 *
 * Der Weg dazwischen - ein Rechner, ein Kabel, eine Funkstrecke - sieht nur
 * Kryptotext. Er darf alles mitschreiben und nichts verstehen.
 *
 * Ablauf, beide Geraete gleichzeitig:
 *   1. Jedes Geraet laesst seinen SE050 einen fluechtigen P-256-Schluessel
 *      anlegen und gibt nur den oeffentlichen Teil heraus.
 *   2. Die oeffentlichen Teile werden ueber den fremden Weg getauscht.
 *   3. Jeder SE050 rechnet daraus dasselbe gemeinsame Geheimnis
 *      (osk_se050_ecdh). Der Rechner dazwischen kann das nicht - er hat
 *      keinen der beiden privaten Teile.
 *   4. Aus dem gemeinsamen Geheimnis entsteht der Uebertragungsschluessel und
 *      eine sechsstellige Zahl. Beide Anzeigen zeigen sie; der Nutzer
 *      vergleicht und drueckt. Stimmen die Zahlen nicht, sitzt jemand in der
 *      Mitte - dann bricht die Uebertragung ab.
 *   5. Der Tresor wandert Stueck fuer Stueck, verschluesselt mit diesem
 *      Schluessel.
 *
 * Was nicht mitgeht:
 *   - Eintraege ohne das Kennzeichen "darf gespiegelt werden".
 *   - Alles, was ein Fingerabdrucksensor hervorbringt. Es liegt nicht im
 *     Tresor und kann darum auch nicht uebertragen werden. Stufe 1 hat gar
 *     keinen Sensor (docs/threat-model.md); kommt einer dazu, bleibt seine
 *     Vorlage im Sensor.
 *   - Das Grundgeheimnis des SE050. Dafuer gibt es den Kopplungsweg des
 *     Chips, nicht diesen hier.
 *
 * Umsetzung: src/classes/store/osk_transfer.c */
#ifndef OSK_TRANSFER_H
#define OSK_TRANSFER_H

#include <stdint.h>
#include <stddef.h>
#include "osk_result.h"
#include "osk_crypto.h"
#include "store_chunk.h"

#define OSK_TRANSFER_FMT 1
#define OSK_TRANSFER_PUB_MAX 65u /* P-256, unkomprimiert */
#define OSK_TRANSFER_DIGITS 6u	 /* Zahl zum Vergleichen */

/* Was beide Seiten waehrend einer Uebertragung bei sich haben. Das Geheime
 * darin wird mit osk_transfer_end geloescht. */
typedef struct {
	uint8_t own_pub[OSK_TRANSFER_PUB_MAX];
	size_t own_pub_len;
	uint8_t peer_pub[OSK_TRANSFER_PUB_MAX];
	size_t peer_pub_len;
	osk_key_t key;
	char digits[OSK_TRANSFER_DIGITS + 1];
	uint32_t seq;
	bool confirmed;
} osk_transfer_t;

/* Schritt 1: fluechtigen Schluessel anlegen und den oeffentlichen Teil
 * bereitstellen. */
osk_result_t osk_transfer_begin(osk_transfer_t *t);

/* Schritte 3 und 4: gemeinsames Geheimnis, Schluessel, Vergleichszahl - und
 * die Freigabe am Geraet. Ohne sie geht kein Byte hinaus. */
osk_result_t osk_transfer_accept_peer(osk_transfer_t *t, const uint8_t *peer_pub, size_t peer_len);

/* Schritt 5, gebende Seite: alle spiegelbaren Eintraege der aufgeschlossenen
 * Profile. */
osk_result_t osk_transfer_send(osk_transfer_t *t, osk_chunk_sink_fn sink, void *ctx,
			       size_t *sent_items);

/* Schritt 5, nehmende Seite. */
osk_result_t osk_transfer_receive(osk_transfer_t *t, osk_chunk_src_fn src, void *ctx,
				  size_t *got_items);

void osk_transfer_end(osk_transfer_t *t);

#endif
