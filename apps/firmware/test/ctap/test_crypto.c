/* Rechenbausteine gegen die Prueffaelle der Normen: SHA-256 (FIPS 180-4),
 * HMAC (RFC 4231), HKDF (RFC 5869) und AES-256 (FIPS 197 / SP 800-38A). */
#include "test_util.h"
#include "osk_crypto.h"

static void test_sha256(void)
{
	uint8_t out[32];

	osk_sha256((const uint8_t *)"abc", 3, out);
	T_HEX("SHA-256(abc)", out, 32,
	      "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

	osk_sha256(NULL, 0, out);
	T_HEX("SHA-256(leer)", out, 32,
	      "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

	/* 56 Bytes: genau an der Auffuell-Grenze, wo ein zweiter Block entsteht */
	const char *m56 = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
	osk_sha256((const uint8_t *)m56, 56, out);
	T_HEX("SHA-256(56 Bytes)", out, 32,
	      "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");

	/* eine Million Mal das Zeichen a, in Stuecken gefuettert */
	osk_sha256_t c;
	osk_sha256_init(&c);
	uint8_t a[1000];
	memset(a, 'a', sizeof(a));
	for (int i = 0; i < 1000; i++)
		osk_sha256_update(&c, a, sizeof(a));
	osk_sha256_final(&c, out);
	T_HEX("SHA-256(1e6 x a)", out, 32,
	      "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

static void test_hmac(void)
{
	uint8_t key[20], out[32];
	memset(key, 0x0b, sizeof(key));

	/* RFC 4231, Fall 1 */
	osk_hmac_sha256(key, sizeof(key), (const uint8_t *)"Hi There", 8, out);
	T_HEX("HMAC RFC4231/1", out, 32,
	      "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7");

	/* RFC 4231, Fall 2 */
	osk_hmac_sha256((const uint8_t *)"Jefe", 4,
			(const uint8_t *)"what do ya want for nothing?", 28, out);
	T_HEX("HMAC RFC4231/2", out, 32,
	      "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");

	/* Schluessel laenger als ein Block (131 Bytes), RFC 4231 Fall 6 */
	uint8_t big[131];
	memset(big, 0xaa, sizeof(big));
	osk_hmac_sha256(big, sizeof(big),
			(const uint8_t *)"Test Using Larger Than Block-Size Key - Hash Key First", 54,
			out);
	T_HEX("HMAC RFC4231/6", out, 32,
	      "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54");

	/* zweiteiliger Text muss dasselbe ergeben wie der zusammenkopierte */
	uint8_t a[32], b[32];
	osk_hmac_sha256((const uint8_t *)"k", 1, (const uint8_t *)"abcdef", 6, a);
	osk_hmac_sha256_2((const uint8_t *)"k", 1, (const uint8_t *)"abc", 3,
			  (const uint8_t *)"def", 3, b);
	T_TRUE("HMAC zweiteilig", memcmp(a, b, 32) == 0);
}

static void test_hkdf(void)
{
	/* RFC 5869, Fall 1 */
	uint8_t ikm[22], salt[13], info[10], out[42];
	memset(ikm, 0x0b, sizeof(ikm));
	for (unsigned i = 0; i < sizeof(salt); i++)
		salt[i] = (uint8_t)i;
	for (unsigned i = 0; i < sizeof(info); i++)
		info[i] = (uint8_t)(0xf0 + i);

	T_EQ("HKDF Fall 1 laeuft",
	     osk_hkdf_sha256(salt, sizeof(salt), ikm, sizeof(ikm), info, sizeof(info),
			     out, sizeof(out)), OSK_OK);
	T_HEX("HKDF RFC5869/1", out, 42,
	      "3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf"
	      "34007208d5b887185865");

	/* Fall 3: ohne Salz und ohne Info */
	T_EQ("HKDF Fall 3 laeuft",
	     osk_hkdf_sha256(NULL, 0, ikm, sizeof(ikm), NULL, 0, out, sizeof(out)), OSK_OK);
	T_HEX("HKDF RFC5869/3", out, 42,
	      "8da4e775a563c18f715f802a063c5a31b8a11f5c5ee1879ec3454e5f3c738d2d"
	      "9d201395faa4b61a96c8");

	/* Null Bytes Ausgabe ist ein Aufruffehler, nicht stille Null */
	T_EQ("HKDF Laenge 0 wird abgewiesen",
	     osk_hkdf_sha256(NULL, 0, ikm, sizeof(ikm), NULL, 0, out, 0), OSK_ERR_ARG);
}

static void test_aes(void)
{
	/* FIPS 197, Anhang C.3: AES-256 ein Block */
	uint8_t key[32], in[16], out[16], back[16];
	for (unsigned i = 0; i < 32; i++)
		key[i] = (uint8_t)i;
	for (unsigned i = 0; i < 16; i++)
		in[i] = (uint8_t)(i * 0x11);
	const uint8_t zero_iv[16] = { 0 };

	T_EQ("AES laeuft", osk_aes256_cbc_encrypt(key, zero_iv, in, out, 16), OSK_OK);
	T_HEX("AES-256 FIPS197 C.3", out, 16, "8ea2b7ca516745bfeafc49904b496089");

	T_EQ("AES zurueck", osk_aes256_cbc_decrypt(key, zero_iv, out, back, 16), OSK_OK);
	T_TRUE("AES Hin und Zurueck", memcmp(in, back, 16) == 0);

	/* SP 800-38A, F.2.5/F.2.6: AES-256-CBC ueber vier Bloecke */
	const uint8_t k38[32] = {
		0x60,0x3d,0xeb,0x10,0x15,0xca,0x71,0xbe,0x2b,0x73,0xae,0xf0,0x85,0x7d,0x77,0x81,
		0x1f,0x35,0x2c,0x07,0x3b,0x61,0x08,0xd7,0x2d,0x98,0x10,0xa3,0x09,0x14,0xdf,0xf4,
	};
	const uint8_t iv38[16] = {
		0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
	};
	const uint8_t pt[64] = {
		0x6b,0xc1,0xbe,0xe2,0x2e,0x40,0x9f,0x96,0xe9,0x3d,0x7e,0x11,0x73,0x93,0x17,0x2a,
		0xae,0x2d,0x8a,0x57,0x1e,0x03,0xac,0x9c,0x9e,0xb7,0x6f,0xac,0x45,0xaf,0x8e,0x51,
		0x30,0xc8,0x1c,0x46,0xa3,0x5c,0xe4,0x11,0xe5,0xfb,0xc1,0x19,0x1a,0x0a,0x52,0xef,
		0xf6,0x9f,0x24,0x45,0xdf,0x4f,0x9b,0x17,0xad,0x2b,0x41,0x7b,0xe6,0x6c,0x37,0x10,
	};
	uint8_t ct[64], pt2[64];
	T_EQ("CBC vier Bloecke", osk_aes256_cbc_encrypt(k38, iv38, pt, ct, 64), OSK_OK);
	T_HEX("AES-256-CBC SP800-38A", ct, 64,
	      "f58c4c04d6e5f1ba779eabfb5f7bfbd6"
	      "9cfc4e967edb808d679f777bc6702c7d"
	      "39f23369a9d9bacfa530e26304231461"
	      "b2eb05e2c39be9fcda6c19078c6a9d1b");
	T_EQ("CBC zurueck", osk_aes256_cbc_decrypt(k38, iv38, ct, pt2, 64), OSK_OK);
	T_TRUE("CBC Hin und Zurueck", memcmp(pt, pt2, 64) == 0);

	/* Entschluesseln in denselben Puffer darf nichts verderben */
	memcpy(pt2, ct, 64);
	T_EQ("CBC am Platz", osk_aes256_cbc_decrypt(k38, iv38, pt2, pt2, 64), OSK_OK);
	T_TRUE("CBC am Platz richtig", memcmp(pt, pt2, 64) == 0);

	/* Laenge kein Vielfaches von 16: Fehler, nicht Ueberlauf */
	T_EQ("CBC krumme Laenge", osk_aes256_cbc_encrypt(key, zero_iv, in, out, 15),
	     OSK_ERR_ARG);
}

static void test_kleinteile(void)
{
	const uint8_t a[4] = { 1, 2, 3, 4 };
	const uint8_t b[4] = { 1, 2, 3, 5 };
	T_TRUE("memeq gleich", osk_memeq(a, a, 4));
	T_TRUE("memeq verschieden", !osk_memeq(a, b, 4));
	T_TRUE("memeq Laenge 0", osk_memeq(a, b, 0));

	uint8_t buf[8];
	memset(buf, 0xff, sizeof(buf));
	osk_wipe(buf, sizeof(buf));
	for (unsigned i = 0; i < sizeof(buf); i++)
		T_TRUE("wipe loescht", buf[i] == 0);
}

int main(void)
{
	test_sha256();
	test_hmac();
	test_hkdf();
	test_aes();
	test_kleinteile();
	return t_report("crypto");
}
