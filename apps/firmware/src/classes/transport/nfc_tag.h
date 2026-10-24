/* Der NFC-Weg des Geraets: ein Typ-4-Tag mit zwei Anwendungen.
 *
 *   FIDO-Applet  - derselbe CTAP-Kern wie am USB (nfc_ctap)
 *   NDEF-Tag     - Antippen ohne App, abschaltbar (nfc_ndef)
 *
 * Diese Datei ist nur die Weiche: sie liest die Anwendungsauswahl und gibt
 * alles Weitere an die gewaehlte Anwendung. Darunter liegt ISO-DEP
 * (nfc_isodep), darueber die Applets. Schicht 3 - Anti-Kollision und
 * RATS/ATS - macht die NFC-Einheit des nRF52840.
 *
 * Es ist nichts davon am Geraet gemessen. Siehe docs/funk.md. */
#ifndef OSK_NFC_TAG_H
#define OSK_NFC_TAG_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"
#include "nfc_isodep.h"

/* Kennung der NDEF-Anwendung (NFC Forum Type 4 Tag 1.1, Abschnitt 5.1). */
#define OSK_NFC_NDEF_AID {0xd2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x01}
#define OSK_NFC_NDEF_AID_LEN 7

typedef enum {
	OSK_NFC_APP_NONE = 0,
	OSK_NFC_APP_FIDO,
	OSK_NFC_APP_NDEF,
} osk_nfc_app_t;

/* Meldet sich bei osk_transport_register fuer OSK_LINK_NFC an und baut die
 * ISO-DEP-Schicht darunter auf. send schickt fertige Rahmen ohne CRC. */
osk_result_t osk_nfc_tag_init(osk_isodep_send_fn send, void *ctx);

/* Den ganzen NFC-Weg abschalten. Ausgeschaltet antwortet das Geraet auf
 * keinen Rahmen - es ist dann kein Tag. */
void osk_nfc_tag_enable(bool on);
bool osk_nfc_tag_enabled(void);

/* Das Feld eines Lesegeraets kommt oder geht. */
void osk_nfc_tag_field_on(void);
void osk_nfc_tag_field_off(void);

/* Ein eingetroffener ISO-DEP-Rahmen, ohne CRC. */
void osk_nfc_tag_rx_frame(const uint8_t *frame, size_t len);

/* Von der Hauptschleife regelmaessig aufzurufen. */
void osk_nfc_tag_tick(void);

/* Welche Anwendung gerade ausgewaehlt ist. Fuer Anzeige und Pruefungen. */
osk_nfc_app_t osk_nfc_tag_selected(void);

#endif
