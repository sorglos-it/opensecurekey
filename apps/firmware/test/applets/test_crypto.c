/* SHA-1, HMAC-SHA1, AES-128, CRC-16 und ModHex gegen die Vorlagen der
 * jeweiligen Spezifikation. Hier wird nichts nachgestellt: diese Werte sind
 * nachpruefbar und stehen in den Dokumenten.
 *
 *   SHA-1    FIPS 180-4 / RFC 3174, Anhang mit den Beispielen
 *   HMAC     RFC 2202, Faelle 1, 2, 3 und 6
 *   AES-128  FIPS 197, Anhang B und C.1
 *   CRC-16   ISO/IEC 13239, Pruefwert ueber "123456789"
 *   ModHex   die Zuordnungstabelle
 */
#include "test_util.h"
#include "osk_otp_crypto.h"

static void test_sha1(void)
{
	uint8_t out[OSK_SHA1_LEN];
	uint8_t buf[1000];

	osk_sha1((const uint8_t *)"", 0u, out);
	T_HEX("SHA-1 von nichts", out, sizeof(out), "da39a3ee5e6b4b0d3255bfef95601890afd80709");

	osk_sha1((const uint8_t *)"abc", 3u, out);
	T_HEX("SHA-1 abc", out, sizeof(out), "a9993e364706816aba3e25717850c26c9cd0d89d");

	{
		static const char *msg = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";

		osk_sha1((const uint8_t *)msg, 56u, out);
		T_HEX("SHA-1 ueber 56 Zeichen", out, sizeof(out),
		      "84983e441c3bd26ebaae4aa1f95129e5e54670f1");
	}

	/* Eine Million 'a' - der lange Fall aus RFC 3174. Er prueft, dass das
	 * Zusammensetzen ueber Blockgrenzen stimmt. */
	{
		osk_sha1_t c;

		for (size_t i = 0; i < sizeof(buf); i++)
			buf[i] = (uint8_t)'a';
		osk_sha1_init(&c);
		for (unsigned i = 0; i < 1000u; i++)
			osk_sha1_update(&c, buf, sizeof(buf));
		osk_sha1_final(&c, out);
		T_HEX("SHA-1 ueber eine Million a", out, sizeof(out),
		      "34aa973cd4c4daa4f61eeb2bdbad27316534016f");
	}

	/* Haeppchenweise muss dasselbe herauskommen wie in einem Stueck. */
	{
		osk_sha1_t c;
		uint8_t once[OSK_SHA1_LEN];

		osk_sha1((const uint8_t *)"abcdefghij", 10u, once);
		osk_sha1_init(&c);
		for (unsigned i = 0; i < 10u; i++)
			osk_sha1_update(&c, (const uint8_t *)"abcdefghij" + i, 1u);
		osk_sha1_final(&c, out);
		T_TRUE("SHA-1 Byte fuer Byte gleich", memcmp(once, out, sizeof(out)) == 0);
	}
}

static void test_hmac(void)
{
	uint8_t key[80];
	uint8_t data[50];
	uint8_t out[OSK_SHA1_LEN];

	/* RFC 2202 Fall 1 */
	memset(key, 0x0b, 20u);
	osk_hmac_sha1(key, 20u, (const uint8_t *)"Hi There", 8u, out);
	T_HEX("HMAC-SHA1 Fall 1", out, sizeof(out), "b617318655057264e28bc0b6fb378c8ef146be00");

	/* Fall 2 */
	osk_hmac_sha1((const uint8_t *)"Jefe", 4u, (const uint8_t *)"what do ya want for nothing?",
		      28u, out);
	T_HEX("HMAC-SHA1 Fall 2", out, sizeof(out), "effcdf6ae5eb2fa2d27416d5f184df9c259a7c79");

	/* Fall 3 */
	memset(key, 0xaa, 20u);
	memset(data, 0xdd, sizeof(data));
	osk_hmac_sha1(key, 20u, data, sizeof(data), out);
	T_HEX("HMAC-SHA1 Fall 3", out, sizeof(out), "125d7342b9ac11cd91a39af48aa17b4f63f175d3");

	/* Fall 6: Schluessel laenger als ein Block - er wird erst gehasht. */
	memset(key, 0xaa, 80u);
	osk_hmac_sha1(key, 80u,
		      (const uint8_t *)"Test Using Larger Than Block-Size Key - Hash Key First",
		      54u, out);
	T_HEX("HMAC-SHA1 langer Schluessel", out, sizeof(out),
	      "aa4ae5e15272d00e95705637ce8a3b55ed402112");
}

static void test_aes(void)
{
	uint8_t key[16];
	uint8_t in[16];
	uint8_t out[16];

	/* FIPS 197 Anhang B */
	T_EQ("Schluessel B", t_unhex("2b7e151628aed2a6abf7158809cf4f3c", key, sizeof(key)), 16);
	T_EQ("Klartext B", t_unhex("3243f6a8885a308d313198a2e0370734", in, sizeof(in)), 16);
	osk_aes128_encrypt_block(key, in, out);
	T_HEX("AES-128 Anhang B", out, sizeof(out), "3925841d02dc09fbdc118597196a0b32");

	/* FIPS 197 Anhang C.1 */
	T_EQ("Schluessel C1", t_unhex("000102030405060708090a0b0c0d0e0f", key, sizeof(key)), 16);
	T_EQ("Klartext C1", t_unhex("00112233445566778899aabbccddeeff", in, sizeof(in)), 16);
	osk_aes128_encrypt_block(key, in, out);
	T_HEX("AES-128 Anhang C.1", out, sizeof(out), "69c4e0d86a7b0430d8cdb78070b4c55a");
}

static void test_crc(void)
{
	uint8_t block[16];
	uint16_t crc;

	/* ISO/IEC 13239: der Pruefwert ueber "123456789". Ohne das
	 * abschliessende XOR, das die Variante X-25 noch anhaengt. */
	T_EQ("CRC-16 Pruefwert", osk_crc16((const uint8_t *)"123456789", 9u), 0x6f91);

	/* Die Eigenschaft, auf die es ankommt: Pruefsumme angehaengt, dann
	 * bleibt ueber dem ganzen Block der feste Rest stehen. So prueft die
	 * Gegenseite ein Einmalkennwort. */
	for (unsigned i = 0; i < 14u; i++)
		block[i] = (uint8_t)(i * 7u + 1u);
	crc = (uint16_t)(osk_crc16(block, 14u) ^ OSK_CRC16_XOROUT);
	block[14] = (uint8_t)crc;
	block[15] = (uint8_t)(crc >> 8);
	T_EQ("CRC-16 Rest ueber den ganzen Block", osk_crc16(block, 16u), OSK_CRC16_RESIDUE);
}

static void test_modhex(void)
{
	uint8_t raw[8];
	uint8_t back[8];
	char text[32];
	size_t len = 0;

	T_EQ("Vorlage einlesen", t_unhex("2d344e83", raw, sizeof(raw)), 4);
	T_TRUE("ModHex schreiben", osk_modhex_encode(raw, 4u, text, sizeof(text)));
	T_TRUE("ModHex Beispiel", strcmp(text, "dteffuje") == 0);

	T_TRUE("ModHex zurueck", osk_modhex_decode(text, 8u, back, sizeof(back), &len));
	T_EQ("ModHex Laenge zurueck", len, 4);
	T_TRUE("ModHex gleich", memcmp(raw, back, 4u) == 0);

	/* Ein Buchstabe, der nicht dazugehoert, wird abgewiesen - 'a', 'm',
	 * 'o', 'q', 's' und 'p' gehoeren nicht in die Tabelle. */
	T_TRUE("ModHex lehnt a ab", !osk_modhex_decode("ca", 2u, back, sizeof(back), &len));
	T_TRUE("ModHex lehnt p ab", !osk_modhex_decode("cp", 2u, back, sizeof(back), &len));
	T_TRUE("ModHex lehnt krumme Laenge ab",
	       !osk_modhex_decode("ccc", 3u, back, sizeof(back), &len));
	T_TRUE("ModHex prueft den Platz", !osk_modhex_encode(raw, 4u, text, 4u));
}

int main(void)
{
	test_sha1();
	test_hmac();
	test_aes();
	test_crc();
	test_modhex();
	return t_report("test_crypto");
}
