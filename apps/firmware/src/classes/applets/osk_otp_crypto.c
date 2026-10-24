/* SHA-1, HMAC, AES-128, CRC-16 und ModHex. Siehe osk_otp_crypto.h. */
#include <string.h>
#include "osk_otp_crypto.h"

/* --- SHA-1 (FIPS 180-4 Abschnitt 6.1) ----------------------------------- */

static uint32_t rotl(uint32_t v, unsigned n)
{
	return (uint32_t)((v << n) | (v >> (32u - n)));
}

static void sha1_block(osk_sha1_t *c, const uint8_t *p)
{
	uint32_t w[80];
	uint32_t a = c->h[0];
	uint32_t b = c->h[1];
	uint32_t d = c->h[2];
	uint32_t e = c->h[3];
	uint32_t f = c->h[4];

	for (unsigned i = 0; i < 16u; i++)
		w[i] = ((uint32_t)p[i * 4u] << 24) | ((uint32_t)p[i * 4u + 1u] << 16) |
		       ((uint32_t)p[i * 4u + 2u] << 8) | (uint32_t)p[i * 4u + 3u];
	for (unsigned i = 16u; i < 80u; i++)
		w[i] = rotl(w[i - 3u] ^ w[i - 8u] ^ w[i - 14u] ^ w[i - 16u], 1u);

	for (unsigned i = 0; i < 80u; i++) {
		uint32_t fn;
		uint32_t k;

		if (i < 20u) {
			fn = (b & d) | ((~b) & e);
			k = 0x5a827999u;
		} else if (i < 40u) {
			fn = b ^ d ^ e;
			k = 0x6ed9eba1u;
		} else if (i < 60u) {
			fn = (b & d) | (b & e) | (d & e);
			k = 0x8f1bbcdcu;
		} else {
			fn = b ^ d ^ e;
			k = 0xca62c1d6u;
		}
		{
			const uint32_t tmp = rotl(a, 5u) + fn + f + k + w[i];

			f = e;
			e = d;
			d = rotl(b, 30u);
			b = a;
			a = tmp;
		}
	}
	c->h[0] += a;
	c->h[1] += b;
	c->h[2] += d;
	c->h[3] += e;
	c->h[4] += f;
}

void osk_sha1_init(osk_sha1_t *c)
{
	c->h[0] = 0x67452301u;
	c->h[1] = 0xefcdab89u;
	c->h[2] = 0x98badcfeu;
	c->h[3] = 0x10325476u;
	c->h[4] = 0xc3d2e1f0u;
	c->total = 0u;
	c->have = 0u;
	memset(c->buf, 0, sizeof(c->buf));
}

void osk_sha1_update(osk_sha1_t *c, const uint8_t *data, size_t len)
{
	if (data == NULL)
		return;
	c->total += len;
	while (len != 0u) {
		const size_t room = OSK_SHA1_BLOCK - c->have;
		const size_t take = (len < room) ? len : room;

		memcpy(c->buf + c->have, data, take);
		c->have += take;
		data += take;
		len -= take;
		if (c->have == OSK_SHA1_BLOCK) {
			sha1_block(c, c->buf);
			c->have = 0u;
		}
	}
}

void osk_sha1_final(osk_sha1_t *c, uint8_t out[OSK_SHA1_LEN])
{
	const uint64_t bits = c->total * 8u;
	uint8_t tail[8];

	osk_sha1_update(c, (const uint8_t *)"\x80", 1u);
	while (c->have != 56u) {
		static const uint8_t zero = 0u;

		osk_sha1_update(c, &zero, 1u);
	}
	for (unsigned i = 0; i < 8u; i++)
		tail[i] = (uint8_t)(bits >> (56u - 8u * i));
	osk_sha1_update(c, tail, sizeof(tail));

	for (unsigned i = 0; i < 5u; i++) {
		out[i * 4u] = (uint8_t)(c->h[i] >> 24);
		out[i * 4u + 1u] = (uint8_t)(c->h[i] >> 16);
		out[i * 4u + 2u] = (uint8_t)(c->h[i] >> 8);
		out[i * 4u + 3u] = (uint8_t)c->h[i];
	}
}

void osk_sha1(const uint8_t *data, size_t len, uint8_t out[OSK_SHA1_LEN])
{
	osk_sha1_t c;

	osk_sha1_init(&c);
	osk_sha1_update(&c, data, len);
	osk_sha1_final(&c, out);
}

/* --- HMAC (RFC 2104) ---------------------------------------------------- */

void osk_hmac_sha1(const uint8_t *key, size_t key_len, const uint8_t *msg, size_t msg_len,
		   uint8_t out[OSK_SHA1_LEN])
{
	uint8_t pad[OSK_SHA1_BLOCK];
	uint8_t inner[OSK_SHA1_LEN];
	uint8_t shortened[OSK_SHA1_LEN];
	osk_sha1_t c;

	if (key_len > OSK_SHA1_BLOCK) {
		osk_sha1(key, key_len, shortened);
		key = shortened;
		key_len = OSK_SHA1_LEN;
	}

	memset(pad, 0x36, sizeof(pad));
	for (size_t i = 0; i < key_len; i++)
		pad[i] = (uint8_t)(pad[i] ^ key[i]);
	osk_sha1_init(&c);
	osk_sha1_update(&c, pad, sizeof(pad));
	osk_sha1_update(&c, msg, msg_len);
	osk_sha1_final(&c, inner);

	memset(pad, 0x5c, sizeof(pad));
	for (size_t i = 0; i < key_len; i++)
		pad[i] = (uint8_t)(pad[i] ^ key[i]);
	osk_sha1_init(&c);
	osk_sha1_update(&c, pad, sizeof(pad));
	osk_sha1_update(&c, inner, sizeof(inner));
	osk_sha1_final(&c, out);

	memset(pad, 0, sizeof(pad));
	memset(inner, 0, sizeof(inner));
	memset(shortened, 0, sizeof(shortened));
	memset(&c, 0, sizeof(c));
}

/* --- AES-128 (FIPS 197) ------------------------------------------------- */

/* Die Ersetzungstabelle ist der Kehrwert im Koerper GF(2^8) mit einer
 * anschliessenden affinen Abbildung - eine feste Zahlenfolge, kein Code. */
static const uint8_t g_sbox[256] = {
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

/* Verdoppeln im Koerper GF(2^8) mit dem Rankenpolynom 0x11B. */
static uint8_t xtime(uint8_t v)
{
	return (uint8_t)((uint8_t)(v << 1) ^ (uint8_t)(((v & 0x80u) != 0u) ? 0x1bu : 0x00u));
}

void osk_aes128_encrypt_block(const uint8_t key[OSK_AES128_KEY], const uint8_t in[OSK_AES_BLOCK],
			      uint8_t out[OSK_AES_BLOCK])
{
	uint8_t rk[11u * 16u];
	uint8_t st[16];
	uint8_t rcon = 1u;

	/* Schluesselplan (Abschnitt 5.2). */
	memcpy(rk, key, 16u);
	for (unsigned i = 1u; i <= 10u; i++) {
		uint8_t *prev = rk + (i - 1u) * 16u;
		uint8_t *cur = rk + i * 16u;

		cur[0] = (uint8_t)(prev[0] ^ g_sbox[prev[13]] ^ rcon);
		cur[1] = (uint8_t)(prev[1] ^ g_sbox[prev[14]]);
		cur[2] = (uint8_t)(prev[2] ^ g_sbox[prev[15]]);
		cur[3] = (uint8_t)(prev[3] ^ g_sbox[prev[12]]);
		for (unsigned j = 4u; j < 16u; j++)
			cur[j] = (uint8_t)(prev[j] ^ cur[j - 4u]);
		rcon = xtime(rcon);
	}

	memcpy(st, in, 16u);
	for (unsigned j = 0; j < 16u; j++)
		st[j] = (uint8_t)(st[j] ^ rk[j]);

	for (unsigned round = 1u; round <= 10u; round++) {
		uint8_t t[16];

		/* SubBytes und ShiftRows in einem Durchgang: Spalte c bekommt
		 * aus Zeile r das Byte der Spalte c+r. */
		for (unsigned c = 0; c < 4u; c++) {
			for (unsigned r = 0; r < 4u; r++)
				t[c * 4u + r] = g_sbox[st[((c + r) & 3u) * 4u + r]];
		}

		if (round != 10u) {
			/* MixColumns (Abschnitt 5.1.3). */
			for (unsigned c = 0; c < 4u; c++) {
				uint8_t *p = t + c * 4u;
				const uint8_t a0 = p[0];
				const uint8_t a1 = p[1];
				const uint8_t a2 = p[2];
				const uint8_t a3 = p[3];

				p[0] = (uint8_t)(xtime(a0) ^ (xtime(a1) ^ a1) ^ a2 ^ a3);
				p[1] = (uint8_t)(a0 ^ xtime(a1) ^ (xtime(a2) ^ a2) ^ a3);
				p[2] = (uint8_t)(a0 ^ a1 ^ xtime(a2) ^ (xtime(a3) ^ a3));
				p[3] = (uint8_t)((xtime(a0) ^ a0) ^ a1 ^ a2 ^ xtime(a3));
			}
		}

		for (unsigned j = 0; j < 16u; j++)
			st[j] = (uint8_t)(t[j] ^ rk[round * 16u + j]);
		memset(t, 0, sizeof(t));
	}

	memcpy(out, st, 16u);
	memset(st, 0, sizeof(st));
	memset(rk, 0, sizeof(rk));
}

/* --- CRC-16 (ISO/IEC 13239) --------------------------------------------- */

uint16_t osk_crc16(const uint8_t *data, size_t len)
{
	uint16_t crc = 0xffffu;

	if (data == NULL)
		return crc;
	for (size_t i = 0; i < len; i++) {
		crc = (uint16_t)(crc ^ data[i]);
		for (unsigned bit = 0; bit < 8u; bit++) {
			const bool low = (crc & 1u) != 0u;

			crc = (uint16_t)(crc >> 1);
			if (low)
				crc = (uint16_t)(crc ^ 0x8408u);
		}
	}
	return crc;
}

/* --- ModHex ------------------------------------------------------------- */

static const char g_modhex[17] = "cbdefghijklnrtuv";

bool osk_modhex_encode(const uint8_t *data, size_t len, char *out, size_t cap)
{
	if (data == NULL || out == NULL)
		return false;
	if (cap < len * 2u + 1u)
		return false;
	for (size_t i = 0; i < len; i++) {
		out[i * 2u] = g_modhex[(data[i] >> 4) & 0x0fu];
		out[i * 2u + 1u] = g_modhex[data[i] & 0x0fu];
	}
	out[len * 2u] = 0;
	return true;
}

static bool modhex_value(char c, uint8_t *out)
{
	for (uint8_t i = 0; i < 16u; i++) {
		if (g_modhex[i] == c) {
			*out = i;
			return true;
		}
	}
	return false;
}

bool osk_modhex_decode(const char *text, size_t text_len, uint8_t *out, size_t cap, size_t *len_out)
{
	if (text == NULL || out == NULL || len_out == NULL)
		return false;
	if ((text_len % 2u) != 0u || text_len / 2u > cap)
		return false;
	for (size_t i = 0; i < text_len / 2u; i++) {
		uint8_t hi = 0;
		uint8_t lo = 0;

		if (!modhex_value(text[i * 2u], &hi) || !modhex_value(text[i * 2u + 1u], &lo))
			return false;
		out[i] = (uint8_t)((hi << 4) | lo);
	}
	*len_out = text_len / 2u;
	return true;
}
