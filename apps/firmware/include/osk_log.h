/* Geraeteprotokoll: wann ging welcher Eintrag an welches Programm.
 * Ringpuffer im Flash, ohne Geheimnisse - nur Kennung, Zeit und Anlass.
 *
 * Umsetzung: src/classes/store/ */
#ifndef OSK_LOG_H
#define OSK_LOG_H

#include <stdint.h>
#include <stddef.h>
#include "osk_result.h"
#include "osk_transport.h"

typedef struct {
	uint64_t when_utc;
	osk_link_t link;
	uint16_t what;	  /* Anlass, siehe shared/protocol.md */
	uint32_t item_id; /* 0 = kein Eintrag beteiligt */
	char asked_by[32];
} osk_log_entry_t;

osk_result_t osk_log_add(const osk_log_entry_t *e);
osk_result_t osk_log_count(size_t *count_out);
osk_result_t osk_log_get(size_t index, osk_log_entry_t *out);
osk_result_t osk_log_clear(void);

/* Prueft die Hash-Kette des Protokolls.
 *
 * Jeder Satz traegt ein Kettenglied auf seinen Vorgaenger: die ersten 16 Byte
 * von dessen Pruefwert. Weil der Pruefwert am Protokollschluessel haengt,
 * kann niemand ohne diesen Schluessel eine Kette neu rechnen - und wer einen
 * einzelnen Satz herausnimmt, laesst das Glied des naechsten ins Leere
 * zeigen.
 *
 * Geprueft wird vom jueengsten Satz rueckwaerts. `checked_out` sagt, ueber
 * wie viele Uebergaenge die Kette gehalten hat; `break_at_out` nennt bei
 * OSK_ERR_FORMAT die Stelle (0 = jueengster Satz).
 *
 * Was NICHT geprueft werden kann: der aelteste noch vorhandene Satz hat
 * keinen Vorgaenger mehr - der Ring hat ihn ueberschrieben. Dort hoert die
 * Kette auf, und das ist das Wesen eines Ringpuffers, kein Befund. Ebenso
 * koennen Saetze aus Fassungen vor 6 nicht geprueft werden; sie haben kein
 * Kettenglied, und die Pruefung endet dort mit OSK_OK. */
osk_result_t osk_log_verify(size_t *checked_out, size_t *break_at_out);

#endif
