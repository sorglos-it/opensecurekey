/* Testvektoren der Spezifikationen fuer die Rechenbausteine.
 *
 * Quellen: FIPS 180-4 (SHA-512), RFC 4231 (HMAC-SHA512), die
 * RIPEMD-160-Veroeffentlichung, die Keccak-Beispielwerte und RFC 6070 in der
 * SHA-512-Fassung. Testdaten, kein Fremdcode. */

#include "test_util.h"
#include "wallet_hash.h"
#include "osk_crypto.h"

static void test_sha512(void)
{
	uint8_t out[OSK_SHA512_LEN];
	uint8_t big[1000];

	osk_sha512((const uint8_t *)"", 0, out);
	T_HEX("sha512 leer", out, sizeof(out),
	      "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce"
	      "47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e");

	osk_sha512((const uint8_t *)"abc", 3, out);
	T_HEX("sha512 abc", out, sizeof(out),
	      "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a"
	      "2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f");

	osk_sha512((const uint8_t *)"abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmn"
				    "hijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu",
		   112, out);
	T_HEX("sha512 112 Byte", out, sizeof(out),
	      "8e959b75dae313da8cf4f72814fc143f8f7779c6eb9f7fa17299aeadb6889018"
	      "501d289e4900f7e4331b99dec4b5433ac7d329eeb6dd26545e96e55b874be909");

	/* Eine Million 'a' waere zu langsam fuer jeden Lauf; 1000 Byte in
	 * vielen kleinen Haeppchen pruefen stattdessen das Puffern. */
	memset(big, 'a', sizeof(big));
	{
		osk_sha512_t c;
		uint8_t once[OSK_SHA512_LEN];

		osk_sha512(big, sizeof(big), once);
		osk_sha512_init(&c);
		for (size_t i = 0; i < sizeof(big); i++)
			osk_sha512_update(&c, big + i, 1);
		osk_sha512_final(&c, out);
		T_TRUE("sha512 haeppchenweise gleich", memcmp(once, out, sizeof(out)) == 0);
	}
}

static void test_hmac_sha512(void)
{
	uint8_t key[131];
	uint8_t out[OSK_SHA512_LEN];

	/* RFC 4231 Fall 1 */
	memset(key, 0x0b, 20);
	osk_hmac_sha512(key, 20, (const uint8_t *)"Hi There", 8, out);
	T_HEX("hmac512 rfc4231 1", out, sizeof(out),
	      "87aa7cdea5ef619d4ff0b4241a1d6cb02379f4e2ce4ec2787ad0b30545e17cde"
	      "daa833b7d6b8a702038b274eaea3f4e4be9d914eeb61f1702e696c203a126854");

	/* RFC 4231 Fall 2: Schluessel kuerzer als ein Block */
	osk_hmac_sha512((const uint8_t *)"Jefe", 4, (const uint8_t *)"what do ya want for nothing?",
			28, out);
	T_HEX("hmac512 rfc4231 2", out, sizeof(out),
	      "164b7a7bfcf819e2e395fbe73b56e0a387bd64222e831fd610270cd7ea250554"
	      "9758bf75c05a994a6d034f65f8f0e6fdcaeab1a34d4a6b4b636e070a38bce737");

	/* RFC 4231 Fall 6: Schluessel laenger als ein Block */
	memset(key, 0xaa, 131);
	osk_hmac_sha512(key, 131,
			(const uint8_t *)"Test Using Larger Than Block-Size Key - Hash Key First",
			54, out);
	T_HEX("hmac512 rfc4231 6", out, sizeof(out),
	      "80b24263c7c1a3ebb71493c1dd7be8b49b46d1f41b4aeec1121b013783f8f352"
	      "6b56d037e05f2598bd0fd2215d6a1e5295e64f73f63f0aec8b915a985d786598");
}

static void test_hmac_two_parts(void)
{
	uint8_t a[5] = {1, 2, 3, 4, 5};
	uint8_t one[OSK_SHA512_LEN], two[OSK_SHA512_LEN];

	osk_hmac_sha512((const uint8_t *)"key", 3, a, sizeof(a), one);
	osk_hmac_sha512_2((const uint8_t *)"key", 3, a, 2, a + 2, 3, two);
	T_TRUE("zweiteilig gleich einteilig", memcmp(one, two, sizeof(one)) == 0);
}

static void test_pbkdf2(void)
{
	uint8_t out[64];

	/* RFC 6070 Fall 1, auf SHA-512 gerechnet (die Vorlage fuer BIP39). */
	T_EQ("pbkdf2 ok",
	     osk_pbkdf2_sha512((const uint8_t *)"password", 8, (const uint8_t *)"salt", 4, 1, out,
			       64),
	     OSK_OK);
	T_HEX("pbkdf2 1 Runde", out, 64,
	      "867f70cf1ade02cff3752599a3a53dc4af34c7a669815ae5d513554e1c8cf252"
	      "c02d470a285a0501bad999bfe943c08f050235d7d68b1da55e63f73b60a57fce");

	T_EQ("pbkdf2 ok",
	     osk_pbkdf2_sha512((const uint8_t *)"password", 8, (const uint8_t *)"salt", 4, 2, out,
			       64),
	     OSK_OK);
	T_HEX("pbkdf2 2 Runden", out, 64,
	      "e1d9c16aa681708a45f5c7c4e215ceb66e011a2e9f0040713f18aefdb866d53c"
	      "f76cab2868a39b9f7840edce4fef5a82be67335c77a6068e04112754f27ccf4e");

	T_EQ("pbkdf2 ohne Runden abgelehnt",
	     osk_pbkdf2_sha512((const uint8_t *)"p", 1, (const uint8_t *)"s", 1, 0, out, 64),
	     OSK_ERR_ARG);
	T_EQ("pbkdf2 zu lang abgelehnt",
	     osk_pbkdf2_sha512((const uint8_t *)"p", 1, (const uint8_t *)"s", 1, 1, out, 65),
	     OSK_ERR_ARG);
}

static void test_ripemd160(void)
{
	uint8_t out[OSK_RIPEMD160_LEN];
	uint8_t big[64];

	osk_ripemd160((const uint8_t *)"", 0, out);
	T_HEX("ripemd leer", out, sizeof(out), "9c1185a5c5e9fc54612808977ee8f548b2258d31");
	osk_ripemd160((const uint8_t *)"abc", 3, out);
	T_HEX("ripemd abc", out, sizeof(out), "8eb208f7e05d987a9b044a8e98c6b087f15a0bfc");
	osk_ripemd160((const uint8_t *)"message digest", 14, out);
	T_HEX("ripemd message digest", out, sizeof(out),
	      "5d0689ef49d2fae572b881b123a85ffa21595f36");
	osk_ripemd160((const uint8_t *)"abcdefghijklmnopqrstuvwxyz", 26, out);
	T_HEX("ripemd a-z", out, sizeof(out), "f71c27109c692c1b56bbdceb5b9d2865b3708dbc");
	osk_ripemd160((const uint8_t *)"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
				       "0123456789",
		      62, out);
	T_HEX("ripemd 62 Byte", out, sizeof(out), "b0e20b6e3116640286ed3a87a5713079b21f5189");
	/* Genau 56 Byte: der Fall, in dem die Auffuellung einen zweiten Block
	 * braucht. Hier scheitert eine falsche Laengenrechnung. */
	memset(big, 'a', 56);
	osk_ripemd160(big, 56, out);
	T_HEX("ripemd 56 Byte", out, sizeof(out), "e72334b46c83cc70bef979e15453706c95b888be");
}

static void test_keccak(void)
{
	uint8_t out[OSK_KECCAK256_LEN];
	uint8_t big[200];

	osk_keccak256((const uint8_t *)"", 0, out);
	T_HEX("keccak leer", out, sizeof(out),
	      "c5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470");
	osk_keccak256((const uint8_t *)"abc", 3, out);
	T_HEX("keccak abc", out, sizeof(out),
	      "4e03657aea45a94fc7d47ba826c8d667c0d1e6e33a64a036ec44f58fa12d6c45");
	osk_keccak256((const uint8_t *)"testing", 7, out);
	T_HEX("keccak testing", out, sizeof(out),
	      "5f16f4c7f149ac4f9510d9cf8cf384038ad348b3bcdc01915f95de12df9d1b02");
	/* Genau ein voller Block (136 Byte) - der Fall, in dem die Auffuellung
	 * einen zusaetzlichen Block braucht. */
	memset(big, 'a', 136);
	osk_keccak256(big, 136, out);
	T_HEX("keccak 136 Byte", out, sizeof(out),
	      "a6c4d403279fe3e0af03729caada8374b5ca54d8065329a3ebcaeb4b60aa386e");
}

static void test_bitcoin_hashes(void)
{
	uint8_t out[32];

	osk_hash256((const uint8_t *)"hello", 5, out);
	T_HEX("hash256 hello", out, 32,
	      "9595c9df90075148eb06860365df33584b75bff782a510c6cd4883a419833d50");
	osk_hash160((const uint8_t *)"hello", 5, out);
	T_HEX("hash160 hello", out, 20, "b6a9c8c230722b7c748331a8b450f05566dc7d0f");
}

int main(void)
{
	test_sha512();
	test_hmac_sha512();
	test_hmac_two_parts();
	test_pbkdf2();
	test_ripemd160();
	test_keccak();
	test_bitcoin_hashes();
	return t_report("hash");
}
