/* Die Griffe am echten Geraet: SPI zur Anzeige, GPIO fuer Reset, Strom,
 * Taster und LED. Das ist die einzige Datei, die den nRF52840 kennt.
 *
 * ACHTUNG - Stand 24.10.2026: diese Datei ist NIE UEBERSETZT worden. Es gibt
 * noch kein Geraet und noch keine Geraetebeschreibung (Overlay), in der die
 * Knoten `oled`, `oled_dc`, `oled_res`, `oled_pwr`, `osk_button` und
 * `osk_led_*` stehen. Alles darueber (Treiber, Bilder, Taster, Sprachen) ist
 * am PC geprueft; dieser Teil ist geschrieben und sonst nichts.
 *
 * Darum steht er hinter #ifdef __ZEPHYR__: in jedem anderen Bau ist die Datei
 * leer und kann nichts kaputt machen. */

#ifdef __ZEPHYR__

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>
#include "osk_ui_port.h"

/* Die Anzeige haengt als einziges Teil an ihrem SPI: 8 Bit, Modus 0, /CS
 * verwaltet der Treiber von Zephyr. Die Taktrate kommt aus dem Overlay. */
static const struct spi_dt_spec oled_spi =
    SPI_DT_SPEC_GET(DT_NODELABEL(oled), SPI_WORD_SET(8) | SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB, 0);

static const struct gpio_dt_spec oled_dc = GPIO_DT_SPEC_GET(DT_NODELABEL(oled_dc), gpios);
static const struct gpio_dt_spec oled_res = GPIO_DT_SPEC_GET(DT_NODELABEL(oled_res), gpios);
static const struct gpio_dt_spec oled_pwr = GPIO_DT_SPEC_GET(DT_NODELABEL(oled_pwr), gpios);
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(DT_NODELABEL(osk_button), gpios);
static const struct gpio_dt_spec led_r = GPIO_DT_SPEC_GET(DT_NODELABEL(osk_led_red), gpios);
static const struct gpio_dt_spec led_g = GPIO_DT_SPEC_GET(DT_NODELABEL(osk_led_green), gpios);
static const struct gpio_dt_spec led_b = GPIO_DT_SPEC_GET(DT_NODELABEL(osk_led_blue), gpios);

static osk_result_t write_spi(const uint8_t *bytes, size_t len, bool is_data)
{
	const struct spi_buf buf = {.buf = (void *)bytes, .len = len};
	const struct spi_buf_set set = {.buffers = &buf, .count = 1};

	if (bytes == NULL || len == 0) {
		return OSK_ERR_ARG;
	}
	/* D/C entscheidet, ob der Controller die Bytes als Befehl oder als Bild
	 * liest. Umgekehrt stehen Befehle im Bild - darum zuerst die Leitung. */
	if (gpio_pin_set_dt(&oled_dc, is_data ? 1 : 0) != 0) {
		return OSK_ERR_IO;
	}
	return (spi_write_dt(&oled_spi, &set) == 0) ? OSK_OK : OSK_ERR_IO;
}

static osk_result_t port_cmd(void *ctx, const uint8_t *bytes, size_t len)
{
	ARG_UNUSED(ctx);
	return write_spi(bytes, len, false);
}

static osk_result_t port_data(void *ctx, const uint8_t *bytes, size_t len)
{
	ARG_UNUSED(ctx);
	return write_spi(bytes, len, true);
}

static osk_result_t port_reset(void *ctx, bool active)
{
	ARG_UNUSED(ctx);
	/* /RES ist im Overlay als GPIO_ACTIVE_LOW eingetragen: 1 heisst Reset. */
	return (gpio_pin_set_dt(&oled_res, active ? 1 : 0) == 0) ? OSK_OK : OSK_ERR_IO;
}

static osk_result_t port_power(void *ctx, bool on)
{
	ARG_UNUSED(ctx);
	return (gpio_pin_set_dt(&oled_pwr, on ? 1 : 0) == 0) ? OSK_OK : OSK_ERR_IO;
}

static void port_delay(void *ctx, uint32_t ms)
{
	ARG_UNUSED(ctx);
	k_msleep((int32_t)ms);
}

static bool port_button(void *ctx)
{
	ARG_UNUSED(ctx);
	return gpio_pin_get_dt(&button) > 0;
}

static uint32_t port_now(void *ctx)
{
	ARG_UNUSED(ctx);
	return (uint32_t)k_uptime_get_32();
}

static void port_led(void *ctx, osk_led_t color)
{
	bool red = (color == OSK_LED_RED) || (color == OSK_LED_AMBER);
	bool green = (color == OSK_LED_GREEN) || (color == OSK_LED_AMBER);
	bool blue = (color == OSK_LED_BLUE);

	ARG_UNUSED(ctx);
	(void)gpio_pin_set_dt(&led_r, red ? 1 : 0);
	(void)gpio_pin_set_dt(&led_g, green ? 1 : 0);
	(void)gpio_pin_set_dt(&led_b, blue ? 1 : 0);
}

/* Warten, ohne den Rechenkern zu beschaeftigen: der Taster weckt ueber
 * GPIOTE im PORT-Modus, darum reicht ein kurzer Schlaf je Durchlauf. */
static void port_idle(void *ctx)
{
	ARG_UNUSED(ctx);
	k_msleep(5);
}

static const struct gpio_dt_spec *const OUTPUTS[] = {&oled_dc, &oled_res, &oled_pwr,
						     &led_r,   &led_g,	  &led_b};

osk_result_t osk_ui_port_nrf_start(bool (*user_verified)(void *ctx))
{
	osk_ui_port_t port = {
	    .display = {.cmd = port_cmd,
			.data = port_data,
			.reset = port_reset,
			.power = port_power,
			.delay_ms = port_delay,
			.ctx = NULL},
	    .button_down = port_button,
	    .now_ms = port_now,
	    .led = port_led,
	    .idle = port_idle,
	    .user_verified = user_verified,
	    .ctx = NULL,
	};

	if (!spi_is_ready_dt(&oled_spi)) {
		return OSK_ERR_IO;
	}
	for (size_t i = 0; i < ARRAY_SIZE(OUTPUTS); i++) {
		if (!gpio_is_ready_dt(OUTPUTS[i]) ||
		    gpio_pin_configure_dt(OUTPUTS[i], GPIO_OUTPUT_INACTIVE) != 0) {
			return OSK_ERR_IO;
		}
	}
	if (!gpio_is_ready_dt(&button) ||
	    gpio_pin_configure_dt(&button, GPIO_INPUT | GPIO_PULL_UP) != 0) {
		return OSK_ERR_IO;
	}
	return osk_ui_begin(&port);
}

#endif /* __ZEPHYR__ */
