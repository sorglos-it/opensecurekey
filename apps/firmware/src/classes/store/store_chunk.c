/* Stueckweises Verschluesseln - siehe store_chunk.h. */

#include <string.h>
#include "store_chunk.h"

static uint8_t g_plain[OSK_CHUNK_PLAIN_MAX];
static uint8_t g_ct[OSK_CHUNK_CT_MAX];
static uint8_t g_hashbuf[OSK_HASH_LEN + OSK_CHUNK_CT_MAX];

uint8_t *osk_chunk_plain(void)
{
	return g_plain;
}

void osk_chunk_wipe(void)
{
	osk_crypto_wipe(g_plain, sizeof(g_plain));
	osk_crypto_wipe(g_ct, sizeof(g_ct));
	osk_crypto_wipe(g_hashbuf, sizeof(g_hashbuf));
}

/* Abdruck fortschreiben: h = SHA-256(h || Kryptotext des Stuecks). */
static osk_result_t roll(uint8_t h[OSK_HASH_LEN], const uint8_t *ct,
			 size_t ct_len)
{
	if (ct_len > OSK_CHUNK_CT_MAX) {
		return OSK_ERR_ARG;
	}
	memcpy(g_hashbuf, h, OSK_HASH_LEN);
	memcpy(g_hashbuf + OSK_HASH_LEN, ct, ct_len);
	return osk_crypto_sha256(g_hashbuf, OSK_HASH_LEN + ct_len, h);
}

/* Die laufende Nummer gehoert in die zusaetzlichen Daten, damit ein Stueck
 * nicht an eine andere Stelle verschoben werden kann. */
static void seq_aad(const uint8_t *aad, size_t aad_len, uint32_t seq,
		    uint8_t *out, size_t cap, size_t *out_len)
{
	if (aad_len + 4u > cap) {
		*out_len = 0;
		return;
	}
	if (aad_len > 0u) {
		memcpy(out, aad, aad_len);
	}
	for (int i = 0; i < 4; i++) {
		out[aad_len + i] = (uint8_t)(seq >> (8 * i));
	}
	*out_len = aad_len + 4u;
}

osk_result_t osk_chunk_put(const uint8_t *aad, size_t aad_len,
			   const osk_key_t *key, uint32_t seq,
			   osk_nonce_purpose_t purpose, size_t plain_len,
			   osk_chunk_sink_fn sink, void *ctx,
			   uint8_t h[OSK_HASH_LEN])
{
	uint8_t full[OSK_CHUNK_AAD_MAX];
	size_t full_len = 0;
	uint8_t nonce[OSK_NONCE_LEN];
	size_t ct_len = 0;
	uint8_t len_le[4];
	osk_result_t r;

	if (sink == NULL || key == NULL || plain_len > sizeof(g_plain) ||
	    aad_len + 4u > sizeof(full)) {
		return OSK_ERR_ARG;
	}
	seq_aad(aad, aad_len, seq, full, sizeof(full), &full_len);
	osk_nonce_build(seq, purpose, nonce);

	r = osk_crypto_seal(key, nonce, full, full_len, g_plain, plain_len,
			    g_ct, sizeof(g_ct), &ct_len);
	if (r != OSK_OK) {
		return r;
	}
	for (int i = 0; i < 4; i++) {
		len_le[i] = (uint8_t)(ct_len >> (8 * i));
	}
	r = sink(ctx, len_le, sizeof(len_le));
	if (r == OSK_OK) {
		r = sink(ctx, g_ct, ct_len);
	}
	if (r != OSK_OK) {
		return r;
	}
	return roll(h, g_ct, ct_len);
}

osk_result_t osk_chunk_get(const uint8_t *aad, size_t aad_len,
			   const osk_key_t *key, uint32_t seq,
			   osk_nonce_purpose_t purpose, osk_chunk_src_fn src,
			   void *ctx, size_t *plain_len_out,
			   uint8_t h[OSK_HASH_LEN])
{
	uint8_t full[OSK_CHUNK_AAD_MAX];
	size_t full_len = 0;
	uint8_t nonce[OSK_NONCE_LEN];
	uint8_t len_le[4];
	osk_result_t r;

	if (src == NULL || key == NULL || plain_len_out == NULL ||
	    aad_len + 4u > sizeof(full)) {
		return OSK_ERR_ARG;
	}
	r = src(ctx, len_le, sizeof(len_le));
	if (r != OSK_OK) {
		return r;
	}
	uint32_t ct_len = 0;

	for (int i = 3; i >= 0; i--) {
		ct_len = (ct_len << 8) | len_le[i];
	}
	if (ct_len < OSK_TAG_LEN || ct_len > sizeof(g_ct)) {
		return OSK_ERR_FORMAT;
	}
	r = src(ctx, g_ct, ct_len);
	if (r != OSK_OK) {
		return r;
	}
	r = roll(h, g_ct, ct_len);
	if (r != OSK_OK) {
		return r;
	}

	seq_aad(aad, aad_len, seq, full, sizeof(full), &full_len);
	osk_nonce_build(seq, purpose, nonce);
	r = osk_crypto_open(key, nonce, full, full_len, g_ct, ct_len, g_plain,
			    sizeof(g_plain), plain_len_out);
	if (r == OSK_ERR_FORMAT) {
		/* Falscher Pruefwert heisst: falsches Kennwort, falsche
		 * Gegenstelle oder beschaedigte Daten. */
		return OSK_ERR_DENIED;
	}
	return r;
}
