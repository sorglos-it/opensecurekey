/* OpenPGP-Karte gegen die Spezifikation 3.4: echte APDUs hinein, Antwort
 * und Statuswort gegen die Abschnitte 4.2 bis 4.4 und 7.2.
 *
 * Alles am PC gegen Attrappen. Keine echte Kurve, kein GnuPG, kein
 * Kartenleser. */
#include "test_util.h"
#include "osk_openpgp.h"
#include "osk_card.h"
#include "osk_ui.h"
#include "mocks.h"
#include "card_helper.h"

static const uint8_t g_pw1[6] = {'1', '2', '3', '4', '5', '6'};
static const uint8_t g_pw3[8] = {'1', '2', '3', '4', '5', '6', '7', '8'};

static void select_pgp(void)
{
	T_EQ("SELECT laeuft", call(0x00u, 0xa4u, 0x04u, 0x00u, osk_openpgp_aid(), 6u, -1), OSK_OK);
	T_EQ("SELECT ok", g_sw, OSK_SW_OK);
}

/* Die Admin-PIN liegt im Tresor: Zaehler, Laenge, Wert. */
static void put_pw3(void)
{
	uint8_t body[16];

	body[0] = 3u;
	body[1] = (uint8_t)sizeof(g_pw3);
	memcpy(body + 2, g_pw3, sizeof(g_pw3));
	T_EQ("Admin-PIN gelegt",
	     mock_store_put(OSK_ITEM_RECOVERY, "PGP-PW3", body, 2u + sizeof(g_pw3)), OSK_OK);
}

static void verify_pw(uint8_t ref, const uint8_t *pw, size_t len)
{
	(void)call(0x00u, 0x20u, 0x00u, ref, pw, len, -1);
}

static void test_select_and_data(void)
{
	const uint8_t *val = NULL;
	const uint8_t *inner = NULL;
	size_t len = 0;
	size_t inner_len = 0;

	mock_reset_all();
	osk_applets_reset();

	/* Die abgekuerzte Kennung aus sechs Bytes reicht zur Auswahl
	 * (Abschnitt 4.2.1) - so macht es GnuPG. */
	select_pgp();
	T_EQ("SELECT liefert keine Daten", g_resp_len, 0);

	/* Die volle Kennung geht auch. */
	T_EQ("volle Kennung",
	     call(0x00u, 0xa4u, 0x04u, 0x00u, osk_openpgp_aid(), OSK_PGP_AID_LEN, -1), OSK_OK);
	T_EQ("volle Kennung ok", g_sw, OSK_SW_OK);

	/* DO 4F: die Kennung selbst. */
	(void)call(0x00u, 0xcau, 0x00u, 0x4fu, NULL, 0u, 256);
	T_EQ("4F gelesen", g_sw, OSK_SW_OK);
	T_EQ("Laenge der Kennung", g_resp_len, OSK_PGP_AID_LEN);
	T_EQ("RID beginnt mit D2", g_resp[0], 0xd2);
	T_EQ("Fassung 3.4", g_resp[6], 0x03);
	T_EQ("Fassung 3.4 tief", g_resp[7], 0x04);

	/* DO 6E: die Anwendungsdaten mit allem, was GnuPG daraus liest. */
	(void)call(0x00u, 0xcau, 0x00u, 0x6eu, NULL, 0u, 1024);
	T_EQ("6E gelesen", g_sw, OSK_SW_OK);
	T_TRUE("Vorlage 6E", osk_tlv_find(g_resp, g_resp_len, 0x6eu, &val, &len));
	T_TRUE("Kennung drin", osk_tlv_find(val, len, 0x4fu, NULL, NULL));
	T_TRUE("historische Bytes", osk_tlv_find(val, len, 0x5f52u, NULL, NULL));
	T_TRUE("Vorlage 73", osk_tlv_find(val, len, 0x73u, &inner, &inner_len));
	T_TRUE("erweiterte Faehigkeiten", osk_tlv_find(inner, inner_len, 0xc0u, NULL, NULL));
	T_TRUE("PW-Status", osk_tlv_find(inner, inner_len, 0xc4u, NULL, NULL));
	T_TRUE("Fingerabdruecke", osk_tlv_find(inner, inner_len, 0xc5u, &val, &len));
	T_EQ("drei mal zwanzig Bytes", len, 60);

	/* Algorithmus-Eigenschaften: ECDSA mit der Kennung der Kurve. */
	(void)call(0x00u, 0xcau, 0x00u, 0x6eu, NULL, 0u, 1024);
	T_TRUE("Vorlage 6E erneut", osk_tlv_find(g_resp, g_resp_len, 0x6eu, &val, &len));
	T_TRUE("Vorlage 73 erneut", osk_tlv_find(val, len, 0x73u, &inner, &inner_len));
	T_TRUE("C1 Signatur", osk_tlv_find(inner, inner_len, 0xc1u, &val, &len));
	T_EQ("ECDSA", val[0], OSK_PGP_ALG_ECDSA);
	T_EQ("Laenge mit Kurvenkennung", len, 9);
	T_TRUE("Vorlage 6E noch einmal", osk_tlv_find(g_resp, g_resp_len, 0x6eu, &val, &len));
	T_TRUE("Vorlage 73 noch einmal", osk_tlv_find(val, len, 0x73u, &inner, &inner_len));
	T_TRUE("C2 Verschluesselung", osk_tlv_find(inner, inner_len, 0xc2u, &val, &len));
	T_EQ("ECDH", val[0], OSK_PGP_ALG_ECDH);

	/* DO 65 und 7A. */
	(void)call(0x00u, 0xcau, 0x00u, 0x65u, NULL, 0u, 256);
	T_EQ("65 gelesen", g_sw, OSK_SW_OK);
	T_EQ("Vorlage 65", g_resp[0], 0x65);
	(void)call(0x00u, 0xcau, 0x00u, 0x7au, NULL, 0u, 256);
	T_EQ("7A gelesen", g_sw, OSK_SW_OK);
	T_TRUE("Zaehler drin", osk_tlv_find(g_resp, g_resp_len, 0x7au, &val, &len));
	T_TRUE("DO 93", osk_tlv_find(val, len, 0x93u, &inner, &inner_len));
	T_EQ("drei Bytes", inner_len, 3);

	/* Ein Datenobjekt, das wir nicht fuehren. */
	(void)call(0x00u, 0xcau, 0x01u, 0x01u, NULL, 0u, 256);
	T_EQ("privates Objekt fehlt", g_sw, OSK_SW_REF_NOT_FOUND);

	/* Unbekanntes Kommando. */
	(void)call(0x00u, 0x13u, 0x00u, 0x00u, NULL, 0u, -1);
	T_EQ("unbekanntes Kommando", g_sw, OSK_SW_INS_NOT_SUPPORTED);
}

static void test_pins(void)
{
	mock_reset_all();
	osk_applets_reset();
	put_pw3();
	select_pgp();

	/* Zustand abfragen. */
	(void)call(0x00u, 0x20u, 0x00u, 0x81u, NULL, 0u, -1);
	T_EQ("PW1 noch nicht frei", g_sw & 0xfff0u, OSK_SW_PIN_TRIES);
	(void)call(0x00u, 0x20u, 0x00u, 0x83u, NULL, 0u, -1);
	T_EQ("PW3 noch nicht frei", g_sw, OSK_SW_PIN_TRIES | 3u);

	/* PW1 fuer das Unterschreiben und fuer alles andere. */
	verify_pw(0x81u, g_pw1, sizeof(g_pw1));
	T_EQ("PW1 (81) angenommen", g_sw, OSK_SW_OK);
	(void)call(0x00u, 0x20u, 0x00u, 0x81u, NULL, 0u, -1);
	T_EQ("PW1 (81) ist frei", g_sw, OSK_SW_OK);
	(void)call(0x00u, 0x20u, 0x00u, 0x82u, NULL, 0u, -1);
	T_EQ("PW1 (82) ist noch nicht frei", g_sw & 0xfff0u, OSK_SW_PIN_TRIES);
	verify_pw(0x82u, g_pw1, sizeof(g_pw1));
	T_EQ("PW1 (82) angenommen", g_sw, OSK_SW_OK);

	/* Falsche PW1 zaehlt im Chip herunter. */
	verify_pw(0x81u, (const uint8_t *)"999999", 6u);
	T_EQ("falsche PW1", g_sw & 0xfff0u, OSK_SW_PIN_TRIES);
	T_EQ("Zaehler im Chip", mock_se050_tries(), 7);

	/* PW3 aus dem Tresor. */
	verify_pw(0x83u, g_pw3, sizeof(g_pw3));
	T_EQ("PW3 angenommen", g_sw, OSK_SW_OK);
	verify_pw(0x83u, (const uint8_t *)"87654321", 8u);
	T_EQ("falsche PW3 zaehlt herunter", g_sw, OSK_SW_PIN_TRIES | 2u);
	verify_pw(0x83u, g_pw3, sizeof(g_pw3));
	T_EQ("richtige PW3 setzt den Zaehler", g_sw, OSK_SW_OK);
	T_EQ("Zaehler wieder voll", osk_card_admin_tries(OSK_CARD_ADMIN_PGP_PW3), 3);

	/* Unbekannte Kennung. */
	verify_pw(0x84u, g_pw1, sizeof(g_pw1));
	T_EQ("fremde Kennung", g_sw, OSK_SW_REF_NOT_FOUND);

	/* P1 = FF nimmt die Freigabe zurueck. */
	(void)call(0x00u, 0x20u, 0xffu, 0x81u, NULL, 0u, -1);
	T_EQ("Freigabe zurueck", g_sw, OSK_SW_OK);
	(void)call(0x00u, 0x20u, 0x00u, 0x81u, NULL, 0u, -1);
	T_EQ("nicht mehr frei", g_sw & 0xfff0u, OSK_SW_PIN_TRIES);

	/* PW1 aendern: alte und neue ohne Trenner. */
	{
		uint8_t both[16];

		memcpy(both, g_pw1, sizeof(g_pw1));
		memcpy(both + sizeof(g_pw1), "abcdefgh", 8u);
		(void)call(0x00u, 0x24u, 0x00u, 0x81u, both, sizeof(g_pw1) + 8u, -1);
		T_EQ("PW1 geaendert", g_sw, OSK_SW_OK);
		verify_pw(0x81u, (const uint8_t *)"abcdefgh", 8u);
		T_EQ("neue PW1 gilt", g_sw, OSK_SW_OK);

		/* Jetzt ist die Laenge acht - danach wird wieder richtig
		 * geteilt. */
		memcpy(both, "abcdefgh", 8u);
		memcpy(both + 8, g_pw1, sizeof(g_pw1));
		(void)call(0x00u, 0x24u, 0x00u, 0x81u, both, 8u + sizeof(g_pw1), -1);
		T_EQ("PW1 zurueckgeaendert", g_sw, OSK_SW_OK);
		verify_pw(0x81u, g_pw1, sizeof(g_pw1));
		T_EQ("alte PW1 gilt wieder", g_sw, OSK_SW_OK);
	}

	/* PW3 aendern: an der gespeicherten Laenge geteilt. */
	{
		uint8_t both[24];

		memcpy(both, g_pw3, sizeof(g_pw3));
		memcpy(both + sizeof(g_pw3), "ADMIN0000", 9u);
		(void)call(0x00u, 0x24u, 0x00u, 0x83u, both, sizeof(g_pw3) + 9u, -1);
		T_EQ("PW3 geaendert", g_sw, OSK_SW_OK);
		verify_pw(0x83u, (const uint8_t *)"ADMIN0000", 9u);
		T_EQ("neue PW3 gilt", g_sw, OSK_SW_OK);
		verify_pw(0x83u, g_pw3, sizeof(g_pw3));
		T_EQ("alte PW3 gilt nicht", g_sw, OSK_SW_PIN_TRIES | 2u);
	}
}

/* RESET RETRY COUNTER: neue PW1 mit PW3 oder mit dem Resetting Code. */
static void test_reset_retry(void)
{
	uint8_t body[16];

	mock_reset_all();
	osk_applets_reset();
	put_pw3();
	select_pgp();

	/* Ohne PW3 geht es nicht. */
	(void)call(0x00u, 0x2cu, 0x02u, 0x81u, (const uint8_t *)"000000", 6u, -1);
	T_EQ("ohne PW3", g_sw, OSK_SW_SECURITY);

	verify_pw(0x83u, g_pw3, sizeof(g_pw3));
	(void)call(0x00u, 0x2cu, 0x02u, 0x81u, (const uint8_t *)"000000", 6u, -1);
	T_EQ("mit PW3 neu gesetzt", g_sw, OSK_SW_OK);
	verify_pw(0x81u, (const uint8_t *)"000000", 6u);
	T_EQ("neue PW1 gilt", g_sw, OSK_SW_OK);

	/* Mit dem Resetting Code (Abschnitt 4.3.4). */
	body[0] = 3u;
	body[1] = 4u;
	memcpy(body + 2, "9999", 4u);
	T_EQ("Resetting Code gelegt", mock_store_put(OSK_ITEM_RECOVERY, "PGP-RC", body, 6u),
	     OSK_OK);

	(void)call(0x00u, 0x2cu, 0x00u, 0x81u, (const uint8_t *)"9999777777", 10u, -1);
	T_EQ("mit Resetting Code", g_sw, OSK_SW_OK);
	verify_pw(0x81u, (const uint8_t *)"777777", 6u);
	T_EQ("die neue PW1 gilt", g_sw, OSK_SW_OK);

	/* Falscher Code zaehlt herunter. */
	(void)call(0x00u, 0x2cu, 0x00u, 0x81u, (const uint8_t *)"1111777777", 10u, -1);
	T_EQ("falscher Code", g_sw, OSK_SW_PIN_TRIES | 2u);

	/* Zu kurzes Datenfeld. */
	(void)call(0x00u, 0x2cu, 0x00u, 0x81u, (const uint8_t *)"9999", 4u, -1);
	T_EQ("nichts Neues dabei", g_sw, OSK_SW_WRONG_LENGTH);
}

static void test_cardholder(void)
{
	mock_reset_all();
	osk_applets_reset();
	put_pw3();
	select_pgp();

	/* Schreiben braucht die Admin-PIN. */
	(void)call(0x00u, 0xdau, 0x00u, 0x5bu, (const uint8_t *)"Weirich<<Thomas", 15u, -1);
	T_EQ("ohne PW3 kein Schreiben", g_sw, OSK_SW_SECURITY);

	verify_pw(0x83u, g_pw3, sizeof(g_pw3));
	(void)call(0x00u, 0xdau, 0x00u, 0x5bu, (const uint8_t *)"Weirich<<Thomas", 15u, -1);
	T_EQ("Name geschrieben", g_sw, OSK_SW_OK);
	(void)call(0x00u, 0xdau, 0x00u, 0x5eu, (const uint8_t *)"thomas", 6u, -1);
	T_EQ("Anmeldename geschrieben", g_sw, OSK_SW_OK);
	(void)call(0x00u, 0xdau, 0x5fu, 0x50u, (const uint8_t *)"https://sorglos.org", 19u, -1);
	T_EQ("Adresse geschrieben", g_sw, OSK_SW_OK);
	(void)call(0x00u, 0xdau, 0x5fu, 0x35u, (const uint8_t *)"1", 1u, -1);
	T_EQ("Geschlecht geschrieben", g_sw, OSK_SW_OK);

	/* Fingerabdruck: genau zwanzig Bytes. */
	{
		uint8_t fpr[20];

		memset(fpr, 0xa5, sizeof(fpr));
		(void)call(0x00u, 0xdau, 0x00u, 0xc7u, fpr, sizeof(fpr), -1);
		T_EQ("Fingerabdruck geschrieben", g_sw, OSK_SW_OK);
		(void)call(0x00u, 0xdau, 0x00u, 0xc7u, fpr, 19u, -1);
		T_EQ("neunzehn Bytes abgelehnt", g_sw, OSK_SW_WRONG_LENGTH);
	}

	/* Zurueckgelesen. */
	(void)call(0x00u, 0xcau, 0x00u, 0x5eu, NULL, 0u, 256);
	T_EQ("Anmeldename gelesen", g_sw, OSK_SW_OK);
	T_EQ("Laenge", g_resp_len, 6);
	T_TRUE("Inhalt", memcmp(g_resp, "thomas", 6u) == 0);

	{
		const uint8_t *val = NULL;
		const uint8_t *inner = NULL;
		size_t len = 0;
		size_t inner_len = 0;

		(void)call(0x00u, 0xcau, 0x00u, 0x65u, NULL, 0u, 256);
		T_TRUE("Vorlage 65", osk_tlv_find(g_resp, g_resp_len, 0x65u, &val, &len));
		T_TRUE("Name drin", osk_tlv_find(val, len, 0x5bu, &inner, &inner_len));
		T_EQ("Namenslaenge", inner_len, 15);
	}

	/* Zu lang wird abgewiesen, nicht abgeschnitten. */
	{
		uint8_t big[100];

		memset(big, 'x', sizeof(big));
		(void)call(0x00u, 0xdau, 0x00u, 0x5bu, big, 65u, -1);
		T_EQ("65 Zeichen abgelehnt", g_sw, OSK_SW_WRONG_LENGTH);
		(void)call(0x00u, 0xdau, 0x00u, 0x5bu, big, 64u, -1);
		T_EQ("64 Zeichen gehen", g_sw, OSK_SW_OK);
	}

	/* Der PW-Status laesst sich umstellen. */
	(void)call(0x00u, 0xdau, 0x00u, 0xc4u, (const uint8_t *)"\x01", 1u, -1);
	T_EQ("PW-Status umgestellt", g_sw, OSK_SW_OK);
	(void)call(0x00u, 0xcau, 0x00u, 0xc4u, NULL, 0u, 256);
	T_EQ("PW-Status gelesen", g_sw, OSK_SW_OK);
	T_EQ("sieben Bytes", g_resp_len, 7);
	T_EQ("gilt fuer mehrere Unterschriften", g_resp[0], 0x01);
	(void)call(0x00u, 0xdau, 0x00u, 0xc4u, (const uint8_t *)"\x02", 1u, -1);
	T_EQ("nur 0 oder 1", g_sw, OSK_SW_WRONG_DATA);

	/* Der Zustand haelt ueber das Aus- und Einschalten: er liegt im
	 * Tresor, nicht im RAM. */
	osk_applets_reset();
	select_pgp();
	(void)call(0x00u, 0xcau, 0x00u, 0x5eu, NULL, 0u, 256);
	T_EQ("nach dem Einschalten noch da", g_resp_len, 6);
	T_TRUE("und gleich", memcmp(g_resp, "thomas", 6u) == 0);
}

static void test_keys(void)
{
	static const uint8_t crt_sign[] = {0xb6, 0x00};
	static const uint8_t crt_dec[] = {0xb8, 0x00};
	static const uint8_t crt_auth[] = {0xa4, 0x00};
	const uint8_t *val = NULL;
	const uint8_t *point = NULL;
	size_t len = 0;
	size_t point_len = 0;
	unsigned before;

	mock_reset_all();
	osk_applets_reset();
	put_pw3();
	select_pgp();

	/* Erzeugen braucht die Admin-PIN. */
	(void)call(0x00u, 0x47u, 0x80u, 0x00u, crt_sign, sizeof(crt_sign), 256);
	T_EQ("ohne PW3", g_sw, OSK_SW_SECURITY);

	verify_pw(0x83u, g_pw3, sizeof(g_pw3));
	before = mock_ui_confirm_count();
	(void)call(0x00u, 0x47u, 0x80u, 0x00u, crt_sign, sizeof(crt_sign), 256);
	T_EQ("Signaturschluessel erzeugt", g_sw, OSK_SW_OK);
	T_TRUE("am Display gefragt", mock_ui_confirm_count() > before);
	T_TRUE("im Chip", mock_se050_has_key(OSK_PGP_KEY_ID(OSK_PGP_KEY_SIGN)));
	T_TRUE("Vorlage 7F49", osk_tlv_find(g_resp, g_resp_len, 0x7f49u, &val, &len));
	T_TRUE("Punkt drin", osk_tlv_find(val, len, 0x86u, &point, &point_len));
	T_EQ("65 Bytes", point_len, 65);
	T_EQ("unkomprimiert", point[0], 0x04);

	(void)call(0x00u, 0x47u, 0x80u, 0x00u, crt_dec, sizeof(crt_dec), 256);
	T_EQ("Verschluesselungsschluessel", g_sw, OSK_SW_OK);
	(void)call(0x00u, 0x47u, 0x80u, 0x00u, crt_auth, sizeof(crt_auth), 256);
	T_EQ("Anmeldeschluessel", g_sw, OSK_SW_OK);

	/* Oeffentlichen Schluessel lesen braucht keine PIN (P1 = 81). */
	osk_applets_reset();
	select_pgp();
	(void)call(0x00u, 0x47u, 0x81u, 0x00u, crt_sign, sizeof(crt_sign), 256);
	T_EQ("Lesen ohne PIN", g_sw, OSK_SW_OK);
	T_TRUE("wieder ein Punkt", osk_tlv_find(g_resp, g_resp_len, 0x7f49u, &val, &len));

	/* Eine Vorlage, die keinen Schluessel nennt. */
	(void)call(0x00u, 0x47u, 0x81u, 0x00u, (const uint8_t *)"\x99\x00", 2u, 256);
	T_EQ("keine Schluesselvorlage", g_sw, OSK_SW_WRONG_DATA);
	(void)call(0x00u, 0x47u, 0x90u, 0x00u, crt_sign, sizeof(crt_sign), 256);
	T_EQ("falsches P1", g_sw, OSK_SW_WRONG_P1P2);
}

static void test_sign_and_decrypt(void)
{
	static const uint8_t crt_sign[] = {0xb6, 0x00};
	static const uint8_t crt_dec[] = {0xb8, 0x00};
	static const uint8_t crt_auth[] = {0xa4, 0x00};
	uint8_t hash[32];
	unsigned before;

	mock_reset_all();
	osk_applets_reset();
	put_pw3();
	select_pgp();
	verify_pw(0x83u, g_pw3, sizeof(g_pw3));
	(void)call(0x00u, 0x47u, 0x80u, 0x00u, crt_sign, sizeof(crt_sign), 256);
	(void)call(0x00u, 0x47u, 0x80u, 0x00u, crt_dec, sizeof(crt_dec), 256);
	(void)call(0x00u, 0x47u, 0x80u, 0x00u, crt_auth, sizeof(crt_auth), 256);
	memset(hash, 0x5a, sizeof(hash));

	/* Unterschreiben ohne PW1 geht nicht. */
	(void)call(0x00u, 0x2au, 0x9eu, 0x9au, hash, sizeof(hash), 256);
	T_EQ("ohne PW1", g_sw, OSK_SW_SECURITY);

	verify_pw(0x81u, g_pw1, sizeof(g_pw1));
	before = mock_ui_confirm_count();
	(void)call(0x00u, 0x2au, 0x9eu, 0x9au, hash, sizeof(hash), 256);
	T_EQ("unterschrieben", g_sw, OSK_SW_OK);
	/* OpenPGP will r und s nackt, nicht als DER. */
	T_EQ("64 Bytes flach", g_resp_len, 64);
	T_EQ("einmal gefragt", mock_ui_confirm_count(), before + 1u);
	T_EQ("Art der Frage", mock_ui_last_kind(), (int)OSK_ASK_SIGN);

	/* PW1 gilt nur fuer eine Unterschrift (PW-Status 0). */
	(void)call(0x00u, 0x2au, 0x9eu, 0x9au, hash, sizeof(hash), 256);
	T_EQ("zweite Unterschrift braucht die PIN neu", g_sw, OSK_SW_SECURITY);

	/* Auf 0x01 umgestellt gilt sie weiter. */
	verify_pw(0x83u, g_pw3, sizeof(g_pw3));
	(void)call(0x00u, 0xdau, 0x00u, 0xc4u, (const uint8_t *)"\x01", 1u, -1);
	T_EQ("Status umgestellt", g_sw, OSK_SW_OK);
	verify_pw(0x81u, g_pw1, sizeof(g_pw1));
	(void)call(0x00u, 0x2au, 0x9eu, 0x9au, hash, sizeof(hash), 256);
	T_EQ("erste", g_sw, OSK_SW_OK);
	(void)call(0x00u, 0x2au, 0x9eu, 0x9au, hash, sizeof(hash), 256);
	T_EQ("zweite geht jetzt", g_sw, OSK_SW_OK);

	/* Der Zaehler der Unterschriften laeuft mit. */
	{
		const uint8_t *val = NULL;
		const uint8_t *inner = NULL;
		size_t len = 0;
		size_t inner_len = 0;

		(void)call(0x00u, 0xcau, 0x00u, 0x7au, NULL, 0u, 256);
		T_TRUE("Vorlage 7A", osk_tlv_find(g_resp, g_resp_len, 0x7au, &val, &len));
		T_TRUE("DO 93", osk_tlv_find(val, len, 0x93u, &inner, &inner_len));
		T_EQ("drei Unterschriften", inner[2], 3);
	}

	/* Der Nutzer sagt nein. */
	mock_ui_set_confirm(OSK_ERR_DENIED);
	(void)call(0x00u, 0x2au, 0x9eu, 0x9au, hash, sizeof(hash), 256);
	T_EQ("abgelehnt", g_sw, OSK_SW_SECURITY);
	T_EQ("nichts heraus", g_resp_len, 0);
	mock_ui_set_confirm(OSK_OK);

	/* Entschluesseln: A6 -> 7F49 -> 86 mit dem fremden Punkt. */
	{
		uint8_t req[80];
		size_t at = 0;

		req[at++] = 0xa6u;
		req[at++] = 0x46u;
		req[at++] = 0x7fu;
		req[at++] = 0x49u;
		req[at++] = 0x43u;
		req[at++] = 0x86u;
		req[at++] = 0x41u;
		req[at++] = 0x04u;
		memset(req + at, 0x77, 64u);
		at += 64u;

		(void)call(0x00u, 0x2au, 0x80u, 0x86u, req, at, 256);
		T_EQ("ohne PW1 (82)", g_sw, OSK_SW_SECURITY);
		verify_pw(0x82u, g_pw1, sizeof(g_pw1));
		(void)call(0x00u, 0x2au, 0x80u, 0x86u, req, at, 256);
		T_EQ("entschluesselt", g_sw, OSK_SW_OK);
		T_EQ("Geheimnis ist 32 Bytes", g_resp_len, 32);
		T_EQ("als Entschluesselung angekuendigt", mock_ui_last_kind(),
		     (int)OSK_ASK_DECRYPT);

		/* Punkt in der falschen Laenge. */
		req[6] = 0x21u;
		(void)call(0x00u, 0x2au, 0x80u, 0x86u, req, 40u, 256);
		T_EQ("falsche Punktlaenge", g_sw, OSK_SW_WRONG_DATA);
		/* Fehlende Huelle. */
		(void)call(0x00u, 0x2au, 0x80u, 0x86u, hash, sizeof(hash), 256);
		T_EQ("keine Huelle A6", g_sw, OSK_SW_WRONG_DATA);
	}

	/* Anmelden mit dem dritten Schluessel. */
	(void)call(0x00u, 0x88u, 0x00u, 0x00u, hash, sizeof(hash), 256);
	T_EQ("angemeldet", g_sw, OSK_SW_OK);
	T_EQ("auch hier flach", g_resp_len, 64);
	T_EQ("als Anmeldung angekuendigt", mock_ui_last_kind(), (int)OSK_ASK_LOGIN);
	(void)call(0x00u, 0x88u, 0x01u, 0x00u, hash, sizeof(hash), 256);
	T_EQ("falsches P1", g_sw, OSK_SW_WRONG_P1P2);

	/* Ein zu langer Hash wird abgewiesen. */
	{
		uint8_t big[64];

		memset(big, 0x11, sizeof(big));
		(void)call(0x00u, 0x2au, 0x9eu, 0x9au, big, sizeof(big), 256);
		T_EQ("zu langer Hash", g_sw, OSK_SW_WRONG_DATA);
	}
}

/* TERMINATE DF und ACTIVATE FILE: Karte auf Werkszustand. */
static void test_terminate(void)
{
	static const uint8_t crt_sign[] = {0xb6, 0x00};

	mock_reset_all();
	osk_applets_reset();
	put_pw3();
	select_pgp();
	verify_pw(0x83u, g_pw3, sizeof(g_pw3));
	(void)call(0x00u, 0x47u, 0x80u, 0x00u, crt_sign, sizeof(crt_sign), 256);
	T_EQ("Schluessel da", g_sw, OSK_SW_OK);
	(void)call(0x00u, 0xdau, 0x00u, 0x5eu, (const uint8_t *)"thomas", 6u, -1);
	T_EQ("Anmeldename da", g_sw, OSK_SW_OK);

	(void)call(0x00u, 0xe6u, 0x00u, 0x00u, NULL, 0u, -1);
	T_EQ("beendet", g_sw, OSK_SW_OK);

	/* Danach geht nur noch ACTIVATE FILE. */
	(void)call(0x00u, 0xcau, 0x00u, 0x5eu, NULL, 0u, 256);
	T_EQ("nichts mehr erlaubt", g_sw, OSK_SW_COND_NOT_SATISFIED);

	(void)call(0x00u, 0x44u, 0x00u, 0x00u, NULL, 0u, -1);
	T_EQ("wieder aktiv", g_sw, OSK_SW_OK);
	T_TRUE("Schluessel weg", !mock_se050_has_key(OSK_PGP_KEY_ID(OSK_PGP_KEY_SIGN)));
	(void)call(0x00u, 0xcau, 0x00u, 0x5eu, NULL, 0u, 256);
	T_EQ("Anmeldename gelesen", g_sw, OSK_SW_OK);
	T_EQ("und leer", g_resp_len, 0);

	/* Ohne PW3 und mit nicht gesperrter PW3 geht TERMINATE nicht. */
	osk_applets_reset();
	select_pgp();
	(void)call(0x00u, 0xe6u, 0x00u, 0x00u, NULL, 0u, -1);
	T_EQ("ohne PW3 kein Beenden", g_sw, OSK_SW_COND_NOT_SATISFIED);
}

int main(void)
{
	setvbuf(stdout, NULL, _IONBF, 0);
	mock_reset_all();
	T_EQ("Anwendungen bereit", osk_applets_init(), OSK_OK);

	test_select_and_data();
	test_pins();
	test_reset_retry();
	test_cardholder();
	test_keys();
	test_sign_and_decrypt();
	test_terminate();
	return t_report("test_openpgp");
}
