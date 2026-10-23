/* Die nicht geheime Kennung zum Suchen.
 *
 * Sie ist fuer FIDO2 da: beim Anmelden muss unter hunderten Eintraegen der
 * passende gefunden werden. Zwei Dinge muessen dabei stimmen, und beide werden
 * hier geprueft:
 *  - Gesucht wird, OHNE ein Geheimnis zu entschluesseln und OHNE eine Freigabe
 *    am Geraet zu verlangen. Sonst waere eine Anmeldung unbenutzbar.
 *  - Die Kennung liegt trotzdem im verschluesselten Teil. Wer den Flash
 *    ausliest, darf nicht sehen, bei welchen Diensten der Nutzer ein Konto
 *    hat. */

#include <string.h>
#include "osk_test.h"
#include "test_util.h"
#include "fake_flash.h"
#include "fake_crypto.h"
#include "fake_ui.h"
#include "store_internal.h"
#include "osk_store_profiles.h"

/* Eine Kennung, wie CTAP sie bildet: der Abdruck eines Dienstnamens. */
static void make_tag(uint8_t out[OSK_TAG_MAX], const char *site)
{
	T_OK(osk_crypto_sha256((const uint8_t *)site, strlen(site), out));
}

static osk_result_t add_tagged(const char *name, const char *site, const char *secret,
			       osk_item_type_t type, const uint8_t *tag, size_t tag_len,
			       osk_item_id_t *id_out)
{
	osk_item_meta_t meta;
	osk_item_id_t spare = 0;

	memset(&meta, 0, sizeof(meta));
	meta.type = type;
	meta.profile = 0;
	meta.name = name;
	meta.site = site;
	meta.tag_len = (uint8_t)tag_len;
	if (tag != NULL && tag_len > 0u) {
		memcpy(meta.tag, tag, tag_len);
	}
	/* osk_store_add will immer wissen, wohin mit der neuen Kennung. Wer sie
	 * nicht braucht, bekommt sie trotzdem - der Vertrag bleibt streng. */
	return osk_store_add(&meta, (const uint8_t *)secret, strlen(secret),
			     (id_out != NULL) ? id_out : &spare);
}

static void case_find(void)
{
	uint8_t tag_a[OSK_TAG_MAX], tag_b[OSK_TAG_MAX];
	osk_item_id_t id_a = 0, id_b = 0, id_plain = 0;
	osk_item_id_t found[8];
	size_t len = 0;

	osk_test_case("Suchen ueber die Kennung findet genau den Eintrag");
	t_device(0, 1u, T_PIN);
	make_tag(tag_a, "bank.example");
	make_tag(tag_b, "post.example");

	T_OK(add_tagged("Bank", "bank.example", "geheim-a", OSK_ITEM_LOGIN, tag_a, OSK_TAG_MAX,
			&id_a));
	T_OK(add_tagged("Post", "post.example", "geheim-b", OSK_ITEM_LOGIN, tag_b, OSK_TAG_MAX,
			&id_b));
	T_OK(add_tagged("Notiz", "", "geheim-c", OSK_ITEM_NOTE, NULL, 0, &id_plain));

	T_OK(osk_store_find_tag(0, OSK_ITEM_LOGIN, tag_a, OSK_TAG_MAX, found, 8, &len));
	T_EQ(len, 1);
	T_EQ(found[0], id_a);

	T_OK(osk_store_find_tag(0, OSK_ITEM_LOGIN, tag_b, OSK_TAG_MAX, found, 8, &len));
	T_EQ(len, 1);
	T_EQ(found[0], id_b);

	osk_test_case("Dabei wird keine Freigabe verlangt");
	fake_ui_reset();
	fake_ui_answer(false); /* der Nutzer wuerde ablehnen */
	T_OK(osk_store_find_tag(0, OSK_ITEM_LOGIN, tag_a, OSK_TAG_MAX, found, 8, &len));
	T_EQ(len, 1);
	/* Nicht ein einziges Mal gefragt - sonst waere eine Anmeldung mit
	 * hunderten Eintraegen unbenutzbar. */
	T_EQ(fake_ui_asks(), 0);
	fake_ui_answer(true);

	osk_test_case("Und kein Geheimnis angefasst");
	/* Gesucht wird im Index, der schon im RAM liegt. Ein Zugriff auf ein
	 * Geheimnis wuerde Flash lesen - also darf dabei nichts gelesen
	 * werden, was nicht der Index ist. Gemessen wird am einfachsten so:
	 * kein Schreibvorgang, kein Protokolleintrag. */
	size_t log_before = 0, log_after = 0;

	T_OK(osk_log_count(&log_before));
	fake_flash_count_reset();
	T_OK(osk_store_find_tag(0, OSK_ITEM_LOGIN, tag_a, OSK_TAG_MAX, found, 8, &len));
	T_OK(osk_log_count(&log_after));
	T_EQ(fake_flash_writes(), 0);
	T_EQ(log_after, log_before);

	osk_test_case("Der Typ gehoert zur Suche");
	T_OK(osk_store_find_tag(0, OSK_ITEM_TOTP, tag_a, OSK_TAG_MAX, found, 8, &len));
	T_EQ(len, 0);

	osk_test_case("Eine unbekannte Kennung findet nichts");
	uint8_t other[OSK_TAG_MAX];

	make_tag(other, "fremd.example");
	T_OK(osk_store_find_tag(0, OSK_ITEM_LOGIN, other, OSK_TAG_MAX, found, 8, &len));
	T_EQ(len, 0);

	osk_test_case("Eine halbe Kennung findet nichts");
	/* Sonst wuerde sich eine Kennung Byte fuer Byte erraten lassen. */
	T_OK(osk_store_find_tag(0, OSK_ITEM_LOGIN, tag_a, 8u, found, 8, &len));
	T_EQ(len, 0);

	osk_test_case("Eintraege ohne Kennung werden nie gefunden");
	uint8_t zero[OSK_TAG_MAX];

	memset(zero, 0, sizeof(zero));
	T_OK(osk_store_find_tag(0, OSK_ITEM_NOTE, zero, OSK_TAG_MAX, found, 8, &len));
	T_EQ(len, 0);

	osk_item_meta_t meta;

	T_OK(osk_store_list(0, 2, &meta));
	T_STR(meta.name, "Notiz");
	T_EQ(meta.tag_len, 0);
}

static void case_many(void)
{
	uint8_t tag[OSK_TAG_MAX];
	osk_item_id_t found[4];
	size_t len = 0;

	osk_test_case("Auch unter vielen Eintraegen, und mehrfach gleich");
	t_device(0, 1u, T_PIN);

	for (int i = 0; i < 40; i++) {
		char name[32], site[64];

		snprintf(name, sizeof(name), "dienst-%d", i);
		snprintf(site, sizeof(site), "dienst%d.example", i);
		make_tag(tag, site);
		T_OK(add_tagged(name, site, "geheim", OSK_ITEM_LOGIN, tag, OSK_TAG_MAX, NULL));
	}

	/* Zwei Nachweise fuer denselben Dienst - das kommt bei FIDO2 vor, wenn
	 * ein Nutzer zwei Konten hat. */
	make_tag(tag, "doppelt.example");
	T_OK(add_tagged("Konto A", "doppelt.example", "geheim-a", OSK_ITEM_LOGIN, tag, OSK_TAG_MAX,
			NULL));
	T_OK(add_tagged("Konto B", "doppelt.example", "geheim-b", OSK_ITEM_LOGIN, tag, OSK_TAG_MAX,
			NULL));

	T_OK(osk_store_find_tag(0, OSK_ITEM_LOGIN, tag, OSK_TAG_MAX, found, 4, &len));
	T_EQ(len, 2);

	osk_test_case("Zu kleiner Platz meldet sich, statt zu schweigen");
	T_RES(osk_store_find_tag(0, OSK_ITEM_LOGIN, tag, OSK_TAG_MAX, found, 1, &len),
	      OSK_ERR_FULL);
	T_EQ(len, 1);

	make_tag(tag, "dienst7.example");
	T_OK(osk_store_find_tag(0, OSK_ITEM_LOGIN, tag, OSK_TAG_MAX, found, 4, &len));
	T_EQ(len, 1);
}

static void case_secret(void)
{
	uint8_t tag[OSK_TAG_MAX];
	osk_item_id_t id = 0;

	osk_test_case("Die Kennung steht nicht im Klartext im Flash");
	t_device(0, 1u, T_PIN);
	make_tag(tag, "geheimer-dienst.example");
	T_OK(add_tagged("Konto", "geheimer-dienst.example", "geheim", OSK_ITEM_LOGIN, tag,
			OSK_TAG_MAX, &id));

	/* Der ganze Index-Ring wird abgesucht: weder die Kennung noch der
	 * Dienstname duerfen darin zu finden sein. Sonst wuesste jeder, der den
	 * Flash ausliest, wo der Nutzer Konten hat. */
	static uint8_t area[OSK_AREA_IDX_LEN];

	T_OK(osk_flash_read(OSK_AREA_IDX_OFF, area, sizeof(area)));
	T_TRUE(memmem_missing(area, sizeof(area), "geheimer-dienst.example"));
	T_TRUE(memmem_missing(area, sizeof(area), "Konto"));

	bool tag_found = false;

	for (size_t i = 0; i + OSK_TAG_MAX <= sizeof(area); i++) {
		if (memcmp(area + i, tag, OSK_TAG_MAX) == 0) {
			tag_found = true;
			break;
		}
	}
	T_TRUE(!tag_found);

	osk_test_case("Nach dem Neustart wird trotzdem gefunden");
	t_reboot();

	osk_item_id_t found[4];
	size_t len = 0;

	T_OK(osk_store_find_tag(0, OSK_ITEM_LOGIN, tag, OSK_TAG_MAX, found, 4, &len));
	T_EQ(len, 1);
	T_EQ(found[0], id);

	osk_test_case("Ein gesperrtes Profil findet nichts");
	osk_store_profile_lock(0);
	T_RES(osk_store_find_tag(0, OSK_ITEM_LOGIN, tag, OSK_TAG_MAX, found, 4, &len),
	      OSK_ERR_DENIED);
	T_EQ(len, 0);
}

static void case_change(void)
{
	uint8_t tag_a[OSK_TAG_MAX], tag_b[OSK_TAG_MAX];
	osk_item_id_t id = 0;
	osk_item_id_t found[4];
	size_t len = 0;

	osk_test_case("Eine geaenderte Kennung gilt sofort und dauerhaft");
	t_device(0, 1u, T_PIN);
	make_tag(tag_a, "alt.example");
	make_tag(tag_b, "neu.example");
	T_OK(add_tagged("Konto", "alt.example", "geheim", OSK_ITEM_LOGIN, tag_a, OSK_TAG_MAX, &id));

	osk_item_meta_t meta;

	memset(&meta, 0, sizeof(meta));
	meta.type = OSK_ITEM_LOGIN;
	meta.profile = 0;
	meta.name = "Konto";
	meta.site = "neu.example";
	meta.tag_len = OSK_TAG_MAX;
	memcpy(meta.tag, tag_b, OSK_TAG_MAX);
	T_OK(osk_store_update(id, &meta, NULL, 0));

	T_OK(osk_store_find_tag(0, OSK_ITEM_LOGIN, tag_a, OSK_TAG_MAX, found, 4, &len));
	T_EQ(len, 0);
	T_OK(osk_store_find_tag(0, OSK_ITEM_LOGIN, tag_b, OSK_TAG_MAX, found, 4, &len));
	T_EQ(len, 1);

	t_reboot();
	T_OK(osk_store_find_tag(0, OSK_ITEM_LOGIN, tag_b, OSK_TAG_MAX, found, 4, &len));
	T_EQ(len, 1);
	T_TRUE(t_secret_is(id, "geheim"));

	osk_test_case("Ein geloeschter Eintrag wird nicht mehr gefunden");
	T_OK(osk_store_delete(id));
	T_OK(osk_store_find_tag(0, OSK_ITEM_LOGIN, tag_b, OSK_TAG_MAX, found, 4, &len));
	T_EQ(len, 0);

	osk_test_case("Unsinnige Vorgaben werden abgelehnt");
	T_RES(osk_store_find_tag(0, OSK_ITEM_LOGIN, NULL, OSK_TAG_MAX, found, 4, &len),
	      OSK_ERR_ARG);
	T_RES(osk_store_find_tag(0, OSK_ITEM_LOGIN, tag_b, 0, found, 4, &len), OSK_ERR_ARG);
	T_RES(osk_store_find_tag(0, OSK_ITEM_LOGIN, tag_b, OSK_TAG_MAX + 1u, found, 4, &len),
	      OSK_ERR_ARG);

	osk_test_case("Eine zu lange Kennung gilt als keine");
	osk_item_meta_t bad;
	osk_item_id_t bad_id = 0;

	memset(&bad, 0, sizeof(bad));
	bad.type = OSK_ITEM_LOGIN;
	bad.profile = 0;
	bad.name = "Krumm";
	bad.site = "krumm.example";
	bad.tag_len = (uint8_t)(OSK_TAG_MAX + 1u);
	memcpy(bad.tag, tag_a, OSK_TAG_MAX);
	T_OK(osk_store_add(&bad, (const uint8_t *)"geheim", 6, &bad_id));

	osk_item_meta_t back;

	T_OK(osk_store_list(0, 0, &back));
	T_EQ(back.tag_len, 0);
}

static void case_power(void)
{
	uint8_t tag[OSK_TAG_MAX];
	osk_item_id_t id = 0;
	uint32_t steps;

	osk_test_case("Kennung nachtragen: Ausfall an jeder Schreibstelle");
	make_tag(tag, "spaet.example");

	t_device(0, 1u, T_PIN);
	T_OK(add_tagged("Konto", "spaet.example", "geheim", OSK_ITEM_LOGIN, NULL, 0, &id));
	osk_item_meta_t meta;

	memset(&meta, 0, sizeof(meta));
	meta.type = OSK_ITEM_LOGIN;
	meta.profile = 0;
	meta.name = "Konto";
	meta.site = "spaet.example";
	meta.tag_len = OSK_TAG_MAX;
	memcpy(meta.tag, tag, OSK_TAG_MAX);

	fake_flash_count_reset();
	T_OK(osk_store_update(id, &meta, NULL, 0));
	steps = fake_flash_writes();

	for (uint32_t at = 1; at <= steps + 1u; at++) {
		t_device(0, 1u, T_PIN);
		T_OK(add_tagged("Konto", "spaet.example", "geheim", OSK_ITEM_LOGIN, NULL, 0, &id));
		fake_crypto_nonce_reset();
		fake_flash_count_reset();
		fake_flash_fail_after(at);

		osk_result_t r = osk_store_update(id, &meta, NULL, 0);

		t_reboot();

		osk_item_id_t found[4];
		size_t len = 0;

		T_OK(osk_store_find_tag(0, OSK_ITEM_LOGIN, tag, OSK_TAG_MAX, found, 4, &len));
		/* Die Kennung ist ganz da oder gar nicht da - nie halb. */
		T_TRUE(len == 0u || len == 1u);
		if (r == OSK_OK) {
			T_EQ(len, 1);
		}
		if (len == 1u) {
			T_EQ(found[0], id);
		}
		/* Und das Geheimnis ist in jedem Fall unversehrt. */
		T_TRUE(t_secret_is(id, "geheim"));
		T_EQ(fake_crypto_nonce_dups(), 0);
	}
}

void test_tag(void)
{
	case_find();
	case_many();
	case_secret();
	case_change();
	case_power();
}
