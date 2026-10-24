/* Pruefungen des Bluetooth-Teils: Kopplung und GATT-Zugriffe.
 *
 * Geprueft wird vor allem, was NICHT geht: "Just Works", die alte
 * Kopplung, Zugriff ohne Verschluesselung, Zugriff ohne Kopplung,
 * Stuecke in falscher Reihenfolge.
 *
 * Es ist keine Funkmessung dabei. Siehe docs/funk.md. */
#include "test_util.h"
#include "mocks.h"
#include "osk_ui.h"
#include "ble_pair.h"
#include "ble_gatt.h"

/* ---- Kopplung ----------------------------------------------------------- */

static void fill_req(osk_ble_pair_req_t *req, osk_ble_io_t io, uint8_t last)
{
	memset(req, 0, sizeof(*req));
	req->peer_io = io;
	req->secure_connections = true;
	req->mitm_requested = true;
	req->bonding = true;
	req->peer_address[0] = 0xc0;
	req->peer_address[5] = last;
	req->peer_name = "Rechner";
}

static void case_method(void)
{
	T_EQ("Display und Ja/Nein: Zahlenvergleich", osk_ble_pair_method(OSK_BLE_IO_DISPLAY_YES_NO),
	     OSK_BLE_METHOD_NUMERIC_COMPARISON);
	T_EQ("Tastatur und Display: Zahlenvergleich",
	     osk_ble_pair_method(OSK_BLE_IO_KEYBOARD_DISPLAY), OSK_BLE_METHOD_NUMERIC_COMPARISON);
	T_EQ("nur Tastatur: Kennzahl", osk_ble_pair_method(OSK_BLE_IO_KEYBOARD_ONLY),
	     OSK_BLE_METHOD_PASSKEY);
	T_EQ("nur Anzeige: Just Works", osk_ble_pair_method(OSK_BLE_IO_DISPLAY_ONLY),
	     OSK_BLE_METHOD_JUST_WORKS);
	T_EQ("nichts: Just Works", osk_ble_pair_method(OSK_BLE_IO_NONE), OSK_BLE_METHOD_JUST_WORKS);
}

static void case_pair_refused(void)
{
	osk_ble_pair_req_t req;

	mock_reset_all();
	mock_ui_set_confirm(OSK_OK);
	osk_ble_pair_reset();

	/* Die alte Kopplung kommt nicht herein. */
	fill_req(&req, OSK_BLE_IO_DISPLAY_YES_NO, 1);
	req.secure_connections = false;
	T_EQ("ohne Secure Connections", osk_ble_pair_begin(&req), OSK_ERR_UNSUPPORTED);

	/* Ohne Schutz gegen Mithoeren auch nicht. */
	fill_req(&req, OSK_BLE_IO_DISPLAY_YES_NO, 1);
	req.mitm_requested = false;
	T_EQ("ohne Schutz gegen Mithoeren", osk_ble_pair_begin(&req), OSK_ERR_UNSUPPORTED);

	/* "Just Works" ist der Fall, der jeden in Funkreichweite hereinlaesst. */
	fill_req(&req, OSK_BLE_IO_NONE, 1);
	T_EQ("Just Works", osk_ble_pair_begin(&req), OSK_ERR_UNSUPPORTED);
	fill_req(&req, OSK_BLE_IO_DISPLAY_ONLY, 1);
	T_EQ("nur Anzeige", osk_ble_pair_begin(&req), OSK_ERR_UNSUPPORTED);

	/* Die Kennzahl waere geschuetzt, ist aber nicht der vereinbarte Weg. */
	fill_req(&req, OSK_BLE_IO_KEYBOARD_ONLY, 1);
	T_EQ("Kennzahl", osk_ble_pair_begin(&req), OSK_ERR_UNSUPPORTED);

	/* Eine Kopplung nur fuer die Sitzung waere ein Zugang ohne Eintrag. */
	fill_req(&req, OSK_BLE_IO_DISPLAY_YES_NO, 1);
	req.bonding = false;
	T_EQ("ohne dauerhaften Eintrag", osk_ble_pair_begin(&req), OSK_ERR_UNSUPPORTED);

	T_EQ("NULL", osk_ble_pair_begin(NULL), OSK_ERR_ARG);
	T_EQ("nichts gekoppelt", osk_ble_bond_count(), 0);
	T_EQ("kein Tastendruck verlangt", mock_ui_confirm_count(), 0);
}

static void case_pair_button(void)
{
	osk_ble_pair_req_t req;

	mock_reset_all();
	osk_ble_pair_reset();
	fill_req(&req, OSK_BLE_IO_DISPLAY_YES_NO, 1);

	/* Ohne Druck gibt es nicht einmal eine Zahl zu sehen. */
	mock_ui_set_confirm(OSK_ERR_DENIED);
	T_EQ("ohne Druck keine Kopplung", osk_ble_pair_begin(&req), OSK_ERR_DENIED);
	T_EQ("gefragt wurde", mock_ui_confirm_count(), 1);
	T_EQ("der Taster war fuer die Kopplung", mock_ui_last_kind(), OSK_ASK_PAIR);
	T_EQ("die Zahl kommt nicht ohne Beginn", osk_ble_pair_confirm_number(123456),
	     OSK_ERR_DENIED);
	T_EQ("nichts eingetragen", osk_ble_bond_count(), 0);
}

static void case_pair_number(void)
{
	osk_ble_pair_req_t req;

	mock_reset_all();
	mock_clock_set(5000);
	osk_ble_pair_reset();
	mock_ui_set_confirm(OSK_OK);
	fill_req(&req, OSK_BLE_IO_DISPLAY_YES_NO, 1);

	T_EQ("Kopplung begonnen", osk_ble_pair_begin(&req), OSK_OK);
	T_TRUE("Name auf dem Display", strcmp(mock_ui_last_subject(), "Rechner") == 0);

	/* Die sechs Ziffern stehen woertlich auf dem Display - mit fuehrenden
	 * Nullen, sonst vergleicht der Nutzer etwas anderes als der Rechner. */
	T_EQ("Zahl bestaetigt", osk_ble_pair_confirm_number(42), OSK_OK);
	T_TRUE("sechs Ziffern", strcmp(mock_ui_last_detail(), "000042") == 0);

	/* Eingetragen wird erst, wenn der Stapel die Strecke meldet. */
	T_EQ("noch nichts eingetragen", osk_ble_bond_count(), 0);
	T_EQ("ohne Verschluesselung nicht", osk_ble_pair_complete(false, true), OSK_ERR_DENIED);
	T_EQ("nichts eingetragen", osk_ble_bond_count(), 0);

	/* Nach dem Fehlschlag ist die Sitzung weg - es geht nicht einfach
	 * weiter. */
	T_EQ("Sitzung beendet", osk_ble_pair_complete(true, true), OSK_ERR_DENIED);

	/* Ohne Beglaubigung ebenfalls nicht. */
	T_EQ("neu begonnen", osk_ble_pair_begin(&req), OSK_OK);
	T_EQ("Zahl bestaetigt", osk_ble_pair_confirm_number(999999), OSK_OK);
	T_TRUE("sechs Neunen", strcmp(mock_ui_last_detail(), "999999") == 0);
	T_EQ("ohne Beglaubigung nicht", osk_ble_pair_complete(true, false), OSK_ERR_DENIED);

	/* Eine Zahl mit sieben Stellen kommt nicht aus dem Vergleich. */
	T_EQ("neu begonnen", osk_ble_pair_begin(&req), OSK_OK);
	T_EQ("zu grosse Zahl", osk_ble_pair_confirm_number(1000000), OSK_ERR_ARG);

	/* Ohne Druck auf die Zahl: keine Kopplung. */
	T_EQ("neu begonnen", osk_ble_pair_begin(&req), OSK_OK);
	mock_ui_set_confirm(OSK_ERR_DENIED);
	T_EQ("Zahl nicht bestaetigt", osk_ble_pair_confirm_number(111111), OSK_ERR_DENIED);
	T_EQ("nichts eingetragen", osk_ble_bond_count(), 0);

	/* Und ohne bestaetigte Zahl darf der Stapel nichts eintragen. */
	mock_ui_set_confirm(OSK_OK);
	T_EQ("neu begonnen", osk_ble_pair_begin(&req), OSK_OK);
	T_EQ("ohne Zahl kein Eintrag", osk_ble_pair_complete(true, true), OSK_ERR_DENIED);
	T_EQ("nichts eingetragen", osk_ble_bond_count(), 0);
}

static void case_bond_list(void)
{
	osk_ble_pair_req_t req;
	osk_ble_bond_t bond;

	mock_reset_all();
	mock_clock_set(7000);
	osk_ble_pair_reset();
	mock_ui_set_confirm(OSK_OK);

	for (uint8_t i = 1; i <= OSK_BLE_MAX_BONDS; i++) {
		fill_req(&req, OSK_BLE_IO_DISPLAY_YES_NO, i);
		T_EQ("begonnen", osk_ble_pair_begin(&req), OSK_OK);
		T_EQ("Zahl", osk_ble_pair_confirm_number(100000u + i), OSK_OK);
		T_EQ("fertig", osk_ble_pair_complete(true, true), OSK_OK);
	}
	T_EQ("Liste voll", osk_ble_bond_count(), OSK_BLE_MAX_BONDS);

	/* Eine weitere Gegenstelle passt nicht mehr - und das wird vor dem
	 * Tastendruck gesagt, nicht danach. */
	const unsigned before = mock_ui_confirm_count();

	fill_req(&req, OSK_BLE_IO_DISPLAY_YES_NO, 99);
	T_EQ("Liste ist voll", osk_ble_pair_begin(&req), OSK_ERR_FULL);
	T_EQ("nicht gefragt", mock_ui_confirm_count(), before);

	/* Eine bekannte Gegenstelle belegt ihren alten Platz wieder. */
	fill_req(&req, OSK_BLE_IO_DISPLAY_YES_NO, 2);
	T_EQ("bekannte Gegenstelle", osk_ble_pair_begin(&req), OSK_OK);
	T_EQ("Zahl", osk_ble_pair_confirm_number(222222), OSK_OK);
	T_EQ("fertig", osk_ble_pair_complete(true, true), OSK_OK);
	T_EQ("kein zweiter Eintrag", osk_ble_bond_count(), OSK_BLE_MAX_BONDS);

	/* Die Liste ist einzeln lesbar. */
	T_TRUE("Platz 0 lesbar", osk_ble_bond_get(0, &bond));
	T_EQ("Adresse vorn", bond.address[0], 0xc0);
	T_EQ("Adresse hinten", bond.address[5], 1);
	T_TRUE("Name da", strcmp(bond.name, "Rechner") == 0);
	T_EQ("Zeitpunkt", bond.paired_ms, 7000);
	T_TRUE("ausserhalb nicht", !osk_ble_bond_get(OSK_BLE_MAX_BONDS, &bond));
	T_TRUE("NULL nicht", !osk_ble_bond_get(0, NULL));

	/* Einzeln loeschen. */
	uint8_t addr[6];

	memset(addr, 0, sizeof(addr));
	addr[0] = 0xc0;
	addr[5] = 1;
	T_TRUE("ist gekoppelt", osk_ble_bond_known(addr));
	T_EQ("geloescht", osk_ble_bond_forget(0), OSK_OK);
	T_TRUE("nicht mehr gekoppelt", !osk_ble_bond_known(addr));
	T_EQ("einer weniger", osk_ble_bond_count(), OSK_BLE_MAX_BONDS - 1);
	T_EQ("zweimal loeschen", osk_ble_bond_forget(0), OSK_ERR_NOT_FOUND);
	T_EQ("ausserhalb", osk_ble_bond_forget(OSK_BLE_MAX_BONDS), OSK_ERR_ARG);
	T_TRUE("die anderen sind noch da", osk_ble_bond_get(1, &bond));
	T_TRUE("NULL ist nicht gekoppelt", !osk_ble_bond_known(NULL));

	osk_ble_bond_forget_all();
	T_EQ("alle weg", osk_ble_bond_count(), 0);
}

static void case_name_cleanup(void)
{
	osk_ble_pair_req_t req;
	osk_ble_bond_t bond;

	mock_reset_all();
	osk_ble_pair_reset();
	mock_ui_set_confirm(OSK_OK);

	/* Der Name kommt von aussen: Steuerzeichen haben auf einem Display
	 * nichts verloren. */
	fill_req(&req, OSK_BLE_IO_DISPLAY_YES_NO, 1);
	req.peer_name = "bo\x01se\nr Name";
	T_EQ("begonnen", osk_ble_pair_begin(&req), OSK_OK);
	T_EQ("Zahl", osk_ble_pair_confirm_number(1), OSK_OK);
	T_EQ("fertig", osk_ble_pair_complete(true, true), OSK_OK);
	T_TRUE("Eintrag da", osk_ble_bond_get(0, &bond));
	T_TRUE("Steuerzeichen ersetzt", strcmp(bond.name, "bo?se?r Name") == 0);

	/* Ohne Namen wird nichts erfunden. */
	osk_ble_bond_forget_all();
	fill_req(&req, OSK_BLE_IO_DISPLAY_YES_NO, 2);
	req.peer_name = NULL;
	T_EQ("begonnen", osk_ble_pair_begin(&req), OSK_OK);
	T_EQ("Zahl", osk_ble_pair_confirm_number(2), OSK_OK);
	T_EQ("fertig", osk_ble_pair_complete(true, true), OSK_OK);
	T_TRUE("Eintrag da", osk_ble_bond_get(0, &bond));
	T_EQ("Name leer", bond.name[0], 0);
}

static void case_rpa(void)
{
	mock_reset_all();
	mock_clock_set(1000);
	osk_ble_pair_reset();

	T_TRUE("am Anfang faellig", osk_ble_rpa_due());
	osk_ble_rpa_rotated();
	T_TRUE("danach nicht", !osk_ble_rpa_due());
	mock_clock_advance(OSK_BLE_RPA_INTERVAL_MS - 1);
	T_TRUE("kurz davor nicht", !osk_ble_rpa_due());
	mock_clock_advance(2);
	T_TRUE("danach wieder", osk_ble_rpa_due());
	osk_ble_rpa_rotated();
	T_TRUE("wieder frisch", !osk_ble_rpa_due());
}

/* ---- GATT --------------------------------------------------------------- */

#define MAX_NOTE 16
static uint8_t g_note[MAX_NOTE][OSK_BLE_MTU_MAX];
static size_t g_note_len[MAX_NOTE];
static size_t g_note_count;

static uint8_t g_msg[OSK_BLE_MSG_MAX];
static size_t g_msg_len;
static unsigned g_msg_calls;
static size_t g_reply_len = 8;
static osk_result_t g_reply_rc = OSK_OK;

static void notify(const uint8_t *data, size_t len, void *ctx)
{
	(void)ctx;
	if (g_note_count < MAX_NOTE && len <= OSK_BLE_MTU_MAX) {
		memcpy(g_note[g_note_count], data, len);
		g_note_len[g_note_count] = len;
	}
	g_note_count++;
}

static osk_result_t message(const uint8_t *msg, size_t len, uint8_t *out, size_t cap,
			    size_t *out_len, void *ctx)
{
	(void)ctx;
	g_msg_calls++;
	g_msg_len = (len <= sizeof(g_msg)) ? len : sizeof(g_msg);
	memcpy(g_msg, msg, g_msg_len);
	if (g_reply_len > cap)
		return OSK_ERR_INTERNAL;
	for (size_t i = 0; i < g_reply_len; i++)
		out[i] = (uint8_t)(i & 0xffu);
	*out_len = g_reply_len;
	return g_reply_rc;
}

static void gatt_clear(void)
{
	g_note_count = 0;
	g_msg_calls = 0;
	g_msg_len = 0;
	memset(g_note_len, 0, sizeof(g_note_len));
}

/* Eine gekoppelte, verschluesselte, beglaubigte Strecke herstellen. */
static void gatt_ready(void)
{
	osk_ble_pair_req_t req;
	uint8_t addr[6];

	mock_reset_all();
	mock_clock_set(1000);
	g_reply_len = 8;
	g_reply_rc = OSK_OK;
	T_EQ("Start", osk_ble_gatt_init(notify, message, NULL), OSK_OK);
	mock_ui_set_confirm(OSK_OK);

	fill_req(&req, OSK_BLE_IO_DISPLAY_YES_NO, 1);
	T_EQ("begonnen", osk_ble_pair_begin(&req), OSK_OK);
	T_EQ("Zahl", osk_ble_pair_confirm_number(123456), OSK_OK);
	T_EQ("fertig", osk_ble_pair_complete(true, true), OSK_OK);

	memset(addr, 0, sizeof(addr));
	addr[0] = 0xc0;
	addr[5] = 1;
	osk_ble_gatt_connected(addr);
	osk_ble_gatt_security_changed(true, true);
	osk_ble_gatt_subscribe(true);
	gatt_clear();
}

static void case_gatt_access(void)
{
	uint8_t addr[6];
	const uint8_t frag[] = {0x80, 0x00, 0x01, 0xaa};
	uint8_t status[8];
	size_t status_len = 0;

	mock_reset_all();
	T_EQ("ohne Senden kein Start", osk_ble_gatt_init(NULL, message, NULL), OSK_ERR_ARG);
	T_EQ("ohne Bearbeiter kein Start", osk_ble_gatt_init(notify, NULL, NULL), OSK_ERR_ARG);
	T_EQ("Start", osk_ble_gatt_init(notify, message, NULL), OSK_OK);
	gatt_clear();

	/* Ohne Verbindung gibt es die Groesse nicht. */
	T_EQ("ohne Verbindung", osk_ble_gatt_write_request(frag, sizeof(frag)),
	     OSK_ATT_INVALID_HANDLE);
	T_EQ("Zustand ohne Verbindung",
	     osk_ble_gatt_read_status(status, sizeof(status), &status_len), OSK_ATT_INVALID_HANDLE);

	memset(addr, 0, sizeof(addr));
	addr[0] = 0xc0;
	addr[5] = 1;
	osk_ble_gatt_connected(addr);
	T_TRUE("noch nicht bereit", !osk_ble_gatt_ready());

	/* Offene Strecke: zu wenig Verschluesselung. */
	T_EQ("ohne Verschluesselung", osk_ble_gatt_write_request(frag, sizeof(frag)),
	     OSK_ATT_INSUFFICIENT_ENCRYPTION);
	T_EQ("Zustand ohne Verschluesselung",
	     osk_ble_gatt_read_status(status, sizeof(status), &status_len),
	     OSK_ATT_INSUFFICIENT_ENCRYPTION);

	/* Verschluesselt, aber nicht beglaubigt: das ist genau "Just Works". */
	osk_ble_gatt_security_changed(true, false);
	T_EQ("ohne Beglaubigung", osk_ble_gatt_write_request(frag, sizeof(frag)),
	     OSK_ATT_INSUFFICIENT_AUTHENTICATION);

	/* Beides da, aber die Gegenstelle ist nicht gekoppelt. */
	osk_ble_gatt_security_changed(true, true);
	T_EQ("nicht gekoppelt", osk_ble_gatt_write_request(frag, sizeof(frag)),
	     OSK_ATT_INSUFFICIENT_AUTHENTICATION);
	T_EQ("kein Aufruf", g_msg_calls, 0);
	T_TRUE("nicht bereit", !osk_ble_gatt_ready());

	/* Abgeschaltet antwortet nichts. */
	gatt_ready();
	T_TRUE("bereit", osk_ble_gatt_ready());
	osk_ble_gatt_enable(false);
	T_TRUE("aus", !osk_ble_gatt_enabled());
	T_EQ("abgeschaltet", osk_ble_gatt_write_request(frag, sizeof(frag)),
	     OSK_ATT_INVALID_HANDLE);
	osk_ble_gatt_enable(true);
}

static void case_gatt_message(void)
{
	uint8_t frag[OSK_BLE_MTU_MAX];
	uint8_t status[8];
	size_t status_len = 0;

	gatt_ready();

	/* Eine kurze Nachricht in einem Stueck. */
	frag[0] = 0x80;
	frag[1] = 0;
	frag[2] = 3;
	frag[3] = 0xaa;
	frag[4] = 0xbb;
	frag[5] = 0xcc;
	T_EQ("angenommen", osk_ble_gatt_write_request(frag, 6), OSK_ATT_OK);
	T_EQ("einmal bearbeitet", g_msg_calls, 1);
	T_EQ("drei Byte", g_msg_len, 3);
	T_EQ("Inhalt", g_msg[0], 0xaa);
	/* Acht Byte Antwort bei 23 Byte Hoechstlaenge: ein Stueck. */
	T_EQ("eine Benachrichtigung", g_note_count, 1);
	T_EQ("erstes Stueck", g_note[0][0], OSK_BLE_FRAG_FIRST);
	T_EQ("Gesamtlaenge hoch", g_note[0][1], 0);
	T_EQ("Gesamtlaenge tief", g_note[0][2], 8);
	T_EQ("Laenge", g_note_len[0], 11);

	/* Der Zustand ist lesbar. */
	T_EQ("Zustand", osk_ble_gatt_read_status(status, sizeof(status), &status_len), OSK_ATT_OK);
	T_EQ("drei Byte", status_len, 3);
	T_EQ("Protokollfassung", status[0], 1);
	T_EQ("an, nichts in Arbeit", status[1], 0x01);
	T_EQ("ein gekoppeltes Geraet", status[2], 1);
}

static void case_gatt_fragments(void)
{
	uint8_t frag[OSK_BLE_MTU_MAX];
	size_t at;

	gatt_ready();
	/* 23 Byte Hoechstlaenge: 20 Byte Nutzlast, erstes Stueck 17 Byte. */
	g_reply_len = 50;

	frag[0] = 0x80;
	frag[1] = 0;
	frag[2] = 40;
	for (at = 0; at < 17; at++)
		frag[3 + at] = (uint8_t)at;
	T_EQ("erstes Stueck", osk_ble_gatt_write_request(frag, 20), OSK_ATT_OK);
	T_EQ("noch nicht bearbeitet", g_msg_calls, 0);

	frag[0] = 0x00;
	for (at = 0; at < 19; at++)
		frag[1 + at] = (uint8_t)(17 + at);
	T_EQ("zweites Stueck", osk_ble_gatt_write_request(frag, 20), OSK_ATT_OK);
	T_EQ("noch nicht bearbeitet", g_msg_calls, 0);

	frag[0] = 0x01;
	for (at = 0; at < 4; at++)
		frag[1 + at] = (uint8_t)(36 + at);
	T_EQ("letztes Stueck", osk_ble_gatt_write_request(frag, 5), OSK_ATT_OK);
	T_EQ("bearbeitet", g_msg_calls, 1);
	T_EQ("vollstaendig", g_msg_len, 40);
	for (at = 0; at < 40; at++)
		T_EQ("Reihenfolge stimmt", g_msg[at], (uint8_t)at);

	/* 50 Byte Antwort: 17 + 19 + 14. */
	T_EQ("drei Benachrichtigungen", g_note_count, 3);
	T_EQ("erstes Stueck markiert", g_note[0][0], OSK_BLE_FRAG_FIRST);
	T_EQ("erstes Stueck voll", g_note_len[0], 20);
	T_EQ("zweites Stueck Nummer 0", g_note[1][0], 0);
	T_EQ("zweites Stueck voll", g_note_len[1], 20);
	T_EQ("drittes Stueck Nummer 1", g_note[2][0], 1);
	T_EQ("Rest", g_note_len[2], 15);

	/* Mit groesserer Hoechstlaenge geht alles in ein Stueck. */
	gatt_ready();
	g_reply_len = 50;
	osk_ble_gatt_set_mtu(200);
	frag[0] = 0x80;
	frag[1] = 0;
	frag[2] = 1;
	frag[3] = 0x11;
	T_EQ("angenommen", osk_ble_gatt_write_request(frag, 4), OSK_ATT_OK);
	T_EQ("eine Benachrichtigung", g_note_count, 1);
	T_EQ("alles in einem Stueck", g_note_len[0], 53);
	g_reply_len = 8;
}

static void case_gatt_broken(void)
{
	uint8_t frag[OSK_BLE_MTU_MAX];

	gatt_ready();

	/* Fortsetzung ohne Anfang. */
	frag[0] = 0x00;
	frag[1] = 0xaa;
	T_EQ("Fortsetzung ohne Anfang", osk_ble_gatt_write_request(frag, 2),
	     OSK_ATT_INVALID_OFFSET);

	/* Erstes Stueck ohne Gesamtlaenge. */
	frag[0] = 0x80;
	frag[1] = 0x00;
	T_EQ("Kopf unvollstaendig", osk_ble_gatt_write_request(frag, 2),
	     OSK_ATT_INVALID_ATTRIBUTE_VALUE_LENGTH);

	/* Gesamtlaenge 0 und Gesamtlaenge zu gross. */
	frag[0] = 0x80;
	frag[1] = 0;
	frag[2] = 0;
	frag[3] = 0xaa;
	T_EQ("Gesamtlaenge 0", osk_ble_gatt_write_request(frag, 4),
	     OSK_ATT_INVALID_ATTRIBUTE_VALUE_LENGTH);
	frag[1] = 0xff;
	frag[2] = 0xff;
	T_EQ("Gesamtlaenge zu gross", osk_ble_gatt_write_request(frag, 4),
	     OSK_ATT_INVALID_ATTRIBUTE_VALUE_LENGTH);

	/* Ein Stueck, das mehr traegt als angekuendigt ist - der Fall, der
	 * ohne Pruefung ueber den Puffer schreibt. */
	frag[0] = 0x80;
	frag[1] = 0;
	frag[2] = 2;
	for (size_t i = 0; i < 17; i++)
		frag[3 + i] = 0x55;
	T_EQ("mehr als angekuendigt", osk_ble_gatt_write_request(frag, 20),
	     OSK_ATT_INVALID_ATTRIBUTE_VALUE_LENGTH);

	/* Falsche Reihenfolge der Fortsetzungen. */
	frag[0] = 0x80;
	frag[1] = 0;
	frag[2] = 40;
	for (size_t i = 0; i < 17; i++)
		frag[3 + i] = 0x11;
	T_EQ("Anfang", osk_ble_gatt_write_request(frag, 20), OSK_ATT_OK);
	frag[0] = 0x05; /* erwartet waere 0 */
	frag[1] = 0x22;
	T_EQ("falsche Nummer", osk_ble_gatt_write_request(frag, 2), OSK_ATT_INVALID_OFFSET);
	/* Danach ist der Zustand leer. */
	frag[0] = 0x00;
	T_EQ("Zustand weg", osk_ble_gatt_write_request(frag, 2), OSK_ATT_INVALID_OFFSET);

	/* Eine Fortsetzung, die mehr traegt als noch fehlt. */
	frag[0] = 0x80;
	frag[1] = 0;
	frag[2] = 20;
	for (size_t i = 0; i < 17; i++)
		frag[3 + i] = 0x11;
	T_EQ("Anfang", osk_ble_gatt_write_request(frag, 20), OSK_ATT_OK);
	frag[0] = 0x00;
	for (size_t i = 0; i < 19; i++)
		frag[1 + i] = 0x22;
	T_EQ("zu viel", osk_ble_gatt_write_request(frag, 20),
	     OSK_ATT_INVALID_ATTRIBUTE_VALUE_LENGTH);

	/* Kopfbyte mit gesetztem Anfangsbit und noch etwas dazu. */
	frag[0] = 0x81;
	T_EQ("krummes Kopfbyte", osk_ble_gatt_write_request(frag, 4),
	     OSK_ATT_INVALID_ATTRIBUTE_VALUE_LENGTH);

	/* Mehr als eine Nutzlast in einem Zugriff. */
	frag[0] = 0x80;
	frag[1] = 0;
	frag[2] = 60;
	T_EQ("zu langer Zugriff", osk_ble_gatt_write_request(frag, 40),
	     OSK_ATT_INVALID_ATTRIBUTE_VALUE_LENGTH);

	/* Leere Zugriffe. */
	T_EQ("leer", osk_ble_gatt_write_request(frag, 0), OSK_ATT_INVALID_ATTRIBUTE_VALUE_LENGTH);
	T_EQ("NULL", osk_ble_gatt_write_request(NULL, 4), OSK_ATT_INVALID_ATTRIBUTE_VALUE_LENGTH);
	T_EQ("kein Aufruf des Bearbeiters", g_msg_calls, 0);
}

static void case_gatt_security_drop(void)
{
	uint8_t frag[OSK_BLE_MTU_MAX];
	uint8_t status[8];
	size_t status_len = 0;

	gatt_ready();

	/* Eine halbe Nachricht, dann sinkt die Sicherheit: die halbe
	 * Nachricht wird nicht auf der schwaecheren Strecke fertiggestellt. */
	frag[0] = 0x80;
	frag[1] = 0;
	frag[2] = 40;
	for (size_t i = 0; i < 17; i++)
		frag[3 + i] = 0x11;
	T_EQ("Anfang", osk_ble_gatt_write_request(frag, 20), OSK_ATT_OK);
	T_EQ("in Arbeit", osk_ble_gatt_read_status(status, sizeof(status), &status_len),
	     OSK_ATT_OK);
	T_EQ("Bit fuer in Arbeit", status[1], 0x03);

	osk_ble_gatt_security_changed(false, false);
	T_EQ("ohne Verschluesselung", osk_ble_gatt_write_request(frag, 20),
	     OSK_ATT_INSUFFICIENT_ENCRYPTION);
	osk_ble_gatt_security_changed(true, true);
	frag[0] = 0x00;
	T_EQ("die halbe Nachricht ist weg", osk_ble_gatt_write_request(frag, 2),
	     OSK_ATT_INVALID_OFFSET);
	T_EQ("nie bearbeitet", g_msg_calls, 0);

	/* Nach dem Trennen ist nichts mehr bereit. */
	osk_ble_gatt_disconnected();
	T_TRUE("nicht bereit", !osk_ble_gatt_ready());
	T_EQ("getrennt", osk_ble_gatt_write_request(frag, 2), OSK_ATT_INVALID_HANDLE);
}

static void case_gatt_mtu_and_notify(void)
{
	uint8_t frag[OSK_BLE_MTU_MAX];

	gatt_ready();

	/* Die Hoechstlaenge wird begrenzt, nicht geglaubt. */
	osk_ble_gatt_set_mtu(5);
	frag[0] = 0x80;
	frag[1] = 0;
	frag[2] = 1;
	frag[3] = 0x11;
	T_EQ("kleine Hoechstlaenge", osk_ble_gatt_write_request(frag, 4), OSK_ATT_OK);
	T_EQ("bearbeitet", g_msg_calls, 1);
	T_TRUE("Antwort kam", g_note_count > 0);

	osk_ble_gatt_set_mtu(5000);
	gatt_clear();
	T_EQ("grosse Hoechstlaenge", osk_ble_gatt_write_request(frag, 4), OSK_ATT_OK);
	T_TRUE("Stueck nicht groesser als erlaubt", g_note_len[0] <= OSK_BLE_MTU_MAX);

	/* Ohne Anmeldung zur Benachrichtigung geht keine Antwort hinaus -
	 * die Nachricht wird aber bearbeitet. */
	gatt_ready();
	osk_ble_gatt_subscribe(false);
	gatt_clear();
	T_EQ("angenommen", osk_ble_gatt_write_request(frag, 4), OSK_ATT_OK);
	T_EQ("bearbeitet", g_msg_calls, 1);
	T_EQ("keine Benachrichtigung", g_note_count, 0);

	/* Ein Bearbeiter, der einen Fehler meldet, erzeugt keine Antwort. */
	gatt_ready();
	g_reply_rc = OSK_ERR_INTERNAL;
	T_EQ("angenommen", osk_ble_gatt_write_request(frag, 4), OSK_ATT_OK);
	T_EQ("bearbeitet", g_msg_calls, 1);
	T_EQ("keine Antwort", g_note_count, 0);
	g_reply_rc = OSK_OK;
}

int main(void)
{
	case_method();
	case_pair_refused();
	case_pair_button();
	case_pair_number();
	case_bond_list();
	case_name_cleanup();
	case_rpa();
	case_gatt_access();
	case_gatt_message();
	case_gatt_fragments();
	case_gatt_broken();
	case_gatt_security_drop();
	case_gatt_mtu_and_notify();
	return t_report("test_ble");
}
