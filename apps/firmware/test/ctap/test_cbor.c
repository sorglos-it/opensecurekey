/* Der CBOR-Leser ist die erste Wand gegen einen Angreifer. Diese Faelle
 * pruefen, dass er genau die kanonische Form annimmt und sonst nichts -
 * und dass keine Laengenangabe aus der Nachricht ihn ueber den Puffer
 * hinausliest. */
#include "test_util.h"
#include "osk_cbor.h"

static void ok_bytes(const char *what, const char *hex)
{
	uint8_t buf[256];
	const size_t n = t_unhex(hex, buf, sizeof(buf));
	T_TRUE("Hex lesbar", n > 0);
	T_EQ(what, osk_cbor_validate(buf, n), OSK_OK);
}

static void bad_bytes(const char *what, const char *hex)
{
	uint8_t buf[256];
	const size_t n = t_unhex(hex, buf, sizeof(buf));
	T_TRUE("Hex lesbar", n > 0);
	T_EQ(what, osk_cbor_validate(buf, n), OSK_ERR_FORMAT);
}

static void test_validate(void)
{
	ok_bytes("0 ist gueltig", "00");
	ok_bytes("23 ist gueltig", "17");
	ok_bytes("24 in einem Zusatzbyte", "1818");
	ok_bytes("leere Liste", "80");
	ok_bytes("leere Abbildung", "a0");
	ok_bytes("Abbildung 1:2", "a10102");
	ok_bytes("sortierte Schluessel", "a2010102 02" "");
	ok_bytes("Text", "63616263");
	ok_bytes("Bytes", "43010203");
	ok_bytes("true/false/null", "83f5f4f6");
	ok_bytes("negative Zahl", "20");

	/* Nicht kleinste Kodierung */
	bad_bytes("24 im Zusatzbyte zu gross kodiert", "1817");
	bad_bytes("Zwei-Byte-Laenge fuer kleinen Wert", "1900ff");
	bad_bytes("Vier-Byte-Laenge fuer kleinen Wert", "1a0000ffff");
	/* 64-Bit und unbestimmte Laenge */
	bad_bytes("64-Bit-Zahl", "1b0000000000000001");
	bad_bytes("unbestimmte Liste", "9fff");
	bad_bytes("unbestimmter Text", "7f6161ff");
	/* Tags und Fliesskomma */
	bad_bytes("Tag", "c074323031332d30332d32315432303a30343a30305a");
	bad_bytes("Fliesskomma halb", "f93c00");
	bad_bytes("Fliesskomma einfach", "fa47c35000");
	/* Sortierung und Doppelung */
	bad_bytes("unsortierte Schluessel", "a2020101 01");
	bad_bytes("doppelter Schluessel", "a201010102");
	/* Laengen, die ueber den Puffer zeigen */
	bad_bytes("Textlaenge zu gross", "6361");
	bad_bytes("Bytelaenge zu gross", "4401020304050607");
	bad_bytes("Liste verspricht mehr als da ist", "8301");
	bad_bytes("Abbildung verspricht mehr als da ist", "a201");
	/* Nachlaufende Bytes */
	bad_bytes("etwas haengt hinten dran", "0000");
	/* Zu tief: fuenf Ebenen */
	bad_bytes("fuenf Ebenen", "81818181 8100");
}

static void test_reader(void)
{
	/* a2 01 18ff 02 43 414243  =  {1: 255, 2: h'414243'} */
	uint8_t buf[32];
	const size_t n = t_unhex("a20118ff0243414243", buf, sizeof(buf));
	T_TRUE("Vorlage lesbar", n > 0);
	T_EQ("gueltig", osk_cbor_validate(buf, n), OSK_OK);

	osk_cbor_reader_t r;
	osk_cbor_reader_init(&r, buf, n);
	size_t count = 0;
	T_EQ("Abbildung betreten", osk_cbor_enter_map(&r, &count), OSK_OK);
	T_EQ("zwei Paare", (long long)count, 2);

	uint64_t key = 0, val = 0;
	T_EQ("Schluessel 1", osk_cbor_read_uint(&r, &key), OSK_OK);
	T_EQ("Schluessel ist 1", (long long)key, 1);
	T_EQ("Wert lesen", osk_cbor_read_uint(&r, &val), OSK_OK);
	T_EQ("Wert ist 255", (long long)val, 255);

	T_EQ("Schluessel 2", osk_cbor_read_uint(&r, &key), OSK_OK);
	T_EQ("Schluessel ist 2", (long long)key, 2);
	const uint8_t *p = NULL;
	size_t plen = 0;
	T_EQ("Bytes lesen", osk_cbor_read_bytes(&r, &p, &plen), OSK_OK);
	T_EQ("drei Bytes", (long long)plen, 3);
	T_TRUE("Inhalt ABC", plen == 3 && memcmp(p, "ABC", 3) == 0);
	osk_cbor_leave(&r);

	/* Falscher Typ liefert einen Fehler und verbraucht nichts. */
	osk_cbor_reader_init(&r, buf, n);
	const char *txt = NULL;
	T_EQ("Text statt Abbildung", osk_cbor_read_text(&r, &txt, &plen), OSK_ERR_FORMAT);
	T_EQ("Leseposition unberuehrt", (long long)r.pos, 0);

	/* Negative Zahlen */
	uint8_t nb[4];
	const size_t nn = t_unhex("38ff", nb, sizeof(nb));   /* -256 */
	osk_cbor_reader_init(&r, nb, nn);
	int64_t iv = 0;
	T_EQ("negativ lesen", osk_cbor_read_int(&r, &iv), OSK_OK);
	T_EQ("minus 256", (long long)iv, -256);
}

static void test_writer(void)
{
	uint8_t buf[64];
	osk_cbor_writer_t w;

	osk_cbor_writer_init(&w, buf, sizeof(buf));
	osk_cbor_put_map(&w, 2);
	osk_cbor_put_uint(&w, 1);
	osk_cbor_put_uint(&w, 255);
	osk_cbor_put_uint(&w, 2);
	osk_cbor_put_bytes(&w, (const uint8_t *)"ABC", 3);
	T_TRUE("Schreiber heil", osk_cbor_writer_ok(&w));
	T_HEX("Schreiber kanonisch", buf, w.len, "a20118ff0243414243");
	/* Was der Schreiber ausgibt, muss der strenge Leser annehmen. */
	T_EQ("eigene Ausgabe ist kanonisch", osk_cbor_validate(buf, w.len), OSK_OK);

	/* Negative Zahlen und COSE-Schluessel */
	osk_cbor_writer_init(&w, buf, sizeof(buf));
	osk_cbor_put_int(&w, -7);
	T_HEX("minus sieben", buf, w.len, "26");

	/* Ueberlauf: der Schreiber wird kaputt und bleibt es. */
	uint8_t tiny[3];
	osk_cbor_writer_init(&w, tiny, sizeof(tiny));
	osk_cbor_put_bytes(&w, (const uint8_t *)"ABCDEF", 6);
	T_TRUE("Ueberlauf erkannt", !osk_cbor_writer_ok(&w));
	osk_cbor_put_uint(&w, 1);
	T_TRUE("bleibt kaputt", !osk_cbor_writer_ok(&w));
}

static void test_skip(void)
{
	uint8_t buf[64];
	const size_t n = t_unhex("a201a1026161026162", buf, sizeof(buf));
	T_TRUE("Vorlage lesbar", n > 0);

	osk_cbor_reader_t r;
	osk_cbor_reader_init(&r, buf, n);
	size_t count = 0;
	T_EQ("betreten", osk_cbor_enter_map(&r, &count), OSK_OK);
	uint64_t key = 0;
	T_EQ("Schluessel", osk_cbor_read_uint(&r, &key), OSK_OK);
	T_EQ("verschachtelten Wert ueberspringen", osk_cbor_skip(&r), OSK_OK);
	T_EQ("zweiter Schluessel", osk_cbor_read_uint(&r, &key), OSK_OK);
	T_EQ("Schluessel ist 2", (long long)key, 2);
	osk_cbor_leave(&r);
}

static void test_key_order(void)
{
	/* Kuerzer sortiert vor laenger, danach byteweise. */
	const uint8_t a[1] = { 0x01 };          /* 1 */
	const uint8_t b[2] = { 0x18, 0xff };    /* 255 */
	const uint8_t t3[4] = { 0x63, 'a', 'b', 'c' };
	T_TRUE("1 vor 255", osk_cbor_key_cmp(a, 1, b, 2) < 0);
	T_TRUE("Zahl vor Text", osk_cbor_key_cmp(a, 1, t3, 4) < 0);
	T_TRUE("gleich ist gleich", osk_cbor_key_cmp(a, 1, a, 1) == 0);
}

int main(void)
{
	test_validate();
	test_reader();
	test_writer();
	test_skip();
	test_key_order();
	return t_report("cbor");
}
