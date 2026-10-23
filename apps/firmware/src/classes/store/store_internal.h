/* Innenleben des Tresors. Nur die Dateien in diesem Ordner binden das hier
 * ein; nach aussen gelten osk_store.h, osk_log.h und die Erweiterungen
 * osk_store_profiles.h, osk_backup.h, osk_transfer.h. */
#ifndef OSK_STORE_INTERNAL_H
#define OSK_STORE_INTERNAL_H

#include "osk_vault_format.h"
#include "osk_flash.h"
#include "osk_crypto.h"
#include "osk_store.h"
#include "osk_log.h"

/* Kennung des Grundgeheimnisses im SE050. Daraus leitet der Chip den
 * geraetegebundenen Wert ab, der nie herauskommt. */
#define OSK_KEYID_VAULT_ROOT 0x0000F100u

/* Laufender Zustand. Alles Geheime darin wird beim Sperren geloescht. */
typedef struct {
	bool open;
	osk_hdr_t hdr;
	uint32_t hdr_slot;  /* Adresse des gueltigen Kopfsatzes */
	uint64_t nonce_cur; /* naechste herauszugebende Nonce */
	osk_key_t key_dev;  /* geraetegebunden, aus dem SE050 */
	osk_key_t key_log;
	osk_key_t key_profile[OSK_PROFILE_MAX];
	bool profile_open[OSK_PROFILE_MAX];
} osk_store_state_t;

osk_store_state_t *osk_store_state(void);

/* ---- Kopf ------------------------------------------------------------- */

/* Liest den gueltigen Kopfsatz. OSK_ERR_NOT_FOUND heisst: leeres Flash. */
osk_result_t osk_hdr_mount(void);

/* Schreibt den Zustand als neuen Kopfsatz. Das ist der Punkt, an dem eine
 * Transaktion gilt - vorher oder nachher gibt es nichts Halbes. */
osk_result_t osk_hdr_commit(void);

/* Zustand sichern und zurueckdrehen, wenn eine Transaktion scheitert. */
void osk_hdr_snapshot(osk_hdr_t *out);
void osk_hdr_restore(const osk_hdr_t *snap);

/* Legt einen leeren Tresor an. */
osk_result_t osk_hdr_format(uint16_t format);

/* Holt eine Nonce. Der Zaehler im Kopf wird vorher festgehalten. */
osk_result_t osk_nonce_take(osk_nonce_purpose_t purpose, uint8_t out[OSK_NONCE_LEN]);

/* ---- Platz ------------------------------------------------------------ */

/* Stellt sicher, dass der Bereich geloescht ist - auch nach einem Abbruch
 * mitten im Loeschen. */
osk_result_t osk_space_ensure_erased(uint32_t addr, uint32_t len);

/* Sucht Platz im Index-Ring, der keine gueltige Fassung ueberschreibt. */
osk_result_t osk_space_index_alloc(uint32_t len, uint32_t *addr_out);

/* Sucht einen freien Datenplatz. Der Suchzeiger laeuft im Kreis weiter, damit
 * nicht immer dieselben Sektoren beschrieben werden. */
osk_result_t osk_space_slot_alloc(uint32_t *slot_out);

void osk_space_slot_set(uint32_t slot, bool used);
bool osk_space_slot_is_used(uint32_t slot);

/* ---- Index ------------------------------------------------------------ */

/* Der Arbeitspuffer: genau eine Indexfassung liegt im RAM. */
osk_idx_page_t *osk_index_page(void);
int osk_index_loaded_profile(void); /* -1 = keine */
void osk_index_drop(void);

/* Laedt die gueltige Fassung eines Profils in den Arbeitspuffer. Ein leeres
 * Profil ergibt count = 0, keinen Fehler. */
osk_result_t osk_index_load(uint8_t profile);

/* Schreibt den Arbeitspuffer an einen neuen Platz und merkt Adresse und
 * Laenge im Zustand. Gueltig wird die Fassung erst mit osk_hdr_commit(). */
osk_result_t osk_index_store(uint8_t profile);

/* ---- Geheimnisse ------------------------------------------------------ */

osk_result_t osk_blob_write(uint32_t slot, osk_item_id_t id, const uint8_t *secret, size_t len,
			    const osk_key_t *key, uint32_t *ct_len_out,
			    uint8_t nonce_out[OSK_NONCE_LEN]);

osk_result_t osk_blob_read(const osk_idx_rec_t *rec, const osk_key_t *key, uint8_t *out, size_t cap,
			   size_t *len_out);

/* Loescht einen Datenplatz. Erst nach dem Kopfsatz, der ihn freigibt. */
osk_result_t osk_blob_erase(uint32_t slot);

/* ---- Protokoll -------------------------------------------------------- */

osk_result_t osk_log_add_internal(uint16_t what, osk_item_id_t id, const char *asked_by);

/* Laesst den Ringkopf neu suchen - nach dem Formatieren und im Test. */
void osk_log_reset_mount(void);

/* ---- Wanderung -------------------------------------------------------- */

/* Rechnet eine geladene Indexfassung einer alten Fassung auf die jetzige um,
 * im RAM. So bleibt ein alter Tresor lesbar, noch bevor er umgeschrieben
 * wird. */
osk_result_t osk_index_upgrade(uint16_t from_format, size_t plain_len);

/* Groesse eines Indexsatzes der alten Fassungen. */
#define OSK_IDX_REC_V1_SIZE 240u
#define OSK_IDX_REC_V2_SIZE 256u

/* ---- Hilfen ----------------------------------------------------------- */

void osk_str_copy(char *dst, size_t cap, const char *src);
uint64_t osk_now_utc(void);

/* Zeitquelle. Auf dem Geraet die Uhr des nRF52840, im Test ein fester Wert. */
void osk_time_set_source(uint64_t (*fn)(void));

#endif
