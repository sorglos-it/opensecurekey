/* Gemeinsame Aufteilung aller Bilder: Titel oben, Inhalt in der Mitte,
 * Fusszeile unten. Nur Geometrie, keine Logik. */
#ifndef VIEW_LAYOUT_H
#define VIEW_LAYOUT_H

#include <stdint.h>
#include "osk_gfx.h"
#include "osk_text.h"

/* Zeichen je Zeile in der jeweiligen Schrift - haengt an der Panelbreite. */
#define VIEW_TITLE_COLUMNS (OSK_DISPLAY_WIDTH / 6)
#define VIEW_BODY_COLUMNS (OSK_DISPLAY_WIDTH / 4)

/* Hoechstens zwei Zeilen Titel; was laenger ist, wird umbrochen. */
#define VIEW_TITLE_LINES 2

/* Titel zeichnen (grosse Schrift, mittig, umbrochen) und den Trennstrich
 * darunter. Liefert die erste freie Bildzeile darunter. */
int16_t view_title(osk_gfx_t *fb, const char *text);

/* Eine Fusszeile von unten gezaehlt: 0 ist die unterste. */
int16_t view_footer_y(uint8_t from_bottom);

/* Fusszeile links, zusaetzlich etwas rechtsbuendig (Seitenzahl). */
void view_footer(osk_gfx_t *fb, uint8_t from_bottom, const char *left, const char *right);

/* Erste und letzte Bildzeile des Inhalts bei `footer_lines` Fusszeilen. */
int16_t view_body_rows(int16_t body_top, uint8_t footer_lines);

#endif
