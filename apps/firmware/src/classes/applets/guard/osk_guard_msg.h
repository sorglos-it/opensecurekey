/* Was das Geraet wirklich unterschreibt - und warum es drei getrennte
 * Zweckzeichenketten sind.
 *
 * Jede Unterschrift nennt ihren Zweck, und der Zweck wird HIER davorgesetzt,
 * im Geraet. Der Rechner schickt ihn nicht mit. Eine Firmware, die
 * unterschreibt, was man ihr hinhaelt, hebt die Trennung auf, so sorgfaeltig
 * der Wirt auch ist - deshalb sind die Zwecke Konstanten in diesem Modul und
 * kommen nie aus der Nachricht.
 *
 * Drei Wege, drei Zwecke:
 *   attest (14) mit `bh`   "osk-guard/attest/1"     0x00 nonce(32) binhash(32)
 *   attest (14) ohne `bh`  "osk-guard/attest-raw/1" 0x00 aufgabe(1..OSK_GUARD_CHALLENGE_MAX)
 *   sign   (18)            "osk-guard/sign/1"       0x00 label 0x00 hash(32)
 *
 * Warum der alte attest-Aufruf einen EIGENEN Zweck braucht: beide
 * attest-Wege wuerden sonst "Zweck 0x00 <Bytes>" unterschreiben. Eine
 * Aufgabe von 64 Byte ist genau so lang wie Zufallswert und Programm-
 * pruefsumme zusammen - wer also den alten Weg mit <frage>||<pruefsumme>
 * aufruft, bekaeme eine Unterschrift, die der Dongle-Weg fuer gueltig haelt,
 * und die Bindung an die Programmdatei waere weg. Dieselbe Rechnung trennt
 * attest von sign. Ein Prueffall (test_forgery.c) stirbt, wenn jemand zwei
 * dieser Zwecke zusammenlegt.
 *
 * Gegenstueck auf der Rechnerseite: GuardMessage() und NodeSigned() in
 * apps/desktop/classes/. Beide Seiten muessen genau dieselben Bytes bilden.
 */
#ifndef OSK_GUARD_MSG_H
#define OSK_GUARD_MSG_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"

/* Feste Laengen des Verfahrens. */
#define OSK_GUARD_NONCE_LEN 32u
#define OSK_GUARD_HASH_LEN 32u
#define OSK_GUARD_PUB_LEN 32u
#define OSK_GUARD_SIG_LEN 64u
#define OSK_GUARD_LABEL_MAX 64u

/* Laengste Aufgabe des alten attest-Aufrufs.
 *
 * Der Rechner laesst 1 bis 256 Byte zu, das Geraet nicht: Ed25519 geht beim
 * SE050 als ganze NACHRICHT hinein (AN12413 4.10.1.2), und der Treiber nimmt
 * hoechstens 128 Byte (osk_se050_sign). Mit dem 22 Byte langen Zweck und dem
 * Trennbyte bleiben 105. Laenger wird abgelehnt - gehasht wird NICHT, denn
 * dann unterschriebe das Geraet andere Bytes als der Pruefer erwartet. */
#define OSK_GUARD_CHALLENGE_MAX 105u

/* Laengste Bytefolge, die eines der drei Verfahren erzeugt. Alle Puffer im
 * Krypto-Pfad sind damit feste Felder; dynamischen Speicher gibt es nicht. */
#define OSK_GUARD_MSG_MAX 128u

/* Baut "osk-guard/attest/1" 0x00 nonce binhash.
 * Laengen werden geprueft, BEVOR gelesen wird. */
osk_result_t osk_guard_msg_attest(const uint8_t *nonce, size_t nonce_len, const uint8_t *bin_hash,
				  size_t hash_len, uint8_t *out, size_t cap, size_t *len_out);

/* Baut "osk-guard/attest-raw/1" 0x00 aufgabe - der alte Aufruf ohne
 * Programmpruefsumme, unter eigenem Zweck. */
osk_result_t osk_guard_msg_attest_raw(const uint8_t *challenge, size_t len, uint8_t *out,
				      size_t cap, size_t *len_out);

/* Baut "osk-guard/sign/1" 0x00 label 0x00 hash. `label` ist Text ohne
 * Nullbyte; das Trennbyte dahinter macht die Zerlegung eindeutig. */
osk_result_t osk_guard_msg_sign(const char *label, size_t label_len, const uint8_t *job_hash,
				size_t hash_len, uint8_t *out, size_t cap, size_t *len_out);

#endif
