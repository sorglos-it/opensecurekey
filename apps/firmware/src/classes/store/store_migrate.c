/* Wanderung von einer Formatfassung zur naechsten.
 *
 * Der Weg ist absichtlich so gebaut, dass kein eigener Schritt noetig ist:
 *  - Jede Indexfassung sagt in ihrem verschluesselten Kopf, welche Fassung sie
 *    hat. Eine alte Fassung wird beim Laden im RAM umgerechnet - der Tresor
 *    ist also schon nach dem Firmware-Update vollstaendig lesbar.
 *  - Geschrieben wird immer die jetzige Fassung. Wer etwas aendert, wandert
 *    nebenbei mit.
 *  - osk_store_migrate() tut dasselbe fuer alle Profile auf einmal, damit der
 *    alte Aufbau nicht unbegrenzt im Flash liegen bleibt.
 *
 * Die Schluessel haengen NICHT an der Formatfassung. Darum braucht die
 * Wanderung keine PIN und kein neues Ableiten - nur Umschreiben.
 *
 * Jedes Profil ist eine eigene Transaktion mit eigenem Kopfsatz. Ein
 * Stromausfall laesst also einen Tresor zurueck, in dem manche Profile schon
 * in der neuen und andere noch in der alten Fassung liegen - lesbar sind
 * beide. Die Fassung im Kopf steigt erst, wenn alle Profile umgeschrieben
 * sind. */

#include <string.h>
#include "store_internal.h"
#include "osk_store_profiles.h"

/* Ein Indexsatz der Fassung 1: ohne Fuellfeld, ohne HOTP-Zaehler und ohne
 * Kennung zum Suchen, also 240 Byte. Wird nur noch gelesen. */
typedef struct {
	uint32_t id;
	uint16_t type;
	uint8_t profile;
	uint8_t flags;
	uint64_t changed_utc;
	uint32_t slot;
	uint32_t blob_ct_len;
	uint32_t secret_len;
	uint8_t nonce[OSK_NONCE_LEN];
	char name[OSK_NAME_MAX];
	char site[OSK_SITE_MAX];
	uint8_t reserved[8];
} osk_idx_rec_v1_t;

OSK_STATIC_ASSERT(sizeof(osk_idx_rec_v1_t) == OSK_IDX_REC_V1_SIZE, rec_v1_240);

/* Ein Indexsatz der Fassung 2: wie Fassung 3, aber ohne die Kennung zum
 * Suchen, also 256 Byte. Wird nur noch gelesen. */
typedef struct {
	uint32_t id;
	uint16_t type;
	uint8_t profile;
	uint8_t flags;
	uint64_t changed_utc;
	uint32_t slot;
	uint32_t blob_ct_len;
	uint32_t secret_len;
	uint32_t pad0;
	uint64_t counter;
	uint8_t nonce[OSK_NONCE_LEN];
	char name[OSK_NAME_MAX];
	char site[OSK_SITE_MAX];
	uint8_t reserved[12];
} osk_idx_rec_v2_t;

OSK_STATIC_ASSERT(sizeof(osk_idx_rec_v2_t) == OSK_IDX_REC_V2_SIZE, rec_v2_256);

/* Die Saetze werden bei jedem Schritt laenger, und der Puffer ist derselbe.
 * Darum wird von hinten nach vorn umgerechnet - wer vorn anfaengt,
 * ueberschreibt sich selbst. */
static osk_result_t upgrade_v1(uint16_t count)
{
	osk_idx_page_t *page = osk_index_page();
	uint8_t *base = (uint8_t *)page->recs;

	for (int i = (int)count - 1; i >= 0; i--) {
		osk_idx_rec_v1_t old;
		osk_idx_rec_t neu;

		memcpy(&old, base + (size_t)i * OSK_IDX_REC_V1_SIZE, sizeof(old));

		memset(&neu, 0, sizeof(neu));
		neu.id = old.id;
		neu.type = old.type;
		neu.profile = old.profile;
		/* Fassung 1 kannte das Kennzeichen "schreibgeschuetzt" nicht;
		 * die beiden anderen Bits haben dieselbe Bedeutung. */
		neu.flags = (uint8_t)(old.flags & (OSK_FLAG_MIRRORED | OSK_FLAG_PAIRED_ONLY));
		neu.changed_utc = old.changed_utc;
		neu.slot = old.slot;
		neu.blob_ct_len = old.blob_ct_len;
		neu.secret_len = old.secret_len;
		neu.counter = 0u; /* HOTP faengt bei 0 an */
		neu.tag_len = 0u; /* keine Kennung zum Suchen */
		memcpy(neu.nonce, old.nonce, sizeof(neu.nonce));
		memcpy(neu.name, old.name, sizeof(neu.name));
		memcpy(neu.site, old.site, sizeof(neu.site));
		neu.name[sizeof(neu.name) - 1] = '\0';
		neu.site[sizeof(neu.site) - 1] = '\0';

		page->recs[i] = neu;
		osk_crypto_wipe(&old, sizeof(old));
	}
	return OSK_OK;
}

static osk_result_t upgrade_v2(uint16_t count)
{
	osk_idx_page_t *page = osk_index_page();
	uint8_t *base = (uint8_t *)page->recs;

	for (int i = (int)count - 1; i >= 0; i--) {
		osk_idx_rec_v2_t old;
		osk_idx_rec_t neu;

		memcpy(&old, base + (size_t)i * OSK_IDX_REC_V2_SIZE, sizeof(old));

		memset(&neu, 0, sizeof(neu));
		neu.id = old.id;
		neu.type = old.type;
		neu.profile = old.profile;
		neu.flags = old.flags;
		neu.changed_utc = old.changed_utc;
		neu.slot = old.slot;
		neu.blob_ct_len = old.blob_ct_len;
		neu.secret_len = old.secret_len;
		neu.counter = old.counter;
		/* Fassung 2 kannte keine Kennung zum Suchen. Sie bleibt leer;
		 * wer sie braucht (FIDO2), setzt sie beim naechsten Aendern.
		 * Geraten wird sie nicht - eine falsche Kennung waere
		 * schlimmer als keine. */
		neu.tag_len = 0u;
		memcpy(neu.nonce, old.nonce, sizeof(neu.nonce));
		memcpy(neu.name, old.name, sizeof(neu.name));
		memcpy(neu.site, old.site, sizeof(neu.site));
		neu.name[sizeof(neu.name) - 1] = '\0';
		neu.site[sizeof(neu.site) - 1] = '\0';

		page->recs[i] = neu;
		osk_crypto_wipe(&old, sizeof(old));
	}
	return OSK_OK;
}

osk_result_t osk_index_upgrade(uint16_t from_format, size_t plain_len)
{
	osk_idx_page_t *page = osk_index_page();
	uint16_t count = page->head.count;
	size_t rec_size;
	osk_result_t r;

	switch (from_format) {
	case OSK_FMT_V1:
		rec_size = OSK_IDX_REC_V1_SIZE;
		break;
	case OSK_FMT_V2:
		rec_size = OSK_IDX_REC_V2_SIZE;
		break;
	default:
		return OSK_ERR_FORMAT;
	}
	if (count > OSK_IDX_BLOCK_RECS ||
	    plain_len != sizeof(osk_idx_head_t) + (size_t)count * rec_size) {
		return OSK_ERR_FORMAT;
	}

	/* Je alte Fassung ein eigener Umrechner, und zwar direkt auf die
	 * jetzige. Das ist mehr Code als eine Kette, aber jeder Umrechner ist
	 * fuer sich lesbar und fuer sich pruefbar. */
	if (from_format == OSK_FMT_V1) {
		r = upgrade_v1(count);
		if (r != OSK_OK) {
			return r;
		}
	} else {
		r = upgrade_v2(count);
		if (r != OSK_OK) {
			return r;
		}
	}
	page->head.format = OSK_FMT_CURRENT;
	return OSK_OK;
}

osk_result_t osk_store_migrate(void)
{
	osk_store_state_t *st = osk_store_state();
	bool all_done = true;
	osk_result_t r;

	if (!st->open) {
		return OSK_ERR_LOCKED;
	}
	if (st->hdr.format == OSK_FMT_CURRENT) {
		return OSK_OK;
	}
	if (st->hdr.format < OSK_FMT_OLDEST_KNOWN || st->hdr.format > OSK_FMT_CURRENT) {
		return OSK_ERR_FORMAT;
	}

	for (uint8_t p = 0; p < OSK_PROFILE_MAX; p++) {
		if (st->hdr.profile_used[p] == 0u) {
			continue;
		}
		if (!st->profile_open[p]) {
			/* Ein gesperrtes Profil kann nicht umgeschrieben werden -
			 * ohne Schluessel kein Index. Es bleibt in der alten
			 * Fassung und ist weiter lesbar, sobald es
			 * aufgeschlossen wird. */
			all_done = false;
			continue;
		}
		osk_index_drop();
		/* Erst Block 0: dabei wird bei einem alten Tresor der
		 * Eintragszaehler im Kopf nachgetragen, und erst danach steht
		 * fest, wie viele Bloecke das Profil hat. */
		r = osk_index_load(p); /* rechnet alte Fassungen im RAM um */
		if (r != OSK_OK) {
			return r;
		}

		uint16_t blocks = st->hdr.index_blocks[p];

		for (uint16_t b = 0; b < blocks; b++) {
			r = osk_index_load_block(p, b);
			if (r != OSK_OK) {
				return r;
			}
			r = osk_index_store(p); /* schreibt die jetzige Fassung */
			if (r != OSK_OK) {
				return r;
			}
		}
		r = osk_hdr_commit();
		if (r != OSK_OK) {
			return r;
		}
	}

	if (!all_done) {
		return OSK_ERR_DENIED;
	}

	uint16_t keep = st->hdr.format;

	st->hdr.format = OSK_FMT_CURRENT;
	r = osk_hdr_commit();
	if (r != OSK_OK) {
		st->hdr.format = keep;
		return r;
	}
	(void)osk_log_add_internal(OSK_LOG_MIGRATE, 0, NULL);
	return OSK_OK;
}
