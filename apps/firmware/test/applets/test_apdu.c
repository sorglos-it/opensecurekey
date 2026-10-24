/* Der APDU-Parser und der TLV-Bau. Das ist der Teil, den ein Angreifer
 * zuerst erreicht: jedes Byte vom Kartenleser laeuft hier durch.
 *
 * Geprueft wird gegen ISO/IEC 7816-4 Abschnitt 5.1 (die vier Faelle, kurz
 * und erweitert) und 5.2 (BER-TLV). Besonderes Augenmerk liegt auf dem,
 * was NICHT angenommen werden darf. */
#include "test_util.h"
#include "osk_apdu.h"

static osk_result_t parse(const char *hex, osk_apdu_t *out)
{
	/* Der Puffer muss den Aufruf ueberleben: `data` im Ergebnis zeigt
	 * hinein, so wie im Betrieb auf den Puffer des Transports. */
	static uint8_t buf[512];
	const size_t len = t_unhex(hex, buf, sizeof(buf));

	/* Laenge 0 gibt es als Vorlage nicht - ausser der Text ist leer. */
	if (len == 0u && hex[0] != 0)
		return OSK_ERR_INTERNAL;
	return osk_apdu_parse(buf, len, out);
}

static void test_cases(void)
{
	osk_apdu_t a;

	/* Fall 1: nur der Kopf. */
	T_EQ("Fall 1", parse("00a40400", &a), OSK_OK);
	T_EQ("Fall 1 CLA", a.cla, 0x00);
	T_EQ("Fall 1 INS", a.ins, 0xa4);
	T_EQ("Fall 1 Lc", a.lc, 0);
	T_TRUE("Fall 1 ohne Le", !a.has_le);

	/* Fall 2 kurz: ein Le-Byte. */
	T_EQ("Fall 2S", parse("00ca004f10", &a), OSK_OK);
	T_TRUE("Fall 2S hat Le", a.has_le);
	T_EQ("Fall 2S Le", a.le, 0x10);
	T_EQ("Fall 2S Lc", a.lc, 0);

	/* Le = 0x00 heisst 256. */
	T_EQ("Fall 2S mit 00", parse("00ca004f00", &a), OSK_OK);
	T_EQ("Le 00 ist 256", a.le, 256);

	/* Fall 3 kurz: Daten, kein Le. */
	T_EQ("Fall 3S", parse("0020008008313233343536ffff", &a), OSK_OK);
	T_EQ("Fall 3S Lc", a.lc, 8);
	T_TRUE("Fall 3S ohne Le", !a.has_le);
	T_EQ("Fall 3S erstes Byte", a.data[0], 0x31);

	/* Fall 4 kurz: Daten und Le. */
	T_EQ("Fall 4S", parse("0087119a057c0381010101", &a), OSK_OK);
	T_EQ("Fall 4S Lc", a.lc, 5);
	T_EQ("Fall 4S erstes Byte", a.data[0], 0x7c);
	T_TRUE("Fall 4S hat Le", a.has_le);
	T_EQ("Fall 4S Le", a.le, 1);

	/* Fall 2 erweitert: 00 dann zwei Bytes Le. */
	T_EQ("Fall 2E", parse("00cb3fff000100", &a), OSK_OK);
	T_TRUE("Fall 2E erweitert", a.extended);
	T_EQ("Fall 2E Le", a.le, 0x0100);
	T_EQ("Fall 2E Lc", a.lc, 0);

	/* Fall 3 erweitert. */
	T_EQ("Fall 3E", parse("00db3fff000003010203", &a), OSK_OK);
	T_TRUE("Fall 3E erweitert", a.extended);
	T_EQ("Fall 3E Lc", a.lc, 3);
	T_TRUE("Fall 3E ohne Le", !a.has_le);

	/* Fall 4 erweitert. */
	T_EQ("Fall 4E", parse("00db3fff0000030102030200", &a), OSK_OK);
	T_EQ("Fall 4E Lc", a.lc, 3);
	T_EQ("Fall 4E Le", a.le, 0x0200);

	/* Le = 0x0000 erweitert heisst 65536 - mehr als wir annehmen, also
	 * auf unsere Obergrenze gekappt statt abgelehnt. */
	T_EQ("Fall 2E mit 0000", parse("00cb3fff000000", &a), OSK_OK);
	T_EQ("Le 0000 gekappt", a.le, OSK_APDU_MAX_RESP);

	/* Verkettung und logischer Kanal stecken im CLA. */
	T_EQ("Verkettet", parse("10db3fff03010203", &a), OSK_OK);
	T_TRUE("Verkettung erkannt", a.chained);
	T_EQ("Kanal 0", a.channel, 0);
	T_EQ("Kanal 2", parse("02a4040002aabb", &a), OSK_OK);
	T_EQ("Kanal gelesen", a.channel, 2);
}

static void test_rejects(void)
{
	osk_apdu_t a;
	uint8_t buf[8];

	/* Zu kurz. */
	T_EQ("drei Bytes", osk_apdu_parse((const uint8_t *)"\x00\xa4\x04", 3u, &a), OSK_ERR_FORMAT);
	T_EQ("nichts", osk_apdu_parse(buf, 0u, &a), OSK_ERR_FORMAT);
	T_EQ("Nullzeiger", osk_apdu_parse(NULL, 4u, &a), OSK_ERR_ARG);
	T_EQ("kein Ziel", osk_apdu_parse(buf, 4u, NULL), OSK_ERR_ARG);

	/* CLA 0xFF ist nach 7816-4 nie gueltig. */
	T_EQ("CLA FF", parse("ffa40400", &a), OSK_ERR_FORMAT);

	/* Lc kuendigt mehr an, als da ist. */
	T_EQ("Lc zu gross", parse("0020008008313233", &a), OSK_ERR_FORMAT);
	/* Ein Byte zu viel hinter Lc und Le. */
	T_EQ("ein Byte zu viel", parse("002000800131ff00", &a), OSK_ERR_FORMAT);
	/* Erweitert: Lc = 0 gibt es nicht. */
	T_EQ("Lc 0 erweitert", parse("00db3fff0000000102", &a), OSK_ERR_FORMAT);
	/* Erweitert: abgeschnittenes Laengenfeld. */
	T_EQ("halbes Lc", parse("00db3fff0001", &a), OSK_ERR_FORMAT);

	/* Mehr Daten als wir annehmen: 4096 angekuendigt. */
	{
		uint8_t big[16];

		big[0] = 0x00u;
		big[1] = 0xdbu;
		big[2] = 0x3fu;
		big[3] = 0xffu;
		big[4] = 0x00u;
		big[5] = 0x10u; /* 4096 */
		big[6] = 0x00u;
		big[7] = 0x41u; /* ein Datenbyte, damit es wirklich Lc ist */
		T_EQ("Lc ueber der Grenze", osk_apdu_parse(big, 8u, &a), OSK_ERR_FULL);
	}
}

static void test_tlv_write(void)
{
	uint8_t buf[512];
	osk_resp_t r;
	osk_tlv_mark_t outer;
	osk_tlv_mark_t inner;
	static const uint8_t val[] = {0xaa, 0xbb};

	/* Einfaches TLV, Tag mit einem und mit zwei Bytes. */
	osk_resp_init(&r, buf, sizeof(buf));
	osk_resp_tlv(&r, 0x4fu, val, sizeof(val));
	osk_resp_tlv(&r, 0x5f50u, val, 1u);
	osk_resp_tlv(&r, 0x5fc105u, val, 0u);
	T_TRUE("kein Ueberlauf", !r.overflow);
	T_HEX("TLV geschrieben", r.buf, r.len, "4f02aabb5f5001aa5fc10500");

	/* Verschachtelt, kurze Laenge. */
	osk_resp_init(&r, buf, sizeof(buf));
	osk_resp_tlv_begin(&r, 0x61u, &outer);
	osk_resp_tlv_begin(&r, 0x79u, &inner);
	osk_resp_tlv(&r, 0x4fu, val, sizeof(val));
	osk_resp_tlv_end(&r, &inner);
	osk_resp_tlv_end(&r, &outer);
	T_HEX("verschachtelt", r.buf, r.len, "610679044f02aabb");

	/* Verschachtelt mit einem Inhalt ueber 127 Bytes: das Laengenfeld
	 * muss nachwachsen und die Daten dabei verschieben. */
	{
		uint8_t big[200];

		memset(big, 0x5a, sizeof(big));
		osk_resp_init(&r, buf, sizeof(buf));
		osk_resp_tlv_begin(&r, 0x7cu, &outer);
		osk_resp_tlv(&r, 0x82u, big, 200u);
		osk_resp_tlv_end(&r, &outer);
		T_TRUE("langes TLV ohne Ueberlauf", !r.overflow);
		/* 7C 81 CF 82 81 C8 <200 Bytes> */
		T_EQ("Tag", r.buf[0], 0x7c);
		T_EQ("Laengenform 81", r.buf[1], 0x81);
		T_EQ("Laenge", r.buf[2], 203);
		T_EQ("inneres Tag", r.buf[3], 0x82);
		T_EQ("Gesamtlaenge", r.len, 206);
		T_EQ("letztes Byte", r.buf[r.len - 1u], 0x5a);
	}

	/* Zwei Bytes Laenge. */
	{
		uint8_t big[400];

		memset(big, 0x11, sizeof(big));
		osk_resp_init(&r, buf, sizeof(buf));
		osk_resp_tlv(&r, 0x53u, big, sizeof(big));
		T_EQ("Laengenform 82", r.buf[1], 0x82);
		T_EQ("Laenge hoch", r.buf[2], 0x01);
		T_EQ("Laenge tief", r.buf[3], 0x90);
		T_EQ("Gesamt", r.len, 404);
	}

	/* Zu klein: der Ueberlauf wird gemerkt und nichts geschrieben. */
	{
		uint8_t tiny[4];

		osk_resp_init(&r, tiny, sizeof(tiny));
		osk_resp_tlv(&r, 0x53u, val, sizeof(val));
		T_TRUE("passt noch", !r.overflow);
		osk_resp_tlv(&r, 0x53u, val, sizeof(val));
		T_TRUE("Ueberlauf gemerkt", r.overflow);
		T_EQ("Laenge bleibt", r.len, 4);
	}
}

static void test_tlv_read(void)
{
	uint8_t buf[512];
	const uint8_t *val = NULL;
	size_t len = 0;
	size_t used = 0;
	uint32_t tag = 0;
	size_t total;

	total = t_unhex("4f02aabb5f5003010203", buf, sizeof(buf));
	T_EQ("Vorlage", total, 10);

	T_TRUE("Tag mit einem Byte", osk_tlv_find(buf, total, 0x4fu, &val, &len));
	T_EQ("Laenge", len, 2);
	T_EQ("Wert", val[0], 0xaa);

	T_TRUE("Tag mit zwei Bytes", osk_tlv_find(buf, total, 0x5f50u, &val, &len));
	T_EQ("Laenge 2", len, 3);
	T_EQ("Wert 2", val[2], 0x03);

	T_TRUE("fehlendes Tag", !osk_tlv_find(buf, total, 0x82u, &val, &len));

	/* Das erste TLV und wie viel es belegt. */
	T_TRUE("erstes TLV", osk_tlv_first(buf, total, &tag, &val, &len, &used));
	T_EQ("erstes Tag", tag, 0x4f);
	T_EQ("belegt", used, 4);

	/* Laenge zeigt ueber das Ende hinaus - das muss auffallen. */
	total = t_unhex("4f05aabb", buf, sizeof(buf));
	T_TRUE("Laenge zu gross", !osk_tlv_find(buf, total, 0x4fu, &val, &len));

	/* Langes Laengenfeld. */
	total = t_unhex("53810241", buf, sizeof(buf));
	T_TRUE("81 verspricht mehr als da ist", !osk_tlv_find(buf, total, 0x53u, &val, &len));
	total = t_unhex("5381024142", buf, sizeof(buf));
	T_TRUE("81 mit Daten", osk_tlv_find(buf, total, 0x53u, &val, &len));
	T_EQ("Laenge aus 81", len, 2);

	/* 0x83 und laenger nehmen wir nicht an. */
	total = t_unhex("53830000014142", buf, sizeof(buf));
	T_TRUE("83 abgelehnt", !osk_tlv_find(buf, total, 0x53u, &val, &len));

	/* Mehrbyte-Tag mit zu vielen Folgebytes. */
	total = t_unhex("5fffff8f0141", buf, sizeof(buf));
	T_TRUE("zu langes Tag", !osk_tlv_first(buf, total, &tag, &val, &len, &used));

	/* Leere Eingabe. */
	T_TRUE("nichts drin", !osk_tlv_first(buf, 0u, &tag, &val, &len, &used));
	T_TRUE("Nullzeiger", !osk_tlv_first(NULL, 4u, &tag, &val, &len, &used));
}

/* Ein TLV, das geschrieben und wieder gelesen wird, muss gleich bleiben. */
static void test_roundtrip(void)
{
	uint8_t buf[2048];
	uint8_t payload[600];
	osk_resp_t r;
	osk_tlv_mark_t m;
	const uint8_t *val = NULL;
	const uint8_t *inner = NULL;
	size_t len = 0;
	size_t inner_len = 0;

	for (size_t i = 0; i < sizeof(payload); i++)
		payload[i] = (uint8_t)i;

	osk_resp_init(&r, buf, sizeof(buf));
	osk_resp_tlv_begin(&r, 0x7f49u, &m);
	osk_resp_tlv(&r, 0x86u, payload, sizeof(payload));
	osk_resp_tlv_end(&r, &m);
	T_TRUE("gebaut", !r.overflow);

	T_TRUE("aeusseres gefunden", osk_tlv_find(r.buf, r.len, 0x7f49u, &val, &len));
	T_TRUE("inneres gefunden", osk_tlv_find(val, len, 0x86u, &inner, &inner_len));
	T_EQ("Laenge haelt", inner_len, sizeof(payload));
	T_TRUE("Inhalt haelt", memcmp(inner, payload, sizeof(payload)) == 0);
}

int main(void)
{
	test_cases();
	test_rejects();
	test_tlv_write();
	test_tlv_read();
	test_roundtrip();
	return t_report("test_apdu");
}
