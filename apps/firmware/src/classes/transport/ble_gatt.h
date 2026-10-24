/* Der Bluetooth-Dienst fuer das Verwaltungsprogramm.
 *
 * Hier laeuft NICHT FIDO. Das ist eine getroffene Entscheidung: Browser
 * unterstuetzen FIDO ueber Bluetooth nicht verlaesslich, also gibt es den
 * Weg hier gar nicht - ein abgeschalteter Weg ist besser als einer, der
 * manchmal geht. Ueber diesen Dienst laeuft dasselbe Protokoll wie ueber
 * den eigenen USB-Kanal (shared/protocol.md).
 *
 * Zugang: nur auf einer verschluesselten, beglaubigten Strecke zu einer
 * gekoppelten Gegenstelle. Jeder Zugriff ohne das bekommt einen Fehler,
 * nicht eine leere Antwort - siehe ble_pair.h fuer die Kopplung.
 *
 * Zerlegung: eine Nachricht geht in Stuecken ueber die Strecke, weil die
 * ATT-Pakethoechstlaenge klein sein kann (23 Byte sind erlaubt). Das erste
 * Stueck nennt die Gesamtlaenge, die folgenden ihre Reihenfolge. Jede
 * Laenge wird vor dem Lesen geprueft; es gibt feste Obergrenzen und keinen
 * dynamischen Speicher.
 *
 * Es ist nichts davon am Geraet gemessen. Siehe docs/funk.md. */
#ifndef OSK_BLE_GATT_H
#define OSK_BLE_GATT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"

/* So lang darf eine Nachricht des Protokolls werden. */
#define OSK_BLE_MSG_MAX 1300
/* Kleinste und groesste ATT-Pakethoechstlaenge (Band 3 Teil F 3.2.8). */
#define OSK_BLE_MTU_MIN 23u
#define OSK_BLE_MTU_MAX 247u

/* Kennungen dieses Dienstes. Eigene 128-Bit-Kennungen, damit nichts mit
 * einem fremden Dienst verwechselt wird. */
#define OSK_BLE_UUID_SERVICE                             \
	{0x4f, 0x53, 0x4b, 0x01, 0x9b, 0x2a, 0x4c, 0x7e, \
	 0x8f, 0x31, 0x2d, 0x6a, 0x5e, 0x0c, 0x73, 0x11}
#define OSK_BLE_UUID_REQUEST                             \
	{0x4f, 0x53, 0x4b, 0x01, 0x9b, 0x2a, 0x4c, 0x7e, \
	 0x8f, 0x31, 0x2d, 0x6a, 0x5e, 0x0c, 0x73, 0x12}
#define OSK_BLE_UUID_RESPONSE                            \
	{0x4f, 0x53, 0x4b, 0x01, 0x9b, 0x2a, 0x4c, 0x7e, \
	 0x8f, 0x31, 0x2d, 0x6a, 0x5e, 0x0c, 0x73, 0x13}
#define OSK_BLE_UUID_STATUS                              \
	{0x4f, 0x53, 0x4b, 0x01, 0x9b, 0x2a, 0x4c, 0x7e, \
	 0x8f, 0x31, 0x2d, 0x6a, 0x5e, 0x0c, 0x73, 0x14}
#define OSK_BLE_UUID_LEN 16

/* Fehler der ATT-Schicht, die dieser Dienst zurueckgibt (Band 3 Teil F,
 * Tabelle 3.4). 0 heisst: in Ordnung. */
#define OSK_ATT_OK 0x00u
#define OSK_ATT_INVALID_HANDLE 0x01u
#define OSK_ATT_WRITE_NOT_PERMITTED 0x03u
#define OSK_ATT_INSUFFICIENT_AUTHENTICATION 0x05u
#define OSK_ATT_INVALID_OFFSET 0x07u
#define OSK_ATT_INVALID_ATTRIBUTE_VALUE_LENGTH 0x0du
#define OSK_ATT_UNLIKELY_ERROR 0x0eu
#define OSK_ATT_INSUFFICIENT_ENCRYPTION 0x0fu

/* Kopfbyte des ersten Stuecks. Danach zwei Byte Gesamtlaenge. */
#define OSK_BLE_FRAG_FIRST 0x80u
/* So viele Fortsetzungsstuecke sind moeglich (0 bis 127). */
#define OSK_BLE_FRAG_MAX_SEQ 0x7fu

/* So gehen Antwortstuecke hinaus (Benachrichtigung auf der Antwortgroesse). */
typedef void (*osk_ble_notify_fn)(const uint8_t *data, size_t len, void *ctx);

/* Bearbeiter einer vollstaendigen Protokollnachricht. Das ist derselbe
 * Bearbeiter, der am eigenen USB-Kanal haengt - dieses Modul kennt das
 * Protokoll nicht. */
typedef osk_result_t (*osk_ble_message_fn)(const uint8_t *msg, size_t len, uint8_t *out, size_t cap,
					   size_t *out_len, void *ctx);

osk_result_t osk_ble_gatt_init(osk_ble_notify_fn notify, osk_ble_message_fn handler, void *ctx);

/* Den ganzen Bluetooth-Weg abschalten. */
void osk_ble_gatt_enable(bool on);
bool osk_ble_gatt_enabled(void);

/* Meldungen des Bluetooth-Stapels. */
void osk_ble_gatt_connected(const uint8_t address[6]);
void osk_ble_gatt_security_changed(bool encrypted, bool authenticated);
void osk_ble_gatt_disconnected(void);
void osk_ble_gatt_set_mtu(uint16_t mtu);
void osk_ble_gatt_subscribe(bool on);

/* Ein Schreibzugriff auf die Anfragegroesse. Liefert einen ATT-Fehler,
 * 0 heisst angenommen. */
uint8_t osk_ble_gatt_write_request(const uint8_t *data, size_t len);

/* Ein Lesezugriff auf die Zustandsgroesse. */
uint8_t osk_ble_gatt_read_status(uint8_t *out, size_t cap, size_t *len_out);

/* Wahr, wenn die Strecke alle Bedingungen erfuellt. Nur fuer Anzeige und
 * Pruefungen. */
bool osk_ble_gatt_ready(void);

#endif
