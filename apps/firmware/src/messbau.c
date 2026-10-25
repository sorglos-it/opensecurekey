/* NUR ZUM MESSEN. Kein Teil des Geraets.
 *
 * Diese Datei uebersetzt sich zu nichts, solange OSK_MESSBAU nicht
 * gesetzt ist - ein gewoehnlicher Bau sieht sie nicht. Sie wird nur
 * gebraucht, um EINE Zahl zu bekommen: wieviel Flash und RAM die Firmware
 * belegt (docs/architecture.md, "How much room is there, really").
 *
 * Warum es sie gibt: der Bau vom 24.10.2026 hat alle 77 Quelldateien
 * fehlerfrei fuer ARM uebersetzt, aber das Binden brach an sechs Namen ab,
 * die es auf dem Geraet noch nicht gibt. Ohne Binden gibt es kein Abbild,
 * und ohne Abbild keine Zahl. Hier stehen also sechs Platzhalter, damit
 * der Binder durchkommt.
 *
 * WAS DIE ZAHL WERT IST: sie ist eine UNTERGRENZE. Die sechs echten
 * Umsetzungen sind groesser als diese Platzhalter - der I2C-Treiber, die
 * Vermittlung der Transporte und die Entpackung der SCP03-Schluessel
 * kommen noch dazu. Die gemessene Groesse waechst also noch; sie schrumpft
 * nicht.
 *
 * Drei der sechs Luecken sind ABSICHTLICH (se050_platform.h sagt das
 * ausdruecklich: "Es gibt absichtlich keine Vorgabe-Umsetzung: fehlt sie,
 * bricht das Binden - und nicht erst der Betrieb mit einem Schluessel aus
 * Nullen"). Genau darum geben die Platzhalter hier auch kein brauchbares
 * Ergebnis zurueck, sondern OSK_ERR_UNSUPPORTED: wer diese Datei
 * versehentlich in ein Geraet baut, bekommt ein Geraet, das nichts tut -
 * und keines, das mit einem Schluessel aus Nullen arbeitet.
 */

#ifdef OSK_MESSBAU

#include "osk_result.h"
#include "osk_transport.h"
#include "se050_platform.h"

const char *osk_result_text(osk_result_t r)
{
	(void)r;
	return "Platzhalter des Messbaus";
}

osk_result_t osk_transport_register(osk_link_t link, osk_handler_fn handler)
{
	(void)link;
	(void)handler;
	return OSK_ERR_UNSUPPORTED;
}

osk_result_t osk_transport_enable(osk_link_t link, bool on)
{
	(void)link;
	(void)on;
	return OSK_ERR_UNSUPPORTED;
}

osk_result_t osk_se050_platform_bus(osk_se050_bus_t **bus_out)
{
	(void)bus_out;
	return OSK_ERR_UNSUPPORTED;
}

osk_result_t osk_se050_platform_scp_keys(osk_scp03_static_keys_t *keys_out)
{
	(void)keys_out;
	return OSK_ERR_UNSUPPORTED;
}

osk_result_t osk_se050_platform_channel_obj(uint32_t *obj_id_out)
{
	(void)obj_id_out;
	return OSK_ERR_UNSUPPORTED;
}

#endif /* OSK_MESSBAU */
