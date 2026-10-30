/* Was die Anzeige vom Geraet braucht - und sonst nichts.
 *
 * osk_ui.h ist der Vertrag nach aussen (die Applets fragen dort). Diese Datei
 * ist der Vertrag nach unten: Anzeige, Taster, Uhr, LED. Alles vier sind
 * Griffe, die der Start der Firmware einhaengt - im Pruefsatz haengt dort eine
 * Attrappe, darum laeuft die ganze Freigabe am PC. */
#ifndef OSK_UI_PORT_H
#define OSK_UI_PORT_H

#include <stdbool.h>
#include <stdint.h>
#include "osk_display.h"
#include "osk_pin.h"
#include "osk_result.h"

/* Die LED sagt dasselbe wie die Anzeige noch einmal in Farbe - als
 * Ergaenzung, nie als Ersatz. */
typedef enum {
	OSK_LED_OFF = 0,
	OSK_LED_GREEN, /* offen, fertig */
	OSK_LED_BLUE,  /* wartet auf den Nutzer */
	OSK_LED_AMBER, /* arbeitet */
	OSK_LED_RED,   /* gesperrt, Fehler */
} osk_led_t;

typedef struct {
	osk_display_bus_t display;
	/* true, solange der Taster gedrueckt ist (schon gegen Masse gelesen). */
	bool (*button_down)(void *ctx);
	/* Ein zweiter Taster, falls die Platine einen bekommt. Darf NULL sein -
	 * dann laeuft alles mit dem einen Taster, und ein Weg, der zwei
	 * braucht, wird mit OSK_ERR_UNSUPPORTED abgelehnt statt still falsch
	 * bedient. Die Tasterzahl ist eine Hardwarefrage; der Code haengt an
	 * diesem einen Griff und nicht an sechs Stellen. */
	bool (*button2_down)(void *ctx);
	uint32_t (*now_ms)(void *ctx);
	void (*led)(void *ctx, osk_led_t color);
	/* Einmal je Durchlauf: schlafen bis zum naechsten Ereignis. Darf NULL
	 * sein, dann wird gepollt. */
	void (*idle)(void *ctx);
	/* Wurde der Nutzer in dieser Sitzung schon geprueft (PIN am Rechner)? */
	bool (*user_verified)(void *ctx);
	void *ctx;
} osk_ui_port_t;

/* Haengt die Griffe ein und startet die Anzeige. Ohne diesen Aufruf gibt
 * osk_ui_confirm() nie eine Freigabe - es gibt dann nichts, worauf der
 * Nutzer lesen koennte, was er freigibt. */
osk_result_t osk_ui_begin(const osk_ui_port_t *port);

void osk_ui_end(void);

/* Wartezeit, wenn der Anrufer keine nennt. */
#define OSK_UI_TIMEOUT_DEFAULT_MS 30000u

/* Fuer die PIN mehr Zeit: mit einem Taster dauert sie laenger als eine
 * Freigabe - wieviel genau, rechnet osk_pin_cost(). */
#define OSK_UI_PIN_TIMEOUT_MS 120000u

/* Stellen, die der Chip erwartet. */
#define OSK_UI_PIN_LEN 6

/* Welche Farbe gehoert zu welchem Zustand? */
osk_led_t osk_ui_led_for(const char *status_key);

/* Am Geraet: Griffe aus der Geraetebeschreibung einhaengen und starten
 * (osk_ui_port_nrf.c). Diese Datei ist nie uebersetzt worden - es gibt noch
 * kein Geraet. */
osk_result_t osk_ui_port_nrf_start(bool (*user_verified)(void *ctx));

/* --- PIN am Geraet --------------------------------------------------------- */

/* Haengt den Sicherheitschip ein, der die PIN prueft und die Fehlversuche
 * zaehlt. Ohne ihn gibt es am Geraet keine PIN-Eingabe - und schon gar keinen
 * Ersatzweg, der am Zaehler vorbeifuehrt.
 *
 * Der Zeiger muss die Anzeige ueberleben; kopiert wird nur er, nicht die
 * Griffe dahinter. NULL haengt ihn wieder aus. */
void osk_ui_set_pin_sink(const osk_pin_sink_t *sink);

/* Fragt die PIN am Geraet ab (vertrauenswuerdiger Kanal: die Ziffern sehen
 * weder USB noch der Rechner). `want` ist die Zahl der Stellen, `scheme` der
 * Bedienweg - OSK_PIN_SCHEME_DEFAULT, solange die Tasterzahl offen ist.
 *
 * OSK_ERR_UNSUPPORTED: der Weg braucht mehr Taster, als eingehaengt sind.
 * OSK_ERR_INTERNAL: keine Anzeige - dann wird nichts abgefragt, denn der
 * Nutzer koennte nicht sehen, an welcher Stelle er ist. */
osk_result_t osk_ui_pin(osk_pin_scheme_t scheme, uint8_t want, uint32_t timeout_ms);

/* Nur fuer den Pruefsatz: das letzte gezeichnete Bild. */
const osk_gfx_t *osk_ui_frame(void);

#endif
