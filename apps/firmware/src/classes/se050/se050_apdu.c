#include <string.h>
#include "se050_apdu.h"
#include "se050_tlv.h"

/* Groesster Puffer, den diese Schicht braucht: der APDU-Puffer des
 * Bausteins plus Luft fuer Auffuellung, MAC, Umschlag und Kopf. */
#define APDU_BUF (SE050_APDU_BUF + 128u)

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

/* Einen vollstaendigen C-APDU hinausschicken und die rohe Antwort
 * zurueckholen - Nutzlast UND die zwei Statusbytes, unveraendert. */
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

/* Einen Befehl OHNE SCP03 schicken: Kopf bauen, Le anhaengen, senden.
 * Die rohe Antwort kommt samt Statuswort zurueck, weil der Aufrufer sie
 * teils noch auspacken muss. */
static osk_result_t plain_exchange(osk_se050_apdu_t *a, uint8_t cla, uint8_t ins, uint8_t p1,
				   uint8_t p2, const uint8_t *payload, size_t payload_len,
				   uint8_t *rsp, size_t rsp_cap, size_t *rsp_len)
{
	if (payload_len > SE050_MAX_PAYLOAD) {
		return OSK_ERR_FULL;
	}

	uint8_t cmd[7u + SE050_MAX_PAYLOAD + 2u];
	size_t n = 4u;
	cmd[0] = cla;
	cmd[1] = ins;
	cmd[2] = p1;
	cmd[3] = p2;
	if (payload_len > 0xFFu) {
		/* Erweiterte Laenge (ISO 7816-4, vom Applet ausdruecklich
		 * zugelassen: AN12413 4.1, "Both standard as well as
		 * extended length APDUs are supported"). Gebraucht wird sie
		 * fuer genau einen Fall: ein RSA-Chiffrat ist so lang wie
		 * der Modulus, bei 4096 bit also 512 Byte, und das passt in
		 * kein Lc-Byte. Ohne das bliebe verschluesselte Post
		 * ungelesen.
		 *
		 * UNBESTAETIGT wie die Werte in se050_t1_wire.h: dass das
		 * Applet diese Form annimmt, steht im Dokument; gemessen
		 * ist es nicht. Am Geraet ist es eine Messung - Befehl
		 * schicken und sehen, ob 0x9000 kommt. */
		cmd[4] = 0x00u;
		cmd[5] = (uint8_t)(payload_len >> 8);
		cmd[6] = (uint8_t)(payload_len & 0xFFu);
		memcpy(&cmd[7], payload, payload_len);
		n = 7u + payload_len;
		/* Bei erweiterter Laenge ist auch Le zwei Byte lang. */
		cmd[n] = 0x00u;
		cmd[n + 1u] = 0x00u;
		n += 2u;
		const osk_result_t re = raw_exchange(a, cmd, n, rsp, rsp_cap, rsp_len);
		osk_wipe(cmd, n);
		return re;
	}
	if (payload_len > 0) {
		cmd[4] = (uint8_t)payload_len;
		memcpy(&cmd[5], payload, payload_len);
		n = 5u + payload_len;
	}
	cmd[n] = 0x00u; /* Le */
	n += 1u;

	const osk_result_t r = raw_exchange(a, cmd, n, rsp, rsp_cap, rsp_len);
	osk_wipe(cmd, n);
	return r;
}

/* Statuswort am Ende einer rohen Antwort. */
static uint16_t sw_of(const uint8_t *rsp, size_t len)
{
	return (uint16_t)(((uint16_t)rsp[len - 2u] << 8) | rsp[len - 1u]);
}

/* ------------------------------------------------------- Applet auswaehlen */

static osk_result_t select_applet(osk_se050_apdu_t *a, uint8_t *version_out, size_t version_cap,
				  size_t *version_len)
{
	static const uint8_t aid[SE050_APPLET_AID_LEN] = SE050_APPLET_AID;

	/* Auswahlbefehl nach Tabelle 51: CLA 0x00, nicht 0x80 - der geht an
	 * den Kartenverwalter von JCOP, nicht an das Applet. */
	uint8_t rsp[64];
	size_t n = 0;
	osk_result_t r =
	    plain_exchange(a, 0x00u, 0xA4u, 0x04u, 0x00u, aid, sizeof(aid), rsp, sizeof(rsp), &n);
	if (r != OSK_OK) {
		return r;
	}

	r = osk_se050_sw_to_result(sw_of(rsp, n));
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

/* ------------------------------------------------------ Sitzungsumschlag */

/* Einen inneren Befehl in ProcessSessionCmd stecken und abschicken
 * (AN12413 4.5.1.3). Der Umschlag ist offen; gesichert ist der Inhalt.
 * Die rohe Antwort kommt samt Statuswort zurueck. */
static osk_result_t session_cmd(osk_se050_apdu_t *a, const uint8_t id[SE050_SESSION_ID_LEN],
				const uint8_t *inner, size_t inner_len, uint8_t *rsp,
				size_t rsp_cap, size_t *rsp_len)
{
	/* Der innere Befehl kann mit SCP03-Verpackung und erweiterter
	 * Laenge deutlich ueber 255 Byte lang werden - ein RSA-Chiffrat von
	 * 512 Byte landet hier. Das TLV-Laengenfeld kann das (0x82 und zwei
	 * Byte, AN12413 4.1.3.2). */
	uint8_t payload[16u + SE050_MAX_PAYLOAD];
	osk_tlv_writer_t w;
	osk_tlv_init(&w, payload, sizeof(payload));
	osk_tlv_put(&w, TAG_SESSION_ID, id, SE050_SESSION_ID_LEN);
	osk_tlv_put(&w, TAG_1, inner, inner_len);

	size_t pl = 0;
	osk_result_t r = osk_tlv_done(&w, &pl);
	if (r != OSK_OK) {
		osk_wipe(payload, sizeof(payload));
		return r;
	}

	r = plain_exchange(a, SE050_CLA, INS_PROCESS, P1_DEFAULT, P2_DEFAULT, payload, pl, rsp,
			   rsp_cap, rsp_len);
	osk_wipe(payload, sizeof(payload));
	return r;
}

/* ------------------------------------------------------- Sitzung aufbauen */

osk_result_t osk_se050_apdu_session_open(osk_se050_apdu_t *a, osk_se050_session_t *s,
					 uint32_t auth_obj_id, const osk_scp03_static_keys_t *keys)
{
	if (a == NULL || s == NULL || keys == NULL) {
		return OSK_ERR_ARG;
	}
	if (!a->applet_selected) {
		return OSK_ERR_IO;
	}
	if (s->active) {
		return OSK_ERR_EXISTS;
	}
	memset(s, 0, sizeof(*s));

	uint8_t rsp[128];
	size_t n = 0;
	osk_result_t r;

	/* ---- 1. CreateSession (AN12413 4.5.1.1). Offen, es gibt noch
	 * keine Sitzung, in die es passen koennte. */
	{
		uint8_t payload[8];
		osk_tlv_writer_t w;
		osk_tlv_init(&w, payload, sizeof(payload));
		osk_tlv_put_u32(&w, TAG_1, auth_obj_id);

		size_t pl = 0;
		r = osk_tlv_done(&w, &pl);
		if (r != OSK_OK) {
			return r;
		}

		r = plain_exchange(a, SE050_CLA, INS_MGMT, P1_DEFAULT, P2_SESSION_CREATE, payload,
				   pl, rsp, sizeof(rsp), &n);
		if (r != OSK_OK) {
			return r;
		}
		r = osk_se050_sw_to_result(sw_of(rsp, n));
		if (r != OSK_OK) {
			return r;
		}

		osk_tlv_reader_t rd;
		osk_tlv_reader_init(&rd, rsp, n - 2u);

		size_t id_len = 0;
		r = osk_tlv_copy(&rd, TAG_1, s->id, sizeof(s->id), &id_len);
		if (r != OSK_OK) {
			return r;
		}
		if (id_len != SE050_SESSION_ID_LEN) {
			return OSK_ERR_FORMAT;
		}
	}

	/* ---- 2. INITIALIZE UPDATE, eingepackt in ProcessSessionCmd.
	 *
	 * Die eigene Zufallszahl kommt vom Wirt, NICHT vom SE050: wer die
	 * Sitzung erst aufbaut, hat noch keinen Grund, dem Gegenueber zu
	 * glauben, und eine vom Gegenueber gewaehlte Zufallszahl macht die
	 * Pruefung des Kryptogramms wertlos. */
	uint8_t host_challenge[SCP03_CHALLENGE_LEN];
	r = osk_random(host_challenge, sizeof(host_challenge));
	if (r != OSK_OK) {
		return r;
	}

	uint8_t host_cryptogram[SCP03_CRYPTOGRAM_LEN];
	memset(host_cryptogram, 0, sizeof(host_cryptogram));

	{
		/* Card Specification D.4.1; P1 ist die Schluesselfassung,
		 * AN12413 4.5.3.1 schreibt dafuer 0x00 vor. */
		uint8_t inner[5u + SCP03_CHALLENGE_LEN + 1u];
		inner[0] = SE050_CLA;
		inner[1] = 0x50u;
		inner[2] = 0x00u;
		inner[3] = 0x00u;
		inner[4] = SCP03_CHALLENGE_LEN;
		memcpy(&inner[5], host_challenge, SCP03_CHALLENGE_LEN);
		inner[5 + SCP03_CHALLENGE_LEN] = 0x00u;

		n = 0;
		r = session_cmd(a, s->id, inner, sizeof(inner), rsp, sizeof(rsp), &n);
		osk_wipe(inner, sizeof(inner));
		if (r != OSK_OK) {
			goto fail;
		}
		r = osk_se050_sw_to_result(sw_of(rsp, n));
		if (r != OSK_OK) {
			goto fail;
		}

		/* Antwort nach Amendment D Tabelle 7-2:
		 *   10 Byte Streudaten | 3 Byte Schluesselinfo |
		 *   8 Byte Zufall des Bausteins | 8 Byte Kryptogramm |
		 *   optional 3 Byte Folgezaehler.
		 * Mehr als die Mindestlaenge ist erlaubt, weniger nicht. */
		const size_t body = n - 2u;
		const size_t need = 10u + 3u + SCP03_CHALLENGE_LEN + SCP03_CRYPTOGRAM_LEN;
		if (body < need) {
			r = OSK_ERR_FORMAT;
			goto fail;
		}

		/* Die Schluesselinfo nennt das Protokoll an zweiter Stelle.
		 * Steht da nicht 0x03, redet der Baustein nicht SCP03 - dann
		 * lieber abbrechen als eine Sitzung bauen, die wir nicht
		 * verstehen. */
		if (rsp[11] != 0x03u) {
			r = OSK_ERR_UNSUPPORTED;
			goto fail;
		}

		const uint8_t *card_challenge = &rsp[13];
		const uint8_t *card_cryptogram = &rsp[13 + SCP03_CHALLENGE_LEN];

		uint8_t want_card[SCP03_CRYPTOGRAM_LEN];
		r = osk_scp03_derive(&s->scp, keys, host_challenge, card_challenge, want_card,
				     host_cryptogram);
		if (r != OSK_OK) {
			goto fail;
		}

		/* Jetzt der Beweis, dass der Baustein denselben festen
		 * Schluessel hat. In gleichbleibender Zeit vergleichen. */
		const int bad = osk_memeq_ct(want_card, card_cryptogram, SCP03_CRYPTOGRAM_LEN);
		osk_wipe(want_card, sizeof(want_card));
		if (bad != 0) {
			r = OSK_ERR_DENIED;
			goto fail;
		}
	}

	/* ---- 3. EXTERNAL AUTHENTICATE, ebenfalls im Umschlag. */
	{
		uint8_t ea[5u + SCP03_CRYPTOGRAM_LEN + SCP03_MAC_LEN];
		size_t ea_len = 0;
		r = osk_scp03_build_external_auth(&s->scp, host_cryptogram, ea, sizeof(ea),
						  &ea_len);
		if (r != OSK_OK) {
			goto fail;
		}

		n = 0;
		r = session_cmd(a, s->id, ea, ea_len, rsp, sizeof(rsp), &n);
		osk_wipe(ea, sizeof(ea));
		if (r != OSK_OK) {
			goto fail;
		}

		/* Die Antwort auf EXTERNAL AUTHENTICATE traegt keinen R-MAC
		 * (Amendment D 6.2.5), also nur das Statuswort pruefen. */
		if (n != 2u) {
			r = OSK_ERR_FORMAT;
			goto fail;
		}
		r = osk_se050_sw_to_result(sw_of(rsp, n));
		if (r != OSK_OK) {
			goto fail;
		}
	}

	s->active = true;
	osk_wipe(host_challenge, sizeof(host_challenge));
	osk_wipe(host_cryptogram, sizeof(host_cryptogram));
	osk_wipe(rsp, sizeof(rsp));
	return OSK_OK;

fail:
	/* Eine halb aufgebaute Sitzung ist schlimmer als keine: sie koennte
	 * fuer offen gehalten werden. Also restlos weg - samt der Sitzung
	 * beim Baustein, sonst ist einer der zwei Plaetze verbraucht. */
	osk_se050_apdu_session_close(a, s);
	osk_wipe(host_challenge, sizeof(host_challenge));
	osk_wipe(host_cryptogram, sizeof(host_cryptogram));
	osk_wipe(rsp, sizeof(rsp));
	return r;
}

void osk_se050_apdu_session_close(osk_se050_apdu_t *a, osk_se050_session_t *s)
{
	if (a == NULL || s == NULL) {
		return;
	}

	/* Dem Baustein sagen, dass die Sitzung weg ist - er hat nur zwei
	 * Plaetze (Tabelle 16), und eine vergessene Sitzung bleibt belegt,
	 * bis das Applet neu ausgewaehlt wird. Ein Fehler dabei aendert
	 * nichts an unserer Seite. */
	if (a->applet_selected) {
		uint8_t payload[16];
		osk_tlv_writer_t w;
		osk_tlv_init(&w, payload, sizeof(payload));
		osk_tlv_put(&w, TAG_SESSION_ID, s->id, SE050_SESSION_ID_LEN);

		size_t pl = 0;
		if (osk_tlv_done(&w, &pl) == OSK_OK) {
			uint8_t rsp[32];
			size_t n = 0;
			(void)plain_exchange(a, SE050_CLA, INS_MGMT, P1_DEFAULT, P2_SESSION_CLOSE,
					     payload, pl, rsp, sizeof(rsp), &n);
			osk_wipe(rsp, sizeof(rsp));
		}
		osk_wipe(payload, sizeof(payload));
	}

	osk_scp03_close(&s->scp);
	osk_wipe(s, sizeof(*s));
}

void osk_se050_apdu_close(osk_se050_apdu_t *a)
{
	if (a == NULL) {
		return;
	}
	osk_se050_apdu_session_close(a, &a->chan);
	(void)osk_se050_t1_close(&a->t1);
	a->applet_selected = false;
}

/* ------------------------------------------------------------ Im Betrieb */

osk_result_t osk_se050_apdu_exchange_in(osk_se050_apdu_t *a, osk_se050_session_t *s, uint8_t ins,
					uint8_t p1, uint8_t p2, const uint8_t *payload,
					size_t payload_len, uint8_t *rsp, size_t rsp_cap,
					size_t *rsp_len)
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
	if (payload_len > SE050_MAX_PAYLOAD) {
		return OSK_ERR_FULL;
	}
	*rsp_len = 0;

	uint8_t raw[APDU_BUF];
	size_t raw_len = 0;
	osk_result_t r;
	uint16_t sw = 0;
	size_t body = 0;

	if (s == NULL || !s->active) {
		/* Offen - nur fuer die Schritte vor der ersten Sitzung. */
		r = plain_exchange(a, SE050_CLA, ins, p1, p2, payload, payload_len, raw,
				   sizeof(raw), &raw_len);
		if (r != OSK_OK) {
			return r;
		}
		sw = sw_of(raw, raw_len);
		body = raw_len - 2u;
		if (body > 0) {
			if (rsp == NULL || body > rsp_cap) {
				osk_wipe(raw, raw_len);
				return OSK_ERR_FULL;
			}
			memcpy(rsp, raw, body);
		}
		osk_wipe(raw, raw_len);
	} else {
		/* Klartext-Befehl bauen, SCP03 darum, dann in den
		 * Sitzungsumschlag. Das Le setzt die Verpackung. */
		uint8_t plain[7u + SE050_MAX_PAYLOAD];
		size_t plain_len = 4u;
		plain[0] = SE050_CLA;
		plain[1] = ins;
		plain[2] = p1;
		plain[3] = p2;
		if (payload_len > 0xFFu) {
			/* Erweiterte Laenge, siehe plain_exchange(). Die
			 * SCP03-Verpackung erkennt diese Form am Nullbyte
			 * im Lc-Feld. */
			plain[4] = 0x00u;
			plain[5] = (uint8_t)(payload_len >> 8);
			plain[6] = (uint8_t)(payload_len & 0xFFu);
			memcpy(&plain[7], payload, payload_len);
			plain_len = 7u + payload_len;
		} else if (payload_len > 0) {
			plain[4] = (uint8_t)payload_len;
			memcpy(&plain[5], payload, payload_len);
			plain_len = 5u + payload_len;
		}

		uint8_t inner[APDU_BUF];
		size_t inner_len = 0;
		r = osk_scp03_wrap(&s->scp, plain, plain_len, inner, sizeof(inner), &inner_len);
		osk_wipe(plain, plain_len);
		if (r != OSK_OK) {
			return r;
		}

		r = session_cmd(a, s->id, inner, inner_len, raw, sizeof(raw), &raw_len);
		osk_wipe(inner, inner_len);
		if (r != OSK_OK) {
			return r;
		}

		/* Die Antwort von ProcessSessionCmd IST die innere Antwort
		 * (AN12413 Tabelle 61) - also noch verpackt, samt
		 * Statuswort. Nichts davor abschneiden. */
		uint8_t body_buf[APDU_BUF];
		r = osk_scp03_unwrap(&s->scp, raw, raw_len, body_buf, sizeof(body_buf), &body, &sw);
		osk_wipe(raw, raw_len);
		if (r != OSK_OK) {
			/* Ein falscher R-MAC heisst: die Antwort ist nicht
			 * die, die der Baustein geschickt hat. Dann ist die
			 * Sitzung verbrannt - weitermachen waere das
			 * Gegenteil von sicher. */
			if (r == OSK_ERR_DENIED) {
				osk_se050_apdu_session_close(a, s);
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

osk_result_t osk_se050_apdu_exchange(osk_se050_apdu_t *a, uint8_t ins, uint8_t p1, uint8_t p2,
				     const uint8_t *payload, size_t payload_len, uint8_t *rsp,
				     size_t rsp_cap, size_t *rsp_len)
{
	if (a == NULL) {
		return OSK_ERR_ARG;
	}
	return osk_se050_apdu_exchange_in(a, &a->chan, ins, p1, p2, payload, payload_len, rsp,
					  rsp_cap, rsp_len);
}
