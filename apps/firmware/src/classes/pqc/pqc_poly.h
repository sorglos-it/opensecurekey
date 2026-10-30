/* Rechnen im Ring Z_3329[X]/(X^256+1) - das Rechenwerk unter ML-KEM.
 *
 * Gebaut nach FIPS 203, Abschnitt 4 (NTT, Auswahlverfahren, Packen,
 * Stauchen). Keine fremde Umsetzung als Vorlage.
 *
 * Alle Koeffizienten liegen immer im Bereich [0, Q). Es gibt keine
 * "noch zu reduzierende" Zwischenform - genau dort entstehen in solchen
 * Umsetzungen die Fehler, die erst bei seltenen Eingaben auffallen.
 *
 * Zeitverhalten: alle Rechenschritte haengen nur an Laengen, nicht an
 * Werten. Die einzige Ausnahme ist osk_poly_sample_ntt(), das vom Zufall
 * abhaengt, wie oft es zieht - es verarbeitet aber nur die oeffentliche
 * Matrix A und kein Geheimnis.
 *
 * Kein dynamischer Speicher.
 */
#ifndef PQC_POLY_H
#define PQC_POLY_H

#include <stddef.h>
#include <stdint.h>

#define OSK_MLKEM_N 256
#define OSK_MLKEM_Q 3329

/* Ein Ringelement. Je nach Zusammenhang in gewoehnlicher Form oder in der
 * NTT-Form - der Typ unterscheidet das nicht, die Benennung der Variablen
 * tut es (Endung _ntt). */
struct osk_poly {
	uint16_t c[OSK_MLKEM_N];
};

void osk_poly_zero(struct osk_poly *p);
void osk_poly_add(struct osk_poly *r, const struct osk_poly *a, const struct osk_poly *b);
void osk_poly_sub(struct osk_poly *r, const struct osk_poly *a, const struct osk_poly *b);

/* Hin und zurueck in die NTT-Form, an der Stelle. */
void osk_poly_ntt(struct osk_poly *p);
void osk_poly_intt(struct osk_poly *p);

/* r += a * b, beide in NTT-Form. Aufsummieren statt Zuweisen, weil jede
 * Zeile der Matrix so gerechnet wird. */
void osk_poly_basemul_acc(struct osk_poly *r, const struct osk_poly *a, const struct osk_poly *b);

/* Ein Element der oeffentlichen Matrix A aus dem Keim ziehen
 * (SampleNTT). Das Ergebnis ist schon in NTT-Form. */
void osk_poly_sample_ntt(struct osk_poly *p, const uint8_t seed[32], uint8_t x1, uint8_t x2);

/* Kleines Rauschen aus 64*eta Byte (SamplePolyCBD). eta ist 2 oder 3. */
void osk_poly_cbd(struct osk_poly *p, const uint8_t *buf, unsigned int eta);

/* d Bit je Koeffizient packen und auspacken. d ist 1, 4, 10 oder 12.
 * out braucht 32*d Byte. */
void osk_poly_encode(uint8_t *out, const struct osk_poly *p, unsigned int d);
void osk_poly_decode(struct osk_poly *p, const uint8_t *in, unsigned int d);

/* Stauchen auf d Bit und zurueck, an der Stelle. */
void osk_poly_compress(struct osk_poly *p, unsigned int d);
void osk_poly_decompress(struct osk_poly *p, unsigned int d);

/* Wahr, wenn alle 256 Werte kleiner als Q sind. Gebraucht fuer die
 * Pruefung des fremden oeffentlichen Schluessels (FIPS 203, 7.2: der
 * Schluessel muss sich unveraendert zurueckpacken lassen). */
int osk_poly_is_reduced(const struct osk_poly *p);

#endif
