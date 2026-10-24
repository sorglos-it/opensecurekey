/* Verteiler der Kartenanwendungen.
 *
 * Auf einem Transport liegt ein Bearbeiter (osk_transport_register), auf der
 * Karte liegen aber mehrere Anwendungen: PIV und OpenPGP. Dieses Modul ist
 * der eine Bearbeiter und sucht nach SELECT die passende Anwendung heraus.
 *
 * Es nimmt ihnen ausserdem drei Dinge ab, die beide Spezifikationen gleich
 * verlangen (ISO/IEC 7816-4 Abschnitte 5.1.1 und 5.3.4):
 *   - Kommandoverkettung: CLA Bit 5 sammelt lange Daten in Haeppchen,
 *   - Antwortverkettung: was nicht in eine kurze Antwort passt, wird mit
 *     61xx angekuendigt und mit GET RESPONSE abgeholt,
 *   - Auswahl nach Anwendungskennung, auch mit abgekuerzter Kennung.
 *
 * Alle Puffer sind fest. Passt etwas nicht, kommt ein Statuswort zurueck -
 * halb gelesen wird nie.
 */
#ifndef OSK_APPLETS_H
#define OSK_APPLETS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"
#include "osk_transport.h"
#include "osk_apdu.h"

/* Eine Anwendung bearbeitet ein Kommando und liefert ihr Statuswort. Die
 * Antwortdaten kommen in `res`, ohne Statuswort - das haengt der Verteiler
 * an, weil nur er weiss, ob verkettet werden muss. */
typedef uint16_t (*osk_applet_fn)(const osk_apdu_t *apdu, osk_resp_t *res);

typedef struct {
	const uint8_t *aid;
	uint8_t aid_len;
	const char *name; /* steht auf dem Display, wenn gefragt wird */
	osk_applet_fn handle;
	void (*reset)(void); /* Karte aus und wieder an: Freigaben fallen */
} osk_applet_t;

/* Bringt die Anwendungen in den Anfangszustand. Angemeldet wird der
 * Verteiler vom Transport (osk_ccid_init), so wie CTAP sich beim
 * USB-HID-Transport anmeldet. */
osk_result_t osk_applets_init(void);

/* Alles auf Anfang: keine Anwendung gewaehlt, keine PIN mehr freigegeben,
 * keine halbe Verkettung offen. Wird beim Ein- und Ausschalten der Karte
 * gerufen. */
void osk_applets_reset(void);

/* Der Bearbeiter fuer osk_transport_register. */
osk_result_t osk_applets_handle(const osk_request_t *req, osk_response_t *res);

/* Die Anwendungen melden sich hierueber an. Reihenfolge entscheidet nur bei
 * abgekuerzten Kennungen, und die sind bei uns eindeutig. */
const osk_applet_t *osk_piv_applet(void);
const osk_applet_t *osk_openpgp_applet(void);

#endif
