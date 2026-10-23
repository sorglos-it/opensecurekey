/* Krypto-Attrappe - siehe fake_crypto.h. KEINE Sicherheit. */

#include <stdlib.h>
#include <string.h>
#include "fake_crypto.h"
#include "osk_crypto.h"

/* ---- durchschaubarer Mischer ----------------------------------------- */
/* Ein 64-Bit-Mischer nach dem Muster von FNV, danach aufgeblasen. Er ist
 * schnell und gleichmaessig genug, um Vertauschungen und Verfaelschungen im
 * Test aufzudecken - und ausdruecklich keine Hashfunktion fuer den Ernstfall. */

static uint64_t mix(uint64_t h, const uint8_t *d, size_t n)
{
	for (size_t i = 0; i < n; i++) {
		h ^= d[i];
		h *= 0x100000001B3ull;
		h ^= h >> 29;
	}
	return h;
}

static void expand(uint64_t h, uint8_t *out, size_t len)
{
	for (size_t i = 0; i < len; i++) {
		if ((i % 8u) == 0u) {
			h = mix(h, (const uint8_t *)&i, sizeof(i));
		}
		out[i] = (uint8_t)(h >> (8u * (i % 8u)));
	}
}

/* ---- Nonce-Verzeichnis ----------------------------------------------- */

struct seen {
	uint8_t key[OSK_KEY_LEN];
	uint8_t nonce[OSK_NONCE_LEN];
};

#define SEEN_MAX 200000u

static struct seen *g_seen;
static size_t g_seen_n;
static size_t g_dups;
static size_t g_vault_dups;
static uint32_t g_rand_state = 1u;

/* Wie viel entschluesselt wurde. Damit laesst sich die Arbeit einer Suche
 * messen, ohne die Geschwindigkeit dieser Attrappe zu messen - die sagt ueber
 * das Geraet nichts. */
static size_t g_open_calls;
static size_t g_open_bytes;

void fake_crypto_open_reset(void)
{
	g_open_calls = 0;
	g_open_bytes = 0;
}

size_t fake_crypto_open_calls(void)
{
	return g_open_calls;
}

size_t fake_crypto_open_bytes(void)
{
	return g_open_bytes;
}

void fake_crypto_init(void)
{
	if (g_seen == NULL) {
		g_seen = malloc(sizeof(struct seen) * SEEN_MAX);
	}
	fake_crypto_nonce_reset();
	fake_crypto_seed(1u);
}

void fake_crypto_seed(uint32_t seed)
{
	g_rand_state = (seed == 0u) ? 1u : seed;
}

void fake_crypto_nonce_reset(void)
{
	g_seen_n = 0;
	g_dups = 0;
	g_vault_dups = 0;
}

size_t fake_crypto_nonce_count(void)
{
	return g_seen_n;
}

size_t fake_crypto_nonce_dups(void)
{
	return g_dups;
}

size_t fake_crypto_vault_nonce_dups(void)
{
	return g_vault_dups;
}

/* Der Zweck steht in den obersten vier Byte der Nonce. 1 bis 3 ist der Tresor
 * selbst; dort muss die Nonce auch ueber alle Schluessel hinweg einmalig
 * sein, weil alle aus demselben Zaehler kommen. */
static bool is_vault_nonce(const uint8_t *nonce)
{
	uint32_t purpose = 0;

	for (int i = 3; i >= 0; i--) {
		purpose = (purpose << 8) | nonce[8 + i];
	}
	return purpose >= 1u && purpose <= 3u;
}

static void note_nonce(const uint8_t *key, const uint8_t *nonce)
{
	for (size_t i = 0; i < g_seen_n; i++) {
		bool same_nonce = memcmp(g_seen[i].nonce, nonce, OSK_NONCE_LEN) == 0;

		if (!same_nonce) {
			continue;
		}
		if (memcmp(g_seen[i].key, key, OSK_KEY_LEN) == 0) {
			g_dups++;
		}
		if (is_vault_nonce(nonce) && is_vault_nonce(g_seen[i].nonce)) {
			g_vault_dups++;
		}
	}
	if (g_seen_n < SEEN_MAX) {
		memcpy(g_seen[g_seen_n].key, key, OSK_KEY_LEN);
		memcpy(g_seen[g_seen_n].nonce, nonce, OSK_NONCE_LEN);
		g_seen_n++;
	}
}

/* ---- osk_crypto.h ---------------------------------------------------- */

static uint64_t base(const osk_key_t *key, const uint8_t *nonce)
{
	uint64_t h = 0xCBF29CE484222325ull;

	h = mix(h, key->b, OSK_KEY_LEN);
	h = mix(h, nonce, OSK_NONCE_LEN);
	return h;
}

static void tag_of(const osk_key_t *key, const uint8_t *nonce, const uint8_t *aad, size_t aad_len,
		   const uint8_t *plain, size_t plain_len, uint8_t out[OSK_TAG_LEN])
{
	uint64_t h = base(key, nonce);
	uint64_t n = (uint64_t)aad_len;

	h = mix(h, (const uint8_t *)&n, sizeof(n));
	if (aad != NULL && aad_len > 0u) {
		h = mix(h, aad, aad_len);
	}
	n = (uint64_t)plain_len;
	h = mix(h, (const uint8_t *)&n, sizeof(n));
	h = mix(h, plain, plain_len);
	expand(h ^ 0x5A5A5A5Aull, out, OSK_TAG_LEN);
}

static void stream(const osk_key_t *key, const uint8_t *nonce, uint8_t *buf, size_t len)
{
	uint8_t ks[64];
	uint64_t h = base(key, nonce);

	for (size_t off = 0; off < len; off += sizeof(ks)) {
		uint64_t blk = off;

		expand(mix(h, (const uint8_t *)&blk, sizeof(blk)), ks, sizeof(ks));
		size_t take = len - off;

		if (take > sizeof(ks)) {
			take = sizeof(ks);
		}
		for (size_t i = 0; i < take; i++) {
			buf[off + i] ^= ks[i];
		}
	}
}

osk_result_t osk_crypto_seal(const osk_key_t *key, const uint8_t nonce[OSK_NONCE_LEN],
			     const uint8_t *aad, size_t aad_len, const uint8_t *plain,
			     size_t plain_len, uint8_t *ct_out, size_t ct_cap, size_t *ct_len_out)
{
	if (key == NULL || nonce == NULL || plain == NULL || ct_out == NULL || ct_len_out == NULL) {
		return OSK_ERR_ARG;
	}
	if (ct_cap < plain_len + OSK_TAG_LEN) {
		return OSK_ERR_FULL;
	}
	note_nonce(key->b, nonce);

	memcpy(ct_out, plain, plain_len);
	stream(key, nonce, ct_out, plain_len);
	tag_of(key, nonce, aad, aad_len, plain, plain_len, ct_out + plain_len);
	*ct_len_out = plain_len + OSK_TAG_LEN;
	return OSK_OK;
}

osk_result_t osk_crypto_open(const osk_key_t *key, const uint8_t nonce[OSK_NONCE_LEN],
			     const uint8_t *aad, size_t aad_len, const uint8_t *ct, size_t ct_len,
			     uint8_t *plain_out, size_t plain_cap, size_t *plain_len_out)
{
	uint8_t tag[OSK_TAG_LEN];
	uint8_t *tmp;

	if (key == NULL || nonce == NULL || ct == NULL || plain_out == NULL ||
	    plain_len_out == NULL || ct_len < OSK_TAG_LEN) {
		return OSK_ERR_ARG;
	}
	/* Gezaehlt wird JEDER Versuch zu entschluesseln, auch der, der an der
	 * Pruefung scheitert. Nur so misst die Zahl die Arbeit und nicht den
	 * Erfolg. */
	g_open_calls++;
	g_open_bytes += ct_len;

	size_t plain_len = ct_len - OSK_TAG_LEN;

	if (plain_cap < plain_len) {
		return OSK_ERR_FULL;
	}

	/* Erst in einem eigenen Puffer entschluesseln und pruefen. So bekommt
	 * der Aufrufer nie halb entschluesselte Daten zu sehen - genau wie
	 * AES-GCM es verspricht. */
	tmp = malloc(plain_len + 1u);
	if (tmp == NULL) {
		return OSK_ERR_INTERNAL;
	}
	memcpy(tmp, ct, plain_len);
	stream(key, nonce, tmp, plain_len);
	tag_of(key, nonce, aad, aad_len, tmp, plain_len, tag);
	if (memcmp(tag, ct + plain_len, OSK_TAG_LEN) != 0) {
		memset(tmp, 0, plain_len);
		free(tmp);
		return OSK_ERR_FORMAT;
	}
	memcpy(plain_out, tmp, plain_len);
	memset(tmp, 0, plain_len);
	free(tmp);
	*plain_len_out = plain_len;
	return OSK_OK;
}

osk_result_t osk_crypto_sha256(const uint8_t *msg, size_t len, uint8_t out[OSK_HASH_LEN])
{
	uint64_t n = (uint64_t)len;
	uint64_t h = mix(0x9E3779B97F4A7C15ull, (const uint8_t *)&n, sizeof(n));

	if (out == NULL || (msg == NULL && len > 0u)) {
		return OSK_ERR_ARG;
	}
	expand(mix(h, msg, len), out, OSK_HASH_LEN);
	return OSK_OK;
}

osk_result_t osk_crypto_hkdf(const uint8_t *ikm, size_t ikm_len, const uint8_t *salt,
			     size_t salt_len, const uint8_t *info, size_t info_len, uint8_t *out,
			     size_t out_len)
{
	uint64_t h = 0x243F6A8885A308D3ull;
	uint64_t n;

	if (ikm == NULL || out == NULL || out_len == 0u) {
		return OSK_ERR_ARG;
	}
	n = (uint64_t)ikm_len;
	h = mix(h, (const uint8_t *)&n, sizeof(n));
	h = mix(h, ikm, ikm_len);
	n = (uint64_t)salt_len;
	h = mix(h, (const uint8_t *)&n, sizeof(n));
	if (salt != NULL) {
		h = mix(h, salt, salt_len);
	}
	n = (uint64_t)info_len;
	h = mix(h, (const uint8_t *)&n, sizeof(n));
	if (info != NULL) {
		h = mix(h, info, info_len);
	}
	expand(h, out, out_len);
	return OSK_OK;
}

osk_result_t osk_crypto_argon2id(const osk_argon2_params_t *p, const char *passphrase,
				 size_t pass_len, const uint8_t salt[OSK_SALT_LEN], uint8_t *out,
				 size_t out_len)
{
	uint64_t h = 0xA2A2A2A2A2A2A2A2ull;

	if (p == NULL || passphrase == NULL || salt == NULL || out == NULL) {
		return OSK_ERR_ARG;
	}
	/* Die Werte gehen mit ein: eine Datei mit anderen Werten ergibt einen
	 * anderen Schluessel. Gerechnet wird natuerlich nichts Schweres. */
	h = mix(h, (const uint8_t *)&p->m_cost_kib, sizeof(p->m_cost_kib));
	h = mix(h, (const uint8_t *)&p->t_cost, sizeof(p->t_cost));
	h = mix(h, (const uint8_t *)&p->lanes, sizeof(p->lanes));
	h = mix(h, salt, OSK_SALT_LEN);
	h = mix(h, (const uint8_t *)passphrase, pass_len);
	expand(h, out, out_len);
	return OSK_OK;
}

osk_result_t osk_crypto_random(uint8_t *out, size_t len)
{
	if (out == NULL) {
		return OSK_ERR_ARG;
	}
	for (size_t i = 0; i < len; i++) {
		/* xorshift32 - fuer den Test genuegt eine feste Folge. */
		g_rand_state ^= g_rand_state << 13;
		g_rand_state ^= g_rand_state >> 17;
		g_rand_state ^= g_rand_state << 5;
		out[i] = (uint8_t)g_rand_state;
	}
	return OSK_OK;
}

bool osk_crypto_equal(const uint8_t *a, const uint8_t *b, size_t len)
{
	uint8_t diff = 0;

	if (a == NULL || b == NULL) {
		return false;
	}
	for (size_t i = 0; i < len; i++) {
		diff |= (uint8_t)(a[i] ^ b[i]);
	}
	return diff == 0u;
}

void osk_crypto_wipe(void *buf, size_t len)
{
	volatile uint8_t *p = (volatile uint8_t *)buf;

	if (buf == NULL) {
		return;
	}
	while (len-- > 0u) {
		*p++ = 0u;
	}
}
