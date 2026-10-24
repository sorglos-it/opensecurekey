/* Der eine Taster: kurz, lang, doppelt - und das Prellen weg.
 *
 * Reine Rechnung ohne Hardware: der Aufrufer sagt, ob der Taster gerade
 * gedrueckt ist und wieviele Millisekunden die Uhr zeigt. Darum laeuft
 * dieselbe Logik am Geraet und im Pruefsatz, wo die Zeit eine Zahl ist.
 *
 * Die Prellzeit des verbauten Tasters (EVQP7A01P) ist mit hoechstens 10 ms
 * angegeben - siehe hardware/pinout.md. Wir warten 15 ms. */
#ifndef OSK_BUTTON_H
#define OSK_BUTTON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define OSK_BTN_DEBOUNCE_MS 15
#define OSK_BTN_LONG_MS 800
#define OSK_BTN_DOUBLE_MS 350

typedef enum {
	OSK_BTN_NONE = 0,
	OSK_BTN_SHORT,
	OSK_BTN_LONG,
	OSK_BTN_DOUBLE,
} osk_btn_event_t;

typedef struct {
	bool stable;	    /* entprellter Zustand */
	bool raw;	    /* letzter gelesener Zustand */
	uint32_t raw_since; /* seit wann der gelesene Zustand anliegt */
	uint32_t down_at;   /* wann der entprellte Druck begann */
	bool waiting;	    /* ein kurzer Druck wartet auf einen zweiten */
	uint32_t waiting_since;
	bool long_sent; /* der lange Druck wurde schon gemeldet */
} osk_button_t;

void osk_button_init(osk_button_t *btn, uint32_t now_ms);

/* Einmal je Durchlauf aufrufen. Liefert hoechstens ein Ereignis.
 *
 * Der lange Druck wird gemeldet, sobald die Zeit erreicht ist - noch waehrend
 * der Finger liegt. Das ist Absicht: der Nutzer sieht die Freigabe, ohne zu
 * raten, wie lange er halten muss.
 * Der kurze Druck wird erst gemeldet, wenn das Fenster fuer einen zweiten
 * Druck verstrichen ist - sonst waere jeder Doppeldruck auch ein kurzer. */
osk_btn_event_t osk_button_feed(osk_button_t *btn, bool pressed, uint32_t now_ms);

#endif
