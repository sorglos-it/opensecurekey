/* Die Geraeteseite der Dongle- und Server-Befehle 14 bis 18 aus
 * shared/protocol.md.
 *
 *   14 attest   Zufallswert (+ Programmpruefsumme) hinein, Unterschrift und
 *               oeffentlicher Schluessel hinaus
 *   15 derive   Ableitung aus dem Grundgeheimnis; gibt es nie zurueck
 *   16 agree    X25519 im Chip; kann der gelieferte Baustein kein ECDH,
 *               kommt sauber OSK_ERR_UNSUPPORTED
 *   17 session  die Freischaltung fuer den Serverbetrieb
 *   18 sign     Auftrag unterschreiben, mit EIGENEM Zweck, getrennt von 14
 *
 * Rahmen: CBOR-Abbildung mit `v`, `c`, `s`, `d`; die Antwort traegt `v`, `s`,
 * `r` und `d`. Befehle ausserhalb von 14 bis 18 beantwortet dieses Applet
 * mit OSK_ERR_UNSUPPORTED - die Befehle 1 bis 13 gehoeren einem anderen
 * Bearbeiter, den es noch nicht gibt.
 *
 * Was hier NICHT passiert: rechnen. Jede Unterschrift, jeder
 * Schluesseltausch und jede Ableitung geht in den SE050 (osk_se050.h).
 * Die Zwecke setzt osk_guard_msg.c davor, im Geraet.
 *
 * Nichts davon ist an einem Geraet gelaufen - die Platine ist nicht gebaut.
 * Gemessen wird am PC gegen Attrappen, siehe test/guard/.
 */
#ifndef OSK_GUARD_FW_H
#define OSK_GUARD_FW_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"
#include "osk_transport.h"
#include "osk_guard_grant.h"
#include "osk_guard_keys.h"

/* Befehlsnummern aus shared/protocol.md. */
#define OSK_GUARD_CMD_ATTEST 14u
#define OSK_GUARD_CMD_DERIVE 15u
#define OSK_GUARD_CMD_AGREE 16u
#define OSK_GUARD_CMD_SESSION 17u
#define OSK_GUARD_CMD_SIGN 18u

/* Protokollfassung, die dieses Applet spricht. */
#define OSK_GUARD_PROTO_VERSION 1u

/* Meldet sich bei osk_transport_register fuer den eigenen Kanal an und
 * setzt Freischaltung und Schluesseltabelle auf den Startzustand: nichts
 * freigeschaltet. */
osk_result_t osk_guard_init(void);

/* Der Bearbeiter. Antwortet immer mit einem vollstaendigen Rahmen; der
 * Rueckgabewert ist OSK_OK, sobald eine Antwort geschrieben wurde - der
 * Grund steht dann in `r` der Antwort. */
osk_result_t osk_guard_handle(const osk_request_t *req, osk_response_t *res);

/* Fragt `attest` am Display nach? shared/protocol.md sagt "configurable".
 * Vorgabe: nein.
 *
 * Grund: ein geschuetztes Programm fragt im Sekundentakt (Guard.Watch,
 * Vorgabe alle 2 Sekunden). Wer dafuer je Frage einen Tastendruck verlangt,
 * bringt den Nutzer dazu, Tastendruecke wegzudruecken, ohne hinzusehen -
 * und damit ist jede spaetere echte Nachfrage wertlos. `attest` gibt kein
 * Geheimnis heraus: es unterschreibt einen Zufallswert unter einem Zweck,
 * der zu nichts anderem passt. */
void osk_guard_set_attest_confirm(bool on);

#endif
