/* USB-Massenspeicher: das kleine, schreibgeschuetzte Laufwerk, mit dem der
 * Stick seine eigenen Verwaltungsprogramme mitbringt.
 *
 * Zwei Schichten in einer Datei, weil beide nur zusammen Sinn ergeben:
 *  - Bulk-Only Transport (USB MSC BBB, Revision 1.0): CBW herein, Daten
 *    hinaus, CSW hinterher.
 *  - ein winziger SCSI-Satz (SPC-4 / SBC-3), der genau so viel kann, wie ein
 *    Rechner zum Lesen eines Laufwerks braucht.
 *
 * Das Entscheidende steht hier und nicht im Kleingedruckten:
 * **jeder Befehl, der schreiben, loeschen, formatieren oder Einstellungen
 * aendern wuerde, wird abgewiesen.** Nicht stillschweigend, sondern mit
 * SCSI-Sinnesdaten DATA PROTECT / WRITE PROTECTED, damit der Rechner es
 * versteht und nicht endlos wiederholt. Ein beschreibbares Laufwerk an einem
 * Sicherheitsschluessel waere genau das BadUSB-Einfallstor, das wir vermeiden
 * (docs/threat-model.md).
 *
 * Diese Schnittstelle erreicht jeder Rechner, an dem der Stick steckt, ohne
 * dass ein Mensch zustimmt. Darum: Laenge vor dem Lesen, feste Obergrenzen,
 * kein dynamischer Speicher, ein Puffer je Geraet.
 *
 * Der Inhalt kommt nie ueber dieses Laufwerk herein, sondern nur ueber ein
 * signiertes Firmware-Update. Erzeugt wird das Abbild mit tools/mkimage.py.
 * Beschreibung: docs/laufwerk.md */
#ifndef OSK_USB_MSC_H
#define OSK_USB_MSC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"

/* Ein Block. 512 Byte ist das, was jeder Rechner ohne Nachfrage annimmt. */
#define OSK_MSC_BLOCK_LEN 512

/* Rahmen des Bulk-Only Transport. Beide Laengen sind fest. */
#define OSK_MSC_CBW_LEN 31
#define OSK_MSC_CSW_LEN 13
#define OSK_MSC_CBW_SIG 0x43425355u
#define OSK_MSC_CSW_SIG 0x53425355u

/* Laengstes Befehlsfeld eines CBW. */
#define OSK_MSC_CDB_MAX 16

/* Zustand im CSW. */
#define OSK_MSC_CSW_PASSED 0x00
#define OSK_MSC_CSW_FAILED 0x01
#define OSK_MSC_CSW_PHASE 0x02

/* Sinnesschluessel (SPC-4, Tabelle 48). Mehr brauchen wir nicht. */
#define OSK_MSC_SK_NO_SENSE 0x00
#define OSK_MSC_SK_NOT_READY 0x02
#define OSK_MSC_SK_ILLEGAL_REQUEST 0x05
#define OSK_MSC_SK_DATA_PROTECT 0x07

/* Zusatzcodes, die wir vergeben. */
#define OSK_MSC_ASC_INVALID_OPCODE 0x20
#define OSK_MSC_ASC_LBA_OUT_OF_RANGE 0x21
#define OSK_MSC_ASC_INVALID_FIELD 0x24
#define OSK_MSC_ASC_WRITE_PROTECTED 0x27
#define OSK_MSC_ASC_MEDIUM_NOT_PRESENT 0x3a

/* So kommt ein Block aus dem Flash. Der Aufrufer fuellt genau
 * OSK_MSC_BLOCK_LEN Bytes oder meldet einen Fehler - halb gefuellt gibt es
 * nicht. Auf dem Geraet liegt dahinter der QSPI-Treiber mit dem Versatz der
 * Laufwerkspartition, im Test eine Attrappe im RAM. */
typedef osk_result_t (*osk_msc_read_fn)(uint32_t lba, uint8_t out[OSK_MSC_BLOCK_LEN], void *ctx);

/* So gehen Daten und CSW zum Rechner hinaus. */
typedef void (*osk_msc_send_fn)(const uint8_t *data, size_t len, void *ctx);

typedef struct {
	osk_msc_read_fn read;
	osk_msc_send_fn send;
	void *ctx;
	uint32_t blocks; /* Groesse des Laufwerks in Bloecken zu 512 Byte */
} osk_msc_config_t;

/* Richtet das Laufwerk ein und meldet es bei osk_transport_register fuer
 * OSK_LINK_USB_MSC an. Das Laufwerk ist danach eingeschaltet. */
osk_result_t osk_msc_init(const osk_msc_config_t *cfg);

/* Ein Paket vom Bulk-OUT-Endpunkt. Erwartet wird ein CBW; alles andere wird
 * als Phasenfehler beantwortet. Datenpakete, die nach einem abgewiesenen
 * Schreibbefehl trotzdem kommen, landen hier ebenfalls und werden verworfen. */
void osk_msc_rx(const uint8_t *data, size_t len);

/* Bulk-Only Mass Storage Reset (Klassenanforderung 0xFF). */
void osk_msc_reset(void);

/* Abschaltbar, weil manche Rechner und BIOSe ueber viele USB-Schnittstellen
 * stolpern. Ausgeschaltet antwortet das Laufwerk auf nichts mehr; damit es
 * auch aus der Geraetebeschreibung verschwindet, muss sich der Stick neu
 * anmelden. Siehe docs/laufwerk.md, Abschnitt "Abschalten". */
osk_result_t osk_msc_set_enabled(bool on);
bool osk_msc_enabled(void);

/* Groesse des Laufwerks in Bloecken. 0 heisst: nicht eingerichtet. */
uint32_t osk_msc_blocks(void);

/* Ein CBW, der nicht einmal ein CBW ist, laesst beide Endpunkte stehen
 * (BBB 6.6.1): es geht nichts mehr hinaus, bis der Rechner zuruecksetzt.
 * Darauf zu antworten hiesse, auf eine unverstandene Nachricht zu antworten. */
bool osk_msc_stalled(void);

/* Wie oft ein Schreibversuch abgewiesen wurde. Das Geraet zeigt das an und
 * schreibt es ins Geraeteprotokoll - ein Rechner, der an diesem Laufwerk
 * schreiben will, tut etwas, das der Nutzer sehen soll. */
uint32_t osk_msc_write_attempts(void);
void osk_msc_write_attempts_reset(void);

#endif
