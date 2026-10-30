/* Freigabe am Geraet: fragen, warten, antworten - siehe osk_ui.h.
 *
 * Der Ablauf einer Frage:
 *   1. Leseseiten. Jede zeigt woertlich, worum es geht. Kurzer Druck
 *      blaettert weiter.
 *   2. Entscheidungsseite. Langer Druck = ja, kurzer Druck = nein,
 *      doppelter Druck = von vorn lesen.
 *   3. Beim Loeschen eine zweite Entscheidungsseite - zweimal lang.
 * Laeuft die Zeit ab, kommt OSK_ERR_TIMEOUT; will der Nutzer nicht,
 * OSK_ERR_DENIED. Ohne Anzeige gibt es keine Freigabe. */

#include <stdint.h>
#include <string.h>
#include "osk_button.h"
#include "osk_lang.h"
#include "osk_pin.h"
#include "osk_ui.h"
#include "osk_ui_port.h"
#include "osk_views.h"

static osk_ui_port_t g_port;
static bool g_ready;
static osk_gfx_t g_frame;
static const osk_pin_sink_t *g_pin_sink;

osk_result_t osk_ui_begin(const osk_ui_port_t *port)
{
	osk_result_t result;

	if (port == NULL || port->button_down == NULL || port->now_ms == NULL) {
		return OSK_ERR_ARG;
	}
	g_port = *port;
	result = osk_display_init(&g_port.display);
	if (result != OSK_OK) {
		g_ready = false;
		return result;
	}
	g_ready = true;
	osk_gfx_clear(&g_frame);
	return osk_display_flush(&g_frame);
}

void osk_ui_end(void)
{
	if (g_ready) {
		if (g_port.led != NULL) {
			g_port.led(g_port.ctx, OSK_LED_OFF);
		}
		(void)osk_display_sleep();
	}
	g_ready = false;
	g_pin_sink = NULL;
	memset(&g_port, 0, sizeof(g_port));
}

const osk_gfx_t *osk_ui_frame(void)
{
	return &g_frame;
}

static void show(void)
{
	(void)osk_display_flush(&g_frame);
}

static void set_led(osk_led_t color)
{
	if (g_port.led != NULL) {
		g_port.led(g_port.ctx, color);
	}
}

osk_led_t osk_ui_led_for(const char *status_key)
{
	if (status_key == NULL) {
		return OSK_LED_OFF;
	}
	if (strcmp(status_key, "status.unlocked") == 0) {
		return OSK_LED_GREEN;
	}
	if (strcmp(status_key, "status.locked") == 0 || strcmp(status_key, "status.error") == 0) {
		return OSK_LED_RED;
	}
	if (strcmp(status_key, "status.busy") == 0) {
		return OSK_LED_AMBER;
	}
	return OSK_LED_BLUE;
}

void osk_ui_status(const char *text_key)
{
	if (!g_ready) {
		return;
	}
	osk_view_status(&g_frame, text_key);
	show();
	set_led(osk_ui_led_for(text_key));
}

/* Ein Durchlauf der Warteschleife: Taster lesen, Zeit pruefen. */
static osk_btn_event_t wait_step(osk_button_t *btn, uint32_t start, uint32_t timeout, bool *expired)
{
	uint32_t now = g_port.now_ms(g_port.ctx);

	*expired = (now - start) >= timeout;
	if (g_port.idle != NULL) {
		g_port.idle(g_port.ctx);
	}
	return osk_button_feed(btn, g_port.button_down(g_port.ctx), now);
}

osk_result_t osk_ui_confirm(const osk_ask_req_t *req)
{
	osk_button_t btn;
	osk_view_pos_t pos = {0, 0, false};
	osk_view_pos_t next = pos;
	size_t pages;
	size_t page = 0;
	bool deciding = false;
	bool again = false;
	uint32_t start;
	uint32_t timeout;

	if (req == NULL) {
		return OSK_ERR_ARG;
	}
	/* Ohne Anzeige weiss der Nutzer nicht, was er freigibt. Dann gibt es
	 * keine Freigabe - auch keine stillschweigende. */
	if (!g_ready) {
		return OSK_ERR_INTERNAL;
	}
	start = g_port.now_ms(g_port.ctx);
	timeout = (req->timeout_ms != 0) ? req->timeout_ms : OSK_UI_TIMEOUT_DEFAULT_MS;
	osk_button_init(&btn, start);
	pages = osk_view_pages(req);
	set_led(OSK_LED_BLUE);
	osk_view_confirm(&g_frame, req, pos, &next, page, pages);
	show();

	for (;;) {
		bool expired = false;
		osk_btn_event_t event = wait_step(&btn, start, timeout, &expired);

		if (expired) {
			osk_ui_status("error.timeout");
			return OSK_ERR_TIMEOUT;
		}
		switch (event) {
		case OSK_BTN_SHORT:
			if (!deciding) {
				if (page + 1 < pages) {
					page++;
					pos = next;
					osk_view_confirm(&g_frame, req, pos, &next, page, pages);
				} else {
					deciding = true;
					osk_view_decide(&g_frame, req, false);
				}
				show();
				break;
			}
			osk_ui_status("error.denied");
			return OSK_ERR_DENIED;
		case OSK_BTN_LONG:
			if (!deciding) {
				deciding = true; /* erst die Entscheidungsseite sehen */
				osk_view_decide(&g_frame, req, false);
				show();
				break;
			}
			if (req->kind == OSK_ASK_WIPE && !again) {
				again = true; /* Loeschen fragt zweimal */
				osk_view_decide(&g_frame, req, true);
				show();
				break;
			}
			osk_ui_status("verify.ok");
			return OSK_OK;
		case OSK_BTN_DOUBLE:
			/* von vorn lesen */
			page = 0;
			pos.item = 0;
			pos.offset = 0;
			pos.label_done = false;
			deciding = false;
			again = false;
			osk_view_confirm(&g_frame, req, pos, &next, page, pages);
			show();
			break;
		case OSK_BTN_NONE:
		default:
			break;
		}
	}
}

/* --- PIN am Geraet --------------------------------------------------------- */

void osk_ui_set_pin_sink(const osk_pin_sink_t *sink)
{
	g_pin_sink = sink;
}

static bool pin_button(void *ctx, uint8_t button)
{
	(void)ctx;
	if (button == 0) {
		return g_port.button_down(g_port.ctx);
	}
	if (g_port.button2_down != NULL) {
		return g_port.button2_down(g_port.ctx);
	}
	return false;
}

static uint32_t pin_now(void *ctx)
{
	(void)ctx;
	return g_port.now_ms(g_port.ctx);
}

static void pin_idle(void *ctx)
{
	(void)ctx;
	if (g_port.idle != NULL) {
		g_port.idle(g_port.ctx);
	}
}

/* Der Stand der Eingabe aufs Glas. Die genommenen Ziffern kommen hier nicht
 * an - osk_pin_ask() gibt sie nicht heraus. */
static void pin_draw(void *ctx, uint8_t pos, uint8_t want, uint8_t value, uint8_t left)
{
	osk_pin_scheme_t scheme = (osk_pin_scheme_t)(uintptr_t)ctx;

	osk_view_pin(&g_frame, pos, want, value, left, scheme);
	show();
	set_led(OSK_LED_BLUE);
}

static void pin_report(void *ctx, osk_result_t result, bool submitted, uint8_t left)
{
	(void)ctx;
	(void)left;
	osk_view_pin_result(&g_frame, result, submitted);
	show();
	set_led((result == OSK_OK) ? OSK_LED_GREEN : OSK_LED_RED);
}

osk_result_t osk_ui_pin(osk_pin_scheme_t scheme, uint8_t want, uint32_t timeout_ms)
{
	osk_pin_io_t io = {pin_button, pin_now, pin_draw, pin_report, pin_idle, NULL};

	if (g_pin_sink == NULL) {
		/* Kein Chip, keine PIN. Es gibt hier absichtlich keinen
		 * Ersatzweg: ein Vergleich im Flash haette keinen Zaehler. */
		return OSK_ERR_UNSUPPORTED;
	}
	if (!g_ready) {
		return OSK_ERR_INTERNAL;
	}
	if (osk_pin_buttons(scheme) > 1 && g_port.button2_down == NULL) {
		return OSK_ERR_UNSUPPORTED;
	}
	io.ctx = (void *)(uintptr_t)scheme;
	if (timeout_ms == 0) {
		timeout_ms = OSK_UI_PIN_TIMEOUT_MS;
	}
	return osk_pin_ask(g_pin_sink, &io, scheme, want, timeout_ms);
}

osk_result_t osk_ui_verify_user(bool *verified_out)
{
	osk_button_t btn;
	uint32_t start;

	if (verified_out == NULL) {
		return OSK_ERR_ARG;
	}
	*verified_out = false;
	if (!g_ready) {
		return OSK_ERR_INTERNAL;
	}
	/* Ein Fingerabdrucksensor ist in Stufe 1 nicht bestueckt (Begruendung in
	 * docs/hardware.md). Geprueft wird mit der PIN am Rechner; die Anzeige
	 * sagt, dass das Geraet darauf wartet, und der Taster beweist, dass ein
	 * Mensch dabei ist. */
	if (g_port.user_verified != NULL && g_port.user_verified(g_port.ctx)) {
		*verified_out = true;
		osk_ui_status("verify.ok");
		return OSK_OK;
	}
	/* Ist ein Sicherheitschip eingehaengt, wird die PIN AM GERAET
	 * eingegeben - der Rechner sieht sie nie. Das ist der Weg, den FIPS
	 * 140-3 Stufe 3 verlangt. Nur ohne Chip bleibt der alte Hinweis. */
	if (g_pin_sink != NULL) {
		osk_result_t result = osk_ui_pin(OSK_PIN_SCHEME_DEFAULT, OSK_UI_PIN_LEN, 0);

		*verified_out = (result == OSK_OK);
		return result;
	}
	osk_view_verify(&g_frame);
	show();
	set_led(OSK_LED_BLUE);
	start = g_port.now_ms(g_port.ctx);
	osk_button_init(&btn, start);
	for (;;) {
		bool expired = false;
		osk_btn_event_t event = wait_step(&btn, start, OSK_UI_TIMEOUT_DEFAULT_MS, &expired);

		if (expired) {
			osk_ui_status("error.timeout");
			return OSK_ERR_TIMEOUT;
		}
		if (event == OSK_BTN_LONG) {
			if (g_port.user_verified != NULL && g_port.user_verified(g_port.ctx)) {
				*verified_out = true;
				osk_ui_status("verify.ok");
				return OSK_OK;
			}
			osk_ui_status("error.denied");
			return OSK_ERR_DENIED;
		}
		if (event == OSK_BTN_SHORT || event == OSK_BTN_DOUBLE) {
			osk_ui_status("error.denied");
			return OSK_ERR_DENIED;
		}
	}
}
