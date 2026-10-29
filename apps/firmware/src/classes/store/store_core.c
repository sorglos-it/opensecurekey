/* Kopfsatz-Ring, Nonce-Zaehler und Platzvergabe.
 *
 * Hier liegt die Grundlage aller Zusagen des Tresors:
 *  - Ein Kopfsatz gilt erst, wenn seine Pruefsumme stimmt. Damit ist jeder
 *    Wechsel atomar, auch wenn der Strom mitten im Schreiben ausfaellt.
 *  - Der Nonce-Zaehler wird festgehalten, BEVOR eine Nonce herausgegeben wird.
 *    Nach einem Stromausfall beginnt der Tresor beim festgehaltenen Wert und
 *    ueberspringt die Luecke - eine Nonce wird nie zweimal benutzt. */

#include <string.h>
#include "store_internal.h"
#include "osk_se050.h"

static osk_store_state_t g_state;
static uint64_t (*g_time_fn)(void);

osk_store_state_t *osk_store_state(void)
{
	return &g_state;
}

/* ---- Hilfen ----------------------------------------------------------- */

/* CRC-32 nach IEEE 802.3, rueckwaerts gerechnet ohne Tabelle - der Tresor
 * braucht ihn nur fuer wenige hundert Byte je Schreibvorgang, und so kostet
 * er keinen Platz im Flash der Firmware. */
uint32_t osk_crc32(const void *buf, size_t len)
{
	const uint8_t *p = (const uint8_t *)buf;
	uint32_t crc = 0xFFFFFFFFu;

	for (size_t i = 0; i < len; i++) {
		crc ^= p[i];
		for (int bit = 0; bit < 8; bit++) {
			uint32_t mask = (uint32_t)-(int32_t)(crc & 1u);
			crc = (crc >> 1) ^ (0xEDB88320u & mask);
		}
	}
	return ~crc;
}

void osk_nonce_build(uint64_t counter, osk_nonce_purpose_t purpose, uint8_t out[OSK_NONCE_LEN])
{
	for (int i = 0; i < 8; i++) {
		out[i] = (uint8_t)(counter >> (8 * i));
	}
	uint32_t p = (uint32_t)purpose;
	for (int i = 0; i < 4; i++) {
		out[8 + i] = (uint8_t)(p >> (8 * i));
	}
}

void osk_str_copy(char *dst, size_t cap, const char *src)
{
	size_t i = 0;

	if (cap == 0) {
		return;
	}
	if (src != NULL) {
		for (; i + 1 < cap && src[i] != '\0'; i++) {
			dst[i] = src[i];
		}
	}
	memset(dst + i, 0, cap - i);
}

uint64_t osk_now_utc(void)
{
	return (g_time_fn != NULL) ? g_time_fn() : 0u;
}

void osk_time_set_source(uint64_t (*fn)(void))
{
	g_time_fn = fn;
}

/* ---- Kopfsatz-Ring ---------------------------------------------------- */

static uint32_t hdr_crc(const osk_hdr_t *h)
{
	return osk_crc32(h, sizeof(*h) - sizeof(h->crc32));
}

static bool hdr_valid(const osk_hdr_t *h)
{
	return h->magic == OSK_HDR_MAGIC && h->hdr_size == sizeof(osk_hdr_t) &&
	       h->crc32 == hdr_crc(h);
}

/* ---- Kopf der Fassungen 1 bis 3 lesen -------------------------------- */

static bool hdr_v3_valid(const osk_hdr_v3_t *h)
{
	return h->magic == OSK_HDR_MAGIC && h->hdr_size == sizeof(osk_hdr_v3_t) &&
	       h->format <= OSK_FMT_V3 && h->crc32 == osk_crc32(h, sizeof(*h) - sizeof(h->crc32));
}

/* Rechnet einen alten Kopf im RAM auf die Fassung 4 um. Genau wie bei den
 * Indexsaetzen gilt: was die alte Fassung nicht hatte, bleibt leer, und
 * geraten wird nichts.
 *
 * Die EINE Indexfassung eines alten Profils wird Block 0 des Profils. Ihre
 * Adresse bleibt, wo sie ist - darum muss hier kein Byte umziehen. Die Anzahl
 * der Eintraege stand im alten Kopf nicht; sie wird beim ersten Laden des
 * Blocks nachgetragen (siehe osk_index_load). */
static void hdr_from_v3(const osk_hdr_v3_t *old, osk_hdr_t *out)
{
	memset(out, 0, sizeof(*out));
	out->magic = old->magic;
	out->format = old->format;
	out->hdr_size = (uint16_t)sizeof(*out);
	out->seq = old->seq;
	out->nonce_next = old->nonce_next;
	out->blob_cursor = old->blob_cursor;
	out->item_next = old->item_next;

	/* Die Platzvergabe im Index-Ring faengt vorn an. Die alten Bloecke
	 * stehen in index_block und werden dabei uebergangen. */
	out->index_alloc = 0u;

	for (int p = 0; p < OSK_PROFILE_MAX; p++) {
		out->profile_used[p] = old->profile_used[p];
		out->index_block[p][0] = old->index_root[p];
		out->index_blocks[p] = (old->index_root[p] != 0u) ? 1u : 0u;
		out->index_count[p] = 0u; /* wird beim Laden nachgetragen */
	}
	memcpy(out->slot_used, old->slot_used, sizeof(old->slot_used));
}

/* ---- Kopf der Fassungen 4 und 5 lesen -------------------------------- */

static bool hdr_v5_valid(const osk_hdr_v5_t *h)
{
	return h->magic == OSK_HDR_MAGIC && h->hdr_size == sizeof(osk_hdr_v5_t) &&
	       h->format <= OSK_FMT_V5 && h->crc32 == osk_crc32(h, sizeof(*h) - sizeof(h->crc32));
}

/* Rechnet einen Kopf der Fassung 4 oder 5 im RAM auf die jetzige Fassung um.
 * Gewachsen ist nur das Pruefmal der Wisch-PIN, und das gab es dort nicht -
 * es bleibt also leer, und geraten wird nichts. Die Saetze liegen im selben
 * Ring und in denselben Plaetzen: der Platz ist 1 KiB gross, beide Fassungen
 * passen hinein. */
static void hdr_from_v5(const osk_hdr_v5_t *old, osk_hdr_t *out)
{
	memset(out, 0, sizeof(*out));
	out->magic = old->magic;
	out->format = old->format;
	out->hdr_size = (uint16_t)sizeof(*out);
	out->seq = old->seq;
	out->sector_of_slot = old->sector_of_slot;
	out->nonce_next = old->nonce_next;
	memcpy(out->index_block, old->index_block, sizeof(out->index_block));
	memcpy(out->index_count, old->index_count, sizeof(out->index_count));
	memcpy(out->index_blocks, old->index_blocks, sizeof(out->index_blocks));
	out->index_alloc = old->index_alloc;
	out->blob_cursor = old->blob_cursor;
	out->item_next = old->item_next;
	memcpy(out->profile_used, old->profile_used, sizeof(out->profile_used));
	memcpy(out->slot_used, old->slot_used, sizeof(out->slot_used));
}

osk_result_t osk_hdr_mount(void)
{
	osk_hdr_t cand;
	bool found = false;
	osk_result_t r;

	memset(&g_state.hdr, 0, sizeof(g_state.hdr));
	g_state.hdr_slot = 0;

	/* Erst der Ring der Fassung 4. */
	for (uint32_t i = 0; i < OSK_HDR_SLOTS; i++) {
		uint32_t addr = OSK_AREA_HDR2_OFF + i * OSK_HDR_SLOT;

		r = osk_flash_read(addr, &cand, sizeof(cand));
		if (r != OSK_OK) {
			return r;
		}
		if (!hdr_valid(&cand)) {
			continue;
		}
		/* Der jueengste Satz gewinnt. Die Folgenummer waechst mit jedem
		 * Satz, also entscheidet sie allein - nicht die Lage im Flash. */
		if (!found || cand.seq > g_state.hdr.seq) {
			g_state.hdr = cand;
			g_state.hdr_slot = addr;
			found = true;
		}
	}

	/* Dann dieselben Plaetze noch einmal als Kopf der Fassungen 4 und 5.
	 * Ein Platz ist 1 KiB gross und traegt genau EINEN Satz, also kann
	 * hoechstens eine der beiden Pruefungen anschlagen. */
	for (uint32_t i = 0; i < OSK_HDR_SLOTS; i++) {
		uint32_t addr = OSK_AREA_HDR2_OFF + i * OSK_HDR_SLOT;
		osk_hdr_v5_t old;

		r = osk_flash_read(addr, &old, sizeof(old));
		if (r != OSK_OK) {
			return r;
		}
		if (!hdr_v5_valid(&old)) {
			continue;
		}
		if (!found || old.seq > g_state.hdr.seq) {
			hdr_from_v5(&old, &g_state.hdr);
			g_state.hdr_slot = addr;
			found = true;
		}
	}

	/* Dann die zwei alten Kopfsektoren. Ein alter Satz gewinnt nur, wenn
	 * seine Folgenummer hoeher ist - nach der Wanderung ist sie das nie,
	 * denn jeder neue Satz zaehlt sie hoch. */
	for (uint32_t i = 0; i < OSK_HDR_V3_SLOTS; i++) {
		uint32_t addr = OSK_AREA_HDR_OFF + i * OSK_HDR_V3_SLOT;
		osk_hdr_v3_t old;

		r = osk_flash_read(addr, &old, sizeof(old));
		if (r != OSK_OK) {
			return r;
		}
		if (!hdr_v3_valid(&old)) {
			continue;
		}
		if (!found || old.seq > g_state.hdr.seq) {
			hdr_from_v3(&old, &g_state.hdr);
			/* Kein Platz im neuen Ring: der naechste Kopfsatz sucht
			 * sich dort selbst den ersten. */
			g_state.hdr_slot = 0u;
			found = true;
		}
	}
	if (!found) {
		return OSK_ERR_NOT_FOUND;
	}

	/* Was zwischen dem letzten festgehaltenen Stand und jetzt an Nonces
	 * herausgegeben wurde, weiss niemand. Also gilt alles bis dorthin als
	 * verbraucht. */
	g_state.nonce_cur = g_state.hdr.nonce_next;
	return OSK_OK;
}

/* Naechster Platz: derselbe Sektor, solange er reicht, sonst der naechste im
 * Ring - dieser wird vorher geloescht. Der gueltige Satz bleibt dabei
 * unberuehrt, denn er steht in einem anderen Sektor. */
#define OSK_HDR_SECTORS (OSK_AREA_HDR2_LEN / OSK_SECTOR)

static osk_result_t hdr_next_slot(uint32_t *addr_out, uint32_t *sector_out)
{
	bool fresh = g_state.hdr_slot < OSK_AREA_HDR2_OFF ||
		     g_state.hdr_slot >= OSK_AREA_HDR2_OFF + OSK_AREA_HDR2_LEN;

	if (fresh) {
		/* Frisch angelegt oder gerade von einer alten Fassung
		 * uebernommen: der erste Platz des ersten Sektors. Geloescht
		 * wird er, weil dort Reste liegen koennen. */
		osk_result_t r = osk_space_ensure_erased(OSK_AREA_HDR2_OFF, OSK_SECTOR);

		if (r != OSK_OK) {
			return r;
		}
		*addr_out = OSK_AREA_HDR2_OFF;
		*sector_out = 0;
		return OSK_OK;
	}

	uint32_t sector = g_state.hdr.sector_of_slot % OSK_HDR_SECTORS;
	uint32_t base = OSK_AREA_HDR2_OFF + sector * OSK_SECTOR;
	uint32_t next = g_state.hdr_slot + OSK_HDR_SLOT;

	if (next < base + OSK_SECTOR) {
		*addr_out = next;
		*sector_out = sector;
		return OSK_OK;
	}

	uint32_t other = (sector + 1u) % OSK_HDR_SECTORS;
	uint32_t other_base = OSK_AREA_HDR2_OFF + other * OSK_SECTOR;
	osk_result_t r = osk_space_ensure_erased(other_base, OSK_SECTOR);

	if (r != OSK_OK) {
		return r;
	}
	*addr_out = other_base;
	*sector_out = other;
	return OSK_OK;
}

osk_result_t osk_hdr_commit(void)
{
	uint8_t page[OSK_HDR_SLOT];
	uint32_t addr, sector;
	osk_result_t r = hdr_next_slot(&addr, &sector);

	if (r != OSK_OK) {
		return r;
	}

	g_state.hdr.magic = OSK_HDR_MAGIC;
	g_state.hdr.hdr_size = (uint16_t)sizeof(osk_hdr_t);
	g_state.hdr.seq += 1u;
	g_state.hdr.sector_of_slot = sector;
	g_state.hdr.crc32 = hdr_crc(&g_state.hdr);

	/* Der Platz ist eine ganze Flash-Seite; der Rest bleibt 0xFF, damit
	 * nichts Fremdes hineingeschrieben wird. */
	memset(page, 0xFF, sizeof(page));
	memcpy(page, &g_state.hdr, sizeof(g_state.hdr));

	r = osk_flash_program(addr, page, sizeof(page));
	if (r != OSK_OK) {
		return r;
	}
	r = osk_flash_sync();
	if (r != OSK_OK) {
		return r;
	}
	g_state.hdr_slot = addr;

	/* Der Anker, und zwar JETZT und nicht vorher: im Flash steht die neue
	 * Folgenummer, also darf der Chip ihr folgen. Andersherum stuende im
	 * Chip nach einem Stromausfall ein Wert, den der Flash nie erreicht -
	 * und der Tresor liesse sich nie wieder oeffnen.
	 *
	 * Scheitert das Anheben, ist die Aenderung trotzdem gueltig: der Chip
	 * hinkt dann um einen Schritt hinterher, und osk_guard_check() holt
	 * ihn beim naechsten Oeffnen nach. Darum wird der Fehler hier nicht
	 * weitergereicht - er wuerde eine gelungene Transaktion
	 * zurueckdrehen, die im Flash schon steht. */
	(void)osk_se050_counter_raise(OSK_KEYID_VAULT_GEN, g_state.hdr.seq);
	return OSK_OK;
}

/* ---- Rueckdrehschutz -------------------------------------------------- */

osk_result_t osk_guard_check(void)
{
	uint64_t anchor = 0;
	osk_result_t r = osk_se050_counter_read(OSK_KEYID_VAULT_GEN, &anchor);

	if (r == OSK_ERR_NOT_FOUND) {
		/* Erstes Oeffnen auf diesem Chip: Zaehler anlegen und auf den
		 * Stand bringen, den der Flash schon hat. Der ANFANGSWERT
		 * eines frisch angelegten Zaehlers steht in AN12413 nicht -
		 * darum wird er gelesen und nicht angenommen. */
		r = osk_se050_counter_create(OSK_KEYID_VAULT_GEN);
		if (r != OSK_OK) {
			return r;
		}
		r = osk_se050_counter_read(OSK_KEYID_VAULT_GEN, &anchor);
	}
	if (r != OSK_OK) {
		return r;
	}

	if (g_state.hdr.seq < anchor) {
		/* Der Flash ist aelter als der Chip. Genau das ist der
		 * Angriff: ein in sich voellig gueltiger alter Zustand -
		 * richtige Pruefsummen, richtige Pruefwerte, richtiges Format.
		 * Gegen einen verfaelschten haette die Verschluesselung schon
		 * geholfen; hier hilft nur der Zaehler, der nicht mit
		 * zurueckgespielt werden kann. */
		return OSK_ERR_ROLLBACK;
	}

	/* Der Flash ist weiter als der Chip: der Strom fiel zwischen Kopfsatz
	 * und Anheben aus. Wiederholbar, also einfach nachholen. */
	if (g_state.hdr.seq > anchor) {
		return osk_se050_counter_raise(OSK_KEYID_VAULT_GEN, g_state.hdr.seq);
	}
	return OSK_OK;
}

osk_result_t osk_store_generation(uint64_t *value_out)
{
	if (value_out == NULL) {
		return OSK_ERR_ARG;
	}
	if (!g_state.open) {
		return OSK_ERR_LOCKED;
	}
	*value_out = g_state.hdr.seq;
	return OSK_OK;
}

osk_result_t osk_hdr_format(uint16_t format)
{
	osk_result_t r;

	/* Beide Kopfbereiche: der alte, damit kein Satz einer alten Fassung
	 * liegen bleibt und beim naechsten Oeffnen mitredet, und der neue. */
	r = osk_space_ensure_erased(OSK_AREA_HDR_OFF, OSK_AREA_HDR_LEN);
	if (r == OSK_OK) {
		r = osk_space_ensure_erased(OSK_AREA_HDR2_OFF, OSK_AREA_HDR2_LEN);
	}
	if (r != OSK_OK) {
		return r;
	}

	memset(&g_state.hdr, 0, sizeof(g_state.hdr));
	g_state.hdr.format = format;
	g_state.hdr.nonce_next = 1u; /* Nonce 0 wird nie benutzt */
	g_state.hdr.index_alloc = 0u;
	g_state.hdr.blob_cursor = 0u;
	g_state.hdr.item_next = 1u; /* Kennung 0 heisst "kein Eintrag" */

	/* Ein frischer Tresor faengt NICHT bei null an, sondern ueber dem
	 * Stand des Chipzaehlers. Sonst waere Loeschen der Weg zurueck: wer
	 * den Flash leert, bekaeme einen Tresor mit seq 1, und der Zaehler im
	 * Chip stuende auf tausend - entweder der Tresor liesse sich nie
	 * oeffnen, oder der Schutz waere weg. Gelesen wird der Stand, nicht
	 * angenommen: der Anfangswert eines frischen Zaehlers steht in
	 * AN12413 nicht. */
	uint64_t anchor = 0;
	osk_result_t cr = osk_se050_counter_read(OSK_KEYID_VAULT_GEN, &anchor);

	if (cr == OSK_ERR_NOT_FOUND) {
		cr = osk_se050_counter_create(OSK_KEYID_VAULT_GEN);
		if (cr == OSK_OK) {
			cr = osk_se050_counter_read(OSK_KEYID_VAULT_GEN, &anchor);
		}
	}
	if (cr != OSK_OK) {
		return cr;
	}
	/* Zwei Breiten, die dasselbe zaehlen: der Chipzaehler ist 64 Bit
	 * breit, die Folgenummer im Kopf 32. Stillschweigend abzuschneiden
	 * waere der Weg zurueck durch die Hintertuer - also lieber gar kein
	 * Tresor als einer mit kleinerer Nummer. Erreicht wird das nie: bei
	 * einem Kopfsatz je Sekunde dauert es 136 Jahre. */
	if (anchor >= 0xFFFFFFFFull) {
		return OSK_ERR_FULL;
	}
	g_state.hdr.seq = (uint32_t)anchor;
	g_state.hdr_slot = 0u;
	g_state.nonce_cur = 1u;

	r = osk_hdr_commit();
	if (r != OSK_OK) {
		return r;
	}
	g_state.nonce_cur = g_state.hdr.nonce_next;
	return OSK_OK;
}

/* ---- Transaktion zurueckdrehen --------------------------------------- */

void osk_hdr_snapshot(osk_hdr_t *out)
{
	*out = g_state.hdr;
}

/* Scheitert eine Transaktion, muss der Zustand im RAM wieder der alte sein -
 * sonst veroeffentlicht der naechste Kopfsatz eine halbe Aenderung.
 *
 * Zwei Felder duerfen dabei NICHT zurueckfallen:
 *  - nonce_next, denn der hoehere Wert steht schon im Flash und die Nonces
 *    darunter gelten als verbraucht
 *  - seq, denn zwei Kopfsaetze mit derselben Folgenummer waeren nicht mehr
 *    zu unterscheiden */
void osk_hdr_restore(const osk_hdr_t *snap)
{
	uint64_t nonce_next = g_state.hdr.nonce_next;
	uint32_t seq = g_state.hdr.seq;

	g_state.hdr = *snap;
	if (g_state.hdr.nonce_next < nonce_next) {
		g_state.hdr.nonce_next = nonce_next;
	}
	if (g_state.hdr.seq < seq) {
		g_state.hdr.seq = seq;
	}
}

/* ---- Nonce ------------------------------------------------------------ */

osk_result_t osk_nonce_take(osk_nonce_purpose_t purpose, uint8_t out[OSK_NONCE_LEN])
{
	if (g_state.nonce_cur >= g_state.hdr.nonce_next) {
		/* Vorrat erschoepft: neuen Bereich festhalten, bevor eine
		 * weitere Nonce das Haus verlaesst. Fehlt der Kopfsatz, gibt es
		 * keine Nonce - lieber kein Schreiben als eine doppelte. */
		uint64_t want = g_state.nonce_cur + OSK_NONCE_RESERVE;
		uint64_t keep = g_state.hdr.nonce_next;

		g_state.hdr.nonce_next = want;
		osk_result_t r = osk_hdr_commit();

		if (r != OSK_OK) {
			g_state.hdr.nonce_next = keep;
			return r;
		}
	}
	osk_nonce_build(g_state.nonce_cur, purpose, out);
	g_state.nonce_cur += 1u;
	return OSK_OK;
}

/* ---- Platz ------------------------------------------------------------ */

osk_result_t osk_space_ensure_erased(uint32_t addr, uint32_t len)
{
	uint8_t buf[OSK_PAGE];

	/* Ein Sektor kann aus einem abgebrochenen Loeschvorgang halb geloescht
	 * stammen. Darum wird nicht geglaubt, sondern nachgesehen. */
	for (uint32_t off = 0; off < len; off += OSK_SECTOR) {
		bool clean = true;

		for (uint32_t p = 0; p < OSK_SECTOR && clean; p += OSK_PAGE) {
			osk_result_t r = osk_flash_read(addr + off + p, buf, sizeof(buf));
			if (r != OSK_OK) {
				return r;
			}
			for (uint32_t i = 0; i < sizeof(buf); i++) {
				if (buf[i] != 0xFFu) {
					clean = false;
					break;
				}
			}
		}
		if (clean) {
			continue;
		}
		osk_result_t r = osk_flash_erase(addr + off, OSK_SECTOR);

		if (r != OSK_OK) {
			return r;
		}
		r = osk_flash_sync();
		if (r != OSK_OK) {
			return r;
		}
	}
	return OSK_OK;
}

/* Liegen [a, a+alen) und [b, b+blen) uebereinander? */
static bool overlaps(uint32_t a, uint32_t alen, uint32_t b, uint32_t blen)
{
	return a < b + blen && b < a + alen;
}

/* Der Index-Ring besteht aus zwei Abschnitten (siehe die Einteilung in
 * osk_vault_format.h). Beide sind in gleich grosse Plaetze von einem Block
 * geteilt und werden als EIN Ring hintereinander durchlaufen. */
#define OSK_IDX_PLACES_SEG0 (OSK_AREA_IDX_LEN / OSK_IDX_BLOCK_SPAN)
#define OSK_IDX_PLACES_SEG1 (OSK_AREA_IDX2_LEN / OSK_IDX_BLOCK_SPAN)
#define OSK_IDX_PLACES (OSK_IDX_PLACES_SEG0 + OSK_IDX_PLACES_SEG1)

static uint32_t idx_place_addr(uint32_t place)
{
	if (place < OSK_IDX_PLACES_SEG0) {
		return OSK_AREA_IDX_OFF + place * OSK_IDX_BLOCK_SPAN;
	}
	return OSK_AREA_IDX2_OFF + (place - OSK_IDX_PLACES_SEG0) * OSK_IDX_BLOCK_SPAN;
}

/* Liegt an dieser Stelle ein Block, der noch gilt? Geprueft wird gegen ALLE
 * Profile, auch gesperrte - darum stehen die Adressen im Klartext-Kopf.
 *
 * Ein Block, der von einer alten Fassung uebernommen wurde, liegt nicht auf
 * einem Platz dieses Rasters. Er wird mit der vollen Blockgroesse gesperrt;
 * das ist mehr, als er wirklich braucht, und darum sicher. */
static bool idx_place_taken(uint32_t addr)
{
	for (int p = 0; p < OSK_PROFILE_MAX; p++) {
		for (uint32_t b = 0; b < OSK_IDX_BLOCKS_MAX; b++) {
			uint32_t root = g_state.hdr.index_block[p][b];

			if (root == 0u) {
				continue;
			}
			if (overlaps(addr, OSK_IDX_BLOCK_SPAN, root, OSK_IDX_BLOCK_SPAN)) {
				return true;
			}
		}
	}
	return false;
}

osk_result_t osk_space_index_alloc(uint32_t *addr_out)
{
	uint32_t cur = g_state.hdr.index_alloc % OSK_IDX_PLACES;

	if (addr_out == NULL) {
		return OSK_ERR_ARG;
	}

	/* Hoechstens so viele Versuche, wie der Ring Plaetze hat - danach ist
	 * der Ring wirklich voll und nicht nur ungluecklich belegt. */
	for (uint32_t t = 0; t < OSK_IDX_PLACES; t++) {
		uint32_t place = (cur + t) % OSK_IDX_PLACES;
		uint32_t addr = idx_place_addr(place);

		if (idx_place_taken(addr)) {
			continue;
		}
		osk_result_t r = osk_space_ensure_erased(addr, OSK_IDX_BLOCK_SPAN);

		if (r != OSK_OK) {
			return r;
		}
		*addr_out = addr;
		/* Der Suchzeiger bleibt nie stehen: so wird ein gerade
		 * freigewordener Platz nicht gleich wieder genommen. */
		g_state.hdr.index_alloc = (place + 1u) % OSK_IDX_PLACES;
		return OSK_OK;
	}
	return OSK_ERR_FULL;
}

bool osk_space_slot_is_used(uint32_t slot)
{
	if (slot >= OSK_BLOB_SLOTS) {
		return true;
	}
	return (g_state.hdr.slot_used[slot / 8u] & (1u << (slot % 8u))) != 0u;
}

void osk_space_slot_set(uint32_t slot, bool used)
{
	if (slot >= OSK_BLOB_SLOTS) {
		return;
	}
	if (used) {
		g_state.hdr.slot_used[slot / 8u] |= (uint8_t)(1u << (slot % 8u));
	} else {
		g_state.hdr.slot_used[slot / 8u] &= (uint8_t)~(1u << (slot % 8u));
	}
}

osk_result_t osk_space_slot_alloc(uint32_t *slot_out)
{
	/* Der Suchzeiger laeuft im Kreis und bleibt nie stehen. So wird ein
	 * gerade freigewordener Platz nicht gleich wieder genommen, und der
	 * Verschleiss verteilt sich ueber alle Plaetze - das ist wichtig,
	 * weil ein Tresor immer dieselben paar Eintraege neu schreibt. */
	uint32_t cur = g_state.hdr.blob_cursor % OSK_BLOB_SLOTS;

	for (uint32_t i = 0; i < OSK_BLOB_SLOTS; i++) {
		uint32_t slot = (cur + i) % OSK_BLOB_SLOTS;

		if (!osk_space_slot_is_used(slot)) {
			g_state.hdr.blob_cursor = (slot + 1u) % OSK_BLOB_SLOTS;
			*slot_out = slot;
			return OSK_OK;
		}
	}
	return OSK_ERR_FULL;
}
