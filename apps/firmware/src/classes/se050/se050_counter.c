/* Monotone Zaehler im SE050: anlegen, lesen, hochsetzen.
 *
 * Ein Zaehler im Baustein laeuft nur vorwaerts, und das ist die ganze
 * Leistung: ein Wert im Flash laesst sich zurueckspielen, dieser nicht.
 * Darum haengt der Rueckdrehschutz des Tresors (Strang D) hier und nicht
 * im Flash, und der FIDO-Signaturzaehler (Strang B) ebenso.
 *
 * Es gibt absichtlich KEIN counter_set() und kein counter_delete(). Beides
 * waere ein Weg zurueck, und damit waere der Zaehler kein Zaehler mehr. So
 * steht es im Vertrag (osk_se050.h).
 *
 * Quelle: AN12413 Rev. 2.12, Abschnitt 4.7.1.6 (WriteCounter), 4.7.3.1
 * (ReadObject) und Tabelle 288 (Speicherbedarf). Der Plan nennt als Quelle
 * AN13187; das Dokument liegt hier nicht vor, und was dieser Treiber
 * schickt, steht vollstaendig in AN12413. Die Zahlen unten sind darum aus
 * AN12413 belegt. Siehe docs/se050.md.
 *
 * WAS DER BAUSTEIN SELBST ZUSICHERT (4.7.1.6), woertlich zwei Saetze, aus
 * denen hier alles folgt:
 *   - "Counters can only be incremented, not decremented."
 *   - "An input value (TAG_3) must always have the same length as the
 *      existing counter (if it exists); otherwise the command will return
 *      an error."
 * Das zweite ist der Grund, warum _raise() die Laenge des vorhandenen
 * Zaehlers liest statt sie anzunehmen. */
#include <string.h>
#include "osk_se050.h"
#include "se050_apdu.h"
#include "se050_tlv.h"
#include "se050_state.h"

/* Laenge eines neu angelegten Zaehlers in Byte.
 *
 * Acht, weil der Vertrag uint64_t fuehrt: jede andere Laenge waere eine
 * stille Obergrenze, die erst beim Ueberlauf auffaellt. AN12413 4.7.1.6
 * erlaubt 1 bis 8 Byte, acht ist das Maximum.
 *
 * Kosten dafuer, belegt: ein Zaehler braucht nach AN12413 Tabelle 288
 * (Abschnitt 8.1, "Secure Object memory File objects") dauerhaft
 * 92 Byte + Zaehlerlaenge, bei acht Byte also 100 Byte NVM. */
#define OSK_COUNTER_LEN 8u

/* ------------------------------------------------------------- Hilfen */

/* Den Wert eines Zaehlers lesen und dabei auch seine Laenge melden.
 *
 * ReadObject (AN12413 4.7.3.1): "If the object is a monotonic counter,
 * the counter value is returned" - in TAG_1, hoechstwertiges Byte zuerst.
 * Die Laenge ist die des Zaehlers, nicht eine feste. */
static osk_result_t counter_fetch(osk_se050_apdu_t *a, osk_key_id_t id, uint64_t *value_out,
				  size_t *len_out)
{
	uint8_t payload[8];
	osk_tlv_writer_t w;
	osk_tlv_init(&w, payload, sizeof(payload));
	osk_tlv_put_u32(&w, TAG_1, id);

	size_t pl = 0;
	osk_result_t r = osk_tlv_done(&w, &pl);
	if (r != OSK_OK) {
		return r;
	}

	uint8_t rsp[32];
	size_t n = 0;
	r = osk_se050_apdu_exchange(a, INS_READ, P1_DEFAULT, P2_DEFAULT, payload, pl, rsp,
				    sizeof(rsp), &n);
	if (r != OSK_OK) {
		return r;
	}

	osk_tlv_reader_t rd;
	osk_tlv_reader_init(&rd, rsp, n);

	const uint8_t *val = NULL;
	size_t val_len = 0;
	r = osk_tlv_get(&rd, TAG_1, &val, &val_len);
	if (r != OSK_OK) {
		return r;
	}
	if (val_len == 0u || val_len > OSK_COUNTER_LEN) {
		/* Ausserhalb von 1 bis 8 Byte ist es kein Zaehler
		 * (AN12413 4.7.1.6). Das wird gemeldet, nicht zurechtgelegt. */
		return OSK_ERR_FORMAT;
	}

	uint64_t v = 0;
	for (size_t i = 0; i < val_len; i++) {
		v = (v << 8) | (uint64_t)val[i];
	}
	*value_out = v;
	*len_out = val_len;
	return OSK_OK;
}

/* ---------------------------------------------------------- Anlegen */

osk_result_t osk_se050_counter_create(osk_key_id_t id)
{
	osk_se050_apdu_t *a = NULL;
	osk_result_t r = osk_se050_state(&a);
	if (r != OSK_OK) {
		return r;
	}

	/* ERST NACHSEHEN, OB ES IHN SCHON GIBT, und das ist keine
	 * Umstaendlichkeit: WriteCounter ohne TAG_3 heisst bei einem
	 * VORHANDENEN Zaehler "um eins erhoehen" (AN12413 4.7.1.6, TAG_3
	 * optional: "if object exists: if not present, increment by 1").
	 * Ein zweiter counter_create() wuerde also still weiterzaehlen,
	 * statt zu sagen, dass der Zaehler schon da ist - und ein
	 * Einrichtungslauf, der zweimal laeuft, verschoebe den
	 * Rueckdrehschutz um eins. */
	uint64_t existing = 0;
	size_t existing_len = 0;
	if (counter_fetch(a, id, &existing, &existing_len) == OSK_OK) {
		return OSK_ERR_EXISTS;
	}

	/* WriteCounter (AN12413 4.7.1.6): TAG_1 Kennung, TAG_2 die Laenge
	 * in Byte. TAG_3 bleibt weg - mit TAG_2 und ohne TAG_3 legt der
	 * Baustein den Zaehler an.
	 *
	 * OFFEN: AN12413 schreibt nicht hin, mit welchem Wert ein neuer
	 * Zaehler anfaengt. Dieser Treiber behauptet darum keinen - wer den
	 * Startwert braucht, liest ihn mit osk_se050_counter_read(). Fuer
	 * osk_se050_counter_raise() ist er gleichgueltig: die Funktion
	 * liest vorher. */
	uint8_t payload[16];
	osk_tlv_writer_t w;
	osk_tlv_init(&w, payload, sizeof(payload));
	osk_tlv_put_u32(&w, TAG_1, id);
	osk_tlv_put_u16(&w, TAG_2, OSK_COUNTER_LEN);

	size_t pl = 0;
	r = osk_tlv_done(&w, &pl);
	if (r != OSK_OK) {
		return r;
	}

	size_t dummy = 0;
	return osk_se050_apdu_exchange(a, INS_WRITE, P1_COUNTER, P2_DEFAULT, payload, pl, NULL, 0,
				       &dummy);
}

/* ------------------------------------------------------------- Lesen */

osk_result_t osk_se050_counter_read(osk_key_id_t id, uint64_t *value_out)
{
	osk_se050_apdu_t *a = NULL;
	const osk_result_t r = osk_se050_state(&a);
	if (r != OSK_OK) {
		return r;
	}
	if (value_out == NULL) {
		return OSK_ERR_ARG;
	}
	*value_out = 0;

	size_t len = 0;
	return counter_fetch(a, id, value_out, &len);
}

/* --------------------------------------------------------- Hochsetzen */

osk_result_t osk_se050_counter_raise(osk_key_id_t id, uint64_t at_least)
{
	osk_se050_apdu_t *a = NULL;
	osk_result_t r = osk_se050_state(&a);
	if (r != OSK_OK) {
		return r;
	}

	/* Erst lesen. Drei Dinge haengen daran, und keines davon ist
	 * verzichtbar:
	 *
	 * 1. WIEDERHOLBARKEIT. Steht der Zaehler schon auf dem Wert oder
	 *    hoeher, wird NICHTS geschickt und das Ergebnis ist OSK_OK. Das
	 *    braucht der Aufrufer nach einem Stromausfall mitten im
	 *    Schreiben: er weiss dann nicht, ob der Befehl angekommen ist,
	 *    und ruft einfach noch einmal auf.
	 * 2. Der Baustein wuerde einen kleineren Wert mit einem Fehler
	 *    beantworten ("can only be incremented"). Dieser Fehler saehe
	 *    aus wie eine Stoerung, ist aber keiner - der Zustand stimmt
	 *    ja.
	 * 3. Die LAENGE. TAG_3 muss genau so lang sein wie der vorhandene
	 *    Zaehler, sonst lehnt der Baustein ab. Also wird sie gelesen
	 *    und nicht angenommen - ein Zaehler, den jemand anders mit
	 *    vier Byte angelegt hat, wird so richtig bedient. */
	uint64_t current = 0;
	size_t len = 0;
	r = counter_fetch(a, id, &current, &len);
	if (r != OSK_OK) {
		return r;
	}
	if (current >= at_least) {
		return OSK_OK;
	}

	/* Passt der Zielwert ueberhaupt in diesen Zaehler? Bei acht Byte
	 * immer; bei einem kuerzeren nicht zwangslaeufig. Das ist ein
	 * Fehler des Aufrufers, nicht des Bausteins, und wird als solcher
	 * gemeldet. */
	if (len < sizeof(uint64_t)) {
		const uint64_t max = (len == 0u) ? 0u : ((uint64_t)1u << (len * 8u)) - 1u;
		if (at_least > max) {
			return OSK_ERR_ARG;
		}
	}

	/* WriteCounter mit TAG_3: "if present, set counter to value"
	 * (AN12413 4.7.1.6). Hoechstwertiges Byte zuerst, genau so lang
	 * wie der Zaehler. TAG_2 bleibt weg - der Zaehler ist da, seine
	 * Laenge steht fest. */
	uint8_t value[OSK_COUNTER_LEN];
	for (size_t i = 0; i < len; i++) {
		value[len - 1u - i] = (uint8_t)((at_least >> (i * 8u)) & 0xFFu);
	}

	uint8_t payload[16u + OSK_COUNTER_LEN];
	osk_tlv_writer_t w;
	osk_tlv_init(&w, payload, sizeof(payload));
	osk_tlv_put_u32(&w, TAG_1, id);
	osk_tlv_put(&w, TAG_3, value, len);

	size_t pl = 0;
	r = osk_tlv_done(&w, &pl);
	if (r != OSK_OK) {
		return r;
	}

	size_t dummy = 0;
	return osk_se050_apdu_exchange(a, INS_WRITE, P1_COUNTER, P2_DEFAULT, payload, pl, NULL, 0,
				       &dummy);
}
