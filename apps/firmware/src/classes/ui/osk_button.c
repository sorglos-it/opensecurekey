/* Taster entprellen und deuten - siehe osk_button.h. */

#include "osk_button.h"

/* Differenz zweier Uhrzeiten. Der Ueberlauf nach 49 Tagen faellt heraus,
 * weil in uint32_t gerechnet wird. */
static uint32_t since(uint32_t now, uint32_t then)
{
	return now - then;
}

void osk_button_init(osk_button_t *btn, uint32_t now_ms)
{
	if (btn == NULL) {
		return;
	}
	btn->stable = false;
	btn->raw = false;
	btn->raw_since = now_ms;
	btn->down_at = now_ms;
	btn->waiting = false;
	btn->waiting_since = now_ms;
	btn->long_sent = false;
}

osk_btn_event_t osk_button_feed(osk_button_t *btn, bool pressed, uint32_t now_ms)
{
	if (btn == NULL) {
		return OSK_BTN_NONE;
	}
	if (pressed != btn->raw) {
		btn->raw = pressed;
		btn->raw_since = now_ms;
	}
	if (btn->raw != btn->stable && since(now_ms, btn->raw_since) >= OSK_BTN_DEBOUNCE_MS) {
		btn->stable = btn->raw;
		if (btn->stable) {
			btn->down_at = now_ms;
			btn->long_sent = false;
		} else if (!btn->long_sent) {
			/* losgelassen, bevor es ein langer Druck war */
			if (btn->waiting) {
				btn->waiting = false;
				return OSK_BTN_DOUBLE;
			}
			btn->waiting = true;
			btn->waiting_since = now_ms;
		}
	}
	if (btn->stable && !btn->long_sent && since(now_ms, btn->down_at) >= OSK_BTN_LONG_MS) {
		btn->long_sent = true;
		btn->waiting = false; /* der lange Druck gilt, nicht der kurze davor */
		return OSK_BTN_LONG;
	}
	if (btn->waiting && since(now_ms, btn->waiting_since) >= OSK_BTN_DOUBLE_MS) {
		btn->waiting = false;
		return OSK_BTN_SHORT;
	}
	return OSK_BTN_NONE;
}
