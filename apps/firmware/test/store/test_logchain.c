/* Das hash-verkettete Geraeteprotokoll.
 *
 * Ein Ringpuffer allein schuetzt nicht: wer den Flash schreiben kann, loescht
 * einen einzelnen Platz, und niemand sieht, dass dort etwas stand. Jeder Satz
 * bindet darum den vorhergehenden ein - die ersten 16 Byte von dessen
 * Pruefwert. Der Pruefwert haengt am Protokollschluessel, der im SE050
 * wurzelt, also laesst sich eine Kette ohne diesen Schluessel nicht ohne
 * Luecke neu rechnen.
 *
 * Gemessen wird hier an der ECHTEN Ablage im Flash, nicht an einem Zaehler im
 * RAM: die Saetze werden im Flash gesucht und dort veraendert. */

#include <string.h>
#include "osk_test.h"
#include "test_util.h"
#include "fake_flash.h"
#include "osk_flash.h"
#include "osk_log.h"
#include "osk_store.h"
#include "osk_store_profiles.h"
#include "store_internal.h"

/* Adresse des Protokollplatzes mit dieser Folgenummer, oder 0. */
static uint32_t slot_of_seq(uint32_t seq)
{
	for (uint32_t i = 0; i < OSK_LOG_SLOTS; i++) {
		uint32_t addr = OSK_AREA_LOG_OFF + i * OSK_LOG_SLOT;
		osk_log_slot_t s;

		if (osk_flash_read(addr, &s, sizeof(s)) != OSK_OK) {
			return 0u;
		}
		if (s.magic == OSK_LOG_MAGIC && s.seq == seq) {
			return addr;
		}
	}
	return 0u;
}

/* Einen Platz unkenntlich machen - so, wie es jemand taete, der einen
 * einzelnen Eintrag loswerden will: der Sektor wird geloescht und die
 * NACHBARN werden unveraendert zurueckgeschrieben. Der Ring sieht danach
 * sauber aus, nur einer fehlt. */
static void drop_slot(uint32_t addr)
{
	uint32_t sector = addr & ~(OSK_SECTOR - 1u);
	uint8_t keep[OSK_SECTOR];

	T_OK(osk_flash_read(sector, keep, sizeof(keep)));
	memset(keep + (addr - sector), 0xFF, OSK_LOG_SLOT);
	T_OK(osk_flash_erase(sector, OSK_SECTOR));
	T_OK(osk_flash_program(sector, keep, sizeof(keep)));
	T_OK(osk_flash_sync());
}

void test_logchain(void)
{
	size_t count = 0, checked = 0, broke = 0;
	uint32_t victim = 0;

	osk_test_case("Die Kette haelt ueber ein frisch beschriebenes Protokoll");
	t_device(0, 1, T_PIN);
	for (int i = 0; i < 12; i++) {
		char name[16];

		snprintf(name, sizeof(name), "E%d", i);
		T_OK(t_add(0, name, "x.example", "geheim", 0, NULL));
	}
	T_OK(osk_log_count(&count));
	T_TRUE(count > 12u);
	T_OK(osk_log_verify(&checked, &broke));
	T_EQ(checked, count - 1u);

	osk_test_case("Jeder Satz laesst sich lesen");
	for (size_t i = 0; i < count; i++) {
		osk_log_entry_t e;

		T_OK(osk_log_get(i, &e));
		T_TRUE(e.what != 0u);
	}

	osk_test_case("Ein herausgenommener Satz faellt auf");
	{
		/* Mitten hinein, nicht an den Rand: am Rand koennte die
		 * Pruefung schon aus einem anderen Grund aufhoeren. */
		osk_log_entry_t e;
		uint32_t seq_mid = 0;

		T_OK(osk_log_get(5, &e));
		/* Die Folgenummer steht nicht im Eintrag - also wird der
		 * Platz ueber die Reihenfolge gesucht: der sechstjuengste. */
		for (uint32_t s = 1; s <= 64u; s++) {
			if (slot_of_seq(s) != 0u) {
				seq_mid = s;
			}
		}
		T_TRUE(seq_mid > 6u);
		victim = slot_of_seq(seq_mid - 3u);
		T_TRUE(victim != 0u);
		drop_slot(victim);

		T_RES(osk_log_verify(&checked, &broke), OSK_ERR_FORMAT);
	}

	osk_test_case("Nach dem Neustart faellt es immer noch auf");
	{
		osk_store_lock();
		fake_flash_revive();
		T_OK(osk_store_open());
		T_RES(osk_log_verify(&checked, &broke), OSK_ERR_FORMAT);
	}

	osk_test_case("Ein umgeschriebenes Kettenglied macht den Satz unlesbar");
	{
		osk_log_slot_t s;
		uint32_t addr;
		size_t n_before = 0, n_after = 0;

		t_device(0, 2, T_PIN);
		for (int i = 0; i < 6; i++) {
			T_OK(t_add(0, "E", "x.example", "geheim", 0, NULL));
		}
		T_OK(osk_log_count(&n_before));

		/* Den jueengsten Satz suchen und sein Kettenglied verdrehen.
		 * Das Glied ist zugleich zusaetzliche Daten der
		 * Verschluesselung - wer es anfasst, faelscht nicht die
		 * Reihenfolge, sondern zerstoert den Satz. */
		addr = 0u;
		for (uint32_t seq = 1; seq <= 64u; seq++) {
			uint32_t a = slot_of_seq(seq);

			if (a != 0u) {
				addr = a;
			}
		}
		T_TRUE(addr != 0u);
		T_OK(osk_flash_read(addr, &s, sizeof(s)));
		/* Der Sektor wird ohnehin geloescht und ganz neu
		 * geschrieben - der Angreifer ist hier also nicht darauf
		 * beschraenkt, Bits auf 0 zu ziehen. Darum wird das Byte
		 * sicher veraendert und nicht nur vielleicht: ein
		 * UND mit 0x7F waere wirkungslos, wenn das oberste Bit schon
		 * 0 ist, und der Fall maesse dann nichts. */
		s.chain[0] ^= (uint8_t)0xFFu;
		s.crc32 = osk_crc32(&s, offsetof(osk_log_slot_t, crc32));
		{
			/* Geschrieben wird eine ganze Seite - der Flash nimmt
			 * nichts Kleineres. Der Rest des Sektors muss dabei
			 * erhalten bleiben, sonst misst der Fall etwas
			 * anderes als das eine verdrehte Bit. */
			uint32_t sector = addr & ~(OSK_SECTOR - 1u);
			uint8_t keep[OSK_SECTOR];

			T_OK(osk_flash_read(sector, keep, sizeof(keep)));
			memcpy(keep + (addr - sector), &s, sizeof(s));
			T_OK(osk_flash_erase(sector, OSK_SECTOR));
			T_OK(osk_flash_program(sector, keep, sizeof(keep)));
			T_OK(osk_flash_sync());
		}

		osk_store_lock();
		T_OK(osk_store_open());
		T_OK(osk_log_count(&n_after));
		/* Der Satz zaehlt noch - seine Pruefsumme stimmt ja -, aber
		 * lesen laesst er sich nicht mehr. */
		{
			osk_log_entry_t e;
			bool one_broken = false;

			for (size_t i = 0; i < n_after; i++) {
				if (osk_log_get(i, &e) != OSK_OK) {
					one_broken = true;
				}
			}
			T_TRUE(one_broken);
		}
	}

	osk_test_case("Ein umgelaufener Ring ist kein Befund");
	{
		/* Der wichtigste Gegenfall: die Pruefung haelt eine Luecke
		 * fuer einen Angriff. Ein Ring, der wirklich umgelaufen ist,
		 * hat aber auch Luecken - die geloeschten Sektoren. Wuerde
		 * sie dort anschlagen, waere sie ein Fehlalarm bei jedem
		 * Geraet, das laenger als 512 Eintraege benutzt wurde, und
		 * damit wertlos. */
		size_t n = 0;

		t_device(0, 4, T_PIN);
		for (int i = 0; i < 700; i++) {
			T_OK(t_add(0, "E", "x.example", "geheim", 0, NULL));
			T_OK(osk_store_delete(t_find_name(0, "E")));
		}
		T_OK(osk_log_count(&n));
		/* Der Ring ist wirklich voll - sonst misst der Fall nichts. */
		T_TRUE(n > 400u);
		T_OK(osk_log_verify(&checked, &broke));
		T_EQ(checked, n - 1u);
	}

	osk_test_case("Ein leeres Protokoll ist kein Befund");
	{
		t_device(0, 3, T_PIN);
		T_OK(osk_log_clear());
		T_OK(osk_log_verify(&checked, &broke));
		T_EQ(checked, 0);
	}

	osk_test_case("Ohne offenen Tresor wird nichts geprueft");
	osk_store_lock();
	T_RES(osk_log_verify(&checked, &broke), OSK_ERR_LOCKED);
}
