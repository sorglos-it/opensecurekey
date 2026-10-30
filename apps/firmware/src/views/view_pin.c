/* Das Bild der PIN-Eingabe - siehe osk_views.h.
 *
 * NACHGEZAEHLT, nicht geschaetzt: die aktive Flaeche ist 10,54 x 14,70 mm auf
 * 48 x 64 Punkten, also 0,220 mm je Punkt in der Breite und 0,230 mm in der
 * Hoehe. Daraus:
 *   grosse Schrift  6 x 10 Punkte  = 1,32 x 2,30 mm je Platz
 *   kleine Schrift  4 x  8 Punkte  = 0,88 x 1,84 mm je Platz
 * Die Ziffer, die gerade gewaehlt wird, steht in der grossen Schrift. Die
 * Punktreihe darunter sagt, an welcher Stelle der Nutzer ist: ein Kasten je
 * Stelle, 3 Punkte breit mit 1 Punkt Abstand - zwoelf Stellen brauchen
 * 12 * 4 - 1 = 47 von 48 Punkten. Mehr Stellen passen nicht, darum ist
 * OSK_PIN_MAX_LEN zwoelf. Dass es passt, prueft test_pin.c nach, Punkt fuer
 * Punkt.
 *
 * WAS NICHT AUF DEM GLAS STEHT: die schon genommenen Ziffern. Das Bild
 * bekommt sie nicht einmal uebergeben. Die Ziffer, die gerade gewaehlt wird,
 * steht dagegen da - ohne sie kann niemand ein Rad durchblaettern. Wer das
 * nicht will, baut mit -DOSK_PIN_HIDE_VALUE; dann steht ein Balken statt der
 * Ziffer, und die Eingabe ist blind. */

#include <stdio.h>
#include <string.h>
#include "osk_lang.h"
#include "osk_pin.h"
#include "osk_text.h"
#include "osk_views.h"
#include "view_layout.h"

/* Alle Masse an einer Stelle. */
#define PIN_VALUE_Y 14
#define PIN_MARK_Y 30
#define PIN_MARK_W 3
#define PIN_MARK_H 5
#define PIN_MARK_PITCH (PIN_MARK_W + 1)
#define PIN_HINT_Y 38

int16_t osk_view_pin_mark_x(uint8_t want)
{
	int16_t width = (int16_t)(want * PIN_MARK_PITCH - 1);

	if (width > OSK_DISPLAY_WIDTH) {
		width = OSK_DISPLAY_WIDTH;
	}
	return (int16_t)((OSK_DISPLAY_WIDTH - width) / 2);
}

bool osk_view_pin_shows_value(void)
{
#if defined(OSK_PIN_HIDE_VALUE)
	return false;
#else
	return true;
#endif
}

/* Die Reihe der Stellen: genommen = voller Kasten, gerade dran = leerer
 * Kasten, noch offen = ein Punkt in der Mitte. */
static void marks(osk_gfx_t *fb, uint8_t pos, uint8_t want)
{
	int16_t x = osk_view_pin_mark_x(want);

	for (uint8_t i = 0; i < want; i++) {
		int16_t left = (int16_t)(x + i * PIN_MARK_PITCH);

		if (i < pos) {
			osk_gfx_fill_rect(fb, left, PIN_MARK_Y, PIN_MARK_W, PIN_MARK_H, true);
		} else if (i == pos) {
			osk_gfx_rect(fb, left, PIN_MARK_Y, PIN_MARK_W, PIN_MARK_H, true);
		} else {
			osk_gfx_set(fb, (int16_t)(left + 1),
				    (int16_t)(PIN_MARK_Y + PIN_MARK_H / 2), true);
		}
	}
}

static const char *hint_key(osk_pin_scheme_t scheme)
{
	switch (scheme) {
	case OSK_PIN_SCHEME_BITS:
		return "pin.hint.bits";
	case OSK_PIN_SCHEME_TWO:
		return "pin.hint.two";
	case OSK_PIN_SCHEME_ONE:
	default:
		return "pin.hint.one";
	}
}

void osk_view_pin(osk_gfx_t *fb, uint8_t pos, uint8_t want, uint8_t value, uint8_t left,
		  osk_pin_scheme_t scheme)
{
	const char *hint;
	osk_text_line_t lines[3];
	size_t next = 0;
	size_t count;
	int16_t y;
	char number[8];

	if (fb == NULL || want == 0) {
		return;
	}
	if (want > OSK_PIN_MAX_LEN) {
		want = OSK_PIN_MAX_LEN;
	}
	if (pos > want) {
		pos = want;
	}
	osk_gfx_clear(fb);
	(void)view_title(fb, osk_lang_text("pin.title"));

	if (osk_view_pin_shows_value() && pos < want) {
		char digit[2];

		digit[0] = (char)('0' + (value % OSK_PIN_VALUES));
		digit[1] = '\0';
		(void)osk_text_draw_center(fb, &osk_font_big, digit, 1, 0, PIN_VALUE_Y,
					   OSK_DISPLAY_WIDTH);
	} else if (pos < want) {
		/* Blind: ein Balken sagt nur, dass eine Ziffer ansteht. */
		osk_gfx_fill_rect(fb, (int16_t)(OSK_DISPLAY_WIDTH / 2 - 3), (int16_t)(PIN_VALUE_Y + 4),
				  6, 2, true);
	}

	marks(fb, pos, want);

	hint = osk_lang_text(hint_key(scheme));
	count = osk_text_wrap(hint, 0, VIEW_BODY_COLUMNS, lines,
			      sizeof(lines) / sizeof(lines[0]), &next);
	y = PIN_HINT_Y;
	for (size_t i = 0; i < count; i++) {
		if (y + osk_font_small.rows > view_footer_y(0)) {
			break;
		}
		(void)osk_text_draw_center(fb, &osk_font_small, hint + lines[i].start, lines[i].len,
					   0, y, OSK_DISPLAY_WIDTH);
		y = (int16_t)(y + osk_font_small.line);
	}

	/* Fusszeile: welche Stelle, und wieviele Versuche der Chip noch
	 * hergibt. 0xFF heisst: der Chip hat es nicht gesagt. */
	(void)snprintf(number, sizeof(number), "%u/%u", (unsigned)(pos + 1u), (unsigned)want);
	if (left != 0xFFu) {
		char rest[8];

		(void)snprintf(rest, sizeof(rest), "%u", (unsigned)left);
		view_footer(fb, 0, number, rest);
	} else {
		view_footer(fb, 0, number, NULL);
	}
}

void osk_view_pin_result(osk_gfx_t *fb, osk_result_t result, bool submitted)
{
	const char *key;

	switch (result) {
	case OSK_OK:
		key = "verify.ok";
		break;
	case OSK_ERR_LOCKED:
		key = "pin.locked";
		break;
	case OSK_ERR_TIMEOUT:
		key = "error.timeout";
		break;
	case OSK_ERR_DENIED:
	default:
		/* Abgebrochen ist nicht falsch: nur was der Chip gesehen hat,
		 * zaehlt als Fehlversuch - und nur das steht hier. */
		key = submitted ? "pin.wrong" : "error.denied";
		break;
	}
	osk_view_status(fb, key);
}
