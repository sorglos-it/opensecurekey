/* Prueflauf des SE050-Treibers, am PC, ohne Hardware.
 *
 * Starten: apps\firmware\test\se050\build.ps1
 *
 * Was NICHT geprueft wird, weil es ohne Geraet nicht geht: ob der echte
 * SE050 die Rahmen annimmt, die dieser Treiber baut. Die Rahmenwerte in
 * se050_t1_wire.h stammen aus ISO/IEC 7816-3, weil NXPs eigene Beschreibung
 * UM11225 nur nach Anmeldung bei NXP zu bekommen ist. Steht in
 * docs/se050.md, Abschnitt "Was geprueft ist". */
#include "tiny_test.h"

int g_tt_failed;
int g_tt_checks;

int main(void)
{
	printf("Prueflauf SE050-Treiber (ohne Hardware)\n\n");

	printf("Pruefsumme\n");
	test_crc();

	printf("\nTLV\n");
	test_tlv();

	printf("\nT=1' ueber I2C\n");
	test_t1();

	printf("\nSCP03\n");
	test_scp03();

	printf("\nKrypto, Statuswort und Fehlerwege\n");
	test_errors();

	printf("\nVertragsfunktionen gegen den Nachbau\n");
	test_contract();

	printf("\nRSA\n");
	test_rsa();

	printf("\nMonotone Zaehler\n");
	test_counter();

	/* Diese Zeile liest tools/check.py aus (Muster
	 * "<name>: N Faelle, M Fehler"). Ohne sie zaehlt der Lauf dort als
	 * "keine Pruefung ausgefuehrt" - und gruener Code ohne eine einzige
	 * Pruefung ist kein gruener Lauf. */
	printf("\nse050: %d Faelle, %d Fehler\n", g_tt_checks, g_tt_failed);
	if (g_tt_failed == 0) {
		printf("GRUEN\n");
		return 0;
	}
	printf("ROT\n");
	return 1;
}
