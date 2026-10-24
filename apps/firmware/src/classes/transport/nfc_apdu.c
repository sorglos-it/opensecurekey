/* APDU-Zerlegung. Siehe nfc_apdu.h.
 * Spezifikation: ISO/IEC 7816-4, Abschnitt 5.1 (Faelle 1 bis 4, kurz und
 * erweitert). */
#include <string.h>
#include "nfc_apdu.h"

/* Kettenbit im Klassenbyte (ISO/IEC 7816-4, Abschnitt 5.1.1). */
#define APDU_CLA_CHAINING 0x10u
/* CTAP schickt Kettenglieder mit 0x90 statt 0x80|0x10 - dieselbe Bedeutung,
 * anderer Wert (CTAP 2.1, Abschnitt 11.3.5.1). */
#define APDU_CLA_CTAP_CHAINING 0x90u

bool osk_apdu_parse(const uint8_t *buf, size_t len, osk_apdu_t *out)
{
	if (!buf || !out)
		return false;
	memset(out, 0, sizeof(*out));
	/* Ohne Kopf gibt es keine APDU. */
	if (len < 4)
		return false;

	out->cla = buf[0];
	out->ins = buf[1];
	out->p1 = buf[2];
	out->p2 = buf[3];
	out->chaining_cla = (buf[0] == APDU_CLA_CTAP_CHAINING);
	out->chained = ((buf[0] & APDU_CLA_CHAINING) != 0) || out->chaining_cla;

	/* Fall 1: nur der Kopf. */
	if (len == 4)
		return true;

	/* Fall 2 kurz: ein einzelnes Le. 0 bedeutet 256. */
	if (len == 5) {
		out->le = (buf[4] == 0) ? 256u : (size_t)buf[4];
		out->has_le = true;
		return true;
	}

	if (buf[4] != 0) {
		/* Kurze Laengen: Lc steht in einem Byte. */
		const size_t lc = (size_t)buf[4];

		/* Die Laenge wird geprueft, bevor data gesetzt wird. */
		if (len < 5 + lc)
			return false;
		out->lc = lc;
		out->data = buf + 5;
		if (len == 5 + lc)
			return true;
		if (len == 6 + lc) {
			out->le = (buf[5 + lc] == 0) ? 256u : (size_t)buf[5 + lc];
			out->has_le = true;
			return true;
		}
		/* Alles andere ist ueberzaehliges Anhaengsel. */
		return false;
	}

	/* buf[4] == 0: erweiterte Laengen. Der Kopf braucht dann drei Bytes. */
	if (len < 7)
		return false;
	out->extended = true;

	/* Fall 2 erweitert: nur Le, drei Bytes. 0 bedeutet 65536. */
	if (len == 7) {
		const size_t le = ((size_t)buf[5] << 8) | (size_t)buf[6];

		out->le = (le == 0) ? 65536u : le;
		out->has_le = true;
		return true;
	}

	const size_t lc = ((size_t)buf[5] << 8) | (size_t)buf[6];

	/* Lc 0 bei erweiterten Laengen ist nicht vergeben. */
	if (lc == 0)
		return false;
	if (len < 7 + lc)
		return false;
	out->lc = lc;
	out->data = buf + 7;
	if (len == 7 + lc)
		return true;
	if (len == 9 + lc) {
		const size_t le = ((size_t)buf[7 + lc] << 8) | (size_t)buf[8 + lc];

		out->le = (le == 0) ? 65536u : le;
		out->has_le = true;
		return true;
	}
	return false;
}

size_t osk_apdu_status(uint8_t *out, size_t cap, size_t len, uint16_t sw)
{
	if (!out || len > cap || cap - len < 2)
		return 0;
	out[len] = (uint8_t)(sw >> 8);
	out[len + 1] = (uint8_t)sw;
	return len + 2;
}

size_t osk_apdu_only_status(uint8_t *out, size_t cap, uint16_t sw)
{
	return osk_apdu_status(out, cap, 0, sw);
}
