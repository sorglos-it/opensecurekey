/* PIV gegen NIST SP 800-73-4: echte APDUs hinein, Antwort und Statuswort
 * gegen die Spezifikation.
 *
 * Alles laeuft am PC gegen Attrappen. Die Unterschriften haben die
 * richtige Form, nicht den richtigen Wert - rechnen wird der SE050. */
#include "test_util.h"
#include "osk_applets.h"
#include "osk_piv.h"
#include "osk_apdu.h"
#include "osk_card.h"
#include "osk_ui.h"
#include "osk_se050.h"
#include "osk_rsa_card.h"
#include "mocks.h"
#include "card_helper.h"

static const uint8_t g_pin[8] = {'1', '2', '3', '4', '5', '6', 0xff, 0xff};

static void select_piv(void)
{
	T_EQ("SELECT laeuft", call(0x00u, 0xa4u, 0x04u, 0x00u, osk_piv_aid, OSK_PIV_AID_LEN, 256),
	     OSK_OK);
	T_EQ("SELECT ok", g_sw, OSK_SW_OK);
}

static void verify_pin(void)
{
	(void)call(0x00u, 0x20u, 0x00u, 0x80u, g_pin, sizeof(g_pin), -1);
	T_EQ("PIN angenommen", g_sw, OSK_SW_OK);
}

static void test_select(void)
{
	const uint8_t *val = NULL;
	size_t len = 0;

	osk_applets_reset();

	/* Ohne Auswahl geht nichts. */
	(void)call(0x00u, 0xcbu, 0x3fu, 0xffu, NULL, 0u, 256);
	T_EQ("ohne Auswahl", g_sw, OSK_SW_COND_NOT_SATISFIED);

	select_piv();
	T_TRUE("Vorlage 61", osk_tlv_find(g_resp, g_resp_len, 0x61u, &val, &len));
	{
		const uint8_t *aid = NULL;
		size_t aid_len = 0;
		const uint8_t *label = NULL;
		size_t label_len = 0;
		const uint8_t *algs = NULL;
		size_t algs_len = 0;

		T_TRUE("Kennung drin", osk_tlv_find(val, len, 0x4fu, &aid, &aid_len));
		T_TRUE("Zuteilungsstelle", osk_tlv_find(val, len, 0x79u, NULL, NULL));
		T_TRUE("Name der Anwendung", osk_tlv_find(val, len, 0x50u, &label, &label_len));
		T_TRUE("Name ist unserer", label_len > 4u && label[0] == 'O');
		T_TRUE("Algorithmenliste", osk_tlv_find(val, len, 0xacu, &algs, &algs_len));
		T_TRUE("P-256 dabei", osk_tlv_find(algs, algs_len, 0x80u, NULL, NULL));
	}

	/* Auswahl mit abgekuerzter Kennung - so machen es die Treiber. */
	T_EQ("abgekuerzte Kennung", call(0x00u, 0xa4u, 0x04u, 0x00u, osk_piv_aid, 9u, 256), OSK_OK);
	T_EQ("abgekuerzt ok", g_sw, OSK_SW_OK);

	/* Falsches P1 oder P2. */
	(void)call(0x00u, 0xa4u, 0x00u, 0x00u, osk_piv_aid, OSK_PIV_AID_LEN, 256);
	T_EQ("P1 falsch", g_sw, OSK_SW_WRONG_P1P2);
	(void)call(0x00u, 0xa4u, 0x04u, 0x02u, osk_piv_aid, OSK_PIV_AID_LEN, 256);
	T_EQ("P2 falsch", g_sw, OSK_SW_WRONG_P1P2);

	/* CLA mit sicherer Nachrichtenuebermittlung und fremder Kanal. */
	select_piv();
	(void)call(0x0cu, 0xcbu, 0x3fu, 0xffu, NULL, 0u, 256);
	T_EQ("SM abgelehnt", g_sw, 0x6882);
	(void)call(0x01u, 0xcbu, 0x3fu, 0xffu, NULL, 0u, 256);
	T_EQ("Kanal abgelehnt", g_sw, OSK_SW_CLA_NOT_SUPPORTED);
	(void)call(0x40u, 0xcbu, 0x3fu, 0xffu, NULL, 0u, 256);
	T_EQ("fremde CLA abgelehnt", g_sw, OSK_SW_CLA_NOT_SUPPORTED);

	/* Unbekanntes Kommando. */
	(void)call(0x00u, 0x12u, 0x00u, 0x00u, NULL, 0u, -1);
	T_EQ("unbekanntes Kommando", g_sw, OSK_SW_INS_NOT_SUPPORTED);
}

static void test_pin(void)
{
	uint8_t wrong[8];

	osk_applets_reset();
	mock_reset_all();
	select_piv();

	/* Nachfrage nach dem Zustand: noch nicht freigegeben. */
	(void)call(0x00u, 0x20u, 0x00u, 0x80u, NULL, 0u, -1);
	T_EQ("Zustand vor der PIN", g_sw & 0xfff0u, OSK_SW_PIN_TRIES);
	T_EQ("acht Versuche", g_sw & 0x000fu, 8);

	/* Falsche PIN zaehlt herunter. */
	memcpy(wrong, g_pin, sizeof(wrong));
	wrong[0] = '9';
	(void)call(0x00u, 0x20u, 0x00u, 0x80u, wrong, sizeof(wrong), -1);
	T_EQ("falsche PIN", g_sw, OSK_SW_PIN_TRIES | 7u);
	T_EQ("Zaehler im Chip", mock_se050_tries(), 7);

	verify_pin();
	T_EQ("Zaehler wieder voll", mock_se050_tries(), 8);

	/* Zustand nach der PIN. */
	(void)call(0x00u, 0x20u, 0x00u, 0x80u, NULL, 0u, -1);
	T_EQ("Zustand nach der PIN", g_sw, OSK_SW_OK);

	/* P1 = FF nimmt die Freigabe zurueck. */
	(void)call(0x00u, 0x20u, 0xffu, 0x80u, NULL, 0u, -1);
	T_EQ("Freigabe zurueck", g_sw, OSK_SW_OK);
	(void)call(0x00u, 0x20u, 0x00u, 0x80u, NULL, 0u, -1);
	T_EQ("wieder nicht freigegeben", g_sw & 0xfff0u, OSK_SW_PIN_TRIES);

	/* Falsche Schluesselkennung. */
	(void)call(0x00u, 0x20u, 0x00u, 0x85u, g_pin, sizeof(g_pin), -1);
	T_EQ("fremde Kennung", g_sw, OSK_SW_REF_NOT_FOUND);

	/* Das Feld muss genau acht Bytes haben, und 0xFF nur am Ende. */
	(void)call(0x00u, 0x20u, 0x00u, 0x80u, g_pin, 6u, -1);
	T_EQ("sechs Bytes abgelehnt", g_sw, OSK_SW_WRONG_LENGTH);
	{
		uint8_t odd[8];

		memcpy(odd, g_pin, sizeof(odd));
		odd[2] = 0xffu;
		(void)call(0x00u, 0x20u, 0x00u, 0x80u, odd, sizeof(odd), -1);
		T_EQ("Fuellbyte in der Mitte", g_sw, OSK_SW_WRONG_LENGTH);
	}

	/* PIN aendern und mit der neuen anmelden. */
	{
		uint8_t both[16];

		memcpy(both, g_pin, 8u);
		memcpy(both + 8, g_pin, 8u);
		both[8] = '9';
		(void)call(0x00u, 0x24u, 0x00u, 0x80u, both, sizeof(both), -1);
		T_EQ("PIN geaendert", g_sw, OSK_SW_OK);

		(void)call(0x00u, 0x20u, 0x00u, 0x80u, g_pin, sizeof(g_pin), -1);
		T_EQ("alte PIN gilt nicht mehr", g_sw & 0xfff0u, OSK_SW_PIN_TRIES);
		(void)call(0x00u, 0x20u, 0x00u, 0x80u, both + 8, 8u, -1);
		T_EQ("neue PIN gilt", g_sw, OSK_SW_OK);
	}

	/* Gesperrt nach acht Fehlversuchen. */
	mock_reset_all();
	osk_applets_reset();
	select_piv();
	for (unsigned i = 0; i < 8u; i++)
		(void)call(0x00u, 0x20u, 0x00u, 0x80u, wrong, sizeof(wrong), -1);
	T_EQ("gesperrt", g_sw, OSK_SW_AUTH_BLOCKED);
	(void)call(0x00u, 0x20u, 0x00u, 0x80u, g_pin, sizeof(g_pin), -1);
	T_EQ("auch die richtige hilft nicht mehr", g_sw, OSK_SW_AUTH_BLOCKED);
}

/* RESET RETRY COUNTER mit dem PUK aus dem Tresor. */
static void test_puk(void)
{
	uint8_t body[16];
	uint8_t data[16];

	mock_reset_all();
	osk_applets_reset();
	select_piv();

	/* Ohne PUK im Tresor gibt es nichts zu pruefen. */
	memset(data, 0xff, sizeof(data));
	memcpy(data, "12345678", 8u);
	memcpy(data + 8, g_pin, 8u);
	(void)call(0x00u, 0x2cu, 0x00u, 0x80u, data, sizeof(data), -1);
	T_EQ("kein PUK vorhanden", g_sw, OSK_SW_REF_NOT_FOUND);

	/* PUK anlegen: Zaehler, Laenge, Wert. */
	body[0] = 3u;
	body[1] = 8u;
	memcpy(body + 2, "12345678", 8u);
	T_EQ("PUK gelegt", mock_store_put(OSK_ITEM_RECOVERY, "PIV-PUK", body, 10u), OSK_OK);

	/* Falscher PUK zaehlt herunter. */
	{
		uint8_t bad[16];

		memset(bad, 0xff, sizeof(bad));
		memcpy(bad, "87654321", 8u);
		memcpy(bad + 8, g_pin, 8u);
		(void)call(0x00u, 0x2cu, 0x00u, 0x80u, bad, sizeof(bad), -1);
		T_EQ("falscher PUK", g_sw, OSK_SW_PIN_TRIES | 2u);
	}

	/* Richtiger PUK setzt die PIN neu. */
	(void)call(0x00u, 0x2cu, 0x00u, 0x80u, data, sizeof(data), -1);
	T_EQ("PIN zurueckgesetzt", g_sw, OSK_SW_OK);
	verify_pin();

	/* Nach dem Erfolg ist der Zaehler wieder voll. */
	T_EQ("PUK-Zaehler wieder voll", osk_card_admin_tries(OSK_CARD_ADMIN_PIV_PUK), 3);
}

static void test_keys_and_sign(void)
{
	static const uint8_t tmpl_p256[] = {0xac, 0x03, 0x80, 0x01, OSK_PIV_ALG_P256};
	static const uint8_t tmpl_rsa[] = {0xac, 0x03, 0x80, 0x01, 0x07};
	uint8_t hash[32];
	uint8_t sign_req[64];
	const uint8_t *val = NULL;
	size_t len = 0;
	unsigned before;

	mock_reset_all();
	osk_applets_reset();
	select_piv();

	for (unsigned i = 0; i < sizeof(hash); i++)
		hash[i] = (uint8_t)(i + 1u);

	/* Erzeugen braucht die PIN. */
	(void)call(0x00u, 0x47u, 0x00u, OSK_PIV_SLOT_AUTH, tmpl_p256, sizeof(tmpl_p256), 256);
	T_EQ("erzeugen ohne PIN", g_sw, OSK_SW_SECURITY);

	verify_pin();
	(void)call(0x00u, 0x47u, 0x00u, 0x9fu, tmpl_p256, sizeof(tmpl_p256), 256);
	T_EQ("fremder Steckplatz", g_sw, OSK_SW_REF_NOT_FOUND);
	/* Bis zum 29.10.2026 stand hier "RSA gibt es nicht". Jetzt gibt es
	 * RSA-2048 (Kennung 07, SP 800-73-4 Teil 1 Tabelle 6-2) - die
	 * Antwort ist 61xx, weil ein 256-Byte-Modulus nicht in eine kurze
	 * Antwort passt. Ausfuehrlich geprueft wird das in
	 * test_rsa_keys(). */
	(void)call(0x00u, 0x47u, 0x00u, OSK_PIV_SLOT_AUTH, tmpl_rsa, sizeof(tmpl_rsa), 256);
	if (OSK_RSA_BUILT)
		T_TRUE("RSA-2048 geht jetzt",
		       g_sw == OSK_SW_OK || (g_sw & 0xff00u) == OSK_SW_MORE);
	else
		T_EQ("ohne CONFIG_OSK_RSA weiter nicht", g_sw, OSK_SW_FUNC_NOT_SUPPORTED);
	/* Eine Kennung, die es in SP 800-73-4 nicht gibt, bleibt abgelehnt:
	 * 0x05 waere RSA-3072 nach SP 800-73-5, und die liegt hier nicht
	 * vor - geraten wird nichts (siehe osk_piv.h). */
	{
		static const uint8_t tmpl_rsa3072[] = {0xac, 0x03, 0x80, 0x01, 0x05};

		(void)call(0x00u, 0x47u, 0x00u, OSK_PIV_SLOT_AUTH, tmpl_rsa3072,
			   sizeof(tmpl_rsa3072), 256);
		T_EQ("RSA-3072 hat in dieser Fassung keine Kennung", g_sw,
		     OSK_SW_FUNC_NOT_SUPPORTED);
	}
	/* Der RSA-Schluessel aus dem Versuch oben wird gleich durch eine
	 * Kurve ersetzt - der Steckplatz haelt genau einen Schluessel. */

	before = mock_ui_confirm_count();
	(void)call(0x00u, 0x47u, 0x00u, OSK_PIV_SLOT_AUTH, tmpl_p256, sizeof(tmpl_p256), 256);
	T_EQ("Schluessel erzeugt", g_sw, OSK_SW_OK);
	T_TRUE("am Display gefragt", mock_ui_confirm_count() > before);
	T_TRUE("Steckplatz stand da", strstr(mock_ui_last_subject(), "9A") != NULL);
	T_TRUE("im Chip angelegt", mock_se050_has_key(OSK_PIV_KEY_ID(OSK_PIV_SLOT_AUTH)));
	T_TRUE("Vorlage 7F49", osk_tlv_find(g_resp, g_resp_len, 0x7f49u, &val, &len));
	{
		const uint8_t *point = NULL;
		size_t point_len = 0;

		T_TRUE("Punkt dabei", osk_tlv_find(val, len, 0x86u, &point, &point_len));
		T_EQ("65 Bytes fuer P-256", point_len, 65);
		T_EQ("unkomprimiert", point[0], 0x04);
	}

	/* Unterschreiben: 7C mit 81. */
	sign_req[0] = 0x7cu;
	sign_req[1] = 0x22u;
	sign_req[2] = 0x81u;
	sign_req[3] = 0x20u;
	memcpy(sign_req + 4, hash, 32u);

	before = mock_ui_confirm_count();
	(void)call(0x00u, 0x87u, OSK_PIV_ALG_P256, OSK_PIV_SLOT_AUTH, sign_req, 36u, 256);
	T_EQ("unterschrieben", g_sw, OSK_SW_OK);
	T_EQ("genau einmal gefragt", mock_ui_confirm_count(), before + 1u);
	T_EQ("Art der Frage ist Unterschrift", mock_ui_last_kind(), (int)OSK_ASK_SIGN);
	T_TRUE("Dienst stand da", strstr(mock_ui_last_subject(), "PIV") != NULL);
	T_TRUE("Vorlage 7C", osk_tlv_find(g_resp, g_resp_len, 0x7cu, &val, &len));
	{
		const uint8_t *sig = NULL;
		size_t sig_len = 0;

		T_TRUE("Unterschrift dabei", osk_tlv_find(val, len, 0x82u, &sig, &sig_len));
		T_EQ("DER-Folge", sig[0], 0x30);
		T_TRUE("Laenge plausibel", sig_len > 8u && sig_len < 80u);
	}

	/* Der Nutzer sagt nein: keine Unterschrift. */
	mock_ui_set_confirm(OSK_ERR_DENIED);
	(void)call(0x00u, 0x87u, OSK_PIV_ALG_P256, OSK_PIV_SLOT_AUTH, sign_req, 36u, 256);
	T_EQ("abgelehnt am Display", g_sw, OSK_SW_SECURITY);
	T_EQ("keine Daten heraus", g_resp_len, 0);
	mock_ui_set_confirm(OSK_OK);

	/* Falsche Algorithmuskennung zum Schluessel. */
	(void)call(0x00u, 0x87u, OSK_PIV_ALG_P384, OSK_PIV_SLOT_AUTH, sign_req, 36u, 256);
	T_EQ("Kurve passt nicht", g_sw, OSK_SW_WRONG_P1P2);

	/* Steckplatz ohne Schluessel. */
	(void)call(0x00u, 0x87u, OSK_PIV_ALG_P256, OSK_PIV_SLOT_CARD, sign_req, 36u, 256);
	T_EQ("kein Schluessel", g_sw, OSK_SW_REF_NOT_FOUND);

	/* Hash laenger als die Kurve. */
	{
		uint8_t big[80];

		big[0] = 0x7cu;
		big[1] = 0x32u;
		big[2] = 0x81u;
		big[3] = 0x30u;
		memset(big + 4, 0x55, 48u);
		(void)call(0x00u, 0x87u, OSK_PIV_ALG_P256, OSK_PIV_SLOT_AUTH, big, 52u, 256);
		T_EQ("zu langer Hash", g_sw, OSK_SW_WRONG_DATA);
	}

	/* Leere Vorlage. */
	{
		static const uint8_t empty[] = {0x7c, 0x00};

		(void)call(0x00u, 0x87u, OSK_PIV_ALG_P256, OSK_PIV_SLOT_AUTH, empty, sizeof(empty),
			   256);
		T_EQ("nichts zu tun", g_sw, OSK_SW_WRONG_DATA);
	}
}

/* 9C verlangt die PIN unmittelbar vor der Unterschrift. */
static void test_pin_always(void)
{
	static const uint8_t tmpl[] = {0xac, 0x03, 0x80, 0x01, OSK_PIV_ALG_P256};
	uint8_t sign_req[40];

	mock_reset_all();
	osk_applets_reset();
	select_piv();
	verify_pin();
	(void)call(0x00u, 0x47u, 0x00u, OSK_PIV_SLOT_SIGN, tmpl, sizeof(tmpl), 256);
	T_EQ("Signaturschluessel erzeugt", g_sw, OSK_SW_OK);

	sign_req[0] = 0x7cu;
	sign_req[1] = 0x22u;
	sign_req[2] = 0x81u;
	sign_req[3] = 0x20u;
	memset(sign_req + 4, 0x33, 32u);

	/* Das Erzeugen stand zwischen VERIFY und Unterschrift - also neu
	 * freigeben. */
	(void)call(0x00u, 0x87u, OSK_PIV_ALG_P256, OSK_PIV_SLOT_SIGN, sign_req, 36u, 256);
	T_EQ("9C ohne frisches VERIFY", g_sw, OSK_SW_SECURITY);

	verify_pin();
	(void)call(0x00u, 0x87u, OSK_PIV_ALG_P256, OSK_PIV_SLOT_SIGN, sign_req, 36u, 256);
	T_EQ("9C nach frischem VERIFY", g_sw, OSK_SW_OK);

	/* Zweimal hintereinander geht nicht. */
	(void)call(0x00u, 0x87u, OSK_PIV_ALG_P256, OSK_PIV_SLOT_SIGN, sign_req, 36u, 256);
	T_EQ("9C zweimal", g_sw, OSK_SW_SECURITY);
}

/* 9E ist die Kartenauthentisierung - ohne PIN, aber mit Tastendruck. */
static void test_card_auth(void)
{
	static const uint8_t tmpl[] = {0xac, 0x03, 0x80, 0x01, OSK_PIV_ALG_P256};
	uint8_t sign_req[40];
	unsigned before;

	mock_reset_all();
	osk_applets_reset();
	select_piv();
	verify_pin();
	(void)call(0x00u, 0x47u, 0x00u, OSK_PIV_SLOT_CARD, tmpl, sizeof(tmpl), 256);
	T_EQ("Kartenschluessel erzeugt", g_sw, OSK_SW_OK);

	/* Freigabe zuruecknehmen - 9E braucht sie nicht. */
	(void)call(0x00u, 0x20u, 0xffu, 0x80u, NULL, 0u, -1);

	sign_req[0] = 0x7cu;
	sign_req[1] = 0x22u;
	sign_req[2] = 0x81u;
	sign_req[3] = 0x20u;
	memset(sign_req + 4, 0x44, 32u);

	before = mock_ui_confirm_count();
	(void)call(0x00u, 0x87u, OSK_PIV_ALG_P256, OSK_PIV_SLOT_CARD, sign_req, 36u, 256);
	T_EQ("9E ohne PIN", g_sw, OSK_SW_OK);
	T_EQ("aber mit Taste", mock_ui_confirm_count(), before + 1u);
}

/* Schluesselaustausch mit 9D. */
static void test_keyagree(void)
{
	static const uint8_t tmpl[] = {0xac, 0x03, 0x80, 0x01, OSK_PIV_ALG_P256};
	uint8_t req[80];
	const uint8_t *val = NULL;
	size_t len = 0;

	mock_reset_all();
	osk_applets_reset();
	select_piv();
	verify_pin();
	(void)call(0x00u, 0x47u, 0x00u, OSK_PIV_SLOT_KEYMGMT, tmpl, sizeof(tmpl), 256);
	T_EQ("Schluessel 9D erzeugt", g_sw, OSK_SW_OK);

	req[0] = 0x7cu;
	req[1] = 0x43u;
	req[2] = 0x85u;
	req[3] = 0x41u;
	req[4] = 0x04u;
	memset(req + 5, 0x22, 64u);

	(void)call(0x00u, 0x87u, OSK_PIV_ALG_P256, OSK_PIV_SLOT_KEYMGMT, req, 69u, 256);
	T_EQ("Austausch gelaufen", g_sw, OSK_SW_OK);
	T_EQ("als Entschluesselung angekuendigt", mock_ui_last_kind(), (int)OSK_ASK_DECRYPT);
	T_TRUE("Vorlage 7C", osk_tlv_find(g_resp, g_resp_len, 0x7cu, &val, &len));
	T_TRUE("Geheimnis dabei", osk_tlv_find(val, len, 0x82u, NULL, NULL));

	/* Ein Punkt in der falschen Laenge wird abgewiesen. */
	req[3] = 0x20u;
	(void)call(0x00u, 0x87u, OSK_PIV_ALG_P256, OSK_PIV_SLOT_KEYMGMT, req, 36u, 256);
	T_EQ("falsche Punktlaenge", g_sw, OSK_SW_WRONG_DATA);

	/* Austausch nur mit 9D. */
	req[3] = 0x41u;
	(void)call(0x00u, 0x87u, OSK_PIV_ALG_P256, OSK_PIV_SLOT_AUTH, req, 69u, 256);
	T_EQ("9A kann das nicht", g_sw, OSK_SW_REF_NOT_FOUND);
}

static void test_data_objects(void)
{
	uint8_t cert[300];
	uint8_t cmd[512];
	const uint8_t *val = NULL;
	size_t len = 0;
	size_t at;
	unsigned reveals;

	mock_reset_all();
	osk_applets_reset();
	select_piv();

	for (size_t i = 0; i < sizeof(cert); i++)
		cert[i] = (uint8_t)(i * 3u + 7u);

	/* Das Discovery Object wird gerechnet, nicht gespeichert. */
	{
		static const uint8_t ask[] = {0x5c, 0x01, 0x7e};

		(void)call(0x00u, 0xcbu, 0x3fu, 0xffu, ask, sizeof(ask), 256);
		T_EQ("Discovery Object", g_sw, OSK_SW_OK);
		T_EQ("Tag 7E", g_resp[0], 0x7e);
		T_TRUE("Kennung drin", osk_tlv_find(g_resp, g_resp_len, 0x7eu, &val, &len));
		T_TRUE("AID drin", osk_tlv_find(val, len, 0x4fu, NULL, NULL));
		T_TRUE("PIN-Regel drin", osk_tlv_find(val, len, 0x5f2fu, NULL, NULL));
	}

	/* Ein Objekt, das es nicht gibt. */
	{
		static const uint8_t ask[] = {0x5c, 0x03, 0x5f, 0xc1, 0x05};

		(void)call(0x00u, 0xcbu, 0x3fu, 0xffu, ask, sizeof(ask), 256);
		T_EQ("Zertifikat fehlt noch", g_sw, OSK_SW_FILE_NOT_FOUND);
	}
	{
		static const uint8_t ask[] = {0x5c, 0x03, 0x5f, 0xc1, 0x77};

		(void)call(0x00u, 0xcbu, 0x3fu, 0xffu, ask, sizeof(ask), 256);
		T_EQ("unbekanntes Objekt", g_sw, OSK_SW_FILE_NOT_FOUND);
	}
	/* Ohne 5C im Datenfeld. */
	(void)call(0x00u, 0xcbu, 0x3fu, 0xffu, cert, 4u, 256);
	T_EQ("kein 5C", g_sw, OSK_SW_WRONG_DATA);
	/* Falsche Datei. */
	{
		static const uint8_t ask[] = {0x5c, 0x01, 0x7e};

		(void)call(0x00u, 0xcbu, 0x00u, 0x00u, ask, sizeof(ask), 256);
		T_EQ("falsches P1P2", g_sw, OSK_SW_WRONG_P1P2);
	}

	/* Schreiben braucht die PIN. */
	at = 0;
	cmd[at++] = 0x5cu;
	cmd[at++] = 0x03u;
	cmd[at++] = 0x5fu;
	cmd[at++] = 0xc1u;
	cmd[at++] = 0x05u;
	cmd[at++] = 0x53u;
	cmd[at++] = 0x82u;
	cmd[at++] = (uint8_t)(sizeof(cert) >> 8);
	cmd[at++] = (uint8_t)sizeof(cert);
	memcpy(cmd + at, cert, sizeof(cert));
	at += sizeof(cert);

	(void)call(0x00u, 0xdbu, 0x3fu, 0xffu, cmd, at, -1);
	T_EQ("schreiben ohne PIN", g_sw, OSK_SW_SECURITY);

	verify_pin();
	(void)call(0x00u, 0xdbu, 0x3fu, 0xffu, cmd, at, -1);
	T_EQ("Zertifikat geschrieben", g_sw, OSK_SW_OK);
	T_EQ("liegt im Tresor", mock_store_count(), 1);

	/* Lesen: einmal fragt es am Display, danach kommt es aus dem RAM. */
	{
		static const uint8_t ask[] = {0x5c, 0x03, 0x5f, 0xc1, 0x05};

		reveals = mock_store_reveal_count();
		(void)call(0x00u, 0xcbu, 0x3fu, 0xffu, ask, sizeof(ask), 2048);
		T_EQ("Zertifikat gelesen", g_sw, OSK_SW_OK);
		T_EQ("einmal geoeffnet", mock_store_reveal_count(), reveals + 1u);
		T_TRUE("in 53 verpackt", osk_tlv_find(g_resp, g_resp_len, 0x53u, &val, &len));
		T_EQ("Laenge stimmt", len, sizeof(cert));
		T_TRUE("Inhalt stimmt", memcmp(val, cert, sizeof(cert)) == 0);

		(void)call(0x00u, 0xcbu, 0x3fu, 0xffu, ask, sizeof(ask), 2048);
		T_EQ("zweites Lesen ok", g_sw, OSK_SW_OK);
		T_EQ("nicht wieder geoeffnet", mock_store_reveal_count(), reveals + 1u);

		/* Nach dem Aus- und Einschalten der Karte wird wieder gefragt. */
		osk_applets_reset();
		select_piv();
		(void)call(0x00u, 0xcbu, 0x3fu, 0xffu, ask, sizeof(ask), 2048);
		T_EQ("nach dem Einschalten wieder gefragt", mock_store_reveal_count(),
		     reveals + 2u);
	}
}

/* Antwortverkettung: ein langes Zertifikat kommt in Haeppchen mit 61xx. */
static void test_response_chaining(void)
{
	static const uint8_t ask[] = {0x5c, 0x03, 0x5f, 0xc1, 0x05};
	uint8_t cert[900];
	uint8_t collected[1024];
	size_t total = 0;
	unsigned rounds = 0;

	mock_reset_all();
	osk_applets_reset();
	select_piv();
	for (size_t i = 0; i < sizeof(cert); i++)
		cert[i] = (uint8_t)(i ^ 0x5au);
	T_EQ("Zertifikat gelegt", mock_store_put(OSK_ITEM_CERT, "PIV-CERT-9A", cert, sizeof(cert)),
	     OSK_OK);

	/* Ohne Le-Feld gelten 256 Bytes - mehr kann eine kurze Antwort nicht. */
	(void)call(0x00u, 0xcbu, 0x3fu, 0xffu, ask, sizeof(ask), -1);
	T_EQ("erstes Stueck kuendigt mehr an", g_sw & 0xff00u, OSK_SW_MORE);
	T_EQ("erstes Stueck ist voll", g_resp_len, 256);
	memcpy(collected, g_resp, g_resp_len);
	total = g_resp_len;

	while ((g_sw & 0xff00u) == OSK_SW_MORE && rounds < 16u) {
		rounds++;
		(void)call(0x00u, 0xc0u, 0x00u, 0x00u, NULL, 0u, 256);
		T_TRUE("GET RESPONSE liefert", g_resp_len > 0u);
		memcpy(collected + total, g_resp, g_resp_len);
		total += g_resp_len;
	}
	T_EQ("am Ende 9000", g_sw, OSK_SW_OK);
	/* 904 Bytes: 256 im ersten Stueck, dann 256, 256 und 136. */
	T_EQ("drei Runden GET RESPONSE", rounds, 3);

	/* Das Ganze ist 53 82 0384 <900 Bytes>. */
	T_EQ("Gesamtlaenge", total, sizeof(cert) + 4u);
	T_EQ("Tag", collected[0], 0x53);
	T_TRUE("Inhalt vollstaendig", memcmp(collected + 4, cert, sizeof(cert)) == 0);

	/* GET RESPONSE ohne offene Antwort. */
	(void)call(0x00u, 0xc0u, 0x00u, 0x00u, NULL, 0u, 256);
	T_EQ("nichts abzuholen", g_sw, OSK_SW_COND_NOT_SATISFIED);

	/* Ein anderes Kommando zwischendurch laesst die Antwort verfallen. */
	(void)call(0x00u, 0xcbu, 0x3fu, 0xffu, ask, sizeof(ask), -1);
	T_EQ("wieder angekuendigt", g_sw & 0xff00u, OSK_SW_MORE);
	(void)call(0x00u, 0x20u, 0x00u, 0x80u, NULL, 0u, -1);
	(void)call(0x00u, 0xc0u, 0x00u, 0x00u, NULL, 0u, 256);
	T_EQ("verfallen", g_sw, OSK_SW_COND_NOT_SATISFIED);
}

/* Kommandoverkettung: ein langes PUT DATA in Stuecken. */
static void test_command_chaining(void)
{
	uint8_t cert[400];
	uint8_t head[16];
	size_t head_len = 0;
	const uint8_t *val = NULL;
	size_t len = 0;

	mock_reset_all();
	osk_applets_reset();
	select_piv();
	verify_pin();

	for (size_t i = 0; i < sizeof(cert); i++)
		cert[i] = (uint8_t)(i + 11u);

	head[head_len++] = 0x5cu;
	head[head_len++] = 0x03u;
	head[head_len++] = 0x5fu;
	head[head_len++] = 0xc1u;
	head[head_len++] = 0x05u;
	head[head_len++] = 0x53u;
	head[head_len++] = 0x82u;
	head[head_len++] = (uint8_t)(sizeof(cert) >> 8);
	head[head_len++] = (uint8_t)sizeof(cert);

	/* Erstes Stueck: Kopf und die erste Haelfte. */
	{
		uint8_t part[256];
		size_t at = 0;

		memcpy(part, head, head_len);
		at = head_len;
		memcpy(part + at, cert, 200u);
		at += 200u;
		(void)call(0x10u, 0xdbu, 0x3fu, 0xffu, part, at, -1);
		T_EQ("erstes Stueck angenommen", g_sw, OSK_SW_OK);
		T_EQ("noch nichts geschrieben", mock_store_count(), 0);
	}
	/* Letztes Stueck ohne das Verkettungsbit. */
	(void)call(0x00u, 0xdbu, 0x3fu, 0xffu, cert + 200u, sizeof(cert) - 200u, -1);
	T_EQ("letztes Stueck", g_sw, OSK_SW_OK);
	T_EQ("jetzt geschrieben", mock_store_count(), 1);

	/* Zurueckgelesen muss dasselbe herauskommen. */
	{
		static const uint8_t ask[] = {0x5c, 0x03, 0x5f, 0xc1, 0x05};

		(void)call(0x00u, 0xcbu, 0x3fu, 0xffu, ask, sizeof(ask), 2048);
		T_EQ("gelesen", g_sw, OSK_SW_OK);
		T_TRUE("verpackt", osk_tlv_find(g_resp, g_resp_len, 0x53u, &val, &len));
		T_EQ("Laenge", len, sizeof(cert));
		T_TRUE("Inhalt", memcmp(val, cert, sizeof(cert)) == 0);
	}

	/* Ein Stueck mit anderem Kommando dazwischen bricht die Kette ab. */
	(void)call(0x10u, 0xdbu, 0x3fu, 0xffu, head, head_len, -1);
	T_EQ("Kette begonnen", g_sw, OSK_SW_OK);
	(void)call(0x00u, 0xcbu, 0x3fu, 0xffu, head, 5u, 256);
	T_EQ("anderes Kommando bricht ab", g_sw, OSK_SW_COND_NOT_SATISFIED);
}


/* --- RSA ---------------------------------------------------------------- */

/* Die drei DigestInfo-Vorsaetze woertlich aus RFC 8017 Abschnitt 9.2,
 * Anmerkung 1 - gegen den RFC geprueft, nicht gegen die Tabelle im Code. */
#define RSA_DI_SHA256 "3031300d060960864801650304020105000420"
#define RSA_DI_SHA384 "3041300d060960864801650304020205000430"

/* Lange Antworten einsammeln: eine RSA-Unterschrift ist so lang wie der
 * Modulus, also laenger als eine kurze Antwort. Der Verteiler kuendigt den
 * Rest mit 61xx an (7816-4 Abschnitt 5.3.4). */
static uint8_t g_long[1024];
static size_t g_long_len;

static uint16_t rsa_collect(void)
{
	g_long_len = 0u;
	for (unsigned round = 0; round < 16u; round++) {
		if (g_resp_len > 0u && g_long_len + g_resp_len <= sizeof(g_long)) {
			memcpy(g_long + g_long_len, g_resp, g_resp_len);
			g_long_len += g_resp_len;
		}
		if ((g_sw & 0xff00u) != OSK_SW_MORE)
			break;
		(void)call(0x00u, 0xc0u, 0x00u, 0x00u, NULL, 0u, 256);
	}
	return g_sw;
}

static uint16_t rsa_generate(uint8_t slot, uint8_t alg)
{
	uint8_t tmpl[8];

	tmpl[0] = 0xacu;
	tmpl[1] = 0x03u;
	tmpl[2] = 0x80u;
	tmpl[3] = 0x01u;
	tmpl[4] = alg;
	(void)call(0x00u, 0x47u, 0x00u, slot, tmpl, 5u, 256);
	return rsa_collect();
}

/* Ein vollstaendiger EMSA-PKCS1-v1_5-Block: 00 01 FF..FF 00 || DigestInfo.
 * Bei PIV baut IHN DER RECHNER, nicht die Karte (SP 800-73-4 Teil 2
 * Abschnitt 3.2.4) - genau das macht diese Funktion nach. */
static size_t rsa_block(const char *prefix_hex, size_t hash_len, size_t mod_len, uint8_t *out,
			size_t cap, uint8_t *hash_out)
{
	uint8_t di[96];
	size_t di_len = t_unhex(prefix_hex, di, sizeof(di));
	size_t pad;

	if (di_len == 0u || mod_len > cap)
		return 0u;
	for (size_t i = 0; i < hash_len; i++) {
		di[di_len + i] = (uint8_t)(0x30u + i);
		if (hash_out != NULL)
			hash_out[i] = di[di_len + i];
	}
	di_len += hash_len;

	pad = mod_len - di_len - 3u;
	out[0] = 0x00u;
	out[1] = 0x01u;
	for (size_t i = 0; i < pad; i++)
		out[2u + i] = 0xffu;
	out[2u + pad] = 0x00u;
	memcpy(out + 3u + pad, di, di_len);
	return mod_len;
}

/* Packt einen Wert in die Vorlage 7C { <tag> <wert> }. */
static size_t rsa_auth_cmd(uint8_t tag, const uint8_t *val, size_t len, uint8_t *out, size_t cap)
{
	size_t at = 0;
	/* Inhalt der Vorlage: das leere 82 (zwei Byte) und das Tag mit
	 * seinem Laengenfeld (vier Byte), dann der Wert. */
	const size_t inner = 6u + len;

	if (cap < inner + 8u)
		return 0u;
	out[at++] = 0x7cu;
	if (inner < 0x80u) {
		out[at++] = (uint8_t)inner;
	} else {
		out[at++] = 0x82u;
		out[at++] = (uint8_t)(inner >> 8);
		out[at++] = (uint8_t)inner;
	}
	out[at++] = 0x82u; /* leeres 82: "antworte" */
	out[at++] = 0x00u;
	out[at++] = tag;
	out[at++] = 0x82u;
	out[at++] = (uint8_t)(len >> 8);
	out[at++] = (uint8_t)len;
	memcpy(out + at, val, len);
	return at + len;
}

static void test_rsa_keys(void)
{
	static uint8_t blk[512];
	static uint8_t cmd[600];
	uint8_t hash[64];
	const uint8_t *inner = NULL;
	const uint8_t *mod = NULL;
	const uint8_t *exp = NULL;
	const uint8_t *sig = NULL;
	size_t inner_len = 0;
	size_t mod_len = 0;
	size_t exp_len = 0;
	size_t sig_len = 0;
	size_t n;
	size_t got;

	mock_reset_all();
	osk_applets_reset();
	select_piv();

	if (!OSK_RSA_BUILT) {
		/* Die Fassung ohne RSA verhaelt sich wie ein Geraet mit einem
		 * SE051A2: RSA wird nicht angeboten und nicht angelegt.
		 * Ausfuehrlich geprueft wird das in test_rsa_missing_chip(),
		 * das in beiden Fassungen dasselbe verlangt. */
		verify_pin();
		T_EQ("ohne CONFIG_OSK_RSA kein RSA-Schluessel",
		     rsa_generate(OSK_PIV_SLOT_AUTH, OSK_PIV_ALG_RSA2048),
		     OSK_SW_FUNC_NOT_SUPPORTED);
		T_TRUE("und keiner im Chip",
		       !mock_se050_has_rsa_key(OSK_PIV_KEY_ID(OSK_PIV_SLOT_AUTH)));
		T_EQ("die Kurve geht weiter",
		     rsa_generate(OSK_PIV_SLOT_AUTH, OSK_PIV_ALG_P256), OSK_SW_OK);
		return;
	}

	/* Mit RSA-Chip steht die Kennung in der Algorithmenliste. */
	{
		const uint8_t *val = NULL;
		const uint8_t *algs = NULL;
		size_t len = 0;
		size_t algs_len = 0;
		bool found_rsa = false;

		T_TRUE("Vorlage 61", osk_tlv_find(g_resp, g_resp_len, 0x61u, &val, &len));
		T_TRUE("Algorithmenliste", osk_tlv_find(val, len, 0xacu, &algs, &algs_len));
		for (size_t i = 0; i + 2u < algs_len; i++) {
			if (algs[i] == 0x80u && algs[i + 1u] == 0x01u &&
			    algs[i + 2u] == OSK_PIV_ALG_RSA2048)
				found_rsa = true;
		}
		T_TRUE("RSA-2048 wird angeboten", found_rsa);
	}

	verify_pin();
	T_EQ("9A als RSA-2048", rsa_generate(OSK_PIV_SLOT_AUTH, OSK_PIV_ALG_RSA2048), OSK_SW_OK);
	T_TRUE("Schluessel liegt im Chip", mock_se050_has_rsa_key(OSK_PIV_KEY_ID(OSK_PIV_SLOT_AUTH)));
	T_EQ("mit 2048 Bit", mock_se050_rsa_bits(OSK_PIV_KEY_ID(OSK_PIV_SLOT_AUTH)), 2048u);

	T_TRUE("7F49", osk_tlv_find(g_long, g_long_len, 0x7f49u, &inner, &inner_len));
	T_TRUE("Modulus in 81", osk_tlv_find(inner, inner_len, 0x81u, &mod, &mod_len));
	T_TRUE("Exponent in 82", osk_tlv_find(inner, inner_len, 0x82u, &exp, &exp_len));
	T_EQ("256 Byte Modulus", mod_len, 256u);
	/* Der Befund von Strang A: drei Byte, nicht vier. */
	T_EQ("Exponent drei Byte", exp_len, 3u);

	/* UNTERSCHREIBEN. Der Rechner schickt den fertig aufgefuellten
	 * Block; die Karte packt ihn aus und laesst den Chip ueber den
	 * benannten Weg des Vertrags unterschreiben. */
	n = rsa_block(RSA_DI_SHA256, 32u, 256u, blk, sizeof(blk), hash);
	T_EQ("Block gebaut", n, 256u);
	got = rsa_auth_cmd(0x81u, blk, n, cmd, sizeof(cmd));
	T_TRUE("Kommando gebaut", got > 256u);
	(void)call(0x00u, 0x87u, OSK_PIV_ALG_RSA2048, OSK_PIV_SLOT_AUTH, cmd, got, 256);
	T_EQ("unterschrieben", rsa_collect(), OSK_SW_OK);
	T_TRUE("Vorlage 7C", osk_tlv_find(g_long, g_long_len, 0x7cu, &inner, &inner_len));
	T_TRUE("Unterschrift in 82", osk_tlv_find(inner, inner_len, 0x82u, &sig, &sig_len));
	T_EQ("so lang wie der Modulus", sig_len, 256u);

	/* DAS IST DIE EIGENTLICHE PRUEFUNG: die Auffuellung wurde GENANNT,
	 * und das Hashverfahren kam aus dem DigestInfo und nicht aus der
	 * Laenge. */
	T_EQ("Auffuellung PKCS#1 v1.5 genannt", mock_rsa_last_pad(), (int)OSK_RSA_PAD_PKCS1_V15);
	T_EQ("Hashverfahren SHA-256", mock_rsa_last_hash(), (int)OSK_HASH_SHA256);
	{
		size_t hl = 0;
		const uint8_t *hv = mock_rsa_last_hash_val(&hl);

		T_EQ("32 Byte Hash", hl, 32u);
		T_TRUE("und genau der aus dem Block", memcmp(hv, hash, 32u) == 0);
	}

	/* Dasselbe mit SHA-384 - ein anderes Verfahren muss auch als ein
	 * anderes ankommen. */
	verify_pin();
	n = rsa_block(RSA_DI_SHA384, 48u, 256u, blk, sizeof(blk), hash);
	got = rsa_auth_cmd(0x81u, blk, n, cmd, sizeof(cmd));
	(void)call(0x00u, 0x87u, OSK_PIV_ALG_RSA2048, OSK_PIV_SLOT_AUTH, cmd, got, 256);
	T_EQ("unterschrieben", rsa_collect(), OSK_SW_OK);
	T_EQ("Hashverfahren SHA-384", mock_rsa_last_hash(), (int)OSK_HASH_SHA384);
	{
		size_t hl = 0;

		(void)mock_rsa_last_hash_val(&hl);
		T_EQ("48 Byte Hash", hl, 48u);
	}

	/* Ein Block, der kein ordentlicher v1.5-Block ist, wird ABGELEHNT
	 * und nicht gerechnet. Eine rohe Schluesseloperation auf einem frei
	 * gewaehlten Wert waere ein Unterschriftenautomat - bei 9E, das
	 * keine PIN braucht, waere das besonders schlimm. */
	{
		const unsigned before = mock_rsa_sign_count();

		verify_pin();
		memset(blk, 0xa5u, 256u);
		got = rsa_auth_cmd(0x81u, blk, 256u, cmd, sizeof(cmd));
		(void)call(0x00u, 0x87u, OSK_PIV_ALG_RSA2048, OSK_PIV_SLOT_AUTH, cmd, got, 256);
		T_EQ("roher Wert abgelehnt", g_sw, OSK_SW_WRONG_DATA);
		T_EQ("und der Chip gar nicht gefragt", mock_rsa_sign_count(), before);

		/* Richtiger Block, aber mit SHA-1 - der Chip koennte es, der
		 * Vertrag nennt es nicht. */
		verify_pin();
		n = rsa_block("3021300906052b0e03021a05000414", 20u, 256u, blk, sizeof(blk),
			      NULL);
		got = rsa_auth_cmd(0x81u, blk, n, cmd, sizeof(cmd));
		(void)call(0x00u, 0x87u, OSK_PIV_ALG_RSA2048, OSK_PIV_SLOT_AUTH, cmd, got, 256);
		T_EQ("SHA-1 abgelehnt", g_sw, OSK_SW_WRONG_DATA);
		T_EQ("Chip immer noch nicht gefragt", mock_rsa_sign_count(), before);
	}

	/* Ohne PIN gar nichts. */
	osk_applets_reset();
	select_piv();
	n = rsa_block(RSA_DI_SHA256, 32u, 256u, blk, sizeof(blk), hash);
	got = rsa_auth_cmd(0x81u, blk, n, cmd, sizeof(cmd));
	(void)call(0x00u, 0x87u, OSK_PIV_ALG_RSA2048, OSK_PIV_SLOT_AUTH, cmd, got, 256);
	T_EQ("ohne PIN nicht", g_sw, OSK_SW_SECURITY);

	/* Nach dem Einschalten weiss die Anwendung nicht mehr, was im
	 * Steckplatz liegt - sie fragt den Chip und lernt es wieder. Ohne
	 * das waere RSA nach jedem Abziehen weg. */
	verify_pin();
	(void)call(0x00u, 0x87u, OSK_PIV_ALG_RSA2048, OSK_PIV_SLOT_AUTH, cmd, got, 256);
	T_EQ("nach dem Einschalten wiedergefunden", rsa_collect(), OSK_SW_OK);
	/* Und die falsche Kennung passt dann nicht mehr. */
	(void)call(0x00u, 0x87u, OSK_PIV_ALG_P256, OSK_PIV_SLOT_AUTH, cmd, got, 256);
	T_EQ("P-256 passt nicht zum RSA-Steckplatz", g_sw, OSK_SW_WRONG_P1P2);
}

/* ENTSCHLUESSELN - ohne das bleibt verschluesselte Post ungelesen.
 *
 * Und hier liegt die Verwechslung, auf die es ankommt: Tag 81 ist in 9A
 * ein aufgefuellter Unterschriftenblock und in 9D ein Chiffrat. Zwei
 * Groessen, gleich benannt, Verschiedenes gemessen. */
static void test_rsa_decrypt(void)
{
	static uint8_t crypt[512];
	static uint8_t cmd[600];
	const uint8_t *inner = NULL;
	const uint8_t *plain = NULL;
	size_t inner_len = 0;
	size_t plain_len = 0;
	size_t got;

	mock_reset_all();
	osk_applets_reset();
	select_piv();
	verify_pin();

	if (!OSK_RSA_BUILT) {
		T_EQ("ohne CONFIG_OSK_RSA kein RSA in 9D",
		     rsa_generate(OSK_PIV_SLOT_KEYMGMT, OSK_PIV_ALG_RSA2048),
		     OSK_SW_FUNC_NOT_SUPPORTED);
		/* Der Schluesselaustausch auf der Kurve geht weiter - das ist
		 * der Weg, auf dem 9D in dieser Fassung entschluesselt. */
		T_EQ("9D als P-256", rsa_generate(OSK_PIV_SLOT_KEYMGMT, OSK_PIV_ALG_P256),
		     OSK_SW_OK);
		return;
	}

	T_EQ("9D als RSA-2048", rsa_generate(OSK_PIV_SLOT_KEYMGMT, OSK_PIV_ALG_RSA2048),
	     OSK_SW_OK);

	for (size_t i = 0; i < sizeof(crypt); i++)
		crypt[i] = (uint8_t)(i * 7u + 1u);

	got = rsa_auth_cmd(0x81u, crypt, 256u, cmd, sizeof(cmd));
	(void)call(0x00u, 0x87u, OSK_PIV_ALG_RSA2048, OSK_PIV_SLOT_KEYMGMT, cmd, got, 256);
	T_EQ("entschluesselt", rsa_collect(), OSK_SW_OK);
	T_EQ("der Chip hat entschluesselt, nicht unterschrieben", mock_rsa_decrypt_count(), 1u);
	T_EQ("und nichts unterschrieben", mock_rsa_sign_count(), 0u);
	T_EQ("Auffuellung genannt", mock_rsa_last_pad(), (int)OSK_RSA_PAD_PKCS1_V15);
	T_TRUE("Vorlage 7C", osk_tlv_find(g_long, g_long_len, 0x7cu, &inner, &inner_len));
	T_TRUE("Klartext in 82", osk_tlv_find(inner, inner_len, 0x82u, &plain, &plain_len));
	T_TRUE("und er ist nicht leer", plain_len > 0u);

	/* Ein Chiffrat ist so lang wie der Modulus. Kuerzeres ist keines. */
	verify_pin();
	got = rsa_auth_cmd(0x81u, crypt, 255u, cmd, sizeof(cmd));
	(void)call(0x00u, 0x87u, OSK_PIV_ALG_RSA2048, OSK_PIV_SLOT_KEYMGMT, cmd, got, 256);
	T_EQ("zu kurz", g_sw, OSK_SW_WRONG_DATA);
	verify_pin();
	got = rsa_auth_cmd(0x81u, crypt, 257u, cmd, sizeof(cmd));
	(void)call(0x00u, 0x87u, OSK_PIV_ALG_RSA2048, OSK_PIV_SLOT_KEYMGMT, cmd, got, 256);
	T_EQ("zu lang", g_sw, OSK_SW_WRONG_DATA);

	/* Tag 85 ist der Schluesselaustausch auf einer Kurve. Bei RSA gibt
	 * es den nicht - und er wird nicht stillschweigend als etwas
	 * anderes gelesen. */
	verify_pin();
	got = rsa_auth_cmd(0x85u, crypt, 65u, cmd, sizeof(cmd));
	(void)call(0x00u, 0x87u, OSK_PIV_ALG_RSA2048, OSK_PIV_SLOT_KEYMGMT, cmd, got, 256);
	T_EQ("Schluesselaustausch gibt es bei RSA nicht", g_sw, OSK_SW_WRONG_DATA);

	/* In 9A ist dasselbe Tag 81 ein Unterschriftenblock - ein Chiffrat
	 * dort ist kein gueltiger Block und wird abgelehnt. */
	verify_pin();
	T_EQ("9A als RSA", rsa_generate(OSK_PIV_SLOT_AUTH, OSK_PIV_ALG_RSA2048), OSK_SW_OK);
	verify_pin();
	got = rsa_auth_cmd(0x81u, crypt, 256u, cmd, sizeof(cmd));
	(void)call(0x00u, 0x87u, OSK_PIV_ALG_RSA2048, OSK_PIV_SLOT_AUTH, cmd, got, 256);
	T_EQ("Chiffrat in 9A ist kein Block", g_sw, OSK_SW_WRONG_DATA);
	T_EQ("und in 9A wird nicht entschluesselt", mock_rsa_decrypt_count(), 1u);
}

/* Der SE051A2: kann RSA ueberhaupt nicht und antwortet auf derselben
 * Adresse wie der C2. Die Anwendung muss SAUBER OHNE RSA arbeiten. */
static void test_rsa_missing_chip(void)
{
	const uint8_t *val = NULL;
	const uint8_t *algs = NULL;
	size_t len = 0;
	size_t algs_len = 0;
	bool found_rsa = false;

	mock_reset_all();
	mock_se050_set_rsa_mask(0u);
	osk_applets_reset();
	select_piv();

	/* RSA wird dann gar nicht erst angeboten: ein Kartenprogramm, das
	 * sich auf die Liste verlaesst und danach ein "geht nicht" bekommt,
	 * bricht an einer Stelle ab, an der es nichts mehr sagen kann. */
	T_TRUE("Vorlage 61", osk_tlv_find(g_resp, g_resp_len, 0x61u, &val, &len));
	T_TRUE("Algorithmenliste", osk_tlv_find(val, len, 0xacu, &algs, &algs_len));
	for (size_t i = 0; i + 2u < algs_len; i++) {
		if (algs[i] == 0x80u && algs[i + 1u] == 0x01u &&
		    algs[i + 2u] == OSK_PIV_ALG_RSA2048)
			found_rsa = true;
	}
	T_TRUE("RSA steht NICHT in der Liste", !found_rsa);
	T_TRUE("P-256 steht weiter drin", osk_tlv_find(algs, algs_len, 0x80u, NULL, NULL));

	verify_pin();
	T_EQ("RSA-Schluessel entsteht nicht",
	     rsa_generate(OSK_PIV_SLOT_AUTH, OSK_PIV_ALG_RSA2048), OSK_SW_FUNC_NOT_SUPPORTED);
	T_TRUE("und liegt auch nicht im Chip",
	       !mock_se050_has_rsa_key(OSK_PIV_KEY_ID(OSK_PIV_SLOT_AUTH)));

	/* Die Kurven gehen weiter - das ist der Punkt. */
	T_EQ("P-256 geht weiter", rsa_generate(OSK_PIV_SLOT_AUTH, OSK_PIV_ALG_P256), OSK_SW_OK);
	{
		const uint8_t *inner = NULL;
		size_t inner_len = 0;

		T_TRUE("7F49", osk_tlv_find(g_long, g_long_len, 0x7f49u, &inner, &inner_len));
		T_TRUE("Punkt in 86", osk_tlv_find(inner, inner_len, 0x86u, NULL, NULL));
	}
	mock_reset_all();
}

int main(void)
{
	/* Ungepuffert, damit ein Absturz die Ausgabe nicht mitnimmt. */
	setvbuf(stdout, NULL, _IONBF, 0);
	mock_reset_all();
	T_EQ("Anwendungen bereit", osk_applets_init(), OSK_OK);

	test_select();
	test_pin();
	test_puk();
	test_keys_and_sign();
	test_pin_always();
	test_card_auth();
	test_keyagree();
	test_data_objects();
	test_response_chaining();
	test_command_chaining();
	test_rsa_keys();
	test_rsa_decrypt();
	test_rsa_missing_chip();
	return t_report("test_piv");
}
