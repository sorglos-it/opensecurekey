/* Die PIN am Geraet eingeben - ohne den Rechner und ohne zu wissen, wieviele
 * Taster verbaut sind.
 *
 * WARUM AM GERAET: ab FIPS 140-3 Stufe 3 ist ein vertrauenswuerdiger Kanal
 * Pflicht. Die PIN darf den Rechner nie sehen - weder als Tastendruck noch
 * ueber USB. Darum wird sie hier gesammelt und direkt an den Sicherheitschip
 * gegeben.
 *
 * DREI SCHICHTEN, damit die Tasterzahl keine Rolle spielt:
 *   1. osk_pin_map()   Rohereignis eines Tasters  ->  Handlung
 *   2. osk_pin_apply() Handlung                   ->  Stand der Eingabe
 *   3. osk_pin_ask()   Schleife darum: lesen, zeichnen, abgeben
 * Ein Wechsel von einem auf zwei Taster aendert nur die Tabelle in Schicht 1.
 * Schicht 2 und 3 sehen keinen Taster.
 *
 * WAS NIE PASSIERT: die Ziffern verlassen diesen Rechenkern nicht als
 * Klartext. Sie gehen einmal an osk_pin_sink_t.submit() - das ist der
 * Sicherheitschip, der sie langsam zu einem Schluessel ableitet - und werden
 * danach ueberschrieben, auch bei Abbruch, Zeitablauf und Fehler.
 *
 * WAS DIE RECHNUNG DER DREI WEGE SAGT, steht in docs/pin-am-geraet.md und
 * kommt aus osk_pin_cost() - die Zahlen stehen nur dort. */
#ifndef OSK_PIN_ENTRY_H
#define OSK_PIN_ENTRY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "osk_button.h"
#include "osk_result.h"

/* Kuerzer als sechs Ziffern laesst der Chip nicht zu; mehr als zwoelf passt
 * nicht mehr als Punktreihe auf 48 Punkte Breite (nachgezaehlt in
 * test_pin.c, Fall "die Punktreihe passt auf die Flaeche"). */
#define OSK_PIN_MIN_LEN 6
#define OSK_PIN_MAX_LEN 12
#define OSK_PIN_VALUES 10

/* Vier Bits je Ziffer beim Weg "lang/kurz". */
#define OSK_PIN_BITS_PER_DIGIT 4

/* Hoechstens zwei Taster - mehr ist auf dieser Platine nicht vorgesehen. */
#define OSK_PIN_BUTTONS_MAX 2

/* --- Schicht 1: welcher Weg wird bedient ---------------------------------- */

typedef enum {
	/* Ein Taster: kurz blaettert die Ziffer weiter, lang nimmt sie,
	 * doppelt loescht die letzte. */
	OSK_PIN_SCHEME_ONE = 0,
	/* Ein Taster: kurz = Bit 0, lang = Bit 1, vier Bits sind eine Ziffer
	 * (0..9; 10..15 wird abgelehnt und die Ziffer neu begonnen). */
	OSK_PIN_SCHEME_BITS,
	/* Zwei Taster: Taster 1 blaettert weiter, Taster 2 nimmt die Ziffer,
	 * Taster 2 lang loescht die letzte. */
	OSK_PIN_SCHEME_TWO,
} osk_pin_scheme_t;

#define OSK_PIN_SCHEME_COUNT 3

/* Welcher Weg gebaut ist, entscheidet die Hardware - nicht dieser Code.
 * Bis Thomas die Tasterzahl festlegt, gilt der eine Taster, weil genau einer
 * bedienbar verbaut ist (SW2 ist zur Loetbruecke geworden). */
#ifndef OSK_PIN_SCHEME_DEFAULT
#define OSK_PIN_SCHEME_DEFAULT OSK_PIN_SCHEME_ONE
#endif

/* Wieviele Taster braucht ein Weg? */
uint8_t osk_pin_buttons(osk_pin_scheme_t scheme);

/* Die Handlungen, die der Kern kennt. Kein Taster, keine Zeit, keine Farbe. */
typedef enum {
	OSK_PIN_ACT_NONE = 0,
	OSK_PIN_ACT_NEXT,   /* naechste Ziffer im Kreis */
	OSK_PIN_ACT_TAKE,   /* diese Ziffer nehmen */
	OSK_PIN_ACT_BIT0,   /* ein Bit 0 (nur beim Weg "lang/kurz") */
	OSK_PIN_ACT_BIT1,   /* ein Bit 1 */
	OSK_PIN_ACT_BACK,   /* letzte Stelle zurueck; auf Stelle 0: Abbruch */
	OSK_PIN_ACT_CANCEL, /* abbrechen */
} osk_pin_act_t;

/* Rohereignis von Taster `button` (0 oder 1) in eine Handlung uebersetzen.
 * Ein Ereignis eines Tasters, den dieser Weg nicht hat, ergibt NONE. */
osk_pin_act_t osk_pin_map(osk_pin_scheme_t scheme, uint8_t button, osk_btn_event_t event);

/* --- Schicht 2: der Stand der Eingabe ------------------------------------- */

/* Hier und nur hier liegen die Ziffern. Wer diese Struktur anlegt, muss sie
 * mit osk_pin_wipe() wieder loeschen - auf jedem Weg hinaus. */
typedef struct {
	uint8_t digits[OSK_PIN_MAX_LEN];
	uint8_t want;	 /* erwartete Laenge */
	uint8_t len;	 /* schon genommene Ziffern */
	uint8_t value;	 /* Ziffer, die gerade gewaehlt wird */
	uint8_t bits;	 /* halbfertige Ziffer beim Weg "lang/kurz" */
	uint8_t bit_len; /* wieviele Bits davon schon da sind */
} osk_pin_entry_t;

typedef enum {
	OSK_PIN_GO_ON = 0, /* weiter eingeben */
	OSK_PIN_READY,	   /* alle Stellen da - jetzt abgeben */
	OSK_PIN_ABORT,	   /* der Nutzer will nicht */
	OSK_PIN_REJECT,	   /* ungueltige Ziffer (10..15): Stelle neu */
} osk_pin_step_t;

/* Laenge pruefen und anfangen. Laenge ausserhalb der Grenzen -> OSK_ERR_ARG,
 * und die Struktur ist trotzdem leer. */
osk_result_t osk_pin_begin(osk_pin_entry_t *entry, uint8_t want);

/* Ueberschreibt die Ziffern. Der Zeiger darf NULL sein. */
void osk_pin_wipe(osk_pin_entry_t *entry);

/* Eine Handlung anwenden. */
osk_pin_step_t osk_pin_apply(osk_pin_entry_t *entry, osk_pin_act_t act);

/* --- Schicht 3: der Vorgang ----------------------------------------------- */

/* Wohin die PIN geht. Umgesetzt wird das im Sicherheitschip (se050/) - nicht
 * hier, und niemals im Flash.
 *
 * DER ZAEHLER: submit() ist der EINZIGE Weg, eine PIN zu pruefen. Der Chip
 * erhoeht seinen Fehlversuchszaehler selbst, bevor er antwortet; es gibt hier
 * keine zweite Tuer, die daran vorbeifuehrt. left() darf nur lesen. Ist der
 * Chip gesperrt, kommt OSK_ERR_LOCKED - und zwar schon vor der ersten
 * Ziffer, damit niemand eine PIN in ein gesperrtes Geraet tippt. */
typedef struct {
	/* Gibt die Ziffern ab. OSK_OK = richtig, OSK_ERR_DENIED = falsch
	 * (Zaehler im Chip ist dann schon erhoeht), OSK_ERR_LOCKED = zu. */
	osk_result_t (*submit)(void *ctx, const uint8_t *digits, size_t len);
	/* Wieviele Versuche bleiben. Darf NULL sein, dann wird nichts angezeigt. */
	osk_result_t (*left)(void *ctx, uint8_t *left_out);
	void *ctx;
} osk_pin_sink_t;

/* Was der Vorgang zum Lesen und Zeichnen braucht. Wieder keine Anzeige und
 * kein Taster - nur Griffe, die der Aufrufer einhaengt. */
typedef struct {
	/* true, solange Taster `button` gedrueckt ist. */
	bool (*button_down)(void *ctx, uint8_t button);
	uint32_t (*now_ms)(void *ctx);
	/* Zeichnet den Stand: Stelle `pos` von `want`, `value` ist die Ziffer,
	 * die gerade gewaehlt wird, `left` die verbleibenden Versuche
	 * (0xFF = unbekannt). Die genommenen Ziffern bekommt das Bild NICHT. */
	void (*draw)(void *ctx, uint8_t pos, uint8_t want, uint8_t value, uint8_t left);
	/* Meldet das Ergebnis. `submitted` sagt, ob der Chip die Ziffern
	 * ueberhaupt gesehen hat - nur dann ist der Zaehler gestiegen. */
	void (*report)(void *ctx, osk_result_t result, bool submitted, uint8_t left);
	void (*idle)(void *ctx);
	void *ctx;
} osk_pin_io_t;

/* Fragt die PIN am Geraet ab und gibt sie an den Chip.
 *
 * Rueckgabe: OSK_OK (richtig), OSK_ERR_DENIED (falsch oder abgebrochen),
 * OSK_ERR_LOCKED (Chip zu), OSK_ERR_TIMEOUT (niemand da), OSK_ERR_ARG.
 * In jedem Fall sind die Ziffern beim Verlassen ueberschrieben. */
osk_result_t osk_pin_ask(const osk_pin_sink_t *sink, const osk_pin_io_t *io,
			 osk_pin_scheme_t scheme, uint8_t want, uint32_t timeout_ms);

/* --- Die Rechnung der drei Wege ------------------------------------------- */

/* Annahmen, aus denen die Zeiten kommen. Mehr steckt nicht dahinter - und sie
 * stehen nur hier, damit docs/pin-am-geraet.md sie nicht ein zweites Mal
 * behauptet. Ein echter Finger ist nicht gemessen worden. */
#define OSK_PIN_T_TAP_MS 120u	  /* Taster unten bei einem kurzen Druck */
#define OSK_PIN_T_RELEASE_MS 150u /* loslassen und zum naechsten Druck */

typedef struct {
	uint16_t presses_worst;
	uint16_t presses_mean; /* gerundet auf ganze Druecke */
	uint32_t ms_worst;
	uint32_t ms_mean;
	uint8_t buttons;
} osk_pin_cost_t;

/* Druecke und Zeit fuer eine PIN von `want` Stellen auf diesem Weg. */
void osk_pin_cost(osk_pin_scheme_t scheme, uint8_t want, osk_pin_cost_t *out);

#endif
