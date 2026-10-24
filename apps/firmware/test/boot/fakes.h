/* Attrappen fuer die Update-Pruefungen: Flash, Signaturpruefer, Hash und
 * der Zaehler im SE050. Keine davon ist Krypto - sie sind da, um
 * ENTSCHEIDUNGEN pruefbar zu machen, nicht um etwas zu sichern.
 *
 * Der Hash ist ausdruecklich kein SHA-256: er ist eine kurze, eindeutige
 * Funktion ueber den Datenstrom. Geprueft wird damit, dass ein falscher
 * Hash zum Verwerfen fuehrt und ein richtiger nicht - und das ist, was
 * dieser Code entscheidet. SHA-256 selbst kommt im Geraet aus dem SDK. */
#ifndef BOOT_FAKES_H
#define BOOT_FAKES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_boot.h"
#include "osk_boot_session.h"

/* --- Flash (zweiter Platz im externen Baustein) --------------------------- */

#define FAKE_SLOT_LEN (64u * 1024u) /* klein gehalten; die echte Groesse steht in pm_static.yml */

typedef struct {
	uint8_t data[FAKE_SLOT_LEN];
	uint32_t written_bytes; /* insgesamt angenommene Byte */
	uint32_t erases;
	/* Abbruch nachstellen: ab diesem Byte nimmt der Flash nichts mehr an.
	 * 0 = nie abbrechen. Teilweise Annahme wird nachgebildet: was noch
	 * unter die Grenze passt, wird geschrieben, der Rest nicht. */
	uint32_t fail_after;
	bool erase_fails;
} fake_flash_t;

void fake_flash_reset(fake_flash_t *f);
osk_result_t fake_flash_erase(void *ctx, uint32_t offset, uint32_t len);
osk_result_t fake_flash_write(void *ctx, uint32_t offset, const uint8_t *data, size_t len);

/* --- Signaturpruefer und Hash --------------------------------------------- */

/* Gueltig ist eine Signatur, wenn sie der "Unterschrift" entspricht, die
 * fake_sign() aus der Nachricht bildet. Ein Byte daneben ist ungueltig. */
void fake_sign(const uint8_t *msg, size_t len, uint8_t sig[OSK_BOOT_SIG_LEN]);
bool fake_verify(const uint8_t *msg, size_t len, const uint8_t *sig);

typedef struct {
	uint64_t acc;
	uint64_t count;
} fake_hash_ctx_t;

void fake_hash_init(void *ctx);
void fake_hash_update(void *ctx, const uint8_t *data, size_t len);
void fake_hash_final(void *ctx, uint8_t out[OSK_BOOT_HASH_LEN]);
/* Derselbe Hash in einem Zug - zum Fuellen des Kopfes in den Pruefungen. */
void fake_hash_all(const uint8_t *data, size_t len, uint8_t out[OSK_BOOT_HASH_LEN]);

/* --- Zaehler im SE050 ----------------------------------------------------- */

void fake_counter_set(uint32_t value);
uint32_t fake_counter_get(void);
void fake_counter_break(bool broken); /* Chip antwortet nicht mehr */
osk_result_t fake_counter_read(uint32_t *out);
osk_result_t fake_counter_raise(uint32_t value);

/* --- Zusammenbau --------------------------------------------------------- */

/* Eine vollstaendige Umgebung: Pruefer, Hash, Zaehler, Brett und laufende
 * Fassung. `hash_ctx` zeigt auf `hash` im selben Gebilde. */
typedef struct {
	osk_boot_backend_t be;
	fake_hash_ctx_t hash;
} fake_env_t;

void fake_env_init(fake_env_t *env, uint32_t board_id, const uint32_t running[4]);

/* Baut ein Paket: Kopf, richtige Signatur, Abbild. Gibt die Gesamtlaenge
 * zurueck. `buf` muss OSK_BOOT_HEADER_LEN + OSK_BOOT_SIG_LEN + image_len
 * gross sein. */
size_t fake_pkg(uint8_t *buf, size_t cap, uint32_t board_id, const uint32_t version[4],
		uint32_t counter, const uint8_t *image, uint32_t image_len);

/* Eine laufende Sitzung mit diesem Flash vorbereiten. */
void fake_session_init(osk_boot_session_t *se, fake_flash_t *flash);

#endif
