/* APDU-Schicht: Befehle bauen, hinschicken, Antwort pruefen.
 *
 * Darunter liegt T=1' (se050_t1.h) und, innerhalb einer Sitzung, SCP03
 * (se050_scp03.h). Darueber liegt se050.c mit den Vertragsfunktionen aus
 * osk_se050.h.
 *
 * SITZUNGEN, und warum das hier so aussieht: Nach AN12413 Abschnitt 3.6.3.2
 * laeuft eine AESKey-Sitzung IMMER in drei Schritten -
 *   1. CreateSession(Kennung des Anmeldeobjekts)  -> 8-Byte Sitzungskennung
 *   2. ProcessSessionCmd(Kennung, SCPInitializeUpdate)
 *   3. ProcessSessionCmd(Kennung, SCPExternalAuthenticate)
 * und danach steckt JEDER Befehl in einem ProcessSessionCmd. Die SCP03-
 * Sicherung liegt dabei im inneren Befehl, der aeussere Umschlag ist offen.
 * Eine erste Fassung dieses Treibers hat INITIALIZE UPDATE direkt
 * geschickt, ohne CreateSession und ohne Umschlag - das waere am Geraet
 * nicht gelaufen.
 *
 * Der Baustein haelt hoechstens zwei Sitzungen gleichzeitig
 * (AN12413 Tabelle 16, MAX_NUMBER_OF_SESSIONS). Darum ist eine Sitzung hier
 * ein eigener Wert, den der Aufrufer anlegt und wieder schliesst, und kein
 * fester Platz im Treiber.
 *
 * Hier wird ausserdem entschieden, was der Aufrufer vom Baustein erfaehrt:
 * ein Statuswort wird in osk_result_t uebersetzt, nie durchgereicht. Nach
 * aussen gibt es keine SE050-Fehlerwelt. */
#ifndef SE050_APDU_H
#define SE050_APDU_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"
#include "se050_t1.h"
#include "se050_scp03.h"
#include "se050_cmd.h"

#define SE050_SESSION_ID_LEN 8u

/* Eine offene Sitzung: Kennung vom Baustein, dazu der SCP03-Zustand. */
typedef struct {
	uint8_t id[SE050_SESSION_ID_LEN];
	osk_scp03_t scp;
	bool active;
} osk_se050_session_t;

typedef struct {
	osk_se050_t1_t t1;

	/* Die Sitzung des Geraets selbst - steht, solange der Treiber offen
	 * ist. Weitere Sitzungen (die PIN) legt der Aufrufer selbst an. */
	osk_se050_session_t chan;

	bool applet_selected;
} osk_se050_apdu_t;

/* Chip anmelden: T=1' aufsetzen und das IoT-Applet auswaehlen.
 * Eine Sitzung steht danach noch NICHT.
 * version_out darf NULL sein, sonst kommen dort die 7 Byte VersionInfo
 * aus der Antwort des Auswahlbefehls hinein (Tabelle 52). */
osk_result_t osk_se050_apdu_open(osk_se050_apdu_t *a, osk_se050_bus_t *bus, uint8_t *version_out,
				 size_t version_cap, size_t *version_len);

/* Eine AESKey-Sitzung gegen ein Anmeldeobjekt aufbauen (AN12413 3.6.3.2).
 * Ab hier ist der Inhalt jedes Befehls in dieser Sitzung verschluesselt.
 *
 * keys wird nur gelesen; der Aufrufer loescht sie danach selbst.
 * Scheitert der Aufbau, ist s in jedem Fall geschlossen und leer. */
osk_result_t osk_se050_apdu_session_open(osk_se050_apdu_t *a, osk_se050_session_t *s,
					 uint32_t auth_obj_id, const osk_scp03_static_keys_t *keys);

/* Sitzung beim Baustein schliessen und alle Schluessel loeschen.
 * Darf jederzeit aufgerufen werden, auch auf einer nie geoeffneten Sitzung. */
void osk_se050_apdu_session_close(osk_se050_apdu_t *a, osk_se050_session_t *s);

/* Alles schliessen: Sitzung des Geraets, T=1', Schluessel. */
void osk_se050_apdu_close(osk_se050_apdu_t *a);

/* Einen Befehl in einer Sitzung schicken und die TLV-Nutzlast der Antwort
 * holen. Der Befehl wird eingepackt, die Antwort ausgepackt - der Aufrufer
 * merkt davon nichts.
 *
 * s darf NULL sein; dann geht der Befehl OFFEN hinaus. Das ist nur fuer die
 * Schritte vor der ersten Sitzung gedacht.
 *
 * Ein Statuswort ungleich 0x9000 wird zu einem passenden osk_result_t.
 * rsp darf NULL sein, wenn keine Nutzlast erwartet wird. */
osk_result_t osk_se050_apdu_exchange_in(osk_se050_apdu_t *a, osk_se050_session_t *s, uint8_t ins,
					uint8_t p1, uint8_t p2, const uint8_t *payload,
					size_t payload_len, uint8_t *rsp, size_t rsp_cap,
					size_t *rsp_len);

/* Kurzform fuer die Sitzung des Geraets. */
osk_result_t osk_se050_apdu_exchange(osk_se050_apdu_t *a, uint8_t ins, uint8_t p1, uint8_t p2,
				     const uint8_t *payload, size_t payload_len, uint8_t *rsp,
				     size_t rsp_cap, size_t *rsp_len);

/* Statuswort nach osk_result_t. Getrennt herausgezogen, damit die
 * Uebersetzung ohne Bus geprueft werden kann. */
osk_result_t osk_se050_sw_to_result(uint16_t sw);

#endif
