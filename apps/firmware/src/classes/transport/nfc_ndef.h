/* Antippen ohne App: NFC Forum Type 4 Tag mit einer NDEF-Nachricht.
 *
 * Wozu das gut ist: Stick ans Handy halten, und das Handy oeffnet eine
 * hinterlegte Adresse oder bekommt ein Einmalkennwort getippt - ohne dass
 * irgendeine App installiert sein muesste. Das Handy liest einfach einen
 * Aufkleber, und der Aufkleber sind wir.
 *
 * Abschaltbar: OSK_NDEF_OFF ist die Vorgabe. Solange das so steht, antwortet
 * diese Anwendung auf gar nichts.
 *
 * Tastendruck: auch hier. Ein Einmalkennwort ist ein Geheimnis und kommt nur
 * nach dem Druck auf den Taster heraus. Fuer die Adresse ist der Druck
 * ebenfalls die Vorgabe, weil "in der Tasche gelesen" sonst moeglich waere;
 * wer das reine Antippen will, schaltet ihn fuer die Adresse ausdruecklich
 * ab (siehe docs/funk.md).
 *
 * Geschrieben wird hier nie: die Datei ist schreibgeschuetzt. Der Parser
 * fuer eingehende Nachrichten ist trotzdem da und streng, weil er die
 * Konfiguration prueft, bevor sie gespeichert wird - und weil er die erste
 * Wand ist, wenn das Schreiben je aufgemacht wird. */
#ifndef OSK_NFC_NDEF_H
#define OSK_NFC_NDEF_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"

/* Die NDEF-Datei dieses Geraets. Mehr als eine kurze Adresse oder ein
 * Kennwort gehoert nicht hinein, darum reicht das reichlich. */
#define OSK_NDEF_FILE_MAX 512
/* Laengste Adresse, die gespeichert werden kann - ohne das Praefix. */
#define OSK_NDEF_URI_MAX 255
/* Laengstes Einmalkennwort, das getippt wird. */
#define OSK_NDEF_OTP_MAX 32
/* So viele Datensaetze liest der Parser hoechstens aus einer Nachricht. */
#define OSK_NDEF_MAX_RECORDS 8

/* Dateikennungen des Type-4-Tags (NFC Forum Type 4 Tag, Abschnitt 4.7). */
#define OSK_NDEF_CC_FILE_ID 0xe103u
#define OSK_NDEF_DATA_FILE_ID 0xe104u

typedef enum {
	OSK_NDEF_OFF = 0, /* Vorgabe: nichts antworten */
	OSK_NDEF_URI,	  /* eine hinterlegte Adresse */
	OSK_NDEF_OTP,	  /* ein Einmalkennwort als Text */
} osk_ndef_mode_t;

/* Praefixe des URI-Datensatzes (NFC Forum URI RTD, Tabelle 3). Nur die
 * beiden, die ein Geraet wie dieses braucht - alles andere steht als
 * vollstaendige Adresse im Text. */
#define OSK_NDEF_URI_NONE 0x00u
#define OSK_NDEF_URI_HTTP_WWW 0x01u
#define OSK_NDEF_URI_HTTPS_WWW 0x02u
#define OSK_NDEF_URI_HTTP 0x03u
#define OSK_NDEF_URI_HTTPS 0x04u

/* Woher das Einmalkennwort kommt. Das Erzeugen gehoert nicht hierher -
 * dieser Transport tippt nur, was ihm gegeben wird. Liefert die Laenge,
 * oder 0, wenn gerade keines herauszugeben ist. */
typedef size_t (*osk_ndef_otp_fn)(char *out, size_t cap, void *ctx);

typedef struct {
	osk_ndef_mode_t mode;
	uint8_t uri_prefix;
	char uri[OSK_NDEF_URI_MAX + 1];
	/* Vorgabe true. Auf false nur, wenn der Nutzer das reine Antippen
	 * ausdruecklich will - und nur fuer OSK_NDEF_URI. */
	bool uri_without_button;
} osk_ndef_config_t;

/* Ein gelesener Datensatz. Die Zeiger zeigen in den Eingabepuffer des
 * Aufrufers; es wird nichts kopiert und nichts angelegt. */
typedef struct {
	uint8_t tnf;
	bool first;
	bool last;
	const uint8_t *type;
	size_t type_len;
	const uint8_t *id;
	size_t id_len;
	const uint8_t *payload;
	size_t payload_len;
} osk_ndef_record_t;

/* Liest eine NDEF-Nachricht. Streng: jede Laenge wird vor dem Zugriff
 * geprueft, es gibt eine feste Obergrenze fuer die Zahl der Datensaetze,
 * und die Flaggen MB/ME muessen zur Lage im Strom passen. Liefert false,
 * sobald irgendetwas nicht stimmt - dann ist *count_out 0. */
bool osk_ndef_parse(const uint8_t *msg, size_t len, osk_ndef_record_t *out, size_t cap,
		    size_t *count_out);

/* Baut die NDEF-Datei (zwei Byte Laenge, dann die Nachricht) aus der
 * Einstellung. Liefert die Laenge, oder 0, wenn nichts auszugeben ist. */
size_t osk_ndef_build_file(uint8_t *out, size_t cap);

/* Einstellung setzen. Prueft die Adresse auf druckbare Zeichen und Laenge;
 * eine Adresse, die nicht passt, wird nicht uebernommen. */
osk_result_t osk_ndef_configure(const osk_ndef_config_t *cfg);
void osk_ndef_get_config(osk_ndef_config_t *out);

/* Quelle des Einmalkennworts anmelden. */
void osk_ndef_set_otp_source(osk_ndef_otp_fn fn, void *ctx);

/* Alles auf Anfang (OSK_NDEF_OFF). */
void osk_ndef_reset(void);

/* Eine APDU an die NDEF-Anwendung. Liefert die Laenge der Antwort.
 * Die Auswahl der Anwendung selbst macht nfc_tag. */
size_t osk_ndef_apdu(const uint8_t *apdu, size_t len, uint8_t *out, size_t cap);

/* Dateiauswahl zuruecksetzen - beim Kommen und Gehen des Feldes. */
void osk_ndef_deselect(void);

#endif
