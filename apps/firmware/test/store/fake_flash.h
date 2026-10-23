/* Flash-Attrappe im RAM, nur fuer den Test am PC.
 *
 * Sie kann zwei Dinge, die echter Flash auch kann und die sonst niemand
 * nachstellen kann:
 *  - Stromausfall an einer bestimmten Schreibstelle. Die betroffene Schreib-
 *    oder Loeschoperation wird HALB ausgefuehrt und meldet dann einen Fehler.
 *    Danach scheitert jeder weitere Zugriff - das Geraet ist aus.
 *  - Zwei Baenke, damit zwei Geraete in einem Testlauf nebeneinander
 *    bestehen koennen (Stick zu Stick).
 *
 * Ausserdem zaehlt sie Loeschvorgaenge je Sektor - damit laesst sich messen,
 * ob der Verschleiss wirklich verteilt wird. */
#ifndef FAKE_FLASH_H
#define FAKE_FLASH_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

void fake_flash_init(void);
void fake_flash_free(void);

/* Bank 0 oder 1. Jede ist ein eigenes Geraet. */
void fake_flash_select(int bank);
int fake_flash_bank(void);

/* Alles auf 0xFF, Zaehler zurueck. Nur die gewaehlte Bank. */
void fake_flash_wipe(void);

/* Nach n erfolgreichen Schreib- oder Loeschvorgaengen ist der Strom weg.
 * n = 0 schaltet die Nachstellung ab. */
void fake_flash_fail_after(uint32_t n);

/* Wie viele Schreib- und Loeschvorgaenge seit dem letzten Zuruecksetzen. */
uint32_t fake_flash_writes(void);
void fake_flash_count_reset(void);

/* Ist der nachgestellte Stromausfall eingetreten? */
bool fake_flash_is_dead(void);
void fake_flash_revive(void);

/* Loeschvorgaenge eines Sektors, fuer die Verschleissmessung. */
uint32_t fake_flash_erase_count(uint32_t addr);
uint32_t fake_flash_erase_total(void);
uint32_t fake_flash_erase_max(uint32_t area_off, uint32_t area_len);
uint32_t fake_flash_sectors_touched(uint32_t area_off, uint32_t area_len);

#endif
