/* Die Bilder der Anzeige. Hier steht nur, WO etwas hinkommt - was gefragt
 * wird, entscheidet osk_ui.c, und WIE es heisst, steht in den Sprachdateien.
 *
 * Auf 48 x 64 Punkten ist das Wichtigste die Aufteilung: oben der Anlass,
 * darunter woertlich, worum es geht, unten was der Taster tut. Passt der
 * woertliche Teil nicht, gibt es mehrere Seiten - abgeschnitten wird nie. */
#ifndef OSK_VIEWS_H
#define OSK_VIEWS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "osk_gfx.h"
#include "osk_ui.h"

/* Ein Stueck Inhalt: die Beschriftung kommt aus der Sprachdatei, der Wert
 * woertlich vom Gegenueber (Dienst, Empfaenger, Betrag). */
typedef struct {
	const char *label_key;
	const char *value;
} osk_view_item_t;

/* Wo das Blaettern steht: welches Stueck, welcher Byteversatz darin. */
typedef struct {
	uint8_t item;
	uint16_t offset;
	bool label_done;
} osk_view_pos_t;

#define OSK_VIEW_ITEMS_MAX 4

/* Fuellt die Stuecke zum Anlass: Anmeldung zeigt den Dienst, eine Signatur
 * Empfaenger und Betrag, das Loeschen seine Warnung. Liefert die Zahl. */
size_t osk_view_items(const osk_ask_req_t *req, osk_view_item_t *items, size_t max_items);

/* Der Schluessel des Titels zum Anlass ("ask.sign.title"). */
const char *osk_view_title_key(osk_ask_t kind);

/* Zeichnet eine Leseseite: Titel, Inhalt ab `pos`, Fusszeile mit Seitenzahl.
 * In *next steht, wo die naechste Seite weitergeht. */
void osk_view_confirm(osk_gfx_t *fb, const osk_ask_req_t *req, osk_view_pos_t pos,
		      osk_view_pos_t *next, size_t page, size_t pages);

/* Zeichnet die Entscheidungsseite: was passiert, und was der Taster tut.
 * `again` ist die zweite Frage beim Loeschen. */
void osk_view_decide(osk_gfx_t *fb, const osk_ask_req_t *req, bool again);

/* Wieviele Leseseiten braucht dieser Anlass? Mindestens 1. */
size_t osk_view_pages(const osk_ask_req_t *req);

/* Zustand des Geraets: gesperrt, offen, arbeitet, Fehler. */
void osk_view_status(osk_gfx_t *fb, const char *text_key);

/* Benutzerpruefung: der Hinweis, die PIN am Rechner einzugeben. */
void osk_view_verify(osk_gfx_t *fb);

#endif
