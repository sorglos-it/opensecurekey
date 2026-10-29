/* Siehe yk_hid.h. */
#include <string.h>

#include "yk_hid.h"
#include "yk_frame.h"
#include "osk_mem.h"

typedef enum {
	ST_IDLE,  /* nichts zu tun - der Host bekommt den Statusbericht */
	ST_TX,	  /* eine Antwort geht Stueck fuer Stueck hinaus */
	ST_TOUCH, /* es wird auf den Taster gewartet */
} state_t;

/* Ein Geraet hat genau eine solche Schnittstelle, darum ein Zustand.
 * Er liegt statisch und nicht auf dem Stapel: ein Zephyr-Fadenstapel ist
 * oft nur 1 bis 2 kB gross, und hier stecken ueber 500 Byte Puffer. */
static struct {
	yk_hid_slot_fn handler;
	void *ctx;
	yk_hid_status_t status;

	yk_frame_rx_t rx;
	yk_frame_tx_t tx;

	state_t state;
	uint32_t now_ms;
	uint32_t frame_started_ms;
	uint32_t touch_started_ms;

	/* Der Befehl, der auf den Taster wartet. Er muss liegen bleiben,
	 * weil der Bearbeiter nach der Freigabe erneut gerufen wird. */
	uint8_t pend_slot;
	uint8_t pend_payload[YK_SLOT_DATA_LEN];

	/* Die Antwort des Bearbeiters, bevor sie zerlegt wird. Eigener
	 * Puffer, damit der Bearbeiter nicht in den Sendezustand schreibt,
	 * waehrend der noch gelesen wird. */
	uint8_t out[YK_RESP_MAX];

	yk_hid_counts_t counts;
	bool ready;
} g;

/* --- Statusbericht ---------------------------------------------------- */

static void fill_status(uint8_t rpt[YK_RPT_LEN])
{
	memset(rpt, 0, YK_RPT_LEN);
	rpt[1] = g.status.version[0];
	rpt[2] = g.status.version[1];
	rpt[3] = g.status.version[2];
	rpt[4] = g.status.prog_seq;
	/* Byte 5 traegt zweierlei: den niederwertigen Teil des Tastgrades
	 * und in den beiden untersten Bit, welche Steckplaetze belegt sind.
	 * [2] liest beides an derselben Stelle. Weil das zwei Groessen mit
	 * einem Namen sind, steht die Verbindung genau hier und nur hier -
	 * der Aufrufer liefert sie getrennt. */
	rpt[5] = (uint8_t)((uint8_t)(g.status.touch_level & 0xffu) |
			   (uint8_t)(g.status.slots_programmed & 0x03u));
	rpt[6] = (uint8_t)((g.status.touch_level >> 8) & 0xffu);
	rpt[YK_RPT_STATUS] = 0u; /* kein Schalter: das ist der Statusbericht */
}

/* --- Befehl ausfuehren ------------------------------------------------ */

/* Ruft den Bearbeiter und bereitet die Antwort vor. */
static void run_handler(uint8_t slot, const uint8_t *payload)
{
	size_t out_len = 0u;

	if (g.handler == NULL) {
		g.state = ST_IDLE;
		return;
	}

	osk_wipe(g.out, sizeof(g.out));

	const osk_result_t r = g.handler(slot, payload, g.out, sizeof(g.out), &out_len, g.ctx);

	if (r == OSK_ERR_DENIED) {
		/* Der Bearbeiter will den Taster. Befehl liegen lassen. */
		g.pend_slot = slot;
		memcpy(g.pend_payload, payload, YK_SLOT_DATA_LEN);
		g.touch_started_ms = g.now_ms;
		g.state = ST_TOUCH;
		return;
	}

	/* Ein Bearbeiter, der Erfolg meldet und mehr Bytes nennt, als er
	 * bekommen hat, wird nicht geglaubt - sonst wuerde hier ueber den
	 * Puffer hinaus gelesen. */
	if (r != OSK_OK || out_len > sizeof(g.out)) {
		osk_wipe(g.out, sizeof(g.out));
		g.state = ST_IDLE;
		return;
	}

	if (out_len == 0u) {
		/* Kein Datensatz: der Host bekommt den Statusbericht und
		 * liest am Programmzaehler ab, ob es geklappt hat. */
		g.state = ST_IDLE;
		return;
	}

	if (!yk_frame_tx_start(&g.tx, g.out, out_len)) {
		g.state = ST_IDLE;
		return;
	}
	g.state = ST_TX;
}

/* --- Schnittstelle --------------------------------------------------- */

osk_result_t yk_hid_init(yk_hid_slot_fn handler, void *ctx, const yk_hid_status_t *status)
{
	if (handler == NULL)
		return OSK_ERR_ARG;

	osk_wipe(&g, sizeof(g));
	g.handler = handler;
	g.ctx = ctx;
	if (status != NULL)
		g.status = *status;
	yk_frame_rx_reset(&g.rx);
	yk_frame_tx_reset(&g.tx);
	g.state = ST_IDLE;
	g.ready = true;
	return OSK_OK;
}

void yk_hid_set_status(const yk_hid_status_t *status)
{
	if (status == NULL)
		return;
	g.status = *status;
}

osk_result_t yk_hid_set_report(const uint8_t rpt[YK_RPT_LEN])
{
	if (!g.ready)
		return OSK_ERR_INTERNAL;
	if (rpt == NULL)
		return OSK_ERR_ARG;

	const yk_rx_t r = yk_frame_rx_report(&g.rx, rpt);

	switch (r) {
	case YK_RX_RESET:
		/* Der Host raeumt auf. Eine halb gesendete Antwort ist
		 * damit verfallen. */
		yk_frame_tx_reset(&g.tx);
		osk_wipe(g.out, sizeof(g.out));
		g.state = ST_IDLE;
		g.counts.resets++;
		return OSK_OK;

	case YK_RX_ABORT:
		yk_frame_tx_reset(&g.tx);
		osk_wipe(g.out, sizeof(g.out));
		osk_wipe(g.pend_payload, sizeof(g.pend_payload));
		g.state = ST_IDLE;
		g.counts.aborts++;
		return OSK_OK;

	case YK_RX_MORE:
		if (g.rx.last_seq == 0u)
			g.frame_started_ms = g.now_ms;
		return OSK_OK;

	case YK_RX_FRAME:
		g.counts.frames_ok++;
		/* Ein neuer Befehl wirft eine noch laufende Antwort weg -
		 * der Host hat sie offenbar nicht mehr gewollt. */
		yk_frame_tx_reset(&g.tx);
		run_handler(yk_frame_slot(&g.rx), yk_frame_payload(&g.rx));
		yk_frame_rx_reset(&g.rx);
		return OSK_OK;

	case YK_RX_BAD:
		g.counts.frames_bad++;
		return OSK_ERR_FORMAT;

	case YK_RX_IGNORE:
	default:
		return OSK_OK;
	}
}

void yk_hid_get_report(uint8_t rpt[YK_RPT_LEN])
{
	if (rpt == NULL)
		return;
	if (!g.ready) {
		memset(rpt, 0, YK_RPT_LEN);
		return;
	}

	if (g.state == ST_TX) {
		if (yk_frame_tx_next(&g.tx, rpt))
			return;
		/* Nichts mehr da - zurueck in den Ruhezustand. */
		g.state = ST_IDLE;
		osk_wipe(g.out, sizeof(g.out));
	}

	if (g.state == ST_TOUCH) {
		/* Wartezustand: 0x20 und in den unteren 5 Bit die
		 * Restsekunden. Mehr als 31 passt dort nicht, darum wird
		 * gedeckelt statt umgebrochen - ein Umbruch wuerde dem Host
		 * eine kurze Restzeit melden und ihn zu frueh aufgeben
		 * lassen. */
		const uint32_t waited = g.now_ms - g.touch_started_ms;
		uint32_t left_s = 0u;

		if (waited < YK_HID_TOUCH_TIMEOUT_MS)
			left_s = (YK_HID_TOUCH_TIMEOUT_MS - waited + 999u) / 1000u;
		if (left_s > YK_SEQ_MASK)
			left_s = YK_SEQ_MASK;

		memset(rpt, 0, YK_RPT_LEN);
		rpt[YK_RPT_STATUS] =
		    (uint8_t)(YK_FLAG_TIMEOUT_WAIT | (uint8_t)(left_s & YK_SEQ_MASK));
		return;
	}

	fill_status(rpt);
}

void yk_hid_tick(uint32_t now_ms)
{
	g.now_ms = now_ms;

	/* Ein angefangener Rahmen darf nicht auf Dauer offen bleiben:
	 * sonst haelt ein einziges Paket den Zusammenbau besetzt und der
	 * naechste ehrliche Host sieht einen fremden Anfang. */
	if (g.rx.active && (uint32_t)(now_ms - g.frame_started_ms) >= YK_HID_FRAME_TIMEOUT_MS) {
		yk_frame_rx_reset(&g.rx);
		g.counts.timeouts++;
	}

	if (g.state == ST_TOUCH &&
	    (uint32_t)(now_ms - g.touch_started_ms) >= YK_HID_TOUCH_TIMEOUT_MS) {
		osk_wipe(g.pend_payload, sizeof(g.pend_payload));
		g.state = ST_IDLE;
		g.counts.timeouts++;
	}
}

void yk_hid_touch_done(bool granted)
{
	if (!g.ready || g.state != ST_TOUCH)
		return;

	const uint8_t slot = g.pend_slot;

	if (!granted) {
		osk_wipe(g.pend_payload, sizeof(g.pend_payload));
		g.state = ST_IDLE;
		return;
	}

	/* Vor dem zweiten Aufruf aus dem Wartezustand heraus, sonst
	 * koennte ein Bearbeiter, der erneut OSK_ERR_DENIED meldet, die
	 * Wartezeit beliebig verlaengern. */
	g.state = ST_IDLE;
	run_handler(slot, g.pend_payload);
	osk_wipe(g.pend_payload, sizeof(g.pend_payload));
}

void yk_hid_counts(yk_hid_counts_t *out)
{
	if (out == NULL)
		return;
	*out = g.counts;
}

/* STAPEL UND SPEICHER - gezaehlt, nicht geschaetzt.
 *
 * Die ZAHLEN stehen hier absichtlich NICHT. Sie werden von
 * test_ykhid.c gedruckt (Pruefsatz "Protokollschicht", Abschnitt
 * Speicherbedarf), weil sie von sizeof und damit von Uebersetzer und
 * Ausrichtung abhaengen. Eine Abschrift hier waere eine zweite Stelle
 * fuer dieselbe Zahl und wuerde still veralten - genau das ist schon
 * passiert: ein erster Entwurf dieses Kommentars nannte 232 Byte fuer
 * yk_frame_tx_t, gemessen wurden 240 (Auffuellung).
 *
 * Was hier steht, ist die Aussage, die NICHT von sizeof abhaengt:
 * der ganze Zustand (Empfangsrahmen, Sendepuffer, liegengebliebener
 * Befehl, Antwortpuffer) liegt in struct g und damit im BSS. KEIN Byte
 * davon steht auf dem Stapel - das ist der Unterschied zur
 * SE050-Schicht, wo 1,8 kB auf dem Stapel lagen und ein Zephyr-Faden
 * nur 1 bis 2 kB hat.
 *
 * Auf dem Stapel liegt je Aufruf: der 8-Byte-Bericht des Aufrufers (den
 * er selbst stellt), ein paar Zeiger und Zaehler. Tiefster Pfad ist
 * yk_hid_set_report -> run_handler -> Bearbeiter; die ersten beiden
 * brauchen zusammen keine 100 Byte. Was der Bearbeiter braucht, zaehlt
 * der, dem er gehoert.
 *
 * Gemessen wurde das Ergebnis von sizeof, nicht die wirkliche
 * Stapeltiefe: dafuer braeuchte es das Geraet. Stufe 2 von 4. */
