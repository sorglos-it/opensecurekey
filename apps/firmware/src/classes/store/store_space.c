/* Index und Geheimnisse im Flash.
 *
 * Reihenfolge jeder Aenderung - und nur diese macht sie atomar:
 *   1. neues Geheimnis auf einen FREIEN Datenplatz schreiben
 *   2. betroffene Indexbloecke an FREIE Plaetze im Index-Ring schreiben
 *   3. Kopfsatz schreiben, der auf alles zeigt  <- ab hier gilt es
 *   4. alten Datenplatz freigeben und loeschen
 * Bricht der Strom vor Schritt 3 weg, gilt der alte Kopfsatz und damit der
 * alte Zustand. Bricht er in Schritt 3 weg, ist die Pruefsumme falsch und der
 * alte Satz gilt weiter. Bricht er in Schritt 4 weg, bleibt ein belegter, aber
 * unbenutzter Platz zurueck - lesbar ist der Tresor in jedem Fall.
 *
 * Der Index eines Profils liegt in Bloecken zu 64 Saetzen. Im RAM ist immer
 * nur EIN Block: daran haengt die Eintragsgrenze. Eine Aenderung schreibt nur
 * die Bloecke neu, die sie anfasst - in der Regel einen, beim Loeschen zwei. */

#include <string.h>
#include "store_internal.h"

static osk_idx_page_t g_idx;
static int g_idx_profile = -1;
static int g_idx_block = -1;
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

int osk_index_loaded_block(void)
{
	return g_idx_block;
}

void osk_index_drop(void)
{
	/* Der Index enthaelt Namen und Adressen - nichts davon bleibt liegen. */
	osk_crypto_wipe(&g_idx, sizeof(g_idx));
	osk_crypto_wipe(g_idx_ct, sizeof(g_idx_ct));
	g_idx_profile = -1;
	g_idx_block = -1;
}

size_t osk_index_count(uint8_t profile)
{
	if (profile >= OSK_PROFILE_MAX) {
		return 0u;
	}
	return osk_store_state()->hdr.index_count[profile];
}

/* ---- Index ------------------------------------------------------------ */

static uint32_t frame_crc(const osk_idx_frame_t *f)
{
	return osk_crc32(f, sizeof(*f) - sizeof(f->crc32));
}

/* Richtet den Arbeitspuffer als leeren Block ein - ohne jeden Flash-Zugriff.
 * So kostet ein noch leeres Profil und ein noch nicht angelegter Block
 * nichts. */
static void index_set_empty(uint8_t profile, uint16_t block)
{
	memset(&g_idx, 0, sizeof(g_idx));
	g_idx.head.magic = OSK_IDX_MAGIC;
	g_idx.head.format = OSK_FMT_CURRENT;
	g_idx.head.count = 0u;
	g_idx.head.generation = 0u;
	g_idx.head.profile = profile;
	g_idx.head.block = block;
}

static osk_result_t index_read_block(uint8_t profile, uint16_t block)
{
	osk_store_state_t *st = osk_store_state();
	osk_idx_frame_t frame;
	osk_result_t r;
	uint32_t root = st->hdr.index_block[profile][block];

	if (root == 0u) {
		index_set_empty(profile, block);
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

	/* Die Fassung steht im verschluesselten Kopf des Blocks, nicht in den
	 * zusaetzlichen Daten. Nur so laesst sich eine Fassung lesen, ohne
	 * vorher zu wissen, welche es ist. */
	r = osk_crypto_open(&st->key_profile[profile], frame.nonce, aad, sizeof(aad), g_idx_ct,
			    frame.ct_len, (uint8_t *)&g_idx, sizeof(g_idx), &plain_len);
	if (r != OSK_OK) {
		/* Falscher Schluessel heisst: falsche PIN fuer dieses Profil. */
		return (r == OSK_ERR_FORMAT) ? OSK_ERR_DENIED : r;
	}
	if (plain_len < sizeof(osk_idx_head_t) || g_idx.head.magic != OSK_IDX_MAGIC ||
	    g_idx.head.profile != profile || g_idx.head.count > OSK_IDX_BLOCK_RECS) {
		return OSK_ERR_FORMAT;
	}
	/* Die Blocknummer im verschluesselten Kopf muss die erwartete sein.
	 * Sonst waere ein echter Block an die Stelle eines anderen geschoben
	 * worden - dieselbe Zusicherung, die sonst die zusaetzlichen Daten
	 * geben, nur hier nach dem Entschluesseln geprueft. In den Fassungen 1
	 * bis 3 stand dort eine 0, und dort gibt es nur Block 0. */
	if (g_idx.head.block != block) {
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
	case OSK_FMT_V4:
	case OSK_FMT_V5:
		/* Gleiche Satzgroesse seit Fassung 3. Fassung 5 benutzt nur
		 * Bytes, die vorher auf 0 standen - ein alter Satz liest sich
		 * darum als "kein oeffentlicher Teil, kein Chip-Schluessel"
		 * und muss nicht umgerechnet werden. */
		rec_size = sizeof(osk_idx_rec_t);
		break;
	default:
		return OSK_ERR_FORMAT;
	}
	if (plain_len != sizeof(osk_idx_head_t) + (size_t)g_idx.head.count * rec_size) {
		return OSK_ERR_FORMAT;
	}
	if (g_idx.head.format == OSK_FMT_V1 || g_idx.head.format == OSK_FMT_V2) {
		r = osk_index_upgrade(g_idx.head.format, plain_len);
		if (r != OSK_OK) {
			return r;
		}
	}
	g_idx.head.format = OSK_FMT_CURRENT;
	return OSK_OK;
}

osk_result_t osk_index_load_block(uint8_t profile, uint16_t block)
{
	osk_store_state_t *st = osk_store_state();
	osk_result_t r;

	if (profile >= OSK_PROFILE_MAX || block >= OSK_IDX_BLOCKS_MAX) {
		return OSK_ERR_ARG;
	}
	if (g_idx_profile == (int)profile && g_idx_block == (int)block) {
		return OSK_OK;
	}
	osk_index_drop();

	r = index_read_block(profile, block);
	if (r != OSK_OK) {
		osk_index_drop();
		return r;
	}
	g_idx_profile = (int)profile;
	g_idx_block = (int)block;

	/* Ein Tresor der Fassungen 1 bis 3 hatte keinen Eintragszaehler im
	 * Kopf: er hatte je Profil genau einen Block, und dessen Zahl ist die
	 * ganze Wahrheit. Sie wird hier nachgetragen. Fuer einen Tresor der
	 * Fassung 4 mit einem Block ist das dieselbe Zahl, also ein Nichts. */
	if (block == 0u && st->hdr.index_blocks[profile] <= 1u) {
		st->hdr.index_count[profile] = g_idx.head.count;
	}
	return OSK_OK;
}

osk_result_t osk_index_load(uint8_t profile)
{
	return osk_index_load_block(profile, 0u);
}

osk_result_t osk_index_rec(uint8_t profile, size_t index, osk_idx_rec_t **out)
{
	osk_result_t r;

	if (profile >= OSK_PROFILE_MAX || out == NULL) {
		return OSK_ERR_ARG;
	}
	if (index >= osk_index_count(profile)) {
		return OSK_ERR_NOT_FOUND;
	}
	r = osk_index_load_block(profile, (uint16_t)(index / OSK_IDX_BLOCK_RECS));
	if (r != OSK_OK) {
		return r;
	}

	size_t in_block = index % OSK_IDX_BLOCK_RECS;

	if (in_block >= g_idx.head.count) {
		/* Kopf und Block sind uneinig - das ist ein Formatfehler und
		 * kein leerer Platz. Geraten wird nichts. */
		return OSK_ERR_FORMAT;
	}
	*out = &g_idx.recs[in_block];
	return OSK_OK;
}

osk_result_t osk_index_store(uint8_t profile)
{
	osk_store_state_t *st = osk_store_state();
	osk_idx_frame_t frame;
	osk_result_t r;

	if (profile >= OSK_PROFILE_MAX || g_idx_profile != (int)profile || g_idx_block < 0) {
		return OSK_ERR_ARG;
	}
	if (g_idx.head.count > OSK_IDX_BLOCK_RECS) {
		return OSK_ERR_INTERNAL;
	}

	uint16_t block = (uint16_t)g_idx_block;

	g_idx.head.magic = OSK_IDX_MAGIC;
	/* Geschrieben wird immer die jetzige Fassung - damit wandert ein alter
	 * Tresor Profil fuer Profil mit, sobald etwas geaendert wird. */
	g_idx.head.format = OSK_FMT_CURRENT;
	g_idx.head.profile = profile;
	g_idx.head.block = block;
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

	r = osk_space_index_alloc(&addr);
	if (r != OSK_OK) {
		return r;
	}

	/* Seitenweise schreiben: erst der Rahmen samt Anfang des Kryptotextes,
	 * dann der Rest. Der Rahmen steht zuerst, aber gueltig wird der Block
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
	st->hdr.index_block[profile][block] = addr;
	if (block + 1u > st->hdr.index_blocks[profile]) {
		st->hdr.index_blocks[profile] = (uint8_t)(block + 1u);
	}
	return OSK_OK;
}

/* Schreibt den geladenen Block - oder laesst ihn wegfallen, wenn er leer
 * geworden ist. Block 0 bleibt immer bestehen, auch leer: an ihm haengt, dass
 * ein Profil ueberhaupt lesbar ist. */
static osk_result_t index_finish_block(uint8_t profile, uint16_t block)
{
	osk_store_state_t *st = osk_store_state();

	if (g_idx.head.count == 0u && block > 0u) {
		st->hdr.index_block[profile][block] = 0u;
		st->hdr.index_blocks[profile] = (uint8_t)block;
		return OSK_OK;
	}
	return osk_index_store(profile);
}

osk_result_t osk_index_append(uint8_t profile, osk_idx_rec_t **out)
{
	osk_store_state_t *st = osk_store_state();
	osk_result_t r;

	if (profile >= OSK_PROFILE_MAX || out == NULL) {
		return OSK_ERR_ARG;
	}

	size_t count = osk_index_count(profile);

	if (count >= OSK_ITEMS_MAX) {
		return OSK_ERR_FULL;
	}

	uint16_t block = (uint16_t)(count / OSK_IDX_BLOCK_RECS);
	size_t in_block = count % OSK_IDX_BLOCK_RECS;

	r = osk_index_load_block(profile, block);
	if (r != OSK_OK) {
		return r;
	}
	/* Bis auf den letzten sind alle Bloecke voll. Stimmt das nicht, ist
	 * der Zaehler im Kopf und der Block uneinig. */
	if (g_idx.head.count != in_block) {
		return OSK_ERR_FORMAT;
	}
	memset(&g_idx.recs[in_block], 0, sizeof(osk_idx_rec_t));
	g_idx.head.count = (uint16_t)(in_block + 1u);
	st->hdr.index_count[profile] = (uint16_t)(count + 1u);
	*out = &g_idx.recs[in_block];
	return OSK_OK;
}

osk_result_t osk_index_remove(uint8_t profile, size_t index)
{
	osk_store_state_t *st = osk_store_state();
	osk_idx_rec_t moved;
	osk_result_t r;

	if (profile >= OSK_PROFILE_MAX) {
		return OSK_ERR_ARG;
	}

	size_t count = osk_index_count(profile);

	if (index >= count) {
		return OSK_ERR_NOT_FOUND;
	}

	size_t last = count - 1u;
	uint16_t last_block = (uint16_t)(last / OSK_IDX_BLOCK_RECS);
	size_t last_in = last % OSK_IDX_BLOCK_RECS;
	uint16_t hole_block = (uint16_t)(index / OSK_IDX_BLOCK_RECS);
	size_t hole_in = index % OSK_IDX_BLOCK_RECS;

	/* Luecke schliessen: der letzte Satz rutscht nach vorn. Liegen die
	 * beiden in verschiedenen Bloecken, werden darum ZWEI Bloecke neu
	 * geschrieben - beide vor dem Kopfsatz, also beide zusammen gueltig. */
	r = osk_index_load_block(profile, last_block);
	if (r != OSK_OK) {
		return r;
	}
	if (last_in >= g_idx.head.count) {
		return OSK_ERR_FORMAT;
	}
	moved = g_idx.recs[last_in];
	memset(&g_idx.recs[last_in], 0, sizeof(osk_idx_rec_t));
	g_idx.head.count = (uint16_t)last_in;

	if (last_block == hole_block) {
		if (hole_in != last_in) {
			g_idx.recs[hole_in] = moved;
		}
		r = index_finish_block(profile, last_block);
	} else {
		r = index_finish_block(profile, last_block);
		if (r == OSK_OK) {
			r = osk_index_load_block(profile, hole_block);
		}
		if (r == OSK_OK) {
			if (hole_in >= g_idx.head.count) {
				r = OSK_ERR_FORMAT;
			} else {
				g_idx.recs[hole_in] = moved;
				r = osk_index_store(profile);
			}
		}
	}
	osk_crypto_wipe(&moved, sizeof(moved));
	if (r != OSK_OK) {
		return r;
	}
	st->hdr.index_count[profile] = (uint16_t)(count - 1u);
	return OSK_OK;
}

/* ---- Geheimnisse und oeffentliche Teile ------------------------------- */
/* Beide liegen auf Datenplaetzen desselben Bereichs und benutzen denselben
 * Rahmen - aber mit VERSCHIEDENER Kennung. Dadurch laesst sich der
 * oeffentliche Teil eines Eintrags nicht an die Stelle seines Geheimnisses
 * schieben und umgekehrt: der Rahmen ist zusaetzliche Daten der
 * Verschluesselung, und eine falsche Kennung darin laesst den Pruefwert
 * anschlagen. */

static uint32_t blob_crc(const osk_blob_frame_t *f)
{
	return osk_crc32(f, sizeof(*f) - sizeof(f->crc32));
}

static osk_result_t slot_write(uint32_t magic, uint32_t slot, osk_item_id_t id, const uint8_t *data,
			       size_t len, const osk_key_t *key, uint32_t *ct_len_out,
			       uint8_t nonce_out[OSK_NONCE_LEN])
{
	osk_store_state_t *st = osk_store_state();
	osk_blob_frame_t frame;
	osk_result_t r;

	if (slot >= OSK_BLOB_SLOTS || len == 0 || len > OSK_SECRET_MAX || data == NULL) {
		return OSK_ERR_ARG;
	}

	r = osk_nonce_take(OSK_NONCE_BLOB, nonce_out);
	if (r != OSK_OK) {
		return r;
	}

	memset(&frame, 0, sizeof(frame));
	frame.magic = magic;
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
			    sizeof(frame) - sizeof(frame.crc32), data, len, g_blob + sizeof(frame),
			    sizeof(g_blob) - sizeof(frame), &ct_len);
	if (r != OSK_OK) {
		osk_crypto_wipe(g_blob, sizeof(g_blob));
		return r;
	}

	uint32_t total = osk_round_page((uint32_t)(sizeof(frame) + ct_len));

	r = osk_space_ensure_erased(osk_blob_addr(slot), OSK_BLOB_SLOT);
	if (r == OSK_OK) {
		r = osk_flash_program(osk_blob_addr(slot), g_blob, total);
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

static osk_result_t slot_read(uint32_t magic, uint32_t slot, osk_item_id_t id, uint32_t want_ct_len,
			      const uint8_t nonce[OSK_NONCE_LEN], const osk_key_t *key,
			      uint8_t *out, size_t cap, size_t *len_out)
{
	osk_blob_frame_t frame;
	osk_result_t r;

	if (out == NULL || len_out == NULL || slot >= OSK_BLOB_SLOTS) {
		return OSK_ERR_ARG;
	}
	if (want_ct_len < OSK_TAG_LEN || want_ct_len > sizeof(g_blob) - sizeof(frame)) {
		return OSK_ERR_FORMAT;
	}

	r = osk_flash_read(osk_blob_addr(slot), &frame, sizeof(frame));
	if (r != OSK_OK) {
		return r;
	}
	if (frame.magic != magic || frame.crc32 != blob_crc(&frame) || frame.id != id ||
	    frame.ct_len != want_ct_len) {
		return OSK_ERR_FORMAT;
	}

	r = osk_flash_read(osk_blob_addr(slot) + sizeof(frame), g_blob, frame.ct_len);
	if (r != OSK_OK) {
		return r;
	}
	r = osk_crypto_open(key, nonce, (const uint8_t *)&frame,
			    sizeof(frame) - sizeof(frame.crc32), g_blob, frame.ct_len, out, cap,
			    len_out);
	osk_crypto_wipe(g_blob, sizeof(g_blob));
	return r;
}

osk_result_t osk_blob_write(uint32_t slot, osk_item_id_t id, const uint8_t *secret, size_t len,
			    const osk_key_t *key, uint32_t *ct_len_out,
			    uint8_t nonce_out[OSK_NONCE_LEN])
{
	return slot_write(OSK_BLOB_MAGIC, slot, id, secret, len, key, ct_len_out, nonce_out);
}

osk_result_t osk_blob_read(const osk_idx_rec_t *rec, const osk_key_t *key, uint8_t *out, size_t cap,
			   size_t *len_out)
{
	if (rec == NULL) {
		return OSK_ERR_ARG;
	}
	return slot_read(OSK_BLOB_MAGIC, rec->slot, rec->id, rec->blob_ct_len, rec->nonce, key, out,
			 cap, len_out);
}

/* Der oeffentliche Teil. Eigener Platz, eigene Nonce, eigene Kennung im
 * Rahmen - und darum lesbar, ohne den Platz des Geheimnisses anzufassen. */
osk_result_t osk_pub_write(uint32_t slot, osk_item_id_t id, const uint8_t *pub, size_t len,
			   const osk_key_t *key, uint32_t *ct_len_out,
			   uint8_t nonce_out[OSK_NONCE_LEN])
{
	if (len > OSK_PUBLIC_MAX) {
		return OSK_ERR_ARG;
	}
	return slot_write(OSK_PUB_MAGIC, slot, id, pub, len, key, ct_len_out, nonce_out);
}

osk_result_t osk_pub_read(const osk_idx_rec_t *rec, const osk_key_t *key, uint8_t *out, size_t cap,
			  size_t *len_out)
{
	if (rec == NULL) {
		return OSK_ERR_ARG;
	}
	/* Die Laenge entscheidet, nicht die Platznummer: Platz 0 ist ein
	 * gueltiger Platz. */
	if (rec->pub_len == 0u) {
		return OSK_ERR_NOT_FOUND;
	}
	return slot_read(OSK_PUB_MAGIC, rec->pub_slot, rec->id, rec->pub_ct_len, rec->pub_nonce,
			 key, out, cap, len_out);
}

osk_result_t osk_blob_erase(uint32_t slot)
{
	if (slot >= OSK_BLOB_SLOTS) {
		return OSK_ERR_ARG;
	}
	return osk_space_ensure_erased(osk_blob_addr(slot), OSK_BLOB_SLOT);
}
