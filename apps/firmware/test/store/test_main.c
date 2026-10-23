/* Alle Pruefsaetze des Tresors, am PC, ohne jede Hardware.
 *
 * Aufruf: store_tests[.exe] - ohne Schalter. Rueckgabe 0, wenn alles gruen
 * ist, sonst 1. Gebaut wird mit apps/firmware/test/store/build_and_run.ps1
 * oder ueber die CMakeLists.txt daneben. */

#include <stdio.h>
#include "osk_test.h"
#include "test_util.h"
#include "fake_flash.h"

int osk_test_fails;
int osk_test_checks;
const char *osk_test_current = "";

void osk_test_case(const char *name)
{
	osk_test_current = name;
	printf("  - %s\n", name);
}

void osk_test_fail(const char *file, int line, const char *what)
{
	osk_test_fails++;
	printf("    FEHLER %s:%d  %s\n", file, line, what);
	printf("           im Fall: %s\n", osk_test_current);
}

const char *osk_test_rtext(osk_result_t r)
{
	switch (r) {
	case OSK_OK:
		return "OK";
	case OSK_ERR_ARG:
		return "ARG";
	case OSK_ERR_IO:
		return "IO";
	case OSK_ERR_TIMEOUT:
		return "TIMEOUT";
	case OSK_ERR_DENIED:
		return "DENIED";
	case OSK_ERR_LOCKED:
		return "LOCKED";
	case OSK_ERR_NOT_FOUND:
		return "NOT_FOUND";
	case OSK_ERR_EXISTS:
		return "EXISTS";
	case OSK_ERR_FULL:
		return "FULL";
	case OSK_ERR_FORMAT:
		return "FORMAT";
	case OSK_ERR_UNSUPPORTED:
		return "UNSUPPORTED";
	case OSK_ERR_INTERNAL:
		return "INTERNAL";
	default:
		return "?";
	}
}

struct suite {
	const char *name;
	void (*fn)(void);
};

int main(void)
{
	static const struct suite suites[] = {
	    {"Woerterliste", test_wordlist},
	    {"Umschlaege (Shamir)", test_shamir},
	    {"Grundlagen", test_core},
	    {"Kennung zum Suchen", test_tag},
	    {"Index in Bloecken", test_paging},
	    {"Stromausfall", test_power},
	    {"Wanderung alter Fassungen", test_migrate},
	    {"Sicherungsdatei", test_backup},
	    {"Stick zu Stick", test_transfer},
	    {"Verschleiss", test_wear},
	};

	printf("Tresor von opensecurekey - Pruefsaetze am PC\n");
	printf("Keine Hardware beteiligt. Krypto und SE050 sind Attrappen.\n\n");

	for (size_t i = 0; i < sizeof(suites) / sizeof(suites[0]); i++) {
		int before = osk_test_fails;

		printf("[%s]\n", suites[i].name);
		t_init();
		suites[i].fn();
		printf("  %s\n\n", (osk_test_fails == before) ? "gruen" : "ROT");
	}
	fake_flash_free();

	printf("%d Pruefungen, %d Fehler\n", osk_test_checks, osk_test_fails);
	return (osk_test_fails == 0) ? 0 : 1;
}
