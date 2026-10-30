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
	     osk_psbt_sign(&psbt, id, "test", "bc", 0x00u, 0x05u, out, sizeof(out), &out_len,
			   &signed_count),
	     OSK_OK);
	T_EQ("eine Eingabe unterschrieben", signed_count, 1);
	T_EQ("Freigabe wurde gefragt", mock_ui_calls(), 1);
	T_EQ("es war eine Unterschrift", mock_ui_kind(), OSK_ASK_SIGN);
	/* Der Empfaenger stand GANZ auf dem Schirm - gegliedert, mit der
	 * Pruefzahl dahinter, und ohne Punkte in der Mitte. */
	T_TRUE("keine Punkte in der Mitte", strstr(mock_ui_subject(), "...") == NULL);
	T_TRUE("Pruefzahl dabei", strchr(mock_ui_subject(), '#') != NULL);
	{
		/* Jedes Zeichen der Adresse stand da, in der Reihenfolge. */
		const char *want = "bc1qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t4";
		const char *got = mock_ui_subject();
		size_t j = 0;

		for (size_t i = 0; got[i] != 0 && got[i] != '#'; i++) {
			if (got[i] != ' ')
				j += (want[j] != 0 && got[i] == want[j]) ? 1u : 0u;
		}
		T_EQ("ganze Adresse stand da", (long long)j, (long long)strlen(want));
		if (j != strlen(want))
			printf("  Betreff: %s\n", got);
	}
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
	     osk_psbt_sign(&psbt, id, "test", "bc", 0x00u, 0x05u, out, sizeof(out), &out_len,
			   &signed_count),
	     OSK_ERR_DENIED);
	T_EQ("nichts unterschrieben", signed_count, 0);
	T_EQ("Chip nicht gefragt", mock_se050_imports(), 0);

	/* Gibt der Tresor den Seed nicht heraus, endet es dort. */
	id = prepare_seed();
	mock_store_deny(true);
	T_EQ("Tresor verweigert",
	     osk_psbt_sign(&psbt, id, "test", "bc", 0x00u, 0x05u, out, sizeof(out), &out_len,
			   &signed_count),
	     OSK_ERR_DENIED);
	T_EQ("Freigabe nicht gefragt", mock_ui_calls(), 0);

	/* Kann der Chip secp256k1 nicht, wird nicht unterschrieben - und
	 * schon gar nicht in der Firmware gerechnet. */
	id = prepare_seed();
	mock_se050_set_curves(1u << OSK_CURVE_P256);
	mock_ui_answer(OSK_OK);
	T_EQ("Chip kann die Kurve nicht",
	     osk_psbt_sign(&psbt, id, "test", "bc", 0x00u, 0x05u, out, sizeof(out), &out_len,
			   &signed_count),
	     OSK_ERR_UNSUPPORTED);
	T_EQ("nichts in den Chip gegeben", mock_se050_imports(), 0);

	/* Liefert der Chip eine verfaelschte Unterschrift, faellt es auf. */
	id = prepare_seed();
	mock_se050_corrupt_sign(true);
	mock_ui_answer(OSK_OK);
	T_EQ("verfaelschte Unterschrift erkannt",
	     osk_psbt_sign(&psbt, id, "test", "bc", 0x00u, 0x05u, out, sizeof(out), &out_len,
			   &signed_count),
	     OSK_ERR_INTERNAL);

	/* Zu kleiner Ausgabepuffer: kein Teilergebnis. */
	id = prepare_seed();
	mock_ui_answer(OSK_OK);
	T_EQ("Puffer zu klein",
	     osk_psbt_sign(&psbt, id, "test", "bc", 0x00u, 0x05u, out, 32, &out_len,
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
		osk_result_t res = osk_psbt_sign(&psbt, id, "test", "bc", 0x00u, 0x05u, out,
						 sizeof(out), &out_len, &signed_count);

		T_TRUE("kein Fehler oder klare Ablehnung", res == OSK_OK || res == OSK_ERR_DENIED);
		if (res == OSK_OK) {
			T_EQ("nichts unterschrieben", signed_count, 0);
			T_EQ("Chip nicht gefragt", mock_se050_imports(), 0);
		}
	}
}

/* --- Was NICHT unterschrieben wird ------------------------------------- */

/* Taproot wird erkannt und abgelehnt - nicht halb unterschrieben.
 *
 * Gebaut aus dem eigenen PSBT, bei dem nur das Skript des Eingangs auf
 * ein Taproot-Skript getauscht wird (OP_1 und 32 Byte Schluessel). Die
 * Wertlaenge geht dabei von 0x1f auf 0x2b: acht Byte Betrag, ein Byte
 * Skriptlaenge, 34 Byte Skript. Alles andere bleibt, wie es war. */
static void test_taproot_abgelehnt(void)
{
	static const char *const TAPROOT_PSBT =
	    "70736274ff0100710200000001aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
	    "aa0000000000feffffff0260ea000000000000160014751e76e8199196d454941c45d1b3a323f1433bd63075"
	    "0000000000001600143e34985dca6fddc9fb369940e4c7d8e2873f529c000000000001012ba0860100000000"
	    "0022512079be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f8179822060330d54fd0dd"
	    "420a6e5f8d3624f5f3482cae350f79d5f0753bf5beef9c2d91af3c1873c5da0a540000800000008000000080"
	    "000000000000000000002202" "03025324888e429ab8e3dbaf1f7802648b9cd01e9b418485c5fa4c1b9b57"
	    "00e1a61873c5da0a540000800000008000000080010000000000000000";
	uint8_t psbt_raw[1024];
	uint8_t out[2048];
	size_t len, out_len = 0, signed_count = 0;
	osk_psbt_t psbt;
	osk_item_id_t id;

	/* Erst die Gegenprobe: dasselbe PSBT OHNE den Tausch geht durch.
	 * Ohne sie wuesste niemand, ob die Ablehnung am Taproot haengt
	 * oder an einem Tippfehler in der Zeichenkette. */
	len = big_unhex(OWN_PSBT, psbt_raw, sizeof(psbt_raw));
	T_TRUE("Gegenprobe gelesen", len > 0);
	T_EQ("Gegenprobe zerlegt", osk_psbt_parse(psbt_raw, len, &psbt), OSK_OK);
	id = prepare_seed();
	mock_ui_reset();
	mock_ui_answer(OSK_OK);
	T_EQ("Gegenprobe unterschrieben",
	     osk_psbt_sign(&psbt, id, "test", "bc", 0x00u, 0x05u, out, sizeof(out), &out_len,
			   &signed_count),
	     OSK_OK);
	T_EQ("Gegenprobe: eine Unterschrift", (long long)signed_count, 1);

	/* Und nun mit Taproot. */
	len = big_unhex(TAPROOT_PSBT, psbt_raw, sizeof(psbt_raw));
	T_TRUE("Taproot-PSBT gelesen", len > 0);
	T_EQ("Taproot-PSBT zerlegt", osk_psbt_parse(psbt_raw, len, &psbt), OSK_OK);
	T_EQ("es ist wirklich Taproot",
	     osk_btc_script_kind(psbt.in[0].utxo_script, psbt.in[0].utxo_script_len),
	     OSK_BTC_P2TR);

	id = prepare_seed();
	mock_ui_reset();
	mock_ui_answer(OSK_OK); /* der Nutzer WUERDE ja sagen */
	signed_count = 99;
	T_EQ("Taproot abgelehnt",
	     osk_psbt_sign(&psbt, id, "test", "bc", 0x00u, 0x05u, out, sizeof(out), &out_len,
			   &signed_count),
	     OSK_ERR_UNSUPPORTED);
	T_EQ("nichts unterschrieben", (long long)signed_count, 0);
	/* Es wurde nicht gefragt: eine Freigabe, die anschliessend
	 * scheitert, ist eine Freigabe fuer nichts. */
	T_EQ("nicht gefragt", (long long)mock_ui_calls(), 0);
	T_EQ("Chip unangetastet", mock_se050_imports(), 0);
	/* Und der Grund steht auf dem Schirm - eine Ablehnung ohne
	 * Erklaerung ist am Geraet nicht zu verstehen. */
	T_TRUE("Grund genannt",
	       strcmp(mock_ui_status_key(), "wallet.taproot-nicht-unterstuetzt") == 0);
	if (strcmp(mock_ui_status_key(), "wallet.taproot-nicht-unterstuetzt") != 0)
		printf("  Meldung: %s\n", mock_ui_status_key());
}

/* Mehr Ausgaenge, als das Geraet anzeigen kann: ABGELEHNT, nicht zum
 * Teil angezeigt.
 *
 * Neun Ausgaenge teilweise zu zeigen waere die gefaehrlichste Anzeige von
 * allen - der Nutzer saehe acht richtige Zeilen und wuesste nicht, dass
 * eine fehlt. Die Transaktion wird von Hand zusammengesetzt, damit die
 * Zahl der Ausgaenge die einzige Groesse ist, die sich aendert. */
static void test_zu_viele_ausgaenge(void)
{
	for (unsigned n = 1; n <= OSK_PSBT_MAX_OUT + 2u; n++) {
		uint8_t psbt_raw[4096];
		uint8_t tx[1024];
		osk_psbt_t psbt;
		size_t k = 0, t = 0;
		osk_result_t res;

		/* Die unsignierte Transaktion. */
		tx[t++] = 0x02; tx[t++] = 0x00; tx[t++] = 0x00; tx[t++] = 0x00;
		tx[t++] = 0x01; /* ein Eingang */
		for (unsigned i = 0; i < 32u; i++)
			tx[t++] = 0xaa;
		tx[t++] = 0x00; tx[t++] = 0x00; tx[t++] = 0x00; tx[t++] = 0x00;
		tx[t++] = 0x00; /* leeres Skript */
		tx[t++] = 0xfe; tx[t++] = 0xff; tx[t++] = 0xff; tx[t++] = 0xff;

		tx[t++] = (uint8_t)n; /* so viele Ausgaenge */
		for (unsigned i = 0; i < n; i++) {
			tx[t++] = 0x60; tx[t++] = 0xea;
			for (unsigned b = 0; b < 6u; b++)
				tx[t++] = 0x00;
			tx[t++] = 0x16; tx[t++] = 0x00; tx[t++] = 0x14;
			for (unsigned b = 0; b < 20u; b++)
				tx[t++] = (uint8_t)(0x10u + i);
		}
		tx[t++] = 0x00; tx[t++] = 0x00; tx[t++] = 0x00; tx[t++] = 0x00;

		/* Und das PSBT darum herum. */
		psbt_raw[k++] = 0x70; psbt_raw[k++] = 0x73; psbt_raw[k++] = 0x62; psbt_raw[k++] = 0x74;
		psbt_raw[k++] = 0xff;
		psbt_raw[k++] = 0x01; psbt_raw[k++] = 0x00; /* unsignierte Transaktion */
		/* Die Laenge als compact size, richtig und nicht geraten: ab
		 * 253 Byte steht 0xfd davor und dann zwei Byte niederwertiges
		 * zuerst. Mit acht Ausgaengen ist die Transaktion laenger als
		 * 253 Byte - ein einzelnes Laengenbyte waere hier still
		 * falsch geworden. */
		if (t < 253u) {
			psbt_raw[k++] = (uint8_t)t;
		} else {
			psbt_raw[k++] = 0xfd;
			psbt_raw[k++] = (uint8_t)(t & 0xffu);
			psbt_raw[k++] = (uint8_t)((t >> 8) & 0xffu);
		}
		memcpy(psbt_raw + k, tx, t);
		k += t;
		psbt_raw[k++] = 0x00;	       /* Ende des Kopfteils */
		psbt_raw[k++] = 0x00;	       /* ein leerer Eingangsteil */
		for (unsigned i = 0; i < n; i++)
			psbt_raw[k++] = 0x00;       /* je Ausgang ein leerer Teil */

		res = osk_psbt_parse(psbt_raw, k, &psbt);
		if (n <= OSK_PSBT_MAX_OUT) {
			/* Bis zur Grenze MUSS es gehen, sonst prueft der Fall
			 * darueber nichts. */
			T_EQ("bis zur Grenze gelesen", res, OSK_OK);
			if (res == OSK_OK)
				T_EQ("Zahl der Ausgaenge", (long long)psbt.out_count,
				     (long long)n);
			else
				printf("  %u Ausgaenge gingen nicht\n", n);
		} else {
			T_TRUE("darueber abgelehnt", res != OSK_OK);
			if (res == OSK_OK)
				printf("  %u Ausgaenge wurden angenommen - zu viele\n", n);
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
	test_taproot_abgelehnt();
	test_zu_viele_ausgaenge();
	return t_report("psbt");
}
