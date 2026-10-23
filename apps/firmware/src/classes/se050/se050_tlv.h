/* TLV-Nutzlast der SE050-APDUs: bauen und lesen.
 *
 * Nach AN12413 Abschnitt 4.1.3: Kennung immer EIN Byte (0x00..0xFF, weicht
 * bewusst von ISO 7816-4 Anhang D ab), Laengenfeld hoechstens drei Byte.
 * Eine Antwort darf ein Drei-Byte-Laengenfeld benutzen, auch wenn der Wert
 * unter 128 liegt - der Leser muss beide Formen annehmen.
 *
 * Der Schreiber hat kein Rueckgabe-Ergebnis je Aufruf. Statt dessen merkt
 * sich der Puffer einen Fehler und liefert ihn am Ende: so steht nicht nach
 * jedem Feld eine Pruefung im Aufrufer, und ein voller Puffer kann nicht
 * uebersehen werden. */
#ifndef SE050_TLV_H
#define SE050_TLV_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"

/* ------------------------------------------------------------- Schreiben */

typedef struct {
	uint8_t *buf;
	size_t cap;
	size_t len;
	bool overflow; /* true, sobald etwas nicht mehr gepasst hat */
} osk_tlv_writer_t;

void osk_tlv_init(osk_tlv_writer_t *w, uint8_t *buf, size_t cap);

void osk_tlv_put(osk_tlv_writer_t *w, uint8_t tag, const uint8_t *val, size_t val_len);
void osk_tlv_put_u8(osk_tlv_writer_t *w, uint8_t tag, uint8_t val);
void osk_tlv_put_u16(osk_tlv_writer_t *w, uint8_t tag, uint16_t val);
void osk_tlv_put_u32(osk_tlv_writer_t *w, uint8_t tag, uint32_t val);

/* OSK_OK und die geschriebene Laenge, oder OSK_ERR_FULL. */
osk_result_t osk_tlv_done(const osk_tlv_writer_t *w, size_t *len_out);

/* ---------------------------------------------------------------- Lesen */

typedef struct {
	const uint8_t *buf;
	size_t len;
} osk_tlv_reader_t;

void osk_tlv_reader_init(osk_tlv_reader_t *r, const uint8_t *buf, size_t len);

/* Erstes Feld mit dieser Kennung suchen. Bei Erfolg zeigt *val in den
 * Eingabepuffer - es wird nichts kopiert, der Puffer muss leben.
 * OSK_ERR_NOT_FOUND, wenn die Kennung fehlt; OSK_ERR_FORMAT, wenn die
 * Nutzlast in sich nicht stimmt. */
osk_result_t osk_tlv_get(const osk_tlv_reader_t *r, uint8_t tag, const uint8_t **val,
			 size_t *val_len);

/* Wie osk_tlv_get(), aber kopiert nach out und prueft die Laenge.
 * OSK_ERR_FULL, wenn der Wert nicht in cap passt. */
osk_result_t osk_tlv_copy(const osk_tlv_reader_t *r, uint8_t tag, uint8_t *out, size_t cap,
			  size_t *len_out);

osk_result_t osk_tlv_get_u8(const osk_tlv_reader_t *r, uint8_t tag, uint8_t *out);
osk_result_t osk_tlv_get_u16(const osk_tlv_reader_t *r, uint8_t tag, uint16_t *out);

#endif
