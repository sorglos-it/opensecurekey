/* Das FIDO-Applet ueber NFC.
 *
 * Darueber laeuft derselbe CTAP-Kern wie am USB: osk_ctap_handle fuer CTAP2,
 * osk_ctap_handle_u2f fuer die alten Dienste. Dieses Modul baut nur den
 * Rahmen - Kettenbildung der Befehle (ISO/IEC 7816-4) und der Antworten
 * (61xx mit GET RESPONSE), die Auswahl und das Abwaehlen des Applets.
 *
 * Tastendruck: der Kern fragt bei jedem Vorgang selbst, ueber osk_ui_confirm,
 * und er fragt bei NFC genauso wie bei USB. Dieses Modul umgeht das nicht und
 * kennt keine Ausnahme "weil NFC ja kurz ist" - das Bedrohungsmodell nennt den
 * Weiterleitungsangriff aus der Tasche ausdruecklich. Damit das Lesegeraet
 * waehrend des Drucks nicht abbricht, wird vorher mehr Zeit angefordert
 * (S(WTX) der ISO-DEP-Schicht).
 *
 * Reset bleibt ueber NFC gesperrt - das macht der Kern, nicht dieses Modul.
 *
 * Es ist nichts davon am Geraet gemessen. Siehe docs/funk.md. */
#ifndef OSK_NFC_CTAP_H
#define OSK_NFC_CTAP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"

/* Kennung des Applets (CTAP 2.1, Abschnitt 11.3.3): RID A000000647,
 * PIX 2F0001. */
#define OSK_NFC_FIDO_AID {0xa0, 0x00, 0x00, 0x06, 0x47, 0x2f, 0x00, 0x01}
#define OSK_NFC_FIDO_AID_LEN 8

/* Befehle des Applets. */
#define OSK_NFC_INS_CTAP_MSG 0x10u
#define OSK_NFC_INS_CTAP_GETRESPONSE 0x11u
#define OSK_NFC_INS_CTAP_CONTROL 0x12u
#define OSK_NFC_INS_GET_RESPONSE 0xc0u

/* So viel traegt eine Antwort hoechstens, bevor sie in Stuecken herausgeht. */
#define OSK_NFC_CTAP_BUF_LEN 1300

/* Alles auf Anfang. Ruft nfc_tag beim Kommen und Gehen des Feldes auf. */
void osk_nfc_ctap_reset(void);

/* Das Applet wurde ausgewaehlt. Schreibt die Auswahlantwort ("U2F_V2",
 * weil dieses Geraet auch CTAP1 kann) und liefert deren Laenge. */
size_t osk_nfc_ctap_select(uint8_t *out, size_t cap);

/* Eine APDU an das ausgewaehlte Applet. Liefert die Laenge der Antwort. */
size_t osk_nfc_ctap_apdu(const uint8_t *apdu, size_t len, uint8_t *out, size_t cap);

/* Wahr, wenn das Applet nach NFCCTAP_CONTROL abgewaehlt ist. */
bool osk_nfc_ctap_deselected(void);

#endif
