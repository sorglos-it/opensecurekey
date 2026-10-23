/* SHA-256, HMAC, HKDF, AES-256-CBC. Siehe osk_crypto.h.
 * Geschrieben nach FIPS 180-4, FIPS 197, RFC 2104 und RFC 5869. */
#include <string.h>
#include "osk_crypto.h"

/* ---- SHA-256 ----------------------------------------------------------- */

static const uint32_t K256[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u,
    0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu,
    0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu,
    0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau, 0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu,
    0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
    0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u, 0x19a4c116u,
    0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u,
    0xc67178f2u,
};

static uint32_t ror32(uint32_t x, unsigned n)
{
	return (x >> n) | (x << (32 - n));
}

static void sha256_block(osk_sha256_t *c, const uint8_t *p)
{
	uint32_t w[64];

	for (unsigned i = 0; i < 16; i++)
		w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16) |
		       ((uint32_t)p[i * 4 + 2] << 8) | (uint32_t)p[i * 4 + 3];
	for (unsigned i = 16; i < 64; i++) {
		const uint32_t s0 = ror32(w[i - 15], 7) ^ ror32(w[i - 15], 18) ^ (w[i - 15] >> 3);
		const uint32_t s1 = ror32(w[i - 2], 17) ^ ror32(w[i - 2], 19) ^ (w[i - 2] >> 10);
		w[i] = w[i - 16] + s0 + w[i - 7] + s1;
	}

	uint32_t a = c->h[0], b = c->h[1], cc = c->h[2], d = c->h[3];
	uint32_t e = c->h[4], f = c->h[5], g = c->h[6], h = c->h[7];

	for (unsigned i = 0; i < 64; i++) {
		const uint32_t S1 = ror32(e, 6) ^ ror32(e, 11) ^ ror32(e, 25);
		const uint32_t ch = (e & f) ^ (~e & g);
		const uint32_t t1 = h + S1 + ch + K256[i] + w[i];
		const uint32_t S0 = ror32(a, 2) ^ ror32(a, 13) ^ ror32(a, 22);
		const uint32_t mj = (a & b) ^ (a & cc) ^ (b & cc);
		const uint32_t t2 = S0 + mj;
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

void osk_sha256_init(osk_sha256_t *c)
{
	c->h[0] = 0x6a09e667u;
	c->h[1] = 0xbb67ae85u;
	c->h[2] = 0x3c6ef372u;
	c->h[3] = 0xa54ff53au;
	c->h[4] = 0x510e527fu;
	c->h[5] = 0x9b05688cu;
	c->h[6] = 0x1f83d9abu;
	c->h[7] = 0x5be0cd19u;
	c->block_len = 0;
	c->total = 0;
}

void osk_sha256_update(osk_sha256_t *c, const uint8_t *p, size_t len)
{
	if (len == 0 || p == NULL)
		return;
	c->total += (uint64_t)len;

	if (c->block_len > 0) {
		const size_t take = (64 - c->block_len < len) ? 64 - c->block_len : len;
		memcpy(c->block + c->block_len, p, take);
		c->block_len += take;
		p += take;
		len -= take;
		if (c->block_len == 64) {
			sha256_block(c, c->block);
			c->block_len = 0;
		}
	}
	while (len >= 64) {
		sha256_block(c, p);
		p += 64;
		len -= 64;
	}
	if (len > 0) {
		memcpy(c->block, p, len);
		c->block_len = len;
	}
}

void osk_sha256_final(osk_sha256_t *c, uint8_t out[OSK_SHA256_LEN])
{
	const uint64_t bits = c->total * 8u;
	uint8_t pad[72];
	size_t pad_len;

	pad[0] = 0x80;
	/* so viele Nullen, dass 56 Bytes mod 64 erreicht sind */
	pad_len = 1 + ((c->block_len < 56) ? (56 - c->block_len - 1) : (120 - c->block_len - 1));
	memset(pad + 1, 0, pad_len - 1);
	for (unsigned i = 0; i < 8; i++)
		pad[pad_len + i] = (uint8_t)(bits >> (56 - 8 * i));
	osk_sha256_update(c, pad, pad_len + 8);

	for (unsigned i = 0; i < 8; i++) {
		out[i * 4] = (uint8_t)(c->h[i] >> 24);
		out[i * 4 + 1] = (uint8_t)(c->h[i] >> 16);
		out[i * 4 + 2] = (uint8_t)(c->h[i] >> 8);
		out[i * 4 + 3] = (uint8_t)c->h[i];
	}
	osk_wipe(c, sizeof(*c));
}

void osk_sha256(const uint8_t *p, size_t len, uint8_t out[OSK_SHA256_LEN])
{
	osk_sha256_t c;
	osk_sha256_init(&c);
	osk_sha256_update(&c, p, len);
	osk_sha256_final(&c, out);
}

/* ---- HMAC -------------------------------------------------------------- */

void osk_hmac_sha256_2(const uint8_t *key, size_t key_len, const uint8_t *a, size_t a_len,
		       const uint8_t *b, size_t b_len, uint8_t out[OSK_SHA256_LEN])
{
	uint8_t k[64];
	uint8_t pad[64];
	uint8_t inner[OSK_SHA256_LEN];
	osk_sha256_t c;

	memset(k, 0, sizeof(k));
	if (key_len > 64)
		osk_sha256(key, key_len, k);
	else if (key_len > 0)
		memcpy(k, key, key_len);

	for (unsigned i = 0; i < 64; i++)
		pad[i] = (uint8_t)(k[i] ^ 0x36);
	osk_sha256_init(&c);
	osk_sha256_update(&c, pad, 64);
	osk_sha256_update(&c, a, a_len);
	osk_sha256_update(&c, b, b_len);
	osk_sha256_final(&c, inner);

	for (unsigned i = 0; i < 64; i++)
		pad[i] = (uint8_t)(k[i] ^ 0x5c);
	osk_sha256_init(&c);
	osk_sha256_update(&c, pad, 64);
	osk_sha256_update(&c, inner, sizeof(inner));
	osk_sha256_final(&c, out);

	osk_wipe(k, sizeof(k));
	osk_wipe(pad, sizeof(pad));
	osk_wipe(inner, sizeof(inner));
}

void osk_hmac_sha256(const uint8_t *key, size_t key_len, const uint8_t *msg, size_t msg_len,
		     uint8_t out[OSK_SHA256_LEN])
{
	osk_hmac_sha256_2(key, key_len, msg, msg_len, NULL, 0, out);
}

/* ---- HKDF -------------------------------------------------------------- */

osk_result_t osk_hkdf_sha256(const uint8_t *salt, size_t salt_len, const uint8_t *ikm,
			     size_t ikm_len, const uint8_t *info, size_t info_len, uint8_t *out,
			     size_t out_len)
{
	uint8_t prk[OSK_SHA256_LEN];
	uint8_t t[OSK_SHA256_LEN];
	size_t t_len = 0;
	uint8_t counter = 1;

	if (out_len == 0 || out_len > 255u * OSK_SHA256_LEN || out == NULL)
		return OSK_ERR_ARG;

	osk_hmac_sha256(salt, salt_len, ikm, ikm_len, prk);

	size_t done = 0;
	while (done < out_len) {
		uint8_t k[64];
		uint8_t pad[64];
		uint8_t inner[OSK_SHA256_LEN];
		osk_sha256_t c;

		/* T(n) = HMAC(PRK, T(n-1) || info || n) */
		memset(k, 0, sizeof(k));
		memcpy(k, prk, sizeof(prk));
		for (unsigned i = 0; i < 64; i++)
			pad[i] = (uint8_t)(k[i] ^ 0x36);
		osk_sha256_init(&c);
		osk_sha256_update(&c, pad, 64);
		osk_sha256_update(&c, t, t_len);
		osk_sha256_update(&c, info, info_len);
		osk_sha256_update(&c, &counter, 1);
		osk_sha256_final(&c, inner);
		for (unsigned i = 0; i < 64; i++)
			pad[i] = (uint8_t)(k[i] ^ 0x5c);
		osk_sha256_init(&c);
		osk_sha256_update(&c, pad, 64);
		osk_sha256_update(&c, inner, sizeof(inner));
		osk_sha256_final(&c, t);
		t_len = sizeof(t);

		const size_t take = (out_len - done < t_len) ? out_len - done : t_len;
		memcpy(out + done, t, take);
		done += take;
		counter++;

		osk_wipe(k, sizeof(k));
		osk_wipe(pad, sizeof(pad));
		osk_wipe(inner, sizeof(inner));
	}

	osk_wipe(prk, sizeof(prk));
	osk_wipe(t, sizeof(t));
	return OSK_OK;
}

/* ---- AES-256 ----------------------------------------------------------- */

static const uint8_t SBOX[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16,
};

static uint8_t RSBOX[256];
static bool rsbox_ready;

static void rsbox_build(void)
{
	if (rsbox_ready)
		return;
	for (unsigned i = 0; i < 256; i++)
		RSBOX[SBOX[i]] = (uint8_t)i;
	rsbox_ready = true;
}

static uint8_t xtime(uint8_t x)
{
	return (uint8_t)((x << 1) ^ ((x & 0x80) ? 0x1b : 0x00));
}

static uint8_t gmul(uint8_t a, uint8_t b)
{
	uint8_t r = 0;
	for (unsigned i = 0; i < 8; i++) {
		if (b & 1)
			r ^= a;
		a = xtime(a);
		b >>= 1;
	}
	return r;
}

/* AES-256: 14 Runden, 60 Wortschluessel. */
static void key_expand(const uint8_t key[32], uint8_t rk[240])
{
	static const uint8_t RCON[7] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40};
	memcpy(rk, key, 32);
	unsigned rcon_i = 0;

	for (unsigned i = 8; i < 60; i++) {
		uint8_t t[4];
		memcpy(t, rk + (i - 1) * 4, 4);
		if (i % 8 == 0) {
			const uint8_t tmp = t[0];
			t[0] = (uint8_t)(SBOX[t[1]] ^ RCON[rcon_i++]);
			t[1] = SBOX[t[2]];
			t[2] = SBOX[t[3]];
			t[3] = SBOX[tmp];
		} else if (i % 8 == 4) {
			for (unsigned j = 0; j < 4; j++)
				t[j] = SBOX[t[j]];
		}
		for (unsigned j = 0; j < 4; j++)
			rk[i * 4 + j] = (uint8_t)(rk[(i - 8) * 4 + j] ^ t[j]);
	}
}

static void add_round_key(uint8_t s[16], const uint8_t *rk)
{
	for (unsigned i = 0; i < 16; i++)
		s[i] ^= rk[i];
}

static void encrypt_block(const uint8_t rk[240], uint8_t s[16])
{
	add_round_key(s, rk);
	for (unsigned round = 1; round <= 14; round++) {
		uint8_t t[16];
		/* SubBytes und ShiftRows in einem Durchgang, spaltenweise */
		for (unsigned c = 0; c < 4; c++)
			for (unsigned r = 0; r < 4; r++)
				t[c * 4 + r] = SBOX[s[((c + r) % 4) * 4 + r]];
		if (round < 14) {
			for (unsigned c = 0; c < 4; c++) {
				const uint8_t a0 = t[c * 4], a1 = t[c * 4 + 1];
				const uint8_t a2 = t[c * 4 + 2], a3 = t[c * 4 + 3];
				s[c * 4] =
				    (uint8_t)(xtime(a0) ^ (uint8_t)(xtime(a1) ^ a1) ^ a2 ^ a3);
				s[c * 4 + 1] =
				    (uint8_t)(a0 ^ xtime(a1) ^ (uint8_t)(xtime(a2) ^ a2) ^ a3);
				s[c * 4 + 2] =
				    (uint8_t)(a0 ^ a1 ^ xtime(a2) ^ (uint8_t)(xtime(a3) ^ a3));
				s[c * 4 + 3] =
				    (uint8_t)((uint8_t)(xtime(a0) ^ a0) ^ a1 ^ a2 ^ xtime(a3));
			}
		} else {
			memcpy(s, t, 16);
		}
		add_round_key(s, rk + round * 16);
		osk_wipe(t, sizeof(t));
	}
}

static void decrypt_block(const uint8_t rk[240], uint8_t s[16])
{
	rsbox_build();
	add_round_key(s, rk + 14 * 16);
	for (unsigned round = 14; round >= 1; round--) {
		uint8_t t[16];
		/* InvShiftRows und InvSubBytes */
		for (unsigned c = 0; c < 4; c++)
			for (unsigned r = 0; r < 4; r++)
				t[((c + r) % 4) * 4 + r] = RSBOX[s[c * 4 + r]];
		add_round_key(t, rk + (round - 1) * 16);
		if (round > 1) {
			for (unsigned c = 0; c < 4; c++) {
				const uint8_t a0 = t[c * 4], a1 = t[c * 4 + 1];
				const uint8_t a2 = t[c * 4 + 2], a3 = t[c * 4 + 3];
				s[c * 4] = (uint8_t)(gmul(a0, 14) ^ gmul(a1, 11) ^ gmul(a2, 13) ^
						     gmul(a3, 9));
				s[c * 4 + 1] = (uint8_t)(gmul(a0, 9) ^ gmul(a1, 14) ^ gmul(a2, 11) ^
							 gmul(a3, 13));
				s[c * 4 + 2] = (uint8_t)(gmul(a0, 13) ^ gmul(a1, 9) ^ gmul(a2, 14) ^
							 gmul(a3, 11));
				s[c * 4 + 3] = (uint8_t)(gmul(a0, 11) ^ gmul(a1, 13) ^ gmul(a2, 9) ^
							 gmul(a3, 14));
			}
		} else {
			memcpy(s, t, 16);
		}
		osk_wipe(t, sizeof(t));
	}
}

osk_result_t osk_aes256_cbc_encrypt(const uint8_t key[32], const uint8_t iv[OSK_AES_BLOCK],
				    const uint8_t *in, uint8_t *out, size_t len)
{
	uint8_t rk[240];
	uint8_t prev[OSK_AES_BLOCK];

	if (!key || !iv || (!in && len) || (!out && len) || (len % OSK_AES_BLOCK) != 0)
		return OSK_ERR_ARG;

	key_expand(key, rk);
	memcpy(prev, iv, OSK_AES_BLOCK);

	for (size_t off = 0; off < len; off += OSK_AES_BLOCK) {
		uint8_t blk[OSK_AES_BLOCK];
		for (unsigned i = 0; i < OSK_AES_BLOCK; i++)
			blk[i] = (uint8_t)(in[off + i] ^ prev[i]);
		encrypt_block(rk, blk);
		memcpy(out + off, blk, OSK_AES_BLOCK);
		memcpy(prev, blk, OSK_AES_BLOCK);
		osk_wipe(blk, sizeof(blk));
	}

	osk_wipe(rk, sizeof(rk));
	osk_wipe(prev, sizeof(prev));
	return OSK_OK;
}

osk_result_t osk_aes256_cbc_decrypt(const uint8_t key[32], const uint8_t iv[OSK_AES_BLOCK],
				    const uint8_t *in, uint8_t *out, size_t len)
{
	uint8_t rk[240];
	uint8_t prev[OSK_AES_BLOCK];

	if (!key || !iv || (!in && len) || (!out && len) || (len % OSK_AES_BLOCK) != 0)
		return OSK_ERR_ARG;

	key_expand(key, rk);
	memcpy(prev, iv, OSK_AES_BLOCK);

	for (size_t off = 0; off < len; off += OSK_AES_BLOCK) {
		uint8_t blk[OSK_AES_BLOCK];
		uint8_t cipher[OSK_AES_BLOCK];
		memcpy(cipher, in + off, OSK_AES_BLOCK);
		memcpy(blk, cipher, OSK_AES_BLOCK);
		decrypt_block(rk, blk);
		for (unsigned i = 0; i < OSK_AES_BLOCK; i++)
			out[off + i] = (uint8_t)(blk[i] ^ prev[i]);
		memcpy(prev, cipher, OSK_AES_BLOCK);
		osk_wipe(blk, sizeof(blk));
		osk_wipe(cipher, sizeof(cipher));
	}

	osk_wipe(rk, sizeof(rk));
	osk_wipe(prev, sizeof(prev));
	return OSK_OK;
}

/* ---- Kleinteile -------------------------------------------------------- */

bool osk_memeq(const void *a, const void *b, size_t len)
{
	const uint8_t *x = (const uint8_t *)a;
	const uint8_t *y = (const uint8_t *)b;
	uint8_t diff = 0;

	for (size_t i = 0; i < len; i++)
		diff |= (uint8_t)(x[i] ^ y[i]);
	return diff == 0;
}

void osk_wipe(void *p, size_t len)
{
	volatile uint8_t *q = (volatile uint8_t *)p;
	while (len--)
		*q++ = 0;
}
