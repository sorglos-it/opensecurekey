/* Statisches Passwort: das Geraet tippt eine feste Zeichenkette.
 *
 * Warum es das gibt: es ist der einzige Weg, der ueberall ankommt - auch
 * am Anmeldebildschirm und im BIOS, wo kein Programm laeuft, das PKCS#11
 * oder eine Erweiterung bedienen koennte (docs/plan.md, "Wege zu fremder
 * Software", erste Zeile der Tabelle).
 *
 * HIER LIEGT NUR DIE KARTENSEITE. Das Tippen selbst gehoert dem
 * USB-Tastatur-Transport (src/classes/transport/usb_kbd.h, Strang E). Die
 * Naht zeigt in diese Richtung:
 *
 *   Der Transport ruft osk_static_pw_fetch() und danach osk_kbd_type().
 *
 * Dieses Modul ruft NICHTS im Transport auf - keine Funktion, keine
 * Bindung. Es liest aus usb_kbd.h genau EINE Angabe, und zwar als Text:
 * welche Zeichen die Tastaturseite ueberhaupt tippen kann.
 *
 * WARUM DIESE EINE ANGABE SEIN MUSS - und das ist der wichtigste Satz
 * dieser Datei. Die Tastaturseite kann 45 Zeichen tippen, nicht 95:
 *
 *   cbdefghijklnrtuv CBDEFGHIJKLNRTUV 0123456789 ! Tab Eingabe
 *
 * Kein a, kein o, kein s, kein Leerzeichen, kein @. Der Grund liegt im
 * Yubico-Protokoll, dessen Abbildung genau 45 Plaetze hat (usb_kbd.h).
 * Diese Karte nahm bis zum 29.10.2026 alles druckbare ASCII an - 95
 * Zeichen. Damit waere ein Passwort entstanden, das die Karte annimmt und
 * die Tastatur dann als Ganzes verweigert: der Nutzer setzt es, und es
 * tippt sich nie. Zwei Groessen, die gleich heissen ("erlaubte Zeichen")
 * und Verschiedenes messen - das Fehlermuster dieses Projekts, zum
 * achten Mal.
 *
 * Darum gilt: WAS NICHT GETIPPT WERDEN KANN, WIRD NICHT ANGENOMMEN. Die
 * Liste steht nicht hier, sondern in OSK_KBD_SCAN_ORDER - eine Angabe an
 * einer Stelle, und wenn Strang E sie erweitert, kann diese Karte sofort
 * mehr. Abgelehnt wird beim SETZEN, nicht beim Tippen.
 *
 * Dass die Zeichen so knapp sind, ist eine Produktfrage und keine dieses
 * Moduls: sie gehoert zum Yubico-Protokoll und ist dem Master gemeldet.
 *
 * DIE BELEGUNG (welcher Tastenplatz welches Zeichen ist) steht ebenfalls
 * nicht hier. Sie ist die Abbildung in der Tastaturseite, gesetzt ueber
 * osk_kbd_set_scan_map(). Eine Belegungs-NUMMER gibt es nicht und darf es
 * hier nicht geben - sie wuerde auf beiden Seiten etwas anderes heissen.
 */
#ifndef OSK_STATIC_PW_H
#define OSK_STATIC_PW_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"
/* Nur fuer OSK_KBD_SCAN_ORDER und OSK_KBD_QUEUE_MAX - zwei Texte, keine
 * Funktion. Es entsteht keine Bindung an die Tastaturseite. */
#include "usb_kbd.h"

/* Zwei Steckplaetze, wie bei den Einmalcodes (osk_otp.h): einer fuer den
 * kurzen Druck, einer fuer den langen. */
#define OSK_STATIC_PW_SLOTS 2u

/* Hoechstlaenge. Es sind dieselben 64 Zeichen, die in einen Tippauftrag
 * passen (OSK_KBD_QUEUE_MAX in usb_kbd.h) - und sie stehen hier NICHT
 * als eigene Zahl, sondern werden von dort geholt. Zwei Obergrenzen
 * waeren wieder zwei Wahrheiten: die kleinere wuerde gelten und niemand
 * wuesste, welche. */
#define OSK_STATIC_PW_MAX ((uint8_t)OSK_KBD_QUEUE_MAX)

typedef struct {
	bool append_enter; /* hinterher die Eingabetaste */
	uint8_t len;	   /* Zeichen, ohne Null am Ende */
	uint8_t text[OSK_STATIC_PW_MAX + 1u]; /* ein Byte mehr fuer die Null */
} osk_static_pw_t;

/* Alles auf Anfang - Karte aus und wieder an. Loescht nur das Gemerkte,
 * nicht den Tresoreintrag. */
void osk_static_pw_reset(void);

/* Prueft, ob eine Zeichenkette als statisches Passwort in Frage kommt:
 * Laenge 1 bis OSK_STATIC_PW_MAX, und jedes Zeichen muss in
 * OSK_KBD_SCAN_ORDER vorkommen - also wirklich tippbar sein. Getrennt
 * aufrufbar, damit das Verwaltungsprogramm dasselbe Urteil bekommt wie
 * die Karte und der Nutzer das Nein beim Eintippen sieht und nicht erst
 * beim Benutzen. */
bool osk_static_pw_text_ok(const uint8_t *text, size_t len);

/* Legt ein Passwort ab oder ersetzt es. Verlangt die freigegebene PIN und
 * einen Tastendruck; das prueft der Aufrufer (das Applet), nicht diese
 * Funktion. OSK_ERR_ARG bei Steckplatz, Laenge oder Zeichen, die nicht
 * gehen. */
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
