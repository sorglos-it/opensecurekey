/* Der verschluesselte Datentraeger: drei Betriebsarten, ein Merkle-Baum, eine
 * Wurzel. Entwurf und Begruendung aller Zahlen: docs/datentraeger-konzept.md.
 * Hier steht der Vertrag, nicht die Begruendung.
 *
 * NICHTS DAVON IST AN HARDWARE GELAUFEN. Es gibt keine Karte und kein Brett.
 * Was die Pruefsaetze unter apps/firmware/test/store/ zeigen, ist: der Ablauf
 * stimmt gegen Attrappen fuer Karte, Krypto und Sicherheitschip.
 *
 * Die drei Arten unterscheiden sich NUR darin, wie - und ob - der
 * Datenschluessel eingewickelt ist. Baum und Signatur haben alle drei:
 *
 *   1  nur dieser Stick   lesen kann nur der Stick, der beschrieben hat   geheim
 *   2  mit Kartenwort     lesen kann jeder Stick, der das Wort kennt      geheim
 *   3  nur gesiegelt      lesen kann jeder Stick                    NICHT geheim
 *
 * ZWEI GRENZEN, die diese Schnittstelle in ihren Namen tragen muss und nicht
 * nur im Kommentar:
 *
 * 1. ECHTHEIT IST NICHT URHEBERSCHAFT. Darum gibt es hier kein verify().
 *    Es gibt osk_volume_root_unchanged() - "die Daten sind seit dem
 *    Signieren unveraendert" - und davon getrennt osk_volume_sender(), das
 *    sagt, WER signiert hat und ob dieser Stick den Absender schon kennt.
 *    Beides zusammen in einer Funktion waere die Luege, die dieses Geraet
 *    nicht sagen darf.
 *
 * 2. GEPRUEFT WIRD BEIM LESEN, NICHT BEIM EINSTECKEN. Es gibt darum keine
 *    Funktion, die "die Karte prueft" - 16 GiB vollstaendig zu pruefen heisst,
 *    sie ganz zu lesen. Was es gibt: osk_volume_root_unchanged() beim
 *    Einstecken (Millisekunden) und osk_volume_block_read(), das je Block
 *    seinen Pfad prueft. Die vollstaendige Pruefung ist ein eigener,
 *    angestossener Vorgang mit genannter Dauer: osk_volume_full_check_*. */
#ifndef OSK_VOLUME_H
#define OSK_VOLUME_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"
#include "osk_crypto.h"
#include "osk_se050.h"

/* --- Masze. Jede Zahl steht genau hier. ------------------------------- */

/* 4096: Flash-Seite des Projekts, Vielfaches des 512-B-Sektors, den der
 * Rechner sieht, und die Zuordnungseinheit des FAT32 nach aussen. Damit
 * faellt Lesen-Aendern-Schreiben fuer Nutzdaten weg. */
#define OSK_VOL_BLOCK_LEN 4096u

/* Ein Eintrag im Stempelfeld je Datenblock. */
#define OSK_VOL_LEAF_LEN 16u	 /* Poly1305-Stempel (1,2) oder SHA-256-Anfang (3) */
#define OSK_VOL_COUNTER_LEN 4u	 /* Schreibzaehler des Blocks, steckt in der Nonce */
#define OSK_VOL_LEAF_RESERVE 12u /* Formatfassung, Zustandsbits */
#define OSK_VOL_LEAF_ENTRY_LEN (OSK_VOL_LEAF_LEN + OSK_VOL_COUNTER_LEN + OSK_VOL_LEAF_RESERVE)

/* Je 4-KiB-Seite: 128 Stempeleintraege, 128 Hashes. Beide Zahlen fallen aus
 * den Maszen und werden nicht zweimal hingeschrieben. */
#define OSK_VOL_LEAVES_PER_PAGE (OSK_VOL_BLOCK_LEN / OSK_VOL_LEAF_ENTRY_LEN)
#define OSK_VOL_FANOUT (OSK_VOL_BLOCK_LEN / OSK_HASH_LEN)

/* Ebenen ueber dem Stempelfeld. 16 GiB brauchen 3; vier ist Luft fuer eine
 * Karte, die es heute nicht gibt. */
#define OSK_VOL_LEVELS_MAX 4u

/* Empfaengerzeilen im Kopf. Mehr braucht keine der drei Arten. */
#define OSK_VOL_RECV_MAX 4u

/* Zehn Woerter aus der Liste von 256 - 80 Bit. Begruendung: nicht strecken,
 * sondern Entropie erzeugen (osk_wordlist.c). */
#define OSK_VOL_WORD_COUNT 10u

/* --- Die drei Betriebsarten ------------------------------------------- */

typedef enum {
	OSK_VOL_MODE_DEVICE = 1, /* nur dieser Stick */
	OSK_VOL_MODE_WORD = 2,	 /* jeder Stick mit dem Kartenwort */
	OSK_VOL_MODE_SEALED = 3, /* jeder Stick, NICHT geheim */
} osk_vol_mode_t;

/* Die eine Frage, die der Nutzer verwechseln kann. Sie steht als Funktion da
 * und nicht als Kommentar, damit die Anzeige sie stellen MUSS. */
bool osk_vol_mode_secret(osk_vol_mode_t mode);

/* Kurzname der Art, fuer Anzeige und Protokoll. Modus 3 heiszt hier NICHT
 * "gesiegelt" allein, sondern "OFFEN - nur gesiegelt": wer nur das erste Wort
 * liest, soll nicht das Falsche verstehen. */
const char *osk_vol_mode_text(osk_vol_mode_t mode);

/* --- Empfaengerzeilen, Modell von age --------------------------------- */

typedef enum {
	OSK_VOL_RECV_DEVICE = 1, /* Chipschluessel dieses Sticks */
	OSK_VOL_RECV_WORD = 2,	 /* aus dem Kartenwort abgeleitet */
	OSK_VOL_RECV_STICK = 3,	 /* X25519 an einen anderen Stick */
} osk_vol_recv_kind_t;

typedef struct {
	osk_vol_recv_kind_t kind;
	/* Nur bei OSK_VOL_RECV_STICK: oeffentlicher Schluessel des Empfaengers. */
	uint8_t peer_pub[OSK_KEY_LEN];
	/* Nur bei OSK_VOL_RECV_WORD: das Kartenwort als Text. Es steht nirgends
	 * auf der Karte und wird auf einem anderen Weg mitgeteilt. */
	const char *word;
	size_t word_len;
} osk_vol_recipient_t;

/* --- Was bewiesen ist, und was nicht ---------------------------------- */

typedef enum {
	OSK_VOL_SENDER_SELF = 0,    /* dieser Stick hat beschrieben */
	OSK_VOL_SENDER_KNOWN = 1,   /* fremder Stick, Schluessel im Tresor gemerkt */
	OSK_VOL_SENDER_UNKNOWN = 2, /* fremder Stick, nie gesehen */
} osk_vol_sender_state_t;

typedef struct {
	osk_vol_sender_state_t state;
	/* Fingerabdruck des Absenderschluessels - das, was die Anzeige nennt. */
	uint8_t fingerprint[OSK_HASH_LEN];
	uint32_t version;    /* Fassungsnummer aus der Wurzel */
	uint64_t stamped_at; /* Zeitstempel des Absenders. SEINE Uhr, nicht unsere. */
} osk_vol_sender_t;

/* Aussagen, die die Anzeige machen darf. Jede andere ist zu viel. Sie liegen
 * als Aufzaehlung hier, damit die Oberflaeche sie nicht selbst formulieren
 * muss und dabei "Karte geprueft" daraus wird. */
typedef enum {
	OSK_VOL_CLAIM_ROOT_SEALED,  /* Siegel gueltig */
	OSK_VOL_CLAIM_ROOT_BAD,	    /* Siegel ungueltig */
	OSK_VOL_CLAIM_WILL_CHECK,   /* Daten werden beim Lesen geprueft */
	OSK_VOL_CLAIM_BLOCK_OK,	    /* dieser Block geprueft - EIN Block, nicht die Karte */
	OSK_VOL_CLAIM_BLOCK_BAD,    /* Block nicht gueltig, Lesen abgebrochen */
	OSK_VOL_CLAIM_OLDER_SEEN,   /* aelterer Stand als zuletzt gesehen */
	OSK_VOL_CLAIM_FULL_CHECKED, /* vollstaendig geprueft, mit Datum */
} osk_vol_claim_t;

const char *osk_vol_claim_text(osk_vol_claim_t claim);

/* --- Auflage an die Anzeige: der Modus wird VOR dem Schreiben genannt --
 *
 * Das ist die Auflage aus dem Entwurf, und sie steht als Vertrag da, weil
 * sie sonst niemand einhaelt: wer glaubt verschluesselt zu haben und hat nur
 * gesiegelt, gibt Geheimnisse weg. Der Fehler entsteht aus einer
 * Verwechslung, nicht aus einem Angriff.
 *
 * Darum ruft osk_volume_write_enable() diese Rueckmeldung und gibt das
 * Schreiben nur frei, wenn sie OSK_OK fuer GENAU DIESE Betriebsart liefert.
 * Ohne Rueckmeldung gibt es keine Schreibfreigabe - nicht "dann eben ohne
 * Anzeige".
 *
 * Was die Anzeige leisten muss (und was diese Datei nicht erzwingen kann,
 * weil sie die Anzeige nicht ist - der ui-Strang baut es):
 *  - Den Modus im Klartext nennen, bei 3 als "OFFEN - nur gesiegelt".
 *  - Fuer Modus 3 eine deutlich ANDERE Darstellung, nicht dieselbe Zeile mit
 *    einem anderen Wort darin.
 *  - Der Tastendruck bestaetigt genau diesen Text, nicht "weiter".
 *  - In Modus 2 die Zahl der Empfaenger nennen - die Kette ist so stark wie
 *    ihr schwaechstes Glied.
 *  - Erst zurueckkehren, wenn der Mensch gedrueckt hat. Ein Zeitablauf ist
 *    ein Fehler (OSK_ERR_TIMEOUT), keine Zustimmung. */
typedef osk_result_t (*osk_vol_ui_confirm_fn)(osk_vol_mode_t mode, uint32_t recipients, bool secret,
					      void *ctx);

/* --- Baumgroesse und Pfadkosten, gerechnet statt geschaetzt ----------- */

typedef struct {
	uint32_t data_blocks; /* Nutzdatenbloecke zu OSK_VOL_BLOCK_LEN */
	uint32_t leaf_pages;  /* Seiten des Stempelfeldes */
	uint32_t levels;      /* Ebenen ueber dem Stempelfeld (1..OSK_VOL_LEVELS_MAX) */
	uint32_t level_pages[OSK_VOL_LEVELS_MAX]; /* Seiten je Ebene, unten zuerst */
	uint32_t first_leaf_page;		  /* Blocknummer auf der Karte */
	uint32_t first_level_page[OSK_VOL_LEVELS_MAX];
	uint32_t root_page[2];	  /* Wurzelsatz A und B */
	uint32_t first_data_page; /* ab hier die Nutzdaten, Block 0 zuerst */
	uint32_t total_pages;	  /* Karte gesamt: Daten, Baum, Wurzelsaetze */
	uint32_t cached_levels;	  /* oberste Ebenen, die in den RAM passen */
} osk_vol_layout_t;

/* Rechnet den Aufbau fuer eine Karte dieser Groesse. Keine Karte, kein
 * Zustand - reine Rechnung, darum auch ohne geoeffneten Datentraeger nutzbar. */
osk_result_t osk_vol_layout(uint32_t data_blocks, osk_vol_layout_t *out);

/* Was EINE Pfadpruefung an Lesevorgaengen auf der Karte kostet - die Zahl,
 * die der Nutzer als Verzoegerung spuert. Enthaelt den Datenblock selbst. */
uint32_t osk_vol_path_reads(const osk_vol_layout_t *l);

/* Wie viele Byte dafuer gehasht werden. Mal die Hashleistung ist das die
 * Rechenzeit; durch den Nutzdatenblock geteilt ist es der Aufschlag auf den
 * Durchsatz. */
uint32_t osk_vol_path_hash_bytes(const osk_vol_layout_t *l);

/* --- Aufbau und Betrieb ----------------------------------------------- */

typedef struct {
	/* Chipobjekte. Welche gebraucht werden, haengt an der Betriebsart. */
	osk_key_id_t root_key;	 /* Grundgeheimnis, Ableitung im Chip */
	osk_key_id_t sign_key;	 /* Ed25519, signiert die Wurzel (Modus 2,3) */
	osk_key_id_t counter_id; /* monotoner Zaehler (Modus 1) */
	osk_vol_ui_confirm_fn ui_confirm;
	void *ui_ctx;
} osk_volume_config_t;

osk_result_t osk_volume_init(const osk_volume_config_t *cfg);

/* Eine Karte neu beschreiben. Es gibt kein nachtraegliches Verschluesseln:
 * ein Moduswechsel heiszt neu beschreiben, weil alles andere ein halber
 * Zustand waere.
 *
 * Verlangt die Zustimmung zur Betriebsart ueber ui_confirm - auch hier, nicht
 * erst beim ersten Block. */
osk_result_t osk_volume_format(osk_vol_mode_t mode, const osk_vol_recipient_t *recv,
			       uint32_t recv_count, uint32_t data_blocks);

/* Eine steckende Karte oeffnen: Wurzelsatz lesen, Siegel pruefen,
 * Datenschluessel auswickeln, wenn eine Empfaengerzeile passt.
 *
 * `unlock` darf NULL sein; dann wird nur ueber den Chipschluessel versucht.
 * Fuer Modus 2 traegt es das Kartenwort. Modus 3 braucht nichts.
 *
 * Meldet OSK_ERR_DENIED, wenn keine Zeile passt - das ist der falsche Stick
 * oder das falsche Wort, und beides ist KEIN Formatfehler. */
osk_result_t osk_volume_open(const osk_vol_recipient_t *unlock);
void osk_volume_close(void);

bool osk_volume_is_open(void);
osk_vol_mode_t osk_volume_mode(void);

/* "Die Daten sind seit dem Signieren unveraendert." Nicht mehr. Prueft
 * Siegel des Wurzelsatzes, Bindung (Zaehler bzw. Fassung) und dass die
 * oberste Baumseite zum Wurzelhash passt. Millisekunden, kein Kartenlauf. */
osk_result_t osk_volume_root_unchanged(void);

/* "Signiert hat dieser Schluessel - und ob wir ihn kennen, ist eine andere
 * Frage." Getrennt von der Funktion darueber, absichtlich. */
osk_result_t osk_volume_sender(osk_vol_sender_t *out);

/* Einen Block lesen und dabei SEINEN Pfad pruefen. Stimmt er nicht, kommt
 * OSK_ERR_FORMAT und `out` bleibt unberuehrt - es geht kein Byte hinaus,
 * bevor es geprueft ist. Nicht abwaehlbar; der Aufrufer bekommt keinen Weg
 * daran vorbei. */
osk_result_t osk_volume_block_read(uint32_t block, uint8_t out[OSK_VOL_BLOCK_LEN]);

/* Schreibfreigabe je Sitzung. Ruft ui_confirm und nennt dabei Modus und Zahl
 * der Empfaenger. Ohne das: OSK_ERR_DENIED auf jeden Schreibbefehl. */
osk_result_t osk_volume_write_enable(void);
bool osk_volume_write_enabled(void);

osk_result_t osk_volume_block_write(uint32_t block, const uint8_t in[OSK_VOL_BLOCK_LEN]);

/* Festschreiben: Stempelseiten, Baum von unten nach oben, Wurzelsatz in den
 * INAKTIVEN Satz, dann die Bindung. Die Reihenfolge ist der Entwurf; ein
 * Abbruch davor laesst den alten Stand gelten. */
osk_result_t osk_volume_commit(void);

/* Wie viele Bloecke seit der letzten Festschreibung offen sind. Die Anzeige
 * unterscheidet damit "gesichert" von "ungesichert, noch N Bloecke offen". */
uint32_t osk_volume_dirty_blocks(void);

/* Modus 3: beschreiben, dann auf Tastendruck versiegeln. Danach weist der
 * Stick jeden Schreibbefehl ab. Entsiegeln macht die Signatur ungueltig. */
osk_result_t osk_volume_seal(void);
bool osk_volume_sealed(void);

/* --- Die vollstaendige Pruefung: eigener Vorgang, genannte Dauer ------ */

/* Was sie dauern wird, in Millisekunden, bei dieser Hashleistung. Wird dem
 * Nutzer VORHER genannt. `hash_bytes_per_s` ist eine Annahme des Aufrufers -
 * gemessen ist sie nicht. */
uint64_t osk_volume_full_check_ms(const osk_vol_layout_t *l, uint32_t hash_bytes_per_s);

/* Prueft `count` Bloecke ab `from` und meldet den ersten, der nicht stimmt.
 * In Stuecken, damit sie abbrechbar und mit Fortschritt anzeigbar ist. */
osk_result_t osk_volume_full_check_range(uint32_t from, uint32_t count, uint32_t *bad_block_out);

#endif
