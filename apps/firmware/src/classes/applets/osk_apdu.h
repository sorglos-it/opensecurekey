/* APDU: zerlegen, zusammenbauen, BER-TLV lesen und schreiben.
 *
 * Das ist der Teil, den ein Angreifer zuerst erreicht: alles, was ueber den
 * Kartenleser hereinkommt, laeuft hier durch, bevor ein Applet es ansieht.
 * Darum gilt hier strenger als sonst:
 *   - vor jedem Lesen wird die Laenge geprueft, nie danach,
 *   - feste Obergrenzen, kein malloc, kein alloca, keine Rekursion,
 *   - ueberzaehlige Bytes am Ende sind ein Fehler, nicht Beifang.
 *
 * Grundlage: ISO/IEC 7816-4 (Aufbau der Kommandos und Antworten,
 * kurze und erweiterte Laengenfelder, Verkettung ueber CLA Bit 5).
 */
#ifndef OSK_APDU_H
#define OSK_APDU_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"

/* Was wir hoechstens annehmen. Mehr braucht kein Kommando von PIV oder
 * OpenPGP; ein Zertifikat ist das Groesste und bleibt darunter. */
#define OSK_APDU_MAX_DATA 3072u
/* Laengstes Kommando am Stueck: Kopf + erweitertes Lc + Daten + Le. */
#define OSK_APDU_MAX_FRAME (7u + OSK_APDU_MAX_DATA + 2u)
/* Groesste Antwort, die wir am Stueck halten (vor der Verkettung). */
#define OSK_APDU_MAX_RESP 3072u

/* Statuswoerter, die wir benutzen (7816-4 Tabelle 6). */
#define OSK_SW_OK 0x9000u
#define OSK_SW_MORE 0x6100u /* 61xx: noch xx Bytes abzuholen */
#define OSK_SW_PIN_TRIES 0x63C0u
#define OSK_SW_WRONG_LENGTH 0x6700u
#define OSK_SW_CHAIN_UNSUPPORTED 0x6883u
#define OSK_SW_COND_NOT_SATISFIED 0x6985u
#define OSK_SW_SECURITY 0x6982u /* Freigabe fehlt */
#define OSK_SW_AUTH_BLOCKED 0x6983u
#define OSK_SW_DATA_INVALID 0x6984u
#define OSK_SW_WRONG_DATA 0x6A80u
#define OSK_SW_FUNC_NOT_SUPPORTED 0x6A81u
#define OSK_SW_FILE_NOT_FOUND 0x6A82u
#define OSK_SW_REF_NOT_FOUND 0x6A88u
#define OSK_SW_WRONG_P1P2 0x6B00u
#define OSK_SW_WRONG_LE 0x6C00u /* 6Cxx: xx ist die richtige Laenge */
#define OSK_SW_INS_NOT_SUPPORTED 0x6D00u
#define OSK_SW_CLA_NOT_SUPPORTED 0x6E00u
#define OSK_SW_UNKNOWN 0x6F00u

/* Ein zerlegtes Kommando. `data` zeigt in den Puffer des Aufrufers und gilt
 * nur so lange wie dieser. */
typedef struct {
	uint8_t cla;
	uint8_t ins;
	uint8_t p1;
	uint8_t p2;
	const uint8_t *data;
	size_t lc;	 /* 0 = keine Daten */
	size_t le;	 /* erwartete Antwortlaenge, nur gueltig wenn has_le */
	bool has_le;	 /* war ein Le-Feld da? */
	bool extended;	 /* erweiterte Laengenfelder benutzt */
	bool chained;	 /* CLA Bit 5: es folgt noch mehr */
	uint8_t channel; /* logischer Kanal aus CLA Bit 0 und 1 */
} osk_apdu_t;

/* Zerlegt ein Kommando. Nimmt alle vier Faelle aus 7816-4 Abschnitt 5.1 an,
 * kurz und erweitert. Fehler: OSK_ERR_ARG bei Nullzeigern,
 * OSK_ERR_FORMAT bei krummem Aufbau, OSK_ERR_FULL wenn Lc oder Le ueber
 * unsere Obergrenze geht. */
osk_result_t osk_apdu_parse(const uint8_t *buf, size_t len, osk_apdu_t *out);

/* --- Antwort zusammenbauen ---------------------------------------------- */

/* Schreibt in den Puffer des Aufrufers und merkt sich, wenn es nicht passte.
 * Nach einem Ueberlauf schreiben alle weiteren Aufrufe nichts mehr - so muss
 * nicht jeder einzelne geprueft werden, nur `overflow` am Ende. */
typedef struct {
	uint8_t *buf;
	size_t cap;
	size_t len;
	bool overflow;
} osk_resp_t;

void osk_resp_init(osk_resp_t *r, uint8_t *buf, size_t cap);
void osk_resp_u8(osk_resp_t *r, uint8_t b);
void osk_resp_bytes(osk_resp_t *r, const uint8_t *p, size_t len);
/* Tag mit einem oder zwei Bytes, dann die Laenge nach BER (kurz oder 81/82),
 * dann die Daten. */
void osk_resp_tlv(osk_resp_t *r, uint32_t tag, const uint8_t *val, size_t len);

/* Verschachtelte TLV ohne zweiten Puffer: `begin` merkt sich die Stelle und
 * laesst Platz fuer die Laenge, `end` traegt sie nach und schiebt die Daten,
 * falls das Laengenfeld laenger ausfallen muss als der Platzhalter.
 *
 * Die Tiefe ist nicht begrenzt: jede Ebene braucht nur eine eigene Marke,
 * und die liegt beim Aufrufer. OSK_TLV_DEPTH sagt, wie tief die Antworten
 * von PIV und OpenPGP gehen - das Attestierungszertifikat geht tiefer
 * (Zertifikat, unterschriebener Teil, Name, Namensmenge, Namensteil), und
 * das ist in Ordnung.
 *
 * Bis zum 29.10.2026 stand hier "hoechstens OSK_TLV_DEPTH Ebenen - tiefer
 * wird nichts von uns". Der Code hat das nie geprueft, und mit der
 * Attestierung war es auch nicht mehr wahr. */
#define OSK_TLV_DEPTH 4

typedef struct {
	size_t at; /* Stelle, an der das Tag beginnt */
	size_t hdr;
} osk_tlv_mark_t;

void osk_resp_tlv_begin(osk_resp_t *r, uint32_t tag, osk_tlv_mark_t *mark);
void osk_resp_tlv_end(osk_resp_t *r, const osk_tlv_mark_t *mark);

/* --- BER-TLV lesen ------------------------------------------------------ */

/* Sucht ein Tag in der obersten Ebene. Tags mit einem, zwei oder drei Bytes
 * (0x5C, 0x7F49, 0x5FC105). Liefert false, wenn das Tag fehlt oder der Aufbau
 * nicht stimmt - auch dann, wenn die Laenge ueber das Ende hinausreicht. */
bool osk_tlv_find(const uint8_t *buf, size_t len, uint32_t tag, const uint8_t **val_out,
		  size_t *len_out);

/* Liest das erste TLV am Anfang des Puffers. `used` sagt, wie viele Bytes es
 * belegt hat. */
bool osk_tlv_first(const uint8_t *buf, size_t len, uint32_t *tag_out, const uint8_t **val_out,
		   size_t *len_out, size_t *used_out);

#endif
