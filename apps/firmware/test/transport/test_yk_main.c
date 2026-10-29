/* Startet die vier Pruefsaetze des Strangs E.
 *
 * Ein gruener Lauf heisst: uebersetzt und am PC ausgefuehrt - Stufe 2
 * von 4. Es ist KEINE Messung am Geraet dabei; es gibt noch keines.
 * Ob ein echter Host die Berichte so annimmt, sagt dieser Lauf nicht. */
#include "yk_test.h"

int main(void)
{
	printf("Strang E - Yubico-HID-Protokoll, Pruefungen am PC\n\n");
	suite_ykcrc();
	suite_ykframe();
	suite_ykhid();
	suite_ykkbd();
	printf("\n");
	return t_report("strang-e");
}
