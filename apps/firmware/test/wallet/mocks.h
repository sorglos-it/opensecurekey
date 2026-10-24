/* Attrappen fuer SE050, Tresor und Anzeige - nur fuer die Pruefungen am PC.
 *
 * Sie sind ausdruecklich NICHT sicher:
 *  - der "Zufall" ist ein Zaehler, damit jeder Lauf dieselben Woerter ergibt
 *  - die Signatur rechnet mit einem aus Schluessel und Hash abgeleiteten k;
 *    das ist nach RFC 6979 gedacht, aber nicht dieselbe Rechnung
 *  - der Tresor liegt im Arbeitsspeicher und ist nicht verschluesselt
 *
 * Was sie koennen muessen: echte secp256k1-Signaturen liefern, damit der
 * Weg "ableiten -> unterschreiben -> nachpruefen" wirklich durchlaeuft.
 * Ein Nachbau, der nur Bytes zurueckgibt, wuerde die Nachpruefung im
 * Signierer abschalten - und genau die ist der Riegel. */
#ifndef MOCKS_H
#define MOCKS_H

#include <stdint.h>
#include <stdbool.h>
#include "osk_result.h"
#include "osk_se050.h"
#include "osk_store.h"
#include "osk_ui.h"

/* --- SE050 --------------------------------------------------------------- */

/* Welche Kurven der Chip "kann". Vorgabe: P256 und secp256k1, ohne
 * Ed25519 - genau der Fall eines ab Werk beschnittenen SE051. */
void mock_se050_set_curves(uint32_t mask);
/* Naechste Antwort von osk_se050_sign erzwingen (OSK_OK = normal rechnen). */
void mock_se050_fail_sign(osk_result_t result);
/* Die Signatur absichtlich verfaelschen - prueft die Nachpruefung. */
void mock_se050_corrupt_sign(bool on);
void mock_se050_reset(void);
unsigned mock_se050_imports(void);
unsigned mock_se050_deletes(void);

/* --- Anzeige ------------------------------------------------------------- */

void mock_ui_answer(osk_result_t result);
void mock_ui_reset(void);
unsigned mock_ui_calls(void);
const char *mock_ui_subject(void);
const char *mock_ui_detail(void);
const char *mock_ui_status_key(void);
osk_ask_t mock_ui_kind(void);

/* --- Tresor -------------------------------------------------------------- */

void mock_store_reset(void);
/* Antwort der Freigabe in osk_store_reveal. */
void mock_store_deny(bool on);
unsigned mock_store_reveals(void);

#endif
