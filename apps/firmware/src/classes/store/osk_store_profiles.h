/* Profile und Kennzeichen - was osk_store.h offen laesst.
 *
 * Jedes Profil hat seine eigene PIN. Die PIN wird im SE050 geprueft (dort
 * liegt auch der Fehlversuchszaehler), und zusaetzlich geht ihr Abdruck in die
 * Schluesselableitung des Profils ein. Darum kann die PIN eines Profils die
 * Daten eines anderen nicht aufschliessen: der Schluessel waere falsch, und
 * der Pruefwert der Verschluesselung schlaegt an.
 *
 * Umsetzung: src/classes/store/store_items.c */
#ifndef OSK_STORE_PROFILES_H
#define OSK_STORE_PROFILES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"
#include "osk_store.h"

#define OSK_PIN_MIN 6 /* Zeichen, nicht Ziffern */
#define OSK_PIN_MAX 64

/* Richtet ein Profil ein. Scheitert, wenn es schon eingerichtet ist. */
osk_result_t osk_store_profile_create(uint8_t profile, const uint8_t *pin, size_t pin_len);

/* Schliesst ein Profil auf. OSK_ERR_DENIED heisst: falsche PIN. */
osk_result_t osk_store_profile_unlock(uint8_t profile, const uint8_t *pin, size_t pin_len);

void osk_store_profile_lock(uint8_t profile);
bool osk_store_profile_is_unlocked(uint8_t profile);
bool osk_store_profile_exists(uint8_t profile);

/* Kennzeichen eines Eintrags: gespiegelt, nur gekoppelte Rechner,
 * schreibgeschuetzt (OSK_FLAG_* in osk_vault_format.h). */
osk_result_t osk_store_item_flags(osk_item_id_t id, uint8_t *flags_out);
osk_result_t osk_store_item_set_readonly(osk_item_id_t id, bool on);

/* Der oeffentliche Teil eines Eintrags - das Gegenstueck zu
 * osk_store_public() in osk_store.h, das ihn herausgibt.
 *
 * Er steht hier und nicht im gemeinsamen Kopf, weil osk_store_add() kein Feld
 * dafuer hat; dasselbe gilt fuer den Schreibschutz darueber. Wer einen
 * Schluessel einliest, legt ihn mit osk_store_add() ab und traegt den
 * oeffentlichen Teil danach hier nach - ein Schritt, zwei Aufrufe.
 *
 * Geraten wird nichts: der Tresor liest keine Schluesselformate (OpenSSH,
 * X.509, OpenPGP), um den oeffentlichen Teil aus dem Geheimnis zu schaelen.
 * Das waere Formatarbeit an fremden Daten mitten im Sicherheitskern, und sie
 * muesste den Platz des Geheimnisses lesen - genau das, was osk_store_public()
 * nicht tun darf.
 *
 * Laenge 0 nimmt den oeffentlichen Teil wieder weg. Ein neues Geheimnis
 * (osk_store_update mit secret) nimmt ihn ebenfalls weg: er gehoerte zum alten
 * Schluessel, und ein falscher oeffentlicher Schluessel ist schlimmer als
 * keiner. */
osk_result_t osk_store_item_set_public(osk_item_id_t id, const uint8_t *pub, size_t len);

/* Liegt der private Schluessel im SE050, kommt der oeffentliche Teil aus dem
 * Chip. Hier wird vermerkt, unter welcher Kennung er dort liegt; 0 nimmt den
 * Vermerk wieder weg. */
osk_result_t osk_store_item_set_se050_key(osk_item_id_t id, uint32_t key_id);

/* HOTP zaehlt bei jeder Benutzung weiter. Der Zaehler gehoert zum Eintrag
 * und wird wie alles andere atomar geschrieben. */
osk_result_t osk_store_item_counter(osk_item_id_t id, uint64_t *out);
osk_result_t osk_store_item_counter_next(osk_item_id_t id, uint64_t *out);

/* ---- Wisch-PIN -------------------------------------------------------
 *
 * Eine zweite PIN je Profil, deren Eingabe den ganzen Tresor loescht. Sie
 * steht hier und nicht in osk_store.h, weil der gemeinsame Kopf vom Master
 * festgelegt ist und kein Feld dafuer hat - wie der Schreibschutz und der
 * oeffentliche Teil darueber.
 *
 * Abschaltbar: ohne gesetzte Wisch-PIN gibt es keine, und dann verhaelt
 * sich das Geraet genau wie bisher.
 *
 * Drei Dinge, die zum Vertrag gehoeren:
 *
 *  - Beim Aufschliessen meldet die Wisch-PIN OSK_ERR_DENIED, dasselbe wie
 *    eine falsche PIN. Wer das Loeschen erzwingen will, soll nicht sehen,
 *    dass es geschehen ist, und wer sie nicht kennt, soll nicht merken,
 *    dass es eine gibt.
 *  - Sie wird vor der Chip-PIN geprueft. Sonst verbrauchte jede Eingabe
 *    einen Fehlversuch im Chip, und nach ein paar Malen liesse sich das
 *    Geraet nicht einmal mehr loeschen.
 *  - Sie darf nicht dieselbe sein wie die echte PIN des Profils; das waere
 *    ein Tresor, der sich beim Aufschliessen selbst loescht. Der Versuch
 *    ergibt OSK_ERR_ARG.
 *
 * Was das Loeschen NICHT kann: den Rueckdrehschutz aufheben. Der Zaehler im
 * SE050 bleibt stehen, und der frische Tresor faengt ueber ihm an. Sonst
 * waere die Wisch-PIN selbst der Weg zurueck - loeschen, alten Stand
 * zurueckspielen, fertig. */
osk_result_t osk_store_wipe_pin_set(uint8_t profile, const uint8_t *pin, size_t pin_len);
osk_result_t osk_store_wipe_pin_clear(uint8_t profile);
bool osk_store_wipe_pin_is_set(uint8_t profile);

#endif
