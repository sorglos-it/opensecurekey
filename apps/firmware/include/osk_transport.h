/* Ein Transport bringt Anfragen herein und Antworten hinaus - USB, NFC oder BLE.
 * Die Applets kennen den Transport nicht; sie sehen nur Bytes und wissen,
 * welcher Weg es war, weil manche Regeln davon abhaengen.
 *
 * Umsetzung: src/classes/transport/ */
#ifndef OSK_TRANSPORT_H
#define OSK_TRANSPORT_H

#include <stdint.h>
#include <stddef.h>
#include "osk_result.h"

typedef enum {
	OSK_LINK_USB_HID,   /* FIDO2 */
	OSK_LINK_USB_CCID,  /* Smartcard: PIV, OpenPGP */
	OSK_LINK_USB_KBD,   /* tippt Passwoerter */
	OSK_LINK_USB_VENDOR,/* eigener Kanal zum Verwaltungsprogramm */
	OSK_LINK_USB_MSC,   /* schreibgeschuetztes Laufwerk */
	OSK_LINK_NFC,
	OSK_LINK_BLE,
} osk_link_t;

/* Eine Anfrage, wie sie bei einem Applet ankommt. */
typedef struct {
	osk_link_t link;
	const uint8_t *data;
	size_t len;
} osk_request_t;

typedef struct {
	uint8_t *data;
	size_t cap;
	size_t len;   /* vom Applet gesetzt */
} osk_response_t;

typedef osk_result_t (*osk_handler_fn)(const osk_request_t *req, osk_response_t *res);

/* Ein Applet meldet sich fuer einen Transport an. Mehrere Transporte duerfen
 * auf denselben Bearbeiter zeigen - CTAP laeuft ueber USB und NFC gleich. */
osk_result_t osk_transport_register(osk_link_t link, osk_handler_fn handler);

/* Einzeln abschaltbar, weil nicht jeder Rechner fuenf USB-Schnittstellen mag. */
osk_result_t osk_transport_enable(osk_link_t link, bool on);

#endif
