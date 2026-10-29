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

/* Lesezugriffe in einem Bereich, Sektor fuer Sektor gezaehlt. Damit laesst
 * sich belegen, dass ein Weg einen Bereich NICHT angefasst hat - etwa
 * osk_store_public(), das den Datenplatz des Geheimnisses nie liest. */
void fake_flash_read_reset(void);
uint32_t fake_flash_reads_in(uint32_t area_off, uint32_t area_len);

/* Ein vollstaendiges Abbild der gewaehlten Bank sichern und spaeter wieder
 * einspielen. Damit laesst sich der eigentliche Angriff auf den
 * Rueckdrehschutz nachstellen: ein in sich VOELLIG GUELTIGER alter Zustand -
 * richtige Pruefsummen, richtige Pruefwerte, richtiges Format -, nicht ein
 * verfaelschter. Gegen einen verfaelschten hilft die Verschluesselung schon.
 *
 * Was NICHT mitgesichert wird, ist der SE050: die Zaehler dort bleiben
 * stehen, und genau daran faellt das Zurueckspielen auf. */
void fake_flash_snapshot(void);
void fake_flash_rollback(void);

/* Loeschvorgaenge eines Sektors, fuer die Verschleissmessung. */
uint32_t fake_flash_erase_count(uint32_t addr);
uint32_t fake_flash_erase_total(void);
uint32_t fake_flash_erase_max(uint32_t area_off, uint32_t area_len);
uint32_t fake_flash_sectors_touched(uint32_t area_off, uint32_t area_len);

#endif
