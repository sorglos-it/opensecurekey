/* Geraeteprotokoll: Ringpuffer mit festen Plaetzen von einer Flash-Seite.
 *
 * Im Protokoll steht nie ein Geheimnis - nur Zeit, Anlass, Eintragskennung und
 * der Name, den das fragende Programm behauptet. Verschluesselt ist es
 * trotzdem, denn schon die Liste der benutzten Dienste ist etwas wert.
 *
 * Zwei Entscheidungen stammen aus dem Stromausfall-Test:
 *
 *  1. Die Folgenummer sagt NICHT, auf welchem Platz ein Satz steht. Der
 *     naechste Satz kommt immer auf den Platz hinter dem jueengsten. Waere es
 *     anders, wuerde nach einem Stromausfall dieselbe Nummer auf denselben
 *     Platz ein zweites Mal geschrieben - und Programmieren kann Bits nur auf
 *     0 ziehen, also stuende dort Unsinn statt eines Satzes.
 *
 *  2. Beschrieben wird nur ein wirklich leerer Platz. Ist er nicht leer, wird
 *     vorher sein ganzer Sektor geloescht. Dabei gehen die aeltesten Saetze
 *     verloren - das ist das Wesen eines Ringpuffers.
 *
 * Der Kopf des Rings wird beim Oeffnen gesucht und nicht im Kopfsatz vermerkt.
 * Sonst wuerde jeder Protokolleintrag einen Kopfsatz kosten und mitten in
 * einer Transaktion liegen. */

#include <string.h>
#include "store_internal.h"

static uint32_t g_seq;        /* hoechste gefundene Folgenummer, 0 = leer */
static uint32_t g_next;       /* Platz fuer den naechsten Satz */
static bool g_mounted;

static uint32_t slot_addr(uint32_t slot)
{
	return OSK_AREA_LOG_OFF + (slot % OSK_LOG_SLOTS) * OSK_LOG_SLOT;
}

/* Deckt magic, Folgenummer, Nonce und den ganzen Kryptotext ab. Die
 * Pruefsumme steht hinten: nur so faellt ein Platz auf, der beim Stromausfall
 * mitten entzwei gerissen ist. */
static uint32_t slot_crc(const osk_log_slot_t *s)
{
	return osk_crc32(s, offsetof(osk_log_slot_t, crc32));
}

static bool slot_valid(const osk_log_slot_t *s)
{
	return s->magic == OSK_LOG_MAGIC && s->seq != 0u &&
	       s->crc32 == slot_crc(s);
}

/* Ist auf diesem Platz noch nie etwas gestanden? */
static osk_result_t slot_is_empty(uint32_t slot, bool *out)
{
	uint8_t buf[OSK_LOG_SLOT];
	osk_result_t r = osk_flash_read(slot_addr(slot), buf, sizeof(buf));

	if (r != OSK_OK) {
		return r;
	}
	*out = true;
	for (size_t i = 0; i < sizeof(buf); i++) {
		if (buf[i] != 0xFFu) {
			*out = false;
			break;
		}
	}
	return OSK_OK;
}

/* Sucht den jueengsten Satz und damit den naechsten freien Platz. Einmal je
 * Sitzung; 512 Plaetze zu lesen kostet nichts. */
static osk_result_t log_mount(void)
{
	osk_log_slot_t slot;
	bool found = false;
	uint32_t head = 0;

	if (g_mounted) {
		return OSK_OK;
	}
	g_seq = 0u;
	g_next = 0u;

	for (uint32_t i = 0; i < OSK_LOG_SLOTS; i++) {
		osk_result_t r = osk_flash_read(slot_addr(i), &slot,
						sizeof(slot));
		if (r != OSK_OK) {
			return r;
		}
		if (slot_valid(&slot) && (!found || slot.seq > g_seq)) {
			g_seq = slot.seq;
			head = i;
			found = true;
		}
	}
	if (found) {
		g_next = (head + 1u) % OSK_LOG_SLOTS;
	} else {
		/* Nichts Gueltiges da: beim ersten leeren Platz anfangen. */
		for (uint32_t i = 0; i < OSK_LOG_SLOTS; i++) {
			bool empty = false;
			osk_result_t r = slot_is_empty(i, &empty);

			if (r != OSK_OK) {
				return r;
			}
			if (empty) {
				g_next = i;
				break;
			}
		}
	}
	g_mounted = true;
	return OSK_OK;
}

void osk_log_reset_mount(void)
{
	g_mounted = false;
	g_seq = 0u;
	g_next = 0u;
}

osk_result_t osk_log_add(const osk_log_entry_t *e)
{
	osk_store_state_t *st = osk_store_state();
	osk_log_plain_t plain;
	osk_log_slot_t slot;
	uint8_t page[OSK_LOG_SLOT];
	osk_result_t r;

	if (e == NULL) {
		return OSK_ERR_ARG;
	}
	if (!st->open) {
		return OSK_ERR_LOCKED;
	}
	r = log_mount();
	if (r != OSK_OK) {
		return r;
	}

	memset(&plain, 0, sizeof(plain));
	plain.when_utc = e->when_utc;
	plain.link = (uint32_t)e->link;
	plain.what = e->what;
	plain.item_id = e->item_id;
	osk_str_copy(plain.asked_by, sizeof(plain.asked_by), e->asked_by);

	memset(&slot, 0, sizeof(slot));
	slot.magic = OSK_LOG_MAGIC;
	slot.seq = g_seq + 1u;
	r = osk_nonce_take(OSK_NONCE_LOG, slot.nonce);
	if (r != OSK_OK) {
		return r;
	}

	/* Die Folgenummer steht im Klartext, damit der Kopf des Rings ohne
	 * Schluessel zu finden ist, und ist zugleich zusaetzliche Daten der
	 * Verschluesselung: wer sie umschreibt, macht den Satz unlesbar,
	 * statt die Reihenfolge zu faelschen. */
	size_t ct_len = 0;
	uint8_t aad[4];

	for (int i = 0; i < 4; i++) {
		aad[i] = (uint8_t)(slot.seq >> (8 * i));
	}
	r = osk_crypto_seal(&st->key_log, slot.nonce, aad, sizeof(aad),
			    (const uint8_t *)&plain, sizeof(plain), slot.ct,
			    sizeof(slot.ct), &ct_len);
	osk_crypto_wipe(&plain, sizeof(plain));
	if (r != OSK_OK) {
		return r;
	}
	/* Erst mit fertigem Kryptotext die Pruefsumme. */
	slot.crc32 = slot_crc(&slot);

	/* Nur auf einen wirklich leeren Platz schreiben. Steht dort noch etwas
	 * - ein alter Satz oder der halbe Rest eines Stromausfalls -, muss
	 * zuerst der Sektor weg. */
	bool empty = false;

	r = slot_is_empty(g_next, &empty);
	if (r != OSK_OK) {
		return r;
	}
	if (!empty) {
		uint32_t sector = slot_addr(g_next) & ~(OSK_SECTOR - 1u);

		r = osk_space_ensure_erased(sector, OSK_SECTOR);
		if (r != OSK_OK) {
			return r;
		}
		/* Vorn im frisch geloeschten Sektor weiterschreiben. */
		g_next = (sector - OSK_AREA_LOG_OFF) / OSK_LOG_SLOT;
	}

	memset(page, 0xFF, sizeof(page));
	memcpy(page, &slot, sizeof(slot));
	r = osk_flash_program(slot_addr(g_next), page, sizeof(page));
	if (r == OSK_OK) {
		r = osk_flash_sync();
	}
	if (r != OSK_OK) {
		return r;
	}
	g_seq = slot.seq;
	g_next = (g_next + 1u) % OSK_LOG_SLOTS;
	return OSK_OK;
}

osk_result_t osk_log_add_internal(uint16_t what, osk_item_id_t id,
				  const char *asked_by)
{
	osk_log_entry_t e;

	memset(&e, 0, sizeof(e));
	e.when_utc = osk_now_utc();
	e.link = OSK_LINK_USB_VENDOR;
	e.what = what;
	e.item_id = id;
	osk_str_copy(e.asked_by, sizeof(e.asked_by), asked_by);
	return osk_log_add(&e);
}

/* Geht vom jueengsten Platz rueckwaerts. Ungueltige Plaetze werden
 * uebersprungen, bei einem leeren Platz ist der Ring zu Ende. So ist jeder
 * gezaehlte Satz auch wirklich lesbar. */
static osk_result_t walk(size_t index, osk_log_slot_t *out, size_t *count_out)
{
	size_t seen = 0;

	for (uint32_t step = 1; step <= OSK_LOG_SLOTS; step++) {
		uint32_t slot = (g_next + OSK_LOG_SLOTS - step) %
				OSK_LOG_SLOTS;
		osk_log_slot_t cur;
		bool empty = false;
		osk_result_t r = slot_is_empty(slot, &empty);

		if (r != OSK_OK) {
			return r;
		}
		if (empty) {
			break;
		}
		r = osk_flash_read(slot_addr(slot), &cur, sizeof(cur));
		if (r != OSK_OK) {
			return r;
		}
		if (!slot_valid(&cur)) {
			continue;
		}
		if (out != NULL && seen == index) {
			*out = cur;
			return OSK_OK;
		}
		seen++;
	}
	if (count_out != NULL) {
		*count_out = seen;
	}
	return (out == NULL) ? OSK_OK : OSK_ERR_NOT_FOUND;
}

osk_result_t osk_log_count(size_t *count_out)
{
	osk_store_state_t *st = osk_store_state();

	if (count_out == NULL) {
		return OSK_ERR_ARG;
	}
	if (!st->open) {
		return OSK_ERR_LOCKED;
	}
	osk_result_t r = log_mount();

	if (r != OSK_OK) {
		return r;
	}
	return walk(0, NULL, count_out);
}

/* index 0 ist der jueengste Satz. */
osk_result_t osk_log_get(size_t index, osk_log_entry_t *out)
{
	osk_store_state_t *st = osk_store_state();
	osk_log_slot_t slot;
	osk_log_plain_t plain;
	osk_result_t r;

	if (out == NULL) {
		return OSK_ERR_ARG;
	}
	if (!st->open) {
		return OSK_ERR_LOCKED;
	}
	r = log_mount();
	if (r != OSK_OK) {
		return r;
	}
	r = walk(index, &slot, NULL);
	if (r != OSK_OK) {
		return r;
	}

	uint8_t aad[4];
	size_t plain_len = 0;

	for (int i = 0; i < 4; i++) {
		aad[i] = (uint8_t)(slot.seq >> (8 * i));
	}
	r = osk_crypto_open(&st->key_log, slot.nonce, aad, sizeof(aad), slot.ct,
			    sizeof(slot.ct), (uint8_t *)&plain, sizeof(plain),
			    &plain_len);
	if (r != OSK_OK) {
		return r;
	}
	if (plain_len != sizeof(plain)) {
		osk_crypto_wipe(&plain, sizeof(plain));
		return OSK_ERR_FORMAT;
	}

	memset(out, 0, sizeof(*out));
	out->when_utc = plain.when_utc;
	out->link = (osk_link_t)plain.link;
	out->what = plain.what;
	out->item_id = plain.item_id;
	osk_str_copy(out->asked_by, sizeof(out->asked_by), plain.asked_by);
	osk_crypto_wipe(&plain, sizeof(plain));
	return OSK_OK;
}

osk_result_t osk_log_clear(void)
{
	osk_store_state_t *st = osk_store_state();

	if (!st->open) {
		return OSK_ERR_LOCKED;
	}
	osk_result_t r = osk_space_ensure_erased(OSK_AREA_LOG_OFF,
						 OSK_AREA_LOG_LEN);
	if (r != OSK_OK) {
		return r;
	}
	g_seq = 0u;
	g_next = 0u;
	g_mounted = true;
	return OSK_OK;
}
