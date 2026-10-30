/* Ethereum: EIP-155, EIP-1559, EIP-191, EIP-712 und EIP-55.
 *
 * Die Transaktion der alten Art ist das Beispiel aus EIP-155 selbst
 * (Nonce 9, 20 Gwei, 21000 Gas, 1 Ether an 0x3535..., Kette 1); Bytes und
 * Hash stehen dort. Die uebrigen Werte sind unabhaengig in Python
 * nachgerechnet. Pruefdaten, kein Fremdcode. */

#include "test_util.h"
#include "wallet_eth.h"
#include "wallet_codec.h"
#include "wallet_signer.h"
#include "wallet_seed.h"
#include "mocks.h"

static const char *const LEGACY_RLP =
    "ec098504a817c800825208943535353535353535353535353535353535353535880de0b6b3a7640000"
    "80018080";
static const char *const LEGACY_HASH =
    "daf5a779ae972f972197303d7b574746c7ef83eadac0f2791ad23db92e4c8e53";
static const char *const FEEMARKET_RLP =
    "02f00109843b9aca008506fc23ac00825208943535353535353535353535353535353535353535880de0"
    "b6b3a764000080c0";
static const char *const FEEMARKET_HASH =
    "61b515f42ee083d21625f1465de93314f0df919cee4e9689347e3e6387f4eeea";
static const char *const MNEMONIC =
    "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon"
    " about";

static void test_legacy(void)
{
	uint8_t raw[128], hash[32];
	osk_eth_tx_t tx;
	osk_wallet_view_t view;
	size_t len = t_unhex(LEGACY_RLP, raw, sizeof(raw));

	T_TRUE("gelesen", len > 0);
	T_EQ("Transaktion gelesen", osk_eth_parse_tx(raw, len, &tx), OSK_OK);
	T_EQ("alte Art", tx.kind, OSK_ETH_LEGACY);
	T_EQ("Nonce", (long long)tx.nonce, 9);
	T_EQ("Gasgrenze", (long long)tx.gas_limit, 21000);
	T_EQ("Kettennummer", (long long)tx.chain_id, 1);
	T_TRUE("Empfaenger da", tx.has_to);
	T_HEX("Empfaenger", tx.to, 20, "3535353535353535353535353535353535353535");
	T_HEX("Betrag", tx.value, 32,
	      "0000000000000000000000000000000000000000000000000de0b6b3a7640000");
	T_HEX("Gaspreis", tx.max_fee, 32,
	      "00000000000000000000000000000000000000000000000000000004a817c800");
	T_EQ("keine Daten", tx.data_len, 0);

	T_EQ("Hash", osk_eth_tx_hash(raw, len, hash), OSK_OK);
	T_HEX("Hash nach EIP-155", hash, 32, LEGACY_HASH);

	T_EQ("Ansicht", osk_eth_view(&tx, "ETH", &view), OSK_OK);
	T_TRUE("lesbar", view.readable);
	T_TRUE("Betrag", strcmp(view.amount, "1.0 ETH") == 0);
	if (strcmp(view.amount, "1.0 ETH") != 0)
		printf("  Betrag %s\n", view.amount);
	/* 21000 * 20 Gwei = 0,00042 Ether */
	T_TRUE("Gebuehr", strcmp(view.fee, "0.00042 ETH") == 0);
	if (strcmp(view.fee, "0.00042 ETH") != 0)
		printf("  Gebuehr %s\n", view.fee);
	T_TRUE("Kette steht dabei", strcmp(view.extra, "chain 1") == 0);
	T_TRUE("Adresse mit Pruefsumme",
	       strcmp(view.recipient, "0x3535353535353535353535353535353535353535") == 0);
}

static void test_feemarket(void)
{
	uint8_t raw[128], hash[32];
	osk_eth_tx_t tx;
	osk_wallet_view_t view;
	size_t len = t_unhex(FEEMARKET_RLP, raw, sizeof(raw));

	T_TRUE("gelesen", len > 0);
	T_EQ("Transaktion gelesen", osk_eth_parse_tx(raw, len, &tx), OSK_OK);
	T_EQ("neue Art", tx.kind, OSK_ETH_FEEMARKET);
	T_EQ("Kettennummer", (long long)tx.chain_id, 1);
	T_EQ("Nonce", (long long)tx.nonce, 9);
	T_HEX("Hoechstgebuehr", tx.max_fee, 32,
	      "00000000000000000000000000000000000000000000000000000006fc23ac00");
	T_HEX("Vorrangsgebuehr", tx.max_priority, 32,
	      "000000000000000000000000000000000000000000000000000000003b9aca00");
	T_TRUE("keine Zugriffsliste", !tx.has_access_list);
	T_EQ("Hash", osk_eth_tx_hash(raw, len, hash), OSK_OK);
	T_HEX("Hash", hash, 32, FEEMARKET_HASH);
	T_EQ("Ansicht", osk_eth_view(&tx, "ETH", &view), OSK_OK);
	T_TRUE("lesbar", view.readable);
	/* 21000 * 30 Gwei = 0,00063 Ether - gerechnet wird mit der
	 * Hoechstgebuehr, denn mehr kann es nicht werden. */
	T_TRUE("Gebuehr", strcmp(view.fee, "0.00063 ETH") == 0);
	if (strcmp(view.fee, "0.00063 ETH") != 0)
		printf("  Gebuehr %s\n", view.fee);
}

static void test_rejects(void)
{
	uint8_t raw[160];
	osk_eth_tx_t tx;
	size_t len;

	/* Alte Art OHNE Kettennummer (sechs Werte): abgelehnt, weil dieselbe
	 * Unterschrift sonst auf jeder EVM-Kette gilt. */
	len = t_unhex("e9098504a817c80082520894353535353535353535353535353535353535353588"
		      "0de0b6b3a764000080",
		      raw, sizeof(raw));
	T_EQ("ohne Kettennummer abgelehnt", osk_eth_parse_tx(raw, len, &tx), OSK_ERR_FORMAT);

	/* Kettennummer 0 gibt es nicht. */
	len = t_unhex("ec098504a817c800825208943535353535353535353535353535353535353535"
		      "880de0b6b3a764000080808080",
		      raw, sizeof(raw));
	T_EQ("Kette 0 abgelehnt", osk_eth_parse_tx(raw, len, &tx), OSK_ERR_UNSUPPORTED);

	/* Unbekanntes Typbyte. */
	len = t_unhex("03c0", raw, sizeof(raw));
	T_EQ("Typ 3 abgelehnt", osk_eth_parse_tx(raw, len, &tx), OSK_ERR_UNSUPPORTED);
	len = t_unhex("7fc0", raw, sizeof(raw));
	T_EQ("Typ 0x7f abgelehnt", osk_eth_parse_tx(raw, len, &tx), OSK_ERR_UNSUPPORTED);

	/* Rest hinter der Liste. */
	len = t_unhex("ec098504a817c800825208943535353535353535353535353535353535353535"
		      "880de0b6b3a7640000800180"
		      "80"
		      "ff",
		      raw, sizeof(raw));
	T_EQ("Rest dahinter abgelehnt", osk_eth_parse_tx(raw, len, &tx), OSK_ERR_FORMAT);

	/* Eine Adresse mit 19 Byte ist keine. */
	len = t_unhex("eb098504a817c8008252089435353535353535353535353535353535353535"
		      "880de0b6b3a764000080018080",
		      raw, sizeof(raw));
	T_EQ("kurze Adresse abgelehnt", osk_eth_parse_tx(raw, len, &tx), OSK_ERR_FORMAT);

	T_EQ("leer abgelehnt", osk_eth_parse_tx(raw, 0, &tx), OSK_ERR_ARG);
}

static void test_contract_call(void)
{
	uint8_t raw[160];
	osk_eth_tx_t tx;
	osk_wallet_view_t view;
	size_t len;

	/* Dieselbe Transaktion, aber mit vier Byte Daten: ein
	 * Vertragsaufruf. Betrag und Adresse sagen dann nicht, was
	 * geschieht - also nicht lesbar. */
	len = t_unhex("f0098504a817c800825208943535353535353535353535353535353535353535"
		      "880de0b6b3a764000084a9059cbb018080",
		      raw, sizeof(raw));
	T_EQ("gelesen", osk_eth_parse_tx(raw, len, &tx), OSK_OK);
	T_EQ("vier Byte Daten", tx.data_len, 4);
	T_EQ("Ansicht", osk_eth_view(&tx, "ETH", &view), OSK_OK);
	T_TRUE("nicht lesbar", !view.readable);
	T_TRUE("Grund genannt", strcmp(view.missing_key, "wallet.vertragsaufruf") == 0);
}

static void test_address(void)
{
	char text[64];
	uint8_t raw[20];
	osk_bip32_priv_t master, key;
	uint8_t seed[64];
	uint8_t address[OSK_ETH_ADDRESS_LEN];

	/* EIP-55: die Pruefsumme steckt in Gross- und Kleinschreibung. */
	T_EQ("Adresse gelesen",
	     t_unhex("5aaeb6053f3e94c9b9a09f33669435e7ef1beaed", raw, sizeof(raw)), 20);
	T_EQ("Text", osk_eth_address_text(raw, text, sizeof(text)), OSK_OK);
	T_TRUE("EIP-55 Fall 1", strcmp(text, "0x5aAeb6053F3E94C9b9A09f33669435E7Ef1BeAed") == 0);
	if (strcmp(text, "0x5aAeb6053F3E94C9b9A09f33669435E7Ef1BeAed") != 0)
		printf("  bekam %s\n", text);
	T_EQ("Adresse gelesen",
	     t_unhex("fb6916095ca1df60bb79ce92ce3ea74c37c5d359", raw, sizeof(raw)), 20);
	T_EQ("Text", osk_eth_address_text(raw, text, sizeof(text)), OSK_OK);
	T_TRUE("EIP-55 Fall 2", strcmp(text, "0xfB6916095ca1df60bB79Ce92cE3Ea74c37c5d359") == 0);
	T_EQ("zu kleiner Puffer", osk_eth_address_text(raw, text, 10), OSK_ERR_ARG);

	/* Und die Adresse zum bekannten Seed: m/44'/60'/0'/0/0 der Mnemonik
	 * "abandon ... about" ist eine der meistgeprueften Adressen
	 * ueberhaupt. */
	T_EQ("Seed", osk_bip39_to_seed(MNEMONIC, NULL, seed), OSK_OK);
	T_EQ("Wurzel", osk_bip32_master(seed, sizeof(seed), OSK_BIP32_SECP256K1, &master), OSK_OK);
	T_EQ("Pfad", osk_bip32_derive_path(&master, "m/44'/60'/0'/0/0", &key), OSK_OK);
	T_EQ("Adresse", osk_eth_address(&key, address), OSK_OK);
	T_HEX("Adresse stimmt", address, 20, "9858effd232b4033e47d90003d41ec34ecaeda94");
	T_EQ("Text", osk_eth_address_text(address, text, sizeof(text)), OSK_OK);
	T_TRUE("mit Pruefsumme", strcmp(text, "0x9858EfFD232B4033E47d90003D41EC34EcaEda94") == 0);
	osk_bip32_wipe(&key);
	osk_bip32_wipe(&master);
}

static void test_message_hash(void)
{
	uint8_t hash[32];

	T_EQ("Nachrichtenhash", osk_eth_message_hash((const uint8_t *)"Hello, Sorglos!", 15, hash),
	     OSK_OK);
	T_HEX("EIP-191", hash, 32,
	      "4e532cf1730b24ccc59eeb8c2c07fe50782015122d12e0069b5778b4cb666f5d");
	T_EQ("zu lange Nachricht abgelehnt", osk_eth_message_hash((const uint8_t *)"x", 513, hash),
	     OSK_ERR_FULL);
}

static void test_typed_hash(void)
{
	uint8_t domain[32], message[32], hash[32];

	for (unsigned i = 0; i < 32; i++) {
		domain[i] = (uint8_t)i;
		message[i] = (uint8_t)(32u + i);
	}
	T_EQ("EIP-712", osk_eth_typed_hash(domain, message, hash), OSK_OK);
	T_HEX("Hash", hash, 32, "71d794446d7c48f892ac3d70ffeb3b889a61afd745fe8bd250056298d7510228");
}

static osk_item_id_t prepare(void)
{
	osk_item_id_t id = 0;

	mock_store_reset();
	mock_se050_reset();
	mock_ui_reset();
	T_EQ("Seed in den Tresor", osk_wallet_seed_store(0, "Ether", MNEMONIC, NULL, &id), OSK_OK);
	return id;
}

static void test_sign(void)
{
	uint8_t raw[128], hash[32], r[32], s[32];
	uint64_t v = 0;
	size_t len = t_unhex(LEGACY_RLP, raw, sizeof(raw));
	osk_item_id_t id = prepare();
	osk_bip32_priv_t master, key;
	uint8_t seed[64];
	osk_ec_point_t point;

	mock_ui_answer(OSK_OK);
	T_EQ("unterschrieben",
	     osk_eth_sign_tx(raw, len, id, "test", "m/44'/60'/0'/0/0", "ETH", r, s, &v),
	     OSK_OK);
	/* EIP-155: v = Kennziffer + 35 + 2 * Kettennummer, also 37 oder 38. */
	T_TRUE("v ist 37 oder 38", v == 37u || v == 38u);
	T_EQ("Freigabe gefragt", mock_ui_calls(), 1);
	T_TRUE("Betrag stand auf dem Schirm",
	       strcmp(mock_ui_detail(), "1.0 ETH + 0.00042 ETH / chain 1") == 0);
	if (strcmp(mock_ui_detail(), "1.0 ETH + 0.00042 ETH / chain 1") != 0)
		printf("  Zeile: %s\n", mock_ui_detail());

	/* Die Unterschrift muss zum Schluessel und zum Hash passen. */
	T_EQ("Seed", osk_bip39_to_seed(MNEMONIC, NULL, seed), OSK_OK);
	T_EQ("Wurzel", osk_bip32_master(seed, sizeof(seed), OSK_BIP32_SECP256K1, &master), OSK_OK);
	T_EQ("Pfad", osk_bip32_derive_path(&master, "m/44'/60'/0'/0/0", &key), OSK_OK);
	{
		osk_ec_point_t own;

		T_EQ("Punkt", osk_ec_public_from_private(key.key, &own), OSK_OK);
		point = own;
	}
	T_EQ("Hash", osk_eth_tx_hash(raw, len, hash), OSK_OK);
	T_TRUE("Unterschrift passt", osk_ec_verify(hash, r, s, &point));
	T_TRUE("s ist klein", !osk_ec_s_is_high(s));
	{
		uint8_t recid = 0xff;

		T_EQ("Kennziffer wiederfindbar", osk_ec_recovery_id(hash, r, s, &point, &recid),
		     OSK_OK);
		T_TRUE("passt zu v", (uint64_t)recid + 35u + 2u == v);
	}
	osk_bip32_wipe(&key);
	osk_bip32_wipe(&master);
}

static void test_sign_refusals(void)
{
	uint8_t raw[160], r[32], s[32];
	uint64_t v = 0;
	osk_item_id_t id;
	size_t len;

	/* Vertragsaufruf ohne Extrafreigabe: nichts wird unterschrieben, und
	 * der Chip wird nicht einmal gefragt. */
	id = prepare();
	len = t_unhex("f0098504a817c800825208943535353535353535353535353535353535353535"
		      "880de0b6b3a764000084a9059cbb018080",
		      raw, sizeof(raw));
	mock_ui_answer(OSK_OK);
	T_EQ("Vertragsaufruf abgelehnt",
	     osk_eth_sign_tx(raw, len, id, "test", "m/44'/60'/0'/0/0", "ETH", r, s, &v),
	     OSK_ERR_DENIED);
	T_EQ("Chip nicht gefragt", mock_se050_imports(), 0);
	T_TRUE("Grund stand auf dem Schirm",
	       strcmp(mock_ui_status_key(), "wallet.vertragsaufruf") == 0);

	/* Und es bleibt dabei. Ein Vertragsaufruf mit Daten, die das Geraet
	 * nicht aufschluesseln kann, wird abgelehnt - immer, nicht nur beim
	 * ersten Mal und nicht nur ohne Freigabe. Es gibt keine Freigabe.
	 *
	 * Das ist der Fall aus dem Auftrag: lieber ablehnen als
	 * naeherungsweise anzeigen. Eine Anzeige, die "ungefaehr" stimmt,
	 * ist schlimmer als eine Ablehnung, weil der Nutzer ihr glaubt. */
	for (unsigned i = 0; i < 5u; i++) {
		id = prepare();
		mock_ui_reset();
		mock_ui_answer(OSK_OK); /* der Nutzer WUERDE ja sagen */
		T_EQ("Vertragsaufruf bleibt abgelehnt",
		     osk_eth_sign_tx(raw, len, id, "test", "m/44'/60'/0'/0/0", "ETH", r, s, &v),
		     OSK_ERR_DENIED);
		T_EQ("Chip blieb unangetastet", mock_se050_imports(), 0);
		T_EQ("es wurde nicht gefragt", (long long)mock_ui_calls(), 0);
	}

	/* EIP-712 in dieser Form ist immer blind: der Aufruf bekommt zwei
	 * Hashes - Bereichstrenner und Strukturhash - und aus Hashes kann
	 * niemand zurueckrechnen, was da stand. Also wird nicht
	 * unterschrieben, und zwar nie.
	 *
	 * Das kostet Vertraeglichkeit mit MetaMask; der Preis steht in
	 * docs/wallet-bau.md. */
	{
		uint8_t domain[32], message[32];

		memset(domain, 1, sizeof(domain));
		memset(message, 2, sizeof(message));

		for (unsigned i = 0; i < 5u; i++) {
			id = prepare();
			mock_ui_reset();
			mock_ui_answer(OSK_OK); /* der Nutzer WUERDE ja sagen */
			T_EQ("EIP-712 abgelehnt",
			     osk_eth_sign_typed(domain, message, id, "test",
						"m/44'/60'/0'/0/0", r, s, &v),
			     OSK_ERR_DENIED);
			T_TRUE("Grund genannt",
			       strcmp(mock_ui_status_key(),
				      "wallet.eip712-inhalt-unbekannt") == 0);
			T_EQ("Chip blieb unangetastet", mock_se050_imports(), 0);
			T_EQ("es wurde nicht gefragt", (long long)mock_ui_calls(), 0);
		}
	}

	/* Eine lesbare Nachricht steht im Klartext auf dem Schirm. */
	id = prepare();
	mock_ui_answer(OSK_OK);
	T_EQ("Nachricht unterschrieben",
	     osk_eth_sign_message((const uint8_t *)"Hello, Sorglos!", 15, id, "test",
				  "m/44'/60'/0'/0/0", r, s, &v),
	     OSK_OK);
	T_TRUE("Text stand da", strcmp(mock_ui_subject(), "Hello, Sorglos!") == 0);
	T_TRUE("v ist 27 oder 28", v == 27u || v == 28u);

	/* Eine Nachricht mit Steuerzeichen nicht. */
	id = prepare();
	mock_ui_answer(OSK_OK);
	T_EQ("unlesbare Nachricht abgelehnt",
	     osk_eth_sign_message((const uint8_t *)"a\x01b", 3, id, "test", "m/44'/60'/0'/0/0",
				  r, s, &v),
	     OSK_ERR_DENIED);
}

int main(void)
{
	test_legacy();
	test_feemarket();
	test_rejects();
	test_contract_call();
	test_address();
	test_message_hash();
	test_typed_hash();
	test_sign();
	test_sign_refusals();
	return t_report("eth");
}
