/* Kopplung ueber Bluetooth. Siehe ble_pair.h.
 * Bluetooth-Kernspezifikation 5.4, Band 3 Teil H, Abschnitt 2.3. */
#include <string.h>
#include "ble_pair.h"
#include "osk_ui.h"

/* Zeit kommt aus dem Kern, damit die Pruefungen sie stellen koennen. */
uint64_t osk_ctap_now_ms(void);

/* Was dieses Geraet kann: es hat ein Display und einen Taster. Genau damit
 * ist der Zahlenvergleich moeglich - und nur dieser wird zugelassen. */
#define OSK_BLE_OWN_IO OSK_BLE_IO_DISPLAY_YES_NO

typedef struct {
	bool active;	/* eine Kopplung laeuft */
	bool confirmed; /* die Zahl ist bestaetigt */
	uint8_t address[6];
	char name[OSK_BLE_NAME_MAX];
} session_t;

static osk_ble_bond_t g_bonds[OSK_BLE_MAX_BONDS];
static session_t g_session;
static uint64_t g_rpa_set_ms;
static bool g_rpa_have;

static void session_reset(void)
{
	memset(&g_session, 0, sizeof(g_session));
}

void osk_ble_pair_reset(void)
{
	memset(g_bonds, 0, sizeof(g_bonds));
	session_reset();
	g_rpa_have = false;
	g_rpa_set_ms = 0;
}

osk_ble_method_t osk_ble_pair_method(osk_ble_io_t peer_io)
{
	/* Dieses Geraet ist "Display und Ja/Nein". Die Zeile daraus aus
	 * Tabelle 2.8 der Kernspezifikation: */
	switch (peer_io) {
	case OSK_BLE_IO_DISPLAY_YES_NO:
	case OSK_BLE_IO_KEYBOARD_DISPLAY:
		return OSK_BLE_METHOD_NUMERIC_COMPARISON;
	case OSK_BLE_IO_KEYBOARD_ONLY:
		return OSK_BLE_METHOD_PASSKEY;
	case OSK_BLE_IO_DISPLAY_ONLY:
	case OSK_BLE_IO_NONE:
	default:
		return OSK_BLE_METHOD_JUST_WORKS;
	}
}

static size_t free_slot(void)
{
	for (size_t i = 0; i < OSK_BLE_MAX_BONDS; i++) {
		if (!g_bonds[i].used)
			return i;
	}
	return OSK_BLE_MAX_BONDS;
}

static size_t slot_of(const uint8_t address[6])
{
	for (size_t i = 0; i < OSK_BLE_MAX_BONDS; i++) {
		if (g_bonds[i].used && memcmp(g_bonds[i].address, address, 6) == 0)
			return i;
	}
	return OSK_BLE_MAX_BONDS;
}

/* Name der Gegenstelle uebernehmen. Er kommt von aussen, also wird er
 * begrenzt und von allem befreit, was auf einem Display nichts verloren
 * hat - ein Name darf die Anzeige nicht umbauen. */
static void copy_name(char *dst, size_t cap, const char *src)
{
	size_t at = 0;

	memset(dst, 0, cap);
	if (!src)
		return;
	while (at + 1 < cap && src[at]) {
		const unsigned char c = (unsigned char)src[at];

		dst[at] = (c >= 0x20u && c <= 0x7eu) ? (char)c : '?';
		at++;
	}
}

osk_result_t osk_ble_pair_begin(const osk_ble_pair_req_t *req)
{
	osk_ask_req_t ask;

	session_reset();
	if (!req)
		return OSK_ERR_ARG;

	/* Die alte Kopplung ist gebrochen - sie kommt hier nicht herein,
	 * auch nicht als Rueckfall. */
	if (!req->secure_connections)
		return OSK_ERR_UNSUPPORTED;
	/* Ohne Schutz gegen Mithoeren waere der Schluesselaustausch
	 * angreifbar, und genau das ist der Punkt des Zahlenvergleichs. */
	if (!req->mitm_requested)
		return OSK_ERR_UNSUPPORTED;
	if (osk_ble_pair_method(req->peer_io) != OSK_BLE_METHOD_NUMERIC_COMPARISON)
		return OSK_ERR_UNSUPPORTED;
	/* Nur dauerhafte Kopplung: eine Sitzungskopplung ohne Eintrag waere
	 * ein Zugang, den der Nutzer in der Liste nicht sehen und nicht
	 * loeschen kann. */
	if (!req->bonding)
		return OSK_ERR_UNSUPPORTED;

	/* Ist diese Gegenstelle schon bekannt, belegt sie ihren alten Platz
	 * wieder - sonst muss einer frei sein. */
	if (slot_of(req->peer_address) == OSK_BLE_MAX_BONDS && free_slot() == OSK_BLE_MAX_BONDS)
		return OSK_ERR_FULL;

	memcpy(g_session.address, req->peer_address, sizeof(g_session.address));
	copy_name(g_session.name, sizeof(g_session.name), req->peer_name);

	/* Erst der Taster, dann ueberhaupt eine Zahl. Wer das Geraet nicht in
	 * der Hand hat, sieht nichts und koppelt nichts. */
	memset(&ask, 0, sizeof(ask));
	ask.kind = OSK_ASK_PAIR;
	ask.subject = g_session.name[0] ? g_session.name : "ble.unknown";
	ask.asked_by = "ble";
	ask.timeout_ms = 30000;
	if (osk_ui_confirm(&ask) != OSK_OK) {
		session_reset();
		return OSK_ERR_DENIED;
	}

	g_session.active = true;
	return OSK_OK;
}

osk_result_t osk_ble_pair_confirm_number(uint32_t number)
{
	char digits[OSK_BLE_COMPARE_DIGITS + 1];
	osk_ask_req_t ask;

	if (!g_session.active)
		return OSK_ERR_DENIED;
	/* Sechs Ziffern, nicht mehr. Ein groesserer Wert kommt nicht aus dem
	 * Vergleich und wird nicht angezeigt. */
	if (number > 999999u) {
		session_reset();
		return OSK_ERR_ARG;
	}

	for (unsigned i = 0; i < OSK_BLE_COMPARE_DIGITS; i++) {
		const unsigned place = OSK_BLE_COMPARE_DIGITS - 1u - i;
		uint32_t div = 1;

		for (unsigned k = 0; k < place; k++)
			div *= 10u;
		digits[i] = (char)('0' + (char)((number / div) % 10u));
	}
	digits[OSK_BLE_COMPARE_DIGITS] = 0;

	memset(&ask, 0, sizeof(ask));
	ask.kind = OSK_ASK_PAIR;
	ask.subject = g_session.name[0] ? g_session.name : "ble.unknown";
	ask.detail = digits; /* die Zahl steht woertlich auf dem Display */
	ask.asked_by = "ble";
	ask.timeout_ms = 30000;
	if (osk_ui_confirm(&ask) != OSK_OK) {
		session_reset();
		return OSK_ERR_DENIED;
	}

	g_session.confirmed = true;
	return OSK_OK;
}

osk_result_t osk_ble_pair_complete(bool encrypted, bool authenticated)
{
	if (!g_session.active || !g_session.confirmed) {
		session_reset();
		return OSK_ERR_DENIED;
	}
	/* Der Stapel muss beides melden. Eine unverschluesselte oder nicht
	 * beglaubigte Strecke wird nicht eingetragen - dann war der
	 * Zahlenvergleich umsonst. */
	if (!encrypted || !authenticated) {
		session_reset();
		return OSK_ERR_DENIED;
	}

	size_t slot = slot_of(g_session.address);

	if (slot == OSK_BLE_MAX_BONDS)
		slot = free_slot();
	if (slot == OSK_BLE_MAX_BONDS) {
		session_reset();
		return OSK_ERR_FULL;
	}

	memset(&g_bonds[slot], 0, sizeof(g_bonds[slot]));
	g_bonds[slot].used = true;
	memcpy(g_bonds[slot].address, g_session.address, sizeof(g_bonds[slot].address));
	memcpy(g_bonds[slot].name, g_session.name, sizeof(g_bonds[slot].name));
	g_bonds[slot].paired_ms = osk_ctap_now_ms();
	g_bonds[slot].last_ms = g_bonds[slot].paired_ms;

	session_reset();
	return OSK_OK;
}

size_t osk_ble_bond_count(void)
{
	size_t n = 0;

	for (size_t i = 0; i < OSK_BLE_MAX_BONDS; i++)
		n += g_bonds[i].used ? 1u : 0u;
	return n;
}

bool osk_ble_bond_get(size_t index, osk_ble_bond_t *out)
{
	if (!out || index >= OSK_BLE_MAX_BONDS)
		return false;
	if (!g_bonds[index].used)
		return false;
	memcpy(out, &g_bonds[index], sizeof(*out));
	return true;
}

osk_result_t osk_ble_bond_forget(size_t index)
{
	if (index >= OSK_BLE_MAX_BONDS)
		return OSK_ERR_ARG;
	if (!g_bonds[index].used)
		return OSK_ERR_NOT_FOUND;
	/* Der Platz wird ueberschrieben, nicht nur freigegeben: dort stand
	 * die Kennung einer Gegenstelle. */
	memset(&g_bonds[index], 0, sizeof(g_bonds[index]));
	return OSK_OK;
}

void osk_ble_bond_forget_all(void)
{
	memset(g_bonds, 0, sizeof(g_bonds));
}

bool osk_ble_bond_known(const uint8_t address[6])
{
	if (!address)
		return false;
	return slot_of(address) != OSK_BLE_MAX_BONDS;
}

bool osk_ble_rpa_due(void)
{
	if (!g_rpa_have)
		return true;
	return (osk_ctap_now_ms() - g_rpa_set_ms) >= OSK_BLE_RPA_INTERVAL_MS;
}

void osk_ble_rpa_rotated(void)
{
	g_rpa_have = true;
	g_rpa_set_ms = osk_ctap_now_ms();
}
