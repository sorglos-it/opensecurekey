/* secp256k1, selbst geschrieben gegen die Kurvenparameter aus SEC 2 v2.
 *
 * Kein Fremdcode, keine uebernommenen Namen. Aufbau:
 *  - Koerperrechnung mod p mit der schnellen Reduktion, die nur fuer diese
 *    Kurve gilt: 2^256 ist mod p dasselbe wie 2^32 + 977
 *  - Punkte innen in Jacobi-Koordinaten (X : Y : Z), damit kein Kehrwert je
 *    Schritt gebraucht wird; nach aussen immer affin
 *  - Punktvervielfachung als "verdoppeln und immer addieren": das Ergebnis
 *    wird mit einer Maske gewaehlt, nicht mit einem Sprung, damit die
 *    Laufzeit nichts ueber den privaten Schluessel verraet */

#include <string.h>
#include "wallet_ec.h"
#include "osk_crypto.h"

/* --- Kurvenparameter ----------------------------------------------------- */

/* p = 2^256 - 2^32 - 977 */
static const osk_bn_t EC_P = {{0xFFFFFC2Fu, 0xFFFFFFFEu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
			       0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu}};
/* n = Ordnung von G */
static const osk_bn_t EC_N = {{0xD0364141u, 0xBFD25E8Cu, 0xAF48A03Bu, 0xBAAEDCE6u, 0xFFFFFFFEu,
			       0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu}};
/* n/2, abgerundet - die Grenze fuer "kleines s" */
static const osk_bn_t EC_N_HALF = {{0x681B20A0u, 0xDFE92F46u, 0x57A4501Du, 0x5D576E73u, 0xFFFFFFFFu,
				    0xFFFFFFFFu, 0xFFFFFFFFu, 0x7FFFFFFFu}};
static const osk_bn_t EC_GX = {{0x16F81798u, 0x59F2815Bu, 0x2DCE28D9u, 0x029BFCDBu, 0xCE870B07u,
				0x55A06295u, 0xF9DCBBACu, 0x79BE667Eu}};
static const osk_bn_t EC_GY = {{0xFB10D4B8u, 0x9C47D08Fu, 0xA6855419u, 0xFD17B448u, 0x0E1108A8u,
				0x5DA4FBFCu, 0x26A3C465u, 0x483ADA77u}};

const osk_bn_t *osk_ec_order(void)
{
	return &EC_N;
}

const osk_bn_t *osk_ec_field(void)
{
	return &EC_P;
}

/* --- Koerperrechnung mod p ----------------------------------------------- */

static void fp_add(osk_bn_t *r, const osk_bn_t *a, const osk_bn_t *b)
{
	osk_bn_add_mod(r, a, b, &EC_P);
}

static void fp_sub(osk_bn_t *r, const osk_bn_t *a, const osk_bn_t *b)
{
	osk_bn_sub_mod(r, a, b, &EC_P);
}

/* Reduktion mod p. Eingabe bis 512 Bit, Ausgabe kleiner als p.
 *
 * Der Trick dieser Kurve: 2^256 = 2^32 + 977 (mod p). Die oberen Limbs
 * wandern also mit dem Faktor (2^32 + 977) nach unten. Vier Durchgaenge
 * bringen jede 512-Bit-Zahl auf hoechstens 2^256 + 2^33, danach bleiben nur
 * noch die beiden festen Nacharbeiten. Die Zahl der Durchgaenge ist fest -
 * eine Abbruchbedingung am Wert wuerde die Laufzeit vom Geheimnis abhaengig
 * machen. */
static void fp_reduce(osk_bn_t *r, const osk_bn2_t *in)
{
	uint32_t t[2 * OSK_BN_LIMBS + 1];
	uint32_t hi[OSK_BN_LIMBS + 1];
	osk_bn_t lo, extra, sum;
	uint32_t mask, carry32, borrow;

	memcpy(t, in->v, sizeof(in->v));
	t[2 * OSK_BN_LIMBS] = 0;

	for (unsigned pass = 0; pass < 4; pass++) {
		uint64_t carry = 0;

		for (unsigned i = 0; i <= OSK_BN_LIMBS; i++) {
			hi[i] = t[OSK_BN_LIMBS + i];
			t[OSK_BN_LIMBS + i] = 0;
		}
		/* t += hi * 977 */
		for (unsigned i = 0; i <= OSK_BN_LIMBS; i++) {
			uint64_t cur = (uint64_t)hi[i] * 977u + (uint64_t)t[i] + carry;

			t[i] = (uint32_t)(cur & 0xFFFFFFFFu);
			carry = cur >> 32;
		}
		for (unsigned i = OSK_BN_LIMBS + 1u; i <= 2u * OSK_BN_LIMBS; i++) {
			uint64_t cur = (uint64_t)t[i] + carry;

			t[i] = (uint32_t)(cur & 0xFFFFFFFFu);
			carry = cur >> 32;
		}
		/* t += hi * 2^32 */
		carry = 0;
		for (unsigned i = 0; i <= OSK_BN_LIMBS; i++) {
			uint64_t cur = (uint64_t)t[i + 1u] + (uint64_t)hi[i] + carry;

			t[i + 1u] = (uint32_t)(cur & 0xFFFFFFFFu);
			carry = cur >> 32;
		}
		for (unsigned i = OSK_BN_LIMBS + 2u; i <= 2u * OSK_BN_LIMBS; i++) {
			uint64_t cur = (uint64_t)t[i] + carry;

			t[i] = (uint32_t)(cur & 0xFFFFFFFFu);
			carry = cur >> 32;
		}
	}

	/* Jetzt steht in t[8] nur noch 0 oder 1. Einmal nach unten falten. */
	memcpy(lo.v, t, sizeof(lo.v));
	mask = (uint32_t)0u - (t[OSK_BN_LIMBS] & 1u);
	osk_bn_zero(&extra);
	extra.v[0] = 977u & mask;
	extra.v[1] = 1u & mask;
	carry32 = osk_bn_add(&lo, &lo, &extra);

	/* Lief das ueber, war der Wert mindestens p: dann steckt ein weiteres
	 * 2^256 drin, und das ist wieder 2^32 + 977. Diesmal kann nichts mehr
	 * ueberlaufen, weil der Rest klein ist. */
	mask = (uint32_t)0u - (carry32 & 1u);
	osk_bn_zero(&extra);
	extra.v[0] = 977u & mask;
	extra.v[1] = 1u & mask;
	(void)osk_bn_add(&lo, &lo, &extra);

	/* Zuletzt: noch kleiner als p machen. Kein if auf dem Wert - gewaehlt
	 * wird mit einer Maske, damit die Laufzeit nichts verraet. Kein
	 * Borger heisst: lo war mindestens p, also gilt die Differenz. */
	borrow = osk_bn_sub(&sum, &lo, &EC_P);
	mask = (uint32_t)0u - (borrow ^ 1u);
	for (unsigned i = 0; i < OSK_BN_LIMBS; i++)
		r->v[i] = (lo.v[i] & ~mask) | (sum.v[i] & mask);

	osk_wipe(t, sizeof(t));
	osk_wipe(hi, sizeof(hi));
	osk_bn_wipe(&lo);
	osk_bn_wipe(&sum);
	osk_bn_wipe(&extra);
}

static void fp_mul(osk_bn_t *r, const osk_bn_t *a, const osk_bn_t *b)
{
	osk_bn2_t wide;

	osk_bn_mul(&wide, a, b);
	fp_reduce(r, &wide);
	osk_wipe(&wide, sizeof(wide));
}

static void fp_sqr(osk_bn_t *r, const osk_bn_t *a)
{
	fp_mul(r, a, a);
}

static void fp_neg(osk_bn_t *r, const osk_bn_t *a)
{
	osk_bn_t zero;

	osk_bn_zero(&zero);
	fp_sub(r, &zero, a);
}

static bool fp_eq(const osk_bn_t *a, const osk_bn_t *b)
{
	return osk_bn_cmp(a, b) == 0;
}

/* r = a^e mod p, e als 256-Bit-Zahl. Immer alle 256 Bits. */
static void fp_pow(osk_bn_t *r, const osk_bn_t *a, const osk_bn_t *e)
{
	osk_bn_t acc, base;

	osk_bn_set_u32(&acc, 1);
	osk_bn_copy(&base, a);
	for (unsigned i = 0; i < 32u * OSK_BN_LIMBS; i++) {
		osk_bn_t prod;

		fp_mul(&prod, &acc, &base);
		if (osk_bn_bit(e, i))
			osk_bn_copy(&acc, &prod);
		fp_sqr(&base, &base);
		osk_bn_wipe(&prod);
	}
	osk_bn_copy(r, &acc);
	osk_bn_wipe(&acc);
	osk_bn_wipe(&base);
}

static void fp_inv(osk_bn_t *r, const osk_bn_t *a)
{
	osk_bn_t e, two;

	osk_bn_set_u32(&two, 2);
	(void)osk_bn_sub(&e, &EC_P, &two);
	fp_pow(r, a, &e);
	osk_bn_wipe(&e);
}

/* Quadratwurzel mod p. Es gilt p = 3 (mod 4), also ist a^((p+1)/4) eine
 * Wurzel - wenn es ueberhaupt eine gibt. Das prueft der Aufrufer, indem er
 * das Ergebnis quadriert. */
static void fp_sqrt(osk_bn_t *r, const osk_bn_t *a)
{
	osk_bn_t e;
	uint32_t carry = 0;

	osk_bn_copy(&e, &EC_P);
	e.v[0] += 1u; /* p endet auf 0x2F, laeuft also nicht ueber */
	for (unsigned k = 0; k < 2; k++) {
		carry = 0;
		for (unsigned i = OSK_BN_LIMBS; i > 0; i--) {
			uint32_t limb = e.v[i - 1u];

			e.v[i - 1u] = (limb >> 1) | (carry << 31);
			carry = limb & 1u;
		}
	}
	fp_pow(r, a, &e);
	osk_bn_wipe(&e);
}

/* --- Punkte -------------------------------------------------------------- */

typedef struct {
	osk_bn_t x, y, z; /* z == 0 heisst: der unendlich ferne Punkt */
} ec_jac_t;

static void jac_set_infinity(ec_jac_t *p)
{
	osk_bn_set_u32(&p->x, 1);
	osk_bn_set_u32(&p->y, 1);
	osk_bn_zero(&p->z);
}

static void jac_from_affine(ec_jac_t *out, const osk_ec_point_t *in)
{
	if (in->infinity) {
		jac_set_infinity(out);
		return;
	}
	osk_bn_copy(&out->x, &in->x);
	osk_bn_copy(&out->y, &in->y);
	osk_bn_set_u32(&out->z, 1);
}

static void jac_to_affine(osk_ec_point_t *out, const ec_jac_t *in)
{
	osk_bn_t zi, zi2, zi3;

	if (osk_bn_is_zero(&in->z)) {
		memset(out, 0, sizeof(*out));
		out->infinity = true;
		return;
	}
	fp_inv(&zi, &in->z);
	fp_sqr(&zi2, &zi);
	fp_mul(&zi3, &zi2, &zi);
	fp_mul(&out->x, &in->x, &zi2);
	fp_mul(&out->y, &in->y, &zi3);
	out->infinity = false;
	osk_bn_wipe(&zi);
	osk_bn_wipe(&zi2);
	osk_bn_wipe(&zi3);
}

/* Verdoppeln. a = 0 bei dieser Kurve, das kuerzt die Rechnung. */
static void jac_double(ec_jac_t *out, const ec_jac_t *in)
{
	osk_bn_t a, b, c, d, t1, t2;

	if (osk_bn_is_zero(&in->z) || osk_bn_is_zero(&in->y)) {
		jac_set_infinity(out);
		return;
	}
	fp_sqr(&a, &in->x);	/* A = X^2 */
	fp_sqr(&b, &in->y);	/* B = Y^2 */
	fp_sqr(&c, &b);		/* C = B^2 */
	fp_mul(&d, &in->x, &b); /* D = X*B */
	fp_add(&d, &d, &d);	/* D = 2*X*B */
	fp_add(&d, &d, &d);	/* D = 4*X*B */
	fp_add(&t1, &a, &a);	/* 2A */
	fp_add(&t1, &t1, &a);	/* E = 3A */
	fp_sqr(&t2, &t1);	/* E^2 */
	fp_sub(&t2, &t2, &d);	/* E^2 - D */
	fp_sub(&t2, &t2, &d);	/* X' = E^2 - 2D */
	/* Y' = E*(D - X') - 8C */
	fp_sub(&a, &d, &t2);
	fp_mul(&a, &t1, &a);
	fp_add(&c, &c, &c);
	fp_add(&c, &c, &c);
	fp_add(&c, &c, &c); /* 8C */
	fp_sub(&a, &a, &c);
	/* Z' = 2*Y*Z */
	fp_mul(&b, &in->y, &in->z);
	fp_add(&b, &b, &b);

	osk_bn_copy(&out->x, &t2);
	osk_bn_copy(&out->y, &a);
	osk_bn_copy(&out->z, &b);

	osk_bn_wipe(&a);
	osk_bn_wipe(&b);
	osk_bn_wipe(&c);
	osk_bn_wipe(&d);
	osk_bn_wipe(&t1);
	osk_bn_wipe(&t2);
}

static void jac_add(ec_jac_t *out, const ec_jac_t *p, const ec_jac_t *q)
{
	osk_bn_t z1z1, z2z2, u1, u2, s1, s2, h, i, j, rr, v, t;

	if (osk_bn_is_zero(&p->z)) {
		*out = *q;
		return;
	}
	if (osk_bn_is_zero(&q->z)) {
		*out = *p;
		return;
	}
	fp_sqr(&z1z1, &p->z);
	fp_sqr(&z2z2, &q->z);
	fp_mul(&u1, &p->x, &z2z2);
	fp_mul(&u2, &q->x, &z1z1);
	fp_mul(&s1, &p->y, &z2z2);
	fp_mul(&s1, &s1, &q->z);
	fp_mul(&s2, &q->y, &z1z1);
	fp_mul(&s2, &s2, &p->z);
	if (fp_eq(&u1, &u2)) {
		if (fp_eq(&s1, &s2))
			jac_double(out, p);
		else
			jac_set_infinity(out);
		goto done;
	}
	fp_sub(&h, &u2, &u1);
	fp_add(&i, &h, &h);
	fp_sqr(&i, &i); /* I = (2H)^2 */
	fp_mul(&j, &h, &i);
	fp_sub(&rr, &s2, &s1);
	fp_add(&rr, &rr, &rr); /* r = 2*(S2-S1) */
	fp_mul(&v, &u1, &i);
	/* X3 = r^2 - J - 2V */
	fp_sqr(&t, &rr);
	fp_sub(&t, &t, &j);
	fp_sub(&t, &t, &v);
	fp_sub(&t, &t, &v);
	osk_bn_copy(&out->x, &t);
	/* Y3 = r*(V - X3) - 2*S1*J */
	fp_sub(&v, &v, &t);
	fp_mul(&v, &rr, &v);
	fp_mul(&t, &s1, &j);
	fp_add(&t, &t, &t);
	fp_sub(&out->y, &v, &t);
	/* Z3 = ((Z1+Z2)^2 - Z1Z1 - Z2Z2) * H */
	fp_add(&t, &p->z, &q->z);
	fp_sqr(&t, &t);
	fp_sub(&t, &t, &z1z1);
	fp_sub(&t, &t, &z2z2);
	fp_mul(&out->z, &t, &h);
done:
	osk_bn_wipe(&z1z1);
	osk_bn_wipe(&z2z2);
	osk_bn_wipe(&u1);
	osk_bn_wipe(&u2);
	osk_bn_wipe(&s1);
	osk_bn_wipe(&s2);
	osk_bn_wipe(&h);
	osk_bn_wipe(&i);
	osk_bn_wipe(&j);
	osk_bn_wipe(&rr);
	osk_bn_wipe(&v);
	osk_bn_wipe(&t);
}

/* out = k * base. Verdoppeln und immer addieren: in jedem Schritt wird
 * beides gerechnet und mit einer Maske gewaehlt. */
static void jac_mul(ec_jac_t *out, const osk_bn_t *k, const ec_jac_t *base)
{
	ec_jac_t acc, sum;

	jac_set_infinity(&acc);
	for (unsigned i = 32u * OSK_BN_LIMBS; i > 0; i--) {
		unsigned index = i - 1u;

		jac_double(&acc, &acc);
		jac_add(&sum, &acc, base);
		if (osk_bn_bit(k, index))
			acc = sum;
	}
	*out = acc;
	osk_wipe(&sum, sizeof(sum));
}

static void generator(ec_jac_t *out)
{
	osk_bn_copy(&out->x, &EC_GX);
	osk_bn_copy(&out->y, &EC_GY);
	osk_bn_set_u32(&out->z, 1);
}

void osk_ec_point_add(osk_ec_point_t *out, const osk_ec_point_t *a, const osk_ec_point_t *b)
{
	ec_jac_t ja, jb, jr;

	jac_from_affine(&ja, a);
	jac_from_affine(&jb, b);
	jac_add(&jr, &ja, &jb);
	jac_to_affine(out, &jr);
	osk_wipe(&ja, sizeof(ja));
	osk_wipe(&jb, sizeof(jb));
	osk_wipe(&jr, sizeof(jr));
}

osk_result_t osk_ec_point_mul_base(const osk_bn_t *k, osk_ec_point_t *out)
{
	ec_jac_t g, r;

	if (k == NULL || out == NULL)
		return OSK_ERR_ARG;
	generator(&g);
	jac_mul(&r, k, &g);
	jac_to_affine(out, &r);
	osk_wipe(&r, sizeof(r));
	return out->infinity ? OSK_ERR_FORMAT : OSK_OK;
}

/* --- Schluessel und Punkte in Bytes ------------------------------------- */

bool osk_ec_privkey_valid(const uint8_t key[32])
{
	osk_bn_t k;
	bool ok;

	if (key == NULL)
		return false;
	osk_bn_from_be(&k, key);
	ok = !osk_bn_is_zero(&k) && osk_bn_cmp(&k, &EC_N) < 0;
	osk_bn_wipe(&k);
	return ok;
}

osk_result_t osk_ec_public_from_private(const uint8_t priv[32], osk_ec_point_t *out)
{
	osk_bn_t k;
	osk_result_t res;

	if (priv == NULL || out == NULL)
		return OSK_ERR_ARG;
	if (!osk_ec_privkey_valid(priv))
		return OSK_ERR_ARG;
	osk_bn_from_be(&k, priv);
	res = osk_ec_point_mul_base(&k, out);
	osk_bn_wipe(&k);
	return res;
}

osk_result_t osk_ec_point_encode(const osk_ec_point_t *p, bool compressed, uint8_t *out, size_t cap,
				 size_t *len_out)
{
	size_t need = compressed ? OSK_EC_PUB_COMPRESSED : OSK_EC_PUB_UNCOMPRESSED;

	if (p == NULL || out == NULL || len_out == NULL)
		return OSK_ERR_ARG;
	if (p->infinity)
		return OSK_ERR_FORMAT;
	if (cap < need)
		return OSK_ERR_ARG;
	if (compressed) {
		out[0] = osk_bn_is_odd(&p->y) ? 0x03u : 0x02u;
		osk_bn_to_be(&p->x, out + 1);
	} else {
		out[0] = 0x04u;
		osk_bn_to_be(&p->x, out + 1);
		osk_bn_to_be(&p->y, out + 33);
	}
	*len_out = need;
	return OSK_OK;
}

/* Liegt (x,y) wirklich auf y^2 = x^3 + 7? Ein Punkt daneben kann eine
 * Signatur auf einer anderen, schwachen Kurve erzwingen. */
static bool on_curve(const osk_bn_t *x, const osk_bn_t *y)
{
	osk_bn_t lhs, rhs, seven;

	if (osk_bn_cmp(x, &EC_P) >= 0 || osk_bn_cmp(y, &EC_P) >= 0)
		return false;
	fp_sqr(&lhs, y);
	fp_sqr(&rhs, x);
	fp_mul(&rhs, &rhs, x);
	osk_bn_set_u32(&seven, 7);
	fp_add(&rhs, &rhs, &seven);
	return fp_eq(&lhs, &rhs);
}

osk_result_t osk_ec_point_decode(const uint8_t *in, size_t len, osk_ec_point_t *out)
{
	if (in == NULL || out == NULL)
		return OSK_ERR_ARG;
	memset(out, 0, sizeof(*out));
	if (len == OSK_EC_PUB_UNCOMPRESSED && in[0] == 0x04u) {
		osk_bn_from_be(&out->x, in + 1);
		osk_bn_from_be(&out->y, in + 33);
		return on_curve(&out->x, &out->y) ? OSK_OK : OSK_ERR_FORMAT;
	}
	if (len == OSK_EC_PUB_COMPRESSED && (in[0] == 0x02u || in[0] == 0x03u)) {
		osk_bn_t y2, y, seven, check;
		bool want_odd = in[0] == 0x03u;

		osk_bn_from_be(&out->x, in + 1);
		if (osk_bn_cmp(&out->x, &EC_P) >= 0)
			return OSK_ERR_FORMAT;
		fp_sqr(&y2, &out->x);
		fp_mul(&y2, &y2, &out->x);
		osk_bn_set_u32(&seven, 7);
		fp_add(&y2, &y2, &seven);
		fp_sqrt(&y, &y2);
		fp_sqr(&check, &y);
		if (!fp_eq(&check, &y2))
			return OSK_ERR_FORMAT; /* kein Quadrat: x liegt nicht auf der Kurve */
		if (osk_bn_is_odd(&y) != want_odd)
			fp_neg(&y, &y);
		osk_bn_copy(&out->y, &y);
		return OSK_OK;
	}
	return OSK_ERR_FORMAT;
}

osk_result_t osk_ec_pub_tweak_add(const osk_ec_point_t *p, const uint8_t tweak[32],
				  osk_ec_point_t *out)
{
	osk_bn_t t;
	osk_ec_point_t add;
	osk_result_t res;

	if (p == NULL || tweak == NULL || out == NULL)
		return OSK_ERR_ARG;
	if (!osk_ec_privkey_valid(tweak))
		return OSK_ERR_FORMAT;
	osk_bn_from_be(&t, tweak);
	res = osk_ec_point_mul_base(&t, &add);
	osk_bn_wipe(&t);
	if (res != OSK_OK)
		return res;
	osk_ec_point_add(out, p, &add);
	return out->infinity ? OSK_ERR_FORMAT : OSK_OK;
}

osk_result_t osk_ec_priv_tweak_add(const uint8_t key[32], const uint8_t tweak[32], uint8_t out[32])
{
	osk_bn_t a, b, sum;

	if (key == NULL || tweak == NULL || out == NULL)
		return OSK_ERR_ARG;
	if (!osk_ec_privkey_valid(key))
		return OSK_ERR_ARG;
	if (!osk_ec_privkey_valid(tweak))
		return OSK_ERR_FORMAT; /* IL >= n: BIP32 verlangt den naechsten Index */
	osk_bn_from_be(&a, key);
	osk_bn_from_be(&b, tweak);
	osk_bn_add_mod(&sum, &a, &b, &EC_N);
	osk_bn_to_be(&sum, out);
	osk_bn_wipe(&a);
	osk_bn_wipe(&b);
	osk_bn_wipe(&sum);
	if (!osk_ec_privkey_valid(out)) {
		osk_wipe(out, 32);
		return OSK_ERR_FORMAT; /* Summe ist 0 - ebenfalls naechster Index */
	}
	return OSK_OK;
}

/* --- Pruefen und Zurueckrechnen ----------------------------------------- */

/* Punkt aus r und der Kennziffer: x = r, y aus der Kurvengleichung. */
static bool point_from_r(const osk_bn_t *r, bool odd, osk_ec_point_t *out)
{
	osk_bn_t y2, y, seven, check;

	if (osk_bn_cmp(r, &EC_P) >= 0)
		return false;
	memset(out, 0, sizeof(*out));
	osk_bn_copy(&out->x, r);
	fp_sqr(&y2, r);
	fp_mul(&y2, &y2, r);
	osk_bn_set_u32(&seven, 7);
	fp_add(&y2, &y2, &seven);
	fp_sqrt(&y, &y2);
	fp_sqr(&check, &y);
	if (!fp_eq(&check, &y2))
		return false;
	if (osk_bn_is_odd(&y) != odd)
		fp_neg(&y, &y);
	osk_bn_copy(&out->y, &y);
	return true;
}

static bool sig_parts(const uint8_t r[32], const uint8_t s[32], osk_bn_t *rr, osk_bn_t *ss)
{
	osk_bn_from_be(rr, r);
	osk_bn_from_be(ss, s);
	if (osk_bn_is_zero(rr) || osk_bn_is_zero(ss))
		return false;
	if (osk_bn_cmp(rr, &EC_N) >= 0 || osk_bn_cmp(ss, &EC_N) >= 0)
		return false;
	return true;
}

/* Q = r^-1 * (s*R - e*G) */
static bool recover(const uint8_t hash[32], const osk_bn_t *rr, const osk_bn_t *ss, bool odd,
		    osk_ec_point_t *out)
{
	osk_ec_point_t rp;
	osk_bn_t e, r_inv, u1, u2, neg_e;
	ec_jac_t jr, jg, t1, t2, sum;

	if (!point_from_r(rr, odd, &rp))
		return false;
	osk_bn_from_be(&e, hash);
	/* e wird mod n genommen - der Hash ist 256 Bit und kann groesser sein. */
	if (osk_bn_cmp(&e, &EC_N) >= 0)
		(void)osk_bn_sub(&e, &e, &EC_N);
	osk_bn_inv_mod(&r_inv, rr, &EC_N);
	osk_bn_mul_mod(&u1, &r_inv, ss, &EC_N);
	osk_bn_sub_mod(&neg_e, &EC_N, &e, &EC_N);
	osk_bn_mul_mod(&u2, &r_inv, &neg_e, &EC_N);

	jac_from_affine(&jr, &rp);
	generator(&jg);
	jac_mul(&t1, &u1, &jr);
	jac_mul(&t2, &u2, &jg);
	jac_add(&sum, &t1, &t2);
	jac_to_affine(out, &sum);

	osk_bn_wipe(&e);
	osk_bn_wipe(&r_inv);
	osk_bn_wipe(&u1);
	osk_bn_wipe(&u2);
	osk_bn_wipe(&neg_e);
	return !out->infinity;
}

osk_result_t osk_ec_recovery_id(const uint8_t hash[32], const uint8_t r[32], const uint8_t s[32],
				const osk_ec_point_t *expected, uint8_t *recid_out)
{
	osk_bn_t rr, ss;

	if (hash == NULL || r == NULL || s == NULL || expected == NULL || recid_out == NULL)
		return OSK_ERR_ARG;
	if (expected->infinity || !sig_parts(r, s, &rr, &ss))
		return OSK_ERR_FORMAT;
	for (unsigned candidate = 0; candidate < 2; candidate++) {
		osk_ec_point_t got;

		if (!recover(hash, &rr, &ss, candidate == 1u, &got))
			continue;
		if (fp_eq(&got.x, &expected->x) && fp_eq(&got.y, &expected->y)) {
			*recid_out = (uint8_t)candidate;
			return OSK_OK;
		}
	}
	return OSK_ERR_FORMAT;
}

bool osk_ec_verify(const uint8_t hash[32], const uint8_t r[32], const uint8_t s[32],
		   const osk_ec_point_t *pub)
{
	osk_bn_t rr, ss, e, w, u1, u2, xmod;
	ec_jac_t jg, jp, t1, t2, sum;
	osk_ec_point_t point;
	bool ok;

	if (hash == NULL || r == NULL || s == NULL || pub == NULL || pub->infinity)
		return false;
	if (!sig_parts(r, s, &rr, &ss))
		return false;
	if (!on_curve(&pub->x, &pub->y))
		return false;
	osk_bn_from_be(&e, hash);
	if (osk_bn_cmp(&e, &EC_N) >= 0)
		(void)osk_bn_sub(&e, &e, &EC_N);
	osk_bn_inv_mod(&w, &ss, &EC_N);
	osk_bn_mul_mod(&u1, &e, &w, &EC_N);
	osk_bn_mul_mod(&u2, &rr, &w, &EC_N);
	generator(&jg);
	jac_from_affine(&jp, pub);
	jac_mul(&t1, &u1, &jg);
	jac_mul(&t2, &u2, &jp);
	jac_add(&sum, &t1, &t2);
	jac_to_affine(&point, &sum);
	if (point.infinity)
		return false;
	osk_bn_copy(&xmod, &point.x);
	if (osk_bn_cmp(&xmod, &EC_N) >= 0)
		(void)osk_bn_sub(&xmod, &xmod, &EC_N);
	ok = osk_bn_cmp(&xmod, &rr) == 0;
	osk_bn_wipe(&e);
	osk_bn_wipe(&w);
	osk_bn_wipe(&u1);
	osk_bn_wipe(&u2);
	return ok;
}

bool osk_ec_s_is_high(const uint8_t s[32])
{
	osk_bn_t v;

	if (s == NULL)
		return false;
	osk_bn_from_be(&v, s);
	return osk_bn_cmp(&v, &EC_N_HALF) > 0;
}

void osk_ec_s_negate(const uint8_t s[32], uint8_t out[32])
{
	osk_bn_t v, r;

	osk_bn_from_be(&v, s);
	osk_bn_sub_mod(&r, &EC_N, &v, &EC_N);
	osk_bn_to_be(&r, out);
	osk_bn_wipe(&v);
	osk_bn_wipe(&r);
}
