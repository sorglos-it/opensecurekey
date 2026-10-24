/* BIP39 gegen die amtlichen Testvektoren der Spezifikation.
 *
 * Die Vektoren stammen aus der Vektorensammlung zu BIP39 (Entropie,
 * Woerter, Seed mit der Passphrase "TREZOR"). Sie sind unabhaengig
 * nachgerechnet worden, bevor sie hier eingetragen wurden - Pruefdaten,
 * kein Fremdcode. */

#include "test_util.h"
#include "wallet_bip39.h"
#include "mocks.h"

struct vector {
	const char *entropy;
	const char *mnemonic;
	const char *seed;
};

/* Die ersten acht sind die Randfaelle der Sammlung: alle Bits 0, 0x7f,
 * 0x80 und 0xff, je in 12 und 24 Woertern. Dazu zwei Faelle mit
 * gemischter Entropie. */
static const struct vector VECTORS[] = {
    {"00000000000000000000000000000000",
     "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon "
     "abandon about",
     "c55257c360c07c72029aebc1b53c05ed0362ada38ead3e3e9efa3708e53495531f09a6987599d1826"
     "4c1e1c92f2cf141630c7a3c4ab7c81b2f001698e7463b04"},
    {"7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f",
     "legal winner thank year wave sausage worth useful legal winner thank yellow",
     "2e8905819b8723fe2c1d161860e5ee1830318dbf49a83bd451cfb8440c28bd6fa457fe1296106559a"
     "3c80937a1c1069be3a3a5bd381ee6260e8d9739fce1f607"},
    {"80808080808080808080808080808080",
     "letter advice cage absurd amount doctor acoustic avoid letter advice cage above",
     "d71de856f81a8acc65e6fc851a38d4d7ec216fd0796d0a6827a3ad6ed5511a30fa280f12eb2e47ed2"
     "ac03b5c462a0358d18d69fe4f985ec81778c1b370b652a8"},
    {"ffffffffffffffffffffffffffffffff", "zoo zoo zoo zoo zoo zoo zoo zoo zoo zoo zoo wrong",
     "ac27495480225222079d7be181583751e86f571027b0497b5b5d11218e0a8a13332572917f0f8e5a5"
     "89620c6f15b11c61dee327651a14c34e18231052e48c069"},
    {"0000000000000000000000000000000000000000000000000000000000000000",
     "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon "
     "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon "
     "abandon abandon abandon art",
     "bda85446c68413707090a52022edd26a1c9462295029f2e60cd7c4f2bbd3097170af7a4d73245cafa"
     "9c3cca8d561a7c3de6f5d4a10be8ed2a5e608d68f92fcc8"},
    {"7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f",
     "legal winner thank year wave sausage worth useful legal winner thank year wave "
     "sausage worth useful legal winner thank year wave sausage worth title",
     "bc09fca1804f7e69da93c2f2028eb238c227f2e9dda30cd63699232578480a4021b146ad717fbb7e4"
     "51ce9eb835f43620bf5c514db0f8add49f5d121449d3e87"},
    {"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
     "zoo zoo zoo zoo zoo zoo zoo zoo zoo zoo zoo zoo zoo zoo zoo zoo zoo zoo zoo zoo "
     "zoo zoo zoo vote",
     "dd48c104698c30cfe2b6142103248622fb7bb0ff692eebb00089b32d22484e1613912f0a5b694407b"
     "e899ffd31ed3992c456cdf60f5d4564b8ba3f05a69890ad"},
    {"77c2b00716cec7213839159e404db50d",
     "jelly better achieve collect unaware mountain thought cargo oxygen act hood bridge",
     "b5b6d0127db1a9d2226af0c3346031d77af31e918dba64287a1b44b8ebf63cdd52676f672a290aae5"
     "02472cf2d602c051f3e6f18055e84e4c43897fc4e51a6ff"},
    {"eaebabb2383351fd31d703840b32e9e2",
     "turtle front uncle idea crush write shrug there lottery flower risk shell",
     "bdfb76a0759f301b0b899a1e3985227e53b3f51e67e3f2a65363caedf3e32fde42a66c404f18d7b05"
     "818c95ef3ca1e5146646856c461c073169467511680876c"},
};

static void test_vectors(void)
{
	for (size_t i = 0; i < sizeof(VECTORS) / sizeof(VECTORS[0]); i++) {
		uint8_t entropy[32], back[32];
		uint8_t seed[OSK_BIP39_SEED_LEN];
		char words[OSK_BIP39_MAX_TEXT];
		size_t ent_len, len = 0;

		ent_len = t_unhex(VECTORS[i].entropy, entropy, sizeof(entropy));
		T_TRUE("Entropie gelesen", ent_len == 16u || ent_len == 32u);
		T_EQ("Woerter gebaut",
		     osk_bip39_from_entropy(entropy, ent_len, words, sizeof(words), &len), OSK_OK);
		T_TRUE("Woerter stimmen", strcmp(words, VECTORS[i].mnemonic) == 0);
		if (strcmp(words, VECTORS[i].mnemonic) != 0)
			printf("  bekam    %s\n  erwartet %s\n", words, VECTORS[i].mnemonic);
		T_EQ("Pruefsumme stimmt", osk_bip39_check(VECTORS[i].mnemonic), OSK_OK);
		T_EQ("zurueck zur Entropie",
		     osk_bip39_to_entropy(VECTORS[i].mnemonic, back, sizeof(back), &len), OSK_OK);
		T_EQ("gleiche Laenge", len, ent_len);
		T_TRUE("gleiche Entropie", memcmp(entropy, back, ent_len) == 0);
		T_EQ("Seed gerechnet", osk_bip39_to_seed(VECTORS[i].mnemonic, "TREZOR", seed),
		     OSK_OK);
		T_HEX("Seed stimmt", seed, sizeof(seed), VECTORS[i].seed);
	}
}

static void test_wordlist(void)
{
	/* Sortiert, eindeutig in den ersten vier Buchstaben - das sichert
	 * BIP39 zu, und darauf verlassen sich Geraete, die nur vier
	 * Buchstaben eintippen lassen. */
	for (size_t i = 1; i < OSK_BIP39_WORDS; i++) {
		if (strcmp(osk_bip39_english[i - 1], osk_bip39_english[i]) >= 0)
			t_fail("Liste sortiert", __FILE__, __LINE__, osk_bip39_english[i]);
	}
	T_TRUE("Liste sortiert geprueft", true);
	for (size_t i = 1; i < OSK_BIP39_WORDS; i++) {
		if (strncmp(osk_bip39_english[i - 1], osk_bip39_english[i], 4) == 0)
			t_fail("vier Buchstaben eindeutig", __FILE__, __LINE__,
			       osk_bip39_english[i]);
	}
	T_TRUE("vier Buchstaben geprueft", true);
	T_EQ("erstes Wort", osk_bip39_word_index("abandon", 7), 0);
	T_EQ("letztes Wort", osk_bip39_word_index("zoo", 3), 2047);
	T_EQ("mittendrin", osk_bip39_word_index("jelly", 5), 958);
	T_EQ("unbekannt", osk_bip39_word_index("sorglos", 7), -1);
	T_EQ("Teilwort gilt nicht", osk_bip39_word_index("aban", 4), -1);
	T_EQ("laenger gilt nicht", osk_bip39_word_index("abandonx", 8), -1);
	T_EQ("leer", osk_bip39_word_index("", 0), -1);
}

static void test_bad_input(void)
{
	uint8_t entropy[32];
	char words[OSK_BIP39_MAX_TEXT];
	size_t len = 0;

	memset(entropy, 0, sizeof(entropy));
	T_EQ("17 Byte Entropie abgelehnt",
	     osk_bip39_from_entropy(entropy, 17, words, sizeof(words), &len), OSK_ERR_ARG);
	T_EQ("15 Byte abgelehnt", osk_bip39_from_entropy(entropy, 15, words, sizeof(words), &len),
	     OSK_ERR_ARG);
	T_EQ("33 Byte abgelehnt", osk_bip39_from_entropy(entropy, 33, words, sizeof(words), &len),
	     OSK_ERR_ARG);
	T_EQ("Puffer zu klein", osk_bip39_from_entropy(entropy, 32, words, 10, &len), OSK_ERR_ARG);

	T_EQ("falsche Pruefsumme",
	     osk_bip39_check("abandon abandon abandon abandon abandon abandon abandon abandon "
			     "abandon abandon abandon abandon"),
	     OSK_ERR_FORMAT);
	T_EQ("zu wenig Woerter", osk_bip39_check("abandon abandon about"), OSK_ERR_FORMAT);
	T_EQ("13 Woerter",
	     osk_bip39_check("abandon abandon abandon abandon abandon abandon abandon abandon "
			     "abandon abandon abandon about about"),
	     OSK_ERR_FORMAT);
	T_EQ("fremdes Wort",
	     osk_bip39_check("sorglos abandon abandon abandon abandon abandon abandon abandon "
			     "abandon abandon abandon about"),
	     OSK_ERR_FORMAT);
	T_EQ("Grossbuchstaben",
	     osk_bip39_check("ABANDON abandon abandon abandon abandon abandon abandon abandon "
			     "abandon abandon abandon about"),
	     OSK_ERR_FORMAT);
	T_EQ("Komma statt Leerzeichen",
	     osk_bip39_check("abandon,abandon abandon abandon abandon abandon abandon abandon "
			     "abandon abandon abandon about"),
	     OSK_ERR_FORMAT);
	T_EQ("leer", osk_bip39_check(""), OSK_ERR_FORMAT);
	T_EQ("nur Leerzeichen", osk_bip39_check("    "), OSK_ERR_FORMAT);
	/* Mehrere Leerzeichen zwischen Woertern sind in Ordnung - das
	 * passiert beim Abtippen und darf nicht am Format scheitern. */
	T_EQ("doppelte Leerzeichen gehen",
	     osk_bip39_check("abandon  abandon abandon abandon abandon abandon abandon abandon "
			     "abandon abandon abandon  about "),
	     OSK_OK);
}

static void test_generate(void)
{
	char words[OSK_BIP39_MAX_TEXT];
	size_t len = 0;

	mock_se050_reset();
	T_EQ("12 Woerter aus dem Chip", osk_bip39_generate(12, words, sizeof(words), &len), OSK_OK);
	T_EQ("Pruefsumme der eigenen Woerter", osk_bip39_check(words), OSK_OK);
	T_EQ("24 Woerter aus dem Chip", osk_bip39_generate(24, words, sizeof(words), &len), OSK_OK);
	T_EQ("Pruefsumme der eigenen Woerter", osk_bip39_check(words), OSK_OK);
	T_EQ("13 Woerter gibt es nicht", osk_bip39_generate(13, words, sizeof(words), &len),
	     OSK_ERR_ARG);
	T_EQ("0 Woerter gibt es nicht", osk_bip39_generate(0, words, sizeof(words), &len),
	     OSK_ERR_ARG);
}

static void test_passphrase(void)
{
	const char *m = "zoo zoo zoo zoo zoo zoo zoo zoo zoo zoo zoo wrong";
	uint8_t with[OSK_BIP39_SEED_LEN], without[OSK_BIP39_SEED_LEN];

	T_EQ("ohne Passphrase", osk_bip39_to_seed(m, NULL, without), OSK_OK);
	T_EQ("leere Passphrase ist dasselbe wie keine", osk_bip39_to_seed(m, "", with), OSK_OK);
	T_TRUE("gleich", memcmp(with, without, sizeof(with)) == 0);
	T_EQ("mit Passphrase", osk_bip39_to_seed(m, "TREZOR", with), OSK_OK);
	T_TRUE("anderer Seed", memcmp(with, without, sizeof(with)) != 0);
	/* Nicht-ASCII wird abgelehnt statt falsch normalisiert. */
	T_EQ("Umlaut abgelehnt", osk_bip39_to_seed(m, "H\xc3\xa4user", with), OSK_ERR_FORMAT);
}

int main(void)
{
	test_wordlist();
	test_vectors();
	test_bad_input();
	test_generate();
	test_passphrase();
	return t_report("bip39");
}
