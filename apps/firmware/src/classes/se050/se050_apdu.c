#include <string.h>
#include "se050_apdu.h"

/* Groesster Puffer, den diese Schicht braucht: der APDU-Puffer des
 * Bausteins plus Luft fuer Auffuellung, MAC und Kopf. */
#define APDU_BUF (SE050_APDU_BUF + 64u)

osk_result_t osk_se050_sw_to_result(uint16_t sw)
{
	switch (sw) {
	case SW_NO_ERROR:
		return OSK_OK;

	/* Der Baustein sagt bei allem, was an einer Bedingung scheitert,
	 * dasselbe: fehlende Freigabe, falsche PIN, nicht vorhandenes
	 * Objekt, geschlossene Sitzung. Genauer geht es nicht - und das ist
	 * richtig so, denn eine feinere Auskunft waere eine Auskunft an den
	 * Angreifer. Wer unterscheiden muss, prueft vorher mit ReadType. */
	case SW_CONDITIONS_NOT_SATISFIED:
		return OSK_ERR_DENIED;

	case SW_SECURITY_STATUS:
		return OSK_ERR_DENIED;

	case SW_COMMAND_NOT_ALLOWED:
		return OSK_ERR_DENIED;

	case SW_WRONG_DATA:
	case SW_DATA_INVALID:
		return OSK_ERR_ARG;

	default:
		break;
	}

	/* 6Axx und 6Bxx sind die Familie "Aufruf passt nicht", 69xx
	 * "darf nicht". Alles andere ist ein Fehler, den wir nicht kennen -
	 * dann OSK_ERR_INTERNAL, nicht OSK_OK. */
	if ((sw & 0xFF00u) == 0x6A00u || (sw & 0xFF00u) == 0x6B00u) {
		return OSK_ERR_ARG;
	}
	if ((sw & 0xFF00u) == 0x6900u) {
		return OSK_ERR_DENIED;
	}
	return OSK_ERR_INTERNAL;
}

/* ------------------------------------------------- Rohen Befehl schicken */

/* Einen vollstaendigen C-APDU unverpackt hinausschicken und die rohe
 * Antwort zurueckholen - Nutzlast und die zwei Statusbytes. */
static osk_result_t raw_exchange(osk_se050_apdu_t *a, const uint8_t *cmd, size_t cmd_len,
				 uint8_t *rsp, size_t rsp_cap, size_t *rsp_len)
{
	const osk_result_t r = osk_se050_t1_transceive(&a->t1, cmd, cmd_len, rsp, rsp_cap, rsp_len);
	if (r != OSK_OK) {
		return r;
	}
	if (*rsp_len < 2u) {
		/* Ohne Statuswort ist das keine Antwort. */
		return OSK_ERR_FORMAT;
	}
	return OSK_OK;
}

/* ------------------------------------------------------- Applet auswaehlen */

static osk_result_t select_applet(osk_se050_apdu_t *a, uint8_t *version_out, size_t version_cap,
				  size_t *version_len)
{
	static const uint8_t aid[SE050_APPLET_AID_LEN] = SE050_APPLET_AID;

	/* Auswahlbefehl nach Tabelle 51: CLA 0x00, nicht 0x80 - der geht an
	 * den Kartenverwalter von JCOP, nicht an das Applet. */
	uint8_t cmd[6u + SE050_APPLET_AID_LEN];
	cmd[0] = 0x00u;
	cmd[1] = 0xA4u;
	cmd[2] = 0x04u;
	cmd[3] = 0x00u;
	cmd[4] = SE050_APPLET_AID_LEN;
	memcpy(&cmd[5], aid, SE050_APPLET_AID_LEN);
	cmd[5 + SE050_APPLET_AID_LEN] = 0x00u; /* Le */

	uint8_t rsp[64];
	size_t n = 0;
	osk_result_t r = raw_exchange(a, cmd, sizeof(cmd), rsp, sizeof(rsp), &n);
	if (r != OSK_OK) {
		return r;
	}

	const uint16_t sw = (uint16_t)(((uint16_t)rsp[n - 2u] << 8) | rsp[n - 1u]);
	r = osk_se050_sw_to_result(sw);
	if (r != OSK_OK) {
		return r;
	}

	const size_t body = n - 2u;
	if (version_out != NULL && version_len != NULL) {
		if (body > version_cap) {
			return OSK_ERR_FULL;
		}
		memcpy(version_out, rsp, body);
		*version_len = body;
	}

	a->applet_selected = true;
	return OSK_OK;
}

osk_result_t osk_se050_apdu_open(osk_se050_apdu_t *a, osk_se050_bus_t *bus, uint8_t *version_out,
				 size_t version_cap, size_t *version_len)
{
	if (a == NULL || bus == NULL) {
		return OSK_ERR_ARG;
	}
	memset(a, 0, sizeof(*a));

	osk_result_t r = osk_crypto_init();
	if (r != OSK_OK) {
		return r;
	}
	r = osk_se050_t1_open(&a->t1, bus);
	if (r != OSK_OK) {
		return r;
	}
	r = select_applet(a, version_out, version_cap, version_len);
	if (r != OSK_OK) {
		(void)osk_se050_t1_close(&a->t1);
		return r;
	}
	return OSK_OK;
}

/* ------------------------------------------------------- Kanal aufbauen */

osk_result_t osk_se050_apdu_open_channel(osk_se050_apdu_t *a, const osk_scp03_static_keys_t *keys)
{
	if (a == NULL || keys == NULL) {
		return OSK_ERR_ARG;
	}
	if (!a->applet_selected) {
		return OSK_ERR_IO;
	}
	if (a->scp.established) {
		return OSK_ERR_EXISTS;
	}

	/* Eigene Zufallszahl. Sie kommt vom Wirt, NICHT vom SE050: wer den
	 * Kanal erst aufbaut, hat noch keinen Grund, dem Gegenueber zu
	 * glauben, und eine vom Gegenueber gewaehlte Zufallszahl macht die
	 * Pruefung des Kryptogramms wertlos. */
	uint8_t host_challenge[SCP03_CHALLENGE_LEN];
	osk_result_t r = osk_random(host_challenge, sizeof(host_challenge));
	if (r != OSK_OK) {
		return r;
	}

	/* INITIALIZE UPDATE (Card Specification D.4.1). P1 ist die
	 * Schluesselfassung; AN12413 4.5.3.1 schreibt 0x00 vor. */
	uint8_t cmd[5u + SCP03_CHALLENGE_LEN + 1u];
	cmd[0] = SE050_CLA;
	cmd[1] = 0x50u;
	cmd[2] = 0x00u;
	cmd[3] = 0x00u;
	cmd[4] = SCP03_CHALLENGE_LEN;
	memcpy(&cmd[5], host_challenge, SCP03_CHALLENGE_LEN);
	cmd[5 + SCP03_CHALLENGE_LEN] = 0x00u; /* Le */

	uint8_t rsp[64];
	size_t n = 0;
	r = raw_exchange(a, cmd, sizeof(cmd), rsp, sizeof(rsp), &n);
	if (r != OSK_OK) {
		goto out;
	}

	{
		const uint16_t sw = (uint16_t)(((uint16_t)rsp[n - 2u] << 8) | rsp[n - 1u]);
		r = osk_se050_sw_to_result(sw);
		if (r != OSK_OK) {
			goto out;
		}
	}

	/* Antwort nach Amendment D Tabelle 7-2:
	 *   10 Byte Streudaten | 3 Byte Schluesselinfo |
	 *   8 Byte Zufall des Bausteins | 8 Byte Kryptogramm |
	 *   optional 3 Byte Folgezaehler.
	 * Mehr als die Mindestlaenge ist erlaubt (der Folgezaehler), weniger
	 * nicht. */
	{
		const size_t body = n - 2u;
		const size_t need = 10u + 3u + SCP03_CHALLENGE_LEN + SCP03_CRYPTOGRAM_LEN;
		if (body < need) {
			r = OSK_ERR_FORMAT;
			goto out;
		}

		/* Die Schluesselinfo nennt das Protokoll an zweiter Stelle.
		 * Steht da nicht 0x03, redet der Baustein nicht SCP03 - dann
		 * lieber abbrechen als einen Kanal bauen, den wir nicht
		 * verstehen. */
		if (rsp[11] != 0x03u) {
			r = OSK_ERR_UNSUPPORTED;
			goto out;
		}

		const uint8_t *card_challenge = &rsp[13];
		const uint8_t *card_cryptogram = &rsp[13 + SCP03_CHALLENGE_LEN];

		uint8_t want_card[SCP03_CRYPTOGRAM_LEN];
		uint8_t host_cryptogram[SCP03_CRYPTOGRAM_LEN];

		r = osk_scp03_derive(&a->scp, keys, host_challenge, card_challenge, want_card,
				     host_cryptogram);
		if (r != OSK_OK) {
			goto out;
		}

		/* Jetzt der Beweis, dass der Baustein denselben festen
		 * Schluessel hat. In gleichbleibender Zeit vergleichen. */
		const int bad = osk_memeq_ct(want_card, card_cryptogram, SCP03_CRYPTOGRAM_LEN);
		osk_wipe(want_card, sizeof(want_card));
		if (bad != 0) {
			osk_wipe(host_cryptogram, sizeof(host_cryptogram));
			osk_scp03_close(&a->scp);
			r = OSK_ERR_DENIED;
			goto out;
		}

		/* EXTERNAL AUTHENTICATE. */
		uint8_t ea[5u + SCP03_CRYPTOGRAM_LEN + SCP03_MAC_LEN];
		size_t ea_len = 0;
		r = osk_scp03_build_external_auth(&a->scp, host_cryptogram, ea, sizeof(ea),
						  &ea_len);
		osk_wipe(host_cryptogram, sizeof(host_cryptogram));
		if (r != OSK_OK) {
			osk_scp03_close(&a->scp);
			goto out;
		}

		n = 0;
		r = raw_exchange(a, ea, ea_len, rsp, sizeof(rsp), &n);
		osk_wipe(ea, sizeof(ea));
		if (r != OSK_OK) {
			osk_scp03_close(&a->scp);
			goto out;
		}

		/* Die Antwort auf EXTERNAL AUTHENTICATE traegt keinen R-MAC
		 * (Amendment D 6.2.5), also nur das Statuswort pruefen. */
		if (n != 2u) {
			osk_scp03_close(&a->scp);
			r = OSK_ERR_FORMAT;
			goto out;
		}
		const uint16_t sw = (uint16_t)(((uint16_t)rsp[0] << 8) | rsp[1]);
		r = osk_se050_sw_to_result(sw);
		if (r != OSK_OK) {
			osk_scp03_close(&a->scp);
			goto out;
		}
	}

out:
	osk_wipe(host_challenge, sizeof(host_challenge));
	osk_wipe(rsp, sizeof(rsp));
	osk_wipe(cmd, sizeof(cmd));
	return r;
}

void osk_se050_apdu_close(osk_se050_apdu_t *a)
{
	if (a == NULL) {
		return;
	}
	osk_scp03_close(&a->scp);
	(void)osk_se050_t1_close(&a->t1);
	a->applet_selected = false;
}

/* ------------------------------------------------------- Im Betrieb */

osk_result_t osk_se050_apdu_exchange(osk_se050_apdu_t *a, uint8_t ins, uint8_t p1, uint8_t p2,
				     const uint8_t *payload, size_t payload_len, uint8_t *rsp,
				     size_t rsp_cap, size_t *rsp_len)
{
	if (a == NULL || rsp_len == NULL) {
		return OSK_ERR_ARG;
	}
	if (payload_len > 0 && payload == NULL) {
		return OSK_ERR_ARG;
	}
	if (!a->applet_selected) {
		return OSK_ERR_IO;
	}
	if (payload_len > 0xFFu) {
		/* Erweiterte Laengen kann der Baustein, dieser Treiber
		 * braucht sie nicht: der laengste Befehl hier ist ein
		 * oeffentlicher P384-Schluessel mit 97 Byte. */
		return OSK_ERR_FULL;
	}
	*rsp_len = 0;

	/* Klartext-Befehl bauen: Kopf, dann Lc und Nutzlast, wenn welche da
	 * ist. Das Le setzt die Verpackung; offen haengen wir es selbst an. */
	uint8_t plain[5u + 0xFFu + 1u];
	size_t plain_len = 4u;
	plain[0] = SE050_CLA;
	plain[1] = ins;
	plain[2] = p1;
	plain[3] = p2;
	if (payload_len > 0) {
		plain[4] = (uint8_t)payload_len;
		memcpy(&plain[5], payload, payload_len);
		plain_len = 5u + payload_len;
	}

	uint8_t wire[APDU_BUF];
	size_t wire_len = 0;
	osk_result_t r;

	if (a->scp.established) {
		r = osk_scp03_wrap(&a->scp, plain, plain_len, wire, sizeof(wire), &wire_len);
		osk_wipe(plain, plain_len);
		if (r != OSK_OK) {
			return r;
		}
	} else {
		/* Offen - nur bis der Kanal steht. Das Le muss hier selbst
		 * dran, weil keine Verpackung es setzt. */
		if (plain_len + 1u > sizeof(wire)) {
			return OSK_ERR_FULL;
		}
		memcpy(wire, plain, plain_len);
		wire[plain_len] = 0x00u;
		wire_len = plain_len + 1u;
		osk_wipe(plain, plain_len);
	}

	uint8_t raw[APDU_BUF];
	size_t raw_len = 0;
	r = raw_exchange(a, wire, wire_len, raw, sizeof(raw), &raw_len);
	osk_wipe(wire, wire_len);
	if (r != OSK_OK) {
		return r;
	}

	uint16_t sw = 0;
	size_t body = 0;

	if (a->scp.established) {
		uint8_t body_buf[APDU_BUF];
		r = osk_scp03_unwrap(&a->scp, raw, raw_len, body_buf, sizeof(body_buf), &body, &sw);
		osk_wipe(raw, raw_len);
		if (r != OSK_OK) {
			/* Ein falscher R-MAC heisst: die Antwort ist nicht
			 * die, die der Baustein geschickt hat. Dann ist der
			 * Kanal verbrannt - weitermachen waere das Gegenteil
			 * von sicher. */
			if (r == OSK_ERR_DENIED) {
				osk_se050_apdu_close(a);
			}
			osk_wipe(body_buf, sizeof(body_buf));
			return r;
		}
		if (body > 0) {
			if (rsp == NULL || body > rsp_cap) {
				osk_wipe(body_buf, sizeof(body_buf));
				return OSK_ERR_FULL;
			}
			memcpy(rsp, body_buf, body);
		}
		osk_wipe(body_buf, sizeof(body_buf));
	} else {
		sw = (uint16_t)(((uint16_t)raw[raw_len - 2u] << 8) | raw[raw_len - 1u]);
		body = raw_len - 2u;
		if (body > 0) {
			if (rsp == NULL || body > rsp_cap) {
				osk_wipe(raw, raw_len);
				return OSK_ERR_FULL;
			}
			memcpy(rsp, raw, body);
		}
		osk_wipe(raw, raw_len);
	}

	const osk_result_t sw_res = osk_se050_sw_to_result(sw);
	if (sw_res != OSK_OK) {
		/* Bei Fehlerstatus gibt es keine brauchbare Nutzlast. */
		if (rsp != NULL && body > 0) {
			osk_wipe(rsp, body);
		}
		return sw_res;
	}

	*rsp_len = body;
	return OSK_OK;
}
