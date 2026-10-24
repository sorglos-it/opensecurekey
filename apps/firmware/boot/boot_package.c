/* Paketkopf lesen und pruefen. Reihenfolge der Fragen ist Absicht:
 * zuerst, ob es ueberhaupt ein Paket ist, dann WER es unterschrieben hat,
 * und erst danach werden Felder daraus geglaubt. Wer die Reihenfolge
 * umstellt, glaubt einem Fremden seine Brettkennung. */

#include <string.h>
#include "osk_boot.h"

static uint32_t read_u32(const uint8_t *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
	       ((uint32_t)p[3] << 24);
}

static uint16_t read_u16(const uint8_t *p)
{
	return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

int osk_boot_version_cmp(const uint32_t a[4], const uint32_t b[4])
{
	for (int i = 0; i < 4; i++) {
		if (a[i] != b[i]) {
			return (a[i] < b[i]) ? -1 : 1;
		}
	}
	return 0;
}

static void parse(const uint8_t *raw, osk_boot_header_t *out)
{
	out->header_len = read_u16(raw + 8);
	out->format = read_u16(raw + 10);
	out->board_id = read_u32(raw + 12);
	for (int i = 0; i < 4; i++) {
		out->version[i] = read_u32(raw + 16 + 4 * i);
	}
	out->counter = read_u32(raw + 32);
	out->image_len = read_u32(raw + 36);
	memcpy(out->image_hash, raw + 40, OSK_BOOT_HASH_LEN);
}

osk_result_t osk_boot_header_check(const osk_boot_backend_t *be, const uint8_t *raw, size_t raw_len,
				   osk_boot_header_t *out)
{
	osk_boot_header_t head;
	uint32_t counter = 0;
	osk_result_t res;

	if (be == NULL || raw == NULL || out == NULL) {
		return OSK_ERR_ARG;
	}
	/* Ohne Pruefer gibt es kein Urteil. Eine fehlende Attrappe darf nie
	 * als "Signatur in Ordnung" durchgehen. */
	if (be->verify == NULL || be->counter_read == NULL) {
		return OSK_ERR_ARG;
	}
	/* Kurz heisst: kein Paket. Auch die fehlende Signatur landet hier -
	 * ein Paket ohne die 64 Byte ist kuerzer als 136. */
	if (raw_len < (size_t)(OSK_BOOT_HEADER_LEN + OSK_BOOT_SIG_LEN)) {
		return OSK_ERR_FORMAT;
	}
	if (memcmp(raw, OSK_BOOT_MAGIC, OSK_BOOT_MAGIC_LEN) != 0) {
		return OSK_ERR_FORMAT;
	}

	parse(raw, &head);
	if (head.format != OSK_BOOT_FORMAT || head.header_len != OSK_BOOT_HEADER_LEN) {
		return OSK_ERR_FORMAT;
	}
	if (head.image_len == 0) {
		return OSK_ERR_FORMAT;
	}

	/* Jetzt die Herkunft - vor jedem weiteren Feld. */
	if (!be->verify(raw, OSK_BOOT_HEADER_LEN, raw + OSK_BOOT_HEADER_LEN)) {
		return OSK_ERR_DENIED;
	}

	if (head.board_id == OSK_BOARD_UNKNOWN || head.board_id != be->board_id) {
		return OSK_ERR_UNSUPPORTED;
	}

	/* Rueckschritt: zwei Wege, und beide muessen zu sein.
	 * - Der Zaehler im SE050 ist die harte Grenze; er laesst sich durch
	 *   Loeschen des Flash nicht zuruecksetzen.
	 * - Die Fassungsnummer ist die weiche: sie faengt den Fall, dass
	 *   zwei Pakete denselben Zaehler tragen (etwa zwei Builds
	 *   derselben Stufe). */
	res = be->counter_read(&counter);
	if (res != OSK_OK) {
		return res;
	}
	if (head.counter < counter) {
		return OSK_ERR_EXISTS;
	}
	if (head.counter == counter &&
	    osk_boot_version_cmp(head.version, be->running_version) <= 0) {
		return OSK_ERR_EXISTS;
	}

	*out = head;
	return OSK_OK;
}
