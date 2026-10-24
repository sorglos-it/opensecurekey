/* osk-guard fuer C: ein geschuetztes Programm in wenigen Zeilen.
 *
 *     osk_guard *guard = NULL;
 *     const char *problem = osk_guard_open("127.0.0.1:8300", key, 32, &guard);
 *     if (problem != NULL) { return 1; }            // kein Stick: nicht los
 *     if (osk_guard_present(guard, NULL) != NULL) { return 1; }
 *
 * Die Bibliothek haengt am lokalen Dienst des Programms `osk` und prueft die
 * Unterschrift SELBST. Dem Dienst wird kein Ja geglaubt - sonst waere der
 * Schutz die Aussage eines Programms ueber sich selbst.
 *
 * Jeder Aufruf gibt NULL zurueck, wenn alles gut ging, sonst einen
 * Fehlerschluessel wie "error.guard.noecdh". Zu jedem Schluessel steht in
 * lang/<sprache>.json ein Satz - derselbe Schluessel in allen fuenf
 * Bibliotheken. Es ist nie errno.
 *
 * Nur die Standardbibliothek und die Sockets des Systems, kein Fremdcode.
 */
#ifndef OSK_GUARD_H
#define OSK_GUARD_H

#include <stddef.h>
#include <stdint.h>

/* Groessen des Verfahrens - dieselben wie im Geraet. */
#define OSK_GUARD_NONCE_LEN  32
#define OSK_GUARD_HASH_LEN   32
#define OSK_GUARD_KEY_LEN    32
#define OSK_GUARD_SIG_LEN    64
#define OSK_GUARD_SECRET_LEN 32

/* Obergrenze fuer Nutzdaten von seal/unseal: 1 MiB, in Hex also 2 MiB. */
#define OSK_GUARD_MAX_PLAIN (1u << 20)

/* Grenzen der Textfelder einer Lizenz. Laengeres wird abgewiesen, nicht
 * abgeschnitten. */
#define OSK_GUARD_MAX_TEXT  128
#define OSK_GUARD_MAX_TIERS 16
#define OSK_GUARD_MAX_TIER  64

/* Die Verbindung zum lokalen Dienst. Inhalt gehoert der Bibliothek. */
typedef struct osk_guard osk_guard;

/* Eine gepruefte Lizenz aus dem Tresor. Zeiten sind Sekunden seit 1970 in
 * UTC; expires_utc == 0 heisst: laeuft nie ab. */
typedef struct {
    char product[OSK_GUARD_MAX_TEXT];
    char holder[OSK_GUARD_MAX_TEXT];
    char number[OSK_GUARD_MAX_TEXT];
    char tiers[OSK_GUARD_MAX_TIERS][OSK_GUARD_MAX_TIER];
    size_t tier_count;
    uint32_t seats;
    int64_t issued_utc;
    int64_t expires_utc;
} osk_license;

/* osk_guard_open prueft die Angaben und sieht nach, ob der Dienst antwortet.
 * device_public_key sind die 32 Byte, die der Hersteller einmal mit
 * `osk guard-key` aus seinem Stick gelesen und ins Programm eingetragen hat.
 *
 * issuer_public_key ist der oeffentliche Schluessel des Herstellers, mit dem
 * Lizenzen unterschrieben sind. Er darf NULL und issuer_len 0 sein; dann
 * scheitert osk_guard_license mit "error.guard.issuer", statt ungeprueft
 * etwas zu liefern. */
const char *osk_guard_open(const char *address,
                           const unsigned char *device_public_key,
                           size_t key_len,
                           const unsigned char *issuer_public_key,
                           size_t issuer_len, osk_guard **out);

/* osk_guard_close gibt die Verbindung frei. NULL ist erlaubt. */
void osk_guard_close(osk_guard *guard);

/* osk_guard_present ist eine Runde Frage und Antwort: der Zufallswert kommt
 * von hier, die Unterschrift wird hier geprueft. Kommt NULL heraus, steckte
 * in diesem Augenblick der richtige Stick, und die gepruefte Programmdatei
 * ist unveraendert.
 *
 * binary_path == NULL heisst: die laufende Datei. Was diese Stufe wert ist,
 * steht in docs/dongle.md - sie haelt das Mitnehmen einer Kopie auf und
 * nichts darueber hinaus. Echter Schutz ist seal/unseal. */
const char *osk_guard_present(osk_guard *guard, const char *binary_path);

/* osk_guard_seal verschliesst Daten mit einem Schluessel, der den Stick nie
 * verlaesst. extra darf NULL sein; ist es gesetzt, muss es beim Oeffnen
 * gleich sein. Der Aufrufer gibt *blob mit free() zurueck. */
const char *osk_guard_seal(osk_guard *guard, const char *label,
                           const unsigned char *extra, size_t extra_len,
                           const unsigned char *plain, size_t plain_len,
                           unsigned char **blob, size_t *blob_len);

/* osk_guard_unseal macht wieder auf. Ohne Stick gibt es den Schluessel
 * nicht - dann kommt ein Fehler und keine halben Daten. Der Aufrufer gibt
 * *plain mit free() zurueck und ueberschreibt es vorher am besten. */
const char *osk_guard_unseal(osk_guard *guard, const char *label,
                             const unsigned char *extra, size_t extra_len,
                             const unsigned char *blob, size_t blob_len,
                             unsigned char **plain, size_t *plain_len);

/* osk_guard_license holt den unterschriebenen Lizenzblock fuer ein Produkt
 * und prueft ihn SELBST: Aufbau, Fassung, dann die Ed25519-Unterschrift
 * gegen den Herstellerschluessel aus osk_guard_open. Erst danach werden die
 * Felder aus der geprueften Nutzlast gelesen. Der Dienst liefert nur den
 * Block - kein Weg fuehrt hier zu Werten, die er sich ausgedacht hat. */
const char *osk_guard_license(osk_guard *guard, const char *product,
                              osk_license *out);

/* osk_guard_license_check prueft einen schon vorliegenden Lizenzblock und
 * braucht dafuer keine Verbindung und keinen Stick - nur den oeffentlichen
 * Schluessel des Herstellers. Die Reihenfolge ist dieselbe wie bei
 * osk_guard_license: Aufbau, Fassung, Unterschrift, und erst danach die
 * Felder aus der geprueften Nutzlast.
 *
 * Nuetzlich fuer ein Programm, das sich den Block weggelegt hat - und das
 * ist auch der Weg, auf dem der Prueflauf die Lizenzvektoren durchfaehrt. */
const char *osk_guard_license_check(const unsigned char *block,
                                    size_t block_len,
                                    const unsigned char *issuer_public_key,
                                    size_t issuer_len, osk_license *out);

/* osk_guard_license_expired sagt, ob die Lizenz zum Zeitpunkt now_utc
 * abgelaufen ist. now_utc < 0 nimmt die Uhr des Rechners. */
int osk_guard_license_expired(const osk_license *license,
                              int64_t now_utc);

/* osk_guard_license_has_tier sagt, ob eine Ausbaustufe enthalten ist.
 * Gross- und Kleinschreibung spielen keine Rolle. */
int osk_guard_license_has_tier(const osk_license *license,
                               const char *name);

/* osk_guard_node_public holt den oeffentlichen Teil eines Knotenschluessels.
 * curve ist "ed25519" zum Unterschreiben oder "x25519" zum
 * Schluesselaustausch. */
const char *osk_guard_node_public(osk_guard *guard, const char *label,
                                  const char *curve,
                                  unsigned char public_key[OSK_GUARD_KEY_LEN]);

/* osk_guard_node_sign laesst den Knoten einen Auftrag unterschreiben. */
const char *osk_guard_node_sign(osk_guard *guard, const char *label,
                                const unsigned char *job, size_t job_len,
                                unsigned char signature[OSK_GUARD_SIG_LEN]);

/* osk_guard_node_verify prueft die Unterschrift eines anderen Knotens. Das
 * braucht keinen Stick und keine Verbindung - nur den oeffentlichen
 * Schluessel der Gegenseite. */
const char *osk_guard_node_verify(const unsigned char *public_key,
                                  size_t key_len, const char *label,
                                  const unsigned char *job, size_t job_len,
                                  const unsigned char *signature,
                                  size_t signature_len);

/* osk_guard_node_agree handelt ein gemeinsames Geheimnis mit der Gegenseite
 * aus. Kann der gelieferte Chip kein ECDH, kommt "error.guard.noecdh" -
 * siehe osk_guard_is_no_ecdh. */
const char *osk_guard_node_agree(osk_guard *guard, const char *label,
                                 const unsigned char *peer_public,
                                 size_t peer_len,
                                 unsigned char secret[OSK_GUARD_SECRET_LEN]);

/* osk_guard_is_no_ecdh sagt, ob der gelieferte Chip keinen
 * Schluesselaustausch kann. Dann faellt der Serverbetrieb mit Schluessel im
 * Chip weg - das Programm sollte den Anwendungsfall abschalten und es sagen,
 * nicht abbrechen. */
int osk_guard_is_no_ecdh(const char *problem);

/* osk_guard_binary_hash rechnet die SHA-256-Pruefsumme einer Programmdatei.
 * path == NULL nimmt die laufende Datei. */
const char *osk_guard_binary_hash(const char *path,
                                  unsigned char out[OSK_GUARD_HASH_LEN]);

#endif /* OSK_GUARD_H */
