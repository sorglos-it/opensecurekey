/* PIV: der Stick als Dienstausweis.
 *
 * Umsetzung nach NIST SP 800-73-4 (Interfaces for Personal Identity
 * Verification), Teil 1 (Datenobjekte) und Teil 2 (Kommandos der
 * Kartenschnittstelle). Vier Steckplaetze:
 *
 *   9A  Anmeldung am Rechner (PIV Authentication)
 *   9C  Unterschrift (Digital Signature) - verlangt die PIN unmittelbar davor
 *   9D  Entschluesselung und Schluesselaustausch (Key Management)
 *   9E  Kartenauthentisierung (Card Authentication) - ohne PIN
 *
 * Die Schluessel entstehen im SE050 und verlassen ihn nicht; die
 * Zertifikate liegen im Tresor als OSK_ITEM_CERT. Jede Unterschrift und
 * jede Herausgabe fragt am Display nach.
 *
 * Was es NICHT gibt, siehe docs/smartcard.md: RSA, den
 * Verwaltungsschluessel 9B (3DES), Biometrie und sichere
 * Nachrichtenuebermittlung auf der Karte.
 */
#ifndef OSK_PIV_H
#define OSK_PIV_H

#include <stdint.h>
#include <stddef.h>
#include "osk_result.h"
#include "osk_applets.h"

/* Die Kennung der PIV-Anwendung, SP 800-73-4 Teil 1 Abschnitt 2.2:
 * NIST-RID A000000308, Anwendung 00001000, Fassung 0100. */
#define OSK_PIV_AID_LEN 11u
extern const uint8_t osk_piv_aid[OSK_PIV_AID_LEN];

/* Steckplaetze (Teil 1 Tabelle 4b). Dazu F9, der Attestierungsschluessel -
 * der steht in keiner Tabelle der Norm, siehe osk_attest.h. */
#define OSK_PIV_SLOT_AUTH 0x9au
#define OSK_PIV_SLOT_SIGN 0x9cu
#define OSK_PIV_SLOT_KEYMGMT 0x9du
#define OSK_PIV_SLOT_CARD 0x9eu

/* Kennungen der Algorithmen (Teil 1 Tabelle 6-2). */
#define OSK_PIV_ALG_P256 0x11u
#define OSK_PIV_ALG_P384 0x14u

/* RSA. Teil 1 Tabelle 6-2 nennt 0x07 fuer "RSA 2048 bit modulus, 65537
 * exponent". Mehr nicht:
 *
 * RSA-3072 und RSA-4096 haben in SP 800-73-4 KEINE Algorithmuskennung.
 * Der Chip kann die Laengen (osk_se050.h, osk_rsa_bits_t), PIV in der
 * Fassung, gegen die hier gebaut wird, kann sie nicht nennen. Die
 * Kennungen dafuer stehen als `offen` - sie werden nicht geraten, weil
 * eine falsche Kennung ein Zertifikat erzeugt, das niemand pruefen kann,
 * und der Fehler erst beim Gegenueber auffaellt.
 *
 * OpenPGP ist davon nicht betroffen: dort steht die Bitlaenge als Zahl im
 * Datenobjekt (Spezifikation 3.4 Abschnitt 4.4.3.9), nicht in einer
 * Kennung. Darum kann OpenPGP hier alle drei Laengen und PIV nur eine -
 * das ist kein Versehen, sondern der Unterschied zwischen den beiden
 * Normen. */
#define OSK_PIV_ALG_RSA2048 0x07u

/* Woher die Schluessel im Chip ihre Kennung bekommen: 'PI' und der
 * Steckplatz. So kollidiert nichts mit CTAP oder OpenPGP. */
#define OSK_PIV_KEY_ID(slot) (0x50490000u | (uint32_t)(slot))

const osk_applet_t *osk_piv_applet(void);

#endif
