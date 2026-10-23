/* Sicherungsdatei: ein verschluesselter Klumpen, der auf Platte oder NAS liegt
 * und einen neuen Stick aufsetzen kann.
 *
 * Der Schluessel der Datei entsteht aus einem Hauptkennwort mit Argon2id. Er
 * hat nichts mit dem SE050 zu tun - sonst waere die Sicherung an genau das
 * Geraet gebunden, das verloren ging.
 *
 * Die Datei besteht aus einem Kopf im Klartext (Fassung, Argon2-Werte, Salz)
 * und danach Stueck fuer Stueck je einem Eintrag, jedes einzeln verschluesselt.
 * So braucht das Geraet nie mehr als einen Eintrag im RAM. Das letzte Stueck
 * ist der Abschluss: es nennt die Anzahl und den Abdruck aller Stuecke davor -
 * eine abgeschnittene Datei faellt damit auf.
 *
 * Fuer den Notfall laesst sich der Dateischluessel in Umschlaege aufteilen,
 * siehe osk_shamir.h. Angehoerige brauchen dann k von n Umschlaegen und kein
 * Kennwort.
 *
 * Umsetzung: src/classes/store/osk_backup.c */
#ifndef OSK_BACKUP_H
#define OSK_BACKUP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"
#include "osk_crypto.h"
#include "osk_shamir.h"

#define OSK_BACKUP_MAGIC0   0x4B534F23u   /* "#OSK" */
#define OSK_BACKUP_MAGIC1   0x31504142u   /* "BAP1" */
#define OSK_BACKUP_FILE_FMT 1

/* Mindestlaenge des Hauptkennworts in ZEICHEN - nicht in Ziffern. Eine
 * Mindestzahl von Ziffern treibt Leute in Geburtsdaten; Zeichen erlauben eine
 * Wortfolge, die man sich wirklich merkt. */
#define OSK_PASS_MIN       12u
#define OSK_PASS_MAX      256u

/* Vorschlag: acht Woerter, also 64 Bit. */
#define OSK_WORDLIST_LEN  256u
#define OSK_PASS_WORDS      8u
#define OSK_PASS_SUGGEST_MAX 80u

extern const char *const osk_wordlist[OSK_WORDLIST_LEN];

/* Argon2id-Werte. Im Geraet ist der Speicher knapp (256 KB RAM im nRF52840),
 * darum wenig Speicher und viele Durchlaeufe. Ein Verwaltungsprogramm auf dem
 * PC darf die Datei mit den groesseren Werten anlegen; gelesen wird immer mit
 * den Werten, die in der Datei stehen. */
#define OSK_ARGON2_DEV_MEM_KIB   64u
#define OSK_ARGON2_DEV_TIME      64u
#define OSK_ARGON2_HOST_MEM_KIB  65536u
#define OSK_ARGON2_HOST_TIME     3u
/* Was dieses Geraet hoechstens stemmen kann, ohne den Speicher zu sprengen. */
#define OSK_ARGON2_MAX_MEM_KIB   128u

/* Kopf der Datei, im Klartext. Er ist zugleich zusaetzliche Daten der
 * Verschluesselung jedes Stuecks - wer darin etwas aendert, macht die Datei
 * unlesbar, statt sie umzudeuten. */
typedef struct {
	uint32_t magic0;
	uint32_t magic1;
	uint16_t file_format;
	uint16_t vault_format;
	uint32_t m_cost_kib;
	uint32_t t_cost;
	uint32_t lanes;
	uint8_t  salt[OSK_SALT_LEN];
	uint8_t  reserved[8];
	uint32_t crc32;
} osk_backup_head_t;

/* Ein Eintrag in der Datei, im Klartext vor dem Verschluesseln. Das Geheimnis
 * steht dahinter. Fingerabdrucksdaten gibt es hier nicht - sie liegen nie im
 * Tresor und wandern darum auch nie in eine Sicherung. */
typedef struct {
	uint32_t id;
	uint16_t type;
	uint8_t  profile;
	uint8_t  flags;
	uint64_t changed_utc;
	uint64_t counter;
	uint32_t secret_len;
	char     name[64];
	char     site[128];
} osk_backup_item_t;

/* Hinaus und herein. Beide geben OSK_ERR_IO zurueck, wenn die Gegenseite nicht
 * genau len Byte geschafft hat - eine halbe Sicherung gilt nicht. */
typedef osk_result_t (*osk_backup_sink_fn)(void *ctx, const uint8_t *data,
					   size_t len);
typedef osk_result_t (*osk_backup_src_fn)(void *ctx, uint8_t *data, size_t len);

/* Prueft ein Hauptkennwort. OSK_ERR_ARG heisst zu kurz. */
osk_result_t osk_backup_check_pass(const char *pass, size_t len);

/* Schlaegt eine Wortfolge vor. Der Nutzer darf sie annehmen oder eine eigene
 * eingeben; vorgeschlagen wird immer, denn selbst gewaehlte Kennwoerter sind
 * fast immer schwaecher. */
osk_result_t osk_backup_suggest_pass(char *out, size_t cap);

/* Schreibt die Sicherung aller aufgeschlossenen Profile. Holt vorher die
 * Freigabe am Geraet und schreibt den Vorgang ins Protokoll.
 * key_out darf NULL sein; sonst kommt der Dateischluessel zurueck, damit er
 * mit osk_shamir_split in Umschlaege geteilt werden kann. */
osk_result_t osk_backup_write(const char *pass, size_t pass_len,
			      const osk_argon2_params_t *params,
			      osk_backup_sink_fn sink, void *ctx,
			      size_t *written_items, osk_key_t *key_out);

/* Liest eine Sicherung in den geoeffneten Tresor. Vorhandene Eintraege bleiben
 * stehen; die Eintraege aus der Datei kommen hinzu. Verlangt Freigabe. */
osk_result_t osk_backup_read(const char *pass, size_t pass_len,
			     osk_backup_src_fn src, void *ctx,
			     size_t *restored_items);

/* Dasselbe mit einem Schluessel aus Umschlaegen statt mit dem Kennwort. */
osk_result_t osk_backup_read_with_key(const osk_key_t *key,
				      osk_backup_src_fn src, void *ctx,
				      size_t *restored_items);

#endif
