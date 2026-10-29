/* Steuerung des Chip-Nachbaus, nur fuer die Prueffaelle. */
#ifndef FAKE_SE050_H
#define FAKE_SE050_H

#include <stdint.h>
#include <stddef.h>
#include "osk_se050.h"

void fake_reset(void);
void fake_add_key(osk_key_id_t id);
int fake_hmac_of(osk_key_id_t id, const uint8_t *msg, size_t len, uint8_t out[32]);

#endif
