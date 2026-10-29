/* Die Seite des RECHNERS, nachgespielt: Schluesseltausch, PIN setzen, ein
 * pinUvAuthToken holen, Nachrichten beglaubigen.
 *
 * Steht hier und nicht in test_pin.c, weil drei Pruefungen es brauchen -
 * test_pin, test_credmgmt und test_config. Dreimal dasselbe abgeschrieben
 * waere dreimal dieselbe Zahl an drei Stellen, und zwei davon veralten
 * still. */
#ifndef PIN_HELPER_H
#define PIN_HELPER_H

#include "ctap_helper.h"
#include "osk_pin.h"

static uint8_t g_body[2048];
static T_UNUSED size_t g_len;

static T_UNUSED void start(void)
{
	mock_reset_all();
	T_EQ("Kern startet", osk_ctap_init(OSK_CTAP_EXT_HMAC_SECRET), OSK_OK);
}

/* ---- Die Seite des Rechners -------------------------------------------- */

typedef struct {
	uint64_t protocol;
	uint8_t pub[64];    /* unser angeblicher Punkt */
	uint8_t shared[64]; /* HMAC-Teil || AES-Teil */
} platform_t;

static T_UNUSED void platform_init(platform_t *p, uint64_t protocol)
{
	uint8_t z[32];

	p->protocol = protocol;
	for (unsigned i = 0; i < 64; i++)
		p->pub[i] = (uint8_t)(0x40 + i);

	mock_se050_ecdh_expect(p->pub, z);
	if (protocol == 1u) {
		uint8_t h[32];
		osk_sha256(z, 32, h);
		memcpy(p->shared, h, 32);
		memcpy(p->shared + 32, h, 32);
	} else {
		static const uint8_t zero[32] = {0};
		osk_hkdf_sha256(zero, 32, z, 32, (const uint8_t *)"CTAP2 HMAC key", 14, p->shared,
				32);
		osk_hkdf_sha256(zero, 32, z, 32, (const uint8_t *)"CTAP2 AES key", 13,
				p->shared + 32, 32);
	}
}

static T_UNUSED size_t platform_encrypt(const platform_t *p, const uint8_t *in, size_t in_len, uint8_t *out)
{
	if (p->protocol == 1u) {
		static const uint8_t zero_iv[16] = {0};
		osk_aes256_cbc_encrypt(p->shared + 32, zero_iv, in, out, in_len);
		return in_len;
	}
	uint8_t iv[16];
	memset(iv, 0x33, sizeof(iv));
	memcpy(out, iv, 16);
	osk_aes256_cbc_encrypt(p->shared + 32, iv, in, out + 16, in_len);
	return in_len + 16;
}

static T_UNUSED size_t platform_decrypt(const platform_t *p, const uint8_t *in, size_t in_len, uint8_t *out)
{
	if (p->protocol == 1u) {
		static const uint8_t zero_iv[16] = {0};
		osk_aes256_cbc_decrypt(p->shared + 32, zero_iv, in, out, in_len);
		return in_len;
	}
	osk_aes256_cbc_decrypt(p->shared + 32, in, in + 16, out, in_len - 16);
	return in_len - 16;
}

static T_UNUSED size_t platform_mac(const platform_t *p, const uint8_t *key, size_t key_len,
			   const uint8_t *msg, size_t msg_len, uint8_t *out)
{
	uint8_t mac[32];
	const size_t use = (key_len > 32) ? 32 : key_len;

	osk_hmac_sha256(key, use, msg, msg_len, mac);
	const size_t n = (p->protocol == 1u) ? 16u : 32u;
	memcpy(out, mac, n);
	return n;
}

/* Schickt einen ClientPIN-Unterbefehl. */
static T_UNUSED uint8_t client_pin(const platform_t *p, uint64_t sub, bool with_key,
			  const uint8_t *new_pin_enc, size_t new_pin_enc_len,
			  const uint8_t *pin_hash_enc, size_t pin_hash_enc_len, const uint8_t *auth,
			  size_t auth_len, uint64_t permissions, const char *rp_id)
{
	uint8_t msg[512];
	osk_cbor_writer_t w;
	size_t entries = 2; /* Protokoll + Unterbefehl */

	if (with_key)
		entries++;
	if (auth)
		entries++;
	if (new_pin_enc)
		entries++;
	if (pin_hash_enc)
		entries++;
	if (permissions)
		entries++;
	if (rp_id)
		entries++;

	msg[0] = OSK_CTAP_CMD_CLIENT_PIN;
	osk_cbor_writer_init(&w, msg + 1, sizeof(msg) - 1);
	osk_cbor_put_map(&w, entries);
	osk_cbor_put_uint(&w, 0x01);
	osk_cbor_put_uint(&w, p->protocol);
	osk_cbor_put_uint(&w, 0x02);
	osk_cbor_put_uint(&w, sub);
	if (with_key) {
		osk_cbor_put_uint(&w, 0x03);
		osk_cbor_put_map(&w, 5);
		osk_cbor_put_uint(&w, 1);
		osk_cbor_put_uint(&w, 2);
		osk_cbor_put_uint(&w, 3);
		osk_cbor_put_int(&w, -25);
		osk_cbor_put_int(&w, -1);
		osk_cbor_put_uint(&w, 1);
		osk_cbor_put_int(&w, -2);
		osk_cbor_put_bytes(&w, p->pub, 32);
		osk_cbor_put_int(&w, -3);
		osk_cbor_put_bytes(&w, p->pub + 32, 32);
	}
	if (auth) {
		osk_cbor_put_uint(&w, 0x04);
		osk_cbor_put_bytes(&w, auth, auth_len);
	}
	if (new_pin_enc) {
		osk_cbor_put_uint(&w, 0x05);
		osk_cbor_put_bytes(&w, new_pin_enc, new_pin_enc_len);
	}
	if (pin_hash_enc) {
		osk_cbor_put_uint(&w, 0x06);
		osk_cbor_put_bytes(&w, pin_hash_enc, pin_hash_enc_len);
	}
	if (permissions) {
		osk_cbor_put_uint(&w, 0x09);
		osk_cbor_put_uint(&w, permissions);
	}
	if (rp_id) {
		osk_cbor_put_uint(&w, 0x0a);
		osk_cbor_put_text(&w, rp_id);
	}
	T_TRUE("Anfrage passt", osk_cbor_writer_ok(&w));
	T_EQ("Anfrage kanonisch", osk_cbor_validate(msg + 1, w.len), OSK_OK);

	return h_call(msg, 1 + w.len, g_body, sizeof(g_body), &g_len);
}

/* Setzt eine PIN auf dem ueblichen Weg. */
static T_UNUSED void set_pin(const platform_t *p, const char *pin)
{
	uint8_t padded[64];
	uint8_t enc[96];
	uint8_t auth[32];

	memset(padded, 0, sizeof(padded));
	memcpy(padded, pin, strlen(pin));
	const size_t enc_len = platform_encrypt(p, padded, 64, enc);
	const size_t auth_len = platform_mac(p, p->shared, 64, enc, enc_len, auth);

	T_EQ("setPIN ok", client_pin(p, 0x03, true, enc, enc_len, NULL, 0, auth, auth_len, 0, NULL),
	     CTAP2_OK);
}

/* Holt ein pinUvAuthToken mit Rechten. */
static T_UNUSED bool get_token(const platform_t *p, const char *pin, uint64_t permissions, const char *rp_id,
		      uint8_t token_out[32], uint8_t *code_out)
{
	uint8_t h[32];
	uint8_t enc[64];

	osk_sha256((const uint8_t *)pin, strlen(pin), h);
	const size_t enc_len = platform_encrypt(p, h, 16, enc);

	const uint8_t code =
	    client_pin(p, 0x09, true, NULL, 0, enc, enc_len, NULL, 0, permissions, rp_id);
	if (code_out)
		*code_out = code;
	if (code != CTAP2_OK)
		return false;

	osk_cbor_reader_t r;
	if (!h_find(g_body, g_len, 0x02, &r))
		return false;
	const uint8_t *ct = NULL;
	size_t ct_len = 0;
	if (osk_cbor_read_bytes(&r, &ct, &ct_len) != OSK_OK)
		return false;
	uint8_t plain[64];
	const size_t plain_len = platform_decrypt(p, ct, ct_len, plain);
	if (plain_len != 32)
		return false;
	memcpy(token_out, plain, 32);
	return true;
}

#endif
