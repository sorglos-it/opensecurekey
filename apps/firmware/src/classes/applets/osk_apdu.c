/* APDU und BER-TLV. Siehe osk_apdu.h.
 * Grundlage: ISO/IEC 7816-4, Abschnitte 5.1 (Kommando) und 5.2 (TLV). */
#include <string.h>
#include "osk_apdu.h"

/* Alles Lesen laeuft ueber diesen Zeiger: `left` ist immer die Zahl der noch
 * vorhandenen Bytes, und jede Entnahme prueft sie vorher. */
typedef struct {
	const uint8_t *p;
	size_t left;
} cursor_t;

static bool take(cursor_t *c, size_t n, const uint8_t **out)
{
	if (c->left < n)
		return false;
	if (out != NULL)
		*out = c->p;
	c->p += n;
	c->left -= n;
	return true;
}

static bool take_u8(cursor_t *c, uint8_t *out)
{
	const uint8_t *p = NULL;

	if (!take(c, 1u, &p))
		return false;
	*out = *p;
	return true;
}

static bool take_u16(cursor_t *c, uint16_t *out)
{
	const uint8_t *p = NULL;

	if (!take(c, 2u, &p))
		return false;
	*out = (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
	return true;
}

osk_result_t osk_apdu_parse(const uint8_t *buf, size_t len, osk_apdu_t *out)
{
	cursor_t c;
	osk_apdu_t a;
	uint8_t b = 0;

	if (buf == NULL || out == NULL)
		return OSK_ERR_ARG;
	if (len < 4u)
		return OSK_ERR_FORMAT;

	c.p = buf;
	c.left = len;
	memset(&a, 0, sizeof(a));

	/* Der Kopf ist immer da - vier Bytes haben wir gerade geprueft. */
	(void)take_u8(&c, &a.cla);
	(void)take_u8(&c, &a.ins);
	(void)take_u8(&c, &a.p1);
	(void)take_u8(&c, &a.p2);

	/* CLA: Bit 5 (0x10) kuendigt Verkettung an, Bit 0 und 1 waehlen den
	 * logischen Kanal. 0xFF ist nach 7816-4 nie gueltig. */
	if (a.cla == 0xffu)
		return OSK_ERR_FORMAT;
	a.chained = (a.cla & 0x10u) != 0u;
	a.channel = (uint8_t)(a.cla & 0x03u);

	if (c.left == 0u) {
		/* Fall 1: nur der Kopf. */
		*out = a;
		return OSK_OK;
	}

	if (!take_u8(&c, &b))
		return OSK_ERR_FORMAT;

	if (b != 0u) {
		/* Kurze Laengenfelder. */
		if (c.left == 0u) {
			/* Fall 2S: das Byte war Le. 0x00 waere 256 gewesen,
			 * aber das ist hier ausgeschlossen. */
			a.has_le = true;
			a.le = b;
			*out = a;
			return OSK_OK;
		}
		a.lc = b;
		if (a.lc > OSK_APDU_MAX_DATA)
			return OSK_ERR_FULL;
		if (!take(&c, a.lc, &a.data))
			return OSK_ERR_FORMAT;
		if (c.left == 0u) {
			/* Fall 3S */
			*out = a;
			return OSK_OK;
		}
		/* Fall 4S: ein einzelnes Le-Byte, sonst nichts. */
		if (!take_u8(&c, &b))
			return OSK_ERR_FORMAT;
		if (c.left != 0u)
			return OSK_ERR_FORMAT;
		a.has_le = true;
		a.le = (b == 0u) ? 256u : b;
		*out = a;
		return OSK_OK;
	}

	/* Das Byte war 0x00. Entweder Fall 2S mit Le = 256, oder der Anfang
	 * eines erweiterten Laengenfeldes. */
	if (c.left == 0u) {
		a.has_le = true;
		a.le = 256u;
		*out = a;
		return OSK_OK;
	}

	a.extended = true;
	if (c.left == 2u) {
		/* Fall 2E: nur Le. 0x0000 heisst 65536, mehr als wir annehmen. */
		uint16_t le16 = 0;

		(void)take_u16(&c, &le16);
		a.has_le = true;
		a.le = (le16 == 0u) ? 65536u : (size_t)le16;
		if (a.le > OSK_APDU_MAX_RESP)
			a.le = OSK_APDU_MAX_RESP;
		*out = a;
		return OSK_OK;
	}

	{
		uint16_t lc16 = 0;

		if (!take_u16(&c, &lc16))
			return OSK_ERR_FORMAT;
		if (lc16 == 0u)
			return OSK_ERR_FORMAT; /* Lc = 0 gibt es nicht */
		a.lc = lc16;
		if (a.lc > OSK_APDU_MAX_DATA)
			return OSK_ERR_FULL;
		if (!take(&c, a.lc, &a.data))
			return OSK_ERR_FORMAT;
	}

	if (c.left == 0u) {
		/* Fall 3E */
		*out = a;
		return OSK_OK;
	}
	{
		uint16_t le16 = 0;

		if (!take_u16(&c, &le16))
			return OSK_ERR_FORMAT;
		if (c.left != 0u)
			return OSK_ERR_FORMAT;
		a.has_le = true;
		a.le = (le16 == 0u) ? 65536u : (size_t)le16;
		if (a.le > OSK_APDU_MAX_RESP)
			a.le = OSK_APDU_MAX_RESP;
	}
	*out = a;
	return OSK_OK;
}

/* --- Antwort zusammenbauen ---------------------------------------------- */

void osk_resp_init(osk_resp_t *r, uint8_t *buf, size_t cap)
{
	r->buf = buf;
	r->cap = (buf == NULL) ? 0u : cap;
	r->len = 0u;
	r->overflow = (buf == NULL);
}

void osk_resp_u8(osk_resp_t *r, uint8_t b)
{
	if (r->overflow)
		return;
	if (r->len + 1u > r->cap) {
		r->overflow = true;
		return;
	}
	r->buf[r->len++] = b;
}

void osk_resp_bytes(osk_resp_t *r, const uint8_t *p, size_t len)
{
	if (r->overflow)
		return;
	if (len == 0u)
		return;
	if (p == NULL || r->len + len > r->cap) {
		r->overflow = true;
		return;
	}
	memcpy(r->buf + r->len, p, len);
	r->len += len;
}

/* Tag ausgeben: fuehrende Nullbytes weglassen, hoechstens drei Bytes. */
static void put_tag(osk_resp_t *r, uint32_t tag)
{
	if ((tag & 0xff0000u) != 0u)
		osk_resp_u8(r, (uint8_t)(tag >> 16));
	if ((tag & 0xffff00u) != 0u)
		osk_resp_u8(r, (uint8_t)(tag >> 8));
	osk_resp_u8(r, (uint8_t)tag);
}

/* Laenge nach BER: unter 128 ein Byte, sonst 0x81/0x82 mit Folgebytes. */
static size_t len_bytes(size_t len)
{
	if (len < 0x80u)
		return 1u;
	if (len <= 0xffu)
		return 2u;
	return 3u;
}

static void put_len(osk_resp_t *r, size_t len)
{
	if (len < 0x80u) {
		osk_resp_u8(r, (uint8_t)len);
	} else if (len <= 0xffu) {
		osk_resp_u8(r, 0x81u);
		osk_resp_u8(r, (uint8_t)len);
	} else {
		osk_resp_u8(r, 0x82u);
		osk_resp_u8(r, (uint8_t)(len >> 8));
		osk_resp_u8(r, (uint8_t)len);
	}
}

void osk_resp_tlv(osk_resp_t *r, uint32_t tag, const uint8_t *val, size_t len)
{
	put_tag(r, tag);
	put_len(r, len);
	osk_resp_bytes(r, val, len);
}

void osk_resp_tlv_begin(osk_resp_t *r, uint32_t tag, osk_tlv_mark_t *mark)
{
	mark->at = r->len;
	put_tag(r, tag);
	/* Platzhalter fuer ein kurzes Laengenfeld. Wird in `end` erweitert,
	 * wenn der Inhalt groesser wurde. */
	mark->hdr = r->len - mark->at + 1u;
	osk_resp_u8(r, 0u);
}

void osk_resp_tlv_end(osk_resp_t *r, const osk_tlv_mark_t *mark)
{
	size_t body_at;
	size_t body_len;
	size_t need;
	size_t grow;

	if (r->overflow)
		return;
	if (mark->at + mark->hdr > r->len) {
		r->overflow = true;
		return;
	}
	body_at = mark->at + mark->hdr;
	body_len = r->len - body_at;
	need = len_bytes(body_len);
	grow = need - 1u; /* der Platzhalter war ein Byte lang */

	if (grow != 0u) {
		if (r->len + grow > r->cap) {
			r->overflow = true;
			return;
		}
		memmove(r->buf + body_at + grow, r->buf + body_at, body_len);
		r->len += grow;
	}
	{
		/* Laengenfeld an die gemerkte Stelle schreiben. */
		osk_resp_t tmp;

		tmp.buf = r->buf;
		tmp.cap = r->cap;
		tmp.len = body_at - 1u;
		tmp.overflow = false;
		put_len(&tmp, body_len);
		if (tmp.overflow)
			r->overflow = true;
	}
}

/* --- BER-TLV lesen ------------------------------------------------------ */

bool osk_tlv_first(const uint8_t *buf, size_t len, uint32_t *tag_out, const uint8_t **val_out,
		   size_t *len_out, size_t *used_out)
{
	cursor_t c;
	uint32_t tag = 0;
	uint8_t b = 0;
	size_t vlen = 0;
	const uint8_t *val = NULL;

	if (buf == NULL || len == 0u)
		return false;
	c.p = buf;
	c.left = len;

	if (!take_u8(&c, &b))
		return false;
	tag = b;
	/* Mehrbyte-Tag: untere fuenf Bits alle gesetzt. Folgebytes haben Bit 7
	 * gesetzt, solange es weitergeht. Wir lassen hoechstens drei Bytes zu. */
	if ((b & 0x1fu) == 0x1fu) {
		unsigned n = 0;

		do {
			if (!take_u8(&c, &b))
				return false;
			tag = (tag << 8) | (uint32_t)b;
			n++;
			if (n > 2u)
				return false;
		} while ((b & 0x80u) != 0u);
	}

	if (!take_u8(&c, &b))
		return false;
	if (b < 0x80u) {
		vlen = b;
	} else if (b == 0x81u) {
		if (!take_u8(&c, &b))
			return false;
		vlen = b;
	} else if (b == 0x82u) {
		uint16_t v = 0;

		if (!take_u16(&c, &v))
			return false;
		vlen = v;
	} else {
		return false; /* laengere Laengenfelder nehmen wir nicht an */
	}

	if (!take(&c, vlen, &val))
		return false;

	if (tag_out != NULL)
		*tag_out = tag;
	if (val_out != NULL)
		*val_out = val;
	if (len_out != NULL)
		*len_out = vlen;
	if (used_out != NULL)
		*used_out = len - c.left;
	return true;
}

bool osk_tlv_find(const uint8_t *buf, size_t len, uint32_t tag, const uint8_t **val_out,
		  size_t *len_out)
{
	size_t at = 0;

	while (at < len) {
		uint32_t got = 0;
		const uint8_t *val = NULL;
		size_t vlen = 0;
		size_t used = 0;

		if (!osk_tlv_first(buf + at, len - at, &got, &val, &vlen, &used))
			return false;
		if (used == 0u)
			return false;
		if (got == tag) {
			if (val_out != NULL)
				*val_out = val;
			if (len_out != NULL)
				*len_out = vlen;
			return true;
		}
		at += used;
	}
	return false;
}
