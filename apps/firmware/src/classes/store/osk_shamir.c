/* Shamir im Koerper GF(256), Polynom 0x11B wie bei AES.
 *
 * Fuer jedes der 32 Byte des Geheimnisses wird ein eigenes Polynom vom Grad
 * k-1 gezogen. Das Glied ohne x ist das Geheimnisbyte, die uebrigen Glieder
 * sind Zufall. Ein Teil ist der Wert des Polynoms an der Stelle "index".
 * k Werte legen das Polynom fest, k-1 Werte lassen jeden Wert offen - deshalb
 * verraten zu wenige Teile nichts. */

#include <string.h>
#include "osk_shamir.h"

/* ---- Rechnen in GF(256) ---------------------------------------------- */

static uint8_t gf_mul(uint8_t a, uint8_t b)
{
	uint8_t p = 0;

	/* Russische Bauernmultiplikation, ohne Tabelle und ohne Sprung, der
	 * von den Daten abhaengt. */
	for (int i = 0; i < 8; i++) {
		uint8_t mask = (uint8_t)-(int8_t)(b & 1u);

		p ^= (uint8_t)(a & mask);
		uint8_t hi = (uint8_t)-(int8_t)((a >> 7) & 1u);

		a = (uint8_t)(a << 1);
		a ^= (uint8_t)(0x1Bu & hi);
		b = (uint8_t)(b >> 1);
	}
	return p;
}

static uint8_t gf_pow(uint8_t a, uint8_t e)
{
	uint8_t r = 1;

	while (e-- > 0) {
		r = gf_mul(r, a);
	}
	return r;
}

/* a^254 ist das Inverse, denn a^255 = 1 fuer jedes a ausser 0. */
static uint8_t gf_inv(uint8_t a)
{
	return (a == 0u) ? 0u : gf_pow(a, 254u);
}

/* ---- Teilen ----------------------------------------------------------- */

osk_result_t osk_shamir_split(const uint8_t secret[OSK_SHARE_SECRET], uint8_t k, uint8_t n,
			      osk_share_t *out)
{
	uint8_t coeff[OSK_SHARE_SECRET][OSK_SHARE_MAX_N];
	uint8_t digest[OSK_HASH_LEN];
	osk_result_t r;

	if (secret == NULL || out == NULL || k < OSK_SHARE_MIN_K || k > n || n > OSK_SHARE_MAX_N) {
		return OSK_ERR_ARG;
	}

	/* Die Zufallsglieder. Sie muessen wirklich zufaellig sein - sonst
	 * liesse sich aus einem Teil auf das Geheimnis schliessen. */
	for (uint8_t j = 1; j < k; j++) {
		for (size_t i = 0; i < OSK_SHARE_SECRET; i++) {
			r = osk_crypto_random(&coeff[i][j], 1);
			if (r != OSK_OK) {
				osk_crypto_wipe(coeff, sizeof(coeff));
				return r;
			}
		}
	}
	for (size_t i = 0; i < OSK_SHARE_SECRET; i++) {
		coeff[i][0] = secret[i];
	}

	r = osk_crypto_sha256(secret, OSK_SHARE_SECRET, digest);
	if (r != OSK_OK) {
		osk_crypto_wipe(coeff, sizeof(coeff));
		return r;
	}

	for (uint8_t s = 0; s < n; s++) {
		uint8_t x = (uint8_t)(s + 1u); /* Stelle 0 ist das Geheimnis */

		memset(&out[s], 0, sizeof(out[s]));
		out[s].version = 1u;
		out[s].k = k;
		out[s].n = n;
		out[s].index = x;
		memcpy(out[s].digest, digest, OSK_SHARE_DIGEST);

		for (size_t i = 0; i < OSK_SHARE_SECRET; i++) {
			uint8_t acc = 0;

			/* Horner: von oben nach unten einsetzen. */
			for (int j = (int)k - 1; j >= 0; j--) {
				acc = (uint8_t)(gf_mul(acc, x) ^ coeff[i][j]);
			}
			out[s].y[i] = acc;
		}
	}
	osk_crypto_wipe(coeff, sizeof(coeff));
	osk_crypto_wipe(digest, sizeof(digest));
	return OSK_OK;
}

/* ---- Zusammensetzen --------------------------------------------------- */

osk_result_t osk_shamir_combine(const osk_share_t *shares, size_t count,
				uint8_t secret_out[OSK_SHARE_SECRET])
{
	uint8_t digest[OSK_HASH_LEN];
	uint8_t k = 0;

	if (shares == NULL || secret_out == NULL || count == 0u || count > OSK_SHARE_MAX_N) {
		return OSK_ERR_ARG;
	}

	k = shares[0].k;
	if (k < OSK_SHARE_MIN_K) {
		return OSK_ERR_FORMAT;
	}
	for (size_t i = 0; i < count; i++) {
		if (shares[i].version != 1u || shares[i].k != k || shares[i].index == 0u) {
			return OSK_ERR_FORMAT;
		}
		for (size_t j = 0; j < i; j++) {
			/* Zweimal derselbe Umschlag ist kein zweiter Teil. */
			if (shares[j].index == shares[i].index) {
				return OSK_ERR_ARG;
			}
		}
	}
	if (count < k) {
		return OSK_ERR_DENIED;
	}

	/* Lagrange an der Stelle 0: jeder Teil wird mit dem Produkt der
	 * uebrigen Stellen gewichtet. */
	memset(secret_out, 0, OSK_SHARE_SECRET);
	for (size_t i = 0; i < k; i++) {
		uint8_t num = 1, den = 1;

		for (size_t j = 0; j < k; j++) {
			if (j == i) {
				continue;
			}
			num = gf_mul(num, shares[j].index);
			den = gf_mul(den, (uint8_t)(shares[i].index ^ shares[j].index));
		}
		uint8_t w = gf_mul(num, gf_inv(den));

		for (size_t b = 0; b < OSK_SHARE_SECRET; b++) {
			secret_out[b] ^= gf_mul(shares[i].y[b], w);
		}
	}

	/* Die Probe. Ohne sie wuerde ein Tippfehler im Umschlag ein falsches
	 * Geheimnis ergeben, mit dem niemand den Fehler bemerkt. */
	osk_result_t r = osk_crypto_sha256(secret_out, OSK_SHARE_SECRET, digest);

	if (r != OSK_OK) {
		osk_crypto_wipe(secret_out, OSK_SHARE_SECRET);
		return r;
	}
	if (!osk_crypto_equal(digest, shares[0].digest, OSK_SHARE_DIGEST)) {
		osk_crypto_wipe(secret_out, OSK_SHARE_SECRET);
		osk_crypto_wipe(digest, sizeof(digest));
		return OSK_ERR_DENIED;
	}
	osk_crypto_wipe(digest, sizeof(digest));
	return OSK_OK;
}

/* ---- Text fuer den Umschlag ------------------------------------------- */

static char hex_digit(uint8_t v)
{
	/* Beide Zweige ausdruecklich als int rechnen: sonst ist der eine int und
	 * der andere unsigned, und gcc 14 bricht mit -Werror=sign-compare ab. */
	return (char)((v < 10u) ? ('0' + (int)v) : ('A' + (int)v - 10));
}

static int hex_value(char c)
{
	if (c >= '0' && c <= '9') {
		return c - '0';
	}
	if (c >= 'A' && c <= 'F') {
		return c - 'A' + 10;
	}
	if (c >= 'a' && c <= 'f') {
		return c - 'a' + 10;
	}
	return -1;
}

osk_result_t osk_shamir_share_text(const osk_share_t *share, char *out, size_t cap)
{
	size_t pos = 0;

	if (share == NULL || out == NULL || cap < OSK_SHARE_TEXT_MAX) {
		return OSK_ERR_ARG;
	}
	/* Aufbau: OSK1-<k>-<n>-<index>-<32 Byte hex>-<4 Byte hex>
	 * Alles gross, nur Ziffern und A-F: so laesst es sich abschreiben. */
	const char *prefix = "OSK1-";

	while (*prefix != '\0') {
		out[pos++] = *prefix++;
	}
	out[pos++] = hex_digit(share->k);
	out[pos++] = '-';
	out[pos++] = hex_digit(share->n);
	out[pos++] = '-';
	out[pos++] = hex_digit((uint8_t)(share->index >> 4));
	out[pos++] = hex_digit((uint8_t)(share->index & 0x0Fu));
	out[pos++] = '-';
	for (size_t i = 0; i < OSK_SHARE_SECRET; i++) {
		out[pos++] = hex_digit((uint8_t)(share->y[i] >> 4));
		out[pos++] = hex_digit((uint8_t)(share->y[i] & 0x0Fu));
	}
	out[pos++] = '-';
	for (size_t i = 0; i < OSK_SHARE_DIGEST; i++) {
		out[pos++] = hex_digit((uint8_t)(share->digest[i] >> 4));
		out[pos++] = hex_digit((uint8_t)(share->digest[i] & 0x0Fu));
	}
	out[pos] = '\0';
	return OSK_OK;
}

/* Liest genau das, was osk_shamir_share_text geschrieben hat. Bindestriche
 * und Leerzeichen werden uebersprungen, Klein- und Grossschreibung ist
 * gleich - abgeschriebene Umschlaege sehen selten perfekt aus. */
osk_result_t osk_shamir_share_parse(const char *text, osk_share_t *out)
{
	uint8_t nib[2u + 2u + (OSK_SHARE_SECRET + OSK_SHARE_DIGEST) * 2u];
	size_t n = 0;

	if (text == NULL || out == NULL) {
		return OSK_ERR_ARG;
	}
	if (!(text[0] == 'O' || text[0] == 'o') || !(text[1] == 'S' || text[1] == 's') ||
	    !(text[2] == 'K' || text[2] == 'k') || text[3] != '1') {
		return OSK_ERR_FORMAT;
	}
	for (const char *p = text + 4; *p != '\0'; p++) {
		if (*p == '-' || *p == ' ') {
			continue;
		}
		int v = hex_value(*p);

		if (v < 0) {
			return OSK_ERR_FORMAT;
		}
		if (n >= sizeof(nib)) {
			return OSK_ERR_FORMAT;
		}
		nib[n++] = (uint8_t)v;
	}
	if (n != sizeof(nib)) {
		return OSK_ERR_FORMAT;
	}

	memset(out, 0, sizeof(*out));
	out->version = 1u;
	out->k = nib[0];
	out->n = nib[1];
	out->index = (uint8_t)((nib[2] << 4) | nib[3]);

	size_t pos = 4;

	for (size_t i = 0; i < OSK_SHARE_SECRET; i++) {
		out->y[i] = (uint8_t)((nib[pos] << 4) | nib[pos + 1]);
		pos += 2;
	}
	for (size_t i = 0; i < OSK_SHARE_DIGEST; i++) {
		out->digest[i] = (uint8_t)((nib[pos] << 4) | nib[pos + 1]);
		pos += 2;
	}
	if (out->index == 0u || out->k < OSK_SHARE_MIN_K) {
		return OSK_ERR_FORMAT;
	}
	return OSK_OK;
}
