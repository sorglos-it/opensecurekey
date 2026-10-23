/* SCP03: der verschluesselte Kanal zwischen nRF52840 und SE050.
 *
 * Pflicht, kein Zusatz. Wer die zwei I2C-Leitungen auf der Platine
 * anzapft, sieht damit nur Chiffrat: jede Befehlsnutzlast ist mit AES-CBC
 * verschluesselt, jeder Befehl und jede Antwort traegt eine Pruefsumme, und
 * die Pruefsummen sind untereinander verkettet - ein eingeschobener,
 * weggelassener oder wiederholter Befehl fliegt auf.
 *
 * Quellen (Nummer und Fassung in docs/se050.md):
 *   GlobalPlatform Secure Channel Protocol 03, Amendment D - Ableitung
 *     (4.1.5), Sitzungsschluessel und Kryptogramme (6.2.1, 6.2.2),
 *     C-MAC (6.2.4), R-MAC (6.2.5), Verschluesselung (6.2.6, 6.2.7)
 *   GlobalPlatform Card Specification, Anhang D.4 - die beiden
 *     Anmeldebefehle INITIALIZE UPDATE und EXTERNAL AUTHENTICATE
 *   AN12413 Abschnitt 4.5.3 - der SE050 verlangt Schluesselfassung 00
 *
 * Diese Schicht rechnet, sie sendet nicht. Wer die beiden Anmeldebefehle
 * ueber den Draht schickt, ist se050_apdu.c. */
#ifndef SE050_SCP03_H
#define SE050_SCP03_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"
#include "se050_crypto.h"

#define SCP03_CHALLENGE_LEN  8u
#define SCP03_CRYPTOGRAM_LEN 8u
#define SCP03_MAC_LEN        8u
#define SCP03_CHAIN_LEN      16u

/* Sicherheitsstufe im P1 von EXTERNAL AUTHENTICATE (Amendment D 7.1.2.1).
 * Wir nehmen immer die hoechste: alles verschluesselt, alles gesichert.
 * Eine niedrigere Stufe steht hier absichtlich nicht - was mitlesbar ist,
 * waere eine Einstellung, die jemand versehentlich falsch setzt. */
#define SCP03_SEC_LEVEL_FULL 0x33u

/* Die drei festen Schluessel eines Schluesselsatzes. Sie liegen NICHT im
 * Flash des nRF52840: der Tresor-Strang uebergibt sie beim Aufbau und
 * loescht sie danach. Key-DEK braucht dieser Treiber nicht. */
typedef struct {
	uint8_t enc[OSK_AES_KEY_LEN];
	uint8_t mac[OSK_AES_KEY_LEN];
} osk_scp03_static_keys_t;

typedef struct {
	uint8_t s_enc[OSK_AES_KEY_LEN];
	uint8_t s_mac[OSK_AES_KEY_LEN];
	uint8_t s_rmac[OSK_AES_KEY_LEN];

	/* Verkettungswert: 16 Byte Null bis einschliesslich EXTERNAL
	 * AUTHENTICATE, danach immer der letzte volle C-MAC. */
	uint8_t chain[SCP03_CHAIN_LEN];

	/* Befehlszaehler, 16 Byte, hoechstwertiges Byte vorn. Beginnt bei 1
	 * fuer den ersten Befehl nach erfolgreicher Anmeldung. */
	uint8_t counter[OSK_AES_BLOCK];

	bool established;
} osk_scp03_t;

/* ---------------------------------------------------------- Kanalaufbau */

/* Schritt 1: Sitzungsschluessel und die beiden Kryptogramme rechnen.
 *
 * Das Kryptogramm des Bausteins wird hier NICHT geprueft - nur gerechnet.
 * Der Vergleich gehoert dem Aufrufer, weil nur er weiss, was der Baustein
 * geschickt hat. Er muss osk_memeq_ct() benutzen.
 *
 * Nach diesem Aufruf steht der Verkettungswert auf 16 Byte Null, wie es
 * Amendment D 6.2.3 fuer EXTERNAL AUTHENTICATE verlangt. */
osk_result_t osk_scp03_derive(osk_scp03_t *s,
			      const osk_scp03_static_keys_t *keys,
			      const uint8_t host_challenge[SCP03_CHALLENGE_LEN],
			      const uint8_t card_challenge[SCP03_CHALLENGE_LEN],
			      uint8_t card_cryptogram_out[SCP03_CRYPTOGRAM_LEN],
			      uint8_t host_cryptogram_out[SCP03_CRYPTOGRAM_LEN]);

/* Schritt 2: EXTERNAL AUTHENTICATE bauen - 8 Byte Kryptogramm des Wirts
 * plus 8 Byte C-MAC, mit dem Verkettungswert 16 Byte Null.
 * Die Nutzlast ist nicht verschluesselt (Amendment D 6.2.3).
 *
 * Danach steht der Verkettungswert auf dem vollen C-MAC dieses Befehls und
 * der Zaehler auf 1; established wird gesetzt. */
osk_result_t osk_scp03_build_external_auth(osk_scp03_t *s,
					   const uint8_t host_cryptogram[SCP03_CRYPTOGRAM_LEN],
					   uint8_t *apdu, size_t cap,
					   size_t *len_out);

/* Kanal vergessen und alle Schluessel aus dem Speicher loeschen. */
void osk_scp03_close(osk_scp03_t *s);

/* --------------------------------------------------------- Im Betrieb */

/* Einen fertigen Klartext-C-APDU (Kopf 4 Byte, Lc, Daten, kein Le) in den
 * gesicherten Befehl verwandeln: Nutzlast auffuellen und verschluesseln,
 * CLA auf 0x84 setzen, Lc anpassen, C-MAC anhaengen, Le anfuegen.
 *
 * Ein Befehl ohne Daten wird nur gesichert, nicht verschluesselt - der
 * Zaehler laeuft trotzdem weiter (Amendment D 6.2.6).
 *
 * out darf nicht auf in zeigen. Platzbedarf: Kopf 5 + Nutzlast auf das
 * naechste Vielfache von 16 aufgerundet + 8 Byte MAC + 1 Byte Le. */
osk_result_t osk_scp03_wrap(osk_scp03_t *s,
			    const uint8_t *in, size_t in_len,
			    uint8_t *out, size_t cap, size_t *len_out);

/* Eine gesicherte Antwort auspacken: R-MAC pruefen, Nutzlast entschluesseln,
 * Auffuellung entfernen. in enthaelt Nutzlast, R-MAC und die zwei
 * Statusbytes; out bekommt die Klartext-Nutzlast, sw_out den Status.
 *
 * Eine Antwort mit Fehlerstatus traegt nach Amendment D 6.2.5 weder MAC
 * noch Verschluesselung: dann sind es genau zwei Bytes, out bleibt leer und
 * der Status wird durchgereicht.
 *
 * OSK_ERR_DENIED, wenn der R-MAC nicht stimmt. Dann ist die Antwort
 * gefaelscht oder vertauscht - der Aufrufer muss den Kanal abreissen, nicht
 * weitermachen. */
osk_result_t osk_scp03_unwrap(osk_scp03_t *s,
			      const uint8_t *in, size_t in_len,
			      uint8_t *out, size_t cap, size_t *len_out,
			      uint16_t *sw_out);

/* ------------------------------------------------------------- Pruefhaken */

/* Die "fixed input data" der Ableitung (Amendment D 4.1.5) als Bytefolge.
 *
 * Steht hier, damit der Aufbau gegen ein veroeffentlichtes Beispiel
 * geprueft werden kann: AN12413 Abschnitt 3.2 nennt fuer die
 * Schluesselableitung ohne Kontext ausdruecklich die Eingaben
 *   S-ENC:  00000000000000000000000400008001
 *   S-MAC:  00000000000000000000000600008001
 *   S-RMAC: 00000000000000000000000700008001
 * Genau diese drei Werte prueft test_scp03.c nach. Ohne diesen Haken
 * waere die Reihenfolge der Felder nur gegen sich selbst geprueft - und
 * ein vertauschtes Feld faellt in einem Rundlauf nie auf.
 *
 * out muss 32 Byte haben. Die Konstanten sind die aus Amendment D
 * Tabelle 4-1 (0x00 bis 0x07). */
osk_result_t osk_scp03_derivation_data(uint8_t out[32], size_t *out_len,
				       uint8_t constant, uint16_t l_bits,
				       const uint8_t *context, size_t context_len);

#endif
