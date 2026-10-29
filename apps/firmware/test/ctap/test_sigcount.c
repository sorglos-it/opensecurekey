/* Der Signaturzaehler - UEBER EINEN NEUSTART HINWEG gemessen.
 *
 * Warum eigenwillig gebaut: eine Pruefung, die den Zaehler nur innerhalb
 * EINES Laufs ansieht, zeigt den Fehler nicht, um den es geht. Ein Zaehler
 * im RAM steigt innerhalb eines Laufs tadellos; er faellt erst beim naechsten
 * Einstecken auf null zurueck. Darum startet diese Pruefung sich SELBST ein
 * zweites Mal als neuen Prozess: damit ist der RAM wirklich weg und nur die
 * Datei der Attrappe (ihr "Flash") bleibt.
 *
 *   Lauf 1:  test_sigcount            - zaehlt hoch, merkt sich den Stand
 *   Lauf 2:  test_sigcount nachstart N - muss oberhalb von N weiterzaehlen
 *
 * Gemessen wird am PC. Am Geraet ist nichts davon gelaufen. */
#include <stdlib.h>
#include "ctap_helper.h"
#include "osk_sigcount.h"

static uint8_t g_body[2048];
static size_t g_len;
static uint32_t g_u2f_counter;

static uint16_t u2f_authenticate(const uint8_t *handle, size_t handle_len);

/* Die Datei liegt neben der Pruefung, nicht im Systemverzeichnis: ein
 * Prueflauf soll nichts hinterlassen, was ein anderer findet. */
#define COUNTER_FILE "osk-sigcount-test.bin"

static void start(void)
{
	mock_reset_all();
	T_EQ("Kern startet", osk_ctap_init(OSK_CTAP_EXT_HMAC_SECRET), OSK_OK);
}

/* Legt einen Zugang an und liefert seine Credential-ID. */
static size_t make_cred(const char *rp_id, uint8_t *cred_id_out, size_t cap)
{
	h_msg_t m;
	const uint8_t *ad = NULL;
	size_t ad_len = 0;

	h_make_credential(&m, rp_id, "anna", false, false, 0, NULL, 0, 0);
	T_EQ("MakeCredential ok", h_call(m.buf, m.len, g_body, sizeof(g_body), &g_len), CTAP2_OK);
	T_TRUE("authData da", h_get_bytes(g_body, g_len, 0x02, &ad, &ad_len));

	/* authData: 32 rpIdHash + 1 Flaggen + 4 Zaehler + 16 aaguid + 2 Laenge */
	T_TRUE("authData lang genug", ad_len > 55);
	const size_t id_len = ((size_t)ad[53] << 8) | ad[54];
	T_TRUE("Credential-ID passt", id_len > 0 && id_len <= cap && 55 + id_len <= ad_len);
	if (id_len == 0 || id_len > cap)
		return 0;
	memcpy(cred_id_out, ad + 55, id_len);
	return id_len;
}

/* Liest den Zaehler aus dem authData der letzten Antwort. */
static uint32_t counter_of_last(void)
{
	const uint8_t *ad = NULL;
	size_t ad_len = 0;

	if (!h_get_bytes(g_body, g_len, 0x02, &ad, &ad_len) || ad_len < 37)
		return 0;
	return ((uint32_t)ad[33] << 24) | ((uint32_t)ad[34] << 16) | ((uint32_t)ad[35] << 8) |
	       ad[36];
}

static uint32_t assert_once(const char *rp_id, const uint8_t *cred_id, size_t cred_id_len)
{
	h_msg_t m;

	build_get_assertion(&m, rp_id, cred_id, cred_id_len, NULL, 0, 0);
	T_EQ("GetAssertion ok", h_call(m.buf, m.len, g_body, sizeof(g_body), &g_len), CTAP2_OK);
	return counter_of_last();
}

/* ---- Lauf 1 ------------------------------------------------------------ */

static uint32_t test_erster_lauf(void)
{
	uint8_t cred_id[128];

	mock_sigcount_file(COUNTER_FILE);
	mock_sigcount_forget(); /* nur hier: wir wollen bei null anfangen */
	start();

	uint32_t stored = 0xffffffffu;
	T_EQ("leerer Zaehler ist lesbar", osk_sigcount_get(&stored), OSK_OK);
	T_EQ("und steht auf null", stored, 0);

	const size_t id_len = make_cred("example.com", cred_id, sizeof(cred_id));
	const uint32_t c_make = counter_of_last();
	T_TRUE("MakeCredential zaehlt mit", c_make >= 1);

	/* Der festgeschriebene Stand ist DERSELBE wie der in der Signatur,
	 * nicht einer dahinter. Das ist die Probe darauf, dass vor dem
	 * Signieren erhoeht und festgeschrieben wird: waere es danach, koennte
	 * eine herausgegebene Signatur eine Zahl tragen, die im Flash nicht
	 * steht - und nach einem Stromausfall waere sie zweimal zu haben. */
	T_EQ("Stand gelesen", osk_sigcount_get(&stored), OSK_OK);
	T_EQ("Signatur und Flash tragen dieselbe Zahl", stored, c_make);

	const uint32_t c1 = assert_once("example.com", cred_id, id_len);
	const uint32_t c2 = assert_once("example.com", cred_id, id_len);
	T_TRUE("Zaehler steigt je Anmeldung", c1 > c_make && c2 > c1);

	T_EQ("Stand gelesen", osk_sigcount_get(&stored), OSK_OK);
	T_EQ("Flash haelt den letzten Wert", stored, c2);

	/* Ein Einstecken und Abziehen OHNE Neustart des Prozesses: der Kern
	 * wird neu gestartet, der Zaehler darf dabei nicht fallen. Das allein
	 * wuerde nichts beweisen - deshalb kommt Lauf 2 dazu. */
	start();
	const uint32_t c3 = assert_once("example.com", cred_id, id_len);
	T_TRUE("nach osk_ctap_init kein Rueckfall", c3 > c2);

	return c3;
}

/* Eine Attrappe, die den Zaehler nicht festschreiben kann, darf NICHT zu
 * einer Signatur fuehren. Das ist die Gegenprobe zur Frage "was, wenn der
 * Flash klemmt" - lieber keine Anmeldung als eine mit einer Zahl, auf die
 * sich niemand verlassen kann. */
static void test_klemmender_flash(const uint8_t *cred_id, size_t cred_id_len)
{
	h_msg_t m;

	mock_sigcount_set_broken(true);

	build_get_assertion(&m, "example.com", cred_id, cred_id_len, NULL, 0, 0);
	T_EQ("GetAssertion verweigert", h_call(m.buf, m.len, g_body, sizeof(g_body), &g_len),
	     CTAP1_ERR_OTHER);

	h_make_credential(&m, "example.com", "anna", false, false, 0, NULL, 0, 0);
	T_EQ("MakeCredential verweigert", h_call(m.buf, m.len, g_body, sizeof(g_body), &g_len),
	     CTAP1_ERR_OTHER);

	/* U2F zaehlt aus derselben Quelle und muss genauso verweigern. Der
	 * alte Fehler sass dort GENAUSO: ein ++st->sign_count im RAM. */
	T_EQ("U2F verweigert", u2f_authenticate(cred_id, cred_id_len), 0x6985);

	mock_sigcount_set_broken(false);
}

/* ---- U2F teilt den Zaehler -------------------------------------------- */

/* Ein erweitertes APDU U2F_AUTHENTICATE mit "enforce user presence". Der
 * Griff IST die Credential-ID (siehe docs/ctap.md). Liefert das Statuswort. */
static uint16_t u2f_authenticate(const uint8_t *handle, size_t handle_len)
{
	uint8_t apdu[512];
	uint8_t data[32 + 32 + 1 + 128];
	uint8_t out[512];
	osk_request_t req;
	osk_response_t res;
	size_t at = 0;

	memset(data, 0xc1, 32); /* Herausforderung */
	at = 32;
	osk_sha256((const uint8_t *)"example.com", 11, data + at);
	at += 32;
	data[at++] = (uint8_t)handle_len;
	memcpy(data + at, handle, handle_len);
	at += handle_len;

	apdu[0] = 0x00;
	apdu[1] = 0x02; /* U2F_AUTHENTICATE */
	apdu[2] = 0x03; /* enforce user presence and sign */
	apdu[3] = 0x00;
	apdu[4] = 0x00;
	apdu[5] = (uint8_t)(at >> 8);
	apdu[6] = (uint8_t)at;
	memcpy(apdu + 7, data, at);

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));
	req.link = OSK_LINK_USB_HID;
	req.data = apdu;
	req.len = 7 + at;
	res.data = out;
	res.cap = sizeof(out);
	if (osk_ctap_handle_u2f(&req, &res) != OSK_OK || res.len < 2)
		return 0;
	/* Bei Erfolg: [Nutzeranwesenheit][4 Byte Zaehler][Signatur]. */
	if (res.len > 7)
		g_u2f_counter = ((uint32_t)out[1] << 24) | ((uint32_t)out[2] << 16) |
				((uint32_t)out[3] << 8) | out[4];
	return (uint16_t)(((uint16_t)out[res.len - 2] << 8) | out[res.len - 1]);
}

static void test_u2f_teilt_den_zaehler(void)
{
	uint8_t cred_id[128];
	uint32_t stored = 0;

	/* Derselbe Zaehler fuer beide Protokolle - nicht zwei, die gleich
	 * heissen. Ein eigener Zaehler je Protokoll waere fuer denselben
	 * Zugang (ein U2F-Griff IST eine Credential-ID) ein Rueckwaertslauf. */
	const size_t id_len = make_cred("example.com", cred_id, sizeof(cred_id));
	const uint32_t c_ctap = counter_of_last();
	T_EQ("U2F-Anmeldung ok", u2f_authenticate(cred_id, id_len), 0x9000);
	T_TRUE("U2F zaehlt oberhalb von CTAP2 weiter", g_u2f_counter > c_ctap);
	T_EQ("Stand lesbar", osk_sigcount_get(&stored), OSK_OK);
	T_EQ("und steht auf dem U2F-Wert", stored, g_u2f_counter);
}

/* ---- Lauf 2: neuer Prozess --------------------------------------------- */

static void test_nach_neustart(uint32_t vorher)
{
	uint8_t cred_id[128];

	mock_sigcount_file(COUNTER_FILE);
	start(); /* KEIN forget: der "Flash" bleibt, so wie echter Flash bleibt */

	uint32_t stored = 0;
	T_EQ("Stand lesbar", osk_sigcount_get(&stored), OSK_OK);
	T_TRUE("nichts verloren", stored >= vorher);

	const size_t id_len = make_cred("example.com", cred_id, sizeof(cred_id));
	const uint32_t c = counter_of_last();
	T_TRUE("im neuen Prozess wird oberhalb weitergezaehlt", c > vorher);

	const uint32_t c2 = assert_once("example.com", cred_id, id_len);
	T_TRUE("und weiter", c2 > c);
}

int main(int argc, char **argv)
{
	if (argc >= 3 && strcmp(argv[1], "nachstart") == 0) {
		test_nach_neustart((uint32_t)strtoul(argv[2], NULL, 10));
		return t_report("sigcount(nachstart)");
	}

	const uint32_t letzter = test_erster_lauf();

	test_u2f_teilt_den_zaehler();

	{
		uint8_t cred_id[128];
		/* Derselbe Dienst wie oben: u2f_authenticate rechnet mit
		 * dem Anwendungs-Hash von "example.com", und ein Griff zu
		 * einem anderen Dienst wuerde schon vorher abgewiesen - dann
		 * prueften wir nicht den klemmenden Flash, sondern den
		 * falschen Dienst. */
		const size_t id_len = make_cred("example.com", cred_id, sizeof(cred_id));
		test_klemmender_flash(cred_id, id_len);
	}

	/* Jetzt der echte Neustart. Der Ausstiegscode des zweiten Laufs
	 * entscheidet mit; faellt er rot aus, faellt diese Pruefung rot aus. */
	{
		char cmd[1024];
		snprintf(cmd, sizeof(cmd), "\"%s\" nachstart %lu", argv[0],
			 (unsigned long)letzter);
		printf("-- zweiter Lauf: %s\n", cmd);
		const int rc = system(cmd);
		T_EQ("zweiter Lauf gruen", rc, 0);
	}

	remove(COUNTER_FILE);
	return t_report("sigcount");
}
