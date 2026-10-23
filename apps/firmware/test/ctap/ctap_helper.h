/* Gemeinsame Hilfen der CTAP-Pruefungen: echte Nachrichten bauen, Antworten
 * aufschluesseln und dabei immer gegen die kanonische Form pruefen. */
#ifndef CTAP_HELPER_H
#define CTAP_HELPER_H

#include "test_util.h"
#include "mocks.h"
#include "osk_cbor.h"
#include "osk_ctap.h"
#include "osk_ctap_internal.h"
#include "osk_crypto.h"
#include "osk_transport.h"

/* Eine Anfrage an den Kern schicken. Liefert das Statusbyte; der CBOR-Teil
 * landet in body/body_len. */
static __attribute__((unused)) uint8_t h_call(const uint8_t *msg, size_t len, uint8_t *body,
					      size_t cap, size_t *body_len)
{
	static uint8_t out[2048];
	osk_request_t req;
	osk_response_t res;

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));
	req.link = OSK_LINK_USB_HID;
	req.data = msg;
	req.len = len;
	res.data = out;
	res.cap = sizeof(out);

	if (osk_ctap_handle(&req, &res) != OSK_OK || res.len < 1)
		return 0xff;

	*body_len = res.len - 1;
	if (*body_len > cap)
		*body_len = cap;
	if (*body_len)
		memcpy(body, out + 1, *body_len);
	return out[0];
}

/* Sucht einen Schluessel mit ganzzahligem Namen in der obersten Abbildung
 * einer Antwort. Liefert einen Leser, der auf dem Wert steht. */
static __attribute__((unused)) bool h_find(const uint8_t *cbor, size_t len, uint64_t want,
					   osk_cbor_reader_t *out)
{
	osk_cbor_reader_t r;
	size_t count = 0;

	osk_cbor_reader_init(&r, cbor, len);
	if (osk_cbor_enter_map(&r, &count) != OSK_OK)
		return false;
	for (size_t i = 0; i < count; i++) {
		uint64_t key = 0;
		if (osk_cbor_read_uint(&r, &key) != OSK_OK)
			return false;
		if (key == want) {
			*out = r;
			return true;
		}
		if (osk_cbor_skip(&r) != OSK_OK)
			return false;
	}
	return false;
}

/* Holt eine Bytefolge unter einem ganzzahligen Schluessel. */
static __attribute__((unused)) bool h_get_bytes(const uint8_t *cbor, size_t len, uint64_t key,
						const uint8_t **p, size_t *plen)
{
	osk_cbor_reader_t r;
	if (!h_find(cbor, len, key, &r))
		return false;
	return osk_cbor_read_bytes(&r, p, plen) == OSK_OK;
}

/* ---- Nachrichten bauen ------------------------------------------------- */

typedef struct {
	uint8_t buf[1024];
	size_t len;
} h_msg_t;

/* MakeCredential mit den Pflichtfeldern. opts darf NULL sein. */
static __attribute__((unused)) void h_make_credential(h_msg_t *m, const char *rp_id,
						      const char *user_name, bool rk,
						      bool hmac_secret, int cred_protect,
						      const uint8_t *pin_auth, size_t pin_auth_len,
						      uint64_t pin_protocol)
{
	osk_cbor_writer_t w;
	uint8_t hash[32];
	size_t entries = 4;
	const bool with_ext = hmac_secret || cred_protect > 0;

	osk_sha256((const uint8_t *)"clientData", 10, hash);
	if (with_ext)
		entries++;
	if (rk)
		entries++;
	if (pin_auth)
		entries += 2;

	m->buf[0] = OSK_CTAP_CMD_MAKE_CREDENTIAL;
	osk_cbor_writer_init(&w, m->buf + 1, sizeof(m->buf) - 1);
	osk_cbor_put_map(&w, entries);

	osk_cbor_put_uint(&w, 0x01);
	osk_cbor_put_bytes(&w, hash, 32);

	osk_cbor_put_uint(&w, 0x02);
	osk_cbor_put_map(&w, 1);
	osk_cbor_put_text(&w, "id");
	osk_cbor_put_text(&w, rp_id);

	osk_cbor_put_uint(&w, 0x03);
	osk_cbor_put_map(&w, 2);
	osk_cbor_put_text(&w, "id");
	osk_cbor_put_bytes(&w, (const uint8_t *)user_name, strlen(user_name));
	osk_cbor_put_text(&w, "name");
	osk_cbor_put_text(&w, user_name);

	osk_cbor_put_uint(&w, 0x04);
	osk_cbor_put_array(&w, 1);
	osk_cbor_put_map(&w, 2);
	osk_cbor_put_text(&w, "alg");
	osk_cbor_put_int(&w, -7);
	osk_cbor_put_text(&w, "type");
	osk_cbor_put_text(&w, "public-key");

	if (with_ext) {
		osk_cbor_put_uint(&w, 0x06);
		osk_cbor_put_map(&w, (cred_protect > 0 && hmac_secret) ? 2u : 1u);
		if (cred_protect > 0) {
			osk_cbor_put_text(&w, "credProtect");
			osk_cbor_put_uint(&w, (uint64_t)cred_protect);
		}
		if (hmac_secret) {
			osk_cbor_put_text(&w, "hmac-secret");
			osk_cbor_put_bool(&w, true);
		}
	}

	if (rk) {
		osk_cbor_put_uint(&w, 0x07);
		osk_cbor_put_map(&w, 1);
		osk_cbor_put_text(&w, "rk");
		osk_cbor_put_bool(&w, true);
	}

	if (pin_auth) {
		osk_cbor_put_uint(&w, 0x08);
		osk_cbor_put_bytes(&w, pin_auth, pin_auth_len);
		osk_cbor_put_uint(&w, 0x09);
		osk_cbor_put_uint(&w, pin_protocol);
	}

	m->len = 1 + w.len;
}

#endif
