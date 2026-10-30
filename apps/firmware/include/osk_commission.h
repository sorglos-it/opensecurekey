/* Erstlauf: die Werksschluessel des Sicherheitschips ersetzen.
 *
 * WARUM DAS SEIN MUSS: Die Werks-Schluessel des Platform-SCP-Kanals stehen
 * im Klartext in NXP AN12973 Tabelle 6, je Variante und Fertigungslos. Wer
 * das freie Dokument liest, liest auf einem Geraet im Werkszustand den
 * ganzen I2C-Verkehr zum Chip mit - und kann ihn stellen. Dann ist der
 * zweite Chip wertlos, denn sein ganzer Zweck ist, dass die Schluessel ihn
 * nie verlassen. Beschrieben in docs/se050.md, "The factory keys are
 * published".
 *
 * osk_se050_rotate_scp_keys() und osk_se050_require_scp() sind das
 * Werkzeug. Diese Datei ist der Ablauf, der sie aufruft - genau einmal je
 * Geraet, in der richtigen Reihenfolge, und so, dass ein Stromverlust
 * mitten darin das Geraet nicht zum Ziegelstein macht.
 *
 * Diese Schicht enthaelt KEINE Krypto, kein Flash und keinen Bus. Alles
 * kommt ueber osk_commission_backend_t von aussen: im Geraet aus der
 * Platinen- und Tresorschicht, in den Pruefungen aus Attrappen. Dasselbe
 * Muster wie osk_boot_backend_t in boot/osk_boot.h, und aus demselben
 * Grund: so ist der Ablauf am PC pruefbar, ohne Chip.
 *
 * Umsetzung: src/classes/se050/se050_commission.c
 * Beschreibung und Begruendung der Entscheidungen: docs/erstlauf.md */
#ifndef OSK_COMMISSION_H
#define OSK_COMMISSION_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_result.h"
#include "osk_se050.h"

/* --- Die zwei Schluesselsaetze -------------------------------------------
 *
 * AN12973 Abschnitt 2.3: der Baustein fuehrt ZWEI Platform-SCP-Saetze, 11
 * und 12. Satz 12 ist der Rettungssatz, mit dem Satz 11 neu gesetzt werden
 * kann. Wer nur 11 ersetzt, laesst eine Hintertuer offen, deren Schluessel
 * bei NXP liegt. Der Erstlauf ersetzt also GENAU ZWEIMAL, und ein Lauf,
 * der nur einen Satz geschafft hat, ist nicht fertig, sondern offen.
 *
 * Die Zahl ist auch das Byte im Befehl: PUT KEY nennt die Schluesselfassung
 * in P1 und im ersten Byte des Datenfeldes, erlaubt ist 0x01 bis 0x7F
 * (GPCS 11.8.2.1). 11 ist 0x0B, 12 ist 0x0C. Es sind keine erfundenen
 * Kennungen, sondern dieselbe Zahl in einem anderen Zahlensystem. */
#define OSK_SCP_SET_MAIN 0x0Bu	   /* Satz 11 - der Arbeitssatz */
#define OSK_SCP_SET_RESCUE 0x0Cu   /* Satz 12 - der Rettungssatz */
#define OSK_SCP_SET_COUNT 2u

/* --- Der Zettel im Flash -------------------------------------------------
 *
 * Er ist das GEDAECHTNIS des Erstlaufs, nicht seine Wahrheit. Die Wahrheit
 * steht im Chip und lautet: welcher Schluesselsatz oeffnet den Kanal. Ein
 * Flag allein waere faelschbar - wer es zuruecksetzt, bekaeme einen zweiten
 * "ersten Start" und damit einen Tausch gegen Schluessel, die er selbst
 * waehlt. Darum wird der Zettel immer gegen die Antwort des Chips geprueft
 * (siehe osk_commission_run und docs/erstlauf.md, Frage 1).
 *
 * Der Zettel traegt die neuen Schluessel. Sie liegen damit im Flash des
 * nRF52840, dessen Auslesesperre uns als gebrochen gilt - das ist die
 * einzige Stelle im Geraet, an der ein dauerhaftes Geheimnis im Rechenkern
 * liegt, und sie ist unvermeidlich: ohne diese Schluessel kann das Geraet
 * seinen Chip nicht ansprechen, und wer sie erst vom Chip holen muesste,
 * braeuchte sie dafuer schon. Was sie schuetzen, ist der Lauscher an den
 * zwei I2C-Leitungen, nicht der Angreifer, der den Rechenkern ausliest.
 * Gegen den schuetzt, dass die NUTZSCHLUESSEL im Chip entstehen und ihn
 * nie verlassen. Begruendet in docs/erstlauf.md, Frage 2.
 *
 * Verpackt wird der Zettel von der Tresorschicht, nicht hier - `store`
 * bekommt und `load` liefert Klartext. */
typedef enum {
	/* Noch nichts getauscht. Ein Geraet, das so aus dem Werk kommt,
	 * und ein Geraet, dessen Zettel fehlt, sehen hier gleich aus. */
	OSK_COMMISSION_FACTORY = 0,
	/* Die neuen Schluessel stehen fest und sind aufgeschrieben, der
	 * Tausch laeuft oder wurde abgebrochen. `done` sagt, welche Saetze
	 * der Chip schon hat. Ein Lauf in diesem Zustand erfindet KEINE
	 * neuen Schluessel - er setzt denselben Tausch fort. */
	OSK_COMMISSION_PENDING = 1,
	/* Beide Saetze ersetzt und der Kanal verpflichtend gemacht. */
	OSK_COMMISSION_DONE = 2,
} osk_commission_stage_t;

#define OSK_COMMISSION_MAGIC 0x314D4F43u /* "COM1" little endian */

typedef struct {
	uint32_t magic;
	uint8_t stage; /* osk_commission_stage_t */
	/* Bit 0: Satz 11 ist getauscht. Bit 1: Satz 12 ist getauscht.
	 * Zwei Bits statt eines Schrittzaehlers, damit nach einem Abbruch
	 * ohne Raten feststeht, welcher Befehl noch fehlt. */
	uint8_t done;
	uint8_t pad[2];
	/* Die neuen Saetze, in der Reihenfolge OSK_SCP_SET_MAIN,
	 * OSK_SCP_SET_RESCUE - also [0] ist Satz 11, [1] ist Satz 12. */
	osk_scp_keyset_t keys[OSK_SCP_SET_COUNT];
} osk_commission_record_t;

#define OSK_COMMISSION_DONE_MAIN 0x01u
#define OSK_COMMISSION_DONE_RESCUE 0x02u
#define OSK_COMMISSION_DONE_BOTH (OSK_COMMISSION_DONE_MAIN | OSK_COMMISSION_DONE_RESCUE)

/* --- Was von aussen kommt ------------------------------------------------
 *
 * Fehlt ein Zeiger, antwortet der Erstlauf OSK_ERR_ARG - und nicht etwa
 * "in Ordnung". Es gibt absichtlich keine Vorgabe-Umsetzung: ein Erstlauf,
 * der mit einem Schluessel aus Nullen durchlaeuft, waere schlimmer als
 * keiner. */
typedef struct {
	/* Den Zettel lesen. OSK_ERR_NOT_FOUND heisst "noch keiner da" und
	 * ist kein Fehler - ein frisches Geraet hat keinen. */
	osk_result_t (*load)(osk_commission_record_t *out);
	/* Den Zettel schreiben. Muss vollstaendig oder gar nicht wirken:
	 * ein halb geschriebener Zettel darf nie gueltig aussehen. Im Geraet
	 * macht das die Tresorschicht mit zwei abwechselnden Abdruecken, wie
	 * osk_boot_state_store(). */
	osk_result_t (*store)(const osk_commission_record_t *rec);

	/* Zufall aus dem Rechenkern - beim nRF52840 aus CryptoCell.
	 * Darf den Chip NICHT fragen; das ist die zweite Quelle. */
	osk_result_t (*random_mcu)(uint8_t *out, size_t len);
	/* Zufall aus dem Sicherheitschip (osk_se050_random). Zwei Quellen,
	 * weil keine von beiden allein den Schluessel bestimmen soll: der
	 * Erstlauf verknuepft sie mit XOR. Faellt eine aus, bricht der Lauf
	 * ab - er rechnet NIE mit einer halben Quelle weiter. */
	osk_result_t (*random_chip)(uint8_t *out, size_t len);

	/* Die Werksschluessel des genannten Satzes liefern, 0x0B oder 0x0C.
	 * Sie haengen am Fertigungslos (OEF ID, AN12973 Tabelle 6) und
	 * stehen deshalb nirgends im Repo - die Platinenschicht liefert,
	 * was fuer das verbaute Teil gilt. Kennt sie den Satz nicht, meldet
	 * sie OSK_ERR_NOT_FOUND. */
	osk_result_t (*factory_keys)(uint8_t key_version, osk_scp_keyset_t *out);

	/* Oeffnet den Kanal mit dem genannten Satz und schliesst ihn
	 * wieder. Das ist die FRAGE AN DEN CHIP, auf die der Erstlauf baut:
	 * OSK_OK heisst "dieser Satz gilt". Im Geraet setzt das die
	 * Platinenschicht um, indem sie den Satz in den Haken
	 * osk_se050_platform_scp_keys() legt und osk_se050_open() aufruft.
	 *
	 * `keys` NULL heisst: der Satz, der jetzt eingerichtet ist. */
	osk_result_t (*probe)(const osk_scp_keyset_t *keys);

	/* Die beiden Vertragsfunktionen des Treibers, eingehaengt, damit
	 * der Ablauf ohne Chip pruefbar bleibt. Im Geraet sind es
	 * osk_se050_rotate_scp_keys() und osk_se050_require_scp(). */
	osk_result_t (*rotate)(uint8_t key_version, const osk_scp_keyset_t *current,
			       const osk_scp_keyset_t *replacement);
	osk_result_t (*require_scp)(void);
} osk_commission_backend_t;

/* --- Der Erstlauf --------------------------------------------------------
 *
 * Wird beim Start aufgerufen, BEVOR irgendein Teil des Geraets den Chip
 * benutzt. Er tut genau eines von drei Dingen:
 *
 *   OSK_OK                 fertig. Entweder war es schon fertig, oder
 *                          dieser Lauf hat es fertig gemacht.
 *   OSK_ERR_ROLLBACK       Zettel und Chip widersprechen sich. Es wird
 *                          NICHTS getauscht - das ist der Fall, in dem ein
 *                          Angreifer einen zweiten ersten Start erzwingen
 *                          wollte. Siehe docs/erstlauf.md, Frage 1.
 *   alles andere           der Tausch ist nicht durch. Der Zettel steht
 *                          dann auf PENDING und nennt, was fehlt; der
 *                          naechste Start setzt mit DENSELBEN Schluesseln
 *                          fort. Das Geraet bleibt ansprechbar.
 *
 * Wiederholbar: ein zweiter Aufruf auf einem fertigen Geraet meldet OSK_OK
 * und schickt keinen einzigen Befehl. Ein dritter Tausch findet nicht
 * statt - weder gegen den Zettel noch gegen den Chip laesst er sich
 * begruenden. */
osk_result_t osk_commission_run(const osk_commission_backend_t *be);

/* Welcher Schluesselsatz gilt jetzt - das, was der SCP03-Haken der
 * Platine ausgeben muss. Liest den Zettel, fragt den Chip nicht.
 *
 * OSK_OK            `out` traegt den Satz, mit dem der Kanal aufzubauen ist
 * OSK_ERR_NOT_FOUND es gilt noch der Werkssatz (kein Zettel, oder FACTORY)
 *
 * Getrennt von osk_commission_run(), weil der Haken bei JEDEM Kanalaufbau
 * aufgerufen wird und der Erstlauf nur einmal beim Start. */
osk_result_t osk_commission_active_keys(const osk_commission_backend_t *be, uint8_t key_version,
					osk_scp_keyset_t *out);

/* --- Der Haken zur Platine -----------------------------------------------
 *
 * Woher Flash, Zufall und die Werksschluessel kommen, haengt am Geraet und
 * gehoert nicht in diesen Vertrag. Darum dieser eine Haken: der Start
 * fragt ihn und ruft dann osk_commission_run().
 *
 * Solange es keine Platinen- und Tresorschicht gibt, antwortet der
 * Platzhalter in se050_commission_platform.c OSK_ERR_UNSUPPORTED - ehrlich
 * und ohne Schluessel aus Nullen. Dasselbe Muster wie bei
 * boot/boot_se050.c und aus demselben Grund: der Baum bleibt baubar, und
 * niemand haelt den Erstlauf fuer eingebaut. */
osk_result_t osk_commission_platform_backend(const osk_commission_backend_t **be_out);

#endif
