/* Die zwei Hilfen, die keine Krypto-Bibliothek braucht und die in beiden
 * Unterbauten gleich sind: Puffer leeren und in gleichbleibender Zeit
 * vergleichen. */
#include <string.h>
#include "se050_crypto.h"

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

int osk_memeq_ct(const void *a, const void *b, size_t len)
{
	const uint8_t *x = (const uint8_t *)a;
	const uint8_t *y = (const uint8_t *)b;
	uint8_t diff = 0;

	/* Immer alle Bytes anfassen: kein frueher Ausstieg, der ueber die
	 * Laufzeit verraet, wie weit zwei Werte uebereinstimmten. */
	for (size_t i = 0; i < len; i++) {
		diff |= (uint8_t)(x[i] ^ y[i]);
	}
	return (diff == 0) ? 0 : 1;
}
