/* 256-Bit-Zahlen, genau so viel wie die Geldboerse braucht.
 *
 * Warum eigene Zahlen: BIP32 rechnet den abgeleiteten Schluessel aus
 * (IL + k_parent) mod n, und Adressen brauchen den oeffentlichen Punkt. Das
 * ist Rechnen auf 256 Bit, und weder der SE050 noch PSA Crypto bieten es an.
 *
 * Feste Breite, kein dynamischer Speicher, keine Rekursion. Die Limbs sind
 * 32 Bit breit und stehen niederwertig zuerst - das passt zum Cortex-M4.
 *
 * Diese Ebene weiss nichts von Kurven. secp256k1 steht in wallet_ec.h. */
#ifndef WALLET_BN_H
#define WALLET_BN_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define OSK_BN_LIMBS 8
#define OSK_BN_BYTES 32

typedef struct {
	uint32_t v[OSK_BN_LIMBS]; /* v[0] ist das niederwertigste Limb */
} osk_bn_t;

/* Doppelte Breite - das Ergebnis einer Multiplikation, vor der Reduktion. */
typedef struct {
	uint32_t v[2 * OSK_BN_LIMBS];
} osk_bn2_t;

void osk_bn_zero(osk_bn_t *r);
void osk_bn_set_u32(osk_bn_t *r, uint32_t x);
void osk_bn_copy(osk_bn_t *r, const osk_bn_t *a);

/* 32 Byte, hoechstwertiges Byte zuerst - so stehen Schluessel und Hashes
 * ueberall in den Spezifikationen. */
void osk_bn_from_be(osk_bn_t *r, const uint8_t in[OSK_BN_BYTES]);
void osk_bn_to_be(const osk_bn_t *a, uint8_t out[OSK_BN_BYTES]);

bool osk_bn_is_zero(const osk_bn_t *a);
bool osk_bn_is_odd(const osk_bn_t *a);
bool osk_bn_bit(const osk_bn_t *a, unsigned index);

/* -1, 0 oder 1. Laeuft immer ueber alle Limbs - keine fruehe Rueckkehr,
 * damit die Laufzeit nichts ueber die Zahl verraet. */
int osk_bn_cmp(const osk_bn_t *a, const osk_bn_t *b);

uint32_t osk_bn_add(osk_bn_t *r, const osk_bn_t *a, const osk_bn_t *b); /* Uebertrag */
uint32_t osk_bn_sub(osk_bn_t *r, const osk_bn_t *a, const osk_bn_t *b); /* Borger */

void osk_bn_add_mod(osk_bn_t *r, const osk_bn_t *a, const osk_bn_t *b, const osk_bn_t *m);
void osk_bn_sub_mod(osk_bn_t *r, const osk_bn_t *a, const osk_bn_t *b, const osk_bn_t *m);

void osk_bn_mul(osk_bn2_t *r, const osk_bn_t *a, const osk_bn_t *b);

/* Allgemeine Reduktion einer 512-Bit-Zahl: bitweise, 512 Schritte. Langsam,
 * aber richtig fuer jeden Modul - gebraucht fuer die Ordnung n, die keine
 * schnelle Form hat. Fuer den Koerper p steht die schnelle Fassung in
 * wallet_ec.c. */
void osk_bn_mod2(osk_bn_t *r, const osk_bn2_t *a, const osk_bn_t *m);

void osk_bn_mul_mod(osk_bn_t *r, const osk_bn_t *a, const osk_bn_t *b, const osk_bn_t *m);

/* Kehrwert nach dem kleinen Satz von Fermat: a^(m-2) mod m. m muss prim
 * sein - fuer n und p der Kurve gilt das. a = 0 ergibt 0. */
void osk_bn_inv_mod(osk_bn_t *r, const osk_bn_t *a, const osk_bn_t *m);

void osk_bn_wipe(osk_bn_t *a);

#endif
