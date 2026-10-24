/* Treiber der Anzeige: SSD1306-Verwandter am SPI.
 *
 * Der Treiber kennt den Bus nicht. Er bekommt vier Griffe (Befehl, Daten,
 * Reset, Strom) und rechnet mit nichts anderem - so laeuft derselbe Code am
 * Geraet und am PC gegen eine Attrappe, die die gesendeten Bytes mitschreibt.
 *
 * Die Groesse des Teils steht in osk_gfx.h und ist eine Konstante: 48 x 64
 * fuer das Zielteil CFAL4864A, 128 x 64 fuer das Modul vom Teststand
 * (-DOSK_PANEL_128X64). Der Spaltenversatz gehoert dazu - ohne ihn stehen
 * beim 48er alle Punkte 32 Spalten daneben. */
#ifndef OSK_DISPLAY_H
#define OSK_DISPLAY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "osk_gfx.h"
#include "osk_result.h"

typedef struct {
	/* Befehlsbytes senden (D/C = 0). */
	osk_result_t (*cmd)(void *ctx, const uint8_t *bytes, size_t len);
	/* Bildbytes senden (D/C = 1). */
	osk_result_t (*data)(void *ctx, const uint8_t *bytes, size_t len);
	/* Reset-Leitung legen: true = /RES auf low (Reset aktiv). */
	osk_result_t (*reset)(void *ctx, bool active);
	/* Lastschalter der Anzeige. */
	osk_result_t (*power)(void *ctx, bool on);
	void (*delay_ms)(void *ctx, uint32_t ms);
	void *ctx;
} osk_display_bus_t;

/* Strom an, Reset, Startfolge. Ohne Bus kommt OSK_ERR_ARG. */
osk_result_t osk_display_init(const osk_display_bus_t *bus);

/* Bild senden. Seitenweise, mit dem Spaltenversatz des Teils. */
osk_result_t osk_display_flush(const osk_gfx_t *fb);

/* Anzeige dunkel, Strom weg - gegen Einbrennen und fuer die Laufzeit. */
osk_result_t osk_display_sleep(void);

osk_result_t osk_display_contrast(uint8_t value);

/* Hell/dunkel vertauschen (fuer die Anzeige des gesperrten Zustands). */
osk_result_t osk_display_inverted(bool inverted);

bool osk_display_ready(void);

#endif
