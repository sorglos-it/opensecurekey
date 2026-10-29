/* RSA-Kartenseite. Siehe osk_rsa_card.h. */
#include <string.h>
#include "osk_rsa_card.h"
#include "osk_card.h"

/* --- Ist RSA da? ------------------------------------------------------- */

/* Drei Zustaende, nicht zwei: ungefragt, ja, nein. Ohne den ersten wuerde
 * ein "nein" nicht von "noch nicht gefragt" zu unterscheiden sein, und
 * dann faellt entweder jede Frage auf den Chip zurueck oder die erste
 * Antwort gilt fuer immer. */
static enum { ASK_UNKNOWN = 0, ASK_YES, ASK_NO } g_asked;
static uint32_t g_mask;

void osk_rsa_forget(void)
{
	g_asked = ASK_UNKNOWN;
	g_mask = 0u;
}

bool osk_rsa_available(void)
{
#if !defined(CONFIG_OSK_RSA)
	/* Fassung ohne RSA. Die einzige Stelle, an der die Kconfig-Option
	 * wirkt - siehe Kopf von osk_rsa_card.h. */
	return false;
#else
	uint32_t mask = 0u;

	if (g_asked != ASK_UNKNOWN)
		return g_asked == ASK_YES;

	if (osk_se050_rsa_sizes(&mask) != OSK_OK || mask == 0u) {
		/* Genau der Fall, vor dem Strang A warnt: ein SE051A2
		 * antwortet auf derselben Adresse und kann kein RSA. Das wird
		 * hier gemerkt und nicht bei jedem Kommando neu erfragt - und
		 * es fuehrt zu einem Statuswort, nicht zu einem Absturz. */
		g_asked = ASK_NO;
		g_mask = 0u;
		return false;
	}
	g_asked = ASK_YES;
	g_mask = mask;
	return true;
#endif
}

bool osk_rsa_bits_enum(uint16_t bits, osk_rsa_bits_t *out)
{
	osk_rsa_bits_t e;

	switch (bits) {
	case 2048u:
		e = OSK_RSA_2048;
		break;
	case 3072u:
		e = OSK_RSA_3072;
		break;
	case 4096u:
		e = OSK_RSA_4096;
		break;
	default:
		return false;
	}
	if (out != NULL)
		*out = e;
	return true;
}

bool osk_rsa_bits_ok(uint16_t bits)
{
	osk_rsa_bits_t e;

	if (!osk_rsa_available())
		return false;
	if (!osk_rsa_bits_enum(bits, &e))
		return false;
	return (g_mask & (1u << (unsigned)e)) != 0u;
}

size_t osk_rsa_mod_bytes(uint16_t bits)
{
	if (!osk_rsa_bits_enum(bits, NULL))
		return 0u;
	return (size_t)bits / 8u;
}

/* --- DigestInfo -------------------------------------------------------- */

/* Die drei Vorsaetze stehen woertlich in RFC 8017 Abschnitt 9.2,
 * Anmerkung 1 ("For the six hash functions mentioned in Appendix B.1, the
 * DER encoding T of the DigestInfo value is equal to the following"). Sie
 * werden als Ganzes verglichen und nicht Stueck fuer Stueck zerlegt: der
 * RFC schreibt genau diese Bytes vor, und was davon abweicht, ist kein
 * DigestInfo, das wir unterschreiben wollen. */
typedef struct {
	osk_hash_t alg;
	uint8_t hash_len;
	uint8_t pre_len;
	uint8_t pre[19];
} di_t;

static const di_t g_di[] = {
    {OSK_HASH_SHA256,
     32u,
     19u,
     {0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01,
      0x05, 0x00, 0x04, 0x20}},
    {OSK_HASH_SHA384,
     48u,
     19u,
     {0x30, 0x41, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x02,
      0x05, 0x00, 0x04, 0x30}},
    {OSK_HASH_SHA512,
     64u,
     19u,
     {0x30, 0x51, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x03,
      0x05, 0x00, 0x04, 0x40}},
};

#define DI_COUNT (sizeof(g_di) / sizeof(g_di[0]))

bool osk_rsa_digestinfo_parse(const uint8_t *in, size_t len, osk_hash_t *alg_out,
			      const uint8_t **hash_out, size_t *hash_len_out)
{
	if (in == NULL)
		return false;
	for (size_t i = 0; i < DI_COUNT; i++) {
		const di_t *d = &g_di[i];

		/* Erst die Laenge, dann der Inhalt - nie umgekehrt. Ein
		 * DigestInfo mit einem Byte zu viel ist keines. */
		if (len != (size_t)d->pre_len + (size_t)d->hash_len)
			continue;
		if (memcmp(in, d->pre, d->pre_len) != 0)
			continue;
		if (alg_out != NULL)
			*alg_out = d->alg;
		if (hash_out != NULL)
			*hash_out = in + d->pre_len;
		if (hash_len_out != NULL)
			*hash_len_out = d->hash_len;
		return true;
	}
	return false;
}

bool osk_rsa_pkcs1_unwrap(const uint8_t *block, size_t block_len, size_t mod_len,
			  osk_hash_t *alg_out, const uint8_t **hash_out, size_t *hash_len_out)
{
	size_t at;
	size_t pad;

	if (block == NULL || mod_len == 0u || mod_len > OSK_RSA_MOD_MAX)
		return false;

	/* Mit fuehrendem 00 (Laenge = Modulus) oder ohne (ein Byte kuerzer).
	 * Beides kommt vor, und beides ist derselbe Block - das fuehrende
	 * Null-Byte traegt keine Information. */
	if (block_len == mod_len) {
		if (block[0] != 0x00u)
			return false;
		at = 1u;
	} else if (block_len + 1u == mod_len) {
		at = 0u;
	} else {
		return false;
	}

	if (block_len - at < 11u)
		return false;
	if (block[at] != 0x01u) /* Blocktyp 1: private Schluesseloperation */
		return false;
	at++;

	pad = 0u;
	while (at < block_len && block[at] == 0xffu) {
		at++;
		pad++;
	}
	/* RFC 8017 Abschnitt 9.2 Schritt 3: mindestens acht Auffuellbytes.
	 * Weniger laesst Raum fuer einen zweiten, kuerzeren Hash im selben
	 * Block - das ist die Luecke, an der Bleichenbacher-Faelschungen
	 * ansetzen. */
	if (pad < 8u)
		return false;
	if (at >= block_len || block[at] != 0x00u)
		return false;
	at++;

	return osk_rsa_digestinfo_parse(block + at, block_len - at, alg_out, hash_out, hash_len_out);
}

/* --- Oeffentlicher Teil ------------------------------------------------ */

osk_result_t osk_rsa_public_parts(osk_key_id_t id, uint8_t *mod, size_t mod_cap, size_t *mod_len,
				  uint8_t *exp, size_t exp_cap, size_t *exp_len)
{
	if (mod == NULL || mod_len == NULL || exp == NULL || exp_len == NULL)
		return OSK_ERR_ARG;
	*mod_len = 0u;
	*exp_len = 0u;
	return osk_se050_rsa_public(id, mod, mod_cap, mod_len, exp, exp_cap, exp_len);
}

uint16_t osk_rsa_modulus_bits(osk_key_id_t id)
{
	uint8_t mod[OSK_RSA_MOD_MAX];
	uint8_t exp[8];
	size_t mod_len = 0;
	size_t exp_len = 0;
	uint16_t bits;

	if (osk_rsa_public_parts(id, mod, sizeof(mod), &mod_len, exp, sizeof(exp), &exp_len) !=
	    OSK_OK)
		return 0u;
	/* Der Chip liefert den Modulus ohne fuehrende Nullen; die Bitlaenge
	 * ist deshalb die Bytelaenge mal acht. Geprueft wird, dass eine der
	 * drei Laengen herauskommt - alles andere waere eine Ueberraschung
	 * und wird nicht stillschweigend gerundet. */
	if (mod_len == 0u || mod_len > OSK_RSA_MOD_MAX)
		return 0u;
	bits = (uint16_t)(mod_len * 8u);
	osk_card_wipe(mod, sizeof(mod));
	if (!osk_rsa_bits_enum(bits, NULL))
		return 0u;
	return bits;
}

uint16_t osk_rsa_put_public(osk_key_id_t id, osk_resp_t *res)
{
	uint8_t mod[OSK_RSA_MOD_MAX];
	uint8_t exp[8];
	size_t mod_len = 0;
	size_t exp_len = 0;
	osk_tlv_mark_t m;

	if (osk_rsa_public_parts(id, mod, sizeof(mod), &mod_len, exp, sizeof(exp), &exp_len) !=
	    OSK_OK)
		return OSK_SW_REF_NOT_FOUND;
	if (mod_len == 0u || exp_len == 0u)
		return OSK_SW_UNKNOWN;

	osk_resp_tlv_begin(res, 0x7f49u, &m);
	osk_resp_tlv(res, 0x81u, mod, mod_len);
	osk_resp_tlv(res, 0x82u, exp, exp_len);
	osk_resp_tlv_end(res, &m);
	osk_card_wipe(mod, sizeof(mod));
	return OSK_SW_OK;
}
