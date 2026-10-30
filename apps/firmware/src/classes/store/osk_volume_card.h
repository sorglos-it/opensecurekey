/* Kartenschnittstelle des Datentraegers - der Baustein oder die microSD, auf
 * der die Bloecke, der Baum und die beiden Wurzelsaetze liegen.
 *
 * Bewusst klein und bewusst NICHT osk_flash.h: der Tresor liegt im QSPI-Flash
 * des Sticks, loescht sektorweise und kennt 0xFF als geloeschten Zustand. Eine
 * SD-Karte tut nichts davon - sie nimmt einen Block und ersetzt ihn, und was
 * Loeschblock und Verschleissausgleich sind, entscheidet ihr eigener
 * Steuerbaustein hinter der Leitung. Wer beides in eine Schnittstelle presst,
 * schreibt entweder Loeschaufrufe, die hier sinnlos sind, oder er nimmt an,
 * der Flash des Sticks ersetze Bloecke an der Stelle.
 *
 * Die Blockgroesse ist OSK_VOL_BLOCK_LEN (4096). Begruendung steht in
 * osk_volume.h; hier steht nur, dass die Karte in genau dieser Koernung
 * angesprochen wird und die Uebersetzung auf 512-Byte-Sektoren Sache des
 * Treibers dahinter ist.
 *
 * Umsetzung auf dem Geraet: noch keine. Im Test: die Attrappe
 * apps/firmware/test/store/fake_card.c.
 *
 * WAS AM ECHTEN GERAET OFFEN BLEIBT - und was eine Attrappe darum nicht
 * beweisen kann:
 *  - Durchsatz. Gerechnet wird mit etwa 3 MB/s (SPI, eine Datenleitung).
 *    Ungemessen; es gibt keine Karte am Brett.
 *  - Die Karte kann einen Block beim Stromausfall HALB ersetzen und liefert
 *    danach weder den alten noch den neuen Inhalt, sondern Mull. Der Entwurf
 *    rechnet damit (Stempel und Baum fallen darauf), die Attrappe stellt es
 *    nach - aber ob eine echte Karte sich so verhaelt und nicht schlimmer
 *    (etwa einen NACHBARBLOCK verliert), ist ungeprueft.
 *  - Zeitablaeufe, Einschaltstrom, Wiederanlauf nach Fehler.
 *  - Eine Karte darf Bloecke stillschweigend woandershin legen. Fuer diesen
 *    Entwurf ist das gleichgueltig, weil er Blocknummern und keine
 *    Flash-Adressen benutzt - gesagt, nicht gemessen. */
#ifndef OSK_VOLUME_CARD_H
#define OSK_VOLUME_CARD_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"

typedef struct {
	uint32_t blocks;      /* Groesse in Bloecken zu OSK_VOL_BLOCK_LEN */
	bool present;	      /* steckt eine Karte? */
	bool write_protected; /* Schreibschutz der Karte selbst */
} osk_card_info_t;

osk_result_t osk_card_info(osk_card_info_t *out);

/* Genau ein Block, genau OSK_VOL_BLOCK_LEN Byte. Halb gefuellt gibt es
 * nicht: entweder OSK_OK und der Puffer ist voll, oder ein Fehler und sein
 * Inhalt ist unbrauchbar. */
osk_result_t osk_card_read(uint32_t block, void *buf, size_t len);
osk_result_t osk_card_write(uint32_t block, const void *buf, size_t len);

/* Erst danach steht das Geschriebene wirklich auf der Karte. Der
 * Festschreibepunkt des Entwurfs haengt daran. */
osk_result_t osk_card_sync(void);

#endif
