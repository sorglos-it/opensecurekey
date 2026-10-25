/* Die zwei Hilfen, die keine Krypto-Bibliothek braucht und die in beiden
 * Unterbauten gleich sind: Puffer leeren und in gleichbleibender Zeit
 * vergleichen. */
#include <string.h>
#include "se050_crypto.h"

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
