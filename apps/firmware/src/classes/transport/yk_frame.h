/* Der Rahmen des Yubico-HID-Protokolls: aus 8-Byte-Berichten wird ein
 * 70-Byte-Rahmen und zurueck.
 *
 * ZWEI GROESSEN, DIE GLEICH HEISSEN - hier steht der Unterschied einmal:
 *   - Ein HID-Feature-Bericht ist 8 Byte lang: 7 Datenbytes und ein
 *     Statusbyte am Ende.
 *   - Ein Rahmen ist 70 Byte lang und besteht aus 10 solchen Berichten.
 *   - Die Nutzlast darin ist 64 Byte.
 * "64 Byte" ist also die Nutzlast, NICHT die Berichtsgroesse. Wer den
 * Parser mit 64-Byte-Berichten baut, baut CTAPHID nachgebildet - das ist
 * ein anderes Protokoll auf einer anderen Schnittstelle (usb_hid.h).
 *
 * Aufbau des Rahmens:
 *   Byte  0..63  Nutzlast, mit Nullen aufgefuellt
 *   Byte  64     Steckplatz-/Befehlsnummer
 *   Byte  65..66 Pruefsumme der NUTZLAST, niederwertiges Byte zuerst
 *   Byte  67..69 drei Fuellbytes
 * Die Pruefsumme deckt nur die 64 Nutzlastbytes - nicht das
 * Steckplatzbyte, das zwischen Nutzlast und Pruefsumme liegt. Darum ist
 * der Restwert 0xf0b8 hier NICHT anwendbar (siehe yk_crc.h).
 *
 * Aufteilung in Berichte: Rahmenbyte 7*n bis 7*n+6 wird Bericht n, das
 * Statusbyte ist 0x80 | n, n laeuft von 0 bis 9.
 *
 * Die Sparregel des Hosts: ein Stueck wird uebersprungen, wenn seine
 * sieben Bytes alle null sind UND seine Nummer nicht 0 oder 9 ist. Der
 * Zaehler laeuft trotzdem weiter. Fuer den Parser heisst das: die
 * Nummern duerfen SPRINGEN, und die Luecken sind Nullen. Ein Parser, der
 * auf fortlaufende Nummern besteht, kommt mit ykman nicht zurecht; einer,
 * der Spruenge nicht nachrechnet, laesst sich Luecken unterschieben.
 *
 * Es wird nichts belegt. Alle Puffer stehen im Zustand des Aufrufers;
 * yk_frame_rx_t und yk_frame_tx_t sind dafuer gedacht, statisch zu
 * liegen, nicht auf dem Stapel.
 *
 * WAS HIER NICHT ENTSCHIEDEN WIRD: wie die 64 Nutzlastbytes eines
 * Schreibbefehls aufgebaut sind - wo also der Zugangscode, die
 * Kennungen und die Schalter liegen. Dafuer gibt es keine Belegstelle,
 * die ich pruefen konnte (die gefundenen Quellen widersprechen sich in
 * den Abstaenden). Diese Schicht liefert Steckplatznummer und 64 rohe
 * Bytes; den Aufbau kennt der Besitzer der Einstellungen.
 *
 * Belegstellen siehe yk_hid.h.
 *
 * Umsetzung: src/classes/transport/yk_frame.c */
#ifndef OSK_YK_FRAME_H
#define OSK_YK_FRAME_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define YK_RPT_LEN 8            /* ein Feature-Bericht */
#define YK_RPT_DATA 7           /* davon Daten; Byte 7 ist der Status */
#define YK_RPT_STATUS 7         /* Platz des Statusbytes im Bericht */
#define YK_SLOT_DATA_LEN 64     /* Nutzlast eines Rahmens */
#define YK_FRAME_LEN 70         /* 64 + Steckplatz + 2 Pruefsumme + 3 Fuellbytes */
#define YK_FRAME_CHUNKS 10      /* 70 / 7, geht glatt auf */

#define YK_FRAME_OFF_SLOT 64
#define YK_FRAME_OFF_CRC 65
#define YK_FRAME_OFF_FILLER 67

/* Statusbyte-Schalter */
#define YK_FLAG_WRITE 0x80u        /* Host setzt, Geraet loescht */
#define YK_FLAG_RESP_PENDING 0x40u /* Geraet hat Daten in diesem Bericht */
#define YK_FLAG_TIMEOUT_WAIT 0x20u /* Geraet wartet; Rest in den unteren 5 Bit */
#define YK_SEQ_MASK 0x1fu          /* die unteren 5 Bit tragen die Nummer */
#define YK_DUMMY_REPORT 0x8fu      /* Leerbericht: abbrechen bzw. erzwingen */

/* So viele Stuecke darf eine Antwort haben. Nicht 32: die Nummer 0
 * beendet die Uebertragung, und bei 32 Stuecken laege die 32 nach
 * Maskierung wieder bei 0 - der Host wuerde still abschneiden. */
#define YK_RESP_CHUNKS_MAX 31
#define YK_RESP_MAX (YK_RESP_CHUNKS_MAX * YK_RPT_DATA) /* 217 */

typedef enum {
	YK_RX_IGNORE, /* kein Schreibpaket - nichts zu tun */
	YK_RX_MORE,   /* angenommen, Rahmen noch nicht fertig */
	YK_RX_FRAME,  /* Rahmen vollstaendig UND Pruefsumme stimmt */
	YK_RX_RESET,  /* Host hat den Antwortzustand zurueckgesetzt */
	YK_RX_ABORT,  /* Leerbericht: laufende Arbeit abbrechen */
	YK_RX_BAD,    /* Paket passt nicht - angefangener Rahmen verworfen */
} yk_rx_t;

typedef struct {
	uint8_t buf[YK_FRAME_LEN];
	uint8_t last_seq; /* zuletzt angenommene Nummer */
	bool active;      /* ein Rahmen ist angefangen */
} yk_frame_rx_t;

typedef struct {
	uint8_t buf[YK_RESP_MAX];
	size_t len;
	uint8_t seq;  /* naechste zu sendende Nummer */
	bool active;  /* es sind noch Stuecke zu senden */
	bool closing; /* nur noch der Abschlussbericht fehlt */
} yk_frame_tx_t;

/* --- Geraeteseite: hereinkommende Berichte --- */

void yk_frame_rx_reset(yk_frame_rx_t *rx);

/* Nimmt genau einen Bericht. Verwirft den angefangenen Rahmen bei jedem
 * Verstoss, statt teilweise zu lesen. */
yk_rx_t yk_frame_rx_report(yk_frame_rx_t *rx, const uint8_t rpt[YK_RPT_LEN]);

/* Erst nach YK_RX_FRAME gueltig. */
uint8_t yk_frame_slot(const yk_frame_rx_t *rx);
const uint8_t *yk_frame_payload(const yk_frame_rx_t *rx); /* YK_SLOT_DATA_LEN Byte */

/* --- Geraeteseite: Antwort in Berichte zerlegen --- */

/* len bis YK_RESP_MAX. false, wenn es nicht passt - dann wird nichts
 * kopiert. */
bool yk_frame_tx_start(yk_frame_tx_t *tx, const uint8_t *data, size_t len);

/* Holt den naechsten Bericht. false, wenn nichts mehr zu senden ist. */
bool yk_frame_tx_next(yk_frame_tx_t *tx, uint8_t rpt[YK_RPT_LEN]);

void yk_frame_tx_reset(yk_frame_tx_t *tx);

/* --- Hostseite: nur fuer die Pruefungen, damit echter Verkehr entsteht --- */

/* Baut den 70-Byte-Rahmen. len bis YK_SLOT_DATA_LEN, Rest wird genullt. */
bool yk_frame_build(uint8_t slot, const uint8_t *payload, size_t len,
		    uint8_t out[YK_FRAME_LEN]);

/* Die Sparregel: muss Stueck seq gesendet werden? */
bool yk_frame_chunk_needed(const uint8_t frame[YK_FRAME_LEN], unsigned seq);

/* Schneidet Stueck seq heraus und setzt das Statusbyte 0x80 | seq. */
bool yk_frame_chunk(const uint8_t frame[YK_FRAME_LEN], unsigned seq,
		    uint8_t rpt[YK_RPT_LEN]);

#endif
