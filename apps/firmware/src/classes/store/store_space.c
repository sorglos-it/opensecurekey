/* Index und Geheimnisse im Flash.
 *
 * Reihenfolge jeder Aenderung - und nur diese macht sie atomar:
 *   1. neues Geheimnis auf einen FREIEN Datenplatz schreiben
 *   2. neue Indexfassung an einen FREIEN Platz im Index-Ring schreiben
 *   3. Kopfsatz schreiben, der auf beides zeigt  <- ab hier gilt es
 *   4. alten Datenplatz freigeben und loeschen
 * Bricht der Strom vor Schritt 3 weg, gilt der alte Kopfsatz und damit der
 * alte Zustand. Bricht er in Schritt 3 weg, ist die Pruefsumme falsch und der
 * alte Satz gilt weiter. Bricht er in Schritt 4 weg, bleibt ein belegter, aber
 * unbenutzter Platz zurueck - lesbar ist der Tresor in jedem Fall. */

#include <string.h>
#include "store_internal.h"

static osk_idx_page_t g_idx;
static int g_idx_profile = -1;
static uint8_t g_idx_ct[sizeof(osk_idx_page_t) + OSK_TAG_LEN];
static uint8_t g_blob[OSK_BLOB_SLOT];

osk_idx_page_t *osk_index_page(void)
{
	return &g_idx;
}

int osk_index_loaded_profile(void)
{
	return g_idx_profile;
}

void osk_index_drop(void)
{
	/* Der Index enthaelt Namen und Adressen - nichts davon bleibt liegen. */
	osk_crypto_wipe(&g_idx, sizeof(g_idx));
	osk_crypto_wipe(g_idx_ct, sizeof(g_idx_ct));
	g_idx_profile = -1;
}

/* ---- Index ------------------------------------------------------------ */

static uint32_t frame_crc(const osk_idx_frame_t *f)
{
	return osk_crc32(f, sizeof(*f) - sizeof(f->crc32));
}

osk_result_t osk_index_load(uint8_t profile)
{
	osk_store_state_t *st = osk_store_state();
	osk_idx_frame_t frame;
	osk_result_t r;

	if (profile >= OSK_PROFILE_MAX) {
		return OSK_ERR_ARG;
	}
	if (g_idx_profile == (int)profile) {
		return OSK_OK;
	}
	osk_index_drop();

	uint32_t root = st->hdr.index_root[profile];

	if (root == 0u) {
		/* leeres Profil - kein Fehler, nur nichts darin */
		g_idx.head.magic = OSK_IDX_MAGIC;
		g_idx.head.format = OSK_FMT_CURRENT;
		g_idx.head.count = 0u;
		g_idx.head.generation = 0u;
		g_idx.head.profile = profile;
		g_idx_profile = (int)profile;
		return OSK_OK;
	}

	r = osk_flash_read(root, &frame, sizeof(frame));
	if (r != OSK_OK) {
		return r;
	}
	if (frame.magic != OSK_IDX_MAGIC || frame.crc32 != frame_crc(&frame) ||
	    frame.ct_len < OSK_TAG_LEN + sizeof(osk_idx_head_t) ||
	    frame.ct_len > sizeof(g_idx_ct)) {
		return OSK_ERR_FORMAT;
	}

	r = osk_flash_read(root + sizeof(frame), g_idx_ct, frame.ct_len);
	if (r != OSK_OK) {
		return r;
	}

	size_t plain_len = 0;
	uint8_t aad[1] = {profile};

	/* Die Fassung steht im verschluesselten Kopf der Indexfassung, nicht in
	 * den zusaetzlichen Daten. Nur so laesst sich eine Fassung lesen, ohne
	 * vorher zu wissen, welche es ist. */
	r = osk_crypto_open(&st->key_profile[profile], frame.nonce, aad, sizeof(aad), g_idx_ct,
			    frame.ct_len, (uint8_t *)&g_idx, sizeof(g_idx), &plain_len);
	if (r != OSK_OK) {
		/* Falscher Schluessel heisst: falsche PIN fuer dieses Profil. */
		osk_index_drop();
		return (r == OSK_ERR_FORMAT) ? OSK_ERR_DENIED : r;
	}
	if (plain_len < sizeof(osk_idx_head_t) || g_idx.head.magic != OSK_IDX_MAGIC ||
	    g_idx.head.profile != profile || g_idx.head.count > OSK_ITEMS_MAX) {
		osk_index_drop();
		return OSK_ERR_FORMAT;
	}

	/* Welche Satzgroesse zu welcher Fassung gehoert. Stimmt die Laenge des
	 * Klartextes damit nicht, ist es keine Fassung, die diese Firmware
	 * kennt - dann wird nichts geraten. */
	size_t rec_size = 0;

	switch (g_idx.head.format) {
	case OSK_FMT_V1:
		rec_size = OSK_IDX_REC_V1_SIZE;
		break;
	case OSK_FMT_V2:
		rec_size = OSK_IDX_REC_V2_SIZE;
		break;
	case OSK_FMT_V3:
		rec_size = sizeof(osk_idx_rec_t);
		break;
	default:
		osk_index_drop();
		return OSK_ERR_FORMAT;
	}
	if (plain_len != sizeof(osk_idx_head_t) + (size_t)g_idx.head.count * rec_size) {
		osk_index_drop();
		return OSK_ERR_FORMAT;
	}
	if (g_idx.head.format != OSK_FMT_CURRENT) {
		r = osk_index_upgrade(g_idx.head.format, plain_len);
		if (r != OSK_OK) {
			osk_index_drop();
			return r;
		}
	}
	g_idx_profile = (int)profile;
	return OSK_OK;
}

osk_result_t osk_index_store(uint8_t profile)
{
	osk_store_state_t *st = osk_store_state();
	osk_idx_frame_t frame;
	osk_result_t r;

	if (profile >= OSK_PROFILE_MAX || g_idx_profile != (int)profile) {
		return OSK_ERR_ARG;
	}
	if (g_idx.head.count > OSK_ITEMS_MAX) {
		return OSK_ERR_INTERNAL;
	}

	g_idx.head.magic = OSK_IDX_MAGIC;
	/* Geschrieben wird immer die jetzige Fassung - damit wandert ein alter
	 * Tresor Profil fuer Profil mit, sobald etwas geaendert wird. */
	g_idx.head.format = OSK_FMT_CURRENT;
	g_idx.head.profile = profile;
	g_idx.head.generation += 1u;

	size_t plain_len =
	    sizeof(osk_idx_head_t) + (size_t)g_idx.head.count * sizeof(osk_idx_rec_t);
	size_t ct_len = 0;
	uint8_t aad[1] = {profile};

	memset(&frame, 0, sizeof(frame));
	r = osk_nonce_take(OSK_NONCE_INDEX, frame.nonce);
	if (r != OSK_OK) {
		return r;
	}
	r = osk_crypto_seal(&st->key_profile[profile], frame.nonce, aad, sizeof(aad),
			    (const uint8_t *)&g_idx, plain_len, g_idx_ct, sizeof(g_idx_ct),
			    &ct_len);
	if (r != OSK_OK) {
		return r;
	}

	frame.magic = OSK_IDX_MAGIC;
	frame.ct_len = (uint32_t)ct_len;
	frame.crc32 = frame_crc(&frame);

	uint32_t total = osk_round_page((uint32_t)(sizeof(frame) + ct_len));
	uint32_t addr = 0;

	r = osk_space_index_alloc(total, &addr);
	if (r != OSK_OK) {
		return r;
	}

	/* Seitenweise schreiben: erst der Rahmen samt Anfang des Kryptotextes,
	 * dann der Rest. Der Rahmen steht zuerst, aber gueltig wird die Fassung
	 * ohnehin erst mit dem Kopfsatz. */
	uint8_t page[OSK_PAGE];
	uint32_t written = 0;
	const uint8_t *src_ct = g_idx_ct;
	size_t ct_done = 0;

	while (written < total) {
		memset(page, 0xFF, sizeof(page));
		uint32_t off = 0;

		if (written == 0) {
			memcpy(page, &frame, sizeof(frame));
			off = sizeof(frame);
		}
		uint32_t room = (uint32_t)(sizeof(page) - off);
		uint32_t take = (uint32_t)(ct_len - ct_done);

		if (take > room) {
			take = room;
		}
		if (take > 0u) {
			memcpy(page + off, src_ct + ct_done, take);
			ct_done += take;
		}
		r = osk_flash_program(addr + written, page, sizeof(page));
		if (r != OSK_OK) {
			return r;
		}
		written += OSK_PAGE;
	}
	r = osk_flash_sync();
	if (r != OSK_OK) {
		return r;
	}

	/* Nur im Zustand merken. Gueltig wird es mit osk_hdr_commit(). */
	st->hdr.index_root[profile] = addr;
	st->hdr.index_len[profile] = total;
	return OSK_OK;
}

/* ---- Geheimnisse ------------------------------------------------------ */

static uint32_t slot_addr(uint32_t slot)
{
	return OSK_AREA_BLOB_OFF + slot * OSK_BLOB_SLOT;
}

static uint32_t blob_crc(const osk_blob_frame_t *f)
{
	return osk_crc32(f, sizeof(*f) - sizeof(f->crc32));
}

osk_result_t osk_blob_write(uint32_t slot, osk_item_id_t id, const uint8_t *secret, size_t len,
			    const osk_key_t *key, uint32_t *ct_len_out,
			    uint8_t nonce_out[OSK_NONCE_LEN])
{
	osk_store_state_t *st = osk_store_state();
	osk_blob_frame_t frame;
	osk_result_t r;

	if (slot >= OSK_BLOB_SLOTS || len == 0 || len > OSK_SECRET_MAX || secret == NULL) {
		return OSK_ERR_ARG;
	}

	r = osk_nonce_take(OSK_NONCE_BLOB, nonce_out);
	if (r != OSK_OK) {
		return r;
	}

	memset(&frame, 0, sizeof(frame));
	frame.magic = OSK_BLOB_MAGIC;
	frame.id = id;
	frame.ct_len = (uint32_t)(len + OSK_TAG_LEN);
	frame.generation = st->hdr.seq;
	frame.crc32 = blob_crc(&frame);

	/* Der Rahmen ist zusaetzliche Daten der Verschluesselung. Damit laesst
	 * sich ein Geheimnis nicht einem anderen Eintrag unterschieben. */
	size_t ct_len = 0;

	memset(g_blob, 0xFF, sizeof(g_blob));
	memcpy(g_blob, &frame, sizeof(frame));
	r = osk_crypto_seal(key, nonce_out, (const uint8_t *)&frame,
			    sizeof(frame) - sizeof(frame.crc32), secret, len,
			    g_blob + sizeof(frame), sizeof(g_blob) - sizeof(frame), &ct_len);
	if (r != OSK_OK) {
		osk_crypto_wipe(g_blob, sizeof(g_blob));
		return r;
	}

	uint32_t total = osk_round_page((uint32_t)(sizeof(frame) + ct_len));

	r = osk_space_ensure_erased(slot_addr(slot), OSK_BLOB_SLOT);
	if (r == OSK_OK) {
		r = osk_flash_program(slot_addr(slot), g_blob, total);
	}
	if (r == OSK_OK) {
		r = osk_flash_sync();
	}
	osk_crypto_wipe(g_blob, sizeof(g_blob));
	if (r != OSK_OK) {
		return r;
	}
	*ct_len_out = (uint32_t)ct_len;
	return OSK_OK;
}

osk_result_t osk_blob_read(const osk_idx_rec_t *rec, const osk_key_t *key, uint8_t *out, size_t cap,
			   size_t *len_out)
{
	osk_blob_frame_t frame;
	osk_result_t r;

	if (rec == NULL || out == NULL || len_out == NULL || rec->slot >= OSK_BLOB_SLOTS) {
		return OSK_ERR_ARG;
	}
	if (rec->blob_ct_len < OSK_TAG_LEN || rec->blob_ct_len > sizeof(g_blob) - sizeof(frame)) {
		return OSK_ERR_FORMAT;
	}

	r = osk_flash_read(slot_addr(rec->slot), &frame, sizeof(frame));
	if (r != OSK_OK) {
		return r;
	}
	if (frame.magic != OSK_BLOB_MAGIC || frame.crc32 != blob_crc(&frame) ||
	    frame.id != rec->id || frame.ct_len != rec->blob_ct_len) {
		return OSK_ERR_FORMAT;
	}

	r = osk_flash_read(slot_addr(rec->slot) + sizeof(frame), g_blob, frame.ct_len);
	if (r != OSK_OK) {
		return r;
	}
	r = osk_crypto_open(key, rec->nonce, (const uint8_t *)&frame,
			    sizeof(frame) - sizeof(frame.crc32), g_blob, frame.ct_len, out, cap,
			    len_out);
	osk_crypto_wipe(g_blob, sizeof(g_blob));
	return r;
}

osk_result_t osk_blob_erase(uint32_t slot)
{
	if (slot >= OSK_BLOB_SLOTS) {
		return OSK_ERR_ARG;
	}
	return osk_space_ensure_erased(slot_addr(slot), OSK_BLOB_SLOT);
}
