/* Stromausfall an JEDER Schreibstelle.
 *
 * Das ist der wichtigste Pruefsatz des Tresors. Fuer jede Aenderung wird
 * zuerst gezaehlt, wie viele Schreib- und Loeschvorgaenge sie braucht. Dann
 * wird sie so oft wiederholt, wie es Stellen gibt - und jedes Mal faellt der
 * Strom an einer anderen Stelle aus, mitten im Schreiben.
 *
 * Danach wird das Geraet neu angemeldet und geprueft:
 *  - Der Tresor laesst sich oeffnen.
 *  - Es gilt der alte ODER der neue Zustand, nie etwas dazwischen.
 *  - Jeder Eintrag ist lesbar und sein Geheimnis unverfaelscht.
 *  - Keine Nonce wurde zweimal benutzt - auch nicht ueber den Ausfall hinweg.
 */

#include <string.h>
#include <stdio.h>
#include "osk_test.h"
#include "test_util.h"
#include "fake_flash.h"
#include "fake_crypto.h"
#include "fake_ui.h"
#include "store_internal.h"
#include "osk_store_profiles.h"

#define S1 "kennwort-eins"
#define S2 "kennwort-zwei"
#define S3 "kennwort-drei"

static osk_item_id_t g_id[3];

/* Drei Eintraege, ohne jede Stoerung. */
static void build(void)
{
	t_device(0, 1u, T_PIN);
	T_OK(t_add(0, "eins", "eins.example", S1, false, &g_id[0]));
	T_OK(t_add(0, "zwei", "zwei.example", S2, true, &g_id[1]));
	T_OK(t_add(0, "drei", "drei.example", S3, false, &g_id[2]));
}

/* Was nach jedem Ausfall gelten muss. */
static void check_intact(const char *want_old, const char *want_new, size_t want_count,
			 osk_result_t r)
{
	t_reboot();

	uint16_t v = 0;

	T_OK(osk_store_format_version(&v));
	T_EQ(v, OSK_FMT_CURRENT);
	T_EQ(t_count(0), want_count);

	/* Die unbeteiligten Eintraege sind immer unversehrt. */
	T_TRUE(t_secret_is(g_id[0], S1));
	T_TRUE(t_secret_is(g_id[2], S3));

	if (want_old != NULL && want_new != NULL) {
		bool old_there = t_secret_is(g_id[1], want_old);
		bool new_there = t_secret_is(g_id[1], want_new);

		/* Genau einer der beiden Zustaende - nichts dazwischen. */
		T_TRUE(old_there != new_there);
		if (r == OSK_OK) {
			T_TRUE(new_there);
		}
	}
	T_EQ(fake_crypto_nonce_dups(), 0);
	T_EQ(fake_crypto_vault_nonce_dups(), 0);

	size_t log_count = 0;

	T_OK(osk_log_count(&log_count));
	for (size_t i = 0; i < log_count; i++) {
		osk_log_entry_t e;
		T_OK(osk_log_get(i, &e));
	}
}

/* Wie viele Schreibstellen hat diese Aenderung? */
static uint32_t count_writes_update(void)
{
	build();
	fake_flash_count_reset();
	T_OK(osk_store_update(g_id[1], NULL, (const uint8_t *)"zaehllauf", 9));
	return fake_flash_writes();
}

static void case_update(void)
{
	uint32_t steps = count_writes_update();

	osk_test_case("Aendern: Ausfall an jeder Schreibstelle");
	T_TRUE(steps >= 3u);

	for (uint32_t at = 1; at <= steps + 1u; at++) {
		build();
		fake_crypto_nonce_reset();
		fake_flash_count_reset();
		fake_flash_fail_after(at);

		osk_result_t r = osk_store_update(g_id[1], NULL, (const uint8_t *)"NEU-ZWEI", 8);
		check_intact(S2, "NEU-ZWEI", 3, r);
	}
	printf("      %u Schreibstellen beim Aendern geprueft\n", steps + 1u);
}

static void case_add(void)
{
	uint32_t steps;

	build();
	fake_flash_count_reset();
	T_OK(t_add(0, "zaehllauf", "z.example", "x", false, NULL));
	steps = fake_flash_writes();

	osk_test_case("Anlegen: Ausfall an jeder Schreibstelle");
	for (uint32_t at = 1; at <= steps + 1u; at++) {
		build();
		fake_crypto_nonce_reset();
		fake_flash_count_reset();
		fake_flash_fail_after(at);

		osk_item_id_t id = 0;
		osk_result_t r = t_add(0, "vier", "vier.example", "kennwort-vier", false, &id);

		t_reboot();
		size_t n = t_count(0);

		/* Entweder drei oder vier Eintraege - nie ein halber. */
		T_TRUE(n == 3u || n == 4u);
		if (r == OSK_OK) {
			T_EQ(n, 4);
			T_TRUE(t_secret_is(id, "kennwort-vier"));
		}
		if (n == 4u) {
			osk_item_id_t found = t_find_name(0, "vier");

			T_TRUE(found != 0);
			T_TRUE(t_secret_is(found, "kennwort-vier"));
		}
		T_TRUE(t_secret_is(g_id[0], S1));
		T_TRUE(t_secret_is(g_id[1], S2));
		T_TRUE(t_secret_is(g_id[2], S3));
		T_EQ(fake_crypto_nonce_dups(), 0);
	}
	printf("      %u Schreibstellen beim Anlegen geprueft\n", steps + 1u);
}

static void case_delete(void)
{
	uint32_t steps;

	build();
	fake_flash_count_reset();
	T_OK(osk_store_delete(g_id[1]));
	steps = fake_flash_writes();

	osk_test_case("Loeschen: Ausfall an jeder Schreibstelle");
	for (uint32_t at = 1; at <= steps + 1u; at++) {
		build();
		fake_crypto_nonce_reset();
		fake_flash_count_reset();
		fake_flash_fail_after(at);

		osk_result_t r = osk_store_delete(g_id[1]);

		t_reboot();
		size_t n = t_count(0);

		T_TRUE(n == 2u || n == 3u);
		if (r == OSK_OK) {
			T_EQ(n, 2);
		}
		if (n == 3u) {
			/* Nicht geloescht heisst: noch vollstaendig lesbar. */
			T_TRUE(t_secret_is(g_id[1], S2));
		}
		T_TRUE(t_secret_is(g_id[0], S1));
		T_TRUE(t_secret_is(g_id[2], S3));
		T_EQ(fake_crypto_nonce_dups(), 0);
	}
	printf("      %u Schreibstellen beim Loeschen geprueft\n", steps + 1u);
}

static void case_profile(void)
{
	uint32_t steps;

	osk_test_case("Profil anlegen: Ausfall an jeder Schreibstelle");
	t_device(0, 1u, T_PIN);
	fake_flash_count_reset();
	T_OK(osk_store_profile_create(1, (const uint8_t *)T_PIN2, strlen(T_PIN2)));
	steps = fake_flash_writes();

	for (uint32_t at = 1; at <= steps + 1u; at++) {
		t_device(0, 1u, T_PIN);
		T_OK(t_add(0, "eins", "eins.example", S1, false, &g_id[0]));
		fake_crypto_nonce_reset();
		fake_flash_count_reset();
		fake_flash_fail_after(at);

		osk_result_t r =
		    osk_store_profile_create(1, (const uint8_t *)T_PIN2, strlen(T_PIN2));

		t_reboot();
		/* Profil 0 bleibt in jedem Fall heil. */
		T_EQ(t_count(0), 1);
		T_TRUE(t_secret_is(g_id[0], S1));

		/* Profil 1 gibt es ganz oder gar nicht. */
		bool exists = osk_store_profile_exists(1);

		if (r == OSK_OK) {
			T_TRUE(exists);
		}
		if (exists) {
			t_use(1u, T_PIN2);
			T_OK(osk_store_profile_unlock(1, (const uint8_t *)T_PIN2, strlen(T_PIN2)));
			T_EQ(t_count(1), 0);
			t_use(1u, T_PIN);
		}
		T_EQ(fake_crypto_nonce_dups(), 0);
	}
	printf("      %u Schreibstellen beim Profil geprueft\n", steps + 1u);
}

static void case_half_written_header(void)
{
	osk_test_case("Ein halber Kopfsatz gilt nicht");
	build();

	/* Von Hand: in den naechsten Kopfplatz wird die erste Haelfte eines
	 * Kopfsatzes geschrieben - genau das hinterlaesst ein Stromausfall
	 * mitten im Schreiben. Die Pruefsumme steht hinten und fehlt also. */
	osk_store_state_t *st = osk_store_state();
	osk_hdr_t fake = st->hdr;
	uint8_t page[OSK_HDR_SLOT];
	uint32_t next = st->hdr_slot + OSK_HDR_SLOT;

	fake.seq += 1000u;	 /* waere der jueengste Satz */
	fake.index_root[0] = 0u; /* und wuerde das Profil leer melden */
	fake.crc32 = 0xDEADBEEFu;

	memset(page, 0xFF, sizeof(page));
	memcpy(page, &fake, sizeof(fake) / 2u);
	T_OK(osk_flash_program(next, page, sizeof(page)));

	t_reboot();

	/* Der halbe Satz wird uebergangen, der alte gilt weiter. */
	T_EQ(t_count(0), 3);
	T_TRUE(t_secret_is(g_id[0], S1));
	T_TRUE(t_secret_is(g_id[1], S2));
	T_TRUE(t_secret_is(g_id[2], S3));
}

static void case_last_step(void)
{
	osk_test_case("Ausfall an der letzten Stelle: alt oder neu, nie halb");
	uint32_t steps = count_writes_update();

	for (uint32_t at = steps - 1u; at <= steps + 2u; at++) {
		build();
		fake_crypto_nonce_reset();
		fake_flash_count_reset();
		fake_flash_fail_after(at);

		osk_result_t r = osk_store_update(g_id[1], NULL, (const uint8_t *)"LETZTE", 6);
		check_intact(S2, "LETZTE", 3, r);
	}
}

void test_power(void)
{
	case_update();
	case_add();
	case_delete();
	case_profile();
	case_half_written_header();
	case_last_step();
}
