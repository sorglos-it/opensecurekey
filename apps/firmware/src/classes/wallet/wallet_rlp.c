/* RLP, selbst geschrieben gegen das gelbe Papier (Anhang B). */

#include <string.h>
#include "wallet_rlp.h"

void osk_rlp_reader_init(osk_rlp_reader_t *r, const uint8_t *p, size_t len)
{
	r->p = p;
	r->len = len;
	r->pos = 0;
}

bool osk_rlp_at_end(const osk_rlp_reader_t *r)
{
	return r->pos >= r->len;
}

/* Liest eine Laengenangabe von `bytes` Byte. Die kuerzeste Form ist
 * Pflicht: fuehrende Null verboten, und der Wert muss ueber der Grenze der
 * kurzen Form liegen. */
static osk_result_t long_length(const uint8_t *p, size_t available, unsigned bytes,
				size_t min_value, size_t *out)
{
	size_t value = 0;

	if (bytes == 0 || bytes > 4u || available < bytes)
		return OSK_ERR_FORMAT;
	if (p[0] == 0x00u)
		return OSK_ERR_FORMAT; /* fuehrende Null in der Laenge */
	for (unsigned i = 0; i < bytes; i++)
		value = (value << 8) | p[i];
	if (value < min_value)
		return OSK_ERR_FORMAT; /* haette kurz geschrieben werden muessen */
	*out = value;
	return OSK_OK;
}

osk_result_t osk_rlp_next(osk_rlp_reader_t *r, osk_rlp_item_t *out)
{
	uint8_t head;
	size_t available;

	if (r == NULL || out == NULL)
		return OSK_ERR_ARG;
	if (r->pos >= r->len)
		return OSK_ERR_NOT_FOUND;
	head = r->p[r->pos];
	available = r->len - r->pos - 1u;

	if (head <= 0x7Fu) {
		out->kind = OSK_RLP_BYTES;
		out->data = r->p + r->pos;
		out->len = 1;
		r->pos += 1u;
		return OSK_OK;
	}
	if (head <= 0xB7u) {
		size_t len = (size_t)(head - 0x80u);

		if (available < len)
			return OSK_ERR_FORMAT;
		/* Ein einzelnes Byte unter 0x80 muss ohne Kopf geschrieben
		 * werden - 0x81 0x05 ist verboten. */
		if (len == 1u && r->p[r->pos + 1u] <= 0x7Fu)
			return OSK_ERR_FORMAT;
		out->kind = OSK_RLP_BYTES;
		out->data = r->p + r->pos + 1u;
		out->len = len;
		r->pos += 1u + len;
		return OSK_OK;
	}
	if (head <= 0xBFu) {
		unsigned bytes = (unsigned)(head - 0xB7u);
		size_t len = 0;
		osk_result_t res = long_length(r->p + r->pos + 1u, available, bytes, 56u, &len);

		if (res != OSK_OK)
			return res;
		if (available < (size_t)bytes + len)
			return OSK_ERR_FORMAT;
		out->kind = OSK_RLP_BYTES;
		out->data = r->p + r->pos + 1u + bytes;
		out->len = len;
		r->pos += 1u + bytes + len;
		return OSK_OK;
	}
	if (head <= 0xF7u) {
		size_t len = (size_t)(head - 0xC0u);

		if (available < len)
			return OSK_ERR_FORMAT;
		out->kind = OSK_RLP_LIST;
		out->data = r->p + r->pos + 1u;
		out->len = len;
		r->pos += 1u + len;
		return OSK_OK;
	}
	{
		unsigned bytes = (unsigned)(head - 0xF7u);
		size_t len = 0;
		osk_result_t res = long_length(r->p + r->pos + 1u, available, bytes, 56u, &len);

		if (res != OSK_OK)
			return res;
		if (available < (size_t)bytes + len)
			return OSK_ERR_FORMAT;
		out->kind = OSK_RLP_LIST;
		out->data = r->p + r->pos + 1u + bytes;
		out->len = len;
		r->pos += 1u + bytes + len;
		return OSK_OK;
	}
}

osk_result_t osk_rlp_as_u64(const osk_rlp_item_t *item, uint64_t *out)
{
	uint64_t value = 0;

	if (item == NULL || out == NULL)
		return OSK_ERR_ARG;
	if (item->kind != OSK_RLP_BYTES)
		return OSK_ERR_FORMAT;
	if (item->len > 8u)
		return OSK_ERR_FORMAT;
	if (item->len > 1u && item->data[0] == 0x00u)
		return OSK_ERR_FORMAT; /* fuehrende Null */
	if (item->len == 1u && item->data[0] == 0x00u)
		return OSK_ERR_FORMAT; /* die Null ist die leere Folge */
	for (size_t i = 0; i < item->len; i++)
		value = (value << 8) | item->data[i];
	*out = value;
	return OSK_OK;
}

osk_result_t osk_rlp_as_u256(const osk_rlp_item_t *item, uint8_t out[32])
{
	if (item == NULL || out == NULL)
		return OSK_ERR_ARG;
	if (item->kind != OSK_RLP_BYTES)
		return OSK_ERR_FORMAT;
	if (item->len > 32u)
		return OSK_ERR_FORMAT;
	if (item->len > 1u && item->data[0] == 0x00u)
		return OSK_ERR_FORMAT;
	if (item->len == 1u && item->data[0] == 0x00u)
		return OSK_ERR_FORMAT;
	memset(out, 0, 32);
	if (item->len > 0)
		memcpy(out + (32u - item->len), item->data, item->len);
	return OSK_OK;
}

size_t osk_rlp_write_u64(uint64_t value, uint8_t *out, size_t cap)
{
	uint8_t raw[8];
	size_t count = 0, first = 0;

	for (unsigned i = 0; i < 8; i++)
		raw[i] = (uint8_t)((value >> (56u - 8u * i)) & 0xFFu);
	while (first < 8u && raw[first] == 0x00u)
		first++;
	count = 8u - first;
	if (count == 0) {
		if (cap < 1u)
			return 0;
		out[0] = 0x80u; /* die Null ist die leere Folge */
		return 1;
	}
	if (count == 1u && raw[first] <= 0x7Fu) {
		if (cap < 1u)
			return 0;
		out[0] = raw[first];
		return 1;
	}
	if (cap < count + 1u)
		return 0;
	out[0] = (uint8_t)(0x80u + count);
	memcpy(out + 1, raw + first, count);
	return count + 1u;
}
