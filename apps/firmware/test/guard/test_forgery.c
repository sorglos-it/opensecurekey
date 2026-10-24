/* Die Faelschung, die der Programm-Strang beim Umbau der Attrappe gefunden
 * hat - hier gegen die Firmware nachgestellt.
 *
 * Der Angriff braucht keine Schwaeche in der Kurve. Er braucht nur, dass
 * zwei verschiedene Anliegen dieselben Bytes unterschreiben:
 *
 *   Dongle-Weg (attest mit `bh`):  Zweck 0x00 nonce(32) pruefsumme(32)
 *   alter Weg  (attest ohne `bh`): Zweck 0x00 aufgabe(1..n)
 *
 * Eine Aufgabe von 64 Byte ist genau so lang wie nonce und pruefsumme
 * zusammen. Mit EINEM gemeinsamen Zweck waere die Antwort auf
 * attest(nonce||pruefsumme) Byte fuer Byte die Antwort des Dongle-Wegs -
 * und ein geaendertes Programm bekaeme wieder gueltige Antworten, solange
 * der echte Stick steckt. Die Bindung an die Programmdatei waere weg.
 *
 * Dasselbe gilt zwischen `attest` und `sign`: wer einen Auftrag als Frage
 * hineingibt, duerfte keine gueltige Auftragsunterschrift bekommen.
 *
 * Dieser Fall vergleicht Bytes. Er STIRBT, sobald jemand zwei der drei
 * Zwecke in osk_guard_msg.c zusammenlegt - gemessen mit mutanten.py.
 */
#include "test_util.h"
#include "mocks.h"
#include "guard_helper.h"
#include "osk_guard_msg.h"
#include "osk_guard_keys.h"

#define LABEL "prod-eu-1"
#define LABEL_LEN 9u

static void setup(void)
{
	mock_reset_all();
	mock_se050_setup_guard();
	osk_guard_grant_clear();
	osk_guard_keys_reset();
	T_EQ("Name eintragen", osk_guard_node_register(LABEL, LABEL_LEN, OSK_GUARD_CURVE_ED25519),
	     OSK_OK);
}

/* Holt eine attest-Antwort. `bh` wird nur mitgeschickt, wenn hash != NULL. */
static bool ask_attest(const uint8_t *challenge, size_t challenge_len, const uint8_t *hash,
		       uint8_t sig_out[OSK_GUARD_SIG_LEN])
{
	gh_frame_t frame;
	gh_reply_t reply;

	gh_begin(&frame, OSK_GUARD_CMD_ATTEST, 7u, hash != NULL ? 2u : 1u);
	osk_cbor_put_text(&frame.w, "ch");
	osk_cbor_put_bytes(&frame.w, challenge, challenge_len);
	if (hash != NULL) {
		osk_cbor_put_text(&frame.w, "bh");
		osk_cbor_put_bytes(&frame.w, hash, OSK_GUARD_HASH_LEN);
	}
	const size_t len = gh_end(&frame);
	if (len == 0u) {
		return false;
	}
	if (gh_call(frame.buf, len, OSK_LINK_USB_VENDOR, &reply) != OSK_OK || !reply.ok) {
		return false;
	}
	if (reply.result != OSK_OK) {
		return false;
	}
	const uint8_t *sig = NULL;
	size_t sig_len = 0;
	if (!gh_bytes(&reply.data, "sig", &sig, &sig_len) || sig_len != OSK_GUARD_SIG_LEN) {
		return false;
	}
	memcpy(sig_out, sig, OSK_GUARD_SIG_LEN);
	return true;
}

static bool ask_sign(const uint8_t hash[OSK_GUARD_HASH_LEN], uint8_t sig_out[OSK_GUARD_SIG_LEN])
{
	gh_frame_t frame;
	gh_reply_t reply;

	gh_begin(&frame, OSK_GUARD_CMD_SIGN, 8u, 2u);
	osk_cbor_put_text(&frame.w, "k");
	osk_cbor_put_text(&frame.w, LABEL);
	osk_cbor_put_text(&frame.w, "h");
	osk_cbor_put_bytes(&frame.w, hash, OSK_GUARD_HASH_LEN);
	const size_t len = gh_end(&frame);
	if (len == 0u) {
		return false;
	}
	if (gh_call(frame.buf, len, OSK_LINK_USB_VENDOR, &reply) != OSK_OK || !reply.ok) {
		return false;
	}
	if (reply.result != OSK_OK) {
		return false;
	}
	const uint8_t *sig = NULL;
	size_t sig_len = 0;
	if (!gh_bytes(&reply.data, "sig", &sig, &sig_len) || sig_len != OSK_GUARD_SIG_LEN) {
		return false;
	}
	memcpy(sig_out, sig, OSK_GUARD_SIG_LEN);
	return true;
}

/* Zwischenprueffung: der Weg bis zum Unterschreiben ist offen, und
 * unterschrieben wurde GENAU die Bytefolge des Dongle-Wegs. Ohne diese
 * Zusicherung koennte der Fall unten auch deshalb gruen sein, weil eine
 * frueherere Wand jede Antwort verhindert. */
static void test_path_is_open(void)
{
	uint8_t nonce[OSK_GUARD_NONCE_LEN];
	uint8_t hash[OSK_GUARD_HASH_LEN];
	uint8_t sig[OSK_GUARD_SIG_LEN];
	uint8_t msg[OSK_GUARD_MSG_MAX];
	uint8_t want[OSK_GUARD_SIG_LEN];
	size_t msg_len = 0;

	setup();
	memset(nonce, 0x31, sizeof(nonce));
	memset(hash, 0x32, sizeof(hash));

	T_TRUE("Dongle-Weg antwortet", ask_attest(nonce, sizeof(nonce), hash, sig));
	T_EQ("Bytefolge gebaut",
	     osk_guard_msg_attest(nonce, sizeof(nonce), hash, sizeof(hash), msg, sizeof(msg),
				  &msg_len),
	     OSK_OK);
	mock_se050_expected_sig(OSK_GUARD_KEY_IDENTITY, msg, msg_len, want);
	T_TRUE("unterschrieben wurde die Dongle-Bytefolge", memcmp(sig, want, sizeof(sig)) == 0);
}

/* Der Angriff: 64 Byte Aufgabe an den alten attest-Aufruf. */
static void test_raw64_is_no_dongle_answer(void)
{
	uint8_t nonce[OSK_GUARD_NONCE_LEN];
	uint8_t hash[OSK_GUARD_HASH_LEN];
	uint8_t joined[OSK_GUARD_NONCE_LEN + OSK_GUARD_HASH_LEN];
	uint8_t sig_dongle[OSK_GUARD_SIG_LEN];
	uint8_t sig_raw[OSK_GUARD_SIG_LEN];

	setup();
	memset(nonce, 0x41, sizeof(nonce));
	memset(hash, 0x42, sizeof(hash));
	memcpy(joined, nonce, sizeof(nonce));
	memcpy(joined + sizeof(nonce), hash, sizeof(hash));

	T_TRUE("Dongle-Weg antwortet", ask_attest(nonce, sizeof(nonce), hash, sig_dongle));
	/* Der Angreifer gibt Frage und Pruefsumme als EINE Aufgabe hinein. */
	T_TRUE("alter Weg antwortet", ask_attest(joined, sizeof(joined), NULL, sig_raw));

	/* Hier stirbt der Fall, wenn die Zwecke zusammengelegt werden. */
	T_TRUE("64-Byte-Aufgabe gilt nicht als Dongle-Antwort",
	       memcmp(sig_raw, sig_dongle, sizeof(sig_raw)) != 0);
}

/* Derselbe Angriff zwischen attest und sign: ein Auftrag, als Frage
 * hineingegeben. */
static void test_raw_is_no_job_answer(void)
{
	uint8_t hash[OSK_GUARD_HASH_LEN];
	uint8_t joined[LABEL_LEN + 1u + OSK_GUARD_HASH_LEN];
	uint8_t sig_job[OSK_GUARD_SIG_LEN];
	uint8_t sig_raw[OSK_GUARD_SIG_LEN];

	setup();
	memset(hash, 0x51, sizeof(hash));
	memcpy(joined, LABEL, LABEL_LEN);
	joined[LABEL_LEN] = 0u;
	memcpy(joined + LABEL_LEN + 1u, hash, sizeof(hash));

	T_TRUE("Auftrag wird unterschrieben", ask_sign(hash, sig_job));
	T_TRUE("alter Weg antwortet", ask_attest(joined, sizeof(joined), NULL, sig_raw));

	/* Die Unterschriften stammen aus verschiedenen Schluesseln UND aus
	 * verschiedenen Zwecken. Beide Trennungen muessen halten, und keine
	 * darf die andere ersetzen - darum prueft der naechste Fall den Zweck
	 * allein, mit demselben Schluessel. */
	T_TRUE("Auftrag als Frage gibt keine Auftragsunterschrift",
	       memcmp(sig_raw, sig_job, sizeof(sig_raw)) != 0);
}

/* Der Zweck allein, ohne den Schluessel als zweite Trennung: dieselbe
 * Bytefolge ohne Zweck, derselbe Geraeteschluessel - nur der Zweck steht
 * davor. Wird er zusammengelegt, sind die beiden Unterschriften gleich. */
static void test_domain_alone_separates(void)
{
	uint8_t body[OSK_GUARD_NONCE_LEN + OSK_GUARD_HASH_LEN];
	uint8_t msg_dongle[OSK_GUARD_MSG_MAX];
	uint8_t msg_raw[OSK_GUARD_MSG_MAX];
	uint8_t sig_dongle[OSK_GUARD_SIG_LEN];
	uint8_t sig_raw[OSK_GUARD_SIG_LEN];
	size_t len_dongle = 0, len_raw = 0;

	setup();
	memset(body, 0x61, sizeof(body));

	T_EQ("Dongle-Bytefolge",
	     osk_guard_msg_attest(body, OSK_GUARD_NONCE_LEN, body + OSK_GUARD_NONCE_LEN,
				  OSK_GUARD_HASH_LEN, msg_dongle, sizeof(msg_dongle), &len_dongle),
	     OSK_OK);
	T_EQ("alte Bytefolge",
	     osk_guard_msg_attest_raw(body, sizeof(body), msg_raw, sizeof(msg_raw), &len_raw),
	     OSK_OK);

	mock_se050_expected_sig(OSK_GUARD_KEY_IDENTITY, msg_dongle, len_dongle, sig_dongle);
	mock_se050_expected_sig(OSK_GUARD_KEY_IDENTITY, msg_raw, len_raw, sig_raw);

	T_TRUE("gleiche Bytes, gleicher Schluessel - und doch verschieden",
	       memcmp(sig_dongle, sig_raw, sizeof(sig_raw)) != 0);
}

/* Zweck gegen Zweck zwischen attest und sign, mit DEMSELBEN Schluessel.
 *
 * Dieser Fall ist entstanden, weil ein Mutant ueberlebt hat: "sign bekommt
 * den attest-Zweck" blieb unentdeckt, weil die beiden Befehle im Betrieb
 * auch verschiedene Schluessel benutzen (Geraeteschluessel gegen
 * Knotenschluessel). Die Trennung war damit nur ueber den Schluessel
 * gemessen, nicht ueber den Zweck - und die Zusage in shared/protocol.md
 * nennt den Zweck.
 *
 * Der Trick ist eine Namenslaenge, die die beiden Bytefolgen gleich lang
 * macht: ein Name von 31 Byte plus sein Trennbyte sind genau 32 Byte, also
 * so lang wie der Zufallswert. Mit einem gemeinsamen Zweck waeren
 *
 *   attest: Z 0x00 <32 Byte> <32 Byte>
 *   sign:   Z 0x00 <31 Byte Name> 0x00 <32 Byte>
 *
 * Byte fuer Byte dieselbe Nachricht. Also waere auch die Unterschrift
 * dieselbe, und ein unterschriebener Auftrag ginge als Dongle-Antwort durch.
 */
static void test_attest_and_sign_domains_differ(void)
{
	static const char label31[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"; /* 31 */
	uint8_t nonce[OSK_GUARD_NONCE_LEN];
	uint8_t hash[OSK_GUARD_HASH_LEN];
	uint8_t msg_attest[OSK_GUARD_MSG_MAX];
	uint8_t msg_sign[OSK_GUARD_MSG_MAX];
	uint8_t sig_attest[OSK_GUARD_SIG_LEN];
	uint8_t sig_sign[OSK_GUARD_SIG_LEN];
	size_t len_attest = 0, len_sign = 0;

	setup();
	T_EQ("Name hat 31 Byte", strlen(label31), 31u);

	/* Zufallswert = Name + Trennbyte. */
	memcpy(nonce, label31, 31u);
	nonce[31] = 0u;
	memset(hash, 0x77, sizeof(hash));

	T_EQ("attest-Bytefolge",
	     osk_guard_msg_attest(nonce, sizeof(nonce), hash, sizeof(hash), msg_attest,
				  sizeof(msg_attest), &len_attest),
	     OSK_OK);
	T_EQ("sign-Bytefolge",
	     osk_guard_msg_sign(label31, 31u, hash, sizeof(hash), msg_sign, sizeof(msg_sign),
				&len_sign),
	     OSK_OK);

	/* Zwischenpruefung: hinter dem Zweck sind die beiden Nachrichten
	 * wirklich gleich. Ohne das waere der Fall unten auch gruen, wenn die
	 * Konstruktion danebengegangen ist - und dann misst er nichts. */
	T_EQ("gleich lang hinter dem Zweck", len_attest - 18u, len_sign - 16u);
	T_TRUE("hinter dem Zweck Byte fuer Byte gleich",
	       memcmp(msg_attest + 18, msg_sign + 16, len_sign - 16u) == 0);

	/* Derselbe Schluessel - die Trennung kann nur noch vom Zweck kommen. */
	mock_se050_expected_sig(OSK_GUARD_KEY_IDENTITY, msg_attest, len_attest, sig_attest);
	mock_se050_expected_sig(OSK_GUARD_KEY_IDENTITY, msg_sign, len_sign, sig_sign);

	T_TRUE("attest und sign trennt der Zweck, nicht nur der Schluessel",
	       memcmp(sig_attest, sig_sign, sizeof(sig_sign)) != 0);
}

int main(void)
{
	test_path_is_open();
	test_raw64_is_no_dongle_answer();
	test_raw_is_no_job_answer();
	test_domain_alone_separates();
	test_attest_and_sign_domains_differ();
	return t_report("guard-faelschung");
}
