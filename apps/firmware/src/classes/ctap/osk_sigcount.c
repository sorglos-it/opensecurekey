/* Siehe osk_sigcount.h. Absichtlich duenn: die Datei ist kein Zaehler,
 * sondern die eine Stelle, an der steht, WELCHER Zaehler gilt. Stuende das
 * in osk_ctap_core.c, muesste es dort an drei Stellen stehen. */
#include "osk_sigcount.h"

osk_result_t osk_sigcount_next(uint32_t *value_out)
{
	if (!value_out)
		return OSK_ERR_ARG;
	*value_out = 0;
	return osk_store_sigcount_next(OSK_SIGCOUNT_SLOT, value_out);
}

osk_result_t osk_sigcount_get(uint32_t *value_out)
{
	if (!value_out)
		return OSK_ERR_ARG;
	*value_out = 0;
	return osk_store_sigcount_get(OSK_SIGCOUNT_SLOT, value_out);
}
