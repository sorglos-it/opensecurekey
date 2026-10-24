/* Eine laufende Aufspielung (Befehl 13 des Protokolls).
 *
 * Getrennt von osk_boot.h, weil der Bootloader diese Schicht NICHT braucht:
 * er liest nur den Zustand und entscheidet. Geschrieben wird vom laufenden
 * Programm, nicht vom Bootloader. */
#ifndef OSK_BOOT_SESSION_H
#define OSK_BOOT_SESSION_H

#include "osk_boot.h"

typedef struct osk_boot_session {
	/* Vom Aufrufer zu setzen, bevor osk_boot_begin() laeuft: */
	void *ctx;	   /* wird den Flash-Funktionen durchgegeben */
	uint32_t slot_len; /* Groesse des zweiten Platzes im QSPI-Flash */
	osk_result_t (*erase)(void *ctx, uint32_t offset, uint32_t len);
	osk_result_t (*write)(void *ctx, uint32_t offset, const uint8_t *data, size_t len);

	/* Von osk_boot_begin() gesetzt - nicht von aussen anfassen: */
	const osk_boot_backend_t *be;
	osk_boot_header_t head;
	uint32_t written;
	bool open;
} osk_boot_session_t;

/* Kopf und Signatur pruefen, zweiten Platz leeren, Zustand auf WRITING.
 * Erst nach OSK_OK darf osk_boot_write() laufen. */
osk_result_t osk_boot_begin(const osk_boot_backend_t *be, osk_boot_slots_t *slots,
			    const uint8_t *raw, size_t raw_len, osk_boot_session_t *se);

/* Ein Stueck des Abbilds, streng der Reihe nach (offset == schon
 * geschriebene Byte). Loecher und Rueckspruenge werden abgelehnt. */
osk_result_t osk_boot_write(osk_boot_slots_t *slots, osk_boot_session_t *se, uint32_t offset,
			    const uint8_t *data, size_t len);

/* Vollstaendigkeit und Hash pruefen, dann READY. Stimmt der Hash nicht,
 * bleibt der Zustand WRITING - das Abbild wird nie gestartet. */
osk_result_t osk_boot_finish(osk_boot_slots_t *slots, osk_boot_session_t *se);

/* Abbrechen. Nach dem Umschalten (TESTING) wird das abgelehnt. */
osk_result_t osk_boot_abort(osk_boot_slots_t *slots, osk_boot_session_t *se);

#endif
