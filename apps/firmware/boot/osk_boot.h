/* Firmware-Update: Paket, Zustand und Entscheidung des Bootloaders.
 *
 * Zwei Plaetze: der laufende liegt im internen Flash des nRF52840, der
 * zweite im externen QSPI-Flash (siehe docs/architecture.md, "How much room
 * is there, really"). Neues wird NEBEN das Laufende geschrieben, geprueft
 * und erst dann umgeschaltet. Bricht etwas ab - Stecker gezogen, Akku leer,
 * Abbild halb geschrieben -, startet die alte Fassung weiter. Das ist die
 * eigentliche Anforderung; alles andere in dieser Datei dient ihr.
 *
 * Diese Schicht enthaelt KEINE Krypto. Signatur und Hash kommen ueber
 * osk_boot_backend_t von aussen: im Geraet aus dem SDK (MCUboot bringt
 * ed25519 und SHA-256 mit), in den Pruefungen aus Attrappen. So steht hier
 * nichts selbst Erfundenes und nichts Abgeschriebenes.
 *
 * Umsetzung: apps/firmware/boot/ - Beschreibung in docs/update.md */
#ifndef OSK_BOOT_H
#define OSK_BOOT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"

/* --- Paketkopf ------------------------------------------------------------
 *
 * Ein Paket ist: Kopf (72 Byte) · Signatur (64 Byte) · Abbild. Die Signatur
 * deckt NUR den Kopf; im Kopf steht der SHA-256 des Abbilds. Damit ist die
 * Herkunft geprueft, bevor das erste Byte des Abbilds angefasst wird, und
 * das Abbild selbst kann im Vorbeischreiben gehasht werden - es muss nicht
 * ganz in den Arbeitsspeicher passen (es ist groesser als der RAM).
 *
 * Alle Zahlen sind little endian, wie beide Chips und tools/sign.py sie
 * schreiben. */

#define OSK_BOOT_MAGIC "OSKUPD1" /* 7 Zeichen + abschliessende Null */
#define OSK_BOOT_MAGIC_LEN 8
#define OSK_BOOT_HEADER_LEN 72
#define OSK_BOOT_SIG_LEN 64
#define OSK_BOOT_HASH_LEN 32
#define OSK_BOOT_FORMAT 1

/* Brettkennung. Ein Abbild fuer ein anderes Brett wird abgelehnt, auch wenn
 * es richtig unterschrieben ist: die Belegung der Anschluesse unterscheidet
 * sich, und eine Firmware, die den falschen Anschluss als Taster liest,
 * kann nicht mehr freigeben. */
typedef enum {
	OSK_BOARD_UNKNOWN = 0,
	OSK_BOARD_NRF52840DK = 1,
	OSK_BOARD_NRF52840DONGLE = 2,
	OSK_BOARD_OSK_V1 = 3, /* das eigene Brett, hardware/ */
} osk_board_t;

typedef struct {
	uint16_t header_len;
	uint16_t format;
	uint32_t board_id;
	uint32_t version[4]; /* v.x.y.z nach claude-md/version.md */
	uint32_t counter;    /* Rueckschritt-Sperre, steigt nie ab */
	uint32_t image_len;
	uint8_t image_hash[OSK_BOOT_HASH_LEN];
} osk_boot_header_t;

/* --- Was von aussen kommt -------------------------------------------------
 *
 * Krypto und Flash sind eingehaengt, nicht eingebaut. Fehlt ein Zeiger,
 * antwortet jede Pruefung OSK_ERR_ARG - und nicht etwa "in Ordnung". */
typedef struct {
	/* Prueft die ed25519-Signatur gegen den eingebauten oeffentlichen
	 * Schluessel. true = gueltig. Der private Teil liegt ausserhalb des
	 * Projektordners und nie im Repo. */
	bool (*verify)(const uint8_t *msg, size_t msg_len, const uint8_t *sig);
	/* SHA-256 ueber einen Datenstrom: init, update, final. */
	void (*hash_init)(void *ctx);
	void (*hash_update)(void *ctx, const uint8_t *data, size_t len);
	void (*hash_final)(void *ctx, uint8_t out[OSK_BOOT_HASH_LEN]);
	void *hash_ctx;

	/* Monotoner Zaehler im SE050. Im Flash waere er zurueckzusetzen -
	 * wer den Flash loescht, duerfte sonst wieder eine alte Fassung
	 * aufspielen. Darum gehoert er in den Chip. */
	osk_result_t (*counter_read)(uint32_t *out);
	osk_result_t (*counter_raise)(uint32_t value);

	/* Dieses Brett und die laufende Fassung - zum Vergleich mit dem Kopf. */
	uint32_t board_id;
	uint32_t running_version[4];
} osk_boot_backend_t;

/* Kopf einlesen und pruefen, in dieser Reihenfolge: Laenge, Kennung, Format,
 * Kopflaenge, Signatur, Brett, Rueckschritt. Die Reihenfolge ist Absicht -
 * ein Paket unbekannter Herkunft wird abgewiesen, bevor irgendein Feld
 * daraus benutzt wird.
 *
 * `raw` zeigt auf Kopf + Signatur, also mindestens 136 Byte. Das Abbild
 * wird hier nicht angefasst.
 *
 * OSK_ERR_FORMAT   Kennung, Format oder Laenge passen nicht
 * OSK_ERR_DENIED   Signatur falsch oder fehlt
 * OSK_ERR_UNSUPPORTED  Abbild fuer ein anderes Brett
 * OSK_ERR_EXISTS   Rueckschritt: gleiche oder aeltere Fassung */
osk_result_t osk_boot_header_check(const osk_boot_backend_t *be, const uint8_t *raw, size_t raw_len,
				   osk_boot_header_t *out);

/* Vergleicht zwei Fassungen: <0, 0, >0 wie strcmp. */
int osk_boot_version_cmp(const uint32_t a[4], const uint32_t b[4]);

/* --- Zustand des Updates --------------------------------------------------
 *
 * Der Zustand liegt in einer eigenen kleinen Partition - NICHT im Tresor.
 * Zwei Abdruecke wechseln sich ab (A und B). Geschrieben wird immer der
 * aeltere; erst wenn er vollstaendig und mit richtiger Pruefsumme steht,
 * gilt er. Bricht das Schreiben ab, bleibt der andere gueltig, und das
 * Geraet startet, was vorher lief. Das ist der Grund fuer zwei Abdruecke:
 * ein einziger kann halb geschrieben sein, und dann ist er weg. */

typedef enum {
	OSK_BOOT_IDLE = 0,	/* nichts im Gange */
	OSK_BOOT_WRITING = 1,	/* Abbild wird in den zweiten Platz geschrieben */
	OSK_BOOT_READY = 2,	/* Abbild vollstaendig und geprueft, Umschalten erbeten */
	OSK_BOOT_TESTING = 3,	/* umgeschaltet, Bestaetigung der neuen Fassung fehlt noch */
	OSK_BOOT_CONFIRMED = 4, /* neue Fassung hat sich gemeldet */
} osk_boot_stage_t;

#define OSK_BOOT_STATE_MAGIC 0x4B425355u /* "USBK" little endian */
#define OSK_BOOT_SLOT_LEN 128		 /* je Abdruck, Rest ist 0 */

typedef struct {
	uint32_t magic;
	uint32_t seq; /* hoeher = neuer; 0 gilt als leer */
	uint8_t stage;
	uint8_t tries; /* Startversuche der neuen Fassung, ohne Bestaetigung */
	uint8_t pad[2];
	uint32_t counter;		 /* Zaehler des Pakets im zweiten Platz */
	uint32_t version[4];		 /* dessen Fassung */
	uint32_t written;		 /* schon geschriebene Byte des Abbilds */
	uint32_t image_len;		 /* erwartete Gesamtlaenge */
	uint8_t hash[OSK_BOOT_HASH_LEN]; /* erwarteter SHA-256 */
	uint32_t crc;			 /* ueber alles davor */
} osk_boot_state_t;

/* Lesen und Schreiben des Zustands. Beide Abdruecke werden uebergeben, so
 * wie sie auf dem Flash liegen - diese Schicht kennt kein Flash. */
typedef struct {
	uint8_t a[OSK_BOOT_SLOT_LEN];
	uint8_t b[OSK_BOOT_SLOT_LEN];
} osk_boot_slots_t;

/* Waehlt den gueltigen Abdruck. Ist keiner gueltig, kommt IDLE mit seq 0
 * heraus und OSK_OK - ein frisches Geraet ist kein Fehler. */
osk_result_t osk_boot_state_load(const osk_boot_slots_t *slots, osk_boot_state_t *out);

/* Schreibt `next` in den aelteren der beiden Abdruecke. `which_out` sagt,
 * welcher beschrieben wurde (0 = a, 1 = b), damit der Aufrufer nur diesen
 * Bereich anfasst. Die Folgenummer wird hier gesetzt. */
osk_result_t osk_boot_state_store(osk_boot_slots_t *slots, osk_boot_state_t *next, int *which_out);

uint32_t osk_boot_crc32(const uint8_t *data, size_t len);

/* --- Entscheidung beim Start ---------------------------------------------
 *
 * Das ist die Frage, die der Bootloader stellt: was starte ich? */
typedef enum {
	OSK_START_PRIMARY = 0,	/* die laufende Fassung im internen Flash */
	OSK_START_SWAP = 1,	/* zweiten Platz hereinholen, dann starten */
	OSK_START_ROLLBACK = 2, /* Umschalten war erfolglos: zurueck auf die alte */
	OSK_START_RECOVERY = 3, /* Bootloader-Betrieb: Taste war beim Einstecken gedrueckt */
} osk_boot_start_t;

/* Wie oft die neue Fassung starten darf, ohne sich zu melden. Danach wird
 * zurueckgedreht. Eine Fassung, die nicht einmal bis zur Bestaetigung
 * kommt, ist kaputt - und dann ist der Stick ohne diesen Rueckweg hin. */
#define OSK_BOOT_MAX_TRIES 3

/* `button_held` kommt vom Taster, zum Zeitpunkt des Einsteckens gelesen.
 * Er hat Vorrang vor allem anderen: der Notfallweg muss auch dann gehen,
 * wenn der Zustand unlesbar ist. */
osk_boot_start_t osk_boot_decide(const osk_boot_state_t *st, bool button_held);

/* Nach dem Umschalten: der Startversuch wird gezaehlt, bevor die neue
 * Fassung laeuft. Nur so kostet ein Absturz einen Versuch - sonst startet
 * eine abstuerzende Fassung endlos. */
osk_result_t osk_boot_mark_try(osk_boot_slots_t *slots);

/* Die neue Fassung hat sich gemeldet: Zaehler im SE050 anheben und den
 * Zustand auf CONFIRMED setzen. Erst hier wird der Rueckschritt gesperrt -
 * vorher waere ein Abbild, das nie gestartet ist, schon als neuester Stand
 * verbucht und der Rueckweg zugemauert. */
osk_result_t osk_boot_confirm(const osk_boot_backend_t *be, osk_boot_slots_t *slots);

/* --- Tresor ---------------------------------------------------------------
 *
 * Das Update fasst die Tresor-Partition nicht an. Diese Funktion sagt, ob
 * ein Bereich beschrieben werden DARF; der Flash-Treiber fragt sie vor
 * jedem Schreiben und jedem Loeschen. Eine Pruefung dazu liegt in
 * apps/firmware/test/boot/test_vault.c. */
typedef struct {
	uint32_t start;
	uint32_t len;
} osk_boot_area_t;

/* Die Bereiche des externen Flash, die dem Update gehoeren. Alles andere -
 * voran der Tresor - ist fuer das Update gesperrt. */
bool osk_boot_may_write(const osk_boot_area_t *allowed, size_t count, uint32_t start, uint32_t len);

/* Hat die Formatfassung des Tresors sich geaendert, wandert er beim ersten
 * Start der neuen Firmware (osk_store_migrate). Das Update selbst wandert
 * nichts - es weiss nicht, ob es ueberhaupt bestaetigt wird. */

#endif
