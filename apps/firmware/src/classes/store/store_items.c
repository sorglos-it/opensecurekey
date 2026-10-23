/* Die Eintraege des Tresors: anlegen, aendern, loeschen, herausgeben.
 *
 * Jede Aenderung ist eine Transaktion in der Reihenfolge aus store_space.c.
 * Kein Weg hier gibt ein Geheimnis heraus, ohne vorher osk_ui_confirm zu
 * fragen und den Zugriff ins Geraeteprotokoll zu schreiben. */

#include <string.h>
#include "store_internal.h"
#include "osk_store_profiles.h"
#include "osk_se050.h"
#include "osk_ui.h"

#define OSK_REVEAL_TIMEOUT_MS 30000u

/* Die Ableitungstexte tragen KEINE Formatfassung. Sonst braeuchte jede
 * Wanderung die PIN aller Profile, um neue Schluessel zu bilden. */
static const char INFO_DEV[] = "osk/vault/device";
static const char INFO_LOG[] = "osk/vault/log";
static const char INFO_PROFILE[] = "osk/vault/profile";

/* ---- Schluessel ------------------------------------------------------- */

/* Der geraetegebundene Wert kommt aus dem SE050 und verlaesst ihn nur als
 * Ableitung. Ohne Chip gibt es keinen Tresor. */
static osk_result_t derive_device_key(void)
{
	osk_store_state_t *st = osk_store_state();
	osk_result_t r = osk_se050_hkdf(OSK_KEYID_VAULT_ROOT,
					(const uint8_t *)INFO_DEV,
					sizeof(INFO_DEV) - 1, st->key_dev.b,
					sizeof(st->key_dev.b));
	if (r != OSK_OK) {
		osk_crypto_wipe(&st->key_dev, sizeof(st->key_dev));
		return r;
	}
	return osk_crypto_hkdf(st->key_dev.b, sizeof(st->key_dev.b), NULL, 0,
			       (const uint8_t *)INFO_LOG, sizeof(INFO_LOG) - 1,
			       st->key_log.b, sizeof(st->key_log.b));
}

static osk_result_t derive_profile_key(uint8_t profile, const uint8_t *pin,
				       size_t pin_len, osk_key_t *out)
{
	osk_store_state_t *st = osk_store_state();
	uint8_t pin_hash[OSK_HASH_LEN];
	uint8_t info[sizeof(INFO_PROFILE) + 1];
	osk_result_t r;

	r = osk_crypto_sha256(pin, pin_len, pin_hash);
	if (r != OSK_OK) {
		return r;
	}
	memcpy(info, INFO_PROFILE, sizeof(INFO_PROFILE) - 1);
	info[sizeof(INFO_PROFILE) - 1] = (uint8_t)('0' + profile);

	r = osk_crypto_hkdf(st->key_dev.b, sizeof(st->key_dev.b), pin_hash,
			    sizeof(pin_hash), info, sizeof(info) - 1, out->b,
			    sizeof(out->b));
	osk_crypto_wipe(pin_hash, sizeof(pin_hash));
	return r;
}

/* ---- Oeffnen und Sperren ---------------------------------------------- */

osk_result_t osk_store_open(void)
{
	osk_store_state_t *st = osk_store_state();
	osk_result_t r;

	if (st->open) {
		return OSK_OK;
	}
	osk_flash_geometry_t geo;

	r = osk_flash_geometry(&geo);
	if (r != OSK_OK) {
		return r;
	}
	if (geo.sector_size != OSK_SECTOR || geo.page_size != OSK_PAGE ||
	    geo.size < OSK_VAULT_SIZE) {
		return OSK_ERR_UNSUPPORTED;
	}

	r = derive_device_key();
	if (r != OSK_OK) {
		return r;
	}

	osk_log_reset_mount();
	r = osk_hdr_mount();
	if (r == OSK_ERR_NOT_FOUND) {
		/* Leeres Flash: frischer Tresor in der jetzigen Fassung. */
		r = osk_hdr_format(OSK_FMT_CURRENT);
	}
	if (r != OSK_OK) {
		osk_crypto_wipe(&st->key_dev, sizeof(st->key_dev));
		osk_crypto_wipe(&st->key_log, sizeof(st->key_log));
		return r;
	}
	if (st->hdr.format < OSK_FMT_OLDEST_KNOWN ||
	    st->hdr.format > OSK_FMT_CURRENT) {
		/* Eine Fassung, die diese Firmware nicht kennt: lieber nicht
		 * oeffnen als falsch lesen. */
		osk_crypto_wipe(&st->key_dev, sizeof(st->key_dev));
		osk_crypto_wipe(&st->key_log, sizeof(st->key_log));
		return OSK_ERR_FORMAT;
	}

	memset(st->profile_open, 0, sizeof(st->profile_open));
	st->open = true;
	(void)osk_log_add_internal(OSK_LOG_OPEN, 0, NULL);
	return OSK_OK;
}

void osk_store_lock(void)
{
	osk_store_state_t *st = osk_store_state();

	if (st->open) {
		(void)osk_log_add_internal(OSK_LOG_LOCK, 0, NULL);
	}
	osk_index_drop();
	osk_crypto_wipe(st->key_profile, sizeof(st->key_profile));
	osk_crypto_wipe(&st->key_log, sizeof(st->key_log));
	osk_crypto_wipe(&st->key_dev, sizeof(st->key_dev));
	memset(st->profile_open, 0, sizeof(st->profile_open));
	st->open = false;
}

bool osk_store_is_open(void)
{
	return osk_store_state()->open;
}

/* ---- Profile ---------------------------------------------------------- */

bool osk_store_profile_exists(uint8_t profile)
{
	osk_store_state_t *st = osk_store_state();

	return st->open && profile < OSK_PROFILE_MAX &&
	       st->hdr.profile_used[profile] != 0u;
}

bool osk_store_profile_is_unlocked(uint8_t profile)
{
	osk_store_state_t *st = osk_store_state();

	return st->open && profile < OSK_PROFILE_MAX &&
	       st->profile_open[profile];
}

void osk_store_profile_lock(uint8_t profile)
{
	osk_store_state_t *st = osk_store_state();

	if (profile >= OSK_PROFILE_MAX) {
		return;
	}
	if (osk_index_loaded_profile() == (int)profile) {
		osk_index_drop();
	}
	osk_crypto_wipe(&st->key_profile[profile],
			sizeof(st->key_profile[profile]));
	st->profile_open[profile] = false;
}

osk_result_t osk_store_profile_create(uint8_t profile, const uint8_t *pin,
				      size_t pin_len)
{
	osk_store_state_t *st = osk_store_state();
	osk_result_t r;

	if (!st->open) {
		return OSK_ERR_LOCKED;
	}
	if (profile >= OSK_PROFILE_MAX || pin == NULL ||
	    pin_len < OSK_PIN_MIN || pin_len > OSK_PIN_MAX) {
		return OSK_ERR_ARG;
	}
	if (st->hdr.profile_used[profile] != 0u) {
		return OSK_ERR_EXISTS;
	}

	osk_hdr_t snap;

	osk_hdr_snapshot(&snap);
	r = derive_profile_key(profile, pin, pin_len, &st->key_profile[profile]);
	if (r != OSK_OK) {
		return r;
	}
	st->profile_open[profile] = true;

	/* Leere Indexfassung schreiben, damit das Profil ab jetzt lesbar ist.
	 * osk_index_load richtet den Arbeitspuffer fuer ein noch leeres Profil
	 * ohne Flash-Zugriff ein. */
	osk_index_drop();
	r = osk_index_load(profile);
	if (r == OSK_OK) {
		r = osk_index_store(profile);
	}
	if (r != OSK_OK) {
		osk_hdr_restore(&snap);
		osk_index_drop();
		osk_store_profile_lock(profile);
		return r;
	}
	st->hdr.profile_used[profile] = 1u;
	r = osk_hdr_commit();
	if (r != OSK_OK) {
		osk_hdr_restore(&snap);
		osk_index_drop();
		osk_store_profile_lock(profile);
		return r;
	}
	return OSK_OK;
}

osk_result_t osk_store_profile_unlock(uint8_t profile, const uint8_t *pin,
				      size_t pin_len)
{
	osk_store_state_t *st = osk_store_state();
	osk_result_t r;

	if (!st->open) {
		return OSK_ERR_LOCKED;
	}
	if (profile >= OSK_PROFILE_MAX || pin == NULL ||
	    pin_len < OSK_PIN_MIN || pin_len > OSK_PIN_MAX) {
		return OSK_ERR_ARG;
	}
	if (st->hdr.profile_used[profile] == 0u) {
		return OSK_ERR_NOT_FOUND;
	}

	/* Erst der Chip: dort liegt der Fehlversuchszaehler, der ein Durchraten
	 * stoppt. Das Flash allein wuerde beliebig viele Versuche erlauben. */
	r = osk_se050_pin_verify(pin, pin_len);
	if (r != OSK_OK) {
		(void)osk_log_add_internal(OSK_LOG_DENY, 0, NULL);
		return r;
	}

	r = derive_profile_key(profile, pin, pin_len, &st->key_profile[profile]);
	if (r != OSK_OK) {
		return r;
	}
	st->profile_open[profile] = true;

	/* Die Probe: nur die richtige PIN dieses Profils entschluesselt
	 * seinen Index. */
	osk_index_drop();
	r = osk_index_load(profile);
	if (r != OSK_OK) {
		osk_store_profile_lock(profile);
		(void)osk_log_add_internal(OSK_LOG_DENY, 0, NULL);
		return r;
	}
	(void)osk_log_add_internal(OSK_LOG_UNLOCK, 0, NULL);
	return OSK_OK;
}

/* ---- Lesen ------------------------------------------------------------ */

static osk_result_t need_profile(uint8_t profile)
{
	osk_store_state_t *st = osk_store_state();

	if (!st->open) {
		return OSK_ERR_LOCKED;
	}
	if (profile >= OSK_PROFILE_MAX) {
		return OSK_ERR_ARG;
	}
	if (st->hdr.profile_used[profile] == 0u) {
		return OSK_ERR_NOT_FOUND;
	}
	if (!st->profile_open[profile]) {
		return OSK_ERR_DENIED;
	}
	return osk_index_load(profile);
}

osk_result_t osk_store_count(uint8_t profile, size_t *count_out)
{
	osk_result_t r = need_profile(profile);

	if (count_out == NULL) {
		return OSK_ERR_ARG;
	}
	if (r != OSK_OK) {
		return r;
	}
	*count_out = osk_index_page()->head.count;
	return OSK_OK;
}

static void meta_from_rec(const osk_idx_rec_t *rec, osk_item_meta_t *out)
{
	out->id = rec->id;
	out->type = (osk_item_type_t)rec->type;
	out->profile = rec->profile;
	/* Die Zeiger zeigen in den Arbeitspuffer des Tresors. Sie gelten, bis
	 * ein anderes Profil geladen oder der Tresor gesperrt wird. */
	out->name = rec->name;
	out->site = rec->site;
	out->changed_utc = rec->changed_utc;
	out->mirrored = (rec->flags & OSK_FLAG_MIRRORED) != 0u;
	out->paired_host_only = (rec->flags & OSK_FLAG_PAIRED_ONLY) != 0u;
}

osk_result_t osk_store_list(uint8_t profile, size_t index, osk_item_meta_t *out)
{
	osk_result_t r = need_profile(profile);

	if (out == NULL) {
		return OSK_ERR_ARG;
	}
	if (r != OSK_OK) {
		return r;
	}
	osk_idx_page_t *page = osk_index_page();

	if (index >= page->head.count) {
		return OSK_ERR_NOT_FOUND;
	}
	meta_from_rec(&page->recs[index], out);
	return OSK_OK;
}

/* Vergleich der Adresse: gleich oder Unterdomaene. Grossschreibung wird
 * gleichgesetzt, denn Rechnernamen unterscheiden sie nicht. */
static bool site_matches(const char *stored, const char *asked)
{
	size_t ls = strlen(stored), la = strlen(asked);

	if (ls == 0u) {
		return false;
	}
	if (la < ls) {
		return false;
	}
	size_t off = la - ls;

	for (size_t i = 0; i < ls; i++) {
		char a = stored[i], b = asked[off + i];

		if (a >= 'A' && a <= 'Z') {
			a = (char)(a - 'A' + 'a');
		}
		if (b >= 'A' && b <= 'Z') {
			b = (char)(b - 'A' + 'a');
		}
		if (a != b) {
			return false;
		}
	}
	/* Entweder genau gleich oder durch einen Punkt abgetrennt - "boese.de"
	 * darf nicht als "se.de" durchgehen. */
	return off == 0u || asked[off - 1] == '.';
}

osk_result_t osk_store_find(uint8_t profile, const char *site,
			    osk_item_id_t *out, size_t cap, size_t *len_out)
{
	osk_result_t r = need_profile(profile);

	if (site == NULL || out == NULL || len_out == NULL) {
		return OSK_ERR_ARG;
	}
	if (r != OSK_OK) {
		return r;
	}
	osk_idx_page_t *page = osk_index_page();
	size_t n = 0;

	for (uint16_t i = 0; i < page->head.count; i++) {
		if (!site_matches(page->recs[i].site, site)) {
			continue;
		}
		if (n < cap) {
			out[n] = page->recs[i].id;
		}
		n++;
	}
	*len_out = (n > cap) ? cap : n;
	return (n > cap) ? OSK_ERR_FULL : OSK_OK;
}

/* Sucht einen Eintrag in allen aufgeschlossenen Profilen. */
static osk_result_t find_rec(osk_item_id_t id, uint8_t *profile_out,
			     size_t *index_out)
{
	osk_store_state_t *st = osk_store_state();

	if (!st->open) {
		return OSK_ERR_LOCKED;
	}
	for (uint8_t p = 0; p < OSK_PROFILE_MAX; p++) {
		if (!st->profile_open[p]) {
			continue;
		}
		if (osk_index_load(p) != OSK_OK) {
			continue;
		}
		osk_idx_page_t *page = osk_index_page();

		for (uint16_t i = 0; i < page->head.count; i++) {
			if (page->recs[i].id == id) {
				*profile_out = p;
				*index_out = i;
				return OSK_OK;
			}
		}
	}
	return OSK_ERR_NOT_FOUND;
}

/* ---- Schreiben -------------------------------------------------------- */
/* Jede Aenderung laeuft nach demselben Muster:
 *
 *   Zustand sichern -> Geheimnis schreiben -> Index schreiben ->
 *   Kopfsatz schreiben -> alten Platz loeschen
 *
 * Geht irgendwo etwas schief, wird der Zustand zurueckgedreht und der
 * Arbeitspuffer verworfen. Der naechste Zugriff liest alles frisch aus dem
 * Flash und sieht genau den Zustand von vor der Aenderung. */

static uint8_t flags_from_meta(const osk_item_meta_t *meta, uint8_t keep)
{
	uint8_t f = (uint8_t)(keep & OSK_FLAG_READONLY);

	if (meta->mirrored) {
		f |= OSK_FLAG_MIRRORED;
	}
	if (meta->paired_host_only) {
		f |= OSK_FLAG_PAIRED_ONLY;
	}
	return f;
}

/* Dreht eine gescheiterte Transaktion zurueck. */
static osk_result_t rollback(const osk_hdr_t *snap, osk_result_t r)
{
	osk_hdr_restore(snap);
	osk_index_drop();
	return r;
}

osk_result_t osk_store_add(const osk_item_meta_t *meta, const uint8_t *secret,
			   size_t secret_len, osk_item_id_t *id_out)
{
	osk_store_state_t *st = osk_store_state();
	osk_hdr_t snap;
	osk_result_t r;

	if (meta == NULL || secret == NULL || id_out == NULL ||
	    secret_len == 0u || secret_len > OSK_SECRET_MAX ||
	    meta->name == NULL || meta->name[0] == 0) {
		return OSK_ERR_ARG;
	}
	r = need_profile(meta->profile);
	if (r != OSK_OK) {
		return r;
	}
	osk_idx_page_t *page = osk_index_page();

	if (page->head.count >= OSK_ITEMS_MAX) {
		return OSK_ERR_FULL;
	}

	osk_hdr_snapshot(&snap);

	uint32_t slot = 0;

	r = osk_space_slot_alloc(&slot);
	if (r != OSK_OK) {
		return rollback(&snap, r);
	}

	osk_item_id_t id = st->hdr.item_next;
	uint32_t ct_len = 0;
	uint8_t nonce[OSK_NONCE_LEN];

	/* 1. das Geheimnis an einen freien Platz */
	r = osk_blob_write(slot, id, secret, secret_len,
			   &st->key_profile[meta->profile], &ct_len, nonce);
	if (r != OSK_OK) {
		return rollback(&snap, r);
	}

	osk_idx_rec_t *rec = &page->recs[page->head.count];

	memset(rec, 0, sizeof(*rec));
	rec->id = id;
	rec->type = (uint16_t)meta->type;
	rec->profile = meta->profile;
	rec->flags = flags_from_meta(meta, 0u);
	rec->changed_utc = osk_now_utc();
	rec->slot = slot;
	rec->blob_ct_len = ct_len;
	rec->secret_len = (uint32_t)secret_len;
	memcpy(rec->nonce, nonce, sizeof(nonce));
	osk_str_copy(rec->name, sizeof(rec->name), meta->name);
	osk_str_copy(rec->site, sizeof(rec->site), meta->site);
	page->head.count += 1u;

	/* 2. der Index */
	r = osk_index_store(meta->profile);
	if (r != OSK_OK) {
		return rollback(&snap, r);
	}

	/* 3. der Kopfsatz - ab hier gilt der neue Zustand */
	osk_space_slot_set(slot, true);
	st->hdr.item_next = id + 1u;
	r = osk_hdr_commit();
	if (r != OSK_OK) {
		return rollback(&snap, r);
	}

	(void)osk_log_add_internal(OSK_LOG_ADD, id, NULL);
	*id_out = id;
	return OSK_OK;
}

osk_result_t osk_store_update(osk_item_id_t id, const osk_item_meta_t *meta,
			      const uint8_t *secret, size_t secret_len)
{
	osk_store_state_t *st = osk_store_state();
	osk_hdr_t snap;
	uint8_t profile = 0;
	size_t index = 0;
	osk_result_t r = find_rec(id, &profile, &index);

	if (r != OSK_OK) {
		return r;
	}
	if (secret != NULL && (secret_len == 0u || secret_len > OSK_SECRET_MAX)) {
		return OSK_ERR_ARG;
	}
	osk_idx_page_t *page = osk_index_page();
	osk_idx_rec_t *rec = &page->recs[index];

	if ((rec->flags & OSK_FLAG_READONLY) != 0u) {
		return OSK_ERR_DENIED;
	}
	if (meta != NULL && meta->profile != profile) {
		/* Ein Umzug zwischen Profilen ist kein Aendern - er braucht
		 * einen neuen Eintrag im Zielprofil, mit dessen Schluessel. */
		return OSK_ERR_UNSUPPORTED;
	}

	osk_hdr_snapshot(&snap);

	uint32_t old_slot = rec->slot;
	uint32_t new_slot = old_slot;

	if (secret != NULL) {
		/* Niemals an derselben Stelle ueberschreiben: solange der neue
		 * Platz nicht gilt, muss der alte vollstaendig lesbar bleiben. */
		r = osk_space_slot_alloc(&new_slot);
		if (r != OSK_OK) {
			return rollback(&snap, r);
		}
		uint32_t ct_len = 0;
		uint8_t nonce[OSK_NONCE_LEN];

		r = osk_blob_write(new_slot, id, secret, secret_len,
				   &st->key_profile[profile], &ct_len, nonce);
		if (r != OSK_OK) {
			return rollback(&snap, r);
		}
		rec->slot = new_slot;
		rec->blob_ct_len = ct_len;
		rec->secret_len = (uint32_t)secret_len;
		memcpy(rec->nonce, nonce, sizeof(nonce));
	}
	if (meta != NULL) {
		rec->type = (uint16_t)meta->type;
		rec->flags = flags_from_meta(meta, rec->flags);
		if (meta->name != NULL) {
			osk_str_copy(rec->name, sizeof(rec->name), meta->name);
		}
		if (meta->site != NULL) {
			osk_str_copy(rec->site, sizeof(rec->site), meta->site);
		}
	}
	rec->changed_utc = osk_now_utc();

	r = osk_index_store(profile);
	if (r != OSK_OK) {
		return rollback(&snap, r);
	}

	/* Mit demselben Kopfsatz wird der neue Platz belegt und der alte frei:
	 * der Index zeigt dann schon auf den neuen. */
	if (new_slot != old_slot) {
		osk_space_slot_set(new_slot, true);
		osk_space_slot_set(old_slot, false);
	}
	r = osk_hdr_commit();
	if (r != OSK_OK) {
		return rollback(&snap, r);
	}
	if (new_slot != old_slot) {
		/* 4. Der alte Platz gehoert niemandem mehr. Scheitert das
		 * Loeschen, bleibt dort Kryptotext ohne Schluessel liegen und
		 * wird beim naechsten Gebrauch des Platzes geloescht. */
		(void)osk_blob_erase(old_slot);
	}
	(void)osk_log_add_internal(OSK_LOG_UPDATE, id, NULL);
	return OSK_OK;
}

osk_result_t osk_store_delete(osk_item_id_t id)
{
	osk_hdr_t snap;
	uint8_t profile = 0;
	size_t index = 0;
	osk_result_t r = find_rec(id, &profile, &index);

	if (r != OSK_OK) {
		return r;
	}
	osk_idx_page_t *page = osk_index_page();

	if ((page->recs[index].flags & OSK_FLAG_READONLY) != 0u) {
		return OSK_ERR_DENIED;
	}
	osk_hdr_snapshot(&snap);

	uint32_t slot = page->recs[index].slot;

	/* Luecke schliessen: der letzte Satz rutscht nach vorn. */
	if (index + 1u < page->head.count) {
		page->recs[index] = page->recs[page->head.count - 1u];
	}
	memset(&page->recs[page->head.count - 1u], 0, sizeof(osk_idx_rec_t));
	page->head.count -= 1u;

	r = osk_index_store(profile);
	if (r != OSK_OK) {
		return rollback(&snap, r);
	}
	osk_space_slot_set(slot, false);
	r = osk_hdr_commit();
	if (r != OSK_OK) {
		return rollback(&snap, r);
	}
	(void)osk_blob_erase(slot);
	(void)osk_log_add_internal(OSK_LOG_DELETE, id, NULL);
	return OSK_OK;
}

/* ---- Herausgeben ------------------------------------------------------ */

osk_result_t osk_store_reveal(osk_item_id_t id, const char *asked_by,
			      uint8_t *out, size_t cap, size_t *len_out)
{
	osk_store_state_t *st = osk_store_state();
	uint8_t profile = 0;
	size_t index = 0;

	if (out == NULL || len_out == NULL) {
		return OSK_ERR_ARG;
	}
	*len_out = 0;

	osk_result_t r = find_rec(id, &profile, &index);

	if (r != OSK_OK) {
		return r;
	}
	osk_idx_rec_t *rec = &osk_index_page()->recs[index];

	/* Die Freigabe wird VOR jeder Pruefung der Puffergroesse geholt. Sonst
	 * koennte ein Programm durch Probieren erfahren, wie lang das Geheimnis
	 * ist, ohne dass der Nutzer je zugestimmt hat. */
	osk_ask_req_t req = {
		.kind = OSK_ASK_SECRET,
		.subject = rec->name,
		.detail = (rec->site[0] != '\0') ? rec->site : NULL,
		.asked_by = asked_by,
		.timeout_ms = OSK_REVEAL_TIMEOUT_MS,
	};

	r = osk_ui_confirm(&req);
	if (r != OSK_OK) {
		(void)osk_log_add_internal(OSK_LOG_REVEAL_DENY, id, asked_by);
		return r;
	}

	if (cap < rec->secret_len) {
		(void)osk_log_add_internal(OSK_LOG_REVEAL_DENY, id, asked_by);
		return OSK_ERR_ARG;
	}

	size_t len = 0;

	r = osk_blob_read(rec, &st->key_profile[profile], out, cap, &len);
	if (r != OSK_OK) {
		(void)osk_log_add_internal(OSK_LOG_REVEAL_DENY, id, asked_by);
		return r;
	}

	/* Erst protokollieren, dann herausgeben: schlaegt das Protokoll fehl,
	 * verlaesst das Geheimnis das Geraet nicht. */
	osk_result_t lr = osk_log_add_internal(OSK_LOG_REVEAL, id, asked_by);

	if (lr != OSK_OK) {
		osk_crypto_wipe(out, cap);
		return lr;
	}
	*len_out = len;
	return OSK_OK;
}

/* ---- Kennzeichen ------------------------------------------------------ */

osk_result_t osk_store_item_flags(osk_item_id_t id, uint8_t *flags_out)
{
	uint8_t profile = 0;
	size_t index = 0;

	if (flags_out == NULL) {
		return OSK_ERR_ARG;
	}
	osk_result_t r = find_rec(id, &profile, &index);

	if (r != OSK_OK) {
		return r;
	}
	*flags_out = osk_index_page()->recs[index].flags;
	return OSK_OK;
}

osk_result_t osk_store_item_set_readonly(osk_item_id_t id, bool on)
{
	uint8_t profile = 0;
	size_t index = 0;
	osk_result_t r = find_rec(id, &profile, &index);

	if (r != OSK_OK) {
		return r;
	}
	osk_idx_rec_t *rec = &osk_index_page()->recs[index];
	uint8_t keep = rec->flags;
	osk_hdr_t snap;

	if (on) {
		rec->flags |= OSK_FLAG_READONLY;
	} else {
		rec->flags &= (uint8_t)(0xFFu ^ OSK_FLAG_READONLY);
	}
	if (rec->flags == keep) {
		return OSK_OK;
	}
	osk_hdr_snapshot(&snap);
	r = osk_index_store(profile);
	if (r == OSK_OK) {
		r = osk_hdr_commit();
	}
	if (r != OSK_OK) {
		return rollback(&snap, r);
	}
	return OSK_OK;
}

osk_result_t osk_store_item_counter(osk_item_id_t id, uint64_t *out)
{
	uint8_t profile = 0;
	size_t index = 0;

	if (out == NULL) {
		return OSK_ERR_ARG;
	}
	osk_result_t r = find_rec(id, &profile, &index);

	if (r != OSK_OK) {
		return r;
	}
	*out = osk_index_page()->recs[index].counter;
	return OSK_OK;
}

osk_result_t osk_store_item_counter_next(osk_item_id_t id, uint64_t *out)
{
	uint8_t profile = 0;
	size_t index = 0;

	if (out == NULL) {
		return OSK_ERR_ARG;
	}
	osk_result_t r = find_rec(id, &profile, &index);

	if (r != OSK_OK) {
		return r;
	}
	osk_idx_rec_t *rec = &osk_index_page()->recs[index];
	osk_hdr_t snap;

	osk_hdr_snapshot(&snap);
	rec->counter += 1u;
	r = osk_index_store(profile);
	if (r == OSK_OK) {
		r = osk_hdr_commit();
	}
	if (r != OSK_OK) {
		return rollback(&snap, r);
	}
	*out = rec->counter;
	return OSK_OK;
}

osk_result_t osk_store_format_version(uint16_t *version_out)
{
	osk_store_state_t *st = osk_store_state();

	if (version_out == NULL) {
		return OSK_ERR_ARG;
	}
	if (!st->open) {
		return OSK_ERR_LOCKED;
	}
	*version_out = st->hdr.format;
	return OSK_OK;
}
