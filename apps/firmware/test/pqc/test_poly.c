/* Das Rechenwerk unter ML-KEM.
 *
 * Der wichtigste Fall hier ist der dritte: die NTT wird gegen eine
 * Multiplikation Zeile fuer Zeile geprueft, so wie man sie in der Schule
 * rechnet. Das ist ein voellig anderer Rechenweg zum selben Ergebnis -
 * ein Fehler in der Tabelle, in den Drehweiten oder im Faktor am Ende
 * faellt dort auf, waehrend ein Hin-und-zurueck-Lauf ihn verschluckt,
 * weil er sich selbst aufhebt.
 */
#include "tiny_test.h"
#include "pqc_poly.h"
#include "pqc_keccak.h"

/* Immer dieselben Werte, aber keine glatten: so ist ein Durchfall
 * nachstellbar und trifft trotzdem nicht nur Null und Eins. */
static void fill(struct osk_poly *p, uint8_t seed)
{
	uint8_t buf[512];

	osk_shake256(buf, sizeof(buf), &seed, 1u);
	for (size_t i = 0; i < OSK_MLKEM_N; i++) {
		uint16_t v = (uint16_t)(buf[2u * i] | ((uint16_t)buf[2u * i + 1u] << 8));

		p->c[i] = (uint16_t)(v % OSK_MLKEM_Q);
	}
}

/* Multiplikation im Ring Z_q[X]/(X^256+1), Zeile fuer Zeile. X^256 ist
 * dabei -1, darum geht der obere Teil mit umgekehrtem Vorzeichen ein. */
static void schoolbook(struct osk_poly *r, const struct osk_poly *a, const struct osk_poly *b)
{
	uint32_t acc[2u * OSK_MLKEM_N];

	for (size_t i = 0; i < 2u * OSK_MLKEM_N; i++) {
		acc[i] = 0u;
	}
	for (size_t i = 0; i < OSK_MLKEM_N; i++) {
		for (size_t j = 0; j < OSK_MLKEM_N; j++) {
			acc[i + j] = (acc[i + j] + (uint32_t)a->c[i] * b->c[j]) % OSK_MLKEM_Q;
		}
	}
	for (size_t i = 0; i < OSK_MLKEM_N; i++) {
		uint32_t high = acc[i + OSK_MLKEM_N] % OSK_MLKEM_Q;

		r->c[i] = (uint16_t)((acc[i] + OSK_MLKEM_Q - high) % OSK_MLKEM_Q);
	}
}

static void ntt_roundtrip(void)
{
	for (uint8_t seed = 0; seed < 4u; seed++) {
		struct osk_poly p, copy;

		fill(&p, seed);
		copy = p;
		osk_poly_ntt(&p);
		osk_poly_intt(&p);
		TT_EQ_MEM(&p, &copy, sizeof(p), "NTT hin und zurueck");
	}
}

static void ntt_matches_schoolbook(void)
{
	for (uint8_t seed = 0; seed < 3u; seed++) {
		struct osk_poly a, b, want, got, a_ntt, b_ntt;

		fill(&a, (uint8_t)(10u + seed));
		fill(&b, (uint8_t)(20u + seed));
		schoolbook(&want, &a, &b);

		a_ntt = a;
		b_ntt = b;
		osk_poly_ntt(&a_ntt);
		osk_poly_ntt(&b_ntt);
		osk_poly_zero(&got);
		osk_poly_basemul_acc(&got, &a_ntt, &b_ntt);
		osk_poly_intt(&got);
		TT_EQ_MEM(&got, &want, sizeof(got), "NTT-Produkt gleich Produkt Zeile fuer Zeile");
	}
}

/* Aufsummieren muss wirklich aufsummieren: in der Matrixzeile haengt
 * alles daran. Zweimal dasselbe Produkt ist das Doppelte. */
static void basemul_accumulates(void)
{
	struct osk_poly a, b, once, twice, sum;

	fill(&a, 31u);
	fill(&b, 32u);
	osk_poly_ntt(&a);
	osk_poly_ntt(&b);
	osk_poly_zero(&once);
	osk_poly_basemul_acc(&once, &a, &b);
	twice = once;
	osk_poly_basemul_acc(&twice, &a, &b);
	osk_poly_add(&sum, &once, &once);
	TT_EQ_MEM(&twice, &sum, sizeof(sum), "zweimal aufsummiert ist das Doppelte");
}

static void add_sub_are_inverse(void)
{
	struct osk_poly a, b, t;

	fill(&a, 41u);
	fill(&b, 42u);
	osk_poly_add(&t, &a, &b);
	osk_poly_sub(&t, &t, &b);
	TT_EQ_MEM(&t, &a, sizeof(a), "plus und minus heben sich auf");
	TT_TRUE(osk_poly_is_reduced(&t), "Ergebnis bleibt reduziert");
}

static void encode_roundtrip(void)
{
	static const unsigned int widths[] = {1u, 4u, 10u, 12u};

	for (size_t w = 0; w < sizeof(widths) / sizeof(widths[0]); w++) {
		unsigned int d = widths[w];
		struct osk_poly p, back;
		uint8_t buf[32u * 12u];

		fill(&p, (uint8_t)(50u + d));
		/* Nur Werte, die in d Bit passen - sonst prueft man das
		 * Abschneiden und nicht das Packen. */
		for (size_t i = 0; i < OSK_MLKEM_N; i++) {
			p.c[i] &= (uint16_t)((1u << d) - 1u);
		}
		osk_poly_encode(buf, &p, d);
		osk_poly_decode(&back, buf, d);
		TT_EQ_MEM(&back, &p, sizeof(p), "packen und auspacken");
	}
}

/* Stauchen verliert etwas - aber nicht viel. Fuer du = 10 darf der
 * Fehler hoechstens q/2^11 betragen, also 1 oder 2. Mehr heisst, dass
 * die Rundung in die falsche Richtung geht. */
static void compress_is_close(void)
{
	struct osk_poly p, back;
	unsigned int worst = 0u;

	fill(&p, 60u);
	back = p;
	osk_poly_compress(&back, 10u);
	osk_poly_decompress(&back, 10u);
	for (size_t i = 0; i < OSK_MLKEM_N; i++) {
		int diff = (int)back.c[i] - (int)p.c[i];
		unsigned int dist;

		if (diff < 0) {
			diff = -diff;
		}
		dist = (unsigned int)diff;
		/* Der Ring ist rund: 0 und q-1 liegen nebeneinander. */
		if (dist > OSK_MLKEM_Q / 2) {
			dist = (unsigned int)OSK_MLKEM_Q - dist;
		}
		if (dist > worst) {
			worst = dist;
		}
	}
	TT_TRUE(worst <= 2u, "Stauchen auf 10 Bit verliert hoechstens 2");
}

/* Ein Bit und zurueck ist der Weg, auf dem die Nachricht reist: 0 bleibt
 * 0, 1 wird die Ringmitte und muss als 1 zurueckkommen. */
static void one_bit_message(void)
{
	struct osk_poly p;

	osk_poly_zero(&p);
	for (size_t i = 0; i < OSK_MLKEM_N; i++) {
		p.c[i] = (uint16_t)(i & 1u);
	}
	osk_poly_decompress(&p, 1u);
	TT_EQ_INT(p.c[0], 0, "Null bleibt Null");
	TT_EQ_INT(p.c[1], (OSK_MLKEM_Q + 1) / 2, "Eins wird die Ringmitte");
	osk_poly_compress(&p, 1u);
	for (size_t i = 0; i < OSK_MLKEM_N; i++) {
		TT_EQ_INT(p.c[i], (int)(i & 1u), "Bit kommt zurueck");
	}
}

static void reduced_check_works(void)
{
	struct osk_poly p;

	fill(&p, 70u);
	TT_TRUE(osk_poly_is_reduced(&p), "frische Werte sind reduziert");
	p.c[255] = (uint16_t)OSK_MLKEM_Q;
	TT_TRUE(!osk_poly_is_reduced(&p), "q selbst ist nicht mehr reduziert");
	p.c[255] = 4095u;
	TT_TRUE(!osk_poly_is_reduced(&p), "4095 ist nicht reduziert");
}

/* Die Matrix A ist oeffentlich und haengt nur am Keim und an den zwei
 * Indexbytes. Zwei Dinge muessen stimmen: gleicher Keim gibt gleiches
 * Ergebnis, und die Reihenfolge der Indexbytes macht einen Unterschied -
 * sonst waere A symmetrisch, und das Transponieren im Kapseln waere
 * wirkungslos. Genau dieser Dreher ist der haeufigste Fehler hier. */
static void sample_ntt_is_asymmetric(void)
{
	uint8_t seed[32];
	struct osk_poly a, b, again;

	for (size_t i = 0; i < sizeof(seed); i++) {
		seed[i] = (uint8_t)(i * 3u + 1u);
	}
	osk_poly_sample_ntt(&a, seed, 0u, 1u);
	osk_poly_sample_ntt(&again, seed, 0u, 1u);
	TT_EQ_MEM(&again, &a, sizeof(a), "gleicher Keim, gleiches Ergebnis");

	osk_poly_sample_ntt(&b, seed, 1u, 0u);
	TT_TRUE(memcmp(&a, &b, sizeof(a)) != 0, "(0,1) ist nicht (1,0)");
	TT_TRUE(osk_poly_is_reduced(&a), "gezogene Werte sind reduziert");
	TT_TRUE(osk_poly_is_reduced(&b), "gezogene Werte sind reduziert");
}

/* Das kleine Rauschen: bei eta = 2 kommen nur die Werte -2..2 vor, und
 * im Ring heisst -1 eben q-1. Alles andere waere ein Fehler beim
 * Bitzaehlen. */
static void cbd_stays_small(void)
{
	uint8_t buf[64u * 2u];
	struct osk_poly p;
	int bad = 0;

	for (size_t i = 0; i < sizeof(buf); i++) {
		buf[i] = (uint8_t)(i * 37u + 11u);
	}
	osk_poly_cbd(&p, buf, 2u);
	for (size_t i = 0; i < OSK_MLKEM_N; i++) {
		uint16_t v = p.c[i];

		if (!(v <= 2u || v >= (uint16_t)(OSK_MLKEM_Q - 2))) {
			bad++;
		}
	}
	TT_EQ_INT(bad, 0, "eta = 2 liefert nur -2..2");
}

void test_poly(void)
{
	TT_CASE("NTT hin und zurueck");
	ntt_roundtrip();

	TT_CASE("NTT gegen Multiplikation Zeile fuer Zeile");
	ntt_matches_schoolbook();

	TT_CASE("Produkte werden aufsummiert");
	basemul_accumulates();

	TT_CASE("plus und minus");
	add_sub_are_inverse();

	TT_CASE("packen und auspacken, 1 / 4 / 10 / 12 Bit");
	encode_roundtrip();

	TT_CASE("Stauchen verliert wenig");
	compress_is_close();

	TT_CASE("ein Bit und zurueck");
	one_bit_message();

	TT_CASE("Pruefung auf reduzierte Werte");
	reduced_check_works();

	TT_CASE("die oeffentliche Matrix und ihre Indexbytes");
	sample_ntt_is_asymmetric();

	TT_CASE("kleines Rauschen bleibt klein");
	cbd_stays_small();
}
