/* Serverbetrieb: agree (16), session (17), sign (18). */
#include "test_util.h"
#include "mocks.h"
#include "guard_helper.h"
#include "osk_guard_keys.h"
#include "osk_ui.h"

#define SIGN_LABEL "prod-eu-1"
#define AGREE_LABEL "link-eu-1"

static void setup(void)
{
	mock_reset_all();
	mock_se050_setup_guard();
	osk_guard_grant_clear();
	osk_guard_keys_reset();
	T_EQ("Unterschreibschluessel eintragen",
	     osk_guard_node_register(SIGN_LABEL, strlen(SIGN_LABEL), OSK_GUARD_CURVE_ED25519),
	     OSK_OK);
	T_EQ("Tauschschluessel eintragen",
	     osk_guard_node_register(AGREE_LABEL, strlen(AGREE_LABEL), OSK_GUARD_CURVE_X25519),
	     OSK_OK);
}

static size_t build_sign(gh_frame_t *frame, const char *label, const uint8_t *hash, size_t len)
{
	gh_begin(frame, OSK_GUARD_CMD_SIGN, 1u, 2u);
	osk_cbor_put_text(&frame->w, "k");
	osk_cbor_put_text(&frame->w, label);
	osk_cbor_put_text(&frame->w, "h");
	osk_cbor_put_bytes(&frame->w, hash, len);
	return gh_end(frame);
}

static size_t build_session_on(gh_frame_t *frame, const char *const *labels, size_t count,
			       uint64_t ttl)
{
	gh_begin(frame, OSK_GUARD_CMD_SESSION, 2u, 3u);
	osk_cbor_put_text(&frame->w, "on");
	osk_cbor_put_bool(&frame->w, true);
	osk_cbor_put_text(&frame->w, "k");
	osk_cbor_put_array(&frame->w, count);
	for (size_t i = 0; i < count; i++) {
		osk_cbor_put_text(&frame->w, labels[i]);
	}
	osk_cbor_put_text(&frame->w, "ttl");
	osk_cbor_put_uint(&frame->w, ttl);
	return gh_end(frame);
}

/* --- 18 sign ------------------------------------------------------------ */

static void test_sign(void)
{
	gh_frame_t frame;
	gh_reply_t reply;
	uint8_t hash[OSK_GUARD_HASH_LEN];

	setup();
	memset(hash, 0x71, sizeof(hash));

	size_t len = build_sign(&frame, SIGN_LABEL, hash, sizeof(hash));
	T_EQ("sign laeuft", gh_call(frame.buf, len, OSK_LINK_USB_VENDOR, &reply), OSK_OK);
	T_EQ("sign: ok", reply.result, OSK_OK);

	const uint8_t *sig = NULL;
	size_t sig_len = 0;
	T_TRUE("sign: sig da", gh_bytes(&reply.data, "sig", &sig, &sig_len));
	T_EQ("sign: 64 Byte", sig_len, OSK_GUARD_SIG_LEN);

	/* Ohne Freischaltung wurde am Display gefragt - und dort stand der
	 * Name des Schluessels. */
	T_EQ("sign fragt", mock_ui_confirm_count(), 1u);
	T_EQ("sign: Anlass unterschreiben", mock_ui_last_kind(), OSK_ASK_SIGN);
	T_TRUE("sign: Name am Display", strcmp(mock_ui_last_subject(), SIGN_LABEL) == 0);
	T_EQ("sign protokolliert", mock_log_count(), 1u);
	T_EQ("sign: Anlass 18", mock_log_what(0u), OSK_GUARD_CMD_SIGN);

	/* Ein unbekannter Name bekommt keinen Schluessel - und keine
	 * Nachfrage, die der Nutzer fuer nichts wegdrueckt. */
	const unsigned before = mock_ui_confirm_count();
	len = build_sign(&frame, "gibt-es-nicht", hash, sizeof(hash));
	T_EQ("unbekannter Name", gh_call(frame.buf, len, OSK_LINK_USB_VENDOR, &reply), OSK_OK);
	T_EQ("unbekannter Name: Grund", reply.result, OSK_ERR_NOT_FOUND);
	T_EQ("unbekannter Name fragt nicht", mock_ui_confirm_count(), before);

	/* Ein Tauschschluessel unterschreibt nicht. */
	len = build_sign(&frame, AGREE_LABEL, hash, sizeof(hash));
	T_EQ("falsche Kurve", gh_call(frame.buf, len, OSK_LINK_USB_VENDOR, &reply), OSK_OK);
	T_EQ("falsche Kurve: Grund", reply.result, OSK_ERR_NOT_FOUND);

	/* Krumme Pruefsumme. */
	len = build_sign(&frame, SIGN_LABEL, hash, 31u);
	T_EQ("kurze Pruefsumme", gh_call(frame.buf, len, OSK_LINK_USB_VENDOR, &reply), OSK_OK);
	T_EQ("kurze Pruefsumme: Grund", reply.result, OSK_ERR_ARG);
}

/* Niemand am Geraet, keine Freischaltung: der Server steht still. Das ist
 * das richtige Verhalten (docs/dongle.md), kein Fehler. */
static void test_sign_without_human(void)
{
	gh_frame_t frame;
	gh_reply_t reply;
	uint8_t hash[OSK_GUARD_HASH_LEN];

	setup();
	memset(hash, 0x72, sizeof(hash));
	mock_ui_set_confirm(OSK_ERR_TIMEOUT); /* niemand drueckt */

	const size_t len = build_sign(&frame, SIGN_LABEL, hash, sizeof(hash));
	T_EQ("sign ohne Mensch", gh_call(frame.buf, len, OSK_LINK_USB_VENDOR, &reply), OSK_OK);
	T_EQ("sign ohne Mensch: Grund", reply.result, OSK_ERR_TIMEOUT);
	T_EQ("nichts unterschrieben, nichts im Protokoll", mock_log_count(), 0u);
}

/* Der oeffentliche Teil kostet keinen Tastendruck. */
static void test_sign_public(void)
{
	gh_frame_t frame;
	gh_reply_t reply;

	setup();

	gh_begin(&frame, OSK_GUARD_CMD_SIGN, 3u, 3u);
	osk_cbor_put_text(&frame.w, "k");
	osk_cbor_put_text(&frame.w, SIGN_LABEL);
	osk_cbor_put_text(&frame.w, "pub");
	osk_cbor_put_bool(&frame.w, true);
	osk_cbor_put_text(&frame.w, "cv");
	osk_cbor_put_text(&frame.w, "ed25519");
	T_EQ("pub laeuft", gh_call(frame.buf, gh_end(&frame), OSK_LINK_USB_VENDOR, &reply), OSK_OK);
	T_EQ("pub: ok", reply.result, OSK_OK);

	const uint8_t *pub = NULL;
	size_t pub_len = 0;
	T_TRUE("pub da", gh_bytes(&reply.data, "pub", &pub, &pub_len));
	T_EQ("pub 32 Byte", pub_len, OSK_GUARD_PUB_LEN);
	T_EQ("pub fragt nicht", mock_ui_confirm_count(), 0u);

	/* Unbekannte Kurve. */
	gh_begin(&frame, OSK_GUARD_CMD_SIGN, 4u, 3u);
	osk_cbor_put_text(&frame.w, "k");
	osk_cbor_put_text(&frame.w, SIGN_LABEL);
	osk_cbor_put_text(&frame.w, "pub");
	osk_cbor_put_bool(&frame.w, true);
	osk_cbor_put_text(&frame.w, "cv");
	osk_cbor_put_text(&frame.w, "p256");
	T_EQ("fremde Kurve", gh_call(frame.buf, gh_end(&frame), OSK_LINK_USB_VENDOR, &reply),
	     OSK_OK);
	T_EQ("fremde Kurve: Grund", reply.result, OSK_ERR_ARG);
}

/* --- 16 agree ----------------------------------------------------------- */

static void test_agree(void)
{
	gh_frame_t frame;
	gh_reply_t reply;
	uint8_t peer[OSK_GUARD_PUB_LEN];

	setup();
	memset(peer, 0x81, sizeof(peer));

	gh_begin(&frame, OSK_GUARD_CMD_AGREE, 5u, 2u);
	osk_cbor_put_text(&frame.w, "k");
	osk_cbor_put_text(&frame.w, AGREE_LABEL);
	osk_cbor_put_text(&frame.w, "pub");
	osk_cbor_put_bytes(&frame.w, peer, sizeof(peer));
	T_EQ("agree laeuft", gh_call(frame.buf, gh_end(&frame), OSK_LINK_USB_VENDOR, &reply),
	     OSK_OK);
	T_EQ("agree: ok", reply.result, OSK_OK);

	const uint8_t *sec = NULL;
	size_t sec_len = 0;
	T_TRUE("agree: sec da", gh_bytes(&reply.data, "sec", &sec, &sec_len));
	T_EQ("agree: 32 Byte", sec_len, OSK_GUARD_PUB_LEN);
	T_EQ("agree fragt", mock_ui_confirm_count(), 1u);

	/* Krumme Laenge des fremden Schluessels. */
	gh_begin(&frame, OSK_GUARD_CMD_AGREE, 6u, 2u);
	osk_cbor_put_text(&frame.w, "k");
	osk_cbor_put_text(&frame.w, AGREE_LABEL);
	osk_cbor_put_text(&frame.w, "pub");
	osk_cbor_put_bytes(&frame.w, peer, 31u);
	T_EQ("kurzer Fremdschluessel",
	     gh_call(frame.buf, gh_end(&frame), OSK_LINK_USB_VENDOR, &reply), OSK_OK);
	T_EQ("kurzer Fremdschluessel: Grund", reply.result, OSK_ERR_ARG);
}

/* Ein geliefertes Stueck, das im Werk auf FIPS gestellt wurde: kein ECDH.
 * Der Grund muss OSK_ERR_UNSUPPORTED sein - nicht OSK_ERR_DENIED, das nach
 * fehlender PIN aussieht. Und unterschreiben geht weiter. */
static void test_agree_no_ecdh(void)
{
	gh_frame_t frame;
	gh_reply_t reply;
	uint8_t peer[OSK_GUARD_PUB_LEN];
	uint8_t hash[OSK_GUARD_HASH_LEN];

	setup();
	memset(peer, 0x82, sizeof(peer));
	memset(hash, 0x83, sizeof(hash));
	mock_se050_set_no_ecdh(true);

	gh_begin(&frame, OSK_GUARD_CMD_AGREE, 7u, 2u);
	osk_cbor_put_text(&frame.w, "k");
	osk_cbor_put_text(&frame.w, AGREE_LABEL);
	osk_cbor_put_text(&frame.w, "pub");
	osk_cbor_put_bytes(&frame.w, peer, sizeof(peer));
	T_EQ("agree ohne ECDH", gh_call(frame.buf, gh_end(&frame), OSK_LINK_USB_VENDOR, &reply),
	     OSK_OK);
	T_EQ("agree ohne ECDH: nicht unterstuetzt", reply.result, OSK_ERR_UNSUPPORTED);
	T_TRUE("und gerade NICHT 'Freigabe fehlt'", reply.result != OSK_ERR_DENIED);
	const uint8_t *sec = NULL;
	size_t sec_len = 0;
	T_TRUE("kein Geheimnis in der Antwort", !gh_bytes(&reply.data, "sec", &sec, &sec_len));

	/* Unterschreiben ist davon nicht betroffen. */
	const size_t len = build_sign(&frame, SIGN_LABEL, hash, sizeof(hash));
	T_EQ("sign ohne ECDH", gh_call(frame.buf, len, OSK_LINK_USB_VENDOR, &reply), OSK_OK);
	T_EQ("sign ohne ECDH: ok", reply.result, OSK_OK);
}

/* --- 17 session --------------------------------------------------------- */

static void test_session(void)
{
	gh_frame_t frame;
	gh_reply_t reply;
	uint8_t hash[OSK_GUARD_HASH_LEN];
	const char *labels[1] = {SIGN_LABEL};

	setup();
	memset(hash, 0x91, sizeof(hash));

	/* Ab Werk aus: ohne Freischaltung wird gefragt. */
	size_t len = build_sign(&frame, SIGN_LABEL, hash, sizeof(hash));
	T_EQ("vorher", gh_call(frame.buf, len, OSK_LINK_USB_VENDOR, &reply), OSK_OK);
	T_EQ("vorher wird gefragt", mock_ui_confirm_count(), 1u);

	/* Einschalten verlangt eine Freigabe am Geraet. */
	len = build_session_on(&frame, labels, 1u, 3600u);
	T_EQ("einschalten", gh_call(frame.buf, len, OSK_LINK_USB_VENDOR, &reply), OSK_OK);
	T_EQ("einschalten: ok", reply.result, OSK_OK);
	T_EQ("einschalten fragt", mock_ui_confirm_count(), 2u);

	/* Jetzt unterschreibt das Geraet ohne Tastendruck - genau das ist der
	 * Serverbetrieb. */
	const unsigned asked = mock_ui_confirm_count();
	len = build_sign(&frame, SIGN_LABEL, hash, sizeof(hash));
	T_EQ("mit Freischaltung", gh_call(frame.buf, len, OSK_LINK_USB_VENDOR, &reply), OSK_OK);
	T_EQ("mit Freischaltung: ok", reply.result, OSK_OK);
	T_EQ("kein Tastendruck", mock_ui_confirm_count(), asked);
	/* Trotzdem im Protokoll: attest, sign, session, sign. */
	T_EQ("Freischaltung und Benutzung stehen im Protokoll", mock_log_count(), 3u);

	/* Nur die benannten Schluessel. Ein anderer fragt weiter nach. */
	T_EQ("zweiten Namen eintragen",
	     osk_guard_node_register("prod-us-1", 9u, OSK_GUARD_CURVE_ED25519), OSK_OK);
	len = build_sign(&frame, "prod-us-1", hash, sizeof(hash));
	T_EQ("anderer Name", gh_call(frame.buf, len, OSK_LINK_USB_VENDOR, &reply), OSK_OK);
	T_EQ("anderer Name: ok nach Nachfrage", reply.result, OSK_OK);
	T_EQ("anderer Name fragt", mock_ui_confirm_count(), asked + 1u);

	/* Der Zustand laesst sich abfragen. */
	gh_begin(&frame, OSK_GUARD_CMD_SESSION, 9u, 1u);
	osk_cbor_put_text(&frame.w, "ask");
	osk_cbor_put_bool(&frame.w, true);
	T_EQ("fragen", gh_call(frame.buf, gh_end(&frame), OSK_LINK_USB_VENDOR, &reply), OSK_OK);
	T_EQ("fragen: ok", reply.result, OSK_OK);
	bool on = false;
	uint64_t ttl = 0;
	T_TRUE("Zustand: on", gh_bool(&reply.data, "on", &on));
	T_TRUE("Zustand: eingeschaltet", on);
	T_TRUE("Zustand: ttl", gh_uint(&reply.data, "ttl", &ttl));
	T_EQ("Zustand: Restzeit", ttl, 3600u);
}

/* Sie laeuft von selbst ab - ohne Zeitgeber, ohne Zutun. */
static void test_session_expires(void)
{
	gh_frame_t frame;
	gh_reply_t reply;
	uint8_t hash[OSK_GUARD_HASH_LEN];
	const char *labels[1] = {SIGN_LABEL};

	setup();
	memset(hash, 0x92, sizeof(hash));

	size_t len = build_session_on(&frame, labels, 1u, 60u);
	T_EQ("einschalten", gh_call(frame.buf, len, OSK_LINK_USB_VENDOR, &reply), OSK_OK);
	T_EQ("einschalten: ok", reply.result, OSK_OK);

	mock_clock_advance(59u * 1000u);
	unsigned asked = mock_ui_confirm_count();
	len = build_sign(&frame, SIGN_LABEL, hash, sizeof(hash));
	T_EQ("kurz davor", gh_call(frame.buf, len, OSK_LINK_USB_VENDOR, &reply), OSK_OK);
	T_EQ("kurz davor: kein Tastendruck", mock_ui_confirm_count(), asked);

	/* Eine Sekunde spaeter ist sie weg, ohne dass jemand etwas tat. */
	mock_clock_advance(1500u);
	asked = mock_ui_confirm_count();
	len = build_sign(&frame, SIGN_LABEL, hash, sizeof(hash));
	T_EQ("danach", gh_call(frame.buf, len, OSK_LINK_USB_VENDOR, &reply), OSK_OK);
	T_EQ("danach wird wieder gefragt", mock_ui_confirm_count(), asked + 1u);

	gh_begin(&frame, OSK_GUARD_CMD_SESSION, 10u, 1u);
	osk_cbor_put_text(&frame.w, "ask");
	osk_cbor_put_bool(&frame.w, true);
	T_EQ("fragen", gh_call(frame.buf, gh_end(&frame), OSK_LINK_USB_VENDOR, &reply), OSK_OK);
	bool on = true;
	T_TRUE("Zustand: on", gh_bool(&reply.data, "on", &on));
	T_TRUE("abgelaufen heisst aus", !on);
}

/* Die Zaeune: Obergrenze der Laufzeit, Zahl der Namen, nur bekannte Namen,
 * nur der Kanal des Verwaltungsprogramms, Nein heisst Nein. */
static void test_session_fences(void)
{
	gh_frame_t frame;
	gh_reply_t reply;
	const char *labels[1] = {SIGN_LABEL};
	const char *many[OSK_GUARD_GRANT_MAX + 1u];

	setup();

	/* 24 Stunden gehen, eine Sekunde mehr nicht - und sie wird nicht
	 * still gekuerzt. */
	size_t len = build_session_on(&frame, labels, 1u, OSK_GUARD_TTL_MAX);
	T_EQ("24 Stunden", gh_call(frame.buf, len, OSK_LINK_USB_VENDOR, &reply), OSK_OK);
	T_EQ("24 Stunden: ok", reply.result, OSK_OK);

	len = build_session_on(&frame, labels, 1u, OSK_GUARD_TTL_MAX + 1u);
	T_EQ("mehr als 24 Stunden", gh_call(frame.buf, len, OSK_LINK_USB_VENDOR, &reply), OSK_OK);
	T_EQ("mehr als 24 Stunden: Grund", reply.result, OSK_ERR_ARG);

	len = build_session_on(&frame, labels, 1u, 0u);
	T_EQ("null Sekunden", gh_call(frame.buf, len, OSK_LINK_USB_VENDOR, &reply), OSK_OK);
	T_EQ("null Sekunden: Grund", reply.result, OSK_ERR_ARG);

	/* Zu viele Namen. */
	for (size_t i = 0; i < OSK_GUARD_GRANT_MAX + 1u; i++) {
		many[i] = SIGN_LABEL;
	}
	len = build_session_on(&frame, many, OSK_GUARD_GRANT_MAX + 1u, 60u);
	T_EQ("zu viele Namen", gh_call(frame.buf, len, OSK_LINK_USB_VENDOR, &reply), OSK_OK);
	T_EQ("zu viele Namen: Grund", reply.result, OSK_ERR_ARG);

	/* Ein Name ohne Schluessel waere eine Zusage ins Leere. */
	const char *ghost[1] = {"gibt-es-nicht"};
	len = build_session_on(&frame, ghost, 1u, 60u);
	T_EQ("Name ohne Schluessel", gh_call(frame.buf, len, OSK_LINK_USB_VENDOR, &reply), OSK_OK);
	T_EQ("Name ohne Schluessel: Grund", reply.result, OSK_ERR_NOT_FOUND);

	/* Nicht aus der Bibliothek: ein anderer Weg als der Kanal des
	 * Verwaltungsprogramms wird abgelehnt - auch wenn der Rahmen stimmt. */
	len = build_session_on(&frame, labels, 1u, 60u);
	T_EQ("anderer Weg", gh_call(frame.buf, len, OSK_LINK_USB_HID, &reply), OSK_OK);
	T_EQ("anderer Weg: abgewiesen", reply.result, OSK_ERR_DENIED);
	T_EQ("anderer Weg fragt nicht einmal", mock_ui_confirm_count(), 1u);

	/* Nein am Geraet heisst: nicht eingeschaltet. */
	mock_ui_set_confirm(OSK_ERR_DENIED);
	len = build_session_on(&frame, labels, 1u, 60u);
	T_EQ("abgelehnt", gh_call(frame.buf, len, OSK_LINK_USB_VENDOR, &reply), OSK_OK);
	T_EQ("abgelehnt: Grund", reply.result, OSK_ERR_DENIED);

	mock_ui_set_confirm(OSK_OK);
	gh_begin(&frame, OSK_GUARD_CMD_SESSION, 11u, 1u);
	osk_cbor_put_text(&frame.w, "ask");
	osk_cbor_put_bool(&frame.w, true);
	T_EQ("fragen", gh_call(frame.buf, gh_end(&frame), OSK_LINK_USB_VENDOR, &reply), OSK_OK);
	bool on = false;
	T_TRUE("Zustand: on", gh_bool(&reply.data, "on", &on));
	T_TRUE("nach dem Nein gilt die erste Freischaltung weiter", on);

	/* Abschalten geht ohne Nachfrage - die Latte hoeher zu legen ist nie
	 * das Gefaehrliche. */
	const unsigned asked = mock_ui_confirm_count();
	gh_begin(&frame, OSK_GUARD_CMD_SESSION, 12u, 1u);
	osk_cbor_put_text(&frame.w, "on");
	osk_cbor_put_bool(&frame.w, false);
	T_EQ("abschalten", gh_call(frame.buf, gh_end(&frame), OSK_LINK_USB_VENDOR, &reply), OSK_OK);
	T_EQ("abschalten: ok", reply.result, OSK_OK);
	T_EQ("abschalten fragt nicht", mock_ui_confirm_count(), asked);

	gh_begin(&frame, OSK_GUARD_CMD_SESSION, 13u, 1u);
	osk_cbor_put_text(&frame.w, "ask");
	osk_cbor_put_bool(&frame.w, true);
	T_EQ("fragen", gh_call(frame.buf, gh_end(&frame), OSK_LINK_USB_VENDOR, &reply), OSK_OK);
	on = true;
	T_TRUE("Zustand: on", gh_bool(&reply.data, "on", &on));
	T_TRUE("nach dem Abschalten ist alles aus", !on);
}

/* osk_guard_init() setzt den Startzustand: nichts freigeschaltet. */
static void test_init_is_off(void)
{
	setup();

	const char *labels[1] = {SIGN_LABEL};
	gh_frame_t frame;
	gh_reply_t reply;
	size_t len = build_session_on(&frame, labels, 1u, 600u);
	T_EQ("einschalten", gh_call(frame.buf, len, OSK_LINK_USB_VENDOR, &reply), OSK_OK);
	T_EQ("einschalten: ok", reply.result, OSK_OK);
	T_TRUE("gilt", osk_guard_grant_allows(SIGN_LABEL, strlen(SIGN_LABEL), 0u));

	T_EQ("init", osk_guard_init(), OSK_OK);
	T_TRUE("nach dem Start ist nichts freigeschaltet",
	       !osk_guard_grant_allows(SIGN_LABEL, strlen(SIGN_LABEL), 0u));
}

/* Im Protokoll muss stehen, WER die Freischaltung eingeschaltet hat und
 * WANN. Ohne das ist "protokolliert" eine Behauptung: ein Satz, der nur
 * sagt, dass irgendwann irgendetwas freigeschaltet wurde, hilft hinterher
 * niemandem.
 *
 * Was das Geraet wissen kann, ist der Kanal und der Name des Schluessels -
 * ein Benutzername stuende nur da, weil der Rechner ihn behauptet hat.
 * Siehe docs/dongle-bau.md, offener Punkt. */
static void test_session_is_logged_with_who_and_when(void)
{
	gh_frame_t frame;
	gh_reply_t reply;
	const char *labels[1] = {SIGN_LABEL};
	size_t len;
	unsigned before, at;
	const char *who;

	setup();
	mock_clock_set(123000u);

	before = mock_log_count();
	len = build_session_on(&frame, labels, 1u, 3600u);
	T_EQ("einschalten", gh_call(frame.buf, len, OSK_LINK_USB_VENDOR, &reply), OSK_OK);
	T_EQ("einschalten: ok", reply.result, OSK_OK);
	T_EQ("ein Satz mehr im Protokoll", mock_log_count(), before + 1u);

	at = mock_log_count() - 1u;
	T_EQ("der Satz ist die Freischaltung", mock_log_what(at), OSK_GUARD_CMD_SESSION);

	who = mock_log_who(at);
	T_TRUE("es steht ein Wer dabei", who != NULL && who[0] != '\0');
	T_TRUE("der Kanal steht drin", strstr(who, "usb-vendor") != NULL);
	T_TRUE("der freigeschaltete Schluessel steht drin",
	       strstr(who, SIGN_LABEL) != NULL);
	T_EQ("und die Zeit des Geraets", (unsigned long)mock_log_when(at), 123ul);

	/* Abschalten steht auch drin - sonst stuende im Protokoll eine
	 * Freischaltung, die nie endete. */
	before = mock_log_count();
	gh_begin(&frame, OSK_GUARD_CMD_SESSION, 7u, 1u);
	osk_cbor_put_text(&frame.w, "on");
	osk_cbor_put_bool(&frame.w, false);
	T_EQ("abschalten", gh_call(frame.buf, gh_end(&frame), OSK_LINK_USB_VENDOR, &reply),
	     OSK_OK);
	T_EQ("abschalten: ok", reply.result, OSK_OK);
	T_EQ("auch das Abschalten steht im Protokoll", mock_log_count(), before + 1u);
	who = mock_log_who(mock_log_count() - 1u);
	T_TRUE("und auch dort der Kanal", who != NULL && strstr(who, "usb-vendor") != NULL);
}

/* Eine abgelehnte Freischaltung darf NICHT als eingeschaltet im Protokoll
 * stehen. Ein Protokoll, das Versuche wie Erfolge fuehrt, ist schlimmer
 * als keines. */
static void test_refused_session_is_not_logged_as_on(void)
{
	gh_frame_t frame;
	gh_reply_t reply;
	const char *labels[1] = {SIGN_LABEL};
	size_t len;
	unsigned before;

	setup();
	mock_ui_set_confirm(OSK_ERR_DENIED);
	before = mock_log_count();
	len = build_session_on(&frame, labels, 1u, 3600u);
	T_EQ("einschalten", gh_call(frame.buf, len, OSK_LINK_USB_VENDOR, &reply), OSK_OK);
	T_EQ("ohne Freigabe abgewiesen", reply.result, OSK_ERR_DENIED);
	T_TRUE("nichts freigeschaltet",
	       !osk_guard_grant_allows(SIGN_LABEL, strlen(SIGN_LABEL), 0u));
	T_EQ("und kein Satz im Protokoll", mock_log_count(), before);
}

int main(void)
{
	test_sign();
	test_sign_without_human();
	test_sign_public();
	test_agree();
	test_agree_no_ecdh();
	test_session();
	test_session_expires();
	test_session_fences();
	test_init_is_off();
	test_session_is_logged_with_who_and_when();
	test_refused_session_is_not_logged_as_on();
	return t_report("guard-knoten");
}
