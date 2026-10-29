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

void test_sigcount(void)
{
	osk_item_id_t cred = 0, other = 0;
	uint32_t v = 0, v2 = 0;
	uint64_t hotp = 0;

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
