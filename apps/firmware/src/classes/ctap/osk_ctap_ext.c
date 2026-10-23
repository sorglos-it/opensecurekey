/* Erweiterungen. Siehe osk_ctap_ext.h.
 * Spezifikation: CTAP 2.2 Abschnitte 12.1, 12.4, 12.5 und 12.7. */
#include <string.h>
#include <stdbool.h>
#include "osk_ctap_ext.h"
#include "osk_ctap_internal.h"
#include "osk_ctap_key.h"
#include "osk_crypto.h"
#include "osk_pin.h"
#include "osk_store.h"

#define EXT_MINPIN_RP_MAX 2

static char g_minpin_rp[EXT_MINPIN_RP_MAX][OSK_MAX_RP_ID];
static size_t g_minpin_rp_count;

osk_result_t osk_ctap_minpin_rp_add(const char *rp_id)
{
	if (!rp_id || g_minpin_rp_count >= EXT_MINPIN_RP_MAX)
		return OSK_ERR_FULL;
	const size_t n = strlen(rp_id);
	if (n == 0 || n >= OSK_MAX_RP_ID)
		return OSK_ERR_ARG;
	memcpy(g_minpin_rp[g_minpin_rp_count], rp_id, n + 1);
	g_minpin_rp_count++;
	return OSK_OK;
}

void osk_ctap_minpin_rp_clear(void)
{
	memset(g_minpin_rp, 0, sizeof(g_minpin_rp));
	g_minpin_rp_count = 0;
}

static bool minpin_rp_allowed(const char *rp_id, size_t len)
{
	for (size_t i = 0; i < g_minpin_rp_count; i++) {
		if (strlen(g_minpin_rp[i]) == len &&
		    memcmp(g_minpin_rp[i], rp_id, len) == 0)
			return true;
	}
	return false;
}

/* Vergleicht einen CBOR-Textschluessel mit einem festen Namen. */
static bool key_is(const char *key, size_t key_len, const char *name)
{
	const size_t n = strlen(name);
	return key_len == n && memcmp(key, name, n) == 0;
}

/* Liest die Abbildung der hmac-secret-Eingabe fuer GetAssertion. */
static osk_result_t parse_hmac_get(osk_cbor_reader_t *r, osk_ctap_ext_in_t *out)
{
	size_t count = 0;

	if (osk_cbor_enter_map(r, &count) != OSK_OK)
		return OSK_ERR_FORMAT;
	if (count > 8) {
		osk_cbor_leave(r);
		return OSK_ERR_FORMAT;
	}

	osk_result_t rc = OSK_OK;
	bool have_key = false;
	for (size_t i = 0; i < count; i++) {
		uint64_t k = 0;
		if (osk_cbor_read_uint(r, &k) != OSK_OK) {
			rc = OSK_ERR_FORMAT;
			break;
		}
		if (k == 1u) {
			/* COSE_Key der Gegenseite */
			size_t inner = 0;
			if (osk_cbor_enter_map(r, &inner) != OSK_OK || inner > 8) {
				rc = OSK_ERR_FORMAT;
				break;
			}
			bool x = false, y = false;
			for (size_t j = 0; j < inner && rc == OSK_OK; j++) {
				int64_t ik = 0;
				if (osk_cbor_read_int(r, &ik) != OSK_OK) {
					rc = OSK_ERR_FORMAT;
					break;
				}
				if (ik == -2 || ik == -3) {
					const uint8_t *p = NULL;
					size_t plen = 0;
					if (osk_cbor_read_bytes(r, &p, &plen) != OSK_OK ||
					    plen != 32) {
						rc = OSK_ERR_FORMAT;
						break;
					}
					memcpy(out->peer_pub + ((ik == -2) ? 0 : 32), p, 32);
					if (ik == -2)
						x = true;
					else
						y = true;
				} else if (osk_cbor_skip(r) != OSK_OK) {
					rc = OSK_ERR_FORMAT;
					break;
				}
			}
			osk_cbor_leave(r);
			if (rc != OSK_OK)
				break;
			if (!x || !y) {
				rc = OSK_ERR_FORMAT;
				break;
			}
			have_key = true;
		} else if (k == 2u) {
			if (osk_cbor_read_bytes(r, &out->salt_enc,
						&out->salt_enc_len) != OSK_OK) {
				rc = OSK_ERR_FORMAT;
				break;
			}
		} else if (k == 3u) {
			if (osk_cbor_read_bytes(r, &out->salt_auth,
						&out->salt_auth_len) != OSK_OK) {
				rc = OSK_ERR_FORMAT;
				break;
			}
		} else if (k == 4u) {
			if (osk_cbor_read_uint(r, &out->hmac_protocol) != OSK_OK) {
				rc = OSK_ERR_FORMAT;
				break;
			}
			out->have_hmac_protocol = true;
		} else if (osk_cbor_skip(r) != OSK_OK) {
			rc = OSK_ERR_FORMAT;
			break;
		}
	}
	osk_cbor_leave(r);

	if (rc == OSK_OK && (!have_key || !out->salt_enc || !out->salt_auth))
		rc = OSK_ERR_FORMAT;
	if (rc == OSK_OK)
		out->hmac_secret_get = true;
	return rc;
}

static osk_result_t parse_large_blob(osk_cbor_reader_t *r, bool for_make,
				     osk_ctap_ext_in_t *out)
{
	size_t count = 0;

	if (osk_cbor_enter_map(r, &count) != OSK_OK)
		return OSK_ERR_FORMAT;
	if (count > 4) {
		osk_cbor_leave(r);
		return OSK_ERR_FORMAT;
	}

	osk_result_t rc = OSK_OK;
	for (size_t i = 0; i < count; i++) {
		const char *k = NULL;
		size_t klen = 0;
		if (osk_cbor_read_text(r, &k, &klen) != OSK_OK) {
			rc = OSK_ERR_FORMAT;
			break;
		}
		if (for_make && key_is(k, klen, "support")) {
			const char *v = NULL;
			size_t vlen = 0;
			if (osk_cbor_read_text(r, &v, &vlen) != OSK_OK) {
				rc = OSK_ERR_FORMAT;
				break;
			}
			if (key_is(v, vlen, "required"))
				out->large_blob_required = true;
			else if (!key_is(v, vlen, "preferred")) {
				rc = OSK_ERR_FORMAT;
				break;
			}
			out->large_blob_create = true;
		} else if (!for_make && key_is(k, klen, "read")) {
			bool v = false;
			if (osk_cbor_read_bool(r, &v) != OSK_OK || !v) {
				rc = OSK_ERR_FORMAT;
				break;
			}
			out->large_blob_read = true;
		} else if (!for_make && key_is(k, klen, "write")) {
			if (osk_cbor_read_bytes(r, &out->large_blob_write,
						&out->large_blob_write_len) != OSK_OK) {
				rc = OSK_ERR_FORMAT;
				break;
			}
		} else if (!for_make && key_is(k, klen, "originalSize")) {
			if (osk_cbor_read_uint(r, &out->large_blob_original_size) != OSK_OK) {
				rc = OSK_ERR_FORMAT;
				break;
			}
			out->large_blob_have_size = true;
		} else {
			rc = OSK_ERR_FORMAT; /* die CDDL laesst nichts anderes zu */
			break;
		}
	}
	osk_cbor_leave(r);

	if (rc != OSK_OK)
		return rc;

	if (!for_make) {
		/* Lesen und Schreiben schliessen sich aus (12.4). */
		const bool have_write = (out->large_blob_write != NULL);
		if (out->large_blob_read && (have_write || out->large_blob_have_size))
			return OSK_ERR_FORMAT;
		if (!out->large_blob_read && !(have_write && out->large_blob_have_size))
			return OSK_ERR_FORMAT;
	}
	return OSK_OK;
}

osk_result_t osk_ctap_ext_parse(osk_cbor_reader_t *r, bool for_make_credential,
				osk_ctap_ext_in_t *out)
{
	size_t count = 0;

	memset(out, 0, sizeof(*out));
	if (osk_cbor_enter_map(r, &count) != OSK_OK)
		return OSK_ERR_FORMAT;
	if (count > 8) {
		osk_cbor_leave(r);
		return OSK_ERR_FORMAT;
	}

	osk_result_t rc = OSK_OK;
	for (size_t i = 0; i < count; i++) {
		const char *k = NULL;
		size_t klen = 0;
		if (osk_cbor_read_text(r, &k, &klen) != OSK_OK) {
			rc = OSK_ERR_FORMAT;
			break;
		}

		if (key_is(k, klen, "hmac-secret")) {
			if (for_make_credential) {
				bool v = false;
				if (osk_cbor_read_bool(r, &v) != OSK_OK) {
					rc = OSK_ERR_FORMAT;
					break;
				}
				out->hmac_secret_create = v;
			} else {
				rc = parse_hmac_get(r, out);
				if (rc != OSK_OK)
					break;
			}
		} else if (key_is(k, klen, "credProtect") && for_make_credential) {
			uint64_t v = 0;
			if (osk_cbor_read_uint(r, &v) != OSK_OK) {
				rc = OSK_ERR_FORMAT;
				break;
			}
			if (v < OSK_CRED_PROTECT_OPTIONAL || v > OSK_CRED_PROTECT_UV_REQUIRED) {
				rc = OSK_ERR_ARG;
				break;
			}
			out->cred_protect = true;
			out->cred_protect_value = (uint8_t)v;
		} else if (key_is(k, klen, "minPinLength") && for_make_credential) {
			bool v = false;
			if (osk_cbor_read_bool(r, &v) != OSK_OK) {
				rc = OSK_ERR_FORMAT;
				break;
			}
			out->min_pin_length = v;
		} else if (key_is(k, klen, "largeBlob")) {
			rc = parse_large_blob(r, for_make_credential, out);
			if (rc != OSK_OK)
				break;
		} else {
			/* Unbekannte Erweiterungen werden stillschweigend
			 * uebergangen - so verlangt es die Spezifikation. */
			if (osk_cbor_skip(r) != OSK_OK) {
				rc = OSK_ERR_FORMAT;
				break;
			}
		}
	}
	osk_cbor_leave(r);
	return rc;
}

osk_result_t osk_ctap_ext_make_output(const osk_ctap_ext_in_t *in,
				      const char *rp_id, size_t rp_id_len,
				      uint8_t cred_protect_set,
				      uint8_t *out, size_t cap, size_t *len_out)
{
	osk_cbor_writer_t w;
	size_t entries = 0;
	const bool want_minpin = in->min_pin_length &&
				 minpin_rp_allowed(rp_id, rp_id_len);

	if (in->cred_protect)
		entries++;
	if (in->hmac_secret_create)
		entries++;
	if (want_minpin)
		entries++;

	*len_out = 0;
	if (entries == 0)
		return OSK_OK;

	osk_cbor_writer_init(&w, out, cap);
	osk_cbor_put_map(&w, entries);
	/* Reihenfolge nach der CTAP-Sortierregel: kuerzer zuerst, dann
	 * lexikalisch - credProtect, hmac-secret, minPinLength. */
	if (in->cred_protect) {
		osk_cbor_put_text(&w, "credProtect");
		osk_cbor_put_uint(&w, cred_protect_set);
	}
	if (in->hmac_secret_create) {
		osk_cbor_put_text(&w, "hmac-secret");
		osk_cbor_put_bool(&w, true);
	}
	if (want_minpin) {
		osk_cbor_put_text(&w, "minPinLength");
		osk_cbor_put_uint(&w, osk_ctap_state()->min_pin_len);
	}

	if (!osk_cbor_writer_ok(&w))
		return OSK_ERR_FULL;
	*len_out = w.len;
	return OSK_OK;
}

osk_result_t osk_ctap_ext_get_output(const osk_ctap_ext_in_t *in,
				     const osk_credid_t *cred, bool uv,
				     uint8_t *out, size_t cap, size_t *len_out,
				     uint8_t *status_out)
{
	uint8_t shared[OSK_PIN_SHARED_LEN];
	uint8_t salts[64];
	size_t salts_len = 0;
	uint8_t cred_random[32];
	uint8_t outputs[64];
	uint8_t enc[64 + OSK_AES_BLOCK];
	size_t enc_len = 0;
	osk_cbor_writer_t w;
	osk_result_t rc = OSK_OK;

	*len_out = 0;
	*status_out = CTAP2_OK;

	if (!in->hmac_secret_get)
		return OSK_OK;
	/* Hat der Nachweis kein hmac-secret, wird die Erweiterung einfach
	 * uebergangen - kein Fehler (12.7). */
	if (!cred->hmac_secret)
		return OSK_OK;

	uint64_t protocol = in->hmac_protocol;
	if (!in->have_hmac_protocol)
		protocol = 1u;   /* Vorgabe, weil wir Protokoll 1 koennen */
	if (!osk_pin_protocol_supported(protocol)) {
		*status_out = CTAP2_ERR_PIN_AUTH_INVALID;
		return OSK_OK;
	}

	if (osk_pin_shared_secret(protocol, in->peer_pub, shared) != OSK_OK) {
		*status_out = CTAP1_ERR_INVALID_PARAMETER;
		goto done;
	}

	if (!osk_pin_verify_mac(protocol, shared, sizeof(shared),
				in->salt_enc, in->salt_enc_len,
				in->salt_auth, in->salt_auth_len)) {
		*status_out = CTAP2_ERR_PIN_AUTH_INVALID;
		goto done;
	}

	if (osk_pin_decrypt(protocol, shared, in->salt_enc, in->salt_enc_len,
			    salts, sizeof(salts), &salts_len) != OSK_OK ||
	    (salts_len != 32 && salts_len != 64)) {
		*status_out = CTAP1_ERR_INVALID_PARAMETER;
		goto done;
	}

	if (osk_ctap_key_secret(uv ? OSK_INFO_HMAC_UV : OSK_INFO_HMAC_NOUV,
				cred->salt, cred->rp_id_hash, cred_random) != OSK_OK) {
		*status_out = CTAP1_ERR_OTHER;
		goto done;
	}

	osk_hmac_sha256(cred_random, sizeof(cred_random), salts, 32, outputs);
	if (salts_len == 64)
		osk_hmac_sha256(cred_random, sizeof(cred_random), salts + 32, 32,
				outputs + 32);

	if (osk_pin_encrypt(protocol, shared, outputs, (salts_len == 64) ? 64u : 32u,
			    enc, sizeof(enc), &enc_len) != OSK_OK) {
		*status_out = CTAP1_ERR_OTHER;
		goto done;
	}

	osk_cbor_writer_init(&w, out, cap);
	osk_cbor_put_map(&w, 1);
	osk_cbor_put_text(&w, "hmac-secret");
	osk_cbor_put_bytes(&w, enc, enc_len);
	if (!osk_cbor_writer_ok(&w)) {
		rc = OSK_ERR_FULL;
		goto done;
	}
	*len_out = w.len;

done:
	osk_wipe(shared, sizeof(shared));
	osk_wipe(salts, sizeof(salts));
	osk_wipe(cred_random, sizeof(cred_random));
	osk_wipe(outputs, sizeof(outputs));
	osk_wipe(enc, sizeof(enc));
	return rc;
}

/* ---- largeBlob: Ablage im Tresor --------------------------------------- */
/* Der Tresor kennt Eintraege mit Name und Adresse. Wir legen je Nachweis
 * einen Eintrag mit der Adresse "osk-largeblob" ab; der Name ist das Salz
 * der Credential-ID als Hex. Den Tresor selbst baut der Tresor-Strang. */

#define LB_SITE "osk-largeblob"
#define LB_NAME_LEN (OSK_KEY_SALT_LEN * 2 + 1)

static void lb_name(const uint8_t salt[OSK_KEY_SALT_LEN], char out[LB_NAME_LEN])
{
	static const char hex[] = "0123456789abcdef";
	for (unsigned i = 0; i < OSK_KEY_SALT_LEN; i++) {
		out[i * 2] = hex[salt[i] >> 4];
		out[i * 2 + 1] = hex[salt[i] & 0x0f];
	}
	out[OSK_KEY_SALT_LEN * 2] = 0;
}

/* Sucht den Eintrag dieses Nachweises. Liefert OSK_ERR_NOT_FOUND, wenn es
 * keinen gibt. */
static osk_result_t lb_find(const uint8_t salt[OSK_KEY_SALT_LEN],
			    osk_item_id_t *id_out)
{
	char want[LB_NAME_LEN];
	size_t count = 0;

	lb_name(salt, want);
	if (osk_store_count(0, &count) != OSK_OK)
		return OSK_ERR_IO;

	for (size_t i = 0; i < count; i++) {
		osk_item_meta_t meta;
		memset(&meta, 0, sizeof(meta));
		if (osk_store_list(0, i, &meta) != OSK_OK)
			continue;
		if (meta.type != OSK_ITEM_FILE || !meta.site || !meta.name)
			continue;
		if (strcmp(meta.site, LB_SITE) != 0)
			continue;
		if (strcmp(meta.name, want) != 0)
			continue;
		*id_out = meta.id;
		return OSK_OK;
	}
	return OSK_ERR_NOT_FOUND;
}

osk_result_t osk_ctap_ext_unsigned_output(const osk_ctap_ext_in_t *in,
					  const osk_credid_t *cred,
					  bool allow_list_present,
					  uint8_t *out, size_t cap, size_t *len_out)
{
	osk_cbor_writer_t w;

	*len_out = 0;

	/* MakeCredential: wir koennen grosse Daten, also sagen wir das. */
	if (in->large_blob_create) {
		osk_cbor_writer_init(&w, out, cap);
		osk_cbor_put_map(&w, 1);
		osk_cbor_put_text(&w, "largeBlob");
		osk_cbor_put_map(&w, 1);
		osk_cbor_put_text(&w, "supported");
		osk_cbor_put_bool(&w, true);
		if (!osk_cbor_writer_ok(&w))
			return OSK_ERR_FULL;
		*len_out = w.len;
		return OSK_OK;
	}

	if (in->large_blob_read) {
		uint8_t blob[512];
		size_t blob_len = 0;
		osk_item_id_t id = 0;

		if (lb_find(cred->salt, &id) != OSK_OK)
			return OSK_OK;   /* nichts gespeichert: Erweiterung schweigt */
		if (osk_store_reveal(id, "largeBlob", blob, sizeof(blob),
				     &blob_len) != OSK_OK || blob_len < 4)
			return OSK_OK;

		/* Die ersten vier Bytes sind die urspruengliche Groesse. */
		const uint64_t orig = ((uint64_t)blob[0] << 24) | ((uint64_t)blob[1] << 16) |
				      ((uint64_t)blob[2] << 8) | (uint64_t)blob[3];
		osk_cbor_writer_init(&w, out, cap);
		osk_cbor_put_map(&w, 1);
		osk_cbor_put_text(&w, "largeBlob");
		osk_cbor_put_map(&w, 2);
		osk_cbor_put_text(&w, "blob");
		osk_cbor_put_bytes(&w, blob + 4, blob_len - 4);
		osk_cbor_put_text(&w, "originalSize");
		osk_cbor_put_uint(&w, orig);
		osk_wipe(blob, sizeof(blob));
		if (!osk_cbor_writer_ok(&w))
			return OSK_ERR_FULL;
		*len_out = w.len;
		return OSK_OK;
	}

	if (in->large_blob_write) {
		bool written = false;
		uint8_t buf[516];

		/* Geschrieben wird nur mit allowList - sonst waere nicht
		 * eindeutig, zu welchem Nachweis die Daten gehoeren (12.4). */
		if (allow_list_present && in->large_blob_write_len + 4 <= sizeof(buf)) {
			char name[LB_NAME_LEN];
			osk_item_id_t id = 0;
			osk_item_meta_t meta;

			lb_name(cred->salt, name);
			buf[0] = (uint8_t)(in->large_blob_original_size >> 24);
			buf[1] = (uint8_t)(in->large_blob_original_size >> 16);
			buf[2] = (uint8_t)(in->large_blob_original_size >> 8);
			buf[3] = (uint8_t)in->large_blob_original_size;
			memcpy(buf + 4, in->large_blob_write, in->large_blob_write_len);

			memset(&meta, 0, sizeof(meta));
			meta.type = OSK_ITEM_FILE;
			meta.profile = 0;
			meta.name = name;
			meta.site = LB_SITE;
			meta.mirrored = true;

			if (lb_find(cred->salt, &id) == OSK_OK) {
				written = osk_store_update(id, &meta, buf,
							   in->large_blob_write_len + 4) == OSK_OK;
			} else {
				written = osk_store_add(&meta, buf,
							in->large_blob_write_len + 4,
							&id) == OSK_OK;
			}
			osk_wipe(buf, sizeof(buf));
		}

		osk_cbor_writer_init(&w, out, cap);
		osk_cbor_put_map(&w, 1);
		osk_cbor_put_text(&w, "largeBlob");
		osk_cbor_put_map(&w, 1);
		osk_cbor_put_text(&w, "written");
		osk_cbor_put_bool(&w, written);
		if (!osk_cbor_writer_ok(&w))
			return OSK_ERR_FULL;
		*len_out = w.len;
		return OSK_OK;
	}

	return OSK_OK;
}
