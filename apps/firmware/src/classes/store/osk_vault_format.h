/* Aufbau des Tresors im Flash. Diese Datei ist die Wahrheit; die Beschreibung
 * in shared/vault-format.md erklaert sie in Worten.
 *
 * Alle Zahlen liegen als Little Endian im Flash - der nRF52840 rechnet so,
 * und die Attrappe im Test ebenso. Alle Strukturen haben feste Groesse und
 * werden mit OSK_STATIC_ASSERT darauf festgenagelt. */
#ifndef OSK_VAULT_FORMAT_H
#define OSK_VAULT_FORMAT_H

#include <stdint.h>
#include <stddef.h>
#include "osk_crypto.h"

#define OSK_STATIC_ASSERT(cond, name) typedef char osk_sa_##name[(cond) ? 1 : -1]

/* ---- Fassungen -------------------------------------------------------- */

#define OSK_FMT_V1 1 /* erste Fassung */
#define OSK_FMT_V2                                      \
	2 /* Eintrag hat das Kennzeichen                \
	   * "schreibgeschuetzt" und einen Zaehler fuer \
	   * HOTP */
#define OSK_FMT_V3                                  \
	3 /* jetzige Fassung: Eintrag hat die nicht \
	   * geheime Kennung zum Suchen (tag), die  \
	   * FIDO2 braucht */
#define OSK_FMT_CURRENT OSK_FMT_V3
#define OSK_FMT_OLDEST_KNOWN OSK_FMT_V1

/* ---- Grenzen ---------------------------------------------------------- */

#define OSK_PROFILE_MAX 4 /* Profile je Geraet, jedes mit eigener PIN */
#define OSK_ITEMS_MAX 64  /* Eintraege je Profil, also 256 im Geraet */
#define OSK_NAME_MAX 64	  /* mit Null-Abschluss */
#define OSK_SITE_MAX 128
#define OSK_SECRET_MAX 4096 /* laengstes Geheimnis, auch kleine Dateien */
#define OSK_TAG_MAX 32	    /* Kennung zum Suchen, siehe osk_store.h */

/* ---- Einteilung der Tresorpartition ---------------------------------- */
/* Sektoren von 4 KiB, Seiten von 256 Byte - die Masse des QSPI-Flash. Beim
 * Oeffnen wird gegen die wirkliche Geometrie geprueft. */

#define OSK_SECTOR 4096u
#define OSK_PAGE 256u

#define OSK_BLOB_SLOT (2u * OSK_SECTOR) /* ein Platz je Geheimnis */
#define OSK_BLOB_SLOTS 1024u		/* Plaetze im Datenbereich */

#define OSK_AREA_HDR_OFF 0u
#define OSK_AREA_HDR_LEN (2u * OSK_SECTOR) /* zwei Sektoren, Wechselbetrieb */
#define OSK_AREA_IDX_OFF (OSK_AREA_HDR_OFF + OSK_AREA_HDR_LEN)
#define OSK_AREA_IDX_LEN (256u * OSK_SECTOR) /* 1 MiB Index-Ring */
#define OSK_AREA_LOG_OFF (OSK_AREA_IDX_OFF + OSK_AREA_IDX_LEN)
#define OSK_AREA_LOG_LEN (32u * OSK_SECTOR) /* 128 KiB Protokoll-Ring */
#define OSK_AREA_BLOB_OFF (OSK_AREA_LOG_OFF + OSK_AREA_LOG_LEN)
#define OSK_AREA_BLOB_LEN (OSK_BLOB_SLOTS * OSK_BLOB_SLOT) /* 8 MiB */
#define OSK_VAULT_SIZE (OSK_AREA_BLOB_OFF + OSK_AREA_BLOB_LEN)

/* ---- Kopf ------------------------------------------------------------- */
/* Der Kopf steht im Klartext, denn ohne ihn laesst sich nichts oeffnen. Er
 * enthaelt nichts Geheimes: Fassung, Zaehlerstand, Adressen und die Belegung
 * der Datenplaetze.
 *
 * Geschrieben wird er als Folge gleich grosser Saetze in zwei Sektoren. Jeder
 * neue Satz landet im naechsten freien Platz; gueltig ist der Satz mit der
 * hoechsten Folgenummer und richtiger Pruefsumme. Ist ein Sektor voll, wird
 * der andere geloescht und dort weitergeschrieben. Das verteilt den Verschleiss
 * und macht jeden Wechsel atomar: ein abgebrochener Satz hat eine falsche
 * Pruefsumme und wird uebergangen. Loeschen setzt Bits nur auf 1, und
 * 0xFFFFFFFF ist keine gueltige Kennung - ein halb geloeschter Sektor kann
 * also nie fuer einen gueltigen Satz gehalten werden. */

#define OSK_HDR_MAGIC 0x564B534Fu /* "OSKV" */
#define OSK_HDR_SLOT OSK_PAGE	  /* ein Kopfsatz je Flash-Seite */
#define OSK_HDR_SLOTS (OSK_AREA_HDR_LEN / OSK_HDR_SLOT)

typedef struct {
	uint32_t magic;
	uint16_t format;	 /* OSK_FMT_* */
	uint16_t hdr_size;	 /* sizeof(osk_hdr_t), fuer spaetere Fassungen */
	uint32_t seq;		 /* waechst mit jedem Satz, nie zurueck */
	uint32_t sector_of_slot; /* in welchem Kopfsektor dieser Satz steht */

	/* Naechste unbenutzte Nonce. Wird VOR dem Schreiben festgehalten, und
	 * zwar in Schritten von OSK_NONCE_RESERVE, damit nicht jedes Geheimnis
	 * einen eigenen Kopfsatz kostet. Beim Anmelden beginnt der Tresor bei
	 * diesem Wert - die zwischendurch herausgegebenen Nonces gelten als
	 * verbraucht. Ein Stromausfall verschenkt dadurch bis zu
	 * OSK_NONCE_RESERVE Nonces und wiederholt keine einzige. */
	uint64_t nonce_next;

	uint32_t index_root[OSK_PROFILE_MAX];  /* Adresse der Indexfassung, 0 = leer */
	uint32_t index_len[OSK_PROFILE_MAX];   /* Laenge dort, mit Rahmen */
	uint32_t index_alloc;		       /* Suchzeiger im Index-Ring */
	uint32_t blob_cursor;		       /* Suchzeiger im Datenbereich, laeuft im Kreis */
	uint32_t item_next;		       /* naechste freie Eintragskennung, nie wiederholt */
	uint8_t profile_used[OSK_PROFILE_MAX]; /* 1 = Profil eingerichtet */

	/* Zum Protokoll steht hier nichts: sein Ringkopf wird beim Oeffnen im
	 * Ring selbst gesucht. Stuende er im Kopf, kostete jeder
	 * Protokolleintrag einen Kopfsatz - mitten in einer Transaktion. */
	uint8_t reserved[12];

	/* Belegung der Datenplaetze, ein Bit je Platz. Sie gehoert in den Kopf,
	 * damit ein Platz auch dann als belegt gilt, wenn das Profil gesperrt
	 * ist und sein Index nicht gelesen werden kann. Verraten wird dadurch
	 * nur, wie viele Geheimnisse im Geraet liegen. */
	uint8_t slot_used[OSK_BLOB_SLOTS / 8];

	uint32_t crc32; /* ueber alle Felder davor */
} osk_hdr_t;

OSK_STATIC_ASSERT(sizeof(osk_hdr_t) <= OSK_HDR_SLOT, hdr_fits_slot);

#define OSK_NONCE_RESERVE 64u

/* Wozu eine Nonce gehoert. Steht in den obersten vier Byte der Nonce, damit
 * derselbe Zaehlerstand in verschiedenen Rollen nie dieselbe Nonce ergibt. */
typedef enum {
	OSK_NONCE_BLOB = 1,
	OSK_NONCE_INDEX = 2,
	OSK_NONCE_LOG = 3,
	OSK_NONCE_BACKUP = 4,
	OSK_NONCE_TRANSFER = 5,
} osk_nonce_purpose_t;

/* ---- Index ------------------------------------------------------------ */
/* Je Profil eine Indexfassung. Eine Fassung wird vollstaendig an einen neuen
 * Platz geschrieben; erst der neue Kopfsatz macht sie gueltig. Dadurch wandert
 * der Index bei jeder Aenderung weiter und nutzt den Ring gleichmaessig ab. */

#define OSK_IDX_MAGIC 0x58444B4Fu /* "OKDX" */

/* Kennzeichen eines Eintrags. */
#define OSK_FLAG_MIRRORED 0x01u	   /* darf auf den Zweitstick */
#define OSK_FLAG_PAIRED_ONLY 0x02u /* nur an gekoppelte Rechner */
#define OSK_FLAG_READONLY 0x04u	   /* nicht aenderbar, nicht loeschbar */

/* Ein Eintrag im Index. Nur Beschreibung, nie das Geheimnis. Feste Groesse,
 * damit eine Fassung ohne Rechnerei gelesen werden kann. */
typedef struct {
	uint32_t id;
	uint16_t type; /* osk_item_type_t */
	uint8_t profile;
	uint8_t flags; /* OSK_FLAG_* */
	uint64_t changed_utc;
	uint32_t slot;	      /* Datenplatz des Geheimnisses */
	uint32_t blob_ct_len; /* Laenge des Kryptotextes dort */
	uint32_t secret_len;  /* Klartextlaenge - steht nur im verschluesselten
			       * Index, verraet also nach aussen nichts */
	uint32_t pad0;
	uint64_t counter; /* HOTP-Zaehler, sonst 0 (ab Fassung 2) */
	uint8_t nonce[OSK_NONCE_LEN];

	/* Kennung zum Suchen (ab Fassung 3). Sie steht hier, also IM
	 * verschluesselten Index, und nicht im Klartext-Kopf. Eine
	 * Klartextliste waere schneller zu durchsuchen, wuerde aber jedem, der
	 * den Flash ausliest, verraten, bei welchen Diensten der Nutzer ein
	 * Konto hat. Gesucht wird darum im entschluesselten Index - die
	 * Geheimnisse selbst bleiben dabei unberuehrt. */
	uint8_t tag[OSK_TAG_MAX];
	uint8_t tag_len; /* 0 = keine Kennung */
	uint8_t pad1[3];

	char name[OSK_NAME_MAX];
	char site[OSK_SITE_MAX];
	uint8_t reserved[40];
} osk_idx_rec_t;

OSK_STATIC_ASSERT(sizeof(osk_idx_rec_t) == 320, idx_rec_320);

/* Kopf einer Indexfassung, verschluesselt wie die Saetze dahinter. */
typedef struct {
	uint32_t magic;
	uint16_t format;
	uint16_t count;
	uint32_t generation;
	uint8_t profile;
	uint8_t reserved[19];
} osk_idx_head_t;

OSK_STATIC_ASSERT(sizeof(osk_idx_head_t) == 32, idx_head_32);

/* Eine ganze Indexfassung im Klartext. Dieser Puffer ist der Arbeitsplatz des
 * Tresors: geladen ist immer nur ein Profil. */
typedef struct {
	osk_idx_head_t head;
	osk_idx_rec_t recs[OSK_ITEMS_MAX];
} osk_idx_page_t;

/* Was im Klartext vor der verschluesselten Indexfassung steht. */
typedef struct {
	uint32_t magic;	 /* OSK_IDX_MAGIC */
	uint32_t ct_len; /* Kryptotext samt Pruefwert */
	uint8_t nonce[OSK_NONCE_LEN];
	uint32_t crc32; /* ueber diesen Rahmen, nicht ueber die Daten */
} osk_idx_frame_t;

OSK_STATIC_ASSERT(sizeof(osk_idx_frame_t) == 24, idx_frame_24);

/* ---- Geheimnisse ------------------------------------------------------ */
/* Jedes Geheimnis liegt auf einem eigenen Platz von zwei Sektoren, mit eigener
 * Nonce. Der Rahmen davor steht im Klartext, damit ein Platz ohne Schluessel
 * geprueft werden kann; die Eintragskennung darin verraet nichts ueber den
 * Inhalt. Ein Platz wird immer ganz geloescht und ganz neu geschrieben. */

#define OSK_BLOB_MAGIC 0x424B534Fu /* "OSKB" */

typedef struct {
	uint32_t magic;
	uint32_t id; /* zu welchem Eintrag das Geheimnis gehoert */
	uint32_t ct_len;
	uint32_t generation; /* Kopf-Folgenummer beim Schreiben */
	uint32_t crc32;	     /* ueber diesen Rahmen */
} osk_blob_frame_t;

OSK_STATIC_ASSERT(sizeof(osk_blob_frame_t) == 20, blob_frame_20);

/* ---- Protokoll -------------------------------------------------------- */
/* Ringpuffer mit festen Plaetzen von einer Seite. Die Folgenummer steht im
 * Klartext, damit der Kopf des Rings ohne Schluessel gefunden wird, und ist
 * zugleich zusaetzliche Daten der Verschluesselung - umschreiben faellt auf. */

#define OSK_LOG_MAGIC 0x4C4B534Fu /* "OSKL" */
#define OSK_LOG_SLOT OSK_PAGE
#define OSK_LOG_SLOTS (OSK_AREA_LOG_LEN / OSK_LOG_SLOT)

typedef struct {
	uint64_t when_utc;
	uint32_t link;
	uint16_t what;
	uint16_t pad;
	uint32_t item_id;
	char asked_by[32];
	uint8_t reserved[12];
} osk_log_plain_t;

OSK_STATIC_ASSERT(sizeof(osk_log_plain_t) == 64, log_plain_64);

/* Die Pruefsumme steht hinten und deckt ALLES davor, auch den Kryptotext.
 * Nur so faellt ein Platz auf, der beim Stromausfall mitten entzwei gerissen
 * ist: sein Rahmen waere sonst heil und der Inhalt halb. */
typedef struct {
	uint32_t magic;
	uint32_t seq;
	uint8_t nonce[OSK_NONCE_LEN];
	uint8_t ct[sizeof(osk_log_plain_t) + OSK_TAG_LEN];
	uint32_t crc32;
} osk_log_slot_t;

OSK_STATIC_ASSERT(sizeof(osk_log_slot_t) <= OSK_LOG_SLOT, log_slot_fits);

/* ---- Anlaesse im Protokoll (shared/protocol.md) ----------------------- */

#define OSK_LOG_OPEN 1
#define OSK_LOG_LOCK 2
#define OSK_LOG_REVEAL 3
#define OSK_LOG_REVEAL_DENY 4
#define OSK_LOG_ADD 5
#define OSK_LOG_UPDATE 6
#define OSK_LOG_DELETE 7
#define OSK_LOG_MIGRATE 8
#define OSK_LOG_BACKUP 9
#define OSK_LOG_RESTORE 10
#define OSK_LOG_TRANSFER 11
#define OSK_LOG_UNLOCK 12
#define OSK_LOG_DENY 13

uint32_t osk_crc32(const void *buf, size_t len);

/* Baut die Nonce aus Zaehlerstand und Verwendung. */
void osk_nonce_build(uint64_t counter, osk_nonce_purpose_t purpose, uint8_t out[OSK_NONCE_LEN]);

/* Rundet auf ganze Flash-Seiten auf. */
static inline uint32_t osk_round_page(uint32_t len)
{
	return (len + OSK_PAGE - 1u) & ~(OSK_PAGE - 1u);
}

#endif
