/* Umsetzung von wallet_signer.h. */

#include <string.h>
#include "wallet_signer.h"
#include "osk_crypto.h"

static uint32_t curve_mask;
static bool curve_mask_known;

void osk_signer_forget_curves(void)
{
	curve_mask = 0;
	curve_mask_known = false;
}

osk_result_t osk_signer_supports(osk_curve_t curve, bool *ok_out)
{
	if (ok_out == NULL)
		return OSK_ERR_ARG;
	if (!curve_mask_known) {
		osk_result_t res = osk_se050_curves(&curve_mask);

		if (res != OSK_OK)
			return res;
		curve_mask_known = true;
	}
	*ok_out = (curve_mask & (1u << (unsigned)curve)) != 0;
	return OSK_OK;
}

osk_result_t osk_signer_der_to_raw(const uint8_t *der, size_t len, uint8_t r_out[32],
				   uint8_t s_out[32])
{
	size_t pos = 0;

	if (der == NULL || r_out == NULL || s_out == NULL)
		return OSK_ERR_ARG;
	if (len < 8u || len > 72u)
		return OSK_ERR_FORMAT;
	if (der[pos++] != 0x30u)
		return OSK_ERR_FORMAT;
	if (der[pos] != len - 2u)
		return OSK_ERR_FORMAT;
	pos++;
	for (unsigned part = 0; part < 2; part++) {
		uint8_t *target = (part == 0) ? r_out : s_out;
		size_t value_len;

		if (pos + 2u > len || der[pos] != 0x02u)
			return OSK_ERR_FORMAT;
		pos++;
		value_len = der[pos++];
		if (value_len == 0 || value_len > 33u || pos + value_len > len)
			return OSK_ERR_FORMAT;
		if ((der[pos] & 0x80u) != 0)
			return OSK_ERR_FORMAT; /* negativ - gibt es hier nicht */
		if (value_len > 1u && der[pos] == 0x00u && (der[pos + 1u] & 0x80u) == 0)
			return OSK_ERR_FORMAT; /* fuehrende Null zu viel */
		if (value_len == 33u && der[pos] != 0x00u)
			return OSK_ERR_FORMAT;
		{
			size_t skip = (value_len == 33u) ? 1u : 0u;
			size_t real = value_len - skip;

			memset(target, 0, 32);
			memcpy(target + (32u - real), der + pos + skip, real);
		}
		pos += value_len;
	}
	if (pos != len)
		return OSK_ERR_FORMAT; /* Anhang hinter der Folge */
	return OSK_OK;
}

/* Eine Zahl in DER: fuehrende Nullen weg, und ein 0x00 davor, wenn das
 * hoechste Bit gesetzt ist. */
static size_t der_int(const uint8_t value[32], uint8_t *out)
{
	size_t first = 0, len, written = 0;

	while (first < 31u && value[first] == 0x00u)
		first++;
	len = 32u - first;
	out[written++] = 0x02u;
	if ((value[first] & 0x80u) != 0) {
		out[written++] = (uint8_t)(len + 1u);
		out[written++] = 0x00u;
	} else {
		out[written++] = (uint8_t)len;
	}
	memcpy(out + written, value + first, len);
	return written + len;
}

osk_result_t osk_signer_raw_to_der(const uint8_t r[32], const uint8_t s[32], uint8_t *out,
				   size_t cap, size_t *len_out)
{
	uint8_t body[2 * 35];
	size_t body_len = 0;

	if (r == NULL || s == NULL || out == NULL || len_out == NULL)
		return OSK_ERR_ARG;
	body_len = der_int(r, body);
	body_len += der_int(s, body + body_len);
	if (cap < body_len + 2u)
		return OSK_ERR_ARG;
	out[0] = 0x30u;
	out[1] = (uint8_t)body_len;
	memcpy(out + 2, body, body_len);
	*len_out = body_len + 2u;
	return OSK_OK;
}

osk_result_t osk_signer_sign_hash(osk_curve_t curve, const uint8_t priv[32], const uint8_t hash[32],
				  const osk_ec_point_t *pub, uint8_t r_out[32], uint8_t s_out[32])
{
	uint8_t der[80];
	uint8_t r[32], s[32];
	size_t der_len = 0;
	osk_ec_point_t own;
	const osk_ec_point_t *check = pub;
	bool supported = false;
	osk_result_t res;

	if (priv == NULL || hash == NULL || r_out == NULL || s_out == NULL)
		return OSK_ERR_ARG;

	res = osk_signer_supports(curve, &supported);
	if (res != OSK_OK)
		return res;
	if (!supported)
		/* Der Chip kann die Kurve nicht. Hier endet es - in der
		 * Firmware wird nicht ersatzweise unterschrieben. */
		return OSK_ERR_UNSUPPORTED;

	/* Nachpruefen koennen wir nur secp256k1; eine andere Kurve geht ohne
	 * eigene Pruefung durch, und das steht in docs/wallet.md. */
	if (curve == OSK_CURVE_SECP256K1 && check == NULL) {
		res = osk_ec_public_from_private(priv, &own);
		if (res != OSK_OK)
			return res;
		check = &own;
	}

	res = osk_se050_key_import_ec(OSK_WALLET_SIGN_SLOT, curve, priv, 32);
	if (res != OSK_OK)
		return res;
	res = osk_se050_sign(OSK_WALLET_SIGN_SLOT, hash, 32, der, sizeof(der), &der_len);
	(void)osk_se050_key_delete(OSK_WALLET_SIGN_SLOT);
	if (res != OSK_OK) {
		osk_wipe(der, sizeof(der));
		return res;
	}
	res = osk_signer_der_to_raw(der, der_len, r, s);
	osk_wipe(der, sizeof(der));
	if (res != OSK_OK)
		return res;

	if (osk_ec_s_is_high(s)) {
		uint8_t low[32];

		osk_ec_s_negate(s, low);
		memcpy(s, low, sizeof(s));
		osk_wipe(low, sizeof(low));
	}

	if (curve == OSK_CURVE_SECP256K1 && check != NULL && !osk_ec_verify(hash, r, s, check)) {
		osk_wipe(r, sizeof(r));
		osk_wipe(s, sizeof(s));
		return OSK_ERR_INTERNAL; /* der Chip hat etwas anderes geliefert */
	}
	memcpy(r_out, r, 32);
	memcpy(s_out, s, 32);
	osk_wipe(r, sizeof(r));
	osk_wipe(s, sizeof(s));
	return OSK_OK;
}
