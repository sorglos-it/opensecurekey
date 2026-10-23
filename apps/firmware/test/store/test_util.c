/* Handgriffe fuer die Pruefsaetze - siehe test_util.h. */

#include <string.h>
#include "test_util.h"
#include "osk_test.h"
#include "fake_flash.h"
#include "fake_crypto.h"
#include "fake_se050.h"
#include "fake_ui.h"
#include "store_internal.h"
#include "osk_store_profiles.h"

static char g_pin[65] = T_PIN;
static uint32_t g_dev = 1u;

/* Feste Zeit: so sind alle Zeitstempel im Test vorhersagbar. */
static uint64_t t_time(void)
{
	return 1760000000ull;
}

void t_init(void)
{
	fake_flash_init();
	fake_crypto_init();
	fake_se050_reset();
	fake_ui_reset();
	osk_time_set_source(t_time);
}

void t_use(uint32_t dev, const char *pin)
{
	g_dev = dev;
	size_t n = strlen(pin);

	if (n > sizeof(g_pin) - 1u) {
		n = sizeof(g_pin) - 1u;
	}
	memset(g_pin, 0, sizeof(g_pin));
	memcpy(g_pin, pin, n);
	fake_se050_device(dev);
	fake_se050_set_pin(pin);
}

void t_device(int bank, uint32_t dev, const char *pin)
{
	osk_store_lock();
	fake_flash_select(bank);
	fake_flash_revive();
	fake_flash_wipe();
	fake_se050_reset();
	t_use(dev, pin);

	T_OK(osk_store_open());
	T_OK(osk_store_profile_create(0, (const uint8_t *)pin, strlen(pin)));
}

void t_reboot(void)
{
	/* Nach einem Stromausfall meldet das Flash Fehler. Das Sperren darf
	 * daran nicht haengen bleiben - es schreibt nur noch ins Protokoll. */
	osk_store_lock();
	fake_flash_revive();
	fake_se050_device(g_dev);
	fake_se050_set_pin(g_pin);
	T_OK(osk_store_open());
	T_OK(osk_store_profile_unlock(0, (const uint8_t *)g_pin, strlen(g_pin)));
}

osk_result_t t_add(uint8_t profile, const char *name, const char *site, const char *secret,
		   bool mirrored, osk_item_id_t *id_out)
{
	osk_item_meta_t meta;
	osk_item_id_t id = 0;

	memset(&meta, 0, sizeof(meta));
	meta.type = OSK_ITEM_LOGIN;
	meta.profile = profile;
	meta.name = name;
	meta.site = site;
	meta.mirrored = mirrored;

	osk_result_t r = osk_store_add(&meta, (const uint8_t *)secret, strlen(secret), &id);

	if (id_out != NULL) {
		*id_out = id;
	}
	return r;
}

bool t_secret_is(osk_item_id_t id, const char *want)
{
	uint8_t buf[OSK_SECRET_MAX];
	size_t len = 0;

	fake_ui_answer(true);
	if (osk_store_reveal(id, "test", buf, sizeof(buf), &len) != OSK_OK) {
		return false;
	}
	if (len != strlen(want)) {
		return false;
	}
	return memcmp(buf, want, len) == 0;
}

size_t t_count(uint8_t profile)
{
	size_t n = 0;

	if (osk_store_count(profile, &n) != OSK_OK) {
		return 0;
	}
	return n;
}

osk_item_id_t t_find_name(uint8_t profile, const char *name)
{
	size_t n = t_count(profile);

	for (size_t i = 0; i < n; i++) {
		osk_item_meta_t meta;

		if (osk_store_list(profile, i, &meta) != OSK_OK) {
			return 0;
		}
		if (strcmp(meta.name, name) == 0) {
			return meta.id;
		}
	}
	return 0;
}

bool memmem_missing(const uint8_t *hay, size_t hay_len, const char *needle)
{
	size_t n = strlen(needle);

	if (n == 0u || hay_len < n) {
		return true;
	}
	for (size_t i = 0; i + n <= hay_len; i++) {
		if (memcmp(hay + i, needle, n) == 0) {
			return false;
		}
	}
	return true;
}
