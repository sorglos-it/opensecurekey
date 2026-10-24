/* Alle Pruefsaetze der Anzeige, am PC, ohne jede Hardware.
 *
 * Das Bild entsteht im Speicher, die Attrappe baut es aus den gesendeten
 * Bytes zurueck und gibt es als Text aus. Auf einem echten Display hat nie
 * etwas davon gestanden - das Teil ist bestellt, nicht da. */

#include <stdio.h>
#include "fake_panel.h"
#include "ui_test.h"

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
	    {"Bildspeicher", test_gfx},	      {"Schriften", test_font},
	    {"Umbruch und UTF-8", test_text}, {"Sprachen", test_lang},
	    {"Taster", test_button},	      {"Treiber der Anzeige", test_panel},
	    {"Freigabebilder", test_confirm}, {"Ablauf der Freigabe", test_flow},
	};

	printf("Anzeige von opensecurekey - Pruefsaetze am PC (%s)\n", OSK_DISPLAY_NAME);
	printf("Keine Hardware beteiligt. Das Display ist eine Attrappe.\n\n");

	for (size_t i = 0; i < sizeof(suites) / sizeof(suites[0]); i++) {
		int before = osk_test_fails;

		printf("[%s]\n", suites[i].name);
		fake_panel_reset();
		suites[i].fn();
		printf("  %s\n\n", (osk_test_fails == before) ? "gruen" : "ROT");
	}

	printf("%d Pruefungen, %d Fehler\n", osk_test_checks, osk_test_fails);
	return (osk_test_fails == 0) ? 0 : 1;
}
