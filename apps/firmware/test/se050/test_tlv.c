/* TLV-Nutzlast: schreiben und lesen, einschliesslich der Faelle, in denen
 * eine Antwort absichtlich kaputt ist. Ein TLV-Leser, der ueber das Ende
 * des Puffers hinausliest, waere hier das gefaehrlichste Loch von allen -
 * die Antwort kommt vom Draht. */
#include "tiny_test.h"
#include "se050_tlv.h"
#include "se050_cmd.h"

void test_tlv(void)
{
	TT_CASE("TLV: schreiben und wieder lesen");
	{
		uint8_t buf[64];
		osk_tlv_writer_t w;
		osk_tlv_init(&w, buf, sizeof(buf));
		osk_tlv_put_u32(&w, TAG_1, 0x01020304u);
		osk_tlv_put_u8(&w, TAG_2, 0x5Au);
		osk_tlv_put_u16(&w, TAG_3, 0xBEEFu);

		size_t len = 0;
		TT_EQ_INT(osk_tlv_done(&w, &len), OSK_OK, "geschrieben");
		/* 3 Felder: je 1 Byte Kennung + 1 Byte Laenge + Wert. */
		TT_EQ_INT(len, (2u + 4u) + (2u + 1u) + (2u + 2u), "Laenge");

		static const uint8_t want[] = {
			TAG_1, 0x04, 0x01, 0x02, 0x03, 0x04,
			TAG_2, 0x01, 0x5A,
			TAG_3, 0x02, 0xBE, 0xEF,
		};
		TT_EQ_MEM(buf, want, sizeof(want), "Byte fuer Byte");

		osk_tlv_reader_t r;
		osk_tlv_reader_init(&r, buf, len);

		uint8_t u8 = 0;
		TT_EQ_INT(osk_tlv_get_u8(&r, TAG_2, &u8), OSK_OK, "u8 gefunden");
		TT_EQ_INT(u8, 0x5Au, "u8 Wert");

		uint16_t u16 = 0;
		TT_EQ_INT(osk_tlv_get_u16(&r, TAG_3, &u16), OSK_OK, "u16 gefunden");
		TT_EQ_INT(u16, 0xBEEFu, "u16 Wert");

		TT_EQ_INT(osk_tlv_get_u8(&r, TAG_5, &u8), OSK_ERR_NOT_FOUND,
			  "fehlende Kennung");
	}

	TT_CASE("TLV: langes Feld bekommt ein Drei-Byte-Laengenfeld");
	{
		uint8_t val[200];
		memset(val, 0xA7, sizeof(val));

		uint8_t buf[256];
		osk_tlv_writer_t w;
		osk_tlv_init(&w, buf, sizeof(buf));
		osk_tlv_put(&w, TAG_1, val, sizeof(val));

		size_t len = 0;
		TT_EQ_INT(osk_tlv_done(&w, &len), OSK_OK, "geschrieben");
		TT_EQ_INT(len, 1u + 3u + sizeof(val), "Laenge mit 0x82");
		TT_EQ_INT(buf[1], 0x82u, "0x82 als Laengenkennung");
		TT_EQ_INT(buf[2], 0x00u, "hoeherwertiges Laengenbyte");
		TT_EQ_INT(buf[3], 200u, "niederwertiges Laengenbyte");

		osk_tlv_reader_t r;
		osk_tlv_reader_init(&r, buf, len);

		uint8_t out[200];
		size_t got = 0;
		TT_EQ_INT(osk_tlv_copy(&r, TAG_1, out, sizeof(out), &got),
			  OSK_OK, "zurueckgelesen");
		TT_EQ_INT(got, sizeof(val), "Laenge zurueck");
		TT_EQ_MEM(out, val, sizeof(val), "Inhalt zurueck");
	}

	TT_CASE("TLV: Antwort darf 0x81 benutzen, auch fuer kurze Werte");
	{
		/* AN12413 4.1.3.2: eine Antwort darf ein laengeres
		 * Laengenfeld nehmen, als noetig waere. Der Leser muss das
		 * annehmen - sonst faellt der Treiber am Geraet um. */
		static const uint8_t in[] = { TAG_1, 0x81, 0x02, 0xAA, 0xBB };
		osk_tlv_reader_t r;
		osk_tlv_reader_init(&r, in, sizeof(in));

		uint16_t v = 0;
		TT_EQ_INT(osk_tlv_get_u16(&r, TAG_1, &v), OSK_OK, "0x81 gelesen");
		TT_EQ_INT(v, 0xAABBu, "Wert");
	}

	TT_CASE("TLV: voller Puffer wird gemeldet, nicht abgeschnitten");
	{
		uint8_t buf[8];
		osk_tlv_writer_t w;
		osk_tlv_init(&w, buf, sizeof(buf));
		osk_tlv_put_u32(&w, TAG_1, 0x11223344u);   /* 6 Byte, passt */
		osk_tlv_put_u32(&w, TAG_2, 0x55667788u);   /* 6 Byte, passt nicht */

		size_t len = 0;
		TT_EQ_INT(osk_tlv_done(&w, &len), OSK_ERR_FULL, "voll gemeldet");
	}

	TT_CASE("TLV: kaputte Eingaben");
	{
		/* Laenge laeuft ueber das Ende hinaus. */
		static const uint8_t over[] = { TAG_1, 0x10, 0x01, 0x02 };
		osk_tlv_reader_t r1;
		osk_tlv_reader_init(&r1, over, sizeof(over));
		const uint8_t *v = NULL;
		size_t n = 0;
		TT_EQ_INT(osk_tlv_get(&r1, TAG_1, &v, &n), OSK_ERR_FORMAT,
			  "Laenge zu gross");

		/* Kennung ohne Laengenbyte. */
		static const uint8_t cut[] = { TAG_1 };
		osk_tlv_reader_t r2;
		osk_tlv_reader_init(&r2, cut, sizeof(cut));
		TT_EQ_INT(osk_tlv_get(&r2, TAG_1, &v, &n), OSK_ERR_FORMAT,
			  "Laengenbyte fehlt");

		/* 0x82 ohne die zwei Laengenbytes. */
		static const uint8_t cut2[] = { TAG_1, 0x82, 0x00 };
		osk_tlv_reader_t r3;
		osk_tlv_reader_init(&r3, cut2, sizeof(cut2));
		TT_EQ_INT(osk_tlv_get(&r3, TAG_1, &v, &n), OSK_ERR_FORMAT,
			  "0x82 abgeschnitten");

		/* Laengenfeld laenger als drei Byte - gibt es nicht. */
		static const uint8_t wide[] = { TAG_1, 0x84, 0, 0, 0, 1, 0xAA };
		osk_tlv_reader_t r4;
		osk_tlv_reader_init(&r4, wide, sizeof(wide));
		TT_EQ_INT(osk_tlv_get(&r4, TAG_1, &v, &n), OSK_ERR_FORMAT,
			  "0x84 abgelehnt");

		/* Leere Eingabe. */
		osk_tlv_reader_t r5;
		osk_tlv_reader_init(&r5, NULL, 0);
		TT_EQ_INT(osk_tlv_get(&r5, TAG_1, &v, &n), OSK_ERR_NOT_FOUND,
			  "leer");
	}

	TT_CASE("TLV: Wert passt nicht in den Zielpuffer");
	{
		static const uint8_t in[] = { TAG_1, 0x04, 1, 2, 3, 4 };
		osk_tlv_reader_t r;
		osk_tlv_reader_init(&r, in, sizeof(in));

		uint8_t out[2];
		size_t got = 0;
		TT_EQ_INT(osk_tlv_copy(&r, TAG_1, out, sizeof(out), &got),
			  OSK_ERR_FULL, "zu klein gemeldet");
	}

	TT_CASE("TLV: das erste Feld mit der Kennung gewinnt");
	{
		static const uint8_t in[] = {
			TAG_1, 0x01, 0x11,
			TAG_1, 0x01, 0x22,
		};
		osk_tlv_reader_t r;
		osk_tlv_reader_init(&r, in, sizeof(in));

		uint8_t v = 0;
		TT_EQ_INT(osk_tlv_get_u8(&r, TAG_1, &v), OSK_OK, "gefunden");
		TT_EQ_INT(v, 0x11u, "das erste");
	}
}
