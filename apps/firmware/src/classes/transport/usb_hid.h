/* USB-HID-Transport fuer FIDO2 (CTAPHID).
 *
 * Rahmen, Kanaele, Kettenbildung, Zeitgrenzen - alles aus CTAP 2.2
 * Abschnitt 11.2. Dieses Modul kennt kein CBOR: es setzt eine Nachricht aus
 * 64-Byte-Paketen zusammen, gibt sie an den angemeldeten Bearbeiter und
 * zerlegt die Antwort wieder in Pakete.
 *
 * Es wird nichts belegt: ein Puffer je Geraet, ein Kanalverzeichnis fester
 * Groesse. Eine Nachricht, die nicht hineinpasst, wird abgewiesen, nicht
 * teilweise gelesen. */
#ifndef OSK_USB_HID_H
#define OSK_USB_HID_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"

#define OSK_HID_PACKET_LEN 64
#define OSK_HID_INIT_DATA (OSK_HID_PACKET_LEN - 7)
#define OSK_HID_CONT_DATA (OSK_HID_PACKET_LEN - 5)
/* Hoechstens so viel darf eine Nachricht haben: 64-7 + 128*(64-5). */
#define OSK_HID_MAX_MSG 7609
/* So viel nehmen wir wirklich an - mehr braucht kein CTAP-Befehl von uns. */
#define OSK_HID_BUF_LEN 1300

#define OSK_HID_BROADCAST_CID 0xffffffffu

/* Befehle (Abschnitt 11.2.9) */
#define CTAPHID_PING 0x81
#define CTAPHID_MSG 0x83
#define CTAPHID_LOCK 0x84
#define CTAPHID_INIT 0x86
#define CTAPHID_WINK 0x88
#define CTAPHID_CBOR 0x90
#define CTAPHID_CANCEL 0x91
#define CTAPHID_KEEPALIVE 0xbb
#define CTAPHID_ERROR 0xbf

/* Fehlercodes des Transports */
#define OSK_HID_ERR_INVALID_CMD 0x01
#define OSK_HID_ERR_INVALID_PAR 0x02
#define OSK_HID_ERR_INVALID_LEN 0x03
#define OSK_HID_ERR_INVALID_SEQ 0x04
#define OSK_HID_ERR_MSG_TIMEOUT 0x05
#define OSK_HID_ERR_CHANNEL_BUSY 0x06
#define OSK_HID_ERR_LOCK_REQUIRED 0x0a
#define OSK_HID_ERR_INVALID_CHANNEL 0x0b
#define OSK_HID_ERR_OTHER 0x7f

/* Faehigkeiten in der INIT-Antwort */
#define OSK_HID_CAP_WINK 0x01
#define OSK_HID_CAP_CBOR 0x04
#define OSK_HID_CAP_NMSG 0x08

/* Eine unvollstaendige Nachricht laeuft nach dieser Zeit ab. */
#define OSK_HID_TIMEOUT_MS 500u

/* So gehen fertige Pakete hinaus. Der Puffer ist immer 64 Bytes lang. */
typedef void (*osk_hid_send_fn)(const uint8_t packet[OSK_HID_PACKET_LEN], void *ctx);

/* Meldet sich bei osk_transport_register fuer OSK_LINK_USB_HID an. */
osk_result_t osk_hid_init(osk_hid_send_fn send, void *ctx);

/* Ein eingetroffenes Paket. Immer genau 64 Bytes. */
void osk_hid_rx_packet(const uint8_t packet[OSK_HID_PACKET_LEN]);

/* Von der Hauptschleife regelmaessig aufzurufen: laesst unvollstaendige
 * Nachrichten ablaufen. */
void osk_hid_tick(void);

/* Schickt ein Lebenszeichen auf dem laufenden Kanal (Abschnitt 11.2.9.1.7). */
void osk_hid_keepalive(uint8_t status);
#define OSK_HID_STATUS_PROCESSING 1
#define OSK_HID_STATUS_UPNEEDED 2

#endif
