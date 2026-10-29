/* PIV. Siehe osk_piv.h.
 * Spezifikation: NIST SP 800-73-4, Teil 1 (Datenobjekte, Tabelle 3 und
 * Tabelle 6-2) und Teil 2 (Kommandos, Abschnitte 3.1 bis 3.3). */
#include <string.h>
#include <stdio.h>
#include "osk_piv.h"
#include "osk_card.h"
#include "osk_apdu.h"
#include "osk_se050.h"
#include "osk_store.h"
#include "osk_ui.h"
#include "osk_rsa_card.h"
#include "osk_attest.h"
#include "osk_serial.h"

const uint8_t osk_piv_aid[OSK_PIV_AID_LEN] = {
    0xa0, 0x00, 0x00, 0x03, 0x08, 0x00, 0x00, 0x10, 0x00, 0x01, 0x00,
};

/* Kommandos (Teil 2 Tabelle 1). */
#define INS_VERIFY 0x20u
#define INS_CHANGE_REFERENCE 0x24u
#define INS_RESET_RETRY 0x2cu
#define INS_SELECT 0xa4u
#define INS_GENERAL_AUTH 0x87u
#define INS_GET_DATA 0xcbu
#define INS_PUT_DATA 0xdbu
#define INS_GENERATE_ASYM 0x47u
#define INS_ATTEST OSK_PIV_INS_ATTEST

/* Schluesselkennungen fuer VERIFY und CHANGE REFERENCE DATA (Teil 2
 * Tabelle 3): 0x80 ist die PIV-PIN, 0x81 der PUK. */
#define REF_PIN 0x80u
#define REF_PUK 0x81u

/* Die PIN kommt immer in acht Bytes, mit 0xFF aufgefuellt (Teil 2 2.4.3). */
#define PIN_FIELD_LEN 8u
#define PIN_PAD 0xffu

/* Steckplatz plus Zertifikat: wir halten je Einschaltvorgang ein Objekt im
 * RAM. Sonst fragt jedes Auflisten eines Zertifikats am Display nach, und
 * ein Programm wie Firefox liest sie beim Start alle durch. */
#define CACHE_LEN 3072u

typedef struct {
	uint32_t tag;	      /* Tag des Datenobjekts, 3 Bytes */
	const char *vault;    /* Kennung im Tresor */
	const char *name;     /* Klartext fuer Display und Liste */
	osk_item_type_t type; /* als was es im Tresor liegt */
	uint8_t slot;	      /* 0 = kein Schluessel dazu */
} do_entry_t;

/* Die Datenobjekte, die wir fuehren. Tags aus Teil 1 Tabelle 3. */
static const do_entry_t g_objects[] = {
    {0x5fc107u, "PIV-CCC", "PIV Card Capability Container", OSK_ITEM_NOTE, 0u},
    {0x5fc102u, "PIV-CHUID", "PIV Ausweiskennung", OSK_ITEM_IDENTITY, 0u},
    {0x5fc105u, "PIV-CERT-9A", "PIV Zertifikat Anmeldung", OSK_ITEM_CERT, OSK_PIV_SLOT_AUTH},
    {0x5fc10au, "PIV-CERT-9C", "PIV Zertifikat Unterschrift", OSK_ITEM_CERT, OSK_PIV_SLOT_SIGN},
    {0x5fc10bu, "PIV-CERT-9D", "PIV Zertifikat Entschluesselung", OSK_ITEM_CERT,
     OSK_PIV_SLOT_KEYMGMT},
    {0x5fc101u, "PIV-CERT-9E", "PIV Zertifikat Karte", OSK_ITEM_CERT, OSK_PIV_SLOT_CARD},
    /* Das Zertifikat des Attestierungsschluessels. Es wird von aussen
     * hereingeschrieben (ein Fertigungsschritt oder der Besitzer selbst)
     * und von osk_attest.c nur gelesen - daraus kommt der Ausstellername. */
    {OSK_ATTEST_CERT_DO, OSK_ATTEST_CERT_TAG, OSK_ATTEST_CERT_NAME, OSK_ITEM_CERT,
     OSK_PIV_SLOT_ATTEST},
};

#define OBJECT_COUNT (sizeof(g_objects) / sizeof(g_objects[0]))

/* Zustand - alles fluechtig und beim Einschalten der Karte wieder auf Null. */
static bool g_pin_verified;
/* 9C verlangt die PIN unmittelbar vor der Unterschrift (Teil 1 Tabelle 4b,
 * "PIN Always"). Dieses Merker faellt bei jedem anderen Kommando. */
static bool g_pin_always;
static uint8_t g_cache[CACHE_LEN];
static size_t g_cache_len;
static uint32_t g_cache_tag;
/* Welches Verfahren in welchem Steckplatz liegt - sonst ist nach einem
 * Neustart nicht bekannt, wie lang die Unterschrift wird. 0 = kein
 * Schluessel. Fuenf Plaetze: vier aus Tabelle 4b und F9. */
static uint8_t g_slot_alg[5];

/* Grosser Puffer fuer RSA: eine Unterschrift oder ein Chiffrat ist so lang
 * wie der Modulus, also bis 512 Byte. Er liegt fest und nicht auf dem
 * Stapel - derselbe Grund wie in osk_attest.c (docs/plan.md, offener
 * Punkt 4a: der Stapel eines Zephyr-Fadens ist oft 1-2 kB gross, und am PC
 * faellt das nie auf). Es wird immer nur ein Kommando bearbeitet. */
static uint8_t g_big[OSK_RSA_MOD_MAX];

static unsigned slot_index(uint8_t slot)
{
	switch (slot) {
	case OSK_PIV_SLOT_AUTH:
		return 0u;
	case OSK_PIV_SLOT_SIGN:
		return 1u;
	case OSK_PIV_SLOT_KEYMGMT:
		return 2u;
	case OSK_PIV_SLOT_CARD:
		return 3u;
	case OSK_PIV_SLOT_ATTEST:
		return 4u;
	default:
		return 5u;
	}
}

/* Die vier Steckplaetze der Norm. F9 ist keiner davon: er unterschreibt
 * nur Attestierungen und nimmt an GENERAL AUTHENTICATE nicht teil. */
static bool slot_is_user(uint8_t slot)
{
	return slot_index(slot) <= 3u;
}

static const char *slot_name(uint8_t slot)
{
	switch (slot) {
	case OSK_PIV_SLOT_AUTH:
		return "PIV Anmeldung (9A)";
	case OSK_PIV_SLOT_SIGN:
		return "PIV Unterschrift (9C)";
	case OSK_PIV_SLOT_KEYMGMT:
		return "PIV Entschluesselung (9D)";
	case OSK_PIV_SLOT_CARD:
		return "PIV Karte (9E)";
	case OSK_PIV_SLOT_ATTEST:
		return "PIV Attestierung (F9)";
	default:
		return "PIV";
	}
}

static const do_entry_t *object_by_tag(uint32_t tag)
{
	for (size_t i = 0; i < OBJECT_COUNT; i++) {
		if (g_objects[i].tag == tag)
			return &g_objects[i];
	}
	return NULL;
}

static void cache_drop(void)
{
	osk_card_wipe(g_cache, sizeof(g_cache));
	g_cache_len = 0u;
	g_cache_tag = 0u;
}

static void piv_reset(void)
{
	g_pin_verified = false;
	g_pin_always = false;
	cache_drop();
	/* Die Schluessel bleiben im Chip; nur unser Wissen ueber die Kurve
	 * ist weg und wird beim ersten Zugriff wieder gelernt. */
	memset(g_slot_alg, 0, sizeof(g_slot_alg));
	osk_card_wipe(g_big, sizeof(g_big));
	/* Auch die Antwort "kann der Chip RSA" faellt: sie wird beim
	 * naechsten Einschalten neu beim Chip geholt und nicht ueber einen
	 * Kartenwechsel hinweg geglaubt. */
	osk_rsa_forget();
	osk_serial_forget();
	osk_attest_reset();
}

/* Laenge der Unterschrift und des Punktes je Kurve. Gilt nur fuer die
 * beiden Kurven; bei RSA haengt die Laenge am Modulus und nicht an der
 * Kennung. */
static size_t alg_half(uint8_t alg)
{
	return (alg == OSK_PIV_ALG_P384) ? 48u : 32u;
}

static bool alg_is_ec(uint8_t alg)
{
	return alg == OSK_PIV_ALG_P256 || alg == OSK_PIV_ALG_P384;
}

/* Ist diese Kennung hier ueberhaupt erlaubt? Bei RSA haengt es nicht nur
 * an der Kennung, sondern auch daran, ob der Chip RSA kann - zwei Fragen,
 * die beide ja sagen muessen (osk_rsa_card.h, Kopf). */
static bool alg_known(uint8_t alg)
{
	if (alg_is_ec(alg))
		return true;
	return alg == OSK_PIV_ALG_RSA2048 && osk_rsa_available();
}

static osk_curve_t alg_curve(uint8_t alg)
{
	return (alg == OSK_PIV_ALG_P384) ? OSK_CURVE_P384 : OSK_CURVE_P256;
}

/* --- SELECT ------------------------------------------------------------- */

/* Antwort nach Teil 2 Abschnitt 3.1.1: Application Property Template. */
static uint16_t cmd_select(osk_resp_t *res)
{
	osk_tlv_mark_t outer;
	osk_tlv_mark_t inner;
	static const char label[] = "OpenSecureKey PIV";
	/* Objektkennung der NIST-Algorithmenliste, Teil 1 Abschnitt 3.1.1. */
	static const uint8_t alg_oid[] = {0x2b, 0x06, 0x01, 0x04, 0x01,
					  0x82, 0x37, 0x15, 0x01, 0x01};
	static const uint8_t alg_p256[] = {OSK_PIV_ALG_P256};
	static const uint8_t alg_p384[] = {OSK_PIV_ALG_P384};
	static const uint8_t alg_rsa[] = {OSK_PIV_ALG_RSA2048};

	osk_resp_tlv_begin(res, 0x61u, &outer);
	osk_resp_tlv(res, 0x4fu, osk_piv_aid + 5u, OSK_PIV_AID_LEN - 5u);
	osk_resp_tlv_begin(res, 0x79u, &inner);
	osk_resp_tlv(res, 0x4fu, osk_piv_aid, 5u);
	osk_resp_tlv_end(res, &inner);
	osk_resp_tlv(res, 0x50u, (const uint8_t *)label, sizeof(label) - 1u);
	osk_resp_tlv_begin(res, 0xacu, &inner);
	osk_resp_tlv(res, 0x80u, alg_p256, sizeof(alg_p256));
	osk_resp_tlv(res, 0x80u, alg_p384, sizeof(alg_p384));
	/* RSA wird nur genannt, wenn der Chip es wirklich kann. Ein
	 * Kartenprogramm, das sich auf die Liste verlaesst und dann ein
	 * "Funktion nicht vorhanden" bekommt, bricht an einer Stelle ab, an
	 * der es nichts mehr sagen kann - und der SE051A2 antwortet auf
	 * derselben Adresse wie der C2 (osk_rsa_card.h, Kopf). */
	if (osk_rsa_available())
		osk_resp_tlv(res, 0x80u, alg_rsa, sizeof(alg_rsa));
	osk_resp_tlv(res, 0x06u, alg_oid, sizeof(alg_oid));
	osk_resp_tlv_end(res, &inner);
	osk_resp_tlv_end(res, &outer);
	return OSK_SW_OK;
}

/* --- VERIFY, CHANGE REFERENCE DATA, RESET RETRY COUNTER ----------------- */

/* Nimmt das achtstellige Feld und schneidet die Fuellbytes ab. */
static bool pin_from_field(const uint8_t *field, size_t len, const uint8_t **pin_out,
			   size_t *pin_len_out)
{
	size_t n;

	if (field == NULL || len != PIN_FIELD_LEN)
		return false;
	n = len;
	while (n > 0u && field[n - 1u] == PIN_PAD)
		n--;
	if (n == 0u)
		return false;
	/* Fuellbytes duerfen nur am Ende stehen. */
	for (size_t i = 0; i < n; i++) {
		if (field[i] == PIN_PAD)
			return false;
	}
	*pin_out = field;
	*pin_len_out = n;
	return true;
}

static uint16_t cmd_verify(const osk_apdu_t *a)
{
	const uint8_t *pin = NULL;
	size_t pin_len = 0;
	uint16_t sw;

	/* P1 = 0xFF setzt die Freigabe zurueck (Teil 2 3.2.1). */
	if (a->p1 == 0xffu) {
		if (a->lc != 0u)
			return OSK_SW_WRONG_LENGTH;
		g_pin_verified = false;
		return OSK_SW_OK;
	}
	if (a->p1 != 0x00u)
		return OSK_SW_WRONG_P1P2;
	if (a->p2 != REF_PIN)
		return OSK_SW_REF_NOT_FOUND;

	if (a->lc == 0u) {
		/* Nachfrage nach dem Zustand. */
		if (g_pin_verified)
			return OSK_SW_OK;
		return osk_card_pin_status();
	}
	if (!pin_from_field(a->data, a->lc, &pin, &pin_len))
		return OSK_SW_WRONG_LENGTH;

	sw = osk_card_pin_verify(pin, pin_len);
	g_pin_verified = (sw == OSK_SW_OK);
	g_pin_always = g_pin_verified;
	return sw;
}

static uint16_t cmd_change_reference(const osk_apdu_t *a)
{
	const uint8_t *old_s = NULL;
	const uint8_t *new_s = NULL;
	size_t old_len = 0;
	size_t new_len = 0;

	if (a->p1 != 0x00u)
		return OSK_SW_WRONG_P1P2;
	if (a->lc != PIN_FIELD_LEN * 2u)
		return OSK_SW_WRONG_LENGTH;
	if (!pin_from_field(a->data, PIN_FIELD_LEN, &old_s, &old_len) ||
	    !pin_from_field(a->data + PIN_FIELD_LEN, PIN_FIELD_LEN, &new_s, &new_len))
		return OSK_SW_WRONG_LENGTH;

	if (a->p2 == REF_PIN)
		return osk_card_pin_change(old_s, old_len, new_s, new_len);
	if (a->p2 == REF_PUK)
		return osk_card_admin_change(OSK_CARD_ADMIN_PIV_PUK, old_s, old_len, new_s,
					     new_len);
	return OSK_SW_REF_NOT_FOUND;
}

/* RESET RETRY COUNTER: PUK und neue PIN, je acht Bytes (Teil 2 3.2.3). */
static uint16_t cmd_reset_retry(const osk_apdu_t *a)
{
	const uint8_t *puk = NULL;
	const uint8_t *pin = NULL;
	size_t puk_len = 0;
	size_t pin_len = 0;
	uint16_t sw;

	if (a->p1 != 0x00u || a->p2 != REF_PIN)
		return OSK_SW_WRONG_P1P2;
	if (a->lc != PIN_FIELD_LEN * 2u)
		return OSK_SW_WRONG_LENGTH;
	if (!pin_from_field(a->data, PIN_FIELD_LEN, &puk, &puk_len) ||
	    !pin_from_field(a->data + PIN_FIELD_LEN, PIN_FIELD_LEN, &pin, &pin_len))
		return OSK_SW_WRONG_LENGTH;

	sw = osk_card_admin_verify(OSK_CARD_ADMIN_PIV_PUK, puk, puk_len);
	if (sw != OSK_SW_OK)
		return sw;

	/* Die PIN liegt im Chip, und der nimmt eine neue nur gegen die alte an.
	 * Das Zuruecksetzen mit dem PUK geht deshalb ueber das Loeschen - was
	 * nur nach der Freigabe am Display passiert, um die osk_se050_pin_clear
	 * bittet, wer sie aufruft (siehe osk_se050.h). */
	{
		osk_ask_req_t ask;

		memset(&ask, 0, sizeof(ask));
		ask.kind = OSK_ASK_PAIR;
		ask.subject = "PIV: PIN mit PUK zuruecksetzen";
		ask.asked_by = "PIV";
		ask.timeout_ms = 30000u;
		if (osk_ui_confirm(&ask) != OSK_OK)
			return OSK_SW_SECURITY;
	}
	if (osk_se050_pin_clear() != OSK_OK)
		return OSK_SW_SECURITY;
	if (osk_se050_pin_set(NULL, 0u, pin, pin_len) != OSK_OK)
		return OSK_SW_SECURITY;
	g_pin_verified = false;
	return OSK_SW_OK;
}

/* --- GET DATA und PUT DATA ---------------------------------------------- */

/* Das Discovery Object wird gerechnet, nicht gespeichert (Teil 1 3.2.6):
 * es sagt nur, welche Anwendung da ist und welche PIN sie benutzt. */
static uint16_t discovery_object(osk_resp_t *res)
{
	osk_tlv_mark_t outer;
	static const uint8_t policy[] = {0x40, 0x00}; /* nur die PIV-PIN */

	osk_resp_tlv_begin(res, 0x7eu, &outer);
	osk_resp_tlv(res, 0x4fu, osk_piv_aid, OSK_PIV_AID_LEN);
	osk_resp_tlv(res, 0x5f2fu, policy, sizeof(policy));
	osk_resp_tlv_end(res, &outer);
	return OSK_SW_OK;
}

static uint16_t cmd_get_data(const osk_apdu_t *a, osk_resp_t *res)
{
	const uint8_t *tag_val = NULL;
	size_t tag_len = 0;
	uint32_t tag = 0;
	const do_entry_t *obj;

	if (a->p1 != 0x3fu || a->p2 != 0xffu)
		return OSK_SW_WRONG_P1P2;
	if (a->lc == 0u || !osk_tlv_find(a->data, a->lc, 0x5cu, &tag_val, &tag_len))
		return OSK_SW_WRONG_DATA;
	if (tag_len == 0u || tag_len > 3u)
		return OSK_SW_WRONG_DATA;
	for (size_t i = 0; i < tag_len; i++)
		tag = (tag << 8) | (uint32_t)tag_val[i];

	if (tag == 0x7eu)
		return discovery_object(res);

	obj = object_by_tag(tag);
	if (obj == NULL)
		return OSK_SW_FILE_NOT_FOUND;

	if (g_cache_tag != tag) {
		uint8_t buf[CACHE_LEN];
		size_t len = 0;
		osk_result_t rc =
		    osk_card_obj_read(obj->type, obj->vault, obj->name, buf, sizeof(buf), &len);

		if (rc == OSK_ERR_NOT_FOUND)
			return OSK_SW_FILE_NOT_FOUND;
		if (rc == OSK_ERR_DENIED)
			return OSK_SW_SECURITY;
		if (rc != OSK_OK || len == 0u)
			return OSK_SW_UNKNOWN;
		cache_drop();
		memcpy(g_cache, buf, len);
		g_cache_len = len;
		g_cache_tag = tag;
		osk_card_wipe(buf, sizeof(buf));
	}

	/* Datenobjekte gehen immer in 0x53 verpackt hinaus (Teil 1 3.1.2). */
	osk_resp_tlv(res, 0x53u, g_cache, g_cache_len);
	return OSK_SW_OK;
}

static uint16_t cmd_put_data(const osk_apdu_t *a)
{
	const uint8_t *tag_val = NULL;
	const uint8_t *body = NULL;
	size_t tag_len = 0;
	size_t body_len = 0;
	uint32_t tag = 0;
	const do_entry_t *obj;
	osk_ask_req_t ask;

	if (a->p1 != 0x3fu || a->p2 != 0xffu)
		return OSK_SW_WRONG_P1P2;
	if (a->lc == 0u || !osk_tlv_find(a->data, a->lc, 0x5cu, &tag_val, &tag_len))
		return OSK_SW_WRONG_DATA;
	if (tag_len == 0u || tag_len > 3u)
		return OSK_SW_WRONG_DATA;
	for (size_t i = 0; i < tag_len; i++)
		tag = (tag << 8) | (uint32_t)tag_val[i];
	if (!osk_tlv_find(a->data, a->lc, 0x53u, &body, &body_len) || body_len == 0u)
		return OSK_SW_WRONG_DATA;
	if (body_len > CACHE_LEN)
		return OSK_SW_WRONG_LENGTH;

	obj = object_by_tag(tag);
	if (obj == NULL)
		return OSK_SW_FILE_NOT_FOUND;

	/* PIV sichert das Schreiben mit dem Verwaltungsschluessel 9B (3DES).
	 * Den gibt es hier nicht - stattdessen die PIN und ein Tastendruck,
	 * siehe docs/smartcard.md. */
	if (!g_pin_verified)
		return OSK_SW_SECURITY;
	memset(&ask, 0, sizeof(ask));
	ask.kind = OSK_ASK_PAIR;
	ask.subject = obj->name;
	ask.detail = "schreiben";
	ask.asked_by = "PIV";
	ask.timeout_ms = 30000u;
	if (osk_ui_confirm(&ask) != OSK_OK)
		return OSK_SW_SECURITY;

	if (osk_card_obj_write(obj->type, obj->vault, obj->name, body, body_len) != OSK_OK)
		return OSK_SW_UNKNOWN;
	if (g_cache_tag == tag)
		cache_drop();
	return OSK_SW_OK;
}

/* --- GENERATE ASYMMETRIC KEY PAIR --------------------------------------- */

static uint16_t cmd_generate(const osk_apdu_t *a, osk_resp_t *res)
{
	const uint8_t *tmpl = NULL;
	const uint8_t *alg_val = NULL;
	size_t tmpl_len = 0;
	size_t alg_len = 0;
	uint8_t alg;
	uint8_t slot = a->p2;
	osk_key_policy_t policy;
	osk_ask_req_t ask;
	uint8_t raw[160];
	size_t raw_len = 0;
	const uint8_t *point = NULL;
	size_t point_len;
	osk_tlv_mark_t mark;

	if (a->p1 != 0x00u)
		return OSK_SW_WRONG_P1P2;
	/* F9 darf hier mit: der Attestierungsschluessel muss auch entstehen
	 * koennen, und das ist derselbe Vorgang. */
	if (slot_index(slot) > 4u)
		return OSK_SW_REF_NOT_FOUND;
	if (a->lc == 0u || !osk_tlv_find(a->data, a->lc, 0xacu, &tmpl, &tmpl_len))
		return OSK_SW_WRONG_DATA;
	if (!osk_tlv_find(tmpl, tmpl_len, 0x80u, &alg_val, &alg_len) || alg_len != 1u)
		return OSK_SW_WRONG_DATA;
	alg = alg_val[0];
	if (!alg_known(alg))
		return OSK_SW_FUNC_NOT_SUPPORTED;
	/* F9 unterschreibt Attestierungen ueber SHA-256; dazu passt eine
	 * P-256 und sonst nichts, siehe osk_attest.c. */
	if (slot == OSK_PIV_SLOT_ATTEST && alg != OSK_PIV_ALG_P256)
		return OSK_SW_FUNC_NOT_SUPPORTED;

	if (!g_pin_verified)
		return OSK_SW_SECURITY;

	memset(&ask, 0, sizeof(ask));
	/* Das Anlegen eines Schluessels ist ein Verwaltungseingriff; die
	 * Liste in osk_ui.h hat dafuer keine eigene Art, OSK_ASK_PAIR ist
	 * die, die "das Geraet wird eingerichtet" bedeutet. */
	ask.kind = OSK_ASK_PAIR;
	ask.subject = slot_name(slot);
	ask.detail = "neues Schluesselpaar";
	ask.asked_by = "PIV";
	ask.timeout_ms = 30000u;
	if (osk_ui_confirm(&ask) != OSK_OK)
		return OSK_SW_SECURITY;

	policy.exportable = false;
	/* 9E braucht keinen Tastendruck (es gibt auch keine PIN dafuer), und
	 * F9 keinen, weil die Attestierung nichts Geheimes herausgibt und nur
	 * selbst zusammengesetzte Daten unterschreibt - die Begruendung steht
	 * ausfuehrlich im Kopf von osk_attest.h. */
	policy.needs_touch = (slot != OSK_PIV_SLOT_CARD && slot != OSK_PIV_SLOT_ATTEST);

	/* Ein vorhandener Schluessel wird ersetzt - PIV kennt kein Anhaengen.
	 * Beide Arten loeschen: im Steckplatz kann vorher das andere
	 * Verfahren gelegen haben. */
	(void)osk_se050_key_delete(OSK_PIV_KEY_ID(slot));

	if (!alg_is_ec(alg)) {
		osk_rsa_bits_t bits;
		uint16_t nbits = 2048u;

		if (!osk_rsa_bits_enum(nbits, &bits) || !osk_rsa_bits_ok(nbits))
			return OSK_SW_FUNC_NOT_SUPPORTED;
		if (osk_se050_rsa_key_create(OSK_PIV_KEY_ID(slot), bits, &policy) != OSK_OK)
			return OSK_SW_UNKNOWN;
		g_slot_alg[slot_index(slot)] = alg;
		/* Antwort nach Teil 2 3.3.1, Tabelle 9: bei RSA 7F49 mit
		 * 0x81 (Modulus) und 0x82 (Exponent). */
		return osk_rsa_put_public(OSK_PIV_KEY_ID(slot), res);
	}

	if (osk_se050_key_create(OSK_PIV_KEY_ID(slot), alg_curve(alg), &policy) != OSK_OK)
		return OSK_SW_UNKNOWN;
	if (osk_se050_key_public(OSK_PIV_KEY_ID(slot), raw, sizeof(raw), &raw_len) != OSK_OK)
		return OSK_SW_UNKNOWN;

	point_len = alg_half(alg) * 2u + 1u;
	if (osk_card_public_point(raw, raw_len, point_len, &point) != OSK_OK)
		return OSK_SW_UNKNOWN;

	g_slot_alg[slot_index(slot)] = alg;

	/* Antwort nach Teil 2 3.3.1: 7F49 mit 0x86, dem Punkt. */
	osk_resp_tlv_begin(res, 0x7f49u, &mark);
	osk_resp_tlv(res, 0x86u, point, point_len);
	osk_resp_tlv_end(res, &mark);
	return OSK_SW_OK;
}

/* --- GENERAL AUTHENTICATE ----------------------------------------------- */

/* Welche Kurve im Steckplatz liegt. Nach einem Neustart wissen wir es nicht
 * mehr und fragen den Chip, indem wir den oeffentlichen Schluessel holen. */
static uint8_t slot_alg(uint8_t slot)
{
	uint8_t raw[160];
	size_t raw_len = 0;
	const unsigned idx = slot_index(slot);

	if (idx > 4u)
		return 0u;
	if (g_slot_alg[idx] != 0u)
		return g_slot_alg[idx];
	if (osk_se050_key_public(OSK_PIV_KEY_ID(slot), raw, sizeof(raw), &raw_len) == OSK_OK) {
		if (raw_len >= 97u && raw[raw_len - 97u] == 0x04u)
			g_slot_alg[idx] = OSK_PIV_ALG_P384;
		else if (raw_len >= 65u && raw[raw_len - 65u] == 0x04u)
			g_slot_alg[idx] = OSK_PIV_ALG_P256;
		return g_slot_alg[idx];
	}
	/* Kein Kurvenschluessel. Vielleicht ein RSA-Schluessel - gefragt
	 * wird der Chip, geraten wird nichts. osk_rsa_modulus_bits() liefert
	 * nur eine der drei bekannten Laengen oder 0. */
	if (osk_rsa_available() && osk_rsa_modulus_bits(OSK_PIV_KEY_ID(slot)) == 2048u)
		g_slot_alg[idx] = OSK_PIV_ALG_RSA2048;
	return g_slot_alg[idx];
}

/* Kurzer Auszug des zu unterschreibenden Wertes fuer das Display. Der Nutzer
 * kann einen Hash nicht pruefen, aber er sieht, dass es derselbe ist, den
 * ihm sein Programm nennt. */
static void hash_preview(const uint8_t *hash, size_t len, char *out, size_t cap)
{
	const size_t show = (len < 4u) ? len : 4u;
	size_t at = 0;

	if (cap == 0u)
		return;
	out[0] = 0;
	for (size_t i = 0; i < show; i++) {
		if (at + 3u >= cap)
			break;
		at += (size_t)snprintf(out + at, cap - at, "%02x", hash[i]);
	}
}

/* 9C verlangt die PIN unmittelbar davor, 9A und 9D die PIN, 9E gar keine
 * (Teil 1 Tabelle 4b). Ein Tastendruck ist bei allen vier noetig - das ist
 * unsere Zugabe, nicht die der Spezifikation.
 *
 * Steht an EINER Stelle, weil Kurve und RSA dieselbe Regel haben: waere
 * sie zweimal geschrieben, wuerde eine der beiden irgendwann nachgeben.
 * Die Texte in slot_policy() (osk_attest.c) sagen dasselbe noch einmal in
 * Worten - test_attest.c prueft beides gegeneinander. */
static uint16_t check_release(uint8_t slot)
{
	if (slot == OSK_PIV_SLOT_SIGN) {
		if (!g_pin_verified || !g_pin_always)
			return OSK_SW_SECURITY;
	} else if (slot != OSK_PIV_SLOT_CARD) {
		if (!g_pin_verified)
			return OSK_SW_SECURITY;
	}
	return OSK_SW_OK;
}

static uint16_t do_sign(uint8_t slot, uint8_t alg, const uint8_t *hash, size_t hash_len,
			osk_resp_t *res)
{
	uint8_t sig[160];
	size_t sig_len = 0;
	osk_ask_req_t ask;
	char preview[16];
	osk_tlv_mark_t mark;
	const size_t half = alg_half(alg);
	uint16_t sw;

	if (hash_len == 0u || hash_len > half)
		return OSK_SW_WRONG_DATA;

	sw = check_release(slot);
	if (sw != OSK_SW_OK)
		return sw;

	hash_preview(hash, hash_len, preview, sizeof(preview));
	memset(&ask, 0, sizeof(ask));
	ask.kind = OSK_ASK_SIGN;
	ask.subject = slot_name(slot);
	ask.detail = preview;
	ask.asked_by = "PIV";
	ask.timeout_ms = 30000u;
	if (osk_ui_confirm(&ask) != OSK_OK)
		return OSK_SW_SECURITY;

	if (osk_se050_sign(OSK_PIV_KEY_ID(slot), hash, hash_len, sig, sizeof(sig), &sig_len) !=
	    OSK_OK)
		return OSK_SW_UNKNOWN;

	/* PIV gibt die Unterschrift in der DER-Form weiter, wie der Chip sie
	 * liefert (Teil 2 3.2.4, Tabelle 7). */
	osk_resp_tlv_begin(res, 0x7cu, &mark);
	osk_resp_tlv(res, 0x82u, sig, sig_len);
	osk_resp_tlv_end(res, &mark);
	osk_card_wipe(sig, sizeof(sig));
	return OSK_SW_OK;
}

static uint16_t do_keyagree(uint8_t slot, uint8_t alg, const uint8_t *peer, size_t peer_len,
			    osk_resp_t *res)
{
	uint8_t secret[64];
	size_t secret_len = 0;
	osk_ask_req_t ask;
	osk_tlv_mark_t mark;
	const size_t point_len = alg_half(alg) * 2u + 1u;

	if (slot != OSK_PIV_SLOT_KEYMGMT)
		return OSK_SW_FUNC_NOT_SUPPORTED;
	if (peer_len != point_len || peer[0] != 0x04u)
		return OSK_SW_WRONG_DATA;
	if (!g_pin_verified)
		return OSK_SW_SECURITY;

	memset(&ask, 0, sizeof(ask));
	ask.kind = OSK_ASK_DECRYPT;
	ask.subject = slot_name(slot);
	ask.asked_by = "PIV";
	ask.timeout_ms = 30000u;
	if (osk_ui_confirm(&ask) != OSK_OK)
		return OSK_SW_SECURITY;

	if (osk_se050_ecdh(OSK_PIV_KEY_ID(slot), peer, peer_len, secret, sizeof(secret),
			   &secret_len) != OSK_OK)
		return OSK_SW_UNKNOWN;

	osk_resp_tlv_begin(res, 0x7cu, &mark);
	osk_resp_tlv(res, 0x82u, secret, secret_len);
	osk_resp_tlv_end(res, &mark);
	osk_card_wipe(secret, sizeof(secret));
	return OSK_SW_OK;
}


/* --- RSA: unterschreiben und entschluesseln ----------------------------- */

/* UNTERSCHREIBEN. Hier liegt die wichtigste Entscheidung dieses Teils, und
 * sie verengt absichtlich, was PIV erlaubt.
 *
 * PIV laesst das Auffuellen beim Aufrufer: er baut den fertigen
 * PKCS#1-v1.5-Block und erwartet, dass die Karte darauf die private
 * Schluesseloperation ROH rechnet (SP 800-73-4 Teil 2 Abschnitt 3.2.4).
 * Das waere ueber osk_se050_rsa_decrypt() mit OSK_RSA_PAD_NONE machbar -
 * rohes Entschluesseln und rohes Unterschreiben sind dieselbe Rechnung.
 *
 * GENAU DAS WIRD NICHT GEMACHT, und der Grund ist 9E: dieser Steckplatz
 * braucht keine PIN. Eine rohe private Schluesseloperation auf einem Wert,
 * den der Rechner frei waehlt, ist ein Unterschriftenautomat fuer alles -
 * wer ihn erreicht, bekommt zu jedem Wert die Unterschrift und kann damit
 * jede Bescheinigung bauen, nicht nur die, die er vorgelegt hat.
 *
 * Darum wird der Block AUSGEPACKT: Auffuellung geprueft, DigestInfo
 * erkannt, Hash und Hashverfahren herausgeholt, und dann wird ueber den
 * benannten Weg des Vertrags unterschrieben (OSK_RSA_PAD_PKCS1_V15). Der
 * Chip baut die Auffuellung neu. Das Ergebnis ist bytegleich, solange der
 * Block ein ordentlicher v1.5-Block mit einem der drei SHA-2-Verfahren
 * ist; alles andere wird abgelehnt statt gerechnet.
 *
 * WAS DAS KOSTET, und das gehoert in den Bericht: ein Kartenprogramm, das
 * mit SHA-1, mit PSS oder mit einem selbst zusammengesetzten Block
 * unterschreiben will, bekommt 6A80. Mit SHA-256, SHA-384 und SHA-512
 * geht es - und das ist, was heutige Programme schicken. */
static uint16_t do_rsa_sign(uint8_t slot, const uint8_t *block, size_t block_len, osk_resp_t *res)
{
	osk_hash_t hash_alg;
	const uint8_t *hash = NULL;
	size_t hash_len = 0;
	size_t sig_len = 0;
	size_t mod_len;
	osk_ask_req_t ask;
	char preview[16];
	osk_tlv_mark_t mark;
	uint16_t sw;

	mod_len = osk_rsa_mod_bytes(osk_rsa_modulus_bits(OSK_PIV_KEY_ID(slot)));
	if (mod_len == 0u)
		return OSK_SW_REF_NOT_FOUND;
	if (!osk_rsa_pkcs1_unwrap(block, block_len, mod_len, &hash_alg, &hash, &hash_len))
		return OSK_SW_WRONG_DATA;

	sw = check_release(slot);
	if (sw != OSK_SW_OK)
		return sw;

	hash_preview(hash, hash_len, preview, sizeof(preview));
	memset(&ask, 0, sizeof(ask));
	ask.kind = OSK_ASK_SIGN;
	ask.subject = slot_name(slot);
	ask.detail = preview;
	ask.asked_by = "PIV";
	ask.timeout_ms = 30000u;
	if (osk_ui_confirm(&ask) != OSK_OK)
		return OSK_SW_SECURITY;

	if (osk_se050_rsa_sign(OSK_PIV_KEY_ID(slot), OSK_RSA_PAD_PKCS1_V15, hash_alg, hash,
			       hash_len, g_big, sizeof(g_big), &sig_len) != OSK_OK)
		return OSK_SW_UNKNOWN;

	osk_resp_tlv_begin(res, 0x7cu, &mark);
	osk_resp_tlv(res, 0x82u, g_big, sig_len);
	osk_resp_tlv_end(res, &mark);
	osk_card_wipe(g_big, sizeof(g_big));
	return OSK_SW_OK;
}

/* ENTSCHLUESSELN - der Teil, ohne den verschluesselte Post ungelesen
 * bleibt. Nur Steckplatz 9D, und nur PKCS#1 v1.5: OAEP kann der Chip in
 * dieser Fassung nicht, und zwar aus einem belegten Grund (AN12413
 * Tabelle 32 kennt OAEP nur mit SHA-1, und osk_hash_t hat kein SHA-1 -
 * siehe docs/se050.md, "Finding: OAEP cannot be honoured"). PIV verlangt
 * v1.5, also kostet das hier nichts. */
static uint16_t do_rsa_decrypt(uint8_t slot, const uint8_t *in, size_t in_len, osk_resp_t *res)
{
	size_t out_len = 0;
	size_t mod_len;
	osk_ask_req_t ask;
	osk_tlv_mark_t mark;

	if (slot != OSK_PIV_SLOT_KEYMGMT)
		return OSK_SW_FUNC_NOT_SUPPORTED;
	mod_len = osk_rsa_mod_bytes(osk_rsa_modulus_bits(OSK_PIV_KEY_ID(slot)));
	if (mod_len == 0u)
		return OSK_SW_REF_NOT_FOUND;
	/* Ein Chiffrat ist so lang wie der Modulus. Kuerzeres ist keines. */
	if (in_len != mod_len)
		return OSK_SW_WRONG_DATA;
	if (!g_pin_verified)
		return OSK_SW_SECURITY;

	memset(&ask, 0, sizeof(ask));
	ask.kind = OSK_ASK_DECRYPT;
	ask.subject = slot_name(slot);
	ask.asked_by = "PIV";
	ask.timeout_ms = 30000u;
	if (osk_ui_confirm(&ask) != OSK_OK)
		return OSK_SW_SECURITY;

	/* `hash_alg` sieht der Vertrag bei v1.5 nicht an; genannt wird es
	 * trotzdem, weil die Auffuellung nie die Vorgabe ist. */
	if (osk_se050_rsa_decrypt(OSK_PIV_KEY_ID(slot), OSK_RSA_PAD_PKCS1_V15, OSK_HASH_SHA256, in,
				  in_len, g_big, sizeof(g_big), &out_len) != OSK_OK)
		return OSK_SW_UNKNOWN;

	osk_resp_tlv_begin(res, 0x7cu, &mark);
	osk_resp_tlv(res, 0x82u, g_big, out_len);
	osk_resp_tlv_end(res, &mark);
	osk_card_wipe(g_big, sizeof(g_big));
	return OSK_SW_OK;
}

static uint16_t cmd_general_auth(const osk_apdu_t *a, osk_resp_t *res)
{
	const uint8_t *tmpl = NULL;
	const uint8_t *val = NULL;
	size_t tmpl_len = 0;
	size_t val_len = 0;
	const uint8_t slot = a->p2;
	const uint8_t alg = a->p1;

	if (!slot_is_user(slot))
		return OSK_SW_REF_NOT_FOUND;
	if (!alg_known(alg))
		return OSK_SW_FUNC_NOT_SUPPORTED;
	if (slot_alg(slot) == 0u)
		return OSK_SW_REF_NOT_FOUND;
	if (slot_alg(slot) != alg)
		return OSK_SW_WRONG_P1P2;
	if (a->lc == 0u || !osk_tlv_find(a->data, a->lc, 0x7cu, &tmpl, &tmpl_len))
		return OSK_SW_WRONG_DATA;

	/* 0x82 leer heisst "antworte", 0x81 ist der Wert, der unterschrieben
	 * werden soll, 0x85 der fremde Punkt fuer den Schluesselaustausch
	 * (Teil 2 Tabelle 7).
	 *
	 * ACHTUNG, und das ist genau das Muster, das dieses Projekt siebenmal
	 * erwischt hat: 0x81 BEDEUTET BEI RSA ZWEIERLEI. In 9A, 9C und 9E ist
	 * es ein aufgefuellter Unterschriftenblock, in 9D ein Chiffrat. Zwei
	 * Groessen, gleich benannt, Verschiedenes gemessen - der Steckplatz
	 * entscheidet, welche gemeint ist. Bei den Kurven stellt sich die
	 * Frage nicht, weil der Schluesselaustausch dort ein eigenes Tag hat. */
	if (osk_tlv_find(tmpl, tmpl_len, 0x85u, &val, &val_len)) {
		if (!alg_is_ec(alg))
			return OSK_SW_WRONG_DATA;
		return do_keyagree(slot, alg, val, val_len, res);
	}
	if (osk_tlv_find(tmpl, tmpl_len, 0x81u, &val, &val_len)) {
		if (alg_is_ec(alg))
			return do_sign(slot, alg, val, val_len, res);
		if (slot == OSK_PIV_SLOT_KEYMGMT)
			return do_rsa_decrypt(slot, val, val_len, res);
		return do_rsa_sign(slot, val, val_len, res);
	}
	return OSK_SW_WRONG_DATA;
}

/* --- Attestierung ------------------------------------------------------- */

/* Siehe osk_attest.h: keine PIN, kein Tastendruck, und der Grund dafuer
 * steht dort. Heraus kommt das Zertifikat als DER, ohne Verpackung - fuer
 * ein eigenes Kommando ist eine Huelle nach PIV-Art nur Ballast. */
static uint16_t cmd_attest(const osk_apdu_t *a, osk_resp_t *res)
{
	static uint8_t cert[OSK_ATTEST_MAX];
	size_t len = 0;
	const uint8_t slot = a->p1;
	uint8_t alg;

	if (a->p2 != 0x00u)
		return OSK_SW_WRONG_P1P2;
	if (a->lc != 0u)
		return OSK_SW_WRONG_LENGTH;
	if (!slot_is_user(slot))
		return OSK_SW_REF_NOT_FOUND;
	alg = slot_alg(slot);
	if (alg == 0u)
		return OSK_SW_REF_NOT_FOUND;

	switch (osk_attest_cert(slot, alg, cert, sizeof(cert), &len)) {
	case OSK_OK:
		break;
	case OSK_ERR_NOT_FOUND:
		/* Entweder der Steckplatz oder F9 ist leer. Beides ist
		 * dasselbe Statuswort, weil die Karte dem Rechner nicht
		 * verraten muss, welcher von beiden. */
		return OSK_SW_REF_NOT_FOUND;
	case OSK_ERR_UNSUPPORTED:
		return OSK_SW_FUNC_NOT_SUPPORTED;
	case OSK_ERR_ARG:
		return OSK_SW_WRONG_P1P2;
	default:
		return OSK_SW_UNKNOWN;
	}

	osk_resp_bytes(res, cert, len);
	return OSK_SW_OK;
}

/* --- Verteiler innerhalb von PIV ---------------------------------------- */

static uint16_t piv_handle(const osk_apdu_t *a, osk_resp_t *res)
{
	const bool was_verify = (a->ins == INS_VERIFY);
	uint16_t sw;

	if ((a->cla & 0xe0u) != 0x00u)
		return OSK_SW_CLA_NOT_SUPPORTED;

	switch (a->ins) {
	case INS_SELECT:
		sw = cmd_select(res);
		break;
	case INS_VERIFY:
		sw = cmd_verify(a);
		break;
	case INS_CHANGE_REFERENCE:
		sw = cmd_change_reference(a);
		break;
	case INS_RESET_RETRY:
		sw = cmd_reset_retry(a);
		break;
	case INS_GET_DATA:
		sw = cmd_get_data(a, res);
		break;
	case INS_PUT_DATA:
		sw = cmd_put_data(a);
		break;
	case INS_GENERATE_ASYM:
		sw = cmd_generate(a, res);
		break;
	case INS_GENERAL_AUTH:
		sw = cmd_general_auth(a, res);
		break;
	case INS_ATTEST:
		sw = cmd_attest(a, res);
		break;
	default:
		sw = OSK_SW_INS_NOT_SUPPORTED;
		break;
	}

	/* "PIN Always" gilt nur fuer das Kommando direkt nach dem VERIFY. */
	if (!was_verify)
		g_pin_always = false;
	return sw;
}

static const osk_applet_t g_piv = {
    osk_piv_aid, OSK_PIV_AID_LEN, "PIV", piv_handle, piv_reset,
};

const osk_applet_t *osk_piv_applet(void)
{
	return &g_piv;
}
