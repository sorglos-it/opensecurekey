/* Profile und Kennzeichen - was osk_store.h offen laesst.
 *
 * Jedes Profil hat seine eigene PIN. Die PIN wird im SE050 geprueft (dort
 * liegt auch der Fehlversuchszaehler), und zusaetzlich geht ihr Abdruck in die
 * Schluesselableitung des Profils ein. Darum kann die PIN eines Profils die
 * Daten eines anderen nicht aufschliessen: der Schluessel waere falsch, und
 * der Pruefwert der Verschluesselung schlaegt an.
 *
 * Umsetzung: src/classes/store/store_items.c */
#ifndef OSK_STORE_PROFILES_H
#define OSK_STORE_PROFILES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"
#include "osk_store.h"

#define OSK_PIN_MIN   6    /* Zeichen, nicht Ziffern */
#define OSK_PIN_MAX  64

/* Richtet ein Profil ein. Scheitert, wenn es schon eingerichtet ist. */
osk_result_t osk_store_profile_create(uint8_t profile, const uint8_t *pin,
				      size_t pin_len);

/* Schliesst ein Profil auf. OSK_ERR_DENIED heisst: falsche PIN. */
osk_result_t osk_store_profile_unlock(uint8_t profile, const uint8_t *pin,
				      size_t pin_len);

void osk_store_profile_lock(uint8_t profile);
bool osk_store_profile_is_unlocked(uint8_t profile);
bool osk_store_profile_exists(uint8_t profile);

/* Kennzeichen eines Eintrags: gespiegelt, nur gekoppelte Rechner,
 * schreibgeschuetzt (OSK_FLAG_* in osk_vault_format.h). */
osk_result_t osk_store_item_flags(osk_item_id_t id, uint8_t *flags_out);
osk_result_t osk_store_item_set_readonly(osk_item_id_t id, bool on);

/* HOTP zaehlt bei jeder Benutzung weiter. Der Zaehler gehoert zum Eintrag
 * und wird wie alles andere atomar geschrieben. */
osk_result_t osk_store_item_counter(osk_item_id_t id, uint64_t *out);
osk_result_t osk_store_item_counter_next(osk_item_id_t id, uint64_t *out);

#endif
