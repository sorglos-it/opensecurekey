/* Der Tresor: typisierte Eintraege im externen Flash, verschluesselt mit
 * einem Schluessel, den der SE050 je Sitzung ableitet.
 *
 * Umsetzung: src/classes/store/ - Datenformat in shared/vault-format.md */
#ifndef OSK_STORE_H
#define OSK_STORE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"

typedef enum {
	OSK_ITEM_LOGIN = 1, /* Benutzername + Passwort + Adresse */
	OSK_ITEM_TOTP,
	OSK_ITEM_HOTP,
	OSK_ITEM_SSH_KEY,
	OSK_ITEM_CERT,
	OSK_ITEM_PGP_KEY,
	OSK_ITEM_API_KEY,
	OSK_ITEM_RECOVERY, /* Wiederherstellungscodes */
	OSK_ITEM_SEED,	   /* BIP39 */
	OSK_ITEM_LICENSE,
	OSK_ITEM_CARD, /* Bank- und Kreditkarte */
	OSK_ITEM_IDENTITY,
	OSK_ITEM_WIFI,
	OSK_ITEM_NOTE,
	OSK_ITEM_FILE,
} osk_item_type_t;

typedef uint32_t osk_item_id_t;

/* Was ohne Freigabe sichtbar sein darf: Name, Typ, Adresse. Niemals das
 * Geheimnis selbst - dafuer gibt es osk_store_reveal(). */
typedef struct {
	osk_item_id_t id;
	osk_item_type_t type;
	uint8_t profile; /* mehrere Profile je Geraet */
	const char *name;
	const char *site; /* Adresse, an die der Eintrag gebunden ist */
	uint64_t changed_utc;
	bool mirrored;	       /* darf auf den Zweitstick uebertragen werden */
	bool paired_host_only; /* nur an gekoppelte Rechner herausgeben */

	/* Nicht geheime Kennung zum Suchen, ohne den Eintrag zu oeffnen.
	 * FIDO2 braucht das: beim Anmelden muss unter hunderten Eintraegen der
	 * passende gefunden werden, und jeden einzelnen zu entschluesseln
	 * waere langsam und wuerde fuer jeden eine Freigabe verlangen.
	 * Darin steht, was zum Wiederfinden reicht - etwa der Hash des
	 * Dienstnamens -, nie das Geheimnis selbst.
	 * Achtung: wer den Flash ausliest UND den Tresorschluessel hat, sieht
	 * damit, bei welchen Diensten der Nutzer ein Konto hat. Darum liegt
	 * auch dieses Feld im verschluesselten Teil. */
	uint8_t tag[32];
	uint8_t tag_len; /* 0 = kein Suchschluessel */
} osk_item_meta_t;

osk_result_t osk_store_open(void);
void osk_store_lock(void);
bool osk_store_is_open(void);

osk_result_t osk_store_count(uint8_t profile, size_t *count_out);
osk_result_t osk_store_list(uint8_t profile, size_t index, osk_item_meta_t *out);
osk_result_t osk_store_find(uint8_t profile, const char *site, osk_item_id_t *out, size_t cap,
			    size_t *len_out);

/* Sucht ueber die nicht geheime Kennung (siehe `tag`), ohne ein einziges
 * Geheimnis zu entschluesseln und ohne Freigabe zu verlangen. */
osk_result_t osk_store_find_tag(uint8_t profile, osk_item_type_t type, const uint8_t *tag,
				size_t tag_len, osk_item_id_t *out, size_t cap, size_t *len_out);

osk_result_t osk_store_add(const osk_item_meta_t *meta, const uint8_t *secret, size_t secret_len,
			   osk_item_id_t *id_out);
osk_result_t osk_store_update(osk_item_id_t id, const osk_item_meta_t *meta, const uint8_t *secret,
			      size_t secret_len);
osk_result_t osk_store_delete(osk_item_id_t id);

/* Den oeffentlichen Teil eines Eintrags holen - ohne Freigabe und ohne das
 * Geheimnis anzufassen.
 *
 * Gilt fuer OSK_ITEM_SSH_KEY, OSK_ITEM_CERT und OSK_ITEM_PGP_KEY. Ein
 * oeffentlicher Schluessel ist kein Geheimnis; ihn wie eines zu behandeln
 * kostet nur Tastendruecke und schuetzt nichts.
 *
 * Der Anlass: Der ssh-Agent muss ssh ALLE Identitaeten nennen, bevor
 * irgendetwas unterschrieben wird. Ohne diese Funktion kostete das einen
 * Tastendruck je Schluessel, nur um den Namen zu erfahren - und der Nutzer
 * lernt dabei, Tastendruecke wegzudruecken, ohne hinzusehen. Genau das
 * zerstoert den Wert jeder spaeteren echten Nachfrage.
 *
 * Liegt der Schluessel im SE050, kommt der oeffentliche Teil von dort
 * (osk_se050_key_public); bei eingelesenen Schluesseln aus dem Tresor. */
osk_result_t osk_store_public(osk_item_id_t id, uint8_t *out, size_t cap, size_t *len_out);

/* Gibt das Geheimnis heraus. Fragt selbst nach Freigabe (osk_ui_confirm)
 * und traegt den Zugriff ins Geraeteprotokoll ein. Der Puffer des Aufrufers
 * wird nach Gebrauch von ihm selbst geloescht. */
osk_result_t osk_store_reveal(osk_item_id_t id, const char *asked_by, uint8_t *out, size_t cap,
			      size_t *len_out);

/* --- Signaturzaehler ----------------------------------------------------
 *
 * FESTGELEGT VOM MASTER am 27.10.2026, vor dem Start der Straenge.
 *
 * Gezaehlt wird in CTAP (Strang B), gespeichert hier (Strang D). Genau
 * deshalb steht der Vertrag im Header und nicht in einem der beiden
 * Straenge: wuerde jeder seinen eigenen bauen, gaebe es zwei Zaehler, die
 * gleich heissen und Verschiedenes messen.
 *
 * WARUM DAS NICHT IM RAM GEHT: Der Zaehler ist das Mittel, mit dem ein
 * Dienst einen geklonten Schluessel erkennt - er muss bei jeder Anmeldung
 * groesser sein als zuvor. Heute liegt er im RAM und faengt nach jedem
 * Einstecken bei null an; damit sieht der Dienst bei JEDER zweiten Anmeldung
 * aus wie ein Klon. Eine Pruefung, die nur innerhalb eines Laufs zaehlt,
 * zeigt den Fehler nicht - sie muss ueber einen Neustart hinweg pruefen. */

/* Zaehler des Zugangs um eins erhoehen und den NEUEN Wert melden. Das
 * Erhoehen und das Festschreiben sind ein Vorgang: wird der Strom mitten
 * darin gezogen, ist der Zaehler entweder alt oder neu, nie dazwischen und
 * nie kleiner als vorher.
 *
 * Diese Funktion wird vor dem Signieren gerufen, nicht danach. Ein Zaehler,
 * der nach einer gelungenen Signatur erhoeht wird, laesst sich durch
 * Stromabziehen im richtigen Moment zweimal auf demselben Wert benutzen. */
osk_result_t osk_store_sigcount_next(osk_item_id_t cred, uint32_t *value_out);

/* Nur ablesen, ohne zu erhoehen - fuer Anzeige und Pruefung. */
osk_result_t osk_store_sigcount_get(osk_item_id_t cred, uint32_t *value_out);

/* --- Rueckdrehschutz ---------------------------------------------------
 *
 * Der Stand des Tresors, der bei jedem Festschreiben um eins steigt. Sein
 * Anker liegt NICHT hier, sondern als monotoner Zaehler im Sicherheitschip
 * (osk_se050_counter_read / _raise): ein Wert im Flash laesst sich mit dem
 * Flash zurueckspielen, der im Chip nicht.
 *
 * Der Tresor hebt den Chipzaehler bei jedem Festschreiben selbst mit an und
 * vergleicht beim Oeffnen. Ist der Stand im Flash KLEINER als der im Chip,
 * ist ein alter Zustand zurueckgespielt worden, und osk_store_open()
 * verweigert.
 *
 * Gepruefter Fall ist ein zurueckgespielter, in sich VOELLIG GUELTIGER
 * Zustand - mit richtiger Signatur, richtiger Pruefsumme, richtigem Format.
 * Gegen einen verfaelschten hilft die Verschluesselung schon; der gueltige
 * alte ist der eigentliche Angriff. */
osk_result_t osk_store_generation(uint64_t *value_out);

/* Formatversion und Wanderung nach einem Firmware-Update. */
osk_result_t osk_store_format_version(uint16_t *version_out);
osk_result_t osk_store_migrate(void);

#endif
