/* CBOR, klein und streng. Siehe osk_cbor.h fuer die Regeln. */
#include <string.h>
#include "osk_cbor.h"

/* Liest einen Kopf (Haupttyp + Zusatzwert) und prueft die kleinste Kodierung.
 * Verbraucht nichts, auch nicht bei Erfolg - das macht der Aufrufer. */
static osk_result_t head_read(const osk_cbor_reader_t *r, uint8_t *major, uint64_t *val,
			      size_t *head_len)
{
	if (osk_cbor_left(r) < 1)
		return OSK_ERR_FORMAT;

	const uint8_t ib = r->buf[r->pos];
	const uint8_t mt = (uint8_t)(ib >> 5);
	const uint8_t ai = (uint8_t)(ib & 0x1f);
	uint64_t v = 0;
	size_t need = 1;

	if (ai < 24) {
		v = ai;
	} else if (ai == 24) {
		need = 2;
		if (osk_cbor_left(r) < need)
			return OSK_ERR_FORMAT;
		v = r->buf[r->pos + 1];
		/* kleinste Kodierung: 0..23 gehoeren in das erste Byte */
		if (v < 24)
			return OSK_ERR_FORMAT;
	} else if (ai == 25) {
		need = 3;
		if (osk_cbor_left(r) < need)
			return OSK_ERR_FORMAT;
		v = ((uint64_t)r->buf[r->pos + 1] << 8) | r->buf[r->pos + 2];
		if (v <= 0xff)
			return OSK_ERR_FORMAT;
	} else if (ai == 26) {
		need = 5;
		if (osk_cbor_left(r) < need)
			return OSK_ERR_FORMAT;
		v = ((uint64_t)r->buf[r->pos + 1] << 24) | ((uint64_t)r->buf[r->pos + 2] << 16) |
		    ((uint64_t)r->buf[r->pos + 3] << 8) | (uint64_t)r->buf[r->pos + 4];
		if (v <= 0xffff)
			return OSK_ERR_FORMAT;
	} else {
		/* 27 = 64 Bit, 28..30 ungueltig, 31 = unbestimmte Laenge.
		 * Nichts davon kommt in einer CTAP-Nachricht vor. */
		return OSK_ERR_FORMAT;
	}

	/* Haupttyp 6 (Tags) ist in CTAP verboten. */
	if (mt == 6)
		return OSK_ERR_FORMAT;
	/* Haupttyp 7: nur false, true und null (20, 21, 22). Keine Fliesskommazahlen. */
	if (mt == 7 && (ai < 20 || ai > 22))
		return OSK_ERR_FORMAT;

	*major = mt;
	*val = v;
	*head_len = need;
	return OSK_OK;
}

void osk_cbor_reader_init(osk_cbor_reader_t *r, const uint8_t *buf, size_t len)
{
	r->buf = buf;
	r->len = buf ? len : 0;
	r->pos = 0;
	r->depth = 0;
}

osk_result_t osk_cbor_peek_type(const osk_cbor_reader_t *r, osk_cbor_type_t *out)
{
	uint8_t mt;
	uint64_t v;
	size_t hl;
	const osk_result_t rc = head_read(r, &mt, &v, &hl);
	if (rc != OSK_OK)
		return rc;
	*out = (osk_cbor_type_t)mt;
	return OSK_OK;
}

osk_result_t osk_cbor_read_uint(osk_cbor_reader_t *r, uint64_t *out)
{
	uint8_t mt;
	uint64_t v;
	size_t hl;
	const osk_result_t rc = head_read(r, &mt, &v, &hl);
	if (rc != OSK_OK)
		return rc;
	if (mt != OSK_CBOR_UINT)
		return OSK_ERR_FORMAT;
	r->pos += hl;
	*out = v;
	return OSK_OK;
}

osk_result_t osk_cbor_read_int(osk_cbor_reader_t *r, int64_t *out)
{
	uint8_t mt;
	uint64_t v;
	size_t hl;
	const osk_result_t rc = head_read(r, &mt, &v, &hl);
	if (rc != OSK_OK)
		return rc;
	if (v > (uint64_t)INT64_MAX)
		return OSK_ERR_FORMAT;
	if (mt == OSK_CBOR_UINT)
		*out = (int64_t)v;
	else if (mt == OSK_CBOR_NINT)
		*out = -1 - (int64_t)v;
	else
		return OSK_ERR_FORMAT;
	r->pos += hl;
	return OSK_OK;
}

/* Gemeinsamer Weg fuer Byte- und Zeichenfolgen: Laenge erst pruefen, dann lesen. */
static osk_result_t read_string(osk_cbor_reader_t *r, uint8_t want_mt, const uint8_t **out,
				size_t *len_out)
{
	uint8_t mt;
	uint64_t v;
	size_t hl;
	const osk_result_t rc = head_read(r, &mt, &v, &hl);
	if (rc != OSK_OK)
		return rc;
	if (mt != want_mt)
		return OSK_ERR_FORMAT;
	if (v > osk_cbor_left(r) - hl)
		return OSK_ERR_FORMAT;
	*out = r->buf + r->pos + hl;
	*len_out = (size_t)v;
	r->pos += hl + (size_t)v;
	return OSK_OK;
}

osk_result_t osk_cbor_read_bytes(osk_cbor_reader_t *r, const uint8_t **out, size_t *len_out)
{
	return read_string(r, OSK_CBOR_BSTR, out, len_out);
}

osk_result_t osk_cbor_read_text(osk_cbor_reader_t *r, const char **out, size_t *len_out)
{
	const uint8_t *p = NULL;
	const osk_result_t rc = read_string(r, OSK_CBOR_TSTR, &p, len_out);
	if (rc == OSK_OK)
		*out = (const char *)p;
	return rc;
}

osk_result_t osk_cbor_read_bool(osk_cbor_reader_t *r, bool *out)
{
	uint8_t mt;
	uint64_t v;
	size_t hl;
	const osk_result_t rc = head_read(r, &mt, &v, &hl);
	if (rc != OSK_OK)
		return rc;
	if (mt != OSK_CBOR_SIMPLE || (v != 20 && v != 21))
		return OSK_ERR_FORMAT;
	r->pos += hl;
	*out = (v == 21);
	return OSK_OK;
}

static osk_result_t enter(osk_cbor_reader_t *r, uint8_t want_mt, size_t *count_out)
{
	uint8_t mt;
	uint64_t v;
	size_t hl;

	if (r->depth >= OSK_CBOR_MAX_DEPTH)
		return OSK_ERR_FORMAT;

	const osk_result_t rc = head_read(r, &mt, &v, &hl);
	if (rc != OSK_OK)
		return rc;
	if (mt != want_mt)
		return OSK_ERR_FORMAT;
	if (v > OSK_CBOR_MAX_ITEMS)
		return OSK_ERR_FORMAT;
	/* Jedes Element braucht mindestens ein Byte - eine Liste, die laenger
	 * behauptet zu sein als der Puffer, wird sofort abgewiesen. */
	const size_t per_item = (want_mt == OSK_CBOR_MAP) ? 2u : 1u;
	if (v * per_item > osk_cbor_left(r) - hl)
		return OSK_ERR_FORMAT;

	r->pos += hl;
	r->depth++;
	*count_out = (size_t)v;
	return OSK_OK;
}

osk_result_t osk_cbor_enter_array(osk_cbor_reader_t *r, size_t *count_out)
{
	return enter(r, OSK_CBOR_ARRAY, count_out);
}

osk_result_t osk_cbor_enter_map(osk_cbor_reader_t *r, size_t *count_out)
{
	return enter(r, OSK_CBOR_MAP, count_out);
}

void osk_cbor_leave(osk_cbor_reader_t *r)
{
	if (r->depth > 0)
		r->depth--;
}

osk_result_t osk_cbor_skip(osk_cbor_reader_t *r)
{
	uint8_t mt;
	uint64_t v;
	size_t hl;
	osk_result_t rc = head_read(r, &mt, &v, &hl);
	if (rc != OSK_OK)
		return rc;

	switch (mt) {
	case OSK_CBOR_UINT:
	case OSK_CBOR_NINT:
	case OSK_CBOR_SIMPLE:
		r->pos += hl;
		return OSK_OK;
	case OSK_CBOR_BSTR:
	case OSK_CBOR_TSTR:
		if (v > osk_cbor_left(r) - hl)
			return OSK_ERR_FORMAT;
		r->pos += hl + (size_t)v;
		return OSK_OK;
	case OSK_CBOR_ARRAY:
	case OSK_CBOR_MAP: {
		size_t count = 0;
		rc = enter(r, mt, &count);
		if (rc != OSK_OK)
			return rc;
		const size_t items = (mt == OSK_CBOR_MAP) ? count * 2u : count;
		for (size_t i = 0; i < items; i++) {
			rc = osk_cbor_skip(r);
			if (rc != OSK_OK) {
				osk_cbor_leave(r);
				return rc;
			}
		}
		osk_cbor_leave(r);
		return OSK_OK;
	}
	default:
		return OSK_ERR_FORMAT;
	}
}

int osk_cbor_key_cmp(const uint8_t *a, size_t alen, const uint8_t *b, size_t blen)
{
	/* Erst der Haupttyp, dann die Laenge, dann byteweise - genau die
	 * Reihenfolge aus CTAP 2.2 Abschnitt 8. */
	const uint8_t ma = (uint8_t)(a[0] >> 5);
	const uint8_t mb = (uint8_t)(b[0] >> 5);
	if (ma != mb)
		return (ma < mb) ? -1 : 1;
	if (alen != blen)
		return (alen < blen) ? -1 : 1;
	return memcmp(a, b, alen);
}

/* Prueft einen Wert und, bei Abbildungen, Sortierung und Eindeutigkeit der
 * Schluessel. Das Gutschein-Konto (budget) begrenzt die Arbeit, damit eine
 * tiefe, breite Nachricht nicht beliebig Rechenzeit kostet. */
static osk_result_t validate_value(osk_cbor_reader_t *r, size_t *budget)
{
	if (*budget == 0)
		return OSK_ERR_FORMAT;
	(*budget)--;

	osk_cbor_type_t t;
	osk_result_t rc = osk_cbor_peek_type(r, &t);
	if (rc != OSK_OK)
		return rc;

	if (t == OSK_CBOR_MAP) {
		size_t count = 0;
		rc = osk_cbor_enter_map(r, &count);
		if (rc != OSK_OK)
			return rc;
		size_t prev_at = 0, prev_len = 0;
		for (size_t i = 0; i < count; i++) {
			const size_t key_at = r->pos;
			rc = validate_value(r, budget);
			if (rc != OSK_OK)
				break;
			const size_t key_len = r->pos - key_at;
			if (i > 0 && osk_cbor_key_cmp(r->buf + prev_at, prev_len, r->buf + key_at,
						      key_len) >= 0) {
				rc = OSK_ERR_FORMAT; /* unsortiert oder doppelt */
				break;
			}
			prev_at = key_at;
			prev_len = key_len;
			rc = validate_value(r, budget);
			if (rc != OSK_OK)
				break;
		}
		osk_cbor_leave(r);
		return rc;
	}

	if (t == OSK_CBOR_ARRAY) {
		size_t count = 0;
		rc = osk_cbor_enter_array(r, &count);
		if (rc != OSK_OK)
			return rc;
		for (size_t i = 0; i < count; i++) {
			rc = validate_value(r, budget);
			if (rc != OSK_OK)
				break;
		}
		osk_cbor_leave(r);
		return rc;
	}

	return osk_cbor_skip(r);
}

osk_result_t osk_cbor_validate(const uint8_t *buf, size_t len)
{
	if (!buf || len == 0)
		return OSK_ERR_FORMAT;

	osk_cbor_reader_t r;
	osk_cbor_reader_init(&r, buf, len);
	size_t budget = OSK_CBOR_MAX_ITEMS * 4u;
	const osk_result_t rc = validate_value(&r, &budget);
	if (rc != OSK_OK)
		return rc;
	/* Nachlaufende Bytes sind ein Fehler, nicht Beiwerk. */
	return (r.pos == len) ? OSK_OK : OSK_ERR_FORMAT;
}

/* ---- Schreiber ---------------------------------------------------------- */

void osk_cbor_writer_init(osk_cbor_writer_t *w, uint8_t *buf, size_t cap)
{
	w->buf = buf;
	w->cap = buf ? cap : 0;
	w->len = 0;
	w->failed = (buf == NULL);
}

bool osk_cbor_writer_ok(const osk_cbor_writer_t *w)
{
	return !w->failed;
}

static void put_raw(osk_cbor_writer_t *w, const uint8_t *p, size_t len)
{
	if (w->failed)
		return;
	if (len > w->cap - w->len) {
		w->failed = true;
		return;
	}
	if (len > 0)
		memcpy(w->buf + w->len, p, len);
	w->len += len;
}

static void put_head(osk_cbor_writer_t *w, uint8_t major, uint64_t v)
{
	uint8_t h[5];
	size_t n;

	if (v < 24) {
		h[0] = (uint8_t)((major << 5) | (uint8_t)v);
		n = 1;
	} else if (v <= 0xff) {
		h[0] = (uint8_t)((major << 5) | 24);
		h[1] = (uint8_t)v;
		n = 2;
	} else if (v <= 0xffff) {
		h[0] = (uint8_t)((major << 5) | 25);
		h[1] = (uint8_t)(v >> 8);
		h[2] = (uint8_t)v;
		n = 3;
	} else if (v <= 0xffffffffu) {
		h[0] = (uint8_t)((major << 5) | 26);
		h[1] = (uint8_t)(v >> 24);
		h[2] = (uint8_t)(v >> 16);
		h[3] = (uint8_t)(v >> 8);
		h[4] = (uint8_t)v;
		n = 5;
	} else {
		/* 64-Bit-Werte kommen in unseren Antworten nicht vor. */
		w->failed = true;
		return;
	}
	put_raw(w, h, n);
}

void osk_cbor_put_uint(osk_cbor_writer_t *w, uint64_t v)
{
	put_head(w, OSK_CBOR_UINT, v);
}

void osk_cbor_put_int(osk_cbor_writer_t *w, int64_t v)
{
	if (v < 0)
		put_head(w, OSK_CBOR_NINT, (uint64_t)(-1 - v));
	else
		put_head(w, OSK_CBOR_UINT, (uint64_t)v);
}

void osk_cbor_put_bytes(osk_cbor_writer_t *w, const uint8_t *p, size_t len)
{
	put_head(w, OSK_CBOR_BSTR, len);
	put_raw(w, p, len);
}

void osk_cbor_put_text_len(osk_cbor_writer_t *w, const char *s, size_t len)
{
	put_head(w, OSK_CBOR_TSTR, len);
	put_raw(w, (const uint8_t *)s, len);
}

void osk_cbor_put_text(osk_cbor_writer_t *w, const char *s)
{
	osk_cbor_put_text_len(w, s, strlen(s));
}

void osk_cbor_put_bool(osk_cbor_writer_t *w, bool v)
{
	put_head(w, OSK_CBOR_SIMPLE, v ? 21u : 20u);
}

void osk_cbor_put_array(osk_cbor_writer_t *w, size_t count)
{
	put_head(w, OSK_CBOR_ARRAY, count);
}

void osk_cbor_put_map(osk_cbor_writer_t *w, size_t count)
{
	put_head(w, OSK_CBOR_MAP, count);
}
