/* Zeichnen im Bildspeicher - siehe osk_gfx.h. */

#include <string.h>
#include "osk_gfx.h"

void osk_gfx_clear(osk_gfx_t *fb)
{
	if (fb != NULL) {
		memset(fb->pixels, 0, sizeof(fb->pixels));
	}
}

void osk_gfx_fill(osk_gfx_t *fb)
{
	if (fb != NULL) {
		memset(fb->pixels, 0xFF, sizeof(fb->pixels));
	}
}

static bool inside(int16_t x, int16_t y)
{
	return x >= 0 && y >= 0 && x < OSK_DISPLAY_WIDTH && y < OSK_DISPLAY_HEIGHT;
}

void osk_gfx_set(osk_gfx_t *fb, int16_t x, int16_t y, bool on)
{
	size_t offset;
	uint8_t bit;

	if (fb == NULL || !inside(x, y)) {
		return;
	}
	offset = (size_t)(y / 8) * OSK_DISPLAY_WIDTH + (size_t)x;
	bit = (uint8_t)(1u << (y % 8));
	if (on) {
		fb->pixels[offset] |= bit;
	} else {
		fb->pixels[offset] = (uint8_t)(fb->pixels[offset] & (uint8_t)~bit);
	}
}

bool osk_gfx_get(const osk_gfx_t *fb, int16_t x, int16_t y)
{
	size_t offset;

	if (fb == NULL || !inside(x, y)) {
		return false;
	}
	offset = (size_t)(y / 8) * OSK_DISPLAY_WIDTH + (size_t)x;
	return (fb->pixels[offset] & (uint8_t)(1u << (y % 8))) != 0;
}

void osk_gfx_hline(osk_gfx_t *fb, int16_t x, int16_t y, int16_t len, bool on)
{
	for (int16_t i = 0; i < len; i++) {
		osk_gfx_set(fb, (int16_t)(x + i), y, on);
	}
}

void osk_gfx_vline(osk_gfx_t *fb, int16_t x, int16_t y, int16_t len, bool on)
{
	for (int16_t i = 0; i < len; i++) {
		osk_gfx_set(fb, x, (int16_t)(y + i), on);
	}
}

void osk_gfx_rect(osk_gfx_t *fb, int16_t x, int16_t y, int16_t w, int16_t h, bool on)
{
	if (w <= 0 || h <= 0) {
		return;
	}
	osk_gfx_hline(fb, x, y, w, on);
	osk_gfx_hline(fb, x, (int16_t)(y + h - 1), w, on);
	osk_gfx_vline(fb, x, y, h, on);
	osk_gfx_vline(fb, (int16_t)(x + w - 1), y, h, on);
}

void osk_gfx_fill_rect(osk_gfx_t *fb, int16_t x, int16_t y, int16_t w, int16_t h, bool on)
{
	for (int16_t row = 0; row < h; row++) {
		osk_gfx_hline(fb, x, (int16_t)(y + row), w, on);
	}
}

void osk_gfx_invert_rect(osk_gfx_t *fb, int16_t x, int16_t y, int16_t w, int16_t h)
{
	for (int16_t row = 0; row < h; row++) {
		for (int16_t col = 0; col < w; col++) {
			int16_t px = (int16_t)(x + col);
			int16_t py = (int16_t)(y + row);

			osk_gfx_set(fb, px, py, !osk_gfx_get(fb, px, py));
		}
	}
}

uint8_t osk_gfx_glyph(osk_gfx_t *fb, const osk_font_t *font, size_t index, int16_t x, int16_t y,
		      bool on)
{
	if (font == NULL) {
		return 0;
	}
	for (uint8_t row = 0; row < font->rows; row++) {
		uint8_t bits = osk_font_row(font, index, row);

		for (uint8_t col = 0; col < font->width; col++) {
			if ((bits & (uint8_t)(1u << col)) != 0) {
				osk_gfx_set(fb, (int16_t)(x + col), (int16_t)(y + row), on);
			}
		}
	}
	return font->advance;
}
