/* Kleine Helfer fuer die Pruefungen: einen Rahmen bauen und eine Antwort
 * auseinandernehmen. Benutzt denselben CBOR-Schreiber und -Leser wie die
 * Firmware - was der Schreiber ausgibt, muss der strenge Leser annehmen.
 */
#ifndef GUARD_HELPER_H
#define GUARD_HELPER_H

#include <string.h>
#include "osk_cbor.h"
#include "osk_guard_fw.h"
#include "osk_transport.h"

#define GH_BUF 512

/* Ein Rahmen im Bau. `d` wird vom Aufrufer gefuellt: erst gh_begin mit der
 * Zahl der Datenpaare, dann die Paare, dann gh_end. */
typedef struct {
	uint8_t buf[GH_BUF];
	osk_cbor_writer_t w;
} gh_frame_t;

static inline void gh_begin(gh_frame_t *f, uint64_t cmd, uint64_t seq, size_t data_pairs)
{
	osk_cbor_writer_init(&f->w, f->buf, sizeof(f->buf));
	osk_cbor_put_map(&f->w, 4u);
	osk_cbor_put_text(&f->w, "v");
	osk_cbor_put_uint(&f->w, OSK_GUARD_PROTO_VERSION);
	osk_cbor_put_text(&f->w, "c");
	osk_cbor_put_uint(&f->w, cmd);
	osk_cbor_put_text(&f->w, "s");
	osk_cbor_put_uint(&f->w, seq);
	osk_cbor_put_text(&f->w, "d");
	osk_cbor_put_map(&f->w, data_pairs);
}

static inline size_t gh_end(gh_frame_t *f)
{
	return osk_cbor_writer_ok(&f->w) ? f->w.len : 0u;
}

/* Die zerlegte Antwort. `data` zeigt in `raw`, lebt also solange die
 * Antwort lebt. */
typedef struct {
	uint8_t raw[GH_BUF];
	size_t len;
	uint64_t version;
	uint64_t seq;
	uint64_t result;
	osk_cbor_reader_t data;
	bool ok; /* Rahmen liess sich lesen */
} gh_reply_t;

/* Sucht einen Schluessel in einer Abbildung - dieselbe Suche wie in der
 * Firmware, aber eigenstaendig geschrieben, damit ein Fehler dort hier
 * nicht mitgemacht wird. */
static inline bool gh_seek(const osk_cbor_reader_t *map, const char *key, osk_cbor_reader_t *out)
{
	osk_cbor_reader_t r = *map;
	size_t count = 0;

	if (osk_cbor_enter_map(&r, &count) != OSK_OK) {
		return false;
	}
	for (size_t i = 0; i < count; i++) {
		const char *name = NULL;
		size_t name_len = 0;
		if (osk_cbor_read_text(&r, &name, &name_len) != OSK_OK) {
			return false;
		}
		if (name_len == strlen(key) && memcmp(name, key, name_len) == 0) {
			*out = r;
			return true;
		}
		if (osk_cbor_skip(&r) != OSK_OK) {
			return false;
		}
	}
	return false;
}

static inline bool gh_uint(const osk_cbor_reader_t *map, const char *key, uint64_t *out)
{
	osk_cbor_reader_t r;
	return gh_seek(map, key, &r) && osk_cbor_read_uint(&r, out) == OSK_OK;
}

static inline bool gh_bytes(const osk_cbor_reader_t *map, const char *key, const uint8_t **out,
			    size_t *len_out)
{
	osk_cbor_reader_t r;
	return gh_seek(map, key, &r) && osk_cbor_read_bytes(&r, out, len_out) == OSK_OK;
}

static inline bool gh_bool(const osk_cbor_reader_t *map, const char *key, bool *out)
{
	osk_cbor_reader_t r;
	return gh_seek(map, key, &r) && osk_cbor_read_bool(&r, out) == OSK_OK;
}

/* Schickt einen Rahmen an das Applet und zerlegt die Antwort. */
static inline osk_result_t gh_call(const uint8_t *frame, size_t len, osk_link_t link,
				   gh_reply_t *out)
{
	osk_request_t req = {link, frame, len};
	osk_response_t res = {out->raw, sizeof(out->raw), 0u};

	memset(out, 0, sizeof(*out));
	res.data = out->raw;
	res.cap = sizeof(out->raw);
	res.len = 0;

	const osk_result_t rc = osk_guard_handle(&req, &res);
	out->len = res.len;
	out->ok = false;
	if (rc != OSK_OK || res.len == 0u) {
		return rc;
	}

	osk_cbor_reader_t top;
	osk_cbor_reader_init(&top, out->raw, out->len);
	if (!gh_uint(&top, "v", &out->version) || !gh_uint(&top, "s", &out->seq) ||
	    !gh_uint(&top, "r", &out->result)) {
		return rc;
	}
	if (!gh_seek(&top, "d", &out->data)) {
		return rc;
	}
	out->ok = true;
	return rc;
}

#endif
