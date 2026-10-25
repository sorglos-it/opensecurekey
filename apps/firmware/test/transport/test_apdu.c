/* Pruefungen der APDU-Zerlegung: alle vier Faelle aus ISO/IEC 7816-4, kurz
 * und erweitert, und daneben die Formen, die ein Angreifer schickt.
 *
 * Was hier bewiesen wird: eine APDU, deren Laengenfelder nicht zum Rahmen
 * passen, wird abgewiesen - und zwar BEVOR ein Zeiger auf die Daten
 * gesetzt wird. */
#include "test_util.h"
#include "nfc_apdu.h"

static void case_lengths(void)
{
	osk_apdu_t c;

	/* Fall 1: nur der Kopf. */
	const uint8_t c1[] = {0x00, 0xa4, 0x04, 0x00};

	T_TRUE("Fall 1 geht", osk_nfc_apdu_parse(c1, sizeof(c1), &c));
	T_EQ("Fall 1 ohne Lc", c.lc, 0);
	T_TRUE("Fall 1 ohne Le", !c.has_le);
	T_TRUE("Fall 1 nicht erweitert", !c.extended);

	/* Fall 2 kurz: nur Le. 0 heisst 256. */
	const uint8_t c2[] = {0x00, 0xb0, 0x00, 0x00, 0x00};

	T_TRUE("Fall 2 geht", osk_nfc_apdu_parse(c2, sizeof(c2), &c));
	T_EQ("Le 0 heisst 256", c.le, 256);
	T_TRUE("Le ist da", c.has_le);

	const uint8_t c2b[] = {0x00, 0xb0, 0x00, 0x00, 0x0f};

	T_TRUE("Fall 2 mit Zahl", osk_nfc_apdu_parse(c2b, sizeof(c2b), &c));
	T_EQ("Le 15", c.le, 15);

	/* Fall 3 kurz: nur Daten. */
	const uint8_t c3[] = {0x00, 0xa4, 0x04, 0x00, 0x03, 0xaa, 0xbb, 0xcc};

	T_TRUE("Fall 3 geht", osk_nfc_apdu_parse(c3, sizeof(c3), &c));
	T_EQ("Lc 3", c.lc, 3);
	T_TRUE("Daten zeigen hinter den Kopf", c.data == c3 + 5);
	T_TRUE("Fall 3 ohne Le", !c.has_le);

	/* Fall 4 kurz: Daten und Le. */
	const uint8_t c4[] = {0x00, 0xa4, 0x04, 0x00, 0x02, 0xaa, 0xbb, 0x10};

	T_TRUE("Fall 4 geht", osk_nfc_apdu_parse(c4, sizeof(c4), &c));
	T_EQ("Lc 2", c.lc, 2);
	T_EQ("Le 16", c.le, 16);

	/* Fall 2 erweitert. */
	const uint8_t e2[] = {0x80, 0x11, 0x00, 0x00, 0x00, 0x01, 0x00};

	T_TRUE("Fall 2 erweitert", osk_nfc_apdu_parse(e2, sizeof(e2), &c));
	T_TRUE("erweitert erkannt", c.extended);
	T_EQ("Le 256 erweitert", c.le, 256);

	const uint8_t e2b[] = {0x80, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00};

	T_TRUE("Fall 2 erweitert mit 0", osk_nfc_apdu_parse(e2b, sizeof(e2b), &c));
	T_EQ("Le 0 heisst 65536", c.le, 65536);

	/* Fall 3 erweitert. */
	const uint8_t e3[] = {0x80, 0x10, 0x00, 0x00, 0x00, 0x00, 0x02, 0xaa, 0xbb};

	T_TRUE("Fall 3 erweitert", osk_nfc_apdu_parse(e3, sizeof(e3), &c));
	T_EQ("Lc 2 erweitert", c.lc, 2);
	T_TRUE("Daten hinter dem langen Kopf", c.data == e3 + 7);
	T_TRUE("kein Le", !c.has_le);

	/* Fall 4 erweitert. */
	const uint8_t e4[] = {0x80, 0x10, 0x00, 0x00, 0x00, 0x00, 0x02, 0xaa, 0xbb, 0x00, 0x40};

	T_TRUE("Fall 4 erweitert", osk_nfc_apdu_parse(e4, sizeof(e4), &c));
	T_EQ("Lc 2", c.lc, 2);
	T_EQ("Le 64", c.le, 64);
}

static void case_broken(void)
{
	osk_apdu_t c;

	const uint8_t too_short[] = {0x00, 0xa4, 0x04};

	T_TRUE("drei Bytes sind keine APDU", !osk_nfc_apdu_parse(too_short, sizeof(too_short), &c));

	/* Sechs Bytes: Lc 1 angekuendigt, aber nur ein Byte dahinter waere
	 * Fall 3 - hier steht ein Byte zu viel ohne Le-Platz. */
	const uint8_t six[] = {0x00, 0xa4, 0x04, 0x00, 0x05, 0xaa};

	T_TRUE("Lc 5 mit einem Byte", !osk_nfc_apdu_parse(six, sizeof(six), &c));

	/* Lc kuendigt mehr an, als da ist. Das ist der Fall, der ohne
	 * Pruefung ueber den Puffer liest. */
	const uint8_t lies[] = {0x00, 0xa4, 0x04, 0x00, 0xff, 0xaa, 0xbb};

	T_TRUE("Lc luegt", !osk_nfc_apdu_parse(lies, sizeof(lies), &c));
	T_TRUE("kein Zeiger auf Daten", c.data == NULL);
	T_EQ("kein Lc", c.lc, 0);

	/* Ein Byte zu viel hinter Fall 4. */
	const uint8_t tail[] = {0x00, 0xa4, 0x04, 0x00, 0x01, 0xaa, 0x10, 0x99};

	T_TRUE("Anhaengsel wird abgewiesen", !osk_nfc_apdu_parse(tail, sizeof(tail), &c));

	/* Erweitert, aber nur sechs Bytes. */
	const uint8_t short_ext[] = {0x80, 0x10, 0x00, 0x00, 0x00, 0x00};

	T_TRUE("erweiterter Kopf unvollstaendig",
	       !osk_nfc_apdu_parse(short_ext, sizeof(short_ext), &c));

	/* Erweitert mit Lc 0 ist nicht vergeben. */
	const uint8_t ext_zero[] = {0x80, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0xaa};

	T_TRUE("erweitertes Lc 0", !osk_nfc_apdu_parse(ext_zero, sizeof(ext_zero), &c));

	/* Erweitert, Lc luegt. */
	const uint8_t ext_lies[] = {0x80, 0x10, 0x00, 0x00, 0x00, 0xff, 0xff, 0xaa};

	T_TRUE("erweitertes Lc luegt", !osk_nfc_apdu_parse(ext_lies, sizeof(ext_lies), &c));

	T_TRUE("NULL ist keine APDU", !osk_nfc_apdu_parse(NULL, 5, &c));
}

static void case_chaining(void)
{
	osk_apdu_t c;

	const uint8_t plain[] = {0x00, 0xa4, 0x04, 0x00, 0x01, 0xaa};

	T_TRUE("ohne Kette geht", osk_nfc_apdu_parse(plain, sizeof(plain), &c));
	T_TRUE("ohne Kettenbit", !c.chained);

	/* Kettenbit nach ISO/IEC 7816-4. */
	const uint8_t iso[] = {0x10, 0xa4, 0x04, 0x00, 0x01, 0xaa};

	T_TRUE("Kettenbit geht", osk_nfc_apdu_parse(iso, sizeof(iso), &c));
	T_TRUE("Kette erkannt", c.chained);
	T_TRUE("nicht die CTAP-Form", !c.chaining_cla);

	/* Die Form, die CTAP benutzt. */
	const uint8_t ctap[] = {0x90, 0x10, 0x00, 0x00, 0x01, 0xaa};

	T_TRUE("CTAP-Kette geht", osk_nfc_apdu_parse(ctap, sizeof(ctap), &c));
	T_TRUE("Kette erkannt", c.chained);
	T_TRUE("CTAP-Form erkannt", c.chaining_cla);
}

static void case_status(void)
{
	uint8_t out[4];

	T_EQ("Statuswort allein", osk_apdu_only_status(out, sizeof(out), OSK_SW_OK), 2);
	T_EQ("hohes Byte", out[0], 0x90);
	T_EQ("tiefes Byte", out[1], 0x00);

	out[0] = 0xaa;
	T_EQ("Statuswort hinter Daten", osk_apdu_status(out, sizeof(out), 1, 0x6a82), 3);
	T_EQ("Daten bleiben", out[0], 0xaa);
	T_EQ("Fehler hoch", out[1], 0x6a);
	T_EQ("Fehler tief", out[2], 0x82);

	/* Kein Platz heisst 0, nicht ein halbes Statuswort. */
	uint8_t tiny[1];

	T_EQ("zu klein", osk_apdu_only_status(tiny, sizeof(tiny), OSK_SW_OK), 0);
	T_EQ("Laenge groesser als Platz", osk_apdu_status(out, 4, 5, OSK_SW_OK), 0);
}

int main(void)
{
	case_lengths();
	case_broken();
	case_chaining();
	case_status();
	return t_report("test_apdu");
}
