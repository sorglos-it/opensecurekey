/* USB-CCID: der Stick meldet sich als Chipkartenleser mit fest eingelegter
 * Karte.
 *
 * Das ist der Grund fuer diesen Weg: Windows, Linux (pcscd) und macOS
 * bringen den Treiber fuer die Klasse 0x0B selbst mit. Niemand muss etwas
 * installieren - PIV und OpenPGP sind damit ueberall erreichbar.
 *
 * Dieses Modul kennt keine APDU: es nimmt eine Bulk-Nachricht auseinander,
 * gibt den Inhalt an den angemeldeten Bearbeiter und verpackt die Antwort
 * wieder. Es wird nichts belegt - ein Puffer, eine feste Obergrenze. Was
 * nicht hineinpasst, wird abgewiesen, nicht teilweise gelesen.
 *
 * Grundlage: USB Device Class Specification for Integrated Circuit Cards
 * (CCID), Revision 1.1, 22. April 2005. Austauschstufe: kurze APDU
 * (dwFeatures Bit 0x00040000), lange Daten laufen wie in PIV und OpenPGP
 * vorgesehen ueber GET RESPONSE und Kommandoverkettung.
 */
#ifndef OSK_USB_CCID_H
#define OSK_USB_CCID_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"

/* Kopf jeder Bulk-Nachricht, in beide Richtungen (Abschnitt 4.1, 4.2). */
#define OSK_CCID_HEADER_LEN 10u

/* So viel Nutzlast nehmen wir an: kurze APDU mit Kopf und Le. */
#define OSK_CCID_MAX_DATA 261u
#define OSK_CCID_MAX_MSG (OSK_CCID_HEADER_LEN + OSK_CCID_MAX_DATA)

/* Nachrichten vom Rechner zum Leser (Abschnitt 4.1). */
#define OSK_CCID_PC_SET_PARAMETERS 0x61u
#define OSK_CCID_PC_ICC_POWER_ON 0x62u
#define OSK_CCID_PC_ICC_POWER_OFF 0x63u
#define OSK_CCID_PC_GET_SLOT_STATUS 0x65u
#define OSK_CCID_PC_SECURE 0x69u
#define OSK_CCID_PC_T0_APDU 0x6au
#define OSK_CCID_PC_ESCAPE 0x6bu
#define OSK_CCID_PC_GET_PARAMETERS 0x6cu
#define OSK_CCID_PC_RESET_PARAMETERS 0x6du
#define OSK_CCID_PC_ICC_CLOCK 0x6eu
#define OSK_CCID_PC_XFR_BLOCK 0x6fu
#define OSK_CCID_PC_MECHANICAL 0x71u
#define OSK_CCID_PC_ABORT 0x72u
#define OSK_CCID_PC_SET_RATE_AND_CLOCK 0x73u

/* Nachrichten vom Leser zum Rechner (Abschnitt 4.2). */
#define OSK_CCID_RDR_DATA_BLOCK 0x80u
#define OSK_CCID_RDR_SLOT_STATUS 0x81u
#define OSK_CCID_RDR_PARAMETERS 0x82u
#define OSK_CCID_RDR_ESCAPE 0x83u
#define OSK_CCID_RDR_RATE_AND_CLOCK 0x84u

/* bStatus: Bit 0 und 1 sagen, wie es der Karte geht, Bit 6 und 7, wie es dem
 * Kommando ging (Abschnitt 6.2.6). Die Karte ist bei uns immer da. */
#define OSK_CCID_ICC_ACTIVE 0x00u
#define OSK_CCID_CMD_OK 0x00u
#define OSK_CCID_CMD_FAILED 0x40u

/* bError, soweit wir sie brauchen (Tabelle 6.2.6). */
#define OSK_CCID_ERR_CMD_NOT_SUPPORTED 0x00u
#define OSK_CCID_ERR_BAD_LENGTH 0x01u
#define OSK_CCID_ERR_BAD_SLOT 0x05u
#define OSK_CCID_ERR_BAD_LEVEL_PARAM 0x08u
#define OSK_CCID_ERR_HW_ERROR 0xf6u

/* Laenge des Klassendeskriptors 0x21 (Abschnitt 5.1). */
#define OSK_CCID_DESC_LEN 54u

/* So geht eine fertige Bulk-Nachricht hinaus. */
typedef void (*osk_ccid_send_fn)(const uint8_t *msg, size_t len, void *ctx);

/* Meldet den Applet-Verteiler bei osk_transport_register fuer
 * OSK_LINK_USB_CCID an. */
osk_result_t osk_ccid_init(osk_ccid_send_fn send, void *ctx);

/* Eine vollstaendige Bulk-Out-Nachricht. Die USB-Schicht darunter weiss aus
 * dwLength, wie viel sie zu erwarten hat, und gibt erst dann ab. Passt die
 * angekuendigte Laenge nicht zur gelieferten, ist das ein Fehler. */
void osk_ccid_rx(const uint8_t *msg, size_t len);

/* Die ATR der fest eingelegten Karte. Liefert die Laenge, oder 0 wenn der
 * Puffer zu klein ist. */
size_t osk_ccid_atr(uint8_t *out, size_t cap);

/* Der Klassendeskriptor, wie ihn die USB-Beschreibung ausliefert. Liefert
 * OSK_CCID_DESC_LEN, oder 0 wenn der Puffer zu klein ist. */
size_t osk_ccid_descriptor(uint8_t *out, size_t cap);

#endif
