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

void osk_nonce_build(uint64_t counter, osk_nonce_purpose_t purpose,
		     uint8_t out[OSK_NONCE_LEN])
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

osk_result_t osk_hdr_mount(void)
{
	osk_hdr_t cand;
	bool found = false;
	osk_result_t r;

	memset(&g_state.hdr, 0, sizeof(g_state.hdr));
	g_state.hdr_slot = 0;

	for (uint32_t i = 0; i < OSK_HDR_SLOTS; i++) {
		uint32_t addr = OSK_AREA_HDR_OFF + i * OSK_HDR_SLOT;

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
	if (!found) {
		return OSK_ERR_NOT_FOUND;
	}

	/* Was zwischen dem letzten festgehaltenen Stand und jetzt an Nonces
	 * herausgegeben wurde, weiss niemand. Also gilt alles bis dorthin als
	 * verbraucht. */
	g_state.nonce_cur = g_state.hdr.nonce_next;
	return OSK_OK;
}

/* Naechster Platz: derselbe Sektor, solange er reicht, sonst der andere -
 * dieser wird vorher geloescht. Der gueltige Satz bleibt dabei unberuehrt. */
static osk_result_t hdr_next_slot(uint32_t *addr_out, uint32_t *sector_out)
{
	uint32_t sector = g_state.hdr.sector_of_slot & 1u;
	uint32_t base = OSK_AREA_HDR_OFF + sector * OSK_SECTOR;
	uint32_t next = g_state.hdr_slot + OSK_HDR_SLOT;

	if (g_state.hdr_slot == 0 && g_state.hdr.seq == 0) {
		/* frisch angelegt: erster Platz im ersten Sektor */
		*addr_out = OSK_AREA_HDR_OFF;
		*sector_out = 0;
		return OSK_OK;
	}
	if (next < base + OSK_SECTOR) {
		*addr_out = next;
		*sector_out = sector;
		return OSK_OK;
	}

	uint32_t other = sector ^ 1u;
	uint32_t other_base = OSK_AREA_HDR_OFF + other * OSK_SECTOR;
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
	return OSK_OK;
}

osk_result_t osk_hdr_format(uint16_t format)
{
	osk_result_t r;

	r = osk_space_ensure_erased(OSK_AREA_HDR_OFF, OSK_AREA_HDR_LEN);
	if (r != OSK_OK) {
		return r;
	}

	memset(&g_state.hdr, 0, sizeof(g_state.hdr));
	g_state.hdr.format = format;
	g_state.hdr.nonce_next = 1u;   /* Nonce 0 wird nie benutzt */
	g_state.hdr.index_alloc = OSK_AREA_IDX_OFF;
	g_state.hdr.blob_cursor = 0u;
	g_state.hdr.item_next = 1u;    /* Kennung 0 heisst "kein Eintrag" */
	g_state.hdr.seq = 0u;
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

osk_result_t osk_nonce_take(osk_nonce_purpose_t purpose,
			    uint8_t out[OSK_NONCE_LEN])
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
			osk_result_t r = osk_flash_read(addr + off + p, buf,
							sizeof(buf));
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

osk_result_t osk_space_index_alloc(uint32_t len, uint32_t *addr_out)
{
	uint32_t need = (osk_round_page(len) + OSK_SECTOR - 1u) &
			~(OSK_SECTOR - 1u);
	uint32_t start = g_state.hdr.index_alloc;

	if (need == 0u || need > OSK_AREA_IDX_LEN) {
		return OSK_ERR_ARG;
	}
	if (start < OSK_AREA_IDX_OFF ||
	    start >= OSK_AREA_IDX_OFF + OSK_AREA_IDX_LEN) {
		start = OSK_AREA_IDX_OFF;
	}
	start &= ~(OSK_SECTOR - 1u);

	/* Hoechstens so viele Versuche, wie der Ring Sektoren hat - danach ist
	 * der Ring wirklich voll und nicht nur ungluecklich belegt. */
	uint32_t tries = OSK_AREA_IDX_LEN / OSK_SECTOR;

	for (uint32_t t = 0; t < tries; t++) {
		if (start + need > OSK_AREA_IDX_OFF + OSK_AREA_IDX_LEN) {
			start = OSK_AREA_IDX_OFF;
		}
		bool blocked = false;

		for (int p = 0; p < OSK_PROFILE_MAX; p++) {
			uint32_t root = g_state.hdr.index_root[p];

			if (root == 0u) {
				continue;
			}
			if (overlaps(start, need, root,
				     g_state.hdr.index_len[p] == 0u
					     ? OSK_SECTOR
					     : g_state.hdr.index_len[p])) {
				blocked = true;
				/* hinter die gueltige Fassung springen */
				uint32_t end = root + g_state.hdr.index_len[p];

				start = (end + OSK_SECTOR - 1u) &
					~(OSK_SECTOR - 1u);
				break;
			}
		}
		if (blocked) {
			continue;
		}
		osk_result_t r = osk_space_ensure_erased(start, need);

		if (r != OSK_OK) {
			return r;
		}
		*addr_out = start;
		g_state.hdr.index_alloc = start + need;
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
		g_state.hdr.slot_used[slot / 8u] &=
			(uint8_t)~(1u << (slot % 8u));
	}
}

osk_result_t osk_space_slot_alloc(uint32_t *slot_out)
{
	/* Der Suchzeiger laeuft im Kreis und bleibt nie stehen. So wird ein
	 * gerade freigewordener Platz nicht gleich wieder genommen, und der
	 * Verschleiss verteilt sich ueber alle 1024 Plaetze - das ist wichtig,
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
