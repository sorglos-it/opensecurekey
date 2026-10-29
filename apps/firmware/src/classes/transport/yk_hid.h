/* Das Yubico-HID-Report-Protokoll, Geraeteseite.
 *
 * Darueber reden fremde Programme mit einem Schluessel: Yubico
 * Authenticator, ykman, und die aelteren Challenge-Response-Anwendungen,
 * mit denen KeePassXC und VeraCrypt einen Tresor aufmachen. Es ist NICHT
 * die CTAP-Schnittstelle; es ist eine eigene HID-Schnittstelle mit
 * eigenen Berichten.
 *
 * ========================= ANGRIFFSFLAECHE =========================
 * Jede Software auf dem Rechner darf HID-Feature-Berichte an dieses
 * Geraet schicken - ohne Rechte, ohne Nachfrage, ohne Nutzer davor. Das
 * hier ist also der Parser mit der kuerzesten Strecke zu einem
 * Angreifer, den das Geraet hat. Darum gelten drei Regeln ohne Ausnahme:
 *   1. Es wird nichts belegt. Alle Puffer sind feste Felder im
 *      statischen Zustand. Grenzen werden VOR dem Kopieren nachgerechnet.
 *   2. Vergleiche ueber Geheimnisse laufen gleich lang (yk_ct.h). Die
 *      Fehlerklasse hat 2024 als EUCLEAK jeden YubiKey 5 vor Fassung 5.7
 *      klonbar gemacht.
 *   3. Der Parser wird gefuzzt, nicht nur mit gueltigen Berichten
 *      gefuettert: test/transport/fuzz_ykhid.c.
 * ===================================================================
 *
 * ANDERS ALS CTAPHID: Dort schiebt das Geraet Pakete ueber einen
 * Interrupt-Endpunkt hinaus. Hier holt der HOST sie mit GET_REPORT ab
 * und schreibt mit SET_REPORT herein. Darum gibt es hier keinen
 * Sendehaken, sondern yk_hid_get_report() - der Host zieht.
 *
 * ERWEITERTE LAENGE BRAUCHT ES HIER NICHT. Strang A hat in der
 * SE050-Schicht erweiterte APDU-Laengen eingebaut, weil ein
 * RSA-4096-Chiffrat 512 Byte lang ist. Dieses Protokoll kann das gar
 * nicht: die Nutzlast eines Rahmens ist auf 64 Byte festgenagelt, eine
 * Antwort auf 217 (yk_frame.h). Was laenger ist, geht ueber CCID, nicht
 * hier durch. Damit entfaellt auch die Stapelgefahr von dort.
 *
 * STAPELBEDARF: Der Zustand liegt statisch (Zaehlung am Ende von
 * yk_hid.c). Auf dem Stapel liegt je Aufruf nur ein 8-Byte-Bericht und
 * ein paar Zaehler.
 *
 * =========================== BELEGSTELLEN ===========================
 * Nachgebaut aus der Beschreibung, nicht aus fremdem Code. Was nicht
 * belegt war, steht als offen und ist nicht geraten.
 *
 * [1] YubiKey-Handbuch (Yubico): ein Feature-Bericht hat 8 nutzbare
 *     Bytes, das letzte traegt die Folgenummer; ein vollstaendiger
 *     Einstellungsrahmen braucht 10 Berichte = 70 Byte.
 * [2] yubikey-manager, yubikit/core/otp (Zahlenwerte, gelesen als
 *     Beleg): Bericht 8, Nutzlast 64, Rahmen 70; Schreibschalter 0x80,
 *     Antwort-steht-an 0x40, Wartezustand 0x20, Nummernmaske 0x1f,
 *     Leerbericht 0x8f; Pruefsumme ueber die Nutzlast allein, auf
 *     Rahmenbyte 65/66, niederwertiges Byte zuerst; drei Fuellbytes;
 *     im Statusbericht an Berichtsbyte 4 der Programmzaehler und an
 *     Byte 5 der Tastgrad; Sparregel fuer Nullstuecke; Abschluss durch
 *     Nummer 0.
 * [3] YubiKey-Handbuch und python-yubico: Pruefsumme ist der CRC-16 nach
 *     ISO 13239, umgedrehtes Polynom 0x8408, Startwert 0xffff,
 *     uebertragen als Einerkomplement, niederwertiges Byte zuerst,
 *     Restwert 0xf0b8 ueber einen zusammenhaengenden Puffer.
 * [4] Pruefwerte zu [3] (YubiOTP-Doku und Yubico-Forum):
 *     8792ebfe26cc130030c20011c89f ergibt uebertragen 0xc823;
 *     8792ebfe26cc130030c20011c89f23c8 ergibt den Restwert 0xf0b8;
 *     55aa00ff ergibt 0xcbb5. Alle drei werden in test_ykcrc.c
 *     nachgerechnet.
 * [5] python-yubico, yubikey_defs (Steckplatznummern, gelesen als Beleg).
 * [6] Yubico YubiKey SDK, Update scan-code map: CLA 00 INS 01 P1 12
 *     P2 00 Lc 2d - die Abbildung ist 45 Byte lang.
 * [7] ykpersonalize(1): die 45 Bytes stehen in einer festen Reihenfolge
 *     (siehe usb_kbd.h), und die Werkseinstellung ist die Folge in
 *     usb_kbd.c.
 * [8] Yubico SDK, YubiKey-host device communication: ein Tastaturbericht
 *     ist 8 Byte - Umschalter, ein reserviertes Byte, sechs Tasten;
 *     Umschalter links ist 0x02.
 *
 * OFFEN - nicht geraten, nicht gebaut:
 *   - Der Aufbau der 64 Nutzlastbytes eines Schreibbefehls (wo
 *     Zugangscode, Kennungen und Schalter liegen). Die gefundenen
 *     Quellen widersprechen sich in den Abstaenden; das Handbuch nennt
 *     eine Tabelle, die einen Unterblock beschreibt, nicht die ganze
 *     Struktur. Diese Schicht gibt darum 64 rohe Bytes weiter.
 *   - Der genaue Aufbau des Tastgrad-Feldes und ob Berichtsbyte 5 in
 *     neueren Fassungen den Tastgrad oder die belegten Steckplaetze
 *     meint. [2] benutzt dasselbe Byte fuer beides. Zwei Groessen, ein
 *     Name - darum traegt yk_hid_status_t beide Felder getrennt und der
 *     Aufrufer entscheidet, was hineingehoert.
 *   - Alles Verhalten am echten Geraet. Dieser Strang steht auf Stufe 2
 *     von 4: geschrieben und uebersetzt. Kein Abbild, kein Geraet.
 * ===================================================================
 *
 * Umsetzung: src/classes/transport/yk_hid.c */
#ifndef OSK_YK_HID_H
#define OSK_YK_HID_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "osk_result.h"
#include "yk_frame.h"

/* Steckplatz- und Befehlsnummern [5]. Sie stehen im Rahmenbyte 64. */
#define YK_SLOT_CONFIG ((uint8_t)0x01u)
#define YK_SLOT_CONFIG2 ((uint8_t)0x03u)
#define YK_SLOT_UPDATE1 ((uint8_t)0x04u)
#define YK_SLOT_UPDATE2 ((uint8_t)0x05u)
#define YK_SLOT_SWAP ((uint8_t)0x06u)
#define YK_SLOT_NDEF ((uint8_t)0x08u)
#define YK_SLOT_NDEF2 ((uint8_t)0x09u)
#define YK_SLOT_DEVICE_SERIAL ((uint8_t)0x10u)
#define YK_SLOT_DEVICE_CONFIG ((uint8_t)0x11u)
#define YK_SLOT_SCAN_MAP ((uint8_t)0x12u)
#define YK_SLOT_YK4_CAPABILITIES ((uint8_t)0x13u)
#define YK_SLOT_CHAL_OTP1 ((uint8_t)0x20u)
#define YK_SLOT_CHAL_OTP2 ((uint8_t)0x28u)
#define YK_SLOT_CHAL_HMAC1 ((uint8_t)0x30u)
#define YK_SLOT_CHAL_HMAC2 ((uint8_t)0x38u)

/* Ein angefangener Rahmen laeuft nach dieser Zeit ab, damit ein
 * Angreifer den Zusammenbau nicht auf Dauer offen haelt. */
#define YK_HID_FRAME_TIMEOUT_MS 500u

/* So lange wird auf den Taster gewartet. Die Restsekunden gehen in die
 * unteren 5 Bit des Statusbytes, mehr als 31 passt dort nicht. */
#define YK_HID_TOUCH_TIMEOUT_MS 15000u

/* Der Statusbericht. Berichtsbyte 0 bleibt null, 1..3 ist die Fassung,
 * 4 der Programmzaehler, 5..6 der Tastgrad, 7 ist null [2]. */
typedef struct {
	uint8_t version[3];	  /* Haupt-, Neben-, Baunummer */
	uint8_t prog_seq;	  /* steigt mit jedem geschriebenen Platz */
	uint16_t touch_level;	  /* Tastgrad */
	uint8_t slots_programmed; /* Bit 0 Platz 1, Bit 1 Platz 2 */
} yk_hid_status_t;

/* Der Bearbeiter eines Befehls. Bekommt die Steckplatznummer und die 64
 * rohen Nutzlastbytes; legt die Antwort in out ab.
 *
 * Rueckgabe:
 *   OSK_OK          Antwort steht in out, *out_len Byte.
 *   OSK_ERR_DENIED  Taster wird gebraucht. Der Transport geht in den
 *                   Wartezustand; der Bearbeiter wird nach
 *                   yk_hid_touch_done(true) erneut gerufen.
 *   alles andere    kein Datensatz; der Host bekommt den Statusbericht.
 *
 * Nichts davon darf belegen oder blockieren. */
typedef osk_result_t (*yk_hid_slot_fn)(uint8_t slot, const uint8_t payload[YK_SLOT_DATA_LEN],
				       uint8_t *out, size_t cap, size_t *out_len, void *ctx);

osk_result_t yk_hid_init(yk_hid_slot_fn handler, void *ctx, const yk_hid_status_t *status);

/* Der Host hat einen Feature-Bericht geschrieben (SET_REPORT). */
osk_result_t yk_hid_set_report(const uint8_t rpt[YK_RPT_LEN]);

/* Der Host holt einen Feature-Bericht (GET_REPORT). Es wird immer ein
 * vollstaendiger Bericht geliefert - notfalls der Statusbericht. */
void yk_hid_get_report(uint8_t rpt[YK_RPT_LEN]);

/* Von der Hauptschleife aufzurufen: laesst angefangene Rahmen und das
 * Warten auf den Taster ablaufen. now_ms ist eine steigende Uhr in
 * Millisekunden; der Transport haelt keine eigene. */
void yk_hid_tick(uint32_t now_ms);

/* Der Taster wurde gedrueckt (true) oder die Freigabe wurde verweigert. */
void yk_hid_touch_done(bool granted);

/* Statusbericht nachfuehren - der Host liest daran ab, ob ein Schreiben
 * angekommen ist. */
void yk_hid_set_status(const yk_hid_status_t *status);

/* Nur fuer die Pruefungen: wie viele Rahmen angenommen und wie viele
 * abgewiesen wurden. Ein Fuzzlauf ohne angenommene Rahmen hat nur die
 * erste Pruefung getroffen. */
typedef struct {
	uint32_t frames_ok;
	uint32_t frames_bad;
	uint32_t resets;
	uint32_t aborts;
	uint32_t timeouts;
} yk_hid_counts_t;

void yk_hid_counts(yk_hid_counts_t *out);

#endif
