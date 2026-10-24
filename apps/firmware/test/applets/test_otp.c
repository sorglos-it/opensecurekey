/* Yubico-OTP und Challenge-Response.
 *
 * Das Einmalkennwort wird nicht nur auf Form geprueft: die Pruefung baut
 * den Block selbst zusammen, verschluesselt ihn und vergleicht. Der Zufall
 * ist dabei vorhersagbar gemacht, damit beide Seiten dasselbe rechnen.
 *
 * Alles am PC. Gegen einen echten Pruefdienst von Yubico und gegen
 * KeePassXC oder VeraCrypt ist nichts davon gelaufen. */
#include "test_util.h"
#include "osk_otp.h"
#include "osk_otp_crypto.h"
#include "osk_card.h"
#include "osk_ui.h"
#include "osk_se050.h"
#include "mocks.h"

static const uint8_t g_public[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
static const uint8_t g_private[6] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
static const uint8_t g_aes[16] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
};

/* Baut den Block, den das Geraet bauen muss, und verschluesselt ihn. */
static void expected_otp(uint16_t use_ctr, uint8_t session_ctr, const uint8_t rnd[2], uint64_t now,
			 char *out, size_t cap)
{
	uint8_t token[16];
	uint8_t cipher[16];
	uint16_t crc;
	uint32_t tstp = (uint32_t)((now * 8u / 1000u) & 0xffffffu);

	memcpy(token, g_private, 6u);
	token[6] = (uint8_t)use_ctr;
	token[7] = (uint8_t)(use_ctr >> 8);
	token[8] = (uint8_t)tstp;
	token[9] = (uint8_t)(tstp >> 8);
	token[10] = (uint8_t)(tstp >> 16);
	token[11] = session_ctr;
	token[12] = rnd[0];
	token[13] = rnd[1];
	crc = (uint16_t)(osk_crc16(token, 14u) ^ OSK_CRC16_XOROUT);
	token[14] = (uint8_t)crc;
	token[15] = (uint8_t)(crc >> 8);

	/* Die Eigenschaft, auf die die Gegenseite baut: ueber den
	 * entschluesselten Block samt Pruefsumme bleibt der feste Rest. */
	T_EQ("Pruefsumme des Blocks", osk_crc16(token, 16u), OSK_CRC16_RESIDUE);

	osk_aes128_encrypt_block(g_aes, token, cipher);
	T_TRUE("Vorlage in ModHex", osk_modhex_encode(g_public, 6u, out, cap));
	T_TRUE("Vorlage weiter", osk_modhex_encode(cipher, 16u, out + 12u, cap - 12u));
}

static void test_yubico(void)
{
	char otp[OSK_OTP_STRING_LEN + 1u];
	char want[OSK_OTP_STRING_LEN + 1u];
	uint8_t rnd[2];
	uint8_t decoded[32];
	size_t len = 0;
	unsigned reveals;

	mock_reset_all();
	mock_clock_set(123456u);

	T_EQ("Steckplatz 1 belegt",
	     osk_otp_program(1u, OSK_OTP_MODE_YUBICO, g_public, g_private, g_aes, sizeof(g_aes)),
	     OSK_OK);
	T_EQ("liegt im Tresor", mock_store_count(), 1);

	/* Denselben Zufall zweimal: einmal fuer die Vorlage, einmal fuer das
	 * Geraet. */
	mock_se050_seed_random(4242u);
	T_EQ("Zufall geholt", osk_se050_random(rnd, sizeof(rnd)), OSK_OK);
	mock_se050_seed_random(4242u);

	reveals = mock_store_reveal_count();
	T_EQ("Kennwort erzeugt", osk_otp_generate(1u, otp, sizeof(otp)), OSK_OK);
	T_EQ("einmal am Display gefragt", mock_store_reveal_count(), reveals + 1u);
	T_TRUE("mit Namen gefragt", strstr(mock_ui_last_subject(), "Steckplatz 1") != NULL);

	T_EQ("44 Zeichen", strlen(otp), OSK_OTP_STRING_LEN);
	T_TRUE("alles ModHex",
	       osk_modhex_decode(otp, OSK_OTP_STRING_LEN, decoded, sizeof(decoded), &len));
	T_EQ("22 Bytes", len, 22);
	T_TRUE("oeffentliche Kennung vorn", memcmp(decoded, g_public, 6u) == 0);

	/* Der erste Lauf: Benutzungszaehler 1, Sitzungszaehler 1. */
	expected_otp(1u, 1u, rnd, 123456u, want, sizeof(want));
	T_TRUE("Kennwort stimmt Zeichen fuer Zeichen", strcmp(otp, want) == 0);
}

static void test_counters(void)
{
	char a[OSK_OTP_STRING_LEN + 1u];
	char b[OSK_OTP_STRING_LEN + 1u];
	uint8_t decoded[32];
	size_t len = 0;

	mock_reset_all();
	T_EQ("Steckplatz 1",
	     osk_otp_program(1u, OSK_OTP_MODE_YUBICO, g_public, g_private, g_aes, sizeof(g_aes)),
	     OSK_OK);

	T_EQ("erstes Kennwort", osk_otp_generate(1u, a, sizeof(a)), OSK_OK);
	T_EQ("zweites Kennwort", osk_otp_generate(1u, b, sizeof(b)), OSK_OK);
	T_TRUE("zwei verschiedene", strcmp(a, b) != 0);

	/* Der Zaehler liegt im Tresor, nicht im RAM: ein neuer Lauf nach
	 * einem Stromausfall faengt nicht wieder bei eins an. Das wird hier
	 * daran geprueft, dass der Eintrag selbst sich geaendert hat. */
	{
		uint8_t body[64];
		size_t body_len = 0;

		T_EQ("Eintrag lesbar",
		     osk_card_obj_read(OSK_ITEM_API_KEY, "OTP-1", "pruefung", body, sizeof(body),
				       &body_len),
		     OSK_OK);
		T_EQ("Benutzungszaehler steht auf eins", body[29], 1);
		T_EQ("Sitzungszaehler steht auf zwei", body[31], 2);
	}

	/* Auch das zweite Kennwort traegt die oeffentliche Kennung vorn. */
	T_TRUE("lesbar", osk_modhex_decode(b, OSK_OTP_STRING_LEN, decoded, sizeof(decoded), &len));
	T_EQ("22 Bytes", len, 22);
	T_TRUE("Kennung vorn", memcmp(decoded, g_public, 6u) == 0);
}

static void test_challenge(void)
{
	uint8_t key[20];
	uint8_t resp[OSK_OTP_RESPONSE_LEN];
	uint8_t want[OSK_OTP_RESPONSE_LEN];
	unsigned reveals;

	mock_reset_all();
	memset(key, 0x0b, sizeof(key));
	T_EQ("Steckplatz 2 belegt",
	     osk_otp_program(2u, OSK_OTP_MODE_HMAC, NULL, NULL, key, sizeof(key)), OSK_OK);

	reveals = mock_store_reveal_count();
	T_EQ("Frage beantwortet", osk_otp_challenge(2u, (const uint8_t *)"Hi There", 8u, resp),
	     OSK_OK);
	T_EQ("einmal am Display gefragt", mock_store_reveal_count(), reveals + 1u);

	/* Dasselbe, was RFC 2202 Fall 1 verlangt - und was KeePassXC
	 * erwartet, wenn es dieselbe Frage stellt. */
	osk_hmac_sha1(key, sizeof(key), (const uint8_t *)"Hi There", 8u, want);
	T_TRUE("Antwort stimmt", memcmp(resp, want, sizeof(resp)) == 0);
	T_HEX("Antwort gegen die Vorlage", resp, sizeof(resp),
	      "b617318655057264e28bc0b6fb378c8ef146be00");

	/* Dieselbe Frage, dieselbe Antwort - daran haengt das Entsperren. */
	{
		uint8_t again[OSK_OTP_RESPONSE_LEN];

		T_EQ("noch einmal", osk_otp_challenge(2u, (const uint8_t *)"Hi There", 8u, again),
		     OSK_OK);
		T_TRUE("gleich", memcmp(resp, again, sizeof(resp)) == 0);
	}

	/* Die laengste Frage, die wir annehmen, und eine zu lange. */
	{
		uint8_t big[80];

		memset(big, 0x42, sizeof(big));
		T_EQ("64 Bytes gehen", osk_otp_challenge(2u, big, 64u, resp), OSK_OK);
		T_EQ("65 Bytes nicht", osk_otp_challenge(2u, big, 65u, resp), OSK_ERR_ARG);
		T_EQ("leere Frage nicht", osk_otp_challenge(2u, big, 0u, resp), OSK_ERR_ARG);
	}
}

static void test_rejects(void)
{
	char otp[OSK_OTP_STRING_LEN + 1u];
	uint8_t resp[OSK_OTP_RESPONSE_LEN];
	uint8_t key[20];

	mock_reset_all();
	memset(key, 0x07, sizeof(key));

	/* Leere Steckplaetze. */
	T_EQ("nichts programmiert", osk_otp_generate(1u, otp, sizeof(otp)), OSK_ERR_NOT_FOUND);
	T_EQ("nichts zu fragen", osk_otp_challenge(1u, (const uint8_t *)"x", 1u, resp),
	     OSK_ERR_NOT_FOUND);

	/* Steckplatznummern ausserhalb. */
	T_EQ("Steckplatz 0", osk_otp_generate(0u, otp, sizeof(otp)), OSK_ERR_ARG);
	T_EQ("Steckplatz 3", osk_otp_generate(3u, otp, sizeof(otp)), OSK_ERR_ARG);
	T_EQ("Programmieren Steckplatz 9",
	     osk_otp_program(9u, OSK_OTP_MODE_HMAC, NULL, NULL, key, sizeof(key)), OSK_ERR_ARG);

	/* Falsche Laengen beim Programmieren. */
	T_EQ("HMAC braucht 20 Bytes", osk_otp_program(1u, OSK_OTP_MODE_HMAC, NULL, NULL, key, 16u),
	     OSK_ERR_ARG);
	T_EQ("Yubico braucht 16 Bytes",
	     osk_otp_program(1u, OSK_OTP_MODE_YUBICO, g_public, g_private, g_aes, 20u),
	     OSK_ERR_ARG);
	T_EQ("Yubico braucht Kennungen",
	     osk_otp_program(1u, OSK_OTP_MODE_YUBICO, NULL, NULL, g_aes, sizeof(g_aes)),
	     OSK_ERR_ARG);
	T_EQ("unbekannte Art", osk_otp_program(1u, 9u, NULL, NULL, key, sizeof(key)),
	     OSK_ERR_UNSUPPORTED);

	/* Die Arten lassen sich nicht vertauschen. */
	T_EQ("HMAC gelegt", osk_otp_program(1u, OSK_OTP_MODE_HMAC, NULL, NULL, key, sizeof(key)),
	     OSK_OK);
	T_EQ("kein Kennwort aus einem HMAC-Platz", osk_otp_generate(1u, otp, sizeof(otp)),
	     OSK_ERR_UNSUPPORTED);
	T_EQ("Yubico gelegt",
	     osk_otp_program(2u, OSK_OTP_MODE_YUBICO, g_public, g_private, g_aes, sizeof(g_aes)),
	     OSK_OK);
	T_EQ("keine Antwort aus einem Kennwort-Platz",
	     osk_otp_challenge(2u, (const uint8_t *)"x", 1u, resp), OSK_ERR_UNSUPPORTED);

	/* Zu kleiner Puffer. */
	T_EQ("Puffer zu klein", osk_otp_generate(2u, otp, 20u), OSK_ERR_ARG);

	/* Der Nutzer sagt nein: nichts kommt heraus. */
	mock_ui_set_confirm(OSK_ERR_DENIED);
	T_EQ("abgelehnt am Display", osk_otp_generate(2u, otp, sizeof(otp)), OSK_ERR_DENIED);
	T_EQ("Antwort auch abgelehnt", osk_otp_challenge(1u, (const uint8_t *)"x", 1u, resp),
	     OSK_ERR_DENIED);
	mock_ui_set_confirm(OSK_OK);
}

/* Der eigene Kanal: Rahmen hinein, Antwort heraus. */
static void test_channel(void)
{
	uint8_t out[64];
	uint8_t frame[80];
	osk_request_t req;
	osk_response_t res;
	uint8_t key[20];

	mock_reset_all();
	memset(key, 0x0b, sizeof(key));
	T_EQ("Kanal angemeldet", osk_otp_init(), OSK_OK);
	T_EQ("Steckplatz 1",
	     osk_otp_program(1u, OSK_OTP_MODE_YUBICO, g_public, g_private, g_aes, sizeof(g_aes)),
	     OSK_OK);
	T_EQ("Steckplatz 2", osk_otp_program(2u, OSK_OTP_MODE_HMAC, NULL, NULL, key, sizeof(key)),
	     OSK_OK);

	req.link = OSK_LINK_USB_VENDOR;
	req.data = frame;
	res.data = out;

	/* Kennwort anfordern. */
	frame[0] = 0x01u;
	frame[1] = 0x01u;
	req.len = 2u;
	res.cap = sizeof(out);
	res.len = 0u;
	T_EQ("Kennwort geholt", osk_otp_handle(&req, &res), OSK_OK);
	T_EQ("44 Zeichen", res.len, OSK_OTP_STRING_LEN);

	/* Frage stellen. */
	frame[0] = 0x02u;
	frame[1] = 0x02u;
	memcpy(frame + 2, "Hi There", 8u);
	req.len = 10u;
	res.len = 0u;
	T_EQ("Frage gestellt", osk_otp_handle(&req, &res), OSK_OK);
	T_EQ("20 Bytes", res.len, OSK_OTP_RESPONSE_LEN);
	T_HEX("Antwort", res.data, res.len, "b617318655057264e28bc0b6fb378c8ef146be00");

	/* Unsinn wird beantwortet, nicht verschluckt. */
	frame[0] = 0x09u;
	frame[1] = 0x01u;
	req.len = 2u;
	res.len = 0u;
	T_EQ("unbekanntes Kommando", osk_otp_handle(&req, &res), OSK_OK);
	T_EQ("Fehlerkennung", res.data[0], 0xff);
	T_EQ("Grund", res.data[1], (uint8_t)OSK_ERR_UNSUPPORTED);

	frame[0] = 0x01u;
	frame[1] = 0x07u;
	req.len = 2u;
	res.len = 0u;
	(void)osk_otp_handle(&req, &res);
	T_EQ("fremder Steckplatz", res.data[1], (uint8_t)OSK_ERR_ARG);

	req.len = 1u;
	res.len = 0u;
	(void)osk_otp_handle(&req, &res);
	T_EQ("halber Rahmen", res.data[1], (uint8_t)OSK_ERR_FORMAT);

	/* Kennwort anfordern mit Anhang: der Rahmen ist dann krumm. */
	frame[0] = 0x01u;
	frame[1] = 0x01u;
	req.len = 5u;
	res.len = 0u;
	(void)osk_otp_handle(&req, &res);
	T_EQ("zu langer Rahmen", res.data[1], (uint8_t)OSK_ERR_FORMAT);

	/* Frage ohne Frage. */
	frame[0] = 0x02u;
	frame[1] = 0x02u;
	req.len = 2u;
	res.len = 0u;
	(void)osk_otp_handle(&req, &res);
	T_EQ("leere Frage", res.data[1], (uint8_t)OSK_ERR_FORMAT);

	/* Zu kleiner Antwortpuffer. */
	frame[0] = 0x01u;
	frame[1] = 0x01u;
	req.len = 2u;
	res.cap = 8u;
	res.len = 0u;
	(void)osk_otp_handle(&req, &res);
	T_EQ("Puffer zu klein", res.data[1], (uint8_t)OSK_ERR_FULL);

	T_EQ("Nullzeiger", osk_otp_handle(NULL, &res), OSK_ERR_ARG);
}

int main(void)
{
	setvbuf(stdout, NULL, _IONBF, 0);
	test_yubico();
	test_counters();
	test_challenge();
	test_rejects();
	test_channel();
	return t_report("test_otp");
}
