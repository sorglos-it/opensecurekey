/* Der DER-Leser und -Schreiber.
 *
 * Diese Prueffaelle sind die wichtigsten der ganzen Schicht, weil der
 * Leser Daten vom Draht entgegennimmt - die Antwort einer fremden
 * Zeitstempelstelle, und die muss nicht freundlich sein. Darum stehen hier
 * mehr abgeschnittene und falsch kodierte Eingaben als gueltige. */
#include <string.h>
#include "authz_t.h"
#include "authz_der.h"

void test_der(void)
{
	TT_CASE("Kurzform lesen");
	{
		const uint8_t in[] = {0x02u, 0x01u, 0x05u};
		authz_der_t c = {in, sizeof(in)};
		uint8_t tag = 0u;
		const uint8_t *val = NULL;
		size_t vlen = 0u;

		TT_EQ_INT(authz_der_next(&c, &tag, &val, &vlen), OSK_OK, "lesen");
		TT_EQ_INT(tag, 0x02, "Typ");
		TT_EQ_INT(vlen, 1, "Laenge");
		TT_EQ_INT(val[0], 5, "Wert");
		TT_EQ_INT(c.len, 0, "nichts mehr uebrig");
	}

	TT_CASE("Langform lesen, 200 Byte");
	{
		uint8_t in[203];
		memset(in, 0xAAu, sizeof(in));
		in[0] = 0x04u;
		in[1] = 0x81u;
		in[2] = 200u;
		authz_der_t c = {in, sizeof(in)};
		const uint8_t *val = NULL;
		size_t vlen = 0u;
		TT_EQ_INT(authz_der_next(&c, NULL, &val, &vlen), OSK_OK, "lesen");
		TT_EQ_INT(vlen, 200, "Laenge");
	}

	TT_CASE("abgeschnitten wird abgewiesen, nicht ueberlesen");
	{
		/* Der haeufigste Fehler in DER-Parsern: die Laenge aus den
		 * Daten nehmen und darauf zugreifen, ohne sie gegen den Rest
		 * des Puffers zu halten. */
		const uint8_t a[] = {0x04u, 0x05u, 0x01u, 0x02u};
		authz_der_t c = {a, sizeof(a)};
		TT_EQ_INT(authz_der_next(&c, NULL, NULL, NULL), OSK_ERR_FORMAT,
			  "Inhalt kuerzer als angesagt");

		const uint8_t b[] = {0x04u};
		authz_der_t c2 = {b, sizeof(b)};
		TT_EQ_INT(authz_der_next(&c2, NULL, NULL, NULL), OSK_ERR_FORMAT, "nur der Typ");

		const uint8_t d[] = {0x04u, 0x82u, 0x01u};
		authz_der_t c3 = {d, sizeof(d)};
		TT_EQ_INT(authz_der_next(&c3, NULL, NULL, NULL), OSK_ERR_FORMAT,
			  "Langform selbst abgeschnitten");

		authz_der_t c4 = {a, 0u};
		TT_EQ_INT(authz_der_next(&c4, NULL, NULL, NULL), OSK_ERR_FORMAT, "leer");
	}

	TT_CASE("unbestimmte Laenge gibt es in DER nicht");
	{
		/* 0x80 heisst in BER "Ende folgt irgendwann". Ein Angreifer
		 * laesst das Ende weg. */
		const uint8_t in[] = {0x30u, 0x80u, 0x00u, 0x00u};
		authz_der_t c = {in, sizeof(in)};
		TT_EQ_INT(authz_der_next(&c, NULL, NULL, NULL), OSK_ERR_FORMAT, "0x80 abgewiesen");
	}

	TT_CASE("nicht minimal kodierte Laengen werden abgewiesen");
	{
		/* Dieselbe Laenge zweimal darstellbar heisst: zwei
		 * verschiedene Byteketten bedeuten dasselbe, und eine
		 * Unterschrift ueber die eine gilt fuer die andere nicht. */
		const uint8_t a[] = {0x04u, 0x81u, 0x01u, 0xFFu};
		authz_der_t c = {a, sizeof(a)};
		TT_EQ_INT(authz_der_next(&c, NULL, NULL, NULL), OSK_ERR_FORMAT,
			  "Langform fuer eine Laenge unter 128");

		const uint8_t b[] = {0x04u, 0x82u, 0x00u, 0x05u, 1u, 2u, 3u, 4u, 5u};
		authz_der_t c2 = {b, sizeof(b)};
		TT_EQ_INT(authz_der_next(&c2, NULL, NULL, NULL), OSK_ERR_FORMAT,
			  "fuehrende Null in der Laenge");

		const uint8_t d[] = {0x04u, 0x85u, 1u, 2u, 3u, 4u, 5u};
		authz_der_t c3 = {d, sizeof(d)};
		TT_EQ_INT(authz_der_next(&c3, NULL, NULL, NULL), OSK_ERR_FORMAT,
			  "mehr als vier Laengenbytes");
	}

	TT_CASE("expect stellt ein unerwartetes Element zurueck");
	{
		/* In RFC 3161 sind mehrere Felder wahlfrei. Ohne das
		 * Zuruecksetzen wuerde das wahlfreie Feld verschluckt und
		 * das naechste nicht mehr gefunden. */
		const uint8_t in[] = {0x02u, 0x01u, 0x01u};
		authz_der_t c = {in, sizeof(in)};
		TT_EQ_INT(authz_der_expect(&c, DER_BOOLEAN, NULL, NULL), OSK_ERR_FORMAT,
			  "nicht der erwartete Typ");
		TT_EQ_INT(c.len, sizeof(in), "der Platz ist unveraendert");
		TT_EQ_INT(authz_der_expect(&c, DER_INTEGER, NULL, NULL), OSK_OK,
			  "und jetzt der richtige");
	}

	TT_CASE("hineingehen und wieder heraus");
	{
		const uint8_t in[] = {0x30u, 0x06u, 0x02u, 0x01u, 0x07u, 0x02u, 0x01u, 0x08u};
		authz_der_t c = {in, sizeof(in)};
		authz_der_t inner;
		TT_EQ_INT(authz_der_enter(&c, DER_SEQUENCE, &inner), OSK_OK, "hinein");
		TT_EQ_INT(inner.len, 6, "Inhalt");
		TT_EQ_INT(c.len, 0, "draussen weiter");

		const uint8_t *val = NULL;
		size_t vlen = 0u;
		TT_EQ_INT(authz_der_expect(&inner, DER_INTEGER, &val, &vlen), OSK_OK, "erste");
		TT_EQ_INT(val[0], 7, "7");
		TT_EQ_INT(authz_der_expect(&inner, DER_INTEGER, &val, &vlen), OSK_OK, "zweite");
		TT_EQ_INT(val[0], 8, "8");
		TT_EQ_INT(authz_der_expect(&inner, DER_INTEGER, &val, &vlen), OSK_ERR_FORMAT,
			  "keine dritte");
	}

	TT_CASE("INTEGER lesen, samt der Faelle, die nicht gelesen werden");
	{
		uint64_t v = 0u;
		const uint8_t one[] = {0x01u};
		TT_EQ_INT(authz_der_u64(one, 1u, &v), OSK_OK, "1");
		TT_EQ_INT(v, 1, "Wert 1");

		const uint8_t pad[] = {0x00u, 0xFFu};
		TT_EQ_INT(authz_der_u64(pad, 2u, &v), OSK_OK, "255 mit Nullbyte");
		TT_EQ_INT(v, 255, "Wert 255");

		const uint8_t big[] = {0x00u, 0xFFu, 0xFFu, 0xFFu, 0xFFu,
				       0xFFu, 0xFFu, 0xFFu, 0xFFu};
		TT_EQ_INT(authz_der_u64(big, 9u, &v), OSK_OK, "acht Byte voll");
		TT_TRUE(v == 0xFFFFFFFFFFFFFFFFull, "alle Bits");

		/* Gesetztes Vorzeichenbit: eine negative Zahl, und sie als
		 * grosse vorzeichenlose zu lesen waere die stille Umdeutung,
		 * die hier nichts verloren hat. */
		const uint8_t neg[] = {0x80u, 0x01u};
		TT_EQ_INT(authz_der_u64(neg, 2u, &v), OSK_ERR_FORMAT, "negativ");

		const uint8_t nonmin[] = {0x00u, 0x01u};
		TT_EQ_INT(authz_der_u64(nonmin, 2u, &v), OSK_ERR_FORMAT, "nicht minimal");

		const uint8_t toolong[] = {0x01u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u};
		TT_EQ_INT(authz_der_u64(toolong, 10u, &v), OSK_ERR_FORMAT, "mehr als acht Byte");

		TT_EQ_INT(authz_der_u64(one, 0u, &v), OSK_ERR_FORMAT, "leer");
	}

	TT_CASE("schreiben und wieder lesen");
	{
		uint8_t buf[64];
		authz_der_w_t w;
		authz_der_w_init(&w, buf, sizeof(buf));

		/* Rueckwaerts: zuerst das letzte Element. */
		size_t mark = w.used;
		authz_der_w_bool(&w, true);
		authz_der_w_u64(&w, 300u);
		authz_der_w_header(&w, DER_SEQUENCE, w.used - mark);

		const uint8_t *start = NULL;
		size_t len = 0u;
		TT_EQ_INT(authz_der_w_done(&w, &start, &len), OSK_OK, "fertig");

		authz_der_t c = {start, len};
		authz_der_t inner;
		TT_EQ_INT(authz_der_enter(&c, DER_SEQUENCE, &inner), OSK_OK, "hinein");

		const uint8_t *val = NULL;
		size_t vlen = 0u;
		TT_EQ_INT(authz_der_expect(&inner, DER_INTEGER, &val, &vlen), OSK_OK, "Zahl");
		uint64_t v = 0u;
		TT_EQ_INT(authz_der_u64(val, vlen, &v), OSK_OK, "lesbar");
		TT_EQ_INT(v, 300, "300 kam heil durch");

		TT_EQ_INT(authz_der_expect(&inner, DER_BOOLEAN, &val, &vlen), OSK_OK, "Wahrheit");
		TT_EQ_INT(vlen, 1, "ein Byte");
		TT_EQ_INT(val[0], 0xFF, "DER kennt nur 0x00 und 0xFF");
	}

	TT_CASE("die Grenze bei 127 und 128 Byte");
	{
		/* Genau hier wechselt die Laengenkodierung, und genau hier
		 * sind Parser falsch. */
		uint8_t payload[128];
		memset(payload, 0x5Au, sizeof(payload));

		for (size_t n = 126u; n <= 128u; n++) {
			uint8_t buf[160];
			authz_der_w_t w;
			authz_der_w_init(&w, buf, sizeof(buf));
			authz_der_w_tlv(&w, DER_OCTET, payload, n);

			const uint8_t *start = NULL;
			size_t len = 0u;
			TT_EQ_INT(authz_der_w_done(&w, &start, &len), OSK_OK, "geschrieben");
			TT_EQ_INT(len, n + (n < 128u ? 2u : 3u), "Kopflaenge stimmt");

			authz_der_t c = {start, len};
			const uint8_t *val = NULL;
			size_t vlen = 0u;
			TT_EQ_INT(authz_der_expect(&c, DER_OCTET, &val, &vlen), OSK_OK, "gelesen");
			TT_EQ_INT(vlen, n, "Laenge");
			TT_EQ_MEM(val, payload, n, "Inhalt");
		}
	}

	TT_CASE("zu kleiner Puffer wird gemeldet, nicht ueberschrieben");
	{
		uint8_t buf[8];
		uint8_t payload[32];
		authz_der_w_t w;
		memset(payload, 1, sizeof(payload));
		authz_der_w_init(&w, buf, sizeof(buf));
		authz_der_w_tlv(&w, DER_OCTET, payload, sizeof(payload));
		TT_EQ_INT(authz_der_w_done(&w, NULL, NULL), OSK_ERR_FULL, "zu klein");
		/* Einmal uebergelaufen bleibt uebergelaufen - ein spaeterer
		 * kleiner Aufruf darf den Befund nicht wieder aufheben. */
		authz_der_w_bool(&w, false);
		TT_EQ_INT(authz_der_w_done(&w, NULL, NULL), OSK_ERR_FULL, "und bleibt es");
	}

	TT_CASE("grosse Zahl aus Rohbytes: fuehrende Nullen weg, Vorzeichen-Null dazu");
	{
		uint8_t buf[32];
		const uint8_t nonce[] = {0x00u, 0x00u, 0x80u, 0x01u};
		authz_der_w_t w;
		authz_der_w_init(&w, buf, sizeof(buf));
		authz_der_w_uint_bytes(&w, nonce, sizeof(nonce));

		const uint8_t *start = NULL;
		size_t len = 0u;
		TT_EQ_INT(authz_der_w_done(&w, &start, &len), OSK_OK, "fertig");

		/* 0x00 0x00 0x80 0x01 -> 0x80 0x01, und weil das hoechste Bit
		 * gesetzt ist, kommt eine 0x00 davor: 02 03 00 80 01 */
		const uint8_t want[] = {0x02u, 0x03u, 0x00u, 0x80u, 0x01u};
		TT_EQ_INT(len, sizeof(want), "Laenge");
		TT_EQ_MEM(start, want, sizeof(want), "Kodierung");
	}
}
