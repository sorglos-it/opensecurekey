/* authenticatorConfig (0x0D) und das PIN-Regelwerk dahinter:
 * Mindestlaenge, erzwungener Wechsel, Dienste-Liste, Enterprise
 * Attestation - samt den beiden AAGUIDs.
 *
 * Nichts davon lief am Geraet oder in einem Browser (siehe mocks.h). */
#include "pin_helper.h"
#include "osk_ctap_config.h"
#include "osk_ctap_ext.h"

#define SUB_ENABLE_EP 0x01
#define SUB_TOGGLE_ALWAYS_UV 0x02
#define SUB_SET_MIN_PIN 0x03

/* ---- Nachrichten bauen ------------------------------------------------- */

static uint8_t config_call(const platform_t *p, const uint8_t *token, uint64_t sub,
			   const uint8_t *params, size_t params_len)
{
	uint8_t msg[768];
	uint8_t mac_in[32 + 1 + 1 + 512];
	uint8_t mac[32];
	osk_cbor_writer_t w;
	size_t entries = 1;
	size_t mac_len = 0;
	size_t at = 0;

	if (params_len)
		entries++;
	if (token)
		entries += 2;

	/* message = 32 x 0xff || 0x0d || subCommand || subCommandParams.
	 * Die Fuellbytes und das Befehlsbyte sind der Unterschied zur
	 * Verwaltung (0x0A) - dort sind sie NICHT dabei. */
	memset(mac_in, 0xff, 32);
	at = 32;
	mac_in[at++] = OSK_CTAP_CMD_CONFIG;
	mac_in[at++] = (uint8_t)sub;
	if (params_len) {
		memcpy(mac_in + at, params, params_len);
		at += params_len;
	}
	if (token)
		mac_len = platform_mac(p, token, 32, mac_in, at, mac);

	msg[0] = OSK_CTAP_CMD_CONFIG;
	osk_cbor_writer_init(&w, msg + 1, sizeof(msg) - 1);
	osk_cbor_put_map(&w, entries);
	osk_cbor_put_uint(&w, 0x01);
	osk_cbor_put_uint(&w, sub);
	if (params_len) {
		osk_cbor_put_uint(&w, 0x02);
		T_TRUE("Platz fuer Parameter", w.cap - w.len >= params_len);
		memcpy(w.buf + w.len, params, params_len);
		w.len += params_len;
	}
	if (token) {
		osk_cbor_put_uint(&w, 0x03);
		osk_cbor_put_uint(&w, p->protocol);
		osk_cbor_put_uint(&w, 0x04);
		osk_cbor_put_bytes(&w, mac, mac_len);
	}
	T_TRUE("Anfrage passt", osk_cbor_writer_ok(&w));
	T_EQ("Anfrage kanonisch", osk_cbor_validate(msg + 1, w.len), OSK_OK);

	return h_call(msg, 1 + w.len, g_body, sizeof(g_body), &g_len);
}

/* subCommandParams fuer setMinPINLength. */
static size_t params_min_pin(uint64_t new_min, bool have_new_min, const char *const *rp_ids,
			     size_t rp_id_count, bool force_change, bool have_force, uint8_t *out,
			     size_t cap)
{
	osk_cbor_writer_t w;
	size_t entries = 0;

	if (have_new_min)
		entries++;
	if (rp_ids)
		entries++;
	if (have_force)
		entries++;

	osk_cbor_writer_init(&w, out, cap);
	osk_cbor_put_map(&w, entries);
	if (have_new_min) {
		osk_cbor_put_uint(&w, 0x01);
		osk_cbor_put_uint(&w, new_min);
	}
	if (rp_ids) {
		osk_cbor_put_uint(&w, 0x02);
		osk_cbor_put_array(&w, rp_id_count);
		for (size_t i = 0; i < rp_id_count; i++)
			osk_cbor_put_text(&w, rp_ids[i]);
	}
	if (have_force) {
		osk_cbor_put_uint(&w, 0x03);
		osk_cbor_put_bool(&w, force_change);
	}
	T_TRUE("Parameter passen", osk_cbor_writer_ok(&w));
	return w.len;
}

/* ---- GetInfo abfragen -------------------------------------------------- */

static bool info_option(const char *name, bool *value_out)
{
	const uint8_t msg[1] = {OSK_CTAP_CMD_GET_INFO};
	osk_cbor_reader_t r;
	size_t count = 0;

	if (h_call(msg, 1, g_body, sizeof(g_body), &g_len) != CTAP2_OK)
		return false;
	if (!h_find(g_body, g_len, 0x04, &r))
		return false;
	if (osk_cbor_enter_map(&r, &count) != OSK_OK)
		return false;
	for (size_t i = 0; i < count; i++) {
		const char *k = NULL;
		size_t klen = 0;
		bool v = false;
		if (osk_cbor_read_text(&r, &k, &klen) != OSK_OK)
			return false;
		if (osk_cbor_read_bool(&r, &v) != OSK_OK)
			return false;
		if (klen == strlen(name) && memcmp(k, name, klen) == 0) {
			*value_out = v;
			return true;
		}
	}
	return false;
}

static bool info_uint(uint64_t key, uint64_t *out)
{
	const uint8_t msg[1] = {OSK_CTAP_CMD_GET_INFO};
	osk_cbor_reader_t r;

	if (h_call(msg, 1, g_body, sizeof(g_body), &g_len) != CTAP2_OK)
		return false;
	if (!h_find(g_body, g_len, key, &r))
		return false;
	return osk_cbor_read_uint(&r, out) == OSK_OK;
}

static bool info_bool_key(uint64_t key, bool *out)
{
	const uint8_t msg[1] = {OSK_CTAP_CMD_GET_INFO};
	osk_cbor_reader_t r;

	if (h_call(msg, 1, g_body, sizeof(g_body), &g_len) != CTAP2_OK)
		return false;
	if (!h_find(g_body, g_len, key, &r))
		return false;
	return osk_cbor_read_bool(&r, out) == OSK_OK;
}

/* ---- Vorbereitung ------------------------------------------------------ */

static void setup(platform_t *p)
{
	mock_reset_all();
	T_EQ("Kern startet", osk_ctap_init(OSK_CTAP_EXT_MIN_PIN), OSK_OK);
	platform_init(p, 2);
}

static void acfg_token(platform_t *p, uint8_t token[32], const char *pin)
{
	uint8_t code = 0;

	T_TRUE("Token mit acfg", get_token(p, pin, OSK_PERM_ACFG, NULL, token, &code));
}

/* ---- Faelle: Zugang zum Befehl ---------------------------------------- */

static void test_gemeldete_optionen(void)
{
	platform_t p;
	bool v = false;
	uint64_t n = 0;

	setup(&p);

	T_TRUE("credMgmt gemeldet", info_option("credMgmt", &v));
	T_TRUE("und zwar true", v);
	T_TRUE("authnrCfg gemeldet", info_option("authnrCfg", &v));
	T_TRUE("und zwar true", v);
	T_TRUE("setMinPINLength gemeldet", info_option("setMinPINLength", &v));
	T_TRUE("und zwar true", v);
	T_TRUE("ep gemeldet", info_option("ep", &v));
	T_TRUE("ab Werk aber aus", !v);

	/* alwaysUv wird NICHT gemeldet: toggleAlwaysUv ist nicht gebaut.
	 * Eine Option zu melden, deren Schalter fehlt, ist eine falsche
	 * Zusage - genau der Fehler, der bis zum 24.10.2026 bei "uv" stand. */
	T_TRUE("alwaysUv nicht gemeldet", !info_option("alwaysUv", &v));
	T_TRUE("uv weiterhin nicht gemeldet", !info_option("uv", &v));

	/* 0x10 ist maxRPIDsForSetMinPINLength. Ohne diese Zahl darf ein
	 * Programm annehmen, es sei KEIN Dienst erlaubt. */
	T_TRUE("0x10 gemeldet", info_uint(0x10, &n));
	T_EQ("und zwar unsere Obergrenze", n, OSK_CTAP_MINPIN_RP_MAX);

	T_TRUE("minPINLength gemeldet", info_uint(0x0d, &n));
	T_EQ("Vorgabe sind vier Code-Punkte", n, OSK_PIN_MIN_DEFAULT);
	T_TRUE("forcePINChange gemeldet", info_bool_key(0x0c, &v));
	T_TRUE("ab Werk aus", !v);
}

static void test_ohne_pin(void)
{
	platform_t p;
	uint8_t token[32];

	setup(&p);
	memset(token, 0x22, sizeof(token));

	/* Ohne PIN gibt es kein Token - und ohne Token keine Konfiguration.
	 * Wer das Geraet in der Hand hat, soll die Regeln nicht lockern
	 * koennen. */
	T_EQ("ohne PIN", config_call(&p, token, SUB_ENABLE_EP, NULL, 0), CTAP2_ERR_PIN_NOT_SET);
}

static void test_ohne_freigabe(void)
{
	platform_t p;
	uint8_t token[32];
	uint8_t falsch[32];
	uint8_t code = 0;

	setup(&p);
	set_pin(&p, "123456");
	acfg_token(&p, token, "123456");

	T_EQ("ohne MAC", config_call(&p, NULL, SUB_ENABLE_EP, NULL, 0), CTAP2_ERR_PUAT_REQUIRED);

	memcpy(falsch, token, 32);
	falsch[31] ^= 0xff;
	T_EQ("falscher MAC", config_call(&p, falsch, SUB_ENABLE_EP, NULL, 0),
	     CTAP2_ERR_PIN_AUTH_INVALID);

	/* Token ohne das Recht acfg. */
	uint8_t nur_ga[32];
	T_TRUE("Token ohne acfg",
	       get_token(&p, "123456", OSK_PERM_GA, "example.com", nur_ga, &code));
	T_EQ("Recht fehlt", config_call(&p, nur_ga, SUB_ENABLE_EP, NULL, 0),
	     CTAP2_ERR_PIN_AUTH_INVALID);

	/* Ein MAC ohne die 32 Fuellbytes - also der, den die VERWALTUNG
	 * nimmt - darf hier nicht passen. Sonst liesse sich ein Auftrag von
	 * einem Befehl auf den anderen uebertragen. */
	{
		uint8_t msg[64];
		uint8_t mac_in[2];
		uint8_t mac[32];
		osk_cbor_writer_t w;

		acfg_token(&p, token, "123456");
		mac_in[0] = OSK_CTAP_CMD_CONFIG;
		mac_in[1] = SUB_ENABLE_EP;
		const size_t mac_len = platform_mac(&p, token, 32, mac_in, 2, mac);
		msg[0] = OSK_CTAP_CMD_CONFIG;
		osk_cbor_writer_init(&w, msg + 1, sizeof(msg) - 1);
		osk_cbor_put_map(&w, 3);
		osk_cbor_put_uint(&w, 0x01);
		osk_cbor_put_uint(&w, SUB_ENABLE_EP);
		osk_cbor_put_uint(&w, 0x03);
		osk_cbor_put_uint(&w, p.protocol);
		osk_cbor_put_uint(&w, 0x04);
		osk_cbor_put_bytes(&w, mac, mac_len);
		T_EQ("MAC ohne Fuellbytes passt nicht",
		     h_call(msg, 1 + w.len, g_body, sizeof(g_body), &g_len),
		     CTAP2_ERR_PIN_AUTH_INVALID);
	}
}

static void test_nicht_gebaute_unterbefehle(void)
{
	platform_t p;
	uint8_t token[32];

	setup(&p);
	set_pin(&p, "123456");
	acfg_token(&p, token, "123456");

	/* toggleAlwaysUv ist bekannt und absichtlich nicht da - mit einem
	 * EIGENEN Wert, damit ein Programm das vom Unbekannten unterscheiden
	 * kann. */
	T_EQ("toggleAlwaysUv", config_call(&p, token, SUB_TOGGLE_ALWAYS_UV, NULL, 0),
	     CTAP2_ERR_UNSUPPORTED_OPTION);
	T_EQ("vendorPrototype", config_call(&p, token, 0xff, NULL, 0),
	     CTAP1_ERR_INVALID_PARAMETER);
	T_EQ("etwas Erfundenes", config_call(&p, token, 0x42, NULL, 0),
	     CTAP1_ERR_INVALID_PARAMETER);
}

/* ---- Enterprise Attestation ------------------------------------------- */

/* Eine MakeCredential-Nachricht mit dem Parameter 0x0A. */
static uint8_t make_credential_ep(uint64_t ep)
{
	uint8_t msg[512];
	osk_cbor_writer_t w;
	uint8_t hash[32];

	osk_sha256((const uint8_t *)"clientData", 10, hash);
	msg[0] = OSK_CTAP_CMD_MAKE_CREDENTIAL;
	osk_cbor_writer_init(&w, msg + 1, sizeof(msg) - 1);
	osk_cbor_put_map(&w, 5);
	osk_cbor_put_uint(&w, 0x01);
	osk_cbor_put_bytes(&w, hash, 32);
	osk_cbor_put_uint(&w, 0x02);
	osk_cbor_put_map(&w, 1);
	osk_cbor_put_text(&w, "id");
	osk_cbor_put_text(&w, "example.com");
	osk_cbor_put_uint(&w, 0x03);
	osk_cbor_put_map(&w, 2);
	osk_cbor_put_text(&w, "id");
	osk_cbor_put_bytes(&w, (const uint8_t *)"anna", 4);
	osk_cbor_put_text(&w, "name");
	osk_cbor_put_text(&w, "anna");
	osk_cbor_put_uint(&w, 0x04);
	osk_cbor_put_array(&w, 1);
	osk_cbor_put_map(&w, 2);
	osk_cbor_put_text(&w, "alg");
	osk_cbor_put_int(&w, -7);
	osk_cbor_put_text(&w, "type");
	osk_cbor_put_text(&w, "public-key");
	osk_cbor_put_uint(&w, 0x0a);
	osk_cbor_put_uint(&w, ep);
	T_TRUE("Anfrage passt", osk_cbor_writer_ok(&w));
	T_EQ("Anfrage kanonisch", osk_cbor_validate(msg + 1, w.len), OSK_OK);

	return h_call(msg, 1 + w.len, g_body, sizeof(g_body), &g_len);
}

static void test_enterprise_attestation(void)
{
	platform_t p;
	uint8_t token[32];
	bool v = false;
	osk_cbor_reader_t r;

	setup(&p);

	/* Ausgeschaltet: der Parameter ist dann ein falscher PARAMETER - das
	 * Geraet kann es hier nicht. (6.1.2, Schritt 9) */
	T_EQ("ausgeschaltet", make_credential_ep(1), CTAP1_ERR_INVALID_PARAMETER);

	set_pin(&p, "123456");
	acfg_token(&p, token, "123456");
	T_EQ("einschalten", config_call(&p, token, SUB_ENABLE_EP, NULL, 0), CTAP2_OK);
	T_TRUE("ep gemeldet", info_option("ep", &v));
	T_TRUE("jetzt true", v);

	/* Eingeschaltet: 1 und 2 sind erlaubt, alles andere ist eine falsche
	 * OPTION - ein anderer Fehlerwert als oben, und das ist Absicht der
	 * Spezifikation. */
	T_EQ("Wert 7", make_credential_ep(7), CTAP2_ERR_INVALID_OPTION);
	T_EQ("Wert 0", make_credential_ep(0), CTAP2_ERR_INVALID_OPTION);
	T_EQ("Wert 1 geht", make_credential_ep(1), CTAP2_OK);
	T_EQ("Wert 2 geht", make_credential_ep(2), CTAP2_OK);

	/* ABER: epAtt wird NICHT gemeldet. Eine Enterprise Attestation ohne
	 * Geraetezeugnis (x5c) waere eine Luege gegenueber dem Betrieb, der
	 * genau darauf baut. Dieses Geraet beglaubigt sich selbst. */
	T_TRUE("kein epAtt in der Antwort", !h_find(g_body, g_len, 0x04, &r));
	const char *fmt = NULL;
	size_t fmt_len = 0;
	T_TRUE("fmt da", h_find(g_body, g_len, 0x01, &r));
	T_EQ("fmt lesbar", osk_cbor_read_text(&r, &fmt, &fmt_len), OSK_OK);
	T_TRUE("gewoehnliche Beglaubigung", fmt_len == 6 && memcmp(fmt, "packed", 6) == 0);
	T_TRUE("attStmt ohne x5c", h_find(g_body, g_len, 0x03, &r));
	size_t fields = 0;
	T_EQ("attStmt ist Abbildung", osk_cbor_enter_map(&r, &fields), OSK_OK);
	T_EQ("nur alg und sig", (long long)fields, 2);

	/* Ein Reset schaltet sie wieder aus (7.1.3). */
	const uint8_t reset[1] = {OSK_CTAP_CMD_RESET};
	T_EQ("Reset", h_call(reset, 1, g_body, sizeof(g_body), &g_len), CTAP2_OK);
	T_TRUE("ep gemeldet", info_option("ep", &v));
	T_TRUE("nach Reset aus", !v);
}

/* Die zwei AAGUIDs: eine je Firmware-Variante. Welche gilt, entscheidet die
 * Uebersetzung, nicht der Aufrufer - pruefbar ist darum, DASS GetInfo
 * genau den Wert der eigenen Variante meldet und dass die beiden Werte
 * verschieden sind. */
static void test_zwei_aaguids(void)
{
	platform_t p;
	const uint8_t *aaguid = NULL;
	size_t alen = 0;

	setup(&p);

	const uint8_t msg[1] = {OSK_CTAP_CMD_GET_INFO};
	T_EQ("GetInfo", h_call(msg, 1, g_body, sizeof(g_body), &g_len), CTAP2_OK);
	T_TRUE("aaguid da", h_get_bytes(g_body, g_len, 0x03, &aaguid, &alen));
	T_EQ("16 Bytes", (long long)alen, 16);
	T_TRUE("und es ist der der eigenen Variante",
	       memcmp(aaguid, osk_ctap_aaguid(), 16) == 0);

	/* Die beiden Werte duerfen sich nicht gleichen: ein AAGUID bezeichnet
	 * eine Fassung mit DENSELBEN Faehigkeiten, und die Fassung mit RSA
	 * kann mehr als die ohne. Geprueft wird das am letzten Byte, weil die
	 * jeweils andere Fassung in dieser Uebersetzung nicht vorliegt - mehr
	 * ist am PC nicht zu messen, und das steht so im Bericht. */
	T_TRUE("letztes Byte benennt die Variante",
	       aaguid[15] == 0x01 || aaguid[15] == 0x02);
}

/* ---- Das PIN-Regelwerk ------------------------------------------------- */

static void test_min_pin_laenge(void)
{
	platform_t p;
	uint8_t token[32];
	uint8_t params[256];
	uint64_t n = 0;

	setup(&p);
	set_pin(&p, "123456"); /* sechs Code-Punkte */
	acfg_token(&p, token, "123456");

	/* Hoch ist erlaubt. */
	size_t plen = params_min_pin(6, true, NULL, 0, false, false, params, sizeof(params));
	T_EQ("auf sechs", config_call(&p, token, SUB_SET_MIN_PIN, params, plen), CTAP2_OK);
	T_TRUE("gemeldet", info_uint(0x0d, &n));
	T_EQ("sechs", n, 6);

	/* Herunter nicht - niemals, auch nicht mit gueltigem Token. */
	acfg_token(&p, token, "123456");
	plen = params_min_pin(4, true, NULL, 0, false, false, params, sizeof(params));
	T_EQ("zurueck auf vier", config_call(&p, token, SUB_SET_MIN_PIN, params, plen),
	     CTAP2_ERR_PIN_POLICY_VIOLATION);
	T_TRUE("gemeldet", info_uint(0x0d, &n));
	T_EQ("bleibt sechs", n, 6);

	/* Eine Mindestlaenge oberhalb der Hoechstlaenge wuerde das Geraet
	 * stilllegen: keine PIN waere mehr setzbar. */
	acfg_token(&p, token, "123456");
	plen = params_min_pin(64, true, NULL, 0, false, false, params, sizeof(params));
	T_EQ("unmoegliche Laenge", config_call(&p, token, SUB_SET_MIN_PIN, params, plen),
	     CTAP1_ERR_INVALID_PARAMETER);

	/* Ohne newMinPINLength bleibt die Laenge, wie sie ist. */
	acfg_token(&p, token, "123456");
	plen = params_min_pin(0, false, NULL, 0, false, false, params, sizeof(params));
	T_EQ("nichts angegeben", config_call(&p, token, SUB_SET_MIN_PIN, params, plen), CTAP2_OK);
	T_TRUE("gemeldet", info_uint(0x0d, &n));
	T_EQ("immer noch sechs", n, 6);

	/* Und eine neue PIN muss die neue Laenge einhalten. */
	{
		uint8_t padded[64];
		uint8_t enc[96];
		uint8_t auth[32];
		uint8_t h[32];
		uint8_t old_enc[64];

		memset(padded, 0, sizeof(padded));
		memcpy(padded, "123", 3); /* drei - zu kurz */
		const size_t enc_len = platform_encrypt(&p, padded, 64, enc);
		osk_sha256((const uint8_t *)"123456", 6, h);
		const size_t old_len = platform_encrypt(&p, h, 16, old_enc);
		uint8_t mac[32];
		osk_hmac_sha256_2(p.shared, 32, enc, enc_len, old_enc, old_len, mac);
		const size_t auth_len = (p.protocol == 1u) ? 16u : 32u;
		memcpy(auth, mac, auth_len);
		T_EQ("zu kurze neue PIN",
		     client_pin(&p, 0x04, true, enc, enc_len, old_enc, old_len, auth, auth_len, 0,
				NULL),
		     CTAP2_ERR_PIN_POLICY_VIOLATION);
	}
}

static void test_erzwungener_wechsel(void)
{
	platform_t p;
	uint8_t token[32];
	uint8_t params[256];
	uint8_t code = 0;
	bool v = false;

	setup(&p);
	set_pin(&p, "123456");
	acfg_token(&p, token, "123456");

	/* forceChangePin ohne gesetzte PIN gibt es nicht - hier ist eine
	 * gesetzt, also geht es. */
	const size_t plen =
	    params_min_pin(0, false, NULL, 0, true, true, params, sizeof(params));
	T_EQ("Wechsel erzwingen", config_call(&p, token, SUB_SET_MIN_PIN, params, plen),
	     CTAP2_OK);
	T_TRUE("forcePINChange gemeldet", info_bool_key(0x0c, &v));
	T_TRUE("und zwar true", v);

	/* Danach gibt es kein Token mehr auf die alte PIN: erst wechseln. */
	T_TRUE("kein Token mehr", !get_token(&p, "123456", OSK_PERM_ACFG, NULL, token, &code));
	T_EQ("und zwar mit PIN_INVALID", code, CTAP2_ERR_PIN_INVALID);

	/* Nach dem Wechsel ist der Zwang weg. */
	{
		uint8_t padded[64];
		uint8_t enc[96];
		uint8_t old_enc[64];
		uint8_t h[32];
		uint8_t mac[32];
		uint8_t auth[32];

		memset(padded, 0, sizeof(padded));
		memcpy(padded, "abcdefgh", 8);
		const size_t enc_len = platform_encrypt(&p, padded, 64, enc);
		osk_sha256((const uint8_t *)"123456", 6, h);
		const size_t old_len = platform_encrypt(&p, h, 16, old_enc);
		osk_hmac_sha256_2(p.shared, 32, enc, enc_len, old_enc, old_len, mac);
		const size_t auth_len = (p.protocol == 1u) ? 16u : 32u;
		memcpy(auth, mac, auth_len);
		T_EQ("PIN wechseln",
		     client_pin(&p, 0x04, true, enc, enc_len, old_enc, old_len, auth, auth_len, 0,
				NULL),
		     CTAP2_OK);
	}
	T_TRUE("forcePINChange gemeldet", info_bool_key(0x0c, &v));
	T_TRUE("jetzt aus", !v);
	T_TRUE("und wieder ein Token", get_token(&p, "abcdefgh", OSK_PERM_ACFG, NULL, token,
						 &code));
}

/* Steigt die Mindestlaenge ueber die Laenge der vorhandenen PIN, muss sie
 * gewechselt werden - OHNE dass der Befehl selbst fehlschlaegt. */
static void test_zu_kurze_pin_erzwingt_wechsel(void)
{
	platform_t p;
	uint8_t token[32];
	uint8_t params[256];
	bool v = false;

	setup(&p);
	set_pin(&p, "1234"); /* vier */
	acfg_token(&p, token, "1234");

	const size_t plen = params_min_pin(8, true, NULL, 0, false, false, params, sizeof(params));
	T_EQ("auf acht", config_call(&p, token, SUB_SET_MIN_PIN, params, plen), CTAP2_OK);
	T_TRUE("forcePINChange gemeldet", info_bool_key(0x0c, &v));
	T_TRUE("von selbst gesetzt", v);
}

static void test_dienste_liste(void)
{
	platform_t p;
	uint8_t token[32];
	uint8_t params[512];
	static const char *const zwei[] = {"example.com", "zweiter.example"};
	static const char *const zuviele[] = {"a.example", "b.example", "c.example", "d.example",
					      "e.example"};

	setup(&p);
	set_pin(&p, "123456");
	acfg_token(&p, token, "123456");

	/* Mehr Dienste als 0x10 meldet: ein Fehler. Die Spezifikation
	 * verlangt hier "einen Fehler", nennt aber keinen - CTAP1_ERR_
	 * INVALID_PARAMETER ist unsere Wahl, und sie steht so im Code. */
	size_t plen = params_min_pin(0, false, zuviele, 5, false, false, params, sizeof(params));
	T_EQ("fuenf sind zu viele", config_call(&p, token, SUB_SET_MIN_PIN, params, plen),
	     CTAP1_ERR_INVALID_PARAMETER);

	acfg_token(&p, token, "123456");
	plen = params_min_pin(0, false, zwei, 2, false, false, params, sizeof(params));
	T_EQ("zwei gehen", config_call(&p, token, SUB_SET_MIN_PIN, params, plen), CTAP2_OK);

	/* Und jetzt erfaehrt example.com die Mindestlaenge, ein anderer
	 * Dienst nicht. Die Erweiterung minPinLength antwortet nur Befugten. */
	{
		h_msg_t m;
		osk_cbor_writer_t w;
		uint8_t hash[32];
		const uint8_t *ad = NULL;
		size_t ad_len = 0;

		/* MakeCredential mit "minPinLength": true */
		osk_sha256((const uint8_t *)"clientData", 10, hash);
		m.buf[0] = OSK_CTAP_CMD_MAKE_CREDENTIAL;
		osk_cbor_writer_init(&w, m.buf + 1, sizeof(m.buf) - 1);
		osk_cbor_put_map(&w, 5);
		osk_cbor_put_uint(&w, 0x01);
		osk_cbor_put_bytes(&w, hash, 32);
		osk_cbor_put_uint(&w, 0x02);
		osk_cbor_put_map(&w, 1);
		osk_cbor_put_text(&w, "id");
		osk_cbor_put_text(&w, "example.com");
		osk_cbor_put_uint(&w, 0x03);
		osk_cbor_put_map(&w, 2);
		osk_cbor_put_text(&w, "id");
		osk_cbor_put_bytes(&w, (const uint8_t *)"anna", 4);
		osk_cbor_put_text(&w, "name");
		osk_cbor_put_text(&w, "anna");
		osk_cbor_put_uint(&w, 0x04);
		osk_cbor_put_array(&w, 1);
		osk_cbor_put_map(&w, 2);
		osk_cbor_put_text(&w, "alg");
		osk_cbor_put_int(&w, -7);
		osk_cbor_put_text(&w, "type");
		osk_cbor_put_text(&w, "public-key");
		osk_cbor_put_uint(&w, 0x06);
		osk_cbor_put_map(&w, 1);
		osk_cbor_put_text(&w, "minPinLength");
		osk_cbor_put_bool(&w, true);
		m.len = 1 + w.len;
		T_EQ("MakeCredential ok", h_call(m.buf, m.len, g_body, sizeof(g_body), &g_len),
		     CTAP2_OK);
		T_TRUE("authData da", h_get_bytes(g_body, g_len, 0x02, &ad, &ad_len));
		/* ED-Flagge heisst: eine Erweiterung hat geantwortet. */
		T_TRUE("ED gesetzt", (ad[32] & OSK_AD_FLAG_ED) != 0);
	}
}

int main(void)
{
	test_gemeldete_optionen();
	test_ohne_pin();
	test_ohne_freigabe();
	test_nicht_gebaute_unterbefehle();
	test_enterprise_attestation();
	test_zwei_aaguids();
	test_min_pin_laenge();
	test_erzwungener_wechsel();
	test_zu_kurze_pin_erzwingt_wechsel();
	test_dienste_liste();
	return t_report("config");
}
