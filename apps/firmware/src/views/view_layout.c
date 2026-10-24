/* Aufteilung der Bilder - siehe view_layout.h. */

#include <string.h>
#include "view_layout.h"

int16_t view_title(osk_gfx_t *fb, const char *text)
{
	osk_text_line_t lines[VIEW_TITLE_LINES];
	size_t next = 0;
	size_t count;
	int16_t y = 0;

	if (text == NULL) {
		text = "";
	}
	count = osk_text_wrap(text, 0, VIEW_TITLE_COLUMNS, lines, VIEW_TITLE_LINES, &next);
	for (size_t i = 0; i < count; i++) {
		(void)osk_text_draw_center(fb, &osk_font_big, text + lines[i].start, lines[i].len,
					   0, y, OSK_DISPLAY_WIDTH);
		y = (int16_t)(y + osk_font_big.line);
	}
	if (count == 0) {
		y = osk_font_big.line;
	}
	osk_gfx_hline(fb, 0, y, OSK_DISPLAY_WIDTH, true);
	return (int16_t)(y + 2);
}

int16_t view_footer_y(uint8_t from_bottom)
{
	return (int16_t)(OSK_DISPLAY_HEIGHT - (int16_t)osk_font_small.line * (from_bottom + 1));
}

void view_footer(osk_gfx_t *fb, uint8_t from_bottom, const char *left, const char *right)
{
	int16_t y = view_footer_y(from_bottom);

	if (left != NULL) {
		(void)osk_text_draw(fb, &osk_font_small, left, strlen(left), 0, y);
	}
	if (right != NULL) {
		int16_t width = osk_text_width(&osk_font_small, right, strlen(right));

		(void)osk_text_draw(fb, &osk_font_small, right, strlen(right),
				    (int16_t)(OSK_DISPLAY_WIDTH - width), y);
	}
}

int16_t view_body_rows(int16_t body_top, uint8_t footer_lines)
{
	int16_t space =
	    (int16_t)(view_footer_y(footer_lines == 0 ? 0 : (uint8_t)(footer_lines - 1)) -
		      body_top - 1);

	if (footer_lines == 0) {
		space = (int16_t)(OSK_DISPLAY_HEIGHT - body_top);
	}
	if (space < 0) {
		space = 0;
	}
	return (int16_t)(space / osk_font_small.line);
}
