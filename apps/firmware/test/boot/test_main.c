/* Alle Pruefungen des Firmware-Updates, am PC, ohne jede Hardware.
 *
 * Aufruf: boot_tests[.exe] - ohne Schalter. Rueckgabe 0, wenn alles gruen
 * ist, sonst 1. Gebaut ueber die CMakeLists.txt daneben.
 *
 * WAS HIER NICHT GEPRUEFT WIRD, und das ist viel: MCUboot selbst laeuft
 * nicht mit, der ARM-Bau existiert nicht, es gibt keinen QSPI-Baustein und
 * keinen SE050. Geprueft wird die ENTSCHEIDUNG - wann ein Paket abgelehnt
 * wird, was nach einem Abbruch startet, welche Bereiche beschrieben werden
 * duerfen. Das ist der Teil, der still falsch sein kann. */

#include <stdio.h>
#include "boot_test.h"

int bt_fails;
int bt_checks;
const char *bt_current = "";

void bt_case(const char *name)
{
	bt_current = name;
	printf("  - %s\n", name);
}

void bt_fail(const char *file, int line, const char *what)
{
	bt_fails++;
	printf("    FEHLER %s:%d  %s\n", file, line, what);
	printf("           im Fall: %s\n", bt_current);
}

const char *bt_rtext(osk_result_t r)
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
	    {"Zustand abbruchsicher", test_state},   {"Signatur", test_signature},
	    {"Rueckschritt", test_rollback},	     {"Falsches Brett", test_board},
	    {"Abbruch an jeder Stelle", test_abort}, {"Tresor bleibt unberuehrt", test_vault},
	    {"Notfallweg", test_recovery},
	};

	printf("Firmware-Update von opensecurekey - Pruefsaetze am PC\n");
	printf("Keine Hardware, kein MCUboot, kein ARM-Bau. Krypto und SE050 sind Attrappen.\n\n");

	for (size_t i = 0; i < sizeof(suites) / sizeof(suites[0]); i++) {
		int before = bt_fails;

		printf("[%s]\n", suites[i].name);
		suites[i].fn();
		printf("  %s\n\n", (bt_fails == before) ? "gruen" : "ROT");
	}

	printf("%d Pruefungen, %d Fehler\n", bt_checks, bt_fails);
	return (bt_fails == 0) ? 0 : 1;
}
