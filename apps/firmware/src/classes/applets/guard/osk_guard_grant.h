/* Die Freischaltung fuer den Serverbetrieb (Befehl 17 `session`).
 *
 * Ohne sie verlangt das Geraet je Unterschrift einen Tastendruck, und dann
 * steht ein Server still - nachts steht niemand im Serverraum. Mit ihr kann
 * alles, was auf diesem Server laeuft, unterschreiben, solange sie gilt.
 * Beide Saetze sind wahr; darum ist sie eingezaeunt (docs/dongle.md):
 *
 *   ab Werk aus      ein leerer Zustand schaltet nichts frei; es gibt keine
 *                    Einstellung, die sie still einschaltet
 *   ausdruecklich    Einschalten verlangt eine Freigabe am Geraet
 *   benannte Keys    eine Freischaltung nennt ihre Schluessel; jeder andere
 *                    fragt weiter nach
 *   laeuft ab        von selbst, hoechstens OSK_GUARD_TTL_MAX Sekunden
 *   protokolliert    die Freischaltung selbst und jede Benutzung
 *   nicht aus der Bibliothek erreichbar
 *                    das prueft osk_guard_fw.c am Transportweg, nicht hier
 *
 * Der Zustand liegt absichtlich nur im RAM: ein Stromausfall oder ein
 * Abziehen des Sticks loescht ihn. Eine Freischaltung, die ein Neustart
 * ueberlebt, wuerde die Zeile "laeuft von selbst ab" aushoehlen.
 */
#ifndef OSK_GUARD_GRANT_H
#define OSK_GUARD_GRANT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"
#include "osk_guard_msg.h"

/* So viele Schluessel darf eine Freischaltung nennen. Klein mit Absicht:
 * "einen Schluessel nennen" ist der empfohlene Betrieb. */
#define OSK_GUARD_GRANT_MAX 4u

/* Obergrenze der Laufzeit in Sekunden - 24 Stunden, wie in docs/dongle.md
 * zugesagt. Laenger wird nicht gekuerzt, sondern abgelehnt: eine still
 * gekuerzte Zeit laesst den Betreiber glauben, er haette mehr. */
#define OSK_GUARD_TTL_MAX 86400u

/* Alles aus. Auch der Startzustand des Geraets - nach dem Einschalten ist
 * nichts freigeschaltet, ohne dass jemand etwas abschalten muesste. */
void osk_guard_grant_clear(void);

/* Schaltet die genannten Schluessel fuer `ttl` Sekunden frei. Der Aufrufer
 * hat die Freigabe am Geraet bereits eingeholt.
 * `now_ms` ist die Geraetezeit; aus ihr entsteht der Ablauf.
 * Fehler: OSK_ERR_ARG bei leerer Liste, zu vielen Namen, krummem Namen,
 * ttl 0 oder ttl > OSK_GUARD_TTL_MAX. Dann bleibt der alte Zustand. */
osk_result_t osk_guard_grant_set(const char *const *labels, const size_t *label_lens, size_t count,
				 uint32_t ttl_s, uint64_t now_ms);

/* Gilt fuer diesen Schluessel gerade eine Freischaltung? Abgelaufene
 * Eintraege werden dabei entfernt. */
bool osk_guard_grant_allows(const char *label, size_t label_len, uint64_t now_ms);

/* Zustand fuer die Antwort auf `ask`: wie viele Schluessel noch gelten und
 * wie viele Sekunden der laengste noch laeuft. */
typedef struct {
	bool on;
	size_t count;
	uint32_t ttl_left_s;
} osk_guard_grant_state_t;

void osk_guard_grant_state(osk_guard_grant_state_t *out, uint64_t now_ms);

/* Name eines noch gueltigen Eintrags, 0 <= index < state.count. Die
 * Reihenfolge ist die des Einschaltens. NULL, wenn es den nicht gibt. */
const char *osk_guard_grant_label(size_t index, size_t *len_out, uint64_t now_ms);

#endif
