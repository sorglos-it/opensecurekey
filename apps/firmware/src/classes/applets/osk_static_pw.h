/* Statisches Passwort: das Geraet tippt eine feste Zeichenkette.
 *
 * Warum es das gibt: es ist der einzige Weg, der ueberall ankommt - auch
 * am Anmeldebildschirm und im BIOS, wo kein Programm laeuft, das PKCS#11
 * oder eine Erweiterung bedienen koennte (docs/plan.md, "Wege zu fremder
 * Software", erste Zeile der Tabelle).
 *
 * HIER LIEGT NUR DIE KARTENSEITE. Das Tippen selbst gehoert dem
 * USB-Tastatur-Transport (src/classes/transport/, Strang E). Die Naht ist
 * absichtlich so gelegt, dass sie in diese Richtung zeigt:
 *
 *   Der Transport ruft osk_static_pw_fetch() und tippt, was er bekommt.
 *
 * Dieses Modul ruft NICHTS im Transport auf. Darum braucht es keine
 * Kopfdatei von dort und die beiden Straenge koennen nebeneinander
 * gebaut werden.
 *
 * Was der Transport dafuer bereitstellen muss, steht in einem Satz:
 *
 *   osk_result_t osk_kbd_type(const uint8_t *ascii, size_t len,
 *                             uint8_t layout, bool enter);
 *
 * und er vergibt die Nummern der Tastaturbelegungen. Bis er welche nennt,
 * nimmt die Karte nur OSK_KBD_LAYOUT_US an - lieber eine Belegung, die
 * nachweislich gemeint ist, als eine Zahl, die auf beiden Seiten etwas
 * anderes heisst. Genau dieser Fehler ist in diesem Projekt siebenmal
 * aufgetreten.
 *
 * ZEICHENVORRAT: nur druckbares ASCII, 0x20 bis 0x7E. Nicht aus
 * Bequemlichkeit, sondern weil jedes Zeichen beim Tippen zu einem
 * Tastencode wird, und wo dieser Code liegt, haengt an der Belegung des
 * Rechners. Ein Umlaut oder ein Zeichen ueber 0x7F wird auf einer
 * fremden Belegung zu einem anderen Zeichen - und ein Passwort, das
 * manchmal anders ankommt, ist schlimmer als keines. Was abgelehnt wird,
 * wird beim Setzen abgelehnt und nicht beim Tippen.
 */
#ifndef OSK_STATIC_PW_H
#define OSK_STATIC_PW_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"

/* Zwei Steckplaetze, wie bei den Einmalcodes (osk_otp.h): einer fuer den
 * kurzen Druck, einer fuer den langen. */
#define OSK_STATIC_PW_SLOTS 2u

/* Hoechstlaenge. 64 Zeichen, weil das der laengste Eintrag ist, den eine
 * Zeile des Verwaltungsprogramms noch zeigt, und weil laengere Passwoerter
 * an Anmeldefeldern haeufig abgeschnitten werden. */
#define OSK_STATIC_PW_MAX 64u

/* Die einzige Belegung, die die Karte heute annimmt. Die Nummer vergibt
 * Strang E; bis dahin ist 0 die, auf die beide Seiten sich einigen. */
#define OSK_KBD_LAYOUT_US 0u

typedef struct {
	uint8_t layout;	    /* Tastaturbelegung */
	bool append_enter;  /* hinterher die Eingabetaste */
	uint8_t len;	    /* Zeichen, ohne Null am Ende */
	uint8_t text[OSK_STATIC_PW_MAX + 1u]; /* ein Byte mehr fuer die Null */
} osk_static_pw_t;

/* Alles auf Anfang - Karte aus und wieder an. Loescht nur das Gemerkte,
 * nicht den Tresoreintrag. */
void osk_static_pw_reset(void);

/* Prueft, ob eine Zeichenkette als statisches Passwort in Frage kommt:
 * Laenge 1 bis OSK_STATIC_PW_MAX, jedes Byte 0x20 bis 0x7E. Getrennt
 * aufrufbar, damit das Verwaltungsprogramm dasselbe Urteil bekommt wie
 * die Karte - und es nur an einer Stelle steht. */
bool osk_static_pw_text_ok(const uint8_t *text, size_t len);

/* Legt ein Passwort ab oder ersetzt es. Verlangt die freigegebene PIN und
 * einen Tastendruck; das prueft der Aufrufer (das Applet), nicht diese
 * Funktion. OSK_ERR_ARG bei Steckplatz, Laenge, Zeichen oder Belegung,
 * die nicht gehen. */
osk_result_t osk_static_pw_set(uint8_t slot, const osk_static_pw_t *pw);

/* Entfernt das Passwort eines Steckplatzes. OSK_ERR_NOT_FOUND, wenn dort
 * keines liegt. */
osk_result_t osk_static_pw_clear(uint8_t slot);

/* Liegt in diesem Steckplatz eines? Fragt nur die Liste des Tresors ab
 * und oeffnet nichts - deshalb fragt es auch nicht am Display. */
bool osk_static_pw_present(uint8_t slot);

/* Holt das Passwort zum Tippen.
 *
 * Das OEFFNET einen Tresoreintrag und fragt damit am Display nach
 * (osk_store_reveal). Das ist nicht wegzulassen: hier verlaesst ein
 * Geheimnis das Geraet, und zwar als getippter Text, den jedes Programm
 * mitlesen kann. `asked_by` steht dabei auf der Anzeige. */
osk_result_t osk_static_pw_fetch(uint8_t slot, const char *asked_by, osk_static_pw_t *out);

#endif
