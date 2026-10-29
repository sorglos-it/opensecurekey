/* OpenPGP-Karte. Siehe osk_openpgp.h.
 * Spezifikation: OpenPGP-Karte 3.4, Abschnitte 4.2 (Auswahl), 4.3 (PINs),
 * 4.4 (Datenobjekte), 7.2 (Kommandos). */
#include <string.h>
#include "osk_openpgp.h"
#include "osk_card.h"
#include "osk_apdu.h"
#include "osk_se050.h"
#include "osk_store.h"
#include "osk_ui.h"
#include "osk_rsa_card.h"
#include "osk_serial.h"

/* Kommandos (Abschnitt 7.2). */
#define INS_SELECT 0xa4u
#define INS_VERIFY 0x20u
#define INS_CHANGE_REFERENCE 0x24u
#define INS_RESET_RETRY 0x2cu
#define INS_GET_DATA 0xcau
#define INS_PUT_DATA 0xdau
#define INS_GENERATE_ASYM 0x47u
#define INS_PSO 0x2au
#define INS_INTERNAL_AUTH 0x88u
#define INS_TERMINATE 0xe6u
#define INS_ACTIVATE 0x44u

/* Kennungen der Passwoerter (Abschnitt 4.3). */
#define PW1_SIGN 0x81u
#define PW1_OTHER 0x82u
#define PW3 0x83u

#define FIELD_MAX 64u
#define FPR_LEN 20u
#define PW1_LEN_DEFAULT 6u

/* Der Zustand, der einen Stromausfall ueberleben muss. Er ist nicht geheim -
 * Name, Anmeldename, Fingerabdruecke, Zaehler -, liegt aber im Tresor, weil
 * es den einzigen dauerhaften Speicher gibt, den diese Schnittstelle
 * anbietet. Gelesen wird einmal je Einschaltvorgang (eine Frage am
 * Display), geschrieben nach jeder Aenderung. */
typedef struct {
	uint8_t sex;
	uint8_t pw1_len;    /* Laenge der gueltigen PW1 - zum Teilen bei CHANGE */
	uint8_t pw1_mode;   /* 0 = PW1 gilt fuer eine Unterschrift, 1 = mehrere */
	uint32_t sig_count; /* Zaehler aus DO 93 */
	/* Ein Byte mehr als die Hoechstlaenge: so endet jedes Textfeld auf
	 * eine Null und darf als Zeichenkette ans Display gehen. */
	uint8_t name_len;
	uint8_t name[FIELD_MAX + 1u];
	uint8_t lang_len;
	uint8_t lang[8 + 1u];
	uint8_t login_len;
	uint8_t login[FIELD_MAX + 1u];
	uint8_t url_len;
	uint8_t url[FIELD_MAX + 1u];
	uint8_t fpr[3][FPR_LEN];    /* C7, C8, C9 */
	uint8_t ca_fpr[3][FPR_LEN]; /* CA-Fingerabdruecke C8..CA in DO C6 */
	uint8_t gen_time[3][4];	    /* Erzeugungszeit, DO CD */
	uint8_t curve[3];	    /* 0 = kein Kurvenschluessel, 1 = P-256, 2 = P-384 */
	/* RSA je Schluessel: 0 = kein RSA-Schluessel, sonst die Bitlaenge.
	 * curve[] und rsa_bits[] sind nie beide gesetzt - in einem
	 * Schluesselplatz liegt genau ein Schluessel.
	 *
	 * Die Exponentenlaenge wird MITGEFUEHRT, obwohl man sie fuer 65537
	 * auch hinschreiben koennte: der Vertrag warnt ausdruecklich, dass
	 * der Exponent je Chip und Schluessel drei oder vier Byte lang ist
	 * und wer 65537 annimmt irgendwann ein falsches Zertifikat baut
	 * (osk_se050.h). Gemeldet wird deshalb, was das Kartenprogramm
	 * verlangt hat, bis ein Schluessel da ist - danach, was der Chip
	 * wirklich hat. */
	uint16_t rsa_bits[3];
	uint16_t rsa_ebits[3];
} pgp_state_t;

static pgp_state_t g_st;
static bool g_loaded;
static bool g_terminated;

/* Freigaben - fluechtig, fallen beim Ein- und Ausschalten. */
static bool g_pw1_sign;
static bool g_pw1_other;
static bool g_pw3;

/* Die Kennung. Die vier Bytes der Seriennummer kommen aus dem
 * Sicherheitschip (osk_serial.h) und sind je Geraet verschieden - bis zum
 * 29.10.2026 stand hier auf jedem Geraet dieselbe, und GnuPG unterscheidet
 * Karten genau daran: zwei unserer Sticks an einem Rechner sahen wie eine
 * Karte aus (stand als Luecke in docs/smartcard.md).
 *
 * Antwortet der Chip nicht, bleibt das Feld auf Null. Das ist kein
 * Ersatzwert, sondern eine sichtbare Aussage: "keine Nummer". Eine
 * erfundene waere wieder auf allen Geraeten dieselbe. */
static uint8_t g_aid[OSK_PGP_AID_LEN] = {
    0xd2, 0x76, 0x00, 0x01, 0x24, 0x01, /* RID und Anwendung */
    0x03, 0x04,				/* Fassung 3.4 */
    0xff, 0xfe,				/* Hersteller: keiner, Selbstbau */
    0x00, 0x00, 0x00, 0x00,		/* Seriennummer - siehe oben */
    0x00, 0x00,
};

/* Der Platz der Seriennummer in der Kennung: Abschnitt 4.2.1 zaehlt RID
 * (5), Anwendung (1), Fassung (2), Hersteller (2), dann vier Bytes
 * Seriennummer. */
#define AID_SERIAL_AT 10u

static void aid_fill_serial(void)
{
	uint8_t sn[OSK_SERIAL_SHORT_LEN];

	if (osk_serial_short(sn) != OSK_OK)
		return;
	memcpy(g_aid + AID_SERIAL_AT, sn, sizeof(sn));
}

const uint8_t *osk_openpgp_aid(void)
{
	aid_fill_serial();
	return g_aid;
}

/* --- Zustand lesen und schreiben ---------------------------------------- */

#define STATE_TAG "PGP-STATE"
#define STATE_NAME "OpenPGP Kartendaten"
/* Fassung 2 seit dem 29.10.2026: die beiden RSA-Felder sind
 * dazugekommen. Fassung 1 wird beim Lesen noch angenommen und bedeutet
 * "kein RSA-Schluessel" - ein Datensatz aus einer aelteren Firmware darf
 * nicht dazu fuehren, dass die Karte auf Anfang springt. */
#define STATE_VERSION 2u

/* Der Zustand wird Feld fuer Feld geschrieben, nicht als Abbild der
 * Struktur: ein anderer Uebersetzer legt Strukturen anders aus. */
static size_t state_pack(const pgp_state_t *s, uint8_t *out, size_t cap)
{
	size_t at = 0;

	if (cap < 1024u)
		return 0u;
	out[at++] = (uint8_t)STATE_VERSION;
	out[at++] = s->sex;
	out[at++] = s->pw1_len;
	out[at++] = s->pw1_mode;
	out[at++] = (uint8_t)(s->sig_count >> 24);
	out[at++] = (uint8_t)(s->sig_count >> 16);
	out[at++] = (uint8_t)(s->sig_count >> 8);
	out[at++] = (uint8_t)s->sig_count;

	out[at++] = s->name_len;
	memcpy(out + at, s->name, FIELD_MAX);
	at += FIELD_MAX;
	out[at++] = s->lang_len;
	memcpy(out + at, s->lang, 8u);
	at += 8u;
	out[at++] = s->login_len;
	memcpy(out + at, s->login, FIELD_MAX);
	at += FIELD_MAX;
	out[at++] = s->url_len;
	memcpy(out + at, s->url, FIELD_MAX);
	at += FIELD_MAX;

	memcpy(out + at, s->fpr, sizeof(s->fpr));
	at += sizeof(s->fpr);
	memcpy(out + at, s->ca_fpr, sizeof(s->ca_fpr));
	at += sizeof(s->ca_fpr);
	memcpy(out + at, s->gen_time, sizeof(s->gen_time));
	at += sizeof(s->gen_time);
	memcpy(out + at, s->curve, sizeof(s->curve));
	at += sizeof(s->curve);
	for (unsigned k = 0; k < 3u; k++) {
		out[at++] = (uint8_t)(s->rsa_bits[k] >> 8);
		out[at++] = (uint8_t)s->rsa_bits[k];
	}
	for (unsigned k = 0; k < 3u; k++) {
		out[at++] = (uint8_t)(s->rsa_ebits[k] >> 8);
		out[at++] = (uint8_t)s->rsa_ebits[k];
	}
	return at;
}

static bool state_unpack(const uint8_t *in, size_t len, pgp_state_t *s)
{
	size_t at = 0;

	if (in == NULL || len < 8u)
		return false;
	if (in[0] != 1u && in[0] != 2u)
		return false;
	/* Wieder Feld fuer Feld - und nur, wenn jedes Feld wirklich da ist. */
	if (len <
	    (size_t)(8u + 1u + FIELD_MAX + 1u + 8u + 1u + FIELD_MAX + 1u + FIELD_MAX +
		     sizeof(s->fpr) + sizeof(s->ca_fpr) + sizeof(s->gen_time) + sizeof(s->curve)))
		return false;
	at = 1u;
	s->sex = in[at++];
	s->pw1_len = in[at++];
	s->pw1_mode = in[at++];
	s->sig_count = ((uint32_t)in[at] << 24) | ((uint32_t)in[at + 1u] << 16) |
		       ((uint32_t)in[at + 2u] << 8) | (uint32_t)in[at + 3u];
	at += 4u;

	s->name_len = in[at++];
	memcpy(s->name, in + at, FIELD_MAX);
	at += FIELD_MAX;
	s->lang_len = in[at++];
	memcpy(s->lang, in + at, 8u);
	at += 8u;
	s->login_len = in[at++];
	memcpy(s->login, in + at, FIELD_MAX);
	at += FIELD_MAX;
	s->url_len = in[at++];
	memcpy(s->url, in + at, FIELD_MAX);
	at += FIELD_MAX;

	memcpy(s->fpr, in + at, sizeof(s->fpr));
	at += sizeof(s->fpr);
	memcpy(s->ca_fpr, in + at, sizeof(s->ca_fpr));
	at += sizeof(s->ca_fpr);
	memcpy(s->gen_time, in + at, sizeof(s->gen_time));
	at += sizeof(s->gen_time);
	memcpy(s->curve, in + at, sizeof(s->curve));
	at += sizeof(s->curve);

	memset(s->rsa_bits, 0, sizeof(s->rsa_bits));
	memset(s->rsa_ebits, 0, sizeof(s->rsa_ebits));
	if (in[0] >= 2u) {
		/* Erst pruefen, dass die zwoelf Bytes wirklich da sind, dann
		 * lesen - nie umgekehrt. */
		if (len < at + 12u)
			return false;
		for (unsigned k = 0; k < 3u; k++) {
			s->rsa_bits[k] = (uint16_t)(((uint16_t)in[at] << 8) | in[at + 1u]);
			at += 2u;
		}
		for (unsigned k = 0; k < 3u; k++) {
			s->rsa_ebits[k] = (uint16_t)(((uint16_t)in[at] << 8) | in[at + 1u]);
			at += 2u;
		}
		/* Eine Laenge, die es nicht gibt, heisst "kein RSA" - nicht
		 * "die naechstgelegene". Der Datensatz kommt aus dem Flash. */
		for (unsigned k = 0; k < 3u; k++) {
			if (s->rsa_bits[k] != 0u && !osk_rsa_bits_enum(s->rsa_bits[k], NULL))
				return false;
			if (s->rsa_bits[k] != 0u && s->curve[k] != 0u)
				return false;
		}
	}

	/* Laengen koennen aus einem beschaedigten Datensatz kommen. */
	if (s->name_len > FIELD_MAX || s->login_len > FIELD_MAX || s->url_len > FIELD_MAX ||
	    s->lang_len > 8u)
		return false;
	/* Die Textfelder enden immer auf eine Null, egal was im Flash stand. */
	s->name[FIELD_MAX] = 0u;
	s->lang[8] = 0u;
	s->login[FIELD_MAX] = 0u;
	s->url[FIELD_MAX] = 0u;
	if (s->pw1_len > OSK_CARD_SECRET_MAX)
		return false;
	return true;
}

static void state_defaults(pgp_state_t *s)
{
	memset(s, 0, sizeof(*s));
	s->pw1_len = PW1_LEN_DEFAULT;
	s->pw1_mode = 0u;
}

static void state_save(void)
{
	uint8_t buf[1024];
	size_t len = state_pack(&g_st, buf, sizeof(buf));

	if (len == 0u)
		return;
	(void)osk_card_obj_write(OSK_ITEM_NOTE, STATE_TAG, STATE_NAME, buf, len);
	osk_card_wipe(buf, sizeof(buf));
}

/* Laedt beim ersten Zugriff. Fehlt der Datensatz, gilt der Anfangszustand -
 * und der wird erst beim ersten Schreiben wirklich angelegt. */
static void state_load(void)
{
	uint8_t buf[1024];
	size_t len = 0;

	if (g_loaded)
		return;
	g_loaded = true;
	state_defaults(&g_st);
	if (osk_card_obj_read(OSK_ITEM_NOTE, STATE_TAG, STATE_NAME, buf, sizeof(buf), &len) !=
	    OSK_OK)
		return;
	if (!state_unpack(buf, len, &g_st))
		state_defaults(&g_st);
	osk_card_wipe(buf, sizeof(buf));
}

static void pgp_reset(void)
{
	g_pw1_sign = false;
	g_pw1_other = false;
	g_pw3 = false;
	g_loaded = false;
	g_terminated = false;
	state_defaults(&g_st);
	/* Die Antwort "kann der Chip RSA" und die Seriennummer werden beim
	 * naechsten Einschalten neu geholt und nicht ueber einen
	 * Kartenwechsel hinweg geglaubt. */
	osk_rsa_forget();
	osk_serial_forget();
}

/* --- Kurven ------------------------------------------------------------- */

/* Die Objektkennungen der Kurven (Abschnitt 4.4.3.9, RFC 6637) standen
 * hier zweimal im Repo, seit die Attestierung sie auch braucht. Jetzt
 * stehen sie in osk_card.h - an einer Stelle. */

/* In einem Schluesselplatz liegt genau ein Schluessel - eine Kurve oder
 * ein RSA. Diese zwei Fragen werden ueberall ueber diese zwei Funktionen
 * gestellt und nirgends durch einen Vergleich mit 0 ersetzt: sonst heisst
 * "curve == 0" an einer Stelle "kein Schluessel" und an einer anderen
 * "RSA", und das ist genau die Verwechslung, die dieses Projekt siebenmal
 * erwischt hat. */
static bool key_is_rsa(unsigned key)
{
	return g_st.rsa_bits[key] != 0u;
}

static bool key_present(unsigned key)
{
	return g_st.curve[key] != 0u || key_is_rsa(key);
}

static size_t curve_half(uint8_t curve)
{
	return (curve == 2u) ? 48u : 32u;
}

static osk_curve_t curve_to_se050(uint8_t curve)
{
	return (curve == 2u) ? OSK_CURVE_P384 : OSK_CURVE_P256;
}

static const uint8_t *curve_oid(uint8_t curve, size_t *len_out)
{
	if (curve == 2u) {
		*len_out = sizeof(osk_oid_p384);
		return osk_oid_p384;
	}
	*len_out = sizeof(osk_oid_p256);
	return osk_oid_p256;
}

/* Algorithmus-Eigenschaften eines Schluessels: Algorithmuskennung und die
 * Objektkennung der Kurve (DO C1 bis C3). */
static void put_alg_attr(osk_resp_t *res, uint32_t tag, unsigned key)
{
	uint8_t buf[16];
	size_t oid_len = 0;
	const uint8_t *oid;
	uint8_t curve;

	if (key_is_rsa(key)) {
		/* Aufbau nach Abschnitt 4.4.3.9: Kennung 01, Laenge des
		 * Modulus in Bit, Laenge des Exponenten in Bit, Einfuhrform.
		 * Die Form 00 ist die uebliche (e, p, q); eingefuehrt wird
		 * bei uns ueberhaupt nichts, der Schluessel entsteht im Chip
		 * und kommt nie heraus. */
		uint16_t ebits = g_st.rsa_ebits[key];
		uint8_t exp[8];
		uint8_t mod[OSK_RSA_MOD_MAX];
		size_t mod_len = 0;
		size_t exp_len = 0;

		/* Liegt ein Schluessel, gilt die Laenge, die der CHIP meldet,
		 * und nicht die, die ein Kartenprogramm einmal verlangt hat.
		 * Der Vertrag warnt genau davor (osk_se050.h). */
		if (osk_rsa_public_parts(OSK_PGP_KEY_ID(key), mod, sizeof(mod), &mod_len, exp,
					 sizeof(exp), &exp_len) == OSK_OK &&
		    exp_len > 0u) {
			unsigned top = 0;

			while (top < exp_len && exp[top] == 0x00u)
				top++;
			if (top < exp_len) {
				unsigned bits = (unsigned)(exp_len - top) * 8u;
				uint8_t b = exp[top];

				while ((b & 0x80u) == 0u) {
					bits--;
					b = (uint8_t)(b << 1);
				}
				ebits = (uint16_t)bits;
			}
		}
		if (ebits == 0u)
			ebits = 17u; /* 65537 ist 2^16+1, also 17 Bit */

		buf[0] = (uint8_t)OSK_PGP_ALG_RSA;
		buf[1] = (uint8_t)(g_st.rsa_bits[key] >> 8);
		buf[2] = (uint8_t)g_st.rsa_bits[key];
		buf[3] = (uint8_t)(ebits >> 8);
		buf[4] = (uint8_t)ebits;
		buf[5] = 0x00u;
		osk_resp_tlv(res, tag, buf, (size_t)6u);
		return;
	}

	curve = g_st.curve[key];
	if (curve == 0u)
		curve = 1u; /* noch kein Schluessel: wir bieten P-256 an */
	oid = curve_oid(curve, &oid_len);
	buf[0] = (key == OSK_PGP_KEY_DEC) ? (uint8_t)OSK_PGP_ALG_ECDH : (uint8_t)OSK_PGP_ALG_ECDSA;
	memcpy(buf + 1, oid, oid_len);
	osk_resp_tlv(res, tag, buf, oid_len + 1u);
}

/* --- Datenobjekte lesen ------------------------------------------------- */

static void put_pw_status(osk_resp_t *res)
{
	uint8_t b[7];

	b[0] = g_st.pw1_mode;
	b[1] = (uint8_t)OSK_CARD_SECRET_MAX; /* Hoechstlaenge PW1 */
	b[2] = (uint8_t)OSK_CARD_SECRET_MAX; /* Hoechstlaenge Resetting Code */
	b[3] = (uint8_t)OSK_CARD_SECRET_MAX; /* Hoechstlaenge PW3 */
	{
		uint8_t tries = 0;

		if (osk_se050_pin_tries_left(&tries) != OSK_OK)
			tries = 0u;
		b[4] = tries;
	}
	b[5] = osk_card_admin_tries(OSK_CARD_ADMIN_PGP_RC);
	b[6] = osk_card_admin_tries(OSK_CARD_ADMIN_PGP_PW3);
	osk_resp_tlv(res, 0xc4u, b, sizeof(b));
}

static void put_cardholder(osk_resp_t *res)
{
	osk_tlv_mark_t m;

	osk_resp_tlv_begin(res, 0x65u, &m);
	osk_resp_tlv(res, 0x5bu, g_st.name, g_st.name_len);
	osk_resp_tlv(res, 0x5f2du, g_st.lang, g_st.lang_len);
	osk_resp_tlv(res, 0x5f35u, &g_st.sex, 1u);
	osk_resp_tlv_end(res, &m);
}

/* Historische Bytes (Abschnitt 6.1): Kategorie 0x00, Kartendienste,
 * Faehigkeiten, dann der Status 9000 am Ende. */
static const uint8_t g_hist[] = {0x00, 0x73, 0x00, 0x00, 0x80, 0x05, 0x90, 0x00};

/* Erweiterte Faehigkeiten (Abschnitt 4.4.3.7), 10 Bytes.
 * Bit 4: PW-Status aenderbar, Bit 3: private Datenobjekte,
 * Bit 2: Algorithmus-Eigenschaften aenderbar. */
static const uint8_t g_ext_caps[] = {
    0x14,		      /* PW-Status aenderbar, Algorithmus aenderbar */
    0x00,		      /* keine sichere Nachrichtenuebermittlung */
    0x00, 0x00,		      /* GET CHALLENGE: nicht vorhanden */
    0x00, 0x00,		      /* kein Platz fuer ein Kartenzertifikat */
    0x00, (uint8_t)FIELD_MAX, /* Hoechstlaenge der besonderen Objekte */
    0x00,		      /* PIN-Block-2-Format: nein */
    0x00,		      /* MSE: nein */
};

static void put_app_related(osk_resp_t *res)
{
	osk_tlv_mark_t outer;
	osk_tlv_mark_t inner;
	uint8_t fpr_all[3u * FPR_LEN];
	uint8_t gen_all[12];

	memcpy(fpr_all, g_st.fpr, sizeof(fpr_all));
	memcpy(gen_all, g_st.gen_time, sizeof(gen_all));

	osk_resp_tlv_begin(res, 0x6eu, &outer);
	osk_resp_tlv(res, 0x4fu, g_aid, OSK_PGP_AID_LEN);
	osk_resp_tlv(res, 0x5f52u, g_hist, sizeof(g_hist));
	osk_resp_tlv_begin(res, 0x73u, &inner);
	osk_resp_tlv(res, 0xc0u, g_ext_caps, sizeof(g_ext_caps));
	put_alg_attr(res, 0xc1u, OSK_PGP_KEY_SIGN);
	put_alg_attr(res, 0xc2u, OSK_PGP_KEY_DEC);
	put_alg_attr(res, 0xc3u, OSK_PGP_KEY_AUTH);
	put_pw_status(res);
	osk_resp_tlv(res, 0xc5u, fpr_all, sizeof(fpr_all));
	{
		uint8_t ca_all[3u * FPR_LEN];

		memcpy(ca_all, g_st.ca_fpr, sizeof(ca_all));
		osk_resp_tlv(res, 0xc6u, ca_all, sizeof(ca_all));
	}
	osk_resp_tlv(res, 0xcdu, gen_all, sizeof(gen_all));
	osk_resp_tlv_end(res, &inner);
	osk_resp_tlv_end(res, &outer);
}

static void put_security_template(osk_resp_t *res)
{
	osk_tlv_mark_t m;
	uint8_t c[3];

	/* DO 93: Zaehler der Unterschriften, drei Bytes, grosses Ende zuerst. */
	c[0] = (uint8_t)(g_st.sig_count >> 16);
	c[1] = (uint8_t)(g_st.sig_count >> 8);
	c[2] = (uint8_t)g_st.sig_count;
	osk_resp_tlv_begin(res, 0x7au, &m);
	osk_resp_tlv(res, 0x93u, c, sizeof(c));
	osk_resp_tlv_end(res, &m);
}

static uint16_t cmd_get_data(const osk_apdu_t *a, osk_resp_t *res)
{
	const uint32_t tag = (uint32_t)(((uint32_t)a->p1 << 8) | (uint32_t)a->p2);

	state_load();
	switch (tag) {
	case 0x004fu:
		osk_resp_bytes(res, g_aid, OSK_PGP_AID_LEN);
		return OSK_SW_OK;
	case 0x005eu:
		osk_resp_bytes(res, g_st.login, g_st.login_len);
		return OSK_SW_OK;
	case 0x5f50u:
		osk_resp_bytes(res, g_st.url, g_st.url_len);
		return OSK_SW_OK;
	case 0x5f52u:
		osk_resp_bytes(res, g_hist, sizeof(g_hist));
		return OSK_SW_OK;
	case 0x0065u:
		put_cardholder(res);
		return OSK_SW_OK;
	case 0x006eu:
		put_app_related(res);
		return OSK_SW_OK;
	case 0x007au:
		put_security_template(res);
		return OSK_SW_OK;
	case 0x00c4u:
		/* Einzeln abgefragt kommt der Wert ohne Huelle. */
		{
			osk_resp_t tmp;
			uint8_t buf[32];
			const uint8_t *val = NULL;
			size_t len = 0;

			osk_resp_init(&tmp, buf, sizeof(buf));
			put_pw_status(&tmp);
			if (tmp.overflow || !osk_tlv_find(tmp.buf, tmp.len, 0xc4u, &val, &len))
				return OSK_SW_UNKNOWN;
			osk_resp_bytes(res, val, len);
		}
		return OSK_SW_OK;
	case 0x00c0u:
		osk_resp_bytes(res, g_ext_caps, sizeof(g_ext_caps));
		return OSK_SW_OK;
	default:
		return OSK_SW_REF_NOT_FOUND;
	}
}

/* --- Datenobjekte schreiben --------------------------------------------- */

static uint16_t put_field(const osk_apdu_t *a, uint8_t *field, uint8_t *len_field, size_t cap)
{
	/* `cap` ist die Nutzlaenge; ein Byte dahinter bleibt fuer die Null. */
	if (a->lc > cap)
		return OSK_SW_WRONG_LENGTH;
	memset(field, 0, cap + 1u);
	if (a->lc != 0u)
		memcpy(field, a->data, a->lc);
	*len_field = (uint8_t)a->lc;
	return OSK_SW_OK;
}

static uint16_t cmd_put_data(const osk_apdu_t *a)
{
	const uint32_t tag = (uint32_t)(((uint32_t)a->p1 << 8) | (uint32_t)a->p2);
	uint16_t sw = OSK_SW_OK;

	state_load();
	/* Alles Schreiben braucht die Admin-PIN (Abschnitt 7.2.8). */
	if (!g_pw3)
		return OSK_SW_SECURITY;

	switch (tag) {
	case 0x005bu:
		sw = put_field(a, g_st.name, &g_st.name_len, FIELD_MAX);
		break;
	case 0x5f2du:
		sw = put_field(a, g_st.lang, &g_st.lang_len, 8u);
		break;
	case 0x5f35u:
		if (a->lc != 1u)
			return OSK_SW_WRONG_LENGTH;
		g_st.sex = a->data[0];
		break;
	case 0x005eu:
		sw = put_field(a, g_st.login, &g_st.login_len, FIELD_MAX);
		break;
	case 0x5f50u:
		sw = put_field(a, g_st.url, &g_st.url_len, FIELD_MAX);
		break;
	case 0x00c7u:
	case 0x00c8u:
	case 0x00c9u:
		if (a->lc != FPR_LEN)
			return OSK_SW_WRONG_LENGTH;
		memcpy(g_st.fpr[tag - 0x00c7u], a->data, FPR_LEN);
		break;
	case 0x00cau:
	case 0x00cbu:
	case 0x00ccu:
		if (a->lc != FPR_LEN)
			return OSK_SW_WRONG_LENGTH;
		memcpy(g_st.ca_fpr[tag - 0x00cau], a->data, FPR_LEN);
		break;
	case 0x00ceu:
		if (a->lc != 12u)
			return OSK_SW_WRONG_LENGTH;
		memcpy(g_st.gen_time, a->data, 12u);
		break;
	case 0x00c4u:
		if (a->lc != 1u)
			return OSK_SW_WRONG_LENGTH;
		if (a->data[0] != 0x00u && a->data[0] != 0x01u)
			return OSK_SW_WRONG_DATA;
		g_st.pw1_mode = a->data[0];
		break;
	case 0x00d3u:
		/* Resetting Code: Abschnitt 4.3.4. Leer heisst "entfernen",
		 * und das koennen wir nicht - ein Tresoreintrag bleibt. */
		if (a->lc == 0u)
			return OSK_SW_FUNC_NOT_SUPPORTED;
		return osk_card_admin_change(OSK_CARD_ADMIN_PGP_RC, NULL, 0u, a->data, a->lc);
	case 0x00c1u:
	case 0x00c2u:
	case 0x00c3u: {
		/* Algorithmus-Eigenschaften: wir nehmen nur unsere zwei Kurven. */
		const unsigned key = (unsigned)(tag - 0x00c1u);

		if (a->lc < 2u)
			return OSK_SW_WRONG_LENGTH;
		if (a->data[0] == (uint8_t)OSK_PGP_ALG_RSA) {
			/* 01 || Modulus-Bits (2) || Exponent-Bits (2) ||
			 * Einfuhrform (1), Abschnitt 4.4.3.9. */
			uint16_t nbits;

			if (!osk_rsa_available())
				return OSK_SW_FUNC_NOT_SUPPORTED;
			if (a->lc != 6u)
				return OSK_SW_WRONG_LENGTH;
			nbits = (uint16_t)(((uint16_t)a->data[1] << 8) | a->data[2]);
			if (!osk_rsa_bits_ok(nbits))
				return OSK_SW_FUNC_NOT_SUPPORTED;
			g_st.rsa_bits[key] = nbits;
			g_st.rsa_ebits[key] =
			    (uint16_t)(((uint16_t)a->data[3] << 8) | a->data[4]);
			/* Ein Platz haelt genau einen Schluessel. */
			g_st.curve[key] = 0u;
			break;
		}
		if (a->lc - 1u == sizeof(osk_oid_p256) &&
		    memcmp(a->data + 1, osk_oid_p256, sizeof(osk_oid_p256)) == 0) {
			g_st.curve[key] = 1u;
			g_st.rsa_bits[key] = 0u;
		} else if (a->lc - 1u == sizeof(osk_oid_p384) &&
			   memcmp(a->data + 1, osk_oid_p384, sizeof(osk_oid_p384)) == 0) {
			g_st.curve[key] = 2u;
			g_st.rsa_bits[key] = 0u;
		} else {
			return OSK_SW_FUNC_NOT_SUPPORTED;
		}
		break;
	}
	default:
		return OSK_SW_REF_NOT_FOUND;
	}

	if (sw != OSK_SW_OK)
		return sw;
	state_save();
	return OSK_SW_OK;
}

/* --- PINs --------------------------------------------------------------- */

static uint16_t cmd_verify(const osk_apdu_t *a)
{
	uint16_t sw;

	if (a->p1 == 0xffu) {
		/* Freigabe zuruecknehmen (Abschnitt 7.2.2). */
		if (a->lc != 0u)
			return OSK_SW_WRONG_LENGTH;
		switch (a->p2) {
		case PW1_SIGN:
			g_pw1_sign = false;
			return OSK_SW_OK;
		case PW1_OTHER:
			g_pw1_other = false;
			return OSK_SW_OK;
		case PW3:
			g_pw3 = false;
			return OSK_SW_OK;
		default:
			return OSK_SW_REF_NOT_FOUND;
		}
	}
	if (a->p1 != 0x00u)
		return OSK_SW_WRONG_P1P2;

	state_load();

	if (a->lc == 0u) {
		/* Nachfrage nach dem Zustand. */
		switch (a->p2) {
		case PW1_SIGN:
			return g_pw1_sign ? OSK_SW_OK : osk_card_pin_status();
		case PW1_OTHER:
			return g_pw1_other ? OSK_SW_OK : osk_card_pin_status();
		case PW3: {
			const uint8_t left = osk_card_admin_tries(OSK_CARD_ADMIN_PGP_PW3);

			if (g_pw3)
				return OSK_SW_OK;
			if (left == 0u)
				return OSK_SW_AUTH_BLOCKED;
			return (uint16_t)(OSK_SW_PIN_TRIES | left);
		}
		default:
			return OSK_SW_REF_NOT_FOUND;
		}
	}

	switch (a->p2) {
	case PW1_SIGN:
	case PW1_OTHER:
		sw = osk_card_pin_verify(a->data, a->lc);
		if (sw == OSK_SW_OK) {
			if (a->p2 == PW1_SIGN)
				g_pw1_sign = true;
			else
				g_pw1_other = true;
			if (g_st.pw1_len != (uint8_t)a->lc) {
				g_st.pw1_len = (uint8_t)a->lc;
				state_save();
			}
		}
		return sw;
	case PW3:
		sw = osk_card_admin_verify(OSK_CARD_ADMIN_PGP_PW3, a->data, a->lc);
		g_pw3 = (sw == OSK_SW_OK);
		return sw;
	default:
		return OSK_SW_REF_NOT_FOUND;
	}
}

static uint16_t cmd_change_reference(const osk_apdu_t *a)
{
	state_load();
	if (a->p1 != 0x00u)
		return OSK_SW_WRONG_P1P2;

	if (a->p2 == PW1_SIGN || a->p2 == PW1_OTHER) {
		/* Das Datenfeld ist alte PIN gefolgt von neuer, ohne Trenner
		 * (Abschnitt 7.2.3). Wo die eine endet, weiss nur die Karte -
		 * darum merken wir die Laenge der gueltigen PIN mit. */
		const size_t old_len = g_st.pw1_len;
		uint16_t sw;

		if (old_len == 0u || a->lc <= old_len)
			return OSK_SW_WRONG_LENGTH;
		sw = osk_card_pin_change(a->data, old_len, a->data + old_len, a->lc - old_len);
		if (sw == OSK_SW_OK) {
			g_st.pw1_len = (uint8_t)(a->lc - old_len);
			g_pw1_sign = false;
			g_pw1_other = false;
			state_save();
		}
		return sw;
	}
	if (a->p2 == PW3) {
		/* Wo die alte Admin-PIN endet, weiss der Tresoreintrag: dort
		 * steht ihre Laenge neben ihr. Das Teilen uebernimmt darum
		 * osk_card_admin_change_split(). */
		const uint16_t sw =
		    osk_card_admin_change_split(OSK_CARD_ADMIN_PGP_PW3, a->data, a->lc);

		if (sw == OSK_SW_OK)
			g_pw3 = false;
		return sw;
	}
	return OSK_SW_REF_NOT_FOUND;
}

static uint16_t cmd_reset_retry(const osk_apdu_t *a)
{
	state_load();
	if (a->p2 != PW1_SIGN)
		return OSK_SW_REF_NOT_FOUND;

	if (a->p1 == 0x02u) {
		/* Neue PW1, abgesichert durch die vorher freigegebene PW3. */
		if (!g_pw3)
			return OSK_SW_SECURITY;
		if (a->lc == 0u || a->lc > OSK_CARD_SECRET_MAX)
			return OSK_SW_WRONG_LENGTH;
		if (osk_se050_pin_clear() != OSK_OK)
			return OSK_SW_SECURITY;
		if (osk_se050_pin_set(NULL, 0u, a->data, a->lc) != OSK_OK)
			return OSK_SW_SECURITY;
		g_st.pw1_len = (uint8_t)a->lc;
		g_pw1_sign = false;
		g_pw1_other = false;
		state_save();
		return OSK_SW_OK;
	}
	if (a->p1 != 0x00u)
		return OSK_SW_WRONG_P1P2;

	/* Mit dem Resetting Code (Abschnitt 7.2.4): Code und neue PW1 ohne
	 * Trenner. Die Laenge des Codes steht in seinem Tresoreintrag. */
	{
		const uint8_t code_len = osk_card_admin_secret_len(OSK_CARD_ADMIN_PGP_RC);
		uint16_t sw;

		if (code_len == 0u)
			return OSK_SW_REF_NOT_FOUND;
		if (a->lc <= code_len || a->lc - code_len > OSK_CARD_SECRET_MAX)
			return OSK_SW_WRONG_LENGTH;
		sw = osk_card_admin_verify(OSK_CARD_ADMIN_PGP_RC, a->data, code_len);
		if (sw != OSK_SW_OK)
			return sw;
		if (osk_se050_pin_clear() != OSK_OK)
			return OSK_SW_SECURITY;
		if (osk_se050_pin_set(NULL, 0u, a->data + code_len, a->lc - code_len) != OSK_OK)
			return OSK_SW_SECURITY;
		g_st.pw1_len = (uint8_t)(a->lc - code_len);
		g_pw1_sign = false;
		g_pw1_other = false;
		state_save();
		return OSK_SW_OK;
	}
}

/* --- Schluessel --------------------------------------------------------- */

static unsigned crt_to_key(const uint8_t *data, size_t len)
{
	if (osk_tlv_find(data, len, 0xb6u, NULL, NULL))
		return OSK_PGP_KEY_SIGN;
	if (osk_tlv_find(data, len, 0xb8u, NULL, NULL))
		return OSK_PGP_KEY_DEC;
	if (osk_tlv_find(data, len, 0xa4u, NULL, NULL))
		return OSK_PGP_KEY_AUTH;
	return 3u;
}

static const char *key_name(unsigned key)
{
	switch (key) {
	case OSK_PGP_KEY_SIGN:
		return "OpenPGP Unterschrift";
	case OSK_PGP_KEY_DEC:
		return "OpenPGP Entschluesselung";
	case OSK_PGP_KEY_AUTH:
		return "OpenPGP Anmeldung";
	default:
		return "OpenPGP";
	}
}

static uint16_t put_public_key(unsigned key, osk_resp_t *res)
{
	uint8_t raw[160];
	size_t raw_len = 0;
	const uint8_t *point = NULL;
	const size_t point_len = curve_half(g_st.curve[key]) * 2u + 1u;
	osk_tlv_mark_t m;

	if (key_is_rsa(key)) {
		/* Bei RSA ist es 7F49 mit 81 (Modulus) und 82 (Exponent) -
		 * Abschnitt 7.2.14. */
		return osk_rsa_put_public(OSK_PGP_KEY_ID(key), res);
	}
	if (osk_se050_key_public(OSK_PGP_KEY_ID(key), raw, sizeof(raw), &raw_len) != OSK_OK)
		return OSK_SW_REF_NOT_FOUND;
	if (osk_card_public_point(raw, raw_len, point_len, &point) != OSK_OK)
		return OSK_SW_UNKNOWN;

	/* Antwort nach Abschnitt 7.2.14: 7F49 mit 0x86, dem Punkt. */
	osk_resp_tlv_begin(res, 0x7f49u, &m);
	osk_resp_tlv(res, 0x86u, point, point_len);
	osk_resp_tlv_end(res, &m);
	return OSK_SW_OK;
}

static uint16_t cmd_generate(const osk_apdu_t *a, osk_resp_t *res)
{
	const unsigned key = crt_to_key(a->data, a->lc);
	osk_key_policy_t policy;
	osk_ask_req_t ask;

	state_load();
	if (key > 2u)
		return OSK_SW_WRONG_DATA;

	if (a->p1 == 0x81u)
		return put_public_key(key, res);
	if (a->p1 != 0x80u)
		return OSK_SW_WRONG_P1P2;

	if (!g_pw3)
		return OSK_SW_SECURITY;

	memset(&ask, 0, sizeof(ask));
	ask.kind = OSK_ASK_PAIR;
	ask.subject = key_name(key);
	ask.detail = "neues Schluesselpaar";
	ask.asked_by = "OpenPGP";
	ask.timeout_ms = 30000u;
	if (osk_ui_confirm(&ask) != OSK_OK)
		return OSK_SW_SECURITY;

	/* Ohne vorher gesetzte Algorithmus-Eigenschaften gilt P-256. */
	if (!key_present(key))
		g_st.curve[key] = 1u;
	policy.exportable = false;
	policy.needs_touch = true;

	(void)osk_se050_key_delete(OSK_PGP_KEY_ID(key));

	if (key_is_rsa(key)) {
		osk_rsa_bits_t bits;

		if (!osk_rsa_bits_ok(g_st.rsa_bits[key]) ||
		    !osk_rsa_bits_enum(g_st.rsa_bits[key], &bits))
			return OSK_SW_FUNC_NOT_SUPPORTED;
		if (osk_se050_rsa_key_create(OSK_PGP_KEY_ID(key), bits, &policy) != OSK_OK)
			return OSK_SW_UNKNOWN;
		state_save();
		return put_public_key(key, res);
	}

	if (osk_se050_key_create(OSK_PGP_KEY_ID(key), curve_to_se050(g_st.curve[key]), &policy) !=
	    OSK_OK)
		return OSK_SW_UNKNOWN;
	state_save();
	return put_public_key(key, res);
}

/* --- Unterschreiben, Entschluesseln, Anmelden --------------------------- */

static uint16_t sign_with(unsigned key, const uint8_t *hash, size_t hash_len, osk_ask_t kind,
			  osk_resp_t *res)
{
	uint8_t der[160];
	uint8_t raw[96];
	size_t der_len = 0;
	size_t raw_len = 0;
	osk_ask_req_t ask;
	const size_t half = curve_half(g_st.curve[key]);

	if (hash_len == 0u || hash_len > half)
		return OSK_SW_WRONG_DATA;
	if (g_st.curve[key] == 0u)
		return OSK_SW_REF_NOT_FOUND;

	memset(&ask, 0, sizeof(ask));
	ask.kind = kind;
	ask.subject = key_name(key);
	ask.detail = (g_st.login_len != 0u) ? (const char *)g_st.login : NULL;
	ask.asked_by = "OpenPGP";
	ask.timeout_ms = 30000u;
	if (osk_ui_confirm(&ask) != OSK_OK)
		return OSK_SW_SECURITY;

	if (osk_se050_sign(OSK_PGP_KEY_ID(key), hash, hash_len, der, sizeof(der), &der_len) !=
	    OSK_OK)
		return OSK_SW_UNKNOWN;
	/* OpenPGP will r und s nackt hintereinander, nicht als DER
	 * (Abschnitt 7.2.10 mit RFC 6637). */
	if (osk_card_sig_der_to_raw(der, der_len, half, raw, sizeof(raw), &raw_len) != OSK_OK)
		return OSK_SW_UNKNOWN;
	osk_resp_bytes(res, raw, raw_len);
	osk_card_wipe(der, sizeof(der));
	osk_card_wipe(raw, sizeof(raw));
	return OSK_SW_OK;
}

/* RSA-Unterschrift. OpenPGP schickt KEINEN nackten Hash, sondern das
 * DigestInfo - Kennung des Verfahrens und Hash zusammen (Abschnitt 7.2.10:
 * die Karte fuellt nach PKCS#1 auf, das DigestInfo baut der Rechner). Der
 * Vertrag will beides getrennt, und aus der Laenge ist es nicht
 * ableitbar - SHA-512/256 und SHA-256 sind beide 32 Byte lang. Darum
 * laeuft es durch osk_rsa_digestinfo_parse() und nicht durch eine
 * Laengentabelle.
 *
 * Ein Kartenprogramm, das SHA-1 oder RIPEMD-160 schickt, bekommt 6A80.
 * Das ist eine Verengung gegenueber der Spezifikation und gehoert in den
 * Bericht; sie folgt daraus, dass osk_hash_t nur die drei SHA-2-Verfahren
 * kennt (osk_se050.h). */
static uint16_t rsa_sign_with(unsigned key, const uint8_t *di, size_t di_len, osk_ask_t kind,
			      osk_resp_t *res)
{
	static uint8_t sig[OSK_RSA_MOD_MAX];
	osk_hash_t hash_alg;
	const uint8_t *hash = NULL;
	size_t hash_len = 0;
	size_t sig_len = 0;
	osk_ask_req_t ask;

	if (!osk_rsa_digestinfo_parse(di, di_len, &hash_alg, &hash, &hash_len))
		return OSK_SW_WRONG_DATA;

	memset(&ask, 0, sizeof(ask));
	ask.kind = kind;
	ask.subject = key_name(key);
	ask.detail = (g_st.login_len != 0u) ? (const char *)g_st.login : NULL;
	ask.asked_by = "OpenPGP";
	ask.timeout_ms = 30000u;
	if (osk_ui_confirm(&ask) != OSK_OK)
		return OSK_SW_SECURITY;

	if (osk_se050_rsa_sign(OSK_PGP_KEY_ID(key), OSK_RSA_PAD_PKCS1_V15, hash_alg, hash,
			       hash_len, sig, sizeof(sig), &sig_len) != OSK_OK)
		return OSK_SW_UNKNOWN;
	/* Bei RSA geht die Unterschrift nackt hinaus, so lang wie der
	 * Modulus - nicht als DER und nicht als r||s. */
	osk_resp_bytes(res, sig, sig_len);
	osk_card_wipe(sig, sizeof(sig));
	return OSK_SW_OK;
}

/* RSA-Entschluesselung, Abschnitt 7.2.11: im Datenfeld steht ein
 * Kennbyte 00 und danach das Chiffrat in der Laenge des Modulus. Heraus
 * kommt der Sitzungsschluessel, den die Karte aus der Auffuellung
 * herausschaelt.
 *
 * Nur PKCS#1 v1.5. OAEP kann der Chip in dieser Fassung nicht, und das
 * ist belegt und nicht unfertig: AN12413 Tabelle 32 kennt OAEP nur mit
 * SHA-1, und osk_hash_t hat kein SHA-1 (docs/se050.md). OpenPGP
 * verschluesselt Sitzungsschluessel mit v1.5, also kostet es hier
 * nichts. */
static uint16_t rsa_decipher(const uint8_t *in, size_t in_len, osk_resp_t *res)
{
	static uint8_t plain[OSK_RSA_MOD_MAX];
	size_t out_len = 0;
	size_t mod_len;
	osk_ask_req_t ask;

	mod_len = osk_rsa_mod_bytes(g_st.rsa_bits[OSK_PGP_KEY_DEC]);
	if (mod_len == 0u)
		return OSK_SW_REF_NOT_FOUND;
	if (in_len != mod_len + 1u || in[0] != 0x00u)
		return OSK_SW_WRONG_DATA;

	memset(&ask, 0, sizeof(ask));
	ask.kind = OSK_ASK_DECRYPT;
	ask.subject = key_name(OSK_PGP_KEY_DEC);
	ask.asked_by = "OpenPGP";
	ask.timeout_ms = 30000u;
	if (osk_ui_confirm(&ask) != OSK_OK)
		return OSK_SW_SECURITY;

	if (osk_se050_rsa_decrypt(OSK_PGP_KEY_ID(OSK_PGP_KEY_DEC), OSK_RSA_PAD_PKCS1_V15,
				  OSK_HASH_SHA256, in + 1u, in_len - 1u, plain, sizeof(plain),
				  &out_len) != OSK_OK)
		return OSK_SW_UNKNOWN;
	osk_resp_bytes(res, plain, out_len);
	osk_card_wipe(plain, sizeof(plain));
	return OSK_SW_OK;
}

static uint16_t cmd_pso(const osk_apdu_t *a, osk_resp_t *res)
{
	state_load();

	/* COMPUTE DIGITAL SIGNATURE (Abschnitt 7.2.10). */
	if (a->p1 == 0x9eu && a->p2 == 0x9au) {
		uint16_t sw;

		if (!g_pw1_sign)
			return OSK_SW_SECURITY;
		if (!key_present(OSK_PGP_KEY_SIGN))
			return OSK_SW_REF_NOT_FOUND;
		sw = key_is_rsa(OSK_PGP_KEY_SIGN)
			 ? rsa_sign_with(OSK_PGP_KEY_SIGN, a->data, a->lc, OSK_ASK_SIGN, res)
			 : sign_with(OSK_PGP_KEY_SIGN, a->data, a->lc, OSK_ASK_SIGN, res);
		if (sw == OSK_SW_OK) {
			g_st.sig_count++;
			state_save();
			/* PW1 gilt nur fuer eine Unterschrift, ausser der
			 * PW-Status sagt etwas anderes (Abschnitt 4.3.1). */
			if (g_st.pw1_mode == 0u)
				g_pw1_sign = false;
		}
		return sw;
	}

	/* DECIPHER (Abschnitt 7.2.11). Bei elliptischen Kurven kommt der
	 * fremde Punkt in A6 -> 7F49 -> 86, und heraus kommt das gemeinsame
	 * Geheimnis. */
	if (a->p1 == 0x80u && a->p2 == 0x86u) {
		const uint8_t *tmpl = NULL;
		const uint8_t *inner = NULL;
		const uint8_t *point = NULL;
		size_t tmpl_len = 0;
		size_t inner_len = 0;
		size_t point_len = 0;
		uint8_t secret[64];
		size_t secret_len = 0;
		osk_ask_req_t ask;

		if (!g_pw1_other)
			return OSK_SW_SECURITY;
		if (!key_present(OSK_PGP_KEY_DEC))
			return OSK_SW_REF_NOT_FOUND;
		if (key_is_rsa(OSK_PGP_KEY_DEC))
			return rsa_decipher(a->data, a->lc, res);
		if (a->lc < 3u)
			return OSK_SW_WRONG_LENGTH;
		if (!osk_tlv_find(a->data, a->lc, 0xa6u, &tmpl, &tmpl_len))
			return OSK_SW_WRONG_DATA;
		if (!osk_tlv_find(tmpl, tmpl_len, 0x7f49u, &inner, &inner_len))
			return OSK_SW_WRONG_DATA;
		if (!osk_tlv_find(inner, inner_len, 0x86u, &point, &point_len))
			return OSK_SW_WRONG_DATA;
		if (point_len != curve_half(g_st.curve[OSK_PGP_KEY_DEC]) * 2u + 1u ||
		    point[0] != 0x04u)
			return OSK_SW_WRONG_DATA;

		memset(&ask, 0, sizeof(ask));
		ask.kind = OSK_ASK_DECRYPT;
		ask.subject = key_name(OSK_PGP_KEY_DEC);
		ask.asked_by = "OpenPGP";
		ask.timeout_ms = 30000u;
		if (osk_ui_confirm(&ask) != OSK_OK)
			return OSK_SW_SECURITY;

		if (osk_se050_ecdh(OSK_PGP_KEY_ID(OSK_PGP_KEY_DEC), point, point_len, secret,
				   sizeof(secret), &secret_len) != OSK_OK)
			return OSK_SW_UNKNOWN;
		osk_resp_bytes(res, secret, secret_len);
		osk_card_wipe(secret, sizeof(secret));
		return OSK_SW_OK;
	}
	return OSK_SW_WRONG_P1P2;
}

static uint16_t cmd_internal_auth(const osk_apdu_t *a, osk_resp_t *res)
{
	state_load();
	if (a->p1 != 0x00u || a->p2 != 0x00u)
		return OSK_SW_WRONG_P1P2;
	if (!g_pw1_other)
		return OSK_SW_SECURITY;
	if (!key_present(OSK_PGP_KEY_AUTH))
		return OSK_SW_REF_NOT_FOUND;
	if (key_is_rsa(OSK_PGP_KEY_AUTH))
		return rsa_sign_with(OSK_PGP_KEY_AUTH, a->data, a->lc, OSK_ASK_LOGIN, res);
	return sign_with(OSK_PGP_KEY_AUTH, a->data, a->lc, OSK_ASK_LOGIN, res);
}

/* --- Zuruecksetzen ------------------------------------------------------ */

static uint16_t cmd_terminate(void)
{
	/* Erlaubt mit freigegebener Admin-PIN oder wenn sie gesperrt ist
	 * (Abschnitt 7.2.16). */
	if (!g_pw3 && osk_card_admin_tries(OSK_CARD_ADMIN_PGP_PW3) != 0u)
		return OSK_SW_COND_NOT_SATISFIED;
	g_terminated = true;
	return OSK_SW_OK;
}

static uint16_t cmd_activate(void)
{
	osk_ask_req_t ask;

	if (!g_terminated)
		return OSK_SW_OK; /* schon aktiv - nach 7.2.17 kein Fehler */

	memset(&ask, 0, sizeof(ask));
	ask.kind = OSK_ASK_WIPE;
	ask.subject = "OpenPGP: Karte zuruecksetzen";
	ask.asked_by = "OpenPGP";
	ask.timeout_ms = 30000u;
	if (osk_ui_confirm(&ask) != OSK_OK)
		return OSK_SW_SECURITY;

	for (unsigned key = 0; key < 3u; key++)
		(void)osk_se050_key_delete(OSK_PGP_KEY_ID(key));
	state_defaults(&g_st);
	g_loaded = true;
	state_save();
	g_terminated = false;
	g_pw1_sign = false;
	g_pw1_other = false;
	g_pw3 = false;
	return OSK_SW_OK;
}

/* --- Verteiler innerhalb von OpenPGP ------------------------------------ */

static uint16_t pgp_handle(const osk_apdu_t *a, osk_resp_t *res)
{
	if ((a->cla & 0xe0u) != 0x00u)
		return OSK_SW_CLA_NOT_SUPPORTED;

	/* Nach TERMINATE DF geht nur noch ACTIVATE FILE. */
	if (g_terminated && a->ins != INS_ACTIVATE && a->ins != INS_SELECT)
		return OSK_SW_COND_NOT_SATISFIED;

	switch (a->ins) {
	case INS_SELECT:
		/* Die Auswahl liefert keine Daten (Abschnitt 7.2.1). */
		g_pw1_sign = false;
		g_pw1_other = false;
		g_pw3 = false;
		return OSK_SW_OK;
	case INS_VERIFY:
		return cmd_verify(a);
	case INS_CHANGE_REFERENCE:
		return cmd_change_reference(a);
	case INS_RESET_RETRY:
		return cmd_reset_retry(a);
	case INS_GET_DATA:
		return cmd_get_data(a, res);
	case INS_PUT_DATA:
		return cmd_put_data(a);
	case INS_GENERATE_ASYM:
		return cmd_generate(a, res);
	case INS_PSO:
		return cmd_pso(a, res);
	case INS_INTERNAL_AUTH:
		return cmd_internal_auth(a, res);
	case INS_TERMINATE:
		return cmd_terminate();
	case INS_ACTIVATE:
		return cmd_activate();
	default:
		return OSK_SW_INS_NOT_SUPPORTED;
	}
}

static const osk_applet_t g_pgp = {
    g_aid, OSK_PGP_AID_LEN, "OpenPGP", pgp_handle, pgp_reset,
};

const osk_applet_t *osk_openpgp_applet(void)
{
	return &g_pgp;
}
