/* CTAP1/U2F fuer Dienste, die noch kein FIDO2 sprechen.
 *
 * Dieselbe Schluesselableitung wie CTAP2: der Schluessel entsteht aus dem
 * Grundgeheimnis im SE050, und der Schluesselgriff (key handle) IST unsere
 * Credential-ID. Ein Nachweis, der ueber U2F entstand, laesst sich darum
 * auch ueber CTAP2 benutzen und umgekehrt.
 *
 * Grundlage: FIDO U2F Raw Message Formats v1.2 und CTAP 2.2 Abschnitt 10. */
#include <string.h>
#include <stdbool.h>
#include "osk_ctap.h"
#include "osk_ctap_internal.h"
#include "osk_ctap_key.h"
#include "osk_credid.h"
#include "osk_crypto.h"
#include "osk_se050.h"
#include "osk_ui.h"

/* Befehlsbytes */
#define U2F_INS_REGISTER     0x01
#define U2F_INS_AUTHENTICATE 0x02
#define U2F_INS_VERSION      0x03

/* Steuerbytes von U2F_AUTHENTICATE */
#define U2F_CHECK_ONLY       0x07
#define U2F_ENFORCE_UP       0x03
#define U2F_DONT_ENFORCE_UP  0x08

/* Statuswoerter nach ISO 7816-4 */
#define SW_NO_ERROR               0x9000
#define SW_CONDITIONS_NOT_SATISFIED 0x6985
#define SW_WRONG_DATA             0x6a80
#define SW_WRONG_LENGTH           0x6700
#define SW_CLA_NOT_SUPPORTED      0x6e00
#define SW_INS_NOT_SUPPORTED      0x6d00

static osk_result_t finish(osk_response_t *res, size_t at, uint16_t sw)
{
	if (res->cap < at + 2)
		return OSK_ERR_FULL;
	res->data[at] = (uint8_t)(sw >> 8);
	res->data[at + 1] = (uint8_t)sw;
	res->len = at + 2;
	return OSK_OK;
}

static osk_result_t only_sw(osk_response_t *res, uint16_t sw)
{
	return finish(res, 0, sw);
}

/* Fragt nach dem Tastendruck. U2F kennt nur ja oder nein. */
static bool u2f_user_present(void)
{
	osk_ask_req_t ask;

	memset(&ask, 0, sizeof(ask));
	ask.kind = OSK_ASK_LOGIN;
	ask.subject = NULL;    /* U2F nennt den Dienst nicht, nur seinen Hash */
	ask.timeout_ms = OSK_UP_TIMEOUT_MS;
	return osk_ui_confirm(&ask) == OSK_OK;
}

static osk_result_t do_register(const uint8_t *data, size_t len, osk_response_t *res)
{
	if (len != 64)
		return only_sw(res, SW_WRONG_LENGTH);

	const uint8_t *challenge = data;
	const uint8_t *app = data + 32;

	if (!u2f_user_present())
		return only_sw(res, SW_CONDITIONS_NOT_SATISFIED);

	osk_credid_t cid;
	memset(&cid, 0, sizeof(cid));
	cid.resident = false;
	cid.hmac_secret = false;
	cid.cred_protect = OSK_CRED_PROTECT_OPTIONAL;
	memcpy(cid.rp_id_hash, app, 32);
	cid.created = (uint32_t)(osk_ctap_now_ms() / 1000u);
	if (osk_se050_random(cid.salt, sizeof(cid.salt)) != OSK_OK)
		return only_sw(res, SW_WRONG_DATA);

	uint8_t handle[OSK_MAX_CRED_ID];
	size_t handle_len = 0;
	if (osk_credid_pack(&cid, handle, sizeof(handle), &handle_len) != OSK_OK)
		return only_sw(res, SW_WRONG_DATA);

	uint8_t pub[64];
	if (osk_ctap_key_public(cid.salt, cid.rp_id_hash, pub) != OSK_OK)
		return only_sw(res, SW_WRONG_DATA);

	/* Unterschrieben wird: 0x00 || app || challenge || handle || 0x04||pub */
	uint8_t hash[32];
	{
		const uint8_t zero = 0x00;
		const uint8_t four = 0x04;
		osk_sha256_t c;
		osk_sha256_init(&c);
		osk_sha256_update(&c, &zero, 1);
		osk_sha256_update(&c, app, 32);
		osk_sha256_update(&c, challenge, 32);
		osk_sha256_update(&c, handle, handle_len);
		osk_sha256_update(&c, &four, 1);
		osk_sha256_update(&c, pub, 64);
		osk_sha256_final(&c, hash);
	}

	/* Wir beglaubigen mit dem gerade entstandenen Schluessel selbst; ein
	 * Beglaubigungszertifikat gibt es nicht. Das Zertifikatsfeld bleibt
	 * daher leer - siehe docs/ctap.md, das ist eine bewusste Luecke. */
	uint8_t sig[OSK_MAX_SIG];
	size_t sig_len = 0;
	if (osk_ctap_key_sign(cid.salt, cid.rp_id_hash, hash, sig, sizeof(sig),
			      &sig_len) != OSK_OK)
		return only_sw(res, SW_WRONG_DATA);

	size_t at = 0;
	const size_t need = 1 + 65 + 1 + handle_len + sig_len + 2;
	if (res->cap < need)
		return OSK_ERR_FULL;

	res->data[at++] = 0x05;
	res->data[at++] = 0x04;
	memcpy(res->data + at, pub, 64);
	at += 64;
	res->data[at++] = (uint8_t)handle_len;
	memcpy(res->data + at, handle, handle_len);
	at += handle_len;
	memcpy(res->data + at, sig, sig_len);
	at += sig_len;

	return finish(res, at, SW_NO_ERROR);
}

static osk_result_t do_authenticate(uint8_t control, const uint8_t *data, size_t len,
				    osk_response_t *res)
{
	if (len < 65)
		return only_sw(res, SW_WRONG_LENGTH);

	const uint8_t *challenge = data;
	const uint8_t *app = data + 32;
	const size_t handle_len = data[64];

	if (len != 65 + handle_len)
		return only_sw(res, SW_WRONG_LENGTH);
	if (control != U2F_CHECK_ONLY && control != U2F_ENFORCE_UP &&
	    control != U2F_DONT_ENFORCE_UP)
		return only_sw(res, SW_WRONG_DATA);

	osk_credid_t cid;
	if (osk_credid_unpack(data + 65, handle_len, &cid) != OSK_OK)
		return only_sw(res, SW_WRONG_DATA);
	/* Der Griff muss zu genau diesem Dienst gehoeren. */
	if (!osk_memeq(cid.rp_id_hash, app, 32))
		return only_sw(res, SW_WRONG_DATA);

	/* check-only: der Griff ist gueltig, aber es wird nichts unterschrieben.
	 * Die Antwort darauf ist nach der Spezifikation genau dieser Fehler. */
	if (control == U2F_CHECK_ONLY)
		return only_sw(res, SW_CONDITIONS_NOT_SATISFIED);

	uint8_t presence = 0;
	if (control == U2F_ENFORCE_UP) {
		if (!u2f_user_present())
			return only_sw(res, SW_CONDITIONS_NOT_SATISFIED);
		presence = 0x01;
	}

	osk_ctap_state_t *st = osk_ctap_state();
	const uint32_t counter = ++st->sign_count;

	uint8_t counter_be[4] = {
		(uint8_t)(counter >> 24), (uint8_t)(counter >> 16),
		(uint8_t)(counter >> 8), (uint8_t)counter,
	};

	/* Unterschrieben wird: app || presence || counter || challenge */
	uint8_t hash[32];
	{
		osk_sha256_t c;
		osk_sha256_init(&c);
		osk_sha256_update(&c, app, 32);
		osk_sha256_update(&c, &presence, 1);
		osk_sha256_update(&c, counter_be, 4);
		osk_sha256_update(&c, challenge, 32);
		osk_sha256_final(&c, hash);
	}

	uint8_t sig[OSK_MAX_SIG];
	size_t sig_len = 0;
	if (osk_ctap_key_sign(cid.salt, cid.rp_id_hash, hash, sig, sizeof(sig),
			      &sig_len) != OSK_OK)
		return only_sw(res, SW_WRONG_DATA);

	size_t at = 0;
	if (res->cap < 1 + 4 + sig_len + 2)
		return OSK_ERR_FULL;
	res->data[at++] = presence;
	memcpy(res->data + at, counter_be, 4);
	at += 4;
	memcpy(res->data + at, sig, sig_len);
	at += sig_len;

	return finish(res, at, SW_NO_ERROR);
}

osk_result_t osk_ctap_handle_u2f(const osk_request_t *req, osk_response_t *res)
{
	if (!req || !res || !res->data || res->cap < 2)
		return OSK_ERR_ARG;
	if (!osk_ctap_state()->ready)
		return OSK_ERR_INTERNAL;

	/* Erweitertes APDU: CLA INS P1 P2 00 Lc1 Lc2 Daten... */
	if (!req->data || req->len < 4)
		return only_sw(res, SW_WRONG_LENGTH);

	const uint8_t cla = req->data[0];
	const uint8_t ins = req->data[1];
	const uint8_t p1 = req->data[2];

	if (cla != 0x00)
		return only_sw(res, SW_CLA_NOT_SUPPORTED);

	size_t data_len = 0;
	const uint8_t *data = NULL;

	if (req->len == 4) {
		data_len = 0;
	} else if (req->len >= 7 && req->data[4] == 0x00) {
		/* erweiterte Laenge, drei Bytes, gross-endisch */
		data_len = ((size_t)req->data[5] << 8) | req->data[6];
		if (req->len < 7 + data_len)
			return only_sw(res, SW_WRONG_LENGTH);
		data = req->data + 7;
	} else if (req->len >= 5) {
		/* kurze Laenge - manche Lesegeraete schicken das ueber NFC */
		data_len = req->data[4];
		if (req->len < 5 + data_len)
			return only_sw(res, SW_WRONG_LENGTH);
		data = req->data + 5;
	} else {
		return only_sw(res, SW_WRONG_LENGTH);
	}

	switch (ins) {
	case U2F_INS_REGISTER:
		return do_register(data, data_len, res);
	case U2F_INS_AUTHENTICATE:
		return do_authenticate(p1, data, data_len, res);
	case U2F_INS_VERSION: {
		if (data_len != 0)
			return only_sw(res, SW_WRONG_LENGTH);
		if (res->cap < 8)
			return OSK_ERR_FULL;
		memcpy(res->data, "U2F_V2", 6);
		return finish(res, 6, SW_NO_ERROR);
	}
	default:
		return only_sw(res, SW_INS_NOT_SUPPORTED);
	}
}
