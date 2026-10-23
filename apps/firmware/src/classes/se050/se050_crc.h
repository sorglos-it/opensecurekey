/* Pruefsumme der T=1'-Rahmen: CRC-16/X-25, auch CRC-16/IBM-SDLC genannt.
 *
 * ISO/IEC 7816-3 schreibt fuer T=1 diese Pruefsumme vor; das SE050-Datenblatt
 * verweist fuer den Epilog auf genau diese Festlegung. Kenngroessen:
 *   Polynom 0x1021, gespiegelt (0x8408), Startwert 0xFFFF,
 *   Ein- und Ausgabe gespiegelt, am Ende mit 0xFFFF verknuepft.
 * Pruefwert fuer "123456789" ist 0x906E - genau das prueft test_crc.c.
 *
 * Im Rahmen stehen die zwei Bytes niederwertig zuerst (siehe se050_t1_wire.h). */
#ifndef SE050_CRC_H
#define SE050_CRC_H

#include <stdint.h>
#include <stddef.h>

uint16_t osk_se050_crc16(const uint8_t *data, size_t len);

/* Fortschreibend, damit Prolog und INF ohne Zwischenpuffer gerechnet werden.
 * Start mit OSK_SE050_CRC_INIT, Abschluss mit osk_se050_crc16_final(). */
#define OSK_SE050_CRC_INIT 0xFFFFu

uint16_t osk_se050_crc16_update(uint16_t crc, const uint8_t *data, size_t len);
uint16_t osk_se050_crc16_final(uint16_t crc);

#endif
