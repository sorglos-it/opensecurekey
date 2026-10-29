/* Die Attrappen. Siehe mocks.h - sie rechnen deterministisch, aber kein
 * echtes P-256. */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "mocks.h"
#include "osk_se050.h"
#include "osk_store.h"
#include "osk_ui.h"
#include "osk_transport.h"
#include "osk_crypto.h"
#include "osk_ctap_key.h"

#define SLOT_MAX 4
#define STORE_MAX 16
#define STORE_SECRET_MAX 600
#define STORE_TEXT_MAX 128

/* ---- Uhr --------------------------------------------------------------- */

static uint64_t g_now;

void mock_clock_set(uint64_t ms)
{
	g_now = ms;
}
void mock_clock_advance(uint64_t ms)
{
	g_now += ms;
}

uint64_t osk_ctap_now_ms(void)
{
	return g_now;
}

/* ---- SE050 ------------------------------------------------------------- */

typedef struct {
	uint32_t id;
	bool used;
	uint8_t seed[32];
} slot_t;

static slot_t g_slots[SLOT_MAX];
static uint8_t g_root[32];
static bool g_open;
static uint8_t g_pin_hash[16];
static bool g_pin_set;
static uint8_t g_tries = 8;
static uint32_t g_rand_state;

static slot_t *slot_find(uint32_t id)
{
	for (unsigned i = 0; i < SLOT_MAX; i++) {
		if (g_slots[i].used && g_slots[i].id == id)
			return &g_slots[i];
	}
	return NULL;
}

static slot_t *slot_alloc(uint32_t id)
{
	slot_t *s = slot_find(id);
	if (s)
		return s;
	for (unsigned i = 0; i < SLOT_MAX; i++) {
		if (!g_slots[i].used) {
			g_slots[i].used = true;
			g_slots[i].id = id;
			return &g_slots[i];
		}
	}
	return NULL;
}

/* Oeffentlicher Punkt aus einem Startwert: kein echter Kurvenpunkt, aber
 * eindeutig je Startwert - genau das pruefen die Faelle. */
static void pub_from_seed(const uint8_t seed[32], uint8_t out[65])
{
	uint8_t x[32], y[32];
	osk_sha256_t c;

	out[0] = 0x04;
	osk_sha256_init(&c);
	osk_sha256_update(&c, seed, 32);
	osk_sha256_update(&c, (const uint8_t *)"x", 1);
	osk_sha256_final(&c, x);
	osk_sha256_init(&c);
	osk_sha256_update(&c, seed, 32);
	osk_sha256_update(&c, (const uint8_t *)"y", 1);
	osk_sha256_final(&c, y);
	memcpy(out + 1, x, 32);
	memcpy(out + 33, y, 32);
}

osk_result_t osk_se050_open(void)
{
	g_open = true;
	return OSK_OK;
}

void osk_se050_close(void)
{
	g_open = false;
}

osk_result_t osk_se050_curves(uint32_t *mask_out)
{
	*mask_out = 1u << OSK_CURVE_P256;
	return OSK_OK;
}

osk_result_t osk_se050_random(uint8_t *out, size_t len)
{
	if (!g_open)
		return OSK_ERR_IO;
	for (size_t i = 0; i < len; i++) {
		g_rand_state = g_rand_state * 1664525u + 1013904223u;
		out[i] = (uint8_t)(g_rand_state >> 24);
	}
	return OSK_OK;
}

void mock_se050_seed_random(uint32_t seed)
{
	g_rand_state = seed;
}

osk_result_t osk_se050_key_create(osk_key_id_t id, osk_curve_t curve,
				  const osk_key_policy_t *policy)
{
	(void)policy;
	if (curve != OSK_CURVE_P256)
		return OSK_ERR_UNSUPPORTED;
	slot_t *s = slot_alloc(id);
	if (!s)
		return OSK_ERR_FULL;
	osk_se050_random(s->seed, sizeof(s->seed));
	if (id == OSK_KEY_ID_ROOT)
		memcpy(g_root, s->seed, sizeof(g_root));
	return OSK_OK;
}

osk_result_t osk_se050_key_delete(osk_key_id_t id)
{
	slot_t *s = slot_find(id);
	if (!s)
		return OSK_ERR_NOT_FOUND;
	memset(s, 0, sizeof(*s));
	return OSK_OK;
}

osk_result_t osk_se050_key_public(osk_key_id_t id, uint8_t *out, size_t cap, size_t *len_out)
{
	const slot_t *s = slot_find(id);
	if (!s)
		return OSK_ERR_NOT_FOUND;
	if (cap < 65)
		return OSK_ERR_FULL;
	uint8_t pub[65];
	pub_from_seed(s->seed, pub);
	memcpy(out, pub, 65);
	*len_out = 65;
	return OSK_OK;
}

/* Unterschrift: HMAC(seed, hash) wird in r und s geteilt und als DER
 * verpackt. Form und Laenge stimmen, die Mathematik nicht. */
static size_t der_encode_int(const uint8_t v[32], uint8_t *out)
{
	size_t at = 0;
	size_t start = 0;

	while (start < 31 && v[start] == 0)
		start++;
	const bool pad = (v[start] & 0x80) != 0;
	const size_t len = 32 - start + (pad ? 1u : 0u);

	out[at++] = 0x02;
	out[at++] = (uint8_t)len;
	if (pad)
		out[at++] = 0x00;
	memcpy(out + at, v + start, 32 - start);
	at += 32 - start;
	return at;
}

osk_result_t osk_se050_sign(osk_key_id_t id, const uint8_t *hash, size_t hash_len, uint8_t *sig_out,
			    size_t cap, size_t *len_out)
{
	const slot_t *s = slot_find(id);
	uint8_t r[32], sv[32];
	uint8_t body[80];
	size_t at = 0;

	if (!s)
		return OSK_ERR_NOT_FOUND;
	if (hash_len != 32)
		return OSK_ERR_ARG;

	osk_hmac_sha256_2(s->seed, 32, hash, 32, (const uint8_t *)"r", 1, r);
	osk_hmac_sha256_2(s->seed, 32, hash, 32, (const uint8_t *)"s", 1, sv);
	r[0] &= 0x7f; /* nie negativ, damit DER kurz bleibt */
	sv[0] &= 0x7f;

	at += der_encode_int(r, body + at);
	at += der_encode_int(sv, body + at);

	if (cap < at + 2)
		return OSK_ERR_FULL;
	sig_out[0] = 0x30;
	sig_out[1] = (uint8_t)at;
	memcpy(sig_out + 2, body, at);
	*len_out = at + 2;
	return OSK_OK;
}

void mock_se050_ecdh_expect(const uint8_t peer_pub[64], uint8_t out[32])
{
	osk_sha256_t c;
	osk_sha256_init(&c);
	osk_sha256_update(&c, (const uint8_t *)"mock-ecdh/v1", 12);
	osk_sha256_update(&c, peer_pub, 64);
	osk_sha256_final(&c, out);
}

osk_result_t osk_se050_ecdh(osk_key_id_t id, const uint8_t *peer_pub, size_t peer_len,
			    uint8_t *secret_out, size_t cap, size_t *len_out)
{
	if (!slot_find(id))
		return OSK_ERR_NOT_FOUND;
	if (peer_len != 65 || peer_pub[0] != 0x04)
		return OSK_ERR_ARG;
	if (cap < 32)
		return OSK_ERR_FULL;
	mock_se050_ecdh_expect(peer_pub + 1, secret_out);
	*len_out = 32;
	return OSK_OK;
}

void mock_se050_hkdf_expect(const uint8_t *info, size_t info_len, uint8_t *out, size_t out_len)
{
	osk_hkdf_sha256((const uint8_t *)"mock-root", 9, g_root, sizeof(g_root), info, info_len,
			out, out_len);
}

osk_result_t osk_se050_hkdf(osk_key_id_t id, const uint8_t *info, size_t info_len, uint8_t *out,
			    size_t out_len)
{
	if (id != OSK_KEY_ID_ROOT)
		return OSK_ERR_NOT_FOUND;
	mock_se050_hkdf_expect(info, info_len, out, out_len);
	return OSK_OK;
}

osk_result_t osk_se050_hmac(osk_key_id_t id, const uint8_t *msg, size_t msg_len, uint8_t *mac_out,
			    size_t cap, size_t *len_out)
{
	const slot_t *s = slot_find(id);
	if (!s)
		return OSK_ERR_NOT_FOUND;
	if (cap < 32)
		return OSK_ERR_FULL;
	osk_hmac_sha256(s->seed, 32, msg, msg_len, mac_out);
	*len_out = 32;
	return OSK_OK;
}

/* Das fluechtige Schluesselobjekt, das der SE050-Strang noch beisteuern
 * muss (siehe osk_ctap_key.h). */
osk_result_t osk_se050_key_derive_transient(osk_key_id_t root, osk_curve_t curve,
					    const uint8_t *info, size_t info_len, osk_key_id_t slot)
{
	uint8_t seed[32];

	if (root != OSK_KEY_ID_ROOT)
		return OSK_ERR_NOT_FOUND;
	if (curve != OSK_CURVE_P256)
		return OSK_ERR_UNSUPPORTED;
	if (!slot_find(root))
		return OSK_ERR_NOT_FOUND;

	osk_hkdf_sha256((const uint8_t *)"mock-derive", 11, g_root, sizeof(g_root), info, info_len,
			seed, sizeof(seed));

	slot_t *s = slot_alloc(slot);
	if (!s)
		return OSK_ERR_FULL;
	memcpy(s->seed, seed, sizeof(s->seed));
	osk_wipe(seed, sizeof(seed));
	return OSK_OK;
}

osk_result_t osk_se050_pin_verify(const uint8_t *pin, size_t len)
{
	if (!g_pin_set)
		return OSK_ERR_NOT_FOUND;
	if (g_tries == 0)
		return OSK_ERR_LOCKED;
	if (len != sizeof(g_pin_hash) || !osk_memeq(pin, g_pin_hash, len)) {
		g_tries--;
		return OSK_ERR_DENIED;
	}
	g_tries = 8;
	return OSK_OK;
}

osk_result_t osk_se050_pin_set(const uint8_t *old_pin, size_t old_len, const uint8_t *new_pin,
			       size_t new_len)
{
	if (new_len != sizeof(g_pin_hash))
		return OSK_ERR_ARG;
	if (g_pin_set) {
		if (!old_pin)
			return OSK_ERR_DENIED;
		const osk_result_t rc = osk_se050_pin_verify(old_pin, old_len);
		if (rc != OSK_OK)
			return rc;
	}
	memcpy(g_pin_hash, new_pin, new_len);
	g_pin_set = true;
	g_tries = 8;
	return OSK_OK;
}

osk_result_t osk_se050_pin_tries_left(uint8_t *tries_out)
{
	if (!g_pin_set)
		return OSK_ERR_NOT_FOUND;
	*tries_out = g_tries;
	return OSK_OK;
}

osk_result_t osk_se050_pin_clear(void)
{
	memset(g_pin_hash, 0, sizeof(g_pin_hash));
	g_pin_set = false;
	g_tries = 8;
	return OSK_OK;
}

void mock_se050_set_pin(const uint8_t *pin_hash16)
{
	memcpy(g_pin_hash, pin_hash16, sizeof(g_pin_hash));
	g_pin_set = true;
	g_tries = 8;
}

void mock_se050_set_tries(uint8_t tries)
{
	g_tries = tries;
}

uint8_t mock_se050_tries(void)
{
	return g_tries;
}

/* ---- Anzeige ----------------------------------------------------------- */

static osk_result_t g_confirm_rc = OSK_OK;
static osk_result_t g_verify_rc = OSK_OK;
static bool g_verify_ok = true;
static unsigned g_confirm_count;
static char g_last_subject[128];
static char g_last_detail[128];
static int g_last_kind;

void mock_ui_set_confirm(osk_result_t rc)
{
	g_confirm_rc = rc;
}
void mock_ui_set_verify(osk_result_t rc, bool verified)
{
	g_verify_rc = rc;
	g_verify_ok = verified;
}
unsigned mock_ui_confirm_count(void)
{
	return g_confirm_count;
}
const char *mock_ui_last_subject(void)
{
	return g_last_subject;
}
const char *mock_ui_last_detail(void)
{
	return g_last_detail;
}
int mock_ui_last_kind(void)
{
	return g_last_kind;
}

osk_result_t osk_ui_confirm(const osk_ask_req_t *req)
{
	g_confirm_count++;
	g_last_kind = (int)req->kind;
	g_last_subject[0] = 0;
	g_last_detail[0] = 0;
	if (req->subject)
		snprintf(g_last_subject, sizeof(g_last_subject), "%s", req->subject);
	if (req->detail)
		snprintf(g_last_detail, sizeof(g_last_detail), "%s", req->detail);
	return g_confirm_rc;
}

osk_result_t osk_ui_verify_user(bool *verified_out)
{
	*verified_out = g_verify_ok;
	return g_verify_rc;
}

void osk_ui_status(const char *text_key)
{
	(void)text_key;
}

/* ---- Tresor ------------------------------------------------------------ */

typedef struct {
	bool used;
	osk_item_id_t id;
	osk_item_type_t type;
	uint8_t profile;
	char name[STORE_TEXT_MAX];
	char site[STORE_TEXT_MAX];
	uint64_t changed;
	bool mirrored;
	uint8_t tag[32];
	uint8_t tag_len;
	uint8_t secret[STORE_SECRET_MAX];
	size_t secret_len;
} item_t;

static item_t g_items[STORE_MAX];
static osk_item_id_t g_next_id = 1;
static bool g_store_open = true;
static bool g_store_full;
static unsigned g_reveal_count;

size_t mock_store_count(void)
{
	size_t n = 0;
	for (unsigned i = 0; i < STORE_MAX; i++) {
		if (g_items[i].used)
			n++;
	}
	return n;
}

void mock_store_set_full(bool full)
{
	g_store_full = full;
}

unsigned mock_store_reveal_count(void)
{
	return g_reveal_count;
}

osk_result_t osk_store_open(void)
{
	g_store_open = true;
	return OSK_OK;
}

void osk_store_lock(void)
{
	g_store_open = false;
}

bool osk_store_is_open(void)
{
	return g_store_open;
}

osk_result_t osk_store_count(uint8_t profile, size_t *count_out)
{
	size_t n = 0;
	for (unsigned i = 0; i < STORE_MAX; i++) {
		if (g_items[i].used && g_items[i].profile == profile)
			n++;
	}
	*count_out = n;
	return OSK_OK;
}

static item_t *nth(uint8_t profile, size_t index)
{
	size_t n = 0;
	for (unsigned i = 0; i < STORE_MAX; i++) {
		if (!g_items[i].used || g_items[i].profile != profile)
			continue;
		if (n == index)
			return &g_items[i];
		n++;
	}
	return NULL;
}

osk_result_t osk_store_list(uint8_t profile, size_t index, osk_item_meta_t *out)
{
	const item_t *it = nth(profile, index);
	if (!it)
		return OSK_ERR_NOT_FOUND;
	out->id = it->id;
	out->type = it->type;
	out->profile = it->profile;
	out->name = it->name;
	out->site = it->site;
	out->changed_utc = it->changed;
	out->mirrored = it->mirrored;
	out->paired_host_only = false;
	memcpy(out->tag, it->tag, sizeof(out->tag));
	out->tag_len = it->tag_len;
	return OSK_OK;
}

osk_result_t osk_store_find_tag(uint8_t profile, osk_item_type_t type, const uint8_t *tag,
				size_t tag_len, osk_item_id_t *out, size_t cap, size_t *len_out)
{
	size_t n = 0;

	if (!tag || tag_len == 0 || tag_len > 32)
		return OSK_ERR_ARG;

	for (unsigned i = 0; i < STORE_MAX; i++) {
		if (!g_items[i].used || g_items[i].profile != profile)
			continue;
		if (g_items[i].type != type)
			continue;
		if (g_items[i].tag_len != tag_len)
			continue;
		if (memcmp(g_items[i].tag, tag, tag_len) != 0)
			continue;
		if (n < cap)
			out[n] = g_items[i].id;
		n++;
	}
	*len_out = n;
	return (n > 0) ? OSK_OK : OSK_ERR_NOT_FOUND;
}

osk_result_t osk_store_find(uint8_t profile, const char *site, osk_item_id_t *out, size_t cap,
			    size_t *len_out)
{
	size_t n = 0;
	for (unsigned i = 0; i < STORE_MAX; i++) {
		if (!g_items[i].used || g_items[i].profile != profile)
			continue;
		if (strcmp(g_items[i].site, site) != 0)
			continue;
		if (n < cap)
			out[n] = g_items[i].id;
		n++;
	}
	*len_out = n;
	return (n > 0) ? OSK_OK : OSK_ERR_NOT_FOUND;
}

osk_result_t osk_store_add(const osk_item_meta_t *meta, const uint8_t *secret, size_t secret_len,
			   osk_item_id_t *id_out)
{
	if (g_store_full)
		return OSK_ERR_FULL;
	if (secret_len > STORE_SECRET_MAX)
		return OSK_ERR_ARG;
	for (unsigned i = 0; i < STORE_MAX; i++) {
		if (g_items[i].used)
			continue;
		item_t *it = &g_items[i];
		memset(it, 0, sizeof(*it));
		it->used = true;
		it->id = g_next_id++;
		it->type = meta->type;
		it->profile = meta->profile;
		it->changed = meta->changed_utc;
		it->mirrored = meta->mirrored;
		memcpy(it->tag, meta->tag, sizeof(it->tag));
		it->tag_len = meta->tag_len;
		if (meta->name)
			snprintf(it->name, sizeof(it->name), "%s", meta->name);
		if (meta->site)
			snprintf(it->site, sizeof(it->site), "%s", meta->site);
		memcpy(it->secret, secret, secret_len);
		it->secret_len = secret_len;
		*id_out = it->id;
		return OSK_OK;
	}
	return OSK_ERR_FULL;
}

static item_t *by_id(osk_item_id_t id)
{
	for (unsigned i = 0; i < STORE_MAX; i++) {
		if (g_items[i].used && g_items[i].id == id)
			return &g_items[i];
	}
	return NULL;
}

osk_result_t osk_store_update(osk_item_id_t id, const osk_item_meta_t *meta, const uint8_t *secret,
			      size_t secret_len)
{
	item_t *it = by_id(id);
	if (!it)
		return OSK_ERR_NOT_FOUND;
	if (secret_len > STORE_SECRET_MAX)
		return OSK_ERR_ARG;
	if (meta) {
		it->type = meta->type;
		it->changed = meta->changed_utc;
		it->mirrored = meta->mirrored;
		memcpy(it->tag, meta->tag, sizeof(it->tag));
		it->tag_len = meta->tag_len;
		if (meta->name)
			snprintf(it->name, sizeof(it->name), "%s", meta->name);
		if (meta->site)
			snprintf(it->site, sizeof(it->site), "%s", meta->site);
	}
	memcpy(it->secret, secret, secret_len);
	it->secret_len = secret_len;
	return OSK_OK;
}

osk_result_t osk_store_delete(osk_item_id_t id)
{
	item_t *it = by_id(id);
	if (!it)
		return OSK_ERR_NOT_FOUND;
	memset(it, 0, sizeof(*it));
	return OSK_OK;
}

osk_result_t osk_store_reveal(osk_item_id_t id, const char *asked_by, uint8_t *out, size_t cap,
			      size_t *len_out)
{
	(void)asked_by;
	g_reveal_count++;
	const item_t *it = by_id(id);
	if (!it)
		return OSK_ERR_NOT_FOUND;
	if (cap < it->secret_len)
		return OSK_ERR_FULL;
	memcpy(out, it->secret, it->secret_len);
	*len_out = it->secret_len;
	return OSK_OK;
}

/* ---- Signaturzaehler ("Flash") ----------------------------------------
 *
 * Der echte Zaehler gehoert Strang D und liegt im externen Flash. Hier
 * steht er in einer Datei, damit ein NEUER PROZESS ihn wiederfindet. Alles
 * andere waere eine Attrappe, die genau die Eigenschaft nicht hat, auf die
 * es ankommt. */

static char g_sigcount_path[512];
static bool g_sigcount_broken;

/* Wurde ein Pfad genannt, ist die Datei "der Flash" und niemand ausser
 * mock_sigcount_forget() ruehrt sie an. Wurde KEINER genannt, ist sie ein
 * Kratzzettel dieses Prozesses: mock_reset_all() und das Prozessende
 * raeumen sie weg. Sonst stolpern zwei Pruefprogramme uebereinander, und
 * ein Prueflauf liesse eine Datei im Arbeitsbaum liegen. */
static bool g_sigcount_named;

void mock_sigcount_file(const char *path)
{
	snprintf(g_sigcount_path, sizeof(g_sigcount_path), "%s", path ? path : "");
	g_sigcount_named = (g_sigcount_path[0] != 0);
}

void mock_sigcount_set_broken(bool broken)
{
	g_sigcount_broken = broken;
}

static const char *sigcount_path(void)
{
	if (g_sigcount_path[0])
		return g_sigcount_path;
	const char *env = getenv("OSK_SIGCOUNT_FILE");
	if (env && env[0])
		return env;
	return "osk-sigcount.bin";
}

void mock_sigcount_forget(void)
{
	remove(sigcount_path());
}

static void sigcount_cleanup(void)
{
	if (!g_sigcount_named)
		remove(sigcount_path());
}

static bool sigcount_load(uint32_t *out)
{
	FILE *f = fopen(sigcount_path(), "rb");
	uint8_t b[4];

	*out = 0;
	if (!f)
		return true; /* nichts da heisst null, nicht Fehler */
	const size_t n = fread(b, 1, sizeof(b), f);
	fclose(f);
	if (n != sizeof(b))
		return false;
	*out = ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | b[3];
	return true;
}

static bool sigcount_store(uint32_t v)
{
	FILE *f = fopen(sigcount_path(), "wb");
	const uint8_t b[4] = {(uint8_t)(v >> 24), (uint8_t)(v >> 16), (uint8_t)(v >> 8),
			      (uint8_t)v};

	if (!f)
		return false;
	const size_t n = fwrite(b, 1, sizeof(b), f);
	fclose(f);
	return n == sizeof(b);
}

osk_result_t osk_store_sigcount_next(osk_item_id_t cred, uint32_t *value_out)
{
	uint32_t v = 0;

	(void)cred;
	if (!value_out)
		return OSK_ERR_ARG;
	if (g_sigcount_broken)
		return OSK_ERR_IO;
	if (!sigcount_load(&v))
		return OSK_ERR_IO;
	if (v == 0xffffffffu)
		return OSK_ERR_FULL; /* weiterzaehlen hiesse rueckwaerts */
	v++;
	if (!sigcount_store(v))
		return OSK_ERR_IO;
	*value_out = v;
	return OSK_OK;
}

osk_result_t osk_store_sigcount_get(osk_item_id_t cred, uint32_t *value_out)
{
	uint32_t v = 0;

	(void)cred;
	if (!value_out)
		return OSK_ERR_ARG;
	if (g_sigcount_broken)
		return OSK_ERR_IO;
	if (!sigcount_load(&v))
		return OSK_ERR_IO;
	*value_out = v;
	return OSK_OK;
}

osk_result_t osk_store_format_version(uint16_t *version_out)
{
	*version_out = 1;
	return OSK_OK;
}

osk_result_t osk_store_migrate(void)
{
	return OSK_OK;
}

/* ---- Transport --------------------------------------------------------- */

osk_result_t osk_transport_register(osk_link_t link, osk_handler_fn handler)
{
	(void)link;
	(void)handler;
	return OSK_OK;
}

osk_result_t osk_transport_enable(osk_link_t link, bool on)
{
	(void)link;
	(void)on;
	return OSK_OK;
}

/* ---- Rueckgabetexte ---------------------------------------------------- */

const char *osk_result_text(osk_result_t r)
{
	switch (r) {
	case OSK_OK:
		return "ok";
	case OSK_ERR_ARG:
		return "arg";
	case OSK_ERR_IO:
		return "io";
	case OSK_ERR_TIMEOUT:
		return "timeout";
	case OSK_ERR_DENIED:
		return "denied";
	case OSK_ERR_LOCKED:
		return "locked";
	case OSK_ERR_NOT_FOUND:
		return "not-found";
	case OSK_ERR_EXISTS:
		return "exists";
	case OSK_ERR_FULL:
		return "full";
	case OSK_ERR_FORMAT:
		return "format";
	case OSK_ERR_UNSUPPORTED:
		return "unsupported";
	default:
		return "internal";
	}
}

/* ---- Alles auf Anfang -------------------------------------------------- */

void mock_reset_all(void)
{
	const osk_key_policy_t policy = {.exportable = false, .needs_touch = false};

	memset(g_slots, 0, sizeof(g_slots));
	memset(g_items, 0, sizeof(g_items));
	memset(g_root, 0, sizeof(g_root));
	memset(g_pin_hash, 0, sizeof(g_pin_hash));
	g_next_id = 1;
	g_pin_set = false;
	g_tries = 8;
	g_store_open = true;
	g_store_full = false;
	g_reveal_count = 0;
	g_sigcount_broken = false;
	/* Der Signaturzaehler bleibt stehen, WENN ein Pfad genannt wurde -
	 * siehe mocks.h. Der Kratzzettel ohne Namen geht weg. */
	if (!g_sigcount_named) {
		static bool registered;
		remove(sigcount_path());
		if (!registered) {
			atexit(sigcount_cleanup);
			registered = true;
		}
	}
	g_confirm_rc = OSK_OK;
	g_verify_rc = OSK_OK;
	g_verify_ok = true;
	g_confirm_count = 0;
	g_last_subject[0] = 0;
	g_last_detail[0] = 0;
	g_last_kind = -1;
	g_now = 1000;
	mock_se050_seed_random(0x5eed1234u);

	osk_se050_open();
	osk_se050_key_create(OSK_KEY_ID_ROOT, OSK_CURVE_P256, &policy);
}
