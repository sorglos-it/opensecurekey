/* PIN/UV-Auth-Protokolle 1 und 2 sowie der Befehl authenticatorClientPIN.
 * Siehe osk_pin.h. Spezifikation: CTAP 2.2, Abschnitt 6.5. */
#include <string.h>
#include <stdbool.h>
#include "osk_pin.h"
#include "osk_crypto.h"
#include "osk_ctap_internal.h"
#include "osk_ctap_key.h"
#include "osk_se050.h"
#include "osk_ui.h"

/* Schluessel des ClientPIN-Befehls (Abschnitt 6.5.5) */
#define KEY_PROTOCOL 0x01
#define KEY_SUBCOMMAND 0x02
#define KEY_KEYAGREE 0x03
#define KEY_AUTH_PARAM 0x04
#define KEY_NEW_PIN_ENC 0x05
#define KEY_PIN_HASH_ENC 0x06
#define KEY_PERMISSIONS 0x09
#define KEY_RPID 0x0a

/* Unterbefehle */
#define SUB_GET_PIN_RETRIES 0x01
#define SUB_GET_KEY_AGREE 0x02
#define SUB_SET_PIN 0x03
#define SUB_CHANGE_PIN 0x04
#define SUB_GET_PIN_TOKEN 0x05
#define SUB_TOKEN_USING_UV 0x06
#define SUB_GET_UV_RETRIES 0x07
#define SUB_TOKEN_USING_PIN 0x09

/* Antwortschluessel */
#define RES_KEY_AGREEMENT 0x01
#define RES_PIN_TOKEN 0x02
#define RES_PIN_RETRIES 0x03
#define RES_POWER_CYCLE 0x04
#define RES_UV_RETRIES 0x05

#define UV_RETRIES_MAX 3

static osk_pin_state_t g_pin;
static uint8_t g_uv_retries = UV_RETRIES_MAX;

osk_pin_state_t *osk_pin_state(void)
{
	return &g_pin;
}

bool osk_pin_protocol_supported(uint64_t protocol)
{
	return protocol == 1u || protocol == 2u;
}

osk_result_t osk_pin_regenerate(void)
{
	const osk_key_policy_t policy = {.exportable = false, .needs_touch = false};

	osk_se050_key_delete(OSK_KEY_ID_KEYAGREE);
	return osk_se050_key_create(OSK_KEY_ID_KEYAGREE, OSK_CURVE_P256, &policy);
}

osk_result_t osk_pin_reset_token(void)
{
	const osk_result_t rc = osk_se050_random(g_pin.token, sizeof(g_pin.token));
	if (rc != OSK_OK) {
		osk_wipe(g_pin.token, sizeof(g_pin.token));
		return rc;
	}
	g_pin.in_use = false;
	g_pin.permissions = 0;
	g_pin.rp_id_set = false;
	g_pin.user_present = false;
	g_pin.user_verified = false;
	return OSK_OK;
}

osk_result_t osk_pin_init(void)
{
	uint8_t tries = 0;

	memset(&g_pin, 0, sizeof(g_pin));
	g_uv_retries = UV_RETRIES_MAX;

	osk_result_t rc = osk_pin_regenerate();
	if (rc != OSK_OK)
		return rc;
	rc = osk_pin_reset_token();
	if (rc != OSK_OK)
		return rc;

	/* Ob eine PIN gesetzt ist, weiss der Chip: gibt es keinen Zaehler,
	 * gibt es auch keine PIN. */
	g_pin.pin_set = (osk_se050_pin_tries_left(&tries) == OSK_OK);
	return OSK_OK;
}

osk_result_t osk_pin_shared_secret(uint64_t protocol, const uint8_t peer_pub[64],
				   uint8_t shared_out[OSK_PIN_SHARED_LEN])
{
	uint8_t point[65];
	uint8_t z[48];
	size_t z_len = 0;
	osk_result_t rc;

	if (!osk_pin_protocol_supported(protocol))
		return OSK_ERR_UNSUPPORTED;

	point[0] = 0x04;
	memcpy(point + 1, peer_pub, 64);

	rc = osk_se050_ecdh(OSK_KEY_ID_KEYAGREE, point, sizeof(point), z, sizeof(z), &z_len);
	if (rc != OSK_OK)
		goto done;
	if (z_len != 32) {
		rc = OSK_ERR_FORMAT;
		goto done;
	}

	if (protocol == 1u) {
		/* Protokoll 1: sharedSecret = SHA-256(Z), fuer MAC und AES derselbe Wert. */
		uint8_t h[32];
		osk_sha256(z, z_len, h);
		memcpy(shared_out, h, 32);
		memcpy(shared_out + 32, h, 32);
		osk_wipe(h, sizeof(h));
	} else {
		/* Protokoll 2: zwei getrennte HKDF-Laeufe, nicht einer mit L=64. */
		static const uint8_t zero_salt[32] = {0};
		rc = osk_hkdf_sha256(zero_salt, sizeof(zero_salt), z, z_len,
				     (const uint8_t *)"CTAP2 HMAC key", 14, shared_out, 32);
		if (rc == OSK_OK)
			rc = osk_hkdf_sha256(zero_salt, sizeof(zero_salt), z, z_len,
					     (const uint8_t *)"CTAP2 AES key", 13, shared_out + 32,
					     32);
	}

done:
	osk_wipe(point, sizeof(point));
	osk_wipe(z, sizeof(z));
	return rc;
}

/* Der AES-Teil des gemeinsamen Geheimnisses. */
static const uint8_t *aes_part(const uint8_t *key)
{
	return key + 32;
}

osk_result_t osk_pin_encrypt(uint64_t protocol, const uint8_t *key, const uint8_t *in,
			     size_t in_len, uint8_t *out, size_t cap, size_t *len_out)
{
	if ((in_len % OSK_AES_BLOCK) != 0 || in_len == 0)
		return OSK_ERR_ARG;

	if (protocol == 1u) {
		static const uint8_t zero_iv[OSK_AES_BLOCK] = {0};
		if (cap < in_len)
			return OSK_ERR_ARG;
		const osk_result_t rc =
		    osk_aes256_cbc_encrypt(aes_part(key), zero_iv, in, out, in_len);
		if (rc == OSK_OK)
			*len_out = in_len;
		return rc;
	}

	if (cap < in_len + OSK_AES_BLOCK)
		return OSK_ERR_ARG;
	const osk_result_t rc = osk_se050_random(out, OSK_AES_BLOCK);
	if (rc != OSK_OK)
		return rc;
	const osk_result_t rc2 =
	    osk_aes256_cbc_encrypt(aes_part(key), out, in, out + OSK_AES_BLOCK, in_len);
	if (rc2 == OSK_OK)
		*len_out = in_len + OSK_AES_BLOCK;
	return rc2;
}

osk_result_t osk_pin_decrypt(uint64_t protocol, const uint8_t *key, const uint8_t *in,
			     size_t in_len, uint8_t *out, size_t cap, size_t *len_out)
{
	if (protocol == 1u) {
		static const uint8_t zero_iv[OSK_AES_BLOCK] = {0};
		if (in_len == 0 || (in_len % OSK_AES_BLOCK) != 0 || cap < in_len)
			return OSK_ERR_FORMAT;
		const osk_result_t rc =
		    osk_aes256_cbc_decrypt(aes_part(key), zero_iv, in, out, in_len);
		if (rc == OSK_OK)
			*len_out = in_len;
		return rc;
	}

	/* Protokoll 2: die ersten 16 Bytes sind der IV. */
	if (in_len <= OSK_AES_BLOCK || ((in_len - OSK_AES_BLOCK) % OSK_AES_BLOCK) != 0)
		return OSK_ERR_FORMAT;
	const size_t body = in_len - OSK_AES_BLOCK;
	if (cap < body)
		return OSK_ERR_FORMAT;
	const osk_result_t rc =
	    osk_aes256_cbc_decrypt(aes_part(key), in, in + OSK_AES_BLOCK, out, body);
	if (rc == OSK_OK)
		*len_out = body;
	return rc;
}

void osk_pin_authenticate(uint64_t protocol, const uint8_t *key, size_t key_len, const uint8_t *msg,
			  size_t msg_len, uint8_t *out, size_t *len_out)
{
	uint8_t mac[32];

	/* Ist der Schluessel das gemeinsame Geheimnis (64 Bytes), zaehlt nur
	 * der HMAC-Teil - die ersten 32 Bytes. */
	const size_t use_len = (key_len > 32) ? 32 : key_len;

	osk_hmac_sha256(key, use_len, msg, msg_len, mac);
	if (protocol == 1u) {
		memcpy(out, mac, 16);
		*len_out = 16;
	} else {
		memcpy(out, mac, 32);
		*len_out = 32;
	}
	osk_wipe(mac, sizeof(mac));
}

bool osk_pin_verify_mac(uint64_t protocol, const uint8_t *key, size_t key_len, const uint8_t *msg,
			size_t msg_len, const uint8_t *mac, size_t mac_len)
{
	uint8_t want[32];
	size_t want_len = 0;

	osk_pin_authenticate(protocol, key, key_len, msg, msg_len, want, &want_len);
	const bool ok = (mac_len == want_len) && osk_memeq(want, mac, want_len);
	osk_wipe(want, sizeof(want));
	return ok;
}

bool osk_pin_verify_token(uint64_t protocol, const uint8_t *msg, size_t msg_len, const uint8_t *mac,
			  size_t mac_len)
{
	/* Ein Token, das nicht in Gebrauch ist, prueft nichts - sonst liesse
	 * sich ein altes pinUvAuthParam wiederverwenden. */
	if (!g_pin.in_use)
		return false;
	return osk_pin_verify_mac(protocol, g_pin.token, sizeof(g_pin.token), msg, msg_len, mac,
				  mac_len);
}

void osk_pin_begin_using(bool user_is_present)
{
	g_pin.in_use = true;
	g_pin.user_present = user_is_present;
	g_pin.user_verified = true;
}

void osk_pin_stop_using(void)
{
	g_pin.in_use = false;
	g_pin.permissions = 0;
	g_pin.rp_id_set = false;
	g_pin.user_present = false;
	g_pin.user_verified = false;
}

void osk_pin_clear_up(void)
{
	if (g_pin.in_use)
		g_pin.user_present = false;
}

void osk_pin_clear_uv(void)
{
	if (g_pin.in_use)
		g_pin.user_verified = false;
}

void osk_pin_clear_perms_except_lbw(void)
{
	if (!g_pin.in_use)
		return;
	g_pin.permissions &= (uint8_t)OSK_PERM_LBW;
	if (g_pin.permissions == 0)
		g_pin.in_use = false;
}

bool osk_pin_get_up_flag(void)
{
	return g_pin.in_use && g_pin.user_present;
}

bool osk_pin_get_uv_flag(void)
{
	return g_pin.in_use && g_pin.user_verified;
}

bool osk_pin_has_permission(uint8_t perm)
{
	return g_pin.in_use && (g_pin.permissions & perm) != 0;
}

osk_result_t osk_pin_put_public_key(osk_cbor_writer_t *w)
{
	uint8_t raw[160];
	size_t raw_len = 0;

	const osk_result_t rc =
	    osk_se050_key_public(OSK_KEY_ID_KEYAGREE, raw, sizeof(raw), &raw_len);
	if (rc != OSK_OK)
		return rc;
	if (raw_len < 65 || raw[raw_len - 65] != 0x04)
		return OSK_ERR_FORMAT;

	const uint8_t *pub = raw + raw_len - 65 + 1;
	/* COSE_Key fuer den Schluesseltausch. Die Spezifikation verlangt den
	 * alg-Eintrag und sonst keinen; -25 ist ECDH-ES + HKDF-256. */
	osk_cbor_put_map(w, 5);
	osk_cbor_put_uint(w, 1);
	osk_cbor_put_uint(w, 2); /* kty: EC2 */
	osk_cbor_put_uint(w, 3);
	osk_cbor_put_int(w, -25); /* alg */
	osk_cbor_put_int(w, -1);
	osk_cbor_put_uint(w, 1); /* crv: P-256 */
	osk_cbor_put_int(w, -2);
	osk_cbor_put_bytes(w, pub, 32);
	osk_cbor_put_int(w, -3);
	osk_cbor_put_bytes(w, pub + 32, 32);
	return OSK_OK;
}

/* ---- Hilfen fuer den Befehl -------------------------------------------- */

/* Liest einen COSE_Key mit P-256-Punkt. Nimmt nur, was die Spezifikation
 * erlaubt: kty, alg, crv, x, y - und keine weiteren Eintraege. */
static osk_result_t read_cose_p256(osk_cbor_reader_t *r, uint8_t pub_out[64])
{
	size_t count = 0;
	bool have_x = false, have_y = false, have_kty = false;

	if (osk_cbor_enter_map(r, &count) != OSK_OK)
		return OSK_ERR_FORMAT;
	if (count > 8) {
		osk_cbor_leave(r);
		return OSK_ERR_FORMAT;
	}

	osk_result_t rc = OSK_OK;
	for (size_t i = 0; i < count; i++) {
		int64_t key = 0;
		if (osk_cbor_read_int(r, &key) != OSK_OK) {
			rc = OSK_ERR_FORMAT;
			break;
		}
		if (key == 1) {
			uint64_t v = 0;
			if (osk_cbor_read_uint(r, &v) != OSK_OK || v != 2u) {
				rc = OSK_ERR_FORMAT;
				break;
			}
			have_kty = true;
		} else if (key == -1) {
			uint64_t v = 0;
			if (osk_cbor_read_uint(r, &v) != OSK_OK || v != 1u) {
				rc = OSK_ERR_FORMAT;
				break;
			}
		} else if (key == -2 || key == -3) {
			const uint8_t *p = NULL;
			size_t plen = 0;
			if (osk_cbor_read_bytes(r, &p, &plen) != OSK_OK || plen != 32) {
				rc = OSK_ERR_FORMAT;
				break;
			}
			memcpy(pub_out + ((key == -2) ? 0 : 32), p, 32);
			if (key == -2)
				have_x = true;
			else
				have_y = true;
		} else {
			/* alg und Unbekanntes ueberspringen */
			if (osk_cbor_skip(r) != OSK_OK) {
				rc = OSK_ERR_FORMAT;
				break;
			}
		}
	}
	osk_cbor_leave(r);

	if (rc == OSK_OK && (!have_x || !have_y || !have_kty))
		rc = OSK_ERR_FORMAT;
	return rc;
}

/* Zaehlt Unicode-Code-Punkte in UTF-8 und prueft dabei grob die Form. */
static osk_result_t count_code_points(const uint8_t *p, size_t len, uint8_t *out)
{
	size_t i = 0, n = 0;

	while (i < len) {
		size_t extra;
		const uint8_t c = p[i];
		if (c < 0x80)
			extra = 0;
		else if ((c & 0xe0) == 0xc0)
			extra = 1;
		else if ((c & 0xf0) == 0xe0)
			extra = 2;
		else if ((c & 0xf8) == 0xf0)
			extra = 3;
		else
			return OSK_ERR_FORMAT;
		if (extra > 0 && i + extra >= len)
			return OSK_ERR_FORMAT;
		for (size_t k = 1; k <= extra; k++) {
			if ((p[i + k] & 0xc0) != 0x80)
				return OSK_ERR_FORMAT;
		}
		i += extra + 1;
		n++;
		if (n > 255)
			return OSK_ERR_FORMAT;
	}
	*out = (uint8_t)n;
	return OSK_OK;
}

typedef struct {
	uint64_t protocol;
	bool have_protocol;
	uint64_t sub;
	bool have_sub;
	uint8_t peer_pub[64];
	bool have_peer;
	const uint8_t *auth_param;
	size_t auth_param_len;
	const uint8_t *new_pin_enc;
	size_t new_pin_enc_len;
	const uint8_t *pin_hash_enc;
	size_t pin_hash_enc_len;
	uint64_t permissions;
	bool have_permissions;
	const char *rp_id;
	size_t rp_id_len;
} pin_req_t;

static uint8_t parse_request(const uint8_t *cbor, size_t len, pin_req_t *out)
{
	osk_cbor_reader_t r;
	size_t count = 0;

	memset(out, 0, sizeof(*out));
	if (osk_cbor_validate(cbor, len) != OSK_OK)
		return CTAP2_ERR_INVALID_CBOR;

	osk_cbor_reader_init(&r, cbor, len);
	if (osk_cbor_enter_map(&r, &count) != OSK_OK)
		return CTAP2_ERR_INVALID_CBOR;

	for (size_t i = 0; i < count; i++) {
		uint64_t key = 0;
		if (osk_cbor_read_uint(&r, &key) != OSK_OK) {
			/* Textschluessel kommen in diesem Befehl nicht vor. */
			if (osk_cbor_skip(&r) != OSK_OK || osk_cbor_skip(&r) != OSK_OK)
				return CTAP2_ERR_INVALID_CBOR;
			continue;
		}
		switch (key) {
		case KEY_PROTOCOL:
			if (osk_cbor_read_uint(&r, &out->protocol) != OSK_OK)
				return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
			out->have_protocol = true;
			break;
		case KEY_SUBCOMMAND:
			if (osk_cbor_read_uint(&r, &out->sub) != OSK_OK)
				return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
			out->have_sub = true;
			break;
		case KEY_KEYAGREE:
			if (read_cose_p256(&r, out->peer_pub) != OSK_OK)
				return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
			out->have_peer = true;
			break;
		case KEY_AUTH_PARAM:
			if (osk_cbor_read_bytes(&r, &out->auth_param, &out->auth_param_len) !=
			    OSK_OK)
				return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
			break;
		case KEY_NEW_PIN_ENC:
			if (osk_cbor_read_bytes(&r, &out->new_pin_enc, &out->new_pin_enc_len) !=
			    OSK_OK)
				return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
			break;
		case KEY_PIN_HASH_ENC:
			if (osk_cbor_read_bytes(&r, &out->pin_hash_enc, &out->pin_hash_enc_len) !=
			    OSK_OK)
				return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
			break;
		case KEY_PERMISSIONS:
			if (osk_cbor_read_uint(&r, &out->permissions) != OSK_OK)
				return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
			out->have_permissions = true;
			break;
		case KEY_RPID:
			if (osk_cbor_read_text(&r, &out->rp_id, &out->rp_id_len) != OSK_OK)
				return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
			if (out->rp_id_len >= OSK_MAX_RP_ID)
				return CTAP1_ERR_INVALID_PARAMETER;
			break;
		default:
			if (osk_cbor_skip(&r) != OSK_OK)
				return CTAP2_ERR_INVALID_CBOR;
			break;
		}
	}
	osk_cbor_leave(&r);

	if (!out->have_sub)
		return CTAP2_ERR_MISSING_PARAMETER;
	return CTAP2_OK;
}

/* Fehlerweg nach einem falschen PIN-Abdruck (6.5.5.6 Schritt 8). */
static uint8_t pin_mismatch(void)
{
	uint8_t tries = 0;

	osk_pin_regenerate();
	g_pin.mismatches++;
	if (osk_se050_pin_tries_left(&tries) == OSK_OK && tries == 0)
		return CTAP2_ERR_PIN_BLOCKED;
	if (g_pin.mismatches >= 3) {
		g_pin.power_cycle_needed = true;
		return CTAP2_ERR_PIN_AUTH_BLOCKED;
	}
	return CTAP2_ERR_PIN_INVALID;
}

/* Gemeinsamer Teil von setPIN und changePIN: neue PIN auspacken und pruefen. */
static uint8_t unpack_new_pin(const pin_req_t *q, const uint8_t *shared,
			      uint8_t pin_hash_out[OSK_PIN_HASH_LEN], uint8_t *code_points_out)
{
	uint8_t padded[80];
	size_t padded_len = 0;
	uint8_t rc_code = CTAP2_OK;

	if (osk_pin_decrypt(q->protocol, shared, q->new_pin_enc, q->new_pin_enc_len, padded,
			    sizeof(padded), &padded_len) != OSK_OK) {
		rc_code = CTAP2_ERR_PIN_AUTH_INVALID;
		goto done;
	}
	if (padded_len != 64) {
		rc_code = CTAP1_ERR_INVALID_PARAMETER;
		goto done;
	}

	size_t pin_len = 64;
	while (pin_len > 0 && padded[pin_len - 1] == 0x00)
		pin_len--;
	if (pin_len == 0 || pin_len > OSK_MAX_PIN_BYTES) {
		rc_code = CTAP2_ERR_PIN_POLICY_VIOLATION;
		goto done;
	}

	uint8_t points = 0;
	if (count_code_points(padded, pin_len, &points) != OSK_OK) {
		rc_code = CTAP2_ERR_PIN_POLICY_VIOLATION;
		goto done;
	}
	if (points < osk_ctap_state()->min_pin_len) {
		rc_code = CTAP2_ERR_PIN_POLICY_VIOLATION;
		goto done;
	}

	uint8_t h[32];
	osk_sha256(padded, pin_len, h);
	memcpy(pin_hash_out, h, OSK_PIN_HASH_LEN);
	osk_wipe(h, sizeof(h));
	*code_points_out = points;

done:
	osk_wipe(padded, sizeof(padded));
	return rc_code;
}

static uint8_t do_set_pin(const pin_req_t *q)
{
	uint8_t shared[OSK_PIN_SHARED_LEN];
	uint8_t new_hash[OSK_PIN_HASH_LEN];
	uint8_t points = 0;
	uint8_t code;

	if (!q->have_peer || !q->new_pin_enc || !q->auth_param)
		return CTAP2_ERR_MISSING_PARAMETER;
	if (g_pin.pin_set)
		return CTAP2_ERR_PIN_AUTH_INVALID;

	if (osk_pin_shared_secret(q->protocol, q->peer_pub, shared) != OSK_OK) {
		osk_wipe(shared, sizeof(shared));
		return CTAP1_ERR_INVALID_PARAMETER;
	}

	if (!osk_pin_verify_mac(q->protocol, shared, sizeof(shared), q->new_pin_enc,
				q->new_pin_enc_len, q->auth_param, q->auth_param_len)) {
		osk_wipe(shared, sizeof(shared));
		return CTAP2_ERR_PIN_AUTH_INVALID;
	}

	code = unpack_new_pin(q, shared, new_hash, &points);
	osk_wipe(shared, sizeof(shared));
	if (code != CTAP2_OK)
		return code;

	if (osk_se050_pin_set(NULL, 0, new_hash, sizeof(new_hash)) != OSK_OK) {
		osk_wipe(new_hash, sizeof(new_hash));
		return CTAP1_ERR_OTHER;
	}
	osk_wipe(new_hash, sizeof(new_hash));

	g_pin.pin_set = true;
	g_pin.pin_code_points = points;
	g_pin.mismatches = 0;
	osk_ctap_state()->force_pin_change = false;
	return CTAP2_OK;
}

static uint8_t do_change_pin(const pin_req_t *q)
{
	uint8_t shared[OSK_PIN_SHARED_LEN];
	uint8_t old_hash[32];
	size_t old_len = 0;
	uint8_t new_hash[OSK_PIN_HASH_LEN];
	uint8_t points = 0;
	uint8_t tries = 0;
	uint8_t code = CTAP2_OK;

	if (!q->have_peer || !q->new_pin_enc || !q->pin_hash_enc || !q->auth_param)
		return CTAP2_ERR_MISSING_PARAMETER;
	if (!g_pin.pin_set)
		return CTAP2_ERR_PIN_NOT_SET;
	if (g_pin.power_cycle_needed)
		return CTAP2_ERR_PIN_AUTH_BLOCKED;
	if (osk_se050_pin_tries_left(&tries) == OSK_OK && tries == 0)
		return CTAP2_ERR_PIN_BLOCKED;

	if (osk_pin_shared_secret(q->protocol, q->peer_pub, shared) != OSK_OK) {
		osk_wipe(shared, sizeof(shared));
		return CTAP1_ERR_INVALID_PARAMETER;
	}

	/* Der MAC geht ueber newPinEnc || pinHashEnc. */
	{
		uint8_t want[32];
		size_t want_len = 0;
		uint8_t mac[32];
		osk_hmac_sha256_2(shared, 32, q->new_pin_enc, q->new_pin_enc_len, q->pin_hash_enc,
				  q->pin_hash_enc_len, mac);
		want_len = (q->protocol == 1u) ? 16u : 32u;
		memcpy(want, mac, want_len);
		const bool ok =
		    (q->auth_param_len == want_len) && osk_memeq(want, q->auth_param, want_len);
		osk_wipe(mac, sizeof(mac));
		osk_wipe(want, sizeof(want));
		if (!ok) {
			osk_wipe(shared, sizeof(shared));
			return CTAP2_ERR_PIN_AUTH_INVALID;
		}
	}

	if (osk_pin_decrypt(q->protocol, shared, q->pin_hash_enc, q->pin_hash_enc_len, old_hash,
			    sizeof(old_hash), &old_len) != OSK_OK ||
	    old_len != OSK_PIN_HASH_LEN) {
		osk_wipe(shared, sizeof(shared));
		osk_wipe(old_hash, sizeof(old_hash));
		return CTAP2_ERR_PIN_AUTH_INVALID;
	}

	code = unpack_new_pin(q, shared, new_hash, &points);
	osk_wipe(shared, sizeof(shared));
	if (code != CTAP2_OK) {
		osk_wipe(old_hash, sizeof(old_hash));
		osk_wipe(new_hash, sizeof(new_hash));
		return code;
	}

	/* Der Chip prueft die alte PIN und fuehrt den Fehlversuchszaehler. */
	const osk_result_t rc =
	    osk_se050_pin_set(old_hash, OSK_PIN_HASH_LEN, new_hash, sizeof(new_hash));
	osk_wipe(old_hash, sizeof(old_hash));
	osk_wipe(new_hash, sizeof(new_hash));

	if (rc == OSK_ERR_DENIED)
		return pin_mismatch();
	if (rc == OSK_ERR_LOCKED)
		return CTAP2_ERR_PIN_BLOCKED;
	if (rc != OSK_OK)
		return CTAP1_ERR_OTHER;

	g_pin.mismatches = 0;
	g_pin.pin_code_points = points;
	osk_ctap_state()->force_pin_change = false;
	osk_pin_reset_token();
	return CTAP2_OK;
}

/* Prueft die eingereichte PIN im Chip. */
static uint8_t check_pin(const pin_req_t *q, const uint8_t *shared)
{
	uint8_t hash[32];
	size_t hash_len = 0;
	uint8_t tries = 0;

	if (g_pin.power_cycle_needed)
		return CTAP2_ERR_PIN_AUTH_BLOCKED;
	if (osk_se050_pin_tries_left(&tries) == OSK_OK && tries == 0)
		return CTAP2_ERR_PIN_BLOCKED;

	if (osk_pin_decrypt(q->protocol, shared, q->pin_hash_enc, q->pin_hash_enc_len, hash,
			    sizeof(hash), &hash_len) != OSK_OK ||
	    hash_len != OSK_PIN_HASH_LEN) {
		osk_wipe(hash, sizeof(hash));
		return CTAP2_ERR_PIN_AUTH_INVALID;
	}

	const osk_result_t rc = osk_se050_pin_verify(hash, OSK_PIN_HASH_LEN);
	osk_wipe(hash, sizeof(hash));

	if (rc == OSK_ERR_DENIED)
		return pin_mismatch();
	if (rc == OSK_ERR_LOCKED)
		return CTAP2_ERR_PIN_BLOCKED;
	if (rc != OSK_OK)
		return CTAP1_ERR_OTHER;

	g_pin.mismatches = 0;
	return CTAP2_OK;
}

/* Rechte pruefen und zuweisen (6.5.5.7.2 Schritt 4 und folgende). */
static uint8_t apply_permissions(const pin_req_t *q)
{
	if (!q->have_permissions || q->permissions == 0)
		return CTAP1_ERR_INVALID_PARAMETER;
	if (q->permissions > 0xff)
		return CTAP2_ERR_UNAUTHORIZED_PERM;

	/* Was wir nicht koennen, darf auch nicht verlangt werden: wir haben
	 * weder credMgmt noch bioEnroll noch authenticatorConfig. */
	const uint8_t unsupported = (uint8_t)(OSK_PERM_CM | OSK_PERM_BE | OSK_PERM_ACFG);
	if ((q->permissions & unsupported) != 0)
		return CTAP2_ERR_UNAUTHORIZED_PERM;

	g_pin.permissions = (uint8_t)(q->permissions & (OSK_PERM_MC | OSK_PERM_GA | OSK_PERM_LBW));
	if (g_pin.permissions == 0)
		return CTAP2_ERR_UNAUTHORIZED_PERM;

	/* mc und ga brauchen eine RP-ID; bei lbw ist sie gleichgueltig. */
	if ((g_pin.permissions & (OSK_PERM_MC | OSK_PERM_GA)) != 0 && !q->rp_id)
		return CTAP2_ERR_MISSING_PARAMETER;

	if (q->rp_id) {
		osk_sha256((const uint8_t *)q->rp_id, q->rp_id_len, g_pin.rp_id_hash);
		g_pin.rp_id_set = true;
	} else {
		g_pin.rp_id_set = false;
	}
	return CTAP2_OK;
}

static uint8_t emit_token(const pin_req_t *q, const uint8_t *shared, osk_cbor_writer_t *w)
{
	uint8_t enc[OSK_PIN_TOKEN_LEN + OSK_AES_BLOCK];
	size_t enc_len = 0;

	if (osk_pin_encrypt(q->protocol, shared, g_pin.token, sizeof(g_pin.token), enc, sizeof(enc),
			    &enc_len) != OSK_OK) {
		osk_wipe(enc, sizeof(enc));
		return CTAP1_ERR_OTHER;
	}
	osk_cbor_put_map(w, 1);
	osk_cbor_put_uint(w, RES_PIN_TOKEN);
	osk_cbor_put_bytes(w, enc, enc_len);
	osk_wipe(enc, sizeof(enc));
	return CTAP2_OK;
}

static uint8_t do_token_using_pin(const pin_req_t *q, bool with_permissions, osk_cbor_writer_t *w)
{
	uint8_t shared[OSK_PIN_SHARED_LEN];
	uint8_t code;

	if (!q->have_peer || !q->pin_hash_enc)
		return CTAP2_ERR_MISSING_PARAMETER;
	if (!g_pin.pin_set)
		return CTAP2_ERR_PIN_NOT_SET;
	if (!with_permissions && (q->have_permissions || q->rp_id))
		return CTAP1_ERR_INVALID_PARAMETER;

	if (osk_pin_shared_secret(q->protocol, q->peer_pub, shared) != OSK_OK) {
		osk_wipe(shared, sizeof(shared));
		return CTAP1_ERR_INVALID_PARAMETER;
	}

	code = check_pin(q, shared);
	if (code != CTAP2_OK) {
		osk_wipe(shared, sizeof(shared));
		return code;
	}

	if (osk_ctap_state()->force_pin_change) {
		osk_wipe(shared, sizeof(shared));
		return CTAP2_ERR_PIN_INVALID;
	}

	if (osk_pin_reset_token() != OSK_OK) {
		osk_wipe(shared, sizeof(shared));
		return CTAP1_ERR_OTHER;
	}
	osk_pin_begin_using(false);

	if (with_permissions) {
		code = apply_permissions(q);
		if (code != CTAP2_OK) {
			osk_pin_stop_using();
			osk_wipe(shared, sizeof(shared));
			return code;
		}
	} else {
		/* getPinToken: die Vorgaberechte mc und ga, RP-ID kommt beim
		 * ersten Gebrauch dazu. */
		g_pin.permissions = (uint8_t)(OSK_PERM_MC | OSK_PERM_GA);
		g_pin.rp_id_set = false;
	}

	code = emit_token(q, shared, w);
	osk_wipe(shared, sizeof(shared));
	return code;
}

static uint8_t do_token_using_uv(const pin_req_t *q, osk_cbor_writer_t *w)
{
	uint8_t shared[OSK_PIN_SHARED_LEN];
	bool verified = false;
	uint8_t code;

	if (!q->have_peer)
		return CTAP2_ERR_MISSING_PARAMETER;
	if (g_uv_retries == 0)
		return CTAP2_ERR_UV_BLOCKED;

	if (osk_pin_shared_secret(q->protocol, q->peer_pub, shared) != OSK_OK) {
		osk_wipe(shared, sizeof(shared));
		return CTAP1_ERR_INVALID_PARAMETER;
	}

	const osk_result_t rc = osk_ui_verify_user(&verified);
	if (rc == OSK_ERR_TIMEOUT) {
		osk_wipe(shared, sizeof(shared));
		return CTAP2_ERR_USER_ACTION_TIMEOUT;
	}
	if (rc != OSK_OK || !verified) {
		if (g_uv_retries > 0)
			g_uv_retries--;
		osk_wipe(shared, sizeof(shared));
		return (g_uv_retries == 0) ? CTAP2_ERR_UV_BLOCKED : CTAP2_ERR_UV_INVALID;
	}
	g_uv_retries = UV_RETRIES_MAX;

	if (osk_pin_reset_token() != OSK_OK) {
		osk_wipe(shared, sizeof(shared));
		return CTAP1_ERR_OTHER;
	}
	osk_pin_begin_using(false);

	code = apply_permissions(q);
	if (code != CTAP2_OK) {
		osk_pin_stop_using();
		osk_wipe(shared, sizeof(shared));
		return code;
	}

	code = emit_token(q, shared, w);
	osk_wipe(shared, sizeof(shared));
	return code;
}

uint8_t osk_pin_handle(const uint8_t *cbor, size_t len, osk_cbor_writer_t *w)
{
	pin_req_t q;
	uint8_t tries = 0;

	const uint8_t perr = parse_request(cbor, len, &q);
	if (perr != CTAP2_OK)
		return perr;

	/* getPINRetries und getUVRetries brauchen kein Protokoll. */
	if (q.sub == SUB_GET_PIN_RETRIES) {
		if (osk_se050_pin_tries_left(&tries) != OSK_OK)
			tries = 0;
		osk_cbor_put_map(w, 2);
		osk_cbor_put_uint(w, RES_PIN_RETRIES);
		osk_cbor_put_uint(w, tries);
		osk_cbor_put_uint(w, RES_POWER_CYCLE);
		osk_cbor_put_bool(w, g_pin.power_cycle_needed);
		return CTAP2_OK;
	}
	if (q.sub == SUB_GET_UV_RETRIES) {
		osk_cbor_put_map(w, 1);
		osk_cbor_put_uint(w, RES_UV_RETRIES);
		osk_cbor_put_uint(w, g_uv_retries);
		return CTAP2_OK;
	}

	if (!q.have_protocol)
		return CTAP2_ERR_MISSING_PARAMETER;
	if (!osk_pin_protocol_supported(q.protocol))
		return CTAP1_ERR_INVALID_PARAMETER;

	switch (q.sub) {
	case SUB_GET_KEY_AGREE:
		osk_cbor_put_map(w, 1);
		osk_cbor_put_uint(w, RES_KEY_AGREEMENT);
		if (osk_pin_put_public_key(w) != OSK_OK)
			return CTAP1_ERR_OTHER;
		return CTAP2_OK;
	case SUB_SET_PIN:
		return do_set_pin(&q);
	case SUB_CHANGE_PIN:
		return do_change_pin(&q);
	case SUB_GET_PIN_TOKEN:
		return do_token_using_pin(&q, false, w);
	case SUB_TOKEN_USING_PIN:
		return do_token_using_pin(&q, true, w);
	case SUB_TOKEN_USING_UV:
		return do_token_using_uv(&q, w);
	default:
		return CTAP2_ERR_INVALID_SUBCOMMAND;
	}
}
