/* Pruefungen der NFC-Weiche und des FIDO-Applets, mit einem gestellten
 * Kern.
 *
 * Warum gestellt: so laesst sich eine Antwort beliebiger Laenge erzwingen,
 * und damit die Kettenbildung der Antwort (61xx mit GET RESPONSE) wirklich
 * pruefen. Dass derselbe ECHTE Kern antwortet und dass er den Tastendruck
 * verlangt, prueft test_nfc.
 *
 * Gearbeitet wird mit echten ISO-DEP-Rahmen, nicht mit APDU-Aufrufen -
 * also auf demselben Weg, den ein Lesegeraet nimmt.
 *
 * Es ist keine Funkmessung dabei. Siehe docs/funk.md. */
#include "test_util.h"
#include "mocks.h"
#include "nfc_tag.h"
#include "nfc_ctap.h"
#include "nfc_ndef.h"
#include "nfc_apdu.h"
#include "osk_transport.h"

/* ---- der gestellte Kern ------------------------------------------------- */

static uint8_t g_req[2048];
static size_t g_req_len;
static unsigned g_calls;
static size_t g_reply_len = 4;

osk_result_t osk_ctap_init(uint32_t extensions);
osk_result_t osk_ctap_cred_count(size_t *count_out);

osk_result_t osk_ctap_init(uint32_t extensions)
{
	(void)extensions;
	return OSK_OK;
}

osk_result_t osk_ctap_cred_count(size_t *count_out)
{
	if (count_out)
		*count_out = 0;
	return OSK_OK;
}

osk_result_t osk_ctap_handle(const osk_request_t *req, osk_response_t *res)
{
	g_calls++;
	g_req_len = (req->len <= sizeof(g_req)) ? req->len : sizeof(g_req);
	memcpy(g_req, req->data, g_req_len);
	if (g_reply_len > res->cap)
		return OSK_ERR_INTERNAL;
	/* Erstes Byte ist der CTAP-Status, danach Nutzdaten. */
	for (size_t i = 0; i < g_reply_len; i++)
		res->data[i] = (uint8_t)(i & 0xffu);
	res->len = g_reply_len;
	return OSK_OK;
}

osk_result_t osk_ctap_handle_u2f(const osk_request_t *req, osk_response_t *res)
{
	static const uint8_t version[] = {'U', '2', 'F', '_', 'V', '2', 0x90, 0x00};

	g_calls++;
	g_req_len = (req->len <= sizeof(g_req)) ? req->len : sizeof(g_req);
	memcpy(g_req, req->data, g_req_len);
	if (res->cap < sizeof(version))
		return OSK_ERR_INTERNAL;
	memcpy(res->data, version, sizeof(version));
	res->len = sizeof(version);
	return OSK_OK;
}

/* ---- Rahmen hinein und heraus ------------------------------------------- */

#define MAX_OUT 8
static uint8_t g_out[MAX_OUT][OSK_ISODEP_FRAME_MAX];
static size_t g_out_len[MAX_OUT];
static size_t g_out_count;
static uint8_t g_bn;

static void sink(const uint8_t *frame, size_t len, void *ctx)
{
	(void)ctx;
	if (g_out_count < MAX_OUT && len <= OSK_ISODEP_FRAME_MAX) {
		memcpy(g_out[g_out_count], frame, len);
		g_out_len[g_out_count] = len;
	}
	g_out_count++;
}

static void clear(void)
{
	g_out_count = 0;
	g_calls = 0;
	g_req_len = 0;
	memset(g_out_len, 0, sizeof(g_out_len));
}

static void start(void)
{
	mock_reset_all();
	mock_clock_set(1000);
	mock_ui_set_confirm(OSK_OK);
	g_reply_len = 4;
	g_bn = 0;
	T_EQ("Start", osk_nfc_tag_init(sink, NULL), OSK_OK);
	osk_nfc_tag_field_on();
	clear();
}

/* Der erste I-Block in dem, was hinausging. Davor kann eine Bitte um mehr
 * Zeit stehen (S(WTX)) - die zaehlt nicht als Antwort. */
static int i_block_index(void)
{
	const size_t n = (g_out_count < MAX_OUT) ? g_out_count : MAX_OUT;

	for (size_t i = 0; i < n; i++) {
		if (g_out_len[i] >= 1 && (g_out[i][0] & 0xc0u) == 0x00u)
			return (int)i;
	}
	return -1;
}

/* Wahr, wenn eine Bitte um mehr Zeit mit hinausging. */
static bool saw_wtx(void)
{
	const size_t n = (g_out_count < MAX_OUT) ? g_out_count : MAX_OUT;

	for (size_t i = 0; i < n; i++) {
		if (g_out_len[i] >= 1 && g_out[i][0] == OSK_ISODEP_S_WTX)
			return true;
	}
	return false;
}

/* Schickt eine APDU als I-Block und setzt die Antwort aus der Kette wieder
 * zusammen - genau das, was ein Lesegeraet tut. */
static size_t exchange(const uint8_t *apdu, size_t len, uint8_t *out, size_t cap)
{
	uint8_t frame[OSK_ISODEP_FRAME_MAX];
	size_t at = 0;

	clear();
	frame[0] = (uint8_t)(OSK_ISODEP_I_BLOCK | g_bn);
	memcpy(frame + 1, apdu, len);
	osk_nfc_tag_rx_frame(frame, 1 + len);
	g_bn ^= 1u;

	for (;;) {
		const int idx = i_block_index();

		if (idx < 0)
			return at;
		const uint8_t pcb = g_out[idx][0];
		const size_t n = g_out_len[idx] - 1u;

		if (at + n <= cap)
			memcpy(out + at, g_out[idx] + 1, n);
		at += n;
		if ((pcb & OSK_ISODEP_CHAINING) == 0)
			break;

		g_out_count = 0;
		memset(g_out_len, 0, sizeof(g_out_len));
		const uint8_t ack = (uint8_t)(OSK_ISODEP_R_ACK | g_bn);

		osk_nfc_tag_rx_frame(&ack, 1);
		g_bn ^= 1u;
	}
	return at;
}

static uint16_t sw_of(const uint8_t *out, size_t len)
{
	if (len < 2)
		return 0;
	return (uint16_t)(((uint16_t)out[len - 2] << 8) | out[len - 1]);
}

static size_t select_fido(uint8_t *out, size_t cap)
{
	const uint8_t aid[] = OSK_NFC_FIDO_AID;
	uint8_t apdu[16];
	size_t at = 0;

	apdu[at++] = 0x00;
	apdu[at++] = 0xa4;
	apdu[at++] = 0x04;
	apdu[at++] = 0x00;
	apdu[at++] = (uint8_t)sizeof(aid);
	memcpy(apdu + at, aid, sizeof(aid));
	at += sizeof(aid);
	apdu[at++] = 0x00;
	return exchange(apdu, at, out, cap);
}

static size_t select_ndef(uint8_t *out, size_t cap)
{
	const uint8_t aid[] = OSK_NFC_NDEF_AID;
	uint8_t apdu[16];
	size_t at = 0;

	apdu[at++] = 0x00;
	apdu[at++] = 0xa4;
	apdu[at++] = 0x04;
	apdu[at++] = 0x00;
	apdu[at++] = (uint8_t)sizeof(aid);
	memcpy(apdu + at, aid, sizeof(aid));
	at += sizeof(aid);
	return exchange(apdu, at, out, cap);
}

/* ---- Faelle ------------------------------------------------------------- */

static void case_select(void)
{
	uint8_t out[2048];

	start();
	T_EQ("ohne Auswahl keine Datei", g_out_count, 0);

	/* Vor der Auswahl beantwortet die Weiche nichts Fachliches. */
	const uint8_t msg[] = {0x80, 0x10, 0x00, 0x00, 0x01, 0x04, 0x00};
	size_t n = exchange(msg, sizeof(msg), out, sizeof(out));

	T_EQ("vor der Auswahl", sw_of(out, n), OSK_SW_COND_NOT_SATISFIED);
	T_EQ("kein Aufruf des Kerns", g_calls, 0);

	n = select_fido(out, sizeof(out));
	T_EQ("Auswahl beantwortet", sw_of(out, n), OSK_SW_OK);
	T_EQ("acht Byte", n, 8);
	T_TRUE("Fassung U2F_V2", memcmp(out, "U2F_V2", 6) == 0);
	T_EQ("FIDO ist gewaehlt", osk_nfc_tag_selected(), OSK_NFC_APP_FIDO);

	/* Eine fremde Kennung waehlt nichts ab. */
	const uint8_t other[] = {0x00, 0xa4, 0x04, 0x00, 0x03, 0xaa, 0xbb, 0xcc};

	n = exchange(other, sizeof(other), out, sizeof(out));
	T_EQ("fremde Kennung", sw_of(out, n), OSK_SW_FILE_NOT_FOUND);
	T_EQ("FIDO bleibt gewaehlt", osk_nfc_tag_selected(), OSK_NFC_APP_FIDO);

	/* Auswahl mit falschem P2. */
	const uint8_t bad_p2[] = {0x00, 0xa4, 0x04, 0x02, 0x01, 0xaa};

	n = exchange(bad_p2, sizeof(bad_p2), out, sizeof(out));
	T_EQ("falsches P2", sw_of(out, n), OSK_SW_WRONG_P1P2);

	/* Auswahl ohne Kennung. */
	const uint8_t no_aid[] = {0x00, 0xa4, 0x04, 0x00, 0x00};

	n = exchange(no_aid, sizeof(no_aid), out, sizeof(out));
	T_EQ("keine Kennung", sw_of(out, n), OSK_SW_WRONG_LENGTH);
}

static void case_msg(void)
{
	uint8_t out[2048];

	start();
	select_fido(out, sizeof(out));

	/* Kurze APDU, kurze Antwort. */
	g_reply_len = 10;
	const uint8_t msg[] = {0x80, 0x10, 0x00, 0x00, 0x02, 0x04, 0x05, 0x00};
	size_t n = exchange(msg, sizeof(msg), out, sizeof(out));

	T_EQ("bearbeitet", g_calls, 1);
	T_EQ("Befehl und CBOR angekommen", g_req_len, 2);
	T_EQ("Befehlsbyte", g_req[0], 0x04);
	T_EQ("Antwort und Statuswort", n, 12);
	T_EQ("in Ordnung", sw_of(out, n), OSK_SW_OK);
	T_EQ("erstes Byte der Antwort", out[0], 0x00);
	/* Vor der Arbeit des Kerns wird mehr Zeit erbeten: der Nutzer muss
	 * druecken, und ohne die Bitte bricht das Lesegeraet ab. */
	T_TRUE("um mehr Zeit gebeten", saw_wtx());

	/* P1 mit fremden Bits und P2 ungleich 0. */
	const uint8_t bad_p1[] = {0x80, 0x10, 0x40, 0x00, 0x01, 0x04, 0x00};

	n = exchange(bad_p1, sizeof(bad_p1), out, sizeof(out));
	T_EQ("fremdes Bit in P1", sw_of(out, n), OSK_SW_WRONG_P1P2);

	const uint8_t bad_p2[] = {0x80, 0x10, 0x00, 0x01, 0x01, 0x04, 0x00};

	n = exchange(bad_p2, sizeof(bad_p2), out, sizeof(out));
	T_EQ("P2 belegt", sw_of(out, n), OSK_SW_WRONG_P1P2);

	/* Das Bit fuer NFCCTAP_GETRESPONSE ist erlaubt. */
	const uint8_t with_bit[] = {0x80, 0x10, 0x80, 0x00, 0x01, 0x04, 0x00};

	n = exchange(with_bit, sizeof(with_bit), out, sizeof(out));
	T_EQ("Bit fuer GETRESPONSE erlaubt", sw_of(out, n), OSK_SW_OK);

	/* Eine Nachricht ohne Inhalt ist keine. */
	const uint8_t empty[] = {0x80, 0x10, 0x00, 0x00, 0x00};

	n = exchange(empty, sizeof(empty), out, sizeof(out));
	T_EQ("leere Nachricht", sw_of(out, n), OSK_SW_WRONG_LENGTH);

	/* Unbekannter Befehl und unbekanntes Klassenbyte. */
	const uint8_t bad_ins[] = {0x80, 0x77, 0x00, 0x00, 0x00};

	n = exchange(bad_ins, sizeof(bad_ins), out, sizeof(out));
	T_EQ("unbekannter Befehl", sw_of(out, n), OSK_SW_INS_NOT_SUPPORTED);

	const uint8_t bad_cla[] = {0x40, 0x10, 0x00, 0x00, 0x00};

	n = exchange(bad_cla, sizeof(bad_cla), out, sizeof(out));
	T_EQ("unbekanntes Klassenbyte", sw_of(out, n), OSK_SW_CLA_NOT_SUPPORTED);
}

static void case_response_chaining(void)
{
	uint8_t out[2048];
	uint8_t all[2048];
	size_t at = 0;

	start();
	select_fido(out, sizeof(out));

	/* 700 Byte Antwort bei kurzen Laengen: 256 + 256 + 188. */
	g_reply_len = 700;
	const uint8_t msg[] = {0x80, 0x10, 0x80, 0x00, 0x01, 0x04, 0x00};
	size_t n = exchange(msg, sizeof(msg), out, sizeof(out));

	T_EQ("erstes Stueck", n, 258);
	T_EQ("Statuswort 61xx", sw_of(out, n) & 0xff00u, 0x6100u);
	T_EQ("noch mehr als 255", sw_of(out, n) & 0x00ffu, 0x0000u);
	memcpy(all + at, out, n - 2);
	at += n - 2;

	const uint8_t get[] = {0x00, 0xc0, 0x00, 0x00, 0x00};

	n = exchange(get, sizeof(get), out, sizeof(out));
	T_EQ("zweites Stueck", n, 258);
	T_EQ("wieder 61xx", sw_of(out, n) & 0xff00u, 0x6100u);
	T_EQ("188 bleiben", sw_of(out, n) & 0x00ffu, 188);
	memcpy(all + at, out, n - 2);
	at += n - 2;

	n = exchange(get, sizeof(get), out, sizeof(out));
	T_EQ("letztes Stueck", n, 190);
	T_EQ("fertig", sw_of(out, n), OSK_SW_OK);
	memcpy(all + at, out, n - 2);
	at += n - 2;

	T_EQ("alles zusammen", at, 700);
	T_EQ("nur ein Aufruf des Kerns fuer alles", g_calls, 0);
	for (size_t i = 0; i < 700; i++)
		T_EQ("Reihenfolge stimmt", all[i], (uint8_t)(i & 0xffu));

	/* Jetzt liegt nichts mehr bereit. */
	n = exchange(get, sizeof(get), out, sizeof(out));
	T_EQ("nichts mehr abzuholen", sw_of(out, n), OSK_SW_COND_NOT_SATISFIED);

	/* Mit erweiterten Laengen kommt alles in einem Zug. */
	const uint8_t ext[] = {0x80, 0x10, 0x00, 0x00, 0x00, 0x00, 0x01, 0x04, 0x00, 0x00};

	n = exchange(ext, sizeof(ext), out, sizeof(out));
	T_EQ("erweiterte Antwort in einem Zug", n, 702);
	T_EQ("fertig", sw_of(out, n), OSK_SW_OK);

	/* Ein kurzes Le begrenzt das Stueck. */
	g_reply_len = 300;
	const uint8_t small_le[] = {0x80, 0x10, 0x00, 0x00, 0x01, 0x04, 0x40};

	n = exchange(small_le, sizeof(small_le), out, sizeof(out));
	T_EQ("64 Byte und Statuswort", n, 66);
	T_EQ("es geht weiter", sw_of(out, n) & 0xff00u, 0x6100u);
	T_EQ("236 bleiben", sw_of(out, n) & 0x00ffu, 236);

	/* Eine neue Anfrage verwirft die liegengebliebene Antwort. */
	g_reply_len = 4;
	const uint8_t again[] = {0x80, 0x10, 0x00, 0x00, 0x01, 0x04, 0x00};

	n = exchange(again, sizeof(again), out, sizeof(out));
	T_EQ("neue Antwort", n, 6);
	T_EQ("fertig", sw_of(out, n), OSK_SW_OK);
	n = exchange(get, sizeof(get), out, sizeof(out));
	T_EQ("die alte ist weg", sw_of(out, n), OSK_SW_COND_NOT_SATISFIED);
}

static void case_getresponse_ctap(void)
{
	uint8_t out[2048];

	start();
	select_fido(out, sizeof(out));
	g_reply_len = 400;

	const uint8_t msg[] = {0x80, 0x10, 0x80, 0x00, 0x01, 0x04, 0x00};
	size_t n = exchange(msg, sizeof(msg), out, sizeof(out));

	T_EQ("erstes Stueck", n, 258);

	/* Die CTAP-eigene Form des Abholens. */
	const uint8_t ctap_get[] = {0x80, 0x11, 0x00, 0x00, 0x00};

	n = exchange(ctap_get, sizeof(ctap_get), out, sizeof(out));
	T_EQ("Rest", n, 146);
	T_EQ("fertig", sw_of(out, n), OSK_SW_OK);

	/* Mit belegten Parametern nicht. */
	g_reply_len = 400;
	exchange(msg, sizeof(msg), out, sizeof(out));
	const uint8_t bad[] = {0x80, 0x11, 0x01, 0x00, 0x00};

	n = exchange(bad, sizeof(bad), out, sizeof(out));
	T_EQ("belegtes P1", sw_of(out, n), OSK_SW_WRONG_P1P2);

	/* Ohne etwas Bereitliegendes. */
	start();
	select_fido(out, sizeof(out));
	n = exchange(ctap_get, sizeof(ctap_get), out, sizeof(out));
	T_EQ("nichts bereit", sw_of(out, n), OSK_SW_COND_NOT_SATISFIED);
}

static void case_command_chaining(void)
{
	uint8_t out[2048];
	uint8_t apdu[260];

	start();
	select_fido(out, sizeof(out));
	g_reply_len = 4;

	/* Zwei Kettenglieder mit 0x90, dann das Schlussglied mit 0x80. */
	apdu[0] = 0x90;
	apdu[1] = 0x10;
	apdu[2] = 0x00;
	apdu[3] = 0x00;
	apdu[4] = 100;
	for (size_t i = 0; i < 100; i++)
		apdu[5 + i] = 0x11;
	size_t n = exchange(apdu, 105, out, sizeof(out));

	T_EQ("Glied bestaetigt", sw_of(out, n), OSK_SW_OK);
	T_EQ("noch nicht bearbeitet", g_calls, 0);

	for (size_t i = 0; i < 100; i++)
		apdu[5 + i] = 0x22;
	n = exchange(apdu, 105, out, sizeof(out));
	T_EQ("Glied bestaetigt", sw_of(out, n), OSK_SW_OK);
	T_EQ("noch nicht bearbeitet", g_calls, 0);

	apdu[0] = 0x80;
	apdu[4] = 50;
	for (size_t i = 0; i < 50; i++)
		apdu[5 + i] = 0x33;
	apdu[55] = 0x00;
	n = exchange(apdu, 56, out, sizeof(out));
	T_EQ("jetzt bearbeitet", g_calls, 1);
	T_EQ("alles zusammengesetzt", g_req_len, 250);
	T_EQ("erstes Glied", g_req[0], 0x11);
	T_EQ("zweites Glied", g_req[100], 0x22);
	T_EQ("drittes Glied", g_req[200], 0x33);
	T_EQ("Antwort", sw_of(out, n), OSK_SW_OK);

	/* Eine Kette, die den Puffer sprengt, wird abgewiesen - und raeumt
	 * dabei auf. */
	start();
	select_fido(out, sizeof(out));
	apdu[0] = 0x90;
	apdu[4] = 250;
	for (size_t i = 0; i < 250; i++)
		apdu[5 + i] = 0x44;
	unsigned accepted = 0;

	for (unsigned i = 0; i < 20; i++) {
		n = exchange(apdu, 255, out, sizeof(out));
		if (sw_of(out, n) != OSK_SW_OK)
			break;
		accepted++;
	}
	T_TRUE("die Kette wurde abgebrochen", accepted < 20);
	T_EQ("nichts bearbeitet", g_calls, 0);
	T_EQ("Laengenfehler", sw_of(out, n), OSK_SW_WRONG_LENGTH);

	/* Danach geht ein neuer Befehl sauber durch. */
	const uint8_t msg[] = {0x80, 0x10, 0x00, 0x00, 0x01, 0x04, 0x00};

	n = exchange(msg, sizeof(msg), out, sizeof(out));
	T_EQ("neu bearbeitet", g_calls, 1);
	T_EQ("ohne Reste", g_req_len, 1);
}

static void case_u2f(void)
{
	uint8_t out[2048];

	start();
	select_fido(out, sizeof(out));

	/* U2F_VERSION: die Antwort traegt ihr Statuswort selbst. */
	const uint8_t version[] = {0x00, 0x03, 0x00, 0x00, 0x00};
	size_t n = exchange(version, sizeof(version), out, sizeof(out));

	T_EQ("bearbeitet", g_calls, 1);
	T_EQ("rohe APDU durchgereicht", g_req_len, sizeof(version));
	T_EQ("acht Byte", n, 8);
	T_TRUE("U2F_V2", memcmp(out, "U2F_V2", 6) == 0);
	T_EQ("in Ordnung", sw_of(out, n), OSK_SW_OK);

	/* Ein unbekannter Befehl mit Klassenbyte 0 geht NICHT an U2F. */
	const uint8_t weird[] = {0x00, 0x44, 0x00, 0x00, 0x00};

	n = exchange(weird, sizeof(weird), out, sizeof(out));
	T_EQ("unbekannter Befehl", sw_of(out, n), OSK_SW_INS_NOT_SUPPORTED);
}

static void case_deselect(void)
{
	uint8_t out[2048];

	start();
	select_fido(out, sizeof(out));

	const uint8_t control[] = {0x80, 0x12, 0x01, 0x00};
	size_t n = exchange(control, sizeof(control), out, sizeof(out));

	T_EQ("abgewaehlt", sw_of(out, n), OSK_SW_OK);
	T_TRUE("Applet ist abgewaehlt", osk_nfc_ctap_deselected());

	/* Danach wird kein CTAP-Befehl mehr beantwortet. */
	const uint8_t msg[] = {0x80, 0x10, 0x00, 0x00, 0x01, 0x04, 0x00};

	n = exchange(msg, sizeof(msg), out, sizeof(out));
	T_EQ("nichts mehr", sw_of(out, n), OSK_SW_COND_NOT_SATISFIED);
	T_EQ("kein Aufruf", g_calls, 0);

	/* Falsche Parameter waehlen nicht ab. */
	start();
	select_fido(out, sizeof(out));
	const uint8_t bad[] = {0x80, 0x12, 0x00, 0x00};

	n = exchange(bad, sizeof(bad), out, sizeof(out));
	T_EQ("falsches P1", sw_of(out, n), OSK_SW_WRONG_P1P2);
	T_TRUE("nicht abgewaehlt", !osk_nfc_ctap_deselected());

	/* Eine neue Auswahl macht das Applet wieder ansprechbar. */
	exchange(control, sizeof(control), out, sizeof(out));
	T_TRUE("abgewaehlt", osk_nfc_ctap_deselected());
	select_fido(out, sizeof(out));
	T_TRUE("wieder da", !osk_nfc_ctap_deselected());
	n = exchange(msg, sizeof(msg), out, sizeof(out));
	T_EQ("wieder beantwortet", sw_of(out, n), OSK_SW_OK);
}

static void case_ndef_app(void)
{
	uint8_t out[2048];
	osk_ndef_config_t cfg;

	start();

	/* Abgeschaltet gibt es die Anwendung nicht. */
	size_t n = select_ndef(out, sizeof(out));

	T_EQ("abgeschaltet", sw_of(out, n), OSK_SW_FILE_NOT_FOUND);
	T_EQ("nichts gewaehlt", osk_nfc_tag_selected(), OSK_NFC_APP_NONE);

	/* Eingeschaltet schon. */
	memset(&cfg, 0, sizeof(cfg));
	cfg.mode = OSK_NDEF_URI;
	cfg.uri_prefix = OSK_NDEF_URI_HTTPS;
	memcpy(cfg.uri, "sorglos.org", 11);
	T_EQ("eingestellt", osk_ndef_configure(&cfg), OSK_OK);

	n = select_ndef(out, sizeof(out));
	T_EQ("Auswahl beantwortet", sw_of(out, n), OSK_SW_OK);
	T_EQ("NDEF ist gewaehlt", osk_nfc_tag_selected(), OSK_NFC_APP_NDEF);

	/* Und jetzt geht auch ein Lesezugriff ueber die Weiche. */
	const uint8_t sel_cc[] = {0x00, 0xa4, 0x00, 0x0c, 0x02, 0xe1, 0x03};

	n = exchange(sel_cc, sizeof(sel_cc), out, sizeof(out));
	T_EQ("Steuerdatei gewaehlt", sw_of(out, n), OSK_SW_OK);
	const uint8_t read_cc[] = {0x00, 0xb0, 0x00, 0x00, 0x0f};

	n = exchange(read_cc, sizeof(read_cc), out, sizeof(out));
	T_EQ("Steuerdatei gelesen", n, 17);
	T_EQ("Abbildungsfassung", out[2], 0x20);

	/* Zurueck zu FIDO, ohne Neustart. */
	n = select_fido(out, sizeof(out));
	T_EQ("FIDO wieder da", sw_of(out, n), OSK_SW_OK);
	T_EQ("FIDO gewaehlt", osk_nfc_tag_selected(), OSK_NFC_APP_FIDO);
}

static void case_switch_off(void)
{
	uint8_t out[2048];

	start();
	select_fido(out, sizeof(out));

	osk_nfc_tag_enable(false);
	T_TRUE("aus", !osk_nfc_tag_enabled());
	clear();
	const uint8_t msg[] = {0x80, 0x10, 0x00, 0x00, 0x01, 0x04, 0x00};
	uint8_t frame[OSK_ISODEP_FRAME_MAX];

	frame[0] = OSK_ISODEP_I_BLOCK;
	memcpy(frame + 1, msg, sizeof(msg));
	osk_nfc_tag_rx_frame(frame, 1 + sizeof(msg));
	T_EQ("kein Rahmen hinaus", g_out_count, 0);
	T_EQ("kein Aufruf", g_calls, 0);

	/* Wieder an: alles beginnt von vorn, ohne Auswahl. */
	osk_nfc_tag_enable(true);
	osk_nfc_tag_field_on();
	g_bn = 0;
	T_EQ("nichts gewaehlt", osk_nfc_tag_selected(), OSK_NFC_APP_NONE);
	size_t n = exchange(msg, sizeof(msg), out, sizeof(out));

	T_EQ("ohne Auswahl", sw_of(out, n), OSK_SW_COND_NOT_SATISFIED);

	/* Start ohne Senderoutine geht nicht. */
	T_EQ("ohne Senden kein Start", osk_nfc_tag_init(NULL, NULL), OSK_ERR_ARG);
}

static void case_field_resets(void)
{
	uint8_t out[2048];

	start();
	select_fido(out, sizeof(out));
	T_EQ("gewaehlt", osk_nfc_tag_selected(), OSK_NFC_APP_FIDO);

	/* Das Feld geht: die Auswahl ist weg. Ein Lesegeraet, das danach
	 * weiterredet, faengt mit der Auswahl neu an. */
	osk_nfc_tag_field_off();
	T_EQ("Auswahl weg", osk_nfc_tag_selected(), OSK_NFC_APP_NONE);
	osk_nfc_tag_field_on();
	g_bn = 0;
	const uint8_t msg[] = {0x80, 0x10, 0x00, 0x00, 0x01, 0x04, 0x00};
	size_t n = exchange(msg, sizeof(msg), out, sizeof(out));

	T_EQ("ohne Auswahl", sw_of(out, n), OSK_SW_COND_NOT_SATISFIED);

	/* Der Ablauf der halben Kette laeuft ueber die Weiche mit. */
	osk_nfc_tag_tick();
	T_EQ("nichts gewaehlt", osk_nfc_tag_selected(), OSK_NFC_APP_NONE);
}

int main(void)
{
	case_select();
	case_msg();
	case_response_chaining();
	case_getresponse_ctap();
	case_command_chaining();
	case_u2f();
	case_deselect();
	case_ndef_app();
	case_switch_off();
	case_field_resets();
	return t_report("test_nfc_chain");
}
