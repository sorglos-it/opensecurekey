/* Siehe yk_crc.h. */
#include "yk_crc.h"

uint16_t yk_crc16(const uint8_t *data, size_t len)
{
	uint16_t crc = 0xffffu;

	if (data == NULL)
		return crc;

	for (size_t i = 0; i < len; i++) {
		crc ^= data[i];
		for (unsigned bit = 0; bit < 8u; bit++) {
			const bool carry = (crc & 1u) != 0u;

			crc = (uint16_t)(crc >> 1);
			if (carry)
				crc = (uint16_t)(crc ^ 0x8408u);
		}
	}
	return crc;
}

uint16_t yk_crc16_wire(const uint8_t *data, size_t len)
{
	return (uint16_t)(~yk_crc16(data, len) & 0xffffu);
}

bool yk_crc16_residue_ok(const uint8_t *data, size_t len)
{
	/* Unter drei Bytes gibt es keine Daten neben der Pruefsumme. */
	if (data == NULL || len < 3u)
		return false;
	return yk_crc16(data, len) == YK_CRC_RESIDUE;
}
