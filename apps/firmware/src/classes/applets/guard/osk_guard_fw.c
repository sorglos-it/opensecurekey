/* Befehle 14 bis 18. Siehe osk_guard_fw.h. */
#include <string.h>
#include "osk_guard_fw.h"
#include "osk_cbor.h"
#include "osk_log.h"
#include "osk_store.h"
#include "osk_ui.h"

/* Die Uhr des Geraets - dieselbe, die der CTAP-Kern benutzt. */
uint64_t osk_ctap_now_ms(void);

/* Wie lange eine Nachfrage am Display offen steht. */
#define CONFIRM_TIMEOUT_MS 30000u

/* Fragt `attest` nach? Siehe osk_guard_set_attest_confirm(). */
static bool g_attest_confirm;

void osk_guard_set_attest_confirm(bool on)
{
	g_attest_confirm = on;
}

/* --- Rahmen lesen -------------------------------------------------------
 *
 * Der Leser aus osk_cbor.h prueft jede Laenge, bevor er liest, und kennt
 * keinen dynamischen Speicher. Was hier NICHT verlangt wird, ist die
 * kanonische Schluesselreihenfolge (osk_cbor_validate): der Rechner sortiert
 * seine Schluessel nach Text, CTAP sortiert nach Laenge - "n" vor "bh". Eine
 * Nachricht deswegen abzulehnen waere eine Wand gegen den eigenen Wirt.
 * Doppelte Schluessel werden abgelehnt, und unbekannte uebersprungen. */

/* Stellt `out` auf den Wert des Schluessels `key` in der Abbildung, auf die
 * `map` zeigt. OSK_ERR_NOT_FOUND, wenn es ihn nicht gibt. */
static osk_result_t map_seek(const osk_cbor_reader_t *map, const char *key, osk_cbor_reader_t *out)
{
	osk_cbor_reader_t r = *map;
	size_t count = 0;
	const size_t key_len = strlen(key);

	if (osk_cbor_enter_map(&r, &count) != OSK_OK) {
		return OSK_ERR_FORMAT;
	}

	bool found = false;
	for (size_t i = 0; i < count; i++) {
		const char *name = NULL;
		size_t name_len = 0;
		if (osk_cbor_read_text(&r, &name, &name_len) != OSK_OK) {
			return OSK_ERR_FORMAT;
		}
		const bool hit = name_len == key_len && memcmp(name, key, key_len) == 0;
		if (hit) {
			if (found) {
				/* Zweimal derselbe Schluessel: dann steht nicht
				 * fest, welcher Wert gilt. */
				return OSK_ERR_FORMAT;
			}
			found = true;
			*out = r;
		}
		if (osk_cbor_skip(&r) != OSK_OK) {
			return OSK_ERR_FORMAT;
		}
	}
	return found ? OSK_OK : OSK_ERR_NOT_FOUND;
}

static osk_result_t field_uint(const osk_cbor_reader_t *map, const char *key, uint64_t *out)
{
	osk_cbor_reader_t r;
	osk_result_t rc = map_seek(map, key, &r);
	if (rc != OSK_OK) {
		return rc;
	}
	return osk_cbor_read_uint(&r, out) == OSK_OK ? OSK_OK : OSK_ERR_FORMAT;
}

static osk_result_t field_bytes(const osk_cbor_reader_t *map, const char *key, const uint8_t **out,
				size_t *len_out)
{
	osk_cbor_reader_t r;
	osk_result_t rc = map_seek(map, key, &r);
	if (rc != OSK_OK) {
		return rc;
	}
	return osk_cbor_read_bytes(&r, out, len_out) == OSK_OK ? OSK_OK : OSK_ERR_FORMAT;
}

static osk_result_t field_text(const osk_cbor_reader_t *map, const char *key, const char **out,
			       size_t *len_out)
{
	osk_cbor_reader_t r;
	osk_result_t rc = map_seek(map, key, &r);
	if (rc != OSK_OK) {
		return rc;
	}
	return osk_cbor_read_text(&r, out, len_out) == OSK_OK ? OSK_OK : OSK_ERR_FORMAT;
}

/* Ein Wahrheitswert, der fehlen darf: fehlt er, gilt `false`. */
static bool field_flag(const osk_cbor_reader_t *map, const char *key)
{
	osk_cbor_reader_t r;
	bool value = false;

	if (map_seek(map, key, &r) != OSK_OK) {
		return false;
	}
	if (osk_cbor_read_bool(&r, &value) != OSK_OK) {
		return false;
	}
	return value;
}

/* --- Rahmen schreiben --------------------------------------------------- */

static void reply_begin(osk_cbor_writer_t *w, osk_response_t *res, size_t data_pairs)
{
	osk_cbor_writer_init(w, res->data, res->cap);
	osk_cbor_put_map(w, 4u); /* d, r, s, v */
	osk_cbor_put_text(w, "d");
	osk_cbor_put_map(w, data_pairs);
}

static osk_result_t reply_end(osk_cbor_writer_t *w, osk_response_t *res, uint64_t seq,
			      osk_result_t result)
{
	osk_cbor_put_text(w, "r");
	osk_cbor_put_uint(w, (uint64_t)result);
	osk_cbor_put_text(w, "s");
	osk_cbor_put_uint(w, seq);
	osk_cbor_put_text(w, "v");
	osk_cbor_put_uint(w, OSK_GUARD_PROTO_VERSION);
	if (!osk_cbor_writer_ok(w)) {
		/* Die Antwort passt nicht in den Puffer des Transports. Lieber
		 * gar keine als eine abgeschnittene. */
		res->len = 0;
		return OSK_ERR_FULL;
	}
	res->len = w->len;
	return OSK_OK;
}

/* Antwort ohne Daten - der ganze Fehlerweg geht hierueber. */
static osk_result_t reply_plain(osk_response_t *res, uint64_t seq, osk_result_t result)
{
	osk_cbor_writer_t w;
	reply_begin(&w, res, 0u);
	return reply_end(&w, res, seq, result);
}

/* --- Protokoll und Freigabe --------------------------------------------- */

/* note_who schreibt einen Satz ins Protokoll und traegt dabei ein, WER
 * ihn ausgeloest hat - soweit das Geraet das wissen kann.
 *
 * Mehr als den Weg, auf dem die Anfrage hereinkam, und den Namen des
 * betroffenen Schluessels weiss es nicht: das Protokoll kennt kein Feld,
 * in dem der Rechner sagt, wer dort am Werk ist, und ein Feld, das der
 * Rechner selbst fuellt, waere ohnehin die Aussage des Verdaechtigen.
 * Darum steht hier der Kanal und nicht ein Benutzername. Was dafuer
 * noetig waere, steht als offener Punkt in docs/dongle-bau.md.
 *
 * Die Freischaltung ist der Satz, bei dem das zaehlt: wer sie einschaltet,
 * nimmt dem ganzen Rechner den Tastendruck ab. */
static void note_who(uint16_t what, const char *who, size_t who_len)
{
	osk_log_entry_t entry;
	memset(&entry, 0, sizeof(entry));
	/* Die Uhr des Geraets zaehlt ab dem Einschalten - siehe note(). */
	entry.when_utc = osk_ctap_now_ms() / 1000u;
	entry.link = OSK_LINK_USB_VENDOR;
	entry.what = what;
	if (who != NULL && who_len > 0u) {
		size_t room = sizeof(entry.asked_by) - 1u;
		size_t take = (who_len < room) ? who_len : room;
		memcpy(entry.asked_by, who, take);
		entry.asked_by[take] = '\0';
	}
	(void)osk_log_add(&entry);
}

/* grant_who baut den Eintrag "Kanal:Schluessel" fuer das Protokoll. Der
 * Kanal ist das, was das Geraet selbst gesehen hat. */
static void note_grant(const char *label, size_t label_len)
{
	char who[16u + 1u + OSK_GUARD_LABEL_MAX + 1u];
	static const char channel[] = "usb-vendor";
	size_t at = sizeof(channel) - 1u;

	memcpy(who, channel, at);
	if (label != NULL && label_len > 0u && label_len <= OSK_GUARD_LABEL_MAX) {
		who[at++] = ':';
		memcpy(who + at, label, label_len);
		at += label_len;
	}
	who[at] = '\0';
	note_who(OSK_GUARD_CMD_SESSION, who, at);
}

static void note(uint16_t what)
{
	osk_log_entry_t entry;
	memset(&entry, 0, sizeof(entry));
	/* Die Uhr des Geraets zaehlt ab dem Einschalten. Eine echte UTC-Zeit
	 * kann nur der Rechner liefern; bis das geklaert ist, steht hier die
	 * Geraetezeit - und das ist im Protokoll zu lesen, nicht zu raten. */
	entry.when_utc = osk_ctap_now_ms() / 1000u;
	entry.link = OSK_LINK_USB_VENDOR;
	entry.what = what;
	(void)osk_log_add(&entry);
}

/* Fragt am Display nach - mit dem Namen des Schluessels, damit dort steht,
 * was freigegeben wird, und nicht nur der Rechner es weiss. */
static osk_result_t confirm(osk_ask_t kind, const char *label, size_t label_len)
{
	char subject[OSK_GUARD_LABEL_MAX + 1u];
	osk_ask_req_t req;

	memset(subject, 0, sizeof(subject));
	if (label != NULL && label_len > 0u && label_len <= OSK_GUARD_LABEL_MAX) {
		memcpy(subject, label, label_len);
	}

	memset(&req, 0, sizeof(req));
	req.kind = kind;
	req.subject = subject;
	req.timeout_ms = CONFIRM_TIMEOUT_MS;
	return osk_ui_confirm(&req);
}

/* Unterschreiben und Schluesseltausch: entweder gilt eine Freischaltung,
 * oder es wird gefragt. Jede Benutzung steht im Protokoll - mit oder ohne
 * Freischaltung. */
static osk_result_t allow_use(const char *label, size_t label_len, osk_ask_t kind)
{
	if (osk_guard_grant_allows(label, label_len, osk_ctap_now_ms())) {
		return OSK_OK;
	}
	return confirm(kind, label, label_len);
}

/* --- 14 attest ---------------------------------------------------------- */

static osk_result_t do_attest(const osk_cbor_reader_t *data, osk_response_t *res, uint64_t seq)
{
	const uint8_t *challenge = NULL;
	size_t challenge_len = 0;
	uint8_t msg[OSK_GUARD_MSG_MAX];
	size_t msg_len = 0;
	osk_result_t rc;

	if (field_bytes(data, "ch", &challenge, &challenge_len) != OSK_OK) {
		return reply_plain(res, seq, OSK_ERR_ARG);
	}

	const uint8_t *bin_hash = NULL;
	size_t hash_len = 0;
	const bool has_hash = field_bytes(data, "bh", &bin_hash, &hash_len) == OSK_OK;

	/* Zwei Wege, zwei Zwecke. Welcher es ist, entscheidet allein das
	 * Vorhandensein der Programmpruefsumme - der Rechner schickt keinen
	 * Zweck mit, und es gibt kein Feld, mit dem er einen waehlen koennte. */
	if (has_hash) {
		rc = osk_guard_msg_attest(challenge, challenge_len, bin_hash, hash_len, msg,
					  sizeof(msg), &msg_len);
	} else {
		rc = osk_guard_msg_attest_raw(challenge, challenge_len, msg, sizeof(msg), &msg_len);
	}
	if (rc != OSK_OK) {
		return reply_plain(res, seq, rc);
	}

	if (g_attest_confirm) {
		rc = confirm(OSK_ASK_LOGIN, NULL, 0u);
		if (rc != OSK_OK) {
			return reply_plain(res, seq, rc);
		}
	}

	uint8_t sig[OSK_GUARD_SIG_LEN];
	uint8_t pub[OSK_GUARD_PUB_LEN];
	rc = osk_guard_keys_attest(msg, msg_len, sig, pub);
	if (rc != OSK_OK) {
		return reply_plain(res, seq, rc);
	}

	note(OSK_GUARD_CMD_ATTEST);

	osk_cbor_writer_t w;
	reply_begin(&w, res, 2u);
	osk_cbor_put_text(&w, "pub");
	osk_cbor_put_bytes(&w, pub, sizeof(pub));
	osk_cbor_put_text(&w, "sig");
	osk_cbor_put_bytes(&w, sig, sizeof(sig));
	return reply_end(&w, res, seq, OSK_OK);
}

/* --- 15 derive ---------------------------------------------------------- */

static osk_result_t do_derive(const osk_cbor_reader_t *data, osk_response_t *res, uint64_t seq)
{
	const uint8_t *info = NULL;
	size_t info_len = 0;
	uint64_t want = 0;

	if (field_bytes(data, "info", &info, &info_len) != OSK_OK ||
	    field_uint(data, "n", &want) != OSK_OK) {
		return reply_plain(res, seq, OSK_ERR_ARG);
	}
	if (want == 0u || want > OSK_GUARD_DERIVE_MAX) {
		return reply_plain(res, seq, OSK_ERR_ARG);
	}

	uint8_t key[OSK_GUARD_DERIVE_MAX];
	osk_result_t rc = osk_guard_keys_derive(info, info_len, key, (size_t)want);
	if (rc != OSK_OK) {
		return reply_plain(res, seq, rc);
	}

	note(OSK_GUARD_CMD_DERIVE);

	osk_cbor_writer_t w;
	reply_begin(&w, res, 1u);
	osk_cbor_put_text(&w, "key");
	osk_cbor_put_bytes(&w, key, (size_t)want);
	rc = reply_end(&w, res, seq, OSK_OK);
	/* Der abgeleitete Schluessel ist ein Geheimnis: er liegt jetzt in der
	 * Antwort, und im eigenen Puffer hat er nichts mehr zu suchen. */
	memset(key, 0, sizeof(key));
	return rc;
}

/* --- 16 agree ----------------------------------------------------------- */

static osk_result_t do_agree(const osk_cbor_reader_t *data, osk_response_t *res, uint64_t seq)
{
	const char *label = NULL;
	size_t label_len = 0;
	const uint8_t *peer = NULL;
	size_t peer_len = 0;

	if (field_text(data, "k", &label, &label_len) != OSK_OK ||
	    field_bytes(data, "pub", &peer, &peer_len) != OSK_OK) {
		return reply_plain(res, seq, OSK_ERR_ARG);
	}
	if (label_len == 0u || label_len > OSK_GUARD_LABEL_MAX || peer_len != OSK_GUARD_PUB_LEN) {
		return reply_plain(res, seq, OSK_ERR_ARG);
	}

	/* Der Schluessel muss es geben, BEVOR am Display gefragt wird: sonst
	 * druckt der Nutzer einen Tastendruck fuer einen Namen weg, der im
	 * Geraet gar nicht vorkommt. */
	osk_key_id_t id = 0;
	osk_result_t rc = osk_guard_node_lookup(label, label_len, OSK_GUARD_CURVE_X25519, &id);
	if (rc != OSK_OK) {
		return reply_plain(res, seq, rc);
	}

	rc = allow_use(label, label_len, OSK_ASK_DECRYPT);
	if (rc != OSK_OK) {
		return reply_plain(res, seq, rc);
	}

	uint8_t secret[OSK_GUARD_PUB_LEN];
	rc = osk_guard_keys_agree(label, label_len, peer, secret);
	if (rc != OSK_OK) {
		/* OSK_ERR_UNSUPPORTED heisst hier: dieser Baustein kann kein
		 * ECDH (im Werk auf FIPS gestellt). Das ist bewusst ein
		 * anderer Grund als OSK_ERR_DENIED - der Rechner soll sagen,
		 * welcher Anwendungsfall wegfaellt, und nicht nach einer
		 * fehlenden PIN aussehen. */
		return reply_plain(res, seq, rc);
	}

	note(OSK_GUARD_CMD_AGREE);

	osk_cbor_writer_t w;
	reply_begin(&w, res, 1u);
	osk_cbor_put_text(&w, "sec");
	osk_cbor_put_bytes(&w, secret, sizeof(secret));
	rc = reply_end(&w, res, seq, OSK_OK);
	memset(secret, 0, sizeof(secret));
	return rc;
}

/* --- 17 session --------------------------------------------------------- */

static osk_result_t session_ask(osk_response_t *res, uint64_t seq)
{
	osk_guard_grant_state_t state;
	const uint64_t now = osk_ctap_now_ms();

	osk_guard_grant_state(&state, now);

	osk_cbor_writer_t w;
	reply_begin(&w, res, 3u);
	osk_cbor_put_text(&w, "k");
	osk_cbor_put_array(&w, state.count);
	for (size_t i = 0; i < state.count; i++) {
		size_t len = 0;
		const char *label = osk_guard_grant_label(i, &len, now);
		if (label == NULL) {
			return reply_plain(res, seq, OSK_ERR_INTERNAL);
		}
		osk_cbor_put_text_len(&w, label, len);
	}
	osk_cbor_put_text(&w, "on");
	osk_cbor_put_bool(&w, state.on);
	osk_cbor_put_text(&w, "ttl");
	osk_cbor_put_uint(&w, state.ttl_left_s);
	return reply_end(&w, res, seq, OSK_OK);
}

static osk_result_t do_session(const osk_cbor_reader_t *data, const osk_request_t *req,
			       osk_response_t *res, uint64_t seq)
{
	/* "Aus der Bibliothek heraus nicht erreichbar" (docs/dongle.md) ist
	 * hier eine Regel des GERAETS, nicht ein fehlender Aufruf im Wirt:
	 * die Freischaltung gibt es nur ueber den Kanal des
	 * Verwaltungsprogramms. Eine eingebundene Bibliothek darf die Latte
	 * nicht fuer den ganzen Rechner tiefer legen - auch nicht, wenn sie
	 * den Rahmen selbst zusammenbaut und ueber einen anderen Weg
	 * hereinschickt. */
	if (req->link != OSK_LINK_USB_VENDOR) {
		return reply_plain(res, seq, OSK_ERR_DENIED);
	}

	if (field_flag(data, "ask")) {
		return session_ask(res, seq);
	}

	if (!field_flag(data, "on")) {
		/* Abschalten geht immer und ohne Nachfrage: die Latte wieder
		 * hoeher zu legen ist nie das Gefaehrliche. */
		osk_guard_grant_clear();
		note_grant(NULL, 0u);
		return reply_plain(res, seq, OSK_OK);
	}

	/* Einschalten: Namen und Laufzeit lesen. */
	osk_cbor_reader_t list;
	if (map_seek(data, "k", &list) != OSK_OK) {
		return reply_plain(res, seq, OSK_ERR_ARG);
	}
	size_t count = 0;
	if (osk_cbor_enter_array(&list, &count) != OSK_OK) {
		return reply_plain(res, seq, OSK_ERR_ARG);
	}
	if (count == 0u || count > OSK_GUARD_GRANT_MAX) {
		return reply_plain(res, seq, OSK_ERR_ARG);
	}

	const char *labels[OSK_GUARD_GRANT_MAX];
	size_t lens[OSK_GUARD_GRANT_MAX];
	for (size_t i = 0; i < count; i++) {
		if (osk_cbor_read_text(&list, &labels[i], &lens[i]) != OSK_OK) {
			return reply_plain(res, seq, OSK_ERR_ARG);
		}
		/* Nur benannte Schluessel, die es gibt. Eine Freischaltung auf
		 * einen Namen ohne Schluessel waere eine Zusage ins Leere -
		 * und sie stuende im Protokoll, als waere sie wirksam. */
		osk_key_id_t id = 0;
		if (osk_guard_node_lookup(labels[i], lens[i], OSK_GUARD_CURVE_ED25519, &id) !=
			OSK_OK &&
		    osk_guard_node_lookup(labels[i], lens[i], OSK_GUARD_CURVE_X25519, &id) !=
			OSK_OK) {
			return reply_plain(res, seq, OSK_ERR_NOT_FOUND);
		}
	}

	uint64_t ttl = 0;
	if (field_uint(data, "ttl", &ttl) != OSK_OK) {
		return reply_plain(res, seq, OSK_ERR_ARG);
	}
	if (ttl == 0u || ttl > OSK_GUARD_TTL_MAX) {
		/* Eine zu lange Laufzeit wird nicht still gekuerzt: der
		 * Betreiber glaubte sonst, er haette mehr Zeit. */
		return reply_plain(res, seq, OSK_ERR_ARG);
	}

	/* Einschalten verlangt immer eine Freigabe am Geraet - eine
	 * Freischaltung, die sich selbst verlaengert oder von einer schon
	 * laufenden gedeckt ist, gaebe es ab Werk nicht mehr aus. */
	osk_result_t rc = confirm(OSK_ASK_SIGN, labels[0], lens[0]);
	if (rc != OSK_OK) {
		return reply_plain(res, seq, rc);
	}

	rc = osk_guard_grant_set(labels, lens, count, (uint32_t)ttl, osk_ctap_now_ms());
	if (rc != OSK_OK) {
		return reply_plain(res, seq, rc);
	}

	note_grant(labels[0], lens[0]);

	osk_cbor_writer_t w;
	reply_begin(&w, res, 2u);
	osk_cbor_put_text(&w, "on");
	osk_cbor_put_bool(&w, true);
	osk_cbor_put_text(&w, "ttl");
	osk_cbor_put_uint(&w, ttl);
	return reply_end(&w, res, seq, OSK_OK);
}

/* --- 18 sign ------------------------------------------------------------ */

static osk_result_t sign_public(const osk_cbor_reader_t *data, osk_response_t *res, uint64_t seq,
				const char *label, size_t label_len)
{
	const char *curve_name = NULL;
	size_t curve_len = 0;
	osk_guard_node_curve_t curve;

	if (field_text(data, "cv", &curve_name, &curve_len) != OSK_OK) {
		return reply_plain(res, seq, OSK_ERR_ARG);
	}
	if (curve_len == strlen("ed25519") && memcmp(curve_name, "ed25519", curve_len) == 0) {
		curve = OSK_GUARD_CURVE_ED25519;
	} else if (curve_len == strlen("x25519") && memcmp(curve_name, "x25519", curve_len) == 0) {
		curve = OSK_GUARD_CURVE_X25519;
	} else {
		return reply_plain(res, seq, OSK_ERR_ARG);
	}

	/* Ein oeffentlicher Schluessel ist kein Geheimnis - kein Tastendruck.
	 * Dasselbe Argument wie bei osk_store_public(): wer fuer jeden Namen
	 * einen Tastendruck verlangt, bringt den Nutzer dazu, sie
	 * wegzudruecken. */
	uint8_t pub[OSK_GUARD_PUB_LEN];
	osk_result_t rc = osk_guard_keys_node_public(label, label_len, curve, pub);
	if (rc != OSK_OK) {
		return reply_plain(res, seq, rc);
	}

	osk_cbor_writer_t w;
	reply_begin(&w, res, 1u);
	osk_cbor_put_text(&w, "pub");
	osk_cbor_put_bytes(&w, pub, sizeof(pub));
	return reply_end(&w, res, seq, OSK_OK);
}

static osk_result_t do_sign(const osk_cbor_reader_t *data, osk_response_t *res, uint64_t seq)
{
	const char *label = NULL;
	size_t label_len = 0;

	if (field_text(data, "k", &label, &label_len) != OSK_OK) {
		return reply_plain(res, seq, OSK_ERR_ARG);
	}
	if (label_len == 0u || label_len > OSK_GUARD_LABEL_MAX) {
		return reply_plain(res, seq, OSK_ERR_ARG);
	}

	if (field_flag(data, "pub")) {
		return sign_public(data, res, seq, label, label_len);
	}

	const uint8_t *job_hash = NULL;
	size_t hash_len = 0;
	if (field_bytes(data, "h", &job_hash, &hash_len) != OSK_OK) {
		return reply_plain(res, seq, OSK_ERR_ARG);
	}

	/* Der Zweck "osk-guard/sign/1" kommt hier davor, im Geraet - und er
	 * ist ein anderer als der von `attest`. Darum kann keine Antwort des
	 * einen Befehls im anderen gelten. */
	uint8_t msg[OSK_GUARD_MSG_MAX];
	size_t msg_len = 0;
	osk_result_t rc =
	    osk_guard_msg_sign(label, label_len, job_hash, hash_len, msg, sizeof(msg), &msg_len);
	if (rc != OSK_OK) {
		return reply_plain(res, seq, rc);
	}

	osk_key_id_t id = 0;
	rc = osk_guard_node_lookup(label, label_len, OSK_GUARD_CURVE_ED25519, &id);
	if (rc != OSK_OK) {
		return reply_plain(res, seq, rc);
	}

	rc = allow_use(label, label_len, OSK_ASK_SIGN);
	if (rc != OSK_OK) {
		/* Kein Mensch am Geraet und keine Freischaltung: genau der
		 * Fall, in dem ein Server stillsteht. Das ist das richtige
		 * Verhalten, kein Fehler (docs/dongle.md). */
		return reply_plain(res, seq, rc);
	}

	uint8_t sig[OSK_GUARD_SIG_LEN];
	rc = osk_guard_keys_node_sign(label, label_len, msg, msg_len, sig);
	if (rc != OSK_OK) {
		return reply_plain(res, seq, rc);
	}

	note(OSK_GUARD_CMD_SIGN);

	osk_cbor_writer_t w;
	reply_begin(&w, res, 1u);
	osk_cbor_put_text(&w, "sig");
	osk_cbor_put_bytes(&w, sig, sizeof(sig));
	return reply_end(&w, res, seq, OSK_OK);
}

/* --- Rahmen ------------------------------------------------------------- */

osk_result_t osk_guard_handle(const osk_request_t *req, osk_response_t *res)
{
	if (req == NULL || res == NULL || req->data == NULL || res->data == NULL) {
		return OSK_ERR_ARG;
	}
	if (req->len == 0u || res->cap == 0u) {
		return OSK_ERR_ARG;
	}
	res->len = 0;

	osk_cbor_reader_t top;
	osk_cbor_reader_init(&top, req->data, req->len);

	osk_cbor_type_t type;
	if (osk_cbor_peek_type(&top, &type) != OSK_OK || type != OSK_CBOR_MAP) {
		return reply_plain(res, 0u, OSK_ERR_FORMAT);
	}

	uint64_t seq = 0;
	(void)field_uint(&top, "s", &seq); /* fehlt sie, ist sie 0 */

	uint64_t version = 0;
	if (field_uint(&top, "v", &version) != OSK_OK) {
		return reply_plain(res, seq, OSK_ERR_FORMAT);
	}
	if (version != OSK_GUARD_PROTO_VERSION) {
		return reply_plain(res, seq, OSK_ERR_UNSUPPORTED);
	}

	uint64_t cmd = 0;
	if (field_uint(&top, "c", &cmd) != OSK_OK) {
		return reply_plain(res, seq, OSK_ERR_FORMAT);
	}

	/* `d` darf fehlen. Dann zeigt der Leser auf eine leere Abbildung,
	 * und jedes Pflichtfeld fehlt - was dieselbe Antwort gibt wie ein
	 * leeres `d`. */
	static const uint8_t empty_map[] = {0xa0};
	osk_cbor_reader_t data;
	if (map_seek(&top, "d", &data) != OSK_OK) {
		osk_cbor_reader_init(&data, empty_map, sizeof(empty_map));
	}
	if (osk_cbor_peek_type(&data, &type) != OSK_OK || type != OSK_CBOR_MAP) {
		return reply_plain(res, seq, OSK_ERR_FORMAT);
	}

	/* Ein Befehl, den dieses Applet nicht hat, bekommt seinen eigenen
	 * Grund - auch bei geschlossenem Tresor. Sonst sagte die Antwort
	 * "gesperrt" zu einem Befehl, den es hier gar nicht gibt, und der
	 * Rechner suchte an der falschen Stelle. */
	if (cmd < OSK_GUARD_CMD_ATTEST || cmd > OSK_GUARD_CMD_SIGN) {
		/* Die Befehle 1 bis 13 gehoeren einem anderen Bearbeiter. */
		return reply_plain(res, seq, OSK_ERR_UNSUPPORTED);
	}

	/* `attest` antwortet auch bei geschlossenem Tresor: ein geschuetztes
	 * Programm fragt "steckst du?", und dafuer braucht es kein Geheimnis.
	 * Alles andere benutzt Schluessel und verlangt den offenen Tresor. */
	if (cmd != OSK_GUARD_CMD_ATTEST && !osk_store_is_open()) {
		return reply_plain(res, seq, OSK_ERR_LOCKED);
	}

	switch (cmd) {
	case OSK_GUARD_CMD_ATTEST:
		return do_attest(&data, res, seq);
	case OSK_GUARD_CMD_DERIVE:
		return do_derive(&data, res, seq);
	case OSK_GUARD_CMD_AGREE:
		return do_agree(&data, res, seq);
	case OSK_GUARD_CMD_SESSION:
		return do_session(&data, req, res, seq);
	case OSK_GUARD_CMD_SIGN:
		return do_sign(&data, res, seq);
	default:
		/* Hierher kommt nichts mehr - die Spanne ist oben geprueft. */
		return reply_plain(res, seq, OSK_ERR_UNSUPPORTED);
	}
}

osk_result_t osk_guard_init(void)
{
	osk_guard_grant_clear(); /* ab Werk aus - und nach jedem Neustart */
	osk_guard_keys_reset();
	g_attest_confirm = false;
	return osk_transport_register(OSK_LINK_USB_VENDOR, osk_guard_handle);
}
