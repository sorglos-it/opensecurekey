/* Zwei alte, aber weit verbreitete Verfahren - das eine tippt der Stick als
 * Tastatur, das andere antwortet auf eine Frage.
 *
 * 1. Yubico-OTP: 44 Zeichen in ModHex. Die ersten 12 sind die oeffentliche
 *    Kennung, die restlichen 32 ein mit AES-128 verschluesselter Block aus
 *    privater Kennung, zwei Zaehlern, Zeitstempel, Zufall und Pruefsumme.
 *    Damit kommt man ueberall hin, wo nur eine Tastatur erlaubt ist.
 *
 * 2. Challenge-Response mit HMAC-SHA1: 1 bis 64 Bytes hinein, 20 Bytes
 *    heraus. Das ist, was KeePassXC und VeraCrypt zum Entsperren benutzen.
 *
 * Beide Geheimnisse liegen im Tresor als OSK_ITEM_API_KEY, nicht im SE050:
 * der Chip hat genau eine HMAC-Funktion und nennt in osk_se050.h kein
 * Hashverfahren, und auf SHA-1 kann man sich dort nicht verlassen. Jede
 * Benutzung oeffnet darum den Tresoreintrag - und das fragt am Display
 * nach, mit dem Namen des Steckplatzes.
 *
 * Was NICHT hier ist: das Tippen selbst (Transport USB-HID-Tastatur) und
 * die Berichtsrahmen, die ein echter YubiKey ueber USB-HID benutzt. Siehe
 * docs/smartcard.md, "Was noch fehlt".
 */
#ifndef OSK_OTP_H
#define OSK_OTP_H

#include <stdint.h>
#include <stddef.h>
#include "osk_result.h"
#include "osk_transport.h"

/* Zwei Steckplaetze, wie bei den Geraeten, die das erfunden haben. */
#define OSK_OTP_SLOTS 2u

/* 44 Zeichen plus die abschliessende Null. */
#define OSK_OTP_STRING_LEN 44u

/* So lang darf eine Frage sein (HMAC-SHA1 Challenge). */
#define OSK_OTP_CHALLENGE_MAX 64u
#define OSK_OTP_RESPONSE_LEN 20u

/* Was in einem Steckplatz liegen kann. */
#define OSK_OTP_MODE_YUBICO 1u
#define OSK_OTP_MODE_HMAC 2u

/* Meldet sich bei osk_transport_register fuer OSK_LINK_USB_VENDOR an. */
osk_result_t osk_otp_init(void);

/* Erzeugt ein Einmalkennwort. `out` braucht OSK_OTP_STRING_LEN + 1 Bytes.
 * Zaehlt die Zaehler im Tresoreintrag weiter, damit sie einen Stromausfall
 * ueberleben. */
osk_result_t osk_otp_generate(unsigned slot, char *out, size_t cap);

/* Antwortet auf eine Frage mit HMAC-SHA1. */
osk_result_t osk_otp_challenge(unsigned slot, const uint8_t *challenge, size_t len,
			       uint8_t out[OSK_OTP_RESPONSE_LEN]);

/* Legt einen Steckplatz an oder ueberschreibt ihn. Fuer das
 * Verwaltungsprogramm und die Pruefungen.
 *   OSK_OTP_MODE_YUBICO: 6 Bytes oeffentliche Kennung, 6 Bytes private
 *                        Kennung, 16 Bytes AES-Schluessel
 *   OSK_OTP_MODE_HMAC:   20 Bytes Schluessel
 */
osk_result_t osk_otp_program(unsigned slot, uint8_t mode, const uint8_t *public_id,
			     const uint8_t *private_id, const uint8_t *key, size_t key_len);

/* Der Bearbeiter des eigenen Kanals. Rahmen:
 *   [0] 0x01 = Kennwort erzeugen, 0x02 = Frage beantworten
 *   [1] Steckplatz, 1 oder 2
 *   [2..] bei 0x02 die Frage
 * Antwort: bei 0x01 die 44 Zeichen, bei 0x02 die 20 Bytes. Im Fehlerfall
 * ein Byte 0xFF und der Grund aus osk_result_t. */
osk_result_t osk_otp_handle(const osk_request_t *req, osk_response_t *res);

#endif
