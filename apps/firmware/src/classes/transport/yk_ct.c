/* Siehe yk_ct.h. */
#include "yk_ct.h"

bool yk_ct_equal(const void *a, const void *b, size_t len)
{
	/* volatile, damit der Uebersetzer die Schleife nicht zu einem
	 * abbrechenden Vergleich zusammenzieht. */
	const volatile uint8_t *pa = (const volatile uint8_t *)a;
	const volatile uint8_t *pb = (const volatile uint8_t *)b;
	volatile uint8_t diff = 0u;

	if (len == 0u)
		return true;
	if (pa == NULL || pb == NULL)
		return false;

	for (size_t i = 0; i < len; i++)
		diff = (uint8_t)(diff | (uint8_t)(pa[i] ^ pb[i]));

	return diff == 0u;
}

bool yk_ct_acc_code_ok(const uint8_t given[YK_ACC_CODE_LEN],
		       const uint8_t stored[YK_ACC_CODE_LEN])
{
	return yk_ct_equal(given, stored, YK_ACC_CODE_LEN);
}
