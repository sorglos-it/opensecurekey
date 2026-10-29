/* Seriennummer. Siehe osk_serial.h. */
#include <string.h>
#include "osk_serial.h"
#include "osk_se050.h"
/* Die Nummer des Grundgeheimnisses steht in der CTAP-Naht und wird von
 * dort geholt, nicht abgeschrieben: eine Zahl gehoert an eine Stelle. */
#include "osk_ctap_key.h"

static uint8_t g_serial[OSK_SERIAL_LEN];
static bool g_known;

void osk_serial_forget(void)
{
	memset(g_serial, 0, sizeof(g_serial));
	g_known = false;
}

bool osk_serial_known(void)
{
	return g_known;
}

osk_result_t osk_serial_device(uint8_t out[OSK_SERIAL_LEN])
{
	uint8_t buf[OSK_SERIAL_LEN];
	osk_result_t rc;

	if (out == NULL)
		return OSK_ERR_ARG;
	if (g_known) {
		memcpy(out, g_serial, OSK_SERIAL_LEN);
		return OSK_OK;
	}

	rc = osk_se050_hkdf(OSK_KEY_ID_ROOT, (const uint8_t *)OSK_INFO_SERIAL,
			    sizeof(OSK_INFO_SERIAL) - 1u, buf, sizeof(buf));
	if (rc != OSK_OK)
		return rc;

	memcpy(g_serial, buf, sizeof(g_serial));
	g_known = true;
	memcpy(out, g_serial, OSK_SERIAL_LEN);
	return OSK_OK;
}

osk_result_t osk_serial_short(uint8_t out[OSK_SERIAL_SHORT_LEN])
{
	uint8_t full[OSK_SERIAL_LEN];
	const osk_result_t rc = osk_serial_device(full);

	if (out == NULL)
		return OSK_ERR_ARG;
	if (rc != OSK_OK)
		return rc;

	memcpy(out, full, OSK_SERIAL_SHORT_LEN);
	/* Alles Null heisst in den Anwendungen "keine Nummer". Der Wert kann
	 * aus der Ableitung herauskommen, selten aber nicht nie - dann waere
	 * eine echte Nummer als "keine" gelesen worden. */
	if (out[0] == 0u && out[1] == 0u && out[2] == 0u && out[3] == 0u)
		out[3] = 1u;
	return OSK_OK;
}
