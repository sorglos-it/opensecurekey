/* Gemischter Schluesselaustausch: ML-KEM-768 zusammen mit X25519.
 *
 * ======================================================================
 *  Regel des Projekts: HYBRID ODER GAR NICHT.
 *
 *  X25519 kommt aus dem Sicherheitschip - der geheime Punkt verlaesst
 *  ihn nie. ML-KEM rechnet der Rechenkern, weil der Sicherheitschip es
 *  nicht kann. Ein reiner ML-KEM-Weg waere deshalb ein RUECKSCHRITT:
 *  der Schluessel laege dann im Rechenkern, und dessen Auslesesperre
 *  ist mit Spannungs-Stoerimpulsen schon ueberwunden worden.
 *
 *  Darum gibt es hier nur eine Tuer, und sie verlangt beide Anteile.
 *  Es gibt keinen Schalter, keine Betriebsart und keinen Pruefpfad, auf
 *  dem ML-KEM allein ein Geheimnis traegt. Der blanke Mechanismus liegt
 *  in src/classes/pqc/pqc_mlkem.h und ist modulintern.
 *
 *  Faellt der ML-KEM-Teil (schlechte Mathematik, Fehler in dieser
 *  Umsetzung), bleibt X25519 aus dem Chip und nichts ist schlechter als
 *  heute. Faellt X25519 (Quantenrechner), bleibt ML-KEM. Begruendung
 *  der Mischung: docs/pqc.md.
 * ======================================================================
 *
 * Nichts davon ist am Geraet gelaufen. Stand: uebersetzt und am PC
 * gegen die amtlichen Testwerte des NIST geprueft - siehe docs/pqc.md.
 *
 * Umsetzung: src/classes/pqc/
 */
#ifndef OSK_PQC_H
#define OSK_PQC_H

#include <stdint.h>
#include "osk_result.h"

/* --- Die gewaehlte Groesse: ML-KEM-768 --------------------------------
 *
 * Alle Laengen unten leiten sich aus diesen fuenf Zahlen ab; keine ist
 * zweimal geschrieben. Warum 768 und nicht 512 oder 1024: docs/pqc.md.
 */
#define OSK_MLKEM_K 3
#define OSK_MLKEM_ETA1 2
#define OSK_MLKEM_ETA2 2
#define OSK_MLKEM_DU 10
#define OSK_MLKEM_DV 4

#define OSK_MLKEM_POLY_BYTES 384
#define OSK_MLKEM_EK_BYTES (OSK_MLKEM_K * OSK_MLKEM_POLY_BYTES + 32)
#define OSK_MLKEM_DK_BYTES (OSK_MLKEM_K * OSK_MLKEM_POLY_BYTES + OSK_MLKEM_EK_BYTES + 64)
#define OSK_MLKEM_CT_BYTES (32 * (OSK_MLKEM_DU * OSK_MLKEM_K + OSK_MLKEM_DV))
#define OSK_MLKEM_SS_BYTES 32

/* --- Der gemischte Weg -----------------------------------------------
 *
 * Aufbau der Puffer. Der X25519-Anteil steht jeweils vorn, weil er der
 * Anteil ist, der immer da sein MUSS - so faellt eine Laenge, in der er
 * fehlt, schon beim Rechnen der Groesse auf und nicht erst im Betrieb.
 *
 *   ek: x_pub(32) || mlkem_ek(1184)        = 1216
 *   dk: x_pub(32) || mlkem_dk(2400)        = 2432   (der geheime
 *       X25519-Punkt steht NICHT darin: er bleibt im Chip)
 *   ct: x_eph_pub(32) || mlkem_ct(1088)    = 1120
 */
#define OSK_PQC_X_BYTES 32
#define OSK_PQC_EK_BYTES (OSK_PQC_X_BYTES + OSK_MLKEM_EK_BYTES)
#define OSK_PQC_DK_BYTES (OSK_PQC_X_BYTES + OSK_MLKEM_DK_BYTES)
#define OSK_PQC_CT_BYTES (OSK_PQC_X_BYTES + OSK_MLKEM_CT_BYTES)
#define OSK_PQC_SS_BYTES 32

/* Bereichstrennung. Wer dieselben zwei Anteile fuer etwas anderes
 * mischt, muss eine andere Kennung nehmen - sonst passt ein Geheimnis
 * aus einem Zusammenhang in den anderen. */
#define OSK_PQC_LABEL "openSecureKey hybrid-kem v1 ML-KEM-768+X25519"

/* Schluesselpaar bauen.
 *
 * x_pub ist der oeffentliche X25519-Punkt aus dem Sicherheitschip; der
 * geheime bleibt dort. d und z sind 32 Byte echter Zufall fuer den
 * ML-KEM-Anteil.
 *
 * OSK_ERR_ARG, wenn ein Zeiger fehlt oder x_pub lauter Nullen ist. */
osk_result_t osk_pqc_keypair(uint8_t ek[OSK_PQC_EK_BYTES], uint8_t dk[OSK_PQC_DK_BYTES],
			     const uint8_t x_pub[OSK_PQC_X_BYTES], const uint8_t d[32],
			     const uint8_t z[32]);

/* Kapseln, fuer den Absender.
 *
 *   ek    oeffentlicher Schluessel der Gegenseite (beide Anteile)
 *   x_ct  eigener fluechtiger X25519-Punkt, aus dem Chip
 *   x_ss  das rohe ECDH-Ergebnis aus dem Chip (32 Byte)
 *   m     32 Byte echter Zufall fuer den ML-KEM-Anteil
 *
 * OSK_ERR_ARG, wenn x_ss lauter Nullen ist - das heisst bei X25519,
 * dass die Gegenseite einen Punkt kleiner Ordnung geschickt hat, und
 * dann traegt nur noch ML-KEM. Das ist genau der Fall, der hier nicht
 * stillschweigend durchgehen darf.
 * OSK_ERR_FORMAT, wenn der ML-KEM-Anteil von ek unbrauchbar ist. */
osk_result_t osk_pqc_encaps(uint8_t ct[OSK_PQC_CT_BYTES], uint8_t ss[OSK_PQC_SS_BYTES],
			    const uint8_t ek[OSK_PQC_EK_BYTES], const uint8_t x_ct[OSK_PQC_X_BYTES],
			    const uint8_t x_ss[OSK_PQC_X_BYTES], const uint8_t m[32]);

/* Entkapseln, fuer den Empfaenger. x_ss ist wieder das rohe
 * ECDH-Ergebnis aus dem Chip, gerechnet gegen ct[0..31]. */
osk_result_t osk_pqc_decaps(uint8_t ss[OSK_PQC_SS_BYTES], const uint8_t dk[OSK_PQC_DK_BYTES],
			    const uint8_t ct[OSK_PQC_CT_BYTES],
			    const uint8_t x_ss[OSK_PQC_X_BYTES]);

#endif
