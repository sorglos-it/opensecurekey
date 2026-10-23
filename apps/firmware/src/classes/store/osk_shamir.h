/* Aufteilung eines Geheimnisses in Teile (Shamir).
 *
 * Gedacht fuer den Notfall: der Schluessel der Sicherungsdatei wird in n Teile
 * geteilt, von denen k genuegen. Die Teile gehen in verschlossene Umschlaege
 * an Angehoerige. Ein einzelner Umschlag verraet nichts - nicht ein Bit.
 *
 * Gerechnet wird im Koerper GF(256) mit demselben Polynom wie AES. Das ist
 * Schulmathematik ueber endlichen Koerpern und keine Verschluesselung, darum
 * steht es hier selbst geschrieben und nicht im Krypto-Anschluss.
 *
 * Umsetzung: src/classes/store/osk_shamir.c */
#ifndef OSK_SHAMIR_H
#define OSK_SHAMIR_H

#include <stdint.h>
#include <stddef.h>
#include "osk_result.h"
#include "osk_crypto.h"

#define OSK_SHARE_MIN_K     2u
#define OSK_SHARE_MAX_N    16u
#define OSK_SHARE_SECRET   OSK_KEY_LEN   /* 32 Byte */
#define OSK_SHARE_DIGEST    4u

/* Ein Teil. Wird als Text ausgedruckt (osk_shamir_share_text) und nie
 * elektronisch aufbewahrt. */
typedef struct {
	uint8_t version;                     /* 1 */
	uint8_t k;                           /* wie viele Teile noetig sind */
	uint8_t n;                           /* wie viele es insgesamt gibt */
	uint8_t index;                        /* 1..255, nie 0 */
	uint8_t y[OSK_SHARE_SECRET];
	uint8_t digest[OSK_SHARE_DIGEST];    /* Probe auf das ganze Geheimnis */
} osk_share_t;

/* Teilt das Geheimnis. Braucht Zufall aus osk_crypto_random. */
osk_result_t osk_shamir_split(const uint8_t secret[OSK_SHARE_SECRET],
			      uint8_t k, uint8_t n, osk_share_t *out);

/* Setzt es wieder zusammen. Zu wenige oder falsche Teile ergeben
 * OSK_ERR_DENIED - nie ein falsches Geheimnis, das als richtig durchgeht. */
osk_result_t osk_shamir_combine(const osk_share_t *shares, size_t count,
				uint8_t secret_out[OSK_SHARE_SECRET]);

/* Ein Teil als lesbarer Text fuer den Umschlag, etwa
 * "OSK-1-2-3-A4F1...-9C2B". cap muss mindestens OSK_SHARE_TEXT_MAX sein. */
#define OSK_SHARE_TEXT_MAX 96u
osk_result_t osk_shamir_share_text(const osk_share_t *share, char *out,
				   size_t cap);
osk_result_t osk_shamir_share_parse(const char *text, osk_share_t *out);

#endif
