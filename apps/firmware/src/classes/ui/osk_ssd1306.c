/* Treiber des Anzeige-Controllers (SSD1306-Verwandter) - siehe osk_display.h.
 *
 * Selbst geschrieben aus dem Datenblatt: die Startfolge ist eine Liste von
 * Befehlsbytes, das Bild geht seitenweise hinaus. Mehr ist es nicht.
 *
 * Nie an einem echten Teil gelaufen - das Geraet ist bestellt, nicht da.
 * Geprueft ist, WELCHE Bytes der Treiber sendet (test/ui), nicht, was ein
 * Controller daraus macht. */

#include <string.h>
#include "osk_display.h"

/* Befehle, die wir benutzen. Namen aus dem Datenblatt. */
#define CMD_CONTRAST 0x81
#define CMD_CHARGE_PUMP 0x8D
#define CMD_SEG_REMAP 0xA1
#define CMD_ALL_ON_RESUME 0xA4
#define CMD_NORMAL 0xA6
#define CMD_INVERSE 0xA7
#define CMD_MULTIPLEX 0xA8
#define CMD_DISPLAY_OFF 0xAE
#define CMD_DISPLAY_ON 0xAF
#define CMD_PAGE_START 0xB0
#define CMD_COM_SCAN_DEC 0xC8
#define CMD_OFFSET 0xD3
#define CMD_CLOCK 0xD5
#define CMD_PRECHARGE 0xD9
#define CMD_COM_PINS 0xDA
#define CMD_VCOMH 0xDB
#define CMD_MEM_MODE 0x20
#define CMD_START_LINE 0x40

static const osk_display_bus_t *g_bus;

static osk_result_t send_cmd(const uint8_t *bytes, size_t len)
{
	if (g_bus == NULL || g_bus->cmd == NULL) {
		return OSK_ERR_INTERNAL;
	}
	return g_bus->cmd(g_bus->ctx, bytes, len);
}

static void wait(uint32_t ms)
{
	if (g_bus != NULL && g_bus->delay_ms != NULL) {
		g_bus->delay_ms(g_bus->ctx, ms);
	}
}

/* Startfolge. Reihenfolge wie im Datenblatt: erst Takt und Geometrie, dann
 * Ladepumpe, dann einschalten. Die Zeilenzahl kommt aus der Panelgroesse. */
static const uint8_t start_sequence[] = {
    CMD_DISPLAY_OFF,
    CMD_CLOCK,
    0x80,
    CMD_MULTIPLEX,
    OSK_DISPLAY_HEIGHT - 1,
    CMD_OFFSET,
    0x00,
    CMD_START_LINE,
    CMD_CHARGE_PUMP,
    0x14, /* aus der eigenen Ladepumpe, kein externes 7,5 V */
    CMD_MEM_MODE,
    0x02, /* Seitenadressierung: wir schreiben Seite fuer Seite */
    CMD_SEG_REMAP,
    CMD_COM_SCAN_DEC,
    CMD_COM_PINS,
    0x12,
    CMD_CONTRAST,
    0x8F,
    CMD_PRECHARGE,
    0xF1,
    CMD_VCOMH,
    0x40,
    CMD_ALL_ON_RESUME,
    CMD_NORMAL,
    CMD_DISPLAY_ON,
};

osk_result_t osk_display_init(const osk_display_bus_t *bus)
{
	osk_result_t result;

	if (bus == NULL || bus->cmd == NULL || bus->data == NULL) {
		return OSK_ERR_ARG;
	}
	g_bus = bus;
	if (bus->power != NULL) {
		result = bus->power(bus->ctx, true);
		if (result != OSK_OK) {
			g_bus = NULL;
			return result;
		}
		wait(10); /* Datenblatt: Versorgung muss stehen, bevor Reset endet */
	}
	if (bus->reset != NULL) {
		result = bus->reset(bus->ctx, true);
		if (result != OSK_OK) {
			g_bus = NULL;
			return result;
		}
		wait(1);
		result = bus->reset(bus->ctx, false);
		if (result != OSK_OK) {
			g_bus = NULL;
			return result;
		}
		wait(10);
	}
	result = send_cmd(start_sequence, sizeof(start_sequence));
	if (result != OSK_OK) {
		g_bus = NULL;
	}
	return result;
}

bool osk_display_ready(void)
{
	return g_bus != NULL;
}

osk_result_t osk_display_flush(const osk_gfx_t *fb)
{
	if (fb == NULL) {
		return OSK_ERR_ARG;
	}
	if (g_bus == NULL) {
		return OSK_ERR_INTERNAL;
	}
	for (uint8_t page = 0; page < OSK_DISPLAY_PAGES; page++) {
		/* Der Spaltenversatz ist der Grund, warum das 48er Teil einen
		 * eigenen Wert braucht: seine 48 Spalten liegen in der Mitte
		 * des 128 Spalten breiten Speichers. */
		const uint8_t column = OSK_DISPLAY_COL_OFFSET;
		const uint8_t head[] = {(uint8_t)(CMD_PAGE_START + page), (uint8_t)(column & 0x0Fu),
					(uint8_t)(0x10u | (column >> 4))};
		osk_result_t result = send_cmd(head, sizeof(head));

		if (result != OSK_OK) {
			return result;
		}
		result = g_bus->data(g_bus->ctx, &fb->pixels[(size_t)page * OSK_DISPLAY_WIDTH],
				     OSK_DISPLAY_WIDTH);
		if (result != OSK_OK) {
			return result;
		}
	}
	return OSK_OK;
}

osk_result_t osk_display_sleep(void)
{
	const uint8_t off[] = {CMD_DISPLAY_OFF};
	osk_result_t result;

	if (g_bus == NULL) {
		return OSK_ERR_INTERNAL;
	}
	result = send_cmd(off, sizeof(off));
	if (g_bus->power != NULL) {
		osk_result_t power = g_bus->power(g_bus->ctx, false);

		if (result == OSK_OK) {
			result = power;
		}
	}
	g_bus = NULL;
	return result;
}

osk_result_t osk_display_contrast(uint8_t value)
{
	const uint8_t bytes[] = {CMD_CONTRAST, value};

	if (g_bus == NULL) {
		return OSK_ERR_INTERNAL;
	}
	return send_cmd(bytes, sizeof(bytes));
}

osk_result_t osk_display_inverted(bool inverted)
{
	const uint8_t bytes[] = {inverted ? CMD_INVERSE : CMD_NORMAL};

	if (g_bus == NULL) {
		return OSK_ERR_INTERNAL;
	}
	return send_cmd(bytes, sizeof(bytes));
}
