/* Rueckdrehschutz: der Tresor gegen einen zurueckgespielten Zustand.
 *
 * Der eigentliche Angriff ist NICHT ein verfaelschter Flash - gegen den hilft
 * die Verschluesselung schon, und ein Pruefsatz, der nur Unsinn vorlegt,
 * prueft ihn nicht. Der Angriff ist ein in sich VOELLIG GUELTIGER alter
 * Zustand: richtige Pruefsummen, richtige Pruefwerte, richtiges Format, von
 * genau diesem Geraet, nur aelter.
 *
 * Darum wird hier zuerst BEWIESEN, dass das zurueckgespielte Abbild in sich
 * gueltig ist - es wird einmal zurueckgespielt, waehrend der Chipzaehler noch
 * passt, und der Tresor oeffnet und liest. Erst danach wird dasselbe Abbild
 * gegen einen weitergelaufenen Chipzaehler gehalten. Ohne diesen ersten
 * Schritt koennte die Abweisung im zweiten auch von einem kaputten Abbild
 * kommen, und der Pruefsatz haette etwas anderes gemessen, als er behauptet. */

#include <string.h>
#include "osk_test.h"
#include "test_util.h"
#include "fake_flash.h"
#include "fake_se050.h"
#include "osk_store.h"
#include "osk_store_profiles.h"
#include "osk_vault_format.h"
#include "osk_se050.h"

/* Oeffnen und aufschliessen wie t_reboot, aber OHNE Erwartung an das
 * Ergebnis - hier soll das Oeffnen ja gerade scheitern duerfen. */
static osk_result_t reopen(void)
{
	osk_store_lock();
	fake_flash_revive();
	return osk_store_open();
}

void test_guard(void)
{
	osk_item_id_t id_a = 0, id_b = 0;
	uint64_t gen_s = 0, gen = 0;

	/* ---- Der Anker liegt im Chip, nicht im Flash ---------------- */

	osk_test_case("Beim ersten Oeffnen entsteht der Zaehler im Chip");
	t_device(0, 1, T_PIN);
	T_TRUE(fake_se050_counter_peek(OSK_KEYID_VAULT_GEN) != (uint64_t)-1);

	osk_test_case("Jedes Festschreiben hebt den Zaehler mit an");
	T_OK(t_add(0, "A", "a.example", "geheim-a", 0, &id_a));
	T_OK(osk_store_generation(&gen_s));
	T_EQ(fake_se050_counter_peek(OSK_KEYID_VAULT_GEN), gen_s);
	T_TRUE(gen_s > 0u);

	/* ---- Erst der Beleg: das Abbild ist in sich gueltig ---------- */

	fake_flash_snapshot();

	osk_test_case("Das gesicherte Abbild ist in sich gueltig");
	fake_flash_rollback();
	T_OK(reopen());
	T_OK(osk_store_profile_unlock(0, (const uint8_t *)T_PIN, strlen(T_PIN)));
	T_TRUE(t_secret_is(id_a, "geheim-a"));

	/* ---- Dann der Angriff: dasselbe Abbild, Chip weitergelaufen -- */

	osk_test_case("Der Tresor laeuft weiter, der Zaehler mit");
	T_OK(t_add(0, "B", "b.example", "geheim-b", 0, &id_b));
	T_OK(osk_store_generation(&gen));
	T_TRUE(gen > gen_s);
	T_EQ(fake_se050_counter_peek(OSK_KEYID_VAULT_GEN), gen);

	osk_test_case("Der gueltige alte Zustand wird abgewiesen");
	fake_flash_rollback();
	T_RES(reopen(), OSK_ERR_ROLLBACK);
	T_TRUE(!osk_store_is_open());

	osk_test_case("Und er bleibt abgewiesen, auch beim zweiten Versuch");
	T_RES(reopen(), OSK_ERR_ROLLBACK);

	osk_test_case("Der Zaehler im Chip ist dabei nicht gefallen");
	T_EQ(fake_se050_counter_peek(OSK_KEYID_VAULT_GEN), gen);

	/* ---- Die Gegenrichtung: der Chip darf hinterherhinken -------- */

	osk_test_case("Nach einem Stromausfall holt der Chip nach, der Tresor oeffnet");
	t_device(0, 2, T_PIN);
	T_OK(t_add(0, "A", "a.example", "geheim-a", 0, &id_a));
	{
		uint64_t before = fake_se050_counter_peek(OSK_KEYID_VAULT_GEN);
		uint32_t n;
		bool torn = false;

		/* Eine Aenderung, an einer Schreibstelle zerrissen. Faellt sie
		 * vor dem Kopfsatz aus, aendert sich nichts; faellt sie
		 * danach aus, steht im Flash mehr als im Chip - und genau das
		 * ist die Lage, die ein Stromausfall zwischen Kopfsatz und
		 * Anheben hinterlaesst. */
		for (n = 1; n <= 40u && !torn; n++) {
			fake_flash_fail_after(n);
			(void)t_add(0, "B", "b.example", "geheim-b", 0, &id_b);
			fake_flash_fail_after(0);
			fake_flash_revive();

			if (reopen() == OSK_OK) {
				uint64_t after = fake_se050_counter_peek(OSK_KEYID_VAULT_GEN);
				uint64_t g = 0;

				T_OK(osk_store_generation(&g));
				/* Nach dem Oeffnen stehen beide gleich: der
				 * Chip ist nachgezogen worden. Kleiner als
				 * der Flash darf er jetzt nicht mehr sein. */
				T_TRUE(after >= before);
				T_EQ(after, g);
				torn = true;
			}
		}
		T_TRUE(torn);
	}

	/* ---- Loeschen ist kein Weg zurueck --------------------------- */

	osk_test_case("Ein frischer Tresor faengt ueber dem Chipzaehler an");
	{
		uint64_t anchor;

		t_device(0, 3, T_PIN);
		T_OK(t_add(0, "A", "a.example", "geheim-a", 0, NULL));
		anchor = fake_se050_counter_peek(OSK_KEYID_VAULT_GEN);

		/* Flash leeren, als haette jemand den Chip ausgeloetet und das
		 * Flash neu beschrieben. Der Zaehler bleibt, wo er ist. */
		osk_store_lock();
		fake_flash_wipe();
		T_OK(osk_store_open());
		T_OK(osk_store_generation(&gen));
		T_TRUE(gen >= anchor);
		T_TRUE(fake_se050_counter_peek(OSK_KEYID_VAULT_GEN) >= anchor);
	}

	osk_test_case("Der Anfangswert eines frischen Zaehlers wird gelesen, nicht angenommen");
	{
		/* In AN12413 steht nicht, womit ein frisch angelegter Zaehler
		 * beginnt. Ein Tresor, der heimlich null annimmt, faellt hier
		 * auf: der Chip faengt bei 5000 an, und die Folgenummer im
		 * Kopf muss darueber liegen. */
		osk_store_lock();
		fake_flash_select(0);
		fake_flash_wipe();
		fake_se050_reset();
		fake_se050_counter_start_at(5000u);
		t_use(4u, T_PIN);
		T_OK(osk_store_open());
		T_OK(osk_store_generation(&gen));
		T_TRUE(gen >= 5000u);
		T_EQ(fake_se050_counter_peek(OSK_KEYID_VAULT_GEN), gen);
		fake_se050_counter_start_at(0u);
	}

	osk_test_case("Der Zaehler laesst sich nicht senken");
	{
		uint64_t now = fake_se050_counter_peek(OSK_KEYID_VAULT_GEN);

		/* Wiederholbar und ohne Wirkung: genau das braucht der Tresor
		 * nach einem Stromausfall mitten im Schreiben. */
		T_OK(osk_se050_counter_raise(OSK_KEYID_VAULT_GEN, 1u));
		T_EQ(fake_se050_counter_peek(OSK_KEYID_VAULT_GEN), now);
		T_OK(osk_se050_counter_raise(OSK_KEYID_VAULT_GEN, now));
		T_EQ(fake_se050_counter_peek(OSK_KEYID_VAULT_GEN), now);
	}

	osk_store_lock();
}
