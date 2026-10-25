/* Die NFC-Weiche. Siehe nfc_tag.h. */
#include <string.h>
#include "nfc_tag.h"
#include "nfc_apdu.h"
#include "nfc_ctap.h"
#include "nfc_ndef.h"
#include "osk_transport.h"
#include "osk_ctap.h"

#define INS_SELECT 0xa4u
/* Auswahl nach Namen (ISO/IEC 7816-4, Abschnitt 11.1.1). */
#define SELECT_P1_BY_NAME 0x04u
#define SELECT_P2_FIRST 0x00u

static bool g_on = true;
static osk_nfc_app_t g_app;

static const uint8_t g_aid_fido[OSK_NFC_FIDO_AID_LEN] = OSK_NFC_FIDO_AID;
static const uint8_t g_aid_ndef[OSK_NFC_NDEF_AID_LEN] = OSK_NFC_NDEF_AID;

/* Vergleich ohne Abbruch beim ersten Unterschied. Die Kennung ist kein
 * Geheimnis, aber diese Form verraet nirgends, wie weit sie gekommen ist -
 * und sie ist hier nicht langsamer. */
static bool same_bytes(const uint8_t *a, const uint8_t *b, size_t len)
{
	uint8_t diff = 0;

	for (size_t i = 0; i < len; i++)
		diff |= (uint8_t)(a[i] ^ b[i]);
	return diff == 0;
}

static size_t select_by_name(const osk_apdu_t *c, uint8_t *out, size_t cap)
{
	if (c->p2 != SELECT_P2_FIRST)
		return osk_apdu_only_status(out, cap, OSK_SW_WRONG_P1P2);
	if (!c->data || c->lc == 0)
		return osk_apdu_only_status(out, cap, OSK_SW_WRONG_LENGTH);

	if (c->lc == sizeof(g_aid_fido) && same_bytes(c->data, g_aid_fido, sizeof(g_aid_fido))) {
		g_app = OSK_NFC_APP_FIDO;
		return osk_nfc_ctap_select(out, cap);
	}
	if (c->lc == sizeof(g_aid_ndef) && same_bytes(c->data, g_aid_ndef, sizeof(g_aid_ndef))) {
		osk_ndef_config_t cfg;

		osk_ndef_get_config(&cfg);
		/* Abgeschaltet heisst: es gibt diese Anwendung nicht. Kein
		 * Hinweis darauf, dass sie eingeschaltet werden koennte. */
		if (cfg.mode == OSK_NDEF_OFF)
			return osk_apdu_only_status(out, cap, OSK_SW_FILE_NOT_FOUND);
		g_app = OSK_NFC_APP_NDEF;
		osk_ndef_deselect();
		return osk_apdu_only_status(out, cap, OSK_SW_OK);
	}

	/* Eine fremde Kennung waehlt nichts ab: die laufende Anwendung bleibt,
	 * damit ein Lesegeraet nicht durch Probieren eine Sitzung abraeumt. */
	return osk_apdu_only_status(out, cap, OSK_SW_FILE_NOT_FOUND);
}

/* Hier gibt ISO-DEP die fertig zusammengesetzte APDU herein. */
static size_t route(const uint8_t *apdu, size_t len, uint8_t *out, size_t cap, void *ctx)
{
	osk_apdu_t c;

	(void)ctx;
	if (!g_on || !apdu || !out || cap < 2)
		return 0;

	/* Die Anwendungsauswahl beantwortet die Weiche selbst - alles andere
	 * geht an die gewaehlte Anwendung. */
	if (osk_nfc_apdu_parse(apdu, len, &c) && c.cla == 0x00u && c.ins == INS_SELECT &&
	    c.p1 == SELECT_P1_BY_NAME)
		return select_by_name(&c, out, cap);

	switch (g_app) {
	case OSK_NFC_APP_FIDO:
		return osk_nfc_ctap_apdu(apdu, len, out, cap);
	case OSK_NFC_APP_NDEF:
		return osk_ndef_apdu(apdu, len, out, cap);
	case OSK_NFC_APP_NONE:
	default:
		/* Ohne Auswahl gibt es keine Datei. */
		return osk_apdu_only_status(out, cap, OSK_SW_COND_NOT_SATISFIED);
	}
}

void osk_nfc_tag_enable(bool on)
{
	g_on = on;
	if (!on)
		osk_nfc_tag_field_off();
}

bool osk_nfc_tag_enabled(void)
{
	return g_on;
}

void osk_nfc_tag_field_on(void)
{
	g_app = OSK_NFC_APP_NONE;
	osk_nfc_ctap_reset();
	osk_ndef_deselect();
	if (g_on)
		osk_isodep_field_on();
}

void osk_nfc_tag_field_off(void)
{
	g_app = OSK_NFC_APP_NONE;
	osk_nfc_ctap_reset();
	osk_ndef_deselect();
	osk_isodep_field_off();
}

void osk_nfc_tag_rx_frame(const uint8_t *frame, size_t len)
{
	if (!g_on)
		return;
	osk_isodep_rx_frame(frame, len);
}

void osk_nfc_tag_tick(void)
{
	osk_isodep_tick();
}

osk_nfc_app_t osk_nfc_tag_selected(void)
{
	return g_app;
}

osk_result_t osk_nfc_tag_init(osk_isodep_send_fn send, void *ctx)
{
	osk_result_t rc;

	if (!send)
		return OSK_ERR_ARG;

	g_on = true;
	g_app = OSK_NFC_APP_NONE;
	osk_nfc_ctap_reset();
	osk_ndef_reset();

	rc = osk_isodep_init(send, route, ctx);
	if (rc != OSK_OK)
		return rc;

	/* Derselbe Bearbeiter wie am USB - das ist der ganze Punkt eines
	 * transportunabhaengigen Kerns. */
	return osk_transport_register(OSK_LINK_NFC, osk_ctap_handle);
}
