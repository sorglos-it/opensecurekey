/* Prueflauf des PQC-Moduls am PC. Kein Geraet.
 *
 * Reihenfolge mit Absicht: erst Keccak, dann das Rechenwerk, dann das
 * Verfahren gegen die amtlichen Werte, dann die Mischung. Faellt etwas
 * durch, sagt die erste rote Zeile, wo gesucht werden muss - und nicht
 * nur "ML-KEM ist falsch".
 */
#include "tiny_test.h"

int g_tt_failed;
int g_tt_checks;

size_t tt_unhex(uint8_t *out, size_t out_max, const char *hex)
{
	size_t n = 0;

	while (hex[0] != '\0' && hex[1] != '\0') {
		unsigned int value = 0u;

		for (unsigned int half = 0; half < 2u; half++) {
			char ch = hex[half];

			value <<= 4;
			if (ch >= '0' && ch <= '9') {
				value |= (unsigned int)(ch - '0');
			} else if (ch >= 'a' && ch <= 'f') {
				value |= (unsigned int)(ch - 'a' + 10);
			} else if (ch >= 'A' && ch <= 'F') {
				value |= (unsigned int)(ch - 'A' + 10);
			} else {
				return 0u;
			}
		}
		if (n >= out_max) {
			return 0u;
		}
		out[n] = (uint8_t)value;
		n++;
		hex += 2;
	}
	/* Eine ungerade Anzahl Zeichen ist kein halbes Byte, sondern ein
	 * Tippfehler im Testwert. */
	if (hex[0] != '\0') {
		return 0u;
	}
	return n;
}

int main(void)
{
	printf("Pruefungen des PQC-Moduls (ML-KEM-768 mit X25519) am PC\n");
	printf("Stand: nichts davon ist am Geraet gelaufen.\n");

	printf("\nKeccak gegen eine fremde Umsetzung\n");
	test_keccak();

	printf("\nRechenwerk im Ring\n");
	test_poly();

	printf("\nML-KEM-768 gegen die amtlichen Testwerte des NIST\n");
	test_mlkem();

	printf("\nDie Mischung mit X25519: hybrid oder gar nicht\n");
	test_hybrid();

	/* Diese Zeile liest tools/check.py aus (Muster
	 * "<name>: N Faelle, M Fehler"). Ohne sie zaehlt der Lauf dort als
	 * "keine Pruefung ausgefuehrt" - und gruener Code ohne eine
	 * einzige Pruefung ist kein gruener Lauf. */
	printf("\npqc: %d Faelle, %d Fehler\n", g_tt_checks, g_tt_failed);
	if (g_tt_failed == 0) {
		printf("GRUEN\n");
		return 0;
	}
	printf("ROT\n");
	return 1;
}
