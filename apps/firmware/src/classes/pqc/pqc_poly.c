/* Rechenwerk unter ML-KEM. Siehe pqc_poly.h. */
#include "pqc_poly.h"
#include "pqc_keccak.h"

/* 17^BitRev7(i) mod 3329 fuer i = 0..127. Nachzurechnen in drei Zeilen:
 *
 *   q = 3329
 *   br = lambda i: int(format(i, '07b')[::-1], 2)
 *   [pow(17, br(i), q) for i in range(128)]
 *
 * 17 ist die 256. Einheitswurzel modulo 3329 (FIPS 203, 4.3). Der Eintrag
 * 0 wird nicht benutzt; er steht da, damit die Stellen den Nummern in der
 * Beschreibung entsprechen. */
static const uint16_t k_zeta[128] = {
    1,	  1729, 2580, 3289, 2642, 630,	1897, 848,  1062, 1919, 193,  797,  2786, 3260, 569,  1746,
    296,  2447, 1339, 1476, 3046, 56,	2240, 1333, 1426, 2094, 535,  2882, 2393, 2879, 1974, 821,
    289,  331,	3253, 1756, 1197, 2304, 2277, 2055, 650,  1977, 2513, 632,  2865, 33,	1320, 1915,
    2319, 1435, 807,  452,  1438, 2868, 1534, 2402, 2647, 2617, 1481, 648,  2474, 3110, 1227, 910,
    17,	  2761, 583,  2649, 1637, 723,	2288, 1100, 1409, 2662, 3281, 233,  756,  2156, 3015, 3050,
    1703, 1651, 2789, 1789, 1847, 952,	1461, 2687, 939,  2308, 2437, 2388, 733,  2337, 268,  641,
    1584, 2298, 2037, 3220, 375,  2549, 2090, 1645, 1063, 319,	2773, 757,  2099, 561,	2466, 2594,
    2804, 1092, 403,  1026, 1143, 2150, 2775, 886,  1722, 1212, 1874, 1029, 2110, 2935, 885,  2154,
};

/* 128^-1 mod 3329 - der Faktor am Ende der Rueckverwandlung. */
#define OSK_MLKEM_N_INV 3303

/* Modulo Q. Q ist eine Konstante, darum uebersetzt das in eine
 * Multiplikation und Schiebeschritte, nicht in eine Division: die Laufzeit
 * haengt nicht vom Wert ab. */
static uint16_t mq(uint32_t x)
{
	return (uint16_t)(x % (uint32_t)OSK_MLKEM_Q);
}

static uint16_t mul(uint16_t a, uint16_t b)
{
	return mq((uint32_t)a * (uint32_t)b);
}

static uint16_t add(uint16_t a, uint16_t b)
{
	uint16_t s = (uint16_t)(a + b);

	return (uint16_t)(s >= (uint16_t)OSK_MLKEM_Q ? s - (uint16_t)OSK_MLKEM_Q : s);
}

static uint16_t sub(uint16_t a, uint16_t b)
{
	return add(a, (uint16_t)(OSK_MLKEM_Q - b) % (uint16_t)OSK_MLKEM_Q);
}

void osk_poly_zero(struct osk_poly *p)
{
	for (size_t i = 0; i < OSK_MLKEM_N; i++) {
		p->c[i] = 0u;
	}
}

void osk_poly_add(struct osk_poly *r, const struct osk_poly *a, const struct osk_poly *b)
{
	for (size_t i = 0; i < OSK_MLKEM_N; i++) {
		r->c[i] = add(a->c[i], b->c[i]);
	}
}

void osk_poly_sub(struct osk_poly *r, const struct osk_poly *a, const struct osk_poly *b)
{
	for (size_t i = 0; i < OSK_MLKEM_N; i++) {
		r->c[i] = sub(a->c[i], b->c[i]);
	}
}

/* FIPS 203, Algorithmus 9. */
void osk_poly_ntt(struct osk_poly *p)
{
	unsigned int i = 1u;

	for (size_t len = 128u; len >= 2u; len /= 2u) {
		for (size_t start = 0; start < OSK_MLKEM_N; start += 2u * len) {
			uint16_t zeta = k_zeta[i];

			i++;
			for (size_t j = start; j < start + len; j++) {
				uint16_t t = mul(zeta, p->c[j + len]);

				p->c[j + len] = sub(p->c[j], t);
				p->c[j] = add(p->c[j], t);
			}
		}
	}
}

/* FIPS 203, Algorithmus 10. */
void osk_poly_intt(struct osk_poly *p)
{
	unsigned int i = 127u;

	for (size_t len = 2u; len <= 128u; len *= 2u) {
		for (size_t start = 0; start < OSK_MLKEM_N; start += 2u * len) {
			uint16_t zeta = k_zeta[i];

			i--;
			for (size_t j = start; j < start + len; j++) {
				uint16_t t = p->c[j];

				p->c[j] = add(t, p->c[j + len]);
				p->c[j + len] = mul(zeta, sub(p->c[j + len], t));
			}
		}
	}
	for (size_t j = 0; j < OSK_MLKEM_N; j++) {
		p->c[j] = mul(p->c[j], (uint16_t)OSK_MLKEM_N_INV);
	}
}

/* FIPS 203, Algorithmen 11 und 12. Der Faktor ist 17^(2*BitRev7(i)+1),
 * und das ist k_zeta[i]^2 * 17 - darum keine zweite Tabelle. */
void osk_poly_basemul_acc(struct osk_poly *r, const struct osk_poly *a, const struct osk_poly *b)
{
	for (size_t i = 0; i < 128u; i++) {
		uint16_t gamma = mul(mul(k_zeta[i], k_zeta[i]), 17u);
		uint16_t a0 = a->c[2u * i], a1 = a->c[2u * i + 1u];
		uint16_t b0 = b->c[2u * i], b1 = b->c[2u * i + 1u];

		r->c[2u * i] = add(r->c[2u * i], add(mul(a0, b0), mul(mul(a1, b1), gamma)));
		r->c[2u * i + 1u] = add(r->c[2u * i + 1u], add(mul(a0, b1), mul(a1, b0)));
	}
}

/* FIPS 203, Algorithmus 7. Aus dem SHAKE128-Strom werden je drei Byte zu
 * zwei Kandidaten; angenommen wird, was kleiner als Q ist. Der Strom wird
 * so lange gezogen, bis 256 Werte zusammen sind. */
void osk_poly_sample_ntt(struct osk_poly *p, const uint8_t seed[32], uint8_t x1, uint8_t x2)
{
	struct osk_keccak st;
	uint8_t tail[2] = {x1, x2};
	size_t taken = 0;

	osk_keccak_init(&st, OSK_SHAKE128_RATE, OSK_KECCAK_PAD_SHAKE);
	osk_keccak_absorb(&st, seed, 32u);
	osk_keccak_absorb(&st, tail, sizeof(tail));

	while (taken < OSK_MLKEM_N) {
		uint8_t b[3];
		uint16_t d1, d2;

		osk_keccak_squeeze(&st, b, sizeof(b));
		d1 = (uint16_t)(b[0] + 256u * (b[1] & 0x0fu));
		d2 = (uint16_t)((b[1] >> 4) + 16u * b[2]);
		if (d1 < (uint16_t)OSK_MLKEM_Q) {
			p->c[taken] = d1;
			taken++;
		}
		if (d2 < (uint16_t)OSK_MLKEM_Q && taken < OSK_MLKEM_N) {
			p->c[taken] = d2;
			taken++;
		}
	}
	osk_keccak_wipe(&st);
}

/* FIPS 203, Algorithmus 8. */
void osk_poly_cbd(struct osk_poly *p, const uint8_t *buf, unsigned int eta)
{
	for (size_t i = 0; i < OSK_MLKEM_N; i++) {
		unsigned int x = 0u, y = 0u;
		size_t base = 2u * i * eta;

		for (unsigned int j = 0; j < eta; j++) {
			size_t bit = base + j;

			x += ((unsigned int)buf[bit / 8u] >> (bit % 8u)) & 1u;
		}
		for (unsigned int j = 0; j < eta; j++) {
			size_t bit = base + eta + j;

			y += ((unsigned int)buf[bit / 8u] >> (bit % 8u)) & 1u;
		}
		p->c[i] = sub((uint16_t)x, (uint16_t)y);
	}
}

/* Bitweise packen, kleinstes Bit zuerst - so liest es FIPS 203, 4.2.1.
 * Bitweise statt wortweise, weil vier Breiten (1, 4, 10, 12) mit einem
 * Stueck Code abgedeckt sind. Das kostet Zeit, aber keine vier
 * Sonderfaelle, in denen je einer falsch sein kann. */
void osk_poly_encode(uint8_t *out, const struct osk_poly *p, unsigned int d)
{
	size_t bits = 0;

	for (size_t i = 0; i < (size_t)d * OSK_MLKEM_N / 8u; i++) {
		out[i] = 0u;
	}
	for (size_t i = 0; i < OSK_MLKEM_N; i++) {
		for (unsigned int b = 0; b < d; b++) {
			if ((p->c[i] >> b) & 1u) {
				out[bits / 8u] |= (uint8_t)(1u << (bits % 8u));
			}
			bits++;
		}
	}
}

/* Gegenstueck. Achtung: das Ergebnis ist nicht reduziert - bei d = 12
 * koennen Werte bis 4095 herauskommen. Wer fremde Daten auspackt, muss
 * danach osk_poly_is_reduced() fragen. */
void osk_poly_decode(struct osk_poly *p, const uint8_t *in, unsigned int d)
{
	size_t bits = 0;

	for (size_t i = 0; i < OSK_MLKEM_N; i++) {
		uint16_t v = 0u;

		for (unsigned int b = 0; b < d; b++) {
			uint16_t bit =
			    (uint16_t)(((unsigned int)in[bits / 8u] >> (bits % 8u)) & 1u);

			v = (uint16_t)(v | (uint16_t)(bit << b));
			bits++;
		}
		p->c[i] = v;
	}
}

/* FIPS 203, 4.2.1: Compress_d(y) = round(2^d/q * y) mod 2^d, halbe Werte
 * aufwaerts. Die Division ist durch die Konstante Q, also wieder keine
 * echte Division. */
void osk_poly_compress(struct osk_poly *p, unsigned int d)
{
	uint32_t mask = (1u << d) - 1u;

	for (size_t i = 0; i < OSK_MLKEM_N; i++) {
		uint32_t t = ((uint32_t)p->c[i] << d) + (uint32_t)(OSK_MLKEM_Q / 2);

		p->c[i] = (uint16_t)((t / (uint32_t)OSK_MLKEM_Q) & mask);
	}
}

void osk_poly_decompress(struct osk_poly *p, unsigned int d)
{
	for (size_t i = 0; i < OSK_MLKEM_N; i++) {
		uint32_t t = (uint32_t)p->c[i] * (uint32_t)OSK_MLKEM_Q + (1u << (d - 1u));

		p->c[i] = (uint16_t)(t >> d);
	}
}

int osk_poly_is_reduced(const struct osk_poly *p)
{
	uint16_t bad = 0u;

	/* Ohne Verzweigung: das Ergebnis ist oeffentlich, aber die Funktion
	 * laeuft auch ueber eigene Schluessel, und eine Schleife, die beim
	 * ersten Treffer abbricht, verraet die Stelle. */
	for (size_t i = 0; i < OSK_MLKEM_N; i++) {
		bad |= (uint16_t)(p->c[i] >= (uint16_t)OSK_MLKEM_Q);
	}
	return bad == 0u;
}
