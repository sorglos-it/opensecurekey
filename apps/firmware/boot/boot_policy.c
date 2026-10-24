/* Was startet der Bootloader, und wann gilt eine neue Fassung als gut?
 *
 * Jede Antwort hier faellt zugunsten der ALTEN Fassung aus, sobald etwas
 * unklar ist. Ein Stick, der nicht mehr startet, ist kaputt; ein Stick, der
 * die alte Fassung startet, ist nur nicht aktualisiert. */

#include <string.h>
#include "osk_boot.h"

osk_boot_start_t osk_boot_decide(const osk_boot_state_t *st, bool button_held)
{
	/* Der Notfallweg hat Vorrang vor allem - auch vor einem unlesbaren
	 * Zustand. Genau dafuer ist er da. */
	if (button_held) {
		return OSK_START_RECOVERY;
	}
	if (st == NULL) {
		return OSK_START_PRIMARY;
	}

	switch ((osk_boot_stage_t)st->stage) {
	case OSK_BOOT_READY:
		/* Nur umschalten, wenn das Abbild vollstaendig ist. Ein
		 * READY mit fehlenden Byte kann es nach einem Abbruch
		 * mitten im Schreiben nicht geben - der Zustand wird erst
		 * danach auf READY gesetzt -, aber geprueft wird es
		 * trotzdem: die Frage kostet nichts, der Irrtum den Stick. */
		if (st->image_len == 0 || st->written != st->image_len) {
			return OSK_START_PRIMARY;
		}
		return OSK_START_SWAP;
	case OSK_BOOT_TESTING:
		/* Umgeschaltet, aber die neue Fassung hat sich nie gemeldet.
		 * Nach OSK_BOOT_MAX_TRIES Versuchen zurueck. */
		if (st->tries >= OSK_BOOT_MAX_TRIES) {
			return OSK_START_ROLLBACK;
		}
		return OSK_START_PRIMARY;
	case OSK_BOOT_IDLE:
	case OSK_BOOT_WRITING:
	case OSK_BOOT_CONFIRMED:
	default:
		/* WRITING heisst: ein Abbild ist halb im zweiten Platz. Das
		 * wird nicht gestartet und nicht hereingeholt - es wird
		 * einfach uebergangen. */
		return OSK_START_PRIMARY;
	}
}

osk_result_t osk_boot_mark_try(osk_boot_slots_t *slots)
{
	osk_boot_state_t st;
	osk_result_t res;
	int which;

	if (slots == NULL) {
		return OSK_ERR_ARG;
	}
	res = osk_boot_state_load(slots, &st);
	if (res != OSK_OK) {
		return res;
	}
	if ((osk_boot_stage_t)st.stage != OSK_BOOT_TESTING) {
		return OSK_ERR_NOT_FOUND;
	}
	if (st.tries < 0xFF) {
		st.tries++;
	}
	return osk_boot_state_store(slots, &st, &which);
}

osk_result_t osk_boot_confirm(const osk_boot_backend_t *be, osk_boot_slots_t *slots)
{
	osk_boot_state_t st;
	osk_result_t res;
	uint32_t have = 0;
	int which;

	if (be == NULL || slots == NULL || be->counter_read == NULL || be->counter_raise == NULL) {
		return OSK_ERR_ARG;
	}
	res = osk_boot_state_load(slots, &st);
	if (res != OSK_OK) {
		return res;
	}
	/* Zweimal bestaetigen ist kein Fehler - ein wiederholter Aufruf darf
	 * nichts zusaetzlich anrichten. */
	if ((osk_boot_stage_t)st.stage == OSK_BOOT_CONFIRMED) {
		return OSK_OK;
	}
	if ((osk_boot_stage_t)st.stage != OSK_BOOT_TESTING) {
		return OSK_ERR_NOT_FOUND;
	}

	/* Erst hier wird der Rueckschritt gesperrt. Wer den Zaehler schon
	 * beim Schreiben anhebt, mauert den Rueckweg zu, bevor bewiesen ist,
	 * dass die neue Fassung ueberhaupt startet. */
	res = be->counter_read(&have);
	if (res != OSK_OK) {
		return res;
	}
	if (st.counter > have) {
		res = be->counter_raise(st.counter);
		if (res != OSK_OK) {
			/* Zaehler nicht angehoben: NICHT bestaetigen. Sonst
			 * stuende der Stick als bestaetigt da, waehrend die
			 * Sperre fehlt. Lieber noch einen Versuch. */
			return res;
		}
	}
	st.stage = (uint8_t)OSK_BOOT_CONFIRMED;
	st.tries = 0;
	return osk_boot_state_store(slots, &st, &which);
}

bool osk_boot_may_write(const osk_boot_area_t *allowed, size_t count, uint32_t start, uint32_t len)
{
	if (allowed == NULL || count == 0 || len == 0) {
		return false;
	}
	/* Ueberlauf am Ende des Adressraums waere ein Weg, die Grenze zu
	 * ueberspringen: start + len kippt um und liegt wieder im erlaubten
	 * Bereich. Dagegen steht die FORM des Vergleichs unten - es wird
	 * nirgends addiert, sondern nur abgezogen, und jedes Abziehen ist
	 * vorher gegen Unterlauf abgesichert. Ein zusaetzlicher Riegel hier
	 * waere toter Code: ein Mutant, der ihn entfernt, blieb am
	 * 24.10.2026 am ganzen Pruefsatz unentdeckt. Toter Code, der wie ein
	 * Riegel aussieht, ist schlimmer als keiner.
	 * Gemessen wird die Eigenschaft weiter - in test_vault.c, Fall
	 * "Ueberlauf des Adressraums hebelt die Grenze nicht aus". */
	for (size_t i = 0; i < count; i++) {
		uint32_t a_start = allowed[i].start;
		uint32_t a_len = allowed[i].len;

		if (a_len == 0 || a_len < len || a_len - 1u > 0xFFFFFFFFu - a_start) {
			continue;
		}
		if (start >= a_start && (start - a_start) <= (a_len - len)) {
			return true;
		}
	}
	return false;
}
