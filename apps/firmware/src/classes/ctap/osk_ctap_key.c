/* Abgeleitete Schluessel ueber den SE050. Siehe osk_ctap_key.h. */
#include <string.h>
#include "osk_ctap_key.h"
#include "osk_se050.h"
#include "osk_crypto.h"

/* Fluechtiges Objekt, in dem der abgeleitete Signierschluessel kurz lebt. */
#define OSK_KEY_ID_TRANSIENT 0x00f1d0ffu

/* Baut den Ableitungstext: Zweck || Salz || rpIdHash. Immer dieselbe
 * Reihenfolge, damit aus demselben Dienst immer derselbe Schluessel wird. */
static size_t build_info(const char *purpose, const uint8_t *salt, const uint8_t rp_id_hash[32],
			 uint8_t *out, size_t cap)
{
	const size_t plen = strlen(purpose);
	const size_t need = plen + OSK_KEY_SALT_LEN + 32;

	if (need > cap)
		return 0;
	memcpy(out, purpose, plen);
	memcpy(out + plen, salt, OSK_KEY_SALT_LEN);
	memcpy(out + plen + OSK_KEY_SALT_LEN, rp_id_hash, 32);
	return need;
}

/* Holt den oeffentlichen Punkt aus dem, was der Chip liefert: entweder
 * schon 65 Bytes unkomprimiert, oder in einen DER-Umschlag gepackt.
 * In beiden Faellen stehen die 65 Bytes am Ende und beginnen mit 0x04. */
static osk_result_t extract_point(const uint8_t *buf, size_t len, uint8_t pub_out[64])
{
	if (len < 65)
		return OSK_ERR_FORMAT;
	const uint8_t *p = buf + len - 65;
	if (p[0] != 0x04)
		return OSK_ERR_FORMAT;
	memcpy(pub_out, p + 1, 64);
	return OSK_OK;
}

osk_result_t osk_ctap_key_public(const uint8_t salt[OSK_KEY_SALT_LEN], const uint8_t rp_id_hash[32],
				 uint8_t pub_out[64])
{
	uint8_t info[96];
	uint8_t raw[160];
	size_t raw_len = 0;

	const size_t info_len = build_info(OSK_INFO_CREDKEY, salt, rp_id_hash, info, sizeof(info));
	if (info_len == 0)
		return OSK_ERR_INTERNAL;

	osk_result_t rc = osk_se050_key_derive_transient(OSK_KEY_ID_ROOT, OSK_CURVE_P256, info,
							 info_len, OSK_KEY_ID_TRANSIENT);
	if (rc != OSK_OK)
		return rc;

	rc = osk_se050_key_public(OSK_KEY_ID_TRANSIENT, raw, sizeof(raw), &raw_len);
	if (rc == OSK_OK)
		rc = extract_point(raw, raw_len, pub_out);

	osk_se050_key_delete(OSK_KEY_ID_TRANSIENT);
	osk_wipe(info, sizeof(info));
	osk_wipe(raw, sizeof(raw));
	return rc;
}

osk_result_t osk_ctap_key_sign(const uint8_t salt[OSK_KEY_SALT_LEN], const uint8_t rp_id_hash[32],
			       const uint8_t hash[32], uint8_t *sig_out, size_t cap,
			       size_t *len_out)
{
	uint8_t info[96];

	const size_t info_len = build_info(OSK_INFO_CREDKEY, salt, rp_id_hash, info, sizeof(info));
	if (info_len == 0)
		return OSK_ERR_INTERNAL;

	osk_result_t rc = osk_se050_key_derive_transient(OSK_KEY_ID_ROOT, OSK_CURVE_P256, info,
							 info_len, OSK_KEY_ID_TRANSIENT);
	if (rc == OSK_OK)
		rc = osk_se050_sign(OSK_KEY_ID_TRANSIENT, hash, 32, sig_out, cap, len_out);

	osk_se050_key_delete(OSK_KEY_ID_TRANSIENT);
	osk_wipe(info, sizeof(info));
	return rc;
}

osk_result_t osk_ctap_key_secret(const char *purpose, const uint8_t salt[OSK_KEY_SALT_LEN],
				 const uint8_t rp_id_hash[32], uint8_t out[32])
{
	uint8_t info[96];

	const size_t info_len = build_info(purpose, salt, rp_id_hash, info, sizeof(info));
	if (info_len == 0)
		return OSK_ERR_INTERNAL;

	const osk_result_t rc = osk_se050_hkdf(OSK_KEY_ID_ROOT, info, info_len, out, 32);
	osk_wipe(info, sizeof(info));
	return rc;
}

osk_result_t osk_ctap_key_wrap_keys(uint8_t enc_out[32], uint8_t mac_out[32])
{
	uint8_t both[64];

	const osk_result_t rc = osk_se050_hkdf(OSK_KEY_ID_ROOT, (const uint8_t *)OSK_INFO_CREDID,
					       strlen(OSK_INFO_CREDID), both, sizeof(both));
	if (rc == OSK_OK) {
		memcpy(enc_out, both, 32);
		memcpy(mac_out, both + 32, 32);
	}
	osk_wipe(both, sizeof(both));
	return rc;
}
