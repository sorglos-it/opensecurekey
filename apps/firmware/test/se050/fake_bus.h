/* Testbus: spielt aufgezeichnete Antworten zurueck, ohne Hardware.
 *
 * Der Treiber merkt keinen Unterschied zu einem echten I2C-Bus - er sieht
 * nur osk_se050_bus_t. Damit laufen Rahmenbau, Kettenbildung, Wiederholung
 * und Zeitlimit am PC durch, obwohl es noch kein Geraet gibt.
 *
 * Aufgezeichnet heisst hier: von Hand nach der Beschreibung gebaut, nicht
 * von einem Geraet mitgeschnitten. Es gibt noch keines. Was diese Tests
 * also beweisen, ist, dass der Treiber sich an SEINE Lesart der
 * Beschreibung haelt - nicht, dass die Lesart stimmt. Der Unterschied
 * steht in docs/se050.md. */
#ifndef FAKE_BUS_H
#define FAKE_BUS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "se050_bus.h"

#define FB_MAX_STEPS 32
/* Groesster Rahmen, den ein Schritt aufbewahrt. Bewusst knapp: der
 * Testbus haelt FB_MAX_STEPS Schritte mit je zwei Puffern dieser Groesse,
 * und jeder Pruefblock legt sich ein eigenes fake_bus_t auf den Stapel.
 * Mit 1200 Byte lief der Windows-Bau in einen Stapelueberlauf (1 MB
 * Vorgabe gegen 8 MB unter Linux) - der Linux-Lauf hatte das nicht
 * gezeigt.
 *
 * 640 statt der frueheren 320: ein oeffentlicher RSA-4096-Modulus ist
 * 512 Byte lang und kommt in EINEM Antwortblock (der Treiber kuendigt
 * SE050_IFSD = 1024 an). Auf dem Hinweg bleibt alles klein - dort
 * zerlegt der Treiber nach dem IFSC des Bausteins. 32 Schritte mal zwei
 * Puffer sind damit 40 kB je Testbus statt 20. */
#define FB_MAX_FRAME 640

/* Was der Testbus bei einem Schritt tun soll. */
typedef enum {
	/* Normal: Schreiben annehmen, dann die Antwort ausliefern. */
	FB_OK = 0,
	/* Kein Chip: jedes Schreiben wird abgelehnt (NACK). */
	FB_NO_CHIP,
	/* Stumm: Schreiben geht durch, es kommt aber nie eine Antwort.
	 * Prueft das Zeitlimit. */
	FB_SILENT,
	/* Abbruch mitten in der Nachricht: nur die ersten cut Byte der
	 * Antwort kommen, dann Stille. */
	FB_TRUNCATE,
	/* Pruefsumme verdorben: die Antwort kommt vollstaendig, aber mit
	 * einem umgedrehten Bit im Epilog. */
	FB_BAD_CRC,
} fb_mode_t;

typedef struct {
	fb_mode_t mode;

	/* Erwartetes Schreiben. want_len == 0 heisst "egal was kommt". */
	uint8_t want[FB_MAX_FRAME];
	size_t want_len;

	/* Antwort, die danach gelesen werden kann. */
	uint8_t give[FB_MAX_FRAME];
	size_t give_len;

	/* Nur bei FB_TRUNCATE: wie viele Byte noch kommen. */
	size_t cut;
} fb_step_t;

/* Antwortgeber: statt aufgezeichneter Schritte beantwortet eine Funktion
 * jeden Rahmen. Das braucht alles, dessen Antwort vom Inhalt des Befehls
 * abhaengt - beim Kanalaufbau zum Beispiel haengt das Kryptogramm an der
 * Zufallszahl, die der Treiber gerade gewuerfelt hat.
 *
 * frame/len ist der geschriebene T=1'-Rahmen. Die Funktion legt ihren
 * Antwortrahmen in out ab und gibt dessen Laenge zurueck, oder 0, wenn sie
 * nicht antworten will. */
typedef size_t (*fb_responder_t)(void *ctx, const uint8_t *frame, size_t len, uint8_t *out,
				 size_t cap);

typedef struct {
	osk_se050_bus_t bus;

	/* Wenn gesetzt, gilt dieser statt der Schrittliste. */
	fb_responder_t responder;
	void *responder_ctx;
	uint8_t resp_buf[FB_MAX_FRAME];
	size_t resp_len;

	fb_step_t steps[FB_MAX_STEPS];
	size_t n_steps;

	/* Laufender Zustand. */
	size_t at_step;
	size_t read_pos;
	bool armed; /* true: Antwort des laufenden Schritts steht an */

	/* Nachzaehlen, was der Treiber getan hat. */
	size_t writes;
	size_t empty_writes; /* Rahmen ohne Datenbyte - darf 0 bleiben */
	size_t waits;
	uint32_t clock_ms;

	/* Erwartung verletzt: so merkt der Test, dass der Treiber etwas
	 * anderes geschickt hat als aufgezeichnet. */
	bool mismatch;
	size_t mismatch_step;
} fake_bus_t;

void fb_init(fake_bus_t *fb);

/* Antwortgeber setzen. Danach wird die Schrittliste nicht mehr benutzt. */
void fb_set_responder(fake_bus_t *fb, fb_responder_t fn, void *ctx);

/* Einen Schritt anhaengen. want darf NULL sein ("egal was kommt"),
 * give darf NULL sein (keine Antwort). */
void fb_add(fake_bus_t *fb, fb_mode_t mode, const uint8_t *want, size_t want_len,
	    const uint8_t *give, size_t give_len);

/* Wie fb_add(), aber baut die Antwort als vollstaendigen T=1'-Rahmen mit
 * richtiger Pruefsumme - so steht in den Tests nur, was wirklich
 * interessiert. */
void fb_add_frame(fake_bus_t *fb, fb_mode_t mode, uint8_t nad, uint8_t pcb, const uint8_t *inf,
		  uint16_t inf_len);

/* Nur bei FB_TRUNCATE: wie viele Byte des letzten Rahmens noch kommen. */
void fb_set_cut(fake_bus_t *fb, size_t cut);

/* Die zwei S-Block-Antworten, die osk_se050_t1_open() und
 * osk_se050_t1_close() erwarten - jeder Test braucht sie. */
void fb_add_open_reply(fake_bus_t *fb);
void fb_add_close_reply(fake_bus_t *fb);

/* true, wenn alle Schritte abgearbeitet und keine Erwartung verletzt ist. */
bool fb_clean(const fake_bus_t *fb);

#endif
