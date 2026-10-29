/* Die Wisch-PIN: eine zweite PIN, deren Eingabe den Tresor loescht.
 *
 * Drei Dinge daran sind der eigentliche Gegenstand dieses Pruefsatzes:
 *  - sie loescht wirklich, und zwar die Geheimnisse im Flash und nicht nur
 *    den Index
 *  - sie sieht von aussen aus wie eine falsche PIN
 *  - sie ist KEIN Weg um den Rueckdrehschutz herum */

#include <string.h>
#include "osk_test.h"
#include "test_util.h"
#include "fake_flash.h"
#include "fake_se050.h"
#include "osk_store.h"
#include "osk_store_profiles.h"
#include "osk_vault_format.h"
#include "osk_flash.h"
#include "fake_se050.h"

#define WIPE_PIN "loeschmich"

void test_wipe(void)
{
	osk_item_id_t id = 0;
	uint64_t gen_before = 0, gen_after = 0;

	osk_test_case("Ohne gesetzte Wisch-PIN gibt es keine");
	t_device(0, 1, T_PIN);
	T_TRUE(!osk_store_wipe_pin_is_set(0));
	T_RES(osk_store_profile_unlock(0, (const uint8_t *)WIPE_PIN, strlen(WIPE_PIN)),
	      OSK_ERR_DENIED);
	/* Der Tresor steht noch, denn es gab nichts zu loeschen. */
	T_OK(osk_store_profile_unlock(0, (const uint8_t *)T_PIN, strlen(T_PIN)));

	osk_test_case("Setzen braucht das offene Profil");
	T_OK(osk_store_wipe_pin_set(0, (const uint8_t *)WIPE_PIN, strlen(WIPE_PIN)));
	T_TRUE(osk_store_wipe_pin_is_set(0));

	osk_test_case("Die echte PIN darf nicht zugleich die Wisch-PIN sein");
	T_RES(osk_store_wipe_pin_set(0, (const uint8_t *)T_PIN, strlen(T_PIN)), OSK_ERR_ARG);

	osk_test_case("Zu kurz ist keine PIN");
	T_RES(osk_store_wipe_pin_set(0, (const uint8_t *)"abc", 3), OSK_ERR_ARG);

	osk_test_case("Sie ueberlebt den Neustart");
	T_OK(t_add(0, "A", "a.example", "geheim-a", 0, &id));
	T_OK(osk_store_generation(&gen_before));
	t_reboot();
	T_TRUE(osk_store_wipe_pin_is_set(0));
	T_TRUE(t_secret_is(id, "geheim-a"));

	osk_test_case("Ihre Eingabe sieht aus wie eine falsche PIN");
	{
		uint8_t tries = fake_se050_tries_left();

		osk_store_lock();
		fake_flash_revive();
		T_OK(osk_store_open());
		T_RES(osk_store_profile_unlock(0, (const uint8_t *)WIPE_PIN, strlen(WIPE_PIN)),
		      OSK_ERR_DENIED);
		/* Kein Fehlversuch im Chip: sonst liesse sich das Geraet nach
		 * ein paar Loeschversuchen nicht einmal mehr loeschen. */
		T_EQ(fake_se050_tries_left(), tries);
	}

	osk_test_case("Danach ist der Tresor wirklich leer");
	{
		osk_store_lock();
		fake_flash_revive();
		T_OK(osk_store_open());
		T_TRUE(!osk_store_profile_exists(0));
		T_RES(osk_store_profile_unlock(0, (const uint8_t *)T_PIN, strlen(T_PIN)),
		      OSK_ERR_NOT_FOUND);
	}

	osk_test_case("Und das Geheimnis steht nicht mehr im Flash");
	{
		uint8_t buf[OSK_SECTOR];
		bool found = false;

		for (uint32_t s = 0; s < OSK_AREA_BLOB_LEN && !found; s += OSK_SECTOR) {
			T_OK(osk_flash_read(OSK_AREA_BLOB_OFF + s, buf, sizeof(buf)));
			found = !memmem_missing(buf, sizeof(buf), "geheim-a");
		}
		T_TRUE(!found);
	}

	osk_test_case("Loeschen ist kein Weg um den Rueckdrehschutz herum");
	{
		/* Das ist der Punkt, an dem eine Wisch-PIN gefaehrlich waere:
		 * wenn der frische Tresor bei null anfinge, waere loeschen
		 * und einen alten Stand zurueckspielen der Weg zurueck. */
		T_OK(osk_store_generation(&gen_after));
		T_TRUE(gen_after >= gen_before);
		T_EQ(fake_se050_counter_peek(OSK_KEYID_VAULT_GEN), gen_after);
	}

	osk_test_case("Abschaltbar: geloescht wird dann nichts mehr");
	{
		t_device(0, 2, T_PIN);
		T_OK(osk_store_wipe_pin_set(0, (const uint8_t *)WIPE_PIN, strlen(WIPE_PIN)));
		T_OK(osk_store_wipe_pin_clear(0));
		T_TRUE(!osk_store_wipe_pin_is_set(0));

		T_OK(t_add(0, "A", "a.example", "geheim-a", 0, &id));
		osk_store_lock();
		T_OK(osk_store_open());
		T_RES(osk_store_profile_unlock(0, (const uint8_t *)WIPE_PIN, strlen(WIPE_PIN)),
		      OSK_ERR_DENIED);
		T_OK(osk_store_profile_unlock(0, (const uint8_t *)T_PIN, strlen(T_PIN)));
		T_TRUE(t_secret_is(id, "geheim-a"));
	}

	osk_test_case("Die Wisch-PIN eines Profils loescht nicht bei einem anderen");
	{
		t_device(0, 3, T_PIN);
		T_OK(osk_store_profile_create(1, (const uint8_t *)T_PIN2, strlen(T_PIN2)));
		T_OK(osk_store_wipe_pin_set(0, (const uint8_t *)WIPE_PIN, strlen(WIPE_PIN)));
		T_OK(t_add(0, "A", "a.example", "geheim-a", 0, &id));

		osk_store_lock();
		T_OK(osk_store_open());
		/* Dieselbe Zeichenfolge, aber gegen Profil 1 - dort ist keine
		 * Wisch-PIN gesetzt, also passiert nichts. */
		fake_se050_set_pin(T_PIN2);
		T_RES(osk_store_profile_unlock(1, (const uint8_t *)WIPE_PIN, strlen(WIPE_PIN)),
		      OSK_ERR_DENIED);
		fake_se050_set_pin(T_PIN);
		T_OK(osk_store_profile_unlock(0, (const uint8_t *)T_PIN, strlen(T_PIN)));
		T_TRUE(t_secret_is(id, "geheim-a"));
	}

	osk_store_lock();
}
