/* Pruefungen der 256-Bit-Zahlen. Die Vergleichswerte sind mit einer
 * unabhaengigen Rechnung in Python entstanden - Pruefdaten, kein Code. */

#include "test_util.h"
#include "wallet_bn.h"
#include "wallet_ec.h"

static osk_bn_t from_hex(const char *hex)
{
	uint8_t raw[32];
	osk_bn_t out;

	memset(raw, 0, sizeof(raw));
	(void)t_unhex(hex, raw, sizeof(raw));
	osk_bn_from_be(&out, raw);
	return out;
}

static void expect(const char *what, const osk_bn_t *a, const char *hex)
{
	uint8_t raw[32];

	osk_bn_to_be(a, raw);
	T_HEX(what, raw, sizeof(raw), hex);
}

static void test_bytes(void)
{
	osk_bn_t a = from_hex("00112233445566778899aabbccddeeff"
			      "00112233445566778899aabbccddeeff");

	expect("hin und zurueck", &a,
	       "00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff");
	T_EQ("Limb 0", a.v[0], 0xccddeeffu);
	T_EQ("Limb 7", a.v[7], 0x00112233u);
}

static void test_add_sub(void)
{
	osk_bn_t a, b, r;
	uint32_t carry;

	osk_bn_set_u32(&a, 0xFFFFFFFFu);
	osk_bn_set_u32(&b, 1);
	carry = osk_bn_add(&r, &a, &b);
	T_EQ("kein Uebertrag", carry, 0);
	T_EQ("Uebertrag ins zweite Limb", r.v[1], 1);
	T_EQ("unteres Limb null", r.v[0], 0);

	a = from_hex("ffffffffffffffffffffffffffffffff"
		     "ffffffffffffffffffffffffffffffff");
	carry = osk_bn_add(&r, &a, &b);
	T_EQ("Uebertrag heraus", carry, 1);
	T_TRUE("Ergebnis null", osk_bn_is_zero(&r));

	osk_bn_set_u32(&a, 5);
	osk_bn_set_u32(&b, 7);
	T_EQ("Borger", osk_bn_sub(&r, &a, &b), 1);
	T_EQ("kein Borger", osk_bn_sub(&r, &b, &a), 0);
	T_EQ("7-5", r.v[0], 2);
}

static void test_cmp(void)
{
	osk_bn_t a, b;

	osk_bn_set_u32(&a, 1);
	osk_bn_set_u32(&b, 2);
	T_EQ("1 < 2", osk_bn_cmp(&a, &b), -1);
	T_EQ("2 > 1", osk_bn_cmp(&b, &a), 1);
	T_EQ("gleich", osk_bn_cmp(&a, &a), 0);
	a = from_hex("0000000000000000000000000000000000000000000000000000000100000000");
	b = from_hex("00000000000000000000000000000000000000000000000000000000ffffffff");
	T_EQ("hoeheres Limb gewinnt", osk_bn_cmp(&a, &b), 1);
}

static void test_mod_arith(void)
{
	const osk_bn_t *n = osk_ec_order();
	osk_bn_t a, b, r;

	/* (n-1) + 2 = 1 mod n */
	osk_bn_set_u32(&b, 1);
	(void)osk_bn_sub(&a, n, &b);
	osk_bn_set_u32(&b, 2);
	osk_bn_add_mod(&r, &a, &b, n);
	expect("(n-1)+2 mod n", &r,
	       "0000000000000000000000000000000000000000000000000000000000000001");

	/* 1 - 2 = n-1 mod n */
	osk_bn_set_u32(&a, 1);
	osk_bn_sub_mod(&r, &a, &b, n);
	expect("1-2 mod n", &r, "fffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364140");

	/* 2^128 * 2^128 = 2^256 = 2^256 - n + ... : gegen Python geprueft */
	a = from_hex("0000000000000000000000000000000100000000000000000000000000000000");
	osk_bn_mul_mod(&r, &a, &a, n);
	expect("2^256 mod n", &r,
	       "000000000000000000000000000000014551231950b75fc4402da1732fc9bebf");
}

static void test_inverse(void)
{
	const osk_bn_t *n = osk_ec_order();
	osk_bn_t a, inv, prod, zero;

	osk_bn_set_u32(&a, 3);
	osk_bn_inv_mod(&inv, &a, n);
	osk_bn_mul_mod(&prod, &a, &inv, n);
	expect("3 * 3^-1 = 1", &prod,
	       "0000000000000000000000000000000000000000000000000000000000000001");

	a = from_hex("18e14a7b6a307f426a94f8114701e7c8e774e7f9a47e2c2035db29a206321725");
	osk_bn_inv_mod(&inv, &a, n);
	osk_bn_mul_mod(&prod, &a, &inv, n);
	expect("grosser Kehrwert", &prod,
	       "0000000000000000000000000000000000000000000000000000000000000001");

	osk_bn_zero(&zero);
	osk_bn_inv_mod(&inv, &zero, n);
	T_TRUE("Kehrwert von 0 ist 0", osk_bn_is_zero(&inv));
}

static void test_bits(void)
{
	osk_bn_t a;

	osk_bn_set_u32(&a, 0x80000001u);
	T_TRUE("Bit 0", osk_bn_bit(&a, 0));
	T_TRUE("Bit 31", osk_bn_bit(&a, 31));
	T_TRUE("Bit 1 nicht", !osk_bn_bit(&a, 1));
	T_TRUE("Bit 255 nicht", !osk_bn_bit(&a, 255));
	T_TRUE("Bit jenseits ist falsch, kein Absturz", !osk_bn_bit(&a, 999));
	T_TRUE("ungerade", osk_bn_is_odd(&a));
	osk_bn_set_u32(&a, 2);
	T_TRUE("gerade", !osk_bn_is_odd(&a));
}

int main(void)
{
	test_bytes();
	test_add_sub();
	test_cmp();
	test_mod_arith();
	test_inverse();
	test_bits();
	return t_report("bn");
}
