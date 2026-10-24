/* Das Aufspielen selbst: Paket annehmen, Abbild in den zweiten Platz
 * schreiben, pruefen, Umschalten erbitten.
 *
 * Der Ablauf ist so geschnitten, dass an JEDER Stelle abgebrochen werden
 * darf. Nach jedem Abbruch gilt: der Zustand ist entweder der alte oder ein
 * vollstaendiger neuer, und in beiden Faellen startet die alte Fassung.
 *
 * Die Reihenfolge, und warum sie so ist:
 *   1. Kopf und Signatur pruefen - bevor ein Byte in den Flash geht.
 *   2. Zustand auf WRITING setzen (written = 0).
 *   3. Abbild der Reihe nach schreiben, Hash im Vorbeigehen bilden.
 *      Der Zustand wird dabei fortgeschrieben, aber nie auf READY.
 *   4. Erst wenn alle Byte da sind UND der Hash stimmt: READY.
 *   5. Der Bootloader schaltet um und setzt TESTING.
 *   6. Die neue Fassung meldet sich -> CONFIRMED, Zaehler im SE050 hoch.
 *
 * Zwischen 2 und 4 liegt der zweite Platz im externen Flash halb
 * beschrieben - und genau darum wird er nicht gestartet. */

#include <string.h>
#include "osk_boot.h"
#include "osk_boot_session.h"

osk_result_t osk_boot_begin(const osk_boot_backend_t *be, osk_boot_slots_t *slots,
			    const uint8_t *raw, size_t raw_len, osk_boot_session_t *se)
{
	osk_boot_state_t st;
	osk_result_t res;
	int which;

	if (be == NULL || slots == NULL || se == NULL) {
		return OSK_ERR_ARG;
	}
	if (be->hash_init == NULL || be->hash_update == NULL || be->hash_final == NULL) {
		return OSK_ERR_ARG;
	}
	if (se->write == NULL || se->erase == NULL) {
		return OSK_ERR_ARG;
	}

	res = osk_boot_header_check(be, raw, raw_len, &se->head);
	if (res != OSK_OK) {
		return res;
	}
	if (se->head.image_len > se->slot_len) {
		/* Passt nicht in den zweiten Platz. Lieber jetzt sagen als
		 * nach drei Minuten Uebertragung. */
		return OSK_ERR_FULL;
	}

	/* Den zweiten Platz leeren. Scheitert das, wird nichts angefasst und
	 * nichts behauptet. */
	res = se->erase(se->ctx, 0, se->head.image_len);
	if (res != OSK_OK) {
		return res;
	}

	memset(&st, 0, sizeof(st));
	st.stage = (uint8_t)OSK_BOOT_WRITING;
	st.counter = se->head.counter;
	memcpy(st.version, se->head.version, sizeof(st.version));
	st.image_len = se->head.image_len;
	st.written = 0;
	memcpy(st.hash, se->head.image_hash, OSK_BOOT_HASH_LEN);
	res = osk_boot_state_store(slots, &st, &which);
	if (res != OSK_OK) {
		return res;
	}

	se->be = be;
	se->written = 0;
	se->open = true;
	be->hash_init(be->hash_ctx);
	return OSK_OK;
}

osk_result_t osk_boot_write(osk_boot_slots_t *slots, osk_boot_session_t *se, uint32_t offset,
			    const uint8_t *data, size_t len)
{
	osk_boot_state_t st;
	osk_result_t res;
	int which;

	if (slots == NULL || se == NULL || data == NULL || len == 0) {
		return OSK_ERR_ARG;
	}
	if (!se->open) {
		return OSK_ERR_NOT_FOUND;
	}
	/* Nur der Reihe nach. Ein Paket, das Loecher laesst, waere nicht zu
	 * hashen, ohne alles zurueckzulesen - und ein Angreifer koennte ein
	 * geprueftes Abbild nachtraeglich an einer Stelle veraendern. */
	if (offset != se->written) {
		return OSK_ERR_FORMAT;
	}
	if ((uint32_t)len > se->head.image_len - se->written) {
		return OSK_ERR_FORMAT;
	}

	res = se->write(se->ctx, offset, data, len);
	if (res != OSK_OK) {
		/* Der Flash hat nicht angenommen. Der Zustand bleibt
		 * WRITING mit dem alten Zaehler - was geschrieben wurde, ist
		 * egal, denn dieses Abbild wird nie gestartet. */
		return res;
	}
	se->be->hash_update(se->be->hash_ctx, data, len);
	se->written += (uint32_t)len;

	/* Den Fortschritt festhalten, aber nur alle 64 kB und am Ende -
	 * jeder Abdruck kostet eine Loeschung im Flash, und 64 MB Abbild
	 * durch 64 kB sind tausend Schreibvorgaenge, nicht Millionen.
	 * Geht der Strom zwischen zwei Abdruecken weg, steht ein kleinerer
	 * Wert in `written` als wirklich geschrieben wurde. Das ist
	 * ungefaehrlich: zu klein heisst "nicht vollstaendig", und das
	 * Abbild wird verworfen, nicht gestartet. */
	if ((se->written % (64u * 1024u)) == 0 || se->written == se->head.image_len) {
		res = osk_boot_state_load(slots, &st);
		if (res != OSK_OK) {
			return res;
		}
		st.written = se->written;
		st.stage = (uint8_t)OSK_BOOT_WRITING;
		res = osk_boot_state_store(slots, &st, &which);
		if (res != OSK_OK) {
			return res;
		}
	}
	return OSK_OK;
}

osk_result_t osk_boot_finish(osk_boot_slots_t *slots, osk_boot_session_t *se)
{
	uint8_t got[OSK_BOOT_HASH_LEN];
	osk_boot_state_t st;
	osk_result_t res;
	int which;
	int diff = 0;

	if (slots == NULL || se == NULL) {
		return OSK_ERR_ARG;
	}
	if (!se->open) {
		return OSK_ERR_NOT_FOUND;
	}
	if (se->written != se->head.image_len) {
		return OSK_ERR_FORMAT;
	}

	se->be->hash_final(se->be->hash_ctx, got);
	/* Vergleich ohne Abkuerzung: die Laufzeit soll nicht verraten, wie
	 * weit zwei Hashes uebereinstimmen. */
	for (int i = 0; i < OSK_BOOT_HASH_LEN; i++) {
		diff |= got[i] ^ se->head.image_hash[i];
	}
	if (diff != 0) {
		se->open = false;
		return OSK_ERR_FORMAT;
	}

	res = osk_boot_state_load(slots, &st);
	if (res != OSK_OK) {
		return res;
	}
	st.stage = (uint8_t)OSK_BOOT_READY;
	st.written = se->written;
	st.tries = 0;
	res = osk_boot_state_store(slots, &st, &which);
	if (res != OSK_OK) {
		return res;
	}
	se->open = false;
	return OSK_OK;
}

osk_result_t osk_boot_abort(osk_boot_slots_t *slots, osk_boot_session_t *se)
{
	osk_boot_state_t st;
	osk_result_t res;
	int which;

	if (slots == NULL) {
		return OSK_ERR_ARG;
	}
	if (se != NULL) {
		se->open = false;
		se->written = 0;
	}
	res = osk_boot_state_load(slots, &st);
	if (res != OSK_OK) {
		return res;
	}
	/* Ein laufendes TESTING darf das Abbrechen nicht anfassen: dort ist
	 * schon umgeschaltet, und der Rueckweg haengt am Zustand. */
	if ((osk_boot_stage_t)st.stage == OSK_BOOT_TESTING) {
		return OSK_ERR_DENIED;
	}
	memset(&st, 0, sizeof(st));
	st.stage = (uint8_t)OSK_BOOT_IDLE;
	return osk_boot_state_store(slots, &st, &which);
}
