/* Attrappen fuer den Pruefsatz des Laufwerks: die Transportliste und ein
 * Laufwerk im RAM. Kein Fremdcode, keine Abhaengigkeit. */
#ifndef OSK_MSC_MOCKS_H
#define OSK_MSC_MOCKS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"
#include "osk_transport.h"
#include "usb_msc.h"

/* --- Transportliste --------------------------------------------------- */

void mock_transport_reset(void);
/* Welcher Transport sich zuletzt angemeldet hat, und ob ueberhaupt. */
bool mock_transport_registered(osk_link_t link);
/* Letzter Stand von osk_transport_enable fuer diesen Transport. -1 = nie
 * gerufen, 0 = aus, 1 = an. */
int mock_transport_enabled(osk_link_t link);

/* --- Laufwerk im RAM --------------------------------------------------- */

/* Legt ein Laufwerk mit so vielen Bloecken an. Jeder Block traegt ein
 * Muster, aus dem sich seine Nummer ablesen laesst - so zeigt ein Test
 * nicht nur, DASS Daten kamen, sondern dass es die richtigen waren. */
void fake_drive_init(uint32_t blocks);
void fake_drive_free(void);
uint32_t fake_drive_blocks(void);

/* Diesen Block beim naechsten Lesen scheitern lassen. UINT32_MAX = keiner. */
void fake_drive_fail_at(uint32_t lba);

/* Wie oft gelesen wurde - damit sich belegen laesst, dass ein abgewiesener
 * Befehl den Flash nicht einmal angefasst hat. */
uint32_t fake_drive_reads(void);
void fake_drive_reads_reset(void);

/* Hat der Flash je einen Schreibbefehl gesehen? Die Attrappe kann gar nicht
 * schreiben - die Frage ist, ob jemand es versucht hat. */
bool fake_drive_was_written(void);

/* Das erwartete Muster eines Blocks, zum Vergleich im Test. */
void fake_drive_pattern(uint32_t lba, uint8_t out[OSK_MSC_BLOCK_LEN]);

osk_result_t fake_drive_read(uint32_t lba, uint8_t out[OSK_MSC_BLOCK_LEN], void *ctx);

/* --- Mitschrift der gesendeten Bytes ----------------------------------- */

#define MOCK_SINK_MAX (300u * OSK_MSC_BLOCK_LEN)

void mock_sink_reset(void);
void mock_sink_send(const uint8_t *data, size_t len, void *ctx);
size_t mock_sink_len(void);
const uint8_t *mock_sink_data(void);
/* Zahl der Aufrufe - ein CSW ohne Datenphase ist genau einer. */
unsigned mock_sink_calls(void);

/* Das letzte CSW aus der Mitschrift. false, wenn keines da ist. */
bool mock_sink_csw(uint32_t *tag_out, uint32_t *residue_out, uint8_t *status_out);

/* --- Bequemlichkeit fuer die Faelle ------------------------------------ */

/* Baut ein CBW und schickt es hinein. cdb_len 0 heisst: die Laenge aus
 * cdb_len_field nehmen, damit auch krumme Angaben pruefbar sind. */
void mock_cbw(uint32_t tag, uint32_t host_len, bool in, uint8_t lun, const uint8_t *cdb,
	      uint8_t cdb_len);

#endif
