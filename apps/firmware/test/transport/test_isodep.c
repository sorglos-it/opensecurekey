/* Pruefungen der ISO-DEP-Schicht: echte Rahmen hinein, Antworten pruefen.
 *
 * Es wird mit Rahmen gearbeitet, wie sie ein Lesegeraet schickt - also mit
 * Steuerbyte, Blocknummer, Kettenbit und Kennung. Geprueft wird vor allem,
 * was die Schicht NICHT tut: zweimal bearbeiten, ueber den Puffer lesen,
 * auf Unsinn antworten.
 *
 * Es ist keine Funkmessung dabei. Siehe docs/funk.md. */
#include <stdlib.h>
#include "test_util.h"
#include "mocks.h"
#include "nfc_isodep.h"

/* Was hinausging. */
#define MAX_OUT 32
static uint8_t g_out[MAX_OUT][OSK_ISODEP_FRAME_MAX];
static size_t g_out_len[MAX_OUT];
static size_t g_out_count;

/* Was der Bearbeiter zu sehen bekam. */
static uint8_t g_req[OSK_ISODEP_APDU_MAX];
static size_t g_req_len;
static unsigned g_calls;
/* So lang antwortet der Bearbeiter. */
static size_t g_reply_len = 4;

static void sink(const uint8_t *frame, size_t len, void *ctx)
{
	(void)ctx;
	if (g_out_count < MAX_OUT && len <= OSK_ISODEP_FRAME_MAX) {
		memcpy(g_out[g_out_count], frame, len);
		g_out_len[g_out_count] = len;
	}
	g_out_count++;
}

static size_t handler(const uint8_t *apdu, size_t len, uint8_t *out, size_t cap, void *ctx)
{
	(void)ctx;
	g_calls++;
	g_req_len = (len <= sizeof(g_req)) ? len : sizeof(g_req);
	memcpy(g_req, apdu, g_req_len);
	if (g_reply_len > cap)
		return 0;
	for (size_t i = 0; i < g_reply_len; i++)
		out[i] = (uint8_t)(i & 0xffu);
	return g_reply_len;
}

static void clear(void)
{
	g_out_count = 0;
	g_req_len = 0;
	g_calls = 0;
	memset(g_out_len, 0, sizeof(g_out_len));
}

static void start(size_t reply_len)
{
	mock_reset_all();
	mock_clock_set(1000);
	g_reply_len = reply_len;
	T_EQ("Start", osk_isodep_init(sink, handler, NULL), OSK_OK);
	osk_isodep_field_on();
	clear();
}

/* Einen I-Block bauen und hineingeben. */
static void send_i(uint8_t bn, bool chaining, const uint8_t *data, size_t len)
{
	uint8_t frame[OSK_ISODEP_FRAME_MAX];
	size_t at = 0;

	frame[at++] =
	    (uint8_t)(OSK_ISODEP_I_BLOCK | (bn & 1u) | (chaining ? OSK_ISODEP_CHAINING : 0u));
	if (len)
		memcpy(frame + at, data, len);
	osk_isodep_rx_frame(frame, at + len);
}

static void send_raw(const uint8_t *frame, size_t len)
{
	osk_isodep_rx_frame(frame, len);
}

static void case_simple(void)
{
	const uint8_t cmd[] = {0x00, 0xa4, 0x04, 0x00, 0x01, 0xaa};

	start(4);
	send_i(0, false, cmd, sizeof(cmd));
	T_EQ("einmal bearbeitet", g_calls, 1);
	T_EQ("ganze APDU angekommen", g_req_len, sizeof(cmd));
	T_TRUE("Inhalt stimmt", memcmp(g_req, cmd, sizeof(cmd)) == 0);
	T_EQ("ein Rahmen hinaus", g_out_count, 1);
	T_EQ("I-Block mit derselben Blocknummer", g_out[0][0], 0x02);
	T_EQ("Antwortlaenge", g_out_len[0], 5);

	/* Der naechste Befehl kommt mit der anderen Blocknummer. */
	clear();
	send_i(1, false, cmd, sizeof(cmd));
	T_EQ("zweiter Befehl bearbeitet", g_calls, 1);
	T_EQ("Antwort mit Blocknummer 1", g_out[0][0], 0x03);
}

static void case_field(void)
{
	const uint8_t cmd[] = {0x00, 0xa4, 0x04, 0x00};

	start(4);
	osk_isodep_field_off();
	clear();
	send_i(0, false, cmd, sizeof(cmd));
	T_EQ("ohne Feld nichts", g_out_count, 0);
	T_EQ("ohne Feld kein Aufruf", g_calls, 0);

	/* Das Feld kommt wieder: alles beginnt von vorn. */
	osk_isodep_field_on();
	clear();
	send_i(0, false, cmd, sizeof(cmd));
	T_EQ("mit Feld wieder da", g_calls, 1);
}

static void case_rx_chaining(void)
{
	uint8_t part[200];

	for (size_t i = 0; i < sizeof(part); i++)
		part[i] = (uint8_t)i;

	start(4);
	/* Erstes Glied mit Kettenbit: Bestaetigung, kein Aufruf. */
	send_i(0, true, part, sizeof(part));
	T_EQ("noch nicht bearbeitet", g_calls, 0);
	T_EQ("eine Bestaetigung", g_out_count, 1);
	T_EQ("R(ACK) mit Blocknummer 0", g_out[0][0], 0xa2);
	T_EQ("Bestaetigung ohne Daten", g_out_len[0], 1);

	clear();
	send_i(1, true, part, sizeof(part));
	T_EQ("noch nicht bearbeitet", g_calls, 0);
	T_EQ("R(ACK) mit Blocknummer 1", g_out[0][0], 0xa3);

	clear();
	send_i(0, false, part, 50);
	T_EQ("jetzt bearbeitet", g_calls, 1);
	T_EQ("alles zusammengesetzt", g_req_len, 450);
	T_TRUE("erstes Glied vorn", memcmp(g_req, part, sizeof(part)) == 0);
	T_TRUE("zweites Glied dahinter", memcmp(g_req + 200, part, sizeof(part)) == 0);
	T_TRUE("drittes Glied am Ende", memcmp(g_req + 400, part, 50) == 0);
}

static void case_tx_chaining(void)
{
	const uint8_t cmd[] = {0x00, 0xb0, 0x00, 0x00, 0x00};
	const uint8_t ack0[] = {0xa2};
	const uint8_t ack1[] = {0xa3};

	/* 600 Byte Antwort bei 256 Byte Rahmen: 255 + 255 + 90. */
	start(600);
	send_i(0, false, cmd, sizeof(cmd));
	T_EQ("erstes Stueck", g_out_count, 1);
	T_EQ("I-Block mit Kettenbit, Blocknummer 0", g_out[0][0], 0x12);
	T_EQ("voller Rahmen", g_out_len[0], 256);

	clear();
	send_raw(ack1, sizeof(ack1));
	T_EQ("zweites Stueck", g_out_count, 1);
	T_EQ("Kettenbit, Blocknummer 1", g_out[0][0], 0x13);
	T_EQ("voller Rahmen", g_out_len[0], 256);

	clear();
	send_raw(ack0, sizeof(ack0));
	T_EQ("letztes Stueck", g_out_count, 1);
	T_EQ("ohne Kettenbit", g_out[0][0], 0x02);
	T_EQ("Rest", g_out_len[0], 91);
	T_TRUE("Kette zu Ende", !osk_isodep_busy());

	/* Eine weitere Bestaetigung holt nichts Neues, sondern wiederholt. */
	clear();
	send_raw(ack1, sizeof(ack1));
	T_EQ("Wiederholung", g_out_count, 1);
	T_EQ("derselbe Rahmen", g_out_len[0], 91);
}

static void case_frame_size(void)
{
	const uint8_t cmd[] = {0x00, 0xb0, 0x00, 0x00, 0x00};

	/* Ein Lesegeraet mit kleiner Hoechstlaenge: 32 Byte Rahmen. */
	start(100);
	osk_isodep_set_frame_size(32);
	clear();
	send_i(0, false, cmd, sizeof(cmd));
	T_EQ("kleiner Rahmen", g_out_len[0], 32);
	T_EQ("mit Kettenbit", g_out[0][0], 0x12);
	osk_isodep_set_frame_size(OSK_ISODEP_FRAME_MAX);
}

static void case_retransmit(void)
{
	const uint8_t cmd[] = {0x00, 0xa4, 0x04, 0x00};
	const uint8_t nak[] = {0xb2};

	start(4);
	send_i(0, false, cmd, sizeof(cmd));
	T_EQ("bearbeitet", g_calls, 1);

	/* Derselbe Block noch einmal: die Antwort kam offenbar nicht an. */
	clear();
	send_i(0, false, cmd, sizeof(cmd));
	T_EQ("nicht zweimal bearbeitet", g_calls, 0);
	T_EQ("aber wiederholt", g_out_count, 1);
	T_EQ("derselbe Rahmen", g_out_len[0], 5);

	/* R(NAK) wiederholt ebenfalls, ohne neu zu bearbeiten. */
	clear();
	send_raw(nak, sizeof(nak));
	T_EQ("NAK bearbeitet nichts", g_calls, 0);
	T_EQ("NAK wiederholt", g_out_count, 1);
}

static void case_deselect(void)
{
	const uint8_t cmd[] = {0x00, 0xa4, 0x04, 0x00};
	const uint8_t deselect[] = {0xc2};

	start(4);
	send_i(0, false, cmd, sizeof(cmd));
	clear();
	send_raw(deselect, sizeof(deselect));
	T_EQ("eine Antwort", g_out_count, 1);
	T_EQ("S(DESELECT) zurueck", g_out[0][0], 0xc2);

	/* Danach beginnt die Blocknummer wieder bei 0. */
	clear();
	send_i(0, false, cmd, sizeof(cmd));
	T_EQ("neu bearbeitet", g_calls, 1);
}

static void case_wtx(void)
{
	const uint8_t cmd[] = {0x00, 0xa4, 0x04, 0x00};
	const uint8_t nak[] = {0xb2};
	const uint8_t wtx_answer[] = {0xf2, 0x05};

	start(4);
	send_i(0, false, cmd, sizeof(cmd));
	clear();

	osk_isodep_request_more_time(5);
	T_EQ("Zeitbitte hinaus", g_out_count, 1);
	T_EQ("S(WTX)", g_out[0][0], 0xf2);
	T_EQ("mit Faktor", g_out[0][1], 5);
	T_EQ("zwei Byte", g_out_len[0], 2);

	/* Zu grosse und zu kleine Faktoren werden begrenzt. */
	clear();
	osk_isodep_request_more_time(0);
	T_EQ("0 wird begrenzt", g_out[0][1], OSK_ISODEP_WTX_MAX);
	clear();
	osk_isodep_request_more_time(200);
	T_EQ("200 wird begrenzt", g_out[0][1], OSK_ISODEP_WTX_MAX);

	/* Die Antwort des Lesegeraets wird nicht beantwortet. */
	clear();
	send_raw(wtx_answer, sizeof(wtx_answer));
	T_EQ("keine Antwort auf S(WTX)", g_out_count, 0);

	/* Und die Zeitbitte hat den wiederholbaren Rahmen nicht ersetzt. */
	clear();
	send_raw(nak, sizeof(nak));
	T_EQ("wiederholt", g_out_count, 1);
	T_EQ("die echte Antwort, nicht die Zeitbitte", g_out_len[0], 5);
	T_EQ("I-Block", g_out[0][0], 0x02);
}

static void case_cid(void)
{
	const uint8_t with_cid[] = {0x0a, 0x07, 0x00, 0xa4, 0x04, 0x00};
	const uint8_t wrong_cid[] = {0x0b, 0x03, 0x00, 0xa4, 0x04, 0x00};
	const uint8_t no_cid[] = {0x03, 0x00, 0xa4, 0x04, 0x00};

	start(4);
	send_raw(with_cid, sizeof(with_cid));
	T_EQ("bearbeitet", g_calls, 1);
	T_EQ("APDU ohne Prolog", g_req_len, 4);
	T_EQ("Kennung kommt zurueck", g_out[0][0], 0x0a);
	T_EQ("dieselbe Kennung", g_out[0][1], 0x07);
	T_EQ("Antwort um ein Byte laenger", g_out_len[0], 6);

	/* Eine andere Kennung mitten in der Sitzung ist ein Fehler. */
	clear();
	send_raw(wrong_cid, sizeof(wrong_cid));
	T_EQ("fremde Kennung wird verworfen", g_out_count, 0);
	T_EQ("kein Aufruf", g_calls, 0);

	/* Und wer die Kennung einmal benutzt, muss sie weiter benutzen. */
	clear();
	send_raw(no_cid, sizeof(no_cid));
	T_EQ("ohne Kennung wird verworfen", g_out_count, 0);
}

static void case_broken_frames(void)
{
	start(4);
	clear();

	/* Leerer Rahmen. */
	const uint8_t empty[1] = {0};

	osk_isodep_rx_frame(empty, 0);
	T_EQ("leerer Rahmen: nichts", g_out_count, 0);
	osk_isodep_rx_frame(NULL, 4);
	T_EQ("NULL: nichts", g_out_count, 0);

	/* I-Block ohne das festgelegte Bit. */
	const uint8_t no_bit[] = {0x00, 0x00, 0xa4};

	send_raw(no_bit, sizeof(no_bit));
	T_EQ("festes Bit fehlt", g_out_count, 0);

	/* I-Block mit gesetztem RFU-Bit. */
	const uint8_t rfu[] = {0x22, 0x00, 0xa4};

	send_raw(rfu, sizeof(rfu));
	T_EQ("RFU-Bit gesetzt", g_out_count, 0);

	/* Knotenadresse: koennen wir nicht. */
	const uint8_t nad[] = {0x06, 0x01, 0x00, 0xa4};

	send_raw(nad, sizeof(nad));
	T_EQ("Knotenadresse: Schweigen", g_out_count, 0);

	/* Kennungsbit gesetzt, aber kein Kennungsbyte da. */
	const uint8_t cid_missing[] = {0x0a};

	send_raw(cid_missing, sizeof(cid_missing));
	T_EQ("Kennung angekuendigt, nicht da", g_out_count, 0);

	/* R-Block mit Daten gibt es nicht. */
	const uint8_t fat_r[] = {0xa2, 0x11, 0x22};

	send_raw(fat_r, sizeof(fat_r));
	T_EQ("R-Block mit Daten", g_out_count, 0);

	/* Nicht vergebener Blocktyp. */
	const uint8_t unknown[] = {0x42, 0x00};

	send_raw(unknown, sizeof(unknown));
	T_EQ("unbekannter Blocktyp", g_out_count, 0);

	T_EQ("kein einziger Aufruf", g_calls, 0);
}

static void case_overflow(void)
{
	uint8_t part[250];

	memset(part, 0x5a, sizeof(part));
	start(4);

	/* So lange kettenbilden, bis der Puffer voll ist. Die Schicht darf
	 * dabei nicht ueber den Puffer schreiben und nichts bearbeiten. */
	unsigned sent = 0;

	for (unsigned i = 0; i < 20; i++) {
		clear();
		send_i((uint8_t)(i & 1u), true, part, sizeof(part));
		if (g_out_count == 0)
			break;
		sent++;
	}
	T_TRUE("die Kette wurde abgebrochen", sent < 20);
	T_EQ("nichts bearbeitet", g_calls, 0);

	/* Nach dem Abbruch ist der Zustand leer: ein neuer Befehl kommt
	 * sauber durch, ohne Reste der abgebrochenen Kette. */
	osk_isodep_field_on();
	clear();
	const uint8_t cmd[] = {0x00, 0xa4, 0x04, 0x00};

	send_i(0, false, cmd, sizeof(cmd));
	T_EQ("neuer Befehl bearbeitet", g_calls, 1);
	T_EQ("ohne Reste", g_req_len, sizeof(cmd));
}

static void case_timeout(void)
{
	const uint8_t cmd[] = {0x00, 0xa4, 0x04, 0x00};
	uint8_t part[100];

	memset(part, 0x11, sizeof(part));
	start(4);

	send_i(0, true, part, sizeof(part));
	T_EQ("Bestaetigung", g_out_count, 1);

	/* Vor der Grenze bleibt die halbe Kette liegen. */
	clear();
	mock_clock_advance(OSK_ISODEP_TIMEOUT_MS - 1);
	osk_isodep_tick();
	send_i(1, false, cmd, sizeof(cmd));
	T_EQ("bearbeitet", g_calls, 1);
	T_EQ("halbe Kette war noch da", g_req_len, sizeof(part) + sizeof(cmd));

	/* Nach der Grenze ist sie weg. */
	start(4);
	send_i(0, true, part, sizeof(part));
	clear();
	mock_clock_advance(OSK_ISODEP_TIMEOUT_MS + 1);
	osk_isodep_tick();
	T_EQ("Ablauf antwortet nicht", g_out_count, 0);
	send_i(1, false, cmd, sizeof(cmd));
	T_EQ("neu bearbeitet", g_calls, 1);
	T_EQ("ohne die alte halbe Kette", g_req_len, sizeof(cmd));
}

static void case_mute_handler(void)
{
	const uint8_t cmd[] = {0x00, 0xa4, 0x04, 0x00};

	/* Ein Bearbeiter, der nichts liefert: die Schicht schweigt, statt
	 * einen leeren Rahmen zu senden. */
	start(0);
	send_i(0, false, cmd, sizeof(cmd));
	T_EQ("bearbeitet", g_calls, 1);
	T_EQ("kein Rahmen", g_out_count, 0);

	/* Eine Antwort, die nicht in den Puffer passt, ebenso. */
	start(OSK_ISODEP_APDU_MAX + 1);
	send_i(0, false, cmd, sizeof(cmd));
	T_EQ("kein Rahmen bei zu langer Antwort", g_out_count, 0);
}

static void case_init(void)
{
	T_EQ("ohne Senden kein Start", osk_isodep_init(NULL, handler, NULL), OSK_ERR_ARG);
	T_EQ("ohne Bearbeiter kein Start", osk_isodep_init(sink, NULL, NULL), OSK_ERR_ARG);
}

int main(void)
{
	case_init();
	case_simple();
	case_field();
	case_rx_chaining();
	case_tx_chaining();
	case_frame_size();
	case_retransmit();
	case_deselect();
	case_wtx();
	case_cid();
	case_broken_frames();
	case_overflow();
	case_timeout();
	case_mute_handler();
	return t_report("test_isodep");
}
