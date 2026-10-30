/* ML-KEM-768 nach FIPS 203. Siehe pqc_mlkem.h. */
#define OSK_PQC_INTERNAL 1
#include "pqc_mlkem.h"
#include "pqc_poly.h"
#include "pqc_keccak.h"
#include "osk_mem.h"

#define POLYVEC_BYTES (OSK_MLKEM_K * OSK_MLKEM_POLY_BYTES)
#define C1_BYTES (32 * OSK_MLKEM_DU * OSK_MLKEM_K)

/* Die laengste Rauschportion, die PRF liefern muss. */
#define PRF_BYTES (64 * (OSK_MLKEM_ETA1 > OSK_MLKEM_ETA2 ? OSK_MLKEM_ETA1 : OSK_MLKEM_ETA2))

struct polyvec {
	struct osk_poly v[OSK_MLKEM_K];
};

/* PRF_eta(s, b) = SHAKE256(s || b, 64*eta) (FIPS 203, 4.1). */
static void prf(uint8_t *out, unsigned int eta, const uint8_t s[32], uint8_t counter)
{
	struct osk_keccak st;

	osk_keccak_init(&st, OSK_SHAKE256_RATE, OSK_KECCAK_PAD_SHAKE);
	osk_keccak_absorb(&st, s, 32u);
	osk_keccak_absorb(&st, &counter, 1u);
	osk_keccak_squeeze(&st, out, 64u * eta);
	osk_keccak_wipe(&st);
}

static void vec_encode12(uint8_t *out, const struct polyvec *p)
{
	for (size_t i = 0; i < OSK_MLKEM_K; i++) {
		osk_poly_encode(out + i * OSK_MLKEM_POLY_BYTES, &p->v[i], 12u);
	}
}

/* Auspacken mit der Pruefung aus FIPS 203, 7.2: nur reduzierte Werte sind
 * ein gueltiger Schluessel. Ein nicht reduzierter liesse sich sonst in
 * zwei Darstellungen schreiben. */
static int vec_decode12(struct polyvec *p, const uint8_t *in)
{
	int ok = 1;

	for (size_t i = 0; i < OSK_MLKEM_K; i++) {
		osk_poly_decode(&p->v[i], in + i * OSK_MLKEM_POLY_BYTES, 12u);
		ok &= osk_poly_is_reduced(&p->v[i]);
	}
	return ok;
}

/* K-PKE.KeyGen (FIPS 203, Alg. 13), aufgeteilt, damit keygen() lesbar
 * bleibt. dk_pke ist der gepackte geheime Vektor. */
static void kpke_keygen(uint8_t ek[OSK_MLKEM_EK_BYTES], uint8_t dk_pke[POLYVEC_BYTES],
			const uint8_t d[32])
{
	uint8_t g_in[33];
	uint8_t g_out[64];
	uint8_t noise[PRF_BYTES];
	struct polyvec s_ntt, e_ntt, t_ntt;
	struct osk_poly a;
	const uint8_t *rho;
	const uint8_t *sigma;
	uint8_t n = 0u;

	for (size_t i = 0; i < 32u; i++) {
		g_in[i] = d[i];
	}
	/* Das angehaengte K ist in FIPS 203 nachgetragen worden: ohne es
	 * haengen die drei Groessen am gleichen Keim. */
	g_in[32] = (uint8_t)OSK_MLKEM_K;
	osk_sha3_512(g_out, g_in, sizeof(g_in));
	rho = g_out;
	sigma = g_out + 32;

	for (size_t i = 0; i < OSK_MLKEM_K; i++) {
		prf(noise, OSK_MLKEM_ETA1, sigma, n);
		n++;
		osk_poly_cbd(&s_ntt.v[i], noise, OSK_MLKEM_ETA1);
		osk_poly_ntt(&s_ntt.v[i]);
	}
	for (size_t i = 0; i < OSK_MLKEM_K; i++) {
		prf(noise, OSK_MLKEM_ETA1, sigma, n);
		n++;
		osk_poly_cbd(&e_ntt.v[i], noise, OSK_MLKEM_ETA1);
		osk_poly_ntt(&e_ntt.v[i]);
	}

	/* t = A o s + e. A wird Element fuer Element gezogen und nicht
	 * aufbewahrt - die ganze Matrix waere 4,5 kB RAM fuer nichts. */
	for (size_t i = 0; i < OSK_MLKEM_K; i++) {
		t_ntt.v[i] = e_ntt.v[i];
		for (size_t j = 0; j < OSK_MLKEM_K; j++) {
			osk_poly_sample_ntt(&a, rho, (uint8_t)j, (uint8_t)i);
			osk_poly_basemul_acc(&t_ntt.v[i], &a, &s_ntt.v[j]);
		}
	}

	vec_encode12(ek, &t_ntt);
	for (size_t i = 0; i < 32u; i++) {
		ek[POLYVEC_BYTES + i] = rho[i];
	}
	vec_encode12(dk_pke, &s_ntt);

	osk_wipe(g_in, sizeof(g_in));
	osk_wipe(g_out, sizeof(g_out));
	osk_wipe(noise, sizeof(noise));
	osk_wipe(&s_ntt, sizeof(s_ntt));
	osk_wipe(&e_ntt, sizeof(e_ntt));
	osk_wipe(&t_ntt, sizeof(t_ntt));
	osk_wipe(&a, sizeof(a));
}

void osk_mlkem_keygen(uint8_t ek[OSK_MLKEM_EK_BYTES], uint8_t dk[OSK_MLKEM_DK_BYTES],
		      const uint8_t d[32], const uint8_t z[32])
{
	kpke_keygen(ek, dk, d);
	for (size_t i = 0; i < OSK_MLKEM_EK_BYTES; i++) {
		dk[POLYVEC_BYTES + i] = ek[i];
	}
	osk_sha3_256(dk + POLYVEC_BYTES + OSK_MLKEM_EK_BYTES, ek, OSK_MLKEM_EK_BYTES);
	for (size_t i = 0; i < 32u; i++) {
		dk[POLYVEC_BYTES + OSK_MLKEM_EK_BYTES + 32 + i] = z[i];
	}
}

/* K-PKE.Encrypt (FIPS 203, Alg. 14). Gibt 0, wenn ek unbrauchbar ist. */
static int kpke_encrypt(uint8_t ct[OSK_MLKEM_CT_BYTES], const uint8_t ek[OSK_MLKEM_EK_BYTES],
			const uint8_t m[32], const uint8_t r[32])
{
	uint8_t noise[PRF_BYTES];
	struct polyvec t_ntt, y_ntt, u;
	struct osk_poly a, v, tmp;
	const uint8_t *rho = ek + POLYVEC_BYTES;
	uint8_t n = 0u;
	int ok;

	ok = vec_decode12(&t_ntt, ek);
	if (!ok) {
		osk_wipe(&t_ntt, sizeof(t_ntt));
		return 0;
	}

	for (size_t i = 0; i < OSK_MLKEM_K; i++) {
		prf(noise, OSK_MLKEM_ETA1, r, n);
		n++;
		osk_poly_cbd(&y_ntt.v[i], noise, OSK_MLKEM_ETA1);
		osk_poly_ntt(&y_ntt.v[i]);
	}

	/* u = NTT^-1(A^T o y) + e1. A^T an Stelle (i,j) ist A an Stelle
	 * (j,i), darum die getauschte Reihenfolge der zwei Indexbytes. */
	for (size_t i = 0; i < OSK_MLKEM_K; i++) {
		osk_poly_zero(&u.v[i]);
		for (size_t j = 0; j < OSK_MLKEM_K; j++) {
			osk_poly_sample_ntt(&a, rho, (uint8_t)i, (uint8_t)j);
			osk_poly_basemul_acc(&u.v[i], &a, &y_ntt.v[j]);
		}
		osk_poly_intt(&u.v[i]);
		prf(noise, OSK_MLKEM_ETA2, r, n);
		n++;
		osk_poly_cbd(&tmp, noise, OSK_MLKEM_ETA2);
		osk_poly_add(&u.v[i], &u.v[i], &tmp);
	}

	/* v = NTT^-1(t o y) + e2 + Decompress_1(m) */
	osk_poly_zero(&v);
	for (size_t i = 0; i < OSK_MLKEM_K; i++) {
		osk_poly_basemul_acc(&v, &t_ntt.v[i], &y_ntt.v[i]);
	}
	osk_poly_intt(&v);
	prf(noise, OSK_MLKEM_ETA2, r, n);
	osk_poly_cbd(&tmp, noise, OSK_MLKEM_ETA2);
	osk_poly_add(&v, &v, &tmp);
	osk_poly_decode(&tmp, m, 1u);
	osk_poly_decompress(&tmp, 1u);
	osk_poly_add(&v, &v, &tmp);

	for (size_t i = 0; i < OSK_MLKEM_K; i++) {
		osk_poly_compress(&u.v[i], OSK_MLKEM_DU);
		osk_poly_encode(ct + i * 32u * OSK_MLKEM_DU, &u.v[i], OSK_MLKEM_DU);
	}
	osk_poly_compress(&v, OSK_MLKEM_DV);
	osk_poly_encode(ct + C1_BYTES, &v, OSK_MLKEM_DV);

	osk_wipe(noise, sizeof(noise));
	osk_wipe(&t_ntt, sizeof(t_ntt));
	osk_wipe(&y_ntt, sizeof(y_ntt));
	osk_wipe(&u, sizeof(u));
	osk_wipe(&a, sizeof(a));
	osk_wipe(&v, sizeof(v));
	osk_wipe(&tmp, sizeof(tmp));
	return 1;
}

/* K-PKE.Decrypt (FIPS 203, Alg. 15). */
static void kpke_decrypt(uint8_t m[32], const uint8_t dk_pke[POLYVEC_BYTES],
			 const uint8_t ct[OSK_MLKEM_CT_BYTES])
{
	struct polyvec s_ntt, u;
	struct osk_poly v, w;

	for (size_t i = 0; i < OSK_MLKEM_K; i++) {
		osk_poly_decode(&u.v[i], ct + i * 32u * OSK_MLKEM_DU, OSK_MLKEM_DU);
		osk_poly_decompress(&u.v[i], OSK_MLKEM_DU);
		osk_poly_ntt(&u.v[i]);
	}
	osk_poly_decode(&v, ct + C1_BYTES, OSK_MLKEM_DV);
	osk_poly_decompress(&v, OSK_MLKEM_DV);

	/* Der eigene geheime Schluessel ist von uns erzeugt; die Pruefung
	 * auf Reduziertheit ist hier dennoch billig und faengt einen
	 * verfaelschten Flash ab. */
	(void)vec_decode12(&s_ntt, dk_pke);

	osk_poly_zero(&w);
	for (size_t i = 0; i < OSK_MLKEM_K; i++) {
		osk_poly_basemul_acc(&w, &s_ntt.v[i], &u.v[i]);
	}
	osk_poly_intt(&w);
	osk_poly_sub(&w, &v, &w);
	osk_poly_compress(&w, 1u);
	osk_poly_encode(m, &w, 1u);

	osk_wipe(&s_ntt, sizeof(s_ntt));
	osk_wipe(&u, sizeof(u));
	osk_wipe(&v, sizeof(v));
	osk_wipe(&w, sizeof(w));
}

osk_result_t osk_mlkem_encaps(uint8_t ct[OSK_MLKEM_CT_BYTES], uint8_t ss[OSK_MLKEM_SS_BYTES],
			      const uint8_t ek[OSK_MLKEM_EK_BYTES], const uint8_t m[32])
{
	uint8_t g_in[64];
	uint8_t g_out[64];
	osk_result_t res = OSK_OK;

	for (size_t i = 0; i < 32u; i++) {
		g_in[i] = m[i];
	}
	osk_sha3_256(g_in + 32, ek, OSK_MLKEM_EK_BYTES);
	osk_sha3_512(g_out, g_in, sizeof(g_in));

	if (!kpke_encrypt(ct, ek, m, g_out + 32)) {
		res = OSK_ERR_FORMAT;
	} else {
		for (size_t i = 0; i < OSK_MLKEM_SS_BYTES; i++) {
			ss[i] = g_out[i];
		}
	}
	osk_wipe(g_in, sizeof(g_in));
	osk_wipe(g_out, sizeof(g_out));
	return res;
}

/* Gleich lange Laufzeit: vergleicht immer alle Byte und verzweigt nicht.
 * Ergebnis 0xff bei Gleichheit, 0x00 sonst. Genau diese Fehlerklasse -
 * eine Abkuerzung im Vergleich - hat 2024 EUCLEAK moeglich gemacht. */
static uint8_t equal_mask(const uint8_t *a, const uint8_t *b, size_t len)
{
	uint8_t diff = 0u;

	for (size_t i = 0; i < len; i++) {
		diff |= (uint8_t)(a[i] ^ b[i]);
	}
	/* diff == 0 -> 0xff, sonst 0x00, ohne Verzweigung */
	return (uint8_t)((((uint16_t)diff - 1u) >> 8) & 0xffu);
}

osk_result_t osk_mlkem_decaps(uint8_t ss[OSK_MLKEM_SS_BYTES], const uint8_t dk[OSK_MLKEM_DK_BYTES],
			      const uint8_t ct[OSK_MLKEM_CT_BYTES])
{
	const uint8_t *ek = dk + POLYVEC_BYTES;
	const uint8_t *h = ek + OSK_MLKEM_EK_BYTES;
	const uint8_t *z = h + 32;
	uint8_t g_in[64];
	uint8_t g_out[64];
	uint8_t reject[OSK_MLKEM_SS_BYTES];
	uint8_t ct2[OSK_MLKEM_CT_BYTES];
	uint8_t own_h[32];
	struct osk_keccak st;
	uint8_t mask;

	/* Steht im eigenen Flash ein anderer oeffentlicher Schluessel als
	 * der, auf den h zeigt, ist dieser dk kaputt - das ist ein Fehler
	 * im Geraet, kein Angriff von der Leitung, und darf gemeldet
	 * werden. */
	osk_sha3_256(own_h, ek, OSK_MLKEM_EK_BYTES);
	if (equal_mask(own_h, h, 32u) != 0xffu) {
		osk_wipe(own_h, sizeof(own_h));
		return OSK_ERR_FORMAT;
	}

	kpke_decrypt(g_in, dk, ct);
	for (size_t i = 0; i < 32u; i++) {
		g_in[32 + i] = h[i];
	}
	osk_sha3_512(g_out, g_in, sizeof(g_in));

	/* J(z || c) - das Ersatzgeheimnis fuer den Fall, dass der
	 * Schluesseltext nicht passt. */
	osk_keccak_init(&st, OSK_SHAKE256_RATE, OSK_KECCAK_PAD_SHAKE);
	osk_keccak_absorb(&st, z, 32u);
	osk_keccak_absorb(&st, ct, OSK_MLKEM_CT_BYTES);
	osk_keccak_squeeze(&st, reject, sizeof(reject));
	osk_keccak_wipe(&st);

	(void)kpke_encrypt(ct2, ek, g_in, g_out + 32);
	mask = equal_mask(ct, ct2, OSK_MLKEM_CT_BYTES);

	/* Auswaehlen ohne if: bei Gleichheit das echte Geheimnis, sonst
	 * das aus z. Ein if waere hier ein Orakel fuer die Frage, ob der
	 * Schluesseltext gueltig war. */
	for (size_t i = 0; i < OSK_MLKEM_SS_BYTES; i++) {
		ss[i] = (uint8_t)((g_out[i] & mask) | (reject[i] & (uint8_t)~mask));
	}

	osk_wipe(g_in, sizeof(g_in));
	osk_wipe(g_out, sizeof(g_out));
	osk_wipe(reject, sizeof(reject));
	osk_wipe(ct2, sizeof(ct2));
	osk_wipe(own_h, sizeof(own_h));
	return OSK_OK;
}
