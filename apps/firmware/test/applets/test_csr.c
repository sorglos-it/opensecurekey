/* Zertifikatsanfrage nach PKCS#10: die Anfrage wird wirklich zerlegt,
 * nicht nur gezaehlt.
 *
 * WARUM SO AUSFUEHRLICH: eine falsch aufgebaute Anfrage faellt nicht hier
 * auf, sondern beim Aussteller - und dort sieht sie aus wie ein Problem
 * mit dem Schluessel. Also wird jedes Feld einzeln angesehen, in der
 * Reihenfolge, die RFC 2986 Abschnitt 4 vorschreibt.
 *
 * ZERLEGT WIRD MIT EINEM EIGENEN LESER, nicht mit den Hilfen aus
 * osk_apdu.c: geprueft wird gegen X.690 und nicht gegen die Umsetzung, die
 * geprueft werden soll. Die Tag-Werte stehen hier darum zum zweiten Mal.
 *
 * WAS DIESER LAUF NICHT BEWEIST: dass die Unterschrift stimmt. Die
 * Attrappe rechnet keine Kurve und kein RSA (mocks.h). Dafuer gibt es
 * csr_openssl.py - dort liest ein FREMDES Programm die Anfrage und
 * bestaetigt die Unterschrift. Was hier geprueft wird, ist der Aufbau und
 * dass der Hash ueber GENAU die Bytes des unterschriebenen Teils geht.
 *
 * Stufe 2 von vier: uebersetzt und am PC gelaufen. Am Geraet nicht. */
#include "test_util.h"
#include "osk_csr.h"
#include "osk_der.h"
#include "osk_spki.h"
#include "osk_se050.h"
#include "osk_rsa_card.h"
#include "osk_crypto.h"
#include "mocks.h"

/* Tags nach X.690 Abschnitt 8 - hier noch einmal und absichtlich nicht
 * aus osk_der.h. */
#define T_INTEGER 0x02u
#define T_BITSTRING 0x03u
#define T_OCTETSTRING 0x04u
#define T_NULL 0x05u
#define T_OID 0x06u
#define T_UTF8 0x0cu
#define T_PRINTABLE 0x13u
#define T_IA5 0x16u
#define T_SEQ 0x30u
#define T_SET 0x31u
#define T_CTX0 0xa0u

/* Kennungen, gegen die geprueft wird. Auch die stehen hier zum zweiten
 * Mal: eine Pruefung, die die Kennung aus der geprueften Datei nimmt,
 * prueft nichts. */
static const uint8_t oid_ec_pub[] = {0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01};
static const uint8_t oid_p256[] = {0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07};
static const uint8_t oid_p384[] = {0x2b, 0x81, 0x04, 0x00, 0x22};
static const uint8_t oid_ecdsa_sha256[] = {0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x02};
static const uint8_t oid_rsa_enc[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01};
static const uint8_t oid_rsa_sha256[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b};
static const uint8_t oid_cn[] = {0x55, 0x04, 0x03};
static const uint8_t oid_c[] = {0x55, 0x04, 0x06};
static const uint8_t oid_o[] = {0x55, 0x04, 0x0a};
static const uint8_t oid_ou[] = {0x55, 0x04, 0x0b};
static const uint8_t oid_sn[] = {0x55, 0x04, 0x05};
static const uint8_t oid_ku[] = {0x55, 0x1d, 0x0f};
static const uint8_t oid_san[] = {0x55, 0x1d, 0x11};
static const uint8_t oid_eku[] = {0x55, 0x1d, 0x25};
static const uint8_t oid_eku_client[] = {0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x02};
static const uint8_t oid_eku_email[] = {0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x04};
static const uint8_t oid_ext_req[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x09, 0x0e};

/* --- ein eigener DER-Leser --------------------------------------------- */

typedef struct {
	const uint8_t *p;
	size_t left;
} cur_t;

static cur_t into(const uint8_t *v, size_t len)
{
	cur_t c;

	c.p = v;
	c.left = len;
	return c;
}

/* Liest das naechste TLV. Laengen kurz, 81 und 82 - mehr kommt in einer
 * Anfrage nicht vor, und alles andere soll auffallen statt geraten zu
 * werden. */
static bool der_next(cur_t *c, uint8_t *tag, const uint8_t **val, size_t *len)
{
	size_t n;
	size_t hdr;

	if (c->left < 2u)
		return false;
	n = c->p[1];
	hdr = 2u;
	if (n == 0x81u) {
		if (c->left < 3u)
			return false;
		n = c->p[2];
		hdr = 3u;
	} else if (n == 0x82u) {
		if (c->left < 4u)
			return false;
		n = ((size_t)c->p[2] << 8) | (size_t)c->p[3];
		hdr = 4u;
	} else if (n > 0x82u) {
		return false;
	}
	if (hdr + n > c->left)
		return false;
	*tag = c->p[0];
	*val = c->p + hdr;
	*len = n;
	c->p += hdr + n;
	c->left -= hdr + n;
	return true;
}

/* Ein TLV mit erwartetem Tag. Liefert false und zaehlt einen Fehler, wenn
 * es fehlt oder anders ist. */
static bool want(cur_t *c, uint8_t tag, const uint8_t **val, size_t *len, const char *what)
{
	uint8_t got = 0;

	if (!der_next(c, &got, val, len)) {
		T_TRUE(what, false);
		return false;
	}
	T_EQ(what, got, tag);
	return got == tag;
}

static bool oid_is(const uint8_t *val, size_t len, const uint8_t *oid, size_t oid_len)
{
	return len == oid_len && memcmp(val, oid, len) == 0;
}

/* Steckt `what` irgendwo in `hay`? Eigene Suche, weil memmem() nicht
 * ueberall da ist und eine Pruefung sich nichts borgen soll. */
static bool contains(const uint8_t *hay, size_t hay_len, const uint8_t *what, size_t what_len)
{
	if (what_len == 0u || hay_len < what_len)
		return false;
	for (size_t i = 0; i + what_len <= hay_len; i++) {
		if (memcmp(hay + i, what, what_len) == 0)
			return true;
	}
	return false;
}

/* --- Schluessel in der Attrappe ---------------------------------------- */

#define KEY_EC 0x43535201u
#define KEY_EC384 0x43535202u
#define KEY_RSA 0x43535203u

static void make_ec(uint32_t id, osk_curve_t curve)
{
	osk_key_policy_t pol;

	memset(&pol, 0, sizeof(pol));
	T_EQ("Kurvenschluessel entsteht", osk_se050_key_create(id, curve, &pol), OSK_OK);
}

static void base_req(osk_csr_req_t *req, uint32_t id, osk_spki_key_t type)
{
	memset(req, 0, sizeof(*req));
	req->key = id;
	req->key_type = type;
	req->cn = "openSecureKey Test";
}

/* --- Der aeussere Aufbau ----------------------------------------------- */

static uint8_t g_der[OSK_CSR_MAX];
static size_t g_der_len;
static uint8_t g_info[OSK_CSR_MAX];
static size_t g_info_len;

/* Zerlegt die Anfrage bis auf die drei Teile und prueft dabei das, was
 * fuer JEDE Anfrage gilt. `info` zeigt hinterher in den unterschriebenen
 * Teil (ohne dessen eigenes Tag), `sig` auf die Unterschrift. */
static bool split(cur_t *info_out, const uint8_t **sig_out, size_t *sig_len_out,
		  const uint8_t *want_sig_oid, size_t want_sig_oid_len, bool want_null_par)
{
	cur_t top = into(g_der, g_der_len);
	cur_t outer;
	cur_t alg;
	const uint8_t *val = NULL;
	const uint8_t *start = NULL;
	size_t len = 0;
	size_t used;

	if (!want(&top, T_SEQ, &val, &len, "Anfrage ist eine SEQUENCE"))
		return false;
	T_EQ("nichts hinter der Anfrage", top.left, 0u);
	outer = into(val, len);

	/* 1. Der unterschriebene Teil - und er muss BYTEGLEICH das sein, was
	 *    osk_csr_info() liefert. Daran haengt alles: der Hash wird ueber
	 *    diese Bytes gebildet, und wenn in der fertigen Anfrage andere
	 *    stehen, gilt die Unterschrift fuer etwas, das niemand liest. */
	start = outer.p;
	if (!want(&outer, T_SEQ, &val, &len, "unterschriebener Teil ist eine SEQUENCE"))
		return false;
	used = (size_t)(outer.p - start);
	T_EQ("unterschriebener Teil hat die Laenge von osk_csr_info()", used, g_info_len);
	T_TRUE("unterschriebener Teil ist bytegleich osk_csr_info()",
	       used == g_info_len && memcmp(start, g_info, g_info_len) == 0);
	*info_out = into(val, len);

	/* 2. Das Verfahren der Unterschrift. */
	if (!want(&outer, T_SEQ, &val, &len, "signatureAlgorithm ist eine SEQUENCE"))
		return false;
	alg = into(val, len);
	if (want(&alg, T_OID, &val, &len, "signatureAlgorithm nennt eine Kennung"))
		T_TRUE("signatureAlgorithm ist die erwartete Kennung",
		       oid_is(val, len, want_sig_oid, want_sig_oid_len));
	if (want_null_par) {
		if (want(&alg, T_NULL, &val, &len, "RSA: ausdruecklicher NULL-Parameter"))
			T_EQ("der NULL-Parameter ist leer", len, 0u);
	} else {
		T_EQ("ECDSA: kein Parameter", alg.left, 0u);
	}

	/* 3. Die Unterschrift. */
	if (!want(&outer, T_BITSTRING, &val, &len, "signature ist ein BIT STRING"))
		return false;
	T_TRUE("BIT STRING hat kein ungenutztes Bit", len > 0u && val[0] == 0x00u);
	*sig_out = val + 1;
	*sig_len_out = len - 1u;
	T_EQ("nichts hinter der Unterschrift", outer.left, 0u);
	return true;
}

/* Die ersten zwei Felder des unterschriebenen Teils: Version und Betreff.
 * Liefert den Betreff zum Weiterzerlegen. */
static bool info_head(cur_t *info, cur_t *subject_out)
{
	const uint8_t *val = NULL;
	size_t len = 0;

	if (!want(info, T_INTEGER, &val, &len, "version ist ein INTEGER"))
		return false;
	T_EQ("version ist ein Byte lang", len, 1u);
	/* 0 heisst "diese Fassung" (RFC 2986 Abschnitt 4.1) - nicht 1 und
	 * nicht 2 wie bei X.509. */
	T_EQ("version ist 0", val[0], 0u);
	if (!want(info, T_SEQ, &val, &len, "subject ist eine SEQUENCE"))
		return false;
	*subject_out = into(val, len);
	return true;
}

/* Ein Namensteil: SET { SEQUENCE { OID, Zeichenkette } }. */
static bool rdn(cur_t *subject, const uint8_t *oid, size_t oid_len, const char *text, uint8_t tag,
		const char *what)
{
	const uint8_t *val = NULL;
	size_t len = 0;
	cur_t set;
	cur_t seq;

	if (!want(subject, T_SET, &val, &len, what))
		return false;
	set = into(val, len);
	if (!want(&set, T_SEQ, &val, &len, "Namensteil ist eine SEQUENCE"))
		return false;
	T_EQ("im Namensteil steht nur eine SEQUENCE", set.left, 0u);
	seq = into(val, len);
	if (want(&seq, T_OID, &val, &len, "Namensteil nennt eine Kennung"))
		T_TRUE("Namensteil hat die erwartete Kennung", oid_is(val, len, oid, oid_len));
	if (want(&seq, tag, &val, &len, "Namensteil hat den erwarteten Zeichenkettentyp")) {
		T_EQ("Namensteil ist so lang wie der Text", len, strlen(text));
		T_TRUE("Namensteil hat den erwarteten Text",
		       len == strlen(text) && memcmp(val, text, len) == 0);
	}
	T_EQ("nichts hinter dem Text", seq.left, 0u);
	return true;
}

/* SubjectPublicKeyInfo einer Kurve. */
static void check_spki_ec(cur_t *info, const uint8_t *curve, size_t curve_len, size_t point_len)
{
	const uint8_t *val = NULL;
	size_t len = 0;
	cur_t spki;
	cur_t alg;

	if (!want(info, T_SEQ, &val, &len, "subjectPKInfo ist eine SEQUENCE"))
		return;
	spki = into(val, len);
	if (!want(&spki, T_SEQ, &val, &len, "subjectPKInfo nennt ein Verfahren"))
		return;
	alg = into(val, len);
	if (want(&alg, T_OID, &val, &len, "Verfahren ist eine Kennung"))
		T_TRUE("Verfahren ist ecPublicKey",
		       oid_is(val, len, oid_ec_pub, sizeof(oid_ec_pub)));
	if (want(&alg, T_OID, &val, &len, "Kurve ist eine Kennung"))
		T_TRUE("Kurve ist die erwartete", oid_is(val, len, curve, curve_len));
	T_EQ("nichts hinter der Kurve", alg.left, 0u);
	if (want(&spki, T_BITSTRING, &val, &len, "oeffentlicher Schluessel ist ein BIT STRING")) {
		T_EQ("kein ungenutztes Bit", val[0], 0u);
		T_EQ("Punkt hat die Laenge der Kurve", len - 1u, point_len);
		T_EQ("Punkt ist unkomprimiert (0x04)", val[1], 0x04u);
	}
	T_EQ("nichts hinter dem Schluessel", spki.left, 0u);
}

/* --- Fall 1: eine Kurve, nur mit Namen --------------------------------- */

static void case_ec_minimal(void)
{
	osk_csr_req_t req;
	cur_t info;
	cur_t subject;
	const uint8_t *sig = NULL;
	const uint8_t *val = NULL;
	size_t sig_len = 0;
	size_t len = 0;
	uint8_t digest[OSK_SHA256_LEN];
	const uint8_t *seen;
	size_t seen_len = 0;

	mock_reset_all();
	make_ec(KEY_EC, OSK_CURVE_P256);
	base_req(&req, KEY_EC, OSK_SPKI_P256);

	T_EQ("osk_csr_info laeuft", osk_csr_info(&req, g_info, sizeof(g_info), &g_info_len),
	     OSK_OK);
	T_TRUE("osk_csr_info liefert etwas", g_info_len > 64u);
	T_EQ("osk_csr_der laeuft", osk_csr_der(&req, g_der, sizeof(g_der), &g_der_len), OSK_OK);
	T_TRUE("die Anfrage ist laenger als der unterschriebene Teil", g_der_len > g_info_len);

	if (!split(&info, &sig, &sig_len, oid_ecdsa_sha256, sizeof(oid_ecdsa_sha256), false))
		return;
	if (!info_head(&info, &subject))
		return;

	/* Nur ein Namensteil, und zwar der CN. */
	(void)rdn(&subject, oid_cn, sizeof(oid_cn), "openSecureKey Test", T_PRINTABLE,
		  "subject hat einen Namensteil");
	T_EQ("subject hat nur diesen einen Namensteil", subject.left, 0u);

	check_spki_ec(&info, oid_p256, sizeof(oid_p256), 65u);

	/* attributes [0] ist PFLICHT und steht auch leer da (RFC 2986
	 * Abschnitt 4.1). Eine Anfrage ohne das Feld ist keine Anfrage. */
	if (want(&info, T_CTX0, &val, &len, "attributes [0] ist da"))
		T_EQ("ohne Erweiterungen ist es leer", len, 0u);
	T_EQ("nichts hinter den Attributen", info.left, 0u);

	/* Die Unterschrift einer Kurve ist die SEQUENCE aus r und s, so wie
	 * der Chip sie liefert. */
	T_EQ("Unterschrift ist eine SEQUENCE", sig[0], T_SEQ);
	{
		cur_t s = into(sig, sig_len);
		cur_t rs;

		if (want(&s, T_SEQ, &val, &len, "Unterschrift laesst sich zerlegen")) {
			rs = into(val, len);
			(void)want(&rs, T_INTEGER, &val, &len, "r ist ein INTEGER");
			(void)want(&rs, T_INTEGER, &val, &len, "s ist ein INTEGER");
			T_EQ("nichts hinter s", rs.left, 0u);
		}
	}

	/* Und der Kern: unterschrieben wurde der SHA-256 ueber GENAU den
	 * unterschriebenen Teil. Nicht ueber die ganze Anfrage, nicht ueber
	 * einen Teil davon. */
	osk_sha256(g_info, g_info_len, digest);
	T_EQ("genau einmal unterschrieben", mock_sign_count(), 1u);
	seen = mock_sign_last_hash(&seen_len);
	T_EQ("der Hash ist 32 Byte lang", seen_len, (size_t)OSK_SHA256_LEN);
	T_TRUE("unterschrieben wurde der Hash des unterschriebenen Teils",
	       seen_len == sizeof(digest) && memcmp(seen, digest, seen_len) == 0);

	/* Zweimal gebaut ergibt dasselbe: nichts in der Anfrage haengt am
	 * Zufall. Eine Anfrage, die sich bei jedem Aufruf aendert, laesst
	 * sich nicht nachrechnen. */
	{
		uint8_t again[OSK_CSR_MAX];
		size_t again_len = 0;

		T_EQ("zweiter Lauf geht auch", osk_csr_der(&req, again, sizeof(again), &again_len),
		     OSK_OK);
		T_EQ("beide Laeufe sind gleich lang", again_len, g_der_len);
		T_TRUE("beide Laeufe sind bytegleich",
		       again_len == g_der_len && memcmp(again, g_der, g_der_len) == 0);
	}
}

/* --- Fall 2: voller Betreff und alle drei Erweiterungen ---------------- */

static void case_full(void)
{
	osk_csr_req_t req;
	cur_t info;
	cur_t subject;
	cur_t attrs;
	cur_t attr;
	cur_t values;
	cur_t exts;
	cur_t ext;
	cur_t inner;
	const uint8_t *sig = NULL;
	const uint8_t *val = NULL;
	size_t sig_len = 0;
	size_t len = 0;

	mock_reset_all();
	make_ec(KEY_EC, OSK_CURVE_P256);
	base_req(&req, KEY_EC, OSK_SPKI_P256);
	req.cn = "key.example.org";
	req.country = "DE";
	req.org = "Sorglos";
	req.org_unit = "PIV 9A";
	req.serial_number = "0123456789ABCDEF";
	req.san[0].kind = OSK_CSR_SAN_DNS;
	req.san[0].name = "key.example.org";
	req.san[1].kind = OSK_CSR_SAN_EMAIL;
	req.san[1].name = "thomas@example.org";
	req.san[2].kind = OSK_CSR_SAN_URI;
	req.san[2].name = "https://example.org/key";
	req.san_count = 3u;
	req.key_usage = OSK_KU_DIGITAL_SIGNATURE | OSK_KU_KEY_ENCIPHERMENT;
	req.eku[0] = OSK_CSR_EKU_CLIENT_AUTH;
	req.eku[1] = OSK_CSR_EKU_EMAIL_PROTECTION;
	req.eku_count = 2u;

	T_EQ("osk_csr_info laeuft", osk_csr_info(&req, g_info, sizeof(g_info), &g_info_len),
	     OSK_OK);
	T_EQ("osk_csr_der laeuft", osk_csr_der(&req, g_der, sizeof(g_der), &g_der_len), OSK_OK);
	if (!split(&info, &sig, &sig_len, oid_ecdsa_sha256, sizeof(oid_ecdsa_sha256), false))
		return;
	if (!info_head(&info, &subject))
		return;

	/* Von allgemein nach besonders - und die Geraetenummer hinter dem
	 * Namen, weil sie ihn naeher bestimmt. */
	(void)rdn(&subject, oid_c, sizeof(oid_c), "DE", T_PRINTABLE, "C ist der erste Namensteil");
	(void)rdn(&subject, oid_o, sizeof(oid_o), "Sorglos", T_PRINTABLE, "O folgt");
	(void)rdn(&subject, oid_ou, sizeof(oid_ou), "PIV 9A", T_PRINTABLE, "OU folgt");
	(void)rdn(&subject, oid_cn, sizeof(oid_cn), "key.example.org", T_PRINTABLE, "CN folgt");
	(void)rdn(&subject, oid_sn, sizeof(oid_sn), "0123456789ABCDEF", T_PRINTABLE,
		  "serialNumber ist der letzte");
	T_EQ("mehr Namensteile sind es nicht", subject.left, 0u);

	check_spki_ec(&info, oid_p256, sizeof(oid_p256), 65u);

	/* Die Erweiterungen reisen als Attribut mit: vier Ebenen, drei davon
	 * Mengen. Wer eine davon weglaesst, bekommt eine Anfrage, deren
	 * Erweiterungen niemand findet. */
	if (!want(&info, T_CTX0, &val, &len, "attributes [0] ist da"))
		return;
	attrs = into(val, len);
	if (!want(&attrs, T_SEQ, &val, &len, "ein Attribut"))
		return;
	T_EQ("nur dieses eine Attribut", attrs.left, 0u);
	attr = into(val, len);
	if (want(&attr, T_OID, &val, &len, "das Attribut nennt eine Kennung"))
		T_TRUE("die Kennung ist extensionRequest",
		       oid_is(val, len, oid_ext_req, sizeof(oid_ext_req)));
	if (!want(&attr, T_SET, &val, &len, "die Werte sind eine Menge"))
		return;
	values = into(val, len);
	if (!want(&values, T_SEQ, &val, &len, "ein Wert, die Liste der Erweiterungen"))
		return;
	T_EQ("nur dieser eine Wert", values.left, 0u);
	exts = into(val, len);

	/* a) Key Usage - kritisch, weil RFC 5280 Abschnitt 4.2.1.3 es
	 *    verlangt. */
	if (want(&exts, T_SEQ, &val, &len, "erste Erweiterung")) {
		ext = into(val, len);
		if (want(&ext, T_OID, &val, &len, "sie nennt eine Kennung"))
			T_TRUE("das ist keyUsage", oid_is(val, len, oid_ku, sizeof(oid_ku)));
		if (want(&ext, 0x01u, &val, &len, "keyUsage ist als kritisch markiert")) {
			T_EQ("das Kennzeichen ist ein Byte", len, 1u);
			T_EQ("und es ist 0xFF, nicht 0x01", val[0], 0xffu);
		}
		if (want(&ext, T_OCTETSTRING, &val, &len,
			 "der Inhalt steckt in einem OCTET STRING")) {
			inner = into(val, len);
			if (want(&inner, T_BITSTRING, &val, &len,
				 "der Inhalt ist ein BIT STRING")) {
				/* Bit 0 und Bit 2 gesetzt: das hoechste
				 * gesetzte Bit ist 2, also sind fuenf Bits am
				 * Ende ungenutzt, und das Byte ist 0xA0. Wer
				 * hier wie ein Rechner zaehlt, bekommt 0x05. */
				T_EQ("zwei Bytes: ungenutzte Bits und die Maske", len, 2u);
				T_EQ("fuenf ungenutzte Bits", val[0], 5u);
				T_EQ("digitalSignature und keyEncipherment", val[1], 0xa0u);
			}
			T_EQ("nichts hinter dem BIT STRING", inner.left, 0u);
		}
		T_EQ("nichts hinter dem Inhalt", ext.left, 0u);
	}

	/* b) Extended Key Usage - nicht kritisch, und darum steht KEIN
	 *    ausdrueckliches FALSE darin: DER laesst den Vorgabewert weg. */
	if (want(&exts, T_SEQ, &val, &len, "zweite Erweiterung")) {
		ext = into(val, len);
		if (want(&ext, T_OID, &val, &len, "sie nennt eine Kennung"))
			T_TRUE("das ist extKeyUsage", oid_is(val, len, oid_eku, sizeof(oid_eku)));
		if (want(&ext, T_OCTETSTRING, &val, &len,
			 "kein FALSE, sondern gleich der Inhalt")) {
			inner = into(val, len);
			if (want(&inner, T_SEQ, &val, &len, "die Zwecke sind eine SEQUENCE")) {
				cur_t list = into(val, len);

				if (want(&list, T_OID, &val, &len, "erster Zweck"))
					T_TRUE("clientAuth", oid_is(val, len, oid_eku_client,
								    sizeof(oid_eku_client)));
				if (want(&list, T_OID, &val, &len, "zweiter Zweck"))
					T_TRUE("emailProtection", oid_is(val, len, oid_eku_email,
									 sizeof(oid_eku_email)));
				T_EQ("mehr Zwecke sind es nicht", list.left, 0u);
			}
		}
	}

	/* c) Die Namen. Alle drei sind PRIMITIVE kontextbezogene Tags und
	 *    tragen eine IA5String - ein zusammengesetztes Tag (0xa2) waere
	 *    ein Name, den kein Pruefer liest. */
	if (want(&exts, T_SEQ, &val, &len, "dritte Erweiterung")) {
		ext = into(val, len);
		if (want(&ext, T_OID, &val, &len, "sie nennt eine Kennung"))
			T_TRUE("das ist subjectAltName",
			       oid_is(val, len, oid_san, sizeof(oid_san)));
		if (want(&ext, T_OCTETSTRING, &val, &len, "subjectAltName ist nicht kritisch")) {
			inner = into(val, len);
			if (want(&inner, T_SEQ, &val, &len, "die Namen sind eine SEQUENCE")) {
				cur_t list = into(val, len);

				if (want(&list, 0x82u, &val, &len, "dNSName hat das Tag [2]"))
					T_TRUE("der Name stimmt",
					       len == strlen("key.example.org") &&
						   memcmp(val, "key.example.org", len) == 0);
				if (want(&list, 0x81u, &val, &len, "rfc822Name hat das Tag [1]"))
					T_TRUE("die Adresse stimmt",
					       len == strlen("thomas@example.org") &&
						   memcmp(val, "thomas@example.org", len) == 0);
				if (want(&list, 0x86u, &val, &len, "URI hat das Tag [6]"))
					T_TRUE("die Adresse stimmt",
					       len == strlen("https://example.org/key") &&
						   memcmp(val, "https://example.org/key", len) ==
						       0);
				T_EQ("mehr Namen sind es nicht", list.left, 0u);
			}
		}
	}
	T_EQ("mehr Erweiterungen sind es nicht", exts.left, 0u);
	T_EQ("nichts hinter den Attributen", info.left, 0u);
}

/* --- Fall 3: P-384 ----------------------------------------------------- */

static void case_p384(void)
{
	osk_csr_req_t req;
	cur_t info;
	cur_t subject;
	const uint8_t *sig = NULL;
	size_t sig_len = 0;

	mock_reset_all();
	make_ec(KEY_EC384, OSK_CURVE_P384);
	base_req(&req, KEY_EC384, OSK_SPKI_P384);
	req.key_usage = OSK_KU_DIGITAL_SIGNATURE;

	T_EQ("osk_csr_info laeuft", osk_csr_info(&req, g_info, sizeof(g_info), &g_info_len),
	     OSK_OK);
	T_EQ("osk_csr_der laeuft", osk_csr_der(&req, g_der, sizeof(g_der), &g_der_len), OSK_OK);
	/* Unterschrieben wird mit SHA-256, auch bei einer P-384: das ist der
	 * einzige Hash dieser Schicht, und die Kennung sagt genau das. */
	if (!split(&info, &sig, &sig_len, oid_ecdsa_sha256, sizeof(oid_ecdsa_sha256), false))
		return;
	if (!info_head(&info, &subject))
		return;
	(void)rdn(&subject, oid_cn, sizeof(oid_cn), "openSecureKey Test", T_PRINTABLE, "CN ist da");
	check_spki_ec(&info, oid_p384, sizeof(oid_p384), 97u);
}

/* --- Fall 4: Zeichenkettentyp ------------------------------------------ */

/* Ein '@' und ein Umlaut sind in einem PrintableString nicht erlaubt. Wer
 * sie trotzdem hineinschreibt, baut eine Anfrage, die ein strenger Pruefer
 * ablehnt - und die Meldung redet dann vom Schluessel. */
static void case_strings(void)
{
	osk_csr_req_t req;
	cur_t info;
	cur_t subject;
	const uint8_t *sig = NULL;
	size_t sig_len = 0;

	mock_reset_all();
	make_ec(KEY_EC, OSK_CURVE_P256);
	base_req(&req, KEY_EC, OSK_SPKI_P256);
	req.cn = "thomas@example.org";
	req.org = "Gebaeude 7 (Sued)";

	T_EQ("osk_csr_info laeuft", osk_csr_info(&req, g_info, sizeof(g_info), &g_info_len),
	     OSK_OK);
	T_EQ("osk_csr_der laeuft", osk_csr_der(&req, g_der, sizeof(g_der), &g_der_len), OSK_OK);
	if (!split(&info, &sig, &sig_len, oid_ecdsa_sha256, sizeof(oid_ecdsa_sha256), false))
		return;
	if (!info_head(&info, &subject))
		return;
	/* Klammern und Bindestriche sind erlaubt - also bleibt O ein
	 * PrintableString. */
	(void)rdn(&subject, oid_o, sizeof(oid_o), "Gebaeude 7 (Sued)", T_PRINTABLE,
		  "O bleibt PrintableString");
	/* Das '@' ist es nicht. */
	(void)rdn(&subject, oid_cn, sizeof(oid_cn), "thomas@example.org", T_UTF8,
		  "CN mit @ wird UTF8String");
}

/* --- Fall 5: RSA ------------------------------------------------------- */

static void case_rsa(void)
{
	osk_csr_req_t req;
	osk_key_policy_t pol;
	cur_t info;
	cur_t subject;
	cur_t spki;
	cur_t alg;
	cur_t bits;
	cur_t nums;
	const uint8_t *sig = NULL;
	const uint8_t *val = NULL;
	size_t sig_len = 0;
	size_t len = 0;
	size_t mod_len = 0;
	uint8_t digest[OSK_SHA256_LEN];
	const uint8_t *seen;
	size_t seen_len = 0;

	mock_reset_all();
	memset(&pol, 0, sizeof(pol));
	osk_rsa_forget();

	if (!OSK_RSA_BUILT) {
		/* DIE ZWEITE RICHTUNG DER KCONFIG-OPTION. Ohne RSA gebaut
		 * heisst: die Anfrage wird abgelehnt und nicht etwa mit einem
		 * leeren Schluessel gebaut. Eine Option, die nur in einer
		 * Richtung geprueft ist, ist keine. */
		base_req(&req, KEY_RSA, OSK_SPKI_RSA);
		T_EQ("ohne RSA gebaut: keine RSA-Anfrage",
		     osk_csr_der(&req, g_der, sizeof(g_der), &g_der_len), OSK_ERR_UNSUPPORTED);
		T_EQ("auch der unterschriebene Teil nicht",
		     osk_csr_info(&req, g_info, sizeof(g_info), &g_info_len), OSK_ERR_UNSUPPORTED);
		return;
	}

	T_EQ("RSA-Schluessel entsteht", osk_se050_rsa_key_create(KEY_RSA, OSK_RSA_2048, &pol),
	     OSK_OK);
	base_req(&req, KEY_RSA, OSK_SPKI_RSA);
	req.key_usage = OSK_KU_DIGITAL_SIGNATURE;

	T_EQ("osk_csr_info laeuft", osk_csr_info(&req, g_info, sizeof(g_info), &g_info_len),
	     OSK_OK);
	T_EQ("osk_csr_der laeuft", osk_csr_der(&req, g_der, sizeof(g_der), &g_der_len), OSK_OK);

	/* Bei RSA ist die Kennung eine andere UND sie hat einen
	 * ausdruecklichen NULL-Parameter. Beides gehoert zusammen. */
	if (!split(&info, &sig, &sig_len, oid_rsa_sha256, sizeof(oid_rsa_sha256), true))
		return;
	if (!info_head(&info, &subject))
		return;
	(void)rdn(&subject, oid_cn, sizeof(oid_cn), "openSecureKey Test", T_PRINTABLE, "CN ist da");

	if (!want(&info, T_SEQ, &val, &len, "subjectPKInfo ist eine SEQUENCE"))
		return;
	spki = into(val, len);
	if (want(&spki, T_SEQ, &val, &len, "subjectPKInfo nennt ein Verfahren")) {
		alg = into(val, len);
		if (want(&alg, T_OID, &val, &len, "Verfahren ist eine Kennung"))
			T_TRUE("Verfahren ist rsaEncryption",
			       oid_is(val, len, oid_rsa_enc, sizeof(oid_rsa_enc)));
		if (want(&alg, T_NULL, &val, &len, "RSA hat einen ausdruecklichen NULL-Parameter"))
			T_EQ("und der ist leer", len, 0u);
		T_EQ("nichts dahinter", alg.left, 0u);
	}
	if (want(&spki, T_BITSTRING, &val, &len, "der Schluessel ist ein BIT STRING")) {
		T_EQ("kein ungenutztes Bit", val[0], 0u);
		bits = into(val + 1, len - 1u);
		if (want(&bits, T_SEQ, &val, &len, "darin Modulus und Exponent")) {
			nums = into(val, len);
			if (want(&nums, T_INTEGER, &val, &len, "Modulus ist ein INTEGER")) {
				/* Der Modulus der Attrappe hat das oberste Bit
				 * gesetzt - also MUSS eine fuehrende Null
				 * davor, sonst waere es eine negative Zahl.
				 * Genau diese Stelle hat am 29.10.2026 das
				 * RSA-Zertifikat gebrochen. */
				T_EQ("Modulus ist 2048 Bit plus fuehrende Null", len, 257u);
				T_EQ("die fuehrende Null ist da", val[0], 0u);
				T_EQ("und dahinter steht das gesetzte oberste Bit", val[1] & 0x80u,
				     0x80u);
				mod_len = len - 1u;
			}
			if (want(&nums, T_INTEGER, &val, &len, "Exponent ist ein INTEGER")) {
				/* DREI Byte, nicht vier - der Vertrag warnt
				 * ausdruecklich davor, vier anzunehmen. */
				T_EQ("Exponent ist drei Byte lang", len, 3u);
				T_HEX("Exponent ist 65537", val, len, "010001");
			}
			T_EQ("nichts hinter dem Exponenten", nums.left, 0u);
		}
	}

	/* Die Unterschrift ist so lang wie der Modulus - eine RSA-Signatur
	 * ist eine Zahl in dieser Breite und nicht kuerzer. */
	T_EQ("Unterschrift ist so lang wie der Modulus", sig_len, mod_len);

	/* Und die Angaben, die der Vertrag verlangt: die Auffuellung wird
	 * GENANNT, und das Hashverfahren auch - aus der Laenge ist es nicht
	 * ableitbar. */
	T_EQ("genau einmal unterschrieben", mock_rsa_sign_count(), 1u);
	T_EQ("Auffuellung ist PKCS#1 v1.5", mock_rsa_last_pad(), (int)OSK_RSA_PAD_PKCS1_V15);
	T_EQ("Hashverfahren ist SHA-256", mock_rsa_last_hash(), (int)OSK_HASH_SHA256);
	osk_sha256(g_info, g_info_len, digest);
	seen = mock_rsa_last_hash_val(&seen_len);
	T_EQ("der Hash ist 32 Byte lang", seen_len, (size_t)OSK_SHA256_LEN);
	T_TRUE("unterschrieben wurde der Hash des unterschriebenen Teils",
	       seen_len == sizeof(digest) && memcmp(seen, digest, seen_len) == 0);

	/* DER ANDERE GRUND, warum RSA fehlen kann: im Geraet sitzt ein
	 * SE051A2. Dieselbe Antwort, ein anderer Weg - und beide muessen
	 * geprueft sein. */
	mock_se050_set_rsa_mask(0u);
	osk_rsa_forget();
	T_EQ("Chip ohne RSA: keine RSA-Anfrage",
	     osk_csr_der(&req, g_der, sizeof(g_der), &g_der_len), OSK_ERR_UNSUPPORTED);
	mock_se050_set_rsa_mask((1u << OSK_RSA_2048) | (1u << OSK_RSA_3072) | (1u << OSK_RSA_4096));
	osk_rsa_forget();
}

/* Die beiden langen Schluessel: 3072 und 4096. Dass sie passen, ist keine
 * Rechnung, sondern eine Platzfrage - und die faellt sonst erst am Geraet
 * auf. */
static void case_rsa_long(void)
{
	osk_csr_req_t req;
	osk_key_policy_t pol;
	static const osk_rsa_bits_t sizes[] = {OSK_RSA_3072, OSK_RSA_4096};
	static const size_t bytes[] = {384u, 512u};

	if (!OSK_RSA_BUILT)
		return;
	for (unsigned i = 0; i < 2u; i++) {
		cur_t info;
		cur_t subject;
		const uint8_t *sig = NULL;
		size_t sig_len = 0;

		mock_reset_all();
		memset(&pol, 0, sizeof(pol));
		osk_rsa_forget();
		T_EQ("langer RSA-Schluessel entsteht",
		     osk_se050_rsa_key_create(KEY_RSA, sizes[i], &pol), OSK_OK);
		base_req(&req, KEY_RSA, OSK_SPKI_RSA);
		req.cn = "openSecureKey lang";
		req.san[0].kind = OSK_CSR_SAN_DNS;
		req.san[0].name = "lang.example.org";
		req.san_count = 1u;
		req.key_usage = OSK_KU_DIGITAL_SIGNATURE;

		T_EQ("osk_csr_info laeuft", osk_csr_info(&req, g_info, sizeof(g_info), &g_info_len),
		     OSK_OK);
		T_EQ("osk_csr_der laeuft", osk_csr_der(&req, g_der, sizeof(g_der), &g_der_len),
		     OSK_OK);
		T_TRUE("die Anfrage passt in den Puffer", g_der_len < OSK_CSR_MAX);
		if (!split(&info, &sig, &sig_len, oid_rsa_sha256, sizeof(oid_rsa_sha256), true))
			continue;
		T_EQ("Unterschrift ist so lang wie der Modulus", sig_len, bytes[i]);
		if (!info_head(&info, &subject))
			continue;
		(void)rdn(&subject, oid_cn, sizeof(oid_cn), "openSecureKey lang", T_PRINTABLE,
			  "CN ist da");
	}
}

/* --- Fall 6: PEM ------------------------------------------------------- */

/* Base64 zurueckrechnen. Eigener Leser, damit geprueft wird, was wirklich
 * in der Datei steht - und nicht, dass zweimal derselbe Code dasselbe
 * tut. */
static int b64_val(char c)
{
	if (c >= 'A' && c <= 'Z')
		return c - 'A';
	if (c >= 'a' && c <= 'z')
		return c - 'a' + 26;
	if (c >= '0' && c <= '9')
		return c - '0' + 52;
	if (c == '+')
		return 62;
	if (c == '/')
		return 63;
	return -1;
}

static void case_pem(void)
{
	static const char head[] = "-----BEGIN CERTIFICATE REQUEST-----\n";
	static const char foot[] = "-----END CERTIFICATE REQUEST-----\n";
	osk_csr_req_t req;
	static char pem[OSK_CSR_PEM_MAX];
	static uint8_t back[OSK_CSR_MAX];
	size_t pem_len = 0;
	size_t back_len = 0;
	size_t longest = 0;
	size_t col = 0;
	uint32_t acc = 0;
	unsigned have = 0;
	bool ok = true;

	mock_reset_all();
	make_ec(KEY_EC, OSK_CURVE_P256);
	base_req(&req, KEY_EC, OSK_SPKI_P256);
	req.san[0].kind = OSK_CSR_SAN_DNS;
	req.san[0].name = "pem.example.org";
	req.san_count = 1u;

	T_EQ("osk_csr_der laeuft", osk_csr_der(&req, g_der, sizeof(g_der), &g_der_len), OSK_OK);
	T_EQ("osk_csr_pem laeuft", osk_csr_pem(g_der, g_der_len, pem, sizeof(pem), &pem_len),
	     OSK_OK);

	T_EQ("die gemeldete Laenge stimmt mit dem Text", pem_len, strlen(pem));
	T_TRUE("die erste Zeile ist die von RFC 7468",
	       pem_len > sizeof(head) && memcmp(pem, head, sizeof(head) - 1u) == 0);
	T_TRUE("die letzte Zeile ist die von RFC 7468",
	       pem_len > sizeof(foot) &&
		   memcmp(pem + pem_len - (sizeof(foot) - 1u), foot, sizeof(foot) - 1u) == 0);
	T_EQ("die Datei endet mit einem Zeilenumbruch", pem[pem_len - 1u], '\n');

	/* Keine Zeile laenger als 64 Zeichen - sonst lesen manche Werkzeuge
	 * die Datei nicht. */
	for (size_t i = sizeof(head) - 1u; i < pem_len - (sizeof(foot) - 1u); i++) {
		if (pem[i] == '\n') {
			if (col > longest)
				longest = col;
			col = 0;
		} else {
			col++;
		}
	}
	T_EQ("keine Zeile ist laenger als 64 Zeichen", longest, 64u);
	T_EQ("die letzte Base64-Zeile ist abgeschlossen", col, 0u);

	/* Und das Entscheidende: zurueckgerechnet steht genau das DER da.
	 * Ein PEM, das etwas anderes enthaelt als die Anfrage, ist eine
	 * zweite Anfrage. */
	for (size_t i = sizeof(head) - 1u; i < pem_len - (sizeof(foot) - 1u); i++) {
		const char c = pem[i];
		int v;

		if (c == '\n')
			continue;
		if (c == '=')
			continue;
		v = b64_val(c);
		if (v < 0) {
			ok = false;
			break;
		}
		acc = (acc << 6) | (uint32_t)v;
		have += 6u;
		if (have >= 8u) {
			have -= 8u;
			if (back_len >= sizeof(back)) {
				ok = false;
				break;
			}
			back[back_len++] = (uint8_t)((acc >> have) & 0xffu);
		}
	}
	T_TRUE("das PEM enthaelt nur Base64-Zeichen", ok);
	T_EQ("zurueckgerechnet ist es so lang wie das DER", back_len, g_der_len);
	T_TRUE("und bytegleich", back_len == g_der_len && memcmp(back, g_der, g_der_len) == 0);

	/* Zu kleiner Puffer: lieber nichts als die Haelfte. Eine halb
	 * geschriebene Anfrage sieht aus wie eine ganze. */
	{
		char small[64];
		size_t n = 0;

		T_EQ("zu kleiner Puffer wird gemeldet",
		     osk_csr_pem(g_der, g_der_len, small, sizeof(small), &n), OSK_ERR_FULL);
		T_EQ("und nichts geschrieben", n, 0u);
	}
	T_EQ("ohne DER kein PEM", osk_csr_pem(NULL, 10u, pem, sizeof(pem), &pem_len), OSK_ERR_ARG);
	T_EQ("ohne Laenge kein PEM", osk_csr_pem(g_der, 0u, pem, sizeof(pem), &pem_len),
	     OSK_ERR_ARG);
}

/* --- Fall 7: was abgelehnt werden muss --------------------------------- */

static void case_errors(void)
{
	osk_csr_req_t req;
	size_t n = 0;

	mock_reset_all();
	make_ec(KEY_EC, OSK_CURVE_P256);

	T_EQ("keine Anfrage", osk_csr_info(NULL, g_der, sizeof(g_der), &n), OSK_ERR_ARG);
	T_EQ("kein Puffer", osk_csr_der(NULL, NULL, 0u, &n), OSK_ERR_ARG);

	base_req(&req, KEY_EC, OSK_SPKI_P256);
	req.cn = NULL;
	T_EQ("ohne Namen keine Anfrage", osk_csr_der(&req, g_der, sizeof(g_der), &n), OSK_ERR_ARG);
	req.cn = "";
	T_EQ("ein leerer Name ist kein Name", osk_csr_der(&req, g_der, sizeof(g_der), &n),
	     OSK_ERR_ARG);

	base_req(&req, KEY_EC, OSK_SPKI_P256);
	req.san_count = (uint8_t)(OSK_CSR_SAN_MAX + 1u);
	T_EQ("zu viele Namen", osk_csr_der(&req, g_der, sizeof(g_der), &n), OSK_ERR_ARG);

	base_req(&req, KEY_EC, OSK_SPKI_P256);
	req.eku_count = (uint8_t)(OSK_CSR_EKU_MAX + 1u);
	T_EQ("zu viele Zwecke", osk_csr_der(&req, g_der, sizeof(g_der), &n), OSK_ERR_ARG);

	base_req(&req, KEY_EC, OSK_SPKI_P256);
	req.key_type = (osk_spki_key_t)99;
	T_EQ("unbekannte Schluesselart", osk_csr_der(&req, g_der, sizeof(g_der), &n), OSK_ERR_ARG);

	/* Ein Steckplatz ohne Schluessel: NOT_FOUND, und nicht eine Anfrage
	 * mit einem leeren oeffentlichen Teil. */
	base_req(&req, 0x43539999u, OSK_SPKI_P256);
	T_EQ("kein Schluessel im Chip", osk_csr_der(&req, g_der, sizeof(g_der), &n),
	     OSK_ERR_NOT_FOUND);

	/* Zu kleiner Puffer, an beiden Stellen. */
	base_req(&req, KEY_EC, OSK_SPKI_P256);
	T_EQ("zu kleiner Puffer beim unterschriebenen Teil", osk_csr_info(&req, g_der, 32u, &n),
	     OSK_ERR_FULL);
	T_EQ("zu kleiner Puffer bei der Anfrage", osk_csr_der(&req, g_der, 64u, &n), OSK_ERR_FULL);

	/* Eine Kurve, die nicht zum Schluessel passt: der Chip liefert 65
	 * Byte, die Anfrage verlangt 97 - das muss auffallen und darf nicht
	 * einen halben Punkt eintragen. */
	base_req(&req, KEY_EC, OSK_SPKI_P384);
	T_EQ("Kurve passt nicht zum Schluessel", osk_csr_der(&req, g_der, sizeof(g_der), &n),
	     OSK_ERR_FORMAT);

	/* Der Chip unterschreibt nicht. */
	mock_reset_all();
	make_ec(KEY_EC, OSK_CURVE_P256);
	base_req(&req, KEY_EC, OSK_SPKI_P256);
	T_EQ("mit Schluessel geht es wieder", osk_csr_der(&req, g_der, sizeof(g_der), &n), OSK_OK);
}

/* --- Fall 8: eine von aussen gelegte Unterschrift ---------------------- */

/* Das ist der Weg, auf dem csr_openssl.py den echten Beleg holt: eine
 * Unterschrift, die ein fremdes Programm gerechnet hat, kommt
 * unveraendert in die Anfrage. Dass dieser Weg traegt, wird hier geprueft
 * - sonst steht und faellt der Beleg mit einem ungeprueften Stueck
 * Attrappe. */
static void case_injected(void)
{
	osk_csr_req_t req;
	static const uint8_t fake[] = {0x30, 0x06, 0x02, 0x01, 0x07, 0x02, 0x01, 0x09};
	cur_t info;
	const uint8_t *sig = NULL;
	size_t sig_len = 0;

	mock_reset_all();
	make_ec(KEY_EC, OSK_CURVE_P256);
	base_req(&req, KEY_EC, OSK_SPKI_P256);
	mock_se050_set_signature(fake, sizeof(fake));

	T_EQ("osk_csr_info laeuft", osk_csr_info(&req, g_info, sizeof(g_info), &g_info_len),
	     OSK_OK);
	T_EQ("osk_csr_der laeuft", osk_csr_der(&req, g_der, sizeof(g_der), &g_der_len), OSK_OK);
	if (!split(&info, &sig, &sig_len, oid_ecdsa_sha256, sizeof(oid_ecdsa_sha256), false))
		return;
	T_EQ("die gelegte Unterschrift ist so lang wie gelegt", sig_len, sizeof(fake));
	T_TRUE("und bytegleich", sig_len == sizeof(fake) && memcmp(sig, fake, sig_len) == 0);

	/* Und der oeffentliche Teil laesst sich genauso legen. */
	{
		static uint8_t point[65];
		cur_t info2;

		point[0] = 0x04u;
		for (size_t i = 1; i < sizeof(point); i++)
			point[i] = (uint8_t)(0xa0u + i);
		mock_se050_set_public(point, sizeof(point));
		/* Der unterschriebene Teil aendert sich mit dem Punkt - also
		 * neu holen, sonst vergleicht split() gegen den alten. */
		T_EQ("osk_csr_info mit gelegtem Punkt",
		     osk_csr_info(&req, g_info, sizeof(g_info), &g_info_len), OSK_OK);
		T_EQ("mit gelegtem Punkt laeuft es auch",
		     osk_csr_der(&req, g_der, sizeof(g_der), &g_der_len), OSK_OK);
		if (split(&info2, &sig, &sig_len, oid_ecdsa_sha256, sizeof(oid_ecdsa_sha256),
			  false)) {
			T_TRUE("der gelegte Punkt steht in der Anfrage",
			       contains(g_der, g_der_len, point, sizeof(point)));
		}
	}
}

int main(void)
{
	case_ec_minimal();
	case_full();
	case_p384();
	case_strings();
	case_rsa();
	case_rsa_long();
	case_pem();
	case_errors();
	case_injected();
	return t_report("test_csr");
}
