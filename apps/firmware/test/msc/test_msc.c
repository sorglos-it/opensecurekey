/* Der Leseweg des Laufwerks: Rahmen, Auskunftsbefehle, Lesen, Grenzen.
 *
 * Was hier bewiesen wird: ein Befehl, dessen Laengen nicht zum Laufwerk
 * passen, wird abgewiesen, BEVOR ein Block gelesen wird - und ein Lesebefehl
 * liefert genau die Bloecke, die er bestellt hat, mit dem Inhalt, der dort
 * steht. Kein Rechner hat dieses Laufwerk je gesehen; das hier laeuft am PC
 * gegen eine Attrappe im RAM. */
#include "test_util.h"
#include "mocks.h"

#define DRIVE_BLOCKS 64u

static void setup(void)
{
	osk_msc_config_t cfg;

	mock_transport_reset();
	fake_drive_init(DRIVE_BLOCKS);
	mock_sink_reset();

	memset(&cfg, 0, sizeof(cfg));
	cfg.read = fake_drive_read;
	cfg.send = mock_sink_send;
	cfg.blocks = DRIVE_BLOCKS;
	osk_msc_init(&cfg);
	mock_sink_reset();
}

/* Die Daten vor dem CSW. */
static const uint8_t *payload(size_t *len_out)
{
	const size_t total = mock_sink_len();

	*len_out = (total >= OSK_MSC_CSW_LEN) ? total - OSK_MSC_CSW_LEN : 0u;
	return mock_sink_data();
}

static uint8_t last_status(void)
{
	uint8_t status = 0xff;

	if (!mock_sink_csw(NULL, NULL, &status))
		return 0xfe;
	return status;
}

static uint32_t last_residue(void)
{
	uint32_t residue = 0xffffffffu;

	mock_sink_csw(NULL, &residue, NULL);
	return residue;
}

/* Holt die Sinnesdaten des letzten Fehlers ab. */
static void fetch_sense(uint8_t *key, uint8_t *asc)
{
	const uint8_t cdb[6] = {0x03, 0, 0, 0, 18, 0};
	size_t len = 0;

	mock_sink_reset();
	mock_cbw(0x5e5e5e5eu, 18, true, 0, cdb, 6);

	const uint8_t *p = payload(&len);

	*key = (len >= 18u) ? (uint8_t)(p[2] & 0x0fu) : 0xffu;
	*asc = (len >= 18u) ? p[12] : 0xffu;
}

static void case_init(void)
{
	osk_msc_config_t cfg;

	mock_transport_reset();
	memset(&cfg, 0, sizeof(cfg));
	T_EQ("ohne Einstellungen kein Laufwerk", osk_msc_init(NULL), OSK_ERR_ARG);
	T_EQ("ohne Leser kein Laufwerk", osk_msc_init(&cfg), OSK_ERR_ARG);

	cfg.read = fake_drive_read;
	cfg.send = mock_sink_send;
	cfg.blocks = 0;
	T_EQ("ein Laufwerk ohne Bloecke gibt es nicht", osk_msc_init(&cfg), OSK_ERR_ARG);
	T_TRUE("nichts angemeldet", !mock_transport_registered(OSK_LINK_USB_MSC));

	cfg.blocks = DRIVE_BLOCKS;
	T_EQ("so geht es", osk_msc_init(&cfg), OSK_OK);
	T_TRUE("beim Transport angemeldet", mock_transport_registered(OSK_LINK_USB_MSC));
	T_EQ("Groesse gemerkt", osk_msc_blocks(), DRIVE_BLOCKS);
	T_TRUE("eingeschaltet", osk_msc_enabled());
}

static void case_inquiry(void)
{
	const uint8_t cdb[6] = {0x12, 0, 0, 0, 36, 0};
	size_t len = 0;

	setup();
	mock_cbw(1, 36, true, 0, cdb, 6);

	const uint8_t *p = payload(&len);

	T_EQ("36 Byte Auskunft", len, 36);
	T_EQ("Blockgeraet", p[0], 0x00);
	T_EQ("Wechselmedium", p[1], 0x80);
	T_EQ("Antwortformat", p[3] & 0x0f, 0x02);
	T_EQ("zusaetzliche Laenge", p[4], 31);
	T_TRUE("Hersteller steht drin", memcmp(p + 8, "Sorglos ", 8) == 0);
	T_EQ("CSW gut", last_status(), OSK_MSC_CSW_PASSED);
	T_EQ("kein Rueckstand", last_residue(), 0);
	T_EQ("kein Block gelesen", fake_drive_reads(), 0);

	/* Weniger anfordern heisst: weniger bekommen, ohne Rueckstand. */
	const uint8_t shortc[6] = {0x12, 0, 0, 0, 8, 0};

	mock_sink_reset();
	mock_cbw(2, 8, true, 0, shortc, 6);
	payload(&len);
	T_EQ("nur 8 Byte", len, 8);
	T_EQ("auch dann gut", last_status(), OSK_MSC_CSW_PASSED);
	T_EQ("kein Rueckstand", last_residue(), 0);

	/* Mehr anfordern heisst: Rueckstand, nicht mehr Daten. */
	mock_sink_reset();
	mock_cbw(3, 64, true, 0, cdb, 6);
	payload(&len);
	T_EQ("trotzdem nur 36", len, 36);
	T_EQ("Rueckstand 28", last_residue(), 28);

	/* Eine Seitennummer ohne EVPD ist ein Widerspruch. */
	const uint8_t page[6] = {0x12, 0x00, 0x80, 0, 36, 0};

	mock_sink_reset();
	mock_cbw(4, 36, true, 0, page, 6);
	T_EQ("Seite ohne EVPD abgewiesen", last_status(), OSK_MSC_CSW_FAILED);

	uint8_t key = 0, asc = 0;

	fetch_sense(&key, &asc);
	T_EQ("Grund: unerlaubtes Feld", key, OSK_MSC_SK_ILLEGAL_REQUEST);
	T_EQ("Zusatz: Feld im Befehl", asc, OSK_MSC_ASC_INVALID_FIELD);

	/* Mit EVPD gibt es die Seriennummernseite - fuer alle Sticks gleich. */
	const uint8_t evpd[6] = {0x12, 0x01, 0x80, 0, 36, 0};

	mock_sink_reset();
	mock_cbw(5, 36, true, 0, evpd, 6);
	payload(&len);
	T_EQ("Seriennummernseite", len, 8);
	T_EQ("gut", last_status(), OSK_MSC_CSW_PASSED);
}

static void case_capacity(void)
{
	const uint8_t cdb10[10] = {0x25, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	size_t len = 0;

	setup();
	mock_cbw(1, 8, true, 0, cdb10, 10);

	const uint8_t *p = payload(&len);

	T_EQ("8 Byte", len, 8);
	T_EQ("letzter Block",
	     ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3],
	     DRIVE_BLOCKS - 1u);
	T_EQ("Blockgroesse",
	     ((uint32_t)p[4] << 24) | ((uint32_t)p[5] << 16) | ((uint32_t)p[6] << 8) | p[7],
	     OSK_MSC_BLOCK_LEN);

	uint8_t cdb16[16];

	memset(cdb16, 0, sizeof(cdb16));
	cdb16[0] = 0x9e;
	cdb16[1] = 0x10;
	mock_sink_reset();
	mock_cbw(2, 32, true, 0, cdb16, 16);
	p = payload(&len);
	T_EQ("32 Byte", len, 32);
	T_EQ("obere Haelfte null", ((uint32_t)p[0] | p[1] | p[2] | p[3]), 0);
	T_EQ("letzter Block auch hier",
	     ((uint32_t)p[4] << 24) | ((uint32_t)p[5] << 16) | ((uint32_t)p[6] << 8) | p[7],
	     DRIVE_BLOCKS - 1u);

	/* Eine andere Unteraktion ist nicht READ CAPACITY(16). */
	cdb16[1] = 0x11;
	mock_sink_reset();
	mock_cbw(3, 32, true, 0, cdb16, 16);
	T_EQ("falsche Unteraktion abgewiesen", last_status(), OSK_MSC_CSW_FAILED);

	/* Die Formatliste nennt dieselbe Groesse. */
	const uint8_t fc[10] = {0x23, 0, 0, 0, 0, 0, 0, 0, 12, 0};

	mock_sink_reset();
	mock_cbw(4, 12, true, 0, fc, 10);
	p = payload(&len);
	T_EQ("12 Byte", len, 12);
	T_EQ("Blockzahl",
	     ((uint32_t)p[4] << 24) | ((uint32_t)p[5] << 16) | ((uint32_t)p[6] << 8) | p[7],
	     DRIVE_BLOCKS);
	T_EQ("formatiertes Medium", p[8], 0x02);
}

static void read10(uint32_t tag, uint32_t lba, uint16_t count, uint32_t host_len)
{
	uint8_t cdb[10];

	memset(cdb, 0, sizeof(cdb));
	cdb[0] = 0x28;
	cdb[2] = (uint8_t)(lba >> 24);
	cdb[3] = (uint8_t)(lba >> 16);
	cdb[4] = (uint8_t)(lba >> 8);
	cdb[5] = (uint8_t)lba;
	cdb[7] = (uint8_t)(count >> 8);
	cdb[8] = (uint8_t)count;
	mock_sink_reset();
	mock_cbw(tag, host_len, true, 0, cdb, 10);
}

static void case_read(void)
{
	uint8_t want[OSK_MSC_BLOCK_LEN];
	size_t len = 0;

	setup();

	/* Ein Block, und zwar der richtige. */
	read10(1, 5, 1, OSK_MSC_BLOCK_LEN);
	const uint8_t *p = payload(&len);

	T_EQ("ein Block", len, OSK_MSC_BLOCK_LEN);
	fake_drive_pattern(5, want);
	T_TRUE("Inhalt von Block 5", memcmp(p, want, OSK_MSC_BLOCK_LEN) == 0);
	T_EQ("gut", last_status(), OSK_MSC_CSW_PASSED);
	T_EQ("kein Rueckstand", last_residue(), 0);

	/* Vier Bloecke am Stueck, der Reihe nach. */
	fake_drive_reads_reset();
	read10(2, 10, 4, 4u * OSK_MSC_BLOCK_LEN);
	p = payload(&len);
	T_EQ("vier Bloecke", len, 4u * OSK_MSC_BLOCK_LEN);
	T_EQ("vier Lesevorgaenge", fake_drive_reads(), 4);
	for (uint32_t i = 0; i < 4u; i++) {
		fake_drive_pattern(10u + i, want);
		T_TRUE("Reihenfolge stimmt",
		       memcmp(p + i * OSK_MSC_BLOCK_LEN, want, OSK_MSC_BLOCK_LEN) == 0);
	}

	/* Der letzte Block geht noch. */
	read10(3, DRIVE_BLOCKS - 1u, 1, OSK_MSC_BLOCK_LEN);
	T_EQ("letzter Block geht", last_status(), OSK_MSC_CSW_PASSED);

	/* Einer weiter nicht mehr - und dafuer wird nichts gelesen. */
	fake_drive_reads_reset();
	read10(4, DRIVE_BLOCKS, 1, OSK_MSC_BLOCK_LEN);
	T_EQ("hinter dem Ende abgewiesen", last_status(), OSK_MSC_CSW_FAILED);
	T_EQ("Rueckstand ist alles", last_residue(), OSK_MSC_BLOCK_LEN);
	T_EQ("kein Block angefasst", fake_drive_reads(), 0);

	uint8_t key = 0, asc = 0;

	fetch_sense(&key, &asc);
	T_EQ("Grund", key, OSK_MSC_SK_ILLEGAL_REQUEST);
	T_EQ("Adresse ausserhalb", asc, OSK_MSC_ASC_LBA_OUT_OF_RANGE);

	/* Ein Bereich, der am Ende ueberlaeuft, wird ganz abgewiesen - nicht
	 * zur Haelfte geliefert. */
	fake_drive_reads_reset();
	read10(5, DRIVE_BLOCKS - 2u, 4, 4u * OSK_MSC_BLOCK_LEN);
	T_EQ("Bereich ueber das Ende", last_status(), OSK_MSC_CSW_FAILED);
	T_EQ("auch hier nichts gelesen", fake_drive_reads(), 0);

	/* Eine Adresse kurz vor dem Umlauf darf nicht durch Addition
	 * hineinrutschen. */
	fake_drive_reads_reset();
	read10(6, 0xfffffff0u, 16, 16u * OSK_MSC_BLOCK_LEN);
	T_EQ("Umlauf abgewiesen", last_status(), OSK_MSC_CSW_FAILED);
	T_EQ("nichts gelesen", fake_drive_reads(), 0);

	/* Null Bloecke ist erlaubt und liefert nichts. */
	read10(7, 0, 0, 0);
	T_EQ("null Bloecke gut", last_status(), OSK_MSC_CSW_PASSED);
	T_EQ("nur das CSW", mock_sink_len(), OSK_MSC_CSW_LEN);

	/* Eine Laenge, die nicht zur Blockzahl passt, ist ein Phasenfehler. */
	fake_drive_reads_reset();
	read10(8, 0, 2, OSK_MSC_BLOCK_LEN);
	T_EQ("Laenge passt nicht", last_status(), OSK_MSC_CSW_PHASE);
	T_EQ("nichts gelesen", fake_drive_reads(), 0);
}

static void case_read_variants(void)
{
	uint8_t cdb[16];
	uint8_t want[OSK_MSC_BLOCK_LEN];
	size_t len = 0;

	setup();

	/* READ(6): Adresse in 21 Bit, Zahl 0 heisst 256. */
	memset(cdb, 0, sizeof(cdb));
	cdb[0] = 0x08;
	cdb[3] = 7;
	cdb[4] = 1;
	mock_sink_reset();
	mock_cbw(1, OSK_MSC_BLOCK_LEN, true, 0, cdb, 6);
	const uint8_t *p = payload(&len);

	T_EQ("READ(6) liefert einen Block", len, OSK_MSC_BLOCK_LEN);
	fake_drive_pattern(7, want);
	T_TRUE("und den richtigen", memcmp(p, want, OSK_MSC_BLOCK_LEN) == 0);

	cdb[4] = 0; /* 256 Bloecke - mehr als das Laufwerk hat */
	mock_sink_reset();
	mock_cbw(2, 256u * OSK_MSC_BLOCK_LEN, true, 0, cdb, 6);
	T_EQ("256 Bloecke passen nicht", last_status(), OSK_MSC_CSW_FAILED);

	/* READ(12) */
	memset(cdb, 0, sizeof(cdb));
	cdb[0] = 0xa8;
	cdb[5] = 3;
	cdb[9] = 2;
	mock_sink_reset();
	mock_cbw(3, 2u * OSK_MSC_BLOCK_LEN, true, 0, cdb, 12);
	payload(&len);
	T_EQ("READ(12) liefert zwei Bloecke", len, 2u * OSK_MSC_BLOCK_LEN);

	/* READ(16): obere vier Byte der Adresse muessen 0 sein. */
	memset(cdb, 0, sizeof(cdb));
	cdb[0] = 0x88;
	cdb[9] = 4;
	cdb[13] = 1;
	mock_sink_reset();
	mock_cbw(4, OSK_MSC_BLOCK_LEN, true, 0, cdb, 16);
	payload(&len);
	T_EQ("READ(16) liefert einen Block", len, OSK_MSC_BLOCK_LEN);

	cdb[2] = 1; /* Adresse jenseits von 2^32 */
	mock_sink_reset();
	mock_cbw(5, OSK_MSC_BLOCK_LEN, true, 0, cdb, 16);
	T_EQ("hohe Adresse abgewiesen", last_status(), OSK_MSC_CSW_FAILED);

	/* Zu kurzer Befehlsblock: die Laenge wird geprueft, bevor an Stelle 9
	 * gelesen wird. */
	memset(cdb, 0, sizeof(cdb));
	cdb[0] = 0x28;
	mock_sink_reset();
	mock_cbw(6, OSK_MSC_BLOCK_LEN, true, 0, cdb, 6);
	T_EQ("READ(10) mit 6 Byte abgewiesen", last_status(), OSK_MSC_CSW_FAILED);
}

static void case_read_broken_medium(void)
{
	size_t len = 0;

	setup();
	/* Block 12 geht nicht mehr. Was davor liegt, kommt noch; der Rest
	 * steht als Rueckstand im CSW und der Zustand ist FEHLER. */
	fake_drive_fail_at(12);
	read10(1, 10, 4, 4u * OSK_MSC_BLOCK_LEN);
	payload(&len);
	T_EQ("zwei Bloecke kamen", len, 2u * OSK_MSC_BLOCK_LEN);
	T_EQ("Fehler gemeldet", last_status(), OSK_MSC_CSW_FAILED);
	T_EQ("Rueckstand sind zwei Bloecke", last_residue(), 2u * OSK_MSC_BLOCK_LEN);

	uint8_t key = 0, asc = 0;

	fetch_sense(&key, &asc);
	T_EQ("Medium nicht bereit", key, OSK_MSC_SK_NOT_READY);
	T_EQ("kein Medium", asc, OSK_MSC_ASC_MEDIUM_NOT_PRESENT);
}

static void case_frame(void)
{
	const uint8_t cdb[6] = {0x00, 0, 0, 0, 0, 0};
	uint8_t cbw[OSK_MSC_CBW_LEN];

	setup();

	/* Der einfachste Befehl: alles in Ordnung. */
	mock_cbw(0x11223344u, 0, false, 0, cdb, 6);
	uint32_t tag = 0;

	T_TRUE("CSW da", mock_sink_csw(&tag, NULL, NULL));
	T_EQ("Kennzeichen kommt zurueck", tag, 0x11223344u);
	T_EQ("gut", last_status(), OSK_MSC_CSW_PASSED);

	/* Befehlsblock ohne Laenge. */
	mock_sink_reset();
	mock_cbw(2, 0, false, 0, cdb, 0);
	T_EQ("Laenge 0 abgewiesen", last_status(), OSK_MSC_CSW_FAILED);

	/* Befehlsblock laenger als das Feld. Der Puffer ist hier voll, damit
	 * der Fall die Pruefung im Geraet misst und nicht seinen eigenen
	 * zu kurzen Puffer. */
	uint8_t full[OSK_MSC_CDB_MAX];

	memset(full, 0, sizeof(full));
	mock_sink_reset();
	mock_cbw(3, 0, false, 0, full, 17);
	T_EQ("Laenge 17 abgewiesen", last_status(), OSK_MSC_CSW_FAILED);

	/* Es gibt nur eine Einheit. */
	mock_sink_reset();
	mock_cbw(4, 0, false, 1, cdb, 6);
	T_EQ("Einheit 1 abgewiesen", last_status(), OSK_MSC_CSW_FAILED);

	/* Unbekannter Befehl. */
	const uint8_t unknown[6] = {0xd5, 0, 0, 0, 0, 0};

	mock_sink_reset();
	mock_cbw(5, 0, false, 0, unknown, 6);
	T_EQ("unbekannt abgewiesen", last_status(), OSK_MSC_CSW_FAILED);

	uint8_t key = 0, asc = 0;

	fetch_sense(&key, &asc);
	T_EQ("Grund", key, OSK_MSC_SK_ILLEGAL_REQUEST);
	T_EQ("unbekannter Befehlscode", asc, OSK_MSC_ASC_INVALID_OPCODE);

	/* Ein Lesebefehl, bei dem der Rechner Daten schicken will. */
	mock_sink_reset();
	mock_cbw(6, 16, false, 0, cdb, 6);
	T_EQ("falsche Richtung ist ein Phasenfehler", last_status(), OSK_MSC_CSW_PHASE);

	/* Kein CBW: falsche Kennung. Darauf wird nicht geantwortet, und
	 * danach geht gar nichts mehr, bis der Rechner zuruecksetzt. */
	osk_msc_reset();
	mock_sink_reset();
	memset(cbw, 0, sizeof(cbw));
	cbw[0] = 'X';
	osk_msc_rx(cbw, sizeof(cbw));
	T_EQ("keine Antwort", mock_sink_calls(), 0);
	T_TRUE("stehengeblieben", osk_msc_stalled());

	mock_cbw(7, 0, false, 0, cdb, 6);
	T_EQ("danach auch nicht", mock_sink_calls(), 0);

	osk_msc_reset();
	T_TRUE("nach dem Zuruecksetzen laeuft es wieder", !osk_msc_stalled());
	mock_cbw(8, 0, false, 0, cdb, 6);
	T_EQ("gut", last_status(), OSK_MSC_CSW_PASSED);

	/* Kein CBW: falsche Laenge. */
	osk_msc_reset();
	mock_sink_reset();
	osk_msc_rx(cbw, 12);
	T_EQ("keine Antwort", mock_sink_calls(), 0);
	T_TRUE("stehengeblieben", osk_msc_stalled());
	osk_msc_reset();
}

static void case_switch_off(void)
{
	const uint8_t cdb[6] = {0x00, 0, 0, 0, 0, 0};

	setup();
	T_EQ("ausschalten geht", osk_msc_set_enabled(false), OSK_OK);
	T_TRUE("aus", !osk_msc_enabled());
	T_EQ("Transportliste weiss davon", mock_transport_enabled(OSK_LINK_USB_MSC), 0);

	mock_sink_reset();
	mock_cbw(1, 0, false, 0, cdb, 6);
	T_EQ("ausgeschaltet antwortet nichts", mock_sink_calls(), 0);

	T_EQ("wieder an", osk_msc_set_enabled(true), OSK_OK);
	T_EQ("Transportliste weiss auch das", mock_transport_enabled(OSK_LINK_USB_MSC), 1);
	mock_sink_reset();
	mock_cbw(2, 0, false, 0, cdb, 6);
	T_EQ("und antwortet wieder", last_status(), OSK_MSC_CSW_PASSED);
}

static void case_sense_is_cleared(void)
{
	const uint8_t bad[6] = {0xd5, 0, 0, 0, 0, 0};
	uint8_t key = 0, asc = 0;

	setup();
	mock_cbw(1, 0, false, 0, bad, 6);
	fetch_sense(&key, &asc);
	T_EQ("Fehler steht an", asc, OSK_MSC_ASC_INVALID_OPCODE);

	/* Zweimal fragen heisst nicht zweimal denselben Fehler bekommen. */
	fetch_sense(&key, &asc);
	T_EQ("abgeholt ist weg", key, OSK_MSC_SK_NO_SENSE);
	T_EQ("und kein Zusatz", asc, 0);

	/* Ein gelungener Befehl raeumt ebenfalls auf. */
	mock_cbw(2, 0, false, 0, bad, 6);
	const uint8_t ok[6] = {0x00, 0, 0, 0, 0, 0};

	mock_sink_reset();
	mock_cbw(3, 0, false, 0, ok, 6);
	fetch_sense(&key, &asc);
	T_EQ("nach einem guten Befehl nichts mehr", key, OSK_MSC_SK_NO_SENSE);
}

int main(void)
{
	case_init();
	case_inquiry();
	case_capacity();
	case_read();
	case_read_variants();
	case_read_broken_medium();
	case_frame();
	case_switch_off();
	case_sense_is_cleared();
	fake_drive_free();
	return t_report("test_msc");
}
