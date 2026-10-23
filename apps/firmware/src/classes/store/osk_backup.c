/* Sicherungsdatei schreiben und lesen.
 *
 * Aufbau der Datei: Kopf im Klartext, dann je ein Stueck pro Eintrag, zuletzt
 * das Abschlussstueck. Die Stuecke kommen aus store_chunk.c; der Kopf ist bei
 * jedem Stueck zusaetzliche Daten der Verschluesselung. Wird im Kopf etwas
 * geaendert - etwa die Argon2-Werte herabgesetzt -, laesst sich kein Stueck
 * mehr entschluesseln. */

#include <string.h>
#include "store_internal.h"
#include "store_chunk.h"
#include "osk_store_profiles.h"
#include "osk_backup.h"
#include "osk_ui.h"

OSK_STATIC_ASSERT(sizeof(osk_backup_item_t) <= OSK_CHUNK_ITEM_HEAD, backup_item_fits);
OSK_STATIC_ASSERT(sizeof(osk_backup_head_t) == 52, backup_head_52);

/* ---- Kennwort --------------------------------------------------------- */

osk_result_t osk_backup_check_pass(const char *pass, size_t len)
{
	if (pass == NULL || len < OSK_PASS_MIN || len > OSK_PASS_MAX) {
		return OSK_ERR_ARG;
	}
	for (size_t i = 0; i < len; i++) {
		/* Steuerzeichen lassen sich auf einer fremden Tastatur nicht
		 * wieder eingeben - und eine Sicherung, die niemand mehr
		 * eintippen kann, ist keine. */
		if ((unsigned char)pass[i] < 0x20u) {
			return OSK_ERR_ARG;
		}
	}
	return OSK_OK;
}

osk_result_t osk_backup_suggest_pass(char *out, size_t cap)
{
	size_t pos = 0;

	if (out == NULL || cap < OSK_PASS_SUGGEST_MAX) {
		return OSK_ERR_ARG;
	}
	for (unsigned w = 0; w < OSK_PASS_WORDS; w++) {
		uint8_t pick = 0;
		osk_result_t r = osk_crypto_random(&pick, 1);

		if (r != OSK_OK) {
			osk_crypto_wipe(out, cap);
			return r;
		}
		/* Ein Byte, 256 Woerter: kein Rest, also keine Schieflage in
		 * der Verteilung. Acht Woerter ergeben 64 Bit. */
		const char *word = osk_wordlist[pick];

		if (w > 0u) {
			if (pos + 1u >= cap) {
				osk_crypto_wipe(out, cap);
				return OSK_ERR_FULL;
			}
			out[pos++] = '-';
		}
		for (const char *p = word; *p != '\0'; p++) {
			if (pos + 1u >= cap) {
				osk_crypto_wipe(out, cap);
				return OSK_ERR_FULL;
			}
			out[pos++] = *p;
		}
	}
	out[pos] = '\0';
	return OSK_OK;
}

/* ---- Kopf ------------------------------------------------------------- */

static uint32_t head_crc(const osk_backup_head_t *h)
{
	return osk_crc32(h, sizeof(*h) - sizeof(h->crc32));
}

static osk_result_t derive_file_key(const osk_backup_head_t *head, const char *pass,
				    size_t pass_len, osk_key_t *out)
{
	osk_argon2_params_t p = {
	    .m_cost_kib = head->m_cost_kib,
	    .t_cost = head->t_cost,
	    .lanes = head->lanes,
	};

	if (p.m_cost_kib == 0u || p.t_cost == 0u || p.lanes == 0u) {
		return OSK_ERR_FORMAT;
	}
	if (p.m_cost_kib > OSK_ARGON2_MAX_MEM_KIB) {
		/* Die Datei wurde auf einem Rechner mit mehr Speicher angelegt.
		 * Dann muss sie dort geoeffnet werden - lieber eine klare
		 * Auskunft als ein Absturz mangels RAM. */
		return OSK_ERR_UNSUPPORTED;
	}
	return osk_crypto_argon2id(&p, pass, pass_len, head->salt, out->b, sizeof(out->b));
}

static osk_result_t read_head(osk_backup_src_fn src, void *ctx, osk_backup_head_t *head)
{
	osk_result_t r = src(ctx, (uint8_t *)head, sizeof(*head));

	if (r != OSK_OK) {
		return r;
	}
	if (head->magic0 != OSK_BACKUP_MAGIC0 || head->magic1 != OSK_BACKUP_MAGIC1 ||
	    head->file_format != OSK_BACKUP_FILE_FMT || head->crc32 != head_crc(head)) {
		return OSK_ERR_FORMAT;
	}
	if (head->vault_format > OSK_FMT_CURRENT) {
		/* Die Datei kommt von einer neueren Firmware. */
		return OSK_ERR_UNSUPPORTED;
	}
	return OSK_OK;
}

static osk_result_t ask(const char *what)
{
	osk_ask_req_t req = {
	    .kind = OSK_ASK_PAIR,
	    .subject = what,
	    .detail = NULL,
	    .asked_by = NULL,
	    .timeout_ms = 60000u,
	};
	osk_result_t r = osk_ui_confirm(&req);

	if (r != OSK_OK) {
		(void)osk_log_add_internal(OSK_LOG_DENY, 0, what);
	}
	return r;
}

/* ---- Schreiben -------------------------------------------------------- */

osk_result_t osk_backup_write(const char *pass, size_t pass_len, const osk_argon2_params_t *params,
			      osk_backup_sink_fn sink, void *ctx, size_t *written_items,
			      osk_key_t *key_out)
{
	osk_store_state_t *st = osk_store_state();
	osk_backup_head_t head;
	osk_key_t key;
	uint8_t h[OSK_HASH_LEN];
	uint8_t *plain = osk_chunk_plain();
	uint32_t seq = 1u;
	size_t count = 0;
	osk_result_t r;

	if (sink == NULL || written_items == NULL) {
		return OSK_ERR_ARG;
	}
	if (!st->open) {
		return OSK_ERR_LOCKED;
	}
	r = osk_backup_check_pass(pass, pass_len);
	if (r != OSK_OK) {
		return r;
	}

	/* Eine Sicherung traegt alle Geheimnisse aus dem Haus. Dafuer gibt es
	 * genau eine Freigabe, und die nennt den Vorgang beim Namen. */
	r = ask("backup");
	if (r != OSK_OK) {
		return r;
	}

	memset(&head, 0, sizeof(head));
	head.magic0 = OSK_BACKUP_MAGIC0;
	head.magic1 = OSK_BACKUP_MAGIC1;
	head.file_format = OSK_BACKUP_FILE_FMT;
	head.vault_format = OSK_FMT_CURRENT;
	head.m_cost_kib = (params != NULL) ? params->m_cost_kib : OSK_ARGON2_DEV_MEM_KIB;
	head.t_cost = (params != NULL) ? params->t_cost : OSK_ARGON2_DEV_TIME;
	head.lanes = (params != NULL) ? params->lanes : 1u;
	r = osk_crypto_random(head.salt, sizeof(head.salt));
	if (r != OSK_OK) {
		return r;
	}
	head.crc32 = head_crc(&head);

	r = derive_file_key(&head, pass, pass_len, &key);
	if (r != OSK_OK) {
		return r;
	}

	memset(h, 0, sizeof(h));
	r = sink(ctx, (const uint8_t *)&head, sizeof(head));

	for (uint8_t p = 0; p < OSK_PROFILE_MAX && r == OSK_OK; p++) {
		if (!st->profile_open[p]) {
			continue;
		}
		r = osk_index_load(p);
		if (r != OSK_OK) {
			break;
		}
		size_t n = osk_index_count(p);

		for (size_t i = 0; i < n && r == OSK_OK; i++) {
			osk_idx_rec_t *from = NULL;
			osk_idx_rec_t copy;

			r = osk_index_rec(p, i, &from);
			if (r != OSK_OK) {
				break;
			}
			/* Eine Kopie, kein Zeiger: der Arbeitspuffer haelt nur
			 * EINEN Indexblock, und beim naechsten Satz kann er
			 * gewechselt haben. */
			copy = *from;

			osk_idx_rec_t *rec = &copy;
			osk_backup_item_t item;
			size_t secret_len = 0;

			memset(plain, 0, OSK_CHUNK_PLAIN_MAX);
			plain[0] = (uint8_t)OSK_CHUNK_ITEM;

			r = osk_blob_read(rec, &st->key_profile[p], plain + 4u + sizeof(item),
					  OSK_SECRET_MAX, &secret_len);
			if (r != OSK_OK) {
				break;
			}

			memset(&item, 0, sizeof(item));
			item.id = rec->id;
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

			r = osk_chunk_put((const uint8_t *)&head, sizeof(head), &key, seq,
					  OSK_NONCE_BACKUP, 4u + sizeof(item) + secret_len, sink,
					  ctx, h);
			osk_crypto_wipe(&item, sizeof(item));
			osk_crypto_wipe(&copy, sizeof(copy));
			seq++;
			count++;
		}
	}

	if (r == OSK_OK) {
		/* Abschluss: Anzahl und Abdruck aller Stuecke davor. */
		memset(plain, 0, OSK_CHUNK_PLAIN_MAX);
		plain[0] = (uint8_t)OSK_CHUNK_END;
		uint32_t c = (uint32_t)count;

		for (int i = 0; i < 4; i++) {
			plain[4 + i] = (uint8_t)(c >> (8 * i));
		}
		memcpy(plain + 8u, h, OSK_HASH_LEN);
		r = osk_chunk_put((const uint8_t *)&head, sizeof(head), &key, seq, OSK_NONCE_BACKUP,
				  8u + OSK_HASH_LEN, sink, ctx, h);
	}

	osk_chunk_wipe();
	if (key_out != NULL && r == OSK_OK) {
		*key_out = key;
	}
	osk_crypto_wipe(&key, sizeof(key));
	if (r != OSK_OK) {
		return r;
	}
	*written_items = count;
	(void)osk_log_add_internal(OSK_LOG_BACKUP, 0, "backup");
	return OSK_OK;
}

/* ---- Lesen ------------------------------------------------------------ */

static osk_result_t read_items(const osk_backup_head_t *head, const osk_key_t *key,
			       osk_backup_src_fn src, void *ctx, size_t *restored_items)
{
	uint8_t h[OSK_HASH_LEN];
	uint8_t h_before[OSK_HASH_LEN];
	uint8_t *plain = osk_chunk_plain();
	uint32_t seq = 1u;
	size_t count = 0;
	osk_result_t r = OSK_OK;

	memset(h, 0, sizeof(h));
	for (;;) {
		size_t plain_len = 0;

		memcpy(h_before, h, sizeof(h));
		r = osk_chunk_get((const uint8_t *)head, sizeof(*head), key, seq, OSK_NONCE_BACKUP,
				  src, ctx, &plain_len, h);
		if (r != OSK_OK) {
			break;
		}
		if (plain_len < 4u) {
			r = OSK_ERR_FORMAT;
			break;
		}
		if (plain[0] == (uint8_t)OSK_CHUNK_END) {
			if (plain_len != 8u + OSK_HASH_LEN) {
				r = OSK_ERR_FORMAT;
				break;
			}
			uint32_t want = 0;

			for (int i = 3; i >= 0; i--) {
				want = (want << 8) | plain[4 + i];
			}
			/* Der Abdruck im Abschluss deckt alle Stuecke VOR ihm. */
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

		osk_backup_item_t item;

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
			/* Ohne aufgeschlossenes Profil gibt es keinen
			 * Schluessel, mit dem der Eintrag abgelegt werden
			 * koennte. */
			r = OSK_ERR_DENIED;
			break;
		}

		osk_item_meta_t meta;

		memset(&meta, 0, sizeof(meta));
		meta.type = (osk_item_type_t)item.type;
		meta.profile = item.profile;
		meta.name = item.name;
		meta.site = item.site;
		meta.mirrored = (item.flags & OSK_FLAG_MIRRORED) != 0u;
		meta.paired_host_only = (item.flags & OSK_FLAG_PAIRED_ONLY) != 0u;
		if (item.tag_len > OSK_TAG_MAX) {
			r = OSK_ERR_FORMAT;
			break;
		}
		meta.tag_len = item.tag_len;
		memcpy(meta.tag, item.tag, sizeof(meta.tag));

		osk_item_id_t new_id = 0;

		/* Jeder Eintrag wird einzeln und atomar abgelegt. Bricht der
		 * Strom mitten in der Wiederherstellung weg, sind die bis dahin
		 * gelesenen Eintraege da und der Tresor ist heil. Die Kennungen
		 * werden neu vergeben - die alten gehoeren zu einem anderen
		 * Geraet. */
		r = osk_store_add(&meta, plain + 4u + sizeof(item), item.secret_len, &new_id);
		osk_crypto_wipe(&item, sizeof(item));
		if (r != OSK_OK) {
			break;
		}
		count++;
		seq++;
		plain = osk_chunk_plain();
	}

	osk_chunk_wipe();
	if (r != OSK_OK) {
		return r;
	}
	if (restored_items != NULL) {
		*restored_items = count;
	}
	(void)osk_log_add_internal(OSK_LOG_RESTORE, 0, "restore");
	return OSK_OK;
}

osk_result_t osk_backup_read(const char *pass, size_t pass_len, osk_backup_src_fn src, void *ctx,
			     size_t *restored_items)
{
	osk_backup_head_t head;
	osk_key_t key;
	osk_result_t r;

	if (src == NULL) {
		return OSK_ERR_ARG;
	}
	if (!osk_store_is_open()) {
		return OSK_ERR_LOCKED;
	}
	r = osk_backup_check_pass(pass, pass_len);
	if (r != OSK_OK) {
		return r;
	}
	r = read_head(src, ctx, &head);
	if (r != OSK_OK) {
		return r;
	}
	r = ask("restore");
	if (r != OSK_OK) {
		return r;
	}
	r = derive_file_key(&head, pass, pass_len, &key);
	if (r != OSK_OK) {
		return r;
	}
	r = read_items(&head, &key, src, ctx, restored_items);
	osk_crypto_wipe(&key, sizeof(key));
	return r;
}

osk_result_t osk_backup_read_with_key(const osk_key_t *key, osk_backup_src_fn src, void *ctx,
				      size_t *restored_items)
{
	osk_backup_head_t head;
	osk_result_t r;

	if (src == NULL || key == NULL) {
		return OSK_ERR_ARG;
	}
	if (!osk_store_is_open()) {
		return OSK_ERR_LOCKED;
	}
	r = read_head(src, ctx, &head);
	if (r != OSK_OK) {
		return r;
	}
	r = ask("restore");
	if (r != OSK_OK) {
		return r;
	}
	return read_items(&head, key, src, ctx, restored_items);
}
