/* Die APDU-Schicht, damit ein Rechner das Geraet ohne Umbau findet.
 *
 * Was hier drin ist und was nicht - und warum:
 *
 *  - Die Befehle fuer Ethereum sind umgesetzt. Damit kommt MetaMask
 *    zurecht: oeffentlichen Schluessel holen, Transaktion unterschreiben,
 *    Nachricht unterschreiben (EIP-191), typisierte Nachricht
 *    unterschreiben (EIP-712), Fassung abfragen. Das sind fuenf Befehle,
 *    und sie sind nach der oeffentlich beschriebenen Form selbst
 *    geschrieben - kein uebernommener Code.
 *
 *  - Fuer Bitcoin gibt es einen EIGENEN Befehl, der ein ganzes PSBT
 *    annimmt und unterschrieben zurueckgibt. Der alte Bitcoin-Weg mit
 *    GET_TRUSTED_INPUT und stueckweise uebergebenen Transaktionen ist
 *    NICHT umgesetzt. Grund: er verlangt, dass das Geraet fremde
 *    Transaktionen in einem eigenen Format zwischenspeichert und mit
 *    einem geraeteeigenen Schluessel beglaubigt. Das waere viel Code fuer
 *    einen Weg, den PSBT ersetzt. Electrum und Sparrow koennen PSBT
 *    ueber Datei oder Zwischenablage - das geht also, nur nicht als
 *    angestecktes Ledger-Geraet. Das steht so in docs/wallet.md.
 *
 * Diese Schicht entscheidet nichts: sie liest Bytes, ruft die Module und
 * schreibt Bytes zurueck. Jede Freigabe faellt weiter in wallet_confirm.
 *
 * Es gibt KEINEN Schalter fuer die Blindunterschrift - nicht in dieser
 * Schicht und nirgends sonst. Ein Rechner kann sie nicht erbitten, und
 * der Nutzer kann sie nicht einschalten. GET_CONFIG sagt das im ersten
 * Byte der Antwort, damit eine Gegenstelle es vorher weiss.
 *
 * Und wer sie trotzdem VERLANGT, bekommt einen Fehler - keine
 * Stillschweigen.
 *
 * Das ist der Unterschied, auf den es ankommt. Ein gesetztes Feld still
 * zu ueberlesen oder auf 0 zurechtzubiegen waere schlimmer als gar keine
 * Pruefung: dann glaubt die Gegenstelle, sie habe das Blindsignieren
 * eingeschaltet, der Nutzer glaubt es sei aus, und beide haetten recht
 * in ihrer eigenen Vorstellung. Ein stiller Riegel ist schlimmer als ein
 * laut krachender.
 *
 * Genau dieser Fehler ist in diesem Projekt am 29.10.2026 schon einmal
 * gebaut worden: osk_store_add() machte eine Kennung 0 klammheimlich zu
 * 1, statt einen Fehler zu geben. Folge waeren zwei Eintraege mit
 * derselben Kennung gewesen - und osk_store_reveal haette unter einer
 * Kennung das Geheimnis des falschen Eintrags herausgegeben.
 *
 * Darum: OSK_APDU_FLAG_BLIND in einer Anfrage heisst
 * OSK_SW_BLIND_REFUSED, nicht "na gut, dann eben ohne".
 *
 * Diese Schicht ist die Stelle, die ein Angreifer unmittelbar erreicht:
 * sie liest Bytes von einem Rechner, dem niemand vertraut. Darum wird
 * sie gefuzzt - test/wallet/fuzz_apdu.c. */
#ifndef WALLET_APDU_H
#define WALLET_APDU_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"
#include "osk_store.h"

#define OSK_APDU_CLA 0xE0u

#define OSK_APDU_INS_GET_PUBKEY 0x02u
#define OSK_APDU_INS_SIGN_TX 0x04u
#define OSK_APDU_INS_GET_CONFIG 0x06u
#define OSK_APDU_INS_SIGN_MESSAGE 0x08u
#define OSK_APDU_INS_SIGN_TYPED 0x0Cu
/* Eigener Befehl: ein ganzes PSBT hinein, unterschrieben heraus. */
#define OSK_APDU_INS_SIGN_PSBT 0xF0u

/* Statusworte. Dieselben Zahlen wie ueberall in der Smartcard-Welt
 * (ISO 7816-4), damit ein Rechner sie versteht. */
#define OSK_SW_OK 0x9000u
#define OSK_SW_DENIED 0x6985u
#define OSK_SW_WRONG_DATA 0x6A80u
#define OSK_SW_WRONG_LENGTH 0x6700u
#define OSK_SW_INS_UNKNOWN 0x6D00u
#define OSK_SW_CLA_UNKNOWN 0x6E00u
#define OSK_SW_LOCKED 0x6982u
#define OSK_SW_BUSY 0x6F00u
/* "Funktion nicht unterstuetzt" nach ISO 7816-4. Hier heisst es genau
 * eines: ein Rechner hat das Blindsignieren VERLANGT. Eigener Code und
 * nicht OSK_SW_DENIED, damit eine Gegenstelle den Unterschied sieht
 * zwischen "der Nutzer hat nein gesagt" (6985) und "danach zu fragen
 * ist in diesem Geraet nicht vorgesehen" (6A81). */
#define OSK_SW_BLIND_REFUSED 0x6A81u

/* Das Bit im Eigenschaftsbyte, das einmal "blind unterschreiben ist
 * moeglich" hiess. GET_CONFIG liefert es fest als 0. Setzt ein Rechner
 * es in einer Anfrage, wird die Anfrage abgewiesen - siehe oben. */
#define OSK_APDU_FLAG_BLIND 0x01u

/* So viel darf eine Transaktion oder ein PSBT zusammen haben. Mehr wird
 * abgelehnt, nicht teilweise bearbeitet. */
#define OSK_APDU_ASSEMBLY_MAX 2048

typedef struct {
	/* Der Tresoreintrag mit dem Seed. Wird vom Aufrufer gesetzt. */
	osk_item_id_t seed_id;
	/* Netzangaben fuer die Anzeige. */
	const char *btc_hrp;
	uint8_t btc_p2pkh_version;
	uint8_t btc_p2sh_version;
	const char *eth_unit;

	/* Zusammensetzen mehrteiliger Befehle. */
	uint8_t assembly[OSK_APDU_ASSEMBLY_MAX];
	size_t assembly_len;
	uint8_t pending_ins;
	char path[128];
	size_t message_expected; /* nur bei EIP-191 */
} osk_apdu_state_t;

void osk_apdu_init(osk_apdu_state_t *state);

/* Eine APDU bearbeiten. `in` ist die ganze Befehls-APDU
 * (CLA INS P1 P2 Lc Daten), `out` nimmt die Antwortdaten OHNE Statuswort
 * auf; das Statuswort steht in `sw_out`.
 *
 * Liefert immer OSK_OK, wenn eine Antwort entstanden ist - auch bei einem
 * Fehler des Rechners; der Fehler steht dann im Statuswort. OSK_ERR_ARG
 * nur, wenn der Aufruf selbst unbrauchbar ist. */
osk_result_t osk_apdu_handle(osk_apdu_state_t *state, const uint8_t *in, size_t in_len,
			     uint8_t *out, size_t cap, size_t *out_len, uint16_t *sw_out);

/* Einen BIP32-Pfad in der Form lesen, die diese Befehle benutzen:
 * ein Byte Anzahl, dann je vier Byte hoechstwertiges Byte zuerst.
 * Schreibt den Pfad als Text ("m/44'/60'/0'/0/0"). */
osk_result_t osk_apdu_read_path(const uint8_t *in, size_t len, char *out, size_t cap,
				size_t *used_out);

#endif
