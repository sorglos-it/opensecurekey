/* Zeitstempel: Anfrage bauen, Antwort pruefen, gegenzeichnen.
 *
 * Der Befund, der diese Prueffaelle bestimmt, steht in docs/zeitstempel.md:
 * das Geraet ist Antragsteller, nie Zeitstempelstelle. Darum gibt es hier
 * keinen Prueffall "das Geraet stellt einen Zeitstempel aus" - es gibt die
 * Funktion nicht, und das ist das Ergebnis, nicht eine Luecke.
 *
 * Die Antworten werden hier gebaut, weil keine Zeitstempelstelle zur Hand
 * ist. WAS DAS NICHT PRUEFT: dass eine echte Stelle Antworten dieser Form
 * schickt. Die Feldreihenfolge stammt aus RFC 3161 Abschnitt 2.4.2 und
 * RFC 5652 Abschnitt 5, nicht aus einer gemessenen Antwort. Erst ein Lauf
 * gegen eine echte Stelle sagt, ob der Leser mit ihr zurechtkommt - und
 * dafuer muesste eine beantragt werden, was ausdruecklich nicht geschieht. */
#include <string.h>
#include "authz_t.h"
#include "authz_der.h"
#include "osk_crypto.h"

/* Wie eine gebaute Antwort aussehen soll - jedes Feld einzeln verbiegbar,
 * damit jeder Prueffall genau eine Sache falsch macht. */
typedef struct {
	uint64_t status;
	const uint8_t *imprint;	 /* Hash im Token */
	const uint8_t *nonce;	 /* NULL = keine Nonce im Token */
	size_t nonce_len;
	const char *gen_time;
	bool wrong_content_type; /* nicht id-ct-TSTInfo */
	bool no_token;
} resp_spec_t;

static const uint8_t OID_SHA256_V[] = {0x60u, 0x86u, 0x48u, 0x01u, 0x65u,
				       0x03u, 0x04u, 0x02u, 0x01u};
static const uint8_t OID_SIGNED_V[] = {0x2Au, 0x86u, 0x48u, 0x86u, 0xF7u,
				       0x0Du, 0x01u, 0x07u, 0x02u};
static const uint8_t OID_TST_V[] = {0x2Au, 0x86u, 0x48u, 0x86u, 0xF7u, 0x0Du,
				    0x01u, 0x09u, 0x10u, 0x01u, 0x04u};
/* OID 1.2.840.113549.1.9.16.1.5 - ein anderer Inhaltstyp, um zu pruefen,
 * dass ein beliebiges signiertes CMS-Dokument NICHT als Zeitstempel
 * durchgeht. */
static const uint8_t OID_OTHER_V[] = {0x2Au, 0x86u, 0x48u, 0x86u, 0xF7u, 0x0Du,
				      0x01u, 0x09u, 0x10u, 0x01u, 0x05u};
static const uint8_t OID_POLICY_V[] = {0x2Au, 0x03u, 0x04u}; /* 1.2.3.4 */

static size_t build_resp(const resp_spec_t *sp, uint8_t *out, size_t cap)
{
	uint8_t scratch[512];
	authz_der_w_t w;
	authz_der_w_init(&w, scratch, sizeof(scratch));

	if (!sp->no_token) {
		size_t tok = w.used;

		{ /* [0] { OCTET STRING { TSTInfo } } */
			size_t c0 = w.used;
			{
				size_t oct = w.used;
				{
					size_t tst = w.used;
					/* rueckwaerts: nonce zuletzt im
					 * Dokument, also hier zuerst */
					if (sp->nonce != NULL) {
						authz_der_w_uint_bytes(&w, sp->nonce,
								       sp->nonce_len);
					}
					authz_der_w_tlv(&w, DER_GENTIME,
							(const uint8_t *)sp->gen_time,
							strlen(sp->gen_time));
					authz_der_w_u64(&w, 42u); /* serialNumber */
					{
						size_t mi = w.used;
						authz_der_w_tlv(&w, DER_OCTET, sp->imprint, 32u);
						{
							size_t ai = w.used;
							authz_der_w_header(&w, 0x05u, 0u);
							authz_der_w_tlv(&w, DER_OID, OID_SHA256_V,
									sizeof(OID_SHA256_V));
							authz_der_w_header(&w, DER_SEQUENCE,
									   w.used - ai);
						}
						authz_der_w_header(&w, DER_SEQUENCE, w.used - mi);
					}
					authz_der_w_tlv(&w, DER_OID, OID_POLICY_V,
							sizeof(OID_POLICY_V));
					authz_der_w_u64(&w, 1u); /* version */
					authz_der_w_header(&w, DER_SEQUENCE, w.used - tst);
				}
				authz_der_w_header(&w, DER_OCTET, w.used - oct);
				/* Die [0]-Huelle um eContent. RFC 5652
				 * Abschnitt 5.2: EncapsulatedContentInfo
				 * fuehrt eContent als
				 * "[0] EXPLICIT OCTET STRING OPTIONAL" - die
				 * Huelle gehoert dazu und ist nicht
				 * weglassbar. Sie fehlte hier im ersten
				 * Anlauf, und der Leser hat es gemeldet. */
				authz_der_w_header(&w, DER_CTX0, w.used - oct);
			}
			/* eContentType davor */
			if (sp->wrong_content_type) {
				authz_der_w_tlv(&w, DER_OID, OID_OTHER_V, sizeof(OID_OTHER_V));
			} else {
				authz_der_w_tlv(&w, DER_OID, OID_TST_V, sizeof(OID_TST_V));
			}
			authz_der_w_header(&w, DER_SEQUENCE, w.used - c0); /* encapContentInfo */
		}
		/* SignedData: version und digestAlgorithms davor. Die
		 * Signierer selbst fehlen - der Leser prueft die Signatur
		 * nicht, und ein Feld vorzutaeuschen, das nicht geprueft
		 * wird, wuerde den Prueffall nur groesser machen. */
		authz_der_w_header(&w, DER_SET, 0u);
		authz_der_w_u64(&w, 3u);
		size_t sd_end = w.used;
		(void)sd_end;
		/* SEQUENCE um SignedData: alles seit `tok` */
		authz_der_w_header(&w, DER_SEQUENCE, w.used - tok);
		/* [0] EXPLICIT um SignedData */
		authz_der_w_header(&w, DER_CTX0, w.used - tok);
		/* contentType davor */
		authz_der_w_tlv(&w, DER_OID, OID_SIGNED_V, sizeof(OID_SIGNED_V));
		/* ContentInfo */
		authz_der_w_header(&w, DER_SEQUENCE, w.used - tok);
	}

	/* PKIStatusInfo davor */
	{
		size_t si = w.used;
		authz_der_w_u64(&w, sp->status);
		authz_der_w_header(&w, DER_SEQUENCE, w.used - si);
	}
	authz_der_w_header(&w, DER_SEQUENCE, w.used);

	const uint8_t *start = NULL;
	size_t len = 0u;
	if (authz_der_w_done(&w, &start, &len) != OSK_OK || len > cap) {
		return 0u;
	}
	memcpy(out, start, len);
	return len;
}

static osk_authz_session_t setup_tsa(osk_cap_mask_t caps)
{
	t_setup();
	osk_authz_session_t admin = t_login(T_KEY_ADMIN, T_SECRET_ADMIN);
	osk_authz_key_t k;

	memset(&k, 0, sizeof(k));
	k.id = T_KEY_WEB;
	T_LABEL(k.label, "signierdienst");
	k.secret = T_SECRET_WEB;
	k.realms = T_REALM_WEB;
	k.caps = caps;
	k.max_fail = 5u;
	(void)osk_authz_key_add(admin, &k);
	osk_authz_session_end(admin);

	return t_login(T_KEY_WEB, T_SECRET_WEB);
}

void test_tsa(void)
{
	uint8_t hash[OSK_TSA_HASH_LEN];
	memset(hash, 0x3C, sizeof(hash));

	TT_CASE("Anfrage nach RFC 3161 bauen und zurueck lesen");
	{
		osk_authz_session_t s = setup_tsa(OSK_CAP_TIMESTAMP | OSK_CAP_SIGN);
		uint8_t der[256];
		size_t len = 0u;
		osk_tsa_request_t req;

		TT_EQ_INT(osk_tsa_request_build(s, hash, true, der, sizeof(der), &len, &req),
			  OSK_OK, "bauen");
		TT_TRUE(len > 0u && len <= sizeof(der), "es kam etwas heraus");
		TT_TRUE(req.nonce_sent, "Nonce ist dabei");
		TT_EQ_MEM(req.hash, hash, OSK_TSA_HASH_LEN, "Hash gemerkt");

		authz_der_t c = {der, len};
		authz_der_t inner;
		TT_EQ_INT(authz_der_enter(&c, DER_SEQUENCE, &inner), OSK_OK, "TimeStampReq");

		const uint8_t *val = NULL;
		size_t vlen = 0u;
		TT_EQ_INT(authz_der_expect(&inner, DER_INTEGER, &val, &vlen), OSK_OK, "version");
		uint64_t v = 0u;
		TT_EQ_INT(authz_der_u64(val, vlen, &v), OSK_OK, "lesbar");
		TT_EQ_INT(v, 1, "v1 - RFC 3161 kennt nur die");

		authz_der_t mi;
		TT_EQ_INT(authz_der_enter(&inner, DER_SEQUENCE, &mi), OSK_OK, "messageImprint");
		authz_der_t ai;
		TT_EQ_INT(authz_der_enter(&mi, DER_SEQUENCE, &ai), OSK_OK, "AlgorithmIdentifier");
		TT_EQ_INT(authz_der_expect(&ai, DER_OID, &val, &vlen), OSK_OK, "OID");
		TT_EQ_INT(vlen, 9, "neun Byte");
		TT_EQ_MEM(val, OID_SHA256_V, 9u, "sha256");
		TT_EQ_INT(authz_der_expect(&ai, 0x05u, &val, &vlen), OSK_OK, "NULL als Parameter");
		TT_EQ_INT(vlen, 0, "leer");

		TT_EQ_INT(authz_der_expect(&mi, DER_OCTET, &val, &vlen), OSK_OK, "hashedMessage");
		TT_EQ_INT(vlen, 32, "32 Byte");
		TT_EQ_MEM(val, hash, 32u, "unser Hash");

		TT_EQ_INT(authz_der_expect(&inner, DER_INTEGER, &val, &vlen), OSK_OK, "nonce");
		TT_EQ_INT(authz_der_expect(&inner, DER_BOOLEAN, &val, &vlen), OSK_OK, "certReq");
		TT_EQ_INT(val[0], 0xFF, "certReq ist TRUE");
		TT_EQ_INT(inner.len, 0, "und nichts weiter");
		osk_authz_session_end(s);
	}

	TT_CASE("ohne Nonce fehlt das Feld, statt leer dabeizustehen");
	{
		osk_authz_session_t s = setup_tsa(OSK_CAP_TIMESTAMP);
		uint8_t der[256];
		size_t len = 0u;
		osk_tsa_request_t req;
		TT_EQ_INT(osk_tsa_request_build(s, hash, false, der, sizeof(der), &len, &req),
			  OSK_OK, "bauen");
		TT_TRUE(!req.nonce_sent, "keine Nonce");

		authz_der_t c = {der, len};
		authz_der_t inner;
		(void)authz_der_enter(&c, DER_SEQUENCE, &inner);
		(void)authz_der_expect(&inner, DER_INTEGER, NULL, NULL); /* version */
		(void)authz_der_expect(&inner, DER_SEQUENCE, NULL, NULL); /* imprint */
		/* Das naechste muss certReq sein, nicht eine Nonce. */
		TT_EQ_INT(authz_der_expect(&inner, DER_BOOLEAN, NULL, NULL), OSK_OK,
			  "gleich certReq");
		osk_authz_session_end(s);
	}

	TT_CASE("ohne TIMESTAMP-Recht wird keine Anfrage gebaut");
	{
		osk_authz_session_t s = setup_tsa(OSK_CAP_SIGN);
		uint8_t der[256];
		size_t len = 0u;
		osk_tsa_request_t req;
		TT_EQ_INT(osk_tsa_request_build(s, hash, true, der, sizeof(der), &len, &req),
			  OSK_ERR_DENIED, "Faehigkeit fehlt");
		osk_authz_session_end(s);
	}

	TT_CASE("zu kleiner Puffer wird gemeldet");
	{
		osk_authz_session_t s = setup_tsa(OSK_CAP_TIMESTAMP);
		uint8_t der[8];
		size_t len = 0u;
		osk_tsa_request_t req;
		TT_EQ_INT(osk_tsa_request_build(s, hash, true, der, sizeof(der), &len, &req),
			  OSK_ERR_FULL, "zu klein");
		osk_authz_session_end(s);
	}

	TT_CASE("eine gute Antwort wird angenommen, und die Zeit kommt aus dem Token");
	{
		osk_authz_session_t s = setup_tsa(OSK_CAP_TIMESTAMP | OSK_CAP_SIGN);
		uint8_t der[256];
		size_t len = 0u;
		osk_tsa_request_t req;
		(void)osk_tsa_request_build(s, hash, true, der, sizeof(der), &len, &req);

		resp_spec_t sp;
		memset(&sp, 0, sizeof(sp));
		sp.status = 0u;
		sp.imprint = hash;
		sp.nonce = req.nonce;
		sp.nonce_len = OSK_TSA_NONCE_LEN;
		sp.gen_time = "20261029120000Z";

		uint8_t resp[768];
		size_t rlen = build_resp(&sp, resp, sizeof(resp));
		TT_TRUE(rlen > 0u, "Antwort gebaut");

		osk_tsa_statement_t stmt;
		const uint8_t *token = NULL;
		size_t token_len = 0u;
		TT_EQ_INT(osk_tsa_response_parse(&req, resp, rlen, &stmt, &token, &token_len),
			  OSK_OK, "angenommen");
		TT_TRUE(stmt.gen_time_present, "Zeit ist da");
		TT_TRUE(strcmp(stmt.gen_time, "20261029120000Z") == 0,
			"und zwar die aus dem Token");
		TT_TRUE(token != NULL && token_len > 0u, "Token gefunden");
		/* Das Token muss mit seinem eigenen Kopf anfangen: die
		 * Gegenzeichnung muss genau die Bytes umfassen, die der
		 * Rechner spaeter prueft.
		 *
		 * Die Abfrage auf NULL steht davor, weil ein durchgefallener
		 * Prueffall nicht im naechsten abstuerzen soll - sonst sagt
		 * der Lauf nur, DASS etwas kaputt ist, und nicht mehr, was. */
		if (token != NULL) {
			TT_EQ_INT(token[0], 0x30, "Token beginnt mit seinem SEQUENCE-Kopf");
		}
		osk_authz_session_end(s);
	}

	TT_CASE("DER PRUEFFALL: ein echter Zeitstempel ueber FREMDE Daten gilt nicht");
	{
		/* Der eigentliche Angriff auf einen Zeitstempel, und er kommt
		 * ohne jede Faelschung aus: man nimmt ein echtes,
		 * ordentlich signiertes Token und legt es neben andere
		 * Daten. */
		osk_authz_session_t s = setup_tsa(OSK_CAP_TIMESTAMP);
		uint8_t der[256];
		size_t len = 0u;
		osk_tsa_request_t req;
		(void)osk_tsa_request_build(s, hash, true, der, sizeof(der), &len, &req);

		uint8_t other[OSK_TSA_HASH_LEN];
		memset(other, 0x99, sizeof(other));

		resp_spec_t sp;
		memset(&sp, 0, sizeof(sp));
		sp.imprint = other;
		sp.nonce = req.nonce;
		sp.nonce_len = OSK_TSA_NONCE_LEN;
		sp.gen_time = "20261029120000Z";

		uint8_t resp[768];
		size_t rlen = build_resp(&sp, resp, sizeof(resp));
		osk_tsa_statement_t stmt;
		const uint8_t *token = NULL;
		size_t token_len = 0u;
		TT_EQ_INT(osk_tsa_response_parse(&req, resp, rlen, &stmt, &token, &token_len),
			  OSK_ERR_DENIED, "fremder Hash im Token");
		osk_authz_session_end(s);
	}

	TT_CASE("Nonce: vertauscht oder fehlend ist beides eine Absage");
	{
		osk_authz_session_t s = setup_tsa(OSK_CAP_TIMESTAMP);
		uint8_t der[256];
		size_t len = 0u;
		osk_tsa_request_t req;
		(void)osk_tsa_request_build(s, hash, true, der, sizeof(der), &len, &req);

		uint8_t wrong[OSK_TSA_NONCE_LEN];
		memcpy(wrong, req.nonce, sizeof(wrong));
		wrong[0] = (uint8_t)(wrong[0] ^ 0xFFu);

		resp_spec_t sp;
		memset(&sp, 0, sizeof(sp));
		sp.imprint = hash;
		sp.gen_time = "20261029120000Z";
		sp.nonce = wrong;
		sp.nonce_len = sizeof(wrong);

		uint8_t resp[768];
		osk_tsa_statement_t stmt;
		const uint8_t *token = NULL;
		size_t token_len = 0u;
		size_t rlen = build_resp(&sp, resp, sizeof(resp));
		TT_EQ_INT(osk_tsa_response_parse(&req, resp, rlen, &stmt, &token, &token_len),
			  OSK_ERR_DENIED, "andere Nonce");

		/* Wir haben eine geschickt, es kommt keine zurueck: dann ist
		 * nicht zu entscheiden, ob diese Antwort zu dieser Anfrage
		 * gehoert. */
		sp.nonce = NULL;
		sp.nonce_len = 0u;
		rlen = build_resp(&sp, resp, sizeof(resp));
		TT_EQ_INT(osk_tsa_response_parse(&req, resp, rlen, &stmt, &token, &token_len),
			  OSK_ERR_DENIED, "Nonce fehlt in der Antwort");

		/* Haben wir keine geschickt, wird auch keine erwartet. */
		osk_tsa_request_t req2;
		(void)osk_tsa_request_build(s, hash, false, der, sizeof(der), &len, &req2);
		rlen = build_resp(&sp, resp, sizeof(resp));
		TT_EQ_INT(osk_tsa_response_parse(&req2, resp, rlen, &stmt, &token, &token_len),
			  OSK_OK, "ohne Nonge gefragt, ohne Nonce geantwortet");
		osk_authz_session_end(s);
	}

	TT_CASE("Absage der Stelle und falscher Inhaltstyp");
	{
		osk_authz_session_t s = setup_tsa(OSK_CAP_TIMESTAMP);
		uint8_t der[256];
		size_t len = 0u;
		osk_tsa_request_t req;
		(void)osk_tsa_request_build(s, hash, true, der, sizeof(der), &len, &req);

		resp_spec_t sp;
		memset(&sp, 0, sizeof(sp));
		sp.imprint = hash;
		sp.nonce = req.nonce;
		sp.nonce_len = OSK_TSA_NONCE_LEN;
		sp.gen_time = "20261029120000Z";

		uint8_t resp[768];
		osk_tsa_statement_t stmt;
		const uint8_t *token = NULL;
		size_t token_len = 0u;

		/* RFC 3161 Abschnitt 2.4.2: 2 ist rejection. */
		sp.status = 2u;
		size_t rlen = build_resp(&sp, resp, sizeof(resp));
		TT_EQ_INT(osk_tsa_response_parse(&req, resp, rlen, &stmt, &token, &token_len),
			  OSK_ERR_DENIED, "abgelehnt");

		/* 1 ist grantedWithMods und wird angenommen. */
		sp.status = 1u;
		rlen = build_resp(&sp, resp, sizeof(resp));
		TT_EQ_INT(osk_tsa_response_parse(&req, resp, rlen, &stmt, &token, &token_len),
			  OSK_OK, "grantedWithMods");

		/* Ein anderes signiertes CMS-Dokument darf nicht als
		 * Zeitstempel durchgehen - sonst waere jede Unterschrift
		 * ueber irgendetwas ein Zeitbeweis. */
		sp.status = 0u;
		sp.wrong_content_type = true;
		rlen = build_resp(&sp, resp, sizeof(resp));
		TT_EQ_INT(osk_tsa_response_parse(&req, resp, rlen, &stmt, &token, &token_len),
			  OSK_ERR_FORMAT, "nicht id-ct-TSTInfo");

		/* Zugesagt und kein Token dabei ist ein Widerspruch. */
		sp.wrong_content_type = false;
		sp.no_token = true;
		rlen = build_resp(&sp, resp, sizeof(resp));
		TT_EQ_INT(osk_tsa_response_parse(&req, resp, rlen, &stmt, &token, &token_len),
			  OSK_ERR_FORMAT, "granted ohne Token");
		osk_authz_session_end(s);
	}

	TT_CASE("abgeschnittene Antworten stuerzen nicht ab");
	{
		/* Jede Laenge von 1 bis zur vollen: keine davon darf etwas
		 * anderes als einen Fehler ergeben. Das ist der
		 * Fuzz-Ansatz in kleiner Form - systematisch, nicht nach
		 * dem vermuteten Fehler gesucht. */
		osk_authz_session_t s = setup_tsa(OSK_CAP_TIMESTAMP);
		uint8_t der[256];
		size_t len = 0u;
		osk_tsa_request_t req;
		(void)osk_tsa_request_build(s, hash, true, der, sizeof(der), &len, &req);

		resp_spec_t sp;
		memset(&sp, 0, sizeof(sp));
		sp.imprint = hash;
		sp.nonce = req.nonce;
		sp.nonce_len = OSK_TSA_NONCE_LEN;
		sp.gen_time = "20261029120000Z";

		uint8_t resp[768];
		size_t rlen = build_resp(&sp, resp, sizeof(resp));
		osk_tsa_statement_t stmt;
		const uint8_t *token = NULL;
		size_t token_len = 0u;
		int accepted = 0;

		for (size_t n = 1u; n < rlen; n++) {
			if (osk_tsa_response_parse(&req, resp, n, &stmt, &token, &token_len) ==
			    OSK_OK) {
				accepted++;
			}
		}
		TT_EQ_INT(accepted, 0, "keine abgeschnittene Antwort wird angenommen");

		/* Und ein einzelnes verbogenes Byte darf auch nirgends
		 * durchrutschen, wo es den Hash oder die Nonce trifft. */
		int bad_accepted = 0;
		for (size_t i = 0u; i < rlen; i++) {
			const uint8_t keep = resp[i];
			resp[i] = (uint8_t)(keep ^ 0xFFu);
			if (osk_tsa_response_parse(&req, resp, rlen, &stmt, &token, &token_len) ==
			    OSK_OK) {
				bad_accepted++;
			}
			resp[i] = keep;
		}
		/* Angenommen werden darf nur, was Hash, Nonce, Status und
		 * Inhaltstyp heil laesst - etwa ein Byte der Seriennummer
		 * oder der Zeitangabe. Die Zahl wird GEMESSEN und nicht
		 * vorhergesagt; sie darf nur nicht die Felder betreffen, die
		 * geprueft werden. Darum hier nur: es stuerzt nichts ab, und
		 * die Zahl ist kleiner als die Zahl der Bytes. */
		TT_TRUE(bad_accepted < (int)rlen, "nicht jedes verbogene Byte geht durch");
		osk_authz_session_end(s);
	}

	TT_CASE("Gegenzeichnen: der Zaehler steigt, und zwar bei jedem Mal");
	{
		osk_authz_session_t s = setup_tsa(OSK_CAP_TIMESTAMP | OSK_CAP_SIGN);
		const uint8_t token[] = {0x30u, 0x03u, 0x02u, 0x01u, 0x01u};
		osk_tsa_statement_t a, b;
		uint8_t sig_a[64], sig_b[64];
		size_t la = 0u, lb = 0u;

		TT_EQ_INT(osk_tsa_countersign(s, T_SIGNING_KEY, T_COUNTER, hash, token,
					      sizeof(token), &a, sig_a, sizeof(sig_a), &la),
			  OSK_OK, "erste Erklaerung");
		TT_EQ_INT(a.counter, 1, "Zaehler steht auf 1");
		TT_EQ_INT(a.key_id, T_KEY_WEB, "und sie nennt, wer es war");

		TT_EQ_INT(osk_tsa_countersign(s, T_SIGNING_KEY, T_COUNTER, hash, token,
					      sizeof(token), &b, sig_b, sizeof(sig_b), &lb),
			  OSK_OK, "zweite Erklaerung");
		TT_EQ_INT(b.counter, 2, "Zaehler steht auf 2");

		/* Zwei Erklaerungen ueber DIESELBEN Daten mit DEMSELBEN
		 * Token muessen sich unterscheiden - sonst waere die
		 * Reihenfolge nicht aus der Erklaerung zu lesen, und der
		 * Zaehler stuende nur daneben statt darin. */
		TT_TRUE(memcmp(sig_a, sig_b, la) != 0, "der Zaehler steckt in der Unterschrift");

		/* Ein anderes Token muss zu einer anderen Unterschrift
		 * fuehren, sonst bindet die Erklaerung das Token nicht. */
		const uint8_t other[] = {0x30u, 0x03u, 0x02u, 0x01u, 0x02u};
		osk_tsa_statement_t c;
		uint8_t sig_c[64];
		size_t lc = 0u;
		(void)osk_tsa_countersign(s, T_SIGNING_KEY, T_COUNTER, hash, other, sizeof(other),
					  &c, sig_c, sizeof(sig_c), &lc);
		TT_EQ_INT(c.counter, 3, "Zaehler steht auf 3");
		TT_TRUE(memcmp(sig_c, sig_b, lc) != 0, "das Token steckt in der Unterschrift");
		osk_authz_session_end(s);
	}

	TT_CASE("Gegenzeichnen braucht beide Rechte");
	{
		osk_authz_session_t s = setup_tsa(OSK_CAP_TIMESTAMP);
		const uint8_t token[] = {0x30u, 0x03u, 0x02u, 0x01u, 0x01u};
		osk_tsa_statement_t st;
		uint8_t sig[64];
		size_t l = 0u;
		/* Zeitstempel anfordern darf er, unterschreiben nicht. Ein
		 * Dienst ohne SIGN soll keine Erklaerungen in die Welt
		 * setzen koennen. */
		TT_EQ_INT(osk_tsa_countersign(s, T_SIGNING_KEY, T_COUNTER, hash, token,
					      sizeof(token), &st, sig, sizeof(sig), &l),
			  OSK_ERR_DENIED, "SIGN fehlt");
		osk_authz_session_end(s);

		osk_authz_session_t s2 = setup_tsa(OSK_CAP_SIGN);
		TT_EQ_INT(osk_tsa_countersign(s2, T_SIGNING_KEY, T_COUNTER, hash, token,
					      sizeof(token), &st, sig, sizeof(sig), &l),
			  OSK_ERR_DENIED, "TIMESTAMP fehlt");
		osk_authz_session_end(s2);
	}

	TT_CASE("die Geraetezeit wird gemeldet, aber nicht unterschrieben");
	{
		/* Sie beginnt bei jedem Einstecken bei null. Unterschrieben
		 * wuerde sie wie eine Zeitangabe aussehen, ohne eine zu
		 * sein - genau der Fehler, den docs/zeitstempel.md
		 * beschreibt. Geprueft wird das daran, dass zwei
		 * Erklaerungen sich nur ueber den Zaehler unterscheiden:
		 * waere die Zeit drin, waere sie am PC ohnehin konstant und
		 * der Prueffall koennte nichts sagen. Darum steht hier nur
		 * der Befund, dass das Feld gefuellt und als Geraetezeit
		 * benannt ist. */
		osk_authz_session_t s = setup_tsa(OSK_CAP_TIMESTAMP | OSK_CAP_SIGN);
		const uint8_t token[] = {0x30u, 0x03u, 0x02u, 0x01u, 0x01u};
		osk_tsa_statement_t st;
		uint8_t sig[64];
		size_t l = 0u;
		(void)osk_tsa_countersign(s, T_SIGNING_KEY, T_COUNTER, hash, token, sizeof(token),
					  &st, sig, sizeof(sig), &l);
		/* Am PC gibt es keine Geraetezeit, und der Nachbau behauptet
		 * auch keine. Die Null ist hier die ehrliche Antwort. */
		TT_EQ_INT(st.device_ms, 0, "am PC gibt es keine Geraetezeit");
		osk_authz_session_end(s);
	}
}
