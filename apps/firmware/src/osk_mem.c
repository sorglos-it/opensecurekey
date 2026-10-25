/* Puffer sicher leeren. Warum das hier liegt: osk_mem.h sagt es.
 *
 * Diese Fassung stammt aus src/classes/se050/se050_crypto_common.c und
 * nicht aus der ctap-Fassung: sie prueft auf NULL. Die andere lief bei
 * einem NULL-Zeiger ins Verderben. Von zwei gleichwertigen Fassungen
 * gewinnt die vorsichtigere. */
#include "osk_mem.h"

#include <stdint.h>

/* Der Zeiger geht durch ein volatile, damit der Uebersetzer das Leeren
 * nicht als "wird ja nie mehr gelesen" wegstreicht. */
void osk_wipe(void *p, size_t len)
{
	if (p == NULL || len == 0) {
		return;
	}
	volatile uint8_t *q = (volatile uint8_t *)p;
	while (len-- > 0) {
		*q++ = 0;
	}
}
