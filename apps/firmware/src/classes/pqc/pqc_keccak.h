/* Keccak-f[1600] und was ML-KEM davon braucht: SHA3-256, SHA3-512,
 * SHAKE128 und SHAKE256.
 *
 * Warum eigener Code und nicht die Hashfunktionen des SDK: ML-KEM braucht
 * SHAKE128 als Strom, aus dem gezogen wird, bis genug Werte angenommen
 * wurden (Verwerfungsauswahl). Eine Schnittstelle, die nur "Hash ueber
 * diesen Puffer" kann, reicht dafuer nicht. Ausserdem gilt im Projekt
 * "Software ohne Abhaengigkeiten", und der Pruefsatz laeuft am PC, wo es
 * kein PSA Crypto gibt.
 *
 * Gebaut nach FIPS 202 (SHA-3). Keine fremde Umsetzung als Vorlage.
 *
 * Kein dynamischer Speicher. Der Zustand liegt beim Aufrufer, und
 * osk_keccak_wipe() loescht ihn.
 */
#ifndef PQC_KECCAK_H
#define PQC_KECCAK_H

#include <stddef.h>
#include <stdint.h>

/* Saugrate in Byte je Aufruf der Permutation. 1600 Bit Zustand minus
 * zweimal die Sicherheitsstufe. Eine Zahl nur an einer Stelle: die
 * Blockgroessen unten leiten sich hier ab. */
#define OSK_KECCAK_STATE_BYTES 200

#define OSK_SHA3_256_RATE 136
#define OSK_SHA3_512_RATE 72
#define OSK_SHAKE128_RATE 168
#define OSK_SHAKE256_RATE 136

/* Bereichsbyte nach FIPS 202 Anhang B. */
#define OSK_KECCAK_PAD_SHA3 0x06u
#define OSK_KECCAK_PAD_SHAKE 0x1fu

struct osk_keccak {
	uint64_t lane[25];
	size_t rate; /* Byte je Block */
	size_t pos;  /* Fuellstand im laufenden Block */
	uint8_t pad; /* Bereichsbyte */
	int squeezing;
};

/* Strom beginnen. rate ist eine der OSK_*_RATE, pad eines der
 * OSK_KECCAK_PAD_*. */
void osk_keccak_init(struct osk_keccak *st, size_t rate, uint8_t pad);

/* Nachricht stueckweise hineingeben. Beliebig oft, auch mit len 0. */
void osk_keccak_absorb(struct osk_keccak *st, const uint8_t *in, size_t len);

/* Beliebig viel herausziehen, stueckweise. Der erste Aufruf schliesst die
 * Nachricht ab; danach ist absorb verboten (und wird ignoriert). */
void osk_keccak_squeeze(struct osk_keccak *st, uint8_t *out, size_t len);

/* Zustand loeschen. Nach jedem Vorgang aufzurufen - der Zustand traegt
 * Reste des Geheimnisses. */
void osk_keccak_wipe(struct osk_keccak *st);

/* Die vier Festlaengen, die ML-KEM benutzt. */
void osk_sha3_256(uint8_t out[32], const uint8_t *in, size_t len);
void osk_sha3_512(uint8_t out[64], const uint8_t *in, size_t len);
void osk_shake256(uint8_t *out, size_t out_len, const uint8_t *in, size_t len);

#endif
