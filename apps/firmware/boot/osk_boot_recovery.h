/* Notfallweg per Tastendruck beim Einstecken. */
#ifndef OSK_BOOT_RECOVERY_H
#define OSK_BOOT_RECOVERY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* So viele Messungen muessen uebereinstimmen. Weniger ist kein Ergebnis. */
#define OSK_BOOT_BUTTON_SAMPLES 5

/* Abstand zwischen zwei Messungen und Groesse des Fensters nach dem Start.
 * 5 Messungen im Abstand von 20 ms sind 80 ms - lange genug gegen
 * Stoerimpulse, kurz genug, dass niemand es als Verzoegerung merkt. */
#define OSK_BOOT_BUTTON_STEP_MS 20
#define OSK_BOOT_BUTTON_WINDOW_MS 500

/* true nur, wenn alle Messungen "gedrueckt" sagen. */
bool osk_boot_button_held(const uint8_t *samples, size_t count);

/* Liegt der Zeitpunkt noch im Fenster direkt nach dem Start? */
bool osk_boot_in_window(uint32_t ms_since_boot);

#endif
