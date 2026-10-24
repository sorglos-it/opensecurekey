/* RLP - die Schreibweise, in der Ethereum alles verpackt.
 *
 * Nur lesen und einzelne Werte schreiben; zusammengesetzt wird nichts.
 * Grund: unterschrieben wird genau der Bytestrom, den der Rechner
 * geschickt hat, und nicht eine selbst gebaute Nachbildung davon. Wer
 * neu zusammensetzt, unterschreibt im Zweifel etwas anderes als er
 * anzeigt - und das ist bei Geld der teuerste denkbare Fehler.
 *
 * Streng nach dem gelben Papier: jede Laenge wird vor dem Lesen geprueft,
 * jede Laengenangabe muss die kuerzest moegliche sein, und eine Zahl darf
 * keine fuehrende Null haben. Alles andere wird abgelehnt, nicht
 * zurechtgebogen - zwei Schreibweisen fuer denselben Wert waeren ein Weg,
 * Anzeige und Unterschrift auseinanderzubringen. */
#ifndef WALLET_RLP_H
#define WALLET_RLP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"

typedef struct {
	const uint8_t *p;
	size_t len;
	size_t pos;
} osk_rlp_reader_t;

typedef enum {
	OSK_RLP_BYTES,
	OSK_RLP_LIST,
} osk_rlp_kind_t;

typedef struct {
	osk_rlp_kind_t kind;
	const uint8_t *data; /* Inhalt, ohne Kopf */
	size_t len;
} osk_rlp_item_t;

void osk_rlp_reader_init(osk_rlp_reader_t *r, const uint8_t *p, size_t len);

/* Liest das naechste Stueck. OSK_ERR_NOT_FOUND, wenn nichts mehr kommt. */
osk_result_t osk_rlp_next(osk_rlp_reader_t *r, osk_rlp_item_t *out);

/* Eine Bytefolge als Zahl lesen. Hoechstens `max_len` Byte, keine
 * fuehrende Null, und 0 ist die leere Folge - so verlangt es RLP. */
osk_result_t osk_rlp_as_u64(const osk_rlp_item_t *item, uint64_t *out);

/* Dasselbe fuer 256 Bit, rechts ausgerichtet in 32 Byte. */
osk_result_t osk_rlp_as_u256(const osk_rlp_item_t *item, uint8_t out[32]);

/* Ist noch etwas uebrig? */
bool osk_rlp_at_end(const osk_rlp_reader_t *r);

/* Eine Zahl als RLP schreiben - gebraucht nur dort, wo wir selbst einen
 * kurzen Kopf bauen (Nachrichtenlaenge bei EIP-191). */
size_t osk_rlp_write_u64(uint64_t value, uint8_t *out, size_t cap);

#endif
