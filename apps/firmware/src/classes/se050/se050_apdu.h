/* APDU-Schicht: Befehle bauen, hinschicken, Antwort pruefen.
 *
 * Darunter liegt T=1' (se050_t1.h) und, sobald der Kanal steht, SCP03
 * (se050_scp03.h). Darueber liegt se050.c mit den Vertragsfunktionen aus
 * osk_se050.h.
 *
 * Hier wird entschieden, was der Aufrufer vom Baustein erfaehrt: ein
 * Statuswort wird in osk_result_t uebersetzt, nie durchgereicht. Nach
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

typedef struct {
	osk_se050_t1_t t1;
	osk_scp03_t scp;
	bool applet_selected;
} osk_se050_apdu_t;

/* Chip anmelden: T=1' aufsetzen und das IoT-Applet auswaehlen.
 * Der Kanal steht danach noch NICHT - dafuer osk_se050_apdu_open_channel().
 * version_out darf NULL sein, sonst kommen dort die 7 Byte VersionInfo
 * aus der Antwort des Auswahlbefehls hinein (Tabelle 52). */
osk_result_t osk_se050_apdu_open(osk_se050_apdu_t *a, osk_se050_bus_t *bus,
				uint8_t *version_out, size_t version_cap,
				size_t *version_len);

/* SCP03 aufbauen: INITIALIZE UPDATE, Kryptogramm des Bausteins pruefen,
 * EXTERNAL AUTHENTICATE. Ab hier ist alles auf dem Bus verschluesselt.
 *
 * keys wird nur gelesen; der Aufrufer loescht sie danach selbst. */
osk_result_t osk_se050_apdu_open_channel(osk_se050_apdu_t *a,
					 const osk_scp03_static_keys_t *keys);

/* Kanal abreissen, Sitzung beenden, alle Schluessel loeschen.
 * Darf jederzeit aufgerufen werden, auch auf einem halb offenen Zustand. */
void osk_se050_apdu_close(osk_se050_apdu_t *a);

/* Einen Befehl schicken und die TLV-Nutzlast der Antwort holen.
 *
 * Steht der Kanal, wird der Befehl eingepackt und die Antwort ausgepackt -
 * der Aufrufer merkt davon nichts. Steht er nicht, geht es offen hinaus;
 * das ist nur bis zum Kanalaufbau erlaubt und danach ein Fehler.
 *
 * Ein Statuswort ungleich 0x9000 wird zu einem passenden osk_result_t.
 * rsp darf NULL sein, wenn keine Nutzlast erwartet wird. */
osk_result_t osk_se050_apdu_exchange(osk_se050_apdu_t *a,
				     uint8_t ins, uint8_t p1, uint8_t p2,
				     const uint8_t *payload, size_t payload_len,
				     uint8_t *rsp, size_t rsp_cap,
				     size_t *rsp_len);

/* Statuswort nach osk_result_t. Getrennt herausgezogen, damit die
 * Uebersetzung ohne Bus geprueft werden kann. */
osk_result_t osk_se050_sw_to_result(uint16_t sw);

#endif
