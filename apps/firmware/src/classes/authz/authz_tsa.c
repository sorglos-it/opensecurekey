/* Zeitstempel nach RFC 3161 - das Geraet als Antragsteller.
 *
 * BEFUND, DER DIESE DATEI BESTIMMT (ausfuehrlich in docs/zeitstempel.md):
 * Das Geraet hat keinen Akku, seine Uhr wird beim Einstecken vom Rechner
 * gestellt, und eine falsche Rechneruhr setzt eine falsche Geraetezeit
 * (docs/architecture.md). Ein Zeitstempel aus dieser Uhr beweist genau so
 * viel wie der Rechner, gegen den er beweisen soll - der Angreifer stellt
 * vorher seine Uhr. RFC 3161 Abschnitt 2.1 verlangt von einer
 * Zeitstempelstelle "a trustworthy source of time"; eine Uhr, die der
 * Antragsteller selbst stellt, ist die unvertrauenswuerdigste denkbare.
 *
 * DARUM: Das Geraet stellt keine Zeitstempel aus, es holt sie. Es gibt in
 * dieser Datei mit Absicht keine Funktion, die aus authz_device_ms() eine
 * Zeitangabe macht. Die Zeit kommt aus dem Token der fremden Stelle.
 *
 * Was das Geraet selbst beweist, ist die REIHENFOLGE: jede Erklaerung
 * traegt einen monotonen Chipzaehler (Strang A, osk_se050_counter_raise()),
 * der sich nicht zurueckdrehen laesst. Das ist der einzige der drei
 * Beweise, der ohne fremde Stelle und ohne Netz gilt.
 *
 * Quellen: RFC 3161 Abschnitt 2.4.1 (TimeStampReq), 2.4.2
 * (TimeStampResp, PKIStatusInfo), 2.4.2 (TSTInfo); RFC 5652 Abschnitt 3
 * und 5.2 (ContentInfo, EncapsulatedContentInfo). */
#include <string.h>
#include "osk_authz.h"
#include "authz_state.h"
#include "authz_der.h"
#include "osk_mem.h"
/* SHA-256 kommt aus ctap/osk_crypto.h und wird NICHT zum zweiten Mal
 * geschrieben. osk_wipe() lag in diesem Projekt schon einmal doppelt vor
 * und brach das Binden des Abbilds (siehe include/osk_mem.h); eine zweite
 * SHA-256 waere derselbe Fehler, nur teurer.
 *
 * DEM MASTER GEMELDET: dass diese Schicht in den Ordner eines anderen
 * Strangs greifen muss, ist ein Zeichen, dass osk_sha256() in
 * include/ gehoert und nicht in ctap/. Verschoben wird es hier nicht -
 * das waere ein Eingriff in Strang B. */
#include "osk_crypto.h"

/* OID 2.16.840.1.101.3.4.2.1 - sha256, FIPS 180-4. */
static const uint8_t OID_SHA256[] = {0x60u, 0x86u, 0x48u, 0x01u, 0x65u,
				     0x03u, 0x04u, 0x02u, 0x01u};

/* OID 1.2.840.113549.1.7.2 - id-signedData, RFC 5652 Abschnitt 5.1. */
static const uint8_t OID_SIGNED_DATA[] = {0x2Au, 0x86u, 0x48u, 0x86u, 0xF7u,
					  0x0Du, 0x01u, 0x07u, 0x02u};

/* OID 1.2.840.113549.1.9.16.1.4 - id-ct-TSTInfo, RFC 3161 Abschnitt 2.4.2. */
static const uint8_t OID_TSTINFO[] = {0x2Au, 0x86u, 0x48u, 0x86u, 0xF7u, 0x0Du,
				      0x01u, 0x09u, 0x10u, 0x01u, 0x04u};

/* --------------------------------------------------------- Anfrage bauen */

osk_result_t osk_tsa_request_build(osk_authz_session_t session,
				   const uint8_t hash[OSK_TSA_HASH_LEN], bool with_nonce,
				   uint8_t *der_out, size_t cap, size_t *len_out,
				   osk_tsa_request_t *req_out)
{
	if (hash == NULL || der_out == NULL || len_out == NULL || req_out == NULL) {
		return OSK_ERR_ARG;
	}

	osk_result_t r = osk_authz_check(session, OSK_CAP_TIMESTAMP, OSK_REALM_ALL);
	if (r != OSK_OK) {
		return r;
	}

	memset(req_out, 0, sizeof(*req_out));
	memcpy(req_out->hash, hash, OSK_TSA_HASH_LEN);

	if (with_nonce) {
		/* Die Nonce kommt aus dem Chip, nicht aus einem Zaehler: sie
		 * ist das einzige Mittel, eine alte Antwort von der zu dieser
		 * Anfrage zu unterscheiden, und muss darum unvorhersehbar
		 * sein. */
		r = osk_se050_random(req_out->nonce, OSK_TSA_NONCE_LEN);
		if (r != OSK_OK) {
			osk_wipe(req_out, sizeof(*req_out));
			return r;
		}
		req_out->nonce_sent = true;
	}

	authz_der_w_t w;
	authz_der_w_init(&w, der_out, cap);

	/* Rueckwaerts: das letzte Feld zuerst.
	 *
	 * certReq TRUE, damit die Stelle ihr Zertifikat mitschickt. Ohne das
	 * koennte der Rechner die Signatur des Tokens spaeter nicht pruefen,
	 * und die Firmware prueft sie ohnehin nicht (siehe unten). */
	authz_der_w_bool(&w, true);

	if (with_nonce) {
		authz_der_w_uint_bytes(&w, req_out->nonce, OSK_TSA_NONCE_LEN);
	}

	/* reqPolicy wird weggelassen: dann waehlt die Stelle ihre
	 * Vorgabe-Richtlinie. Eine hier einzusetzen hiesse, eine Kennung zu
	 * fuehren, die wir nicht belegen koennen. */

	/* messageImprint ::= SEQUENCE { AlgorithmIdentifier, OCTET STRING } */
	size_t mark = w.used;
	authz_der_w_tlv(&w, DER_OCTET, hash, OSK_TSA_HASH_LEN);
	{
		/* AlgorithmIdentifier mit ausdruecklichem NULL als Parameter.
		 * RFC 4055 Abschnitt 2.1 zieht das Fehlen vor, laesst beides
		 * zu - und die Stellen am Markt erwarten ueberwiegend das
		 * NULL. Weggelassen waere strenger und wird oefter
		 * abgewiesen. */
		size_t am = w.used;
		authz_der_w_header(&w, 0x05u, 0u); /* NULL */
		authz_der_w_tlv(&w, DER_OID, OID_SHA256, sizeof(OID_SHA256));
		authz_der_w_header(&w, DER_SEQUENCE, w.used - am);
	}
	authz_der_w_header(&w, DER_SEQUENCE, w.used - mark);

	/* version ::= INTEGER { v1(1) } */
	authz_der_w_u64(&w, 1u);

	authz_der_w_header(&w, DER_SEQUENCE, w.used);

	const uint8_t *start = NULL;
	size_t out_len = 0u;
	r = authz_der_w_done(&w, &start, &out_len);
	if (r != OSK_OK) {
		osk_wipe(req_out, sizeof(*req_out));
		return r;
	}
	/* Rueckwaerts geschrieben heisst: das Ergebnis liegt am Ende des
	 * Puffers. Der Aufrufer erwartet es am Anfang, also wird es einmal
	 * nach vorn geschoben - memmove, weil sich die Bereiche
	 * ueberschneiden koennen. */
	memmove(der_out, start, out_len);
	*len_out = out_len;
	return OSK_OK;
}

/* ------------------------------------------------------- Antwort pruefen */

/* Eine GeneralizedTime sieht aus wie "20261029T..." - hier wird nur
 * geprueft, dass nichts darin steht, was eine Anzeige oder ein Protokoll
 * verbiegen kann. Ob das Datum stimmt, kann dieses Geraet nicht wissen:
 * es hat keine Uhr, gegen die es vergleichen koennte. Genau darum ist der
 * Zeitstempel ueberhaupt fremd eingeholt. */
static bool gentime_printable(const uint8_t *p, size_t n)
{
	if (n < 11u || n > OSK_TSA_GENTIME_MAX) {
		return false;
	}
	for (size_t i = 0u; i < n; i++) {
		const uint8_t c = p[i];
		const bool digit = (c >= (uint8_t)'0' && c <= (uint8_t)'9');
		if (!digit && c != (uint8_t)'Z' && c != (uint8_t)'.' && c != (uint8_t)'+' &&
		    c != (uint8_t)'-') {
			return false;
		}
	}
	return true;
}

/* Vom TimeStampToken (ContentInfo) bis zum TSTInfo durchgehen.
 * RFC 5652: ContentInfo { contentType, [0] content }, SignedData
 * { version, digestAlgorithms, encapContentInfo { eContentType,
 * [0] eContent }, ... }. */
static osk_result_t walk_to_tstinfo(const uint8_t *token, size_t token_len, authz_der_t *tst_out)
{
	authz_der_t outer = {token, token_len};
	authz_der_t ci;
	osk_result_t r = authz_der_enter(&outer, DER_SEQUENCE, &ci);
	if (r != OSK_OK) {
		return r;
	}

	const uint8_t *oid = NULL;
	size_t oid_len = 0u;
	r = authz_der_expect(&ci, DER_OID, &oid, &oid_len);
	if (r != OSK_OK) {
		return r;
	}
	if (oid_len != sizeof(OID_SIGNED_DATA) ||
	    memcmp(oid, OID_SIGNED_DATA, sizeof(OID_SIGNED_DATA)) != 0) {
		return OSK_ERR_FORMAT;
	}

	authz_der_t c0;
	r = authz_der_enter(&ci, DER_CTX0, &c0);
	if (r != OSK_OK) {
		return r;
	}
	authz_der_t sd;
	r = authz_der_enter(&c0, DER_SEQUENCE, &sd);
	if (r != OSK_OK) {
		return r;
	}

	/* version und digestAlgorithms ueberspringen - ihr Inhalt wird hier
	 * nicht gebraucht, weil die Signatur nicht geprueft wird. */
	r = authz_der_expect(&sd, DER_INTEGER, NULL, NULL);
	if (r != OSK_OK) {
		return r;
	}
	r = authz_der_expect(&sd, DER_SET, NULL, NULL);
	if (r != OSK_OK) {
		return r;
	}

	authz_der_t eci;
	r = authz_der_enter(&sd, DER_SEQUENCE, &eci);
	if (r != OSK_OK) {
		return r;
	}
	r = authz_der_expect(&eci, DER_OID, &oid, &oid_len);
	if (r != OSK_OK) {
		return r;
	}
	/* Der Inhaltstyp MUSS id-ct-TSTInfo sein. Ohne diese Pruefung wuerde
	 * jedes andere signierte CMS-Dokument als Zeitstempel durchgehen -
	 * eine Signatur ueber irgendetwas, als Zeitbeweis gelesen. */
	if (oid_len != sizeof(OID_TSTINFO) || memcmp(oid, OID_TSTINFO, sizeof(OID_TSTINFO)) != 0) {
		return OSK_ERR_FORMAT;
	}

	authz_der_t ec;
	r = authz_der_enter(&eci, DER_CTX0, &ec);
	if (r != OSK_OK) {
		return r;
	}
	const uint8_t *inner = NULL;
	size_t inner_len = 0u;
	r = authz_der_expect(&ec, DER_OCTET, &inner, &inner_len);
	if (r != OSK_OK) {
		return r;
	}

	authz_der_t wrap = {inner, inner_len};
	return authz_der_enter(&wrap, DER_SEQUENCE, tst_out);
}

osk_result_t osk_tsa_response_parse(const osk_tsa_request_t *req, const uint8_t *der, size_t len,
				    osk_tsa_statement_t *stmt_out, const uint8_t **token_out,
				    size_t *token_len_out)
{
	if (req == NULL || der == NULL || stmt_out == NULL || token_out == NULL ||
	    token_len_out == NULL) {
		return OSK_ERR_ARG;
	}
	memset(stmt_out, 0, sizeof(*stmt_out));
	*token_out = NULL;
	*token_len_out = 0u;

	authz_der_t top = {der, len};
	authz_der_t resp;
	osk_result_t r = authz_der_enter(&top, DER_SEQUENCE, &resp);
	if (r != OSK_OK) {
		return r;
	}

	/* PKIStatusInfo ::= SEQUENCE { status INTEGER, ... } */
	authz_der_t si;
	r = authz_der_enter(&resp, DER_SEQUENCE, &si);
	if (r != OSK_OK) {
		return r;
	}
	const uint8_t *val = NULL;
	size_t vlen = 0u;
	r = authz_der_expect(&si, DER_INTEGER, &val, &vlen);
	if (r != OSK_OK) {
		return r;
	}
	uint64_t status = 0u;
	r = authz_der_u64(val, vlen, &status);
	if (r != OSK_OK) {
		return r;
	}
	/* RFC 3161 Abschnitt 2.4.2: 0 granted, 1 grantedWithMods. Alles
	 * andere ist eine Absage, und dann ist kein Token dabei. */
	if (status != 0u && status != 1u) {
		return OSK_ERR_DENIED;
	}

	/* Das Token als Ganzes - mit Kopf, denn die Gegenzeichnung muss
	 * genau die Bytes umfassen, die der Rechner spaeter prueft. */
	const uint8_t *tok_start = resp.p;
	size_t before = resp.len;
	r = authz_der_expect(&resp, DER_SEQUENCE, NULL, NULL);
	if (r != OSK_OK) {
		/* Granted ohne Token ist ein Widerspruch in sich. */
		return OSK_ERR_FORMAT;
	}
	const size_t tok_len = before - resp.len;

	authz_der_t tst;
	r = walk_to_tstinfo(tok_start, tok_len, &tst);
	if (r != OSK_OK) {
		return r;
	}

	/* TSTInfo: version, policy, messageImprint, serialNumber, genTime */
	r = authz_der_expect(&tst, DER_INTEGER, NULL, NULL);
	if (r != OSK_OK) {
		return r;
	}
	r = authz_der_expect(&tst, DER_OID, NULL, NULL);
	if (r != OSK_OK) {
		return r;
	}

	authz_der_t mi;
	r = authz_der_enter(&tst, DER_SEQUENCE, &mi);
	if (r != OSK_OK) {
		return r;
	}
	r = authz_der_expect(&mi, DER_SEQUENCE, NULL, NULL); /* AlgorithmIdentifier */
	if (r != OSK_OK) {
		return r;
	}
	r = authz_der_expect(&mi, DER_OCTET, &val, &vlen);
	if (r != OSK_OK) {
		return r;
	}
	/* Der Hash im Token MUSS unserer sein. Ohne diese Pruefung liesse
	 * sich ein echter Zeitstempel ueber fremde Daten als Beweis fuer
	 * unsere ausgeben - der eigentliche Angriff auf einen Zeitstempel,
	 * und er kommt ohne jede Faelschung aus. */
	if (vlen != OSK_TSA_HASH_LEN ||
	    authz_equal_ct(val, req->hash, OSK_TSA_HASH_LEN) != 0) {
		return OSK_ERR_DENIED;
	}

	r = authz_der_expect(&tst, DER_INTEGER, NULL, NULL); /* serialNumber */
	if (r != OSK_OK) {
		return r;
	}
	r = authz_der_expect(&tst, DER_GENTIME, &val, &vlen);
	if (r != OSK_OK) {
		return r;
	}
	if (!gentime_printable(val, vlen)) {
		return OSK_ERR_FORMAT;
	}
	memcpy(stmt_out->gen_time, val, vlen);
	stmt_out->gen_time[vlen] = '\0';
	stmt_out->gen_time_present = true;

	/* accuracy, ordering und nonce sind wahlfrei und stehen in dieser
	 * Reihenfolge. Gesucht wird die Nonce; was davor liegt, wird
	 * uebergangen, ohne dass das Fehlen eines Feldes ein Fehler ist. */
	if (req->nonce_sent) {
		(void)authz_der_expect(&tst, DER_SEQUENCE, NULL, NULL); /* accuracy */
		(void)authz_der_expect(&tst, DER_BOOLEAN, NULL, NULL);	/* ordering */

		r = authz_der_expect(&tst, DER_INTEGER, &val, &vlen);
		if (r != OSK_OK) {
			/* Wir haben eine Nonce geschickt, es kommt keine
			 * zurueck: dann ist nicht zu entscheiden, ob diese
			 * Antwort zu dieser Anfrage gehoert. Das ist eine
			 * Absage und keine Kleinigkeit. */
			return OSK_ERR_DENIED;
		}
		/* Die Stelle darf fuehrende Nullen weglassen - der Vergleich
		 * geht darum auf den Zahlenwert, nicht auf die Bytes. */
		size_t a = 0u, b = 0u;
		while (a + 1u < vlen && val[a] == 0x00u) {
			a++;
		}
		while (b + 1u < OSK_TSA_NONCE_LEN && req->nonce[b] == 0x00u) {
			b++;
		}
		if ((vlen - a) != (OSK_TSA_NONCE_LEN - b) ||
		    authz_equal_ct(val + a, req->nonce + b, vlen - a) != 0) {
			return OSK_ERR_DENIED;
		}
	}

	stmt_out->device_ms = authz_device_ms();
	*token_out = tok_start;
	*token_len_out = tok_len;
	return OSK_OK;
}

/* ------------------------------------------------------- Gegenzeichnen */

osk_result_t osk_tsa_countersign(osk_authz_session_t session, osk_key_id_t signing_key,
				 osk_key_id_t counter_id, const uint8_t hash[OSK_TSA_HASH_LEN],
				 const uint8_t *token, size_t token_len,
				 osk_tsa_statement_t *stmt_out, uint8_t *sig_out, size_t cap,
				 size_t *sig_len_out)
{
	if (hash == NULL || token == NULL || stmt_out == NULL || sig_out == NULL ||
	    sig_len_out == NULL) {
		return OSK_ERR_ARG;
	}
	if (token_len == 0u) {
		return OSK_ERR_ARG;
	}

	/* Beides wird gebraucht: unterschreiben darf nicht jeder, und
	 * Zeitstempel anfordern ist ein eigenes Recht. Ein Dienst, der nur
	 * signieren darf, soll keine Zeitstempel-Erklaerungen in die Welt
	 * setzen koennen. */
	osk_result_t r = osk_authz_check(session, OSK_CAP_TIMESTAMP | OSK_CAP_SIGN, OSK_REALM_ALL);
	if (r != OSK_OK) {
		return r;
	}

	uint16_t who = 0u;
	r = osk_authz_session_identity(session, &who, NULL);
	if (r != OSK_OK) {
		return r;
	}

	/* Den Zaehler ERST hochsetzen, dann unterschreiben. Umgekehrt
	 * entstuende bei einem Stromausfall zwischen beiden Schritten eine
	 * unterschriebene Erklaerung mit einem Zaehlerstand, der noch
	 * einmal vergeben wird - zwei Erklaerungen mit derselben Nummer,
	 * und die Reihenfolge waere nicht mehr eindeutig. So entsteht
	 * umgekehrt hoechstens eine Luecke, und eine Luecke ist ehrlich. */
	uint64_t seq = 0u;
	r = osk_se050_counter_read(counter_id, &seq);
	if (r != OSK_OK) {
		return r;
	}
	seq += 1u;
	r = osk_se050_counter_raise(counter_id, seq);
	if (r != OSK_OK) {
		return r;
	}

	uint8_t token_hash[OSK_SHA256_LEN];
	osk_sha256(token, token_len, token_hash);

	/* Die Erklaerung, feste Laenge und feste Reihenfolge:
	 *
	 *   Trennzeichenkette | Datenhash | Tokenhash | Zaehler | wer
	 *
	 * Feste Laengen, damit kein Feld in das naechste hineinwachsen kann -
	 * bei veraenderlichen Laengen ohne Trenner lassen sich zwei
	 * verschiedene Erklaerungen auf dieselben Bytes bringen. */
	uint8_t stmt[sizeof(AUTHZ_CTX_TSA) - 1u + OSK_TSA_HASH_LEN + OSK_SHA256_LEN + 8u + 2u];
	size_t n = 0u;
	const size_t ctx_len = sizeof(AUTHZ_CTX_TSA) - 1u;

	memcpy(stmt, AUTHZ_CTX_TSA, ctx_len);
	n += ctx_len;
	memcpy(stmt + n, hash, OSK_TSA_HASH_LEN);
	n += OSK_TSA_HASH_LEN;
	memcpy(stmt + n, token_hash, OSK_SHA256_LEN);
	n += OSK_SHA256_LEN;
	for (size_t i = 0u; i < 8u; i++) {
		stmt[n + i] = (uint8_t)(seq >> (8u * (7u - i)));
	}
	n += 8u;
	stmt[n++] = (uint8_t)(who >> 8);
	stmt[n++] = (uint8_t)who;

	/* Die GERAETEZEIT steht ABSICHTLICH NICHT in der unterschriebenen
	 * Erklaerung. Sie beginnt bei jedem Einstecken bei null und waere
	 * damit keine Angabe, die etwas bezeichnet - unterschrieben wuerde
	 * sie aber wie eine aussehen. Sie wird nur gemeldet, damit ein
	 * Protokoll sie neben die Erklaerung legen kann. */
	uint8_t digest[OSK_SHA256_LEN];
	osk_sha256(stmt, n, digest);
	osk_wipe(stmt, sizeof(stmt));

	r = osk_se050_sign(signing_key, digest, sizeof(digest), sig_out, cap, sig_len_out);
	osk_wipe(digest, sizeof(digest));
	if (r != OSK_OK) {
		return r;
	}

	stmt_out->counter = seq;
	stmt_out->key_id = who;
	stmt_out->device_ms = authz_device_ms();
	return OSK_OK;
}
