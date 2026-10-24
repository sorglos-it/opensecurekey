/* Umsetzung der 256-Bit-Zahlen aus wallet_bn.h. Selbst geschrieben.
 *
 * Grundsaetze, die hier ueberall gelten:
 *  - feste Breite, nichts auf dem Haufen, keine Rekursion
 *  - kein Zweig haengt am Wert eines Geheimnisses; wo gewaehlt wird, wird
 *    mit einer Maske gewaehlt, nicht mit einem if
 *  - jede Funktion schreibt ihr Ergebnis vollstaendig, auch im Sonderfall */

#include <string.h>
#include "wallet_bn.h"
#include "osk_crypto.h"

void osk_bn_zero(osk_bn_t *r)
{
	memset(r->v, 0, sizeof(r->v));
}

void osk_bn_set_u32(osk_bn_t *r, uint32_t x)
{
	osk_bn_zero(r);
	r->v[0] = x;
}

void osk_bn_copy(osk_bn_t *r, const osk_bn_t *a)
{
	memcpy(r->v, a->v, sizeof(r->v));
}

void osk_bn_from_be(osk_bn_t *r, const uint8_t in[OSK_BN_BYTES])
{
	for (unsigned i = 0; i < OSK_BN_LIMBS; i++) {
		const uint8_t *p = in + 4u * (OSK_BN_LIMBS - 1u - i);

		r->v[i] = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) |
			  (uint32_t)p[3];
	}
}

void osk_bn_to_be(const osk_bn_t *a, uint8_t out[OSK_BN_BYTES])
{
	for (unsigned i = 0; i < OSK_BN_LIMBS; i++) {
		uint8_t *p = out + 4u * (OSK_BN_LIMBS - 1u - i);
		uint32_t x = a->v[i];

		p[0] = (uint8_t)((x >> 24) & 0xFFu);
		p[1] = (uint8_t)((x >> 16) & 0xFFu);
		p[2] = (uint8_t)((x >> 8) & 0xFFu);
		p[3] = (uint8_t)(x & 0xFFu);
	}
}

bool osk_bn_is_zero(const osk_bn_t *a)
{
	uint32_t any = 0;

	for (unsigned i = 0; i < OSK_BN_LIMBS; i++)
		any |= a->v[i];
	return any == 0;
}

bool osk_bn_is_odd(const osk_bn_t *a)
{
	return (a->v[0] & 1u) != 0;
}

bool osk_bn_bit(const osk_bn_t *a, unsigned index)
{
	if (index >= 32u * OSK_BN_LIMBS)
		return false;
	return ((a->v[index / 32u] >> (index % 32u)) & 1u) != 0;
}

int osk_bn_cmp(const osk_bn_t *a, const osk_bn_t *b)
{
	int result = 0;

	/* Von unten nach oben, ohne fruehe Rueckkehr: das hoechste
	 * unterschiedliche Limb schreibt zuletzt und gewinnt damit. */
	for (unsigned i = 0; i < OSK_BN_LIMBS; i++) {
		if (a->v[i] != b->v[i])
			result = (a->v[i] < b->v[i]) ? -1 : 1;
	}
	return result;
}

uint32_t osk_bn_add(osk_bn_t *r, const osk_bn_t *a, const osk_bn_t *b)
{
	uint64_t carry = 0;

	for (unsigned i = 0; i < OSK_BN_LIMBS; i++) {
		uint64_t sum = (uint64_t)a->v[i] + (uint64_t)b->v[i] + carry;

		r->v[i] = (uint32_t)(sum & 0xFFFFFFFFu);
		carry = sum >> 32;
	}
	return (uint32_t)carry;
}

uint32_t osk_bn_sub(osk_bn_t *r, const osk_bn_t *a, const osk_bn_t *b)
{
	uint64_t borrow = 0;

	for (unsigned i = 0; i < OSK_BN_LIMBS; i++) {
		uint64_t diff = (uint64_t)a->v[i] - (uint64_t)b->v[i] - borrow;

		r->v[i] = (uint32_t)(diff & 0xFFFFFFFFu);
		borrow = (diff >> 32) & 1u;
	}
	return (uint32_t)borrow;
}

/* Nimmt b, wenn take gesetzt ist - ohne Sprung. */
static void bn_select(osk_bn_t *r, const osk_bn_t *a, const osk_bn_t *b, uint32_t take)
{
	uint32_t mask = (uint32_t)0u - (take & 1u);

	for (unsigned i = 0; i < OSK_BN_LIMBS; i++)
		r->v[i] = (a->v[i] & ~mask) | (b->v[i] & mask);
}

void osk_bn_add_mod(osk_bn_t *r, const osk_bn_t *a, const osk_bn_t *b, const osk_bn_t *m)
{
	osk_bn_t sum, reduced;
	uint32_t carry, borrow;

	carry = osk_bn_add(&sum, a, b);
	borrow = osk_bn_sub(&reduced, &sum, m);
	/* Abziehen, wenn die Summe ueberlief ODER ohne Borger blieb. */
	bn_select(r, &sum, &reduced, carry | (borrow ^ 1u));
	osk_bn_wipe(&sum);
	osk_bn_wipe(&reduced);
}

void osk_bn_sub_mod(osk_bn_t *r, const osk_bn_t *a, const osk_bn_t *b, const osk_bn_t *m)
{
	osk_bn_t diff, fixed;
	uint32_t borrow;

	borrow = osk_bn_sub(&diff, a, b);
	(void)osk_bn_add(&fixed, &diff, m);
	bn_select(r, &diff, &fixed, borrow);
	osk_bn_wipe(&diff);
	osk_bn_wipe(&fixed);
}

void osk_bn_mul(osk_bn2_t *r, const osk_bn_t *a, const osk_bn_t *b)
{
	uint32_t acc[2 * OSK_BN_LIMBS];

	memset(acc, 0, sizeof(acc));
	for (unsigned i = 0; i < OSK_BN_LIMBS; i++) {
		uint64_t carry = 0;

		for (unsigned j = 0; j < OSK_BN_LIMBS; j++) {
			uint64_t cur =
			    (uint64_t)a->v[i] * (uint64_t)b->v[j] + (uint64_t)acc[i + j] + carry;

			acc[i + j] = (uint32_t)(cur & 0xFFFFFFFFu);
			carry = cur >> 32;
		}
		acc[i + OSK_BN_LIMBS] = (uint32_t)carry;
	}
	memcpy(r->v, acc, sizeof(acc));
	osk_wipe(acc, sizeof(acc));
}

/* r = r*2 + bit, alles mod m. r bleibt immer kleiner als m. */
static void bn_shift_in(osk_bn_t *r, uint32_t bit, const osk_bn_t *m)
{
	osk_bn_t doubled, reduced;
	uint32_t carry = 0, borrow;

	for (unsigned i = 0; i < OSK_BN_LIMBS; i++) {
		uint32_t next = r->v[i] >> 31;

		doubled.v[i] = (uint32_t)((r->v[i] << 1) | carry);
		carry = next;
	}
	doubled.v[0] |= (bit & 1u);
	borrow = osk_bn_sub(&reduced, &doubled, m);
	bn_select(r, &doubled, &reduced, carry | (borrow ^ 1u));
	osk_bn_wipe(&doubled);
	osk_bn_wipe(&reduced);
}

void osk_bn_mod2(osk_bn_t *r, const osk_bn2_t *a, const osk_bn_t *m)
{
	osk_bn_t acc;

	osk_bn_zero(&acc);
	for (unsigned i = 2u * 32u * OSK_BN_LIMBS; i > 0; i--) {
		unsigned index = i - 1u;
		uint32_t bit = (a->v[index / 32u] >> (index % 32u)) & 1u;

		bn_shift_in(&acc, bit, m);
	}
	osk_bn_copy(r, &acc);
	osk_bn_wipe(&acc);
}

void osk_bn_mul_mod(osk_bn_t *r, const osk_bn_t *a, const osk_bn_t *b, const osk_bn_t *m)
{
	osk_bn2_t wide;

	osk_bn_mul(&wide, a, b);
	osk_bn_mod2(r, &wide, m);
	osk_wipe(&wide, sizeof(wide));
}

void osk_bn_inv_mod(osk_bn_t *r, const osk_bn_t *a, const osk_bn_t *m)
{
	osk_bn_t exponent, base, acc, two;

	if (osk_bn_is_zero(a)) {
		osk_bn_zero(r);
		return;
	}
	osk_bn_set_u32(&two, 2);
	(void)osk_bn_sub(&exponent, m, &two); /* m - 2; m ist prim und > 2 */
	osk_bn_copy(&base, a);
	osk_bn_set_u32(&acc, 1);
	for (unsigned i = 0; i < 32u * OSK_BN_LIMBS; i++) {
		if (osk_bn_bit(&exponent, i))
			osk_bn_mul_mod(&acc, &acc, &base, m);
		osk_bn_mul_mod(&base, &base, &base, m);
	}
	osk_bn_copy(r, &acc);
	osk_bn_wipe(&exponent);
	osk_bn_wipe(&base);
	osk_bn_wipe(&acc);
}

void osk_bn_wipe(osk_bn_t *a)
{
	osk_wipe(a->v, sizeof(a->v));
}
