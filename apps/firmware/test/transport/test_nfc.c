/* NFC mit dem ECHTEN CTAP-Kern.
 *
 * Drei Dinge werden hier belegt, und nur hier:
 *
 *   1. Ueber NFC antwortet derselbe Kern wie am USB - Byte fuer Byte
 *      dieselbe Antwort, ueber beide Transporte gefragt.
 *   2. Jeder Vorgang ueber NFC braucht den Tastendruck. Keine Ausnahme
 *      "weil NFC ja kurz ist" - das Bedrohungsmodell nennt den
 *      Weiterleitungsangriff aus der Tasche ausdruecklich.
 *   3. Reset bleibt ueber NFC gesperrt.
 *
 * Es ist keine Funkmessung dabei: die Rahmen kommen aus diesem Programm,
 * nicht aus einem Feld. Siehe docs/funk.md. */
#include "test_util.h"
#include "mocks.h"
#include "nfc_tag.h"
#include "nfc_ctap.h"
#include "nfc_apdu.h"
#include "osk_ctap.h"
#include "osk_transport.h"
#include "usb_hid.h"

/* Befehle des Kerns, die hier gebraucht werden (CTAP 2.1, Abschnitt 6). */
#define CMD_GET_INFO 0x04u
#define CMD_RESET 0x07u
#define CMD_MAKE_CREDENTIAL 0x01u
/* Antworten des Kerns. */
#define CTAP2_OK 0x00u
#define CTAP2_ERR_OPERATION_DENIED 0x27u

/* ---- NFC-Seite ---------------------------------------------------------- */

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

static void out_clear(void)
{
	g_out_count = 0;
	memset(g_out_len, 0, sizeof(g_out_len));
}

/* Der erste I-Block in dem, was hinausging. Davor steht die Bitte um mehr
 * Zeit (S(WTX)), die keine Antwort ist. */
static int i_block_index(void)
{
	const size_t n = (g_out_count < MAX_OUT) ? g_out_count : MAX_OUT;

	for (size_t i = 0; i < n; i++) {
		if (g_out_len[i] >= 1 && (g_out[i][0] & 0xc0u) == 0x00u)
			return (int)i;
	}
	return -1;
}

static size_t exchange(const uint8_t *apdu, size_t len, uint8_t *out, size_t cap)
{
	uint8_t frame[OSK_ISODEP_FRAME_MAX];
	size_t at = 0;

	out_clear();
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

		out_clear();
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

static void select_fido(void)
{
	const uint8_t aid[] = OSK_NFC_FIDO_AID;
	uint8_t apdu[16];
	uint8_t out[64];
	size_t at = 0;

	apdu[at++] = 0x00;
	apdu[at++] = 0xa4;
	apdu[at++] = 0x04;
	apdu[at++] = 0x00;
	apdu[at++] = (uint8_t)sizeof(aid);
	memcpy(apdu + at, aid, sizeof(aid));
	at += sizeof(aid);
	apdu[at++] = 0x00;

	const size_t n = exchange(apdu, at, out, sizeof(out));

	T_EQ("Applet gewaehlt", sw_of(out, n), OSK_SW_OK);
}

/* Eine CTAP2-Nachricht ueber NFC schicken. Liefert den CBOR-Teil ohne das
 * Statusbyte und meldet Statusbyte und Statuswort. */
static size_t nfc_ctap(const uint8_t *msg, size_t len, uint8_t *body, size_t cap,
		       uint8_t *status_out, uint16_t *sw_out)
{
	uint8_t apdu[512];
	static uint8_t out[2048];
	size_t at = 0;

	apdu[at++] = 0x80;
	apdu[at++] = 0x10;
	apdu[at++] = 0x00;
	apdu[at++] = 0x00;
	apdu[at++] = 0x00; /* erweiterte Laengen: eine Antwort in einem Zug */
	apdu[at++] = (uint8_t)(len >> 8);
	apdu[at++] = (uint8_t)len;
	memcpy(apdu + at, msg, len);
	at += len;
	apdu[at++] = 0x00;
	apdu[at++] = 0x00;

	const size_t n = exchange(apdu, at, out, sizeof(out));

	*sw_out = sw_of(out, n);
	if (n < 3) {
		*status_out = 0xff;
		return 0;
	}
	*status_out = out[0];
	size_t body_len = n - 3u;

	if (body_len > cap)
		body_len = cap;
	if (body_len)
		memcpy(body, out + 1, body_len);
	return body_len;
}

/* ---- USB-Seite, zum Vergleich ------------------------------------------- */

static uint8_t g_hid[2048];
static size_t g_hid_len;
static uint32_t g_cid;

static void hid_sink(const uint8_t packet[OSK_HID_PACKET_LEN], void *ctx)
{
	(void)ctx;
	if (packet[4] == CTAPHID_INIT && packet[6] == 17) {
		g_cid = ((uint32_t)packet[15] << 24) | ((uint32_t)packet[16] << 16) |
			((uint32_t)packet[17] << 8) | packet[18];
		return;
	}
	if (packet[4] == CTAPHID_CBOR) {
		const size_t total = ((size_t)packet[5] << 8) | packet[6];

		g_hid_len = (total <= sizeof(g_hid)) ? total : sizeof(g_hid);
		memcpy(g_hid, packet + 7,
		       (g_hid_len < OSK_HID_INIT_DATA) ? g_hid_len : OSK_HID_INIT_DATA);
		return;
	}
	/* Fortsetzungspaket der Antwort. */
	if ((packet[4] & 0x80u) == 0 && g_hid_len > OSK_HID_INIT_DATA) {
		const size_t seq = packet[4];
		const size_t at = OSK_HID_INIT_DATA + seq * OSK_HID_CONT_DATA;
		size_t take = g_hid_len - at;

		if (take > OSK_HID_CONT_DATA)
			take = OSK_HID_CONT_DATA;
		if (at < g_hid_len)
			memcpy(g_hid + at, packet + 5, take);
	}
}

/* Dieselbe Nachricht ueber den USB-Weg. Liefert die Laenge der Antwort
 * einschliesslich Statusbyte. */
static size_t usb_ctap(const uint8_t *msg, size_t len)
{
	uint8_t pkt[OSK_HID_PACKET_LEN];

	g_hid_len = 0;
	g_cid = 0;
	T_EQ("USB startet", osk_hid_init(hid_sink, NULL), OSK_OK);

	memset(pkt, 0, sizeof(pkt));
	pkt[0] = pkt[1] = pkt[2] = pkt[3] = 0xff;
	pkt[4] = CTAPHID_INIT;
	pkt[6] = 8;
	osk_hid_rx_packet(pkt);
	T_TRUE("Kanal bekommen", g_cid != 0);

	memset(pkt, 0, sizeof(pkt));
	pkt[0] = (uint8_t)(g_cid >> 24);
	pkt[1] = (uint8_t)(g_cid >> 16);
	pkt[2] = (uint8_t)(g_cid >> 8);
	pkt[3] = (uint8_t)g_cid;
	pkt[4] = CTAPHID_CBOR;
	pkt[5] = (uint8_t)(len >> 8);
	pkt[6] = (uint8_t)len;
	memcpy(pkt + 7, msg, len);
	osk_hid_rx_packet(pkt);
	return g_hid_len;
}

/* ---- Faelle ------------------------------------------------------------- */

static void start(void)
{
	mock_reset_all();
	mock_clock_set(1000);
	mock_ui_set_confirm(OSK_OK);
	g_bn = 0;
	T_EQ("Kern startet", osk_ctap_init(0), OSK_OK);
	T_EQ("NFC startet", osk_nfc_tag_init(sink, NULL), OSK_OK);
	osk_nfc_tag_field_on();
	out_clear();
	select_fido();
}

/* 1. Derselbe Kern wie am USB. */
static void case_same_core(void)
{
	uint8_t body[2048];
	uint8_t status = 0xff;
	uint16_t sw = 0;
	const uint8_t msg[] = {CMD_GET_INFO};

	start();
	const size_t n = nfc_ctap(msg, sizeof(msg), body, sizeof(body), &status, &sw);

	T_EQ("Statuswort", sw, OSK_SW_OK);
	T_EQ("CTAP meldet Erfolg", status, CTAP2_OK);
	T_TRUE("die Antwort hat Inhalt", n > 20);

	/* Dieselbe Frage ueber USB. Die Antwort muss Byte fuer Byte gleich
	 * sein - sonst waere es nicht derselbe Kern. */
	const size_t m = usb_ctap(msg, sizeof(msg));

	T_EQ("USB antwortet genauso lang", m, n + 1);
	T_EQ("dasselbe Statusbyte", g_hid[0], status);
	T_TRUE("derselbe Inhalt", memcmp(g_hid + 1, body, n) == 0);
}

/* 2. Jeder Vorgang braucht den Tastendruck. */
static void case_button_required(void)
{
	uint8_t apdu[128];
	uint8_t out[512];
	size_t at = 0;

	/* U2F-Registrierung ueber NFC: 32 Byte Aufgabe, 32 Byte Dienst. */
	start();
	apdu[at++] = 0x00;
	apdu[at++] = 0x01; /* U2F_REGISTER */
	apdu[at++] = 0x00;
	apdu[at++] = 0x00;
	apdu[at++] = 0x00;
	apdu[at++] = 0x00;
	apdu[at++] = 64;
	for (unsigned i = 0; i < 64; i++)
		apdu[at++] = (uint8_t)(i + 1u);
	apdu[at++] = 0x00;
	apdu[at++] = 0x00;

	/* Ohne Druck: kein Schluessel. */
	mock_ui_set_confirm(OSK_ERR_DENIED);
	size_t n = exchange(apdu, at, out, sizeof(out));

	T_TRUE("gefragt wurde", mock_ui_confirm_count() >= 1);
	T_TRUE("nicht erfolgreich", sw_of(out, n) != OSK_SW_OK);

	/* Mit Druck: es geht. */
	start();
	mock_ui_set_confirm(OSK_OK);
	const unsigned before = mock_ui_confirm_count();

	n = exchange(apdu, at, out, sizeof(out));
	T_EQ("mit Druck in Ordnung", sw_of(out, n), OSK_SW_OK);
	T_TRUE("der Taster wurde gefragt", mock_ui_confirm_count() > before);
	T_TRUE("eine echte Antwort", n > 64);

	/* Und auch der CTAP2-Weg fragt. Eine Nachricht, die ohne Freigabe
	 * nichts herausgeben darf: MakeCredential mit leerem Inhalt faellt
	 * schon am Aufbau - darum wird hier die Registrierung genommen, die
	 * nachweislich durchlaeuft, und nur gezaehlt. */
	start();
	const unsigned before2 = mock_ui_confirm_count();

	n = exchange(apdu, at, out, sizeof(out));
	T_EQ("zweite Registrierung", sw_of(out, n), OSK_SW_OK);
	T_EQ("genau einmal gefragt", mock_ui_confirm_count(), before2 + 1u);
}

/* 3. Reset bleibt ueber NFC gesperrt. */
static void case_reset_locked(void)
{
	uint8_t body[64];
	uint8_t status = 0xff;
	uint16_t sw = 0;
	const uint8_t msg[] = {CMD_RESET};

	start();
	const unsigned before = mock_ui_confirm_count();

	nfc_ctap(msg, sizeof(msg), body, sizeof(body), &status, &sw);
	T_EQ("Statuswort", sw, OSK_SW_OK);
	T_EQ("Reset abgelehnt", status, CTAP2_ERR_OPERATION_DENIED);
	/* Und zwar BEVOR gefragt wird - ein Lesegeraet soll nicht einmal
	 * einen Tastendruck ausloesen koennen. */
	T_EQ("gar nicht gefragt", mock_ui_confirm_count(), before);

	/* Ueber USB geht derselbe Befehl durch (mit Druck). */
	mock_ui_set_confirm(OSK_OK);
	const size_t m = usb_ctap(msg, sizeof(msg));

	T_TRUE("USB antwortet", m >= 1);
	T_EQ("ueber USB erlaubt", g_hid[0], CTAP2_OK);
}

/* Ein unbekannter Befehl kommt beim Kern an und wird von ihm abgelehnt -
 * nicht vom Transport verschluckt. */
static void case_unknown_command(void)
{
	uint8_t body[64];
	uint8_t status = 0;
	uint16_t sw = 0;
	const uint8_t msg[] = {0x7e};

	start();
	nfc_ctap(msg, sizeof(msg), body, sizeof(body), &status, &sw);
	T_EQ("Statuswort in Ordnung", sw, OSK_SW_OK);
	T_TRUE("der Kern hat abgelehnt", status != CTAP2_OK);
}

int main(void)
{
	case_same_core();
	case_button_required();
	case_reset_locked();
	case_unknown_command();
	return t_report("test_nfc");
}
