/* Der Merkle-Baum des Datentraegers: Aufbau, Groesse, Pfadkosten.
 *
 * Das Besondere, und der Grund fuer die Wahl von ChaCha20-Poly1305: die
 * BLAETTER SIND NICHT DIE DATEN, SONDERN DIE STEMPEL. Der Stempel ist beim
 * Entschluesseln ohnehin geprueft; ihn noch einmal zu hashen waere doppelte
 * Arbeit an der Stelle, an der der Durchsatz klemmt.
 *
 * Alles hier ist reine Rechnung ohne Karte und ohne Zustand - darum laesst es
 * sich pruefen, ohne dass es eine Karte gibt. Und es gibt keine. */

#include <string.h>
#include "osk_volume.h"
#include "volume_internal.h"

/* Wie viele Baumseiten die Spitze im RAM halten darf. 4 Seiten sind 16 KiB.
 * Mit Datenblock, Stempelseite und einer Ebene-1-Seite kommt der Datentraeger
 * damit auf 28 KiB von 256 KiB - gerechnet, nicht gemessen. */
#define OSK_VOL_TREE_CACHE_PAGES 4u

static uint32_t up(uint32_t n, uint32_t per)
{
	return (n + per - 1u) / per;
}

osk_result_t osk_vol_layout(uint32_t data_blocks, osk_vol_layout_t *out)
{
	uint32_t pages;
	uint32_t page;
	uint32_t sum;
	uint32_t i;

	if (out == NULL || data_blocks == 0u) {
		return OSK_ERR_ARG;
	}
	memset(out, 0, sizeof(*out));
	out->data_blocks = data_blocks;
	out->leaf_pages = up(data_blocks, OSK_VOL_LEAVES_PER_PAGE);

	/* Ebenen, bis eine einzige Seite uebrig ist. Eine Ebene gibt es immer,
	 * auch ueber einer einzigen Stempelseite - sonst waere der Wurzelhash
	 * der Hash der Stempelseite, und der Baum haette keine Spitze, die
	 * sich im RAM halten laesst. */
	pages = out->leaf_pages;
	do {
		if (out->levels >= OSK_VOL_LEVELS_MAX) {
			return OSK_ERR_FULL; /* Karte zu gross fuer diesen Baum */
		}
		pages = up(pages, OSK_VOL_FANOUT);
		out->level_pages[out->levels] = pages;
		out->levels++;
	} while (pages > 1u);

	/* Welche obersten Ebenen in den RAM passen. Sie kosten beim Lesen
	 * keinen Zugriff auf die Karte - genau dafuer ist die Verzweigung da. */
	sum = 0u;
	out->cached_levels = 0u;
	for (i = out->levels; i > 0u; i--) {
		sum += out->level_pages[i - 1u];
		if (sum > OSK_VOL_TREE_CACHE_PAGES) {
			break;
		}
		out->cached_levels++;
	}

	/* Reihenfolge auf der Karte: zwei Wurzelsaetze, Stempelfeld, Ebenen,
	 * dann die Nutzdaten. Die Wurzelsaetze stehen vorn, weil sie beim
	 * Einstecken als erstes gelesen werden. */
	out->root_page[0] = 0u;
	out->root_page[1] = 1u;
	page = 2u;
	out->first_leaf_page = page;
	page += out->leaf_pages;
	for (i = 0u; i < out->levels; i++) {
		out->first_level_page[i] = page;
		page += out->level_pages[i];
	}
	out->first_data_page = page;
	page += data_blocks;
	out->total_pages = page;
	return OSK_OK;
}

uint32_t osk_vol_path_reads(const osk_vol_layout_t *l)
{
	if (l == NULL) {
		return 0u;
	}
	/* Der Datenblock selbst, seine Stempelseite, und je nicht gemerkter
	 * Ebene eine Seite. */
	return 2u + (l->levels - l->cached_levels);
}

uint32_t osk_vol_path_hash_bytes(const osk_vol_layout_t *l)
{
	if (l == NULL) {
		return 0u;
	}
	/* Gehasht wird jede gelesene BAUMSEITE. Der Datenblock wird nicht
	 * gehasht - sein Stempel ist das Blatt, und den prueft das
	 * Entschluesseln. Das ist die Ersparnis. */
	return (1u + (l->levels - l->cached_levels)) * OSK_VOL_BLOCK_LEN;
}

uint64_t osk_volume_full_check_ms(const osk_vol_layout_t *l, uint32_t hash_bytes_per_s)
{
	uint64_t bytes;

	if (l == NULL || hash_bytes_per_s == 0u) {
		return 0u;
	}
	/* Vollstaendig heiszt: jeder Datenblock und der ganze Baum. Nur das
	 * Hashen; Lesen von der Karte und Entschluesseln kommen hinzu und
	 * haengen an Hardware, die es nicht gibt. */
	bytes = (uint64_t)l->total_pages * OSK_VOL_BLOCK_LEN;
	return (bytes * 1000u) / hash_bytes_per_s;
}

/* --- Blattzugriff ------------------------------------------------------ */

uint32_t vol_leaf_page_of(const osk_vol_layout_t *l, uint32_t block)
{
	return l->first_leaf_page + block / OSK_VOL_LEAVES_PER_PAGE;
}

uint32_t vol_leaf_offset_of(uint32_t block)
{
	return (block % OSK_VOL_LEAVES_PER_PAGE) * OSK_VOL_LEAF_ENTRY_LEN;
}

/* --- Modus und Aussagen ----------------------------------------------- */

bool osk_vol_mode_secret(osk_vol_mode_t mode)
{
	return mode == OSK_VOL_MODE_DEVICE || mode == OSK_VOL_MODE_WORD;
}

const char *osk_vol_mode_text(osk_vol_mode_t mode)
{
	switch (mode) {
	case OSK_VOL_MODE_DEVICE:
		return "verschluesselt - nur dieser Stick";
	case OSK_VOL_MODE_WORD:
		return "verschluesselt - mit Kartenwort";
	case OSK_VOL_MODE_SEALED:
		/* Das Wort OFFEN steht vorn, weil Menschen den Anfang lesen. */
		return "OFFEN - nur gesiegelt, nicht geheim";
	default:
		return "unbekannte Betriebsart";
	}
}

const char *osk_vol_claim_text(osk_vol_claim_t claim)
{
	switch (claim) {
	case OSK_VOL_CLAIM_ROOT_SEALED:
		return "Siegel gueltig";
	case OSK_VOL_CLAIM_ROOT_BAD:
		return "Siegel ungueltig";
	case OSK_VOL_CLAIM_WILL_CHECK:
		return "Daten werden beim Lesen geprueft";
	case OSK_VOL_CLAIM_BLOCK_OK:
		/* "dieser Block", nicht "die Karte". Der Unterschied ist der
		 * ganze Punkt: die Karte ganz zu pruefen hiesse, sie ganz zu
		 * lesen. */
		return "dieser Block geprueft";
	case OSK_VOL_CLAIM_BLOCK_BAD:
		return "Block nicht gueltig - Lesen abgebrochen";
	case OSK_VOL_CLAIM_OLDER_SEEN:
		return "aelterer Stand als zuletzt gesehen";
	case OSK_VOL_CLAIM_FULL_CHECKED:
		return "vollstaendig geprueft";
	default:
		return "";
	}
}
