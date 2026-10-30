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
#include "osk_pin.h"
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

/* Benutzerpruefung: der Hinweis, die PIN am Rechner einzugeben.
 * Nur noch fuer den Fall, dass am Geraet keine Eingabe moeglich ist. */
void osk_view_verify(osk_gfx_t *fb);

/* --- PIN am Geraet --------------------------------------------------------- */

/* Der Stand der Eingabe: Stelle `pos` von `want`, `value` ist die Ziffer, die
 * gerade gewaehlt wird, `left` die verbleibenden Versuche (0xFF = unbekannt).
 * Die schon genommenen Ziffern bekommt dieses Bild NICHT - es kann sie also
 * auch nicht zeigen. */
void osk_view_pin(osk_gfx_t *fb, uint8_t pos, uint8_t want, uint8_t value, uint8_t left,
		  osk_pin_scheme_t scheme);

/* Was der Chip gesagt hat. `submitted` = der Chip hat die Ziffern gesehen. */
void osk_view_pin_result(osk_gfx_t *fb, osk_result_t result, bool submitted);

/* Links die Punktreihe der Stellen beginnt hier - zum Nachzaehlen. */
int16_t osk_view_pin_mark_x(uint8_t want);

/* Steht die Ziffer, die gewaehlt wird, auf dem Glas? (-DOSK_PIN_HIDE_VALUE) */
bool osk_view_pin_shows_value(void);

/* --- Auflagen fuer andere Teile -------------------------------------------- */

/* Mehr Zeichen zeigt die Anzeige nicht an einem Stueck; eine Bitcoin-Adresse
 * (62) und eine IBAN (34) passen. */
#define OSK_VIEW_VALUE_MAX_CHARS 96

/* Kann die Anzeige diesen Wert Zeichen fuer Zeichen zeigen? Nein heisst:
 * NICHT anzeigen und den Vorgang ablehnen - ein halb gezeigter Empfaenger ist
 * schlimmer als keiner. */
bool osk_view_value_ok(const char *value);

/* Der Betriebsmodus, BEVOR geschrieben wird: Name gross, Ziel woertlich,
 * unten die Aussage geheim/nicht geheim. */
void osk_view_mode(osk_gfx_t *fb, const char *mode_key, const char *target, bool secret);

#endif
