/* T=1' ueber I2C: Rahmen bauen und lesen, Pruefsumme, Wiederholung,
 * Zeitlimit, Kettenbildung langer Nachrichten.
 *
 * Diese Schicht kennt nur Bytefolgen. Was darin steht - eine APDU - ist ihr
 * gleichgueltig. Darunter liegt se050_bus.h, darueber se050_apdu.h.
 *
 * Alle Zahlenwerte des Drahtprotokolls stehen in se050_t1_wire.h. */
#ifndef SE050_T1_H
#define SE050_T1_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"
#include "se050_bus.h"
#include "se050_t1_wire.h"

typedef struct {
	osk_se050_bus_t *bus;

	/* Folgenummern: eine je Richtung, jede nur 0 oder 1. */
	uint8_t seq_tx;
	uint8_t seq_rx;

	/* Groesste INF-Laenge, die der Baustein annimmt. Startet bei
	 * SE050_IFSD_DEFAULT und wird von einem S-IFS kleiner gesetzt. */
	uint8_t ifsc;

	/* true, sobald ein Rahmen abgewiesen oder abgebrochen wurde. Solange
	 * das steht, sendet der Treiber keinen neuen Befehl, sondern setzt
	 * erst zurueck - sonst laeuft er in Erratasheet I2C.1. */
	bool needs_resync;

	/* true zwischen open() und close(). */
	bool open;
} osk_se050_t1_t;

/* Schnittstelle zuruecksetzen und Zaehler auf Anfang. Danach ist die
 * Schicht bereit. Der Baustein bekommt einen S-Block "Soft Reset". */
osk_result_t osk_se050_t1_open(osk_se050_t1_t *t1, osk_se050_bus_t *bus);

/* Sitzung beenden: S-Block "End of APDU session", danach schlaeft der
 * Baustein. Fehler werden gemeldet, aber die Schicht ist danach in jedem
 * Fall geschlossen. */
osk_result_t osk_se050_t1_close(osk_se050_t1_t *t1);

/* Eine vollstaendige Nachricht hin und die vollstaendige Antwort zurueck.
 * Lange Nachrichten werden in Ketten zerlegt und Antwortketten wieder
 * zusammengesetzt. Bei Erfolg steht in *rsp_len die Antwortlaenge.
 *
 * OSK_ERR_FORMAT, wenn die Antwort nicht in rsp_cap passt - der Puffer
 * bleibt dann unbrauchbar, der Aufrufer darf ihn nicht auswerten. */
osk_result_t osk_se050_t1_transceive(osk_se050_t1_t *t1,
				     const uint8_t *cmd, size_t cmd_len,
				     uint8_t *rsp, size_t rsp_cap,
				     size_t *rsp_len);

/* Nach einem Fehler wieder aufsetzen: S-Block "Resync", Zaehler auf 0.
 * Wird von transceive() selbst aufgerufen; einzeln nur fuer Tests. */
osk_result_t osk_se050_t1_resync(osk_se050_t1_t *t1);

/* Einen Rahmen in buf bauen. Gibt die Gesamtlaenge zurueck oder 0, wenn
 * die Angaben nicht passen. Getrennt herausgezogen, damit der Rahmenbau
 * ohne Bus geprueft werden kann. */
size_t osk_se050_t1_build(uint8_t *buf, size_t cap, uint8_t nad, uint8_t pcb,
			  const uint8_t *inf, uint8_t inf_len);

/* Einen Rahmen pruefen: Laenge stimmig und Pruefsumme richtig.
 * len ist die Gesamtlaenge einschliesslich Prolog und Epilog. */
osk_result_t osk_se050_t1_check(const uint8_t *frame, size_t len);

#endif
