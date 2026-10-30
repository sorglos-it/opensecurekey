/* Die PIN am Geraet - siehe osk_pin.h.
 *
 * Diese Datei kennt keinen Taster und keine Anzeige. Sie kennt Handlungen,
 * Ziffern und einen Chip, der die Ziffern haben will. Darum laeuft sie im
 * Pruefsatz am PC genauso wie am Geraet. */

#include <string.h>
#include "osk_pin.h"

/* Ueberschreiben, das der Uebersetzer nicht wegwerfen darf. */
static void scrub(volatile uint8_t *bytes, size_t len)
{
	while (len-- > 0) {
		*bytes++ = 0;
	}
}

uint8_t osk_pin_buttons(osk_pin_scheme_t scheme)
{
	return (scheme == OSK_PIN_SCHEME_TWO) ? 2u : 1u;
}

osk_pin_act_t osk_pin_map(osk_pin_scheme_t scheme, uint8_t button, osk_btn_event_t event)
{
	if (button >= osk_pin_buttons(scheme)) {
		return OSK_PIN_ACT_NONE;
	}
	switch (scheme) {
	case OSK_PIN_SCHEME_ONE:
		if (event == OSK_BTN_SHORT) {
			return OSK_PIN_ACT_NEXT;
		}
		if (event == OSK_BTN_LONG) {
			return OSK_PIN_ACT_TAKE;
		}
		if (event == OSK_BTN_DOUBLE) {
			return OSK_PIN_ACT_BACK;
		}
		return OSK_PIN_ACT_NONE;
	case OSK_PIN_SCHEME_BITS:
		if (event == OSK_BTN_SHORT) {
			return OSK_PIN_ACT_BIT0;
		}
		if (event == OSK_BTN_LONG) {
			return OSK_PIN_ACT_BIT1;
		}
		if (event == OSK_BTN_DOUBLE) {
			return OSK_PIN_ACT_BACK;
		}
		return OSK_PIN_ACT_NONE;
	case OSK_PIN_SCHEME_TWO:
	default:
		if (button == 0) {
			/* Der Blaettertaster tut nur eines. Ein langer Druck
			 * darauf ist ein Versehen und bleibt folgenlos. */
			return (event == OSK_BTN_SHORT) ? OSK_PIN_ACT_NEXT : OSK_PIN_ACT_NONE;
		}
		if (event == OSK_BTN_SHORT) {
			return OSK_PIN_ACT_TAKE;
		}
		if (event == OSK_BTN_LONG) {
			return OSK_PIN_ACT_BACK;
		}
		if (event == OSK_BTN_DOUBLE) {
			return OSK_PIN_ACT_CANCEL;
		}
		return OSK_PIN_ACT_NONE;
	}
}

osk_result_t osk_pin_begin(osk_pin_entry_t *entry, uint8_t want)
{
	if (entry == NULL) {
		return OSK_ERR_ARG;
	}
	memset(entry, 0, sizeof(*entry));
	if (want < OSK_PIN_MIN_LEN || want > OSK_PIN_MAX_LEN) {
		return OSK_ERR_ARG;
	}
	entry->want = want;
	return OSK_OK;
}

void osk_pin_wipe(osk_pin_entry_t *entry)
{
	if (entry == NULL) {
		return;
	}
	scrub(entry->digits, sizeof(entry->digits));
	entry->len = 0;
	entry->value = 0;
	entry->bits = 0;
	entry->bit_len = 0;
}

/* Eine fertige Ziffer ablegen. */
static osk_pin_step_t take(osk_pin_entry_t *entry, uint8_t digit)
{
	if (entry->len >= entry->want) {
		return OSK_PIN_READY;
	}
	entry->digits[entry->len] = digit;
	entry->len++;
	entry->value = 0;
	entry->bits = 0;
	entry->bit_len = 0;
	return (entry->len >= entry->want) ? OSK_PIN_READY : OSK_PIN_GO_ON;
}

osk_pin_step_t osk_pin_apply(osk_pin_entry_t *entry, osk_pin_act_t act)
{
	if (entry == NULL || entry->want == 0) {
		return OSK_PIN_ABORT;
	}
	switch (act) {
	case OSK_PIN_ACT_NEXT:
		entry->value = (uint8_t)((entry->value + 1u) % OSK_PIN_VALUES);
		return OSK_PIN_GO_ON;
	case OSK_PIN_ACT_TAKE:
		return take(entry, entry->value);
	case OSK_PIN_ACT_BIT0:
	case OSK_PIN_ACT_BIT1:
		entry->bits = (uint8_t)(((unsigned)entry->bits << 1) |
					((act == OSK_PIN_ACT_BIT1) ? 1u : 0u));
		entry->bit_len++;
		if (entry->bit_len < OSK_PIN_BITS_PER_DIGIT) {
			return OSK_PIN_GO_ON;
		}
		if (entry->bits < OSK_PIN_VALUES) {
			return take(entry, entry->bits);
		}
		/* 10..15 ist keine Ziffer. Die Stelle faengt von vorn an -
		 * stillschweigend eine falsche Ziffer zu nehmen waere
		 * schlimmer, denn der Zaehler im Chip zahlt das. */
		entry->bits = 0;
		entry->bit_len = 0;
		return OSK_PIN_REJECT;
	case OSK_PIN_ACT_BACK:
		if (entry->bit_len > 0) {
			entry->bits = 0;
			entry->bit_len = 0;
			return OSK_PIN_GO_ON;
		}
		if (entry->len > 0) {
			entry->len--;
			entry->digits[entry->len] = 0;
			entry->value = 0;
			return OSK_PIN_GO_ON;
		}
		/* Zurueck von der ersten Stelle ist der Abbruch. */
		return OSK_PIN_ABORT;
	case OSK_PIN_ACT_CANCEL:
		return OSK_PIN_ABORT;
	case OSK_PIN_ACT_NONE:
	default:
		return OSK_PIN_GO_ON;
	}
}

/* --- der Vorgang ---------------------------------------------------------- */

static void draw_now(const osk_pin_io_t *io, const osk_pin_entry_t *entry, uint8_t left)
{
	if (io->draw != NULL) {
		io->draw(io->ctx, entry->len, entry->want, entry->value, left);
	}
}

static uint8_t attempts_left(const osk_pin_sink_t *sink, osk_result_t *result_out)
{
	uint8_t left = 0xFFu;

	*result_out = OSK_OK;
	if (sink->left != NULL) {
		uint8_t value = 0;
		osk_result_t result = sink->left(sink->ctx, &value);

		if (result != OSK_OK) {
			*result_out = result;
			return 0xFFu;
		}
		left = value;
		if (left == 0) {
			*result_out = OSK_ERR_LOCKED;
		}
	}
	return left;
}

osk_result_t osk_pin_ask(const osk_pin_sink_t *sink, const osk_pin_io_t *io,
			 osk_pin_scheme_t scheme, uint8_t want, uint32_t timeout_ms)
{
	osk_button_t buttons[OSK_PIN_BUTTONS_MAX];
	osk_pin_entry_t entry;
	uint8_t count = osk_pin_buttons(scheme);
	uint8_t left;
	uint32_t start;
	osk_result_t result;

	if (sink == NULL || sink->submit == NULL || io == NULL || io->button_down == NULL ||
	    io->now_ms == NULL || timeout_ms == 0) {
		return OSK_ERR_ARG;
	}
	result = osk_pin_begin(&entry, want);
	if (result != OSK_OK) {
		return result;
	}
	/* Erst fragen, ob der Chip ueberhaupt noch Versuche hergibt. Eine PIN
	 * in ein gesperrtes Geraet zu tippen bringt nichts und zeigt sie nur
	 * jemandem, der zuschaut. */
	left = attempts_left(sink, &result);
	if (result != OSK_OK) {
		if (io->report != NULL) {
			io->report(io->ctx, result, false, 0);
		}
		osk_pin_wipe(&entry);
		return result;
	}

	start = io->now_ms(io->ctx);
	for (uint8_t i = 0; i < count; i++) {
		osk_button_init(&buttons[i], start);
	}
	draw_now(io, &entry, left);

	for (;;) {
		uint32_t now = io->now_ms(io->ctx);
		osk_pin_act_t act = OSK_PIN_ACT_NONE;
		osk_pin_step_t step;

		if ((now - start) >= timeout_ms) {
			osk_pin_wipe(&entry);
			if (io->report != NULL) {
				io->report(io->ctx, OSK_ERR_TIMEOUT, false, left);
			}
			return OSK_ERR_TIMEOUT;
		}
		if (io->idle != NULL) {
			io->idle(io->ctx);
		}
		for (uint8_t i = 0; i < count; i++) {
			osk_btn_event_t event =
			    osk_button_feed(&buttons[i], io->button_down(io->ctx, i), now);
			osk_pin_act_t mapped = osk_pin_map(scheme, i, event);

			if (mapped != OSK_PIN_ACT_NONE && act == OSK_PIN_ACT_NONE) {
				act = mapped;
			}
		}
		if (act == OSK_PIN_ACT_NONE) {
			continue;
		}
		step = osk_pin_apply(&entry, act);
		if (step == OSK_PIN_ABORT) {
			/* Mitten in der Eingabe abgebrochen: nichts Halbes
			 * bleibt liegen, und der Chip hat nie etwas gesehen -
			 * der Zaehler steigt also auch nicht. */
			osk_pin_wipe(&entry);
			if (io->report != NULL) {
				io->report(io->ctx, OSK_ERR_DENIED, false, left);
			}
			return OSK_ERR_DENIED;
		}
		draw_now(io, &entry, left);
		if (step != OSK_PIN_READY) {
			continue;
		}
		/* Der einzige Weg zur Pruefung. Der Chip zaehlt mit. */
		result = sink->submit(sink->ctx, entry.digits, entry.len);
		osk_pin_wipe(&entry);
		if (result != OSK_OK) {
			osk_result_t ignored;

			left = attempts_left(sink, &ignored);
		}
		if (io->report != NULL) {
			io->report(io->ctx, result, true, left);
		}
		return result;
	}
}

/* --- die Rechnung --------------------------------------------------------- */

/* Ein kurzer Druck, der noch durch das Doppelklick-Fenster muss: das Ereignis
 * kommt erst, wenn kein zweiter Druck mehr folgen kann. Bei zwei Tastern ist
 * das nicht noetig, weil der zweite Taster die Absicht schon verraet - das
 * ist der groesste Posten in der ganzen Rechnung. */
static uint32_t short_ms(bool needs_double_window)
{
	uint32_t base = OSK_PIN_T_TAP_MS + OSK_PIN_T_RELEASE_MS;

	return needs_double_window ? (base + OSK_BTN_DOUBLE_MS) : base;
}

static uint32_t long_ms(void)
{
	/* Der lange Druck wird gemeldet, sobald die Zeit erreicht ist. */
	return OSK_BTN_LONG_MS + OSK_PIN_T_RELEASE_MS;
}

void osk_pin_cost(osk_pin_scheme_t scheme, uint8_t want, osk_pin_cost_t *out)
{
	uint32_t n = want;
	uint32_t s;

	if (out == NULL) {
		return;
	}
	memset(out, 0, sizeof(*out));
	out->buttons = osk_pin_buttons(scheme);
	switch (scheme) {
	case OSK_PIN_SCHEME_ONE:
		/* Je Stelle: bis zu neun Mal blaettern, dann lang nehmen.
		 * Im Mittel 4,5 Mal blaettern. */
		s = short_ms(true);
		out->presses_worst = (uint16_t)(n * (OSK_PIN_VALUES - 1 + 1));
		out->presses_mean = (uint16_t)((n * 11u + 1u) / 2u); /* 4,5 + 1 */
		out->ms_worst = n * (9u * s + long_ms());
		out->ms_mean = (n * 9u * s) / 2u + n * long_ms();
		break;
	case OSK_PIN_SCHEME_BITS:
		/* Vier Druecke je Stelle, kein Nehmen. Schlimmster Fall: alle
		 * vier lang. Im Mittel halb lang, halb kurz. */
		s = short_ms(true);
		out->presses_worst = (uint16_t)(n * OSK_PIN_BITS_PER_DIGIT);
		out->presses_mean = out->presses_worst;
		out->ms_worst = n * OSK_PIN_BITS_PER_DIGIT * long_ms();
		out->ms_mean = n * OSK_PIN_BITS_PER_DIGIT * (long_ms() + s) / 2u;
		break;
	case OSK_PIN_SCHEME_TWO:
	default:
		/* Blaettern und Nehmen sind beides kurze Druecke - und keiner
		 * muss auf das Doppelklick-Fenster warten. */
		s = short_ms(false);
		out->presses_worst = (uint16_t)(n * (OSK_PIN_VALUES - 1 + 1));
		out->presses_mean = (uint16_t)((n * 11u + 1u) / 2u);
		out->ms_worst = n * 10u * s;
		out->ms_mean = (n * 9u * s) / 2u + n * s;
		break;
	}
}
