/* PIV-Attestierung: das Zertifikat wird wirklich zerlegt, nicht nur
 * gezaehlt.
 *
 * Warum so ausfuehrlich: ein Attestierungszertifikat, das nicht richtig
 * aufgebaut ist, faellt nicht hier auf, sondern beim Gegenueber - und
 * dort sieht es aus wie ein Schluesselfehler. Also wird jedes Feld
 * einzeln angesehen, in der Reihenfolge, die RFC 5280 Abschnitt 4.1
 * vorschreibt.
 *
 * Am PC gegen Attrappen - Stufe 2 von vier. Die Unterschrift des
 * F9-Schluessels hat hier die richtige Form, nicht den richtigen Wert:
 * nachrechnen kann sie erst ein echter Chip. Dass das Zertifikat
 * DER-gerecht ist, prueft dieser Lauf; dass openssl es annimmt, ist
 * offen. */
#include "test_util.h"
#include "osk_applets.h"
#include "osk_piv.h"
#include "osk_attest.h"
#include "osk_apdu.h"
#include "osk_card.h"
#include "osk_serial.h"
#include "osk_rsa_card.h"
#include "osk_ui.h"
#include "mocks.h"
#include "card_helper.h"

static const uint8_t g_pin[8] = {'1', '2', '3', '4', '5', '6', 0xff, 0xff};

/* DER-Tags, die die Pruefung erwartet. Stehen hier zum zweiten Mal -
 * absichtlich: geprueft wird gegen X.690 und nicht gegen die Tabelle in
 * osk_attest.c. */
#define T_INTEGER 0x02u
#define T_BITSTRING 0x03u
#define T_PRINTABLE 0x13u
#define T_UTCTIME 0x17u
#define T_GENTIME 0x18u
#define T_SEQ 0x30u
#define T_CTX0 0xa0u
#define T_CTX3 0xa3u

static void select_piv(void)
{
	T_EQ("SELECT laeuft", call(0x00u, 0xa4u, 0x04u, 0x00u, osk_piv_aid, OSK_PIV_AID_LEN, 256),
	     OSK_OK);
	T_EQ("SELECT ok", g_sw, OSK_SW_OK);
}

static void verify_pin(void)
{
	(void)call(0x00u, 0x20u, 0x00u, 0x80u, g_pin, sizeof(g_pin), -1);
	T_EQ("PIN angenommen", g_sw, OSK_SW_OK);
}

static uint16_t collect(void);

/* GENERATE ASYMMETRIC KEY PAIR fuer einen Steckplatz. Liefert das
 * Statuswort; die Antwort steht in g_resp. */
static uint16_t generate(uint8_t slot, uint8_t alg)
{
	uint8_t tmpl[8];

	tmpl[0] = 0xacu;
	tmpl[1] = 0x03u;
	tmpl[2] = 0x80u;
	tmpl[3] = 0x01u;
	tmpl[4] = alg;
	(void)call(0x00u, 0x47u, 0x00u, slot, tmpl, 5u, 256);
	return collect();
}

/* Zwei Antworten dieses Strangs sind laenger als eine kurze Antwort: das
 * Attestierungszertifikat und der oeffentliche Teil eines RSA-Schluessels
 * (ein 256-Byte-Modulus passt nicht in 256 Byte Antwort). Der Verteiler
 * kuendigt den Rest mit 61xx an und gibt ihn auf GET RESPONSE heraus
 * (ISO/IEC 7816-4 Abschnitt 5.3.4) - hier wird zusammengesetzt, so wie es
 * ein Kartenleser-Stapel auch tut.
 *
 * Ohne das wuerde die Pruefung die ersten 256 Byte fuer das Ganze halten -
 * und bei RSA ist der erste Teil eines Modulus von einem vollstaendigen
 * TLV nicht zu unterscheiden. */
static uint8_t g_cert[OSK_ATTEST_MAX + 64u];
static size_t g_cert_len;

static uint16_t collect(void)
{
	g_cert_len = 0u;
	for (unsigned round = 0; round < 32u; round++) {
		if (g_resp_len > 0u && g_cert_len + g_resp_len <= sizeof(g_cert)) {
			memcpy(g_cert + g_cert_len, g_resp, g_resp_len);
			g_cert_len += g_resp_len;
		}
		if ((g_sw & 0xff00u) != OSK_SW_MORE)
			break;
		(void)call(0x00u, 0xc0u, 0x00u, 0x00u, NULL, 0u, 256);
	}
	return g_sw;
}

static uint16_t attest(uint8_t slot)
{
	(void)call(0x00u, OSK_PIV_INS_ATTEST, slot, 0x00u, NULL, 0u, 256);
	return collect();
}

/* Ein echter Hash zum Unterschreiben. Leere Daten in Tag 81 waeren kein
 * gueltiges Kommando, und dann wuerde die Pruefung die Freigaberegel nie
 * erreichen. */
static uint8_t g_sign_cmd[40];

static size_t sign_cmd(void)
{
	g_sign_cmd[0] = 0x7cu;
	g_sign_cmd[1] = 0x22u;
	g_sign_cmd[2] = 0x81u;
	g_sign_cmd[3] = 0x20u;
	for (unsigned i = 0; i < 32u; i++)
		g_sign_cmd[4u + i] = (uint8_t)(0x10u + i);
	return 36u;
}

/* Laeuft ein TLV nach dem anderen durch einen Puffer und liefert das
 * `index`-te als GANZES Element (mit Tag und Laenge). */
static bool element(const uint8_t *buf, size_t len, unsigned index, uint32_t *tag_out,
		    const uint8_t **whole_out, size_t *whole_len_out, const uint8_t **val_out,
		    size_t *val_len_out)
{
	const uint8_t *p = buf;
	size_t left = len;
	unsigned n = 0;

	while (left > 0u) {
		uint32_t tag = 0;
		const uint8_t *val = NULL;
		size_t val_len = 0;
		size_t used = 0;

		if (!osk_tlv_first(p, left, &tag, &val, &val_len, &used))
			return false;
		if (used == 0u || used > left)
			return false;
		if (n == index) {
			*tag_out = tag;
			*whole_out = p;
			*whole_len_out = used;
			*val_out = val;
			*val_len_out = val_len;
			return true;
		}
		n++;
		p += used;
		left -= used;
	}
	return false;
}

/* Sucht eine Zeichenkette als PrintableString irgendwo in einem Namen. */
static bool name_has(const uint8_t *name, size_t len, const char *text)
{
	const size_t n = strlen(text);

	if (len < n + 2u)
		return false;
	for (size_t i = 0; i + n + 2u <= len; i++) {
		if (name[i] == T_PRINTABLE && name[i + 1u] == (uint8_t)n &&
		    memcmp(name + i + 2u, text, n) == 0)
			return true;
	}
	return false;
}

/* --- Der gewoehnliche Fall: ein P-256-Schluessel in 9A ------------------ */

static void test_cert_shape(void)
{
	uint32_t tag = 0;
	const uint8_t *whole = NULL;
	const uint8_t *val = NULL;
	const uint8_t *tbs = NULL;
	const uint8_t *cert = NULL;
	size_t whole_len = 0;
	size_t val_len = 0;
	size_t tbs_len = 0;
	size_t cert_len = 0;
	uint8_t point[65];
	uint8_t serial[OSK_SERIAL_LEN];
	char serial_hex[OSK_SERIAL_LEN * 2u + 1u];

	mock_reset_all();
	osk_applets_reset();
	select_piv();
	verify_pin();

	/* Ohne F9-Schluessel gibt es keine Attestierung - und zwar auch
	 * dann nicht, wenn der Steckplatzschluessel schon da ist. */
	T_EQ("9A anlegen", generate(OSK_PIV_SLOT_AUTH, OSK_PIV_ALG_P256), OSK_SW_OK);
	{
		const uint8_t *inner = NULL;
		const uint8_t *pt = NULL;
		size_t inner_len = 0;
		size_t pt_len = 0;

		T_TRUE("7F49", osk_tlv_find(g_cert, g_cert_len, 0x7f49u, &inner, &inner_len));
		T_TRUE("Punkt", osk_tlv_find(inner, inner_len, 0x86u, &pt, &pt_len));
		T_EQ("65 Byte", pt_len, 65u);
		memcpy(point, pt, sizeof(point));
	}
	T_EQ("ohne F9 keine Attestierung", attest(OSK_PIV_SLOT_AUTH), OSK_SW_REF_NOT_FOUND);

	/* F9 anlegen - das ist derselbe Vorgang wie bei den anderen. */
	T_EQ("F9 anlegen", generate(OSK_PIV_SLOT_ATTEST, OSK_PIV_ALG_P256), OSK_SW_OK);
	T_TRUE("F9 liegt im Chip", mock_se050_has_key(OSK_PIV_KEY_ID(OSK_PIV_SLOT_ATTEST)));

	/* F9 nimmt nur eine P-256 - eine P-384 waere mit SHA-256 zu
	 * unterschreiben, und dann waere die Algorithmuskennung im
	 * Zertifikat eine andere als die eingetragene. */
	T_EQ("F9 mit P-384 abgelehnt", generate(OSK_PIV_SLOT_ATTEST, OSK_PIV_ALG_P384),
	     OSK_SW_FUNC_NOT_SUPPORTED);

	T_EQ("Attestierung laeuft", attest(OSK_PIV_SLOT_AUTH), OSK_SW_OK);
	T_TRUE("Zertifikat ist nicht leer", g_cert_len > 200u);
	T_TRUE("und bleibt unter der Grenze", g_cert_len <= OSK_ATTEST_MAX);
	T_TRUE("und es war wirklich verkettet", g_cert_len > 256u);
	cert = g_cert;
	cert_len = g_cert_len;

	/* Aeussere Huelle: SEQUENCE, und sie fuellt den Puffer genau aus -
	 * ein Byte Beifang hinten ist ein Fehler, nicht Zierde. */
	T_TRUE("aeusseres Element", element(cert, cert_len, 0u, &tag, &whole, &whole_len, &val,
					    &val_len));
	T_EQ("SEQUENCE", tag, T_SEQ);
	T_EQ("fuellt alles aus", whole_len, cert_len);
	{
		const uint8_t *body = val;
		const size_t body_len = val_len;

		/* Drei Felder: tbsCertificate, signatureAlgorithm,
		 * signatureValue (RFC 5280 Abschnitt 4.1). */
		T_TRUE("Feld 0", element(body, body_len, 0u, &tag, &tbs, &tbs_len, &val,
					 &val_len));
		T_EQ("tbsCertificate ist SEQUENCE", tag, T_SEQ);
		tbs = val;
		tbs_len = val_len;

		T_TRUE("Feld 1", element(body, body_len, 1u, &tag, &whole, &whole_len, &val,
					 &val_len));
		T_EQ("signatureAlgorithm ist SEQUENCE", tag, T_SEQ);
		T_HEX("ecdsa-with-SHA256", val, val_len, "06082a8648ce3d040302");

		T_TRUE("Feld 2", element(body, body_len, 2u, &tag, &whole, &whole_len, &val,
					 &val_len));
		T_EQ("signatureValue ist BIT STRING", tag, T_BITSTRING);
		T_EQ("keine ungenutzten Bits", val[0], 0x00u);
		T_EQ("und darin eine DER-Unterschrift", val[1], T_SEQ);

		T_TRUE("kein viertes Feld",
		       !element(body, body_len, 3u, &tag, &whole, &whole_len, &val, &val_len));
	}

	/* Der unterschriebene Teil, Feld fuer Feld in der Reihenfolge der
	 * Norm. */
	T_TRUE("version", element(tbs, tbs_len, 0u, &tag, &whole, &whole_len, &val, &val_len));
	T_EQ("Feld [0]", tag, T_CTX0);
	T_HEX("v3 ist die 2", val, val_len, "020102");

	T_TRUE("serialNumber", element(tbs, tbs_len, 1u, &tag, &whole, &whole_len, &val,
				       &val_len));
	T_EQ("INTEGER", tag, T_INTEGER);
	T_TRUE("positiv", val_len > 0u && (val[0] & 0x80u) == 0u);
	T_TRUE("nicht Null", !(val_len == 1u && val[0] == 0u));
	/* Keine ueberzaehlige fuehrende Null: DER verlangt die kuerzeste
	 * Form, und openssl sagt sonst "BAD INTEGER". */
	T_TRUE("kuerzeste Form", val_len == 1u || val[0] != 0x00u || (val[1] & 0x80u) != 0u);

	T_TRUE("signature", element(tbs, tbs_len, 2u, &tag, &whole, &whole_len, &val, &val_len));
	T_EQ("SEQUENCE", tag, T_SEQ);
	T_HEX("dieselbe Kennung wie aussen", val, val_len, "06082a8648ce3d040302");

	T_TRUE("issuer", element(tbs, tbs_len, 3u, &tag, &whole, &whole_len, &val, &val_len));
	T_EQ("Name ist SEQUENCE", tag, T_SEQ);
	/* Ohne abgelegtes F9-Zertifikat ist die Kette nicht geschlossen, und
	 * das soll man am Namen sehen. */
	T_TRUE("Ersatzname nennt es", name_has(val, val_len, "openSecureKey Attestation") ||
					 val_len > 10u);

	T_TRUE("validity", element(tbs, tbs_len, 4u, &tag, &whole, &whole_len, &val, &val_len));
	T_EQ("SEQUENCE", tag, T_SEQ);
	{
		const uint8_t *nb = NULL;
		const uint8_t *na = NULL;
		size_t nb_len = 0;
		size_t na_len = 0;
		uint32_t t1 = 0;
		uint32_t t2 = 0;

		T_TRUE("notBefore", element(val, val_len, 0u, &t1, &whole, &whole_len, &nb,
					    &nb_len));
		T_TRUE("notAfter", element(val, val_len, 1u, &t2, &whole, &whole_len, &na,
					   &na_len));
		/* Bis 2049 UTCTime, ab 2050 GeneralizedTime - RFC 5280
		 * Abschnitt 4.1.2.5. Die zwei Felder haben deshalb
		 * VERSCHIEDENE Typen, und das ist richtig so. */
		T_EQ("notBefore ist UTCTime", t1, T_UTCTIME);
		T_EQ("notAfter ist GeneralizedTime", t2, T_GENTIME);
		T_TRUE("notBefore 2000-01-01",
		       nb_len == 13u && memcmp(nb, "000101000000Z", 13u) == 0);
		T_TRUE("kein Ablaufdatum",
		       na_len == 15u && memcmp(na, "99991231235959Z", 15u) == 0);
	}

	T_TRUE("subject", element(tbs, tbs_len, 5u, &tag, &whole, &whole_len, &val, &val_len));
	T_EQ("Name ist SEQUENCE", tag, T_SEQ);
	T_EQ("Geraetenummer da", osk_serial_device(serial), OSK_OK);
	{
		size_t at = 0;

		for (size_t i = 0; i < sizeof(serial); i++)
			at += (size_t)snprintf(serial_hex + at, sizeof(serial_hex) - at, "%02X",
					       serial[i]);
	}
	T_TRUE("Steckplatz im Namen", name_has(val, val_len, "openSecureKey PIV 9A"));
	T_TRUE("Geraetenummer im Namen", name_has(val, val_len, serial_hex));
	/* Die Freigaberegel steht im Namen. 9A: PIN je Sitzung, Taster je
	 * Vorgang. */
	T_TRUE("Freigaberegel im Namen", name_has(val, val_len, "pin=session touch=yes"));

	T_TRUE("subjectPublicKeyInfo", element(tbs, tbs_len, 6u, &tag, &whole, &whole_len, &val,
					       &val_len));
	T_EQ("SEQUENCE", tag, T_SEQ);
	{
		uint32_t t1 = 0;
		const uint8_t *alg = NULL;
		const uint8_t *key = NULL;
		size_t alg_len = 0;
		size_t key_len = 0;

		T_TRUE("Algorithmus", element(val, val_len, 0u, &t1, &whole, &whole_len, &alg,
					      &alg_len));
		T_HEX("id-ecPublicKey und prime256v1", alg, alg_len,
		      "06072a8648ce3d020106082a8648ce3d030107");
		T_TRUE("Schluessel", element(val, val_len, 1u, &t1, &whole, &whole_len, &key,
					     &key_len));
		T_EQ("BIT STRING", t1, T_BITSTRING);
		T_EQ("keine ungenutzten Bits", key[0], 0x00u);
		T_EQ("65 Byte Punkt", key_len, 66u);
		/* UND ES IST WIRKLICH DER SCHLUESSEL AUS DEM STECKPLATZ. Ohne
		 * diesen Vergleich koennte das Zertifikat ueber irgendeinen
		 * Punkt sprechen und immer noch richtig aussehen. */
		T_TRUE("derselbe Punkt wie bei GENERATE",
		       memcmp(key + 1u, point, sizeof(point)) == 0);
	}

	T_TRUE("extensions", element(tbs, tbs_len, 7u, &tag, &whole, &whole_len, &val, &val_len));
	T_EQ("Feld [3]", tag, T_CTX3);
	{
		uint32_t t1 = 0;
		const uint8_t *exts = NULL;
		const uint8_t *one = NULL;
		size_t exts_len = 0;
		size_t one_len = 0;

		T_TRUE("Liste", element(val, val_len, 0u, &t1, &whole, &whole_len, &exts,
					&exts_len));
		T_EQ("SEQUENCE", t1, T_SEQ);

		T_TRUE("erste Erweiterung", element(exts, exts_len, 0u, &t1, &whole, &whole_len,
						    &one, &one_len));
		/* keyUsage (2.5.29.15), als kritisch markiert, und bei 9A
		 * digitalSignature - Bit 0, also 0x80 mit sieben ungenutzten
		 * Bits. */
		T_HEX("keyUsage, kritisch, digitalSignature", one, one_len,
		      "0603551d0f0101ff040403020780");

		T_TRUE("zweite Erweiterung", element(exts, exts_len, 1u, &t1, &whole, &whole_len,
						     &one, &one_len));
		T_HEX("basicConstraints, kritisch, keine CA", one, one_len,
		      "0603551d130101ff04023000");
		T_TRUE("keine dritte",
		       !element(exts, exts_len, 2u, &t1, &whole, &whole_len, &one, &one_len));
	}

	T_TRUE("kein neuntes Feld",
	       !element(tbs, tbs_len, 8u, &tag, &whole, &whole_len, &val, &val_len));
}

/* Keine PIN, kein Tastendruck - die Begruendung steht im Kopf von
 * osk_attest.h, und hier wird sie gemessen. */
static void test_no_release_needed(void)
{
	unsigned asked;

	mock_reset_all();
	osk_applets_reset();
	select_piv();
	verify_pin();
	T_EQ("9A anlegen", generate(OSK_PIV_SLOT_AUTH, OSK_PIV_ALG_P256), OSK_SW_OK);
	T_EQ("F9 anlegen", generate(OSK_PIV_SLOT_ATTEST, OSK_PIV_ALG_P256), OSK_SW_OK);

	/* Karte aus und wieder an: die Schluessel bleiben im Chip, die
	 * PIN-Freigabe faellt. */
	osk_applets_reset();
	select_piv();

	asked = mock_ui_confirm_count();
	T_EQ("Attestierung ohne PIN", attest(OSK_PIV_SLOT_AUTH), OSK_SW_OK);
	T_EQ("und ohne Frage am Display", mock_ui_confirm_count(), asked);

	/* Auch wenn der Nutzer am Display nein sagen WUERDE, wird nicht
	 * gefragt - es gibt nichts zu fragen. */
	mock_ui_set_confirm(OSK_ERR_DENIED);
	T_EQ("Nein am Display aendert nichts", attest(OSK_PIV_SLOT_AUTH), OSK_SW_OK);
	mock_ui_set_confirm(OSK_OK);

	/* Zum Vergleich: unterschreiben verlangt beides weiterhin. */
	{
		const size_t n = sign_cmd();

		(void)call(0x00u, 0x87u, OSK_PIV_ALG_P256, OSK_PIV_SLOT_AUTH, g_sign_cmd, n, 256);
		T_EQ("unterschreiben ohne PIN nicht", g_sw, OSK_SW_SECURITY);
	}
}

static void test_rejects(void)
{
	mock_reset_all();
	osk_applets_reset();
	select_piv();
	verify_pin();
	T_EQ("F9 anlegen", generate(OSK_PIV_SLOT_ATTEST, OSK_PIV_ALG_P256), OSK_SW_OK);

	/* Ein Steckplatz ohne Schluessel. */
	T_EQ("9C ist leer", attest(OSK_PIV_SLOT_SIGN), OSK_SW_REF_NOT_FOUND);
	/* Ein Steckplatz, den es nicht gibt. */
	T_EQ("8A gibt es nicht", attest(0x8au), OSK_SW_REF_NOT_FOUND);
	T_EQ("00 gibt es nicht", attest(0x00u), OSK_SW_REF_NOT_FOUND);
	/* F9 attestiert sich nicht selbst: eine Aussage ueber sich selbst
	 * beweist nichts. */
	T_EQ("F9 nicht ueber sich selbst", attest(OSK_PIV_SLOT_ATTEST), OSK_SW_REF_NOT_FOUND);

	/* Falsches P2, und Daten, wo keine hingehoeren. */
	(void)call(0x00u, OSK_PIV_INS_ATTEST, OSK_PIV_SLOT_AUTH, 0x01u, NULL, 0u, 256);
	T_EQ("P2 falsch", g_sw, OSK_SW_WRONG_P1P2);
	{
		const uint8_t junk[2] = {0x00u, 0x00u};

		(void)call(0x00u, OSK_PIV_INS_ATTEST, OSK_PIV_SLOT_AUTH, 0x00u, junk,
			   sizeof(junk), 256);
		T_EQ("Daten dabei", g_sw, OSK_SW_WRONG_LENGTH);
	}
}

/* Der Aussteller muss BYTEGLEICH der Inhaber des F9-Zertifikats sein,
 * sonst findet kein Pruefer die Kette. */
static void test_issuer_from_stored_cert(void)
{
	/* Ein kleines, DER-gerechtes Zertifikat von Hand. Es taugt nicht als
	 * Zertifikat - die Gueltigkeit ist leer -, aber genau das ist der
	 * Punkt: der Aussteller wird aus der STELLE geholt, an der die Norm
	 * ihn hat, und nicht daran erkannt, dass er plausibel aussieht. */
	static const char cert_hex[] = "3036"
				       "3034"
				       "a003020102"
				       "020101"
				       "300a06082a8648ce3d040302"
				       "300d310b3009060355040313024341"  /* issuer CN=CA */
				       "3000"
				       "300d310b3009060355040313024639"; /* subject CN=F9 */
	/* Der Inhaber des F9-Zertifikats, als ganzes Element. */
	static const char subject_hex[] = "300d310b3009060355040313024639";
	uint8_t cert[64];
	uint8_t want[32];
	uint8_t put[80];
	size_t cert_len;
	size_t want_len;
	size_t at = 0;
	uint32_t tag = 0;
	const uint8_t *whole = NULL;
	const uint8_t *val = NULL;
	const uint8_t *tbs = NULL;
	size_t whole_len = 0;
	size_t val_len = 0;
	size_t tbs_len = 0;

	cert_len = t_unhex(cert_hex, cert, sizeof(cert));
	want_len = t_unhex(subject_hex, want, sizeof(want));
	T_EQ("Zertifikat gebaut", cert_len, 56u);
	T_EQ("Inhaber gebaut", want_len, 15u);

	mock_reset_all();
	osk_applets_reset();
	select_piv();
	verify_pin();
	T_EQ("9A anlegen", generate(OSK_PIV_SLOT_AUTH, OSK_PIV_ALG_P256), OSK_SW_OK);
	T_EQ("F9 anlegen", generate(OSK_PIV_SLOT_ATTEST, OSK_PIV_ALG_P256), OSK_SW_OK);

	/* Das F9-Zertifikat ueber PUT DATA ablegen - Datenobjekt 5FFF01. */
	put[at++] = 0x5cu;
	put[at++] = 0x03u;
	put[at++] = 0x5fu;
	put[at++] = 0xffu;
	put[at++] = 0x01u;
	put[at++] = 0x53u;
	put[at++] = (uint8_t)cert_len;
	memcpy(put + at, cert, cert_len);
	at += cert_len;
	(void)call(0x00u, 0xdbu, 0x3fu, 0xffu, put, at, -1);
	T_EQ("F9-Zertifikat abgelegt", g_sw, OSK_SW_OK);

	/* Und es kommt auch wieder heraus. */
	{
		const uint8_t tagq[5] = {0x5cu, 0x03u, 0x5fu, 0xffu, 0x01u};
		const uint8_t *got = NULL;
		size_t got_len = 0;

		(void)call(0x00u, 0xcbu, 0x3fu, 0xffu, tagq, sizeof(tagq), 256);
		T_EQ("gelesen", collect(), OSK_SW_OK);
		T_TRUE("in 53 verpackt", osk_tlv_find(g_cert, g_cert_len, 0x53u, &got, &got_len));
		T_EQ("gleiche Laenge", got_len, cert_len);
		T_TRUE("gleicher Inhalt", memcmp(got, cert, cert_len) == 0);
	}

	T_EQ("Attestierung laeuft", attest(OSK_PIV_SLOT_AUTH), OSK_SW_OK);
	T_TRUE("aeussere Huelle",
	       element(g_cert, g_cert_len, 0u, &tag, &whole, &whole_len, &val, &val_len));
	T_TRUE("tbs", element(val, val_len, 0u, &tag, &whole, &whole_len, &tbs, &tbs_len));
	T_TRUE("issuer", element(tbs, tbs_len, 3u, &tag, &whole, &whole_len, &val, &val_len));
	T_EQ("gleiche Laenge wie der Inhaber des F9-Zertifikats", whole_len, want_len);
	T_TRUE("und bytegleich", memcmp(whole, want, want_len) == 0);
}

/* RSA im Steckplatz: der oeffentliche Teil geht als Modulus und Exponent
 * ins Zertifikat, und der Verwendungszweck ist ein anderer. */
static void test_rsa_slot(void)
{
	uint32_t tag = 0;
	const uint8_t *whole = NULL;
	const uint8_t *val = NULL;
	const uint8_t *tbs = NULL;
	size_t whole_len = 0;
	size_t val_len = 0;
	size_t tbs_len = 0;
	uint8_t mod[256];
	uint8_t serial_a[16];
	size_t serial_a_len = 0;

	mock_reset_all();
	osk_applets_reset();
	select_piv();
	verify_pin();
	T_EQ("F9 anlegen", generate(OSK_PIV_SLOT_ATTEST, OSK_PIV_ALG_P256), OSK_SW_OK);

	/* Erst eine Kurve in 9D, attestieren, Seriennummer merken. */
	T_EQ("9D als P-256", generate(OSK_PIV_SLOT_KEYMGMT, OSK_PIV_ALG_P256), OSK_SW_OK);
	T_EQ("attestiert", attest(OSK_PIV_SLOT_KEYMGMT), OSK_SW_OK);
	T_TRUE("Huelle", element(g_cert, g_cert_len, 0u, &tag, &whole, &whole_len, &val,
				 &val_len));
	T_TRUE("tbs", element(val, val_len, 0u, &tag, &whole, &whole_len, &tbs, &tbs_len));
	T_TRUE("serialNumber", element(tbs, tbs_len, 1u, &tag, &whole, &whole_len, &val,
				       &val_len));
	serial_a_len = val_len;
	memcpy(serial_a, val, val_len);
	/* Bei einer Kurve in 9D ist der Zweck keyAgreement - Bit 4, also
	 * 0x08 mit drei ungenutzten Bits. */
	T_TRUE("extensions", element(tbs, tbs_len, 7u, &tag, &whole, &whole_len, &val, &val_len));
	{
		const uint8_t *exts = NULL;
		const uint8_t *one = NULL;
		size_t exts_len = 0;
		size_t one_len = 0;
		uint32_t t1 = 0;

		T_TRUE("Liste", element(val, val_len, 0u, &t1, &whole, &whole_len, &exts,
					&exts_len));
		T_TRUE("keyUsage", element(exts, exts_len, 0u, &t1, &whole, &whole_len, &one,
					   &one_len));
		T_HEX("Kurve in 9D: keyAgreement", one, one_len, "0603551d0f0101ff040403020308");
	}

	if (!OSK_RSA_BUILT) {
		/* Ohne RSA bleibt es beim Kurvenschluessel - und die
		 * Attestierung darueber ist oben schon geprueft. */
		T_EQ("ohne CONFIG_OSK_RSA kein RSA in 9D",
		     generate(OSK_PIV_SLOT_KEYMGMT, OSK_PIV_ALG_RSA2048),
		     OSK_SW_FUNC_NOT_SUPPORTED);
		(void)mod;
		return;
	}

	/* Jetzt RSA in denselben Steckplatz. */
	T_EQ("9D als RSA-2048", generate(OSK_PIV_SLOT_KEYMGMT, OSK_PIV_ALG_RSA2048), OSK_SW_OK);
	{
		const uint8_t *inner = NULL;
		const uint8_t *m = NULL;
		const uint8_t *e = NULL;
		size_t inner_len = 0;
		size_t m_len = 0;
		size_t e_len = 0;

		T_TRUE("7F49", osk_tlv_find(g_cert, g_cert_len, 0x7f49u, &inner, &inner_len));
		T_TRUE("Modulus in 81", osk_tlv_find(inner, inner_len, 0x81u, &m, &m_len));
		T_TRUE("Exponent in 82", osk_tlv_find(inner, inner_len, 0x82u, &e, &e_len));
		T_EQ("256 Byte Modulus", m_len, 256u);
		T_EQ("Exponent drei Byte", e_len, 3u);
		memcpy(mod, m, m_len);
	}

	T_EQ("attestiert", attest(OSK_PIV_SLOT_KEYMGMT), OSK_SW_OK);
	T_TRUE("Huelle", element(g_cert, g_cert_len, 0u, &tag, &whole, &whole_len, &val,
				 &val_len));
	T_TRUE("tbs", element(val, val_len, 0u, &tag, &whole, &whole_len, &tbs, &tbs_len));

	/* Die Seriennummer des Zertifikats haengt am oeffentlichen
	 * Schluessel. Anderer Schluessel im selben Steckplatz, also andere
	 * Nummer - sonst gaebe es zwei Zertifikate eines Ausstellers mit
	 * derselben Nummer, und das verbietet RFC 5280 Abschnitt 4.1.2.2. */
	T_TRUE("serialNumber", element(tbs, tbs_len, 1u, &tag, &whole, &whole_len, &val,
				       &val_len));
	T_TRUE("andere Nummer als bei der Kurve",
	       val_len != serial_a_len || memcmp(val, serial_a, val_len) != 0);

	T_TRUE("subjectPublicKeyInfo", element(tbs, tbs_len, 6u, &tag, &whole, &whole_len, &val,
					       &val_len));
	{
		uint32_t t1 = 0;
		const uint8_t *alg = NULL;
		const uint8_t *key = NULL;
		const uint8_t *inner = NULL;
		size_t alg_len = 0;
		size_t key_len = 0;
		size_t inner_len = 0;

		T_TRUE("Algorithmus", element(val, val_len, 0u, &t1, &whole, &whole_len, &alg,
					      &alg_len));
		T_HEX("rsaEncryption mit NULL", alg, alg_len, "06092a864886f70d0101010500");
		T_TRUE("Schluessel", element(val, val_len, 1u, &t1, &whole, &whole_len, &key,
					     &key_len));
		T_EQ("BIT STRING", t1, T_BITSTRING);
		T_EQ("keine ungenutzten Bits", key[0], 0x00u);

		/* Darin: SEQUENCE { INTEGER Modulus, INTEGER Exponent }. */
		T_TRUE("RSAPublicKey", element(key + 1u, key_len - 1u, 0u, &t1, &whole,
					       &whole_len, &inner, &inner_len));
		T_EQ("SEQUENCE", t1, T_SEQ);
		T_TRUE("Modulus", element(inner, inner_len, 0u, &t1, &whole, &whole_len, &val,
					  &val_len));
		T_EQ("INTEGER", t1, T_INTEGER);
		/* Das oberste Bit des Modulus ist gesetzt, also MUSS eine
		 * fuehrende Null davor - sonst waere die Zahl negativ. Genau
		 * die Stelle, an der ein falsches Zertifikat entsteht. */
		T_EQ("257 Byte mit fuehrender Null", val_len, 257u);
		T_EQ("fuehrende Null", val[0], 0x00u);
		T_TRUE("und dann der Modulus", memcmp(val + 1u, mod, 256u) == 0);

		T_TRUE("Exponent", element(inner, inner_len, 1u, &t1, &whole, &whole_len, &val,
					   &val_len));
		T_HEX("65537, drei Byte", val, val_len, "010001");
	}

	/* RSA in 9D wickelt einen Schluessel ein, eine Kurve vereinbart
	 * einen - zwei Dinge, die beide "entschluesseln" heissen. */
	T_TRUE("extensions", element(tbs, tbs_len, 7u, &tag, &whole, &whole_len, &val, &val_len));
	{
		const uint8_t *exts = NULL;
		const uint8_t *one = NULL;
		size_t exts_len = 0;
		size_t one_len = 0;
		uint32_t t1 = 0;

		T_TRUE("Liste", element(val, val_len, 0u, &t1, &whole, &whole_len, &exts,
					&exts_len));
		T_TRUE("keyUsage", element(exts, exts_len, 0u, &t1, &whole, &whole_len, &one,
					   &one_len));
		T_HEX("RSA in 9D: keyEncipherment", one, one_len,
		      "0603551d0f0101ff040403020520");
	}
}

/* Die Freigaberegel steht zweimal: als Text im Zertifikat und als Ablauf
 * in osk_piv.c. Hier werden die zwei gegeneinander geprueft - eine
 * Beschreibung, die nur mit sich selbst uebereinstimmt, beschreibt
 * nichts. */
static void test_policy_matches_behaviour(void)
{
	uint32_t tag = 0;
	const uint8_t *whole = NULL;
	const uint8_t *val = NULL;
	const uint8_t *tbs = NULL;
	size_t whole_len = 0;
	size_t val_len = 0;
	size_t tbs_len = 0;

	mock_reset_all();
	osk_applets_reset();
	select_piv();
	verify_pin();
	T_EQ("F9", generate(OSK_PIV_SLOT_ATTEST, OSK_PIV_ALG_P256), OSK_SW_OK);
	T_EQ("9C", generate(OSK_PIV_SLOT_SIGN, OSK_PIV_ALG_P256), OSK_SW_OK);
	T_EQ("9E", generate(OSK_PIV_SLOT_CARD, OSK_PIV_ALG_P256), OSK_SW_OK);

	/* 9E sagt "pin=no" - und unterschreibt wirklich ohne PIN. */
	T_EQ("9E attestiert", attest(OSK_PIV_SLOT_CARD), OSK_SW_OK);
	T_TRUE("Huelle", element(g_cert, g_cert_len, 0u, &tag, &whole, &whole_len, &val,
				 &val_len));
	T_TRUE("tbs", element(val, val_len, 0u, &tag, &whole, &whole_len, &tbs, &tbs_len));
	T_TRUE("subject", element(tbs, tbs_len, 5u, &tag, &whole, &whole_len, &val, &val_len));
	T_TRUE("9E sagt pin=no", name_has(val, val_len, "pin=no touch=yes"));

	osk_applets_reset();
	select_piv();
	{
		const size_t n = sign_cmd();

		(void)call(0x00u, 0x87u, OSK_PIV_ALG_P256, OSK_PIV_SLOT_CARD, g_sign_cmd, n, 256);
		T_EQ("9E unterschreibt ohne PIN", g_sw, OSK_SW_OK);
	}

	/* 9C sagt "pin=always" - und eine zweite Unterschrift ohne frisches
	 * VERIFY wird wirklich abgelehnt. */
	verify_pin();
	T_EQ("9C attestiert", attest(OSK_PIV_SLOT_SIGN), OSK_SW_OK);
	T_TRUE("Huelle", element(g_cert, g_cert_len, 0u, &tag, &whole, &whole_len, &val,
				 &val_len));
	T_TRUE("tbs", element(val, val_len, 0u, &tag, &whole, &whole_len, &tbs, &tbs_len));
	T_TRUE("subject", element(tbs, tbs_len, 5u, &tag, &whole, &whole_len, &val, &val_len));
	T_TRUE("9C sagt pin=always", name_has(val, val_len, "pin=always touch=yes"));

	verify_pin();
	{
		const size_t n = sign_cmd();

		(void)call(0x00u, 0x87u, OSK_PIV_ALG_P256, OSK_PIV_SLOT_SIGN, g_sign_cmd, n, 256);
		T_EQ("erste Unterschrift", g_sw, OSK_SW_OK);
		(void)sign_cmd();
		(void)call(0x00u, 0x87u, OSK_PIV_ALG_P256, OSK_PIV_SLOT_SIGN, g_sign_cmd, n, 256);
		T_EQ("zweite ohne frisches VERIFY nicht", g_sw, OSK_SW_SECURITY);
	}
}

/* Ein Chip ohne RSA: die Attestierung muss weiter gehen, nur eben ohne
 * RSA-Steckplaetze. */
static void test_no_rsa_chip(void)
{
	mock_reset_all();
	mock_se050_set_rsa_mask(0u);
	osk_applets_reset();
	select_piv();
	verify_pin();

	T_EQ("F9 geht weiter", generate(OSK_PIV_SLOT_ATTEST, OSK_PIV_ALG_P256), OSK_SW_OK);
	T_EQ("9A geht weiter", generate(OSK_PIV_SLOT_AUTH, OSK_PIV_ALG_P256), OSK_SW_OK);
	T_EQ("attestieren geht weiter", attest(OSK_PIV_SLOT_AUTH), OSK_SW_OK);

	/* Und ein RSA-Schluessel entsteht gar nicht erst - mit einem
	 * Statuswort, nicht mit einem Absturz. */
	T_EQ("RSA abgelehnt", generate(OSK_PIV_SLOT_SIGN, OSK_PIV_ALG_RSA2048),
	     OSK_SW_FUNC_NOT_SUPPORTED);
	T_TRUE("und kein Schluessel entstanden",
	       !mock_se050_has_rsa_key(OSK_PIV_KEY_ID(OSK_PIV_SLOT_SIGN)));
	mock_reset_all();
}

int main(void)
{
	setvbuf(stdout, NULL, _IONBF, 0);
	mock_reset_all();
	T_EQ("Anwendungen bereit", osk_applets_init(), OSK_OK);

	test_cert_shape();
	test_no_release_needed();
	test_rejects();
	test_issuer_from_stored_cert();
	test_rsa_slot();
	test_policy_matches_behaviour();
	test_no_rsa_chip();
	return t_report("test_attest");
}
