/* Umsetzung der Attrappen. Siehe mocks.h - nichts hiervon ist sicher. */

#include <string.h>
#include "mocks.h"
#include "wallet_bn.h"
#include "wallet_ec.h"
#include "wallet_hash.h"
#include "wallet_signer.h"
#include "osk_crypto.h"

/* --- SE050 --------------------------------------------------------------- */

static uint32_t curves_mask = (1u << OSK_CURVE_P256) | (1u << OSK_CURVE_SECP256K1);
static osk_result_t sign_result = OSK_OK;
static bool corrupt_sign;
static unsigned import_count, delete_count;
static uint8_t slot_key[32];
static bool slot_used;
static osk_curve_t slot_curve;
static uint32_t random_counter;

void mock_se050_set_curves(uint32_t mask)
{
	curves_mask = mask;
	osk_signer_forget_curves();
}

void mock_se050_fail_sign(osk_result_t result)
{
	sign_result = result;
}

void mock_se050_corrupt_sign(bool on)
{
	corrupt_sign = on;
}

void mock_se050_reset(void)
{
	curves_mask = (1u << OSK_CURVE_P256) | (1u << OSK_CURVE_SECP256K1);
	sign_result = OSK_OK;
	corrupt_sign = false;
	import_count = 0;
	delete_count = 0;
	slot_used = false;
	random_counter = 0;
	memset(slot_key, 0, sizeof(slot_key));
	osk_signer_forget_curves();
}

unsigned mock_se050_imports(void)
{
	return import_count;
}

unsigned mock_se050_deletes(void)
{
	return delete_count;
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
	*mask_out = curves_mask;
	return OSK_OK;
}

/* Kein Zufall, sondern ein Zaehler: so ergibt derselbe Lauf dieselben
 * Woerter, und ein Fehler ist nachstellbar. */
osk_result_t osk_se050_random(uint8_t *out, size_t len)
{
	if (out == NULL)
		return OSK_ERR_ARG;
	for (size_t i = 0; i < len; i++)
		out[i] = (uint8_t)((random_counter + (uint32_t)i) & 0xFFu);
	random_counter += (uint32_t)len;
	return OSK_OK;
}

osk_result_t osk_se050_key_import_ec(osk_key_id_t id, osk_curve_t curve, const uint8_t *priv,
				     size_t len)
{
	if (priv == NULL || len != 32u)
		return OSK_ERR_ARG;
	if (id != OSK_WALLET_SIGN_SLOT)
		return OSK_ERR_ARG;
	if ((curves_mask & (1u << (unsigned)curve)) == 0)
		return OSK_ERR_UNSUPPORTED;
	memcpy(slot_key, priv, 32);
	slot_curve = curve;
	slot_used = true;
	import_count++;
	return OSK_OK;
}

osk_result_t osk_se050_key_delete(osk_key_id_t id)
{
	(void)id;
	slot_used = false;
	memset(slot_key, 0, sizeof(slot_key));
	delete_count++;
	return OSK_OK;
}

/* ECDSA auf secp256k1, mit einem aus Schluessel und Hash abgeleiteten k. */
osk_result_t osk_se050_sign(osk_key_id_t id, const uint8_t *hash, size_t hash_len, uint8_t *sig_out,
			    size_t cap, size_t *len_out)
{
	const osk_bn_t *n = osk_ec_order();
	uint8_t mac[OSK_SHA512_LEN];
	osk_bn_t k, e, d, r, s, kinv, tmp;
	osk_ec_point_t point;
	uint8_t r_raw[32], s_raw[32];
	unsigned tries = 0;

	(void)id;
	if (sign_result != OSK_OK)
		return sign_result;
	if (!slot_used)
		return OSK_ERR_NOT_FOUND;
	if (slot_curve != OSK_CURVE_SECP256K1)
		return OSK_ERR_UNSUPPORTED;
	if (hash == NULL || hash_len != 32u || sig_out == NULL || len_out == NULL)
		return OSK_ERR_ARG;

	osk_bn_from_be(&d, slot_key);
	osk_bn_from_be(&e, hash);
	if (osk_bn_cmp(&e, n) >= 0)
		(void)osk_bn_sub(&e, &e, n);

	for (;;) {
		uint8_t seed[33];

		if (tries > 8)
			return OSK_ERR_INTERNAL;
		memcpy(seed, slot_key, 32);
		seed[32] = (uint8_t)tries;
		osk_hmac_sha512(seed, sizeof(seed), hash, hash_len, mac);
		osk_bn_from_be(&k, mac);
		if (osk_bn_cmp(&k, n) >= 0)
			(void)osk_bn_sub(&k, &k, n);
		tries++;
		if (osk_bn_is_zero(&k))
			continue;
		if (osk_ec_point_mul_base(&k, &point) != OSK_OK)
			continue;
		osk_bn_copy(&r, &point.x);
		if (osk_bn_cmp(&r, n) >= 0)
			(void)osk_bn_sub(&r, &r, n);
		if (osk_bn_is_zero(&r))
			continue;
		osk_bn_inv_mod(&kinv, &k, n);
		osk_bn_mul_mod(&tmp, &r, &d, n);
		osk_bn_add_mod(&tmp, &tmp, &e, n);
		osk_bn_mul_mod(&s, &kinv, &tmp, n);
		if (osk_bn_is_zero(&s))
			continue;
		break;
	}

	osk_bn_to_be(&r, r_raw);
	osk_bn_to_be(&s, s_raw);
	if (corrupt_sign)
		s_raw[31] = (uint8_t)(s_raw[31] ^ 0x01u);
	return osk_signer_raw_to_der(r_raw, s_raw, sig_out, cap, len_out);
}

/* Nicht gebraucht, aber im Vertrag - und der Binder will sie sehen. */
osk_result_t osk_se050_key_create(osk_key_id_t id, osk_curve_t curve,
				  const osk_key_policy_t *policy)
{
	(void)id;
	(void)curve;
	(void)policy;
	return OSK_ERR_UNSUPPORTED;
}

osk_result_t osk_se050_key_public(osk_key_id_t id, uint8_t *out, size_t cap, size_t *len_out)
{
	osk_ec_point_t point;

	(void)id;
	if (!slot_used)
		return OSK_ERR_NOT_FOUND;
	if (osk_ec_public_from_private(slot_key, &point) != OSK_OK)
		return OSK_ERR_INTERNAL;
	return osk_ec_point_encode(&point, false, out, cap, len_out);
}

/* --- Anzeige ------------------------------------------------------------- */

static osk_result_t ui_answer = OSK_OK;
static unsigned ui_calls;
static char ui_subject[256];
static char ui_detail[256];
static char ui_status[128];
static osk_ask_t ui_kind;

void mock_ui_answer(osk_result_t result)
{
	ui_answer = result;
}

void mock_ui_reset(void)
{
	ui_answer = OSK_OK;
	ui_calls = 0;
	ui_subject[0] = '\0';
	ui_detail[0] = '\0';
	ui_status[0] = '\0';
	ui_kind = OSK_ASK_LOGIN;
}

unsigned mock_ui_calls(void)
{
	return ui_calls;
}

const char *mock_ui_subject(void)
{
	return ui_subject;
}

const char *mock_ui_detail(void)
{
	return ui_detail;
}

const char *mock_ui_status_key(void)
{
	return ui_status;
}

osk_ask_t mock_ui_kind(void)
{
	return ui_kind;
}

static void copy_text(char *out, size_t cap, const char *in)
{
	size_t len;

	if (in == NULL) {
		out[0] = '\0';
		return;
	}
	len = strlen(in);
	if (len > cap - 1u)
		len = cap - 1u;
	memcpy(out, in, len);
	out[len] = '\0';
}

osk_result_t osk_ui_confirm(const osk_ask_req_t *req)
{
	if (req == NULL)
		return OSK_ERR_ARG;
	ui_calls++;
	ui_kind = req->kind;
	copy_text(ui_subject, sizeof(ui_subject), req->subject);
	copy_text(ui_detail, sizeof(ui_detail), req->detail);
	return ui_answer;
}

osk_result_t osk_ui_verify_user(bool *verified_out)
{
	if (verified_out != NULL)
		*verified_out = true;
	return OSK_OK;
}

void osk_ui_status(const char *text_key)
{
	copy_text(ui_status, sizeof(ui_status), text_key);
}

/* --- Tresor -------------------------------------------------------------- */

#define MOCK_ITEMS 8
#define MOCK_SECRET 128

static struct {
	bool used;
	osk_item_id_t id;
	osk_item_type_t type;
	uint8_t profile;
	char name[48];
	uint8_t tag[32];
	uint8_t tag_len;
	bool mirrored;
	bool paired_host_only;
	uint8_t secret[MOCK_SECRET];
	size_t secret_len;
} items[MOCK_ITEMS];

static bool store_open_flag = true;
static bool store_deny;
static unsigned reveal_count;
static osk_item_id_t next_id = 1;

void mock_store_reset(void)
{
	memset(items, 0, sizeof(items));
	store_open_flag = true;
	store_deny = false;
	reveal_count = 0;
	next_id = 1;
}

void mock_store_deny(bool on)
{
	store_deny = on;
}

unsigned mock_store_reveals(void)
{
	return reveal_count;
}

osk_result_t osk_store_open(void)
{
	store_open_flag = true;
	return OSK_OK;
}

void osk_store_lock(void)
{
	store_open_flag = false;
}

bool osk_store_is_open(void)
{
	return store_open_flag;
}

osk_result_t osk_store_count(uint8_t profile, size_t *count_out)
{
	size_t count = 0;

	if (count_out == NULL)
		return OSK_ERR_ARG;
	for (unsigned i = 0; i < MOCK_ITEMS; i++) {
		if (items[i].used && items[i].profile == profile)
			count++;
	}
	*count_out = count;
	return OSK_OK;
}

osk_result_t osk_store_list(uint8_t profile, size_t index, osk_item_meta_t *out)
{
	size_t seen = 0;

	if (out == NULL)
		return OSK_ERR_ARG;
	for (unsigned i = 0; i < MOCK_ITEMS; i++) {
		if (!items[i].used || items[i].profile != profile)
			continue;
		if (seen++ != index)
			continue;
		memset(out, 0, sizeof(*out));
		out->id = items[i].id;
		out->type = items[i].type;
		out->profile = items[i].profile;
		out->name = items[i].name;
		out->mirrored = items[i].mirrored;
		out->paired_host_only = items[i].paired_host_only;
		memcpy(out->tag, items[i].tag, sizeof(out->tag));
		out->tag_len = items[i].tag_len;
		return OSK_OK;
	}
	return OSK_ERR_NOT_FOUND;
}

osk_result_t osk_store_add(const osk_item_meta_t *meta, const uint8_t *secret, size_t secret_len,
			   osk_item_id_t *id_out)
{
	if (meta == NULL || secret == NULL || id_out == NULL)
		return OSK_ERR_ARG;
	if (secret_len > MOCK_SECRET)
		return OSK_ERR_FULL;
	for (unsigned i = 0; i < MOCK_ITEMS; i++) {
		if (items[i].used)
			continue;
		memset(&items[i], 0, sizeof(items[i]));
		items[i].used = true;
		items[i].id = next_id++;
		items[i].type = meta->type;
		items[i].profile = meta->profile;
		items[i].mirrored = meta->mirrored;
		items[i].paired_host_only = meta->paired_host_only;
		if (meta->name != NULL)
			copy_text(items[i].name, sizeof(items[i].name), meta->name);
		memcpy(items[i].tag, meta->tag, sizeof(items[i].tag));
		items[i].tag_len = meta->tag_len;
		memcpy(items[i].secret, secret, secret_len);
		items[i].secret_len = secret_len;
		*id_out = items[i].id;
		return OSK_OK;
	}
	return OSK_ERR_FULL;
}

osk_result_t osk_store_reveal(osk_item_id_t id, const char *asked_by, uint8_t *out, size_t cap,
			      size_t *len_out)
{
	(void)asked_by;
	if (out == NULL || len_out == NULL)
		return OSK_ERR_ARG;
	reveal_count++;
	if (store_deny)
		return OSK_ERR_DENIED;
	for (unsigned i = 0; i < MOCK_ITEMS; i++) {
		if (!items[i].used || items[i].id != id)
			continue;
		if (items[i].secret_len > cap)
			return OSK_ERR_ARG;
		memcpy(out, items[i].secret, items[i].secret_len);
		*len_out = items[i].secret_len;
		return OSK_OK;
	}
	return OSK_ERR_NOT_FOUND;
}

osk_result_t osk_store_delete(osk_item_id_t id)
{
	for (unsigned i = 0; i < MOCK_ITEMS; i++) {
		if (items[i].used && items[i].id == id) {
			memset(&items[i], 0, sizeof(items[i]));
			return OSK_OK;
		}
	}
	return OSK_ERR_NOT_FOUND;
}

/* --- Gemeinsames --------------------------------------------------------- */

const char *osk_result_text(osk_result_t r)
{
	switch (r) {
	case OSK_OK:
		return "ok";
	case OSK_ERR_ARG:
		return "arg";
	case OSK_ERR_FORMAT:
		return "format";
	case OSK_ERR_DENIED:
		return "denied";
	case OSK_ERR_UNSUPPORTED:
		return "unsupported";
	default:
		return "fehler";
	}
}
