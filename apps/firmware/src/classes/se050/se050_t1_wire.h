/* Alle Zahlenwerte des T=1'-Drahtprotokolls - und nur diese - stehen hier.
 *
 * WARUM EINE EIGENE DATEI: Die Rahmenbeschreibung von NXP (UM11225
 * "SE05x T=1 Over I2C Specification") ist als "COMPANY PUBLIC" eingestuft und
 * nur nach Anmeldung bei NXP herunterzuladen. Was oeffentlich belegt ist,
 * steht unten unter BELEGT. Der Rest ist aus ISO/IEC 7816-3 T=1 abgeleitet,
 * von dem T=1' seinem Namen nach stammt, und ausdruecklich UNBESTAETIGT.
 *
 * Liegt UM11225 vor, ist NUR diese Datei gegenzulesen. Der uebrige Treiber
 * benutzt ausschliesslich die Namen von hier und bleibt unberuehrt.
 *
 * BELEGT (Quellen mit Nummer und Fassung in docs/se050.md):
 *   - Rahmen = Prolog (NAD, PCB, LEN) + INF (0..LEN Byte) + Epilog (2-Byte-CRC16)
 *   - ein LEN-Byte deckt 0x00..0xFE ab
 *   - Pruefsumme nach ISO/IEC 7816-3, also CRC-16/X-25
 *   - PCB unterscheidet I-Block, R-Block und S-Block
 *   - S-Block: PCB-Bits b8 und b7 sind 11
 *   - Vorgabe-Busadresse des SE050 ist 0x48
 *   - "End of APDU session request" als S-Block schickt den Baustein in den
 *     Stromsparzustand (Datenblatt)
 *   - ein Rahmen ohne Datenbyte laesst den Baustein haengen (Erratasheet I2C.2)
 *   - zwei Befehle ohne Abholen der Antwort lassen ihn haengen (I2C.1)
 *
 * UNBESTAETIGT, aus ISO/IEC 7816-3 T=1 abgeleitet - bei Fehlverhalten am
 * Geraet ZUERST hier nachsehen:
 *   - die genauen Bitlagen in PCB fuer Folgenummer, Kettenbit und Fehlergrund
 *   - die Zahlenwerte der S-Block-Codes
 *   - die NAD-Bytes fuer Hin- und Rueckrichtung
 *   - die Byte-Reihenfolge der CRC im Epilog
 *   - alle Zeitgrenzen und die Zahl der Wiederholungen
 */
#ifndef SE050_WIRE_H
#define SE050_WIRE_H

#include <stdint.h>

/* ---------------------------------------------------------------- BELEGT */

/* Vorgabeadresse auf dem Bus (7 Bit). */
#define SE050_I2C_ADDR          0x48u

/* Prolog: NAD, PCB, LEN. Epilog: zwei CRC-Bytes. */
#define SE050_T1_PROLOGUE_LEN   3u
#define SE050_T1_EPILOGUE_LEN   2u
#define SE050_T1_OVERHEAD       (SE050_T1_PROLOGUE_LEN + SE050_T1_EPILOGUE_LEN)

/* Groesste INF-Laenge, die ein LEN-Byte ausdruecken kann. */
#define SE050_T1_MAX_INF        0xFEu

/* S-Block: b8 und b7 gesetzt. */
#define SE050_PCB_TYPE_MASK     0xC0u
#define SE050_PCB_TYPE_S        0xC0u

/* ---------------------------------------------------- UNBESTAETIGT (ISO) */

/* I-Block: b8 = 0. R-Block: b8b7 = 10. */
#define SE050_PCB_TYPE_I        0x00u
#define SE050_PCB_TYPE_R        0x80u

/* Blockart erkennen. Das MUSS ueber diese drei Pruefungen laufen und nicht
 * ueber einen Vergleich gegen SE050_PCB_TYPE_MASK: beim I-Block ist nur b8
 * die Art, b7 traegt die Folgenummer. Ein I-Block mit Folgenummer 1 wird
 * sonst fuer einen R-Block gehalten - und genau das ist beim Bau dieses
 * Treibers passiert. Der zweite Block einer Antwortkette kam nie an. */
#define SE050_PCB_IS_I(pcb)     (((pcb) & 0x80u) == 0x00u)
#define SE050_PCB_IS_R(pcb)     (((pcb) & 0xC0u) == 0x80u)
#define SE050_PCB_IS_S(pcb)     (((pcb) & 0xC0u) == 0xC0u)

/* I-Block: b7 traegt die Folgenummer N(S), b6 das Kettenbit M.
 * M = 1 heisst "es folgt noch mindestens ein Block". */
#define SE050_PCB_I_SEQ         0x40u
#define SE050_PCB_I_MORE        0x20u

/* R-Block: b5 traegt die erwartete Folgenummer N(R),
 * b1..b2 den Grund der Ablehnung. */
#define SE050_PCB_R_SEQ         0x10u
#define SE050_PCB_R_ERR_MASK    0x03u
#define SE050_PCB_R_ERR_NONE    0x00u   /* nur Quittung, bitte weitersenden */
#define SE050_PCB_R_ERR_CRC     0x01u   /* Pruefsumme stimmte nicht */
#define SE050_PCB_R_ERR_OTHER   0x02u   /* anderer Fehler */

/* S-Block: b6 = 0 Anfrage, b6 = 1 Antwort; b5..b1 ist der Code. */
#define SE050_PCB_S_RESPONSE    0x20u
#define SE050_PCB_S_CODE_MASK   0x1Fu

/* S-Block-Codes. Reihenfolge wie in ISO/IEC 7816-3 (RESYNCH, IFS, ABORT,
 * WTX) mit den geraetenahen Codes von T=1' dahinter. */
#define SE050_S_RESYNC          0x00u   /* Zaehler beidseitig zuruecksetzen  */
#define SE050_S_IFS             0x01u   /* groesste INF-Laenge mitteilen     */
#define SE050_S_ABORT           0x02u   /* laufende Kette abbrechen          */
#define SE050_S_WTX             0x03u   /* Baustein bittet um mehr Zeit      */
#define SE050_S_END_APDU        0x05u   /* Sitzung beenden, Stromsparen      */
#define SE050_S_CHIP_RESET      0x06u   /* Baustein neu starten              */
#define SE050_S_GET_ATR         0x07u   /* Kennblock des Bausteins lesen     */
#define SE050_S_IF_SOFT_RESET   0x0Fu   /* Schnittstelle zuruecksetzen       */

/* NAD: Hinweg Host -> Baustein, Rueckweg Baustein -> Host. */
#define SE050_NAD_TO_SE         0x5Au
#define SE050_NAD_TO_HOST       0xA5u

/* Byte-Reihenfolge der CRC im Epilog: niederwertiges Byte zuerst. */
#define SE050_CRC_LSB_FIRST     1

/* Zeitgrenzen in Millisekunden. Reichlich bemessen: zu kurz heisst
 * Abbruch mitten in einer Antwort, und das ist der Zustand, der den
 * Baustein nach Erratasheet I2C.1 haengen laesst. */
#define SE050_T_ANSWER_MS       1000u   /* bis der erste Prolog kommt        */
#define SE050_T_WTX_STEP_MS     1000u   /* ein Schritt der WTX-Verlaengerung */
#define SE050_T_WTX_MAX_MS      30000u  /* Obergrenze aller Verlaengerungen  */
#define SE050_T_POLL_MS         1u      /* Pause zwischen zwei Leseversuchen */

/* Wie oft ein Rahmen nach einer Ablehnung erneut gesendet wird, bevor der
 * Treiber aufgibt. ISO/IEC 7816-3 nennt drei Versuche. */
#define SE050_MAX_RETRY         3u

/* Hoechste INF-Laenge, die der Treiber selbst benutzt. Gleich der oberen
 * Grenze des LEN-Bytes; wird per S-IFS vom Baustein nach unten korrigiert. */
#define SE050_IFSD_DEFAULT      SE050_T1_MAX_INF

#endif
