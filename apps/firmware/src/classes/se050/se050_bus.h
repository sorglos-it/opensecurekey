/* Schmale Bus-Schnittstelle unter dem Treiber.
 *
 * Darueber liegt T=1' und weiss nichts von I2C. Auf dem Geraet steckt hier
 * der I2C-Treiber des nRF52840, im Pruefbau am PC ein Testbus mit
 * aufgezeichneten Antworten. Beides nutzt denselben Treibercode.
 *
 * Alle Funktionen liefern 0 bei Erfolg und einen negativen Wert bei Fehler.
 * Ein Bus, der gerade nichts zu geben hat, meldet OSK_BUS_AGAIN - das ist
 * kein Fehler, sondern "spaeter noch einmal fragen". */
#ifndef SE050_BUS_H
#define SE050_BUS_H

#include <stdint.h>
#include <stddef.h>

/* Noch nichts da - erneut versuchen. Nicht als Fehler behandeln. */
#define OSK_BUS_AGAIN   (-1)
/* Leitung gestoert, Baustein antwortet nicht (NACK), Bus haengt. */
#define OSK_BUS_IO      (-2)
/* Aufrufparameter unbrauchbar. */
#define OSK_BUS_ARG     (-3)

typedef struct osk_se050_bus osk_se050_bus_t;

struct osk_se050_bus {
	/* Schreibt len Bytes in EINEM Bus-Vorgang.
	 * Wichtig: nie mit len == 0 aufrufen. Ein leerer Rahmen, der nur die
	 * Adresse enthaelt, laesst den SE050 dauerhaft die Taktleitung halten
	 * (Erratasheet I2C.2) - der Baustein ist dann nur noch per Stromabwurf
	 * zu retten. Der Treiber prueft das, der Bus muss es nicht. */
	int (*write)(osk_se050_bus_t *self, const uint8_t *data, size_t len);

	/* Liest bis zu len Bytes. Liefert die Anzahl gelesener Bytes (>= 0)
	 * oder OSK_BUS_AGAIN, wenn der Baustein noch rechnet. */
	int (*read)(osk_se050_bus_t *self, uint8_t *data, size_t len);

	/* Wartet mindestens ms Millisekunden. Darf laenger warten. */
	void (*wait_ms)(osk_se050_bus_t *self, uint32_t ms);

	/* Millisekunden seit einem festen Zeitpunkt - nur Differenzen werden
	 * benutzt, der Nullpunkt ist frei. Muss ueberlaufsicher sein, indem
	 * der Aufrufer stets Differenzen in uint32_t rechnet. */
	uint32_t (*now_ms)(osk_se050_bus_t *self);

	/* Optional: Baustein hart aus- und einschalten (ENA-Pin).
	 * NULL, wenn die Platine das nicht kann. Der Treiber meldet dann
	 * einen haengenden Bus nur, statt ihn zu reparieren. */
	int (*power_cycle)(osk_se050_bus_t *self);

	/* Freier Zeiger fuer die jeweilige Umsetzung. */
	void *ctx;
};

#endif
