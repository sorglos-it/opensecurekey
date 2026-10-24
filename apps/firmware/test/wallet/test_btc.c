/* Bitcoin: Transaktion lesen und wieder schreiben, Skriptarten, Adressen,
 * Hash der alten Art. Die Vergleichswerte sind unabhaengig in Python
 * nachgerechnet; die Adressen sind die Vektoren aus BIP173. */

#include "test_util.h"
#include "wallet_btc.h"

static const char *const LEGACY_TX =
    "0100000001bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb01000000"
    "00ffffffff0150c30000000000001976a914c0cebcd6c3d3ca8c75dc5ec62ebe55330ef910e288ac0000"
    "0000";

static void test_parse_and_back(void)
{
	uint8_t raw[256], back[256];
	osk_btc_tx_t tx;
	size_t len, back_len = 0;

	len = t_unhex(LEGACY_TX, raw, sizeof(raw));
	T_TRUE("gelesen", len > 0);
	T_EQ("Transaktion gelesen", osk_btc_parse_tx(raw, len, &tx), OSK_OK);
	T_EQ("Version", (long long)tx.version, 1);
	T_EQ("ein Eingang", tx.in_count, 1);
	T_EQ("ein Ausgang", tx.out_count, 1);
	T_EQ("vout", (long long)tx.in[0].vout, 1);
	T_EQ("Sequenz", (long long)tx.in[0].sequence, 0xffffffffLL);
	T_EQ("Betrag", (long long)tx.out[0].value, 50000);
	T_EQ("Skriptlaenge", tx.out[0].script_len, 25);
	T_EQ("Sperrzeit", (long long)tx.locktime, 0);

	/* Beim Lesen darf nichts verloren gehen. */
	T_EQ("geschrieben", osk_btc_serialize_tx(&tx, back, sizeof(back), &back_len), OSK_OK);
	T_EQ("gleiche Laenge", back_len, len);
	T_TRUE("gleiche Bytes", memcmp(raw, back, len) == 0);
	T_EQ("Puffer zu klein", osk_btc_serialize_tx(&tx, back, 10, &back_len), OSK_ERR_ARG);
}

static void test_parse_rejects(void)
{
	uint8_t raw[256];
	osk_btc_tx_t tx;
	size_t len;

	/* Rest am Ende. */
	len = t_unhex(LEGACY_TX, raw, sizeof(raw));
	raw[len] = 0xffu;
	T_EQ("Rest am Ende abgelehnt", osk_btc_parse_tx(raw, len + 1u, &tx), OSK_ERR_FORMAT);
	/* Abgeschnitten. */
	T_EQ("abgeschnitten abgelehnt", osk_btc_parse_tx(raw, len - 5u, &tx), OSK_ERR_FORMAT);
	T_EQ("leer abgelehnt", osk_btc_parse_tx(raw, 0, &tx), OSK_ERR_FORMAT);
	/* Eingangsskript in einer unsignierten Transaktion: das ist eine
	 * schon unterschriebene, und die hat hier nichts zu suchen. */
	len = t_unhex("0100000001bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
		      "bbbbbb01000000020102ffffffff0150c30000000000001976a914c0cebcd6c3d3ca8c"
		      "75dc5ec62ebe55330ef910e288ac00000000",
		      raw, sizeof(raw));
	T_EQ("Eingangsskript abgelehnt", osk_btc_parse_tx(raw, len, &tx), OSK_ERR_FORMAT);
	/* Zeugen-Markierung: gehoert nicht in eine unsignierte Transaktion. */
	len = t_unhex("010000000001", raw, sizeof(raw));
	T_EQ("Zeugenform abgelehnt", osk_btc_parse_tx(raw, len, &tx), OSK_ERR_FORMAT);
	/* Betrag ueber 21 Millionen Bitcoin. */
	len = t_unhex("0100000001bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
		      "bbbbbb010000000"
		      "0ffffffff01ffffffffffffffff1976a914c0cebcd6c3d3ca8c75dc5ec62ebe55330ef9"
		      "10e288ac00000000",
		      raw, sizeof(raw));
	T_EQ("unmoeglicher Betrag abgelehnt", osk_btc_parse_tx(raw, len, &tx), OSK_ERR_FORMAT);
}

static void test_varint(void)
{
	uint8_t raw[16];
	uint64_t value = 0;
	size_t pos = 0, len;

	len = t_unhex("fc", raw, sizeof(raw));
	T_TRUE("252 kurz", osk_btc_read_varint(raw, len, &pos, &value) && value == 252u);
	pos = 0;
	len = t_unhex("fdfd00", raw, sizeof(raw));
	T_TRUE("253 lang", osk_btc_read_varint(raw, len, &pos, &value) && value == 253u);
	/* Nicht kuerzeste Form: verboten, sonst gibt es zwei Schreibweisen
	 * fuer dieselbe Zahl - und damit zwei Transaktionskennungen. */
	pos = 0;
	len = t_unhex("fd0100", raw, sizeof(raw));
	T_TRUE("1 als lange Form abgelehnt", !osk_btc_read_varint(raw, len, &pos, &value));
	pos = 0;
	len = t_unhex("fe00000100", raw, sizeof(raw));
	T_TRUE("65536 als 4 Byte geht",
	       osk_btc_read_varint(raw, len, &pos, &value) && value == 65536u);
	pos = 0;
	len = t_unhex("fe10000000", raw, sizeof(raw));
	T_TRUE("16 als 4 Byte abgelehnt", !osk_btc_read_varint(raw, len, &pos, &value));
	pos = 0;
	len = t_unhex("fd", raw, sizeof(raw));
	T_TRUE("abgeschnitten abgelehnt", !osk_btc_read_varint(raw, len, &pos, &value));

	T_EQ("252 schreiben", osk_btc_write_varint(252, raw), 1);
	T_HEX("fc", raw, 1, "fc");
	T_EQ("253 schreiben", osk_btc_write_varint(253, raw), 3);
	T_HEX("fdfd00", raw, 3, "fdfd00");
	T_EQ("65536 schreiben", osk_btc_write_varint(65536, raw), 5);
	T_HEX("fe00000100", raw, 5, "fe00000100");
}

static void test_script_kinds(void)
{
	uint8_t script[64];
	size_t len;

	len = t_unhex("76a914751e76e8199196d454941c45d1b3a323f1433bd688ac", script, sizeof(script));
	T_EQ("P2PKH", osk_btc_script_kind(script, len), OSK_BTC_P2PKH);
	len = t_unhex("a914751e76e8199196d454941c45d1b3a323f1433bd687", script, sizeof(script));
	T_EQ("P2SH", osk_btc_script_kind(script, len), OSK_BTC_P2SH);
	len = t_unhex("0014751e76e8199196d454941c45d1b3a323f1433bd6", script, sizeof(script));
	T_EQ("P2WPKH", osk_btc_script_kind(script, len), OSK_BTC_P2WPKH);
	len = t_unhex("00201863143c14c5166804bd19203356da136c985678cd4d27a1b8c6329604903262",
		      script, sizeof(script));
	T_EQ("P2WSH", osk_btc_script_kind(script, len), OSK_BTC_P2WSH);
	len = t_unhex("512079be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798",
		      script, sizeof(script));
	T_EQ("P2TR", osk_btc_script_kind(script, len), OSK_BTC_P2TR);
	len = t_unhex("6a0548656c6c6f", script, sizeof(script));
	T_EQ("OP_RETURN", osk_btc_script_kind(script, len), OSK_BTC_OPRETURN);
	len = t_unhex("5102aabb", script, sizeof(script));
	T_EQ("Unbekanntes", osk_btc_script_kind(script, len), OSK_BTC_UNKNOWN);
	T_EQ("leer", osk_btc_script_kind(script, 0), OSK_BTC_UNKNOWN);
}

static void test_addresses(void)
{
	uint8_t script[64];
	char text[OSK_BTC_ADDRESS_MAX];
	size_t len, text_len = 0;

	len = t_unhex("76a914751e76e8199196d454941c45d1b3a323f1433bd688ac", script, sizeof(script));
	T_EQ("P2PKH-Adresse",
	     osk_btc_address_from_script(script, len, "bc", 0x00u, 0x05u, text, sizeof(text),
					 &text_len),
	     OSK_OK);
	T_TRUE("stimmt", strcmp(text, "1BgGZ9tcN4rm9KBzDn7KprQz87SZ26SAMH") == 0);

	len = t_unhex("0014751e76e8199196d454941c45d1b3a323f1433bd6", script, sizeof(script));
	T_EQ("P2WPKH-Adresse",
	     osk_btc_address_from_script(script, len, "bc", 0x00u, 0x05u, text, sizeof(text),
					 &text_len),
	     OSK_OK);
	T_TRUE("stimmt", strcmp(text, "bc1qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t4") == 0);

	len = t_unhex("512079be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798",
		      script, sizeof(script));
	T_EQ("Taproot-Adresse",
	     osk_btc_address_from_script(script, len, "bc", 0x00u, 0x05u, text, sizeof(text),
					 &text_len),
	     OSK_OK);
	T_TRUE("stimmt",
	       strcmp(text, "bc1p0xlxvlhemja6c4dqv22uapctqupfhlxm9h8z3k2e72q4k9hcz7vqzk5jj0") == 0);

	/* Testnetz: dieselbe Zahl, anderes Vorwort. */
	len = t_unhex("0014751e76e8199196d454941c45d1b3a323f1433bd6", script, sizeof(script));
	T_EQ("Testnetz",
	     osk_btc_address_from_script(script, len, "tb", 0x6fu, 0xc4u, text, sizeof(text),
					 &text_len),
	     OSK_OK);
	T_TRUE("stimmt", strcmp(text, "tb1qw508d6qejxtdg4y5r3zarvary0c5xw7kxpjzsx") == 0);

	/* OP_RETURN hat keine Adresse - und darum darf nicht unterschrieben
	 * werden, ohne dass der Nutzer das ausdruecklich erlaubt. */
	len = t_unhex("6a0548656c6c6f", script, sizeof(script));
	T_EQ("OP_RETURN hat keine Adresse",
	     osk_btc_address_from_script(script, len, "bc", 0x00u, 0x05u, text, sizeof(text),
					 &text_len),
	     OSK_ERR_UNSUPPORTED);
}

static void test_scripts(void)
{
	uint8_t hash[20], script[32];
	size_t len = 0;

	T_EQ("hash160", t_unhex("751e76e8199196d454941c45d1b3a323f1433bd6", hash, sizeof(hash)),
	     20);
	T_EQ("P2PKH gebaut", osk_btc_script_p2pkh(hash, script, sizeof(script), &len), OSK_OK);
	T_EQ("25 Byte", len, 25);
	T_HEX("Skript", script, len, "76a914751e76e8199196d454941c45d1b3a323f1433bd688ac");
	T_EQ("P2WPKH gebaut", osk_btc_script_p2wpkh(hash, script, sizeof(script), &len), OSK_OK);
	T_EQ("22 Byte", len, 22);
	T_HEX("Skript", script, len, "0014751e76e8199196d454941c45d1b3a323f1433bd6");
	T_EQ("Puffer zu klein", osk_btc_script_p2pkh(hash, script, 10, &len), OSK_ERR_ARG);
}

static void test_sighash(void)
{
	uint8_t raw[256], script[32], hash[32];
	osk_btc_tx_t tx;
	size_t len, script_len;

	len = t_unhex(LEGACY_TX, raw, sizeof(raw));
	T_EQ("gelesen", osk_btc_parse_tx(raw, len, &tx), OSK_OK);
	script_len =
	    t_unhex("76a914751e76e8199196d454941c45d1b3a323f1433bd688ac", script, sizeof(script));
	T_EQ("Hash der alten Art",
	     osk_btc_sighash_legacy(&tx, 0, script, script_len, OSK_BTC_SIGHASH_ALL, hash), OSK_OK);
	T_HEX("stimmt", hash, 32,
	      "61ad23e291eaf8b34667b5c6a0932e35d40ecdd0f97fc8ac2649cf19a6b95b0a");

	/* Nur SIGHASH_ALL - alles andere laesst Teile austauschbar, und das
	 * kann auf einem kleinen Schirm niemand beurteilen. */
	T_EQ("SIGHASH_NONE abgelehnt",
	     osk_btc_sighash_legacy(&tx, 0, script, script_len, 0x02u, hash), OSK_ERR_UNSUPPORTED);
	T_EQ("SIGHASH_ANYONECANPAY abgelehnt",
	     osk_btc_sighash_legacy(&tx, 0, script, script_len, 0x81u, hash), OSK_ERR_UNSUPPORTED);
	T_EQ("Eingang gibt es nicht",
	     osk_btc_sighash_legacy(&tx, 5, script, script_len, OSK_BTC_SIGHASH_ALL, hash),
	     OSK_ERR_ARG);
	T_EQ("Skript zu lang",
	     osk_btc_sighash_segwit(&tx, 0, script, OSK_BTC_SCRIPT_MAX + 1u, 1000,
				    OSK_BTC_SIGHASH_ALL, hash),
	     OSK_ERR_ARG);

	/* Ein anderer Betrag muss einen anderen Hash ergeben - das ist der
	 * ganze Sinn von BIP143. */
	{
		uint8_t a[32], b[32];

		T_EQ("Hash mit 1000",
		     osk_btc_sighash_segwit(&tx, 0, script, script_len, 1000, OSK_BTC_SIGHASH_ALL,
					    a),
		     OSK_OK);
		T_EQ("Hash mit 1001",
		     osk_btc_sighash_segwit(&tx, 0, script, script_len, 1001, OSK_BTC_SIGHASH_ALL,
					    b),
		     OSK_OK);
		T_TRUE("verschieden", memcmp(a, b, 32) != 0);
	}
}

int main(void)
{
	test_parse_and_back();
	test_parse_rejects();
	test_varint();
	test_script_kinds();
	test_addresses();
	test_scripts();
	test_sighash();
	return t_report("btc");
}
