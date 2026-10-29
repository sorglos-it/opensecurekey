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
#define OSK_FMT_V3                                 \
	3 /* Eintrag hat die nicht geheime Kennung \
	   * zum Suchen (tag), die FIDO2 braucht.  \
	   * Je Profil lag EINE Indexfassung ganz  \
	   * im RAM - daher nur 64 Eintraege. */
#define OSK_FMT_V4                                    \
	4 /* jetzige Fassung: der Index liegt in      \
	   * Bloecken zu 64 Saetzen, von denen immer  \
	   * nur einer im RAM ist. Dadurch 768        \
	   * Eintraege je Profil. Der Satz selbst ist \
	   * unveraendert 320 Byte lang; geaendert    \
	   * haben sich der Klartext-Kopf und die     \
	   * Einteilung der Partition. */
#define OSK_FMT_V5                                    \
	5 /* jetzige Fassung: der Eintrag kann einen  \
	   * OEFFENTLICHEN Teil haben - auf eigenem   \
	   * Datenplatz oder im SE050. Der Satz ist   \
	   * weiter 320 Byte lang; benutzt werden nur \
	   * Bytes, die in Fassung 4 als reserved auf \
	   * 0 standen. Ein Block der Fassung 4 liest \
	   * sich darum unveraendert als "kein        \
	   * oeffentlicher Teil" - umzurechnen ist    \
	   * nichts. */
#define OSK_FMT_V6 6
/* Fassung 6: der Kopf hat ein Pruefmal je Profil fuer die Wisch-PIN, und der
 * Protokollplatz traegt das Kettenglied zum vorhergehenden Satz. Der
 * Indexsatz ist weiter 320 Byte lang - der Signaturzaehler liegt in Bytes,
 * die in Fassung 5 als reserved auf 0 standen, und liest sich dort also als
 * "noch nie signiert". Der KOPF dagegen waechst, darum wird der Kopf der
 * Fassungen 4 und 5 beim Anmelden im RAM umgerechnet - genau wie der der
 * Fassung 3. */
#define OSK_FMT_CURRENT OSK_FMT_V6
#define OSK_FMT_OLDEST_KNOWN OSK_FMT_V1

/* ---- Grenzen ---------------------------------------------------------- */

#define OSK_PROFILE_MAX 4 /* Profile je Geraet, jedes mit eigener PIN */

/* Der Index eines Profils liegt in Bloecken. Im RAM ist immer nur EIN Block -
 * das ist der ganze Grund dieser Einteilung. Ein Block hat feste Groesse,
 * damit die Platzvergabe im Index-Ring ohne Laengenliste im Kopf auskommt. */
#define OSK_IDX_BLOCK_RECS 64u /* Saetze je Block: 64 * 320 B = 20 KiB RAM */
#define OSK_IDX_BLOCKS_MAX 12u /* Bloecke je Profil */

#define OSK_ITEMS_MAX (OSK_IDX_BLOCK_RECS * OSK_IDX_BLOCKS_MAX) /* 768 je Profil */
#define OSK_NAME_MAX 64						/* mit Null-Abschluss */
#define OSK_SITE_MAX 128
#define OSK_SECRET_MAX 4096 /* laengstes Geheimnis, auch kleine Dateien */
#define OSK_PUBLIC_MAX 4096 /* laengster oeffentlicher Teil (Zertifikat) */
#define OSK_TAG_MAX 32	    /* Kennung zum Suchen, siehe osk_store.h */

/* ---- Einteilung der Tresorpartition ---------------------------------- */
/* Sektoren von 4 KiB, Seiten von 256 Byte - die Masse des QSPI-Flash. Beim
 * Oeffnen wird gegen die wirkliche Geometrie geprueft.
 *
 * Die Partition hat ZWEI Bereiche, und das ist eine Entscheidung, nicht ein
 * Versehen:
 *
 *   Bereich 0  genau wie in den Fassungen 1 bis 3 - Adresse fuer Adresse.
 *   Bereich 1  in Fassung 4 dahinter angehaengt.
 *
 * Dadurch behaelt jedes Geheimnis eines alten Tresors seine Adresse, und die
 * Wanderung von Fassung 3 muss kein einziges Byte umkopieren. Haette Fassung 4
 * den Index-Ring an seiner Stelle vergroessert, waere der Datenbereich nach
 * hinten gerutscht - dann muessten beim Umstieg alle Geheimnisse umziehen,
 * waehrend genau diese Daten noch gebraucht werden. Ein angehaengter Bereich
 * ist weniger schoen zu lesen und sehr viel sicherer. */

#define OSK_SECTOR 4096u
#define OSK_PAGE 256u

#define OSK_BLOB_SLOT (2u * OSK_SECTOR) /* ein Platz je Geheimnis */

/* --- Bereich 0: unveraendert seit Fassung 1 --- */

#define OSK_AREA_HDR_OFF 0u
#define OSK_AREA_HDR_LEN (2u * OSK_SECTOR) /* Kopf der Fassungen 1 bis 3 */
#define OSK_AREA_IDX_OFF (OSK_AREA_HDR_OFF + OSK_AREA_HDR_LEN)
#define OSK_AREA_IDX_LEN (256u * OSK_SECTOR) /* 1 MiB Index-Ring, Abschnitt 0 */
#define OSK_AREA_LOG_OFF (OSK_AREA_IDX_OFF + OSK_AREA_IDX_LEN)
#define OSK_AREA_LOG_LEN (32u * OSK_SECTOR) /* 128 KiB Protokoll-Ring */
#define OSK_AREA_BLOB_OFF (OSK_AREA_LOG_OFF + OSK_AREA_LOG_LEN)
#define OSK_BLOB_SLOTS_SEG0 1024u				/* Datenplaetze 0 bis 1023 */
#define OSK_AREA_BLOB_LEN (OSK_BLOB_SLOTS_SEG0 * OSK_BLOB_SLOT) /* 8 MiB */
#define OSK_AREA_V3_END (OSK_AREA_BLOB_OFF + OSK_AREA_BLOB_LEN)

/* --- Bereich 1: neu in Fassung 4 --- */

/* Der Kopf der Fassung 4 ist groesser als eine Flash-Seite (die Belegung der
 * Datenplaetze allein braucht 384 Byte). Er kann darum nicht in den zwei alten
 * Kopfsektoren liegen: dort passten nur vier Saetze je Sektor, und jeder
 * vierte Wechsel haette einen Loeschvorgang gekostet. Er bekommt deshalb
 * eigene 16 Sektoren im neuen Bereich - 64 Saetze, und der Verschleiss
 * verteilt sich besser als vorher. Die zwei alten Sektoren bleiben liegen und
 * werden nach der Wanderung nicht mehr beschrieben. */
#define OSK_AREA_HDR2_OFF OSK_AREA_V3_END
#define OSK_AREA_HDR2_LEN (16u * OSK_SECTOR) /* 64 KiB Kopf-Ring */

#define OSK_AREA_IDX2_OFF (OSK_AREA_HDR2_OFF + OSK_AREA_HDR2_LEN)
#define OSK_AREA_IDX2_LEN (768u * OSK_SECTOR) /* 3 MiB Index-Ring, Abschnitt 1 */

#define OSK_AREA_BLOB2_OFF (OSK_AREA_IDX2_OFF + OSK_AREA_IDX2_LEN)
#define OSK_BLOB_SLOTS 3072u /* Datenplaetze insgesamt, einer je Eintrag */
#define OSK_AREA_BLOB2_LEN ((OSK_BLOB_SLOTS - OSK_BLOB_SLOTS_SEG0) * OSK_BLOB_SLOT) /* 16 MiB */

#define OSK_VAULT_SIZE (OSK_AREA_BLOB2_OFF + OSK_AREA_BLOB2_LEN) /* 28,2 MiB */

/* Ein Datenplatz je Eintrag: 4 Profile mal 768 Eintraege gehen sich aus. */
OSK_STATIC_ASSERT(OSK_BLOB_SLOTS >= OSK_PROFILE_MAX * OSK_ITEMS_MAX, slots_for_items);

/* Adresse eines Datenplatzes. Die Plaetze 0 bis 1023 liegen da, wo sie in
 * Fassung 3 lagen - darum die zwei Zweige. */
static inline uint32_t osk_blob_addr(uint32_t slot)
{
	if (slot < OSK_BLOB_SLOTS_SEG0) {
		return OSK_AREA_BLOB_OFF + slot * OSK_BLOB_SLOT;
	}
	return OSK_AREA_BLOB2_OFF + (slot - OSK_BLOB_SLOTS_SEG0) * OSK_BLOB_SLOT;
}

/* ---- Rueckdrehschutz -------------------------------------------------- */
/* Der Stand des Tresors ist die Folgenummer des Kopfsatzes (seq). Sie steigt
 * bei jedem Festschreiben um eins und ist damit schon da - eine zweite Zahl
 * danebenzustellen hiesse, zwei Groessen zu fuehren, die dasselbe messen
 * sollen und irgendwann auseinanderlaufen.
 *
 * Ihr Anker liegt NICHT im Flash, sondern als monotoner Zaehler im SE050. Ein
 * Wert im Flash laesst sich mit dem Flash zurueckspielen, der im Chip nicht.
 *
 * Die REIHENFOLGE ist der ganze Trick: erst der Kopfsatz, dann der Chip.
 * Faellt der Strom dazwischen aus, steht im Flash seq und im Chip seq-1 -
 * der Tresor oeffnet und holt den Chip beim naechsten Oeffnen nach. Andersherum
 * stuende im Chip ein Wert, den der Flash nie erreicht, und das Geraet waere
 * durch einen Stromausfall unbrauchbar. */
#define OSK_KEYID_VAULT_GEN 0x0000F101u

/* Laenge des Wisch-PIN-Pruefmals. */
#define OSK_WIPE_TAG_LEN 16u

/* ---- Kopf ------------------------------------------------------------- */
/* Der Kopf steht im Klartext, denn ohne ihn laesst sich nichts oeffnen. Er
 * enthaelt nichts Geheimes: Fassung, Zaehlerstand, Adressen und die Belegung
 * der Datenplaetze.
 *
 * Geschrieben wird er als Folge gleich grosser Saetze in einem Ring von
 * Sektoren. Jeder neue Satz landet im naechsten freien Platz; gueltig ist der
 * Satz mit der hoechsten Folgenummer und richtiger Pruefsumme. Ist ein Sektor
 * voll, wird der naechste geloescht und dort weitergeschrieben. Das verteilt
 * den Verschleiss und macht jeden Wechsel atomar: ein abgebrochener Satz hat
 * eine falsche Pruefsumme und wird uebergangen. Loeschen setzt Bits nur auf 1,
 * und 0xFFFFFFFF ist keine gueltige Kennung - ein halb geloeschter Sektor kann
 * also nie fuer einen gueltigen Satz gehalten werden.
 *
 * Welche Fassung ein Satz hat, sagt hdr_size. Darum laesst sich ein Kopf der
 * Fassungen 1 bis 3 lesen, ohne vorher zu wissen, dass er alt ist. */

#define OSK_HDR_MAGIC 0x564B534Fu /* "OSKV" */

/* Der Kopf der Fassungen 1 bis 3: ein Satz je Flash-Seite, zwei Sektoren.
 * Wird nur noch GELESEN - beim Umstieg auf Fassung 4. */
#define OSK_HDR_V3_SLOT OSK_PAGE
#define OSK_HDR_V3_SLOTS (OSK_AREA_HDR_LEN / OSK_HDR_V3_SLOT)

typedef struct {
	uint32_t magic;
	uint16_t format;
	uint16_t hdr_size;
	uint32_t seq;
	uint32_t sector_of_slot;
	uint64_t nonce_next;
	uint32_t index_root[OSK_PROFILE_MAX]; /* die EINE Indexfassung je Profil */
	uint32_t index_len[OSK_PROFILE_MAX];
	uint32_t index_alloc;
	uint32_t blob_cursor;
	uint32_t item_next;
	uint8_t profile_used[OSK_PROFILE_MAX];
	uint8_t reserved[12];
	uint8_t slot_used[OSK_BLOB_SLOTS_SEG0 / 8];
	uint32_t crc32;
} osk_hdr_v3_t;

OSK_STATIC_ASSERT(sizeof(osk_hdr_v3_t) <= OSK_HDR_V3_SLOT, hdr_v3_fits_slot);

/* Der Kopf der Fassung 4. Ein Satz braucht vier Flash-Seiten, weil die
 * Belegung der 3072 Datenplaetze allein 384 Byte kostet. */
#define OSK_HDR_SLOT (4u * OSK_PAGE)
#define OSK_HDR_SLOTS (OSK_AREA_HDR2_LEN / OSK_HDR_SLOT)
#define OSK_HDR_SLOTS_PER_SECTOR (OSK_SECTOR / OSK_HDR_SLOT)

/* Der Kopf der Fassungen 4 und 5. Beide hatten denselben Aufbau; nur die
 * Fassungsnummer unterschied sie. Wird nur noch GELESEN - beim Umstieg auf
 * Fassung 6. */
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

	/* Wo die Indexbloecke jedes Profils liegen. Die Adressen stehen im
	 * Klartext, und das muss so sein: der Tresor muss Platz im Index-Ring
	 * vergeben koennen, ohne die Bloecke eines GESPERRTEN Profils zu
	 * ueberschreiben - deren Adressen darf er also auch dann kennen.
	 * Verraten wird dadurch, wie viele Eintraege ein Profil hat; die Groesse
	 * einer Indexfassung hat das schon in Fassung 3 ungefaehr gezeigt. */
	uint32_t index_block[OSK_PROFILE_MAX][OSK_IDX_BLOCKS_MAX]; /* 0 = unbenutzt */
	uint16_t index_count[OSK_PROFILE_MAX];			   /* Eintraege im Profil */
	uint8_t index_blocks[OSK_PROFILE_MAX];			   /* belegte Bloecke */

	uint32_t index_alloc;		       /* Suchzeiger im Index-Ring */
	uint32_t blob_cursor;		       /* Suchzeiger im Datenbereich, laeuft im Kreis */
	uint32_t item_next;		       /* naechste freie Eintragskennung, nie wiederholt */
	uint8_t profile_used[OSK_PROFILE_MAX]; /* 1 = Profil eingerichtet */

	/* Zum Protokoll steht hier nichts: sein Ringkopf wird beim Oeffnen im
	 * Ring selbst gesucht. Stuende er im Kopf, kostete jeder
	 * Protokolleintrag einen Kopfsatz - mitten in einer Transaktion.
	 *
	 * Die Groesse ist mit Bedacht gewaehlt: sie schiebt crc32 genau an das
	 * Ende der Struktur. Die Pruefsumme wird ueber alles davor gerechnet,
	 * und das Ende wird als sizeof minus vier Byte bestimmt - gaebe es
	 * hinter crc32 eine Auffuellung des Uebersetzers, deckte die Pruefsumme
	 * sich selbst mit und wuerde nie stimmen. Der Riegel darunter haelt
	 * das fest. */
	uint8_t reserved[16];

	/* Belegung der Datenplaetze, ein Bit je Platz. Sie gehoert in den Kopf,
	 * damit ein Platz auch dann als belegt gilt, wenn das Profil gesperrt
	 * ist und sein Index nicht gelesen werden kann. Verraten wird dadurch
	 * nur, wie viele Geheimnisse im Geraet liegen. */
	uint8_t slot_used[OSK_BLOB_SLOTS / 8];

	uint32_t crc32; /* ueber alle Felder davor */
} osk_hdr_v5_t;

OSK_STATIC_ASSERT(offsetof(osk_hdr_v5_t, crc32) == sizeof(osk_hdr_v5_t) - sizeof(uint32_t),
		  hdr_v5_crc_is_last);

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

	/* Wo die Indexbloecke jedes Profils liegen. Die Adressen stehen im
	 * Klartext, und das muss so sein: der Tresor muss Platz im Index-Ring
	 * vergeben koennen, ohne die Bloecke eines GESPERRTEN Profils zu
	 * ueberschreiben - deren Adressen darf er also auch dann kennen.
	 * Verraten wird dadurch, wie viele Eintraege ein Profil hat; die Groesse
	 * einer Indexfassung hat das schon in Fassung 3 ungefaehr gezeigt. */
	uint32_t index_block[OSK_PROFILE_MAX][OSK_IDX_BLOCKS_MAX]; /* 0 = unbenutzt */
	uint16_t index_count[OSK_PROFILE_MAX];			   /* Eintraege im Profil */
	uint8_t index_blocks[OSK_PROFILE_MAX];			   /* belegte Bloecke */

	uint32_t index_alloc;		       /* Suchzeiger im Index-Ring */
	uint32_t blob_cursor;		       /* Suchzeiger im Datenbereich, laeuft im Kreis */
	uint32_t item_next;		       /* naechste freie Eintragskennung, nie wiederholt */
	uint8_t profile_used[OSK_PROFILE_MAX]; /* 1 = Profil eingerichtet */

	/* Pruefmal der Wisch-PIN je Profil (ab Fassung 6). Alles 0 = keine
	 * gesetzt. Es ist KEIN Abdruck der PIN allein: gerechnet wird ueber
	 * den geraetegebundenen Wert aus dem SE050, darum nuetzt die Zahl auf
	 * einem anderen Rechner nichts. Sie steht im Klartext-Kopf, weil sie
	 * geprueft werden muss, waehrend das Profil noch zu ist - genau dann
	 * wird die PIN ja eingegeben. */
	uint8_t wipe_tag[OSK_PROFILE_MAX][OSK_WIPE_TAG_LEN];

	/* Zum Protokoll steht hier nichts: sein Ringkopf wird beim Oeffnen im
	 * Ring selbst gesucht. Stuende er im Kopf, kostete jeder
	 * Protokolleintrag einen Kopfsatz - mitten in einer Transaktion.
	 *
	 * Die Groesse ist mit Bedacht gewaehlt: sie schiebt crc32 genau an das
	 * Ende der Struktur. Die Pruefsumme wird ueber alles davor gerechnet,
	 * und das Ende wird als sizeof minus vier Byte bestimmt - gaebe es
	 * hinter crc32 eine Auffuellung des Uebersetzers, deckte die Pruefsumme
	 * sich selbst mit und wuerde nie stimmen. Der Riegel darunter haelt
	 * das fest. */
	uint8_t reserved[16];

	/* Belegung der Datenplaetze, ein Bit je Platz. Sie gehoert in den Kopf,
	 * damit ein Platz auch dann als belegt gilt, wenn das Profil gesperrt
	 * ist und sein Index nicht gelesen werden kann. Verraten wird dadurch
	 * nur, wie viele Geheimnisse im Geraet liegen. */
	uint8_t slot_used[OSK_BLOB_SLOTS / 8];

	uint32_t crc32; /* ueber alle Felder davor */
} osk_hdr_t;

OSK_STATIC_ASSERT(sizeof(osk_hdr_t) <= OSK_HDR_SLOT, hdr_fits_slot);
OSK_STATIC_ASSERT(sizeof(osk_hdr_t) != sizeof(osk_hdr_v3_t), hdr_sizes_differ);

/* Die Pruefsumme steht am Ende, ohne Auffuellung dahinter - sonst deckte sie
 * sich selbst mit. Gilt fuer beide Fassungen des Kopfes. */
OSK_STATIC_ASSERT(offsetof(osk_hdr_t, crc32) == sizeof(osk_hdr_t) - sizeof(uint32_t),
		  hdr_crc_is_last);
OSK_STATIC_ASSERT(offsetof(osk_hdr_v3_t, crc32) == sizeof(osk_hdr_v3_t) - sizeof(uint32_t),
		  hdr_v3_crc_is_last);

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
/* Je Profil mehrere Indexbloecke zu je 64 Saetzen. Ein Block wird vollstaendig
 * an einen neuen Platz geschrieben; erst der neue Kopfsatz macht ihn gueltig.
 * Dadurch wandert der Index bei jeder Aenderung weiter und nutzt den Ring
 * gleichmaessig ab.
 *
 * Eine Aenderung schreibt nur den EINEN betroffenen Block neu, nicht den
 * ganzen Index. Die Bloecke sind bis auf den letzten immer voll - so sagt die
 * Nummer eines Eintrags ohne Nachrechnen, in welchem Block er steht:
 * Block = Nummer / 64, Platz darin = Nummer % 64. */

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

	/* Der oeffentliche Teil (ab Fassung 5). Er liegt auf einem EIGENEN
	 * Datenplatz, nicht beim Geheimnis - nur darum kann
	 * osk_store_public() ihn herausgeben, ohne den Platz des Geheimnisses
	 * auch nur zu lesen. Verschluesselt ist er trotzdem: nicht geheim
	 * heisst nicht, dass jeder, der den Flash ausliest, die Liste der
	 * Schluessel des Nutzers bekommen soll.
	 *
	 * Liegt der private Schluessel im SE050, steht hier stattdessen seine
	 * Kennung, und der oeffentliche Teil kommt aus dem Chip. */
	/* ACHTUNG: Ob ein oeffentlicher Teil da ist, entscheidet pub_len und
	 * NICHT pub_slot. Platz 0 ist ein gueltiger Datenplatz - eine 0 in
	 * pub_slot als "keiner" zu lesen wuerde genau einen Eintrag im Geraet
	 * falsch behandeln, und zwar den, der zuerst angelegt wurde. */
	uint32_t pub_slot;
	uint32_t pub_ct_len; /* Laenge des Kryptotextes dort */
	uint16_t pub_len;    /* 0 = keiner; sonst hoechstens OSK_PUBLIC_MAX */
	uint8_t pad2[2];
	uint32_t se050_key; /* 0 = kein Schluessel im Chip */
	uint8_t pub_nonce[OSK_NONCE_LEN];

	/* Signaturzaehler von FIDO2 (ab Fassung 6). Gezaehlt wird in CTAP
	 * (Strang B), gespeichert hier - siehe osk_store_sigcount_next().
	 *
	 * ACHTUNG, das ist NICHT `counter` weiter oben: der zaehlt HOTP und
	 * wird dem Nutzer angezeigt, dieser hier geht an den Dienst und
	 * beweist, dass der Schluessel nicht geklont ist. Zwei Zaehler, zwei
	 * Zwecke - sie heissen darum verschieden.
	 *
	 * In Fassung 5 lagen hier Nullen (Teil von reserved). Ein alter Satz
	 * liest sich dadurch als "noch nie signiert", und das ist richtig. */
	uint32_t sigcount;

	uint8_t reserved[8];
} osk_idx_rec_t;

OSK_STATIC_ASSERT(sizeof(osk_idx_rec_t) == 320, idx_rec_320);

/* Kopf eines Indexblocks, verschluesselt wie die Saetze dahinter.
 *
 * Die Blocknummer steht IM verschluesselten Teil und nicht in den zusaetzlichen
 * Daten. Das ist Absicht: die zusaetzlichen Daten sind in allen Fassungen die
 * Profilnummer allein, und nur darum laesst sich eine alte Fassung ueberhaupt
 * oeffnen, ohne vorher zu wissen, welche es ist. Geprueft wird die Nummer nach
 * dem Entschluesseln. Das schuetzt genauso gut: der Pruefwert deckt den Kopf
 * mit, ein Block mit anderer Nummer laesst sich also nicht faelschen, und ein
 * echter Block an die Stelle eines anderen geschoben faellt an der Nummer auf.
 *
 * In den Fassungen 1 bis 3 lag hier eine 0 (Teil von reserved). Eine alte
 * Indexfassung liest sich dadurch als Block 0 - genau richtig. */
typedef struct {
	uint32_t magic;
	uint16_t format;
	uint16_t count; /* Saetze in DIESEM Block */
	uint32_t generation;
	uint8_t profile;
	uint8_t pad0;
	uint16_t block; /* Nummer des Blocks im Profil (ab Fassung 4) */
	uint8_t reserved[16];
} osk_idx_head_t;

OSK_STATIC_ASSERT(sizeof(osk_idx_head_t) == 32, idx_head_32);

/* Ein Indexblock im Klartext. Dieser Puffer ist der Arbeitsplatz des Tresors:
 * geladen ist immer nur EIN Block EINES Profils. Daran haengt die
 * Eintragsgrenze, nicht am Flash. */
typedef struct {
	osk_idx_head_t head;
	osk_idx_rec_t recs[OSK_IDX_BLOCK_RECS];
} osk_idx_page_t;

/* Was im Klartext vor der verschluesselten Indexfassung steht. */
typedef struct {
	uint32_t magic;	 /* OSK_IDX_MAGIC */
	uint32_t ct_len; /* Kryptotext samt Pruefwert */
	uint8_t nonce[OSK_NONCE_LEN];
	uint32_t crc32; /* ueber diesen Rahmen, nicht ueber die Daten */
} osk_idx_frame_t;

OSK_STATIC_ASSERT(sizeof(osk_idx_frame_t) == 24, idx_frame_24);

/* Platz, den ein Block im Index-Ring belegt - fuer jeden Block gleich, auch
 * fuer einen halb gefuellten. Dadurch braucht der Kopf keine Laengenliste, und
 * die Platzvergabe rechnet mit einer Zahl statt mit achtundvierzig. */
#define OSK_IDX_BLOCK_SPAN                                                               \
	((((uint32_t)(sizeof(osk_idx_frame_t) + sizeof(osk_idx_page_t) + OSK_TAG_LEN)) + \
	  OSK_SECTOR - 1u) &                                                             \
	 ~(OSK_SECTOR - 1u))

/* Alle gueltigen Bloecke muessen gleichzeitig in den Ring passen, und zwar mit
 * Luft: ein neuer Block wird immer an einen FREIEN Platz geschrieben, bevor der
 * alte ungueltig wird. */
OSK_STATIC_ASSERT((OSK_AREA_IDX_LEN + OSK_AREA_IDX2_LEN) >
		      2u * OSK_PROFILE_MAX * OSK_IDX_BLOCKS_MAX * OSK_IDX_BLOCK_SPAN,
		  idx_ring_has_room);

/* ---- Geheimnisse ------------------------------------------------------ */
/* Jedes Geheimnis liegt auf einem eigenen Platz von zwei Sektoren, mit eigener
 * Nonce. Der Rahmen davor steht im Klartext, damit ein Platz ohne Schluessel
 * geprueft werden kann; die Eintragskennung darin verraet nichts ueber den
 * Inhalt. Ein Platz wird immer ganz geloescht und ganz neu geschrieben. */

#define OSK_BLOB_MAGIC 0x424B534Fu /* "OSKB" */
#define OSK_PUB_MAGIC 0x504B534Fu  /* "OSKP" - oeffentlicher Teil */

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

#define OSK_LOG_MAGIC 0x434B534Fu /* "OSKC" - verkettet, ab Fassung 6 */
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
/* Laenge eines Kettenglieds: die ersten Byte des Pruefwerts des
 * VORHERGEHENDEN Platzes. Der Pruefwert ist schon da und haengt am Schluessel
 * des Protokolls - ein eigener Abdruck waere eine zweite Groesse fuer
 * dieselbe Sache, und ein UNgeschluesselter waere wertlos: wer einen Satz
 * herausnimmt, koennte eine offene Kette einfach neu rechnen. */
#define OSK_LOG_CHAIN_LEN 16u

typedef struct {
	uint32_t magic;
	uint32_t seq;
	uint8_t nonce[OSK_NONCE_LEN];

	/* Kettenglied zum vorhergehenden Satz (ab Fassung 6). Es steht im
	 * Klartext, damit der Platz noch lesbar ist, wenn sein Vorgaenger im
	 * Ring schon ueberschrieben wurde - und es ist zugleich zusaetzliche
	 * Daten der Verschluesselung. Wer es umschreibt, macht den Satz
	 * unlesbar; wer einen Satz herausnimmt, laesst das Glied des naechsten
	 * ins Leere zeigen. Beides faellt auf. */
	uint8_t chain[OSK_LOG_CHAIN_LEN];

	uint8_t ct[sizeof(osk_log_plain_t) + OSK_TAG_LEN];
	uint32_t crc32;
} osk_log_slot_t;

OSK_STATIC_ASSERT(sizeof(osk_log_slot_t) <= OSK_LOG_SLOT, log_slot_fits);
OSK_STATIC_ASSERT(OSK_LOG_CHAIN_LEN <= OSK_TAG_LEN, chain_fits_tag);

/* Saetze der Fassungen 1 bis 5 hatten kein Kettenglied und eine andere
 * Kennung. Sie werden weiter gelesen, damit ein Firmware-Update das
 * Geraeteprotokoll nicht wegwirft - geprueft werden kann die Kette ueber sie
 * aber nicht, und genau das meldet osk_log_verify(). */
#define OSK_LOG_MAGIC_V5 0x4C4B534Fu /* "OSKL" - bis Fassung 5 */

typedef struct {
	uint32_t magic;
	uint32_t seq;
	uint8_t nonce[OSK_NONCE_LEN];
	uint8_t ct[sizeof(osk_log_plain_t) + OSK_TAG_LEN];
	uint32_t crc32;
} osk_log_slot_v5_t;

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
