/* Zwei Auflagen, die andere Teile vom Bild brauchen - siehe osk_views.h.
 *
 * 1. LESBAR ODER ABLEHNEN (fuer die Geldboerse). Ein Betrag oder ein
 *    Empfaenger, den die Anzeige nicht Zeichen fuer Zeichen darstellen kann,
 *    wird NICHT gezeigt. Denn die Schrift macht aus allem Unbekannten ein
 *    Fragezeichen - und "0,5 ETH an 0x71C7?6?EC7" sieht aus wie eine Adresse,
 *    ist aber eine andere. Wer so etwas freigibt, gibt es blind frei. Darum
 *    sagt osk_view_value_ok() vorher nein, und der Aufrufer bricht ab.
 *
 * 2. MODUS VOR DEM SCHREIBEN (fuer den Datentraeger). Welcher Betriebsmodus
 *    gilt, steht auf dem Glas, bevor geschrieben wird - zusammen mit der
 *    Aussage, ob er geheim ist. Eine Verwechslung gibt Geheimnisse weg. */

#include <string.h>
#include "osk_font.h"
#include "osk_lang.h"
#include "osk_text.h"
#include "osk_views.h"
#include "view_layout.h"

bool osk_view_value_ok(const char *value)
{
	size_t len;
	size_t pos = 0;
	size_t question = osk_font_index('?');

	if (value == NULL || value[0] == '\0') {
		return false;
	}
	len = strlen(value);
	if (osk_text_chars(value, len) > OSK_VIEW_VALUE_MAX_CHARS) {
		return false;
	}
	while (pos < len) {
		uint32_t code = 0;

		pos += osk_utf8_next(value, len, pos, &code);
		/* Steuerzeichen kann man nicht sehen - ein Zeilenumbruch in
		 * einem Empfaengernamen waere unsichtbar und gefaehrlich. */
		if (code < 0x20u || code == 0x7Fu) {
			return false;
		}
		/* Alles, was die Schrift nicht kennt, wuerde ein Fragezeichen.
		 * Ein echtes Fragezeichen darf natuerlich vorkommen. */
		if (code != '?' && osk_font_index(code) == question) {
			return false;
		}
	}
	return true;
}

void osk_view_mode(osk_gfx_t *fb, const char *mode_key, const char *target, bool secret)
{
	int16_t top;
	const char *name;
	const char *note;
	osk_text_line_t lines[OSK_TEXT_MAX_LINES];
	size_t next = 0;
	size_t count;
	int16_t y;
	int16_t rows;

	if (fb == NULL) {
		return;
	}
	osk_gfx_clear(fb);
	top = view_title(fb, osk_lang_text("mode.title"));
	name = osk_lang_text((mode_key != NULL) ? mode_key : "mode.unknown");
	y = (int16_t)(top + 1);
	(void)osk_text_draw_center(fb, &osk_font_big, name, strlen(name), 0, y,
				   OSK_DISPLAY_WIDTH);
	y = (int16_t)(y + osk_font_big.line + 1);

	/* Wohin geschrieben wird - woertlich, und nur wenn es lesbar ist. */
	if (target != NULL && osk_view_value_ok(target)) {
		rows = view_body_rows(y, 2);
		count = osk_text_wrap(target, 0, VIEW_BODY_COLUMNS, lines,
				      (rows > 0) ? (size_t)rows : 0u, &next);
		for (size_t i = 0; i < count; i++) {
			(void)osk_text_draw(fb, &osk_font_small, target + lines[i].start,
					    lines[i].len, 0, y);
			y = (int16_t)(y + osk_font_small.line);
		}
	}

	/* Die Aussage, auf die es ankommt: geheim oder nicht. */
	note = osk_lang_text(secret ? "mode.secret" : "mode.open");
	view_footer(fb, 1, osk_lang_text("mode.before_write"), NULL);
	view_footer(fb, 0, note, NULL);
}
