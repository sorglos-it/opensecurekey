/* Stick zu Stick.
 *
 * Beide Geraete liegen in einer eigenen Flash-Bank mit eigenem SE050. Der
 * Test schaltet zwischen ihnen um, so wie es in Wirklichkeit zwei Geraete
 * gleichzeitig tun. Der "Rechner in der Mitte" ist hier ein Puffer - er sieht
 * genau das, was auch ein Angreifer saehe. */

#include <string.h>
#include <stdio.h>
#include "osk_test.h"
#include "test_util.h"
#include "fake_flash.h"
#include "fake_crypto.h"
#include "fake_se050.h"
#include "fake_ui.h"
#include "store_internal.h"
#include "osk_store_profiles.h"
#include "osk_transfer.h"

#define WIRE_MAX (64u * 1024u)

static uint8_t g_wire[WIRE_MAX];
static size_t g_wire_len;
static size_t g_wire_pos;

static osk_result_t sink(void *ctx, const uint8_t *data, size_t len)
{
	(void)ctx;
	if (g_wire_len + len > sizeof(g_wire)) {
		return OSK_ERR_FULL;
	}
	memcpy(g_wire + g_wire_len, data, len);
	g_wire_len += len;
	return OSK_OK;
}

static osk_result_t src(void *ctx, uint8_t *data, size_t len)
{
	(void)ctx;
	if (g_wire_pos + len > g_wire_len) {
		return OSK_ERR_IO;
	}
	memcpy(data, g_wire + g_wire_pos, len);
	g_wire_pos += len;
	return OSK_OK;
}

/* Auf Geraet A umschalten - der Tresor wird neu angemeldet. */
static void to_a(void)
{
	osk_store_lock();
	fake_flash_select(0);
	t_use(1u, T_PIN);
	T_OK(osk_store_open());
	T_OK(osk_store_profile_unlock(0, (const uint8_t *)T_PIN,
				      strlen(T_PIN)));
}

static void to_b(void)
{
	osk_store_lock();
	fake_flash_select(1);
	t_use(2u, T_PIN2);
	T_OK(osk_store_open());
	T_OK(osk_store_profile_unlock(0, (const uint8_t *)T_PIN2,
				      strlen(T_PIN2)));
}

/* A bekommt drei Eintraege, davon zwei spiegelbar. B bleibt leer. */
static void build_pair(void)
{
	t_device(0, 1u, T_PIN);
	T_OK(t_add(0, "Bank", "bank.example", "kennwort-eins", true, NULL));
	T_OK(t_add(0, "Geheim", "intern.example", "bleibt-hier", false, NULL));
	T_OK(t_add(0, "Post", "post.example", "kennwort-zwei", true, NULL));

	t_device(1, 2u, T_PIN2);
	g_wire_len = 0;
	g_wire_pos = 0;
}

static void case_happy(void)
{
	osk_transfer_t ta, tb;
	uint8_t pub_a[OSK_TRANSFER_PUB_MAX], pub_b[OSK_TRANSFER_PUB_MAX];
	size_t len_a = 0, len_b = 0;
	char digits_a[OSK_TRANSFER_DIGITS + 1];
	size_t sent = 0, got = 0;

	osk_test_case("Beide Geraete zeigen dieselbe Zahl");
	build_pair();
	fake_ui_answer(true);

	to_a();
	T_OK(osk_transfer_begin(&ta));
	memcpy(pub_a, ta.own_pub, ta.own_pub_len);
	len_a = ta.own_pub_len;

	to_b();
	T_OK(osk_transfer_begin(&tb));
	memcpy(pub_b, tb.own_pub, tb.own_pub_len);
	len_b = tb.own_pub_len;
	T_TRUE(len_a > 0u && len_b > 0u);
	T_TRUE(memcmp(pub_a, pub_b, len_a) != 0);

	to_a();
	T_OK(osk_transfer_accept_peer(&ta, pub_b, len_b));
	T_EQ(strlen(ta.digits), OSK_TRANSFER_DIGITS);
	for (size_t i = 0; i < OSK_TRANSFER_DIGITS; i++) {
		T_TRUE(ta.digits[i] >= '0' && ta.digits[i] <= '9');
	}
	memcpy(digits_a, ta.digits, sizeof(digits_a));
	/* Die Anzeige hat die Zahl wirklich gezeigt. */
	T_EQ(fake_ui_last_kind(), OSK_ASK_PAIR);
	T_TRUE(fake_ui_last_detail() != NULL);
	T_STR(fake_ui_last_detail(), digits_a);

	to_b();
	T_OK(osk_transfer_accept_peer(&tb, pub_a, len_a));
	T_STR(tb.digits, digits_a);

	osk_test_case("Nur spiegelbare Eintraege gehen hinueber");
	to_a();
	T_OK(osk_transfer_send(&ta, sink, NULL, &sent));
	T_EQ(sent, 2);
	T_TRUE(g_wire_len > 0u);

	osk_test_case("Auf der Leitung steht nur Kryptotext");
	T_TRUE(memmem_missing(g_wire, g_wire_len, "kennwort-eins"));
	T_TRUE(memmem_missing(g_wire, g_wire_len, "bank.example"));
	T_TRUE(memmem_missing(g_wire, g_wire_len, "Bank"));

	osk_test_case("Das zweite Geraet hat danach dieselben Geheimnisse");
	to_b();
	g_wire_pos = 0;
	T_OK(osk_transfer_receive(&tb, src, NULL, &got));
	T_EQ(got, 2);
	T_EQ(t_count(0), 2);

	osk_item_id_t id = t_find_name(0, "Bank");

	T_TRUE(id != 0);
	T_TRUE(t_secret_is(id, "kennwort-eins"));
	id = t_find_name(0, "Post");
	T_TRUE(id != 0);
	T_TRUE(t_secret_is(id, "kennwort-zwei"));
	T_EQ(t_find_name(0, "Geheim"), 0);

	osk_test_case("Beim Zweitstick steht die Uebertragung im Protokoll");
	osk_log_entry_t e;
	size_t log_count = 0;
	bool found = false;

	T_OK(osk_log_count(&log_count));
	for (size_t i = 0; i < log_count; i++) {
		T_OK(osk_log_get(i, &e));
		if (e.what == OSK_LOG_TRANSFER) {
			found = true;
		}
	}
	T_TRUE(found);

	osk_test_case("Nach dem Neustart ist drueben alles noch da");
	osk_store_lock();
	fake_flash_select(1);
	t_use(2u, T_PIN2);
	T_OK(osk_store_open());
	T_OK(osk_store_profile_unlock(0, (const uint8_t *)T_PIN2,
				      strlen(T_PIN2)));
	T_EQ(t_count(0), 2);
	T_TRUE(t_secret_is(t_find_name(0, "Bank"), "kennwort-eins"));

	osk_test_case("Der fluechtige Schluessel wird weggeraeumt");
	osk_transfer_end(&tb);
	T_EQ(tb.confirmed, false);
	to_a();
	osk_transfer_end(&ta);
}

static void case_mitm(void)
{
	osk_transfer_t ta, tb, tm;
	uint8_t pub_a[OSK_TRANSFER_PUB_MAX], pub_b[OSK_TRANSFER_PUB_MAX];
	uint8_t pub_m[OSK_TRANSFER_PUB_MAX];
	size_t len_a, len_b, len_m;
	char digits_a[OSK_TRANSFER_DIGITS + 1];
	size_t sent = 0, got = 0;

	osk_test_case("Jemand in der Mitte: die Zahlen passen nicht zusammen");
	build_pair();
	fake_ui_answer(true);

	to_a();
	T_OK(osk_transfer_begin(&ta));
	memcpy(pub_a, ta.own_pub, ta.own_pub_len);
	len_a = ta.own_pub_len;

	to_b();
	T_OK(osk_transfer_begin(&tb));
	memcpy(pub_b, tb.own_pub, tb.own_pub_len);
	len_b = tb.own_pub_len;

	/* Der Angreifer schiebt einen eigenen Schluessel dazwischen. Dafuer
	 * genuegt hier ein drittes "Geraet". */
	osk_store_lock();
	fake_flash_select(1);
	t_use(99u, T_PIN2);
	T_OK(osk_store_open());
	T_OK(osk_transfer_begin(&tm));
	memcpy(pub_m, tm.own_pub, tm.own_pub_len);
	len_m = tm.own_pub_len;

	to_a();
	T_OK(osk_transfer_accept_peer(&ta, pub_m, len_m));
	memcpy(digits_a, ta.digits, sizeof(digits_a));

	to_b();
	T_OK(osk_transfer_accept_peer(&tb, pub_a, len_a));

	/* Genau das sieht der Nutzer: zwei verschiedene Zahlen. */
	T_TRUE(strcmp(digits_a, tb.digits) != 0);

	osk_test_case("Und die Stuecke passen nicht zum anderen Schluessel");
	to_a();
	g_wire_len = 0;
	T_OK(osk_transfer_send(&ta, sink, NULL, &sent));
	to_b();
	g_wire_pos = 0;
	T_RES(osk_transfer_receive(&tb, src, NULL, &got), OSK_ERR_DENIED);
	T_EQ(t_count(0), 0);

	osk_test_case("Der eigene Schluessel zurueckgespiegelt wird abgelehnt");
	to_a();
	osk_transfer_end(&ta);
	T_OK(osk_transfer_begin(&ta));
	T_RES(osk_transfer_accept_peer(&ta, ta.own_pub, ta.own_pub_len),
	      OSK_ERR_DENIED);
	osk_transfer_end(&ta);
	(void)len_b;
	(void)pub_b;
}

static void case_no_confirm(void)
{
	osk_transfer_t ta, tb;
	uint8_t pub_b[OSK_TRANSFER_PUB_MAX];
	size_t len_b, sent = 0;

	osk_test_case("Ohne Druck auf den Knopf geht kein Byte hinaus");
	build_pair();

	to_b();
	fake_ui_answer(true);
	T_OK(osk_transfer_begin(&tb));
	memcpy(pub_b, tb.own_pub, tb.own_pub_len);
	len_b = tb.own_pub_len;

	to_a();
	T_OK(osk_transfer_begin(&ta));
	fake_ui_answer(false);
	T_RES(osk_transfer_accept_peer(&ta, pub_b, len_b), OSK_ERR_DENIED);
	T_EQ(ta.confirmed, false);

	g_wire_len = 0;
	T_RES(osk_transfer_send(&ta, sink, NULL, &sent), OSK_ERR_DENIED);
	T_EQ(g_wire_len, 0);

	osk_test_case("Die Ablehnung steht im Protokoll");
	fake_ui_answer(true);
	osk_log_entry_t e;

	T_OK(osk_log_get(0, &e));
	T_EQ(e.what, OSK_LOG_DENY);
	osk_transfer_end(&ta);
}

static void case_damaged_wire(void)
{
	osk_transfer_t ta, tb;
	uint8_t pub_a[OSK_TRANSFER_PUB_MAX], pub_b[OSK_TRANSFER_PUB_MAX];
	size_t len_a, len_b, sent = 0, got = 0;

	osk_test_case("Eine abgeschnittene Uebertragung wird nicht uebernommen");
	build_pair();
	fake_ui_answer(true);

	to_a();
	T_OK(osk_transfer_begin(&ta));
	memcpy(pub_a, ta.own_pub, ta.own_pub_len);
	len_a = ta.own_pub_len;
	to_b();
	T_OK(osk_transfer_begin(&tb));
	memcpy(pub_b, tb.own_pub, tb.own_pub_len);
	len_b = tb.own_pub_len;
	to_a();
	T_OK(osk_transfer_accept_peer(&ta, pub_b, len_b));
	to_b();
	T_OK(osk_transfer_accept_peer(&tb, pub_a, len_a));

	to_a();
	g_wire_len = 0;
	T_OK(osk_transfer_send(&ta, sink, NULL, &sent));
	g_wire_len -= 60u;   /* das Abschlussstueck fehlt */

	to_b();
	g_wire_pos = 0;
	T_TRUE(osk_transfer_receive(&tb, src, NULL, &got) != OSK_OK);
	osk_transfer_end(&tb);
}

void test_transfer(void)
{
	case_happy();
	case_mitm();
	case_no_confirm();
	case_damaged_wire();
}
