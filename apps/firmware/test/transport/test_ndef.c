/* Pruefungen des NDEF-Teils: der Parser, der Bau der Datei und die
 * Anwendung des Type-4-Tags.
 *
 * Der Parser ist die erste Wand fuer einen Angreifer mit einer Antenne,
 * darum stehen hier vor allem Nachrichten, die luegen: Laengen, die ueber
 * das Ende zeigen, geteilte Datensaetze, fehlende Endmarkierung.
 *
 * Es ist keine Funkmessung dabei. Siehe docs/funk.md. */
#include "test_util.h"
#include "mocks.h"
#include "nfc_ndef.h"
#include "nfc_apdu.h"

static void set_uri(const char *uri, bool without_button)
{
	osk_ndef_config_t cfg;

	memset(&cfg, 0, sizeof(cfg));
	cfg.mode = OSK_NDEF_URI;
	cfg.uri_prefix = OSK_NDEF_URI_HTTPS;
	cfg.uri_without_button = without_button;
	size_t n = 0;

	while (uri[n] && n < OSK_NDEF_URI_MAX) {
		cfg.uri[n] = uri[n];
		n++;
	}
	T_EQ("Adresse uebernommen", osk_ndef_configure(&cfg), OSK_OK);
}

/* ---- Parser ------------------------------------------------------------- */

static void case_parse_good(void)
{
	osk_ndef_record_t rec[OSK_NDEF_MAX_RECORDS];
	size_t count = 0;

	/* Ein kurzer URI-Datensatz. */
	const uint8_t one[] = {0xd1, 0x01, 0x04, 'U', 0x04, 'a', '.', 'b'};

	T_TRUE("ein Datensatz", osk_ndef_parse(one, sizeof(one), rec, 8, &count));
	T_EQ("genau einer", count, 1);
	T_EQ("gut bekannter Typ", rec[0].tnf, 0x01);
	T_EQ("Typlaenge", rec[0].type_len, 1);
	T_EQ("Typ U", rec[0].type[0], 'U');
	T_EQ("Nutzlaenge", rec[0].payload_len, 4);
	T_TRUE("Anfang markiert", rec[0].first);
	T_TRUE("Ende markiert", rec[0].last);

	/* Zwei Datensaetze. */
	const uint8_t two[] = {0x91, 0x01, 0x01, 'T', 'x', 0x51, 0x01, 0x01, 'T', 'y'};

	T_TRUE("zwei Datensaetze", osk_ndef_parse(two, sizeof(two), rec, 8, &count));
	T_EQ("genau zwei", count, 2);
	T_TRUE("erster ist Anfang", rec[0].first && !rec[0].last);
	T_TRUE("zweiter ist Ende", !rec[1].first && rec[1].last);

	/* Mit Kennung (IL) und langer Laenge (ohne SR). */
	const uint8_t with_id[] = {0xc9, 0x01, 0x00, 0x00, 0x00, 0x02, 0x03,
				   'T',	 'i',  'd',  '!',  'x',	 'y'};

	T_TRUE("Kennung und lange Laenge",
	       osk_ndef_parse(with_id, sizeof(with_id), rec, 8, &count));
	T_EQ("einer", count, 1);
	T_EQ("Kennungslaenge", rec[0].id_len, 3);
	T_EQ("Nutzlaenge", rec[0].payload_len, 2);

	/* Eine leere Datei ist in Ordnung, nur eben ohne Datensatz. */
	const uint8_t empty[1] = {0};

	count = 99;
	T_TRUE("leere Nachricht geht", osk_ndef_parse(empty, 0, rec, 8, &count));
	T_EQ("kein Datensatz", count, 0);
}

static void case_parse_bad(void)
{
	osk_ndef_record_t rec[OSK_NDEF_MAX_RECORDS];
	size_t count = 7;

	/* Nutzlaenge zeigt ueber das Ende - der Fall, der ohne Pruefung
	 * fremden Speicher liest. */
	const uint8_t over[] = {0xd1, 0x01, 0xff, 'U', 0x04, 'a'};

	T_TRUE("Laenge luegt", !osk_ndef_parse(over, sizeof(over), rec, 8, &count));
	T_EQ("kein Datensatz gemeldet", count, 0);

	/* Typlaenge zeigt ueber das Ende. */
	const uint8_t over_type[] = {0xd1, 0x40, 0x01, 'U', 'x'};

	T_TRUE("Typlaenge luegt", !osk_ndef_parse(over_type, sizeof(over_type), rec, 8, &count));

	/* Kennungslaenge zeigt ueber das Ende. */
	const uint8_t over_id[] = {0xd9, 0x01, 0x01, 0x40, 'T', 'x'};

	T_TRUE("Kennungslaenge luegt", !osk_ndef_parse(over_id, sizeof(over_id), rec, 8, &count));

	/* Kopf abgeschnitten. */
	const uint8_t stump[] = {0xd1};

	T_TRUE("ein Byte ist kein Datensatz",
	       !osk_ndef_parse(stump, sizeof(stump), rec, 8, &count));

	/* Lange Laenge angekuendigt, aber die vier Bytes fehlen. */
	const uint8_t no_len[] = {0xc1, 0x01, 0x00, 0x00};

	T_TRUE("lange Laenge unvollstaendig",
	       !osk_ndef_parse(no_len, sizeof(no_len), rec, 8, &count));

	/* Geteilter Datensatz: nehmen wir nicht an. */
	const uint8_t chunked[] = {0xb1, 0x01, 0x01, 'T', 'x', 0x51, 0x01, 0x01, 'T', 'y'};

	T_TRUE("geteilt wird abgewiesen",
	       !osk_ndef_parse(chunked, sizeof(chunked), rec, 8, &count));

	/* Nicht vergebener Typbereich. */
	const uint8_t reserved[] = {0xd7, 0x01, 0x01, 'T', 'x'};

	T_TRUE("TNF 7 wird abgewiesen",
	       !osk_ndef_parse(reserved, sizeof(reserved), rec, 8, &count));

	/* Anfangsmarkierung fehlt. */
	const uint8_t no_mb[] = {0x51, 0x01, 0x01, 'T', 'x'};

	T_TRUE("ohne Anfangsmarkierung", !osk_ndef_parse(no_mb, sizeof(no_mb), rec, 8, &count));

	/* Anfangsmarkierung mitten im Strom. */
	const uint8_t mb_twice[] = {0x91, 0x01, 0x01, 'T', 'x', 0xd1, 0x01, 0x01, 'T', 'y'};

	T_TRUE("zwei Anfaenge", !osk_ndef_parse(mb_twice, sizeof(mb_twice), rec, 8, &count));

	/* Endmarkierung fehlt. */
	const uint8_t no_me[] = {0x91, 0x01, 0x01, 'T', 'x'};

	T_TRUE("ohne Endmarkierung", !osk_ndef_parse(no_me, sizeof(no_me), rec, 8, &count));

	/* Hinter dem Ende steht noch etwas. */
	const uint8_t tail[] = {0xd1, 0x01, 0x01, 'T', 'x', 0xff, 0xff};

	T_TRUE("Anhaengsel hinter dem Ende", !osk_ndef_parse(tail, sizeof(tail), rec, 8, &count));

	/* Mehr Datensaetze als Platz da ist. */
	uint8_t many[9 * 5];
	size_t at = 0;

	for (unsigned i = 0; i < 9; i++) {
		many[at++] = (uint8_t)((i == 0 ? 0x80u : 0x00u) | 0x10u | 0x01u);
		many[at++] = 0x01;
		many[at++] = 0x01;
		many[at++] = 'T';
		many[at++] = (uint8_t)('0' + (char)i);
	}
	T_TRUE("neun Datensaetze sind zu viele", !osk_ndef_parse(many, at, rec, 8, &count));

	/* Falsche Aufrufe. */
	T_TRUE("NULL", !osk_ndef_parse(NULL, 5, rec, 8, &count));
	T_TRUE("kein Platz", !osk_ndef_parse(over, sizeof(over), rec, 0, &count));
}

/* ---- Bauen -------------------------------------------------------------- */

static void case_build(void)
{
	uint8_t file[OSK_NDEF_FILE_MAX];
	osk_ndef_record_t rec[OSK_NDEF_MAX_RECORDS];
	size_t count = 0;

	osk_ndef_reset();
	T_EQ("abgeschaltet baut nichts", osk_ndef_build_file(file, sizeof(file)), 0);

	set_uri("sorglos.org/key", false);
	const size_t n = osk_ndef_build_file(file, sizeof(file));

	T_EQ("Datei gebaut", n, 2 + 4 + 1 + 15);
	T_EQ("Laenge hoch", file[0], 0);
	T_EQ("Laenge tief", file[1], (uint8_t)(n - 2));

	/* Was gebaut wurde, muss der eigene Parser auch lesen koennen. */
	T_TRUE("eigene Datei ist lesbar", osk_ndef_parse(file + 2, n - 2, rec, 8, &count));
	T_EQ("ein Datensatz", count, 1);
	T_EQ("Typ U", rec[0].type[0], 'U');
	T_EQ("Praefix https://", rec[0].payload[0], OSK_NDEF_URI_HTTPS);
	T_TRUE("Adresse dahinter", memcmp(rec[0].payload + 1, "sorglos.org/key", 15) == 0);

	/* Kein Platz heisst 0, nicht eine halbe Datei. */
	T_EQ("zu klein", osk_ndef_build_file(file, 4), 0);
	T_EQ("gar kein Platz", osk_ndef_build_file(file, 1), 0);
}

static void case_config(void)
{
	osk_ndef_config_t cfg;

	osk_ndef_reset();
	memset(&cfg, 0, sizeof(cfg));
	cfg.mode = OSK_NDEF_URI;
	T_EQ("leere Adresse", osk_ndef_configure(&cfg), OSK_ERR_FORMAT);

	/* Steuerzeichen in der Adresse: abgewiesen. */
	cfg.uri[0] = 'a';
	cfg.uri[1] = 0x07;
	cfg.uri[2] = 'b';
	T_EQ("Steuerzeichen", osk_ndef_configure(&cfg), OSK_ERR_FORMAT);

	/* Unbekanntes Praefix. */
	memset(&cfg, 0, sizeof(cfg));
	cfg.mode = OSK_NDEF_URI;
	cfg.uri[0] = 'a';
	cfg.uri_prefix = 0x20;
	T_EQ("fremdes Praefix", osk_ndef_configure(&cfg), OSK_ERR_ARG);

	/* Unbekannte Betriebsart. */
	memset(&cfg, 0, sizeof(cfg));
	cfg.mode = (osk_ndef_mode_t)42;
	T_EQ("fremde Betriebsart", osk_ndef_configure(&cfg), OSK_ERR_ARG);

	T_EQ("NULL", osk_ndef_configure(NULL), OSK_ERR_ARG);

	/* Die abgeschaltete Betriebsart braucht keine Adresse. */
	memset(&cfg, 0, sizeof(cfg));
	cfg.mode = OSK_NDEF_OFF;
	T_EQ("abschalten geht immer", osk_ndef_configure(&cfg), OSK_OK);
	osk_ndef_get_config(&cfg);
	T_EQ("ist aus", cfg.mode, OSK_NDEF_OFF);
}

/* ---- Einmalkennwort ----------------------------------------------------- */

static unsigned g_otp_calls;
static const char *g_otp_text = "123456";

static size_t otp_source(char *out, size_t cap, void *ctx)
{
	(void)ctx;
	g_otp_calls++;
	size_t n = 0;

	while (g_otp_text[n] && n < cap) {
		out[n] = g_otp_text[n];
		n++;
	}
	return n;
}

static void set_otp(void)
{
	osk_ndef_config_t cfg;

	memset(&cfg, 0, sizeof(cfg));
	cfg.mode = OSK_NDEF_OTP;
	T_EQ("Kennwortbetrieb an", osk_ndef_configure(&cfg), OSK_OK);
	osk_ndef_set_otp_source(otp_source, NULL);
}

/* ---- Die Anwendung ------------------------------------------------------ */

static size_t select_file(uint16_t id, uint8_t *out, size_t cap)
{
	const uint8_t apdu[] = {0x00, 0xa4, 0x00, 0x0c, 0x02, (uint8_t)(id >> 8), (uint8_t)id};

	return osk_ndef_apdu(apdu, sizeof(apdu), out, cap);
}

static size_t read_binary(uint16_t offset, uint8_t le, uint8_t *out, size_t cap)
{
	const uint8_t apdu[] = {0x00, 0xb0, (uint8_t)(offset >> 8), (uint8_t)offset, le};

	return osk_ndef_apdu(apdu, sizeof(apdu), out, cap);
}

static uint16_t sw_of(const uint8_t *out, size_t len)
{
	if (len < 2)
		return 0;
	return (uint16_t)(((uint16_t)out[len - 2] << 8) | out[len - 1]);
}

static void case_app_off(void)
{
	uint8_t out[OSK_NDEF_FILE_MAX];

	mock_reset_all();
	osk_ndef_reset();
	size_t n = select_file(OSK_NDEF_CC_FILE_ID, out, sizeof(out));

	T_EQ("abgeschaltet: zwei Byte", n, 2);
	T_EQ("Datei nicht gefunden", sw_of(out, n), OSK_SW_FILE_NOT_FOUND);
}

static void case_app_cc(void)
{
	uint8_t out[OSK_NDEF_FILE_MAX];

	mock_reset_all();
	osk_ndef_reset();
	set_uri("sorglos.org/key", false);

	size_t n = select_file(OSK_NDEF_CC_FILE_ID, out, sizeof(out));

	T_EQ("Auswahl bestaetigt", sw_of(out, n), OSK_SW_OK);

	n = read_binary(0, 15, out, sizeof(out));
	T_EQ("15 Byte und Statuswort", n, 17);
	T_EQ("gelesen", sw_of(out, n), OSK_SW_OK);
	T_EQ("Laenge der Steuerdatei", out[1], 15);
	T_EQ("Abbildungsfassung 2.0", out[2], 0x20);
	T_EQ("Steuerblock der NDEF-Datei", out[7], 0x04);
	T_EQ("Dateikennung hoch", out[9], 0xe1);
	T_EQ("Dateikennung tief", out[10], 0x04);
	T_EQ("lesen erlaubt", out[13], 0x00);
	T_EQ("schreiben nie", out[14], 0xff);

	/* Hinter dem Ende wird nicht gelesen. */
	n = read_binary(100, 5, out, sizeof(out));
	T_EQ("Versatz hinter dem Ende", sw_of(out, n), OSK_SW_WRONG_P1P2);

	/* Genau am Ende: null Bytes, aber kein Fehler. */
	n = read_binary(15, 5, out, sizeof(out));
	T_EQ("am Ende nur Statuswort", n, 2);
	T_EQ("in Ordnung", sw_of(out, n), OSK_SW_OK);
}

static void case_app_read_needs_button(void)
{
	uint8_t out[OSK_NDEF_FILE_MAX];

	mock_reset_all();
	osk_ndef_reset();
	set_uri("sorglos.org/key", false);
	select_file(OSK_NDEF_DATA_FILE_ID, out, sizeof(out));

	/* Ohne Druck gibt es die Datei nicht. */
	mock_ui_set_confirm(OSK_ERR_DENIED);
	size_t n = read_binary(0, 0, out, sizeof(out));

	T_EQ("ohne Druck nur Statuswort", n, 2);
	T_EQ("abgelehnt", sw_of(out, n), OSK_SW_COND_NOT_SATISFIED);
	T_TRUE("gefragt wurde", mock_ui_confirm_count() >= 1);

	/* Mit Druck kommt sie. */
	mock_ui_set_confirm(OSK_OK);
	n = read_binary(0, 0, out, sizeof(out));
	T_EQ("gelesen", sw_of(out, n), OSK_SW_OK);
	T_TRUE("Datei ist da", n > 2);
	T_EQ("Laenge tief", out[1], (uint8_t)(n - 2 - 2));

	/* Ein zweites Lesen in derselben Sitzung fragt nicht noch einmal. */
	const unsigned before = mock_ui_confirm_count();

	n = read_binary(0, 4, out, sizeof(out));
	T_EQ("Teillesung geht", sw_of(out, n), OSK_SW_OK);
	T_EQ("vier Byte und Statuswort", n, 6);
	T_EQ("nicht erneut gefragt", mock_ui_confirm_count(), before);

	/* Nach dem Abwaehlen wird wieder gefragt. */
	osk_ndef_deselect();
	select_file(OSK_NDEF_DATA_FILE_ID, out, sizeof(out));
	n = read_binary(0, 0, out, sizeof(out));
	T_TRUE("wieder gefragt", mock_ui_confirm_count() > before);
	T_EQ("gelesen", sw_of(out, n), OSK_SW_OK);
}

static void case_app_without_button(void)
{
	uint8_t out[OSK_NDEF_FILE_MAX];

	/* Wer das reine Antippen ausdruecklich will, schaltet den Druck fuer
	 * die Adresse ab - und nur fuer sie. */
	mock_reset_all();
	osk_ndef_reset();
	set_uri("sorglos.org/key", true);
	mock_ui_set_confirm(OSK_ERR_DENIED);
	select_file(OSK_NDEF_DATA_FILE_ID, out, sizeof(out));
	size_t n = read_binary(0, 0, out, sizeof(out));

	T_EQ("gelesen ohne Druck", sw_of(out, n), OSK_SW_OK);
	T_EQ("nicht gefragt", mock_ui_confirm_count(), 0);
}

static void case_app_otp(void)
{
	uint8_t out[OSK_NDEF_FILE_MAX];
	osk_ndef_record_t rec[OSK_NDEF_MAX_RECORDS];
	size_t count = 0;

	mock_reset_all();
	osk_ndef_reset();
	g_otp_calls = 0;
	set_otp();

	/* Der Schalter fuer das reine Antippen gilt fuer das Kennwort NICHT:
	 * ein Geheimnis kommt nur nach dem Druck heraus. */
	osk_ndef_config_t cfg;

	osk_ndef_get_config(&cfg);
	cfg.uri_without_button = true;
	T_EQ("Einstellung gesetzt", osk_ndef_configure(&cfg), OSK_OK);
	osk_ndef_set_otp_source(otp_source, NULL);

	mock_ui_set_confirm(OSK_ERR_DENIED);
	select_file(OSK_NDEF_DATA_FILE_ID, out, sizeof(out));
	size_t n = read_binary(0, 0, out, sizeof(out));

	T_EQ("ohne Druck kein Kennwort", sw_of(out, n), OSK_SW_COND_NOT_SATISFIED);
	T_EQ("die Quelle wurde nicht gefragt", g_otp_calls, 0);
	T_TRUE("der Taster wurde gefragt", mock_ui_confirm_count() >= 1);

	/* Mit Druck: ein Text-Datensatz mit dem Kennwort. */
	mock_ui_set_confirm(OSK_OK);
	n = read_binary(0, 0, out, sizeof(out));
	T_EQ("gelesen", sw_of(out, n), OSK_SW_OK);
	T_EQ("Quelle einmal gefragt", g_otp_calls, 1);
	T_TRUE("lesbar", osk_ndef_parse(out + 2, n - 4, rec, 8, &count));
	T_EQ("ein Datensatz", count, 1);
	T_EQ("Typ T", rec[0].type[0], 'T');
	T_EQ("Statusbyte UTF-8 mit zwei Sprachzeichen", rec[0].payload[0], 0x02);
	T_TRUE("Sprache en", rec[0].payload[1] == 'e' && rec[0].payload[2] == 'n');
	T_TRUE("Kennwort dahinter", memcmp(rec[0].payload + 3, "123456", 6) == 0);

	/* In derselben Sitzung wird das Kennwort nicht neu gezogen - sonst
	 * liest das Handy zwei halbe Kennwoerter. */
	n = read_binary(2, 4, out, sizeof(out));
	T_EQ("Teillesung", sw_of(out, n), OSK_SW_OK);
	T_EQ("Quelle nicht erneut gefragt", g_otp_calls, 1);

	/* Eine Quelle, die nichts liefert, ergibt keine Datei. */
	osk_ndef_deselect();
	g_otp_text = "";
	select_file(OSK_NDEF_DATA_FILE_ID, out, sizeof(out));
	n = read_binary(0, 0, out, sizeof(out));
	T_EQ("keine Datei", sw_of(out, n), OSK_SW_COND_NOT_SATISFIED);

	/* Und eine Quelle mit Steuerzeichen ebenso. */
	osk_ndef_deselect();
	g_otp_text = "12\n34";
	select_file(OSK_NDEF_DATA_FILE_ID, out, sizeof(out));
	n = read_binary(0, 0, out, sizeof(out));
	T_EQ("Steuerzeichen abgewiesen", sw_of(out, n), OSK_SW_COND_NOT_SATISFIED);
	g_otp_text = "123456";

	/* Ohne angemeldete Quelle auch nicht. */
	osk_ndef_deselect();
	osk_ndef_set_otp_source(NULL, NULL);
	select_file(OSK_NDEF_DATA_FILE_ID, out, sizeof(out));
	n = read_binary(0, 0, out, sizeof(out));
	T_EQ("ohne Quelle keine Datei", sw_of(out, n), OSK_SW_COND_NOT_SATISFIED);
}

static void case_app_errors(void)
{
	uint8_t out[OSK_NDEF_FILE_MAX];

	mock_reset_all();
	osk_ndef_reset();
	set_uri("sorglos.org/key", true);
	mock_ui_set_confirm(OSK_OK);

	/* Schreiben gibt es nicht - und das wird gesagt, nicht verschwiegen. */
	const uint8_t update[] = {0x00, 0xd6, 0x00, 0x00, 0x02, 0xaa, 0xbb};
	size_t n = osk_ndef_apdu(update, sizeof(update), out, sizeof(out));

	T_EQ("Schreiben abgelehnt", sw_of(out, n), OSK_SW_COND_NOT_SATISFIED);

	/* Unbekannter Befehl. */
	const uint8_t weird[] = {0x00, 0x77, 0x00, 0x00};

	n = osk_ndef_apdu(weird, sizeof(weird), out, sizeof(out));
	T_EQ("unbekannter Befehl", sw_of(out, n), OSK_SW_INS_NOT_SUPPORTED);

	/* Fremdes Klassenbyte. */
	const uint8_t other_cla[] = {0x80, 0xb0, 0x00, 0x00, 0x05};

	n = osk_ndef_apdu(other_cla, sizeof(other_cla), out, sizeof(out));
	T_EQ("fremdes Klassenbyte", sw_of(out, n), OSK_SW_CLA_NOT_SUPPORTED);

	/* Kaputte APDU. */
	const uint8_t broken[] = {0x00, 0xb0, 0x00};

	n = osk_ndef_apdu(broken, sizeof(broken), out, sizeof(out));
	T_EQ("kaputte APDU", sw_of(out, n), OSK_SW_WRONG_LENGTH);

	/* Auswahl mit falschen Parametern. */
	const uint8_t bad_p[] = {0x00, 0xa4, 0x00, 0x00, 0x02, 0xe1, 0x04};

	n = osk_ndef_apdu(bad_p, sizeof(bad_p), out, sizeof(out));
	T_EQ("falsche Parameter", sw_of(out, n), OSK_SW_WRONG_P1P2);

	/* Auswahl mit falscher Laenge. */
	const uint8_t bad_len[] = {0x00, 0xa4, 0x00, 0x0c, 0x01, 0xe1};

	n = osk_ndef_apdu(bad_len, sizeof(bad_len), out, sizeof(out));
	T_EQ("falsche Laenge", sw_of(out, n), OSK_SW_WRONG_LENGTH);

	/* Unbekannte Datei. */
	n = select_file(0xe199, out, sizeof(out));
	T_EQ("unbekannte Datei", sw_of(out, n), OSK_SW_FILE_NOT_FOUND);

	/* Lesen ohne Auswahl. */
	osk_ndef_deselect();
	n = read_binary(0, 5, out, sizeof(out));
	T_EQ("ohne Auswahl", sw_of(out, n), OSK_SW_COND_NOT_SATISFIED);

	/* Zu kleiner Ausgabepuffer: nichts, nicht etwas Halbes. */
	uint8_t tiny[1];

	T_EQ("kein Platz", osk_ndef_apdu(update, sizeof(update), tiny, sizeof(tiny)), 0);
	T_EQ("NULL", osk_ndef_apdu(NULL, 4, out, sizeof(out)), 0);
}

int main(void)
{
	case_parse_good();
	case_parse_bad();
	case_build();
	case_config();
	case_app_off();
	case_app_cc();
	case_app_read_needs_button();
	case_app_without_button();
	case_app_otp();
	case_app_errors();
	return t_report("test_ndef");
}
