/* Zugang zum externen QSPI-Flash. Der Tresor kennt nur diese sechs Funktionen;
 * auf dem Geraet liegt dahinter der QSPI-Treiber des nRF Connect SDK, im Test
 * eine Attrappe im RAM (apps/firmware/test/store/fake_flash.c).
 *
 * Regeln des Bausteins, die der Tresor voraussetzt:
 *  - Geloeschter Zustand ist 0xFF.
 *  - Programmieren kann Bits nur von 1 auf 0 setzen.
 *  - Geloescht wird immer ein ganzer Sektor.
 *  - Ein Schreib- oder Loeschvorgang darf jederzeit durch Stromausfall
 *    abbrechen; der Tresor rechnet damit und prueft jede Stelle nach. */
#ifndef OSK_FLASH_H
#define OSK_FLASH_H

#include <stdint.h>
#include <stddef.h>
#include "osk_result.h"

typedef struct {
	uint32_t size;          /* nutzbare Groesse der Tresorpartition in Byte */
	uint32_t sector_size;   /* kleinste loeschbare Einheit */
	uint32_t page_size;     /* kleinste schreibbare Einheit */
} osk_flash_geometry_t;

osk_result_t osk_flash_geometry(osk_flash_geometry_t *out);

/* Adressen sind Offsets ab Anfang der Tresorpartition, nie absolute
 * Flash-Adressen - so weiss der Tresor nichts von der Partitionstabelle. */
osk_result_t osk_flash_read(uint32_t addr, void *buf, size_t len);

/* len muss ein Vielfaches von page_size sein, addr daran ausgerichtet. */
osk_result_t osk_flash_program(uint32_t addr, const void *buf, size_t len);

/* addr muss an sector_size ausgerichtet sein, len ein Vielfaches davon. */
osk_result_t osk_flash_erase(uint32_t addr, size_t len);

/* Sorgt dafuer, dass alles Geschriebene wirklich im Baustein steht. Erst
 * danach darf der naechste Schritt einer Transaktion beginnen. */
osk_result_t osk_flash_sync(void);

#endif
