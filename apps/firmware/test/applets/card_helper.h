/* Ein Kommando an den Verteiler schicken und die Antwort auseinandernehmen.
 * Von test_piv und test_openpgp gemeinsam benutzt. */
#ifndef CARD_HELPER_H
#define CARD_HELPER_H

#include <string.h>
#include <stdbool.h>
#include "osk_applets.h"
#include "osk_apdu.h"

static uint8_t g_resp[4096];
static size_t g_resp_len;
static uint16_t g_sw;

/* le < 0 heisst "kein Le-Feld". Erweiterte Laengenfelder kommen von selbst,
 * sobald eine der beiden Laengen nicht mehr in ein Byte passt - beide
 * Felder muessen dann dieselbe Form haben (7816-4 Abschnitt 5.1). */
static osk_result_t call(uint8_t cla, uint8_t ins, uint8_t p1, uint8_t p2, const uint8_t *data,
			 size_t lc, long le)
{
	static uint8_t buf[OSK_APDU_MAX_FRAME];
	osk_request_t req;
	osk_response_t res;
	size_t at = 0;
	osk_result_t rc;
	const bool ext = (lc > 255u) || (le > 256);

	buf[at++] = cla;
	buf[at++] = ins;
	buf[at++] = p1;
	buf[at++] = p2;
	if (ext)
		buf[at++] = 0x00u;
	if (lc != 0u) {
		if (ext) {
			buf[at++] = (uint8_t)(lc >> 8);
			buf[at++] = (uint8_t)lc;
		} else {
			buf[at++] = (uint8_t)lc;
		}
		memcpy(buf + at, data, lc);
		at += lc;
	}
	if (le >= 0) {
		if (ext) {
			buf[at++] = (uint8_t)((unsigned long)le >> 8);
			buf[at++] = (uint8_t)le;
		} else {
			buf[at++] = (uint8_t)((le == 256) ? 0 : le);
		}
	}

	req.link = OSK_LINK_USB_CCID;
	req.data = buf;
	req.len = at;
	res.data = g_resp;
	res.cap = sizeof(g_resp);
	res.len = 0u;
	rc = osk_applets_handle(&req, &res);
	if (res.len >= 2u) {
		g_sw = (uint16_t)(((uint16_t)res.data[res.len - 2u] << 8) |
				  (uint16_t)res.data[res.len - 1u]);
		g_resp_len = res.len - 2u;
	} else {
		g_sw = 0u;
		g_resp_len = 0u;
	}
	return rc;
}

#endif
