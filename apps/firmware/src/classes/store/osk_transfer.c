/* Stick zu Stick - siehe osk_transfer.h fuer den Ablauf. */

#include <string.h>
#include "store_internal.h"
#include "store_chunk.h"
#include "osk_store_profiles.h"
#include "osk_transfer.h"
#include "osk_se050.h"
#include "osk_ui.h"

/* Fluechtiger Schluessel im Chip. Er wird je Uebertragung neu angelegt und
 * danach geloescht - so taugt kein mitgeschnittener Verkehr spaeter noch
 * etwas, auch wenn das Geraet in falsche Haende geraet. */
#define OSK_KEYID_TRANSFER 0x0000F200u

static const char INFO_KEY[] = "osk/transfer/1/key";
static const char INFO_CMP[] = "osk/transfer/1/compare";

/* Derselbe Aufbau wie bei einem Eintrag der Sicherungsdatei - nur ohne die
 * Kennung, denn die gehoert zum alten Geraet. */
typedef struct {
	uint16_t type;
	uint8_t profile;
	uint8_t flags;
	uint64_t changed_utc;
	uint64_t counter;
	uint32_t secret_len;
	uint8_t tag[OSK_TAG_MAX];
	uint8_t tag_len;
	uint8_t pad[3];
	char name[64];
	char site[128];
} osk_transfer_item_t;

OSK_STATIC_ASSERT(sizeof(osk_transfer_item_t) <= OSK_CHUNK_ITEM_HEAD, transfer_item_fits);

/* ---- Schluessel und Vergleichszahl ------------------------------------ */

/* Beide Seiten muessen dieselben Zahlen bekommen, obwohl jede ihren eigenen
 * Schluessel "zuerst" hat. Darum werden die beiden oeffentlichen Teile nach
 * ihren Bytes sortiert, bevor sie eingerechnet werden. */
static void sorted_pubs(const osk_transfer_t *t, const uint8_t **lo, size_t *lo_len,
			const uint8_t **hi, size_t *hi_len)
{
	size_t n = (t->own_pub_len < t->peer_pub_len) ? t->own_pub_len : t->peer_pub_len;
	int cmp = memcmp(t->own_pub, t->peer_pub, n);

	if (cmp == 0) {
		cmp = (t->own_pub_len < t->peer_pub_len) ? -1 : 1;
	}
	if (cmp < 0) {
		*lo = t->own_pub;
		*lo_len = t->own_pub_len;
		*hi = t->peer_pub;
		*hi_len = t->peer_pub_len;
	} else {
		*lo = t->peer_pub;
		*lo_len = t->peer_pub_len;
		*hi = t->own_pub;
		*hi_len = t->own_pub_len;
	}
}

/* Der Mitschnitt der Verhandlung: beide oeffentlichen Teile, sortiert. Er
 * geht in den Schluessel UND in die Vergleichszahl ein. Nur deshalb beweist
 * die Zahl auf der Anzeige, dass beide Seiten denselben Partner haben. */
static osk_result_t transcript(const osk_transfer_t *t, const char *info, size_t info_len,
			       uint8_t *out, size_t out_len, const uint8_t *ikm, size_t ikm_len)
{
	uint8_t buf[64u + 2u * OSK_TRANSFER_PUB_MAX];
	const uint8_t *lo, *hi;
	size_t lo_len, hi_len, pos = 0;

	sorted_pubs(t, &lo, &lo_len, &hi, &hi_len);
	if (info_len + lo_len + hi_len > sizeof(buf)) {
		return OSK_ERR_INTERNAL;
	}
	memcpy(buf + pos, info, info_len);
	pos += info_len;
	memcpy(buf + pos, lo, lo_len);
	pos += lo_len;
	memcpy(buf + pos, hi, hi_len);
	pos += hi_len;

	osk_result_t r = osk_crypto_hkdf(ikm, ikm_len, NULL, 0, buf, pos, out, out_len);

	osk_crypto_wipe(buf, sizeof(buf));
	return r;
}

/* ---- Ablauf ----------------------------------------------------------- */

osk_result_t osk_transfer_begin(osk_transfer_t *t)
{
	osk_key_policy_t policy = {.exportable = false, .needs_touch = false};
	osk_result_t r;

	if (t == NULL) {
		return OSK_ERR_ARG;
	}
	if (!osk_store_is_open()) {
		return OSK_ERR_LOCKED;
	}
	memset(t, 0, sizeof(*t));

	/* Ein alter Schluessel derselben Kennung wird weggeraeumt, damit nicht
	 * zwei Uebertragungen denselben benutzen. */
	(void)osk_se050_key_delete(OSK_KEYID_TRANSFER);
	r = osk_se050_key_create(OSK_KEYID_TRANSFER, OSK_CURVE_P256, &policy);
	if (r != OSK_OK) {
		return r;
	}
	return osk_se050_key_public(OSK_KEYID_TRANSFER, t->own_pub, sizeof(t->own_pub),
				    &t->own_pub_len);
}

osk_result_t osk_transfer_accept_peer(osk_transfer_t *t, const uint8_t *peer_pub, size_t peer_len)
{
	uint8_t shared[OSK_TRANSFER_PUB_MAX];
	size_t shared_len = 0;
	uint8_t cmp[4];
	osk_result_t r;

	if (t == NULL || peer_pub == NULL || peer_len == 0u || peer_len > sizeof(t->peer_pub) ||
	    t->own_pub_len == 0u) {
		return OSK_ERR_ARG;
	}
	if (peer_len == t->own_pub_len && memcmp(peer_pub, t->own_pub, peer_len) == 0) {
		/* Der eigene oeffentliche Teil zurueckgespiegelt: so wuerde ein
		 * Rechner in der Mitte versuchen, beide Seiten mit sich selbst
		 * reden zu lassen. */
		return OSK_ERR_DENIED;
	}
	memcpy(t->peer_pub, peer_pub, peer_len);
	t->peer_pub_len = peer_len;

	r = osk_se050_ecdh(OSK_KEYID_TRANSFER, peer_pub, peer_len, shared, sizeof(shared),
			   &shared_len);
	if (r != OSK_OK) {
		return r;
	}

	r = transcript(t, INFO_KEY, sizeof(INFO_KEY) - 1, t->key.b, sizeof(t->key.b), shared,
		       shared_len);
	if (r == OSK_OK) {
		r = transcript(t, INFO_CMP, sizeof(INFO_CMP) - 1, cmp, sizeof(cmp), shared,
			       shared_len);
	}
	osk_crypto_wipe(shared, sizeof(shared));
	if (r != OSK_OK) {
		osk_crypto_wipe(&t->key, sizeof(t->key));
		return r;
	}

	/* Sechs Stellen aus den ersten vier Byte. Mehr Stellen liest niemand
	 * zuverlaessig ab, weniger waeren zu leicht zu treffen: eine Faelschung
	 * muss 1 zu 1 000 000 raten, und zwar bei laufender Anzeige. */
	uint32_t v = ((uint32_t)cmp[0] << 24) | ((uint32_t)cmp[1] << 16) | ((uint32_t)cmp[2] << 8) |
		     (uint32_t)cmp[3];

	v %= 1000000u;
	for (int i = (int)OSK_TRANSFER_DIGITS - 1; i >= 0; i--) {
		t->digits[i] = (char)('0' + (v % 10u));
		v /= 10u;
	}
	t->digits[OSK_TRANSFER_DIGITS] = '\0';
	osk_crypto_wipe(cmp, sizeof(cmp));

	osk_ask_req_t req = {
	    .kind = OSK_ASK_PAIR,
	    .subject = "pair",
	    .detail = t->digits,
	    .asked_by = NULL,
	    .timeout_ms = 60000u,
	};

	r = osk_ui_confirm(&req);
	if (r != OSK_OK) {
		osk_crypto_wipe(&t->key, sizeof(t->key));
		(void)osk_log_add_internal(OSK_LOG_DENY, 0, "pair");
		return r;
	}
	t->confirmed = true;
	t->seq = 1u;
	return OSK_OK;
}

void osk_transfer_end(osk_transfer_t *t)
{
	(void)osk_se050_key_delete(OSK_KEYID_TRANSFER);
	osk_chunk_wipe();
	if (t != NULL) {
		osk_crypto_wipe(t, sizeof(*t));
	}
}

/* Die zusaetzlichen Daten jedes Stuecks: Fassung und beide oeffentlichen
 * Teile, sortiert. Ein Stueck aus einer anderen Uebertragung passt damit
 * nicht. */
static void transfer_aad(const osk_transfer_t *t, uint8_t *out, size_t cap, size_t *len_out)
{
	const uint8_t *lo, *hi;
	size_t lo_len, hi_len, pos = 0;

	sorted_pubs(t, &lo, &lo_len, &hi, &hi_len);
	out[pos++] = (uint8_t)OSK_TRANSFER_FMT;
	if (pos + lo_len + hi_len > cap) {
		*len_out = pos;
		return;
	}
	memcpy(out + pos, lo, lo_len);
	pos += lo_len;
	memcpy(out + pos, hi, hi_len);
	pos += hi_len;
	*len_out = pos;
}

osk_result_t osk_transfer_send(osk_transfer_t *t, osk_chunk_sink_fn sink, void *ctx,
			       size_t *sent_items)
{
	osk_store_state_t *st = osk_store_state();
	uint8_t aad[1u + 2u * OSK_TRANSFER_PUB_MAX];
	size_t aad_len = 0;
	uint8_t h[OSK_HASH_LEN];
	uint8_t *plain = osk_chunk_plain();
	size_t count = 0;
	osk_result_t r = OSK_OK;

	if (t == NULL || sink == NULL || sent_items == NULL) {
		return OSK_ERR_ARG;
	}
	if (!st->open) {
		return OSK_ERR_LOCKED;
	}
	if (!t->confirmed) {
		/* Ohne verglichene Zahl verlaesst nichts das Geraet. */
		return OSK_ERR_DENIED;
	}
	transfer_aad(t, aad, sizeof(aad), &aad_len);
	memset(h, 0, sizeof(h));

	for (uint8_t p = 0; p < OSK_PROFILE_MAX && r == OSK_OK; p++) {
		if (!st->profile_open[p]) {
			continue;
		}
		r = osk_index_load(p);
		if (r != OSK_OK) {
			break;
		}
		osk_idx_page_t *page = osk_index_page();
		uint16_t n = page->head.count;

		for (uint16_t i = 0; i < n && r == OSK_OK; i++) {
			osk_idx_rec_t *rec = &page->recs[i];

			/* Nur was ausdruecklich gespiegelt werden darf. */
			if ((rec->flags & OSK_FLAG_MIRRORED) == 0u) {
				continue;
			}
			osk_transfer_item_t item;
			size_t secret_len = 0;

			memset(plain, 0, OSK_CHUNK_PLAIN_MAX);
			plain[0] = (uint8_t)OSK_CHUNK_ITEM;

			r = osk_blob_read(rec, &st->key_profile[p], plain + 4u + sizeof(item),
					  OSK_SECRET_MAX, &secret_len);
			if (r != OSK_OK) {
				break;
			}
			memset(&item, 0, sizeof(item));
			item.type = rec->type;
			item.profile = rec->profile;
			item.flags = rec->flags;
			item.changed_utc = rec->changed_utc;
			item.counter = rec->counter;
			item.secret_len = (uint32_t)secret_len;
			item.tag_len = rec->tag_len;
			memcpy(item.tag, rec->tag, sizeof(item.tag));
			memcpy(item.name, rec->name, sizeof(item.name));
			memcpy(item.site, rec->site, sizeof(item.site));
			memcpy(plain + 4u, &item, sizeof(item));

			r = osk_chunk_put(aad, aad_len, &t->key, t->seq, OSK_NONCE_TRANSFER,
					  4u + sizeof(item) + secret_len, sink, ctx, h);
			osk_crypto_wipe(&item, sizeof(item));
			t->seq++;
			count++;
		}
	}

	if (r == OSK_OK) {
		memset(plain, 0, OSK_CHUNK_PLAIN_MAX);
		plain[0] = (uint8_t)OSK_CHUNK_END;
		uint32_t c = (uint32_t)count;

		for (int i = 0; i < 4; i++) {
			plain[4 + i] = (uint8_t)(c >> (8 * i));
		}
		memcpy(plain + 8u, h, OSK_HASH_LEN);
		r = osk_chunk_put(aad, aad_len, &t->key, t->seq, OSK_NONCE_TRANSFER,
				  8u + OSK_HASH_LEN, sink, ctx, h);
		t->seq++;
	}
	osk_chunk_wipe();
	if (r != OSK_OK) {
		return r;
	}
	*sent_items = count;
	(void)osk_log_add_internal(OSK_LOG_TRANSFER, 0, "pair");
	return OSK_OK;
}

osk_result_t osk_transfer_receive(osk_transfer_t *t, osk_chunk_src_fn src, void *ctx,
				  size_t *got_items)
{
	uint8_t aad[1u + 2u * OSK_TRANSFER_PUB_MAX];
	size_t aad_len = 0;
	uint8_t h[OSK_HASH_LEN];
	uint8_t h_before[OSK_HASH_LEN];
	uint8_t *plain = osk_chunk_plain();
	size_t count = 0;
	osk_result_t r = OSK_OK;

	if (t == NULL || src == NULL || got_items == NULL) {
		return OSK_ERR_ARG;
	}
	if (!osk_store_is_open()) {
		return OSK_ERR_LOCKED;
	}
	if (!t->confirmed) {
		return OSK_ERR_DENIED;
	}
	transfer_aad(t, aad, sizeof(aad), &aad_len);
	memset(h, 0, sizeof(h));

	for (;;) {
		size_t plain_len = 0;

		memcpy(h_before, h, sizeof(h));
		r = osk_chunk_get(aad, aad_len, &t->key, t->seq, OSK_NONCE_TRANSFER, src, ctx,
				  &plain_len, h);
		if (r != OSK_OK) {
			break;
		}
		t->seq++;
		if (plain_len < 4u) {
			r = OSK_ERR_FORMAT;
			break;
		}
		if (plain[0] == (uint8_t)OSK_CHUNK_END) {
			uint32_t want = 0;

			if (plain_len != 8u + OSK_HASH_LEN) {
				r = OSK_ERR_FORMAT;
				break;
			}
			for (int i = 3; i >= 0; i--) {
				want = (want << 8) | plain[4 + i];
			}
			if (want != (uint32_t)count ||
			    !osk_crypto_equal(plain + 8u, h_before, OSK_HASH_LEN)) {
				r = OSK_ERR_FORMAT;
			}
			break;
		}
		if (plain[0] != (uint8_t)OSK_CHUNK_ITEM) {
			r = OSK_ERR_FORMAT;
			break;
		}

		osk_transfer_item_t item;

		if (plain_len < 4u + sizeof(item)) {
			r = OSK_ERR_FORMAT;
			break;
		}
		memcpy(&item, plain + 4u, sizeof(item));
		if (item.secret_len == 0u || item.secret_len > OSK_SECRET_MAX ||
		    plain_len != 4u + sizeof(item) + item.secret_len ||
		    item.profile >= OSK_PROFILE_MAX) {
			r = OSK_ERR_FORMAT;
			break;
		}
		item.name[sizeof(item.name) - 1] = '\0';
		item.site[sizeof(item.site) - 1] = '\0';
		if (!osk_store_profile_is_unlocked(item.profile)) {
			r = OSK_ERR_DENIED;
			break;
		}

		osk_item_meta_t meta;

		memset(&meta, 0, sizeof(meta));
		meta.type = (osk_item_type_t)item.type;
		meta.profile = item.profile;
		meta.name = item.name;
		meta.site = item.site;
		meta.mirrored = true; /* es kam ueber die Spiegelung */
		meta.paired_host_only = (item.flags & OSK_FLAG_PAIRED_ONLY) != 0u;
		if (item.tag_len > OSK_TAG_MAX) {
			r = OSK_ERR_FORMAT;
			break;
		}
		meta.tag_len = item.tag_len;
		memcpy(meta.tag, item.tag, sizeof(meta.tag));

		osk_item_id_t new_id = 0;

		r = osk_store_add(&meta, plain + 4u + sizeof(item), item.secret_len, &new_id);
		osk_crypto_wipe(&item, sizeof(item));
		if (r != OSK_OK) {
			break;
		}
		count++;
		plain = osk_chunk_plain();
	}

	osk_chunk_wipe();
	if (r != OSK_OK) {
		return r;
	}
	*got_items = count;
	(void)osk_log_add_internal(OSK_LOG_TRANSFER, 0, "pair");
	return OSK_OK;
}
