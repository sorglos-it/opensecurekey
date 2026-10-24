/* Text auf die Anzeige bringen: UTF-8 lesen, umbrechen, zeichnen.
 *
 * Die eine Regel, an der alles haengt: ein Text wird NIE stillschweigend
 * abgeschnitten. Passt er nicht in eine Zeile, wird er umbrochen; passen die
 * Zeilen nicht auf eine Seite, bekommt der Nutzer mehrere Seiten. Ein
 * abgeschnittener Empfaenger ist bei einer Ueberweisung gefaehrlicher als
 * gar keine Anzeige - siehe docs/display.md. */
#ifndef OSK_TEXT_H
#define OSK_TEXT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "osk_gfx.h"

/* Mehr Zeilen als auf die hoechste Anzeige passen, braucht keine Seite. */
#define OSK_TEXT_MAX_LINES 16

/* Ein Abschnitt des Textes: Byteversatz und Laenge in Bytes. */
typedef struct {
	uint16_t start;
	uint16_t len;
} osk_text_line_t;

/* Ein Zeichen aus UTF-8. Liefert die Zahl der verbrauchten Bytes (mindestens
 * 1, auch bei kaputten Folgen - dort kommt U+FFFD heraus, damit die Schleife
 * immer weiterlaeuft und nie haengt). */
size_t osk_utf8_next(const char *text, size_t len, size_t pos, uint32_t *codepoint_out);

/* Zahl der Zeichen (nicht Bytes) in einem UTF-8-Text. */
size_t osk_text_chars(const char *text, size_t len);

/* Bricht `text` ab Byte `from` in Zeilen von hoechstens `columns` Zeichen um.
 * Umbrochen wird am Leerzeichen; ein Wort, das allein zu lang ist (eine
 * Adresse, ein langer Dienstname), wird hart geteilt - verloren geht nichts.
 *
 * Rueckgabe: Zahl der gefuellten Zeilen. In *next_from steht der Byteversatz,
 * an dem die naechste Seite weitergeht; er ist gleich der Textlaenge, wenn
 * alles gezeigt wurde. */
size_t osk_text_wrap(const char *text, size_t from, uint8_t columns, osk_text_line_t *lines,
		     size_t max_lines, size_t *next_from);

/* Wieviele Seiten mit `max_lines` Zeilen braucht der ganze Text? Mindestens 1. */
size_t osk_text_pages(const char *text, uint8_t columns, size_t max_lines);

/* Byteversatz, an dem Seite `page` (ab 0) beginnt. */
size_t osk_text_page_start(const char *text, uint8_t columns, size_t max_lines, size_t page);

/* Zeichnet einen Abschnitt und liefert die gezeichnete Breite in Punkten. */
int16_t osk_text_draw(osk_gfx_t *fb, const osk_font_t *font, const char *text, size_t len,
		      int16_t x, int16_t y);

/* Zeichnet mittig in einer Flaeche von `width` Punkten Breite. */
int16_t osk_text_draw_center(osk_gfx_t *fb, const osk_font_t *font, const char *text, size_t len,
			     int16_t x, int16_t y, int16_t width);

int16_t osk_text_width(const osk_font_t *font, const char *text, size_t len);

#endif
