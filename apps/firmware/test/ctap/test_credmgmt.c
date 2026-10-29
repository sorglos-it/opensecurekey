/* authenticatorCredentialManagement (0x0A): zaehlen, auflisten, loeschen,
 * Nutzerangaben aendern - von der Seite des Rechners aus gespielt, mit
 * echten Nachrichten und echten MACs.
 *
 * Nichts davon lief am Geraet oder in einem Browser. Chip, Anzeige und
 * Tresor sind Attrappen (siehe mocks.h). */
#include "pin_helper.h"
#include "osk_credid.h"
#include "osk_ctap_cm.h"

/* Unterbefehle, wie in osk_ctap_cm.c */
#define SUB_METADATA 0x01
#define SUB_RPS_BEGIN 0x02
#define SUB_RPS_NEXT 0x03
#define SUB_CREDS_BEGIN 0x04
#define SUB_CREDS_NEXT 0x05
#define SUB_DELETE 0x06
#define SUB_UPDATE_USER 0x07

/* ---- Nachrichten bauen ------------------------------------------------- */

/* Schickt einen Verwaltungsbefehl. params/params_len duerfen leer sein.
 * token == NULL heisst: ohne pinUvAuthParam (fuer die GetNext-Befehle und
 * fuer die Probe, dass es ohne nicht geht). */
static uint8_t cm_call(const platform_t *p, const uint8_t *token, uint64_t sub,
		       const uint8_t *params, size_t params_len)
{
	uint8_t msg[768];
	uint8_t mac_in[1 + 512];
	uint8_t mac[32];
	osk_cbor_writer_t w;
	size_t entries = 1;
	size_t mac_len = 0;

	if (params_len)
		entries++;
	if (token)
		entries += 2;

	/* Die Nachricht fuer den MAC: subCommand || subCommandParams. Ohne
	 * Fuellbytes und ohne Befehlsbyte - anders als bei 0x0D. */
	mac_in[0] = (uint8_t)sub;
	if (params_len)
		memcpy(mac_in + 1, params, params_len);
	if (token)
		mac_len = platform_mac(p, token, 32, mac_in, 1 + params_len, mac);

	msg[0] = OSK_CTAP_CMD_CRED_MGMT;
	osk_cbor_writer_init(&w, msg + 1, sizeof(msg) - 1);
	osk_cbor_put_map(&w, entries);
	osk_cbor_put_uint(&w, 0x01);
	osk_cbor_put_uint(&w, sub);
	if (params_len) {
		osk_cbor_put_uint(&w, 0x02);
		/* Fertiges CBOR unveraendert anfuegen: nur so sind die Bytes
		 * im MAC und die Bytes in der Nachricht dieselben. */
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

	const uint8_t code = h_call(msg, 1 + w.len, g_body, sizeof(g_body), &g_len);
	if (code == CTAP2_OK && g_len > 0)
		T_EQ("Antwort kanonisch", osk_cbor_validate(g_body, g_len), OSK_OK);
	return code;
}

static size_t params_rp_hash(const uint8_t rp_hash[32], uint8_t *out, size_t cap)
{
	osk_cbor_writer_t w;

	osk_cbor_writer_init(&w, out, cap);
	osk_cbor_put_map(&w, 1);
	osk_cbor_put_uint(&w, 0x01);
	osk_cbor_put_bytes(&w, rp_hash, 32);
	T_TRUE("Parameter passen", osk_cbor_writer_ok(&w));
	return w.len;
}

static size_t params_cred(const uint8_t *cred_id, size_t cred_id_len, const uint8_t *user_id,
			  size_t user_id_len, const char *user_name, uint8_t *out, size_t cap)
{
	osk_cbor_writer_t w;

	osk_cbor_writer_init(&w, out, cap);
	osk_cbor_put_map(&w, user_id ? 2u : 1u);
	osk_cbor_put_uint(&w, 0x02);
	osk_cbor_put_map(&w, 2);
	osk_cbor_put_text(&w, "id");
	osk_cbor_put_bytes(&w, cred_id, cred_id_len);
	osk_cbor_put_text(&w, "type");
	osk_cbor_put_text(&w, "public-key");
	if (user_id) {
		osk_cbor_put_uint(&w, 0x03);
		osk_cbor_put_map(&w, user_name ? 2u : 1u);
		osk_cbor_put_text(&w, "id");
		osk_cbor_put_bytes(&w, user_id, user_id_len);
		if (user_name) {
			osk_cbor_put_text(&w, "name");
			osk_cbor_put_text(&w, user_name);
		}
	}
	T_TRUE("Parameter passen", osk_cbor_writer_ok(&w));
	return w.len;
}

/* ---- Vorbereitung ------------------------------------------------------ */

/* Legt einen auffindbaren Zugang an und liefert seine Credential-ID.
 *
 * Holt sich dafuer ein EIGENES Token mit dem Recht mc fuer genau diesen
 * Dienst. Das muss so: mc und ga verlangen eine RP-ID, und MakeCredential
 * raeumt danach alle Rechte ausser lbw weg (6.5.5.7.1). Ein Token, mit dem
 * erst ein Zugang angelegt und dann verwaltet wird, gibt es nicht - das
 * Programm am Rechner muss ein neues holen, und genau das spielt diese
 * Pruefung nach. */
static size_t make_rk(platform_t *p, const char *rp_id, const char *user, uint8_t *cred_id_out,
		      size_t cap)
{
	h_msg_t m;
	uint8_t hash[32];
	uint8_t auth[32];
	uint8_t tok[32];
	uint8_t code = 0;
	const uint8_t *ad = NULL;
	size_t ad_len = 0;

	T_TRUE("Token fuer mc", get_token(p, "123456", OSK_PERM_MC, rp_id, tok, &code));
	osk_sha256((const uint8_t *)"clientData", 10, hash);
	const size_t auth_len = platform_mac(p, tok, 32, hash, 32, auth);
	h_make_credential(&m, rp_id, user, true, false, 0, auth, auth_len, p->protocol);
	T_EQ("MakeCredential ok", h_call(m.buf, m.len, g_body, sizeof(g_body), &g_len), CTAP2_OK);
	T_TRUE("authData da", h_get_bytes(g_body, g_len, 0x02, &ad, &ad_len));
	if (ad_len < 56)
		return 0;
	const size_t id_len = ((size_t)ad[53] << 8) | ad[54];
	T_TRUE("Credential-ID passt", id_len > 0 && id_len <= cap);
	if (id_len == 0 || id_len > cap)
		return 0;
	memcpy(cred_id_out, ad + 55, id_len);
	return id_len;
}

/* PIN setzen, Kern starten. */
static void setup(platform_t *p)
{
	mock_reset_all();
	T_EQ("Kern startet", osk_ctap_init(OSK_CTAP_EXT_HMAC_SECRET | OSK_CTAP_EXT_CRED_PROTECT),
	     OSK_OK);
	platform_init(p, 2);
	set_pin(p, "123456");
}

/* Ein frisches Token mit dem Recht cm. rp_id darf NULL sein (dann gilt es
 * fuer alle Dienste). */
static void cm_token(platform_t *p, uint8_t token[32], const char *rp_id)
{
	uint8_t code = 0;

	T_TRUE("Token mit cm", get_token(p, "123456", OSK_PERM_CM, rp_id, token, &code));
}

/* ---- Faelle ------------------------------------------------------------ */

static void test_metadata(void)
{
	platform_t p;
	uint8_t token[32];
	uint8_t cred_id[128];

	setup(&p);
	cm_token(&p, token, NULL);

	T_EQ("getCredsMetadata ok", cm_call(&p, token, SUB_METADATA, NULL, 0), CTAP2_OK);
	osk_cbor_reader_t r;
	uint64_t used = 99, left = 0;
	T_TRUE("Anzahl da", h_find(g_body, g_len, 0x01, &r));
	T_EQ("Anzahl lesbar", osk_cbor_read_uint(&r, &used), OSK_OK);
	T_EQ("noch kein Zugang", used, 0);
	T_TRUE("Rest da", h_find(g_body, g_len, 0x02, &r));
	T_EQ("Rest lesbar", osk_cbor_read_uint(&r, &left), OSK_OK);
	T_EQ("Rest ist die ganze Zahl", left, OSK_MAX_RESIDENT);

	/* Ein Token wird von der VERWALTUNG nicht verbraucht: derselbe
	 * Aufruf geht gleich noch einmal durch. */
	T_EQ("getCredsMetadata zweimal mit demselben Token",
	     cm_call(&p, token, SUB_METADATA, NULL, 0), CTAP2_OK);

	/* MakeCredential raeumt dagegen alle Rechte ausser lbw weg
	 * (6.5.5.7.1). Danach braucht es ein neues Token - nicht, weil hier
	 * etwas kaputt ist, sondern weil die Spezifikation es so will. */
	T_TRUE("Zugang angelegt",
	       make_rk(&p, "example.com", "anna", cred_id, sizeof(cred_id)) > 0);
	T_EQ("Recht nach MakeCredential weg", cm_call(&p, token, SUB_METADATA, NULL, 0),
	     CTAP2_ERR_PIN_AUTH_INVALID);
	cm_token(&p, token, NULL);

	T_EQ("getCredsMetadata erneut", cm_call(&p, token, SUB_METADATA, NULL, 0), CTAP2_OK);
	T_TRUE("Anzahl da", h_find(g_body, g_len, 0x01, &r));
	T_EQ("Anzahl lesbar", osk_cbor_read_uint(&r, &used), OSK_OK);
	T_EQ("jetzt einer", used, 1);
	T_TRUE("Rest da", h_find(g_body, g_len, 0x02, &r));
	T_EQ("Rest lesbar", osk_cbor_read_uint(&r, &left), OSK_OK);
	T_EQ("einer weniger frei", left, OSK_MAX_RESIDENT - 1);
}

static void test_ohne_freigabe(void)
{
	platform_t p;
	uint8_t token[32];

	setup(&p);
	cm_token(&p, token, NULL);

	/* Ohne pinUvAuthParam */
	T_EQ("ohne MAC", cm_call(&p, NULL, SUB_METADATA, NULL, 0), CTAP2_ERR_PUAT_REQUIRED);

	/* Mit falschem Token */
	uint8_t falsch[32];
	memcpy(falsch, token, 32);
	falsch[0] ^= 0xff;
	T_EQ("falscher MAC", cm_call(&p, falsch, SUB_METADATA, NULL, 0),
	     CTAP2_ERR_PIN_AUTH_INVALID);

	/* Richtiges Token, aber ohne das Recht cm. */
	uint8_t code = 0;
	uint8_t nur_ga[32];
	T_TRUE("Token ohne cm", get_token(&p, "123456", OSK_PERM_GA, "example.com", nur_ga, &code));
	T_EQ("Recht fehlt", cm_call(&p, nur_ga, SUB_METADATA, NULL, 0), CTAP2_ERR_PIN_AUTH_INVALID);

	/* Ein MAC ueber einen ANDEREN Unterbefehl darf nicht passen: sonst
	 * liesse sich ein "zaehlen" in ein "loeschen" umdeuten. */
	uint8_t msg[256];
	uint8_t mac[32];
	osk_cbor_writer_t w;
	const uint8_t wrong_sub = SUB_METADATA;
	const size_t mac_len = platform_mac(&p, token, 32, &wrong_sub, 1, mac);
	msg[0] = OSK_CTAP_CMD_CRED_MGMT;
	osk_cbor_writer_init(&w, msg + 1, sizeof(msg) - 1);
	osk_cbor_put_map(&w, 3);
	osk_cbor_put_uint(&w, 0x01);
	osk_cbor_put_uint(&w, SUB_RPS_BEGIN); /* anderer Unterbefehl! */
	osk_cbor_put_uint(&w, 0x03);
	osk_cbor_put_uint(&w, p.protocol);
	osk_cbor_put_uint(&w, 0x04);
	osk_cbor_put_bytes(&w, mac, mac_len);
	T_EQ("MAC vom falschen Unterbefehl",
	     h_call(msg, 1 + w.len, g_body, sizeof(g_body), &g_len), CTAP2_ERR_PIN_AUTH_INVALID);
}

static void test_enumerate_rps(void)
{
	platform_t p;
	uint8_t token[32];
	uint8_t cred_id[128];
	osk_cbor_reader_t r;
	uint64_t total = 0;

	setup(&p);
	cm_token(&p, token, NULL);

	/* Ohne einen einzigen Zugang: kein Dienst zu melden. */
	T_EQ("leer", cm_call(&p, token, SUB_RPS_BEGIN, NULL, 0), CTAP2_ERR_NO_CREDENTIALS);

	/* Zwei Dienste, einer davon mit zwei Konten. */
	T_TRUE("1", make_rk(&p, "example.com", "anna", cred_id, sizeof(cred_id)) > 0);
	T_TRUE("2", make_rk(&p, "example.com", "bert", cred_id, sizeof(cred_id)) > 0);
	T_TRUE("3", make_rk(&p, "zweiter.example", "anna", cred_id, sizeof(cred_id)) > 0);
	cm_token(&p, token, NULL);

	T_EQ("enumerateRPsBegin ok", cm_call(&p, token, SUB_RPS_BEGIN, NULL, 0), CTAP2_OK);
	T_TRUE("totalRPs da", h_find(g_body, g_len, 0x05, &r));
	T_EQ("totalRPs lesbar", osk_cbor_read_uint(&r, &total), OSK_OK);
	/* ZWEI, nicht drei: zwei Konten bei einem Dienst sind EIN Dienst. */
	T_EQ("Dienste ohne Doppel gezaehlt", total, 2);

	/* Die rpIdHash der Antwort muss zum genannten Namen passen. */
	const uint8_t *hash = NULL;
	size_t hash_len = 0;
	T_TRUE("rpIdHash da", h_get_bytes(g_body, g_len, 0x04, &hash, &hash_len));
	T_EQ("rpIdHash 32 Bytes", (long long)hash_len, 32);
	T_TRUE("rp da", h_find(g_body, g_len, 0x03, &r));
	size_t fields = 0;
	T_EQ("rp ist Abbildung", osk_cbor_enter_map(&r, &fields), OSK_OK);
	const char *k = NULL, *v = NULL;
	size_t klen = 0, vlen = 0;
	T_EQ("Schluessel", osk_cbor_read_text(&r, &k, &klen), OSK_OK);
	T_TRUE("heisst id", klen == 2 && memcmp(k, "id", 2) == 0);
	T_EQ("Wert", osk_cbor_read_text(&r, &v, &vlen), OSK_OK);
	uint8_t want[32];
	osk_sha256((const uint8_t *)v, vlen, want);
	T_TRUE("Name und Hash gehoeren zusammen", memcmp(want, hash, 32) == 0);

	/* Der zweite Dienst, dann Schluss. */
	T_EQ("GetNextRP ok", cm_call(&p, NULL, SUB_RPS_NEXT, NULL, 0), CTAP2_OK);
	T_TRUE("kein totalRPs mehr", !h_find(g_body, g_len, 0x05, &r));
	const uint8_t *hash2 = NULL;
	size_t hash2_len = 0;
	T_TRUE("rpIdHash da", h_get_bytes(g_body, g_len, 0x04, &hash2, &hash2_len));
	T_TRUE("anderer Dienst als der erste", memcmp(hash2, want, 32) != 0);
	T_EQ("dann ist Schluss", cm_call(&p, NULL, SUB_RPS_NEXT, NULL, 0),
	     CTAP2_ERR_NO_CREDENTIALS);
	T_EQ("und bleibt Schluss", cm_call(&p, NULL, SUB_RPS_NEXT, NULL, 0),
	     CTAP2_ERR_NOT_ALLOWED);
}

static void test_aufzaehlung_bricht_ab(void)
{
	platform_t p;
	uint8_t token[32];
	uint8_t cred_id[128];

	setup(&p);
	T_TRUE("1", make_rk(&p, "example.com", "anna", cred_id, sizeof(cred_id)) > 0);
	T_TRUE("2", make_rk(&p, "zweiter.example", "anna", cred_id, sizeof(cred_id)) > 0);
	cm_token(&p, token, NULL);

	/* GetNext ohne Begin ist nicht erlaubt - sonst waere es ein Weg,
	 * ohne Freigabe zu lesen. */
	T_EQ("GetNext ohne Begin", cm_call(&p, NULL, SUB_RPS_NEXT, NULL, 0),
	     CTAP2_ERR_NOT_ALLOWED);

	T_EQ("Begin", cm_call(&p, token, SUB_RPS_BEGIN, NULL, 0), CTAP2_OK);

	/* Ein anderer Befehl dazwischen beendet die Aufzaehlung. */
	const uint8_t info[1] = {OSK_CTAP_CMD_GET_INFO};
	T_EQ("GetInfo dazwischen", h_call(info, 1, g_body, sizeof(g_body), &g_len), CTAP2_OK);
	T_EQ("danach kein GetNext", cm_call(&p, NULL, SUB_RPS_NEXT, NULL, 0),
	     CTAP2_ERR_NOT_ALLOWED);

	/* Auch ein anderer Unterbefehl der Verwaltung beendet sie. */
	T_EQ("Begin", cm_call(&p, token, SUB_RPS_BEGIN, NULL, 0), CTAP2_OK);
	T_EQ("Metadata dazwischen", cm_call(&p, token, SUB_METADATA, NULL, 0), CTAP2_OK);
	T_EQ("danach kein GetNext", cm_call(&p, NULL, SUB_RPS_NEXT, NULL, 0),
	     CTAP2_ERR_NOT_ALLOWED);
}

static void test_enumerate_creds(void)
{
	platform_t p;
	uint8_t token[32];
	uint8_t cred_a[128], cred_b[128];
	uint8_t params[64];
	uint8_t rp_hash[32];
	osk_cbor_reader_t r;
	uint64_t total = 0, prot = 0;

	setup(&p);
	const size_t a_len = make_rk(&p, "example.com", "anna", cred_a, sizeof(cred_a));
	const size_t b_len = make_rk(&p, "example.com", "bert", cred_b, sizeof(cred_b));
	cm_token(&p, token, NULL);
	T_TRUE("zwei Zugaenge", a_len > 0 && b_len > 0);
	osk_sha256((const uint8_t *)"example.com", 11, rp_hash);

	const size_t plen = params_rp_hash(rp_hash, params, sizeof(params));
	T_EQ("enumerateCredentialsBegin ok", cm_call(&p, token, SUB_CREDS_BEGIN, params, plen),
	     CTAP2_OK);
	T_TRUE("totalCredentials da", h_find(g_body, g_len, 0x09, &r));
	T_EQ("lesbar", osk_cbor_read_uint(&r, &total), OSK_OK);
	T_EQ("zwei Konten", total, 2);

	/* credentialID, publicKey und credProtect muessen da sein. */
	const uint8_t *id = NULL;
	size_t id_len = 0;
	T_TRUE("credentialID da", h_find(g_body, g_len, 0x07, &r));
	size_t fields = 0;
	T_EQ("Abbildung", osk_cbor_enter_map(&r, &fields), OSK_OK);
	const char *k = NULL;
	size_t klen = 0;
	T_EQ("Schluessel", osk_cbor_read_text(&r, &k, &klen), OSK_OK);
	T_TRUE("heisst id", klen == 2 && memcmp(k, "id", 2) == 0);
	T_EQ("Wert", osk_cbor_read_bytes(&r, &id, &id_len), OSK_OK);
	T_TRUE("eine unserer beiden IDs",
	       (id_len == a_len && memcmp(id, cred_a, id_len) == 0) ||
		   (id_len == b_len && memcmp(id, cred_b, id_len) == 0));
	/* KOPIEREN: der Zeiger zeigt in g_body, und der naechste Aufruf
	 * schreibt g_body neu. Ein Vergleich gegen diesen Zeiger haette
	 * nachher die neue Antwort mit sich selbst verglichen - und immer
	 * "gleich" gesagt. */
	uint8_t erste_id[128];
	T_TRUE("ID passt in den Puffer", id_len <= sizeof(erste_id));
	memcpy(erste_id, id, (id_len <= sizeof(erste_id)) ? id_len : sizeof(erste_id));
	const size_t erste_len = id_len;

	T_TRUE("publicKey da", h_find(g_body, g_len, 0x08, &r));
	size_t cose = 0;
	T_EQ("COSE_Key ist eine Abbildung", osk_cbor_enter_map(&r, &cose), OSK_OK);
	T_EQ("fuenf Felder", (long long)cose, 5);

	T_TRUE("credProtect da", h_find(g_body, g_len, 0x0a, &r));
	T_EQ("lesbar", osk_cbor_read_uint(&r, &prot), OSK_OK);
	T_EQ("Vorgabestufe", prot, OSK_CRED_PROTECT_OPTIONAL);

	/* Der Nutzername faehrt mit. */
	T_TRUE("user da", h_find(g_body, g_len, 0x06, &r));
	T_EQ("Abbildung", osk_cbor_enter_map(&r, &fields), OSK_OK);
	T_EQ("id und name", (long long)fields, 2);

	T_EQ("zweites Konto", cm_call(&p, NULL, SUB_CREDS_NEXT, NULL, 0), CTAP2_OK);
	T_TRUE("ohne total", !h_find(g_body, g_len, 0x09, &r));
	const uint8_t *id2 = NULL;
	size_t id2_len = 0;
	T_TRUE("credentialID da", h_find(g_body, g_len, 0x07, &r));
	T_EQ("Abbildung", osk_cbor_enter_map(&r, &fields), OSK_OK);
	T_EQ("Schluessel", osk_cbor_read_text(&r, &k, &klen), OSK_OK);
	T_EQ("Wert", osk_cbor_read_bytes(&r, &id2, &id2_len), OSK_OK);
	T_TRUE("und es ist das ANDERE",
	       id2_len != erste_len || memcmp(id2, erste_id, erste_len) != 0);

	T_EQ("dann Schluss", cm_call(&p, NULL, SUB_CREDS_NEXT, NULL, 0), CTAP2_ERR_NO_CREDENTIALS);

	/* Ein Dienst ohne Zugaenge. */
	uint8_t leer_hash[32];
	osk_sha256((const uint8_t *)"niemand.example", 15, leer_hash);
	const size_t plen2 = params_rp_hash(leer_hash, params, sizeof(params));
	T_EQ("unbekannter Dienst", cm_call(&p, token, SUB_CREDS_BEGIN, params, plen2),
	     CTAP2_ERR_NO_CREDENTIALS);

	/* Ohne rpIdHash fehlt ein Pflichtfeld. */
	T_EQ("ohne Parameter", cm_call(&p, token, SUB_CREDS_BEGIN, NULL, 0),
	     CTAP2_ERR_MISSING_PARAMETER);
}

static void test_loeschen(void)
{
	platform_t p;
	uint8_t token[32];
	uint8_t cred_a[128], cred_b[128];
	uint8_t params[256];
	h_msg_t m;

	setup(&p);
	const size_t a_len = make_rk(&p, "example.com", "anna", cred_a, sizeof(cred_a));
	const size_t b_len = make_rk(&p, "example.com", "bert", cred_b, sizeof(cred_b));
	cm_token(&p, token, NULL);
	T_TRUE("zwei Zugaenge", a_len > 0 && b_len > 0);
	T_EQ("zwei im Tresor", (long long)mock_store_count(), 2);

	/* Eine ID, die nicht von diesem Geraet stammt, fuehrt zu nichts -
	 * und verraet auch nichts anderes als "nicht gefunden". */
	uint8_t fremd[128];
	memcpy(fremd, cred_a, a_len);
	fremd[20] ^= 0x01;
	size_t plen = params_cred(fremd, a_len, NULL, 0, NULL, params, sizeof(params));
	T_EQ("verbogene ID", cm_call(&p, token, SUB_DELETE, params, plen),
	     CTAP2_ERR_NO_CREDENTIALS);
	T_EQ("nichts geloescht", (long long)mock_store_count(), 2);

	/* Ein nicht auffindbarer Zugang hat keinen Eintrag - es gibt daran
	 * nichts zu loeschen. */
	uint8_t nicht_rk[128];
	h_make_credential(&m, "example.com", "carl", false, false, 0, NULL, 0, 0);
	T_EQ("nicht auffindbar angelegt", h_call(m.buf, m.len, g_body, sizeof(g_body), &g_len),
	     CTAP2_OK);
	{
		const uint8_t *ad = NULL;
		size_t ad_len = 0;
		T_TRUE("authData", h_get_bytes(g_body, g_len, 0x02, &ad, &ad_len));
		const size_t n = ((size_t)ad[53] << 8) | ad[54];
		T_TRUE("ID passt", n > 0 && n <= sizeof(nicht_rk));
		memcpy(nicht_rk, ad + 55, n);
		plen = params_cred(nicht_rk, n, NULL, 0, NULL, params, sizeof(params));
	}
	/* Auch dieses MakeCredential hat die Rechte geraeumt. */
	cm_token(&p, token, NULL);
	T_EQ("nicht auffindbar: nichts zu loeschen", cm_call(&p, token, SUB_DELETE, params, plen),
	     CTAP2_ERR_NO_CREDENTIALS);

	/* Jetzt der echte Fall. */
	plen = params_cred(cred_a, a_len, NULL, 0, NULL, params, sizeof(params));
	T_EQ("deleteCredential ok", cm_call(&p, token, SUB_DELETE, params, plen), CTAP2_OK);
	T_EQ("einer weg", (long long)mock_store_count(), 1);

	/* Und er ist auch zum Anmelden weg. */
	build_get_assertion(&m, "example.com", cred_a, a_len, NULL, 0, 0);
	T_EQ("geloeschter Zugang meldet nicht mehr an",
	     h_call(m.buf, m.len, g_body, sizeof(g_body), &g_len), CTAP2_ERR_NO_CREDENTIALS);

	/* Der andere lebt. */
	build_get_assertion(&m, "example.com", cred_b, b_len, NULL, 0, 0);
	T_EQ("der andere lebt", h_call(m.buf, m.len, g_body, sizeof(g_body), &g_len), CTAP2_OK);

	/* Auch GetAssertion raeumt die Rechte weg (6.5.5.7.1), nicht nur
	 * MakeCredential. Also noch einmal ein Token. */
	cm_token(&p, token, NULL);

	/* Zweimal loeschen ist einmal zu viel. */
	plen = params_cred(cred_a, a_len, NULL, 0, NULL, params, sizeof(params));
	T_EQ("zweites Loeschen", cm_call(&p, token, SUB_DELETE, params, plen),
	     CTAP2_ERR_NO_CREDENTIALS);
}

static void test_nutzerangaben(void)
{
	platform_t p;
	uint8_t token[32];
	uint8_t cred[128];
	uint8_t params[256];
	uint8_t rp_hash[32];
	osk_cbor_reader_t r;

	setup(&p);
	const size_t len = make_rk(&p, "example.com", "anna", cred, sizeof(cred));
	cm_token(&p, token, NULL);
	T_TRUE("Zugang da", len > 0);
	osk_sha256((const uint8_t *)"example.com", 11, rp_hash);

	/* Die Nutzerkennung ist in h_make_credential der Name als Bytes. */
	const uint8_t *uid = (const uint8_t *)"anna";

	/* Ohne user-Abbildung fehlt ein Pflichtfeld. */
	size_t plen = params_cred(cred, len, NULL, 0, NULL, params, sizeof(params));
	T_EQ("ohne user", cm_call(&p, token, SUB_UPDATE_USER, params, plen),
	     CTAP2_ERR_MISSING_PARAMETER);

	/* Eine ANDERE Nutzerkennung wird abgewiesen: sonst haenge man einen
	 * vorhandenen Passkey still auf ein anderes Konto um. */
	plen = params_cred(cred, len, (const uint8_t *)"bert", 4, "Bert B.", params,
			   sizeof(params));
	T_EQ("fremde Nutzerkennung", cm_call(&p, token, SUB_UPDATE_USER, params, plen),
	     CTAP1_ERR_INVALID_PARAMETER);

	/* Der Name darf sich aendern. */
	plen = params_cred(cred, len, uid, 4, "Anna Neu", params, sizeof(params));
	T_EQ("updateUserInformation ok", cm_call(&p, token, SUB_UPDATE_USER, params, plen),
	     CTAP2_OK);
	T_EQ("kein zweiter Eintrag daneben", (long long)mock_store_count(), 1);

	/* Und die Aufzaehlung zeigt den neuen Namen. */
	const size_t rplen = params_rp_hash(rp_hash, params, sizeof(params));
	T_EQ("Aufzaehlung", cm_call(&p, token, SUB_CREDS_BEGIN, params, rplen), CTAP2_OK);
	T_TRUE("user da", h_find(g_body, g_len, 0x06, &r));
	size_t fields = 0;
	T_EQ("Abbildung", osk_cbor_enter_map(&r, &fields), OSK_OK);
	const char *k = NULL;
	size_t klen = 0;
	const uint8_t *got_id = NULL;
	size_t got_id_len = 0;
	T_EQ("id", osk_cbor_read_text(&r, &k, &klen), OSK_OK);
	T_EQ("Wert", osk_cbor_read_bytes(&r, &got_id, &got_id_len), OSK_OK);
	T_TRUE("Kennung unveraendert", got_id_len == 4 && memcmp(got_id, uid, 4) == 0);
	const char *name = NULL;
	size_t name_len = 0;
	T_EQ("name", osk_cbor_read_text(&r, &k, &klen), OSK_OK);
	T_EQ("Wert", osk_cbor_read_text(&r, &name, &name_len), OSK_OK);
	T_TRUE("neuer Name", name_len == 8 && memcmp(name, "Anna Neu", 8) == 0);

	/* Der Zugang meldet danach noch an - der Satz ist heil geblieben. */
	h_msg_t m;
	build_get_assertion(&m, "example.com", cred, len, NULL, 0, 0);
	T_EQ("meldet noch an", h_call(m.buf, m.len, g_body, sizeof(g_body), &g_len), CTAP2_OK);
}

/* Ein Token, das auf EINEN Dienst festgelegt ist, darf auch nur dessen
 * Zugaenge sehen und loeschen. */
static void test_token_auf_einen_dienst(void)
{
	platform_t p;
	uint8_t token[32];
	uint8_t cred_fremd[128];
	uint8_t params[256];
	uint8_t rp_hash[32];
	uint8_t code = 0;
	osk_cbor_reader_t r;
	uint64_t total = 0;

	setup(&p);
	T_TRUE("eigener", make_rk(&p, "example.com", "anna", params, sizeof(params)) > 0);
	const size_t fremd_len =
	    make_rk(&p, "zweiter.example", "anna", cred_fremd, sizeof(cred_fremd));
	cm_token(&p, token, NULL);
	T_TRUE("fremder", fremd_len > 0);

	/* Jetzt ein Token nur fuer example.com. */
	T_TRUE("Token mit Dienst",
	       get_token(&p, "123456", OSK_PERM_CM, "example.com", token, &code));

	osk_sha256((const uint8_t *)"example.com", 11, rp_hash);
	size_t plen = params_rp_hash(rp_hash, params, sizeof(params));
	T_EQ("eigener Dienst geht", cm_call(&p, token, SUB_CREDS_BEGIN, params, plen), CTAP2_OK);

	uint8_t fremd_hash[32];
	osk_sha256((const uint8_t *)"zweiter.example", 15, fremd_hash);
	plen = params_rp_hash(fremd_hash, params, sizeof(params));
	T_EQ("fremder Dienst nicht", cm_call(&p, token, SUB_CREDS_BEGIN, params, plen),
	     CTAP2_ERR_PIN_AUTH_INVALID);

	/* Auch loeschen nicht. */
	plen = params_cred(cred_fremd, fremd_len, NULL, 0, NULL, params, sizeof(params));
	T_EQ("fremder Zugang nicht loeschbar", cm_call(&p, token, SUB_DELETE, params, plen),
	     CTAP2_ERR_PIN_AUTH_INVALID);
	T_EQ("nichts geloescht", (long long)mock_store_count(), 2);

	/* Und die Dienste-Liste zeigt nur den einen. */
	T_EQ("enumerateRPs", cm_call(&p, token, SUB_RPS_BEGIN, NULL, 0), CTAP2_OK);
	T_TRUE("totalRPs da", h_find(g_body, g_len, 0x05, &r));
	T_EQ("lesbar", osk_cbor_read_uint(&r, &total), OSK_OK);
	T_EQ("nur der eigene Dienst", total, 1);
}

static void test_unbekannter_unterbefehl(void)
{
	platform_t p;
	uint8_t token[32];

	setup(&p);
	cm_token(&p, token, NULL);
	T_EQ("0x08 gibt es nicht", cm_call(&p, token, 0x08, NULL, 0),
	     CTAP2_ERR_INVALID_SUBCOMMAND);
	T_EQ("0x00 auch nicht", cm_call(&p, token, 0x00, NULL, 0), CTAP2_ERR_INVALID_SUBCOMMAND);
}

static void test_ohne_pin(void)
{
	platform_t p;

	mock_reset_all();
	T_EQ("Kern startet", osk_ctap_init(0), OSK_OK);
	platform_init(&p, 2);

	/* Ohne PIN gibt es kein Token - und damit keine Verwaltung. */
	uint8_t kein_token[32];
	memset(kein_token, 0x11, sizeof(kein_token));
	T_EQ("ohne PIN", cm_call(&p, kein_token, SUB_METADATA, NULL, 0), CTAP2_ERR_PIN_NOT_SET);
}

int main(void)
{
	test_metadata();
	test_ohne_freigabe();
	test_enumerate_rps();
	test_aufzaehlung_bricht_ab();
	test_enumerate_creds();
	test_loeschen();
	test_nutzerangaben();
	test_token_auf_einen_dienst();
	test_unbekannter_unterbefehl();
	test_ohne_pin();
	return t_report("credmgmt");
}
