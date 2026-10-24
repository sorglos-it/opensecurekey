/* Zustandsbild und Hinweis zur Benutzerpruefung - siehe osk_views.h.
 *
 * Der Zustand steht gross in der Mitte, damit er auch aus der Hand heraus
 * lesbar ist. Die LED sagt dasselbe noch einmal in Farbe - als Ergaenzung,
 * nicht als Ersatz: wer Farben schlecht unterscheidet, liest den Text. */

#include <string.h>
#include "osk_lang.h"
#include "osk_text.h"
#include "osk_views.h"
#include "view_layout.h"

/* Text mittig im Bild, umbrochen, in grosser Schrift. */
static void middle(osk_gfx_t *fb, const char *text, uint8_t max_lines)
{
	osk_text_line_t lines[VIEW_TITLE_LINES + 2];
	size_t next = 0;
	size_t count;
	int16_t y;

	if (max_lines > sizeof(lines) / sizeof(lines[0])) {
		max_lines = (uint8_t)(sizeof(lines) / sizeof(lines[0]));
	}
	count = osk_text_wrap(text, 0, VIEW_TITLE_COLUMNS, lines, max_lines, &next);
	y = (int16_t)((OSK_DISPLAY_HEIGHT - (int16_t)count * osk_font_big.line) / 2);
	for (size_t i = 0; i < count; i++) {
		(void)osk_text_draw_center(fb, &osk_font_big, text + lines[i].start, lines[i].len,
					   0, y, OSK_DISPLAY_WIDTH);
		y = (int16_t)(y + osk_font_big.line);
	}
	/* Bleibt Text uebrig, geht er in kleiner Schrift darunter weiter -
	 * abgeschnitten wird auch hier nicht. */
	if (next < strlen(text)) {
		osk_text_line_t rest[4];
		size_t rest_next = next;
		size_t rest_count =
		    osk_text_wrap(text, next, VIEW_BODY_COLUMNS, rest, 4, &rest_next);

		for (size_t i = 0; i < rest_count; i++) {
			(void)osk_text_draw_center(fb, &osk_font_small, text + rest[i].start,
						   rest[i].len, 0, y, OSK_DISPLAY_WIDTH);
			y = (int16_t)(y + osk_font_small.line);
		}
	}
}

void osk_view_status(osk_gfx_t *fb, const char *text_key)
{
	osk_gfx_clear(fb);
	middle(fb, osk_lang_text(text_key), 3);
}

void osk_view_verify(osk_gfx_t *fb)
{
	int16_t top;
	const char *hint = osk_lang_text("verify.hint");
	osk_text_line_t lines[OSK_TEXT_MAX_LINES];
	size_t next = 0;
	size_t count;
	int16_t y;

	osk_gfx_clear(fb);
	top = view_title(fb, osk_lang_text("verify.title"));
	y = top;
	count =
	    osk_text_wrap(hint, 0, VIEW_BODY_COLUMNS, lines, (size_t)view_body_rows(top, 0), &next);
	for (size_t i = 0; i < count; i++) {
		(void)osk_text_draw(fb, &osk_font_small, hint + lines[i].start, lines[i].len, 0, y);
		y = (int16_t)(y + osk_font_small.line);
	}
}
