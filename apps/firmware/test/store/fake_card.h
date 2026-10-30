/* Karten-Attrappe im RAM, nur fuer den Test am PC.
 *
 * Sie stellt die KARTENSCHNITTSTELLE nach (osk_volume_card.h), nicht den
 * Flash des Sticks: ein Block wird an der Stelle ersetzt, es gibt kein
 * Loeschen und keinen 0xFF-Zustand.
 *
 * WAS SIE NICHT BEWEIST, und das ist der Punkt: sie prueft die eigene Seite
 * des Vertrags. Ob eine echte microSD sich so verhaelt - Zeitablaeufe,
 * halb ersetzte Bloecke, verlorene Nachbarbloecke, Durchsatz - ist offen, es
 * gibt keine Karte am Brett. Die Liste der offenen Annahmen steht in
 * osk_volume_card.h.
 *
 * Zwei Dinge kann sie, die sonst niemand nachstellen kann:
 *  - einen Block hinter dem Ruecken des Datentraegers veraendern (so arbeitet
 *    ein Angreifer mit der Karte in der Hand),
 *  - einen alten Stand der ganzen Karte zuruecklegen (der Rueckdreh). */
#ifndef FAKE_CARD_H
#define FAKE_CARD_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

void fake_card_init(uint32_t blocks);
void fake_card_free(void);
void fake_card_eject(void);
void fake_card_insert(void);
void fake_card_write_protect(bool on);

/* Zaehler, damit sich Kosten MESSEN lassen statt schaetzen: wie viele
 * Lesevorgaenge eine Pfadpruefung wirklich kostet. */
void fake_card_count_reset(void);
uint32_t fake_card_reads(void);
uint32_t fake_card_writes(void);

/* Ein Byte eines Blockes umkippen - der veraenderte Block. */
void fake_card_flip(uint32_t block, uint32_t offset);

/* Ganze Karte sichern und zuruecklegen: der Rueckdreh. Der Sicherheitschip
 * wird dabei NICHT mitgesichert - genau daran faellt er in Modus 1 auf. */
void fake_card_snapshot(void);
void fake_card_rollback(void);

/* Zwei Karten in einem Lauf, damit sich Stick zu Stick pruefen laesst. */
void fake_card_select(int slot);

#endif
