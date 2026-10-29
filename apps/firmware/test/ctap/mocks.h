/* Attrappen fuer SE050, Anzeige, Tresor und Transport.
 *
 * Es ist nichts am Geraet pruefbar - die Platine ist noch nicht gebaut.
 * Diese Attrappen ersetzen darum alles, was der CTAP-Kern nach aussen
 * aufruft. Sie rechnen DETERMINISTISCH, aber sie rechnen NICHT echtes
 * P-256: der Chip tut das spaeter. Was hier geprueft wird, ist der Aufbau
 * der Nachrichten und der Ablauf, nicht die Kurvenarithmetik.
 *
 * Wo eine Attrappe etwas anders macht als der Chip, steht es dabei. */
#ifndef MOCKS_H
#define MOCKS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"

/* Alles auf Anfang: Grundgeheimnis, Tresor, Anzeige, Zaehler. */
void mock_reset_all(void);

/* ---- Anzeige ----------------------------------------------------------- */
/* Was osk_ui_confirm antworten soll. */
void mock_ui_set_confirm(osk_result_t rc);
/* Was osk_ui_verify_user antworten soll. */
void mock_ui_set_verify(osk_result_t rc, bool verified);
/* Wie oft gefragt wurde, und was zuletzt auf dem Display stand. */
unsigned mock_ui_confirm_count(void);
const char *mock_ui_last_subject(void);
const char *mock_ui_last_detail(void);
int mock_ui_last_kind(void);

/* ---- SE050 ------------------------------------------------------------- */
/* Dieselbe Rechnung wie die ECDH-Attrappe im Chipersatz: damit kann die
 * Pruefung die Seite des Rechners spielen. */
void mock_se050_ecdh_expect(const uint8_t peer_pub[64], uint8_t out[32]);
/* Ableitung wie osk_se050_hkdf, fuer Vergleiche in den Pruefungen. */
void mock_se050_hkdf_expect(const uint8_t *info, size_t info_len, uint8_t *out, size_t out_len);
/* PIN von Hand setzen, ohne den ClientPIN-Weg. */
void mock_se050_set_pin(const uint8_t *pin_hash16);
void mock_se050_set_tries(uint8_t tries);
uint8_t mock_se050_tries(void);
/* Zufall vorhersagbar machen: derselbe Startwert, dieselbe Folge. */
void mock_se050_seed_random(uint32_t seed);

/* ---- Tresor ------------------------------------------------------------ */
size_t mock_store_count(void);
void mock_store_set_full(bool full);
/* Wie oft ein Eintrag geoeffnet wurde. Das ist der Punkt der Kennung:
 * suchen darf nichts oeffnen, was nicht gemeint ist. */
unsigned mock_store_reveal_count(void);

/* ---- Signaturzaehler --------------------------------------------------
 *
 * Der Zaehler des Tresors (osk_store_sigcount_next/_get) liegt in einer
 * DATEI, nicht in einer Variablen. Das ist der Punkt: eine Variable ueberlebt
 * keinen Neustart, und genau das ist der Fehler, der behoben werden sollte.
 * Eine Pruefung, die den Zaehler nur innerhalb eines Laufs ansieht, kann
 * nicht zeigen, ob er im Flash angekommen ist.
 *
 * mock_reset_all() loescht die Datei NICHT - ein Aus- und Einstecken loescht
 * den Flash auch nicht. Wer wirklich bei null anfangen will, sagt es. */
void mock_sigcount_forget(void);
/* Wo die Datei liegt. Ohne Aufruf: OSK_SIGCOUNT_FILE aus der Umgebung, sonst
 * "osk-sigcount.bin" im laufenden Verzeichnis. */
void mock_sigcount_file(const char *path);
/* Jeden Zugriff auf den Zaehler scheitern lassen - fuer die Pruefung, dass
 * dann NICHT signiert wird. */
void mock_sigcount_set_broken(bool broken);

/* ---- Uhr --------------------------------------------------------------- */
void mock_clock_set(uint64_t ms);
void mock_clock_advance(uint64_t ms);

#endif
