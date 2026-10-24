/* Der Rueckschritt-Zaehler im SE050.
 *
 * WARUM NICHT IM FLASH: Ein Zaehler im Flash laesst sich zuruecksetzen -
 * Flash loeschen, Zaehler auf 0, alte Firmware wieder aufspielbar. Wer das
 * Geraet in der Hand hat, kommt an den QSPI-Baustein heran. Der SE050 gibt
 * seinen Zaehler nicht her und laesst ihn nicht sinken; darum liegt er dort,
 * genau wie der Fehlversuchszaehler der PIN (siehe docs/threat-model.md).
 *
 * ZWEI FUNKTIONEN FEHLEN NOCH IN osk_se050.h. Die Datei gehoert dem Master;
 * gemeldet ist:
 *
 *     osk_result_t osk_se050_counter_read(osk_key_id_t id, uint32_t *out);
 *     osk_result_t osk_se050_counter_raise(osk_key_id_t id, uint32_t value);
 *
 * `raise` setzt den Zaehler auf `value`, wenn dieser hoeher ist als der
 * gespeicherte, und antwortet sonst OSK_ERR_EXISTS - absenken geht nie.
 * Der SE05x hat dafuer den Objekttyp "Counter" (AN12413, SetCounterValue /
 * IncCounterValue); die Pruefung "nur hoeher" muss die Umsetzung
 * sicherstellen, nicht der Aufrufer.
 *
 * Solange die zwei Funktionen fehlen, uebersetzt diese Datei einen
 * ehrlichen Platzhalter: er meldet OSK_ERR_UNSUPPORTED. Damit bleibt der
 * Baum baubar, und niemand haelt den Zaehler fuer eingebaut. Sobald die
 * Funktionen da sind, wird mit -DOSK_HAVE_SE050_COUNTER gebaut. */

#include "osk_boot.h"
#include "osk_boot_se050.h"

#ifdef OSK_HAVE_SE050_COUNTER
#include "osk_se050.h"

/* Kennung des Zaehlerobjekts im Chip. Fest, damit eine neue Firmware
 * denselben Zaehler findet - ein Update, das einen neuen anlegt, hebelt die
 * Sperre aus. */
#define OSK_BOOT_COUNTER_ID 0x4F534B01u

osk_result_t osk_boot_counter_read(uint32_t *out)
{
	if (out == NULL) {
		return OSK_ERR_ARG;
	}
	return osk_se050_counter_read(OSK_BOOT_COUNTER_ID, out);
}

osk_result_t osk_boot_counter_raise(uint32_t value)
{
	return osk_se050_counter_raise(OSK_BOOT_COUNTER_ID, value);
}

#else

osk_result_t osk_boot_counter_read(uint32_t *out)
{
	(void)out;
	return OSK_ERR_UNSUPPORTED;
}

osk_result_t osk_boot_counter_raise(uint32_t value)
{
	(void)value;
	return OSK_ERR_UNSUPPORTED;
}

#endif
