#include <string.h>
#include "se050_tlv.h"

/* ------------------------------------------------------------- Schreiben */

void osk_tlv_init(osk_tlv_writer_t *w, uint8_t *buf, size_t cap)
{
	w->buf = buf;
	w->cap = (buf == NULL) ? 0 : cap;
	w->len = 0;
	w->overflow = (buf == NULL);
}

/* Laengenfeld nach AN12413 4.1.3.2: unter 0x80 ein Byte, sonst 0x82 und
 * zwei Byte. Mehr gibt der Baustein nicht her (APDU-Puffer 894 Byte). */
static size_t len_field_size(size_t val_len)
{
	return (val_len < 0x80u) ? 1u : 3u;
}

void osk_tlv_put(osk_tlv_writer_t *w, uint8_t tag, const uint8_t *val, size_t val_len)
{
	if (w->overflow) {
		return;
	}
	if (val_len > 0 && val == NULL) {
		w->overflow = true;
		return;
	}
	if (val_len > 0xFFFFu) {
		w->overflow = true;
		return;
	}

	const size_t need = 1u + len_field_size(val_len) + val_len;
	if (need > w->cap - w->len) {
		w->overflow = true;
		return;
	}

	uint8_t *p = &w->buf[w->len];
	*p++ = tag;
	if (val_len < 0x80u) {
		*p++ = (uint8_t)val_len;
	} else {
		*p++ = 0x82u;
		*p++ = (uint8_t)(val_len >> 8);
		*p++ = (uint8_t)(val_len & 0xFFu);
	}
	if (val_len > 0) {
		memcpy(p, val, val_len);
	}
	w->len += need;
}

void osk_tlv_put_u8(osk_tlv_writer_t *w, uint8_t tag, uint8_t val)
{
	osk_tlv_put(w, tag, &val, 1);
}

void osk_tlv_put_u16(osk_tlv_writer_t *w, uint8_t tag, uint16_t val)
{
	const uint8_t b[2] = {(uint8_t)(val >> 8), (uint8_t)(val & 0xFFu)};
	osk_tlv_put(w, tag, b, sizeof(b));
}

void osk_tlv_put_u32(osk_tlv_writer_t *w, uint8_t tag, uint32_t val)
{
	const uint8_t b[4] = {
	    (uint8_t)(val >> 24),
	    (uint8_t)(val >> 16),
	    (uint8_t)(val >> 8),
	    (uint8_t)(val & 0xFFu),
	};
	osk_tlv_put(w, tag, b, sizeof(b));
}

osk_result_t osk_tlv_done(const osk_tlv_writer_t *w, size_t *len_out)
{
	if (w->overflow) {
		return OSK_ERR_FULL;
	}
	if (len_out != NULL) {
		*len_out = w->len;
	}
	return OSK_OK;
}

/* ---------------------------------------------------------------- Lesen */

void osk_tlv_reader_init(osk_tlv_reader_t *r, const uint8_t *buf, size_t len)
{
	r->buf = buf;
	r->len = (buf == NULL) ? 0 : len;
}

osk_result_t osk_tlv_get(const osk_tlv_reader_t *r, uint8_t tag, const uint8_t **val,
			 size_t *val_len)
{
	if (r == NULL || val == NULL || val_len == NULL) {
		return OSK_ERR_ARG;
	}

	size_t pos = 0;
	while (pos < r->len) {
		const uint8_t t = r->buf[pos++];

		if (pos >= r->len) {
			return OSK_ERR_FORMAT;
		}
		const uint8_t l0 = r->buf[pos++];

		size_t n;
		if (l0 < 0x80u) {
			n = l0;
		} else if (l0 == 0x81u) {
			if (pos + 1 > r->len) {
				return OSK_ERR_FORMAT;
			}
			n = r->buf[pos];
			pos += 1;
		} else if (l0 == 0x82u) {
			if (pos + 2 > r->len) {
				return OSK_ERR_FORMAT;
			}
			n = ((size_t)r->buf[pos] << 8) | r->buf[pos + 1];
			pos += 2;
		} else {
			/* Laengenfeld laenger als drei Byte kommt nach
			 * AN12413 4.1.3.2 nicht vor. */
			return OSK_ERR_FORMAT;
		}

		if (n > r->len - pos) {
			return OSK_ERR_FORMAT;
		}

		if (t == tag) {
			*val = &r->buf[pos];
			*val_len = n;
			return OSK_OK;
		}
		pos += n;
	}
	return OSK_ERR_NOT_FOUND;
}

osk_result_t osk_tlv_copy(const osk_tlv_reader_t *r, uint8_t tag, uint8_t *out, size_t cap,
			  size_t *len_out)
{
	const uint8_t *val = NULL;
	size_t n = 0;

	const osk_result_t res = osk_tlv_get(r, tag, &val, &n);
	if (res != OSK_OK) {
		return res;
	}
	if (n > cap) {
		return OSK_ERR_FULL;
	}
	if (n > 0) {
		if (out == NULL) {
			return OSK_ERR_ARG;
		}
		memcpy(out, val, n);
	}
	if (len_out != NULL) {
		*len_out = n;
	}
	return OSK_OK;
}

osk_result_t osk_tlv_get_u8(const osk_tlv_reader_t *r, uint8_t tag, uint8_t *out)
{
	const uint8_t *val = NULL;
	size_t n = 0;

	const osk_result_t res = osk_tlv_get(r, tag, &val, &n);
	if (res != OSK_OK) {
		return res;
	}
	if (n != 1 || out == NULL) {
		return OSK_ERR_FORMAT;
	}
	*out = val[0];
	return OSK_OK;
}

osk_result_t osk_tlv_get_u16(const osk_tlv_reader_t *r, uint8_t tag, uint16_t *out)
{
	const uint8_t *val = NULL;
	size_t n = 0;

	const osk_result_t res = osk_tlv_get(r, tag, &val, &n);
	if (res != OSK_OK) {
		return res;
	}
	if (n != 2 || out == NULL) {
		return OSK_ERR_FORMAT;
	}
	*out = (uint16_t)(((uint16_t)val[0] << 8) | val[1]);
	return OSK_OK;
}
