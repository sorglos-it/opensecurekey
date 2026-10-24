/* RLP: die Beispiele aus dem gelben Papier und die Faelle, in denen eine
 * zweite Schreibweise fuer denselben Wert abgelehnt werden muss. */

#include "test_util.h"
#include "wallet_rlp.h"

static void expect_bytes(const char *what, const char *hex, const char *want_hex)
{
	uint8_t raw[256];
	size_t len = t_unhex(hex, raw, sizeof(raw));
	osk_rlp_reader_t r;
	osk_rlp_item_t item;

	osk_rlp_reader_init(&r, raw, len);
	T_EQ(what, osk_rlp_next(&r, &item), OSK_OK);
	T_EQ("Bytefolge", item.kind, OSK_RLP_BYTES);
	T_HEX(what, item.data, item.len, want_hex);
	T_TRUE("nichts dahinter", osk_rlp_at_end(&r));
}

static void expect_reject(const char *what, const char *hex)
{
	uint8_t raw[256];
	size_t len = t_unhex(hex, raw, sizeof(raw));
	osk_rlp_reader_t r;
	osk_rlp_item_t item;
	osk_result_t res;

	osk_rlp_reader_init(&r, raw, len);
	res = osk_rlp_next(&r, &item);
	/* Entweder faellt es beim Lesen auf, oder es bleibt ein Rest - beides
	 * ist eine Ablehnung, solange es nicht glatt durchgeht. */
	if (res == OSK_OK && osk_rlp_at_end(&r))
		t_fail(what, __FILE__, __LINE__, hex);
	T_TRUE(what, res != OSK_OK || !osk_rlp_at_end(&r));
}

static void test_basic(void)
{
	/* "dog" ist das Beispiel der Spezifikation. */
	expect_bytes("dog", "83646f67", "646f67");
	expect_bytes("leer", "80", "");
	expect_bytes("ein Byte 0x0f", "0f", "0f");
	expect_bytes("0x04 0x00", "820400", "0400");
	/* 56 Byte: der erste Fall mit langer Laengenangabe. */
	expect_bytes("56 Byte",
		     "b838"
		     "00000000000000000000000000000000000000000000000000000000"
		     "00000000000000000000000000000000000000000000000000000000",
		     "00000000000000000000000000000000000000000000000000000000"
		     "00000000000000000000000000000000000000000000000000000000");
}

static void test_lists(void)
{
	uint8_t raw[64];
	size_t len;
	osk_rlp_reader_t r, inner;
	osk_rlp_item_t item;

	/* ["cat", "dog"] */
	len = t_unhex("c88363617483646f67", raw, sizeof(raw));
	osk_rlp_reader_init(&r, raw, len);
	T_EQ("Liste gelesen", osk_rlp_next(&r, &item), OSK_OK);
	T_EQ("ist eine Liste", item.kind, OSK_RLP_LIST);
	T_TRUE("Liste war alles", osk_rlp_at_end(&r));
	osk_rlp_reader_init(&inner, item.data, item.len);
	T_EQ("erstes Glied", osk_rlp_next(&inner, &item), OSK_OK);
	T_HEX("cat", item.data, item.len, "636174");
	T_EQ("zweites Glied", osk_rlp_next(&inner, &item), OSK_OK);
	T_HEX("dog", item.data, item.len, "646f67");
	T_EQ("Schluss", osk_rlp_next(&inner, &item), OSK_ERR_NOT_FOUND);

	/* Die leere Liste. */
	len = t_unhex("c0", raw, sizeof(raw));
	osk_rlp_reader_init(&r, raw, len);
	T_EQ("leere Liste", osk_rlp_next(&r, &item), OSK_OK);
	T_EQ("ist Liste", item.kind, OSK_RLP_LIST);
	T_EQ("ohne Inhalt", item.len, 0);
}

static void test_numbers(void)
{
	uint8_t raw[64];
	size_t len;
	osk_rlp_reader_t r;
	osk_rlp_item_t item;
	uint64_t value = 0;
	uint8_t big[32];

	len = t_unhex("8504a817c800", raw, sizeof(raw)); /* 20 Gwei */
	osk_rlp_reader_init(&r, raw, len);
	T_EQ("gelesen", osk_rlp_next(&r, &item), OSK_OK);
	T_EQ("als Zahl", osk_rlp_as_u64(&item, &value), OSK_OK);
	T_TRUE("20000000000", value == 20000000000ULL);

	len = t_unhex("880de0b6b3a7640000", raw, sizeof(raw)); /* 1 Ether in Wei */
	osk_rlp_reader_init(&r, raw, len);
	T_EQ("gelesen", osk_rlp_next(&r, &item), OSK_OK);
	T_EQ("als 256 Bit", osk_rlp_as_u256(&item, big), OSK_OK);
	T_HEX("1 Ether", big, 32,
	      "0000000000000000000000000000000000000000000000000de0b6b3a7640000");

	/* Die Null ist die leere Folge; 0x00 als Inhalt ist verboten, weil es
	 * eine zweite Schreibweise fuer dieselbe Zahl waere. */
	len = t_unhex("00", raw, sizeof(raw));
	osk_rlp_reader_init(&r, raw, len);
	T_EQ("0x00 gelesen", osk_rlp_next(&r, &item), OSK_OK);
	T_EQ("aber keine Zahl", osk_rlp_as_u64(&item, &value), OSK_ERR_FORMAT);

	len = t_unhex("820001", raw, sizeof(raw));
	osk_rlp_reader_init(&r, raw, len);
	T_EQ("gelesen", osk_rlp_next(&r, &item), OSK_OK);
	T_EQ("fuehrende Null abgelehnt", osk_rlp_as_u64(&item, &value), OSK_ERR_FORMAT);

	len = t_unhex("89010000000000000000", raw, sizeof(raw)); /* 2^64 */
	osk_rlp_reader_init(&r, raw, len);
	T_EQ("gelesen", osk_rlp_next(&r, &item), OSK_OK);
	T_EQ("zu gross fuer 64 Bit", osk_rlp_as_u64(&item, &value), OSK_ERR_FORMAT);
	T_EQ("aber gut fuer 256 Bit", osk_rlp_as_u256(&item, big), OSK_OK);
}

static void test_canonical(void)
{
	/* Ein Byte unter 0x80 mit Kopf: verboten. */
	expect_reject("0x81 0x05 abgelehnt", "8105");
	/* Lange Form fuer etwas, das kurz geht: verboten. */
	expect_reject("lange Form fuer 3 Byte abgelehnt", "b803646f67");
	expect_reject("lange Listenform fuer 3 Byte abgelehnt", "f803c20102");
	/* Fuehrende Null in der Laengenangabe: verboten. */
	expect_reject("Laenge mit fuehrender Null", "b900ff");
	/* Zu kurz: die angekuendigten Bytes fehlen. */
	expect_reject("abgeschnitten", "83646f");
	expect_reject("Liste abgeschnitten", "c8836361");
	expect_reject("Laengenangabe abgeschnitten", "b8");
	expect_reject("nichts da", "");
}

static void test_write(void)
{
	uint8_t out[16];

	T_EQ("Null ist leer", osk_rlp_write_u64(0, out, sizeof(out)), 1);
	T_HEX("0x80", out, 1, "80");
	T_EQ("kleine Zahl ohne Kopf", osk_rlp_write_u64(15, out, sizeof(out)), 1);
	T_HEX("0x0f", out, 1, "0f");
	T_EQ("0x80 braucht einen Kopf", osk_rlp_write_u64(128, out, sizeof(out)), 2);
	T_HEX("0x81 0x80", out, 2, "8180");
	T_EQ("1024", osk_rlp_write_u64(1024, out, sizeof(out)), 3);
	T_HEX("0x82 0x04 0x00", out, 3, "820400");
	T_EQ("Puffer zu klein", osk_rlp_write_u64(1024, out, 2), 0);
}

int main(void)
{
	test_basic();
	test_lists();
	test_numbers();
	test_canonical();
	test_write();
	return t_report("rlp");
}
