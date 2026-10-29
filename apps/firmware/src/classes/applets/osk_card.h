/* Was PIV und OpenPGP gemeinsam brauchen: Freigabe, Verwaltungsgeheimnisse,
 * und ein paar kleine Umrechnungen.
 *
 * Die PIN liegt im SE050 - samt Fehlversuchszaehler, denn ein Zaehler im
 * Flash laesst sich durch Stromwegnehmen zuruecksetzen. Beide Anwendungen
 * benutzen darum dieselbe Geraete-PIN. Das ist bewusst so: zwei PINs auf
 * einem Geraet, von denen eine schwaecher geschuetzt ist, sind keine
 * Verbesserung.
 *
 * Die Verwaltungsgeheimnisse (PIV-PUK, OpenPGP-PW3) kann der Chip nicht
 * halten - er hat genau eine PIN. Sie liegen deshalb im Tresor, und ihr
 * Fehlversuchszaehler liegt im selben Datensatz, also im Flash und nicht im
 * RAM: so uebersteht er einen Stromausfall. Jede Pruefung eines solchen
 * Geheimnisses fragt ueber osk_store_reveal() am Display nach - das ist
 * gewollt, denn das ist jedes Mal ein Verwaltungseingriff.
 */
#ifndef OSK_CARD_H
#define OSK_CARD_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"
#include "osk_store.h"

/* Laengste PIN und laengstes Verwaltungsgeheimnis, das wir annehmen. */
#define OSK_CARD_SECRET_MAX 64u

/* Welches Verwaltungsgeheimnis gemeint ist. Jedes liegt als eigener
 * Tresoreintrag vom Typ OSK_ITEM_RECOVERY mit eigener Kennung. */
typedef enum {
	OSK_CARD_ADMIN_PIV_PUK = 0,
	OSK_CARD_ADMIN_PGP_PW3,
	OSK_CARD_ADMIN_PGP_RC, /* Resetting Code, OpenPGP 3.4 Abschnitt 4.3.4 */
	OSK_CARD_ADMIN_COUNT
} osk_card_admin_t;

/* --- Geraete-PIN im Chip ------------------------------------------------ */

/* Prueft die PIN. Liefert ein Statuswort: 9000, 63Cx mit der Zahl der
 * restlichen Versuche, oder 6983 wenn gesperrt. */
uint16_t osk_card_pin_verify(const uint8_t *pin, size_t len);

/* Wie viele Versuche noch - ohne zu pruefen (VERIFY mit leerem Datenfeld). */
uint16_t osk_card_pin_status(void);

uint16_t osk_card_pin_change(const uint8_t *old_pin, size_t old_len, const uint8_t *new_pin,
			     size_t new_len);

/* --- Verwaltungsgeheimnisse im Tresor ----------------------------------- */

uint16_t osk_card_admin_verify(osk_card_admin_t which, const uint8_t *secret, size_t len);

/* Setzt ein neues Geheimnis. `cur` muss das bisherige sein, ausser das
 * Geheimnis gibt es noch nicht - dann darf `cur` leer sein, und der
 * Eintrag wird angelegt. */
uint16_t osk_card_admin_change(osk_card_admin_t which, const uint8_t *cur, size_t cur_len,
			       const uint8_t *next, size_t next_len);

/* Setzt ein neues Geheimnis, wenn beide ohne Trenner hintereinander
 * ankommen - so verlangt es OpenPGP (Abschnitt 7.2.3). Geteilt wird an der
 * Laenge, die im Datensatz neben dem Geheimnis steht. */
uint16_t osk_card_admin_change_split(osk_card_admin_t which, const uint8_t *both, size_t both_len);

/* Zahl der restlichen Versuche, oder 0 wenn es das Geheimnis nicht gibt. */
uint8_t osk_card_admin_tries(osk_card_admin_t which);

/* Laenge des gespeicherten Geheimnisses, oder 0 wenn es keines gibt. Fragt
 * am Display nach, weil der Datensatz dafuer geoeffnet werden muss. */
uint8_t osk_card_admin_secret_len(osk_card_admin_t which);

/* --- Datenobjekte im Tresor --------------------------------------------- */

/* Beide Anwendungen legen Zertifikate und andere nicht geheime Datenobjekte
 * im Tresor ab, gefunden ueber die nicht geheime Kennung. Gelesen wird ueber
 * osk_store_reveal(), das am Display nachfragt - darum liest der Aufrufer
 * einmal je Einschaltvorgang und merkt sich das Ergebnis. */
osk_result_t osk_card_obj_read(osk_item_type_t type, const char *tag, const char *asked_by,
			       uint8_t *out, size_t cap, size_t *len_out);

/* Legt an oder ersetzt. `name` steht spaeter in der Liste des
 * Verwaltungsprogramms. */
osk_result_t osk_card_obj_write(osk_item_type_t type, const char *tag, const char *name,
				const uint8_t *data, size_t len);

/* --- Objektkennungen der beiden Kurven ---------------------------------- */

/* RFC 5480 Abschnitt 2.1.1.1: prime256v1 ist 1.2.840.10045.3.1.7,
 * secp384r1 ist 1.3.132.0.34. Nur der Inhalt, ohne Tag und Laenge.
 *
 * Sie stehen hier, weil zwei Teile sie brauchen: OpenPGP in den
 * Algorithmus-Eigenschaften (Spezifikation 3.4 Abschnitt 4.4.3.9) und die
 * Attestierung im SubjectPublicKeyInfo (osk_attest.c). Dieselbe Zahl
 * gehoert an eine Stelle - zweimal hingeschrieben veraltet eine davon
 * still. */
#define OSK_OID_P256_LEN 8u
#define OSK_OID_P384_LEN 5u
extern const uint8_t osk_oid_p256[OSK_OID_P256_LEN];
extern const uint8_t osk_oid_p384[OSK_OID_P384_LEN];

/* --- Kleinigkeiten ------------------------------------------------------ */

/* Vergleich ohne Zeitunterschied. true = gleich. */
bool osk_card_equal(const uint8_t *a, size_t a_len, const uint8_t *b, size_t b_len);

void osk_card_wipe(void *p, size_t len);

/* Holt den unkomprimierten Punkt (0x04 || X || Y) aus dem, was der Chip als
 * oeffentlichen Schluessel liefert - der haengt ihn hinten an eine
 * Verpackung. `point_len` ist 65 bei P-256 und 97 bei P-384. */
osk_result_t osk_card_public_point(const uint8_t *raw, size_t raw_len, size_t point_len,
				   const uint8_t **point_out);

/* ECDSA-Unterschrift von DER (SEQUENCE aus zwei INTEGER) in die flache Form
 * r || s mit je `half` Bytes. OpenPGP verlangt die flache Form, PIV die DER-Form. */
osk_result_t osk_card_sig_der_to_raw(const uint8_t *der, size_t der_len, size_t half, uint8_t *out,
				     size_t cap, size_t *len_out);

#endif
