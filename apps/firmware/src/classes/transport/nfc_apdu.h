/* APDU-Zerlegung nach ISO/IEC 7816-4, kurze und erweiterte Laengen.
 *
 * Reines Lesen: diese Datei legt nichts an, aendert nichts und ruft nichts
 * auf. Sie prueft die Laengen VOR jedem Zugriff und liefert entweder eine
 * vollstaendig gepruefte APDU oder einen Fehler - niemals etwas Halbes.
 *
 * Das ist der zweite Parser, den ein Angreifer mit einer Antenne erreicht
 * (nach ISO-DEP), darum steht hier jede Grenze ausdruecklich da. */
#ifndef OSK_NFC_APDU_H
#define OSK_NFC_APDU_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Statuswoerter, die dieses Geraet benutzt (ISO/IEC 7816-4 Tabelle 6). */
#define OSK_SW_OK 0x9000u
#define OSK_SW_UPDATE 0x9100u /* CTAP: es geht noch weiter */
#define OSK_SW_WRONG_LENGTH 0x6700u
#define OSK_SW_COND_NOT_SATISFIED 0x6985u
#define OSK_SW_WRONG_DATA 0x6a80u
#define OSK_SW_FILE_NOT_FOUND 0x6a82u
#define OSK_SW_WRONG_P1P2 0x6a86u
#define OSK_SW_INS_NOT_SUPPORTED 0x6d00u
#define OSK_SW_CLA_NOT_SUPPORTED 0x6e00u
#define OSK_SW_NO_PRECISE_DIAGNOSIS 0x6f00u

/* Eine geprueft zerlegte Befehls-APDU. data zeigt in den Eingabepuffer des
 * Aufrufers - es wird nichts kopiert. */
typedef struct {
	uint8_t cla;
	uint8_t ins;
	uint8_t p1;
	uint8_t p2;
	const uint8_t *data;
	size_t lc;	   /* 0, wenn kein Datenfeld da ist */
	size_t le;	   /* erwartete Antwortlaenge */
	bool has_le;	   /* war ein Le-Feld da? */
	bool extended;	   /* erweiterte Laengen benutzt */
	bool chained;	   /* CLA sagt: es folgt noch ein Teil */
	bool chaining_cla; /* CTAP benutzt 0x90 statt 0x10 fuer die Kette */
} osk_apdu_t;

/* Zerlegt eine APDU. Liefert true nur, wenn alle Felder stimmig sind. */
bool osk_nfc_apdu_parse(const uint8_t *buf, size_t len, osk_apdu_t *out);

/* Schreibt ein Statuswort an das Ende einer Antwort. Liefert die neue
 * Gesamtlaenge, oder 0, wenn der Platz nicht reicht. */
size_t osk_apdu_status(uint8_t *out, size_t cap, size_t len, uint16_t sw);

/* Nur das Statuswort, ohne Daten. */
size_t osk_apdu_only_status(uint8_t *out, size_t cap, uint16_t sw);

#endif
