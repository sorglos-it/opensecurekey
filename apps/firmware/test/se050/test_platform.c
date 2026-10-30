/* Umsetzung der drei Platinenhaken fuer den Prueflauf. Vertrag und Grund,
 * warum sie nicht mehr in test_contract.c stehen: test_platform.h. */
#include <string.h>
#include <stdbool.h>

#include "test_platform.h"
#include "se050_sim.h"
#include "se050_platform.h"

fake_bus_t *g_bus;

static osk_scp03_static_keys_t g_host_keys;
static bool g_host_keys_set;

void tp_set_host_keys(const osk_scp03_static_keys_t *keys)
{
	if (keys == NULL) {
		memset(&g_host_keys, 0, sizeof(g_host_keys));
		g_host_keys_set = false;
		return;
	}
	g_host_keys = *keys;
	g_host_keys_set = true;
}

osk_result_t osk_se050_platform_bus(osk_se050_bus_t **bus_out)
{
	if (g_bus == NULL) {
		return OSK_ERR_IO;
	}
	*bus_out = &g_bus->bus;
	return OSK_OK;
}

osk_result_t osk_se050_platform_scp_keys(osk_scp03_static_keys_t *keys_out)
{
	*keys_out = g_host_keys_set ? g_host_keys : *sim_keys();
	return OSK_OK;
}

osk_result_t osk_se050_platform_channel_obj(uint32_t *obj_id_out)
{
	*obj_id_out = TEST_CHANNEL_OBJ;
	return OSK_OK;
}
