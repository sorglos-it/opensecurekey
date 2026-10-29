/* Das Geraet tippt als USB-Tastatur - das statische Passwort.
 *
 * Warum es diesen Weg gibt: eine getippte Zeichenkette kommt ueberall an,
 * auch im Anmeldebildschirm und im BIOS, wo kein Programm laeuft, das mit
 * einem Schluessel reden koennte.
 *
 * ================ DIE SCHNITTSTELLE FUER STRANG C =================
 * Strang C besitzt das Passwort: er haelt es im Tresor, prueft die
 * Freigabe und entscheidet, WANN getippt wird. Diese Schicht hier weiss
 * nicht, was sie tippt; sie uebersetzt Zeichen in Tastencodes und schiebt
 * Berichte hinaus.
 *
 * Die Arbeitsteilung in zwei Saetzen:
 *   C ruft osk_kbd_type(text, len) - einmal, wenn die Freigabe da ist.
 *   Diese Schicht liefert dafuer die Abbildung Zeichen -> Tastencode und
 *   die Warteschlange; C liefert Text, Zeitpunkt und Freigabe.
 *
 * Was C dabei WISSEN muss:
 *   - Der Text wird NICHT liegen gelassen. osk_kbd_type() uebersetzt
 *     sofort in Tastencodes und der Aufrufer darf seinen Klartext danach
 *     loeschen. Die Codes in der Warteschlange sind weiterhin ein
 *     Geheimnis und werden nach dem Tippen geloescht.
 *   - Es passen OSK_KBD_QUEUE_MAX Zeichen in einen Auftrag. Wie lang ein
 *     Passwort sein DARF, entscheidet nicht diese Schicht, sondern der
 *     Speicher, in dem es liegt. Hier steht nur, was in einen Auftrag
 *     geht.
 *   - Ein Zeichen, das in der Abbildung nicht vorkommt, laesst den ganzen
 *     Auftrag scheitern (OSK_ERR_UNSUPPORTED) - es wird NICHTS getippt.
 *     Halb getippte Passwoerter sind schlimmer als keine: der Nutzer
 *     sieht nicht, dass etwas fehlt.
 *   - Das Tippen braucht osk_kbd_pump() aus der Hauptschleife. Ein
 *     Auftrag ist nicht fertig, wenn osk_kbd_type() zurueckkommt.
 *   - Jedes Zeichen kostet genau zwei Berichte, unabhaengig davon, welches
 *     es ist. Die Dauer des Tippens verraet also die LAENGE des
 *     Passworts, aber nicht seinen Inhalt. Die Laenge ist ohnehin
 *     sichtbar, der Inhalt nicht - darum ist das so gewollt und nicht
 *     bloss hingenommen.
 * ===================================================================
 *
 * DIE ABBILDUNG. Tastencodes nennen Tastenplaetze, nicht Zeichen: Platz
 * 0x1c ist auf einer englischen Tastatur Y und auf einer deutschen Z [8].
 * Darum gehoert zu einem statischen Passwort immer eine Belegung. Das
 * Yubico-Protokoll schreibt sie mit YK_SLOT_SCAN_MAP (0x12): 45 Byte [6],
 * in einer festen Reihenfolge [7]:
 *
 *   cbdefghijklnrtuv CBDEFGHIJKLNRTUV 0123456789 ! Tab Eingabe
 *
 * 16 Kleinbuchstaben (die Modhex-Zeichen), dieselben 16 gross, zehn
 * Ziffern, Ausrufezeichen, Tabulator, Eingabetaste. Das ergibt 45.
 * Gesetztes Bit 0x80 in einem Code heisst "mit Umschalter".
 *
 * Es wird nichts belegt; die Warteschlange ist ein festes Feld.
 *
 * Belegstellen [6] [7] [8] siehe yk_hid.h.
 *
 * Stufe 2 von 4: geschrieben und uebersetzt. Ob ein Rechner die Berichte
 * als Tastendruecke annimmt, ist nicht gemessen - dafuer braucht es das
 * Geraet.
 *
 * Umsetzung: src/classes/transport/usb_kbd.c */
#ifndef OSK_USB_KBD_H
#define OSK_USB_KBD_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "osk_result.h"

/* Ein Tastaturbericht: Umschalter, ein reserviertes Byte, sechs Tasten [8]. */
#define OSK_KBD_RPT_LEN 8
#define OSK_KBD_MOD_LSHIFT 0x02u

/* Die Abbildung hat 45 Eintraege [6]. */
#define OSK_KBD_SCAN_MAP_LEN 45

/* Die Reihenfolge der 45 Zeichen [7]. Genau einmal im Baum. */
#define OSK_KBD_SCAN_ORDER "cbdefghijklnrtuvCBDEFGHIJKLNRTUV0123456789!\t\r"

/* So viele Zeichen gehen in einen Auftrag. */
#define OSK_KBD_QUEUE_MAX 64

/* Gesetztes Bit heisst: mit Umschalter tippen. */
#define OSK_KBD_SHIFT 0x80u

/* So geht ein fertiger Bericht hinaus. Immer OSK_KBD_RPT_LEN Byte. */
typedef void (*osk_kbd_send_fn)(const uint8_t rpt[OSK_KBD_RPT_LEN], void *ctx);

osk_result_t osk_kbd_init(osk_kbd_send_fn send, void *ctx);

/* Setzt die Abbildung, wie YK_SLOT_SCAN_MAP sie schreibt. */
osk_result_t osk_kbd_set_scan_map(const uint8_t map[OSK_KBD_SCAN_MAP_LEN]);

/* Zurueck auf die Werkseinstellung [7]. */
void osk_kbd_reset_scan_map(void);

/* Liest die Abbildung zurueck - fuer den Lesebefehl und die Pruefungen. */
const uint8_t *osk_kbd_scan_map(void);

/* Ein Zeichen in einen Tastencode. false, wenn es nicht in der
 * Reihenfolge vorkommt. */
bool osk_kbd_scan_code(char c, uint8_t *code);

/* Einen Auftrag einstellen. Entweder geht alles oder nichts.
 *   OSK_ERR_FULL          laenger als OSK_KBD_QUEUE_MAX
 *   OSK_ERR_UNSUPPORTED   ein Zeichen fehlt in der Abbildung
 *   OSK_ERR_EXISTS        es wird noch getippt
 * Nach OSK_OK darf der Aufrufer seinen Klartext loeschen. */
osk_result_t osk_kbd_type(const char *text, size_t len);

/* Dasselbe mit rohen Tastencodes - fuer Zeichen, die in der Abbildung
 * nicht vorkommen, und fuer die Modhex-Ausgabe eines OTP. */
osk_result_t osk_kbd_type_codes(const uint8_t *codes, size_t len);

/* Aus der Hauptschleife: schiebt den naechsten Bericht hinaus.
 * false, wenn nichts mehr zu tippen ist. */
bool osk_kbd_pump(void);

/* Bricht ab und loescht die Warteschlange. */
void osk_kbd_cancel(void);

bool osk_kbd_busy(void);

#endif
