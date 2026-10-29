/* Gemeinsame Teile der Kartenanwendungen. Siehe osk_card.h. */
#include <string.h>
#include "osk_card.h"
#include "osk_se050.h"
#include "osk_store.h"
#include "osk_apdu.h"

/* Die Kurvenkennungen - siehe osk_card.h. */
const uint8_t osk_oid_p256[OSK_OID_P256_LEN] = {0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07};
const uint8_t osk_oid_p384[OSK_OID_P384_LEN] = {0x2b, 0x81, 0x04, 0x00, 0x22};

/* Wie oft ein Verwaltungsgeheimnis falsch sein darf. PIV SP 800-73-4 Teil 2
 * Abschnitt 3.2.2 und OpenPGP 3.4 Abschnitt 4.3.1 nennen beide drei. */
#define ADMIN_TRIES_MAX 3u

/* Aufbau des Tresoreintrags: [0] restliche Versuche, [1] Laenge,
 * [2..] das Geheimnis. Der Zaehler liegt mit im Flash, damit er einen
 * Stromausfall uebersteht. */
#define ADMIN_HEAD 2u
#define ADMIN_BODY_MAX (ADMIN_HEAD + OSK_CARD_SECRET_MAX)

/* Nicht geheime Kennung, mit der der Eintrag ohne Entschluesseln gefunden
 * wird (osk_store_find_tag). */
static const char *const g_admin_tag[OSK_CARD_ADMIN_COUNT] = {
    "PIV-PUK",
    "PGP-PW3",
    "PGP-RC",
};

static const char *const g_admin_name[OSK_CARD_ADMIN_COUNT] = {
    "PIV PUK",
    "OpenPGP Admin-PIN",
    "OpenPGP Resetting Code",
};

void osk_card_wipe(void *p, size_t len)
{
	volatile uint8_t *q = (volatile uint8_t *)p;

	while (len-- != 0u)
		*q++ = 0u;
}

bool osk_card_equal(const uint8_t *a, size_t a_len, const uint8_t *b, size_t b_len)
{
	uint8_t diff = (uint8_t)((a_len == b_len) ? 0u : 1u);
	size_t n = (a_len < b_len) ? a_len : b_len;

	if (a == NULL || b == NULL)
		return false;
	for (size_t i = 0; i < n; i++)
		diff |= (uint8_t)(a[i] ^ b[i]);
	return diff == 0u;
}

/* --- Geraete-PIN -------------------------------------------------------- */

/* Der Chip meldet nur Erfolg oder Grund; das Statuswort bauen wir daraus. */
static uint16_t pin_sw(osk_result_t rc)
{
	uint8_t tries = 0;

	switch (rc) {
	case OSK_OK:
		return OSK_SW_OK;
	case OSK_ERR_LOCKED:
		return OSK_SW_AUTH_BLOCKED;
	case OSK_ERR_DENIED:
		if (osk_se050_pin_tries_left(&tries) != OSK_OK)
			return OSK_SW_SECURITY;
		if (tries == 0u)
			return OSK_SW_AUTH_BLOCKED;
		if (tries > 0x0fu)
			tries = 0x0fu;
		return (uint16_t)(OSK_SW_PIN_TRIES | tries);
	case OSK_ERR_ARG:
		return OSK_SW_WRONG_LENGTH;
	default:
		return OSK_SW_SECURITY;
	}
}

uint16_t osk_card_pin_verify(const uint8_t *pin, size_t len)
{
	if (pin == NULL || len == 0u || len > OSK_CARD_SECRET_MAX)
		return OSK_SW_WRONG_LENGTH;
	return pin_sw(osk_se050_pin_verify(pin, len));
}

uint16_t osk_card_pin_status(void)
{
	uint8_t tries = 0;

	if (osk_se050_pin_tries_left(&tries) != OSK_OK)
		return OSK_SW_SECURITY;
	if (tries == 0u)
		return OSK_SW_AUTH_BLOCKED;
	if (tries > 0x0fu)
		tries = 0x0fu;
	/* 63Cx auch dann, wenn alle Versuche noch da sind: die Gegenseite hat
	 * nach dem leeren VERIFY nicht nachgewiesen, dass sie die PIN kennt,
	 * also ist der Zustand "nicht freigegeben" (7816-4 Tabelle 6). */
	return (uint16_t)(OSK_SW_PIN_TRIES | tries);
}

uint16_t osk_card_pin_change(const uint8_t *old_pin, size_t old_len, const uint8_t *new_pin,
			     size_t new_len)
{
	if (old_pin == NULL || new_pin == NULL)
		return OSK_SW_WRONG_LENGTH;
	if (old_len == 0u || new_len == 0u || old_len > OSK_CARD_SECRET_MAX ||
	    new_len > OSK_CARD_SECRET_MAX)
		return OSK_SW_WRONG_LENGTH;
	return pin_sw(osk_se050_pin_set(old_pin, old_len, new_pin, new_len));
}

/* --- Verwaltungsgeheimnisse --------------------------------------------- */

/* Sucht den Eintrag und liefert seine Kopfdaten mit. Der Inhalt wird dabei
 * NICHT geoeffnet - das verlangt eine Freigabe und geschieht erst in
 * admin_read(). */
static bool admin_find(osk_card_admin_t which, osk_item_id_t *id_out, osk_item_meta_t *meta_out)
{
	const char *tag;
	osk_item_id_t ids[2];
	size_t found = 0;
	size_t count = 0;

	if (which >= OSK_CARD_ADMIN_COUNT)
		return false;
	tag = g_admin_tag[which];
	if (osk_store_find_tag(0u, OSK_ITEM_RECOVERY, (const uint8_t *)tag, strlen(tag), ids,
			       (size_t)2, &found) != OSK_OK)
		return false;
	if (found == 0u)
		return false;

	/* Zum Schreiben braucht osk_store_update die Kopfdaten, und die gibt
	 * nur die Liste her. */
	if (osk_store_count(0u, &count) != OSK_OK)
		return false;
	for (size_t i = 0; i < count; i++) {
		osk_item_meta_t meta;

		if (osk_store_list(0u, i, &meta) != OSK_OK)
			continue;
		if (meta.id != ids[0])
			continue;
		if (id_out != NULL)
			*id_out = meta.id;
		if (meta_out != NULL)
			*meta_out = meta;
		return true;
	}
	return false;
}

/* Oeffnet den Eintrag. Fragt ueber osk_store_reveal am Display nach - jede
 * Pruefung eines Verwaltungsgeheimnisses ist ein Verwaltungseingriff. */
static bool admin_read(osk_card_admin_t which, uint8_t *body, size_t cap, size_t *len_out,
		       osk_item_id_t *id_out, osk_item_meta_t *meta_out)
{
	osk_item_id_t id = 0;
	osk_item_meta_t meta;
	size_t len = 0;

	if (!admin_find(which, &id, &meta))
		return false;
	if (osk_store_reveal(id, g_admin_name[which], body, cap, &len) != OSK_OK)
		return false;
	if (len < ADMIN_HEAD || (size_t)body[1] + ADMIN_HEAD > len)
		return false;
	if (len_out != NULL)
		*len_out = len;
	if (id_out != NULL)
		*id_out = id;
	if (meta_out != NULL)
		*meta_out = meta;
	return true;
}

uint8_t osk_card_admin_tries(osk_card_admin_t which)
{
	uint8_t body[ADMIN_BODY_MAX];
	uint8_t tries = 0;

	/* Den Zaehler allein kann nur sagen, wer den Datensatz aufmacht - er
	 * liegt im verschluesselten Teil. Fehlt der Eintrag, gibt es nichts
	 * zu versuchen. */
	if (!admin_read(which, body, sizeof(body), NULL, NULL, NULL))
		return 0u;
	tries = body[0];
	osk_card_wipe(body, sizeof(body));
	return (tries > ADMIN_TRIES_MAX) ? ADMIN_TRIES_MAX : tries;
}

uint8_t osk_card_admin_secret_len(osk_card_admin_t which)
{
	uint8_t body[ADMIN_BODY_MAX];
	uint8_t len;

	if (!admin_read(which, body, sizeof(body), NULL, NULL, NULL))
		return 0u;
	len = body[1];
	osk_card_wipe(body, sizeof(body));
	return (len > OSK_CARD_SECRET_MAX) ? 0u : len;
}

/* Schreibt den Datensatz zurueck - nur, um den Zaehler zu aendern. */
static bool admin_store(osk_item_id_t id, const osk_item_meta_t *meta, const uint8_t *body,
			size_t len)
{
	return osk_store_update(id, meta, body, len) == OSK_OK;
}

uint16_t osk_card_admin_verify(osk_card_admin_t which, const uint8_t *secret, size_t len)
{
	uint8_t body[ADMIN_BODY_MAX];
	osk_item_id_t id = 0;
	osk_item_meta_t meta;
	size_t body_len = 0;
	uint16_t sw;
	bool ok;

	if (which >= OSK_CARD_ADMIN_COUNT)
		return OSK_SW_REF_NOT_FOUND;
	if (secret == NULL || len == 0u || len > OSK_CARD_SECRET_MAX)
		return OSK_SW_WRONG_LENGTH;
	if (!admin_read(which, body, sizeof(body), &body_len, &id, &meta))
		return OSK_SW_REF_NOT_FOUND;

	if (body[0] == 0u) {
		osk_card_wipe(body, sizeof(body));
		return OSK_SW_AUTH_BLOCKED;
	}

	/* Erst den Zaehler herunterzaehlen, dann vergleichen. Wer den Strom
	 * im richtigen Moment wegnimmt, soll keinen Freiversuch bekommen. */
	body[0]--;
	if (!admin_store(id, &meta, body, body_len)) {
		osk_card_wipe(body, sizeof(body));
		return OSK_SW_SECURITY;
	}

	ok = osk_card_equal(body + ADMIN_HEAD, body[1], secret, len);
	if (ok) {
		body[0] = ADMIN_TRIES_MAX;
		if (!admin_store(id, &meta, body, body_len)) {
			osk_card_wipe(body, sizeof(body));
			return OSK_SW_SECURITY;
		}
		sw = OSK_SW_OK;
	} else {
		const uint8_t left = body[0];

		sw = (left == 0u) ? OSK_SW_AUTH_BLOCKED : (uint16_t)(OSK_SW_PIN_TRIES | left);
	}
	osk_card_wipe(body, sizeof(body));
	return sw;
}

uint16_t osk_card_admin_change(osk_card_admin_t which, const uint8_t *cur, size_t cur_len,
			       const uint8_t *next, size_t next_len)
{
	uint8_t body[ADMIN_BODY_MAX];
	osk_item_id_t id = 0;
	osk_item_meta_t meta;
	uint16_t sw;

	if (which >= OSK_CARD_ADMIN_COUNT)
		return OSK_SW_REF_NOT_FOUND;
	if (next == NULL || next_len == 0u || next_len > OSK_CARD_SECRET_MAX)
		return OSK_SW_WRONG_LENGTH;

	if (admin_find(which, &id, &meta)) {
		sw = osk_card_admin_verify(which, cur, cur_len);
		if (sw != OSK_SW_OK)
			return sw;
		body[0] = ADMIN_TRIES_MAX;
		body[1] = (uint8_t)next_len;
		memcpy(body + ADMIN_HEAD, next, next_len);
		sw = admin_store(id, &meta, body, ADMIN_HEAD + next_len) ? OSK_SW_OK
									 : OSK_SW_SECURITY;
		osk_card_wipe(body, sizeof(body));
		return sw;
	}

	/* Gibt es noch keines, wird es angelegt. Das ist der Weg der
	 * Erstinbetriebnahme; danach braucht jede Aenderung das bisherige. */
	memset(&meta, 0, sizeof(meta));
	meta.type = OSK_ITEM_RECOVERY;
	meta.profile = 0u;
	meta.name = g_admin_name[which];
	meta.site = NULL;
	meta.mirrored = false;
	meta.paired_host_only = true;
	meta.tag_len = (uint8_t)strlen(g_admin_tag[which]);
	memcpy(meta.tag, g_admin_tag[which], meta.tag_len);

	body[0] = ADMIN_TRIES_MAX;
	body[1] = (uint8_t)next_len;
	memcpy(body + ADMIN_HEAD, next, next_len);
	sw = (osk_store_add(&meta, body, ADMIN_HEAD + next_len, &id) == OSK_OK) ? OSK_SW_OK
										: OSK_SW_SECURITY;
	osk_card_wipe(body, sizeof(body));
	return sw;
}

uint16_t osk_card_admin_change_split(osk_card_admin_t which, const uint8_t *both, size_t both_len)
{
	const uint8_t cur_len = osk_card_admin_secret_len(which);

	if (both == NULL)
		return OSK_SW_WRONG_LENGTH;
	if (cur_len == 0u)
		return OSK_SW_REF_NOT_FOUND;
	if (both_len <= cur_len || both_len - cur_len > OSK_CARD_SECRET_MAX)
		return OSK_SW_WRONG_LENGTH;
	return osk_card_admin_change(which, both, cur_len, both + cur_len, both_len - cur_len);
}

/* --- Datenobjekte im Tresor --------------------------------------------- */

/* Sucht ueber die Kennung und liefert die Kopfdaten. Oeffnet nichts. */
static bool obj_find(osk_item_type_t type, const char *tag, osk_item_id_t *id_out,
		     osk_item_meta_t *meta_out)
{
	osk_item_id_t ids[2];
	size_t found = 0;
	size_t count = 0;

	if (tag == NULL)
		return false;
	if (osk_store_find_tag(0u, type, (const uint8_t *)tag, strlen(tag), ids, (size_t)2,
			       &found) != OSK_OK)
		return false;
	if (found == 0u)
		return false;
	if (osk_store_count(0u, &count) != OSK_OK)
		return false;
	for (size_t i = 0; i < count; i++) {
		osk_item_meta_t meta;

		if (osk_store_list(0u, i, &meta) != OSK_OK)
			continue;
		if (meta.id != ids[0])
			continue;
		if (id_out != NULL)
			*id_out = meta.id;
		if (meta_out != NULL)
			*meta_out = meta;
		return true;
	}
	return false;
}

osk_result_t osk_card_obj_read(osk_item_type_t type, const char *tag, const char *asked_by,
			       uint8_t *out, size_t cap, size_t *len_out)
{
	osk_item_id_t id = 0;

	if (out == NULL || len_out == NULL)
		return OSK_ERR_ARG;
	if (!obj_find(type, tag, &id, NULL))
		return OSK_ERR_NOT_FOUND;
	return osk_store_reveal(id, asked_by, out, cap, len_out);
}

osk_result_t osk_card_obj_write(osk_item_type_t type, const char *tag, const char *name,
				const uint8_t *data, size_t len)
{
	osk_item_id_t id = 0;
	osk_item_meta_t meta;

	if (data == NULL || len == 0u || tag == NULL)
		return OSK_ERR_ARG;
	if (strlen(tag) > sizeof(meta.tag))
		return OSK_ERR_ARG;

	if (obj_find(type, tag, &id, &meta))
		return osk_store_update(id, &meta, data, len);

	memset(&meta, 0, sizeof(meta));
	meta.type = type;
	meta.profile = 0u;
	meta.name = name;
	meta.site = NULL;
	meta.mirrored = false;
	meta.paired_host_only = false;
	meta.tag_len = (uint8_t)strlen(tag);
	memcpy(meta.tag, tag, meta.tag_len);
	return osk_store_add(&meta, data, len, &id);
}

/* --- Kleinigkeiten ------------------------------------------------------ */

osk_result_t osk_card_public_point(const uint8_t *raw, size_t raw_len, size_t point_len,
				   const uint8_t **point_out)
{
	if (raw == NULL || point_out == NULL || point_len == 0u)
		return OSK_ERR_ARG;
	/* Der Chip liefert den Punkt am Ende einer Verpackung. Genau so liest
	 * ihn auch der CTAP-Strang (src/classes/ctap/osk_pin.c). */
	if (raw_len < point_len)
		return OSK_ERR_FORMAT;
	if (raw[raw_len - point_len] != 0x04u)
		return OSK_ERR_FORMAT;
	*point_out = raw + raw_len - point_len;
	return OSK_OK;
}

/* Ein DER-INTEGER in ein Feld fester Breite: fuehrende Nullen weg, dann
 * rechts ausgerichtet hineinlegen. */
static osk_result_t int_to_fixed(const uint8_t *val, size_t len, size_t half, uint8_t *out)
{
	while (len > 1u && val[0] == 0x00u) {
		val++;
		len--;
	}
	if (len > half)
		return OSK_ERR_FORMAT;
	memset(out, 0, half);
	memcpy(out + (half - len), val, len);
	return OSK_OK;
}

osk_result_t osk_card_sig_der_to_raw(const uint8_t *der, size_t der_len, size_t half, uint8_t *out,
				     size_t cap, size_t *len_out)
{
	const uint8_t *seq = NULL;
	size_t seq_len = 0;
	uint32_t tag = 0;
	const uint8_t *val = NULL;
	size_t val_len = 0;
	size_t used = 0;
	osk_result_t rc;

	if (der == NULL || out == NULL || len_out == NULL || half == 0u)
		return OSK_ERR_ARG;
	if (cap < half * 2u)
		return OSK_ERR_FULL;
	if (!osk_tlv_first(der, der_len, &tag, &seq, &seq_len, &used) || tag != 0x30u)
		return OSK_ERR_FORMAT;

	if (!osk_tlv_first(seq, seq_len, &tag, &val, &val_len, &used) || tag != 0x02u ||
	    val_len == 0u)
		return OSK_ERR_FORMAT;
	rc = int_to_fixed(val, val_len, half, out);
	if (rc != OSK_OK)
		return rc;

	if (!osk_tlv_first(seq + used, seq_len - used, &tag, &val, &val_len, NULL) ||
	    tag != 0x02u || val_len == 0u)
		return OSK_ERR_FORMAT;
	rc = int_to_fixed(val, val_len, half, out + half);
	if (rc != OSK_OK)
		return rc;

	*len_out = half * 2u;
	return OSK_OK;
}
