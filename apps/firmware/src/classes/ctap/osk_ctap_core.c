/* Der CTAP2-Kern: Verteiler, MakeCredential, GetAssertion, GetNextAssertion,
 * GetInfo, Reset und Selection. ClientPIN liegt in osk_pin.c, U2F in
 * osk_ctap_u2f.c, die Erweiterungen in osk_ctap_ext.c.
 *
 * Spezifikation: CTAP 2.2 (14.07.2025), Abschnitte 6.1 bis 6.9.
 * Der Kern kennt keinen Transport: er bekommt [Befehlsbyte][CBOR] und
 * antwortet mit [Antwortwert][CBOR].
 *
 * Es wird kein Schluessel je Dienst gespeichert - jeder entsteht aus dem
 * Grundgeheimnis im SE050, siehe osk_ctap_key.h und osk_credid.h. */
#include <string.h>
#include <stdbool.h>
#include "osk_ctap.h"
#include "osk_ctap_internal.h"
#include "osk_ctap_ext.h"
#include "osk_ctap_key.h"
#include "osk_credid.h"
#include "osk_crypto.h"
#include "osk_pin.h"
#include "osk_se050.h"
#include "osk_sigcount.h"
#include "osk_ctap_cm.h"
#include "osk_ctap_config.h"
#include "osk_store.h"
#include "osk_transport.h"
#include "osk_ui.h"

/* Kennung dieses Geraetemodells - eine JE FIRMWARE-VARIANTE.
 *
 * FIDO verlangt, dass ein AAGUID eine Fassung mit DENSELBEN Faehigkeiten
 * bezeichnet. Dieses Geraet wird in zwei Fassungen uebersetzt, mit und ohne
 * RSA (Kconfig-Option, siehe docs/plan.md, Abschnitt "Querschnitt"). Die
 * Fassung mit RSA kann nachweisbar mehr; derselbe AAGUID fuer beide waere
 * also eine falsche Zusage an jeden Dienst, der danach entscheidet.
 *
 * Darum zwei Werte, und die VARIANTE entscheidet, nicht der Aufrufer: es
 * gibt keine Funktion, die den AAGUID setzt. Wer die Firmware ohne RSA
 * aufspielt, bekommt zwangslaeufig den zweiten.
 *
 * Beide Werte sind VORLAEUFIG und werden mit der ersten Serie festgelegt
 * (docs/plan.md, offener Punkt 10: AAGUID eintragen). Sie unterscheiden sich
 * im letzten Byte, damit beim Nachsehen im Protokoll sofort zu erkennen ist,
 * welche Fassung geantwortet hat. */
/* Nur der Wert der gebauten Fassung wird definiert. Beide unbedingt zu
 * definieren und erst in osk_ctap_aaguid() zu waehlen sieht sauberer aus,
 * laesst aber in JEDER Richtung die andere Konstante unbenutzt - und
 * -Werror=unused-const-variable macht daraus einen Fehler. So gebaut hat
 * dieser Satz am 30.10.2026 in keiner Richtung uebersetzt. */
#if defined(CONFIG_OSK_RSA)
static const uint8_t OSK_AAGUID_RSA[16] = {
    0x6f, 0x73, 0x6b, 0x2d, 0x6f, 0x70, 0x65, 0x6e, 0x73, 0x65, 0x63, 0x6b, 0x65, 0x79, 0x00, 0x01,
};
#else
static const uint8_t OSK_AAGUID_NO_RSA[16] = {
    0x6f, 0x73, 0x6b, 0x2d, 0x6f, 0x70, 0x65, 0x6e, 0x73, 0x65, 0x63, 0x6b, 0x65, 0x79, 0x00, 0x02,
};
#endif

const uint8_t *osk_ctap_aaguid(void)
{
#if defined(CONFIG_OSK_RSA)
	return OSK_AAGUID_RSA;
#else
	return OSK_AAGUID_NO_RSA;
#endif
}

static osk_ctap_state_t g_state;

osk_ctap_state_t *osk_ctap_state(void)
{
	return &g_state;
}

/* Zeitquelle. Die Firmware ersetzt das durch die Uhr des nRF52840; in den
 * Tests zaehlt eine Attrappe. */
#if !defined(OSK_CTAP_TEST_CLOCK)
uint64_t osk_ctap_now_ms(void)
{
	static uint64_t tick;
	return ++tick;
}
#endif

/* ---- Bausteine --------------------------------------------------------- */

osk_result_t osk_ctap_build_auth_data(const uint8_t rp_id_hash[32], uint8_t flags,
				      uint32_t sign_count, const uint8_t *attested,
				      size_t attested_len, const uint8_t *ext_cbor, size_t ext_len,
				      uint8_t *out, size_t cap, size_t *len_out)
{
	size_t need = 37 + attested_len + ext_len;

	if (!out || cap < need)
		return OSK_ERR_FULL;

	memcpy(out, rp_id_hash, 32);
	out[32] = flags;
	out[33] = (uint8_t)(sign_count >> 24);
	out[34] = (uint8_t)(sign_count >> 16);
	out[35] = (uint8_t)(sign_count >> 8);
	out[36] = (uint8_t)sign_count;
	if (attested_len)
		memcpy(out + 37, attested, attested_len);
	if (ext_len)
		memcpy(out + 37 + attested_len, ext_cbor, ext_len);
	*len_out = need;
	return OSK_OK;
}

void osk_ctap_put_cose_p256(osk_cbor_writer_t *w, const uint8_t pub[64])
{
	/* Schluessel aufsteigend: 1, 3, -1, -2, -3 - Haupttyp 0 vor 1, dann
	 * nach Wert. Genau so verlangt es die kanonische Form. */
	osk_cbor_put_map(w, 5);
	osk_cbor_put_uint(w, 1);
	osk_cbor_put_uint(w, 2); /* kty: EC2 */
	osk_cbor_put_uint(w, 3);
	osk_cbor_put_int(w, OSK_COSE_ALG_ES256); /* alg */
	osk_cbor_put_int(w, -1);
	osk_cbor_put_uint(w, 1); /* crv: P-256 */
	osk_cbor_put_int(w, -2);
	osk_cbor_put_bytes(w, pub, 32);
	osk_cbor_put_int(w, -3);
	osk_cbor_put_bytes(w, pub + 32, 32);
}

/* Fragt den Nutzer. subject ist immer der Dienst, damit auf dem Display
 * steht, wofuer freigegeben wird. */
static uint8_t ask_user(osk_ask_t kind, const char *rp_id, const char *detail)
{
	osk_ask_req_t ask;

	memset(&ask, 0, sizeof(ask));
	ask.kind = kind;
	ask.subject = rp_id;
	ask.detail = detail;
	ask.timeout_ms = OSK_UP_TIMEOUT_MS;

	const osk_result_t rc = osk_ui_confirm(&ask);
	if (rc == OSK_OK)
		return CTAP2_OK;
	if (rc == OSK_ERR_TIMEOUT)
		return CTAP2_ERR_USER_ACTION_TIMEOUT;
	return CTAP2_ERR_OPERATION_DENIED;
}

/* Kopiert einen CBOR-Text in einen Puffer mit Nullabschluss. */
static bool copy_text(const char *src, size_t len, char *dst, size_t cap)
{
	if (len >= cap)
		return false;
	memcpy(dst, src, len);
	dst[len] = 0;
	return true;
}

/* ---- Residente Nachweise im Tresor ------------------------------------- */

size_t osk_ctap_rk_pack(const uint8_t *cred_id, size_t cred_id_len, const uint8_t *user_id,
			size_t user_id_len, const char *user_name, uint8_t *out, size_t cap)
{
	const size_t name_len = user_name ? strlen(user_name) : 0;
	const size_t need = 1 + 1 + user_id_len + 1 + name_len + 1 + cred_id_len;
	size_t at = 0;

	if (need > cap || user_id_len > 255 || name_len > 255 || cred_id_len > 255)
		return 0;

	out[at++] = OSK_RK_RECORD_VERSION;
	out[at++] = (uint8_t)user_id_len;
	memcpy(out + at, user_id, user_id_len);
	at += user_id_len;
	out[at++] = (uint8_t)name_len;
	if (name_len)
		memcpy(out + at, user_name, name_len);
	at += name_len;
	out[at++] = (uint8_t)cred_id_len;
	memcpy(out + at, cred_id, cred_id_len);
	at += cred_id_len;
	return at;
}

bool osk_ctap_rk_unpack(const uint8_t *rec, size_t len, osk_cred_t *out)
{
	size_t at = 0;

	if (len < 4 || rec[0] != OSK_RK_RECORD_VERSION)
		return false;
	at = 1;

	const size_t uid_len = rec[at++];
	if (uid_len > OSK_MAX_USER_ID || at + uid_len + 1 > len)
		return false;
	memcpy(out->user_id, rec + at, uid_len);
	out->user_id_len = uid_len;
	at += uid_len;

	const size_t name_len = rec[at++];
	if (at + name_len + 1 > len || name_len >= sizeof(out->user_name))
		return false;
	memcpy(out->user_name, rec + at, name_len);
	out->user_name[name_len] = 0;
	at += name_len;

	const size_t cid_len = rec[at++];
	if (cid_len > OSK_MAX_CRED_ID || at + cid_len != len)
		return false;
	memcpy(out->cred_id, rec + at, cid_len);
	out->cred_id_len = cid_len;
	out->resident = true;
	return true;
}

/* Sucht den Tresoreintrag eines residenten Nachweises ueber die nicht
 * geheime Kennung: tag = rpIdHash. Damit wird nur noch geoeffnet, was
 * wirklich zu diesem Dienst gehoert - kein fremder Eintrag und keine
 * Freigabe fuer Eintraege, die gar nicht gemeint sind.
 * Entweder nur ueber den Dienst (fuer die Suche) oder zusaetzlich ueber die
 * Nutzerkennung (zum Ueberschreiben). start zeigt beim naechsten Aufruf auf
 * den naechsten Treffer. */
static osk_result_t rk_find(const uint8_t rp_id_hash[32], const uint8_t *user_id,
			    size_t user_id_len, osk_item_id_t *id_out, osk_cred_t *cred_out,
			    size_t *start)
{
	osk_item_id_t ids[OSK_MAX_CANDIDATES];
	size_t count = 0;

	const osk_result_t rc =
	    osk_store_find_tag(0, OSK_ITEM_FILE, rp_id_hash, 32, ids, OSK_MAX_CANDIDATES, &count);
	if (rc != OSK_OK) {
		if (start)
			*start = 0;
		return OSK_ERR_NOT_FOUND;
	}

	for (size_t i = (start ? *start : 0); i < count; i++) {
		uint8_t rec[OSK_RK_RECORD_MAX];
		size_t rec_len = 0;
		osk_cred_t cred;

		if (osk_store_reveal(ids[i], "FIDO2", rec, sizeof(rec), &rec_len) != OSK_OK)
			continue;

		memset(&cred, 0, sizeof(cred));
		const bool ok = osk_ctap_rk_unpack(rec, rec_len, &cred);
		osk_wipe(rec, sizeof(rec));
		if (!ok)
			continue;
		if (user_id && (cred.user_id_len != user_id_len ||
				memcmp(cred.user_id, user_id, user_id_len) != 0))
			continue;

		if (id_out)
			*id_out = ids[i];
		if (cred_out)
			*cred_out = cred;
		if (start)
			*start = i + 1;
		return OSK_OK;
	}
	if (start)
		*start = count;
	return OSK_ERR_NOT_FOUND;
}

/* Hat dieser auffindbare Nachweis noch seinen Eintrag im Tresor?
 *
 * Das ist die Frage, die "geloescht" von "geloescht, aber weiter benutzbar"
 * trennt: der Schluessel wird ABGELEITET, die Credential-ID allein genuegt
 * zum Signieren. Ohne diese Pruefung nimmt deleteCredential (0x0A/0x06) nur
 * den Eintrag weg und die Anmeldung laeuft weiter - ein Loeschen, das nichts
 * loescht. Gefunden am 29.10.2026, weil eine Pruefung nach dem Loeschen
 * noch anmelden konnte.
 *
 * Fuer NICHT auffindbare Nachweise gilt das nicht und kann es nicht: sie
 * haben keinen Eintrag, ihre Zahl ist unbegrenzt. Sie sind erst mit dem
 * Grundgeheimnis weg (authenticatorReset).
 *
 * cred_out darf NULL sein; sonst stehen darin die Nutzerangaben. */
static bool rk_has_cred(const uint8_t rp_id_hash[32], const uint8_t *cred_id, size_t cred_id_len,
			osk_cred_t *cred_out)
{
	osk_cred_t rk;
	size_t at = 0;

	memset(&rk, 0, sizeof(rk));
	while (rk_find(rp_id_hash, NULL, 0, NULL, &rk, &at) == OSK_OK) {
		if (rk.cred_id_len == cred_id_len &&
		    memcmp(rk.cred_id, cred_id, cred_id_len) == 0) {
			if (cred_out)
				*cred_out = rk;
			return true;
		}
	}
	return false;
}

/* ---- Anfrage von MakeCredential ---------------------------------------- */

typedef struct {
	const uint8_t *client_data_hash;
	size_t client_data_hash_len;
	char rp_id[OSK_MAX_RP_ID];
	size_t rp_id_len;
	uint8_t user_id[OSK_MAX_USER_ID];
	size_t user_id_len;
	char user_name[OSK_MAX_USER_NAME];
	bool have_es256;
	bool have_alg;
	const uint8_t *exclude[OSK_MAX_LIST];
	size_t exclude_len[OSK_MAX_LIST];
	size_t exclude_count;
	bool have_ext;
	osk_ctap_ext_in_t ext;
	bool opt_rk, opt_up, opt_uv;
	bool have_opt_up, have_opt_uv;
	const uint8_t *pin_auth;
	size_t pin_auth_len;
	bool have_pin_auth;
	uint64_t pin_protocol;
	bool have_pin_protocol;
	uint64_t enterprise;	/* 0x0A: 1 = RP-gefuehrt, 2 = Betrieb-gefuehrt */
	bool have_enterprise;
} mc_req_t;

/* Liest eine Liste von PublicKeyCredentialDescriptor. Zu lange oder
 * fremdartige Eintraege werden uebergangen, nicht geraten. */
static osk_result_t read_descriptor_list(osk_cbor_reader_t *r, const uint8_t **ids, size_t *lens,
					 size_t cap, size_t *count_out, bool *too_many)
{
	size_t count = 0;

	*count_out = 0;
	*too_many = false;
	if (osk_cbor_enter_array(r, &count) != OSK_OK)
		return OSK_ERR_FORMAT;
	if (count == 0) {
		osk_cbor_leave(r);
		return OSK_ERR_ARG; /* leere Liste ist verboten */
	}
	if (count > cap)
		*too_many = true;

	osk_result_t rc = OSK_OK;
	for (size_t i = 0; i < count; i++) {
		size_t fields = 0;
		const uint8_t *id = NULL;
		size_t id_len = 0;
		bool is_public_key = false;

		if (osk_cbor_enter_map(r, &fields) != OSK_OK) {
			rc = OSK_ERR_FORMAT;
			break;
		}
		for (size_t f = 0; f < fields && rc == OSK_OK; f++) {
			const char *k = NULL;
			size_t klen = 0;
			if (osk_cbor_read_text(r, &k, &klen) != OSK_OK) {
				rc = OSK_ERR_FORMAT;
				break;
			}
			if (klen == 2 && memcmp(k, "id", 2) == 0) {
				if (osk_cbor_read_bytes(r, &id, &id_len) != OSK_OK) {
					rc = OSK_ERR_FORMAT;
					break;
				}
			} else if (klen == 4 && memcmp(k, "type", 4) == 0) {
				const char *v = NULL;
				size_t vlen = 0;
				if (osk_cbor_read_text(r, &v, &vlen) != OSK_OK) {
					rc = OSK_ERR_FORMAT;
					break;
				}
				is_public_key = (vlen == 10 && memcmp(v, "public-key", 10) == 0);
			} else if (osk_cbor_skip(r) != OSK_OK) {
				rc = OSK_ERR_FORMAT;
				break;
			}
		}
		osk_cbor_leave(r);
		if (rc != OSK_OK)
			break;
		/* Fehlt id oder type, ist der Eintrag unvollstaendig. */
		if (!id) {
			rc = OSK_ERR_FORMAT;
			break;
		}
		if (!is_public_key || id_len > OSK_MAX_CRED_ID)
			continue;
		if (*count_out < cap) {
			ids[*count_out] = id;
			lens[*count_out] = id_len;
			(*count_out)++;
		}
	}
	osk_cbor_leave(r);
	return rc;
}

static osk_result_t read_options(osk_cbor_reader_t *r, bool *rk, bool *up, bool *uv, bool *have_up,
				 bool *have_uv, bool *have_unknown_rk)
{
	size_t count = 0;

	if (osk_cbor_enter_map(r, &count) != OSK_OK)
		return OSK_ERR_FORMAT;

	osk_result_t rc = OSK_OK;
	for (size_t i = 0; i < count; i++) {
		const char *k = NULL;
		size_t klen = 0;
		bool v = false;
		if (osk_cbor_read_text(r, &k, &klen) != OSK_OK) {
			rc = OSK_ERR_FORMAT;
			break;
		}
		if (osk_cbor_read_bool(r, &v) != OSK_OK) {
			/* Werte, die keine Wahrheitswerte sind, sind ein Formfehler */
			rc = OSK_ERR_FORMAT;
			break;
		}
		if (klen == 2 && memcmp(k, "rk", 2) == 0) {
			*rk = v;
			if (have_unknown_rk)
				*have_unknown_rk = true;
		} else if (klen == 2 && memcmp(k, "up", 2) == 0) {
			*up = v;
			*have_up = true;
		} else if (klen == 2 && memcmp(k, "uv", 2) == 0) {
			*uv = v;
			*have_uv = true;
		}
		/* Unbekannte Schalter gelten als nicht vorhanden. */
	}
	osk_cbor_leave(r);
	return rc;
}

static uint8_t parse_make_credential(const uint8_t *cbor, size_t len, mc_req_t *q)
{
	osk_cbor_reader_t r;
	size_t count = 0;
	bool exclude_too_many = false;

	memset(q, 0, sizeof(*q));
	q->opt_up = true; /* Vorgabe */

	if (osk_cbor_validate(cbor, len) != OSK_OK)
		return CTAP2_ERR_INVALID_CBOR;

	osk_cbor_reader_init(&r, cbor, len);
	if (osk_cbor_enter_map(&r, &count) != OSK_OK)
		return CTAP2_ERR_INVALID_CBOR;

	bool have_rp = false, have_user = false, have_params = false;

	for (size_t i = 0; i < count; i++) {
		uint64_t key = 0;
		if (osk_cbor_read_uint(&r, &key) != OSK_OK) {
			if (osk_cbor_skip(&r) != OSK_OK || osk_cbor_skip(&r) != OSK_OK)
				return CTAP2_ERR_INVALID_CBOR;
			continue;
		}

		switch (key) {
		case 0x01:
			if (osk_cbor_read_bytes(&r, &q->client_data_hash,
						&q->client_data_hash_len) != OSK_OK)
				return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
			if (q->client_data_hash_len != 32)
				return CTAP1_ERR_INVALID_PARAMETER;
			break;
		case 0x02: { /* rp */
			size_t fields = 0;
			if (osk_cbor_enter_map(&r, &fields) != OSK_OK)
				return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
			for (size_t f = 0; f < fields; f++) {
				const char *k = NULL;
				size_t klen = 0;
				if (osk_cbor_read_text(&r, &k, &klen) != OSK_OK)
					return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
				if (klen == 2 && memcmp(k, "id", 2) == 0) {
					const char *v = NULL;
					size_t vlen = 0;
					if (osk_cbor_read_text(&r, &v, &vlen) != OSK_OK)
						return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
					if (!copy_text(v, vlen, q->rp_id, sizeof(q->rp_id)))
						return CTAP1_ERR_INVALID_PARAMETER;
					q->rp_id_len = vlen;
					have_rp = (vlen > 0);
				} else if (osk_cbor_skip(&r) != OSK_OK) {
					return CTAP2_ERR_INVALID_CBOR;
				}
			}
			osk_cbor_leave(&r);
			break;
		}
		case 0x03: { /* user */
			size_t fields = 0;
			if (osk_cbor_enter_map(&r, &fields) != OSK_OK)
				return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
			for (size_t f = 0; f < fields; f++) {
				const char *k = NULL;
				size_t klen = 0;
				if (osk_cbor_read_text(&r, &k, &klen) != OSK_OK)
					return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
				if (klen == 2 && memcmp(k, "id", 2) == 0) {
					const uint8_t *v = NULL;
					size_t vlen = 0;
					if (osk_cbor_read_bytes(&r, &v, &vlen) != OSK_OK)
						return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
					if (vlen > OSK_MAX_USER_ID)
						return CTAP1_ERR_INVALID_PARAMETER;
					memcpy(q->user_id, v, vlen);
					q->user_id_len = vlen;
					have_user = true;
				} else if (klen == 4 && memcmp(k, "name", 4) == 0) {
					const char *v = NULL;
					size_t vlen = 0;
					if (osk_cbor_read_text(&r, &v, &vlen) != OSK_OK)
						return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
					if (vlen >= sizeof(q->user_name))
						vlen = sizeof(q->user_name) - 1;
					memcpy(q->user_name, v, vlen);
					q->user_name[vlen] = 0;
				} else if (osk_cbor_skip(&r) != OSK_OK) {
					return CTAP2_ERR_INVALID_CBOR;
				}
			}
			osk_cbor_leave(&r);
			break;
		}
		case 0x04: { /* pubKeyCredParams */
			size_t n = 0;
			if (osk_cbor_enter_array(&r, &n) != OSK_OK)
				return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
			have_params = true;
			for (size_t j = 0; j < n; j++) {
				size_t fields = 0;
				int64_t alg = 0;
				bool have_alg = false, is_pk = false;
				if (osk_cbor_enter_map(&r, &fields) != OSK_OK)
					return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
				for (size_t f = 0; f < fields; f++) {
					const char *k = NULL;
					size_t klen = 0;
					if (osk_cbor_read_text(&r, &k, &klen) != OSK_OK)
						return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
					if (klen == 3 && memcmp(k, "alg", 3) == 0) {
						if (osk_cbor_read_int(&r, &alg) != OSK_OK)
							return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
						have_alg = true;
					} else if (klen == 4 && memcmp(k, "type", 4) == 0) {
						const char *v = NULL;
						size_t vlen = 0;
						if (osk_cbor_read_text(&r, &v, &vlen) != OSK_OK)
							return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
						is_pk = (vlen == 10 &&
							 memcmp(v, "public-key", 10) == 0);
					} else if (osk_cbor_skip(&r) != OSK_OK) {
						return CTAP2_ERR_INVALID_CBOR;
					}
				}
				osk_cbor_leave(&r);
				/* Fehlt ein Pflichtfeld, ist die Nachricht fehlerhaft. */
				if (!have_alg)
					return CTAP2_ERR_INVALID_CBOR;
				q->have_alg = true;
				if (is_pk && alg == OSK_COSE_ALG_ES256 && !q->have_es256)
					q->have_es256 = true;
			}
			osk_cbor_leave(&r);
			break;
		}
		case 0x05: { /* excludeList */
			const osk_result_t rc =
			    read_descriptor_list(&r, q->exclude, q->exclude_len, OSK_MAX_LIST,
						 &q->exclude_count, &exclude_too_many);
			if (rc == OSK_ERR_ARG)
				return CTAP1_ERR_INVALID_PARAMETER;
			if (rc != OSK_OK)
				return CTAP2_ERR_INVALID_CBOR;
			break;
		}
		case 0x06:
			if (osk_ctap_ext_parse(&r, true, &q->ext) != OSK_OK)
				return CTAP2_ERR_INVALID_CBOR;
			q->have_ext = true;
			break;
		case 0x07: {
			bool have_rk = false;
			if (read_options(&r, &q->opt_rk, &q->opt_up, &q->opt_uv, &q->have_opt_up,
					 &q->have_opt_uv, &have_rk) != OSK_OK)
				return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
			break;
		}
		case 0x08:
			if (osk_cbor_read_bytes(&r, &q->pin_auth, &q->pin_auth_len) != OSK_OK)
				return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
			q->have_pin_auth = true;
			break;
		case 0x09:
			if (osk_cbor_read_uint(&r, &q->pin_protocol) != OSK_OK)
				return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
			q->have_pin_protocol = true;
			break;
		case 0x0a: /* enterpriseAttestation */
			if (osk_cbor_read_uint(&r, &q->enterprise) != OSK_OK)
				return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
			q->have_enterprise = true;
			break;
		default:
			if (osk_cbor_skip(&r) != OSK_OK)
				return CTAP2_ERR_INVALID_CBOR;
			break;
		}
	}
	osk_cbor_leave(&r);

	if (!q->client_data_hash || !have_rp || !have_user || !have_params)
		return CTAP2_ERR_MISSING_PARAMETER;
	if (exclude_too_many)
		return CTAP2_ERR_LIMIT_EXCEEDED;
	return CTAP2_OK;
}

/* ---- MakeCredential ---------------------------------------------------- */

static uint8_t do_make_credential(const uint8_t *cbor, size_t len, osk_cbor_writer_t *w)
{
	mc_req_t q;
	uint8_t rp_id_hash[32];
	uint8_t flags = 0;

	const uint8_t perr = parse_make_credential(cbor, len, &q);
	if (perr != CTAP2_OK)
		return perr;

	osk_pin_state_t *pin = osk_pin_state();

	/* Schritt 1: leeres pinUvAuthParam ist die alte Art, ein Geraet zu
	 * waehlen. Erst fragen, dann den passenden Fehler melden. */
	if (q.have_pin_auth && q.pin_auth_len == 0) {
		const uint8_t up = ask_user(OSK_ASK_LOGIN, q.rp_id, NULL);
		if (up != CTAP2_OK)
			return up;
		return pin->pin_set ? CTAP2_ERR_PIN_INVALID : CTAP2_ERR_PIN_NOT_SET;
	}

	/* Schritt 2 */
	if (q.have_pin_auth) {
		if (!q.have_pin_protocol)
			return CTAP2_ERR_MISSING_PARAMETER;
		if (!osk_pin_protocol_supported(q.pin_protocol))
			return CTAP1_ERR_INVALID_PARAMETER;
	}

	/* Schritt 3: Verfahren waehlen. */
	if (!q.have_es256)
		return CTAP2_ERR_UNSUPPORTED_ALGORITHM;

	/* Enterprise Attestation (Abschnitt 6.1.2, Schritt 9).
	 *
	 * Die beiden Fehlerwerte sind VERSCHIEDEN und das ist Absicht der
	 * Spezifikation: ausgeschaltet ist ein falscher PARAMETER (das Geraet
	 * kann es hier nicht), ein krummer Wert eine falsche OPTION. */
	if (q.have_enterprise) {
		if (!g_state.ep_enabled)
			return CTAP1_ERR_INVALID_PARAMETER;
		if (q.enterprise != 1u && q.enterprise != 2u)
			return CTAP2_ERR_INVALID_OPTION;
	}

	/* Schritt 4 und 5: Schalter deuten. */
	if (q.have_pin_auth)
		q.opt_uv = false; /* pinUvAuthParam hat Vorrang */
	if (q.have_opt_up && !q.opt_up)
		return CTAP2_ERR_INVALID_OPTION;

	/* Schritt 7: Ohne Benutzerpruefung keinen residenten Schluessel.
	 * makeCredUvNotRqd meldet true, nicht residente duerfen also ohne. */
	if (pin->pin_set && !q.have_pin_auth && !q.opt_uv && q.opt_rk)
		return CTAP2_ERR_PUAT_REQUIRED;

	osk_sha256((const uint8_t *)q.rp_id, q.rp_id_len, rp_id_hash);

	/* Schritt 11: Benutzerpruefung. */
	if (q.have_pin_auth) {
		if (!osk_pin_verify_token(q.pin_protocol, q.client_data_hash, 32, q.pin_auth,
					  q.pin_auth_len))
			return CTAP2_ERR_PIN_AUTH_INVALID;
		if (!osk_pin_has_permission(OSK_PERM_MC))
			return CTAP2_ERR_PIN_AUTH_INVALID;
		if (pin->rp_id_set && !osk_memeq(pin->rp_id_hash, rp_id_hash, 32))
			return CTAP2_ERR_PIN_AUTH_INVALID;
		if (!osk_pin_get_uv_flag())
			return CTAP2_ERR_PIN_AUTH_INVALID;
		if (!pin->rp_id_set) {
			memcpy(pin->rp_id_hash, rp_id_hash, 32);
			pin->rp_id_set = true;
		}
		flags |= OSK_AD_FLAG_UV;
	} else if (q.opt_uv) {
		bool verified = false;
		const osk_result_t rc = osk_ui_verify_user(&verified);
		if (rc == OSK_ERR_TIMEOUT)
			return CTAP2_ERR_USER_ACTION_TIMEOUT;
		if (rc != OSK_OK || !verified)
			return pin->pin_set ? CTAP2_ERR_PUAT_REQUIRED : CTAP2_ERR_OPERATION_DENIED;
		flags |= (uint8_t)(OSK_AD_FLAG_UV | OSK_AD_FLAG_UP);
	}

	/* Schritt 12: excludeList. Ein Treffer wird erst nach einem
	 * Tastendruck gemeldet - genau wie bei U2F. */
	for (size_t i = 0; i < q.exclude_count; i++) {
		osk_credid_t found;
		if (osk_credid_unpack(q.exclude[i], q.exclude_len[i], &found) != OSK_OK)
			continue;
		if (!osk_memeq(found.rp_id_hash, rp_id_hash, 32))
			continue;
		if (found.cred_protect == OSK_CRED_PROTECT_UV_REQUIRED &&
		    (flags & OSK_AD_FLAG_UV) == 0)
			continue; /* gilt als nicht gefunden */
		/* Ein geloeschter auffindbarer Nachweis schliesst nicht mehr
		 * aus - sonst koennte sich niemand nach dem Loeschen neu
		 * anmelden. Gleiche Regel wie bei GetAssertion. */
		if (found.resident &&
		    !rk_has_cred(rp_id_hash, q.exclude[i], q.exclude_len[i], NULL))
			continue;
		if ((flags & OSK_AD_FLAG_UP) == 0)
			(void)ask_user(OSK_ASK_LOGIN, q.rp_id, NULL);
		return CTAP2_ERR_CREDENTIAL_EXCLUDED;
	}

	/* Schritt 14: Tastendruck, falls nicht schon geschehen. */
	if ((flags & OSK_AD_FLAG_UP) == 0) {
		if (q.have_pin_auth && osk_pin_get_up_flag()) {
			flags |= OSK_AD_FLAG_UP;
		} else {
			const uint8_t up = ask_user(OSK_ASK_LOGIN, q.rp_id, q.user_name);
			if (up != CTAP2_OK)
				return up;
			flags |= OSK_AD_FLAG_UP;
		}
	}
	osk_pin_clear_up();
	osk_pin_clear_uv();
	osk_pin_clear_perms_except_lbw();

	/* Schritt 16: Schluessel ableiten und Credential-ID bilden. */
	osk_credid_t cid;
	memset(&cid, 0, sizeof(cid));
	cid.resident = q.opt_rk;
	cid.hmac_secret = q.ext.hmac_secret_create;
	cid.cred_protect =
	    q.ext.cred_protect ? q.ext.cred_protect_value : OSK_CRED_PROTECT_OPTIONAL;
	memcpy(cid.rp_id_hash, rp_id_hash, 32);
	cid.created = (uint32_t)(osk_ctap_now_ms() / 1000u);
	if (osk_se050_random(cid.salt, sizeof(cid.salt)) != OSK_OK)
		return CTAP1_ERR_OTHER;

	uint8_t cred_id[OSK_MAX_CRED_ID];
	size_t cred_id_len = 0;
	if (osk_credid_pack(&cid, cred_id, sizeof(cred_id), &cred_id_len) != OSK_OK)
		return CTAP1_ERR_OTHER;

	uint8_t pub[64];
	if (osk_ctap_key_public(cid.salt, cid.rp_id_hash, pub) != OSK_OK)
		return CTAP1_ERR_OTHER;

	/* Schritt 17: residente Nachweise liegen im Tresor. */
	if (q.opt_rk) {
		uint8_t rec[OSK_RK_RECORD_MAX];
		osk_item_meta_t meta;
		osk_item_id_t old_id = 0;
		osk_item_id_t new_id = 0;

		const size_t rec_len = osk_ctap_rk_pack(cred_id, cred_id_len, q.user_id, q.user_id_len,
					       q.user_name, rec, sizeof(rec));
		if (rec_len == 0)
			return CTAP1_ERR_OTHER;

		memset(&meta, 0, sizeof(meta));
		meta.type = OSK_ITEM_FILE;
		meta.profile = 0;
		meta.name = q.rp_id;
		meta.site = OSK_RK_SITE;
		meta.mirrored = true;
		meta.changed_utc = osk_ctap_now_ms() / 1000u;
		/* Suchschluessel: der Hash des Dienstnamens. Er liegt im
		 * verschluesselten Teil des Tresors, verraet also nichts. */
		memcpy(meta.tag, rp_id_hash, 32);
		meta.tag_len = 32;

		osk_result_t rc;
		if (rk_find(rp_id_hash, q.user_id, q.user_id_len, &old_id, NULL, NULL) == OSK_OK)
			rc = osk_store_update(old_id, &meta, rec, rec_len);
		else
			rc = osk_store_add(&meta, rec, rec_len, &new_id);
		osk_wipe(rec, sizeof(rec));

		if (rc == OSK_ERR_FULL)
			return CTAP2_ERR_KEY_STORE_FULL;
		if (rc != OSK_OK)
			return CTAP1_ERR_OTHER;
	}

	/* Erweiterungsausgaben */
	uint8_t ext_cbor[OSK_MAX_EXT_OUT];
	size_t ext_len = 0;
	if (q.have_ext && osk_ctap_ext_make_output(&q.ext, q.rp_id, q.rp_id_len, cid.cred_protect,
						   ext_cbor, sizeof(ext_cbor), &ext_len) != OSK_OK)
		return CTAP1_ERR_OTHER;
	if (ext_len > 0)
		flags |= OSK_AD_FLAG_ED;

	uint8_t unsigned_cbor[OSK_MAX_EXT_OUT];
	size_t unsigned_len = 0;
	if (q.have_ext &&
	    osk_ctap_ext_unsigned_output(&q.ext, &cid, false, unsigned_cbor, sizeof(unsigned_cbor),
					 &unsigned_len) != OSK_OK)
		return CTAP1_ERR_OTHER;

	/* Attested Credential Data */
	uint8_t attested[16 + 2 + OSK_MAX_CRED_ID + 80];
	size_t attested_len = 0;
	{
		osk_cbor_writer_t kw;
		memcpy(attested, osk_ctap_aaguid(), 16);
		attested[16] = (uint8_t)(cred_id_len >> 8);
		attested[17] = (uint8_t)cred_id_len;
		memcpy(attested + 18, cred_id, cred_id_len);
		osk_cbor_writer_init(&kw, attested + 18 + cred_id_len,
				     sizeof(attested) - 18 - cred_id_len);
		osk_ctap_put_cose_p256(&kw, pub);
		if (!osk_cbor_writer_ok(&kw))
			return CTAP1_ERR_OTHER;
		attested_len = 18 + cred_id_len + kw.len;
	}
	flags |= OSK_AD_FLAG_AT;

	/* Der Signaturzaehler kommt aus dem Flash und wird VOR dem Signieren
	 * erhoeht - siehe osk_sigcount.h. Geht das nicht, wird nicht
	 * signiert: eine Signatur mit einer Zahl, auf die sich niemand
	 * verlassen kann, ist schlechter als keine. */
	uint32_t sign_count = 0;
	if (osk_sigcount_next(&sign_count) != OSK_OK)
		return CTAP1_ERR_OTHER;

	uint8_t auth_data[OSK_MAX_AUTH_DATA];
	size_t auth_len = 0;
	if (osk_ctap_build_auth_data(rp_id_hash, flags, sign_count, attested, attested_len,
				     ext_cbor, ext_len, auth_data, sizeof(auth_data),
				     &auth_len) != OSK_OK)
		return CTAP2_ERR_REQUEST_TOO_LARGE;

	/* Eigenbeglaubigung (self attestation): unterschrieben wird mit dem
	 * gerade entstandenen Schluessel. Das ist die datenschonendste Form -
	 * sie verraet kein Geraetemodell. */
	uint8_t to_sign_hash[32];
	{
		osk_sha256_t c;
		osk_sha256_init(&c);
		osk_sha256_update(&c, auth_data, auth_len);
		osk_sha256_update(&c, q.client_data_hash, 32);
		osk_sha256_final(&c, to_sign_hash);
	}

	uint8_t sig[OSK_MAX_SIG];
	size_t sig_len = 0;
	if (osk_ctap_key_sign(cid.salt, cid.rp_id_hash, to_sign_hash, sig, sizeof(sig), &sig_len) !=
	    OSK_OK)
		return CTAP1_ERR_OTHER;

	/* Antwort: fmt, authData, attStmt und, falls vorhanden, die
	 * ungesignierten Ausgaben. */
	const size_t entries = (unsigned_len > 0) ? 4u : 3u;
	osk_cbor_put_map(w, entries);
	osk_cbor_put_uint(w, 0x01);
	osk_cbor_put_text(w, "packed");
	osk_cbor_put_uint(w, 0x02);
	osk_cbor_put_bytes(w, auth_data, auth_len);
	/* Hier stuende der Schluessel 0x04 (epAtt), wenn eine echte Enterprise
	 * Attestation herausgegeben worden waere. Sie wird NICHT herausgegeben,
	 * und zwar aus einem Grund, nicht aus Vergessen: eine Enterprise
	 * Attestation ist nur etwas wert, wenn sie ein GERAETEZEUGNIS mitbringt
	 * (x5c mit einem Zertifikat der Serie). Dieses Geraet hat keines - es
	 * beglaubigt sich selbst, siehe docs/ctap.md. Eingeschaltete Enterprise
	 * Attestation wird darum mit einer gewoehnlichen Beglaubigung
	 * beantwortet; das erlaubt die Spezifikation ausdruecklich. epAtt zu
	 * melden, ohne das Zeugnis zu haben, waere eine Luege gegenueber dem
	 * Betrieb, der genau darauf baut.
	 *
	 * Was fehlt, ist also ein Serienzertifikat und sein Schluessel im
	 * Sicherheitschip - nicht Code an dieser Stelle. */
	osk_cbor_put_uint(w, 0x03);
	osk_cbor_put_map(w, 2);
	osk_cbor_put_text(w, "alg");
	osk_cbor_put_int(w, OSK_COSE_ALG_ES256);
	osk_cbor_put_text(w, "sig");
	osk_cbor_put_bytes(w, sig, sig_len);
	if (unsigned_len > 0) {
		osk_cbor_put_uint(w, 0x06);
		/* schon fertiges CBOR, unveraendert anfuegen */
		if (w->cap - w->len < unsigned_len)
			return CTAP2_ERR_REQUEST_TOO_LARGE;
		memcpy(w->buf + w->len, unsigned_cbor, unsigned_len);
		w->len += unsigned_len;
	}

	return osk_cbor_writer_ok(w) ? CTAP2_OK : CTAP2_ERR_REQUEST_TOO_LARGE;
}

/* ---- GetAssertion ------------------------------------------------------ */

typedef struct {
	char rp_id[OSK_MAX_RP_ID];
	size_t rp_id_len;
	const uint8_t *client_data_hash;
	size_t client_data_hash_len;
	const uint8_t *allow[OSK_MAX_LIST];
	size_t allow_len[OSK_MAX_LIST];
	size_t allow_count;
	bool allow_present;
	bool have_ext;
	osk_ctap_ext_in_t ext;
	bool opt_up, opt_uv, opt_rk_seen;
	bool have_opt_up;
	const uint8_t *pin_auth;
	size_t pin_auth_len;
	bool have_pin_auth;
	uint64_t pin_protocol;
	bool have_pin_protocol;
} ga_req_t;

static uint8_t parse_get_assertion(const uint8_t *cbor, size_t len, ga_req_t *q)
{
	osk_cbor_reader_t r;
	size_t count = 0;
	bool allow_too_many = false;

	memset(q, 0, sizeof(*q));
	q->opt_up = true;

	if (osk_cbor_validate(cbor, len) != OSK_OK)
		return CTAP2_ERR_INVALID_CBOR;

	osk_cbor_reader_init(&r, cbor, len);
	if (osk_cbor_enter_map(&r, &count) != OSK_OK)
		return CTAP2_ERR_INVALID_CBOR;

	bool have_rp = false;

	for (size_t i = 0; i < count; i++) {
		uint64_t key = 0;
		if (osk_cbor_read_uint(&r, &key) != OSK_OK) {
			if (osk_cbor_skip(&r) != OSK_OK || osk_cbor_skip(&r) != OSK_OK)
				return CTAP2_ERR_INVALID_CBOR;
			continue;
		}
		switch (key) {
		case 0x01: {
			const char *v = NULL;
			size_t vlen = 0;
			if (osk_cbor_read_text(&r, &v, &vlen) != OSK_OK)
				return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
			if (vlen == 0 || !copy_text(v, vlen, q->rp_id, sizeof(q->rp_id)))
				return CTAP1_ERR_INVALID_PARAMETER;
			q->rp_id_len = vlen;
			have_rp = true;
			break;
		}
		case 0x02:
			if (osk_cbor_read_bytes(&r, &q->client_data_hash,
						&q->client_data_hash_len) != OSK_OK)
				return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
			if (q->client_data_hash_len != 32)
				return CTAP1_ERR_INVALID_PARAMETER;
			break;
		case 0x03: {
			const osk_result_t rc =
			    read_descriptor_list(&r, q->allow, q->allow_len, OSK_MAX_LIST,
						 &q->allow_count, &allow_too_many);
			if (rc == OSK_ERR_ARG)
				return CTAP1_ERR_INVALID_PARAMETER;
			if (rc != OSK_OK)
				return CTAP2_ERR_INVALID_CBOR;
			q->allow_present = true;
			break;
		}
		case 0x04:
			if (osk_ctap_ext_parse(&r, false, &q->ext) != OSK_OK)
				return CTAP2_ERR_INVALID_CBOR;
			q->have_ext = true;
			break;
		case 0x05: {
			bool rk = false, have_uv = false;
			bool rk_seen = false;
			if (read_options(&r, &rk, &q->opt_up, &q->opt_uv, &q->have_opt_up, &have_uv,
					 &rk_seen) != OSK_OK)
				return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
			q->opt_rk_seen = rk_seen;
			break;
		}
		case 0x06:
			if (osk_cbor_read_bytes(&r, &q->pin_auth, &q->pin_auth_len) != OSK_OK)
				return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
			q->have_pin_auth = true;
			break;
		case 0x07:
			if (osk_cbor_read_uint(&r, &q->pin_protocol) != OSK_OK)
				return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
			q->have_pin_protocol = true;
			break;
		default:
			if (osk_cbor_skip(&r) != OSK_OK)
				return CTAP2_ERR_INVALID_CBOR;
			break;
		}
	}
	osk_cbor_leave(&r);

	if (!have_rp || !q->client_data_hash)
		return CTAP2_ERR_MISSING_PARAMETER;
	if (allow_too_many)
		return CTAP2_ERR_LIMIT_EXCEEDED;
	if (q->opt_rk_seen)
		return CTAP2_ERR_UNSUPPORTED_OPTION;
	return CTAP2_OK;
}

/* Schreibt eine Antwort auf GetAssertion oder GetNextAssertion. */
static uint8_t emit_assertion(const osk_cred_t *cred, const osk_credid_t *cid,
			      const uint8_t rp_id_hash[32], const uint8_t client_data_hash[32],
			      uint8_t flags, size_t number_of_credentials, const uint8_t *ext_cbor,
			      size_t ext_len, const uint8_t *unsigned_cbor, size_t unsigned_len,
			      osk_cbor_writer_t *w)
{
	uint8_t auth_data[OSK_MAX_AUTH_DATA];
	size_t auth_len = 0;

	if (ext_len > 0)
		flags |= OSK_AD_FLAG_ED;

	/* Siehe osk_sigcount.h: aus dem Flash, vor dem Signieren. */
	uint32_t sign_count = 0;
	if (osk_sigcount_next(&sign_count) != OSK_OK)
		return CTAP1_ERR_OTHER;

	if (osk_ctap_build_auth_data(rp_id_hash, flags, sign_count, NULL, 0, ext_cbor, ext_len,
				     auth_data, sizeof(auth_data), &auth_len) != OSK_OK)
		return CTAP2_ERR_REQUEST_TOO_LARGE;

	uint8_t hash[32];
	{
		osk_sha256_t c;
		osk_sha256_init(&c);
		osk_sha256_update(&c, auth_data, auth_len);
		osk_sha256_update(&c, client_data_hash, 32);
		osk_sha256_final(&c, hash);
	}

	uint8_t sig[OSK_MAX_SIG];
	size_t sig_len = 0;
	if (osk_ctap_key_sign(cid->salt, cid->rp_id_hash, hash, sig, sizeof(sig), &sig_len) !=
	    OSK_OK)
		return CTAP1_ERR_OTHER;

	/* Nutzerangaben nur, wenn eine Benutzerpruefung gelaufen ist. */
	const bool with_user = (cred->resident && cred->user_id_len > 0);
	const bool with_user_details =
	    with_user && (flags & OSK_AD_FLAG_UV) != 0 && cred->user_name[0] != 0;

	size_t entries = 3;
	if (with_user)
		entries++;
	if (number_of_credentials > 1)
		entries++;
	if (unsigned_len > 0)
		entries++;

	osk_cbor_put_map(w, entries);
	osk_cbor_put_uint(w, 0x01);
	osk_cbor_put_map(w, 2);
	osk_cbor_put_text(w, "id");
	osk_cbor_put_bytes(w, cred->cred_id, cred->cred_id_len);
	osk_cbor_put_text(w, "type");
	osk_cbor_put_text(w, "public-key");
	osk_cbor_put_uint(w, 0x02);
	osk_cbor_put_bytes(w, auth_data, auth_len);
	osk_cbor_put_uint(w, 0x03);
	osk_cbor_put_bytes(w, sig, sig_len);
	if (with_user) {
		osk_cbor_put_uint(w, 0x04);
		osk_cbor_put_map(w, with_user_details ? 2u : 1u);
		osk_cbor_put_text(w, "id");
		osk_cbor_put_bytes(w, cred->user_id, cred->user_id_len);
		if (with_user_details) {
			osk_cbor_put_text(w, "name");
			osk_cbor_put_text(w, cred->user_name);
		}
	}
	if (number_of_credentials > 1) {
		osk_cbor_put_uint(w, 0x05);
		osk_cbor_put_uint(w, number_of_credentials);
	}
	if (unsigned_len > 0) {
		osk_cbor_put_uint(w, 0x08);
		if (w->cap - w->len < unsigned_len)
			return CTAP2_ERR_REQUEST_TOO_LARGE;
		memcpy(w->buf + w->len, unsigned_cbor, unsigned_len);
		w->len += unsigned_len;
	}

	return osk_cbor_writer_ok(w) ? CTAP2_OK : CTAP2_ERR_REQUEST_TOO_LARGE;
}

static uint8_t do_get_assertion(const uint8_t *cbor, size_t len, osk_cbor_writer_t *w)
{
	ga_req_t q;
	uint8_t rp_id_hash[32];
	uint8_t flags = 0;
	osk_cred_t found[OSK_MAX_CANDIDATES];
	osk_credid_t cids[OSK_MAX_CANDIDATES];
	size_t n = 0;

	const uint8_t perr = parse_get_assertion(cbor, len, &q);
	if (perr != CTAP2_OK)
		return perr;

	osk_pin_state_t *pin = osk_pin_state();
	g_state.next_state.active = false;

	if (q.have_pin_auth && q.pin_auth_len == 0) {
		const uint8_t up = ask_user(OSK_ASK_LOGIN, q.rp_id, NULL);
		if (up != CTAP2_OK)
			return up;
		return pin->pin_set ? CTAP2_ERR_PIN_INVALID : CTAP2_ERR_PIN_NOT_SET;
	}
	if (q.have_pin_auth) {
		if (!q.have_pin_protocol)
			return CTAP2_ERR_MISSING_PARAMETER;
		if (!osk_pin_protocol_supported(q.pin_protocol))
			return CTAP1_ERR_INVALID_PARAMETER;
		q.opt_uv = false;
	}

	osk_sha256((const uint8_t *)q.rp_id, q.rp_id_len, rp_id_hash);

	/* Schritt 6: Benutzerpruefung. */
	if (q.have_pin_auth) {
		if (!osk_pin_verify_token(q.pin_protocol, q.client_data_hash, 32, q.pin_auth,
					  q.pin_auth_len))
			return CTAP2_ERR_PIN_AUTH_INVALID;
		if (!osk_pin_get_uv_flag())
			return CTAP2_ERR_PIN_AUTH_INVALID;
		if (!osk_pin_has_permission(OSK_PERM_GA))
			return CTAP2_ERR_PIN_AUTH_INVALID;
		if (pin->rp_id_set && !osk_memeq(pin->rp_id_hash, rp_id_hash, 32))
			return CTAP2_ERR_PIN_AUTH_INVALID;
		if (!pin->rp_id_set) {
			memcpy(pin->rp_id_hash, rp_id_hash, 32);
			pin->rp_id_set = true;
		}
		flags |= OSK_AD_FLAG_UV;
	} else if (q.opt_uv) {
		bool verified = false;
		const osk_result_t rc = osk_ui_verify_user(&verified);
		if (rc == OSK_ERR_TIMEOUT)
			return CTAP2_ERR_USER_ACTION_TIMEOUT;
		if (rc != OSK_OK || !verified)
			return pin->pin_set ? CTAP2_ERR_PUAT_REQUIRED : CTAP2_ERR_OPERATION_DENIED;
		flags |= (uint8_t)(OSK_AD_FLAG_UV | OSK_AD_FLAG_UP);
	}

	/* Schritt 7: passende Nachweise suchen. */
	if (q.allow_present) {
		for (size_t i = 0; i < q.allow_count && n < OSK_MAX_CANDIDATES; i++) {
			osk_credid_t cid;
			if (osk_credid_unpack(q.allow[i], q.allow_len[i], &cid) != OSK_OK)
				continue;
			if (!osk_memeq(cid.rp_id_hash, rp_id_hash, 32))
				continue;
			if (cid.cred_protect == OSK_CRED_PROTECT_UV_REQUIRED &&
			    (flags & OSK_AD_FLAG_UV) == 0)
				continue;
			memset(&found[n], 0, sizeof(found[n]));
			memcpy(found[n].cred_id, q.allow[i], q.allow_len[i]);
			found[n].cred_id_len = q.allow_len[i];
			found[n].cred_protect = cid.cred_protect;
			found[n].hmac_secret = cid.hmac_secret;
			found[n].created = cid.created;
			/* Nutzerangaben aus dem Tresor dazu - und fehlt der
			 * Eintrag, gilt der Nachweis als nicht vorhanden.
			 * Warum: siehe rk_has_cred(). */
			if (cid.resident) {
				osk_cred_t rk;
				memset(&rk, 0, sizeof(rk));
				if (!rk_has_cred(rp_id_hash, q.allow[i], q.allow_len[i], &rk))
					continue; /* geloescht: nicht vorhanden */
				found[n].resident = true;
				found[n].user_id_len = rk.user_id_len;
				memcpy(found[n].user_id, rk.user_id, rk.user_id_len);
				memcpy(found[n].user_name, rk.user_name,
				       sizeof(found[n].user_name));
			}
			cids[n] = cid;
			n++;
		}
	} else {
		size_t at = 0;
		osk_cred_t rk;
		memset(&rk, 0, sizeof(rk));
		while (n < OSK_MAX_CANDIDATES &&
		       rk_find(rp_id_hash, NULL, 0, NULL, &rk, &at) == OSK_OK) {
			osk_credid_t cid;
			if (osk_credid_unpack(rk.cred_id, rk.cred_id_len, &cid) != OSK_OK)
				continue;
			if (!osk_memeq(cid.rp_id_hash, rp_id_hash, 32))
				continue;
			if (cid.cred_protect == OSK_CRED_PROTECT_UV_REQUIRED &&
			    (flags & OSK_AD_FLAG_UV) == 0)
				continue;
			if (cid.cred_protect == OSK_CRED_PROTECT_WITH_ID_LIST &&
			    (flags & OSK_AD_FLAG_UV) == 0)
				continue;
			found[n] = rk;
			found[n].cred_protect = cid.cred_protect;
			found[n].hmac_secret = cid.hmac_secret;
			found[n].created = cid.created;
			cids[n] = cid;
			n++;
		}
	}

	if (n == 0)
		return CTAP2_ERR_NO_CREDENTIALS;

	/* Schritt 9: Tastendruck. */
	if ((flags & OSK_AD_FLAG_UP) == 0 && q.opt_up) {
		if (q.have_pin_auth && osk_pin_get_up_flag()) {
			flags |= OSK_AD_FLAG_UP;
		} else {
			const uint8_t up =
			    ask_user(OSK_ASK_LOGIN, q.rp_id,
				     found[0].user_name[0] ? found[0].user_name : NULL);
			if (up != CTAP2_OK)
				return up;
			flags |= OSK_AD_FLAG_UP;
		}
	}
	osk_pin_clear_up();
	osk_pin_clear_uv();
	osk_pin_clear_perms_except_lbw();

	/* hmac-secret braucht einen Tastendruck (12.7). */
	if (q.ext.hmac_secret_get && !q.opt_up)
		return CTAP2_ERR_UNSUPPORTED_OPTION;

	/* Schritt 11 und 12: einen Nachweis auswaehlen. Ohne allowList und
	 * mit mehreren Treffern wird der neueste zuerst gemeldet, der Rest
	 * ueber GetNextAssertion. */
	size_t pick = 0;
	if (!q.allow_present && n > 1) {
		for (size_t i = 1; i < n; i++) {
			if (found[i].created > found[pick].created)
				pick = i;
		}
	}

	uint8_t ext_cbor[OSK_MAX_EXT_OUT];
	size_t ext_len = 0;
	uint8_t status = CTAP2_OK;
	if (q.have_ext) {
		if (osk_ctap_ext_get_output(&q.ext, &cids[pick], (flags & OSK_AD_FLAG_UV) != 0,
					    ext_cbor, sizeof(ext_cbor), &ext_len,
					    &status) != OSK_OK)
			return CTAP1_ERR_OTHER;
		if (status != CTAP2_OK)
			return status;
	}

	uint8_t unsigned_cbor[OSK_MAX_EXT_OUT];
	size_t unsigned_len = 0;
	if (q.have_ext &&
	    osk_ctap_ext_unsigned_output(&q.ext, &cids[pick], q.allow_present, unsigned_cbor,
					 sizeof(unsigned_cbor), &unsigned_len) != OSK_OK)
		return CTAP1_ERR_OTHER;

	const size_t report_count = (!q.allow_present) ? n : 1u;

	/* Zustand fuer GetNextAssertion merken. */
	if (report_count > 1) {
		osk_get_next_t *st = &g_state.next_state;
		memset(st, 0, sizeof(*st));
		st->active = true;
		memcpy(st->rp_id_hash, rp_id_hash, 32);
		memcpy(st->client_data_hash, q.client_data_hash, 32);
		st->flags = flags;
		st->count = 0;
		/* Reihenfolge: neuester zuerst. */
		size_t order[OSK_MAX_CANDIDATES];
		for (size_t i = 0; i < n; i++)
			order[i] = i;
		for (size_t i = 0; i < n; i++) {
			for (size_t j = i + 1; j < n; j++) {
				if (found[order[j]].created > found[order[i]].created) {
					const size_t t = order[i];
					order[i] = order[j];
					order[j] = t;
				}
			}
		}
		for (size_t i = 0; i < n; i++)
			st->creds[st->count++] = found[order[i]];
		st->next = 1;
		st->last_use_ms = osk_ctap_now_ms();
		pick = order[0];
	}

	return emit_assertion(&found[pick], &cids[pick], rp_id_hash, q.client_data_hash, flags,
			      report_count, ext_cbor, ext_len, unsigned_cbor, unsigned_len, w);
}

static uint8_t do_get_next_assertion(osk_cbor_writer_t *w)
{
	osk_get_next_t *st = &g_state.next_state;

	if (!st->active)
		return CTAP2_ERR_NOT_ALLOWED;
	if (st->next >= st->count) {
		st->active = false;
		return CTAP2_ERR_NOT_ALLOWED;
	}
	if (osk_ctap_now_ms() - st->last_use_ms > OSK_GET_NEXT_WINDOW_MS) {
		st->active = false;
		return CTAP2_ERR_NOT_ALLOWED;
	}

	const osk_cred_t *cred = &st->creds[st->next];
	osk_credid_t cid;
	if (osk_credid_unpack(cred->cred_id, cred->cred_id_len, &cid) != OSK_OK) {
		st->active = false;
		return CTAP1_ERR_OTHER;
	}

	const uint8_t code = emit_assertion(cred, &cid, st->rp_id_hash, st->client_data_hash,
					    st->flags, 1, NULL, 0, NULL, 0, w);
	if (code == CTAP2_OK) {
		st->next++;
		st->last_use_ms = osk_ctap_now_ms();
	}
	return code;
}

/* ---- GetInfo ----------------------------------------------------------- */

static uint8_t do_get_info(osk_cbor_writer_t *w)
{
	const osk_pin_state_t *pin = osk_pin_state();
	size_t rk_left = 0;
	size_t rk_count = 0;

	if (osk_store_count(0, &rk_count) != OSK_OK)
		rk_count = 0;
	rk_left = (rk_count < OSK_MAX_RESIDENT) ? OSK_MAX_RESIDENT - rk_count : 0u;

	/* Schluessel aufsteigend, wie die kanonische Form verlangt. */
	osk_cbor_put_map(w, 14);

	osk_cbor_put_uint(w, 0x01);
	osk_cbor_put_array(w, 3);
	osk_cbor_put_text(w, "U2F_V2");
	osk_cbor_put_text(w, "FIDO_2_0");
	osk_cbor_put_text(w, "FIDO_2_1");

	osk_cbor_put_uint(w, 0x02);
	osk_cbor_put_array(w, 4);
	osk_cbor_put_text(w, "credProtect");
	osk_cbor_put_text(w, "hmac-secret");
	osk_cbor_put_text(w, "largeBlob");
	osk_cbor_put_text(w, "minPinLength");

	osk_cbor_put_uint(w, 0x03);
	osk_cbor_put_bytes(w, osk_ctap_aaguid(), 16);

	osk_cbor_put_uint(w, 0x04);
	/* "uv" meint in CTAP 2.1 die im GERAET eingebaute Benutzerpruefung -
	 * also Fingerabdruck oder dergleichen -, nicht die PIN. Die wird
	 * getrennt ueber "clientPin" gemeldet.
	 *
	 * Solange kein Sensor verbaut ist, waere "uv": true eine falsche
	 * Zusage: ein Dienst, der userVerification verlangt, glaubt dann, das
	 * Geraet koenne es selbst. Also lassen wir die Option weg - so sagt die
	 * Spezifikation, dass wir sie nicht unterstuetzen. Kommt der Sensor,
	 * wird daraus ein echtes true (dann sind es wieder sieben Eintraege).
	 *
	 * Gefunden vom Fingerabdruck-Strang beim Nachlesen, ob der Sensor
	 * Aenderungen am CTAP-Teil braucht. */
	/* Die Reihenfolge ist nicht Geschmack: kanonisches CBOR sortiert
	 * Textschluessel erst nach LAENGE, dann nach Bytes. Darum
	 * ep/rk/up (2), plat (4), credMgmt (8), authnrCfg/clientPin (9),
	 * pinUvAuthToken (14), setMinPINLength (15), makeCredUvNotRqd (16).
	 * Eine falsche Reihenfolge wuerde unser eigener Leser verwerfen -
	 * genau das prueft test_ctap. */
	osk_cbor_put_map(w, 10);
	/* "ep" vorhanden heisst: das Geraet KANN Enterprise Attestation; der
	 * Wert sagt, ob sie eingeschaltet ist. Ab Werk aus, einschaltbar nur
	 * ueber authenticatorConfig 0x01 - keine Webseite kann das. */
	osk_cbor_put_text(w, "ep");
	osk_cbor_put_bool(w, g_state.ep_enabled);
	osk_cbor_put_text(w, "rk");
	osk_cbor_put_bool(w, true);
	osk_cbor_put_text(w, "up");
	osk_cbor_put_bool(w, true);
	osk_cbor_put_text(w, "plat");
	osk_cbor_put_bool(w, false);
	osk_cbor_put_text(w, "credMgmt");
	osk_cbor_put_bool(w, true);
	osk_cbor_put_text(w, "authnrCfg");
	osk_cbor_put_bool(w, true);
	osk_cbor_put_text(w, "clientPin");
	osk_cbor_put_bool(w, pin->pin_set);
	osk_cbor_put_text(w, "pinUvAuthToken");
	osk_cbor_put_bool(w, true);
	osk_cbor_put_text(w, "setMinPINLength");
	osk_cbor_put_bool(w, true);
	osk_cbor_put_text(w, "makeCredUvNotRqd");
	osk_cbor_put_bool(w, true);
	/* "alwaysUv" wird ABSICHTLICH nicht gemeldet: Config 0x02
	 * (toggleAlwaysUv) ist nicht gebaut. Eine Option zu melden, deren
	 * Schalter fehlt, ist eine falsche Zusage - dieselbe Sorte Fehler, die
	 * bis zum 24.10.2026 bei "uv" stand. */

	osk_cbor_put_uint(w, 0x05);
	osk_cbor_put_uint(w, OSK_MAX_MSG);

	osk_cbor_put_uint(w, 0x06);
	osk_cbor_put_array(w, 2);
	osk_cbor_put_uint(w, 2);
	osk_cbor_put_uint(w, 1);

	osk_cbor_put_uint(w, 0x07);
	osk_cbor_put_uint(w, OSK_MAX_LIST);

	osk_cbor_put_uint(w, 0x08);
	osk_cbor_put_uint(w, OSK_CREDID_LEN);

	osk_cbor_put_uint(w, 0x09);
	osk_cbor_put_array(w, 2);
	osk_cbor_put_text(w, "usb");
	osk_cbor_put_text(w, "nfc");

	osk_cbor_put_uint(w, 0x0a);
	osk_cbor_put_array(w, 1);
	osk_cbor_put_map(w, 2);
	osk_cbor_put_text(w, "alg");
	osk_cbor_put_int(w, OSK_COSE_ALG_ES256);
	osk_cbor_put_text(w, "type");
	osk_cbor_put_text(w, "public-key");

	osk_cbor_put_uint(w, 0x0c);
	osk_cbor_put_bool(w, g_state.force_pin_change);

	osk_cbor_put_uint(w, 0x0d);
	osk_cbor_put_uint(w, g_state.min_pin_len);

	/* 0x10 maxRPIDsForSetMinPINLength: wie viele Dienste die Liste der
	 * minPinLength-Erweiterung aufnimmt. Die Zahl MUSS gemeldet werden,
	 * wer setMinPINLength anbietet - ohne sie darf ein Programm annehmen,
	 * es sei keiner erlaubt.
	 *
	 * Achtung, hier lag eine Falle: 0x10 ist maxRPIDsForSetMinPINLength,
	 * 0x11 ist preferredPlatformUvAttempts. Zwei Nachbarn, von denen einer
	 * eine Anzahl von Diensten und der andere eine Anzahl von Versuchen
	 * meint - nachgelesen in Abschnitt 6.4, nicht aus dem Kopf. */
	osk_cbor_put_uint(w, 0x10);
	osk_cbor_put_uint(w, OSK_CTAP_MINPIN_RP_MAX);

	osk_cbor_put_uint(w, 0x14);
	osk_cbor_put_uint(w, rk_left);

	return osk_cbor_writer_ok(w) ? CTAP2_OK : CTAP2_ERR_REQUEST_TOO_LARGE;
}

/* ---- Reset und Selection ----------------------------------------------- */

static uint8_t do_reset(osk_link_t link)
{
	/* Ueber NFC bleibt Reset gesperrt: ein Lesegeraet in der Naehe soll
	 * nicht aus Versehen alles loeschen (6.6 erlaubt das ausdruecklich). */
	if (link == OSK_LINK_NFC)
		return CTAP2_ERR_OPERATION_DENIED;

	const uint8_t up = ask_user(OSK_ASK_WIPE, NULL, NULL);
	if (up != CTAP2_OK)
		return up;

	/* Das Grundgeheimnis neu ziehen: damit sind alle abgeleiteten
	 * Schluessel auf einen Schlag wertlos, auch die ueber U2F erzeugten. */
	const osk_key_policy_t policy = {.exportable = false, .needs_touch = false};
	osk_se050_key_delete(OSK_KEY_ID_ROOT);
	if (osk_se050_key_create(OSK_KEY_ID_ROOT, OSK_CURVE_P256, &policy) != OSK_OK)
		return CTAP1_ERR_OTHER;

	/* Die PIN gehoert auch weg: nach einem Reset meldet GetInfo
	 * clientPin = false. */
	osk_se050_pin_clear();

	/* Residente Nachweise aus dem Tresor entfernen. */
	size_t count = 0;
	if (osk_store_count(0, &count) == OSK_OK) {
		for (size_t i = count; i > 0; i--) {
			osk_item_meta_t meta;
			memset(&meta, 0, sizeof(meta));
			if (osk_store_list(0, i - 1, &meta) != OSK_OK)
				continue;
			if (!meta.site)
				continue;
			if (strcmp(meta.site, OSK_RK_SITE) == 0 ||
			    strcmp(meta.site, "osk-largeblob") == 0)
				osk_store_delete(meta.id);
		}
	}

	/* Der Signaturzaehler wird NICHT zurueckgedreht. Es gibt dafuer keinen
	 * Vertrag in osk_store.h, und das ist richtig: nach einem Reset ist
	 * jeder alte Nachweis ohnehin wertlos (neues Grundgeheimnis), ein
	 * Zurueckdrehen brauchte es also nicht - koennte aber einen Dienst,
	 * der einen alten Stand noch kennt, einen Klon sehen lassen. */
	g_state.min_pin_len = OSK_PIN_MIN_DEFAULT;
	g_state.force_pin_change = false;
	g_state.ep_enabled = false;
	memset(&g_state.next_state, 0, sizeof(g_state.next_state));
	memset(&g_state.cm_enum, 0, sizeof(g_state.cm_enum));
	osk_ctap_minpin_rp_clear();
	osk_pin_init();
	return CTAP2_OK;
}

static uint8_t do_selection(void)
{
	return ask_user(OSK_ASK_LOGIN, NULL, NULL);
}

/* ---- Verteiler --------------------------------------------------------- */

osk_result_t osk_ctap_init(uint32_t extensions)
{
	memset(&g_state, 0, sizeof(g_state));
	g_state.extensions = extensions;
	g_state.min_pin_len = OSK_PIN_MIN_DEFAULT;

	const osk_result_t rc = osk_pin_init();
	if (rc != OSK_OK)
		return rc;

	g_state.ready = true;
	return OSK_OK;
}

osk_result_t osk_ctap_handle(const osk_request_t *req, osk_response_t *res)
{
	osk_cbor_writer_t w;
	uint8_t code;

	if (!req || !res || !res->data || res->cap < 1)
		return OSK_ERR_ARG;
	if (!g_state.ready)
		return OSK_ERR_INTERNAL;
	if (req->len < 1 || !req->data) {
		res->data[0] = CTAP1_ERR_INVALID_LENGTH;
		res->len = 1;
		return OSK_OK;
	}
	if (req->len - 1 > OSK_MAX_MSG) {
		res->data[0] = CTAP2_ERR_REQUEST_TOO_LARGE;
		res->len = 1;
		return OSK_OK;
	}

	const uint8_t cmd = req->data[0];
	const uint8_t *body = req->data + 1;
	const size_t body_len = req->len - 1;

	/* Antwort: erst das Statusbyte, dann CBOR. */
	osk_cbor_writer_init(&w, res->data + 1, res->cap - 1);

	switch (cmd) {
	case OSK_CTAP_CMD_MAKE_CREDENTIAL:
		code =
		    body_len ? do_make_credential(body, body_len, &w) : CTAP2_ERR_MISSING_PARAMETER;
		break;
	case OSK_CTAP_CMD_GET_ASSERTION:
		code =
		    body_len ? do_get_assertion(body, body_len, &w) : CTAP2_ERR_MISSING_PARAMETER;
		break;
	case OSK_CTAP_CMD_GET_INFO:
		code = body_len ? CTAP1_ERR_INVALID_LENGTH : do_get_info(&w);
		break;
	case OSK_CTAP_CMD_CLIENT_PIN:
		code = body_len ? osk_pin_handle(body, body_len, &w) : CTAP2_ERR_MISSING_PARAMETER;
		break;
	case OSK_CTAP_CMD_RESET:
		code = body_len ? CTAP1_ERR_INVALID_LENGTH : do_reset(req->link);
		break;
	case OSK_CTAP_CMD_GET_NEXT:
		code = body_len ? CTAP1_ERR_INVALID_LENGTH : do_get_next_assertion(&w);
		break;
	case OSK_CTAP_CMD_SELECTION:
		code = body_len ? CTAP1_ERR_INVALID_LENGTH : do_selection();
		break;
	case OSK_CTAP_CMD_CRED_MGMT:
		code = body_len ? osk_ctap_cm_handle(body, body_len, &w)
				: CTAP2_ERR_MISSING_PARAMETER;
		break;
	case OSK_CTAP_CMD_CONFIG:
		code = body_len ? osk_ctap_config_handle(body, body_len, &w)
				: CTAP2_ERR_MISSING_PARAMETER;
		break;
	case OSK_CTAP_CMD_BIO_ENROLLMENT:
		/* Abgelehnt, und das ist eine Entscheidung, keine Luecke: kein
		 * Dienst und kein Browser darf einen Finger einlernen. Das
		 * geschieht am Geraet, siehe docs/ctap.md. */
		code = CTAP1_ERR_INVALID_COMMAND;
		break;
	default:
		code = CTAP1_ERR_INVALID_COMMAND;
		break;
	}

	res->data[0] = code;
	res->len = (code == CTAP2_OK) ? 1 + w.len : 1;
	/* Nach allem ausser GetAssertion ist der gemerkte Zustand hinfaellig. */
	if (cmd != OSK_CTAP_CMD_GET_ASSERTION && cmd != OSK_CTAP_CMD_GET_NEXT)
		g_state.next_state.active = false;
	/* Dasselbe fuer die Aufzaehlung der Verwaltung: ein GetNext darf nur
	 * unmittelbar auf ein Begin oder ein GetNext folgen. Sonst liesse sich
	 * ein fremder Befehl dazwischenschieben und danach weiterzaehlen. */
	if (cmd != OSK_CTAP_CMD_CRED_MGMT) {
		g_state.cm_enum.rp_active = false;
		g_state.cm_enum.cred_active = false;
	}
	return OSK_OK;
}

osk_result_t osk_ctap_cred_count(size_t *count_out)
{
	size_t count = 0;
	size_t found = 0;

	if (!count_out)
		return OSK_ERR_ARG;
	if (osk_store_count(0, &count) != OSK_OK)
		return OSK_ERR_IO;

	for (size_t i = 0; i < count; i++) {
		osk_item_meta_t meta;
		memset(&meta, 0, sizeof(meta));
		if (osk_store_list(0, i, &meta) != OSK_OK)
			continue;
		if (meta.site && strcmp(meta.site, OSK_RK_SITE) == 0)
			found++;
	}
	*count_out = found;
	return OSK_OK;
}
