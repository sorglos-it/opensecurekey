/* Die Seriennummer: eine je Geraet, ueber die Anwendungen hinweg
 * dieselbe, und keine erfundene, wenn der Chip nicht antwortet.
 *
 * Am PC gegen Attrappen - Stufe 2 von vier. Dass die Nummer am Geraet
 * wirklich je Stueck verschieden ist, haengt daran, dass das
 * Grundgeheimnis im Chip je Stueck verschieden ist, und das ist erst mit
 * Hardware messbar. */
#include "test_util.h"
#include "osk_serial.h"
#include "osk_openpgp.h"
#include "osk_applets.h"
#include "mocks.h"

/* Der Platz der Seriennummer in der OpenPGP-Kennung (Abschnitt 4.2.1:
 * RID 5, Anwendung 1, Fassung 2, Hersteller 2, dann vier Bytes). Steht
 * hier zum zweiten Mal, damit die Pruefung gegen die SPEZIFIKATION
 * zaehlt und nicht gegen die Rechnung im Code. */
#define AID_SERIAL_AT 10u

static void test_device(void)
{
	uint8_t a[OSK_SERIAL_LEN];
	uint8_t b[OSK_SERIAL_LEN];
	bool all_zero = true;

	mock_reset_all();
	osk_serial_forget();

	T_TRUE("vor dem ersten Lesen unbekannt", !osk_serial_known());
	T_EQ("Nummer kommt", osk_serial_device(a), OSK_OK);
	T_TRUE("jetzt bekannt", osk_serial_known());
	for (size_t i = 0; i < sizeof(a); i++)
		all_zero = all_zero && (a[i] == 0u);
	T_TRUE("nicht alles Null", !all_zero);

	/* Zweites Lesen: derselbe Wert, und ohne den Chip noch einmal zu
	 * fragen. */
	T_EQ("zweites Lesen", osk_serial_device(b), OSK_OK);
	T_TRUE("gleicher Wert", memcmp(a, b, sizeof(a)) == 0);

	/* Karte aus und wieder an: der Wert muss WIEDER derselbe sein. Eine
	 * Nummer, die sich beim Einstecken aendert, ist keine. */
	osk_serial_forget();
	T_TRUE("nach dem Vergessen unbekannt", !osk_serial_known());
	T_EQ("neu geholt", osk_serial_device(b), OSK_OK);
	T_TRUE("ueber den Neustart stabil", memcmp(a, b, sizeof(a)) == 0);
}

static void test_short(void)
{
	uint8_t full[OSK_SERIAL_LEN];
	uint8_t s[OSK_SERIAL_SHORT_LEN];

	mock_reset_all();
	osk_serial_forget();

	T_EQ("lang", osk_serial_device(full), OSK_OK);
	T_EQ("kurz", osk_serial_short(s), OSK_OK);
	T_EQ("vier Byte", (int)OSK_SERIAL_SHORT_LEN, 4);
	T_TRUE("die ersten vier der langen", memcmp(s, full, sizeof(s)) == 0);
	T_TRUE("nicht vier Nullen",
	       !(s[0] == 0u && s[1] == 0u && s[2] == 0u && s[3] == 0u));
}

/* Antwortet der Chip nicht, gibt es KEINE Nummer - und keine erfundene.
 * Eine Ersatznummer waere auf allen Geraeten wieder dieselbe, und damit
 * waere genau die Luecke zurueck, die hier geschlossen wird. */
static void test_no_chip(void)
{
	uint8_t a[OSK_SERIAL_LEN];
	uint8_t s[OSK_SERIAL_SHORT_LEN];

	mock_reset_all();
	osk_serial_forget();
	mock_se050_set_hkdf_fail(true);

	memset(a, 0xeeu, sizeof(a));
	T_TRUE("ohne Chip keine Nummer", osk_serial_device(a) != OSK_OK);
	T_TRUE("und nichts geschrieben", a[0] == 0xeeu);
	T_TRUE("gilt weiter als unbekannt", !osk_serial_known());
	memset(s, 0xeeu, sizeof(s));
	T_TRUE("kurze Form auch nicht", osk_serial_short(s) != OSK_OK);

	/* Antwortet der Chip wieder, kommt die Nummer - ohne Neustart. */
	mock_se050_set_hkdf_fail(false);
	T_EQ("danach geht es", osk_serial_device(a), OSK_OK);
	T_TRUE("jetzt bekannt", osk_serial_known());
}

/* Die Luecke, um die es ging: GnuPG unterscheidet Karten an der
 * Seriennummer in der Kennung. Bis zum 29.10.2026 stand dort auf jedem
 * Geraet 00 53 4B 01. */
static void test_aid(void)
{
	const uint8_t *aid;
	uint8_t s[OSK_SERIAL_SHORT_LEN];
	static const uint8_t old_fixed[4] = {0x00u, 0x53u, 0x4bu, 0x01u};

	mock_reset_all();
	osk_serial_forget();
	T_EQ("Anwendungen bereit", osk_applets_init(), OSK_OK);

	T_EQ("kurze Nummer", osk_serial_short(s), OSK_OK);
	aid = osk_openpgp_aid();
	T_TRUE("Kennung da", aid != NULL);
	T_HEX("RID und Anwendung unveraendert", aid, 6u, "d27600012401");
	T_EQ("Fassung 3.4 oben", aid[6], 0x03u);
	T_EQ("Fassung 3.4 unten", aid[7], 0x04u);
	T_TRUE("Seriennummer steht in der Kennung",
	       memcmp(aid + AID_SERIAL_AT, s, sizeof(s)) == 0);
	T_TRUE("und ist nicht mehr die feste von damals",
	       memcmp(aid + AID_SERIAL_AT, old_fixed, sizeof(old_fixed)) != 0);
	T_EQ("Kennung ist 16 Byte lang", (int)OSK_PGP_AID_LEN, 16);
}

int main(void)
{
	setvbuf(stdout, NULL, _IONBF, 0);
	mock_reset_all();

	test_device();
	test_short();
	test_no_chip();
	test_aid();
	return t_report("test_serial");
}
