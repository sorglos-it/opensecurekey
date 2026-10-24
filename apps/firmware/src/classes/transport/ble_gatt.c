/* Der Bluetooth-Dienst fuer das Verwaltungsprogramm. Siehe ble_gatt.h.
 * Bluetooth-Kernspezifikation 5.4, Band 3 Teil F (ATT) und Teil G (GATT). */
#include <string.h>
#include "ble_gatt.h"
#include "ble_pair.h"

/* Fassung des Protokolls aus shared/protocol.md. */
#define OSK_BLE_PROTOCOL_VERSION 1

typedef struct {
	bool active;
	size_t expect; /* angekuendigte Gesamtlaenge */
	size_t have;
	int next_seq; /* -1 = noch kein Fortsetzungsstueck erwartet */
	uint8_t buf[OSK_BLE_MSG_MAX];
} assembly_t;

static osk_ble_notify_fn g_notify;
static osk_ble_message_fn g_handler;
static void *g_ctx;

static bool g_on = true;
static bool g_connected;
static bool g_encrypted;
static bool g_authenticated;
static bool g_bonded;
static bool g_subscribed;
static uint16_t g_mtu = OSK_BLE_MTU_MIN;
static assembly_t g_asm;

static void assembly_reset(void)
{
	/* Der Puffer kann eine Nachricht mit Geheimnissen getragen haben. */
	memset(&g_asm, 0, sizeof(g_asm));
	g_asm.next_seq = -1;
}

bool osk_ble_gatt_ready(void)
{
	return g_on && g_connected && g_encrypted && g_authenticated && g_bonded;
}

void osk_ble_gatt_enable(bool on)
{
	g_on = on;
	if (!on)
		osk_ble_gatt_disconnected();
}

bool osk_ble_gatt_enabled(void)
{
	return g_on;
}

void osk_ble_gatt_connected(const uint8_t address[6])
{
	assembly_reset();
	g_connected = true;
	g_encrypted = false;
	g_authenticated = false;
	g_subscribed = false;
	g_mtu = OSK_BLE_MTU_MIN;
	/* Eine unbekannte Gegenstelle darf sich verbinden - das muss sie, um
	 * sich koppeln zu lassen. Beantwortet wird ihr aber nichts. */
	g_bonded = osk_ble_bond_known(address);
}

void osk_ble_gatt_security_changed(bool encrypted, bool authenticated)
{
	g_encrypted = encrypted;
	g_authenticated = authenticated;
	/* Sinkt die Sicherheit mitten in einer Nachricht, ist die Nachricht
	 * weg - sie wird nicht auf der schwaecheren Strecke fertiggestellt. */
	if (!encrypted || !authenticated)
		assembly_reset();
}

void osk_ble_gatt_disconnected(void)
{
	assembly_reset();
	g_connected = false;
	g_encrypted = false;
	g_authenticated = false;
	g_bonded = false;
	g_subscribed = false;
	g_mtu = OSK_BLE_MTU_MIN;
}

void osk_ble_gatt_set_mtu(uint16_t mtu)
{
	if (mtu < OSK_BLE_MTU_MIN)
		mtu = OSK_BLE_MTU_MIN;
	if (mtu > OSK_BLE_MTU_MAX)
		mtu = OSK_BLE_MTU_MAX;
	g_mtu = mtu;
}

void osk_ble_gatt_subscribe(bool on)
{
	g_subscribed = on;
}

/* Nutzlast je Benachrichtigung: ATT-Hoechstlaenge minus Kopf der
 * Benachrichtigung (ein Byte Kennzeichen, zwei Byte Griff). */
static size_t notify_payload(void)
{
	size_t n = (size_t)g_mtu;

	if (n < OSK_BLE_MTU_MIN)
		n = OSK_BLE_MTU_MIN;
	return n - 3u;
}

/* Schickt eine Antwort in Stuecken: erst die Gesamtlaenge, dann der Rest
 * mit laufender Nummer. */
static void send_message(const uint8_t *msg, size_t len)
{
	uint8_t frag[3 + OSK_BLE_MTU_MAX];
	const size_t room = notify_payload();
	size_t at = 0;
	int seq = 0;

	if (!g_notify || !g_subscribed || len == 0 || len > OSK_BLE_MSG_MAX)
		return;
	/* Das erste Stueck traegt drei Byte Kopf - darunter geht nichts. */
	if (room < 4)
		return;

	frag[0] = OSK_BLE_FRAG_FIRST;
	frag[1] = (uint8_t)(len >> 8);
	frag[2] = (uint8_t)len;

	size_t take = len;

	if (take > room - 3u)
		take = room - 3u;
	memcpy(frag + 3, msg, take);
	g_notify(frag, take + 3u, g_ctx);
	at = take;

	while (at < len) {
		if (seq > (int)OSK_BLE_FRAG_MAX_SEQ)
			break;
		take = len - at;
		if (take > room - 1u)
			take = room - 1u;
		frag[0] = (uint8_t)seq;
		memcpy(frag + 1, msg + at, take);
		g_notify(frag, take + 1u, g_ctx);
		at += take;
		seq++;
	}
	memset(frag, 0, sizeof(frag));
}

/* Eine vollstaendige Nachricht bearbeiten und die Antwort hinausschicken. */
static void dispatch(void)
{
	uint8_t out[OSK_BLE_MSG_MAX];
	size_t out_len = 0;

	if (!g_handler) {
		assembly_reset();
		return;
	}
	memset(out, 0, sizeof(out));
	if (g_handler(g_asm.buf, g_asm.have, out, sizeof(out), &out_len, g_ctx) == OSK_OK &&
	    out_len > 0 && out_len <= sizeof(out))
		send_message(out, out_len);
	memset(out, 0, sizeof(out));
	assembly_reset();
}

uint8_t osk_ble_gatt_write_request(const uint8_t *data, size_t len)
{
	if (!g_on || !g_connected)
		return OSK_ATT_INVALID_HANDLE;
	if (!data || len == 0)
		return OSK_ATT_INVALID_ATTRIBUTE_VALUE_LENGTH;
	/* Reihenfolge der Pruefungen: zuerst Verschluesselung, dann
	 * Beglaubigung, dann die Kopplung. Jeder Fall bekommt seinen eigenen
	 * Fehler, damit das Programm dem Nutzer sagen kann, was fehlt. */
	if (!g_encrypted)
		return OSK_ATT_INSUFFICIENT_ENCRYPTION;
	if (!g_authenticated)
		return OSK_ATT_INSUFFICIENT_AUTHENTICATION;
	if (!g_bonded)
		return OSK_ATT_INSUFFICIENT_AUTHENTICATION;
	/* Mehr als eine ATT-Nutzlast kann in einem Zugriff nicht ankommen. */
	if (len > notify_payload())
		return OSK_ATT_INVALID_ATTRIBUTE_VALUE_LENGTH;

	const uint8_t head = data[0];

	if (head & OSK_BLE_FRAG_FIRST) {
		/* Erstes Stueck: Kopf, Gesamtlaenge, Daten. */
		if (head != OSK_BLE_FRAG_FIRST)
			return OSK_ATT_INVALID_ATTRIBUTE_VALUE_LENGTH;
		if (len < 4)
			return OSK_ATT_INVALID_ATTRIBUTE_VALUE_LENGTH;

		const size_t total = ((size_t)data[1] << 8) | (size_t)data[2];
		const size_t part = len - 3u;

		if (total == 0 || total > OSK_BLE_MSG_MAX)
			return OSK_ATT_INVALID_ATTRIBUTE_VALUE_LENGTH;
		/* Ein Stueck darf nie mehr tragen, als noch angekuendigt ist. */
		if (part > total)
			return OSK_ATT_INVALID_ATTRIBUTE_VALUE_LENGTH;

		assembly_reset();
		g_asm.active = true;
		g_asm.expect = total;
		memcpy(g_asm.buf, data + 3, part);
		g_asm.have = part;
		g_asm.next_seq = 0;
	} else {
		/* Fortsetzungsstueck. Ohne ersten Teil gibt es nichts
		 * fortzusetzen. */
		if (!g_asm.active)
			return OSK_ATT_INVALID_OFFSET;
		if ((int)head != g_asm.next_seq) {
			assembly_reset();
			return OSK_ATT_INVALID_OFFSET;
		}

		const size_t part = len - 1u;

		if (part == 0)
			return OSK_ATT_INVALID_ATTRIBUTE_VALUE_LENGTH;
		/* Laenge VOR dem Kopieren gegen das Angekuendigte pruefen. */
		if (part > g_asm.expect - g_asm.have) {
			assembly_reset();
			return OSK_ATT_INVALID_ATTRIBUTE_VALUE_LENGTH;
		}
		memcpy(g_asm.buf + g_asm.have, data + 1, part);
		g_asm.have += part;
		g_asm.next_seq++;
		if (g_asm.next_seq > (int)OSK_BLE_FRAG_MAX_SEQ) {
			assembly_reset();
			return OSK_ATT_INVALID_OFFSET;
		}
	}

	if (g_asm.have >= g_asm.expect)
		dispatch();
	return OSK_ATT_OK;
}

uint8_t osk_ble_gatt_read_status(uint8_t *out, size_t cap, size_t *len_out)
{
	if (len_out)
		*len_out = 0;
	if (!g_on || !g_connected)
		return OSK_ATT_INVALID_HANDLE;
	if (!out || cap < 3)
		return OSK_ATT_INVALID_ATTRIBUTE_VALUE_LENGTH;
	/* Auch der Zustand ist nichts fuer eine offene Strecke: er verraet,
	 * wie viele Geraete gekoppelt sind. */
	if (!g_encrypted)
		return OSK_ATT_INSUFFICIENT_ENCRYPTION;
	if (!g_authenticated || !g_bonded)
		return OSK_ATT_INSUFFICIENT_AUTHENTICATION;

	out[0] = OSK_BLE_PROTOCOL_VERSION;
	/* Bit 0: dieser Weg ist an. Bit 1: eine Nachricht ist in Arbeit. */
	out[1] = (uint8_t)(0x01u | (g_asm.active ? 0x02u : 0x00u));
	out[2] = (uint8_t)osk_ble_bond_count();
	if (len_out)
		*len_out = 3;
	return OSK_ATT_OK;
}

osk_result_t osk_ble_gatt_init(osk_ble_notify_fn notify, osk_ble_message_fn handler, void *ctx)
{
	if (!notify || !handler)
		return OSK_ERR_ARG;

	g_notify = notify;
	g_handler = handler;
	g_ctx = ctx;
	g_on = true;
	osk_ble_gatt_disconnected();
	osk_ble_pair_reset();

	/* Hier wird absichtlich KEIN CTAP-Bearbeiter angemeldet: FIDO ueber
	 * Bluetooth gibt es auf diesem Geraet nicht (siehe Kopf der Datei).
	 * Wer OSK_LINK_BLE anspricht, erreicht nur das Geraeteprotokoll -
	 * und zwar ueber den Bearbeiter, der hier uebergeben wurde. */
	return OSK_OK;
}
