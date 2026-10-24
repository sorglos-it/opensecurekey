/* Notfallweg: Bootloader-Betrieb, wenn beim Einstecken die Taste gedrueckt
 * ist. Damit bleibt ein Geraet erreichbar, dessen Firmware nicht mehr
 * antwortet - ohne Oeffnen und ohne Debugger.
 *
 * Hier steht nur die Entscheidung, nicht das Lesen des Anschlusses: das
 * gehoert in die Zephyr-Schicht und ist ohne Brett nicht pruefbar. Die
 * Entscheidung ist pruefbar, und sie ist der Teil, der schiefgehen kann.
 *
 * Zwei Fehler sollen ausgeschlossen sein:
 *  - Ein einzelner Stoerimpuls auf der Leitung darf nicht in den
 *    Bootloader fuehren. Also wird mehrfach gelesen, und ALLE Messungen
 *    muessen "gedrueckt" sagen.
 *  - Die Taste muss beim EINSTECKEN gedrueckt sein, nicht irgendwann
 *    danach. Sonst koennte ein Programm, das den Nutzer zum Druecken
 *    bringt, in den Bootloader schalten. Darum wird nur im Fenster
 *    direkt nach dem Start gelesen. */

#include "osk_boot.h"
#include "osk_boot_recovery.h"

bool osk_boot_button_held(const uint8_t *samples, size_t count)
{
	if (samples == NULL || count < OSK_BOOT_BUTTON_SAMPLES) {
		/* Zu wenig gemessen heisst "nicht gedrueckt". Im Zweifel
		 * startet die Firmware, nicht der Bootloader. */
		return false;
	}
	for (size_t i = 0; i < count; i++) {
		if (samples[i] == 0) {
			return false;
		}
	}
	return true;
}

bool osk_boot_in_window(uint32_t ms_since_boot)
{
	return ms_since_boot <= OSK_BOOT_BUTTON_WINDOW_MS;
}
