/* PIV-Attestierung je Steckplatz.
 *
 * WAS SIE BEWEIST: dass der Schluessel eines Steckplatzes im
 * Sicherheitschip entstanden ist und ihn nie verlassen hat. Ohne das ist
 * "der Schluessel liegt in Hardware" eine Behauptung - der oeffentliche
 * Schluessel in einem Zertifikat sieht genauso aus, wenn er auf einem
 * Rechner erzeugt und hineingeschrieben wurde.
 *
 * WIE: ein zweiter Schluessel im Chip, Steckplatz F9, unterschreibt ein
 * X.509-Zertifikat ueber den oeffentlichen Teil des Steckplatzschluessels.
 * Wer dem F9-Schluessel glaubt, glaubt der Aussage. Der Verweis geht damit
 * nur einen Schritt weiter - und genau das ist die Grenze, die ins
 * Handbuch gehoert: *wem* man den F9-Schluessel abnimmt, entscheidet, ob
 * die Attestierung etwas wert ist. Solange niemand von aussen das
 * F9-Zertifikat bescheinigt hat, beweist die Kette nur, dass ein und
 * dasselbe Geraet beide Schluessel haelt.
 *
 * DIE ZAHLEN F9 und das Datenobjekt 5FFF01 kommen aus dem Auftrag und
 * liegen in den Bereichen, die 7816-4 fuer eigene Belegung freilaesst. Sie
 * sind dieselben, die am Markt ueblich sind - eine Nummer in einer
 * Schnittstelle ist kein Code, und es ist nichts uebernommen worden.
 * WOHLGEMERKT: der INHALT des Zertifikats ist unser eigener. Fremde
 * Werkzeuge werden die Angaben darin nicht an der Stelle finden, an der
 * sie sie suchen. Wer Vertraeglichkeit mit einem fremden Werkzeug will,
 * braucht dessen Beschreibung; die liegt hier nicht vor, und geraten wird
 * nichts.
 *
 * WAS IM ZERTIFIKAT STEHT, und warum dort:
 *   - Inhaber (Subject): der Steckplatz als CN, die Geraetenummer im
 *     Standardfeld serialNumber (2.5.4.5), die Freigaberegel des
 *     Steckplatzes als OU. Alles drei in Feldern, die es im Standard
 *     schon gibt. KEINE eigenen Erweiterungen mit erfundenen
 *     Objektkennungen - eine Kennung, die niemand vergeben hat, ist
 *     irgendwann die eines anderen.
 *   - Aussteller (Issuer): der Inhabername aus dem F9-Zertifikat, wenn
 *     eines abgelegt ist. Sonst unser eigener Ersatzname. Ohne diesen
 *     Abgleich passt die Kette nicht zusammen, und die Pruefung schlaegt
 *     aus einem Grund fehl, der nichts mit dem Schluessel zu tun hat.
 *   - Gueltigkeit: fest, 2000-01-01 bis 9999-12-31. Das Geraet hat keine
 *     Uhr, der es glauben darf - die Zeit kommt beim Einstecken vom
 *     Rechner (docs/plan.md, "Kein Akku"). Ein Zertifikat mit einem Datum
 *     aus einer gestellten Uhr waere mal gueltig und mal nicht.
 *     9999-12-31T23:59:59Z ist die Form, die RFC 5280 Abschnitt 4.1.2.5
 *     fuer "kein festgelegtes Ablaufdatum" vorsieht.
 *
 * WAS DIE ATTESTIERUNG NICHT VERLANGT: keine PIN und keinen Tastendruck.
 * Begruendung, und sie ist wichtig: dabei wird nichts Geheimes
 * herausgegeben - ein oeffentlicher Schluessel und ein Name. Und der
 * F9-Schluessel unterschreibt ausschliesslich Daten, die dieses Modul
 * selbst zusammensetzt, nie etwas, das von aussen kommt. Damit ist er
 * kein Unterschriftenautomat fuer fremde Werte. Waere es anders, waere
 * die Freigabe Pflicht.
 */
#ifndef OSK_ATTEST_H
#define OSK_ATTEST_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"

/* Der Steckplatz des Attestierungsschluessels. */
#define OSK_PIV_SLOT_ATTEST 0xf9u

/* Das Kommando. CLA 00, Lc 0, P1 = Steckplatz, P2 = 00; heraus kommt das
 * Zertifikat als DER, ohne weitere Verpackung. */
#define OSK_PIV_INS_ATTEST 0xf9u

/* Wo das F9-Zertifikat liegt: Datenobjekt und Ablage im Tresor. Die drei
 * Angaben stehen hier und nicht in osk_piv.c, weil beide Dateien sie
 * brauchen - osk_piv.c fuer GET DATA und PUT DATA, osk_attest.c fuer den
 * Ausstellernamen. Zweimal hingeschrieben waere eine davon irgendwann
 * anders. 5FFF01 liegt im Bereich, den ISO/IEC 7816-4 fuer eigene
 * Belegung freilaesst. */
#define OSK_ATTEST_CERT_DO 0x5fff01u
#define OSK_ATTEST_CERT_TAG "PIV-CERT-F9"
#define OSK_ATTEST_CERT_NAME "PIV Zertifikat Attestierung"

/* Laengstes Zertifikat, das entstehen kann: ein RSA-4096-Schluessel
 * bringt allein 512 Byte Modulus mit, dazu Namen, Gueltigkeit,
 * Erweiterungen und die Unterschrift. Bleibt unter OSK_APDU_MAX_RESP,
 * also passt es in eine verkettete Antwort. */
#define OSK_ATTEST_MAX 1792u

/* Baut das Zertifikat fuer einen Steckplatz.
 *
 * `slot` ist 9A, 9C, 9D oder 9E - F9 attestiert sich nicht selbst.
 * `alg` ist die PIV-Algorithmuskennung des Schluessels in diesem
 * Steckplatz; daran haengt, ob der oeffentliche Teil als Kurvenpunkt oder
 * als Modulus und Exponent eingetragen wird.
 *
 * Fehler:
 *   OSK_ERR_ARG         Steckplatz oder Kennung gehen nicht
 *   OSK_ERR_NOT_FOUND   im Steckplatz oder in F9 liegt kein Schluessel
 *   OSK_ERR_UNSUPPORTED F9 ist keine P-256 - siehe osk_attest.c
 *   OSK_ERR_FULL        `cap` zu klein
 */
osk_result_t osk_attest_cert(uint8_t slot, uint8_t alg, uint8_t *out, size_t cap,
			     size_t *len_out);

/* Karte aus und wieder an. */
void osk_attest_reset(void);

#endif
