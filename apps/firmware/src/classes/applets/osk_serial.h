/* Eine Seriennummer je Geraet, dieselbe fuer alle Anwendungen.
 *
 * Warum es sie braucht: GnuPG unterscheidet Karten an der Seriennummer in
 * der OpenPGP-Kennung. Solange die auf jedem Geraet dieselbe ist, sehen
 * zwei unserer Sticks an einem Rechner wie eine Karte aus (das stand als
 * Luecke in docs/smartcard.md). Die PIV-Attestierung braucht sie ebenfalls:
 * ein Zertifikat ohne Geraetebezug sagt nicht, WELCHES Geraet den
 * Schluessel haelt.
 *
 * WOHER SIE KOMMT: aus dem Sicherheitschip, abgeleitet aus dem
 * Grundgeheimnis ueber osk_se050_hkdf(). Nicht aus der FICR des
 * nRF52840 - dessen Auslesesperre ist mit Spannungs-Stoerimpulsen schon
 * ueberwunden worden (docs/plan.md, Abschnitt 4), und ein Wert, den man
 * von aussen abschreiben kann, ist als Kennung nicht besser, kostet aber
 * eine zweite Quelle. Nicht aus einem Fertigungsschritt, weil es den
 * nicht gibt.
 *
 * WAS SIE NICHT IST: ein Beweis. Die Seriennummer ist oeffentlich - sie
 * steht in der Kartenkennung und in jedem Attestierungszertifikat. Jeder
 * kann eine beliebige Zahl behaupten. Faelschungssicher ist nicht die
 * Nummer, sondern die Unterschrift des F9-Schluessels darueber
 * (osk_attest.h). Wer die Nummer als Ausweis benutzt, hat sie
 * missverstanden.
 *
 * Eigenschaften, die daraus folgen:
 *   - je Geraet eindeutig, weil das Grundgeheimnis je Geraet eindeutig ist
 *   - stabil ueber Neustarts, weil die Ableitung dieselbe bleibt
 *   - sie wechselt beim Zuruecksetzen auf Werkszustand, weil dann ein
 *     neues Grundgeheimnis entsteht. Das ist gewollt: danach ist es fuer
 *     jeden Dienst ein anderes Geraet.
 */
#ifndef OSK_SERIAL_H
#define OSK_SERIAL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"

/* Lange Geraetekennung - fuer das Attestierungszertifikat. */
#define OSK_SERIAL_LEN 16u

/* Kurze Form, vier Bytes: so lang ist das Seriennummernfeld der
 * OpenPGP-Kennung (Spezifikation 3.4 Abschnitt 4.2.1). */
#define OSK_SERIAL_SHORT_LEN 4u

/* Der Ableitungszweck. Eigener Text, damit diese Ableitung nie denselben
 * Wert liefern kann wie eine andere - dieselbe Regel wie bei den
 * OSK_INFO_*-Texten in src/classes/ctap/osk_ctap_key.h. */
#define OSK_INFO_SERIAL "osk/device/serial/v1"

/* Liefert die Geraetekennung. Wird einmal je Einschaltvorgang beim Chip
 * geholt und dann gemerkt.
 *
 * Antwortet der Chip nicht, kommt OSK_ERR_IO und NICHTS wird
 * geschrieben - es gibt keine Ersatznummer. Eine erfundene Null waere
 * schlimmer als keine: zwei Geraete ohne Chip saehen wieder gleich aus. */
osk_result_t osk_serial_device(uint8_t out[OSK_SERIAL_LEN]);

/* Die kurze Form fuer die Kartenkennung. Die ersten vier Bytes der
 * langen; aus vier Nullen wird 00 00 00 01, weil alles-Null in den
 * Anwendungen "keine Nummer" bedeutet. */
osk_result_t osk_serial_short(uint8_t out[OSK_SERIAL_SHORT_LEN]);

/* true, sobald die Nummer einmal gelesen werden konnte. */
bool osk_serial_known(void);

/* Karte aus und wieder an: alles Gemerkte faellt. */
void osk_serial_forget(void);

#endif
