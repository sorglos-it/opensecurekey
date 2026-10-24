/* Yubico-OTP und Challenge-Response. Siehe osk_otp.h. */
#include <string.h>
#include "osk_otp.h"
#include "osk_otp_crypto.h"
#include "osk_card.h"
#include "osk_se050.h"
#include "osk_store.h"

/* Der Block, der verschluesselt wird - 16 Bytes, Aufbau nach der
 * Beschreibung des Yubico-OTP-Formats:
 *   0..5   private Kennung (6)
 *   6..7   Benutzungszaehler, kleines Ende zuerst
 *   8..10  Zeitstempel in Achteln einer Sekunde, kleines Ende zuerst
 *   11     Sitzungszaehler
 *   12..13 Zufall
 *   14..15 Pruefsumme ueber 0..13 (Kehrwert), kleines Ende zuerst
 * Wer den Block entschluesselt, rechnet die Pruefsumme ueber alle 16 Bytes
 * und erwartet 0xF0B8. */
#define TOKEN_LEN 16u
#define PUBLIC_ID_LEN 6u
#define PRIVATE_ID_LEN 6u
#define AES_KEY_LEN 16u
#define HMAC_KEY_LEN 20u

/* Aufbau des Tresoreintrags. Die Zaehler liegen mit darin, also im Flash:
 * ein Zaehler im RAM faengt nach jedem Stromausfall von vorn an, und dann
 * nimmt der Pruefdienst das Kennwort nicht mehr an (oder, schlimmer, er
 * nimmt ein altes noch einmal an). */
#define OFF_MODE 0u
#define OFF_PUBLIC 1u
#define OFF_PRIVATE (OFF_PUBLIC + PUBLIC_ID_LEN)
#define OFF_AES (OFF_PRIVATE + PRIVATE_ID_LEN)
#define OFF_USE_CTR (OFF_AES + AES_KEY_LEN)
#define OFF_SESSION_CTR (OFF_USE_CTR + 2u)
#define YUBICO_BODY_LEN (OFF_SESSION_CTR + 1u)

#define OFF_HMAC_KEY 1u
#define HMAC_BODY_LEN (OFF_HMAC_KEY + HMAC_KEY_LEN)

#define BODY_MAX 64u

/* Die Uhr des Geraets - dieselbe, die der CTAP-Kern benutzt. */
uint64_t osk_ctap_now_ms(void);

static const char *const g_slot_tag[OSK_OTP_SLOTS] = {"OTP-1", "OTP-2"};
static const char *const g_slot_name[OSK_OTP_SLOTS] = {
    "Einmalkennwort Steckplatz 1",
    "Einmalkennwort Steckplatz 2",
};

/* Steckplatz 1 oder 2 nach aussen, 0 oder 1 nach innen. */
static bool slot_ok(unsigned slot)
{
	return slot >= 1u && slot <= OSK_OTP_SLOTS;
}

static osk_result_t slot_read(unsigned slot, uint8_t *body, size_t cap, size_t *len_out)
{
	if (!slot_ok(slot))
		return OSK_ERR_ARG;
	return osk_card_obj_read(OSK_ITEM_API_KEY, g_slot_tag[slot - 1u], g_slot_name[slot - 1u],
				 body, cap, len_out);
}

static osk_result_t slot_write(unsigned slot, const uint8_t *body, size_t len)
{
	if (!slot_ok(slot))
		return OSK_ERR_ARG;
	return osk_card_obj_write(OSK_ITEM_API_KEY, g_slot_tag[slot - 1u], g_slot_name[slot - 1u],
				  body, len);
}

osk_result_t osk_otp_program(unsigned slot, uint8_t mode, const uint8_t *public_id,
			     const uint8_t *private_id, const uint8_t *key, size_t key_len)
{
	uint8_t body[BODY_MAX];
	osk_result_t rc;

	if (!slot_ok(slot) || key == NULL)
		return OSK_ERR_ARG;
	memset(body, 0, sizeof(body));

	if (mode == OSK_OTP_MODE_YUBICO) {
		if (public_id == NULL || private_id == NULL || key_len != AES_KEY_LEN)
			return OSK_ERR_ARG;
		body[OFF_MODE] = OSK_OTP_MODE_YUBICO;
		memcpy(body + OFF_PUBLIC, public_id, PUBLIC_ID_LEN);
		memcpy(body + OFF_PRIVATE, private_id, PRIVATE_ID_LEN);
		memcpy(body + OFF_AES, key, AES_KEY_LEN);
		/* Die Zaehler fangen bei 1 an - 0 gilt als "noch nie benutzt". */
		body[OFF_USE_CTR] = 1u;
		body[OFF_USE_CTR + 1u] = 0u;
		body[OFF_SESSION_CTR] = 0u;
		rc = slot_write(slot, body, YUBICO_BODY_LEN);
	} else if (mode == OSK_OTP_MODE_HMAC) {
		if (key_len != HMAC_KEY_LEN)
			return OSK_ERR_ARG;
		body[OFF_MODE] = OSK_OTP_MODE_HMAC;
		memcpy(body + OFF_HMAC_KEY, key, HMAC_KEY_LEN);
		rc = slot_write(slot, body, HMAC_BODY_LEN);
	} else {
		rc = OSK_ERR_UNSUPPORTED;
	}

	osk_card_wipe(body, sizeof(body));
	return rc;
}

osk_result_t osk_otp_generate(unsigned slot, char *out, size_t cap)
{
	uint8_t body[BODY_MAX];
	uint8_t token[TOKEN_LEN];
	uint8_t cipher[TOKEN_LEN];
	size_t body_len = 0;
	uint16_t use_ctr;
	uint16_t crc;
	uint32_t tstp;
	osk_result_t rc;

	if (out == NULL || cap < OSK_OTP_STRING_LEN + 1u)
		return OSK_ERR_ARG;
	out[0] = 0;

	/* Das Oeffnen des Eintrags fragt am Display nach, mit dem Namen des
	 * Steckplatzes (osk_store_reveal). Ein zweites Nachfragen hier waere
	 * nur eine zweite Taste fuer denselben Vorgang. */
	rc = slot_read(slot, body, sizeof(body), &body_len);
	if (rc != OSK_OK)
		return rc;
	if (body_len < YUBICO_BODY_LEN || body[OFF_MODE] != OSK_OTP_MODE_YUBICO) {
		osk_card_wipe(body, sizeof(body));
		return OSK_ERR_UNSUPPORTED;
	}

	use_ctr = (uint16_t)((uint16_t)body[OFF_USE_CTR] | ((uint16_t)body[OFF_USE_CTR + 1u] << 8));
	if (body[OFF_SESSION_CTR] == 0xffu) {
		/* Sitzungszaehler voll: der Benutzungszaehler geht weiter.
		 * Laeuft auch der ueber, ist der Schluessel verbraucht - der
		 * Pruefdienst wuerde ein altes Kennwort wieder annehmen. */
		if (use_ctr == 0x7fffu) {
			osk_card_wipe(body, sizeof(body));
			return OSK_ERR_FULL;
		}
		use_ctr++;
		body[OFF_SESSION_CTR] = 0u;
	} else {
		body[OFF_SESSION_CTR]++;
	}
	body[OFF_USE_CTR] = (uint8_t)use_ctr;
	body[OFF_USE_CTR + 1u] = (uint8_t)(use_ctr >> 8);

	/* Erst den neuen Zaehlerstand festschreiben, dann das Kennwort
	 * bilden. Wer den Strom dazwischen wegnimmt, verliert ein Kennwort -
	 * er bekommt keines zweimal. */
	rc = slot_write(slot, body, body_len);
	if (rc != OSK_OK) {
		osk_card_wipe(body, sizeof(body));
		return rc;
	}

	memset(token, 0, sizeof(token));
	memcpy(token, body + OFF_PRIVATE, PRIVATE_ID_LEN);
	token[6] = (uint8_t)use_ctr;
	token[7] = (uint8_t)(use_ctr >> 8);
	tstp = (uint32_t)((osk_ctap_now_ms() * 8u / 1000u) & 0xffffffu);
	token[8] = (uint8_t)tstp;
	token[9] = (uint8_t)(tstp >> 8);
	token[10] = (uint8_t)(tstp >> 16);
	token[11] = body[OFF_SESSION_CTR];
	if (osk_se050_random(token + 12, 2u) != OSK_OK) {
		osk_card_wipe(body, sizeof(body));
		osk_card_wipe(token, sizeof(token));
		return OSK_ERR_IO;
	}
	/* Angehaengt wird der Kehrwert, siehe osk_otp_crypto.h. */
	crc = (uint16_t)(osk_crc16(token, 14u) ^ OSK_CRC16_XOROUT);
	token[14] = (uint8_t)crc;
	token[15] = (uint8_t)(crc >> 8);

	osk_aes128_encrypt_block(body + OFF_AES, token, cipher);

	if (!osk_modhex_encode(body + OFF_PUBLIC, PUBLIC_ID_LEN, out, cap) ||
	    !osk_modhex_encode(cipher, TOKEN_LEN, out + PUBLIC_ID_LEN * 2u,
			       cap - PUBLIC_ID_LEN * 2u)) {
		osk_card_wipe(body, sizeof(body));
		osk_card_wipe(token, sizeof(token));
		osk_card_wipe(cipher, sizeof(cipher));
		out[0] = 0;
		return OSK_ERR_FULL;
	}

	osk_card_wipe(body, sizeof(body));
	osk_card_wipe(token, sizeof(token));
	osk_card_wipe(cipher, sizeof(cipher));
	return OSK_OK;
}

osk_result_t osk_otp_challenge(unsigned slot, const uint8_t *challenge, size_t len,
			       uint8_t out[OSK_OTP_RESPONSE_LEN])
{
	uint8_t body[BODY_MAX];
	size_t body_len = 0;
	osk_result_t rc;

	if (challenge == NULL || out == NULL)
		return OSK_ERR_ARG;
	if (len == 0u || len > OSK_OTP_CHALLENGE_MAX)
		return OSK_ERR_ARG;

	rc = slot_read(slot, body, sizeof(body), &body_len);
	if (rc != OSK_OK)
		return rc;
	if (body_len < HMAC_BODY_LEN || body[OFF_MODE] != OSK_OTP_MODE_HMAC) {
		osk_card_wipe(body, sizeof(body));
		return OSK_ERR_UNSUPPORTED;
	}

	osk_hmac_sha1(body + OFF_HMAC_KEY, HMAC_KEY_LEN, challenge, len, out);
	osk_card_wipe(body, sizeof(body));
	return OSK_OK;
}

/* --- eigener Kanal ------------------------------------------------------ */

#define CMD_GENERATE 0x01u
#define CMD_CHALLENGE 0x02u

static osk_result_t fail(osk_response_t *res, osk_result_t rc)
{
	if (res->cap >= 2u) {
		res->data[0] = 0xffu;
		res->data[1] = (uint8_t)rc;
		res->len = 2u;
	} else {
		res->len = 0u;
	}
	/* Der Fehler steht in der Antwort; der Transport hat nichts falsch
	 * gemacht und soll keinen eigenen Fehler melden. */
	return OSK_OK;
}

osk_result_t osk_otp_handle(const osk_request_t *req, osk_response_t *res)
{
	unsigned slot;

	if (req == NULL || res == NULL || res->data == NULL)
		return OSK_ERR_ARG;
	res->len = 0u;
	if (req->data == NULL || req->len < 2u)
		return fail(res, OSK_ERR_FORMAT);

	slot = req->data[1];
	if (!slot_ok(slot))
		return fail(res, OSK_ERR_ARG);

	if (req->data[0] == CMD_GENERATE) {
		char text[OSK_OTP_STRING_LEN + 1u];
		osk_result_t rc;

		if (req->len != 2u)
			return fail(res, OSK_ERR_FORMAT);
		rc = osk_otp_generate(slot, text, sizeof(text));
		if (rc != OSK_OK)
			return fail(res, rc);
		if (res->cap < OSK_OTP_STRING_LEN)
			return fail(res, OSK_ERR_FULL);
		memcpy(res->data, text, OSK_OTP_STRING_LEN);
		res->len = OSK_OTP_STRING_LEN;
		osk_card_wipe(text, sizeof(text));
		return OSK_OK;
	}

	if (req->data[0] == CMD_CHALLENGE) {
		uint8_t resp[OSK_OTP_RESPONSE_LEN];
		const size_t len = req->len - 2u;
		osk_result_t rc;

		if (len == 0u || len > OSK_OTP_CHALLENGE_MAX)
			return fail(res, OSK_ERR_FORMAT);
		rc = osk_otp_challenge(slot, req->data + 2, len, resp);
		if (rc != OSK_OK)
			return fail(res, rc);
		if (res->cap < sizeof(resp)) {
			osk_card_wipe(resp, sizeof(resp));
			return fail(res, OSK_ERR_FULL);
		}
		memcpy(res->data, resp, sizeof(resp));
		res->len = sizeof(resp);
		osk_card_wipe(resp, sizeof(resp));
		return OSK_OK;
	}

	return fail(res, OSK_ERR_UNSUPPORTED);
}

osk_result_t osk_otp_init(void)
{
	return osk_transport_register(OSK_LINK_USB_VENDOR, osk_otp_handle);
}
