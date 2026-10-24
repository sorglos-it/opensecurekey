/* Die unterschriebenen Bytefolgen. Siehe osk_guard_msg.h. */
#include <string.h>
#include "osk_guard_msg.h"

/* Die drei Zwecke. Sie stehen hier als getrennte Konstanten, damit ein
 * Zusammenlegen eine Aenderung an DIESER Datei ist und der Prueffall
 * test_forgery.c daran stirbt. */
static const char attest_domain[] = "osk-guard/attest/1";
static const char attest_raw_domain[] = "osk-guard/attest-raw/1";
static const char sign_domain[] = "osk-guard/sign/1";

/* Ein Baukasten mit fester Obergrenze: jeder Anhang prueft vorher, dass er
 * hineinpasst, und meldet sonst OSK_ERR_ARG. Kein Realloc, kein Abschneiden. */
typedef struct {
	uint8_t *buf;
	size_t cap;
	size_t len;
	bool full;
} msg_builder_t;

static void put(msg_builder_t *b, const void *p, size_t len)
{
	if (b->full || len > b->cap - b->len) {
		b->full = true;
		return;
	}
	if (len > 0u) {
		memcpy(b->buf + b->len, p, len);
	}
	b->len += len;
}

static void put_byte(msg_builder_t *b, uint8_t v)
{
	put(b, &v, 1u);
}

/* Zweck plus Trennbyte - der Anfang jeder der drei Bytefolgen. */
static void put_domain(msg_builder_t *b, const char *domain)
{
	put(b, domain, strlen(domain));
	put_byte(b, 0u);
}

static osk_result_t finish(const msg_builder_t *b, size_t *len_out)
{
	if (b->full) {
		return OSK_ERR_ARG;
	}
	*len_out = b->len;
	return OSK_OK;
}

osk_result_t osk_guard_msg_attest(const uint8_t *nonce, size_t nonce_len, const uint8_t *bin_hash,
				  size_t hash_len, uint8_t *out, size_t cap, size_t *len_out)
{
	if (nonce == NULL || bin_hash == NULL || out == NULL || len_out == NULL) {
		return OSK_ERR_ARG;
	}
	/* Der Dongle-Weg hat zwei feste Laengen. Nur so laesst sich die
	 * Bytefolge eindeutig zerlegen - und nur deshalb darf sie ohne
	 * Trennbyte zwischen den beiden Teilen auskommen. */
	if (nonce_len != OSK_GUARD_NONCE_LEN || hash_len != OSK_GUARD_HASH_LEN) {
		return OSK_ERR_ARG;
	}

	msg_builder_t b = {out, cap, 0u, false};
	put_domain(&b, attest_domain);
	put(&b, nonce, nonce_len);
	put(&b, bin_hash, hash_len);
	return finish(&b, len_out);
}

osk_result_t osk_guard_msg_attest_raw(const uint8_t *challenge, size_t len, uint8_t *out,
				      size_t cap, size_t *len_out)
{
	if (challenge == NULL || out == NULL || len_out == NULL) {
		return OSK_ERR_ARG;
	}
	if (len == 0u || len > OSK_GUARD_CHALLENGE_MAX) {
		return OSK_ERR_ARG;
	}

	msg_builder_t b = {out, cap, 0u, false};
	put_domain(&b, attest_raw_domain);
	put(&b, challenge, len);
	return finish(&b, len_out);
}

osk_result_t osk_guard_msg_sign(const char *label, size_t label_len, const uint8_t *job_hash,
				size_t hash_len, uint8_t *out, size_t cap, size_t *len_out)
{
	if (label == NULL || job_hash == NULL || out == NULL || len_out == NULL) {
		return OSK_ERR_ARG;
	}
	if (label_len == 0u || label_len > OSK_GUARD_LABEL_MAX) {
		return OSK_ERR_ARG;
	}
	if (hash_len != OSK_GUARD_HASH_LEN) {
		return OSK_ERR_ARG;
	}
	/* Ein Nullbyte im Namen wuerde die Zerlegung mehrdeutig machen: zwei
	 * verschiedene Namen koennten dieselben Bytes ergeben. */
	if (memchr(label, 0, label_len) != NULL) {
		return OSK_ERR_ARG;
	}

	msg_builder_t b = {out, cap, 0u, false};
	put_domain(&b, sign_domain);
	put(&b, label, label_len);
	put_byte(&b, 0u);
	put(&b, job_hash, hash_len);
	return finish(&b, len_out);
}
