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

/* Welche Farbe gehoert zu welchem Zustand? */
osk_led_t osk_ui_led_for(const char *status_key);

/* Am Geraet: Griffe aus der Geraetebeschreibung einhaengen und starten
 * (osk_ui_port_nrf.c). Diese Datei ist nie uebersetzt worden - es gibt noch
 * kein Geraet. */
osk_result_t osk_ui_port_nrf_start(bool (*user_verified)(void *ctx));

/* Nur fuer den Pruefsatz: das letzte gezeichnete Bild. */
const osk_gfx_t *osk_ui_frame(void);

#endif
