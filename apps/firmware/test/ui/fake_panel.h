/* Anzeige-Attrappe: nimmt die Bytes, die der Treiber sendet, baut daraus das
 * Bild zurueck und gibt es als Text aus. So ist im Pruefsatz sichtbar, was
 * auf dem Schirm STAENDE - am Geraet ist nie etwas gestanden.
 *
 * Dazu die Uhr und der Taster als Drehbuch: der Pruefsatz legt Druecke in die
 * Warteschlange, die Zeit laeuft nur, wenn die Warteschleife laeuft. */
#ifndef FAKE_PANEL_H
#define FAKE_PANEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "osk_button.h"
#include "osk_gfx.h"
#include "osk_ui_port.h"

/* Jeder Durchlauf der Warteschleife bewegt die Uhr um so viele Millisekunden. */
#define FAKE_PANEL_STEP_MS 5

void fake_panel_reset(void);

/* Die Griffe fuer osk_ui_begin(): Anzeige, Taster, Uhr, LED. */
const osk_ui_port_t *fake_panel_port(void);

/* Dieselben Griffe mit einem zweiten Taster. */
const osk_ui_port_t *fake_panel_port_two(void);

/* --- was der Treiber gesendet hat ------------------------------------------ */

size_t fake_panel_cmd_count(void);
uint8_t fake_panel_cmd(size_t index);
bool fake_panel_sent_cmd(uint8_t byte);
size_t fake_panel_flushes(void);
size_t fake_panel_data_bytes(void);
bool fake_panel_powered(void);
bool fake_panel_reset_done(void);
osk_led_t fake_panel_led(void);

/* Soll das Senden scheitern? Dann meldet der Bus OSK_ERR_IO. */
void fake_panel_fail_bus(bool fail);

/* Das Bild, wie es aus den gesendeten Bytes wieder zusammengesetzt wurde. */
const osk_gfx_t *fake_panel_image(void);
bool fake_panel_pixel(int16_t x, int16_t y);
size_t fake_panel_ink(void);

/* --- Uhr und Taster -------------------------------------------------------- */

void fake_panel_set_now(uint32_t ms);
uint32_t fake_panel_now(void);

/* Gilt der Nutzer als geprueft (PIN am Rechner)? */
void fake_panel_set_verified(bool verified);

/* Ein Abschnitt des Drehbuchs: Taster gedrueckt oder los, so lange. */
void fake_panel_push(bool down, uint32_t ms);

/* Kurzer Druck samt Pause danach - ergibt ein OSK_BTN_SHORT. */
void fake_panel_tap(uint32_t hold_ms);

/* Dasselbe fuer den zweiten Taster, eigenes Drehbuch, dieselbe Uhr. */
void fake_panel_push2(bool down, uint32_t ms);

/* --- Bild als Text --------------------------------------------------------- */

/* Gibt das Bild aus: '#' fuer einen Punkt, '.' fuer leer. */
void fake_panel_print(const char *title);

/* Steht `needle` irgendwo im Bild? Die Punkte werden gegen die Schrift
 * verglichen, Zeichen fuer Zeichen - das ist die Gegenprobe dazu, dass ein
 * Name wirklich lesbar auf dem Schirm steht und nicht nur im Speicher. */
bool fake_panel_shows(const char *needle);

/* Dasselbe nur in einer der beiden Schriften. */
bool fake_panel_shows_in(const osk_font_t *font, const char *needle);

#endif
