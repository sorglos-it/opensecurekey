/* Kopplung ueber Bluetooth: LE Secure Connections mit Zahlenvergleich.
 *
 * Regeln, die hier durchgesetzt werden - nicht nur dokumentiert:
 *
 *   1. Nur LE Secure Connections. Die alte Kopplung ("Legacy Pairing") wird
 *      abgewiesen, auch wenn das Gegenueber nichts anderes kann.
 *   2. Kein "Just Works". Wer nichts anzeigen oder bestaetigen kann, kommt
 *      nicht herein - sonst koppelt sich jeder in Funkreichweite.
 *   3. Zahlenvergleich: die sechs Ziffern stehen auf dem Display des Geraets
 *      und muessen am Rechner gleich aussehen.
 *   4. Verschluesselung ist Pflicht. Ohne sie wird kein Dienst beantwortet.
 *   5. Gekoppelt wird erst nach einem Tastendruck am Geraet. Ohne Druck gibt
 *      es die Zahl nicht einmal zu sehen.
 *   6. Wechselnde Funkadresse (RPA) gegen Verfolgung.
 *   7. Gekoppelte Geraete sind einzeln loeschbar.
 *
 * Bluetooth-Kernspezifikation 5.4, Band 3 Teil H, Abschnitt 2.3
 * (Kopplungsverfahren und Auswahl der Methode).
 *
 * Es ist nichts davon am Geraet gemessen - es gibt noch keine Platine.
 * Siehe docs/funk.md. */
#ifndef OSK_BLE_PAIR_H
#define OSK_BLE_PAIR_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"

/* So viele Gegenstellen koennen gekoppelt sein. Mehr braucht niemand fuer
 * einen Sicherheitsschluessel, und eine feste Zahl heisst: kein dynamischer
 * Speicher und keine Liste, die ein Angreifer vollmachen kann. */
#define OSK_BLE_MAX_BONDS 4
/* Laenge eines Namens in der Liste, die das Programm anzeigt. */
#define OSK_BLE_NAME_MAX 32
/* Die Zahl des Vergleichs hat immer sechs Ziffern (Band 3 Teil H 2.3.5.6). */
#define OSK_BLE_COMPARE_DIGITS 6

/* Wie lange die wechselnde Funkadresse gilt. Die Kernspezifikation erlaubt
 * bis zu 11,5 Stunden; 15 Minuten ist der Wert, den auch die Empfehlung
 * fuer Datenschutz nennt - kuerzer kostet nur Strom. */
#define OSK_BLE_RPA_INTERVAL_MS (15u * 60u * 1000u)

/* Was das Gegenueber anzeigen und eingeben kann (Band 3 Teil H, Tabelle 2.3). */
typedef enum {
	OSK_BLE_IO_DISPLAY_ONLY = 0,
	OSK_BLE_IO_DISPLAY_YES_NO,
	OSK_BLE_IO_KEYBOARD_ONLY,
	OSK_BLE_IO_NONE,
	OSK_BLE_IO_KEYBOARD_DISPLAY,
} osk_ble_io_t;

/* Die Methode, die sich daraus ergibt. */
typedef enum {
	OSK_BLE_METHOD_NONE = 0,
	OSK_BLE_METHOD_JUST_WORKS, /* wird immer abgewiesen */
	OSK_BLE_METHOD_NUMERIC_COMPARISON,
	OSK_BLE_METHOD_PASSKEY,
} osk_ble_method_t;

/* Eine Kopplungsanfrage, wie sie der Bluetooth-Stapel meldet. */
typedef struct {
	osk_ble_io_t peer_io;
	bool secure_connections; /* Gegenueber kann LE Secure Connections */
	bool mitm_requested;	 /* Gegenueber verlangt Schutz gegen Mithoeren */
	bool bonding;		 /* dauerhaft koppeln, nicht nur diese Sitzung */
	uint8_t peer_address[6];
	const char *peer_name; /* darf NULL sein */
} osk_ble_pair_req_t;

/* Ein gekoppeltes Geraet, wie das Programm es sieht. */
typedef struct {
	bool used;
	uint8_t address[6];
	char name[OSK_BLE_NAME_MAX];
	uint64_t paired_ms; /* wann gekoppelt */
	uint64_t last_ms;   /* wann zuletzt verbunden */
} osk_ble_bond_t;

void osk_ble_pair_reset(void);

/* Welche Methode sich aus den Faehigkeiten ergibt. Das ist die Tabelle aus
 * der Kernspezifikation, nachgerechnet - damit die Pruefung sie einzeln
 * befragen kann. */
osk_ble_method_t osk_ble_pair_method(osk_ble_io_t peer_io);

/* Eine Kopplungsanfrage pruefen und, wenn sie passt, den Zahlenvergleich
 * beginnen. Fragt dabei den Taster.
 *
 * OSK_ERR_UNSUPPORTED - keine Secure Connections oder "Just Works"
 * OSK_ERR_DENIED      - der Nutzer hat nicht gedrueckt
 * OSK_ERR_FULL        - die Liste ist voll
 * OSK_OK              - weiter mit osk_ble_pair_show_number */
osk_result_t osk_ble_pair_begin(const osk_ble_pair_req_t *req);

/* Die sechs Ziffern aus dem Schluesselaustausch anzeigen und bestaetigen
 * lassen. Erst danach gilt die Kopplung. */
osk_result_t osk_ble_pair_confirm_number(uint32_t number);

/* Der Stapel meldet, dass die Kopplung fertig und die Strecke
 * verschluesselt ist. Erst hier wandert der Eintrag in die Liste. */
osk_result_t osk_ble_pair_complete(bool encrypted, bool authenticated);

/* Die Liste fuer das Verwaltungsprogramm. */
size_t osk_ble_bond_count(void);
bool osk_ble_bond_get(size_t index, osk_ble_bond_t *out);
/* Einzeln loeschen - nach Platz in der Liste. */
osk_result_t osk_ble_bond_forget(size_t index);
void osk_ble_bond_forget_all(void);

/* Wahr, wenn diese Adresse gekoppelt ist. */
bool osk_ble_bond_known(const uint8_t address[6]);

/* Wechselnde Funkadresse: wahr, wenn eine neue faellig ist. Der Aufrufer
 * laesst sie dann vom Stapel neu ziehen und meldet das zurueck. */
bool osk_ble_rpa_due(void);
void osk_ble_rpa_rotated(void);

#endif
