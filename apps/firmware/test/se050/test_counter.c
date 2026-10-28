/* Monotone Zaehler gegen den Nachbau.
 *
 * Der Nachbau fuehrt die Zaehler hier wirklich (sim_objects_live), statt
 * eine festgelegte Antwort abzuspulen. Das ist der Unterschied, auf den
 * es ankommt: ein Zaehler, der nur eine abgespulte Antwort zurueckgibt,
 * zaehlt nicht, und dass nichts zurueckgeht, waere nicht pruefbar.
 *
 * Grenze wie in se050_sim.h: Nachbau und Treiber lesen AN12413 gleich.
 * Am Geraet ist nichts davon gelaufen. */
#include "tiny_test.h"
#include "fake_bus.h"
#include "se050_sim.h"
#include "osk_se050.h"
#include "se050_cmd.h"

/* Liegt in test_contract.c, wo auch die Platinenhaken stehen. */
int boot_sim(fake_bus_t *fb, se050_sim_t *sim);

#define CNT_A 0x30000001u
#define CNT_B 0x30000002u
#define CNT_UNKNOWN 0x3000FFFFu

static int boot_counters(fake_bus_t *fb, se050_sim_t *sim)
{
	fb_init(fb);
	sim_attach(sim, fb);
	sim_objects_live(sim, true);
	return boot_sim(fb, sim) != 0;
}

void test_counter(void)
{
	TT_CASE("Zaehler: ohne Anmeldung antwortet nichts");
	{
		osk_se050_close();

		uint64_t v = 0;
		TT_EQ_INT(osk_se050_counter_create(CNT_A), OSK_ERR_IO, "create");
		TT_EQ_INT(osk_se050_counter_read(CNT_A, &v), OSK_ERR_IO, "read");
		TT_EQ_INT(osk_se050_counter_raise(CNT_A, 1), OSK_ERR_IO, "raise");
	}

	TT_CASE("Zaehler: anlegen nennt die Laenge, nicht einen Wert");
	{
		fake_bus_t fb;
		se050_sim_t sim;
		TT_TRUE(boot_counters(&fb, &sim), "angemeldet");

		TT_EQ_INT(osk_se050_counter_create(CNT_A), OSK_OK, "angelegt");
		TT_EQ_INT(sim.last_ins, INS_WRITE, "INS_WRITE");
		TT_EQ_INT(sim.last_p1, P1_COUNTER, "P1_COUNTER");
		TT_EQ_INT(sim.last_p2, P2_DEFAULT, "P2_DEFAULT");

		/* TAG_1 Kennung (4 Byte), TAG_2 Laenge in BYTE (2 Byte).
		 * Acht Byte, weil der Vertrag uint64_t fuehrt - jede andere
		 * Laenge waere eine stille Obergrenze. */
		TT_EQ_INT(sim.last_payload_len, 6u + 4u, "zwei Felder");
		TT_EQ_INT(sim.last_payload[6], TAG_2, "TAG_2");
		TT_EQ_INT((sim.last_payload[8] << 8) | sim.last_payload[9], 8,
			  "acht Byte Zaehlerlaenge");

		/* KEIN TAG_3: mit einem Wert waere es ein Setzen, und bei
		 * einem vorhandenen Zaehler ein Hochzaehlen. */
		for (size_t i = 0; i < sim.last_payload_len; i++) {
			if (sim.last_payload[i] == TAG_3) {
				TT_TRUE(false, "kein TAG_3 beim Anlegen");
			}
		}

		uint64_t v = 123;
		TT_EQ_INT(osk_se050_counter_read(CNT_A, &v), OSK_OK, "gelesen");
		TT_EQ_INT(v, 0, "neu angelegt steht er auf null");
		TT_EQ_INT(osk_se050_counter_read(CNT_A, NULL), OSK_ERR_ARG, "NULL");

		osk_se050_close();
	}

	TT_CASE("Zaehler: zweimal anlegen zaehlt NICHT weiter");
	{
		/* Die Falle: WriteCounter ohne TAG_3 heisst bei einem
		 * vorhandenen Zaehler "um eins erhoehen" (AN12413 4.7.1.6).
		 * Ein Einrichtungslauf, der zweimal laeuft, haette den
		 * Rueckdrehschutz damit still verschoben. */
		fake_bus_t fb;
		se050_sim_t sim;
		TT_TRUE(boot_counters(&fb, &sim), "angemeldet");

		TT_EQ_INT(osk_se050_counter_create(CNT_A), OSK_OK, "angelegt");
		TT_EQ_INT(osk_se050_counter_raise(CNT_A, 5), OSK_OK, "auf 5");

		TT_EQ_INT(osk_se050_counter_create(CNT_A), OSK_ERR_EXISTS, "zweites Anlegen meldet");

		uint64_t v = 0;
		TT_EQ_INT(osk_se050_counter_read(CNT_A, &v), OSK_OK, "gelesen");
		TT_EQ_INT(v, 5, "unveraendert 5");

		osk_se050_close();
	}

	TT_CASE("Zaehler: hochsetzen ist wiederholbar");
	{
		fake_bus_t fb;
		se050_sim_t sim;
		TT_TRUE(boot_counters(&fb, &sim), "angemeldet");
		TT_EQ_INT(osk_se050_counter_create(CNT_A), OSK_OK, "angelegt");

		TT_EQ_INT(osk_se050_counter_raise(CNT_A, 5), OSK_OK, "auf 5");
		TT_EQ_INT(sim.last_payload[6], TAG_3, "TAG_3 traegt den Wert");
		TT_EQ_INT(sim.last_payload[7], 8u, "acht Byte, so lang wie der Zaehler");
		TT_EQ_INT(sim.last_payload[15], 5u, "hoechstwertiges Byte zuerst");

		uint64_t v = 0;
		TT_EQ_INT(osk_se050_counter_read(CNT_A, &v), OSK_OK, "gelesen");
		TT_EQ_INT(v, 5, "steht auf 5");

		/* Derselbe Aufruf nochmal: nichts aendert sich, und es geht
		 * kein Befehl hinaus. Das braucht der Aufrufer nach einem
		 * Stromausfall mitten im Schreiben - er weiss nicht, ob der
		 * erste Aufruf angekommen ist. */
		size_t before = sim.commands;
		TT_EQ_INT(osk_se050_counter_raise(CNT_A, 5), OSK_OK, "zweimal derselbe Wert");
		TT_EQ_INT(sim.commands, before + 1u, "nur das Lesen, kein Schreiben");
		TT_EQ_INT(osk_se050_counter_read(CNT_A, &v), OSK_OK, "gelesen");
		TT_EQ_INT(v, 5, "immer noch 5");

		/* Ein kleinerer Wert ist keine Aenderung. Der Zaehler bleibt,
		 * wo er ist, und es geht kein Schreibbefehl hinaus. */
		before = sim.commands;
		TT_EQ_INT(osk_se050_counter_raise(CNT_A, 3), OSK_OK, "kleinerer Wert");
		TT_EQ_INT(sim.commands, before + 1u, "nur das Lesen");
		TT_EQ_INT(osk_se050_counter_read(CNT_A, &v), OSK_OK, "gelesen");
		TT_EQ_INT(v, 5, "nicht zurueckgedreht");

		/* Hoeher geht weiter. */
		TT_EQ_INT(osk_se050_counter_raise(CNT_A, 9), OSK_OK, "auf 9");
		TT_EQ_INT(osk_se050_counter_read(CNT_A, &v), OSK_OK, "gelesen");
		TT_EQ_INT(v, 9, "steht auf 9");

		/* Null ist immer schon erreicht. */
		before = sim.commands;
		TT_EQ_INT(osk_se050_counter_raise(CNT_A, 0), OSK_OK, "auf null");
		TT_EQ_INT(sim.commands, before + 1u, "nur das Lesen");

		osk_se050_close();
	}

	TT_CASE("Zaehler: ueberlebt einen Neustart der Firmware");
	{
		/* DER FALL, UM DEN ES GEHT. Ein Test, der nur innerhalb
		 * eines Laufs zaehlt, zeigt den Fehler nicht: der Zaehler
		 * koennte im RAM des nRF52840 liegen und bei jedem
		 * Einstecken bei null anfangen. Also wird der Treiber
		 * geschlossen und neu geoeffnet - der Baustein behaelt
		 * seine Objekte, der Treiber verliert alles. */
		fake_bus_t fb;
		se050_sim_t sim;
		TT_TRUE(boot_counters(&fb, &sim), "angemeldet");
		TT_EQ_INT(osk_se050_counter_create(CNT_A), OSK_OK, "angelegt");
		TT_EQ_INT(osk_se050_counter_raise(CNT_A, 42), OSK_OK, "auf 42");

		/* Stecker gezogen. */
		osk_se050_close();
		fb_init(&fb);
		sim_reattach(&sim, &fb);
		TT_TRUE(boot_sim(&fb, &sim) != 0, "neu angemeldet");

		uint64_t v = 0;
		TT_EQ_INT(osk_se050_counter_read(CNT_A, &v), OSK_OK, "gelesen");
		TT_EQ_INT(v, 42, "nach dem Neustart noch 42 - faengt nicht bei null an");

		/* Und er laesst sich nach dem Neustart nicht zurueckdrehen. */
		TT_EQ_INT(osk_se050_counter_raise(CNT_A, 7), OSK_OK, "kleinerer Wert");
		TT_EQ_INT(osk_se050_counter_read(CNT_A, &v), OSK_OK, "gelesen");
		TT_EQ_INT(v, 42, "unveraendert");

		/* Anlegen meldet auch nach dem Neustart, dass es ihn gibt -
		 * der Einrichtungsassistent darf zweimal laufen. */
		TT_EQ_INT(osk_se050_counter_create(CNT_A), OSK_ERR_EXISTS, "gibt es schon");
		TT_EQ_INT(osk_se050_counter_read(CNT_A, &v), OSK_OK, "gelesen");
		TT_EQ_INT(v, 42, "und das Melden zaehlt nicht weiter");

		osk_se050_close();
	}

	TT_CASE("Zaehler: zwei Zaehler stoeren sich nicht");
	{
		/* Strang B zaehlt FIDO-Signaturen, Strang D haengt den
		 * Rueckdrehschutz des Tresors daran. Beide brauchen einen
		 * eigenen. */
		fake_bus_t fb;
		se050_sim_t sim;
		TT_TRUE(boot_counters(&fb, &sim), "angemeldet");

		TT_EQ_INT(osk_se050_counter_create(CNT_A), OSK_OK, "erster");
		TT_EQ_INT(osk_se050_counter_create(CNT_B), OSK_OK, "zweiter");
		TT_EQ_INT(osk_se050_counter_raise(CNT_A, 1000), OSK_OK, "A auf 1000");

		uint64_t a = 0;
		uint64_t b = 1;
		TT_EQ_INT(osk_se050_counter_read(CNT_A, &a), OSK_OK, "A gelesen");
		TT_EQ_INT(osk_se050_counter_read(CNT_B, &b), OSK_OK, "B gelesen");
		TT_EQ_INT(a, 1000, "A steht auf 1000");
		TT_EQ_INT(b, 0, "B steht noch auf null");

		osk_se050_close();
	}

	TT_CASE("Zaehler: grosse Werte, bis an die Obergrenze");
	{
		fake_bus_t fb;
		se050_sim_t sim;
		TT_TRUE(boot_counters(&fb, &sim), "angemeldet");
		TT_EQ_INT(osk_se050_counter_create(CNT_A), OSK_OK, "angelegt");

		/* Obergrenze eines Acht-Byte-Zaehlers: 0xFFFFFFFFFFFFFFFF.
		 * "When a counter reaches its maximum value ... it cannot be
		 * incremented again" (AN12413 4.7.1.6). */
		const uint64_t max = 0xFFFFFFFFFFFFFFFFull;
		TT_EQ_INT(osk_se050_counter_raise(CNT_A, max), OSK_OK, "auf die Obergrenze");

		uint64_t v = 0;
		TT_EQ_INT(osk_se050_counter_read(CNT_A, &v), OSK_OK, "gelesen");
		TT_TRUE(v == max, "steht auf der Obergrenze");

		/* Auch dort ist der Aufruf noch wiederholbar. */
		TT_EQ_INT(osk_se050_counter_raise(CNT_A, max), OSK_OK, "wiederholbar");

		osk_se050_close();
	}

	TT_CASE("Zaehler: ein kuerzerer Zaehler wird mit seiner Laenge bedient");
	{
		/* Die Laenge wird gelesen, nicht angenommen: "An input value
		 * (TAG_3) must always have the same length as the existing
		 * counter; otherwise the command will return an error"
		 * (AN12413 4.7.1.6). Ein Zaehler, den jemand anders mit vier
		 * Byte angelegt hat, muss trotzdem bedienbar sein. */
		fake_bus_t fb;
		se050_sim_t sim;
		TT_TRUE(boot_counters(&fb, &sim), "angemeldet");

		/* Vier Byte, am Nachbau vorbei am Treiber eingerichtet. */
		sim.counters[0].used = true;
		sim.counters[0].id = CNT_B;
		sim.counters[0].len = 4u;
		sim.counters[0].value = 10u;

		uint64_t v = 0;
		TT_EQ_INT(osk_se050_counter_read(CNT_B, &v), OSK_OK, "gelesen");
		TT_EQ_INT(v, 10, "steht auf 10");

		TT_EQ_INT(osk_se050_counter_raise(CNT_B, 0xFFFFFFFFull), OSK_OK, "auf 2^32-1");
		TT_EQ_INT(sim.last_payload[7], 4u, "vier Byte, nicht acht");
		TT_EQ_INT(osk_se050_counter_read(CNT_B, &v), OSK_OK, "gelesen");
		TT_TRUE(v == 0xFFFFFFFFull, "steht auf 2^32-1");

		/* Darueber passt es nicht mehr hinein. Das ist ein Fehler des
		 * Aufrufers und wird als solcher gemeldet, statt den Baustein
		 * ablehnen zu lassen. */
		const size_t before = sim.commands;
		TT_EQ_INT(osk_se050_counter_raise(CNT_B, 0x100000000ull), OSK_ERR_ARG,
			  "2^32 passt nicht in vier Byte");
		TT_EQ_INT(sim.commands, before + 1u, "nur das Lesen, kein Schreiben");

		osk_se050_close();
	}

	TT_CASE("Zaehler: eine unbekannte Kennung wird gemeldet, nicht erfunden");
	{
		fake_bus_t fb;
		se050_sim_t sim;
		TT_TRUE(boot_counters(&fb, &sim), "angemeldet");

		uint64_t v = 7;
		TT_TRUE(osk_se050_counter_read(CNT_UNKNOWN, &v) != OSK_OK, "lesen scheitert");
		TT_EQ_INT(v, 0, "und laesst keine alte Zahl stehen");
		TT_TRUE(osk_se050_counter_raise(CNT_UNKNOWN, 1) != OSK_OK, "hochsetzen scheitert");
		TT_TRUE(fb_clean(&fb), "Testbus ohne Beanstandung");

		osk_se050_close();
	}
}
