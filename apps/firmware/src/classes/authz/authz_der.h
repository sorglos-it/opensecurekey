/* Winziger DER-Leser und -Schreiber, nur fuer RFC 3161.
 *
 * Absichtlich kein allgemeiner ASN.1-Werkzeugkasten: gebraucht werden acht
 * Typen, und ein vollstaendiger Parser waere mehr Code, mehr Flash und mehr
 * Angriffsfläche fuer dieselbe Leistung.
 *
 * Diese Schicht nimmt Puffer vom Draht entgegen - die Antwort einer fremden
 * Zeitstempelstelle. Jede Laenge wird gegen den verbleibenden Puffer
 * geprueft, bevor sie benutzt wird; es gibt keinen Weg, bei dem ein Zeiger
 * ohne Pruefung weiterwandert. Der Plan verlangt, dass neue Parser gefuzzt
 * werden (docs/plan.md Abschnitt 8) - dieser gehoert dazu. */
#ifndef AUTHZ_DER_H
#define AUTHZ_DER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"

#define DER_SEQUENCE   0x30u
#define DER_SET	       0x31u
#define DER_INTEGER    0x02u
#define DER_OCTET      0x04u
#define DER_OID	       0x06u
#define DER_BOOLEAN    0x01u
#define DER_GENTIME    0x18u
#define DER_CTX0       0xA0u /* [0] konstruiert */

/* Was noch zu lesen ist. Keine Zeiger auf Anfang oder Ende, nur Rest -
 * damit es keine zwei Stellen gibt, die beim Weiterwandern auseinander
 * laufen koennen. */
typedef struct {
	const uint8_t *p;
	size_t len;
} authz_der_t;

/* Das naechste Element nehmen. `val` zeigt in den Eingabepuffer, es wird
 * nichts kopiert. OSK_ERR_FORMAT, sobald eine Laenge nicht in den Rest
 * passt - und zwar vor jedem Zugriff darauf. */
osk_result_t authz_der_next(authz_der_t *c, uint8_t *tag_out, const uint8_t **val_out,
			    size_t *vlen_out);

/* Das naechste Element nehmen und darauf bestehen, dass es diesen Typ hat. */
osk_result_t authz_der_expect(authz_der_t *c, uint8_t tag, const uint8_t **val_out,
			      size_t *vlen_out);

/* In ein konstruiertes Element hineingehen: `inner` liest dann dessen
 * Inhalt, `c` steht danach dahinter. */
osk_result_t authz_der_enter(authz_der_t *c, uint8_t tag, authz_der_t *inner_out);

/* Eine kleine vorzeichenlose INTEGER lesen. Laenger als acht Byte oder mit
 * gesetztem Vorzeichenbit ist hier ein Formatfehler und keine grosse Zahl:
 * die Felder, um die es geht (version, status), sind klein, und eine
 * stillschweigend abgeschnittene Zahl waere schlimmer als eine Absage. */
osk_result_t authz_der_u64(const uint8_t *val, size_t vlen, uint64_t *out);

/* --- Schreiben ---------------------------------------------------------
 *
 * Rueckwaerts, vom Ende des Puffers her. Grund: die Laenge eines Elements
 * steht vor seinem Inhalt, ist aber erst nach ihm bekannt. Vorwaerts
 * schreiben heisst entweder zweimal rechnen oder hinterher verschieben -
 * beides sind zwei Wahrheiten ueber dieselbe Laenge. Rueckwaerts ist es
 * eine. */
typedef struct {
	uint8_t *buf;  /* Anfang des Puffers */
	size_t cap;    /* seine Groesse */
	size_t used;   /* so viele Byte am ENDE sind belegt */
	bool overflow; /* einmal gesetzt, bleibt gesetzt */
} authz_der_w_t;

void authz_der_w_init(authz_der_w_t *w, uint8_t *buf, size_t cap);

/* Rohbytes voranstellen. */
void authz_der_w_bytes(authz_der_w_t *w, const uint8_t *p, size_t len);

/* Kopf (Typ und Laenge) voranstellen - nachdem der Inhalt schon drin ist.
 * `content_len` ist, was dieser Kopf umfasst. */
void authz_der_w_header(authz_der_w_t *w, uint8_t tag, size_t content_len);

/* Ein vollstaendiges Element: Kopf und Inhalt. */
void authz_der_w_tlv(authz_der_w_t *w, uint8_t tag, const uint8_t *val, size_t vlen);

/* Eine vorzeichenlose Zahl als INTEGER. Ein fuehrendes Nullbyte kommt
 * dazu, wenn das hoechste Bit gesetzt ist - sonst waere es in DER eine
 * negative Zahl. */
void authz_der_w_u64(authz_der_w_t *w, uint64_t v);

/* Grosse vorzeichenlose Zahl aus Rohbytes (fuer die Nonce). Fuehrende
 * Nullen fallen weg, ein Nullbyte kommt bei gesetztem hoechstem Bit dazu -
 * dieselbe Regel wie oben, nur fuer einen Wert, der nicht in uint64 passt. */
void authz_der_w_uint_bytes(authz_der_w_t *w, const uint8_t *p, size_t len);

void authz_der_w_bool(authz_der_w_t *w, bool v);

/* Fertig: wo das Ergebnis im Puffer anfaengt und wie lang es ist.
 * OSK_ERR_FULL, wenn irgendwann zu wenig Platz war - geprueft wird am
 * Ende, nicht bei jedem Aufruf, aber `overflow` vergisst nichts. */
osk_result_t authz_der_w_done(const authz_der_w_t *w, const uint8_t **start_out,
			      size_t *len_out);

#endif
