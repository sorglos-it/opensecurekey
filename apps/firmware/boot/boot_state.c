/* Der Zustand des Updates, abbruchsicher abgelegt.
 *
 * Zwei Abdruecke, A und B, je 128 Byte. Geschrieben wird immer der aeltere.
 * Gueltig ist ein Abdruck nur mit Kennung UND richtiger Pruefsumme - ein
 * halb geschriebener faellt durch, und dann gilt weiter der andere. Darum
 * kann kein Abbruch den Zustand verlieren.
 *
 * Geloeschter Flash ist ueberall 0xFF; das ist keine gueltige Kennung, ein
 * frisches Geraet liest also sauber "nichts im Gange". */

#include <string.h>
#include "osk_boot.h"

/* CRC-32 wie in PNG und zlib (umgekehrtes Polynom 0xEDB88320), Tabelle zur
 * Laufzeit gerechnet - 1 kB Tabelle im Flash spart man sich hier lieber.
 * Das ist keine Krypto: die Pruefsumme faengt abgebrochene Schreibvorgaenge,
 * nicht Faelschungen. Dafuer ist die Signatur da. */
uint32_t osk_boot_crc32(const uint8_t *data, size_t len)
{
	uint32_t crc = 0xFFFFFFFFu;

	if (data == NULL) {
		return 0;
	}
	for (size_t i = 0; i < len; i++) {
		crc ^= data[i];
		for (int bit = 0; bit < 8; bit++) {
			crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)(-(int32_t)(crc & 1u)));
		}
	}
	return crc ^ 0xFFFFFFFFu;
}

/* --- Abdruck <-> Byte ----------------------------------------------------- */

#define OFF_MAGIC 0
#define OFF_SEQ 4
#define OFF_STAGE 8
#define OFF_TRIES 9
#define OFF_COUNTER 12
#define OFF_VERSION 16
#define OFF_WRITTEN 32
#define OFF_IMAGE_LEN 36
#define OFF_HASH 40
#define OFF_CRC 72
#define COVERED 72 /* von der Pruefsumme gedeckte Byte */

static void put_u32(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)(v & 0xFFu);
	p[1] = (uint8_t)((v >> 8) & 0xFFu);
	p[2] = (uint8_t)((v >> 16) & 0xFFu);
	p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static uint32_t get_u32(const uint8_t *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
	       ((uint32_t)p[3] << 24);
}

static void pack(const osk_boot_state_t *st, uint8_t out[OSK_BOOT_SLOT_LEN])
{
	memset(out, 0, OSK_BOOT_SLOT_LEN);
	put_u32(out + OFF_MAGIC, OSK_BOOT_STATE_MAGIC);
	put_u32(out + OFF_SEQ, st->seq);
	out[OFF_STAGE] = st->stage;
	out[OFF_TRIES] = st->tries;
	put_u32(out + OFF_COUNTER, st->counter);
	for (int i = 0; i < 4; i++) {
		put_u32(out + OFF_VERSION + 4 * i, st->version[i]);
	}
	put_u32(out + OFF_WRITTEN, st->written);
	put_u32(out + OFF_IMAGE_LEN, st->image_len);
	memcpy(out + OFF_HASH, st->hash, OSK_BOOT_HASH_LEN);
	put_u32(out + OFF_CRC, osk_boot_crc32(out, COVERED));
}

static bool unpack(const uint8_t in[OSK_BOOT_SLOT_LEN], osk_boot_state_t *out)
{
	if (get_u32(in + OFF_MAGIC) != OSK_BOOT_STATE_MAGIC) {
		return false;
	}
	if (get_u32(in + OFF_CRC) != osk_boot_crc32(in, COVERED)) {
		return false;
	}
	/* Folgenummer 0 heisst "leer" und wird nie geschrieben. */
	if (get_u32(in + OFF_SEQ) == 0) {
		return false;
	}
	if (in[OFF_STAGE] > (uint8_t)OSK_BOOT_CONFIRMED) {
		return false;
	}
	memset(out, 0, sizeof(*out));
	out->magic = OSK_BOOT_STATE_MAGIC;
	out->seq = get_u32(in + OFF_SEQ);
	out->stage = in[OFF_STAGE];
	out->tries = in[OFF_TRIES];
	out->counter = get_u32(in + OFF_COUNTER);
	for (int i = 0; i < 4; i++) {
		out->version[i] = get_u32(in + OFF_VERSION + 4 * i);
	}
	out->written = get_u32(in + OFF_WRITTEN);
	out->image_len = get_u32(in + OFF_IMAGE_LEN);
	memcpy(out->hash, in + OFF_HASH, OSK_BOOT_HASH_LEN);
	out->crc = get_u32(in + OFF_CRC);
	return true;
}

osk_result_t osk_boot_state_load(const osk_boot_slots_t *slots, osk_boot_state_t *out)
{
	osk_boot_state_t a, b;
	bool a_ok, b_ok;

	if (slots == NULL || out == NULL) {
		return OSK_ERR_ARG;
	}
	a_ok = unpack(slots->a, &a);
	b_ok = unpack(slots->b, &b);

	if (!a_ok && !b_ok) {
		/* Frisches Geraet, oder beide Abdruecke hin. Beides heisst
		 * dasselbe: es laeuft kein Update, starte das Laufende. */
		memset(out, 0, sizeof(*out));
		out->magic = OSK_BOOT_STATE_MAGIC;
		out->stage = (uint8_t)OSK_BOOT_IDLE;
		return OSK_OK;
	}
	if (!b_ok || (a_ok && a.seq > b.seq)) {
		*out = a;
	} else {
		*out = b;
	}
	return OSK_OK;
}

osk_result_t osk_boot_state_store(osk_boot_slots_t *slots, osk_boot_state_t *next, int *which_out)
{
	osk_boot_state_t a, b;
	bool a_ok, b_ok;
	uint32_t newest = 0;
	int which;

	if (slots == NULL || next == NULL || which_out == NULL) {
		return OSK_ERR_ARG;
	}
	if (next->stage > (uint8_t)OSK_BOOT_CONFIRMED) {
		return OSK_ERR_ARG;
	}
	a_ok = unpack(slots->a, &a);
	b_ok = unpack(slots->b, &b);
	if (a_ok) {
		newest = a.seq;
	}
	if (b_ok && b.seq > newest) {
		newest = b.seq;
	}
	/* Den aelteren beschreiben, damit der gueltige stehen bleibt, bis der
	 * neue vollstaendig ist. Ist einer ungueltig, ist er der aeltere. */
	if (!a_ok) {
		which = 0;
	} else if (!b_ok) {
		which = 1;
	} else {
		which = (a.seq < b.seq) ? 0 : 1;
	}

	next->magic = OSK_BOOT_STATE_MAGIC;
	next->seq = newest + 1;
	if (next->seq == 0) {
		/* Ueberlauf nach 4 Milliarden Schreibvorgaengen. Nicht
		 * erreichbar - aber 0 heisst "leer", also nie 0 schreiben. */
		next->seq = 1;
	}
	pack(next, (which == 0) ? slots->a : slots->b);
	next->crc = get_u32(((which == 0) ? slots->a : slots->b) + OFF_CRC);
	*which_out = which;
	return OSK_OK;
}
