/* Krypto-Attrappe fuer den Test am PC. KEINE Sicherheit.
 *
 * Sie erfuellt osk_crypto.h mit einer durchschaubaren Rechnung, damit sich
 * der AUFBAU des Tresors pruefen laesst: Reihenfolge der Schreibvorgaenge,
 * Atomizitaet, Nonce-Verwaltung, Wanderung, Sicherung, Uebertragung.
 *
 * Was sie NICHT prueft: ob AES-256-GCM, SHA-256, HKDF und Argon2id richtig
 * sind. Die kommen auf dem Geraet aus dem nRF Connect SDK (osk_crypto_psa.c)
 * und werden dort geprueft, nicht hier.
 *
 * Zusaetzlich fuehrt sie ein Verzeichnis aller benutzten Nonces. Damit laesst
 * sich beweisen, was sonst niemand beweisen kann: dass keine Nonce zweimal
 * benutzt wird - auch nicht nach einem Stromausfall. */
#ifndef FAKE_CRYPTO_H
#define FAKE_CRYPTO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

void fake_crypto_init(void);

/* Zufall ist im Test eine feste Folge, damit jeder Lauf gleich verlaeuft. */
void fake_crypto_seed(uint32_t seed);

/* Nonce-Verzeichnis. Gezaehlt wird jede Nonce, mit der verschluesselt wurde. */
void fake_crypto_nonce_reset(void);
size_t fake_crypto_nonce_count(void);

/* Wie oft dieselbe Nonce mit demselben Schluessel benutzt wurde. Muss 0 sein. */
size_t fake_crypto_nonce_dups(void);

/* Wie oft dieselbe Nonce ueberhaupt benutzt wurde, egal mit welchem
 * Schluessel. Im Tresor selbst (Zweck 1 bis 3) muss auch das 0 sein. */
size_t fake_crypto_vault_nonce_dups(void);

/* Wie viel entschluesselt wurde: Aufrufe und Bytes. Das ist die ARBEIT einer
 * Suche, und die laesst sich hier ehrlich messen - die LAUFZEIT dieser
 * Attrappe sagt ueber das Geraet nichts. */
void fake_crypto_open_reset(void);
size_t fake_crypto_open_calls(void);
size_t fake_crypto_open_bytes(void);

#endif
