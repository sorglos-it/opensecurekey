/* Umsetzung der Rechenbausteine aus wallet_hash.h.
 *
 * Selbst geschrieben gegen FIPS 180-4 (SHA-512), FIPS 202 (Keccak-f[1600]),
 * RFC 2104 (HMAC), RFC 8018 (PBKDF2) und die RIPEMD-160-Veroeffentlichung.
 * Kein uebernommener Code, keine uebernommenen Namen. */

#include <string.h>
#include "wallet_hash.h"
#include "osk_crypto.h"

/* --- SHA-512 ------------------------------------------------------------- */

static const uint64_t SHA512_K[80] = {
    0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
    0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
    0xd807aa98a3030242ULL, 0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
    0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
    0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL, 0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
    0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
    0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL,
    0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL, 0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
    0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
    0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
    0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL, 0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL,
    0xd192e819d6ef5218ULL, 0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
    0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL,
    0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL, 0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL,
    0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
    0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
    0xca273eceea26619cULL, 0xd186b8c721c0c207ULL, 0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL,
    0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
    0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL,
    0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL, 0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL,
};

static uint64_t ror64(uint64_t x, unsigned n)
{
	return (x >> n) | (x << (64u - n));
}

static uint64_t load_be64(const uint8_t *p)
{
	uint64_t v = 0;
	for (unsigned i = 0; i < 8; i++)
		v = (v << 8) | p[i];
	return v;
}

static void store_be64(uint8_t *p, uint64_t v)
{
	for (unsigned i = 0; i < 8; i++)
		p[i] = (uint8_t)((v >> (56u - 8u * i)) & 0xFFu);
}

static void sha512_block(osk_sha512_t *c, const uint8_t *block)
{
	uint64_t w[80];
	uint64_t a, b, cc, d, e, f, g, h;

	for (unsigned i = 0; i < 16; i++)
		w[i] = load_be64(block + 8u * i);
	for (unsigned i = 16; i < 80; i++) {
		uint64_t s0 = ror64(w[i - 15], 1) ^ ror64(w[i - 15], 8) ^ (w[i - 15] >> 7);
		uint64_t s1 = ror64(w[i - 2], 19) ^ ror64(w[i - 2], 61) ^ (w[i - 2] >> 6);
		w[i] = w[i - 16] + s0 + w[i - 7] + s1;
	}
	a = c->h[0];
	b = c->h[1];
	cc = c->h[2];
	d = c->h[3];
	e = c->h[4];
	f = c->h[5];
	g = c->h[6];
	h = c->h[7];
	for (unsigned i = 0; i < 80; i++) {
		uint64_t s1 = ror64(e, 14) ^ ror64(e, 18) ^ ror64(e, 41);
		uint64_t ch = (e & f) ^ ((~e) & g);
		uint64_t t1 = h + s1 + ch + SHA512_K[i] + w[i];
		uint64_t s0 = ror64(a, 28) ^ ror64(a, 34) ^ ror64(a, 39);
		uint64_t maj = (a & b) ^ (a & cc) ^ (b & cc);
		uint64_t t2 = s0 + maj;

		h = g;
		g = f;
		f = e;
		e = d + t1;
		d = cc;
		cc = b;
		b = a;
		a = t1 + t2;
	}
	c->h[0] += a;
	c->h[1] += b;
	c->h[2] += cc;
	c->h[3] += d;
	c->h[4] += e;
	c->h[5] += f;
	c->h[6] += g;
	c->h[7] += h;
	osk_wipe(w, sizeof(w));
}

void osk_sha512_init(osk_sha512_t *c)
{
	c->h[0] = 0x6a09e667f3bcc908ULL;
	c->h[1] = 0xbb67ae8584caa73bULL;
	c->h[2] = 0x3c6ef372fe94f82bULL;
	c->h[3] = 0xa54ff53a5f1d36f1ULL;
	c->h[4] = 0x510e527fade682d1ULL;
	c->h[5] = 0x9b05688c2b3e6c1fULL;
	c->h[6] = 0x1f83d9abfb41bd6bULL;
	c->h[7] = 0x5be0cd19137e2179ULL;
	c->block_len = 0;
	c->total_lo = 0;
	memset(c->block, 0, sizeof(c->block));
}

void osk_sha512_update(osk_sha512_t *c, const uint8_t *p, size_t len)
{
	if (len == 0)
		return;
	c->total_lo += (uint64_t)len;
	if (c->block_len > 0) {
		size_t need = 128u - c->block_len;
		size_t take = len < need ? len : need;

		memcpy(c->block + c->block_len, p, take);
		c->block_len += take;
		p += take;
		len -= take;
		if (c->block_len < 128u)
			return;
		sha512_block(c, c->block);
		c->block_len = 0;
	}
	while (len >= 128u) {
		sha512_block(c, p);
		p += 128u;
		len -= 128u;
	}
	if (len > 0) {
		memcpy(c->block, p, len);
		c->block_len = len;
	}
}

void osk_sha512_final(osk_sha512_t *c, uint8_t out[OSK_SHA512_LEN])
{
	uint64_t bits = c->total_lo << 3;
	uint8_t pad = 0x80;

	osk_sha512_update(c, &pad, 1);
	/* Laengenfeld ist 128 Bit breit; die oberen 64 Bit sind hier immer 0. */
	while (c->block_len != 112u) {
		uint8_t zero = 0;

		osk_sha512_update(c, &zero, 1);
	}
	memset(c->block + 112, 0, 8);
	store_be64(c->block + 120, bits);
	sha512_block(c, c->block);
	for (unsigned i = 0; i < 8; i++)
		store_be64(out + 8u * i, c->h[i]);
	osk_wipe(c, sizeof(*c));
}

void osk_sha512(const uint8_t *p, size_t len, uint8_t out[OSK_SHA512_LEN])
{
	osk_sha512_t c;

	osk_sha512_init(&c);
	osk_sha512_update(&c, p, len);
	osk_sha512_final(&c, out);
}

/* --- HMAC-SHA512 --------------------------------------------------------- */

static void hmac512_start(osk_sha512_t *inner, uint8_t opad[128], const uint8_t *key,
			  size_t key_len)
{
	uint8_t ipad[128];
	uint8_t short_key[OSK_SHA512_LEN];

	if (key_len > 128u) {
		osk_sha512(key, key_len, short_key);
		key = short_key;
		key_len = OSK_SHA512_LEN;
	}
	memset(ipad, 0x36, sizeof(ipad));
	memset(opad, 0x5c, 128);
	for (size_t i = 0; i < key_len; i++) {
		ipad[i] = (uint8_t)(ipad[i] ^ key[i]);
		opad[i] = (uint8_t)(opad[i] ^ key[i]);
	}
	osk_sha512_init(inner);
	osk_sha512_update(inner, ipad, sizeof(ipad));
	osk_wipe(ipad, sizeof(ipad));
	osk_wipe(short_key, sizeof(short_key));
}

static void hmac512_finish(osk_sha512_t *inner, uint8_t opad[128], uint8_t out[OSK_SHA512_LEN])
{
	uint8_t mid[OSK_SHA512_LEN];
	osk_sha512_t outer;

	osk_sha512_final(inner, mid);
	osk_sha512_init(&outer);
	osk_sha512_update(&outer, opad, 128);
	osk_sha512_update(&outer, mid, sizeof(mid));
	osk_sha512_final(&outer, out);
	osk_wipe(mid, sizeof(mid));
	osk_wipe(opad, 128);
}

void osk_hmac_sha512_2(const uint8_t *key, size_t key_len, const uint8_t *a, size_t a_len,
		       const uint8_t *b, size_t b_len, uint8_t out[OSK_SHA512_LEN])
{
	osk_sha512_t inner;
	uint8_t opad[128];

	hmac512_start(&inner, opad, key, key_len);
	if (a_len > 0)
		osk_sha512_update(&inner, a, a_len);
	if (b_len > 0)
		osk_sha512_update(&inner, b, b_len);
	hmac512_finish(&inner, opad, out);
}

void osk_hmac_sha512(const uint8_t *key, size_t key_len, const uint8_t *msg, size_t msg_len,
		     uint8_t out[OSK_SHA512_LEN])
{
	osk_hmac_sha512_2(key, key_len, msg, msg_len, NULL, 0, out);
}

/* --- PBKDF2-HMAC-SHA512 -------------------------------------------------- */

osk_result_t osk_pbkdf2_sha512(const uint8_t *password, size_t password_len, const uint8_t *salt,
			       size_t salt_len, uint32_t rounds, uint8_t *out, size_t out_len)
{
	uint8_t u[OSK_SHA512_LEN];
	uint8_t acc[OSK_SHA512_LEN];
	uint8_t counter[4] = {0, 0, 0, 1};

	if (out == NULL || out_len == 0 || out_len > OSK_SHA512_LEN || rounds == 0)
		return OSK_ERR_ARG;
	if (password == NULL && password_len > 0)
		return OSK_ERR_ARG;
	if (salt == NULL && salt_len > 0)
		return OSK_ERR_ARG;

	/* Nur ein Block: mehr als 64 Byte gibt diese Fassung nicht her, und
	 * BIP39 braucht genau 64. Der Blockzaehler ist deshalb fest 1. */
	osk_hmac_sha512_2(password, password_len, salt, salt_len, counter, sizeof(counter), u);
	memcpy(acc, u, sizeof(acc));
	for (uint32_t i = 1; i < rounds; i++) {
		osk_hmac_sha512(password, password_len, u, sizeof(u), u);
		for (size_t j = 0; j < sizeof(acc); j++)
			acc[j] = (uint8_t)(acc[j] ^ u[j]);
	}
	memcpy(out, acc, out_len);
	osk_wipe(u, sizeof(u));
	osk_wipe(acc, sizeof(acc));
	return OSK_OK;
}

/* --- RIPEMD-160 ---------------------------------------------------------- */

static uint32_t rol32(uint32_t x, unsigned n)
{
	return n == 0 ? x : (uint32_t)((x << n) | (x >> (32u - n)));
}

static const uint8_t RMD_RL[80] = {
    0,	1, 2,  3, 4,  5,  6, 7,	 8, 9,	10, 11, 12, 13, 14, 15, 7,  4,	13, 1,
    10, 6, 15, 3, 12, 0,  9, 5,	 2, 14, 11, 8,	3,  10, 14, 4,	9,  15, 8,  1,
    2,	7, 0,  6, 13, 11, 5, 12, 1, 9,	11, 10, 0,  8,	12, 4,	13, 3,	7,  15,
    14, 5, 6,  2, 4,  0,  5, 9,	 7, 12, 2,  10, 14, 1,	3,  8,	11, 6,	15, 13,
};
static const uint8_t RMD_RR[80] = {
    5,	14, 7,	0,  9,	2,  11, 4,  13, 6, 15, 8, 1,  10, 3,  12, 6, 11, 3, 7,
    0,	13, 5,	10, 14, 15, 8,	12, 4,	9, 1,  2, 15, 5,  1,  3,  7, 14, 6, 9,
    11, 8,  12, 2,  10, 0,  4,	13, 8,	6, 4,  1, 3,  11, 15, 0,  5, 12, 2, 13,
    9,	7,  10, 14, 12, 15, 10, 4,  1,	5, 8,  7, 6,  2,  13, 14, 0, 3,	 9, 11,
};
static const uint8_t RMD_SL[80] = {
    11, 14, 15, 12, 5, 8,  7,  9,  11, 13, 14, 15, 6,  7,  9,  8,  7,  6,  8,  13,
    11, 9,  7,	15, 7, 12, 15, 9,  11, 7,  13, 12, 11, 13, 6,  7,  14, 9,  13, 15,
    14, 8,  13, 6,  5, 12, 7,  5,  11, 12, 14, 15, 14, 15, 9,  8,  9,  14, 5,  6,
    8,	6,  5,	12, 9, 15, 5,  11, 6,  8,  13, 12, 5,  12, 13, 14, 11, 8,  5,  6,
};
static const uint8_t RMD_SR[80] = {
    8,	9,  9,	11, 13, 15, 15, 5, 7,  7,  8,  11, 14, 14, 12, 6,  9,  13, 15, 7,
    12, 8,  9,	11, 7,	7,  12, 7, 6,  15, 13, 11, 9,  7,  15, 11, 8,  6,  6,  14,
    12, 13, 5,	14, 13, 13, 7,	5, 15, 5,  8,  11, 14, 14, 6,  14, 6,  9,  12, 9,
    12, 5,  15, 8,  8,	5,  12, 9, 12, 5,  14, 6,  8,  13, 6,  5,  15, 13, 11, 11,
};
static const uint32_t RMD_KL[5] = {0x00000000u, 0x5a827999u, 0x6ed9eba1u, 0x8f1bbcdcu, 0xa953fd4eu};
static const uint32_t RMD_KR[5] = {0x50a28be6u, 0x5c4dd124u, 0x6d703ef3u, 0x7a6d76e9u, 0x00000000u};

static uint32_t rmd_f(unsigned round, uint32_t x, uint32_t y, uint32_t z)
{
	switch (round) {
	case 0:
		return x ^ y ^ z;
	case 1:
		return (x & y) | ((~x) & z);
	case 2:
		return (x | (~y)) ^ z;
	case 3:
		return (x & z) | (y & (~z));
	default:
		return x ^ (y | (~z));
	}
}

static uint32_t load_le32(const uint8_t *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
	       ((uint32_t)p[3] << 24);
}

static void store_le32(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)(v & 0xFFu);
	p[1] = (uint8_t)((v >> 8) & 0xFFu);
	p[2] = (uint8_t)((v >> 16) & 0xFFu);
	p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static void ripemd160_block(uint32_t h[5], const uint8_t *block)
{
	uint32_t w[16];
	uint32_t al = h[0], bl = h[1], cl = h[2], dl = h[3], el = h[4];
	uint32_t ar = h[0], br = h[1], cr = h[2], dr = h[3], er = h[4];

	for (unsigned i = 0; i < 16; i++)
		w[i] = load_le32(block + 4u * i);
	for (unsigned i = 0; i < 80; i++) {
		unsigned round = i / 16u;
		uint32_t t;

		t = rol32(al + rmd_f(round, bl, cl, dl) + w[RMD_RL[i]] + RMD_KL[round], RMD_SL[i]) +
		    el;
		al = el;
		el = dl;
		dl = rol32(cl, 10);
		cl = bl;
		bl = t;

		t = rol32(ar + rmd_f(4u - round, br, cr, dr) + w[RMD_RR[i]] + RMD_KR[round],
			  RMD_SR[i]) +
		    er;
		ar = er;
		er = dr;
		dr = rol32(cr, 10);
		cr = br;
		br = t;
	}
	{
		uint32_t t = h[1] + cl + dr;

		h[1] = h[2] + dl + er;
		h[2] = h[3] + el + ar;
		h[3] = h[4] + al + br;
		h[4] = h[0] + bl + cr;
		h[0] = t;
	}
	osk_wipe(w, sizeof(w));
}

void osk_ripemd160(const uint8_t *p, size_t len, uint8_t out[OSK_RIPEMD160_LEN])
{
	uint32_t h[5] = {0x67452301u, 0xefcdab89u, 0x98badcfeu, 0x10325476u, 0xc3d2e1f0u};
	uint8_t tail[128];
	uint64_t bits = (uint64_t)len << 3;
	size_t rest, pad_len;

	while (len >= 64u) {
		ripemd160_block(h, p);
		p += 64u;
		len -= 64u;
	}
	rest = len;
	memcpy(tail, p, rest);
	tail[rest] = 0x80;
	pad_len = (rest < 56u) ? 64u : 128u;
	memset(tail + rest + 1u, 0, pad_len - rest - 1u);
	store_le32(tail + pad_len - 8u, (uint32_t)(bits & 0xFFFFFFFFu));
	store_le32(tail + pad_len - 4u, (uint32_t)((bits >> 32) & 0xFFFFFFFFu));
	for (size_t off = 0; off < pad_len; off += 64u)
		ripemd160_block(h, tail + off);
	for (unsigned i = 0; i < 5; i++)
		store_le32(out + 4u * i, h[i]);
	osk_wipe(tail, sizeof(tail));
	osk_wipe(h, sizeof(h));
}

/* --- Keccak-256 (Ethereum, Auffuellbyte 0x01) ---------------------------- */

#define KECCAK_RATE 136u /* 1600 - 2*256 Bit, in Byte */

static const uint64_t KECCAK_RC[24] = {
    0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808aULL, 0x8000000080008000ULL,
    0x000000000000808bULL, 0x0000000080000001ULL, 0x8000000080008081ULL, 0x8000000000008009ULL,
    0x000000000000008aULL, 0x0000000000000088ULL, 0x0000000080008009ULL, 0x000000008000000aULL,
    0x000000008000808bULL, 0x800000000000008bULL, 0x8000000000008089ULL, 0x8000000000008003ULL,
    0x8000000000008002ULL, 0x8000000000000080ULL, 0x000000000000800aULL, 0x800000008000000aULL,
    0x8000000080008081ULL, 0x8000000000008080ULL, 0x0000000080000001ULL, 0x8000000080008008ULL,
};
static const unsigned KECCAK_ROT[25] = {0,  1,	62, 28, 27, 36, 44, 6,	55, 20, 3,  10, 43,
					25, 39, 41, 45, 15, 21, 8,  18, 2,  61, 56, 14};

static uint64_t rol64(uint64_t x, unsigned n)
{
	return n == 0 ? x : (uint64_t)((x << n) | (x >> (64u - n)));
}

static void keccak_f(uint64_t a[25])
{
	for (unsigned round = 0; round < 24; round++) {
		uint64_t c[5], d[5], b[25];

		for (unsigned x = 0; x < 5; x++)
			c[x] = a[x] ^ a[x + 5] ^ a[x + 10] ^ a[x + 15] ^ a[x + 20];
		for (unsigned x = 0; x < 5; x++)
			d[x] = c[(x + 4u) % 5u] ^ rol64(c[(x + 1u) % 5u], 1);
		for (unsigned x = 0; x < 5; x++)
			for (unsigned y = 0; y < 5; y++)
				a[x + 5u * y] ^= d[x];
		for (unsigned x = 0; x < 5; x++)
			for (unsigned y = 0; y < 5; y++)
				b[y + 5u * ((2u * x + 3u * y) % 5u)] =
				    rol64(a[x + 5u * y], KECCAK_ROT[x + 5u * y]);
		for (unsigned x = 0; x < 5; x++)
			for (unsigned y = 0; y < 5; y++)
				a[x + 5u * y] = b[x + 5u * y] ^ ((~b[((x + 1u) % 5u) + 5u * y]) &
								 b[((x + 2u) % 5u) + 5u * y]);
		a[0] ^= KECCAK_RC[round];
	}
}

static void keccak_absorb(uint64_t state[25], const uint8_t *block)
{
	for (unsigned i = 0; i < KECCAK_RATE / 8u; i++) {
		uint64_t lane = 0;

		for (unsigned j = 0; j < 8; j++)
			lane |= (uint64_t)block[8u * i + j] << (8u * j);
		state[i] ^= lane;
	}
	keccak_f(state);
}

void osk_keccak256(const uint8_t *p, size_t len, uint8_t out[OSK_KECCAK256_LEN])
{
	uint64_t state[25];
	uint8_t tail[KECCAK_RATE];
	size_t rest;

	memset(state, 0, sizeof(state));
	while (len >= KECCAK_RATE) {
		keccak_absorb(state, p);
		p += KECCAK_RATE;
		len -= KECCAK_RATE;
	}
	rest = len;
	memcpy(tail, p, rest);
	memset(tail + rest, 0, KECCAK_RATE - rest);
	tail[rest] = 0x01; /* Keccak der urspruenglichen Fassung, nicht SHA-3 */
	tail[KECCAK_RATE - 1u] = (uint8_t)(tail[KECCAK_RATE - 1u] | 0x80u);
	keccak_absorb(state, tail);
	for (unsigned i = 0; i < 4; i++)
		for (unsigned j = 0; j < 8; j++)
			out[8u * i + j] = (uint8_t)((state[i] >> (8u * j)) & 0xFFu);
	osk_wipe(state, sizeof(state));
	osk_wipe(tail, sizeof(tail));
}

/* --- Zusammengesetztes --------------------------------------------------- */

void osk_hash256(const uint8_t *p, size_t len, uint8_t out[32])
{
	uint8_t once[OSK_SHA256_LEN];

	osk_sha256(p, len, once);
	osk_sha256(once, sizeof(once), out);
	osk_wipe(once, sizeof(once));
}

void osk_hash160(const uint8_t *p, size_t len, uint8_t out[OSK_RIPEMD160_LEN])
{
	uint8_t once[OSK_SHA256_LEN];

	osk_sha256(p, len, once);
	osk_ripemd160(once, sizeof(once), out);
	osk_wipe(once, sizeof(once));
}
