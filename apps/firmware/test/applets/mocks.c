/* Attrappen - siehe mocks.h. KEINE Sicherheit, nur Form und Ablauf. */
#include <string.h>
#include <stdio.h>
#include "mocks.h"
#include "osk_se050.h"
#include "osk_store.h"
#include "osk_ui.h"
#include "osk_transport.h"
#include "osk_otp_crypto.h"

/* ---- Anzeige ----------------------------------------------------------- */

static osk_result_t g_confirm_rc = OSK_OK;
static unsigned g_confirm_count;
static char g_last_subject[64];
static char g_last_detail[64];
static int g_last_kind = -1;

void mock_ui_set_confirm(osk_result_t rc)
{
	g_confirm_rc = rc;
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
	if (req->subject != NULL)
		snprintf(g_last_subject, sizeof(g_last_subject), "%s", req->subject);
	if (req->detail != NULL)
		snprintf(g_last_detail, sizeof(g_last_detail), "%s", req->detail);
	return g_confirm_rc;
}

osk_result_t osk_ui_verify_user(bool *verified_out)
{
	*verified_out = true;
	return OSK_OK;
}

void osk_ui_status(const char *text_key)
{
	(void)text_key;
}

/* ---- SE050 ------------------------------------------------------------- */

#define SLOTS 8

typedef struct {
	bool used;
	uint32_t id;
	osk_curve_t curve;
	uint8_t seed[20];
} slot_t;

static slot_t g_slots[SLOTS];
static char g_pin[65] = "123456";
static bool g_pin_set = true;
static uint8_t g_tries = 8u;
static uint32_t g_rand = 1u;

void mock_se050_set_pin(const char *pin)
{
	memset(g_pin, 0, sizeof(g_pin));
	if (pin == NULL) {
		g_pin_set = false;
		return;
	}
	snprintf(g_pin, sizeof(g_pin), "%s", pin);
	g_pin_set = true;
	g_tries = 8u;
}

uint8_t mock_se050_tries(void)
{
	return g_tries;
}

void mock_se050_seed_random(uint32_t seed)
{
	g_rand = seed;
}

static slot_t *slot_find(uint32_t id)
{
	for (int i = 0; i < SLOTS; i++) {
		if (g_slots[i].used && g_slots[i].id == id)
			return &g_slots[i];
	}
	return NULL;
}

bool mock_se050_has_key(uint32_t id)
{
	return slot_find(id) != NULL;
}

osk_result_t osk_se050_open(void)
{
	return OSK_OK;
}

void osk_se050_close(void)
{
}

osk_result_t osk_se050_curves(uint32_t *mask_out)
{
	if (mask_out == NULL)
		return OSK_ERR_ARG;
	*mask_out = (1u << OSK_CURVE_P256) | (1u << OSK_CURVE_P384);
	return OSK_OK;
}

osk_result_t osk_se050_random(uint8_t *out, size_t len)
{
	if (out == NULL)
		return OSK_ERR_ARG;
	for (size_t i = 0; i < len; i++) {
		/* Einfacher Zaehlergenerator - vorhersagbar ist hier erwuenscht. */
		g_rand = g_rand * 1103515245u + 12345u;
		out[i] = (uint8_t)(g_rand >> 16);
	}
	return OSK_OK;
}

osk_result_t osk_se050_key_create(osk_key_id_t id, osk_curve_t curve,
				  const osk_key_policy_t *policy)
{
	slot_t *s = slot_find(id);

	if (policy != NULL && policy->exportable)
		return OSK_ERR_UNSUPPORTED; /* wie der echte Chip: nichts raus */
	if (s != NULL)
		return OSK_ERR_EXISTS;
	for (int i = 0; i < SLOTS; i++) {
		if (!g_slots[i].used) {
			g_slots[i].used = true;
			g_slots[i].id = id;
			g_slots[i].curve = curve;
			/* Das Keimmaterial haengt nur an der Kennung: dieselbe
			 * Kennung ergibt denselben Schluessel. */
			{
				uint8_t in[4];

				in[0] = (uint8_t)id;
				in[1] = (uint8_t)(id >> 8);
				in[2] = (uint8_t)(id >> 16);
				in[3] = (uint8_t)(id >> 24);
				osk_sha1(in, sizeof(in), g_slots[i].seed);
			}
			return OSK_OK;
		}
	}
	return OSK_ERR_FULL;
}

osk_result_t osk_se050_key_delete(osk_key_id_t id)
{
	slot_t *s = slot_find(id);

	if (s == NULL)
		return OSK_ERR_NOT_FOUND;
	memset(s, 0, sizeof(*s));
	return OSK_OK;
}

static size_t curve_point_len(osk_curve_t c)
{
	return (c == OSK_CURVE_P384) ? 97u : 65u;
}

osk_result_t osk_se050_key_public(osk_key_id_t id, uint8_t *out, size_t cap, size_t *len_out)
{
	const slot_t *s = slot_find(id);
	size_t point_len;
	size_t total;

	if (s == NULL)
		return OSK_ERR_NOT_FOUND;
	point_len = curve_point_len(s->curve);
	/* Der echte Chip haengt den Punkt hinten an eine Verpackung. Zwei
	 * Bytes davor stellen das nach, damit die Pruefung genau den Weg
	 * geht, den der Chip verlangt. */
	total = point_len + 2u;
	if (out == NULL || cap < total)
		return OSK_ERR_FULL;
	out[0] = 0x30u;
	out[1] = (uint8_t)point_len;
	out[2] = 0x04u;
	for (size_t i = 1; i < point_len; i++)
		out[2u + i] = (uint8_t)(s->seed[i % sizeof(s->seed)] ^ (uint8_t)i);
	*len_out = total;
	return OSK_OK;
}

/* Ein DER-INTEGER mit fuehrender Null, wenn das oberste Bit gesetzt ist. */
static size_t der_int(const uint8_t *v, size_t len, uint8_t *out)
{
	size_t at = 0;
	size_t start = 0;

	while (start + 1u < len && v[start] == 0x00u)
		start++;
	out[at++] = 0x02u;
	if ((v[start] & 0x80u) != 0u) {
		out[at++] = (uint8_t)(len - start + 1u);
		out[at++] = 0x00u;
	} else {
		out[at++] = (uint8_t)(len - start);
	}
	memcpy(out + at, v + start, len - start);
	return at + (len - start);
}

osk_result_t osk_se050_sign(osk_key_id_t id, const uint8_t *hash, size_t hash_len, uint8_t *sig_out,
			    size_t cap, size_t *len_out)
{
	const slot_t *s = slot_find(id);
	uint8_t r[48];
	uint8_t sv[48];
	uint8_t body[128];
	size_t half;
	size_t at = 0;

	if (s == NULL)
		return OSK_ERR_NOT_FOUND;
	if (hash == NULL || hash_len == 0u)
		return OSK_ERR_ARG;
	half = (s->curve == OSK_CURVE_P384) ? 48u : 32u;

	/* r und s aus HMAC(seed, hash) - deterministisch, aber keine Kurve. */
	osk_hmac_sha1(s->seed, sizeof(s->seed), hash, hash_len, r);
	memcpy(r + 20, r, 20);
	memcpy(r + 40, r, 8);
	osk_hmac_sha1(r, 20u, hash, hash_len, sv);
	memcpy(sv + 20, sv, 20);
	memcpy(sv + 40, sv, 8);
	r[0] = (uint8_t)(r[0] & 0x7fu);
	sv[0] = (uint8_t)(sv[0] & 0x7fu);

	at += der_int(r, half, body + at);
	at += der_int(sv, half, body + at);
	if (sig_out == NULL || cap < at + 2u)
		return OSK_ERR_FULL;
	sig_out[0] = 0x30u;
	sig_out[1] = (uint8_t)at;
	memcpy(sig_out + 2, body, at);
	*len_out = at + 2u;
	return OSK_OK;
}

osk_result_t osk_se050_ecdh(osk_key_id_t id, const uint8_t *peer_pub, size_t peer_len,
			    uint8_t *secret_out, size_t cap, size_t *len_out)
{
	const slot_t *s = slot_find(id);
	uint8_t mac[OSK_SHA1_LEN];

	if (s == NULL)
		return OSK_ERR_NOT_FOUND;
	if (peer_pub == NULL || peer_len == 0u)
		return OSK_ERR_ARG;
	if (secret_out == NULL || cap < 32u)
		return OSK_ERR_FULL;
	osk_hmac_sha1(s->seed, sizeof(s->seed), peer_pub, peer_len, mac);
	memcpy(secret_out, mac, 20u);
	memcpy(secret_out + 20, mac, 12u);
	*len_out = 32u;
	return OSK_OK;
}

osk_result_t osk_se050_hkdf(osk_key_id_t id, const uint8_t *info, size_t info_len, uint8_t *out,
			    size_t out_len)
{
	uint8_t key[4];
	uint8_t mac[OSK_SHA1_LEN];

	if (out == NULL || out_len > sizeof(mac))
		return OSK_ERR_ARG;
	key[0] = (uint8_t)id;
	key[1] = (uint8_t)(id >> 8);
	key[2] = (uint8_t)(id >> 16);
	key[3] = (uint8_t)(id >> 24);
	osk_hmac_sha1(key, sizeof(key), info, info_len, mac);
	memcpy(out, mac, out_len);
	return OSK_OK;
}

osk_result_t osk_se050_key_derive_transient(osk_key_id_t root, osk_curve_t curve,
					    const uint8_t *info, size_t info_len, osk_key_id_t slot)
{
	(void)root;
	(void)info;
	(void)info_len;
	(void)osk_se050_key_delete(slot);
	return osk_se050_key_create(slot, curve, NULL);
}

osk_result_t osk_se050_hmac(osk_key_id_t id, const uint8_t *msg, size_t msg_len, uint8_t *mac_out,
			    size_t cap, size_t *len_out)
{
	const slot_t *s = slot_find(id);
	uint8_t mac[OSK_SHA1_LEN];

	if (s == NULL)
		return OSK_ERR_NOT_FOUND;
	if (mac_out == NULL || cap < sizeof(mac))
		return OSK_ERR_FULL;
	osk_hmac_sha1(s->seed, sizeof(s->seed), msg, msg_len, mac);
	memcpy(mac_out, mac, sizeof(mac));
	*len_out = sizeof(mac);
	return OSK_OK;
}

osk_result_t osk_se050_pin_verify(const uint8_t *pin, size_t len)
{
	if (!g_pin_set)
		return OSK_ERR_NOT_FOUND;
	if (g_tries == 0u)
		return OSK_ERR_LOCKED;
	if (pin == NULL)
		return OSK_ERR_ARG;
	if (len == strlen(g_pin) && memcmp(pin, g_pin, len) == 0) {
		g_tries = 8u;
		return OSK_OK;
	}
	g_tries--;
	return (g_tries == 0u) ? OSK_ERR_LOCKED : OSK_ERR_DENIED;
}

osk_result_t osk_se050_pin_set(const uint8_t *old_pin, size_t old_len, const uint8_t *new_pin,
			       size_t new_len)
{
	if (new_pin == NULL || new_len == 0u || new_len > sizeof(g_pin) - 1u)
		return OSK_ERR_ARG;
	if (g_pin_set) {
		const osk_result_t rc = osk_se050_pin_verify(old_pin, old_len);

		if (rc != OSK_OK)
			return rc;
	}
	memset(g_pin, 0, sizeof(g_pin));
	memcpy(g_pin, new_pin, new_len);
	g_pin_set = true;
	g_tries = 8u;
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
	memset(g_pin, 0, sizeof(g_pin));
	g_pin_set = false;
	g_tries = 8u;
	return OSK_OK;
}

osk_result_t osk_se050_free_memory(uint16_t *bytes_out)
{
	if (bytes_out == NULL)
		return OSK_ERR_ARG;
	*bytes_out = 0x7fffu;
	return OSK_OK;
}

osk_result_t osk_se050_rotate_scp_keys(uint8_t key_version, const osk_scp_keyset_t *current,
				       const osk_scp_keyset_t *replacement)
{
	(void)key_version;
	(void)current;
	(void)replacement;
	return OSK_OK;
}

osk_result_t osk_se050_require_scp(void)
{
	return OSK_OK;
}

/* ---- Tresor ------------------------------------------------------------ */

#define ITEMS 16
#define SECRET_MAX 3072

typedef struct {
	bool used;
	osk_item_id_t id;
	osk_item_type_t type;
	uint8_t profile;
	char name[64];
	char site[64];
	uint8_t tag[32];
	uint8_t tag_len;
	uint8_t secret[SECRET_MAX];
	size_t secret_len;
} item_t;

static item_t g_items[ITEMS];
static osk_item_id_t g_next_id = 1u;
static unsigned g_reveal_count;
static bool g_open = true;

size_t mock_store_count(void)
{
	size_t n = 0;

	for (int i = 0; i < ITEMS; i++)
		n += g_items[i].used ? 1u : 0u;
	return n;
}

unsigned mock_store_reveal_count(void)
{
	return g_reveal_count;
}

static item_t *item_find(osk_item_id_t id)
{
	for (int i = 0; i < ITEMS; i++) {
		if (g_items[i].used && g_items[i].id == id)
			return &g_items[i];
	}
	return NULL;
}

osk_result_t osk_store_open(void)
{
	g_open = true;
	return OSK_OK;
}

void osk_store_lock(void)
{
	g_open = false;
}

bool osk_store_is_open(void)
{
	return g_open;
}

osk_result_t osk_store_count(uint8_t profile, size_t *count_out)
{
	size_t n = 0;

	if (count_out == NULL)
		return OSK_ERR_ARG;
	for (int i = 0; i < ITEMS; i++) {
		if (g_items[i].used && g_items[i].profile == profile)
			n++;
	}
	*count_out = n;
	return OSK_OK;
}

osk_result_t osk_store_list(uint8_t profile, size_t index, osk_item_meta_t *out)
{
	size_t n = 0;

	if (out == NULL)
		return OSK_ERR_ARG;
	for (int i = 0; i < ITEMS; i++) {
		const item_t *it = &g_items[i];

		if (!it->used || it->profile != profile)
			continue;
		if (n++ != index)
			continue;
		memset(out, 0, sizeof(*out));
		out->id = it->id;
		out->type = it->type;
		out->profile = it->profile;
		out->name = it->name;
		out->site = it->site[0] != 0 ? it->site : NULL;
		out->changed_utc = 0u;
		out->tag_len = it->tag_len;
		memcpy(out->tag, it->tag, sizeof(out->tag));
		return OSK_OK;
	}
	return OSK_ERR_NOT_FOUND;
}

osk_result_t osk_store_find(uint8_t profile, const char *site, osk_item_id_t *out, size_t cap,
			    size_t *len_out)
{
	size_t n = 0;

	if (site == NULL || out == NULL || len_out == NULL)
		return OSK_ERR_ARG;
	for (int i = 0; i < ITEMS && n < cap; i++) {
		const item_t *it = &g_items[i];

		if (it->used && it->profile == profile && strcmp(it->site, site) == 0)
			out[n++] = it->id;
	}
	*len_out = n;
	return OSK_OK;
}

osk_result_t osk_store_find_tag(uint8_t profile, osk_item_type_t type, const uint8_t *tag,
				size_t tag_len, osk_item_id_t *out, size_t cap, size_t *len_out)
{
	size_t n = 0;

	if (tag == NULL || out == NULL || len_out == NULL)
		return OSK_ERR_ARG;
	if (tag_len == 0u || tag_len > 32u)
		return OSK_ERR_ARG;
	for (int i = 0; i < ITEMS && n < cap; i++) {
		const item_t *it = &g_items[i];

		if (!it->used || it->profile != profile || it->type != type)
			continue;
		if (it->tag_len != tag_len || memcmp(it->tag, tag, tag_len) != 0)
			continue;
		out[n++] = it->id;
	}
	*len_out = n;
	return OSK_OK;
}

static void item_fill(item_t *it, const osk_item_meta_t *meta, const uint8_t *secret, size_t len)
{
	it->type = meta->type;
	it->profile = meta->profile;
	memset(it->name, 0, sizeof(it->name));
	memset(it->site, 0, sizeof(it->site));
	if (meta->name != NULL)
		snprintf(it->name, sizeof(it->name), "%s", meta->name);
	if (meta->site != NULL)
		snprintf(it->site, sizeof(it->site), "%s", meta->site);
	it->tag_len = meta->tag_len;
	memcpy(it->tag, meta->tag, sizeof(it->tag));
	memcpy(it->secret, secret, len);
	it->secret_len = len;
}

osk_result_t osk_store_add(const osk_item_meta_t *meta, const uint8_t *secret, size_t secret_len,
			   osk_item_id_t *id_out)
{
	if (meta == NULL || secret == NULL || id_out == NULL)
		return OSK_ERR_ARG;
	if (secret_len == 0u || secret_len > SECRET_MAX)
		return OSK_ERR_ARG;
	for (int i = 0; i < ITEMS; i++) {
		if (!g_items[i].used) {
			memset(&g_items[i], 0, sizeof(g_items[i]));
			g_items[i].used = true;
			g_items[i].id = g_next_id++;
			item_fill(&g_items[i], meta, secret, secret_len);
			*id_out = g_items[i].id;
			return OSK_OK;
		}
	}
	return OSK_ERR_FULL;
}

osk_result_t osk_store_update(osk_item_id_t id, const osk_item_meta_t *meta, const uint8_t *secret,
			      size_t secret_len)
{
	item_t *it = item_find(id);

	if (it == NULL)
		return OSK_ERR_NOT_FOUND;
	if (meta == NULL || secret == NULL)
		return OSK_ERR_ARG;
	if (secret_len == 0u || secret_len > SECRET_MAX)
		return OSK_ERR_ARG;
	item_fill(it, meta, secret, secret_len);
	return OSK_OK;
}

osk_result_t osk_store_delete(osk_item_id_t id)
{
	item_t *it = item_find(id);

	if (it == NULL)
		return OSK_ERR_NOT_FOUND;
	memset(it, 0, sizeof(*it));
	return OSK_OK;
}

/* Wie der echte Tresor: fragt selbst am Display nach, bevor etwas
 * herausgeht. Daran haengt die Pruefung, dass jede Herausgabe sichtbar ist. */
osk_result_t osk_store_reveal(osk_item_id_t id, const char *asked_by, uint8_t *out, size_t cap,
			      size_t *len_out)
{
	const item_t *it = item_find(id);
	osk_ask_req_t ask;

	if (it == NULL)
		return OSK_ERR_NOT_FOUND;
	if (out == NULL || len_out == NULL)
		return OSK_ERR_ARG;
	if (cap < it->secret_len)
		return OSK_ERR_FULL;

	memset(&ask, 0, sizeof(ask));
	ask.kind = OSK_ASK_SECRET;
	ask.subject = (asked_by != NULL) ? asked_by : it->name;
	ask.asked_by = asked_by;
	ask.timeout_ms = 30000u;
	if (osk_ui_confirm(&ask) != OSK_OK)
		return OSK_ERR_DENIED;

	g_reveal_count++;
	memcpy(out, it->secret, it->secret_len);
	*len_out = it->secret_len;
	return OSK_OK;
}

osk_result_t osk_store_format_version(uint16_t *version_out)
{
	if (version_out == NULL)
		return OSK_ERR_ARG;
	*version_out = 1u;
	return OSK_OK;
}

osk_result_t osk_store_migrate(void)
{
	return OSK_OK;
}

osk_result_t mock_store_put(osk_item_type_t type, const char *tag, const uint8_t *secret,
			    size_t len)
{
	osk_item_meta_t meta;
	osk_item_id_t id = 0;

	memset(&meta, 0, sizeof(meta));
	meta.type = type;
	meta.profile = 0u;
	meta.name = tag;
	meta.tag_len = (uint8_t)strlen(tag);
	memcpy(meta.tag, tag, meta.tag_len);
	return osk_store_add(&meta, secret, len, &id);
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

/* ---- Uhr --------------------------------------------------------------- */

static uint64_t g_now_ms = 1000u;

void mock_clock_set(uint64_t ms)
{
	g_now_ms = ms;
}

uint64_t osk_ctap_now_ms(void)
{
	return g_now_ms;
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
		return "not found";
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

/* ---- alles auf Anfang -------------------------------------------------- */

void mock_reset_all(void)
{
	memset(g_slots, 0, sizeof(g_slots));
	memset(g_items, 0, sizeof(g_items));
	g_next_id = 1u;
	g_reveal_count = 0u;
	g_open = true;
	g_confirm_rc = OSK_OK;
	g_confirm_count = 0u;
	g_last_subject[0] = 0;
	g_last_detail[0] = 0;
	g_last_kind = -1;
	g_rand = 1u;
	g_now_ms = 1000u;
	mock_se050_set_pin("123456");
}
