/* PIV-Attestierung. Siehe osk_attest.h.
 * X.509 nach RFC 5280; Objektkennungen nach RFC 5480 (elliptische Kurven)
 * und RFC 8017 Anhang A.1 (rsaEncryption).
 *
 * Das DER-Schreiben selbst liegt seit dem 30.10.2026 in osk_der.h und das
 * SubjectPublicKeyInfo in osk_spki.h: die Zertifikatsanfrage (osk_csr.h)
 * braucht genau dieselben Bytes, und zweimal hingeschrieben waere eine der
 * beiden Fassungen irgendwann anders. */
#include <string.h>
#include <stdio.h>
#include "osk_attest.h"
#include "osk_der.h"
#include "osk_spki.h"
#include "osk_piv.h"
#include "osk_card.h"
#include "osk_apdu.h"
#include "osk_se050.h"
#include "osk_serial.h"
#include "osk_rsa_card.h"
/* SHA-256 kommt aus src/classes/ctap/osk_crypto.h und wird nicht noch
 * einmal geschrieben - genauso, wie die Geldboerse es haelt
 * (src/classes/wallet/wallet_hash.h, Kopf). */
#include "osk_crypto.h"

/* Die zwei Zwischenpuffer liegen fest und nicht auf dem Stapel. Grund:
 * beim Zusammenfuehren von Strang A ist genau das aufgefallen - knapp
 * 1,8 kB Puffer auf dem Stapel eines Fadens, der unter Zephyr oft 1-2 kB
 * gross ist (docs/plan.md, offener Punkt 4a). Am PC faellt so etwas nicht
 * auf; am Geraet ist es ein Absturz oder eine Stapelwaechter-Meldung. Es
 * wird immer nur ein Kommando bearbeitet, darum reicht je einer. */
static uint8_t g_tbs[OSK_ATTEST_MAX];
/* Laengster oeffentlicher Teil: RSA-4096 sind 512 Byte Modulus, dazu
 * Exponent, Verpackung und die Algorithmuskennung. */
static uint8_t g_spki[600];

void osk_attest_reset(void)
{
	osk_card_wipe(g_tbs, sizeof(g_tbs));
	osk_card_wipe(g_spki, sizeof(g_spki));
}

/* --- Steckplatz-Angaben ------------------------------------------------- */

static bool slot_ok(uint8_t slot)
{
	return slot == OSK_PIV_SLOT_AUTH || slot == OSK_PIV_SLOT_SIGN ||
	       slot == OSK_PIV_SLOT_KEYMGMT || slot == OSK_PIV_SLOT_CARD;
}

/* Die Freigaberegel des Steckplatzes, als Text ins Zertifikat. Sie ist
 * eine Aussage ueber das Geraet und gehoert deshalb hinein: ein Schluessel
 * in 9E, der ohne PIN benutzbar ist, ist etwas anderes als einer in 9C,
 * der die PIN unmittelbar davor verlangt. Die Werte stehen hier und in
 * do_sign() in osk_piv.c - geaendert werden muessen sie an beiden Stellen,
 * und darum prueft test_attest.c sie gegen den Ablauf, nicht nur gegen
 * sich selbst. */
static const char *slot_policy(uint8_t slot)
{
	switch (slot) {
	case OSK_PIV_SLOT_AUTH:
		return "pin=session touch=yes";
	case OSK_PIV_SLOT_SIGN:
		return "pin=always touch=yes";
	case OSK_PIV_SLOT_KEYMGMT:
		return "pin=session touch=yes";
	case OSK_PIV_SLOT_CARD:
		return "pin=no touch=yes";
	default:
		return "unknown";
	}
}

/* Verwendungszweck des Schluessels. 9D wickelt oder vereinbart einen
 * Schluessel, die anderen drei unterschreiben - und bei 9D haengt es auch
 * noch am Verfahren: RSA wickelt ein (keyEncipherment), eine Kurve
 * vereinbart (keyAgreement). Zwei Dinge, die beide "entschluesseln"
 * heissen und Verschiedenes tun. */
static void der_key_usage(osk_resp_t *r, uint8_t slot, bool is_rsa)
{
	osk_tlv_mark_t ext;
	osk_tlv_mark_t oct;
	uint16_t mask;

	/* Die Bitnummern sind die der Norm, nicht die eines Rechnerwortes;
	 * osk_der_key_usage_bits() rechnet sie um und zaehlt die ungenutzten
	 * Bits am Ende. */
	if (slot == OSK_PIV_SLOT_KEYMGMT)
		mask = is_rsa ? OSK_KU_KEY_ENCIPHERMENT : OSK_KU_KEY_AGREEMENT;
	else
		mask = OSK_KU_DIGITAL_SIGNATURE;

	/* critical = TRUE. RFC 5280 Abschnitt 4.2.1.3 verlangt es fuer
	 * keyUsage; ohne die Markierung darf ein Pruefer die Angabe
	 * uebergehen, und dann sagt sie nichts. */
	osk_der_ext_begin(r, &osk_oid_key_usage, true, &ext, &oct);
	osk_der_key_usage_bits(r, mask);
	osk_der_ext_end(r, &ext, &oct);
}

static void der_basic_constraints(osk_resp_t *r)
{
	osk_tlv_mark_t ext;
	osk_tlv_mark_t oct;
	osk_tlv_mark_t seq;

	osk_der_ext_begin(r, &osk_oid_basic_constraints, true, &ext, &oct);
	/* Leere SEQUENCE: cA fehlt, heisst FALSE (RFC 5280 4.2.1.9). Der
	 * attestierte Schluessel ist keine Zertifizierungsstelle. */
	osk_resp_tlv_begin(r, OSK_DER_SEQUENCE, &seq);
	osk_resp_tlv_end(r, &seq);
	osk_der_ext_end(r, &ext, &oct);
}

/* --- Aussteller -------------------------------------------------------- */

/* Holt den Inhabernamen aus dem abgelegten F9-Zertifikat. Er wird Byte
 * fuer Byte uebernommen, nicht nachgebaut: der Aussteller eines
 * Zertifikats muss BYTEGLEICH der Inhaber des naechsten sein, sonst findet
 * kein Pruefer die Kette. Ein nachgebauter Name mit derselben Zeichenkette
 * in einem anderen Zeichenkettentyp ist ein anderer Name.
 *
 * Aufbau nach RFC 5280 Abschnitt 4.1:
 *   Certificate ::= SEQUENCE { tbsCertificate SEQUENCE { ... } ... }
 * und in tbsCertificate ist der Inhaber das sechste Feld, wenn das
 * Feld [0] version da ist, sonst das fuenfte. Gezaehlt wird ab 0. */
static bool issuer_from_cert(const uint8_t *cert, size_t cert_len, const uint8_t **name_out,
			     size_t *name_len_out)
{
	const uint8_t *body = NULL;
	const uint8_t *tbs = NULL;
	size_t body_len = 0;
	size_t tbs_len = 0;
	size_t used = 0;
	uint32_t tag = 0;
	unsigned want;
	unsigned seen = 0;
	const uint8_t *p;
	size_t left;

	if (cert == NULL || cert_len == 0u)
		return false;
	if (!osk_tlv_first(cert, cert_len, &tag, &body, &body_len, &used) ||
	    tag != OSK_DER_SEQUENCE)
		return false;
	if (!osk_tlv_first(body, body_len, &tag, &tbs, &tbs_len, &used) || tag != OSK_DER_SEQUENCE)
		return false;

	p = tbs;
	left = tbs_len;
	if (left == 0u)
		return false;
	want = (p[0] == OSK_DER_CTX0) ? 5u : 4u;

	while (left > 0u) {
		const uint8_t *val = NULL;
		size_t val_len = 0;

		if (!osk_tlv_first(p, left, &tag, &val, &val_len, &used))
			return false;
		if (used == 0u || used > left)
			return false;
		if (seen == want) {
			if (tag != OSK_DER_SEQUENCE)
				return false;
			*name_out = p;
			*name_len_out = used;
			return true;
		}
		seen++;
		p += used;
		left -= used;
	}
	return false;
}

/* --- Das Zertifikat ---------------------------------------------------- */

static void to_hex(const uint8_t *in, size_t len, char *out, size_t cap)
{
	size_t at = 0;

	if (cap == 0u)
		return;
	out[0] = 0;
	for (size_t i = 0; i < len; i++) {
		if (at + 3u > cap)
			break;
		at += (size_t)snprintf(out + at, cap - at, "%02X", in[i]);
	}
}

osk_result_t osk_attest_cert(uint8_t slot, uint8_t alg, uint8_t *out, size_t cap, size_t *len_out)
{
	osk_resp_t rs;
	osk_resp_t rt;
	osk_resp_t ro;
	osk_tlv_mark_t tbs;
	osk_tlv_mark_t mark;
	osk_result_t rc;
	const bool is_rsa = (alg != OSK_PIV_ALG_P256 && alg != OSK_PIV_ALG_P384);
	uint8_t device[OSK_SERIAL_LEN];
	char serial_hex[OSK_SERIAL_LEN * 2u + 1u];
	char cn[48];
	uint8_t cert_serial[8];
	uint8_t digest[OSK_SHA256_LEN];
	uint8_t sig[160];
	size_t sig_len = 0;
	size_t spki_len;
	size_t tbs_len;
	const uint8_t *issuer = NULL;
	size_t issuer_len = 0;
	static uint8_t f9_cert[OSK_ATTEST_MAX];
	size_t f9_len = 0;

	if (out == NULL || len_out == NULL)
		return OSK_ERR_ARG;
	if (!slot_ok(slot))
		return OSK_ERR_ARG;
	if (is_rsa && !osk_rsa_available())
		return OSK_ERR_UNSUPPORTED;

	/* Der F9-Schluessel muss da sein, und er muss eine P-256 sein: die
	 * Unterschrift laeuft ueber SHA-256, und das ist der einzige Hash,
	 * den diese Schicht hat (osk_crypto.h). Eine P-384 mit SHA-256 zu
	 * unterschreiben waere erlaubt, aber dann waere die
	 * Algorithmuskennung im Zertifikat eine andere als die, die hier
	 * steht - also wird es abgelehnt statt stillschweigend gemischt. */
	{
		uint8_t raw[160];
		size_t raw_len = 0;
		const uint8_t *point = NULL;

		if (osk_se050_key_public(OSK_PIV_KEY_ID(OSK_PIV_SLOT_ATTEST), raw, sizeof(raw),
					 &raw_len) != OSK_OK)
			return OSK_ERR_NOT_FOUND;
		if (osk_card_public_point(raw, raw_len, 65u, &point) != OSK_OK)
			return OSK_ERR_UNSUPPORTED;
	}

	rc = osk_serial_device(device);
	if (rc != OSK_OK)
		return rc;
	to_hex(device, sizeof(device), serial_hex, sizeof(serial_hex));

	/* 1. Der oeffentliche Teil des Steckplatzschluessels, in einen
	 *    eigenen Puffer. Er muss vor der Seriennummer des Zertifikats
	 *    feststehen, weil diese aus ihm entsteht. */
	osk_resp_init(&rs, g_spki, sizeof(g_spki));
	rc = osk_spki_write(&rs, OSK_PIV_KEY_ID(slot),
			    is_rsa ? OSK_SPKI_RSA
				   : ((alg == OSK_PIV_ALG_P384) ? OSK_SPKI_P384 : OSK_SPKI_P256));
	if (rc != OSK_OK)
		return rc;
	if (rs.overflow)
		return OSK_ERR_FULL;
	spki_len = rs.len;

	/* 2. Seriennummer des Zertifikats. Sie muss bei einem Aussteller
	 *    eindeutig sein (RFC 5280 4.1.2.2). Aus Geraetenummer und
	 *    Steckplatz allein waere sie das NICHT: wird der Schluessel im
	 *    selben Steckplatz neu erzeugt, entsteht ein zweites Zertifikat
	 *    mit derselben Nummer und einem anderen Schluessel. Darum haengt
	 *    der oeffentliche Schluessel mit drin. */
	{
		osk_sha256_t h;
		uint8_t full[OSK_SHA256_LEN];
		const uint8_t slot_b[1] = {slot};

		osk_sha256_init(&h);
		osk_sha256_update(&h, device, sizeof(device));
		osk_sha256_update(&h, slot_b, sizeof(slot_b));
		osk_sha256_update(&h, g_spki, spki_len);
		osk_sha256_final(&h, full);
		memcpy(cert_serial, full, sizeof(cert_serial));
		/* Positiv halten: das oberste Bit waere sonst das Vorzeichen,
		 * und eine negative Seriennummer lehnen Pruefer ab. */
		cert_serial[0] = (uint8_t)(cert_serial[0] & 0x7fu);
		if (cert_serial[0] == 0u)
			cert_serial[0] = 1u;
	}

	/* 3. Der Aussteller. Aus dem F9-Zertifikat, wenn eines abgelegt ist. */
	if (osk_card_obj_read(OSK_ITEM_CERT, OSK_ATTEST_CERT_TAG, OSK_ATTEST_CERT_NAME, f9_cert,
			      sizeof(f9_cert), &f9_len) == OSK_OK) {
		if (!issuer_from_cert(f9_cert, f9_len, &issuer, &issuer_len)) {
			issuer = NULL;
			issuer_len = 0u;
		}
	}

	/* 4. Der unterschriebene Teil. */
	osk_resp_init(&rt, g_tbs, sizeof(g_tbs));
	osk_resp_tlv_begin(&rt, OSK_DER_SEQUENCE, &tbs);

	osk_resp_tlv_begin(&rt, OSK_DER_CTX0, &mark);
	{
		static const uint8_t two[] = {0x02u}; /* v3 ist die 2 */

		osk_der_uint(&rt, two, sizeof(two));
	}
	osk_resp_tlv_end(&rt, &mark);

	osk_der_uint(&rt, cert_serial, sizeof(cert_serial));
	osk_der_alg(&rt, &osk_oid_ecdsa_sha256, NULL, false);

	if (issuer != NULL) {
		osk_resp_bytes(&rt, issuer, issuer_len);
	} else {
		char fallback[80];

		snprintf(fallback, sizeof(fallback), "openSecureKey Attestation unchained %s",
			 serial_hex);
		osk_resp_tlv_begin(&rt, OSK_DER_SEQUENCE, &mark);
		osk_der_rdn(&rt, &osk_oid_cn, fallback);
		osk_resp_tlv_end(&rt, &mark);
	}

	/* Gueltigkeit, fest. Bis 2049 schreibt RFC 5280 Abschnitt 4.1.2.5
	 * UTCTime vor, ab 2050 GeneralizedTime - darum sind die zwei Felder
	 * verschiedene Typen, und das ist richtig so. */
	osk_resp_tlv_begin(&rt, OSK_DER_SEQUENCE, &mark);
	{
		static const char nb[] = "000101000000Z";
		static const char na[] = "99991231235959Z";

		osk_resp_tlv(&rt, OSK_DER_UTCTIME, (const uint8_t *)nb, sizeof(nb) - 1u);
		osk_resp_tlv(&rt, OSK_DER_GENTIME, (const uint8_t *)na, sizeof(na) - 1u);
	}
	osk_resp_tlv_end(&rt, &mark);

	/* Inhaber. */
	snprintf(cn, sizeof(cn), "openSecureKey PIV %02X", slot);
	osk_resp_tlv_begin(&rt, OSK_DER_SEQUENCE, &mark);
	osk_der_rdn(&rt, &osk_oid_cn, cn);
	osk_der_rdn(&rt, &osk_oid_serial_number, serial_hex);
	osk_der_rdn(&rt, &osk_oid_ou, slot_policy(slot));
	osk_resp_tlv_end(&rt, &mark);

	osk_resp_bytes(&rt, g_spki, spki_len);

	osk_resp_tlv_begin(&rt, OSK_DER_CTX3, &mark);
	{
		osk_tlv_mark_t exts;

		osk_resp_tlv_begin(&rt, OSK_DER_SEQUENCE, &exts);
		der_key_usage(&rt, slot, is_rsa);
		der_basic_constraints(&rt);
		osk_resp_tlv_end(&rt, &exts);
	}
	osk_resp_tlv_end(&rt, &mark);

	osk_resp_tlv_end(&rt, &tbs);
	if (rt.overflow)
		return OSK_ERR_FULL;
	tbs_len = rt.len;

	/* 5. Unterschreiben. Der Hash wird hier gebildet und nicht im Chip -
	 *    so verlangt es der Vertrag, und so zeigt die Anzeige spaeter
	 *    genau das, was unterschrieben wird (osk_se050.h). */
	osk_sha256(g_tbs, tbs_len, digest);
	if (osk_se050_sign(OSK_PIV_KEY_ID(OSK_PIV_SLOT_ATTEST), digest, sizeof(digest), sig,
			   sizeof(sig), &sig_len) != OSK_OK)
		return OSK_ERR_IO;

	/* 6. Zusammensetzen. */
	osk_resp_init(&ro, out, cap);
	osk_resp_tlv_begin(&ro, OSK_DER_SEQUENCE, &mark);
	osk_resp_bytes(&ro, g_tbs, tbs_len);
	osk_der_alg(&ro, &osk_oid_ecdsa_sha256, NULL, false);
	osk_der_bits(&ro, sig, sig_len);
	osk_resp_tlv_end(&ro, &mark);
	if (ro.overflow)
		return OSK_ERR_FULL;

	*len_out = ro.len;
	return OSK_OK;
}
