/* Zertifikatsanfrage nach PKCS#10. Siehe osk_csr.h.
 *
 * Gebaut nach RFC 2986 (Anfrage), RFC 2985 Abschnitt 5.4.2 (Erweiterungen
 * als Attribut), RFC 5280 Abschnitt 4.2 (die Erweiterungen selbst) und
 * RFC 7468 Abschnitt 4 (PEM). Nach der Beschreibung, nicht nach einer
 * vorhandenen Umsetzung. */
#include <string.h>
#include "osk_csr.h"
#include "osk_der.h"
#include "osk_spki.h"
#include "osk_card.h"
#include "osk_rsa_card.h"
/* SHA-256 kommt aus src/classes/ctap/osk_crypto.h und wird nicht noch
 * einmal geschrieben. */
#include "osk_crypto.h"

/* Der unterschriebene Teil liegt fest und nicht auf dem Stapel: bis zu
 * 2 kB auf einem Faden, der unter Zephyr oft 1-2 kB Stapel hat, waere am
 * Geraet ein Absturz. Am PC faellt so etwas nicht auf. Es wird immer nur
 * eine Anfrage bearbeitet, darum reicht einer. */
static uint8_t g_info[OSK_CSR_MAX];
/* Laengste Unterschrift: RSA-4096, so lang wie der Modulus. */
static uint8_t g_sig[OSK_RSA_MOD_MAX];

void osk_csr_reset(void)
{
	osk_card_wipe(g_info, sizeof(g_info));
	osk_card_wipe(g_sig, sizeof(g_sig));
}

/* --- Betreff ------------------------------------------------------------ */

/* Von allgemein nach besonders: Land, Organisation, Abteilung, Name. So
 * liest es jeder Aussteller, und X.501 meint mit einem Namen genau diesen
 * Weg von der Wurzel nach unten. Die Geraetenummer kommt hinter den Namen,
 * weil sie ihn naeher bestimmt und nicht umgekehrt. */
static void write_subject(osk_resp_t *r, const osk_csr_req_t *req)
{
	osk_tlv_mark_t m;

	osk_resp_tlv_begin(r, OSK_DER_SEQUENCE, &m);
	if (req->country != NULL)
		osk_der_rdn(r, &osk_oid_country, req->country);
	if (req->org != NULL)
		osk_der_rdn(r, &osk_oid_org, req->org);
	if (req->org_unit != NULL)
		osk_der_rdn(r, &osk_oid_ou, req->org_unit);
	osk_der_rdn(r, &osk_oid_cn, req->cn);
	if (req->serial_number != NULL)
		osk_der_rdn(r, &osk_oid_serial_number, req->serial_number);
	osk_resp_tlv_end(r, &m);
}

/* --- Erweiterungen ----------------------------------------------------- */

/* Das Tag eines Namens in GeneralName. Die Zahlen sind die Nummern aus
 * RFC 5280 Abschnitt 4.2.1.6, in ein kontextbezogenes Tag gesetzt. Alle
 * drei tragen eine IA5String, und darum sind sie PRIMITIV (0x80) und nicht
 * zusammengesetzt (0xa0) - wer hier 0xa0 schreibt, baut einen Namen, den
 * kein Pruefer liest. */
static uint8_t san_tag(osk_csr_san_kind_t kind)
{
	switch (kind) {
	case OSK_CSR_SAN_EMAIL:
		return 0x81u; /* rfc822Name [1] */
	case OSK_CSR_SAN_URI:
		return 0x86u; /* uniformResourceIdentifier [6] */
	default:
		return 0x82u; /* dNSName [2] */
	}
}

static void write_san(osk_resp_t *r, const osk_csr_req_t *req)
{
	osk_tlv_mark_t outer;
	osk_tlv_mark_t inner;
	osk_tlv_mark_t names;

	/* Nicht kritisch: der Betreff ist nicht leer (cn ist Pflicht), und
	 * dann verlangt RFC 5280 Abschnitt 4.2.1.6 die Markierung
	 * ausdruecklich NICHT. Bei leerem Betreff waere sie Pflicht - dieser
	 * Fall wird hier nicht gebaut. */
	osk_der_ext_begin(r, &osk_oid_san, false, &outer, &inner);
	osk_resp_tlv_begin(r, OSK_DER_SEQUENCE, &names);
	for (uint8_t i = 0; i < req->san_count; i++) {
		const char *name = req->san[i].name;

		if (name == NULL)
			continue;
		osk_resp_tlv(r, san_tag(req->san[i].kind), (const uint8_t *)name, strlen(name));
	}
	osk_resp_tlv_end(r, &names);
	osk_der_ext_end(r, &outer, &inner);
}

static const osk_oid_t *eku_oid(osk_csr_eku_t e)
{
	switch (e) {
	case OSK_CSR_EKU_CLIENT_AUTH:
		return &osk_oid_eku_client;
	case OSK_CSR_EKU_CODE_SIGNING:
		return &osk_oid_eku_code;
	case OSK_CSR_EKU_EMAIL_PROTECTION:
		return &osk_oid_eku_email;
	default:
		return &osk_oid_eku_server;
	}
}

static void write_eku(osk_resp_t *r, const osk_csr_req_t *req)
{
	osk_tlv_mark_t outer;
	osk_tlv_mark_t inner;
	osk_tlv_mark_t seq;

	osk_der_ext_begin(r, &osk_oid_ext_key_usage, false, &outer, &inner);
	osk_resp_tlv_begin(r, OSK_DER_SEQUENCE, &seq);
	for (uint8_t i = 0; i < req->eku_count; i++)
		osk_der_oid(r, eku_oid(req->eku[i]));
	osk_resp_tlv_end(r, &seq);
	osk_der_ext_end(r, &outer, &inner);
}

static void write_key_usage(osk_resp_t *r, const osk_csr_req_t *req)
{
	osk_tlv_mark_t outer;
	osk_tlv_mark_t inner;

	/* critical = TRUE. RFC 5280 Abschnitt 4.2.1.3 verlangt es fuer
	 * keyUsage; ohne die Markierung darf ein Pruefer die Angabe
	 * uebergehen, und dann sagt sie nichts. */
	osk_der_ext_begin(r, &osk_oid_key_usage, true, &outer, &inner);
	osk_der_key_usage_bits(r, req->key_usage);
	osk_der_ext_end(r, &outer, &inner);
}

/* Die Erweiterungen reisen als Attribut mit, nicht als eigenes Feld:
 *
 *   attributes [0] SET OF Attribute
 *   Attribute ::= SEQUENCE { type OID, values SET OF ANY }
 *
 * und fuer Erweiterungen ist `type` extensionRequest und der eine Wert
 * eine SEQUENCE OF Extension (RFC 2985 Abschnitt 5.4.2). Das sind vier
 * Ebenen uebereinander, und drei davon sind Mengen - wer eine davon
 * weglaesst, bekommt eine Anfrage, die gelesen wird und deren
 * Erweiterungen niemand findet.
 *
 * Das Feld wird IMMER geschrieben, auch leer: es ist in RFC 2986 nicht
 * wahlfrei. Eine Anfrage ohne [0] ist keine Anfrage. */
static void write_attributes(osk_resp_t *r, const osk_csr_req_t *req)
{
	osk_tlv_mark_t set;
	osk_tlv_mark_t attr;
	osk_tlv_mark_t values;
	osk_tlv_mark_t exts;
	const bool any = (req->san_count > 0u) || (req->key_usage != 0u) || (req->eku_count > 0u);

	osk_resp_tlv_begin(r, OSK_DER_CTX0, &set);
	if (any) {
		osk_resp_tlv_begin(r, OSK_DER_SEQUENCE, &attr);
		osk_der_oid(r, &osk_oid_ext_request);
		osk_resp_tlv_begin(r, OSK_DER_SET, &values);
		osk_resp_tlv_begin(r, OSK_DER_SEQUENCE, &exts);
		if (req->key_usage != 0u)
			write_key_usage(r, req);
		if (req->eku_count > 0u)
			write_eku(r, req);
		if (req->san_count > 0u)
			write_san(r, req);
		osk_resp_tlv_end(r, &exts);
		osk_resp_tlv_end(r, &values);
		osk_resp_tlv_end(r, &attr);
	}
	osk_resp_tlv_end(r, &set);
}

/* --- Der unterschriebene Teil ------------------------------------------ */

static osk_result_t check_req(const osk_csr_req_t *req)
{
	if (req == NULL || req->cn == NULL || req->cn[0] == 0)
		return OSK_ERR_ARG;
	if (req->san_count > OSK_CSR_SAN_MAX || req->eku_count > OSK_CSR_EKU_MAX)
		return OSK_ERR_ARG;
	if (req->key_type != OSK_SPKI_P256 && req->key_type != OSK_SPKI_P384 &&
	    req->key_type != OSK_SPKI_RSA)
		return OSK_ERR_ARG;
	return OSK_OK;
}

osk_result_t osk_csr_info(const osk_csr_req_t *req, uint8_t *out, size_t cap, size_t *len_out)
{
	osk_resp_t r;
	osk_tlv_mark_t m;
	osk_result_t rc;
	static const uint8_t zero[] = {0x00u};

	rc = check_req(req);
	if (rc != OSK_OK)
		return rc;
	if (out == NULL || len_out == NULL)
		return OSK_ERR_ARG;

	osk_resp_init(&r, out, cap);
	osk_resp_tlv_begin(&r, OSK_DER_SEQUENCE, &m);
	/* version: 0 heisst "diese Fassung" (RFC 2986 Abschnitt 4.1). Nicht
	 * 1 - die Zahl zaehlt hier ab 0 und nicht wie bei X.509, wo v3 die
	 * 2 ist. */
	osk_der_uint(&r, zero, sizeof(zero));
	write_subject(&r, req);
	rc = osk_spki_write(&r, req->key, req->key_type);
	if (rc != OSK_OK)
		return rc;
	write_attributes(&r, req);
	osk_resp_tlv_end(&r, &m);

	if (r.overflow)
		return OSK_ERR_FULL;
	*len_out = r.len;
	return OSK_OK;
}

/* --- Unterschreiben ---------------------------------------------------- */

static osk_result_t sign_info(const osk_csr_req_t *req, const uint8_t *info, size_t info_len,
			      size_t *sig_len_out)
{
	uint8_t digest[OSK_SHA256_LEN];

	/* Der Hash wird HIER gebildet und nicht im Chip - so verlangt es der
	 * Vertrag (osk_se050.h), und so kann die Anzeige spaeter genau das
	 * zeigen, was unterschrieben wird. */
	osk_sha256(info, info_len, digest);

	if (req->key_type == OSK_SPKI_RSA) {
		/* Die Auffuellung wird GENANNT, sie ist nie die Vorgabe - so
		 * steht es im Vertrag. PKCS#1 v1.5 passt zur Kennung
		 * sha256WithRSAEncryption, die unten geschrieben wird. PSS
		 * waere eine andere Kennung mit eigenen Parametern und ist
		 * hier nicht gebaut (docs/csr.md, offene Punkte). */
		if (osk_se050_rsa_sign(req->key, OSK_RSA_PAD_PKCS1_V15, OSK_HASH_SHA256, digest,
				       sizeof(digest), g_sig, sizeof(g_sig), sig_len_out) != OSK_OK)
			return OSK_ERR_IO;
		return OSK_OK;
	}
	if (osk_se050_sign(req->key, digest, sizeof(digest), g_sig, sizeof(g_sig), sig_len_out) !=
	    OSK_OK)
		return OSK_ERR_IO;
	return OSK_OK;
}

osk_result_t osk_csr_der(const osk_csr_req_t *req, uint8_t *out, size_t cap, size_t *len_out)
{
	osk_resp_t r;
	osk_tlv_mark_t m;
	osk_result_t rc;
	size_t info_len = 0;
	size_t sig_len = 0;
	const osk_oid_t *alg;

	rc = osk_csr_info(req, g_info, sizeof(g_info), &info_len);
	if (rc != OSK_OK)
		return rc;
	if (out == NULL || len_out == NULL)
		return OSK_ERR_ARG;

	rc = sign_info(req, g_info, info_len, &sig_len);
	if (rc != OSK_OK)
		return rc;

	alg = (req->key_type == OSK_SPKI_RSA) ? &osk_oid_rsa_sha256 : &osk_oid_ecdsa_sha256;

	osk_resp_init(&r, out, cap);
	osk_resp_tlv_begin(&r, OSK_DER_SEQUENCE, &m);
	osk_resp_bytes(&r, g_info, info_len);
	/* Bei ECDSA hat der AlgorithmIdentifier KEINEN Parameter, bei RSA
	 * einen ausdruecklichen NULL-Wert. Das ist nicht dasselbe, und beide
	 * Normen sagen es verschieden: RFC 5758 Abschnitt 3.2 gegen RFC 8017
	 * Anhang A.2.3. */
	osk_der_alg(&r, alg, NULL, req->key_type == OSK_SPKI_RSA);
	/* Die Unterschrift kommt so hinein, wie der Chip sie liefert: bei
	 * ECDSA ist das die SEQUENCE aus r und s (die Kartenseite rechnet sie
	 * anderswo in die flache Form um, osk_card_sig_der_to_raw()), bei
	 * RSA die Zahl, so lang wie der Modulus. In beiden Faellen ist das
	 * genau das, was die Norm im BIT STRING erwartet. */
	osk_der_bits(&r, g_sig, sig_len);
	osk_resp_tlv_end(&r, &m);

	if (r.overflow)
		return OSK_ERR_FULL;
	*len_out = r.len;
	return OSK_OK;
}

/* --- PEM --------------------------------------------------------------- */

static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* Base64 nach RFC 4648 Abschnitt 4, mit Auffuellen durch '='. Drei Bytes
 * werden vier Zeichen; was am Ende fehlt, wird als 0 gerechnet und das
 * Ergebnis entsprechend abgeschnitten. */
static size_t b64_block(const uint8_t *in, size_t have, char *out)
{
	const uint32_t v = ((uint32_t)in[0] << 16) | ((have > 1u ? (uint32_t)in[1] : 0u) << 8) |
			   (have > 2u ? (uint32_t)in[2] : 0u);

	out[0] = b64[(v >> 18) & 0x3fu];
	out[1] = b64[(v >> 12) & 0x3fu];
	out[2] = (have > 1u) ? b64[(v >> 6) & 0x3fu] : '=';
	out[3] = (have > 2u) ? b64[v & 0x3fu] : '=';
	return 4u;
}

osk_result_t osk_csr_pem(const uint8_t *der, size_t der_len, char *out, size_t cap, size_t *len_out)
{
	static const char head[] = "-----BEGIN CERTIFICATE REQUEST-----\n";
	static const char foot[] = "-----END CERTIFICATE REQUEST-----\n";
	size_t at = 0;
	unsigned col = 0;

	if (der == NULL || out == NULL || len_out == NULL || der_len == 0u)
		return OSK_ERR_ARG;

	/* Alles einmal nachrechnen, bevor etwas geschrieben wird: eine halb
	 * geschriebene Anfrage sieht aus wie eine ganze. */
	{
		const size_t groups = (der_len + 2u) / 3u;
		const size_t chars = groups * 4u;
		const size_t need =
		    (sizeof(head) - 1u) + chars + (chars + 63u) / 64u + (sizeof(foot) - 1u) + 1u;

		if (cap < need)
			return OSK_ERR_FULL;
	}

	memcpy(out + at, head, sizeof(head) - 1u);
	at += sizeof(head) - 1u;

	for (size_t i = 0; i < der_len; i += 3u) {
		const size_t have = der_len - i;

		at += b64_block(der + i, have, out + at);
		col += 4u;
		if (col == 64u) {
			out[at++] = '\n';
			col = 0;
		}
	}
	if (col != 0u)
		out[at++] = '\n';

	memcpy(out + at, foot, sizeof(foot) - 1u);
	at += sizeof(foot) - 1u;
	out[at] = 0;
	*len_out = at;
	return OSK_OK;
}
