/* Verteiler. Siehe osk_applets.h.
 * Grundlage: ISO/IEC 7816-4, Abschnitte 5.1.1 (CLA), 5.3.4 (Verkettung),
 * 11.1.1 (SELECT) und 11.1.2 (GET RESPONSE). */
#include <string.h>
#include "osk_applets.h"

#define INS_SELECT 0xa4u
#define INS_GET_RESPONSE 0xc0u

/* Sichere Nachrichtenuebermittlung auf der Karte bieten wir nicht an: der
 * Weg zum Rechner ist USB, und das Geheimnis liegt ohnehin im SE050. */
#define SW_SM_NOT_SUPPORTED 0x6882u

static const osk_applet_t *g_applets[2];
static unsigned g_count;
static const osk_applet_t *g_selected;

/* Was noch abzuholen ist (Antwortverkettung). */
static uint8_t g_pending[OSK_APDU_MAX_RESP];
static size_t g_pending_len;
static size_t g_pending_at;

/* Was schon hereingekommen ist (Kommandoverkettung). */
static uint8_t g_chain[OSK_APDU_MAX_DATA];
static size_t g_chain_len;
static bool g_chain_open;
static uint8_t g_chain_ins;
static uint8_t g_chain_p1;
static uint8_t g_chain_p2;

static void chain_drop(void)
{
	g_chain_open = false;
	g_chain_len = 0u;
	g_chain_ins = 0u;
	g_chain_p1 = 0u;
	g_chain_p2 = 0u;
	memset(g_chain, 0, sizeof(g_chain));
}

static void pending_drop(void)
{
	g_pending_len = 0u;
	g_pending_at = 0u;
	memset(g_pending, 0, sizeof(g_pending));
}

void osk_applets_reset(void)
{
	g_selected = NULL;
	chain_drop();
	pending_drop();
	for (unsigned i = 0; i < g_count; i++) {
		if (g_applets[i]->reset != NULL)
			g_applets[i]->reset();
	}
}

/* Statuswort ans Ende der Antwort. */
static void put_sw(osk_response_t *res, uint16_t sw)
{
	if (res->cap < res->len + 2u) {
		/* Zwei Bytes muessen immer passen, sonst kann die Karte nicht
		 * einmal einen Fehler melden. */
		res->len = 0u;
		if (res->cap < 2u)
			return;
	}
	res->data[res->len++] = (uint8_t)(sw >> 8);
	res->data[res->len++] = (uint8_t)sw;
}

/* Nur das Statuswort, ohne Daten. */
static osk_result_t only_sw(osk_response_t *res, uint16_t sw)
{
	res->len = 0u;
	put_sw(res, sw);
	return OSK_OK;
}

/* Gibt so viel von der offenen Antwort heraus, wie das Kommando erlaubt, und
 * kuendigt den Rest mit 61xx an. */
static osk_result_t deliver(osk_response_t *res, size_t want)
{
	size_t left = g_pending_len - g_pending_at;
	size_t take;
	size_t rest;

	if (want == 0u)
		want = 256u;
	take = (left < want) ? left : want;
	if (take + 2u > res->cap)
		take = (res->cap > 2u) ? res->cap - 2u : 0u;

	res->len = 0u;
	if (take != 0u) {
		memcpy(res->data, g_pending + g_pending_at, take);
		res->len = take;
	}
	g_pending_at += take;
	rest = g_pending_len - g_pending_at;

	if (rest == 0u) {
		pending_drop();
		put_sw(res, OSK_SW_OK);
	} else {
		/* 61xx: xx ist die Zahl der noch wartenden Bytes, 00 heisst
		 * "mehr als 255". */
		put_sw(res, (uint16_t)(OSK_SW_MORE | (rest > 255u ? 0u : rest)));
	}
	return OSK_OK;
}

/* Sucht die Anwendung, deren Kennung mit den gegebenen Bytes beginnt. */
static const osk_applet_t *find_applet(const uint8_t *aid, size_t len)
{
	if (len == 0u || len > 16u)
		return NULL;
	for (unsigned i = 0; i < g_count; i++) {
		const osk_applet_t *a = g_applets[i];

		if (len <= a->aid_len && memcmp(a->aid, aid, len) == 0)
			return a;
	}
	return NULL;
}

osk_result_t osk_applets_handle(const osk_request_t *req, osk_response_t *res)
{
	osk_apdu_t apdu;
	osk_apdu_t work;
	osk_resp_t out;
	uint8_t body[OSK_APDU_MAX_RESP];
	uint16_t sw;
	osk_result_t rc;

	if (req == NULL || res == NULL || res->data == NULL || res->cap < 2u)
		return OSK_ERR_ARG;

	rc = osk_apdu_parse(req->data, req->len, &apdu);
	if (rc != OSK_OK) {
		chain_drop();
		pending_drop();
		/* Zu lang und krumm gebaut sind fuer die Gegenseite dasselbe:
		 * das Kommando hatte nicht die Laenge, die es haben muesste. */
		return only_sw(res, OSK_SW_WRONG_LENGTH);
	}

	/* Logische Kanaele ausser 0 gibt es bei uns nicht, und sichere
	 * Nachrichtenuebermittlung auch nicht. */
	if (apdu.channel != 0u)
		return only_sw(res, OSK_SW_CLA_NOT_SUPPORTED);
	if ((apdu.cla & 0x0cu) != 0u)
		return only_sw(res, SW_SM_NOT_SUPPORTED);
	if ((apdu.cla & 0xe0u) != 0x00u && (apdu.cla & 0xe0u) != 0x80u)
		return only_sw(res, OSK_SW_CLA_NOT_SUPPORTED);

	/* GET RESPONSE holt die offene Antwort ab - und nur die. */
	if (apdu.ins == INS_GET_RESPONSE && (apdu.cla & 0xe0u) == 0x00u) {
		if (g_pending_len == 0u || g_pending_at >= g_pending_len) {
			pending_drop();
			return only_sw(res, OSK_SW_COND_NOT_SATISFIED);
		}
		if (apdu.lc != 0u)
			return only_sw(res, OSK_SW_WRONG_LENGTH);
		return deliver(res, apdu.has_le ? apdu.le : 256u);
	}

	/* Jedes andere Kommando laesst eine offene Antwort verfallen. */
	pending_drop();

	/* Kommandoverkettung: sammeln, bis das letzte Stueck kommt. */
	if (apdu.chained) {
		if (g_chain_open &&
		    (g_chain_ins != apdu.ins || g_chain_p1 != apdu.p1 || g_chain_p2 != apdu.p2)) {
			chain_drop();
			return only_sw(res, OSK_SW_COND_NOT_SATISFIED);
		}
		if (g_chain_len + apdu.lc > sizeof(g_chain)) {
			chain_drop();
			return only_sw(res, OSK_SW_WRONG_LENGTH);
		}
		if (apdu.lc != 0u)
			memcpy(g_chain + g_chain_len, apdu.data, apdu.lc);
		g_chain_len += apdu.lc;
		g_chain_open = true;
		g_chain_ins = apdu.ins;
		g_chain_p1 = apdu.p1;
		g_chain_p2 = apdu.p2;
		return only_sw(res, OSK_SW_OK);
	}

	work = apdu;
	if (g_chain_open) {
		if (g_chain_ins != apdu.ins || g_chain_p1 != apdu.p1 || g_chain_p2 != apdu.p2) {
			chain_drop();
			return only_sw(res, OSK_SW_COND_NOT_SATISFIED);
		}
		if (g_chain_len + apdu.lc > sizeof(g_chain)) {
			chain_drop();
			return only_sw(res, OSK_SW_WRONG_LENGTH);
		}
		if (apdu.lc != 0u)
			memcpy(g_chain + g_chain_len, apdu.data, apdu.lc);
		g_chain_len += apdu.lc;
		work.data = g_chain;
		work.lc = g_chain_len;
		g_chain_open = false;
	}

	osk_resp_init(&out, body, sizeof(body));

	if (work.ins == INS_SELECT && (work.cla & 0xe0u) == 0x00u) {
		const osk_applet_t *found;

		if (work.p1 != 0x04u) {
			chain_drop();
			return only_sw(res, OSK_SW_WRONG_P1P2);
		}
		/* P2: nur "erstes oder einziges Vorkommen" (7816-4 Tabelle 63). */
		if (work.p2 != 0x00u) {
			chain_drop();
			return only_sw(res, OSK_SW_WRONG_P1P2);
		}
		found = find_applet(work.data, work.lc);
		if (found == NULL) {
			g_selected = NULL;
			chain_drop();
			return only_sw(res, OSK_SW_FILE_NOT_FOUND);
		}
		g_selected = found;
		sw = found->handle(&work, &out);
	} else {
		if (g_selected == NULL) {
			chain_drop();
			return only_sw(res, OSK_SW_COND_NOT_SATISFIED);
		}
		sw = g_selected->handle(&work, &out);
	}

	/* Erst JETZT den Sammelpuffer leeren: `work.data` hat bis hierher
	 * hineingezeigt. Ihn vorher zu loeschen hat die Daten unter der
	 * Anwendung weggezogen - gefunden von test_piv, Kommandoverkettung. */
	chain_drop();

	if (out.overflow)
		return only_sw(res, OSK_SW_UNKNOWN);
	if (sw != OSK_SW_OK || out.len == 0u) {
		/* Bei einem Fehler gehen keine halben Daten hinaus. */
		if (sw != OSK_SW_OK)
			return only_sw(res, sw);
		return only_sw(res, OSK_SW_OK);
	}

	/* Antwort merken und so viel herausgeben, wie Le erlaubt. */
	memcpy(g_pending, out.buf, out.len);
	g_pending_len = out.len;
	g_pending_at = 0u;
	return deliver(res, work.has_le ? work.le : 256u);
}

osk_result_t osk_applets_init(void)
{
	g_count = 0u;
	g_applets[g_count++] = osk_piv_applet();
	g_applets[g_count++] = osk_openpgp_applet();
	osk_applets_reset();
	return OSK_OK;
}
