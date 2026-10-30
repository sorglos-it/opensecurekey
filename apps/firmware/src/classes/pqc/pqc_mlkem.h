/* ML-KEM-768 nach FIPS 203 - der blanke Schluesselkapselmechanismus.
 *
 * ======================================================================
 *  DIESER KOPF IST MODULINTERN. Er liegt absichtlich NICHT in
 *  apps/firmware/include/.
 *
 *  Grund: ein Geheimnis, das allein aus ML-KEM kommt, darf im Geraet
 *  nirgends entstehen. Der Schluessel laege dann im Rechenkern statt im
 *  Sicherheitschip, und die Auslesesperre des Rechenkerns ist mit
 *  Spannungs-Stoerimpulsen schon ueberwunden worden. Nach aussen gibt
 *  es nur den gemischten Weg in apps/firmware/include/osk_pqc.h, und
 *  der verlangt beide Anteile.
 *
 *  Wer diesen Kopf einbindet, muss OSK_PQC_INTERNAL setzen. Das ist
 *  keine Sperre gegen Boeswillige, sondern ein Stolperdraht gegen die
 *  bequeme Abkuerzung: "nur fuer den Test" ist genau der Weg, auf dem
 *  solche Pfade entstehen und dann bleiben.
 *
 *  Einbinden darf das zweierlei: pqc_hybrid.c, der gemischte Weg, und
 *  der Pruefsatz unter apps/firmware/test/pqc/, der gegen die amtlichen
 *  Testwerte des NIST prueft. Ohne diesen Zugriff waere die Umsetzung
 *  nur gegen sich selbst geprueft, und das beweist nichts: ein
 *  Rundlauf geht auch auf, wenn beide Seiten denselben Fehler machen.
 * ======================================================================
 *
 * Keine fremde Umsetzung als Vorlage - gebaut nach der Beschreibung.
 * Kein dynamischer Speicher; alle Puffer kommen vom Aufrufer.
 *
 * Die Groessen (K, ETA, DU, DV und alle Laengen) stehen in osk_pqc.h -
 * eine Zahl nur an einer Stelle.
 */
#ifndef PQC_MLKEM_H
#define PQC_MLKEM_H

#ifndef OSK_PQC_INTERNAL
#error "pqc_mlkem.h ist modulintern - siehe osk_pqc.h fuer den gemischten Weg"
#endif

#include <stddef.h>
#include <stdint.h>
#include "osk_result.h"
#include "osk_pqc.h"

/* Schluesselpaar aus zwei Keimen. d und z muessen echter Zufall sein;
 * sie werden hier nur verrechnet, nicht erzeugt - so ist die Funktion
 * gegen die amtlichen Testwerte pruefbar (FIPS 203, Alg. 16). */
void osk_mlkem_keygen(uint8_t ek[OSK_MLKEM_EK_BYTES], uint8_t dk[OSK_MLKEM_DK_BYTES],
		      const uint8_t d[32], const uint8_t z[32]);

/* Kapseln (FIPS 203, Alg. 17). m ist der 32-Byte-Zufall des Absenders.
 * Gibt OSK_ERR_FORMAT, wenn ek nicht reduziert ist - die Pruefung aus
 * FIPS 203, 7.2. */
osk_result_t osk_mlkem_encaps(uint8_t ct[OSK_MLKEM_CT_BYTES], uint8_t ss[OSK_MLKEM_SS_BYTES],
			      const uint8_t ek[OSK_MLKEM_EK_BYTES], const uint8_t m[32]);

/* Entkapseln (FIPS 203, Alg. 18). Passt der Schluesseltext nicht, kommt
 * KEIN Fehler, sondern ein anderes, aus z abgeleitetes Geheimnis
 * (stille Verwerfung). Wer hier einen Fehler meldete, baute ein Orakel. */
osk_result_t osk_mlkem_decaps(uint8_t ss[OSK_MLKEM_SS_BYTES], const uint8_t dk[OSK_MLKEM_DK_BYTES],
			      const uint8_t ct[OSK_MLKEM_CT_BYTES]);

#endif
