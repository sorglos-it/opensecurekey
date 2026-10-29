/* DER lesen und schreiben, so viel wie RFC 3161 braucht.
 *
 * Die Leseseite nimmt Daten vom Draht. Jede der drei Stellen, an denen
 * eine Laenge aus den Daten kommt, prueft sie vorher gegen den Rest des
 * Puffers: der Kopf selbst, die Langform der Laenge, und der Inhalt. Eine
 * dieser drei zu vergessen ist der uebliche Fehler in DER-Parsern. */
#include <string.h>
#include "authz_der.h"

/* ---------------------------------------------------------------- Lesen */

osk_result_t authz_der_next(authz_der_t *c, uint8_t *tag_out, const uint8_t **val_out,
			    size_t *vlen_out)
{
	if (c == NULL || c->p == NULL) {
		return OSK_ERR_ARG;
	}
	/* Typ und erstes Laengenbyte: zwei Byte muessen da sein. */
	if (c->len < 2u) {
		return OSK_ERR_FORMAT;
	}

	const uint8_t tag = c->p[0];
	size_t off = 2u;
	size_t vlen;
	const uint8_t l0 = c->p[1];

	if ((l0 & 0x80u) == 0u) {
		vlen = l0;
	} else {
		const size_t nbytes = (size_t)(l0 & 0x7Fu);
		/* Unbestimmte Laenge gibt es in DER nicht, nur in BER. Sie
		 * hier zu erlauben hiesse, auf ein Ende-Zeichen zu warten,
		 * das ein Angreifer weglassen kann. */
		if (nbytes == 0u || nbytes > 4u) {
			return OSK_ERR_FORMAT;
		}
		if (c->len < 2u + nbytes) {
			return OSK_ERR_FORMAT;
		}
		/* Fuehrendes Nullbyte in der Langform ist in DER nicht
		 * erlaubt: dieselbe Laenge waere dann zweimal darstellbar. */
		if (c->p[2] == 0x00u) {
			return OSK_ERR_FORMAT;
		}
		vlen = 0u;
		for (size_t i = 0u; i < nbytes; i++) {
			vlen = (vlen << 8) | (size_t)c->p[2u + i];
		}
		off = 2u + nbytes;
		/* Kurzform waere moeglich gewesen - in DER ist dann die
		 * Langform falsch. */
		if (vlen < 0x80u) {
			return OSK_ERR_FORMAT;
		}
	}

	if (vlen > c->len - off) {
		return OSK_ERR_FORMAT;
	}

	if (tag_out != NULL) {
		*tag_out = tag;
	}
	if (val_out != NULL) {
		*val_out = c->p + off;
	}
	if (vlen_out != NULL) {
		*vlen_out = vlen;
	}
	c->p += off + vlen;
	c->len -= off + vlen;
	return OSK_OK;
}

osk_result_t authz_der_expect(authz_der_t *c, uint8_t tag, const uint8_t **val_out,
			      size_t *vlen_out)
{
	uint8_t got = 0u;
	authz_der_t save = *c;

	osk_result_t r = authz_der_next(c, &got, val_out, vlen_out);
	if (r != OSK_OK) {
		return r;
	}
	if (got != tag) {
		/* Zurueckstellen: der Aufrufer darf einen Fehlschlag als
		 * "das Feld ist nicht da" lesen, weil in RFC 3161 mehrere
		 * Felder wahlfrei sind. Ohne das Zuruecksetzen waere das
		 * wahlfreie Feld verschluckt. */
		*c = save;
		return OSK_ERR_FORMAT;
	}
	return OSK_OK;
}

osk_result_t authz_der_enter(authz_der_t *c, uint8_t tag, authz_der_t *inner_out)
{
	const uint8_t *val = NULL;
	size_t vlen = 0u;

	if (inner_out == NULL) {
		return OSK_ERR_ARG;
	}
	osk_result_t r = authz_der_expect(c, tag, &val, &vlen);
	if (r != OSK_OK) {
		return r;
	}
	inner_out->p = val;
	inner_out->len = vlen;
	return OSK_OK;
}

osk_result_t authz_der_u64(const uint8_t *val, size_t vlen, uint64_t *out)
{
	if (val == NULL || out == NULL) {
		return OSK_ERR_ARG;
	}
	if (vlen == 0u) {
		return OSK_ERR_FORMAT;
	}
	/* Negative Zahlen gibt es in diesen Feldern nicht. Eine mit
	 * gesetztem Vorzeichenbit als grosse vorzeichenlose zu lesen waere
	 * genau die stille Umdeutung, die hier nichts verloren hat. */
	if ((val[0] & 0x80u) != 0u) {
		return OSK_ERR_FORMAT;
	}
	/* Fuehrendes Nullbyte ist nur erlaubt, wenn das naechste Byte das
	 * hoechste Bit gesetzt hat - sonst ist die Zahl nicht minimal
	 * kodiert. */
	if (vlen > 1u && val[0] == 0x00u && (val[1] & 0x80u) == 0u) {
		return OSK_ERR_FORMAT;
	}
	size_t start = (vlen > 1u && val[0] == 0x00u) ? 1u : 0u;
	if (vlen - start > 8u) {
		return OSK_ERR_FORMAT;
	}
	uint64_t v = 0u;
	for (size_t i = start; i < vlen; i++) {
		v = (v << 8) | (uint64_t)val[i];
	}
	*out = v;
	return OSK_OK;
}

/* ------------------------------------------------------------ Schreiben */

void authz_der_w_init(authz_der_w_t *w, uint8_t *buf, size_t cap)
{
	w->buf = buf;
	w->cap = cap;
	w->used = 0u;
	w->overflow = false;
}

/* Die einzige Stelle, an der der Puffer waechst. Darum steht die
 * Platzpruefung nur hier - jede andere Schreibfunktion geht durch sie. */
static uint8_t *reserve(authz_der_w_t *w, size_t n)
{
	if (w->overflow) {
		return NULL;
	}
	if (n > w->cap - w->used) {
		w->overflow = true;
		return NULL;
	}
	w->used += n;
	return w->buf + (w->cap - w->used);
}

void authz_der_w_bytes(authz_der_w_t *w, const uint8_t *p, size_t len)
{
	uint8_t *dst = reserve(w, len);

	if (dst != NULL) {
		memcpy(dst, p, len);
	}
}

void authz_der_w_header(authz_der_w_t *w, uint8_t tag, size_t content_len)
{
	if (content_len < 0x80u) {
		uint8_t *dst = reserve(w, 2u);
		if (dst != NULL) {
			dst[0] = tag;
			dst[1] = (uint8_t)content_len;
		}
		return;
	}

	/* So viele Byte, wie die Laenge wirklich braucht - nicht vier
	 * feste. Eine nicht minimale Laenge ist in DER falsch, und ein
	 * Pruefer, der streng liest, lehnt sie ab. */
	uint8_t tmp[4];
	size_t n = 0u;
	size_t v = content_len;

	while (v != 0u) {
		tmp[n++] = (uint8_t)(v & 0xFFu);
		v >>= 8;
	}

	uint8_t *dst = reserve(w, 2u + n);
	if (dst == NULL) {
		return;
	}
	dst[0] = tag;
	dst[1] = (uint8_t)(0x80u | n);
	for (size_t i = 0u; i < n; i++) {
		dst[2u + i] = tmp[n - 1u - i];
	}
}

void authz_der_w_tlv(authz_der_w_t *w, uint8_t tag, const uint8_t *val, size_t vlen)
{
	authz_der_w_bytes(w, val, vlen);
	authz_der_w_header(w, tag, vlen);
}

void authz_der_w_u64(authz_der_w_t *w, uint64_t v)
{
	uint8_t tmp[9];
	size_t n = 0u;

	do {
		tmp[n++] = (uint8_t)(v & 0xFFu);
		v >>= 8;
	} while (v != 0u);

	if ((tmp[n - 1u] & 0x80u) != 0u) {
		tmp[n++] = 0x00u;
	}

	uint8_t *dst = reserve(w, n);
	if (dst == NULL) {
		authz_der_w_header(w, DER_INTEGER, n);
		return;
	}
	for (size_t i = 0u; i < n; i++) {
		dst[i] = tmp[n - 1u - i];
	}
	authz_der_w_header(w, DER_INTEGER, n);
}

void authz_der_w_uint_bytes(authz_der_w_t *w, const uint8_t *p, size_t len)
{
	size_t start = 0u;

	while (start + 1u < len && p[start] == 0x00u) {
		start++;
	}
	const size_t body = len - start;
	const bool pad = (body > 0u) && ((p[start] & 0x80u) != 0u);

	authz_der_w_bytes(w, p + start, body);
	if (pad) {
		const uint8_t zero = 0x00u;
		authz_der_w_bytes(w, &zero, 1u);
	}
	authz_der_w_header(w, DER_INTEGER, body + (pad ? 1u : 0u));
}

void authz_der_w_bool(authz_der_w_t *w, bool v)
{
	/* DER kennt genau zwei Werte: 0x00 und 0xFF. Jedes andere Byte ist
	 * in BER wahr, in DER falsch kodiert. */
	const uint8_t b = v ? 0xFFu : 0x00u;

	authz_der_w_tlv(w, DER_BOOLEAN, &b, 1u);
}

osk_result_t authz_der_w_done(const authz_der_w_t *w, const uint8_t **start_out, size_t *len_out)
{
	if (w->overflow) {
		return OSK_ERR_FULL;
	}
	if (start_out != NULL) {
		*start_out = w->buf + (w->cap - w->used);
	}
	if (len_out != NULL) {
		*len_out = w->used;
	}
	return OSK_OK;
}
