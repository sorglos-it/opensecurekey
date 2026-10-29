/* authenticatorConfig (0x0D), CTAP 2.2 Abschnitt 6.11.
 *
 * Drei Schalter, die dem BETRIEB gehoeren und nicht der Webseite:
 *
 *   0x01 enableEnterpriseAttestation - einmal ein, nie wieder aus
 *   0x02 toggleAlwaysUv              - NICHT gebaut, siehe unten
 *   0x03 setMinPINLength             - Mindestlaenge, Dienste-Liste,
 *                                      erzwungener Wechsel
 *
 * Jeder Aufruf braucht ein pinUvAuthToken mit dem Recht acfg. Ohne PIN gibt
 * es kein Token, also auch keine Konfiguration - das ist beabsichtigt: wer
 * das Geraet in der Hand hat, aber die PIN nicht kennt, darf die Regeln
 * nicht lockern.
 *
 * WARUM 0x02 FEHLT: alwaysUv heisst, dass JEDE Anmeldung eine
 * Benutzerpruefung verlangt. Das greift in jeden Ablauf ein, auch in die von
 * MakeCredential und GetAssertion, und es widerspricht makeCredUvNotRqd,
 * das wir melden. Ein halb gebauter Schalter waere hier gefaehrlicher als
 * kein Schalter: ein Betrieb, der ihn setzt, glaubt danach an eine
 * Zusicherung, die das Geraet nicht einhaelt. Darum antwortet 0x02
 * CTAP2_ERR_UNSUPPORTED_OPTION, und GetInfo meldet die Option alwaysUv
 * nicht. */
#ifndef OSK_CTAP_CONFIG_H
#define OSK_CTAP_CONFIG_H

#include <stdint.h>
#include <stddef.h>
#include "osk_cbor.h"

/* Der ganze Befehl. Liefert das CTAP-Statusbyte. */
uint8_t osk_ctap_config_handle(const uint8_t *cbor, size_t len, osk_cbor_writer_t *w);

#endif
