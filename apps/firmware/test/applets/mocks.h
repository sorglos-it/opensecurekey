/* Attrappen fuer SE050, Tresor, Anzeige, Transport und Uhr.
 *
 * Es ist nichts am Geraet pruefbar - die Platine ist nicht gebaut. Diese
 * Attrappen ersetzen alles, was die Kartenanwendungen nach aussen aufrufen.
 * Sie rechnen DETERMINISTISCH, aber sie rechnen keine echte Kurve: eine
 * Unterschrift hat hier die richtige Form und Laenge, nicht den richtigen
 * Wert. Geprueft wird der Aufbau der APDU und der Ablauf, nicht die
 * Mathematik - die macht spaeter der Chip.
 *
 * Wo eine Attrappe etwas anders macht als der echte Baustein, steht es
 * dabei.
 */
#ifndef MOCKS_H
#define MOCKS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"
#include "osk_store.h"

/* Alles auf Anfang: Chip leer, Tresor leer, PIN "123456", Anzeige sagt ja. */
void mock_reset_all(void);

/* ---- Anzeige ----------------------------------------------------------- */
void mock_ui_set_confirm(osk_result_t rc);
unsigned mock_ui_confirm_count(void);
const char *mock_ui_last_subject(void);
const char *mock_ui_last_detail(void);
int mock_ui_last_kind(void);

/* ---- SE050 ------------------------------------------------------------- */
void mock_se050_set_pin(const char *pin);
uint8_t mock_se050_tries(void);
bool mock_se050_has_key(uint32_t id);
/* Zufall vorhersagbar machen. */
void mock_se050_seed_random(uint32_t seed);

/* ---- Tresor ------------------------------------------------------------ */
size_t mock_store_count(void);
/* Wie oft ein Eintrag geoeffnet wurde - jedes Oeffnen fragt am Display. */
unsigned mock_store_reveal_count(void);
/* Einen Eintrag von Hand anlegen, ohne ueber die Anwendung zu gehen. */
osk_result_t mock_store_put(osk_item_type_t type, const char *tag, const uint8_t *secret,
			    size_t len);

/* ---- Uhr --------------------------------------------------------------- */
void mock_clock_set(uint64_t ms);

#endif
