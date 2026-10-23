/* Grundlagen: Format, Profile, Eintraege, Freigabe, Nonce-Zaehler. */

#include <string.h>
#include "osk_test.h"
#include "test_util.h"
#include "fake_flash.h"
#include "fake_crypto.h"
#include "fake_se050.h"
#include "fake_ui.h"
#include "store_internal.h"
#include "osk_store_profiles.h"

static void case_format(void)
{
	osk_test_case("Leeres Flash ergibt einen Tresor der jetzigen Fassung");
	t_device(0, 1u, T_PIN);

	uint16_t v = 0;

	T_OK(osk_store_format_version(&v));
	T_EQ(v, OSK_FMT_CURRENT);
	T_TRUE(osk_store_is_open());
	T_EQ(t_count(0), 0);

	osk_test_case("Gesperrt antwortet nichts");
	osk_store_lock();
	T_TRUE(!osk_store_is_open());
	size_t n = 0;

	T_RES(osk_store_count(0, &n), OSK_ERR_LOCKED);
}

static void case_profiles(void)
{
	osk_test_case("Profile haben getrennte PINs");
	t_device(0, 1u, T_PIN);

	/* Zweites Profil mit eigener PIN. Die PIN des Chips ist dieselbe -
	 * getrennt sind die Schluessel. */
	fake_se050_set_pin(T_PIN2);
	T_OK(osk_store_profile_create(1, (const uint8_t *)T_PIN2, strlen(T_PIN2)));
	T_OK(t_add(1, "zweites", "b.example", "geheim-b", false, NULL));
	fake_se050_set_pin(T_PIN);
	T_OK(t_add(0, "erstes", "a.example", "geheim-a", false, NULL));

	T_EQ(t_count(0), 1);
	T_EQ(t_count(1), 1);

	osk_test_case("Ein gesperrtes Profil gibt nichts heraus");
	osk_store_profile_lock(1);
	size_t n = 0;

	T_RES(osk_store_count(1, &n), OSK_ERR_DENIED);

	osk_test_case("Die PIN eines Profils oeffnet das andere nicht");
	fake_se050_set_pin(T_PIN);
	/* Die PIN des Chips stimmt, der abgeleitete Schluessel aber nicht -
	 * der Pruefwert der Verschluesselung schlaegt an. */
	T_RES(osk_store_profile_unlock(1, (const uint8_t *)T_PIN, strlen(T_PIN)), OSK_ERR_DENIED);

	osk_test_case("Falsche PIN wird vom Chip gezaehlt");
	uint8_t before = fake_se050_tries_left();

	T_RES(osk_store_profile_unlock(0, (const uint8_t *)"falschfalsch", 12), OSK_ERR_DENIED);
	T_EQ(fake_se050_tries_left(), before - 1u);

	osk_test_case("Zu kurze PIN wird abgelehnt");
	T_RES(osk_store_profile_create(2, (const uint8_t *)"12345", 5), OSK_ERR_ARG);
}

static void case_items(void)
{
	osk_item_id_t id1 = 0, id2 = 0;

	osk_test_case("Anlegen, auflisten, suchen");
	t_device(0, 1u, T_PIN);
	T_OK(t_add(0, "Bank", "login.bank.example", "kennwort-eins", true, &id1));
	T_OK(t_add(0, "Post", "mail.post.example", "kennwort-zwei", false, &id2));
	T_EQ(t_count(0), 2);
	T_TRUE(id1 != id2);
	T_TRUE(id1 != 0 && id2 != 0);

	osk_item_meta_t meta;

	T_OK(osk_store_list(0, 0, &meta));
	T_STR(meta.name, "Bank");
	T_STR(meta.site, "login.bank.example");
	T_TRUE(meta.mirrored);
	T_EQ(meta.changed_utc, 1760000000ull);

	osk_item_id_t found[4];
	size_t len = 0;

	T_OK(osk_store_find(0, "login.bank.example", found, 4, &len));
	T_EQ(len, 1);
	T_EQ(found[0], id1);

	osk_test_case("Unterdomaene passt, aehnlicher Name nicht");
	T_OK(osk_store_find(0, "sub.login.bank.example", found, 4, &len));
	T_EQ(len, 1);
	T_OK(osk_store_find(0, "boeselogin.bank.example", found, 4, &len));
	T_EQ(len, 0);

	osk_test_case("Geheimnis kommt unveraendert zurueck");
	T_TRUE(t_secret_is(id1, "kennwort-eins"));
	T_TRUE(t_secret_is(id2, "kennwort-zwei"));

	osk_test_case("Aendern laesst das andere Geheimnis in Ruhe");
	T_OK(osk_store_update(id1, NULL, (const uint8_t *)"neues-kennwort", 14));
	T_TRUE(t_secret_is(id1, "neues-kennwort"));
	T_TRUE(t_secret_is(id2, "kennwort-zwei"));

	osk_test_case("Nach dem Neustart ist alles noch da");
	t_reboot();
	T_EQ(t_count(0), 2);
	T_TRUE(t_secret_is(id1, "neues-kennwort"));
	T_TRUE(t_secret_is(id2, "kennwort-zwei"));

	osk_test_case("Loeschen nimmt nur den einen Eintrag");
	T_OK(osk_store_delete(id2));
	T_EQ(t_count(0), 1);
	T_TRUE(t_secret_is(id1, "neues-kennwort"));
	uint8_t buf[64];
	size_t got = 0;

	T_RES(osk_store_reveal(id2, "test", buf, sizeof(buf), &got), OSK_ERR_NOT_FOUND);

	osk_test_case("Eine Kennung wird nie wieder vergeben");
	osk_item_id_t id3 = 0;

	T_OK(t_add(0, "Neu", "neu.example", "x", false, &id3));
	T_TRUE(id3 != id2);
	T_TRUE(id3 > id1);
}

static void case_readonly(void)
{
	osk_item_id_t id = 0;

	osk_test_case("Schreibgeschuetzt heisst nicht aenderbar");
	t_device(0, 1u, T_PIN);
	T_OK(t_add(0, "Firmenschluessel", "vpn.example", "unantastbar", false, &id));
	T_OK(osk_store_item_set_readonly(id, true));

	uint8_t flags = 0;

	T_OK(osk_store_item_flags(id, &flags));
	T_TRUE((flags & OSK_FLAG_READONLY) != 0u);

	T_RES(osk_store_update(id, NULL, (const uint8_t *)"anders", 6), OSK_ERR_DENIED);
	T_RES(osk_store_delete(id), OSK_ERR_DENIED);
	T_TRUE(t_secret_is(id, "unantastbar"));

	osk_test_case("Der Schutz haelt den Neustart aus");
	t_reboot();
	T_OK(osk_store_item_flags(id, &flags));
	T_TRUE((flags & OSK_FLAG_READONLY) != 0u);

	osk_test_case("Aufgehoben laesst er sich aendern");
	T_OK(osk_store_item_set_readonly(id, false));
	T_OK(osk_store_update(id, NULL, (const uint8_t *)"anders", 6));
	T_TRUE(t_secret_is(id, "anders"));
}

static void case_counter(void)
{
	osk_item_id_t id = 0;
	uint64_t c = 0;

	osk_test_case("HOTP-Zaehler waechst und haelt");
	t_device(0, 1u, T_PIN);
	T_OK(t_add(0, "HOTP", "hotp.example", "saat", false, &id));
	T_OK(osk_store_item_counter(id, &c));
	T_EQ(c, 0);
	T_OK(osk_store_item_counter_next(id, &c));
	T_EQ(c, 1);
	T_OK(osk_store_item_counter_next(id, &c));
	T_EQ(c, 2);
	t_reboot();
	T_OK(osk_store_item_counter(id, &c));
	T_EQ(c, 2);
}

static void case_reveal(void)
{
	osk_item_id_t id = 0;
	uint8_t buf[OSK_SECRET_MAX];
	size_t len = 0;

	osk_test_case("Ohne Freigabe kommt kein Geheimnis heraus");
	t_device(0, 1u, T_PIN);
	T_OK(t_add(0, "Bank", "bank.example", "zwoelfzeichen", false, &id));

	fake_ui_reset();
	fake_ui_answer(false);
	memset(buf, 0xAA, sizeof(buf));
	len = 12345u;
	T_RES(osk_store_reveal(id, "firefox", buf, sizeof(buf), &len), OSK_ERR_DENIED);
	T_EQ(len, 0);
	T_EQ(fake_ui_asks(), 1);
	/* Der Puffer bleibt unberuehrt - kein gekuerztes Geheimnis. */
	for (size_t i = 0; i < sizeof(buf); i++) {
		if (buf[i] != 0xAAu) {
			T_TRUE(false);
			break;
		}
	}

	osk_test_case("Auch die Laenge verraet ohne Freigabe nichts");
	/* Ein zu kleiner Puffer darf NICHT vor der Frage auffallen - sonst
	 * laesst sich die Laenge des Geheimnisses durch Probieren ermitteln. */
	fake_ui_reset();
	fake_ui_answer(false);
	T_RES(osk_store_reveal(id, "firefox", buf, 1, &len), OSK_ERR_DENIED);
	T_EQ(fake_ui_asks(), 1);

	osk_test_case("Mit Freigabe, aber zu kleinem Puffer: kein Inhalt");
	fake_ui_reset();
	fake_ui_answer(true);
	T_RES(osk_store_reveal(id, "firefox", buf, 1, &len), OSK_ERR_ARG);
	T_EQ(len, 0);

	osk_test_case("Die Anzeige nennt Name und Adresse");
	fake_ui_reset();
	fake_ui_answer(true);
	T_OK(osk_store_reveal(id, "firefox", buf, sizeof(buf), &len));
	T_EQ(len, 13);
	T_EQ(fake_ui_last_kind(), OSK_ASK_SECRET);
	T_STR(fake_ui_last_subject(), "Bank");
	T_TRUE(fake_ui_last_detail() != NULL);
	T_STR(fake_ui_last_detail(), "bank.example");

	osk_test_case("Jeder Zugriff steht im Protokoll, mit Fragesteller");
	size_t count = 0;

	T_OK(osk_log_count(&count));
	T_TRUE(count > 0);

	osk_log_entry_t e;

	T_OK(osk_log_get(0, &e));
	T_EQ(e.what, OSK_LOG_REVEAL);
	T_EQ(e.item_id, id);
	T_STR(e.asked_by, "firefox");

	osk_test_case("Auch die Ablehnung steht im Protokoll");
	fake_ui_answer(false);
	T_RES(osk_store_reveal(id, "schadcode", buf, sizeof(buf), &len), OSK_ERR_DENIED);
	T_OK(osk_log_get(0, &e));
	T_EQ(e.what, OSK_LOG_REVEAL_DENY);
	T_STR(e.asked_by, "schadcode");

	osk_test_case("Im Protokoll steht nie ein Geheimnis");
	T_OK(osk_log_count(&count));
	for (size_t i = 0; i < count; i++) {
		T_OK(osk_log_get(i, &e));
		T_TRUE(strstr(e.asked_by, "zwoelfzeichen") == NULL);
	}
}

static void case_log_ring(void)
{
	osk_test_case("Das Protokoll laeuft im Kreis und bleibt lesbar");
	t_device(0, 1u, T_PIN);

	for (int i = 0; i < 600; i++) {
		osk_log_entry_t e;

		memset(&e, 0, sizeof(e));
		e.when_utc = 1760000000ull + (uint64_t)i;
		e.what = OSK_LOG_OPEN;
		e.item_id = (uint32_t)i;
		T_OK(osk_log_add(&e));
	}
	size_t count = 0;

	T_OK(osk_log_count(&count));
	T_TRUE(count > 400);
	T_TRUE(count <= OSK_LOG_SLOTS);

	/* Jeder gezaehlte Satz muss wirklich lesbar sein. */
	for (size_t i = 0; i < count; i++) {
		osk_log_entry_t e;

		T_OK(osk_log_get(i, &e));
	}
	osk_log_entry_t newest;

	T_OK(osk_log_get(0, &newest));
	T_EQ(newest.item_id, 599);
}

static void case_nonce(void)
{
	osk_item_id_t id = 0;

	osk_test_case("Keine Nonce wird zweimal benutzt");
	t_device(0, 1u, T_PIN);
	fake_crypto_nonce_reset();

	T_OK(t_add(0, "A", "a.example", "aaa", false, &id));
	for (int i = 0; i < 80; i++) {
		char secret[32];

		snprintf(secret, sizeof(secret), "kennwort-%d", i);
		T_OK(osk_store_update(id, NULL, (const uint8_t *)secret, strlen(secret)));
	}
	T_TRUE(fake_crypto_nonce_count() > 100);
	T_EQ(fake_crypto_nonce_dups(), 0);
	T_EQ(fake_crypto_vault_nonce_dups(), 0);

	osk_test_case("Der Zaehler springt beim Neustart nach vorn");
	osk_store_state_t *st = osk_store_state();
	uint64_t persisted = st->hdr.nonce_next;
	uint64_t used = st->nonce_cur;

	T_TRUE(persisted >= used);
	t_reboot();
	/* Alles unterhalb des festgehaltenen Werts gilt als verbraucht. */
	T_TRUE(osk_store_state()->nonce_cur >= persisted);

	osk_test_case("Auch ueber viele Neustarts bleibt sie einmalig");
	for (int i = 0; i < 20; i++) {
		char secret[32];

		snprintf(secret, sizeof(secret), "nach-neustart-%d", i);
		T_OK(osk_store_update(id, NULL, (const uint8_t *)secret, strlen(secret)));
		t_reboot();
	}
	T_EQ(fake_crypto_nonce_dups(), 0);
	T_EQ(fake_crypto_vault_nonce_dups(), 0);
	T_TRUE(t_secret_is(id, "nach-neustart-19"));
}

static void case_limits(void)
{
	osk_test_case("Ein Profil ist voll, wenn es voll ist");
	t_device(0, 1u, T_PIN);

	for (unsigned i = 0; i < OSK_ITEMS_MAX; i++) {
		char name[32];

		snprintf(name, sizeof(name), "eintrag-%u", i);
		T_OK(t_add(0, name, "x.example", "geheim", false, NULL));
	}
	T_EQ(t_count(0), OSK_ITEMS_MAX);
	T_RES(t_add(0, "einer-zuviel", "x.example", "geheim", false, NULL), OSK_ERR_FULL);

	osk_test_case("Zu langes Geheimnis wird abgelehnt");
	static uint8_t big[OSK_SECRET_MAX + 1];
	osk_item_meta_t meta;
	osk_item_id_t id = 0;

	memset(big, 'x', sizeof(big));
	memset(&meta, 0, sizeof(meta));
	meta.type = OSK_ITEM_NOTE;
	meta.profile = 0;
	meta.name = "zu-lang";
	meta.site = "";
	T_RES(osk_store_add(&meta, big, sizeof(big), &id), OSK_ERR_ARG);

	osk_test_case("Ein Geheimnis in voller Laenge geht");
	t_device(0, 1u, T_PIN);
	T_OK(osk_store_add(&meta, big, OSK_SECRET_MAX, &id));
	uint8_t out[OSK_SECRET_MAX];
	size_t len = 0;

	fake_ui_answer(true);
	T_OK(osk_store_reveal(id, "test", out, sizeof(out), &len));
	T_EQ(len, OSK_SECRET_MAX);
	T_MEM(out, big, OSK_SECRET_MAX);
}

void test_core(void)
{
	case_format();
	case_profiles();
	case_items();
	case_readonly();
	case_counter();
	case_reveal();
	case_log_ring();
	case_nonce();
	case_limits();
}
