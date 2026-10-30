/* Die Platinenhaken fuer den Prueflauf, an EINER Stelle.
 *
 * se050_platform.h verlangt drei Haken: Bus, SCP03-Schluessel und die
 * Kennung des Anmeldeobjekts. Im Geraet erfuellt sie die Platinen- und
 * Tresorschicht. Sie standen in test_contract.c, also in einer Pruefdatei;
 * der Erstlauf-Test braucht dieselben Haken und muss zusaetzlich steuern,
 * WELCHE Schluessel der Treiber nimmt - genau das tut auf dem Geraet die
 * Tresorschicht. Zwei Pruefdateien koennen dieselbe Funktion nicht beide
 * anbieten, darum liegen sie jetzt hier. */
#ifndef TEST_PLATFORM_H
#define TEST_PLATFORM_H

#include "fake_bus.h"
#include "se050_scp03.h"

/* Der Bus, den der Treiber bekommt. NULL heisst: keine Leitung, der
 * Treiber meldet OSK_ERR_IO. */
extern fake_bus_t *g_bus;

/* Kennung des Anmeldeobjekts fuer den Geraetekanal. Auf dem Geraet legt
 * das die Platine fest; hier ein fester Wert, den der Test nachprueft. */
#define TEST_CHANNEL_OBJ 0x0F0000FFu

/* Welche festen Schluessel der Haken ausgibt. NULL stellt die Vorgabe
 * wieder her: die Schluessel des Nachbaus (sim_keys()). */
void tp_set_host_keys(const osk_scp03_static_keys_t *keys);

#endif
