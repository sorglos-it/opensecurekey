/* Pruefungen der Protokollschicht: Statusbericht, Antwort abholen,
 * Warten auf den Taster, Ablauf angefangener Rahmen.
 *
 * Hier wird auch der Speicherbedarf GEDRUCKT statt in einem Kommentar
 * behauptet - eine Zahl in einem Kommentar veraltet still. */
#include "yk_test.h"
#include "yk_hid.h"
#include "yk_frame.h"
#include "yk_ct.h"

/* --- ein Bearbeiter zum Pruefen --------------------------------------- */

static struct {
	unsigned calls;
	uint8_t last_slot;
	uint8_t last_payload[YK_SLOT_DATA_LEN];
	/* Was der Bearbeiter beim naechsten Aufruf tun soll. */
	osk_result_t reply;
	size_t reply_len;
	bool lie_about_len; /* meldet mehr Bytes, als er bekommen hat */
} h;

static osk_result_t handler(uint8_t slot, const uint8_t payload[YK_SLOT_DATA_LEN], uint8_t *out,
			    size_t cap, size_t *out_len, void *ctx)
{
	(void)ctx;
	h.calls++;
	h.last_slot = slot;
	memcpy(h.last_payload, payload, YK_SLOT_DATA_LEN);

	if (h.lie_about_len) {
		*out_len = cap + 1u;
		return OSK_OK;
	}
	if (h.reply != OSK_OK) {
		*out_len = 0u;
		return h.reply;
	}
	for (size_t i = 0; i < h.reply_len && i < cap; i++)
		out[i] = (uint8_t)(0xe0u + i);
	*out_len = h.reply_len;
	return OSK_OK;
}

static void reset_handler(osk_result_t reply, size_t len)
{
	memset(&h, 0, sizeof(h));
	h.reply = reply;
	h.reply_len = len;
}

/* Schickt einen Rahmen hinein, wie ein Host es tut. */
static void send_frame(uint8_t slot, const uint8_t *payload, size_t len)
{
	uint8_t frame[YK_FRAME_LEN];
	uint8_t rpt[YK_RPT_LEN];

	yk_frame_build(slot, payload, len, frame);
	for (unsigned seq = 0; seq < YK_FRAME_CHUNKS; seq++) {
		if (!yk_frame_chunk_needed(frame, seq))
			continue;
		yk_frame_chunk(frame, seq, rpt);
		yk_hid_set_report(rpt);
	}
}

static yk_hid_status_t demo_status(void)
{
	yk_hid_status_t st;

	memset(&st, 0, sizeof(st));
	st.version[0] = 5u;
	st.version[1] = 7u;
	st.version[2] = 1u;
	st.prog_seq = 3u;
	st.touch_level = 0x0210u;
	st.slots_programmed = 0x01u;
	return st;
}

/* --- Pruefungen ------------------------------------------------------- */

static void status_report(void)
{
	uint8_t rpt[YK_RPT_LEN];
	const yk_hid_status_t st = demo_status();

	reset_handler(OSK_ERR_NOT_FOUND, 0u);
	T_EQ("Anmeldung geht", yk_hid_init(handler, NULL, &st), OSK_OK);
	T_EQ("ohne Bearbeiter wird abgewiesen", yk_hid_init(NULL, NULL, &st), OSK_ERR_ARG);
	/* Die abgewiesene Anmeldung darf den Zustand nicht zerstoert haben. */
	yk_hid_init(handler, NULL, &st);

	yk_hid_get_report(rpt);
	T_EQ("Statusbericht: Byte 0 bleibt null", rpt[0], 0);
	T_EQ("Fassung Haupt", rpt[1], 5);
	T_EQ("Fassung Neben", rpt[2], 7);
	T_EQ("Fassung Bau", rpt[3], 1);
	T_EQ("Programmzaehler steht an Byte 4", rpt[4], 3);
	/* Byte 5 traegt Tastgrad UND belegte Steckplaetze. 0x10 oder 0x01
	 * ergibt 0x11. */
	T_EQ("Byte 5: Tastgrad und Steckplaetze", rpt[5], 0x11);
	T_EQ("Byte 6: Tastgrad oben", rpt[6], 0x02);
	T_EQ("Statusbericht hat keinen Schalter", rpt[YK_RPT_STATUS], 0);

	/* Nachfuehren muss ankommen - daran liest der Host ab, ob ein
	 * Schreiben geklappt hat. */
	yk_hid_status_t st2 = st;

	st2.prog_seq = 4u;
	yk_hid_set_status(&st2);
	yk_hid_get_report(rpt);
	T_EQ("nachgefuehrter Programmzaehler", rpt[4], 4);
	yk_hid_set_status(NULL);
	yk_hid_get_report(rpt);
	T_EQ("NULL aendert nichts", rpt[4], 4);
}

static void command_and_answer(void)
{
	uint8_t rpt[YK_RPT_LEN];
	uint8_t challenge[YK_SLOT_DATA_LEN];
	const yk_hid_status_t st = demo_status();
	yk_hid_counts_t c;
	uint8_t got[YK_RESP_MAX];
	size_t n = 0;
	unsigned reports = 0;

	memset(challenge, 0, sizeof(challenge));
	for (size_t i = 0; i < 8u; i++)
		challenge[i] = (uint8_t)(0x31u + i);

	/* 20 Byte Antwort - die Laenge einer HMAC-SHA1. */
	reset_handler(OSK_OK, 20u);
	yk_hid_init(handler, NULL, &st);
	send_frame(YK_SLOT_CHAL_HMAC1, challenge, sizeof(challenge));

	T_EQ("der Bearbeiter wurde einmal gerufen", h.calls, 1);
	T_EQ("mit dem richtigen Steckplatz", h.last_slot, YK_SLOT_CHAL_HMAC1);
	T_HEX("und der richtigen Nutzlast", h.last_payload, 8u, "3132333435363738");

	yk_hid_counts(&c);
	T_EQ("ein Rahmen angenommen", c.frames_ok, 1);
	T_EQ("keiner abgewiesen", c.frames_bad, 0);

	/* Antwort abholen, wie ykman es tut. */
	for (unsigned i = 0; i < 40u; i++) {
		yk_hid_get_report(rpt);
		const uint8_t s = rpt[YK_RPT_STATUS];

		if ((s & YK_FLAG_RESP_PENDING) == 0u)
			break;
		if (reports > 0u && (s & YK_SEQ_MASK) == 0u) {
			reports++;
			break;
		}
		if (n + YK_RPT_DATA <= sizeof(got)) {
			memcpy(got + n, rpt, YK_RPT_DATA);
			n += YK_RPT_DATA;
		}
		reports++;
	}
	T_EQ("drei Stuecke und ein Abschluss", reports, 4);
	T_HEX("die Antwort kommt heil heraus", got, 20u,
	      "e0e1e2e3e4e5e6e7e8e9eaebecedeeeff0f1f2f3");

	/* Danach wieder der Statusbericht. */
	yk_hid_get_report(rpt);
	T_EQ("danach kommt der Statusbericht", rpt[YK_RPT_STATUS], 0);
}

static void no_data_and_lying_handler(void)
{
	uint8_t rpt[YK_RPT_LEN];
	uint8_t payload[4] = {1, 2, 3, 4};
	const yk_hid_status_t st = demo_status();

	/* Bearbeiter ohne Datensatz: der Host bekommt den Statusbericht. */
	reset_handler(OSK_ERR_NOT_FOUND, 0u);
	yk_hid_init(handler, NULL, &st);
	send_frame(YK_SLOT_DEVICE_SERIAL, payload, sizeof(payload));
	T_EQ("der Bearbeiter wurde gerufen", h.calls, 1);
	yk_hid_get_report(rpt);
	T_EQ("ohne Datensatz kommt der Statusbericht", rpt[YK_RPT_STATUS], 0);

	/* Erfolg, aber Laenge 0: auch Statusbericht. */
	reset_handler(OSK_OK, 0u);
	yk_hid_init(handler, NULL, &st);
	send_frame(YK_SLOT_DEVICE_SERIAL, payload, sizeof(payload));
	yk_hid_get_report(rpt);
	T_EQ("Laenge 0 gibt den Statusbericht", rpt[YK_RPT_STATUS], 0);

	/* Ein Bearbeiter, der mehr Bytes meldet, als er bekommen hat,
	 * darf nicht geglaubt werden - sonst wird hier ueber den Puffer
	 * hinaus gelesen. Das ist die Zeile, die ein Sanitizer sonst
	 * findet, wenn ein Strang sich verrechnet. */
	reset_handler(OSK_OK, 0u);
	h.lie_about_len = true;
	yk_hid_init(handler, NULL, &st);
	send_frame(YK_SLOT_DEVICE_SERIAL, payload, sizeof(payload));
	T_EQ("der Bearbeiter wurde gerufen", h.calls, 1);
	yk_hid_get_report(rpt);
	T_EQ("die gelogene Laenge wird nicht geglaubt", rpt[YK_RPT_STATUS], 0);
}

static void touch_wait(void)
{
	uint8_t rpt[YK_RPT_LEN];
	uint8_t payload[4] = {9, 9, 9, 9};
	const yk_hid_status_t st = demo_status();
	yk_hid_counts_t c;

	reset_handler(OSK_ERR_DENIED, 0u);
	yk_hid_init(handler, NULL, &st);
	yk_hid_tick(1000u);
	send_frame(YK_SLOT_CHAL_HMAC2, payload, sizeof(payload));

	T_EQ("der Bearbeiter hat den Taster verlangt", h.calls, 1);
	yk_hid_get_report(rpt);
	T_TRUE("Wartezustand gemeldet", (rpt[YK_RPT_STATUS] & YK_FLAG_TIMEOUT_WAIT) != 0u);
	T_TRUE("ohne Antwortschalter", (rpt[YK_RPT_STATUS] & YK_FLAG_RESP_PENDING) == 0u);
	T_EQ("volle Restzeit in Sekunden", rpt[YK_RPT_STATUS] & YK_SEQ_MASK, 15);

	/* Zeit laeuft: nach 10 Sekunden bleiben 5. */
	yk_hid_tick(11000u);
	yk_hid_get_report(rpt);
	T_EQ("Restzeit laeuft mit", rpt[YK_RPT_STATUS] & YK_SEQ_MASK, 5);

	/* Taster gedrueckt: der Bearbeiter wird erneut gerufen, diesmal
	 * mit derselben Nutzlast. */
	h.reply = OSK_OK;
	h.reply_len = 4u;
	yk_hid_touch_done(true);
	T_EQ("der Bearbeiter wurde ein zweites Mal gerufen", h.calls, 2);
	T_HEX("mit derselben Nutzlast", h.last_payload, 4u, "09090909");
	yk_hid_get_report(rpt);
	T_TRUE("jetzt kommt die Antwort", (rpt[YK_RPT_STATUS] & YK_FLAG_RESP_PENDING) != 0u);

	/* Verweigert: kein zweiter Aufruf, zurueck in den Ruhezustand. */
	reset_handler(OSK_ERR_DENIED, 0u);
	yk_hid_init(handler, NULL, &st);
	yk_hid_tick(0u);
	send_frame(YK_SLOT_CHAL_HMAC2, payload, sizeof(payload));
	yk_hid_touch_done(false);
	T_EQ("verweigert ruft nicht erneut", h.calls, 1);
	yk_hid_get_report(rpt);
	T_EQ("und es kommt der Statusbericht", rpt[YK_RPT_STATUS], 0);

	/* Ein Bearbeiter, der immer den Taster verlangt, darf die
	 * Wartezeit nicht beliebig verlaengern: nach der Freigabe steht
	 * der Wartezustand neu, aber die Uhr auch. */
	reset_handler(OSK_ERR_DENIED, 0u);
	yk_hid_init(handler, NULL, &st);
	yk_hid_tick(0u);
	send_frame(YK_SLOT_CHAL_HMAC2, payload, sizeof(payload));
	yk_hid_tick(1000u);
	yk_hid_touch_done(true);
	T_EQ("zweiter Aufruf", h.calls, 2);
	yk_hid_tick(1000u + YK_HID_TOUCH_TIMEOUT_MS);
	yk_hid_counts(&c);
	T_TRUE("das Warten laeuft ab", c.timeouts >= 1u);
	yk_hid_get_report(rpt);
	T_EQ("nach dem Ablauf kommt der Statusbericht", rpt[YK_RPT_STATUS], 0);
}

static void frame_timeout(void)
{
	uint8_t frame[YK_FRAME_LEN];
	uint8_t rpt[YK_RPT_LEN];
	uint8_t payload[YK_SLOT_DATA_LEN];
	const yk_hid_status_t st = demo_status();
	yk_hid_counts_t c;

	for (size_t i = 0; i < sizeof(payload); i++)
		payload[i] = (uint8_t)(i + 1u);
	yk_frame_build(YK_SLOT_CONFIG, payload, sizeof(payload), frame);

	reset_handler(OSK_OK, 4u);
	yk_hid_init(handler, NULL, &st);
	yk_hid_tick(5000u);

	/* Halber Rahmen, dann Stille. */
	for (unsigned seq = 0; seq < 5u; seq++) {
		yk_frame_chunk(frame, seq, rpt);
		yk_hid_set_report(rpt);
	}
	yk_hid_tick(5000u + YK_HID_FRAME_TIMEOUT_MS);
	yk_hid_counts(&c);
	T_EQ("der angefangene Rahmen ist abgelaufen", c.timeouts, 1);

	/* Danach ist die Fortsetzung kein gueltiger Anschluss mehr - ein
	 * Angreifer kann den Zusammenbau also nicht offen halten und
	 * spaeter vollenden. */
	yk_frame_chunk(frame, 5u, rpt);
	T_EQ("die Fortsetzung wird abgewiesen", yk_hid_set_report(rpt), OSK_ERR_FORMAT);
	T_EQ("der Bearbeiter wurde nie gerufen", h.calls, 0);

	/* Und ein vollstaendiger Rahmen danach geht wieder. */
	send_frame(YK_SLOT_CONFIG, payload, sizeof(payload));
	T_EQ("danach geht es wieder", h.calls, 1);
}

static void reset_and_abort(void)
{
	uint8_t rpt[YK_RPT_LEN];
	uint8_t payload[4] = {1, 2, 3, 4};
	const yk_hid_status_t st = demo_status();
	yk_hid_counts_t c;

	/* Der Ruecksetzbericht wirft eine halb abgeholte Antwort weg. */
	reset_handler(OSK_OK, 20u);
	yk_hid_init(handler, NULL, &st);
	send_frame(YK_SLOT_CHAL_HMAC1, payload, sizeof(payload));
	yk_hid_get_report(rpt);
	T_TRUE("erstes Stueck der Antwort", (rpt[YK_RPT_STATUS] & YK_FLAG_RESP_PENDING) != 0u);

	memset(rpt, 0xff, sizeof(rpt));
	T_EQ("Ruecksetzbericht angenommen", yk_hid_set_report(rpt), OSK_OK);
	yk_hid_get_report(rpt);
	T_EQ("die Antwort ist weg", rpt[YK_RPT_STATUS], 0);
	yk_hid_counts(&c);
	T_EQ("ein Ruecksetzen gezaehlt", c.resets, 1);

	/* Der Leerbericht bricht ab. */
	reset_handler(OSK_OK, 20u);
	yk_hid_init(handler, NULL, &st);
	send_frame(YK_SLOT_CHAL_HMAC1, payload, sizeof(payload));
	memset(rpt, 0, sizeof(rpt));
	rpt[YK_RPT_STATUS] = (uint8_t)YK_DUMMY_REPORT;
	T_EQ("Leerbericht angenommen", yk_hid_set_report(rpt), OSK_OK);
	yk_hid_get_report(rpt);
	T_EQ("die Antwort ist weg", rpt[YK_RPT_STATUS], 0);
	yk_hid_counts(&c);
	T_EQ("ein Abbruch gezaehlt", c.aborts, 1);

	/* NULL-Bericht. */
	T_EQ("NULL wird abgewiesen", yk_hid_set_report(NULL), OSK_ERR_ARG);
	yk_hid_get_report(NULL); /* darf nur nichts tun */
}

static void constant_time(void)
{
	const uint8_t a[YK_ACC_CODE_LEN] = {1, 2, 3, 4, 5, 6};
	const uint8_t b[YK_ACC_CODE_LEN] = {1, 2, 3, 4, 5, 6};
	const uint8_t first_differs[YK_ACC_CODE_LEN] = {9, 2, 3, 4, 5, 6};
	const uint8_t last_differs[YK_ACC_CODE_LEN] = {1, 2, 3, 4, 5, 9};

	T_TRUE("gleich ist gleich", yk_ct_acc_code_ok(a, b));
	T_TRUE("erstes Byte anders", !yk_ct_acc_code_ok(a, first_differs));
	T_TRUE("letztes Byte anders", !yk_ct_acc_code_ok(a, last_differs));
	T_TRUE("Laenge 0 ist gleich", yk_ct_equal(a, b, 0));
	T_TRUE("NULL ist ungleich", !yk_ct_equal(a, NULL, 6));
	T_TRUE("NULL ist ungleich", !yk_ct_equal(NULL, b, 6));

	/* WAS HIER NICHT GEPRUEFT WIRD, und warum das ehrlich bleiben
	 * muss: dass die Laufzeit WIRKLICH gleich ist. Das ist am PC
	 * nicht messbar - der Rechner hat Sprungvorhersage, Zwischenspeicher
	 * und andere Last, und eine Zeitmessung hier wuerde ein Ergebnis
	 * liefern, das ueber den nRF52840 nichts sagt. Was hier geprueft
	 * ist: der Vergleich faellt bei einem Unterschied am ANFANG genauso
	 * aus wie bei einem am ENDE. Die Gleichheit der Laufzeit ist durch
	 * den Aufbau gegeben (keine Verzweigung ueber den Inhalt, volatile
	 * gegen das Zusammenziehen) und bleibt bis zu einer Messung am
	 * Geraet eine Zusicherung des Aufbaus, kein Messwert. Stufe 2. */
}

static void sizes(void)
{
	/* Gedruckt, nicht behauptet. Wer die Puffer aendert, sieht die
	 * neue Zahl im Prueflauf - ein Kommentar waere still veraltet. */
	printf("  Speicherbedarf (sizeof auf diesem Rechner):\n");
	printf("    yk_frame_rx_t  %3zu Byte\n", sizeof(yk_frame_rx_t));
	printf("    yk_frame_tx_t  %3zu Byte\n", sizeof(yk_frame_tx_t));
	printf("    yk_hid_status_t %2zu Byte\n", sizeof(yk_hid_status_t));
	printf("    Antwortgrenze  %3u Byte\n", (unsigned)YK_RESP_MAX);

	/* Der Zustand von yk_hid.c liegt statisch und ist von aussen
	 * nicht messbar. Was messbar ist: die Teile, aus denen er
	 * besteht. Die Summe steht in yk_hid.c und wird hier gegen die
	 * Teile gehalten, damit beide nicht auseinanderlaufen. */
	const size_t parts = sizeof(yk_frame_rx_t) + sizeof(yk_frame_tx_t) + YK_SLOT_DATA_LEN +
			     YK_RESP_MAX;

	printf("    Summe der grossen Felder %zu Byte\n", parts);
	T_TRUE("die grossen Felder bleiben unter 700 Byte", parts < 700u);
}

void suite_ykhid(void)
{
	printf("== Protokollschicht\n");
	status_report();
	command_and_answer();
	no_data_and_lying_handler();
	touch_wait();
	frame_timeout();
	reset_and_abort();
	constant_time();
	sizes();
}
