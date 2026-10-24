/* Die beiden Schriften der Anzeige. Selbst gezeichnet, keine Fremdschrift.
 *
 * Aufbau: ein Byte je Bildzeile, Bit 0 ist die linke Spalte. Zeile 0 traegt
 * das Zeichen ueber dem Buchstaben (ä, é, ñ ...), die letzte Zeile die
 * Unterlaengen (g, j, p, q, y) und die Cedille unter dem c.
 *
 * Die Bitmuster stehen in osk_font_data.c und werden von generate.py
 * erzeugt - die Buchstaben mit Zeichen entstehen aus dem Grundbuchstaben.
 * Was die Schriften kosten, steht in docs/display.md. */
#ifndef OSK_FONT_H
#define OSK_FONT_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
	uint8_t width;	 /* Punkte breit */
	uint8_t rows;	 /* Punkte hoch, Zeile 0 eingeschlossen */
	uint8_t advance; /* Abstand bis zum naechsten Zeichen */
	uint8_t line;	 /* Abstand bis zur naechsten Textzeile */
	const uint8_t *glyphs;
} osk_font_t;

/* Gross: 8 Zeichen je Zeile auf 48 Punkten - fuer Titel und Beschriftung.
 * Klein: 12 Zeichen je Zeile - fuer das, was woertlich gezeigt wird. */
extern const osk_font_t osk_font_big;
extern const osk_font_t osk_font_small;

size_t osk_font_glyph_count(void);

/* Codepunkt -> Platz in der Tabelle. Unbekanntes wird zum Fragezeichen;
 * es entsteht nie eine Luecke, in der ein Zeichen verschwindet. */
size_t osk_font_index(uint32_t codepoint);

uint8_t osk_font_row(const osk_font_t *font, size_t index, uint8_t row);

#endif
