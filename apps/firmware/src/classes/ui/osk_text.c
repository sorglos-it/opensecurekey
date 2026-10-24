/* Text lesen, umbrechen, zeichnen - siehe osk_text.h. */

#include <string.h>
#include "osk_text.h"

size_t osk_utf8_next(const char *text, size_t len, size_t pos, uint32_t *codepoint_out)
{
	uint8_t lead;
	size_t need;
	uint32_t value;

	if (codepoint_out != NULL) {
		*codepoint_out = 0xFFFDu;
	}
	if (text == NULL || pos >= len) {
		return 0;
	}
	lead = (uint8_t)text[pos];
	if (lead < 0x80u) {
		if (codepoint_out != NULL) {
			*codepoint_out = lead;
		}
		return 1;
	}
	if ((lead & 0xE0u) == 0xC0u) {
		need = 1;
		value = lead & 0x1Fu;
	} else if ((lead & 0xF0u) == 0xE0u) {
		need = 2;
		value = lead & 0x0Fu;
	} else if ((lead & 0xF8u) == 0xF0u) {
		need = 3;
		value = lead & 0x07u;
	} else {
		return 1; /* Folgebyte ohne Kopf: ein Byte weiter, U+FFFD */
	}
	if (pos + need >= len) {
		return 1; /* abgeschnittene Folge am Ende: ein Byte weiter, U+FFFD */
	}
	for (size_t i = 1; i <= need; i++) {
		uint8_t next = (uint8_t)text[pos + i];

		if ((next & 0xC0u) != 0x80u) {
			return 1;
		}
		value = (value << 6) | (next & 0x3Fu);
	}
	if (codepoint_out != NULL) {
		*codepoint_out = value;
	}
	return need + 1;
}

size_t osk_text_chars(const char *text, size_t len)
{
	size_t pos = 0;
	size_t count = 0;

	while (pos < len) {
		size_t step = osk_utf8_next(text, len, pos, NULL);

		if (step == 0) {
			break;
		}
		pos += step;
		count++;
	}
	return count;
}

/* Byteversatz nach `chars` Zeichen ab `from`. */
static size_t advance_chars(const char *text, size_t len, size_t from, size_t chars)
{
	size_t pos = from;

	for (size_t i = 0; i < chars && pos < len; i++) {
		size_t step = osk_utf8_next(text, len, pos, NULL);

		if (step == 0) {
			break;
		}
		pos += step;
	}
	return pos;
}

/* Eine Zeile abschneiden: liefert das Ende der Zeile und wo es weitergeht.
 * Am Leerzeichen wird getrennt, wenn eines im Fenster liegt; sonst hart. */
static void take_line(const char *text, size_t len, size_t from, uint8_t columns, size_t *end_out,
		      size_t *next_out)
{
	size_t limit = advance_chars(text, len, from, columns);
	size_t space = 0;
	size_t pos = from;

	if (limit >= len) {
		*end_out = len;
		*next_out = len;
		return;
	}
	/* Passt das naechste Zeichen hinter der Grenze noch zur Zeile? Nur wenn
	 * es ein Leerzeichen ist, endet die Zeile genau an der Grenze. */
	while (pos < limit) {
		size_t step = osk_utf8_next(text, len, pos, NULL);

		if (step == 0) {
			break;
		}
		if (text[pos] == ' ') {
			space = pos;
		}
		pos += step;
	}
	if (text[limit] == ' ') {
		*end_out = limit;
		*next_out = limit + 1;
		return;
	}
	if (space > from) {
		*end_out = space;
		*next_out = space + 1; /* das Leerzeichen selbst wird nicht gezeigt */
		return;
	}
	*end_out = limit; /* ein Wort, das allein zu lang ist: hart teilen */
	*next_out = limit;
}

size_t osk_text_wrap(const char *text, size_t from, uint8_t columns, osk_text_line_t *lines,
		     size_t max_lines, size_t *next_from)
{
	size_t len;
	size_t pos = from;
	size_t filled = 0;

	if (next_from != NULL) {
		*next_from = from;
	}
	if (text == NULL || lines == NULL || columns == 0 || max_lines == 0) {
		return 0;
	}
	len = strlen(text);
	if (from > len) {
		return 0;
	}
	while (pos < len && filled < max_lines) {
		size_t end;
		size_t next;

		take_line(text, len, pos, columns, &end, &next);
		lines[filled].start = (uint16_t)pos;
		lines[filled].len = (uint16_t)(end - pos);
		filled++;
		if (next <= pos) {
			break; /* kein Fortschritt - darf nicht vorkommen, nie haengen */
		}
		pos = next;
	}
	if (next_from != NULL) {
		*next_from = pos;
	}
	return filled;
}

size_t osk_text_pages(const char *text, uint8_t columns, size_t max_lines)
{
	osk_text_line_t lines[OSK_TEXT_MAX_LINES];
	size_t pos = 0;
	size_t pages = 0;
	size_t len = (text != NULL) ? strlen(text) : 0;

	if (max_lines > sizeof(lines) / sizeof(lines[0])) {
		max_lines = sizeof(lines) / sizeof(lines[0]);
	}
	do {
		size_t next = pos;

		(void)osk_text_wrap(text, pos, columns, lines, max_lines, &next);
		pages++;
		if (next <= pos) {
			break;
		}
		pos = next;
	} while (pos < len);
	return pages;
}

size_t osk_text_page_start(const char *text, uint8_t columns, size_t max_lines, size_t page)
{
	osk_text_line_t lines[OSK_TEXT_MAX_LINES];
	size_t pos = 0;
	size_t len = (text != NULL) ? strlen(text) : 0;

	if (max_lines > sizeof(lines) / sizeof(lines[0])) {
		max_lines = sizeof(lines) / sizeof(lines[0]);
	}
	for (size_t i = 0; i < page && pos < len; i++) {
		size_t next = pos;

		(void)osk_text_wrap(text, pos, columns, lines, max_lines, &next);
		if (next <= pos) {
			break;
		}
		pos = next;
	}
	return pos;
}

int16_t osk_text_width(const osk_font_t *font, const char *text, size_t len)
{
	if (font == NULL) {
		return 0;
	}
	return (int16_t)(osk_text_chars(text, len) * font->advance);
}

int16_t osk_text_draw(osk_gfx_t *fb, const osk_font_t *font, const char *text, size_t len,
		      int16_t x, int16_t y)
{
	size_t pos = 0;
	int16_t pen = x;

	if (font == NULL || text == NULL) {
		return 0;
	}
	while (pos < len) {
		uint32_t codepoint = 0;
		size_t step = osk_utf8_next(text, len, pos, &codepoint);

		if (step == 0) {
			break;
		}
		pen = (int16_t)(pen +
				osk_gfx_glyph(fb, font, osk_font_index(codepoint), pen, y, true));
		pos += step;
	}
	return (int16_t)(pen - x);
}

int16_t osk_text_draw_center(osk_gfx_t *fb, const osk_font_t *font, const char *text, size_t len,
			     int16_t x, int16_t y, int16_t width)
{
	int16_t used = osk_text_width(font, text, len);
	int16_t start = x;

	if (used < width) {
		start = (int16_t)(x + (width - used) / 2);
	}
	return osk_text_draw(fb, font, text, len, start, y);
}
