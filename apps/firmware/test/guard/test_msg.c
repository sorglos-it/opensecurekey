/* Die unterschriebenen Bytefolgen - ohne Chip, ohne Rahmen, ohne Attrappe.
 * Hier wird nur geprueft, WAS unterschrieben wird. */
#include "test_util.h"
#include "osk_guard_msg.h"

static void test_attest_bytes(void)
{
	uint8_t nonce[OSK_GUARD_NONCE_LEN];
	uint8_t hash[OSK_GUARD_HASH_LEN];
	uint8_t msg[OSK_GUARD_MSG_MAX];
	size_t len = 0;

	for (size_t i = 0; i < sizeof(nonce); i++) {
		nonce[i] = (uint8_t)i;
	}
	for (size_t i = 0; i < sizeof(hash); i++) {
		hash[i] = (uint8_t)(0xf0u + i);
	}

	T_EQ("attest baut",
	     osk_guard_msg_attest(nonce, sizeof(nonce), hash, sizeof(hash), msg, sizeof(msg), &len),
	     OSK_OK);
	/* Zweck, Trennbyte, Zufallswert, Pruefsumme - genau wie GuardMessage()
	 * auf der Rechnerseite. */
	T_EQ("attest Laenge", len, 18 + 1 + 32 + 32);
	T_TRUE("attest Zweck", memcmp(msg, "osk-guard/attest/1", 18) == 0);
	T_EQ("attest Trennbyte", msg[18], 0);
	T_TRUE("attest Zufallswert", memcmp(msg + 19, nonce, sizeof(nonce)) == 0);
	T_TRUE("attest Pruefsumme", memcmp(msg + 51, hash, sizeof(hash)) == 0);

	/* Feste Laengen, damit die Bytefolge eindeutig zerlegbar ist. */
	T_EQ("attest kurzer Zufallswert",
	     osk_guard_msg_attest(nonce, 31u, hash, sizeof(hash), msg, sizeof(msg), &len),
	     OSK_ERR_ARG);
	T_EQ("attest kurze Pruefsumme",
	     osk_guard_msg_attest(nonce, sizeof(nonce), hash, 31u, msg, sizeof(msg), &len),
	     OSK_ERR_ARG);
	T_EQ("attest ohne Puffer",
	     osk_guard_msg_attest(nonce, sizeof(nonce), hash, sizeof(hash), msg, 10u, &len),
	     OSK_ERR_ARG);
}

static void test_attest_raw_bytes(void)
{
	uint8_t challenge[OSK_GUARD_CHALLENGE_MAX + 1u];
	uint8_t msg[OSK_GUARD_MSG_MAX];
	size_t len = 0;

	memset(challenge, 0x11, sizeof(challenge));

	T_EQ("raw baut", osk_guard_msg_attest_raw(challenge, 8u, msg, sizeof(msg), &len), OSK_OK);
	T_EQ("raw Laenge", len, 22 + 1 + 8);
	T_TRUE("raw Zweck", memcmp(msg, "osk-guard/attest-raw/1", 22) == 0);
	T_EQ("raw Trennbyte", msg[22], 0);

	T_EQ("raw leer", osk_guard_msg_attest_raw(challenge, 0u, msg, sizeof(msg), &len),
	     OSK_ERR_ARG);
	/* Die Obergrenze des Geraets: mehr nimmt osk_se050_sign() fuer
	 * Ed25519 nicht an, und gehasht wird nicht. */
	T_EQ("raw Obergrenze",
	     osk_guard_msg_attest_raw(challenge, OSK_GUARD_CHALLENGE_MAX, msg, sizeof(msg), &len),
	     OSK_OK);
	T_EQ("raw Laenge an der Grenze", len, OSK_GUARD_MSG_MAX);
	T_EQ("raw zu lang",
	     osk_guard_msg_attest_raw(challenge, OSK_GUARD_CHALLENGE_MAX + 1u, msg, sizeof(msg),
				      &len),
	     OSK_ERR_ARG);
}

static void test_sign_bytes(void)
{
	uint8_t hash[OSK_GUARD_HASH_LEN];
	uint8_t msg[OSK_GUARD_MSG_MAX];
	char label[OSK_GUARD_LABEL_MAX + 1u];
	size_t len = 0;

	memset(hash, 0x22, sizeof(hash));
	memset(label, 'a', sizeof(label));

	T_EQ("sign baut",
	     osk_guard_msg_sign("prod-eu-1", 9u, hash, sizeof(hash), msg, sizeof(msg), &len),
	     OSK_OK);
	T_EQ("sign Laenge", len, 16 + 1 + 9 + 1 + 32);
	T_TRUE("sign Zweck", memcmp(msg, "osk-guard/sign/1", 16) == 0);
	T_EQ("sign Trennbyte", msg[16], 0);
	T_TRUE("sign Name", memcmp(msg + 17, "prod-eu-1", 9) == 0);
	T_EQ("sign Trennbyte hinter dem Namen", msg[26], 0);
	T_TRUE("sign Pruefsumme", memcmp(msg + 27, hash, sizeof(hash)) == 0);

	T_EQ("sign langer Name",
	     osk_guard_msg_sign(label, OSK_GUARD_LABEL_MAX, hash, sizeof(hash), msg, sizeof(msg),
				&len),
	     OSK_OK);
	T_EQ("sign zu langer Name",
	     osk_guard_msg_sign(label, OSK_GUARD_LABEL_MAX + 1u, hash, sizeof(hash), msg,
				sizeof(msg), &len),
	     OSK_ERR_ARG);
	T_EQ("sign leerer Name",
	     osk_guard_msg_sign("", 0u, hash, sizeof(hash), msg, sizeof(msg), &len), OSK_ERR_ARG);
	T_EQ("sign krumme Pruefsumme",
	     osk_guard_msg_sign("a", 1u, hash, 31u, msg, sizeof(msg), &len), OSK_ERR_ARG);
	/* Ein Nullbyte im Namen machte die Zerlegung mehrdeutig: "a\0b" und
	 * "a" mit Name-Ende waeren dieselben Bytes. */
	T_EQ("sign Nullbyte im Namen",
	     osk_guard_msg_sign("a\0b", 3u, hash, sizeof(hash), msg, sizeof(msg), &len),
	     OSK_ERR_ARG);
}

/* Die drei Zwecke muessen verschieden sein - sonst kann eine Antwort aus
 * einem Zweck in einem anderen gelten. Das ist hier nur der Anfang der
 * Bytefolge; der ganze Angriff steht in test_forgery.c. */
static void test_domains_differ(void)
{
	uint8_t nonce[OSK_GUARD_NONCE_LEN];
	uint8_t hash[OSK_GUARD_HASH_LEN];
	uint8_t a[OSK_GUARD_MSG_MAX];
	uint8_t b[OSK_GUARD_MSG_MAX];
	uint8_t c[OSK_GUARD_MSG_MAX];
	size_t la = 0, lb = 0, lc = 0;

	memset(nonce, 1, sizeof(nonce));
	memset(hash, 2, sizeof(hash));

	T_EQ("a", osk_guard_msg_attest(nonce, sizeof(nonce), hash, sizeof(hash), a, sizeof(a), &la),
	     OSK_OK);
	T_EQ("b", osk_guard_msg_attest_raw(nonce, sizeof(nonce), b, sizeof(b), &lb), OSK_OK);
	T_EQ("c", osk_guard_msg_sign("k", 1u, hash, sizeof(hash), c, sizeof(c), &lc), OSK_OK);

	T_TRUE("attest != raw", la != lb || memcmp(a, b, la) != 0);
	T_TRUE("attest != sign", la != lc || memcmp(a, c, la) != 0);
	T_TRUE("raw != sign", lb != lc || memcmp(b, c, lb) != 0);
}

int main(void)
{
	test_attest_bytes();
	test_attest_raw_bytes();
	test_sign_bytes();
	test_domains_differ();
	return t_report("guard-bytefolgen");
}
