/* authenticatorCredentialManagement (0x0A), CTAP 2.2 Abschnitt 6.8.
 *
 * Die Zugaenge AM GERAET verwalten: zaehlen, auflisten, einzeln loeschen,
 * Nutzerangaben aendern. Ohne das sammelt sich im Tresor Muell, den niemand
 * wegbekommt - ein Dienst, bei dem man sein Konto geloescht hat, bleibt als
 * Passkey im Geraet liegen, bis es ganz zurueckgesetzt wird.
 *
 * Betroffen sind nur AUFFINDBARE (residente) Nachweise. Die nicht
 * auffindbaren werden abgeleitet und liegen nirgends; es gibt an ihnen
 * nichts zu verwalten und nichts zu loeschen - sie verschwinden erst mit dem
 * Grundgeheimnis (authenticatorReset).
 *
 * Jeder Aufruf braucht ein pinUvAuthToken mit dem Recht cm. Die Aufzaehlung
 * laeuft in Schritten: ein Begin, dann GetNext, solange etwas kommt. Der
 * Stand dazu liegt in osk_ctap_state()->cm_enum und ist nach jedem anderen
 * Befehl hinfaellig. */
#ifndef OSK_CTAP_CM_H
#define OSK_CTAP_CM_H

#include <stdint.h>
#include <stddef.h>
#include "osk_cbor.h"

/* Der ganze Befehl. Liefert das CTAP-Statusbyte. */
uint8_t osk_ctap_cm_handle(const uint8_t *cbor, size_t len, osk_cbor_writer_t *w);

#endif
