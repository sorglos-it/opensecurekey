/* Rahmen, attest (14) und derive (15) gegen die Attrappen. */
#include "test_util.h"
#include "mocks.h"
#include "guard_helper.h"
#include "osk_guard_keys.h"

static void setup(void)
{
	mock_reset_all();
	mock_se050_setup_guard();
	osk_guard_grant_clear();
	osk_guard_keys_reset();
	osk_guard_set_attest_confirm(false);
}

/* --- Rahmen ------------------------------------------------------------- */

static void test_frame(void)
{
	gh_reply_t reply;
	gh_frame_t frame;

	setup();

	/* Kein CBOR. Der Leser prueft jede Laenge, bevor er liest - hier
	 * kommt eine Antwort mit Grund heraus, kein Absturz. */
	const uint8_t junk[] = {0xff, 0xff, 0xff};
	T_EQ("Muell gibt Antwort", gh_call(junk, sizeof(junk), OSK_LINK_USB_VENDOR, &reply),
	     OSK_OK);
	T_TRUE("Muell: Rahmen lesbar", reply.ok);
	T_EQ("Muell: Grund", reply.result, OSK_ERR_FORMAT);

	/* Leere Nachricht. */
	const uint8_t nothing[] = {0x00};
	T_EQ("ein Byte gibt Antwort", gh_call(nothing, 1u, OSK_LINK_USB_VENDOR, &reply), OSK_OK);
	T_EQ("ein Byte: Grund", reply.result, OSK_ERR_FORMAT);

	/* Falsche Protokollfassung. */
	osk_cbor_writer_init(&frame.w, frame.buf, sizeof(frame.buf));
	osk_cbor_put_map(&frame.w, 3u);
	osk_cbor_put_text(&frame.w, "v");
	osk_cbor_put_uint(&frame.w, 2u);
	osk_cbor_put_text(&frame.w, "c");
	osk_cbor_put_uint(&frame.w, OSK_GUARD_CMD_ATTEST);
	osk_cbor_put_text(&frame.w, "s");
	osk_cbor_put_uint(&frame.w, 3u);
	T_TRUE("Fassung 2 gebaut", osk_cbor_writer_ok(&frame.w));
	T_EQ("Fassung 2 antwortet", gh_call(frame.buf, frame.w.len, OSK_LINK_USB_VENDOR, &reply),
	     OSK_OK);
	T_EQ("Fassung 2: Grund", reply.result, OSK_ERR_UNSUPPORTED);
	T_EQ("Fassung 2: Folgenummer kommt zurueck", reply.seq, 3u);

	/* Ein Befehl, den dieses Applet nicht hat (1 = hello gehoert einem
	 * anderen Bearbeiter). */
	gh_begin(&frame, 1u, 4u, 0u);
	T_EQ("hello antwortet", gh_call(frame.buf, gh_end(&frame), OSK_LINK_USB_VENDOR, &reply),
	     OSK_OK);
	T_EQ("hello: Grund", reply.result, OSK_ERR_UNSUPPORTED);

	/* Antwort traegt Fassung und Folgenummer. */
	gh_begin(&frame, OSK_GUARD_CMD_DERIVE, 99u, 0u);
	T_EQ("derive ohne Daten", gh_call(frame.buf, gh_end(&frame), OSK_LINK_USB_VENDOR, &reply),
	     OSK_OK);
	T_EQ("Fassung in der Antwort", reply.version, OSK_GUARD_PROTO_VERSION);
	T_EQ("Folgenummer in der Antwort", reply.seq, 99u);
	T_EQ("derive ohne Daten: Grund", reply.result, OSK_ERR_ARG);
}

/* Der Tresor ist zu: attest antwortet trotzdem, alles andere nicht. */
static void test_locked(void)
{
	gh_frame_t frame;
	gh_reply_t reply;
	uint8_t nonce[OSK_GUARD_NONCE_LEN];

	setup();
	memset(nonce, 7, sizeof(nonce));
	mock_store_set_open(false);

	gh_begin(&frame, OSK_GUARD_CMD_ATTEST, 1u, 1u);
	osk_cbor_put_text(&frame.w, "ch");
	osk_cbor_put_bytes(&frame.w, nonce, sizeof(nonce));
	T_EQ("attest bei zu", gh_call(frame.buf, gh_end(&frame), OSK_LINK_USB_VENDOR, &reply),
	     OSK_OK);
	T_EQ("attest braucht keinen offenen Tresor", reply.result, OSK_OK);

	/* Ein unbekannter Befehl bekommt seinen eigenen Grund, auch bei
	 * geschlossenem Tresor - nicht "gesperrt" zu etwas, das es hier gar
	 * nicht gibt. */
	gh_begin(&frame, 5u, 3u, 0u);
	T_EQ("fremder Befehl bei zu",
	     gh_call(frame.buf, gh_end(&frame), OSK_LINK_USB_VENDOR, &reply), OSK_OK);
	T_EQ("fremder Befehl: nicht unterstuetzt", reply.result, OSK_ERR_UNSUPPORTED);

	gh_begin(&frame, OSK_GUARD_CMD_DERIVE, 2u, 2u);
	osk_cbor_put_text(&frame.w, "info");
	osk_cbor_put_bytes(&frame.w, (const uint8_t *)"x", 1u);
	osk_cbor_put_text(&frame.w, "n");
	osk_cbor_put_uint(&frame.w, 32u);
	T_EQ("derive bei zu", gh_call(frame.buf, gh_end(&frame), OSK_LINK_USB_VENDOR, &reply),
	     OSK_OK);
	T_EQ("derive verlangt offenen Tresor", reply.result, OSK_ERR_LOCKED);
}

/* --- 14 attest ---------------------------------------------------------- */

static void test_attest(void)
{
	gh_frame_t frame;
	gh_reply_t reply;
	uint8_t nonce[OSK_GUARD_NONCE_LEN];
	uint8_t hash[OSK_GUARD_HASH_LEN];

	setup();
	memset(nonce, 0x10, sizeof(nonce));
	memset(hash, 0x20, sizeof(hash));

	gh_begin(&frame, OSK_GUARD_CMD_ATTEST, 1u, 2u);
	osk_cbor_put_text(&frame.w, "ch");
	osk_cbor_put_bytes(&frame.w, nonce, sizeof(nonce));
	osk_cbor_put_text(&frame.w, "bh");
	osk_cbor_put_bytes(&frame.w, hash, sizeof(hash));
	T_EQ("attest laeuft", gh_call(frame.buf, gh_end(&frame), OSK_LINK_USB_VENDOR, &reply),
	     OSK_OK);
	T_EQ("attest: ok", reply.result, OSK_OK);

	const uint8_t *sig = NULL, *pub = NULL;
	size_t sig_len = 0, pub_len = 0;
	T_TRUE("attest: sig da", gh_bytes(&reply.data, "sig", &sig, &sig_len));
	T_TRUE("attest: pub da", gh_bytes(&reply.data, "pub", &pub, &pub_len));
	T_EQ("attest: sig 64 Byte", sig_len, OSK_GUARD_SIG_LEN);
	T_EQ("attest: pub 32 Byte", pub_len, OSK_GUARD_PUB_LEN);

	/* Ohne Freigabe-Vorgabe wird nicht gefragt - sonst lernt der Nutzer,
	 * Tastendruecke wegzudruecken (siehe osk_guard_fw.h). */
	T_EQ("attest fragt nicht", mock_ui_confirm_count(), 0u);
	/* Jede Benutzung steht im Protokoll. */
	T_EQ("attest protokolliert", mock_log_count(), 1u);
	T_EQ("attest: Anlass 14", mock_log_what(0u), OSK_GUARD_CMD_ATTEST);

	/* Eine andere Frage gibt eine andere Antwort - eine mitgeschnittene
	 * ist nichts wert. */
	uint8_t first[OSK_GUARD_SIG_LEN];
	memcpy(first, sig, sizeof(first));
	nonce[0] = 0x11;
	gh_begin(&frame, OSK_GUARD_CMD_ATTEST, 2u, 2u);
	osk_cbor_put_text(&frame.w, "ch");
	osk_cbor_put_bytes(&frame.w, nonce, sizeof(nonce));
	osk_cbor_put_text(&frame.w, "bh");
	osk_cbor_put_bytes(&frame.w, hash, sizeof(hash));
	T_EQ("zweite Frage", gh_call(frame.buf, gh_end(&frame), OSK_LINK_USB_VENDOR, &reply),
	     OSK_OK);
	T_TRUE("zweite Antwort da", gh_bytes(&reply.data, "sig", &sig, &sig_len));
	T_TRUE("neue Frage, neue Antwort", memcmp(first, sig, sizeof(first)) != 0);

	/* Krumme Laengen auf dem Dongle-Weg. */
	gh_begin(&frame, OSK_GUARD_CMD_ATTEST, 3u, 2u);
	osk_cbor_put_text(&frame.w, "ch");
	osk_cbor_put_bytes(&frame.w, nonce, 31u);
	osk_cbor_put_text(&frame.w, "bh");
	osk_cbor_put_bytes(&frame.w, hash, sizeof(hash));
	T_EQ("kurze Frage", gh_call(frame.buf, gh_end(&frame), OSK_LINK_USB_VENDOR, &reply),
	     OSK_OK);
	T_EQ("kurze Frage: Grund", reply.result, OSK_ERR_ARG);

	/* Aufgabe laenger als das Geraet unterschreiben kann. */
	uint8_t big[OSK_GUARD_CHALLENGE_MAX + 1u];
	memset(big, 3, sizeof(big));
	gh_begin(&frame, OSK_GUARD_CMD_ATTEST, 4u, 1u);
	osk_cbor_put_text(&frame.w, "ch");
	osk_cbor_put_bytes(&frame.w, big, sizeof(big));
	T_EQ("zu lange Aufgabe", gh_call(frame.buf, gh_end(&frame), OSK_LINK_USB_VENDOR, &reply),
	     OSK_OK);
	T_EQ("zu lange Aufgabe: Grund", reply.result, OSK_ERR_ARG);
}

/* Mit eingeschalteter Nachfrage: ein Nein ist ein Nein. */
static void test_attest_confirm(void)
{
	gh_frame_t frame;
	gh_reply_t reply;
	uint8_t nonce[OSK_GUARD_NONCE_LEN];

	setup();
	memset(nonce, 5, sizeof(nonce));
	osk_guard_set_attest_confirm(true);
	mock_ui_set_confirm(OSK_ERR_DENIED);

	gh_begin(&frame, OSK_GUARD_CMD_ATTEST, 1u, 1u);
	osk_cbor_put_text(&frame.w, "ch");
	osk_cbor_put_bytes(&frame.w, nonce, sizeof(nonce));
	T_EQ("attest mit Nachfrage",
	     gh_call(frame.buf, gh_end(&frame), OSK_LINK_USB_VENDOR, &reply), OSK_OK);
	T_EQ("attest: abgelehnt", reply.result, OSK_ERR_DENIED);
	T_EQ("es wurde gefragt", mock_ui_confirm_count(), 1u);
	T_EQ("abgelehnt kommt nicht ins Protokoll", mock_log_count(), 0u);
	osk_guard_set_attest_confirm(false);
}

/* --- 15 derive ---------------------------------------------------------- */

static void test_derive(void)
{
	gh_frame_t frame;
	gh_reply_t reply;
	const uint8_t info[] = "osk/node/1";

	setup();

	gh_begin(&frame, OSK_GUARD_CMD_DERIVE, 1u, 2u);
	osk_cbor_put_text(&frame.w, "info");
	osk_cbor_put_bytes(&frame.w, info, sizeof(info) - 1u);
	osk_cbor_put_text(&frame.w, "n");
	osk_cbor_put_uint(&frame.w, 32u);
	T_EQ("derive laeuft", gh_call(frame.buf, gh_end(&frame), OSK_LINK_USB_VENDOR, &reply),
	     OSK_OK);
	T_EQ("derive: ok", reply.result, OSK_OK);

	const uint8_t *key = NULL;
	size_t key_len = 0;
	T_TRUE("derive: key da", gh_bytes(&reply.data, "key", &key, &key_len));
	T_EQ("derive: 32 Byte", key_len, 32u);

	uint8_t first[32];
	memcpy(first, key, sizeof(first));

	/* Gleiches info gibt denselben Schluessel - sonst waere nichts
	 * ableitbar. */
	gh_begin(&frame, OSK_GUARD_CMD_DERIVE, 2u, 2u);
	osk_cbor_put_text(&frame.w, "info");
	osk_cbor_put_bytes(&frame.w, info, sizeof(info) - 1u);
	osk_cbor_put_text(&frame.w, "n");
	osk_cbor_put_uint(&frame.w, 32u);
	T_EQ("derive zweimal", gh_call(frame.buf, gh_end(&frame), OSK_LINK_USB_VENDOR, &reply),
	     OSK_OK);
	T_TRUE("derive: key da", gh_bytes(&reply.data, "key", &key, &key_len));
	T_TRUE("gleiches info, gleicher Schluessel", memcmp(first, key, sizeof(first)) == 0);

	/* Anderes info gibt einen anderen Schluessel. */
	const uint8_t other[] = "osk/node/2";
	gh_begin(&frame, OSK_GUARD_CMD_DERIVE, 3u, 2u);
	osk_cbor_put_text(&frame.w, "info");
	osk_cbor_put_bytes(&frame.w, other, sizeof(other) - 1u);
	osk_cbor_put_text(&frame.w, "n");
	osk_cbor_put_uint(&frame.w, 32u);
	T_EQ("derive anders", gh_call(frame.buf, gh_end(&frame), OSK_LINK_USB_VENDOR, &reply),
	     OSK_OK);
	T_TRUE("derive: key da", gh_bytes(&reply.data, "key", &key, &key_len));
	T_TRUE("anderes info, anderer Schluessel", memcmp(first, key, sizeof(first)) != 0);

	/* Und: das Grundgeheimnis kommt nicht heraus. Es ist das Geheimnis
	 * des Schluessels OSK_GUARD_KEY_ROOT in der Attrappe; was derive
	 * liefert, darf damit nicht uebereinstimmen - fuer keine der
	 * zulaessigen Laengen. */
	uint8_t root_secret[32];
	mock_se050_root_secret(root_secret);
	for (uint64_t n = 1u; n <= OSK_GUARD_DERIVE_MAX; n++) {
		gh_begin(&frame, OSK_GUARD_CMD_DERIVE, 4u, 2u);
		osk_cbor_put_text(&frame.w, "info");
		osk_cbor_put_bytes(&frame.w, info, sizeof(info) - 1u);
		osk_cbor_put_text(&frame.w, "n");
		osk_cbor_put_uint(&frame.w, n);
		T_EQ("derive laeuft",
		     gh_call(frame.buf, gh_end(&frame), OSK_LINK_USB_VENDOR, &reply), OSK_OK);
		T_EQ("derive: ok", reply.result, OSK_OK);
		T_TRUE("derive: key da", gh_bytes(&reply.data, "key", &key, &key_len));
		T_EQ("derive: gewuenschte Laenge", key_len, n);
		if (key_len <= sizeof(root_secret)) {
			T_TRUE("derive gibt nie das Grundgeheimnis",
			       memcmp(key, root_secret, key_len) != 0);
		}
	}

	/* Grenzen. */
	gh_begin(&frame, OSK_GUARD_CMD_DERIVE, 5u, 2u);
	osk_cbor_put_text(&frame.w, "info");
	osk_cbor_put_bytes(&frame.w, info, sizeof(info) - 1u);
	osk_cbor_put_text(&frame.w, "n");
	osk_cbor_put_uint(&frame.w, OSK_GUARD_DERIVE_MAX + 1u);
	T_EQ("derive zu viel", gh_call(frame.buf, gh_end(&frame), OSK_LINK_USB_VENDOR, &reply),
	     OSK_OK);
	T_EQ("derive zu viel: Grund", reply.result, OSK_ERR_ARG);

	gh_begin(&frame, OSK_GUARD_CMD_DERIVE, 6u, 2u);
	osk_cbor_put_text(&frame.w, "info");
	osk_cbor_put_bytes(&frame.w, info, 0u);
	osk_cbor_put_text(&frame.w, "n");
	osk_cbor_put_uint(&frame.w, 32u);
	T_EQ("derive ohne info", gh_call(frame.buf, gh_end(&frame), OSK_LINK_USB_VENDOR, &reply),
	     OSK_OK);
	T_EQ("derive ohne info: Grund", reply.result, OSK_ERR_ARG);

	/* derive fragt nicht nach - es gibt kein Geheimnis heraus, das nicht
	 * schon aus dem offenen Tresor folgt. */
	T_EQ("derive fragt nicht", mock_ui_confirm_count(), 0u);
}

/* Der Chip antwortet nicht: der Grund muss durchkommen, nicht OSK_OK mit
 * leerer Unterschrift. */
static void test_chip_offline(void)
{
	gh_frame_t frame;
	gh_reply_t reply;
	uint8_t nonce[OSK_GUARD_NONCE_LEN];

	setup();
	memset(nonce, 9, sizeof(nonce));
	mock_se050_set_offline(true);

	gh_begin(&frame, OSK_GUARD_CMD_ATTEST, 1u, 1u);
	osk_cbor_put_text(&frame.w, "ch");
	osk_cbor_put_bytes(&frame.w, nonce, sizeof(nonce));
	T_EQ("attest ohne Chip", gh_call(frame.buf, gh_end(&frame), OSK_LINK_USB_VENDOR, &reply),
	     OSK_OK);
	T_EQ("attest ohne Chip: Grund", reply.result, OSK_ERR_IO);
	T_EQ("nichts im Protokoll", mock_log_count(), 0u);
}

int main(void)
{
	test_frame();
	test_locked();
	test_attest();
	test_attest_confirm();
	test_derive();
	test_chip_offline();
	return t_report("guard-rahmen");
}
