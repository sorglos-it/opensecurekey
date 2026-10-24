/* BIP39: aus Zufall werden Woerter, aus Woertern wird der Seed.
 *
 * Der Seed liegt danach im Tresor (OSK_ITEM_SEED), nicht im SE050: der
 * SE05x kann Schluessel nicht intern ableiten (belegt in docs/se050.md),
 * also muss das Grundgeheimnis dort liegen, wo die Firmware es fuer die
 * Ableitung erreicht.
 *
 * Grenzen dieser Fassung, bewusst und geprueft:
 *  - nur die englische Wortliste. Sie ist die einzige, die jede Geldboerse
 *    versteht; eine zweite Liste wuerde Woerter erzeugen, die kein anderes
 *    Geraet wieder einlesen kann.
 *  - Mnemonik und Passphrase nur in ASCII. BIP39 verlangt fuer andere
 *    Zeichen eine Unicode-Normalform (NFKD), und die falsch zu machen heisst
 *    einen anderen Seed - also lieber ablehnen als raten. */
#ifndef WALLET_BIP39_H
#define WALLET_BIP39_H

#include <stdint.h>
#include <stddef.h>
#include "osk_result.h"

#define OSK_BIP39_WORDS 2048
#define OSK_BIP39_MIN_ENTROPY 16 /* 12 Woerter */
#define OSK_BIP39_MAX_ENTROPY 32 /* 24 Woerter */
#define OSK_BIP39_MAX_WORDS 24
/* 24 Woerter mit hoechstens 8 Buchstaben, 23 Leerzeichen, Nullbyte */
#define OSK_BIP39_MAX_TEXT 224
#define OSK_BIP39_SEED_LEN 64

extern const char *const osk_bip39_english[OSK_BIP39_WORDS];

/* Wortnummer oder -1. Der Vergleich laeuft ueber genau `len` Zeichen. */
int osk_bip39_word_index(const char *word, size_t len);

/* Woerter aus vorgegebener Entropie. ent_len muss 16, 20, 24, 28 oder 32
 * sein - also 12, 15, 18, 21 oder 24 Woerter. */
osk_result_t osk_bip39_from_entropy(const uint8_t *ent, size_t ent_len, char *out, size_t cap,
				    size_t *len_out);

/* Woerter aus dem Zufall des SE050. `words` ist 12, 15, 18, 21 oder 24.
 * Der Zufall kommt aus dem Chip, nicht aus der Firmware - das ist der
 * einzige Zufall auf diesem Geraet, dem zu trauen ist. */
osk_result_t osk_bip39_generate(unsigned words, char *out, size_t cap, size_t *len_out);

/* Prueft Wortzahl, jedes Wort und die Pruefsumme. OSK_ERR_FORMAT nennt
 * keinen Grund im Rueckgabewert - der Grund steht im Protokoll, damit die
 * Meldung dem Nutzer nicht verraet, welches Wort falsch ist. */
osk_result_t osk_bip39_check(const char *mnemonic);

osk_result_t osk_bip39_to_entropy(const char *mnemonic, uint8_t *out, size_t cap, size_t *len_out);

/* Der Seed. `passphrase` darf NULL sein (dann leer). Prueft die Pruefsumme
 * NICHT - das macht osk_bip39_check(); so laesst sich auch eine Mnemonik
 * einlesen, die ein anderes Geraet ohne Pruefsumme geschrieben hat, aber
 * nur, wenn der Aufrufer das ausdruecklich will. */
osk_result_t osk_bip39_to_seed(const char *mnemonic, const char *passphrase,
			       uint8_t seed[OSK_BIP39_SEED_LEN]);

#endif
