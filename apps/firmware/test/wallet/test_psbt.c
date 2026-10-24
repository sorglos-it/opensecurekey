/* PSBT: die Vektoren aus BIP174 und ein vollstaendiger Durchlauf
 * "lesen - anzeigen - unterschreiben - nachpruefen".
 *
 * Das PSBT fuer den Durchlauf ist mit einer unabhaengigen Rechnung in
 * Python gebaut: Seed aus der BIP39-Mnemonik "abandon ... about" ohne
 * Passphrase, Pfad m/84'/0'/0'/0/0, ein Eingang ueber 100 000 Satoshi,
 * 60 000 an eine fremde Adresse, 30 000 als Wechselgeld zurueck - also
 * 10 000 Gebuehr. Der erwartete BIP143-Hash steht ebenfalls dort. */

#include "test_util.h"
#include "psbt_vectors.h"
#include "wallet_psbt.h"
#include "wallet_seed.h"
#include "wallet_signer.h"
#include "wallet_codec.h"
#include "mocks.h"

/* t_unhex aus test_util.h reicht fuer die langen Vektoren nicht (2048
 * Zeichen). Darum hier ein eigener Leser mit groesserem Puffer. */
static size_t big_unhex(const char *in, uint8_t *out, size_t cap)
{
	size_t n = 0;

	for (const char *s = in; *s != '\0'; s += 2) {
		unsigned value = 0;

		if (s[1] == '\0')
			return 0;
		for (unsigned k = 0; k < 2; k++) {
			char c = s[k];

			value <<= 4;
			if (c >= '0' && c <= '9')
				value |= (unsigned)(c - '0');
			else if (c >= 'a' && c <= 'f')
				value |= (unsigned)(c - 'a') + 10u;
			else if (c >= 'A' && c <= 'F')
				value |= (unsigned)(c - 'A') + 10u;
			else
				return 0;
		}
		if (n >= cap)
			return 0;
		out[n++] = (uint8_t)value;
	}
	return n;
}

static uint8_t raw[4096];

static void test_invalid(void)
{
	for (size_t i = 0; i < sizeof(PSBT_INVALID) / sizeof(PSBT_INVALID[0]); i++) {
		osk_psbt_t psbt;
		size_t len = big_unhex(PSBT_INVALID[i].hex, raw, sizeof(raw));
		osk_result_t res;

		T_TRUE("Vektor gelesen", len > 0);
		res = osk_psbt_parse(raw, len, &psbt);
		if (res == OSK_OK)
			t_fail("ungueltiges PSBT abgelehnt", __FILE__, __LINE__,
			       PSBT_INVALID[i].name);
		T_TRUE("abgelehnt", res != OSK_OK);
	}
}

static void test_valid(void)
{
	for (size_t i = 0; i < sizeof(PSBT_VALID) / sizeof(PSBT_VALID[0]); i++) {
		osk_psbt_t psbt;
		size_t len = big_unhex(PSBT_VALID[i].hex, raw, sizeof(raw));
		osk_result_t res;
		/* Die beiden PSBT ohne Eingaenge lehnt diese Fassung
		 * ausdruecklich ab: es gibt nichts zu unterschreiben, und
		 * "null Eingaenge" ist in den Bytes nicht von der
		 * Zeugen-Markierung zu unterscheiden. Steht in docs/wallet.md. */
		bool expect_reject = (i == 8u || i == 9u);

		T_TRUE("Vektor gelesen", len > 0);
		res = osk_psbt_parse(raw, len, &psbt);
		if (expect_reject) {
			T_EQ("PSBT ohne Eingaenge abgelehnt", res, OSK_ERR_FORMAT);
			continue;
		}
		if (res != OSK_OK)
			t_fail("gueltiges PSBT gelesen", __FILE__, __LINE__, PSBT_VALID[i].name);
		T_EQ("gelesen", res, OSK_OK);
		if (res != OSK_OK)
			continue;
		T_TRUE("Eingaenge gezaehlt", psbt.in_count >= 1u);
		T_TRUE("Ausgaenge gezaehlt", psbt.out_count >= 1u);
		/* Jeder Eingang, fuer den ein Betrag bekannt ist, muss unter
		 * der Obergrenze liegen - sonst waere die Gebuehr Unsinn. */
		for (size_t k = 0; k < psbt.in_count; k++)
			T_TRUE("Betrag im Rahmen", psbt.in[k].value <= 2100000000000000ULL);
	}
}

/* Das PSBT aus der Vorbemerkung. */
static const char *const OWN_PSBT =
    "70736274ff0100710200000001aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    "aa0000000000feffffff0260ea000000000000160014751e76e8199196d454941c45d1b3a323f1433bd63075"
    "0000000000001600143e34985dca6fddc9fb369940e4c7d8e2873f529c000000000001011fa0860100000000"
    "00160014c0cebcd6c3d3ca8c75dc5ec62ebe55330ef910e222060330d54fd0dd420a6e5f8d3624f5f3482cae"
    "350f79d5f0753bf5beef9c2d91af3c1873c5da0a540000800000008000000080000000000000000000002202"
    "03025324888e429ab8e3dbaf1f7802648b9cd01e9b418485c5fa4c1b9b5700e1a61873c5da0a540000800000"
    "008000000080010000000000000000";
static const char *const OWN_MNEMONIC =
    "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon"
    " about";
static const char *const OWN_SIGHASH =
    "de1130606e7551f3c3c7e0573fe547ba0b13c2fb3d900fb5fd80a82415d30f55";
static const char *const OWN_PUB =
    "0330d54fd0dd420a6e5f8d3624f5f3482cae350f79d5f0753bf5beef9c2d91af3c";

static osk_item_id_t prepare_seed(void)
{
	osk_item_id_t id = 0;

	mock_store_reset();
	mock_se050_reset();
	mock_ui_reset();
	T_EQ("Seed in den Tresor", osk_wallet_seed_store(0, "Geldboerse", OWN_MNEMONIC, NULL, &id),
	     OSK_OK);
	return id;
}

static void test_view(void)
{
	osk_psbt_t psbt;
	osk_wallet_view_t view;
	size_t len = big_unhex(OWN_PSBT, raw, sizeof(raw));

	T_TRUE("eigenes PSBT gelesen", len > 0);
	T_EQ("gelesen", osk_psbt_parse(raw, len, &psbt), OSK_OK);
	T_EQ("ein Eingang", psbt.in_count, 1);
	T_EQ("zwei Ausgaenge", psbt.out_count, 2);
	T_TRUE("Zeugen-UTXO", psbt.in[0].has_witness_utxo);
	T_EQ("Betrag des Eingangs", (long long)psbt.in[0].value, 100000);
	T_EQ("eine Ableitungsangabe", psbt.in[0].deriv_count, 1);
	T_EQ("Fingerabdruck", (long long)psbt.in[0].deriv[0].fingerprint, 0x73c5da0aLL);
	T_EQ("Pfadlaenge", psbt.in[0].deriv[0].path_len, 5);

	T_EQ("Ansicht", osk_psbt_view(&psbt, 0x73c5da0au, "bc", 0x00u, 0x05u, &view), OSK_OK);
	T_TRUE("lesbar", view.readable);
	/* Die fremde Adresse, nicht das Wechselgeld. */
	T_TRUE("Empfaenger stimmt",
	       strcmp(view.recipient, "bc1qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t4") == 0);
	if (strcmp(view.recipient, "bc1qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t4") != 0)
		printf("  bekam %s\n", view.recipient);
	T_TRUE("Betrag stimmt", strcmp(view.amount, "0.0006 BTC") == 0);
	if (strcmp(view.amount, "0.0006 BTC") != 0)
		printf("  Betrag %s\n", view.amount);
	T_TRUE("Gebuehr stimmt", strcmp(view.fee, "0.0001 BTC") == 0);
	if (strcmp(view.fee, "0.0001 BTC") != 0)
		printf("  Gebuehr %s\n", view.fee);
	T_EQ("nur ein Empfaenger", (long long)view.more_recipients, 0);

	/* Mit einem fremden Fingerabdruck ist das Wechselgeld kein
	 * Wechselgeld mehr: dann sind es zwei Empfaenger und 90 000 Satoshi. */
	T_EQ("Ansicht mit fremdem Abdruck",
	     osk_psbt_view(&psbt, 0x11223344u, "bc", 0x00u, 0x05u, &view), OSK_OK);
	T_TRUE("Betrag jetzt hoeher", strcmp(view.amount, "0.0009 BTC") == 0);
	T_EQ("ein weiterer Empfaenger", (long long)view.more_recipients, 1);
}

static void test_sign(void)
{
	osk_psbt_t psbt, again;
	osk_item_id_t id;
	uint8_t out[4096];
	uint8_t want_hash[32], pub_raw[33], r[32], s[32];
	size_t len, out_len = 0, signed_count = 0;
	osk_ec_point_t point;
	bool found = false;

	id = prepare_seed();
	len = big_unhex(OWN_PSBT, raw, sizeof(raw));
	T_EQ("gelesen", osk_psbt_parse(raw, len, &psbt), OSK_OK);

	mock_ui_answer(OSK_OK);
	T_EQ("unterschrieben",
	     osk_psbt_sign(&psbt, id, "test", "bc", 0x00u, 0x05u, false, out, sizeof(out), &out_len,
			   &signed_count),
	     OSK_OK);
	T_EQ("eine Eingabe unterschrieben", signed_count, 1);
	T_EQ("Freigabe wurde gefragt", mock_ui_calls(), 1);
	T_EQ("es war eine Unterschrift", mock_ui_kind(), OSK_ASK_SIGN);
	T_TRUE("Empfaenger stand auf dem Schirm",
	       strcmp(mock_ui_subject(), "bc1qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t4") == 0);
	T_TRUE("Betrag und Gebuehr standen auf dem Schirm",
	       strcmp(mock_ui_detail(), "0.0006 BTC + 0.0001 BTC") == 0);
	if (strcmp(mock_ui_detail(), "0.0006 BTC + 0.0001 BTC") != 0)
		printf("  Zeile: %s\n", mock_ui_detail());
	T_EQ("der Schluessel ging in den Chip und wieder heraus", mock_se050_imports(), 1);
	T_TRUE("der fluechtige Platz wurde geleert", mock_se050_deletes() >= 1u);

	/* Das Ergebnis muss wieder ein lesbares PSBT sein ... */
	T_EQ("Ergebnis lesbar", osk_psbt_parse(out, out_len, &again), OSK_OK);
	T_TRUE("jetzt mit Unterschrift", again.in[0].already_signed);
	T_EQ("gleiche Transaktion", memcmp(&again.tx.in[0].txid, &psbt.tx.in[0].txid, 32), 0);

	/* ... und die Unterschrift muss zu unserem Schluessel und zu dem
	 * Hash passen, den eine unabhaengige Rechnung ergibt. */
	T_EQ("Hash gelesen", t_unhex(OWN_SIGHASH, want_hash, sizeof(want_hash)), 32);
	T_EQ("Punkt gelesen", t_unhex(OWN_PUB, pub_raw, sizeof(pub_raw)), 33);
	T_EQ("Punkt gueltig", osk_ec_point_decode(pub_raw, 33, &point), OSK_OK);

	/* Die Unterschrift im Ergebnis finden: Schluessel 0x02 || pub. */
	for (size_t i = 0; i + 1u < out_len; i++) {
		if (out[i] != 34u || out[i + 1u] != 0x02u)
			continue;
		if (memcmp(out + i + 2u, pub_raw, 33) != 0)
			continue;
		{
			size_t pos = i + 35u;
			size_t sig_len = out[pos];

			T_TRUE("Laenge plausibel", sig_len >= 9u && sig_len <= 73u);
			T_EQ("Hashtyp SIGHASH_ALL", out[pos + sig_len], 0x01u);
			T_EQ("DER lesbar",
			     osk_signer_der_to_raw(out + pos + 1u, sig_len - 1u, r, s), OSK_OK);
			T_TRUE("Unterschrift passt", osk_ec_verify(want_hash, r, s, &point));
			T_TRUE("s ist klein", !osk_ec_s_is_high(s));
			found = true;
		}
		break;
	}
	T_TRUE("Unterschrift im Ergebnis gefunden", found);
}

static void test_refusals(void)
{
	osk_psbt_t psbt;
	osk_item_id_t id;
	uint8_t out[4096];
	size_t len, out_len = 0, signed_count = 0;

	len = big_unhex(OWN_PSBT, raw, sizeof(raw));

	/* Sagt der Nutzer nein, wird nichts unterschrieben und der Chip nie
	 * gefragt. */
	id = prepare_seed();
	T_EQ("gelesen", osk_psbt_parse(raw, len, &psbt), OSK_OK);
	mock_ui_answer(OSK_ERR_DENIED);
	T_EQ("Nutzer sagt nein",
	     osk_psbt_sign(&psbt, id, "test", "bc", 0x00u, 0x05u, false, out, sizeof(out), &out_len,
			   &signed_count),
	     OSK_ERR_DENIED);
	T_EQ("nichts unterschrieben", signed_count, 0);
	T_EQ("Chip nicht gefragt", mock_se050_imports(), 0);

	/* Gibt der Tresor den Seed nicht heraus, endet es dort. */
	id = prepare_seed();
	mock_store_deny(true);
	T_EQ("Tresor verweigert",
	     osk_psbt_sign(&psbt, id, "test", "bc", 0x00u, 0x05u, false, out, sizeof(out), &out_len,
			   &signed_count),
	     OSK_ERR_DENIED);
	T_EQ("Freigabe nicht gefragt", mock_ui_calls(), 0);

	/* Kann der Chip secp256k1 nicht, wird nicht unterschrieben - und
	 * schon gar nicht in der Firmware gerechnet. */
	id = prepare_seed();
	mock_se050_set_curves(1u << OSK_CURVE_P256);
	mock_ui_answer(OSK_OK);
	T_EQ("Chip kann die Kurve nicht",
	     osk_psbt_sign(&psbt, id, "test", "bc", 0x00u, 0x05u, false, out, sizeof(out), &out_len,
			   &signed_count),
	     OSK_ERR_UNSUPPORTED);
	T_EQ("nichts in den Chip gegeben", mock_se050_imports(), 0);

	/* Liefert der Chip eine verfaelschte Unterschrift, faellt es auf. */
	id = prepare_seed();
	mock_se050_corrupt_sign(true);
	mock_ui_answer(OSK_OK);
	T_EQ("verfaelschte Unterschrift erkannt",
	     osk_psbt_sign(&psbt, id, "test", "bc", 0x00u, 0x05u, false, out, sizeof(out), &out_len,
			   &signed_count),
	     OSK_ERR_INTERNAL);

	/* Zu kleiner Ausgabepuffer: kein Teilergebnis. */
	id = prepare_seed();
	mock_ui_answer(OSK_OK);
	T_EQ("Puffer zu klein",
	     osk_psbt_sign(&psbt, id, "test", "bc", 0x00u, 0x05u, false, out, 32, &out_len,
			   &signed_count),
	     OSK_ERR_ARG);
}

/* Ein PSBT, dessen Ableitungsangabe auf ein anderes Geraet zeigt, wird
 * gelesen und angezeigt, aber nicht unterschrieben. */
static void test_foreign_key(void)
{
	osk_psbt_t psbt;
	osk_item_id_t id;
	uint8_t out[4096];
	size_t len, out_len = 0, signed_count = 0;

	id = prepare_seed();
	len = big_unhex(PSBT_VALID[5].hex, raw, sizeof(raw));
	T_TRUE("fremdes PSBT gelesen", len > 0);
	T_EQ("gelesen", osk_psbt_parse(raw, len, &psbt), OSK_OK);
	mock_ui_answer(OSK_OK);
	{
		osk_result_t res = osk_psbt_sign(&psbt, id, "test", "bc", 0x00u, 0x05u, true, out,
						 sizeof(out), &out_len, &signed_count);

		T_TRUE("kein Fehler oder klare Ablehnung", res == OSK_OK || res == OSK_ERR_DENIED);
		if (res == OSK_OK) {
			T_EQ("nichts unterschrieben", signed_count, 0);
			T_EQ("Chip nicht gefragt", mock_se050_imports(), 0);
		}
	}
}

int main(void)
{
	test_invalid();
	test_valid();
	test_view();
	test_sign();
	test_refusals();
	test_foreign_key();
	return t_report("psbt");
}
