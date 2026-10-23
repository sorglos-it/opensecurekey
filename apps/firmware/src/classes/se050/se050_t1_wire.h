/* Alle Zahlenwerte des T=1'-Drahtprotokolls - und nur diese - stehen hier.
 *
 * QUELLE: GlobalPlatform Technology, "Next Gen APDU Transport",
 * GPC_SPE_172, Fassung 1.0.0.34 (Public Review, April 2025), Abschnitt 4.
 * Frei herunterladbar und frei weitergebbar - jeder kann diese Schicht
 * gegen die Beschreibung nachlesen, und das ist bei einem
 * Sicherheitsschluessel keine Nebensache.
 *
 * WARUM NICHT UM11225: Der SE051 versteht ZWEI T=1-Protokolle und erkennt
 * am ersten Prolog selbst, welches kommt - NXPs UM11225 oder dieses hier
 * (SE051-Datenblatt Rev. 1.4, Abschnitt 1 und 2.1). UM11225 liegt hinter
 * einem NXP-Konto; diese Beschreibung nicht. Darum diese.
 *
 * ZWEI DINGE SIND NOCH OFFEN, beide in docs/se050.md:
 * 1. Fassung: Das SE051-Datenblatt nennt "APDU Transport over SPI/I2C
 *    v1.0" (Januar 2020), vorliegend ist 1.0.0.34 (April 2025, Public
 *    Review). Die Rahmenwerte unten sollten gleich sein - belegt ist das
 *    nicht. Stellen, an denen die aeltere Fassung abweichen koennte, sind
 *    mit OFFEN markiert.
 * 2. Die automatische Erkennung ist eine Zusage auf Papier. Am Geraet ist
 *    sie die allererste Messung: greift sie nicht, ist dieser Weg
 *    hinfaellig und es braucht doch UM11225.
 *
 * Der uebrige Treiber benutzt ausschliesslich die Namen von hier. Stellt
 * sich am Chip heraus, dass doch UM11225 gilt, kostet der Rueckweg diese
 * eine Datei - nicht einen Umbau. */
#ifndef SE050_T1_WIRE_H
#define SE050_T1_WIRE_H

#include <stdint.h>

/* Vorgabeadresse des SE050/SE051 auf dem Bus, 7 Bit.
 * Quelle: SE050-Datenblatt Rev. 3.8, Abschnitt 4. */
#define SE050_I2C_ADDR 0x48u

/* ------------------------------------------------------------- Rahmen */

/* Tabelle 4-1: NAD (1) | PCB (1) | LEN (2) | INF (LEN) | CRC (2).
 * Der Prolog ist VIER Byte, nicht drei - das LEN-Feld ist bei T=1'
 * zweibytig. Das ist der einzige echte Formatunterschied zu UM11225. */
#define SE050_T1_PROLOGUE_LEN 4u
#define SE050_T1_EPILOGUE_LEN 2u
#define SE050_T1_OVERHEAD (SE050_T1_PROLOGUE_LEN + SE050_T1_EPILOGUE_LEN)

/* "The LEN and CRC fields shall have their Most Significant Byte sent
 * first (i.e. big-endian order)" - Abschnitt 4.2.
 *
 * Das war in der geratenen Fassung falsch herum. Die Tests lesen diesen
 * Schalter, statt eine Reihenfolge anzunehmen - darum zog das Berichtigen
 * sie mit, statt sie zu brechen. */
#define SE050_CRC_LSB_FIRST 0

/* Groesste INF-Laenge, die das Protokoll ueberhaupt zulaesst (Abschnitt
 * 4.2.3 und 4.2.5): darueber schuetzt die 2-Byte-Pruefsumme nicht mehr
 * zuverlaessig. Das ist die Protokollgrenze, nicht unsere. */
#define SE050_T1_MAX_INF 4089u

/* Was WIR annehmen koennen. Ein Puffer ueber 4 kB je Richtung waere hier
 * masslos: der APDU-Puffer des Bausteins ist ohnehin nur 894 Byte
 * (AN12413 Tabelle 16). Diesen Wert teilt der Treiber dem Baustein per
 * S(IFS request) mit; alles Laengere lehnt er ab, wie es Abschnitt 4.2.3
 * verlangt ("Values exceeding the current IFS value of the receiver"). */
#define SE050_IFSD 1024u

/* Was der Baustein annehmen kann, solange wir es nicht besser wissen.
 * "A default IFSC value of 8 is defined and shall be used by the CTLR if
 * it doesn't know IFSC" - Abschnitt 4.1. Acht Byte reichen genau fuer die
 * S(CIP request), mit der wir den richtigen Wert erfragen. */
#define SE050_IFSC_DEFAULT 8u

/* --------------------------------------------------------------- PCB */

/* Tabelle 4-4. Blockart erkennen: das MUSS ueber diese drei Pruefungen
 * laufen. Beim I-Block ist nur b8 die Art, b7 traegt die Folgenummer - ein
 * I-Block mit Folgenummer 1 wird sonst fuer einen R-Block gehalten, und
 * genau das ist beim Bau dieses Treibers einmal passiert. */
#define SE050_PCB_IS_I(pcb) (((pcb) & 0x80u) == 0x00u)
#define SE050_PCB_IS_R(pcb) (((pcb) & 0xC0u) == 0x80u)
#define SE050_PCB_IS_S(pcb) (((pcb) & 0xC0u) == 0xC0u)

#define SE050_PCB_TYPE_I 0x00u
#define SE050_PCB_TYPE_R 0x80u
#define SE050_PCB_TYPE_S 0xC0u

/* I-Block: b7 ist die Folgenummer N(S), b6 das Kettenbit M.
 * M = 1 heisst "es folgt noch mindestens ein Block". */
#define SE050_PCB_I_SEQ 0x40u
#define SE050_PCB_I_MORE 0x20u

/* R-Block: b5 ist die erwartete Folgenummer N(R), b4..b1 der Grund.
 * Das Feld ist vier Bit breit, auch wenn nur drei Werte vergeben sind. */
#define SE050_PCB_R_SEQ 0x10u
#define SE050_PCB_R_ERR_MASK 0x0Fu
#define SE050_PCB_R_ERR_NONE 0x00u  /* fehlerfreie Quittung             */
#define SE050_PCB_R_ERR_CRC 0x01u   /* Pruefsumme stimmte nicht         */
#define SE050_PCB_R_ERR_OTHER 0x02u /* anderer Fehler                   */

/* S-Block: b6 = 0 Anfrage, b6 = 1 Antwort; b5..b1 ist der Code. */
#define SE050_PCB_S_RESPONSE 0x20u
#define SE050_PCB_S_CODE_MASK 0x1Fu

/* S-Block-Codes, Tabelle 4-4. Fuenf davon standen in der geratenen
 * Fassung schon richtig; CIP, RELEASE und SWR waren falsch oder fehlten. */
#define SE050_S_RESYNC 0x00u  /* Zaehler beidseitig zuruecksetzen       */
#define SE050_S_IFS 0x01u     /* groesste INF-Laenge mitteilen          */
#define SE050_S_ABORT 0x02u   /* laufende Kette abbrechen               */
#define SE050_S_WTX 0x03u     /* Baustein bittet um mehr Zeit           */
#define SE050_S_CIP 0x04u     /* Schnittstellenwerte erfragen           */
#define SE050_S_RELEASE 0x06u /* Baustein darf stromsparen              */
#define SE050_S_SWR 0x0Fu     /* Schnittstelle zuruecksetzen            */

/* Der SE050 nennt den Stromspar-Block "End of APDU session request"; das
 * SE051-Datenblatt sagt ausdruecklich, das ist derselbe wie RELEASE aus
 * GP T=1oI2C. Darum hier nur ein Name. */
#define SE050_S_END_APDU SE050_S_RELEASE

/* --------------------------------------------------------------- NAD */

/* Abschnitt 4.2.1: b8 und b4 zusammen geben die Richtung an, b7-b6-b5
 * sind DAD und b3-b2-b1 sind SAD. Fuer Teilnehmer ohne logische
 * Verbindungen empfiehlt die Beschreibung genau diese zwei Werte. */
#define SE050_NAD_TO_SE 0x29u	/* DAD=010, SAD=001 */
#define SE050_NAD_TO_HOST 0x92u /* DAD=001, SAD=010 */

/* ------------------------------------------------------------- Zeiten */

/* "If the CIP has never been retrieved and the CTLR is not aware of the
 * BWT that may be used, the CTLR shall use a default BWT value (DBWT) of
 * 300ms" - Abschnitt 4.3.2. Der richtige Wert kommt aus der CIP. */
#define SE050_BWT_DEFAULT_MS 300u

/* Obergrenze aller WTX-Verlaengerungen zusammen. OFFEN: die Beschreibung
 * nennt keine; das ist unsere Vorsichtsgrenze, damit ein Baustein, der
 * immer weiter um Zeit bittet, das Geraet nicht endlos haengen laesst. */
#define SE050_T_WTX_MAX_MS 30000u

/* Pause zwischen zwei Leseversuchen. Unsere Wahl, nicht aus der
 * Beschreibung. */
#define SE050_T_POLL_MS 1u

/* Wie oft ein Rahmen nach einer Ablehnung erneut gesendet wird.
 * ISO/IEC 7816-3 Abschnitt 11.6.3 nennt drei Versuche, und GP erklaert
 * diese Fehlerbehandlung fuer weiter gueltig (Abschnitt 4.1). */
#define SE050_MAX_RETRY 3u

#endif
