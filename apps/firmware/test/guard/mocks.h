/* Attrappen fuer SE050, Tresor, Anzeige, Protokoll und Uhr.
 *
 * Es ist nichts am Geraet pruefbar - die Platine ist nicht gebaut. Diese
 * Attrappen ersetzen alles, was das guard-Applet nach aussen aufruft.
 *
 * Sie rechnen DETERMINISTISCH, aber sie rechnen keine echte Kurve: eine
 * "Unterschrift" ist hier HMAC-SHA256 ueber die Nachricht, zweimal
 * verkettet auf 64 Byte. Das hat die richtige Form und Laenge, nicht den
 * richtigen Wert - echtes Ed25519 macht spaeter der Chip.
 *
 * Fuer die Pruefung der Zwecktrennung reicht das genau aus, und das ist
 * kein Zufall: die Attrappe ist eine Funktion der unterschriebenen
 * BYTEFOLGE. Gleiche Bytefolge gibt dieselbe Unterschrift, verschiedene
 * Bytefolgen verschiedene. Wer also zeigen will, dass eine attest-Antwort
 * nicht als sign-Antwort durchgeht, vergleicht Bytes - und braucht dafuer
 * keine Kurve, sondern nur diese Eigenschaft. Bei echtem Ed25519 gilt sie
 * ebenso (es ist deterministisch, RFC 8032).
 */
#ifndef MOCKS_H
#define MOCKS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"
#include "osk_se050.h"

/* Alles auf Anfang: Chip offen, Grundgeheimnis gesetzt, Tresor offen,
 * Anzeige sagt ja, Protokoll leer, Uhr auf 0. */
void mock_reset_all(void);

/* ---- Anzeige ----------------------------------------------------------- */
void mock_ui_set_confirm(osk_result_t rc);
unsigned mock_ui_confirm_count(void);
const char *mock_ui_last_subject(void);
int mock_ui_last_kind(void);

/* ---- Tresor ------------------------------------------------------------ */
void mock_store_set_open(bool open);

/* ---- Uhr --------------------------------------------------------------- */
void mock_clock_set(uint64_t ms);
void mock_clock_advance(uint64_t ms);

/* ---- SE050 ------------------------------------------------------------- */

/* Stellt ein geliefertes Stueck nach, das im Werk auf FIPS gestellt wurde:
 * es lehnt ECDH ab (AN12413 4.10.3.1). */
void mock_se050_set_no_ecdh(bool on);

/* Lehnt den Chip jeden Zugriff ab (Leitung gestoert)? */
void mock_se050_set_offline(bool on);

/* Einen Schluessel von Hand anlegen - das macht sonst die Einrichtung,
 * nicht einer der Befehle 14 bis 18. */
void mock_se050_add_key(uint32_t id, osk_curve_t curve);

/* Geraeteschluessel und Grundgeheimnis anlegen, wie nach der
 * Erstinbetriebnahme. */
void mock_se050_setup_guard(void);

bool mock_se050_has_key(uint32_t id);

/* Was eine korrekte Unterschrift dieses Schluessels ueber genau diese
 * Bytefolge waere. Damit prueft ein Fall, WAS unterschrieben wurde - nicht
 * nur, dass irgendetwas zurueckkam. */
void mock_se050_expected_sig(uint32_t id, const uint8_t *msg, size_t len, uint8_t out[64]);

/* Das Grundgeheimnis der Attrappe. Im Chip ist es unlesbar; hier liegt es
 * offen, damit ein Fall pruefen kann, dass `derive` es NICHT herausgibt. */
void mock_se050_root_secret(uint8_t out[32]);

/* ---- Protokoll --------------------------------------------------------- */
unsigned mock_log_count(void);
/* Anlass des n-ten Eintrags, oder 0. */
uint16_t mock_log_what(unsigned index);

/* Wer den Satz ausgeloest hat (Feld asked_by), und wann (Geraetezeit in
 * Sekunden). NULL bzw. 0, wenn es den Satz nicht gibt. */
const char *mock_log_who(unsigned index);
uint64_t mock_log_when(unsigned index);

#endif
