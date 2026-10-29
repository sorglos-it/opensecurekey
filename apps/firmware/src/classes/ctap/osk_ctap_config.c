/* authenticatorConfig (0x0D). Siehe osk_ctap_config.h.
 * Spezifikation: CTAP 2.2 (14.07.2025), Abschnitt 6.11. */
#include <string.h>
#include "osk_ctap_config.h"
#include "osk_ctap_internal.h"
#include "osk_ctap_ext.h"
#include "osk_pin.h"
#include "osk_crypto.h"

/* Schluessel der Anfrage (6.11, Tabelle) */
#define KEY_SUBCOMMAND 0x01
#define KEY_SUBCOMMAND_PARAMS 0x02
#define KEY_PIN_PROTOCOL 0x03
#define KEY_PIN_AUTH 0x04

/* Unterbefehle */
#define SUB_ENABLE_EP 0x01
#define SUB_TOGGLE_ALWAYS_UV 0x02
#define SUB_SET_MIN_PIN 0x03
#define SUB_VENDOR_PROTOTYPE 0xff

/* Schluessel von subCommandParams bei setMinPINLength */
#define PARAM_NEW_MIN_LEN 0x01
#define PARAM_RP_IDS 0x02
#define PARAM_FORCE_CHANGE 0x03
#define PARAM_COMPLEXITY 0x04

typedef struct {
	uint64_t sub;
	bool have_sub;
	const uint8_t *params; /* rohe Bytes, genau wie in der Nachricht */
	size_t params_len;
	uint64_t protocol;
	bool have_protocol;
	const uint8_t *pin_auth;
	size_t pin_auth_len;
	bool have_pin_auth;
} cfg_req_t;

static uint8_t parse_request(const uint8_t *cbor, size_t len, cfg_req_t *q)
{
	osk_cbor_reader_t r;
	size_t count = 0;

	memset(q, 0, sizeof(*q));

	if (osk_cbor_validate(cbor, len) != OSK_OK)
		return CTAP2_ERR_INVALID_CBOR;

	osk_cbor_reader_init(&r, cbor, len);
	if (osk_cbor_enter_map(&r, &count) != OSK_OK)
		return CTAP2_ERR_INVALID_CBOR;

	for (size_t i = 0; i < count; i++) {
		uint64_t key = 0;
		if (osk_cbor_read_uint(&r, &key) != OSK_OK)
			return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
		switch (key) {
		case KEY_SUBCOMMAND:
			if (osk_cbor_read_uint(&r, &q->sub) != OSK_OK)
				return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
			q->have_sub = true;
			break;
		case KEY_SUBCOMMAND_PARAMS: {
			/* Der MAC geht ueber die Bytes, wie sie ANKAMEN, nicht
			 * ueber eine neu geschriebene Fassung. Darum wird hier
			 * der Ausschnitt gemerkt und nicht nachgebaut: zwei
			 * Kodierungen derselben Abbildung haetten denselben
			 * Sinn und verschiedene Bytes, und die Pruefung des MAC
			 * wuerde scheitern. */
			const size_t start = r.pos;
			if (osk_cbor_skip(&r) != OSK_OK)
				return CTAP2_ERR_INVALID_CBOR;
			q->params = cbor + start;
			q->params_len = r.pos - start;
			break;
		}
		case KEY_PIN_PROTOCOL:
			if (osk_cbor_read_uint(&r, &q->protocol) != OSK_OK)
				return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
			q->have_protocol = true;
			break;
		case KEY_PIN_AUTH:
			if (osk_cbor_read_bytes(&r, &q->pin_auth, &q->pin_auth_len) != OSK_OK)
				return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
			q->have_pin_auth = true;
			break;
		default:
			if (osk_cbor_skip(&r) != OSK_OK)
				return CTAP2_ERR_INVALID_CBOR;
			break;
		}
	}
	osk_cbor_leave(&r);

	if (!q->have_sub)
		return CTAP2_ERR_MISSING_PARAMETER;
	return CTAP2_OK;
}

/* Freigabe nach 6.11, Schritt 4. */
static uint8_t check_auth(const cfg_req_t *q)
{
	/* message = 32 x 0xff || 0x0d || subCommand || subCommandParams
	 *
	 * Die 32 Fuellbytes und das Befehlsbyte sind der Unterschied zur
	 * Verwaltung (0x0A), die OHNE sie rechnet. Zwei Befehle, zwei
	 * verschiedene Nachrichten - wer das verwechselt, baut einen MAC, der
	 * sich von einem Befehl auf den anderen uebertragen laesst. */
	uint8_t msg[32 + 1 + 1 + 256];
	size_t at = 0;

	if (!q->have_pin_auth)
		return CTAP2_ERR_PUAT_REQUIRED;
	if (!q->have_protocol)
		return CTAP2_ERR_MISSING_PARAMETER;
	if (!osk_pin_protocol_supported(q->protocol))
		return CTAP1_ERR_INVALID_PARAMETER;
	if (q->params_len > sizeof(msg) - 34)
		return CTAP1_ERR_INVALID_PARAMETER;

	memset(msg, 0xff, 32);
	at = 32;
	msg[at++] = OSK_CTAP_CMD_CONFIG;
	msg[at++] = (uint8_t)q->sub;
	if (q->params_len) {
		memcpy(msg + at, q->params, q->params_len);
		at += q->params_len;
	}

	if (!osk_pin_verify_token(q->protocol, msg, at, q->pin_auth, q->pin_auth_len))
		return CTAP2_ERR_PIN_AUTH_INVALID;
	if (!osk_pin_has_permission(OSK_PERM_ACFG))
		return CTAP2_ERR_PIN_AUTH_INVALID;
	return CTAP2_OK;
}

/* 6.11.3 enableEnterpriseAttestation. Einmal ein - es gibt kein Aus. Das
 * Zurueckstellen geht nur ueber authenticatorReset, der ohnehin alles
 * loescht (7.1.3). */
static uint8_t do_enable_ep(void)
{
	osk_ctap_state()->ep_enabled = true;
	return CTAP2_OK;
}

typedef struct {
	uint64_t new_min;
	bool have_new_min;
	bool force_change;
	bool complexity;
	const char *rp_ids[OSK_CTAP_MINPIN_RP_MAX];
	size_t rp_id_lens[OSK_CTAP_MINPIN_RP_MAX];
	size_t rp_id_count;
	bool rp_ids_present;
	bool too_many_rp_ids;
} minpin_params_t;

static uint8_t parse_minpin_params(const cfg_req_t *q, minpin_params_t *p)
{
	osk_cbor_reader_t r;
	size_t count = 0;

	memset(p, 0, sizeof(*p));
	if (!q->params || q->params_len == 0)
		return CTAP2_OK; /* alles weggelassen ist erlaubt (Schritt 1) */

	osk_cbor_reader_init(&r, q->params, q->params_len);
	if (osk_cbor_enter_map(&r, &count) != OSK_OK)
		return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;

	for (size_t i = 0; i < count; i++) {
		uint64_t key = 0;
		if (osk_cbor_read_uint(&r, &key) != OSK_OK)
			return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
		switch (key) {
		case PARAM_NEW_MIN_LEN:
			if (osk_cbor_read_uint(&r, &p->new_min) != OSK_OK)
				return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
			p->have_new_min = true;
			break;
		case PARAM_RP_IDS: {
			size_t n = 0;
			if (osk_cbor_enter_array(&r, &n) != OSK_OK)
				return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
			p->rp_ids_present = true;
			for (size_t j = 0; j < n; j++) {
				const char *v = NULL;
				size_t vlen = 0;
				if (osk_cbor_read_text(&r, &v, &vlen) != OSK_OK)
					return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
				if (p->rp_id_count < OSK_CTAP_MINPIN_RP_MAX) {
					p->rp_ids[p->rp_id_count] = v;
					p->rp_id_lens[p->rp_id_count] = vlen;
					p->rp_id_count++;
				} else {
					p->too_many_rp_ids = true;
				}
			}
			osk_cbor_leave(&r);
			break;
		}
		case PARAM_FORCE_CHANGE:
			if (osk_cbor_read_bool(&r, &p->force_change) != OSK_OK)
				return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
			break;
		case PARAM_COMPLEXITY:
			if (osk_cbor_read_bool(&r, &p->complexity) != OSK_OK)
				return CTAP2_ERR_CBOR_UNEXPECTED_TYPE;
			break;
		default:
			if (osk_cbor_skip(&r) != OSK_OK)
				return CTAP2_ERR_INVALID_CBOR;
			break;
		}
	}
	osk_cbor_leave(&r);
	return CTAP2_OK;
}

/* 6.11.4 setMinPINLength. Die Schrittnummern stehen an den Zeilen, damit
 * beim Nachlesen keiner uebersprungen wird. */
static uint8_t do_set_min_pin(const cfg_req_t *q)
{
	osk_ctap_state_t *st = osk_ctap_state();
	osk_pin_state_t *pin = osk_pin_state();
	minpin_params_t p;
	bool force = false;

	const uint8_t perr = parse_minpin_params(q, &p);
	if (perr != CTAP2_OK)
		return perr;

	/* Schritt 1: fehlt die neue Laenge, bleibt die alte. */
	if (p.have_new_min && p.new_min > OSK_PIN_MAX_CODE_POINTS)
		return CTAP1_ERR_INVALID_PARAMETER;
	const uint8_t new_min = p.have_new_min ? (uint8_t)p.new_min : st->min_pin_len;

	/* Schritt 3: nur hoch, nie herunter. Die Spezifikation sagt dazu:
	 * Mindestlaengen duerfen nur steigen, nicht sinken. */
	if (new_min < st->min_pin_len)
		return CTAP2_ERR_PIN_POLICY_VIOLATION;

	/* Schritt 8a: zu viele Dienste in der Liste. Die Spezifikation
	 * verlangt hier "einen Fehler", nennt aber keinen - CTAP1_ERR_INVALID_
	 * PARAMETER ist darum UNSERE Wahl, nicht ihre: die Liste ist ein
	 * Parameter der Anfrage. Die Obergrenze meldet GetInfo unter 0x10
	 * (maxRPIDsForSetMinPINLength), damit kein Programm raten muss. */
	if (p.too_many_rp_ids)
		return CTAP1_ERR_INVALID_PARAMETER;

	/* Schritt 4: erzwungener Wechsel verlangt eine gesetzte PIN. */
	if (p.force_change) {
		if (!pin->pin_set)
			return CTAP2_ERR_PIN_NOT_SET;
		force = true;
	}

	/* Schritt 5: pinComplexityPolicy melden wir nicht und koennen sie
	 * nicht. Ein Programm, das sie verlangt, bekommt darum einen Fehler
	 * statt ein stilles Ja. */
	if (p.complexity)
		return CTAP1_ERR_INVALID_PARAMETER;

	/* Ab hier wird geaendert; vorher ist nichts angefasst worden, damit
	 * ein Fehler keinen halben Zustand hinterlaesst. */
	st->min_pin_len = new_min;

	/* Schritt 6: ist die vorhandene PIN jetzt zu kurz, muss sie gewechselt
	 * werden. Das ist KEIN Fehler dieses Befehls. */
	if (pin->pin_set && pin->pin_code_points != 0 && pin->pin_code_points < new_min)
		force = true;

	/* Schritt 8b/8c: die Liste ersetzt die bisherige. Eine ab Werk
	 * eingebaute, unveraenderliche Liste gibt es bei uns nicht, also sind
	 * "ersetzen" und "hinzufuegen" dasselbe. */
	if (p.rp_ids_present && p.rp_id_count > 0) {
		char name[OSK_MAX_RP_ID];
		osk_ctap_minpin_rp_clear();
		for (size_t i = 0; i < p.rp_id_count; i++) {
			if (p.rp_id_lens[i] == 0 || p.rp_id_lens[i] >= sizeof(name)) {
				osk_ctap_minpin_rp_clear();
				return CTAP1_ERR_INVALID_PARAMETER;
			}
			memcpy(name, p.rp_ids[i], p.rp_id_lens[i]);
			name[p.rp_id_lens[i]] = 0;
			/* Schritt 8d */
			if (osk_ctap_minpin_rp_add(name) != OSK_OK) {
				osk_ctap_minpin_rp_clear();
				return CTAP2_ERR_KEY_STORE_FULL;
			}
		}
	}

	/* Schritt 7: wird ein Wechsel erzwungen, sind alle Rechte und alle
	 * Token hinfaellig. Sonst arbeitete ein Programm mit dem alten Token
	 * weiter, als waere nichts gewesen. */
	if (force) {
		st->force_pin_change = true;
		osk_pin_stop_using();
		osk_pin_reset_token();
	}

	return CTAP2_OK;
}

uint8_t osk_ctap_config_handle(const uint8_t *cbor, size_t len, osk_cbor_writer_t *w)
{
	cfg_req_t q;

	(void)w; /* keiner der gebauten Unterbefehle antwortet mit Inhalt */

	const uint8_t perr = parse_request(cbor, len, &q);
	if (perr != CTAP2_OK)
		return perr;

	/* Schritt 2: nicht gebaute Unterbefehle. toggleAlwaysUv (0x02) steht
	 * hier mit einem EIGENEN Wert, damit ein Programm den Unterschied
	 * sieht: der Unterbefehl ist bekannt und absichtlich nicht da
	 * (Begruendung in osk_ctap_config.h), waehrend 0xff
	 * (vendorPrototype) und alles andere unbekannt ist. */
	if (q.sub == SUB_TOGGLE_ALWAYS_UV)
		return CTAP2_ERR_UNSUPPORTED_OPTION;
	if (q.sub != SUB_ENABLE_EP && q.sub != SUB_SET_MIN_PIN)
		return CTAP1_ERR_INVALID_PARAMETER;

	/* Ohne PIN gibt es kein Token und damit keine Konfiguration. */
	if (!osk_pin_state()->pin_set)
		return CTAP2_ERR_PIN_NOT_SET;

	const uint8_t aerr = check_auth(&q);
	if (aerr != CTAP2_OK)
		return aerr;

	switch (q.sub) {
	case SUB_ENABLE_EP:
		return do_enable_ep();
	case SUB_SET_MIN_PIN:
		return do_set_min_pin(&q);
	default:
		return CTAP2_ERR_INVALID_SUBCOMMAND;
	}
}
