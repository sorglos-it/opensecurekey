/* DER schreiben. Siehe osk_der.h. */
#include <string.h>
#include "osk_der.h"
#include "osk_card.h"

/* --- Objektkennungen ---------------------------------------------------- */

/* Die Bytes sind die Kennung selbst, nach X.690 Abschnitt 8.19 gepackt:
 * erstes Byte 40*erster + zweiter Zweig, danach jede Zahl in Siebenerbits
 * mit gesetztem obersten Bit, solange noch etwas folgt. Aufgeschrieben
 * wird die fertige Form und nicht der Weg dorthin - geraten ist nichts,
 * jede Zeile nennt die Kennung im Klartext daneben. */

static const uint8_t b_ec_public[] = {0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01};
static const uint8_t b_ecdsa_sha256[] = {0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x02};
static const uint8_t b_rsa[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01};
static const uint8_t b_rsa_sha256[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b};
static const uint8_t b_cn[] = {0x55, 0x04, 0x03};
static const uint8_t b_country[] = {0x55, 0x04, 0x06};
static const uint8_t b_org[] = {0x55, 0x04, 0x0a};
static const uint8_t b_ou[] = {0x55, 0x04, 0x0b};
static const uint8_t b_serial_number[] = {0x55, 0x04, 0x05};
static const uint8_t b_key_usage[] = {0x55, 0x1d, 0x0f};
static const uint8_t b_san[] = {0x55, 0x1d, 0x11};
static const uint8_t b_basic_constraints[] = {0x55, 0x1d, 0x13};
static const uint8_t b_ext_key_usage[] = {0x55, 0x1d, 0x25};
static const uint8_t b_eku_server[] = {0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x01};
static const uint8_t b_eku_client[] = {0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x02};
static const uint8_t b_eku_code[] = {0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x03};
static const uint8_t b_eku_email[] = {0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x04};
static const uint8_t b_ext_request[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x09, 0x0e};

#define OID(name, bytes)            \
	const osk_oid_t name = {    \
	    (bytes),                \
	    (uint8_t)sizeof(bytes), \
	}

OID(osk_oid_ec_public, b_ec_public);
OID(osk_oid_ecdsa_sha256, b_ecdsa_sha256);
OID(osk_oid_rsa, b_rsa);
OID(osk_oid_rsa_sha256, b_rsa_sha256);
OID(osk_oid_cn, b_cn);
OID(osk_oid_country, b_country);
OID(osk_oid_org, b_org);
OID(osk_oid_ou, b_ou);
OID(osk_oid_serial_number, b_serial_number);
OID(osk_oid_key_usage, b_key_usage);
OID(osk_oid_san, b_san);
OID(osk_oid_basic_constraints, b_basic_constraints);
OID(osk_oid_ext_key_usage, b_ext_key_usage);
OID(osk_oid_eku_server, b_eku_server);
OID(osk_oid_eku_client, b_eku_client);
OID(osk_oid_eku_code, b_eku_code);
OID(osk_oid_eku_email, b_eku_email);
OID(osk_oid_ext_request, b_ext_request);

/* Die Kurvenkennungen kommen aus osk_card.c und werden hier nur verpackt. */
const osk_oid_t osk_oid_curve_p256 = {osk_oid_p256, (uint8_t)OSK_OID_P256_LEN};
const osk_oid_t osk_oid_curve_p384 = {osk_oid_p384, (uint8_t)OSK_OID_P384_LEN};

/* --- Schreiben ---------------------------------------------------------- */

void osk_der_oid(osk_resp_t *r, const osk_oid_t *oid)
{
	if (oid == NULL)
		return;
	osk_resp_tlv(r, OSK_DER_OID, oid->v, oid->len);
}

void osk_der_true(osk_resp_t *r)
{
	static const uint8_t t[] = {0xffu};

	osk_resp_tlv(r, OSK_DER_BOOLEAN, t, sizeof(t));
}

void osk_der_alg(osk_resp_t *r, const osk_oid_t *oid, const osk_oid_t *par, bool par_null)
{
	osk_tlv_mark_t m;

	osk_resp_tlv_begin(r, OSK_DER_SEQUENCE, &m);
	osk_der_oid(r, oid);
	if (par_null)
		osk_resp_tlv(r, OSK_DER_NULL, NULL, 0u);
	else if (par != NULL)
		osk_der_oid(r, par);
	osk_resp_tlv_end(r, &m);
}

void osk_der_uint(osk_resp_t *r, const uint8_t *v, size_t len)
{
	size_t at = 0;

	if (v == NULL || len == 0u)
		return;
	while (at + 1u < len && v[at] == 0x00u)
		at++;
	if ((v[at] & 0x80u) != 0u) {
		/* Das Laengenfeld NICHT selbst schreiben: ein Modulus mit
		 * 4096 Bit und gesetztem oberstem Bit ergibt 513 Byte, und das
		 * passt in kein einzelnes Byte. Genau dieser Fehler war in der
		 * Attestierung bis zum 29.10.2026 drin - bei einer Kurve
		 * faellt er nicht auf, weil dort nichts ueber 127 Byte lang
		 * wird, und er hat erst das RSA-Zertifikat gebrochen. Die
		 * Marke laesst osk_resp_tlv_end() die kuerzeste gueltige Form
		 * waehlen. */
		osk_tlv_mark_t m;

		osk_resp_tlv_begin(r, OSK_DER_INTEGER, &m);
		osk_resp_u8(r, 0x00u);
		osk_resp_bytes(r, v + at, len - at);
		osk_resp_tlv_end(r, &m);
	} else {
		osk_resp_tlv(r, OSK_DER_INTEGER, v + at, len - at);
	}
}

void osk_der_bits(osk_resp_t *r, const uint8_t *v, size_t len)
{
	osk_tlv_mark_t m;

	osk_resp_tlv_begin(r, OSK_DER_BITSTRING, &m);
	osk_resp_u8(r, 0x00u);
	osk_resp_bytes(r, v, len);
	osk_resp_tlv_end(r, &m);
}

/* Ist jedes Zeichen in einem PrintableString erlaubt? X.680 Tabelle 10. */
static bool printable_ok(const char *text)
{
	for (const char *p = text; *p != 0; p++) {
		const char c = *p;

		if (c >= 'A' && c <= 'Z')
			continue;
		if (c >= 'a' && c <= 'z')
			continue;
		if (c >= '0' && c <= '9')
			continue;
		if (strchr(" '()+,-./:=?", c) != NULL)
			continue;
		return false;
	}
	return true;
}

void osk_der_string(osk_resp_t *r, const char *text)
{
	if (text == NULL)
		return;
	osk_resp_tlv(r, printable_ok(text) ? OSK_DER_PRINTABLE : OSK_DER_UTF8,
		     (const uint8_t *)text, strlen(text));
}

void osk_der_rdn(osk_resp_t *r, const osk_oid_t *oid, const char *text)
{
	osk_tlv_mark_t set;
	osk_tlv_mark_t seq;

	if (text == NULL)
		return;
	osk_resp_tlv_begin(r, OSK_DER_SET, &set);
	osk_resp_tlv_begin(r, OSK_DER_SEQUENCE, &seq);
	osk_der_oid(r, oid);
	osk_der_string(r, text);
	osk_resp_tlv_end(r, &seq);
	osk_resp_tlv_end(r, &set);
}

void osk_der_ext_begin(osk_resp_t *r, const osk_oid_t *oid, bool critical, osk_tlv_mark_t *outer,
		       osk_tlv_mark_t *inner)
{
	osk_resp_tlv_begin(r, OSK_DER_SEQUENCE, outer);
	osk_der_oid(r, oid);
	/* critical = FALSE ist die Vorgabe und wird dann NICHT geschrieben:
	 * DER laesst ein Feld mit dem Vorgabewert weg (X.690 Abschnitt 11.5).
	 * Ein ausdrueckliches FALSE waere BER. */
	if (critical)
		osk_der_true(r);
	osk_resp_tlv_begin(r, OSK_DER_OCTETSTRING, inner);
}

void osk_der_ext_end(osk_resp_t *r, const osk_tlv_mark_t *outer, const osk_tlv_mark_t *inner)
{
	osk_resp_tlv_end(r, inner);
	osk_resp_tlv_end(r, outer);
}

void osk_der_key_usage_bits(osk_resp_t *r, uint16_t mask)
{
	osk_tlv_mark_t m;
	uint8_t bytes[2] = {0u, 0u};
	unsigned highest = 0;
	size_t used;
	uint8_t unused;

	/* Gezaehlt wird wie in RFC 5280 Abschnitt 4.2.1.3: Bit 0 ist das
	 * hoechstwertige Bit des ersten Bytes. Wer die Maske so hinschreibt,
	 * wie ein Rechner zaehlt, vertauscht digitalSignature mit
	 * decipherOnly. */
	for (unsigned n = 0; n < 16u; n++) {
		if ((mask & (uint16_t)(1u << n)) != 0u) {
			bytes[n / 8u] = (uint8_t)(bytes[n / 8u] | (0x80u >> (n % 8u)));
			highest = n;
		}
	}
	if (mask == 0u) {
		/* Leerer BIT STRING: ein Byte, das sagt, dass kein Bit
		 * ungenutzt ist. RFC 5280 verlangt mindestens ein Bit - wer
		 * hier 0 uebergibt, soll die Erweiterung gar nicht schreiben,
		 * und darum ist das hier kein stiller Ersatzwert. */
		osk_resp_tlv_begin(r, OSK_DER_BITSTRING, &m);
		osk_resp_u8(r, 0x00u);
		osk_resp_tlv_end(r, &m);
		return;
	}
	used = highest / 8u + 1u;
	unused = (uint8_t)(7u - (highest % 8u));

	osk_resp_tlv_begin(r, OSK_DER_BITSTRING, &m);
	osk_resp_u8(r, unused);
	osk_resp_bytes(r, bytes, used);
	osk_resp_tlv_end(r, &m);
}
