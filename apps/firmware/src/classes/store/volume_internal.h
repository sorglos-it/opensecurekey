/* Nur zwischen den volume_*.c-Dateien. Nichts davon ist Vertrag - der steht
 * in osk_volume.h. */
#ifndef OSK_VOLUME_INTERNAL_H
#define OSK_VOLUME_INTERNAL_H

#include <stdint.h>
#include "osk_volume.h"
#include "osk_volume_crypto.h"

/* Zweck in der Nonce. Trennt Daten von Wurzel und eingewickelten
 * Schluesseln, wie OSK_NONCE_BLOB/INDEX/LOG beim Tresor. */
#define OSK_VOL_PURPOSE_DATA 0x44415441u /* DATA */
#define OSK_VOL_PURPOSE_WRAP 0x57524150u /* WRAP */

/* Fassung des Formats auf der Karte. Steigt, wenn sich der Aufbau aendert. */
#define OSK_VOL_FORMAT_VERSION 1u

#define OSK_VOL_MAGIC 0x564B534Fu /* "OSKV" als Zahl */

/* Kennung des Datentraegers. 16 Byte Zufall, steht in jedem Block in den
 * zusaetzlichen Daten - damit eine FREMDE Karte in diesem Stick auffaellt. */
#define OSK_VOL_ID_LEN 16u

/* Eine Empfaengerzeile auf der Karte, feste Laenge. Feste Laenge, weil der
 * Kopf ohne Laengenfelder auskommen soll: jedes Laengenfeld an einer Stelle,
 * die ein fremder Rechner beeinflussen kann, ist ein Parser mehr. */
#define OSK_VOL_RECV_SALT_LEN 16u
#define OSK_VOL_RECV_EPHEM_LEN 32u
#define OSK_VOL_RECV_WRAP_LEN (OSK_KEY_LEN + OSK_VOL_AEAD_TAG_LEN) /* 48 */
#define OSK_VOL_RECV_LINE_LEN \
	(4u + OSK_VOL_RECV_SALT_LEN + OSK_VOL_RECV_EPHEM_LEN + OSK_VOL_RECV_WRAP_LEN)

/* Stelle des Blattes eines Blocks im Stempelfeld. */
uint32_t vol_leaf_page_of(const osk_vol_layout_t *l, uint32_t block);
uint32_t vol_leaf_offset_of(uint32_t block);

/* Eine Empfaengerzeile bauen beziehungsweise aufmachen. volume_recv.c. */
osk_result_t vol_recv_wrap(const osk_volume_config_t *cfg, const osk_vol_recipient_t *recv,
			   const uint8_t id[OSK_VOL_ID_LEN], const osk_key_t *k_data,
			   uint8_t line[OSK_VOL_RECV_LINE_LEN]);

/* Meldet OSK_ERR_DENIED, wenn diese Zeile nicht zu diesem Stick und diesem
 * Wort passt - nicht OSK_ERR_FORMAT. Der falsche Stick ist kein Fehler im
 * Format. */
osk_result_t vol_recv_unwrap(const osk_volume_config_t *cfg,
			     const uint8_t line[OSK_VOL_RECV_LINE_LEN],
			     const uint8_t id[OSK_VOL_ID_LEN], const osk_vol_recipient_t *unlock,
			     osk_key_t *k_data_out);

#endif
