/* Handgriffe, die jeder Pruefsatz braucht. */
#ifndef TEST_UTIL_H
#define TEST_UTIL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_store.h"
#include "osk_result.h"

#define T_PIN "geheim1234"
#define T_PIN2 "anders5678"

/* Alle Attrappen in den Anfangszustand. */
void t_init(void);

/* Frisches Geraet: Bank waehlen, Flash leeren, Tresor oeffnen, Profil 0
 * anlegen und aufschliessen. dev unterscheidet die Geraete (SE050). */
void t_device(int bank, uint32_t dev, const char *pin);

/* Dasselbe ohne Loeschen: so als haette jemand den Stick neu eingesteckt.
 * Darf auch nach einem nachgestellten Stromausfall gerufen werden. */
void t_reboot(void);

/* Welche PIN und welches Geraet t_reboot benutzt. */
void t_use(uint32_t dev, const char *pin);

/* Einen Eintrag anlegen - mit Namen, Adresse und Geheimnis als Text. */
osk_result_t t_add(uint8_t profile, const char *name, const char *site, const char *secret,
		   bool mirrored, osk_item_id_t *id_out);

/* Geheimnis holen und mit dem Text vergleichen. Gibt true zurueck, wenn es
 * passt. Die Freigabe wird dabei bejaht. */
bool t_secret_is(osk_item_id_t id, const char *want);

/* Anzahl der Eintraege in einem Profil. */
size_t t_count(uint8_t profile);

/* Sucht einen Eintrag nach Namen; 0 wenn es ihn nicht gibt. */
osk_item_id_t t_find_name(uint8_t profile, const char *name);

/* Sucht einen Text in einem Puffer. true heisst: NICHT gefunden - so liest
 * sich die Pruefung "im Kryptotext steht kein Klartext" von selbst. */
bool memmem_missing(const uint8_t *hay, size_t hay_len, const char *needle);

#endif
