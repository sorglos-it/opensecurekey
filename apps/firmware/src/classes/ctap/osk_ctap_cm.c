/* authenticatorCredentialManagement (0x0A). Siehe osk_ctap_cm.h.
 * Spezifikation: CTAP 2.2 (14.07.2025), Abschnitt 6.8. */
#include <string.h>
#include "osk_ctap_cm.h"
#include "osk_ctap_internal.h"
#include "osk_ctap_key.h"
#include "osk_credid.h"
#include "osk_crypto.h"
#include "osk_pin.h"
#include "osk_store.h"

/* Schluessel der Anfrage (6.8, Tabelle) */
#define KEY_SUBCOMMAND 0x01
#define KEY_SUBCOMMAND_PARAMS 0x02
#define KEY_PIN_PROTOCOL 0x03
#define KEY_PIN_AUTH 0x04

/* Unterbefehle */
#define SUB_METADATA 0x01
#define SUB_RPS_BEGIN 0x02
#define SUB_RPS_NEXT 0x03
#define SUB_CREDS_BEGIN 0x04
#define SUB_CREDS_NEXT 0x05
#define SUB_DELETE 0x06
#define SUB_UPDATE_USER 0x07

/* Schluessel von subCommandParams */
#define PARAM_RP_ID_HASH 0x01
#define PARAM_CRED_ID 0x02
#define PARAM_USER 0x03

/* Antwortschluessel */
#define RES_EXISTING_COUNT 0x01
#define RES_REMAINING 0x02
#define RES_RP 0x03
#define RES_RP_ID_HASH 0x04
#define RES_TOTAL_RPS 0x05
#define RES_USER 0x06
#define RES_CRED_ID 0x07
#define RES_PUBLIC_KEY 0x08
#define RES_TOTAL_CREDS 0x09
#define RES_CRED_PROTECT 0x0a

typedef struct {
	uint64_t sub;
	bool have_sub;
	const uint8_t *params; /* rohe Bytes, so wie angekommen */
	size_t params_len;
	uint64_t protocol;
	bool have_protocol;
	const uint8_t *pin_auth;
	size_t pin_auth_len;
	bool have_pin_auth;
} cm_req_t;

static uint8_t parse_request(const uint8_t *cbor, size_t len, cm_req_t *q)
{
	osk_cbor_reader_t r;
	size_t count = 0;

	memset(q, 0, sizeof(*q));

	if (osk_cbor_validate(cbor, len) != OSK_OK)
		return CTAP2_ERR_INVALID_CBOR;

	osk_cbor_reader_init(&r, cbor, len);
	if (osk_cbor_enter_map(&r, &count) != OSK_OK)
		return CTAP2_ERR_INVALID_CBOR;

	for (size_t i = 0; i < count; i++) {
		uint64_t key = 0;
		if (osk_cbor_read_uint(&r, &key) != OSK_OK)
			return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
		switch (key) {
		case KEY_SUBCOMMAND:
			if (osk_cbor_read_uint(&r, &q->sub) != OSK_OK)
				return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
			q->have_sub = true;
			break;
		case KEY_SUBCOMMAND_PARAMS: {
			/* Wie bei authenticatorConfig: der MAC geht ueber die
			 * Bytes, die ANKAMEN. Darum der Ausschnitt statt eines
			 * Nachbaus. */
			const size_t start = r.pos;
			if (osk_cbor_skip(&r) != OSK_OK)
				return CTAP2_ERR_INVALID_CBOR;
			q->params = cbor + start;
			q->params_len = r.pos - start;
			break;
		}
		case KEY_PIN_PROTOCOL:
			if (osk_cbor_read_uint(&r, &q->protocol) != OSK_OK)
				return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
			q->have_protocol = true;
			break;
		case KEY_PIN_AUTH:
			if (osk_cbor_read_bytes(&r, &q->pin_auth, &q->pin_auth_len) != OSK_OK)
				return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
			q->have_pin_auth = true;
			break;
		default:
			if (osk_cbor_skip(&r) != OSK_OK)
				return CTAP2_ERR_INVALID_CBOR;
			break;
		}
	}
	osk_cbor_leave(&r);

	if (!q->have_sub)
		return CTAP2_ERR_MISSING_PARAMETER;
	return CTAP2_OK;
}

/* Freigabe. Die Nachricht ist subCommand [|| subCommandParams] - OHNE die
 * 32 Fuellbytes und OHNE Befehlsbyte, anders als bei authenticatorConfig
 * (6.11). Dieselben zwei Woerter, zwei verschiedene Nachrichten: wer das
 * verwechselt, baut einen MAC, der von einem Befehl auf den anderen passt. */
static uint8_t check_auth(const cm_req_t *q)
{
	uint8_t msg[1 + 512];
	size_t at = 0;

	if (!q->have_pin_auth)
		return CTAP2_ERR_PUAT_REQUIRED;
	if (!q->have_protocol)
		return CTAP2_ERR_MISSING_PARAMETER;
	if (!osk_pin_protocol_supported(q->protocol))
		return CTAP1_ERR_INVALID_PARAMETER;
	if (q->params_len > sizeof(msg) - 1)
		return CTAP1_ERR_INVALID_PARAMETER;

	msg[at++] = (uint8_t)q->sub;
	if (q->params_len) {
		memcpy(msg + at, q->params, q->params_len);
		at += q->params_len;
	}

	if (!osk_pin_verify_token(q->protocol, msg, at, q->pin_auth, q->pin_auth_len))
		return CTAP2_ERR_PIN_AUTH_INVALID;
	if (!osk_pin_has_permission(OSK_PERM_CM))
		return CTAP2_ERR_PIN_AUTH_INVALID;
	return CTAP2_OK;
}

/* ---- Der Tresor als Liste residenter Nachweise ------------------------- */

/* Ein Platz im Tresor, soweit er ohne Oeffnen zu sehen ist. */
typedef struct {
	osk_item_id_t id;
	char rp_id[OSK_MAX_RP_ID];
	uint8_t rp_id_hash[32];
} rk_slot_t;

/* Ist der Eintrag an Platz `index` ein residenter Nachweis? Dann die nicht
 * geheimen Angaben herausgeben. Oeffnet nichts. */
static bool slot_at(size_t index, rk_slot_t *out)
{
	osk_item_meta_t meta;

	memset(&meta, 0, sizeof(meta));
	if (osk_store_list(0, index, &meta) != OSK_OK)
		return false;
	if (!meta.site || strcmp(meta.site, OSK_RK_SITE) != 0)
		return false;
	if (meta.tag_len != 32)
		return false;

	memset(out, 0, sizeof(*out));
	out->id = meta.id;
	memcpy(out->rp_id_hash, meta.tag, 32);
	if (meta.name) {
		const size_t n = strlen(meta.name);
		if (n >= sizeof(out->rp_id))
			return false;
		memcpy(out->rp_id, meta.name, n + 1);
	}
	return true;
}

static size_t store_slots(void)
{
	size_t count = 0;

	if (osk_store_count(0, &count) != OSK_OK)
		return 0;
	return count;
}

/* Wie viele residente Nachweise liegen im Tresor? */
static size_t count_resident(void)
{
	const size_t slots = store_slots();
	size_t n = 0;

	for (size_t i = 0; i < slots; i++) {
		rk_slot_t s;
		if (slot_at(i, &s))
			n++;
	}
	return n;
}

/* Erster Treffer dieses Dienstes? Nur der wird bei enumerateRPs gemeldet -
 * so bleibt die Liste ohne Doppel, OHNE sich merken zu muessen, was schon
 * genannt wurde. Ein Merkzettel waere ein zweiter Stand neben dem Tresor. */
static bool is_first_of_rp(size_t index, const uint8_t rp_id_hash[32])
{
	for (size_t i = 0; i < index; i++) {
		rk_slot_t s;
		if (!slot_at(i, &s))
			continue;
		if (memcmp(s.rp_id_hash, rp_id_hash, 32) == 0)
			return false;
	}
	return true;
}

/* Der Filter des Tokens: hat es eine RP-ID, gilt es nur fuer diesen Dienst. */
static bool rp_allowed(const uint8_t rp_id_hash[32])
{
	const osk_pin_state_t *pin = osk_pin_state();

	if (!pin->rp_id_set)
		return true;
	return memcmp(pin->rp_id_hash, rp_id_hash, 32) == 0;
}

static size_t count_rps(void)
{
	const size_t slots = store_slots();
	size_t n = 0;

	for (size_t i = 0; i < slots; i++) {
		rk_slot_t s;
		if (!slot_at(i, &s))
			continue;
		if (!rp_allowed(s.rp_id_hash))
			continue;
		if (is_first_of_rp(i, s.rp_id_hash))
			n++;
	}
	return n;
}

static size_t count_creds_of(const uint8_t rp_id_hash[32])
{
	const size_t slots = store_slots();
	size_t n = 0;

	for (size_t i = 0; i < slots; i++) {
		rk_slot_t s;
		if (!slot_at(i, &s))
			continue;
		if (memcmp(s.rp_id_hash, rp_id_hash, 32) != 0)
			continue;
		n++;
	}
	return n;
}

/* Den Satz eines Platzes oeffnen. Das kostet eine Entschluesselung, darum
 * erst hier und nicht beim Suchen. */
static bool read_cred(const rk_slot_t *s, osk_cred_t *cred_out, osk_credid_t *cid_out)
{
	uint8_t rec[OSK_RK_RECORD_MAX];
	size_t rec_len = 0;
	bool ok;

	memset(cred_out, 0, sizeof(*cred_out));
	if (osk_store_reveal(s->id, "FIDO2-Verwaltung", rec, sizeof(rec), &rec_len) != OSK_OK)
		return false;
	ok = osk_ctap_rk_unpack(rec, rec_len, cred_out);
	osk_wipe(rec, sizeof(rec));
	if (!ok)
		return false;
	if (osk_credid_unpack(cred_out->cred_id, cred_out->cred_id_len, cid_out) != OSK_OK)
		return false;
	/* Der Satz und die Credential-ID muessen denselben Dienst nennen.
	 * Weichen sie ab, ist der Eintrag nicht von uns oder verbogen - dann
	 * wird er nicht gemeldet, statt irgendetwas zu melden. */
	return memcmp(cid_out->rp_id_hash, s->rp_id_hash, 32) == 0;
}

/* ---- 6.8.1 getCredsMetadata -------------------------------------------- */

static uint8_t do_metadata(osk_cbor_writer_t *w)
{
	const size_t used = count_resident();
	const size_t left = (used < OSK_MAX_RESIDENT) ? OSK_MAX_RESIDENT - used : 0u;

	osk_cbor_put_map(w, 2);
	osk_cbor_put_uint(w, RES_EXISTING_COUNT);
	osk_cbor_put_uint(w, used);
	osk_cbor_put_uint(w, RES_REMAINING);
	osk_cbor_put_uint(w, left);
	return osk_cbor_writer_ok(w) ? CTAP2_OK : CTAP2_ERR_REQUEST_TOO_LARGE;
}

/* ---- 6.8.2/6.8.3 enumerateRPs ------------------------------------------ */

static uint8_t emit_rp(const rk_slot_t *s, size_t total, bool with_total, osk_cbor_writer_t *w)
{
	osk_cbor_put_map(w, with_total ? 3u : 2u);
	osk_cbor_put_uint(w, RES_RP);
	osk_cbor_put_map(w, 1);
	osk_cbor_put_text(w, "id");
	osk_cbor_put_text(w, s->rp_id);
	osk_cbor_put_uint(w, RES_RP_ID_HASH);
	osk_cbor_put_bytes(w, s->rp_id_hash, 32);
	if (with_total) {
		osk_cbor_put_uint(w, RES_TOTAL_RPS);
		osk_cbor_put_uint(w, total);
	}
	return osk_cbor_writer_ok(w) ? CTAP2_OK : CTAP2_ERR_REQUEST_TOO_LARGE;
}

/* Naechster Dienst ab Platz *from. Setzt *from hinter den Treffer. */
static bool next_rp(size_t *from, rk_slot_t *out)
{
	const size_t slots = store_slots();

	for (size_t i = *from; i < slots; i++) {
		rk_slot_t s;
		if (!slot_at(i, &s))
			continue;
		if (!rp_allowed(s.rp_id_hash))
			continue;
		if (!is_first_of_rp(i, s.rp_id_hash))
			continue;
		*out = s;
		*from = i + 1;
		return true;
	}
	*from = slots;
	return false;
}

static uint8_t do_rps_begin(osk_cbor_writer_t *w)
{
	osk_cm_enum_t *e = &osk_ctap_state()->cm_enum;
	rk_slot_t s;
	size_t at = 0;

	const size_t total = count_rps();
	if (total == 0)
		return CTAP2_ERR_NO_CREDENTIALS;
	if (!next_rp(&at, &s))
		return CTAP2_ERR_NO_CREDENTIALS;

	memset(e, 0, sizeof(*e));
	e->rp_active = true;
	e->cursor = at;
	return emit_rp(&s, total, true, w);
}

static uint8_t do_rps_next(osk_cbor_writer_t *w)
{
	osk_cm_enum_t *e = &osk_ctap_state()->cm_enum;
	rk_slot_t s;

	if (!e->rp_active)
		return CTAP2_ERR_NOT_ALLOWED;
	if (!next_rp(&e->cursor, &s)) {
		e->rp_active = false;
		return CTAP2_ERR_NO_CREDENTIALS;
	}
	return emit_rp(&s, 0, false, w);
}

/* ---- 6.8.4/6.8.5 enumerateCredentials ---------------------------------- */

static uint8_t emit_cred(const rk_slot_t *s, size_t total, bool with_total, osk_cbor_writer_t *w)
{
	osk_cred_t cred;
	osk_credid_t cid;
	uint8_t pub[64];

	if (!read_cred(s, &cred, &cid))
		return CTAP1_ERR_OTHER;
	if (osk_ctap_key_public(cid.salt, cid.rp_id_hash, pub) != OSK_OK)
		return CTAP1_ERR_OTHER;

	osk_cbor_put_map(w, with_total ? 5u : 4u);
	osk_cbor_put_uint(w, RES_USER);
	osk_cbor_put_map(w, cred.user_name[0] ? 2u : 1u);
	osk_cbor_put_text(w, "id");
	osk_cbor_put_bytes(w, cred.user_id, cred.user_id_len);
	if (cred.user_name[0]) {
		osk_cbor_put_text(w, "name");
		osk_cbor_put_text(w, cred.user_name);
	}
	osk_cbor_put_uint(w, RES_CRED_ID);
	osk_cbor_put_map(w, 2);
	osk_cbor_put_text(w, "id");
	osk_cbor_put_bytes(w, cred.cred_id, cred.cred_id_len);
	osk_cbor_put_text(w, "type");
	osk_cbor_put_text(w, "public-key");
	osk_cbor_put_uint(w, RES_PUBLIC_KEY);
	osk_ctap_put_cose_p256(w, pub);
	if (with_total) {
		osk_cbor_put_uint(w, RES_TOTAL_CREDS);
		osk_cbor_put_uint(w, total);
	}
	osk_cbor_put_uint(w, RES_CRED_PROTECT);
	osk_cbor_put_uint(w, cid.cred_protect);

	osk_wipe(&cred, sizeof(cred));
	return osk_cbor_writer_ok(w) ? CTAP2_OK : CTAP2_ERR_REQUEST_TOO_LARGE;
}

static bool next_cred(size_t *from, const uint8_t rp_id_hash[32], rk_slot_t *out)
{
	const size_t slots = store_slots();

	for (size_t i = *from; i < slots; i++) {
		rk_slot_t s;
		if (!slot_at(i, &s))
			continue;
		if (memcmp(s.rp_id_hash, rp_id_hash, 32) != 0)
			continue;
		*out = s;
		*from = i + 1;
		return true;
	}
	*from = slots;
	return false;
}

/* Liest rpIDHash (0x01) aus subCommandParams. */
static uint8_t read_param_rp_hash(const cm_req_t *q, uint8_t out[32])
{
	osk_cbor_reader_t r;
	size_t count = 0;
	bool found = false;

	if (!q->params || q->params_len == 0)
		return CTAP2_ERR_MISSING_PARAMETER;

	osk_cbor_reader_init(&r, q->params, q->params_len);
	if (osk_cbor_enter_map(&r, &count) != OSK_OK)
		return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
	for (size_t i = 0; i < count; i++) {
		uint64_t key = 0;
		if (osk_cbor_read_uint(&r, &key) != OSK_OK)
			return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
		if (key == PARAM_RP_ID_HASH) {
			const uint8_t *p = NULL;
			size_t plen = 0;
			if (osk_cbor_read_bytes(&r, &p, &plen) != OSK_OK)
				return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
			if (plen != 32)
				return CTAP1_ERR_INVALID_PARAMETER;
			memcpy(out, p, 32);
			found = true;
		} else if (osk_cbor_skip(&r) != OSK_OK) {
			return CTAP2_ERR_INVALID_CBOR;
		}
	}
	osk_cbor_leave(&r);
	return found ? CTAP2_OK : CTAP2_ERR_MISSING_PARAMETER;
}

static uint8_t do_creds_begin(const cm_req_t *q, osk_cbor_writer_t *w)
{
	osk_cm_enum_t *e = &osk_ctap_state()->cm_enum;
	uint8_t rp_hash[32];
	rk_slot_t s;
	size_t at = 0;

	const uint8_t perr = read_param_rp_hash(q, rp_hash);
	if (perr != CTAP2_OK)
		return perr;
	if (!rp_allowed(rp_hash))
		return CTAP2_ERR_PIN_AUTH_INVALID;

	const size_t total = count_creds_of(rp_hash);
	if (total == 0)
		return CTAP2_ERR_NO_CREDENTIALS;
	if (!next_cred(&at, rp_hash, &s))
		return CTAP2_ERR_NO_CREDENTIALS;

	memset(e, 0, sizeof(*e));
	e->cred_active = true;
	e->cursor = at;
	memcpy(e->rp_id_hash, rp_hash, 32);
	return emit_cred(&s, total, true, w);
}

static uint8_t do_creds_next(osk_cbor_writer_t *w)
{
	osk_cm_enum_t *e = &osk_ctap_state()->cm_enum;
	rk_slot_t s;

	if (!e->cred_active)
		return CTAP2_ERR_NOT_ALLOWED;
	if (!next_cred(&e->cursor, e->rp_id_hash, &s)) {
		e->cred_active = false;
		return CTAP2_ERR_NO_CREDENTIALS;
	}
	return emit_cred(&s, 0, false, w);
}

/* ---- 6.8.6/6.8.7 loeschen und aendern ---------------------------------- */

/* Liest credentialID (0x02) und, wenn verlangt, user (0x03). */
typedef struct {
	const uint8_t *cred_id;
	size_t cred_id_len;
	const uint8_t *user_id;
	size_t user_id_len;
	const char *user_name;
	size_t user_name_len;
	bool have_user;
} target_t;

static uint8_t read_target(const cm_req_t *q, target_t *t)
{
	osk_cbor_reader_t r;
	size_t count = 0;

	memset(t, 0, sizeof(*t));
	if (!q->params || q->params_len == 0)
		return CTAP2_ERR_MISSING_PARAMETER;

	osk_cbor_reader_init(&r, q->params, q->params_len);
	if (osk_cbor_enter_map(&r, &count) != OSK_OK)
		return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;

	for (size_t i = 0; i < count; i++) {
		uint64_t key = 0;
		if (osk_cbor_read_uint(&r, &key) != OSK_OK)
			return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
		if (key == PARAM_CRED_ID) {
			size_t fields = 0;
			if (osk_cbor_enter_map(&r, &fields) != OSK_OK)
				return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
			for (size_t f = 0; f < fields; f++) {
				const char *k = NULL;
				size_t klen = 0;
				if (osk_cbor_read_text(&r, &k, &klen) != OSK_OK)
					return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
				if (klen == 2 && memcmp(k, "id", 2) == 0) {
					if (osk_cbor_read_bytes(&r, &t->cred_id,
								&t->cred_id_len) != OSK_OK)
						return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
				} else if (osk_cbor_skip(&r) != OSK_OK) {
					return CTAP2_ERR_INVALID_CBOR;
				}
			}
			osk_cbor_leave(&r);
		} else if (key == PARAM_USER) {
			size_t fields = 0;
			if (osk_cbor_enter_map(&r, &fields) != OSK_OK)
				return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
			t->have_user = true;
			for (size_t f = 0; f < fields; f++) {
				const char *k = NULL;
				size_t klen = 0;
				if (osk_cbor_read_text(&r, &k, &klen) != OSK_OK)
					return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
				if (klen == 2 && memcmp(k, "id", 2) == 0) {
					if (osk_cbor_read_bytes(&r, &t->user_id,
								&t->user_id_len) != OSK_OK)
						return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
				} else if (klen == 4 && memcmp(k, "name", 4) == 0) {
					if (osk_cbor_read_text(&r, &t->user_name,
							       &t->user_name_len) != OSK_OK)
						return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
				} else if (osk_cbor_skip(&r) != OSK_OK) {
					return CTAP2_ERR_INVALID_CBOR;
				}
			}
			osk_cbor_leave(&r);
		} else if (osk_cbor_skip(&r) != OSK_OK) {
			return CTAP2_ERR_INVALID_CBOR;
		}
	}
	osk_cbor_leave(&r);

	if (!t->cred_id || t->cred_id_len == 0)
		return CTAP2_ERR_MISSING_PARAMETER;
	if (t->cred_id_len > OSK_MAX_CRED_ID)
		return CTAP2_ERR_NO_CREDENTIALS;
	return CTAP2_OK;
}

/* Sucht den Platz, dessen Satz genau diese Credential-ID fuehrt. Zuerst
 * wird die ID selbst geprueft (MAC), dann nur noch der Dienst durchsucht,
 * zu dem sie gehoert - eine fremde oder verbogene ID fuehrt zu keiner
 * Entschluesselung. */
static uint8_t find_by_cred_id(const uint8_t *cred_id, size_t cred_id_len, rk_slot_t *slot_out,
			       osk_cred_t *cred_out, osk_credid_t *cid_out)
{
	osk_credid_t want;
	size_t at = 0;
	rk_slot_t s;

	if (osk_credid_unpack(cred_id, cred_id_len, &want) != OSK_OK)
		return CTAP2_ERR_NO_CREDENTIALS;
	if (!rp_allowed(want.rp_id_hash))
		return CTAP2_ERR_PIN_AUTH_INVALID;
	/* Eine nicht auffindbare ID ist echt, hat aber keinen Eintrag - es
	 * gibt daran nichts zu verwalten. */
	if (!want.resident)
		return CTAP2_ERR_NO_CREDENTIALS;

	while (next_cred(&at, want.rp_id_hash, &s)) {
		osk_cred_t cred;
		osk_credid_t cid;
		if (!read_cred(&s, &cred, &cid))
			continue;
		if (cred.cred_id_len == cred_id_len &&
		    memcmp(cred.cred_id, cred_id, cred_id_len) == 0) {
			*slot_out = s;
			*cred_out = cred;
			*cid_out = cid;
			return CTAP2_OK;
		}
		osk_wipe(&cred, sizeof(cred));
	}
	return CTAP2_ERR_NO_CREDENTIALS;
}

static uint8_t do_delete(const cm_req_t *q)
{
	target_t t;
	rk_slot_t s;
	osk_cred_t cred;
	osk_credid_t cid;

	const uint8_t perr = read_target(q, &t);
	if (perr != CTAP2_OK)
		return perr;

	const uint8_t ferr = find_by_cred_id(t.cred_id, t.cred_id_len, &s, &cred, &cid);
	if (ferr != CTAP2_OK)
		return ferr;
	osk_wipe(&cred, sizeof(cred));

	if (osk_store_delete(s.id) != OSK_OK)
		return CTAP1_ERR_OTHER;
	return CTAP2_OK;
}

static uint8_t do_update_user(const cm_req_t *q)
{
	target_t t;
	rk_slot_t s;
	osk_cred_t cred;
	osk_credid_t cid;
	osk_item_meta_t meta;
	uint8_t rec[OSK_RK_RECORD_MAX];
	char name[OSK_MAX_USER_NAME];
	uint8_t code = CTAP2_OK;

	const uint8_t perr = read_target(q, &t);
	if (perr != CTAP2_OK)
		return perr;
	if (!t.have_user || !t.user_id)
		return CTAP2_ERR_MISSING_PARAMETER;

	const uint8_t ferr = find_by_cred_id(t.cred_id, t.cred_id_len, &s, &cred, &cid);
	if (ferr != CTAP2_OK)
		return ferr;

	/* Die Nutzerkennung darf sich NICHT aendern. Sie ist das, woran der
	 * Dienst das Konto erkennt; liesse man sie austauschen, wuerde ein
	 * vorhandener Passkey still auf ein anderes Konto umgehaengt. Die
	 * Spezifikation nennt dafuer keinen Fehlerwert - CTAP1_ERR_INVALID_
	 * PARAMETER ist unsere Wahl, weil der Parameter nicht passt. */
	if (t.user_id_len != cred.user_id_len ||
	    memcmp(t.user_id, cred.user_id, cred.user_id_len) != 0) {
		osk_wipe(&cred, sizeof(cred));
		return CTAP1_ERR_INVALID_PARAMETER;
	}

	if (t.user_name_len >= sizeof(name)) {
		osk_wipe(&cred, sizeof(cred));
		return CTAP1_ERR_INVALID_PARAMETER;
	}
	memset(name, 0, sizeof(name));
	if (t.user_name_len)
		memcpy(name, t.user_name, t.user_name_len);

	const size_t rec_len = osk_ctap_rk_pack(cred.cred_id, cred.cred_id_len, cred.user_id,
						cred.user_id_len, name, rec, sizeof(rec));
	if (rec_len == 0) {
		osk_wipe(&cred, sizeof(cred));
		return CTAP1_ERR_OTHER;
	}

	/* Alles ausser dem Namen bleibt, wie es war - auch der Suchschluessel.
	 * Ihn hier neu zu setzen waere die Gelegenheit, ihn falsch zu setzen. */
	memset(&meta, 0, sizeof(meta));
	meta.type = OSK_ITEM_FILE;
	meta.profile = 0;
	meta.name = s.rp_id;
	meta.site = OSK_RK_SITE;
	meta.mirrored = true;
	meta.changed_utc = osk_ctap_now_ms() / 1000u;
	memcpy(meta.tag, s.rp_id_hash, 32);
	meta.tag_len = 32;

	if (osk_store_update(s.id, &meta, rec, rec_len) != OSK_OK)
		code = CTAP1_ERR_OTHER;

	osk_wipe(rec, sizeof(rec));
	osk_wipe(&cred, sizeof(cred));
	return code;
}

/* ---- Verteiler --------------------------------------------------------- */

uint8_t osk_ctap_cm_handle(const uint8_t *cbor, size_t len, osk_cbor_writer_t *w)
{
	cm_req_t q;
	osk_cm_enum_t *e = &osk_ctap_state()->cm_enum;

	const uint8_t perr = parse_request(cbor, len, &q);
	if (perr != CTAP2_OK)
		return perr;

	if (!osk_pin_state()->pin_set)
		return CTAP2_ERR_PIN_NOT_SET;

	/* Die beiden GetNext-Unterbefehle bringen keinen MAC mit (6.8): sie
	 * setzen eine Aufzaehlung fort, die mit einem geprueften Begin
	 * angefangen hat. Ohne laufende Aufzaehlung sind sie nicht erlaubt -
	 * sonst waere sie ein Weg, ohne Freigabe zu lesen. */
	switch (q.sub) {
	case SUB_RPS_NEXT:
		return do_rps_next(w);
	case SUB_CREDS_NEXT:
		return do_creds_next(w);
	default:
		break;
	}

	/* Jeder andere Unterbefehl beendet eine laufende Aufzaehlung. */
	e->rp_active = false;
	e->cred_active = false;

	if (q.sub != SUB_METADATA && q.sub != SUB_RPS_BEGIN && q.sub != SUB_CREDS_BEGIN &&
	    q.sub != SUB_DELETE && q.sub != SUB_UPDATE_USER)
		return CTAP2_ERR_INVALID_SUBCOMMAND;

	const uint8_t aerr = check_auth(&q);
	if (aerr != CTAP2_OK)
		return aerr;

	switch (q.sub) {
	case SUB_METADATA:
		return do_metadata(w);
	case SUB_RPS_BEGIN:
		return do_rps_begin(w);
	case SUB_CREDS_BEGIN:
		return do_creds_begin(&q, w);
	case SUB_DELETE:
		return do_delete(&q);
	case SUB_UPDATE_USER:
		return do_update_user(&q);
	default:
		return CTAP2_ERR_INVALID_SUBCOMMAND;
	}
}
