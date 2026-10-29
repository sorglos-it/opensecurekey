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
/* Den Chip bei der Ableitung scheitern lassen - so wird pruefbar, dass
 * die Seriennummer dann FEHLT und nicht erfunden wird. */
void mock_se050_set_hkdf_fail(bool fail);

/* ---- SE050: RSA -------------------------------------------------------- */

/* Welche Laengen der nachgestellte Chip koennen soll, als Bitmaske aus
 * (1 << osk_rsa_bits_t). 0 stellt den SE051A2 nach - den Baustein, der RSA
 * ueberhaupt nicht kann und auf derselben Adresse antwortet wie der C2.
 * Genau dieser Fall muss geprueft werden, und nur so ist er pruefbar. */
void mock_se050_set_rsa_mask(uint32_t mask);
bool mock_se050_has_rsa_key(uint32_t id);
uint16_t mock_se050_rsa_bits(uint32_t id);

/* Was beim letzten osk_se050_rsa_sign() oder _rsa_decrypt() wirklich
 * ankam. Darauf haengt die wichtigste Pruefung dieses Strangs: dass die
 * Auffuellung GENANNT wird und dass das Hashverfahren stimmt - das ist aus
 * der Laenge nicht ableitbar, SHA-512/256 und SHA-256 sind beide 32 Byte
 * lang. -1 heisst "noch kein Aufruf". */
int mock_rsa_last_pad(void);
int mock_rsa_last_hash(void);
const uint8_t *mock_rsa_last_hash_val(size_t *len_out);
unsigned mock_rsa_sign_count(void);
unsigned mock_rsa_decrypt_count(void);

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
