/* Nachbau der Gegenseite, nur fuer die Tests.
 *
 * Der Nachbau beantwortet T=1'-Rahmen so, wie der Treiber es von einem
 * SE050 erwartet: Schnittstelle zuruecksetzen, Applet auswaehlen, SCP03
 * aufbauen, danach verschluesselte Befehle. Damit laeuft osk_se050_open()
 * bis zum stehenden Kanal durch, ohne Hardware.
 *
 * WICHTIG, damit niemand mehr hineinliest, als drinsteht: der Nachbau
 * folgt DEMSELBEN Verstaendnis der Beschreibung wie der Treiber. Wenn
 * beide dieselbe Stelle falsch verstehen, passen sie trotzdem zusammen.
 * Diese Tests zeigen also, dass die Schichten des Treibers zueinander
 * passen - nicht, dass sie zum echten Baustein passen. Steht in
 * docs/se050.md unter "Was geprueft ist und was nicht". */
#ifndef SE050_SIM_H
#define SE050_SIM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "fake_bus.h"
#include "se050_scp03.h"

typedef struct {
	/* Feste Schluessel - dieselben, die der Testhaken dem Treiber gibt. */
	osk_scp03_static_keys_t keys;

	/* Sitzungszustand der Gegenseite. */
	osk_scp03_t scp;
	bool applet_selected;
	bool channel;

	/* Zufallszahl, die der Nachbau liefert. Fest, damit ein Fehlerbild
	 * wiederholbar ist. */
	uint8_t card_challenge[8];

	/* Zaehler fuer den Antwort-Startwert: der Wert, mit dem der gerade
	 * beantwortete Befehl verschluesselt war. */
	uint8_t cmd_counter[16];

	/* Mitschrift fuer die Tests. */
	uint8_t last_ins;
	uint8_t last_p1;
	uint8_t last_p2;
	size_t commands;

	/* Die Klartext-Nutzlast des letzten Befehls. */
	uint8_t last_payload[256];
	size_t last_payload_len;

	/* Was auf den naechsten Befehl geantwortet wird. */
	uint8_t next_body[256];
	size_t next_body_len;
	uint16_t next_sw;
} se050_sim_t;

/* Nachbau an den Testbus haengen. */
void sim_attach(se050_sim_t *sim, fake_bus_t *fb);

/* Antwort fuer den naechsten Befehl festlegen. body darf NULL sein. */
void sim_set_reply(se050_sim_t *sim, const uint8_t *body, size_t len, uint16_t sw);

/* Die festen Schluessel, die Nachbau und Testhaken gemeinsam benutzen. */
const osk_scp03_static_keys_t *sim_keys(void);

#endif
