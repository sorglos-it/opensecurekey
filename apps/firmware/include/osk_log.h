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
	uint16_t what;        /* Anlass, siehe shared/protocol.md */
	uint32_t item_id;     /* 0 = kein Eintrag beteiligt */
	char asked_by[32];
} osk_log_entry_t;

osk_result_t osk_log_add(const osk_log_entry_t *e);
osk_result_t osk_log_count(size_t *count_out);
osk_result_t osk_log_get(size_t index, osk_log_entry_t *out);
osk_result_t osk_log_clear(void);

#endif
