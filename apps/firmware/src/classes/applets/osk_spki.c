/* SubjectPublicKeyInfo. Siehe osk_spki.h. */
#include "osk_spki.h"
#include "osk_der.h"
#include "osk_card.h"
#include "osk_rsa_card.h"

size_t osk_spki_point_len(osk_spki_key_t type)
{
	switch (type) {
	case OSK_SPKI_P256:
		return 65u;
	case OSK_SPKI_P384:
		return 97u;
	default:
		return 0u;
	}
}

static osk_result_t write_ec(osk_resp_t *r, osk_key_id_t id, osk_spki_key_t type)
{
	/* Laengster Rohwert: Verpackung des Chips plus der Punkt einer
	 * P-384. Auf dem Stapel, weil es unter 128 Byte bleibt - die grossen
	 * Puffer liegen fest (osk_attest.c, Kopf). */
	uint8_t raw[128];
	size_t raw_len = 0;
	const uint8_t *point = NULL;
	const size_t point_len = osk_spki_point_len(type);
	const osk_oid_t *curve =
	    (type == OSK_SPKI_P384) ? &osk_oid_curve_p384 : &osk_oid_curve_p256;
	osk_tlv_mark_t m;

	if (osk_se050_key_public(id, raw, sizeof(raw), &raw_len) != OSK_OK)
		return OSK_ERR_NOT_FOUND;
	if (osk_card_public_point(raw, raw_len, point_len, &point) != OSK_OK)
		return OSK_ERR_FORMAT;

	osk_resp_tlv_begin(r, OSK_DER_SEQUENCE, &m);
	osk_der_alg(r, &osk_oid_ec_public, curve, false);
	osk_der_bits(r, point, point_len);
	osk_resp_tlv_end(r, &m);
	return OSK_OK;
}

static osk_result_t write_rsa(osk_resp_t *r, osk_key_id_t id)
{
	/* Fest und nicht auf dem Stapel: ein Modulus mit 4096 Bit ist 512
	 * Byte, und der Faden unter Zephyr hat oft nur 1-2 kB Stapel. */
	static uint8_t mod[OSK_RSA_MOD_MAX];
	uint8_t exp[8];
	size_t mod_len = 0;
	size_t exp_len = 0;
	osk_tlv_mark_t outer;
	osk_tlv_mark_t bits;
	osk_tlv_mark_t seq;

	if (!osk_rsa_available())
		return OSK_ERR_UNSUPPORTED;
	if (osk_rsa_public_parts(id, mod, sizeof(mod), &mod_len, exp, sizeof(exp), &exp_len) !=
	    OSK_OK)
		return OSK_ERR_NOT_FOUND;
	if (mod_len == 0u || exp_len == 0u)
		return OSK_ERR_FORMAT;

	osk_resp_tlv_begin(r, OSK_DER_SEQUENCE, &outer);
	/* Der Parameter ist ein ausdruecklicher NULL-Wert, nicht "nichts":
	 * RFC 8017 Anhang A.1 schreibt ihn vor, und ohne ihn lesen manche
	 * Pruefer den Schluessel gar nicht. */
	osk_der_alg(r, &osk_oid_rsa, NULL, true);
	osk_resp_tlv_begin(r, OSK_DER_BITSTRING, &bits);
	osk_resp_u8(r, 0x00u);
	osk_resp_tlv_begin(r, OSK_DER_SEQUENCE, &seq);
	osk_der_uint(r, mod, mod_len);
	osk_der_uint(r, exp, exp_len);
	osk_resp_tlv_end(r, &seq);
	osk_resp_tlv_end(r, &bits);
	osk_resp_tlv_end(r, &outer);
	return OSK_OK;
}

osk_result_t osk_spki_write(osk_resp_t *r, osk_key_id_t id, osk_spki_key_t type)
{
	if (r == NULL)
		return OSK_ERR_ARG;
	switch (type) {
	case OSK_SPKI_P256:
	case OSK_SPKI_P384:
		return write_ec(r, id, type);
	case OSK_SPKI_RSA:
		return write_rsa(r, id);
	default:
		return OSK_ERR_ARG;
	}
}
