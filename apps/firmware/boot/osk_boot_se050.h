/* Zugang zum Rueckschritt-Zaehler im Sicherheitschip.
 * Diese zwei Funktionen werden in osk_boot_backend_t eingehaengt. */
#ifndef OSK_BOOT_SE050_H
#define OSK_BOOT_SE050_H

#include <stdint.h>
#include "osk_result.h"

/* OSK_ERR_UNSUPPORTED, solange osk_se050.h die Zaehlerfunktionen nicht hat.
 * Dann schlaegt jede Pruefung eines Pakets fehl, statt stillschweigend
 * ohne Rueckschritt-Sperre zu laufen. */
osk_result_t osk_boot_counter_read(uint32_t *out);
osk_result_t osk_boot_counter_raise(uint32_t value);

#endif
