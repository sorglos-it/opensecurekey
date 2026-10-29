/* Prueflauf der Rechteschicht, am PC, ohne Hardware.
 *
 * Starten: apps\firmware\test\authz\build.ps1  (Windows, Visual-Studio-
 * Build-Tools) oder `make` in diesem Ordner (gcc).
 *
 * WAS DIESER LAUF NICHT PRUEFT, und das gehoert in dieselbe Zeile wie das
 * Wort "gruen":
 *   - ob ein echter SE051 dieselben HMACs und Zaehlerstaende liefert. Hier
 *     antwortet fake_se050.c. Das ist Sache von Strang A und am Geraet zu
 *     messen.
 *   - ob eine echte Zeitstempelstelle Antworten der Form schickt, die
 *     test_tsa.c baut. Die Form stammt aus RFC 3161 und RFC 5652, nicht
 *     aus einer gemessenen Antwort.
 *   - ob die Sitzungen einen Stromausfall ueberstehen. Sie liegen im RAM
 *     und sind nach dem Abziehen weg - das ist so gewollt, aber am Geraet
 *     nicht nachgemessen.
 *
 * Das Projekt steht damit auf Stufe 2 von 4: geschrieben / uebersetzt /
 * im Abbild / am Geraet gelaufen. Siehe docs/rechtebereiche.md. */
#include "authz_t.h"

int g_tt_failed;
int g_tt_checks;

int main(void)
{
	/* Ausgabe ungepuffert. Sonst geht bei einem Absturz genau die Zeile
	 * verloren, die sagt, wo er war - der Puffer wird nicht mehr
	 * geschrieben, und der Lauf sieht aus, als haette er nie begonnen.
	 * Einmal in diesem Projekt passiert und zwei Durchgaenge gekostet. */
	setvbuf(stdout, NULL, _IONBF, 0);

	printf("Prueflauf Rechteschicht (ohne Hardware)\n\n");

	printf("DER lesen und schreiben\n");
	test_der();

	printf("\nRechtebereiche und die Trennung\n");
	test_realm();

	printf("\nZugangsschluessel: Anmeldung und Weitergabe\n");
	test_keys();

	printf("\nZugangsdaten im Chip\n");
	test_cred();

	printf("\nZeitstempel nach RFC 3161\n");
	test_tsa();

	/* Diese Zeile liest tools/check.py aus (Muster
	 * "<name>: N Faelle, M Fehler"). Ohne sie zaehlt der Lauf dort als
	 * "keine Pruefung ausgefuehrt" - und gruener Code ohne eine einzige
	 * Pruefung ist kein gruener Lauf. */
	printf("\nauthz: %d Faelle, %d Fehler\n", g_tt_checks, g_tt_failed);
	if (g_tt_failed == 0) {
		printf("GRUEN\n");
		return 0;
	}
	printf("ROT\n");
	return 1;
}
