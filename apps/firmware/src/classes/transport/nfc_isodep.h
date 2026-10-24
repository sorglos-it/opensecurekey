/* ISO-DEP (ISO/IEC 14443-4) als Karte: Rahmen, Kettenbildung, Zeitgrenzen.
 *
 * Diese Schicht kennt keine APDU und kein CTAP. Sie nimmt die Rahmen, die
 * vom Lesegeraet kommen, setzt aus einer Kette eine vollstaendige APDU
 * zusammen, gibt sie nach oben und zerlegt die Antwort wieder in Rahmen.
 *
 * Ein Angreifer mit einer Antenne steht naeher am Geraet als einer am USB:
 * darum wird jede Laenge VOR dem Lesen geprueft, es gibt feste Obergrenzen
 * und keinen dynamischen Speicher. Ein Rahmen, der nicht passt, wird
 * abgewiesen - nie teilweise gelesen.
 *
 * Was NICHT hier liegt: Schicht 3 (Anti-Kollision, RATS/ATS) macht die
 * NFC-Einheit des nRF52840. Diese Schicht faengt beim ersten Block nach
 * dem RATS an.
 *
 * Es ist nichts davon am Geraet gemessen - die Platine ist noch nicht da.
 * Siehe docs/funk.md. */
#ifndef OSK_NFC_ISODEP_H
#define OSK_NFC_ISODEP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"

/* Groesste Rahmenlaenge, die wir annehmen (FSC). 256 ist der groesste Wert,
 * den ISO/IEC 14443-4 fuer eine Karte vorsieht. */
#define OSK_ISODEP_FRAME_MAX 256
/* Nutzlast je Rahmen: Rahmen minus Steuerbyte, Kennung und Knotenadresse. */
#define OSK_ISODEP_INF_MAX (OSK_ISODEP_FRAME_MAX - 3)
/* So gross darf eine zusammengesetzte APDU werden. Mehr braucht kein Befehl,
 * den dieses Geraet kennt; der CTAP-Kern nimmt ohnehin nicht mehr. */
#define OSK_ISODEP_APDU_MAX 1300

/* Steuerbyte (PCB), ISO/IEC 14443-4 Abschnitt 7.1. */
#define OSK_ISODEP_I_BLOCK 0x02u
#define OSK_ISODEP_R_ACK 0xa2u
#define OSK_ISODEP_R_NAK 0xb2u
#define OSK_ISODEP_S_DESELECT 0xc2u
#define OSK_ISODEP_S_WTX 0xf2u
#define OSK_ISODEP_CHAINING 0x10u
#define OSK_ISODEP_CID_PRESENT 0x08u
#define OSK_ISODEP_NAD_PRESENT 0x04u
#define OSK_ISODEP_BLOCK_NUMBER 0x01u

/* Groesster Verlaengerungsfaktor, den wir anfordern (Abschnitt 7.5.5.2):
 * 1..59 sind erlaubt, 0 und 60..63 nicht. */
#define OSK_ISODEP_WTX_MAX 59u

/* Eine unvollstaendige Kette laeuft nach dieser Zeit ab. Danach ist der
 * Zustand auf Anfang - ein halb gesendeter Befehl bleibt nie liegen. */
#define OSK_ISODEP_TIMEOUT_MS 2000u

/* So gehen fertige Rahmen hinaus (ohne CRC - die rechnet die NFC-Einheit). */
typedef void (*osk_isodep_send_fn)(const uint8_t *frame, size_t len, void *ctx);

/* Bearbeiter einer vollstaendigen APDU. Liefert die Laenge der Antwort in
 * out; 0 heisst "nichts zu senden". */
typedef size_t (*osk_isodep_apdu_fn)(const uint8_t *apdu, size_t len, uint8_t *out, size_t cap,
				     void *ctx);

osk_result_t osk_isodep_init(osk_isodep_send_fn send, osk_isodep_apdu_fn apdu, void *ctx);

/* Das Feld eines Lesegeraets ist da beziehungsweise weg. Beim Kommen und
 * beim Gehen wird alles vergessen, damit kein Rest aus einer fremden
 * Sitzung weiterlebt. */
void osk_isodep_field_on(void);
void osk_isodep_field_off(void);

/* Die groesste Rahmenlaenge, die das Lesegeraet annimmt (FSD). Sie kommt aus
 * dem RATS/ATS-Austausch der Schicht 3; ohne Angabe gilt OSK_ISODEP_FRAME_MAX. */
void osk_isodep_set_frame_size(size_t fsd);

/* Ein eingetroffener Rahmen, ohne CRC. */
void osk_isodep_rx_frame(const uint8_t *frame, size_t len);

/* Von der Hauptschleife aufzurufen: laesst unvollstaendige Ketten ablaufen. */
void osk_isodep_tick(void);

/* Wahr, solange eine Antwortkette noch nicht abgeholt ist. Nur fuer die
 * Pruefungen und die Anzeige. */
bool osk_isodep_busy(void);

/* Fordert vom Lesegeraet mehr Zeit an (S(WTX)), weil der Nutzer noch
 * druecken muss. Ohne das bricht das Lesegeraet nach seinem FWT ab. */
void osk_isodep_request_more_time(uint8_t wtxm);

#endif
