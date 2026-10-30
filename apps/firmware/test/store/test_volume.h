/* Gemeinsames fuer die zwei Pruefsaetze des Datentraegers. */
#ifndef TEST_VOLUME_H
#define TEST_VOLUME_H

#include <stdint.h>
#include <stdbool.h>
#include "osk_volume.h"

#define VT_ROOT_KEY 0x4F534B10u
#define VT_SIGN_KEY 0x4F534B11u
#define VT_COUNTER 0x4F534B12u

/* Die Anzeige-Attrappe: merkt sich, WAS sie gefragt wurde. Genau darum geht
 * es bei der Auflage - nicht dass gefragt wurde, sondern mit welchem Modus. */
osk_result_t vt_ui_confirm(osk_vol_mode_t mode, uint32_t recipients, bool secret, void *ctx);
void vt_ui_reset(bool say_no);
osk_vol_mode_t vt_asked_mode(void);
uint32_t vt_asked_recipients(void);
bool vt_asked_secret(void);
unsigned vt_asked_calls(void);

/* Diesen Stick nehmen: eigenes Grundgeheimnis, eigener Schluessel, eigener
 * Zaehler. Zwei verschiedene Nummern sind zwei verschiedene Geraete. */
void vt_stick(uint32_t device);
osk_result_t vt_init(void);
void vt_card(uint32_t data_blocks);

#endif
