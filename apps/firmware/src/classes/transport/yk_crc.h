/* Die Pruefsumme des Yubico-Protokolls.
 *
 * Es ist der CRC-16 nach ISO 13239: umgedrehtes Polynom 0x8408, Startwert
 * 0xffff, kein Abschluss-XOR. Uebertragen wird das Einerkomplement,
 * niederwertiges Byte zuerst. Laeuft man die Rechnung ueber Daten UND
 * uebertragene Pruefsumme, bleibt immer 0xf0b8 stehen.
 *
 * ACHTUNG, zwei Groessen mit demselben Namen: "die Yubico-Pruefsumme"
 * meint an zwei Stellen Verschiedenes.
 *   - Im OTP-Zeichen folgt die Pruefsumme unmittelbar auf die Daten. Dort
 *     gilt der Restwert 0xf0b8 ueber einen zusammenhaengenden Puffer.
 *   - Im HID-Rahmen liegt zwischen Nutzlast und Pruefsumme noch das
 *     Steckplatzbyte (siehe yk_frame.h). Dort ist der Restwert NICHT ueber
 *     den Rahmen zu bilden; gerechnet wird ueber die 64 Nutzlastbytes und
 *     das Ergebnis verglichen.
 * Beide Wege stehen hier getrennt, damit keiner fuer den anderen
 * eingesetzt wird.
 *
 * Belegstellen siehe yk_hid.h.
 *
 * Umsetzung: src/classes/transport/yk_crc.c */
#ifndef OSK_YK_CRC_H
#define OSK_YK_CRC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Der Restwert eines vollstaendigen, zusammenhaengenden Puffers. */
#define YK_CRC_RESIDUE 0xf0b8u

/* Die rohe Rechnung: Startwert 0xffff, kein Abschluss-XOR. */
uint16_t yk_crc16(const uint8_t *data, size_t len);

/* Was uebertragen wird: das Einerkomplement der rohen Rechnung. */
uint16_t yk_crc16_wire(const uint8_t *data, size_t len);

/* Zusammenhaengender Puffer, Pruefsumme am Ende: Restwert pruefen. */
bool yk_crc16_residue_ok(const uint8_t *data, size_t len);

#endif
