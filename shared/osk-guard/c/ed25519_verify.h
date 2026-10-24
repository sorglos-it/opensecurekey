/* Ed25519 - nur PRUEFEN, nie unterschreiben.
 *
 * Nach den Formeln aus RFC 8032 selbst geschrieben. Unterschreiben bleibt
 * im Chip: dort liegt der geheime Schluessel, und dort gehoert er hin.
 * Pruefen beruehrt kein Geheimnis - ein Fehler hier kostet eine abgelehnte
 * Unterschrift, nicht einen verlorenen Schluessel.
 *
 * Abgelehnt wird unter anderem:
 *  - S >= Gruppenordnung L (RFC 8032, Abschnitt 5.1.7)
 *  - ein oeffentlicher Schluessel oder ein R, das kein Kurvenpunkt ist
 *  - ein y-Wert, der nicht kleiner als 2^255-19 ist
 */
#ifndef OSK_ED25519_VERIFY_H
#define OSK_ED25519_VERIFY_H

#include <stddef.h>

#define OSK_ED25519_KEY_LEN 32
#define OSK_ED25519_SIG_LEN 64

/* Gibt 1 zurueck, wenn die Unterschrift zu Schluessel und Nachricht passt,
 * sonst 0. Nachricht darf leer sein (message_len == 0). */
int osk_ed25519_verify(const unsigned char public_key[OSK_ED25519_KEY_LEN],
                       const unsigned char *message, size_t message_len,
                       const unsigned char signature[OSK_ED25519_SIG_LEN]);

#endif /* OSK_ED25519_VERIFY_H */
