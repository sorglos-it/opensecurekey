/* CTAP1/U2F: Rahmen, Registrierung, Anmeldung - und dass der
 * Schluesselgriff dieselbe Credential-ID ist wie bei CTAP2. */
#include "ctap_helper.h"
#include "osk_credid.h"

static uint8_t g_out[512];

static size_t u2f_call(const uint8_t *apdu, size_t len, uint16_t *sw_out)
{
	osk_request_t req;
	osk_response_t res;

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));
	req.link = OSK_LINK_USB_HID;
	req.data = apdu;
	req.len = len;
	res.data = g_out;
	res.cap = sizeof(g_out);

	if (osk_ctap_handle_u2f(&req, &res) != OSK_OK || res.len < 2) {
		*sw_out = 0;
		return 0;
	}
	*sw_out = (uint16_t)(((uint16_t)g_out[res.len - 2] << 8) | g_out[res.len - 1]);
	return res.len - 2;
}

/* Baut ein erweitertes APDU. */
static size_t build_apdu(uint8_t ins, uint8_t p1, const uint8_t *data, size_t len, uint8_t *out)
{
	out[0] = 0x00;
	out[1] = ins;
	out[2] = p1;
	out[3] = 0x00;
	out[4] = 0x00;
	out[5] = (uint8_t)(len >> 8);
	out[6] = (uint8_t)len;
	if (len)
		memcpy(out + 7, data, len);
	return 7 + len;
}

static void start(void)
{
	mock_reset_all();
	T_EQ("Kern startet", osk_ctap_init(0), OSK_OK);
}

static void test_version(void)
{
	uint8_t apdu[16];
	uint16_t sw = 0;

	start();
	const size_t n = build_apdu(0x03, 0x00, NULL, 0, apdu);
	const size_t len = u2f_call(apdu, n, &sw);
	T_EQ("SW ist 9000", (long long)sw, 0x9000);
	T_TRUE("Antwort ist U2F_V2", len == 6 && memcmp(g_out, "U2F_V2", 6) == 0);
}

static void test_register_und_authenticate(void)
{
	uint8_t apdu[256];
	uint8_t req[64];
	uint16_t sw = 0;
	uint8_t handle[OSK_MAX_CRED_ID];
	size_t handle_len = 0;
	uint8_t app[32];

	start();

	/* 32 Bytes Herausforderung, 32 Bytes Anwendungs-Hash. */
	memset(req, 0xc1, 32);
	osk_sha256((const uint8_t *)"https://example.com", 19, app);
	memcpy(req + 32, app, 32);

	size_t n = build_apdu(0x01, 0x00, req, sizeof(req), apdu);
	size_t len = u2f_call(apdu, n, &sw);
	T_EQ("Registrierung ok", (long long)sw, 0x9000);
	T_TRUE("gefragt wurde", mock_ui_confirm_count() == 1);
	T_TRUE("Antwort lang genug", len > 1 + 65 + 1);

	T_EQ("Reservebyte ist 0x05", g_out[0], 0x05);
	T_EQ("Punkt beginnt mit 0x04", g_out[1], 0x04);
	handle_len = g_out[66];
	T_EQ("Griff hat die Laenge unserer Credential-ID", (long long)handle_len, OSK_CREDID_LEN);
	memcpy(handle, g_out + 67, handle_len);

	/* Der Griff ist eine gueltige Credential-ID fuer genau diesen Dienst. */
	osk_credid_t cid;
	T_EQ("Griff ist eine Credential-ID", osk_credid_unpack(handle, handle_len, &cid), OSK_OK);
	T_TRUE("gehoert zum Dienst", memcmp(cid.rp_id_hash, app, 32) == 0);
	T_TRUE("nicht resident", !cid.resident);

	/* Jetzt anmelden. */
	uint8_t auth_req[200];
	memset(auth_req, 0xc2, 32);
	memcpy(auth_req + 32, app, 32);
	auth_req[64] = (uint8_t)handle_len;
	memcpy(auth_req + 65, handle, handle_len);
	const size_t auth_len = 65 + handle_len;

	n = build_apdu(0x02, 0x03, auth_req, auth_len, apdu);
	len = u2f_call(apdu, n, &sw);
	T_EQ("Anmeldung ok", (long long)sw, 0x9000);
	T_TRUE("Antwort lang genug", len > 5);
	T_EQ("Anwesenheitsbyte", g_out[0], 0x01);
	const uint32_t c1 = ((uint32_t)g_out[1] << 24) | ((uint32_t)g_out[2] << 16) |
			    ((uint32_t)g_out[3] << 8) | g_out[4];
	T_TRUE("Zaehler laeuft", c1 > 0);

	/* Noch einmal: der Zaehler steigt. */
	len = u2f_call(apdu, n, &sw);
	T_EQ("zweite Anmeldung", (long long)sw, 0x9000);
	const uint32_t c2 = ((uint32_t)g_out[1] << 24) | ((uint32_t)g_out[2] << 16) |
			    ((uint32_t)g_out[3] << 8) | g_out[4];
	T_TRUE("Zaehler gestiegen", c2 > c1);

	/* check-only: der Griff ist bekannt - die Antwort darauf ist
	 * ausgerechnet der Fehler 6985. */
	n = build_apdu(0x02, 0x07, auth_req, auth_len, apdu);
	(void)u2f_call(apdu, n, &sw);
	T_EQ("check-only bei bekanntem Griff", (long long)sw, 0x6985);

	/* Fremder Dienst mit demselben Griff: 6a80. */
	uint8_t other_app[32];
	osk_sha256((const uint8_t *)"https://boese.example", 21, other_app);
	memcpy(auth_req + 32, other_app, 32);
	n = build_apdu(0x02, 0x03, auth_req, auth_len, apdu);
	(void)u2f_call(apdu, n, &sw);
	T_EQ("fremder Dienst", (long long)sw, 0x6a80);

	/* Verfaelschter Griff: 6a80. */
	memcpy(auth_req + 32, app, 32);
	auth_req[70] ^= 0x01;
	n = build_apdu(0x02, 0x03, auth_req, auth_len, apdu);
	(void)u2f_call(apdu, n, &sw);
	T_EQ("verfaelschter Griff", (long long)sw, 0x6a80);
	auth_req[70] ^= 0x01;

	/* Nutzer verweigert: 6985 und nichts unterschrieben. */
	mock_ui_set_confirm(OSK_ERR_DENIED);
	n = build_apdu(0x02, 0x03, auth_req, auth_len, apdu);
	len = u2f_call(apdu, n, &sw);
	T_EQ("verweigert", (long long)sw, 0x6985);
	T_EQ("keine Nutzdaten", (long long)len, 0);
}

/* Ein mit U2F angelegter Griff muss auch ueber CTAP2 gehen. */
static void test_u2f_und_ctap2_zusammen(void)
{
	uint8_t apdu[256];
	uint8_t req[64];
	uint16_t sw = 0;
	uint8_t app[32];
	uint8_t handle[OSK_MAX_CRED_ID];
	size_t handle_len = 0;

	start();
	memset(req, 0xc1, 32);
	/* CTAP2 bildet rpIdHash aus "example.com"; U2F nimmt den
	 * Anwendungs-Hash. Damit beides zusammenpasst, nehmen wir hier
	 * denselben Wert: SHA-256("example.com"). */
	osk_sha256((const uint8_t *)"example.com", 11, app);
	memcpy(req + 32, app, 32);

	const size_t n = build_apdu(0x01, 0x00, req, sizeof(req), apdu);
	(void)u2f_call(apdu, n, &sw);
	T_EQ("Registrierung ok", (long long)sw, 0x9000);
	handle_len = g_out[66];
	memcpy(handle, g_out + 67, handle_len);

	/* Dieselbe ID in einer CTAP2-GetAssertion. */
	uint8_t msg[512];
	osk_cbor_writer_t w;
	uint8_t hash[32];
	osk_sha256((const uint8_t *)"clientData", 10, hash);
	msg[0] = OSK_CTAP_CMD_GET_ASSERTION;
	osk_cbor_writer_init(&w, msg + 1, sizeof(msg) - 1);
	osk_cbor_put_map(&w, 3);
	osk_cbor_put_uint(&w, 0x01);
	osk_cbor_put_text(&w, "example.com");
	osk_cbor_put_uint(&w, 0x02);
	osk_cbor_put_bytes(&w, hash, 32);
	osk_cbor_put_uint(&w, 0x03);
	osk_cbor_put_array(&w, 1);
	osk_cbor_put_map(&w, 2);
	osk_cbor_put_text(&w, "id");
	osk_cbor_put_bytes(&w, handle, handle_len);
	osk_cbor_put_text(&w, "type");
	osk_cbor_put_text(&w, "public-key");

	uint8_t body[1024];
	size_t body_len = 0;
	T_EQ("CTAP2 nimmt den U2F-Griff an", h_call(msg, 1 + w.len, body, sizeof(body), &body_len),
	     CTAP2_OK);
}

static void test_rahmenfehler(void)
{
	uint8_t apdu[256];
	uint16_t sw = 0;

	start();

	/* Falsche Befehlsklasse */
	apdu[0] = 0x80;
	apdu[1] = 0x03;
	apdu[2] = 0;
	apdu[3] = 0;
	(void)u2f_call(apdu, 4, &sw);
	T_EQ("CLA nicht 0", (long long)sw, 0x6e00);

	/* Unbekannter Befehl */
	const size_t n = build_apdu(0x42, 0x00, NULL, 0, apdu);
	(void)u2f_call(apdu, n, &sw);
	T_EQ("unbekanntes INS", (long long)sw, 0x6d00);

	/* Registrierung mit falscher Datenlaenge */
	uint8_t too_short[10];
	memset(too_short, 0, sizeof(too_short));
	const size_t n2 = build_apdu(0x01, 0x00, too_short, sizeof(too_short), apdu);
	(void)u2f_call(apdu, n2, &sw);
	T_EQ("Registrierung zu kurz", (long long)sw, 0x6700);

	/* Anmeldung, bei der die Griffslaenge nicht zur Nachricht passt */
	uint8_t bad[70];
	memset(bad, 0, sizeof(bad));
	bad[64] = 200; /* behauptet 200 Bytes Griff */
	const size_t n3 = build_apdu(0x02, 0x03, bad, sizeof(bad), apdu);
	(void)u2f_call(apdu, n3, &sw);
	T_EQ("Griffslaenge luegt", (long long)sw, 0x6700);

	/* APDU, das kuerzer ist als sein eigenes Laengenfeld verspricht */
	apdu[0] = 0x00;
	apdu[1] = 0x01;
	apdu[2] = 0;
	apdu[3] = 0;
	apdu[4] = 0x00;
	apdu[5] = 0x01;
	apdu[6] = 0x00; /* 256 Bytes angekuendigt */
	(void)u2f_call(apdu, 7, &sw);
	T_EQ("APDU zu kurz", (long long)sw, 0x6700);

	/* Zu kurzes APDU ueberhaupt */
	(void)u2f_call(apdu, 2, &sw);
	T_EQ("drei Bytes sind kein APDU", (long long)sw, 0x6700);
}

int main(void)
{
	test_version();
	test_register_und_authenticate();
	test_u2f_und_ctap2_zusammen();
	test_rahmenfehler();
	return t_report("u2f");
}
