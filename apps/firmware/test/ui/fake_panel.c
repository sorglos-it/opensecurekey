/* Anzeige-Attrappe - siehe fake_panel.h. */

#include <stdio.h>
#include <string.h>
#include "fake_panel.h"
#include "osk_font.h"
#include "osk_text.h"

#define CMD_MAX 256
#define SCRIPT_MAX 32

struct segment {
	bool down;
	uint32_t ms;
};

static uint8_t g_cmds[CMD_MAX];
static size_t g_cmd_count;
static size_t g_flushes;
static size_t g_data_bytes;
static bool g_powered;
static bool g_reset_done;
static bool g_fail;
static osk_led_t g_led;
static osk_gfx_t g_image;

/* Wohin die naechsten Bildbytes gehen - der Treiber sagt es mit den
 * Befehlen 0xB0+Seite und Spalte low/high. */
static uint8_t g_page;
static uint8_t g_column;

static uint32_t g_now;
static struct segment g_script[SCRIPT_MAX];
static size_t g_script_count;
static size_t g_script_at;
static uint32_t g_in_segment;

void fake_panel_reset(void)
{
	memset(g_cmds, 0, sizeof(g_cmds));
	g_cmd_count = 0;
	g_flushes = 0;
	g_data_bytes = 0;
	g_powered = false;
	g_reset_done = false;
	g_fail = false;
	g_led = OSK_LED_OFF;
	osk_gfx_clear(&g_image);
	g_page = 0;
	g_column = 0;
	g_now = 1000; /* nicht bei 0 anfangen: ein Fehler bei der Zeitrechnung
		       * faellt so eher auf */
	memset(g_script, 0, sizeof(g_script));
	g_script_count = 0;
	g_script_at = 0;
	g_in_segment = 0;
}

static osk_result_t bus_cmd(void *ctx, const uint8_t *bytes, size_t len)
{
	(void)ctx;
	if (g_fail) {
		return OSK_ERR_IO;
	}
	for (size_t i = 0; i < len; i++) {
		if (g_cmd_count < CMD_MAX) {
			g_cmds[g_cmd_count++] = bytes[i];
		}
		if (bytes[i] >= 0xB0u && bytes[i] <= 0xB7u) {
			g_page = (uint8_t)(bytes[i] - 0xB0u);
		} else if (bytes[i] <= 0x0Fu) {
			g_column = (uint8_t)((g_column & 0xF0u) | bytes[i]);
		} else if (bytes[i] >= 0x10u && bytes[i] <= 0x1Fu) {
			g_column =
			    (uint8_t)((g_column & 0x0Fu) | (uint8_t)((bytes[i] & 0x0Fu) << 4));
		}
	}
	return OSK_OK;
}

static osk_result_t bus_data(void *ctx, const uint8_t *bytes, size_t len)
{
	(void)ctx;
	if (g_fail) {
		return OSK_ERR_IO;
	}
	/* Der Spaltenversatz wird hier wieder abgezogen - stimmt er nicht,
	 * landet das Bild daneben, und genau das soll auffallen. */
	for (size_t i = 0; i < len; i++) {
		int x = (int)g_column - OSK_DISPLAY_COL_OFFSET + (int)i;

		if (x >= 0 && x < OSK_DISPLAY_WIDTH && g_page < OSK_DISPLAY_PAGES) {
			g_image.pixels[(size_t)g_page * OSK_DISPLAY_WIDTH + (size_t)x] = bytes[i];
		}
	}
	g_data_bytes += len;
	if (g_page + 1u == OSK_DISPLAY_PAGES) {
		g_flushes++;
	}
	return OSK_OK;
}

static osk_result_t bus_reset(void *ctx, bool active)
{
	(void)ctx;
	if (!active) {
		g_reset_done = true;
	}
	return g_fail ? OSK_ERR_IO : OSK_OK;
}

static osk_result_t bus_power(void *ctx, bool on)
{
	(void)ctx;
	g_powered = on;
	return g_fail ? OSK_ERR_IO : OSK_OK;
}

static void bus_delay(void *ctx, uint32_t ms)
{
	(void)ctx;
	g_now += ms;
}

static bool port_button(void *ctx)
{
	(void)ctx;
	if (g_script_at < g_script_count) {
		return g_script[g_script_at].down;
	}
	return false;
}

static uint32_t port_now(void *ctx)
{
	(void)ctx;
	return g_now;
}

static void port_led(void *ctx, osk_led_t color)
{
	(void)ctx;
	g_led = color;
}

/* Die Uhr laeuft nur hier: ein Durchlauf der Warteschleife, ein Schritt. */
static void port_idle(void *ctx)
{
	(void)ctx;
	g_now += FAKE_PANEL_STEP_MS;
	if (g_script_at < g_script_count) {
		g_in_segment += FAKE_PANEL_STEP_MS;
		if (g_in_segment >= g_script[g_script_at].ms) {
			g_script_at++;
			g_in_segment = 0;
		}
	}
}

static bool g_verified;

void fake_panel_set_verified(bool verified)
{
	g_verified = verified;
}

static bool port_verified(void *ctx)
{
	(void)ctx;
	return g_verified;
}

static const osk_ui_port_t PORT = {
    .display = {.cmd = bus_cmd,
		.data = bus_data,
		.reset = bus_reset,
		.power = bus_power,
		.delay_ms = bus_delay,
		.ctx = NULL},
    .button_down = port_button,
    .now_ms = port_now,
    .led = port_led,
    .idle = port_idle,
    .user_verified = port_verified,
    .ctx = NULL,
};

const osk_ui_port_t *fake_panel_port(void)
{
	return &PORT;
}

size_t fake_panel_cmd_count(void)
{
	return g_cmd_count;
}

uint8_t fake_panel_cmd(size_t index)
{
	return (index < g_cmd_count) ? g_cmds[index] : 0;
}

bool fake_panel_sent_cmd(uint8_t byte)
{
	for (size_t i = 0; i < g_cmd_count; i++) {
		if (g_cmds[i] == byte) {
			return true;
		}
	}
	return false;
}

size_t fake_panel_flushes(void)
{
	return g_flushes;
}

size_t fake_panel_data_bytes(void)
{
	return g_data_bytes;
}

bool fake_panel_powered(void)
{
	return g_powered;
}

bool fake_panel_reset_done(void)
{
	return g_reset_done;
}

osk_led_t fake_panel_led(void)
{
	return g_led;
}

void fake_panel_fail_bus(bool fail)
{
	g_fail = fail;
}

const osk_gfx_t *fake_panel_image(void)
{
	return &g_image;
}

bool fake_panel_pixel(int16_t x, int16_t y)
{
	return osk_gfx_get(&g_image, x, y);
}

size_t fake_panel_ink(void)
{
	size_t count = 0;

	for (int16_t y = 0; y < OSK_DISPLAY_HEIGHT; y++) {
		for (int16_t x = 0; x < OSK_DISPLAY_WIDTH; x++) {
			count += osk_gfx_get(&g_image, x, y) ? 1u : 0u;
		}
	}
	return count;
}

void fake_panel_set_now(uint32_t ms)
{
	g_now = ms;
}

uint32_t fake_panel_now(void)
{
	return g_now;
}

void fake_panel_push(bool down, uint32_t ms)
{
	if (g_script_count < SCRIPT_MAX) {
		g_script[g_script_count].down = down;
		g_script[g_script_count].ms = ms;
		g_script_count++;
	}
}

void fake_panel_tap(uint32_t hold_ms)
{
	fake_panel_push(true, hold_ms);
	fake_panel_push(false, OSK_BTN_DOUBLE_MS + 100u);
}

void fake_panel_print(const char *title)
{
	printf("    %s (%s)\n", (title != NULL) ? title : "Bild", OSK_DISPLAY_NAME);
	for (int16_t y = 0; y < OSK_DISPLAY_HEIGHT; y++) {
		printf("    |");
		for (int16_t x = 0; x < OSK_DISPLAY_WIDTH; x++) {
			putchar(osk_gfx_get(&g_image, x, y) ? '#' : '.');
		}
		printf("|\n");
	}
}

/* Steht an (x, y) genau dieses Zeichen? Verglichen werden alle Zeilen der
 * Schriftstelle - ein anderes Zeichen faellt damit durch. */
static bool glyph_at(const osk_font_t *font, size_t index, int16_t x, int16_t y)
{
	for (uint8_t row = 0; row < font->rows; row++) {
		uint8_t want = osk_font_row(font, index, row);

		for (uint8_t col = 0; col < font->width; col++) {
			bool on = (want & (uint8_t)(1u << col)) != 0;

			if (on != osk_gfx_get(&g_image, (int16_t)(x + col), (int16_t)(y + row))) {
				return false;
			}
		}
	}
	return true;
}

static bool needle_at(const osk_font_t *font, const char *needle, int16_t x, int16_t y)
{
	size_t len = strlen(needle);
	size_t pos = 0;
	int16_t pen = x;

	while (pos < len) {
		uint32_t codepoint = 0;
		size_t step = osk_utf8_next(needle, len, pos, &codepoint);

		if (step == 0) {
			break;
		}
		if (!glyph_at(font, osk_font_index(codepoint), pen, y)) {
			return false;
		}
		pen = (int16_t)(pen + font->advance);
		pos += step;
	}
	return true;
}

bool fake_panel_shows_in(const osk_font_t *font, const char *needle)
{
	if (font == NULL || needle == NULL || needle[0] == '\0') {
		return false;
	}
	for (int16_t y = 0; y + font->rows <= OSK_DISPLAY_HEIGHT; y++) {
		for (int16_t x = 0; x < OSK_DISPLAY_WIDTH; x++) {
			if (needle_at(font, needle, x, y)) {
				return true;
			}
		}
	}
	return false;
}

bool fake_panel_shows(const char *needle)
{
	return fake_panel_shows_in(&osk_font_small, needle) ||
	       fake_panel_shows_in(&osk_font_big, needle);
}
