/* Der eine Zustand des Treibers, fuer die Vertragsdateien daneben.
 *
 * se050.c haelt die offene Sitzung und das Merkmalbild des Applets. RSA
 * (se050_rsa.c) und die monotonen Zaehler (se050_counter.c) liegen in
 * eigenen Dateien, brauchen aber denselben Kanal - ein zweiter waere ein
 * Fehler, kein Merkmal.
 *
 * Nur fuer den Treiber selbst. Nach aussen gilt osk_se050.h. */
#ifndef SE050_STATE_H
#define SE050_STATE_H

#include <stdint.h>
#include "osk_result.h"
#include "se050_apdu.h"

/* Die offene Sitzung. OSK_ERR_IO, solange osk_se050_open() nicht
 * erfolgreich gelaufen ist - dieselbe Antwort wie bisher. */
osk_result_t osk_se050_state(osk_se050_apdu_t **apdu_out);

/* Das Merkmalbild aus VersionInfo (AN12413 Tabelle 5), wie es beim
 * Anmelden gelesen wurde. 0, solange nicht angemeldet. */
uint16_t osk_se050_applet_config(void);

#endif
