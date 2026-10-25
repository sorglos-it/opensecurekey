/* Antippen ohne App. Siehe nfc_ndef.h.
 * Spezifikationen: NFC Forum Type 4 Tag 1.1, NDEF 1.0, URI RTD 1.0,
 * Text RTD 1.0. */
#include <string.h>
#include "nfc_ndef.h"
#include "nfc_apdu.h"
#include "osk_ui.h"

/* Flaggen im Kopfbyte eines Datensatzes (NDEF 1.0, Abschnitt 3.2). */
#define NDEF_MB 0x80u
#define NDEF_ME 0x40u
#define NDEF_CF 0x20u
#define NDEF_SR 0x10u
#define NDEF_IL 0x08u
#define NDEF_TNF 0x07u
#define NDEF_TNF_WELL_KNOWN 0x01u
#define NDEF_TNF_RESERVED 0x07u

/* Befehle des Type-4-Tags. */
#define INS_SELECT 0xa4u
#define INS_READ_BINARY 0xb0u
#define INS_UPDATE_BINARY 0xd6u

/* Die Steuerdatei (CC) ist bei uns immer 15 Byte lang. */
#define CC_LEN 15u

static osk_ndef_config_t g_cfg;
static osk_ndef_otp_fn g_otp;
static void *g_otp_ctx;

/* Welche Datei das Lesegeraet gerade ausgewaehlt hat. 0 = keine. */
static uint16_t g_selected;

/* Die Datei dieser Sitzung. Sie wird EINMAL gebaut und dann unveraendert
 * gelesen: ein Einmalkennwort darf sich nicht zwischen zwei Teillesungen
 * aendern, sonst liest das Handy zwei halbe Kennwoerter. */
static uint8_t g_file[OSK_NDEF_FILE_MAX];
static size_t g_file_len;
static bool g_file_ready;
static bool g_granted; /* der Taster wurde in dieser Sitzung gedrueckt */

/* Laenge einer Zeichenkette, die hoechstens cap Bytes lang sein darf.
 * Selbst gebaut, weil die begrenzte Variante aus der Standardbibliothek
 * nicht in jedem C11-Satz vorhanden ist. */
static size_t bounded_len(const char *text, size_t cap)
{
	size_t n = 0;

	while (n < cap && text[n])
		n++;
	return n;
}

static bool printable(const char *text, size_t len)
{
	for (size_t i = 0; i < len; i++) {
		const unsigned char c = (unsigned char)text[i];

		/* Nur sichtbares ASCII. Steuerzeichen in einer Adresse sind
		 * entweder ein Fehler oder ein Versuch. */
		if (c < 0x20u || c > 0x7eu)
			return false;
	}
	return true;
}

void osk_ndef_reset(void)
{
	memset(&g_cfg, 0, sizeof(g_cfg));
	g_cfg.mode = OSK_NDEF_OFF;
	g_otp = NULL;
	g_otp_ctx = NULL;
	osk_ndef_deselect();
}

void osk_ndef_deselect(void)
{
	g_selected = 0;
	g_file_ready = false;
	g_file_len = 0;
	g_granted = false;
	/* Die Datei kann ein Einmalkennwort getragen haben. */
	memset(g_file, 0, sizeof(g_file));
}

void osk_ndef_set_otp_source(osk_ndef_otp_fn fn, void *ctx)
{
	g_otp = fn;
	g_otp_ctx = ctx;
}

osk_result_t osk_ndef_configure(const osk_ndef_config_t *cfg)
{
	if (!cfg)
		return OSK_ERR_ARG;
	if (cfg->mode != OSK_NDEF_OFF && cfg->mode != OSK_NDEF_URI && cfg->mode != OSK_NDEF_OTP)
		return OSK_ERR_ARG;
	if (cfg->uri_prefix > OSK_NDEF_URI_HTTPS)
		return OSK_ERR_ARG;

	if (cfg->mode == OSK_NDEF_URI) {
		/* Die Zeichenkette muss innerhalb des Feldes enden. */
		const size_t n = bounded_len(cfg->uri, sizeof(cfg->uri));

		if (n == 0 || n > OSK_NDEF_URI_MAX)
			return OSK_ERR_FORMAT;
		if (!printable(cfg->uri, n))
			return OSK_ERR_FORMAT;
	}

	memcpy(&g_cfg, cfg, sizeof(g_cfg));
	g_cfg.uri[OSK_NDEF_URI_MAX] = 0;
	osk_ndef_deselect();
	return OSK_OK;
}

void osk_ndef_get_config(osk_ndef_config_t *out)
{
	if (out)
		memcpy(out, &g_cfg, sizeof(*out));
}

/* ---- Lesen -------------------------------------------------------------- */

bool osk_ndef_parse(const uint8_t *msg, size_t len, osk_ndef_record_t *out, size_t cap,
		    size_t *count_out)
{
	size_t at = 0;
	size_t count = 0;

	if (count_out)
		*count_out = 0;
	if (!msg || !out || cap == 0)
		return false;
	/* Eine leere Datei ist keine kaputte Datei. */
	if (len == 0)
		return true;

	while (at < len) {
		if (count >= cap || count >= OSK_NDEF_MAX_RECORDS)
			return false;

		/* Kopfbyte und Typlaenge: zwei Bytes muessen da sein. */
		if (len - at < 2)
			return false;
		const uint8_t flags = msg[at];
		const uint8_t tnf = (uint8_t)(flags & NDEF_TNF);
		const size_t type_len = (size_t)msg[at + 1];
		size_t p = at + 2;

		if (tnf == NDEF_TNF_RESERVED)
			return false;
		/* Geteilte Datensaetze nehmen wir nicht an - sie bringen nur
		 * einen zweiten Zustand in den Parser. */
		if (flags & NDEF_CF)
			return false;
		/* MB nur am Anfang, ME nur am Ende. */
		if ((count == 0) != ((flags & NDEF_MB) != 0))
			return false;

		size_t payload_len;

		if (flags & NDEF_SR) {
			if (len - p < 1)
				return false;
			payload_len = (size_t)msg[p];
			p += 1;
		} else {
			if (len - p < 4)
				return false;
			payload_len = ((size_t)msg[p] << 24) | ((size_t)msg[p + 1] << 16) |
				      ((size_t)msg[p + 2] << 8) | (size_t)msg[p + 3];
			p += 4;
		}

		size_t id_len = 0;

		if (flags & NDEF_IL) {
			if (len - p < 1)
				return false;
			id_len = (size_t)msg[p];
			p += 1;
		}

		/* Jede Laenge wird gegen den Rest geprueft, bevor ein Zeiger
		 * darauf gesetzt wird. Die Summe kann nicht ueberlaufen, weil
		 * jeder Summand einzeln gegen den Rest geprueft wird. */
		if (len - p < type_len)
			return false;
		out[count].type = type_len ? (msg + p) : NULL;
		out[count].type_len = type_len;
		p += type_len;

		if (len - p < id_len)
			return false;
		out[count].id = id_len ? (msg + p) : NULL;
		out[count].id_len = id_len;
		p += id_len;

		if (len - p < payload_len)
			return false;
		out[count].payload = payload_len ? (msg + p) : NULL;
		out[count].payload_len = payload_len;
		p += payload_len;

		out[count].tnf = tnf;
		out[count].first = (flags & NDEF_MB) != 0;
		out[count].last = (flags & NDEF_ME) != 0;
		count++;
		at = p;

		if (flags & NDEF_ME)
			break;
	}

	/* Der letzte gelesene Datensatz muss das Ende markiert haben, und
	 * hinter dem Ende darf nichts mehr stehen. */
	if (count == 0 || !out[count - 1].last || at != len)
		return false;

	if (count_out)
		*count_out = count;
	return true;
}

/* ---- Bauen -------------------------------------------------------------- */

/* Schreibt einen kurzen Datensatz mit einem Typ von einem Byte. */
static size_t put_short_record(uint8_t *out, size_t cap, uint8_t type, const uint8_t *payload,
			       size_t payload_len)
{
	const size_t need = 4 + payload_len;

	if (payload_len > 255 || need > cap)
		return 0;
	out[0] = (uint8_t)(NDEF_MB | NDEF_ME | NDEF_SR | NDEF_TNF_WELL_KNOWN);
	out[1] = 1;
	out[2] = (uint8_t)payload_len;
	out[3] = type;
	if (payload_len)
		memcpy(out + 4, payload, payload_len);
	return need;
}

size_t osk_ndef_build_file(uint8_t *out, size_t cap)
{
	uint8_t payload[1 + OSK_NDEF_URI_MAX];
	size_t payload_len = 0;
	uint8_t type;

	if (!out || cap < 2)
		return 0;

	switch (g_cfg.mode) {
	case OSK_NDEF_URI: {
		const size_t n = bounded_len(g_cfg.uri, sizeof(g_cfg.uri));

		if (n == 0 || n > OSK_NDEF_URI_MAX)
			return 0;
		type = 'U';
		payload[0] = g_cfg.uri_prefix;
		memcpy(payload + 1, g_cfg.uri, n);
		payload_len = n + 1;
		break;
	}
	case OSK_NDEF_OTP: {
		char otp[OSK_NDEF_OTP_MAX + 1];
		size_t n;

		if (!g_otp)
			return 0;
		memset(otp, 0, sizeof(otp));
		n = g_otp(otp, OSK_NDEF_OTP_MAX, g_otp_ctx);
		if (n == 0 || n > OSK_NDEF_OTP_MAX || !printable(otp, n)) {
			memset(otp, 0, sizeof(otp));
			return 0;
		}
		/* Text-Datensatz: Statusbyte (UTF-8, Sprachkennung 2 Byte),
		 * dann die Sprache, dann der Text. */
		type = 'T';
		payload[0] = 0x02;
		payload[1] = 'e';
		payload[2] = 'n';
		memcpy(payload + 3, otp, n);
		payload_len = n + 3;
		memset(otp, 0, sizeof(otp));
		break;
	}
	case OSK_NDEF_OFF:
	default:
		return 0;
	}

	const size_t msg = put_short_record(out + 2, cap - 2, type, payload, payload_len);

	memset(payload, 0, sizeof(payload));
	if (msg == 0)
		return 0;
	out[0] = (uint8_t)(msg >> 8);
	out[1] = (uint8_t)msg;
	return msg + 2;
}

/* ---- Die Anwendung ------------------------------------------------------ */

static size_t cc_file(uint8_t *out, size_t cap)
{
	if (cap < CC_LEN)
		return 0;
	/* Laenge der Steuerdatei */
	out[0] = 0x00;
	out[1] = (uint8_t)CC_LEN;
	/* Abbildungsfassung 2.0 */
	out[2] = 0x20;
	/* Groesste Antwort und groesster Befehl, die wir annehmen */
	out[3] = 0x00;
	out[4] = 0xf6;
	out[5] = 0x00;
	out[6] = 0xf6;
	/* Steuerblock der NDEF-Datei */
	out[7] = 0x04;
	out[8] = 0x06;
	out[9] = (uint8_t)(OSK_NDEF_DATA_FILE_ID >> 8);
	out[10] = (uint8_t)OSK_NDEF_DATA_FILE_ID;
	out[11] = (uint8_t)(OSK_NDEF_FILE_MAX >> 8);
	out[12] = (uint8_t)OSK_NDEF_FILE_MAX;
	out[13] = 0x00; /* lesen: frei */
	out[14] = 0xff; /* schreiben: nie */
	return CC_LEN;
}

/* Fragt den Taster, falls fuer die eingestellte Betriebsart noetig. */
static bool allowed(void)
{
	osk_ask_req_t ask;

	if (g_granted)
		return true;
	if (g_cfg.mode == OSK_NDEF_URI && g_cfg.uri_without_button) {
		g_granted = true;
		return true;
	}

	memset(&ask, 0, sizeof(ask));
	ask.kind = (g_cfg.mode == OSK_NDEF_OTP) ? OSK_ASK_SECRET : OSK_ASK_LOGIN;
	ask.subject = (g_cfg.mode == OSK_NDEF_OTP) ? "ndef.otp" : "ndef.uri";
	ask.asked_by = "nfc";
	ask.timeout_ms = 15000;
	if (osk_ui_confirm(&ask) != OSK_OK)
		return false;
	g_granted = true;
	return true;
}

/* Baut die Datei dieser Sitzung, einmal. */
static bool ensure_file(void)
{
	if (g_file_ready)
		return g_file_len > 0;
	g_file_len = osk_ndef_build_file(g_file, sizeof(g_file));
	g_file_ready = true;
	return g_file_len > 0;
}

static size_t do_select(const osk_apdu_t *c, uint8_t *out, size_t cap)
{
	/* Nur die Auswahl nach Dateikennung. Alles andere macht nfc_tag. */
	if (c->p1 != 0x00u || c->p2 != 0x0cu)
		return osk_apdu_only_status(out, cap, OSK_SW_WRONG_P1P2);
	if (c->lc != 2 || !c->data)
		return osk_apdu_only_status(out, cap, OSK_SW_WRONG_LENGTH);

	const uint16_t id = (uint16_t)(((uint16_t)c->data[0] << 8) | c->data[1]);

	if (id != OSK_NDEF_CC_FILE_ID && id != OSK_NDEF_DATA_FILE_ID)
		return osk_apdu_only_status(out, cap, OSK_SW_FILE_NOT_FOUND);
	g_selected = id;
	return osk_apdu_only_status(out, cap, OSK_SW_OK);
}

static size_t do_read(const osk_apdu_t *c, uint8_t *out, size_t cap)
{
	uint8_t buf[OSK_NDEF_FILE_MAX];
	const uint8_t *src;
	size_t src_len;
	const size_t offset = ((size_t)c->p1 << 8) | (size_t)c->p2;
	size_t want = c->has_le ? c->le : 0;

	if (g_selected == OSK_NDEF_CC_FILE_ID) {
		src_len = cc_file(buf, sizeof(buf));
		src = buf;
	} else if (g_selected == OSK_NDEF_DATA_FILE_ID) {
		if (!allowed())
			return osk_apdu_only_status(out, cap, OSK_SW_COND_NOT_SATISFIED);
		if (!ensure_file())
			return osk_apdu_only_status(out, cap, OSK_SW_COND_NOT_SATISFIED);
		src = g_file;
		src_len = g_file_len;
	} else {
		return osk_apdu_only_status(out, cap, OSK_SW_COND_NOT_SATISFIED);
	}

	/* Hinter dem Ende wird nicht gelesen - und auch nicht bis dorthin. */
	if (offset > src_len)
		return osk_apdu_only_status(out, cap, OSK_SW_WRONG_P1P2);

	const size_t left = src_len - offset;

	if (want == 0 || want > left)
		want = left;
	if (want + 2 > cap)
		return osk_apdu_only_status(out, cap, OSK_SW_WRONG_LENGTH);
	if (want)
		memcpy(out, src + offset, want);
	return osk_apdu_status(out, cap, want, OSK_SW_OK);
}

size_t osk_ndef_apdu(const uint8_t *apdu, size_t len, uint8_t *out, size_t cap)
{
	osk_apdu_t c;

	if (!apdu || !out || cap < 2)
		return 0;
	if (g_cfg.mode == OSK_NDEF_OFF)
		return osk_apdu_only_status(out, cap, OSK_SW_FILE_NOT_FOUND);
	if (!osk_nfc_apdu_parse(apdu, len, &c))
		return osk_apdu_only_status(out, cap, OSK_SW_WRONG_LENGTH);
	/* Diese Anwendung kennt nur das Klassenbyte ohne jede Option. */
	if (c.cla != 0x00u)
		return osk_apdu_only_status(out, cap, OSK_SW_CLA_NOT_SUPPORTED);

	switch (c.ins) {
	case INS_SELECT:
		return do_select(&c, out, cap);
	case INS_READ_BINARY:
		return do_read(&c, out, cap);
	case INS_UPDATE_BINARY:
		/* Schreiben ist nicht vorgesehen, und das sagen wir klar -
		 * nicht durch stilles Nichtstun. */
		return osk_apdu_only_status(out, cap, OSK_SW_COND_NOT_SATISFIED);
	default:
		return osk_apdu_only_status(out, cap, OSK_SW_INS_NOT_SUPPORTED);
	}
}
