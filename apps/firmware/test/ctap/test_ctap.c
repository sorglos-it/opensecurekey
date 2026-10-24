/* Echte CTAP2-Nachrichten hinein, Antworten gegen die Spezifikation
 * geprueft: GetInfo, MakeCredential, GetAssertion, GetNextAssertion,
 * Selection, Reset und die Erweiterungen.
 *
 * Nichts davon lief am Geraet oder in einem Browser - der Chip, die Anzeige
 * und der Tresor sind Attrappen (siehe mocks.h). */
#include "ctap_helper.h"
#include "osk_credid.h"
#include "osk_ctap_ext.h"
#include "osk_pin.h"

static uint8_t g_body[2048];
static size_t g_len;

static void start(void)
{
	mock_reset_all();
	T_EQ("Kern startet",
	     osk_ctap_init(OSK_CTAP_EXT_HMAC_SECRET | OSK_CTAP_EXT_PRF | OSK_CTAP_EXT_LARGE_BLOB |
			   OSK_CTAP_EXT_CRED_PROTECT | OSK_CTAP_EXT_MIN_PIN),
	     OSK_OK);
}

/* ---- GetInfo ----------------------------------------------------------- */

static void test_get_info(void)
{
	start();

	const uint8_t msg[1] = {OSK_CTAP_CMD_GET_INFO};
	const uint8_t code = h_call(msg, sizeof(msg), g_body, sizeof(g_body), &g_len);
	T_EQ("GetInfo ok", code, CTAP2_OK);
	T_EQ("Antwort ist kanonisches CBOR", osk_cbor_validate(g_body, g_len), OSK_OK);

	/* aaguid muss 16 Bytes sein (WebAuthn 6.5.1). */
	const uint8_t *aaguid = NULL;
	size_t alen = 0;
	T_TRUE("aaguid vorhanden", h_get_bytes(g_body, g_len, 0x03, &aaguid, &alen));
	T_EQ("aaguid 16 Bytes", (long long)alen, 16);

	/* maxCredentialIdLength muss zu unserer ID passen. */
	osk_cbor_reader_t r;
	uint64_t v = 0;
	T_TRUE("maxCredentialIdLength vorhanden", h_find(g_body, g_len, 0x08, &r));
	T_EQ("Wert lesbar", osk_cbor_read_uint(&r, &v), OSK_OK);
	T_EQ("passt zur Credential-ID", (long long)v, OSK_CREDID_LEN);

	/* pinUvAuthProtocols: Protokoll 2 steht vorn. */
	T_TRUE("pinUvAuthProtocols vorhanden", h_find(g_body, g_len, 0x06, &r));
	size_t n = 0;
	T_EQ("Liste", osk_cbor_enter_array(&r, &n), OSK_OK);
	T_EQ("zwei Protokolle", (long long)n, 2);
	T_EQ("erstes lesen", osk_cbor_read_uint(&r, &v), OSK_OK);
	T_EQ("Protokoll 2 bevorzugt", (long long)v, 2);

	/* Die Optionen einzeln nachsehen. Frueher stand hier nur "options
	 * vorhanden" - und genau darum fiel jahrelang nicht auf, dass "uv"
	 * fest auf true stand, obwohl das Geraet keine eingebaute
	 * Benutzerpruefung hat. "uv" meint in CTAP 2.1 den Fingerabdruck,
	 * nicht die PIN; die wird ueber "clientPin" gemeldet. */
	T_TRUE("options vorhanden", h_find(g_body, g_len, 0x04, &r));
	size_t opt_count = 0;
	T_EQ("options ist eine Map", osk_cbor_enter_map(&r, &opt_count), OSK_OK);
	bool saw_uv = false, saw_client_pin = false, client_pin_value = true;
	for (size_t i = 0; i < opt_count; i++) {
		const char *key = NULL;
		size_t key_len = 0;
		T_EQ("Schluessel lesbar", osk_cbor_read_text(&r, &key, &key_len), OSK_OK);
		bool value = false;
		T_EQ("Wert lesbar", osk_cbor_read_bool(&r, &value), OSK_OK);
		if (key_len == 2 && memcmp(key, "uv", 2) == 0) {
			saw_uv = true;
		} else if (key_len == 9 && memcmp(key, "clientPin", 9) == 0) {
			saw_client_pin = true;
			client_pin_value = value;
		}
	}
	/* Ohne Sensor darf "uv" gar nicht auftauchen: so sagt die
	 * Spezifikation, dass wir es nicht koennen. Ein false waere die
	 * Aussage "koennen wir, ist aber nicht eingerichtet" - auch das
	 * stimmte nicht. Kommt der Sensor, wird dieser Fall umgedreht. */
	T_TRUE("uv wird NICHT gemeldet, es gibt keinen Sensor", !saw_uv);
	T_TRUE("clientPin wird gemeldet", saw_client_pin);
	T_TRUE("clientPin ist false ohne gesetzte PIN", !client_pin_value);

	/* Mit Nutzdaten ist GetInfo ein Laengenfehler. */
	const uint8_t msg2[2] = {OSK_CTAP_CMD_GET_INFO, 0x00};
	T_EQ("GetInfo mit Anhang", h_call(msg2, sizeof(msg2), g_body, sizeof(g_body), &g_len),
	     CTAP1_ERR_INVALID_LENGTH);
}

/* ---- MakeCredential ---------------------------------------------------- */

/* Prueft den authData-Block einer MakeCredential-Antwort. */
static void check_auth_data(const uint8_t *body, size_t len, const char *rp_id,
			    uint8_t want_flags_set, const uint8_t **cred_id_out,
			    size_t *cred_id_len_out)
{
	const uint8_t *ad = NULL;
	size_t ad_len = 0;
	uint8_t want_hash[32];

	T_TRUE("authData vorhanden", h_get_bytes(body, len, 0x02, &ad, &ad_len));
	T_TRUE("authData lang genug", ad_len >= 37 + 18);
	if (ad_len < 55)
		return;

	osk_sha256((const uint8_t *)rp_id, strlen(rp_id), want_hash);
	T_TRUE("rpIdHash stimmt", memcmp(ad, want_hash, 32) == 0);
	T_EQ("Flaggen gesetzt", ad[32] & want_flags_set, want_flags_set);
	T_TRUE("AT-Flagge gesetzt", (ad[32] & OSK_AD_FLAG_AT) != 0);

	const size_t id_len = ((size_t)ad[53] << 8) | ad[54];
	T_EQ("Credential-ID hat unsere Laenge", (long long)id_len, OSK_CREDID_LEN);
	if (cred_id_out) {
		*cred_id_out = ad + 55;
		*cred_id_len_out = id_len;
	}

	/* Hinter der ID steht der oeffentliche Schluessel als COSE_Key. */
	const uint8_t *cose = ad + 55 + id_len;
	const size_t cose_len = ad_len - 55 - id_len;
	osk_cbor_reader_t r;
	osk_cbor_reader_init(&r, cose, cose_len);
	size_t count = 0;
	T_EQ("COSE_Key ist eine Abbildung", osk_cbor_enter_map(&r, &count), OSK_OK);
	T_EQ("fuenf Eintraege", (long long)count, 5);
}

static void test_make_credential(void)
{
	h_msg_t m;

	start();
	h_make_credential(&m, "example.com", "anna", false, false, 0, NULL, 0, 0);

	const uint8_t code = h_call(m.buf, m.len, g_body, sizeof(g_body), &g_len);
	T_EQ("MakeCredential ok", code, CTAP2_OK);
	T_EQ("Antwort kanonisch", osk_cbor_validate(g_body, g_len), OSK_OK);

	/* Der Dienstname muss auf dem Display gestanden haben. */
	T_EQ("einmal gefragt", (long long)mock_ui_confirm_count(), 1);
	T_TRUE("Dienst im Display", strcmp(mock_ui_last_subject(), "example.com") == 0);
	T_TRUE("Nutzer im Display", strcmp(mock_ui_last_detail(), "anna") == 0);

	/* fmt muss "packed" sein. */
	osk_cbor_reader_t r;
	T_TRUE("fmt vorhanden", h_find(g_body, g_len, 0x01, &r));
	const char *fmt = NULL;
	size_t flen = 0;
	T_EQ("fmt lesbar", osk_cbor_read_text(&r, &fmt, &flen), OSK_OK);
	T_TRUE("fmt ist packed", flen == 6 && memcmp(fmt, "packed", 6) == 0);

	const uint8_t *cred_id = NULL;
	size_t cred_id_len = 0;
	check_auth_data(g_body, g_len, "example.com", OSK_AD_FLAG_UP, &cred_id, &cred_id_len);

	/* Die ID muss sich mit unserem Grundgeheimnis wieder oeffnen lassen. */
	osk_credid_t cid;
	T_EQ("Credential-ID gueltig", osk_credid_unpack(cred_id, cred_id_len, &cid), OSK_OK);
	T_TRUE("nicht resident", !cid.resident);

	/* attStmt: alg und sig */
	T_TRUE("attStmt vorhanden", h_find(g_body, g_len, 0x03, &r));
	size_t count = 0;
	T_EQ("attStmt ist Abbildung", osk_cbor_enter_map(&r, &count), OSK_OK);
	T_EQ("zwei Eintraege", (long long)count, 2);
}

static void test_make_credential_fehler(void)
{
	h_msg_t m;
	osk_cbor_writer_t w;

	start();

	/* Kein ES256 im Angebot: die Spezifikation verlangt genau diesen Fehler. */
	m.buf[0] = OSK_CTAP_CMD_MAKE_CREDENTIAL;
	osk_cbor_writer_init(&w, m.buf + 1, sizeof(m.buf) - 1);
	{
		uint8_t hash[32];
		osk_sha256((const uint8_t *)"c", 1, hash);
		osk_cbor_put_map(&w, 4);
		osk_cbor_put_uint(&w, 1);
		osk_cbor_put_bytes(&w, hash, 32);
		osk_cbor_put_uint(&w, 2);
		osk_cbor_put_map(&w, 1);
		osk_cbor_put_text(&w, "id");
		osk_cbor_put_text(&w, "example.com");
		osk_cbor_put_uint(&w, 3);
		osk_cbor_put_map(&w, 1);
		osk_cbor_put_text(&w, "id");
		osk_cbor_put_bytes(&w, (const uint8_t *)"u", 1);
		osk_cbor_put_uint(&w, 4);
		osk_cbor_put_array(&w, 1);
		osk_cbor_put_map(&w, 2);
		osk_cbor_put_text(&w, "alg");
		osk_cbor_put_int(&w, -257); /* RS256 */
		osk_cbor_put_text(&w, "type");
		osk_cbor_put_text(&w, "public-key");
		m.len = 1 + w.len;
	}
	T_EQ("unbekanntes Verfahren", h_call(m.buf, m.len, g_body, sizeof(g_body), &g_len),
	     CTAP2_ERR_UNSUPPORTED_ALGORITHM);

	/* Pflichtfeld fehlt */
	m.buf[0] = OSK_CTAP_CMD_MAKE_CREDENTIAL;
	osk_cbor_writer_init(&w, m.buf + 1, sizeof(m.buf) - 1);
	osk_cbor_put_map(&w, 1);
	osk_cbor_put_uint(&w, 1);
	osk_cbor_put_bytes(&w, (const uint8_t *)"zu kurz", 7);
	m.len = 1 + w.len;
	T_EQ("clientDataHash falsch lang", h_call(m.buf, m.len, g_body, sizeof(g_body), &g_len),
	     CTAP1_ERR_INVALID_PARAMETER);

	/* Nutzer sagt nein */
	start();
	h_make_credential(&m, "example.com", "anna", false, false, 0, NULL, 0, 0);
	mock_ui_set_confirm(OSK_ERR_DENIED);
	T_EQ("abgelehnt", h_call(m.buf, m.len, g_body, sizeof(g_body), &g_len),
	     CTAP2_ERR_OPERATION_DENIED);

	/* Zeit abgelaufen */
	mock_ui_set_confirm(OSK_ERR_TIMEOUT);
	T_EQ("Zeit abgelaufen", h_call(m.buf, m.len, g_body, sizeof(g_body), &g_len),
	     CTAP2_ERR_USER_ACTION_TIMEOUT);

	/* Unbekannter Befehl */
	const uint8_t bad[1] = {0x7e};
	T_EQ("unbekannter Befehl", h_call(bad, 1, g_body, sizeof(g_body), &g_len),
	     CTAP1_ERR_INVALID_COMMAND);

	/* Kaputtes CBOR */
	const uint8_t broken[3] = {OSK_CTAP_CMD_MAKE_CREDENTIAL, 0xa1, 0x01};
	T_EQ("kaputtes CBOR", h_call(broken, 3, g_body, sizeof(g_body), &g_len),
	     CTAP2_ERR_INVALID_CBOR);
}

/* ---- GetAssertion ------------------------------------------------------ */

/* Baut eine GetAssertion-Nachricht mit allowList. */
static void build_get_assertion(h_msg_t *m, const char *rp_id, const uint8_t *cred_id,
				size_t cred_id_len, const uint8_t *pin_auth, size_t pin_auth_len,
				uint64_t pin_protocol)
{
	osk_cbor_writer_t w;
	uint8_t hash[32];
	size_t entries = 2;

	osk_sha256((const uint8_t *)"clientData", 10, hash);
	if (cred_id)
		entries++;
	if (pin_auth)
		entries += 2;

	m->buf[0] = OSK_CTAP_CMD_GET_ASSERTION;
	osk_cbor_writer_init(&w, m->buf + 1, sizeof(m->buf) - 1);
	osk_cbor_put_map(&w, entries);
	osk_cbor_put_uint(&w, 0x01);
	osk_cbor_put_text(&w, rp_id);
	osk_cbor_put_uint(&w, 0x02);
	osk_cbor_put_bytes(&w, hash, 32);
	if (cred_id) {
		osk_cbor_put_uint(&w, 0x03);
		osk_cbor_put_array(&w, 1);
		osk_cbor_put_map(&w, 2);
		osk_cbor_put_text(&w, "id");
		osk_cbor_put_bytes(&w, cred_id, cred_id_len);
		osk_cbor_put_text(&w, "type");
		osk_cbor_put_text(&w, "public-key");
	}
	if (pin_auth) {
		osk_cbor_put_uint(&w, 0x06);
		osk_cbor_put_bytes(&w, pin_auth, pin_auth_len);
		osk_cbor_put_uint(&w, 0x07);
		osk_cbor_put_uint(&w, pin_protocol);
	}
	m->len = 1 + w.len;
}

/* Erzeugt einen Nachweis und gibt seine Credential-ID zurueck. */
static size_t create_cred(const char *rp_id, const char *user, bool rk, bool hmac_secret,
			  int cred_protect, uint8_t *id_out)
{
	h_msg_t m;
	const uint8_t *cred_id = NULL;
	size_t cred_id_len = 0;

	h_make_credential(&m, rp_id, user, rk, hmac_secret, cred_protect, NULL, 0, 0);
	const uint8_t code = h_call(m.buf, m.len, g_body, sizeof(g_body), &g_len);
	T_EQ("Nachweis angelegt", code, CTAP2_OK);
	if (code != CTAP2_OK)
		return 0;

	const uint8_t *ad = NULL;
	size_t ad_len = 0;
	if (!h_get_bytes(g_body, g_len, 0x02, &ad, &ad_len) || ad_len < 55)
		return 0;
	cred_id_len = ((size_t)ad[53] << 8) | ad[54];
	cred_id = ad + 55;
	memcpy(id_out, cred_id, cred_id_len);
	return cred_id_len;
}

static void test_get_assertion(void)
{
	uint8_t cred_id[OSK_MAX_CRED_ID];
	h_msg_t m;

	start();
	const size_t cred_id_len = create_cred("example.com", "anna", false, false, 0, cred_id);
	T_TRUE("ID bekommen", cred_id_len == OSK_CREDID_LEN);

	build_get_assertion(&m, "example.com", cred_id, cred_id_len, NULL, 0, 0);
	const uint8_t code = h_call(m.buf, m.len, g_body, sizeof(g_body), &g_len);
	T_EQ("GetAssertion ok", code, CTAP2_OK);
	T_EQ("Antwort kanonisch", osk_cbor_validate(g_body, g_len), OSK_OK);

	/* Der Dienst stand im Display. */
	T_TRUE("Dienst im Display", strcmp(mock_ui_last_subject(), "example.com") == 0);

	/* credential.id muss dieselbe sein. */
	osk_cbor_reader_t r;
	T_TRUE("credential vorhanden", h_find(g_body, g_len, 0x01, &r));
	size_t count = 0;
	T_EQ("Abbildung", osk_cbor_enter_map(&r, &count), OSK_OK);
	const char *k = NULL;
	size_t klen = 0;
	T_EQ("Schluessel id", osk_cbor_read_text(&r, &k, &klen), OSK_OK);
	const uint8_t *got = NULL;
	size_t got_len = 0;
	T_EQ("id lesbar", osk_cbor_read_bytes(&r, &got, &got_len), OSK_OK);
	T_TRUE("dieselbe Credential-ID",
	       got_len == cred_id_len && memcmp(got, cred_id, got_len) == 0);

	/* authData ohne AT-Flagge, mit UP. */
	const uint8_t *ad = NULL;
	size_t ad_len = 0;
	T_TRUE("authData vorhanden", h_get_bytes(g_body, g_len, 0x02, &ad, &ad_len));
	T_EQ("authData ist 37 Bytes", (long long)ad_len, 37);
	T_TRUE("UP gesetzt", (ad[32] & OSK_AD_FLAG_UP) != 0);
	T_TRUE("AT nicht gesetzt", (ad[32] & OSK_AD_FLAG_AT) == 0);
	T_TRUE("UV nicht gesetzt", (ad[32] & OSK_AD_FLAG_UV) == 0);

	/* Der Zaehler laeuft vorwaerts. */
	const uint32_t c1 =
	    ((uint32_t)ad[33] << 24) | ((uint32_t)ad[34] << 16) | ((uint32_t)ad[35] << 8) | ad[36];
	T_EQ("GetAssertion erneut", h_call(m.buf, m.len, g_body, sizeof(g_body), &g_len), CTAP2_OK);
	T_TRUE("authData wieder da", h_get_bytes(g_body, g_len, 0x02, &ad, &ad_len));
	const uint32_t c2 =
	    ((uint32_t)ad[33] << 24) | ((uint32_t)ad[34] << 16) | ((uint32_t)ad[35] << 8) | ad[36];
	T_TRUE("Zaehler steigt", c2 > c1);

	/* Falscher Dienst: die ID gehoert nicht dazu. */
	build_get_assertion(&m, "anderer.example", cred_id, cred_id_len, NULL, 0, 0);
	T_EQ("falscher Dienst", h_call(m.buf, m.len, g_body, sizeof(g_body), &g_len),
	     CTAP2_ERR_NO_CREDENTIALS);

	/* Verfaelschte ID */
	cred_id[20] ^= 0x01;
	build_get_assertion(&m, "example.com", cred_id, cred_id_len, NULL, 0, 0);
	T_EQ("verfaelschte ID", h_call(m.buf, m.len, g_body, sizeof(g_body), &g_len),
	     CTAP2_ERR_NO_CREDENTIALS);
	cred_id[20] ^= 0x01;

	/* Ohne allowList und ohne residenten Nachweis: nichts da. */
	build_get_assertion(&m, "example.com", NULL, 0, NULL, 0, 0);
	T_EQ("nichts zu finden", h_call(m.buf, m.len, g_body, sizeof(g_body), &g_len),
	     CTAP2_ERR_NO_CREDENTIALS);
}

/* ---- Residente Nachweise und GetNextAssertion -------------------------- */

static void test_resident_und_next(void)
{
	uint8_t id1[OSK_MAX_CRED_ID], id2[OSK_MAX_CRED_ID];
	h_msg_t m;

	start();
	/* Residente Nachweise brauchen ohne PIN keine Benutzerpruefung,
	 * solange keine PIN gesetzt ist. */
	T_TRUE("erster resident",
	       create_cred("example.com", "anna", true, false, 0, id1) == OSK_CREDID_LEN);
	mock_clock_advance(5000);
	T_TRUE("zweiter resident",
	       create_cred("example.com", "berta", true, false, 0, id2) == OSK_CREDID_LEN);
	T_EQ("zwei im Tresor", (long long)mock_store_count(), 2);

	size_t counted = 0;
	T_EQ("Zaehler", osk_ctap_cred_count(&counted), OSK_OK);
	T_EQ("zwei Nachweise", (long long)counted, 2);

	/* Ohne allowList werden beide gefunden; der neuere kommt zuerst. */
	build_get_assertion(&m, "example.com", NULL, 0, NULL, 0, 0);
	T_EQ("GetAssertion ok", h_call(m.buf, m.len, g_body, sizeof(g_body), &g_len), CTAP2_OK);
	T_EQ("Antwort kanonisch", osk_cbor_validate(g_body, g_len), OSK_OK);

	osk_cbor_reader_t r;
	uint64_t n = 0;
	T_TRUE("numberOfCredentials vorhanden", h_find(g_body, g_len, 0x05, &r));
	T_EQ("Wert lesbar", osk_cbor_read_uint(&r, &n), OSK_OK);
	T_EQ("zwei Treffer", (long long)n, 2);

	/* Nutzerkennung ist dabei, der Name nur mit Benutzerpruefung. */
	T_TRUE("user vorhanden", h_find(g_body, g_len, 0x04, &r));
	size_t count = 0;
	T_EQ("user ist Abbildung", osk_cbor_enter_map(&r, &count), OSK_OK);
	T_EQ("nur die Kennung ohne uv", (long long)count, 1);

	/* GetNextAssertion liefert den zweiten, ohne numberOfCredentials. */
	const uint8_t next[1] = {OSK_CTAP_CMD_GET_NEXT};
	T_EQ("GetNextAssertion ok", h_call(next, 1, g_body, sizeof(g_body), &g_len), CTAP2_OK);
	T_EQ("Antwort kanonisch", osk_cbor_validate(g_body, g_len), OSK_OK);
	T_TRUE("kein numberOfCredentials mehr", !h_find(g_body, g_len, 0x05, &r));
	/* Beim zweiten Mal gibt es nichts mehr. */
	T_EQ("dritter Aufruf", h_call(next, 1, g_body, sizeof(g_body), &g_len),
	     CTAP2_ERR_NOT_ALLOWED);

	/* Nach 30 Sekunden ist der Zustand verfallen. */
	T_EQ("GetAssertion erneut", h_call(m.buf, m.len, g_body, sizeof(g_body), &g_len), CTAP2_OK);
	mock_clock_advance(31000);
	T_EQ("zu spaet", h_call(next, 1, g_body, sizeof(g_body), &g_len), CTAP2_ERR_NOT_ALLOWED);

	/* GetNextAssertion ohne vorheriges GetAssertion */
	start();
	T_EQ("ohne Vorgeschichte", h_call(next, 1, g_body, sizeof(g_body), &g_len),
	     CTAP2_ERR_NOT_ALLOWED);
}

/* Derselbe Dienst, derselbe Nutzer: der alte Eintrag wird ueberschrieben. */
static void test_resident_ueberschreiben(void)
{
	uint8_t id[OSK_MAX_CRED_ID];

	start();
	T_TRUE("erster", create_cred("example.com", "anna", true, false, 0, id) > 0);
	T_EQ("einer im Tresor", (long long)mock_store_count(), 1);
	T_TRUE("zweiter mit gleichem Nutzer",
	       create_cred("example.com", "anna", true, false, 0, id) > 0);
	T_EQ("noch immer einer", (long long)mock_store_count(), 1);
}

static void test_tresor_voll(void)
{
	uint8_t id[OSK_MAX_CRED_ID];
	h_msg_t m;

	start();
	mock_store_set_full(true);
	h_make_credential(&m, "example.com", "anna", true, false, 0, NULL, 0, 0);
	T_EQ("Tresor voll", h_call(m.buf, m.len, g_body, sizeof(g_body), &g_len),
	     CTAP2_ERR_KEY_STORE_FULL);
	mock_store_set_full(false);
	T_TRUE("danach geht es wieder", create_cred("example.com", "anna", true, false, 0, id) > 0);
}

/* ---- credProtect ------------------------------------------------------- */

static void test_cred_protect(void)
{
	uint8_t id[OSK_MAX_CRED_ID];
	h_msg_t m;

	start();
	const size_t len = create_cred("example.com", "anna", false, false, 3, id);
	T_TRUE("angelegt", len == OSK_CREDID_LEN);

	/* Die Stufe steht in der Ausgabe und in der ID. */
	const uint8_t *ad = NULL;
	size_t ad_len = 0;
	T_TRUE("authData", h_get_bytes(g_body, g_len, 0x02, &ad, &ad_len));
	T_TRUE("ED-Flagge gesetzt", (ad[32] & OSK_AD_FLAG_ED) != 0);

	osk_credid_t cid;
	T_EQ("ID gueltig", osk_credid_unpack(id, len, &cid), OSK_OK);
	T_EQ("Stufe 3 gemerkt", cid.cred_protect, 3);

	/* Stufe 3 verlangt Benutzerpruefung: ohne uv gilt der Nachweis als
	 * nicht vorhanden (Spezifikation 6.2.2 Schritt 7.4). */
	build_get_assertion(&m, "example.com", id, len, NULL, 0, 0);
	T_EQ("ohne uv unsichtbar", h_call(m.buf, m.len, g_body, sizeof(g_body), &g_len),
	     CTAP2_ERR_NO_CREDENTIALS);

	/* Unzulaessige Stufe wird abgewiesen. */
	start();
	h_make_credential(&m, "example.com", "anna", false, false, 7, NULL, 0, 0);
	T_EQ("Stufe 7 gibt es nicht", h_call(m.buf, m.len, g_body, sizeof(g_body), &g_len),
	     CTAP2_ERR_INVALID_CBOR);
}

/* ---- hmac-secret ------------------------------------------------------- */

/* Spielt die Seite des Rechners: Schluesseltausch, Salze verschluesseln,
 * Antwort entschluesseln. */
static void test_hmac_secret(void)
{
	uint8_t id[OSK_MAX_CRED_ID];
	osk_cbor_writer_t w;
	h_msg_t m;
	uint8_t platform_pub[64];
	uint8_t z[32];
	uint8_t shared[64];
	uint8_t salt[32];
	uint8_t salt_enc[48];
	size_t salt_enc_len = 0;
	uint8_t salt_auth[32];
	size_t salt_auth_len = 0;

	start();
	const size_t len = create_cred("example.com", "anna", false, true, 0, id);
	T_TRUE("mit hmac-secret angelegt", len == OSK_CREDID_LEN);

	/* Die Ausgabe sagt, dass die Erweiterung verarbeitet wurde. */
	const uint8_t *ad = NULL;
	size_t ad_len = 0;
	T_TRUE("authData", h_get_bytes(g_body, g_len, 0x02, &ad, &ad_len));
	T_TRUE("ED-Flagge", (ad[32] & OSK_AD_FLAG_ED) != 0);

	/* Rechnerseite: eigener Punkt, gemeinsames Geheimnis nach Protokoll 2. */
	for (unsigned i = 0; i < 64; i++)
		platform_pub[i] = (uint8_t)(i + 1);
	mock_se050_ecdh_expect(platform_pub, z);
	{
		static const uint8_t zero[32] = {0};
		osk_hkdf_sha256(zero, 32, z, 32, (const uint8_t *)"CTAP2 HMAC key", 14, shared, 32);
		osk_hkdf_sha256(zero, 32, z, 32, (const uint8_t *)"CTAP2 AES key", 13, shared + 32,
				32);
	}
	memset(salt, 0xa5, sizeof(salt));

	/* encrypt(shared, salt1): Protokoll 2 setzt einen IV davor. */
	{
		uint8_t iv[16];
		memset(iv, 0x11, sizeof(iv));
		memcpy(salt_enc, iv, 16);
		T_EQ("Salz verschluesseln",
		     osk_aes256_cbc_encrypt(shared + 32, iv, salt, salt_enc + 16, 32), OSK_OK);
		salt_enc_len = 48;
		osk_hmac_sha256(shared, 32, salt_enc, salt_enc_len, salt_auth);
		salt_auth_len = 32;
	}

	/* GetAssertion mit der Erweiterung. */
	{
		uint8_t hash[32];
		osk_sha256((const uint8_t *)"clientData", 10, hash);
		m.buf[0] = OSK_CTAP_CMD_GET_ASSERTION;
		osk_cbor_writer_init(&w, m.buf + 1, sizeof(m.buf) - 1);
		osk_cbor_put_map(&w, 4);
		osk_cbor_put_uint(&w, 0x01);
		osk_cbor_put_text(&w, "example.com");
		osk_cbor_put_uint(&w, 0x02);
		osk_cbor_put_bytes(&w, hash, 32);
		osk_cbor_put_uint(&w, 0x03);
		osk_cbor_put_array(&w, 1);
		osk_cbor_put_map(&w, 2);
		osk_cbor_put_text(&w, "id");
		osk_cbor_put_bytes(&w, id, len);
		osk_cbor_put_text(&w, "type");
		osk_cbor_put_text(&w, "public-key");
		osk_cbor_put_uint(&w, 0x04);
		osk_cbor_put_map(&w, 1);
		osk_cbor_put_text(&w, "hmac-secret");
		osk_cbor_put_map(&w, 4);
		osk_cbor_put_uint(&w, 1);
		osk_cbor_put_map(&w, 5);
		osk_cbor_put_uint(&w, 1);
		osk_cbor_put_uint(&w, 2);
		osk_cbor_put_uint(&w, 3);
		osk_cbor_put_int(&w, -25);
		osk_cbor_put_int(&w, -1);
		osk_cbor_put_uint(&w, 1);
		osk_cbor_put_int(&w, -2);
		osk_cbor_put_bytes(&w, platform_pub, 32);
		osk_cbor_put_int(&w, -3);
		osk_cbor_put_bytes(&w, platform_pub + 32, 32);
		osk_cbor_put_uint(&w, 2);
		osk_cbor_put_bytes(&w, salt_enc, salt_enc_len);
		osk_cbor_put_uint(&w, 3);
		osk_cbor_put_bytes(&w, salt_auth, salt_auth_len);
		osk_cbor_put_uint(&w, 4);
		osk_cbor_put_uint(&w, 2);
		T_TRUE("Anfrage passt in den Puffer", osk_cbor_writer_ok(&w));
		m.len = 1 + w.len;
	}
	T_EQ("Anfrage ist kanonisch", osk_cbor_validate(m.buf + 1, m.len - 1), OSK_OK);

	T_EQ("GetAssertion mit hmac-secret", h_call(m.buf, m.len, g_body, sizeof(g_body), &g_len),
	     CTAP2_OK);
	T_EQ("Antwort kanonisch", osk_cbor_validate(g_body, g_len), OSK_OK);

	/* Die Ausgabe steht als Erweiterung im authData. */
	T_TRUE("authData", h_get_bytes(g_body, g_len, 0x02, &ad, &ad_len));
	T_TRUE("ED gesetzt", (ad[32] & OSK_AD_FLAG_ED) != 0);
	T_TRUE("authData laenger als 37", ad_len > 37);

	/* Erweiterungsausgabe entschluesseln und gegen die eigene Rechnung
	 * halten: HMAC-SHA-256(CredRandomWithoutUV, salt1). */
	osk_cbor_reader_t r;
	osk_cbor_reader_init(&r, ad + 37, ad_len - 37);
	size_t count = 0;
	T_EQ("Erweiterungsabbildung", osk_cbor_enter_map(&r, &count), OSK_OK);
	T_EQ("ein Eintrag", (long long)count, 1);
	const char *k = NULL;
	size_t klen = 0;
	T_EQ("Schluessel lesbar", osk_cbor_read_text(&r, &k, &klen), OSK_OK);
	T_TRUE("heisst hmac-secret", klen == 11 && memcmp(k, "hmac-secret", 11) == 0);
	const uint8_t *enc = NULL;
	size_t enc_len = 0;
	T_EQ("Wert lesbar", osk_cbor_read_bytes(&r, &enc, &enc_len), OSK_OK);
	T_EQ("IV plus ein Block", (long long)enc_len, 48);

	uint8_t got[32];
	T_EQ("entschluesseln", osk_aes256_cbc_decrypt(shared + 32, enc, enc + 16, got, 32), OSK_OK);

	/* Dasselbe selbst rechnen: CredRandomWithoutUV kommt aus der
	 * Ableitung ueber das Salz der Credential-ID. */
	osk_credid_t cid;
	T_EQ("ID oeffnen", osk_credid_unpack(id, len, &cid), OSK_OK);
	uint8_t cred_random[32], want[32];
	T_EQ("CredRandom",
	     osk_ctap_key_secret(OSK_INFO_HMAC_NOUV, cid.salt, cid.rp_id_hash, cred_random),
	     OSK_OK);
	osk_hmac_sha256(cred_random, 32, salt, 32, want);
	T_TRUE("hmac-secret stimmt", memcmp(got, want, 32) == 0);

	/* Ein Nachweis ohne hmac-secret liefert keine Ausgabe. */
	start();
	const size_t len2 = create_cred("example.com", "anna", false, false, 0, id);
	T_TRUE("ohne hmac-secret angelegt", len2 == OSK_CREDID_LEN);
	/* dieselbe Anfrage noch einmal, nur mit der neuen ID */
	{
		uint8_t hash[32];
		osk_sha256((const uint8_t *)"clientData", 10, hash);
		m.buf[0] = OSK_CTAP_CMD_GET_ASSERTION;
		osk_cbor_writer_init(&w, m.buf + 1, sizeof(m.buf) - 1);
		osk_cbor_put_map(&w, 4);
		osk_cbor_put_uint(&w, 0x01);
		osk_cbor_put_text(&w, "example.com");
		osk_cbor_put_uint(&w, 0x02);
		osk_cbor_put_bytes(&w, hash, 32);
		osk_cbor_put_uint(&w, 0x03);
		osk_cbor_put_array(&w, 1);
		osk_cbor_put_map(&w, 2);
		osk_cbor_put_text(&w, "id");
		osk_cbor_put_bytes(&w, id, len2);
		osk_cbor_put_text(&w, "type");
		osk_cbor_put_text(&w, "public-key");
		osk_cbor_put_uint(&w, 0x04);
		osk_cbor_put_map(&w, 1);
		osk_cbor_put_text(&w, "hmac-secret");
		osk_cbor_put_map(&w, 4);
		osk_cbor_put_uint(&w, 1);
		osk_cbor_put_map(&w, 5);
		osk_cbor_put_uint(&w, 1);
		osk_cbor_put_uint(&w, 2);
		osk_cbor_put_uint(&w, 3);
		osk_cbor_put_int(&w, -25);
		osk_cbor_put_int(&w, -1);
		osk_cbor_put_uint(&w, 1);
		osk_cbor_put_int(&w, -2);
		osk_cbor_put_bytes(&w, platform_pub, 32);
		osk_cbor_put_int(&w, -3);
		osk_cbor_put_bytes(&w, platform_pub + 32, 32);
		osk_cbor_put_uint(&w, 2);
		osk_cbor_put_bytes(&w, salt_enc, salt_enc_len);
		osk_cbor_put_uint(&w, 3);
		osk_cbor_put_bytes(&w, salt_auth, salt_auth_len);
		osk_cbor_put_uint(&w, 4);
		osk_cbor_put_uint(&w, 2);
		m.len = 1 + w.len;
	}
	T_EQ("geht trotzdem durch", h_call(m.buf, m.len, g_body, sizeof(g_body), &g_len), CTAP2_OK);
	T_TRUE("authData", h_get_bytes(g_body, g_len, 0x02, &ad, &ad_len));
	T_EQ("keine Erweiterungsausgabe", (long long)ad_len, 37);

	/* Falscher saltAuth: die Anfrage fliegt auf. */
	salt_auth[0] ^= 0xff;
	{
		uint8_t hash[32];
		osk_sha256((const uint8_t *)"clientData", 10, hash);
		m.buf[0] = OSK_CTAP_CMD_GET_ASSERTION;
		osk_cbor_writer_init(&w, m.buf + 1, sizeof(m.buf) - 1);
		osk_cbor_put_map(&w, 4);
		osk_cbor_put_uint(&w, 0x01);
		osk_cbor_put_text(&w, "example.com");
		osk_cbor_put_uint(&w, 0x02);
		osk_cbor_put_bytes(&w, hash, 32);
		osk_cbor_put_uint(&w, 0x03);
		osk_cbor_put_array(&w, 1);
		osk_cbor_put_map(&w, 2);
		osk_cbor_put_text(&w, "id");
		osk_cbor_put_bytes(&w, id, len2);
		osk_cbor_put_text(&w, "type");
		osk_cbor_put_text(&w, "public-key");
		osk_cbor_put_uint(&w, 0x04);
		osk_cbor_put_map(&w, 1);
		osk_cbor_put_text(&w, "hmac-secret");
		osk_cbor_put_map(&w, 4);
		osk_cbor_put_uint(&w, 1);
		osk_cbor_put_map(&w, 5);
		osk_cbor_put_uint(&w, 1);
		osk_cbor_put_uint(&w, 2);
		osk_cbor_put_uint(&w, 3);
		osk_cbor_put_int(&w, -25);
		osk_cbor_put_int(&w, -1);
		osk_cbor_put_uint(&w, 1);
		osk_cbor_put_int(&w, -2);
		osk_cbor_put_bytes(&w, platform_pub, 32);
		osk_cbor_put_int(&w, -3);
		osk_cbor_put_bytes(&w, platform_pub + 32, 32);
		osk_cbor_put_uint(&w, 2);
		osk_cbor_put_bytes(&w, salt_enc, salt_enc_len);
		osk_cbor_put_uint(&w, 3);
		osk_cbor_put_bytes(&w, salt_auth, salt_auth_len);
		osk_cbor_put_uint(&w, 4);
		osk_cbor_put_uint(&w, 2);
		m.len = 1 + w.len;
	}
	/* Mit einem Nachweis, der hmac-secret hat, muss das ein Fehler sein. */
	uint8_t id3[OSK_MAX_CRED_ID];
	start();
	const size_t len3 = create_cred("example.com", "anna", false, true, 0, id3);
	T_TRUE("mit hmac-secret", len3 == OSK_CREDID_LEN);
	memcpy(m.buf + 1 + 0, m.buf + 1, 0); /* nichts, nur Klarheit */
	{
		uint8_t hash[32];
		osk_sha256((const uint8_t *)"clientData", 10, hash);
		m.buf[0] = OSK_CTAP_CMD_GET_ASSERTION;
		osk_cbor_writer_init(&w, m.buf + 1, sizeof(m.buf) - 1);
		osk_cbor_put_map(&w, 4);
		osk_cbor_put_uint(&w, 0x01);
		osk_cbor_put_text(&w, "example.com");
		osk_cbor_put_uint(&w, 0x02);
		osk_cbor_put_bytes(&w, hash, 32);
		osk_cbor_put_uint(&w, 0x03);
		osk_cbor_put_array(&w, 1);
		osk_cbor_put_map(&w, 2);
		osk_cbor_put_text(&w, "id");
		osk_cbor_put_bytes(&w, id3, len3);
		osk_cbor_put_text(&w, "type");
		osk_cbor_put_text(&w, "public-key");
		osk_cbor_put_uint(&w, 0x04);
		osk_cbor_put_map(&w, 1);
		osk_cbor_put_text(&w, "hmac-secret");
		osk_cbor_put_map(&w, 4);
		osk_cbor_put_uint(&w, 1);
		osk_cbor_put_map(&w, 5);
		osk_cbor_put_uint(&w, 1);
		osk_cbor_put_uint(&w, 2);
		osk_cbor_put_uint(&w, 3);
		osk_cbor_put_int(&w, -25);
		osk_cbor_put_int(&w, -1);
		osk_cbor_put_uint(&w, 1);
		osk_cbor_put_int(&w, -2);
		osk_cbor_put_bytes(&w, platform_pub, 32);
		osk_cbor_put_int(&w, -3);
		osk_cbor_put_bytes(&w, platform_pub + 32, 32);
		osk_cbor_put_uint(&w, 2);
		osk_cbor_put_bytes(&w, salt_enc, salt_enc_len);
		osk_cbor_put_uint(&w, 3);
		osk_cbor_put_bytes(&w, salt_auth, salt_auth_len);
		osk_cbor_put_uint(&w, 4);
		osk_cbor_put_uint(&w, 2);
		m.len = 1 + w.len;
	}
	T_EQ("falscher saltAuth fliegt auf", h_call(m.buf, m.len, g_body, sizeof(g_body), &g_len),
	     CTAP2_ERR_PIN_AUTH_INVALID);
}

/* ---- minPinLength ------------------------------------------------------ */

static void test_min_pin_length(void)
{
	h_msg_t m;
	osk_cbor_writer_t w;
	uint8_t hash[32];

	start();
	osk_sha256((const uint8_t *)"clientData", 10, hash);

	/* Eine Anfrage mit minPinLength: true */
	m.buf[0] = OSK_CTAP_CMD_MAKE_CREDENTIAL;
	osk_cbor_writer_init(&w, m.buf + 1, sizeof(m.buf) - 1);
	osk_cbor_put_map(&w, 5);
	osk_cbor_put_uint(&w, 1);
	osk_cbor_put_bytes(&w, hash, 32);
	osk_cbor_put_uint(&w, 2);
	osk_cbor_put_map(&w, 1);
	osk_cbor_put_text(&w, "id");
	osk_cbor_put_text(&w, "firma.example");
	osk_cbor_put_uint(&w, 3);
	osk_cbor_put_map(&w, 1);
	osk_cbor_put_text(&w, "id");
	osk_cbor_put_bytes(&w, (const uint8_t *)"u", 1);
	osk_cbor_put_uint(&w, 4);
	osk_cbor_put_array(&w, 1);
	osk_cbor_put_map(&w, 2);
	osk_cbor_put_text(&w, "alg");
	osk_cbor_put_int(&w, -7);
	osk_cbor_put_text(&w, "type");
	osk_cbor_put_text(&w, "public-key");
	osk_cbor_put_uint(&w, 6);
	osk_cbor_put_map(&w, 1);
	osk_cbor_put_text(&w, "minPinLength");
	osk_cbor_put_bool(&w, true);
	m.len = 1 + w.len;

	/* Ohne Freigabe der RP-ID bleibt die Erweiterung stumm. */
	T_EQ("angelegt", h_call(m.buf, m.len, g_body, sizeof(g_body), &g_len), CTAP2_OK);
	const uint8_t *ad = NULL;
	size_t ad_len = 0;
	T_TRUE("authData", h_get_bytes(g_body, g_len, 0x02, &ad, &ad_len));
	const size_t id_len = ((size_t)ad[53] << 8) | ad[54];
	osk_cbor_reader_t r;
	osk_cbor_reader_init(&r, ad + 55 + id_len, ad_len - 55 - id_len);
	T_EQ("nur der COSE_Key, keine Erweiterung", osk_cbor_skip(&r), OSK_OK);
	T_EQ("nichts dahinter", (long long)osk_cbor_left(&r), 0);
	T_TRUE("ED nicht gesetzt", (ad[32] & OSK_AD_FLAG_ED) == 0);

	/* Nach der Freigabe kommt die Laenge heraus. */
	T_EQ("Dienst freigeben", osk_ctap_minpin_rp_add("firma.example"), OSK_OK);
	T_EQ("angelegt", h_call(m.buf, m.len, g_body, sizeof(g_body), &g_len), CTAP2_OK);
	T_TRUE("authData", h_get_bytes(g_body, g_len, 0x02, &ad, &ad_len));
	T_TRUE("ED gesetzt", (ad[32] & OSK_AD_FLAG_ED) != 0);
	const size_t id_len2 = ((size_t)ad[53] << 8) | ad[54];
	osk_cbor_reader_init(&r, ad + 55 + id_len2, ad_len - 55 - id_len2);
	T_EQ("COSE_Key ueberspringen", osk_cbor_skip(&r), OSK_OK);
	size_t count = 0;
	T_EQ("Erweiterungsabbildung", osk_cbor_enter_map(&r, &count), OSK_OK);
	T_EQ("ein Eintrag", (long long)count, 1);
	const char *k = NULL;
	size_t klen = 0;
	T_EQ("Name lesbar", osk_cbor_read_text(&r, &k, &klen), OSK_OK);
	T_TRUE("heisst minPinLength", klen == 12 && memcmp(k, "minPinLength", 12) == 0);
	uint64_t v = 0;
	T_EQ("Wert lesbar", osk_cbor_read_uint(&r, &v), OSK_OK);
	T_EQ("Vorgabe vier Zeichen", (long long)v, OSK_PIN_MIN_DEFAULT);
	osk_ctap_minpin_rp_clear();
}

/* ---- largeBlob --------------------------------------------------------- */

static void test_large_blob(void)
{
	uint8_t id[OSK_MAX_CRED_ID];
	h_msg_t m;
	osk_cbor_writer_t w;
	uint8_t hash[32];

	start();
	osk_sha256((const uint8_t *)"clientData", 10, hash);

	/* Anlegen mit largeBlob support: preferred */
	m.buf[0] = OSK_CTAP_CMD_MAKE_CREDENTIAL;
	osk_cbor_writer_init(&w, m.buf + 1, sizeof(m.buf) - 1);
	osk_cbor_put_map(&w, 6);
	osk_cbor_put_uint(&w, 1);
	osk_cbor_put_bytes(&w, hash, 32);
	osk_cbor_put_uint(&w, 2);
	osk_cbor_put_map(&w, 1);
	osk_cbor_put_text(&w, "id");
	osk_cbor_put_text(&w, "example.com");
	osk_cbor_put_uint(&w, 3);
	osk_cbor_put_map(&w, 1);
	osk_cbor_put_text(&w, "id");
	osk_cbor_put_bytes(&w, (const uint8_t *)"u", 1);
	osk_cbor_put_uint(&w, 4);
	osk_cbor_put_array(&w, 1);
	osk_cbor_put_map(&w, 2);
	osk_cbor_put_text(&w, "alg");
	osk_cbor_put_int(&w, -7);
	osk_cbor_put_text(&w, "type");
	osk_cbor_put_text(&w, "public-key");
	osk_cbor_put_uint(&w, 6);
	osk_cbor_put_map(&w, 1);
	osk_cbor_put_text(&w, "largeBlob");
	osk_cbor_put_map(&w, 1);
	osk_cbor_put_text(&w, "support");
	osk_cbor_put_text(&w, "preferred");
	osk_cbor_put_uint(&w, 7);
	osk_cbor_put_map(&w, 1);
	osk_cbor_put_text(&w, "rk");
	osk_cbor_put_bool(&w, true);
	m.len = 1 + w.len;

	T_EQ("angelegt", h_call(m.buf, m.len, g_body, sizeof(g_body), &g_len), CTAP2_OK);
	T_EQ("Antwort kanonisch", osk_cbor_validate(g_body, g_len), OSK_OK);
	/* Die Antwort auf largeBlob steht in den ungesignierten Ausgaben (0x06). */
	osk_cbor_reader_t r;
	T_TRUE("unsignedExtensionOutputs vorhanden", h_find(g_body, g_len, 0x06, &r));

	const uint8_t *ad = NULL;
	size_t ad_len = 0;
	T_TRUE("authData", h_get_bytes(g_body, g_len, 0x02, &ad, &ad_len));
	const size_t cid_len = ((size_t)ad[53] << 8) | ad[54];
	memcpy(id, ad + 55, cid_len);

	/* Schreiben: mit allowList, write und originalSize. */
	const uint8_t blob[8] = {1, 2, 3, 4, 5, 6, 7, 8};
	m.buf[0] = OSK_CTAP_CMD_GET_ASSERTION;
	osk_cbor_writer_init(&w, m.buf + 1, sizeof(m.buf) - 1);
	osk_cbor_put_map(&w, 4);
	osk_cbor_put_uint(&w, 0x01);
	osk_cbor_put_text(&w, "example.com");
	osk_cbor_put_uint(&w, 0x02);
	osk_cbor_put_bytes(&w, hash, 32);
	osk_cbor_put_uint(&w, 0x03);
	osk_cbor_put_array(&w, 1);
	osk_cbor_put_map(&w, 2);
	osk_cbor_put_text(&w, "id");
	osk_cbor_put_bytes(&w, id, cid_len);
	osk_cbor_put_text(&w, "type");
	osk_cbor_put_text(&w, "public-key");
	osk_cbor_put_uint(&w, 0x04);
	osk_cbor_put_map(&w, 1);
	osk_cbor_put_text(&w, "largeBlob");
	osk_cbor_put_map(&w, 2);
	osk_cbor_put_text(&w, "write");
	osk_cbor_put_bytes(&w, blob, sizeof(blob));
	osk_cbor_put_text(&w, "originalSize");
	osk_cbor_put_uint(&w, 99);
	m.len = 1 + w.len;
	T_EQ("Anfrage kanonisch", osk_cbor_validate(m.buf + 1, m.len - 1), OSK_OK);

	T_EQ("schreiben", h_call(m.buf, m.len, g_body, sizeof(g_body), &g_len), CTAP2_OK);
	T_EQ("Antwort kanonisch", osk_cbor_validate(g_body, g_len), OSK_OK);
	T_TRUE("ungesignierte Ausgabe", h_find(g_body, g_len, 0x08, &r));

	/* Lesen: read true */
	m.buf[0] = OSK_CTAP_CMD_GET_ASSERTION;
	osk_cbor_writer_init(&w, m.buf + 1, sizeof(m.buf) - 1);
	osk_cbor_put_map(&w, 4);
	osk_cbor_put_uint(&w, 0x01);
	osk_cbor_put_text(&w, "example.com");
	osk_cbor_put_uint(&w, 0x02);
	osk_cbor_put_bytes(&w, hash, 32);
	osk_cbor_put_uint(&w, 0x03);
	osk_cbor_put_array(&w, 1);
	osk_cbor_put_map(&w, 2);
	osk_cbor_put_text(&w, "id");
	osk_cbor_put_bytes(&w, id, cid_len);
	osk_cbor_put_text(&w, "type");
	osk_cbor_put_text(&w, "public-key");
	osk_cbor_put_uint(&w, 0x04);
	osk_cbor_put_map(&w, 1);
	osk_cbor_put_text(&w, "largeBlob");
	osk_cbor_put_map(&w, 1);
	osk_cbor_put_text(&w, "read");
	osk_cbor_put_bool(&w, true);
	m.len = 1 + w.len;

	T_EQ("lesen", h_call(m.buf, m.len, g_body, sizeof(g_body), &g_len), CTAP2_OK);
	T_TRUE("ungesignierte Ausgabe", h_find(g_body, g_len, 0x08, &r));
	/* {"largeBlob": {"blob": ..., "originalSize": 99}} */
	size_t count = 0;
	T_EQ("Abbildung", osk_cbor_enter_map(&r, &count), OSK_OK);
	const char *k = NULL;
	size_t klen = 0;
	T_EQ("Name", osk_cbor_read_text(&r, &k, &klen), OSK_OK);
	T_TRUE("heisst largeBlob", klen == 9 && memcmp(k, "largeBlob", 9) == 0);
	T_EQ("innen", osk_cbor_enter_map(&r, &count), OSK_OK);
	T_EQ("zwei Eintraege", (long long)count, 2);
	T_EQ("blob", osk_cbor_read_text(&r, &k, &klen), OSK_OK);
	const uint8_t *got = NULL;
	size_t got_len = 0;
	T_EQ("Inhalt", osk_cbor_read_bytes(&r, &got, &got_len), OSK_OK);
	T_TRUE("derselbe Inhalt", got_len == sizeof(blob) && memcmp(got, blob, got_len) == 0);
	T_EQ("originalSize", osk_cbor_read_text(&r, &k, &klen), OSK_OK);
	uint64_t v = 0;
	T_EQ("Wert", osk_cbor_read_uint(&r, &v), OSK_OK);
	T_EQ("99", (long long)v, 99);

	/* read und write zusammen ist nach der CDDL verboten. */
	m.buf[0] = OSK_CTAP_CMD_GET_ASSERTION;
	osk_cbor_writer_init(&w, m.buf + 1, sizeof(m.buf) - 1);
	osk_cbor_put_map(&w, 3);
	osk_cbor_put_uint(&w, 0x01);
	osk_cbor_put_text(&w, "example.com");
	osk_cbor_put_uint(&w, 0x02);
	osk_cbor_put_bytes(&w, hash, 32);
	osk_cbor_put_uint(&w, 0x04);
	osk_cbor_put_map(&w, 1);
	osk_cbor_put_text(&w, "largeBlob");
	osk_cbor_put_map(&w, 3);
	osk_cbor_put_text(&w, "read");
	osk_cbor_put_bool(&w, true);
	osk_cbor_put_text(&w, "write");
	osk_cbor_put_bytes(&w, blob, sizeof(blob));
	osk_cbor_put_text(&w, "originalSize");
	osk_cbor_put_uint(&w, 99);
	m.len = 1 + w.len;
	T_EQ("lesen und schreiben zugleich", h_call(m.buf, m.len, g_body, sizeof(g_body), &g_len),
	     CTAP2_ERR_INVALID_CBOR);
}

/* ---- Selection und Reset ----------------------------------------------- */

static void test_selection(void)
{
	start();
	const uint8_t msg[1] = {OSK_CTAP_CMD_SELECTION};

	T_EQ("Selection ok", h_call(msg, 1, g_body, sizeof(g_body), &g_len), CTAP2_OK);
	T_EQ("keine Nutzdaten in der Antwort", (long long)g_len, 0);

	mock_ui_set_confirm(OSK_ERR_DENIED);
	T_EQ("abgelehnt", h_call(msg, 1, g_body, sizeof(g_body), &g_len),
	     CTAP2_ERR_OPERATION_DENIED);
	mock_ui_set_confirm(OSK_ERR_TIMEOUT);
	T_EQ("Zeit abgelaufen", h_call(msg, 1, g_body, sizeof(g_body), &g_len),
	     CTAP2_ERR_USER_ACTION_TIMEOUT);
}

static void test_reset(void)
{
	uint8_t id[OSK_MAX_CRED_ID];
	h_msg_t m;

	start();
	const size_t len = create_cred("example.com", "anna", true, false, 0, id);
	T_TRUE("angelegt", len == OSK_CREDID_LEN);
	T_EQ("im Tresor", (long long)mock_store_count(), 1);

	const uint8_t msg[1] = {OSK_CTAP_CMD_RESET};
	T_EQ("Reset ok", h_call(msg, 1, g_body, sizeof(g_body), &g_len), CTAP2_OK);
	T_EQ("Tresor leer", (long long)mock_store_count(), 0);

	/* Die alte Credential-ID ist danach wertlos: das Grundgeheimnis ist neu. */
	build_get_assertion(&m, "example.com", id, len, NULL, 0, 0);
	T_EQ("alter Nachweis weg", h_call(m.buf, m.len, g_body, sizeof(g_body), &g_len),
	     CTAP2_ERR_NO_CREDENTIALS);

	/* Ohne Tastendruck kein Reset. */
	mock_ui_set_confirm(OSK_ERR_DENIED);
	T_EQ("abgelehnt", h_call(msg, 1, g_body, sizeof(g_body), &g_len),
	     CTAP2_ERR_OPERATION_DENIED);
}

static void test_reset_ueber_nfc(void)
{
	static uint8_t out[256];
	osk_request_t req;
	osk_response_t res;

	start();
	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));
	const uint8_t msg[1] = {OSK_CTAP_CMD_RESET};
	req.link = OSK_LINK_NFC;
	req.data = msg;
	req.len = 1;
	res.data = out;
	res.cap = sizeof(out);

	T_EQ("Aufruf laeuft", osk_ctap_handle(&req, &res), OSK_OK);
	T_EQ("Reset ueber NFC gesperrt", out[0], CTAP2_ERR_OPERATION_DENIED);
	T_EQ("und es wurde nicht gefragt", (long long)mock_ui_confirm_count(), 0);
}

/* ---- Grenzen ----------------------------------------------------------- */

static void test_grenzen(void)
{
	h_msg_t m;
	osk_cbor_writer_t w;
	uint8_t hash[32];

	start();
	osk_sha256((const uint8_t *)"clientData", 10, hash);

	/* Mehr Einträge in der allowList als wir annehmen. */
	m.buf[0] = OSK_CTAP_CMD_GET_ASSERTION;
	osk_cbor_writer_init(&w, m.buf + 1, sizeof(m.buf) - 1);
	osk_cbor_put_map(&w, 3);
	osk_cbor_put_uint(&w, 0x01);
	osk_cbor_put_text(&w, "example.com");
	osk_cbor_put_uint(&w, 0x02);
	osk_cbor_put_bytes(&w, hash, 32);
	osk_cbor_put_uint(&w, 0x03);
	osk_cbor_put_array(&w, OSK_MAX_LIST + 1);
	for (unsigned i = 0; i <= OSK_MAX_LIST; i++) {
		const uint8_t fake[4] = {1, 2, 3, (uint8_t)i};
		osk_cbor_put_map(&w, 2);
		osk_cbor_put_text(&w, "id");
		osk_cbor_put_bytes(&w, fake, sizeof(fake));
		osk_cbor_put_text(&w, "type");
		osk_cbor_put_text(&w, "public-key");
	}
	m.len = 1 + w.len;
	T_EQ("zu viele Eintraege", h_call(m.buf, m.len, g_body, sizeof(g_body), &g_len),
	     CTAP2_ERR_LIMIT_EXCEEDED);

	/* Leere allowList ist verboten. */
	m.buf[0] = OSK_CTAP_CMD_GET_ASSERTION;
	osk_cbor_writer_init(&w, m.buf + 1, sizeof(m.buf) - 1);
	osk_cbor_put_map(&w, 3);
	osk_cbor_put_uint(&w, 0x01);
	osk_cbor_put_text(&w, "example.com");
	osk_cbor_put_uint(&w, 0x02);
	osk_cbor_put_bytes(&w, hash, 32);
	osk_cbor_put_uint(&w, 0x03);
	osk_cbor_put_array(&w, 0);
	m.len = 1 + w.len;
	T_EQ("leere allowList", h_call(m.buf, m.len, g_body, sizeof(g_body), &g_len),
	     CTAP1_ERR_INVALID_PARAMETER);

	/* Der Schalter rk ist in GetAssertion nicht erlaubt. */
	m.buf[0] = OSK_CTAP_CMD_GET_ASSERTION;
	osk_cbor_writer_init(&w, m.buf + 1, sizeof(m.buf) - 1);
	osk_cbor_put_map(&w, 3);
	osk_cbor_put_uint(&w, 0x01);
	osk_cbor_put_text(&w, "example.com");
	osk_cbor_put_uint(&w, 0x02);
	osk_cbor_put_bytes(&w, hash, 32);
	osk_cbor_put_uint(&w, 0x05);
	osk_cbor_put_map(&w, 1);
	osk_cbor_put_text(&w, "rk");
	osk_cbor_put_bool(&w, true);
	m.len = 1 + w.len;
	T_EQ("rk in GetAssertion", h_call(m.buf, m.len, g_body, sizeof(g_body), &g_len),
	     CTAP2_ERR_UNSUPPORTED_OPTION);

	/* up: false in MakeCredential ist verboten. */
	m.buf[0] = OSK_CTAP_CMD_MAKE_CREDENTIAL;
	osk_cbor_writer_init(&w, m.buf + 1, sizeof(m.buf) - 1);
	osk_cbor_put_map(&w, 5);
	osk_cbor_put_uint(&w, 1);
	osk_cbor_put_bytes(&w, hash, 32);
	osk_cbor_put_uint(&w, 2);
	osk_cbor_put_map(&w, 1);
	osk_cbor_put_text(&w, "id");
	osk_cbor_put_text(&w, "example.com");
	osk_cbor_put_uint(&w, 3);
	osk_cbor_put_map(&w, 1);
	osk_cbor_put_text(&w, "id");
	osk_cbor_put_bytes(&w, (const uint8_t *)"u", 1);
	osk_cbor_put_uint(&w, 4);
	osk_cbor_put_array(&w, 1);
	osk_cbor_put_map(&w, 2);
	osk_cbor_put_text(&w, "alg");
	osk_cbor_put_int(&w, -7);
	osk_cbor_put_text(&w, "type");
	osk_cbor_put_text(&w, "public-key");
	osk_cbor_put_uint(&w, 7);
	osk_cbor_put_map(&w, 1);
	osk_cbor_put_text(&w, "up");
	osk_cbor_put_bool(&w, false);
	m.len = 1 + w.len;
	T_EQ("up false", h_call(m.buf, m.len, g_body, sizeof(g_body), &g_len),
	     CTAP2_ERR_INVALID_OPTION);

	/* Nachricht groesser als maxMsgSize. */
	static uint8_t big[OSK_MAX_MSG + 8];
	memset(big, 0, sizeof(big));
	big[0] = OSK_CTAP_CMD_MAKE_CREDENTIAL;
	T_EQ("zu grosse Nachricht", h_call(big, sizeof(big), g_body, sizeof(g_body), &g_len),
	     CTAP2_ERR_REQUEST_TOO_LARGE);

	/* Leere Nachricht */
	T_EQ("leere Nachricht", h_call(big, 0, g_body, sizeof(g_body), &g_len),
	     CTAP1_ERR_INVALID_LENGTH);
}

/* Die Kennung im Tresor ist dafuer da, dass das Suchen nichts oeffnet, was
 * nicht gemeint ist: sonst braeuchte jede Anmeldung eine Freigabe je
 * fremdem Eintrag. Dieser Fall zaehlt mit. */
static void test_suche_oeffnet_nur_den_treffer(void)
{
	uint8_t id[OSK_MAX_CRED_ID];
	h_msg_t m;

	start();
	T_TRUE("Nachweis fuer example.com",
	       create_cred("example.com", "anna", true, false, 0, id) == OSK_CREDID_LEN);
	T_TRUE("Nachweis fuer bank.example",
	       create_cred("bank.example", "berta", true, false, 0, id) == OSK_CREDID_LEN);
	T_TRUE("Nachweis fuer forum.example",
	       create_cred("forum.example", "clara", true, false, 0, id) == OSK_CREDID_LEN);
	T_EQ("drei im Tresor", (long long)mock_store_count(), 3);

	/* Jetzt bei einem der drei anmelden, ohne allowList. */
	const unsigned before = mock_store_reveal_count();
	build_get_assertion(&m, "bank.example", NULL, 0, NULL, 0, 0);
	T_EQ("GetAssertion ok", h_call(m.buf, m.len, g_body, sizeof(g_body), &g_len), CTAP2_OK);
	const unsigned opened = mock_store_reveal_count() - before;

	/* Genau einer darf geoeffnet worden sein - der gesuchte. Ohne die
	 * Kennung waeren es alle drei gewesen. */
	T_EQ("nur ein Eintrag geoeffnet", (long long)opened, 1);

	/* Und es war der richtige: die Nutzerkennung in der Antwort gehoert
	 * zu berta. */
	osk_cbor_reader_t r;
	T_TRUE("user vorhanden", h_find(g_body, g_len, 0x04, &r));
	size_t count = 0;
	T_EQ("Abbildung", osk_cbor_enter_map(&r, &count), OSK_OK);
	const char *k = NULL;
	size_t klen = 0;
	T_EQ("Schluessel", osk_cbor_read_text(&r, &k, &klen), OSK_OK);
	const uint8_t *uid = NULL;
	size_t uid_len = 0;
	T_EQ("Kennung lesbar", osk_cbor_read_bytes(&r, &uid, &uid_len), OSK_OK);
	T_TRUE("gehoert zu berta", uid_len == 5 && memcmp(uid, "berta", 5) == 0);

	/* Ein Dienst, zu dem nichts im Tresor liegt, oeffnet gar nichts. */
	const unsigned before2 = mock_store_reveal_count();
	build_get_assertion(&m, "fremd.example", NULL, 0, NULL, 0, 0);
	T_EQ("nichts gefunden", h_call(m.buf, m.len, g_body, sizeof(g_body), &g_len),
	     CTAP2_ERR_NO_CREDENTIALS);
	T_EQ("und nichts geoeffnet", (long long)(mock_store_reveal_count() - before2), 0);
}

int main(void)
{
	test_get_info();
	test_make_credential();
	test_make_credential_fehler();
	test_get_assertion();
	test_resident_und_next();
	test_suche_oeffnet_nur_den_treffer();
	test_resident_ueberschreiben();
	test_tresor_voll();
	test_cred_protect();
	test_hmac_secret();
	test_min_pin_length();
	test_large_blob();
	test_selection();
	test_reset();
	test_reset_ueber_nfc();
	test_grenzen();
	return t_report("ctap");
}
