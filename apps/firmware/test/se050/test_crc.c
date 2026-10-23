/* Pruefsumme der Rahmen: CRC-16/X-25.
 *
 * Der Pruefwert fuer "123456789" ist 0x906E. Der stammt NICHT von NXP,
 * sondern aus der allgemein benutzten Kennzeichnung dieses
 * CRC-Verfahrens - er prueft also wirklich, dass hier X-25 gerechnet wird
 * und nicht eine der vielen anderen CRC-16-Spielarten. */
#include "tiny_test.h"
#include "se050_crc.h"

void test_crc(void)
{
	TT_CASE("CRC: Pruefwert fuer 123456789");
	{
		static const uint8_t s[] = "123456789";
		const uint16_t crc = osk_se050_crc16(s, 9);
		TT_EQ_INT(crc, 0x906Eu, "X-25-Pruefwert");
	}

	TT_CASE("CRC: leere Eingabe");
	{
		/* Startwert 0xFFFF, am Ende mit 0xFFFF verknuepft - bleibt 0. */
		const uint16_t crc = osk_se050_crc16(NULL, 0);
		TT_EQ_INT(crc, 0x0000u, "leer gibt 0");
	}

	TT_CASE("CRC: fortschreibend gleich wie in einem Zug");
	{
		static const uint8_t s[] = "123456789";
		uint16_t c = OSK_SE050_CRC_INIT;
		c = osk_se050_crc16_update(c, &s[0], 4);
		c = osk_se050_crc16_update(c, &s[4], 5);
		TT_EQ_INT(osk_se050_crc16_final(c), 0x906Eu,
			  "in zwei Stuecken gerechnet");
	}

	TT_CASE("CRC: ein umgedrehtes Bit faellt auf");
	{
		uint8_t a[16];
		for (size_t i = 0; i < sizeof(a); i++) {
			a[i] = (uint8_t)(i * 7u + 3u);
		}
		const uint16_t before = osk_se050_crc16(a, sizeof(a));

		for (size_t byte = 0; byte < sizeof(a); byte++) {
			for (int bit = 0; bit < 8; bit++) {
				a[byte] ^= (uint8_t)(1u << bit);
				TT_TRUE(osk_se050_crc16(a, sizeof(a)) != before,
					"ein Bitfehler aendert die Pruefsumme");
				a[byte] ^= (uint8_t)(1u << bit);
			}
		}
	}
}
