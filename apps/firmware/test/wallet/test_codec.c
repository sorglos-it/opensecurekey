/* Schreibweisen: Base58Check, Bech32, Bech32m, EIP-55, Hex.
 *
 * Die Adressen sind die Testvektoren aus BIP173 und BIP350; die
 * Base58Check-Werte sind unabhaengig nachgerechnet. Pruefdaten, kein
 * Fremdcode. */

#include "test_util.h"
#include "wallet_codec.h"

static void test_base58(void)
{
	uint8_t raw[32], back[96];
	char text[OSK_BASE58_MAX_TEXT];
	size_t len = 0, back_len = 0;

	raw[0] = 0x00u;
	T_EQ("hash160 gelesen",
	     t_unhex("751e76e8199196d454941c45d1b3a323f1433bd6", raw + 1, sizeof(raw) - 1), 20);
	T_EQ("kodiert", osk_base58check_encode(raw, 21, text, sizeof(text), &len), OSK_OK);
	T_TRUE("P2PKH-Adresse", strcmp(text, "1BgGZ9tcN4rm9KBzDn7KprQz87SZ26SAMH") == 0);
	if (strcmp(text, "1BgGZ9tcN4rm9KBzDn7KprQz87SZ26SAMH") != 0)
		printf("  bekam %s\n", text);
	T_EQ("gelesen", osk_base58check_decode(text, back, sizeof(back), &back_len), OSK_OK);
	T_EQ("21 Byte", back_len, 21);
	T_TRUE("gleich", memcmp(raw, back, 21) == 0);

	raw[0] = 0x05u;
	T_EQ("kodiert", osk_base58check_encode(raw, 21, text, sizeof(text), &len), OSK_OK);
	T_TRUE("P2SH-Adresse", strcmp(text, "3CNHUhP3uyB9EUtRLsmvFUmvGdjGdkTxJw") == 0);

	/* Fuehrende Nullen muessen als '1' erhalten bleiben - genau daran
	 * scheitern Umsetzungen gern. */
	memset(raw, 0, 5);
	T_EQ("kodiert", osk_base58check_encode(raw, 5, text, sizeof(text), &len), OSK_OK);
	T_TRUE("fuenf Nullen", strcmp(text, "1111146Q4wc") == 0);
	if (strcmp(text, "1111146Q4wc") != 0)
		printf("  bekam %s\n", text);
	T_EQ("gelesen", osk_base58check_decode(text, back, sizeof(back), &back_len), OSK_OK);
	T_EQ("fuenf Byte", back_len, 5);
	T_TRUE("alles null", back[0] == 0 && back[4] == 0);

	/* Verbogene Pruefsumme, fremdes Zeichen, zu kurz. */
	T_EQ("Pruefsumme falsch",
	     osk_base58check_decode("1BgGZ9tcN4rm9KBzDn7KprQz87SZ26SAMG", back, sizeof(back),
				    &back_len),
	     OSK_ERR_FORMAT);
	T_EQ("Null gibt es nicht im Alphabet",
	     osk_base58check_decode("0BgGZ9tcN4rm9KBzDn7KprQz87SZ26SAMH", back, sizeof(back),
				    &back_len),
	     OSK_ERR_FORMAT);
	T_EQ("zu kurz", osk_base58check_decode("1234", back, sizeof(back), &back_len),
	     OSK_ERR_FORMAT);
	T_EQ("leer", osk_base58check_decode("", back, sizeof(back), &back_len), OSK_ERR_FORMAT);
	T_EQ("Puffer zu klein", osk_base58check_encode(raw, 5, text, 4, &len), OSK_ERR_ARG);
}

struct address_case {
	const char *hrp;
	uint8_t version;
	const char *program;
	const char *text;
};

static const struct address_case ADDRESSES[] = {
    {"bc", 0, "751e76e8199196d454941c45d1b3a323f1433bd6",
     "bc1qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t4"},
    {"bc", 0, "1863143c14c5166804bd19203356da136c985678cd4d27a1b8c6329604903262",
     "bc1qrp33g0q5c5txsp9arysrx4k6zdkfs4nce4xj0gdcccefvpysxf3qccfmv3"},
    {"bc", 1, "79be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798",
     "bc1p0xlxvlhemja6c4dqv22uapctqupfhlxm9h8z3k2e72q4k9hcz7vqzk5jj0"},
    {"tb", 0, "751e76e8199196d454941c45d1b3a323f1433bd6",
     "tb1qw508d6qejxtdg4y5r3zarvary0c5xw7kxpjzsx"},
};

static void test_bech32(void)
{
	for (size_t i = 0; i < sizeof(ADDRESSES) / sizeof(ADDRESSES[0]); i++) {
		uint8_t program[40], back[40];
		char text[128];
		size_t len = 0, program_len, back_len = 0;
		uint8_t version = 0xff;

		program_len = t_unhex(ADDRESSES[i].program, program, sizeof(program));
		T_TRUE("Programm gelesen", program_len > 0);
		T_EQ("kodiert",
		     osk_bech32_encode_address(ADDRESSES[i].hrp, ADDRESSES[i].version, program,
					       program_len, text, sizeof(text), &len),
		     OSK_OK);
		if (strcmp(text, ADDRESSES[i].text) != 0)
			printf("FEHLER\n  bekam    %s\n  erwartet %s\n", text, ADDRESSES[i].text);
		T_TRUE("Adresse stimmt", strcmp(text, ADDRESSES[i].text) == 0);
		T_EQ("gelesen",
		     osk_bech32_decode_address(ADDRESSES[i].text, ADDRESSES[i].hrp, &version, back,
					       sizeof(back), &back_len),
		     OSK_OK);
		T_EQ("Version stimmt", version, ADDRESSES[i].version);
		T_EQ("Laenge stimmt", back_len, program_len);
		T_TRUE("Programm stimmt", memcmp(program, back, program_len) == 0);
	}
}

static void test_bech32_rejects(void)
{
	uint8_t program[40];
	uint8_t version = 0;
	size_t len = 0;
	char text[128];
	uint8_t prog20[20];

	memset(prog20, 0x11, sizeof(prog20));

	/* Gemischte Gross- und Kleinschreibung ist nach BIP173 verboten. */
	T_EQ("gemischt abgelehnt",
	     osk_bech32_decode_address("bc1QW508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t4", "bc", &version,
				       program, sizeof(program), &len),
	     OSK_ERR_FORMAT);
	/* Ganz gross ist erlaubt. */
	T_EQ("ganz gross geht",
	     osk_bech32_decode_address("BC1QW508D6QEJXTDG4Y5R3ZARVARY0C5XW7KV8F3T4", "bc", &version,
				       program, sizeof(program), &len),
	     OSK_OK);
	/* Verbogene Pruefsumme. */
	T_EQ("Pruefsumme falsch",
	     osk_bech32_decode_address("bc1qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t5", "bc", &version,
				       program, sizeof(program), &len),
	     OSK_ERR_FORMAT);
	/* Falsches Vorwort. */
	T_EQ("fremdes Vorwort",
	     osk_bech32_decode_address("tb1qw508d6qejxtdg4y5r3zarvary0c5xw7kxpjzsx", "bc", &version,
				       program, sizeof(program), &len),
	     OSK_ERR_FORMAT);
	/* Version 1 mit der alten Pruefsummenart (Bech32 statt Bech32m) ist
	 * nach BIP350 ungueltig - und umgekehrt. */
	T_EQ("Version 1 mit alter Pruefsumme",
	     osk_bech32_decode_address("bc1pw508d6qejxtdg4y5r3zarvary0c5xw7kw508d6qejxtdg4y5r"
				       "3zarvary0c5xw7k7grplx",
				       "bc", &version, program, sizeof(program), &len),
	     OSK_ERR_FORMAT);
	/* Kein Trennzeichen. */
	T_EQ("ohne Trennzeichen",
	     osk_bech32_decode_address("qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t4", "bc", &version,
				       program, sizeof(program), &len),
	     OSK_ERR_FORMAT);
	T_EQ("leer", osk_bech32_decode_address("", "bc", &version, program, sizeof(program), &len),
	     OSK_ERR_FORMAT);

	/* Beim Schreiben: Version 0 kennt nur 20 und 32 Byte. */
	T_EQ("Version 0 mit 21 Byte abgelehnt",
	     osk_bech32_encode_address("bc", 0, prog20, 21, text, sizeof(text), &len), OSK_ERR_ARG);
	T_EQ("Version 17 gibt es nicht",
	     osk_bech32_encode_address("bc", 17, prog20, 20, text, sizeof(text), &len),
	     OSK_ERR_ARG);
	T_EQ("Puffer zu klein", osk_bech32_encode_address("bc", 0, prog20, 20, text, 10, &len),
	     OSK_ERR_ARG);
}

static void test_hex(void)
{
	uint8_t out[8];
	size_t len = 0;

	T_EQ("gelesen", osk_hex_decode("00ff10", 6, out, sizeof(out), &len), OSK_OK);
	T_EQ("drei Byte", len, 3);
	T_HEX("Inhalt", out, 3, "00ff10");
	T_EQ("Grossbuchstaben gehen", osk_hex_decode("ABCD", 4, out, sizeof(out), &len), OSK_OK);
	T_EQ("krumme Laenge", osk_hex_decode("abc", 3, out, sizeof(out), &len), OSK_ERR_FORMAT);
	T_EQ("fremdes Zeichen", osk_hex_decode("ab!d", 4, out, sizeof(out), &len), OSK_ERR_FORMAT);
	T_EQ("Puffer zu klein", osk_hex_decode("00112233445566778899", 20, out, sizeof(out), &len),
	     OSK_ERR_ARG);
}

int main(void)
{
	test_base58();
	test_bech32();
	test_bech32_rejects();
	test_hex();
	return t_report("codec");
}
