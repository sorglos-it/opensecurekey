/* Der Platinenhaken des Erstlaufs - noch ein Platzhalter.
 *
 * WAS HIER FEHLT, und zwar nicht ein wenig: der Erstlauf braucht vier
 * Dinge, die es in diesem Baum noch nicht gibt.
 *
 *   load / store      Ein eigener, kleiner Bereich im Flash, der dem
 *                     Update gesperrt ist (osk_boot_may_write) und der
 *                     ganz oder gar nicht schreibt - zwei abwechselnde
 *                     Abdruecke, wie osk_boot_state_store(). Der Zettel
 *                     traegt Schluessel, also verpackt ihn die
 *                     Tresorschicht.
 *   random_mcu        Zufall aus CryptoCell des nRF52840.
 *   random_chip       osk_se050_random() - braucht einen stehenden Kanal,
 *                     also den Werkskanal beim ersten Start.
 *   factory_keys      Die Werkssaetze aus NXP AN12973 Tabelle 6, gewaehlt
 *                     nach der OEF ID des verbauten Teils. Sie stehen
 *                     absichtlich nirgends in diesem Repo (docs/se050.md):
 *                     der Treiber braucht sie nie fest eingebaut, und ein
 *                     Repo, das sie mitbringt, macht den Fehler der
 *                     Werkseinstellung nur bequemer.
 *   probe             Einen Satz in den SCP03-Haken legen und
 *                     osk_se050_open() rufen.
 *
 * Bis dahin: OSK_ERR_UNSUPPORTED. Der Start meldet das, und zwar sichtbar
 * - ein Geraet, auf dem der Erstlauf nicht gelaufen ist, laeuft mit
 * oeffentlich bekannten Schluesseln, und das soll in der Ausgabe stehen.
 *
 * Sobald die vier da sind, wird mit -DOSK_HAVE_COMMISSION_PLATFORM gebaut
 * und diese Datei liefert den echten Haken. */

#include "osk_commission.h"

#ifdef OSK_HAVE_COMMISSION_PLATFORM
#error "OSK_HAVE_COMMISSION_PLATFORM ist gesetzt, aber der echte Haken fehlt noch"
#endif

osk_result_t osk_commission_platform_backend(const osk_commission_backend_t **be_out)
{
	if (be_out == NULL) {
		return OSK_ERR_ARG;
	}
	*be_out = NULL;
	return OSK_ERR_UNSUPPORTED;
}
