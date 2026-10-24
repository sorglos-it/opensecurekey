/* Das FIDO-Applet ueber NFC. Siehe nfc_ctap.h.
 * Spezifikationen: CTAP 2.1 Abschnitt 11.3, ISO/IEC 7816-4 Abschnitt 5.1. */
#include <string.h>
#include "nfc_ctap.h"
#include "nfc_apdu.h"
#include "nfc_isodep.h"
#include "osk_transport.h"
#include "osk_ctap.h"

/* Bit in P1 von NFCCTAP_MSG: das Gegenueber kann NFCCTAP_GETRESPONSE. */
#define P1_SUPPORTS_GETRESPONSE 0x80u

/* U2F-Befehle (CTAP1), die ueber dasselbe Applet laufen. */
#define U2F_REGISTER 0x01u
#define U2F_AUTHENTICATE 0x02u
#define U2F_VERSION 0x03u

/* So viel gibt eine kurze Antwort je Stueck heraus (ISO/IEC 7816-4). */
#define SHORT_MAX 256u

typedef struct {
	bool active; /* eine Befehlskette wird gerade gesammelt */
	size_t len;
	uint8_t buf[OSK_NFC_CTAP_BUF_LEN];
} chain_t;

typedef struct {
	size_t len;
	size_t at;
	uint8_t buf[OSK_NFC_CTAP_BUF_LEN];
} pending_t;

static chain_t g_in;
static pending_t g_out;
static bool g_deselected;

static void in_reset(void)
{
	/* Der Puffer kann eine Nachricht mit Geheimnissen getragen haben. */
	memset(&g_in, 0, sizeof(g_in));
}

static void out_reset(void)
{
	memset(&g_out, 0, sizeof(g_out));
}

void osk_nfc_ctap_reset(void)
{
	in_reset();
	out_reset();
	g_deselected = false;
}

bool osk_nfc_ctap_deselected(void)
{
	return g_deselected;
}

size_t osk_nfc_ctap_select(uint8_t *out, size_t cap)
{
	/* "U2F_V2": dieses Geraet kann CTAP1 UND CTAP2, und genau dafuer
	 * sieht die Spezifikation diese Antwort vor (Abschnitt 11.3.3).
	 * Ein Programm, das CTAP2 kann, fragt danach authenticatorGetInfo. */
	static const uint8_t version[] = {'U', '2', 'F', '_', 'V', '2'};

	osk_nfc_ctap_reset();
	if (!out || cap < sizeof(version) + 2)
		return 0;
	memcpy(out, version, sizeof(version));
	return osk_apdu_status(out, cap, sizeof(version), OSK_SW_OK);
}

/* Gibt das naechste Stueck einer langen Antwort heraus. Passt der Rest
 * nicht in ein Stueck, endet die Antwort mit 61xx und das Gegenueber holt
 * mit GET RESPONSE weiter. */
static size_t deliver(uint8_t *out, size_t cap, size_t want, bool extended)
{
	const size_t left = g_out.len - g_out.at;
	size_t take = left;

	if (!extended) {
		/* Kurze Laengen: hoechstens 256 Byte je Antwort. */
		if (take > SHORT_MAX)
			take = SHORT_MAX;
	}
	if (want != 0 && take > want)
		take = want;
	if (take + 2 > cap)
		return osk_apdu_only_status(out, cap, OSK_SW_WRONG_LENGTH);
	if (take)
		memcpy(out, g_out.buf + g_out.at, take);
	g_out.at += take;

	const size_t rest = g_out.len - g_out.at;

	if (rest == 0) {
		const size_t n = osk_apdu_status(out, cap, take, OSK_SW_OK);

		out_reset();
		return n;
	}
	/* 61xx: so viele Bytes stehen noch bereit (0 heisst "mehr als 255"). */
	const uint8_t more = (rest > 255) ? 0u : (uint8_t)rest;

	return osk_apdu_status(out, cap, take, (uint16_t)(0x6100u | more));
}

/* Legt eine fertige Antwort bereit und gibt das erste Stueck heraus. */
static size_t answer(const uint8_t *data, size_t len, uint8_t *out, size_t cap, size_t want,
		     bool extended)
{
	out_reset();
	if (len > sizeof(g_out.buf))
		return osk_apdu_only_status(out, cap, OSK_SW_NO_PRECISE_DIAGNOSIS);
	if (len)
		memcpy(g_out.buf, data, len);
	g_out.len = len;
	g_out.at = 0;
	return deliver(out, cap, want, extended);
}

/* CTAP2 bearbeiten: der Kern bekommt genau die Bytes, die am USB auch
 * ankaemen - Befehlsbyte plus CBOR. */
static size_t do_ctap_msg(const osk_apdu_t *c, uint8_t *out, size_t cap)
{
	uint8_t body[OSK_NFC_CTAP_BUF_LEN];
	size_t body_len;
	uint8_t res_buf[OSK_NFC_CTAP_BUF_LEN];
	osk_request_t req;
	osk_response_t res;

	/* P2 ist nicht vergeben und muss 0 sein; in P1 ist nur das Bit fuer
	 * NFCCTAP_GETRESPONSE erlaubt. */
	if (c->p2 != 0 || (c->p1 & (uint8_t)~P1_SUPPORTS_GETRESPONSE) != 0)
		return osk_apdu_only_status(out, cap, OSK_SW_WRONG_P1P2);

	/* Ein Kettenglied: anhaengen und bestaetigen. Die Laenge wird vor dem
	 * Kopieren gegen den Rest des Puffers geprueft. */
	if (c->lc > sizeof(g_in.buf) - g_in.len) {
		in_reset();
		return osk_apdu_only_status(out, cap, OSK_SW_WRONG_LENGTH);
	}
	if (c->lc && c->data)
		memcpy(g_in.buf + g_in.len, c->data, c->lc);
	g_in.len += c->lc;
	g_in.active = true;

	if (c->chained)
		return osk_apdu_only_status(out, cap, OSK_SW_OK);

	body_len = g_in.len;
	if (body_len == 0) {
		in_reset();
		return osk_apdu_only_status(out, cap, OSK_SW_WRONG_LENGTH);
	}
	memcpy(body, g_in.buf, body_len);
	in_reset();

	/* Der Nutzer muss druecken, und das dauert. Ohne diese Bitte bricht
	 * das Lesegeraet nach seiner eigenen Wartezeit ab. */
	osk_isodep_request_more_time(OSK_ISODEP_WTX_MAX);

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));
	req.link = OSK_LINK_NFC;
	req.data = body;
	req.len = body_len;
	res.data = res_buf;
	res.cap = sizeof(res_buf);

	const osk_result_t rc = osk_ctap_handle(&req, &res);
	size_t n;

	if (rc != OSK_OK || res.len < 1) {
		n = osk_apdu_only_status(out, cap, OSK_SW_NO_PRECISE_DIAGNOSIS);
	} else {
		n = answer(res_buf, res.len, out, cap, c->le, c->extended);
	}
	memset(body, 0, sizeof(body));
	memset(res_buf, 0, sizeof(res_buf));
	return n;
}

/* CTAP1/U2F: die rohe APDU geht durch, die Antwort traegt ihr Statuswort
 * schon selbst. */
static size_t do_u2f(const uint8_t *apdu, size_t len, const osk_apdu_t *c, uint8_t *out, size_t cap)
{
	uint8_t res_buf[OSK_NFC_CTAP_BUF_LEN];
	osk_request_t req;
	osk_response_t res;

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));
	req.link = OSK_LINK_NFC;
	req.data = apdu;
	req.len = len;
	res.data = res_buf;
	res.cap = sizeof(res_buf);

	osk_isodep_request_more_time(OSK_ISODEP_WTX_MAX);

	size_t n;

	if (osk_ctap_handle_u2f(&req, &res) != OSK_OK || res.len < 2) {
		n = osk_apdu_only_status(out, cap, OSK_SW_NO_PRECISE_DIAGNOSIS);
	} else if (res.len > cap) {
		n = osk_apdu_only_status(out, cap, OSK_SW_WRONG_LENGTH);
	} else {
		/* Das Statuswort steckt schon in der Antwort des Kerns, darum
		 * wird hier nichts angehaengt und auch nichts gekettet: die
		 * U2F-Antworten dieses Geraets passen in einen Rahmen. */
		memcpy(out, res_buf, res.len);
		n = res.len;
		(void)c;
	}
	memset(res_buf, 0, sizeof(res_buf));
	return n;
}

size_t osk_nfc_ctap_apdu(const uint8_t *apdu, size_t len, uint8_t *out, size_t cap)
{
	osk_apdu_t c;

	if (!apdu || !out || cap < 2)
		return 0;
	if (g_deselected)
		return osk_apdu_only_status(out, cap, OSK_SW_COND_NOT_SATISFIED);
	if (!osk_apdu_parse(apdu, len, &c))
		return osk_apdu_only_status(out, cap, OSK_SW_WRONG_LENGTH);

	/* Eine neue Anfrage, obwohl noch eine Antwort bereitliegt: die alte
	 * wird verworfen. Nur GET RESPONSE holt sie weiter ab. */
	if (c.ins != OSK_NFC_INS_GET_RESPONSE && c.ins != OSK_NFC_INS_CTAP_GETRESPONSE && g_out.len)
		out_reset();

	/* CTAP2-Befehle: Klassenbyte 0x80, Kettenglieder 0x90. */
	if (c.cla == 0x80u || c.cla == 0x90u) {
		switch (c.ins) {
		case OSK_NFC_INS_CTAP_MSG:
			return do_ctap_msg(&c, out, cap);
		case OSK_NFC_INS_CTAP_GETRESPONSE:
			/* P1 und P2 sind nicht vergeben. */
			if (c.p1 != 0 || c.p2 != 0)
				return osk_apdu_only_status(out, cap, OSK_SW_WRONG_P1P2);
			if (g_out.len == 0)
				return osk_apdu_only_status(out, cap, OSK_SW_COND_NOT_SATISFIED);
			return deliver(out, cap, c.le, c.extended);
		case OSK_NFC_INS_CTAP_CONTROL:
			/* Abwaehlen (Abschnitt 11.3.4): danach wird kein
			 * CTAP-Befehl mehr beantwortet. */
			if (c.p1 != 0x01u || c.p2 != 0x00u)
				return osk_apdu_only_status(out, cap, OSK_SW_WRONG_P1P2);
			osk_nfc_ctap_reset();
			g_deselected = true;
			return osk_apdu_only_status(out, cap, OSK_SW_OK);
		default:
			return osk_apdu_only_status(out, cap, OSK_SW_INS_NOT_SUPPORTED);
		}
	}

	if (c.cla == 0x00u) {
		switch (c.ins) {
		case U2F_REGISTER:
		case U2F_AUTHENTICATE:
		case U2F_VERSION:
			return do_u2f(apdu, len, &c, out, cap);
		case OSK_NFC_INS_GET_RESPONSE:
			if (g_out.len == 0)
				return osk_apdu_only_status(out, cap, OSK_SW_COND_NOT_SATISFIED);
			return deliver(out, cap, c.le, c.extended);
		default:
			return osk_apdu_only_status(out, cap, OSK_SW_INS_NOT_SUPPORTED);
		}
	}

	return osk_apdu_only_status(out, cap, OSK_SW_CLA_NOT_SUPPORTED);
}
