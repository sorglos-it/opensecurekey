/* Credential-ID packen und pruefen. Siehe osk_credid.h fuer den Aufbau. */
#include <string.h>
#include "osk_credid.h"
#include "osk_crypto.h"
#include "osk_ctap_internal.h"
#include "osk_se050.h"

#define BODY_LEN 64
#define IV_LEN   16
#define TAG_LEN  16

osk_result_t osk_credid_pack(const osk_credid_t *in, uint8_t *out, size_t cap,
			     size_t *len_out)
{
	uint8_t enc_key[32], mac_key[32];
	uint8_t body[BODY_LEN];
	uint8_t tag[32];
	osk_result_t rc;

	if (!in || !out || !len_out || cap < OSK_CREDID_LEN)
		return OSK_ERR_ARG;
	if (in->cred_protect < OSK_CRED_PROTECT_OPTIONAL ||
	    in->cred_protect > OSK_CRED_PROTECT_UV_REQUIRED)
		return OSK_ERR_ARG;

	memset(body, 0, sizeof(body));
	body[0] = (uint8_t)((in->resident ? 0x01 : 0x00) | (in->hmac_secret ? 0x02 : 0x00));
	body[1] = in->cred_protect;
	memcpy(body + 2, in->rp_id_hash, 32);
	memcpy(body + 34, in->salt, OSK_KEY_SALT_LEN);
	body[50] = (uint8_t)(in->created >> 24);
	body[51] = (uint8_t)(in->created >> 16);
	body[52] = (uint8_t)(in->created >> 8);
	body[53] = (uint8_t)in->created;

	rc = osk_ctap_key_wrap_keys(enc_key, mac_key);
	if (rc != OSK_OK)
		goto done;

	out[0] = OSK_CREDID_VERSION;
	/* Der IV ist der Zufall aus der Ableitung selbst: er ist je ID neu und
	 * kommt aus dem Chip, nicht aus dem RAM des nRF52840. */
	rc = osk_se050_random(out + 1, IV_LEN);
	if (rc != OSK_OK)
		goto done;

	rc = osk_aes256_cbc_encrypt(enc_key, out + 1, body, out + 1 + IV_LEN, BODY_LEN);
	if (rc != OSK_OK)
		goto done;

	osk_hmac_sha256(mac_key, sizeof(mac_key), out, 1 + IV_LEN + BODY_LEN, tag);
	memcpy(out + 1 + IV_LEN + BODY_LEN, tag, TAG_LEN);
	*len_out = OSK_CREDID_LEN;

done:
	osk_wipe(enc_key, sizeof(enc_key));
	osk_wipe(mac_key, sizeof(mac_key));
	osk_wipe(body, sizeof(body));
	osk_wipe(tag, sizeof(tag));
	return rc;
}

osk_result_t osk_credid_unpack(const uint8_t *in, size_t len, osk_credid_t *out)
{
	uint8_t enc_key[32], mac_key[32];
	uint8_t body[BODY_LEN];
	uint8_t tag[32];
	osk_result_t rc = OSK_ERR_NOT_FOUND;

	if (!in || !out || len != OSK_CREDID_LEN)
		return OSK_ERR_NOT_FOUND;
	if (in[0] != OSK_CREDID_VERSION)
		return OSK_ERR_NOT_FOUND;

	if (osk_ctap_key_wrap_keys(enc_key, mac_key) != OSK_OK) {
		rc = OSK_ERR_IO;
		goto done;
	}

	/* Erst der MAC. Der Vergleich laeuft ohne Zeitverrat. */
	osk_hmac_sha256(mac_key, sizeof(mac_key), in, 1 + IV_LEN + BODY_LEN, tag);
	if (!osk_memeq(tag, in + 1 + IV_LEN + BODY_LEN, TAG_LEN))
		goto done;

	if (osk_aes256_cbc_decrypt(enc_key, in + 1, in + 1 + IV_LEN, body, BODY_LEN) != OSK_OK)
		goto done;

	/* Der freie Teil muss null sein - sonst passt der Inhalt nicht zu
	 * dieser Fassung, und wir deuten nichts hinein. */
	for (unsigned i = 54; i < BODY_LEN; i++) {
		if (body[i] != 0)
			goto done;
	}
	if (body[1] < OSK_CRED_PROTECT_OPTIONAL || body[1] > OSK_CRED_PROTECT_UV_REQUIRED)
		goto done;
	if ((body[0] & 0xfc) != 0)
		goto done;

	out->resident = (body[0] & 0x01) != 0;
	out->hmac_secret = (body[0] & 0x02) != 0;
	out->cred_protect = body[1];
	memcpy(out->rp_id_hash, body + 2, 32);
	memcpy(out->salt, body + 34, OSK_KEY_SALT_LEN);
	out->created = ((uint32_t)body[50] << 24) | ((uint32_t)body[51] << 16) |
		       ((uint32_t)body[52] << 8) | (uint32_t)body[53];
	rc = OSK_OK;

done:
	osk_wipe(enc_key, sizeof(enc_key));
	osk_wipe(mac_key, sizeof(mac_key));
	osk_wipe(body, sizeof(body));
	osk_wipe(tag, sizeof(tag));
	return rc;
}
