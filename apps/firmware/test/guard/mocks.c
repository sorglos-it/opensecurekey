/* Die Attrappen. Siehe mocks.h. */
#include <string.h>
#include "mocks.h"
#include "osk_se050.h"
#include "osk_store.h"
#include "osk_ui.h"
#include "osk_log.h"
#include "osk_transport.h"
#include "osk_crypto.h"
#include "osk_guard_keys.h"

#define SLOT_MAX 16
#define LOG_MAX 64
#define SUBJECT_MAX 80

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
	bool used;
	uint32_t id;
	osk_curve_t curve;
	uint8_t seed[32];
} slot_t;

static slot_t g_slots[SLOT_MAX];
static bool g_no_ecdh;
static bool g_offline;

static slot_t *slot_find(uint32_t id)
{
	for (unsigned i = 0; i < SLOT_MAX; i++) {
		if (g_slots[i].used && g_slots[i].id == id) {
			return &g_slots[i];
		}
	}
	return NULL;
}

/* Jeder Schluessel bekommt ein festes, aus seiner Nummer abgeleitetes
 * Geheimnis - damit sind alle Laeufe gleich. */
static void seed_of(uint32_t id, uint8_t out[32])
{
	uint8_t head[4 + 11];
	memcpy(head, "mock-se050-", 11);
	head[11] = (uint8_t)(id >> 24);
	head[12] = (uint8_t)(id >> 16);
	head[13] = (uint8_t)(id >> 8);
	head[14] = (uint8_t)id;
	osk_sha256(head, sizeof(head), out);
}

static slot_t *slot_make(uint32_t id, osk_curve_t curve)
{
	slot_t *found = slot_find(id);
	if (found != NULL) {
		return found;
	}
	for (unsigned i = 0; i < SLOT_MAX; i++) {
		if (g_slots[i].used) {
			continue;
		}
		g_slots[i].used = true;
		g_slots[i].id = id;
		g_slots[i].curve = curve;
		seed_of(id, g_slots[i].seed);
		return &g_slots[i];
	}
	return NULL;
}

void mock_se050_add_key(uint32_t id, osk_curve_t curve)
{
	(void)slot_make(id, curve);
}

void mock_se050_setup_guard(void)
{
	/* Der Geraeteschluessel unterschreibt attest, das Grundgeheimnis
	 * dient nur der Ableitung. Beide entstehen bei der Einrichtung. */
	(void)slot_make(OSK_GUARD_KEY_IDENTITY, OSK_CURVE_ED25519);
	(void)slot_make(OSK_GUARD_KEY_ROOT, OSK_CURVE_ED25519);
}

void mock_se050_set_no_ecdh(bool on)
{
	g_no_ecdh = on;
}

void mock_se050_set_offline(bool on)
{
	g_offline = on;
}

bool mock_se050_has_key(uint32_t id)
{
	return slot_find(id) != NULL;
}

/* Die "Unterschrift": 64 Byte aus zwei verketteten HMACs ueber die
 * Nachricht. Deterministisch, und verschiedene Nachrichten geben
 * verschiedene Werte - genau die Eigenschaft, auf die sich die Pruefung der
 * Zwecktrennung stuetzt. */
static void fake_sign(const uint8_t seed[32], const uint8_t *msg, size_t len, uint8_t out[64])
{
	osk_hmac_sha256(seed, 32u, msg, len, out);
	osk_hmac_sha256(seed, 32u, out, 32u, out + 32);
}

void mock_se050_expected_sig(uint32_t id, const uint8_t *msg, size_t len, uint8_t out[64])
{
	uint8_t seed[32];
	seed_of(id, seed);
	fake_sign(seed, msg, len, out);
}

void mock_se050_root_secret(uint8_t out[32])
{
	seed_of(OSK_GUARD_KEY_ROOT, out);
}

osk_result_t osk_se050_open(void)
{
	return g_offline ? OSK_ERR_IO : OSK_OK;
}

void osk_se050_close(void)
{
}

osk_result_t osk_se050_curves(uint32_t *mask_out)
{
	if (mask_out == NULL) {
		return OSK_ERR_ARG;
	}
	*mask_out = (1u << OSK_CURVE_ED25519) | (g_no_ecdh ? 0u : (1u << OSK_CURVE_X25519));
	return OSK_OK;
}

osk_result_t osk_se050_random(uint8_t *out, size_t len)
{
	if (out == NULL) {
		return OSK_ERR_ARG;
	}
	for (size_t i = 0; i < len; i++) {
		out[i] = (uint8_t)(0xa5u ^ (uint8_t)i);
	}
	return OSK_OK;
}

osk_result_t osk_se050_key_create(osk_key_id_t id, osk_curve_t curve,
				  const osk_key_policy_t *policy)
{
	if (g_offline) {
		return OSK_ERR_IO;
	}
	if (policy == NULL) {
		return OSK_ERR_ARG;
	}
	if (slot_find(id) != NULL) {
		return OSK_ERR_EXISTS;
	}
	return slot_make(id, curve) != NULL ? OSK_OK : OSK_ERR_FULL;
}

osk_result_t osk_se050_key_delete(osk_key_id_t id)
{
	slot_t *found = slot_find(id);
	if (found == NULL) {
		return OSK_ERR_NOT_FOUND;
	}
	memset(found, 0, sizeof(*found));
	return OSK_OK;
}

osk_result_t osk_se050_key_public(osk_key_id_t id, uint8_t *out, size_t cap, size_t *len_out)
{
	if (g_offline) {
		return OSK_ERR_IO;
	}
	if (out == NULL || len_out == NULL) {
		return OSK_ERR_ARG;
	}
	const slot_t *slot = slot_find(id);
	if (slot == NULL) {
		return OSK_ERR_NOT_FOUND;
	}
	if (cap < 32u) {
		return OSK_ERR_ARG;
	}
	uint8_t buf[32 + 4];
	memcpy(buf, "pub:", 4);
	memcpy(buf + 4, slot->seed, 32);
	osk_sha256(buf, sizeof(buf), out);
	*len_out = 32u;
	return OSK_OK;
}

osk_result_t osk_se050_sign(osk_key_id_t id, const uint8_t *hash, size_t hash_len, uint8_t *sig_out,
			    size_t cap, size_t *len_out)
{
	if (g_offline) {
		return OSK_ERR_IO;
	}
	if (hash == NULL || hash_len == 0u || sig_out == NULL || len_out == NULL) {
		return OSK_ERR_ARG;
	}
	/* Dieselbe Grenze wie im Treiber: fuer Ed25519 geht die ganze
	 * Nachricht in den Chip, und der nimmt hoechstens 128 Byte. */
	if (hash_len > 128u) {
		return OSK_ERR_ARG;
	}
	const slot_t *slot = slot_find(id);
	if (slot == NULL) {
		return OSK_ERR_NOT_FOUND;
	}
	if (slot->curve != OSK_CURVE_ED25519) {
		return OSK_ERR_ARG;
	}
	if (cap < 64u) {
		return OSK_ERR_ARG;
	}
	fake_sign(slot->seed, hash, hash_len, sig_out);
	*len_out = 64u;
	return OSK_OK;
}

osk_result_t osk_se050_ecdh(osk_key_id_t id, const uint8_t *peer_pub, size_t peer_len,
			    uint8_t *secret_out, size_t cap, size_t *len_out)
{
	if (g_offline) {
		return OSK_ERR_IO;
	}
	/* Ein im Werk auf FIPS gestelltes Stueck lehnt ECDH ab. Das ist der
	 * Fall, den docs/threat-model.md offen nennt. */
	if (g_no_ecdh) {
		return OSK_ERR_UNSUPPORTED;
	}
	if (peer_pub == NULL || secret_out == NULL || len_out == NULL || peer_len != 32u) {
		return OSK_ERR_ARG;
	}
	const slot_t *slot = slot_find(id);
	if (slot == NULL) {
		return OSK_ERR_NOT_FOUND;
	}
	if (slot->curve != OSK_CURVE_X25519) {
		return OSK_ERR_ARG;
	}
	if (cap < 32u) {
		return OSK_ERR_ARG;
	}
	osk_hmac_sha256(slot->seed, 32u, peer_pub, peer_len, secret_out);
	*len_out = 32u;
	return OSK_OK;
}

osk_result_t osk_se050_hkdf(osk_key_id_t id, const uint8_t *info, size_t info_len, uint8_t *out,
			    size_t out_len)
{
	if (g_offline) {
		return OSK_ERR_IO;
	}
	if (info == NULL || out == NULL || out_len == 0u) {
		return OSK_ERR_ARG;
	}
	const slot_t *slot = slot_find(id);
	if (slot == NULL) {
		return OSK_ERR_NOT_FOUND;
	}
	/* Echtes HKDF nach RFC 5869 aus dem Baum - nur das Grundgeheimnis ist
	 * eine Attrappe. Das Grundgeheimnis selbst kommt hier nie heraus. */
	return osk_hkdf_sha256(NULL, 0, slot->seed, 32u, info, info_len, out, out_len);
}

osk_result_t osk_se050_key_derive_transient(osk_key_id_t root, osk_curve_t curve,
					    const uint8_t *info, size_t info_len, osk_key_id_t slot)
{
	(void)root;
	(void)curve;
	(void)info;
	(void)info_len;
	(void)slot;
	/* Wie der echte Treiber: der SE05x kann das nicht (docs/se050.md). */
	return OSK_ERR_UNSUPPORTED;
}

osk_result_t osk_se050_hmac(osk_key_id_t id, const uint8_t *msg, size_t msg_len, uint8_t *mac_out,
			    size_t cap, size_t *len_out)
{
	if (msg == NULL || mac_out == NULL || len_out == NULL || cap < 32u) {
		return OSK_ERR_ARG;
	}
	const slot_t *slot = slot_find(id);
	if (slot == NULL) {
		return OSK_ERR_NOT_FOUND;
	}
	osk_hmac_sha256(slot->seed, 32u, msg, msg_len, mac_out);
	*len_out = 32u;
	return OSK_OK;
}

osk_result_t osk_se050_pin_verify(const uint8_t *pin, size_t len)
{
	(void)pin;
	(void)len;
	return OSK_OK;
}

osk_result_t osk_se050_pin_set(const uint8_t *old_pin, size_t old_len, const uint8_t *new_pin,
			       size_t new_len)
{
	(void)old_pin;
	(void)old_len;
	(void)new_pin;
	(void)new_len;
	return OSK_OK;
}

osk_result_t osk_se050_pin_tries_left(uint8_t *tries_out)
{
	if (tries_out == NULL) {
		return OSK_ERR_ARG;
	}
	*tries_out = 8u;
	return OSK_OK;
}

osk_result_t osk_se050_pin_clear(void)
{
	return OSK_OK;
}

osk_result_t osk_se050_free_memory(uint16_t *bytes_out)
{
	if (bytes_out == NULL) {
		return OSK_ERR_ARG;
	}
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

/* ---- Anzeige ----------------------------------------------------------- */

static osk_result_t g_confirm = OSK_OK;
static unsigned g_confirm_count;
static char g_last_subject[SUBJECT_MAX];
static int g_last_kind = -1;

void mock_ui_set_confirm(osk_result_t rc)
{
	g_confirm = rc;
}

unsigned mock_ui_confirm_count(void)
{
	return g_confirm_count;
}

const char *mock_ui_last_subject(void)
{
	return g_last_subject;
}

int mock_ui_last_kind(void)
{
	return g_last_kind;
}

osk_result_t osk_ui_confirm(const osk_ask_req_t *req)
{
	if (req == NULL) {
		return OSK_ERR_ARG;
	}
	g_confirm_count++;
	g_last_kind = (int)req->kind;
	memset(g_last_subject, 0, sizeof(g_last_subject));
	if (req->subject != NULL) {
		size_t len = strlen(req->subject);
		if (len > sizeof(g_last_subject) - 1u) {
			len = sizeof(g_last_subject) - 1u;
		}
		memcpy(g_last_subject, req->subject, len);
	}
	return g_confirm;
}

osk_result_t osk_ui_verify_user(bool *verified_out)
{
	if (verified_out == NULL) {
		return OSK_ERR_ARG;
	}
	*verified_out = true;
	return OSK_OK;
}

void osk_ui_status(const char *text_key)
{
	(void)text_key;
}

/* ---- Tresor ------------------------------------------------------------ */

static bool g_store_open = true;

void mock_store_set_open(bool open)
{
	g_store_open = open;
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

/* ---- Protokoll --------------------------------------------------------- */

static osk_log_entry_t g_log[LOG_MAX];
static unsigned g_log_count;

unsigned mock_log_count(void)
{
	return g_log_count;
}

uint16_t mock_log_what(unsigned index)
{
	return index < g_log_count ? g_log[index].what : 0u;
}

const char *mock_log_who(unsigned index)
{
	if (index >= g_log_count) {
		return NULL;
	}
	return g_log[index].asked_by;
}

uint64_t mock_log_when(unsigned index)
{
	if (index >= g_log_count) {
		return 0u;
	}
	return g_log[index].when_utc;
}

osk_result_t osk_log_add(const osk_log_entry_t *e)
{
	if (e == NULL) {
		return OSK_ERR_ARG;
	}
	if (g_log_count >= LOG_MAX) {
		return OSK_ERR_FULL;
	}
	g_log[g_log_count++] = *e;
	return OSK_OK;
}

osk_result_t osk_log_count(size_t *count_out)
{
	if (count_out == NULL) {
		return OSK_ERR_ARG;
	}
	*count_out = g_log_count;
	return OSK_OK;
}

osk_result_t osk_log_get(size_t index, osk_log_entry_t *out)
{
	if (out == NULL) {
		return OSK_ERR_ARG;
	}
	if (index >= g_log_count) {
		return OSK_ERR_NOT_FOUND;
	}
	*out = g_log[index];
	return OSK_OK;
}

osk_result_t osk_log_clear(void)
{
	g_log_count = 0;
	memset(g_log, 0, sizeof(g_log));
	return OSK_OK;
}

/* ---- Transport --------------------------------------------------------- */

osk_result_t osk_transport_register(osk_link_t link, osk_handler_fn handler)
{
	(void)link;
	return handler != NULL ? OSK_OK : OSK_ERR_ARG;
}

osk_result_t osk_transport_enable(osk_link_t link, bool on)
{
	(void)link;
	(void)on;
	return OSK_OK;
}

/* ---- alles auf Anfang -------------------------------------------------- */

void mock_reset_all(void)
{
	memset(g_slots, 0, sizeof(g_slots));
	g_no_ecdh = false;
	g_offline = false;
	g_now = 0;
	g_confirm = OSK_OK;
	g_confirm_count = 0;
	g_last_kind = -1;
	memset(g_last_subject, 0, sizeof(g_last_subject));
	g_store_open = true;
	(void)osk_log_clear();
}
