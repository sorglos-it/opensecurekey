/* Der Signaturzaehler: EINE Stelle, an der entschieden wird, woher die Zahl
 * kommt, die in jedem authData steht.
 *
 * WOZU ER DA IST: Ein Dienst erkennt einen geklonten Schluessel daran, dass
 * der Zaehler nicht mehr steigt. Er muss bei jeder Anmeldung groesser sein
 * als bei der vorigen - ueber Einstecken und Abziehen hinweg.
 *
 * WAS VORHER FALSCH WAR: Der Zaehler lag im RAM (osk_ctap_state_t.sign_count)
 * und fing nach jedem Einstecken bei null an. Beim zweiten Einstecken sah
 * das Geraet fuer den Dienst aus wie ein Klon. Behoben am 29.10.2026.
 *
 * WARUM EINE EINZIGE ZAHL FUER ALLE ZUGAENGE (und nicht eine je Zugang):
 * Nicht auffindbare Nachweise werden ABGELEITET und nirgends gespeichert -
 * ihre Zahl ist unbegrenzt, und sie haben keinen Tresoreintrag, an dem ein
 * eigener Zaehler haengen koennte. Ein Zaehler je Zugang ginge also nur fuer
 * die auffindbaren, und die Mischung waere gefaehrlich: faellt ein Zugang
 * einmal auf den gemeinsamen Zaehler zurueck, kann die Zahl KLEINER werden
 * als zuvor - und genau das ist das Kennzeichen eines Klons. Eine einzige,
 * geraeteweit steigende Zahl ist fuer jeden einzelnen Zugang ebenfalls
 * streng steigend und kann das nicht.
 *
 * Der Preis steht im Bericht und gehoert ins Handbuch: zwei Dienste koennen
 * an der Zahl ablesen, wie oft das Geraet insgesamt benutzt wurde, und daran
 * erkennen, dass sie dasselbe Geraet sehen. CTAP erlaubt diese Bauart
 * ausdruecklich (Abschnitt 6.1.1, "may ... a global signature counter").
 *
 * VOR DEM SIGNIEREN, nicht danach: siehe osk_store.h. */
#ifndef OSK_SIGCOUNT_H
#define OSK_SIGCOUNT_H

#include <stdint.h>
#include "osk_result.h"
#include "osk_store.h"

/* Der Platz, unter dem der gemeinsame Zaehler im Tresor liegt. Er ist KEIN
 * Tresoreintrag - der Tresor gibt Kennungen ab 1 aus (osk_store.h), die 0
 * ist damit frei und bezeichnet hier "der Zaehler des Geraets".
 *
 * Das ist die eine Stelle, an der der Vertrag osk_store_sigcount_next() auf
 * unsere Bauart trifft: er nimmt eine Eintragskennung, und fuer einen
 * geraeteweiten Zaehler gibt es keine.
 *
 * ERLEDIGT am 29.10.2026, entschieden vom Master, umgesetzt in Strang D.
 * Kurz vorher war das hier ein Fehler am Geraet: der Tresor suchte auch
 * fuer die Kennung 0 einen Indexsatz, fand keinen und meldete
 * OSK_ERR_NOT_FOUND - CTAP haette gar nicht signiert. Am PC fiel es nicht
 * auf, weil die Attrappe in test/ctap/mocks.c ihren eigenen geraeteweiten
 * Zaehler fuehrt und die Kennung uebergeht; sie prueft diese Seite des
 * Vertrags, nicht jene.
 *
 * Jetzt gilt im Tresor eine festgelegte Aufteilung, dieselbe wie hier:
 *   Kennung 0           der Zaehler des GERAETS, als sigcount_dev im
 *                       Kopfsatz - fuer alle Nachweise ohne eigenen
 *                       Indexsatz, also die nicht auffindbaren und U2F
 *   jede andere Kennung GENAU dieser eine Eintrag, in seinem Indexsatz
 * Siehe osk_vault_format.h (OSK_SIGCOUNT_DEVICE) und shared/vault-format.md.
 *
 * Was es kostet, steht dort gemessen: je Anmeldung ein Kopfsatz im Flash
 * und eine Anhebung des monotonen Zaehlers im SE050. */
#define OSK_SIGCOUNT_SLOT ((osk_item_id_t)0)

/* Um eins erhoehen, festschreiben und den NEUEN Wert liefern. Wird VOR dem
 * Signieren gerufen.
 *
 * Schlaegt das Festschreiben fehl, schlaegt der ganze Vorgang fehl. Es wird
 * NICHT auf einen Zaehler im RAM zurueckgefallen: das waere wieder der
 * Fehler von oben, nur schwerer zu sehen. */
osk_result_t osk_sigcount_next(uint32_t *value_out);

/* Nur ablesen, ohne zu erhoehen - fuer Anzeige und Pruefung. */
osk_result_t osk_sigcount_get(uint32_t *value_out);

#endif
