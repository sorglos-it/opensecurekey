/* Die Credential-ID haelt alles zusammen: ohne sie findet kein abgeleiteter
 * Schluessel zurueck. Diese Faelle pruefen, dass sie nur dann wieder
 * aufgeht, wenn sie unveraendert von diesem Geraet stammt. */
#include "test_util.h"
#include "mocks.h"
#include "osk_credid.h"
#include "osk_ctap_key.h"
#include "osk_crypto.h"
#include "osk_se050.h"

static void fill_hash(uint8_t out[32], const char *rp)
{
	osk_sha256((const uint8_t *)rp, strlen(rp), out);
}

static osk_credid_t sample(const char *rp)
{
	osk_credid_t c;
	memset(&c, 0, sizeof(c));
	c.resident = true;
	c.hmac_secret = true;
	c.cred_protect = 2;
	fill_hash(c.rp_id_hash, rp);
	osk_se050_random(c.salt, sizeof(c.salt));
	c.created = 1234567u;
	return c;
}

static void test_hin_und_zurueck(void)
{
	mock_reset_all();

	const osk_credid_t in = sample("example.com");
	uint8_t id[OSK_CREDID_LEN];
	size_t len = 0;

	T_EQ("packen", osk_credid_pack(&in, id, sizeof(id), &len), OSK_OK);
	T_EQ("Laenge", (long long)len, OSK_CREDID_LEN);

	osk_credid_t out;
	T_EQ("entpacken", osk_credid_unpack(id, len, &out), OSK_OK);
	T_TRUE("resident erhalten", out.resident == in.resident);
	T_TRUE("hmac-secret erhalten", out.hmac_secret == in.hmac_secret);
	T_EQ("credProtect erhalten", out.cred_protect, in.cred_protect);
	T_TRUE("rpIdHash erhalten", memcmp(out.rp_id_hash, in.rp_id_hash, 32) == 0);
	T_TRUE("Salz erhalten", memcmp(out.salt, in.salt, OSK_KEY_SALT_LEN) == 0);
	T_EQ("Zeit erhalten", (long long)out.created, (long long)in.created);
}

static void test_zwei_ids_sind_verschieden(void)
{
	mock_reset_all();

	const osk_credid_t a = sample("example.com");
	const osk_credid_t b = sample("example.com");
	uint8_t ida[OSK_CREDID_LEN], idb[OSK_CREDID_LEN];
	size_t la = 0, lb = 0;

	T_EQ("erste packen", osk_credid_pack(&a, ida, sizeof(ida), &la), OSK_OK);
	T_EQ("zweite packen", osk_credid_pack(&b, idb, sizeof(idb), &lb), OSK_OK);
	T_TRUE("dieselbe RP, aber andere ID", memcmp(ida, idb, OSK_CREDID_LEN) != 0);

	/* Und die abgeleiteten Schluessel sind auch verschieden. */
	uint8_t pa[64], pb[64];
	T_EQ("Schluessel a", osk_ctap_key_public(a.salt, a.rp_id_hash, pa), OSK_OK);
	T_EQ("Schluessel b", osk_ctap_key_public(b.salt, b.rp_id_hash, pb), OSK_OK);
	T_TRUE("verschiedene Schluessel", memcmp(pa, pb, 64) != 0);
}

static void test_jedes_bit_fliegt_auf(void)
{
	mock_reset_all();

	const osk_credid_t in = sample("example.com");
	uint8_t id[OSK_CREDID_LEN];
	size_t len = 0;
	T_EQ("packen", osk_credid_pack(&in, id, sizeof(id), &len), OSK_OK);

	/* Jedes einzelne Byte kippen und pruefen, dass nichts mehr aufgeht.
	 * Das ist der eigentliche Schutz: ohne MAC koennte ein Angreifer die
	 * rpIdHash tauschen und den Schluessel einer anderen Seite benutzen. */
	int accepted = 0;
	for (size_t i = 0; i < len; i++) {
		uint8_t bad[OSK_CREDID_LEN];
		osk_credid_t out;
		memcpy(bad, id, len);
		bad[i] ^= 0x01;
		if (osk_credid_unpack(bad, len, &out) == OSK_OK)
			accepted++;
	}
	T_EQ("kein verfaelschtes Byte geht durch", accepted, 0);
}

static void test_fremde_ids(void)
{
	mock_reset_all();

	const osk_credid_t in = sample("example.com");
	uint8_t id[OSK_CREDID_LEN];
	size_t len = 0;
	T_EQ("packen", osk_credid_pack(&in, id, sizeof(id), &len), OSK_OK);

	osk_credid_t out;
	T_EQ("zu kurz", osk_credid_unpack(id, len - 1, &out), OSK_ERR_NOT_FOUND);
	T_EQ("zu lang", osk_credid_unpack(id, len + 1, &out), OSK_ERR_NOT_FOUND);
	T_EQ("leer", osk_credid_unpack(NULL, 0, &out), OSK_ERR_NOT_FOUND);

	uint8_t other[OSK_CREDID_LEN];
	memcpy(other, id, len);
	other[0] = 0x02; /* unbekannte Fassung */
	T_EQ("fremde Fassung", osk_credid_unpack(other, len, &out), OSK_ERR_NOT_FOUND);

	/* Nach einem Reset ist das Grundgeheimnis neu - alte IDs sind wertlos. */
	const osk_key_policy_t policy = {.exportable = false, .needs_touch = false};
	osk_se050_key_delete(OSK_KEY_ID_ROOT);
	T_EQ("neues Grundgeheimnis", osk_se050_key_create(OSK_KEY_ID_ROOT, OSK_CURVE_P256, &policy),
	     OSK_OK);
	T_EQ("alte ID nach Reset wertlos", osk_credid_unpack(id, len, &out), OSK_ERR_NOT_FOUND);
}

static void test_ableitung_ist_wiederholbar(void)
{
	mock_reset_all();

	const osk_credid_t in = sample("example.com");
	uint8_t p1[64], p2[64];

	T_EQ("erster Aufruf", osk_ctap_key_public(in.salt, in.rp_id_hash, p1), OSK_OK);
	T_EQ("zweiter Aufruf", osk_ctap_key_public(in.salt, in.rp_id_hash, p2), OSK_OK);
	T_TRUE("derselbe Schluessel", memcmp(p1, p2, 64) == 0);

	/* Dieselbe Zahl, anderer Zweck: die largeBlobKey darf nicht ueber
	 * hmac-secret erreichbar sein (Spezifikation 12.3). */
	uint8_t uv[32], nouv[32], blob[32];
	T_EQ("CredRandom mit uv", osk_ctap_key_secret(OSK_INFO_HMAC_UV, in.salt, in.rp_id_hash, uv),
	     OSK_OK);
	T_EQ("CredRandom ohne uv",
	     osk_ctap_key_secret(OSK_INFO_HMAC_NOUV, in.salt, in.rp_id_hash, nouv), OSK_OK);
	T_EQ("largeBlobKey", osk_ctap_key_secret(OSK_INFO_LARGEBLOB, in.salt, in.rp_id_hash, blob),
	     OSK_OK);
	T_TRUE("uv und nicht-uv verschieden", memcmp(uv, nouv, 32) != 0);
	T_TRUE("largeBlobKey nicht gleich uv", memcmp(uv, blob, 32) != 0);
	T_TRUE("largeBlobKey nicht gleich nicht-uv", memcmp(nouv, blob, 32) != 0);
}

int main(void)
{
	test_hin_und_zurueck();
	test_zwei_ids_sind_verschieden();
	test_jedes_bit_fliegt_auf();
	test_fremde_ids();
	test_ableitung_ist_wiederholbar();
	return t_report("credid");
}
