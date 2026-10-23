/* CBOR, klein und streng - nur das, was CTAP2 braucht.
 *
 * Der Leser ist der gefaehrlichste Teil der Firmware: hier kommt ein
 * Angreifer zuerst an. Darum gilt ohne Ausnahme:
 *   - jede Laenge wird geprueft, BEVOR gelesen wird,
 *   - kein dynamischer Speicher,
 *   - feste Obergrenzen fuer Tiefe und Anzahl,
 *   - nur die kanonische Form nach CTAP 2.2 Abschnitt 8 wird angenommen
 *     (kleinste Kodierung, feste Laengen, sortierte Schluessel, keine Tags).
 *
 * Der Schreiber erzeugt ebenfalls nur die kanonische Form. Die Tests lesen
 * jede erzeugte Antwort mit diesem Leser wieder ein - was der Schreiber
 * ausgibt, muss der strenge Leser annehmen. */
#ifndef OSK_CBOR_H
#define OSK_CBOR_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"

/* Obergrenzen. CTAP erlaubt hoechstens vier Ebenen; wir nehmen genau vier. */
#define OSK_CBOR_MAX_DEPTH 4
/* Mehr Elemente als das kann keine echte CTAP-Nachricht haben. */
#define OSK_CBOR_MAX_ITEMS 256

typedef enum {
	OSK_CBOR_UINT = 0,
	OSK_CBOR_NINT = 1,
	OSK_CBOR_BSTR = 2,
	OSK_CBOR_TSTR = 3,
	OSK_CBOR_ARRAY = 4,
	OSK_CBOR_MAP = 5,
	OSK_CBOR_SIMPLE = 7,
} osk_cbor_type_t;

typedef struct {
	const uint8_t *buf;
	size_t len;
	size_t pos;
	unsigned depth;
} osk_cbor_reader_t;

void osk_cbor_reader_init(osk_cbor_reader_t *r, const uint8_t *buf, size_t len);

/* Wie viele Bytes sind noch da? */
static inline size_t osk_cbor_left(const osk_cbor_reader_t *r)
{
	return r->len - r->pos;
}

/* Art des naechsten Werts, ohne ihn zu verbrauchen. */
osk_result_t osk_cbor_peek_type(const osk_cbor_reader_t *r, osk_cbor_type_t *out);

/* Ganze Zahlen. _int liefert auch negative Werte (COSE braucht das). */
osk_result_t osk_cbor_read_uint(osk_cbor_reader_t *r, uint64_t *out);
osk_result_t osk_cbor_read_int(osk_cbor_reader_t *r, int64_t *out);

/* Zeichen- und Bytefolgen werden NICHT kopiert: der Zeiger zeigt in den
 * Eingabepuffer. Der Aufrufer darf ihn nur nutzen, solange die Nachricht lebt. */
osk_result_t osk_cbor_read_bytes(osk_cbor_reader_t *r, const uint8_t **out, size_t *len_out);
osk_result_t osk_cbor_read_text(osk_cbor_reader_t *r, const char **out, size_t *len_out);
osk_result_t osk_cbor_read_bool(osk_cbor_reader_t *r, bool *out);

/* Betritt eine Liste oder Abbildung und liefert die Anzahl der Elemente
 * (bei Abbildungen die Anzahl der Paare). */
osk_result_t osk_cbor_enter_array(osk_cbor_reader_t *r, size_t *count_out);
osk_result_t osk_cbor_enter_map(osk_cbor_reader_t *r, size_t *count_out);
void         osk_cbor_leave(osk_cbor_reader_t *r);

/* Ueberspringt genau einen Wert samt Inhalt. */
osk_result_t osk_cbor_skip(osk_cbor_reader_t *r);

/* Prueft eine ganze Nachricht gegen die kanonische Form, ohne sie zu deuten.
 * Genau ein Wert muss den Puffer vollstaendig fuellen. */
osk_result_t osk_cbor_validate(const uint8_t *buf, size_t len);

/* Vergleicht zwei Schluessel nach der CTAP-Sortierregel: erst Haupttyp,
 * dann Laenge, dann byteweise. Negativ, null oder positiv wie memcmp. */
int osk_cbor_key_cmp(const uint8_t *a, size_t alen, const uint8_t *b, size_t blen);

/* ---- Schreiber ---------------------------------------------------------- */

typedef struct {
	uint8_t *buf;
	size_t cap;
	size_t len;
	bool failed;   /* einmal uebergelaufen, bleibt kaputt */
} osk_cbor_writer_t;

void osk_cbor_writer_init(osk_cbor_writer_t *w, uint8_t *buf, size_t cap);

void osk_cbor_put_uint(osk_cbor_writer_t *w, uint64_t v);
void osk_cbor_put_int(osk_cbor_writer_t *w, int64_t v);
void osk_cbor_put_bytes(osk_cbor_writer_t *w, const uint8_t *p, size_t len);
void osk_cbor_put_text(osk_cbor_writer_t *w, const char *s);
void osk_cbor_put_text_len(osk_cbor_writer_t *w, const char *s, size_t len);
void osk_cbor_put_bool(osk_cbor_writer_t *w, bool v);
void osk_cbor_put_array(osk_cbor_writer_t *w, size_t count);
void osk_cbor_put_map(osk_cbor_writer_t *w, size_t count);

/* true, wenn alles hineingepasst hat. */
bool osk_cbor_writer_ok(const osk_cbor_writer_t *w);

#endif
