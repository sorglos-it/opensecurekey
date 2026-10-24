/* Bildspeicher und das Zeichnen darin: Punkte, Linien, Flaechen, Buchstaben.
 * Kein Fenstersystem, keine Fremdbibliothek - mehr braucht eine Anzeige mit
 * 48 x 64 Punkten nicht.
 *
 * Die Groesse ist eine Konstante, keine Annahme im Code: ohne Schalter gilt
 * das Zielteil (CFAL4864A, 48 x 64), mit -DOSK_PANEL_128X64 das 0,96-Zoll-Modul
 * vom Teststand. Jeder Zeichenaufruf schneidet am Rand ab, nie daneben. */
#ifndef OSK_GFX_H
#define OSK_GFX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "osk_font.h"

#if defined(OSK_PANEL_128X64)
#define OSK_DISPLAY_WIDTH 128
#define OSK_DISPLAY_HEIGHT 64
#define OSK_DISPLAY_COL_OFFSET 0
#define OSK_DISPLAY_NAME "128x64"
#else
/* CFAL4864A-071BW: 48 Spalten, mittig im 128-Spalten-Speicher des Controllers. */
#define OSK_DISPLAY_WIDTH 48
#define OSK_DISPLAY_HEIGHT 64
#define OSK_DISPLAY_COL_OFFSET 32
#define OSK_DISPLAY_NAME "48x64"
#endif

#define OSK_DISPLAY_PAGES (OSK_DISPLAY_HEIGHT / 8)
#define OSK_GFX_BYTES (OSK_DISPLAY_WIDTH * OSK_DISPLAY_PAGES)

/* Genau die Form, die der Controller erwartet: eine Seite sind acht
 * Bildzeilen, Bit 0 ist oben. Darum kostet das Senden kein Umsortieren. */
typedef struct {
	uint8_t pixels[OSK_GFX_BYTES];
} osk_gfx_t;

void osk_gfx_clear(osk_gfx_t *fb);
void osk_gfx_fill(osk_gfx_t *fb);
void osk_gfx_set(osk_gfx_t *fb, int16_t x, int16_t y, bool on);
bool osk_gfx_get(const osk_gfx_t *fb, int16_t x, int16_t y);
void osk_gfx_hline(osk_gfx_t *fb, int16_t x, int16_t y, int16_t len, bool on);
void osk_gfx_vline(osk_gfx_t *fb, int16_t x, int16_t y, int16_t len, bool on);
void osk_gfx_rect(osk_gfx_t *fb, int16_t x, int16_t y, int16_t w, int16_t h, bool on);
void osk_gfx_fill_rect(osk_gfx_t *fb, int16_t x, int16_t y, int16_t w, int16_t h, bool on);
void osk_gfx_invert_rect(osk_gfx_t *fb, int16_t x, int16_t y, int16_t w, int16_t h);

/* Ein Zeichen aus der Schrift. Liefert die Breite, um die weitergerueckt wird. */
uint8_t osk_gfx_glyph(osk_gfx_t *fb, const osk_font_t *font, size_t index, int16_t x, int16_t y,
		      bool on);

#endif
