#include "se050_crc.h"

/* Bitweise gerechnet statt mit Tabelle: 512 Byte Flash gespart, und bei
 * hoechstens knapp 900 Byte je Rahmen fallen die Takte nicht auf. */
uint16_t osk_se050_crc16_update(uint16_t crc, const uint8_t *data, size_t len)
{
	for (size_t i = 0; i < len; i++) {
		crc ^= (uint16_t)data[i];
		for (int bit = 0; bit < 8; bit++) {
			if (crc & 1u) {
				crc = (uint16_t)((crc >> 1) ^ 0x8408u);
			} else {
				crc = (uint16_t)(crc >> 1);
			}
		}
	}
	return crc;
}

uint16_t osk_se050_crc16_final(uint16_t crc)
{
	return (uint16_t)(crc ^ 0xFFFFu);
}

uint16_t osk_se050_crc16(const uint8_t *data, size_t len)
{
	return osk_se050_crc16_final(osk_se050_crc16_update(OSK_SE050_CRC_INIT, data, len));
}
