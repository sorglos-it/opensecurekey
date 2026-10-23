/* PIN-Protokolle 1 und 2 von der Seite des Rechners aus gespielt:
 * Schluesseltausch, PIN setzen, Token holen, Token benutzen - und alle
 * Wege, auf denen es scheitern muss.
 *
 * Der Fehlversuchszaehler liegt in der SE050-Attrappe, so wie er spaeter
 * im Chip liegt. */
#include "ctap_helper.h"
#include "osk_pin.h"

static uint8_t g_body[2048];
static size_t g_len;

static void start(void)
{
	mock_reset_all();
	T_EQ("Kern startet", osk_ctap_init(OSK_CTAP_EXT_HMAC_SECRET), OSK_OK);
}

/* ---- Die Seite des Rechners -------------------------------------------- */

typedef struct {
	uint64_t protocol;
	uint8_t pub[64];    /* unser angeblicher Punkt */
	uint8_t shared[64]; /* HMAC-Teil || AES-Teil */
} platform_t;

static void platform_init(platform_t *p, uint64_t protocol)
{
	uint8_t z[32];

	p->protocol = protocol;
	for (unsigned i = 0; i < 64; i++)
		p->pub[i] = (uint8_t)(0x40 + i);

	mock_se050_ecdh_expect(p->pub, z);
	if (protocol == 1u) {
		uint8_t h[32];
		osk_sha256(z, 32, h);
		memcpy(p->shared, h, 32);
		memcpy(p->shared + 32, h, 32);
	} else {
		static const uint8_t zero[32] = {0};
		osk_hkdf_sha256(zero, 32, z, 32, (const uint8_t *)"CTAP2 HMAC key", 14, p->shared,
				32);
		osk_hkdf_sha256(zero, 32, z, 32, (const uint8_t *)"CTAP2 AES key", 13,
				p->shared + 32, 32);
	}
}

static size_t platform_encrypt(const platform_t *p, const uint8_t *in, size_t in_len, uint8_t *out)
{
	if (p->protocol == 1u) {
		static const uint8_t zero_iv[16] = {0};
		osk_aes256_cbc_encrypt(p->shared + 32, zero_iv, in, out, in_len);
		return in_len;
	}
	uint8_t iv[16];
	memset(iv, 0x33, sizeof(iv));
	memcpy(out, iv, 16);
	osk_aes256_cbc_encrypt(p->shared + 32, iv, in, out + 16, in_len);
	return in_len + 16;
}

static size_t platform_decrypt(const platform_t *p, const uint8_t *in, size_t in_len, uint8_t *out)
{
	if (p->protocol == 1u) {
		static const uint8_t zero_iv[16] = {0};
		osk_aes256_cbc_decrypt(p->shared + 32, zero_iv, in, out, in_len);
		return in_len;
	}
	osk_aes256_cbc_decrypt(p->shared + 32, in, in + 16, out, in_len - 16);
	return in_len - 16;
}

static size_t platform_mac(const platform_t *p, const uint8_t *key, size_t key_len,
			   const uint8_t *msg, size_t msg_len, uint8_t *out)
{
	uint8_t mac[32];
	const size_t use = (key_len > 32) ? 32 : key_len;

	osk_hmac_sha256(key, use, msg, msg_len, mac);
	const size_t n = (p->protocol == 1u) ? 16u : 32u;
	memcpy(out, mac, n);
	return n;
}

/* Schickt einen ClientPIN-Unterbefehl. */
static uint8_t client_pin(const platform_t *p, uint64_t sub, bool with_key,
			  const uint8_t *new_pin_enc, size_t new_pin_enc_len,
			  const uint8_t *pin_hash_enc, size_t pin_hash_enc_len, const uint8_t *auth,
			  size_t auth_len, uint64_t permissions, const char *rp_id)
{
	uint8_t msg[512];
	osk_cbor_writer_t w;
	size_t entries = 2; /* Protokoll + Unterbefehl */

	if (with_key)
		entries++;
	if (auth)
		entries++;
	if (new_pin_enc)
		entries++;
	if (pin_hash_enc)
		entries++;
	if (permissions)
		entries++;
	if (rp_id)
		entries++;

	msg[0] = OSK_CTAP_CMD_CLIENT_PIN;
	osk_cbor_writer_init(&w, msg + 1, sizeof(msg) - 1);
	osk_cbor_put_map(&w, entries);
	osk_cbor_put_uint(&w, 0x01);
	osk_cbor_put_uint(&w, p->protocol);
	osk_cbor_put_uint(&w, 0x02);
	osk_cbor_put_uint(&w, sub);
	if (with_key) {
		osk_cbor_put_uint(&w, 0x03);
		osk_cbor_put_map(&w, 5);
		osk_cbor_put_uint(&w, 1);
		osk_cbor_put_uint(&w, 2);
		osk_cbor_put_uint(&w, 3);
		osk_cbor_put_int(&w, -25);
		osk_cbor_put_int(&w, -1);
		osk_cbor_put_uint(&w, 1);
		osk_cbor_put_int(&w, -2);
		osk_cbor_put_bytes(&w, p->pub, 32);
		osk_cbor_put_int(&w, -3);
		osk_cbor_put_bytes(&w, p->pub + 32, 32);
	}
	if (auth) {
		osk_cbor_put_uint(&w, 0x04);
		osk_cbor_put_bytes(&w, auth, auth_len);
	}
	if (new_pin_enc) {
		osk_cbor_put_uint(&w, 0x05);
		osk_cbor_put_bytes(&w, new_pin_enc, new_pin_enc_len);
	}
	if (pin_hash_enc) {
		osk_cbor_put_uint(&w, 0x06);
		osk_cbor_put_bytes(&w, pin_hash_enc, pin_hash_enc_len);
	}
	if (permissions) {
		osk_cbor_put_uint(&w, 0x09);
		osk_cbor_put_uint(&w, permissions);
	}
	if (rp_id) {
		osk_cbor_put_uint(&w, 0x0a);
		osk_cbor_put_text(&w, rp_id);
	}
	T_TRUE("Anfrage passt", osk_cbor_writer_ok(&w));
	T_EQ("Anfrage kanonisch", osk_cbor_validate(msg + 1, w.len), OSK_OK);

	return h_call(msg, 1 + w.len, g_body, sizeof(g_body), &g_len);
}

/* Setzt eine PIN auf dem ueblichen Weg. */
static void set_pin(const platform_t *p, const char *pin)
{
	uint8_t padded[64];
	uint8_t enc[96];
	uint8_t auth[32];

	memset(padded, 0, sizeof(padded));
	memcpy(padded, pin, strlen(pin));
	const size_t enc_len = platform_encrypt(p, padded, 64, enc);
	const size_t auth_len = platform_mac(p, p->shared, 64, enc, enc_len, auth);

	T_EQ("setPIN ok", client_pin(p, 0x03, true, enc, enc_len, NULL, 0, auth, auth_len, 0, NULL),
	     CTAP2_OK);
}

/* Holt ein pinUvAuthToken mit Rechten. */
static bool get_token(const platform_t *p, const char *pin, uint64_t permissions, const char *rp_id,
		      uint8_t token_out[32], uint8_t *code_out)
{
	uint8_t h[32];
	uint8_t enc[64];

	osk_sha256((const uint8_t *)pin, strlen(pin), h);
	const size_t enc_len = platform_encrypt(p, h, 16, enc);

	const uint8_t code =
	    client_pin(p, 0x09, true, NULL, 0, enc, enc_len, NULL, 0, permissions, rp_id);
	if (code_out)
		*code_out = code;
	if (code != CTAP2_OK)
		return false;

	osk_cbor_reader_t r;
	if (!h_find(g_body, g_len, 0x02, &r))
		return false;
	const uint8_t *ct = NULL;
	size_t ct_len = 0;
	if (osk_cbor_read_bytes(&r, &ct, &ct_len) != OSK_OK)
		return false;
	uint8_t plain[64];
	const size_t plain_len = platform_decrypt(p, ct, ct_len, plain);
	if (plain_len != 32)
		return false;
	memcpy(token_out, plain, 32);
	return true;
}

/* ---- Faelle ------------------------------------------------------------ */

static void test_key_agreement(void)
{
	platform_t p;

	start();
	platform_init(&p, 2);

	T_EQ("getKeyAgreement ok", client_pin(&p, 0x02, false, NULL, 0, NULL, 0, NULL, 0, 0, NULL),
	     CTAP2_OK);
	T_EQ("Antwort kanonisch", osk_cbor_validate(g_body, g_len), OSK_OK);

	/* Die Antwort muss einen COSE_Key mit fuenf Eintraegen tragen. */
	osk_cbor_reader_t r;
	T_TRUE("keyAgreement vorhanden", h_find(g_body, g_len, 0x01, &r));
	size_t count = 0;
	T_EQ("Abbildung", osk_cbor_enter_map(&r, &count), OSK_OK);
	T_EQ("fuenf Eintraege", (long long)count, 5);

	/* Unbekanntes Protokoll wird abgewiesen. */
	platform_t bad = p;
	bad.protocol = 3;
	T_EQ("Protokoll 3 gibt es nicht",
	     client_pin(&bad, 0x02, false, NULL, 0, NULL, 0, NULL, 0, 0, NULL),
	     CTAP1_ERR_INVALID_PARAMETER);

	/* Unbekannter Unterbefehl */
	T_EQ("Unterbefehl 0x42", client_pin(&p, 0x42, false, NULL, 0, NULL, 0, NULL, 0, 0, NULL),
	     CTAP2_ERR_INVALID_SUBCOMMAND);
}

static void test_set_und_token(uint64_t protocol)
{
	platform_t p;
	uint8_t token[32];

	start();
	platform_init(&p, protocol);

	/* Ohne PIN gibt es kein Token. */
	uint8_t code = 0;
	T_TRUE("ohne PIN kein Token",
	       !get_token(&p, "123456", OSK_PERM_MC, "example.com", token, &code));
	T_EQ("Grund ist: keine PIN gesetzt", code, CTAP2_ERR_PIN_NOT_SET);

	set_pin(&p, "123456");

	/* Zweimal setzen geht nicht. */
	{
		uint8_t padded[64], enc[96], auth[32];
		memset(padded, 0, sizeof(padded));
		memcpy(padded, "654321", 6);
		const size_t enc_len = platform_encrypt(&p, padded, 64, enc);
		const size_t auth_len = platform_mac(&p, p.shared, 64, enc, enc_len, auth);
		T_EQ("setPIN zum zweiten Mal",
		     client_pin(&p, 0x03, true, enc, enc_len, NULL, 0, auth, auth_len, 0, NULL),
		     CTAP2_ERR_PIN_AUTH_INVALID);
	}

	/* clientPin steht jetzt in GetInfo auf true. */
	{
		const uint8_t msg[1] = {OSK_CTAP_CMD_GET_INFO};
		T_EQ("GetInfo", h_call(msg, 1, g_body, sizeof(g_body), &g_len), CTAP2_OK);
	}

	/* Token mit mc und ga holen. */
	T_TRUE("Token geholt",
	       get_token(&p, "123456", OSK_PERM_MC | OSK_PERM_GA, "example.com", token, &code));

	/* Damit MakeCredential: pinUvAuthParam = authenticate(token, clientDataHash) */
	uint8_t hash[32], auth[32];
	osk_sha256((const uint8_t *)"clientData", 10, hash);
	const size_t auth_len = platform_mac(&p, token, 32, hash, 32, auth);

	h_msg_t m;
	h_make_credential(&m, "example.com", "anna", true, false, 0, auth, auth_len, protocol);
	T_EQ("MakeCredential mit Token", h_call(m.buf, m.len, g_body, sizeof(g_body), &g_len),
	     CTAP2_OK);

	/* Jetzt muss die UV-Flagge gesetzt sein. */
	const uint8_t *ad = NULL;
	size_t ad_len = 0;
	T_TRUE("authData", h_get_bytes(g_body, g_len, 0x02, &ad, &ad_len));
	T_TRUE("UV gesetzt", (ad[32] & OSK_AD_FLAG_UV) != 0);
	T_TRUE("UP gesetzt", (ad[32] & OSK_AD_FLAG_UP) != 0);

	/* Nach dem Tastendruck sind die Rechte verbraucht (6.5.5.7). */
	T_EQ("dasselbe Token noch einmal", h_call(m.buf, m.len, g_body, sizeof(g_body), &g_len),
	     CTAP2_ERR_PIN_AUTH_INVALID);

	/* Neues Token, falscher Dienst. */
	T_TRUE("Token fuer anderen Dienst",
	       get_token(&p, "123456", OSK_PERM_MC, "anderer.example", token, &code));
	const size_t auth_len2 = platform_mac(&p, token, 32, hash, 32, auth);
	h_make_credential(&m, "example.com", "anna", false, false, 0, auth, auth_len2, protocol);
	T_EQ("Token gilt nur fuer seinen Dienst",
	     h_call(m.buf, m.len, g_body, sizeof(g_body), &g_len), CTAP2_ERR_PIN_AUTH_INVALID);

	/* Falscher MAC */
	T_TRUE("neues Token", get_token(&p, "123456", OSK_PERM_MC, "example.com", token, &code));
	token[0] ^= 0xff;
	const size_t auth_len3 = platform_mac(&p, token, 32, hash, 32, auth);
	h_make_credential(&m, "example.com", "anna", false, false, 0, auth, auth_len3, protocol);
	T_EQ("falsches Token", h_call(m.buf, m.len, g_body, sizeof(g_body), &g_len),
	     CTAP2_ERR_PIN_AUTH_INVALID);
}

static void test_falsche_pin(void)
{
	platform_t p;
	uint8_t token[32];
	uint8_t code = 0;

	start();
	platform_init(&p, 2);
	set_pin(&p, "123456");

	const uint8_t before = mock_se050_tries();
	T_TRUE("Zaehler vorhanden", before > 0);

	T_TRUE("falsche PIN", !get_token(&p, "999999", OSK_PERM_MC, "example.com", token, &code));
	T_EQ("Antwort PIN_INVALID", code, CTAP2_ERR_PIN_INVALID);
	T_TRUE("Zaehler im Chip ist gesunken", mock_se050_tries() < before);

	T_TRUE("zweiter Fehlversuch",
	       !get_token(&p, "999999", OSK_PERM_MC, "example.com", token, &code));
	T_EQ("wieder PIN_INVALID", code, CTAP2_ERR_PIN_INVALID);

	/* Nach dem dritten Mal hintereinander: Strom aus (6.5.5.7.2). */
	T_TRUE("dritter Fehlversuch",
	       !get_token(&p, "999999", OSK_PERM_MC, "example.com", token, &code));
	T_EQ("jetzt PIN_AUTH_BLOCKED", code, CTAP2_ERR_PIN_AUTH_BLOCKED);
	T_TRUE("auch mit der richtigen PIN gesperrt",
	       !get_token(&p, "123456", OSK_PERM_MC, "example.com", token, &code));
	T_EQ("bleibt gesperrt", code, CTAP2_ERR_PIN_AUTH_BLOCKED);

	/* getPINRetries sagt, wie viele Versuche noch offen sind. */
	T_EQ("getPINRetries", client_pin(&p, 0x01, false, NULL, 0, NULL, 0, NULL, 0, 0, NULL),
	     CTAP2_OK);
	osk_cbor_reader_t r;
	uint64_t v = 99;
	T_TRUE("pinRetries vorhanden", h_find(g_body, g_len, 0x03, &r));
	T_EQ("lesbar", osk_cbor_read_uint(&r, &v), OSK_OK);
	T_EQ("passt zum Chip", (long long)v, (long long)mock_se050_tries());

	/* Zaehler ganz leer: PIN_BLOCKED. */
	start();
	platform_init(&p, 2);
	set_pin(&p, "123456");
	mock_se050_set_tries(0);
	T_TRUE("Zaehler leer", !get_token(&p, "123456", OSK_PERM_MC, "example.com", token, &code));
	T_EQ("PIN_BLOCKED", code, CTAP2_ERR_PIN_BLOCKED);
}

static void test_pin_zu_kurz(void)
{
	platform_t p;
	uint8_t padded[64], enc[96], auth[32];

	start();
	platform_init(&p, 2);

	/* Drei Zeichen - die Mindestlaenge ist vier. */
	memset(padded, 0, sizeof(padded));
	memcpy(padded, "123", 3);
	const size_t enc_len = platform_encrypt(&p, padded, 64, enc);
	const size_t auth_len = platform_mac(&p, p.shared, 64, enc, enc_len, auth);
	T_EQ("zu kurze PIN",
	     client_pin(&p, 0x03, true, enc, enc_len, NULL, 0, auth, auth_len, 0, NULL),
	     CTAP2_ERR_PIN_POLICY_VIOLATION);

	/* Leere PIN */
	memset(padded, 0, sizeof(padded));
	const size_t enc_len2 = platform_encrypt(&p, padded, 64, enc);
	const size_t auth_len2 = platform_mac(&p, p.shared, 64, enc, enc_len2, auth);
	T_EQ("leere PIN",
	     client_pin(&p, 0x03, true, enc, enc_len2, NULL, 0, auth, auth_len2, 0, NULL),
	     CTAP2_ERR_PIN_POLICY_VIOLATION);

	/* Vier Zeichen gehen. Umlaute zaehlen als ein Code-Punkt, auch wenn
	 * sie zwei Bytes brauchen. */
	start();
	platform_init(&p, 2);
	memset(padded, 0, sizeof(padded));
	memcpy(padded, "\xc3\xa4\xc3\xb6\xc3\xbc\xc3\x9f", 8); /* vier Umlaute */
	const size_t enc_len3 = platform_encrypt(&p, padded, 64, enc);
	const size_t auth_len3 = platform_mac(&p, p.shared, 64, enc, enc_len3, auth);
	T_EQ("vier Umlaute reichen",
	     client_pin(&p, 0x03, true, enc, enc_len3, NULL, 0, auth, auth_len3, 0, NULL),
	     CTAP2_OK);

	/* Falsche Laenge des verschluesselten Teils */
	start();
	platform_init(&p, 2);
	memset(padded, 0, sizeof(padded));
	memcpy(padded, "123456", 6);
	const size_t enc_len4 = platform_encrypt(&p, padded, 32, enc); /* nur 32 */
	const size_t auth_len4 = platform_mac(&p, p.shared, 64, enc, enc_len4, auth);
	T_EQ("newPinEnc falsch lang",
	     client_pin(&p, 0x03, true, enc, enc_len4, NULL, 0, auth, auth_len4, 0, NULL),
	     CTAP1_ERR_INVALID_PARAMETER);

	/* Falscher MAC auf newPinEnc */
	const size_t enc_len5 = platform_encrypt(&p, padded, 64, enc);
	size_t auth_len5 = platform_mac(&p, p.shared, 64, enc, enc_len5, auth);
	auth[0] ^= 0xff;
	T_EQ("falscher MAC",
	     client_pin(&p, 0x03, true, enc, enc_len5, NULL, 0, auth, auth_len5, 0, NULL),
	     CTAP2_ERR_PIN_AUTH_INVALID);
	(void)auth_len5;
}

static void test_change_pin(void)
{
	platform_t p;
	uint8_t token[32];
	uint8_t code = 0;

	start();
	platform_init(&p, 2);
	set_pin(&p, "123456");

	/* PIN wechseln: MAC geht ueber newPinEnc || pinHashEnc. */
	uint8_t padded[64], new_enc[96], old_hash[32], old_enc[64], auth[32];
	memset(padded, 0, sizeof(padded));
	memcpy(padded, "abcdef", 6);
	const size_t new_len = platform_encrypt(&p, padded, 64, new_enc);
	osk_sha256((const uint8_t *)"123456", 6, old_hash);
	const size_t old_len = platform_encrypt(&p, old_hash, 16, old_enc);

	{
		uint8_t both[160];
		memcpy(both, new_enc, new_len);
		memcpy(both + new_len, old_enc, old_len);
		const size_t auth_len =
		    platform_mac(&p, p.shared, 64, both, new_len + old_len, auth);
		T_EQ("changePIN ok",
		     client_pin(&p, 0x04, true, new_enc, new_len, old_enc, old_len, auth, auth_len,
				0, NULL),
		     CTAP2_OK);
	}

	/* Die alte PIN gilt nicht mehr, die neue schon. */
	T_TRUE("alte PIN ungueltig",
	       !get_token(&p, "123456", OSK_PERM_MC, "example.com", token, &code));
	T_EQ("PIN_INVALID", code, CTAP2_ERR_PIN_INVALID);
	T_TRUE("neue PIN gilt", get_token(&p, "abcdef", OSK_PERM_MC, "example.com", token, &code));

	/* changePIN mit falscher alter PIN. */
	osk_sha256((const uint8_t *)"falsch", 6, old_hash);
	const size_t old_len2 = platform_encrypt(&p, old_hash, 16, old_enc);
	memset(padded, 0, sizeof(padded));
	memcpy(padded, "neu1234", 7);
	const size_t new_len2 = platform_encrypt(&p, padded, 64, new_enc);
	{
		uint8_t both[160];
		memcpy(both, new_enc, new_len2);
		memcpy(both + new_len2, old_enc, old_len2);
		const size_t auth_len =
		    platform_mac(&p, p.shared, 64, both, new_len2 + old_len2, auth);
		T_EQ("falsche alte PIN",
		     client_pin(&p, 0x04, true, new_enc, new_len2, old_enc, old_len2, auth,
				auth_len, 0, NULL),
		     CTAP2_ERR_PIN_INVALID);
	}
}

static void test_rechte(void)
{
	platform_t p;
	uint8_t token[32];
	uint8_t code = 0;

	start();
	platform_init(&p, 2);
	set_pin(&p, "123456");

	/* Rechte, die wir nicht haben, werden abgewiesen. */
	T_TRUE("credMgmt gibt es nicht",
	       !get_token(&p, "123456", OSK_PERM_CM, "example.com", token, &code));
	T_EQ("UNAUTHORIZED_PERMISSION", code, CTAP2_ERR_UNAUTHORIZED_PERM);

	/* Rechte 0 ist verboten. */
	{
		uint8_t h[32], enc[64];
		osk_sha256((const uint8_t *)"123456", 6, h);
		const size_t enc_len = platform_encrypt(&p, h, 16, enc);
		/* permissions = 0 wird von client_pin als "nicht vorhanden"
		 * geschrieben; darum fehlt der Parameter und das ist auch
		 * ein Fehler. */
		T_EQ("ohne Rechte",
		     client_pin(&p, 0x09, true, NULL, 0, enc, enc_len, NULL, 0, 0, "example.com"),
		     CTAP1_ERR_INVALID_PARAMETER);
	}

	/* ga allein genuegt nicht fuer MakeCredential. */
	T_TRUE("nur ga", get_token(&p, "123456", OSK_PERM_GA, "example.com", token, &code));
	uint8_t hash[32], auth[32];
	osk_sha256((const uint8_t *)"clientData", 10, hash);
	const size_t auth_len = platform_mac(&p, token, 32, hash, 32, auth);
	h_msg_t m;
	h_make_credential(&m, "example.com", "anna", false, false, 0, auth, auth_len, 2);
	T_EQ("mc fehlt", h_call(m.buf, m.len, g_body, sizeof(g_body), &g_len),
	     CTAP2_ERR_PIN_AUTH_INVALID);
}

static void test_leeres_pin_auth(void)
{
	platform_t p;
	h_msg_t m;
	const uint8_t empty[1] = {0};

	/* Ohne PIN: erst fragen, dann PIN_NOT_SET. */
	start();
	platform_init(&p, 2);
	h_make_credential(&m, "example.com", "anna", false, false, 0, empty, 0, 2);
	T_EQ("leeres pinUvAuthParam ohne PIN", h_call(m.buf, m.len, g_body, sizeof(g_body), &g_len),
	     CTAP2_ERR_PIN_NOT_SET);
	T_EQ("es wurde gefragt", (long long)mock_ui_confirm_count(), 1);

	/* Mit PIN: PIN_INVALID. */
	set_pin(&p, "123456");
	T_EQ("leeres pinUvAuthParam mit PIN", h_call(m.buf, m.len, g_body, sizeof(g_body), &g_len),
	     CTAP2_ERR_PIN_INVALID);

	/* pinUvAuthParam ohne Protokollangabe. */
	uint8_t auth[32];
	memset(auth, 0x11, sizeof(auth));
	{
		osk_cbor_writer_t w;
		uint8_t hash[32];
		osk_sha256((const uint8_t *)"clientData", 10, hash);
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
		osk_cbor_put_uint(&w, 8);
		osk_cbor_put_bytes(&w, auth, sizeof(auth));
		m.len = 1 + w.len;
	}
	T_EQ("Protokoll fehlt", h_call(m.buf, m.len, g_body, sizeof(g_body), &g_len),
	     CTAP2_ERR_MISSING_PARAMETER);
}

static void test_reset_loescht_pin(void)
{
	platform_t p;
	uint8_t token[32];
	uint8_t code = 0;

	start();
	platform_init(&p, 2);
	set_pin(&p, "123456");
	T_TRUE("Token geht", get_token(&p, "123456", OSK_PERM_MC, "example.com", token, &code));

	const uint8_t msg[1] = {OSK_CTAP_CMD_RESET};
	T_EQ("Reset", h_call(msg, 1, g_body, sizeof(g_body), &g_len), CTAP2_OK);

	/* Nach dem Reset ist auch das gemeinsame Geheimnis hinfaellig, weil
	 * der Schluesseltauschschluessel neu gezogen wurde. Die Attrappe
	 * rechnet ihr ECDH aber nur aus dem Punkt des Rechners, darum pruefen
	 * wir hier nur, dass keine PIN mehr gesetzt ist. */
	T_TRUE("keine PIN mehr", !osk_pin_state()->pin_set);
}

int main(void)
{
	test_key_agreement();
	test_set_und_token(2);
	test_set_und_token(1);
	test_falsche_pin();
	test_pin_zu_kurz();
	test_change_pin();
	test_rechte();
	test_leeres_pin_auth();
	test_reset_loescht_pin();
	return t_report("pin");
}
