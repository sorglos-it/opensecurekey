/* Krypto-Naht fuer SCP03: AES und CMAC.
 *
 * Hier wird NICHTS gerechnet. Diese Datei sagt nur, WAS der Kanal braucht;
 * gerechnet wird in einer geprueften Bibliothek:
 *   se050_crypto_psa.c  - auf dem Geraet: PSA Crypto aus dem nRF Connect SDK
 *   se050_crypto_cng.c  - im Pruefbau am PC: Windows CNG (bcrypt.dll)
 * Eigene AES- oder CMAC-Rechnung gibt es in diesem Projekt nicht und soll
 * es nie geben.
 *
 * SCP03 braucht genau drei Dinge (GlobalPlatform Amendment D, Abschnitt 4.1
 * und 6.2): AES-CMAC als Pseudozufallsfunktion der Schluesselableitung und
 * fuer die Nachrichtensicherung, AES-CBC fuer die Nutzlast, und einen
 * einzelnen AES-Block fuer den Startwert der CBC-Kette.
 *
 * Alle Schluessel sind 16 Byte: der SE050 nimmt fuer Anmeldeobjekte nur
 * AES-128 (AN12413 Abschnitt 3.2.3). */
#ifndef SE050_CRYPTO_H
#define SE050_CRYPTO_H

#include <stdint.h>
#include <stddef.h>
#include "osk_result.h"

#define OSK_AES_BLOCK 16u
#define OSK_AES_KEY_LEN 16u

/* Einmal beim Start aufrufen, bevor etwas anderes benutzt wird.
 * Mehrfaches Aufrufen ist erlaubt und tut nichts. */
osk_result_t osk_crypto_init(void);

/* AES-CMAC nach NIST SP 800-38B. mac ist immer 16 Byte lang;
 * wer nur die ersten 8 braucht, schneidet selbst ab. */
osk_result_t osk_aes_cmac(const uint8_t key[OSK_AES_KEY_LEN], const uint8_t *msg, size_t msg_len,
			  uint8_t mac[OSK_AES_BLOCK]);

/* Ein einzelner AES-Block vorwaerts, ohne Betriebsart und ohne Auffuellen.
 * Nur fuer den Startwert der CBC-Kette (Amendment D 6.2.6). */
osk_result_t osk_aes_ecb_encrypt_block(const uint8_t key[OSK_AES_KEY_LEN],
				       const uint8_t in[OSK_AES_BLOCK], uint8_t out[OSK_AES_BLOCK]);

/* AES-CBC ohne eigenes Auffuellen - len muss ein Vielfaches von 16 sein.
 * Das Auffuellen nach ISO 9797-1 Verfahren 2 macht se050_scp03.c, weil es
 * zur Nachricht gehoert und nicht zur Verschluesselung. */
osk_result_t osk_aes_cbc_encrypt(const uint8_t key[OSK_AES_KEY_LEN],
				 const uint8_t iv[OSK_AES_BLOCK], const uint8_t *in, uint8_t *out,
				 size_t len);
osk_result_t osk_aes_cbc_decrypt(const uint8_t key[OSK_AES_KEY_LEN],
				 const uint8_t iv[OSK_AES_BLOCK], const uint8_t *in, uint8_t *out,
				 size_t len);

/* Zufall vom Wirt - fuer die eigene Zufallszahl beim Kanalaufbau.
 * Der Zufall des SE050 kommt nicht in Frage: wer den Kanal aufbaut, kann
 * dem Gegenueber noch nicht glauben. */
osk_result_t osk_random(uint8_t *out, size_t len);

/* Puffer sicher leeren. Ein blosses memset() darf der Uebersetzer
 * wegstreichen, wenn danach nicht mehr gelesen wird - diese Fassung nicht. */
void osk_wipe(void *p, size_t len);

/* Zwei Puffer in gleichbleibender Zeit vergleichen. 0 bei Gleichheit.
 * Pflicht beim Pruefen von MAC und Kryptogramm: ein frueher Abbruch verraet,
 * wie viele Bytes stimmten. */
int osk_memeq_ct(const void *a, const void *b, size_t len);

#endif
