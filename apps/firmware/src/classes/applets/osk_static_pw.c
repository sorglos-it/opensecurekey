/* Statisches Passwort, Kartenseite. Siehe osk_static_pw.h. */
#include <string.h>
#include <stdio.h>
#include "osk_static_pw.h"
#include "osk_card.h"
#include "osk_store.h"

/* Im Tresor liegt es als OSK_ITEM_LOGIN - das ist, was es ist: ein
 * Passwort. Die Einmalcodes nehmen OSK_ITEM_API_KEY, und das bleibt
 * getrennt, sonst findet die Suche nach Marke beides. */
#define PW_TYPE OSK_ITEM_LOGIN

/* Satzaufbau: Fassung, reserviert, Schalter, Laenge, dann der Text. Feld
 * fuer Feld geschrieben und nicht als Abbild der Struktur - ein anderer
 * Uebersetzer legt Strukturen anders aus (dieselbe Begruendung wie bei
 * state_pack() in osk_openpgp.c).
 *
 * Das zweite Byte war bis zum 29.10.2026 eine Belegungsnummer. Es gibt
 * keine: die Belegung ist die Abbildung in der Tastaturseite
 * (osk_kbd_set_scan_map()), und eine Nummer hier haette auf beiden Seiten
 * etwas anderes geheissen. Es bleibt als reserviertes Byte stehen und
 * muss 0 sein - so laesst sich der Satz spaeter erweitern, ohne dass
 * jemand eine alte Zahl als neue Bedeutung liest. */
#define REC_VERSION 1u
#define REC_HEAD 4u
#define REC_RESERVED 1u /* das Byte, das 0 sein muss */
#define FLAG_ENTER 0x01u

static const char *slot_tag(uint8_t slot)
{
	switch (slot) {
	case 0u:
		return "SPW-1";
	case 1u:
		return "SPW-2";
	default:
		return NULL;
	}
}

static const char *slot_name(uint8_t slot)
{
	return (slot == 0u) ? "Statisches Passwort 1" : "Statisches Passwort 2";
}

void osk_static_pw_reset(void)
{
	/* Dieses Modul merkt sich nichts: ein statisches Passwort wird
	 * genau dann geholt, wenn getippt werden soll, und danach nicht
	 * aufbewahrt. Die Funktion gibt es trotzdem, damit der Verteiler
	 * sie wie bei den anderen Teilen rufen kann - und damit beim
	 * naechsten Zwischenspeicher hier eine Stelle steht, an die er
	 * gehoert. */
}

/* Kann die Tastaturseite dieses Zeichen tippen? Gefragt wird die Liste
 * aus usb_kbd.h und keine eigene - siehe Kopf von osk_static_pw.h. */
static bool typeable(uint8_t c)
{
	static const char order[] = OSK_KBD_SCAN_ORDER;

	for (size_t i = 0; i + 1u < sizeof(order); i++) {
		if ((uint8_t)order[i] == c)
			return true;
	}
	return false;
}

bool osk_static_pw_text_ok(const uint8_t *text, size_t len)
{
	if (text == NULL || len == 0u || len > OSK_STATIC_PW_MAX)
		return false;
	for (size_t i = 0; i < len; i++) {
		if (!typeable(text[i]))
			return false;
	}
	return true;
}

static bool slot_item(uint8_t slot, osk_item_id_t *id_out)
{
	const char *tag = slot_tag(slot);
	osk_item_id_t ids[2];
	size_t n = 0;

	if (tag == NULL)
		return false;
	if (osk_store_find_tag(0u, PW_TYPE, (const uint8_t *)tag, strlen(tag), ids, (size_t)2,
			       &n) != OSK_OK)
		return false;
	if (n != 1u)
		return false;
	if (id_out != NULL)
		*id_out = ids[0];
	return true;
}

bool osk_static_pw_present(uint8_t slot)
{
	return slot_item(slot, NULL);
}

osk_result_t osk_static_pw_set(uint8_t slot, const osk_static_pw_t *pw)
{
	uint8_t rec[REC_HEAD + OSK_STATIC_PW_MAX];
	const char *tag = slot_tag(slot);
	osk_result_t rc;

	if (tag == NULL || pw == NULL)
		return OSK_ERR_ARG;
	if (!osk_static_pw_text_ok(pw->text, pw->len))
		return OSK_ERR_ARG;

	rec[0] = (uint8_t)REC_VERSION;
	rec[1] = 0u; /* reserviert, siehe oben */
	rec[2] = pw->append_enter ? (uint8_t)FLAG_ENTER : 0u;
	rec[3] = pw->len;
	memcpy(rec + REC_HEAD, pw->text, pw->len);

	rc = osk_card_obj_write(PW_TYPE, tag, slot_name(slot), rec, REC_HEAD + (size_t)pw->len);
	osk_card_wipe(rec, sizeof(rec));
	return rc;
}

osk_result_t osk_static_pw_clear(uint8_t slot)
{
	osk_item_id_t id = 0;

	if (slot_tag(slot) == NULL)
		return OSK_ERR_ARG;
	if (!slot_item(slot, &id))
		return OSK_ERR_NOT_FOUND;
	return osk_store_delete(id);
}

osk_result_t osk_static_pw_fetch(uint8_t slot, const char *asked_by, osk_static_pw_t *out)
{
	uint8_t rec[REC_HEAD + OSK_STATIC_PW_MAX];
	size_t len = 0;
	const char *tag = slot_tag(slot);
	osk_result_t rc;

	if (tag == NULL || out == NULL)
		return OSK_ERR_ARG;
	memset(out, 0, sizeof(*out));

	/* Oeffnet den Eintrag und fragt damit am Display nach. */
	rc = osk_card_obj_read(PW_TYPE, tag, (asked_by != NULL) ? asked_by : slot_name(slot), rec,
			       sizeof(rec), &len);
	if (rc != OSK_OK)
		return rc;

	if (len < REC_HEAD || rec[0] != (uint8_t)REC_VERSION) {
		osk_card_wipe(rec, sizeof(rec));
		return OSK_ERR_FORMAT;
	}
	if ((size_t)rec[3] + REC_HEAD != len) {
		/* Die Laenge im Satz und die Laenge des Satzes sind zwei
		 * Angaben; stimmen sie nicht ueberein, wird nicht die eine
		 * der anderen angepasst, sondern abgelehnt. */
		osk_card_wipe(rec, sizeof(rec));
		return OSK_ERR_FORMAT;
	}

	if (rec[REC_RESERVED] != 0u) {
		/* Ein Satz, in dem das reservierte Byte belegt ist, kommt von
		 * einer Fassung, die etwas damit meinte. Diese hier weiss
		 * nicht was - also wird er nicht halb gelesen. */
		osk_card_wipe(rec, sizeof(rec));
		return OSK_ERR_FORMAT;
	}
	out->append_enter = (rec[2] & FLAG_ENTER) != 0u;
	out->len = rec[3];
	memcpy(out->text, rec + REC_HEAD, out->len);
	osk_card_wipe(rec, sizeof(rec));

	/* Auch beim Lesen geprueft, nicht nur beim Schreiben: der Satz kommt
	 * aus dem Flash, und was dort steht, hat diese Fassung des Programms
	 * nicht unbedingt hingeschrieben - und was die Tastaturseite tippen
	 * kann, kann sich mit einer neuen Abbildung geaendert haben. */
	if (!osk_static_pw_text_ok(out->text, out->len)) {
		osk_card_wipe(out, sizeof(*out));
		return OSK_ERR_FORMAT;
	}
	return OSK_OK;
}
