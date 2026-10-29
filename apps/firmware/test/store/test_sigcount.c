/* Der Signaturzaehler von FIDO2.
 *
 * Gezaehlt wird in CTAP (Strang B), gespeichert hier. Der eine Fehler, den
 * ein Pruefsatz innerhalb EINES Laufes nicht sieht, ist der wichtigste: ein
 * Zaehler im RAM faengt nach jedem Einstecken bei null an, und dann sieht der
 * Dienst bei jeder zweiten Anmeldung einen Klon. Darum wird hier ueber einen
 * Neustart hinweg geprueft.
 *
 * Und er ist NICHT der HOTP-Zaehler im selben Satz. Dass beide
 * nebeneinander laufen, ohne sich zu beruehren, steht weiter unten als
 * eigener Fall - zwei Groessen, die gleich heissen koennten und
 * Verschiedenes messen, sind in diesem Projekt der haeufigste Fehler. */

#include <string.h>
#include "osk_test.h"
#include "test_util.h"
#include "osk_store.h"
#include "osk_store_profiles.h"
#include "fake_flash.h"
#include "fake_se050.h"
#include "osk_vault_format.h"
#include <stdio.h>

void test_sigcount(void)
{
	osk_item_id_t cred = 0, other = 0;
	uint32_t v = 0, v2 = 0;
	uint64_t hotp = 0;

	/* ---- Der geraeteweite Zaehler: Kennung 0 --------------------------
	 *
	 * CTAP zaehlt geraeteweit und nicht je Eintrag, weil nicht auffindbare
	 * Nachweise abgeleitet werden und gar keinen Tresoreintrag haben, an
	 * dem ein Zaehler haengen koennte (osk_sigcount.h). Es fragt darum
	 * unter der Kennung 0 - die ist frei, denn der Tresor gibt
	 * Eintragskennungen ab 1 aus.
	 *
	 * Dieser Fall ist am 29.10.2026 ROT geschrieben worden und hat den
	 * Fehler gezeigt: die erste Umsetzung suchte auch fuer die 0 einen
	 * Indexsatz und meldete OSK_ERR_NOT_FOUND. Am PC fiel das nie auf,
	 * weil die Attrappe in test/ctap/mocks.c ihren eigenen geraeteweiten
	 * Zaehler fuehrt - sie prueft die CTAP-Seite des Vertrags, nicht
	 * diese. */

	osk_test_case("Die Kennung 0 ist der Zaehler des Geraets");
	t_device(0, 9, T_PIN);
	T_OK(osk_store_sigcount_get(0, &v));
	T_EQ(v, 0);
	T_OK(osk_store_sigcount_next(0, &v));
	T_EQ(v, 1);
	T_OK(osk_store_sigcount_next(0, &v));
	T_EQ(v, 2);

	osk_test_case("Er ueberlebt den Neustart - der Fehler, den ein Lauf nicht zeigt");
	t_reboot();
	T_OK(osk_store_sigcount_get(0, &v));
	T_EQ(v, 2);
	T_OK(osk_store_sigcount_next(0, &v));
	T_EQ(v, 3);
	t_reboot();
	T_OK(osk_store_sigcount_get(0, &v));
	T_EQ(v, 3);

	osk_test_case("Er braucht kein aufgeschlossenes Profil");
	{
		/* Ein nicht auffindbarer Nachweis hat keinen Eintrag und kein
		 * Profil. Haenge der Zaehler an einem Profil, waere er fuer
		 * genau die Nachweise unerreichbar, fuer die er da ist. */
		osk_store_profile_lock(0);
		T_OK(osk_store_sigcount_get(0, &v));
		T_OK(osk_store_sigcount_next(0, &v));
		T_EQ(v, 4);
		T_OK(osk_store_profile_unlock(0, (const uint8_t *)T_PIN, strlen(T_PIN)));
	}

	osk_test_case("Was eine Anmeldung kostet - gemessen, nicht geschaetzt");
	{
		/* Der geraeteweite Zaehler steigt bei JEDER Anmeldung, und
		 * jedes Erhoehen ist ein Kopfsatz. Was das heisst, gehoert
		 * gezaehlt und nicht ueberschlagen. */
		const uint32_t n = 1000u;
		uint32_t writes, erases, worst, chip;

		t_device(0, 10, T_PIN);
		fake_flash_count_reset();
		fake_se050_raises_reset();
		for (uint32_t i = 0; i < n; i++) {
			T_OK(osk_store_sigcount_next(0, &v));
		}
		T_EQ(v, n);
		writes = fake_flash_writes();
		erases = fake_flash_erase_max(OSK_AREA_HDR2_OFF, OSK_AREA_HDR2_LEN);
		worst = fake_flash_erase_total();
		chip = fake_se050_raises();

		printf("      %u Anmeldungen: %u Schreib-/Loeschvorgaenge im Flash,\n",
		       n, writes);
		printf("      haeufigster Kopfsektor %u mal geloescht, %u Loeschvorgaenge\n",
		       erases, worst);
		printf("      insgesamt, %u Anhebungen im SE050\n", chip);
		printf("      -> je Anmeldung %u,%02u Schreibvorgaenge, %u,%03u Loeschungen\n",
		       writes / n, (writes * 100u / n) % 100u, erases / n,
		       (erases * 1000u / n) % 1000u);

		/* Ein Kopfsatz je Anmeldung, nicht mehr. Waere es mehr,
		 * stimmte etwas an der Transaktion nicht. */
		T_EQ(chip, n);
		T_TRUE(writes <= 2u * n);
		/* 16 Sektoren zu vier Saetzen: ein Loeschvorgang je vier
		 * Anmeldungen, verteilt auf 16 Sektoren. */
		T_TRUE(erases <= n / 16u + 4u);
	}

	osk_test_case("Ein frischer Eintrag hat noch nie signiert");
	t_device(0, 1, T_PIN);
	T_OK(t_add(0, "Passkey", "fido.example", "schluessel", 0, &cred));
	T_OK(osk_store_sigcount_get(cred, &v));
	T_EQ(v, 0);

	osk_test_case("Jeder Aufruf gibt einen groesseren Wert");
	for (uint32_t i = 1; i <= 5u; i++) {
		T_OK(osk_store_sigcount_next(cred, &v));
		T_EQ(v, i);
	}
	T_OK(osk_store_sigcount_get(cred, &v2));
	T_EQ(v2, 5);

	osk_test_case("Nach dem Neustart zaehlt er weiter, nicht von vorn");
	t_reboot();
	T_OK(osk_store_sigcount_get(cred, &v));
	T_EQ(v, 5);
	T_OK(osk_store_sigcount_next(cred, &v));
	T_EQ(v, 6);
	t_reboot();
	T_OK(osk_store_sigcount_get(cred, &v));
	T_EQ(v, 6);

	osk_test_case("Nur Ablesen zaehlt nicht weiter");
	T_OK(osk_store_sigcount_get(cred, &v));
	T_OK(osk_store_sigcount_get(cred, &v2));
	T_EQ(v, v2);

	osk_test_case("Jeder Eintrag hat seinen eigenen Zaehler");
	T_OK(t_add(0, "Zweiter", "b.example", "schluessel", 0, &other));
	T_OK(osk_store_sigcount_get(other, &v));
	T_EQ(v, 0);
	T_OK(osk_store_sigcount_next(other, &v));
	T_EQ(v, 1);
	T_OK(osk_store_sigcount_get(cred, &v));
	T_EQ(v, 6);

	osk_test_case("Der HOTP-Zaehler ist ein anderer und bleibt unberuehrt");
	T_OK(osk_store_item_counter(cred, &hotp));
	T_EQ(hotp, 0);
	T_OK(osk_store_item_counter_next(cred, &hotp));
	T_EQ(hotp, 1);
	T_OK(osk_store_sigcount_get(cred, &v));
	T_EQ(v, 6); /* vom HOTP-Zaehler nicht mitgezogen */
	T_OK(osk_store_sigcount_next(cred, &v));
	T_EQ(v, 7);
	T_OK(osk_store_item_counter(cred, &hotp));
	T_EQ(hotp, 1); /* und umgekehrt auch nicht */

	osk_test_case("Aendern des Geheimnisses nimmt den Zaehler nicht zurueck");
	{
		osk_item_meta_t meta;

		memset(&meta, 0, sizeof(meta));
		meta.type = OSK_ITEM_LOGIN;
		meta.profile = 0;
		meta.name = "Passkey";
		meta.site = "fido.example";
		T_OK(osk_store_update(cred, &meta, (const uint8_t *)"neu", 3));
		T_OK(osk_store_sigcount_get(cred, &v));
		T_EQ(v, 7);
	}

	osk_test_case("Ohne aufgeschlossenes Profil gibt es keinen Zaehler");
	osk_store_profile_lock(0);
	T_TRUE(osk_store_sigcount_get(cred, &v) != OSK_OK);
	T_TRUE(osk_store_sigcount_next(cred, &v) != OSK_OK);

	osk_test_case("Einen Eintrag, den es nicht gibt, zaehlt niemand");
	T_OK(osk_store_profile_unlock(0, (const uint8_t *)T_PIN, strlen(T_PIN)));
	T_RES(osk_store_sigcount_get(999999u, &v), OSK_ERR_NOT_FOUND);

	osk_store_lock();
}
