/* Stueckweises Verschluesseln fuer Sicherungsdatei und Stick-zu-Stick.
 *
 * Beide tragen den Tresor durch eine Leitung, die niemandem gehoert: die eine
 * auf eine Platte, die andere ueber einen Rechner zum Zweitstick. Beide
 * duerfen dabei nie mehr als einen Eintrag im RAM halten. Darum teilen sie
 * dieselben zwei Puffer und denselben Rahmen:
 *
 *   [u32 Laenge des Kryptotextes][Kryptotext mit Pruefwert]
 *
 * Zusaetzliche Daten der Verschluesselung sind der Kopf des jeweiligen
 * Vorgangs und die laufende Nummer des Stuecks. Dadurch laesst sich ein Stueck
 * nicht in eine andere Datei, an eine andere Stelle oder in eine andere
 * Uebertragung verschieben. Der fortgeschriebene Abdruck deckt alle Stuecke
 * davor; das Abschlussstueck nennt ihn, damit ein Abbruch auffaellt. */
#ifndef OSK_STORE_CHUNK_H
#define OSK_STORE_CHUNK_H

#include <stdint.h>
#include <stddef.h>
#include "osk_result.h"
#include "osk_crypto.h"
#include "osk_vault_format.h"

#define OSK_CHUNK_ITEM 1u
#define OSK_CHUNK_END  2u

/* Kopf (4 Byte) + Beschreibung des Eintrags (224) + Geheimnis (4096). */
#define OSK_CHUNK_ITEM_HEAD 224u
#define OSK_CHUNK_PLAIN_MAX (4u + OSK_CHUNK_ITEM_HEAD + OSK_SECRET_MAX)
#define OSK_CHUNK_CT_MAX    (OSK_CHUNK_PLAIN_MAX + OSK_TAG_LEN)

/* Platz fuer die zusaetzlichen Daten: Kopf der Sicherungsdatei (52) oder
 * Fassung plus beide oeffentlichen Schluesselteile der Uebertragung (131),
 * jeweils plus die laufende Nummer. */
#define OSK_CHUNK_AAD_MAX 192u

typedef osk_result_t (*osk_chunk_sink_fn)(void *ctx, const uint8_t *data,
					  size_t len);
typedef osk_result_t (*osk_chunk_src_fn)(void *ctx, uint8_t *data, size_t len);

/* Der gemeinsame Klartextpuffer. Wer ein Stueck schreibt, legt den Inhalt
 * hier ab; wer eines liest, findet ihn hier. */
uint8_t *osk_chunk_plain(void);

void osk_chunk_wipe(void);

osk_result_t osk_chunk_put(const uint8_t *aad, size_t aad_len,
			   const osk_key_t *key, uint32_t seq,
			   osk_nonce_purpose_t purpose, size_t plain_len,
			   osk_chunk_sink_fn sink, void *ctx,
			   uint8_t h[OSK_HASH_LEN]);

/* OSK_ERR_DENIED heisst: falscher Schluessel oder beschaedigtes Stueck. */
osk_result_t osk_chunk_get(const uint8_t *aad, size_t aad_len,
			   const osk_key_t *key, uint32_t seq,
			   osk_nonce_purpose_t purpose, osk_chunk_src_fn src,
			   void *ctx, size_t *plain_len_out,
			   uint8_t h[OSK_HASH_LEN]);

#endif
